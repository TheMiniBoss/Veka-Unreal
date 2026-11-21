// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3e: timelines in documents (language, runtime), the Test mode and the Timeline tab.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TestMode/SVekaTestPanels.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/SVekaInspector.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "CodeEditor/SVekaCodeEditor.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Primitives/VekaText.h"
#include "Anima/VekaTimeline.h"
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
	constexpr EAutomationTestFlags VekaTestModeFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	const TCHAR* FadeSource = TEXT(R"VK(view Fade {
    @State var done: Bool = false
    @State var replays: Int = 0
    @Timeline Intro(length: 1.0, autoplay: true) {
        track fade { 0.0: 0.0, 0.4: 1.0 .EaseOut }
        track lift { 0.0: -24.0, 0.5: 0.0 .Linear }
        at 0.5 {
            done = true
        }
    }
    @Timeline Pulse(loop: true, speed: 2.0) {
        track scale { 0.0: 1.0, 0.5: 1.2, 1.0: 1.0 }
    }

    VStack(spacing: 8) {
        Text("Hello")
            .opacity(Intro.fade)
            .offset(y: Intro.lift)
        Button("Replay") {
            replays += 1
            Intro.stop()
            Intro.play()
            Pulse.seek(0.25)
        }
    }
}
)VK");

	UVekaView* MakeView(const TCHAR* Name, FVekaDocument Document)
	{
		static int32 Counter = 0;
		const FString Unique = FString::Printf(TEXT("%s_%d"), Name, ++Counter);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/VekaTestModeTests/%s"), *Unique));
		UVekaView* View = NewObject<UVekaView>(Package, FName(*Unique), RF_Public | RF_Standalone | RF_Transactional);
		View->Document = MoveTemp(Document);
		return View;
	}

	void Discard(UVekaView* View)
	{
		View->ClearFlags(RF_Public | RF_Standalone);
		View->MarkAsGarbage();
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

	/** Texts equal to Value, in tree order. */
	void FindTexts(const TSharedPtr<FVekaNode>& Node, const FString& Value, TArray<TSharedPtr<FVekaNode>>& Out)
	{
		if (!Node.IsValid())
		{
			return;
		}
		const FVekaText* Text = Node->GetPrimitiveAs<FVekaText>();
		if (Text && !Node->IsInRemovedSubtree() && Text->Text.ToString() == Value)
		{
			Out.Add(Node);
		}
		for (const TSharedRef<FVekaNode>& Child : Node->GetChildren())
		{
			FindTexts(Child, Value, Out);
		}
	}

	/** A click on a view, through Slate (what the Test mode lets the user do). */
	void Tap(FVekaRuntime& Runtime, const TSharedPtr<FVekaNode>& Start)
	{
		TSharedPtr<FVekaNode> Node = Start;
		for (FVekaNode* Current = Start.Get(); Current; Current = Current->GetParent())
		{
			if (Current->WantsInput())
			{
				Node = Current->AsShared();
				break;
			}
		}
		if (!Node.IsValid())
		{
			return;
		}
		const TSharedRef<SWidget> Widget = Node->GetOrCreateWidget();
		const FVector2f Size(Node->GetArrangedRect().Size);
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
		TSet<FKey> Down;
		Down.Add(EKeys::LeftMouseButton);
		Widget->OnMouseButtonDown(Geometry, FPointerEvent(0, Size * 0.5f, Size * 0.5f, Down, EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
		Runtime.Flush();
		Widget->OnMouseButtonUp(Geometry, FPointerEvent(0, Size * 0.5f, Size * 0.5f, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
		Runtime.Flush();
	}

	bool LogHas(const SVekaViewPreview& Preview, const FString& Fragment)
	{
		return Preview.GetLog().ContainsByPredicate([&Fragment](const FVekaPreviewLogEntry& Entry) { return Entry.Text.Contains(Fragment); });
	}

	FString LogText(const SVekaViewPreview& Preview)
	{
		TArray<FString> Lines;
		for (const FVekaPreviewLogEntry& Entry : Preview.GetLog())
		{
			Lines.Add(Entry.Text);
		}
		return FString::Join(Lines, TEXT(" | "));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTimelineLanguageTest, "VekaUI.Lang.Timeline", VekaTestModeFlags)
bool FVekaTimelineLanguageTest::RunTest(const FString& Parameters)
{
	const FVekaParseResult Result = Veka::Lang::Parse(FadeSource);
	FString Problems;
	for (const FVekaDiagnostic& Diagnostic : Result.Diagnostics)
	{
		Problems += Diagnostic.ToString() + TEXT("\n");
	}
	if (!TestFalse(TEXT("parses:\n") + Problems, Result.HasErrors()))
	{
		return false;
	}
	const FVekaDocument& Document = Result.Document;
	TestEqual(TEXT("two timelines"), Document.Timelines.Num(), 2);
	const FVekaDocumentTimeline* Intro = Document.FindTimeline(TEXT("Intro"));
	if (!TestNotNull(TEXT("Intro"), Intro))
	{
		return false;
	}
	TestTrue(TEXT("settings"), Intro->bAutoPlay && Intro->Data.Length == 1.f && !Intro->Data.bLoop);
	TestEqual(TEXT("tracks"), Intro->Data.Tracks.Num(), 2);
	TestEqual(TEXT("curve of a key"), Intro->Data.Tracks[0].Keys[1].Curve, EVekaAnimationCurve::EaseOut);
	TestEqual(TEXT("negative key value"), Intro->Data.Tracks[1].Keys[0].Value, -24.f);
	TestEqual(TEXT("cue"), Intro->Cues.Num(), 1);
	TestTrue(TEXT("Pulse loops at speed 2"), Document.FindTimeline(TEXT("Pulse"))->Data.bLoop && Document.FindTimeline(TEXT("Pulse"))->Data.Speed == 2.f);
	TestTrue(TEXT("Intro.fade reads a timeline"), Document.Expressions.ContainsByPredicate([](const FVekaAstExpr& Expr) { return Expr.Kind == EVekaExprKind::Timeline && Expr.Name == TEXT("Intro"); }));
	const FVekaAstNode* Button = Document.Nodes.FindByPredicate([](const FVekaAstNode& Node) { return Node.Type == TEXT("Button"); });
	TestTrue(TEXT("timeline controls"), Button && Button->Actions.Num() == 4 && Button->Actions[1].Kind == EVekaActionKind::Stop && Button->Actions[2].Kind == EVekaActionKind::Play
		&& Button->Actions[3].Kind == EVekaActionKind::Seek);
	TestTrue(TEXT("valid document"), Document.Validate().IsEmpty());
	TestTrue(TEXT("analyzes clean"), !Veka::Lang::Analyze(Document).ContainsByPredicate([](const FVekaDiagnostic& D) { return true; }));

	// Round trip and canonical form.
	const FString Printed = Veka::Lang::Print(Document, TEXT("Fade"));
	TestEqual(TEXT("printed as written"), Printed, FString(FadeSource));
	const FVekaParseResult Again = Veka::Lang::Parse(Printed);
	FString Difference;
	TestTrue(FString::Printf(TEXT("round trip (%s)"), *Difference), Veka::Lang::StructurallyEqual(Again.Document, Document, &Difference));

	// Diagnostics.
	const FVekaParseResult Track = Veka::Lang::Parse(TEXT("view A {\n    @Timeline T { track a { 0: 0 } }\n    Text(\"x\")\n        .opacity(T.nope)\n}"));
	TestTrue(TEXT("unknown track warned"), Veka::Lang::Analyze(Track.Document).ContainsByPredicate([](const FVekaDiagnostic& D) { return D.Message.ToString().Contains(TEXT("no track 'nope'")); }));
	struct FCase { const TCHAR* Source; const TCHAR* Fragment; };
	const FCase Cases[] = {
		{ TEXT("view A {\n    @Timeline T { track a { 0: 0 .Bouncy } }\n    Text(\"x\")\n}"), TEXT("Unknown curve") },
		{ TEXT("view A {\n    @Timeline T(color: 1) { }\n    Text(\"x\")\n}"), TEXT("Unknown timeline setting") },
		{ TEXT("view A {\n    @State var n: Int = 0\n    @Timeline T { }\n    Button(\"x\") {\n        n += 1\n        T.jump()\n    }\n}"), TEXT("play(), pause()") },
		{ TEXT("view A {\n    @Timeline T { wobble }\n    Text(\"x\")\n}"), TEXT("'track name") },
	};
	for (const FCase& Case : Cases)
	{
		const FVekaParseResult Bad = Veka::Lang::Parse(Case.Source);
		TestTrue(FString::Printf(TEXT("error '%s'"), Case.Fragment), Bad.Diagnostics.ContainsByPredicate([&Case](const FVekaDiagnostic& D) { return D.Message.ToString().Contains(Case.Fragment); }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTimelineRuntimeTest, "VekaUI.View.Timeline", VekaTestModeFlags)
bool FVekaTimelineRuntimeTest::RunTest(const FString& Parameters)
{
	UVekaView* View = MakeView(TEXT("VV_TimelineRuntime"), Veka::Lang::Parse(FadeSource).Document);
	ON_SCOPE_EXIT { Discard(View); };
	const TSharedRef<FVekaViewStore> Store = MakeShared<FVekaViewStore>();
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	Host->SetRoot(View->BuildRoot(Store));
	PaintOnce(Host);
	FVekaRuntime& Runtime = *Host->GetRuntime();
	if (!TestTrue(TEXT("players registered"), Store->Timelines.Contains(TEXT("Intro")) && Store->Timelines.Contains(TEXT("Pulse"))))
	{
		return false;
	}
	const TSharedPtr<FVekaTimelinePlayer> Intro = Store->Timelines[TEXT("Intro")];
	const TSharedPtr<FVekaTimelinePlayer> Pulse = Store->Timelines[TEXT("Pulse")];
	TestTrue(TEXT("autoplay"), Intro->IsPlaying());
	TestFalse(TEXT("no autoplay for Pulse"), Pulse->IsPlaying());

	for (int32 Step = 0; Step < 3; ++Step)
	{
		Runtime.Tick(0.1);
	}
	TestTrue(FString::Printf(TEXT("playhead advanced (%f)"), Intro->GetTime()), FMath::IsNearlyEqual(Intro->GetTime(), 0.3f, 0.02f));
	const float Fade = Intro->Value(TEXT("fade"));
	TestTrue(FString::Printf(TEXT("track between its keys (%f)"), Fade), Fade > 0.f && Fade < 1.f);
	TestFalse(TEXT("cue not reached yet"), Store->Get(TEXT("done")).BoolValue);
	for (int32 Step = 0; Step < 4; ++Step)
	{
		Runtime.Tick(0.1);
	}
	TestTrue(TEXT("cue ran its actions at 0.5 s"), Store->Get(TEXT("done")).BoolValue);
	TestEqual(TEXT("track at its last key"), Intro->Value(TEXT("fade")), 1.f);

	// Actions control timelines.
	TArray<TSharedPtr<FVekaNode>> Replay;
	FindTexts(Runtime.GetRootNode(), TEXT("Replay"), Replay);
	if (!TestEqual(TEXT("replay button"), Replay.Num(), 1))
	{
		return false;
	}
	Tap(Runtime, Replay[0]);
	TestEqual(TEXT("button action ran"), Store->Get(TEXT("replays")).AsInt(), static_cast<int64>(1));
	TestTrue(FString::Printf(TEXT("Intro restarted (%f)"), Intro->GetTime()), Intro->IsPlaying() && Intro->GetTime() < 0.05f);
	TestTrue(FString::Printf(TEXT("Pulse sought (%f)"), Pulse->GetTime()), FMath::IsNearlyEqual(Pulse->GetTime(), 0.25f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTestModeShopTest, "VekaUI.Editor.Test.Shop", VekaTestModeFlags)
bool FVekaTestModeShopTest::RunTest(const FString& Parameters)
{
	// The milestone: the shop tested in Play with a mock model, without PIE.
	UVekaView* Item = MakeView(TEXT("VV_TestItem"), Veka::Samples::MakeShopItemDocument());
	UVekaView* Shop = MakeView(TEXT("VV_TestShop"), Veka::Samples::MakeShopDocument(Item));
	ON_SCOPE_EXIT { Discard(Shop); Discard(Item); };
	Shop->PreviewModelClass = UVekaSampleViewModel::StaticClass();
	const TSharedRef<FVekaViewEditorContext> Context = MakeShared<FVekaViewEditorContext>(Shop);
	const TSharedRef<SVekaViewPreview> Preview = SNew(SVekaViewPreview).View(Shop).bPersistSettings(false);
	Preview->SetContext(Context);
	const TSharedRef<SVekaTestPanel> Test = SNew(SVekaTestPanel, Context, Preview);
	Test->SetPlaying(true);
	TestTrue(TEXT("Play: clicks go to the view"), Test->IsPlaying() && !Preview->GetSettings().bSelectMode);
	PaintOnce(Preview);
	FVekaRuntime& Runtime = *Preview->GetHost()->GetRuntime();

	// Mock data: give the player 30 gold (the Test tab edits the preview model).
	UVekaViewModel* Model = Preview->GetModel();
	if (!TestNotNull(TEXT("mock model"), Model))
	{
		return false;
	}
	TestTrue(TEXT("mock gold"), Model->SetValue(TEXT("Gold"), FVekaValue::Int(30)));
	Runtime.Flush();
	PaintOnce(Preview);
	TArray<TSharedPtr<FVekaNode>> Gold;
	FindTexts(Runtime.GetRootNode(), TEXT("30 gold"), Gold);
	TestEqual(TEXT("the view shows the mock value"), Gold.Num(), 1);

	// Buy the potion (25 gold): the event and the state change are logged.
	TArray<TSharedPtr<FVekaNode>> Buys;
	FindTexts(Runtime.GetRootNode(), TEXT("Buy"), Buys);
	if (!TestEqual(TEXT("three Buy buttons"), Buys.Num(), 3))
	{
		return false;
	}
	Tap(Runtime, Buys[0]);
	TestTrue(TEXT("emit logged: ") + LogText(*Preview), LogHas(*Preview, TEXT("emit Buy(25)")));
	TestTrue(TEXT("state change logged: ") + LogText(*Preview), LogHas(*Preview, TEXT("lastBought = ")));
	TestEqual(TEXT("live state"), Preview->GetStore()->Get(TEXT("lastBought")).AsString(), FString(TEXT("Health potion")));

	// Too expensive with 30 gold: the lantern does nothing.
	const int32 Before = Preview->GetLog().Num();
	Tap(Runtime, Buys[2]);
	TestEqual(TEXT("disabled article logs nothing"), Preview->GetLog().Num(), Before);

	Preview->ClearLog();
	TestTrue(TEXT("log cleared"), Preview->GetLog().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaTimelinePanelTest, "VekaUI.Editor.Timeline.Panel", VekaTestModeFlags)
bool FVekaTimelinePanelTest::RunTest(const FString& Parameters)
{
	// The milestone: an animation made with the Timeline tab.
	UAssetEditorSubsystem* Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	UVekaView* View = MakeView(TEXT("VV_TimelinePanel"), Veka::Samples::MakeStarterDocument());
	ON_SCOPE_EXIT { Discard(View); };
	if (!TestNotNull(TEXT("asset editors"), Editors) || !TestTrue(TEXT("opens"), Editors->OpenEditorForAsset(View)))
	{
		return false;
	}
	ON_SCOPE_EXIT { Editors->CloseAllEditorsForAsset(View); };
	FVekaViewEditorToolkit* Toolkit = static_cast<FVekaViewEditorToolkit*>(Editors->FindEditorForAsset(View, false));
	const TSharedPtr<SVekaTimelinePanel> Panel = Toolkit->GetTimelinePanel();
	if (!TestTrue(TEXT("Timeline and Test tabs"), Panel.IsValid() && Toolkit->GetTestPanel().IsValid()))
	{
		return false;
	}

	const FName Timeline = Panel->AddTimeline();
	TestEqual(TEXT("new timeline"), Timeline, FName(TEXT("Intro")));
	const FName Track = Panel->AddTrack();
	TestEqual(TEXT("new track"), Track, FName(TEXT("value")));
	Panel->AddKey(0, 0.5f);
	const FVekaDocumentTimeline* Data = View->Document.FindTimeline(Timeline);
	TestTrue(TEXT("three keys"), Data && Data->Data.Tracks[0].Keys.Num() == 3);
	Panel->SetKeyValue(0, 1, 0.2f);
	Panel->MoveKey(0, 1, 0.25f);
	Data = View->Document.FindTimeline(Timeline);
	TestTrue(TEXT("key moved and set"), Data && Data->Data.Tracks[0].Keys[1].Time == 0.25f && Data->Data.Tracks[0].Keys[1].Value == 0.2f);

	// Read the track in an argument: the text fades with the timeline.
	const int32 Text = View->Document.Nodes.IndexOfByPredicate([](const FVekaAstNode& Node) { return Node.Type == TEXT("Text"); });
	Toolkit->GetContext()->Edit(INVTEXT("Add opacity"), [Text](FVekaDocument& Doc) -> TArray<int32> { Veka::Editor::Doc::AddModifier(Doc, Text, TEXT("opacity")); return { Text }; });
	const int32 Opacity = View->Document.Nodes[Text].Modifiers.Num() - 1;
	TestTrue(TEXT("track read in an argument"), Toolkit->GetInspector()->ApplyArgumentCode(Text, Opacity, TEXT("value"), TEXT("Intro.value")));
	const FString Code = Toolkit->GetCodeEditor()->GetText();
	TestTrue(TEXT("code shows the timeline: ") + Code, Code.Contains(TEXT("@Timeline Intro(length: 1.0) {")) && Code.Contains(TEXT("track value { 0.0: 0.0, 0.25: 0.2, 1.0: 1.0 }"))
		&& Code.Contains(TEXT(".opacity(Intro.value)")));

	// Scrub and play in the viewport.
	const TSharedPtr<FVekaTimelinePlayer> Player = Panel->GetPlayer();
	if (!TestTrue(TEXT("the viewport plays it"), Player.IsValid()))
	{
		return false;
	}
	Panel->Scrub(0.25f);
	TestTrue(TEXT("scrubbed"), FMath::IsNearlyEqual(Player->GetTime(), 0.25f) && !Player->IsPlaying());
	TestTrue(TEXT("track value at the playhead"), FMath::IsNearlyEqual(Player->Value(TEXT("value")), 0.2f, 0.001f));
	Panel->DeleteKey(0, 1);
	TestEqual(TEXT("key deleted"), View->Document.FindTimeline(Timeline)->Data.Tracks[0].Keys.Num(), 2);
	TestTrue(TEXT("playhead kept across the edit"), Panel->GetPlayer().IsValid() && FMath::IsNearlyEqual(Panel->GetPlayer()->GetTime(), 0.25f));
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo brings the key back"), View->Document.FindTimeline(Timeline)->Data.Tracks[0].Keys.Num(), 3);
	return true;
}

#endif
