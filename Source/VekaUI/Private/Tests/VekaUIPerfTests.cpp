// Copyright TheMiniBoss. All Rights Reserved.

// Phase 4f: the CPU budgets of VEKAUI_PHASE0.md §13, measured (numbers in the log: "VekaUI perf:").
//  - a screen of 500 nodes at rest costs no Veka work (no tick: the host stops ticking; ticks forced anyway are ~free);
//  - 100 simultaneous spring animations < 0.1 ms of Game Thread per frame.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaView.h"
#include "VekaBuilders.h"
#include "Widgets/SWindow.h"
#include "Widgets/SInvalidationPanel.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "VekaUILog.h"

namespace
{
	/** The toggle of the springs view (set by the test). */
	TOptional<TVekaState<bool>> GSpringsToggle;

	class FVekaSpringsView : public FVekaView
	{
		VEKA_VIEW(FVekaSpringsView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> On = Ctx.State<bool>(TEXT("On"), false);
			GSpringsToggle = On;
			TArray<FVekaElement> Bars;
			for (int32 Index = 0; Index < 100; ++Index)
			{
				// Implicit spring: the presented offset follows the state, no Body evaluation per frame.
				Bars.Add(Rectangle().Fill(FLinearColor::White).Frame(10.f, 3.f)
					.Offset(FVector2D(On.Get() ? 200.0 + Index : 0.0, 0.0))
					.Animation(FVekaAnimationSpec::Spring(0.6, 0.8), On.Get()));
			}
			return VStack(1.f, EVekaHAlign::Leading).Children(MoveTemp(Bars));
		}
	};

	double PaintMs(const TSharedRef<SWidget>& Host, const FVector2f& Size, int32 Count)
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		double Total = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FSlateWindowElementList Elements(Window);
			const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			const double Start = FPlatformTime::Seconds();
			Host->SlatePrepass(1.f);
			Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
			Total += FPlatformTime::Seconds() - Start;
		}
		return Total * 1000.0 / Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaPerfBudgetTest, "VekaUI.Perf.Budgets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaPerfBudgetTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	// 1. A screen of 500 nodes at rest.
	{
		TArray<FVekaElement> Rows;
		for (int32 Index = 0; Index < 500; ++Index)
		{
			Rows.Add(Text(FString::Printf(TEXT("Row %d"), Index)).Font(EVekaTextStyle::Footnote));
		}
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(Grid({ FVekaGridItem::Adaptive(90.f) }, 4.f, 2.f).Children(MoveTemp(Rows)));
		const FVector2f Size(1600.f, 1000.f);
		PaintMs(Host, Size, 2); // builds and warms the caches
		const TSharedRef<FVekaRuntime> Runtime = Host->GetRuntime();
		Runtime->Tick(1.0 / 60.0);
		TestFalse(TEXT("500 nodes at rest: no pending work (the host stops ticking)"), Runtime->HasPendingWork());
		const double Start = FPlatformTime::Seconds();
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			Runtime->Tick(1.0 / 60.0);
		}
		const double TickMs = (FPlatformTime::Seconds() - Start) * 1000.0 / 600.0;
		const double Paint = PaintMs(Host, Size, 30);
		// The same host cached by Slate (invalidation panel, as with global invalidation): nothing changed, no repaint.
		const TSharedRef<SInvalidationPanel> Cached = SNew(SInvalidationPanel)[Host];
		PaintMs(Cached, Size, 2);
		const double CachedPaint = PaintMs(Cached, Size, 30);
		UE_LOG(LogVekaUI, Display, TEXT("VekaUI perf: 500 nodes at rest: tick %.4f ms (forced; 0 in practice), paint %.3f ms per frame, %.3f ms in an invalidation panel"), TickMs, Paint, CachedPaint);
		TestTrue(FString::Printf(TEXT("ticks at rest are free (%.4f ms)"), TickMs), TickMs < 0.01);
	}

	// 2. 100 simultaneous springs.
	{
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(View<FVekaSpringsView>());
		const FVector2f Size(800.f, 600.f);
		PaintMs(Host, Size, 2);
		const TSharedRef<FVekaRuntime> Runtime = Host->GetRuntime();
		if (!TestTrue(TEXT("springs view built"), GSpringsToggle.IsSet()))
		{
			return false;
		}
		GSpringsToggle->Set(true);
		Runtime->Tick(0.0);
		PaintMs(Host, Size, 1);
		const int32 Animations = Runtime->GetScheduler().Num();
		TestTrue(FString::Printf(TEXT("100 springs running (%d animations)"), Animations), Animations >= 100);
		double Total = 0.0;
		double Worst = 0.0;
		int32 Frames = 0;
		while (Runtime->GetScheduler().IsAnimating() && Frames < 240)
		{
			const double Start = FPlatformTime::Seconds();
			Runtime->Tick(1.0 / 60.0);
			const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
			Total += Ms;
			Worst = FMath::Max(Worst, Ms);
			++Frames;
		}
		const double Average = Frames > 0 ? Total / Frames : 0.0;
		UE_LOG(LogVekaUI, Display, TEXT("VekaUI perf: %d springs: %.4f ms average, %.4f ms worst per frame over %d frames"), Animations, Average, Worst, Frames);
		TestTrue(TEXT("the springs settle"), Frames > 5 && !Runtime->GetScheduler().IsAnimating());
		TestTrue(FString::Printf(TEXT("100 springs < 0.1 ms per frame (%.4f ms)"), Average), Average < 0.1);
		GSpringsToggle.Reset();
	}
	return true;
}

#endif
