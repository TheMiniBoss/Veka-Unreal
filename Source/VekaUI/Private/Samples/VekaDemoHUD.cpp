// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaDemoHUD.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaView.h"
#include "Core/VekaPaint.h"
#include "VekaBuilders.h"
#include "Controls/VekaControls.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	/** The HUD's animated state, driven by a ticker (a demo has no gameplay to drive it). */
	TOptional<TVekaState<int32>> GHudStep;
	FTSTicker::FDelegateHandle GHudTicker;

	void StartTicker()
	{
		if (!GHudTicker.IsValid())
		{
			GHudTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				if (GHudStep.IsSet())
				{
					GHudStep->Set(GHudStep->Get() + 1);
				}
				return true;
			}), 1.6f);
		}
	}

	void StopTicker()
	{
		if (GHudTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GHudTicker);
			GHudTicker.Reset();
		}
		GHudStep.Reset();
	}

	FVekaPaint Token(const TCHAR* Name, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary)
	{
		return FVekaPaint::FromToken(FName(Name), Hierarchy);
	}

	class FVekaLiquidGlassHudView : public FVekaView
	{
		VEKA_VIEW(FVekaLiquidGlassHudView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Step = Ctx.State<int32>(TEXT("Step"), 0);
			GHudStep = Step;
			const int32 Tab = Step.Get() % 4;
			const bool bDropAttached = (Step.Get() / 2) % 2 == 0;

			// Top left: status.
			const FVekaElement Status = HStack(14.f)(
				Symbol(TEXT("heart.fill")).Font(EVekaTextStyle::Title3).Foreground(Token(TEXT("Red"))),
				VStack(4.f, EVekaHAlign::Leading)(
					Capsule().Fill(Token(TEXT("Green"))).Frame(132.f, 6.f).Background(Token(TEXT("Fill")), 3.f),
					Capsule().Fill(Token(TEXT("Cyan"))).Frame(96.f, 6.f)
				),
				Text(TEXT("72")).Font(EVekaTextStyle::Title3).MonospacedDigit()
			)
			.Padding(FMargin(20.f, 12.f))
			.Glass(FVekaCornerRadii(26.f));

			// Top center: compass.
			TArray<FVekaElement> Marks;
			for (const TCHAR* Mark : { TEXT("W"), TEXT("·"), TEXT("NW"), TEXT("·"), TEXT("N"), TEXT("·"), TEXT("NE"), TEXT("·"), TEXT("E") })
			{
				Marks.Add(Text(Mark).Font(EVekaTextStyle::Footnote).FontWeight(EVekaFontWeight::SemiBold).Frame(32.f, 18.f));
			}
			const FVekaElement Compass = ZStack(FVekaAlignment::Top())(
				HStack(0.f).Children(MoveTemp(Marks)).Padding(FMargin(14.f, 10.f)),
				Capsule().Fill(Token(TEXT("Orange"))).Frame(3.f, 10.f)
			)
			.Glass(FVekaCornerRadii(19.f));

			// Top right: minimap.
			const FVekaElement Minimap = ZStack()(
				Circle().Stroke(Token(TEXT("Foreground"), EVekaHierarchy::Tertiary), 1.f).Frame(150.f, 150.f),
				Circle().Stroke(Token(TEXT("Foreground"), EVekaHierarchy::Quaternary), 1.f).Frame(90.f, 90.f),
				Circle().Fill(Token(TEXT("Red"))).Frame(8.f, 8.f).Offset(FVector2D(34.0, -22.0)),
				Circle().Fill(Token(TEXT("Yellow"))).Frame(8.f, 8.f).Offset(FVector2D(-40.0, 30.0)),
				Symbol(TEXT("location.fill")).Font(EVekaTextStyle::Headline).Foreground(Token(TEXT("Accent")))
			)
			.Frame(170.f, 170.f)
			.FrostedGlass(FVekaCornerRadii(85.f));

			// Right: objective.
			const FVekaElement Objective = VStack(8.f, EVekaHAlign::Leading)(
				HStack(8.f)(Symbol(TEXT("flag.fill")).Foreground(Token(TEXT("Yellow"))), Text(TEXT("Objective")).Font(EVekaTextStyle::Headline)),
				Text(TEXT("Reach the lighthouse before dawn")).Font(EVekaTextStyle::Subheadline),
				ProgressView(0.4f).Frame(240.f, -1.f)
			)
			.Padding(18.f)
			.FrostedGlass(FVekaCornerRadii(18.f));

			// Bottom center: tab bar (the selection springs from tab to tab) and a drop that joins and leaves it.
			const TCHAR* Icons[] = { TEXT("house.fill"), TEXT("map.fill"), TEXT("bag.fill"), TEXT("gearshape.fill") };
			TArray<FVekaElement> Tabs;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Tabs.Add(Symbol(Icons[Index]).Font(EVekaTextStyle::Title2).Foreground(Index == Tab ? Token(TEXT("Accent")) : Token(TEXT("Foreground"))).Frame(72.f, 56.f));
			}
			const FVekaElement TabBar = ZStack(FVekaAlignment::Leading())(
				ZStack(FVekaAlignment::Leading())(
					Capsule().Fill(Token(TEXT("Foreground"), EVekaHierarchy::Quaternary)).Frame(72.f, 48.f)
						.Offset(FVector2D(8.0 + Tab * 76.0, 0.0)).Animation(FVekaAnimationSpec::Spring(0.45, 0.72), Tab),
					HStack(4.f).Children(MoveTemp(Tabs)).Padding(FMargin(8.f, 4.f))
				).Glass(FVekaCornerRadii(32.f)),
				Symbol(TEXT("plus")).Font(EVekaTextStyle::Title2).Frame(60.f, 60.f).Glass(FVekaCornerRadii(30.f))
					.Offset(FVector2D(bDropAttached ? 326.0 : 352.0, 0.0)).Animation(FVekaAnimationSpec::Spring(0.7, 0.65), bDropAttached)
			)
			.GlassGroup(30.f)
			.Frame(430.f, 64.f, FVekaAlignment::Leading());

			// Bottom left: ammo.
			const FVekaElement Ammo = HStack(10.f)(
				Symbol(TEXT("bolt.fill")).Foreground(Token(TEXT("Yellow"))),
				Text(TEXT("24 / 120")).Font(EVekaTextStyle::Title3).MonospacedDigit()
			)
			.Padding(FMargin(18.f, 10.f))
			.Glass(FVekaCornerRadii(22.f));

			return VStack(0.f)(
				HStack(0.f, EVekaVAlign::Top)(Status, Spacer(), Compass, Spacer(), Minimap),
				HStack(0.f)(Spacer(), Objective).Padding(FMargin(0.f, 24.f, 0.f, 0.f)),
				Spacer(),
				HStack(0.f, EVekaVAlign::Bottom)(Ammo, Spacer(), TabBar, Spacer(), Spacer(120.f))
			)
			.Padding(36.f)
			.Foreground(FLinearColor::White)
			.ColorScheme(EVekaColorScheme::Dark)
			.FlexFrame(0.0, Infinity, 0.0, Infinity);
		}
	};

	TWeakPtr<SVekaHost> GConsoleHud;

	void ToggleHud(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogTemp, Warning, TEXT("Veka.HudDemo: no game viewport (run it in PIE or in a game)"));
			return;
		}
		if (const TSharedPtr<SVekaHost> Existing = GConsoleHud.Pin())
		{
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			GConsoleHud.Reset();
			StopTicker();
			return;
		}
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::Samples::LiquidGlassHud());
		Viewport->AddViewportWidgetContent(Host, 100);
		GConsoleHud = Host;
		StartTicker();
	}

	FAutoConsoleCommand GHudCommand(TEXT("Veka.HudDemo"), TEXT("Toggles the Liquid Glass HUD demo over the game viewport."), FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleHud));

	// `-VekaHudDemo` on the command line: the HUD over every game map. Works in Shipping, where the console is gone and
	// the command line's map URL (so `?game=`) is ignored.
	FDelayedAutoRegisterHelper GHudSwitch(EDelayedRegisterRunPhase::ObjectSystemReady, []
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("VekaHudDemo")))
		{
			return;
		}
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([](UWorld* World)
		{
			UGameViewportClient* Viewport = World && World->IsGameWorld() ? World->GetGameViewport() : nullptr;
			if (!Viewport || GConsoleHud.IsValid() || Cast<AVekaDemoGameMode>(World->GetAuthGameMode()))
			{
				return; // no viewport, already shown, or the demo game mode's HUD shows it
			}
			const TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::Samples::LiquidGlassHud());
			Viewport->AddViewportWidgetContent(Host, 100);
			GConsoleHud = Host;
			StartTicker();
		});
	});
}

namespace Veka::Samples
{
	FVekaElement LiquidGlassHud()
	{
		return Veka::View<FVekaLiquidGlassHudView>();
	}
}

void AVekaDemoHUD::BeginPlay()
{
	Super::BeginPlay();
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Host = SNew(SVekaHost).Root(Veka::Samples::LiquidGlassHud());
		Viewport->AddViewportWidgetContent(Host.ToSharedRef(), 100);
		StartTicker();
	}
}

void AVekaDemoHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Host.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Host.ToSharedRef());
		}
		Host.Reset();
		StopTicker();
	}
	Super::EndPlay(Reason);
}

AVekaDemoGameMode::AVekaDemoGameMode()
{
	HUDClass = AVekaDemoHUD::StaticClass();
}
