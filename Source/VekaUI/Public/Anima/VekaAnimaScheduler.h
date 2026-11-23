// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Anima/VekaAnimationSpec.h"

struct FVekaAnimationHandle
{
	uint64 Id = 0;
	bool IsValid() const { return Id != 0; }
};

/**
 * One scheduler per runtime (not one tick per widget). Active animations live in a flat array; the host
 * ticks the scheduler only while something animates.
 */
class VEKAUI_API FVekaAnimaScheduler
{
public:
	/**
	 * Animates a scalar from From to To. OnUpdate gets every new value (including the last one).
	 * If Channel is set and an animation already runs on it, that animation is retargeted: springs keep
	 * their current value and velocity, timed curves restart from the current value.
	 */
	FVekaAnimationHandle Animate(const FVekaAnimationSpec& Spec, double From, double To,
		TFunction<void(double)> OnUpdate, TFunction<void()> OnComplete = nullptr, const void* Channel = nullptr);

	void Cancel(FVekaAnimationHandle Handle);
	void CancelChannel(const void* Channel);
	void CancelAll();

	/** Advances every animation. Returns true while animations remain. */
	bool Tick(double DeltaSeconds);

	bool IsAnimating() const { return Active.Num() > 0; }
	int32 Num() const { return Active.Num(); }

private:
	struct FActiveAnimation
	{
		uint64 Id = 0;
		const void* Channel = nullptr;
		FVekaAnimationSpec Spec;
		double From = 0.0;
		double To = 1.0;
		double Elapsed = 0.0;
		double Value = 0.0;
		double Velocity = 0.0;
		int32 Iteration = 0;
		bool bReversed = false;
		bool bFinished = false;
		/** Shared, so the per-frame calls copy a pointer, not the function (TFunction allocates). */
		TSharedPtr<TFunction<void(double)>> OnUpdate;
		TSharedPtr<TFunction<void()>> OnComplete;
	};

	void Advance(FActiveAnimation& Animation, double DeltaSeconds) const;

	TArray<FActiveAnimation> Active;
	uint64 NextId = 1;
};
