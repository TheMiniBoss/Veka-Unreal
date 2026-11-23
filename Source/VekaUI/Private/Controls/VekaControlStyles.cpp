// Copyright TheMiniBoss. All Rights Reserved.

#include "Controls/VekaControlStyles.h"
#include "Core/VekaEnvironment.h"
#include "Core/VekaPaint.h"
#include "VekaBuilders.h"
#include "Symbols/VekaSymbolView.h"
#include "Anima/VekaAnimationSpec.h"

namespace
{
	FVekaPaint Token(const TCHAR* Name, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary)
	{
		return FVekaPaint::FromToken(FName(Name), Hierarchy);
	}

	int32 InteractionKey(bool bA, bool bB, bool bC = false)
	{
		return (bA ? 1 : 0) | (bB ? 2 : 0) | (bC ? 4 : 0);
	}
}

// --- Button styles ---------------------------------------------------------------------------------------------

FVekaElement FVekaButtonStyle::MakeBody(const FVekaButtonStyleConfiguration& Configuration) const
{
	return Configuration.Label;
}

FVekaElement FVekaPlainButtonStyle::MakeBody(const FVekaButtonStyleConfiguration& C) const
{
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	FVekaElement Body = C.Label.Font(M.TextStyle).Opacity(C.bIsPressed ? 0.55f : 1.f).Animation(FVekaAnimationSpec::EaseOut(0.12), C.bIsPressed);
	return WithEnabled(WithFocusRing(MoveTemp(Body), C.bIsFocused, 4.f), C.bIsEnabled);
}

FVekaElement FVekaBorderlessButtonStyle::MakeBody(const FVekaButtonStyleConfiguration& C) const
{
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	const FVekaPaint Foreground = C.Role == EVekaButtonRole::Destructive ? Token(TEXT("Red")) : TintPaint();
	FVekaElement Body = C.Label.Font(M.TextStyle).Foreground(Foreground)
		.Padding(FMargin(4.f, 2.f))
		.Opacity(C.bIsPressed ? 0.45f : (C.bIsHovered ? 0.8f : 1.f))
		.Animation(FVekaAnimationSpec::EaseOut(0.12), InteractionKey(C.bIsPressed, C.bIsHovered));
	return WithEnabled(WithFocusRing(MoveTemp(Body), C.bIsFocused, M.Radius * 0.6f), C.bIsEnabled);
}

FVekaElement FVekaBorderedButtonStyle::MakeBody(const FVekaButtonStyleConfiguration& C) const
{
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	const FVekaPaint Foreground = C.Role == EVekaButtonRole::Destructive ? Token(TEXT("Red")) : TintPaint();
	const FVekaPaint Fill = Token(TEXT("ButtonBackground"), C.bIsPressed || C.bIsHovered ? EVekaHierarchy::Primary : EVekaHierarchy::Secondary);
	FVekaElement Body = C.Label.Font(M.TextStyle).Foreground(Foreground)
		.Padding(FMargin(M.HorizontalPadding, 0.f))
		.FlexFrame(-1.0, -1.0, M.Height, -1.0)
		.Background(Fill, M.Radius)
		.Brightness(C.bIsPressed ? 0.12f : 0.f)
		.Animation(FVekaAnimationSpec::EaseOut(0.12), InteractionKey(C.bIsPressed, C.bIsHovered));
	return WithEnabled(WithFocusRing(MoveTemp(Body), C.bIsFocused, M.Radius), C.bIsEnabled);
}

FVekaElement FVekaBorderedProminentButtonStyle::MakeBody(const FVekaButtonStyleConfiguration& C) const
{
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	const FVekaPaint Fill = C.Role == EVekaButtonRole::Destructive ? Token(TEXT("Red")) : TintPaint();
	FVekaElement Body = C.Label.Font(M.TextStyle).Foreground(FLinearColor::White)
		.Padding(FMargin(M.HorizontalPadding, 0.f))
		.FlexFrame(-1.0, -1.0, M.Height, -1.0)
		.Background(Fill, M.Radius)
		.Brightness(C.bIsPressed ? -0.12f : (C.bIsHovered ? 0.06f : 0.f))
		.Animation(FVekaAnimationSpec::EaseOut(0.12), InteractionKey(C.bIsPressed, C.bIsHovered));
	return WithEnabled(WithFocusRing(MoveTemp(Body), C.bIsFocused, M.Radius), C.bIsEnabled);
}

// --- Toggle styles ---------------------------------------------------------------------------------------------

FVekaElement FVekaToggleStyle::MakeBody(const FVekaToggleStyleConfiguration& Configuration) const
{
	return Configuration.Label;
}

FVekaElement FVekaSwitchToggleStyle::MakeBody(const FVekaToggleStyleConfiguration& C) const
{
	using namespace Veka;
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	const float TrackHeight = M.Height - 4.f;
	const float TrackWidth = FMath::RoundToFloat(TrackHeight * 1.75f);
	const float Knob = TrackHeight - 4.f;

	FVekaElement Switch = ZStack(FVekaAlignment::Leading())(
		Capsule().Fill(C.bIsOn ? Token(TEXT("ToggleAccent")) : Token(TEXT("ToggleBackground"))).Frame(TrackWidth, TrackHeight),
		Circle().Fill(FLinearColor::White).Frame(Knob, Knob)
			.Shadow(FLinearColor(0.f, 0.f, 0.f, 0.3f), 3.f, FVector2D(0.0, 1.0), Knob * 0.5f)
			.Padding(2.f)
			.Offset(FVector2D(C.bIsOn ? TrackWidth - TrackHeight : 0.f, 0.0))
	)
	.Brightness(C.bIsPressed ? 0.08f : 0.f)
	.Animation(FVekaAnimationSpec::Snappy(), InteractionKey(C.bIsOn, C.bIsPressed));
	Switch = WithFocusRing(MoveTemp(Switch), C.bIsFocused, TrackHeight * 0.5f);

	return WithEnabled(HStack(10.f)(C.Label.Font(M.TextStyle), Spacer(), MoveTemp(Switch)), C.bIsEnabled);
}

FVekaElement FVekaCheckboxToggleStyle::MakeBody(const FVekaToggleStyleConfiguration& C) const
{
	using namespace Veka;
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	const float Box = FMath::RoundToFloat(M.Height * 0.6f);
	const float Radius = FMath::Max(3.f, Box * 0.25f);

	FVekaElement Mark = ZStack()(
		RoundedRectangle(Radius).Fill(C.bIsOn ? Token(TEXT("ToggleAccent")) : FVekaPaint(FLinearColor::Transparent)).Frame(Box, Box)
			.Border(C.bIsOn ? FVekaPaint(FLinearColor::Transparent) : Token(TEXT("Gray")), 1.5f, Radius),
		Symbol(TEXT("checkmark")).SymbolWeight(0.85f).FontSize(Box * 0.42f).Foreground(FLinearColor::White)
			.Opacity(C.bIsOn ? 1.f : 0.f).Scale(C.bIsOn ? 1.f : 0.5f)
	)
	.Brightness(C.bIsPressed ? 0.1f : 0.f)
	.Animation(FVekaAnimationSpec::Snappy(), InteractionKey(C.bIsOn, C.bIsPressed));
	Mark = WithFocusRing(MoveTemp(Mark), C.bIsFocused, Radius);

	return WithEnabled(HStack(8.f)(MoveTemp(Mark), C.Label.Font(M.TextStyle)), C.bIsEnabled);
}

FVekaElement FVekaButtonToggleStyle::MakeBody(const FVekaToggleStyleConfiguration& C) const
{
	using namespace Veka::Controls;
	const FSizeMetrics M = GetSizeMetrics(C.ControlSize);
	FVekaElement Body = C.Label.Font(M.TextStyle).Foreground(C.bIsOn ? FVekaPaint(FLinearColor::White) : Token(TEXT("ToggleAccent")))
		.Padding(FMargin(M.HorizontalPadding, 0.f))
		.FlexFrame(-1.0, -1.0, M.Height, -1.0)
		.Background(C.bIsOn ? Token(TEXT("ToggleAccent")) : Token(TEXT("ToggleBackground"), C.bIsHovered ? EVekaHierarchy::Primary : EVekaHierarchy::Secondary), M.Radius)
		.Brightness(C.bIsPressed ? (C.bIsOn ? -0.1f : 0.1f) : 0.f)
		.Animation(FVekaAnimationSpec::EaseOut(0.15), InteractionKey(C.bIsOn, C.bIsPressed, C.bIsHovered));
	return WithEnabled(WithFocusRing(MoveTemp(Body), C.bIsFocused, M.Radius), C.bIsEnabled);
}

// --- Environment -----------------------------------------------------------------------------------------------

bool FVekaControlStyle::operator==(const FVekaControlStyle& Other) const
{
	return ButtonStyle == Other.ButtonStyle && ToggleStyle == Other.ToggleStyle && ControlSize == Other.ControlSize
		&& PickerStyle == Other.PickerStyle && RadioAxis == Other.RadioAxis && TextFieldStyle == Other.TextFieldStyle
		&& GaugeStyle == Other.GaugeStyle && ProgressStyle == Other.ProgressStyle && LabelStyle == Other.LabelStyle;
}

void FVekaControlStyleModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	FVekaControlStyle S = VekaEnv::GetControlStyle(Environment);
	const FVekaControlStyle& V = Value;
	switch (Field)
	{
	case EVekaControlStyleField::ButtonStyle: S.ButtonStyle = V.ButtonStyle; break;
	case EVekaControlStyleField::ToggleStyle: S.ToggleStyle = V.ToggleStyle; break;
	case EVekaControlStyleField::ControlSize: S.ControlSize = V.ControlSize; break;
	case EVekaControlStyleField::PickerStyle: S.PickerStyle = V.PickerStyle; S.RadioAxis = V.RadioAxis; break;
	case EVekaControlStyleField::TextFieldStyle: S.TextFieldStyle = V.TextFieldStyle; break;
	case EVekaControlStyleField::GaugeStyle: S.GaugeStyle = V.GaugeStyle; break;
	case EVekaControlStyleField::ProgressStyle: S.ProgressStyle = V.ProgressStyle; break;
	case EVekaControlStyleField::LabelStyle: S.LabelStyle = V.LabelStyle; break;
	}
	Environment.Set<FVekaControlStyle>(VekaEnv::ControlStyle(), S);
}

const FVekaControlStyle& VekaEnv::GetControlStyle(const FVekaEnvironment& Environment)
{
	static const FVekaControlStyle Default;
	const FVekaControlStyle* Found = Environment.Find<FVekaControlStyle>(ControlStyle());
	return Found ? *Found : Default;
}

namespace Veka::Controls
{
	FInstancedStruct MakeButtonStyle(EVekaButtonStyleKind Kind)
	{
		switch (Kind)
		{
		case EVekaButtonStyleKind::Plain: return FInstancedStruct::Make(FVekaPlainButtonStyle());
		case EVekaButtonStyleKind::Borderless: return FInstancedStruct::Make(FVekaBorderlessButtonStyle());
		case EVekaButtonStyleKind::BorderedProminent: return FInstancedStruct::Make(FVekaBorderedProminentButtonStyle());
		default: return FInstancedStruct::Make(FVekaBorderedButtonStyle());
		}
	}

	FInstancedStruct MakeToggleStyle(EVekaToggleStyleKind Kind)
	{
		switch (Kind)
		{
		case EVekaToggleStyleKind::Checkbox: return FInstancedStruct::Make(FVekaCheckboxToggleStyle());
		case EVekaToggleStyleKind::Button: return FInstancedStruct::Make(FVekaButtonToggleStyle());
		default: return FInstancedStruct::Make(FVekaSwitchToggleStyle());
		}
	}

	const FVekaButtonStyle& GetButtonStyle(const FVekaEnvironment& Environment)
	{
		static const FVekaBorderedButtonStyle Default;
		const FVekaButtonStyle* Style = VekaEnv::GetControlStyle(Environment).ButtonStyle.GetPtr<FVekaButtonStyle>();
		return Style ? *Style : Default;
	}

	const FVekaToggleStyle& GetToggleStyle(const FVekaEnvironment& Environment)
	{
		static const FVekaSwitchToggleStyle Default;
		const FVekaToggleStyle* Style = VekaEnv::GetControlStyle(Environment).ToggleStyle.GetPtr<FVekaToggleStyle>();
		return Style ? *Style : Default;
	}

	FSizeMetrics GetSizeMetrics(EVekaControlSize Size)
	{
		switch (Size)
		{
		case EVekaControlSize::Mini: return { 16.f, 6.f, 4.f, EVekaTextStyle::Caption2 };
		case EVekaControlSize::Small: return { 22.f, 9.f, 6.f, EVekaTextStyle::Footnote };
		case EVekaControlSize::Large: return { 38.f, 18.f, 11.f, EVekaTextStyle::Body };
		default: return { 28.f, 13.f, 8.f, EVekaTextStyle::Body };
		}
	}

	FVekaPaint TintPaint()
	{
		return FVekaPaint::FromToken(TEXT("Tint"));
	}

	FVekaElement WithFocusRing(FVekaElement Element, bool bFocused, float Radius)
	{
		return bFocused ? MoveTemp(Element).Border(TintPaint(), 2.5f, Radius) : MoveTemp(Element);
	}

	FVekaElement WithEnabled(FVekaElement Element, bool bEnabled)
	{
		return bEnabled ? MoveTemp(Element) : MoveTemp(Element).Opacity(0.4f);
	}
}
