// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"

namespace
{
	constexpr EAutomationTestFlags VekaTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	TSharedRef<FVekaRuntime> MakeRuntime(const FVekaElement& Root)
	{
		TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
		Runtime->SetRoot(Root);
		return Runtime;
	}

	/** FVector2D comparison with a tolerance (TestEqual has no FVector2D overload on every version). */
	#define VEKA_TEST_VEC(What, A, B) TestTrue(What, FVector2D(A).Equals(FVector2D(B), 1.0e-4))

	FVekaRect ChildRect(const TSharedPtr<FVekaNode>& Node, int32 Index)
	{
		return Node->GetChildren()[Index]->GetArrangedRect();
	}
}

// --- Anima ----------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEasingTest, "VekaUI.Anima.Easing", VekaTestFlags)
bool FVekaEasingTest::RunTest(const FString& Parameters)
{
	const EVekaAnimationCurve Curves[] = {
		EVekaAnimationCurve::Linear, EVekaAnimationCurve::EaseIn, EVekaAnimationCurve::EaseOut, EVekaAnimationCurve::EaseInOut,
		EVekaAnimationCurve::CircEaseIn, EVekaAnimationCurve::CircEaseOut, EVekaAnimationCurve::CircEaseInOut, EVekaAnimationCurve::TimingCurve };

	for (EVekaAnimationCurve Curve : Curves)
	{
		TestEqual(TEXT("Starts at 0"), Veka::Easing::Evaluate(Curve, 0.0), 0.0, 1.0e-6);
		TestEqual(TEXT("Ends at 1"), Veka::Easing::Evaluate(Curve, 1.0), 1.0, 1.0e-6);
		double Previous = 0.0;
		for (int32 Step = 1; Step <= 50; ++Step)
		{
			const double Value = Veka::Easing::Evaluate(Curve, Step / 50.0);
			TestTrue(TEXT("Monotonic"), Value + 1.0e-6 >= Previous);
			Previous = Value;
		}
	}

	TestEqual(TEXT("Linear bezier"), Veka::Easing::CubicBezier(0.0, 0.0, 1.0, 1.0, 0.3), 0.3, 1.0e-4);
	TestEqual(TEXT("EaseInOut midpoint"), Veka::Easing::Evaluate(EVekaAnimationCurve::EaseInOut, 0.5), 0.5, 1.0e-4);
	TestTrue(TEXT("EaseIn is slow at start"), Veka::Easing::Evaluate(EVekaAnimationCurve::EaseIn, 0.25) < 0.25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSpringTest, "VekaUI.Anima.Spring", VekaTestFlags)
bool FVekaSpringTest::RunTest(const FString& Parameters)
{
	const FVekaAnimationSpec Spec = FVekaAnimationSpec::Spring(0.5, 0.7);

	// Analytic solution starts at 0 and rests at 1.
	TestEqual(TEXT("Spring starts at 0"), Veka::Spring::Evaluate(Spec.Mass, Spec.Stiffness, Spec.Damping, 0.0, 0.0).Position, 0.0, 1.0e-6);
	TestEqual(TEXT("Spring rests at 1"), Veka::Spring::Evaluate(Spec.Mass, Spec.Stiffness, Spec.Damping, 0.0, 5.0).Position, 1.0, 1.0e-3);

	// Underdamped spring overshoots.
	double Peak = 0.0;
	for (int32 Step = 0; Step < 200; ++Step)
	{
		Peak = FMath::Max(Peak, Veka::Spring::Evaluate(Spec.Mass, Spec.Stiffness, Spec.Damping, 0.0, Step / 100.0).Position);
	}
	TestTrue(TEXT("Bouncy spring overshoots"), Peak > 1.0);

	// Numerical stepping matches the analytic curve.
	double Position = 0.0;
	double Velocity = 0.0;
	Veka::Spring::Step(Position, Velocity, 1.0, Spec.Mass, Spec.Stiffness, Spec.Damping, 0.3);
	const double Expected = Veka::Spring::Evaluate(Spec.Mass, Spec.Stiffness, Spec.Damping, 0.0, 0.3).Position;
	TestEqual(TEXT("Stepping matches analytic"), Position, Expected, 0.02);

	// Critically damped: no overshoot.
	const FVekaAnimationSpec Smooth = FVekaAnimationSpec::Smooth();
	double MaxSmooth = 0.0;
	for (int32 Step = 0; Step < 300; ++Step)
	{
		MaxSmooth = FMath::Max(MaxSmooth, Veka::Spring::Evaluate(Smooth.Mass, Smooth.Stiffness, Smooth.Damping, 0.0, Step / 100.0).Position);
	}
	TestTrue(TEXT("Critically damped does not overshoot"), MaxSmooth <= 1.0 + 1.0e-3);

	TestTrue(TEXT("Settling time is finite"), Veka::Spring::EstimateSettlingTime(Spec.Mass, Spec.Stiffness, Spec.Damping) < 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSchedulerTest, "VekaUI.Anima.Scheduler", VekaTestFlags)
bool FVekaSchedulerTest::RunTest(const FString& Parameters)
{
	FVekaAnimaScheduler Scheduler;
	double Value = 0.0;
	bool bCompleted = false;
	Scheduler.Animate(FVekaAnimationSpec::Linear(1.0), 0.0, 10.0, [&Value](double V) { Value = V; }, [&bCompleted]() { bCompleted = true; });

	Scheduler.Tick(0.05);
	Scheduler.Tick(0.05);
	TestEqual(TEXT("Linear halfway after 0.1s of 1s"), Value, 1.0, 1.0e-6);
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Scheduler.Tick(0.05);
	}
	TestEqual(TEXT("Reaches target"), Value, 10.0, 1.0e-6);
	TestTrue(TEXT("Completion called"), bCompleted);
	TestFalse(TEXT("Nothing left"), Scheduler.IsAnimating());

	// Retarget on a channel keeps a single animation.
	int32 Channel = 0;
	Scheduler.Animate(FVekaAnimationSpec::Spring(), 0.0, 1.0, [&Value](double V) { Value = V; }, nullptr, &Channel);
	Scheduler.Tick(0.05);
	Scheduler.Animate(FVekaAnimationSpec::Spring(), 0.0, 2.0, [&Value](double V) { Value = V; }, nullptr, &Channel);
	TestEqual(TEXT("Retarget keeps one animation"), Scheduler.Num(), 1);
	for (int32 Step = 0; Step < 200; ++Step)
	{
		Scheduler.Tick(1.0 / 60.0);
	}
	TestEqual(TEXT("Retargeted spring settles on new target"), Value, 2.0, 1.0e-2);
	return true;
}

// --- Vector -----------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSvgTest, "VekaUI.Vector.Svg", VekaTestFlags)
bool FVekaSvgTest::RunTest(const FString& Parameters)
{
	FVekaPath Path;
	FString Error;
	TestTrue(TEXT("Parses absolute"), FVekaPath::FromSvg(TEXT("M0 0 L10 0 L10 10 L0 10 Z"), Path, &Error));
	const FBox2D Bounds = Path.GetBounds();
	VEKA_TEST_VEC(TEXT("Bounds min"), Bounds.Min, FVector2D(0.0, 0.0));
	VEKA_TEST_VEC(TEXT("Bounds max"), Bounds.Max, FVector2D(10.0, 10.0));
	TestEqual(TEXT("Perimeter"), Path.GetLength(), 40.0, 1.0e-6);

	TestTrue(TEXT("Parses relative + H/V"), FVekaPath::FromSvg(TEXT("m5,5 h10 v10 h-10 z"), Path, &Error));
	VEKA_TEST_VEC(TEXT("Relative bounds"), Path.GetBounds().Max, FVector2D(15.0, 15.0));

	TestTrue(TEXT("Parses arcs with packed flags"), FVekaPath::FromSvg(TEXT("M0 10 a10 10 0 1110 10"), Path, &Error));
	TestTrue(TEXT("Parses curves"), FVekaPath::FromSvg(TEXT("M0 0 C10 0 10 10 20 10 S30 20 40 20 Q50 0 60 20 T80 20"), Path, &Error));
	TestTrue(TEXT("Parses exponents"), FVekaPath::FromSvg(TEXT("M1e1 0 L2.5e1 -1.5E1"), Path, &Error));
	VEKA_TEST_VEC(TEXT("Exponent value"), Path.Commands[1].P0, FVector2D(25.0, -15.0));

	TestFalse(TEXT("Rejects garbage"), FVekaPath::FromSvg(TEXT("M0 0 L10 # 5"), Path, &Error));
	TestFalse(TEXT("Rejects missing command"), FVekaPath::FromSvg(TEXT("10 10"), Path, &Error));

	// Half circle arc: length ~ pi * r.
	TestTrue(TEXT("Arc"), FVekaPath::FromSvg(TEXT("M0 0 A10 10 0 0 1 20 0"), Path, &Error));
	TestEqual(TEXT("Half circle length"), Path.GetLength(0.01), UE_DOUBLE_PI * 10.0, 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaPathOpsTest, "VekaUI.Vector.PathOps", VekaTestFlags)
bool FVekaPathOpsTest::RunTest(const FString& Parameters)
{
	FVekaPath Square;
	Square.AddRect(FVekaRect(FVector2D::ZeroVector, FVector2D(10.0, 10.0)));
	TArray<FVekaPolyline> Polylines;
	Square.Flatten(Polylines);

	TArray<FVekaPolyline> Half;
	Veka::PathOps::Trim(Polylines, 0.0, 0.5, Half);
	double HalfLength = 0.0;
	for (const FVekaPolyline& Polyline : Half)
	{
		HalfLength += Veka::PathOps::PolylineLength(Polyline);
	}
	TestEqual(TEXT("Trim to half"), HalfLength, 20.0, 1.0e-6);

	TArray<FVekaPolyline> Dashes;
	const float Pattern[] = { 5.f, 5.f };
	Veka::PathOps::Dash(Polylines, Pattern, 0.0, Dashes);
	TestEqual(TEXT("Dash count on 40-unit perimeter"), Dashes.Num(), 4);

	const FVector2D Concave[] = { FVector2D(0, 0), FVector2D(10, 0), FVector2D(10, 10), FVector2D(5, 4), FVector2D(0, 10) };
	TArray<int32> Indices;
	TestTrue(TEXT("Triangulates concave polygon"), Veka::PathOps::Triangulate(Concave, Indices));
	TestEqual(TEXT("n-2 triangles"), Indices.Num(), 9);

	TestTrue(TEXT("Contains inside"), Square.ContainsPoint(FVector2D(5.0, 5.0)));
	TestFalse(TEXT("Not outside"), Square.ContainsPoint(FVector2D(15.0, 5.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaShapeHitTest, "VekaUI.Vector.ShapeHitTest", VekaTestFlags)
bool FVekaShapeHitTest::RunTest(const FString& Parameters)
{
	const FVekaRect Bounds(FVector2D::ZeroVector, FVector2D(100.0, 50.0));

	const FVekaShape Capsule = FVekaShape::Capsule();
	TestTrue(TEXT("Capsule center"), Capsule.Contains(FVector2D(50.0, 25.0), Bounds));
	TestFalse(TEXT("Capsule corner is outside"), Capsule.Contains(FVector2D(2.0, 2.0), Bounds));

	const FVekaShape Circle = FVekaShape::Circle();
	TestTrue(TEXT("Circle uses centered square"), Circle.Contains(FVector2D(50.0, 25.0), Bounds));
	TestFalse(TEXT("Circle excludes the sides"), Circle.Contains(FVector2D(10.0, 25.0), Bounds));

	const FVekaShape Rounded = FVekaShape::RoundedRectangle(FVekaCornerRadii(20.f, 0.f, 0.f, 0.f));
	TestFalse(TEXT("Rounded top-left corner"), Rounded.Contains(FVector2D(1.0, 1.0), Bounds));
	TestTrue(TEXT("Sharp top-right corner"), Rounded.Contains(FVector2D(99.0, 1.0), Bounds));

	const FVekaShape Ellipse = FVekaShape::Ellipse();
	TestTrue(TEXT("Ellipse inside"), Ellipse.Contains(FVector2D(90.0, 25.0), Bounds));
	TestFalse(TEXT("Ellipse outside"), Ellipse.Contains(FVector2D(95.0, 5.0), Bounds));
	return true;
}

// --- Layout -----------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaStackLayoutTest, "VekaUI.Layout.Stack", VekaTestFlags)
bool FVekaStackLayoutTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	// [50] spacer [30] in 200: spacer takes 200 - 50 - 30 - 2*10 = 100.
	TSharedRef<FVekaRuntime> Runtime = MakeRuntime(HStack(10)(
		Color(FLinearColor::Red).Frame(50, 20),
		Spacer(0),
		Color(FLinearColor::Blue).Frame(30, 20)));
	TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();

	const FVector2D Size = Root->Measure(FVekaProposal::FromSize(FVector2D(200.0, 100.0)));
	VEKA_TEST_VEC(TEXT("HStack fills proposal with a spacer"), Size, FVector2D(200.0, 20.0));
	Root->ArrangeRecursive(Size);
	TestTrue(TEXT("First child"), ChildRect(Root, 0).Equals(FVekaRect(FVector2D(0, 0), FVector2D(50, 20))));
	TestTrue(TEXT("Spacer"), ChildRect(Root, 1).Equals(FVekaRect(FVector2D(60, 10), FVector2D(100, 0))));
	TestTrue(TEXT("Last child"), ChildRect(Root, 2).Equals(FVekaRect(FVector2D(170, 0), FVector2D(30, 20))));

	// Ideal size without proposal: spacer at min length.
	VEKA_TEST_VEC(TEXT("Ideal HStack"), Root->Measure(FVekaProposal::Unspecified()), FVector2D(100.0, 20.0));

	// VStack cross alignment.
	TSharedRef<FVekaRuntime> VRuntime = MakeRuntime(VStack(4, EVekaHAlign::Trailing)(
		Color(FLinearColor::Red).Frame(40, 10),
		Color(FLinearColor::Red).Frame(80, 10)));
	TSharedPtr<FVekaNode> VRoot = VRuntime->GetRootNode();
	const FVector2D VSize = VRoot->Measure(FVekaProposal::Unspecified());
	VEKA_TEST_VEC(TEXT("VStack size"), VSize, FVector2D(80.0, 24.0));
	VRoot->ArrangeRecursive(VSize);
	TestTrue(TEXT("Trailing alignment"), ChildRect(VRoot, 0).Equals(FVekaRect(FVector2D(40, 0), FVector2D(40, 10))));
	TestTrue(TEXT("Second row"), ChildRect(VRoot, 1).Equals(FVekaRect(FVector2D(0, 14), FVector2D(80, 10))));

	// ZStack: union of sizes, centered.
	TSharedRef<FVekaRuntime> ZRuntime = MakeRuntime(ZStack()(
		Color(FLinearColor::Red).Frame(100, 40),
		Color(FLinearColor::Red).Frame(20, 20)));
	TSharedPtr<FVekaNode> ZRoot = ZRuntime->GetRootNode();
	const FVector2D ZSize = ZRoot->Measure(FVekaProposal::Unspecified());
	VEKA_TEST_VEC(TEXT("ZStack size"), ZSize, FVector2D(100.0, 40.0));
	ZRoot->ArrangeRecursive(ZSize);
	TestTrue(TEXT("ZStack centers"), ChildRect(ZRoot, 1).Equals(FVekaRect(FVector2D(40, 10), FVector2D(20, 20))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaModifierLayoutTest, "VekaUI.Layout.Modifiers", VekaTestFlags)
bool FVekaModifierLayoutTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	// Padding adds to the size; content rect is inset.
	TSharedRef<FVekaRuntime> Runtime = MakeRuntime(Color(FLinearColor::Red).Frame(50, 20).Padding(FMargin(10, 5)));
	TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();
	const FVector2D Size = Root->Measure(FVekaProposal::Unspecified());
	VEKA_TEST_VEC(TEXT("Padding size"), Size, FVector2D(70.0, 30.0));
	Root->Arrange(Size);
	TestTrue(TEXT("Content rect inset"), Root->GetContentRect().Equals(FVekaRect(FVector2D(10, 5), FVector2D(50, 20))));

	// FillWidth takes the proposed width and aligns the content.
	TSharedRef<FVekaRuntime> FillRuntime = MakeRuntime(Color(FLinearColor::Red).Frame(50, 20).FillWidth(FVekaAlignment::Leading()));
	TSharedPtr<FVekaNode> FillRoot = FillRuntime->GetRootNode();
	const FVector2D FillSize = FillRoot->Measure(FVekaProposal::FromSize(FVector2D(300.0, 100.0)));
	VEKA_TEST_VEC(TEXT("FillWidth"), FillSize, FVector2D(300.0, 20.0));
	FillRoot->Arrange(FillSize);
	TestTrue(TEXT("Leading content"), FillRoot->GetContentRect().Equals(FVekaRect(FVector2D(0, 0), FVector2D(50, 20))));

	// A layout-priority child is served first.
	TSharedRef<FVekaRuntime> PriorityRuntime = MakeRuntime(HStack(0)(
		Color(FLinearColor::Red),
		Color(FLinearColor::Blue).FlexFrame(-1, 150, -1, -1).LayoutPriority(1)));
	TSharedPtr<FVekaNode> PriorityRoot = PriorityRuntime->GetRootNode();
	PriorityRoot->ArrangeRecursive(FVector2D(200.0, 10.0));
	TestEqual(TEXT("Priority child gets its max first"), ChildRect(PriorityRoot, 1).Size.X, 150.0);
	TestEqual(TEXT("Other child gets the rest"), ChildRect(PriorityRoot, 0).Size.X, 50.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaGridLayoutTest, "VekaUI.Layout.Grid", VekaTestFlags)
bool FVekaGridLayoutTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	TSharedRef<FVekaRuntime> Runtime = MakeRuntime(Grid({ FVekaGridItem::Adaptive(50.f) }, 10.f, 10.f)(
		ForEach(5, [](int32) { return Color(FLinearColor::Red).FlexFrame(-1, Veka::Infinity, 20, 20); })));
	TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();

	// 230 wide: (230 + 10) / (50 + 10) = 4 columns of 50.
	const FVector2D Size = Root->Measure(FVekaProposal(230.0, TOptional<double>()));
	VEKA_TEST_VEC(TEXT("Grid size"), Size, FVector2D(230.0, 50.0));
	Root->ArrangeRecursive(Size);
	TestTrue(TEXT("Second column"), ChildRect(Root, 1).Equals(FVekaRect(FVector2D(60, 0), FVector2D(50, 20))));
	TestTrue(TEXT("Wraps to second row"), ChildRect(Root, 4).Equals(FVekaRect(FVector2D(0, 30), FVector2D(50, 20))));
	return true;
}

// --- State & reconciliation ------------------------------------------------------------------------------

namespace
{
	/** Exposes its state handle to the test. */
	class FTestCounterView : public FVekaView
	{
		VEKA_VIEW(FTestCounterView)

	public:
		TSharedRef<TVekaState<int32>> Out;
		explicit FTestCounterView(TSharedRef<TVekaState<int32>> InOut) : Out(InOut) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<int32> Count = Ctx.State<int32>(TEXT("Count"), 0);
			*Out = Count;
			return Veka::Text(FString::FromInt(Count.Get())).OnTap([Count]() { Count.Set(Count.Get() + 1); });
		}
	};

	class FTestListView : public FVekaView
	{
		VEKA_VIEW(FTestListView)

	public:
		TSharedRef<TVekaState<TArray<int32>>> Out;
		explicit FTestListView(TSharedRef<TVekaState<TArray<int32>>> InOut) : Out(InOut) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<TArray<int32>> Items = Ctx.State<TArray<int32>>(TEXT("Items"), TArray<int32>{ 1, 2, 3 });
			*Out = Items;
			return Veka::VStack()(
				Veka::ForEach(Items.Get(), [](int32 Item) { return Item; }, [](int32 Item) { return Veka::Text(FString::FromInt(Item)); }));
		}
	};

	FString TextOf(const TSharedRef<FVekaNode>& Node)
	{
		const FVekaText* Text = Node->GetPrimitiveAs<FVekaText>();
		return Text ? Text->Text.ToString() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaStateTest, "VekaUI.Core.State", VekaTestFlags)
bool FVekaStateTest::RunTest(const FString& Parameters)
{
	TSharedRef<TVekaState<int32>> Handle = MakeShared<TVekaState<int32>>();
	TSharedRef<FVekaRuntime> Runtime = MakeRuntime(Veka::View<FTestCounterView>(Handle));
	TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();

	TestEqual(TEXT("Initial body"), TextOf(Root->GetChildren()[0]), FString(TEXT("0")));
	const TSharedRef<FVekaNode> TextNode = Root->GetChildren()[0];

	// Gesture -> state -> flush -> new body. Same node instance is reused.
	const TArray<TFunction<void()>> Taps = TextNode->GetGestures().OnTap;
	TestEqual(TEXT("One tap handler"), Taps.Num(), 1);
	Taps[0]();
	Taps[0]();
	TestTrue(TEXT("Pending work after state change"), Runtime->HasPendingWork());
	Runtime->Flush();
	TestEqual(TEXT("Body re-evaluated once with both changes"), TextOf(Root->GetChildren()[0]), FString(TEXT("2")));
	TestTrue(TEXT("Text node reused"), Root->GetChildren()[0] == TextNode);

	// Setting the same value does not schedule work.
	Handle->Set(2);
	TestFalse(TEXT("No-op set"), Runtime->HasPendingWork());

	// State survives a parent re-render with the same root.
	Runtime->SetRoot(Veka::View<FTestCounterView>(Handle));
	TestEqual(TEXT("State kept across SetRoot"), TextOf(Runtime->GetRootNode()->GetChildren()[0]), FString(TEXT("2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaReconcileKeysTest, "VekaUI.Core.ReconcileKeys", VekaTestFlags)
bool FVekaReconcileKeysTest::RunTest(const FString& Parameters)
{
	TSharedRef<TVekaState<TArray<int32>>> Handle = MakeShared<TVekaState<TArray<int32>>>();
	TSharedRef<FVekaRuntime> Runtime = MakeRuntime(Veka::View<FTestListView>(Handle));
	TSharedPtr<FVekaNode> Stack = Runtime->GetRootNode()->GetChildren()[0];
	TestEqual(TEXT("Three items"), Stack->GetChildren().Num(), 3);

	TMap<FString, FVekaNode*> Before;
	for (const TSharedRef<FVekaNode>& Child : Stack->GetChildren())
	{
		Before.Add(TextOf(Child), &Child.Get());
	}

	Handle->Set(TArray<int32>{ 3, 1, 2, 4 });
	Runtime->Flush();

	const TArray<TSharedRef<FVekaNode>>& After = Stack->GetChildren();
	TestEqual(TEXT("Four items"), After.Num(), 4);
	TestEqual(TEXT("Order follows data"), TextOf(After[0]), FString(TEXT("3")));
	TestTrue(TEXT("Item 3 node kept"), &After[0].Get() == Before.FindRef(TEXT("3")));
	TestTrue(TEXT("Item 1 node kept"), &After[1].Get() == Before.FindRef(TEXT("1")));
	TestTrue(TEXT("Item 4 is new"), !Before.Contains(TextOf(After[3])));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTextLayoutTest, "VekaUI.Primitives.TextLayout", VekaTestFlags)
bool FVekaTextLayoutTest::RunTest(const FString& Parameters)
{
	// 10 units per character.
	auto Measure = [](const FString& Line) { return Line.Len() * 10.0; };

	TArray<FString> Lines = Veka::TextLayout::BreakLines(TEXT("hello big world"), 100.0, 0, true, Measure);
	TestEqual(TEXT("Wraps into two lines"), Lines.Num(), 2);
	TestEqual(TEXT("First line"), Lines[0], FString(TEXT("hello big")));

	Lines = Veka::TextLayout::BreakLines(TEXT("hello big world again"), 100.0, 1, true, Measure);
	TestEqual(TEXT("Line limit"), Lines.Num(), 1);
	TestTrue(TEXT("Ellipsis"), Lines[0].EndsWith(TEXT("…")));

	Lines = Veka::TextLayout::BreakLines(TEXT("a\nb"), -1.0, 0, true, Measure);
	TestEqual(TEXT("Explicit newline"), Lines.Num(), 2);

	Lines = Veka::TextLayout::BreakLines(TEXT("abcdefghijklmnop"), 50.0, 0, false, Measure);
	TestEqual(TEXT("Long word split by characters"), Lines.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSchemaTest, "VekaUI.Core.Schema", VekaTestFlags)
bool FVekaSchemaTest::RunTest(const FString& Parameters)
{
	FVekaSchema& Schema = FVekaSchema::Get();
	Schema.Refresh();
	TestNotNull(TEXT("Text registered"), Schema.Find(TEXT("Text"), EVekaSchemaKind::Primitive));
	TestNotNull(TEXT("Stack registered"), Schema.Find(TEXT("Stack"), EVekaSchemaKind::Primitive));
	TestNotNull(TEXT("padding registered"), Schema.Find(TEXT("padding"), EVekaSchemaKind::Modifier));
	TestNotNull(TEXT("background registered"), Schema.Find(TEXT("background"), EVekaSchemaKind::Modifier));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
