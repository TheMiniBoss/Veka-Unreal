// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaCatalog.h"
#include "Core/VekaTypes.h"
#include "Typography/VekaTypography.h"
#include "Controls/VekaControlStyles.h"
#include "Symbols/VekaSymbolView.h"

const FVekaCatalogArgument* FVekaCatalogEntry::FindArgument(FName Argument) const
{
	return Arguments.FindByPredicate([Argument](const FVekaCatalogArgument& Item) { return Item.Name == Argument; });
}

#define VEKA_CATALOG_NAME(Literal) ([]() -> const FName& { static const FName Cached(TEXT(Literal)); return Cached; }())

namespace Veka::Catalog
{
	namespace
	{
		using EKind = EVekaArgumentKind;

		FVekaCatalogArgument A(const TCHAR* Name, EKind Kind = EKind::Any, const UEnum* Enum = nullptr)
		{
			FVekaCatalogArgument Out;
			Out.Name = Name;
			Out.Kind = Kind;
			Out.Enum = Enum;
			return Out;
		}

		FVekaCatalogEntry E(const TCHAR* Name, TArray<FVekaCatalogArgument> Arguments, const TCHAR* Primary = nullptr, bool bContainer = false, bool bActions = false)
		{
			FVekaCatalogEntry Out;
			Out.Name = Name;
			Out.Arguments = MoveTemp(Arguments);
			Out.PrimaryArgument = Primary ? FName(Primary) : NAME_None;
			Out.bContainer = bContainer;
			Out.bActions = bActions;
			return Out;
		}
	}

	const TArray<FVekaCatalogEntry>& Components()
	{
		// Must match the components the interpreter builds by name (VekaInterpreter.cpp, MakeElement).
		static const TArray<FVekaCatalogEntry> Entries = {
			E(TEXT("VStack"), { A(TEXT("spacing"), EKind::Number), A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaHAlign>()) }, nullptr, true),
			E(TEXT("HStack"), { A(TEXT("spacing"), EKind::Number), A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaVAlign>()) }, nullptr, true),
			E(TEXT("ZStack"), {}, nullptr, true),
			E(TEXT("Text"), { A(TEXT("text"), EKind::Text) }, TEXT("text")),
			E(TEXT("Symbol"), { A(TEXT("name"), EKind::Name), A(TEXT("value"), EKind::Number) }, TEXT("name")),
			E(TEXT("Label"), { A(TEXT("title"), EKind::Text), A(TEXT("symbol"), EKind::Name) }, TEXT("title")),
			E(TEXT("Button"), { A(TEXT("title"), EKind::Text), A(TEXT("symbol"), EKind::Name), A(TEXT("role"), EKind::Enum, StaticEnum<EVekaButtonRole>()) }, TEXT("title"), true, true),
			E(TEXT("Toggle"), { A(TEXT("title"), EKind::Text), A(TEXT("isOn"), EKind::Binding) }, TEXT("title")),
			E(TEXT("Slider"), { A(TEXT("value"), EKind::Binding), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number), A(TEXT("step"), EKind::Number) }, TEXT("value")),
			E(TEXT("Stepper"), { A(TEXT("title"), EKind::Text), A(TEXT("value"), EKind::Binding), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number),
				A(TEXT("step"), EKind::Number), A(TEXT("decimals"), EKind::Number) }, TEXT("title")),
			E(TEXT("TextField"), { A(TEXT("placeholder"), EKind::Text), A(TEXT("text"), EKind::Binding) }, TEXT("placeholder")),
			E(TEXT("ProgressView"), { A(TEXT("value"), EKind::Number), A(TEXT("label"), EKind::Text) }, TEXT("value")),
			E(TEXT("Gauge"), { A(TEXT("value"), EKind::Number), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number), A(TEXT("label"), EKind::Text),
				A(TEXT("current"), EKind::Text) }, TEXT("value")),
			E(TEXT("Divider"), {}),
			E(TEXT("Spacer"), { A(TEXT("min"), EKind::Number) }, TEXT("min")),
			E(TEXT("Rectangle"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("RoundedRectangle"), { A(TEXT("radius"), EKind::Number), A(TEXT("fill"), EKind::Paint) }, TEXT("radius")),
			E(TEXT("Capsule"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("Circle"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("Ellipse"), { A(TEXT("fill"), EKind::Paint) }),
		};
		return Entries;
	}

	const TArray<FVekaCatalogEntry>& Modifiers()
	{
		// Must match VekaInterpreter.cpp, ApplyModifier.
		static const TArray<FVekaCatalogEntry> Entries = {
			E(TEXT("onTap"), {}, nullptr, false, true),
			E(TEXT("on"), { A(TEXT("event"), EKind::Name) }, TEXT("event"), false, true),
			E(TEXT("padding"), { A(TEXT("value"), EKind::Number), A(TEXT("horizontal"), EKind::Number), A(TEXT("vertical"), EKind::Number) }, TEXT("value")),
			E(TEXT("frame"), { A(TEXT("width"), EKind::Number), A(TEXT("height"), EKind::Number), A(TEXT("fillWidth"), EKind::Bool),
				A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaHAlign>()) }),
			E(TEXT("background"), { A(TEXT("paint"), EKind::Paint), A(TEXT("radius"), EKind::Number) }, TEXT("paint")),
			E(TEXT("foreground"), { A(TEXT("paint"), EKind::Paint) }, TEXT("paint")),
			E(TEXT("border"), { A(TEXT("paint"), EKind::Paint), A(TEXT("width"), EKind::Number), A(TEXT("radius"), EKind::Number) }, TEXT("paint")),
			E(TEXT("opacity"), { A(TEXT("value"), EKind::Number) }, TEXT("value")),
			E(TEXT("clip"), {}),
			E(TEXT("offset"), { A(TEXT("x"), EKind::Number), A(TEXT("y"), EKind::Number) }),
			E(TEXT("scale"), { A(TEXT("value"), EKind::Number) }, TEXT("value")),
			E(TEXT("rotation"), { A(TEXT("degrees"), EKind::Number) }, TEXT("degrees")),
			E(TEXT("font"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaTextStyle>()) }, TEXT("style")),
			E(TEXT("fontWeight"), { A(TEXT("weight"), EKind::Enum, StaticEnum<EVekaFontWeight>()) }, TEXT("weight")),
			E(TEXT("fontSize"), { A(TEXT("size"), EKind::Number) }, TEXT("size")),
			E(TEXT("disabled"), { A(TEXT("value"), EKind::Bool) }, TEXT("value")),
			E(TEXT("buttonStyle"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaButtonStyleKind>()) }, TEXT("style")),
			E(TEXT("toggleStyle"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaToggleStyleKind>()) }, TEXT("style")),
			E(TEXT("controlSize"), { A(TEXT("size"), EKind::Enum, StaticEnum<EVekaControlSize>()) }, TEXT("size")),
			E(TEXT("symbolRenderingMode"), { A(TEXT("mode"), EKind::Enum, StaticEnum<EVekaSymbolRenderingMode>()) }, TEXT("mode")),
			E(TEXT("animation"), { A(TEXT("value")) }, TEXT("value")),
			E(TEXT("transition"), { A(TEXT("kind"), EKind::Name) }, TEXT("kind")),
			E(TEXT("style"), { A(TEXT("name"), EKind::Style) }, TEXT("name")),
			E(TEXT("backdropBlur"), { A(TEXT("radius"), EKind::Number), A(TEXT("cornerRadius"), EKind::Number), A(TEXT("tint"), EKind::Paint),
				A(TEXT("tintAmount"), EKind::Number), A(TEXT("saturation"), EKind::Number) }, TEXT("radius")),
			E(TEXT("frostedGlass"), { A(TEXT("blur"), EKind::Number), A(TEXT("cornerRadius"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("noise"), EKind::Number) }),
			E(TEXT("blendMode"), { A(TEXT("mode"), EKind::Enum, StaticEnum<EVekaBlendMode>()) }, TEXT("mode")),
			E(TEXT("liquidDistortion"), { A(TEXT("strength"), EKind::Number), A(TEXT("scale"), EKind::Number), A(TEXT("speed"), EKind::Number),
				A(TEXT("cornerRadius"), EKind::Number) }, TEXT("strength")),
			E(TEXT("glass"), { A(TEXT("cornerRadius"), EKind::Number), A(TEXT("blur"), EKind::Number), A(TEXT("refraction"), EKind::Number), A(TEXT("bezel"), EKind::Number),
				A(TEXT("aberration"), EKind::Number), A(TEXT("highlight"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("tintAmount"), EKind::Number) }, TEXT("cornerRadius")),
			E(TEXT("glassGroup"), { A(TEXT("smoothness"), EKind::Number), A(TEXT("blur"), EKind::Number), A(TEXT("refraction"), EKind::Number), A(TEXT("bezel"), EKind::Number),
				A(TEXT("aberration"), EKind::Number), A(TEXT("highlight"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("tintAmount"), EKind::Number) }, TEXT("smoothness")),
		};
		return Entries;
	}

	const FVekaCatalogEntry* FindComponent(FName Name)
	{
		return Components().FindByPredicate([Name](const FVekaCatalogEntry& Entry) { return Entry.Name == Name; });
	}

	const FVekaCatalogEntry* FindModifier(FName Name)
	{
		return Modifiers().FindByPredicate([Name](const FVekaCatalogEntry& Entry) { return Entry.Name == Name; });
	}

	const TArray<FVekaCatalogFunction>& Functions()
	{
		auto F = [](const TCHAR* Name, int32 Min, int32 Max, const TCHAR* Signature, const TCHAR* Description)
		{
			FVekaCatalogFunction Out;
			Out.Name = Name;
			Out.MinArguments = Min;
			Out.MaxArguments = Max;
			Out.Signature = Signature;
			Out.Description = Description;
			return Out;
		};
		static const TArray<FVekaCatalogFunction> Entries = {
			F(TEXT("sin"), 1, 1, TEXT("sin(angle)"), TEXT("Sine of an angle in radians.")),
			F(TEXT("cos"), 1, 1, TEXT("cos(angle)"), TEXT("Cosine of an angle in radians.")),
			F(TEXT("tan"), 1, 1, TEXT("tan(angle)"), TEXT("Tangent of an angle in radians.")),
			F(TEXT("atan2"), 2, 2, TEXT("atan2(y, x)"), TEXT("Angle of the point (x, y), in radians.")),
			F(TEXT("abs"), 1, 1, TEXT("abs(x)"), TEXT("Absolute value.")),
			F(TEXT("sqrt"), 1, 1, TEXT("sqrt(x)"), TEXT("Square root (0 below 0).")),
			F(TEXT("pow"), 2, 2, TEXT("pow(x, power)"), TEXT("x to a power.")),
			F(TEXT("exp"), 1, 1, TEXT("exp(x)"), TEXT("e to the power x.")),
			F(TEXT("floor"), 1, 1, TEXT("floor(x)"), TEXT("Largest whole number not above x.")),
			F(TEXT("ceil"), 1, 1, TEXT("ceil(x)"), TEXT("Smallest whole number not below x.")),
			F(TEXT("round"), 1, 1, TEXT("round(x)"), TEXT("Nearest whole number.")),
			F(TEXT("fract"), 1, 1, TEXT("fract(x)"), TEXT("Fractional part: x - floor(x), 0 to 1 (a value that loops).")),
			F(TEXT("sign"), 1, 1, TEXT("sign(x)"), TEXT("-1, 0 or 1.")),
			F(TEXT("clamp"), 3, 3, TEXT("clamp(x, low, high)"), TEXT("x kept between low and high.")),
			F(TEXT("lerp"), 3, 3, TEXT("lerp(a, b, t)"), TEXT("From a (t = 0) to b (t = 1).")),
			F(TEXT("smoothstep"), 3, 3, TEXT("smoothstep(edge0, edge1, x)"), TEXT("0 before edge0, 1 after edge1, a smooth curve in between.")),
			F(TEXT("random"), 1, 3, TEXT("random(seed) / random(seed, low, high)"),
				TEXT("A random number from a seed: the same seed always gives the same number (0 to 1, or low to high).")),
			[&F]
			{
				FVekaCatalogFunction Out = F(TEXT("hsv"), 3, 4, TEXT("hsv(hue, saturation, value, alpha)"),
					TEXT("A color from its hue (degrees, 0 = red, 120 = green, 240 = blue), saturation and value (0 to 1)."));
				Out.bColor = true;
				return Out;
			}(),
			[&F]
			{
				FVekaCatalogFunction Out = F(TEXT("rgba"), 4, 4, TEXT("rgba(red, green, blue, alpha)"), TEXT("A color from its channels, 0 to 1 (values can be computed)."));
				Out.bColor = true;
				return Out;
			}(),
		};
		return Entries;
	}

	FLinearColor CallColorFunction(FName Name, TConstArrayView<double> Arguments)
	{
		auto A = [&Arguments](int32 Index, double Default) { return Arguments.IsValidIndex(Index) ? Arguments[Index] : Default; };
		if (Name == TEXT("hsv"))
		{
			const float Hue = static_cast<float>(FMath::Fmod(FMath::Fmod(A(0, 0.0), 360.0) + 360.0, 360.0));
			FLinearColor Color = FLinearColor(Hue, FMath::Clamp(static_cast<float>(A(1, 1.0)), 0.f, 1.f), FMath::Clamp(static_cast<float>(A(2, 1.0)), 0.f, 1.f)).HSVToLinearRGB();
			Color.A = FMath::Clamp(static_cast<float>(A(3, 1.0)), 0.f, 1.f);
			return Color;
		}
		if (Name == TEXT("rgba"))
		{
			return FLinearColor(static_cast<float>(A(0, 0.0)), static_cast<float>(A(1, 0.0)), static_cast<float>(A(2, 0.0)), static_cast<float>(A(3, 1.0)));
		}
		return FLinearColor::White;
	}

	const FVekaCatalogFunction* FindFunction(FName Name)
	{
		return Functions().FindByPredicate([Name](const FVekaCatalogFunction& Entry) { return Entry.Name == Name; });
	}

	double CallFunction(FName Name, TConstArrayView<double> Arguments)
	{
		auto A = [&Arguments](int32 Index) { return Arguments.IsValidIndex(Index) ? Arguments[Index] : 0.0; };
		// Cached names: no string work per call (an animation calls these hundreds of times a frame).
		if (Name == VEKA_CATALOG_NAME("sin")) { return FMath::Sin(A(0)); }
		if (Name == VEKA_CATALOG_NAME("cos")) { return FMath::Cos(A(0)); }
		if (Name == VEKA_CATALOG_NAME("tan")) { return FMath::Tan(A(0)); }
		if (Name == VEKA_CATALOG_NAME("atan2")) { return FMath::Atan2(A(0), A(1)); }
		if (Name == VEKA_CATALOG_NAME("abs")) { return FMath::Abs(A(0)); }
		if (Name == VEKA_CATALOG_NAME("sqrt")) { return FMath::Sqrt(FMath::Max(0.0, A(0))); }
		if (Name == VEKA_CATALOG_NAME("pow")) { return FMath::Pow(A(0), A(1)); }
		if (Name == VEKA_CATALOG_NAME("exp")) { return FMath::Exp(A(0)); }
		if (Name == VEKA_CATALOG_NAME("floor")) { return FMath::FloorToDouble(A(0)); }
		if (Name == VEKA_CATALOG_NAME("ceil")) { return FMath::CeilToDouble(A(0)); }
		if (Name == VEKA_CATALOG_NAME("round")) { return FMath::RoundToDouble(A(0)); }
		if (Name == VEKA_CATALOG_NAME("fract")) { return A(0) - FMath::FloorToDouble(A(0)); }
		if (Name == VEKA_CATALOG_NAME("sign")) { return FMath::Sign(A(0)); }
		if (Name == VEKA_CATALOG_NAME("clamp")) { return FMath::Clamp(A(0), FMath::Min(A(1), A(2)), FMath::Max(A(1), A(2))); }
		if (Name == VEKA_CATALOG_NAME("lerp")) { return A(0) + (A(1) - A(0)) * A(2); }
		if (Name == VEKA_CATALOG_NAME("smoothstep"))
		{
			const double Span = A(1) - A(0);
			const double T = FMath::Clamp(Span == 0.0 ? (A(2) >= A(1) ? 1.0 : 0.0) : (A(2) - A(0)) / Span, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		if (Name == VEKA_CATALOG_NAME("random"))
		{
			// SplitMix64 of the seed's bits: the same seed always gives the same number, close seeds unrelated ones.
			const double Seed = A(0) + 0.0;
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Seed, sizeof(Bits));
			uint64 Z = Bits + 0x9E3779B97F4A7C15ull;
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			Z ^= Z >> 31;
			const double Unit = static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
			return Arguments.Num() >= 3 ? A(1) + (A(2) - A(1)) * Unit : Unit;
		}
		return 0.0;
	}
}
