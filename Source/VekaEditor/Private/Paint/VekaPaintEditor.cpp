// Copyright TheMiniBoss. All Rights Reserved.

#include "Paint/VekaPaintEditor.h"
#include "Colora/VekaPaintAsset.h"
#include "Colora/VekaTheme.h"
#include "Core/VekaEnvironment.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VekaPaintEditor"

namespace Veka::Editor::Mesh
{
	void Resample(FVekaPaint& Paint, int32 Width, int32 Height)
	{
		Width = FMath::Clamp(Width, 2, 12);
		Height = FMath::Clamp(Height, 2, 12);
		const FVekaRect Unit(FVector2D::ZeroVector, FVector2D(1.0, 1.0));
		FVekaMeshGradient Mesh;
		Mesh.Width = Width;
		Mesh.Height = Height;
		Mesh.bSmooth = Paint.Mesh.bSmooth;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FVekaMeshPoint& Point = Mesh.Points.AddDefaulted_GetRef();
				Point.Position = FVector2D(static_cast<double>(X) / (Width - 1), static_cast<double>(Y) / (Height - 1));
				Point.Color = Paint.Evaluate(Point.Position, Unit);
				Point.Color.A = 1.f;
			}
		}
		Paint.Type = EVekaPaintType::MeshGradient;
		Paint.Mesh = MoveTemp(Mesh);
	}

	void MovePoint(FVekaMeshGradient& Mesh, int32 Index, const FVector2D& Position)
	{
		if (Mesh.Points.IsValidIndex(Index))
		{
			Mesh.Points[Index].Position = FVector2D(FMath::Clamp(Position.X, 0.0, 1.0), FMath::Clamp(Position.Y, 0.0, 1.0));
		}
	}

	int32 PointAt(const FVekaMeshGradient& Mesh, const FVector2D& Position, double Radius)
	{
		int32 Best = INDEX_NONE;
		double BestDistance = Radius;
		for (int32 Index = 0; Index < Mesh.Points.Num(); ++Index)
		{
			const double Distance = FVector2D::Distance(Mesh.Points[Index].Position, Position);
			if (Distance <= BestDistance)
			{
				Best = Index;
				BestDistance = Distance;
			}
		}
		return Best;
	}
}

/** Lattice and draggable points over the drawn paint. */
class SVekaMeshHandles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaMeshHandles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, SVekaPaintEditor* InOwner)
	{
		Owner = InOwner;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(360.0, 360.0); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style,
		bool bParentEnabled) const override
	{
		const FVekaPaint& Paint = Owner->GetDisplayedPaint();
		if (Paint.Type != EVekaPaintType::MeshGradient || !Paint.Mesh.IsValid())
		{
			return LayerId;
		}
		const FVekaMeshGradient& Mesh = Paint.Mesh;
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		auto ToLocal = [&Size](const FVector2D& Unit) { return FVector2f(Unit * Size); };
		for (int32 Y = 0; Y < Mesh.Height; ++Y)
		{
			for (int32 X = 0; X < Mesh.Width; ++X)
			{
				if (X + 1 < Mesh.Width)
				{
					FSlateDrawElement::MakeLines(Out, LayerId, Geometry.ToPaintGeometry(), { ToLocal(Mesh.At(X, Y).Position), ToLocal(Mesh.At(X + 1, Y).Position) }, ESlateDrawEffect::None,
						FLinearColor(1.f, 1.f, 1.f, 0.45f), true, 1.f);
				}
				if (Y + 1 < Mesh.Height)
				{
					FSlateDrawElement::MakeLines(Out, LayerId, Geometry.ToPaintGeometry(), { ToLocal(Mesh.At(X, Y).Position), ToLocal(Mesh.At(X, Y + 1).Position) }, ESlateDrawEffect::None,
						FLinearColor(1.f, 1.f, 1.f, 0.45f), true, 1.f);
				}
			}
		}
		const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
		for (int32 Index = 0; Index < Mesh.Points.Num(); ++Index)
		{
			const bool bSelected = Index == Owner->GetSelectedPoint();
			const float Half = bSelected ? 7.f : 5.f;
			const FVector2f Center = ToLocal(Mesh.Points[Index].Position);
			FSlateDrawElement::MakeBox(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(Half * 2.f + 2.f), FSlateLayoutTransform(Center - FVector2f(Half + 1.f))), White, ESlateDrawEffect::None,
				FLinearColor::Black);
			FSlateDrawElement::MakeBox(Out, LayerId + 2, Geometry.ToPaintGeometry(FVector2f(Half * 2.f), FSlateLayoutTransform(Center - FVector2f(Half))), White, ESlateDrawEffect::None,
				bSelected ? FLinearColor(1.f, 0.6f, 0.1f) : Mesh.Points[Index].Color.CopyWithNewOpacity(1.f));
		}
		return LayerId + 3;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVekaPaint& Paint = Owner->GetDisplayedPaint();
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton || Paint.Type != EVekaPaintType::MeshGradient)
		{
			return FReply::Unhandled();
		}
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const FVector2D Unit = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / Size;
		const int32 Index = Veka::Editor::Mesh::PointAt(Paint.Mesh, Unit, 12.0 / FMath::Max(Size.X, 1.0));
		Owner->SelectPoint(Index);
		if (Index == INDEX_NONE)
		{
			return FReply::Handled();
		}
		Dragging = Index;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Dragging == INDEX_NONE)
		{
			return FReply::Unhandled();
		}
		LastPosition = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / FVector2D(Geometry.GetLocalSize());
		Owner->PreviewMove(Dragging, LastPosition);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Dragging != INDEX_NONE)
		{
			const int32 Index = Dragging;
			Dragging = INDEX_NONE;
			if (HasMouseCapture())
			{
				Owner->CommitMove(Index, FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / FVector2D(Geometry.GetLocalSize()));
			}
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVekaPaint& Paint = Owner->GetDisplayedPaint();
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const int32 Index = Veka::Editor::Mesh::PointAt(Paint.Mesh, FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / Size, 12.0 / FMath::Max(Size.X, 1.0));
		if (Index != INDEX_NONE)
		{
			Owner->PickColor(Index);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	SVekaPaintEditor* Owner = nullptr;
	int32 Dragging = INDEX_NONE;
	FVector2D LastPosition = FVector2D::ZeroVector;
};

void SVekaPaintEditor::Construct(const FArguments& InArgs, UVekaPaintAsset* InAsset)
{
	Asset = InAsset;
	Canvas = SNew(SVekaHost);
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Canvas->SetEnvironment(Environment);
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)[MakeToolbar()]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
				[
					SNew(SBox).WidthOverride(420.f).HeightOverride(420.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[Canvas.ToSharedRef()]
						+ SOverlay::Slot()[SAssignNew(Handles, SVekaMeshHandles, this)]
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(220.f)[SAssignNew(PointPanel, SVerticalBox)]
			]
		]
	];
	Rebuild();
}

TSharedRef<SWidget> SVekaPaintEditor::MakeToolbar()
{
	auto Button = [this](const FText& Label, const FText& Tooltip, TFunction<void()> OnClick)
	{
		return SNew(SButton).ToolTipText(Tooltip).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })[SNew(STextBlock).Text(Label)];
	};
	auto Grid = [this](int32 DeltaWidth, int32 DeltaHeight)
	{
		Edit(LOCTEXT("ResizeMesh", "Resize mesh gradient"), [DeltaWidth, DeltaHeight](FVekaPaint& Paint)
		{
			const int32 Width = Paint.Type == EVekaPaintType::MeshGradient ? Paint.Mesh.Width : 3;
			const int32 Height = Paint.Type == EVekaPaintType::MeshGradient ? Paint.Mesh.Height : 3;
			Veka::Editor::Mesh::Resample(Paint, Width + DeltaWidth, Height + DeltaHeight);
		});
		SelectedPoint = INDEX_NONE;
		Rebuild();
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("ToMesh", "Mesh gradient"), LOCTEXT("ToMeshTooltip", "Turn this paint into a mesh gradient (colors sampled from it)"),
			[this]() { Edit(LOCTEXT("ToMeshStep", "Convert to mesh gradient"), [](FVekaPaint& Paint) { Veka::Editor::Mesh::Resample(Paint, 3, 3); }); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f, 2.f, 0.f))[Button(LOCTEXT("AddColumn", "+ Column"), FText::GetEmpty(), [Grid]() { Grid(1, 0); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2.f, 0.f))[Button(LOCTEXT("RemoveColumn", "- Column"), FText::GetEmpty(), [Grid]() { Grid(-1, 0); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2.f, 0.f))[Button(LOCTEXT("AddRow", "+ Row"), FText::GetEmpty(), [Grid]() { Grid(0, 1); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2.f, 0.f))[Button(LOCTEXT("RemoveRow", "- Row"), FText::GetEmpty(), [Grid]() { Grid(0, -1); })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 3.f, 0.f))[SNew(STextBlock).Text(LOCTEXT("Smooth", "Smooth"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("SmoothTooltip", "Bicubic (Catmull-Rom) interpolation; off = bilinear"))
			.IsChecked_Lambda([this]() { return Displayed.Mesh.bSmooth ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				Edit(LOCTEXT("SetSmooth", "Change mesh gradient"), [State](FVekaPaint& Paint) { Paint.Mesh.bSmooth = State == ECheckBoxState::Checked; });
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.f, 0.f, 0.f, 0.f))
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("SpaceTooltip", "Color space the colors are interpolated in"))
			.OnGetMenuContent_Lambda([this]()
			{
				FMenuBuilder Menu(true, nullptr);
				const UEnum* Spaces = StaticEnum<EVekaColorSpace>();
				for (int32 Index = 0; Index < Spaces->NumEnums() - 1; ++Index)
				{
					const EVekaColorSpace Space = static_cast<EVekaColorSpace>(Spaces->GetValueByIndex(Index));
					Menu.AddMenuEntry(FText::FromString(Spaces->GetNameStringByIndex(Index)), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Space]()
					{
						Edit(LOCTEXT("SetSpace", "Change color space"), [Space](FVekaPaint& Paint) { Paint.ColorSpace = Space; });
					})));
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(StaticEnum<EVekaColorSpace>()->GetNameStringByValue(static_cast<int64>(Displayed.ColorSpace))); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.f, 0.f, 0.f, 0.f))[Button(LOCTEXT("ResetPositions", "Even grid"), LOCTEXT("ResetPositionsTooltip", "Put the points back on a regular grid"),
			[this]() { Edit(LOCTEXT("ResetPositionsStep", "Even mesh grid"), [](FVekaPaint& Paint)
			{
				for (int32 Y = 0; Y < Paint.Mesh.Height; ++Y)
				{
					for (int32 X = 0; X < Paint.Mesh.Width && Paint.Mesh.IsValid(); ++X)
					{
						Paint.Mesh.At(X, Y).Position = FVector2D(static_cast<double>(X) / (Paint.Mesh.Width - 1), static_cast<double>(Y) / (Paint.Mesh.Height - 1));
					}
				}
			}); })];
}

void SVekaPaintEditor::Rebuild()
{
	if (const UVekaPaintAsset* Paint = Asset.Get())
	{
		Displayed = Paint->Paint;
	}
	Canvas->SetRoot(Veka::Rectangle().Fill(Displayed));
	Handles->Invalidate(EInvalidateWidgetReason::Paint);
	RebuildPointPanel();
}

void SVekaPaintEditor::Edit(const FText& Description, TFunctionRef<void(FVekaPaint&)> Change)
{
	UVekaPaintAsset* Paint = Asset.Get();
	if (!Paint)
	{
		return;
	}
	const FScopedTransaction Transaction(Description);
	Paint->SetFlags(RF_Transactional);
	Paint->Modify();
	Change(Paint->Paint);
	Paint->PostEditChange();
	Rebuild();
}

void SVekaPaintEditor::SelectPoint(int32 Index)
{
	SelectedPoint = Index;
	Handles->Invalidate(EInvalidateWidgetReason::Paint);
	RebuildPointPanel();
}

void SVekaPaintEditor::PreviewMove(int32 Index, const FVector2D& Position)
{
	Veka::Editor::Mesh::MovePoint(Displayed.Mesh, Index, Position);
	Canvas->SetRoot(Veka::Rectangle().Fill(Displayed));
	Handles->Invalidate(EInvalidateWidgetReason::Paint);
}

void SVekaPaintEditor::CommitMove(int32 Index, const FVector2D& Position)
{
	Edit(LOCTEXT("MovePoint", "Move mesh point"), [Index, Position](FVekaPaint& Paint) { Veka::Editor::Mesh::MovePoint(Paint.Mesh, Index, Position); });
}

void SVekaPaintEditor::PickColor(int32 Index)
{
	if (!Displayed.Mesh.Points.IsValidIndex(Index))
	{
		return;
	}
	SelectPoint(Index);
	FColorPickerArgs Args(Displayed.Mesh.Points[Index].Color, FOnLinearColorValueChanged::CreateLambda([this, Index](FLinearColor Picked)
	{
		Edit(LOCTEXT("SetPointColor", "Change mesh point color"), [Index, Picked](FVekaPaint& Paint)
		{
			if (Paint.Mesh.Points.IsValidIndex(Index))
			{
				Paint.Mesh.Points[Index].Color = Picked;
				Paint.Mesh.Points[Index].Token = NAME_None;
			}
		});
	}));
	Args.bUseAlpha = true;
	Args.bOnlyRefreshOnMouseUp = true;
	OpenColorPicker(Args);
}

void SVekaPaintEditor::RebuildPointPanel()
{
	PointPanel->ClearChildren();
	if (Displayed.Type != EVekaPaintType::MeshGradient)
	{
		PointPanel->AddSlot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("NotMesh", "This paint is not a mesh gradient: edit it in Details, or click Mesh gradient to turn it into one."))
		];
		return;
	}
	PointPanel->AddSlot().AutoHeight()
	[
		SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.Text(FText::Format(LOCTEXT("MeshInfo", "{0} x {1} points. Drag a point to move it, double-click it to pick its color."), FText::AsNumber(Displayed.Mesh.Width),
			FText::AsNumber(Displayed.Mesh.Height)))
	];
	if (!Displayed.Mesh.Points.IsValidIndex(SelectedPoint))
	{
		return;
	}
	const int32 Index = SelectedPoint;
	const FVekaMeshPoint& Point = Displayed.Mesh.Points[Index];
	PointPanel->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 4.f))
	[
		SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFontBold"))
		.Text(FText::Format(LOCTEXT("PointTitle", "Point {0}, {1}"), FText::AsNumber(Index % Displayed.Mesh.Width), FText::AsNumber(Index / Displayed.Mesh.Width)))
	];
	PointPanel->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SColorBlock).Color(Point.Color).Size(FVector2D(48.0, 20.0))
			.ToolTipText(LOCTEXT("PickTooltip", "Click to pick the color"))
			.OnMouseButtonDown_Lambda([this, Index](const FGeometry&, const FPointerEvent&) { PickColor(Index); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f))
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("TokenTooltip", "Color from a Colora token (follows the theme and light / dark)"))
			.OnGetMenuContent_Lambda([this, Index]()
			{
				FMenuBuilder Menu(true, nullptr);
				Menu.AddMenuEntry(LOCTEXT("NoToken", "(own color)"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Index]()
				{
					Edit(LOCTEXT("ClearToken", "Change mesh point"), [Index](FVekaPaint& Paint) { Paint.Mesh.Points[Index].Token = NAME_None; });
				})));
				for (const FName Token : Veka::Colora::DefaultTokenNames())
				{
					Menu.AddMenuEntry(FText::FromName(Token), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Index, Token]()
					{
						Edit(LOCTEXT("SetToken", "Change mesh point"), [Index, Token](FVekaPaint& Paint)
						{
							Paint.Mesh.Points[Index].Token = Token;
							Veka::Colora::GetDefaultColor(Token, EVekaColorScheme::Dark, Paint.Mesh.Points[Index].Color);
						});
					})));
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()[SNew(STextBlock).Text(Point.Token.IsNone() ? LOCTEXT("OwnColor", "own color") : FText::FromName(Point.Token))]
		]
	];
	auto Coordinate = [this, Index](const FText& Label, double Value, bool bX)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SNumericEntryBox<double>).MinValue(0.0).MaxValue(1.0).Value(TOptional<double>(Value))
				.OnValueCommitted_Lambda([this, Index, bX](double NewValue, ETextCommit::Type)
				{
					FVector2D Position = Displayed.Mesh.Points[Index].Position;
					(bX ? Position.X : Position.Y) = NewValue;
					CommitMove(Index, Position);
				})
			];
	};
	PointPanel->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))[Coordinate(LOCTEXT("X", "x"), Point.Position.X, true)];
	PointPanel->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[Coordinate(LOCTEXT("Y", "y"), Point.Position.Y, false)];
}

// --- Toolkit -------------------------------------------------------------------------------------------------------

const FName FVekaPaintEditorToolkit::EditorTabId(TEXT("VekaPaintEditor_Editor"));
const FName FVekaPaintEditorToolkit::DetailsTabId(TEXT("VekaPaintEditor_Details"));

FText FVekaPaintEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Veka Paint Editor");
}

void FVekaPaintEditorToolkit::InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaPaintAsset* InAsset)
{
	Asset = InAsset;
	InAsset->SetFlags(RF_Transactional);
	Editor = SNew(SVekaPaintEditor, InAsset);
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = PropertyEditor.CreateDetailView(Args);
	Details->SetObject(InAsset);
	PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* Object, FPropertyChangedEvent&)
	{
		if (Object == Asset.Get() && Editor.IsValid())
		{
			Editor->Rebuild();
		}
	});
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_VekaPaintEditor_Layout_v1"))
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.72f)->AddTab(EditorTabId, ETabState::OpenedTab)->SetHideTabWell(true))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.28f)->AddTab(DetailsTabId, ETabState::OpenedTab)));
	InitAssetEditor(Mode, InitToolkitHost, TEXT("VekaPaintEditorApp"), Layout, true, true, InAsset);
}

void FVekaPaintEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("Workspace", "Veka Paint Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(EditorTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab).Label(LOCTEXT("PaintTab", "Paint"))[Editor.ToSharedRef()];
	})).SetDisplayName(LOCTEXT("PaintTab", "Paint")).SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
	{
		return SNew(SDockTab).Label(LOCTEXT("DetailsTab", "Details"))[Details.ToSharedRef()];
	})).SetDisplayName(LOCTEXT("DetailsTab", "Details")).SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FVekaPaintEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(EditorTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
}

void FVekaPaintEditorToolkit::PostUndo(bool bSuccess)
{
	Veka::Paints::NotifyChanged();
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
