// Copyright TheMiniBoss. All Rights Reserved.

// Phase 4: backdrops (render graph capture shared by a host, blur levels, quality, language).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaBackdrop.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "View/VekaViewAsset.h"
#include "View/VekaDocument.h"
#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaPaint.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Modifiers/VekaColoraModifiers.h"

namespace
{
	/** Custom elements (live drawers) and boxes a host paints. */
	struct FPaintCounts
	{
		int32 Custom = 0;
		int32 Boxes = 0;
	};

	FPaintCounts PaintAndCount(const FVekaElement& Root)
	{
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(Root);
		const FVector2f Size(400.f, 300.f);
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Host->SlatePrepass(1.f);
		Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		FPaintCounts Counts;
		for (const FSlateCustomDrawerElement& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Custom>())
		{
			Counts.Custom += Element.CustomDrawer.IsValid() ? 1 : 0;
		}
		Counts.Boxes = Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_RoundedBox>().Num();
		return Counts;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaBackdropTest, "VekaUI.Render.Backdrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaBackdropTest::RunTest(const FString& Parameters)
{
	// Blur levels: continuous from 0, each level doubles the radius, capped by the quality.
	TestEqual(TEXT("no blur, level 0"), Veka::Render::BlurLevel(0.f), 0.f);
	TestTrue(TEXT("1.5 px is level 1"), FMath::IsNearlyEqual(Veka::Render::BlurLevel(1.5f), 1.f));
	TestTrue(TEXT("4.5 px is level 2"), FMath::IsNearlyEqual(Veka::Render::BlurLevel(4.5f), 2.f));
	TestTrue(TEXT("levels grow with the radius"), Veka::Render::BlurLevel(30.f) > Veka::Render::BlurLevel(29.f));
	TestEqual(TEXT("24 px needs 5 levels"), Veka::Render::BlurLevels(24.f), 5);
	TestEqual(TEXT("levels are capped"), Veka::Render::BlurLevels(5000.f), 7);
	TestEqual(TEXT("low quality caps at 4"), Veka::Render::MaxBlurLevels(EVekaBackdropQuality::Low), 4);

	// One capture per host, one composite per backdrop.
	using namespace Veka;
	const FVekaElement One = Text(TEXT("Glass")).Padding(20.f).BackdropBlur(16.f, FVekaCornerRadii(12.f));
	TestEqual(TEXT("one backdrop: its composite (it takes the capture)"), PaintAndCount(One).Custom, 1);
	const FVekaElement Three = VStack(8.f)(
		Text(TEXT("A")).Padding(10.f).BackdropBlur(6.f),
		Text(TEXT("B")).Padding(10.f).BackdropBlur(30.f),
		Text(TEXT("C")).Padding(10.f).FrostedGlass()
	);
	TestEqual(TEXT("three backdrops: three composites (the first one captures for all)"), PaintAndCount(Three).Custom, 3);
	TestEqual(TEXT("no backdrop: no capture"), PaintAndCount(Text(TEXT("Plain")).Padding(10.f)).Custom, 0);

	// Quality 0: a tinted fill instead, no capture.
	IConsoleVariable* Quality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Veka.Backdrop.Quality"));
	if (TestNotNull(TEXT("quality setting"), Quality))
	{
		const int32 Saved = Quality->GetInt();
		ON_SCOPE_EXIT { Quality->Set(Saved, ECVF_SetByCode); };
		Quality->Set(0, ECVF_SetByCode);
		const FPaintCounts Off = PaintAndCount(One);
		TestEqual(TEXT("quality 0: no capture"), Off.Custom, 0);
		TestTrue(TEXT("quality 0: a fill"), Off.Boxes >= 1);
	}

	// Frosted glass preset.
	const FVekaBackdropModifier Frosted = FVekaBackdropModifier::FrostedGlass();
	TestTrue(TEXT("frosted glass has grain and more tint"), Frosted.Noise > 0.f && Frosted.TintAmount > FVekaBackdropModifier().TintAmount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaBackdropViewTest, "VekaUI.Render.BackdropInViews", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaBackdropViewTest::RunTest(const FString& Parameters)
{
	// `.frostedGlass(blur: 20, tint: .Accent, cornerRadius: 12)` in a view builds the backdrop modifier.
	FVekaDocumentBuilder Builder;
	FVekaDocumentBuilder::FNode Node = Builder.Node(TEXT("Text"));
	Node.Arg(TEXT("text"), Builder.Lit(TEXT("Glass")));
	Node.Modifier(TEXT("frostedGlass"), { { TEXT("blur"), Builder.Lit(20.0) }, { TEXT("tint"), Builder.Lit(TEXT("Accent")) }, { TEXT("cornerRadius"), Builder.Lit(12.0) } });
	Builder.SetRoot(Node);
	UVekaView* View = NewObject<UVekaView>();
	View->Document = Builder.Release();
	const FVekaElement Root = View->BuildRoot();
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(Root);
	Host->SlatePrepass(1.f);
	const TSharedPtr<FVekaNode> Built = Host->GetRuntime()->GetRootNode();
	const FVekaBackdropModifier* Backdrop = nullptr;
	TFunction<void(const FVekaNode&)> Find = [&](const FVekaNode& N)
	{
		for (const FInstancedStruct& Modifier : N.GetModifiers())
		{
			Backdrop = Backdrop ? Backdrop : Modifier.GetPtr<FVekaBackdropModifier>();
		}
		for (const TSharedRef<FVekaNode>& Child : N.GetChildren())
		{
			Find(*Child);
		}
	};
	if (TestTrue(TEXT("tree built"), Built.IsValid()))
	{
		Find(*Built);
	}
	if (TestNotNull(TEXT("backdrop modifier"), Backdrop))
	{
		TestEqual(TEXT("blur"), Backdrop->Radius, 20.f);
		TestEqual(TEXT("corner radius"), Backdrop->CornerRadii.TopLeft, 12.f);
		TestTrue(TEXT("tint token"), Backdrop->Tint.Type == EVekaPaintType::Token && Backdrop->Tint.Token == TEXT("Accent"));
		TestTrue(TEXT("frosted: grain"), Backdrop->Noise > 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaGlassTest, "VekaUI.Render.Glass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaGlassTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	// A glass: its composite and the host capture.
	TestEqual(TEXT("one glass"), PaintAndCount(Text(TEXT("Glass")).Padding(20.f).Glass(FVekaCornerRadii(16.f))).Custom, 1);

	// A group: the children give their shapes, only the group draws (plus the capture).
	const FVekaElement Group = HStack(8.f)(
		Text(TEXT("A")).Padding(12.f).Glass(FVekaCornerRadii(12.f)),
		Text(TEXT("B")).Padding(12.f).Glass(FVekaCornerRadii(12.f))
	).GlassGroup(20.f);
	TestEqual(TEXT("a group of two: one glass"), PaintAndCount(Group).Custom, 1);

	// Glass and backdrops in one host share the capture.
	const FVekaElement Mixed = VStack(8.f)(Text(TEXT("A")).Padding(10.f).Glass(), Text(TEXT("B")).Padding(10.f).BackdropBlur(12.f));
	TestEqual(TEXT("glass + backdrop: two composites, one capture"), PaintAndCount(Mixed).Custom, 2);

	// Quality 0: fills, no capture (the group draws nothing, its children fill).
	IConsoleVariable* Quality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Veka.Backdrop.Quality"));
	if (TestNotNull(TEXT("quality setting"), Quality))
	{
		const int32 Saved = Quality->GetInt();
		ON_SCOPE_EXIT { Quality->Set(Saved, ECVF_SetByCode); };
		Quality->Set(0, ECVF_SetByCode);
		const FPaintCounts Off = PaintAndCount(Group);
		TestEqual(TEXT("quality 0: no capture"), Off.Custom, 0);
		TestTrue(TEXT("quality 0: the two children fill"), Off.Boxes >= 2);
	}

	// In the language.
	FVekaDocumentBuilder Builder;
	FVekaDocumentBuilder::FNode Stack = Builder.Node(TEXT("HStack"));
	FVekaDocumentBuilder::FNode Item = Builder.Node(TEXT("Text"));
	Item.Arg(TEXT("text"), Builder.Lit(TEXT("Tab")));
	Item.Modifier(TEXT("glass"), { { TEXT("cornerRadius"), Builder.Lit(20.0) }, { TEXT("refraction"), Builder.Lit(10.0) } });
	Stack.Children({ Item });
	Stack.Modifier(TEXT("glassGroup"), { { TEXT("smoothness"), Builder.Lit(24.0) } });
	Builder.SetRoot(Stack);
	UVekaView* View = NewObject<UVekaView>();
	View->Document = Builder.Release();
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(View->BuildRoot());
	Host->SlatePrepass(1.f);
	const TSharedPtr<FVekaNode> Root = Host->GetRuntime()->GetRootNode();
	const FVekaGlassGroupModifier* GroupModifier = nullptr;
	const FVekaGlassModifier* GlassModifier = nullptr;
	TFunction<void(const FVekaNode&)> Find = [&](const FVekaNode& N)
	{
		for (const FInstancedStruct& Modifier : N.GetModifiers())
		{
			GroupModifier = GroupModifier ? GroupModifier : Modifier.GetPtr<FVekaGlassGroupModifier>();
			GlassModifier = GlassModifier ? GlassModifier : Modifier.GetPtr<FVekaGlassModifier>();
		}
		for (const TSharedRef<FVekaNode>& Child : N.GetChildren())
		{
			Find(*Child);
		}
	};
	if (TestTrue(TEXT("tree built"), Root.IsValid()))
	{
		Find(*Root);
	}
	TestTrue(TEXT(".glassGroup(smoothness: 24)"), GroupModifier && GroupModifier->Smoothness == 24.f);
	TestTrue(TEXT(".glass(cornerRadius: 20, refraction: 10)"), GlassModifier && GlassModifier->CornerRadii.TopLeft == 20.f && GlassModifier->Material.Refraction == 10.f);
	return true;
}

namespace
{
	/** Renders Root (with Slate, custom elements included) and returns its pixels. */
	TArray<FColor> RenderPixels(const FVekaElement& Root, const FIntPoint Size)
	{
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(Root);
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Nearest, false);
		Target->AddToRoot();
		ON_SCOPE_EXIT { Target->RemoveFromRoot(); };
		FWidgetRenderer Renderer(false, true);
		Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.f);
		Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.f);
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		return Pixels;
	}

	float Channel(uint8 Value) { return Value / 255.f; }

	FVector3f ToVector(const FColor& Color) { return FVector3f(Channel(Color.R), Channel(Color.G), Channel(Color.B)); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaBlendModeTest, "VekaUI.Render.BlendModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaBlendModeTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	// Elements: a blend paints Begin, Between and End around its content (painted twice); Normal / Add: none.
	TestEqual(TEXT("multiply: three steps"), PaintAndCount(Rectangle().Fill(FLinearColor::Red).Frame(40.f, 40.f).BlendMode(EVekaBlendMode::Multiply)).Custom, 3);
	TestEqual(TEXT("normal: nothing"), PaintAndCount(Rectangle().Fill(FLinearColor::Red).Frame(40.f, 40.f).BlendMode(EVekaBlendMode::Normal)).Custom, 0);
	TestEqual(TEXT("add: drawn directly"), PaintAndCount(Rectangle().Fill(FLinearColor::Red).Frame(40.f, 40.f).BlendMode(EVekaBlendMode::Add)).Custom, 0);
	TestEqual(TEXT("liquid distortion: one composite"), PaintAndCount(Text(TEXT("Haze")).Padding(20.f).AddModifier(FInstancedStruct::Make(FVekaLiquidDistortionModifier()))).Custom, 1);

	// Pixels against the W3C formulas, on the stored (display) values: a background, a plain swatch of the source
	// color, then swatches of the same color blended with the background.
	const FLinearColor Back(0.70f, 0.35f, 0.15f);
	const FLinearColor Source(0.20f, 0.55f, 0.80f);
	struct FCase
	{
		EVekaBlendMode Mode;
		TFunction<float(float, float)> Formula;
	};
	const TArray<FCase> Cases = {
		{ EVekaBlendMode::Multiply, [](float B, float S) { return B * S; } },
		{ EVekaBlendMode::Screen, [](float B, float S) { return B + S - B * S; } },
		{ EVekaBlendMode::Darken, [](float B, float S) { return FMath::Min(B, S); } },
		{ EVekaBlendMode::Lighten, [](float B, float S) { return FMath::Max(B, S); } },
		{ EVekaBlendMode::Difference, [](float B, float S) { return FMath::Abs(B - S); } },
		{ EVekaBlendMode::Exclusion, [](float B, float S) { return B + S - 2.f * B * S; } },
		{ EVekaBlendMode::Overlay, [](float B, float S) { return B <= 0.5f ? 2.f * B * S : 1.f - 2.f * (1.f - B) * (1.f - S); } },
	};
	TArray<FVekaElement> Swatches = { Rectangle().Fill(Source).Frame(20.f, 20.f) };
	for (const FCase& Case : Cases)
	{
		Swatches.Add(Rectangle().Fill(Source).Frame(20.f, 20.f).BlendMode(Case.Mode));
	}
	const FIntPoint Size(400, 60);
	const TArray<FColor> Pixels = RenderPixels(ZStack(FVekaAlignment::TopLeading())(
		Rectangle().Fill(Back).Frame(400.f, 60.f),
		HStack(10.f).Children(MoveTemp(Swatches)).Padding(FMargin(10.f, 20.f))
	), Size);
	if (!TestEqual(TEXT("pixels read"), Pixels.Num(), Size.X * Size.Y))
	{
		return false;
	}
	auto At = [&](int32 X, int32 Y) { return ToVector(Pixels[Y * Size.X + X]); };
	const FVector3f B = At(5, 5);
	const FVector3f S = At(20, 30);
	TestTrue(FString::Printf(TEXT("background and source differ (%s / %s)"), *B.ToString(), *S.ToString()), !B.Equals(S, 0.05f));
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		const FVector3f Got = At(20 + (Index + 1) * 30, 30);
		const FVector3f Expected(Cases[Index].Formula(B.X, S.X), Cases[Index].Formula(B.Y, S.Y), Cases[Index].Formula(B.Z, S.Z));
		const FString Mode = StaticEnum<EVekaBlendMode>()->GetNameStringByValue(static_cast<int64>(Cases[Index].Mode));
		TestTrue(FString::Printf(TEXT("%s: got %s, expected %s (B %s, S %s)"), *Mode, *Got.ToString(), *Expected.ToString(), *B.ToString(), *S.ToString()), Got.Equals(Expected, 3.f / 255.f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaOverlapLayersTest, "VekaUI.Render.OverlapLayers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaOverlapLayersTest::RunTest(const FString& Parameters)
{
	// Overlapping children paint above each other: a backdrop in the second child of a ZStack comes after every element
	// of the first child (it must see them), even when the first child is deeper.
	using namespace Veka;
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(ZStack()(
		VStack(4.f)(Text(TEXT("A")), VStack(2.f)(Text(TEXT("B")).Padding(4.f).Background(FLinearColor::Red, 4.f))).Padding(20.f),
		Text(TEXT("C")).Padding(30.f).BackdropBlur(10.f)
	));
	const FVector2f Size(300.f, 200.f);
	TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
	FSlateWindowElementList Elements(Window);
	FHittestGrid Grid;
	Grid.SetHittestArea(FVector2f::ZeroVector, Size);
	const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
	Host->SlatePrepass(1.f);
	Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
	int32 BackdropLayer = -1;
	for (const FSlateCustomDrawerElement& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Custom>())
	{
		BackdropLayer = FMath::Max(BackdropLayer, static_cast<int32>(Element.GetLayer()));
	}
	// Layers of the text elements, in paint order (shaped or plain, whichever Slate made).
	TArray<int32> TextLayers;
	for (const auto& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_ShapedText>())
	{
		TextLayers.Add(static_cast<int32>(Element.GetLayer()));
	}
	if (TextLayers.Num() < 3)
	{
		TextLayers.Reset();
		for (const auto& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Text>())
		{
			TextLayers.Add(static_cast<int32>(Element.GetLayer()));
		}
	}
	if (!TestTrue(FString::Printf(TEXT("three texts (%d) and a backdrop (%d)"), TextLayers.Num(), BackdropLayer), TextLayers.Num() >= 3 && BackdropLayer >= 0))
	{
		return false;
	}
	// The first two texts (A, B) belong to the first child.
	TestTrue(FString::Printf(TEXT("backdrop (%d) above A (%d) and B (%d)"), BackdropLayer, TextLayers[0], TextLayers[1]),
		BackdropLayer > TextLayers[0] && BackdropLayer > TextLayers[1]);
	return true;
}

#endif
