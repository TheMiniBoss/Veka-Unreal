// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"

namespace
{
	FVekaElement DebugButton(const TCHAR* Label)
	{
		using namespace Veka;
		return Text(Label).Font(Fonts::Bold(13)).Padding(FMargin(12, 6)).Background(FLinearColor::Blue, 8);
	}

	class FDebugScaleView : public FVekaView
	{
		VEKA_VIEW(FDebugScaleView)
	public:
		TSharedRef<TVekaState<float>> Out;
		explicit FDebugScaleView(TSharedRef<TVekaState<float>> InOut) : Out(InOut) {}
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<float> Scale = Ctx.State<float>(TEXT("Scale"), 1.f);
			*Out = Scale;
			return VStack(6, EVekaHAlign::Leading)(
				HStack(8)(DebugButton(TEXT("A-")), DebugButton(TEXT("A+")), Text(TEXT("Text scale x1")).Font(EVekaTextStyle::Caption1)),
				VStack(2, EVekaHAlign::Leading)(
					Text(TEXT("Headline")).Font(EVekaTextStyle::Headline),
					Text(TEXT("Body text follows the global text size.")),
					Text(TEXT("Footnote")).Font(EVekaTextStyle::Footnote)
				).TextScale(Scale.Get()).Animation(FVekaAnimationSpec::Smooth(), Scale.Get()));
		}
	};
}

// Regression (2c): a container whose rect animates (layout transition) laid its children out at the animated size,
// squeezing a sibling column and re-wrapping its text. Content now keeps the final layout.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaDebugRowTest, "VekaUI.Anima.LayoutKeepsFinalContent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaDebugRowTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	TSharedRef<TVekaState<float>> Scale = MakeShared<TVekaState<float>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(VStack(18, EVekaHAlign::Leading)(
		HStack(40)(Text(TEXT("A wide first row that sets the width of the column")).Font(EVekaTextStyle::Title1)),
		HStack(40, EVekaVAlign::Top)(
			VStack(8, EVekaHAlign::Leading)(
				Text(TEXT("1 280")).Font(EVekaTextStyle::LargeTitle).MonospacedDigit(),
				HStack(8)(DebugButton(TEXT("+1")), DebugButton(TEXT("+137")), DebugButton(TEXT("-58"))),
				Text(TEXT("contentTransition(.numericText) + monospacedDigit")).Font(EVekaTextStyle::Caption1)),
			VStack(8, EVekaHAlign::Leading)(
				DebugButton(TEXT("Replay")),
				Text(TEXT("Welcome to VekaUI")).Font(EVekaTextStyle::Title2),
				Text(TEXT("Welcome to VekaUI")).Font(EVekaTextStyle::Title2)),
			View<FDebugScaleView>(Scale))));

	const TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();
	auto Layout = [&]()
	{
		const FVector2D Size = Root->Measure(FVekaProposal(2400.0, TOptional<double>()));
		Root->ArrangeRecursive(FVector2D(2400.0, Size.Y));
	};
	// The first column does not change: it must keep its width while the third one grows.
	auto CheckFirstColumn = [&](int32 Frame)
	{
		const TSharedRef<FVekaNode> Column = Root->GetChildren()[1]->GetChildren()[0];
		TestTrue(FString::Printf(TEXT("Column keeps its width (frame %d)"), Frame),
			Column->GetArrangedRect().Size.X >= Column->Measure(FVekaProposal::Unspecified()).X - 0.5);
	};
	Layout();
	CheckFirstColumn(-1);
	Scale->Set(1.15f);
	Runtime->Flush();
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		Runtime->Tick(1.0 / 30.0);
		Layout();
		CheckFirstColumn(Frame);
	}
	return true;
}

#endif
