// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/VekaPaint.h"
#include "Vector/VekaPath.h"
#include "Typography/VekaFont.h"
#include "VekaSymbol.generated.h"

/** Role of a symbol layer, used by the hierarchical and palette rendering modes (Alpha 13: red / green / blue). */
UENUM(BlueprintType)
enum class EVekaSymbolRole : uint8
{
	Primary,
	Secondary,
	Tertiary
};

UENUM(BlueprintType)
enum class EVekaSymbolShapeKind : uint8
{
	/** Center line drawn with the symbol weight (round caps and joins). */
	Stroke,
	/** Filled outline (weight-independent). */
	Fill
};

/**
 * One shape of a symbol, in the 24 x 24 design grid (y down; live area about 3..21, see the "keylinegrid" symbol).
 * Built-in symbols are stroke skeletons: their weight is the stroke width, so the 9 weights (and any value in
 * between) need no masters and animate continuously.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSymbolShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolShapeKind Kind = EVekaSymbolShapeKind::Stroke;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVekaPath Path;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolRole Role = EVekaSymbolRole::Primary;

	/** Layer of the variable value (0 = lit first), or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	int32 VariableIndex = -1;

	/** Closed outline that the .fill variant fills (shapes drawn inside it become knockouts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bFillable = false;

	/** Multicolor mode: theme token or explicit color (unset = foreground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FName ColorToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FLinearColor Color = FLinearColor::Transparent;

	/** Stroke width multiplier (dots, thin details). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "0"))
	float StrokeScale = 1.f;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSymbolDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	TArray<FString> Keywords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	TArray<FVekaSymbolShape> Shapes;

	/** Number of variable layers (wifi arcs, bars...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "0"))
	int32 VariableSteps = 0;

	/** Rotation / bounce anchor in the design grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVector2D Anchor = FVector2D(12.0, 12.0);
};

/**
 * A user symbol (imported from SVG, edited in the Symbol Editor). Used by name like a built-in symbol
 * (`Symbol(#"logo")`): loaded symbols register themselves (Definition.Name, or the asset name), the editor loads all.
 */
UCLASS(BlueprintType)
class VEKAUI_API UVekaSymbol : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Symbol")
	FVekaSymbolDefinition Definition;

	/** Name views use: Definition.Name, or the asset name. */
	FName GetSymbolName() const { return Definition.Name.IsNone() ? GetFName() : Definition.Name; }

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
};

/** A set of symbols, registered with Veka::Symbols::RegisterLibrary (looked up before the built-in ones). */
UCLASS(BlueprintType)
class VEKAUI_API UVekaSymbolLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Symbol")
	TArray<TObjectPtr<UVekaSymbol>> Symbols;
};

UENUM(BlueprintType)
enum class EVekaSymbolRenderingMode : uint8
{
	/** Every layer in the foreground paint. */
	Monochrome,
	/** One paint, layers at 100 / 60 / 30 % (Colora hierarchy). */
	Hierarchical,
	/** One paint per role (SymbolPalette). */
	Palette,
	/** The symbol's own colors (tokens), foreground elsewhere. */
	Multicolor
};

UENUM(BlueprintType)
enum class EVekaSymbolScale : uint8
{
	Small,
	Medium,
	Large
};

UENUM(BlueprintType)
enum class EVekaSymbolEnclosure : uint8
{
	None,
	Circle,
	Square
};

/** Alpha 13 variants (Default, Circle, Slash, SlashCircle) plus fill and square. Also written in names: "bell.slash.fill". */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSymbolVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bFill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolEnclosure Enclosure = EVekaSymbolEnclosure::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bSlash = false;

	bool operator==(const FVekaSymbolVariant& Other) const { return bFill == Other.bFill && Enclosure == Other.Enclosure && bSlash == Other.bSlash; }
	bool operator!=(const FVekaSymbolVariant& Other) const { return !(*this == Other); }
	/** Union of two variants (a name suffix adds to the environment variant). */
	FVekaSymbolVariant Merged(const FVekaSymbolVariant& Other) const;
};

UENUM(BlueprintType)
enum class EVekaSymbolEffectKind : uint8
{
	None,
	Appear,
	Disappear,
	Bounce,
	Pulse,
	/** Scales up while active (indefinite) or once (discrete). */
	Scale,
	/** Lights the variable layers one after the other. */
	VariableColor,
	Wiggle,
	Rotate,
	Breathe,
	/** Strokes are drawn along their length. */
	DrawOn
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSymbolEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolEffectKind Kind = EVekaSymbolEffectKind::None;

	/** Layers start one after the other (by role). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bByLayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "0.05"))
	float Speed = 1.f;

	bool operator==(const FVekaSymbolEffect& Other) const { return Kind == Other.Kind && bByLayer == Other.bByLayer && Speed == Other.Speed; }
};

/** Placement of one layer by an effect. */
struct FVekaSymbolLayerTransform
{
	FVector2D Offset = FVector2D::ZeroVector;
	float Scale = 1.f;
	float Rotation = 0.f;
	float Opacity = 1.f;
	/** Drawn part of the strokes (DrawOn). */
	float Trim = 1.f;
	/** Variable layers lit by VariableColor (-1 = not driven). */
	float VariableHighlight = -1.f;
};

namespace Veka::Symbols
{
	/** Symbol by name: registered libraries first, then the built-in library, then the Alpha 13 aliases. */
	VEKAUI_API const FVekaSymbolDefinition* Find(FName Name);

	/**
	 * Splits "bell.slash.fill" into a known symbol and its variant suffixes (.fill, .circle, .square, .slash).
	 * Returns nullptr if no symbol matches.
	 */
	VEKAUI_API const FVekaSymbolDefinition* Resolve(FName FullName, FVekaSymbolVariant& OutVariant);

	VEKAUI_API void RegisterLibrary(UVekaSymbolLibrary* Library);
	VEKAUI_API void UnregisterLibrary(UVekaSymbolLibrary* Library);

	/** A standalone symbol asset (looked up after the libraries, before the built-in symbols). */
	VEKAUI_API void RegisterSymbol(UVekaSymbol* Symbol);
	VEKAUI_API void UnregisterSymbol(UVekaSymbol* Symbol);
	/** A definition changed (Symbol Editor): drops its cached geometry; views draw it again. */
	VEKAUI_API void NotifyChanged(const FVekaSymbolDefinition& Definition);
	/** Bumped by NotifyChanged (part of the symbol drawing cache key). */
	VEKAUI_API uint32 GetRevision();
	/** Every symbol name: libraries, registered symbols, built-in. */
	VEKAUI_API TArray<FName> GetAllNames();

	/** Names of the built-in symbols, in library order. */
	VEKAUI_API TArray<FName> GetBuiltInNames();
	/** Alpha 13 glyph name ("Info", "Warning"...) -> Veka symbol name. */
	VEKAUI_API const TMap<FName, FName>& GetAlpha13Aliases();

	/** Flattened polylines of a shape, in design units (cached per definition). */
	VEKAUI_API const TArray<FVekaPolyline>& GetShapeGeometry(const FVekaSymbolDefinition& Definition, int32 ShapeIndex);

	/** Symbol weight 0 (ultralight) .. 1 (black) matching a font weight. */
	VEKAUI_API float WeightFromFont(EVekaFontWeight Weight);
	/** Stroke width in design units for a weight. */
	VEKAUI_API float StrokeWidth(float Weight);
	/** Size of the symbol box relative to the font size (Small / Medium / Large). */
	VEKAUI_API float ScaleFactor(EVekaSymbolScale Scale);

	/** Placement of a layer (0..2) by an effect, Elapsed seconds after it started. Active = indefinite effect is on. */
	VEKAUI_API FVekaSymbolLayerTransform EvaluateEffect(const FVekaSymbolEffect& Effect, int32 Layer, double Elapsed, bool bActive, int32 VariableSteps);
	/** True once a one-shot effect has finished (indefinite effects never finish while active). */
	VEKAUI_API bool IsEffectFinished(const FVekaSymbolEffect& Effect, double Elapsed, bool bActive, int32 VariableSteps = 3);
	/** Effects that keep running while active (Pulse, VariableColor, Rotate, Breathe, Scale; Bounce and Wiggle repeat). */
	VEKAUI_API bool IsIndefinite(EVekaSymbolEffectKind Kind);
}
