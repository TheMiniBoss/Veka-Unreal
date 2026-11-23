// Copyright TheMiniBoss. All Rights Reserved.

#include "TestMode/SVekaTestPanels.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/VekaDocumentEdit.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Anima/VekaTimeline.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VekaTestMode"

namespace
{
	constexpr float RulerHeight = 22.f;
	constexpr float RowHeight = 26.f;
	constexpr float LaneMargin = 10.f;

	TSharedRef<SWidget> SectionTitle(const FText& Title)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 2.f))
			[
				SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("SmallFontBold"))
			]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)];
	}

	TSharedRef<SWidget> SmallButton(const FText& Label, const FText& Tooltip, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.ToolTipText(Tooltip)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label)
			];
	}
}

// --- Test panel ----------------------------------------------------------------------------------------------------

void SVekaTestPanel::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, const TSharedRef<SVekaViewPreview>& InPreview)
{
	Context = InContext;
	Preview = InPreview;
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	ModelDetails = PropertyEditor.CreateDetailView(Args);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SCheckBox)
				.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
				.ToolTipText(LOCTEXT("PlayTooltip", "Play: clicks, hover and focus go to the view (no PIE). Off: clicks select views."))
				.IsChecked_Lambda([this]() { return IsPlaying() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetPlaying(State == ECheckBoxState::Checked); })
				[
					SNew(SBox).Padding(FMargin(10.f, 2.f))
					[
						SNew(STextBlock).Text(LOCTEXT("Play", "Play"))
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f))
			[
				SmallButton(LOCTEXT("ResetState", "Reset state"), LOCTEXT("ResetStateTooltip", "Back to the declared initial states"), [this]() { Preview->ResetState(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SmallButton(LOCTEXT("ClearLog", "Clear log"), FText::GetEmpty(), [this]() { Preview->ClearLog(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, 0.f))
		[
			SectionTitle(LOCTEXT("States", "States (live)"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, 2.f))
		[
			SAssignNew(States, SVerticalBox)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, 0.f))
		[
			SectionTitle(LOCTEXT("Log", "Log (events and state changes)"))
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(6.f, 2.f))
		[
			SAssignNew(LogList, SListView<TSharedPtr<FVekaPreviewLogEntry>>)
			.ListItemsSource(&LogItems)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SVekaTestPanel::MakeLogRow)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, 0.f))
		[
			SectionTitle(LOCTEXT("Mock", "Preview model (mock values, not saved)"))
		]
		+ SVerticalBox::Slot().FillHeight(0.8f).Padding(FMargin(6.f, 2.f))
		[
			ModelDetails.ToSharedRef()
		]
	];
	LogHandle = Preview->OnLogChanged.AddLambda([this]() { RefreshLog(); RebuildStates(); });
	RefreshHandle = Preview->OnRefreshed.AddLambda([this]() { RebuildStates(); ModelDetails->SetObject(Preview->GetModel()); });
	RebuildStates();
	RefreshLog();
	ModelDetails->SetObject(Preview->GetModel());
}

SVekaTestPanel::~SVekaTestPanel()
{
	if (Preview.IsValid())
	{
		Preview->OnLogChanged.Remove(LogHandle);
		Preview->OnRefreshed.Remove(RefreshHandle);
	}
}

void SVekaTestPanel::SetPlaying(bool bPlaying)
{
	Preview->SetSelectMode(!bPlaying);
}

bool SVekaTestPanel::IsPlaying() const
{
	return !Preview->GetSettings().bSelectMode;
}

void SVekaTestPanel::RebuildStates()
{
	States->ClearChildren();
	const TSharedPtr<FVekaViewStore> Store = Preview->GetStore();
	const FVekaDocument* Document = Context->GetDocument();
	if (!Store.IsValid() || !Document)
	{
		return;
	}
	for (const FVekaStateDeclaration& State : Document->States)
	{
		const FName Name = State.Name;
		States->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromName(Name))
			]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SEditableTextBox)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(FText::FromString(Veka::Lang::PrintValue(Store->Get(Name))))
				.ToolTipText(LOCTEXT("SetStateTooltip", "Set the live value (Enter)"))
				.OnTextCommitted_Lambda([this, Name](const FText& Text, ETextCommit::Type Commit)
				{
					FVekaValue Value;
					TArray<FVekaDiagnostic> Diagnostics;
					if (Commit == ETextCommit::OnEnter && Preview->GetStore().IsValid()
						&& Veka::Lang::ParseValue(Text.ToString(), Context->GetTextNamespace(), Value, Diagnostics))
					{
						Preview->GetStore()->Set(Name, Value);
					}
				})
			]
		];
	}
	if (Document->States.IsEmpty())
	{
		States->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(LOCTEXT("NoStates", "No states."))];
	}
}

void SVekaTestPanel::RefreshLog()
{
	LogItems.Reset();
	for (const FVekaPreviewLogEntry& Entry : Preview->GetLog())
	{
		LogItems.Add(MakeShared<FVekaPreviewLogEntry>(Entry));
	}
	LogList->RequestListRefresh();
	if (!LogItems.IsEmpty())
	{
		LogList->RequestScrollIntoView(LogItems.Last());
	}
}

TSharedRef<ITableRow> SVekaTestPanel::MakeLogRow(TSharedPtr<FVekaPreviewLogEntry> Entry, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FVekaPreviewLogEntry>>, Owner)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 8)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(FText::FromString(FString::Printf(TEXT("%7.2f"), Entry->Time)))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(Entry->bEvent ? FLinearColor(0.98f, 0.62f, 0.30f) : FLinearColor(0.75f, 0.78f, 0.82f))
				.Text(FText::FromString(Entry->Text))
			]
		];
}

// --- Timeline lanes ------------------------------------------------------------------------------------------------

/** Ruler, one lane per track (curve + keys), cue markers and the playhead. Click the ruler to scrub. */
class SVekaTimelineLanes : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaTimelineLanes) {}
	SLATE_END_ARGS()

	TFunction<const FVekaDocumentTimeline*()> GetTimeline;
	TFunction<float()> GetTime;
	TFunction<void(float)> OnScrub;
	TFunction<void(int32, int32)> OnSelectKey;
	TFunction<void(int32, int32, float)> OnMoveKey;
	TFunction<void(int32, float)> OnAddKey;
	TFunction<void()> OnDeleteKey;
	int32 SelectedTrack = INDEX_NONE;
	int32 SelectedKey = INDEX_NONE;

	void Construct(const FArguments& InArgs) {}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const FVekaDocumentTimeline* Timeline = GetTimeline ? GetTimeline() : nullptr;
		return FVector2D(400.0, RulerHeight + RowHeight * (Timeline ? FMath::Max(1, Timeline->Data.Tracks.Num()) : 1) + 6.0);
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style,
		bool bParentEnabled) const override
	{
		const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		auto Box = [&](const FVector2D& Position, const FVector2D& BoxSize, const FLinearColor& Color, int32 Layer)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(BoxSize), FSlateLayoutTransform(FVector2f(Position))), White, ESlateDrawEffect::None, Color);
		};
		Box(FVector2D::ZeroVector, Size, FLinearColor(0.015f, 0.015f, 0.018f), LayerId);
		const FVekaDocumentTimeline* Timeline = GetTimeline ? GetTimeline() : nullptr;
		if (!Timeline)
		{
			return LayerId + 1;
		}
		const float Length = Duration(*Timeline);
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 7);

		// Ruler.
		Box(FVector2D::ZeroVector, FVector2D(Size.X, RulerHeight), FLinearColor(0.03f, 0.03f, 0.035f), LayerId);
		const float Step = Length > 5.f ? 1.f : Length > 2.f ? 0.5f : 0.1f;
		for (float T = 0.f; T <= Length + KINDA_SMALL_NUMBER; T += Step)
		{
			const float X = TimeToX(T, Length, Size.X);
			const bool bMajor = FMath::IsNearlyZero(FMath::Fmod(T + 0.0001f, Step * 5.f), 0.001f) || Step >= 0.5f;
			Box(FVector2D(X, bMajor ? 8.0 : 14.0), FVector2D(1.0, bMajor ? 14.0 : 8.0), FLinearColor(0.4f, 0.4f, 0.45f), LayerId + 1);
			if (bMajor)
			{
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(40.f, 10.f), FSlateLayoutTransform(FVector2f(X + 2.f, 0.f))),
					FString::Printf(TEXT("%.1fs"), T), Font, ESlateDrawEffect::None, FLinearColor(0.55f, 0.55f, 0.6f));
			}
		}
		for (const FVekaTimelineCue& Cue : Timeline->Cues)
		{
			Box(FVector2D(TimeToX(Cue.Time, Length, Size.X) - 3.0, RulerHeight - 7.0), FVector2D(7.0, 7.0), FLinearColor(1.f, 0.85f, 0.2f), LayerId + 2);
		}

		// Lanes.
		for (int32 Track = 0; Track < Timeline->Data.Tracks.Num(); ++Track)
		{
			const FVekaTimelineTrack& Data = Timeline->Data.Tracks[Track];
			const double Top = RulerHeight + Track * RowHeight;
			Box(FVector2D(0.0, Top), FVector2D(Size.X, RowHeight - 1.0), Track % 2 ? FLinearColor(0.025f, 0.025f, 0.03f) : FLinearColor(0.035f, 0.035f, 0.04f), LayerId);
			float Min = 0.f;
			float Max = 1.f;
			for (const FVekaTimelineKey& Key : Data.Keys)
			{
				Min = FMath::Min(Min, Key.Value);
				Max = FMath::Max(Max, Key.Value);
			}
			auto ValueToY = [&](float Value) { return Top + RowHeight - 4.0 - (Value - Min) / FMath::Max(Max - Min, KINDA_SMALL_NUMBER) * (RowHeight - 8.0); };
			TArray<FVector2f> Curve;
			for (int32 Sample = 0; Sample <= 60; ++Sample)
			{
				const float T = Length * Sample / 60.f;
				Curve.Add(FVector2f(TimeToX(T, Length, Size.X), ValueToY(Data.Evaluate(T))));
			}
			FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Curve, ESlateDrawEffect::None, FLinearColor(0.35f, 0.82f, 0.78f, 0.7f), true, 1.f);
			for (int32 Key = 0; Key < Data.Keys.Num(); ++Key)
			{
				const bool bSelected = Track == SelectedTrack && Key == SelectedKey;
				const float Time = bSelected && bDraggingKey ? DragTime : Data.Keys[Key].Time;
				const FVector2D Center(TimeToX(Time, Length, Size.X), ValueToY(Data.Keys[Key].Value));
				Box(Center - FVector2D(4.0), FVector2D(8.0), bSelected ? FLinearColor(1.f, 0.6f, 0.1f) : FLinearColor(0.9f, 0.9f, 0.95f), LayerId + 2);
			}
		}

		// Playhead.
		const float Now = GetTime ? GetTime() : 0.f;
		Box(FVector2D(TimeToX(Now, Length, Size.X), 0.0), FVector2D(1.5, Size.Y), FLinearColor(1.f, 0.3f, 0.3f), LayerId + 3);
		return LayerId + 4;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVekaDocumentTimeline* Timeline = GetTimeline ? GetTimeline() : nullptr;
		if (!Timeline || Event.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}
		const FVector2D Local = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		const float Width = Geometry.GetLocalSize().X;
		const float Length = Duration(*Timeline);
		if (Local.Y < RulerHeight)
		{
			bScrubbing = true;
			OnScrub(XToTime(Local.X, Length, Width));
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
		}
		const int32 Track = FMath::FloorToInt((Local.Y - RulerHeight) / RowHeight);
		if (Timeline->Data.Tracks.IsValidIndex(Track))
		{
			const TArray<FVekaTimelineKey>& Keys = Timeline->Data.Tracks[Track].Keys;
			for (int32 Key = 0; Key < Keys.Num(); ++Key)
			{
				if (FMath::Abs(TimeToX(Keys[Key].Time, Length, Width) - Local.X) <= 6.f)
				{
					SelectedTrack = Track;
					SelectedKey = Key;
					OnSelectKey(Track, Key);
					bDraggingKey = true;
					DragTime = Keys[Key].Time;
					return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
				}
			}
		}
		return FReply::Handled().SetUserFocus(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVekaDocumentTimeline* Timeline = GetTimeline ? GetTimeline() : nullptr;
		if (!Timeline || (!bScrubbing && !bDraggingKey))
		{
			return FReply::Unhandled();
		}
		const float Time = XToTime(FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())).X, Duration(*Timeline), Geometry.GetLocalSize().X);
		if (bScrubbing)
		{
			OnScrub(Time);
		}
		else
		{
			DragTime = Time;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (bDraggingKey)
		{
			bDraggingKey = false;
			OnMoveKey(SelectedTrack, SelectedKey, DragTime);
		}
		bScrubbing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVekaDocumentTimeline* Timeline = GetTimeline ? GetTimeline() : nullptr;
		const FVector2D Local = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		const int32 Track = FMath::FloorToInt((Local.Y - RulerHeight) / RowHeight);
		if (Timeline && Local.Y >= RulerHeight && Timeline->Data.Tracks.IsValidIndex(Track))
		{
			OnAddKey(Track, XToTime(Local.X, Duration(*Timeline), Geometry.GetLocalSize().X));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
	{
		if (Event.GetKey() == EKeys::Delete && SelectedKey != INDEX_NONE)
		{
			OnDeleteKey();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	static float Duration(const FVekaDocumentTimeline& Timeline)
	{
		return FMath::Max(Timeline.Data.GetLength(), 0.5f);
	}

private:
	static float TimeToX(float Time, float Length, float Width)
	{
		return LaneMargin + Time / Length * FMath::Max(Width - 2.f * LaneMargin, 1.f);
	}

	static float XToTime(float X, float Length, float Width)
	{
		// Snapped to 0.01 s (keys print as 0.25, not 0.2503261).
		const float Time = FMath::Clamp((X - LaneMargin) / FMath::Max(Width - 2.f * LaneMargin, 1.f) * Length, 0.f, Length);
		return FMath::RoundToFloat(Time * 100.f) / 100.f;
	}

	bool bScrubbing = false;
	bool bDraggingKey = false;
	float DragTime = 0.f;
};

// --- Timeline panel ------------------------------------------------------------------------------------------------

void SVekaTimelinePanel::Construct(const FArguments& InArgs, const TSharedRef<FVekaViewEditorContext>& InContext, const TSharedRef<SVekaViewPreview>& InPreview)
{
	Context = InContext;
	Preview = InPreview;
	SAssignNew(Lanes, SVekaTimelineLanes);
	Lanes->GetTimeline = [this]() { return GetTimeline(); };
	Lanes->GetTime = [this]()
	{
		const TSharedPtr<FVekaTimelinePlayer> Player = GetPlayer();
		return Player.IsValid() ? Player->GetTime() : 0.f;
	};
	Lanes->OnScrub = [this](float Time) { Scrub(Time); };
	Lanes->OnSelectKey = [this](int32 Track, int32 Key)
	{
		SelectedTrack = Track;
		SelectedKey = Key;
		Rebuild();
	};
	Lanes->OnMoveKey = [this](int32 Track, int32 Key, float Time) { MoveKey(Track, Key, Time); };
	Lanes->OnAddKey = [this](int32 Track, float Time) { AddKey(Track, Time); };
	Lanes->OnDeleteKey = [this]() { DeleteKey(SelectedTrack, SelectedKey); };

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f)
		[
			SAssignNew(Content, SVerticalBox)
			.IsEnabled_Lambda([this]() { return !Context->IsReadOnly(); })
		]
	];
	// The playhead moves while a timeline plays.
	RegisterActiveTimer(1.f / 30.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		Lanes->Invalidate(EInvalidateWidgetReason::Paint);
		return EActiveTimerReturnType::Continue;
	}));
	DocumentHandle = Context->OnDocumentChanged.AddSP(this, &SVekaTimelinePanel::Rebuild);
	const FVekaDocument* Document = Context->GetDocument();
	if (Document && !Document->Timelines.IsEmpty())
	{
		Selected = Document->Timelines[0].Name;
	}
	Rebuild();
}

SVekaTimelinePanel::~SVekaTimelinePanel()
{
	if (Context.IsValid())
	{
		Context->OnDocumentChanged.Remove(DocumentHandle);
	}
}

int32 SVekaTimelinePanel::TimelineIndex() const
{
	const FVekaDocument* Document = Context->GetDocument();
	return Document ? Document->Timelines.IndexOfByPredicate([this](const FVekaDocumentTimeline& Timeline) { return Timeline.Name == Selected; }) : INDEX_NONE;
}

const FVekaDocumentTimeline* SVekaTimelinePanel::GetTimeline() const
{
	const FVekaDocument* Document = Context->GetDocument();
	const int32 Index = TimelineIndex();
	return Document && Document->Timelines.IsValidIndex(Index) ? &Document->Timelines[Index] : nullptr;
}

TSharedPtr<FVekaTimelinePlayer> SVekaTimelinePanel::GetPlayer() const
{
	const TSharedPtr<FVekaViewStore> Store = Preview->GetStore();
	const TSharedPtr<FVekaTimelinePlayer>* Player = Store.IsValid() ? Store->Timelines.Find(Selected) : nullptr;
	return Player ? *Player : nullptr;
}

void SVekaTimelinePanel::Scrub(float Time)
{
	if (const TSharedPtr<FVekaTimelinePlayer> Player = GetPlayer())
	{
		Player->Pause();
		Player->Seek(Time);
	}
	Lanes->Invalidate(EInvalidateWidgetReason::Paint);
}

bool SVekaTimelinePanel::EditTimeline(const FText& Description, TFunction<void(FVekaDocumentTimeline&)> Change)
{
	const int32 Index = TimelineIndex();
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const float Time = Lanes->GetTime();
	const bool bDone = Context->Edit(Description, [Index, &Change](FVekaDocument& Document) -> TArray<int32>
	{
		Change(Document.Timelines[Index]);
		return {};
	});
	// The view was rebuilt: keep the playhead where it was.
	if (const TSharedPtr<FVekaTimelinePlayer> Player = GetPlayer())
	{
		if (!Player->IsPlaying())
		{
			Player->Seek(Time);
		}
	}
	return bDone;
}

FName SVekaTimelinePanel::AddTimeline()
{
	FName Name;
	Context->Edit(LOCTEXT("AddTimeline", "Add timeline"), [&Name](FVekaDocument& Document) -> TArray<int32>
	{
		Name = Veka::Editor::Doc::UniqueName(Document, TEXT("Intro"));
		FVekaDocumentTimeline& Timeline = Document.Timelines.AddDefaulted_GetRef();
		Timeline.Name = Name;
		Timeline.Data.Length = 1.f;
		return {};
	});
	SelectTimeline(Name);
	return Name;
}

void SVekaTimelinePanel::SelectTimeline(FName Name)
{
	Selected = Name;
	SelectedTrack = INDEX_NONE;
	SelectedKey = INDEX_NONE;
	Rebuild();
}

FName SVekaTimelinePanel::AddTrack()
{
	const FVekaDocumentTimeline* Timeline = GetTimeline();
	if (!Timeline)
	{
		return NAME_None;
	}
	FName Name(TEXT("value"));
	for (int32 Suffix = 2; Timeline->Data.Tracks.ContainsByPredicate([&Name](const FVekaTimelineTrack& Track) { return Track.Name == Name; }); ++Suffix)
	{
		Name = FName(*FString::Printf(TEXT("value%d"), Suffix));
	}
	EditTimeline(LOCTEXT("AddTrack", "Add track"), [Name](FVekaDocumentTimeline& Edited)
	{
		FVekaTimelineTrack& Track = Edited.Data.Tracks.AddDefaulted_GetRef();
		Track.Name = Name;
		Track.Keys = { FVekaTimelineKey(0.f, 0.f), FVekaTimelineKey(FMath::Max(Edited.Data.GetLength(), 1.f), 1.f) };
	});
	return Name;
}

void SVekaTimelinePanel::AddKey(int32 Track, float Time)
{
	EditTimeline(LOCTEXT("AddKey", "Add keyframe"), [this, Track, Time](FVekaDocumentTimeline& Edited)
	{
		if (!Edited.Data.Tracks.IsValidIndex(Track))
		{
			return;
		}
		TArray<FVekaTimelineKey>& Keys = Edited.Data.Tracks[Track].Keys;
		const float Value = Edited.Data.Tracks[Track].Evaluate(Time);
		Keys.Add(FVekaTimelineKey(Time, Value));
		Keys.StableSort([](const FVekaTimelineKey& A, const FVekaTimelineKey& B) { return A.Time < B.Time; });
		SelectedTrack = Track;
		SelectedKey = Keys.IndexOfByPredicate([Time](const FVekaTimelineKey& Key) { return Key.Time == Time; });
	});
}

void SVekaTimelinePanel::MoveKey(int32 Track, int32 Key, float Time)
{
	EditTimeline(LOCTEXT("MoveKey", "Move keyframe"), [this, Track, Key, Time](FVekaDocumentTimeline& Edited)
	{
		if (!Edited.Data.Tracks.IsValidIndex(Track) || !Edited.Data.Tracks[Track].Keys.IsValidIndex(Key))
		{
			return;
		}
		TArray<FVekaTimelineKey>& Keys = Edited.Data.Tracks[Track].Keys;
		const FVekaTimelineKey Moved(Time, Keys[Key].Value, Keys[Key].Curve);
		Keys.RemoveAt(Key);
		Keys.Add(Moved);
		Keys.StableSort([](const FVekaTimelineKey& A, const FVekaTimelineKey& B) { return A.Time < B.Time; });
		SelectedKey = Keys.IndexOfByPredicate([Time](const FVekaTimelineKey& Item) { return Item.Time == Time; });
	});
}

void SVekaTimelinePanel::SetKeyValue(int32 Track, int32 Key, float Value)
{
	EditTimeline(LOCTEXT("SetKeyValue", "Change keyframe"), [Track, Key, Value](FVekaDocumentTimeline& Edited)
	{
		if (Edited.Data.Tracks.IsValidIndex(Track) && Edited.Data.Tracks[Track].Keys.IsValidIndex(Key))
		{
			Edited.Data.Tracks[Track].Keys[Key].Value = Value;
		}
	});
}

void SVekaTimelinePanel::DeleteKey(int32 Track, int32 Key)
{
	EditTimeline(LOCTEXT("DeleteKey", "Delete keyframe"), [Track, Key](FVekaDocumentTimeline& Edited)
	{
		if (Edited.Data.Tracks.IsValidIndex(Track) && Edited.Data.Tracks[Track].Keys.IsValidIndex(Key))
		{
			Edited.Data.Tracks[Track].Keys.RemoveAt(Key);
		}
	});
	SelectedKey = INDEX_NONE;
	Rebuild();
}

TSharedRef<SWidget> SVekaTimelinePanel::MakeHeader()
{
	const FVekaDocument* Document = Context->GetDocument();
	const FVekaDocumentTimeline* Timeline = GetTimeline();
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.OnGetMenuContent_Lambda([this, Document]()
			{
				FMenuBuilder Menu(true, nullptr);
				for (const FVekaDocumentTimeline& Item : Document->Timelines)
				{
					const FName Name = Item.Name;
					Menu.AddMenuEntry(FText::FromName(Name), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Name]() { SelectTimeline(Name); })));
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock).Text(Timeline ? FText::FromName(Timeline->Name) : LOCTEXT("NoTimeline", "(no timeline)"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f))
		[
			SmallButton(LOCTEXT("AddTimelineButton", "+ Timeline"), LOCTEXT("AddTimelineTooltip", "New timeline (@Timeline)"), [this]() { AddTimeline(); })
		];
	if (Timeline)
	{
		const FName Name = Timeline->Name;
		Row->AddSlot().AutoWidth().Padding(FMargin(4.f, 0.f))
		[
			SNew(SBox).WidthOverride(110.f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromName(Name))
				.ToolTipText(LOCTEXT("RenameTimeline", "Rename (every use follows)"))
				.OnTextCommitted_Lambda([this, Name](const FText& Text, ETextCommit::Type)
				{
					const FName New(*Text.ToString().TrimStartAndEnd());
					if (New != Name && Context->Edit(LOCTEXT("RenameTimelineStep", "Rename timeline"), [Name, New](FVekaDocument& Doc) -> TArray<int32>
					{
						Veka::Editor::Doc::RenameDeclaration(Doc, Name, New);
						return {};
					}))
					{
						SelectTimeline(New);
					}
				})
			]
		];
		Row->AddSlot().AutoWidth()
		[
			SmallButton(LOCTEXT("RemoveTimeline", "x"), LOCTEXT("RemoveTimelineTooltip", "Remove the timeline"), [this, Name]()
			{
				Context->Edit(LOCTEXT("RemoveTimelineStep", "Remove timeline"), [Name](FVekaDocument& Doc) -> TArray<int32>
				{
					Veka::Editor::Doc::RemoveDeclaration(Doc, Name);
					return {};
				});
				SelectTimeline(NAME_None);
			})
		];
		auto Number = [this](const FText& Label, float Value, float Min, TFunction<void(FVekaDocumentTimeline&, float)> Set) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 3.f, 0.f))[SNew(STextBlock).Text(Label)]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(56.f)
					[
						SNew(SNumericEntryBox<float>)
						.MinValue(Min)
						.Value(TOptional<float>(Value))
						.OnValueCommitted_Lambda([this, Set](float NewValue, ETextCommit::Type)
						{
							EditTimeline(LOCTEXT("SetSetting", "Change timeline"), [Set, NewValue](FVekaDocumentTimeline& Edited) { Set(Edited, NewValue); });
						})
					]
				];
		};
		auto Check = [this](const FText& Label, bool bValue, TFunction<void(FVekaDocumentTimeline&, bool)> Set) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 3.f, 0.f))[SNew(STextBlock).Text(Label)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged_Lambda([this, Set](ECheckBoxState State)
					{
						EditTimeline(LOCTEXT("SetFlag", "Change timeline"), [Set, State](FVekaDocumentTimeline& Edited) { Set(Edited, State == ECheckBoxState::Checked); });
					})
				];
		};
		Row->AddSlot().AutoWidth()[Number(LOCTEXT("Length", "length"), Timeline->Data.Length, 0.f, [](FVekaDocumentTimeline& T, float V) { T.Data.Length = V; })];
		Row->AddSlot().AutoWidth()[Number(LOCTEXT("Speed", "speed"), Timeline->Data.Speed, 0.01f, [](FVekaDocumentTimeline& T, float V) { T.Data.Speed = V; })];
		Row->AddSlot().AutoWidth()[Check(LOCTEXT("Loop", "loop"), Timeline->Data.bLoop, [](FVekaDocumentTimeline& T, bool B) { T.Data.bLoop = B; })];
		Row->AddSlot().AutoWidth()[Check(LOCTEXT("AutoPlay", "autoplay"), Timeline->bAutoPlay, [](FVekaDocumentTimeline& T, bool B) { T.bAutoPlay = B; })];
	}
	return Row;
}

TSharedRef<SWidget> SVekaTimelinePanel::MakeTransport()
{
	auto Control = [this](const FText& Label, const FText& Tooltip, TFunction<void(FVekaTimelinePlayer&)> Do)
	{
		return SmallButton(Label, Tooltip, [this, Do]()
		{
			if (const TSharedPtr<FVekaTimelinePlayer> Player = GetPlayer())
			{
				Do(*Player);
			}
		});
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[Control(LOCTEXT("PlayTimeline", "Play"), LOCTEXT("PlayTimelineTooltip", "Play in the viewport"), [](FVekaTimelinePlayer& P)
		{
			if (P.GetTime() >= P.Data.GetLength() && !P.IsReversed())
			{
				P.Seek(0.f);
			}
			P.Play();
		})]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f, 0.f))[Control(LOCTEXT("PauseTimeline", "Pause"), FText::GetEmpty(), [](FVekaTimelinePlayer& P) { P.Pause(); })]
		+ SHorizontalBox::Slot().AutoWidth()[Control(LOCTEXT("StopTimeline", "Stop"), LOCTEXT("StopTimelineTooltip", "Pause and go back to the start"), [](FVekaTimelinePlayer& P) { P.Stop(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f, 0.f))[Control(LOCTEXT("ReverseTimeline", "Reverse"), LOCTEXT("ReverseTimelineTooltip", "Play the other way"), [](FVekaTimelinePlayer& P) { P.Reverse(); })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.Text_Lambda([this]()
			{
				const TSharedPtr<FVekaTimelinePlayer> Player = GetPlayer();
				const FVekaDocumentTimeline* Timeline = GetTimeline();
				return FText::FromString(FString::Printf(TEXT("%.2f s / %.2f s"), Player.IsValid() ? Player->GetTime() : 0.f, Timeline ? Timeline->Data.GetLength() : 0.f));
			})
		];
}

TSharedRef<SWidget> SVekaTimelinePanel::MakeKeyEditor()
{
	const FVekaDocumentTimeline* Timeline = GetTimeline();
	if (!Timeline || !Timeline->Data.Tracks.IsValidIndex(SelectedTrack) || !Timeline->Data.Tracks[SelectedTrack].Keys.IsValidIndex(SelectedKey))
	{
		return SNew(STextBlock).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("KeyHint", "Double-click a lane to add a keyframe; click a keyframe to edit it, drag it to move it, Delete to remove it. Click the ruler to scrub."));
	}
	const FVekaTimelineKey& Key = Timeline->Data.Tracks[SelectedTrack].Keys[SelectedKey];
	const int32 Track = SelectedTrack;
	const int32 Index = SelectedKey;
	const UEnum* Curves = StaticEnum<EVekaAnimationCurve>();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("KeyTime", "time"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f, 0.f))
		[
			SNew(SBox).WidthOverride(60.f)
			[
				SNew(SNumericEntryBox<float>).MinValue(0.f).Value(TOptional<float>(Key.Time))
				.OnValueCommitted_Lambda([this, Track, Index](float Value, ETextCommit::Type) { MoveKey(Track, Index, Value); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))[SNew(STextBlock).Text(LOCTEXT("KeyValue", "value"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f, 0.f))
		[
			SNew(SBox).WidthOverride(60.f)
			[
				SNew(SNumericEntryBox<float>).Value(TOptional<float>(Key.Value))
				.OnValueCommitted_Lambda([this, Track, Index](float Value, ETextCommit::Type) { SetKeyValue(Track, Index, Value); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 3.f, 0.f))[SNew(STextBlock).Text(LOCTEXT("KeyCurve", "curve"))]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SComboButton)
			.OnGetMenuContent_Lambda([this, Track, Index, Curves]()
			{
				FMenuBuilder Menu(true, nullptr);
				for (int32 Item = 0; Item < Curves->NumEnums() - 1; ++Item)
				{
					const EVekaAnimationCurve Curve = static_cast<EVekaAnimationCurve>(Curves->GetValueByIndex(Item));
					Menu.AddMenuEntry(FText::FromString(Curves->GetNameStringByIndex(Item)), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Track, Index, Curve]()
					{
						EditTimeline(LOCTEXT("SetCurve", "Change keyframe"), [Track, Index, Curve](FVekaDocumentTimeline& Edited)
						{
							if (Edited.Data.Tracks.IsValidIndex(Track) && Edited.Data.Tracks[Track].Keys.IsValidIndex(Index))
							{
								Edited.Data.Tracks[Track].Keys[Index].Curve = Curve;
							}
						});
					})));
				}
				return Menu.MakeWidget();
			})
			.ButtonContent()
			[
				SNew(STextBlock).Text(FText::FromString(Curves->GetNameStringByValue(static_cast<int64>(Key.Curve))))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f))
		[
			SmallButton(LOCTEXT("DeleteKeyButton", "Delete key"), FText::GetEmpty(), [this, Track, Index]() { DeleteKey(Track, Index); })
		];
}

void SVekaTimelinePanel::Rebuild()
{
	Content->ClearChildren();
	const FVekaDocumentTimeline* Timeline = GetTimeline();
	if (!Timeline && Context->GetDocument() && !Context->GetDocument()->Timelines.IsEmpty())
	{
		Selected = Context->GetDocument()->Timelines[0].Name;
		Timeline = GetTimeline();
	}
	Lanes->SelectedTrack = SelectedTrack;
	Lanes->SelectedKey = SelectedKey;
	Content->AddSlot().AutoHeight()[MakeHeader()];
	if (!Timeline)
	{
		Content->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f))
		[
			SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("NoTimelines", "No timeline. Add one, add tracks and keyframes, then read a track in any argument: .opacity(Intro.fade)."))
		];
		return;
	}
	Content->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f))[MakeTransport()];

	// Track names next to their lanes.
	TSharedRef<SVerticalBox> Names = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(RulerHeight)];
	const FName TimelineName = Timeline->Name;
	for (int32 Track = 0; Track < Timeline->Data.Tracks.Num(); ++Track)
	{
		const FName TrackName = Timeline->Data.Tracks[Track].Name;
		Names->AddSlot().AutoHeight()
		[
			SNew(SBox).HeightOverride(RowHeight).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SEditableTextBox)
					.Text(FText::FromName(TrackName))
					.ToolTipText(FText::Format(LOCTEXT("TrackTooltip", "Read it as {0}.{1} (renaming updates the uses)"), FText::FromName(TimelineName), FText::FromName(TrackName)))
					.OnTextCommitted_Lambda([this, Track, TrackName, TimelineName](const FText& Text, ETextCommit::Type)
					{
						const FName New(*Text.ToString().TrimStartAndEnd());
						if (New.IsNone() || New == TrackName)
						{
							return;
						}
						const int32 Index = TimelineIndex();
						Context->Edit(LOCTEXT("RenameTrack", "Rename track"), [Index, Track, TrackName, TimelineName, New](FVekaDocument& Doc) -> TArray<int32>
						{
							Doc.Timelines[Index].Data.Tracks[Track].Name = New;
							for (FVekaAstExpr& Expr : Doc.Expressions)
							{
								const bool bOfTimeline = Expr.Kind == EVekaExprKind::Member && Expr.Name == TrackName && Expr.Operands.Num() == 1
									&& Doc.Expressions.IsValidIndex(Expr.Operands[0]) && Doc.Expressions[Expr.Operands[0]].Kind == EVekaExprKind::Timeline
									&& Doc.Expressions[Expr.Operands[0]].Name == TimelineName;
								if (bOfTimeline)
								{
									Expr.Name = New;
								}
							}
							return {};
						});
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").Text(LOCTEXT("RemoveTrack", "x"))
					.OnClicked_Lambda([this, Track]()
					{
						EditTimeline(LOCTEXT("RemoveTrackStep", "Remove track"), [Track](FVekaDocumentTimeline& Edited) { Edited.Data.Tracks.RemoveAt(Track); });
						SelectedTrack = INDEX_NONE;
						SelectedKey = INDEX_NONE;
						Rebuild();
						return FReply::Handled();
					})
				]
			]
		];
	}
	Names->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
	[
		SmallButton(LOCTEXT("AddTrackButton", "+ Track"), LOCTEXT("AddTrackTooltip", "New track (read it in an argument as Timeline.track)"), [this]() { AddTrack(); })
	];
	Content->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(130.f)[Names]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(4.f, 0.f, 0.f, 0.f))
		[
			Lanes.ToSharedRef()
		]
	];
	Content->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f))[MakeKeyEditor()];
}

#undef LOCTEXT_NAMESPACE
