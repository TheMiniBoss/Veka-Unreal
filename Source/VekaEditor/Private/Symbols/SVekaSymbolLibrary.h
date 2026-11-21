// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Views/SListView.h"

class SVekaHost;

namespace Veka::Editor::SymbolLibrary
{
	/** Tab id of the library (Window > Tools > Veka Symbol Library). */
	extern const FName TabId;
	/** Symbols whose name, keywords or category contain Query, in Category ("" / None = all). */
	TArray<FName> Filter(const FString& Query, FName Category);
	/** Categories of every symbol (registered and built-in), sorted. */
	TArray<FName> Categories();
}

/**
 * Symbol Library: every symbol (the project's and the built-in ones) by category, searched by name or keyword, with a
 * preview in every weight, rendering mode, scale and variant. Copy name puts `#"name"` in the clipboard.
 */
class SVekaSymbolLibrary : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaSymbolLibrary) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetQuery(const FString& InQuery);
	void SetCategory(FName InCategory);
	void SelectSymbol(FName Name);
	int32 GetShownCount() const { return Shown.Num(); }

private:
	void Refresh();
	void RefreshPreview();
	TSharedRef<ITableRow> MakeTile(TSharedPtr<FName> Name, const TSharedRef<STableViewBase>& Owner);

	TArray<TSharedPtr<FName>> Shown;
	TArray<TSharedPtr<FName>> CategoryItems;
	TSharedPtr<STileView<TSharedPtr<FName>>> Tiles;
	TSharedPtr<SListView<TSharedPtr<FName>>> CategoryList;
	TSharedPtr<SVekaHost> Preview;
	FString Query;
	FName Category;
	FName Selected;
};
