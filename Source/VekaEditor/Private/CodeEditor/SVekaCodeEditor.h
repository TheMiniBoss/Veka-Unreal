// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "VekaLanguage.h"
#include "CodeEditor/VekaCodeAssist.h"

class UVekaView;
class SMultiLineEditableText;
class SScrollBar;
class SVerticalBox;
class FVekaCodeMarshaller;
class FActiveTimerHandle;

/**
 * Code tab of the Veka View editor (Advanced mode): the view as Veka code. Typing re-parses after a 150 ms pause; valid
 * code becomes the document (one undo step per pause, the viewport follows), invalid code stays in the editor with
 * its errors underlined and listed, and the document keeps its last valid version: no text is ever lost.
 * Completion (Ctrl+Space, or as you type `.`, `@`, names), Tab indents, Enter keeps the indentation, Format rewrites
 * the code the canonical way.
 */
class SVekaCodeEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaCodeEditor) : _View(nullptr) {}
		SLATE_ARGUMENT(UVekaView*, View)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVekaCodeEditor() override;

	/** The document changed outside the code (undo, Details, another tool). Reloads, unless the code has unapplied edits. */
	void OnDocumentChanged();
	/** Replaces the code with the document's (drops unapplied edits). */
	void ReloadFromDocument();

	FString GetText() const;
	/** Sets the code as if typed (parsed after the pause, or now with FlushParse). */
	void SetText(const FString& Text);
	/** Parses the code now and applies it if it is valid. */
	void FlushParse();
	/** Canonical layout of the code (only when it parses). */
	void Format();

	const TArray<FVekaDiagnostic>& GetDiagnostics() const { return Diagnostics; }
	bool HasErrors() const { return bTextHasErrors; }
	/** The document changed while the code had unapplied edits (the banner offers to reload). */
	bool HasPendingExternalChange() const { return bExternalChangePending; }

	/** Code shown for a view: its stored code if it still parses to the document, else the printed document. */
	static FString GetDisplayText(const UVekaView* View);

	/** Suggestions at the cursor (opens the popup if there are any). */
	void OpenCompletion(bool bExplicit);
	void CloseCompletion();
	bool IsCompletionOpen() const { return !CompletionItems.IsEmpty(); }
	const FVekaCompletion& GetCompletion() const { return Completion; }
	void AcceptCompletion(int32 Index);
	/** Moves the cursor (0-based line and column). */
	void GoTo(int32 Line, int32 Column);

private:
	void HandleTextChanged(const FText& Text);
	FReply HandleKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	EActiveTimerReturnType HandleParseTimer(double CurrentTime, float DeltaTime);
	void ScheduleCompletionRefresh(bool bAutomatic);
	void SetEditorText(const FString& Text);
	void RefreshHighlights();
	void RebuildDiagnosticList();
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FMargin GetCompletionPadding() const;
	TSharedRef<ITableRow> MakeCompletionRow(TSharedPtr<FVekaCompletionItem> Item, const TSharedRef<STableViewBase>& Owner);
	FVekaCompletionSources MakeSources() const;

	TWeakObjectPtr<UVekaView> View;
	TSharedPtr<SMultiLineEditableText> Editor;
	TSharedPtr<SScrollBar> VerticalScroll;
	TSharedPtr<FVekaCodeMarshaller> Marshaller;
	TSharedPtr<SVerticalBox> DiagnosticList;
	TSharedPtr<SListView<TSharedPtr<FVekaCompletionItem>>> CompletionList;
	TArray<TSharedPtr<FVekaCompletionItem>> CompletionItems;
	FVekaCompletion Completion;
	int32 CompletionLine = 0;
	TWeakPtr<FActiveTimerHandle> ParseTimer;
	TArray<FVekaDiagnostic> Diagnostics;
	TFunction<UVekaView*(FName)> Resolver;
	FSlateFontInfo Font;

	bool bSettingText = false;
	bool bApplying = false;
	bool bDirty = false;
	bool bTextHasErrors = false;
	bool bExternalChangePending = false;
	bool bAutoCompletionPending = false;
	bool bExplicitCompletionPending = false;
};
