// Roland Arsenault
// Center for Coastal and Ocean Mapping
// University of New Hampshire
// Copyright 2026, All rights reserved.

#ifndef ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP
#define ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP

#include <cmath>
#include <optional>

namespace echo_helm
{

/// Removes the geoid round trip from mavros's NavSatFix altitudes.
///
/// A GNSS receiver solves for ellipsoidal height, then derives mean sea level
/// using its own internal geoid table. ArduPilot forwards both numbers
/// (GPS_RAW_INT.alt and .alt_ellipsoid) so nothing is lost, but
/// GLOBAL_POSITION_INT carries only the MSL value, and mavros converts that
/// back to ellipsoidal with GeographicLib EGM96-5. Inverting one geoid model
/// with a different one leaves the difference behind: measured at the UNH pier
/// on 2026-08-21, the receiver's undulation was -27.7150 m and EGM96-5 was
/// -27.0890 m, so every mavros NavSatFix altitude sat 0.626 m too high. That
/// error flowed into odom, the sea-surface estimate, the tide and every
/// sounding.
///
/// The correction needs no geoid model of its own. mavros builds both
/// `global_position/raw/fix` and `gpsstatus/gps1/raw` from the *same*
/// GPS_RAW_INT message, so the EGM96 term it added cancels exactly in
///
///     correction = alt_ellipsoid - raw_fix_altitude
///
/// leaving the difference between the two models. Adding that to any mavros
/// altitude re-references it to the ellipsoid the receiver actually solved in.
/// Because the term is measured rather than tabulated, it stays correct across
/// a site move, a different geoid dataset, a mavros upgrade or a receiver
/// firmware change, with no constant for anyone to remember to update.
///
/// This class is deliberately free of ROS types so the pairing and staleness
/// rules -- the parts that can go quietly wrong -- are unit-testable.
class EllipsoidalCorrector
{
public:
  /// \param pair_tolerance  maximum stamp separation (s) for two samples to be
  ///        treated as coming from the same GPS_RAW_INT. mavros stamps them at
  ///        publish time, tens of microseconds apart, while successive
  ///        GPS_RAW_INT messages are ~200 ms apart at 5 Hz, so anything well
  ///        inside that gap separates the two cases cleanly.
  /// \param correction_timeout  how long (s) a correction may be reused after
  ///        the last good pair. The term is physically near-constant, so
  ///        holding it briefly is safe; holding it indefinitely would let the
  ///        boat move to another site on a stale value.
  EllipsoidalCorrector(double pair_tolerance, double correction_timeout)
  : pair_tolerance_(pair_tolerance), correction_timeout_(correction_timeout)
  {
  }

  /// Latest `global_position/raw/fix` altitude (mavros: receiver MSL + EGM96).
  void setRawFix(double altitude, double stamp)
  {
    raw_fix_altitude_ = altitude;
    raw_fix_stamp_ = stamp;
    tryPair();
  }

  /// Latest `gpsstatus/gps1/raw`: MSL and ellipsoidal altitude, both metres.
  void setGpsRaw(double altitude_msl, double altitude_ellipsoid, double stamp)
  {
    gps_raw_msl_ = altitude_msl;
    gps_raw_ellipsoid_ = altitude_ellipsoid;
    gps_raw_stamp_ = stamp;
    tryPair();
  }

  /// True once a correction has been established and is still within its
  /// timeout at time \p now.
  bool hasCorrection(double now) const
  {
    return correction_.has_value() && correctionAge(now) <= correction_timeout_;
  }

  /// Seconds since the correction was last established; infinite if never.
  double correctionAge(double now) const
  {
    if (!correction_.has_value()) {
      return std::numeric_limits<double>::infinity();
    }
    return now - correction_stamp_;
  }

  /// Metres to add to a mavros altitude. Only meaningful once established.
  std::optional<double> correction() const
  {
    return correction_;
  }

  /// The receiver's own geoid undulation, alt_ellipsoid - alt. Exposed so a
  /// diagnostic can show it changing rather than leaving it to be inferred.
  std::optional<double> receiverUndulation() const
  {
    if (!paired_) {
      return std::nullopt;
    }
    return gps_raw_ellipsoid_ - gps_raw_msl_;
  }

  /// The undulation mavros applied, raw_fix_altitude - alt. Should track
  /// GeographicLib EGM96-5 at the current position.
  std::optional<double> mavrosUndulation() const
  {
    if (!paired_) {
      return std::nullopt;
    }
    return raw_fix_altitude_ - gps_raw_msl_;
  }

  /// Corrected altitude, or nothing if no usable correction is available.
  ///
  /// Returning nothing rather than the uncorrected value is deliberate: a
  /// silently uncorrected altitude is wrong by more than half a metre and
  /// indistinguishable from a good one downstream.
  std::optional<double> correct(double altitude, double now) const
  {
    if (!hasCorrection(now)) {
      return std::nullopt;
    }
    return altitude + *correction_;
  }

private:
  void tryPair()
  {
    if (std::isnan(raw_fix_stamp_) || std::isnan(gps_raw_stamp_)) {
      return;
    }
    if (std::fabs(raw_fix_stamp_ - gps_raw_stamp_) > pair_tolerance_) {
      // The two topics have decoupled; keep whatever correction we already
      // hold and let it age out rather than pairing across messages.
      return;
    }
    correction_ = gps_raw_ellipsoid_ - raw_fix_altitude_;
    correction_stamp_ = std::max(raw_fix_stamp_, gps_raw_stamp_);
    paired_ = true;
  }

  double pair_tolerance_;
  double correction_timeout_;

  double raw_fix_altitude_ = 0.0;
  double raw_fix_stamp_ = std::numeric_limits<double>::quiet_NaN();
  double gps_raw_msl_ = 0.0;
  double gps_raw_ellipsoid_ = 0.0;
  double gps_raw_stamp_ = std::numeric_limits<double>::quiet_NaN();

  bool paired_ = false;
  std::optional<double> correction_;
  double correction_stamp_ = 0.0;
};

}  // namespace echo_helm

#endif  // ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP
