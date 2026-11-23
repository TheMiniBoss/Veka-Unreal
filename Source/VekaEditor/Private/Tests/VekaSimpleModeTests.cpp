// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3d: the Simple mode — document edits, paths, shared selection, viewport picking, panels, round trip with code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SimpleMode/VekaDocumentEdit.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/SVekaOutliner.h"
#include "SimpleMode/SVekaInspector.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "CodeEditor/SVekaCodeEditor.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

namespace
{
	using namespace Veka::Editor;
	constexpr EAutomationTestFlags VekaSimpleTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UVekaView* MakeView(const TCHAR* Name, FVekaDocument Document)
	{
		static int32 Counter = 0;
		const FString Unique = FString::Printf(TEXT("%s_%d"), Name, ++Counter);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/VekaSimpleTests/%s"), *Unique));
		UVekaView* View = NewObject<UVekaView>(Package, FName(*Unique), RF_Public | RF_Standalone | RF_Transactional);
		View->Document = MoveTemp(Document);
		return View;
	}

	void Discard(UVekaView* View)
	{
		View->ClearFlags(RF_Public | RF_Standalone);
		View->MarkAsGarbage();
	}

	int32 FindNode(const FVekaDocument& Document, FName Type, int32 Occurrence = 0)
	{
		for (int32 Index = 0; Index < Document.Nodes.Num(); ++Index)
		{
			if (Document.Nodes[Index].Type == Type && Occurrence-- == 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 Code(FVekaDocument& Document, const FString& Source, TConstArrayView<FName> Locals = {})
	{
		TArray<FVekaDiagnostic> Diagnostics;
		return Veka::Lang::ParseExpression(Document, Source, Locals, TEXT("Veka.Test"), Diagnostics);
	}

	void PaintOnce(const TSharedRef<SWidget>& Widget, const FVector2f& Size = FVector2f(1200.f, 800.f))
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(Size).CreateTitleBar(false);
		FSlateWindowElementList Elements(Window);
		FHittestGrid Grid;
		Grid.SetHittestArea(FVector2f::ZeroVector, Size);
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Widget->SlatePrepass(1.f);
			Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}
	}

	/** Diagnostics of a document as one string (empty: clean). */
	FString Problems(const FVekaDocument& Document)
	{
		FString Out;
		for (const FVekaDiagnostic& Diagnostic : Veka::Lang::Analyze(Document))
		{
			if (Diagnostic.Severity == EVekaDiagnosticSeverity::Error)
			{
				Out += Diagnostic.ToString() + TEXT("\n");
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSimpleBuildTest, "VekaUI.Editor.Simple.Build", VekaSimpleTestFlags)
bool FVekaSimpleBuildTest::RunTest(const FString& Parameters)
{
	// The milestone: a complete view made only with the Simple mode's edits (what the panels call).
	FVekaDocument Document;
	const int32 Root = Doc::Insert(Document, TEXT("VStack"), INDEX_NONE, false, INDEX_NONE, nullptr, TEXT("Veka.Test"));
	TestEqual(TEXT("first view is the root"), Document.Root, Root);
	Doc::AddState(Document, Doc::UniqueName(Document, TEXT("count")), FVekaValue::Int(0));
	Doc::AddState(Document, Doc::UniqueName(Document, TEXT("on")), FVekaValue::Bool(true));
	Doc::AddEvent(Document, TEXT("Saved"), EVekaValueType::Int);
	TestEqual(TEXT("unique names"), Doc::UniqueName(Document, TEXT("count")), FName(TEXT("count2")));

	const int32 Title = Doc::Insert(Document, TEXT("Text"), Root, false, INDEX_NONE, nullptr, TEXT("Veka.Test"));
	Doc::SetArgument(Document, Title, INDEX_NONE, TEXT("text"), Code(Document, TEXT("\"Tapped \\(count) times\"")));
	const int32 Font = Doc::AddModifier(Document, Title, TEXT("font"));
	Doc::SetArgument(Document, Title, Font, TEXT("style"), Doc::AddLiteral(Document, FVekaValue::String(TEXT("Title2"))));
	Doc::SetLabel(Document, Title, TEXT("Counter title"));

	const int32 Row = Doc::Insert(Document, TEXT("HStack"), Root, false, INDEX_NONE);
	const int32 Button = Doc::Insert(Document, TEXT("Button"), Row, false, INDEX_NONE, nullptr, TEXT("Veka.Test"));
	FVekaAstAction& Add = Doc::Actions(Document, Button, INDEX_NONE)->AddDefaulted_GetRef();
	Add.Kind = EVekaActionKind::Increment;
	Add.Target = TEXT("count");
	Add.bAnimated = true;
	FVekaAstAction& Emit = Doc::Actions(Document, Button, INDEX_NONE)->AddDefaulted_GetRef();
	Emit.Kind = EVekaActionKind::Emit;
	Emit.Target = TEXT("Saved");
	Emit.Expr = Code(Document, TEXT("count"));
	const int32 Toggle = Doc::Insert(Document, TEXT("Toggle"), Row, false, INDEX_NONE, nullptr, TEXT("Veka.Test"));
	TestEqual(TEXT("a new Toggle binds the first Bool state"), Veka::Lang::PrintExpression(Document, Doc::FindArgument(Document, Toggle, INDEX_NONE, TEXT("isOn"))), FString(TEXT("on")));

	const int32 Condition = Doc::Insert(Document, TEXT("if"), Root, false, INDEX_NONE);
	Document.Nodes[Condition].Expr = Code(Document, TEXT("count > 3"));
	Doc::Insert(Document, TEXT("Text"), Condition, false, INDEX_NONE, nullptr, TEXT("Veka.Test"));
	Doc::Insert(Document, TEXT("Divider"), Condition, true, INDEX_NONE);

	const int32 Padding = Doc::AddModifier(Document, Root, TEXT("padding"));
	const int32 Background = Doc::AddModifier(Document, Root, TEXT("background"));
	Doc::MoveModifier(Document, Root, Background, Padding);
	TestEqual(TEXT("modifier moved first"), Document.Nodes[Root].Modifiers[0].Name, FName(TEXT("background")));

	// Move, duplicate, wrap, remove.
	TestTrue(TEXT("move the button after the title"), Doc::Move(Document, Button, Root, false, 1));
	TestFalse(TEXT("a view cannot go inside itself"), Doc::Move(Document, Root, Row, false, 0));
	const int32 Copy = Doc::Duplicate(Document, Title);
	TestEqual(TEXT("duplicate right after"), Doc::FindLocation(Document, Copy).Index, Doc::FindLocation(Document, Title).Index + 1);
	const int32 Wrapper = Doc::Wrap(Document, { Title, Copy }, TEXT("ZStack"));
	TestEqual(TEXT("wrapped together"), Document.Nodes[Wrapper].Children.Num(), 2);
	Doc::Remove(Document, { Copy });

	// Renaming a state renames its uses.
	TestTrue(TEXT("rename"), Doc::RenameDeclaration(Document, TEXT("count"), TEXT("taps")));
	TestFalse(TEXT("names stay unique"), Doc::RenameDeclaration(Document, TEXT("taps"), TEXT("on")));
	Doc::Compact(Document);

	const FString Printed = Veka::Lang::Print(Document, TEXT("Counter"));
	TestTrue(TEXT("uses renamed: ") + Printed, Printed.Contains(TEXT("taps += 1")) && Printed.Contains(TEXT("\\(taps)")) && !Printed.Contains(TEXT("count"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("label kept as a comment"), Printed.Contains(TEXT("// Counter title")));
	TestTrue(TEXT("compact leaves no orphan"), Document.Nodes.Num() == 9);
	TestTrue(TEXT("valid view:\n") + Problems(Document), Problems(Document).IsEmpty());

	// Simple -> Code -> Simple: nothing lost.
	const FVekaParseResult Reparsed = Veka::Lang::Parse(Printed);
	FString Difference;
	TestFalse(TEXT("the code parses"), Reparsed.HasErrors());
	TestTrue(FString::Printf(TEXT("round trip (%s):\n%s"), *Difference, *Printed), Veka::Lang::StructurallyEqual(Reparsed.Document, Document, &Difference));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSimpleRoundTripTest, "VekaUI.Editor.Simple.RoundTrip", VekaSimpleTestFlags)
bool FVekaSimpleRoundTripTest::RunTest(const FString& Parameters)
{
	// Code -> Simple edits -> Code: the shop keeps its comments, components and everything not edited.
	UVekaView* Item = MakeView(TEXT("VV_SimpleItem"), Veka::Samples::MakeShopItemDocument());
	ON_SCOPE_EXIT { Discard(Item); };
	FVekaParseOptions Options;
	Options.ResolveComponent = [Item](FName Name) { return Name == Item->GetFName() ? Item : nullptr; };
	const FString Source = Veka::Lang::Print(Veka::Samples::MakeShopDocument(Item), TEXT("Shop"));
	FVekaDocument Document = Veka::Lang::Parse(Source, Options).Document;
	const FVekaDocument Original = Document;

	const int32 Component = Document.Nodes.IndexOfByPredicate([](const FVekaAstNode& Node) { return Node.Component != nullptr; });
	const int32 Opacity = Doc::AddModifier(Document, Component, TEXT("opacity"));
	TestEqual(TEXT("default value"), Veka::Lang::PrintExpression(Document, Doc::FindArgument(Document, Component, Opacity, TEXT("value"))), FString(TEXT("0.5")));
	Doc::RemoveModifier(Document, Component, Opacity);
	Doc::Compact(Document);
	FString Difference;
	TestTrue(FString::Printf(TEXT("add then remove leaves the shop as it was (%s)"), *Difference), Veka::Lang::StructurallyEqual(Document, Original, &Difference));

	// A real edit survives printing and parsing, comments included.
	const int32 Toggle = FindNode(Document, TEXT("Toggle"));
	Doc::SetLabel(Document, Toggle, TEXT("Gift option"));
	Doc::AddModifier(Document, Toggle, TEXT("padding"));
	Doc::Compact(Document);
	const FString Printed = Veka::Lang::Print(Document, TEXT("Shop"));
	const FVekaParseResult Again = Veka::Lang::Parse(Printed, Options);
	TestTrue(FString::Printf(TEXT("edited shop round trips (%s)"), *Difference), Veka::Lang::StructurallyEqual(Again.Document, Document, &Difference));
	TestTrue(TEXT("the shop's own comment is still there"), Printed.Contains(TEXT("// One component per article of the model.")));

	// Paths survive printing / parsing (selection kept across code edits).
	for (int32 Node = 0; Node < Document.Nodes.Num(); ++Node)
	{
		const TArray<int32> Path = Doc::Path(Document, Node);
		TestEqual(FString::Printf(TEXT("path of node %d"), Node), Doc::FromPath(Document, Path), Node);
		const int32 InParsed = Doc::FromPath(Again.Document, Path);
		TestTrue(FString::Printf(TEXT("same node after a re-parse (%d)"), Node), Again.Document.Nodes.IsValidIndex(InParsed) && Again.Document.Nodes[InParsed].Type == Document.Nodes[Node].Type);
	}
	// The line of a node leads back to it.
	const int32 TextAtLine = Doc::NodeAtLine(Again.Document, Again.Document.Nodes[FindNode(Again.Document, TEXT("Toggle"))].Source.Line);
	TestEqual(TEXT("node at its line"), TextAtLine, FindNode(Again.Document, TEXT("Toggle")));
	TestTrue(TEXT("loop variable in scope inside ForEach"), Doc::LocalsAt(Document, Component).Contains(FName(TEXT("item"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSimpleContextTest, "VekaUI.Editor.Simple.Context", VekaSimpleTestFlags)
bool FVekaSimpleContextTest::RunTest(const FString& Parameters)
{
	UVekaView* View = MakeView(TEXT("VV_SimpleContext"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	const TSharedRef<FVekaViewEditorContext> Context = MakeShared<FVekaViewEditorContext>(View);
	int32 SelectionEvents = 0;
	Context->OnSelectionChanged.AddLambda([&SelectionEvents]() { ++SelectionEvents; });

	const int32 Label = FindNode(View->Document, TEXT("Text"));
	Context->Select({ Label }, EVekaSelectionSource::Outliner);
	TestEqual(TEXT("selected"), Context->GetPrimarySelection(), Label);
	TestEqual(TEXT("source kept"), Context->GetLastSelectionSource(), EVekaSelectionSource::Outliner);

	// An edit selects what it made, as one undo step.
	TestTrue(TEXT("palette insert"), InsertAtSelection(*Context, TEXT("Divider"), nullptr));
	const int32 Divider = Context->GetPrimarySelection();
	TestTrue(TEXT("the new view is selected"), View->Document.Nodes.IsValidIndex(Divider) && View->Document.Nodes[Divider].Type == TEXT("Divider"));
	TestEqual(TEXT("inserted after the text (not a container)"), Doc::FindLocation(View->Document, Divider).Index, 1);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	Context->NotifyDocumentChanged();
	TestEqual(TEXT("undo removed it"), FindNode(View->Document, TEXT("Divider")), INDEX_NONE);
	TestTrue(TEXT("stale selection dropped"), Context->GetSelectedNodes().IsEmpty() || View->Document.Nodes[Context->GetPrimarySelection()].Type != TEXT("Divider"));

	// Read-only while the code has errors: nothing changes.
	bool bReadOnly = true;
	Context->ReadOnly = [&bReadOnly]() { return bReadOnly; };
	const FVekaDocument Before = View->Document;
	TestFalse(TEXT("refused while read-only"), InsertAtSelection(*Context, TEXT("Text"), nullptr));
	TestTrue(TEXT("document untouched"), Veka::Lang::StructurallyEqual(View->Document, Before));
	bReadOnly = false;
	TestTrue(SelectionEvents > 0 ? TEXT("selection events") : TEXT("selection events (none)"), SelectionEvents > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSimplePickTest, "VekaUI.Editor.Simple.Pick", VekaSimpleTestFlags)
bool FVekaSimplePickTest::RunTest(const FString& Parameters)
{
	UVekaView* View = MakeView(TEXT("VV_SimplePick"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	const TSharedRef<FVekaViewEditorContext> Context = MakeShared<FVekaViewEditorContext>(View);
	const TSharedRef<SVekaViewPreview> Preview = SNew(SVekaViewPreview).View(View).bPersistSettings(false);
	Preview->SetContext(Context);
	PaintOnce(Preview);

	// Every runtime view built from the document knows its node.
	const int32 Button = FindNode(View->Document, TEXT("Button"));
	const FVekaNode* ButtonNode = nullptr;
	TFunction<void(const FVekaNode&)> Find = [&](const FVekaNode& Node)
	{
		if (Node.GetSourceElement().GetSourceDocument() == &View->Document && Node.GetSourceElement().GetSourceNode() == Button)
		{
			ButtonNode = &Node;
		}
		for (const TSharedRef<FVekaNode>& Child : Node.GetChildren())
		{
			Find(*Child);
		}
	};
	Find(*Preview->GetHost()->GetRuntime()->GetRootNode());
	if (!TestNotNull(TEXT("the button's runtime view is tagged with its node"), ButtonNode))
	{
		return false;
	}
	const FVector2D Center = ButtonNode->GetRootPosition() + ButtonNode->GetArrangedRect().Size * 0.5;
	TestEqual(TEXT("a click on the button picks it"), Preview->PickNode(Center), Button);
	TestEqual(TEXT("off the view: nothing"), Preview->PickNode(FVector2D(-50.0, -50.0)), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSimplePanelsTest, "VekaUI.Editor.Simple.Panels", VekaSimpleTestFlags)
bool FVekaSimplePanelsTest::RunTest(const FString& Parameters)
{
	UAssetEditorSubsystem* Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	UVekaView* View = MakeView(TEXT("VV_SimplePanels"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	if (!TestNotNull(TEXT("asset editors"), Editors) || !TestTrue(TEXT("opens"), Editors->OpenEditorForAsset(View)))
	{
		return false;
	}
	ON_SCOPE_EXIT { Editors->CloseAllEditorsForAsset(View); };
	FVekaViewEditorToolkit* Toolkit = static_cast<FVekaViewEditorToolkit*>(Editors->FindEditorForAsset(View, false));
	const TSharedPtr<FVekaViewEditorContext> Context = Toolkit->GetContext();
	if (!TestTrue(TEXT("Simple mode panels"), Context.IsValid() && Toolkit->GetOutliner().IsValid() && Toolkit->GetInspector().IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("the outliner shows every view"), Toolkit->GetOutliner()->GetItemCount(), View->Document.Nodes.Num());

	// Inspector edits: a text, a constant, an expression; a wrong expression changes nothing.
	const int32 Text = FindNode(View->Document, TEXT("Text"));
	Context->Select({ Text }, EVekaSelectionSource::Outliner);
	Toolkit->GetInspector()->ApplyArgumentText(Text, INDEX_NONE, TEXT("text"), TEXT("Welcome"));
	TestTrue(TEXT("text edited"), Toolkit->GetCodeEditor()->GetText().Contains(TEXT("Text(\"Welcome\")")));
	const int32 Selected = Context->GetPrimarySelection();
	TestEqual(TEXT("selection follows the edit"), View->Document.Nodes.IsValidIndex(Selected) ? View->Document.Nodes[Selected].Type : NAME_None, FName(TEXT("Text")));
	Toolkit->GetInspector()->ApplyArgumentValue(Selected, 0, TEXT("style"), FVekaValue::String(TEXT("Headline")));
	TestTrue(TEXT("enum edited"), Toolkit->GetCodeEditor()->GetText().Contains(TEXT(".font(.Headline)")));
	TestTrue(TEXT("expression edited"), Toolkit->GetInspector()->ApplyArgumentCode(Selected, INDEX_NONE, TEXT("text"), TEXT("\"\\(count) taps\"")));
	TestTrue(TEXT("expression shown in code"), Toolkit->GetCodeEditor()->GetText().Contains(TEXT("Text(\"\\(count) taps\")")));
	const FVekaDocument Before = View->Document;
	TestFalse(TEXT("a wrong expression is refused"), Toolkit->GetInspector()->ApplyArgumentCode(Selected, INDEX_NONE, TEXT("text"), TEXT("nope +")));
	TestTrue(TEXT("and changes nothing"), Veka::Lang::StructurallyEqual(View->Document, Before));

	// While the code has errors the Simple mode is read-only.
	Toolkit->GetCodeEditor()->SetText(Toolkit->GetCodeEditor()->GetText().Replace(TEXT("count += 1"), TEXT("cnt += 1")));
	Toolkit->GetCodeEditor()->FlushParse();
	TestTrue(TEXT("code error"), Toolkit->GetCodeEditor()->HasErrors());
	TestTrue(TEXT("Simple mode read-only"), Context->IsReadOnly());
	TestFalse(TEXT("palette refused"), InsertAtSelection(*Context, TEXT("Divider"), nullptr));
	Toolkit->GetCodeEditor()->ReloadFromDocument();
	TestFalse(TEXT("editable again"), Context->IsReadOnly());

	// Code cursor <-> selection.
	Context->Select({ FindNode(View->Document, TEXT("Button")) }, EVekaSelectionSource::Outliner);
	const FVekaDocument& Parsed = Toolkit->GetCodeEditor()->GetLastParsed();
	TestTrue(TEXT("the code knows where the button is"), Parsed.Nodes.IsValidIndex(Doc::FromPath(Parsed, Context->GetSelectedPaths().Last())));
	return true;
}

#endif
