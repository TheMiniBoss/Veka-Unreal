// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3b: the Veka View asset editor — viewport, undo / redo, opening, thumbnails.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ViewEditor/SVekaViewPreview.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaEnvironment.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ObjectTools.h"
#include "Misc/ObjectThumbnail.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/Package.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr EAutomationTestFlags VekaEditorTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	void PaintOnce(const TSharedRef<SWidget>& Widget, const FVector2f& Size = FVector2f(1600.f, 1000.f))
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		// Twice: the first pass fits the zoom and lays the views out, the second paints the settled layout.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Widget->SlatePrepass(1.f);
			Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}
	}

	/** A saved-asset-like view (public, standalone, transactional) in a temporary package; removed at the end. */
	UVekaView* MakeAsset(const TCHAR* Name, FVekaDocument Document)
	{
		// A fresh package each time: objects of an earlier test are already marked as garbage.
		static int32 Counter = 0;
		const FString Unique = FString::Printf(TEXT("%s_%d"), Name, ++Counter);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/VekaViewEditorTests/%s"), *Unique));
		UVekaView* View = NewObject<UVekaView>(Package, FName(*Unique), RF_Public | RF_Standalone | RF_Transactional);
		View->Document = MoveTemp(Document);
		return View;
	}

	void Discard(UVekaView* View)
	{
		if (View)
		{
			View->ClearFlags(RF_Public | RF_Standalone);
			View->MarkAsGarbage();
		}
	}

	struct FShop
	{
		UVekaView* Item = nullptr;
		UVekaView* Shop = nullptr;

		FShop()
		{
			Item = MakeAsset(TEXT("VV_EditorTestItem"), Veka::Samples::MakeShopItemDocument());
			Shop = MakeAsset(TEXT("VV_EditorTestShop"), Veka::Samples::MakeShopDocument(Item));
			Shop->PreviewModelClass = UVekaSampleViewModel::StaticClass();
		}

		~FShop()
		{
			Discard(Shop);
			Discard(Item);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEditorPreviewTest, "VekaUI.Editor.Preview", VekaEditorTestFlags)
bool FVekaEditorPreviewTest::RunTest(const FString& Parameters)
{
	const FShop Assets;
	const TSharedRef<SVekaViewPreview> Preview = SNew(SVekaViewPreview).View(Assets.Shop).bPersistSettings(false);
	TestTrue(TEXT("preview model created from the asset"), Cast<UVekaSampleViewModel>(Preview->GetModel()) != nullptr);
	TestNotNull(TEXT("the view is mounted"), Preview->GetHost()->GetRuntime()->GetRootNode().Get());

	// Every screen: laid out at pixels / DPI (project DPI curve), the screen fits the viewport.
	const TArray<FVekaDevicePreset>& Presets = Veka::Editor::DevicePresets();
	TestEqual(TEXT("five screens"), Presets.Num(), 5);
	for (int32 Index = 0; Index < Presets.Num(); ++Index)
	{
		Preview->SetDevice(Index);
		const FVector2D Expected = FVector2D(Presets[Index].Size) / Veka::Editor::ProjectDPIScale(Presets[Index].Size);
		TestTrue(FString::Printf(TEXT("%s logical size"), *Presets[Index].Name.ToString()), Preview->GetLogicalSize().Equals(Expected, 0.01));
		PaintOnce(Preview);
		const FVector2D Laid = FVector2D(Preview->GetHost()->GetPaintSpaceGeometry().GetLocalSize());
		TestTrue(FString::Printf(TEXT("%s: the view is laid out at %s (got %s)"), *Presets[Index].Name.ToString(), *Expected.ToString(), *Laid.ToString()), Laid.Equals(Expected, 1.0));
		const FVector2D OnScreen = FVector2D(Presets[Index].Size) * Preview->GetZoom();
		TestTrue(FString::Printf(TEXT("%s fits the viewport (%s)"), *Presets[Index].Name.ToString(), *OnScreen.ToString()), OnScreen.X <= 1600.0 && OnScreen.Y <= 1000.0 && OnScreen.X > 100.0);
	}

	// Fixed DPI.
	Preview->SetDevice(0);
	Preview->SetDPIOverride(2.f);
	TestTrue(TEXT("DPI 2 halves the layout size"), Preview->GetLogicalSize().Equals(FVector2D(960.0, 540.0), 0.01));
	PaintOnce(Preview);
	TestTrue(TEXT("laid out at 960 x 540"), FVector2D(Preview->GetHost()->GetPaintSpaceGeometry().GetLocalSize()).Equals(FVector2D(960.0, 540.0), 1.0));
	Preview->SetDPIOverride(0.f);

	// Color scheme reaches the view.
	Preview->SetScheme(EVekaColorScheme::Light);
	const FVekaEnvironment& Light = Preview->GetHost()->GetRuntime()->GetEnvironment();
	TestEqual(TEXT("light scheme"), VekaEnv::GetColorScheme(Light), EVekaColorScheme::Light);
	TestTrue(TEXT("dark text on light"), VekaEnv::GetForegroundColor(Light).GetLuminance() < 0.5f);
	Preview->SetScheme(EVekaColorScheme::Dark);
	TestTrue(TEXT("light text on dark"), VekaEnv::GetForegroundColor(Preview->GetHost()->GetRuntime()->GetEnvironment()).GetLuminance() > 0.5f);

	// Overlays.
	PaintOnce(Preview);
	TestEqual(TEXT("no overlay by default"), Preview->GetOverlayRectCount(), 0);
	Preview->SetShowBounds(true);
	PaintOnce(Preview);
	const int32 Bounds = Preview->GetOverlayRectCount();
	TestTrue(FString::Printf(TEXT("one outline per view (%d)"), Bounds), Bounds > 20);
	Preview->SetShowBounds(false);
	Preview->SetShowPadding(true);
	PaintOnce(Preview);
	// At least the shop's padding (4 strips) and each of the 3 articles' (4 strips); controls add their own.
	TestTrue(FString::Printf(TEXT("padding strips (%d)"), Preview->GetOverlayRectCount()), Preview->GetOverlayRectCount() >= 16);
	Preview->SetShowPadding(false);

	// The preview is live and keeps its state across refreshes; Reset state goes back to the declared values.
	Preview->GetStore()->Set(TEXT("gift"), FVekaValue::Bool(true));
	Preview->Refresh();
	TestTrue(TEXT("state kept by Refresh"), Preview->GetStore()->Get(TEXT("gift")).BoolValue);
	// A state the user did not touch follows an edit of its declared value.
	Assets.Shop->Document.States[1].Initial = FVekaValue::String(TEXT("Lantern"));
	Preview->Refresh();
	TestEqual(TEXT("untouched state takes the new initial value"), Preview->GetStore()->Get(TEXT("lastBought")).AsString(), FString(TEXT("Lantern")));
	TestTrue(TEXT("touched state still kept"), Preview->GetStore()->Get(TEXT("gift")).BoolValue);
	Preview->ResetState();
	TestFalse(TEXT("state reset"), Preview->GetStore()->Get(TEXT("gift")).BoolValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEditorUndoTest, "VekaUI.Editor.Undo", VekaEditorTestFlags)
bool FVekaEditorUndoTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("GEditor"), GEditor))
	{
		return false;
	}
	UVekaView* View = MakeAsset(TEXT("VV_EditorTestUndo"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	const FVekaDocument Before = View->Document;
	const FVekaDocument After = Veka::Samples::MakeShopItemDocument();

	Veka::Editor::ApplyDocument(View, After, INVTEXT("Veka test edit"));
	TestTrue(TEXT("edit applied"), Veka::Lang::StructurallyEqual(View->Document, After));
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	FString Difference;
	TestTrue(FString::Printf(TEXT("undo restores the document (%s)"), *Difference), Veka::Lang::StructurallyEqual(View->Document, Before, &Difference));
	TestTrue(TEXT("redo"), GEditor->RedoTransaction());
	TestTrue(TEXT("redo applies it again"), Veka::Lang::StructurallyEqual(View->Document, After));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEditorToolkitTest, "VekaUI.Editor.Toolkit", VekaEditorTestFlags)
bool FVekaEditorToolkitTest::RunTest(const FString& Parameters)
{
	UAssetEditorSubsystem* Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("asset editor subsystem"), Editors))
	{
		return false;
	}
	const FShop Assets;
	// Same path as a double-click in the Content Browser (UAssetDefinition_VekaView::OpenAssets).
	TestTrue(TEXT("opens"), Editors->OpenEditorForAsset(Assets.Shop));
	ON_SCOPE_EXIT { Editors->CloseAllEditorsForAsset(Assets.Shop); };
	IAssetEditorInstance* Instance = Editors->FindEditorForAsset(Assets.Shop, false);
	if (!TestTrue(TEXT("the Veka View editor is open"), Instance && Instance->GetEditorName() == TEXT("VekaViewEditor")))
	{
		return false;
	}
	FVekaViewEditorToolkit* Toolkit = static_cast<FVekaViewEditorToolkit*>(Instance);
	TestTrue(TEXT("edits the asset"), Toolkit->GetView() == Assets.Shop);
	const TSharedPtr<SVekaViewPreview> Preview = Toolkit->GetPreview();
	if (!TestTrue(TEXT("viewport"), Preview.IsValid()))
	{
		return false;
	}
	TestNotNull(TEXT("the viewport shows the view"), Preview->GetHost()->GetRuntime()->GetRootNode().Get());

	// An edit refreshes the viewport; so do undo and redo.
	const int32 Start = Preview->GetRefreshCount();
	FVekaDocument Edited = Assets.Shop->Document;
	Edited.States[0].Initial = FVekaValue::Bool(true);
	Veka::Editor::ApplyDocument(Assets.Shop, Edited, INVTEXT("Veka test edit"));
	TestTrue(TEXT("edit refreshes the viewport"), Preview->GetRefreshCount() > Start);
	const int32 AfterEdit = Preview->GetRefreshCount();
	GEditor->UndoTransaction();
	TestTrue(TEXT("undo refreshes the viewport"), Preview->GetRefreshCount() > AfterEdit);
	TestFalse(TEXT("undo restored the initial value"), Assets.Shop->Document.States[0].Initial.BoolValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEditorThumbnailTest, "VekaUI.Editor.Thumbnail", VekaEditorTestFlags)
bool FVekaEditorThumbnailTest::RunTest(const FString& Parameters)
{
	const FShop Assets;
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(Assets.Shop, 256, 256, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &Thumbnail);
	TestEqual(TEXT("thumbnail width"), Thumbnail.GetImageWidth(), 256);
	const TArray<uint8>& Pixels = Thumbnail.GetUncompressedImageData();
	TSet<uint32> Colors;
	for (int32 Index = 0; Index + 3 < Pixels.Num(); Index += 4)
	{
		Colors.Add(Pixels[Index] | (Pixels[Index + 1] << 8) | (Pixels[Index + 2] << 16));
	}
	TestTrue(FString::Printf(TEXT("the thumbnail shows the view (%d colors)"), Colors.Num()), Colors.Num() > 20);
	return true;
}

#endif
