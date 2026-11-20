// Copyright TheMiniBoss. All Rights Reserved.

#include "Typography/VekaTypography.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"

namespace
{
	FVekaTextStyleSpec Spec(float Size, EVekaFontWeight Weight, float LineHeight)
	{
		FVekaTextStyleSpec Out;
		Out.Size = Size;
		Out.Weight = Weight;
		Out.LineHeight = LineHeight;
		return Out;
	}
}

bool FVekaTypography::operator==(const FVekaTypography& Other) const
{
	return Style == Other.Style && Family == Other.Family && bOverrideWeight == Other.bOverrideWeight && Weight == Other.Weight
		&& bItalic == Other.bItalic && Size == Other.Size && bOverrideTracking == Other.bOverrideTracking && Tracking == Other.Tracking
		&& LineSpacing == Other.LineSpacing && Case == Other.Case && bMonospacedDigits == Other.bMonospacedDigits && LineLimit == Other.LineLimit
		&& Truncation == Other.Truncation && Alignment == Other.Alignment && MinimumScaleFactor == Other.MinimumScaleFactor
		&& ContentTransition == Other.ContentTransition && ContentTransitionDuration == Other.ContentTransitionDuration && Effect == Other.Effect
		&& Scale == Other.Scale && bCustomFont == Other.bCustomFont && (!bCustomFont || CustomFont == Other.CustomFont);
}

void FVekaTypographyModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	FVekaTypography T = VekaEnv::GetTypography(Environment);
	const FVekaTypography& V = Value;
	switch (Field)
	{
	case EVekaTypographyField::Style:
		// A style replaces the font (size, explicit Slate font) but, like SwiftUI, weight and italic are
		// separate values applied on top of it, whatever the order of the modifiers.
		T.Style = V.Style;
		T.Size = 0.f;
		T.bCustomFont = false;
		break;
	case EVekaTypographyField::Family: T.Family = V.Family; T.bCustomFont = false; break;
	case EVekaTypographyField::Weight: T.bOverrideWeight = true; T.Weight = V.Weight; break;
	case EVekaTypographyField::Italic: T.bItalic = V.bItalic; break;
	case EVekaTypographyField::Size: T.Size = V.Size; break;
	case EVekaTypographyField::Tracking: T.bOverrideTracking = true; T.Tracking = V.Tracking; break;
	case EVekaTypographyField::LineSpacing: T.LineSpacing = V.LineSpacing; break;
	case EVekaTypographyField::Case: T.Case = V.Case; break;
	case EVekaTypographyField::MonospacedDigits: T.bMonospacedDigits = V.bMonospacedDigits; break;
	case EVekaTypographyField::LineLimit: T.LineLimit = V.LineLimit; break;
	case EVekaTypographyField::Truncation: T.Truncation = V.Truncation; break;
	case EVekaTypographyField::Alignment: T.Alignment = V.Alignment; break;
	case EVekaTypographyField::MinimumScaleFactor: T.MinimumScaleFactor = V.MinimumScaleFactor; break;
	case EVekaTypographyField::ContentTransition: T.ContentTransition = V.ContentTransition; T.ContentTransitionDuration = V.ContentTransitionDuration; break;
	case EVekaTypographyField::Effect: T.Effect = V.Effect; break;
	case EVekaTypographyField::Scale: T.Scale = V.Scale; break;
	case EVekaTypographyField::CustomFont: T.bCustomFont = true; T.CustomFont = V.CustomFont; break;
	}
	Environment.Set<FVekaTypography>(VekaEnv::Typography(), T);
}

namespace VekaEnv
{
	const FVekaTypography& GetTypography(const FVekaEnvironment& Environment)
	{
		static const FVekaTypography Default;
		const FVekaTypography* Found = Environment.Find<FVekaTypography>(Typography());
		return Found ? *Found : Default;
	}
}

namespace Veka::Typography
{
	FVekaTextStyleSpec GetStyleSpec(EVekaTextStyle Style, const UVekaTheme* Theme)
	{
		if (Theme)
		{
			if (const FVekaTextStyleSpec* Found = Theme->FindTextStyle(Style))
			{
				return *Found;
			}
		}
		// Slate point sizes (1 pt = 4/3 px at 100 % DPI).
		switch (Style)
		{
		case EVekaTextStyle::LargeTitle: return Spec(30.f, EVekaFontWeight::Bold, 1.f);
		case EVekaTextStyle::Title1: return Spec(24.f, EVekaFontWeight::Bold, 1.f);
		case EVekaTextStyle::Title2: return Spec(20.f, EVekaFontWeight::SemiBold, 1.f);
		case EVekaTextStyle::Title3: return Spec(17.f, EVekaFontWeight::SemiBold, 1.05f);
		case EVekaTextStyle::Headline: return Spec(14.f, EVekaFontWeight::SemiBold, 1.05f);
		case EVekaTextStyle::Body: return Spec(14.f, EVekaFontWeight::Regular, 1.1f);
		case EVekaTextStyle::Callout: return Spec(13.f, EVekaFontWeight::Regular, 1.1f);
		case EVekaTextStyle::Subheadline: return Spec(12.5f, EVekaFontWeight::Regular, 1.1f);
		case EVekaTextStyle::Footnote: return Spec(11.5f, EVekaFontWeight::Regular, 1.1f);
		case EVekaTextStyle::Caption1: return Spec(11.f, EVekaFontWeight::Regular, 1.1f);
		case EVekaTextStyle::Caption2:
		default: return Spec(10.f, EVekaFontWeight::Medium, 1.1f);
		}
	}

	float OpticalTracking(float Size)
	{
		// +1 % at 10 pt, 0 from 16 pt, about -1.2 % at 30 pt.
		const float Tracking = Size <= 16.f ? 0.01f * (16.f - Size) / 6.f : -0.012f * (Size - 16.f) / 14.f;
		return FMath::Clamp(Tracking, -0.015f, 0.02f);
	}

	FResolvedText Resolve(const FVekaEnvironment& Environment)
	{
		const FVekaTypography& T = VekaEnv::GetTypography(Environment);
		FResolvedText Out;
		Out.Case = T.Case;
		Out.bMonospacedDigits = T.bMonospacedDigits;
		Out.LineLimit = T.LineLimit;
		Out.Truncation = T.Truncation;
		Out.Alignment = T.Alignment;
		Out.MinimumScaleFactor = FMath::Clamp(T.MinimumScaleFactor, 0.1f, 1.f);
		Out.ContentTransition = T.ContentTransition;
		Out.ContentTransitionDuration = T.ContentTransitionDuration;
		Out.Effect = T.Effect;

		if (T.bCustomFont)
		{
			Out.Font = T.CustomFont;
			Out.Font.Size *= T.Scale;
			if (T.bOverrideTracking)
			{
				Out.Font.LetterSpacing = FMath::RoundToInt32(T.Tracking * 1000.f);
			}
			Out.LineHeight = T.LineSpacing > 0.f ? T.LineSpacing : 1.f;
			return Out;
		}

		const UVekaTheme* Theme = VekaEnv::GetTheme(Environment);
		const FVekaTextStyleSpec StyleSpec = GetStyleSpec(T.Style, Theme);
		const float Size = (T.Size > 0.f ? T.Size : StyleSpec.Size) * FMath::Max(T.Scale, 0.1f);
		const EVekaFontWeight Weight = T.bOverrideWeight ? T.Weight : StyleSpec.Weight;
		const float Tracking = T.bOverrideTracking ? T.Tracking : OpticalTracking(Size);
		const UVekaFontFamily* Family = T.Family ? T.Family.Get() : (Theme ? Theme->FontFamily.Get() : nullptr);
		Out.Font = MakeFont(Family, Size, Weight, T.bItalic, Tracking);
		Out.LineHeight = T.LineSpacing > 0.f ? T.LineSpacing : StyleSpec.LineHeight;
		return Out;
	}

	FSlateFontInfo StyleFont(EVekaTextStyle Style, const FVekaEnvironment* Environment)
	{
		const UVekaTheme* Theme = Environment ? VekaEnv::GetTheme(*Environment) : nullptr;
		const FVekaTypography* Typography = Environment ? &VekaEnv::GetTypography(*Environment) : nullptr;
		const FVekaTextStyleSpec StyleSpec = GetStyleSpec(Style, Theme);
		const float Size = StyleSpec.Size * (Typography ? Typography->Scale : 1.f);
		const UVekaFontFamily* Family = Typography && Typography->Family ? Typography->Family.Get() : (Theme ? Theme->FontFamily.Get() : nullptr);
		return MakeFont(Family, Size, StyleSpec.Weight, false, OpticalTracking(Size));
	}
}
