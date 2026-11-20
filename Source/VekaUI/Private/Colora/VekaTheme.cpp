// Copyright TheMiniBoss. All Rights Reserved.

#include "Colora/VekaTheme.h"
#include "Core/VekaEnvironment.h"
#include "VekaUILog.h"

namespace
{
	struct FDefaultToken
	{
		const TCHAR* Name;
		uint32 Light;      // 0xRRGGBB, sRGB
		float LightAlpha;
		uint32 Dark;
		float DarkAlpha;
	};

	// Alpha 13 Colora preset names with system palette values (light / dark). The exact Alpha 13 values live in
	// the Colora_System asset of the Blueprint branch; a theme asset can override any of them.
	const FDefaultToken GDefaultTokens[] = {
		{ TEXT("Accent"),           0x007AFF, 1.f,  0x0A84FF, 1.f },
		{ TEXT("Black"),            0x000000, 1.f,  0x000000, 1.f },
		{ TEXT("Blue"),             0x007AFF, 1.f,  0x0A84FF, 1.f },
		{ TEXT("Brown"),            0xA2845E, 1.f,  0xAC8E68, 1.f },
		{ TEXT("Clear"),            0x000000, 0.f,  0x000000, 0.f },
		{ TEXT("Cyan"),             0x32ADE6, 1.f,  0x64D2FF, 1.f },
		{ TEXT("Gray"),             0x8E8E93, 1.f,  0x8E8E93, 1.f },
		{ TEXT("Green"),            0x34C759, 1.f,  0x30D158, 1.f },
		{ TEXT("Indigo"),           0x5856D6, 1.f,  0x5E5CE6, 1.f },
		{ TEXT("Mint"),             0x00C7BE, 1.f,  0x63E6E2, 1.f },
		{ TEXT("Orange"),           0xFF9500, 1.f,  0xFF9F0A, 1.f },
		{ TEXT("Pink"),             0xFF2D55, 1.f,  0xFF375F, 1.f },
		{ TEXT("Purple"),           0xAF52DE, 1.f,  0xBF5AF2, 1.f },
		{ TEXT("Red"),              0xFF3B30, 1.f,  0xFF453A, 1.f },
		{ TEXT("Teal"),             0x30B0C7, 1.f,  0x40C8E0, 1.f },
		{ TEXT("White"),            0xFFFFFF, 1.f,  0xFFFFFF, 1.f },
		{ TEXT("Yellow"),           0xFFCC00, 1.f,  0xFFD60A, 1.f },
		{ TEXT("Tint"),             0x007AFF, 1.f,  0x0A84FF, 1.f },
		{ TEXT("Background"),       0xFFFFFF, 1.f,  0x000000, 1.f },
		{ TEXT("WindowBackground"), 0xF2F2F7, 1.f,  0x1C1C1E, 1.f },
		{ TEXT("Foreground"),       0x000000, 1.f,  0xFFFFFF, 1.f },
		{ TEXT("Separator"),        0x3C3C43, 0.29f, 0x545458, 0.6f },
		{ TEXT("Selection"),        0x007AFF, 0.25f, 0x0A84FF, 0.35f },
		{ TEXT("Placeholder"),      0x3C3C43, 0.3f, 0xEBEBF5, 0.3f },
		{ TEXT("Link"),             0x007AFF, 1.f,  0x0984FF, 1.f },
		{ TEXT("Fill"),             0x787880, 0.2f, 0x787880, 0.36f },
	};

	FLinearColor FromHex(uint32 Rgb, float Alpha)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor((Rgb >> 16) & 0xFF, (Rgb >> 8) & 0xFF, Rgb & 0xFF, 255));
		Color.A = Alpha;
		return Color;
	}

	const TMap<FName, const FDefaultToken*>& GetDefaultMap()
	{
		static const TMap<FName, const FDefaultToken*> Map = []()
		{
			TMap<FName, const FDefaultToken*> Result;
			for (const FDefaultToken& Token : GDefaultTokens)
			{
				Result.Add(FName(Token.Name), &Token);
			}
			return Result;
		}();
		return Map;
	}

	constexpr int32 MaxTokenDepth = 4;
	const FLinearColor UnknownTokenColor(1.f, 0.f, 1.f, 1.f);

	void WarnUnknownToken(FName Token)
	{
		static TSet<FName> Warned;
		if (!Warned.Contains(Token))
		{
			Warned.Add(Token);
			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI: unknown Colora token '%s' (shown in magenta)."), *Token.ToString());
		}
	}

	FVekaPaint ResolveImpl(const FVekaPaint& Paint, const UVekaTheme* Theme, EVekaColorScheme Scheme, int32 Depth);

	/** Paint of a token, without hierarchy. */
	FVekaPaint LookupToken(FName Token, const UVekaTheme* Theme, EVekaColorScheme Scheme, int32 Depth)
	{
		if (Depth > MaxTokenDepth)
		{
			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI: Colora token '%s' refers to itself (cycle)."), *Token.ToString());
			return FVekaPaint(UnknownTokenColor);
		}
		if (Theme)
		{
			if (const FVekaThemeColor* Found = Theme->FindColor(Token))
			{
				return ResolveImpl(Scheme == EVekaColorScheme::Dark ? Found->Dark : Found->Light, Theme, Scheme, Depth + 1);
			}
		}
		FLinearColor Color;
		if (Veka::Colora::GetDefaultColor(Token, Scheme, Color))
		{
			return FVekaPaint(Color);
		}
		WarnUnknownToken(Token);
		return FVekaPaint(UnknownTokenColor);
	}

	FLinearColor TokenColor(FName Token, EVekaHierarchy Hierarchy, const UVekaTheme* Theme, EVekaColorScheme Scheme, int32 Depth)
	{
		FLinearColor Color = LookupToken(Token, Theme, Scheme, Depth).GetAverageColor();
		Color.A *= Veka::Colora::HierarchyAlpha(Hierarchy);
		return Color;
	}

	FVekaPaint ResolveImpl(const FVekaPaint& Paint, const UVekaTheme* Theme, EVekaColorScheme Scheme, int32 Depth)
	{
		if (Paint.Type == EVekaPaintType::Token)
		{
			FVekaPaint Resolved = LookupToken(Paint.Token, Theme, Scheme, Depth);
			const float Alpha = Veka::Colora::HierarchyAlpha(Paint.Hierarchy);
			if (Alpha < 1.f)
			{
				Resolved.Color.A *= Alpha;
				for (FVekaGradientStop& Stop : Resolved.Stops)
				{
					Stop.Color.A *= Alpha;
				}
				for (FVekaMeshPoint& Point : Resolved.Mesh.Points)
				{
					Point.Color.A *= Alpha;
				}
			}
			return Resolved;
		}

		bool bHasTokens = false;
		for (const FVekaGradientStop& Stop : Paint.Stops)
		{
			bHasTokens |= !Stop.Token.IsNone();
		}
		for (const FVekaMeshPoint& Point : Paint.Mesh.Points)
		{
			bHasTokens |= !Point.Token.IsNone();
		}
		if (!bHasTokens)
		{
			return Paint;
		}

		FVekaPaint Resolved = Paint;
		for (FVekaGradientStop& Stop : Resolved.Stops)
		{
			if (!Stop.Token.IsNone())
			{
				Stop.Color = TokenColor(Stop.Token, Stop.Hierarchy, Theme, Scheme, Depth + 1);
				Stop.Token = NAME_None;
			}
		}
		for (FVekaMeshPoint& Point : Resolved.Mesh.Points)
		{
			if (!Point.Token.IsNone())
			{
				Point.Color = TokenColor(Point.Token, EVekaHierarchy::Primary, Theme, Scheme, Depth + 1);
				Point.Token = NAME_None;
			}
		}
		return Resolved;
	}
}

const FVekaThemeColor* UVekaTheme::FindColor(FName Token) const
{
	int32 Guard = 0;
	for (const UVekaTheme* Theme = this; Theme && Guard < 16; Theme = Theme->Parent, ++Guard)
	{
		if (const FVekaThemeColor* Found = Theme->Colors.Find(Token))
		{
			return Found;
		}
	}
	return nullptr;
}

const float* UVekaTheme::FindMetric(FName Name) const
{
	int32 Guard = 0;
	for (const UVekaTheme* Theme = this; Theme && Guard < 16; Theme = Theme->Parent, ++Guard)
	{
		if (const float* Found = Theme->Metrics.Find(Name))
		{
			return Found;
		}
	}
	return nullptr;
}

const FVekaTextStyleSpec* UVekaTheme::FindTextStyle(EVekaTextStyle Style) const
{
	int32 Guard = 0;
	for (const UVekaTheme* Theme = this; Theme && Guard < 16; Theme = Theme->Parent, ++Guard)
	{
		if (const FVekaTextStyleSpec* Found = Theme->TextStyles.Find(Style))
		{
			return Found;
		}
	}
	return nullptr;
}

namespace VekaEnv
{
	const UVekaTheme* GetTheme(const FVekaEnvironment& Environment)
	{
		return Environment.Get<const UVekaTheme*>(Theme(), nullptr);
	}

	EVekaBlendMode GetBlendMode(const FVekaEnvironment& Environment)
	{
		return Environment.Get<EVekaBlendMode>(BlendMode(), EVekaBlendMode::Normal);
	}

	const FVekaColorMatrix* GetColorMatrix(const FVekaEnvironment& Environment)
	{
		return Environment.Find<FVekaColorMatrix>(ColorMatrix());
	}
}

namespace Veka::Colora
{
	TConstArrayView<FName> DefaultTokenNames()
	{
		static const TArray<FName> Names = []()
		{
			TArray<FName> Result;
			for (const FDefaultToken& Token : GDefaultTokens)
			{
				Result.Add(FName(Token.Name));
			}
			return Result;
		}();
		return Names;
	}

	bool GetDefaultColor(FName Token, EVekaColorScheme Scheme, FLinearColor& OutColor)
	{
		if (const FDefaultToken* const* Found = GetDefaultMap().Find(Token))
		{
			const FDefaultToken& Entry = **Found;
			OutColor = Scheme == EVekaColorScheme::Dark ? FromHex(Entry.Dark, Entry.DarkAlpha) : FromHex(Entry.Light, Entry.LightAlpha);
			return true;
		}
		return false;
	}

	float HierarchyAlpha(EVekaHierarchy Hierarchy)
	{
		switch (Hierarchy)
		{
		case EVekaHierarchy::Primary: return 1.f;
		case EVekaHierarchy::Secondary: return 0.6f;
		case EVekaHierarchy::Tertiary: return 0.3f;
		case EVekaHierarchy::Quaternary: return 0.18f;
		case EVekaHierarchy::Transparent:
		default: return 0.f;
		}
	}

	FVekaPaint Resolve(const FVekaPaint& Paint, const FVekaEnvironment& Environment)
	{
		return ResolveImpl(Paint, VekaEnv::GetTheme(Environment), VekaEnv::GetColorScheme(Environment), 0);
	}

	FLinearColor ResolveColor(FName Token, const FVekaEnvironment& Environment, EVekaHierarchy Hierarchy)
	{
		return TokenColor(Token, Hierarchy, VekaEnv::GetTheme(Environment), VekaEnv::GetColorScheme(Environment), 0);
	}

	float Metric(FName Name, const FVekaEnvironment& Environment, float Default)
	{
		if (const UVekaTheme* Theme = VekaEnv::GetTheme(Environment))
		{
			if (const float* Found = Theme->FindMetric(Name))
			{
				return *Found;
			}
		}
		return Default;
	}
}
