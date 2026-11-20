// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaAnimationSpec.h"

namespace
{
	FVekaAnimationSpec MakeTimed(EVekaAnimationCurve Curve, double Duration)
	{
		FVekaAnimationSpec Spec;
		Spec.Curve = Curve;
		Spec.Duration = FMath::Max(0.0, Duration);
		return Spec;
	}
}

FVekaAnimationSpec FVekaAnimationSpec::Linear(double InDuration) { return MakeTimed(EVekaAnimationCurve::Linear, InDuration); }
FVekaAnimationSpec FVekaAnimationSpec::EaseIn(double InDuration) { return MakeTimed(EVekaAnimationCurve::EaseIn, InDuration); }
FVekaAnimationSpec FVekaAnimationSpec::EaseOut(double InDuration) { return MakeTimed(EVekaAnimationCurve::EaseOut, InDuration); }
FVekaAnimationSpec FVekaAnimationSpec::EaseInOut(double InDuration) { return MakeTimed(EVekaAnimationCurve::EaseInOut, InDuration); }

FVekaAnimationSpec FVekaAnimationSpec::Timing(double X1, double Y1, double X2, double Y2, double InDuration)
{
	FVekaAnimationSpec Spec = MakeTimed(EVekaAnimationCurve::TimingCurve, InDuration);
	Spec.TimingCurve = FVector4(FMath::Clamp(X1, 0.0, 1.0), Y1, FMath::Clamp(X2, 0.0, 1.0), Y2);
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::Spring(double Response, double DampingFraction, double InInitialVelocity)
{
	// SwiftUI: stiffness = (2π / response)² · mass, damping = 4π · dampingFraction · mass / response.
	const double SafeResponse = FMath::Max(Response, 0.01);
	FVekaAnimationSpec Spec;
	Spec.Curve = EVekaAnimationCurve::Spring;
	Spec.Mass = 1.0;
	Spec.Stiffness = FMath::Square(UE_DOUBLE_TWO_PI / SafeResponse);
	Spec.Damping = 4.0 * UE_DOUBLE_PI * FMath::Max(DampingFraction, 0.0) / SafeResponse;
	Spec.InitialVelocity = InInitialVelocity;
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::InterpolatingSpring(double InMass, double InStiffness, double InDamping, double InInitialVelocity)
{
	FVekaAnimationSpec Spec;
	Spec.Curve = EVekaAnimationCurve::Spring;
	Spec.Mass = FMath::Max(InMass, 0.001);
	Spec.Stiffness = FMath::Max(InStiffness, 0.001);
	Spec.Damping = FMath::Max(InDamping, 0.0);
	Spec.InitialVelocity = InInitialVelocity;
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::Delayed(double Seconds) const
{
	FVekaAnimationSpec Spec = *this;
	Spec.Delay = FMath::Max(0.0, Seconds);
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::Repeat(int32 Count, bool bInAutoReverse) const
{
	FVekaAnimationSpec Spec = *this;
	Spec.RepeatCount = FMath::Max(Count, 0);
	Spec.bAutoReverse = bInAutoReverse;
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::RepeatForever(bool bInAutoReverse) const
{
	FVekaAnimationSpec Spec = *this;
	Spec.RepeatCount = -1;
	Spec.bAutoReverse = bInAutoReverse;
	return Spec;
}

FVekaAnimationSpec FVekaAnimationSpec::WithSpeed(double InSpeed) const
{
	FVekaAnimationSpec Spec = *this;
	Spec.Speed = FMath::Max(InSpeed, 0.01);
	return Spec;
}
