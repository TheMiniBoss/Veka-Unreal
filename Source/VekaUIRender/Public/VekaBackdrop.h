// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"

/** One backdrop: an axis-aligned rect in Slate absolute space (pixels), blurred and composited. */
struct FVekaBackdropParams
{
	FVector2f Min = FVector2f::ZeroVector;
	FVector2f Max = FVector2f::ZeroVector;
	/** Blur radius in pixels (0: the copy itself). Any value: two pyramid levels are blended. */
	float BlurRadius = 20.f;
	/** Top left, top right, bottom right, bottom left, in pixels. */
	FVector4f CornerRadii = FVector4f(0.f, 0.f, 0.f, 0.f);
	/** Color mixed over the blur (alpha = amount). */
	FLinearColor Tint = FLinearColor(1.f, 1.f, 1.f, 0.1f);
	float Saturation = 1.4f;
	/** Grain of frosted glass (0..1). */
	float Noise = 0.f;
	float Opacity = 1.f;
};

/** Backdrop quality (r.Veka.Backdrop.Quality, a scalability setting). */
enum class EVekaBackdropQuality : uint8
{
	/** No capture: backdrops draw their fallback fill. */
	Off,
	/** Pyramid levels sampled directly (no upsampling), up to 4 levels. */
	Low,
	Medium,
	High,
	Epic
};

/**
 * Every backdrop of one Veka host in one frame (a "layer"): one copy of what is behind the host (the union of the
 * backdrops' rects), one dual Kawase pyramid, shared by all of them. Game thread: Add() while painting, then
 * MakeCaptureElement() at the host's first layer. A new layer is made for every frame.
 */
class VEKAUIRENDER_API FVekaBackdropLayer : public TSharedFromThis<FVekaBackdropLayer, ESPMode::ThreadSafe>
{
public:
	/** Registers a backdrop and returns its composite element (FSlateDrawElement::MakeCustom at the node's layer). */
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Add(const FVekaBackdropParams& Params);
	/** Registers a backdrop without a composite element (a standalone backdrop draws both). */
	int32 AddItem(const FVekaBackdropParams& Params) { return Items.Add(Params); }
	void SetQuality(EVekaBackdropQuality InQuality) { Quality = InQuality; }
	bool IsEmpty() const { return Items.IsEmpty(); }
	int32 Num() const { return Items.Num(); }
	/** The capture element, drawn before the host's content (nullptr when the layer is empty). */
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeCaptureElement();

private:
	friend class FVekaBackdropCaptureElement;
	friend class FVekaBackdropCompositeElement;

	// Game thread, then read only by the render thread.
	TArray<FVekaBackdropParams> Items;
	EVekaBackdropQuality Quality = EVekaBackdropQuality::High;

	// Render thread: the pyramid of this frame (pooled, so composites in another graph can register it).
	FIntRect CaptureRect;
	/** Blurred textures by level (0 = the copy), all covering CaptureRect. */
	TArray<TRefCountPtr<struct IPooledRenderTarget>> Levels;
	float PixelScale = 1.f;
	bool bCaptured = false;
};

namespace Veka::Render
{
	VEKAUIRENDER_API TSharedRef<FVekaBackdropLayer> MakeBackdropLayer();

	/** A backdrop with its own capture (outside of a layer). */
	VEKAUIRENDER_API TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeBackdropElement(const FVekaBackdropParams& Params);

	/** Pyramid level for a blur radius in pixels: 0 = no blur, each level doubles the radius (about 1.5 px at level 1). */
	VEKAUIRENDER_API float BlurLevel(float BlurRadius);
	/** Integer levels the pyramid needs for a radius (ceil of BlurLevel, capped by the quality). */
	VEKAUIRENDER_API int32 BlurLevels(float BlurRadius);
	VEKAUIRENDER_API int32 MaxBlurLevels(EVekaBackdropQuality Quality);

	VEKAUIRENDER_API EVekaBackdropQuality GetBackdropQuality();
}
