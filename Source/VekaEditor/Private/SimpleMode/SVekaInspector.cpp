// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/SVekaInspector.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "SimpleMode/VekaEditorLook.h"
#include "VekaLanguage.h"
#include "VekaBuilders.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "Colora/VekaTheme.h"
#include "Core/VekaEnvironment.h"
#include "Slate/SVekaHost.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaPaintAsset.h"
#include "View/VekaComponentLibrary.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VekaInspector"

namespace
{
	using namespace Veka::Editor;

	constexpr float NameWidth = 96.f;

	FString RowKey(int32 Node, int32 Modifier, FName Name)
	{
		return FString::Printf(TEXT("%d/%d/%s"), Node, Modifier, *Name.ToString());
	}

	FString ToHexColor(const FLinearColor& Color)
	{
		const FColor C = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X%02X"), C.R, C.G, C.B, C.A);
	}

	FLinearColor PaintColor(const FVekaValue& Value, EVekaColorScheme Scheme)
	{
		if (Value.Type == EVekaValueType::Color)
		{
			return Value.ColorValue;
		}
		const FString Text = Value.AsString();
		if (Text.StartsWith(TEXT("#")))
		{
			return Veka::Hex(Text);
		}
		if (const FVekaPaint* Named = Veka::Paints::Find(FName(*Text)))
		{
			return Named->GetAverageColor();
		}
		FLinearColor Out = FLinearColor::Transparent;
		Veka::Colora::GetDefaultColor(FName(*Text), Scheme, Out);
		return Out;
	}

	FVekaValue DefaultValue(EVekaValueType Type)
	{
		switch (Type)
		{
		case EVekaValueType::Bool: return FVekaValue::Bool(false);
		case EVekaValueType::Int: return FVekaValue::Int(0);
		case EVekaValueType::Float: return FVekaValue::Float(0.0);
		case EVekaValueType::String: return FVekaValue::String(FString());
		case EVekaValueType::Text: return FVekaValue::Text(FText::GetEmpty());
		case EVekaValueType::Color: return FVekaValue::Color(FLinearColor::White);
		case EVekaValueType::Vector2: return FVekaValue::Vector2(FVector2D::ZeroVector);
		default: return FVekaValue();
		}
	}

	EVekaArgumentKind KindForType(EVekaValueType Type)
	{
		switch (Type)
		{
		case EVekaValueType::Int:
		case EVekaValueType::Float: return EVekaArgumentKind::Number;
		case EVekaValueType::Bool: return EVekaArgumentKind::Bool;
		case EVekaValueType::Text: return EVekaArgumentKind::Text;
		case EVekaValueType::Color: return EVekaArgumentKind::Paint;
		default: return EVekaArgumentKind::Any;
		}
	}

	const TArray<EVekaValueType>& ValueTypes()
	{
		static const TArray<EVekaValueType> Types = { EVekaValueType::Bool, EVekaValueType::Int, EVekaValueType::Float, EVekaValueType::String, EVekaValueType::Text,
			EVekaValueType::Color, EVekaValueType::Vector2, EVekaValueType::Array, EVekaValueType::Struct, EVekaValueType::None };
		return Types;
	}

	TSharedRef<SWidget> NameLabel(const FText& Name)
	{
		return SNew(SBox).WidthOverride(NameWidth).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Name).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	/** A combo button whose menu lists Labels; Picked receives the index. */
	TSharedRef<SWidget> MakeChoice(const FText& Current, const TArray<FText>& Labels, TFunction<void(int32)> Picked, const TArray<FLinearColor>* Swatches = nullptr)
	{
		return SNew(SComboButton)
			.OnGetMenuContent_Lambda([Labels, Picked, Swatches = Swatches ? *Swatches : TArray<FLinearColor>()]()
			{
				FMenuBuilder Menu(true, nullptr);
				for (int32 Index = 0; Index < Labels.Num(); ++Index)
				{
					const FUIAction Action(FExecuteAction::CreateLambda([Picked, Index]() { Picked(Index); }));
					if (Swatches.IsValidIndex(Index))
					{
						Menu.AddMenuEntry(Action, SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
							[
								SNew(SColorBlock).Color(Swatches[Index]).Size(FVector2D(14.0, 14.0)).AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(Labels[Index])
							]);
					}
					else
					{
						Menu.AddMenuEntry(Labels[Index], FText::GetEmpty(), FSlateIcon(), Action);
					}
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock).Text(Current)
			];
	}
}

void SVekaInspector::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext)
{
	Context = InContext;
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SAssignNew(Content, SVerticalBox)
			.IsEnabled_Lambda([this]() { return !Context->IsReadOnly(); })
		]
	];
	DocumentHandle = Context->OnDocumentChanged.AddSP(this, &SVekaInspector::Rebuild);
	SelectionHandle = Context->OnSelectionChanged.AddSP(this, &SVekaInspector::Rebuild);
	Rebuild();
}

SVekaInspector::~SVekaInspector()
{
	if (Context.IsValid())
	{
		Context->OnDocumentChanged.Remove(DocumentHandle);
		Context->OnSelectionChanged.Remove(SelectionHandle);
	}
}

void SVekaInspector::AddSection(const FText& Title, FName Icon, const FText& Tooltip, const TSharedRef<SWidget>& Body, const TSharedPtr<SWidget>& Buttons)
{
	Content->AddSlot().AutoHeight()[Look::Section(Title, Icon, Tooltip, Body, Buttons)];
}

TSharedRef<SWidget> SVekaInspector::MakeArgumentsGrid(int32 Node, int32 Modifier, const TArray<FVekaCatalogArgument>& Arguments, FName Owner)
{
	// Short fields (numbers, on / off) go two by two, like width and height; the others take the whole line.
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	TSharedPtr<SWidget> Pending;
	auto Flush = [&]()
	{
		if (Pending.IsValid())
		{
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Look::FieldGap))[Look::Pair(Pending.ToSharedRef(), SNew(SBox))];
			Pending.Reset();
		}
	};
	for (const FVekaCatalogArgument& Argument : Arguments)
	{
		const TSharedRef<SWidget> Field = MakeArgumentRow(Node, Modifier, Argument, Owner);
		const bool bShort = Argument.Kind == EVekaArgumentKind::Number || Argument.Kind == EVekaArgumentKind::Bool;
		if (!bShort)
		{
			Flush();
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Look::FieldGap))[Field];
		}
		else if (Pending.IsValid())
		{
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Look::FieldGap))[Look::Pair(Pending.ToSharedRef(), Field)];
			Pending.Reset();
		}
		else
		{
			Pending = Field;
		}
	}
	Flush();
	return Box;
}

void SVekaInspector::Rebuild()
{
	Content->ClearChildren();
	const FVekaDocument* Document = Context->GetDocument();
	const int32 Node = Context->GetPrimarySelection();
	if (Context->IsReadOnly())
	{
		Content->AddSlot().AutoHeight().Padding(FMargin(Look::SectionPadding, Look::SectionPadding, Look::SectionPadding, 0.f))
		[
			SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1.f, 0.5f, 0.4f))
			.Text(LOCTEXT("ReadOnly", "The code has errors: fix them in the Code tab to edit here again (your text is never overwritten)."))
		];
	}
	if (!Document || !Document->Nodes.IsValidIndex(Node))
	{
		Content->AddSlot().AutoHeight().Padding(Look::SectionPadding)
		[
			SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(Look::Subdued())
			.Text(LOCTEXT("NoSelection", "Select a view in the Views list or the viewport."))
		];
		return;
	}
	const FVekaAstNode& N = Document->Nodes[Node];
	const Look::FNodeLook Described = Look::DescribeNode(*Document, Node);

	// Header: what it is, in plain words, and its name in the Views list.
	Content->AddSlot().AutoHeight()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(Look::SectionPadding, 14.f, Look::SectionPadding, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(Described.Color.R, Described.Color.G, Described.Color.B, 0.16f))
				.Padding(FMargin(6.f))
				[
					Look::Icon(Described.Icon, 18.f, Described.Color)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Described.Title).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromName(N.Kind == EVekaAstNodeKind::Element ? N.Type : NAME_None)).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(Look::Subdued())
					.Visibility(N.Kind == EVekaAstNodeKind::Element && !Described.Title.EqualTo(FText::FromName(N.Type)) ? EVisibility::Visible : EVisibility::Collapsed)
					.ToolTipText(LOCTEXT("CodeNameTooltip", "Its name in the code"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(Look::SectionPadding, 2.f, Look::SectionPadding, 10.f))
		[
			SNew(STextBlock).AutoWrapText(true).Text(Described.Description).ColorAndOpacity(Look::Subdued())
			.Visibility(Described.Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(Look::SectionPadding, 0.f, Look::SectionPadding, Look::SectionPadding))
		[
			Look::Field(LOCTEXT("Label", "Name in the Views list"), LOCTEXT("LabelTooltip", "Helps you find it in the Views list; written as a // comment in the code. Empty: its type is shown."),
				SNew(SEditableTextBox)
				.HintText(Described.Title)
				.Text(FText::FromString(N.Comments.IsEmpty() ? FString() : N.Comments[0]))
				.OnTextCommitted_Lambda([this, Node](const FText& Text, ETextCommit::Type)
				{
					const FString Label = Text.ToString();
					Context->Edit(LOCTEXT("SetLabel", "Rename view"), [Node, &Label](FVekaDocument& Doc) -> TArray<int32>
					{
						Doc::SetLabel(Doc, Node, Label);
						return { Node };
					});
				}))
		]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator).Thickness(1.f).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.06f))]
	];

	if (N.Kind == EVekaAstNodeKind::Let)
	{
		const FString Key = RowKey(Node, -2, TEXT("expr"));
		AddSection(LOCTEXT("LetValue", "Value"), TEXT("equal"), LOCTEXT("LetValueTooltip", "A formula: numbers, states, loop variables, functions (sin, random, time...)."),
			Look::Field(FText::FromString(TEXT("let ") + N.LoopVariable.ToString() + TEXT(" =")), LOCTEXT("LetFieldTooltip", "Its value, written in Veka."),
				MakeCodeBox(Key, Veka::Lang::PrintExpression(*Document, N.Expr), [this, Node, Key](const FString& Code)
				{
					return EditCodeExpression(LOCTEXT("SetLet", "Edit value"), Key, Code, Node, INDEX_NONE, [Node](FVekaDocument& Doc, int32 Expr) { Doc.Nodes[Node].Expr = Expr; });
				})));
		return;
	}
	if (N.Kind == EVekaAstNodeKind::If || N.Kind == EVekaAstNodeKind::ForEach)
	{
		const bool bIf = N.Kind == EVekaAstNodeKind::If;
		const FString Key = RowKey(Node, -2, TEXT("expr"));
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Look::FieldGap))
			[
				Look::Field(bIf ? LOCTEXT("ConditionField", "Show when") : LOCTEXT("ItemsField", "Repeat for"),
					bIf ? LOCTEXT("ConditionTooltip", "A condition: a Bool state, a comparison (count > 3), !state...") : LOCTEXT("ItemsTooltip", "A number (36: 36 times) or a list (model.Items: once per item)."),
					MakeCodeBox(Key, Veka::Lang::PrintExpression(*Document, N.Expr), [this, Node, Key](const FString& Code)
					{
						return EditCodeExpression(LOCTEXT("SetCondition", "Edit expression"), Key, Code, Node, INDEX_NONE, [Node](FVekaDocument& Doc, int32 Expr) { Doc.Nodes[Node].Expr = Expr; });
					}))
			];
		if (!bIf)
		{
			auto VariableField = [this, Node](const FText& Label, const FText& Tooltip, FName Current, bool bIndex) -> TSharedRef<SWidget>
			{
				return Look::Field(Label, Tooltip, SNew(SEditableTextBox)
					.Text(FText::FromName(Current))
					.OnTextCommitted_Lambda([this, Node, bIndex](const FText& Text, ETextCommit::Type)
					{
						const FName Name(*Text.ToString().TrimStartAndEnd());
						Context->Edit(LOCTEXT("SetLoopVariable", "Rename loop variable"), [Node, Name, bIndex](FVekaDocument& Doc) -> TArray<int32>
						{
							FVekaAstNode& Loop = Doc.Nodes[Node];
							const FName Old = bIndex ? Loop.IndexVariable : Loop.LoopVariable;
							(bIndex ? Loop.IndexVariable : Loop.LoopVariable) = Name;
							// Uses of the variable inside the loop follow.
							for (FVekaAstExpr& Expr : Doc.Expressions)
							{
								if (Expr.Kind == EVekaExprKind::Local && Expr.Name == Old && !Old.IsNone())
								{
									Expr.Name = Name;
								}
							}
							return { Node };
						});
					}));
			};
			Body->AddSlot().AutoHeight()
			[
				Look::Pair(VariableField(LOCTEXT("ItemVariable", "Item name"), LOCTEXT("ItemVariableTooltip", "How the views inside call the current item (or number)."), N.LoopVariable, false),
					VariableField(LOCTEXT("IndexVariable", "Index name"), LOCTEXT("IndexVariableTooltip", "Optional: how they call its position (0, 1, 2...)."), N.IndexVariable, true))
			];
		}
		AddSection(Described.Title, Described.Icon, Described.Description, Body);
	}
	else if (N.Kind == EVekaAstNodeKind::Slot)
	{
		TArray<FText> Labels = { LOCTEXT("DefaultSlot", "default") };
		TArray<FName> Names = { NAME_None };
		for (const FVekaSlotDeclaration& Slot : Document->Slots)
		{
			if (!Slot.Name.IsNone())
			{
				Labels.Add(FText::FromName(Slot.Name));
				Names.Add(Slot.Name);
			}
		}
		AddSection(LOCTEXT("SlotSection", "Slot"), TEXT("tray"), Described.Description,
			Look::Field(LOCTEXT("SlotField", "Shows the content of"), LOCTEXT("SlotFieldTooltip", "Which slot of this component."),
				MakeChoice(N.Type.IsNone() ? LOCTEXT("DefaultSlot", "default") : FText::FromName(N.Type), Labels, [this, Node, Names](int32 Index)
				{
					Context->Edit(LOCTEXT("SetSlot", "Change slot"), [Node, &Names, Index](FVekaDocument& Doc) -> TArray<int32>
					{
						Doc.Nodes[Node].Type = Names[Index];
						return { Node };
					});
				})));
	}
	else
	{
		// Its settings: the catalog's arguments, or a component's params.
		TArray<FVekaCatalogArgument> Arguments;
		const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(N.Type);
		if (Entry)
		{
			Arguments = Entry->Arguments;
		}
		else if (N.Component)
		{
			for (const FVekaParamDeclaration& Param : N.Component->Document.Params)
			{
				FVekaCatalogArgument Argument;
				Argument.Name = Param.Name;
				Argument.Kind = KindForType(Param.Type);
				Arguments.Add(Argument);
			}
		}
		else
		{
			for (const FVekaAstArgument& Given : N.Arguments)
			{
				FVekaCatalogArgument Argument;
				Argument.Name = Given.Name;
				Arguments.Add(Argument);
			}
		}

		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		if (Arguments.IsEmpty())
		{
			Body->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Look::Subdued()).Text(LOCTEXT("NoArguments", "Nothing to set: shape it with the modifiers below."))];
		}
		else
		{
			Body->AddSlot().AutoHeight()[MakeArgumentsGrid(Node, INDEX_NONE, Arguments, N.Type)];
		}

		// Content of a component's slot.
		const FVekaNodeLocation Location = Doc::FindLocation(*Document, Node);
		const UVekaView* ParentComponent = Document->Nodes.IsValidIndex(Location.Parent) ? Document->Nodes[Location.Parent].Component.Get() : nullptr;
		if (ParentComponent && !ParentComponent->Document.Slots.IsEmpty())
		{
			TArray<FText> Labels;
			TArray<FName> Names;
			for (const FVekaSlotDeclaration& Slot : ParentComponent->Document.Slots)
			{
				Labels.Add(Slot.Name.IsNone() ? LOCTEXT("DefaultSlot", "default") : FText::FromName(Slot.Name));
				Names.Add(Slot.Name);
			}
			Body->AddSlot().AutoHeight().Padding(FMargin(0.f, Look::FieldGap, 0.f, 0.f))
			[
				Look::Field(LOCTEXT("InSlot", "In slot"), LOCTEXT("InSlotTooltip", "Which slot of the component this view fills."),
					MakeChoice(N.SlotName.IsNone() ? LOCTEXT("DefaultSlot", "default") : FText::FromName(N.SlotName), Labels, [this, Node, Names](int32 Index)
					{
						Context->Edit(LOCTEXT("SetInSlot", "Change slot"), [Node, &Names, Index](FVekaDocument& Doc) -> TArray<int32>
						{
							Doc.Nodes[Node].SlotName = Names[Index];
							return { Node };
						});
					}))
			];
		}
		AddSection(N.Component ? LOCTEXT("ParamsSection", "Parameters") : LOCTEXT("SettingsSection", "Settings"), NAME_None,
			N.Component ? LOCTEXT("ParamsTooltip", "The values this component asks for (its @Param).") : Described.Description, Body);

		if (Entry && Entry->bActions)
		{
			AddSection(LOCTEXT("Actions", "When clicked"), TEXT("hand.thumbsup"), LOCTEXT("ActionsTooltip", "What happens on a click: change a state, send an event, play a timeline..."),
				MakeActionsEditor(Node, INDEX_NONE));
		}
	}

	Content->AddSlot().AutoHeight()[MakeModifierSection(Node)];
}

TSharedRef<SWidget> SVekaInspector::MakeCodeBox(const FString& Key, const FString& Initial, TFunction<bool(const FString&)> Apply)
{
	const FText* Error = Errors.Find(Key);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SEditableTextBox)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.Text(FText::FromString(Initial))
			.HintText(LOCTEXT("CodeHint", "Veka expression"))
			.ToolTipText(LOCTEXT("CodeTooltip", "Any Veka expression: \"text\", 12, .leading, a state, count + 1, \"\\(model.Gold) gold\"..."))
			.OnTextCommitted_Lambda([Apply, Initial](const FText& Text, ETextCommit::Type Commit)
			{
				if (Commit != ETextCommit::OnCleared && Text.ToString() != Initial)
				{
					Apply(Text.ToString());
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Visibility(Error ? EVisibility::Visible : EVisibility::Collapsed)
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.4f))
			.Text(Error ? *Error : FText::GetEmpty())
		];
}

bool SVekaInspector::EditCodeExpression(const FText& Description, const FString& Key, const FString& Code, int32 Node, int32 Modifier, TFunction<void(FVekaDocument&, int32)> Assign)
{
	const FVekaDocument* Document = Context->GetDocument();
	if (!Document)
	{
		return false;
	}
	const TArray<FName> Locals = Doc::LocalsAt(*Document, Node, Modifier);
	const FString Namespace = Context->GetTextNamespace();
	// Checked on a scratch copy first: a wrong expression changes nothing.
	FVekaDocument Scratch = *Document;
	TArray<FVekaDiagnostic> Diagnostics;
	if (!Code.TrimStartAndEnd().IsEmpty() && Veka::Lang::ParseExpression(Scratch, Code, Locals, Namespace, Diagnostics) == INDEX_NONE)
	{
		Errors.Add(Key, Diagnostics.IsEmpty() ? LOCTEXT("BadExpression", "Not a valid expression") : Diagnostics[0].Message);
		Rebuild();
		return false;
	}
	Errors.Remove(Key);
	Context->Edit(Description, [&](FVekaDocument& Doc) -> TArray<int32>
	{
		TArray<FVekaDiagnostic> Ignored;
		const int32 Expr = Code.TrimStartAndEnd().IsEmpty() ? INDEX_NONE : Veka::Lang::ParseExpression(Doc, Code, Locals, Namespace, Ignored);
		Assign(Doc, Expr);
		return { Node };
	});
	return true;
}

bool SVekaInspector::ApplyArgumentCode(int32 Node, int32 Modifier, FName Name, const FString& Code)
{
	return EditCodeExpression(LOCTEXT("SetArgument", "Edit argument"), RowKey(Node, Modifier, Name), Code, Node, Modifier, [Node, Modifier, Name](FVekaDocument& Doc, int32 Expr)
	{
		Doc::SetArgument(Doc, Node, Modifier, Name, Expr);
	});
}

void SVekaInspector::ApplyArgumentValue(int32 Node, int32 Modifier, FName Name, const FVekaValue& Value)
{
	Context->Edit(LOCTEXT("SetArgument", "Edit argument"), [Node, Modifier, Name, &Value](FVekaDocument& Doc) -> TArray<int32>
	{
		Doc::SetArgument(Doc, Node, Modifier, Name, Value.IsNone() ? INDEX_NONE : Doc::AddLiteral(Doc, Value));
		return { Node };
	});
}

void SVekaInspector::ApplyArgumentText(int32 Node, int32 Modifier, FName Name, const FString& Text)
{
	ApplyArgumentCode(Node, Modifier, Name, Veka::Lang::PrintValue(FVekaValue::Text(FText::FromString(Text))));
}

TSharedRef<SWidget> SVekaInspector::MakeArgumentRow(int32 Node, int32 Modifier, const FVekaCatalogArgument& Spec, FName Owner)
{
	const FString Key = RowKey(Node, Modifier, Spec.Name);
	const FName Name = Spec.Name;
	const bool bModifier = Modifier != INDEX_NONE;
	const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(Owner, bModifier);
	const FVekaArgumentDoc* ArgumentDoc = Doc ? Doc->FindArgument(Name) : nullptr;
	const FText Label = Veka::Catalog::ArgumentLabel(Owner, bModifier, Name);
	const FText Help = ArgumentDoc ? ArgumentDoc->Help : FText::GetEmpty();
	const FVekaDocument* Document = Context->GetDocument();
	const bool bSet = Document && Doc::FindArgument(*Document, Node, Modifier, Name) != INDEX_NONE;
	const bool bCode = CodeRows.Contains(Key);

	// fx: write it as code (a state, a formula); x: back to its default.
	TSharedRef<SHorizontalBox> Extra = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(3.f, 0.f))
			.ToolTipText(bCode ? LOCTEXT("EditAsValueTooltip", "Back to the simple editor") : LOCTEXT("EditAsCodeTooltip", "Write it as code: a state (count), a formula (count * 2), a function (sin(time))..."))
			.OnClicked_Lambda([this, Key, bCode]()
			{
				bCode ? (void)CodeRows.Remove(Key) : (void)CodeRows.Add(Key);
				Rebuild();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("Fx", "fx")).Font(FAppStyle::GetFontStyle("SmallFontBold"))
				.ColorAndOpacity(bCode ? FLinearColor(0.45f, 0.65f, 1.f) : Look::Subdued())
			]
		];
	if (bSet)
	{
		Extra->AddSlot().AutoWidth()
		[
			Look::IconButton(TEXT("xmark"), LOCTEXT("ClearTooltip", "Remove it: its default applies"), [this, Node, Modifier, Name]()
			{
				ApplyArgumentValue(Node, Modifier, Name, FVekaValue());
				return FReply::Handled();
			})
		];
	}
	return Look::Field(Label, Help, MakeArgumentEditor(Node, Modifier, Spec, Owner), Extra);
}

TSharedRef<SWidget> SVekaInspector::MakeArgumentEditor(int32 Node, int32 Modifier, const FVekaCatalogArgument& Spec, FName Owner)
{
	const FVekaDocument* Document = Context->GetDocument();
	const FName Name = Spec.Name;
	const FString Key = RowKey(Node, Modifier, Name);
	const int32 ExprIndex = Doc::FindArgument(*Document, Node, Modifier, Name);
	const FVekaAstExpr* Expr = Document->Expressions.IsValidIndex(ExprIndex) ? &Document->Expressions[ExprIndex] : nullptr;
	const bool bLiteral = !Expr || Expr->Kind == EVekaExprKind::Literal;
	const FVekaValue Value = Expr && bLiteral ? Expr->Value : FVekaValue();
	auto Code = [&]()
	{
		return MakeCodeBox(Key, Expr ? Veka::Lang::PrintExpression(*Document, ExprIndex) : FString(), [this, Node, Modifier, Name](const FString& Text)
		{
			return ApplyArgumentCode(Node, Modifier, Name, Text);
		});
	};
	if (CodeRows.Contains(Key))
	{
		return Code();
	}

	switch (Spec.Kind)
	{
	case EVekaArgumentKind::Binding:
	case EVekaArgumentKind::Any:
	{
		// A state to bind; Any arguments holding something else are edited as code.
		if (Spec.Kind == EVekaArgumentKind::Any && Expr && Expr->Kind != EVekaExprKind::State && !(bLiteral && Value.IsNone()))
		{
			return Code();
		}
		TArray<FText> Labels = { LOCTEXT("NoState", "(none)") };
		TArray<FName> States = { NAME_None };
		for (const FVekaStateDeclaration& State : Document->States)
		{
			Labels.Add(FText::Format(LOCTEXT("StateEntry", "{0}  ({1})"), FText::FromName(State.Name), FText::FromString(Veka::Lang::TypeName(State.Initial.Type))));
			States.Add(State.Name);
		}
		const FText Current = !Expr ? LOCTEXT("NoState", "(none)") : Expr->Kind == EVekaExprKind::State ? FText::FromName(Expr->Name)
			: FText::Format(LOCTEXT("ConstantBinding", "{0} (constant)"), FText::FromString(Veka::Lang::PrintExpression(*Document, ExprIndex)));
		return MakeChoice(Current, Labels, [this, Node, Modifier, Name, States](int32 Index)
		{
			ApplyArgumentCode(Node, Modifier, Name, States[Index].IsNone() ? FString() : States[Index].ToString());
		});
	}
	case EVekaArgumentKind::Bool:
		if (!bLiteral || (Expr && Value.Type != EVekaValueType::Bool))
		{
			return Code();
		}
		return SNew(SCheckBox)
			.IsChecked(Value.BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this, Node, Modifier, Name](ECheckBoxState State) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::Bool(State == ECheckBoxState::Checked)); });
	case EVekaArgumentKind::Number:
	{
		if (!bLiteral || (Expr && !Value.IsNumber()))
		{
			return Code();
		}
		const bool bWasFloat = Value.Type == EVekaValueType::Float;
		const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(Owner, Modifier != INDEX_NONE);
		const FVekaArgumentDoc* ArgumentDoc = Doc ? Doc->FindArgument(Name) : nullptr;
		return Look::WithUnit(SNew(SNumericEntryBox<double>)
			.AllowSpin(true)
			.MinSliderValue(0.0)
			.MaxSliderValue(Owner == TEXT("opacity") || Owner == TEXT("ProgressView") || Owner == TEXT("Slider") ? 1.0 : 400.0)
			.Value(Expr ? TOptional<double>(Value.AsFloat()) : TOptional<double>())
			.UndeterminedString(LOCTEXT("Unset", "default"))
			.OnValueCommitted_Lambda([this, Node, Modifier, Name, bWasFloat](double NewValue, ETextCommit::Type)
			{
				const bool bInt = !bWasFloat && FMath::IsNearlyEqual(NewValue, FMath::RoundToDouble(NewValue));
				ApplyArgumentValue(Node, Modifier, Name, bInt ? FVekaValue::Int(static_cast<int64>(FMath::RoundToDouble(NewValue))) : FVekaValue::Float(NewValue));
			}), ArgumentDoc ? ArgumentDoc->Unit : FText::GetEmpty());
	}
	case EVekaArgumentKind::Enum:
	{
		if (!bLiteral || !Spec.Enum || (Expr && Value.Type != EVekaValueType::String))
		{
			return Code();
		}
		TArray<FText> Labels;
		TArray<FString> Names;
		TArray<FName> Icons;
		for (int32 Index = 0; Index < Spec.Enum->NumEnums() - 1; ++Index)
		{
			Names.Add(Spec.Enum->GetNameStringByIndex(Index));
			Labels.Add(Spec.Enum->GetDisplayNameTextByIndex(Index));
			const FString& Item = Names.Last();
			const bool bHorizontal = Spec.Enum == StaticEnum<EVekaHAlign>();
			const bool bVertical = Spec.Enum == StaticEnum<EVekaVAlign>();
			Icons.Add(bHorizontal ? FName(Item == TEXT("Leading") ? TEXT("text.alignleft") : Item == TEXT("Trailing") ? TEXT("text.alignright") : TEXT("text.aligncenter"))
				: bVertical ? FName(Item == TEXT("Top") ? TEXT("arrow.up.to.line") : Item == TEXT("Bottom") ? TEXT("arrow.down.to.line") : TEXT("minus"))
				: NAME_None);
		}
		const int32 Current = Expr ? Names.IndexOfByKey(Value.StringValue) : INDEX_NONE;
		auto Pick = [this, Node, Modifier, Name, Names](int32 Index) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Names[Index])); };
		if (Names.Num() <= 4)
		{
			return Look::Segmented(Labels, Icons, Labels, Current, Pick);
		}
		return MakeChoice(Labels.IsValidIndex(Current) ? Labels[Current] : LOCTEXT("Unset", "default"), Labels, Pick);
	}
	case EVekaArgumentKind::Paint:
	{
		if (!bLiteral || (Expr && Value.Type != EVekaValueType::String && Value.Type != EVekaValueType::Color))
		{
			return Code();
		}
		TArray<FText> Labels;
		TArray<FString> Tokens;
		TArray<FLinearColor> Swatches;
		for (const FName Token : Veka::Colora::DefaultTokenNames())
		{
			Tokens.Add(Token.ToString());
			Labels.Add(FText::FromName(Token));
			FLinearColor Color;
			Veka::Colora::GetDefaultColor(Token, EVekaColorScheme::Dark, Color);
			Swatches.Add(Color);
		}
		// The project's paint assets (mesh gradients...), shown with their average color.
		for (const FName PaintName : Veka::Paints::GetAllNames())
		{
			Tokens.Add(PaintName.ToString());
			Labels.Add(FText::Format(LOCTEXT("PaintAssetEntry", "{0}  (paint)"), FText::FromName(PaintName)));
			Swatches.Add(Veka::Paints::Find(PaintName) ? Veka::Paints::Find(PaintName)->GetAverageColor() : FLinearColor::White);
		}
		const FLinearColor Current = Expr ? PaintColor(Value, EVekaColorScheme::Dark) : FLinearColor::Transparent;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				SNew(SColorBlock)
				.Color(Current)
				.Size(FVector2D(22.0, 16.0))
				.ToolTipText(LOCTEXT("CustomColor", "Click: pick any color"))
				.OnMouseButtonDown_Lambda([this, Node, Modifier, Name, Current](const FGeometry&, const FPointerEvent&)
				{
					FColorPickerArgs Args(Current, FOnLinearColorValueChanged::CreateLambda([this, Node, Modifier, Name](FLinearColor Picked)
					{
						ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(ToHexColor(Picked)));
					}));
					Args.bUseAlpha = true;
					Args.bOnlyRefreshOnMouseUp = true;
					OpenColorPicker(Args);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				MakeChoice(Expr ? FText::FromString(Veka::Lang::PrintExpression(*Document, ExprIndex)) : LOCTEXT("Unset", "default"), Labels,
					[this, Node, Modifier, Name, Tokens](int32 Index) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Tokens[Index])); }, &Swatches)
			];
	}
	case EVekaArgumentKind::Style:
	{
		if (!bLiteral || (Expr && Value.Type != EVekaValueType::String))
		{
			return Code();
		}
		TArray<FText> Labels;
		TArray<FString> Styles;
		for (const UVekaComponentLibrary* Library : Veka::Components::GetLibraries())
		{
			for (const FVekaSharedStyle& Style : Library->Styles)
			{
				if (!Style.Name.IsNone() && !Styles.Contains(Style.Name.ToString()))
				{
					Styles.Add(Style.Name.ToString());
					Labels.Add(FText::Format(LOCTEXT("StyleEntry", ".{0}  ({1})"), FText::FromName(Style.Name), Library->GetLibraryName()));
				}
			}
		}
		return MakeChoice(Expr ? FText::FromString(TEXT(".") + Value.StringValue) : LOCTEXT("NoStyle", "(choose a style)"), Labels, [this, Node, Modifier, Name, Styles](int32 Index)
		{
			ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Styles[Index]));
		});
	}
	case EVekaArgumentKind::Text:
	{
		if (!bLiteral || (Expr && Value.Type != EVekaValueType::Text))
		{
			return Code();
		}
		const FString* Source = Expr ? FTextInspector::GetSourceString(Value.TextValue) : nullptr;
		return SNew(SEditableTextBox)
			.Text(FText::FromString(Source ? *Source : FString()))
			.HintText(LOCTEXT("TextHint", "Text shown (translatable)"))
			.OnTextCommitted_Lambda([this, Node, Modifier, Name](const FText& Text, ETextCommit::Type Commit)
			{
				if (Commit != ETextCommit::OnCleared)
				{
					ApplyArgumentText(Node, Modifier, Name, Text.ToString());
				}
			});
	}
	case EVekaArgumentKind::Name:
	{
		if (!bLiteral || (Expr && Value.Type != EVekaValueType::String))
		{
			return Code();
		}
		const bool bSymbol = Name == TEXT("name") || Name == TEXT("symbol");
		if (bSymbol)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(SBox).WidthOverride(20.f).HeightOverride(20.f)
					[
						SNew(SVekaHost).Root(Expr ? Veka::Symbol(FName(*Value.StringValue)) : FVekaElement())
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this, Node, Modifier, Name]()
					{
						return MakeSymbolPicker([this, Node, Modifier, Name](FName Picked) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Picked.ToString())); });
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(Expr ? FText::FromString(Value.StringValue) : LOCTEXT("Unset", "default"))
					]
				];
		}
		if (Owner == TEXT("transition"))
		{
			const TArray<FString> Kinds = { TEXT("opacity"), TEXT("scale"), TEXT("slide"), TEXT("move"), TEXT("blur") };
			return MakeChoice(Expr ? FText::FromString(TEXT(".") + Value.StringValue) : LOCTEXT("Unset", "default"),
				{ FText::FromString(TEXT(".opacity")), FText::FromString(TEXT(".scale")), FText::FromString(TEXT(".slide")), FText::FromString(TEXT(".move")), FText::FromString(TEXT(".blur")) },
				[this, Node, Modifier, Name, Kinds](int32 Index) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Kinds[Index])); });
		}
		if (Owner == TEXT("on"))
		{
			const UVekaView* Component = Document->Nodes[Node].Component;
			TArray<FText> Labels;
			TArray<FString> Events;
			if (Component)
			{
				for (const FVekaEventDeclaration& Event : Component->Document.EventDeclarations)
				{
					Events.Add(Event.Name.ToString());
					Labels.Add(FText::FromName(Event.Name));
				}
			}
			return MakeChoice(Expr ? FText::FromString(Value.StringValue) : LOCTEXT("NoEvent", "(event)"), Labels,
				[this, Node, Modifier, Name, Events](int32 Index) { ApplyArgumentValue(Node, Modifier, Name, FVekaValue::String(Events[Index])); });
		}
		return SNew(SEditableTextBox)
			.Text(FText::FromString(Value.AsString()))
			.OnTextCommitted_Lambda([this, Node, Modifier, Name](const FText& Text, ETextCommit::Type)
			{
				ApplyArgumentValue(Node, Modifier, Name, Text.IsEmpty() ? FVekaValue() : FVekaValue::String(Text.ToString()));
			});
	}
	default:
		return Code();
	}
}

TSharedRef<SWidget> SVekaInspector::MakeSymbolPicker(TFunction<void(FName)> OnPicked)
{
	struct FState
	{
		TArray<TSharedPtr<FName>> All;
		TArray<TSharedPtr<FName>> Shown;
		TSharedPtr<SListView<TSharedPtr<FName>>> List;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	for (const FName Name : Veka::Symbols::GetAllNames())
	{
		State->All.Add(MakeShared<FName>(Name));
	}
	State->Shown = State->All;
	return SNew(SBox)
		.WidthOverride(280.f)
		.HeightOverride(360.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
			[
				SNew(SSearchBox)
				.OnTextChanged_Lambda([State](const FText& Text)
				{
					State->Shown = State->All.FilterByPredicate([&Text](const TSharedPtr<FName>& Name) { return Name->ToString().Contains(Text.ToString()); });
					State->List->RequestListRefresh();
				})
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(State->List, SListView<TSharedPtr<FName>>)
				.ListItemsSource(&State->Shown)
				.OnGenerateRow_Lambda([](TSharedPtr<FName> Name, const TSharedRef<STableViewBase>& Owner)
				{
					return SNew(STableRow<TSharedPtr<FName>>, Owner)
						.Padding(FMargin(4.f, 2.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
							[
								SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)
								[
									SNew(SVekaHost).Root(Veka::Symbol(*Name))
								]
							]
							+ SHorizontalBox::Slot().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromName(*Name))
							]
						];
				})
				.OnMouseButtonClick_Lambda([OnPicked](TSharedPtr<FName> Name)
				{
					OnPicked(*Name);
					FSlateApplication::Get().DismissAllMenus();
				})
			]
		];
}

TSharedRef<SWidget> SVekaInspector::MakeActionsEditor(int32 Node, int32 Modifier)
{
	const FVekaDocument* Document = Context->GetDocument();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const FVekaAstNode& N = Document->Nodes[Node];
	const TArray<FVekaAstAction>& InActions = Modifier == INDEX_NONE ? N.Actions : N.Modifiers[Modifier].Actions;
	const TArray<FText> KindLabels = { LOCTEXT("ActionSet", "set"), LOCTEXT("ActionToggle", "toggle"), LOCTEXT("ActionIncrement", "add"), LOCTEXT("ActionEmit", "emit"),
		LOCTEXT("ActionPlay", "play"), LOCTEXT("ActionPause", "pause"), LOCTEXT("ActionStop", "stop"), LOCTEXT("ActionReverse", "reverse"), LOCTEXT("ActionSeek", "seek") };
	const TArray<EVekaActionKind> Kinds = { EVekaActionKind::Set, EVekaActionKind::Toggle, EVekaActionKind::Increment, EVekaActionKind::Emit, EVekaActionKind::Play,
		EVekaActionKind::Pause, EVekaActionKind::Stop, EVekaActionKind::Reverse, EVekaActionKind::Seek };
	auto EditActions = [this, Node, Modifier](const FText& Description, TFunction<void(TArray<FVekaAstAction>&)> Change)
	{
		Context->Edit(Description, [Node, Modifier, &Change](FVekaDocument& Doc) -> TArray<int32>
		{
			if (TArray<FVekaAstAction>* List = Doc::Actions(Doc, Node, Modifier))
			{
				Change(*List);
			}
			return { Node };
		});
	};
	for (int32 Index = 0; Index < InActions.Num(); ++Index)
	{
		const FVekaAstAction& Action = InActions[Index];
		TArray<FText> Targets;
		TArray<FName> TargetNames;
		if (Action.Kind == EVekaActionKind::Emit)
		{
			for (const FVekaEventDeclaration& Event : Document->EventDeclarations)
			{
				Targets.Add(FText::FromName(Event.Name));
				TargetNames.Add(Event.Name);
			}
		}
		else if (Action.Kind >= EVekaActionKind::Play)
		{
			for (const FVekaDocumentTimeline& Timeline : Document->Timelines)
			{
				Targets.Add(FText::FromName(Timeline.Name));
				TargetNames.Add(Timeline.Name);
			}
		}
		else
		{
			for (const FVekaStateDeclaration& State : Document->States)
			{
				Targets.Add(FText::FromName(State.Name));
				TargetNames.Add(State.Name);
			}
		}
		const FString Key = RowKey(Node, Modifier, FName(*FString::Printf(TEXT("action%d"), Index)));
		const bool bValue = Action.Kind == EVekaActionKind::Set || Action.Kind == EVekaActionKind::Increment || Action.Kind == EVekaActionKind::Emit
			|| Action.Kind == EVekaActionKind::Seek;
		Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeChoice(KindLabels[Kinds.IndexOfByKey(Action.Kind)], KindLabels, [EditActions, Index, Kinds](int32 Picked)
				{
					EditActions(LOCTEXT("SetActionKind", "Change action"), [Index, Kind = Kinds[Picked]](TArray<FVekaAstAction>& List) { List[Index].Kind = Kind; List[Index].Target = NAME_None; });
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f))
			[
				MakeChoice(Action.Target.IsNone() ? LOCTEXT("PickTarget", "(pick)") : FText::FromName(Action.Target), Targets, [EditActions, Index, TargetNames](int32 Picked)
				{
					EditActions(LOCTEXT("SetActionTarget", "Change action"), [Index, Target = TargetNames[Picked]](TArray<FVekaAstAction>& List) { List[Index].Target = Target; });
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				bValue ? MakeCodeBox(Key, Veka::Lang::PrintExpression(*Document, Action.Expr), [this, Node, Modifier, Index, Key](const FString& Code)
				{
					return EditCodeExpression(LOCTEXT("SetActionValue", "Change action"), Key, Code, Node, Modifier, [Node, Modifier, Index](FVekaDocument& Doc, int32 Expr)
					{
						if (TArray<FVekaAstAction>* List = Doc::Actions(Doc, Node, Modifier))
						{
							(*List)[Index].Expr = Expr;
						}
					});
				}) : SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f))
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("AnimatedTooltip", "Animated (withAnimation)"))
				.IsChecked(Action.bAnimated ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([EditActions, Index](ECheckBoxState State)
				{
					EditActions(LOCTEXT("SetAnimated", "Change action"), [Index, State](TArray<FVekaAstAction>& List) { List[Index].bAnimated = State == ECheckBoxState::Checked; });
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.Text(LOCTEXT("RemoveAction", "x"))
				.OnClicked_Lambda([EditActions, Index]()
				{
					EditActions(LOCTEXT("RemoveActionStep", "Remove action"), [Index](TArray<FVekaAstAction>& List) { List.RemoveAt(Index); });
					return FReply::Handled();
				})
			]
		];
	}
	Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
	[
		SNew(SButton)
		.Text(LOCTEXT("AddAction", "+ Action"))
		.OnClicked_Lambda([EditActions, Document]()
		{
			EditActions(LOCTEXT("AddActionStep", "Add action"), [Document](TArray<FVekaAstAction>& List)
			{
				FVekaAstAction& Action = List.AddDefaulted_GetRef();
				Action.Kind = Document->EventDeclarations.IsEmpty() ? EVekaActionKind::Toggle : EVekaActionKind::Emit;
				Action.Target = Action.Kind == EVekaActionKind::Emit ? Document->EventDeclarations[0].Name : (Document->States.IsEmpty() ? NAME_None : Document->States[0].Name);
			});
			return FReply::Handled();
		})
	];
	return Box;
}

TSharedRef<SWidget> SVekaInspector::MakeModifierSection(int32 Node)
{
	const FVekaDocument* Document = Context->GetDocument();
	const FVekaAstNode& N = Document->Nodes[Node];
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Index = 0; Index < N.Modifiers.Num(); ++Index)
	{
		const FVekaAstModifier& Modifier = N.Modifiers[Index];
		const FVekaCatalogEntry* Entry = Veka::Catalog::FindModifier(Modifier.Name);
		const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(Modifier.Name, true);
		auto Step = [this, Node, Index](int32 Delta)
		{
			Context->Edit(LOCTEXT("MoveModifier", "Reorder modifiers"), [Node, Index, Delta](FVekaDocument& Doc) -> TArray<int32>
			{
				Doc::MoveModifier(Doc, Node, Index, Index + Delta);
				return { Node };
			});
			return FReply::Handled();
		};
		TArray<FVekaCatalogArgument> Arguments;
		if (Entry)
		{
			Arguments = Entry->Arguments;
		}
		else
		{
			for (const FVekaAstArgument& Given : Modifier.Arguments)
			{
				FVekaCatalogArgument Argument;
				Argument.Name = Given.Name;
				Arguments.Add(Argument);
			}
		}
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		if (!Arguments.IsEmpty())
		{
			Body->AddSlot().AutoHeight()[MakeArgumentsGrid(Node, Index, Arguments, Modifier.Name)];
		}
		if (Entry && Entry->bActions)
		{
			Body->AddSlot().AutoHeight()[MakeActionsEditor(Node, Index)];
		}
		if (Arguments.IsEmpty() && !(Entry && Entry->bActions))
		{
			Body->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Look::Subdued()).AutoWrapText(true).Text(Doc ? Doc->Description : FText::GetEmpty())];
		}
		const TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Look::IconButton(TEXT("chevron.up"), LOCTEXT("UpTooltip", "Move up: applied earlier, closer to the view"), [Step]() { return Step(-1); }, Index > 0)]
			+ SHorizontalBox::Slot().AutoWidth()[Look::IconButton(TEXT("chevron.down"), LOCTEXT("DownTooltip", "Move down: applied later, around what is above"), [Step]() { return Step(1); }, Index < N.Modifiers.Num() - 1)]
			+ SHorizontalBox::Slot().AutoWidth()[Look::IconButton(TEXT("minus"), LOCTEXT("RemoveModifierTooltip", "Remove"), [this, Node, Index]()
			{
				Context->Edit(LOCTEXT("RemoveModifierStep", "Remove modifier"), [Node, Index](FVekaDocument& Doc) -> TArray<int32>
				{
					Doc::RemoveModifier(Doc, Node, Index);
					return { Node };
				});
				return FReply::Handled();
			})];
		Box->AddSlot().AutoHeight()
		[
			Look::Section(Doc ? Doc->Title : FText::FromString(TEXT(".") + Modifier.Name.ToString()), Doc ? Doc->Icon : FName(TEXT("wand.and.stars")),
				Doc ? FText::Format(LOCTEXT("ModifierTooltip", "{0}\n.{1} in the code. Modifiers apply from top to bottom."), Doc->Description, FText::FromName(Modifier.Name)) : FText::GetEmpty(),
				Body, Buttons)
		];
	}

	// Add a modifier: a menu by family, each with its icon and what it does.
	Box->AddSlot().AutoHeight().Padding(FMargin(Look::SectionPadding, 10.f, Look::SectionPadding, 16.f))
	[
		SNew(SComboButton)
		.HasDownArrow(false)
		.ToolTipText(LOCTEXT("AddModifierTooltip", "Size, padding, colors, text style, effects, animations, clicks..."))
		.OnGetMenuContent_Lambda([this, Node]()
		{
			FMenuBuilder Menu(true, nullptr);
			static const TCHAR* Families[] = { TEXT("Layout"), TEXT("Style"), TEXT("Text"), TEXT("Controls"), TEXT("Effects"), TEXT("Animation"), TEXT("Interaction") };
			for (const TCHAR* Family : Families)
			{
				Menu.BeginSection(NAME_None, FText::FromString(Family));
				for (const FVekaCatalogEntry& Entry : Veka::Catalog::Modifiers())
				{
					const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(Entry.Name, true);
					if (!Doc || Doc->Category.ToString() != Family)
					{
						continue;
					}
					const FName Name = Entry.Name;
					Menu.AddMenuEntry(FUIAction(FExecuteAction::CreateLambda([this, Node, Name]()
						{
							Context->Edit(LOCTEXT("AddModifierStep", "Add modifier"), [Node, Name](FVekaDocument& Doc) -> TArray<int32>
							{
								Doc::AddModifier(Doc, Node, Name);
								return { Node };
							});
						})),
						SNew(SHorizontalBox)
						.ToolTipText(Doc->Description)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[Look::Icon(Doc->Icon, 14.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(Doc->Title)]);
				}
				Menu.EndSection();
			}
			return Menu.MakeWidget();
		})
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))[Look::Icon(TEXT("plus"), 12.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("AddModifier", "Add a modifier"))]
		]
	];
	return Box;
}

// --- State tab -------------------------------------------------------------------------------------------------

void SVekaStatePanel::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, TFunction<UObject*()> InGetModel)
{
	Context = InContext;
	GetModel = MoveTemp(InGetModel);
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	ModelDetails = PropertyEditor.CreateDetailView(Args);
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f)
		[
			SAssignNew(Content, SVerticalBox)
		]
	];
	DocumentHandle = Context->OnDocumentChanged.AddSP(this, &SVekaStatePanel::Rebuild);
	Rebuild();
}

SVekaStatePanel::~SVekaStatePanel()
{
	if (Context.IsValid())
	{
		Context->OnDocumentChanged.Remove(DocumentHandle);
	}
}

TSharedRef<SWidget> SVekaStatePanel::MakeDeclarationRow(FName Name, const FText& Kind, EVekaValueType Type, const FVekaValue* Value, bool bTyped)
{
	const FString Key = Name.ToString();
	const FText* Error = Errors.Find(Key);
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.35f)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromName(Name))
			.ToolTipText(LOCTEXT("RenameDeclaration", "Rename (every use follows)"))
			.OnTextCommitted_Lambda([this, Name](const FText& Text, ETextCommit::Type)
			{
				const FName New(*Text.ToString().TrimStartAndEnd());
				if (New != Name)
				{
					Context->Edit(LOCTEXT("RenameDeclarationStep", "Rename"), [Name, New](FVekaDocument& Doc) -> TArray<int32>
					{
						Doc::RenameDeclaration(Doc, Name, New);
						return {};
					});
				}
			})
		];
	if (bTyped)
	{
		TArray<FText> Labels;
		for (const EVekaValueType Item : ValueTypes())
		{
			Labels.Add(FText::FromString(Veka::Lang::TypeName(Item)));
		}
		Row->AddSlot().AutoWidth().Padding(FMargin(4.f, 0.f))
		[
			MakeChoice(FText::FromString(Veka::Lang::TypeName(Type)), Labels, [this, Name](int32 Index)
			{
				const EVekaValueType NewType = ValueTypes()[Index];
				Context->Edit(LOCTEXT("SetTypeStep", "Change type"), [Name, NewType](FVekaDocument& Doc) -> TArray<int32>
				{
					for (FVekaStateDeclaration& State : Doc.States)
					{
						if (State.Name == Name)
						{
							State.Initial = DefaultValue(NewType);
						}
					}
					for (FVekaParamDeclaration& Param : Doc.Params)
					{
						if (Param.Name == Name)
						{
							Param.Type = NewType;
							Param.Default = DefaultValue(NewType);
						}
					}
					for (FVekaEventDeclaration& Event : Doc.EventDeclarations)
					{
						if (Event.Name == Name)
						{
							Event.PayloadType = NewType;
						}
					}
					return {};
				});
			})
		];
	}
	if (Value)
	{
		Row->AddSlot().FillWidth(0.4f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SEditableTextBox)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(FText::FromString(Value->IsNone() ? FString() : Veka::Lang::PrintValue(*Value)))
				.HintText(LOCTEXT("ValueHint", "initial value"))
				.OnTextCommitted_Lambda([this, Name, Key](const FText& Text, ETextCommit::Type)
				{
					FVekaValue Parsed;
					TArray<FVekaDiagnostic> Diagnostics;
					if (!Text.IsEmpty() && !Veka::Lang::ParseValue(Text.ToString(), Context->GetTextNamespace(), Parsed, Diagnostics))
					{
						Errors.Add(Key, Diagnostics.IsEmpty() ? LOCTEXT("BadValue", "Not a constant") : Diagnostics[0].Message);
						Rebuild();
						return;
					}
					Errors.Remove(Key);
					Context->Edit(LOCTEXT("SetValueStep", "Change value"), [Name, &Parsed](FVekaDocument& Doc) -> TArray<int32>
					{
						for (FVekaStateDeclaration& State : Doc.States)
						{
							if (State.Name == Name)
							{
								State.Initial = Parsed;
							}
						}
						for (FVekaParamDeclaration& Param : Doc.Params)
						{
							if (Param.Name == Name)
							{
								Param.Default = Parsed;
							}
						}
						return {};
					});
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Visibility(Error ? EVisibility::Visible : EVisibility::Collapsed).ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.4f))
				.Text(Error ? *Error : FText::GetEmpty())
			]
		];
	}
	Row->AddSlot().AutoWidth()
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.Text(LOCTEXT("RemoveDeclaration", "x"))
		.ToolTipText(FText::Format(LOCTEXT("RemoveDeclarationTooltip", "Remove this {0}"), Kind))
		.OnClicked_Lambda([this, Name]()
		{
			Context->Edit(LOCTEXT("RemoveDeclarationStep", "Remove"), [Name](FVekaDocument& Doc) -> TArray<int32>
			{
				Doc::RemoveDeclaration(Doc, Name);
				return {};
			});
			return FReply::Handled();
		})
	];
	return Row;
}

void SVekaStatePanel::Rebuild()
{
	Content->ClearChildren();
	const FVekaDocument* Document = Context->GetDocument();
	if (!Document)
	{
		return;
	}
	TSharedRef<SVerticalBox> Declarations = SNew(SVerticalBox).IsEnabled_Lambda([this]() { return !Context->IsReadOnly(); });
	Content->AddSlot().AutoHeight()[Declarations];
	auto Section = [&Declarations](const FText& Title, TFunction<void()> OnAdd)
	{
		Declarations->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("SmallFontBold"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("AddDeclaration", "+ Add")).OnClicked_Lambda([OnAdd]() { OnAdd(); return FReply::Handled(); })
			]
		];
	};
	Section(LOCTEXT("States", "States"), [this]()
	{
		Context->Edit(LOCTEXT("AddStateStep", "Add state"), [](FVekaDocument& Doc) -> TArray<int32>
		{
			Doc::AddState(Doc, Doc::UniqueName(Doc, TEXT("state")), FVekaValue::Bool(false));
			return {};
		});
	});
	for (const FVekaStateDeclaration& State : Document->States)
	{
		Declarations->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[MakeDeclarationRow(State.Name, LOCTEXT("StateKind", "state"), State.Initial.Type, &State.Initial, true)];
	}
	Section(LOCTEXT("Params", "Params (given by the caller)"), [this]()
	{
		Context->Edit(LOCTEXT("AddParamStep", "Add param"), [](FVekaDocument& Doc) -> TArray<int32>
		{
			Doc::AddParam(Doc, Doc::UniqueName(Doc, TEXT("title")), EVekaValueType::Text, FVekaValue::Text(FText::GetEmpty()));
			return {};
		});
	});
	for (const FVekaParamDeclaration& Param : Document->Params)
	{
		Declarations->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[MakeDeclarationRow(Param.Name, LOCTEXT("ParamKind", "param"), Param.Type, &Param.Default, true)];
	}
	Section(LOCTEXT("Events", "Events (sent to the caller)"), [this]()
	{
		Context->Edit(LOCTEXT("AddEventStep", "Add event"), [](FVekaDocument& Doc) -> TArray<int32>
		{
			Doc::AddEvent(Doc, Doc::UniqueName(Doc, TEXT("tapped")), EVekaValueType::None);
			return {};
		});
	});
	for (const FVekaEventDeclaration& Event : Document->EventDeclarations)
	{
		Declarations->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[MakeDeclarationRow(Event.Name, LOCTEXT("EventKind", "event"), Event.PayloadType, nullptr, true)];
	}
	Section(LOCTEXT("Slots", "Slots (content given by the caller)"), [this]()
	{
		Context->Edit(LOCTEXT("AddSlotStep", "Add slot"), [](FVekaDocument& Doc) -> TArray<int32>
		{
			Doc::AddSlot(Doc, Doc::UniqueName(Doc, TEXT("content")));
			return {};
		});
	});
	for (const FVekaSlotDeclaration& Slot : Document->Slots)
	{
		Declarations->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[MakeDeclarationRow(Slot.Name, LOCTEXT("SlotKind", "slot"), EVekaValueType::None, nullptr, false)];
	}

	// Mock values of the preview model.
	Content->AddSlot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 4.f))
	[
		SNew(STextBlock).Text(LOCTEXT("MockValues", "Preview model (mock values, not saved)")).Font(FAppStyle::GetFontStyle("SmallFontBold"))
	];
	UObject* Model = GetModel ? GetModel() : nullptr;
	ModelDetails->SetObject(Model);
	Content->AddSlot().AutoHeight()
	[
		Model ? StaticCastSharedRef<SWidget>(ModelDetails.ToSharedRef())
			: StaticCastSharedRef<SWidget>(SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("NoModel", "No preview model: set Preview Model Class in Details to give model.X values.")))
	];
}

#undef LOCTEXT_NAMESPACE
