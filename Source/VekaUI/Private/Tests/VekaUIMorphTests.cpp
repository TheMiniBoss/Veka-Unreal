// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 2f: smooth-min field, contour extraction, ShapeGroup (merged drawing and hit-test), matched geometry.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Slate/SVekaNodeWidget.h"
#include "Anima/VekaImplicit.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"

namespace
{
	constexpr EAutomationTestFlags VekaMorphTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	struct FMorphPaint
	{
		TSharedRef<SWindow> Window;
		FSlateWindowElementList Elements;
		FHittestGrid HittestGrid;

		FMorphPaint(const TSharedRef<SWidget>& Widget, const FVector2f& Size)
			: Window(SNew(SWindow).ClientSize(Size).CreateTitleBar(false))
			, Elements(Window)
		{
			HittestGrid.SetHittestArea(FVector2f::ZeroVector, Size);
			const FPaintArgs Args(nullptr, HittestGrid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Widget->SlatePrepass(1.f);
			Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}

		int32 Meshes() const { return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_CustomVerts>().Num(); }
		int32 Boxes() const { return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_RoundedBox>().Num(); }
	};

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

	double PolygonArea(const FVekaPolyline& Loop)
	{
		double Area = 0.0;
		for (int32 I = 0, J = Loop.Points.Num() - 1; I < Loop.Points.Num(); J = I++)
		{
			Area += (Loop.Points[J].X * Loop.Points[I].Y - Loop.Points[I].X * Loop.Points[J].Y);
		}
		return FMath::Abs(Area) * 0.5;
	}

	double CircleField(const FVector2D& Point, const FVector2D& Center, double Radius)
	{
		return FVector2D::Distance(Point, Center) - Radius;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaMorphFieldTest, "VekaUI.Morph.Field", VekaMorphTestFlags)
bool FVekaMorphFieldTest::RunTest(const FString& Parameters)
{
	using Veka::Morph::SmoothMin;
	TestEqual(TEXT("Far apart: plain min"), SmoothMin(1.0, 30.0, 10.0), 1.0);
	TestTrue(TEXT("Close: below the min (the bridge)"), SmoothMin(4.0, 5.0, 10.0) < 4.0);
	TestEqual(TEXT("K = 0: plain min"), SmoothMin(4.0, 5.0, 0.0), 4.0);
	TestTrue(TEXT("Symmetric"), FMath::IsNearlyEqual(SmoothMin(3.0, 7.0, 6.0), SmoothMin(7.0, 3.0, 6.0), 1e-9));

	// Two circles 10 apart: the middle point is outside each, inside the smooth union with a 20 blend.
	FVekaMorphShape A;
	A.Shape = FVekaShape::Circle();
	A.Bounds = FVekaRect(FVector2D::ZeroVector, FVector2D(60.0, 60.0));
	FVekaMorphShape B = A;
	B.Origin = FVector2D(70.0, 0.0);
	const TArray<FVekaMorphShape> Shapes = { A, B };
	const FVector2D Middle(65.0, 30.0);
	TestTrue(TEXT("Gap point outside the first circle"), A.SignedDistance(Middle) > 0.0);
	TestTrue(TEXT("Merged with a 24 blend"), Veka::Morph::Field(Shapes, 24.0, Middle) < 0.0);
	TestTrue(TEXT("Counter-check: not merged without blending"), Veka::Morph::Field(Shapes, 0.0, Middle) > 0.0);

	// Transforms: an offset moves the shape.
	FVekaMorphShape Moved = A;
	Moved.bTransformed = true;
	Moved.InverseTransform = FSlateRenderTransform(FVector2f(-100.f, 0.f));
	Moved.Pivot = FVector2D(30.0, 30.0);
	TestTrue(TEXT("Offset shape is at its new place"), Moved.SignedDistance(FVector2D(130.0, 30.0)) < 0.0);
	TestTrue(TEXT("And no longer at the old one"), Moved.SignedDistance(FVector2D(30.0, 30.0)) > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaMorphContourTest, "VekaUI.Morph.Contour", VekaMorphTestFlags)
bool FVekaMorphContourTest::RunTest(const FString& Parameters)
{
	using Veka::Morph::ExtractContour;
	TArray<FVekaPolyline> Loops;
	const FVekaRect Area(FVector2D(-10.0, -10.0), FVector2D(120.0, 120.0));
	ExtractContour([](const FVector2D& P) { return CircleField(P, FVector2D(50.0, 50.0), 40.0); }, Area, 1.5, Loops);
	if (TestEqual(TEXT("A circle gives one loop"), Loops.Num(), 1))
	{
		const double Expected = UE_DOUBLE_PI * 40.0 * 40.0;
		TestTrue(FString::Printf(TEXT("Area within 1 %% (%.0f / %.0f)"), PolygonArea(Loops[0]), Expected), FMath::Abs(PolygonArea(Loops[0]) - Expected) < Expected * 0.01);
		TestTrue(TEXT("Closed"), Loops[0].bClosed);
	}

	auto Two = [](double Smoothness) {
		return [Smoothness](const FVector2D& P) {
			return Veka::Morph::SmoothMin(CircleField(P, FVector2D(30.0, 50.0), 20.0), CircleField(P, FVector2D(78.0, 50.0), 20.0), Smoothness);
		};
	};
	ExtractContour(Two(0.0), Area, 1.5, Loops);
	TestEqual(TEXT("Two circles 8 apart, no blend: two loops"), Loops.Num(), 2);
	ExtractContour(Two(16.0), Area, 1.5, Loops);
	TestEqual(TEXT("With a 16 blend: one merged loop"), Loops.Num(), 1);

	ExtractContour([](const FVector2D& P) { return 1.0; }, Area, 1.5, Loops);
	TestEqual(TEXT("Nothing inside: no loop"), Loops.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaMorphShapeGroupTest, "VekaUI.Morph.ShapeGroup", VekaMorphTestFlags)
bool FVekaMorphShapeGroupTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		AddInfo(TEXT("No Slate renderer: skipped."));
		return true;
	}
	using namespace Veka;
	auto Pair = []() { return HStack(10.f)(Circle().Fill(FLinearColor::Red).Frame(60.f, 60.f), Circle().Fill(FLinearColor::Red).Frame(60.f, 60.f)); };

	const FMorphPaint Plain(SNew(SVekaHost).Root(Pair()), FVector2f(200.f, 100.f));
	TestEqual(TEXT("Counter-check: outside a group each circle draws itself"), Plain.Boxes(), 2);

	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(VStack()(ShapeGroup(24.f)(Pair())));
	const FMorphPaint Merged(Host, FVector2f(200.f, 100.f));
	TestEqual(TEXT("In a group the circles do not draw themselves"), Merged.Boxes(), 0);
	TestEqual(TEXT("The group draws one merged shape"), Merged.Meshes(), 1);

	const TSharedPtr<FVekaNode> Group = FindNode(Host->GetRuntime()->GetRootNode(), [](const FVekaNode& Node) { return Node.GetPrimitiveAs<FVekaShapeGroup>() != nullptr; });
	if (TestNotNull(TEXT("Group node"), Group.Get()))
	{
		const FVector2D Size = Group->GetArrangedRect().Size;
		const FVector2D Bridge(Size.X * 0.5, Size.Y * 0.5);
		TestTrue(TEXT("Hit inside a circle"), Group->HitTest(FVector2D(30.0, Size.Y * 0.5)));
		TestTrue(TEXT("Hit in the bridge between the circles"), Group->HitTest(Bridge));
		TestFalse(TEXT("No hit in a corner"), Group->HitTest(FVector2D(2.0, 2.0)));
	}

	TSharedRef<SVekaHost> Hard = SNew(SVekaHost).Root(VStack()(ShapeGroup(0.f)(Pair())));
	const FMorphPaint HardPaint(Hard, FVector2f(200.f, 100.f));
	const TSharedPtr<FVekaNode> HardGroup = FindNode(Hard->GetRuntime()->GetRootNode(), [](const FVekaNode& Node) { return Node.GetPrimitiveAs<FVekaShapeGroup>() != nullptr; });
	if (HardGroup.IsValid())
	{
		const FVector2D Size = HardGroup->GetArrangedRect().Size;
		TestFalse(TEXT("Counter-check: no bridge without blending"), HardGroup->HitTest(FVector2D(Size.X * 0.5, Size.Y * 0.5)));
	}

	// Moving a child (offset) changes the outline (clicks only reach the group inside its layout bounds, as in Slate).
	TSharedRef<SVekaHost> Apart = SNew(SVekaHost).Root(VStack()(ShapeGroup(20.f)(ZStack(FVekaAlignment::Leading())(Circle().Frame(60.f, 60.f), Circle().Frame(60.f, 60.f).Offset(FVector2D(200.0, 0.0))).Frame(300.f, 60.f, FVekaAlignment::Leading()))));
	const FMorphPaint ApartPaint(Apart, FVector2f(400.f, 100.f));
	const TSharedPtr<FVekaNode> ApartGroup = FindNode(Apart->GetRuntime()->GetRootNode(), [](const FVekaNode& Node) { return Node.GetPrimitiveAs<FVekaShapeGroup>() != nullptr; });
	if (ApartGroup.IsValid())
	{
		TestTrue(TEXT("Offset circle is part of the merged shape"), ApartGroup->HitTest(FVector2D(230.0, 30.0)));
		TestFalse(TEXT("Far apart: no bridge"), ApartGroup->HitTest(FVector2D(130.0, 30.0)));
	}
	return true;
}

namespace
{
	/** A small capsule that becomes a big card (matched id "card") on demand. */
	class FMatchedTestView : public FVekaView
	{
		VEKA_VIEW(FMatchedTestView)
	public:
		bool bMatched = true;
		explicit FMatchedTestView(bool bInMatched) : bMatched(bInMatched) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Expanded = Ctx.State<bool>(TEXT("Expanded"), false);
			GExpanded = Expanded;
			FVekaElement Small = Capsule().Frame(80.f, 30.f);
			FVekaElement Big = RoundedRectangle(16.f).Frame(300.f, 200.f);
			if (bMatched)
			{
				Small = MoveTemp(Small).MatchedGeometry(TEXT("card"), TEXT("test"));
				Big = MoveTemp(Big).MatchedGeometry(TEXT("card"), TEXT("test"));
			}
			return VStack(0.f, EVekaHAlign::Leading)(If(!Expanded.Get(), MoveTemp(Small)), If(Expanded.Get(), MoveTemp(Big))).Frame(400.f, 300.f, FVekaAlignment::TopLeading());
		}

		static TVekaState<bool> GExpanded;
	};
	TVekaState<bool> FMatchedTestView::GExpanded;

	TSharedPtr<FVekaNode> BigCard(FVekaRuntime& Runtime)
	{
		return FindNode(Runtime.GetRootNode(), [](const FVekaNode& Node)
		{
			const FVekaShapeView* Shape = Node.GetPrimitiveAs<FVekaShapeView>();
			return Shape && Shape->Shape.Kind == EVekaShapeKind::RoundedRectangle && !Node.IsInRemovedSubtree();
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaMorphMatchedGeometryTest, "VekaUI.Morph.MatchedGeometry", VekaMorphTestFlags)
bool FVekaMorphMatchedGeometryTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	auto Run = [this](bool bMatched, bool bAnimated, FVector2D& OutFirst, FVector2D& OutLater, FVector2D& OutEnd)
	{
		TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Veka::View<FMatchedTestView>(bMatched));
		FVekaRuntime& Runtime = *Host->GetRuntime();
		auto Repaint = [&Host]() { const FMorphPaint Paint(Host, FVector2f(400.f, 300.f)); };
		Repaint();
		if (bAnimated)
		{
			Veka::WithAnimation(FVekaAnimationSpec::EaseInOut(0.5), [&]() { FMatchedTestView::GExpanded.Set(true); });
		}
		else
		{
			FMatchedTestView::GExpanded.Set(true);
		}
		Runtime.Flush();
		Repaint();
		const TSharedPtr<FVekaNode> Card = BigCard(Runtime);
		if (!Card.IsValid())
		{
			AddError(TEXT("No card"));
			return;
		}
		OutFirst = Card->GetArrangedRect().Size;
		Runtime.Tick(0.25);
		Repaint();
		OutLater = BigCard(Runtime)->GetArrangedRect().Size;
		for (int32 Step = 0; Step < 20; ++Step)
		{
			Runtime.Tick(0.1);
		}
		Repaint();
		OutEnd = BigCard(Runtime)->GetArrangedRect().Size;
	};

	FVector2D First, Later, End;
	Run(true, true, First, Later, End);
	TestTrue(FString::Printf(TEXT("Starts from the capsule's frame (%s)"), *First.ToString()), First.Equals(FVector2D(80.0, 30.0), 1.0));
	TestTrue(FString::Printf(TEXT("Grows (%s)"), *Later.ToString()), Later.X > 85.0 && Later.X < 295.0);
	TestTrue(FString::Printf(TEXT("Ends at its own frame (%s)"), *End.ToString()), End.Equals(FVector2D(300.0, 200.0), 1.0));

	Run(true, false, First, Later, End);
	TestTrue(FString::Printf(TEXT("Counter-check: without an animation it is at once at its frame (%s)"), *First.ToString()), First.Equals(FVector2D(300.0, 200.0), 1.0));

	Run(false, true, First, Later, End);
	TestTrue(FString::Printf(TEXT("Counter-check: without a matched id it does not start from the capsule (%s)"), *First.ToString()), First.Equals(FVector2D(300.0, 200.0), 1.0));
	return true;
}

#endif
