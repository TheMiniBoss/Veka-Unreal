// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaElement.h"
#include "Core/VekaView.h"
#include "Core/VekaPaint.h"
#include "Core/VekaEnvironment.h"
#include "Modifiers/VekaLayoutModifiers.h"
#include "Modifiers/VekaVisualModifiers.h"
#include "Modifiers/VekaEnvironmentModifiers.h"
#include "Modifiers/VekaInteractionModifiers.h"
#include "Modifiers/VekaColoraModifiers.h"
#include "Modifiers/VekaAnimaModifiers.h"
#include "Primitives/VekaShapeView.h"
#include "Symbols/VekaSymbolView.h"
#include "Vector/VekaStroke.h"
#include "VekaUILog.h"

namespace
{
	const FInstancedStruct& EmptyInstancedStruct()
	{
		static const FInstancedStruct Empty;
		return Empty;
	}

	const TArray<FInstancedStruct>& EmptyModifiers()
	{
		static const TArray<FInstancedStruct> Empty;
		return Empty;
	}

	const TArray<FVekaElement>& EmptyChildren()
	{
		static const TArray<FVekaElement> Empty;
		return Empty;
	}
}

FVekaElement FVekaElement::MakePrimitive(FInstancedStruct&& Primitive, TArray<FVekaElement>&& Children)
{
	FVekaElement Element;
	if (!Primitive.IsValid() || !Primitive.GetScriptStruct()->IsChildOf(FVekaPrimitive::StaticStruct()))
	{
		UE_LOG(LogVekaUI, Warning, TEXT("FVekaElement::MakePrimitive: struct is not an FVekaPrimitive"));
		return Element;
	}
	Element.Data = MakeShared<FVekaElementData>();
	Element.Data->Kind = EVekaElementKind::Primitive;
	Element.Data->Primitive = MoveTemp(Primitive);
	Element.Data->Children = MoveTemp(Children);
	return Element;
}

FVekaElement FVekaElement::MakeView(TSharedRef<const FVekaView> View)
{
	FVekaElement Element;
	Element.Data = MakeShared<FVekaElementData>();
	Element.Data->Kind = EVekaElementKind::View;
	Element.Data->View = View;
	return Element;
}

FVekaElement FVekaElement::MakeFragment(TArray<FVekaElement>&& Children)
{
	FVekaElement Element;
	Element.Data = MakeShared<FVekaElementData>();
	Element.Data->Kind = EVekaElementKind::Fragment;
	Element.Data->Children = MoveTemp(Children);
	return Element;
}

bool FVekaElement::IsEmpty() const
{
	return !Data.IsValid() || Data->Kind == EVekaElementKind::Empty;
}

EVekaElementKind FVekaElement::GetKind() const
{
	return Data.IsValid() ? Data->Kind : EVekaElementKind::Empty;
}

const FInstancedStruct& FVekaElement::GetPrimitive() const
{
	return Data.IsValid() ? Data->Primitive : EmptyInstancedStruct();
}

TSharedPtr<const FVekaView> FVekaElement::GetView() const
{
	return Data.IsValid() ? Data->View : nullptr;
}

const TArray<FInstancedStruct>& FVekaElement::GetModifiers() const
{
	return Data.IsValid() ? Data->Modifiers : EmptyModifiers();
}

const TArray<FVekaElement>& FVekaElement::GetChildren() const
{
	return Data.IsValid() ? Data->Children : EmptyChildren();
}

FName FVekaElement::GetId() const
{
	return Data.IsValid() ? Data->Id : NAME_None;
}

FName FVekaElement::GetTypeName() const
{
	if (!Data.IsValid())
	{
		return NAME_None;
	}
	switch (Data->Kind)
	{
	case EVekaElementKind::Primitive:
		return Data->Primitive.GetScriptStruct() ? Data->Primitive.GetScriptStruct()->GetFName() : NAME_None;
	case EVekaElementKind::View:
		return Data->View.IsValid() ? Data->View->GetViewType() : NAME_None;
	default:
		return NAME_None;
	}
}

void FVekaElement::MakeUnique()
{
	if (!Data.IsValid())
	{
		// Modifiers on an empty element are kept on an empty primitive so they still apply (e.g. a spacer-like frame).
		Data = MakeShared<FVekaElementData>();
		Data->Kind = EVekaElementKind::Primitive;
		Data->Primitive = FInstancedStruct::Make(FVekaPrimitive());
		return;
	}
	if (Data.GetSharedReferenceCount() > 1)
	{
		Data = MakeShared<FVekaElementData>(*Data);
	}
}

void FVekaElement::AddModifierInternal(FInstancedStruct&& InModifier)
{
	if (!InModifier.IsValid() || !InModifier.GetScriptStruct()->IsChildOf(FVekaModifier::StaticStruct()))
	{
		UE_LOG(LogVekaUI, Warning, TEXT("FVekaElement::AddModifier: struct is not an FVekaModifier"));
		return;
	}
	MakeUnique();
	Data->Modifiers.Add(MoveTemp(InModifier));
}

template <typename T>
T* FVekaElement::MutablePrimitive(const TCHAR* Context)
{
	if (GetKind() != EVekaElementKind::Primitive)
	{
		UE_LOG(LogVekaUI, Warning, TEXT("%s: not valid on this element"), Context);
		return nullptr;
	}
	MakeUnique();
	T* Typed = Data->Primitive.GetMutablePtr<T>();
	if (!Typed)
	{
		UE_LOG(LogVekaUI, Warning, TEXT("%s: not valid on this element"), Context);
	}
	return Typed;
}

FVekaElement FVekaElement::AddModifier(FInstancedStruct&& InModifier) &&
{
	AddModifierInternal(MoveTemp(InModifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Id(FName InId) &&
{
	if (Data.IsValid())
	{
		MakeUnique();
		Data->Id = InId;
	}
	return MoveTemp(*this);
}

// --- Layout -------------------------------------------------------------------------------------

FVekaElement FVekaElement::Padding(float All) &&
{
	return MoveTemp(*this).Padding(FMargin(All));
}

FVekaElement FVekaElement::Padding(const FMargin& Insets) &&
{
	AddModifierInternal(FInstancedStruct::Make(FVekaPaddingModifier(Insets)));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Frame(double Width, double Height, FVekaAlignment Alignment) &&
{
	FVekaFrameModifier Frame;
	Frame.Width = Width;
	Frame.Height = Height;
	Frame.Alignment = Alignment;
	AddModifierInternal(FInstancedStruct::Make(Frame));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::FlexFrame(double MinWidth, double MaxWidth, double MinHeight, double MaxHeight, FVekaAlignment Alignment) &&
{
	FVekaFrameModifier Frame;
	Frame.MinWidth = MinWidth;
	Frame.MaxWidth = MaxWidth;
	Frame.MinHeight = MinHeight;
	Frame.MaxHeight = MaxHeight;
	Frame.Alignment = Alignment;
	AddModifierInternal(FInstancedStruct::Make(Frame));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::FillWidth(FVekaAlignment Alignment) &&
{
	return MoveTemp(*this).FlexFrame(-1.0, Veka::Infinity, -1.0, -1.0, Alignment);
}

FVekaElement FVekaElement::FillHeight(FVekaAlignment Alignment) &&
{
	return MoveTemp(*this).FlexFrame(-1.0, -1.0, -1.0, Veka::Infinity, Alignment);
}

FVekaElement FVekaElement::LayoutPriority(double Priority) &&
{
	FVekaLayoutPriorityModifier Modifier;
	Modifier.Priority = Priority;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Appearance ---------------------------------------------------------------------------------

FVekaElement FVekaElement::Background(const FVekaPaint& Paint, const FVekaCornerRadii& Radii) &&
{
	FVekaBackgroundModifier Modifier;
	Modifier.Paint = Paint;
	Modifier.CornerRadii = Radii;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Border(const FVekaPaint& Paint, float Width, const FVekaCornerRadii& Radii) &&
{
	FVekaBorderModifier Modifier;
	Modifier.Paint = Paint;
	Modifier.Width = Width;
	Modifier.CornerRadii = Radii;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Shadow(const FLinearColor& Color, float Radius, const FVector2D& Offset, const FVekaCornerRadii& Radii) &&
{
	FVekaShadowModifier Modifier;
	Modifier.Color = Color;
	Modifier.Radius = Radius;
	Modifier.Offset = Offset;
	Modifier.CornerRadii = Radii;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Opacity(float Value) &&
{
	FVekaOpacityModifier Modifier;
	Modifier.Opacity = FMath::Clamp(Value, 0.f, 1.f);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Clip() &&
{
	AddModifierInternal(FInstancedStruct::Make(FVekaClipModifier()));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Offset(const FVector2D& Delta) &&
{
	FVekaOffsetModifier Modifier;
	Modifier.Offset = Delta;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Scale(const FVector2D& InScale) &&
{
	FVekaScaleModifier Modifier;
	Modifier.Scale = InScale;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Rotation(float Degrees) &&
{
	FVekaRotationModifier Modifier;
	Modifier.Degrees = Degrees;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Shapes -------------------------------------------------------------------------------------

FVekaElement FVekaElement::Fill(const FVekaPaint& Paint) &&
{
	if (FVekaShapeView* ShapeView = MutablePrimitive<FVekaShapeView>(TEXT("Fill")))
	{
		ShapeView->bFill = true;
		ShapeView->bFillWithForeground = false;
		ShapeView->Fill = Paint;
	}
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Stroke(const FVekaStrokeStyle& Style) &&
{
	if (FVekaShapeView* ShapeView = MutablePrimitive<FVekaShapeView>(TEXT("Stroke")))
	{
		ShapeView->bStroke = true;
		ShapeView->Stroke = Style;
		// A shape with only a stroke is not filled (SwiftUI's .stroke replaces the default fill).
		if (ShapeView->bFillWithForeground)
		{
			ShapeView->bFill = false;
		}
	}
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Stroke(const FVekaPaint& Paint, float Width) &&
{
	return MoveTemp(*this).Stroke(FVekaStrokeStyle(Paint, Width));
}

FVekaElement FVekaElement::Trim(float From, float To) &&
{
	if (FVekaShapeView* ShapeView = MutablePrimitive<FVekaShapeView>(TEXT("Trim")))
	{
		ShapeView->Stroke.TrimFrom = FMath::Clamp(FMath::Min(From, To), 0.f, 1.f);
		ShapeView->Stroke.TrimTo = FMath::Clamp(FMath::Max(From, To), 0.f, 1.f);
	}
	return MoveTemp(*this);
}

// --- Environment --------------------------------------------------------------------------------

FVekaElement FVekaElement::Foreground(const FVekaPaint& Paint) &&
{
	FVekaForegroundModifier Modifier;
	Modifier.Paint = Paint;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Font(const FSlateFontInfo& InFont) &&
{
	FVekaFontModifier Modifier;
	Modifier.Font = InFont;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::ColorScheme(EVekaColorScheme Scheme) &&
{
	FVekaColorSchemeModifier Modifier;
	Modifier.Scheme = Scheme;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Interaction --------------------------------------------------------------------------------

FVekaElement FVekaElement::OnTap(TFunction<void()> Action) &&
{
	FVekaOnTapModifier Modifier;
	Modifier.Action = MoveTemp(Action);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::OnHover(TFunction<void(bool)> Action) &&
{
	FVekaOnHoverModifier Modifier;
	Modifier.Action = MoveTemp(Action);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::OnPress(TFunction<void(bool)> Action) &&
{
	FVekaOnPressModifier Modifier;
	Modifier.Action = MoveTemp(Action);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Colora ----------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::Theme(UVekaTheme* InTheme) &&
{
	FVekaThemeModifier Modifier;
	Modifier.Theme = InTheme;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::ColorAdjust(EVekaColorAdjust Adjust, float Amount) &&
{
	FVekaColorAdjustModifier Modifier;
	Modifier.Adjust = Adjust;
	Modifier.Amount = Amount;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::BlendMode(EVekaBlendMode Mode) &&
{
	FVekaBlendModeModifier Modifier;
	Modifier.Mode = Mode;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Anima -----------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::AnimationKey(const FVekaAnimationSpec& Spec, int64 ValueKey) &&
{
	FVekaAnimationModifier Modifier;
	Modifier.Spec = Spec;
	Modifier.ValueKey = ValueKey;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Transition(const FVekaTransition& InTransition) &&
{
	FVekaTransitionModifier Modifier;
	Modifier.Transition = InTransition;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Typography ------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::TypographyField(EVekaTypographyField Field, const FVekaTypography& Value) &&
{
	FVekaTypographyModifier Modifier;
	Modifier.Field = Field;
	Modifier.Value = Value;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Symbols ---------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::SymbolStyleField(EVekaSymbolStyleField Field, const FVekaSymbolStyle& Value) &&
{
	FVekaSymbolStyleModifier Modifier;
	Modifier.Field = Field;
	Modifier.Value = Value;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::SymbolRenderingMode(EVekaSymbolRenderingMode Mode) &&
{
	FVekaSymbolStyle V;
	V.RenderingMode = Mode;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::RenderingMode, V);
}

FVekaElement FVekaElement::SymbolVariant(const FVekaSymbolVariant& Variant) &&
{
	FVekaSymbolStyle V;
	V.Variant = Variant;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Variant, V);
}

FVekaElement FVekaElement::SymbolScale(EVekaSymbolScale Scale) &&
{
	FVekaSymbolStyle V;
	V.Scale = Scale;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Scale, V);
}

FVekaElement FVekaElement::SymbolPalette(const TArray<FVekaPaint>& Paints) &&
{
	FVekaSymbolStyle V;
	V.Palette = Paints;
	V.RenderingMode = EVekaSymbolRenderingMode::Palette;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Palette, V).SymbolStyleField(EVekaSymbolStyleField::RenderingMode, V);
}

FVekaElement FVekaElement::SymbolWeight(float Weight) &&
{
	FVekaSymbolStyle V;
	V.Weight = Weight;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Weight, V);
}

FVekaElement FVekaElement::SymbolEffect(const FVekaSymbolEffect& Effect, bool bIsActive) &&
{
	FVekaSymbolStyle V;
	V.Effect = Effect;
	V.bEffectActive = bIsActive;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Effect, V);
}

FVekaElement FVekaElement::SymbolEffectKey(const FVekaSymbolEffect& Effect, int64 Trigger) &&
{
	FVekaSymbolStyle V;
	V.Effect = Effect;
	V.EffectTrigger = Trigger;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Effect, V);
}

FVekaElement FVekaElement::SymbolReplace(EVekaSymbolReplace Mode, float Duration) &&
{
	FVekaSymbolStyle V;
	V.Replace = Mode;
	V.ReplaceDuration = Duration;
	return MoveTemp(*this).SymbolStyleField(EVekaSymbolStyleField::Replace, V);
}

FVekaElement FVekaElement::SymbolValue(float Value) &&
{
	if (FVekaSymbolView* Symbol = MutablePrimitive<FVekaSymbolView>(TEXT("SymbolValue")))
	{
		Symbol->VariableValue = FMath::Clamp(Value, -1.f, 1.f);
	}
	return MoveTemp(*this);
}

FVekaElement FVekaElement::SymbolBadge(int32 Count) &&
{
	if (FVekaSymbolView* Symbol = MutablePrimitive<FVekaSymbolView>(TEXT("SymbolBadge")))
	{
		Symbol->Badge = Count;
	}
	return MoveTemp(*this);
}
