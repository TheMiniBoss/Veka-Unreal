// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"

namespace
{
	constexpr EAutomationTestFlags VekaSymbolTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	FVekaEnvironment SymbolEnvOf(const FVekaElement& Element)
	{
		TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
		Runtime->SetRoot(Element);
		return Runtime->GetRootNode()->GetEnvironment();
	}

	struct FSymbolCapture
	{
		TSharedRef<SWindow> Window;
		FSlateWindowElementList Elements;
		FHittestGrid HittestGrid;

		FSymbolCapture(const FVekaElement& Root, const FVector2f& Size = FVector2f(200.f, 200.f))
			: Window(SNew(SWindow).ClientSize(Size).CreateTitleBar(false))
			, Elements(Window)
		{
			TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Root);
			HittestGrid.SetHittestArea(FVector2f::ZeroVector, Size);
			const FPaintArgs Args(nullptr, HittestGrid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Host->SlatePrepass(1.f);
			Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}

		const FSlateDrawElementArray<FSlateCustomVertsElement>& Meshes() const
		{
			return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_CustomVerts>();
		}

		int32 TextCount() const
		{
			return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Text>().Num();
		}

		/** Extent of the fully covered vertices of every mesh. */
		FBox2f CoreBounds() const
		{
			FBox2f Box(ForceInit);
			for (const FSlateCustomVertsElement& Mesh : Meshes())
			{
				for (const FSlateVertex& Vertex : Mesh.Vertices)
				{
					if (Vertex.Color.A > 0)
					{
						Box += Vertex.Position;
					}
				}
			}
			return Box;
		}

		bool HasAlphaNear(uint8 Alpha, int32 Tolerance = 4) const
		{
			for (const FSlateCustomVertsElement& Mesh : Meshes())
			{
				for (const FSlateVertex& Vertex : Mesh.Vertices)
				{
					if (FMath::Abs(static_cast<int32>(Vertex.Color.A) - Alpha) <= Tolerance)
					{
						return true;
					}
				}
			}
			return false;
		}
	};

	FVekaSymbolVariant MakeVariant(bool bFill, EVekaSymbolEnclosure Enclosure = EVekaSymbolEnclosure::None, bool bSlash = false)
	{
		FVekaSymbolVariant Variant;
		Variant.bFill = bFill;
		Variant.Enclosure = Enclosure;
		Variant.bSlash = bSlash;
		return Variant;
	}

	bool HasRenderer()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolLibraryTest, "VekaUI.Symbols.Library", VekaSymbolTestFlags)
bool FVekaSymbolLibraryTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Symbols;
	const TArray<FName> Names = GetBuiltInNames();
	TestTrue(FString::Printf(TEXT("About 150 built-in symbols (%d)"), Names.Num()), Names.Num() >= 150);

	TSet<FName> Unique(Names);
	TestEqual(TEXT("Unique names"), Unique.Num(), Names.Num());

	int32 Bad = 0;
	for (const FName Name : Names)
	{
		const FVekaSymbolDefinition* Definition = Find(Name);
		if (!Definition || Definition->Shapes.IsEmpty())
		{
			AddError(FString::Printf(TEXT("%s has no shape"), *Name.ToString()));
			++Bad;
			continue;
		}
		for (int32 Index = 0; Index < Definition->Shapes.Num(); ++Index)
		{
			const TArray<FVekaPolyline>& Geometry = GetShapeGeometry(*Definition, Index);
			bool bInGrid = !Geometry.IsEmpty();
			for (const FVekaPolyline& Polyline : Geometry)
			{
				for (const FVector2D& Point : Polyline.Points)
				{
					bInGrid &= Point.X >= -0.5 && Point.X <= 24.5 && Point.Y >= -0.5 && Point.Y <= 24.5;
				}
			}
			if (!bInGrid)
			{
				AddError(FString::Printf(TEXT("%s shape %d is empty or outside the 24 grid"), *Name.ToString(), Index));
				++Bad;
			}
		}
	}
	TestEqual(TEXT("Every symbol has geometry in the grid"), Bad, 0);

	for (const TPair<FName, FName>& Alias : GetAlpha13Aliases())
	{
		TestNotNull(*FString::Printf(TEXT("Alpha 13 alias %s"), *Alias.Key.ToString()), Find(Alias.Key));
	}
	TestNull(TEXT("Unknown name"), Find(TEXT("no.such.symbol")));

	// Multicolor tokens: a color right after a shape tints it; one after a role change only tints what follows.
	const FVekaSymbolDefinition* Heart = Find(TEXT("heart"));
	TestTrue(TEXT("heart is red in multicolor"), Heart && Heart->Shapes[0].ColorToken == TEXT("Red"));
	const FVekaSymbolDefinition* Palette = Find(TEXT("paintpalette"));
	TestTrue(TEXT("Palette body keeps the foreground"), Palette && Palette->Shapes[0].ColorToken.IsNone());
	TestTrue(TEXT("Palette dots keep their own colors"), Palette && Palette->Shapes.Num() == 5
		&& Palette->Shapes[1].ColorToken == TEXT("Red") && Palette->Shapes[2].ColorToken == TEXT("Yellow") && Palette->Shapes[4].ColorToken == TEXT("Blue"));
	const FVekaSymbolDefinition* Rain = Find(TEXT("cloud.rain"));
	TestTrue(TEXT("Cloud stays foreground, drops blue"), Rain && Rain->Shapes[0].ColorToken.IsNone() && Rain->Shapes.Last().ColorToken == TEXT("Blue"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolResolveTest, "VekaUI.Symbols.Resolve", VekaSymbolTestFlags)
bool FVekaSymbolResolveTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Symbols;
	FVekaSymbolVariant Variant;

	const FVekaSymbolDefinition* Bell = Resolve(TEXT("bell.slash.fill"), Variant);
	TestTrue(TEXT("bell.slash.fill -> bell"), Bell && Bell->Name == TEXT("bell"));
	TestTrue(TEXT("fill + slash"), Variant.bFill && Variant.bSlash && Variant.Enclosure == EVekaSymbolEnclosure::None);

	const FVekaSymbolDefinition* Check = Resolve(TEXT("checkmark.circle"), Variant);
	TestTrue(TEXT("checkmark.circle -> checkmark"), Check && Check->Name == TEXT("checkmark"));
	TestEqual(TEXT("Circle enclosure"), Variant.Enclosure, EVekaSymbolEnclosure::Circle);
	TestFalse(TEXT("Previous variant reset"), Variant.bFill);

	// "circle" is a symbol itself: circle.fill is the filled circle, not an enclosure.
	const FVekaSymbolDefinition* Circle = Resolve(TEXT("circle.fill"), Variant);
	TestTrue(TEXT("circle.fill -> circle"), Circle && Circle->Name == TEXT("circle"));
	TestTrue(TEXT("Fill, no enclosure"), Variant.bFill && Variant.Enclosure == EVekaSymbolEnclosure::None);

	const FVekaSymbolDefinition* Path = Resolve(TEXT("arrow.2.circlepath"), Variant);
	TestTrue(TEXT("Dotted name with a number"), Path && Path->Name == TEXT("arrow.2.circlepath") && Variant == FVekaSymbolVariant());

	const FVekaSymbolDefinition* Info = Resolve(TEXT("Info"), Variant);
	TestTrue(TEXT("Alpha 13 Info -> info.circle"), Info && Info->Name == TEXT("info") && Variant.Enclosure == EVekaSymbolEnclosure::Circle);

	TestNull(TEXT("Unknown base"), Resolve(TEXT("nothing.fill"), Variant));
	TestNull(TEXT("Unknown suffix"), Resolve(TEXT("bell.blue"), Variant));
	TestNull(TEXT("None"), Resolve(NAME_None, Variant));

	FVekaSymbolVariant A;
	A.bFill = true;
	FVekaSymbolVariant B;
	B.Enclosure = EVekaSymbolEnclosure::Square;
	const FVekaSymbolVariant Merged = A.Merged(B);
	TestTrue(TEXT("Variants merge"), Merged.bFill && Merged.Enclosure == EVekaSymbolEnclosure::Square && !Merged.bSlash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolMetricsTest, "VekaUI.Symbols.Metrics", VekaSymbolTestFlags)
bool FVekaSymbolMetricsTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	using namespace Veka::Symbols;

	float Previous = -1.f;
	bool bIncreasing = true;
	for (int32 Weight = 0; Weight <= static_cast<int32>(EVekaFontWeight::ExtraBlack); ++Weight)
	{
		const float Value = WeightFromFont(static_cast<EVekaFontWeight>(Weight));
		bIncreasing &= Value > Previous;
		Previous = Value;
	}
	TestTrue(TEXT("Symbol weight follows the 12 font weights"), bIncreasing);
	TestEqual(TEXT("Hairline"), WeightFromFont(EVekaFontWeight::Hairline), 0.f);
	TestEqual(TEXT("ExtraBlack"), WeightFromFont(EVekaFontWeight::ExtraBlack), 1.f);
	TestTrue(TEXT("Stroke width grows with weight"), StrokeWidth(0.f) < StrokeWidth(0.5f) && StrokeWidth(0.5f) < StrokeWidth(1.f));

	const float Body = GetSymbolSize(SymbolEnvOf(Symbol(TEXT("bell"))));
	const float Title = GetSymbolSize(SymbolEnvOf(Symbol(TEXT("bell")).Font(EVekaTextStyle::Title1)));
	const float Small = GetSymbolSize(SymbolEnvOf(Symbol(TEXT("bell")).SymbolScale(EVekaSymbolScale::Small)));
	const float Large = GetSymbolSize(SymbolEnvOf(Symbol(TEXT("bell")).SymbolScale(EVekaSymbolScale::Large)));
	TestTrue(TEXT("Follows the font size"), Title > Body);
	TestTrue(TEXT("Small < medium < large"), Small < Body && Body < Large);

	// Environment style.
	const FVekaSymbolStyle& Style = VekaEnv::GetSymbolStyle(SymbolEnvOf(Symbol(TEXT("bell"))
		.SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical)
		.SymbolVariant(MakeVariant(true))
		.SymbolWeight(0.8f)));
	TestEqual(TEXT("Mode"), Style.RenderingMode, EVekaSymbolRenderingMode::Hierarchical);
	TestTrue(TEXT("Variant"), Style.Variant.bFill);
	TestTrue(TEXT("Weight override"), Style.bOverrideWeight && FMath::IsNearlyEqual(Style.Weight, 0.8f));
	TestFalse(TEXT("Default has no override"), VekaEnv::GetSymbolStyle(VekaEnv::MakeDefault()).bOverrideWeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolEffectsTest, "VekaUI.Symbols.Effects", VekaSymbolTestFlags)
bool FVekaSymbolEffectsTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Symbols;
	FVekaSymbolEffect Effect;

	Effect.Kind = EVekaSymbolEffectKind::None;
	const FVekaSymbolLayerTransform Identity = EvaluateEffect(Effect, 0, 0.3, true, 3);
	TestTrue(TEXT("None is identity"), Identity.Scale == 1.f && Identity.Opacity == 1.f && Identity.Rotation == 0.f && Identity.Trim == 1.f);

	Effect.Kind = EVekaSymbolEffectKind::Appear;
	TestTrue(TEXT("Appear starts hidden and small"), EvaluateEffect(Effect, 0, 0.0, false, 3).Opacity < 0.01f && EvaluateEffect(Effect, 0, 0.0, false, 3).Scale < 0.5f);
	const FVekaSymbolLayerTransform Shown = EvaluateEffect(Effect, 0, 2.0, false, 3);
	TestTrue(TEXT("Appear ends at rest"), FMath::IsNearlyEqual(Shown.Scale, 1.f, 1e-3f) && FMath::IsNearlyEqual(Shown.Opacity, 1.f, 1e-3f));
	TestTrue(TEXT("Appear finishes"), IsEffectFinished(Effect, 2.0, false));
	TestFalse(TEXT("Appear not finished at start"), IsEffectFinished(Effect, 0.1, false));
	TestTrue(TEXT("Layers start one after the other"), EvaluateEffect(Effect, 2, 0.15, false, 3).Opacity < EvaluateEffect(Effect, 0, 0.15, false, 3).Opacity);
	Effect.bByLayer = false;
	TestEqual(TEXT("Whole symbol: same timing"), EvaluateEffect(Effect, 2, 0.15, false, 3).Opacity, EvaluateEffect(Effect, 0, 0.15, false, 3).Opacity);
	Effect.bByLayer = true;

	Effect.Kind = EVekaSymbolEffectKind::Disappear;
	TestTrue(TEXT("Disappear stays hidden"), EvaluateEffect(Effect, 0, 5.0, false, 3).Opacity < 0.01f);

	Effect.Kind = EVekaSymbolEffectKind::Pulse;
	TestFalse(TEXT("Active pulse never finishes"), IsEffectFinished(Effect, 100.0, true));
	TestTrue(TEXT("One-shot pulse finishes"), IsEffectFinished(Effect, 3.0, false));
	TestTrue(TEXT("Pulse dims mid-way"), EvaluateEffect(Effect, 0, 0.5, true, 3).Opacity < 0.5f);

	Effect.Kind = EVekaSymbolEffectKind::Rotate;
	TestTrue(TEXT("Rotate: quarter turn in 0.25 s"), FMath::IsNearlyEqual(EvaluateEffect(Effect, 0, 0.25, true, 3).Rotation, 90.f, 0.5f));
	Effect.Speed = 2.f;
	TestTrue(TEXT("Speed doubles it"), FMath::IsNearlyEqual(EvaluateEffect(Effect, 0, 0.25, true, 3).Rotation, 180.f, 0.5f));
	Effect.Speed = 1.f;

	Effect.Kind = EVekaSymbolEffectKind::DrawOn;
	const float Early = EvaluateEffect(Effect, 0, 0.1, false, 3).Trim;
	const float Late = EvaluateEffect(Effect, 0, 0.5, false, 3).Trim;
	TestTrue(TEXT("DrawOn draws along"), Early < Late && Late < 1.f);
	TestEqual(TEXT("DrawOn complete"), EvaluateEffect(Effect, 0, 3.0, false, 3).Trim, 1.f);

	Effect.Kind = EVekaSymbolEffectKind::VariableColor;
	TestEqual(TEXT("VariableColor lights layers in turn"), EvaluateEffect(Effect, 0, 0.65, true, 3).VariableHighlight, 2.f);
	TestEqual(TEXT("One-shot VariableColor releases the layers"), EvaluateEffect(Effect, 0, 5.0, false, 3).VariableHighlight, -1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolRenderTest, "VekaUI.Symbols.Render", VekaSymbolTestFlags)
bool FVekaSymbolRenderTest::RunTest(const FString& Parameters)
{
	if (!HasRenderer())
	{
		AddInfo(TEXT("No Slate renderer: skipped."));
		return true;
	}
	using namespace Veka;

	const FSymbolCapture Check(Symbol(TEXT("checkmark")));
	TestTrue(TEXT("A symbol draws meshes"), Check.Meshes().Num() > 0);
	TestTrue(TEXT("Monochrome is opaque white"), Check.HasAlphaNear(255, 0));

	// Weight = stroke width: the height of "minus" is its stroke.
	const float Thin = FSymbolCapture(Symbol(TEXT("minus")).SymbolWeight(0.1f).Font(EVekaTextStyle::LargeTitle)).CoreBounds().GetSize().Y;
	const float Thick = FSymbolCapture(Symbol(TEXT("minus")).SymbolWeight(1.f).Font(EVekaTextStyle::LargeTitle)).CoreBounds().GetSize().Y;
	TestTrue(FString::Printf(TEXT("Heavier symbol, thicker stroke (%.2f < %.2f)"), Thin, Thick), Thin * 1.8f < Thick);
	const float Regular = FSymbolCapture(Symbol(TEXT("minus")).Font(EVekaTextStyle::LargeTitle)).CoreBounds().GetSize().Y;
	const float Bold = FSymbolCapture(Symbol(TEXT("minus")).FontWeight(EVekaFontWeight::Black).Font(EVekaTextStyle::LargeTitle)).CoreBounds().GetSize().Y;
	TestTrue(TEXT("Font weight drives the symbol weight"), Regular < Bold);
	const float BodyWidth = FSymbolCapture(Symbol(TEXT("minus"))).CoreBounds().GetSize().X;
	const float TitleWidth = FSymbolCapture(Symbol(TEXT("minus")).Font(EVekaTextStyle::LargeTitle)).CoreBounds().GetSize().X;
	TestTrue(TEXT("Font size drives the symbol size"), BodyWidth < TitleWidth);

	// Variants.
	const int32 Outline = FSymbolCapture(Symbol(TEXT("circle"))).Meshes().Num();
	const int32 Filled = FSymbolCapture(Symbol(TEXT("circle.fill"))).Meshes().Num();
	TestTrue(TEXT("Fill adds a filled mesh"), Filled > Outline);
	const int32 Slashed = FSymbolCapture(Symbol(TEXT("bell.slash"))).Meshes().Num();
	const int32 Plain = FSymbolCapture(Symbol(TEXT("bell"))).Meshes().Num();
	TestEqual(TEXT("Slash = gap + line"), Slashed, Plain + 2);
	TestEqual(TEXT("Environment variant"), FSymbolCapture(Symbol(TEXT("bell")).SymbolVariant(MakeVariant(false, EVekaSymbolEnclosure::None, true))).Meshes().Num(), Plain + 2);
	TestEqual(TEXT("Circle enclosure adds an outline"), FSymbolCapture(Symbol(TEXT("checkmark.circle"))).Meshes().Num(), Check.Meshes().Num() + 1);

	// Rendering modes.
	const FSymbolCapture Mono(Symbol(TEXT("exclamationmark.triangle")));
	const FSymbolCapture Hier(Symbol(TEXT("exclamationmark.triangle")).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical));
	TestFalse(TEXT("Monochrome: no 60 % layer"), Mono.HasAlphaNear(153));
	TestTrue(TEXT("Hierarchical: secondary layer at 60 %"), Hier.HasAlphaNear(153));
	const FSymbolCapture Palette(Symbol(TEXT("exclamationmark.triangle")).SymbolPalette({ FLinearColor(1.f, 1.f, 0.f), FLinearColor(1.f, 0.f, 0.f) }));
	bool bRed = false;
	bool bYellow = false;
	for (const FSlateCustomVertsElement& Mesh : Palette.Meshes())
	{
		for (const FSlateVertex& Vertex : Mesh.Vertices)
		{
			bRed |= Vertex.Color.A == 255 && Vertex.Color.R > 240 && Vertex.Color.G < 15;
			bYellow |= Vertex.Color.A == 255 && Vertex.Color.R > 240 && Vertex.Color.G > 240 && Vertex.Color.B < 15;
		}
	}
	TestTrue(TEXT("Palette: both colors"), bRed && bYellow);

	// Variable value: unlit layers at 25 %.
	TestTrue(TEXT("wifi at 0: arcs dimmed"), FSymbolCapture(Symbol(TEXT("wifi"), 0.f)).HasAlphaNear(64));
	TestFalse(TEXT("wifi at 1: all lit"), FSymbolCapture(Symbol(TEXT("wifi"), 1.f)).HasAlphaNear(64));
	TestFalse(TEXT("wifi without value: all lit"), FSymbolCapture(Symbol(TEXT("wifi"))).HasAlphaNear(64));

	// Badge and unknown names.
	TestEqual(TEXT("Badge count is text"), FSymbolCapture(Symbol(TEXT("bell")).SymbolBadge(3)).TextCount(), 1);
	TestEqual(TEXT("No badge, no text"), FSymbolCapture(Symbol(TEXT("bell"))).TextCount(), 0);
	TestEqual(TEXT("Dot badge: ring + dot"), FSymbolCapture(Symbol(TEXT("bell")).SymbolBadge(-1)).Meshes().Num(), Plain + 2);
	const FSymbolCapture Unknown(Symbol(TEXT("no.such.symbol")));
	TestTrue(TEXT("Unknown name draws the placeholder"), Unknown.Meshes().Num() > 0 && !Unknown.HasAlphaNear(255, 0));
	return true;
}

#endif
