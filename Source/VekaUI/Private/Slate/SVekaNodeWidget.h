// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SPanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Children.h"

class FVekaNode;

/**
 * The single Slate widget of a Veka node. Layout comes from the node (modifiers + primitive), paint
 * goes modifier by modifier around the primitive and the child widgets. No wrapper widget per modifier.
 */
class SVekaNodeWidget : public SPanel
{
public:
	SLATE_BEGIN_ARGS(SVekaNodeWidget) {}
	SLATE_END_ARGS()

	SVekaNodeWidget();

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaNode>& InNode);

	/** Child widgets, render transform and hit-test visibility from the node. */
	void SyncFromNode();

	// SWidget
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	struct FPaintPass
	{
		const FPaintArgs& Args;
		const FGeometry& Geometry;
		const FSlateRect& CullingRect;
		FSlateWindowElementList& DrawElements;
		bool bEnabled;
	};

	/** Child widget geometries from the node's last Arrange (does not arrange the node). */
	void ArrangeChildWidgets(const FVekaNode& InNode, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;
	int32 PaintLevel(const FPaintPass& Pass, FVekaNode& Node, int32 Level, int32 LayerId, const FWidgetStyle& Style) const;
	int32 PaintContent(const FPaintPass& Pass, FVekaNode& Node, int32 LayerId, const FWidgetStyle& Style) const;
	void SetPressed(bool bInPressed);

	/** Keeps repainting while the primitive animates on its own (text effects, content transitions). */
	void EnsureAnimationFrames() const;
	EActiveTimerReturnType HandleAnimationFrame(double InCurrentTime, float InDeltaTime);

	TWeakPtr<FVekaNode> Node;
	TSlotlessChildren<SWidget> Children;
	mutable TWeakPtr<FActiveTimerHandle> AnimationFrameTimer;
	bool bPressed = false;
	bool bHovered = false;
};
