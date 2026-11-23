// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaPaint.h"
#include "Controls/VekaControlStyles.h"
#include "VekaControlPrimitives.generated.h"

class UMediaPlayer;

UENUM(BlueprintType)
enum class EVekaTrackShape : uint8
{
	Linear,
	Circular
};

/**
 * Track of a slider, gauge or progress view: a capsule or a ring, a filled part, an optional knob / marker,
 * or an indeterminate moving segment.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Track", VekaCategory = "Controls"))
struct VEKAUI_API FVekaTrack : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	EVekaTrackShape Shape = EVekaTrackShape::Linear;

	/** 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float Fraction = 0.f;

	/** A segment runs along the track (unknown progress). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	bool bIndeterminate = false;

	/** Height of a linear track, width of a ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float Thickness = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FVekaPaint TrackPaint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FVekaPaint FillPaint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	bool bShowsFill = true;

	/** Knob (slider) or marker (gauge) diameter; 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float KnobSize = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FVekaPaint KnobPaint = FVekaPaint(FLinearColor::White);

	/** Ring around the knob (marker / color slider). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float KnobBorder = 0.f;

	/** Circular: degrees of the ring (360 = full, 270 = open at the bottom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float ArcDegrees = 360.f;

	/** Width of a linear track when nothing is proposed; diameter of a ring (fixed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float IdealLength = 160.f;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;

	/** Fraction under a local x (linear tracks, knob inset taken into account). */
	static float FractionAt(double LocalX, double Width, float KnobSize);
};

/** Text input: a native Slate editable text inside a field drawn by Veka (style from `.TextFieldStyle`, focus ring). */
USTRUCT(BlueprintType, meta = (VekaName = "TextInput", VekaCategory = "Controls"))
struct VEKAUI_API FVekaTextInput : public FVekaPrimitive
{
	GENERATED_BODY()

	/** Shown when the field is not being edited (the binding's value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	FText Placeholder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	bool bSecure = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	bool bSelectAllOnFocus = false;

	/** Every edit. */
	TFunction<void(const FString&)> OnChanged;
	/** Enter (bSubmitted) or focus lost. */
	TFunction<void(const FString&, bool)> OnCommitted;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
	virtual TSharedPtr<SWidget> GetNativeWidget(FVekaPrimitiveState* State) const override;
};

/** Thin separator line across its stack (vertical in an HStack, horizontal elsewhere). */
USTRUCT(BlueprintType, meta = (VekaName = "Divider", VekaCategory = "Controls"))
struct VEKAUI_API FVekaDivider : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	float Thickness = 1.f;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
};

/** Playback bar of a media player: position, buffered track and times (repaints while playing). */
USTRUCT(BlueprintType, meta = (VekaName = "MediaScrubber", VekaCategory = "Controls"))
struct VEKAUI_API FVekaMediaScrubber : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Controls")
	TObjectPtr<UMediaPlayer> Player;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;

	/** Width taken by the time labels on each side of the bar. */
	static constexpr float LabelWidth = 44.f;
};
