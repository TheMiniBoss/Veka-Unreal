// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaTypes.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaModifier.h"
#include "Colora/VekaColor.h"

class FVekaView;
class UVekaTheme;
struct FVekaElementData;
struct FVekaPaint;
struct FVekaStrokeStyle;

enum class EVekaElementKind : uint8
{
	/** Nothing (e.g. the false branch of If). Keeps its slot so sibling identity stays stable. */
	Empty,
	/** A primitive or container (FVekaPrimitive-derived struct). */
	Primitive,
	/** A composite view (FVekaView with a Body). */
	View,
	/** A list of elements flattened into the parent (ForEach). */
	Fragment
};

/**
 * Description of a piece of UI. Light value type: cheap to copy (shared, copy-on-write data).
 * Built by the functions in VekaBuilders.h, by a UVekaView asset (Phase 2) or by the DSL (Phase 3).
 *
 * Modifier methods return a new element; on temporaries no copy is made:
 *   Veka::Text(INVTEXT("Hi")).Padding(8).Background(FLinearColor::Red)
 */
class VEKAUI_API FVekaElement
{
public:
	FVekaElement() = default;

	static FVekaElement MakePrimitive(FInstancedStruct&& Primitive, TArray<FVekaElement>&& Children);
	static FVekaElement MakeView(TSharedRef<const FVekaView> View);
	static FVekaElement MakeFragment(TArray<FVekaElement>&& Children);

	template <typename TPrimitive>
	static FVekaElement Make(const TPrimitive& Primitive, TArray<FVekaElement> Children = TArray<FVekaElement>())
	{
		static_assert(TIsDerivedFrom<TPrimitive, FVekaPrimitive>::Value, "TPrimitive must derive from FVekaPrimitive");
		return MakePrimitive(FInstancedStruct::Make(Primitive), MoveTemp(Children));
	}

	// --- Read access ---------------------------------------------------------------------------

	bool IsEmpty() const;
	EVekaElementKind GetKind() const;
	const FInstancedStruct& GetPrimitive() const;
	TSharedPtr<const FVekaView> GetView() const;
	const TArray<FInstancedStruct>& GetModifiers() const;
	const TArray<FVekaElement>& GetChildren() const;
	/** Explicit identity set with Id(), or None. */
	FName GetId() const;
	/** Struct name for primitives, view type for views. Used to decide if a node can be reused. */
	FName GetTypeName() const;

	// --- Generic modifiers (extension API) -----------------------------------------------------

	/** Appends any FVekaModifier-derived struct. */
	template <typename TModifier>
	FVekaElement Modifier(const TModifier& InModifier) const&
	{
		return FVekaElement(*this).Modifier(InModifier);
	}
	template <typename TModifier>
	FVekaElement Modifier(const TModifier& InModifier) &&
	{
		static_assert(TIsDerivedFrom<TModifier, FVekaModifier>::Value, "TModifier must derive from FVekaModifier");
		AddModifierInternal(FInstancedStruct::Make(InModifier));
		return MoveTemp(*this);
	}

	FVekaElement AddModifier(FInstancedStruct&& InModifier) &&;

	/** Explicit identity among siblings (lists, matched geometry later). */
	FVekaElement Id(FName InId) const& { return FVekaElement(*this).Id(InId); }
	FVekaElement Id(FName InId) &&;

	// --- Layout ----------------------------------------------------------------------------------

	FVekaElement Padding(float All) const& { return FVekaElement(*this).Padding(All); }
	FVekaElement Padding(float All) &&;
	FVekaElement Padding(const FMargin& Insets) const& { return FVekaElement(*this).Padding(Insets); }
	FVekaElement Padding(const FMargin& Insets) &&;

	/** Fixed size. Pass a negative value to leave a dimension free. */
	FVekaElement Frame(double Width, double Height, FVekaAlignment Alignment = FVekaAlignment()) const& { return FVekaElement(*this).Frame(Width, Height, Alignment); }
	FVekaElement Frame(double Width, double Height, FVekaAlignment Alignment = FVekaAlignment()) &&;

	/** Flexible frame. Pass a negative value to leave a bound unset; Veka::Infinity to fill. */
	FVekaElement FlexFrame(double MinWidth, double MaxWidth, double MinHeight, double MaxHeight, FVekaAlignment Alignment = FVekaAlignment()) const& { return FVekaElement(*this).FlexFrame(MinWidth, MaxWidth, MinHeight, MaxHeight, Alignment); }
	FVekaElement FlexFrame(double MinWidth, double MaxWidth, double MinHeight, double MaxHeight, FVekaAlignment Alignment = FVekaAlignment()) &&;

	/** Shortcut for FlexFrame(-1, Infinity, -1, -1): take all the width offered. */
	FVekaElement FillWidth(FVekaAlignment Alignment = FVekaAlignment()) const& { return FVekaElement(*this).FillWidth(Alignment); }
	FVekaElement FillWidth(FVekaAlignment Alignment = FVekaAlignment()) &&;
	FVekaElement FillHeight(FVekaAlignment Alignment = FVekaAlignment()) const& { return FVekaElement(*this).FillHeight(Alignment); }
	FVekaElement FillHeight(FVekaAlignment Alignment = FVekaAlignment()) &&;

	FVekaElement LayoutPriority(double Priority) const& { return FVekaElement(*this).LayoutPriority(Priority); }
	FVekaElement LayoutPriority(double Priority) &&;

	// --- Appearance ------------------------------------------------------------------------------

	FVekaElement Background(const FVekaPaint& Paint, const FVekaCornerRadii& Radii = FVekaCornerRadii()) const& { return FVekaElement(*this).Background(Paint, Radii); }
	FVekaElement Background(const FVekaPaint& Paint, const FVekaCornerRadii& Radii = FVekaCornerRadii()) &&;

	FVekaElement Border(const FVekaPaint& Paint, float Width = 1.f, const FVekaCornerRadii& Radii = FVekaCornerRadii()) const& { return FVekaElement(*this).Border(Paint, Width, Radii); }
	FVekaElement Border(const FVekaPaint& Paint, float Width = 1.f, const FVekaCornerRadii& Radii = FVekaCornerRadii()) &&;

	FVekaElement Shadow(const FLinearColor& Color, float Radius, const FVector2D& Offset = FVector2D::ZeroVector, const FVekaCornerRadii& Radii = FVekaCornerRadii()) const& { return FVekaElement(*this).Shadow(Color, Radius, Offset, Radii); }
	FVekaElement Shadow(const FLinearColor& Color, float Radius, const FVector2D& Offset = FVector2D::ZeroVector, const FVekaCornerRadii& Radii = FVekaCornerRadii()) &&;

	FVekaElement Opacity(float Value) const& { return FVekaElement(*this).Opacity(Value); }
	FVekaElement Opacity(float Value) &&;

	/** Clips the content to the node rect (rectangular clip; rounded clips come with the render module). */
	FVekaElement Clip() const& { return FVekaElement(*this).Clip(); }
	FVekaElement Clip() &&;

	FVekaElement Offset(const FVector2D& Delta) const& { return FVekaElement(*this).Offset(Delta); }
	FVekaElement Offset(const FVector2D& Delta) &&;
	FVekaElement Scale(const FVector2D& InScale) const& { return FVekaElement(*this).Scale(InScale); }
	FVekaElement Scale(const FVector2D& InScale) &&;
	FVekaElement Scale(float Uniform) const& { return FVekaElement(*this).Scale(FVector2D(Uniform, Uniform)); }
	FVekaElement Scale(float Uniform) && { return MoveTemp(*this).Scale(FVector2D(Uniform, Uniform)); }
	FVekaElement Rotation(float Degrees) const& { return FVekaElement(*this).Rotation(Degrees); }
	FVekaElement Rotation(float Degrees) &&;

	// --- Shapes (only valid on Shape / Color elements) --------------------------------------------

	FVekaElement Fill(const FVekaPaint& Paint) const& { return FVekaElement(*this).Fill(Paint); }
	FVekaElement Fill(const FVekaPaint& Paint) &&;
	FVekaElement Stroke(const FVekaStrokeStyle& Style) const& { return FVekaElement(*this).Stroke(Style); }
	FVekaElement Stroke(const FVekaStrokeStyle& Style) &&;
	FVekaElement Stroke(const FVekaPaint& Paint, float Width) const& { return FVekaElement(*this).Stroke(Paint, Width); }
	FVekaElement Stroke(const FVekaPaint& Paint, float Width) &&;
	/** Draws only part of the stroke, 0..1 along its length (animatable "draw-on" effect). */
	FVekaElement Trim(float From, float To) const& { return FVekaElement(*this).Trim(From, To); }
	FVekaElement Trim(float From, float To) &&;

	// --- Environment -----------------------------------------------------------------------------

	/** Color or theme token for text, symbols and foreground-filled shapes. */
	FVekaElement Foreground(const FVekaPaint& Paint) const& { return FVekaElement(*this).Foreground(Paint); }
	FVekaElement Foreground(const FVekaPaint& Paint) &&;
	FVekaElement Font(const FSlateFontInfo& InFont) const& { return FVekaElement(*this).Font(InFont); }
	FVekaElement Font(const FSlateFontInfo& InFont) &&;
	FVekaElement ColorScheme(EVekaColorScheme Scheme) const& { return FVekaElement(*this).ColorScheme(Scheme); }
	FVekaElement ColorScheme(EVekaColorScheme Scheme) &&;

	// --- Colora ----------------------------------------------------------------------------------

	/** Theme of the subtree (tokens and metrics). nullptr = built-in defaults. */
	FVekaElement Theme(UVekaTheme* InTheme) const& { return FVekaElement(*this).Theme(InTheme); }
	FVekaElement Theme(UVekaTheme* InTheme) &&;
	/** Color adjustments of everything drawn in the subtree (colors, gradients, text; textures get a tint only). */
	FVekaElement ColorAdjust(EVekaColorAdjust Adjust, float Amount) const& { return FVekaElement(*this).ColorAdjust(Adjust, Amount); }
	FVekaElement ColorAdjust(EVekaColorAdjust Adjust, float Amount) &&;
	FVekaElement Brightness(float Amount) const& { return ColorAdjust(EVekaColorAdjust::Brightness, Amount); }
	FVekaElement Brightness(float Amount) && { return MoveTemp(*this).ColorAdjust(EVekaColorAdjust::Brightness, Amount); }
	FVekaElement Contrast(float Amount) const& { return ColorAdjust(EVekaColorAdjust::Contrast, Amount); }
	FVekaElement Contrast(float Amount) && { return MoveTemp(*this).ColorAdjust(EVekaColorAdjust::Contrast, Amount); }
	FVekaElement Saturation(float Amount) const& { return ColorAdjust(EVekaColorAdjust::Saturation, Amount); }
	FVekaElement Saturation(float Amount) && { return MoveTemp(*this).ColorAdjust(EVekaColorAdjust::Saturation, Amount); }
	FVekaElement HueRotation(float Degrees) const& { return ColorAdjust(EVekaColorAdjust::HueRotation, Degrees); }
	FVekaElement HueRotation(float Degrees) && { return MoveTemp(*this).ColorAdjust(EVekaColorAdjust::HueRotation, Degrees); }
	FVekaElement Grayscale(float Amount) const& { return ColorAdjust(EVekaColorAdjust::Grayscale, Amount); }
	FVekaElement Grayscale(float Amount) && { return MoveTemp(*this).ColorAdjust(EVekaColorAdjust::Grayscale, Amount); }
	/** Normal or Add (additive, light-like) for the shapes and gradients of the subtree. */
	FVekaElement BlendMode(EVekaBlendMode Mode) const& { return FVekaElement(*this).BlendMode(Mode); }
	FVekaElement BlendMode(EVekaBlendMode Mode) &&;

	// --- Interaction -----------------------------------------------------------------------------

	FVekaElement OnTap(TFunction<void()> Action) const& { return FVekaElement(*this).OnTap(MoveTemp(Action)); }
	FVekaElement OnTap(TFunction<void()> Action) &&;
	FVekaElement OnHover(TFunction<void(bool)> Action) const& { return FVekaElement(*this).OnHover(MoveTemp(Action)); }
	FVekaElement OnHover(TFunction<void(bool)> Action) &&;
	FVekaElement OnPress(TFunction<void(bool)> Action) const& { return FVekaElement(*this).OnPress(MoveTemp(Action)); }
	FVekaElement OnPress(TFunction<void(bool)> Action) &&;

private:
	void MakeUnique();
	void AddModifierInternal(FInstancedStruct&& InModifier);
	/** Mutable primitive of type T, or nullptr (with a warning) if the element is not a T. */
	template <typename T>
	T* MutablePrimitive(const TCHAR* Context);

	TSharedPtr<FVekaElementData> Data;
};

struct FVekaElementData
{
	EVekaElementKind Kind = EVekaElementKind::Empty;
	FInstancedStruct Primitive;
	TSharedPtr<const FVekaView> View;
	TArray<FInstancedStruct> Modifiers;
	TArray<FVekaElement> Children;
	FName Id;
};
