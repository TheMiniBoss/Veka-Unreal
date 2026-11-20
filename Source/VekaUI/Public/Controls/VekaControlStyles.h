// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Core/VekaElement.h"
#include "Core/VekaModifier.h"
#include "VekaControlStyles.generated.h"

class FVekaEnvironment;

/** Size of controls (SwiftUI controlSize). */
UENUM(BlueprintType)
enum class EVekaControlSize : uint8
{
	Mini,
	Small,
	Regular,
	Large
};

UENUM(BlueprintType)
enum class EVekaButtonRole : uint8
{
	Normal,
	/** Red: deletes something. */
	Destructive,
	Cancel
};

/** Alpha 13 E_VekaButtonStyle. */
UENUM(BlueprintType)
enum class EVekaButtonStyleKind : uint8
{
	Plain,
	Borderless,
	Bordered,
	BorderedProminent
};

/** Alpha 13 E_VekaToggleType. */
UENUM(BlueprintType)
enum class EVekaToggleStyleKind : uint8
{
	Switch,
	Button,
	Checkbox
};

/** Alpha 13 E_VekaPickerType. */
UENUM(BlueprintType)
enum class EVekaPickerStyle : uint8
{
	Menu,
	Segmented,
	Palette,
	Radio
};

/** Alpha 13 E_VekaTextFieldStyle. */
UENUM(BlueprintType)
enum class EVekaTextFieldStyle : uint8
{
	Plain,
	Rectangle,
	Rounded,
	Capsule
};

/** Alpha 13 E_VekaTextFieldType: how a number field shows its value. */
UENUM(BlueprintType)
enum class EVekaTextFieldType : uint8
{
	Text,
	Number,
	Percent,
	Currency
};

/** Alpha 13 E_VekaGaugeType. */
UENUM(BlueprintType)
enum class EVekaGaugeStyle : uint8
{
	Linear,
	/** Linear with a marker on a gradient track. */
	LinearAlt,
	Circular,
	/** Open ring with a marker and the min / max values. */
	CircularAlt
};

/** Alpha 13 E_VekaProgressType. */
UENUM(BlueprintType)
enum class EVekaProgressStyle : uint8
{
	Linear,
	Circular,
	/** Spinner (always indeterminate). */
	Lazy
};

/** Alpha 13 E_VekaLabelMode. */
UENUM(BlueprintType)
enum class EVekaLabelStyle : uint8
{
	TitleAndIcon,
	Title,
	Icon
};

/** Alpha 13 E_VekaColorPickerMode. */
UENUM(BlueprintType)
enum class EVekaColorPickerMode : uint8
{
	RGB,
	HSV,
	OkLCH,
	Hue,
	Luminosity,
	Saturation,
	Grayscale,
	CMYK
};

/** Alpha 13 E_VekaColorPickerRGBMode: how channel values are written. */
UENUM(BlueprintType)
enum class EVekaColorChannelRange : uint8
{
	/** 0.0 - 1.0 */
	Unit,
	/** 0 - 255 */
	Byte,
	/** 0 % - 100 % */
	Percent
};

/** What a button style receives (SwiftUI ButtonStyleConfiguration). */
struct FVekaButtonStyleConfiguration
{
	FVekaElement Label;
	EVekaButtonRole Role = EVekaButtonRole::Normal;
	EVekaControlSize ControlSize = EVekaControlSize::Regular;
	bool bIsPressed = false;
	bool bIsHovered = false;
	/** Focused from the keyboard (draw a focus ring). */
	bool bIsFocused = false;
	bool bIsEnabled = true;
};

/**
 * Look of a button. Derive a USTRUCT and override MakeBody to make your own, then `.ButtonStyle(FMyStyle())`.
 * The base draws the label alone (plain).
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaButtonStyle
{
	GENERATED_BODY()

	virtual ~FVekaButtonStyle() = default;
	virtual FVekaElement MakeBody(const FVekaButtonStyleConfiguration& Configuration) const;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaPlainButtonStyle : public FVekaButtonStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaButtonStyleConfiguration& Configuration) const override;
};

/** Tinted label, no background. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaBorderlessButtonStyle : public FVekaButtonStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaButtonStyleConfiguration& Configuration) const override;
};

/** Tinted label on a gray fill (default). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaBorderedButtonStyle : public FVekaButtonStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaButtonStyleConfiguration& Configuration) const override;
};

/** White label on the tint color. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaBorderedProminentButtonStyle : public FVekaButtonStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaButtonStyleConfiguration& Configuration) const override;
};

/** What a toggle style receives (SwiftUI ToggleStyleConfiguration). */
struct FVekaToggleStyleConfiguration
{
	FVekaElement Label;
	bool bIsOn = false;
	EVekaControlSize ControlSize = EVekaControlSize::Regular;
	bool bIsPressed = false;
	bool bIsHovered = false;
	bool bIsFocused = false;
	bool bIsEnabled = true;
};

/** Look of a toggle. The toggle flips on tap / Enter; the style only draws. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaToggleStyle
{
	GENERATED_BODY()

	virtual ~FVekaToggleStyle() = default;
	virtual FVekaElement MakeBody(const FVekaToggleStyleConfiguration& Configuration) const;
};

/** Label, then a switch at the trailing edge (default). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSwitchToggleStyle : public FVekaToggleStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaToggleStyleConfiguration& Configuration) const override;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaCheckboxToggleStyle : public FVekaToggleStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaToggleStyleConfiguration& Configuration) const override;
};

/** A button that stays prominent while on. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaButtonToggleStyle : public FVekaToggleStyle
{
	GENERATED_BODY()
	virtual FVekaElement MakeBody(const FVekaToggleStyleConfiguration& Configuration) const override;
};

/** Styles of the controls of a subtree. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaControlStyle
{
	GENERATED_BODY()

	/** FVekaButtonStyle-derived; empty = bordered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls", meta = (BaseStruct = "/Script/VekaUI.VekaButtonStyle"))
	FInstancedStruct ButtonStyle;

	/** FVekaToggleStyle-derived; empty = switch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls", meta = (BaseStruct = "/Script/VekaUI.VekaToggleStyle"))
	FInstancedStruct ToggleStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaControlSize ControlSize = EVekaControlSize::Regular;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaPickerStyle PickerStyle = EVekaPickerStyle::Menu;

	/** Radio pickers: options in a column or a row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaAxis RadioAxis = EVekaAxis::Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaTextFieldStyle TextFieldStyle = EVekaTextFieldStyle::Rounded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaGaugeStyle GaugeStyle = EVekaGaugeStyle::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaProgressStyle ProgressStyle = EVekaProgressStyle::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaLabelStyle LabelStyle = EVekaLabelStyle::TitleAndIcon;

	bool operator==(const FVekaControlStyle& Other) const;
};

UENUM(BlueprintType)
enum class EVekaControlStyleField : uint8
{
	ButtonStyle,
	ToggleStyle,
	ControlSize,
	/** Picker style and radio axis. */
	PickerStyle,
	TextFieldStyle,
	GaugeStyle,
	ProgressStyle,
	LabelStyle
};

/** Sets one field of the control style for the subtree. */
USTRUCT(BlueprintType, meta = (VekaName = "controlStyle", VekaCategory = "Controls"))
struct VEKAUI_API FVekaControlStyleModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaControlStyleField Field = EVekaControlStyleField::ButtonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FVekaControlStyle Value;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

namespace VekaEnv
{
	inline FName ControlStyle() { static const FName Name(TEXT("Veka.ControlStyle")); return Name; }

	VEKAUI_API const FVekaControlStyle& GetControlStyle(const FVekaEnvironment& Environment);
}

namespace Veka::Controls
{
	VEKAUI_API FInstancedStruct MakeButtonStyle(EVekaButtonStyleKind Kind);
	VEKAUI_API FInstancedStruct MakeToggleStyle(EVekaToggleStyleKind Kind);
	VEKAUI_API const FVekaButtonStyle& GetButtonStyle(const FVekaEnvironment& Environment);
	VEKAUI_API const FVekaToggleStyle& GetToggleStyle(const FVekaEnvironment& Environment);

	/** Metrics of a control size. */
	struct FSizeMetrics
	{
		float Height = 28.f;
		float HorizontalPadding = 12.f;
		float Radius = 8.f;
		EVekaTextStyle TextStyle = EVekaTextStyle::Body;
	};
	VEKAUI_API FSizeMetrics GetSizeMetrics(EVekaControlSize Size);

	/** Accent color of the subtree ("Tint" token). */
	VEKAUI_API FVekaPaint TintPaint();
	/** Keyboard focus ring around a control. */
	VEKAUI_API FVekaElement WithFocusRing(FVekaElement Element, bool bFocused, float Radius);
	/** Dimmed when disabled. */
	VEKAUI_API FVekaElement WithEnabled(FVekaElement Element, bool bEnabled);
}
