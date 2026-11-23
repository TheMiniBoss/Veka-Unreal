// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Core/VekaElement.h"
#include "Core/VekaState.h"
#include "Controls/VekaControlStyles.h"

class UMediaPlayer;
class UMediaTexture;

// Controls take TVekaBinding (Core/VekaState.h); a TVekaState converts to it: Veka::Toggle(INVTEXT("Wi-Fi"), OnState).

/** One choice of a picker: a title, a symbol, or a color swatch (Palette style). */
struct VEKAUI_API FVekaPickerOption
{
	FText Title;
	FName Symbol;
	/** Palette swatch (alpha > 0). */
	FLinearColor Color = FLinearColor::Transparent;

	FVekaPickerOption() = default;
	FVekaPickerOption(FText InTitle, FName InSymbol = NAME_None) : Title(MoveTemp(InTitle)), Symbol(InSymbol) {}
	static FVekaPickerOption Swatch(const FLinearColor& InColor, FText InTitle = FText::GetEmpty());
};

/** Alpha 13 E_VekaMenuSectionType. */
enum class EVekaMenuItemKind : uint8
{
	Button,
	Divider,
	/** Section title. */
	Label,
	/** Inline choices with a checkmark on the selected one. */
	Picker,
	SubMenu
};

/** One entry of a Menu or ContextMenu. */
struct VEKAUI_API FVekaMenuItem
{
	EVekaMenuItemKind Kind = EVekaMenuItemKind::Button;
	FText Title;
	FName Symbol;
	/** Shown at the trailing edge ("Ctrl+S"). */
	FText Shortcut;
	TFunction<void()> Action;
	TArray<FVekaMenuItem> Children;
	TVekaBinding<int32> Selection;
	TArray<FVekaPickerOption> Options;
	bool bDestructive = false;
	bool bEnabled = true;

	static FVekaMenuItem Button(FText InTitle, TFunction<void()> InAction, FName InSymbol = NAME_None, FText InShortcut = FText::GetEmpty());
	static FVekaMenuItem Divider();
	static FVekaMenuItem Label(FText InTitle);
	static FVekaMenuItem Picker(FText InTitle, TVekaBinding<int32> InSelection, TArray<FVekaPickerOption> InOptions);
	static FVekaMenuItem SubMenu(FText InTitle, TArray<FVekaMenuItem> InChildren, FName InSymbol = NAME_None);

	FVekaMenuItem& Destructive() { bDestructive = true; return *this; }
	FVekaMenuItem& Enabled(bool bInEnabled) { bEnabled = bInEnabled; return *this; }
};

namespace Veka
{
	// --- Buttons -------------------------------------------------------------------------------------------------

	VEKAUI_API FVekaElement Button(FVekaElement Label, TFunction<void()> Action, EVekaButtonRole Role = EVekaButtonRole::Normal);
	VEKAUI_API FVekaElement Button(const FText& Title, TFunction<void()> Action, EVekaButtonRole Role = EVekaButtonRole::Normal);
	/** Title with a symbol (follows the label style). */
	VEKAUI_API FVekaElement Button(const FText& Title, FName Symbol, TFunction<void()> Action, EVekaButtonRole Role = EVekaButtonRole::Normal);
	/** Click, then press a key to bind it (Escape cancels). */
	VEKAUI_API FVekaElement KeyBindButton(TVekaBinding<FKey> Key);

	// --- Toggles, sliders, steppers ----------------------------------------------------------------------------------

	VEKAUI_API FVekaElement Toggle(FVekaElement Label, TVekaBinding<bool> IsOn);
	VEKAUI_API FVekaElement Toggle(const FText& Title, TVekaBinding<bool> IsOn);
	/** Step 0 = continuous. Arrow keys move by the step (or 5 % of the range). */
	VEKAUI_API FVekaElement Slider(TVekaBinding<float> Value, float Min = 0.f, float Max = 1.f, float Step = 0.f);
	/** Slider on a painted track (color channels). */
	VEKAUI_API FVekaElement ColorSlider(TVekaBinding<float> Value, const FVekaPaint& Track, float Min = 0.f, float Max = 1.f);
	VEKAUI_API FVekaElement Stepper(const FText& Title, TVekaBinding<float> Value, float Min, float Max, float Step = 1.f, int32 Decimals = 0);

	// --- Choices -------------------------------------------------------------------------------------------------

	/**
	 * Selection = index of the option (-1 = none). Style from `.PickerStyle` (menu, segmented, palette, radio).
	 * bAllowsEmpty: tapping the selected option clears it (Alpha 13 "Radio Flexible"; otherwise "Radio Strict").
	 */
	VEKAUI_API FVekaElement Picker(const FText& Title, TVekaBinding<int32> Selection, TArray<FVekaPickerOption> Options, bool bAllowsEmpty = false);
	/** A button that opens a menu. */
	VEKAUI_API FVekaElement Menu(FVekaElement Label, TArray<FVekaMenuItem> Items);
	VEKAUI_API FVekaElement Menu(const FText& Title, TArray<FVekaMenuItem> Items);
	/** The content of a menu (also used by ContextMenu and the menu picker). */
	VEKAUI_API FVekaElement MenuContent(TArray<FVekaMenuItem> Items);

	// --- Text ----------------------------------------------------------------------------------------------------

	VEKAUI_API FVekaElement TextField(const FText& Placeholder, TVekaBinding<FString> Text, TFunction<void()> OnSubmit = nullptr);
	VEKAUI_API FVekaElement SecureField(const FText& Placeholder, TVekaBinding<FString> Text, TFunction<void()> OnSubmit = nullptr);
	/** Number, percent (0.25 = 25 %) or currency field; the value is parsed and reformatted when editing ends. */
	VEKAUI_API FVekaElement NumberField(const FText& Placeholder, TVekaBinding<double> Value, EVekaTextFieldType Type = EVekaTextFieldType::Number, const FString& CurrencyCode = TEXT("EUR"));

	// --- Color ---------------------------------------------------------------------------------------------------

	/** Title and a swatch that opens the color panel. */
	VEKAUI_API FVekaElement ColorPicker(const FText& Title, TVekaBinding<FLinearColor> Color, bool bSupportsOpacity = true);
	/** Channel sliders of a mode (RGB, HSV, okLCH, hue, luminosity, saturation, grayscale, CMYK). */
	VEKAUI_API FVekaElement ColorPanel(TVekaBinding<FLinearColor> Color, EVekaColorPickerMode Mode = EVekaColorPickerMode::RGB, bool bSupportsOpacity = true,
		EVekaColorChannelRange Range = EVekaColorChannelRange::Byte);

	// --- Values --------------------------------------------------------------------------------------------------

	/** Style from `.GaugeStyle`. Empty labels are hidden. */
	VEKAUI_API FVekaElement Gauge(float Value, float Min = 0.f, float Max = 1.f, const FText& Label = FText::GetEmpty(), const FText& CurrentValueLabel = FText::GetEmpty(),
		const FText& MinLabel = FText::GetEmpty(), const FText& MaxLabel = FText::GetEmpty());
	/** Value 0..1, or < 0 for unknown progress. Style from `.ProgressViewStyle`. */
	VEKAUI_API FVekaElement ProgressView(float Value = -1.f, const FText& Label = FText::GetEmpty());

	// --- Misc ----------------------------------------------------------------------------------------------------

	VEKAUI_API FVekaElement Divider();
	/** Video or audio of a media player, with play / pause and a scrubber. */
	VEKAUI_API FVekaElement MediaView(UMediaPlayer* Player, UMediaTexture* Texture, bool bShowsControls = true);
	/** Empty state: a symbol, a title, a description and optional actions (Alpha 13 Veka_Unavailable). */
	VEKAUI_API FVekaElement ContentUnavailable(FName Symbol, const FText& Title, const FText& Description = FText::GetEmpty(), FVekaElement Actions = FVekaElement());
}

enum class EVekaPopupPlacement : uint8
{
	/** Under the control that was clicked. */
	Below,
	/** To the right of it (submenus). */
	Right,
	/** At the mouse (context menus). */
	AtCursor
};

namespace Veka::Popup
{
	/**
	 * Shows Content in a popup anchored to the control whose gesture is running (call it from OnTap / OnSecondaryTap).
	 * The popup inherits the control's environment. Returns false outside a gesture.
	 */
	VEKAUI_API bool Present(const FVekaElement& Content, EVekaPopupPlacement Placement = EVekaPopupPlacement::Below);
	/** Closes every popup. */
	VEKAUI_API void DismissAll();
}

namespace Veka::ColorMath
{
	/** Channels of a color in a mode (sRGB based, hue in degrees), and back. Alpha is kept. */
	VEKAUI_API TArray<float> Decompose(const FLinearColor& Color, EVekaColorPickerMode Mode);
	/** Base gives the channels a mode does not have (hue mode keeps saturation and value) and the alpha. */
	VEKAUI_API FLinearColor Compose(TConstArrayView<float> Channels, EVekaColorPickerMode Mode, const FLinearColor& Base);
	/** Name and range of each channel of a mode. */
	VEKAUI_API void GetChannels(EVekaColorPickerMode Mode, TArray<FText>& OutNames, TArray<FVector2f>& OutRanges);
	/** "#RRGGBB" (sRGB). */
	VEKAUI_API FString ToHex(const FLinearColor& Color);
}
