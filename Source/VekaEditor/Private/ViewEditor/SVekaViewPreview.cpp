// Copyright TheMiniBoss. All Rights Reserved.

#include "ViewEditor/SVekaViewPreview.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Modifiers/VekaLayoutModifiers.h"
#include "View/VekaViewAsset.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "VekaViewEditor"

namespace Veka::Editor
{
	const TArray<FVekaDevicePreset>& DevicePresets()
	{
		static const TArray<FVekaDevicePreset> Presets = {
			{ LOCTEXT("Device1080p", "1080p"), FIntPoint(1920, 1080) },
			{ LOCTEXT("Device1440p", "1440p"), FIntPoint(2560, 1440) },
			{ LOCTEXT("Device4K", "4K"), FIntPoint(3840, 2160) },
			{ LOCTEXT("DeviceUltrawide", "Ultrawide"), FIntPoint(3440, 1440) },
			{ LOCTEXT("DeviceMobile", "Mobile"), FIntPoint(1170, 2532) },
		};
		return Presets;
	}

	float ProjectDPIScale(FIntPoint Size)
	{
		const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
		return Scale > KINDA_SMALL_NUMBER ? Scale : 1.f;
	}
}

namespace
{
	const TCHAR* ConfigSection = TEXT("VekaUI.ViewEditor");
	constexpr float MinZoom = 0.05f;
	constexpr float MaxZoom = 8.f;
	constexpr float FitMargin = 32.f;
}

/**
 * Places its child (the device screen, laid out at the logical size) scaled by zoom x DPI, centered + pan, and handles
 * zoom (wheel, around the cursor) and pan (right or middle drag).
 */
class SVekaPreviewCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaPreviewCanvas) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetClipping(EWidgetClipping::ClipToBounds);
		ChildSlot[InArgs._Content.Widget];
	}

	void SetDevice(FIntPoint InSize, float InDPIScale, const FText& InLabel)
	{
		if (InSize != DeviceSize)
		{
			bFitPending = true;
			bUserView = false;
		}
		DeviceSize = InSize;
		DPIScale = InDPIScale;
		Label = InLabel;
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	void ZoomToFit()
	{
		bFitPending = true;
		bUserView = false;
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	float GetZoom() const { return Zoom; }

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
	{
		const FVector2D Available = FVector2D(AllottedGeometry.GetLocalSize());
		// Until the user zooms or pans, the screen follows the viewport size.
		if (!bUserView && !Available.Equals(LastAvailable, 0.5))
		{
			bFitPending = true;
		}
		LastAvailable = Available;
		if (bFitPending && Available.X > 1.0 && Available.Y > 1.0)
		{
			Zoom = FMath::Clamp(static_cast<float>(FMath::Min((Available.X - 2.0 * FitMargin) / DeviceSize.X, (Available.Y - 2.0 * FitMargin) / DeviceSize.Y)), MinZoom, MaxZoom);
			Pan = FVector2D::ZeroVector;
			bFitPending = false;
		}
		const FVector2D Logical = FVector2D(DeviceSize) / DPIScale;
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2f(Logical), FSlateLayoutTransform(Zoom * DPIScale, FVector2f(ScreenOrigin(Available)))));
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
		const FVector2D Available = FVector2D(AllottedGeometry.GetLocalSize());
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0.012f, 0.012f, 0.014f));

		// Screen shadow and label.
		const FVector2D Origin = ScreenOrigin(Available);
		const FVector2D Screen = FVector2D(DeviceSize) * Zoom;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(Screen + FVector2D(8.0)), FSlateLayoutTransform(FVector2f(Origin - FVector2D(4.0)))),
			White, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.5f));
		const FText Caption = FText::Format(LOCTEXT("ScreenCaption", "{0}   {1}%"), Label, FText::AsNumber(FMath::RoundToInt(Zoom * 100.f)));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(400.f, 16.f), FSlateLayoutTransform(FVector2f(Origin + FVector2D(0.0, -20.0)))),
			Caption, FAppStyle::GetFontStyle("SmallFont"), ESlateDrawEffect::None, FLinearColor(0.5f, 0.5f, 0.55f));

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.0, 240.0); }

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const FVector2D Available = FVector2D(MyGeometry.GetLocalSize());
		const FVector2D Cursor = FVector2D(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		const FVector2D DevicePoint = (Cursor - ScreenOrigin(Available)) / Zoom;
		Zoom = FMath::Clamp(Zoom * FMath::Pow(1.15f, MouseEvent.GetWheelDelta()), MinZoom, MaxZoom);
		// Keep the point under the cursor in place.
		const FVector2D NewOrigin = Cursor - DevicePoint * Zoom;
		Pan = NewOrigin - Available * 0.5 + FVector2D(DeviceSize) * Zoom * 0.5;
		bUserView = true;
		Invalidate(EInvalidateWidgetReason::Layout);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
		{
			bPanning = true;
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bPanning && (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton))
		{
			bPanning = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bPanning)
		{
			Pan += FVector2D(MouseEvent.GetCursorDelta()) / MyGeometry.Scale;
			bUserView = true;
			Invalidate(EInvalidateWidgetReason::Layout);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return bPanning ? FCursorReply::Cursor(EMouseCursor::GrabHandClosed) : FCursorReply::Unhandled();
	}

private:
	FVector2D ScreenOrigin(const FVector2D& Available) const
	{
		return Available * 0.5 + Pan - FVector2D(DeviceSize) * Zoom * 0.5;
	}

	FIntPoint DeviceSize = FIntPoint(1920, 1080);
	float DPIScale = 1.f;
	FText Label;
	mutable float Zoom = 0.25f;
	mutable FVector2D Pan = FVector2D::ZeroVector;
	mutable bool bFitPending = true;
	mutable FVector2D LastAvailable = FVector2D::ZeroVector;
	/** The user zoomed or panned: keep their view when the viewport is resized. */
	bool bUserView = false;
	bool bPanning = false;
};

/** Bounds of every view and padding areas, drawn over the screen (same space as the host). */
class SVekaPreviewOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaPreviewOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SVekaHost>& InHost, const FVekaPreviewSettings* InSettings)
	{
		Host = InHost;
		Settings = InSettings;
		SetVisibility(EVisibility::HitTestInvisible);
	}

	mutable int32 LastRectCount = 0;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		LastRectCount = 0;
		const TSharedPtr<SVekaHost> PinnedHost = Host.Pin();
		if (!PinnedHost || (!Settings->bShowBounds && !Settings->bShowPadding))
		{
			return LayerId;
		}
		if (const TSharedPtr<FVekaNode> Root = PinnedHost->GetRuntime()->GetRootNode())
		{
			PaintNode(*Root, AllottedGeometry, OutDrawElements, LayerId);
		}
		return LayerId + 1;
	}

private:
	void PaintNode(const FVekaNode& Node, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		const FVector2D Origin = Node.GetRootPosition();
		if (Settings->bShowPadding)
		{
			const TArray<FInstancedStruct>& Modifiers = Node.GetModifiers();
			for (int32 Level = 0; Level < Modifiers.Num(); ++Level)
			{
				if (!Modifiers[Level].GetPtr<FVekaPaddingModifier>())
				{
					continue;
				}
				// Modifier rects go from the innermost (0) to the outermost; the padding is between its rect and the next one in.
				const FVekaRect Outer = Node.GetModifierRect(Level);
				const FVekaRect Inner = Level > 0 ? Node.GetModifierRect(Level - 1) : Node.GetContentRect();
				const FLinearColor Green(0.2f, 0.85f, 0.4f, 0.28f);
				Fill(Geometry, OutDrawElements, LayerId, Origin + Outer.Position, FVector2D(Outer.Size.X, Inner.Top() - Outer.Top()), Green);
				Fill(Geometry, OutDrawElements, LayerId, FVector2D(Origin.X + Outer.Left(), Origin.Y + Inner.Bottom()), FVector2D(Outer.Size.X, Outer.Bottom() - Inner.Bottom()), Green);
				Fill(Geometry, OutDrawElements, LayerId, FVector2D(Origin.X + Outer.Left(), Origin.Y + Inner.Top()), FVector2D(Inner.Left() - Outer.Left(), Inner.Size.Y), Green);
				Fill(Geometry, OutDrawElements, LayerId, FVector2D(Origin.X + Inner.Right(), Origin.Y + Inner.Top()), FVector2D(Outer.Right() - Inner.Right(), Inner.Size.Y), Green);
			}
		}
		if (Settings->bShowBounds)
		{
			const FVector2D Size = Node.GetArrangedRect().Size;
			const float Scale = Geometry.Scale > 0.f ? 1.f / Geometry.Scale : 1.f;
			TArray<FVector2f> Points = { FVector2f(Origin), FVector2f(Origin + FVector2D(Size.X, 0.0)), FVector2f(Origin + Size), FVector2f(Origin + FVector2D(0.0, Size.Y)), FVector2f(Origin) };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, FLinearColor(0.25f, 0.75f, 1.f, 0.85f), true, Scale);
			++LastRectCount;
		}
		for (const TSharedRef<FVekaNode>& Child : Node.GetChildren())
		{
			PaintNode(*Child, Geometry, OutDrawElements, LayerId);
		}
	}

	void Fill(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))), FAppStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None, Color);
		++LastRectCount;
	}

	TWeakPtr<SVekaHost> Host;
	const FVekaPreviewSettings* Settings = nullptr;
};

void SVekaViewPreview::Construct(const FArguments& InArgs)
{
	View = InArgs._View;
	bPersistSettings = InArgs._bPersistSettings;
	if (bPersistSettings)
	{
		LoadSettings();
	}

	Host = SNew(SVekaHost);
	Overlay = SNew(SVekaPreviewOverlay, Host.ToSharedRef(), &Settings);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeToolbar()
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(Canvas, SVekaPreviewCanvas)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("WhiteBrush"))
					.ColorAndOpacity(this, &SVekaViewPreview::GetScreenColor)
				]
				+ SOverlay::Slot()
				[
					Host.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					Overlay.ToSharedRef()
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.f, 3.f))
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.Text(this, &SVekaViewPreview::GetStatusText)
			]
		]
	];

	ApplyEnvironment();
	ApplyDevice();
	Refresh();
}

SVekaViewPreview::~SVekaViewPreview()
{
	if (Store.IsValid())
	{
		Store->OnEvent = nullptr;
	}
}

void SVekaViewPreview::Refresh()
{
	UVekaView* Asset = View.Get();
	if (!Asset)
	{
		Host->SetRoot(FVekaElement());
		return;
	}

	UClass* ModelClass = nullptr;
#if WITH_EDITORONLY_DATA
	ModelClass = Asset->PreviewModelClass.Get();
#endif
	if (!ModelClass)
	{
		Model.Reset();
	}
	else if (!Model.IsValid() || Model->GetClass() != ModelClass)
	{
		Model.Reset(NewObject<UVekaViewModel>(GetTransientPackage(), ModelClass, NAME_None, RF_Transient));
	}

	TSharedRef<FVekaViewStore> NewStore = MakeShared<FVekaViewStore>();
	if (Store.IsValid())
	{
		// Keep what the user did in the preview (toggles, counters) across edits; a state still at its declared
		// value takes the new declared value (editing an initial value shows at once).
		for (const FVekaStateDeclaration& State : Asset->Document.States)
		{
			const FVekaValue* Declared = DeclaredInitials.Find(State.Name);
			if (Store->Contains(State.Name) && Store->Get(State.Name).Type == State.Initial.Type && (!Declared || !(Store->Get(State.Name) == *Declared)))
			{
				NewStore->Set(State.Name, Store->Get(State.Name));
			}
		}
		Store->OnEvent = nullptr;
	}
	NewStore->SetModel(Model.Get());
	TWeakPtr<SVekaViewPreview> WeakThis = SharedThis(this);
	NewStore->OnEvent = [WeakThis](FName Event, const FVekaValue& Payload)
	{
		if (TSharedPtr<SVekaViewPreview> Preview = WeakThis.Pin())
		{
			Preview->LastEvent = Payload.IsNone() ? FText::FromName(Event) : FText::Format(LOCTEXT("EventWithPayload", "{0}({1})"), FText::FromName(Event), FText::FromString(Payload.AsString()));
		}
	};
	Store = NewStore;
	DeclaredInitials.Reset();
	for (const FVekaStateDeclaration& State : Asset->Document.States)
	{
		DeclaredInitials.Add(State.Name, State.Initial);
	}
	Host->SetRoot(Asset->BuildRoot(NewStore));
	++RefreshCount;
}

void SVekaViewPreview::ResetState()
{
	if (Store.IsValid())
	{
		Store->OnEvent = nullptr;
	}
	Store.Reset();
	LastEvent = FText::GetEmpty();
	Refresh();
}

void SVekaViewPreview::SetDevice(int32 Index)
{
	Settings.Device = FMath::Clamp(Index, 0, Veka::Editor::DevicePresets().Num() - 1);
	ApplyDevice();
	SaveSettings();
}

void SVekaViewPreview::SetDPIOverride(float Scale)
{
	Settings.DPIOverride = FMath::Max(0.f, Scale);
	ApplyDevice();
	SaveSettings();
}

void SVekaViewPreview::SetScheme(EVekaColorScheme Scheme)
{
	Settings.Scheme = Scheme;
	ApplyEnvironment();
	SaveSettings();
}

void SVekaViewPreview::SetTheme(UVekaTheme* Theme)
{
	Settings.Theme = Theme;
	ApplyEnvironment();
	SaveSettings();
}

void SVekaViewPreview::SetShowBounds(bool bShow)
{
	Settings.bShowBounds = bShow;
	Overlay->Invalidate(EInvalidateWidgetReason::Paint);
	SaveSettings();
}

void SVekaViewPreview::SetShowPadding(bool bShow)
{
	Settings.bShowPadding = bShow;
	Overlay->Invalidate(EInvalidateWidgetReason::Paint);
	SaveSettings();
}

void SVekaViewPreview::ZoomToFit()
{
	Canvas->ZoomToFit();
}

FIntPoint SVekaViewPreview::GetDeviceSize() const
{
	return Veka::Editor::DevicePresets()[Settings.Device].Size;
}

float SVekaViewPreview::GetDPIScale() const
{
	return Settings.DPIOverride > 0.f ? Settings.DPIOverride : Veka::Editor::ProjectDPIScale(GetDeviceSize());
}

FVector2D SVekaViewPreview::GetLogicalSize() const
{
	return FVector2D(GetDeviceSize()) / GetDPIScale();
}

float SVekaViewPreview::GetZoom() const
{
	return Canvas->GetZoom();
}

int32 SVekaViewPreview::GetOverlayRectCount() const
{
	return Overlay->LastRectCount;
}

FReply SVekaViewPreview::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F)
	{
		ZoomToFit();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SVekaViewPreview::ApplyEnvironment()
{
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Settings.Scheme);
	Environment.Set<const UVekaTheme*>(VekaEnv::Theme(), Settings.Theme.Get());
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	Host->SetEnvironment(Environment);
}

void SVekaViewPreview::ApplyDevice()
{
	const FIntPoint Size = GetDeviceSize();
	Canvas->SetDevice(Size, GetDPIScale(), GetDeviceLabel());
}

FSlateColor SVekaViewPreview::GetScreenColor() const
{
	FVekaEnvironment Environment;
	Environment.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Settings.Scheme);
	Environment.Set<const UVekaTheme*>(VekaEnv::Theme(), Settings.Theme.Get());
	// Screen = Background; cards use WindowBackground (white / black screen, light gray / dark gray cards).
	return FSlateColor(Veka::Colora::ResolveColor(TEXT("Background"), Environment));
}

FText SVekaViewPreview::GetDeviceLabel() const
{
	const FVekaDevicePreset& Preset = Veka::Editor::DevicePresets()[Settings.Device];
	return FText::Format(LOCTEXT("DeviceLabel", "{0}  {1} x {2}"), Preset.Name, FText::AsNumber(Preset.Size.X, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(Preset.Size.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
}

FText SVekaViewPreview::GetDPILabel() const
{
	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = 2;
	Format.MaximumFractionalDigits = 2;
	return Settings.DPIOverride > 0.f ? FText::Format(LOCTEXT("DPIFixed", "DPI {0}"), FText::AsNumber(Settings.DPIOverride, &Format))
		: FText::Format(LOCTEXT("DPIAuto", "DPI auto ({0})"), FText::AsNumber(GetDPIScale(), &Format));
}

FText SVekaViewPreview::GetStatusText() const
{
	const FVector2D Logical = GetLogicalSize();
	const FText Layout = FText::Format(LOCTEXT("StatusLayout", "Layout {0} x {1} pt"), FText::AsNumber(FMath::RoundToInt(Logical.X)), FText::AsNumber(FMath::RoundToInt(Logical.Y)));
	const FText ModelText = Model.IsValid() ? FText::Format(LOCTEXT("StatusModel", "model {0}"), FText::FromString(Model->GetClass()->GetName())) : LOCTEXT("StatusNoModel", "no preview model");
	if (LastEvent.IsEmpty())
	{
		return FText::Format(LOCTEXT("Status", "{0}   ·   {1}   ·   {2}"), Layout, GetDPILabel(), ModelText);
	}
	return FText::Format(LOCTEXT("StatusEvent", "{0}   ·   {1}   ·   {2}   ·   last event: {3}"), Layout, GetDPILabel(), ModelText, LastEvent);
}

TSharedRef<SWidget> SVekaViewPreview::MakeToolbar()
{
	auto Toggle = [this](const FText& Label, const FText& Tooltip, TFunction<bool()> IsOn, TFunction<void(bool)> Set) -> TSharedRef<SWidget>
	{
		return SNew(SCheckBox)
			.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
			.ToolTipText(Tooltip)
			.IsChecked_Lambda([IsOn]() { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); })
			[
				SNew(SBox).Padding(FMargin(8.f, 2.f))
				[
					SNew(STextBlock).Text(Label)
				]
			];
	};
	auto Gap = []() { return SNew(SBox).WidthOverride(6.f); };

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(6.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("DeviceTooltip", "Screen the view is previewed on"))
				.OnGetMenuContent(this, &SVekaViewPreview::MakeDeviceMenu)
				.ButtonContent()
				[
					SNew(STextBlock).Text(this, &SVekaViewPreview::GetDeviceLabel)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Gap()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("DPITooltip", "UI scale. Auto follows the project's DPI curve (Project Settings > User Interface), like the game."))
				.OnGetMenuContent(this, &SVekaViewPreview::MakeDPIMenu)
				.ButtonContent()
				[
					SNew(STextBlock).Text(this, &SVekaViewPreview::GetDPILabel)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Gap()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Toggle(LOCTEXT("Light", "Light"), LOCTEXT("LightTooltip", "Light color scheme"), [this]() { return Settings.Scheme == EVekaColorScheme::Light; },
					[this](bool) { SetScheme(EVekaColorScheme::Light); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Toggle(LOCTEXT("Dark", "Dark"), LOCTEXT("DarkTooltip", "Dark color scheme"), [this]() { return Settings.Scheme == EVekaColorScheme::Dark; },
					[this](bool) { SetScheme(EVekaColorScheme::Dark); })
			]
			+ SHorizontalBox::Slot().AutoWidth()[Gap()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(220.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UVekaTheme::StaticClass())
					.AllowClear(true)
					.DisplayThumbnail(false)
					.ObjectPath_Lambda([this]() { return Settings.Theme.IsValid() ? Settings.Theme->GetPathName() : FString(); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetTheme(Cast<UVekaTheme>(Asset.GetAsset())); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Gap()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Toggle(LOCTEXT("Bounds", "Bounds"), LOCTEXT("BoundsTooltip", "Outline every view"), [this]() { return Settings.bShowBounds; }, [this](bool bOn) { SetShowBounds(bOn); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Toggle(LOCTEXT("Padding", "Padding"), LOCTEXT("PaddingTooltip", "Show padding areas"), [this]() { return Settings.bShowPadding; }, [this](bool bOn) { SetShowPadding(bOn); })
			]
		];
}

TSharedRef<SWidget> SVekaViewPreview::MakeDeviceMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const TArray<FVekaDevicePreset>& Presets = Veka::Editor::DevicePresets();
	for (int32 Index = 0; Index < Presets.Num(); ++Index)
	{
		const FText Label = FText::Format(LOCTEXT("DeviceEntry", "{0}  ({1} x {2})"), Presets[Index].Name, FText::AsNumber(Presets[Index].Size.X, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::AsNumber(Presets[Index].Size.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
		Menu.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Index]() { SetDevice(Index); }), FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Index]() { return Settings.Device == Index; })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SVekaViewPreview::MakeDPIMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const float Scales[] = { 0.f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 3.f };
	for (const float Scale : Scales)
	{
		const FText Label = Scale > 0.f ? FText::AsNumber(Scale) : LOCTEXT("DPIAutoEntry", "Auto (project DPI curve)");
		Menu.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Scale]() { SetDPIOverride(Scale); }), FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Scale]() { return FMath::IsNearlyEqual(Settings.DPIOverride, Scale); })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
	return Menu.MakeWidget();
}

void SVekaViewPreview::LoadSettings()
{
	int32 Device = 0;
	float DPI = 0.f;
	bool bLight = false;
	FString ThemePath;
	GConfig->GetInt(ConfigSection, TEXT("Device"), Device, GEditorPerProjectIni);
	GConfig->GetFloat(ConfigSection, TEXT("DPIOverride"), DPI, GEditorPerProjectIni);
	GConfig->GetBool(ConfigSection, TEXT("Light"), bLight, GEditorPerProjectIni);
	GConfig->GetBool(ConfigSection, TEXT("ShowBounds"), Settings.bShowBounds, GEditorPerProjectIni);
	GConfig->GetBool(ConfigSection, TEXT("ShowPadding"), Settings.bShowPadding, GEditorPerProjectIni);
	GConfig->GetString(ConfigSection, TEXT("Theme"), ThemePath, GEditorPerProjectIni);
	Settings.Device = FMath::Clamp(Device, 0, Veka::Editor::DevicePresets().Num() - 1);
	Settings.DPIOverride = FMath::Max(0.f, DPI);
	Settings.Scheme = bLight ? EVekaColorScheme::Light : EVekaColorScheme::Dark;
	Settings.Theme = ThemePath.IsEmpty() ? nullptr : LoadObject<UVekaTheme>(nullptr, *ThemePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void SVekaViewPreview::SaveSettings() const
{
	if (!bPersistSettings)
	{
		return;
	}
	GConfig->SetInt(ConfigSection, TEXT("Device"), Settings.Device, GEditorPerProjectIni);
	GConfig->SetFloat(ConfigSection, TEXT("DPIOverride"), Settings.DPIOverride, GEditorPerProjectIni);
	GConfig->SetBool(ConfigSection, TEXT("Light"), Settings.Scheme == EVekaColorScheme::Light, GEditorPerProjectIni);
	GConfig->SetBool(ConfigSection, TEXT("ShowBounds"), Settings.bShowBounds, GEditorPerProjectIni);
	GConfig->SetBool(ConfigSection, TEXT("ShowPadding"), Settings.bShowPadding, GEditorPerProjectIni);
	GConfig->SetString(ConfigSection, TEXT("Theme"), Settings.Theme.IsValid() ? *Settings.Theme->GetPathName() : TEXT(""), GEditorPerProjectIni);
}

#undef LOCTEXT_NAMESPACE
