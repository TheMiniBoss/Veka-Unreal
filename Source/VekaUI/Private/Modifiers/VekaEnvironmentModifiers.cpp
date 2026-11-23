// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEnvironmentModifiers.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Typography/VekaTypography.h"

void FVekaForegroundModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	// Outer modifiers ran first: the theme and color scheme of this point of the tree are known.
	const FVekaPaint Resolved = Veka::Colora::Resolve(Paint, Environment);
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Resolved.GetAverageColor());
	Environment.Set<FVekaPaint>(VekaEnv::ForegroundPaint(), Resolved);
}

void FVekaFontModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	FVekaTypographyModifier Typography;
	Typography.Field = EVekaTypographyField::CustomFont;
	Typography.Value.CustomFont = Font;
	Typography.ModifyEnvironment(Environment);
}

void FVekaColorSchemeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Scheme);
}
