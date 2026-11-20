// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaInteractionModifiers.h"

void FVekaOnTapModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	if (Action)
	{
		Gestures.OnTap.Add(Action);
	}
}

void FVekaOnHoverModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	if (Action)
	{
		Gestures.OnHover.Add(Action);
	}
}

void FVekaOnPressModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	if (Action)
	{
		Gestures.OnPress.Add(Action);
	}
}

void FVekaOnSecondaryTapModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	if (Action)
	{
		Gestures.OnSecondaryTap.Add(Action);
	}
}

void FVekaOnDragModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	if (Action)
	{
		Gestures.OnDrag.Add(Action);
	}
}

void FVekaFocusableModifier::CollectGestures(FVekaGestureSet& Gestures) const
{
	Gestures.bFocusable = true;
	if (OnKey)
	{
		Gestures.OnKey.Add(OnKey);
	}
	if (OnFocus)
	{
		Gestures.OnFocus.Add(OnFocus);
	}
}

void FVekaDisabledModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<bool>(VekaEnv::Enabled(), VekaEnv::IsEnabled(Environment) && !bDisabled);
}

bool VekaEnv::IsEnabled(const FVekaEnvironment& Environment)
{
	return Environment.Get<bool>(Enabled(), true);
}

namespace Veka::Interaction
{
	namespace
	{
		const FEventSource* GCurrentSource = nullptr;
	}

	const FEventSource* GetCurrentEventSource()
	{
		return GCurrentSource;
	}

	FEventScope::FEventScope(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, const FVekaEnvironment& Environment, const FVector2D& ScreenPosition)
	{
		Source.Widget = Widget;
		Source.Geometry = Geometry;
		Source.Environment = Environment;
		Source.ScreenPosition = ScreenPosition;
		Previous = GCurrentSource;
		GCurrentSource = &Source;
	}

	FEventScope::~FEventScope()
	{
		GCurrentSource = Previous;
	}
}
