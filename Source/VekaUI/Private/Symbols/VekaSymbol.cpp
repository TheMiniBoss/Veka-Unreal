// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/VekaSymbol.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace Veka::Symbols::Private
{
	void BuildBuiltInLibrary(TArray<FVekaSymbolDefinition>& Out);
}

FVekaSymbolVariant FVekaSymbolVariant::Merged(const FVekaSymbolVariant& Other) const
{
	FVekaSymbolVariant Out = *this;
	Out.bFill |= Other.bFill;
	Out.bSlash |= Other.bSlash;
	if (Other.Enclosure != EVekaSymbolEnclosure::None)
	{
		Out.Enclosure = Other.Enclosure;
	}
	return Out;
}

namespace Veka::Symbols
{
	namespace
	{
		struct FBuiltIn
		{
			TArray<FVekaSymbolDefinition> Definitions;
			TMap<FName, int32> ByName;

			FBuiltIn()
			{
				Private::BuildBuiltInLibrary(Definitions);
				for (int32 Index = 0; Index < Definitions.Num(); ++Index)
				{
					ensureMsgf(!ByName.Contains(Definitions[Index].Name), TEXT("Duplicate Veka symbol %s"), *Definitions[Index].Name.ToString());
					ByName.Add(Definitions[Index].Name, Index);
				}
			}
		};

		const FBuiltIn& GetBuiltIn()
		{
			static const FBuiltIn BuiltIn;
			return BuiltIn;
		}

		TArray<TWeakObjectPtr<UVekaSymbolLibrary>>& GetLibraries()
		{
			static TArray<TWeakObjectPtr<UVekaSymbolLibrary>> Libraries;
			return Libraries;
		}

		struct FGeometryCache
		{
			TMap<const FVekaSymbolDefinition*, TArray<TArray<FVekaPolyline>>> Shapes;
		};

		FGeometryCache& GetGeometryCache()
		{
			static FGeometryCache Cache;
			return Cache;
		}

		const FVekaSymbolDefinition* FindExact(FName Name)
		{
			TArray<TWeakObjectPtr<UVekaSymbolLibrary>>& Libraries = GetLibraries();
			for (int32 Index = Libraries.Num() - 1; Index >= 0; --Index)
			{
				if (const UVekaSymbolLibrary* Library = Libraries[Index].Get())
				{
					for (const UVekaSymbol* Symbol : Library->Symbols)
					{
						if (Symbol && Symbol->Definition.Name == Name)
						{
							return &Symbol->Definition;
						}
					}
				}
			}
			const FBuiltIn& BuiltIn = GetBuiltIn();
			if (const int32* Found = BuiltIn.ByName.Find(Name))
			{
				return &BuiltIn.Definitions[*Found];
			}
			return nullptr;
		}

		double Clamp01(double Value) { return FMath::Clamp(Value, 0.0, 1.0); }
		double EaseInOut(double T) { T = Clamp01(T); return T * T * (3.0 - 2.0 * T); }
		double EaseOut(double T) { T = Clamp01(T); return 1.0 - (1.0 - T) * (1.0 - T); }
		double EaseOutBack(double T)
		{
			T = Clamp01(T);
			const double C1 = 1.70158;
			const double C3 = C1 + 1.0;
			return 1.0 + C3 * FMath::Pow(T - 1.0, 3.0) + C1 * FMath::Pow(T - 1.0, 2.0);
		}

		/** Seconds of one run (at speed 1). */
		double Duration(EVekaSymbolEffectKind Kind, int32 VariableSteps)
		{
			switch (Kind)
			{
			case EVekaSymbolEffectKind::Appear: return 0.4;
			case EVekaSymbolEffectKind::Disappear: return 0.3;
			case EVekaSymbolEffectKind::Bounce: return 0.5;
			case EVekaSymbolEffectKind::Pulse: return 1.0;
			case EVekaSymbolEffectKind::Scale: return 0.5;
			case EVekaSymbolEffectKind::VariableColor: return 0.3 * (FMath::Max(VariableSteps, 1) + 1);
			case EVekaSymbolEffectKind::Wiggle: return 0.6;
			case EVekaSymbolEffectKind::Rotate: return 0.8;
			case EVekaSymbolEffectKind::Breathe: return 2.0;
			case EVekaSymbolEffectKind::DrawOn: return 0.7;
			default: return 0.0;
			}
		}

		constexpr double LayerDelay = 0.08;
	}

	const FVekaSymbolDefinition* Find(FName Name)
	{
		if (const FVekaSymbolDefinition* Found = FindExact(Name))
		{
			return Found;
		}
		FVekaSymbolVariant Ignored;
		return GetAlpha13Aliases().Contains(Name) ? Resolve(Name, Ignored) : nullptr;
	}

	const FVekaSymbolDefinition* Resolve(FName FullName, FVekaSymbolVariant& OutVariant)
	{
		OutVariant = FVekaSymbolVariant();
		if (FullName.IsNone())
		{
			return nullptr;
		}
		FString Candidate = FullName.ToString();
		if (const FName* Alias = GetAlpha13Aliases().Find(FullName))
		{
			Candidate = Alias->ToString();
		}
		while (!Candidate.IsEmpty())
		{
			if (const FVekaSymbolDefinition* Found = FindExact(FName(*Candidate)))
			{
				return Found;
			}
			int32 Dot = INDEX_NONE;
			if (!Candidate.FindLastChar(TEXT('.'), Dot))
			{
				return nullptr;
			}
			const FString Suffix = Candidate.Mid(Dot + 1);
			if (Suffix == TEXT("fill"))
			{
				OutVariant.bFill = true;
			}
			else if (Suffix == TEXT("slash"))
			{
				OutVariant.bSlash = true;
			}
			else if (Suffix == TEXT("circle"))
			{
				OutVariant.Enclosure = EVekaSymbolEnclosure::Circle;
			}
			else if (Suffix == TEXT("square"))
			{
				OutVariant.Enclosure = EVekaSymbolEnclosure::Square;
			}
			else
			{
				return nullptr;
			}
			Candidate.LeftInline(Dot);
		}
		return nullptr;
	}

	void RegisterLibrary(UVekaSymbolLibrary* Library)
	{
		if (Library)
		{
			GetLibraries().AddUnique(Library);
		}
	}

	void UnregisterLibrary(UVekaSymbolLibrary* Library)
	{
		GetLibraries().Remove(Library);
		if (Library)
		{
			for (const UVekaSymbol* Symbol : Library->Symbols)
			{
				if (Symbol)
				{
					GetGeometryCache().Shapes.Remove(&Symbol->Definition);
				}
			}
		}
	}

	TArray<FName> GetBuiltInNames()
	{
		TArray<FName> Names;
		for (const FVekaSymbolDefinition& Definition : GetBuiltIn().Definitions)
		{
			Names.Add(Definition.Name);
		}
		return Names;
	}

	const TMap<FName, FName>& GetAlpha13Aliases()
	{
		static const TMap<FName, FName> Aliases = {
			{ TEXT("Info"), TEXT("info.circle") },
			{ TEXT("Warning"), TEXT("exclamationmark.triangle") },
			{ TEXT("Check"), TEXT("checkmark.circle") },
			{ TEXT("Exclamation"), TEXT("exclamationmark.circle") },
			{ TEXT("Interrogation"), TEXT("questionmark.circle") },
			{ TEXT("Clock"), TEXT("clock") },
			{ TEXT("Arrows"), TEXT("arrow.left.right") },
			{ TEXT("ArrowUpDown"), TEXT("chevron.up.down") },
			{ TEXT("Thunder"), TEXT("bolt") },
			{ TEXT("Triangle"), TEXT("triangle") },
			{ TEXT("Eraser"), TEXT("eraser") },
			{ TEXT("Loading"), TEXT("loading") },
			{ TEXT("Logo"), TEXT("veka.logo") },
			{ TEXT("Placeholder"), TEXT("placeholder") },
			{ TEXT("Test"), TEXT("testtube") },
			{ TEXT("KeylineGrid"), TEXT("keylinegrid") },
		};
		return Aliases;
	}

	const TArray<FVekaPolyline>& GetShapeGeometry(const FVekaSymbolDefinition& Definition, int32 ShapeIndex)
	{
		static const TArray<FVekaPolyline> Empty;
		if (!Definition.Shapes.IsValidIndex(ShapeIndex))
		{
			return Empty;
		}
		check(IsInGameThread());
		TArray<TArray<FVekaPolyline>>& Shapes = GetGeometryCache().Shapes.FindOrAdd(&Definition);
		if (Shapes.Num() != Definition.Shapes.Num())
		{
			Shapes.Reset();
			Shapes.SetNum(Definition.Shapes.Num());
			for (int32 Index = 0; Index < Definition.Shapes.Num(); ++Index)
			{
				// 0.12 grid units: a tenth of a pixel at body size, far fewer vertices than a finer flattening.
				Definition.Shapes[Index].Path.Flatten(Shapes[Index], 0.12);
			}
		}
		return Shapes[ShapeIndex];
	}

	float WeightFromFont(EVekaFontWeight Weight)
	{
		switch (Weight)
		{
		case EVekaFontWeight::Hairline: return 0.f;
		case EVekaFontWeight::Thin: return 0.06f;
		case EVekaFontWeight::ExtraLight: return 0.125f;
		case EVekaFontWeight::Light: return 0.25f;
		case EVekaFontWeight::News: return 0.32f;
		case EVekaFontWeight::Regular: return 0.375f;
		case EVekaFontWeight::Medium: return 0.5f;
		case EVekaFontWeight::SemiBold: return 0.625f;
		case EVekaFontWeight::Bold: return 0.75f;
		case EVekaFontWeight::ExtraBold: return 0.82f;
		case EVekaFontWeight::Black: return 0.9f;
		case EVekaFontWeight::ExtraBlack: return 1.f;
		default: return 0.375f;
		}
	}

	float StrokeWidth(float Weight)
	{
		// Slightly eased so the light end matches the hairline / thin text stems.
		return FMath::Lerp(0.45f, 3.4f, FMath::Pow(FMath::Clamp(Weight, 0.f, 1.f), 0.85f));
	}

	float ScaleFactor(EVekaSymbolScale Scale)
	{
		switch (Scale)
		{
		case EVekaSymbolScale::Small: return 0.95f;
		case EVekaSymbolScale::Large: return 1.5f;
		default: return 1.2f;
		}
	}

	bool IsIndefinite(EVekaSymbolEffectKind Kind)
	{
		switch (Kind)
		{
		case EVekaSymbolEffectKind::Pulse:
		case EVekaSymbolEffectKind::VariableColor:
		case EVekaSymbolEffectKind::Rotate:
		case EVekaSymbolEffectKind::Breathe:
		case EVekaSymbolEffectKind::Scale:
		case EVekaSymbolEffectKind::Bounce:
		case EVekaSymbolEffectKind::Wiggle:
			return true;
		default:
			return false;
		}
	}

	FVekaSymbolLayerTransform EvaluateEffect(const FVekaSymbolEffect& Effect, int32 Layer, double Elapsed, bool bActive, int32 VariableSteps)
	{
		FVekaSymbolLayerTransform Out;
		const EVekaSymbolEffectKind Kind = Effect.Kind;
		if (Kind == EVekaSymbolEffectKind::None || Elapsed < 0.0)
		{
			return Out;
		}
		const double T = Elapsed * FMath::Max(Effect.Speed, 0.05f);
		const double D = Duration(Kind, VariableSteps);
		const bool bLoop = bActive && IsIndefinite(Kind);
		double TL = T - (Effect.bByLayer ? Layer * LayerDelay : 0.0);
		if (bLoop && Kind != EVekaSymbolEffectKind::Rotate && Kind != EVekaSymbolEffectKind::Scale && TL > 0.0)
		{
			// Bounce and wiggle rest between repeats; the others loop seamlessly.
			const double Period = (Kind == EVekaSymbolEffectKind::Bounce || Kind == EVekaSymbolEffectKind::Wiggle) ? D + 0.7 : D;
			TL = FMath::Fmod(TL, Period);
		}
		const double P = Clamp01(D > 0.0 ? TL / D : 1.0);

		switch (Kind)
		{
		case EVekaSymbolEffectKind::Appear:
			Out.Scale = static_cast<float>(FMath::Lerp(0.3, 1.0, EaseOutBack(P)));
			Out.Opacity = static_cast<float>(EaseInOut(P * 2.0));
			break;
		case EVekaSymbolEffectKind::Disappear:
			Out.Scale = static_cast<float>(FMath::Lerp(1.0, 0.3, P * P));
			Out.Opacity = static_cast<float>(1.0 - EaseInOut(P));
			break;
		case EVekaSymbolEffectKind::Bounce:
		{
			// Squash, then overshoot and settle.
			const double Squash = FMath::Sin(UE_DOUBLE_PI * Clamp01(P / 0.35));
			const double Rise = FMath::Sin(UE_DOUBLE_PI * Clamp01((P - 0.35) / 0.65));
			Out.Scale = static_cast<float>(1.0 - 0.14 * Squash + 0.1 * Rise);
			Out.Offset = FVector2D(0.0, -1.6 * Rise);
			break;
		}
		case EVekaSymbolEffectKind::Pulse:
		{
			const double Wave = 0.5 - 0.5 * FMath::Cos(UE_DOUBLE_TWO_PI * P);
			Out.Opacity = static_cast<float>(1.0 - 0.65 * Wave);
			break;
		}
		case EVekaSymbolEffectKind::Scale:
			Out.Scale = bActive
				? static_cast<float>(FMath::Lerp(1.0, 1.2, EaseOut(TL / 0.25)))
				: static_cast<float>(1.0 + 0.2 * FMath::Sin(UE_DOUBLE_PI * P));
			break;
		case EVekaSymbolEffectKind::VariableColor:
		{
			const int32 Steps = FMath::Max(VariableSteps, 1);
			const double Lit = bLoop ? FMath::Fmod(T / 0.3, Steps + 1.0) : FMath::Min(T / 0.3, Steps + 1.0);
			// Back to "not driven" once a single run is over.
			Out.VariableHighlight = (!bLoop && T >= D) ? -1.f : static_cast<float>(FMath::FloorToDouble(Lit));
			break;
		}
		case EVekaSymbolEffectKind::Wiggle:
			Out.Rotation = static_cast<float>(12.0 * FMath::Sin(UE_DOUBLE_TWO_PI * 2.5 * P) * (1.0 - P));
			break;
		case EVekaSymbolEffectKind::Rotate:
			Out.Rotation = bLoop ? static_cast<float>(FMath::Fmod(360.0 * T, 360.0)) : static_cast<float>(360.0 * EaseInOut(P));
			break;
		case EVekaSymbolEffectKind::Breathe:
		{
			const double Wave = 0.5 - 0.5 * FMath::Cos(UE_DOUBLE_TWO_PI * P);
			Out.Scale = static_cast<float>(1.0 + 0.08 * Wave);
			Out.Opacity = static_cast<float>(1.0 - 0.25 * Wave);
			break;
		}
		case EVekaSymbolEffectKind::DrawOn:
			Out.Trim = static_cast<float>(EaseInOut(P));
			break;
		default:
			break;
		}
		return Out;
	}

	bool IsEffectFinished(const FVekaSymbolEffect& Effect, double Elapsed, bool bActive, int32 VariableSteps)
	{
		if (Effect.Kind == EVekaSymbolEffectKind::None)
		{
			return true;
		}
		if (bActive && IsIndefinite(Effect.Kind))
		{
			return false;
		}
		const double T = Elapsed * FMath::Max(Effect.Speed, 0.05f);
		return T >= Duration(Effect.Kind, VariableSteps) + (Effect.bByLayer ? 2.0 * LayerDelay : 0.0);
	}
}
