// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Curves/CurveFloat.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaAnimaTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/** The scheduler clamps frames to 0.1 s: advance in steps. */
	void Advance(FVekaRuntime& Runtime, double Seconds)
	{
		const double Step = 0.05;
		for (double Done = 0.0; Done < Seconds - 1.0e-6; Done += Step)
		{
			Runtime.Tick(FMath::Min(Step, Seconds - Done));
		}
	}

	template <typename T>
	using TStateOut = TSharedRef<TVekaState<T>>;

	/** Rectangle whose width and color follow a bool, animated with `.Animation(spec, value)`. */
	class FImplicitView : public FVekaView
	{
		VEKA_VIEW(FImplicitView)
	public:
		TStateOut<bool> Out;
		bool bAnimated;
		FImplicitView(TStateOut<bool> InOut, bool bInAnimated) : Out(InOut), bAnimated(bInAnimated) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Wide = Ctx.State<bool>(TEXT("Wide"), false);
			*Out = Wide;
			FVekaElement Shape = Veka::Rectangle().Fill(Wide.Get() ? FLinearColor::Red : FLinearColor::Blue).Frame(Wide.Get() ? 200.0 : 100.0, 50.0);
			return bAnimated ? MoveTemp(Shape).Animation(FVekaAnimationSpec::Linear(1.0), Wide.Get()) : Shape;
		}
	};

	/** Circle offset by a state (no animation modifier: WithAnimation drives it). */
	class FOffsetView : public FVekaView
	{
		VEKA_VIEW(FOffsetView)
	public:
		TStateOut<float> Out;
		explicit FOffsetView(TStateOut<float> InOut) : Out(InOut) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<float> X = Ctx.State<float>(TEXT("X"), 0.f);
			*Out = X;
			return Veka::Circle().Frame(10, 10).Offset(FVector2D(X.Get(), 0.0));
		}
	};

	/** Keyed list of fixed-size rows. */
	class FListView : public FVekaView
	{
		VEKA_VIEW(FListView)
	public:
		TStateOut<TArray<int32>> Out;
		FVekaTransition Transition;
		FListView(TStateOut<TArray<int32>> InOut, const FVekaTransition& InTransition) : Out(InOut), Transition(InTransition) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<TArray<int32>> Items = Ctx.State<TArray<int32>>(TEXT("Items"), TArray<int32>{ 1, 2, 3 });
			*Out = Items;
			const FVekaTransition RowTransition = Transition;
			return Veka::VStack(10)(Veka::ForEach(Items.Get(), [](int32 Item) { return Item; }, [RowTransition](int32 Item)
			{
				return Veka::Rectangle().Frame(100, 20).Transition(RowTransition);
			}));
		}
	};

	const FVekaFrameModifier* FrameOf(const TSharedPtr<FVekaNode>& Node)
	{
		return Node->FindModifier<FVekaFrameModifier>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaImplicitTest, "VekaUI.Anima.Implicit", VekaAnimaTestFlags)
bool FVekaAnimaImplicitTest::RunTest(const FString& Parameters)
{
	TStateOut<bool> Wide = MakeShared<TVekaState<bool>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FImplicitView>(Wide, true));
	const TSharedPtr<FVekaNode> Shape = Runtime->GetRootNode()->GetChildren()[0];
	TestEqual(TEXT("Starts at 100"), FrameOf(Shape)->Width, 100.0, 1.0e-6);

	Wide->Set(true);
	Runtime->Flush();
	TestTrue(TEXT("Same node"), Runtime->GetRootNode()->GetChildren()[0] == Shape.ToSharedRef());
	TestEqual(TEXT("Presented value starts where it was"), FrameOf(Shape)->Width, 100.0, 1.0e-6);
	TestEqual(TEXT("Target is the new value"), Shape->GetTargetModifiers()[0].Get<FVekaFrameModifier>().Width, 200.0, 1.0e-6);

	Advance(*Runtime, 0.5);
	TestEqual(TEXT("Halfway (linear)"), FrameOf(Shape)->Width, 150.0, 2.0);
	const FLinearColor Mid = Shape->GetPrimitiveAs<FVekaShapeView>()->Fill.Color;
	TestTrue(TEXT("Color is between blue and red"), Mid.R > 0.05f && Mid.B > 0.05f);

	Advance(*Runtime, 0.6);
	TestEqual(TEXT("Reaches the target"), FrameOf(Shape)->Width, 200.0, 1.0e-3);
	TestTrue(TEXT("Color reaches red"), Shape->GetPrimitiveAs<FVekaShapeView>()->Fill.Color.Equals(FLinearColor::Red, 1.0e-3f));
	TestEqual(TEXT("Animations cleaned up"), Runtime->GetImplicit().NumValueAnimations(), 0);

	// Without a transaction, changes snap.
	TStateOut<bool> Plain = MakeShared<TVekaState<bool>>();
	TSharedRef<FVekaRuntime> PlainRuntime = MakeShared<FVekaRuntime>();
	PlainRuntime->SetRoot(Veka::View<FImplicitView>(Plain, false));
	Plain->Set(true);
	PlainRuntime->Flush();
	TestEqual(TEXT("Snaps without animation"), FrameOf(PlainRuntime->GetRootNode()->GetChildren()[0])->Width, 200.0, 1.0e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaWithAnimationTest, "VekaUI.Anima.WithAnimation", VekaAnimaTestFlags)
bool FVekaAnimaWithAnimationTest::RunTest(const FString& Parameters)
{
	TStateOut<float> X = MakeShared<TVekaState<float>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FOffsetView>(X));
	const TSharedPtr<FVekaNode> Circle = Runtime->GetRootNode()->GetChildren()[0];
	auto OffsetX = [&Circle]() { return Circle->FindModifier<FVekaOffsetModifier>()->Offset.X; };

	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { X->Set(100.f); });
	TestNull(TEXT("Transaction closed after the block"), Veka::Anima::GetActiveTransaction());
	Runtime->Flush();
	TestEqual(TEXT("Starts at 0"), OffsetX(), 0.0, 1.0e-6);
	Advance(*Runtime, 0.5);
	TestEqual(TEXT("Halfway"), OffsetX(), 50.0, 2.0);

	// Retarget mid-flight: continues from the presented value, no jump.
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { X->Set(0.f); });
	Runtime->Flush();
	TestEqual(TEXT("No jump on retarget"), OffsetX(), 50.0, 2.0);
	Advance(*Runtime, 0.5);
	TestEqual(TEXT("Going back"), OffsetX(), 25.0, 2.0);
	Advance(*Runtime, 0.6);
	TestEqual(TEXT("Back at 0"), OffsetX(), 0.0, 1.0e-3);

	// Outside WithAnimation: snaps.
	X->Set(42.f);
	Runtime->Flush();
	TestEqual(TEXT("Snaps outside WithAnimation"), OffsetX(), 42.0, 1.0e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaTransitionsTest, "VekaUI.Anima.Transitions", VekaAnimaTestFlags)
bool FVekaAnimaTransitionsTest::RunTest(const FString& Parameters)
{
	TStateOut<TArray<int32>> Items = MakeShared<TVekaState<TArray<int32>>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FListView>(Items, FVekaTransition::Combined(FVekaTransition::Opacity(), FVekaTransition::Scale(0.5f))));
	const TSharedPtr<FVekaNode> Stack = Runtime->GetRootNode()->GetChildren()[0];
	TestEqual(TEXT("Three rows"), Stack->GetChildren().Num(), 3);
	TestEqual(TEXT("Initial rows do not transition"), Stack->GetChildren()[0]->GetTransitionProgress(), 1.f, 1.0e-6f);

	// Removal: kept as a ghost while it leaves.
	const TSharedRef<FVekaNode> Second = Stack->GetChildren()[1];
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { Items->Set(TArray<int32>{ 1, 3 }); });
	Runtime->Flush();
	TestEqual(TEXT("Two live rows"), Stack->GetChildren().Num(), 2);
	TestEqual(TEXT("One leaving row"), Stack->GetRemovedChildren().Num(), 1);
	TestTrue(TEXT("Leaving row is removing"), Second->IsRemoving());
	Advance(*Runtime, 0.5);
	TestEqual(TEXT("Removal halfway"), Second->GetTransitionProgress(), 0.5f, 0.05f);
	TestEqual(TEXT("Opacity follows"), Second->GetTransitionOpacity(), 0.5f, 0.05f);
	const FSlateRenderTransform Transform = Second->ComputeRenderTransform(FVector2D(100.0, 20.0));
	TestEqual(TEXT("Scale effect halfway"), Transform.GetMatrix().GetScale().GetVector().X, 0.75f, 0.03f);
	Advance(*Runtime, 0.6);
	TestEqual(TEXT("Ghost gone"), Stack->GetRemovedChildren().Num(), 0);
	TestNull(TEXT("Detached"), Second->GetParent());

	// Insertion: starts away, ends at identity.
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { Items->Set(TArray<int32>{ 1, 3, 4 }); });
	Runtime->Flush();
	const TSharedRef<FVekaNode> Inserted = Stack->GetChildren()[2];
	TestEqual(TEXT("Inserted starts at 0"), Inserted->GetTransitionProgress(), 0.f, 1.0e-6f);
	Advance(*Runtime, 1.1);
	TestEqual(TEXT("Inserted at identity"), Inserted->GetTransitionProgress(), 1.f, 1.0e-6f);
	TestEqual(TEXT("Opacity back to 1"), Inserted->GetTransitionOpacity(), 1.f, 1.0e-6f);

	// Removed and re-added during its removal: the same node comes back.
	const TSharedRef<FVekaNode> First = Stack->GetChildren()[0];
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { Items->Set(TArray<int32>{ 3, 4 }); });
	Runtime->Flush();
	Advance(*Runtime, 0.3);
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { Items->Set(TArray<int32>{ 1, 3, 4 }); });
	Runtime->Flush();
	TestTrue(TEXT("Revived node reused"), Stack->GetChildren()[0] == First);
	TestFalse(TEXT("No longer removing"), First->IsRemoving());

	// Without a transaction: immediate.
	Items->Set(TArray<int32>{ 1 });
	Runtime->Flush();
	Advance(*Runtime, 1.1);
	TestEqual(TEXT("No ghosts without transaction"), Stack->GetRemovedChildren().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaLayoutTest, "VekaUI.Anima.LayoutTransition", VekaAnimaTestFlags)
bool FVekaAnimaLayoutTest::RunTest(const FString& Parameters)
{
	TStateOut<TArray<int32>> Items = MakeShared<TVekaState<TArray<int32>>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FListView>(Items, FVekaTransition::Opacity()));
	const TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();
	const TSharedPtr<FVekaNode> Stack = Root->GetChildren()[0];
	const FVector2D Size(100.0, 200.0);
	Root->ArrangeRecursive(Size);
	const TSharedRef<FVekaNode> Row1 = Stack->GetChildren()[0];
	TestEqual(TEXT("Row 1 at the top"), Row1->GetArrangedRect().Position.Y, 0.0, 1.0e-6);

	// A row inserted above pushes row 1 down: its position animates.
	Veka::WithAnimation(FVekaAnimationSpec::Linear(1.0), [&] { Items->Set(TArray<int32>{ 0, 1, 2, 3 }); });
	Runtime->Flush();
	Root->ArrangeRecursive(Size);
	TestEqual(TEXT("Row 1 starts at its old place"), Row1->GetArrangedRect().Position.Y, 0.0, 1.0e-3);
	Advance(*Runtime, 0.5);
	Root->ArrangeRecursive(Size);
	TestEqual(TEXT("Row 1 halfway"), Row1->GetArrangedRect().Position.Y, 15.0, 1.5);
	Advance(*Runtime, 0.6);
	Root->ArrangeRecursive(Size);
	TestEqual(TEXT("Row 1 at its new place"), Row1->GetArrangedRect().Position.Y, 30.0, 1.0e-3);
	TestEqual(TEXT("New row placed directly"), Stack->GetChildren()[0]->GetArrangedRect().Position.Y, 0.0, 1.0e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaTimelineTest, "VekaUI.Anima.Timeline", VekaAnimaTestFlags)
bool FVekaAnimaTimelineTest::RunTest(const FString& Parameters)
{
	FVekaTimelineData Data;
	Data.AddTrack(TEXT("X"), { FVekaTimelineKey(0.f, 0.f, EVekaAnimationCurve::Linear), FVekaTimelineKey(1.f, 100.f) });
	Data.AddEvent(0.5f, TEXT("Half")).AddEvent(1.f, TEXT("End"));
	TestEqual(TEXT("Length from keys"), Data.GetLength(), 1.f, 1.0e-6f);
	TestEqual(TEXT("Evaluate"), Data.Evaluate(TEXT("X"), 0.25f), 25.f, 1.0e-3f);
	TestEqual(TEXT("Clamped after the end"), Data.Evaluate(TEXT("X"), 3.f), 100.f, 1.0e-3f);

	TSharedRef<FVekaTimelinePlayer> Player = MakeShared<FVekaTimelinePlayer>();
	Player->Data = Data;
	TArray<FName> Fired;
	Player->OnEvent = [&Fired](FName Name) { Fired.Add(Name); };
	Player->Play();
	Player->Advance(0.6);
	TestEqual(TEXT("Value while playing"), Player->Value(TEXT("X")), 60.f, 1.0e-3f);
	TestEqual(TEXT("Half fired"), Fired, TArray<FName>{ TEXT("Half") });
	Player->Advance(0.6);
	TestFalse(TEXT("Stops at the end"), Player->IsPlaying());
	TestEqual(TEXT("End fired once"), Fired, TArray<FName>{ TEXT("Half"), TEXT("End") });

	Fired.Reset();
	Player->Reverse();
	Player->Advance(0.7);
	TestEqual(TEXT("Plays backward"), Player->GetTime(), 0.3f, 1.0e-3f);
	TestEqual(TEXT("Half fired backward"), Fired, TArray<FName>{ TEXT("Half") });

	Player->Seek(0.9f);
	TestEqual(TEXT("Seek"), Player->Value(TEXT("X")), 90.f, 1.0e-3f);

	Data.bLoop = true;
	TSharedRef<FVekaTimelinePlayer> Looping = MakeShared<FVekaTimelinePlayer>();
	Looping->Data = Data;
	int32 HalfCount = 0;
	Looping->OnEvent = [&HalfCount](FName Name) { HalfCount += Name == TEXT("Half") ? 1 : 0; };
	Looping->Play();
	Looping->Advance(1.2);
	TestTrue(TEXT("Still playing (loop)"), Looping->IsPlaying());
	TestEqual(TEXT("Wrapped time"), Looping->GetTime(), 0.2f, 1.0e-3f);
	Looping->Advance(0.4);
	TestEqual(TEXT("Half fired on each loop"), HalfCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAnimaCurveTest, "VekaUI.Anima.CustomCurveAndTypes", VekaAnimaTestFlags)
bool FVekaAnimaCurveTest::RunTest(const FString& Parameters)
{
	UCurveFloat* Curve = NewObject<UCurveFloat>(GetTransientPackage());
	Curve->FloatCurve.AddKey(0.f, 0.f);
	Curve->FloatCurve.AddKey(1.f, 0.9f);
	Curve->FloatCurve.AddKey(2.f, 1.f);
	for (auto It = Curve->FloatCurve.GetKeyHandleIterator(); It; ++It)
	{
		Curve->FloatCurve.SetKeyInterpMode(*It, RCIM_Linear);
	}
	const FVekaAnimationSpec Spec = FVekaAnimationSpec::FromCurve(Curve, 1.0);
	TestEqual(TEXT("Curve time range mapped to progress"), Spec.EvaluateProgress(0.5), 0.9, 1.0e-3);

	FVekaAnimaScheduler Scheduler;
	double Value = 0.0;
	Scheduler.Animate(Spec, 0.0, 100.0, [&Value](double V) { Value = V; });
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Scheduler.Tick(0.05);
	}
	TestEqual(TEXT("Scheduler follows the curve"), Value, 90.0, 1.0);

	// TVekaState::Animate on non-scalar types.
	TStateOut<float> Unused = MakeShared<TVekaState<float>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FOffsetView>(Unused));
	FVekaContext Ctx(*Runtime->GetRootNode(), Runtime->GetRootNode()->GetEnvironment());
	const TVekaState<FVector2D> Position = Ctx.State<FVector2D>(TEXT("Position"), FVector2D::ZeroVector);
	const TVekaState<FLinearColor> Tint = Ctx.State<FLinearColor>(TEXT("Tint"), FLinearColor::Black);
	Position.Animate(FVector2D(100.0, 50.0), FVekaAnimationSpec::Linear(1.0));
	Tint.Animate(FLinearColor::White, FVekaAnimationSpec::Linear(1.0));
	Advance(*Runtime, 0.5);
	TestTrue(TEXT("Vector halfway"), Position.Get().Equals(FVector2D(50.0, 25.0), 2.0));
	TestTrue(TEXT("Color between black and white"), Tint.Get().R > 0.05f && Tint.Get().R < 0.95f);
	Advance(*Runtime, 0.6);
	TestTrue(TEXT("Vector arrived"), Position.Get().Equals(FVector2D(100.0, 50.0), 1.0e-3));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
