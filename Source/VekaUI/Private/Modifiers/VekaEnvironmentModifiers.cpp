// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEnvironmentModifiers.h"
#include "Core/VekaEnvironment.h"

void FVekaForegroundModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Color);
}

void FVekaFontModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<FSlateFontInfo>(VekaEnv::Font(), Font);
}

void FVekaColorSchemeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Scheme);
}
