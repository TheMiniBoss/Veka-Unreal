// Copyright TheMiniBoss. All Rights Reserved.

// Phase 4 spike: `Veka.BackdropSpike` (in PIE or a game) shows the two backdrop methods side by side over the game
// viewport. Run it again to remove them. `stat gpu` / `ProfileGPU` show their cost ("Veka Backdrop" for B).

#include "Modifiers/VekaEffectModifiers.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "StructUtils/InstancedStruct.h"

namespace
{
	TWeakPtr<SVekaHost> GSpikeHost;

	FVekaElement Panel(const TCHAR* Title, const TCHAR* Subtitle, EVekaBackdropMethod Method, float Radius)
	{
		using namespace Veka;
		FVekaBackdropModifier Backdrop;
		Backdrop.Radius = Radius;
		Backdrop.CornerRadii = FVekaCornerRadii(28.f);
		Backdrop.Method = Method;
		return VStack(6.f, EVekaHAlign::Leading)(
			Text(FString(Title)).Font(EVekaTextStyle::Title2).Foreground(FLinearColor::White),
			Text(FString(Subtitle)).Font(EVekaTextStyle::Footnote).Foreground(FLinearColor(1.f, 1.f, 1.f, 0.75f)),
			Spacer(),
			HStack(8.f)(Circle().Fill(FLinearColor(1.f, 0.35f, 0.3f)).Frame(18.f, 18.f), Capsule().Fill(FLinearColor(1.f, 1.f, 1.f, 0.6f)).Frame(120.f, 8.f))
		)
		.Padding(24.f)
		.Frame(380.0, 240.0)
		.AddModifier(FInstancedStruct::Make(Backdrop));
	}

	void ToggleSpike(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogTemp, Warning, TEXT("Veka.BackdropSpike: no game viewport (run it in PIE or in a game)"));
			return;
		}
		if (const TSharedPtr<SVekaHost> Existing = GSpikeHost.Pin())
		{
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			GSpikeHost.Reset();
			return;
		}
		const float Radius = Args.IsEmpty() ? 24.f : FCString::Atof(*Args[0]);
		const bool bFull = Args.Num() > 1 && Args[1] == TEXT("full");
		using namespace Veka;
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		if (bFull)
		{
			// The whole viewport behind one backdrop (B), to measure the full-screen cost (§13 budget: glass < 1 ms GPU).
			FVekaBackdropModifier Backdrop;
			Backdrop.Radius = Radius;
			Host->SetRoot(Text(TEXT("Full-screen backdrop (B)")).Font(EVekaTextStyle::LargeTitle).Foreground(FLinearColor::White)
				.FlexFrame(0.0, TNumericLimits<double>::Max(), 0.0, TNumericLimits<double>::Max()).AddModifier(FInstancedStruct::Make(Backdrop)));
			Viewport->AddViewportWidgetContent(Host, 100);
			GSpikeHost = Host;
			return;
		}
		Host->SetRoot(HStack(48.f)(
			Panel(TEXT("A · Engine post buffer"), TEXT("SPostBufferUpdate + Gaussian processor"), EVekaBackdropMethod::EnginePostBuffer, Radius),
			Panel(TEXT("B · Render graph"), TEXT("Custom element, dual Kawase, SDF composite"), EVekaBackdropMethod::RenderGraph, Radius)
		).Padding(64.f));
		Viewport->AddViewportWidgetContent(Host, 100);
		GSpikeHost = Host;
	}

	FAutoConsoleCommand GSpikeCommand(TEXT("Veka.BackdropSpike"), TEXT("Toggles the Phase 4 backdrop spike over the game viewport (Veka.BackdropSpike [radius] [full])."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleSpike));
}
