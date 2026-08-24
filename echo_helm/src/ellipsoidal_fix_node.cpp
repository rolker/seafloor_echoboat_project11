// Roland Arsenault
// Center for Coastal and Ocean Mapping
// University of New Hampshire
// Copyright 2026, All rights reserved.

// Republishes mavros's fused global position with a genuinely WGS-84
// ellipsoidal altitude.
//
// sensor_msgs/NavSatFix specifies "Altitude [m]. Positive is above the WGS 84
// ellipsoid", but mavros publishes GLOBAL_POSITION_INT's AMSL altitude
// converted with GeographicLib EGM96-5, while the receiver derived that AMSL
// value with its own internal geoid table. The two models disagree -- by
// 0.626 m at the UNH pier -- and the difference lands in every consumer. See
// echo_helm/ellipsoidal_corrector.hpp for the derivation.
//
// The fix belongs here rather than in a navigation consumer such as
// mru_transform: the message contract is already unambiguous, so the defect is
// in the publisher, and compensating downstream would corrupt any source that
// honours the contract (an SBG NavSatFix is already ellipsoidal). This node is
// a stopgap in the right shape -- it should be deleted the day mavros prefers
// GPS_RAW_INT.alt_ellipsoid upstream.
//
// Threading: everything below runs under the default single-threaded executor
// (see main()). The three subscription callbacks and the diagnostic timer share
// the corrector and the counters without a mutex, which is safe only because
// that executor serialises them. Anything that moves this node into a
// multi-threaded executor, or a component container with a parallel callback
// group, must add the locking first.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "mavros_msgs/msg/gpsraw.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "sensor_msgs/msg/nav_sat_status.hpp"

#include "echo_helm/ellipsoidal_corrector.hpp"

using namespace std::chrono_literals;

namespace
{

diagnostic_msgs::msg::KeyValue keyValue(const std::string & key, const std::string & value)
{
  diagnostic_msgs::msg::KeyValue kv;
  kv.key = key;
  kv.value = value;
  return kv;
}

/// Four decimal places: sub-millimetre on a metres value, sub-millisecond on a
/// seconds value. Deliberately unit-agnostic -- the diagnostic keys carry the
/// unit (`correction_m`, `correction_age_s`), the formatter does not.
std::string fixed4(double value)
{
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.4f", value);
  return buffer;
}

}  // namespace

class EllipsoidalFix : public rclcpp::Node
{
public:
  EllipsoidalFix()
  : rclcpp::Node("ellipsoidal_fix")
  {
    const auto input_topic = declare_parameter<std::string>(
      "input_topic", "mavros/global_position/global");
    const auto raw_fix_topic = declare_parameter<std::string>(
      "raw_fix_topic", "mavros/global_position/raw/fix");
    const auto gps_raw_topic = declare_parameter<std::string>(
      "gps_raw_topic", "mavros/gpsstatus/gps1/raw");
    const auto output_topic = declare_parameter<std::string>(
      "output_topic", "mavros/global_position/global_ellipsoidal");
    const auto pair_tolerance = declare_parameter<double>("pair_tolerance", 0.05);
    const auto correction_timeout = declare_parameter<double>("correction_timeout", 30.0);
    // A difference between two geoid models is sub-metre; 3 m leaves generous
    // headroom for an unusual receiver table while still being two orders of
    // magnitude below the ~26 m a missing MAVLink-v2 alt_ellipsoid produces.
    const auto max_correction = declare_parameter<double>("max_correction", 3.0);
    // How long the input may be silent before the node reports a fault. The
    // fused global position runs at several Hz, so a couple of seconds is many
    // missed messages, not jitter.
    input_timeout_ = declare_parameter<double>("input_timeout", 3.0);
    diagnostic_name_ = declare_parameter<std::string>(
      "diagnostic_name", "GPS: ellipsoidal fix");
    hardware_id_ = declare_parameter<std::string>("hardware_id", "");

    // A field param edit that points the output at one of the inputs would
    // build a self-feeding loop, adding the correction again on every cycle and
    // diverging 0.626 m per pass -- silently, because every message on it is a
    // well-formed NavSatFix. Refuse to start instead.
    for (const auto & input : {input_topic, raw_fix_topic}) {
      if (output_topic == input) {
        throw std::invalid_argument(
                "output_topic \"" + output_topic + "\" is also an input topic; "
                "this would feed the node its own output and diverge without bound");
      }
    }

    corrector_ = std::make_unique<echo_helm::EllipsoidalCorrector>(
      pair_tolerance, correction_timeout, max_correction);

    // Best effort subscriptions are compatible with both best effort and
    // reliable publishers, so this attaches to mavros however it is configured.
    auto qos = rclcpp::SensorDataQoS();

    publisher_ = create_publisher<sensor_msgs::msg::NavSatFix>(output_topic, 10);
    diagnostic_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", 10);

    input_subscription_ = create_subscription<sensor_msgs::msg::NavSatFix>(
      input_topic, qos, std::bind(&EllipsoidalFix::inputCallback, this, std::placeholders::_1));
    raw_fix_subscription_ = create_subscription<sensor_msgs::msg::NavSatFix>(
      raw_fix_topic, qos, std::bind(&EllipsoidalFix::rawFixCallback, this, std::placeholders::_1));
    gps_raw_subscription_ = create_subscription<mavros_msgs::msg::GPSRAW>(
      gps_raw_topic, qos, std::bind(&EllipsoidalFix::gpsRawCallback, this, std::placeholders::_1));

    input_topic_ = input_topic;
    diagnostic_timer_ = create_wall_timer(1s, std::bind(&EllipsoidalFix::publishDiagnostic, this));

    RCLCPP_INFO_STREAM(
      get_logger(), "Correcting \"" << input_topic << "\" to \"" << output_topic
                                    << "\" using \"" << raw_fix_topic << "\" and \""
                                    << gps_raw_topic << "\"");
  }

private:
  static double toSeconds(const builtin_interfaces::msg::Time & stamp)
  {
    return static_cast<double>(stamp.sec) + 1e-9 * static_cast<double>(stamp.nanosec);
  }

  void rawFixCallback(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
  {
    // STATUS_NO_FIX (-1) means the receiver is still acquiring and the altitude
    // field is whatever the driver last had -- frequently zero. Pairing that
    // with a good gps1/raw would latch a correction from acquisition-time
    // rubbish and hold it for the full correction_timeout.
    const bool quality_ok = msg->status.status >= sensor_msgs::msg::NavSatStatus::STATUS_FIX;
    corrector_->setRawFix(msg->altitude, toSeconds(msg->header.stamp), quality_ok);
  }

  void gpsRawCallback(const mavros_msgs::msg::GPSRAW::SharedPtr msg)
  {
    // Two independent gates, because they fail independently:
    //
    // fix_type below 3D means there is no vertical solution yet, so alt and
    // alt_ellipsoid are both meaningless (and typically zero) during
    // acquisition.
    //
    // alt_ellipsoid is a MAVLink-v2 extension. mavros copies it
    // unconditionally, so over a v1 link -- or from a receiver that does not
    // populate it -- the field arrives as exactly 0 mm in an otherwise valid
    // message. A genuine reading of exactly 0 mm above the ellipsoid does not
    // occur in practice, so the sentinel is safe to reject, and the corrector's
    // max_correction bound catches anything this misses.
    const bool has_3d_fix = msg->fix_type >= mavros_msgs::msg::GPSRAW::GPS_FIX_TYPE_3D_FIX;
    const bool has_ellipsoid = msg->alt_ellipsoid != 0;
    if (!has_ellipsoid) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 30000,
        "GPS_RAW_INT.alt_ellipsoid is absent (0); this link is probably MAVLink v1. "
        "No ellipsoidal correction is possible and no corrected fix will be published.");
    }
    corrector_->setGpsRaw(
      msg->alt / 1000.0, msg->alt_ellipsoid / 1000.0, toSeconds(msg->header.stamp),
      has_3d_fix && has_ellipsoid);
  }

  void inputCallback(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
  {
    const double t = now().seconds();
    last_input_time_ = t;
    ++input_count_;

    const auto corrected = corrector_->correct(msg->altitude, t);
    if (!corrected) {
      // Publishing the uncorrected altitude would be wrong by more than half a
      // metre and indistinguishable downstream from a good fix, so it is
      // withheld and the diagnostic says why.
      //
      // On BizzyBoat as configured today, withholding also lets mru_transform's
      // sensor_timeout fail over to the SBG. That failover is a property of the
      // boat's config, not of this node: it holds only while a second nav
      // source is fitted and listed in `sensor_names` (the SBG is a loaner),
      // and the generic `echo.yaml` configures a single sensor. Where there is
      // no second source, withholding is a deliberate outage in preference to a
      // silent half-metre error -- the diagnostic is what makes it visible.
      ++skipped_count_;
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 10000,
        "No geoid correction available; withholding corrected fix");
      return;
    }
    auto out = *msg;
    out.altitude = *corrected;
    publisher_->publish(out);
    last_publish_time_ = t;
    ++published_count_;
  }

  void publishDiagnostic()
  {
    const double t = now().seconds();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = diagnostic_name_;
    status.hardware_id = hardware_id_;

    const auto correction = corrector_->correction();
    const bool have_fresh_correction = corrector_->hasCorrection(t);

    // Two independent things can be wrong, and the old version only looked at
    // one of them. A correction can be perfectly fresh while nothing at all is
    // coming out, because the input topic died or mavros respawned under us --
    // and a node that reports OK while publishing nothing is worse than one
    // that publishes nothing loudly.
    const bool input_flowing = input_count_ > 0 &&
      (t - last_input_time_) <= input_timeout_ &&
      (t - last_input_time_) >= -input_timeout_;

    auto level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    std::string message;

    if (!correction) {
      level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      message = std::string("no correction established: ") +
        echo_helm::EllipsoidalCorrector::rejectDescription(corrector_->lastReject());
    } else if (!have_fresh_correction) {
      level = diagnostic_msgs::msg::DiagnosticStatus::STALE;
      message = "correction stale; not publishing";
    } else {
      message = "correcting " + fixed4(*correction) + " m";
    }

    if (!input_flowing) {
      // Silent input outranks a healthy correction: nothing is reaching the
      // output either way.
      level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      const std::string why = input_count_ == 0 ?
        "no message ever received on \"" + input_topic_ + "\"" :
        "no message on \"" + input_topic_ + "\" for " +
        fixed4(t - last_input_time_) + " s";
      message = message.empty() ? why : why + "; " + message;
    }

    status.level = level;
    status.message = message;

    if (correction) {
      status.values.push_back(keyValue("correction_m", fixed4(*correction)));
      status.values.push_back(
        keyValue("correction_age_s", fixed4(corrector_->correctionAge(t))));
    }
    // These two are the terms whose disagreement *is* the defect. Publishing
    // them means a change of receiver, geoid dataset or mavros version shows up
    // here instead of being inferred from a tide error days later.
    if (const auto undulation = corrector_->receiverUndulation()) {
      status.values.push_back(keyValue("receiver_undulation_m", fixed4(*undulation)));
    }
    if (const auto undulation = corrector_->mavrosUndulation()) {
      status.values.push_back(keyValue("mavros_undulation_m", fixed4(*undulation)));
    }
    // A pairing that is drifting towards the tolerance presents, once it
    // crosses, as a total outage with no explanation. Publish the skew so the
    // drift is visible first.
    if (const auto skew = corrector_->pairSkew()) {
      status.values.push_back(keyValue("pair_skew_s", fixed4(*skew)));
    }
    status.values.push_back(
      keyValue("rejected_pairs", std::to_string(corrector_->rejectedPairCount())));
    status.values.push_back(
      keyValue(
        "last_reject",
        echo_helm::EllipsoidalCorrector::rejectDescription(corrector_->lastReject())));
    status.values.push_back(keyValue("input_topic", input_topic_));
    status.values.push_back(keyValue("received", std::to_string(input_count_)));
    status.values.push_back(
      keyValue(
        "since_last_input_s",
        input_count_ == 0 ? "never" : fixed4(t - last_input_time_)));
    status.values.push_back(keyValue("published", std::to_string(published_count_)));
    status.values.push_back(
      keyValue(
        "since_last_publish_s",
        published_count_ == 0 ? "never" : fixed4(t - last_publish_time_)));
    status.values.push_back(keyValue("withheld", std::to_string(skipped_count_)));

    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();
    array.status.push_back(status);
    diagnostic_publisher_->publish(array);
  }

  std::unique_ptr<echo_helm::EllipsoidalCorrector> corrector_;
  std::string diagnostic_name_;
  std::string hardware_id_;
  std::string input_topic_;
  double input_timeout_ = 3.0;
  double last_input_time_ = 0.0;
  double last_publish_time_ = 0.0;
  uint64_t input_count_ = 0;
  uint64_t published_count_ = 0;
  uint64_t skipped_count_ = 0;

  rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr publisher_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostic_publisher_;
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr input_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr raw_fix_subscription_;
  rclcpp::Subscription<mavros_msgs::msg::GPSRAW>::SharedPtr gps_raw_subscription_;
  rclcpp::TimerBase::SharedPtr diagnostic_timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  try {
    // Single-threaded by design -- see the threading note at the top of the file.
    rclcpp::spin(std::make_shared<EllipsoidalFix>());
  } catch (const std::exception & e) {
    // A misconfiguration must fail loudly and stay down rather than respawn
    // into the same bad state; the message is the only thing the operator gets.
    RCLCPP_FATAL(rclcpp::get_logger("ellipsoidal_fix"), "%s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
