// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaColoraModifiers.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"

void FVekaThemeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<const UVekaTheme*>(VekaEnv::Theme(), Theme.Get());
}

void FVekaColorAdjustModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	// Environment modifiers run outermost first, so the existing matrix is the outer one: apply ours first.
	const FVekaColorMatrix Mine = FVekaColorMatrix::Make(Adjust, Amount);
	const FVekaColorMatrix* Outer = VekaEnv::GetColorMatrix(Environment);
	Environment.Set<FVekaColorMatrix>(VekaEnv::ColorMatrix(), Outer ? Mine.Then(*Outer) : Mine);
}

void FVekaBlendModeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<EVekaBlendMode>(VekaEnv::BlendMode(), Mode);
}
