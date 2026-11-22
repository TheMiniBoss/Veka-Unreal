// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaModifier.h"
#include "Core/VekaEnvironment.h"
#include "Layout/Geometry.h"
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

USTRUCT(meta = (VekaName = "onSecondaryTap", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaOnSecondaryTapModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<void(const FVector2D&)> Action;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};

USTRUCT(meta = (VekaName = "onDrag", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaOnDragModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<void(const FVekaDragValue&)> Action;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};

/** Makes the view take keyboard focus (Tab, click) and receive keys. */
USTRUCT(meta = (VekaName = "focusable", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaFocusableModifier : public FVekaModifier
{
	GENERATED_BODY()

	TFunction<bool(const FKeyEvent&)> OnKey;
	TFunction<void(bool)> OnFocus;

	virtual void CollectGestures(FVekaGestureSet& Gestures) const override;
};

/** Disables the controls of the subtree (no input; controls draw themselves dimmed). */
USTRUCT(BlueprintType, meta = (VekaName = "disabled", VekaCategory = "Interaction"))
struct VEKAUI_API FVekaDisabledModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Interaction")
	bool bDisabled = true;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
	virtual bool ModifiesEnvironment() const override { return true; }
};

namespace VekaEnv
{
	/** bool: false inside .Disabled(true). */
	inline FName Enabled() { static const FName Name(TEXT("Veka.Enabled")); return Name; }

	VEKAUI_API bool IsEnabled(const FVekaEnvironment& Environment);
}

namespace Veka::Interaction
{
	/** The widget, geometry and environment of the input event being handled (popups anchor to it). */
	struct FEventSource
	{
		TWeakPtr<SWidget> Widget;
		FGeometry Geometry;
		FVekaEnvironment Environment;
		FVector2D ScreenPosition = FVector2D::ZeroVector;
	};

	/** Set while gesture callbacks run (nullptr otherwise). */
	VEKAUI_API const FEventSource* GetCurrentEventSource();

	struct VEKAUI_API FEventScope
	{
		FEventScope(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, const FVekaEnvironment& Environment, const FVector2D& ScreenPosition);
		~FEventScope();

	private:
		FEventSource Source;
		const FEventSource* Previous = nullptr;
	};
}
