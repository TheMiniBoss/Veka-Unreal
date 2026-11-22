// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaElement.h"
#include "Core/VekaView.h"
#include "Core/VekaPaint.h"
#include "Core/VekaEnvironment.h"
#include "Modifiers/VekaLayoutModifiers.h"
#include "Modifiers/VekaVisualModifiers.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "Modifiers/VekaEnvironmentModifiers.h"
#include "Modifiers/VekaInteractionModifiers.h"
#include "Modifiers/VekaColoraModifiers.h"
#include "Modifiers/VekaAnimaModifiers.h"
#include "Primitives/VekaShapeView.h"
#include "Symbols/VekaSymbolView.h"
#include "Controls/VekaControls.h"
#include "Morph/VekaMorph.h"
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

const void* FVekaElement::GetSourceDocument() const
{
	return Data.IsValid() ? Data->SourceDocument : nullptr;
}

int32 FVekaElement::GetSourceNode() const
{
	return Data.IsValid() ? Data->SourceNode : INDEX_NONE;
}

FVekaElement FVekaElement::Source(const void* Document, int32 Node) &&
{
	if (Data.IsValid())
	{
		MakeUnique();
		Data->SourceDocument = Document;
		Data->SourceNode = Node;
	}
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

FVekaElement FVekaElement::BackdropBlur(float Radius, const FVekaCornerRadii& Radii, const FVekaPaint& Tint, float TintAmount) &&
{
	FVekaBackdropModifier Modifier;
	Modifier.Radius = Radius;
	Modifier.CornerRadii = Radii;
	Modifier.Tint = Tint;
	Modifier.TintAmount = TintAmount;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::BackdropBlur(float Radius, const FVekaCornerRadii& Radii) &&
{
	FVekaBackdropModifier Modifier;
	return MoveTemp(*this).BackdropBlur(Radius, Radii, Modifier.Tint, Modifier.TintAmount);
}

FVekaElement FVekaElement::FrostedGlass(const FVekaCornerRadii& Radii) &&
{
	FVekaBackdropModifier Modifier = FVekaBackdropModifier::FrostedGlass();
	Modifier.CornerRadii = Radii;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::FrostedGlass(const FVekaCornerRadii& Radii, const FVekaPaint& Tint, float Blur, float Noise) &&
{
	FVekaBackdropModifier Modifier = FVekaBackdropModifier::FrostedGlass(Tint, Blur, Noise);
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

// --- Interaction (2e) ------------------------------------------------------------------------------------------

FVekaElement FVekaElement::OnSecondaryTap(TFunction<void(const FVector2D&)> Action) &&
{
	FVekaOnSecondaryTapModifier Modifier;
	Modifier.Action = MoveTemp(Action);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::OnDrag(TFunction<void(const FVekaDragValue&)> Action) &&
{
	FVekaOnDragModifier Modifier;
	Modifier.Action = MoveTemp(Action);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Focusable(TFunction<bool(const FKeyEvent&)> OnKey, TFunction<void(bool)> OnFocus) &&
{
	FVekaFocusableModifier Modifier;
	Modifier.OnKey = MoveTemp(OnKey);
	Modifier.OnFocus = MoveTemp(OnFocus);
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::Disabled(bool bDisabled) &&
{
	FVekaDisabledModifier Modifier;
	Modifier.bDisabled = bDisabled;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

// --- Controls --------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::ControlStyleField(EVekaControlStyleField Field, const FVekaControlStyle& Value) &&
{
	FVekaControlStyleModifier Modifier;
	Modifier.Field = Field;
	Modifier.Value = Value;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}

FVekaElement FVekaElement::ButtonStyle(EVekaButtonStyleKind Kind) &&
{
	return MoveTemp(*this).ButtonStyleStruct(Veka::Controls::MakeButtonStyle(Kind));
}

FVekaElement FVekaElement::ButtonStyleStruct(const FInstancedStruct& Style) &&
{
	FVekaControlStyle V;
	V.ButtonStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::ButtonStyle, V);
}

FVekaElement FVekaElement::ToggleStyle(EVekaToggleStyleKind Kind) &&
{
	return MoveTemp(*this).ToggleStyleStruct(Veka::Controls::MakeToggleStyle(Kind));
}

FVekaElement FVekaElement::ToggleStyleStruct(const FInstancedStruct& Style) &&
{
	FVekaControlStyle V;
	V.ToggleStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::ToggleStyle, V);
}

FVekaElement FVekaElement::ControlSize(EVekaControlSize Size) &&
{
	FVekaControlStyle V;
	V.ControlSize = Size;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::ControlSize, V);
}

FVekaElement FVekaElement::PickerStyle(EVekaPickerStyle Style, EVekaAxis RadioAxis) &&
{
	FVekaControlStyle V;
	V.PickerStyle = Style;
	V.RadioAxis = RadioAxis;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::PickerStyle, V);
}

FVekaElement FVekaElement::TextFieldStyle(EVekaTextFieldStyle Style) &&
{
	FVekaControlStyle V;
	V.TextFieldStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::TextFieldStyle, V);
}

FVekaElement FVekaElement::GaugeStyle(EVekaGaugeStyle Style) &&
{
	FVekaControlStyle V;
	V.GaugeStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::GaugeStyle, V);
}

FVekaElement FVekaElement::ProgressViewStyle(EVekaProgressStyle Style) &&
{
	FVekaControlStyle V;
	V.ProgressStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::ProgressStyle, V);
}

FVekaElement FVekaElement::LabelStyle(EVekaLabelStyle Style) &&
{
	FVekaControlStyle V;
	V.LabelStyle = Style;
	return MoveTemp(*this).ControlStyleField(EVekaControlStyleField::LabelStyle, V);
}

FVekaElement FVekaElement::ContextMenu(const TArray<FVekaMenuItem>& Items) &&
{
	return MoveTemp(*this).OnSecondaryTap([Items](const FVector2D&)
	{
		Veka::Popup::Present(Veka::MenuContent(Items), EVekaPopupPlacement::AtCursor);
	});
}

// --- Morph -----------------------------------------------------------------------------------------------------

FVekaElement FVekaElement::MatchedGeometry(FName InId, FName InNamespace) &&
{
	FVekaMatchedGeometryModifier Modifier;
	Modifier.Id = InId;
	Modifier.Namespace = InNamespace;
	AddModifierInternal(FInstancedStruct::Make(Modifier));
	return MoveTemp(*this);
}
