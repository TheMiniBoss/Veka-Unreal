// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Anima/VekaAnimationSpec.h"
#include "VekaTimeline.generated.h"

class FVekaNode;

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTimelineKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	float Value = 0.f;

	/** Easing of the segment that starts at this key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	EVekaAnimationCurve Curve = EVekaAnimationCurve::EaseInOut;

	FVekaTimelineKey() = default;
	FVekaTimelineKey(float InTime, float InValue, EVekaAnimationCurve InCurve = EVekaAnimationCurve::EaseInOut) : Time(InTime), Value(InValue), Curve(InCurve) {}
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	FName Name;

	/** Sorted by time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	TArray<FVekaTimelineKey> Keys;

	float Evaluate(float Time) const;
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTimelineEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	FName Name;
};

/**
 * Keyframed tracks + named events. A view reads track values in its Body (Ctx.Timeline), so a timeline can
 * drive any property, or flip a state at a given time (which then triggers implicit animations).
 * Tracks targeting `NodeId.PropertyPath` directly come with the Veka Editor (Phase 3).
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTimelineData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	TArray<FVekaTimelineTrack> Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	TArray<FVekaTimelineEvent> Events;

	/** Seconds. 0 = up to the last key or event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline", meta = (ClampMin = "0"))
	float Length = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Timeline", meta = (ClampMin = "0.01"))
	float Speed = 1.f;

	float GetLength() const;
	/** Value of a track at Time (0 if the track does not exist). */
	float Evaluate(FName Track, float Time) const;

	FVekaTimelineData& AddTrack(FName Name, TArray<FVekaTimelineKey> Keys);
	FVekaTimelineData& AddEvent(float Time, FName Name);
};

/** Reusable timeline asset. */
UCLASS(BlueprintType)
class VEKAUI_API UVekaTimeline : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Timeline")
	FVekaTimelineData Data;
};

/** Playback state of a timeline, owned by a view node (kept across re-evaluations). */
class VEKAUI_API FVekaTimelinePlayer : public TSharedFromThis<FVekaTimelinePlayer>
{
public:
	FVekaTimelineData Data;
	TWeakPtr<FVekaNode> Owner;
	TFunction<void(FName)> OnEvent;

	void Play();
	void Pause();
	/** Pauses and goes back to the start. */
	void Stop();
	void Seek(float InTime);
	/** Plays in the other direction from the current time. */
	void Reverse();

	bool IsPlaying() const { return bPlaying; }
	float GetTime() const { return Time; }
	bool IsReversed() const { return Direction < 0.f; }
	float Value(FName Track) const { return Data.Evaluate(Track, Time); }

	/** Called by the runtime while playing. Fires crossed events and re-evaluates the owner view. */
	void Advance(double DeltaSeconds);

private:
	void FireEvents(float From, float To);
	void NotifyOwner();

	float Time = 0.f;
	float Direction = 1.f;
	bool bPlaying = false;
};

/** What Ctx.Timeline returns: control and values of a timeline stored in the view. */
class VEKAUI_API FVekaTimelineHandle
{
public:
	FVekaTimelineHandle() = default;
	explicit FVekaTimelineHandle(TSharedPtr<FVekaTimelinePlayer> InPlayer) : Player(MoveTemp(InPlayer)) {}

	void Play() const { if (Player) { Player->Play(); } }
	void Pause() const { if (Player) { Player->Pause(); } }
	void Stop() const { if (Player) { Player->Stop(); } }
	void Seek(float Time) const { if (Player) { Player->Seek(Time); } }
	void Reverse() const { if (Player) { Player->Reverse(); } }
	bool IsPlaying() const { return Player && Player->IsPlaying(); }
	float GetTime() const { return Player ? Player->GetTime() : 0.f; }
	float Value(FName Track) const { return Player ? Player->Value(Track) : 0.f; }
	/** Called when playback crosses an event (set it in Body; the latest callback is kept). */
	void OnEvent(TFunction<void(FName)> Callback) const { if (Player) { Player->OnEvent = MoveTemp(Callback); } }

	TSharedPtr<FVekaTimelinePlayer> GetPlayer() const { return Player; }

private:
	TSharedPtr<FVekaTimelinePlayer> Player;
};
