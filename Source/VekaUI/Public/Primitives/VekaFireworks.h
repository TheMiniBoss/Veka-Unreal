// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaElement.h"
#include "VekaFireworks.generated.h"

/**
 * Small fireworks drawn over their frame (iOS 27 birthday card): bursts at random places and irregular times, a white
 * flash, then sparks that fly out, hang, twinkle, fall a little and fade. Repaints every frame while it is shown (remove
 * it to stop it). Takes the proposed size; never takes clicks.
 *
 *   ZStack { Text("Today, Mar 13"); Fireworks(rate: 1.6, particles: 56) }
 */
USTRUCT(BlueprintType, meta = (VekaName = "Fireworks", VekaCategory = "Effects"))
struct VEKAUI_API FVekaFireworks : public FVekaPrimitive
{
	GENERATED_BODY()

	/** Bursts per second, on average (they come at irregular intervals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks", meta = (ClampMin = "0.05", ClampMax = "20"))
	float Rate = 1.6f;

	/** Sparks per burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks", meta = (ClampMin = "4", ClampMax = "400"))
	int32 Particles = 56;

	/** Radius a burst reaches, in px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks", meta = (ClampMin = "4"))
	float Radius = 34.f;

	/** Size of a spark, in px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks", meta = (ClampMin = "0.5"))
	float Size = 2.2f;

	/** Seconds a burst lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks", meta = (ClampMin = "0.2"))
	float Duration = 1.7f;

	/** Downward pull on the sparks, in px/s². */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks")
	float Gravity = 14.f;

	/** Random seed; 0 draws different fireworks every time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Fireworks")
	int32 Seed = 0;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool HitTest(const FVector2D& LocalPoint, const FVector2D& FrameSize) const override { return false; }
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
};

namespace Veka
{
	/** Fireworks over the frame (see FVekaFireworks). */
	VEKAUI_API FVekaElement Fireworks(float Rate = 1.6f, int32 Particles = 56);
}
