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
	TestEqual(TEXT("one backdrop: its composite and the host's capture"), PaintAndCount(One).Custom, 2);
	const FVekaElement Three = VStack(8.f)(
		Text(TEXT("A")).Padding(10.f).BackdropBlur(6.f),
		Text(TEXT("B")).Padding(10.f).BackdropBlur(30.f),
		Text(TEXT("C")).Padding(10.f).FrostedGlass()
	);
	TestEqual(TEXT("three backdrops share one capture"), PaintAndCount(Three).Custom, 4);
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

#endif
