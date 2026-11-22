// Copyright TheMiniBoss. All Rights Reserved.

// `Veka.BackdropDemo` (in PIE or a game) shows backdrops over the game viewport: three panels with different blur
// radii and a frosted glass bar, all sharing one capture. `Veka.BackdropDemo full` covers the whole viewport (to measure
// the full-screen cost with `stat gpu`: "Veka Backdrop"). Run it again to remove it.

#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Core/VekaPaint.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "StructUtils/InstancedStruct.h"
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
					Panel(TEXT("Radius 60"), TEXT("backdropBlur(60)"), 60.f),
					VStack(6.f, EVekaHAlign::Leading)(
						Text(TEXT("Distortion")).Font(EVekaTextStyle::Title2),
						Text(TEXT("liquidDistortion(14)")).Font(EVekaTextStyle::Footnote).Opacity(0.75f)
					)
					.Padding(24.f)
					.Frame(300.0, 200.0, FVekaAlignment::TopLeading())
					.AddModifier(FInstancedStruct::Make([]() { FVekaLiquidDistortionModifier M; M.Strength = 14.f; M.Scale = 140.f; M.CornerRadii = FVekaCornerRadii(28.f); return M; }()))
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
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		if (Args.Contains(TEXT("full")))
		{
			// One glass over the whole viewport (§13 budget: full-screen glass < 1 ms GPU in High).
			Host->SetRoot(Text(TEXT("Full-screen glass")).Font(EVekaTextStyle::LargeTitle)
				.FlexFrame(0.0, TNumericLimits<double>::Max(), 0.0, TNumericLimits<double>::Max()).Glass(FVekaCornerRadii(40.f)));
			Viewport->AddViewportWidgetContent(Host, 100);
			GGlassHost = Host;
			return;
		}
		const float Gap = Args.IsEmpty() ? 150.f : FCString::Atof(*Args[0]);
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

	FAutoConsoleCommand GGlassCommand(TEXT("Veka.GlassDemo"), TEXT("Toggles the liquid glass demo over the game viewport (Veka.GlassDemo [gap] | full)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleGlassDemo));
}

// `Veka.BlendDemo` (PIE or game): a swatch of each blend mode over the game viewport, with its name.
namespace
{
	TWeakPtr<SVekaHost> GBlendHost;

	void ToggleBlendDemo(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			return;
		}
		if (const TSharedPtr<SVekaHost> Existing = GBlendHost.Pin())
		{
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			GBlendHost.Reset();
			return;
		}
		using namespace Veka;
		TArray<FVekaElement> Swatches;
		const UEnum* Modes = StaticEnum<EVekaBlendMode>();
		for (int32 Index = 0; Index < Modes->NumEnums() - 1; ++Index)
		{
			const EVekaBlendMode Mode = static_cast<EVekaBlendMode>(Modes->GetValueByIndex(Index));
			Swatches.Add(VStack(6.f)(
				ZStack()(
					Circle().Fill(FLinearColor(0.95f, 0.25f, 0.2f)).Frame(70.f, 70.f).BlendMode(Mode).Offset(FVector2D(-14.0, 0.0)),
					Circle().Fill(FLinearColor(0.15f, 0.45f, 0.95f)).Frame(70.f, 70.f).BlendMode(Mode).Offset(FVector2D(14.0, 0.0))
				).Frame(110.0, 80.0),
				Text(Modes->GetNameStringByIndex(Index)).Font(EVekaTextStyle::Footnote)
			));
		}
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		TArray<FVekaElement> Rows;
		for (int32 Start = 0; Start < Swatches.Num(); Start += 9)
		{
			TArray<FVekaElement> Row;
			for (int32 Index = Start; Index < FMath::Min(Start + 9, Swatches.Num()); ++Index)
			{
				Row.Add(Swatches[Index]);
			}
			Rows.Add(HStack(12.f).Children(MoveTemp(Row)));
		}
		Host->SetRoot(VStack(24.f, EVekaHAlign::Leading).Children(MoveTemp(Rows)).Padding(60.f));
		Viewport->AddViewportWidgetContent(Host, 100);
		GBlendHost = Host;
	}

	FAutoConsoleCommand GBlendCommand(TEXT("Veka.BlendDemo"), TEXT("Toggles the blend modes demo over the game viewport."), FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleBlendDemo));
}
