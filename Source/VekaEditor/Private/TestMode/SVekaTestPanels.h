// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FVekaViewEditorContext;
class SVekaViewPreview;
class SVerticalBox;
class IDetailsView;
class SVekaTimelineLanes;
class FVekaTimelinePlayer;
struct FVekaPreviewLogEntry;
struct FVekaDocumentTimeline;

/**
 * Test mode (3e): use the view in the viewport without PIE (clicks, hover, focus), watch and set its states, read the
 * log of emitted events and state changes, and edit the preview model's mock values.
 */
class SVekaTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaTestPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, const TSharedRef<SVekaViewPreview>& InPreview);
	virtual ~SVekaTestPanel() override;

	/** Viewport clicks use the view (true) or select (false). */
	void SetPlaying(bool bPlaying);
	bool IsPlaying() const;

private:
	void RebuildStates();
	void RefreshLog();
	TSharedRef<ITableRow> MakeLogRow(TSharedPtr<FVekaPreviewLogEntry> Entry, const TSharedRef<STableViewBase>& Owner);

	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<SVekaViewPreview> Preview;
	TSharedPtr<SVerticalBox> States;
	TSharedPtr<SListView<TSharedPtr<FVekaPreviewLogEntry>>> LogList;
	TArray<TSharedPtr<FVekaPreviewLogEntry>> LogItems;
	TSharedPtr<IDetailsView> ModelDetails;
	FDelegateHandle LogHandle;
	FDelegateHandle RefreshHandle;
};

/**
 * Timeline tab (3e): the view's timelines — tracks, keyframes (double-click a lane to add, drag to move, Delete),
 * curves, settings — played, paused, reversed and scrubbed live in the viewport.
 */
class SVekaTimelinePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaTimelinePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, const TSharedRef<SVekaViewPreview>& InPreview);
	virtual ~SVekaTimelinePanel() override;

	// Edits (one undo step each), also used by tests.
	FName AddTimeline();
	void SelectTimeline(FName Name);
	FName GetSelectedTimeline() const { return Selected; }
	FName AddTrack();
	void AddKey(int32 Track, float Time);
	void MoveKey(int32 Track, int32 Key, float Time);
	void SetKeyValue(int32 Track, int32 Key, float Value);
	void DeleteKey(int32 Track, int32 Key);

	/** Player of the selected timeline in the viewport. */
	TSharedPtr<FVekaTimelinePlayer> GetPlayer() const;
	void Scrub(float Time);

private:
	const FVekaDocumentTimeline* GetTimeline() const;
	int32 TimelineIndex() const;
	void Rebuild();
	bool EditTimeline(const FText& Description, TFunction<void(FVekaDocumentTimeline&)> Change);
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeTransport();
	TSharedRef<SWidget> MakeKeyEditor();

	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<SVekaViewPreview> Preview;
	TSharedPtr<SVerticalBox> Content;
	TSharedPtr<SVekaTimelineLanes> Lanes;
	FName Selected;
	int32 SelectedTrack = INDEX_NONE;
	int32 SelectedKey = INDEX_NONE;
	FDelegateHandle DocumentHandle;
};
