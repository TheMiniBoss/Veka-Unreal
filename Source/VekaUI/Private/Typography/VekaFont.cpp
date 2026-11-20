// Copyright TheMiniBoss. All Rights Reserved.

#include "Typography/VekaFont.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontFaceInterface.h"
#include "Engine/FontFace.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "VekaUILog.h"

namespace
{
	const TCHAR* WeightFileName(EVekaFontWeight Weight)
	{
		switch (Weight)
		{
		case EVekaFontWeight::Hairline: return TEXT("Hairline");
		case EVekaFontWeight::Thin: return TEXT("Thin");
		case EVekaFontWeight::ExtraLight: return TEXT("ExtraLight");
		case EVekaFontWeight::Light: return TEXT("Light");
		case EVekaFontWeight::News: return TEXT("News");
		case EVekaFontWeight::Regular: return TEXT("Regular");
		case EVekaFontWeight::Medium: return TEXT("Medium");
		case EVekaFontWeight::SemiBold: return TEXT("SemiBold");
		case EVekaFontWeight::Bold: return TEXT("Bold");
		case EVekaFontWeight::ExtraBold: return TEXT("ExtraBold");
		case EVekaFontWeight::Black: return TEXT("Black");
		case EVekaFontWeight::ExtraBlack:
		default: return TEXT("ExtraBlack");
		}
	}

	constexpr int32 NumWeights = static_cast<int32>(EVekaFontWeight::ExtraBlack) + 1;

	/** Engine font as fallback for characters the family does not have (CJK, symbols...). */
	void SetEngineFallback(FCompositeFont& Font)
	{
		const FSlateFontInfo EngineFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
		if (const FCompositeFont* Engine = EngineFont.GetCompositeFont())
		{
			Font.FallbackTypeface.Typeface = Engine->DefaultTypeface;
			Font.SubTypefaces = Engine->SubTypefaces;
		}
	}
}

namespace Veka::Typography
{
	FName TypefaceName(EVekaFontWeight Weight, bool bItalic)
	{
		return FName(*FString::Printf(TEXT("%s%s"), WeightFileName(Weight), bItalic ? TEXT("Italic") : TEXT("")));
	}

	TSharedPtr<const FCompositeFont> GetDefaultFamily()
	{
		static TSharedPtr<const FCompositeFont> Family = []() -> TSharedPtr<const FCompositeFont>
		{
			TSharedRef<FStandaloneCompositeFont> Font = MakeShared<FStandaloneCompositeFont>();
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VekaUI"));
			const FString Directory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Fonts/LTFlodeNeue")) : FString();
			bool bAny = false;
			for (int32 Index = 0; Index < NumWeights; ++Index)
			{
				const EVekaFontWeight Weight = static_cast<EVekaFontWeight>(Index);
				for (const bool bItalic : { false, true })
				{
					// Files are LTFlodeNeue-Bold.otf / LTFlodeNeue-BoldItalic.otf; Regular italic is "Italic".
					const FString Style = Weight == EVekaFontWeight::Regular && bItalic ? FString(TEXT("Italic")) : FString(WeightFileName(Weight)) + (bItalic ? TEXT("Italic") : TEXT(""));
					const FString File = FPaths::Combine(Directory, FString::Printf(TEXT("LTFlodeNeue-%s.otf"), *Style));
					if (!Directory.IsEmpty() && FPaths::FileExists(File))
					{
						Font->DefaultTypeface.AppendFont(TypefaceName(Weight, bItalic), File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
						bAny = true;
					}
				}
			}
			if (!bAny)
			{
				UE_LOG(LogVekaUI, Warning, TEXT("VekaUI: LT Flode Neue not found in %s; using the engine font."), *Directory);
				const FSlateFontInfo EngineFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
				if (const FCompositeFont* Engine = EngineFont.GetCompositeFont())
				{
					Font->DefaultTypeface = Engine->DefaultTypeface;
				}
			}
			SetEngineFallback(*Font);
			return Font;
		}();
		return Family;
	}

	FSlateFontInfo MakeFont(const UVekaFontFamily* Family, float Size, EVekaFontWeight Weight, bool bItalic, float TrackingEm)
	{
		TSharedPtr<const FCompositeFont> Composite = Family ? Family->GetCompositeFont() : nullptr;
		if (!Composite.IsValid())
		{
			Composite = GetDefaultFamily();
		}

		// Pick an existing typeface (closest weight, then upright).
		FName Typeface = TypefaceName(Weight, bItalic);
		auto Has = [&Composite](FName Name)
		{
			return Composite->DefaultTypeface.Fonts.ContainsByPredicate([Name](const FTypefaceEntry& Entry) { return Entry.Name == Name; });
		};
		if (!Has(Typeface))
		{
			for (int32 Distance = 1; Distance < NumWeights && !Has(Typeface); ++Distance)
			{
				for (int32 Sign : { 1, -1 })
				{
					const int32 Index = static_cast<int32>(Weight) + Sign * Distance;
					if (Index >= 0 && Index < NumWeights && Has(TypefaceName(static_cast<EVekaFontWeight>(Index), bItalic)))
					{
						Typeface = TypefaceName(static_cast<EVekaFontWeight>(Index), bItalic);
						break;
					}
				}
			}
			if (!Has(Typeface) && bItalic)
			{
				return MakeFont(Family, Size, Weight, false, TrackingEm);
			}
		}

		FSlateFontInfo Font(Composite, Size, Typeface);
		Font.LetterSpacing = FMath::RoundToInt32(TrackingEm * 1000.f);
		return Font;
	}
}

TSharedPtr<const FCompositeFont> UVekaFontFamily::GetCompositeFont() const
{
	if (Cached.IsValid())
	{
		return Cached;
	}
	TSharedRef<FStandaloneCompositeFont> Font = MakeShared<FStandaloneCompositeFont>();
	for (int32 Index = 0; Index < NumWeights; ++Index)
	{
		const EVekaFontWeight Weight = static_cast<EVekaFontWeight>(Index);
		for (const bool bItalic : { false, true })
		{
			const TMap<EVekaFontWeight, TObjectPtr<UFontFace>>& Map = bItalic ? ItalicFaces : Faces;
			const TObjectPtr<UFontFace>* Face = Map.Find(Weight);
			if (Face && *Face)
			{
				FTypefaceEntry& Entry = Font->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(Veka::Typography::TypefaceName(Weight, bItalic)));
				Entry.Font = FFontData(Face->Get());
			}
		}
	}
	if (Font->DefaultTypeface.Fonts.IsEmpty())
	{
		return nullptr;
	}
	SetEngineFallback(*Font);
	Cached = Font;
	return Cached;
}

#if WITH_EDITOR
void UVekaFontFamily::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Cached.Reset();
}
#endif
