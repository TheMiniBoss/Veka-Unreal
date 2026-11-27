// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/VekaEditorLook.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "View/VekaCatalog.h"
#include "View/VekaDocument.h"
#include "View/VekaViewAsset.h"
#include "VekaLanguage.h"
#include "VekaBuilders.h"
#include "Slate/SVekaHost.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VekaEditorLook"

namespace Veka::Editor::Look
{
	FLinearColor Subdued()
	{
		return FLinearColor(0.55f, 0.56f, 0.60f);
	}

	FLinearColor FamilyColor(const FText& Family)
	{
		const FString Name = Family.ToString();
		if (Name == TEXT("Layout")) { return FLinearColor(0.45f, 0.65f, 1.f); }
		if (Name == TEXT("Content")) { return FLinearColor(0.86f, 0.87f, 0.90f); }
		if (Name == TEXT("Controls")) { return FLinearColor(0.40f, 0.85f, 0.55f); }
		if (Name == TEXT("Shapes")) { return FLinearColor(0.98f, 0.62f, 0.30f); }
		if (Name == TEXT("Logic")) { return FLinearColor(0.80f, 0.47f, 0.95f); }
		if (Name == TEXT("Drawing")) { return FLinearColor(1.f, 0.45f, 0.70f); }
		if (Name == TEXT("Views")) { return FLinearColor(0.35f, 0.82f, 0.78f); }
		return FLinearColor(0.75f, 0.76f, 0.80f);
	}

	TSharedRef<SWidget> Icon(FName Symbol, float Size, const FLinearColor& Color)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(SVekaHost).Root(Veka::Symbol(Symbol).FontSize(Size * 0.86f).Foreground(FVekaPaint(Color)).Frame(Size, Size))
			];
	}

	namespace
	{
		FText Quote(const FString& Text)
		{
			return FText::FromString(TEXT("“") + Text.Left(40) + (Text.Len() > 40 ? TEXT("…") : TEXT("")) + TEXT("”"));
		}

		/** The text a node shows, when it is written as a constant. */
		FString LiteralArgument(const FVekaDocument& Document, const FVekaAstNode& N, FName Argument)
		{
			for (const FVekaAstArgument& Given : N.Arguments)
			{
				if (Given.Name == Argument && Document.Expressions.IsValidIndex(Given.Expr))
				{
					const FVekaAstExpr& Expr = Document.Expressions[Given.Expr];
					if (Expr.Kind == EVekaExprKind::Literal)
					{
						return Expr.Value.AsString();
					}
					return Veka::Lang::PrintExpression(Document, Given.Expr);
				}
			}
			return FString();
		}
	}

	FNodeLook DescribeNode(const FVekaDocument& Document, int32 Node)
	{
		FNodeLook Out;
		if (!Document.Nodes.IsValidIndex(Node))
		{
			return Out;
		}
		const FVekaAstNode& N = Document.Nodes[Node];
		const FLinearColor Logic = FamilyColor(LOCTEXT("Logic", "Logic"));
		switch (N.Kind)
		{
		case EVekaAstNodeKind::If:
			Out.Icon = TEXT("questionmark");
			Out.Title = LOCTEXT("If", "Condition");
			Out.Description = LOCTEXT("IfHelp", "Shows its views only when the condition is true (else: the other branch).");
			Out.Detail = FText::FromString(TEXT("if ") + Veka::Lang::PrintExpression(Document, N.Expr));
			Out.Color = Logic;
			return Out;
		case EVekaAstNodeKind::ForEach:
			Out.Icon = TEXT("repeat");
			Out.Title = LOCTEXT("ForEach", "Repeat");
			Out.Description = LOCTEXT("ForEachHelp", "Repeats its views: a number of times, or once per item of a list.");
			Out.Detail = FText::FromString((N.LoopVariable.IsNone() ? FString() : N.LoopVariable.ToString() + TEXT(" in ")) + Veka::Lang::PrintExpression(Document, N.Expr));
			Out.Color = Logic;
			return Out;
		case EVekaAstNodeKind::Let:
			Out.Icon = TEXT("equal");
			Out.Title = FText::FromName(N.LoopVariable);
			Out.Description = LOCTEXT("LetHelp", "A named value for the views after it in this block.");
			Out.Detail = FText::FromString(Veka::Lang::PrintExpression(Document, N.Expr));
			Out.Color = Logic;
			return Out;
		case EVekaAstNodeKind::Slot:
			Out.Icon = TEXT("tray");
			Out.Title = LOCTEXT("Slot", "Slot");
			Out.Description = LOCTEXT("SlotHelp", "Where this component shows the content its caller gives.");
			Out.Detail = N.Type.IsNone() ? LOCTEXT("DefaultSlot", "default") : FText::FromName(N.Type);
			Out.Color = Logic;
			return Out;
		default:
			break;
		}
		if (N.Component)
		{
			Out.Icon = TEXT("cube");
			Out.Title = FText::FromName(N.Type);
			Out.Description = LOCTEXT("ComponentHelp", "A view of the project used here as a component.");
			Out.Color = FamilyColor(LOCTEXT("Views", "Views"));
			return Out;
		}
		const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(N.Type, false);
		Out.Icon = Doc ? Doc->Icon : FName(TEXT("cube"));
		Out.Title = Doc ? Doc->Title : FText::FromName(N.Type);
		Out.Description = Doc ? Doc->Description : FText::GetEmpty();
		Out.Color = Doc ? FamilyColor(Doc->Category) : FamilyColor(FText::GetEmpty());
		// Detail: what it shows.
		static const FName TextArgs[] = { TEXT("text"), TEXT("title"), TEXT("placeholder"), TEXT("label") };
		for (const FName Argument : TextArgs)
		{
			const FString Shown = LiteralArgument(Document, N, Argument);
			if (!Shown.IsEmpty())
			{
				Out.Detail = Quote(Shown);
				return Out;
			}
		}
		if (N.Type == TEXT("Symbol"))
		{
			Out.Detail = FText::FromString(LiteralArgument(Document, N, TEXT("name")));
		}
		else if (!N.Children.IsEmpty())
		{
			Out.Detail = N.Children.Num() == 1 ? LOCTEXT("OneView", "1 view") : FText::Format(LOCTEXT("Views", "{0} views"), FText::AsNumber(N.Children.Num()));
		}
		return Out;
	}

	TSharedRef<SWidget> Section(const FText& Title, FName Icon, const FText& Tooltip, const TSharedRef<SWidget>& Body, const TSharedPtr<SWidget>& Buttons)
	{
		TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox).ToolTipText(Tooltip);
		if (!Icon.IsNone())
		{
			Header->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[Look::Icon(Icon, 14.f)];
		}
		Header->AddSlot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("NormalFontBold"))
			];
		if (Buttons.IsValid())
		{
			Header->AddSlot().AutoWidth().VAlign(VAlign_Center)[Buttons.ToSharedRef()];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(SectionPadding, 10.f, SectionPadding - 4.f, 6.f))[Header]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(SectionPadding, 0.f, SectionPadding, SectionPadding))[Body]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator).Thickness(1.f).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.06f))];
	}

	TSharedRef<SWidget> Field(const FText& Label, const FText& Tooltip, const TSharedRef<SWidget>& Editor, const TSharedPtr<SWidget>& LabelExtra)
	{
		TSharedRef<SHorizontalBox> LabelRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(Subdued())
			];
		if (LabelExtra.IsValid())
		{
			LabelRow->AddSlot().AutoWidth().VAlign(VAlign_Center)[LabelExtra.ToSharedRef()];
		}
		return SNew(SVerticalBox)
			.ToolTipText(Tooltip)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 3.f))[LabelRow]
			+ SVerticalBox::Slot().AutoHeight()[Editor];
	}

	TSharedRef<SWidget> Pair(const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, FieldGap * 0.5f, 0.f))[Left]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(FieldGap * 0.5f, 0.f, 0.f, 0.f))[Right];
	}

	TSharedRef<SWidget> IconButton(FName Symbol, const FText& Tooltip, TFunction<FReply()> OnClicked, TAttribute<bool> Enabled)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4.f, 3.f))
			.ToolTipText(Tooltip)
			.IsEnabled(Enabled)
			.OnClicked_Lambda([OnClicked]() { return OnClicked(); })
			[
				Icon(Symbol, 12.f)
			];
	}

	TSharedRef<SWidget> WithUnit(const TSharedRef<SWidget>& Editor, const FText& Unit)
	{
		// Always a field-shaped box, also when empty ("Auto"), so it reads as something to type into.
		const TSharedRef<SWidget> Boxed = SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.017f))
			.Padding(FMargin(0.f))
			[
				SNew(SBox).MinDesiredHeight(22.f).VAlign(VAlign_Center)[Editor]
			];
		if (Unit.IsEmpty())
		{
			return Boxed;
		}
		return SNew(SOverlay)
			+ SOverlay::Slot()[Boxed]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock).Text(Unit).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(Subdued()).Visibility(EVisibility::HitTestInvisible)
			];
	}

	TSharedRef<SWidget> Segmented(const TArray<FText>& Labels, const TArray<FName>& Icons, const TArray<FText>& Tooltips, int32 Current, TFunction<void(int32)> Picked)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 Index = 0; Index < Labels.Num(); ++Index)
		{
			const bool bIcon = Icons.IsValidIndex(Index) && !Icons[Index].IsNone();
			Row->AddSlot().FillWidth(1.f).Padding(FMargin(Index == 0 ? 0.f : 1.f, 0.f, 0.f, 0.f))
				[
					SNew(SCheckBox)
					.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
					.ToolTipText(Tooltips.IsValidIndex(Index) && !Tooltips[Index].IsEmpty() ? Tooltips[Index] : Labels[Index])
					.IsChecked(Index == Current ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged_Lambda([Picked, Index](ECheckBoxState) { Picked(Index); })
					[
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(4.f, 3.f))
						[
							bIcon ? Icon(Icons[Index], 13.f) : StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Labels[Index]).Font(FAppStyle::GetFontStyle("SmallFont")))
						]
					]
				];
		}
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.25f))
			.Padding(FMargin(2.f))
			[
				Row
			];
	}
}

#undef LOCTEXT_NAMESPACE
