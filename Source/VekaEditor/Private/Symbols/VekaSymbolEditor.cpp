// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/VekaSymbolEditor.h"
#include "Symbols/VekaSymbol.h"
#include "Symbols/VekaSymbolView.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Vector/VekaStroke.h"
#include "VekaBuilders.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VekaSymbolEditor"

namespace
{
	FLinearColor RoleTint(EVekaSymbolRole Role)
	{
		switch (Role)
		{
		case EVekaSymbolRole::Secondary: return FLinearColor(0.45f, 0.65f, 1.f);
		case EVekaSymbolRole::Tertiary: return FLinearColor(0.95f, 0.55f, 0.70f);
		default: return FLinearColor(0.92f, 0.92f, 0.95f);
		}
	}

	TSharedRef<SWidget> Label(const FText& Text)
	{
		return SNew(SBox).WidthOverride(92.f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text).ColorAndOpacity(FSlateColor::UseSubduedForeground())];
	}

	TSharedRef<SWidget> Title(const FText& Text)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 2.f))[SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle("SmallFontBold"))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)];
	}

	TSharedRef<SWidget> Choice(const FText& Current, const TArray<FText>& Labels, TFunction<void(int32)> Picked)
	{
		return SNew(SComboButton)
			.OnGetMenuContent_Lambda([Labels, Picked]()
			{
				FMenuBuilder Menu(true, nullptr);
				for (int32 Index = 0; Index < Labels.Num(); ++Index)
				{
					Menu.AddMenuEntry(Labels[Index], FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([Picked, Index]() { Picked(Index); })));
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()[SNew(STextBlock).Text(Current)];
	}

	FVekaEnvironment DarkEnvironment()
	{
		FVekaEnvironment Environment = VekaEnv::MakeDefault();
		Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
		return Environment;
	}
}

FVekaElement SVekaSymbolEditor::MakeCanvas(const FVekaSymbolDefinition& Definition, float InWeight, int32 InSelected, float Size)
{
	TArray<FVekaElement> Layers;
	Layers.Add(Veka::Rectangle().Fill(FLinearColor(0.02f, 0.02f, 0.025f)));

	// Keyline grid: unit lines, the live area (3..21), the circle and the center lines.
	FVekaPath Grid;
	for (int32 Line = 0; Line <= 24; ++Line)
	{
		Grid.MoveTo(FVector2D(Line / 24.0, 0.0)).LineTo(FVector2D(Line / 24.0, 1.0));
		Grid.MoveTo(FVector2D(0.0, Line / 24.0)).LineTo(FVector2D(1.0, Line / 24.0));
	}
	Layers.Add(Veka::Path(Grid).Stroke(FLinearColor(1.f, 1.f, 1.f, 0.05f), 1.f));
	FVekaPath Keylines;
	Keylines.AddRect(FVekaRect(FVector2D(3.0 / 24.0), FVector2D(18.0 / 24.0)));
	Keylines.AddCircle(FVector2D(0.5, 0.5), 10.0 / 24.0);
	Keylines.MoveTo(FVector2D(0.5, 0.0)).LineTo(FVector2D(0.5, 1.0));
	Keylines.MoveTo(FVector2D(0.0, 0.5)).LineTo(FVector2D(1.0, 0.5));
	Layers.Add(Veka::Path(Keylines).Stroke(FLinearColor(0.3f, 0.75f, 1.f, 0.25f), 1.f));

	// The layers, in design units scaled to the canvas.
	const float PixelsPerUnit = Size / 24.f;
	for (int32 Index = 0; Index < Definition.Shapes.Num(); ++Index)
	{
		const FVekaSymbolShape& Shape = Definition.Shapes[Index];
		const FLinearColor Color = Index == InSelected ? FLinearColor(1.f, 0.6f, 0.1f) : RoleTint(Shape.Role);
		FVekaElement Layer = Veka::Path(Shape.Path.Transformed(FVector2D(1.0 / 24.0), FVector2D::ZeroVector));
		if (Shape.Kind == EVekaSymbolShapeKind::Fill)
		{
			Layer = MoveTemp(Layer).Fill(Color);
		}
		else
		{
			FVekaStrokeStyle Style;
			Style.Paint = Color;
			Style.Width = Veka::Symbols::StrokeWidth(InWeight) * Shape.StrokeScale * PixelsPerUnit;
			Style.Cap = EVekaLineCap::Round;
			Style.Join = EVekaLineJoin::Round;
			Layer = MoveTemp(Layer).Stroke(Style);
		}
		Layers.Add(MoveTemp(Layer));
	}
	return Veka::ZStack().Children(MoveTemp(Layers)).Frame(Size, Size);
}

void SVekaSymbolEditor::Construct(const FArguments& InArgs, UVekaSymbol* InSymbol)
{
	Symbol = InSymbol;
	Canvas = SNew(SVekaHost);
	Canvas->SetEnvironment(DarkEnvironment());
	Previews = SNew(SVekaHost);
	Previews->SetEnvironment(DarkEnvironment());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			// Canvas and its weight.
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f)
			[
				SNew(SBox).WidthOverride(400.f)
				[
				SNew(SVerticalBox)
				// The canvas shrinks with the window (the weight slider stays visible).
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
					[
						SNew(SBox).WidthOverride(384.f).HeightOverride(384.f)[Canvas.ToSharedRef()]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Weight", "Weight"))]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SSlider)
						.Value_Lambda([this]() { return Weight; })
						.OnValueChanged_Lambda([this](float Value) { Weight = Value; Rebuild(); })
					]
				]
				]
			]
			// Layers and properties.
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(8.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[Title(LOCTEXT("Layers", "Layers"))]
				+ SScrollBox::Slot()[SAssignNew(Layers, SVerticalBox)]
				+ SScrollBox::Slot()[Title(LOCTEXT("Layer", "Selected layer"))]
				+ SScrollBox::Slot()[SAssignNew(Properties, SVerticalBox)]
			]
		]
		// Previews.
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Title(LOCTEXT("Previews", "Previews: weights, modes, scales, variants"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))
			[
				SNew(SBox).HeightOverride(220.f)[Previews.ToSharedRef()]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Variable", "Variable value"))]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSlider)
					.ToolTipText(LOCTEXT("VariableTooltip", "Lights the variable layers (data-variable 0, 1... / Variable Index)"))
					.Value_Lambda([this]() { return VariableValue; })
					.OnValueChanged_Lambda([this](float Value) { VariableValue = Value; Rebuild(); })
				]
			]
		]
	];
	Rebuild();
}

void SVekaSymbolEditor::Edit(const FText& Description, TFunctionRef<void(FVekaSymbolDefinition&)> Change)
{
	UVekaSymbol* Asset = Symbol.Get();
	if (!Asset)
	{
		return;
	}
	const FScopedTransaction Transaction(Description);
	Asset->SetFlags(RF_Transactional);
	Asset->Modify();
	Change(Asset->Definition);
	Asset->PostEditChange();
	Rebuild();
}

void SVekaSymbolEditor::SelectLayer(int32 Layer)
{
	SelectedLayer = Layer;
	Rebuild();
}

FVekaElement SVekaSymbolEditor::MakePreviews() const
{
	const UVekaSymbol* Asset = Symbol.Get();
	if (!Asset)
	{
		return FVekaElement();
	}
	const FName Name = Asset->GetSymbolName();
	auto Captioned = [](FVekaElement Element, const TCHAR* Caption)
	{
		return Veka::VStack(4.f)(MoveTemp(Element), Veka::Text(FString(Caption)).FontSize(9.f).Opacity(0.6f));
	};
	TArray<FVekaElement> WeightRow;
	const EVekaFontWeight Weights[] = { EVekaFontWeight::Thin, EVekaFontWeight::ExtraLight, EVekaFontWeight::Light, EVekaFontWeight::Regular, EVekaFontWeight::Medium,
		EVekaFontWeight::SemiBold, EVekaFontWeight::Bold, EVekaFontWeight::ExtraBold, EVekaFontWeight::Black };
	const UEnum* WeightEnum = StaticEnum<EVekaFontWeight>();
	for (const EVekaFontWeight Item : Weights)
	{
		WeightRow.Add(Captioned(Veka::Symbol(Name, VariableValue).FontSize(28.f).FontWeight(Item), *WeightEnum->GetNameStringByValue(static_cast<int64>(Item))));
	}
	TArray<FVekaElement> ModeRow = {
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(28.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Monochrome), TEXT("Monochrome")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(28.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical), TEXT("Hierarchical")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(28.f).SymbolPalette({ FVekaPaint::FromToken(TEXT("Red")), FVekaPaint::FromToken(TEXT("Green")), FVekaPaint::FromToken(TEXT("Blue")) }),
			TEXT("Palette")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(28.f).SymbolRenderingMode(EVekaSymbolRenderingMode::Multicolor), TEXT("Multicolor")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(20.f).SymbolScale(EVekaSymbolScale::Small), TEXT("Small")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(20.f).SymbolScale(EVekaSymbolScale::Medium), TEXT("Medium")),
		Captioned(Veka::Symbol(Name, VariableValue).FontSize(20.f).SymbolScale(EVekaSymbolScale::Large), TEXT("Large")),
	};
	TArray<FVekaElement> VariantRow;
	for (const TCHAR* Suffix : { TEXT(""), TEXT(".fill"), TEXT(".circle"), TEXT(".square"), TEXT(".slash"), TEXT(".circle.fill") })
	{
		VariantRow.Add(Captioned(Veka::Symbol(FName(*(Name.ToString() + Suffix)), VariableValue).FontSize(28.f), *(Suffix[0] ? FString(Suffix) : FString(TEXT("default")))));
	}
	return Veka::VStack(14.f)(Veka::HStack(14.f).Children(MoveTemp(WeightRow)), Veka::HStack(14.f).Children(MoveTemp(ModeRow)), Veka::HStack(14.f).Children(MoveTemp(VariantRow))).Padding(8.f);
}

void SVekaSymbolEditor::Rebuild()
{
	UVekaSymbol* Asset = Symbol.Get();
	if (!Asset)
	{
		return;
	}
	SelectedLayer = FMath::Clamp(SelectedLayer, 0, FMath::Max(0, Asset->Definition.Shapes.Num() - 1));
	Canvas->SetRoot(MakeCanvas(Asset->Definition, Weight, SelectedLayer, 384.f));
	Previews->SetRoot(MakePreviews());

	Layers->ClearChildren();
	const TArray<FText> Roles = { LOCTEXT("Primary", "Primary"), LOCTEXT("Secondary", "Secondary"), LOCTEXT("Tertiary", "Tertiary") };
	for (int32 Index = 0; Index < Asset->Definition.Shapes.Num(); ++Index)
	{
		const FVekaSymbolShape& Shape = Asset->Definition.Shapes[Index];
		const FText Caption = FText::Format(LOCTEXT("LayerCaption", "{0}. {1}  ·  {2}{3}"), FText::AsNumber(Index + 1),
			Shape.Kind == EVekaSymbolShapeKind::Fill ? LOCTEXT("Fill", "fill") : LOCTEXT("Stroke", "stroke"), Roles[static_cast<int32>(Shape.Role)],
			Shape.VariableIndex >= 0 ? FText::Format(LOCTEXT("VariableSuffix", "  ·  variable {0}"), FText::AsNumber(Shape.VariableIndex)) : FText::GetEmpty());
		Layers->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SColorBlock).Color(RoleTint(Shape.Role)).Size(FVector2D(10.0, 10.0))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), Index == SelectedLayer ? "PrimaryButton" : "SimpleButton")
				.OnClicked_Lambda([this, Index]() { SelectLayer(Index); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(Caption)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").Text(LOCTEXT("Up", "^")).IsEnabled(Index > 0)
				.ToolTipText(LOCTEXT("UpTooltip", "Draw earlier (below)"))
				.OnClicked_Lambda([this, Index]()
				{
					Edit(LOCTEXT("MoveLayer", "Reorder layers"), [Index](FVekaSymbolDefinition& Definition) { Definition.Shapes.Swap(Index, Index - 1); });
					SelectLayer(Index - 1);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").Text(LOCTEXT("Down", "v")).IsEnabled(Index < Asset->Definition.Shapes.Num() - 1)
				.ToolTipText(LOCTEXT("DownTooltip", "Draw later (above)"))
				.OnClicked_Lambda([this, Index]()
				{
					Edit(LOCTEXT("MoveLayer", "Reorder layers"), [Index](FVekaSymbolDefinition& Definition) { Definition.Shapes.Swap(Index, Index + 1); });
					SelectLayer(Index + 1);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").Text(LOCTEXT("Delete", "x"))
				.OnClicked_Lambda([this, Index]()
				{
					Edit(LOCTEXT("DeleteLayer", "Delete layer"), [Index](FVekaSymbolDefinition& Definition) { Definition.Shapes.RemoveAt(Index); });
					return FReply::Handled();
				})
			]
		];
	}

	Properties->ClearChildren();
	if (!Asset->Definition.Shapes.IsValidIndex(SelectedLayer))
	{
		return;
	}
	const FVekaSymbolShape& Shape = Asset->Definition.Shapes[SelectedLayer];
	const int32 Layer = SelectedLayer;
	auto Row = [this](const FText& Name, TSharedRef<SWidget> Editor)
	{
		Properties->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[Label(Name)] + SHorizontalBox::Slot().FillWidth(1.f)[Editor]
		];
	};
	Row(LOCTEXT("Kind", "Kind"), Choice(Shape.Kind == EVekaSymbolShapeKind::Fill ? LOCTEXT("Fill", "fill") : LOCTEXT("Stroke", "stroke"),
		{ LOCTEXT("Stroke", "stroke"), LOCTEXT("Fill", "fill") }, [this, Layer](int32 Index)
		{
			Edit(LOCTEXT("SetKind", "Change layer"), [Layer, Index](FVekaSymbolDefinition& D) { D.Shapes[Layer].Kind = Index == 1 ? EVekaSymbolShapeKind::Fill : EVekaSymbolShapeKind::Stroke; });
		}));
	Row(LOCTEXT("Role", "Role"), Choice(Roles[static_cast<int32>(Shape.Role)], Roles, [this, Layer](int32 Index)
		{
			Edit(LOCTEXT("SetRole", "Change layer"), [Layer, Index](FVekaSymbolDefinition& D) { D.Shapes[Layer].Role = static_cast<EVekaSymbolRole>(Index); });
		}));
	Row(LOCTEXT("VariableLayer", "Variable layer"), SNew(SNumericEntryBox<int32>)
		.MinValue(-1).MaxValue(16)
		.ToolTipText(LOCTEXT("VariableLayerTooltip", "-1: always lit; 0, 1, 2...: lit in this order by the variable value"))
		.Value(TOptional<int32>(Shape.VariableIndex))
		.OnValueCommitted_Lambda([this, Layer](int32 Value, ETextCommit::Type)
		{
			Edit(LOCTEXT("SetVariable", "Change layer"), [Layer, Value](FVekaSymbolDefinition& D)
			{
				D.Shapes[Layer].VariableIndex = Value;
				int32 Steps = 0;
				for (const FVekaSymbolShape& Item : D.Shapes)
				{
					Steps = FMath::Max(Steps, Item.VariableIndex + 1);
				}
				D.VariableSteps = Steps;
			});
		}));
	Row(LOCTEXT("Fillable", "Fillable"), SNew(SCheckBox)
		.ToolTipText(LOCTEXT("FillableTooltip", "The .fill variant fills this closed outline"))
		.IsChecked(Shape.bFillable ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged_Lambda([this, Layer](ECheckBoxState State)
		{
			Edit(LOCTEXT("SetFillable", "Change layer"), [Layer, State](FVekaSymbolDefinition& D) { D.Shapes[Layer].bFillable = State == ECheckBoxState::Checked; });
		}));
	Row(LOCTEXT("StrokeScale", "Stroke scale"), SNew(SNumericEntryBox<float>)
		.MinValue(0.f).MaxValue(8.f)
		.Value(TOptional<float>(Shape.StrokeScale))
		.OnValueCommitted_Lambda([this, Layer](float Value, ETextCommit::Type)
		{
			Edit(LOCTEXT("SetStrokeScale", "Change layer"), [Layer, Value](FVekaSymbolDefinition& D) { D.Shapes[Layer].StrokeScale = Value; });
		}));
	const FLinearColor Current = Shape.Color;
	Row(LOCTEXT("Color", "Multicolor"), SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SColorBlock)
			.Color(Current.A > 0.f ? Current : FLinearColor(0.2f, 0.2f, 0.2f))
			.Size(FVector2D(28.0, 16.0))
			.ToolTipText(LOCTEXT("ColorTooltip", "Color in the Multicolor mode (click to pick)"))
			.OnMouseButtonDown_Lambda([this, Layer, Current](const FGeometry&, const FPointerEvent&)
			{
				FColorPickerArgs Args(Current.A > 0.f ? Current : FLinearColor::White, FOnLinearColorValueChanged::CreateLambda([this, Layer](FLinearColor Picked)
				{
					Edit(LOCTEXT("SetColor", "Change layer color"), [Layer, Picked](FVekaSymbolDefinition& D) { D.Shapes[Layer].Color = Picked; });
				}));
				Args.bOnlyRefreshOnMouseUp = true;
				OpenColorPicker(Args);
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f))
		[
			SNew(SButton).Text(LOCTEXT("ClearColor", "Foreground"))
			.ToolTipText(LOCTEXT("ClearColorTooltip", "No own color: drawn with the foreground"))
			.OnClicked_Lambda([this, Layer]()
			{
				Edit(LOCTEXT("ClearColorStep", "Change layer color"), [Layer](FVekaSymbolDefinition& D) { D.Shapes[Layer].Color = FLinearColor::Transparent; D.Shapes[Layer].ColorToken = NAME_None; });
				return FReply::Handled();
			})
		]);
}

// --- Toolkit -------------------------------------------------------------------------------------------------------

const FName FVekaSymbolEditorToolkit::EditorTabId(TEXT("VekaSymbolEditor_Editor"));
const FName FVekaSymbolEditorToolkit::DetailsTabId(TEXT("VekaSymbolEditor_Details"));

FText FVekaSymbolEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Veka Symbol Editor");
}

void FVekaSymbolEditorToolkit::InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaSymbol* InSymbol)
{
	Symbol = InSymbol;
	InSymbol->SetFlags(RF_Transactional);
	Veka::Symbols::RegisterSymbol(InSymbol);
	Editor = SNew(SVekaSymbolEditor, InSymbol);
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = PropertyEditor.CreateDetailView(Args);
	Details->SetObject(InSymbol);
	PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* Object, FPropertyChangedEvent&)
	{
		if (Object == Symbol.Get() && Editor.IsValid())
		{
			Editor->Rebuild();
		}
	});
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_VekaSymbolEditor_Layout_v1"))
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.75f)->AddTab(EditorTabId, ETabState::OpenedTab)->SetHideTabWell(true))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.25f)->AddTab(DetailsTabId, ETabState::OpenedTab)));
	InitAssetEditor(Mode, InitToolkitHost, TEXT("VekaSymbolEditorApp"), Layout, true, true, InSymbol);
}

void FVekaSymbolEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("Workspace", "Veka Symbol Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(EditorTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab).Label(LOCTEXT("SymbolTab", "Symbol"))[Editor.ToSharedRef()];
	})).SetDisplayName(LOCTEXT("SymbolTab", "Symbol")).SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab).Label(LOCTEXT("DetailsTab", "Details"))[Details.ToSharedRef()];
	})).SetDisplayName(LOCTEXT("DetailsTab", "Details")).SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FVekaSymbolEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(EditorTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
}

void FVekaSymbolEditorToolkit::PostUndo(bool bSuccess)
{
	if (UVekaSymbol* Asset = Symbol.Get())
	{
		Veka::Symbols::NotifyChanged(Asset->Definition);
	}
	if (Editor.IsValid())
	{
		Editor->Rebuild();
	}
	if (Details.IsValid())
	{
		Details->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
