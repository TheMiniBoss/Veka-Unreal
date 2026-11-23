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
	/** Liquid distortion: displacement of the background (pixels), size of the waves (pixels) and time (seconds). */
	float DistortionStrength = 0.f;
	float DistortionScale = 120.f;
	float DistortionTime = 0.f;
	float Opacity = 1.f;
};

/**
 * Blend of a node's content with what is behind it (Multiply ... Luminosity). The content is painted twice: over
 * black, then over white, which gives its coverage without the back buffer's alpha (10:10:10:2). Mode: index in
 * the W3C order Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn, HardLight, SoftLight, Difference,
 * Exclusion, Hue, Saturation, Color, Luminosity.
 */
struct FVekaBlendParams
{
	FVector2f Min = FVector2f::ZeroVector;
	FVector2f Max = FVector2f::ZeroVector;
	int32 Mode = 0;
};

/** The three elements of a blend: before the content, between its two paints, after. */
struct FVekaBlendElements
{
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Begin;
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Between;
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> End;
};

/** One shape of a glass: a rounded rect in Slate absolute space. */
struct FVekaGlassShape
{
	FVector2f Min = FVector2f::ZeroVector;
	FVector2f Max = FVector2f::ZeroVector;
	/** Top left, top right, bottom right, bottom left, in pixels. */
	FVector4f CornerRadii = FVector4f(0.f, 0.f, 0.f, 0.f);
};

/**
 * Liquid glass over what is behind it: the shapes (smooth union when several), refraction from the normal of their
 * signed distance field near the edge (the bezel), chromatic aberration, a specular rim lit from LightPosition, a
 * light blur and a tint. All lengths in Slate absolute units.
 */
struct FVekaGlassParams
{
	static constexpr int32 MaxShapes = 8;
	TArray<FVekaGlassShape, TInlineAllocator<MaxShapes>> Shapes;
	/** How far apart two shapes start to melt together (0 = plain union). */
	float Smoothness = 0.f;
	float BlurRadius = 3.f;
	/** Displacement of the background at the very edge. */
	float Refraction = 18.f;
	/** Width of the curved edge where refraction and the rim happen. */
	float Bezel = 18.f;
	/** Color split of the refraction (0..1). */
	float Aberration = 0.35f;
	/** Strength of the specular rim (0..1). */
	float Highlight = 0.6f;
	FVector2f LightPosition = FVector2f(-10000.f, -10000.f);
	FLinearColor Tint = FLinearColor(1.f, 1.f, 1.f, 0.04f);
	float Saturation = 1.2f;
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
	/**
	 * Registers a glass and returns its composite element. Glass shapes can still be added to Params on the game thread
	 * until the end of the frame (a glass group gets its children's shapes after its own paint).
	 */
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> AddGlass(const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe>& Params);
	/** Registers a backdrop without a composite element (a standalone backdrop draws both). */
	int32 AddItem(const FVekaBackdropParams& Params) { return Items.Add(Params); }
	void SetQuality(EVekaBackdropQuality InQuality) { Quality = InQuality; }
	bool IsEmpty() const { return Items.IsEmpty() && Glasses.IsEmpty(); }
	int32 Num() const { return Items.Num() + Glasses.Num(); }
	/** A capture element drawn before the host's content: the layer's backdrops then see only what is behind the host (nullptr when empty). */
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> MakeCaptureElement();

private:
	friend class FVekaBackdropCaptureElement;
	friend class FVekaBackdropCompositeElement;
	friend class FVekaGlassCompositeElement;

	// Game thread, then read only by the render thread.
	TArray<FVekaBackdropParams> Items;
	TArray<TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe>> Glasses;
	EVekaBackdropQuality Quality = EVekaBackdropQuality::High;

	// Render thread: the pyramid of this frame (pooled, so composites in another graph can register it).
	FIntRect CaptureRect;
	/** Blurred textures by level (0 = the copy), all covering CaptureRect. */
	TArray<TRefCountPtr<struct IPooledRenderTarget>> Levels;
	float PixelScale = 1.f;
	bool bCaptured = false;
	/** The capture is taken by the first composite drawn (so it sees what the host drew before it). */
	bool bCaptureTried = false;
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

	/** Elements of a blend against the backdrop (see FVekaBlendParams). */
	VEKAUIRENDER_API FVekaBlendElements MakeBlendElements(const FVekaBlendParams& Params);
}
