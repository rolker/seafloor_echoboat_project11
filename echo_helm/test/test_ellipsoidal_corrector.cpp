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

// Feed one coincident pair, as mavros delivers it: both messages built from the
// same GPS_RAW_INT, stamped microseconds apart at publish time.
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
