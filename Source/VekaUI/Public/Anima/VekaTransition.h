// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderTransform.h"
#include "VekaTransition.generated.h"

UENUM(BlueprintType)
enum class EVekaEdge : uint8
{
	Leading,
	Trailing,
	Top,
	Bottom
};

/** Alpha 13 transition kinds. */
UENUM(BlueprintType)
enum class EVekaTransitionKind : uint8
{
	Identity,
	Opacity,
	/** Scale from Amount (0 = from nothing). */
	Scale,
	/** Moves by the node size toward Edge. */
	Move,
	/** Moves by a fixed Offset. */
	Offset,
	/** Blur from Amount pixels. Approximated by opacity until the render module (Phase 4). */
	Blur
};

/** One effect of a transition, described at its "away" end (progress 0); progress 1 is identity. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTransitionEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	EVekaTransitionKind Kind = EVekaTransitionKind::Opacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	EVekaEdge Edge = EVekaEdge::Leading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	float Amount = 0.f;
};

/**
 * How a node enters and leaves the tree when the change happens in an animation transaction
 * (WithAnimation or `.Animation(spec, value)` above it). Effects combine. Default: Opacity.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	TArray<FVekaTransitionEffect> Insertion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	TArray<FVekaTransitionEffect> Removal;

	static FVekaTransition Identity();
	static FVekaTransition Opacity();
	static FVekaTransition Scale(float From = 0.f);
	/** Enters from the leading edge, leaves toward the trailing edge. */
	static FVekaTransition Slide();
	static FVekaTransition Move(EVekaEdge Edge);
	/** Enters from Edge, leaves toward the opposite edge, with a fade. */
	static FVekaTransition Push(EVekaEdge From);
	static FVekaTransition Offset(const FVector2D& Delta);
	static FVekaTransition Blur(float Radius = 10.f);
	/** Both transitions at once. */
	static FVekaTransition Combined(const FVekaTransition& A, const FVekaTransition& B);
	/** Insertion of one, removal of the other. */
	static FVekaTransition Asymmetric(const FVekaTransition& InInsertion, const FVekaTransition& InRemoval);

	/** Opacity factor of a set of effects at Progress (1 = identity). */
	static float EvaluateOpacity(TConstArrayView<FVekaTransitionEffect> Effects, float Progress);
	/** Render transform of a set of effects at Progress, for a node of Size (around its center). */
	static FSlateRenderTransform EvaluateTransform(TConstArrayView<FVekaTransitionEffect> Effects, float Progress, const FVector2D& Size);
};
