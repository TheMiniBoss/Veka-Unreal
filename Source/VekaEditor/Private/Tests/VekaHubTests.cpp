// Copyright TheMiniBoss. All Rights Reserved.

// Phase 4d: the Veka Hub (DesignLab).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Hub/VekaHub.h"
#include "Slate/SVekaHost.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaTheme.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "View/VekaViewAsset.h"
#include "VekaLanguage.h"
#include "Core/VekaRuntime.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "HAL/PlatformProcess.h"
#include "Core/VekaNode.h"
#include "Primitives/VekaCanvas.h"

namespace
{
	struct FHubPaint
	{
		int32 Custom = 0;
		int32 Text = 0;
	};

	FHubPaint PaintHub(Veka::Editor::Hub::ESection Section, TArray<UVekaTheme*> Themes = {})
	{
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(Veka::Editor::Hub::MakeHub(Section, MoveTemp(Themes)));
		const FVector2f Size(1500.f, 1000.f);
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Host->SlatePrepass(1.f);
		Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		FHubPaint Out;
		for (const FSlateCustomDrawerElement& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Custom>())
		{
			Out.Custom += Element.CustomDrawer.IsValid() ? 1 : 0;
		}
		Out.Text = Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Text>().Num() + Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_ShapedText>().Num();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaHubTest, "VekaUI.Editor.Hub", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaHubTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Editor::Hub;
	TestTrue(TEXT("Hub tab registered"), FGlobalTabmanager::Get()->HasTabSpawner(TabId));

	// Every DesignLab section builds and draws (sidebar and content).
	for (int32 Index = 0; Index < static_cast<int32>(ESection::Count); ++Index)
	{
		const ESection Section = static_cast<ESection>(Index);
		const FHubPaint Paint = PaintHub(Section);
		TestTrue(FString::Printf(TEXT("%s draws text (%d)"), *SectionTitle(Section).ToString(), Paint.Text), Paint.Text > 10);
	}

	// Effects: backdrop, glass, distortion (the first one captures for all) and a blend (three steps).
	const FHubPaint Effects = PaintHub(ESection::Effects);
	TestTrue(FString::Printf(TEXT("effects draw their GPU elements (%d)"), Effects.Custom), Effects.Custom >= 6);

	// The project's themes are offered (VT_Alpha13 is in the plugin) and the Hub draws with one.
	const TArray<UVekaTheme*> Themes = GatherThemes();
	TestTrue(TEXT("VT_Alpha13 offered"), Themes.ContainsByPredicate([](const UVekaTheme* Theme) { return Theme && Theme->GetFName() == TEXT("VT_Alpha13"); }));
	TestTrue(TEXT("draws with the themes"), PaintHub(ESection::Colors, Themes).Text > 10);

	// The sidebar's symbols exist.
	for (const TCHAR* Name : { TEXT("veka.logo"), TEXT("slider.horizontal.3"), TEXT("paintpalette"), TEXT("star"), TEXT("textformat.size"), TEXT("wand.and.stars"),
		TEXT("sparkles"), TEXT("drop"), TEXT("square.grid.2x2"), TEXT("book"), TEXT("clock") })
	{
		FVekaSymbolVariant Variant;
		TestNotNull(FString::Printf(TEXT("symbol %s"), Name), Veka::Symbols::Resolve(FName(Name), Variant));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaHubLearnTest, "VekaUI.Editor.HubLearn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaHubLearnTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Editor::Hub;
	// Examples: the starter and the Examples folder; each parses and analyzes without errors.
	const TArray<FExample> Examples = GetExamples();
	TestTrue(FString::Printf(TEXT("examples found (%d)"), Examples.Num()), Examples.Num() >= 4);
	for (const FExample& Example : Examples)
	{
		const FVekaParseResult Result = Veka::Lang::Parse(Example.Source);
		TArray<FString> Problems;
		for (const FVekaDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Problems.Add(Diagnostic.ToString());
		}
		if (!Result.HasErrors())
		{
			for (const FVekaDiagnostic& Diagnostic : Veka::Lang::Analyze(Result.Document))
			{
				Problems.Add(Diagnostic.ToString());
			}
		}
		TestTrue(FString::Printf(TEXT("%s is clean: %s"), *Example.Name.ToString(), *FString::Join(Problems, TEXT(" / "))), Problems.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has a description"), *Example.Name.ToString()), Example.Description.IsEmpty());
	}

	// "Open as asset" makes the view in /Game/VekaExamples (left unsaved), with its source.
	const FExample* Settings = Examples.FindByPredicate([](const FExample& E) { return E.Name == TEXT("VV_ExampleSettings"); });
	if (TestNotNull(TEXT("settings example"), Settings))
	{
		UVekaView* View = OpenExample(*Settings, false);
		if (TestNotNull(TEXT("asset made"), View))
		{
			TestEqual(TEXT("in /Game/VekaExamples"), View->GetPackage()->GetName(), FString(TEXT("/Game/VekaExamples/VV_ExampleSettings")));
			TestTrue(TEXT("document filled"), View->Document.Nodes.Num() > 3);
			TestTrue(TEXT("same asset the second time"), OpenExample(*Settings, false) == View);
			// A copy that no longer matches its example (here: the example changed after the copy was made) is spotted
			// and updated on request, code included.
			TestFalse(TEXT("a fresh copy matches its example"), IsExampleCopyOutdated(View, *Settings));
			View->Document = Veka::Lang::Parse(TEXT("view VV_ExampleSettings {\n    Text(\"Old\")\n}\n")).Document;
			View->SourceText = TEXT("old");
			TestTrue(TEXT("an old copy is spotted"), IsExampleCopyOutdated(View, *Settings));
			TestTrue(TEXT("the copy is updated"), UpdateExampleCopy(View, *Settings));
			TestFalse(TEXT("the updated copy matches"), IsExampleCopyOutdated(View, *Settings));
			TestEqual(TEXT("its code is the example's"), View->SourceText, Settings->Source);
			View->ClearFlags(RF_Public | RF_Standalone);
			View->MarkAsGarbage();
		}
	}

	// Changelog: releases with entries, newest first.
	const TArray<FChangelogRelease> Changelog = GetChangelog();
	TestTrue(FString::Printf(TEXT("changelog releases (%d)"), Changelog.Num()), Changelog.Num() >= 10);
	const int32 Index4e = Changelog.IndexOfByPredicate([](const FChangelogRelease& Release) { return Release.Title.StartsWith(TEXT("4e")); });
	const int32 Index4d = Changelog.IndexOfByPredicate([](const FChangelogRelease& Release) { return Release.Title.StartsWith(TEXT("4d")); });
	TestTrue(TEXT("newest first (4e before 4d)"), Index4e != INDEX_NONE && Index4d != INDEX_NONE && Index4e < Index4d);
	TestTrue(TEXT("the newest release has entries"), Changelog.Num() > 0 && Changelog[0].Entries.Num() >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaFireworksInVekaTest, "VekaUI.Editor.FireworksInVeka", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaFireworksInVekaTest::RunTest(const FString& Parameters)
{
	// The birthday card's fireworks are written in .vk only (time, let, random, sin, cos, hsv): colored sparks show up
	// on the card as time passes, at a cost logged per frame ("VekaUI perf:").
	const TArray<Veka::Editor::Hub::FExample> Examples = Veka::Editor::Hub::GetExamples();
	const Veka::Editor::Hub::FExample* Card = Examples.FindByPredicate([](const Veka::Editor::Hub::FExample& Example) { return Example.Name == TEXT("VV_ExampleBirthdayCard"); });
	if (!TestNotNull(TEXT("birthday card example"), Card))
	{
		return false;
	}
	TestFalse(TEXT("no C++ fireworks component"), Card->Source.Contains(TEXT("Fireworks(")));
	const FVekaParseResult Parsed = Veka::Lang::Parse(Card->Source);
	if (!TestFalse(TEXT("parses"), Parsed.HasErrors()))
	{
		return false;
	}
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(Veka::Interpreter::MakeView(Parsed.Document, MakeShared<FVekaViewStore>()));
	const FIntPoint Size(250, 290);
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Nearest, false);
	Target->AddToRoot();
	ON_SCOPE_EXIT { Target->RemoveFromRoot(); };
	FWidgetRenderer Renderer(false, true);
	Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.f);
	const TSharedRef<FVekaRuntime> Runtime = Host->GetRuntime();

	// Sparks: bright and saturated pixels in the card (white text is not saturated, the wallpaper is dark).
	int32 BestSparks = 0;
	double TickTotal = 0.0;
	int32 Ticks = 0;
	for (int32 Step = 0; Step < 8; ++Step)
	{
		for (int32 Frame = 0; Frame < 15; ++Frame)
		{
			const double Start = FPlatformTime::Seconds();
			Runtime->Tick(1.0 / 60.0);
			TickTotal += FPlatformTime::Seconds() - Start;
			++Ticks;
		}
		Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.25f);
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		int32 Sparks = 0;
		for (int32 Y = 16; Y < 92 && Pixels.Num() == Size.X * Size.Y; ++Y)
		{
			for (int32 X = 12; X < 238; ++X)
			{
				const FLinearColor Hsv = FLinearColor(Pixels[Y * Size.X + X]).LinearRGBToHSV();
				Sparks += Hsv.B > 0.55f && Hsv.G > 0.3f ? 1 : 0;
			}
		}
		BestSparks = FMath::Max(BestSparks, Sparks);
	}
	const double Ms = TickTotal * 1000.0 / FMath::Max(1, Ticks);
	UE_LOG(LogTemp, Display, TEXT("VekaUI perf: fireworks in .vk (~150 nodes re-evaluated): %.3f ms per frame, %d spark pixels at best"), Ms, BestSparks);
	TestTrue(FString::Printf(TEXT("colored sparks on the card (%d pixels)"), BestSparks), BestSparks > 15);
	// Measured 2.3 ms (6.6 before the interpreter and reconciler optimizations): a guard against regressions, not a budget.
	TestTrue(FString::Printf(TEXT("re-evaluated every frame under 4 ms (%.3f ms)"), Ms), Ms < 4.0);
	TestTrue(TEXT("the view keeps evaluating (time)"), Runtime->HasPendingWork());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaCanvasFireworksTest, "VekaUI.Editor.CanvasFireworks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaCanvasFireworksTest::RunTest(const FString& Parameters)
{
	// VV_Fireworks: the SwiftUI Canvas + TimelineView fireworks written in .vk only (Canvas, Dot, Line, Glow, time).
	// The canvas repaints every frame without re-evaluating the view: no runtime work, the cost is in the paint.
	const TArray<Veka::Editor::Hub::FExample> Examples = Veka::Editor::Hub::GetExamples();
	const Veka::Editor::Hub::FExample* Example = Examples.FindByPredicate([](const Veka::Editor::Hub::FExample& Item) { return Item.Name == TEXT("VV_Fireworks"); });
	if (!TestNotNull(TEXT("VV_Fireworks example"), Example))
	{
		return false;
	}
	const FVekaParseResult Parsed = Veka::Lang::Parse(Example->Source);
	if (!TestFalse(TEXT("parses"), Parsed.HasErrors()))
	{
		for (const FVekaDiagnostic& Diagnostic : Parsed.Diagnostics)
		{
			AddError(Diagnostic.ToString());
		}
		return false;
	}
	const TArray<FVekaDiagnostic> Problems = Veka::Lang::Analyze(Parsed.Document);
	for (const FVekaDiagnostic& Diagnostic : Problems)
	{
		AddError(Diagnostic.ToString());
	}
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(Veka::Interpreter::MakeView(Parsed.Document, MakeShared<FVekaViewStore>()));
	const FIntPoint Size(372, 132);
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Nearest, false);
	Target->AddToRoot();
	ON_SCOPE_EXIT { Target->RemoveFromRoot(); };
	FWidgetRenderer Renderer(false, true);
	Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.f);
	const TSharedRef<FVekaRuntime> Runtime = Host->GetRuntime();
	Runtime->Tick(1.0 / 60.0);
	TestFalse(TEXT("no view re-evaluation for the canvas (it only repaints)"), Runtime->HasPendingWork());
	// On screen, the canvas asks for a frame every frame (its widget then repaints itself, like an animated symbol).
	TFunction<bool(const FVekaNode&)> Animates = [&Animates](const FVekaNode& Node)
	{
		if (Node.GetPrimitiveAs<FVekaCanvas>() && Node.GetPrimitiveState() && Node.GetPrimitiveState()->NeedsAnimationFrame(FPlatformTime::Seconds()))
		{
			return true;
		}
		for (const TSharedRef<FVekaNode>& Child : Node.GetChildren())
		{
			if (Animates(*Child))
			{
				return true;
			}
		}
		return false;
	};
	TestTrue(TEXT("the canvas asks for animation frames"), Runtime->GetRootNode().IsValid() && Animates(*Runtime->GetRootNode()));

	// Lit sparks over 2.5 s of real time (the canvas counts time from its store).
	int32 BestSparks = 0;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		FPlatformProcess::Sleep(0.25f);
		Renderer.DrawWidget(Target, Host, FVector2D(Size), 0.25f);
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		int32 Sparks = 0;
		for (const FColor& Pixel : Pixels)
		{
			Sparks += FMath::Max3(Pixel.R, Pixel.G, Pixel.B) > 80 ? 1 : 0;
		}
		if (Sparks > BestSparks && Pixels.Num() == Size.X * Size.Y)
		{
			// The brightest frame, kept to look at: Saved/VekaUI/CanvasFireworks.png.
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("VekaUI/CanvasFireworks.png")));
		}
		BestSparks = FMath::Max(BestSparks, Sparks);
	}
	TestTrue(FString::Printf(TEXT("sparks drawn (%d lit pixels)"), BestSparks), BestSparks > 40);

	// The paint cost of a frame (Slate prepass + paint of the host with ~1000 marks).
	TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2f(Size)).CreateTitleBar(false);
	FHittestGrid Grid;
	Grid.SetHittestArea(FVector2f::ZeroVector, FVector2f(Size));
	double Total = 0.0;
	int32 Marks = 0;
	constexpr int32 Frames = 30;
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		FSlateWindowElementList Elements(Window);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		const double Start = FPlatformTime::Seconds();
		Host->SlatePrepass(1.f);
		Host->Paint(Args, FGeometry::MakeRoot(FVector2f(Size), FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		Total += FPlatformTime::Seconds() - Start;
		Marks = FMath::Max(Marks, Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_RoundedBox>().Num());
	}
	// Additive dots are sent with a tiny alpha: Slate drops boxes with a fully transparent tint (they did not draw at all).
	TestTrue(FString::Printf(TEXT("dots reach the paint (%d rounded boxes at best)"), Marks), Marks > 100);
	UE_LOG(LogTemp, Display, TEXT("VekaUI perf: canvas fireworks in .vk (6 bursts x 180 sparks): %.3f ms paint per frame, %d dots, 0 view evaluations, %d lit pixels at best"),
		Total * 1000.0 / Frames, Marks, BestSparks);
	return true;
}

#endif
