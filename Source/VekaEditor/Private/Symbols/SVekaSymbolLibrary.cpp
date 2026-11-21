// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/SVekaSymbolLibrary.h"
#include "Symbols/VekaSymbol.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "VekaBuilders.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VekaSymbolLibrary"

namespace Veka::Editor::SymbolLibrary
{
	const FName TabId(TEXT("VekaSymbolLibrary"));

	TArray<FName> Filter(const FString& Query, FName Category)
	{
		TArray<FName> Out;
		const FString Needle = Query.TrimStartAndEnd();
		for (const FName Name : Veka::Symbols::GetAllNames())
		{
			const FVekaSymbolDefinition* Definition = Veka::Symbols::Find(Name);
			if (!Definition)
			{
				continue;
			}
			if (!Category.IsNone() && Definition->Category != Category)
			{
				continue;
			}
			bool bMatch = Needle.IsEmpty() || Name.ToString().Contains(Needle) || Definition->Category.ToString().Contains(Needle);
			for (int32 Index = 0; !bMatch && Index < Definition->Keywords.Num(); ++Index)
			{
				bMatch = Definition->Keywords[Index].Contains(Needle);
			}
			if (bMatch)
			{
				Out.Add(Name);
			}
		}
		return Out;
	}

	TArray<FName> Categories()
	{
		TArray<FName> Out;
		for (const FName Name : Veka::Symbols::GetAllNames())
		{
			if (const FVekaSymbolDefinition* Definition = Veka::Symbols::Find(Name))
			{
				if (!Definition->Category.IsNone())
				{
					Out.AddUnique(Definition->Category);
				}
			}
		}
		Out.Sort(FNameLexicalLess());
		return Out;
	}
}

void SVekaSymbolLibrary::Construct(const FArguments& InArgs)
{
	Preview = SNew(SVekaHost);
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	Preview->SetEnvironment(Environment);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "Search symbols (name, keyword, category)"))
			.OnTextChanged_Lambda([this](const FText& Text) { SetQuery(Text.ToString()); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.18f)
			[
				SAssignNew(CategoryList, SListView<TSharedPtr<FName>>)
				.ListItemsSource(&CategoryItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& Owner)
				{
					return SNew(STableRow<TSharedPtr<FName>>, Owner).Padding(FMargin(6.f, 3.f))
						[
							SNew(STextBlock).Text(Item->IsNone() ? LOCTEXT("All", "All") : FText::FromName(*Item))
						];
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FName> Item, ESelectInfo::Type) { if (Item.IsValid()) { SetCategory(*Item); } })
			]
			+ SSplitter::Slot().Value(0.52f)
			[
				SAssignNew(Tiles, STileView<TSharedPtr<FName>>)
				.ListItemsSource(&Shown)
				.ItemWidth(84.f)
				.ItemHeight(78.f)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateTile(this, &SVekaSymbolLibrary::MakeTile)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FName> Item, ESelectInfo::Type) { if (Item.IsValid()) { Selected = *Item; RefreshPreview(); } })
			]
			+ SSplitter::Slot().Value(0.30f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f)[Preview.ToSharedRef()]
				+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CopyName", "Copy name"))
					.ToolTipText(LOCTEXT("CopyNameTooltip", "Copies #\"name\" for Symbol(...) / symbol: in Veka code"))
					.IsEnabled_Lambda([this]() { return !Selected.IsNone(); })
					.OnClicked_Lambda([this]()
					{
						const FString Text = FString::Printf(TEXT("#\"%s\""), *Selected.ToString());
						FPlatformApplicationMisc::ClipboardCopy(*Text);
						FNotificationInfo Info(FText::Format(LOCTEXT("Copied", "Copied {0}"), FText::FromString(Text)));
						Info.ExpireDuration = 2.f;
						FSlateNotificationManager::Get().AddNotification(Info);
						return FReply::Handled();
					})
				]
			]
		]
	];
	CategoryItems.Add(MakeShared<FName>(NAME_None));
	for (const FName Item : Veka::Editor::SymbolLibrary::Categories())
	{
		CategoryItems.Add(MakeShared<FName>(Item));
	}
	CategoryList->RequestListRefresh();
	Refresh();
}

void SVekaSymbolLibrary::SetQuery(const FString& InQuery)
{
	Query = InQuery;
	Refresh();
}

void SVekaSymbolLibrary::SetCategory(FName InCategory)
{
	Category = InCategory;
	Refresh();
}

void SVekaSymbolLibrary::SelectSymbol(FName Name)
{
	Selected = Name;
	RefreshPreview();
}

void SVekaSymbolLibrary::Refresh()
{
	Shown.Reset();
	for (const FName Name : Veka::Editor::SymbolLibrary::Filter(Query, Category))
	{
		Shown.Add(MakeShared<FName>(Name));
	}
	Tiles->RequestListRefresh();
	RefreshPreview();
}

void SVekaSymbolLibrary::RefreshPreview()
{
	if (Selected.IsNone())
	{
		Preview->SetRoot(Veka::Text(LOCTEXT("PickOne", "Pick a symbol")).Opacity(0.5f));
		return;
	}
	const FVekaSymbolDefinition* Definition = Veka::Symbols::Find(Selected);
	auto Row = [](TArray<FVekaElement> Items) { return Veka::HStack(10.f).Children(MoveTemp(Items)); };
	TArray<FVekaElement> Weights;
	for (const EVekaFontWeight Weight : { EVekaFontWeight::Thin, EVekaFontWeight::Light, EVekaFontWeight::Regular, EVekaFontWeight::SemiBold, EVekaFontWeight::Bold, EVekaFontWeight::Black })
	{
		Weights.Add(Veka::Symbol(Selected).FontSize(22.f).FontWeight(Weight));
	}
	TArray<FVekaElement> Modes = {
		Veka::Symbol(Selected).FontSize(22.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Monochrome),
		Veka::Symbol(Selected).FontSize(22.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical).Foreground(FVekaPaint::FromToken(TEXT("Tint"))),
		Veka::Symbol(Selected).FontSize(22.f).SymbolPalette({ FVekaPaint::FromToken(TEXT("Orange")), FVekaPaint::FromToken(TEXT("Teal")), FVekaPaint::FromToken(TEXT("Pink")) }),
		Veka::Symbol(Selected).FontSize(22.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Multicolor),
	};
	TArray<FVekaElement> Scales = {
		Veka::Symbol(Selected).FontSize(18.f).SymbolScale(EVekaSymbolScale::Small),
		Veka::Symbol(Selected).FontSize(18.f).SymbolScale(EVekaSymbolScale::Medium),
		Veka::Symbol(Selected).FontSize(18.f).SymbolScale(EVekaSymbolScale::Large),
	};
	TArray<FVekaElement> Variants;
	for (const TCHAR* Suffix : { TEXT(".fill"), TEXT(".circle"), TEXT(".square"), TEXT(".slash") })
	{
		Variants.Add(Veka::Symbol(FName(*(Selected.ToString() + Suffix))).FontSize(22.f));
	}
	const FString Caption = Definition ? FString::Printf(TEXT("%s · %s%s"), *Selected.ToString(), *Definition->Category.ToString(),
		Definition->VariableSteps > 0 ? *FString::Printf(TEXT(" · %d variable layers"), Definition->VariableSteps) : TEXT("")) : Selected.ToString();
	Preview->SetRoot(Veka::VStack(12.f)(
		Veka::Symbol(Selected).FontSize(72.f),
		Veka::Text(Caption).FontSize(11.f).Opacity(0.7f),
		Row(MoveTemp(Weights)), Row(MoveTemp(Modes)), Row(MoveTemp(Scales)), Row(MoveTemp(Variants))).Padding(10.f));
}

TSharedRef<ITableRow> SVekaSymbolLibrary::MakeTile(TSharedPtr<FName> Name, const TSharedRef<STableViewBase>& Owner)
{
	TSharedRef<SVekaHost> Icon = SNew(SVekaHost);
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	Icon->SetEnvironment(Environment);
	Icon->SetRoot(Veka::Symbol(*Name).FontSize(24.f));
	return SNew(STableRow<TSharedPtr<FName>>, Owner)
		.Padding(4.f)
		.ToolTipText(FText::FromName(*Name))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(36.f).HeightOverride(36.f)[Icon]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(STextBlock).Text(FText::FromName(*Name)).Font(FAppStyle::GetFontStyle("SmallFont")).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
		];
}

#undef LOCTEXT_NAMESPACE
