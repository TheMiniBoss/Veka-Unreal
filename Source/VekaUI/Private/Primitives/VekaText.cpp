// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaText.h"
#include "Core/VekaEnvironment.h"
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

	struct FTextMetrics
	{
		TSharedPtr<FSlateFontMeasure> Measure;
		FSlateFontInfo Font;

		double Width(const FString& Line) const
		{
			if (Measure.IsValid())
			{
				return Measure->Measure(Line, Font).X;
			}
			// Headless fallback (tests, commandlets).
			return Line.Len() * Font.Size * 0.55;
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

	struct FVekaTextState : public FVekaPrimitiveState
	{
		// Last layout, reused while text, font and width do not change.
		FString CachedText;
		FSlateFontInfo CachedFont;
		double CachedWidth = -1.0;
		int32 CachedLineLimit = -1;
		bool bCachedEllipsis = false;
		TArray<FString> Lines;
		TArray<double> LineWidths;
		double LineHeight = 0.0;
	};

	/** Breaks lines for MaxWidth and fills the state cache. */
	const FVekaTextState& Layout(const FVekaText& Text, FVekaTextState& State, const FSlateFontInfo& Font, double MaxWidth)
	{
		const FString String = Text.Text.ToString();
		const bool bEllipsis = Text.Truncation == EVekaTextTruncation::Tail;
		if (State.CachedWidth == MaxWidth && State.CachedText == String && State.CachedFont == Font
			&& State.CachedLineLimit == Text.LineLimit && State.bCachedEllipsis == bEllipsis)
		{
			return State;
		}

		FTextMetrics Metrics{ GetFontMeasure(), Font };
		State.Lines = Veka::TextLayout::BreakLines(String, MaxWidth, Text.LineLimit, bEllipsis,
			[&Metrics](const FString& Line) { return Metrics.Width(Line); });
		State.LineWidths.Reset(State.Lines.Num());
		for (const FString& Line : State.Lines)
		{
			State.LineWidths.Add(Metrics.Width(Line));
		}
		State.LineHeight = Metrics.LineHeight() * Text.LineSpacing;
		State.CachedText = String;
		State.CachedFont = Font;
		State.CachedWidth = MaxWidth;
		State.CachedLineLimit = Text.LineLimit;
		State.bCachedEllipsis = bEllipsis;
		return State;
	}

	double MaxWidthFor(const FVekaProposal& Proposal)
	{
		return Proposal.Width.IsSet() && !Veka::IsInfinite(Proposal.Width.GetValue()) ? FMath::Max(Proposal.Width.GetValue(), 0.0) : -1.0;
	}
}

namespace Veka::TextLayout
{
	TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, bool bEllipsis, FMeasureWidth MeasureWidth)
	{
		TArray<FString> Lines;
		const bool bWrap = MaxWidth > 0.0 && !Veka::IsInfinite(MaxWidth);

		TArray<FString> Paragraphs;
		Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
		if (Paragraphs.IsEmpty())
		{
			Paragraphs.Add(FString());
		}

		bool bCut = false;
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
				if (Current.IsEmpty() || MeasureWidth(Candidate) <= MaxWidth)
				{
					Current = Candidate;
				}
				else
				{
					Lines.Add(Current);
					Current = Word;
				}

				// A single word wider than the line is split by characters.
				while (MeasureWidth(Current) > MaxWidth && Current.Len() > 1)
				{
					int32 Fit = Current.Len() - 1;
					while (Fit > 1 && MeasureWidth(Current.Left(Fit)) > MaxWidth)
					{
						--Fit;
					}
					Lines.Add(Current.Left(Fit));
					Current = Current.RightChop(Fit);
				}
			}
			Lines.Add(Current);
		}

		if (LineLimit > 0 && Lines.Num() > LineLimit)
		{
			Lines.SetNum(LineLimit);
			bCut = true;
		}

		if (bCut && bEllipsis && Lines.Num() > 0)
		{
			FString& Last = Lines.Last();
			FString WithEllipsis = Last + Ellipsis;
			if (bWrap)
			{
				while (!Last.IsEmpty() && MeasureWidth(WithEllipsis) > MaxWidth)
				{
					Last.LeftChopInline(1);
					Last.TrimEndInline();
					WithEllipsis = Last + Ellipsis;
				}
			}
			Last = WithEllipsis;
		}
		return Lines;
	}
}

FSlateFontInfo FVekaText::ResolveFont(const FVekaEnvironment& Environment) const
{
	return bOverrideFont ? Font : VekaEnv::GetFont(Environment);
}

TSharedPtr<FVekaPrimitiveState> FVekaText::CreateState() const
{
	return MakeShared<FVekaTextState>();
}

FVector2D FVekaText::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	FVekaTextState LocalState;
	FVekaTextState& State = Context.State ? static_cast<FVekaTextState&>(*Context.State) : LocalState;

	const FSlateFontInfo ResolvedFont = ResolveFont(Context.Environment);
	const FVekaTextState& Result = Layout(*this, State, ResolvedFont, MaxWidthFor(Proposal));

	double Width = 0.0;
	for (double LineWidth : Result.LineWidths)
	{
		Width = FMath::Max(Width, LineWidth);
	}
	return FVector2D(FMath::CeilToDouble(Width), FMath::CeilToDouble(Result.LineHeight * Result.Lines.Num()));
}

int32 FVekaText::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Context.State)
	{
		return Context.LayerId;
	}
	FVekaTextState& State = static_cast<FVekaTextState&>(*Context.State);

	const FSlateFontInfo ResolvedFont = ResolveFont(Context.Environment);
	// Re-layout with the final width (the measure pass may have used another proposal).
	const FVekaTextState& Result = Layout(*this, State, ResolvedFont, Bounds.Size.X > 0.0 ? Bounds.Size.X + 0.5 : -1.0);

	const FLinearColor TextColor = Context.Tint(bOverrideColor ? Color : VekaEnv::GetForegroundColor(Context.Environment));
	for (int32 Index = 0; Index < Result.Lines.Num(); ++Index)
	{
		const double LineWidth = Result.LineWidths[Index];
		const double X = Bounds.Position.X + FVekaAlignment::AlignH(Alignment, Bounds.Size.X, LineWidth);
		const double Y = Bounds.Position.Y + Index * Result.LineHeight;
		const FVekaRect LineRect(FVector2D(X, Y), FVector2D(LineWidth, Result.LineHeight));
		FSlateDrawElement::MakeText(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(LineRect), Result.Lines[Index],
			ResolvedFont, Context.DrawEffects, TextColor);
	}
	return Context.LayerId;
}
