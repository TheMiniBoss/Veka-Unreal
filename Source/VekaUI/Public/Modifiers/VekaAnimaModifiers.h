// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaModifier.h"
#include "Anima/VekaAnimationSpec.h"
#include "Anima/VekaTransition.h"
#include "VekaAnimaModifiers.generated.h"

/**
 * `.Animation(spec, value)`: when Value changes, every animatable change of this node and its subtree
 * (sizes, colors, paints, offsets, opacity, trims, radii, font sizes, layout positions) animates with Spec,
 * and inserted / removed children play their transitions. Value is stored as a hash.
 */
USTRUCT(BlueprintType, meta = (VekaName = "animation", VekaCategory = "Anima"))
struct VEKAUI_API FVekaAnimationModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	FVekaAnimationSpec Spec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	int64 ValueKey = 0;
};

/** How this node enters / leaves when it is inserted or removed inside an animation transaction. */
USTRUCT(BlueprintType, meta = (VekaName = "transition", VekaCategory = "Anima"))
struct VEKAUI_API FVekaTransitionModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Anima")
	FVekaTransition Transition;
};
