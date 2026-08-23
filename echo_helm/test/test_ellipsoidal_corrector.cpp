// Roland Arsenault
// Center for Coastal and Ocean Mapping
// University of New Hampshire
// Copyright 2026, All rights reserved.

// Unit tests for EllipsoidalCorrector.
//
// The arithmetic is one subtraction; what can go quietly wrong is the pairing
// and the staleness rule, so that is where the tests concentrate. The measured
// values below are from the UNH pier on 2026-08-21, when the defect was found:
// receiver undulation -27.7150 m, EGM96-5 -27.0890 m, correction -0.6260 m.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "echo_helm/ellipsoidal_corrector.hpp"

using echo_helm::EllipsoidalCorrector;

namespace
{

// Measured at the UNH pier, 2026-08-21.
constexpr double kGpsMsl = 1.2847;
constexpr double kReceiverUndulation = -27.7150;
constexpr double kMavrosUndulation = -27.0890;
constexpr double kGpsEllipsoid = kGpsMsl + kReceiverUndulation;   // -26.4303
constexpr double kRawFixAltitude = kGpsMsl + kMavrosUndulation;   // -25.8043
constexpr double kCorrection = kGpsEllipsoid - kRawFixAltitude;   // -0.6260

// Feed one coincident pair, as mavros delivers it: both messages are built from
// the same GPS_RAW_INT and stamped with the same synchronized_header, so in the
// field the two stamps are identical. A small non-zero skew is used here so the
// pairing logic is exercised rather than trivially satisfied.
void feedPair(EllipsoidalCorrector & corrector, double t, double skew = 60e-6)
{
  corrector.setRawFix(kRawFixAltitude, t);
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, t + skew);
}

}  // namespace

TEST(EllipsoidalCorrector, NoCorrectionBeforeAnyData)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  EXPECT_FALSE(corrector.hasCorrection(100.0));
  EXPECT_FALSE(corrector.correction().has_value());
  EXPECT_FALSE(corrector.correct(-26.8, 100.0).has_value());
}

TEST(EllipsoidalCorrector, NoCorrectionFromOneTopicAlone)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  corrector.setRawFix(kRawFixAltitude, 100.0);
  EXPECT_FALSE(corrector.hasCorrection(100.0));
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.0);
  EXPECT_TRUE(corrector.hasCorrection(100.0));
}

TEST(EllipsoidalCorrector, RecoversTheMeasuredCorrection)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  ASSERT_TRUE(corrector.correction().has_value());
  EXPECT_NEAR(*corrector.correction(), kCorrection, 1e-9);
  EXPECT_NEAR(*corrector.correction(), -0.6260, 1e-4);
}

TEST(EllipsoidalCorrector, CorrectionIsIndependentOfArrivalOrder)
{
  EllipsoidalCorrector a(0.05, 30.0);
  a.setRawFix(kRawFixAltitude, 100.0);
  a.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.00006);

  EllipsoidalCorrector b(0.05, 30.0);
  b.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.00006);
  b.setRawFix(kRawFixAltitude, 100.0);

  EXPECT_NEAR(*a.correction(), *b.correction(), 1e-12);
}

TEST(EllipsoidalCorrector, CorrectsAnAltitude)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  // The fused /global altitude at the same moment.
  const auto corrected = corrector.correct(-26.8020, 100.0);
  ASSERT_TRUE(corrected.has_value());
  EXPECT_NEAR(*corrected, -27.4280, 1e-4);
}

TEST(EllipsoidalCorrector, ExposesBothUndulations)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  ASSERT_TRUE(corrector.receiverUndulation().has_value());
  ASSERT_TRUE(corrector.mavrosUndulation().has_value());
  EXPECT_NEAR(*corrector.receiverUndulation(), -27.7150, 1e-4);
  EXPECT_NEAR(*corrector.mavrosUndulation(), -27.0890, 1e-4);
  // Their difference is the correction; that identity is the whole method.
  EXPECT_NEAR(
    *corrector.receiverUndulation() - *corrector.mavrosUndulation(),
    *corrector.correction(), 1e-9);
}

TEST(EllipsoidalCorrector, RejectsPairsOutsideTheTolerance)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  // 200 ms apart: successive GPS_RAW_INT messages at 5 Hz, not one pair.
  corrector.setRawFix(kRawFixAltitude, 100.0);
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.2);
  EXPECT_FALSE(corrector.hasCorrection(100.2));
}

TEST(EllipsoidalCorrector, AcceptsPairsAtTheToleranceBoundary)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  corrector.setRawFix(kRawFixAltitude, 100.0);
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.05);
  EXPECT_TRUE(corrector.hasCorrection(100.05));
}

TEST(EllipsoidalCorrector, KeepsAGoodCorrectionWhenTopicsDecouple)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  const double established = *corrector.correction();
  // raw/fix keeps arriving, gps1/raw stops: the pair no longer coincides.
  corrector.setRawFix(kRawFixAltitude + 0.1, 105.0);
  ASSERT_TRUE(corrector.correction().has_value());
  EXPECT_NEAR(*corrector.correction(), established, 1e-12);
}

TEST(EllipsoidalCorrector, StaleCorrectionIsNotUsed)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  EXPECT_TRUE(corrector.hasCorrection(129.9));
  EXPECT_FALSE(corrector.hasCorrection(130.1));
  EXPECT_FALSE(corrector.correct(-26.8020, 130.1).has_value());
}

TEST(EllipsoidalCorrector, StaleCorrectionRecoversOnFreshData)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  ASSERT_FALSE(corrector.hasCorrection(200.0));
  feedPair(corrector, 200.0);
  EXPECT_TRUE(corrector.hasCorrection(200.0));
}

TEST(EllipsoidalCorrector, AgeIsInfiniteBeforeAnyCorrection)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  EXPECT_TRUE(std::isinf(corrector.correctionAge(100.0)));
}

TEST(EllipsoidalCorrector, AgeTracksTheLastGoodPair)
{
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  EXPECT_NEAR(corrector.correctionAge(110.0), 10.0, 1e-3);
  feedPair(corrector, 120.0);
  EXPECT_NEAR(corrector.correctionAge(121.0), 1.0, 1e-3);
}

TEST(EllipsoidalCorrector, TracksACorrectionThatChanges)
{
  // The term is near-constant at one site but moves with position; a long
  // transit must not leave the boat on a stale value.
  EllipsoidalCorrector corrector(0.05, 30.0);
  feedPair(corrector, 100.0);
  EXPECT_NEAR(*corrector.correction(), -0.6260, 1e-4);
  corrector.setRawFix(-25.0, 200.0);
  corrector.setGpsRaw(2.0, -25.9, 200.0);
  EXPECT_NEAR(*corrector.correction(), -0.9, 1e-9);
}

TEST(EllipsoidalCorrector, ZeroCorrectionIsStillAValidCorrection)
{
  // If a receiver's geoid ever agreed with EGM96 exactly, the correction is
  // zero -- which must not be confused with "no correction available".
  EllipsoidalCorrector corrector(0.05, 30.0);
  corrector.setRawFix(-25.8043, 100.0);
  corrector.setGpsRaw(kGpsMsl, -25.8043, 100.0);
  EXPECT_TRUE(corrector.hasCorrection(100.0));
  EXPECT_NEAR(*corrector.correction(), 0.0, 1e-12);
  const auto corrected = corrector.correct(-26.8020, 100.0);
  ASSERT_TRUE(corrected.has_value());
  EXPECT_NEAR(*corrected, -26.8020, 1e-12);
}

// --- Validity gating -------------------------------------------------------
//
// The failure these guard against is the one that matters most: a structurally
// valid message carrying a zero where a measurement should be, producing a
// ~26 m "correction" that publishes as a good fix on the topic every sounding
// depends on. mavros copies GPS_RAW_INT.alt_ellipsoid unconditionally, and that
// field is a MAVLink-v2 extension, so a v1 link delivers exactly this.

TEST(EllipsoidalCorrector, RejectsAbsentAltEllipsoidAsImplausible)
{
  // MAVLink v1: alt_ellipsoid absent, so mavros publishes 0 m. The caller's own
  // sentinel check is the first line of defence; this asserts the second one --
  // that even if a zero reaches the corrector it is refused, because
  // 0 - (-25.8043) = +25.8043 m is not a geoid disagreement.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  corrector.setRawFix(kRawFixAltitude, 100.0);
  corrector.setGpsRaw(kGpsMsl, 0.0, 100.0);
  EXPECT_FALSE(corrector.hasCorrection(100.0));
  EXPECT_FALSE(corrector.correction().has_value());
  EXPECT_FALSE(corrector.correct(-26.8020, 100.0).has_value());
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::Implausible);
  EXPECT_EQ(corrector.rejectedPairCount(), 1u);
}

TEST(EllipsoidalCorrector, ImplausiblePairDoesNotDisturbAGoodCorrection)
{
  // A link that drops to v1 mid-run must not overwrite the correction already
  // held; it must let it age out on its own.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0);
  const double established = *corrector.correction();
  corrector.setRawFix(kRawFixAltitude, 101.0);
  corrector.setGpsRaw(kGpsMsl, 0.0, 101.0);
  ASSERT_TRUE(corrector.correction().has_value());
  EXPECT_NEAR(*corrector.correction(), established, 1e-12);
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::Implausible);
  // ...and it still ages out rather than being held on the strength of the
  // rejected sample's stamp.
  EXPECT_FALSE(corrector.hasCorrection(131.0));
}

TEST(EllipsoidalCorrector, AcceptsACorrectionAtThePlausibilityBoundary)
{
  // The bound must not clip a legitimately larger geoid disagreement.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  corrector.setRawFix(-25.0, 100.0);
  corrector.setGpsRaw(kGpsMsl, -28.0, 100.0);
  ASSERT_TRUE(corrector.hasCorrection(100.0));
  EXPECT_NEAR(*corrector.correction(), -3.0, 1e-9);
}

TEST(EllipsoidalCorrector, RejectsSamplesFailingTheQualityGate)
{
  // Acquisition-time zeros: fix_type below 3D, or NavSatFix.status STATUS_NO_FIX.
  // The caller passes quality_ok=false and no correction may be latched.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  corrector.setRawFix(0.0, 100.0, /*quality_ok=*/false);
  corrector.setGpsRaw(0.0, 0.0, 100.0, /*quality_ok=*/false);
  EXPECT_FALSE(corrector.hasCorrection(100.0));
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::Incomplete);
  EXPECT_EQ(corrector.rejectedPairCount(), 0u);
}

TEST(EllipsoidalCorrector, ARejectedSampleIsDiscardedNotHeld)
{
  // The dangerous shape: a bad raw/fix arrives, then a good gps1/raw with a
  // coincident stamp. If the bad sample were merely ignored and left in place,
  // the good one would pair with it and latch a correction from rubbish.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  corrector.setRawFix(0.0, 100.0, /*quality_ok=*/false);
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.0, /*quality_ok=*/true);
  EXPECT_FALSE(corrector.hasCorrection(100.0));
  // A good raw/fix then completes a genuine pair.
  corrector.setRawFix(kRawFixAltitude, 100.0, /*quality_ok=*/true);
  ASSERT_TRUE(corrector.hasCorrection(100.0));
  EXPECT_NEAR(*corrector.correction(), kCorrection, 1e-9);
}

TEST(EllipsoidalCorrector, QualityLossDoesNotDisturbAGoodCorrection)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0);
  const double established = *corrector.correction();
  corrector.setGpsRaw(0.0, 0.0, 101.0, /*quality_ok=*/false);
  ASSERT_TRUE(corrector.correction().has_value());
  EXPECT_NEAR(*corrector.correction(), established, 1e-12);
}

TEST(EllipsoidalCorrector, RejectsNonFiniteAltitudes)
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();

  EllipsoidalCorrector a(0.05, 30.0, 3.0);
  a.setRawFix(nan, 100.0);
  a.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.0);
  EXPECT_FALSE(a.hasCorrection(100.0));

  EllipsoidalCorrector b(0.05, 30.0, 3.0);
  b.setRawFix(kRawFixAltitude, 100.0);
  b.setGpsRaw(kGpsMsl, inf, 100.0);
  EXPECT_FALSE(b.hasCorrection(100.0));
  EXPECT_FALSE(b.correction().has_value());
}

TEST(EllipsoidalCorrector, NonFiniteAltitudeNeverReachesTheOutput)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0);
  ASSERT_TRUE(corrector.hasCorrection(100.0));
  EXPECT_FALSE(
    corrector.correct(std::numeric_limits<double>::quiet_NaN(), 100.0).has_value());
}

// --- Clock steps -----------------------------------------------------------

TEST(EllipsoidalCorrector, BackwardClockStepMakesTheCorrectionStale)
{
  // Message stamps come from the FCU-synchronized clock, `now` from the node
  // clock. One boot-time sync can put `now` behind a correction established
  // moments earlier. A raw subtraction gives a negative age, which compares
  // <= timeout for ever: the correction would be pinned as permanently fresh
  // and the boat would run the rest of the session on a stale value.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 1000.0);
  ASSERT_TRUE(corrector.hasCorrection(1000.0));

  EXPECT_TRUE(std::isinf(corrector.correctionAge(900.0)));
  EXPECT_FALSE(corrector.hasCorrection(900.0));
  EXPECT_FALSE(corrector.correct(-26.8020, 900.0).has_value());
}

TEST(EllipsoidalCorrector, AgeIsNeverNegative)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0);
  // Within the pair tolerance a slightly-behind clock is ordinary jitter.
  EXPECT_GE(corrector.correctionAge(100.0 - 0.01), 0.0);
  EXPECT_NEAR(corrector.correctionAge(100.0 - 0.01), 0.0, 1e-12);
  EXPECT_TRUE(corrector.hasCorrection(100.0 - 0.01));
}

TEST(EllipsoidalCorrector, RecoversAfterABackwardClockStep)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 1000.0);
  ASSERT_FALSE(corrector.hasCorrection(900.0));
  // The next coincident pair re-establishes against the new clock.
  feedPair(corrector, 900.0);
  EXPECT_TRUE(corrector.hasCorrection(900.0));
}

// --- Undulation snapshots --------------------------------------------------

TEST(EllipsoidalCorrector, UndulationsComeFromOnePairNotMixedSamples)
{
  // Once the topics decouple the reported undulations must keep describing the
  // last good pair. Recomputing them live from the latest samples would mix an
  // old gps1/raw with a new raw/fix and drift silently -- and these two values
  // exist precisely to make such a drift visible.
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0);
  const double receiver = *corrector.receiverUndulation();
  const double mavros = *corrector.mavrosUndulation();

  // gps1/raw stops; raw/fix keeps arriving with a changing altitude.
  corrector.setRawFix(kRawFixAltitude + 0.5, 105.0);
  corrector.setRawFix(kRawFixAltitude + 1.5, 110.0);

  EXPECT_NEAR(*corrector.receiverUndulation(), receiver, 1e-12);
  EXPECT_NEAR(*corrector.mavrosUndulation(), mavros, 1e-12);
  // The identity that defines the method still holds on the reported values.
  EXPECT_NEAR(
    *corrector.receiverUndulation() - *corrector.mavrosUndulation(),
    *corrector.correction(), 1e-9);
}

TEST(EllipsoidalCorrector, NoUndulationsBeforeAPair)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  corrector.setRawFix(kRawFixAltitude, 100.0);
  EXPECT_FALSE(corrector.receiverUndulation().has_value());
  EXPECT_FALSE(corrector.mavrosUndulation().has_value());
  EXPECT_FALSE(corrector.pairSkew().has_value());
}

// --- Diagnosability --------------------------------------------------------

TEST(EllipsoidalCorrector, ReportsThePairSkew)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  feedPair(corrector, 100.0, 0.02);
  ASSERT_TRUE(corrector.pairSkew().has_value());
  EXPECT_NEAR(*corrector.pairSkew(), 0.02, 1e-9);
}

TEST(EllipsoidalCorrector, ReportsWhyPairingFailed)
{
  EllipsoidalCorrector corrector(0.05, 30.0, 3.0);
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::Incomplete);

  corrector.setRawFix(kRawFixAltitude, 100.0);
  corrector.setGpsRaw(kGpsMsl, kGpsEllipsoid, 100.2);
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::NotCoincident);

  feedPair(corrector, 200.0);
  EXPECT_EQ(corrector.lastReject(), EllipsoidalCorrector::Reject::None);
}
