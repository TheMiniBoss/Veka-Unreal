// Copyright TheMiniBoss. All Rights Reserved.

// Tests of the Slate path (paint + pointer input) and of the UMG bridge. They paint into an off-screen element list
// and call the widget input handlers directly, so they run headless (-nullrhi) and need no OS window.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Slate/SVekaNodeWidget.h"
#include "Samples/VekaPlayground.h"
#include "UMG/VekaHostWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "InputCoreTypes.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaSlateTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/** Off-screen paint of a widget tree: an unregistered window only owns the element list. */
	struct FVekaOffscreenPaint
	{
		TSharedRef<SWindow> Window;
		FSlateWindowElementList Elements;
		FHittestGrid HittestGrid;
		int32 MaxLayer = 0;

		FVekaOffscreenPaint(const TSharedRef<SWidget>& Widget, const FVector2f& Size)
			: Window(SNew(SWindow).ClientSize(Size).CreateTitleBar(false))
			, Elements(Window)
		{
			HittestGrid.SetHittestArea(FVector2f::ZeroVector, Size);
			const FPaintArgs Args(nullptr, HittestGrid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
			Widget->SlatePrepass(1.f);
			MaxLayer = Widget->Paint(Args, Geometry, FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}

		const FSlateDrawElementArray<FSlateRoundedBoxElement>& RoundedBoxes() const
		{
			return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_RoundedBox>();
		}
	};

	FPointerEvent LeftMouse(const FVector2f& Position, bool bButtonDown)
	{
		TSet<FKey> Pressed;
		if (bButtonDown)
		{
			Pressed.Add(EKeys::LeftMouseButton);
		}
		return FPointerEvent(0, Position, Position, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	TSharedPtr<FVekaNode> FindNode(const TSharedPtr<FVekaNode>& Node, TFunctionRef<bool(const FVekaNode&)> Predicate)
	{
		if (!Node.IsValid())
		{
			return nullptr;
		}
		if (Predicate(*Node))
		{
			return Node;
		}
		for (const TSharedRef<FVekaNode>& Child : Node->GetChildren())
		{
			if (TSharedPtr<FVekaNode> Found = FindNode(Child, Predicate))
			{
				return Found;
			}
		}
		return nullptr;
	}

	TSharedPtr<FVekaNode> FindText(const TSharedPtr<FVekaNode>& Root, TFunctionRef<bool(const FString&)> Predicate)
	{
		return FindNode(Root, [&Predicate](const FVekaNode& Node)
		{
			const FVekaText* Text = Node.GetPrimitiveAs<FVekaText>();
			return Text && Predicate(Text->Text.ToString());
		});
	}

	FString TextOf(const TSharedPtr<FVekaNode>& Node)
	{
		const FVekaText* Text = Node.IsValid() ? Node->GetPrimitiveAs<FVekaText>() : nullptr;
		return Text ? Text->Text.ToString() : FString();
	}

	/** Press + release on a node's own widget, at the center of its arranged rect, as Slate would deliver them. */
	void Tap(FVekaRuntime& Runtime, const TSharedPtr<FVekaNode>& Node, bool bAsDoubleClick)
	{
		const TSharedRef<SVekaNodeWidget> Widget = StaticCastSharedRef<SVekaNodeWidget>(Node->GetOrCreateWidget());
		const FVector2f Size(Node->GetArrangedRect().Size);
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
		const FVector2f Center = Size * 0.5f;

		if (bAsDoubleClick)
		{
			Widget->OnMouseButtonDoubleClick(Geometry, LeftMouse(Center, true));
		}
		else
		{
			Widget->OnMouseButtonDown(Geometry, LeftMouse(Center, true));
		}
		// A frame passes between press and release: the press state re-evaluates the view (scale modifier).
		Runtime.Flush();
		Widget->OnMouseButtonUp(Geometry, LeftMouse(Center, false));
		Runtime.Flush();
	}
}

// Regression (D-002): painting a leaf with modifiers before any OnArrangeChildren read empty modifier rects
// (editor crash when opening the Playground tab).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSlatePaintModifiersTest, "VekaUI.Slate.PaintModifiers", VekaSlateTestFlags)
bool FVekaSlatePaintModifiersTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}

	const FLinearColor Fill(1.f, 0.f, 0.f);
	const FLinearColor Back(0.f, 0.f, 1.f);
	const FLinearColor Edge(0.f, 1.f, 0.f);

	TSharedRef<SVekaHost> Host = SNew(SVekaHost)
		.Root(Veka::RoundedRectangle(4).Fill(Fill).Frame(80, 40)
			.Padding(10)
			.Background(Back, 6)
			.Border(Edge, 2, 6));

	const FVekaOffscreenPaint Paint(Host, FVector2f(200.f, 100.f));
	const auto& Boxes = Paint.RoundedBoxes();
	TestTrue(TEXT("Painted something"), Boxes.Num() >= 2);

	const FSlateRoundedBoxElement* Border = nullptr;
	const FSlateRoundedBoxElement* Background = nullptr;
	for (const FSlateRoundedBoxElement& Box : Boxes)
	{
		if (Box.GetOutlineWeight() > 0.f)
		{
			Border = &Box;
		}
		else if (Box.GetTint().Equals(Back, 1.0e-3f))
		{
			Background = &Box;
		}
	}

	TestNotNull(TEXT("Background drawn"), Background);
	if (TestNotNull(TEXT("Border drawn"), Border))
	{
		// Regression (D-002): the fill of a RoundedBox comes from the element tint; the border must not fill.
		TestEqual(TEXT("Border fill is transparent"), Border->GetTint().A, 0.f, 1.0e-4f);
		TestTrue(TEXT("Border outline color"), Border->GetOutlineColor().Equals(Edge, 1.0e-3f));
		TestEqual(TEXT("Border width"), Border->GetOutlineWeight(), 2.f, 1.0e-4f);
	}
	return true;
}

// The UMG bridge builds the playground and it paints without errors (what the Widget Blueprint Designer does).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaUMGHostTest, "VekaUI.Slate.UMGHostPlayground", VekaSlateTestFlags)
bool FVekaUMGHostTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}

	UVekaHostWidget* HostWidget = NewObject<UVekaHostWidget>(GetTransientPackage());
	HostWidget->ViewClass = UVekaPlaygroundViewSource::StaticClass();
	const TSharedRef<SWidget> Widget = HostWidget->TakeWidget();
	if (!TestEqual(TEXT("UMG widget is a Veka host"), Widget->GetType(), FName(TEXT("SVekaHost"))))
	{
		HostWidget->ReleaseSlateResources(true);
		return false;
	}

	const TSharedRef<SVekaHost> Host = StaticCastSharedRef<SVekaHost>(Widget);
	TSharedPtr<FVekaNode> Root = Host->GetRuntime()->GetRootNode();
	TestTrue(TEXT("Playground root built"), Root.IsValid());

	const FVekaOffscreenPaint Paint(Widget, FVector2f(900.f, 700.f));
	TestTrue(TEXT("Playground painted shapes"), Paint.RoundedBoxes().Num() > 10);
	TestNotNull(TEXT("Counter text present"), FindText(Root, [](const FString& S) { return S.StartsWith(TEXT("Tapped")); }).Get());

	HostWidget->ReleaseSlateResources(true);
	return true;
}

// Regression (D-002): the "+1" button of the playground (onPress + onTap + scale), tapped through its widget.
// Slate delivers the second click of a fast pair as a double-click: it must count as a tap too.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSlateTapTest, "VekaUI.Slate.TapAndDoubleClick", VekaSlateTestFlags)
bool FVekaSlateTapTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}

	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::Samples::Playground());
	FVekaRuntime& Runtime = *Host->GetRuntime();
	const TSharedPtr<FVekaNode> Root = Runtime.GetRootNode();

	// Layout (and modifier rects) come from a real paint pass.
	{
		const FVekaOffscreenPaint Paint(Host, FVector2f(900.f, 700.f));
	}

	const TSharedPtr<FVekaNode> Button = FindText(Root, [](const FString& S) { return S == TEXT("+1"); });
	auto Counter = [&Root]() { return TextOf(FindText(Root, [](const FString& S) { return S.StartsWith(TEXT("Tapped")); })); };
	if (!TestNotNull(TEXT("+1 button found"), Button.Get()))
	{
		return false;
	}
	TestTrue(TEXT("+1 takes input"), Button->WantsInput());
	TestEqual(TEXT("Starts at 0"), Counter(), FString(TEXT("Tapped 0 times")));

	Tap(Runtime, Button, false);
	TestEqual(TEXT("Tap counted"), Counter(), FString(TEXT("Tapped 1 times")));

	Tap(Runtime, Button, true);
	TestEqual(TEXT("Double-click counted as a tap"), Counter(), FString(TEXT("Tapped 2 times")));

	Tap(Runtime, Button, false);
	TestEqual(TEXT("Third tap counted"), Counter(), FString(TEXT("Tapped 3 times")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
