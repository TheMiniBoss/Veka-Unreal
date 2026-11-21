// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "Core/VekaTypes.h"
#include "View/VekaValue.h"

class SVekaHost;
class SVekaPreviewCanvas;
class FVekaViewStore;
class UVekaView;
class UVekaViewModel;
class UVekaTheme;
class FVekaViewEditorContext;

/** A screen the view is previewed on. */
struct FVekaDevicePreset
{
	FText Name;
	/** Pixels. */
	FIntPoint Size;
};

/** A line of the Test mode log: an emitted event or a state change. */
struct FVekaPreviewLogEntry
{
	/** Seconds since the viewport opened. */
	double Time = 0.0;
	FString Text;
	bool bEvent = false;
};

/** What the viewport shows (kept between sessions in EditorPerProjectUserSettings). */
struct FVekaPreviewSettings
{
	int32 Device = 0;
	/** 0 = the project's DPI curve (Project Settings > User Interface), as the game would scale it. */
	float DPIOverride = 0.f;
	EVekaColorScheme Scheme = EVekaColorScheme::Dark;
	TWeakObjectPtr<UVekaTheme> Theme;
	bool bShowBounds = false;
	bool bShowPadding = false;
	/** Clicks select views (Simple mode) instead of using them. */
	bool bSelectMode = true;
};

namespace Veka::Editor
{
	/** 1080p, 1440p, 4K, ultrawide, mobile. */
	const TArray<FVekaDevicePreset>& DevicePresets();
	/** DPI scale the game applies to a screen of this size (project DPI curve). */
	float ProjectDPIScale(FIntPoint Size);
}

/**
 * Viewport of the Veka View editor: the view running on a device screen (resolution, DPI), in light or dark with a
 * Colora theme, zoomable and pannable (wheel, right or middle drag, F to fit), with bounds / padding overlays. The view
 * is live: buttons, toggles and states work; events it emits show in the status bar.
 */
class SVekaViewPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaViewPreview) : _View(nullptr), _bPersistSettings(true) {}
		SLATE_ARGUMENT(UVekaView*, View)
		/** Loads / saves the settings in the user config (off in tests). */
		SLATE_ARGUMENT(bool, bPersistSettings)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVekaViewPreview() override;

	/** Rebuilds the view from the asset (document or preview model changed). State values are kept. */
	void Refresh();
	/** Back to the declared initial states. */
	void ResetState();

	void SetDevice(int32 Index);
	void SetDPIOverride(float Scale);
	void SetScheme(EVekaColorScheme Scheme);
	void SetTheme(UVekaTheme* Theme);
	void SetShowBounds(bool bShow);
	void SetShowPadding(bool bShow);
	void SetSelectMode(bool bSelect);
	/** Selection shared with the outliner and the code (3d). */
	void SetContext(const TSharedPtr<FVekaViewEditorContext>& InContext);
	/** Document node under a point of the screen (host space), or INDEX_NONE. */
	int32 PickNode(const FVector2D& HostPosition) const;
	void ZoomToFit();

	const FVekaPreviewSettings& GetSettings() const { return Settings; }
	FIntPoint GetDeviceSize() const;
	float GetDPIScale() const;
	/** Size the view is laid out in (device pixels / DPI scale). */
	FVector2D GetLogicalSize() const;
	float GetZoom() const;
	TSharedPtr<SVekaHost> GetHost() const { return Host; }
	TSharedPtr<FVekaViewStore> GetStore() const { return Store; }
	UVekaViewModel* GetModel() const { return Model.Get(); }
	int32 GetRefreshCount() const { return RefreshCount; }
	/** Overlay rectangles drawn in the last paint (bounds + padding strips). */
	int32 GetOverlayRectCount() const;

	/** Events emitted and states changed while using the view (Test mode). */
	const TArray<FVekaPreviewLogEntry>& GetLog() const { return Log; }
	void ClearLog();
	FSimpleMulticastDelegate OnLogChanged;
	/** The view was rebuilt (new store: states, timelines). */
	FSimpleMulticastDelegate OnRefreshed;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeDeviceMenu();
	TSharedRef<SWidget> MakeDPIMenu();
	FText GetDeviceLabel() const;
	FText GetDPILabel() const;
	FText GetStatusText() const;
	FSlateColor GetScreenColor() const;
	void ApplyEnvironment();
	void ApplyDevice();
	void LoadSettings();
	void SaveSettings() const;

	TWeakObjectPtr<UVekaView> View;
	TStrongObjectPtr<UVekaViewModel> Model;
	TSharedPtr<FVekaViewStore> Store;
	TSharedPtr<SVekaHost> Host;
	TSharedPtr<SVekaPreviewCanvas> Canvas;
	TSharedPtr<class SVekaPreviewOverlay> Overlay;
	TSharedPtr<FVekaViewEditorContext> Context;
	FDelegateHandle SelectionHandle;
	FVekaPreviewSettings Settings;
	bool bPersistSettings = true;
	int32 RefreshCount = 0;
	FText LastEvent;
	TArray<FVekaPreviewLogEntry> Log;
	double StartTime = 0.0;
	void AddLog(const FString& Text, bool bEvent);
	/** Declared initial values at the last refresh (a state still equal to it follows edits of the declaration). */
	TMap<FName, FVekaValue> DeclaredInitials;
};
