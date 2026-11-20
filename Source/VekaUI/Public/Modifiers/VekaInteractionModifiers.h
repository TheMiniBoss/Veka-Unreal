// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaModifier.h"
#include "VekaInteractionModifiers.generated.h"

/**
 * Gesture modifiers. In Phase 1 the action is a C++ callback (not serialized). Phase 2/3 add DSL actions
 * (assign, toggle, emit...) stored as data so assets and the Veka Editor can use them.
 */
USTRUCT(meta = (VekaName = "onTap", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaOnTapModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<void()> Action;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};

USTRUCT(meta = (VekaName = "onHover", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaOnHoverModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<void(bool)> Action;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};

USTRUCT(meta = (VekaName = "onPress", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaOnPressModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<void(bool)> Action;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};
