// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaEasing.h"

namespace Veka::Easing
{
	double CubicBezier(double X1, double Y1, double X2, double Y2, double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		if (T <= 0.0 || T >= 1.0)
		{
			return T;
		}

		// B(s) = 3(1-s)²s·P1 + 3(1-s)s²·P2 + s³ (P0 = 0, P3 = 1), solved for x(s) = T.
		const double Cx = 3.0 * X1;
		const double Bx = 3.0 * (X2 - X1) - Cx;
		const double Ax = 1.0 - Cx - Bx;
		const double Cy = 3.0 * Y1;
		const double By = 3.0 * (Y2 - Y1) - Cy;
		const double Ay = 1.0 - Cy - By;

		auto SampleX = [&](double S) { return ((Ax * S + Bx) * S + Cx) * S; };
		auto SampleY = [&](double S) { return ((Ay * S + By) * S + Cy) * S; };
		auto SampleDX = [&](double S) { return (3.0 * Ax * S + 2.0 * Bx) * S + Cx; };

		// Newton-Raphson, then bisection fallback.
		double S = T;
		for (int32 Iteration = 0; Iteration < 8; ++Iteration)
		{
			const double Error = SampleX(S) - T;
			if (FMath::Abs(Error) < 1.0e-7)
			{
				return SampleY(S);
			}
			const double Derivative = SampleDX(S);
			if (FMath::Abs(Derivative) < 1.0e-6)
			{
				break;
			}
			S -= Error / Derivative;
		}

		double Low = 0.0;
		double High = 1.0;
		S = T;
		for (int32 Iteration = 0; Iteration < 64; ++Iteration)
		{
			const double X = SampleX(S);
			if (FMath::Abs(X - T) < 1.0e-7)
			{
				break;
			}
			if (X < T)
			{
				Low = S;
			}
			else
			{
				High = S;
			}
			S = (Low + High) * 0.5;
		}
		return SampleY(S);
	}

	double Evaluate(EVekaAnimationCurve Curve, double T, const FVector4& TimingCurve)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		switch (Curve)
		{
		case EVekaAnimationCurve::Linear:
			return T;
		case EVekaAnimationCurve::EaseIn:
			return CubicBezier(0.42, 0.0, 1.0, 1.0, T);
		case EVekaAnimationCurve::EaseOut:
			return CubicBezier(0.0, 0.0, 0.58, 1.0, T);
		case EVekaAnimationCurve::EaseInOut:
			return CubicBezier(0.42, 0.0, 0.58, 1.0, T);
		case EVekaAnimationCurve::CircEaseIn:
			return 1.0 - FMath::Sqrt(FMath::Max(0.0, 1.0 - T * T));
		case EVekaAnimationCurve::CircEaseOut:
			return FMath::Sqrt(FMath::Max(0.0, 1.0 - FMath::Square(T - 1.0)));
		case EVekaAnimationCurve::CircEaseInOut:
			return T < 0.5
				? (1.0 - FMath::Sqrt(FMath::Max(0.0, 1.0 - FMath::Square(2.0 * T)))) * 0.5
				: (FMath::Sqrt(FMath::Max(0.0, 1.0 - FMath::Square(-2.0 * T + 2.0))) + 1.0) * 0.5;
		case EVekaAnimationCurve::TimingCurve:
			return CubicBezier(TimingCurve.X, TimingCurve.Y, TimingCurve.Z, TimingCurve.W, T);
		case EVekaAnimationCurve::Spring:
		default:
			return T;
		}
	}
}

namespace Veka::Spring
{
	namespace
	{
		/** Displacement x(t) = position - target for x(0) = X0, x'(0) = V0. */
		double Displacement(double Mass, double Stiffness, double Damping, double X0, double V0, double Time)
		{
			const double Omega0 = FMath::Sqrt(Stiffness / Mass);
			const double Zeta = Damping / (2.0 * FMath::Sqrt(Stiffness * Mass));

			if (Zeta < 1.0 - 1.0e-6)
			{
				const double OmegaD = Omega0 * FMath::Sqrt(1.0 - Zeta * Zeta);
				const double Envelope = FMath::Exp(-Zeta * Omega0 * Time);
				return Envelope * (X0 * FMath::Cos(OmegaD * Time) + ((V0 + Zeta * Omega0 * X0) / OmegaD) * FMath::Sin(OmegaD * Time));
			}
			if (Zeta <= 1.0 + 1.0e-6)
			{
				return FMath::Exp(-Omega0 * Time) * (X0 + (V0 + Omega0 * X0) * Time);
			}
			const double Root = FMath::Sqrt(Zeta * Zeta - 1.0);
			const double R1 = -Omega0 * (Zeta - Root);
			const double R2 = -Omega0 * (Zeta + Root);
			const double C1 = (V0 - R2 * X0) / (R1 - R2);
			const double C2 = X0 - C1;
			return C1 * FMath::Exp(R1 * Time) + C2 * FMath::Exp(R2 * Time);
		}
	}

	FSample Evaluate(double Mass, double Stiffness, double Damping, double InitialVelocity, double Time)
	{
		Mass = FMath::Max(Mass, 1.0e-4);
		Stiffness = FMath::Max(Stiffness, 1.0e-4);
		Damping = FMath::Max(Damping, 0.0);
		Time = FMath::Max(Time, 0.0);

		constexpr double H = 1.0e-4;
		const double X = Displacement(Mass, Stiffness, Damping, -1.0, InitialVelocity, Time);
		const double XNext = Displacement(Mass, Stiffness, Damping, -1.0, InitialVelocity, Time + H);
		const double XPrev = Displacement(Mass, Stiffness, Damping, -1.0, InitialVelocity, FMath::Max(0.0, Time - H));
		const double Velocity = Time > H ? (XNext - XPrev) / (2.0 * H) : (XNext - X) / H;

		FSample Sample;
		Sample.Position = 1.0 + X;
		Sample.Velocity = Velocity;
		return Sample;
	}

	void Step(double& Position, double& Velocity, double Target, double Mass, double Stiffness, double Damping, double DeltaTime)
	{
		Mass = FMath::Max(Mass, 1.0e-4);
		constexpr double MaxSubStep = 1.0 / 240.0;
		double Remaining = FMath::Max(DeltaTime, 0.0);
		while (Remaining > 0.0)
		{
			const double Dt = FMath::Min(Remaining, MaxSubStep);
			const double Acceleration = (-Stiffness * (Position - Target) - Damping * Velocity) / Mass;
			Velocity += Acceleration * Dt;
			Position += Velocity * Dt;
			Remaining -= Dt;
		}
	}

	double EstimateSettlingTime(double Mass, double Stiffness, double Damping, double InitialVelocity, double Epsilon)
	{
		double Position = 0.0;
		double Velocity = InitialVelocity;
		constexpr double Dt = 1.0 / 240.0;
		double Time = 0.0;
		double SettledSince = -1.0;
		while (Time < 10.0)
		{
			Step(Position, Velocity, 1.0, Mass, Stiffness, Damping, Dt);
			Time += Dt;
			const bool bSettled = FMath::Abs(Position - 1.0) < Epsilon && FMath::Abs(Velocity) < Epsilon;
			if (bSettled)
			{
				if (SettledSince < 0.0)
				{
					SettledSince = Time;
				}
				if (Time - SettledSince > 0.1)
				{
					return SettledSince;
				}
			}
			else
			{
				SettledSince = -1.0;
			}
		}
		return 10.0;
	}
}
