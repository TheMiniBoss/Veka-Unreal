// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEnvironmentModifiers.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"

void FVekaForegroundModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	// Outer modifiers ran first: the theme and color scheme of this point of the tree are known.
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::Resolve(Paint, Environment).GetAverageColor());
}

void FVekaFontModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<FSlateFontInfo>(VekaEnv::Font(), Font);
}

void FVekaColorSchemeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Scheme);
}
