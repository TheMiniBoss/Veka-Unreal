// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3f: SVG import into a Veka Symbol, the Symbol Editor and Library, paint assets and the mesh gradient editor.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Symbols/VekaSvgImport.h"
#include "Symbols/VekaSymbolAssets.h"
#include "Symbols/VekaSymbolEditor.h"
#include "Symbols/SVekaSymbolLibrary.h"
#include "Paint/VekaPaintEditor.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaPaintAsset.h"
#include "Primitives/VekaShapeView.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ObjectTools.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopeExit.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaAssetTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	const TCHAR* LogoSvg = TEXT(R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<!-- A test logo -->
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
  <g transform="translate(4,4)">
    <path d="M4 20 L20 4 L36 20 Z" fill="#ff8800"/>
    <circle cx="20" cy="28" r="8" fill="none" stroke="black" stroke-width="3" data-role="secondary"/>
  </g>
  <rect x="8" y="40" width="32" height="4" rx="2" opacity="0.4"/>
  <path d="M10 10 Q 24 2 38 10" fill="none" stroke="currentColor" stroke-width="2" data-variable="0"/>
  <path d="M14 14 Q 24 8 34 14" fill="none" stroke="currentColor" stroke-width="2" data-variable="1"/>
  <text x="0" y="0">skip</text>
</svg>
)SVG");

	FName UniqueName(const TCHAR* Base)
	{
		static int32 Counter = 0;
		return FName(*FString::Printf(TEXT("%s_%d"), Base, ++Counter));
	}

	UPackage* TempPackage(FName Name)
	{
		return CreatePackage(*FString::Printf(TEXT("/Temp/VekaAssetTests/%s"), *Name.ToString()));
	}

	template <typename T>
	void Discard(T* Object)
	{
		if (Object)
		{
			Object->ClearFlags(RF_Public | RF_Standalone);
			Object->MarkAsGarbage();
		}
	}

	UVekaSymbol* ImportLogo(FName Name)
	{
		UVekaSymbolImportFactory* Factory = NewObject<UVekaSymbolImportFactory>();
		const FString Text(LogoSvg);
		const TCHAR* Buffer = *Text;
		return Cast<UVekaSymbol>(Factory->FactoryCreateText(UVekaSymbol::StaticClass(), TempPackage(Name), Name, RF_Public | RF_Standalone | RF_Transactional, nullptr,
			TEXT("svg"), Buffer, Buffer + Text.Len(), GWarn));
	}

	int32 ThumbnailColors(UObject* Object)
	{
		FObjectThumbnail Thumbnail;
		ThumbnailTools::RenderThumbnail(Object, 128, 128, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &Thumbnail);
		const TArray<uint8>& Pixels = Thumbnail.GetUncompressedImageData();
		TSet<uint32> Colors;
		for (int32 Index = 0; Index + 3 < Pixels.Num(); Index += 4)
		{
			Colors.Add(Pixels[Index] | (Pixels[Index + 1] << 8) | (Pixels[Index + 2] << 16));
		}
		return Colors.Num();
	}

	void PaintOnce(const TSharedRef<SWidget>& Widget, const FVector2f& Size = FVector2f(400.f, 300.f))
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Widget->SlatePrepass(1.f);
		Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSvgImportTest, "VekaUI.Editor.Symbols.Import", VekaAssetTestFlags)
bool FVekaSvgImportTest::RunTest(const FString& Parameters)
{
	FVekaSymbolDefinition Definition;
	TArray<FString> Warnings;
	FString Error;
	if (!TestTrue(TEXT("imports: ") + Error, Veka::Editor::Svg::ImportSymbol(LogoSvg, Definition, Warnings, &Error)))
	{
		return false;
	}
	if (!TestEqual(TEXT("one layer per drawable element"), Definition.Shapes.Num(), 5))
	{
		return false;
	}
	TestTrue(TEXT("text skipped with a warning"), Warnings.ContainsByPredicate([](const FString& W) { return W.Contains(TEXT("<text>")); }));
	const FVekaSymbolShape& Triangle = Definition.Shapes[0];
	TestEqual(TEXT("filled path is a Fill layer"), Triangle.Kind, EVekaSymbolShapeKind::Fill);
	TestTrue(TEXT("explicit color kept for multicolor"), Triangle.Color.A > 0.f && Triangle.Color.R > Triangle.Color.B);
	const FBox2D Bounds = Triangle.Path.GetBounds();
	// (4..36 + translate 4) in a 48 viewBox -> 4..20 in the 24 grid.
	TestTrue(FString::Printf(TEXT("fitted into the 24 grid (%s)"), *Bounds.ToString()), FMath::IsNearlyEqual(Bounds.Min.X, 4.0, 0.05) && FMath::IsNearlyEqual(Bounds.Max.X, 20.0, 0.05));
	const FVekaSymbolShape& Ring = Definition.Shapes[1];
	TestEqual(TEXT("stroke-only element is a Stroke layer"), Ring.Kind, EVekaSymbolShapeKind::Stroke);
	TestEqual(TEXT("data-role"), Ring.Role, EVekaSymbolRole::Secondary);
	const float Regular = Veka::Symbols::StrokeWidth(Veka::Symbols::WeightFromFont(EVekaFontWeight::Regular));
	TestTrue(FString::Printf(TEXT("stroke width follows the weight (scale %f)"), Ring.StrokeScale), FMath::IsNearlyEqual(Ring.StrokeScale, 1.5f / Regular, 0.01f));
	TestEqual(TEXT("low opacity is tertiary"), Definition.Shapes[2].Role, EVekaSymbolRole::Tertiary);
	TestEqual(TEXT("variable layer 0"), Definition.Shapes[3].VariableIndex, 0);
	TestEqual(TEXT("variable layer 1"), Definition.Shapes[4].VariableIndex, 1);
	TestEqual(TEXT("variable steps"), Definition.VariableSteps, 2);

	FVekaSymbolDefinition Bad;
	TestFalse(TEXT("not an svg"), Veka::Editor::Svg::ImportSymbol(TEXT("<html></html>"), Bad, Warnings));

	// The milestone: an imported symbol used in a view by its name (the import log reports the skipped <text>).
	AddExpectedMessage(TEXT("<text> is not supported"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	const FName Name = UniqueName(TEXT("TestLogo"));
	UVekaSymbol* Symbol = ImportLogo(Name);
	ON_SCOPE_EXIT { Discard(Symbol); };
	if (!TestNotNull(TEXT("factory made a UVekaSymbol"), Symbol))
	{
		return false;
	}
	TestTrue(TEXT("found by name"), Veka::Symbols::Find(Name) == &Symbol->Definition);
	TestTrue(TEXT("listed with every symbol"), Veka::Symbols::GetAllNames().Contains(Name));
	FVekaSymbolVariant Variant;
	TestTrue(TEXT("variants resolve too"), Veka::Symbols::Resolve(FName(*(Name.ToString() + TEXT(".circle"))), Variant) == &Symbol->Definition
		&& Variant.Enclosure == EVekaSymbolEnclosure::Circle);
	const FVekaParseResult View = Veka::Lang::Parse(FString::Printf(TEXT("view Uses {\n    Symbol(#\"%s\")\n        .font(.Title1)\n}\n"), *Name.ToString()));
	TestFalse(TEXT("view parses"), View.HasErrors());
	UVekaView* Asset = NewObject<UVekaView>(TempPackage(UniqueName(TEXT("VV_UsesLogo"))), UniqueName(TEXT("VV_UsesLogo")), RF_Public | RF_Standalone);
	ON_SCOPE_EXIT { Discard(Asset); };
	Asset->Document = View.Document;
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(Asset->BuildRoot());
	PaintOnce(Host);
	TestTrue(TEXT("the view lays the symbol out"), Host->GetDesiredSize().X > 4.0);
	TestTrue(TEXT("symbol thumbnail drawn"), ThumbnailColors(Symbol) > 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolEditorTest, "VekaUI.Editor.Symbols.Editor", VekaAssetTestFlags)
bool FVekaSymbolEditorTest::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("<text> is not supported"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	const FName Name = UniqueName(TEXT("EditLogo"));
	UVekaSymbol* Symbol = ImportLogo(Name);
	ON_SCOPE_EXIT { Discard(Symbol); };
	if (!TestNotNull(TEXT("symbol"), Symbol))
	{
		return false;
	}
	const TSharedRef<SVekaSymbolEditor> Editor = SNew(SVekaSymbolEditor, Symbol);
	TestFalse(TEXT("canvas element"), SVekaSymbolEditor::MakeCanvas(Symbol->Definition, 0.4f, 0, 384.f).IsEmpty());

	// Layer edits are undoable and redraw the symbol (geometry cache dropped, revision bumped).
	const uint32 Revision = Veka::Symbols::GetRevision();
	Editor->Edit(INVTEXT("Role"), [](FVekaSymbolDefinition& D) { D.Shapes[0].Role = EVekaSymbolRole::Tertiary; });
	TestEqual(TEXT("role changed"), Symbol->Definition.Shapes[0].Role, EVekaSymbolRole::Tertiary);
	TestTrue(TEXT("revision bumped"), Veka::Symbols::GetRevision() != Revision);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo restores the role"), Symbol->Definition.Shapes[0].Role, EVekaSymbolRole::Primary);

	const double Before = Veka::Symbols::GetShapeGeometry(Symbol->Definition, 0)[0].Points[0].X;
	Editor->Edit(INVTEXT("Move"), [](FVekaSymbolDefinition& D) { D.Shapes[0].Path = D.Shapes[0].Path.Transformed(FVector2D(1.0), FVector2D(2.0, 0.0)); });
	const double After = Veka::Symbols::GetShapeGeometry(Symbol->Definition, 0)[0].Points[0].X;
	TestTrue(FString::Printf(TEXT("geometry follows the edit (%f -> %f)"), Before, After), FMath::IsNearlyEqual(After - Before, 2.0, 0.01));
	Editor->Edit(INVTEXT("Delete"), [](FVekaSymbolDefinition& D) { D.Shapes.RemoveAt(4); });
	TestEqual(TEXT("layer deleted"), Symbol->Definition.Shapes.Num(), 4);

	// Regression: a .fill symbol drawn, then given more layers, draws again (its knockouts were cached per layer count).
	const TSharedRef<SVekaHost> Filled = SNew(SVekaHost);
	Filled->SetRoot(Veka::Symbol(FName(*(Name.ToString() + TEXT(".fill")))).FontSize(48.f));
	PaintOnce(Filled);
	Editor->Edit(INVTEXT("Add"), [](FVekaSymbolDefinition& D)
	{
		FVekaSymbolShape Dot;
		Dot.Path.AddCircle(FVector2D(12.0, 12.0), 2.0);
		D.Shapes.Add(Dot);
		D.Shapes.Add(Dot);
	});
	TestEqual(TEXT("layers added"), Symbol->Definition.Shapes.Num(), 6);
	PaintOnce(Filled);
	Editor->Edit(INVTEXT("Remove"), [](FVekaSymbolDefinition& D) { D.Shapes.SetNum(1); });
	PaintOnce(Filled);
	TestEqual(TEXT("one layer left, still drawn"), Symbol->Definition.Shapes.Num(), 1);

	// Library search.
	TestTrue(TEXT("library finds it by name"), Veka::Editor::SymbolLibrary::Filter(Name.ToString(), NAME_None).Contains(Name));
	TestTrue(TEXT("imported category"), Veka::Editor::SymbolLibrary::Categories().Contains(FName(TEXT("Imported"))));
	TestTrue(TEXT("keyword search"), Veka::Editor::SymbolLibrary::Filter(TEXT("favorite"), NAME_None).Contains(FName(TEXT("star"))));
	TestTrue(TEXT("category filter"), !Veka::Editor::SymbolLibrary::Filter(FString(), TEXT("Imported")).Contains(FName(TEXT("star"))));
	const TSharedRef<SVekaSymbolLibrary> Library = SNew(SVekaSymbolLibrary);
	Library->SetQuery(TEXT("arrow"));
	TestTrue(FString::Printf(TEXT("library tiles (%d)"), Library->GetShownCount()), Library->GetShownCount() > 3);
	Library->SelectSymbol(Name);

	// The Symbol Editor opens on double-click.
	UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	TestTrue(TEXT("opens"), Editors->OpenEditorForAsset(Symbol));
	IAssetEditorInstance* Instance = Editors->FindEditorForAsset(Symbol, false);
	TestTrue(TEXT("the Symbol Editor"), Instance && Instance->GetEditorName() == TEXT("VekaSymbolEditor"));
	Editors->CloseAllEditorsForAsset(Symbol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaPaintAssetTest, "VekaUI.Editor.Paint.Mesh", VekaAssetTestFlags)
bool FVekaPaintAssetTest::RunTest(const FString& Parameters)
{
	const FName Name = UniqueName(TEXT("Aurora"));
	UVekaPaintFactory* Factory = NewObject<UVekaPaintFactory>();
	UVekaPaintAsset* Paint = Cast<UVekaPaintAsset>(Factory->FactoryCreateNew(UVekaPaintAsset::StaticClass(), TempPackage(Name), Name, RF_Public | RF_Standalone | RF_Transactional,
		nullptr, GWarn));
	ON_SCOPE_EXIT { Discard(Paint); };
	if (!TestNotNull(TEXT("paint asset"), Paint))
	{
		return false;
	}
	TestTrue(TEXT("starts as a 3 x 3 mesh"), Paint->Paint.Type == EVekaPaintType::MeshGradient && Paint->Paint.Mesh.Width == 3 && Paint->Paint.Mesh.IsValid());
	TestTrue(TEXT("found by name"), Veka::Paints::Find(Name) == &Paint->Paint);

	// Used in a view where a token goes: `.background(.Aurora)`.
	const FString Source = FString::Printf(TEXT("view Bg {\n    Rectangle(fill: .%s)\n}\n"), *Name.ToString());
	const FVekaParseResult Parsed = Veka::Lang::Parse(Source);
	TestTrue(TEXT("a paint asset is a valid paint"), !Veka::Lang::Analyze(Parsed.Document).ContainsByPredicate([](const FVekaDiagnostic& D) { return D.Severity == EVekaDiagnosticSeverity::Error; }));
	TestTrue(TEXT("unknown names still reported"), Veka::Lang::Analyze(Veka::Lang::Parse(TEXT("view Bg {\n    Rectangle(fill: .NopeNope)\n}\n")).Document)
		.ContainsByPredicate([](const FVekaDiagnostic& D) { return D.Message.ToString().Contains(TEXT("NopeNope")); }));
	UVekaView* View = NewObject<UVekaView>(TempPackage(UniqueName(TEXT("VV_Bg"))), UniqueName(TEXT("VV_Bg")), RF_Public | RF_Standalone);
	ON_SCOPE_EXIT { Discard(View); };
	View->Document = Parsed.Document;
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(View->BuildRoot());
	PaintOnce(Host);
	const FVekaShapeView* Shape = nullptr;
	TFunction<void(const FVekaNode&)> Find = [&](const FVekaNode& Node)
	{
		if (const FVekaShapeView* Found = Node.GetPrimitiveAs<FVekaShapeView>())
		{
			Shape = Found;
		}
		for (const TSharedRef<FVekaNode>& Child : Node.GetChildren())
		{
			Find(*Child);
		}
	};
	Find(*Host->GetRuntime()->GetRootNode());
	TestTrue(TEXT("the view fills with the mesh gradient"), Shape && Shape->Fill.Type == EVekaPaintType::MeshGradient && Shape->Fill.Mesh.Width == 3);

	// Mesh operations.
	FVekaPaint Copy = Paint->Paint;
	const FLinearColor Corner = Copy.Mesh.At(0, 0).Color;
	Veka::Editor::Mesh::Resample(Copy, 4, 3);
	TestTrue(TEXT("resampled to 4 x 3"), Copy.Mesh.Width == 4 && Copy.Mesh.Points.Num() == 12);
	TestTrue(TEXT("corner color kept"), Copy.Mesh.At(0, 0).Color.Equals(Corner, 0.05f));
	Veka::Editor::Mesh::MovePoint(Copy.Mesh, 5, FVector2D(1.5, -1.0));
	TestTrue(TEXT("moves stay in the square"), Copy.Mesh.Points[5].Position.Equals(FVector2D(1.0, 0.0)));
	TestEqual(TEXT("point under the mouse"), Veka::Editor::Mesh::PointAt(Copy.Mesh, FVector2D(0.01, 0.01), 0.05), 0);

	// Edited with the mouse: a drag previews, the release writes the asset (one undo step).
	const TSharedRef<SVekaPaintEditor> Editor = SNew(SVekaPaintEditor, Paint);
	const FVector2D Start = Paint->Paint.Mesh.Points[4].Position;
	Editor->SelectPoint(4);
	Editor->PreviewMove(4, FVector2D(0.7, 0.3));
	TestTrue(TEXT("drag previews"), Editor->GetDisplayedPaint().Mesh.Points[4].Position.Equals(FVector2D(0.7, 0.3)));
	TestTrue(TEXT("asset untouched while dragging"), Paint->Paint.Mesh.Points[4].Position.Equals(Start));
	Editor->CommitMove(4, FVector2D(0.7, 0.3));
	TestTrue(TEXT("release writes the asset"), Paint->Paint.Mesh.Points[4].Position.Equals(FVector2D(0.7, 0.3)));
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestTrue(TEXT("undo puts the point back"), Paint->Paint.Mesh.Points[4].Position.Equals(Start));
	Editor->Edit(INVTEXT("Resize"), [](FVekaPaint& P) { Veka::Editor::Mesh::Resample(P, 4, 4); });
	TestEqual(TEXT("rows and columns"), Paint->Paint.Mesh.Points.Num(), 16);
	TestTrue(TEXT("paint thumbnail drawn"), ThumbnailColors(Paint) > 20);

	UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	TestTrue(TEXT("opens"), Editors->OpenEditorForAsset(Paint));
	IAssetEditorInstance* Instance = Editors->FindEditorForAsset(Paint, false);
	TestTrue(TEXT("the paint editor"), Instance && Instance->GetEditorName() == TEXT("VekaPaintEditor"));
	Editors->CloseAllEditorsForAsset(Paint);
	return true;
}

#endif
