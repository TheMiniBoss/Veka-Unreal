// Copyright TheMiniBoss. All Rights Reserved.

#include "Controls/VekaControls.h"
#include "Controls/VekaControlPrimitives.h"
#include "Core/VekaView.h"
#include "Core/VekaEnvironment.h"
#include "Core/VekaPaint.h"
#include "Colora/VekaColor.h"
#include "Colora/VekaTheme.h"
#include "Modifiers/VekaInteractionModifiers.h"
#include "Symbols/VekaSymbolView.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Anima/VekaAnimationSpec.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IMenu.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"

// --- Data helpers ----------------------------------------------------------------------------------------------

FVekaPickerOption FVekaPickerOption::Swatch(const FLinearColor& InColor, FText InTitle)
{
	FVekaPickerOption Option(MoveTemp(InTitle));
	Option.Color = InColor;
	return Option;
}

FVekaMenuItem FVekaMenuItem::Button(FText InTitle, TFunction<void()> InAction, FName InSymbol, FText InShortcut)
{
	FVekaMenuItem Item;
	Item.Kind = EVekaMenuItemKind::Button;
	Item.Title = MoveTemp(InTitle);
	Item.Action = MoveTemp(InAction);
	Item.Symbol = InSymbol;
	Item.Shortcut = MoveTemp(InShortcut);
	return Item;
}

FVekaMenuItem FVekaMenuItem::Divider()
{
	FVekaMenuItem Item;
	Item.Kind = EVekaMenuItemKind::Divider;
	return Item;
}

FVekaMenuItem FVekaMenuItem::Label(FText InTitle)
{
	FVekaMenuItem Item;
	Item.Kind = EVekaMenuItemKind::Label;
	Item.Title = MoveTemp(InTitle);
	return Item;
}

FVekaMenuItem FVekaMenuItem::Picker(FText InTitle, TVekaBinding<int32> InSelection, TArray<FVekaPickerOption> InOptions)
{
	FVekaMenuItem Item;
	Item.Kind = EVekaMenuItemKind::Picker;
	Item.Title = MoveTemp(InTitle);
	Item.Selection = MoveTemp(InSelection);
	Item.Options = MoveTemp(InOptions);
	return Item;
}

FVekaMenuItem FVekaMenuItem::SubMenu(FText InTitle, TArray<FVekaMenuItem> InChildren, FName InSymbol)
{
	FVekaMenuItem Item;
	Item.Kind = EVekaMenuItemKind::SubMenu;
	Item.Title = MoveTemp(InTitle);
	Item.Children = MoveTemp(InChildren);
	Item.Symbol = InSymbol;
	return Item;
}

namespace
{
	using namespace Veka;
	using namespace Veka::Controls;

	FVekaPaint TokenPaint(const TCHAR* Name, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary)
	{
		return FVekaPaint::FromToken(FName(Name), Hierarchy);
	}

	EVekaControlSize SizeOf(const FVekaEnvironment& Environment)
	{
		return VekaEnv::GetControlStyle(Environment).ControlSize;
	}

	/** Background of popups (menus, color panel). */
	FLinearColor PopupColor(const FVekaEnvironment& Environment)
	{
		return VekaEnv::GetColorScheme(Environment) == EVekaColorScheme::Dark ? Hex(TEXT("#2B2B30FF")) : Hex(TEXT("#F7F7F8FF"));
	}

	FVekaElement PopupCard(FVekaElement Content, const FVekaEnvironment& Environment, float Radius = 10.f)
	{
		return MoveTemp(Content)
			.Background(PopupColor(Environment), Radius)
			.Border(TokenPaint(TEXT("Separator")), 1.f, Radius)
			.Shadow(FLinearColor(0.f, 0.f, 0.f, 0.35f), 14.f, FVector2D(0.0, 6.0), Radius);
	}

	bool ArrowStep(const FKeyEvent& Event, int32& OutDirection)
	{
		const FKey Key = Event.GetKey();
		if (Key == EKeys::Right || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Up)
		{
			OutDirection = 1;
			return true;
		}
		if (Key == EKeys::Left || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Down)
		{
			OutDirection = -1;
			return true;
		}
		return false;
	}

	FVekaElement OptionContent(const FVekaPickerOption& Option)
	{
		if (!Option.Symbol.IsNone() && !Option.Title.IsEmpty())
		{
			return Veka::Label(Option.Title, Option.Symbol);
		}
		return Option.Symbol.IsNone() ? Text(Option.Title) : Symbol(Option.Symbol);
	}

	// --- Button / toggle -------------------------------------------------------------------------------------------

	class FVekaButtonView : public FVekaView
	{
		VEKA_VIEW(FVekaButtonView)

	public:
		FVekaElement Label;
		TFunction<void()> Action;
		EVekaButtonRole Role = EVekaButtonRole::Normal;

		FVekaButtonView(FVekaElement InLabel, TFunction<void()> InAction, EVekaButtonRole InRole) : Label(MoveTemp(InLabel)), Action(MoveTemp(InAction)), Role(InRole) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Hovered = Ctx.State<bool>(TEXT("Hovered"), false);
			const TVekaState<bool> Pressed = Ctx.State<bool>(TEXT("Pressed"), false);
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();

			FVekaButtonStyleConfiguration C;
			C.Label = Label;
			C.Role = Role;
			C.ControlSize = SizeOf(Env);
			C.bIsEnabled = VekaEnv::IsEnabled(Env);
			C.bIsPressed = Pressed.Get() && C.bIsEnabled;
			C.bIsHovered = Hovered.Get() && C.bIsEnabled;
			C.bIsFocused = Focused.Get() && C.bIsEnabled;

			TFunction<void()> Callback = Action;
			return GetButtonStyle(Env).MakeBody(C)
				.OnHover([Hovered](bool bIn) { Hovered.Set(bIn); })
				.OnPress([Pressed](bool bIn) { Pressed.Set(bIn); })
				.OnTap([Callback]() { if (Callback) { Callback(); } })
				.Focusable(nullptr, [Focused](bool bIn) { Focused.Set(bIn); });
		}
	};

	class FVekaToggleView : public FVekaView
	{
		VEKA_VIEW(FVekaToggleView)

	public:
		FVekaElement Label;
		TVekaBinding<bool> IsOn;

		FVekaToggleView(FVekaElement InLabel, TVekaBinding<bool> InIsOn) : Label(MoveTemp(InLabel)), IsOn(MoveTemp(InIsOn)) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Hovered = Ctx.State<bool>(TEXT("Hovered"), false);
			const TVekaState<bool> Pressed = Ctx.State<bool>(TEXT("Pressed"), false);
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();

			FVekaToggleStyleConfiguration C;
			C.Label = Label;
			C.bIsOn = IsOn.Get();
			C.ControlSize = SizeOf(Env);
			C.bIsEnabled = VekaEnv::IsEnabled(Env);
			C.bIsPressed = Pressed.Get() && C.bIsEnabled;
			C.bIsHovered = Hovered.Get() && C.bIsEnabled;
			C.bIsFocused = Focused.Get() && C.bIsEnabled;

			const TVekaBinding<bool> Binding = IsOn;
			return GetToggleStyle(Env).MakeBody(C)
				.OnHover([Hovered](bool bIn) { Hovered.Set(bIn); })
				.OnPress([Pressed](bool bIn) { Pressed.Set(bIn); })
				.OnTap([Binding]() { Binding.Set(!Binding.Get()); })
				.Focusable(nullptr, [Focused](bool bIn) { Focused.Set(bIn); });
		}
	};

	class FVekaKeyBindView : public FVekaView
	{
		VEKA_VIEW(FVekaKeyBindView)

	public:
		TVekaBinding<FKey> Key;

		explicit FVekaKeyBindView(TVekaBinding<FKey> InKey) : Key(MoveTemp(InKey)) {}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Listening = Ctx.State<bool>(TEXT("Listening"), false);
			const TVekaState<bool> Hovered = Ctx.State<bool>(TEXT("Hovered"), false);
			const TVekaState<bool> Pressed = Ctx.State<bool>(TEXT("Pressed"), false);
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();
			const FKey Current = Key.Get();

			FVekaButtonStyleConfiguration C;
			C.Label = Listening.Get()
				? Veka::Label(INVTEXT("Press a key..."), TEXT("keyboard")).Opacity(0.8f)
				: Veka::Label(Current.IsValid() ? Current.GetDisplayName() : INVTEXT("None"), TEXT("keyboard"));
			C.ControlSize = SizeOf(Env);
			C.bIsEnabled = VekaEnv::IsEnabled(Env);
			C.bIsPressed = Pressed.Get() && C.bIsEnabled;
			C.bIsHovered = Hovered.Get() && C.bIsEnabled;
			C.bIsFocused = (Focused.Get() || Listening.Get()) && C.bIsEnabled;

			const TVekaBinding<FKey> Binding = Key;
			FVekaElement Body = Listening.Get() ? FVekaBorderedProminentButtonStyle().MakeBody(C) : GetButtonStyle(Env).MakeBody(C);
			return MoveTemp(Body)
				.OnHover([Hovered](bool bIn) { Hovered.Set(bIn); })
				.OnPress([Pressed](bool bIn) { Pressed.Set(bIn); })
				.OnTap([Listening]() { Listening.Set(true); })
				.Focusable([Listening, Binding](const FKeyEvent& Event)
				{
					if (!Listening.Get())
					{
						return false;
					}
					if (Event.GetKey() != EKeys::Escape)
					{
						Binding.Set(Event.GetKey());
					}
					Listening.Set(false);
					return true;
				}, [Focused, Listening](bool bIn)
				{
					Focused.Set(bIn);
				});
		}
	};

	// --- Slider / stepper ----------------------------------------------------------------------------------------

	class FVekaSliderView : public FVekaView
	{
		VEKA_VIEW(FVekaSliderView)

	public:
		TVekaBinding<float> Value;
		float Min = 0.f;
		float Max = 1.f;
		float Step = 0.f;
		bool bColorTrack = false;
		FVekaPaint Track;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();
			const bool bEnabled = VekaEnv::IsEnabled(Env);
			const float Range = Max - Min;
			const float Current = Value.Get();

			FVekaTrack T;
			T.Fraction = Range > 0.f ? (Current - Min) / Range : 0.f;
			T.Thickness = bColorTrack ? 10.f : 4.f;
			T.TrackPaint = bColorTrack ? Track : TokenPaint(TEXT("Fill"));
			T.FillPaint = TintPaint();
			T.bShowsFill = !bColorTrack;
			T.KnobSize = bColorTrack ? 18.f : 20.f;
			T.KnobBorder = 0.f;
			const float Knob = T.KnobSize;

			const TVekaBinding<float> Binding = Value;
			const float InMin = Min;
			const float InMax = Max;
			const float InStep = Step;
			auto SetValue = [Binding, InMin, InMax, InStep](float NewValue)
			{
				if (InStep > 0.f)
				{
					NewValue = InMin + FMath::RoundToFloat((NewValue - InMin) / InStep) * InStep;
				}
				Binding.Set(FMath::Clamp(NewValue, InMin, InMax));
			};
			const float Increment = Step > 0.f ? Step : Range * 0.05f;

			FVekaElement Body = FVekaElement::Make(T)
				.OnDrag([SetValue, Knob, InMin, Range](const FVekaDragValue& Drag)
				{
					SetValue(InMin + FVekaTrack::FractionAt(Drag.Location.X, Drag.Size.X, Knob) * Range);
				})
				.Focusable([SetValue, Binding, Increment, InMin, InMax](const FKeyEvent& Event)
				{
					int32 Direction = 0;
					if (ArrowStep(Event, Direction))
					{
						SetValue(Binding.Get() + Direction * Increment);
						return true;
					}
					if (Event.GetKey() == EKeys::Home || Event.GetKey() == EKeys::End)
					{
						SetValue(Event.GetKey() == EKeys::Home ? InMin : InMax);
						return true;
					}
					return false;
				}, [Focused](bool bIn) { Focused.Set(bIn); });
			return WithEnabled(WithFocusRing(MoveTemp(Body), Focused.Get() && bEnabled, Knob * 0.5f), bEnabled);
		}
	};

	class FVekaStepperView : public FVekaView
	{
		VEKA_VIEW(FVekaStepperView)

	public:
		FText Title;
		TVekaBinding<float> Value;
		float Min = 0.f;
		float Max = 10.f;
		float Step = 1.f;
		int32 Decimals = 0;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();
			const FSizeMetrics M = GetSizeMetrics(SizeOf(Env));
			const bool bEnabled = VekaEnv::IsEnabled(Env);
			const float Current = Value.Get();

			FNumberFormattingOptions Format;
			Format.SetMinimumFractionalDigits(Decimals).SetMaximumFractionalDigits(Decimals);
			const TVekaBinding<float> Binding = Value;
			const float InMin = Min;
			const float InMax = Max;
			const float InStep = Step;
			auto Change = [Binding, InMin, InMax, InStep](int32 Direction)
			{
				Binding.Set(FMath::Clamp(Binding.Get() + Direction * InStep, InMin, InMax));
			};

			auto StepButton = [&](const TCHAR* SymbolName, int32 Direction, bool bDisabled)
			{
				return Button(Symbol(SymbolName).SymbolWeight(0.6f).Foreground(TintPaint()).Frame(M.Height + 4.f, M.Height - 4.f), [Change, Direction]() { Change(Direction); })
					.ButtonStyle(EVekaButtonStyleKind::Plain)
					.Disabled(bDisabled);
			};

			FVekaElement Control = HStack(0.f)(
				StepButton(TEXT("minus"), -1, Current <= Min),
				Divider().Frame(1.f, M.Height * 0.5f),
				StepButton(TEXT("plus"), 1, Current >= Max)
			)
			.Background(TokenPaint(TEXT("Fill"), EVekaHierarchy::Secondary), M.Radius);
			Control = WithFocusRing(MoveTemp(Control), Focused.Get() && bEnabled, M.Radius);

			return WithEnabled(HStack(10.f)(
				Text(Title).Font(M.TextStyle),
				Spacer(),
				Text(FText::AsNumber(Current, &Format)).Font(M.TextStyle).MonospacedDigit(),
				MoveTemp(Control)
			), bEnabled)
			.Focusable([Change](const FKeyEvent& Event)
			{
				int32 Direction = 0;
				if (ArrowStep(Event, Direction))
				{
					Change(Direction);
					return true;
				}
				return false;
			}, [Focused](bool bIn) { Focused.Set(bIn); });
		}
	};

	// --- Picker --------------------------------------------------------------------------------------------------

	class FVekaPickerView : public FVekaView
	{
		VEKA_VIEW(FVekaPickerView)

	public:
		FText Title;
		TVekaBinding<int32> Selection;
		TArray<FVekaPickerOption> Options;
		bool bAllowsEmpty = false;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Focused = Ctx.State<bool>(TEXT("Focused"), false);
			const FVekaEnvironment& Env = Ctx.GetEnvironment();
			const FVekaControlStyle& Style = VekaEnv::GetControlStyle(Env);
			const FSizeMetrics M = GetSizeMetrics(Style.ControlSize);
			const bool bEnabled = VekaEnv::IsEnabled(Env);
			const int32 Selected = Selection.Get();
			const int32 Count = Options.Num();

			const TVekaBinding<int32> Binding = Selection;
			const bool bEmpty = bAllowsEmpty;
			auto Select = [Binding, bEmpty](int32 Index)
			{
				Binding.Set(bEmpty && Binding.Get() == Index ? INDEX_NONE : Index);
			};
			auto OnKey = [Binding, Count](const FKeyEvent& Event)
			{
				int32 Direction = 0;
				if (Count > 0 && ArrowStep(Event, Direction))
				{
					// Up / left go to the previous option in a list.
					Binding.Set(FMath::Clamp(Binding.Get() + Direction, 0, Count - 1));
					return true;
				}
				return false;
			};

			FVekaElement Control;
			switch (Style.PickerStyle)
			{
			case EVekaPickerStyle::Segmented:
			{
				const FLinearColor SelectedFill = VekaEnv::GetColorScheme(Env) == EVekaColorScheme::Dark ? Hex(TEXT("#636366FF")) : FLinearColor::White;
				TArray<FVekaElement> Segments;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const bool bOn = Index == Selected;
					Segments.Add(OptionContent(Options[Index]).Font(M.TextStyle)
						.Padding(FMargin(10.f, 0.f))
						.FlexFrame(-1.0, Veka::Infinity, M.Height - 4.f, M.Height - 4.f)
						.Background(bOn ? FVekaPaint(SelectedFill) : FVekaPaint(FLinearColor::Transparent), M.Radius - 2.f)
						.Shadow(FLinearColor(0.f, 0.f, 0.f, bOn ? 0.2f : 0.f), 3.f, FVector2D(0.0, 1.0), M.Radius - 2.f)
						.OnTap([Select, Index]() { Select(Index); }));
				}
				Control = HStack(2.f).Children(MoveTemp(Segments))
					.Padding(2.f)
					.Background(TokenPaint(TEXT("Fill"), EVekaHierarchy::Secondary), M.Radius)
					.Animation(FVekaAnimationSpec::Snappy(), Selected);
				break;
			}
			case EVekaPickerStyle::Radio:
			{
				TArray<FVekaElement> Rows;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const bool bOn = Index == Selected;
					Rows.Add(HStack(7.f)(
						ZStack()(
							Circle().Fill(bOn ? TintPaint() : FVekaPaint(FLinearColor::Transparent)).Frame(16.f, 16.f)
								.Border(bOn ? FVekaPaint(FLinearColor::Transparent) : TokenPaint(TEXT("Gray")), 1.5f, 8.f),
							Circle().Fill(FLinearColor::White).Frame(6.f, 6.f).Opacity(bOn ? 1.f : 0.f)
						),
						OptionContent(Options[Index]).Font(M.TextStyle)
					).OnTap([Select, Index]() { Select(Index); }));
				}
				Control = Stack(Style.RadioAxis, Style.RadioAxis == EVekaAxis::Vertical ? 8.f : 16.f, FVekaAlignment::Leading()).Children(MoveTemp(Rows))
					.Animation(FVekaAnimationSpec::Snappy(), Selected);
				break;
			}
			case EVekaPickerStyle::Palette:
			{
				TArray<FVekaElement> Swatches;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const bool bOn = Index == Selected;
					const FVekaPickerOption& Option = Options[Index];
					FVekaElement Swatch = Option.Color.A > 0.f
						? Circle().Fill(Option.Color).Frame(22.f, 22.f)
						: OptionContent(Option).Font(M.TextStyle).Frame(22.f, 22.f);
					Swatches.Add(MoveTemp(Swatch).Padding(3.f)
						.Border(bOn ? TintPaint() : FVekaPaint(FLinearColor::Transparent), 2.f, 14.f)
						.OnTap([Select, Index]() { Select(Index); }));
				}
				Control = HStack(6.f).Children(MoveTemp(Swatches)).Animation(FVekaAnimationSpec::Snappy(), Selected);
				break;
			}
			default:
			{
				// Menu: the current choice and a popup of all of them.
				const FText Current = Options.IsValidIndex(Selected) ? Options[Selected].Title : INVTEXT("-");
				TArray<FVekaMenuItem> Items = { FVekaMenuItem::Picker(FText::GetEmpty(), Selection, Options) };
				Control = Button(HStack(6.f)(Text(Current), Symbol(TEXT("chevron.up.down")).SymbolWeight(0.7f).SymbolScale(EVekaSymbolScale::Small)),
					[Items]() { Veka::Popup::Present(MenuContent(Items), EVekaPopupPlacement::Below); });
				break;
			}
			}

			Control = WithFocusRing(MoveTemp(Control), Focused.Get() && bEnabled, M.Radius);
			FVekaElement Body;
			if (Title.IsEmpty())
			{
				Body = MoveTemp(Control);
			}
			else if (Style.PickerStyle == EVekaPickerStyle::Radio && Style.RadioAxis == EVekaAxis::Vertical)
			{
				Body = VStack(8.f, EVekaHAlign::Leading)(Text(Title).Font(M.TextStyle).FontWeight(EVekaFontWeight::SemiBold), MoveTemp(Control));
			}
			else if (Style.PickerStyle == EVekaPickerStyle::Segmented)
			{
				Body = HStack(12.f)(Text(Title).Font(M.TextStyle), MoveTemp(Control));
			}
			else
			{
				Body = HStack(12.f)(Text(Title).Font(M.TextStyle), Spacer(), MoveTemp(Control));
			}
			if (Style.PickerStyle == EVekaPickerStyle::Menu)
			{
				return WithEnabled(MoveTemp(Body), bEnabled);
			}
			return WithEnabled(MoveTemp(Body), bEnabled).Focusable(OnKey, [Focused](bool bIn) { Focused.Set(bIn); });
		}
	};

	// --- Menus ---------------------------------------------------------------------------------------------------

	class FVekaMenuRowView : public FVekaView
	{
		VEKA_VIEW(FVekaMenuRowView)

	public:
		FText Title;
		FName Symbol;
		FText Trailing;
		bool bSubMenu = false;
		bool bDestructive = false;
		bool bEnabled = true;
		TFunction<void()> Action;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<bool> Hovered = Ctx.State<bool>(TEXT("Hovered"), false);
			const bool bHighlight = Hovered.Get() && bEnabled;

			FVekaElement Icon = Symbol.IsNone() ? Rectangle().Fill(FLinearColor::Transparent).Frame(16.f, 16.f) : Veka::Symbol(Symbol).Frame(16.f, 16.f);
			FVekaElement Accessory = bSubMenu
				? Veka::Symbol(TEXT("chevron.right")).SymbolScale(EVekaSymbolScale::Small)
				: Text(Trailing).Opacity(0.6f);
			FVekaElement Row = HStack(8.f)(MoveTemp(Icon), Text(Title), Spacer(16.f), MoveTemp(Accessory))
				.Padding(FMargin(8.f, 4.f))
				.FillWidth(FVekaAlignment::Leading())
				.Background(bHighlight ? TintPaint() : FVekaPaint(FLinearColor::Transparent), 6.f);
			if (bHighlight)
			{
				Row = MoveTemp(Row).Foreground(FLinearColor::White);
			}
			else if (bDestructive)
			{
				Row = MoveTemp(Row).Foreground(TokenPaint(TEXT("Red")));
			}
			const TFunction<void()> Callback = Action;
			const bool bCanRun = bEnabled;
			return MoveTemp(Row)
				.Opacity(bEnabled ? 1.f : 0.4f)
				.OnHover([Hovered](bool bIn) { Hovered.Set(bIn); })
				.OnTap([Callback, bCanRun]() { if (bCanRun && Callback) { Callback(); } });
		}
	};

	FVekaElement MenuRow(const FText& Title, FName SymbolName, const FText& Trailing, bool bSubMenu, bool bDestructive, bool bEnabled, TFunction<void()> Action)
	{
		TSharedRef<FVekaMenuRowView> Row = MakeShared<FVekaMenuRowView>();
		Row->Title = Title;
		Row->Symbol = SymbolName;
		Row->Trailing = Trailing;
		Row->bSubMenu = bSubMenu;
		Row->bDestructive = bDestructive;
		Row->bEnabled = bEnabled;
		Row->Action = MoveTemp(Action);
		return FVekaElement::MakeView(Row);
	}

	class FVekaMenuContentView : public FVekaView
	{
		VEKA_VIEW(FVekaMenuContentView)

	public:
		TArray<FVekaMenuItem> Items;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			TArray<FVekaElement> Rows;
			for (const FVekaMenuItem& Item : Items)
			{
				switch (Item.Kind)
				{
				case EVekaMenuItemKind::Divider:
					Rows.Add(Divider().Padding(FMargin(8.f, 4.f)));
					break;
				case EVekaMenuItemKind::Label:
					Rows.Add(Text(Item.Title).Font(EVekaTextStyle::Footnote).FontWeight(EVekaFontWeight::SemiBold).Opacity(0.55f).Padding(FMargin(8.f, 5.f, 8.f, 2.f)));
					break;
				case EVekaMenuItemKind::Picker:
				{
					if (!Item.Title.IsEmpty())
					{
						Rows.Add(Text(Item.Title).Font(EVekaTextStyle::Footnote).FontWeight(EVekaFontWeight::SemiBold).Opacity(0.55f).Padding(FMargin(8.f, 5.f, 8.f, 2.f)));
					}
					const int32 Selected = Item.Selection.Get();
					for (int32 Index = 0; Index < Item.Options.Num(); ++Index)
					{
						const TVekaBinding<int32> Binding = Item.Selection;
						Rows.Add(MenuRow(Item.Options[Index].Title, Index == Selected ? FName(TEXT("checkmark")) : NAME_None, FText::GetEmpty(), false, false, Item.bEnabled,
							[Binding, Index]() { Binding.Set(Index); Veka::Popup::DismissAll(); }));
					}
					break;
				}
				case EVekaMenuItemKind::SubMenu:
				{
					const TArray<FVekaMenuItem> Children = Item.Children;
					Rows.Add(MenuRow(Item.Title, Item.Symbol, FText::GetEmpty(), true, false, Item.bEnabled,
						[Children]() { Veka::Popup::Present(MenuContent(Children), EVekaPopupPlacement::Right); }));
					break;
				}
				default:
				{
					const TFunction<void()> Action = Item.Action;
					Rows.Add(MenuRow(Item.Title, Item.Symbol, Item.Shortcut, false, Item.bDestructive, Item.bEnabled,
						[Action]() { if (Action) { Action(); } Veka::Popup::DismissAll(); }));
					break;
				}
				}
			}
			return PopupCard(VStack(0.f, EVekaHAlign::Leading).Children(MoveTemp(Rows)).Padding(5.f).FlexFrame(200.0, -1.0, -1.0, -1.0, FVekaAlignment::TopLeading()),
				Ctx.GetEnvironment());
		}
	};

	// --- Color -----------------------------------------------------------------------------------------------------

	FText ChannelValueText(float Value, const FVector2f& Range, EVekaColorPickerMode Mode, EVekaColorChannelRange Display)
	{
		if (Range.Y > 2.f)
		{
			return FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt32(Value)));
		}
		if (Range.Y < 0.99f)
		{
			return FText::FromString(FString::Printf(TEXT("%.2f"), Value));
		}
		// The 0-1 / 0-255 / % choice is for color components; saturation, value and lightness read as percentages.
		const bool bComponent = Mode == EVekaColorPickerMode::RGB || Mode == EVekaColorPickerMode::Grayscale || Mode == EVekaColorPickerMode::CMYK;
		if (!bComponent)
		{
			return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Value * 100.f)));
		}
		switch (Display)
		{
		case EVekaColorChannelRange::Unit: return FText::FromString(FString::Printf(TEXT("%.2f"), Value));
		case EVekaColorChannelRange::Percent: return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Value * 100.f)));
		default: return FText::AsNumber(FMath::RoundToInt32(Value * 255.f));
		}
	}

	/**
	 * Channel values the panel last set. Channels are a lossy view of a color (hue 360 = hue 0, any hue at zero
	 * saturation): while they still give the current color, they are shown instead of re-decomposing the color.
	 */
	struct FChannelMemory
	{
		EVekaColorPickerMode Mode = EVekaColorPickerMode::RGB;
		TArray<float> Channels;
	};

	TArray<float> CurrentChannels(const FLinearColor& Color, EVekaColorPickerMode Mode, const FChannelMemory& Memory)
	{
		if (Memory.Mode == Mode && !Memory.Channels.IsEmpty() && ColorMath::Compose(Memory.Channels, Mode, Color).Equals(Color, 2e-3f))
		{
			return Memory.Channels;
		}
		return ColorMath::Decompose(Color, Mode);
	}

	class FVekaColorPanelView : public FVekaView
	{
		VEKA_VIEW(FVekaColorPanelView)

	public:
		TVekaBinding<FLinearColor> Color;
		EVekaColorPickerMode InitialMode = EVekaColorPickerMode::RGB;
		bool bSupportsOpacity = true;
		EVekaColorChannelRange Range = EVekaColorChannelRange::Byte;
		bool bCard = false;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const TVekaState<int32> ModeState = Ctx.State<int32>(TEXT("Mode"), static_cast<int32>(InitialMode));
			const EVekaColorPickerMode Mode = static_cast<EVekaColorPickerMode>(FMath::Clamp(ModeState.Get(), 0, static_cast<int32>(EVekaColorPickerMode::CMYK)));
			const FLinearColor Current = Color.Get();
			const TSharedPtr<FChannelMemory> Memory = Ctx.State<TSharedPtr<FChannelMemory>>(TEXT("Memory"), MakeShared<FChannelMemory>()).Get();

			TArray<FText> Names;
			TArray<FVector2f> Ranges;
			ColorMath::GetChannels(Mode, Names, Ranges);
			const TArray<float> Channels = CurrentChannels(Current, Mode, *Memory);

			TArray<FVekaElement> Rows;
			for (int32 Index = 0; Index < Names.Num(); ++Index)
			{
				const TVekaBinding<FLinearColor> Binding = Color;
				TVekaBinding<float> ChannelBinding(
					[Binding, Mode, Index, Memory]() { return CurrentChannels(Binding.Get(), Mode, *Memory)[Index]; },
					[Binding, Mode, Index, Memory](const float& Value)
					{
						const FLinearColor Base = Binding.Get();
						TArray<float> Values = CurrentChannels(Base, Mode, *Memory);
						Values[Index] = Value;
						Memory->Mode = Mode;
						Memory->Channels = Values;
						Binding.Set(ColorMath::Compose(Values, Mode, Base));
					});

				TArray<FVekaGradientStop> Stops;
				for (int32 Step = 0; Step <= 12; ++Step)
				{
					const float T = Step / 12.f;
					TArray<float> Values = Channels;
					Values[Index] = FMath::Lerp(Ranges[Index].X, Ranges[Index].Y, T);
					FLinearColor StopColor = ColorMath::Compose(Values, Mode, Current);
					StopColor.A = 1.f;
					Stops.Add(FVekaGradientStop(StopColor, T));
				}
				Rows.Add(HStack(8.f)(
					Text(Names[Index]).Font(EVekaTextStyle::Caption1).Opacity(0.7f).Frame(16.f, 16.f, FVekaAlignment::Leading()),
					ColorSlider(ChannelBinding, FVekaPaint::Linear(Stops).InSpace(EVekaColorSpace::Linear), Ranges[Index].X, Ranges[Index].Y),
					Text(ChannelValueText(Channels[Index], Ranges[Index], Mode, Range)).Font(EVekaTextStyle::Caption1).MonospacedDigit().Frame(40.f, 16.f, FVekaAlignment::Trailing())
				));
			}
			if (bSupportsOpacity)
			{
				const TVekaBinding<FLinearColor> Binding = Color;
				TVekaBinding<float> Alpha([Binding]() { return Binding.Get().A; }, [Binding](const float& Value) { FLinearColor C = Binding.Get(); C.A = Value; Binding.Set(C); });
				FLinearColor Opaque = Current;
				Opaque.A = 1.f;
				FLinearColor Clear = Current;
				Clear.A = 0.f;
				Rows.Add(HStack(8.f)(
					Text(INVTEXT("A")).Font(EVekaTextStyle::Caption1).Opacity(0.7f).Frame(16.f, 16.f, FVekaAlignment::Leading()),
					ColorSlider(Alpha, FVekaPaint::Linear(Clear, Opaque), 0.f, 1.f),
					Text(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Current.A * 100.f)))).Font(EVekaTextStyle::Caption1).MonospacedDigit()
						.Frame(40.f, 16.f, FVekaAlignment::Trailing())
				));
			}

			const TArray<FVekaPickerOption> Modes = {
				FVekaPickerOption(INVTEXT("RGB")), FVekaPickerOption(INVTEXT("HSV")), FVekaPickerOption(INVTEXT("okLCH")), FVekaPickerOption(INVTEXT("Hue")),
				FVekaPickerOption(INVTEXT("Luminosity")), FVekaPickerOption(INVTEXT("Saturation")), FVekaPickerOption(INVTEXT("Grayscale")), FVekaPickerOption(INVTEXT("CMYK")) };
			FVekaElement Panel = VStack(10.f, EVekaHAlign::Leading)(
				HStack(10.f)(
					RoundedRectangle(6.f).Fill(Current).Frame(40.f, 26.f).Border(TokenPaint(TEXT("Separator")), 1.f, 6.f),
					Text(FText::FromString(ColorMath::ToHex(Current))).Font(EVekaTextStyle::Callout).MonospacedDigit(),
					Spacer(),
					Picker(FText::GetEmpty(), ModeState, Modes).PickerStyle(EVekaPickerStyle::Menu).ControlSize(EVekaControlSize::Small)
				),
				VStack(6.f, EVekaHAlign::Leading).Children(MoveTemp(Rows))
			)
			.FlexFrame(260.0, 260.0, -1.0, -1.0, FVekaAlignment::TopLeading());
			if (!bCard)
			{
				return Panel;
			}
			return PopupCard(MoveTemp(Panel).Padding(12.f), Ctx.GetEnvironment(), 12.f);
		}
	};

	// --- Values ----------------------------------------------------------------------------------------------------

	class FVekaGaugeView : public FVekaView
	{
		VEKA_VIEW(FVekaGaugeView)

	public:
		float Value = 0.f;
		float Min = 0.f;
		float Max = 1.f;
		FText Label;
		FText CurrentValueLabel;
		FText MinLabel;
		FText MaxLabel;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const FVekaEnvironment& Env = Ctx.GetEnvironment();
			const EVekaGaugeStyle Style = VekaEnv::GetControlStyle(Env).GaugeStyle;
			const float Fraction = Max > Min ? FMath::Clamp((Value - Min) / (Max - Min), 0.f, 1.f) : 0.f;
			const FText Current = CurrentValueLabel.IsEmpty() ? FText::AsPercent(Fraction) : CurrentValueLabel;
			const FVekaPaint Gradient = FVekaPaint::Linear(TArray<FVekaGradientStop>{ FVekaGradientStop::FromToken(TEXT("Tint"), 0.f, EVekaHierarchy::Tertiary), FVekaGradientStop::FromToken(TEXT("Tint"), 1.f) });

			FVekaTrack T;
			T.Fraction = Fraction;
			T.TrackPaint = TokenPaint(TEXT("Fill"));
			T.FillPaint = TintPaint();
			const bool bAlt = Style == EVekaGaugeStyle::LinearAlt || Style == EVekaGaugeStyle::CircularAlt;
			if (bAlt)
			{
				T.TrackPaint = Gradient;
				T.bShowsFill = false;
				T.KnobSize = 11.f;
				T.KnobBorder = 3.f;
			}

			auto Caption = [](const FText& String) { return Text(String).Font(EVekaTextStyle::Caption2).Opacity(0.6f); };
			const bool bScale = !MinLabel.IsEmpty() || !MaxLabel.IsEmpty();

			if (Style == EVekaGaugeStyle::Circular || Style == EVekaGaugeStyle::CircularAlt)
			{
				T.Shape = EVekaTrackShape::Circular;
				T.Thickness = bAlt ? 6.f : 5.f;
				T.ArcDegrees = 270.f;
				T.IdealLength = 58.f;
				FVekaElement Ring = ZStack()(
					FVekaElement::Make(T),
					Text(Current).Font(EVekaTextStyle::Footnote).FontWeight(EVekaFontWeight::SemiBold).MonospacedDigit().LineLimit(1).MinimumScaleFactor(0.6f).Frame(40.f, 20.f)
				);
				return VStack(2.f)(
					MoveTemp(Ring),
					If(bScale, HStack()(Caption(MinLabel), Spacer(0.f), Caption(MaxLabel)).Frame(52.f, 12.f)),
					If(!Label.IsEmpty(), Caption(Label))
				);
			}

			T.Thickness = 6.f;
			return VStack(4.f, EVekaHAlign::Leading)(
				If(!Label.IsEmpty(), Text(Label).Font(EVekaTextStyle::Footnote)),
				FVekaElement::Make(T),
				If(bScale || !CurrentValueLabel.IsEmpty(), HStack()(Caption(MinLabel), Spacer(), Caption(CurrentValueLabel), Spacer(), Caption(MaxLabel)))
			);
		}
	};

	class FVekaProgressView : public FVekaView
	{
		VEKA_VIEW(FVekaProgressView)

	public:
		float Value = -1.f;
		FText Label;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			const EVekaProgressStyle Style = VekaEnv::GetControlStyle(Ctx.GetEnvironment()).ProgressStyle;
			const bool bIndeterminate = Value < 0.f;

			if (Style == EVekaProgressStyle::Lazy)
			{
				FVekaSymbolEffect Spin;
				Spin.Kind = EVekaSymbolEffectKind::Rotate;
				Spin.Speed = 0.8f;
				FVekaElement Spinner = Symbol(TEXT("loading")).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical).SymbolEffect(Spin, true);
				return Label.IsEmpty() ? MoveTemp(Spinner) : HStack(8.f)(MoveTemp(Spinner), Text(Label).Opacity(0.7f));
			}

			FVekaTrack T;
			T.Fraction = FMath::Clamp(Value, 0.f, 1.f);
			T.bIndeterminate = bIndeterminate;
			T.TrackPaint = TokenPaint(TEXT("Fill"));
			T.FillPaint = TintPaint();
			if (Style == EVekaProgressStyle::Circular)
			{
				T.Shape = EVekaTrackShape::Circular;
				T.Thickness = 3.5f;
				T.IdealLength = 22.f;
				FVekaElement Ring = FVekaElement::Make(T);
				return Label.IsEmpty() ? MoveTemp(Ring) : HStack(8.f)(MoveTemp(Ring), Text(Label).Opacity(0.7f));
			}
			T.Thickness = 5.f;
			return VStack(4.f, EVekaHAlign::Leading)(
				If(!Label.IsEmpty(), Text(Label).Font(EVekaTextStyle::Footnote)),
				FVekaElement::Make(T)
			);
		}
	};

	// --- Media / label ---------------------------------------------------------------------------------------------

	class FVekaMediaView : public FVekaView
	{
		VEKA_VIEW(FVekaMediaView)

	public:
		TObjectPtr<UMediaPlayer> Player;
		TObjectPtr<UMediaTexture> Texture;
		bool bShowsControls = true;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Player);
			Collector.AddReferencedObject(Texture);
		}

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			if (!Player)
			{
				return ContentUnavailable(TEXT("film"), INVTEXT("No media"), INVTEXT("Give MediaView a media player and its texture."));
			}
			const TVekaState<int32> Refresh = Ctx.State<int32>(TEXT("Refresh"), 0);
			const TWeakObjectPtr<UMediaPlayer> Weak = Player.Get();
			const bool bPlaying = Player->IsPlaying();

			FVekaMediaScrubber Scrubber;
			Scrubber.Player = Player;
			FVekaElement Controls = HStack(10.f)(
				Button(Symbol(bPlaying ? TEXT("pause") : TEXT("play")).SymbolVariant([] { FVekaSymbolVariant V; V.bFill = true; return V; }()).Foreground(FLinearColor::White),
					[Weak, Refresh]()
					{
						if (UMediaPlayer* Pinned = Weak.Get())
						{
							if (Pinned->IsPlaying())
							{
								Pinned->Pause();
							}
							else
							{
								Pinned->Play();
							}
						}
						Refresh.Set(Refresh.Get() + 1);
					}).ButtonStyle(EVekaButtonStyleKind::Plain),
				FVekaElement::Make(Scrubber).OnDrag([Weak](const FVekaDragValue& Drag)
				{
					UMediaPlayer* Pinned = Weak.Get();
					if (!Pinned || Drag.Phase == EVekaDragPhase::Began)
					{
						return;
					}
					const double Bar = FMath::Max(Drag.Size.X - 2.0 * FVekaMediaScrubber::LabelWidth, 1.0);
					const double F = FMath::Clamp((Drag.Location.X - FVekaMediaScrubber::LabelWidth) / Bar, 0.0, 1.0);
					Pinned->Seek(FTimespan::FromSeconds(F * Pinned->GetDuration().GetTotalSeconds()));
				})
			)
			.Padding(FMargin(12.f, 8.f))
			.Background(FLinearColor(0.f, 0.f, 0.f, 0.55f), 10.f)
			.Padding(10.f);

			return ZStack(FVekaAlignment::Bottom())(
				Image(Texture, true),
				If(bShowsControls, MoveTemp(Controls))
			).Background(FLinearColor::Black, 8.f).Clip();
		}
	};

	class FVekaLabelView : public FVekaView
	{
		VEKA_VIEW(FVekaLabelView)

	public:
		FText Title;
		FName SymbolName;

		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			switch (VekaEnv::GetControlStyle(Ctx.GetEnvironment()).LabelStyle)
			{
			case EVekaLabelStyle::Title: return Text(Title);
			case EVekaLabelStyle::Icon: return Symbol(SymbolName);
			default: return HStack(6.f)(Symbol(SymbolName), Text(Title));
			}
		}
	};
}

// --- Builders --------------------------------------------------------------------------------------------------

namespace Veka
{
	FVekaElement Label(const FText& Title, FName SymbolName)
	{
		TSharedRef<FVekaLabelView> View = MakeShared<FVekaLabelView>();
		View->Title = Title;
		View->SymbolName = SymbolName;
		return FVekaElement::MakeView(View);
	}

	FVekaElement Button(FVekaElement InLabel, TFunction<void()> Action, EVekaButtonRole Role)
	{
		return FVekaElement::MakeView(MakeShared<FVekaButtonView>(MoveTemp(InLabel), MoveTemp(Action), Role));
	}

	FVekaElement Button(const FText& Title, TFunction<void()> Action, EVekaButtonRole Role)
	{
		return Button(Text(Title), MoveTemp(Action), Role);
	}

	FVekaElement Button(const FText& Title, FName SymbolName, TFunction<void()> Action, EVekaButtonRole Role)
	{
		return Button(Label(Title, SymbolName), MoveTemp(Action), Role);
	}

	FVekaElement KeyBindButton(TVekaBinding<FKey> Key)
	{
		return FVekaElement::MakeView(MakeShared<FVekaKeyBindView>(MoveTemp(Key)));
	}

	FVekaElement Toggle(FVekaElement InLabel, TVekaBinding<bool> IsOn)
	{
		return FVekaElement::MakeView(MakeShared<FVekaToggleView>(MoveTemp(InLabel), MoveTemp(IsOn)));
	}

	FVekaElement Toggle(const FText& Title, TVekaBinding<bool> IsOn)
	{
		return Toggle(Text(Title), MoveTemp(IsOn));
	}

	FVekaElement Slider(TVekaBinding<float> Value, float Min, float Max, float Step)
	{
		TSharedRef<FVekaSliderView> View = MakeShared<FVekaSliderView>();
		View->Value = MoveTemp(Value);
		View->Min = Min;
		View->Max = FMath::Max(Max, Min);
		View->Step = FMath::Max(Step, 0.f);
		return FVekaElement::MakeView(View);
	}

	FVekaElement ColorSlider(TVekaBinding<float> Value, const FVekaPaint& Track, float Min, float Max)
	{
		TSharedRef<FVekaSliderView> View = MakeShared<FVekaSliderView>();
		View->Value = MoveTemp(Value);
		View->Min = Min;
		View->Max = FMath::Max(Max, Min);
		View->bColorTrack = true;
		View->Track = Track;
		return FVekaElement::MakeView(View);
	}

	FVekaElement Stepper(const FText& Title, TVekaBinding<float> Value, float Min, float Max, float Step, int32 Decimals)
	{
		TSharedRef<FVekaStepperView> View = MakeShared<FVekaStepperView>();
		View->Title = Title;
		View->Value = MoveTemp(Value);
		View->Min = Min;
		View->Max = FMath::Max(Max, Min);
		View->Step = Step;
		View->Decimals = FMath::Max(0, Decimals);
		return FVekaElement::MakeView(View);
	}

	FVekaElement Picker(const FText& Title, TVekaBinding<int32> Selection, TArray<FVekaPickerOption> Options, bool bAllowsEmpty)
	{
		TSharedRef<FVekaPickerView> View = MakeShared<FVekaPickerView>();
		View->Title = Title;
		View->Selection = MoveTemp(Selection);
		View->Options = MoveTemp(Options);
		View->bAllowsEmpty = bAllowsEmpty;
		return FVekaElement::MakeView(View);
	}

	FVekaElement MenuContent(TArray<FVekaMenuItem> Items)
	{
		TSharedRef<FVekaMenuContentView> View = MakeShared<FVekaMenuContentView>();
		View->Items = MoveTemp(Items);
		return FVekaElement::MakeView(View);
	}

	FVekaElement Menu(FVekaElement InLabel, TArray<FVekaMenuItem> Items)
	{
		return Button(HStack(5.f)(MoveTemp(InLabel), Symbol(TEXT("chevron.down")).SymbolWeight(0.7f).SymbolScale(EVekaSymbolScale::Small)),
			[Items = MoveTemp(Items)]() { Veka::Popup::Present(MenuContent(Items), EVekaPopupPlacement::Below); });
	}

	FVekaElement Menu(const FText& Title, TArray<FVekaMenuItem> Items)
	{
		return Menu(Text(Title), MoveTemp(Items));
	}

	FVekaElement TextField(const FText& Placeholder, TVekaBinding<FString> InText, TFunction<void()> OnSubmit)
	{
		FVekaTextInput Input;
		Input.Text = InText.Get();
		Input.Placeholder = Placeholder;
		Input.OnChanged = [InText](const FString& Value) { InText.Set(Value); };
		Input.OnCommitted = [OnSubmit](const FString&, bool bSubmitted) { if (bSubmitted && OnSubmit) { OnSubmit(); } };
		return FVekaElement::Make(Input);
	}

	FVekaElement SecureField(const FText& Placeholder, TVekaBinding<FString> InText, TFunction<void()> OnSubmit)
	{
		FVekaTextInput Input;
		Input.Text = InText.Get();
		Input.Placeholder = Placeholder;
		Input.bSecure = true;
		Input.OnChanged = [InText](const FString& Value) { InText.Set(Value); };
		Input.OnCommitted = [OnSubmit](const FString&, bool bSubmitted) { if (bSubmitted && OnSubmit) { OnSubmit(); } };
		return FVekaElement::Make(Input);
	}

	FVekaElement NumberField(const FText& Placeholder, TVekaBinding<double> Value, EVekaTextFieldType Type, const FString& CurrencyCode)
	{
		auto Format = [Type, CurrencyCode](double Number) -> FString
		{
			switch (Type)
			{
			case EVekaTextFieldType::Percent: return FText::AsPercent(Number).ToString();
			case EVekaTextFieldType::Currency: return FText::AsCurrencyBase(FMath::RoundToInt64(Number * 100.0), CurrencyCode).ToString();
			case EVekaTextFieldType::Number: return FText::AsNumber(Number).ToString();
			default: return FString::SanitizeFloat(Number);
			}
		};
		auto Parse = [Type](const FString& Entered, double& OutNumber) -> bool
		{
			// Keep digits, sign and separators; the last separator is the decimal point when both appear.
			FString Clean;
			for (const TCHAR Char : Entered)
			{
				if (FChar::IsDigit(Char) || Char == TEXT('-') || Char == TEXT(',') || Char == TEXT('.'))
				{
					Clean.AppendChar(Char);
				}
			}
			int32 LastComma = INDEX_NONE;
			int32 LastDot = INDEX_NONE;
			Clean.FindLastChar(TEXT(','), LastComma);
			Clean.FindLastChar(TEXT('.'), LastDot);
			if (LastComma != INDEX_NONE && LastDot != INDEX_NONE)
			{
				Clean.ReplaceInline(LastComma > LastDot ? TEXT(".") : TEXT(","), TEXT(""));
			}
			Clean.ReplaceInline(TEXT(","), TEXT("."));
			if (Clean.IsEmpty() || !FCString::IsNumeric(*Clean))
			{
				return false;
			}
			OutNumber = FCString::Atod(*Clean);
			if (Type == EVekaTextFieldType::Percent)
			{
				OutNumber /= 100.0;
			}
			return true;
		};

		FVekaTextInput Input;
		Input.Text = Format(Value.Get());
		Input.Placeholder = Placeholder;
		Input.bSelectAllOnFocus = true;
		Input.OnCommitted = [Value, Parse](const FString& Committed, bool)
		{
			double Number = 0.0;
			if (Parse(Committed, Number))
			{
				Value.Set(Number);
			}
		};
		return FVekaElement::Make(Input);
	}

	FVekaElement ColorPicker(const FText& Title, TVekaBinding<FLinearColor> Color, bool bSupportsOpacity)
	{
		const FLinearColor Current = Color.Get();
		FVekaElement Swatch = Circle().Fill(Current).Frame(22.f, 22.f).Border(FVekaPaint::FromToken(TEXT("Separator")), 1.f, 11.f);
		FVekaElement SwatchButton = Button(MoveTemp(Swatch), [Color, bSupportsOpacity]()
		{
			TSharedRef<FVekaColorPanelView> Panel = MakeShared<FVekaColorPanelView>();
			Panel->Color = Color;
			Panel->bSupportsOpacity = bSupportsOpacity;
			Panel->bCard = true;
			Veka::Popup::Present(FVekaElement::MakeView(Panel), EVekaPopupPlacement::Below);
		}).ButtonStyle(EVekaButtonStyleKind::Plain);
		return Title.IsEmpty() ? MoveTemp(SwatchButton) : HStack(10.f)(Text(Title), Spacer(), MoveTemp(SwatchButton));
	}

	FVekaElement ColorPanel(TVekaBinding<FLinearColor> Color, EVekaColorPickerMode Mode, bool bSupportsOpacity, EVekaColorChannelRange Range)
	{
		TSharedRef<FVekaColorPanelView> Panel = MakeShared<FVekaColorPanelView>();
		Panel->Color = MoveTemp(Color);
		Panel->InitialMode = Mode;
		Panel->bSupportsOpacity = bSupportsOpacity;
		Panel->Range = Range;
		return FVekaElement::MakeView(Panel);
	}

	FVekaElement Gauge(float Value, float Min, float Max, const FText& InLabel, const FText& CurrentValueLabel, const FText& MinLabel, const FText& MaxLabel)
	{
		TSharedRef<FVekaGaugeView> View = MakeShared<FVekaGaugeView>();
		View->Value = Value;
		View->Min = Min;
		View->Max = Max;
		View->Label = InLabel;
		View->CurrentValueLabel = CurrentValueLabel;
		View->MinLabel = MinLabel;
		View->MaxLabel = MaxLabel;
		return FVekaElement::MakeView(View);
	}

	FVekaElement ProgressView(float Value, const FText& InLabel)
	{
		TSharedRef<FVekaProgressView> View = MakeShared<FVekaProgressView>();
		View->Value = Value;
		View->Label = InLabel;
		return FVekaElement::MakeView(View);
	}

	FVekaElement Divider()
	{
		return FVekaElement::Make(FVekaDivider());
	}

	FVekaElement MediaView(UMediaPlayer* Player, UMediaTexture* Texture, bool bShowsControls)
	{
		TSharedRef<FVekaMediaView> View = MakeShared<FVekaMediaView>();
		View->Player = Player;
		View->Texture = Texture;
		View->bShowsControls = bShowsControls;
		return FVekaElement::MakeView(View);
	}

	FVekaElement ContentUnavailable(FName SymbolName, const FText& Title, const FText& Description, FVekaElement Actions)
	{
		return VStack(8.f)(
			Symbol(SymbolName).Font(EVekaTextStyle::LargeTitle).Opacity(0.45f),
			Text(Title).Font(EVekaTextStyle::Title3).FontWeight(EVekaFontWeight::Bold),
			If(!Description.IsEmpty(), Text(Description).Opacity(0.6f).MultilineAlignment(EVekaHAlign::Center)),
			MoveTemp(Actions)
		).Padding(24.f);
	}
}

// --- Popups ----------------------------------------------------------------------------------------------------

namespace Veka::Popup
{
	bool Present(const FVekaElement& Content, EVekaPopupPlacement Placement)
	{
		const Veka::Interaction::FEventSource* Source = Veka::Interaction::GetCurrentEventSource();
		if (!Source || !FSlateApplication::IsInitialized())
		{
			return false;
		}
		const TSharedPtr<SWidget> Widget = Source->Widget.Pin();
		if (!Widget.IsValid())
		{
			return false;
		}
		FWidgetPath Path;
		if (!FSlateApplication::Get().FindPathToWidget(Widget.ToSharedRef(), Path))
		{
			return false;
		}

		TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetEnvironment(Source->Environment);
		Host->SetRoot(Content);

		const FVector2D Size(Source->Geometry.GetLocalSize());
		FVector2D Location;
		switch (Placement)
		{
		case EVekaPopupPlacement::Right: Location = FVector2D(Source->Geometry.LocalToAbsolute(FVector2D(Size.X + 2.0, -5.0))); break;
		case EVekaPopupPlacement::AtCursor: Location = Source->ScreenPosition; break;
		default: Location = FVector2D(Source->Geometry.LocalToAbsolute(FVector2D(0.0, Size.Y + 4.0))); break;
		}
		const FPopupTransitionEffect Effect(Placement == EVekaPopupPlacement::AtCursor ? FPopupTransitionEffect::ContextMenu : FPopupTransitionEffect::ComboButton);
		return FSlateApplication::Get().PushMenu(Widget.ToSharedRef(), Path, Host, Location, Effect, true).IsValid();
	}

	void DismissAll()
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

// --- Color math ------------------------------------------------------------------------------------------------

namespace Veka::ColorMath
{
	namespace
	{
		FVector3f ToSRGB(const FLinearColor& C)
		{
			using namespace Veka::Colora;
			return FVector3f(LinearToSRGB(FMath::Clamp(C.R, 0.f, 1.f)), LinearToSRGB(FMath::Clamp(C.G, 0.f, 1.f)), LinearToSRGB(FMath::Clamp(C.B, 0.f, 1.f)));
		}

		FLinearColor FromSRGB(const FVector3f& S, float Alpha)
		{
			using namespace Veka::Colora;
			return FLinearColor(SRGBToLinear(FMath::Clamp(S.X, 0.f, 1.f)), SRGBToLinear(FMath::Clamp(S.Y, 0.f, 1.f)), SRGBToLinear(FMath::Clamp(S.Z, 0.f, 1.f)), Alpha);
		}

		FVector3f ToHSV(const FVector3f& S)
		{
			const float Max = FMath::Max3(S.X, S.Y, S.Z);
			const float Min = FMath::Min3(S.X, S.Y, S.Z);
			const float Delta = Max - Min;
			float H = 0.f;
			if (Delta > 1e-6f)
			{
				if (Max == S.X) { H = 60.f * FMath::Fmod((S.Y - S.Z) / Delta, 6.f); }
				else if (Max == S.Y) { H = 60.f * ((S.Z - S.X) / Delta + 2.f); }
				else { H = 60.f * ((S.X - S.Y) / Delta + 4.f); }
			}
			if (H < 0.f) { H += 360.f; }
			return FVector3f(H, Max > 1e-6f ? Delta / Max : 0.f, Max);
		}

		FVector3f FromHSV(const FVector3f& HSV)
		{
			const float H = FMath::Fmod(FMath::Max(HSV.X, 0.f), 360.f) / 60.f;
			const float C = HSV.Z * HSV.Y;
			const float X = C * (1.f - FMath::Abs(FMath::Fmod(H, 2.f) - 1.f));
			const float M = HSV.Z - C;
			FVector3f RGB;
			switch (FMath::FloorToInt32(H))
			{
			case 0: RGB = FVector3f(C, X, 0.f); break;
			case 1: RGB = FVector3f(X, C, 0.f); break;
			case 2: RGB = FVector3f(0.f, C, X); break;
			case 3: RGB = FVector3f(0.f, X, C); break;
			case 4: RGB = FVector3f(X, 0.f, C); break;
			default: RGB = FVector3f(C, 0.f, X); break;
			}
			return RGB + FVector3f(M, M, M);
		}

		FVekaOkLCH ToLCH(const FLinearColor& C)
		{
			return Veka::Colora::OkLabToOkLCH(Veka::Colora::LinearToOkLab(C));
		}

		FLinearColor FromLCH(float L, float Chroma, float H, float Alpha)
		{
			FVekaOkLCH Lch;
			Lch.L = L;
			Lch.C = Chroma;
			Lch.H = H;
			FLinearColor Out = Veka::Colora::OkLabToLinear(Veka::Colora::OkLCHToOkLab(Lch), Alpha);
			Out.R = FMath::Clamp(Out.R, 0.f, 1.f);
			Out.G = FMath::Clamp(Out.G, 0.f, 1.f);
			Out.B = FMath::Clamp(Out.B, 0.f, 1.f);
			return Out;
		}
	}

	TArray<float> Decompose(const FLinearColor& Color, EVekaColorPickerMode Mode)
	{
		const FVector3f S = ToSRGB(Color);
		switch (Mode)
		{
		case EVekaColorPickerMode::HSV: { const FVector3f H = ToHSV(S); return { H.X, H.Y, H.Z }; }
		case EVekaColorPickerMode::OkLCH: { const FVekaOkLCH L = ToLCH(Color); return { L.L, L.C, L.H }; }
		case EVekaColorPickerMode::Hue: return { ToHSV(S).X };
		case EVekaColorPickerMode::Luminosity: return { ToLCH(Color).L };
		case EVekaColorPickerMode::Saturation: return { ToHSV(S).Y };
		case EVekaColorPickerMode::Grayscale: return { 0.2126f * S.X + 0.7152f * S.Y + 0.0722f * S.Z };
		case EVekaColorPickerMode::CMYK:
		{
			const float K = 1.f - FMath::Max3(S.X, S.Y, S.Z);
			const float D = FMath::Max(1.f - K, 1e-6f);
			return { (1.f - S.X - K) / D, (1.f - S.Y - K) / D, (1.f - S.Z - K) / D, K };
		}
		default: return { S.X, S.Y, S.Z };
		}
	}

	FLinearColor Compose(TConstArrayView<float> Channels, EVekaColorPickerMode Mode, const FLinearColor& Base)
	{
		auto At = [&Channels](int32 Index) { return Channels.IsValidIndex(Index) ? Channels[Index] : 0.f; };
		const float Alpha = Base.A;
		const FVector3f S = ToSRGB(Base);
		switch (Mode)
		{
		case EVekaColorPickerMode::HSV: return FromSRGB(FromHSV(FVector3f(At(0), At(1), At(2))), Alpha);
		case EVekaColorPickerMode::OkLCH: return FromLCH(At(0), At(1), At(2), Alpha);
		case EVekaColorPickerMode::Hue: { FVector3f H = ToHSV(S); H.X = At(0); return FromSRGB(FromHSV(H), Alpha); }
		case EVekaColorPickerMode::Luminosity: { const FVekaOkLCH L = ToLCH(Base); return FromLCH(At(0), L.C, L.H, Alpha); }
		case EVekaColorPickerMode::Saturation: { FVector3f H = ToHSV(S); H.Y = At(0); return FromSRGB(FromHSV(H), Alpha); }
		case EVekaColorPickerMode::Grayscale: return FromSRGB(FVector3f(At(0), At(0), At(0)), Alpha);
		case EVekaColorPickerMode::CMYK:
		{
			const float K = At(3);
			return FromSRGB(FVector3f((1.f - At(0)) * (1.f - K), (1.f - At(1)) * (1.f - K), (1.f - At(2)) * (1.f - K)), Alpha);
		}
		default: return FromSRGB(FVector3f(At(0), At(1), At(2)), Alpha);
		}
	}

	void GetChannels(EVekaColorPickerMode Mode, TArray<FText>& OutNames, TArray<FVector2f>& OutRanges)
	{
		const FVector2f Unit(0.f, 1.f);
		const FVector2f Degrees(0.f, 360.f);
		switch (Mode)
		{
		case EVekaColorPickerMode::HSV: OutNames = { INVTEXT("H"), INVTEXT("S"), INVTEXT("V") }; OutRanges = { Degrees, Unit, Unit }; break;
		case EVekaColorPickerMode::OkLCH: OutNames = { INVTEXT("L"), INVTEXT("C"), INVTEXT("H") }; OutRanges = { Unit, FVector2f(0.f, 0.37f), Degrees }; break;
		case EVekaColorPickerMode::Hue: OutNames = { INVTEXT("H") }; OutRanges = { Degrees }; break;
		case EVekaColorPickerMode::Luminosity: OutNames = { INVTEXT("L") }; OutRanges = { Unit }; break;
		case EVekaColorPickerMode::Saturation: OutNames = { INVTEXT("S") }; OutRanges = { Unit }; break;
		case EVekaColorPickerMode::Grayscale: OutNames = { INVTEXT("W") }; OutRanges = { Unit }; break;
		case EVekaColorPickerMode::CMYK: OutNames = { INVTEXT("C"), INVTEXT("M"), INVTEXT("Y"), INVTEXT("K") }; OutRanges = { Unit, Unit, Unit, Unit }; break;
		default: OutNames = { INVTEXT("R"), INVTEXT("G"), INVTEXT("B") }; OutRanges = { Unit, Unit, Unit }; break;
		}
	}

	FString ToHex(const FLinearColor& Color)
	{
		const FColor C = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X"), C.R, C.G, C.B);
	}
}
