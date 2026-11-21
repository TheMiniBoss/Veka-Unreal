// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "View/VekaViewAsset.h"

const FVekaDocument* FVekaViewEditorContext::GetDocument() const
{
	const UVekaView* Asset = View.Get();
	return Asset ? &Asset->Document : nullptr;
}

FString FVekaViewEditorContext::GetTextNamespace() const
{
	const UVekaView* Asset = View.Get();
	return Asset ? TEXT("Veka.") + Asset->GetName() : FString(TEXT("Veka"));
}

TArray<int32> FVekaViewEditorContext::GetSelectedNodes() const
{
	TArray<int32> Out;
	if (const FVekaDocument* Document = GetDocument())
	{
		for (const TArray<int32>& Path : SelectedPaths)
		{
			const int32 Node = Veka::Editor::Doc::FromPath(*Document, Path);
			if (Node != INDEX_NONE)
			{
				Out.AddUnique(Node);
			}
		}
	}
	return Out;
}

int32 FVekaViewEditorContext::GetPrimarySelection() const
{
	const TArray<int32> Nodes = GetSelectedNodes();
	return Nodes.IsEmpty() ? INDEX_NONE : Nodes.Last();
}

bool FVekaViewEditorContext::IsSelected(int32 Node) const
{
	return GetSelectedNodes().Contains(Node);
}

void FVekaViewEditorContext::Select(const TArray<int32>& Nodes, EVekaSelectionSource Source)
{
	TArray<TArray<int32>> Paths;
	if (const FVekaDocument* Document = GetDocument())
	{
		for (const int32 Node : Nodes)
		{
			if (Document->Nodes.IsValidIndex(Node))
			{
				Paths.Add(Veka::Editor::Doc::Path(*Document, Node));
			}
		}
	}
	SelectPaths(Paths, Source);
}

void FVekaViewEditorContext::SelectPaths(const TArray<TArray<int32>>& Paths, EVekaSelectionSource Source)
{
	if (Paths == SelectedPaths)
	{
		return;
	}
	SelectedPaths = Paths;
	LastSource = Source;
	OnSelectionChanged.Broadcast();
}

bool FVekaViewEditorContext::Edit(const FText& Description, TFunctionRef<TArray<int32>(FVekaDocument&)> Change)
{
	UVekaView* Asset = View.Get();
	if (!Asset || IsReadOnly())
	{
		return false;
	}
	FVekaDocument Copy = Asset->Document;
	const TArray<int32> ToSelect = Change(Copy);
	// Paths are taken before compacting: compacting keeps the tree, only the flat arrays move.
	TArray<TArray<int32>> Paths;
	for (const int32 Node : ToSelect)
	{
		if (Copy.Nodes.IsValidIndex(Node))
		{
			Paths.Add(Veka::Editor::Doc::Path(Copy, Node));
		}
	}
	Veka::Editor::Doc::Compact(Copy);
	Veka::Editor::ApplyDocument(Asset, Copy, Description);
	if (!ToSelect.IsEmpty() || !Paths.IsEmpty())
	{
		SelectPaths(Paths, EVekaSelectionSource::Other);
	}
	return true;
}

void FVekaViewEditorContext::NotifyDocumentChanged()
{
	if (const FVekaDocument* Document = GetDocument())
	{
		const int32 Before = SelectedPaths.Num();
		SelectedPaths.RemoveAll([Document](const TArray<int32>& Path) { return Veka::Editor::Doc::FromPath(*Document, Path) == INDEX_NONE; });
		if (SelectedPaths.Num() != Before)
		{
			OnSelectionChanged.Broadcast();
		}
	}
	OnDocumentChanged.Broadcast();
}
