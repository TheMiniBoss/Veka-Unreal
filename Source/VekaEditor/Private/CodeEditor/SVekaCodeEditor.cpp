// Copyright TheMiniBoss. All Rights Reserved.

#include "CodeEditor/SVekaCodeEditor.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "View/VekaViewAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"
#include "Framework/Text/ISlateLineHighlighter.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VekaCodeEditor"

namespace
{
	constexpr float ParseDelay = 0.15f;
	constexpr float TextMargin = 6.f;

	/** Red wavy line under an error range. */
	class FVekaSquiggle : public ISlateLineHighlighter
	{
	public:
		FLinearColor Color = FLinearColor(1.f, 0.25f, 0.25f);

		virtual int32 OnPaint(const FPaintArgs& Args, const FTextLayout::FLineView& Line, const FVector2D Offset, const float Width, const FTextBlockStyle& DefaultStyle,
			const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			// Line offsets are in layout (scaled) units, like the engine's own highlighters.
			const float InverseScale = AllottedGeometry.Scale > 0.f ? 1.f / AllottedGeometry.Scale : 1.f;
			const float Left = (Line.Offset.X + Offset.X) * InverseScale;
			const float Bottom = (Line.Offset.Y + Line.TextHeight) * InverseScale - 1.5f;
			const float Right = Left + FMath::Max(Width * InverseScale, 4.f);
			TArray<FVector2f> Points;
			bool bUp = true;
			for (float X = Left; X <= Right; X += 2.f)
			{
				Points.Add(FVector2f(X, Bottom + (bUp ? -1.f : 1.f)));
				bUp = !bUp;
			}
			if (Points.Num() >= 2)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 1.f);
			}
			return LayerId;
		}
	};
}

/** Colors the code (Veka::Editor::TokenizeLine) and underlines the diagnostics. */
class FVekaCodeMarshaller : public FBaseTextLayoutMarshaller
{
public:
	explicit FVekaCodeMarshaller(const FSlateFontInfo& Font)
	{
		Base = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
		Base.SetFont(Font);
		Base.SetColorAndOpacity(FLinearColor(0.84f, 0.85f, 0.88f));
		auto Style = [this](const FLinearColor& Color)
		{
			FTextBlockStyle Out = Base;
			Out.SetColorAndOpacity(Color);
			return Out;
		};
		Styles.Add(EVekaCodeToken::Keyword, Style(FLinearColor(0.80f, 0.47f, 0.95f)));
		Styles.Add(EVekaCodeToken::Declaration, Style(FLinearColor(0.98f, 0.62f, 0.30f)));
		Styles.Add(EVekaCodeToken::Component, Style(FLinearColor(0.35f, 0.82f, 0.78f)));
		Styles.Add(EVekaCodeToken::Modifier, Style(FLinearColor(0.45f, 0.65f, 1.f)));
		Styles.Add(EVekaCodeToken::EnumValue, Style(FLinearColor(0.95f, 0.55f, 0.70f)));
		Styles.Add(EVekaCodeToken::String, Style(FLinearColor(0.98f, 0.45f, 0.38f)));
		Styles.Add(EVekaCodeToken::Number, Style(FLinearColor(0.82f, 0.78f, 0.45f)));
		Styles.Add(EVekaCodeToken::Color, Style(FLinearColor(0.82f, 0.78f, 0.45f)));
		Styles.Add(EVekaCodeToken::Comment, Style(FLinearColor(0.45f, 0.48f, 0.52f)));
		Squiggle = MakeShared<FVekaSquiggle>();
		Warning = MakeShared<FVekaSquiggle>();
		Warning->Color = FLinearColor(1.f, 0.75f, 0.2f);
	}

	void SetDiagnostics(const TArray<FVekaDiagnostic>& InDiagnostics) { Diagnostics = InDiagnostics; }

	virtual void SetText(const FString& Source, FTextLayout& Layout) override
	{
		TArray<FTextRange> Ranges;
		FTextRange::CalculateLineRangesFromString(Source, Ranges);
		TArray<FTextLayout::FNewLineData> Lines;
		Lines.Reserve(Ranges.Num());
		for (const FTextRange& Range : Ranges)
		{
			const TSharedRef<FString> LineText = MakeShared<FString>(Source.Mid(Range.BeginIndex, Range.Len()));
			TArray<TSharedRef<IRun>> Runs;
			int32 At = 0;
			for (const FVekaCodeSpan& Span : Veka::Editor::TokenizeLine(*LineText))
			{
				if (Span.Start > At)
				{
					Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, Base, FTextRange(At, Span.Start)));
				}
				const FTextBlockStyle* Style = Styles.Find(Span.Kind);
				Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, Style ? *Style : Base, FTextRange(Span.Start, Span.Start + Span.Length)));
				At = Span.Start + Span.Length;
			}
			if (At < LineText->Len() || Runs.IsEmpty())
			{
				Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, Base, FTextRange(At, LineText->Len())));
			}
			Lines.Emplace(LineText, MoveTemp(Runs));
		}
		Layout.AddLines(Lines);

		for (const FVekaDiagnostic& Diagnostic : Diagnostics)
		{
			const int32 LineIndex = Diagnostic.Range.Line - 1;
			if (!Ranges.IsValidIndex(LineIndex))
			{
				continue;
			}
			const int32 LineLength = Ranges[LineIndex].Len();
			if (LineLength == 0)
			{
				continue;
			}
			const int32 Begin = FMath::Clamp(Diagnostic.Range.Column - 1, 0, LineLength - 1);
			const int32 End = FMath::Clamp(Begin + FMath::Max(Diagnostic.Range.Length, 1), Begin + 1, LineLength);
			const bool bError = Diagnostic.Severity == EVekaDiagnosticSeverity::Error;
			Layout.AddLineHighlight(FTextLineHighlight(LineIndex, FTextRange(Begin, End), 10, bError ? Squiggle.ToSharedRef() : Warning.ToSharedRef()));
		}
	}

	virtual void GetText(FString& Target, const FTextLayout& Layout) override
	{
		Layout.GetAsText(Target);
	}

private:
	FTextBlockStyle Base;
	TMap<EVekaCodeToken, FTextBlockStyle> Styles;
	TSharedPtr<FVekaSquiggle> Squiggle;
	TSharedPtr<FVekaSquiggle> Warning;
	TArray<FVekaDiagnostic> Diagnostics;
};

void SVekaCodeEditor::Construct(const FArguments& InArgs)
{
	View = InArgs._View;
	Font = FCoreStyle::GetDefaultFontStyle("Mono", 10);
	Marshaller = MakeShared<FVekaCodeMarshaller>(Font);
	Resolver = Veka::Lang::MakeAssetResolver();
	VerticalScroll = SNew(SScrollBar).Orientation(Orient_Vertical);
	TSharedRef<SScrollBar> HorizontalScroll = SNew(SScrollBar).Orientation(Orient_Horizontal);

	ChildSlot
	[
		SNew(SVerticalBox)
		// Status and tools.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("FormatTooltip", "Rewrite the code the canonical way (indentation, one modifier per line)"))
					.OnClicked_Lambda([this]() { Format(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("Format", "Format"))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f))
				[
					SNew(STextBlock)
					.Text(this, &SVekaCodeEditor::GetStatusText)
					.ColorAndOpacity(this, &SVekaCodeEditor::GetStatusColor)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return bExternalChangePending ? EVisibility::Visible : EVisibility::Collapsed; })
					.ToolTipText(LOCTEXT("ReloadTooltip", "Replace the code with the current document (your unapplied edits are dropped)"))
					.OnClicked_Lambda([this]() { ReloadFromDocument(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("Reload", "Reload from document"))
					]
				]
			]
		]
		// The code.
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.012f, 0.013f, 0.016f))
				.Padding(0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SAssignNew(Editor, SMultiLineEditableText)
							.Marshaller(Marshaller)
							.Font(Font)
							.Margin(FMargin(TextMargin))
							.AutoWrapText(false)
							.VScrollBar(VerticalScroll)
							.HScrollBar(HorizontalScroll)
							.OnTextChanged(this, &SVekaCodeEditor::HandleTextChanged)
							.OnKeyDownHandler(this, &SVekaCodeEditor::HandleKeyDown)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							HorizontalScroll
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						VerticalScroll.ToSharedRef()
					]
				]
			]
			// Completion popup, at the cursor.
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(TAttribute<FMargin>(this, &SVekaCodeEditor::GetCompletionPadding))
			[
				SNew(SBorder)
				.Visibility_Lambda([this]() { return IsCompletionOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				.Padding(2.f)
				[
					SNew(SBox)
					.WidthOverride(340.f)
					.MaxDesiredHeight(240.f)
					[
						SAssignNew(CompletionList, SListView<TSharedPtr<FVekaCompletionItem>>)
						.ListItemsSource(&CompletionItems)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SVekaCodeEditor::MakeCompletionRow)
						.OnMouseButtonDoubleClick_Lambda([this](TSharedPtr<FVekaCompletionItem> Item) { AcceptCompletion(CompletionItems.IndexOfByKey(Item)); })
					]
				]
			]
		]
		// Errors and warnings (click to jump).
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.MaxDesiredHeight(120.f)
			.Visibility_Lambda([this]() { return Diagnostics.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(6.f, 3.f))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(DiagnosticList, SVerticalBox)
					]
				]
			]
		]
	];

	ReloadFromDocument();
}

SVekaCodeEditor::~SVekaCodeEditor()
{
}

FString SVekaCodeEditor::GetDisplayText(const UVekaView* InView)
{
	if (!InView)
	{
		return FString();
	}
#if WITH_EDITORONLY_DATA
	if (!InView->SourceText.IsEmpty())
	{
		FVekaParseOptions Options;
		Options.ResolveComponent = Veka::Lang::MakeAssetResolver();
		const FVekaParseResult Stored = Veka::Lang::Parse(InView->SourceText, Options);
		if (!Stored.HasErrors() && Veka::Lang::StructurallyEqual(Stored.Document, InView->Document))
		{
			return InView->SourceText;
		}
	}
#endif
	return Veka::Lang::Print(InView->Document, InView->GetFName());
}

void SVekaCodeEditor::OnDocumentChanged()
{
	if (bApplying)
	{
		return;
	}
	if (bDirty || bTextHasErrors)
	{
		// Never replace code the user is writing; the banner offers to reload.
		bExternalChangePending = true;
		return;
	}
	ReloadFromDocument();
}

void SVekaCodeEditor::ReloadFromDocument()
{
	const FTextLocation Cursor = Editor->GetCursorLocation();
	SetEditorText(GetDisplayText(View.Get()));
	bDirty = false;
	bTextHasErrors = false;
	bExternalChangePending = false;
	Diagnostics.Reset();
	if (const UVekaView* Asset = View.Get())
	{
		for (const FVekaDiagnostic& Diagnostic : Veka::Lang::Analyze(Asset->Document))
		{
			if (Diagnostic.Severity == EVekaDiagnosticSeverity::Warning)
			{
				Diagnostics.Add(Diagnostic);
			}
		}
	}
	RefreshHighlights();
	if (Cursor.IsValid())
	{
		Editor->GoTo(Cursor);
	}
}

FString SVekaCodeEditor::GetText() const
{
	return Editor->GetText().ToString();
}

void SVekaCodeEditor::SetText(const FString& Text)
{
	SetEditorText(Text);
	HandleTextChanged(FText::FromString(Text));
}

void SVekaCodeEditor::SetEditorText(const FString& Text)
{
	TGuardValue<bool> Guard(bSettingText, true);
	FString Normalized = Text;
	Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Editor->SetText(FText::FromString(Normalized));
}

void SVekaCodeEditor::HandleTextChanged(const FText& Text)
{
	if (bSettingText)
	{
		return;
	}
	bDirty = true;
	if (const TSharedPtr<FActiveTimerHandle> Timer = ParseTimer.Pin())
	{
		UnRegisterActiveTimer(Timer.ToSharedRef());
	}
	ParseTimer = RegisterActiveTimer(ParseDelay, FWidgetActiveTimerDelegate::CreateSP(this, &SVekaCodeEditor::HandleParseTimer));
	ScheduleCompletionRefresh(true);
}

EActiveTimerReturnType SVekaCodeEditor::HandleParseTimer(double CurrentTime, float DeltaTime)
{
	ParseTimer.Reset();
	FlushParse();
	return EActiveTimerReturnType::Stop;
}

void SVekaCodeEditor::FlushParse()
{
	if (const TSharedPtr<FActiveTimerHandle> Timer = ParseTimer.Pin())
	{
		UnRegisterActiveTimer(Timer.ToSharedRef());
		ParseTimer.Reset();
	}
	UVekaView* Asset = View.Get();
	if (!Asset)
	{
		return;
	}
	const FString Text = GetText();
	FVekaParseOptions Options;
	Options.ResolveComponent = Resolver;
	const FVekaParseResult Result = Veka::Lang::Parse(Text, Options);
	Diagnostics = Result.Diagnostics;
	bool bErrors = Result.HasErrors();
	if (!bErrors)
	{
		Diagnostics.Append(Veka::Lang::Analyze(Result.Document));
		bErrors = Diagnostics.ContainsByPredicate([](const FVekaDiagnostic& Diagnostic) { return Diagnostic.Severity == EVekaDiagnosticSeverity::Error; });
	}
	if (!Result.ViewName.IsNone() && Result.ViewName != Asset->GetFName())
	{
		FVekaDiagnostic& Name = Diagnostics.AddDefaulted_GetRef();
		Name.Severity = EVekaDiagnosticSeverity::Warning;
		Name.Message = FText::Format(LOCTEXT("ViewName", "The view is named {0} here but the asset is {1} (components use the asset name)"),
			FText::FromName(Result.ViewName), FText::FromName(Asset->GetFName()));
		Name.Range.Line = 1;
		Name.Range.Column = 1;
	}
	bDirty = false;
	bTextHasErrors = bErrors;
	if (!bErrors)
	{
		if (!Veka::Lang::StructurallyEqual(Result.Document, Asset->Document))
		{
			TGuardValue<bool> Guard(bApplying, true);
			Veka::Editor::ApplyDocument(Asset, Result.Document, LOCTEXT("EditCode", "Edit Veka code"), &Text);
		}
#if WITH_EDITORONLY_DATA
		else if (Asset->SourceText != Text)
		{
			// Layout only (spaces, blank lines): kept with the asset, nothing to undo in the document.
			Asset->SourceText = Text;
			Asset->MarkPackageDirty();
		}
#endif
		bExternalChangePending = false;
	}
	RefreshHighlights();
}

void SVekaCodeEditor::Format()
{
	FVekaParseOptions Options;
	Options.ResolveComponent = Resolver;
	const FVekaParseResult Result = Veka::Lang::Parse(GetText(), Options);
	if (Result.HasErrors())
	{
		FlushParse();
		return;
	}
	const FTextLocation Cursor = Editor->GetCursorLocation();
	SetText(Veka::Lang::Print(Result.Document, Result.ViewName.IsNone() && View.IsValid() ? View->GetFName() : Result.ViewName));
	FlushParse();
	if (Cursor.IsValid())
	{
		Editor->GoTo(FTextLocation(Cursor.GetLineIndex(), 0));
	}
}

void SVekaCodeEditor::RefreshHighlights()
{
	Marshaller->SetDiagnostics(Diagnostics);
	Marshaller->MakeDirty();
	{
		TGuardValue<bool> Guard(bSettingText, true);
		Editor->Refresh();
	}
	RebuildDiagnosticList();
}

void SVekaCodeEditor::RebuildDiagnosticList()
{
	DiagnosticList->ClearChildren();
	for (const FVekaDiagnostic& Diagnostic : Diagnostics)
	{
		const bool bError = Diagnostic.Severity == EVekaDiagnosticSeverity::Error;
		const int32 Line = Diagnostic.Range.Line - 1;
		const int32 Column = Diagnostic.Range.Column - 1;
		DiagnosticList->AddSlot().AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.OnClicked_Lambda([this, Line, Column]() { GoTo(Line, Column); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(bError ? FLinearColor(1.f, 0.4f, 0.4f) : FLinearColor(1.f, 0.78f, 0.3f))
				.Text(FText::FromString(Diagnostic.ToString()))
			]
		];
	}
}

void SVekaCodeEditor::GoTo(int32 Line, int32 Column)
{
	const FTextLocation Location(FMath::Max(0, Line), FMath::Max(0, Column));
	Editor->GoTo(Location);
	Editor->ScrollTo(Location);
	FSlateApplication::Get().SetKeyboardFocus(Editor, EFocusCause::SetDirectly);
}

FText SVekaCodeEditor::GetStatusText() const
{
	const int32 Errors = Diagnostics.FilterByPredicate([](const FVekaDiagnostic& Diagnostic) { return Diagnostic.Severity == EVekaDiagnosticSeverity::Error; }).Num();
	if (bExternalChangePending)
	{
		return LOCTEXT("StatusExternal", "The document changed (undo or another tool) while the code has edits not applied");
	}
	if (bDirty)
	{
		return LOCTEXT("StatusTyping", "...");
	}
	if (Errors > 0)
	{
		return FText::Format(LOCTEXT("StatusErrors", "{0} {0}|plural(one=error,other=errors) — the view keeps its last valid version"), FText::AsNumber(Errors));
	}
	return LOCTEXT("StatusOk", "Applied");
}

FSlateColor SVekaCodeEditor::GetStatusColor() const
{
	if (bExternalChangePending || bTextHasErrors)
	{
		return FSlateColor(FLinearColor(1.f, 0.5f, 0.4f));
	}
	return FSlateColor(FLinearColor(0.5f, 0.8f, 0.55f));
}

FVekaCompletionSources SVekaCodeEditor::MakeSources() const
{
	FVekaCompletionSources Sources;
	const UVekaView* Asset = View.Get();
	Sources.Document = Asset ? &Asset->Document : nullptr;
#if WITH_EDITORONLY_DATA
	Sources.ModelClass = Asset ? Asset->PreviewModelClass.Get() : nullptr;
#endif
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UVekaView::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Data : Assets)
	{
		if (!Asset || Data.AssetName != Asset->GetFName())
		{
			Sources.ProjectViews.AddUnique(Data.AssetName);
		}
	}
	return Sources;
}

void SVekaCodeEditor::ScheduleCompletionRefresh(bool bAutomatic)
{
	// After the text box has moved its cursor (next frame).
	if (bAutoCompletionPending || bExplicitCompletionPending)
	{
		bExplicitCompletionPending |= !bAutomatic;
		return;
	}
	bAutoCompletionPending = bAutomatic;
	bExplicitCompletionPending = !bAutomatic;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		const bool bExplicit = bExplicitCompletionPending;
		bAutoCompletionPending = false;
		bExplicitCompletionPending = false;
		if (bExplicit || IsCompletionOpen())
		{
			OpenCompletion(bExplicit);
		}
		else
		{
			// Open by itself after `.`, `@`, inside a string being a symbol, or two letters of a name.
			const FTextLocation Cursor = Editor->GetCursorLocation();
			TArray<FString> Lines;
			GetText().ParseIntoArray(Lines, TEXT("\n"), false);
			if (Lines.IsValidIndex(Cursor.GetLineIndex()) && Cursor.GetOffset() > 0)
			{
				const FString& Line = Lines[Cursor.GetLineIndex()];
				const int32 Offset = FMath::Min(Cursor.GetOffset(), Line.Len());
				const TCHAR Typed = Offset > 0 ? Line[Offset - 1] : 0;
				const bool bWord = Offset >= 2 && FChar::IsAlpha(Line[Offset - 1]) && FChar::IsAlpha(Line[Offset - 2]);
				if (Typed == TEXT('.') || Typed == TEXT('@') || bWord)
				{
					OpenCompletion(false);
				}
			}
		}
		return EActiveTimerReturnType::Stop;
	}));
}

void SVekaCodeEditor::OpenCompletion(bool bExplicit)
{
	const FTextLocation Cursor = Editor->GetCursorLocation();
	Completion = Veka::Editor::ComputeCompletion(GetText(), Cursor.GetLineIndex(), Cursor.GetOffset(), MakeSources(), bExplicit);
	CompletionLine = Cursor.GetLineIndex();
	CompletionItems.Reset();
	for (const FVekaCompletionItem& Item : Completion.Items)
	{
		CompletionItems.Add(MakeShared<FVekaCompletionItem>(Item));
		if (CompletionItems.Num() >= 200)
		{
			break;
		}
	}
	CompletionList->RequestListRefresh();
	if (!CompletionItems.IsEmpty())
	{
		CompletionList->SetSelection(CompletionItems[0]);
		CompletionList->RequestScrollIntoView(CompletionItems[0]);
	}
}

void SVekaCodeEditor::CloseCompletion()
{
	CompletionItems.Reset();
	CompletionList->RequestListRefresh();
}

void SVekaCodeEditor::AcceptCompletion(int32 Index)
{
	if (!CompletionItems.IsValidIndex(Index))
	{
		CloseCompletion();
		return;
	}
	const FString Insert = CompletionItems[Index]->Insert;
	Editor->SelectText(FTextLocation(CompletionLine, Completion.ReplaceStart), FTextLocation(CompletionLine, Completion.ReplaceEnd));
	Editor->InsertTextAtCursor(Insert);
	CloseCompletion();
	if (Insert.EndsWith(TEXT(".")))
	{
		ScheduleCompletionRefresh(false);
	}
}

FReply SVekaCodeEditor::HandleKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::SpaceBar && KeyEvent.IsControlDown())
	{
		ScheduleCompletionRefresh(false);
		return FReply::Handled();
	}
	if (IsCompletionOpen())
	{
		const TArray<TSharedPtr<FVekaCompletionItem>> Selected = CompletionList->GetSelectedItems();
		int32 Index = Selected.IsEmpty() ? 0 : CompletionItems.IndexOfByKey(Selected[0]);
		if (Key == EKeys::Down || Key == EKeys::Up)
		{
			Index = (Index + (Key == EKeys::Down ? 1 : -1) + CompletionItems.Num()) % CompletionItems.Num();
			CompletionList->SetSelection(CompletionItems[Index]);
			CompletionList->RequestScrollIntoView(CompletionItems[Index]);
			return FReply::Handled();
		}
		if (Key == EKeys::Enter || Key == EKeys::Tab)
		{
			AcceptCompletion(Index);
			return FReply::Handled();
		}
		if (Key == EKeys::Escape)
		{
			CloseCompletion();
			return FReply::Handled();
		}
		if (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Home || Key == EKeys::End)
		{
			CloseCompletion();
		}
	}
	if (Key == EKeys::Tab && !KeyEvent.IsShiftDown())
	{
		Editor->InsertTextAtCursor(FString(TEXT("    ")));
		return FReply::Handled();
	}
	if (Key == EKeys::Enter && !KeyEvent.IsShiftDown())
	{
		// Keep the indentation, one level more after `{`.
		const FTextLocation Cursor = Editor->GetCursorLocation();
		TArray<FString> Lines;
		GetText().ParseIntoArray(Lines, TEXT("\n"), false);
		FString Indent;
		if (Lines.IsValidIndex(Cursor.GetLineIndex()))
		{
			const FString& Line = Lines[Cursor.GetLineIndex()];
			for (const TCHAR C : Line)
			{
				if (C != TEXT(' '))
				{
					break;
				}
				Indent.AppendChar(C);
			}
			if (Line.Left(FMath::Min(Cursor.GetOffset(), Line.Len())).TrimEnd().EndsWith(TEXT("{")))
			{
				Indent += TEXT("    ");
			}
		}
		Editor->InsertTextAtCursor(TEXT("\n") + Indent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FMargin SVekaCodeEditor::GetCompletionPadding() const
{
	const FTextLocation Cursor = Editor->GetCursorLocation();
	TArray<FString> Lines;
	GetText().ParseIntoArray(Lines, TEXT("\n"), false);
	const FString Line = Lines.IsValidIndex(CompletionLine) ? Lines[CompletionLine] : FString();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float LineHeight = Measure->GetMaxCharacterHeight(Font);
	const float X = Measure->Measure(Line.Left(FMath::Clamp(Completion.ReplaceStart, 0, Line.Len())), Font).X;
	const float Scrolled = VerticalScroll.IsValid() ? VerticalScroll->DistanceFromTop() * LineHeight * FMath::Max(1, Lines.Num()) : 0.f;
	return FMargin(TextMargin + X, TextMargin + LineHeight * (CompletionLine + 1) - Scrolled + 2.f, 0.f, 0.f);
}

TSharedRef<ITableRow> SVekaCodeEditor::MakeCompletionRow(TSharedPtr<FVekaCompletionItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FVekaCompletionItem>>, Owner)
		.Padding(FMargin(4.f, 1.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Font(Font).Text(FText::FromString(Item->Label))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(FText::FromString(Item->Detail))
			]
		];
}

#undef LOCTEXT_NAMESPACE
