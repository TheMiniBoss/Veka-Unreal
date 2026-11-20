// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VekaAnimationSpec.generated.h"

class UCurveFloat;

UENUM(BlueprintType)
enum class EVekaAnimationCurve : uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
	CircEaseIn,
	CircEaseOut,
	CircEaseInOut,
	/** Cubic Bézier (x1, y1, x2, y2), like CSS cubic-bezier / SwiftUI timingCurve. */
	TimingCurve,
	/** Physical spring (mass, stiffness, damping). Duration is ignored: the spring settles on its own. */
	Spring,
	/** UCurveFloat (CustomCurve): its time range is mapped to the duration, its value is the progress. */
	Custom
};

/** How a value moves from its current value to a new one. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAnimationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	EVekaAnimationCurve Curve = EVekaAnimationCurve::EaseInOut;

	/** Seconds (ignored by springs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima", meta = (ClampMin = "0"))
	double Duration = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima", meta = (ClampMin = "0"))
	double Delay = 0.0;

	/** Time multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima", meta = (ClampMin = "0.01"))
	double Speed = 1.0;

	/** Extra iterations after the first one. -1 = forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima", meta = (ClampMin = "-1"))
	int32 RepeatCount = 0;

	/** Every other iteration plays backward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	bool bAutoReverse = false;

	/** Control points (x1, y1, x2, y2) when Curve = TimingCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	FVector4 TimingCurve = FVector4(0.42, 0.0, 0.58, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima|Spring", meta = (ClampMin = "0.001"))
	double Mass = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima|Spring", meta = (ClampMin = "0.001"))
	double Stiffness = 130.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima|Spring", meta = (ClampMin = "0"))
	double Damping = 18.85;

	/** Initial velocity in value units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima|Spring")
	double InitialVelocity = 0.0;

	/** Curve = Custom. Keep the curve asset loaded while the animation runs (assets are, by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	TObjectPtr<UCurveFloat> CustomCurve;

	static FVekaAnimationSpec Linear(double InDuration = 0.35);
	static FVekaAnimationSpec EaseIn(double InDuration = 0.35);
	static FVekaAnimationSpec EaseOut(double InDuration = 0.35);
	static FVekaAnimationSpec EaseInOut(double InDuration = 0.35);
	static FVekaAnimationSpec Timing(double X1, double Y1, double X2, double Y2, double InDuration = 0.35);

	/**
	 * Spring defined like SwiftUI: Response = period of the undamped spring (s), DampingFraction = 1 no bounce,
	 * < 1 bounces. Default (0.55, 0.825) is SwiftUI's default spring.
	 */
	static FVekaAnimationSpec Spring(double Response = 0.55, double DampingFraction = 0.825, double InInitialVelocity = 0.0);

	/** Spring with explicit physical parameters. */
	static FVekaAnimationSpec InterpolatingSpring(double InMass, double InStiffness, double InDamping, double InInitialVelocity = 0.0);

	/** Progress follows a UCurveFloat (time range mapped to Duration, value 0..1 = from..to). */
	static FVekaAnimationSpec FromCurve(UCurveFloat* InCurve, double InDuration = 0.35);

	/** Eased progress of a timed curve at T (0..1), custom curves included. */
	double EvaluateProgress(double T) const;

	static FVekaAnimationSpec Smooth() { return Spring(0.5, 1.0); }
	static FVekaAnimationSpec Snappy() { return Spring(0.5, 0.85); }
	static FVekaAnimationSpec Bouncy() { return Spring(0.5, 0.7); }

	FVekaAnimationSpec Delayed(double Seconds) const;
	FVekaAnimationSpec Repeat(int32 Count, bool bInAutoReverse = true) const;
	FVekaAnimationSpec RepeatForever(bool bInAutoReverse = true) const;
	FVekaAnimationSpec WithSpeed(double InSpeed) const;

	bool IsSpring() const { return Curve == EVekaAnimationCurve::Spring; }
};
