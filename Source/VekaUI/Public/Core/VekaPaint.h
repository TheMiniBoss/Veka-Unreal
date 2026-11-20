// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Core/VekaTypes.h"
#include "Colora/VekaColor.h"
#include "VekaPaint.generated.h"

struct FVekaPaintContext;
struct FVekaPolyline;
class FVekaEnvironment;
class UTexture;
class UMaterialInterface;

/**
 * What fills or strokes a shape (Colora). Gradients, mesh gradients and token paints are rendered by the Veka
 * tessellator (per-vertex colors computed in the gradient's color space, antialiased edges). Glass and backdrop
 * paints come with the render module (Phase 4).
 */
UENUM(BlueprintType)
enum class EVekaPaintType : uint8
{
	None,
	Solid,
	LinearGradient,
	/** Circles around Center; radii in local units (SwiftUI RadialGradient). */
	RadialGradient,
	/** Ellipse fitted to the shape; radii as fractions of the half size (SwiftUI EllipticalGradient). */
	EllipticalGradient,
	/** Sweep around Center from StartAngle to EndAngle, degrees, clockwise from +X (SwiftUI AngularGradient). */
	AngularGradient,
	/** Grid of colored points (replaces the Alpha 13 Gradient Matrix). */
	MeshGradient,
	/** Texture, stretched, tiled or 9-sliced. Color tints it. */
	Image,
	/** UI material. Color tints it. */
	Material,
	/** Semantic color of the theme (Colora token), resolved from the environment (theme + color scheme). */
	Token
};

/** Alpha levels of Colora (Alpha 13 hierarchy): primary 100 %, secondary 60 %, tertiary 30 %, quaternary 18 %. */
UENUM(BlueprintType)
enum class EVekaHierarchy : uint8
{
	Primary,
	Secondary,
	Tertiary,
	Quaternary,
	Transparent
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaGradientStop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FLinearColor Color = FLinearColor::White;

	/** 0..1 along the gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float Location = 0.f;

	/** If set, the color comes from this theme token (Color is ignored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FName Token;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	EVekaHierarchy Hierarchy = EVekaHierarchy::Primary;

	FVekaGradientStop() = default;
	FVekaGradientStop(const FLinearColor& InColor, float InLocation) : Color(InColor), Location(InLocation) {}
	static FVekaGradientStop FromToken(FName InToken, float InLocation, EVekaHierarchy InHierarchy = EVekaHierarchy::Primary)
	{
		FVekaGradientStop Stop;
		Stop.Token = InToken;
		Stop.Location = InLocation;
		Stop.Hierarchy = InHierarchy;
		return Stop;
	}
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaMeshPoint
{
	GENERATED_BODY()

	/** Unit-space position ((0,0) top-left, (1,1) bottom-right of the shape). Animate it for living backgrounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FLinearColor Color = FLinearColor::White;

	/** If set, the color comes from this theme token. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FName Token;
};

/**
 * Mesh gradient: Width x Height points, row by row (index = Y * Width + X). Colors are interpolated in the paint's
 * color space; bSmooth uses Catmull-Rom (bicubic) interpolation for positions and colors, otherwise bilinear.
 * Bézier handles per point come with the mesh gradient editor (Phase 3).
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaMeshGradient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "2"))
	int32 Width = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "2"))
	int32 Height = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TArray<FVekaMeshPoint> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	bool bSmooth = true;

	bool IsValid() const { return Width >= 2 && Height >= 2 && Points.Num() == Width * Height; }

	/** Regular grid with the given colors (row by row). Missing colors are white. */
	static FVekaMeshGradient Grid(int32 InWidth, int32 InHeight, TConstArrayView<FLinearColor> Colors, bool bInSmooth = true);

	FVekaMeshPoint& At(int32 X, int32 Y) { return Points[Y * Width + X]; }
	const FVekaMeshPoint& At(int32 X, int32 Y) const { return Points[Y * Width + X]; }
};

/** What fills or strokes a shape. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaPaint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	EVekaPaintType Type = EVekaPaintType::Solid;

	/** Solid color; tint for Image / Material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FLinearColor Color = FLinearColor::White;

	/** Token paints: name of the theme color (see Veka::Colora::DefaultTokenNames). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FName Token;

	/** Token paints: alpha level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	EVekaHierarchy Hierarchy = EVekaHierarchy::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TArray<FVekaGradientStop> Stops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	EVekaColorSpace ColorSpace = EVekaColorSpace::OkLab;

	/** Linear: unit-space start ((0,0) = top-left, (1,1) = bottom-right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D StartPoint = FVector2D(0.0, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D EndPoint = FVector2D(1.0, 0.5);

	/** Radial / elliptical / angular: unit-space center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D Center = FVector2D(0.5, 0.5);

	/** Radial: local units. Elliptical: fraction of the half size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "0"))
	float StartRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "0"))
	float EndRadius = 1.f;

	/** Angular: degrees, clockwise from +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	float StartAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	float EndAngle = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVekaMeshGradient Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TObjectPtr<UTexture> Image;

	/** Image: 9-slice margins (0..1 of the texture). Zero = stretched or tiled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FMargin ImageMargin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TEnumAsByte<ESlateBrushTileType::Type> ImageTiling = ESlateBrushTileType::NoTile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TObjectPtr<UMaterialInterface> Material;

	FVekaPaint() = default;
	/** Implicit: a color is a solid paint. */
	FVekaPaint(const FLinearColor& InColor) : Type(EVekaPaintType::Solid), Color(InColor) {}

	static FVekaPaint Solid(const FLinearColor& InColor) { return FVekaPaint(InColor); }
	static FVekaPaint Clear();
	/** Theme color (Colora token), e.g. FVekaPaint::FromToken("Accent"). */
	static FVekaPaint FromToken(FName InToken, EVekaHierarchy InHierarchy = EVekaHierarchy::Primary);
	static FVekaPaint Linear(TArray<FVekaGradientStop> InStops, const FVector2D& InStart = FVector2D(0.0, 0.5), const FVector2D& InEnd = FVector2D(1.0, 0.5));
	static FVekaPaint Linear(const FLinearColor& From, const FLinearColor& To, const FVector2D& InStart = FVector2D(0.0, 0.5), const FVector2D& InEnd = FVector2D(1.0, 0.5));
	static FVekaPaint Radial(TArray<FVekaGradientStop> InStops, float InStartRadius, float InEndRadius, const FVector2D& InCenter = FVector2D(0.5, 0.5));
	static FVekaPaint Elliptical(TArray<FVekaGradientStop> InStops, float InStartFraction = 0.f, float InEndFraction = 1.f, const FVector2D& InCenter = FVector2D(0.5, 0.5));
	static FVekaPaint Angular(TArray<FVekaGradientStop> InStops, float InStartAngle = 0.f, float InEndAngle = 360.f, const FVector2D& InCenter = FVector2D(0.5, 0.5));
	static FVekaPaint FromMesh(const FVekaMeshGradient& InMesh);
	static FVekaPaint FromImage(UTexture* InTexture, const FMargin& InNineSlice = FMargin(), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile);
	static FVekaPaint FromMaterial(UMaterialInterface* InMaterial);

	/** Same paint, interpolated in another space. */
	FVekaPaint InSpace(EVekaColorSpace InSpace) const { FVekaPaint Copy(*this); Copy.ColorSpace = InSpace; return Copy; }

	/** Linear, radial, elliptical, angular or mesh: drawn by the Veka tessellator. */
	bool IsGradient() const;

	bool IsVisible() const;

	/** Representative color (for places that cannot draw a gradient yet, like text). */
	FLinearColor GetAverageColor() const;

	/**
	 * Color of a resolved paint (no token) at a local point. Bounds is the rect the paint is fitted to.
	 * For many points use Veka::Colora::FPaintSampler (precomputes stops and mesh).
	 */
	FLinearColor Evaluate(const FVector2D& LocalPoint, const FVekaRect& Bounds) const;

	/**
	 * Draws the paint in Rect with rounded corners. Tokens are resolved from Context.Environment. BrushCache must
	 * stay alive until the frame is rendered (keep it in the modifier or the primitive state, never on the stack).
	 */
	int32 Draw(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, FSlateBrush& BrushCache) const;

	/** Draws only an outline of Width inside Rect. */
	int32 DrawOutline(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, float Width, FSlateBrush& BrushCache) const;
};
