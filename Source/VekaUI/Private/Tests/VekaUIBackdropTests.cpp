// Copyright TheMiniBoss. All Rights Reserved.

// Phase 4.0: the backdrop element (render graph) and the modifier that emits it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaBackdrop.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "StructUtils/InstancedStruct.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaBackdropTest, "VekaUI.Render.Backdrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaBackdropTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a small blur needs one level"), Veka::Render::BlurLevels(1.f), 1);
	TestEqual(TEXT("24 px: four levels"), Veka::Render::BlurLevels(24.f), 4);
	TestEqual(TEXT("levels are capped"), Veka::Render::BlurLevels(5000.f), 7);
	TestTrue(TEXT("an element is made"), Veka::Render::MakeBackdropElement(FVekaBackdropParams()).IsValid());

	// The modifier adds one custom element per paint (B), behind the node's content.
	FVekaBackdropModifier Backdrop;
	Backdrop.Radius = 16.f;
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(Veka::Text(TEXT("Glass")).Padding(20.f).AddModifier(FInstancedStruct::Make(Backdrop)));
	const FVector2f Size(300.f, 200.f);
	TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
	FSlateWindowElementList Elements(Window);
	FHittestGrid Grid;
	Grid.SetHittestArea(FVector2f::ZeroVector, Size);
	const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
	Host->SlatePrepass(1.f);
	Host->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
	int32 Custom = 0;
	for (const FSlateCustomDrawerElement& Element : Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Custom>())
	{
		Custom += Element.CustomDrawer.IsValid() ? 1 : 0;
	}
	TestEqual(TEXT("one live custom element (kept until rendered)"), Custom, 1);
	return true;
}

#endif
