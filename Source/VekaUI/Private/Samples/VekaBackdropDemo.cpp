// Copyright TheMiniBoss. All Rights Reserved.

// `Veka.BackdropDemo` (in PIE or a game) shows backdrops over the game viewport: three panels with different blur
// radii and a frosted glass bar, all sharing one capture. `Veka.BackdropDemo full` covers the whole viewport (to measure
// the full-screen cost with `stat gpu`: "Veka Backdrop"). Run it again to remove it.

#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Core/VekaPaint.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TWeakPtr<SVekaHost> GDemoHost;

	FVekaElement Panel(const FString& Title, const FString& Subtitle, float Radius)
	{
		using namespace Veka;
		return VStack(6.f, EVekaHAlign::Leading)(
			Text(Title).Font(EVekaTextStyle::Title2),
			Text(Subtitle).Font(EVekaTextStyle::Footnote).Opacity(0.75f),
			Spacer(),
			HStack(8.f)(Circle().Fill(FVekaPaint::FromToken(TEXT("Accent"))).Frame(18.f, 18.f), Capsule().Fill(FVekaPaint::FromToken(TEXT("Fill"))).Frame(120.f, 8.f))
		)
		.Padding(24.f)
		.Frame(300.0, 200.0)
		.BackdropBlur(Radius, FVekaCornerRadii(28.f));
	}

	void ToggleDemo(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogTemp, Warning, TEXT("Veka.BackdropDemo: no game viewport (run it in PIE or in a game)"));
			return;
		}
		if (const TSharedPtr<SVekaHost> Existing = GDemoHost.Pin())
		{
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			GDemoHost.Reset();
			return;
		}
		using namespace Veka;
		const bool bFull = Args.Contains(TEXT("full"));
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		if (bFull)
		{
			Host->SetRoot(Text(TEXT("Full-screen backdrop")).Font(EVekaTextStyle::LargeTitle)
				.FlexFrame(0.0, TNumericLimits<double>::Max(), 0.0, TNumericLimits<double>::Max()).BackdropBlur(24.f));
		}
		else
		{
			Host->SetRoot(VStack(32.f, EVekaHAlign::Leading)(
				HStack(32.f)(
					Panel(TEXT("Radius 6"), TEXT("backdropBlur(6)"), 6.f),
					Panel(TEXT("Radius 24"), TEXT("backdropBlur(24)"), 24.f),
					Panel(TEXT("Radius 60"), TEXT("backdropBlur(60)"), 60.f)
				),
				HStack(16.f)(
					Symbol(TEXT("house.fill")).Font(EVekaTextStyle::Title2),
					Symbol(TEXT("magnifyingglass")).Font(EVekaTextStyle::Title2),
					Symbol(TEXT("person.circle")).Font(EVekaTextStyle::Title2),
					Text(TEXT("frostedGlass · one capture for the four"))
				)
				.Padding(FMargin(28.f, 16.f))
				.FrostedGlass(FVekaCornerRadii(32.f))
			).Padding(64.f));
		}
		Viewport->AddViewportWidgetContent(Host, 100);
		GDemoHost = Host;
	}

	FAutoConsoleCommand GDemoCommand(TEXT("Veka.BackdropDemo"), TEXT("Toggles the backdrop demo over the game viewport (Veka.BackdropDemo [full])."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleDemo));
}

// `Veka.GlassDemo` (PIE or game): liquid glass over the game viewport. A large glass panel (refraction at the edge,
// color split, rim lit from the pointer) and a glass group where a pill and a drop melt together.
namespace
{
	TWeakPtr<SVekaHost> GGlassHost;

	void ToggleGlassDemo(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogTemp, Warning, TEXT("Veka.GlassDemo: no game viewport (run it in PIE or in a game)"));
			return;
		}
		if (const TSharedPtr<SVekaHost> Existing = GGlassHost.Pin())
		{
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			GGlassHost.Reset();
			return;
		}
		using namespace Veka;
		const float Gap = Args.IsEmpty() ? 150.f : FCString::Atof(*Args[0]);
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(VStack(40.f, EVekaHAlign::Leading)(
			VStack(8.f, EVekaHAlign::Leading)(
				Text(TEXT("Liquid Glass")).Font(EVekaTextStyle::LargeTitle),
				Text(TEXT("refraction · aberration · rim lit from the pointer")).Font(EVekaTextStyle::Body).Opacity(0.8f)
			)
			.Padding(36.f)
			.Frame(560.0, 260.0)
			.Glass(FVekaCornerRadii(44.f)),
			ZStack(FVekaAlignment::Leading())(
				HStack(22.f)(
					Symbol(TEXT("house.fill")).Font(EVekaTextStyle::Title2),
					Symbol(TEXT("magnifyingglass")).Font(EVekaTextStyle::Title2),
					Symbol(TEXT("person.circle")).Font(EVekaTextStyle::Title2)
				)
				.Padding(FMargin(28.f, 0.f))
				.Frame(240.0, 68.0)
				.Glass(FVekaCornerRadii(34.f)),
				Symbol(TEXT("plus")).Font(EVekaTextStyle::Title2)
					.Frame(68.0, 68.0)
					.Glass(FVekaCornerRadii(34.f))
					.Offset(FVector2D(240.0 + Gap - 150.0, 0.0))
			)
			.GlassGroup(28.f)
		).Padding(80.f));
		Viewport->AddViewportWidgetContent(Host, 100);
		GGlassHost = Host;
	}

	FAutoConsoleCommand GGlassCommand(TEXT("Veka.GlassDemo"), TEXT("Toggles the liquid glass demo over the game viewport (Veka.GlassDemo [gap])."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleGlassDemo));
}
