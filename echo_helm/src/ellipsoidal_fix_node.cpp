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

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "mavros_msgs/msg/gpsraw.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"

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

std::string metres(double value)
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
    diagnostic_name_ = declare_parameter<std::string>(
      "diagnostic_name", "GPS: ellipsoidal fix");
    hardware_id_ = declare_parameter<std::string>("hardware_id", "");

    corrector_ = std::make_unique<echo_helm::EllipsoidalCorrector>(
      pair_tolerance, correction_timeout);

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
    corrector_->setRawFix(msg->altitude, toSeconds(msg->header.stamp));
  }

  void gpsRawCallback(const mavros_msgs::msg::GPSRAW::SharedPtr msg)
  {
    corrector_->setGpsRaw(
      msg->alt / 1000.0, msg->alt_ellipsoid / 1000.0, toSeconds(msg->header.stamp));
  }

  void inputCallback(const sensor_msgs::msg::NavSatFix::SharedPtr msg)
  {
    const auto corrected = corrector_->correct(msg->altitude, now().seconds());
    if (!corrected) {
      // Publishing the uncorrected altitude would be wrong by more than half a
      // metre and indistinguishable downstream from a good fix. Withholding it
      // lets mru_transform's sensor_timeout fail over to another source, and
      // the diagnostic says why.
      ++skipped_count_;
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 10000,
        "No geoid correction available; withholding corrected fix");
      return;
    }
    auto out = *msg;
    out.altitude = *corrected;
    publisher_->publish(out);
    ++published_count_;
  }

  void publishDiagnostic()
  {
    const double t = now().seconds();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = diagnostic_name_;
    status.hardware_id = hardware_id_;

    const auto correction = corrector_->correction();
    if (!correction) {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      status.message = "no correction established";
    } else if (!corrector_->hasCorrection(t)) {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::STALE;
      status.message = "correction stale; not publishing";
    } else {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
      status.message = "correcting " + metres(*correction) + " m";
    }

    if (correction) {
      status.values.push_back(keyValue("correction_m", metres(*correction)));
      status.values.push_back(
        keyValue("correction_age_s", metres(corrector_->correctionAge(t))));
    }
    // These two are the terms whose disagreement *is* the defect. Publishing
    // them means a change of receiver, geoid dataset or mavros version shows up
    // here instead of being inferred from a tide error days later.
    if (const auto undulation = corrector_->receiverUndulation()) {
      status.values.push_back(keyValue("receiver_undulation_m", metres(*undulation)));
    }
    if (const auto undulation = corrector_->mavrosUndulation()) {
      status.values.push_back(keyValue("mavros_undulation_m", metres(*undulation)));
    }
    status.values.push_back(keyValue("published", std::to_string(published_count_)));
    status.values.push_back(keyValue("withheld", std::to_string(skipped_count_)));

    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();
    array.status.push_back(status);
    diagnostic_publisher_->publish(array);
  }

  std::unique_ptr<echo_helm::EllipsoidalCorrector> corrector_;
  std::string diagnostic_name_;
  std::string hardware_id_;
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
  rclcpp::spin(std::make_shared<EllipsoidalFix>());
  rclcpp::shutdown();
  return 0;
}
