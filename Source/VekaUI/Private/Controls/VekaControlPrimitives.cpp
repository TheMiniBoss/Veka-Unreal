// Copyright TheMiniBoss. All Rights Reserved.

#include "Controls/VekaControlPrimitives.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"
#include "Typography/VekaTypography.h"
#include "Vector/VekaPath.h"
#include "Modifiers/VekaInteractionModifiers.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "MediaPlayer.h"

namespace
{
	TArray<FVekaPolyline> RoundedRectPolylines(const FVekaRect& Rect, float Radius)
	{
		FVekaPath Path;
		Path.AddRoundedRect(Rect, FVekaCornerRadii(FMath::Max(0.f, Radius)));
		TArray<FVekaPolyline> Out;
		Path.Flatten(Out, 0.2);
		return Out;
	}

	TArray<FVekaPolyline> CirclePolylines(const FVector2D& Center, double Radius)
	{
		FVekaPath Path;
		Path.AddCircle(Center, Radius);
		TArray<FVekaPolyline> Out;
		Path.Flatten(Out, 0.2);
		return Out;
	}

	TArray<FVekaPolyline> ArcPolylines(const FVector2D& Center, double Radius, double StartDegrees, double SweepDegrees)
	{
		TArray<FVekaPolyline> Out;
		FVekaPolyline& Arc = Out.AddDefaulted_GetRef();
		const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FMath::Abs(SweepDegrees) / 4.0), 2, 128);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Angle = FMath::DegreesToRadians(StartDegrees + SweepDegrees * Step / Steps);
			Arc.Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Out;
	}

	void Fill(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const FVekaRect& Bounds)
	{
		if (Paint.IsVisible())
		{
			Veka::PaintRender::FillPolylines(Context, Polylines, Paint, Bounds);
		}
	}

	FVekaPaint Resolved(const FVekaPaint& Paint, const FVekaEnvironment& Environment)
	{
		return Veka::Colora::Resolve(Paint, Environment);
	}

	FVekaPaint TokenPaint(const TCHAR* Token, const FVekaEnvironment& Environment, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary)
	{
		return FVekaPaint(Veka::Colora::ResolveColor(FName(Token), Environment, Hierarchy));
	}

	struct FTrackState : public FVekaPrimitiveState
	{
		bool bAnimating = false;
		double Start = FPlatformTime::Seconds();

		virtual bool NeedsAnimationFrame(double Now) const override { return bAnimating; }
	};

	void DrawKnob(FVekaPaintContext& Context, const FVector2D& Center, float Size, float Border, const FVekaPaint& KnobPaint, const FVekaPaint& BorderPaint, const FVekaRect& Bounds)
	{
		const double R = Size * 0.5;
		Fill(Context, CirclePolylines(Center + FVector2D(0.0, 1.0), R + 0.5), FVekaPaint(FLinearColor(0.f, 0.f, 0.f, 0.28f)), Bounds);
		if (Border > 0.f)
		{
			Fill(Context, CirclePolylines(Center, R), BorderPaint, Bounds);
			Fill(Context, CirclePolylines(Center, FMath::Max(0.0, R - Border)), KnobPaint, Bounds);
		}
		else
		{
			Fill(Context, CirclePolylines(Center, R), KnobPaint, Bounds);
		}
	}
}

// --- Track -----------------------------------------------------------------------------------------------------

TSharedPtr<FVekaPrimitiveState> FVekaTrack::CreateState() const
{
	return MakeShared<FTrackState>();
}

FVector2D FVekaTrack::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	auto Usable = [](const TOptional<double>& Value) { return Value.IsSet() && Value.GetValue() > 0.0 && Value.GetValue() < Veka::Infinity * 0.5; };
	if (Shape == EVekaTrackShape::Circular)
	{
		// Rings keep their size (SwiftUI circular gauges and spinners do): a stack row would squeeze them.
		return FVector2D(IdealLength, IdealLength);
	}
	const double Width = Usable(Proposal.Width) ? Proposal.Width.GetValue() : IdealLength;
	return FVector2D(Width, FMath::Max(Thickness, KnobSize));
}

float FVekaTrack::FractionAt(double LocalX, double Width, float InKnobSize)
{
	const double Inset = InKnobSize * 0.5;
	const double Usable = FMath::Max(Width - 2.0 * Inset, 1.0);
	return static_cast<float>(FMath::Clamp((LocalX - Inset) / Usable, 0.0, 1.0));
}

int32 FVekaTrack::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0 || !FSlateApplication::IsInitialized())
	{
		return Context.LayerId;
	}
	FTrackState* State = static_cast<FTrackState*>(Context.State);
	const double Now = FPlatformTime::Seconds();
	if (State)
	{
		State->bAnimating = bIndeterminate;
	}
	const double Time = State ? Now - State->Start : 0.0;
	const float F = FMath::Clamp(Fraction, 0.f, 1.f);
	const FVekaPaint Track = Resolved(TrackPaint, Context.Environment);
	const FVekaPaint FillColor = Resolved(FillPaint, Context.Environment);
	const FVekaPaint Knob = Resolved(KnobPaint, Context.Environment);

	if (Shape == EVekaTrackShape::Circular)
	{
		const double Side = FMath::Min(Bounds.Size.X, Bounds.Size.Y);
		const FVector2D Center = Bounds.Position + Bounds.Size * 0.5;
		const double Radius = Side * 0.5 - FMath::Max(Thickness, KnobSize) * 0.5;
		const bool bFull = ArcDegrees >= 359.9f;
		const double Start = bFull ? -90.0 : 90.0 + (360.0 - ArcDegrees) * 0.5;
		const EVekaLineCap Cap = EVekaLineCap::Round;
		if (Track.IsVisible())
		{
			if (bFull)
			{
				TArray<FVekaPolyline> Ring = CirclePolylines(Center, Radius);
				Veka::PaintRender::StrokePolylines(Context, Ring, Track, Thickness, Bounds, EVekaLineCap::Butt, EVekaLineJoin::Round);
			}
			else
			{
				Veka::PaintRender::StrokePolylines(Context, ArcPolylines(Center, Radius, Start, ArcDegrees), Track, Thickness, Bounds, Cap, EVekaLineJoin::Round);
			}
		}
		FVekaPaintContext Over = Context;
		Over.LayerId = Context.LayerId + 1;
		if (bIndeterminate)
		{
			const double Spin = FMath::Fmod(Time, 1.0) * 360.0;
			Veka::PaintRender::StrokePolylines(Over, ArcPolylines(Center, Radius, Start + Spin, 100.0), FillColor, Thickness, Bounds, Cap, EVekaLineJoin::Round);
			return Over.LayerId;
		}
		if (bShowsFill && F > 0.f)
		{
			Veka::PaintRender::StrokePolylines(Over, ArcPolylines(Center, Radius, Start, ArcDegrees * F), FillColor, Thickness, Bounds, Cap, EVekaLineJoin::Round);
		}
		if (KnobSize > 0.f)
		{
			Over.LayerId = Context.LayerId + 2;
			const double Angle = FMath::DegreesToRadians(Start + ArcDegrees * F);
			DrawKnob(Over, Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius, KnobSize, KnobBorder, Knob, FillColor, Bounds);
		}
		return Context.LayerId + 2;
	}

	// Linear.
	const double Width = Bounds.Size.X;
	const double CenterY = Bounds.Position.Y + Bounds.Size.Y * 0.5;
	const FVekaRect TrackRect(FVector2D(Bounds.Position.X, CenterY - Thickness * 0.5), FVector2D(Width, Thickness));
	Fill(Context, RoundedRectPolylines(TrackRect, Thickness * 0.5f), Track, Bounds);

	FVekaPaintContext Over = Context;
	Over.LayerId = Context.LayerId + 1;
	if (bIndeterminate)
	{
		const double Segment = Width * 0.3;
		const double Phase = FMath::Fmod(Time, 1.4) / 1.4;
		const double X0 = FMath::Max(0.0, -Segment + Phase * (Width + Segment));
		const double X1 = FMath::Min(Width, -Segment + Phase * (Width + Segment) + Segment);
		if (X1 > X0)
		{
			const FVekaRect Moving(FVector2D(Bounds.Position.X + X0, TrackRect.Position.Y), FVector2D(X1 - X0, Thickness));
			Fill(Over, RoundedRectPolylines(Moving, Thickness * 0.5f), FillColor, Bounds);
		}
		return Over.LayerId;
	}

	const double Inset = KnobSize * 0.5;
	const double KnobX = Bounds.Position.X + Inset + F * FMath::Max(Width - 2.0 * Inset, 0.0);
	if (bShowsFill && F > 0.f)
	{
		const double FillWidth = KnobSize > 0.f ? KnobX - Bounds.Position.X : Width * F;
		const FVekaRect FillRect(TrackRect.Position, FVector2D(FMath::Max(FillWidth, static_cast<double>(Thickness)), Thickness));
		Fill(Over, RoundedRectPolylines(FillRect, Thickness * 0.5f), FillColor, Bounds);
	}
	if (KnobSize > 0.f)
	{
		Over.LayerId = Context.LayerId + 2;
		DrawKnob(Over, FVector2D(KnobX, CenterY), KnobSize, KnobBorder, Knob, FillColor, Bounds);
	}
	return Context.LayerId + 2;
}

// --- Text input ------------------------------------------------------------------------------------------------

namespace
{
	struct FTextInputCallbacks
	{
		TFunction<void(const FString&)> OnChanged;
		TFunction<void(const FString&, bool)> OnCommitted;
	};

	struct FTextInputState : public FVekaPrimitiveState
	{
		TSharedRef<FTextInputCallbacks> Callbacks = MakeShared<FTextInputCallbacks>();
		TSharedPtr<SBox> Box;
		TSharedPtr<SEditableText> Editable;

		// Last values pushed to the widget (Slate setters invalidate every time).
		FSlateFontInfo LastFont;
		FLinearColor LastColor = FLinearColor::Transparent;
		FText LastHint;
		bool bLastSecure = false;
		bool bLastEnabled = true;
		float LastPadding = -1.f;

		bool HasFocus() const { return Editable.IsValid() && Editable->HasKeyboardFocus(); }
	};

	float HorizontalPadding(EVekaTextFieldStyle Style, float Height)
	{
		switch (Style)
		{
		case EVekaTextFieldStyle::Plain: return 0.f;
		case EVekaTextFieldStyle::Capsule: return FMath::Max(10.f, Height * 0.45f);
		default: return 8.f;
		}
	}
}

TSharedPtr<FVekaPrimitiveState> FVekaTextInput::CreateState() const
{
	return MakeShared<FTextInputState>();
}

TSharedPtr<SWidget> FVekaTextInput::GetNativeWidget(FVekaPrimitiveState* InState) const
{
	if (!InState || !FSlateApplication::IsInitialized())
	{
		return nullptr;
	}
	FTextInputState& State = static_cast<FTextInputState&>(*InState);
	State.Callbacks->OnChanged = OnChanged;
	State.Callbacks->OnCommitted = OnCommitted;
	if (!State.Editable.IsValid())
	{
		TSharedRef<FTextInputCallbacks> Callbacks = State.Callbacks;
		SAssignNew(State.Box, SBox)
			.VAlign(VAlign_Center)
			[
				SAssignNew(State.Editable, SEditableText)
				.Text(FText::FromString(Text))
				.HintText(Placeholder)
				.IsPassword(bSecure)
				.SelectAllTextWhenFocused(bSelectAllOnFocus)
				.OnTextChanged_Lambda([Callbacks](const FText& NewText)
				{
					if (Callbacks->OnChanged)
					{
						Callbacks->OnChanged(NewText.ToString());
					}
				})
				.OnTextCommitted_Lambda([Callbacks](const FText& NewText, ETextCommit::Type Commit)
				{
					if (Callbacks->OnCommitted)
					{
						Callbacks->OnCommitted(NewText.ToString(), Commit == ETextCommit::OnEnter);
					}
				})
			];
		State.LastHint = Placeholder;
		State.bLastSecure = bSecure;
	}
	return State.Box;
}

FVector2D FVekaTextInput::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	const EVekaTextFieldStyle Style = VekaEnv::GetControlStyle(Context.Environment).TextFieldStyle;
	const FSlateFontInfo Font = Veka::Typography::Resolve(Context.Environment).Font;
	const double Line = FMath::CeilToDouble(Font.Size * (4.0 / 3.0) * 1.2);
	const double Height = Line + (Style == EVekaTextFieldStyle::Plain ? 4.0 : 9.0);
	const bool bWidth = Proposal.Width.IsSet() && Proposal.Width.GetValue() > 0.0 && Proposal.Width.GetValue() < Veka::Infinity * 0.5;
	return FVector2D(bWidth ? Proposal.Width.GetValue() : 200.0, Height);
}

int32 FVekaTextInput::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Context.State)
	{
		return Context.LayerId;
	}
	FTextInputState& State = static_cast<FTextInputState&>(*Context.State);
	State.Callbacks->OnChanged = OnChanged;
	State.Callbacks->OnCommitted = OnCommitted;
	const bool bFocused = State.HasFocus();
	const EVekaTextFieldStyle Style = VekaEnv::GetControlStyle(Context.Environment).TextFieldStyle;

	// Keep the native widget in sync with the element and the environment.
	if (State.Editable.IsValid())
	{
		const FSlateFontInfo Font = Veka::Typography::Resolve(Context.Environment).Font;
		if (!(Font == State.LastFont))
		{
			State.Editable->SetFont(Font);
			State.LastFont = Font;
		}
		const FLinearColor Color = VekaEnv::GetForegroundColor(Context.Environment);
		if (!Color.Equals(State.LastColor))
		{
			State.Editable->SetColorAndOpacity(FSlateColor(Color));
			State.LastColor = Color;
		}
		if (!Placeholder.IdenticalTo(State.LastHint))
		{
			State.Editable->SetHintText(Placeholder);
			State.LastHint = Placeholder;
		}
		if (bSecure != State.bLastSecure)
		{
			State.Editable->SetIsPassword(bSecure);
			State.bLastSecure = bSecure;
		}
		const bool bEnabled = VekaEnv::IsEnabled(Context.Environment);
		if (bEnabled != State.bLastEnabled)
		{
			State.Editable->SetEnabled(bEnabled);
			State.bLastEnabled = bEnabled;
		}
		const float Padding = HorizontalPadding(Style, static_cast<float>(Bounds.Size.Y));
		if (Padding != State.LastPadding && State.Box.IsValid())
		{
			State.Box->SetPadding(FMargin(Padding, 0.f));
			State.LastPadding = Padding;
		}
		if (!bFocused && State.Editable->GetText().ToString() != Text)
		{
			State.Editable->SetText(FText::FromString(Text));
		}
	}

	// Field chrome.
	const float Radius = Style == EVekaTextFieldStyle::Capsule ? static_cast<float>(Bounds.Size.Y * 0.5)
		: Style == EVekaTextFieldStyle::Rounded ? 7.f : Style == EVekaTextFieldStyle::Rectangle ? 2.f : 0.f;
	FVekaPaintContext Layer = Context;
	if (Style != EVekaTextFieldStyle::Plain)
	{
		const TArray<FVekaPolyline> Outline = RoundedRectPolylines(Bounds, Radius);
		Fill(Layer, Outline, TokenPaint(TEXT("Fill"), Context.Environment, EVekaHierarchy::Secondary), Bounds);
		Layer.LayerId++;
		const FVekaRect Inner(Bounds.Position + FVector2D(0.5, 0.5), Bounds.Size - FVector2D(1.0, 1.0));
		Veka::PaintRender::StrokePolylines(Layer, RoundedRectPolylines(Inner, Radius), bFocused ? TokenPaint(TEXT("Tint"), Context.Environment) : TokenPaint(TEXT("Separator"), Context.Environment),
			bFocused ? 2.f : 1.f, Bounds, EVekaLineCap::Butt, EVekaLineJoin::Round);
	}
	else
	{
		const FVekaRect Line(FVector2D(Bounds.Position.X, Bounds.Position.Y + Bounds.Size.Y - (bFocused ? 2.0 : 1.0)), FVector2D(Bounds.Size.X, bFocused ? 2.0 : 1.0));
		Fill(Layer, RoundedRectPolylines(Line, 0.f), bFocused ? TokenPaint(TEXT("Tint"), Context.Environment) : TokenPaint(TEXT("Separator"), Context.Environment), Bounds);
	}
	return Layer.LayerId;
}

// --- Divider ---------------------------------------------------------------------------------------------------

FVector2D FVekaDivider::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	auto Finite = [](const TOptional<double>& Value) { return Value.IsSet() && Value.GetValue() < Veka::Infinity * 0.5 ? FMath::Max(0.0, Value.GetValue()) : 0.0; };
	const bool bVertical = Context.ParentStackAxis.IsSet() && Context.ParentStackAxis.GetValue() == EVekaAxis::Horizontal;
	return bVertical ? FVector2D(Thickness, Finite(Proposal.Height)) : FVector2D(Finite(Proposal.Width), Thickness);
}

int32 FVekaDivider::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	Fill(Context, RoundedRectPolylines(Bounds, 0.f), TokenPaint(TEXT("Separator"), Context.Environment), Bounds);
	return Context.LayerId;
}

// --- Media scrubber --------------------------------------------------------------------------------------------

namespace
{
	struct FScrubberState : public FVekaPrimitiveState
	{
		TWeakObjectPtr<UMediaPlayer> Player;

		virtual bool NeedsAnimationFrame(double Now) const override
		{
			const UMediaPlayer* Pinned = Player.Get();
			return Pinned && Pinned->IsPlaying();
		}
	};

	FString FormatTime(double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt32(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}
}

TSharedPtr<FVekaPrimitiveState> FVekaMediaScrubber::CreateState() const
{
	return MakeShared<FScrubberState>();
}

FVector2D FVekaMediaScrubber::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	const bool bWidth = Proposal.Width.IsSet() && Proposal.Width.GetValue() > 0.0 && Proposal.Width.GetValue() < Veka::Infinity * 0.5;
	return FVector2D(bWidth ? Proposal.Width.GetValue() : 260.0, 20.0);
}

int32 FVekaMediaScrubber::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return Context.LayerId;
	}
	if (FScrubberState* State = static_cast<FScrubberState*>(Context.State))
	{
		State->Player = Player;
	}
	const double Duration = Player ? Player->GetDuration().GetTotalSeconds() : 0.0;
	const double Time = Player ? Player->GetTime().GetTotalSeconds() : 0.0;
	const float F = Duration > 0.0 ? static_cast<float>(FMath::Clamp(Time / Duration, 0.0, 1.0)) : 0.f;

	const double BarX = Bounds.Position.X + LabelWidth;
	const double BarWidth = FMath::Max(Bounds.Size.X - 2.0 * LabelWidth, 1.0);
	const double CenterY = Bounds.Position.Y + Bounds.Size.Y * 0.5;
	const FVekaRect Track(FVector2D(BarX, CenterY - 2.0), FVector2D(BarWidth, 4.0));
	Fill(Context, RoundedRectPolylines(Track, 2.f), FVekaPaint(FLinearColor(1.f, 1.f, 1.f, 0.25f)), Bounds);
	FVekaPaintContext Over = Context;
	Over.LayerId++;
	if (F > 0.f)
	{
		Fill(Over, RoundedRectPolylines(FVekaRect(Track.Position, FVector2D(FMath::Max(4.0, BarWidth * F), 4.0)), 2.f), FVekaPaint(FLinearColor::White), Bounds);
	}

	FSlateFontInfo Font = Veka::Typography::StyleFont(EVekaTextStyle::Caption1, &Context.Environment);
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FString Left = FormatTime(Time);
	const FString Right = FormatTime(Duration);
	const FVector2D LeftSize = Measure->Measure(Left, Font);
	const FVector2D RightSize = Measure->Measure(Right, Font);
	const FLinearColor Color = Over.Tint(FLinearColor(1.f, 1.f, 1.f, 0.85f));
	FSlateDrawElement::MakeText(Over.DrawElements, Over.LayerId, Over.MakePaintGeometry(FVekaRect(FVector2D(Bounds.Position.X, CenterY - LeftSize.Y * 0.5), LeftSize)),
		Left, Font, Over.DrawEffects, Color);
	FSlateDrawElement::MakeText(Over.DrawElements, Over.LayerId, Over.MakePaintGeometry(FVekaRect(FVector2D(Bounds.Position.X + Bounds.Size.X - RightSize.X, CenterY - RightSize.Y * 0.5), RightSize)),
		Right, Font, Over.DrawEffects, Color);
	return Over.LayerId;
}
