// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"

class SWidget;
struct FVekaDocument;

/**
 * The look of the Simple mode panels (outliner, palette, inspector): icons from the Veka symbol library, titled
 * sections, fields with their label above, explanations in tooltips. One place so every panel reads the same way.
 */
namespace Veka::Editor::Look
{
	/** Spacing of the panels (4 / 8 / 12 / 16 rhythm). */
	constexpr float SectionPadding = 12.f;
	constexpr float FieldGap = 8.f;

	/** Colors: secondary text, the icon of each family. */
	FLinearColor Subdued();
	FLinearColor FamilyColor(const FText& Family);

	/** A symbol of the built-in library drawn as an icon. */
	TSharedRef<SWidget> Icon(FName Symbol, float Size = 14.f, const FLinearColor& Color = FLinearColor(0.75f, 0.76f, 0.80f));

	/** What a document node is, in plain words (outliner rows, inspector header). */
	struct FNodeLook
	{
		FName Icon;
		/** "Vertical stack", "Text", the component's name... */
		FText Title;
		/** One sentence: what it does. */
		FText Description;
		/** What it shows: the text, the symbol, the number of views inside, the condition... */
		FText Detail;
		FLinearColor Color = FLinearColor::White;
	};
	FNodeLook DescribeNode(const FVekaDocument& Document, int32 Node);

	/** A section of a panel: icon and bold title, buttons on the right, the body, a thin line under it. */
	TSharedRef<SWidget> Section(const FText& Title, FName Icon, const FText& Tooltip, const TSharedRef<SWidget>& Body, const TSharedPtr<SWidget>& Buttons = nullptr);
	/** A field: small label above (the tooltip explains it), optional widgets after the label, the editor below. */
	TSharedRef<SWidget> Field(const FText& Label, const FText& Tooltip, const TSharedRef<SWidget>& Editor, const TSharedPtr<SWidget>& LabelExtra = nullptr);
	/** Two fields side by side, half the width each. */
	TSharedRef<SWidget> Pair(const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right);
	/** A small flat button showing a symbol. */
	TSharedRef<SWidget> IconButton(FName Symbol, const FText& Tooltip, TFunction<FReply()> OnClicked, TAttribute<bool> Enabled = true);
	/** The unit (px, pt, °) shown at the right end of a field. */
	TSharedRef<SWidget> WithUnit(const TSharedRef<SWidget>& Editor, const FText& Unit);
	/** Segmented buttons (few choices): Labels or Icons per choice, Current highlighted. */
	TSharedRef<SWidget> Segmented(const TArray<FText>& Labels, const TArray<FName>& Icons, const TArray<FText>& Tooltips, int32 Current, TFunction<void(int32)> Picked);
}
