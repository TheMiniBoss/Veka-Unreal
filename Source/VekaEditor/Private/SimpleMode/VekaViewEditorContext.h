// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/VekaDocument.h"

class UVekaView;

/** Who changed the selection (a panel does not react to its own changes). */
enum class EVekaSelectionSource : uint8
{
	Outliner,
	Viewport,
	Code,
	Other
};

/**
 * State shared by the panels of one Veka View editor: the edited view, the selection (kept as node paths, so it
 * survives edits and re-parses) and the edit entry point. Simple mode panels are read-only while the code has errors.
 */
class FVekaViewEditorContext : public TSharedFromThis<FVekaViewEditorContext>
{
public:
	explicit FVekaViewEditorContext(UVekaView* InView) : View(InView) {}

	UVekaView* GetView() const { return View.Get(); }
	const FVekaDocument* GetDocument() const;
	/** "Veka.<view>": namespace of texts made in the editor (same as the parser's). */
	FString GetTextNamespace() const;

	TArray<int32> GetSelectedNodes() const;
	/** The last selected node, or INDEX_NONE. */
	int32 GetPrimarySelection() const;
	bool IsSelected(int32 Node) const;
	void Select(const TArray<int32>& Nodes, EVekaSelectionSource Source);
	void SelectPaths(const TArray<TArray<int32>>& Paths, EVekaSelectionSource Source);
	const TArray<TArray<int32>>& GetSelectedPaths() const { return SelectedPaths; }
	EVekaSelectionSource GetLastSelectionSource() const { return LastSource; }

	/**
	 * Changes the document as one undo step: Change edits a copy and returns the nodes to select afterwards (indices in
	 * the copy). The copy is compacted and applied (Veka::Editor::ApplyDocument). Refused while read-only.
	 */
	bool Edit(const FText& Description, TFunctionRef<TArray<int32>(FVekaDocument&)> Change);

	/** Read-only (code with errors): the Simple mode would otherwise overwrite text being written. */
	bool IsReadOnly() const { return ReadOnly ? ReadOnly() : false; }
	TFunction<bool()> ReadOnly;

	/** Broadcast by the editor after any change of the document (edits, undo, code, Details). */
	FSimpleMulticastDelegate OnDocumentChanged;
	FSimpleMulticastDelegate OnSelectionChanged;

	/** The document changed: drops selected paths that no longer exist, then broadcasts. */
	void NotifyDocumentChanged();

private:
	TWeakObjectPtr<UVekaView> View;
	TArray<TArray<int32>> SelectedPaths;
	EVekaSelectionSource LastSource = EVekaSelectionSource::Other;
};
