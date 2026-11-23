// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaText.h"
#include "Core/VekaEnvironment.h"
#include "Core/VekaPaint.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"
#include "Anima/VekaEasing.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	const TCHAR* Ellipsis = TEXT("…");

	TSharedPtr<FSlateFontMeasure> GetFontMeasure()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		return Renderer ? TSharedPtr<FSlateFontMeasure>(Renderer->GetFontMeasureService()) : nullptr;
	}

	/** Measures with the Slate font service (or an estimate when headless); handles monospaced digits. */
	struct FTextMetrics
	{
		TSharedPtr<FSlateFontMeasure> Measure;
		FSlateFontInfo Font;
		bool bMonospacedDigits = false;
		double DigitAdvance = 0.0;

		FTextMetrics(const FSlateFontInfo& InFont, bool bInMonospacedDigits)
			: Measure(GetFontMeasure())
			, Font(InFont)
			, bMonospacedDigits(bInMonospacedDigits)
		{
			if (bMonospacedDigits)
			{
				for (TCHAR Digit = TEXT('0'); Digit <= TEXT('9'); ++Digit)
				{
					DigitAdvance = FMath::Max(DigitAdvance, Raw(FString::Chr(Digit)));
				}
			}
		}

		double Raw(const FString& Line) const
		{
			if (Line.IsEmpty())
			{
				return 0.0;
			}
			if (Measure.IsValid())
			{
				return Measure->Measure(Line, Font).X;
			}
			// Headless fallback (tests, commandlets).
			return Line.Len() * Font.Size * 0.55;
		}

		double Width(const FString& Line) const
		{
			if (!bMonospacedDigits)
			{
				return Raw(Line);
			}
			double Total = 0.0;
			FString Run;
			for (const TCHAR Char : Line)
			{
				if (FChar::IsDigit(Char))
				{
					Total += Raw(Run) + DigitAdvance;
					Run.Reset();
				}
				else
				{
					Run.AppendChar(Char);
				}
			}
			return Total + Raw(Run);
		}

		/** X of the start of each character, plus the end (Len + 1 values). */
		void GlyphPositions(const FString& Line, TArray<double>& Out) const
		{
			Out.Reset(Line.Len() + 1);
			Out.Add(0.0);
			for (int32 Index = 1; Index <= Line.Len(); ++Index)
			{
				Out.Add(Width(Line.Left(Index)));
			}
		}

		double LineHeight() const
		{
			if (Measure.IsValid())
			{
				return Measure->GetMaxCharacterHeight(Font);
			}
			return Font.Size * 1.3;
		}
	};

	struct FVekaLaidOutLine
	{
		FString Text;
		double Width = 0.0;
		/** Filled only when glyph rendering is needed. */
		TArray<double> GlyphX;
	};

	struct FVekaLaidOutText
	{
		TArray<FVekaLaidOutLine> Lines;
		double LineHeight = 0.0;
		FSlateFontInfo Font;
		double MaxLineWidth = 0.0;
	};

	struct FVekaTextState : public FVekaPrimitiveState
	{
		// Layout cache.
		FString CachedText;
		FSlateFontInfo CachedFont;
		double CachedWidth = -2.0;
		int32 CachedLineLimit = -1;
		EVekaTextTruncation CachedTruncation = EVekaTextTruncation::None;
		float CachedMinScale = 1.f;
		bool bCachedMono = false;
		float CachedLineHeight = 1.f;
		bool bCachedGlyphs = false;
		FVekaLaidOutText Layout;

		// Content transitions and effects.
		FString DisplayedText;
		FVekaLaidOutText PreviousLayout;
		FVekaLaidOutText LastPaintedLayout;
		double AppearTime = -1.0;
		double ChangeTime = -1.0;
		float TransitionDuration = 0.f;
		bool bTransitionActive = false;
		FVekaTextEffect Effect;
		int32 GlyphCount = 0;

		virtual bool NeedsAnimationFrame(double Now) const override
		{
			if (bTransitionActive && Now - ChangeTime < TransitionDuration)
			{
				return true;
			}
			const double Start = FMath::Max(AppearTime, ChangeTime);
			return Effect.Kind != EVekaTextEffectKind::None && Start >= 0.0 && !Veka::TextEffects::IsFinished(Effect, GlyphCount, Now - Start);
		}
	};

	FString ApplyCase(const FString& Text, EVekaTextCase Case)
	{
		// Narrow no-break space (French digit grouping) is missing from most UI fonts: use a no-break space.
		static const FString NarrowNoBreakSpace = FString::Chr(static_cast<TCHAR>(0x202F));
		static const FString NoBreakSpace = FString::Chr(static_cast<TCHAR>(0x00A0));
		FString Display = Text.Replace(*NarrowNoBreakSpace, *NoBreakSpace);
		switch (Case)
		{
		case EVekaTextCase::Uppercase: return Display.ToUpper();
		case EVekaTextCase::Lowercase: return Display.ToLower();
		default: return Display;
		}
	}

	double MaxWidthFor(const FVekaProposal& Proposal)
	{
		return Proposal.Width.IsSet() && !Veka::IsInfinite(Proposal.Width.GetValue()) ? FMath::Max(Proposal.Width.GetValue(), 0.0) : -1.0;
	}

	FVekaLaidOutText DoLayout(const FString& Text, const Veka::Typography::FResolvedText& R, double MaxWidth, float Scale, bool bGlyphs, bool& bOutCut)
	{
		FVekaLaidOutText Out;
		Out.Font = R.Font;
		Out.Font.Size *= Scale;
		const FTextMetrics Metrics(Out.Font, R.bMonospacedDigits);
		const TArray<FString> Lines = Veka::TextLayout::BreakLines(Text, MaxWidth, R.LineLimit, R.Truncation,
			[&Metrics](const FString& Line) { return Metrics.Width(Line); }, &bOutCut);
		for (const FString& Line : Lines)
		{
			FVekaLaidOutLine& Layout = Out.Lines.AddDefaulted_GetRef();
			Layout.Text = Line;
			Layout.Width = Metrics.Width(Line);
			if (bGlyphs)
			{
				Metrics.GlyphPositions(Line, Layout.GlyphX);
			}
			Out.MaxLineWidth = FMath::Max(Out.MaxLineWidth, Layout.Width);
		}
		Out.LineHeight = Metrics.LineHeight() * R.LineHeight;
		return Out;
	}

	const FVekaLaidOutText& Layout(FVekaTextState& State, const FString& Text, const Veka::Typography::FResolvedText& R, double MaxWidth, bool bGlyphs)
	{
		if (State.CachedWidth == MaxWidth && State.CachedText == Text && State.CachedFont == R.Font && State.CachedLineLimit == R.LineLimit
			&& State.CachedTruncation == R.Truncation && State.CachedMinScale == R.MinimumScaleFactor && State.bCachedMono == R.bMonospacedDigits
			&& State.CachedLineHeight == R.LineHeight && (State.bCachedGlyphs || !bGlyphs))
		{
			return State.Layout;
		}

		bool bCut = false;
		State.Layout = DoLayout(Text, R, MaxWidth, 1.f, bGlyphs, bCut);

		// Shrink before truncating (SwiftUI minimumScaleFactor).
		if (bCut && R.MinimumScaleFactor < 1.f && MaxWidth > 0.0)
		{
			float Low = R.MinimumScaleFactor;
			float High = 1.f;
			bool bLowFits = false;
			FVekaLaidOutText Best = DoLayout(Text, R, MaxWidth, Low, bGlyphs, bCut);
			bLowFits = !bCut;
			if (bLowFits)
			{
				for (int32 Iteration = 0; Iteration < 7; ++Iteration)
				{
					const float Mid = 0.5f * (Low + High);
					FVekaLaidOutText Candidate = DoLayout(Text, R, MaxWidth, Mid, bGlyphs, bCut);
					if (!bCut)
					{
						Low = Mid;
						Best = MoveTemp(Candidate);
					}
					else
					{
						High = Mid;
					}
				}
			}
			State.Layout = MoveTemp(Best);
		}

		State.CachedText = Text;
		State.CachedFont = R.Font;
		State.CachedWidth = MaxWidth;
		State.CachedLineLimit = R.LineLimit;
		State.CachedTruncation = R.Truncation;
		State.CachedMinScale = R.MinimumScaleFactor;
		State.bCachedMono = R.bMonospacedDigits;
		State.CachedLineHeight = R.LineHeight;
		State.bCachedGlyphs = bGlyphs;
		return State.Layout;
	}

	void EnsureGlyphPositions(FVekaLaidOutText& Layout, const Veka::Typography::FResolvedText& R)
	{
		const FTextMetrics Metrics(Layout.Font, R.bMonospacedDigits);
		for (FVekaLaidOutLine& Line : Layout.Lines)
		{
			if (Line.GlyphX.Num() != Line.Text.Len() + 1)
			{
				Metrics.GlyphPositions(Line.Text, Line.GlyphX);
			}
		}
	}

	double Now()
	{
		return FPlatformTime::Seconds();
	}

	/** Draws one character in a box of Size at Position (local), scaled around the box center. */
	void DrawGlyph(FVekaPaintContext& Context, const FSlateFontInfo& Font, TCHAR Char, const FVector2D& Position, const FVector2D& Size,
		float Scale, const FLinearColor& Color)
	{
		if (Color.A <= 0.f || Scale <= 0.f || FChar::IsWhitespace(Char))
		{
			return;
		}
		const FVector2D Origin = Position + Size * (1.0 - Scale) * 0.5;
		FSlateDrawElement::MakeText(Context.DrawElements, Context.LayerId,
			Context.Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(Scale, FVector2f(Origin))),
			FString::Chr(Char), Font, Context.DrawEffects, Color);
	}
}

// --- Line breaking -----------------------------------------------------------------------------------------

namespace Veka::TextLayout
{
	/**
	 * Layout sizes are fractional and Slate snaps geometry to pixels: a text proposed its own width minus a
	 * fraction of a pixel must not wrap or truncate (labels in buttons wrapped to two lines).
	 */
	constexpr double WrapSlack = 1.0;

	FString TruncateLine(const FString& Line, double MaxWidth, EVekaTextTruncation Truncation, FMeasureWidth MeasureWidth)
	{
		if (Truncation == EVekaTextTruncation::None)
		{
			return Line;
		}
		const bool bLimit = MaxWidth > 0.0 && !Veka::IsInfinite(MaxWidth);
		auto Fits = [&](const FString& Candidate) { return !bLimit || MeasureWidth(Candidate) <= MaxWidth + WrapSlack; };

		// Number of characters kept, found by binary search on the fit.
		auto Build = [&](int32 Kept) -> FString
		{
			switch (Truncation)
			{
			case EVekaTextTruncation::Head:
				return Ellipsis + Line.Right(Kept).TrimStart();
			case EVekaTextTruncation::Middle:
			{
				const int32 Left = (Kept + 1) / 2;
				const int32 Right = Kept - Left;
				return Line.Left(Left).TrimEnd() + Ellipsis + Line.Right(Right).TrimStart();
			}
			default:
				return Line.Left(Kept).TrimEnd() + Ellipsis;
			}
		};

		int32 Low = 0;
		int32 High = Line.Len();
		while (Low < High)
		{
			const int32 Mid = (Low + High + 1) / 2;
			if (Fits(Build(Mid)))
			{
				Low = Mid;
			}
			else
			{
				High = Mid - 1;
			}
		}
		return Build(Low);
	}

	TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, EVekaTextTruncation Truncation, FMeasureWidth MeasureWidth, bool* OutCut)
	{
		TArray<FString> Lines;
		// A width of 0 is a real constraint (the narrowest the text can be: how stacks measure flexibility); -1 = none.
		const bool bWrap = MaxWidth >= 0.0 && !Veka::IsInfinite(MaxWidth);

		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		if (Paragraphs.IsEmpty())
		{
			Paragraphs.Add(FString());
		}

		for (const FString& Paragraph : Paragraphs)
		{
			if (!bWrap)
			{
				Lines.Add(Paragraph);
				continue;
			}

			TArray<FString> Words;
			Paragraph.ParseIntoArray(Words, TEXT(" "), true);
			if (Words.IsEmpty())
			{
				Lines.Add(FString());
				continue;
			}

			FString Current;
			for (const FString& Word : Words)
			{
				const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
				if (Current.IsEmpty() || MeasureWidth(Candidate) <= MaxWidth + WrapSlack)
				{
					Current = Candidate;
				}
				else
				{
					Lines.Add(Current);
					Current = Word;
				}

				// A single word wider than the line is split by characters.
				while (MeasureWidth(Current) > MaxWidth + WrapSlack && Current.Len() > 1)
				{
					int32 Fit = Current.Len() - 1;
					while (Fit > 1 && MeasureWidth(Current.Left(Fit)) > MaxWidth + WrapSlack)
					{
						--Fit;
					}
					Lines.Add(Current.Left(Fit));
					Current = Current.RightChop(Fit);
				}
			}
			Lines.Add(Current);
		}

		bool bCut = false;
		if (LineLimit > 0 && Lines.Num() > LineLimit)
		{
			// The last kept line stands for everything that follows (so Head / Middle show the end of the text).
			FString Rest = Lines[LineLimit - 1];
			for (int32 Index = LineLimit; Index < Lines.Num(); ++Index)
			{
				Rest += TEXT(" ") + Lines[Index];
			}
			Lines.SetNum(LineLimit);
			Lines.Last() = Rest;
			bCut = true;
		}

		if (bCut && Truncation != EVekaTextTruncation::None && Lines.Num() > 0)
		{
			Lines.Last() = TruncateLine(Lines.Last(), MaxWidth, Truncation, MeasureWidth);
		}
		else if (bCut && Lines.Num() > 0 && bWrap)
		{
			// No ellipsis: keep only what fits on the last line.
			Lines.Last() = TruncateLine(Lines.Last(), MaxWidth, EVekaTextTruncation::Tail, MeasureWidth).LeftChop(1);
		}
		if (OutCut)
		{
			*OutCut = bCut;
		}
		return Lines;
	}

	TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, bool bEllipsis, FMeasureWidth MeasureWidth)
	{
		return BreakLines(Text, MaxWidth, LineLimit, bEllipsis ? EVekaTextTruncation::Tail : EVekaTextTruncation::None, MeasureWidth, nullptr);
	}
}

// --- Effects -----------------------------------------------------------------------------------------------

namespace Veka::TextEffects
{
	FGlyphTransform Evaluate(const FVekaTextEffect& Effect, int32 Index, int32 Count, double Elapsed)
	{
		FGlyphTransform Out;
		switch (Effect.Kind)
		{
		case EVekaTextEffectKind::Appear:
		{
			const double Local = (Elapsed - Index * Effect.Stagger) / FMath::Max(Effect.Duration, 0.01f);
			const double T = Veka::Easing::Evaluate(EVekaAnimationCurve::EaseOut, FMath::Clamp(Local, 0.0, 1.0));
			Out.Alpha = static_cast<float>(T);
			Out.Offset.Y = Effect.Amplitude * (1.0 - T);
			Out.Scale = static_cast<float>(FMath::Lerp(0.85, 1.0, T));
			break;
		}
		case EVekaTextEffectKind::Typewriter:
		{
			const double Reveal = Elapsed / FMath::Max(Effect.Stagger, 0.001f);
			Out.Alpha = static_cast<float>(FMath::Clamp(Reveal - Index, 0.0, 1.0));
			break;
		}
		case EVekaTextEffectKind::Wave:
		{
			const double Period = FMath::Max(Effect.Stagger, 0.05f) * 20.0;
			Out.Offset.Y = Effect.Amplitude * FMath::Sin(UE_TWO_PI * (Elapsed / Period - Index * 0.08));
			break;
		}
		default:
			break;
		}
		return Out;
	}

	bool IsFinished(const FVekaTextEffect& Effect, int32 Count, double Elapsed)
	{
		switch (Effect.Kind)
		{
		case EVekaTextEffectKind::Appear: return Elapsed >= FMath::Max(Count - 1, 0) * Effect.Stagger + Effect.Duration;
		case EVekaTextEffectKind::Typewriter: return Elapsed >= Count * Effect.Stagger;
		case EVekaTextEffectKind::Wave: return false;
		default: return true;
		}
	}

	int32 NumericDirection(const FString& Old, const FString& New)
	{
		auto Parse = [](const FString& S, double& Out)
		{
			FString Digits;
			for (const TCHAR Char : S)
			{
				if (FChar::IsDigit(Char) || Char == TEXT('.') || Char == TEXT('-'))
				{
					Digits.AppendChar(Char);
				}
			}
			if (Digits.IsEmpty())
			{
				return false;
			}
			Out = FCString::Atod(*Digits);
			return true;
		};
		double A = 0.0;
		double B = 0.0;
		if (!Parse(Old, A) || !Parse(New, B) || A == B)
		{
			return 0;
		}
		return B > A ? 1 : -1;
	}
}

// --- Primitive ---------------------------------------------------------------------------------------------

Veka::Typography::FResolvedText FVekaText::ResolveText(const FVekaEnvironment& Environment) const
{
	Veka::Typography::FResolvedText Resolved = Veka::Typography::Resolve(Environment);
	if (bOverrideFont)
	{
		Resolved.Font = Font;
	}
	return Resolved;
}

TSharedPtr<FVekaPrimitiveState> FVekaText::CreateState() const
{
	return MakeShared<FVekaTextState>();
}

FVector2D FVekaText::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	FVekaTextState LocalState;
	FVekaTextState& State = Context.State ? static_cast<FVekaTextState&>(*Context.State) : LocalState;

	const Veka::Typography::FResolvedText Resolved = ResolveText(Context.Environment);
	const FString String = ApplyCase(Text.ToString(), Resolved.Case);
	const double MaxWidth = MaxWidthFor(Proposal);
	const FVekaLaidOutText& Result = Layout(State, String, Resolved, MaxWidth, false);
	double Width = FMath::CeilToDouble(Result.MaxLineWidth);
	if (MaxWidth >= 0.0 && Width > MaxWidth && Result.MaxLineWidth <= MaxWidth + Veka::TextLayout::WrapSlack)
	{
		// Kept on one line thanks to the sub-pixel slack: still report the proposed width (glyphs overhang < 1 px).
		Width = MaxWidth;
	}
	return FVector2D(Width, FMath::CeilToDouble(Result.LineHeight * Result.Lines.Num()));
}

int32 FVekaText::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Context.State)
	{
		return Context.LayerId;
	}
	FVekaTextState& State = static_cast<FVekaTextState&>(*Context.State);
	const double Time = Now();

	const Veka::Typography::FResolvedText Resolved = ResolveText(Context.Environment);
	const FString String = ApplyCase(Text.ToString(), Resolved.Case);

	// Foreground: explicit color, else the environment paint (gradients color each glyph).
	FVekaPaint ForegroundPaint(bOverrideColor ? Color : VekaEnv::GetForegroundColor(Context.Environment));
	if (!bOverrideColor)
	{
		if (const FVekaPaint* Paint = Context.Environment.Find<FVekaPaint>(VekaEnv::ForegroundPaint()))
		{
			ForegroundPaint = *Paint;
		}
	}
	const bool bGradient = ForegroundPaint.IsGradient();

	// Content change: remember what was shown, for the content transition and the effects.
	if (State.DisplayedText != String)
	{
		if (!State.DisplayedText.IsEmpty() || State.AppearTime >= 0.0)
		{
			State.PreviousLayout = State.LastPaintedLayout;
			State.ChangeTime = Time;
			State.bTransitionActive = Resolved.ContentTransition != EVekaContentTransition::None;
			State.TransitionDuration = Resolved.ContentTransitionDuration;
		}
		State.DisplayedText = String;
	}
	if (State.AppearTime < 0.0)
	{
		State.AppearTime = Time;
	}
	State.Effect = Resolved.Effect;
	State.GlyphCount = String.Len();

	const double Progress = State.bTransitionActive && State.TransitionDuration > 0.f
		? FMath::Clamp((Time - State.ChangeTime) / State.TransitionDuration, 0.0, 1.0) : 1.0;
	if (Progress >= 1.0)
	{
		State.bTransitionActive = false;
	}
	const double EffectStart = FMath::Max(State.AppearTime, State.ChangeTime);
	const bool bEffect = Resolved.Effect.Kind != EVekaTextEffectKind::None
		&& !(Resolved.Effect.Kind != EVekaTextEffectKind::Wave && Veka::TextEffects::IsFinished(Resolved.Effect, State.GlyphCount, Time - EffectStart));
	const bool bNumeric = State.bTransitionActive && Resolved.ContentTransition == EVekaContentTransition::NumericText;
	const bool bGlyphs = bGradient || Resolved.bMonospacedDigits || bEffect || bNumeric;

	// Re-layout with the final width (the measure pass may have used another proposal).
	const FVekaLaidOutText& Result = Layout(State, String, Resolved, Bounds.Size.X > 0.0 ? Bounds.Size.X + 0.5 : -1.0, bGlyphs);
	State.LastPaintedLayout = Result;
	if (bGlyphs)
	{
		EnsureGlyphPositions(State.LastPaintedLayout, Resolved);
	}
	const FVekaLaidOutText& Lines = State.LastPaintedLayout;

	const Veka::Colora::FPaintSampler Sampler(Veka::Colora::Resolve(ForegroundPaint, Context.Environment), Bounds);
	const FLinearColor Solid = Context.Tint(Sampler.Sample(Bounds.Center()));
	auto LineX = [&](const FVekaLaidOutLine& Line) { return Bounds.Position.X + FVekaAlignment::AlignH(Resolved.Alignment, Bounds.Size.X, Line.Width); };

	// Cross-fade: the old text fades out where it was.
	const bool bCrossFade = State.bTransitionActive && Resolved.ContentTransition == EVekaContentTransition::Opacity;
	const float NewAlpha = bCrossFade ? static_cast<float>(Progress) : 1.f;
	if (bCrossFade)
	{
		for (int32 Index = 0; Index < State.PreviousLayout.Lines.Num(); ++Index)
		{
			const FVekaLaidOutLine& Line = State.PreviousLayout.Lines[Index];
			const FVekaRect LineRect(FVector2D(LineX(Line), Bounds.Position.Y + Index * State.PreviousLayout.LineHeight), FVector2D(Line.Width, State.PreviousLayout.LineHeight));
			FLinearColor Faded = Solid;
			Faded.A *= 1.f - static_cast<float>(Progress);
			FSlateDrawElement::MakeText(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(LineRect), Line.Text, State.PreviousLayout.Font, Context.DrawEffects, Faded);
		}
	}

	if (!bGlyphs)
	{
		FLinearColor LineColor = Solid;
		LineColor.A *= NewAlpha;
		for (int32 Index = 0; Index < Lines.Lines.Num(); ++Index)
		{
			const FVekaLaidOutLine& Line = Lines.Lines[Index];
			const FVekaRect LineRect(FVector2D(LineX(Line), Bounds.Position.Y + Index * Lines.LineHeight), FVector2D(Line.Width, Lines.LineHeight));
			FSlateDrawElement::MakeText(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(LineRect), Line.Text, Lines.Font, Context.DrawEffects, LineColor);
		}
		return Context.LayerId;
	}

	// Glyph by glyph: effects, numeric transitions, monospaced digits, gradient text.
	// Larger number: the new digits come from below and the old ones leave upward (and the other way round).
	const int32 Direction = Veka::TextEffects::NumericDirection(State.PreviousLayout.Lines.Num() > 0 ? State.PreviousLayout.Lines[0].Text : FString(), String) < 0 ? -1 : 1;
	const double Eased = Veka::Easing::Evaluate(EVekaAnimationCurve::EaseInOut, Progress);
	int32 GlyphIndex = 0;
	for (int32 LineIndex = 0; LineIndex < Lines.Lines.Num(); ++LineIndex)
	{
		const FVekaLaidOutLine& Line = Lines.Lines[LineIndex];
		const double X0 = LineX(Line);
		const double Y0 = Bounds.Position.Y + LineIndex * Lines.LineHeight;

		const FVekaLaidOutLine* OldLine = bNumeric && State.PreviousLayout.Lines.IsValidIndex(LineIndex) ? &State.PreviousLayout.Lines[LineIndex] : nullptr;
		if (bNumeric)
		{
			Context.DrawElements.PushClip(FSlateClippingZone(Context.MakePaintGeometry(FVekaRect(FVector2D(Bounds.Position.X, Y0), FVector2D(Bounds.Size.X, Lines.LineHeight)))));
		}

		const int32 Len = Line.Text.Len();
		for (int32 Char = 0; Char < Len; ++Char, ++GlyphIndex)
		{
			const FVector2D Position(X0 + Line.GlyphX[Char], Y0);
			const FVector2D Size(Line.GlyphX[Char + 1] - Line.GlyphX[Char], Lines.LineHeight);
			Veka::TextEffects::FGlyphTransform Transform;
			if (bEffect)
			{
				Transform = Veka::TextEffects::Evaluate(Resolved.Effect, GlyphIndex, State.GlyphCount, Time - EffectStart);
			}
			FLinearColor GlyphColor = bGradient ? Context.Tint(Sampler.Sample(Position + Size * 0.5)) : Solid;
			GlyphColor.A *= Transform.Alpha * NewAlpha;

			if (OldLine)
			{
				// Characters aligned from the right: unchanged ones stay, changed ones roll.
				const int32 FromRight = Len - 1 - Char;
				const int32 OldIndex = OldLine->Text.Len() - 1 - FromRight;
				const TCHAR OldChar = OldIndex >= 0 ? OldLine->Text[OldIndex] : 0;
				if (OldChar != Line.Text[Char])
				{
					const double Roll = Lines.LineHeight * 0.8;
					FLinearColor InColor = GlyphColor;
					InColor.A *= static_cast<float>(Eased);
					DrawGlyph(Context, Lines.Font, Line.Text[Char], Position + FVector2D(0.0, Direction * Roll * (1.0 - Eased)), Size, 1.f, InColor);
					if (OldChar)
					{
						FLinearColor OutColor = GlyphColor;
						OutColor.A *= 1.f - static_cast<float>(Eased);
						DrawGlyph(Context, State.PreviousLayout.Font, OldChar, Position + FVector2D(0.0, -Direction * Roll * Eased), Size, 1.f, OutColor);
					}
					continue;
				}
			}
			DrawGlyph(Context, Lines.Font, Line.Text[Char], Position + Transform.Offset, Size, Transform.Scale, GlyphColor);
		}

		// Old characters with no counterpart (the number got shorter) roll out on the left.
		if (OldLine && OldLine->Text.Len() > Len)
		{
			const double Advance = Len > 0 ? Line.GlyphX[1] - Line.GlyphX[0] : Lines.Font.Size;
			for (int32 Extra = 0; Extra < OldLine->Text.Len() - Len; ++Extra)
			{
				const TCHAR OldChar = OldLine->Text[OldLine->Text.Len() - Len - 1 - Extra];
				const FVector2D Position(X0 - Advance * (Extra + 1), Y0 - Direction * Lines.LineHeight * 0.8 * Eased);
				FLinearColor OutColor = Solid;
				OutColor.A *= 1.f - static_cast<float>(Eased);
				DrawGlyph(Context, State.PreviousLayout.Font, OldChar, Position, FVector2D(Advance, Lines.LineHeight), 1.f, OutColor);
			}
		}

		if (bNumeric)
		{
			Context.DrawElements.PopClip();
		}
	}
	return Context.LayerId;
}
