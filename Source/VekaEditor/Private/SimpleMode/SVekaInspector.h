// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "View/VekaDocument.h"

class FVekaViewEditorContext;
class SVerticalBox;
class IDetailsView;
struct FVekaCatalogArgument;

/**
 * Inspector of the Simple mode, generated from the catalog / schema for the selected view: its label, its arguments
 * (typed editors: number, toggle, enum menu, Colora token or color picker, symbol picker, state binding; any argument
 * can also be written as an expression), its modifier stack (add, reorder, remove, edit) and its actions.
 */
class SVekaInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaInspector) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext);
	virtual ~SVekaInspector() override;

	void Rebuild();

	/** Sets an argument from Veka code (`"Buy"`, `count + 1`); false (and an error shown) if it does not parse. */
	bool ApplyArgumentCode(int32 Node, int32 Modifier, FName Name, const FString& Code);
	/** Sets an argument to a constant (typed editors). */
	void ApplyArgumentValue(int32 Node, int32 Modifier, FName Name, const FVekaValue& Value);
	/** Sets a Text argument from plain text (localizable, same key as the parser would give). */
	void ApplyArgumentText(int32 Node, int32 Modifier, FName Name, const FString& Text);

private:
	TSharedRef<SWidget> MakeArgumentRow(int32 Node, int32 Modifier, const FVekaCatalogArgument& Spec, FName Owner);
	TSharedRef<SWidget> MakeArgumentEditor(int32 Node, int32 Modifier, const FVekaCatalogArgument& Spec, FName Owner);
	TSharedRef<SWidget> MakeCodeBox(const FString& Key, const FString& Initial, TFunction<bool(const FString&)> Apply);
	TSharedRef<SWidget> MakeActionsEditor(int32 Node, int32 Modifier);
	TSharedRef<SWidget> MakeModifierSection(int32 Node);
	TSharedRef<SWidget> MakeSymbolPicker(TFunction<void(FName)> OnPicked);
	void AddSection(const FText& Title, FName Icon, const FText& Tooltip, const TSharedRef<SWidget>& Body, const TSharedPtr<SWidget>& Buttons = nullptr);
	/** Fields of arguments: short ones (numbers, on / off) two by two, the others on their own line. */
	TSharedRef<SWidget> MakeArgumentsGrid(int32 Node, int32 Modifier, const TArray<FVekaCatalogArgument>& Arguments, FName Owner);
	bool EditCodeExpression(const FText& Description, const FString& Key, const FString& Code, int32 Node, int32 Modifier, TFunction<void(FVekaDocument&, int32)> Assign);

	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<SVerticalBox> Content;
	/** Rows shown as code although they hold a constant ("Edit as code"). */
	TSet<FString> CodeRows;
	/** Last parse error of a code box, by row key. */
	TMap<FString, FText> Errors;
	FDelegateHandle DocumentHandle;
	FDelegateHandle SelectionHandle;
};

/**
 * State tab of the Simple mode: the view's states, params, events and slots (add, rename everywhere, type, initial
 * value, remove), and the mock values of the preview model (not saved).
 */
class SVekaStatePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaStatePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, TFunction<UObject*()> InGetModel);
	virtual ~SVekaStatePanel() override;

	void Rebuild();

private:
	TSharedRef<SWidget> MakeDeclarationRow(FName Name, const FText& Kind, EVekaValueType Type, const FVekaValue* Value, bool bTyped);

	TSharedPtr<FVekaViewEditorContext> Context;
	TFunction<UObject*()> GetModel;
	TSharedPtr<SVerticalBox> Content;
	TSharedPtr<IDetailsView> ModelDetails;
	TMap<FString, FText> Errors;
	FDelegateHandle DocumentHandle;
};
