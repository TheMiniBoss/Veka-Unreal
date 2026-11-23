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
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

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

namespace
{
	/** The sample component and shop, as transient assets. */
	struct FShopAssets
	{
		UVekaView* Item = nullptr;
		UVekaView* Shop = nullptr;

		FShopAssets()
		{
			Item = NewObject<UVekaView>(GetTransientPackage(), TEXT("VV_TestShopItem"));
			Item->Document = Veka::Samples::MakeShopItemDocument();
			Item->AddToRoot();
			Shop = NewObject<UVekaView>(GetTransientPackage(), TEXT("VV_TestShop"));
			Shop->Document = Veka::Samples::MakeShopDocument(Item);
			Shop->AddToRoot();
		}

		~FShopAssets()
		{
			Item->RemoveFromRoot();
			Shop->RemoveFromRoot();
		}
	};

	/** The "Buy" button of the article whose title is Title. */
	TSharedPtr<FVekaNode> BuyButtonOf(const TSharedPtr<FVekaNode>& Root, const FString& Title)
	{
		const TSharedPtr<FVekaNode> Name = FindTextEq(Root, Title);
		// Up to the article's HStack, then its "Buy" label.
		for (FVekaNode* Current = Name.Get(); Current; Current = Current->GetParent())
		{
			if (const TSharedPtr<FVekaNode> Buy = FindTextEq(Current->AsShared(), TEXT("Buy")))
			{
				return Buy;
			}
		}
		return nullptr;
	}

	/** Component with its own state: a counter with a label parameter. */
	FVekaDocument MakeCounterComponentDocument()
	{
		FVekaDocumentBuilder B;
		B.Param(TEXT("label"), EVekaValueType::String, FVekaValue::String(TEXT("?")));
		B.State(TEXT("taps"), FVekaValue::Int(0));
		B.SetRoot(B.Node(TEXT("Button"))
			.Arg(TEXT("title"), B.Format(TEXT("{0}: {1}"), { B.ParamRef(TEXT("label")), B.Var(TEXT("taps")) }))
			.Action(FVekaDocumentBuilder::Increment(TEXT("taps"), B.Lit(1))));
		return B.Release();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewValueV2Test, "VekaUI.View.ValueV2", VekaViewTestFlags)
bool FVekaViewValueV2Test::RunTest(const FString& Parameters)
{
	const FVekaValue Item = FVekaValue::Struct({ { TEXT("Name"), FVekaValue::Text(INVTEXT("Shield")) }, { TEXT("Price"), FVekaValue::Int(60) } });
	const FVekaValue List = FVekaValue::Array({ Item, FVekaValue::Int(3) });
	TestEqual(TEXT("Array size"), List.Num(), 2);
	TestEqual(TEXT("Item then field"), List.GetItem(0).GetField(TEXT("Price")), FVekaValue::Int(60));
	TestTrue(TEXT("Out of range is None"), List.GetItem(5).IsNone());
	TestTrue(TEXT("Missing field is None"), Item.GetField(TEXT("Weight")).IsNone());
	TestTrue(TEXT("Text equals the same string"), FVekaValue::Text(INVTEXT("Shield")) == FVekaValue::String(TEXT("Shield")));
	TestTrue(TEXT("Arrays compare by content"), List == FVekaValue::Array({ Item, FVekaValue::Float(3.0) }));
	TestTrue(TEXT("Counter-check: different content"), List != FVekaValue::Array({ Item, FVekaValue::Int(4) }));
	TestEqual(TEXT("Equal arrays hash equal"), GetTypeHash(List), GetTypeHash(FVekaValue::Array({ Item, FVekaValue::Int(3) })));

	// Reflection: an array of structs (the sample model's items), texts kept localizable.
	UVekaSampleViewModel* Model = NewObject<UVekaSampleViewModel>(GetTransientPackage());
	const FVekaValue Items = Model->GetValue(TEXT("Items"));
	if (TestEqual(TEXT("Model items as an array"), Items.Num(), 3))
	{
		TestEqual(TEXT("Struct field is a Text"), Items.GetItem(1).GetField(TEXT("Name")).Type, EVekaValueType::Text);
		TestEqual(TEXT("Field value"), Items.GetItem(1).GetField(TEXT("Price")), FVekaValue::Int(60));
		TestTrue(TEXT("Bool field"), Items.GetItem(1).GetField(TEXT("bNew")).AsBool());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewDocumentV2Test, "VekaUI.View.DocumentV2", VekaViewTestFlags)
bool FVekaViewDocumentV2Test::RunTest(const FString& Parameters)
{
	// Format 1 (Phase 2): event names, no version.
	FVekaDocument Old;
	Old.Events = { TEXT("Buy"), TEXT("Close") };
	Old.FormatVersion = 0;
	TestTrue(TEXT("Upgrade changes an old document"), Old.Upgrade());
	TestEqual(TEXT("Events moved to declarations"), Old.EventDeclarations.Num(), 2);
	TestTrue(TEXT("Old list emptied"), Old.Events.IsEmpty());
	TestTrue(TEXT("Event still known"), Old.HasEvent(TEXT("Close")));
	TestEqual(TEXT("Current version"), Old.FormatVersion, FVekaDocument::CurrentFormatVersion);
	TestFalse(TEXT("Counter-check: a current document is left alone"), Old.Upgrade());

	const FShopAssets Assets;
	const TArray<FString> ItemProblems = Assets.Item->Document.Validate();
	TestEqual(FString::Printf(TEXT("Component sample valid (%s)"), *FString::Join(ItemProblems, TEXT("; "))), ItemProblems.Num(), 0);
	const TArray<FString> ShopProblems = Assets.Shop->Document.Validate();
	TestEqual(FString::Printf(TEXT("Shop sample valid (%s)"), *FString::Join(ShopProblems, TEXT("; "))), ShopProblems.Num(), 0);

	FVekaDocumentBuilder B;
	B.SetRoot(B.Node(TEXT("VStack")).Children({
		B.Node(TEXT("Text")).Arg(TEXT("text"), B.ParamRef(TEXT("nope"))),
		B.SlotContent(TEXT("ghost")),
		B.Component(Assets.Item).Arg(TEXT("weight"), B.Lit(1))
			.Children({ B.Node(TEXT("Text")).InSlot(TEXT("footer")) })
			.On(TEXT("sell"), {}) }));
	const FString All = FString::Join(B.Release().Validate(), TEXT(" | "));
	TestTrue(TEXT("Unknown param"), All.Contains(TEXT("unknown param 'nope'")));
	TestTrue(TEXT("Undeclared slot"), All.Contains(TEXT("undeclared slot 'ghost'")));
	TestTrue(TEXT("Component param"), All.Contains(TEXT("has no param 'weight'")));
	TestTrue(TEXT("Component slot"), All.Contains(TEXT("has no slot 'footer'")));
	TestTrue(TEXT("Component event"), All.Contains(TEXT("has no event 'sell'")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewComponentsTest, "VekaUI.View.Components", VekaViewTestFlags)
bool FVekaViewComponentsTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	// Two instances of a component with its own state: state is per instance, parameters come from the caller.
	UVekaView* Counter = NewObject<UVekaView>(GetTransientPackage(), TEXT("VV_TestCounter"));
	Counter->Document = MakeCounterComponentDocument();
	Counter->AddToRoot();
	ON_SCOPE_EXIT { Counter->RemoveFromRoot(); };
	FVekaDocumentBuilder B;
	B.State(TEXT("prefix"), FVekaValue::String(TEXT("A")));
	B.SetRoot(B.Node(TEXT("VStack")).Children({
		B.Component(Counter).Arg(TEXT("label"), B.Var(TEXT("prefix"))),
		B.Component(Counter).Arg(TEXT("label"), B.Lit(TEXT("B"))) }));
	TSharedRef<FVekaViewStore> Store = MakeShared<FVekaViewStore>();
	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::Interpreter::MakeView(B.Release(), Store));
	FVekaRuntime& Runtime = *Host->GetRuntime();
	PaintOnce(Host);
	auto Root = [&Runtime]() { return Runtime.GetRootNode(); };

	Tap(Runtime, FindTextEq(Root(), TEXT("A: 0")));
	PaintOnce(Host);
	Tap(Runtime, FindTextEq(Root(), TEXT("A: 1")));
	PaintOnce(Host);
	TestNotNull(TEXT("First instance counted twice"), FindTextEq(Root(), TEXT("A: 2")).Get());
	TestNotNull(TEXT("Second instance untouched (own state)"), FindTextEq(Root(), TEXT("B: 0")).Get());
	TestFalse(TEXT("Component state is not in the caller's store"), Store->Contains(TEXT("taps")));

	Store->Set(TEXT("prefix"), FVekaValue::String(TEXT("Z")));
	Runtime.Flush();
	PaintOnce(Host);
	TestNotNull(TEXT("New parameter, state kept"), FindTextEq(Root(), TEXT("Z: 2")).Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewShopTest, "VekaUI.View.Shop", VekaViewTestFlags)
bool FVekaViewShopTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	const FShopAssets Assets;
	UVekaHostWidget* HostWidget = NewObject<UVekaHostWidget>(GetTransientPackage());
	UVekaSampleViewModel* Model = NewObject<UVekaSampleViewModel>(HostWidget);
	HostWidget->View = Assets.Shop;
	HostWidget->ViewModel = Model;

	UVekaTestEventSink* Sink = NewObject<UVekaTestEventSink>(GetTransientPackage());
	Sink->AddToRoot();
	ON_SCOPE_EXIT { Sink->RemoveFromRoot(); };
	FScriptDelegate Delegate;
	Delegate.BindUFunction(Sink, GET_FUNCTION_NAME_CHECKED(UVekaTestEventSink, HandleEvent));
	HostWidget->OnVekaEvent.Add(Delegate);

	const TSharedRef<SWidget> Widget = HostWidget->TakeWidget();
	FVekaRuntime& Runtime = *StaticCastSharedRef<SVekaHost>(Widget)->GetRuntime();
	PaintOnce(Widget);
	auto Root = [&Runtime]() { return Runtime.GetRootNode(); };

	TestNotNull(TEXT("Model value"), FindTextEq(Root(), TEXT("Aria")).Get());
	TestNotNull(TEXT("Localizable format"), FindTextEq(Root(), TEXT("120 gold")).Get());
	TestNotNull(TEXT("Component 1 (param from item.Name)"), FindTextEq(Root(), TEXT("Health potion")).Get());
	TestNotNull(TEXT("Component 2"), FindTextEq(Root(), TEXT("Oak shield")).Get());
	TestNotNull(TEXT("Component 3"), FindTextEq(Root(), TEXT("Lantern")).Get());
	TestNotNull(TEXT("Price through a component param"), FindTextEq(Root(), TEXT("60 gold")).Get());
	int32 Badges = 0;
	Find(Root(), [&Badges](const FVekaNode& Node)
	{
		const FVekaText* Text = Node.GetPrimitiveAs<FVekaText>();
		Badges += Text && Text->Text.ToString() == TEXT("NEW") ? 1 : 0;
		return false;
	});
	TestEqual(TEXT("Slot content only for the new item"), Badges, 1);

	// 140 > 120 gold: the lantern's component is disabled (counter-check of the event path).
	Tap(Runtime, BuyButtonOf(Root(), TEXT("Lantern")));
	TestEqual(TEXT("Disabled component emits nothing"), Sink->Events.Num(), 0);

	Tap(Runtime, BuyButtonOf(Root(), TEXT("Oak shield")));
	PaintOnce(Widget);
	if (TestEqual(TEXT("Component event handled by the caller, then sent to the host"), Sink->Events.Num(), 1))
	{
		TestEqual(TEXT("Host event"), Sink->Events[0], FName(TEXT("Buy")));
		TestEqual(TEXT("Payload = the component's price param"), Sink->Payloads[0], FVekaValue::Int(60));
	}
	TestNotNull(TEXT("Handler action ran in the caller's scope (item.Name)"), FindTextEq(Root(), TEXT("Last bought: Oak shield")).Get());

	// The Blueprint handler takes the gold; the list follows.
	UVekaSampleLibrary::HandleShopEvent(HostWidget, Sink->Events.Last(), Sink->Payloads.Last());
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Gold updated"), FindTextEq(Root(), TEXT("60 gold")).Get());

	// SetState / GetState through reflection, like Blueprint nodes.
	{
		UFunction* Function = HostWidget->FindFunctionChecked(TEXT("SetState"));
		struct FParams { FName Name; FVekaValue Value; } Params{ TEXT("gift"), FVekaValue::Bool(true) };
		HostWidget->ProcessEvent(Function, &Params);
	}
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Blueprint SetState re-evaluates"), FindTextEq(Root(), TEXT("A ribbon will be added.")).Get());
	{
		UFunction* Function = HostWidget->FindFunctionChecked(TEXT("GetState"));
		struct FParams { FName Name; FVekaValue ReturnValue; } Params{ TEXT("lastBought"), FVekaValue() };
		HostWidget->ProcessEvent(Function, &Params);
		TestEqual(TEXT("Blueprint GetState"), Params.ReturnValue.AsString(), FString(TEXT("Oak shield")));
	}

	// Silent model change (a Blueprint "Set" node) seen by polling.
	Model->PlayerName = TEXT("Kai");
	FTSTicker::GetCoreTicker().Tick(0.2f);
	Runtime.Flush();
	PaintOnce(Widget);
	TestNotNull(TEXT("Polling"), FindTextEq(Root(), TEXT("Kai")).Get());

	HostWidget->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaViewSampleBlueprintTest, "VekaUI.View.SampleBlueprint", VekaViewTestFlags)
bool FVekaViewSampleBlueprintTest::RunTest(const FString& Parameters)
{
	// The Widget Blueprint made by Veka.CreateSampleAssets: its event graph handles OnVekaEvent.
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/VekaSamples/WBP_VekaShop.WBP_VekaShop_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UWorld* World = GWorld;
	if (!WidgetClass || !World || !FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("WBP_VekaShop not found (run Veka.CreateSampleAssets): skipped."));
		return true;
	}
	UUserWidget* Widget = CreateWidget<UUserWidget>(World, WidgetClass);
	if (!TestNotNull(TEXT("Widget created"), Widget))
	{
		return false;
	}
	UVekaHostWidget* Host = Cast<UVekaHostWidget>(Widget->GetWidgetFromName(TEXT("VekaHost")));
	if (!TestNotNull(TEXT("Veka Host in the widget"), Host))
	{
		return false;
	}
	UVekaSampleViewModel* Model = Cast<UVekaSampleViewModel>(Host->ViewModel);
	if (!TestNotNull(TEXT("Sample model"), Model))
	{
		return false;
	}
	Model->Gold = 120;
	const TSharedRef<SWidget> Slate = Host->TakeWidget();
	FVekaRuntime& Runtime = *StaticCastSharedRef<SVekaHost>(Slate)->GetRuntime();
	PaintOnce(Slate);
	Tap(Runtime, BuyButtonOf(Runtime.GetRootNode(), TEXT("Health potion")));
	TestEqual(TEXT("The Blueprint graph took 25 gold"), Model->Gold, 95);
	Host->ReleaseSlateResources(true);
	return true;
}

#endif
