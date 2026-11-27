// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SListView.h"
#include "Input/DragAndDrop.h"

class FVekaViewEditorContext;
class UVekaView;

/** Dragging outliner nodes (move) or a palette entry (insert). */
class FVekaNodeDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FVekaNodeDragDrop, FDragDropOperation)

	/** Existing nodes being moved. */
	TArray<int32> Nodes;
	/** Or a new node: a catalog type, "if", "ForEach", "Slot", or a view of the project. */
	FName NewType;
	TWeakObjectPtr<UVekaView> NewComponent;
	FText Label;

	static TSharedRef<FVekaNodeDragDrop> New(const TArray<int32>& Nodes, FName NewType, UVekaView* NewComponent, const FText& Label);
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};

struct FVekaOutlinerItem
{
	int32 Node = INDEX_NONE;
	/** "else" group of the If node Node. */
	bool bElseGroup = false;
	TArray<TSharedPtr<FVekaOutlinerItem>> Children;
};

namespace Veka::Editor
{
	/** Inserts a new node where the selection suggests: inside a selected container, after a selected node, else in the root. */
	bool InsertAtSelection(FVekaViewEditorContext& Context, FName Type, UVekaView* Component);
	/** Drop of nodes / a new node on an outliner item (move or insert). */
	bool DropOnItem(FVekaViewEditorContext& Context, const FVekaNodeDragDrop& Operation, const FVekaOutlinerItem& Target, EItemDropZone Zone);
}

/**
 * Outliner of the Simple mode: the view's tree. Drag to move (or drop palette entries), double-click or F2 to rename
 * (label = `// comment`), multi-selection, Delete, Ctrl+D duplicate, right-click to add / wrap / duplicate / delete.
 */
class SVekaOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaOutliner) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext);
	virtual ~SVekaOutliner() override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void Rebuild();
	int32 GetItemCount() const;

private:
	TSharedRef<ITableRow> MakeRow(TSharedPtr<FVekaOutlinerItem> Item, const TSharedRef<STableViewBase>& Owner);
	void GetItemChildren(TSharedPtr<FVekaOutlinerItem> Item, TArray<TSharedPtr<FVekaOutlinerItem>>& OutChildren);
	void HandleTreeSelection(TSharedPtr<FVekaOutlinerItem> Item, ESelectInfo::Type Info);
	void HandleContextSelection();
	TSharedPtr<SWidget> MakeContextMenu();
	void DeleteSelection();
	void DuplicateSelection();
	void WrapSelection(FName Container);
	TSharedPtr<FVekaOutlinerItem> FindItem(int32 Node) const;

	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<STreeView<TSharedPtr<FVekaOutlinerItem>>> Tree;
	TArray<TSharedPtr<FVekaOutlinerItem>> RootItems;
	TMap<int32, TSharedPtr<FVekaOutlinerItem>> ItemsByNode;
	TSet<FString> Collapsed;
	bool bSyncing = false;
	FDelegateHandle DocumentHandle;
	FDelegateHandle SelectionHandle;
};

struct FVekaPaletteEntry
{
	FName Type;
	FText Label;
	/** A symbol of the built-in library. */
	FName Icon;
	FLinearColor IconColor = FLinearColor::White;
	FText Category;
	TWeakObjectPtr<UVekaView> Component;
	/** Parameters and slots of a component view. */
	FText Tooltip;
	bool bHeader = false;
};

/** Palette of the Simple mode: components, logic and the project's views. Drag onto the outliner, or double-click. */
class SVekaPalette : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaPalette) : _PickOnClick(false) {}
		/** One click adds the entry and closes the menu it is in (the Outliner's "+ Add" popup); otherwise double-click or drag. */
		SLATE_ARGUMENT(bool, PickOnClick)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext);
	virtual ~SVekaPalette() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Every entry (catalog, then component libraries, then the other views of the project), without headers. */
	static TArray<FVekaPaletteEntry> GatherEntries(const UVekaView* Exclude);

private:
	void Refresh();
	TSharedRef<ITableRow> MakeRow(TSharedPtr<FVekaPaletteEntry> Entry, const TSharedRef<STableViewBase>& Owner);

	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<SListView<TSharedPtr<FVekaPaletteEntry>>> List;
	TArray<TSharedPtr<FVekaPaletteEntry>> Entries;
	FString Filter;
	bool bPickOnClick = false;
	/** Views added, removed, renamed or changed, or a library changed: the palette is gathered again. */
	bool bStale = false;
	uint32 LibraryRevision = 0;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;
	FDelegateHandle ChangedHandle;
};
