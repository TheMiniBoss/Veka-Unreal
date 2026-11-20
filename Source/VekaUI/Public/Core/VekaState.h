// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Anima/VekaAnimationSpec.h"
#include "Colora/VekaColor.h"
#include "Layout/Margin.h"
#include <type_traits>

class FVekaNode;

/** Storage of one @State value. Lives in the node of the view that declared it, survives re-evaluations. */
struct VEKAUI_API FVekaStateBoxBase
{
	virtual ~FVekaStateBoxBase() = default;

	/** Node that owns the state; re-evaluated when the value changes. */
	TWeakPtr<FVekaNode> Owner;

	/** sizeof(T), used to catch a name reused with another type. */
	SIZE_T TypeSize = 0;

	/** Marks the owner view dirty (re-evaluated at the end of the frame, changes are coalesced). */
	void NotifyChanged() const;

	/** Drives a numeric value with the animation scheduler of the owner's runtime. */
	void AnimateScalar(double From, double To, const FVekaAnimationSpec& Spec, TFunction<void(double)> Apply) const;

	/** Drives a progress 0 -> 1 (restarts on each call); for values that are not a single number. */
	void AnimateProgress(const FVekaAnimationSpec& Spec, TFunction<void(double)> Apply) const;
};

/**
 * Types TVekaState::Animate can interpolate. Numbers animate directly (springs keep their velocity on retarget);
 * the others interpolate a progress with Lerp (a retarget restarts from the presented value).
 */
template <typename T>
struct TVekaAnimatable
{
	static constexpr bool bSupported = std::is_arithmetic_v<T>;
};

template <>
struct TVekaAnimatable<FVector2D>
{
	static constexpr bool bSupported = true;
	static FVector2D Lerp(const FVector2D& A, const FVector2D& B, double T) { return A + (B - A) * T; }
};

template <>
struct TVekaAnimatable<FLinearColor>
{
	static constexpr bool bSupported = true;
	static FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, double T) { return Veka::Colora::Lerp(A, B, static_cast<float>(T), EVekaColorSpace::OkLab); }
};

template <>
struct TVekaAnimatable<FMargin>
{
	static constexpr bool bSupported = true;
	static FMargin Lerp(const FMargin& A, const FMargin& B, double T)
	{
		const float F = static_cast<float>(T);
		return FMargin(FMath::Lerp(A.Left, B.Left, F), FMath::Lerp(A.Top, B.Top, F), FMath::Lerp(A.Right, B.Right, F), FMath::Lerp(A.Bottom, B.Bottom, F));
	}
};

template <typename T>
struct TVekaStateBox final : public FVekaStateBoxBase
{
	T Value{};
};

template <typename T>
class TVekaState;

/** Two-way access to a value owned elsewhere (SwiftUI's Binding / `$value`). A TVekaState converts to it. */
template <typename T>
class TVekaBinding
{
public:
	TVekaBinding() = default;
	/** Binding to a @State (controls: Veka::Toggle(Title, OnState)). */
	TVekaBinding(const TVekaState<T>& State);
	TVekaBinding(TFunction<T()> InGetter, TFunction<void(const T&)> InSetter)
		: Getter(MoveTemp(InGetter)), Setter(MoveTemp(InSetter))
	{
	}

	/** Read-only binding to a constant. */
	static TVekaBinding Constant(const T& Value)
	{
		return TVekaBinding([Value]() { return Value; }, [](const T&) {});
	}

	T Get() const { return Getter ? Getter() : T{}; }
	void Set(const T& Value) const
	{
		if (Setter)
		{
			Setter(Value);
		}
	}
	bool IsBound() const { return static_cast<bool>(Getter); }

private:
	TFunction<T()> Getter;
	TFunction<void(const T&)> Setter;
};

/**
 * Handle to a @State value. Cheap to copy; copies point to the same storage, so it can be captured in
 * gesture lambdas. Get it from FVekaContext::State inside a view Body.
 */
template <typename T>
class TVekaState
{
public:
	TVekaState() = default;
	explicit TVekaState(TSharedRef<TVekaStateBox<T>> InBox) : Box(MoveTemp(InBox)) {}

	bool IsValid() const { return Box.IsValid(); }

	const T& Get() const
	{
		check(Box.IsValid());
		return Box->Value;
	}
	const T& operator*() const { return Get(); }
	const T* operator->() const { return &Get(); }

	/** Sets the value. No-op if equal (when T has operator==). Otherwise the owner view is re-evaluated. */
	void Set(const T& NewValue) const
	{
		check(Box.IsValid());
		if constexpr (requires(const T& A, const T& B) { A == B; })
		{
			if (Box->Value == NewValue)
			{
				return;
			}
		}
		Box->Value = NewValue;
		Box->NotifyChanged();
	}

	/** In-place change (containers). Always notifies. */
	template <typename FunctorType>
	void Modify(FunctorType&& Functor) const
	{
		check(Box.IsValid());
		Functor(Box->Value);
		Box->NotifyChanged();
	}

	/** Two-way binding for controls (Toggle(isOn: $state)). */
	TVekaBinding<T> Binding() const
	{
		TSharedRef<TVekaStateBox<T>> Shared = Box.ToSharedRef();
		return TVekaBinding<T>(
			[Shared]() { return Shared->Value; },
			[Shared](const T& Value) { TVekaState<T>(Shared).Set(Value); });
	}

	/**
	 * Animates the state to Target (numbers, FVector2D, FLinearColor, FMargin; see TVekaAnimatable).
	 * A new call on the same state retargets from the presented value.
	 */
	void Animate(const T& Target, const FVekaAnimationSpec& Spec) const
	{
		static_assert(TVekaAnimatable<T>::bSupported, "TVekaState::Animate: T needs a TVekaAnimatable specialization. Use `.Animation(spec, value)` for view properties.");
		check(Box.IsValid());
		TSharedRef<TVekaStateBox<T>> Shared = Box.ToSharedRef();
		if constexpr (std::is_arithmetic_v<T>)
		{
			Box->AnimateScalar(static_cast<double>(Box->Value), static_cast<double>(Target), Spec,
				[Shared](double Value)
				{
					Shared->Value = static_cast<T>(Value);
					Shared->NotifyChanged();
				});
		}
		else
		{
			const T From = Box->Value;
			Box->AnimateProgress(Spec, [Shared, From, Target](double Progress)
			{
				Shared->Value = TVekaAnimatable<T>::Lerp(From, Target, Progress);
				Shared->NotifyChanged();
			});
		}
	}

private:
	TSharedPtr<TVekaStateBox<T>> Box;
};

template <typename T>
TVekaBinding<T>::TVekaBinding(const TVekaState<T>& State)
	: TVekaBinding(State.Binding())
{
}
