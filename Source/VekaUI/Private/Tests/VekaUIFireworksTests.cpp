// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Primitives/VekaFireworks.h"
#include "Core/VekaSchema.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaFireworksTest, "VekaUI.Effects.Fireworks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaFireworksTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	// A schema primitive: `Fireworks(rate:, particles:, ...)` in .vk, in the palette under Effects.
	TestNotNull(TEXT("Fireworks in the schema"), FVekaSchema::Get().Find(TEXT("Fireworks"), EVekaSchemaKind::Primitive));
	FVekaFireworks Defaults;
	TestTrue(TEXT("never takes clicks"), !Defaults.HitTest(FVector2D(10.0, 10.0), FVector2D(100.0, 100.0)));
	TestTrue(TEXT("repaints every frame"), Defaults.CreateState().IsValid() && Defaults.CreateState()->NeedsAnimationFrame(0.0));

	// Pixels: black frame, nothing at first (the first burst comes after a short delay), sparks a moment later.
	FVekaFireworks Primitive;
	Primitive.Rate = 12.f;
	Primitive.Particles = 80;
	Primitive.Radius = 30.f;
	Primitive.Seed = 7;
	const FIntPoint Size(200, 120);
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(ZStack()(Rectangle().Fill(FLinearColor::Black), FVekaElement::Make(Primitive)).Frame(Size.X, Size.Y));
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Nearest, false);
	Target->AddToRoot();
	ON_SCOPE_EXIT { Target->RemoveFromRoot(); };
	FWidgetRenderer Renderer(false, true);

	auto LitPixels = [&]()
	{
		Renderer.DrawWidget(Target, Host, FVector2D(Size), 1.f / 60.f);
		TArray<FColor> Pixels;
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		int32 Lit = 0;
		int32 Colored = 0;
		for (const FColor& Pixel : Pixels)
		{
			const int32 Max = FMath::Max3(Pixel.R, Pixel.G, Pixel.B);
			const int32 Min = FMath::Min3(Pixel.R, Pixel.G, Pixel.B);
			Lit += Max > 60 ? 1 : 0;
			Colored += Max > 60 && Max - Min > 40 ? 1 : 0;
		}
		return TPair<int32, int32>(Lit, Colored);
	};

	const TPair<int32, int32> First = LitPixels();
	TestTrue(FString::Printf(TEXT("nothing at first (%d lit pixels)"), First.Key), First.Key < 10);
	int32 BestLit = 0;
	int32 BestColored = 0;
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		FPlatformProcess::Sleep(0.05f);
		const TPair<int32, int32> Now = LitPixels();
		BestLit = FMath::Max(BestLit, Now.Key);
		BestColored = FMath::Max(BestColored, Now.Value);
	}
	TestTrue(FString::Printf(TEXT("sparks a moment later (%d lit pixels)"), BestLit), BestLit > 60);
	TestTrue(FString::Printf(TEXT("colored sparks, not only the white flash (%d)"), BestColored), BestColored > 20);
	return true;
}

#endif
