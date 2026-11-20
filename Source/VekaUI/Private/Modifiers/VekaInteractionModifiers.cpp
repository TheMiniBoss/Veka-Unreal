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
