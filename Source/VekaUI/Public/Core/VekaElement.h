// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaTypes.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaModifier.h"
#include "Colora/VekaColor.h"
#include "Anima/VekaAnimationSpec.h"
#include "Anima/VekaTransition.h"
#include "Typography/VekaTypography.h"

class FVekaView;
class UVekaTheme;
struct FVekaElementData;
struct FVekaPaint;
struct FVekaStrokeStyle;
struct FVekaSymbolStyle;
struct FVekaSymbolVariant;
struct FVekaSymbolEffect;
enum class EVekaSymbolStyleField : uint8;
enum class EVekaSymbolRenderingMode : uint8;
enum class EVekaSymbolScale : uint8;
enum class EVekaSymbolReplace : uint8;

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

	// --- Typography (applies to every Text of the subtree) -----------------------------------

	FVekaElement TypographyField(EVekaTypographyField Field, const FVekaTypography& Value) const& { return FVekaElement(*this).TypographyField(Field, Value); }
	FVekaElement TypographyField(EVekaTypographyField Field, const FVekaTypography& Value) &&;
	/** Semantic text style (size, weight, line height, optical tracking from the theme). */
	FVekaElement Font(EVekaTextStyle InStyle) const& { return FVekaElement(*this).Font(InStyle); }
	FVekaElement Font(EVekaTextStyle InStyle) && { FVekaTypography V; V.Style = InStyle; return MoveTemp(*this).TypographyField(EVekaTypographyField::Style, V); }
	/** One of the 12 weights. */
	FVekaElement FontWeight(EVekaFontWeight InWeight) const& { return FVekaElement(*this).FontWeight(InWeight); }
	FVekaElement FontWeight(EVekaFontWeight InWeight) && { FVekaTypography V; V.Weight = InWeight; return MoveTemp(*this).TypographyField(EVekaTypographyField::Weight, V); }
	FVekaElement Italic(bool bInItalic = true) const& { return FVekaElement(*this).Italic(bInItalic); }
	FVekaElement Italic(bool bInItalic = true) && { FVekaTypography V; V.bItalic = bInItalic; return MoveTemp(*this).TypographyField(EVekaTypographyField::Italic, V); }
	/** Size in Slate points (replaces the style size, keeps the weight). */
	FVekaElement FontSize(float InSize) const& { return FVekaElement(*this).FontSize(InSize); }
	FVekaElement FontSize(float InSize) && { FVekaTypography V; V.Size = InSize; return MoveTemp(*this).TypographyField(EVekaTypographyField::Size, V); }
	FVekaElement FontFamily(UVekaFontFamily* InFamily) const& { return FVekaElement(*this).FontFamily(InFamily); }
	FVekaElement FontFamily(UVekaFontFamily* InFamily) && { FVekaTypography V; V.Family = InFamily; return MoveTemp(*this).TypographyField(EVekaTypographyField::Family, V); }
	/** Letter spacing in em (0.05 = 5 %); default is the optical tracking of the size. */
	FVekaElement Tracking(float Em) const& { return FVekaElement(*this).Tracking(Em); }
	FVekaElement Tracking(float Em) && { FVekaTypography V; V.Tracking = Em; return MoveTemp(*this).TypographyField(EVekaTypographyField::Tracking, V); }
	/** Line height multiplier. */
	FVekaElement LineSpacing(float Multiplier) const& { return FVekaElement(*this).LineSpacing(Multiplier); }
	FVekaElement LineSpacing(float Multiplier) && { FVekaTypography V; V.LineSpacing = Multiplier; return MoveTemp(*this).TypographyField(EVekaTypographyField::LineSpacing, V); }
	FVekaElement TextCase(EVekaTextCase InCase) const& { return FVekaElement(*this).TextCase(InCase); }
	FVekaElement TextCase(EVekaTextCase InCase) && { FVekaTypography V; V.Case = InCase; return MoveTemp(*this).TypographyField(EVekaTypographyField::Case, V); }
	/** Equal-width digits (counters, timers, tables). */
	FVekaElement MonospacedDigit(bool bOn = true) const& { return FVekaElement(*this).MonospacedDigit(bOn); }
	FVekaElement MonospacedDigit(bool bOn = true) && { FVekaTypography V; V.bMonospacedDigits = bOn; return MoveTemp(*this).TypographyField(EVekaTypographyField::MonospacedDigits, V); }
	/** 0 = unlimited. */
	FVekaElement LineLimit(int32 Lines) const& { return FVekaElement(*this).LineLimit(Lines); }
	FVekaElement LineLimit(int32 Lines) && { FVekaTypography V; V.LineLimit = Lines; return MoveTemp(*this).TypographyField(EVekaTypographyField::LineLimit, V); }
	FVekaElement Truncation(EVekaTextTruncation Mode) const& { return FVekaElement(*this).Truncation(Mode); }
	FVekaElement Truncation(EVekaTextTruncation Mode) && { FVekaTypography V; V.Truncation = Mode; return MoveTemp(*this).TypographyField(EVekaTypographyField::Truncation, V); }
	FVekaElement MultilineAlignment(EVekaHAlign InAlignment) const& { return FVekaElement(*this).MultilineAlignment(InAlignment); }
	FVekaElement MultilineAlignment(EVekaHAlign InAlignment) && { FVekaTypography V; V.Alignment = InAlignment; return MoveTemp(*this).TypographyField(EVekaTypographyField::Alignment, V); }
	/** Text may shrink down to Factor to fit before it is truncated. */
	FVekaElement MinimumScaleFactor(float Factor) const& { return FVekaElement(*this).MinimumScaleFactor(Factor); }
	FVekaElement MinimumScaleFactor(float Factor) && { FVekaTypography V; V.MinimumScaleFactor = Factor; return MoveTemp(*this).TypographyField(EVekaTypographyField::MinimumScaleFactor, V); }
	/** How the text animates when its content changes (NumericText: rolling digits). */
	FVekaElement ContentTransition(EVekaContentTransition Transition, float Duration = 0.35f) const& { return FVekaElement(*this).ContentTransition(Transition, Duration); }
	FVekaElement ContentTransition(EVekaContentTransition Transition, float Duration = 0.35f) && { FVekaTypography V; V.ContentTransition = Transition; V.ContentTransitionDuration = Duration; return MoveTemp(*this).TypographyField(EVekaTypographyField::ContentTransition, V); }
	/** Per-glyph effect (appear, typewriter, wave). */
	FVekaElement TextEffect(const FVekaTextEffect& Effect) const& { return FVekaElement(*this).TextEffect(Effect); }
	FVekaElement TextEffect(const FVekaTextEffect& Effect) && { FVekaTypography V; V.Effect = Effect; return MoveTemp(*this).TypographyField(EVekaTypographyField::Effect, V); }
	/** Global text size multiplier of the subtree (accessibility). */
	FVekaElement TextScale(float Scale) const& { return FVekaElement(*this).TextScale(Scale); }
	FVekaElement TextScale(float Scale) && { FVekaTypography V; V.Scale = Scale; return MoveTemp(*this).TypographyField(EVekaTypographyField::Scale, V); }

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

	// --- Anima -----------------------------------------------------------------------------------

	/**
	 * Animates every change of this view and its subtree with Spec when Value changes (SwiftUI's
	 * `.animation(_:value:)`). Value can be anything hashable (GetTypeHash).
	 */
	template <typename TValue>
	FVekaElement Animation(const FVekaAnimationSpec& Spec, const TValue& Value) const& { return FVekaElement(*this).AnimationKey(Spec, static_cast<int64>(GetTypeHash(Value))); }
	template <typename TValue>
	FVekaElement Animation(const FVekaAnimationSpec& Spec, const TValue& Value) && { return MoveTemp(*this).AnimationKey(Spec, static_cast<int64>(GetTypeHash(Value))); }
	FVekaElement AnimationKey(const FVekaAnimationSpec& Spec, int64 ValueKey) const& { return FVekaElement(*this).AnimationKey(Spec, ValueKey); }
	FVekaElement AnimationKey(const FVekaAnimationSpec& Spec, int64 ValueKey) &&;

	/** How this view enters / leaves inside an animation transaction (default: opacity). */
	FVekaElement Transition(const FVekaTransition& InTransition) const& { return FVekaElement(*this).Transition(InTransition); }
	FVekaElement Transition(const FVekaTransition& InTransition) &&;

	// --- Symbols ---------------------------------------------------------------------------------

	/** Sets one field of the symbol style of the subtree (the methods below are shortcuts). */
	FVekaElement SymbolStyleField(EVekaSymbolStyleField Field, const FVekaSymbolStyle& Value) const& { return FVekaElement(*this).SymbolStyleField(Field, Value); }
	FVekaElement SymbolStyleField(EVekaSymbolStyleField Field, const FVekaSymbolStyle& Value) &&;
	/** Monochrome, hierarchical (100 / 60 / 30 %), palette or multicolor. */
	FVekaElement SymbolRenderingMode(EVekaSymbolRenderingMode Mode) const& { return FVekaElement(*this).SymbolRenderingMode(Mode); }
	FVekaElement SymbolRenderingMode(EVekaSymbolRenderingMode Mode) &&;
	/** Adds a variant (fill, circle / square enclosure, slash) to the symbols of the subtree. */
	FVekaElement SymbolVariant(const FVekaSymbolVariant& Variant) const& { return FVekaElement(*this).SymbolVariant(Variant); }
	FVekaElement SymbolVariant(const FVekaSymbolVariant& Variant) &&;
	/** Small, medium or large symbols relative to the font (SwiftUI imageScale). */
	FVekaElement SymbolScale(EVekaSymbolScale Scale) const& { return FVekaElement(*this).SymbolScale(Scale); }
	FVekaElement SymbolScale(EVekaSymbolScale Scale) &&;
	/** Palette mode colors (primary, secondary, tertiary layers); also sets the palette mode. */
	FVekaElement SymbolPalette(const TArray<FVekaPaint>& Paints) const& { return FVekaElement(*this).SymbolPalette(Paints); }
	FVekaElement SymbolPalette(const TArray<FVekaPaint>& Paints) &&;
	/** Explicit symbol weight 0..1 (default: the font weight). */
	FVekaElement SymbolWeight(float Weight) const& { return FVekaElement(*this).SymbolWeight(Weight); }
	FVekaElement SymbolWeight(float Weight) &&;
	/** Indefinite effect while bIsActive (pulse, rotate, breathe...), or appear / disappear / draw on when it becomes true. */
	FVekaElement SymbolEffect(const FVekaSymbolEffect& Effect, bool bIsActive = true) const& { return FVekaElement(*this).SymbolEffect(Effect, bIsActive); }
	FVekaElement SymbolEffect(const FVekaSymbolEffect& Effect, bool bIsActive = true) &&;
	/** Plays the effect once every time Value changes (SwiftUI `.symbolEffect(_:value:)`). */
	template <typename TValue>
	FVekaElement SymbolEffectOnChange(const FVekaSymbolEffect& Effect, const TValue& Value) const& { return FVekaElement(*this).SymbolEffectKey(Effect, static_cast<int64>(GetTypeHash(Value))); }
	template <typename TValue>
	FVekaElement SymbolEffectOnChange(const FVekaSymbolEffect& Effect, const TValue& Value) && { return MoveTemp(*this).SymbolEffectKey(Effect, static_cast<int64>(GetTypeHash(Value))); }
	FVekaElement SymbolEffectKey(const FVekaSymbolEffect& Effect, int64 Trigger) const& { return FVekaElement(*this).SymbolEffectKey(Effect, Trigger); }
	FVekaElement SymbolEffectKey(const FVekaSymbolEffect& Effect, int64 Trigger) &&;
	/** How symbols animate when their name or variant changes. */
	FVekaElement SymbolReplace(EVekaSymbolReplace Mode, float Duration = 0.35f) const& { return FVekaElement(*this).SymbolReplace(Mode, Duration); }
	FVekaElement SymbolReplace(EVekaSymbolReplace Mode, float Duration = 0.35f) &&;
	/** Symbol element only: variable value 0..1 (-1 = off). */
	FVekaElement SymbolValue(float Value) const& { return FVekaElement(*this).SymbolValue(Value); }
	FVekaElement SymbolValue(float Value) &&;
	/** Symbol element only: badge count (> 0), dot (-1) or none (0). */
	FVekaElement SymbolBadge(int32 Count) const& { return FVekaElement(*this).SymbolBadge(Count); }
	FVekaElement SymbolBadge(int32 Count) &&;

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
