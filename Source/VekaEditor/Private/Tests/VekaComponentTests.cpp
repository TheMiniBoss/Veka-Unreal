// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3g: components composed as assets (@Param / @Slot) in the palette, completion and analysis; component
// libraries and shared styles; the Alpha 13 themes migrated to UVekaTheme.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/VekaComponentAssets.h"
#include "Migration/VekaAlpha13Migration.h"
#include "CodeEditor/VekaCodeAssist.h"
#include "SimpleMode/SVekaOutliner.h"
#include "View/VekaComponentLibrary.h"
#include "View/VekaViewAsset.h"
#include "Colora/VekaTheme.h"
#include "Core/VekaEnvironment.h"
#include "Slate/SVekaHost.h"
#include "VekaLanguage.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaComponentTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FName UniqueName(const TCHAR* Base)
	{
		static int32 Counter = 0;
		return FName(*FString::Printf(TEXT("%s_%d"), Base, ++Counter));
	}

	template <typename T>
	T* MakeAsset(FName Name)
	{
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/VekaComponentTests/%s"), *Name.ToString()));
		return NewObject<T>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
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

	FVector2D ViewSize(const FString& Source, const FVekaParseOptions& Options = FVekaParseOptions())
	{
		UVekaView* View = MakeAsset<UVekaView>(UniqueName(TEXT("VV_Sized")));
		ON_SCOPE_EXIT { Discard(View); };
		View->Document = Veka::Lang::Parse(Source, Options).Document;
		const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
		Host->SetRoot(View->BuildRoot());
		PaintOnce(Host);
		Host->SlatePrepass(1.f);
		return Host->GetDesiredSize();
	}

	bool HasDiagnostic(const TArray<FVekaDiagnostic>& Diagnostics, const FString& Part, EVekaDiagnosticSeverity Severity)
	{
		return Diagnostics.ContainsByPredicate([&](const FVekaDiagnostic& D) { return D.Severity == Severity && D.Message.ToString().Contains(Part, ESearchCase::CaseSensitive); });
	}

	FString AllDiagnostics(const TArray<FVekaDiagnostic>& Diagnostics)
	{
		TArray<FString> Lines;
		for (const FVekaDiagnostic& D : Diagnostics)
		{
			Lines.Add(D.ToString());
		}
		return FString::Join(Lines, TEXT(" / "));
	}

	FVekaCompletion CompleteAt(const FString& TextWithCursor, const FVekaCompletionSources& Sources)
	{
		int32 Cursor = INDEX_NONE;
		TextWithCursor.FindChar(TEXT('|'), Cursor);
		const FString Text = TextWithCursor.Left(Cursor) + TextWithCursor.Mid(Cursor + 1);
		int32 Line = 0;
		int32 LineStart = 0;
		for (int32 Index = 0; Index < Cursor; ++Index)
		{
			if (Text[Index] == TEXT('\n'))
			{
				++Line;
				LineStart = Index + 1;
			}
		}
		return Veka::Editor::ComputeCompletion(Text, Line, Cursor - LineStart, Sources, true);
	}

	bool Offers(const FVekaCompletion& Completion, const FString& Label)
	{
		return Completion.Items.ContainsByPredicate([&Label](const FVekaCompletionItem& Item) { return Item.Label == Label; });
	}

	const TCHAR* BadgeSource = TEXT(R"VK(view Badge {
    @Param var title: Text = "New"
    @Param var count: Int
    @Param var tint: Color = .Red
    @Slot default
    HStack(spacing: 4) {
        Text(title)
        Text("\(count)")
        Slot(default)
    }
    .padding(4)
    .background(tint, radius: 6)
}
)VK");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaComponentParamsTest, "VekaUI.Editor.Components.Params", VekaComponentTestFlags)
bool FVekaComponentParamsTest::RunTest(const FString& Parameters)
{
	const FName BadgeName = UniqueName(TEXT("VV_Badge"));
	UVekaView* Badge = MakeAsset<UVekaView>(BadgeName);
	ON_SCOPE_EXIT { Discard(Badge); };
	Badge->Document = Veka::Lang::Parse(BadgeSource).Document;
	FVekaParseOptions Options;
	Options.ResolveComponent = [Badge](FName Name) { return Name == Badge->GetFName() ? Badge : nullptr; };
	const FString Name = BadgeName.ToString();

	// The analysis checks a component's call against its @Param and @Slot declarations.
	const FVekaParseResult Good = Veka::Lang::Parse(FString::Printf(TEXT("view A {\n    %s(title: \"Sale\", count: 3) {\n        Text(\"!\")\n    }\n}\n"), *Name), Options);
	const TArray<FVekaDiagnostic> GoodDiagnostics = Veka::Lang::Analyze(Good.Document);
	TestFalse(TEXT("valid call: ") + AllDiagnostics(GoodDiagnostics), Good.HasErrors() || GoodDiagnostics.ContainsByPredicate([](const FVekaDiagnostic& D) { return D.Severity == EVekaDiagnosticSeverity::Error; }));

	const FVekaParseResult Bad = Veka::Lang::Parse(FString::Printf(TEXT("view A {\n    %s(title: 4, size: 2)\n}\n"), *Name), Options);
	const TArray<FVekaDiagnostic> BadDiagnostics = Veka::Lang::Analyze(Bad.Document);
	TestTrue(TEXT("unknown parameter: ") + AllDiagnostics(BadDiagnostics), HasDiagnostic(BadDiagnostics, TEXT("has no parameter 'size'"), EVekaDiagnosticSeverity::Error));
	TestTrue(TEXT("wrong type"), HasDiagnostic(BadDiagnostics, TEXT("'title' of"), EVekaDiagnosticSeverity::Error));
	TestTrue(TEXT("parameter without default left out"), HasDiagnostic(BadDiagnostics, TEXT("'count' is not given"), EVekaDiagnosticSeverity::Warning));

	const FVekaParseResult Slotted = Veka::Lang::Parse(FString::Printf(TEXT("view A {\n    %s(count: 1) {\n        Text(\"x\")\n            .inSlot(footer)\n    }\n}\n"), *Name), Options);
	TestTrue(TEXT("unknown slot"), HasDiagnostic(Veka::Lang::Analyze(Slotted.Document), TEXT("has no slot 'footer'"), EVekaDiagnosticSeverity::Error));

	// Completion offers the component's parameters, and paints for a Color parameter.
	FVekaCompletionSources Sources;
	Sources.ProjectViews = { BadgeName };
	Sources.ResolveView = [Badge](FName Asked) { return Asked == Badge->GetFName() ? Badge : nullptr; };
	FVekaCompletion C = CompleteAt(FString::Printf(TEXT("view A {\n    %s(|\n}"), *Name), Sources);
	TestTrue(TEXT("parameters offered"), Offers(C, TEXT("title:")) && Offers(C, TEXT("count:")) && Offers(C, TEXT("tint:")));
	C = CompleteAt(FString::Printf(TEXT("view A {\n    %s(count: 2, |\n}"), *Name), Sources);
	TestFalse(TEXT("a given parameter is not offered again"), Offers(C, TEXT("count:")));
	C = CompleteAt(FString::Printf(TEXT("view A {\n    %s(tint: .|\n}"), *Name), Sources);
	TestTrue(TEXT("paints for a Color parameter"), Offers(C, TEXT("Orange")) && Offers(C, TEXT("ToggleAccent")));

	// The palette lists it, with its parameters and slots, and refreshes on its own (tested through GatherEntries).
	const TArray<FVekaPaletteEntry> Entries = SVekaPalette::GatherEntries(nullptr);
	const FVekaPaletteEntry* Entry = Entries.FindByPredicate([Badge](const FVekaPaletteEntry& E) { return E.Component.Get() == Badge; });
	if (TestNotNull(TEXT("component in the palette"), Entry))
	{
		TestTrue(TEXT("tooltip lists parameters and slots: ") + Entry->Tooltip.ToString(), Entry->Tooltip.ToString().Contains(TEXT("count: Int")) && Entry->Tooltip.ToString().Contains(TEXT("slots: default")));
	}
	TestNull(TEXT("a view is not offered inside itself"), SVekaPalette::GatherEntries(Badge).FindByPredicate([Badge](const FVekaPaletteEntry& E) { return E.Component.Get() == Badge; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaComponentLibraryTest, "VekaUI.Editor.Components.Library", VekaComponentTestFlags)
bool FVekaComponentLibraryTest::RunTest(const FString& Parameters)
{
	UVekaView* Badge = MakeAsset<UVekaView>(UniqueName(TEXT("VV_LibBadge")));
	ON_SCOPE_EXIT { Discard(Badge); };
	Badge->Document = Veka::Lang::Parse(BadgeSource).Document;

	// A library made by the factory starts with a compiled Card style.
	UVekaComponentLibraryFactory* Factory = NewObject<UVekaComponentLibraryFactory>();
	const FName LibraryName = UniqueName(TEXT("VL_Test"));
	UVekaComponentLibrary* Library = Cast<UVekaComponentLibrary>(Factory->FactoryCreateNew(UVekaComponentLibrary::StaticClass(),
		CreatePackage(*FString::Printf(TEXT("/Temp/VekaComponentTests/%s"), *LibraryName.ToString())), LibraryName, RF_Public | RF_Standalone, nullptr, nullptr));
	ON_SCOPE_EXIT { Discard(Library); };
	if (!TestNotNull(TEXT("library"), Library) || !TestEqual(TEXT("one style"), Library->Styles.Num(), 1))
	{
		return false;
	}
	const FVekaSharedStyle& Card = Library->Styles[0];
	TestTrue(TEXT("Card compiles: ") + Card.CompileErrors, Card.CompileErrors.IsEmpty() && Card.Compiled.Nodes.IsValidIndex(Card.Compiled.Root));
	TestEqual(TEXT("two modifiers"), Card.Compiled.Nodes.IsValidIndex(Card.Compiled.Root) ? Card.Compiled.Nodes[Card.Compiled.Root].Modifiers.Num() : 0, 2);
	TestTrue(TEXT("registered: found by name"), Veka::Components::FindStyle(TEXT("Card")) != nullptr);

	// A style with an error does not compile (and says where).
	FVekaSharedStyle Broken;
	Broken.Name = TEXT("Broken");
	Broken.Modifiers = TEXT(".padding(8)\n.notAModifier(value: 3)");
	Veka::Editor::Components::CompileStyle(Broken);
	TestTrue(TEXT("error reported: ") + Broken.CompileErrors, Broken.CompileErrors.Contains(TEXT("notAModifier")));
	FVekaSharedStyle Unparsed;
	Unparsed.Modifiers = TEXT(".padding(8)\n.frame(3)");
	Veka::Editor::Components::CompileStyle(Unparsed);
	TestTrue(TEXT("parse errors give the style's own line: ") + Unparsed.CompileErrors, Unparsed.CompileErrors.StartsWith(TEXT("2:")));
	TestFalse(TEXT("nothing applied"), Broken.Compiled.Nodes.IsValidIndex(Broken.Compiled.Root));

	// `.style(.Card)` applies the style's modifiers: 16 of padding on each side.
	const FString Plain = TEXT("view A {\n    Rectangle(fill: .Red)\n        .frame(width: 40, height: 20)\n}\n");
	const FString Styled = TEXT("view A {\n    Rectangle(fill: .Red)\n        .frame(width: 40, height: 20)\n        .style(.Card)\n}\n");
	const FVector2D PlainSize = ViewSize(Plain);
	const FVector2D StyledSize = ViewSize(Styled);
	TestTrue(FString::Printf(TEXT("style applied (%s -> %s)"), *PlainSize.ToString(), *StyledSize.ToString()), FMath::IsNearlyEqual(StyledSize.X - PlainSize.X, 32.0, 0.5));
	TestFalse(TEXT("known style: no warning"), HasDiagnostic(Veka::Lang::Analyze(Veka::Lang::Parse(Styled).Document), TEXT("is not a style"), EVekaDiagnosticSeverity::Warning));
	const FString Unknown = TEXT("view A {\n    Text(\"a\")\n        .style(.Nope)\n}\n");
	TestTrue(TEXT("unknown style warned"), HasDiagnostic(Veka::Lang::Analyze(Veka::Lang::Parse(Unknown).Document), TEXT("'.Nope' is not a style"), EVekaDiagnosticSeverity::Warning));
	AddExpectedMessage(TEXT("unknown style 'Nope'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	ViewSize(Unknown);

	// The palette groups the library's components under its name; completion offers its styles.
	Library->DisplayName = FText::FromString(TEXT("Test Kit"));
	Library->Components.Add(Badge);
	TestTrue(TEXT("library of the view"), Veka::Components::FindLibraryOf(Badge) == Library);
	const TArray<FVekaPaletteEntry> Entries = SVekaPalette::GatherEntries(nullptr);
	const FVekaPaletteEntry* Entry = Entries.FindByPredicate([Badge](const FVekaPaletteEntry& E) { return E.Component.Get() == Badge; });
	TestTrue(TEXT("palette group is the library"), Entry && Entry->Category.ToString() == TEXT("Test Kit"));
	FVekaCompletionSources Sources;
	TestTrue(TEXT("styles completed"), Offers(CompleteAt(TEXT("view A {\n    Text(\"a\")\n        .style(.|\n}"), Sources), TEXT("Card")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaAlpha13ThemeTest, "VekaUI.Editor.Components.Alpha13Theme", VekaComponentTestFlags)
bool FVekaAlpha13ThemeTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Editor::Alpha13;
	TestEqual(TEXT("struct field names"), CleanFieldName(TEXT("Light_2_1C5CAC2F46DA51ACA06E0089A90FD3C8")), FString(TEXT("Light")));
	TestEqual(TEXT("plain names kept"), CleanFieldName(TEXT("SliderSize")), FString(TEXT("SliderSize")));
	TestEqual(TEXT("preset AccentColor"), TokenForPreset(TEXT("AccentColor")), FName(TEXT("Accent")));
	TestEqual(TEXT("preset typo"), TokenForPreset(TEXT("WindowBakground")), FName(TEXT("WindowBackground")));
	TestEqual(TEXT("sub-theme slot"), TokenForSlot(TEXT("Toggle"), TEXT("ToggleAccentColor")), FName(TEXT("ToggleAccent")));
	TestEqual(TEXT("knob slot"), TokenForSlot(TEXT("Slider"), TEXT("KnobColor")), FName(TEXT("SliderKnob")));
	TestEqual(TEXT("root slot"), TokenForSlot(FString(), TEXT("TextColor")), FName(TEXT("ThemeText")));
	TestEqual(TEXT("root control slot"), TokenForSlot(FString(), TEXT("GaugeAccentColor")), FName(TEXT("GaugeAccent")));
	TestEqual(TEXT("hierarchy"), HierarchyFromName(TEXT("Quarternary")), EVekaHierarchy::Quaternary);

	// Control tokens fall back to the system tokens, so the controls look the same without a theme.
	FVekaEnvironment Dark = VekaEnv::MakeDefault();
	Dark.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), EVekaColorScheme::Dark);
	TestTrue(TEXT("ToggleAccent = Tint without a theme"), Veka::Colora::ResolveColor(TEXT("ToggleAccent"), Dark).Equals(Veka::Colora::ResolveColor(TEXT("Tint"), Dark)));
	TestTrue(TEXT("SliderBackground = Fill"), Veka::Colora::ResolveColor(TEXT("SliderBackground"), Dark).Equals(Veka::Colora::ResolveColor(TEXT("Fill"), Dark)));
	TestTrue(TEXT("control tokens are known"), Veka::Colora::IsKnownToken(TEXT("MenuAccent")));

	// The migrated theme (Veka.MigrateAlpha13Themes, saved in the plugin): the Alpha 13 Colora values, light and dark.
	const UVekaTheme* Theme = LoadObject<UVekaTheme>(nullptr, TEXT("/VekaUI/Themes/VT_Alpha13.VT_Alpha13"));
	if (!TestNotNull(TEXT("VT_Alpha13 in the plugin"), Theme))
	{
		return false;
	}
	TestEqual(TEXT("every Colora preset"), Theme->Colors.Num(), 31);
	const FVekaThemeColor* Accent = Theme->FindColor(TEXT("Accent"));
	TestTrue(TEXT("Alpha 13 accent (light #007AFF)"), Accent && Accent->Light.Color.ToFColorSRGB() == FColor(0x00, 0x7A, 0xFF));
	TestTrue(TEXT("Alpha 13 accent (dark #0A84FF)"), Accent && Accent->Dark.Color.ToFColorSRGB() == FColor(0x0A, 0x84, 0xFF));
	const FVekaThemeColor* Indigo = Theme->FindColor(TEXT("Indigo"));
	TestTrue(TEXT("Alpha 13 indigo differs from the system one (#6155F5)"), Indigo && Indigo->Light.Color.ToFColorSRGB() == FColor(0x61, 0x55, 0xF5));
	const FVekaThemeColor* Background = Theme->FindColor(TEXT("ThemeBackground"));
	TestTrue(TEXT("theme background is the Background preset"), Background && Background->Light.Type == EVekaPaintType::Token && Background->Light.Token == TEXT("Background"));
	const float* SliderHeight = Theme->FindMetric(TEXT("Slider.Height"));
	TestTrue(TEXT("slider size"), SliderHeight && FMath::IsNearlyEqual(*SliderHeight, 8.f));

	FVekaEnvironment Themed = Dark;
	Themed.Set<const UVekaTheme*>(VekaEnv::Theme(), Theme);
	TestTrue(TEXT("ToggleAccent follows the theme's accent"), Veka::Colora::ResolveColor(TEXT("ToggleAccent"), Themed).Equals(Accent ? Accent->Dark.Color : FLinearColor::Black, 1e-3f));
	TestTrue(TEXT("theme metrics reach the controls"), FMath::IsNearlyEqual(Veka::Colora::Metric(TEXT("Slider.Height"), Themed, 4.f), 8.f));
	return true;
}

#endif
