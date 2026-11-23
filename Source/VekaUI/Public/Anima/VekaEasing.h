// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Anima/VekaAnimationSpec.h"

namespace Veka::Easing
{
	/** Eased progress for T in 0..1 (Spring is not a timed curve: returns T). */
	VEKAUI_API double Evaluate(EVekaAnimationCurve Curve, double T, const FVector4& TimingCurve = FVector4(0.42, 0.0, 0.58, 1.0));

	/** CSS cubic-bezier(X1, Y1, X2, Y2) evaluated at progress T. */
	VEKAUI_API double CubicBezier(double X1, double Y1, double X2, double Y2, double T);
}

namespace Veka::Spring
{
	struct FSample
	{
		double Position = 0.0;
		double Velocity = 0.0;
	};

	/** Analytic damped spring for a unit step: starts at 0, rests at 1. InitialVelocity in units/s. */
	VEKAUI_API FSample Evaluate(double Mass, double Stiffness, double Damping, double InitialVelocity, double Time);

	/** Semi-implicit Euler step toward Target (sub-stepped for stability). */
	VEKAUI_API void Step(double& Position, double& Velocity, double Target, double Mass, double Stiffness, double Damping, double DeltaTime);

	/** Time until |x - 1| and |v| stay under Epsilon (capped at 10 s). */
	VEKAUI_API double EstimateSettlingTime(double Mass, double Stiffness, double Damping, double InitialVelocity = 0.0, double Epsilon = 1.0e-3);
}
