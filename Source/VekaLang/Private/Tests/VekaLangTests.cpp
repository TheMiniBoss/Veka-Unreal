// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3a: the Veka language (.vk) — parse, canonical print, round trip, diagnostics, analysis.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaLangTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** The sample views, named like the assets Veka.CreateSampleAssets makes (so the printed names resolve in the project). */
	struct FSamples
	{
		UVekaView* Starter = nullptr;
		UVekaView* ShopItem = nullptr;
		UVekaView* Shop = nullptr;

		FSamples()
		{
			UPackage* Package = CreatePackage(TEXT("/Temp/VekaLangTests"));
			auto Make = [Package](const TCHAR* Name, FVekaDocument Document)
			{
				UVekaView* View = FindObject<UVekaView>(Package, Name);
				if (!View)
				{
					View = NewObject<UVekaView>(Package, Name, RF_Transient);
				}
				View->Document = MoveTemp(Document);
				return View;
			};
			Starter = Make(TEXT("VV_Starter"), Veka::Samples::MakeStarterDocument());
			ShopItem = Make(TEXT("VV_ShopItem"), Veka::Samples::MakeShopItemDocument());
			Shop = Make(TEXT("VV_Shop"), Veka::Samples::MakeShopDocument(ShopItem));
		}

		TArray<UVekaView*> All() const { return { Starter, ShopItem, Shop }; }

		FVekaParseOptions Options() const
		{
			FVekaParseOptions Out;
			UVekaView* Item = ShopItem;
			Out.ResolveComponent = [Item](FName Name) { return Name == Item->GetFName() ? Item : nullptr; };
			return Out;
		}
	};

	FString Diagnostics(const FVekaParseResult& Result)
	{
		TArray<FString> Lines;
		for (const FVekaDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Lines.Add(Diagnostic.ToString());
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString Normalize(FString Text)
	{
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return Text;
	}

	/** Parse(Print(Doc)) == Doc, and printing is a fixpoint. */
	bool CheckRoundTrip(FAutomationTestBase& Test, const FVekaDocument& Document, FName ViewName, const FVekaParseOptions& Options)
	{
		const FString Printed = Veka::Lang::Print(Document, ViewName);
		const FVekaParseResult Reparsed = Veka::Lang::Parse(Printed, Options);
		if (Reparsed.HasErrors())
		{
			Test.AddError(FString::Printf(TEXT("%s: the printed text does not parse:\n%s\n--- text ---\n%s"), *ViewName.ToString(), *Diagnostics(Reparsed), *Printed));
			return false;
		}
		Test.TestEqual(FString::Printf(TEXT("%s: view name"), *ViewName.ToString()), Reparsed.ViewName, ViewName);
		FString Difference;
		if (!Veka::Lang::StructurallyEqual(Reparsed.Document, Document, &Difference))
		{
			Test.AddError(FString::Printf(TEXT("%s: Parse(Print(doc)) != doc (%s)\n--- text ---\n%s"), *ViewName.ToString(), *Difference, *Printed));
			return false;
		}
		const FString Again = Veka::Lang::Print(Reparsed.Document, ViewName);
		if (Again != Printed)
		{
			Test.AddError(FString::Printf(TEXT("%s: printing is not a fixpoint\n--- first ---\n%s\n--- second ---\n%s"), *ViewName.ToString(), *Printed, *Again));
			return false;
		}
		return true;
	}

	/** Source -> document -> text -> document, both documents equal and the text canonical. */
	bool CheckSource(FAutomationTestBase& Test, const FString& Source, const FVekaParseOptions& Options = FVekaParseOptions())
	{
		const FVekaParseResult Result = Veka::Lang::Parse(Source, Options);
		if (Result.HasErrors())
		{
			Test.AddError(FString::Printf(TEXT("does not parse:\n%s\n--- source ---\n%s"), *Diagnostics(Result), *Source));
			return false;
		}
		return CheckRoundTrip(Test, Result.Document, Result.ViewName, Options);
	}

	bool HasDiagnostic(const TArray<FVekaDiagnostic>& List, const FString& Fragment, int32 Line = 0, int32 Column = 0, TOptional<EVekaDiagnosticSeverity> Severity = {})
	{
		return List.ContainsByPredicate([&](const FVekaDiagnostic& Diagnostic)
		{
			return Diagnostic.Message.ToString().Contains(Fragment) && (Line == 0 || Diagnostic.Range.Line == Line) && (Column == 0 || Diagnostic.Range.Column == Column)
				&& (!Severity.IsSet() || Diagnostic.Severity == Severity.GetValue());
		});
	}

	FString JoinDiagnostics(const TArray<FVekaDiagnostic>& List)
	{
		TArray<FString> Lines;
		for (const FVekaDiagnostic& Diagnostic : List)
		{
			Lines.Add(Diagnostic.ToString());
		}
		return FString::Join(Lines, TEXT("\n"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangSamplesRoundTripTest, "VekaUI.Lang.RoundTrip.Samples", VekaLangTestFlags)
bool FVekaLangSamplesRoundTripTest::RunTest(const FString& Parameters)
{
	const FSamples Samples;
	for (UVekaView* View : Samples.All())
	{
		CheckRoundTrip(*this, View->Document, View->GetFName(), Samples.Options());
		const TArray<FVekaDiagnostic> Problems = Veka::Lang::Analyze(View->Document);
		TestTrue(FString::Printf(TEXT("%s analyzes clean:\n%s"), *View->GetName(), *JoinDiagnostics(Problems)), Problems.IsEmpty());
	}

	// The component is found by name and keeps its params as the primary-less argument list.
	const FVekaParseResult Shop = Veka::Lang::Parse(Veka::Lang::Print(Samples.Shop->Document, Samples.Shop->GetFName()), Samples.Options());
	const FVekaAstNode* Component = Shop.Document.Nodes.FindByPredicate([](const FVekaAstNode& Node) { return Node.Component != nullptr; });
	TestTrue(TEXT("the shop item component is resolved"), Component && Component->Component == Samples.ShopItem);

	// Without a resolver the name stays an unknown component (the analyzer reports it).
	const FVekaParseResult Unresolved = Veka::Lang::Parse(Veka::Lang::Print(Samples.Shop->Document, Samples.Shop->GetFName()));
	TestTrue(TEXT("unresolved component reported by the analyzer"), HasDiagnostic(Veka::Lang::Analyze(Unresolved.Document), TEXT("VV_ShopItem")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangGoldenTest, "VekaUI.Lang.Golden", VekaLangTestFlags)
bool FVekaLangGoldenTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VekaUI"));
	if (!TestTrue(TEXT("VekaUI plugin found"), Plugin.IsValid()))
	{
		return false;
	}
	const FString Folder = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Samples"), TEXT("Veka"));
	const FSamples Samples;
	for (UVekaView* View : Samples.All())
	{
		const FString Printed = Veka::Lang::Print(View->Document, View->GetFName());
		const FString Path = FPaths::Combine(Folder, View->GetName() + TEXT(".vk"));
		FString Golden;
		if (!FFileHelper::LoadFileToString(Golden, *Path) || Normalize(Golden) != Printed)
		{
			FFileHelper::SaveStringToFile(Printed, *(Path + TEXT(".actual")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			AddError(FString::Printf(TEXT("%s differs from the printed document (written next to it as .actual)"), *Path));
			continue;
		}
		// The file itself imports back to the same document.
		const FVekaParseResult Parsed = Veka::Lang::Parse(Golden, Samples.Options());
		FString Difference;
		TestFalse(FString::Printf(TEXT("%s parses:\n%s"), *Path, *Diagnostics(Parsed)), Parsed.HasErrors());
		TestTrue(FString::Printf(TEXT("%s imports to the sample (%s)"), *Path, *Difference), Veka::Lang::StructurallyEqual(Parsed.Document, View->Document, &Difference));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangCorpusTest, "VekaUI.Lang.RoundTrip.Corpus", VekaLangTestFlags)
bool FVekaLangCorpusTest::RunTest(const FString& Parameters)
{
	// Every declaration, action, operator and literal form.
	CheckSource(*this, TEXT(R"VK(// Every action and value form.
// Second line of the header.
view Everything {
    @State var count: Int = 0
    @State var enabled: Bool = true
    @State var speed: Float = 1.5
    @State var name: String = #"Ada \"the\" first\n"
    @State var tint: Color = rgba(1.0, 0.5, 0.25, 1.0)
    @State var pos: Vector2 = vec2(3.0, -4.5)
    @State var tags: Array = [1, 2.5, .three, "four"]
    @State var info: Struct = { a: 1, b: "x" }
    @State var none
    @Event Saved(Int)
    @Event Closed

    VStack(spacing: 8, alignment: .leading) {
        Text("Count: \(count)")
        Text("\(count) of \(count * 2 + 1) (\(enabled ? #"on" : #"off"))")
        Button("Add") {
            count += 1
            count -= 2
            count += -3
            enabled.toggle()
            withAnimation {
                speed = speed * 2.0
                name = enabled ? #"yes" : #"no"
            }
            count = 0
            emit Saved(count)
            emit Closed
        }
        // A chain of conditions.
        if count > 3 && enabled || !enabled {
            Text("Big")
        } else if count == 0 {
            Text("Zero")
        } else if count < 0 {
            Text("Negative")
        } else {
            Text("Small")
        }
        Text(format("{1} then {0}", count, speed))
        Text(#"Raw \\ string")
        Rectangle(fill: #FF8800)
            .frame(width: 10.0 - (2.0 - 1.0), height: max(4, min(count, 9)))
            .opacity((count + 1) * 0.5 / 2)
            .offset(x: -3, y: tags[1] % 2)
            .rotation(-(speed))
            .disabled(!(count >= 2 != enabled))
    }
    .padding(16)
}
)VK"));

	// Params, slots, loops, the model, gestures.
	CheckSource(*this, TEXT(R"VK(view Card {
    @Param var title: Text = "Card"
    @Param var price: Int
    @Param var anything: Any
    @Slot default
    @Slot footer
    @Event buy(Int)

    VStack {
        // The header.
        Text(title)
        Slot(default)
        ForEach(model.Items) { item, index in
            HStack {
                Text(item.Name)
                Text("#\(index + 1)")
                Spacer
            }
            .onTap {
                emit buy(item.Price)
            }
        }
        ForEach(model.Items) { item in
            Text(item.Name)
        }
        ForEach(model.Matrix) {
            Divider
        }
        Text(model.Rows[0].Name)
        Slot(footer)
    }
}
)VK"));

	// A button with content and an action block; a component used with its single param as primary argument.
	CheckSource(*this, TEXT(R"VK(view Fancy {
    @State var n: Int = 0

    Button {
        Label("Go", symbol: .star)
        Text("\(n)")
    } action {
        n += 1
    }
    .buttonStyle(.bordered)
}
)VK"));

	// Canonical form: sloppy layout and named primary arguments print back the canonical way.
	{
		const FVekaParseResult Sloppy = Veka::Lang::Parse(TEXT("view   Tidy{@State var on:Bool=false\nToggle(title:\"Wi-Fi\",isOn:on).padding(value:4)}"));
		TestFalse(TEXT("sloppy source parses"), Sloppy.HasErrors());
		const FString Expected = TEXT("view Tidy {\n    @State var on: Bool = false\n\n    Toggle(\"Wi-Fi\", isOn: on)\n        .padding(4)\n}\n");
		TestEqual(TEXT("canonical print"), Veka::Lang::Print(Sloppy.Document, Sloppy.ViewName), Expected);
	}

	// Comments survive (document header, node comments).
	{
		const FVekaParseResult Commented = Veka::Lang::Parse(TEXT("// Header\nview C {\n    VStack {\n        // About the text\n        Text(\"a\")\n    }\n}\n"));
		TestTrue(TEXT("header comment kept"), Commented.Document.Comments == TArray<FString>{ TEXT("Header") });
		TestTrue(TEXT("node comment kept"), Commented.Document.Nodes.ContainsByPredicate([](const FVekaAstNode& Node) { return Node.Comments == TArray<FString>{ TEXT("About the text") }; }));
	}

	// Source positions are filled in (the editor jumps to them).
	{
		const FVekaParseResult Positions = Veka::Lang::Parse(TEXT("view P {\n    VStack {\n        Text(\"a\")\n    }\n}\n"));
		const FVekaAstNode* Text = Positions.Document.Nodes.FindByPredicate([](const FVekaAstNode& Node) { return Node.Type == TEXT("Text"); });
		TestTrue(TEXT("Text node at 3:9"), Text && Text->Source.Line == 3 && Text->Source.Column == 9);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangErrorsTest, "VekaUI.Lang.Errors", VekaLangTestFlags)
bool FVekaLangErrorsTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Source;
		const TCHAR* Fragment;
		int32 Line;
		int32 Column;
	};
	const FCase Cases[] = {
		{ TEXT("view A {\n    Text(cnt)\n}"), TEXT("Unknown name 'cnt'"), 2, 10 },
		{ TEXT("view A {\n    @State var x: Integer = 0\n    Text(\"a\")\n}"), TEXT("Unknown type 'Integer'"), 2, 19 },
		{ TEXT("view A {\n    Text(\"abc)\n}"), TEXT("Unterminated string"), 2, 10 },
		{ TEXT("view A {\n    Button(\"x\") {\n        missing = 1\n    }\n}"), TEXT("'missing' is not a state"), 3, 9 },
		{ TEXT("view A {\n    Button(\"x\") {\n        emit Nope\n    }\n}"), TEXT("Event 'Nope' is not declared"), 3, 14 },
		{ TEXT("view A {\n    @State var x: Int = 0\n    @State var x: Int = 1\n    Text(\"a\")\n}"), TEXT("declared twice"), 3, 16 },
		{ TEXT("view A {\n    @State var x: Int = model.Gold\n    Text(\"a\")\n}"), TEXT("must be a constant"), 2, 25 },
		{ TEXT("view A {\n    Text(\"a\")\n    Text(\"b\")\n}"), TEXT("one root"), 3, 5 },
		{ TEXT("view A {\n    Rectangle(fill: #12345)\n}"), TEXT("#RRGGBB"), 2, 21 },
		{ TEXT("view A {\n    Text(\"\\q\")\n}"), TEXT("Unknown escape"), 2, 10 },
		{ TEXT("view A {\n    @Thing var x\n    Text(\"a\")\n}"), TEXT("Unknown declaration"), 2, 6 },
		{ TEXT("view A {\n    VStack {\n        Text(\"a\")\n}"), TEXT("Expected"), 0, 0 },
	};
	for (const FCase& Case : Cases)
	{
		const FVekaParseResult Result = Veka::Lang::Parse(Case.Source);
		TestTrue(FString::Printf(TEXT("'%s' at %d:%d in:\n%s\ngot:\n%s"), Case.Fragment, Case.Line, Case.Column, Case.Source, *Diagnostics(Result)),
			HasDiagnostic(Result.Diagnostics, Case.Fragment, Case.Line, Case.Column, EVekaDiagnosticSeverity::Error));
	}

	// Recovery: every error of the file is reported, not only the first.
	const FVekaParseResult Several = Veka::Lang::Parse(TEXT("view A {\n    VStack {\n        Text(a)\n        Text(b)\n        Button(\"x\") {\n            c = 1\n        }\n    }\n}"));
	TestTrue(FString::Printf(TEXT("three errors:\n%s"), *Diagnostics(Several)),
		HasDiagnostic(Several.Diagnostics, TEXT("'a'"), 3) && HasDiagnostic(Several.Diagnostics, TEXT("'b'"), 4) && HasDiagnostic(Several.Diagnostics, TEXT("'c'"), 6));

	// A broken view still gives the rest of the document (the editor keeps what it can).
	const FVekaParseResult Partial = Veka::Lang::Parse(TEXT("view A {\n    @State var n: Int = 3\n    VStack {\n        Text(#)\n        Text(\"ok\")\n    }\n}"));
	TestTrue(TEXT("partial document keeps its state"), Partial.HasErrors() && Partial.Document.States.Num() == 1);

	// The message format used by the editor and the import log.
	FVekaDiagnostic Diagnostic;
	Diagnostic.Message = FText::FromString(TEXT("Unknown name 'cnt'"));
	Diagnostic.Range.Line = 12;
	Diagnostic.Range.Column = 5;
	TestEqual(TEXT("diagnostic text"), Diagnostic.ToString(), FString(TEXT("12:5: error: Unknown name 'cnt'")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangAnalyzeTest, "VekaUI.Lang.Analyze", VekaLangTestFlags)
bool FVekaLangAnalyzeTest::RunTest(const FString& Parameters)
{
	const FVekaParseResult Result = Veka::Lang::Parse(TEXT(R"VK(view Checked {
    @State var on: Bool = false

    VStack(alignment: .middle) {
        Foo()
        Text("x")
            .font(.huge)
            .foreground(.NotAToken)
        Toggle("t", isOn: true)
        Toggle("u", isOn: on)
        Text("y")
            .bogus(amount: 1)
        Text("z")
            .padding(width: 3)
            .opacity(.half)
        Divider {
            Text("inside")
        }
        Text("w")
            .padding(8) {
                on = true
            }
    }
}
)VK"));
	TestFalse(FString::Printf(TEXT("parses:\n%s"), *Diagnostics(Result)), Result.HasErrors());
	const TArray<FVekaDiagnostic> Problems = Veka::Lang::Analyze(Result.Document);
	const FString All = JoinDiagnostics(Problems);
	using ESeverity = EVekaDiagnosticSeverity;
	TestTrue(TEXT("bad enum value: ") + All, HasDiagnostic(Problems, TEXT(".middle"), 4, 0, ESeverity::Error));
	TestTrue(TEXT("enum values listed: ") + All, HasDiagnostic(Problems, TEXT(".Leading")));
	TestTrue(TEXT("unknown component: ") + All, HasDiagnostic(Problems, TEXT("'Foo'"), 5, 9, ESeverity::Error));
	TestTrue(TEXT("bad font: ") + All, HasDiagnostic(Problems, TEXT(".huge"), 7, 0, ESeverity::Error));
	TestTrue(TEXT("bad Colora token: ") + All, HasDiagnostic(Problems, TEXT("NotAToken"), 8, 0, ESeverity::Error));
	TestTrue(TEXT("constant binding: ") + All, HasDiagnostic(Problems, TEXT("'isOn'"), 9, 0, ESeverity::Warning));
	TestFalse(TEXT("state binding is fine: ") + All, HasDiagnostic(Problems, TEXT("'isOn'"), 10));
	TestTrue(TEXT("unknown modifier: ") + All, HasDiagnostic(Problems, TEXT(".bogus"), 11, 0, ESeverity::Error));
	TestTrue(TEXT("unknown argument: ") + All, HasDiagnostic(Problems, TEXT("'width'"), 14, 0, ESeverity::Error));
	TestTrue(TEXT("not a number: ") + All, HasDiagnostic(Problems, TEXT("expects a number"), 15, 0, ESeverity::Error));
	TestTrue(TEXT("no content: ") + All, HasDiagnostic(Problems, TEXT("has no content"), 16, 0, ESeverity::Error));
	TestTrue(TEXT("no actions: ") + All, HasDiagnostic(Problems, TEXT("takes no actions"), 19, 0, ESeverity::Error));
	TestEqual(TEXT("nothing else reported: ") + All, Problems.Num(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaLangLocalizationTest, "VekaUI.Lang.Localization", VekaLangTestFlags)
bool FVekaLangLocalizationTest::RunTest(const FString& Parameters)
{
	auto FirstText = [](const FVekaParseResult& Result) -> FText
	{
		for (const FVekaAstExpr& Expr : Result.Document.Expressions)
		{
			if (Expr.Value.Type == EVekaValueType::Text)
			{
				return Expr.Value.TextValue;
			}
		}
		return FText();
	};
	const FString Source = TEXT("view Greeter {\n    Text(\"Hello\")\n}\n");
	const FText A = FirstText(Veka::Lang::Parse(Source));
	const FText B = FirstText(Veka::Lang::Parse(Source));
	TestEqual(TEXT("namespace from the view name"), FTextInspector::GetNamespace(A).Get(FString()), FString(TEXT("Veka.Greeter")));
	TestFalse(TEXT("has a key"), FTextInspector::GetKey(A).Get(FString()).IsEmpty());
	TestEqual(TEXT("stable key"), FTextInspector::GetKey(A).Get(FString()), FTextInspector::GetKey(B).Get(FString()));
	TestEqual(TEXT("source string"), A.ToString(), FString(TEXT("Hello")));

	const FText Other = FirstText(Veka::Lang::Parse(TEXT("view Greeter {\n    Text(\"Bye\")\n}\n")));
	TestNotEqual(TEXT("other text, other key"), FTextInspector::GetKey(Other).Get(FString()), FTextInspector::GetKey(A).Get(FString()));

	FVekaParseOptions Options;
	Options.TextNamespace = TEXT("MyGame.HUD");
	TestEqual(TEXT("namespace option"), FTextInspector::GetNamespace(FirstText(Veka::Lang::Parse(Source, Options))).Get(FString()), FString(TEXT("MyGame.HUD")));
	return true;
}

#endif
