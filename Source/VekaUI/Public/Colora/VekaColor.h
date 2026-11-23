// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VekaColor.generated.h"

/** Space in which gradients and color animations interpolate. */
UENUM(BlueprintType)
enum class EVekaColorSpace : uint8
{
	/** Gamma-encoded sRGB (what most design tools do; muddy midpoints between complementary colors). */
	SRGB,
	/** Linear light. */
	Linear,
	/** Perceptual Oklab: even, clean gradients (default). */
	OkLab,
	/** Oklab in polar form (lightness, chroma, hue): keeps saturation, hue takes the shortest way. */
	OkLCH
};

/**
 * How content is composited with what is under it. Normal and Add are drawn directly; the others (W3C compositing
 * modes) blend the subtree with its backdrop (Phase 4: the content is painted over black and over white, then blended).
 */
UENUM(BlueprintType)
enum class EVekaBlendMode : uint8
{
	Normal,
	/** Additive (light-like). Rendered with premultiplied alpha and zero coverage alpha. */
	Add,
	Multiply,
	Screen,
	Overlay,
	Darken,
	Lighten,
	ColorDodge,
	ColorBurn,
	HardLight,
	SoftLight,
	Difference,
	Exclusion,
	Hue,
	Saturation,
	Color,
	Luminosity
};

/** Color adjustments, cf. SwiftUI's brightness / contrast / saturation / hueRotation / grayscale. */
UENUM(BlueprintType)
enum class EVekaColorAdjust : uint8
{
	/** Adds Amount to each channel (-1..1). */
	Brightness,
	/** Scales around mid-gray (1 = unchanged). */
	Contrast,
	/** 0 = gray, 1 = unchanged, >1 = more saturated. */
	Saturation,
	/** Degrees. */
	HueRotation,
	/** 0 = unchanged, 1 = fully gray. */
	Grayscale
};

/** Oklab color (L in 0..1, a and b roughly -0.4..0.4). See https://bottosson.github.io/posts/oklab/ */
struct FVekaOkLab
{
	float L = 0.f;
	float A = 0.f;
	float B = 0.f;
};

/** Oklab in polar form. Hue in degrees 0..360. */
struct FVekaOkLCH
{
	float L = 0.f;
	float C = 0.f;
	float H = 0.f;
};

/**
 * 4x5 color matrix applied to gamma-encoded sRGB + alpha (like CSS / SVG feColorMatrix).
 * Out = M * (r, g, b, a, 1).
 */
struct VEKAUI_API FVekaColorMatrix
{
	float M[4][5];

	FVekaColorMatrix();

	static FVekaColorMatrix Identity() { return FVekaColorMatrix(); }
	static FVekaColorMatrix Make(EVekaColorAdjust Adjust, float Amount);

	bool IsIdentity() const;

	/** Matrix that applies Inner first, then this one. */
	FVekaColorMatrix Then(const FVekaColorMatrix& Outer) const;

	/** Applies to a linear color (converted to sRGB and back). */
	FLinearColor Apply(const FLinearColor& Linear) const;

	bool operator==(const FVekaColorMatrix& Other) const;
	bool operator!=(const FVekaColorMatrix& Other) const { return !(*this == Other); }
};

namespace Veka::Colora
{
	VEKAUI_API FVekaOkLab LinearToOkLab(const FLinearColor& Linear);
	VEKAUI_API FLinearColor OkLabToLinear(const FVekaOkLab& Lab, float Alpha = 1.f);
	VEKAUI_API FVekaOkLCH OkLabToOkLCH(const FVekaOkLab& Lab);
	VEKAUI_API FVekaOkLab OkLCHToOkLab(const FVekaOkLCH& Lch);

	VEKAUI_API float LinearToSRGB(float Value);
	VEKAUI_API float SRGBToLinear(float Value);

	/**
	 * Color in the components of Space (sRGB / linear: r g b; Oklab: L a b; OkLCH: L C H), alpha premultiplied
	 * into the color components so that transparent stops do not darken the gradient. W = alpha.
	 */
	VEKAUI_API FVector4f ToSpace(const FLinearColor& Linear, EVekaColorSpace Space);
	VEKAUI_API FLinearColor FromSpace(const FVector4f& Value, EVekaColorSpace Space);

	/** Interpolation of two space values (hue takes the shortest path in OkLCH). */
	VEKAUI_API FVector4f LerpInSpace(const FVector4f& A, const FVector4f& B, float T, EVekaColorSpace Space);

	/** A to B at T, interpolated in Space. Inputs and output are linear colors. */
	VEKAUI_API FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, float T, EVekaColorSpace Space = EVekaColorSpace::OkLab);
}
