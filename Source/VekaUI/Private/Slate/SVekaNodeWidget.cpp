// Copyright TheMiniBoss. All Rights Reserved.

#include "Slate/SVekaNodeWidget.h"
#include "Core/VekaNode.h"
#include "Core/VekaModifier.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"

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
	bool bSame = Children.Num() == ChildNodes.Num();
	TArray<TSharedRef<SWidget>> Widgets;
	Widgets.Reserve(ChildNodes.Num());
	for (int32 Index = 0; Index < ChildNodes.Num(); ++Index)
	{
		TSharedRef<SWidget> ChildWidget = ChildNodes[Index]->GetOrCreateWidget();
		bSame = bSame && Children.GetChildAt(Index) == ChildWidget;
		Widgets.Add(ChildWidget);
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
		SetVisibility(Pinned->WantsInput() ? EVisibility::Visible : EVisibility::SelfHitTestInvisible);
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
	const int32 Num = FMath::Min(LiveNodes.Num() + RemovedNodes.Num(), Children.Num());
	for (int32 Index = 0; Index < Num; ++Index)
	{
		// Removed children keep the last rect they were given.
		const FVekaRect& Rect = Index < LiveNodes.Num() ? LiveNodes[Index]->GetArrangedRect() : RemovedNodes[Index - LiveNodes.Num()]->GetArrangedRect();
		// FChildren only gives const widgets from a const method; MakeChild needs a mutable reference.
		const TSharedRef<SWidget> ChildWidget = const_cast<TSlotlessChildren<SWidget>&>(Children).GetChildAt(Index);
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildWidget, FVector2f(Rect.Size), FSlateLayoutTransform(FVector2f(Rect.Position))));
	}
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
		const int32 ChildLayer = MaxLayer + 1;
		for (int32 Index = 0; Index < Arranged.Num(); ++Index)
		{
			const FArrangedWidget& Child = Arranged[Index];
			const int32 Painted = Child.Widget->Paint(Pass.Args.WithNewParent(this), Child.Geometry, Pass.CullingRect, Pass.DrawElements,
				ChildLayer, Style, Pass.bEnabled);
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

void SVekaNodeWidget::SetPressed(bool bInPressed)
{
	if (bPressed == bInPressed)
	{
		return;
	}
	bPressed = bInPressed;
	if (TSharedPtr<FVekaNode> Pinned = Node.Pin())
	{
		// Copy: a callback can re-evaluate views and replace the node's gestures.
		const TArray<TFunction<void(bool)>> Callbacks = Pinned->GetGestures().OnPress;
		for (const TFunction<void(bool)>& Callback : Callbacks)
		{
			Callback(bPressed);
		}
	}
}

FReply SVekaNodeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FVekaNode> Pinned = Node.Pin();
	if (!Pinned.IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FVekaGestureSet& Gestures = Pinned->GetGestures();
	if (Gestures.OnTap.IsEmpty() && Gestures.OnPress.IsEmpty())
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (!Pinned->HitTest(Local))
	{
		return FReply::Unhandled();
	}

	SetPressed(true);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SVekaNodeWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Slate sends the second click of a fast pair as a double-click instead of a button down: treat it as a
	// new press so rapid taps all count (like SButton).
	return OnMouseButtonDown(MyGeometry, MouseEvent);
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
		if (Pinned->HitTest(Local))
		{
			const TArray<TFunction<void()>> Callbacks = Pinned->GetGestures().OnTap;
			for (const TFunction<void()>& Callback : Callbacks)
			{
				Callback();
			}
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
	SetPressed(false);
}
