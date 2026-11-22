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

#endif
