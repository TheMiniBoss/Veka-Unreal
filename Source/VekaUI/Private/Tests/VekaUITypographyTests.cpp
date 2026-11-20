// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Fonts/CompositeFont.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"

namespace
{
	constexpr EAutomationTestFlags VekaTypoTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/** Environment of the root of a tree made of one element (its own modifiers applied). */
	FVekaEnvironment EnvOf(const FVekaElement& Element)
	{
		TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
		Runtime->SetRoot(Element);
		return Runtime->GetRootNode()->GetEnvironment();
	}

	int32 CountTextElements(const TSharedRef<SWidget>& Widget, const FVector2f& Size)
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Widget->SlatePrepass(1.f);
		Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Text>().Num()
			+ Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_ShapedText>().Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyFontsTest, "VekaUI.Typography.Fonts", VekaTypoTestFlags)
bool FVekaTypographyFontsTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<const FCompositeFont> Family = Veka::Typography::GetDefaultFamily();
	if (!TestTrue(TEXT("Default family"), Family.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("LT Flode Neue: 12 weights + italics"), Family->DefaultTypeface.Fonts.Num(), 24);
	TestTrue(TEXT("Engine fallback for missing glyphs"), Family->FallbackTypeface.Typeface.Fonts.Num() > 0);

	const FSlateFontInfo Bold = Veka::Typography::MakeFont(nullptr, 20.f, EVekaFontWeight::Bold, false, 0.02f);
	TestEqual(TEXT("Bold typeface"), Bold.TypefaceFontName, FName(TEXT("Bold")));
	TestEqual(TEXT("Size"), Bold.Size, 20.f);
	TestEqual(TEXT("Tracking in 1/1000 em"), Bold.LetterSpacing, 20);
	TestEqual(TEXT("Italic typeface"), Veka::Typography::MakeFont(nullptr, 14.f, EVekaFontWeight::Hairline, true).TypefaceFontName, FName(TEXT("HairlineItalic")));
	TestTrue(TEXT("Same composite (cache-friendly)"), Bold.CompositeFont == Family);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyResolveTest, "VekaUI.Typography.Resolve", VekaTypoTestFlags)
bool FVekaTypographyResolveTest::RunTest(const FString& Parameters)
{
	using namespace Veka;
	using Veka::Typography::Resolve;

	const Typography::FResolvedText Default = Resolve(VekaEnv::MakeDefault());
	TestEqual(TEXT("Default is Body"), Default.Font.Size, Typography::GetStyleSpec(EVekaTextStyle::Body).Size);
	TestEqual(TEXT("Body is Regular"), Default.Font.TypefaceFontName, FName(TEXT("Regular")));

	const Typography::FResolvedText Title = Resolve(EnvOf(Text(TEXT("x")).Font(EVekaTextStyle::Title1)));
	TestEqual(TEXT("Title1 size"), Title.Font.Size, Typography::GetStyleSpec(EVekaTextStyle::Title1).Size);
	TestEqual(TEXT("Title1 weight"), Title.Font.TypefaceFontName, FName(TEXT("Bold")));
	TestTrue(TEXT("Large text gets negative optical tracking"), Title.Font.LetterSpacing < 0);

	// Inner modifiers win: weight set inside a style keeps the style size.
	const Typography::FResolvedText Light = Resolve(EnvOf(Text(TEXT("x")).FontWeight(EVekaFontWeight::Light).Font(EVekaTextStyle::Title1)));
	TestEqual(TEXT("Inner weight wins"), Light.Font.TypefaceFontName, FName(TEXT("Light")));
	TestEqual(TEXT("Style size kept"), Light.Font.Size, Typography::GetStyleSpec(EVekaTextStyle::Title1).Size);

	// A style set inside resets an outer weight.
	const Typography::FResolvedText Reset = Resolve(EnvOf(Text(TEXT("x")).Font(EVekaTextStyle::Body).FontWeight(EVekaFontWeight::Black)));
	TestEqual(TEXT("Inner style resets outer weight"), Reset.Font.TypefaceFontName, FName(TEXT("Regular")));

	const Typography::FResolvedText Scaled = Resolve(EnvOf(Text(TEXT("x")).FontSize(20.f).TextScale(1.5f)));
	TestEqual(TEXT("Size override x text scale"), Scaled.Font.Size, 30.f, 1.0e-4f);

	const Typography::FResolvedText Tracked = Resolve(EnvOf(Text(TEXT("x")).Tracking(0.1f).TextCase(EVekaTextCase::Uppercase).MonospacedDigit().LineLimit(2)
		.Truncation(EVekaTextTruncation::Middle).MultilineAlignment(EVekaHAlign::Center).MinimumScaleFactor(0.5f)
		.ContentTransition(EVekaContentTransition::NumericText, 0.5f)));
	TestEqual(TEXT("Tracking"), Tracked.Font.LetterSpacing, 100);
	TestEqual(TEXT("Case"), Tracked.Case, EVekaTextCase::Uppercase);
	TestTrue(TEXT("Monospaced digits"), Tracked.bMonospacedDigits);
	TestEqual(TEXT("Line limit"), Tracked.LineLimit, 2);
	TestEqual(TEXT("Truncation"), Tracked.Truncation, EVekaTextTruncation::Middle);
	TestEqual(TEXT("Alignment"), Tracked.Alignment, EVekaHAlign::Center);
	TestEqual(TEXT("Minimum scale"), Tracked.MinimumScaleFactor, 0.5f);
	TestEqual(TEXT("Content transition"), Tracked.ContentTransition, EVekaContentTransition::NumericText);
	TestEqual(TEXT("Transition duration"), Tracked.ContentTransitionDuration, 0.5f);

	// Explicit Slate font (Phase 1 API) still works.
	const FSlateFontInfo Custom = Typography::MakeFont(nullptr, 9.f, EVekaFontWeight::Black);
	TestEqual(TEXT("Custom font"), Resolve(EnvOf(Text(TEXT("x")).Font(Custom))).Font.TypefaceFontName, FName(TEXT("Black")));

	TestTrue(TEXT("Optical tracking: looser when small"), Typography::OpticalTracking(10.f) > 0.f);
	TestTrue(TEXT("Optical tracking: tighter when large"), Typography::OpticalTracking(30.f) < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyTruncationTest, "VekaUI.Typography.Truncation", VekaTypoTestFlags)
bool FVekaTypographyTruncationTest::RunTest(const FString& Parameters)
{
	using namespace Veka::TextLayout;
	auto Measure = [](const FString& Line) { return Line.Len() * 10.0; };
	const FString Long = TEXT("abcdefghijklmnopqrstuvwxyz");

	TestEqual(TEXT("Tail"), TruncateLine(Long, 100.0, EVekaTextTruncation::Tail, Measure), FString(TEXT("abcdefghi…")));
	TestEqual(TEXT("Head"), TruncateLine(Long, 100.0, EVekaTextTruncation::Head, Measure), FString(TEXT("…rstuvwxyz")));
	TestEqual(TEXT("Middle"), TruncateLine(Long, 100.0, EVekaTextTruncation::Middle, Measure), FString(TEXT("abcde…wxyz")));

	bool bCut = false;
	TArray<FString> Lines = BreakLines(TEXT("one two three four five six"), 100.0, 2, EVekaTextTruncation::Head, Measure, &bCut);
	TestTrue(TEXT("Cut"), bCut);
	TestEqual(TEXT("Two lines"), Lines.Num(), 2);
	TestTrue(TEXT("Head shows the end of the text"), Lines[1].StartsWith(TEXT("…")) && Lines[1].EndsWith(TEXT("six")));

	Lines = BreakLines(TEXT("one two three four"), 100.0, 0, EVekaTextTruncation::Tail, Measure, &bCut);
	TestFalse(TEXT("Not cut without a limit"), bCut);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyEffectsTest, "VekaUI.Typography.Effects", VekaTypoTestFlags)
bool FVekaTypographyEffectsTest::RunTest(const FString& Parameters)
{
	using namespace Veka::TextEffects;
	FVekaTextEffect Appear;
	Appear.Kind = EVekaTextEffectKind::Appear;
	Appear.Stagger = 0.1f;
	Appear.Duration = 0.2f;
	TestEqual(TEXT("Glyph 0 invisible at start"), Evaluate(Appear, 0, 5, 0.0).Alpha, 0.f, 1.0e-4f);
	TestEqual(TEXT("Glyph 0 done after its duration"), Evaluate(Appear, 0, 5, 0.25).Alpha, 1.f, 1.0e-4f);
	TestEqual(TEXT("Glyph 3 still waiting"), Evaluate(Appear, 3, 5, 0.25).Alpha, 0.f, 1.0e-4f);
	TestTrue(TEXT("Rises from below"), Evaluate(Appear, 0, 5, 0.05).Offset.Y > 0.0);
	TestFalse(TEXT("Not finished early"), IsFinished(Appear, 5, 0.3));
	TestTrue(TEXT("Finished after the last glyph"), IsFinished(Appear, 5, 0.61));

	FVekaTextEffect Typewriter;
	Typewriter.Kind = EVekaTextEffectKind::Typewriter;
	Typewriter.Stagger = 0.1f;
	TestEqual(TEXT("Typewriter shows glyph 1 at 0.25 s"), Evaluate(Typewriter, 1, 5, 0.25).Alpha, 1.f, 1.0e-4f);
	TestEqual(TEXT("Typewriter hides glyph 4 at 0.25 s"), Evaluate(Typewriter, 4, 5, 0.25).Alpha, 0.f, 1.0e-4f);

	FVekaTextEffect Wave;
	Wave.Kind = EVekaTextEffectKind::Wave;
	TestFalse(TEXT("Wave never finishes"), IsFinished(Wave, 5, 1000.0));

	TestEqual(TEXT("Numeric up"), NumericDirection(TEXT("$9.99"), TEXT("$10.49")), 1);
	TestEqual(TEXT("Numeric down"), NumericDirection(TEXT("120"), TEXT("99")), -1);
	TestEqual(TEXT("Not numeric"), NumericDirection(TEXT("abc"), TEXT("abd")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyRenderTest, "VekaUI.Typography.GlyphRendering", VekaTypoTestFlags)
bool FVekaTypographyRenderTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Slate not initialized: skipped."));
		return true;
	}
	using namespace Veka;
	const FVector2f Size(400.f, 60.f);

	TestEqual(TEXT("Plain text: one element per line"), CountTextElements(SNew(SVekaHost).Root(Text(TEXT("Hello"))), Size), 1);
	TestEqual(TEXT("Gradient text: one element per glyph"),
		CountTextElements(SNew(SVekaHost).Root(Text(TEXT("Hello")).Foreground(FVekaPaint::Linear(FLinearColor::Red, FLinearColor::Blue))), Size), 5);
	TestEqual(TEXT("Monospaced digits: glyph by glyph"), CountTextElements(SNew(SVekaHost).Root(Text(TEXT("1234")).MonospacedDigit()), Size), 4);

	// Numeric content transition: right after a change both the old and the new digits are drawn.
	TSharedRef<SVekaHost> Counter = SNew(SVekaHost).Root(Text(TEXT("19")).ContentTransition(EVekaContentTransition::NumericText, 1.f));
	CountTextElements(Counter, Size);
	Counter->SetRoot(Text(TEXT("20")).ContentTransition(EVekaContentTransition::NumericText, 1.f));
	CountTextElements(Counter, Size); // Notices the change (new digits still fully transparent).
	FPlatformProcess::Sleep(0.3f);
	TestEqual(TEXT("Both digits roll (2 old + 2 new)"), CountTextElements(Counter, Size), 4);
	return true;
}


namespace
{
	class FScaledTextView : public FVekaView
	{
		VEKA_VIEW(FScaledTextView)
	public:
		TSharedRef<TVekaState<float>> Out;
		explicit FScaledTextView(TSharedRef<TVekaState<float>> InOut) : Out(InOut) {}
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<float> Scale = Ctx.State<float>(TEXT("Scale"), 1.f);
			*Out = Scale;
			return Veka::VStack()(Veka::Text(TEXT("Hello world"))).TextScale(Scale.Get()).Animation(FVekaAnimationSpec::Linear(0.2), Scale.Get());
		}
	};
}

// Regression (2c): an animated environment value (text scale) reached the children without clearing their
// measure caches, so they kept their old size while drawing with the new font.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTypographyScaleAnimationTest, "VekaUI.Typography.AnimatedTextScale", VekaTypoTestFlags)
bool FVekaTypographyScaleAnimationTest::RunTest(const FString& Parameters)
{
	TSharedRef<TVekaState<float>> Scale = MakeShared<TVekaState<float>>();
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(Veka::View<FScaledTextView>(Scale));
	const TSharedPtr<FVekaNode> Label = Runtime->GetRootNode()->GetChildren()[0]->GetChildren()[0];
	const double Before = Label->Measure(FVekaProposal::Unspecified()).X;

	Scale->Set(2.f);
	Runtime->Flush();
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Runtime->Tick(0.05);
		Label->Measure(FVekaProposal::Unspecified()); // Layout happens every frame.
	}
	const double After = Label->Measure(FVekaProposal::Unspecified()).X;
	TestEqual(TEXT("Text measures with the animated scale"), After / Before, 2.0, 0.15);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
