// Roland Arsenault
// Center for Coastal and Ocean Mapping
// University of New Hampshire
// Copyright 2026, All rights reserved.

#ifndef ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP
#define ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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
/// **What the correction is, and therefore what it may be.** It is the
/// disagreement between two geoid models at one point on the earth. Global
/// geoid models agree with each other to well under a metre; a correction of
/// tens of metres is not a geoid disagreement, it is a broken input. The most
/// likely broken input is a real one: `GPS_RAW_INT.alt_ellipsoid` is a
/// MAVLink-v2 extension field, and mavros copies it unconditionally, so a v1
/// link -- or a receiver that does not populate it -- delivers a structurally
/// valid message carrying **zero**. Zero produces
/// `correction = 0 - (-25.8) = +25.8 m`, which would publish as a perfectly
/// well-formed fix on the topic every sounding depends on. Hence
/// `max_correction`: a bound on a physically sub-metre quantity, enforced
/// before a correction is ever established. Callers must additionally reject
/// the absent-field sentinel and gate on fix quality (see \p quality_ok) so
/// nothing is latched from acquisition-time zeros.
///
/// This class is deliberately free of ROS types so the pairing, validity and
/// staleness rules -- the parts that can go quietly wrong -- are unit-testable.
class EllipsoidalCorrector
{
public:
  /// Why the most recent pairing attempt did not establish a correction.
  /// Exposed so a stuck node reports a cause rather than a silent outage.
  enum class Reject : uint8_t
  {
    None,           ///< The last attempt established a correction.
    Incomplete,     ///< One of the two topics has not delivered a valid sample.
    NotCoincident,  ///< The two samples are further apart than pair_tolerance.
    NonFinite,      ///< An altitude was NaN or infinite.
    Implausible,    ///< |correction| exceeded max_correction.
  };

  /// \param pair_tolerance  maximum stamp separation (s) for two samples to be
  ///        treated as coming from the same GPS_RAW_INT. Both mavros plugins
  ///        build their message from the same GPS_RAW_INT and stamp it with
  ///        `uas->synchronized_header(..., time_usec)`, so for a genuine pair
  ///        the two stamps are *identical*; the tolerance exists to absorb
  ///        clock-source differences and any future divergence in how mavros
  ///        stamps, not a known publish-time offset. Successive GPS_RAW_INT
  ///        messages are ~200 ms apart at 5 Hz, so anything well inside that
  ///        gap separates a pair from two consecutive messages cleanly.
  /// \param correction_timeout  how long (s) a correction may be reused after
  ///        the last good pair. The term is physically near-constant, so
  ///        holding it briefly is safe; holding it indefinitely would let the
  ///        boat move to another site on a stale value.
  /// \param max_correction  plausibility bound (m) on |correction|. The
  ///        quantity is a difference between two geoid models and so is
  ///        sub-metre in practice; the bound is what stops a missing
  ///        MAVLink-v2 `alt_ellipsoid` (which arrives as 0) from latching a
  ///        ~26 m offset as a valid correction.
  EllipsoidalCorrector(
    double pair_tolerance, double correction_timeout, double max_correction = 3.0)
  : pair_tolerance_(pair_tolerance),
    correction_timeout_(correction_timeout),
    max_correction_(max_correction)
  {
  }

  /// Latest `global_position/raw/fix` altitude (mavros: receiver MSL + EGM96).
  ///
  /// \param quality_ok  false when the publisher's own fix status says this
  ///        sample is not a usable 3D fix. A rejected sample is discarded, not
  ///        merely ignored, so it can never later be paired with a good one.
  void setRawFix(double altitude, double stamp, bool quality_ok = true)
  {
    if (!quality_ok || !std::isfinite(altitude) || !std::isfinite(stamp)) {
      invalidateRawFix();
      return;
    }
    raw_fix_altitude_ = altitude;
    raw_fix_stamp_ = stamp;
    tryPair();
  }

  /// Latest `gpsstatus/gps1/raw`: MSL and ellipsoidal altitude, both metres.
  ///
  /// \param quality_ok  false when `fix_type` is below a 3D fix, or when
  ///        `alt_ellipsoid` carried the absent-field sentinel. See the class
  ///        comment: this gate and \p max_correction together are what keep a
  ///        MAVLink-v1 link from publishing a ~26 m error as a valid fix.
  void setGpsRaw(double altitude_msl, double altitude_ellipsoid, double stamp,
    bool quality_ok = true)
  {
    if (!quality_ok || !std::isfinite(altitude_msl) ||
      !std::isfinite(altitude_ellipsoid) || !std::isfinite(stamp))
    {
      invalidateGpsRaw();
      return;
    }
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

  /// The receiver's own geoid undulation, alt_ellipsoid - alt, snapshotted at
  /// the last *successful* pair. Exposed so a diagnostic can show it changing
  /// rather than leaving it to be inferred.
  ///
  /// Snapshotting rather than recomputing on demand is deliberate: computing it
  /// live from the latest samples would mix an old gps1/raw with a new raw/fix
  /// once the topics decoupled, and the reading would drift silently -- exactly
  /// the failure this value exists to make visible.
  std::optional<double> receiverUndulation() const
  {
    return receiver_undulation_;
  }

  /// The undulation mavros applied, raw_fix_altitude - alt, snapshotted at the
  /// last successful pair. Should track GeographicLib EGM96-5 at the current
  /// position.
  std::optional<double> mavrosUndulation() const
  {
    return mavros_undulation_;
  }

  /// Stamp separation (s) of the last successful pair; nothing until one pairs.
  /// Published as a diagnostic so a drifting pairing is visible before it
  /// crosses the tolerance and presents as a silent outage.
  std::optional<double> pairSkew() const
  {
    return pair_skew_;
  }

  /// Why the most recent pairing attempt failed, or Reject::None.
  Reject lastReject() const
  {
    return last_reject_;
  }

  /// Count of samples that arrived complete and coincident but were rejected as
  /// non-finite or implausible. A non-zero, growing count with no correction is
  /// the signature of a MAVLink-v1 link (`alt_ellipsoid` absent).
  uint64_t rejectedPairCount() const
  {
    return rejected_pair_count_;
  }

  /// Corrected altitude, or nothing if no usable correction is available.
  ///
  /// Returning nothing rather than the uncorrected value is deliberate: a
  /// silently uncorrected altitude is wrong by more than half a metre and
  /// indistinguishable from a good one downstream.
  std::optional<double> correct(double altitude, double now) const
  {
    if (!std::isfinite(altitude) || !hasCorrection(now)) {
      return std::nullopt;
    }
    return altitude + *correction_;
  }

  /// Human-readable form of a rejection reason, for diagnostics and logs.
  static const char * rejectDescription(Reject reject)
  {
    switch (reject) {
      case Reject::None:
        return "ok";
      case Reject::Incomplete:
        return "waiting for a valid sample on both topics";
      case Reject::NotCoincident:
        return "raw/fix and gps1/raw stamps are not coincident";
      case Reject::NonFinite:
        return "non-finite altitude";
      case Reject::Implausible:
        return "correction beyond max_correction (alt_ellipsoid absent?)";
    }
    return "unknown";
  }

private:
  void invalidateRawFix()
  {
    raw_fix_stamp_ = std::numeric_limits<double>::quiet_NaN();
    last_reject_ = Reject::Incomplete;
  }

  void invalidateGpsRaw()
  {
    gps_raw_stamp_ = std::numeric_limits<double>::quiet_NaN();
    last_reject_ = Reject::Incomplete;
  }

  void tryPair()
  {
    if (std::isnan(raw_fix_stamp_) || std::isnan(gps_raw_stamp_)) {
      last_reject_ = Reject::Incomplete;
      return;
    }
    const double skew = std::fabs(raw_fix_stamp_ - gps_raw_stamp_);
    if (skew > pair_tolerance_) {
      // The two topics have decoupled; keep whatever correction we already
      // hold and let it age out rather than pairing across messages.
      last_reject_ = Reject::NotCoincident;
      return;
    }
    const double candidate = gps_raw_ellipsoid_ - raw_fix_altitude_;
    if (!std::isfinite(candidate)) {
      ++rejected_pair_count_;
      last_reject_ = Reject::NonFinite;
      return;
    }
    if (std::fabs(candidate) > max_correction_) {
      // Not a geoid disagreement. Overwhelmingly the MAVLink-v2 alt_ellipsoid
      // extension arriving as 0 over a v1 link. Keep any correction already
      // held (it will age out on its own) and refuse to establish this one.
      ++rejected_pair_count_;
      last_reject_ = Reject::Implausible;
      return;
    }
    correction_ = candidate;
    correction_stamp_ = std::max(raw_fix_stamp_, gps_raw_stamp_);
    // Snapshot both undulations from this pair, so the pair they describe is
    // always the pair the correction came from.
    receiver_undulation_ = gps_raw_ellipsoid_ - gps_raw_msl_;
    mavros_undulation_ = raw_fix_altitude_ - gps_raw_msl_;
    pair_skew_ = skew;
    last_reject_ = Reject::None;
  }

  double pair_tolerance_;
  double correction_timeout_;
  double max_correction_;

  double raw_fix_altitude_ = 0.0;
  double raw_fix_stamp_ = std::numeric_limits<double>::quiet_NaN();
  double gps_raw_msl_ = 0.0;
  double gps_raw_ellipsoid_ = 0.0;
  double gps_raw_stamp_ = std::numeric_limits<double>::quiet_NaN();

  std::optional<double> correction_;
  double correction_stamp_ = 0.0;
  std::optional<double> receiver_undulation_;
  std::optional<double> mavros_undulation_;
  std::optional<double> pair_skew_;
  Reject last_reject_ = Reject::Incomplete;
  uint64_t rejected_pair_count_ = 0;
};

}  // namespace echo_helm

#endif  // ECHO_HELM_ELLIPSOIDAL_CORRECTOR_HPP
