// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaTransition.h"

namespace
{
	FVekaTransitionEffect MakeEffect(EVekaTransitionKind Kind, EVekaEdge Edge = EVekaEdge::Leading, const FVector2D& Offset = FVector2D::ZeroVector, float Amount = 0.f)
	{
		FVekaTransitionEffect Effect;
		Effect.Kind = Kind;
		Effect.Edge = Edge;
		Effect.Offset = Offset;
		Effect.Amount = Amount;
		return Effect;
	}

	FVekaTransition Symmetric(const FVekaTransitionEffect& Effect)
	{
		FVekaTransition Transition;
		Transition.Insertion.Add(Effect);
		Transition.Removal.Add(Effect);
		return Transition;
	}

	EVekaEdge Opposite(EVekaEdge Edge)
	{
		switch (Edge)
		{
		case EVekaEdge::Leading: return EVekaEdge::Trailing;
		case EVekaEdge::Trailing: return EVekaEdge::Leading;
		case EVekaEdge::Top: return EVekaEdge::Bottom;
		default: return EVekaEdge::Top;
		}
	}

	FVector2D EdgeDirection(EVekaEdge Edge)
	{
		switch (Edge)
		{
		case EVekaEdge::Leading: return FVector2D(-1.0, 0.0);
		case EVekaEdge::Trailing: return FVector2D(1.0, 0.0);
		case EVekaEdge::Top: return FVector2D(0.0, -1.0);
		default: return FVector2D(0.0, 1.0);
		}
	}
}

FVekaTransition FVekaTransition::Identity() { return Symmetric(MakeEffect(EVekaTransitionKind::Identity)); }
FVekaTransition FVekaTransition::Opacity() { return Symmetric(MakeEffect(EVekaTransitionKind::Opacity)); }
FVekaTransition FVekaTransition::Scale(float From) { return Symmetric(MakeEffect(EVekaTransitionKind::Scale, EVekaEdge::Leading, FVector2D::ZeroVector, From)); }
FVekaTransition FVekaTransition::Move(EVekaEdge Edge) { return Symmetric(MakeEffect(EVekaTransitionKind::Move, Edge)); }
FVekaTransition FVekaTransition::Offset(const FVector2D& Delta) { return Symmetric(MakeEffect(EVekaTransitionKind::Offset, EVekaEdge::Leading, Delta)); }
FVekaTransition FVekaTransition::Blur(float Radius) { return Symmetric(MakeEffect(EVekaTransitionKind::Blur, EVekaEdge::Leading, FVector2D::ZeroVector, Radius)); }

FVekaTransition FVekaTransition::Slide()
{
	return Asymmetric(Move(EVekaEdge::Leading), Move(EVekaEdge::Trailing));
}

FVekaTransition FVekaTransition::Push(EVekaEdge From)
{
	return Asymmetric(Combined(Move(From), Opacity()), Combined(Move(Opposite(From)), Opacity()));
}

FVekaTransition FVekaTransition::Combined(const FVekaTransition& A, const FVekaTransition& B)
{
	FVekaTransition Out = A;
	Out.Insertion.Append(B.Insertion);
	Out.Removal.Append(B.Removal);
	return Out;
}

FVekaTransition FVekaTransition::Asymmetric(const FVekaTransition& InInsertion, const FVekaTransition& InRemoval)
{
	FVekaTransition Out;
	Out.Insertion = InInsertion.Insertion;
	Out.Removal = InRemoval.Removal;
	return Out;
}

float FVekaTransition::EvaluateOpacity(TConstArrayView<FVekaTransitionEffect> Effects, float Progress)
{
	float Opacity = 1.f;
	for (const FVekaTransitionEffect& Effect : Effects)
	{
		if (Effect.Kind == EVekaTransitionKind::Opacity || Effect.Kind == EVekaTransitionKind::Blur)
		{
			Opacity *= FMath::Clamp(Progress, 0.f, 1.f);
		}
	}
	return Opacity;
}

FSlateRenderTransform FVekaTransition::EvaluateTransform(TConstArrayView<FVekaTransitionEffect> Effects, float Progress, const FVector2D& Size)
{
	const float Away = 1.f - Progress;
	float ScaleValue = 1.f;
	FVector2D Translation = FVector2D::ZeroVector;
	for (const FVekaTransitionEffect& Effect : Effects)
	{
		switch (Effect.Kind)
		{
		case EVekaTransitionKind::Scale:
			ScaleValue *= FMath::Lerp(1.f, FMath::Max(Effect.Amount, 0.f), Away);
			break;
		case EVekaTransitionKind::Move:
			Translation += EdgeDirection(Effect.Edge) * Size * Away;
			break;
		case EVekaTransitionKind::Offset:
			Translation += Effect.Offset * Away;
			break;
		default:
			break;
		}
	}
	return Concatenate(FSlateRenderTransform(FScale2f(FMath::Max(ScaleValue, 1.0e-3f))), FSlateRenderTransform(FVector2f(Translation)));
}
