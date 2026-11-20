// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaAnimaScheduler.h"
#include "Anima/VekaEasing.h"

FVekaAnimationHandle FVekaAnimaScheduler::Animate(const FVekaAnimationSpec& Spec, double From, double To,
	TFunction<void(double)> OnUpdate, TFunction<void()> OnComplete, const void* Channel)
{
	if (Channel)
	{
		if (FActiveAnimation* Existing = Active.FindByPredicate([Channel](const FActiveAnimation& A) { return A.Channel == Channel && !A.bFinished; }))
		{
			// Retarget from the presented value (springs keep their velocity).
			const double CurrentValue = Existing->Value;
			const double CurrentVelocity = Existing->Velocity;
			const bool bKeepVelocity = Existing->Spec.IsSpring() && Spec.IsSpring();
			Existing->Spec = Spec;
			Existing->From = CurrentValue;
			Existing->To = To;
			Existing->Elapsed = 0.0;
			Existing->Value = CurrentValue;
			Existing->Velocity = bKeepVelocity ? CurrentVelocity : Spec.InitialVelocity;
			Existing->Iteration = 0;
			Existing->bReversed = false;
			Existing->OnUpdate = MoveTemp(OnUpdate);
			Existing->OnComplete = MoveTemp(OnComplete);
			FVekaAnimationHandle Handle;
			Handle.Id = Existing->Id;
			return Handle;
		}
	}

	FActiveAnimation& Animation = Active.AddDefaulted_GetRef();
	Animation.Id = NextId++;
	Animation.Channel = Channel;
	Animation.Spec = Spec;
	Animation.From = From;
	Animation.To = To;
	Animation.Value = From;
	Animation.Velocity = Spec.InitialVelocity;
	Animation.OnUpdate = MoveTemp(OnUpdate);
	Animation.OnComplete = MoveTemp(OnComplete);

	FVekaAnimationHandle Handle;
	Handle.Id = Animation.Id;
	return Handle;
}

void FVekaAnimaScheduler::Cancel(FVekaAnimationHandle Handle)
{
	Active.RemoveAll([Handle](const FActiveAnimation& A) { return A.Id == Handle.Id; });
}

void FVekaAnimaScheduler::CancelChannel(const void* Channel)
{
	Active.RemoveAll([Channel](const FActiveAnimation& A) { return A.Channel == Channel; });
}

void FVekaAnimaScheduler::CancelAll()
{
	Active.Reset();
}

void FVekaAnimaScheduler::Advance(FActiveAnimation& Animation, double DeltaSeconds) const
{
	const FVekaAnimationSpec& Spec = Animation.Spec;
	double Dt = DeltaSeconds * FMath::Max(Spec.Speed, 0.01);

	// Delay
	if (Animation.Elapsed < Spec.Delay)
	{
		const double Wait = FMath::Min(Dt, Spec.Delay - Animation.Elapsed);
		Animation.Elapsed += Wait;
		Dt -= Wait;
		if (Dt <= 0.0)
		{
			return;
		}
	}

	const double Start = Animation.bReversed ? Animation.To : Animation.From;
	const double End = Animation.bReversed ? Animation.From : Animation.To;

	bool bIterationDone = false;
	if (Spec.IsSpring())
	{
		Veka::Spring::Step(Animation.Value, Animation.Velocity, End, Spec.Mass, Spec.Stiffness, Spec.Damping, Dt);
		const double Range = FMath::Max(1.0, FMath::Abs(End - Start));
		const double Epsilon = 1.0e-3 * Range;
		if (FMath::Abs(Animation.Value - End) < Epsilon && FMath::Abs(Animation.Velocity) < Epsilon * 10.0)
		{
			Animation.Value = End;
			Animation.Velocity = 0.0;
			bIterationDone = true;
		}
		Animation.Elapsed += Dt;
	}
	else
	{
		Animation.Elapsed += Dt;
		const double Duration = FMath::Max(Spec.Duration, 1.0e-4);
		const double T = FMath::Clamp((Animation.Elapsed - Spec.Delay) / Duration, 0.0, 1.0);
		const double Eased = Spec.EvaluateProgress(T);
		Animation.Value = Start + (End - Start) * Eased;
		bIterationDone = T >= 1.0;
	}

	if (!bIterationDone)
	{
		return;
	}

	const bool bMoreIterations = Spec.RepeatCount < 0 || Animation.Iteration < Spec.RepeatCount;
	if (bMoreIterations)
	{
		++Animation.Iteration;
		Animation.Elapsed = Spec.Delay;
		if (Spec.bAutoReverse)
		{
			Animation.bReversed = !Animation.bReversed;
		}
		else
		{
			Animation.Value = Animation.From;
		}
		Animation.Velocity = Spec.InitialVelocity;
	}
	else
	{
		Animation.bFinished = true;
	}
}

bool FVekaAnimaScheduler::Tick(double DeltaSeconds)
{
	if (Active.IsEmpty())
	{
		return false;
	}

	// Clamp huge frames (breakpoints, hitches) so springs stay stable.
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0, 0.1);

	// 1) Advance every animation. 2) Call callbacks on copies: callbacks may start or retarget
	// animations, which can reallocate the array.
	struct FPendingCall
	{
		uint64 Id;
		double Value;
		bool bFinished;
		TFunction<void(double)> OnUpdate;
		TFunction<void()> OnComplete;
	};
	TArray<FPendingCall> Calls;
	Calls.Reserve(Active.Num());

	for (FActiveAnimation& Animation : Active)
	{
		Advance(Animation, DeltaSeconds);
		Calls.Add({ Animation.Id, Animation.Value, Animation.bFinished, Animation.OnUpdate, Animation.bFinished ? Animation.OnComplete : nullptr });
	}

	// Remove finished ones before callbacks so a callback can start a new animation on the same channel.
	Active.RemoveAll([](const FActiveAnimation& A) { return A.bFinished; });

	for (FPendingCall& Call : Calls)
	{
		if (Call.OnUpdate)
		{
			Call.OnUpdate(Call.Value);
		}
		if (Call.bFinished && Call.OnComplete)
		{
			Call.OnComplete();
		}
	}

	return !Active.IsEmpty();
}
