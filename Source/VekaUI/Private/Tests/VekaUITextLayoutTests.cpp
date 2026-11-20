// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"

// Regression (2c): a Text measured with a width of 0 did not wrap, so every text looked inflexible and stacks
// split their space in equal shares, squeezing long labels ("+137" wrapped inside its button).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTextInStacksTest, "VekaUI.Layout.TextInStacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaTextInStacksTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	auto Button = [](const TCHAR* Label)
	{
		return Text(Label).Font(Fonts::Bold(13)).Padding(FMargin(12, 6)).Background(FLinearColor::Blue, 8);
	};
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(HStack(40, EVekaVAlign::Top)(
		VStack(8, EVekaHAlign::Leading)(
			Text(TEXT("1 280")).Font(EVekaTextStyle::LargeTitle).MonospacedDigit(),
			HStack(8)(Button(TEXT("+1")), Button(TEXT("+137")), Button(TEXT("-58"))),
			Text(TEXT("contentTransition(.numericText) + monospacedDigit")).Font(EVekaTextStyle::Caption1)),
		Text(TEXT("Other column")).Font(EVekaTextStyle::Title2)));
	const TSharedPtr<FVekaNode> Root = Runtime->GetRootNode();
	const FVector2D Size = Root->Measure(FVekaProposal(1500.0, TOptional<double>()));
	Root->ArrangeRecursive(Size);

	// With enough room, every node gets its ideal size.
	TFunction<void(const TSharedRef<FVekaNode>&)> Check = [&](const TSharedRef<FVekaNode>& Node)
	{
		if (Node != Root)
		{
			const FVector2D Ideal = Node->Measure(FVekaProposal::Unspecified());
			const FVekaText* Label = Node->GetPrimitiveAs<FVekaText>();
			TestTrue(FString::Printf(TEXT("'%s' gets its ideal width"), Label ? *Label->Text.ToString() : *Node->GetTypeName().ToString()),
				Node->GetArrangedRect().Size.X >= Ideal.X - 0.5);
		}
		for (const TSharedRef<FVekaNode>& Child : Node->GetChildren())
		{
			Check(Child);
		}
	};
	Check(Root.ToSharedRef());

	// Children of very different widths, placed at the size measured with a large proposal: placement must
	// repeat the measured distribution (re-dividing the final width in equal shares squeezed the widest one).
	Runtime->SetRoot(VStack(0, EVekaHAlign::Leading)(HStack(40)(
		Text(TEXT("Short")),
		Text(TEXT("A much longer label that is wider than a third of the row width")),
		Text(TEXT("Medium length label")))));
	const TSharedPtr<FVekaNode> Column = Runtime->GetRootNode();
	Column->ArrangeRecursive(Column->Measure(FVekaProposal(2000.0, TOptional<double>())));
	for (const TSharedRef<FVekaNode>& Label : Column->GetChildren()[0]->GetChildren())
	{
		TestTrue(FString::Printf(TEXT("'%s' not squeezed"), *Label->GetPrimitiveAs<FVekaText>()->Text.ToString()),
			Label->GetArrangedRect().Size.X >= Label->Measure(FVekaProposal::Unspecified()).X - 0.5);
	}

	// Squeezed: text wraps instead of overflowing.
	const FVector2D Narrow = Root->Measure(FVekaProposal(400.0, TOptional<double>()));
	TestTrue(TEXT("Fits a narrow proposal"), Narrow.X <= 400.0 + 0.5);
	return true;
}

#endif
