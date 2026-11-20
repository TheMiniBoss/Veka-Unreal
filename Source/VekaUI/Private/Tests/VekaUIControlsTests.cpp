// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 2e: control styles, color math, and the controls driven through the node widgets' input handlers
// (headless: input handlers are called directly, as in VekaUISlateTests).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Slate/SVekaNodeWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr EAutomationTestFlags VekaControlsTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	FVekaEnvironment ControlEnvOf(const FVekaElement& Element)
	{
		TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
		Runtime->SetRoot(Element);
		return Runtime->GetRootNode()->GetEnvironment();
	}

	struct FControlsPaint
	{
		TSharedRef<SWindow> Window;
		FSlateWindowElementList Elements;
		FHittestGrid HittestGrid;

		FControlsPaint(const TSharedRef<SWidget>& Widget, const FVector2f& Size)
			: Window(SNew(SWindow).ClientSize(Size).CreateTitleBar(false))
			, Elements(Window)
		{
			HittestGrid.SetHittestArea(FVector2f::ZeroVector, Size);
			const FPaintArgs Args(nullptr, HittestGrid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Widget->SlatePrepass(1.f);
			Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}
	};

	FPointerEvent Mouse(const FVector2f& Position, const FKey& Button, bool bDown)
	{
		TSet<FKey> Pressed;
		if (bDown)
		{
			Pressed.Add(Button);
		}
		return FPointerEvent(0, Position, Position, Pressed, Button, 0.f, FModifierKeysState());
	}

	TSharedPtr<FVekaNode> Find(const TSharedPtr<FVekaNode>& Node, TFunctionRef<bool(const FVekaNode&)> Predicate)
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
			if (TSharedPtr<FVekaNode> Found = Find(Child, Predicate))
			{
				return Found;
			}
		}
		return nullptr;
	}

	TSharedPtr<FVekaNode> FindText(const TSharedPtr<FVekaNode>& Root, const FString& Value)
	{
		return Find(Root, [&Value](const FVekaNode& Node)
		{
			const FVekaText* Text = Node.GetPrimitiveAs<FVekaText>();
			return Text && Text->Text.ToString() == Value;
		});
	}

	TSharedPtr<FVekaNode> FindSymbol(const TSharedPtr<FVekaNode>& Root, FName Name)
	{
		return Find(Root, [Name](const FVekaNode& Node)
		{
			const FVekaSymbolView* Symbol = Node.GetPrimitiveAs<FVekaSymbolView>();
			return Symbol && Symbol->Name == Name;
		});
	}

	/** The node itself or the closest ancestor that takes input. */
	TSharedPtr<FVekaNode> Interactive(const TSharedPtr<FVekaNode>& Node)
	{
		for (FVekaNode* Current = Node.Get(); Current; Current = Current->GetParent())
		{
			if (Current->WantsInput() || Current->GetGestures().bFocusable)
			{
				return Current->AsShared();
			}
		}
		return nullptr;
	}

	struct FDriver
	{
		FVekaRuntime& Runtime;

		TSharedRef<SVekaNodeWidget> WidgetOf(const TSharedPtr<FVekaNode>& Node) const
		{
			return StaticCastSharedRef<SVekaNodeWidget>(Node->GetOrCreateWidget());
		}

		FGeometry GeometryOf(const TSharedPtr<FVekaNode>& Node) const
		{
			return FGeometry::MakeRoot(FVector2f(Node->GetArrangedRect().Size), FSlateLayoutTransform());
		}

		void Tap(const TSharedPtr<FVekaNode>& Node, float FractionX = 0.5f) const
		{
			const TSharedRef<SVekaNodeWidget> Widget = WidgetOf(Node);
			const FGeometry Geometry = GeometryOf(Node);
			const FVector2f At(Geometry.GetLocalSize().X * FractionX, Geometry.GetLocalSize().Y * 0.5f);
			Widget->OnMouseButtonDown(Geometry, Mouse(At, EKeys::LeftMouseButton, true));
			Runtime.Flush();
			Widget->OnMouseButtonUp(Geometry, Mouse(At, EKeys::LeftMouseButton, false));
			Runtime.Flush();
		}

		void Drag(const TSharedPtr<FVekaNode>& Node, float FromX, float ToX) const
		{
			const TSharedRef<SVekaNodeWidget> Widget = WidgetOf(Node);
			const FGeometry Geometry = GeometryOf(Node);
			const FVector2f Size = Geometry.GetLocalSize();
			Widget->OnMouseButtonDown(Geometry, Mouse(FVector2f(Size.X * FromX, Size.Y * 0.5f), EKeys::LeftMouseButton, true));
			Runtime.Flush();
			Widget->OnMouseMove(Geometry, Mouse(FVector2f(Size.X * (FromX + ToX) * 0.5f, Size.Y * 0.5f), EKeys::LeftMouseButton, true));
			Widget->OnMouseMove(Geometry, Mouse(FVector2f(Size.X * ToX, Size.Y * 0.5f), EKeys::LeftMouseButton, true));
			Runtime.Flush();
			Widget->OnMouseButtonUp(Geometry, Mouse(FVector2f(Size.X * ToX, Size.Y * 0.5f), EKeys::LeftMouseButton, false));
			Runtime.Flush();
		}

		bool Key(const TSharedPtr<FVekaNode>& Node, const FKey& Key) const
		{
			const FReply Reply = WidgetOf(Node)->OnKeyDown(GeometryOf(Node), FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
			Runtime.Flush();
			return Reply.IsEventHandled();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaControlStylesTest, "VekaUI.Controls.Styles", VekaControlsTestFlags)
bool FVekaControlStylesTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	const FVekaEnvironment Default = VekaEnv::MakeDefault();
	TestFalse(TEXT("No button style by default (bordered)"), VekaEnv::GetControlStyle(Default).ButtonStyle.IsValid());
	TestTrue(TEXT("Enabled by default"), VekaEnv::IsEnabled(Default));

	const FVekaControlStyle Prominent = VekaEnv::GetControlStyle(ControlEnvOf(Text(TEXT("x")).ButtonStyle(EVekaButtonStyleKind::BorderedProminent)));
	TestNotNull(TEXT("Enum style"), Prominent.ButtonStyle.GetPtr<FVekaBorderedProminentButtonStyle>());
	TestNull(TEXT("Not another style"), Prominent.ButtonStyle.GetPtr<FVekaBorderlessButtonStyle>());
	const FVekaControlStyle Custom = VekaEnv::GetControlStyle(ControlEnvOf(Text(TEXT("x")).ButtonStyle(FVekaPlainButtonStyle())));
	TestNotNull(TEXT("Struct style (custom styles path)"), Custom.ButtonStyle.GetPtr<FVekaPlainButtonStyle>());
	TestNotNull(TEXT("Seen as a button style"), Custom.ButtonStyle.GetPtr<FVekaButtonStyle>());

	const FVekaControlStyle Many = VekaEnv::GetControlStyle(ControlEnvOf(Text(TEXT("x"))
		.ToggleStyle(EVekaToggleStyleKind::Checkbox)
		.ControlSize(EVekaControlSize::Large)
		.PickerStyle(EVekaPickerStyle::Radio, EVekaAxis::Horizontal)
		.TextFieldStyle(EVekaTextFieldStyle::Capsule)
		.GaugeStyle(EVekaGaugeStyle::CircularAlt)
		.ProgressViewStyle(EVekaProgressStyle::Lazy)
		.LabelStyle(EVekaLabelStyle::Icon)));
	TestNotNull(TEXT("Toggle style"), Many.ToggleStyle.GetPtr<FVekaCheckboxToggleStyle>());
	TestEqual(TEXT("Control size"), Many.ControlSize, EVekaControlSize::Large);
	TestTrue(TEXT("Radio in a row"), Many.PickerStyle == EVekaPickerStyle::Radio && Many.RadioAxis == EVekaAxis::Horizontal);
	TestEqual(TEXT("Text field"), Many.TextFieldStyle, EVekaTextFieldStyle::Capsule);
	TestEqual(TEXT("Gauge"), Many.GaugeStyle, EVekaGaugeStyle::CircularAlt);
	TestEqual(TEXT("Progress"), Many.ProgressStyle, EVekaProgressStyle::Lazy);
	TestEqual(TEXT("Label"), Many.LabelStyle, EVekaLabelStyle::Icon);

	// Inner style wins.
	const FVekaControlStyle Inner = VekaEnv::GetControlStyle(ControlEnvOf(Text(TEXT("x")).ButtonStyle(EVekaButtonStyleKind::Plain).ButtonStyle(EVekaButtonStyleKind::Bordered)));
	TestNotNull(TEXT("Inner button style wins"), Inner.ButtonStyle.GetPtr<FVekaPlainButtonStyle>());

	TestFalse(TEXT("Disabled"), VekaEnv::IsEnabled(ControlEnvOf(Text(TEXT("x")).Disabled())));
	TestFalse(TEXT("A subtree cannot re-enable what an ancestor disabled"), VekaEnv::IsEnabled(ControlEnvOf(Text(TEXT("x")).Disabled(false).Disabled(true))));
	TestTrue(TEXT("Disabled(false) keeps enabled"), VekaEnv::IsEnabled(ControlEnvOf(Text(TEXT("x")).Disabled(false))));

	float Previous = 0.f;
	bool bGrowing = true;
	for (int32 Size = 0; Size <= static_cast<int32>(EVekaControlSize::Large); ++Size)
	{
		const float Height = Veka::Controls::GetSizeMetrics(static_cast<EVekaControlSize>(Size)).Height;
		bGrowing &= Height > Previous;
		Previous = Height;
	}
	TestTrue(TEXT("Control sizes grow"), bGrowing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaControlColorMathTest, "VekaUI.Controls.ColorMath", VekaControlsTestFlags)
bool FVekaControlColorMathTest::RunTest(const FString& Parameters)
{
	using namespace Veka::ColorMath;
	const FLinearColor Sample = FLinearColor(FColor(51, 153, 204));

	for (int32 Mode = 0; Mode <= static_cast<int32>(EVekaColorPickerMode::CMYK); ++Mode)
	{
		const EVekaColorPickerMode M = static_cast<EVekaColorPickerMode>(Mode);
		const FLinearColor Back = Compose(Decompose(Sample, M), M, Sample);
		const bool bLossy = M == EVekaColorPickerMode::Grayscale;
		if (!bLossy)
		{
			TestTrue(FString::Printf(TEXT("Round trip mode %d"), Mode), Back.Equals(Sample, 0.01f));
		}
		TArray<FText> Names;
		TArray<FVector2f> Ranges;
		GetChannels(M, Names, Ranges);
		TestEqual(FString::Printf(TEXT("Channel count mode %d"), Mode), Names.Num(), Decompose(Sample, M).Num());
	}

	const TArray<float> Red = Decompose(FLinearColor::Red, EVekaColorPickerMode::HSV);
	TestTrue(TEXT("Red: hue 0, full saturation and value"), FMath::IsNearlyZero(Red[0], 0.5f) && FMath::IsNearlyEqual(Red[1], 1.f, 1e-3f) && FMath::IsNearlyEqual(Red[2], 1.f, 1e-3f));
	const TArray<float> Cyan = Decompose(FLinearColor(0.f, 1.f, 1.f), EVekaColorPickerMode::CMYK);
	TestTrue(TEXT("Cyan in CMYK"), FMath::IsNearlyEqual(Cyan[0], 1.f, 1e-3f) && FMath::IsNearlyZero(Cyan[1], 1e-3f) && FMath::IsNearlyZero(Cyan[3], 1e-3f));

	// Single-channel modes keep the other channels of the base color.
	const FLinearColor Shifted = Compose({ 300.f }, EVekaColorPickerMode::Hue, Sample);
	const TArray<float> Before = Decompose(Sample, EVekaColorPickerMode::HSV);
	const TArray<float> After = Decompose(Shifted, EVekaColorPickerMode::HSV);
	TestTrue(TEXT("Hue mode moves the hue only"), FMath::IsNearlyEqual(After[0], 300.f, 0.5f) && FMath::IsNearlyEqual(After[1], Before[1], 0.01f) && FMath::IsNearlyEqual(After[2], Before[2], 0.01f));
	FLinearColor Translucent = Sample;
	Translucent.A = 0.4f;
	TestEqual(TEXT("Alpha kept"), Compose({ 0.5f, 0.5f, 0.5f }, EVekaColorPickerMode::RGB, Translucent).A, 0.4f);
	const FLinearColor Gray = Compose({ 0.5f }, EVekaColorPickerMode::Grayscale, Sample);
	TestTrue(TEXT("Grayscale is neutral"), FMath::IsNearlyEqual(Gray.R, Gray.G, 1e-4f) && FMath::IsNearlyEqual(Gray.G, Gray.B, 1e-4f));

	TestEqual(TEXT("Hex"), ToHex(Sample), FString(TEXT("#3399CC")));
	TestNotEqual(TEXT("Hex differs for another color"), ToHex(FLinearColor::Red), FString(TEXT("#3399CC")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaControlInteractionTest, "VekaUI.Controls.Interaction", VekaControlsTestFlags)
bool FVekaControlInteractionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	using namespace Veka;

	// Values owned by the test; bindings read and write them.
	TSharedRef<int32> Saves = MakeShared<int32>(0);
	TSharedRef<int32> Locked = MakeShared<int32>(0);
	TSharedRef<bool> WiFi = MakeShared<bool>(false);
	TSharedRef<float> Volume = MakeShared<float>(0.f);
	TSharedRef<float> Stepped = MakeShared<float>(0.f);
	TSharedRef<float> Count = MakeShared<float>(9.f);
	TSharedRef<int32> Segment = MakeShared<int32>(0);
	TSharedRef<int32> Radio = MakeShared<int32>(2);
	TSharedRef<FKey> Bound = MakeShared<FKey>(EKeys::E);
	TSharedRef<FString> Name = MakeShared<FString>();
	TSharedRef<double> Price = MakeShared<double>(10.0);
	TSharedRef<double> Share = MakeShared<double>(0.1);
	TSharedRef<int32> Secondary = MakeShared<int32>(0);
	TSharedRef<int32> Taps = MakeShared<int32>(0);
	TSharedRef<int32> Drags = MakeShared<int32>(0);

	auto Bind = [](auto Value)
	{
		using T = typename TRemoveReference<decltype(*Value)>::Type;
		return TVekaBinding<T>([Value]() { return *Value; }, [Value](const T& NewValue) { *Value = NewValue; });
	};

	const FVekaElement Root = VStack(8.f, EVekaHAlign::Leading)(
		Button(INVTEXT("Save"), [Saves]() { ++*Saves; }),
		Button(INVTEXT("Locked"), [Locked]() { ++*Locked; }).Disabled(),
		Toggle(INVTEXT("Wi-Fi"), Bind(WiFi)),
		Slider(Bind(Volume)).Frame(200.f, 24.f),
		Slider(Bind(Stepped), 0.f, 1.f, 0.25f).Frame(200.f, 24.f),
		Stepper(INVTEXT("Count"), Bind(Count), 0.f, 10.f),
		Picker(INVTEXT("Seg"), Bind(Segment), { FVekaPickerOption(INVTEXT("Alpha")), FVekaPickerOption(INVTEXT("Beta")), FVekaPickerOption(INVTEXT("Gamma")) })
			.PickerStyle(EVekaPickerStyle::Segmented),
		Picker(INVTEXT("Radio"), Bind(Radio), { FVekaPickerOption(INVTEXT("One")), FVekaPickerOption(INVTEXT("Two")), FVekaPickerOption(INVTEXT("Three")) }, true)
			.PickerStyle(EVekaPickerStyle::Radio),
		KeyBindButton(Bind(Bound)),
		TextField(INVTEXT("Name"), Bind(Name)),
		NumberField(INVTEXT("Price"), Bind(Price), EVekaTextFieldType::Number),
		NumberField(INVTEXT("Share"), Bind(Share), EVekaTextFieldType::Percent),
		Text(TEXT("Context")).OnSecondaryTap([Secondary](const FVector2D&) { ++*Secondary; }),
		Rectangle().Frame(120.f, 20.f).OnTap([Taps]() { ++*Taps; }).OnDrag([Drags](const FVekaDragValue&) { ++*Drags; }),
		HStack(4.f)(Text(TEXT("L")), Divider(), Text(TEXT("R"))).FlexFrame(-1.0, -1.0, 30.0, 30.0),
		Divider()
	).Frame(400.f, 900.f, FVekaAlignment::TopLeading());

	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Root);
	FVekaRuntime& Runtime = *Host->GetRuntime();
	{
		const FControlsPaint Paint(Host, FVector2f(400.f, 900.f));
	}
	const TSharedPtr<FVekaNode> Tree = Runtime.GetRootNode();
	const FDriver Driver{ Runtime };

	// Buttons.
	const TSharedPtr<FVekaNode> Save = Interactive(FindText(Tree, TEXT("Save")));
	const TSharedPtr<FVekaNode> LockedButton = Interactive(FindText(Tree, TEXT("Locked")));
	if (!TestNotNull(TEXT("Save button"), Save.Get()) || !TestNotNull(TEXT("Locked button node"), FindText(Tree, TEXT("Locked")).Get()))
	{
		return false;
	}
	Driver.Tap(Save);
	TestEqual(TEXT("Tap runs the action"), *Saves, 1);
	TestTrue(TEXT("Buttons take keyboard focus"), Driver.WidgetOf(Save)->SupportsKeyboardFocus());
	TestTrue(TEXT("Enter is handled"), Driver.Key(Save, EKeys::Enter));
	TestEqual(TEXT("Enter activates"), *Saves, 2);
	TestFalse(TEXT("Other keys pass (Tab navigation)"), Driver.Key(Save, EKeys::Tab));
	if (LockedButton.IsValid())
	{
		Driver.Tap(LockedButton);
		TestFalse(TEXT("Disabled button takes no focus"), Driver.WidgetOf(LockedButton)->SupportsKeyboardFocus());
	}
	TestEqual(TEXT("Disabled button ignores taps"), *Locked, 0);

	// Toggle.
	const TSharedPtr<FVekaNode> Toggle = Interactive(FindText(Tree, TEXT("Wi-Fi")));
	if (TestNotNull(TEXT("Toggle"), Toggle.Get()))
	{
		Driver.Tap(Toggle);
		TestTrue(TEXT("Toggle on"), *WiFi);
		Driver.Tap(Toggle);
		TestFalse(TEXT("Toggle off"), *WiFi);
	}

	// Sliders: the first two tracks.
	TArray<TSharedPtr<FVekaNode>> Tracks;
	Find(Tree, [&Tracks](const FVekaNode& Node)
	{
		if (Node.GetPrimitiveAs<FVekaTrack>())
		{
			Tracks.Add(const_cast<FVekaNode&>(Node).AsShared());
		}
		return false;
	});
	if (TestTrue(TEXT("Two slider tracks"), Tracks.Num() >= 2))
	{
		const TSharedPtr<FVekaNode> Free = Interactive(Tracks[0]);
		Driver.Drag(Free, 0.1f, 0.75f);
		const float Expected = FVekaTrack::FractionAt(0.75 * Free->GetArrangedRect().Size.X, Free->GetArrangedRect().Size.X, 20.f);
		TestTrue(FString::Printf(TEXT("Drag sets the value (%.3f ~ %.3f)"), *Volume, Expected), FMath::IsNearlyEqual(*Volume, Expected, 0.01f));
		const float BeforeKey = *Volume;
		TestTrue(TEXT("Arrow handled"), Driver.Key(Free, EKeys::Right));
		TestTrue(TEXT("Right arrow adds 5 %"), FMath::IsNearlyEqual(*Volume, FMath::Min(1.f, BeforeKey + 0.05f), 1e-3f));
		Driver.Key(Free, EKeys::End);
		TestEqual(TEXT("End = max"), *Volume, 1.f);

		Driver.Drag(Interactive(Tracks[1]), 0.f, 0.6f);
		TestEqual(TEXT("Stepped slider snaps to 0.5"), *Stepped, 0.5f);
	}

	// Drag vs tap.
	const TSharedPtr<FVekaNode> Pad = Find(Tree, [](const FVekaNode& Node) { return !Node.GetGestures().OnDrag.IsEmpty() && !Node.GetGestures().OnTap.IsEmpty(); });
	if (TestNotNull(TEXT("Drag pad"), Pad.Get()))
	{
		Driver.Tap(Pad);
		TestEqual(TEXT("Press without moving taps"), *Taps, 1);
		Driver.Drag(Pad, 0.1f, 0.9f);
		TestEqual(TEXT("A drag is not a tap"), *Taps, 1);
		TestTrue(TEXT("Drag events (began, changed, ended)"), *Drags >= 4);
	}

	// Stepper (clamped).
	const TSharedPtr<FVekaNode> Plus = Interactive(FindSymbol(Tree, TEXT("plus")));
	const TSharedPtr<FVekaNode> Minus = Interactive(FindSymbol(Tree, TEXT("minus")));
	if (TestNotNull(TEXT("Stepper +"), Plus.Get()) && TestNotNull(TEXT("Stepper -"), Minus.Get()))
	{
		Driver.Tap(Plus);
		TestEqual(TEXT("+1"), *Count, 10.f);
		Driver.Tap(Plus);
		TestEqual(TEXT("Clamped at max"), *Count, 10.f);
		Driver.Tap(Interactive(FindSymbol(Tree, TEXT("minus"))));
		TestEqual(TEXT("-1"), *Count, 9.f);
	}

	// Pickers.
	const TSharedPtr<FVekaNode> Beta = Interactive(FindText(Tree, TEXT("Beta")));
	if (TestNotNull(TEXT("Segment Beta"), Beta.Get()))
	{
		Driver.Tap(Beta);
		TestEqual(TEXT("Segment selected"), *Segment, 1);
	}
	const TSharedPtr<FVekaNode> Three = Interactive(FindText(Tree, TEXT("Three")));
	const TSharedPtr<FVekaNode> One = Interactive(FindText(Tree, TEXT("One")));
	if (TestNotNull(TEXT("Radio Three"), Three.Get()) && TestNotNull(TEXT("Radio One"), One.Get()))
	{
		Driver.Tap(Three);
		TestEqual(TEXT("Radio flexible: tapping the selected option clears it"), *Radio, -1);
		Driver.Tap(Interactive(FindText(Runtime.GetRootNode(), TEXT("One"))));
		TestEqual(TEXT("Radio selects"), *Radio, 0);
	}

	// Key binding.
	const TSharedPtr<FVekaNode> KeyButton = Interactive(FindText(Tree, TEXT("E")));
	if (TestNotNull(TEXT("Key bind button"), KeyButton.Get()))
	{
		TestFalse(TEXT("Not listening: keys pass"), Driver.Key(KeyButton, EKeys::F) && *Bound == EKeys::F);
		Driver.Tap(KeyButton);
		const TSharedPtr<FVekaNode> Listening = Interactive(FindText(Runtime.GetRootNode(), TEXT("Press a key...")));
		if (TestNotNull(TEXT("Listening label"), Listening.Get()))
		{
			Driver.Key(Listening, EKeys::F);
			TestEqual(TEXT("Key bound"), *Bound, FKey(EKeys::F));
		}
	}

	// Text fields (the callbacks the native widget calls).
	TArray<const FVekaTextInput*> Inputs;
	Find(Runtime.GetRootNode(), [&Inputs](const FVekaNode& Node)
	{
		if (const FVekaTextInput* Input = Node.GetPrimitiveAs<FVekaTextInput>())
		{
			Inputs.Add(Input);
		}
		return false;
	});
	if (TestEqual(TEXT("Three text inputs"), Inputs.Num(), 3))
	{
		Inputs[0]->OnChanged(TEXT("Ada"));
		TestEqual(TEXT("Text field writes the binding"), *Name, FString(TEXT("Ada")));
		Inputs[1]->OnCommitted(FString(TEXT("1")) + FString::Chr(0x202F) + TEXT("234,5"), true);
		TestEqual(TEXT("Number parsed (grouping, decimal comma)"), *Price, 1234.5);
		Inputs[1]->OnCommitted(TEXT("1,234.25"), true);
		TestEqual(TEXT("Number parsed (English)"), *Price, 1234.25);
		Inputs[1]->OnCommitted(TEXT("abc"), true);
		TestEqual(TEXT("Invalid input keeps the value"), *Price, 1234.25);
		Inputs[2]->OnCommitted(TEXT("25 %"), true);
		TestEqual(TEXT("Percent parsed"), *Share, 0.25);
		TestTrue(TEXT("Percent shown with %"), Inputs[2]->Text.Contains(TEXT("%")));
	}

	// Right click.
	const TSharedPtr<FVekaNode> Context = FindText(Runtime.GetRootNode(), TEXT("Context"));
	if (TestNotNull(TEXT("Context node"), Context.Get()))
	{
		const TSharedRef<SVekaNodeWidget> Widget = Driver.WidgetOf(Context);
		const FGeometry Geometry = Driver.GeometryOf(Context);
		const FVector2f Center = Geometry.GetLocalSize() * 0.5f;
		Widget->OnMouseButtonDown(Geometry, Mouse(Center, EKeys::LeftMouseButton, true));
		Widget->OnMouseButtonUp(Geometry, Mouse(Center, EKeys::LeftMouseButton, false));
		TestEqual(TEXT("Left click is not a secondary tap"), *Secondary, 0);
		Widget->OnMouseButtonDown(Geometry, Mouse(Center, EKeys::RightMouseButton, true));
		TestEqual(TEXT("Right click"), *Secondary, 1);
	}

	// Dividers follow their stack (the stepper has one too: take the last two).
	TArray<TSharedPtr<FVekaNode>> Dividers;
	Find(Runtime.GetRootNode(), [&Dividers](const FVekaNode& Node)
	{
		if (Node.GetPrimitiveAs<FVekaDivider>())
		{
			Dividers.Add(const_cast<FVekaNode&>(Node).AsShared());
		}
		return false;
	});
	if (TestTrue(TEXT("Dividers found"), Dividers.Num() >= 2))
	{
		const FVector2D InRow = Dividers[Dividers.Num() - 2]->GetArrangedRect().Size;
		const FVector2D InColumn = Dividers.Last()->GetArrangedRect().Size;
		TestTrue(FString::Printf(TEXT("Vertical in an HStack (%.0f x %.0f)"), InRow.X, InRow.Y), InRow.X <= 1.01 && InRow.Y > 10.0);
		TestTrue(FString::Printf(TEXT("Horizontal in a VStack (%.0f x %.0f)"), InColumn.X, InColumn.Y), InColumn.Y <= 1.01 && InColumn.X > 100.0);
	}

	TestFalse(TEXT("Popups need a gesture"), Veka::Popup::Present(Text(TEXT("x"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaControlRenderTest, "VekaUI.Controls.Render", VekaControlsTestFlags)
bool FVekaControlRenderTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		AddInfo(TEXT("No Slate renderer: skipped."));
		return true;
	}
	using namespace Veka;

	auto Meshes = [](const FVekaElement& Element)
	{
		TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Element);
		const FControlsPaint Paint(Host, FVector2f(300.f, 200.f));
		return Paint.Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_CustomVerts>().Num();
	};

	for (int32 Style = 0; Style <= static_cast<int32>(EVekaGaugeStyle::CircularAlt); ++Style)
	{
		TestTrue(FString::Printf(TEXT("Gauge style %d draws"), Style), Meshes(Gauge(0.4f, 0.f, 1.f, INVTEXT("Speed"), FText::GetEmpty(), INVTEXT("0"), INVTEXT("100")).GaugeStyle(static_cast<EVekaGaugeStyle>(Style))) > 0);
	}
	const int32 WithKnob = Meshes(Gauge(0.4f).GaugeStyle(EVekaGaugeStyle::LinearAlt));
	const int32 WithFill = Meshes(Gauge(0.4f).GaugeStyle(EVekaGaugeStyle::Linear));
	TestTrue(TEXT("Alt gauge draws a marker (more meshes than the plain one)"), WithKnob > WithFill);
	TestTrue(TEXT("Empty progress draws its track"), Meshes(ProgressView(0.f)) > 0);
	TestTrue(TEXT("Circular progress"), Meshes(ProgressView(0.5f).ProgressViewStyle(EVekaProgressStyle::Circular)) > 0);
	TestTrue(TEXT("Lazy progress is a symbol"), Meshes(ProgressView().ProgressViewStyle(EVekaProgressStyle::Lazy)) > 0);

	// Indeterminate progress repaints continuously; determinate does not.
	auto Animates = [](const FVekaElement& Element)
	{
		TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Element);
		const FControlsPaint Paint(Host, FVector2f(300.f, 100.f));
		bool bAnimating = false;
		Find(Host->GetRuntime()->GetRootNode(), [&bAnimating](const FVekaNode& Node)
		{
			if (Node.GetPrimitiveAs<FVekaTrack>() && Node.GetPrimitiveState())
			{
				bAnimating |= Node.GetPrimitiveState()->NeedsAnimationFrame(FPlatformTime::Seconds());
			}
			return false;
		});
		return bAnimating;
	};
	TestTrue(TEXT("Indeterminate animates"), Animates(ProgressView()));
	TestFalse(TEXT("Determinate is still"), Animates(ProgressView(0.3f)));

	// Label styles.
	auto CountText = [](const FVekaElement& Element)
	{
		TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Element);
		const FControlsPaint Paint(Host, FVector2f(300.f, 100.f));
		return Paint.Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Text>().Num()
			+ Paint.Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_ShapedText>().Num();
	};
	TestTrue(TEXT("Label shows its title"), CountText(Label(INVTEXT("Inbox"), TEXT("tray"))) > 0);
	TestEqual(TEXT("Icon-only label has no text"), CountText(Label(INVTEXT("Inbox"), TEXT("tray")).LabelStyle(EVekaLabelStyle::Icon)), 0);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
// Regression (2e): labels in buttons, toggles and pickers wrapped to two lines. A one-word label measures the same
// at proposal 0 and infinity (the stack replayed the degenerate one), and pixel-snapped geometry proposed text a
// fraction of a pixel less than its width. Counter-check: a really narrow frame still wraps.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaControlLabelsOneLineTest, "VekaUI.Controls.LabelsOnOneLine", VekaControlsTestFlags)
bool FVekaControlLabelsOneLineTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	using namespace Veka;
	auto Lines = [](const FVekaElement& Element, const FString& Word)
	{
		TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Element);
		const FControlsPaint Paint(Host, FVector2f(900.f, 300.f));
		const TSharedPtr<FVekaNode> Node = FindText(Host->GetRuntime()->GetRootNode(), Word);
		if (!Node.IsValid())
		{
			return -1;
		}
		const double Single = Node->Measure(FVekaProposal::Unspecified()).Y;
		return FMath::RoundToInt32(Node->GetArrangedRect().Size.Y / FMath::Max(Single, 1.0));
	};
	TestEqual(TEXT("Button with a label"), Lines(HStack(10.f)(Button(INVTEXT("Prominent"), TEXT("paperplane"), []() {})), TEXT("Prominent")), 1);
	TestEqual(TEXT("Label in a stack at its ideal width"), Lines(HStack(6.f)(Symbol(TEXT("paperplane")), Text(TEXT("Prominent"))).Frame(109.f, 30.f), TEXT("Prominent")), 1);
	TestEqual(TEXT("Switch toggle"), Lines(Toggle(Label(INVTEXT("Bluetooth"), TEXT("bluetooth")), TVekaBinding<bool>::Constant(false)).Frame(240.f, -1.f), TEXT("Bluetooth")), 1);
	TestEqual(TEXT("Menu picker"), Lines(Picker(INVTEXT("Size"), TVekaBinding<int32>::Constant(0), { FVekaPickerOption(INVTEXT("Small")) }).Frame(200.f, -1.f), TEXT("Small")), 1);
	const int32 Narrow = Lines(VStack()(Text(TEXT("Two words"))).Frame(50.f, 200.f), TEXT("Two words"));
	TestTrue(FString::Printf(TEXT("Counter-check: a narrow frame still wraps (%d lines)"), Narrow), Narrow > 1);
	return true;
}
#endif
