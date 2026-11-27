// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/SVekaOutliner.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "SimpleMode/VekaEditorLook.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "View/VekaComponentLibrary.h"
#include "Components/VekaComponentAssets.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "VekaOutliner"

TSharedRef<FVekaNodeDragDrop> FVekaNodeDragDrop::New(const TArray<int32>& InNodes, FName InType, UVekaView* InComponent, const FText& InLabel)
{
	TSharedRef<FVekaNodeDragDrop> Operation = MakeShared<FVekaNodeDragDrop>();
	Operation->Nodes = InNodes;
	Operation->NewType = InType;
	Operation->NewComponent = InComponent;
	Operation->Label = InLabel;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FVekaNodeDragDrop::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(STextBlock).Text(Label)
		];
}

namespace
{
	FString PathKey(const TArray<int32>& Path)
	{
		FString Key;
		for (const int32 Step : Path)
		{
			Key += FString::Printf(TEXT("%d/"), Step);
		}
		return Key;
	}
}

namespace Veka::Editor
{
	bool InsertAtSelection(FVekaViewEditorContext& Context, FName Type, UVekaView* Component)
	{
		const FVekaDocument* Current = Context.GetDocument();
		if (!Current)
		{
			return false;
		}
		const int32 Selected = Context.GetPrimarySelection();
		const FString Namespace = Context.GetTextNamespace();
		return Context.Edit(LOCTEXT("InsertNode", "Add view"), [&](FVekaDocument& Document) -> TArray<int32>
		{
			if (!Document.Nodes.IsValidIndex(Document.Root))
			{
				return { Doc::Insert(Document, Type, INDEX_NONE, false, INDEX_NONE, Component, Namespace) };
			}
			if (Document.Nodes.IsValidIndex(Selected) && Doc::CanHaveChildren(Document, Selected))
			{
				return { Doc::Insert(Document, Type, Selected, false, INDEX_NONE, Component, Namespace) };
			}
			const FVekaNodeLocation Location = Doc::FindLocation(Document, Selected);
			if (Document.Nodes.IsValidIndex(Selected) && !Location.IsRoot() && Location.Index != INDEX_NONE)
			{
				return { Doc::Insert(Document, Type, Location.Parent, Location.bElse, Location.Index + 1, Component, Namespace) };
			}
			if (Doc::CanHaveChildren(Document, Document.Root))
			{
				return { Doc::Insert(Document, Type, Document.Root, false, INDEX_NONE, Component, Namespace) };
			}
			// The root cannot hold children: put it and the new view in a VStack.
			const int32 Stack = Doc::Wrap(Document, { Document.Root }, TEXT("VStack"));
			return { Doc::Insert(Document, Type, Stack, false, INDEX_NONE, Component, Namespace) };
		});
	}

	bool DropOnItem(FVekaViewEditorContext& Context, const FVekaNodeDragDrop& Operation, const FVekaOutlinerItem& Target, EItemDropZone Zone)
	{
		const FVekaDocument* Current = Context.GetDocument();
		if (!Current || !Current->Nodes.IsValidIndex(Target.Node))
		{
			return false;
		}
		// Where the drop puts things: a parent list and an index in it.
		int32 Parent = INDEX_NONE;
		bool bElse = false;
		int32 Index = INDEX_NONE;
		const FVekaNodeLocation Location = Doc::FindLocation(*Current, Target.Node);
		const bool bOnto = Zone == EItemDropZone::OntoItem || Target.bElseGroup || Location.IsRoot();
		if (bOnto)
		{
			if (Target.bElseGroup)
			{
				Parent = Target.Node;
				bElse = true;
			}
			else if (Doc::CanHaveChildren(*Current, Target.Node))
			{
				Parent = Target.Node;
			}
			else if (!Location.IsRoot())
			{
				Parent = Location.Parent;
				bElse = Location.bElse;
				Index = Location.Index + 1;
			}
			else
			{
				return false;
			}
		}
		else
		{
			Parent = Location.Parent;
			bElse = Location.bElse;
			Index = Location.Index + (Zone == EItemDropZone::BelowItem ? 1 : 0);
		}
		for (const int32 Node : Operation.Nodes)
		{
			if (Doc::IsInside(*Current, Parent, Node))
			{
				return false;
			}
		}
		const FString Namespace = Context.GetTextNamespace();
		return Context.Edit(Operation.Nodes.IsEmpty() ? LOCTEXT("DropNew", "Add view") : LOCTEXT("MoveNodes", "Move views"), [&](FVekaDocument& Document) -> TArray<int32>
		{
			if (Operation.Nodes.IsEmpty())
			{
				return { Doc::Insert(Document, Operation.NewType, Parent, bElse, Index, Operation.NewComponent.Get(), Namespace) };
			}
			int32 At = Index;
			for (const int32 Node : Operation.Nodes)
			{
				const FVekaNodeLocation From = Doc::FindLocation(Document, Node);
				const bool bSameList = From.Parent == Parent && From.bElse == bElse;
				if (Doc::Move(Document, Node, Parent, bElse, At) && At != INDEX_NONE)
				{
					const FVekaNodeLocation Now = Doc::FindLocation(Document, Node);
					At = Now.Index + 1;
				}
				(void)bSameList;
			}
			return Operation.Nodes;
		});
	}
}

void SVekaOutliner::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext)
{
	Context = InContext;
	TWeakPtr<FVekaViewEditorContext> WeakContext = InContext;
	ChildSlot
	[
		SNew(SVerticalBox)
		// Header: the view's layers and the button that adds one (the palette, searchable, in a popup).
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.f, 6.f, 6.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
				.Text(LOCTEXT("OutlinerTitle", "Views"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.HasDownArrow(false)
				.ToolTipText(LOCTEXT("AddTooltip", "Add a component, a shape or a view of the project inside the selection (or after it)"))
				.IsEnabled_Lambda([WeakContext]() { const TSharedPtr<FVekaViewEditorContext> Pinned = WeakContext.Pin(); return Pinned && !Pinned->IsReadOnly(); })
				.OnGetMenuContent_Lambda([WeakContext]() -> TSharedRef<SWidget>
				{
					const TSharedPtr<FVekaViewEditorContext> Pinned = WeakContext.Pin();
					if (!Pinned)
					{
						return SNullWidget::NullWidget;
					}
					return SNew(SBox).WidthOverride(300.f).HeightOverride(460.f)[SNew(SVekaPalette, Pinned.ToSharedRef()).PickOnClick(true)];
				})
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Plus")).ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("Add", "Add"))
					]
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(Tree, STreeView<TSharedPtr<FVekaOutlinerItem>>)
			.TreeItemsSource(&RootItems)
			.SelectionMode(ESelectionMode::Multi)
			.OnGenerateRow(this, &SVekaOutliner::MakeRow)
			.OnGetChildren(this, &SVekaOutliner::GetItemChildren)
			.OnSelectionChanged(this, &SVekaOutliner::HandleTreeSelection)
			.OnContextMenuOpening(this, &SVekaOutliner::MakeContextMenu)
			.OnExpansionChanged_Lambda([this](TSharedPtr<FVekaOutlinerItem> Item, bool bExpanded)
			{
				const FVekaDocument* Document = Context->GetDocument();
				if (!bSyncing && Document && Item.IsValid())
				{
					const FString Key = PathKey(Veka::Editor::Doc::Path(*Document, Item->Node)) + (Item->bElseGroup ? TEXT("else") : TEXT(""));
					if (bExpanded)
					{
						Collapsed.Remove(Key);
					}
					else
					{
						Collapsed.Add(Key);
					}
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return RootItems.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("Empty", "Empty view: click + Add to start."))
		]
	];
	DocumentHandle = Context->OnDocumentChanged.AddSP(this, &SVekaOutliner::Rebuild);
	SelectionHandle = Context->OnSelectionChanged.AddSP(this, &SVekaOutliner::HandleContextSelection);
	Rebuild();
}

SVekaOutliner::~SVekaOutliner()
{
	if (Context.IsValid())
	{
		Context->OnDocumentChanged.Remove(DocumentHandle);
		Context->OnSelectionChanged.Remove(SelectionHandle);
	}
}

void SVekaOutliner::Rebuild()
{
	RootItems.Reset();
	ItemsByNode.Reset();
	const FVekaDocument* Document = Context->GetDocument();
	TArray<TSharedPtr<FVekaOutlinerItem>> All;
	TFunction<TSharedPtr<FVekaOutlinerItem>(int32)> Build = [&](int32 Node) -> TSharedPtr<FVekaOutlinerItem>
	{
		TSharedPtr<FVekaOutlinerItem> Item = MakeShared<FVekaOutlinerItem>();
		Item->Node = Node;
		ItemsByNode.Add(Node, Item);
		All.Add(Item);
		const FVekaAstNode& N = Document->Nodes[Node];
		for (const int32 Child : N.Children)
		{
			if (Document->Nodes.IsValidIndex(Child))
			{
				Item->Children.Add(Build(Child));
			}
		}
		if (N.Kind == EVekaAstNodeKind::If)
		{
			TSharedPtr<FVekaOutlinerItem> Else = MakeShared<FVekaOutlinerItem>();
			Else->Node = Node;
			Else->bElseGroup = true;
			All.Add(Else);
			for (const int32 Child : N.ElseChildren)
			{
				if (Document->Nodes.IsValidIndex(Child))
				{
					Else->Children.Add(Build(Child));
				}
			}
			Item->Children.Add(Else);
		}
		return Item;
	};
	if (Document && Document->Nodes.IsValidIndex(Document->Root))
	{
		RootItems.Add(Build(Document->Root));
	}
	TGuardValue<bool> Guard(bSyncing, true);
	Tree->RequestTreeRefresh();
	for (const TSharedPtr<FVekaOutlinerItem>& Item : All)
	{
		const FString Key = PathKey(Veka::Editor::Doc::Path(*Document, Item->Node)) + (Item->bElseGroup ? TEXT("else") : TEXT(""));
		Tree->SetItemExpansion(Item, !Collapsed.Contains(Key));
	}
	HandleContextSelection();
}

int32 SVekaOutliner::GetItemCount() const
{
	return ItemsByNode.Num();
}

void SVekaOutliner::GetItemChildren(TSharedPtr<FVekaOutlinerItem> Item, TArray<TSharedPtr<FVekaOutlinerItem>>& OutChildren)
{
	OutChildren = Item->Children;
}

TSharedPtr<FVekaOutlinerItem> SVekaOutliner::FindItem(int32 Node) const
{
	const TSharedPtr<FVekaOutlinerItem>* Found = ItemsByNode.Find(Node);
	return Found ? *Found : nullptr;
}

TSharedRef<ITableRow> SVekaOutliner::MakeRow(TSharedPtr<FVekaOutlinerItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	const FVekaDocument* Document = Context->GetDocument();
	const int32 Node = Item->Node;
	TSharedPtr<SWidget> Content;
	if (Item->bElseGroup)
	{
		Content = SNew(STextBlock).Text(LOCTEXT("Else", "else")).ColorAndOpacity(FLinearColor(0.80f, 0.47f, 0.95f));
	}
	else
	{
		const Veka::Editor::Look::FNodeLook Look = Veka::Editor::Look::DescribeNode(*Document, Node);
		const FVekaAstNode& N = Document->Nodes[Node];
		const bool bLabeled = !N.Comments.IsEmpty() && !N.Comments[0].IsEmpty();
		// A label replaces the type's name; the type then shows as the detail.
		const FText Main = bLabeled ? FText::FromString(N.Comments[0]) : Look.Title;
		const FText Detail = bLabeled ? Look.Title : Look.Detail;
		Content = SNew(SHorizontalBox)
			.ToolTipText(FText::Format(LOCTEXT("RowTooltip", "{0}\n\nDouble-click or F2 to give it a name. Drag to move it, right-click for more."), Look.Description))
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				Veka::Editor::Look::Icon(Look.Icon, 14.f, Look.Color)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
				.Text(Main)
				.IsSelected_Lambda([this, Item]() { return Tree->IsItemSelected(Item); })
				.OnTextCommitted_Lambda([this, Node](const FText& Text, ETextCommit::Type Commit)
				{
					if (Commit != ETextCommit::OnCleared)
					{
						const FString Label = Text.ToString();
						Context->Edit(LOCTEXT("Rename", "Rename view"), [Node, &Label](FVekaDocument& Document) -> TArray<int32>
						{
							Veka::Editor::Doc::SetLabel(Document, Node, Label);
							return { Node };
						});
					}
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 4.f, 0.f))
			[
				SNew(STextBlock).Text(Detail).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(Veka::Editor::Look::Subdued())
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}

	return SNew(STableRow<TSharedPtr<FVekaOutlinerItem>>, Owner)
		.Padding(FMargin(2.f, 3.f))
		.OnDragDetected_Lambda([this, Item](const FGeometry&, const FPointerEvent& Event) -> FReply
		{
			if (Item->bElseGroup || !Event.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				return FReply::Unhandled();
			}
			TArray<int32> Nodes;
			for (const TSharedPtr<FVekaOutlinerItem>& Selected : Tree->GetSelectedItems())
			{
				if (!Selected->bElseGroup)
				{
					Nodes.Add(Selected->Node);
				}
			}
			if (!Nodes.Contains(Item->Node))
			{
				Nodes = { Item->Node };
			}
			const FVekaDocument* Document = Context->GetDocument();
			if (!Document || Nodes.Contains(Document->Root))
			{
				return FReply::Unhandled();
			}
			const FText Label = FText::FromString(Veka::Editor::Doc::DisplayName(*Document, Nodes[0]) + (Nodes.Num() > 1 ? FString::Printf(TEXT(" +%d"), Nodes.Num() - 1) : FString()));
			return FReply::Handled().BeginDragDrop(FVekaNodeDragDrop::New(Nodes, NAME_None, nullptr, Label));
		})
		.OnCanAcceptDrop_Lambda([this](const FDragDropEvent& Event, EItemDropZone Zone, TSharedPtr<FVekaOutlinerItem> Target) -> TOptional<EItemDropZone>
		{
			const TSharedPtr<FVekaNodeDragDrop> Operation = Event.GetOperationAs<FVekaNodeDragDrop>();
			const FVekaDocument* Document = Context->GetDocument();
			if (!Operation.IsValid() || !Document || Context->IsReadOnly())
			{
				return {};
			}
			for (const int32 Node : Operation->Nodes)
			{
				if (Veka::Editor::Doc::IsInside(*Document, Target->Node, Node) && !(Target->Node == Node && Zone != EItemDropZone::OntoItem))
				{
					return {};
				}
			}
			const bool bContainer = Target->bElseGroup || Veka::Editor::Doc::CanHaveChildren(*Document, Target->Node);
			if (Target->Node == Document->Root || Target->bElseGroup)
			{
				return bContainer ? TOptional<EItemDropZone>(EItemDropZone::OntoItem) : TOptional<EItemDropZone>();
			}
			return Zone == EItemDropZone::OntoItem && !bContainer ? EItemDropZone::BelowItem : Zone;
		})
		.OnAcceptDrop_Lambda([this](const FDragDropEvent& Event, EItemDropZone Zone, TSharedPtr<FVekaOutlinerItem> Target) -> FReply
		{
			const TSharedPtr<FVekaNodeDragDrop> Operation = Event.GetOperationAs<FVekaNodeDragDrop>();
			return Operation.IsValid() && Veka::Editor::DropOnItem(*Context, *Operation, *Target, Zone) ? FReply::Handled() : FReply::Unhandled();
		})
		[
			Content.ToSharedRef()
		];
}

void SVekaOutliner::HandleTreeSelection(TSharedPtr<FVekaOutlinerItem> Item, ESelectInfo::Type Info)
{
	// Direct: set by code (our own sync, or the tree dropping items it no longer has after a refresh).
	if (bSyncing || Info == ESelectInfo::Direct)
	{
		return;
	}
	TArray<int32> Nodes;
	for (const TSharedPtr<FVekaOutlinerItem>& Selected : Tree->GetSelectedItems())
	{
		if (!Selected->bElseGroup)
		{
			Nodes.Add(Selected->Node);
		}
	}
	TGuardValue<bool> Guard(bSyncing, true);
	Context->Select(Nodes, EVekaSelectionSource::Outliner);
}

void SVekaOutliner::HandleContextSelection()
{
	if (bSyncing && Context->GetLastSelectionSource() == EVekaSelectionSource::Outliner)
	{
		return;
	}
	TGuardValue<bool> Guard(bSyncing, true);
	Tree->ClearSelection();
	for (const int32 Node : Context->GetSelectedNodes())
	{
		if (const TSharedPtr<FVekaOutlinerItem> Item = FindItem(Node))
		{
			Tree->SetItemSelection(Item, true, ESelectInfo::Direct);
			Tree->RequestScrollIntoView(Item);
		}
	}
}

TSharedPtr<SWidget> SVekaOutliner::MakeContextMenu()
{
	const FVekaDocument* Document = Context->GetDocument();
	const int32 Selected = Context->GetPrimarySelection();
	if (!Document || Selected == INDEX_NONE || Context->IsReadOnly())
	{
		return nullptr;
	}
	FMenuBuilder Menu(true, nullptr);
	if (Veka::Editor::Doc::CanHaveChildren(*Document, Selected) || true)
	{
		Menu.AddSubMenu(LOCTEXT("Add", "Add"), LOCTEXT("AddTooltip", "Add a view inside the selection (or after it)"), FNewMenuDelegate::CreateLambda([this](FMenuBuilder& Sub)
		{
			for (const FVekaPaletteEntry& Entry : SVekaPalette::GatherEntries(Context->GetView()))
			{
				const FName Type = Entry.Type;
				UVekaView* Component = Entry.Component.Get();
				Sub.AddMenuEntry(Entry.Label, Entry.Category, FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Type, Component]()
				{
					Veka::Editor::InsertAtSelection(*Context, Type, Component);
				})));
			}
		}));
	}
	Menu.AddSubMenu(LOCTEXT("WrapIn", "Wrap in"), FText::GetEmpty(), FNewMenuDelegate::CreateLambda([this](FMenuBuilder& Sub)
	{
		for (const TCHAR* Container : { TEXT("VStack"), TEXT("HStack"), TEXT("ZStack") })
		{
			const FName Type(Container);
			Sub.AddMenuEntry(FText::FromName(Type), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Type]() { WrapSelection(Type); })));
		}
	}));
	Menu.AddMenuEntry(LOCTEXT("Duplicate", "Duplicate"), LOCTEXT("DuplicateTooltip", "Ctrl+D"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SVekaOutliner::DuplicateSelection)));
	Menu.AddMenuEntry(LOCTEXT("Delete", "Delete"), LOCTEXT("DeleteTooltip", "Delete"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SVekaOutliner::DeleteSelection)));
	return Menu.MakeWidget();
}

void SVekaOutliner::DeleteSelection()
{
	const TArray<int32> Nodes = Context->GetSelectedNodes();
	if (!Nodes.IsEmpty())
	{
		Context->Edit(LOCTEXT("DeleteNodes", "Delete views"), [&Nodes](FVekaDocument& Document) -> TArray<int32>
		{
			Veka::Editor::Doc::Remove(Document, Nodes);
			return {};
		});
		Context->Select({}, EVekaSelectionSource::Outliner);
	}
}

void SVekaOutliner::DuplicateSelection()
{
	const TArray<int32> Nodes = Context->GetSelectedNodes();
	Context->Edit(LOCTEXT("DuplicateNodes", "Duplicate views"), [&Nodes](FVekaDocument& Document) -> TArray<int32>
	{
		TArray<int32> Copies;
		for (const int32 Node : Nodes)
		{
			const int32 Copy = Veka::Editor::Doc::Duplicate(Document, Node);
			if (Copy != INDEX_NONE)
			{
				Copies.Add(Copy);
			}
		}
		return Copies;
	});
}

void SVekaOutliner::WrapSelection(FName Container)
{
	const TArray<int32> Nodes = Context->GetSelectedNodes();
	const FVekaDocument* Document = Context->GetDocument();
	if (Nodes.IsEmpty() || !Document)
	{
		return;
	}
	// Only siblings can be wrapped together.
	const FVekaNodeLocation First = Veka::Editor::Doc::FindLocation(*Document, Nodes[0]);
	for (const int32 Node : Nodes)
	{
		const FVekaNodeLocation Location = Veka::Editor::Doc::FindLocation(*Document, Node);
		if (Location.Parent != First.Parent || Location.bElse != First.bElse)
		{
			return;
		}
	}
	Context->Edit(LOCTEXT("WrapNodes", "Wrap views"), [&Nodes, Container](FVekaDocument& Document) -> TArray<int32>
	{
		return { Veka::Editor::Doc::Wrap(Document, Nodes, Container) };
	});
}

FReply SVekaOutliner::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Context->IsReadOnly())
	{
		return FReply::Unhandled();
	}
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		DeleteSelection();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::D && InKeyEvent.IsControlDown())
	{
		DuplicateSelection();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::F2)
	{
		const TArray<TSharedPtr<FVekaOutlinerItem>> Selected = Tree->GetSelectedItems();
		if (Selected.Num() == 1)
		{
			if (const TSharedPtr<ITableRow> Row = Tree->WidgetFromItem(Selected[0]))
			{
				// The label editor is the row's inline text block.
				TArray<TSharedRef<SWidget>> Stack = { Row->AsWidget() };
				while (!Stack.IsEmpty())
				{
					const TSharedRef<SWidget> Widget = Stack.Pop();
					if (Widget->GetType() == TEXT("SInlineEditableTextBlock"))
					{
						StaticCastSharedRef<SInlineEditableTextBlock>(Widget)->EnterEditingMode();
						break;
					}
					FChildren* Children = Widget->GetChildren();
					for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
					{
						Stack.Add(Children->GetChildAt(Index));
					}
				}
			}
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// --- Palette -------------------------------------------------------------------------------------------------

TArray<FVekaPaletteEntry> SVekaPalette::GatherEntries(const UVekaView* Exclude)
{
	TArray<FVekaPaletteEntry> Out;
	auto Add = [&Out](const TCHAR* Type, const FText& Category)
	{
		FVekaPaletteEntry& Entry = Out.AddDefaulted_GetRef();
		Entry.Type = Type;
		Entry.Category = Category;
		Entry.IconColor = Veka::Editor::Look::FamilyColor(Category);
		if (const FVekaCatalogDoc* Doc = Veka::Catalog::FindDoc(Type, false))
		{
			Entry.Label = Doc->Title;
			Entry.Icon = Doc->Icon;
			Entry.Tooltip = Doc->Description;
			return;
		}
		// Logic.
		const FString Name = Type;
		Entry.Label = Name == TEXT("if") ? LOCTEXT("PaletteIf", "Condition") : Name == TEXT("ForEach") ? LOCTEXT("PaletteForEach", "Repeat") : LOCTEXT("PaletteSlot", "Slot");
		Entry.Icon = Name == TEXT("if") ? FName(TEXT("questionmark")) : Name == TEXT("ForEach") ? FName(TEXT("repeat")) : FName(TEXT("tray"));
		Entry.Tooltip = Name == TEXT("if") ? LOCTEXT("PaletteIfHelp", "Shows its views only when a condition is true.")
			: Name == TEXT("ForEach") ? LOCTEXT("PaletteForEachHelp", "Repeats its views a number of times, or once per item of a list.")
			: LOCTEXT("PaletteSlotHelp", "In a component: where the content given by its caller goes.");
	};
	const FText Layout = LOCTEXT("CategoryLayout", "Layout");
	const FText Content = LOCTEXT("CategoryContent", "Content");
	const FText Controls = LOCTEXT("CategoryControls", "Controls");
	const FText Shapes = LOCTEXT("CategoryShapes", "Shapes");
	const FText Logic = LOCTEXT("CategoryLogic", "Logic");
	for (const TCHAR* Type : { TEXT("VStack"), TEXT("HStack"), TEXT("ZStack"), TEXT("Spacer"), TEXT("Divider") })
	{
		Add(Type, Layout);
	}
	for (const TCHAR* Type : { TEXT("Text"), TEXT("Label"), TEXT("Symbol") })
	{
		Add(Type, Content);
	}
	for (const TCHAR* Type : { TEXT("Button"), TEXT("Toggle"), TEXT("Slider"), TEXT("Stepper"), TEXT("TextField"), TEXT("ProgressView"), TEXT("Gauge") })
	{
		Add(Type, Controls);
	}
	for (const TCHAR* Type : { TEXT("Rectangle"), TEXT("RoundedRectangle"), TEXT("Capsule"), TEXT("Circle"), TEXT("Ellipse") })
	{
		Add(Type, Shapes);
	}
	for (const TCHAR* Type : { TEXT("if"), TEXT("ForEach"), TEXT("Slot") })
	{
		Add(Type, Logic);
	}
	for (const Veka::Editor::Components::FComponentEntry& Component : Veka::Editor::Components::GatherComponents(Exclude))
	{
		FVekaPaletteEntry& Entry = Out.AddDefaulted_GetRef();
		Entry.Type = Component.View->GetFName();
		Entry.Label = FText::FromName(Entry.Type);
		Entry.Category = Component.Group;
		Entry.Icon = TEXT("cube");
		Entry.IconColor = Veka::Editor::Look::FamilyColor(LOCTEXT("Views", "Views"));
		Entry.Component = Component.View;
		Entry.Tooltip = FText::FromString(Veka::Editor::Components::DescribeComponent(*Component.View));
	}
	return Out;
}

SVekaPalette::~SVekaPalette()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		IAssetRegistry& Registry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.OnAssetAdded().Remove(AddedHandle);
		Registry.OnAssetRemoved().Remove(RemovedHandle);
		Registry.OnAssetRenamed().Remove(RenamedHandle);
	}
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ChangedHandle);
}

void SVekaPalette::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bStale || LibraryRevision != Veka::Components::GetRevision())
	{
		bStale = false;
		LibraryRevision = Veka::Components::GetRevision();
		Refresh();
	}
}

void SVekaPalette::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext)
{
	Context = InContext;
	bPickOnClick = InArgs._PickOnClick;
	LibraryRevision = Veka::Components::GetRevision();
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		auto IsView = [](const FAssetData& Asset) { return Asset.AssetClassPath == UVekaView::StaticClass()->GetClassPathName(); };
		AddedHandle = Registry.OnAssetAdded().AddSPLambda(this, [this, IsView](const FAssetData& Asset) { bStale |= IsView(Asset); });
		RemovedHandle = Registry.OnAssetRemoved().AddSPLambda(this, [this, IsView](const FAssetData& Asset) { bStale |= IsView(Asset); });
		RenamedHandle = Registry.OnAssetRenamed().AddSPLambda(this, [this, IsView](const FAssetData& Asset, const FString&) { bStale |= IsView(Asset); });
		// A view's @Param / @Slot changed (its tooltip), or a library's list.
		ChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSPLambda(this, [this](UObject* Object, FPropertyChangedEvent&)
		{
			bStale |= Object && (Object->IsA<UVekaView>() || Object->IsA<UVekaComponentLibrary>());
		});
	}
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SSearchBox)
			.OnTextChanged_Lambda([this](const FText& Text) { Filter = Text.ToString(); Refresh(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(List, SListView<TSharedPtr<FVekaPaletteEntry>>)
			.ListItemsSource(&Entries)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SVekaPalette::MakeRow)
			.OnMouseButtonDoubleClick_Lambda([this](TSharedPtr<FVekaPaletteEntry> Entry)
			{
				if (Entry.IsValid() && !Entry->bHeader)
				{
					Veka::Editor::InsertAtSelection(*Context, Entry->Type, Entry->Component.Get());
				}
			})
			// In the "+ Add" popup one click adds and closes.
			.OnMouseButtonClick_Lambda([this](TSharedPtr<FVekaPaletteEntry> Entry)
			{
				if (bPickOnClick && Entry.IsValid() && !Entry->bHeader)
				{
					Veka::Editor::InsertAtSelection(*Context, Entry->Type, Entry->Component.Get());
					FSlateApplication::Get().DismissAllMenus();
				}
			})
		]
	];
	Refresh();
}

void SVekaPalette::Refresh()
{
	Entries.Reset();
	FString LastCategory;
	for (const FVekaPaletteEntry& Entry : GatherEntries(Context->GetView()))
	{
		if (!Filter.IsEmpty() && !Entry.Label.ToString().Contains(Filter) && !Entry.Type.ToString().Contains(Filter) && !Entry.Tooltip.ToString().Contains(Filter))
		{
			continue;
		}
		if (Entry.Category.ToString() != LastCategory)
		{
			LastCategory = Entry.Category.ToString();
			TSharedPtr<FVekaPaletteEntry> Header = MakeShared<FVekaPaletteEntry>();
			Header->Label = Entry.Category;
			Header->Category = Entry.Category;
			Header->bHeader = true;
			Entries.Add(Header);
		}
		Entries.Add(MakeShared<FVekaPaletteEntry>(Entry));
	}
	List->RequestListRefresh();
}

TSharedRef<ITableRow> SVekaPalette::MakeRow(TSharedPtr<FVekaPaletteEntry> Entry, const TSharedRef<STableViewBase>& Owner)
{
	if (Entry->bHeader)
	{
		return SNew(STableRow<TSharedPtr<FVekaPaletteEntry>>, Owner)
			.ShowSelection(false)
			.Padding(FMargin(6.f, 10.f, 2.f, 3.f))
			[
				SNew(STextBlock).Text(Entry->Label.ToUpper()).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(Veka::Editor::Look::Subdued())
			];
	}
	const FText How = bPickOnClick ? LOCTEXT("PaletteHowClick", "Click to add it at the selection.") : LOCTEXT("PaletteHow", "Drag it into the Views list, or double-click to add it at the selection.");
	return SNew(STableRow<TSharedPtr<FVekaPaletteEntry>>, Owner)
		.Padding(FMargin(6.f, 3.f, 2.f, 3.f))
		.ToolTipText(Entry->Tooltip.IsEmpty() ? How : FText::Format(LOCTEXT("PaletteComponentTooltip", "{0}\n\n{1}"), Entry->Tooltip, How))
		.OnDragDetected_Lambda([Entry](const FGeometry&, const FPointerEvent& Event) -> FReply
		{
			return FReply::Handled().BeginDragDrop(FVekaNodeDragDrop::New({}, Entry->Type, Entry->Component.Get(), Entry->Label));
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(Entry->IconColor.R, Entry->IconColor.G, Entry->IconColor.B, 0.14f))
				.Padding(FMargin(4.f))
				[
					Veka::Editor::Look::Icon(Entry->Icon.IsNone() ? FName(TEXT("cube")) : Entry->Icon, 14.f, Entry->IconColor)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Entry->Label)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Entry->Tooltip).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(Veka::Editor::Look::Subdued())
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Visibility(Entry->Tooltip.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
