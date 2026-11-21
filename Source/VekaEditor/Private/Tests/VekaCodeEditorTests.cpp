// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3c: the Code tab — colors, completion, apply while typing, undo, errors that erase nothing, the shop in code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CodeEditor/SVekaCodeEditor.h"
#include "CodeEditor/VekaCodeAssist.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaCodeTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UVekaView* MakeView(const TCHAR* Name, FVekaDocument Document)
	{
		static int32 Counter = 0;
		const FString Unique = FString::Printf(TEXT("%s_%d"), Name, ++Counter);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/VekaCodeEditorTests/%s"), *Unique));
		UVekaView* View = NewObject<UVekaView>(Package, FName(*Unique), RF_Public | RF_Standalone | RF_Transactional);
		View->Document = MoveTemp(Document);
		return View;
	}

	void Discard(UVekaView* View)
	{
		View->ClearFlags(RF_Public | RF_Standalone);
		View->MarkAsGarbage();
	}

	bool HasSpan(const TArray<FVekaCodeSpan>& Spans, const FString& Line, const FString& Text, EVekaCodeToken Kind)
	{
		return Spans.ContainsByPredicate([&](const FVekaCodeSpan& Span) { return Span.Kind == Kind && Line.Mid(Span.Start, Span.Length) == Text; });
	}

	/** Completion with the cursor where `|` is. */
	FVekaCompletion CompleteAt(const FString& TextWithCursor, const FVekaCompletionSources& Sources, bool bExplicit = false)
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
		return Veka::Editor::ComputeCompletion(Text, Line, Cursor - LineStart, Sources, bExplicit);
	}

	bool Offers(const FVekaCompletion& Completion, const FString& Label)
	{
		return Completion.Items.ContainsByPredicate([&Label](const FVekaCompletionItem& Item) { return Item.Label == Label; });
	}

	FString Labels(const FVekaCompletion& Completion)
	{
		TArray<FString> Out;
		for (int32 Index = 0; Index < FMath::Min(Completion.Items.Num(), 12); ++Index)
		{
			Out.Add(Completion.Items[Index].Label);
		}
		return FString::Join(Out, TEXT(", "));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaCodeTokenizeTest, "VekaUI.Editor.Code.Tokenize", VekaCodeTestFlags)
bool FVekaCodeTokenizeTest::RunTest(const FString& Parameters)
{
	const FString Line = TEXT("        Text(\"Hi \\(n)\", x: 1.5).font(.title) // note");
	const TArray<FVekaCodeSpan> Spans = Veka::Editor::TokenizeLine(Line);
	TestTrue(TEXT("component"), HasSpan(Spans, Line, TEXT("Text"), EVekaCodeToken::Component));
	TestTrue(TEXT("string with interpolation"), HasSpan(Spans, Line, TEXT("\"Hi \\(n)\""), EVekaCodeToken::String));
	TestTrue(TEXT("number"), HasSpan(Spans, Line, TEXT("1.5"), EVekaCodeToken::Number));
	TestTrue(TEXT("modifier"), HasSpan(Spans, Line, TEXT(".font"), EVekaCodeToken::Modifier));
	TestTrue(TEXT("enum value"), HasSpan(Spans, Line, TEXT(".title"), EVekaCodeToken::EnumValue));
	TestTrue(TEXT("comment"), HasSpan(Spans, Line, TEXT("// note"), EVekaCodeToken::Comment));

	const FString Declaration = TEXT("    @State var tint: Color = #FF8800");
	const TArray<FVekaCodeSpan> DeclarationSpans = Veka::Editor::TokenizeLine(Declaration);
	TestTrue(TEXT("declaration"), HasSpan(DeclarationSpans, Declaration, TEXT("@State"), EVekaCodeToken::Declaration));
	TestTrue(TEXT("keyword"), HasSpan(DeclarationSpans, Declaration, TEXT("var"), EVekaCodeToken::Keyword));
	TestTrue(TEXT("hex color"), HasSpan(DeclarationSpans, Declaration, TEXT("#FF8800"), EVekaCodeToken::Color));

	const FString Member = TEXT("            .modifier(item.Name)");
	const TArray<FVekaCodeSpan> MemberSpans = Veka::Editor::TokenizeLine(Member);
	TestTrue(TEXT("line-start modifier"), HasSpan(MemberSpans, Member, TEXT(".modifier"), EVekaCodeToken::Modifier));
	TestFalse(TEXT("member access is not a modifier"), HasSpan(MemberSpans, Member, TEXT(".Name"), EVekaCodeToken::Modifier));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaCodeCompletionTest, "VekaUI.Editor.Code.Completion", VekaCodeTestFlags)
bool FVekaCodeCompletionTest::RunTest(const FString& Parameters)
{
	const FVekaDocument Shop = Veka::Samples::MakeShopDocument(nullptr);
	FVekaCompletionSources Sources;
	Sources.Document = &Shop;
	Sources.ModelClass = UVekaSampleViewModel::StaticClass();
	Sources.ProjectViews = { TEXT("VV_ShopItem") };
	const FString Head = TEXT("view A {\n    @State var gift: Bool = false\n    VStack {\n");

	FVekaCompletion C = CompleteAt(Head + TEXT("        Text(\"a\")\n            .|\n    }\n}"), Sources);
	TestTrue(TEXT("modifiers after a view: ") + Labels(C), Offers(C, TEXT("font")) && Offers(C, TEXT("padding")) && Offers(C, TEXT("inSlot")));
	TestFalse(TEXT("no components after a dot"), Offers(C, TEXT("VStack")));

	C = CompleteAt(Head + TEXT("        Text(\"a\")\n            .pad|"), Sources);
	TestTrue(TEXT("filtered by what is typed: ") + Labels(C), Offers(C, TEXT("padding")) && !Offers(C, TEXT("font")));
	TestEqual(TEXT("replaces the typed prefix"), C.ReplaceEnd - C.ReplaceStart, 3);

	C = CompleteAt(Head + TEXT("        Text(\"a\")\n            .font(.|"), Sources);
	TestTrue(TEXT("enum values of the argument: ") + Labels(C), Offers(C, TEXT("Title1")) && Offers(C, TEXT("Headline")));

	C = CompleteAt(Head + TEXT("        VStack(spacing: 4, alignment: .|"), Sources);
	TestTrue(TEXT("named enum argument: ") + Labels(C), Offers(C, TEXT("Leading")));

	C = CompleteAt(Head + TEXT("        Rectangle(fill: .Or|"), Sources);
	TestTrue(TEXT("Colora tokens: ") + Labels(C), Offers(C, TEXT("Orange")));

	C = CompleteAt(TEXT("view A {\n    @|"), Sources);
	TestTrue(TEXT("declarations: ") + Labels(C), Offers(C, TEXT("State")) && Offers(C, TEXT("Event")));

	C = CompleteAt(Head + TEXT("        VSt|"), Sources);
	TestTrue(TEXT("components at a line start: ") + Labels(C), Offers(C, TEXT("VStack")));
	TestEqual(TEXT("a container opens its call"), C.Items.IsEmpty() ? FString() : C.Items[0].Insert, FString(TEXT("VStack(")));

	C = CompleteAt(Head + TEXT("        VV_|"), Sources);
	TestTrue(TEXT("views of the project: ") + Labels(C), Offers(C, TEXT("VV_ShopItem")));

	C = CompleteAt(Head + TEXT("        Toggle(\"Wrap\", |"), Sources, true);
	TestTrue(TEXT("argument names and states: ") + Labels(C), Offers(C, TEXT("isOn:")) && Offers(C, TEXT("gift")));
	TestFalse(TEXT("the primary argument is already given"), Offers(C, TEXT("title:")));

	C = CompleteAt(Head + TEXT("        Symbol(#\"chec|"), Sources);
	TestTrue(TEXT("symbol names: ") + Labels(C), Offers(C, TEXT("checkmark")));

	C = CompleteAt(Head + TEXT("        Text(model.|"), Sources);
	TestTrue(TEXT("model properties: ") + Labels(C), Offers(C, TEXT("Gold")) && Offers(C, TEXT("Items")));

	C = CompleteAt(Head + TEXT("        Button(\"x\") {\n            gift.|"), Sources);
	TestTrue(TEXT("toggle on a state: ") + Labels(C), Offers(C, TEXT("toggle()")));

	C = CompleteAt(Head + TEXT("        // VSt|"), Sources, true);
	TestTrue(TEXT("nothing in a comment"), C.Items.IsEmpty());
	C = CompleteAt(Head + TEXT("        |"), Sources, false);
	TestTrue(TEXT("nothing typed: no popup unless asked"), C.Items.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaCodeApplyTest, "VekaUI.Editor.Code.Apply", VekaCodeTestFlags)
bool FVekaCodeApplyTest::RunTest(const FString& Parameters)
{
	UVekaView* View = MakeView(TEXT("VV_CodeApply"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	const TSharedRef<SVekaCodeEditor> Code = SNew(SVekaCodeEditor).View(View);
	const FString Printed = Veka::Lang::Print(View->Document, View->GetFName());
	TestEqual(TEXT("the code tab shows the document"), Code->GetText(), Printed);
	const FVekaDocument Before = View->Document;

	// Valid code becomes the document; the user's layout (comment, blank line) is kept with the asset.
	FString Edited = Printed.Replace(TEXT("VStack(spacing: 12)"), TEXT("VStack(spacing: 20)"));
	Edited = TEXT("// My counter\n") + Edited.Replace(TEXT("        Button("), TEXT("\n        Button("));
	Code->SetText(Edited);
	TestTrue(TEXT("not applied before the pause"), Veka::Lang::StructurallyEqual(View->Document, Before));
	Code->FlushParse();
	TestFalse(TEXT("valid"), Code->HasErrors());
	TestTrue(TEXT("applied"), Veka::Lang::StructurallyEqual(View->Document, Veka::Lang::Parse(Edited).Document));
	TestEqual(TEXT("code kept with the asset"), View->SourceText, Edited);
	TestEqual(TEXT("shown as written"), SVekaCodeEditor::GetDisplayText(View), Edited);

	// Undo / redo bring back the document and its code.
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	Code->OnDocumentChanged();
	TestTrue(TEXT("undo restores the document"), Veka::Lang::StructurallyEqual(View->Document, Before));
	TestEqual(TEXT("undo restores the code"), Code->GetText(), Printed);
	TestTrue(TEXT("redo"), GEditor->RedoTransaction());
	Code->OnDocumentChanged();
	TestEqual(TEXT("redo restores the written code"), Code->GetText(), Edited);

	// An error erases nothing: the text stays, the document keeps its last valid version.
	const FVekaDocument Valid = View->Document;
	const FString Broken = Edited.Replace(TEXT("Text(\"Hello Veka\")"), TEXT("Text(cnt)"));
	Code->SetText(Broken);
	Code->FlushParse();
	TestTrue(TEXT("error found"), Code->HasErrors());
	TestTrue(TEXT("error message with its line"), Code->GetDiagnostics().ContainsByPredicate([](const FVekaDiagnostic& Diagnostic)
	{
		return Diagnostic.Message.ToString().Contains(TEXT("cnt")) && Diagnostic.Range.Line == 6;
	}));
	TestEqual(TEXT("the broken text is still there"), Code->GetText(), Broken);
	TestTrue(TEXT("the document is untouched"), Veka::Lang::StructurallyEqual(View->Document, Valid));

	// The document changes elsewhere while the code is broken: the code is kept, a reload is offered.
	Veka::Editor::ApplyDocument(View, Veka::Samples::MakeStarterDocument(), INVTEXT("Other tool"));
	Code->OnDocumentChanged();
	TestTrue(TEXT("outside change noticed"), Code->HasPendingExternalChange());
	TestEqual(TEXT("broken text still kept"), Code->GetText(), Broken);
	Code->ReloadFromDocument();
	TestEqual(TEXT("reload shows the document"), Code->GetText(), Veka::Lang::Print(View->Document, View->GetFName()));
	TestTrue(TEXT("other tools drop the stored code"), View->SourceText.IsEmpty());

	// Format: canonical layout.
	Code->SetText(TEXT("view ") + View->GetName() + TEXT(" {@State var count:Int=0\nVStack(spacing:12){Text(\"Hello Veka\").font(.Title2)\nButton(\"Tapped \\(count) times\"){withAnimation{count+=1}}}.padding(24)}"));
	Code->Format();
	TestEqual(TEXT("formatted"), Code->GetText(), Veka::Lang::Print(View->Document, View->GetFName()));
	TestFalse(TEXT("formatted code is valid"), Code->HasErrors());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaCodeShopTest, "VekaUI.Editor.Code.Shop", VekaCodeTestFlags)
bool FVekaCodeShopTest::RunTest(const FString& Parameters)
{
	// The milestone: the shop written entirely as code in a blank view (VV_ShopItem: /Game/VekaSamples, from
	// Veka.CreateSampleAssets).
	FString Golden;
	const FString Path = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VekaUI"))->GetBaseDir(), TEXT("Samples"), TEXT("Veka"), TEXT("VV_Shop.vk"));
	if (!TestTrue(TEXT("VV_Shop.vk"), FFileHelper::LoadFileToString(Golden, *Path)))
	{
		return false;
	}
	UVekaView* View = MakeView(TEXT("VV_CodeShop"), FVekaDocument());
	ON_SCOPE_EXIT { Discard(View); };
	const TSharedRef<SVekaCodeEditor> Code = SNew(SVekaCodeEditor).View(View);
	const FString Source = Golden.Replace(TEXT("view VV_Shop {"), *FString::Printf(TEXT("view %s {"), *View->GetName()));
	Code->SetText(Source);
	Code->FlushParse();
	FString Problems;
	for (const FVekaDiagnostic& Diagnostic : Code->GetDiagnostics())
	{
		Problems += Diagnostic.ToString() + TEXT("\n");
	}
	if (!TestFalse(TEXT("the shop code is valid:\n") + Problems, Code->HasErrors()))
	{
		return false;
	}
	FVekaParseOptions Options;
	Options.ResolveComponent = Veka::Lang::MakeAssetResolver();
	FString Difference;
	TestTrue(FString::Printf(TEXT("the view is the shop (%s)"), *Difference), Veka::Lang::StructurallyEqual(View->Document, Veka::Lang::Parse(Source, Options).Document, &Difference));
	TestTrue(TEXT("its component is the project's VV_ShopItem"), View->Document.Nodes.ContainsByPredicate([](const FVekaAstNode& Node)
	{
		return Node.Component && Node.Component->GetFName() == TEXT("VV_ShopItem");
	}));
	return true;
}

#endif
