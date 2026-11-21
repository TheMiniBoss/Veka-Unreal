// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 2g: FVekaValue, documents, the interpreter, the UVekaView asset and the Blueprint bridge of the Veka Host.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Tests/VekaUIViewTestObjects.h"
#include "Samples/VekaSampleView.h"
#include "Slate/SVekaNodeWidget.h"
#include "UMG/VekaHostWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/Package.h"
#include "Misc/ScopeExit.h"
#include "Containers/Ticker.h"

namespace
{
	constexpr EAutomationTestFlags VekaViewTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	void PaintOnce(const TSharedRef<SWidget>& Widget, const FVector2f& Size = FVector2f(500.f, 700.f))
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Widget->SlatePrepass(1.f);
		Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
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

	TSharedPtr<FVekaNode> FindText(const TSharedPtr<FVekaNode>& Root, TFunctionRef<bool(const FString&)> Predicate)
	{
		return Find(Root, [&Predicate](const FVekaNode& Node)
		{
			const FVekaText* Text = Node.GetPrimitiveAs<FVekaText>();
			return Text && !Node.IsInRemovedSubtree() && Predicate(Text->Text.ToString());
		});
	}

	TSharedPtr<FVekaNode> FindTextEq(const TSharedPtr<FVekaNode>& Root, const FString& Value)
	{
		return FindText(Root, [&Value](const FString& S) { return S == Value; });
	}

	FString TextStarting(const TSharedPtr<FVekaNode>& Root, const FString& Prefix)
	{
		const TSharedPtr<FVekaNode> Node = FindText(Root, [&Prefix](const FString& S) { return S.StartsWith(Prefix); });
		return Node.IsValid() ? Node->GetPrimitiveAs<FVekaText>()->Text.ToString() : FString();
	}

	void Tap(FVekaRuntime& Runtime, const TSharedPtr<FVekaNode>& Start)
	{
		TSharedPtr<FVekaNode> Node = Start;
		for (FVekaNode* Current = Start.Get(); Current; Current = Current->GetParent())
		{
			if (Current->WantsInput())
			{
				Node = Current->AsShared();
				break;
			}
		}
		if (!Node.IsValid())
		{
			return;
		}
		const TSharedRef<SVekaNodeWidget> Widget = StaticCastSharedRef<SVekaNodeWidget>(Node->GetOrCreateWidget());
		const FVector2f Size(Node->GetArrangedRect().Size);
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
		TSet<FKey> Down;
		Down.Add(EKeys::LeftMouseButton);
		Widget->OnMouseButtonDown(Geometry, FPointerEvent(0, Size * 0.5f, Size * 0.5f, Down, EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
		Runtime.Flush();
		Widget->OnMouseButtonUp(Geometry, FPointerEvent(0, Size * 0.5f, Size * 0.5f, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
		Runtime.Flush();
	}

	/** Counter document: Text "Count: N", a +1 button, an If and a ForEach, an emit. */
	FVekaDocument MakeCounterDocument()
	{
		FVekaDocumentBuilder B;
		B.State(TEXT("count"), FVekaValue::Int(0));
		B.Event(TEXT("Reset"));
		using Op = EVekaExprOp;
		auto Root = B.Node(TEXT("VStack")).Children({
			B.Node(TEXT("Text")).Arg(TEXT("text"), B.Format(TEXT("Count: {0}"), { B.Var(TEXT("count")) })),
			B.Node(TEXT("Button")).Arg(TEXT("title"), B.Lit(TEXT("Plus"))).Action(FVekaDocumentBuilder::Increment(TEXT("count"), B.Lit(1))),
			B.Node(TEXT("Button")).Arg(TEXT("title"), B.Lit(TEXT("Reset"))).Action(FVekaDocumentBuilder::Emit(TEXT("Reset"), B.Var(TEXT("count"))))
				.Action(FVekaDocumentBuilder::Set(TEXT("count"), B.Lit(0))),
			B.If(B.Binary(Op::GreaterEqual, B.Var(TEXT("count")), B.Lit(2)),
				{ B.Node(TEXT("Text")).Arg(TEXT("text"), B.Lit(TEXT("Big"))) },
				{ B.Node(TEXT("Text")).Arg(TEXT("text"), B.Lit(TEXT("Small"))) }),
			B.ForEach(B.Var(TEXT("count")), TEXT("i"), {
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Format(TEXT("Row {0}"), { B.Local(TEXT("i")) })) }),
			B.Node(TEXT("Shape")).Arg(TEXT("Fill"), B.Lit(FLinearColor::Red)).Modifier(TEXT("frame"), { { TEXT("width"), B.Lit(20) }, { TEXT("height"), B.Lit(10) } }),
			B.Node(TEXT("Frobnicator"))
		});
		B.SetRoot(Root);
		return B.Release();
	}

	int32 CountRows(const TSharedPtr<FVekaNode>& Node)
	{
		int32 Count = 0;
		Find(Node, [&Count](const FVekaNode& Item)
		{
			const FVekaText* Text = Item.GetPrimitiveAs<FVekaText>();
			Count += Text && !Item.IsInRemovedSubtree() && Text->Text.ToString().StartsWith(TEXT("Row ")) ? 1 : 0;
			return false;
		});
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewValueTest, "VekaUI.View.Value", VekaViewTestFlags)
bool FVekaViewValueTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("1 == 1.0"), FVekaValue::Int(1) == FVekaValue::Float(1.0));
	TestTrue(TEXT("Counter-check: 1 != 2"), FVekaValue::Int(1) != FVekaValue::Int(2));
	TestTrue(TEXT("\"1\" is not the number 1"), FVekaValue::String(TEXT("1")) != FVekaValue::Int(1));
	TestEqual(TEXT("Float prints short"), FVekaValue::Float(1.5).AsString(), FString(TEXT("1.5")));
	TestEqual(TEXT("Float rounding noise hidden"), FVekaValue::Float(0.72f * 100.0).AsString(), FString(TEXT("72")));
	TestEqual(TEXT("Int prints"), FVekaValue::Int(42).AsString(), FString(TEXT("42")));
	TestTrue(TEXT("Truthy number"), FVekaValue::Int(3).AsBool());
	TestFalse(TEXT("Empty string is false"), FVekaValue::String(TEXT("")).AsBool());
	TestEqual(TEXT("Hex string to color"), FVekaValue::String(TEXT("#FF0000")).AsColor().ToFColor(true), FColor(255, 0, 0));
	TestEqual(TEXT("Equal values hash equal"), GetTypeHash(FVekaValue::Int(5)), GetTypeHash(FVekaValue::Float(5.0)));

	// Reflection: enum by name, float, and a type that cannot hold a color.
	FVekaStack Stack;
	const UScriptStruct* Struct = FVekaStack::StaticStruct();
	const FProperty* Spacing = Struct->FindPropertyByName(TEXT("Spacing"));
	if (TestNotNull(TEXT("Stack.Spacing"), Spacing))
	{
		TestTrue(TEXT("Number into a float"), FVekaValue::Int(12).ToProperty(Spacing, Spacing->ContainerPtrToValuePtr<void>(&Stack)));
		TestEqual(TEXT("Spacing set"), Stack.Spacing, 12.f);
		TestEqual(TEXT("Read back"), FVekaValue::FromProperty(Spacing, Spacing->ContainerPtrToValuePtr<void>(&Stack)).AsFloat(), 12.0);
	}
	const FProperty* Axis = Struct->FindPropertyByName(TEXT("Axis"));
	if (TestNotNull(TEXT("Stack.Axis"), Axis))
	{
		TestTrue(TEXT("Enum by name"), FVekaValue::String(TEXT("Horizontal")).ToProperty(Axis, Axis->ContainerPtrToValuePtr<void>(&Stack)));
		TestEqual(TEXT("Axis set"), Stack.Axis, EVekaAxis::Horizontal);
		TestFalse(TEXT("Counter-check: unknown enum name refused"), FVekaValue::String(TEXT("Diagonal")).ToProperty(Axis, Axis->ContainerPtrToValuePtr<void>(&Stack)));
		TestEqual(TEXT("Unchanged after a refusal"), Stack.Axis, EVekaAxis::Horizontal);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewDocumentTest, "VekaUI.View.Document", VekaViewTestFlags)
bool FVekaViewDocumentTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Valid = Veka::Samples::MakeShopDocument().Validate();
	TestEqual(FString::Printf(TEXT("Sample is valid (%s)"), *FString::Join(Valid, TEXT("; "))), Valid.Num(), 0);
	TestEqual(TEXT("Counter document is valid"), MakeCounterDocument().Validate().Num(), 0);

	FVekaDocumentBuilder B;
	B.State(TEXT("a"), FVekaValue::Int(0));
	auto Root = B.Node(TEXT("Button")).Action(FVekaDocumentBuilder::Set(TEXT("nope"), B.Var(TEXT("missing")))).Action(FVekaDocumentBuilder::Emit(TEXT("Undeclared")));
	B.SetRoot(Root);
	FVekaDocument Broken = B.Release();
	Broken.Nodes[0].Children.Add(42);
	const TArray<FString> Problems = Broken.Validate();
	const FString All = FString::Join(Problems, TEXT(" | "));
	TestTrue(TEXT("Unknown state read"), All.Contains(TEXT("unknown state 'missing'")));
	TestTrue(TEXT("Unknown state written"), All.Contains(TEXT("changes unknown state 'nope'")));
	TestTrue(TEXT("Undeclared event"), All.Contains(TEXT("undeclared event 'Undeclared'")));
	TestTrue(TEXT("Bad child index"), All.Contains(TEXT("child 42 out of range")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewInterpreterTest, "VekaUI.View.Interpreter", VekaViewTestFlags)
bool FVekaViewInterpreterTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	TSharedRef<FVekaViewStore> Store = MakeShared<FVekaViewStore>();
	TArray<TPair<FName, FVekaValue>> Emitted;
	Store->OnEvent = [&Emitted](FName Event, const FVekaValue& Payload) { Emitted.Emplace(Event, Payload); };

	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::Interpreter::MakeView(MakeCounterDocument(), Store));
	FVekaRuntime& Runtime = *Host->GetRuntime();
	PaintOnce(Host);
	auto Root = [&Runtime]() { return Runtime.GetRootNode(); };

	TestEqual(TEXT("Initial state"), TextStarting(Root(), TEXT("Count:")), FString(TEXT("Count: 0")));
	TestNotNull(TEXT("If: else branch"), FindTextEq(Root(), TEXT("Small")).Get());
	TestNull(TEXT("If: then branch hidden"), FindTextEq(Root(), TEXT("Big")).Get());
	TestEqual(TEXT("ForEach: no rows"), CountRows(Root()), 0);

	Tap(Runtime, FindTextEq(Root(), TEXT("Plus")));
	PaintOnce(Host);
	Tap(Runtime, FindTextEq(Root(), TEXT("Plus")));
	PaintOnce(Host);
	TestEqual(TEXT("Action increments the state"), TextStarting(Root(), TEXT("Count:")), FString(TEXT("Count: 2")));
	TestEqual(TEXT("Store holds an int"), Store->Get(TEXT("count")), FVekaValue::Int(2));
	TestNotNull(TEXT("If: then branch shown"), FindTextEq(Root(), TEXT("Big")).Get());
	TestNull(TEXT("If: else branch gone"), FindTextEq(Root(), TEXT("Small")).Get());
	TestEqual(TEXT("ForEach: two rows"), CountRows(Root()), 2);
	TestNotNull(TEXT("Loop variable"), FindTextEq(Root(), TEXT("Row 1")).Get());

	// External change (what SetState does).
	Store->Set(TEXT("count"), FVekaValue::Int(5));
	Runtime.Flush();
	PaintOnce(Host);
	TestEqual(TEXT("External set re-evaluates"), TextStarting(Root(), TEXT("Count:")), FString(TEXT("Count: 5")));
	TestEqual(TEXT("ForEach follows"), CountRows(Root()), 5);

	Tap(Runtime, FindTextEq(Root(), TEXT("Reset")));
	PaintOnce(Host);
	if (TestEqual(TEXT("One event"), Emitted.Num(), 1))
	{
		TestEqual(TEXT("Event name"), Emitted[0].Key, FName(TEXT("Reset")));
		TestEqual(TEXT("Payload evaluated before the next action"), Emitted[0].Value, FVekaValue::Int(5));
	}
	TestEqual(TEXT("Second action ran"), TextStarting(Root(), TEXT("Count:")), FString(TEXT("Count: 0")));

	// Schema fallback and unknown components.
	const TSharedPtr<FVekaNode> Shape = Find(Root(), [](const FVekaNode& Node) { return Node.GetPrimitiveAs<FVekaShapeView>() != nullptr; });
	if (TestNotNull(TEXT("Schema primitive 'Shape'"), Shape.Get()))
	{
		const FVekaShapeView* View = Shape->GetPrimitiveAs<FVekaShapeView>();
		TestEqual(TEXT("Paint set by reflection"), View->Fill.Color, FLinearColor::Red);
		TestTrue(TEXT("Frame modifier applied"), Shape->GetArrangedRect().Size.Equals(FVector2D(20.0, 10.0), 0.5));
	}
	TestNotNull(TEXT("Unknown component shows a placeholder"), FindTextEq(Root(), TEXT("Unknown 'Frobnicator'")).Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewHostBridgeTest, "VekaUI.View.HostBridge", VekaViewTestFlags)
bool FVekaViewHostBridgeTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	UVekaView* View = NewObject<UVekaView>(GetTransientPackage());
	View->Document = Veka::Samples::MakeShopDocument();
	UVekaHostWidget* HostWidget = NewObject<UVekaHostWidget>(GetTransientPackage());
	UVekaSampleViewModel* Model = NewObject<UVekaSampleViewModel>(HostWidget);
	Model->PlayerName = TEXT("Nova");
	Model->Gold = 60;
	HostWidget->View = View;
	HostWidget->ViewModel = Model;
	HostWidget->InitialState.Add(TEXT("count"), FVekaValue::Int(2));

	UVekaTestEventSink* Sink = NewObject<UVekaTestEventSink>(GetTransientPackage());
	Sink->AddToRoot();
	ON_SCOPE_EXIT { Sink->RemoveFromRoot(); };
	FScriptDelegate Delegate;
	Delegate.BindUFunction(Sink, GET_FUNCTION_NAME_CHECKED(UVekaTestEventSink, HandleEvent));
	HostWidget->OnVekaEvent.Add(Delegate);

	const TSharedRef<SWidget> Widget = HostWidget->TakeWidget();
	if (!TestEqual(TEXT("UMG widget is a Veka host"), Widget->GetType(), FName(TEXT("SVekaHost"))))
	{
		return false;
	}
	FVekaRuntime& Runtime = *StaticCastSharedRef<SVekaHost>(Widget)->GetRuntime();
	PaintOnce(Widget);
	auto Root = [&Runtime]() { return Runtime.GetRootNode(); };

	TestNotNull(TEXT("Model value shown"), FindTextEq(Root(), TEXT("Nova")).Get());
	TestNotNull(TEXT("Format with a model value"), FindTextEq(Root(), TEXT("60 gold")).Get());
	TestEqual(TEXT("InitialState wins over the declared value"), TextStarting(Root(), TEXT("Buy ")), FString(TEXT("Buy 2 for 50 gold")));

	// SetState through reflection, as a Blueprint node calls it.
	{
		UFunction* Function = HostWidget->FindFunctionChecked(TEXT("SetState"));
		struct FParams { FName Name; FVekaValue Value; } Params{ TEXT("count"), FVekaValue::Int(3) };
		HostWidget->ProcessEvent(Function, &Params);
	}
	Runtime.Flush();
	PaintOnce(Widget);
	TestEqual(TEXT("Blueprint SetState re-evaluates"), TextStarting(Root(), TEXT("Buy ")), FString(TEXT("Buy 3 for 75 gold")));
	TestNotNull(TEXT("75 > 60 gold: warning shown"), FindTextEq(Root(), TEXT("Not enough gold")).Get());

	// The button is disabled: tapping does nothing (counter-check of the event path below).
	Tap(Runtime, FindText(Root(), [](const FString& S) { return S.StartsWith(TEXT("Buy ")); }));
	TestEqual(TEXT("Disabled button emits nothing"), Sink->Events.Num(), 0);

	// The model changes (NotifyChanged): enough gold now.
	Model->Gold = 200;
	Model->NotifyChanged();
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Model change re-evaluates"), FindTextEq(Root(), TEXT("200 gold")).Get());
	TestNull(TEXT("Warning gone"), FindTextEq(Root(), TEXT("Not enough gold")).Get());

	Tap(Runtime, FindText(Root(), [](const FString& S) { return S.StartsWith(TEXT("Buy ")); }));
	if (TestEqual(TEXT("OnVekaEvent reached the Blueprint-style handler"), Sink->Events.Num(), 1))
	{
		TestEqual(TEXT("Event name"), Sink->Events[0], FName(TEXT("Buy")));
		TestEqual(TEXT("Payload = count"), Sink->Payloads[0], FVekaValue::Int(3));
	}

	// GetState through reflection; SetValue on the model by name.
	{
		UFunction* Function = HostWidget->FindFunctionChecked(TEXT("GetState"));
		struct FParams { FName Name; FVekaValue ReturnValue; } Params{ TEXT("count"), FVekaValue() };
		HostWidget->ProcessEvent(Function, &Params);
		TestEqual(TEXT("Blueprint GetState"), Params.ReturnValue, FVekaValue::Int(3));
	}
	TestTrue(TEXT("Model SetValue by name"), Model->SetValue(TEXT("PlayerName"), FVekaValue::String(TEXT("Kai"))));
	TestFalse(TEXT("Counter-check: unknown model property"), Model->SetValue(TEXT("Mana"), FVekaValue::Int(1)));
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Name changed"), FindTextEq(Root(), TEXT("Kai")).Get());

	// A Blueprint "Set" does not notify: the store notices on its own (polling).
	Model->Gold = 10;
	FTSTicker::GetCoreTicker().Tick(0.2f);
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Silent model change seen by polling"), FindTextEq(Root(), TEXT("10 gold")).Get());

	HostWidget->ReleaseSlateResources(true);
	return true;
}

#endif
