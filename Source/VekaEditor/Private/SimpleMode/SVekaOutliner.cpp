// Copyright TheMiniBoss. All Rights Reserved.

#include "SimpleMode/SVekaOutliner.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "View/VekaCatalog.h"
#include "View/VekaViewAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

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

	FLinearColor KindColor(const FVekaDocument& Document, int32 Node)
	{
		const FVekaAstNode& N = Document.Nodes[Node];
		if (N.Kind != EVekaAstNodeKind::Element)
		{
			return FLinearColor(0.80f, 0.47f, 0.95f);
		}
		if (N.Component)
		{
			return FLinearColor(0.98f, 0.62f, 0.30f);
		}
		const FVekaCatalogEntry* Entry = Veka::Catalog::FindComponent(N.Type);
		return Entry && Entry->bContainer ? FLinearColor(0.35f, 0.82f, 0.78f) : FLinearColor(0.45f, 0.65f, 1.f);
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
	ChildSlot
	[
		SNew(SVerticalBox)
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
			.Text(LOCTEXT("Empty", "Empty view: double-click a component in the Palette to start."))
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
		Content = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(KindColor(*Document, Node))
				.Padding(FMargin(3.f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
				.Text(FText::FromString(Veka::Editor::Doc::DisplayName(*Document, Node)))
				.ToolTipText(LOCTEXT("RenameTooltip", "Double-click or F2: label of the view (a // comment in code)"))
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
			];
	}

	return SNew(STableRow<TSharedPtr<FVekaOutlinerItem>>, Owner)
		.Padding(FMargin(2.f, 1.f))
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
		Entry.Label = FText::FromString(Type);
		Entry.Category = Category;
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
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UVekaView::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (Exclude && Asset.AssetName == Exclude->GetFName())
		{
			continue;
		}
		FVekaPaletteEntry& Entry = Out.AddDefaulted_GetRef();
		Entry.Type = Asset.AssetName;
		Entry.Label = FText::FromName(Asset.AssetName);
		Entry.Category = LOCTEXT("CategoryViews", "Views of the project");
		Entry.Component = Cast<UVekaView>(Asset.GetAsset());
	}
	return Out;
}

void SVekaPalette::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext)
{
	Context = InContext;
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
		if (!Filter.IsEmpty() && !Entry.Label.ToString().Contains(Filter))
		{
			continue;
		}
		if (Entry.Category.ToString() != LastCategory)
		{
			LastCategory = Entry.Category.ToString();
			TSharedPtr<FVekaPaletteEntry> Header = MakeShared<FVekaPaletteEntry>();
			Header->Label = Entry.Category;
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
			.Padding(FMargin(2.f, 6.f, 2.f, 2.f))
			[
				SNew(STextBlock).Text(Entry->Label).Font(FAppStyle::GetFontStyle("SmallFontBold")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}
	return SNew(STableRow<TSharedPtr<FVekaPaletteEntry>>, Owner)
		.Padding(FMargin(10.f, 2.f, 2.f, 2.f))
		.ToolTipText(LOCTEXT("PaletteTooltip", "Drag onto the Outliner, or double-click to add it at the selection"))
		.OnDragDetected_Lambda([Entry](const FGeometry&, const FPointerEvent& Event) -> FReply
		{
			return FReply::Handled().BeginDragDrop(FVekaNodeDragDrop::New({}, Entry->Type, Entry->Component.Get(), Entry->Label));
		})
		[
			SNew(STextBlock).Text(Entry->Label)
		];
}

#undef LOCTEXT_NAMESPACE
