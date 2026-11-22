// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"

/** One backdrop: an axis-aligned rect in Slate absolute space (pixels), blurred and composited. */
struct FVekaBackdropParams
{
	FVector2f Min = FVector2f::ZeroVector;
	FVector2f Max = FVector2f::ZeroVector;
	/** Blur radius in pixels (0: no blur, just the tint over the copy). */
	float BlurRadius = 20.f;
	/** Top left, top right, bottom right, bottom left, in pixels. */
	FVector4f CornerRadii = FVector4f(0.f, 0.f, 0.f, 0.f);
	/** Color mixed over the blur (alpha = amount). */
	FLinearColor Tint = FLinearColor(1.f, 1.f, 1.f, 0.1f);
	float Saturation = 1.4f;
	float Opacity = 1.f;
};

namespace Veka::Render
{
	/**
	 * A Slate custom element (FSlateDrawElement::MakeCustom) that copies what was drawn under the rect, blurs it
	 * on the GPU (dual Kawase, render graph) and composites it inside the rounded rect. Spike B of Phase 4.
	 */
	VEKAUIRENDER_API TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeBackdropElement(const FVekaBackdropParams& Params);

	/** Number of pyramid levels for a blur radius (each level halves the resolution). */
	VEKAUIRENDER_API int32 BlurLevels(float BlurRadius);
}
