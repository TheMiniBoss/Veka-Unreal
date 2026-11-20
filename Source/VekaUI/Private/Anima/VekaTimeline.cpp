// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaTimeline.h"
#include "Anima/VekaEasing.h"
#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaView.h"

// --- Data --------------------------------------------------------------------------------------------------

float FVekaTimelineTrack::Evaluate(float Time) const
{
	if (Keys.IsEmpty())
	{
		return 0.f;
	}
	if (Time <= Keys[0].Time)
	{
		return Keys[0].Value;
	}
	for (int32 Index = 1; Index < Keys.Num(); ++Index)
	{
		const FVekaTimelineKey& A = Keys[Index - 1];
		const FVekaTimelineKey& B = Keys[Index];
		if (Time <= B.Time)
		{
			const float Span = B.Time - A.Time;
			const double T = Span > UE_SMALL_NUMBER ? (Time - A.Time) / Span : 1.0;
			const double Eased = A.Curve == EVekaAnimationCurve::Spring ? T : Veka::Easing::Evaluate(A.Curve, T);
			return FMath::Lerp(A.Value, B.Value, static_cast<float>(Eased));
		}
	}
	return Keys.Last().Value;
}

float FVekaTimelineData::GetLength() const
{
	if (Length > 0.f)
	{
		return Length;
	}
	float Max = 0.f;
	for (const FVekaTimelineTrack& Track : Tracks)
	{
		if (Track.Keys.Num() > 0)
		{
			Max = FMath::Max(Max, Track.Keys.Last().Time);
		}
	}
	for (const FVekaTimelineEvent& Event : Events)
	{
		Max = FMath::Max(Max, Event.Time);
	}
	return Max;
}

float FVekaTimelineData::Evaluate(FName Track, float Time) const
{
	for (const FVekaTimelineTrack& Candidate : Tracks)
	{
		if (Candidate.Name == Track)
		{
			return Candidate.Evaluate(Time);
		}
	}
	return 0.f;
}

FVekaTimelineData& FVekaTimelineData::AddTrack(FName Name, TArray<FVekaTimelineKey> Keys)
{
	Keys.StableSort([](const FVekaTimelineKey& A, const FVekaTimelineKey& B) { return A.Time < B.Time; });
	FVekaTimelineTrack& Track = Tracks.AddDefaulted_GetRef();
	Track.Name = Name;
	Track.Keys = MoveTemp(Keys);
	return *this;
}

FVekaTimelineData& FVekaTimelineData::AddEvent(float Time, FName Name)
{
	FVekaTimelineEvent& Event = Events.AddDefaulted_GetRef();
	Event.Time = Time;
	Event.Name = Name;
	return *this;
}

// --- Player ------------------------------------------------------------------------------------------------

void FVekaTimelinePlayer::NotifyOwner()
{
	if (TSharedPtr<FVekaNode> Node = Owner.Pin())
	{
		Node->MarkDirty();
	}
}

void FVekaTimelinePlayer::Play()
{
	const float Length = Data.GetLength();
	if (!bPlaying)
	{
		// Restart from the end that playback would stop at.
		if (Direction > 0.f && Time >= Length)
		{
			Time = 0.f;
		}
		else if (Direction < 0.f && Time <= 0.f)
		{
			Time = Length;
		}
	}
	bPlaying = true;
	if (TSharedPtr<FVekaNode> Node = Owner.Pin())
	{
		Node->GetRuntime().RegisterTimeline(AsShared());
	}
	NotifyOwner();
}

void FVekaTimelinePlayer::Pause()
{
	bPlaying = false;
	NotifyOwner();
}

void FVekaTimelinePlayer::Stop()
{
	bPlaying = false;
	Time = Direction > 0.f ? 0.f : Data.GetLength();
	NotifyOwner();
}

void FVekaTimelinePlayer::Seek(float InTime)
{
	Time = FMath::Clamp(InTime, 0.f, Data.GetLength());
	NotifyOwner();
}

void FVekaTimelinePlayer::Reverse()
{
	Direction = -Direction;
	Play();
}

void FVekaTimelinePlayer::FireEvents(float From, float To)
{
	if (!OnEvent)
	{
		return;
	}
	const float Low = FMath::Min(From, To);
	const float High = FMath::Max(From, To);
	// Copy: the callback may change the timeline.
	const TArray<FVekaTimelineEvent> Events = Data.Events;
	const TFunction<void(FName)> Callback = OnEvent;
	for (const FVekaTimelineEvent& Event : Events)
	{
		// Half-open interval in the playback direction so an event on a boundary fires once.
		const bool bCrossed = To >= From ? (Event.Time > Low && Event.Time <= High) : (Event.Time >= Low && Event.Time < High);
		if (bCrossed)
		{
			Callback(Event.Name);
		}
	}
}

void FVekaTimelinePlayer::Advance(double DeltaSeconds)
{
	if (!bPlaying)
	{
		return;
	}
	const float Length = Data.GetLength();
	if (Length <= 0.f)
	{
		bPlaying = false;
		return;
	}

	const float Previous = Time;
	float Next = Time + static_cast<float>(DeltaSeconds) * FMath::Max(Data.Speed, 0.01f) * Direction;

	if (Next > Length || Next < 0.f)
	{
		if (Data.bLoop)
		{
			const float Boundary = Direction > 0.f ? Length : 0.f;
			FireEvents(Previous, Boundary);
			const float Wrapped = FMath::Fmod(Next + Length, Length);
			// An event at the start (or at the end, backward) fires again on each loop.
			FireEvents(Direction > 0.f ? -UE_SMALL_NUMBER : Length + UE_SMALL_NUMBER, Wrapped);
			Time = Wrapped;
		}
		else
		{
			Time = FMath::Clamp(Next, 0.f, Length);
			FireEvents(Previous, Time);
			bPlaying = false;
		}
	}
	else
	{
		Time = Next;
		FireEvents(Previous, Time);
	}
	NotifyOwner();
}

// --- Context -----------------------------------------------------------------------------------------------

FVekaTimelineHandle FVekaContext::Timeline(FName Name, const FVekaTimelineData& Data)
{
	TSharedPtr<FVekaStateBoxBase>& Slot = FindOrAddStateSlot(Name);
	using FPlayerBox = TVekaStateBox<TSharedPtr<FVekaTimelinePlayer>>;
	if (!Slot.IsValid() || Slot->TypeSize != sizeof(TSharedPtr<FVekaTimelinePlayer>))
	{
		TSharedRef<FPlayerBox> Box = MakeShared<FPlayerBox>();
		Box->TypeSize = sizeof(TSharedPtr<FVekaTimelinePlayer>);
		Box->Owner = GetNodeWeak();
		Box->Value = MakeShared<FVekaTimelinePlayer>();
		Box->Value->Owner = GetNodeWeak();
		Slot = Box;
	}
	TSharedPtr<FVekaTimelinePlayer> Player = StaticCastSharedPtr<FPlayerBox>(Slot)->Value;
	Player->Data = Data;
	return FVekaTimelineHandle(Player);
}
