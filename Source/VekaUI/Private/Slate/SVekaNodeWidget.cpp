// Copyright TheMiniBoss. All Rights Reserved.

#include "Slate/SVekaNodeWidget.h"
#include "Core/VekaNode.h"
#include "Core/VekaModifier.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Modifiers/VekaInteractionModifiers.h"

SVekaNodeWidget::SVekaNodeWidget()
	: Children(this)
{
	SetCanTick(false);
}

void SVekaNodeWidget::Construct(const FArguments& InArgs, const TSharedRef<FVekaNode>& InNode)
{
	Node = InNode;
	SyncFromNode();
}

void SVekaNodeWidget::SyncFromNode()
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid())
	{
		Children.Empty();
		return;
	}

	// Rebuild the child widget list only if it changed. Children being removed are drawn after the live ones.
	TArray<TSharedRef<FVekaNode>> ChildNodes = Pinned->GetChildren();
	ChildNodes.Append(Pinned->GetRemovedChildren());
	bool bSame = Children.Num() >= ChildNodes.Num();
	TArray<TSharedRef<SWidget>> Widgets;
	Widgets.Reserve(ChildNodes.Num());
	for (int32 Index = 0; Index < ChildNodes.Num(); ++Index)
	{
		TSharedRef<SWidget> ChildWidget = ChildNodes[Index]->GetOrCreateWidget();
		bSame = bSame && Children.GetChildAt(Index) == ChildWidget;
		Widgets.Add(ChildWidget);
	}
	// The primitive's native widget (text input) goes last, over the content rect.
	NativeWidget = Pinned->IsInRemovedSubtree() ? nullptr : Pinned->GetPrimitive().GetNativeWidget(Pinned->GetPrimitiveState());
	if (NativeWidget.IsValid())
	{
		bSame = bSame && Children.Num() == Widgets.Num() + 1 && Children.GetChildAt(Widgets.Num()) == NativeWidget.ToSharedRef();
		Widgets.Add(NativeWidget.ToSharedRef());
	}
	else
	{
		bSame = bSame && Children.Num() == Widgets.Num();
	}
	if (!bSame)
	{
		Children.Empty();
		for (const TSharedRef<SWidget>& ChildWidget : Widgets)
		{
			Children.Add(ChildWidget);
		}
		Invalidate(EInvalidateWidgetReason::ChildOrder);
	}

	// Render transform (offset / scale / rotation modifiers), around the center.
	if (Pinned->HasRenderTransform())
	{
		SetRenderTransformPivot(FVector2D(0.5, 0.5));
		SetRenderTransform(Pinned->ComputeRenderTransform(Pinned->GetArrangedRect().Size));
	}
	else if (GetRenderTransform().IsSet())
	{
		SetRenderTransform(TOptional<FSlateRenderTransform>());
	}

	// Only interactive nodes take pointer events; others let them through to their children.
	if (Pinned->IsInRemovedSubtree())
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}
	else
	{
		SetVisibility(Pinned->WantsInput() && VekaEnv::IsEnabled(Pinned->GetEnvironment()) ? EVisibility::Visible : EVisibility::SelfHitTestInvisible);
	}
}

FVector2D SVekaNodeWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		return Pinned->Measure(FVekaProposal::Unspecified());
	}
	return FVector2D::ZeroVector;
}

void SVekaNodeWidget::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid())
	{
		return;
	}

	Pinned->Arrange(AllottedGeometry.GetLocalSize());
	ArrangeChildWidgets(*Pinned, AllottedGeometry, ArrangedChildren);
}

void SVekaNodeWidget::ArrangeChildWidgets(const FVekaNode& InNode, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const TArray<TSharedRef<FVekaNode>>& LiveNodes = InNode.GetChildren();
	const TArray<TSharedRef<FVekaNode>>& RemovedNodes = InNode.GetRemovedChildren();
	const int32 NodeWidgets = Children.Num() - (NativeWidget.IsValid() ? 1 : 0);
	const int32 Num = FMath::Min(LiveNodes.Num() + RemovedNodes.Num(), NodeWidgets);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		// Removed children keep the last rect they were given.
		const FVekaRect& Rect = Index < LiveNodes.Num() ? LiveNodes[Index]->GetArrangedRect() : RemovedNodes[Index - LiveNodes.Num()]->GetArrangedRect();
		// FChildren only gives const widgets from a const method; MakeChild needs a mutable reference.
		const TSharedRef<SWidget> ChildWidget = const_cast<TSlotlessChildren<SWidget>&>(Children).GetChildAt(Index);
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildWidget, FVector2f(Rect.Size), FSlateLayoutTransform(FVector2f(Rect.Position))));
	}
	if (NativeWidget.IsValid() && Children.Num() > 0)
	{
		const FVekaRect& Content = InNode.GetContentRect();
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(NativeWidget.ToSharedRef(), FVector2f(Content.Size), FSlateLayoutTransform(FVector2f(Content.Position))));
	}
}

void SVekaNodeWidget::EnsureAnimationFrames() const
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	const FVekaPrimitiveState* State = Pinned.IsValid() ? Pinned->GetPrimitiveState() : nullptr;
	if (State && !AnimationFrameTimer.IsValid() && State->NeedsAnimationFrame(FPlatformTime::Seconds()))
	{
		SVekaNodeWidget* Mutable = const_cast<SVekaNodeWidget*>(this);
		AnimationFrameTimer = Mutable->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(Mutable, &SVekaNodeWidget::HandleAnimationFrame));
	}
}

EActiveTimerReturnType SVekaNodeWidget::HandleAnimationFrame(double InCurrentTime, float InDeltaTime)
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	const FVekaPrimitiveState* State = Pinned.IsValid() ? Pinned->GetPrimitiveState() : nullptr;
	Invalidate(EInvalidateWidgetReason::Paint);
	if (State && State->NeedsAnimationFrame(FPlatformTime::Seconds()))
	{
		return EActiveTimerReturnType::Continue;
	}
	AnimationFrameTimer.Reset();
	return EActiveTimerReturnType::Stop;
}

FChildren* SVekaNodeWidget::GetChildren()
{
	return &Children;
}

int32 SVekaNodeWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid())
	{
		return LayerId;
	}

	// Modifier rects and the content rect must match this geometry before any modifier paints
	// (leaves have no OnArrangeChildren call, and modifiers can change between two layouts).
	Pinned->Arrange(AllottedGeometry.GetLocalSize());

	EnsureAnimationFrames();

	const FPaintPass Pass{ Args, AllottedGeometry, MyCullingRect, OutDrawElements, ShouldBeEnabled(bParentEnabled) };
	const float TransitionOpacity = Pinned->GetTransitionOpacity();
	if (TransitionOpacity < 1.f)
	{
		if (TransitionOpacity <= 0.f)
		{
			return LayerId;
		}
		FWidgetStyle Faded = InWidgetStyle;
		Faded.BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, TransitionOpacity));
		return PaintLevel(Pass, *Pinned, Pinned->GetModifiers().Num() - 1, LayerId, Faded);
	}
	return PaintLevel(Pass, *Pinned, Pinned->GetModifiers().Num() - 1, LayerId, InWidgetStyle);
}

int32 SVekaNodeWidget::PaintLevel(const FPaintPass& Pass, FVekaNode& InNode, int32 Level, int32 LayerId, const FWidgetStyle& Style) const
{
	if (Level < 0)
	{
		return PaintContent(Pass, InNode, LayerId, Style);
	}

	const FVekaModifier* Modifier = InNode.GetModifiers()[Level].GetPtr<FVekaModifier>();
	if (!Modifier)
	{
		return PaintLevel(Pass, InNode, Level - 1, LayerId, Style);
	}

	const FVekaRect& Rect = InNode.GetModifierRect(Level);
	const ESlateDrawEffect Effects = Pass.bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	// Behind: drawn with the style seen by this modifier (outer opacity applies, inner does not).
	FVekaPaintContext BehindContext{ Pass.Geometry, Pass.DrawElements, LayerId, Style, Effects, InNode.GetEnvironment(), InNode.GetPrimitiveState() };
	int32 MaxLayer = FMath::Max(LayerId, Modifier->PaintBehind(BehindContext, Rect));

	FWidgetStyle InnerStyle = Style;
	Modifier->ModifyWidgetStyle(InnerStyle);

	const bool bClip = Modifier->ClipsContent();
	if (bClip)
	{
		Pass.DrawElements.PushClip(FSlateClippingZone(BehindContext.MakePaintGeometry(Rect)));
	}
	MaxLayer = FMath::Max(MaxLayer, PaintLevel(Pass, InNode, Level - 1, MaxLayer + 1, InnerStyle));
	if (Modifier->PaintsContentTwice())
	{
		FVekaPaintContext BetweenContext{ Pass.Geometry, Pass.DrawElements, MaxLayer + 1, Style, Effects, InNode.GetEnvironment(), InNode.GetPrimitiveState() };
		MaxLayer = FMath::Max(MaxLayer, Modifier->PaintBetween(BetweenContext, Rect));
		MaxLayer = FMath::Max(MaxLayer, PaintLevel(Pass, InNode, Level - 1, MaxLayer + 1, InnerStyle));
	}
	if (bClip)
	{
		Pass.DrawElements.PopClip();
	}

	FVekaPaintContext FrontContext{ Pass.Geometry, Pass.DrawElements, MaxLayer + 1, Style, Effects, InNode.GetEnvironment(), InNode.GetPrimitiveState() };
	MaxLayer = FMath::Max(MaxLayer, Modifier->PaintFront(FrontContext, Rect));
	return MaxLayer;
}

int32 SVekaNodeWidget::PaintContent(const FPaintPass& Pass, FVekaNode& InNode, int32 LayerId, const FWidgetStyle& Style) const
{
	const FVekaPrimitive& Primitive = InNode.GetPrimitive();
	const FVekaRect& Content = InNode.GetContentRect();
	const ESlateDrawEffect Effects = Pass.bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FVekaPaintContext Context{ Pass.Geometry, Pass.DrawElements, LayerId, Style, Effects, InNode.GetEnvironment(), InNode.GetPrimitiveState() };
	int32 MaxLayer = FMath::Max(LayerId, Primitive.Paint(Context, Content));

	if (Children.Num() > 0)
	{
		const bool bClip = Primitive.ClipsChildren();
		if (bClip)
		{
			Pass.DrawElements.PushClip(FSlateClippingZone(Context.MakePaintGeometry(Content)));
		}

		FArrangedChildren Arranged(EVisibility::Visible);
		ArrangeChildWidgets(InNode, Pass.Geometry, Arranged); // already arranged in OnPaint
		// Children that do not overlap share their layers (better batching); overlapping ones (a ZStack, offsets) each
		// paint above the previous one, or the deeper layers of an earlier child would cover a later child (and its
		// backdrops would not see what is under them).
		bool bOverlap = false;
		for (int32 A = 0; A < Arranged.Num() && !bOverlap; ++A)
		{
			const FSlateRect RectA = Arranged[A].Geometry.GetRenderBoundingRect();
			for (int32 B = A + 1; B < Arranged.Num() && !bOverlap; ++B)
			{
				bOverlap = FSlateRect::DoRectanglesIntersect(RectA, Arranged[B].Geometry.GetRenderBoundingRect());
			}
		}
		const int32 ChildLayer = MaxLayer + 1;
		for (int32 Index = 0; Index < Arranged.Num(); ++Index)
		{
			const FArrangedWidget& Child = Arranged[Index];
			const int32 Painted = Child.Widget->Paint(Pass.Args.WithNewParent(this), Child.Geometry, Pass.CullingRect, Pass.DrawElements,
				bOverlap ? MaxLayer + 1 : ChildLayer, Style, Pass.bEnabled);
			MaxLayer = FMath::Max(MaxLayer, Painted);
		}

		if (bClip)
		{
			Pass.DrawElements.PopClip();
		}
	}

	FVekaPaintContext OverlayContext{ Pass.Geometry, Pass.DrawElements, MaxLayer + 1, Style, Effects, InNode.GetEnvironment(), InNode.GetPrimitiveState() };
	MaxLayer = FMath::Max(MaxLayer, Primitive.PaintOverlay(OverlayContext, Content));
	return MaxLayer;
}

// --- Input --------------------------------------------------------------------------------------------

namespace
{
	/** Distance (slate units) after which a press with a drag gesture is a drag, not a tap. */
	constexpr double DragTapSlop = 4.0;

	bool IsActivationKey(const FKey& Key)
	{
		return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	}
}

bool SVekaNodeWidget::IsInputEnabled(const FVekaNode& InNode) const
{
	return !InNode.IsInRemovedSubtree() && VekaEnv::IsEnabled(InNode.GetEnvironment());
}

void SVekaNodeWidget::SetPressed(bool bInPressed)
{
	if (bPressed == bInPressed)
	{
		return;
	}
	bPressed = bInPressed;
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		Veka::Interaction::FEventScope Scope(SharedThis(this), GetCachedGeometry(), Pinned->GetEnvironment(), FSlateApplication::Get().GetCursorPos());
		// Copy: a callback can re-evaluate views and replace the node's gestures.
		const TArray<TFunction<void(bool)>> Callbacks = Pinned->GetGestures().OnPress;
		for (const TFunction<void(bool)>& Callback : Callbacks)
		{
			Callback(bPressed);
		}
	}
}

void SVekaNodeWidget::SendDrag(FVekaNode& InNode, const FGeometry& MyGeometry, const FVector2D& Local, EVekaDragPhase Phase)
{
	const TArray<TFunction<void(const FVekaDragValue&)>> Callbacks = InNode.GetGestures().OnDrag;
	if (Callbacks.IsEmpty())
	{
		return;
	}
	FVekaDragValue Value;
	Value.Phase = Phase;
	Value.Location = Local;
	Value.StartLocation = DragStart;
	Value.Translation = Local - DragStart;
	Value.Size = FVector2D(MyGeometry.GetLocalSize());
	Veka::Interaction::FEventScope Scope(SharedThis(this), MyGeometry, InNode.GetEnvironment(), MyGeometry.LocalToAbsolute(Local));
	for (const TFunction<void(const FVekaDragValue&)>& Callback : Callbacks)
	{
		Callback(Value);
	}
}

void SVekaNodeWidget::SendTap(FVekaNode& InNode, const FGeometry& MyGeometry)
{
	Veka::Interaction::FEventScope Scope(SharedThis(this), MyGeometry, InNode.GetEnvironment(), FSlateApplication::Get().GetCursorPos());
	const TArray<TFunction<void()>> Callbacks = InNode.GetGestures().OnTap;
	for (const TFunction<void()>& Callback : Callbacks)
	{
		Callback();
	}
}

FReply SVekaNodeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid() || !IsInputEnabled(*Pinned))
	{
		return FReply::Unhandled();
	}
	const FVekaGestureSet& Gestures = Pinned->GetGestures();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Gestures.OnSecondaryTap.IsEmpty() || !Pinned->HitTest(Local))
		{
			return FReply::Unhandled();
		}
		Veka::Interaction::FEventScope Scope(SharedThis(this), MyGeometry, Pinned->GetEnvironment(), MouseEvent.GetScreenSpacePosition());
		const TArray<TFunction<void(const FVector2D&)>> Callbacks = Gestures.OnSecondaryTap;
		for (const TFunction<void(const FVector2D&)>& Callback : Callbacks)
		{
			Callback(FVector2D(MouseEvent.GetScreenSpacePosition()));
		}
		return FReply::Handled();
	}

	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	if (Gestures.OnTap.IsEmpty() && Gestures.OnPress.IsEmpty() && Gestures.OnDrag.IsEmpty() && !Gestures.bFocusable)
	{
		return FReply::Unhandled();
	}
	if (!Pinned->HitTest(Local))
	{
		return FReply::Unhandled();
	}

	const bool bFocusable = Gestures.bFocusable;
	DragStart = Local;
	bDragMoved = false;
	SetPressed(true);
	if (TSharedPtr<FVekaNode> Still = Node.Pin())
	{
		SendDrag(*Still, MyGeometry, Local, EVekaDragPhase::Began);
	}
	FReply Reply = FReply::Handled().CaptureMouse(SharedThis(this));
	if (bFocusable)
	{
		Reply.SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	return Reply;
}

FReply SVekaNodeWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Slate sends the second click of a fast pair as a double-click instead of a button down: treat it as a
	// new press so rapid taps all count (like SButton).
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SVekaNodeWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bPressed)
	{
		return FReply::Unhandled();
	}
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid() || Pinned->GetGestures().OnDrag.IsEmpty())
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	bDragMoved = bDragMoved || FVector2D::Distance(Local, DragStart) > DragTapSlop;
	SendDrag(*Pinned, MyGeometry, Local, EVekaDragPhase::Changed);
	return FReply::Handled();
}

FReply SVekaNodeWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bPressed || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	SetPressed(false);

	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (Pinned.IsValid())
	{
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const bool bHadDrag = !Pinned->GetGestures().OnDrag.IsEmpty();
		SendDrag(*Pinned, MyGeometry, Local, EVekaDragPhase::Ended);
		if (Pinned->HitTest(Local) && !(bHadDrag && bDragMoved))
		{
			SendTap(*Pinned, MyGeometry);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SVekaNodeWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid())
	{
		return FReply::Unhandled();
	}
	FVekaInputContext Context{ *Pinned, Pinned->GetPrimitiveState(), MyGeometry };
	return Pinned->GetPrimitive().OnMouseWheel(Context, MouseEvent);
}

void SVekaNodeWidget::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SPanel::OnMouseEnter(MyGeometry, MouseEvent);
	if (bHovered)
	{
		return;
	}
	bHovered = true;
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		Veka::Interaction::FEventScope Scope(SharedThis(this), MyGeometry, Pinned->GetEnvironment(), MouseEvent.GetScreenSpacePosition());
		const TArray<TFunction<void(bool)>> Callbacks = Pinned->GetGestures().OnHover;
		for (const TFunction<void(bool)>& Callback : Callbacks)
		{
			Callback(true);
		}
	}
}

void SVekaNodeWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SPanel::OnMouseLeave(MouseEvent);
	if (!bHovered)
	{
		return;
	}
	bHovered = false;
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		const TArray<TFunction<void(bool)>> Callbacks = Pinned->GetGestures().OnHover;
		for (const TFunction<void(bool)>& Callback : Callbacks)
		{
			Callback(false);
		}
	}
}

void SVekaNodeWidget::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SPanel::OnMouseCaptureLost(CaptureLostEvent);
	if (bPressed)
	{
		if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
		{
			SendDrag(*Pinned, GetCachedGeometry(), DragStart, EVekaDragPhase::Ended);
		}
	}
	SetPressed(false);
}

bool SVekaNodeWidget::SupportsKeyboardFocus() const
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	return Pinned.IsValid() && Pinned->GetGestures().bFocusable && IsInputEnabled(*Pinned);
}

FReply SVekaNodeWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid() || !IsInputEnabled(*Pinned))
	{
		return FReply::Unhandled();
	}
	Veka::Interaction::FEventScope Scope(SharedThis(this), MyGeometry, Pinned->GetEnvironment(), MyGeometry.LocalToAbsolute(MyGeometry.GetLocalSize() * 0.5f));
	const TArray<TFunction<bool(const FKeyEvent&)>> Callbacks = Pinned->GetGestures().OnKey;
	for (const TFunction<bool(const FKeyEvent&)>& Callback : Callbacks)
	{
		if (Callback(InKeyEvent))
		{
			return FReply::Handled();
		}
	}
	// Enter / Space activate a focused control that has a tap action (buttons).
	if (IsActivationKey(InKeyEvent.GetKey()) && !Pinned->GetGestures().OnTap.IsEmpty())
	{
		SendTap(*Pinned, MyGeometry);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SVekaNodeWidget::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	SetFocusVisible(InFocusEvent.GetCause() == EFocusCause::Navigation);
	return FReply::Handled();
}

void SVekaNodeWidget::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SPanel::OnFocusLost(InFocusEvent);
	SetFocusVisible(false);
}

void SVekaNodeWidget::SetFocusVisible(bool bVisible)
{
	if (bFocusVisible == bVisible)
	{
		return;
	}
	bFocusVisible = bVisible;
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		const TArray<TFunction<void(bool)>> Callbacks = Pinned->GetGestures().OnFocus;
		for (const TFunction<void(bool)>& Callback : Callbacks)
		{
			Callback(bVisible);
		}
	}
}
