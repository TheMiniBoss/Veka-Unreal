// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaFireworks.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"

namespace
{
	/** Two colors per burst (most sparks in the first): gold, rose, green, cyan, violet, coral. */
	const FLinearColor Palettes[][2] = {
		{ FLinearColor(1.00f, 0.78f, 0.32f), FLinearColor(1.00f, 0.95f, 0.80f) },
		{ FLinearColor(1.00f, 0.36f, 0.56f), FLinearColor(1.00f, 0.72f, 0.40f) },
		{ FLinearColor(0.45f, 1.00f, 0.50f), FLinearColor(0.85f, 1.00f, 0.55f) },
		{ FLinearColor(0.38f, 0.86f, 1.00f), FLinearColor(0.85f, 0.95f, 1.00f) },
		{ FLinearColor(0.78f, 0.48f, 1.00f), FLinearColor(1.00f, 0.55f, 0.85f) },
		{ FLinearColor(1.00f, 0.42f, 0.30f), FLinearColor(0.45f, 1.00f, 0.60f) },
	};

	struct FBurst
	{
		FVector2D Center;
		double Start = 0.0;
		int32 Seed = 0;
		int32 Palette = 0;
		float Radius = 0.f;
	};

	struct FFireworksState : public FVekaPrimitiveState
	{
		FRandomStream Random;
		bool bStarted = false;
		double LastPaint = -1.0;
		double NextBurst = 0.0;
		TArray<FBurst> Bursts;

		// Fireworks never settle: the node repaints every frame while it exists.
		virtual bool NeedsAnimationFrame(double Now) const override { return true; }
	};

	/** 1 - (1 - x)^3: fast at first, then hangs in the air. */
	double EaseOut(double X)
	{
		const double Inv = 1.0 - FMath::Clamp(X, 0.0, 1.0);
		return 1.0 - Inv * Inv * Inv;
	}
}

FVector2D FVekaFireworks::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	return Proposal.ReplaceUnspecified(FVector2D(100.0, 100.0));
}

TSharedPtr<FVekaPrimitiveState> FVekaFireworks::CreateState() const
{
	return MakeShared<FFireworksState>();
}

int32 FVekaFireworks::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	FFireworksState* State = static_cast<FFireworksState*>(Context.State);
	if (!State || Bounds.Size.X <= 1.0 || Bounds.Size.Y <= 1.0)
	{
		return Context.LayerId;
	}

	const double Now = FPlatformTime::Seconds();
	const double Life = FMath::Max(0.2, static_cast<double>(Duration));
	if (!State->bStarted)
	{
		State->Random.Initialize(Seed != 0 ? Seed : static_cast<int32>(FPlatformTime::Cycles()));
		State->NextBurst = Now + 0.12;
		State->bStarted = true;
	}
	// Not painted for a while (hidden, scrolled away): start again instead of firing the backlog.
	if (State->LastPaint >= 0.0 && Now - State->LastPaint > 0.5)
	{
		State->Bursts.Reset();
		State->NextBurst = Now + 0.1;
	}
	State->LastPaint = Now;

	// Spawn: random places (kept inside the frame), irregular intervals around 1 / Rate.
	State->Bursts.RemoveAll([Now, Life](const FBurst& Burst) { return Now - Burst.Start > Life; });
	const double Period = 1.0 / FMath::Max(0.05, static_cast<double>(Rate));
	while (Now >= State->NextBurst)
	{
		if (State->Bursts.Num() < 16)
		{
			FRandomStream& R = State->Random;
			FBurst Burst;
			Burst.Radius = Radius * R.FRandRange(0.75f, 1.15f);
			const double MarginX = FMath::Min(static_cast<double>(Burst.Radius) * 0.8, Bounds.Size.X * 0.35);
			const double MarginY = FMath::Min(static_cast<double>(Burst.Radius) * 0.6, Bounds.Size.Y * 0.3);
			Burst.Center = FVector2D(FMath::Lerp(MarginX, Bounds.Size.X - MarginX, R.GetFraction()), FMath::Lerp(MarginY, Bounds.Size.Y - MarginY * 1.4, R.GetFraction()));
			Burst.Start = State->NextBurst;
			Burst.Seed = R.RandHelper(MAX_int32);
			Burst.Palette = R.RandHelper(UE_ARRAY_COUNT(Palettes));
			State->Bursts.Add(Burst);
		}
		State->NextBurst += Period * State->Random.FRandRange(0.35f, 1.65f);
	}

	// Round dots: rounded boxes whose corner radius is half their size (Slate does not clamp a larger radius: the box
	// would vanish), one brush per quarter pixel of radius.
	static const TArray<FSlateRoundedBoxBrush> Dots = []
	{
		TArray<FSlateRoundedBoxBrush> Brushes;
		for (int32 Step = 1; Step <= 128; ++Step)
		{
			Brushes.Emplace(FLinearColor::White, Step * 0.25f);
		}
		return Brushes;
	}();
	const int32 Layer = Context.LayerId;
	auto Disc = [&](const FVector2D& Center, double DotRadius, const FLinearColor& Color)
	{
		if (Color.A <= 0.004f || DotRadius <= 0.125)
		{
			return;
		}
		const int32 Step = FMath::Clamp(FMath::RoundToInt(DotRadius * 4.0), 1, Dots.Num());
		const double Snapped = Step * 0.25;
		const FVekaRect Rect(Bounds.Position + Center - FVector2D(Snapped), FVector2D(Snapped * 2.0));
		FSlateDrawElement::MakeBox(Context.DrawElements, Layer, Context.MakePaintGeometry(Rect), &Dots[Step - 1], Context.DrawEffects, Context.Tint(Color));
	};

	const int32 Count = FMath::Clamp(Particles, 4, 400);
	const double Expand = Life * 0.55;
	for (const FBurst& Burst : State->Bursts)
	{
		const double T = Now - Burst.Start;
		if (T < 0.0)
		{
			continue;
		}
		const double U = T / Life;

		// The flash where it bursts.
		if (T < 0.2)
		{
			const double F = T / 0.2;
			const double FlashRadius = Size * (2.0 + 4.0 * F);
			const float Alpha = static_cast<float>(FMath::Pow(1.0 - F, 1.5));
			Disc(Burst.Center, FlashRadius * 2.4, FLinearColor(1.f, 1.f, 1.f, 0.14f * Alpha));
			Disc(Burst.Center, FlashRadius * 1.5, FLinearColor(1.f, 1.f, 1.f, 0.35f * Alpha));
			Disc(Burst.Center, FlashRadius, FLinearColor(1.f, 1.f, 1.f, Alpha));
		}

		// The sparks: a projected sphere (more of them near the rim), white hot at first, then their color.
		FRandomStream R(Burst.Seed);
		const double Fade = 1.0 - FMath::SmoothStep(0.45, 1.0, U);
		const float Heat = static_cast<float>(FMath::Clamp(T / 0.3, 0.0, 1.0));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = (Index + R.FRandRange(-0.5f, 0.5f)) * UE_TWO_PI / Count;
			const double Speed = FMath::Sqrt(R.FRandRange(0.12f, 1.f));
			const double Phase = R.FRandRange(0.f, UE_TWO_PI);
			const double SizeFactor = R.FRandRange(0.7f, 1.25f);
			const FLinearColor Color = Palettes[Burst.Palette][R.FRand() < 0.72f ? 0 : 1];
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			const double Reach = Burst.Radius * Speed;

			auto PositionAt = [&](double Time)
			{
				return Burst.Center + Direction * (Reach * EaseOut(Time / Expand)) + FVector2D(0.0, 0.5 * Gravity * Time * Time);
			};

			const double Twinkle = U > 0.45 ? 0.6 + 0.4 * FMath::Sin(T * 30.0 + Phase) : 1.0;
			const float Alpha = static_cast<float>(Fade * Twinkle);
			if (Alpha <= 0.004f)
			{
				continue;
			}
			FLinearColor Spark = FMath::Lerp(FLinearColor::White, Color, Heat);
			const double SparkRadius = 0.5 * Size * SizeFactor * (1.0 - 0.45 * U);
			const FVector2D Position = PositionAt(T);

			// Glow, short trail while it flies, core.
			Disc(Position, SparkRadius * 2.6, FLinearColor(Color.R, Color.G, Color.B, 0.16f * Alpha));
			if (T < Expand)
			{
				for (int32 Ghost = 1; Ghost <= 2; ++Ghost)
				{
					const double GhostTime = T - 0.035 * Ghost;
					if (GhostTime > 0.0)
					{
						Disc(PositionAt(GhostTime), SparkRadius * (1.0 - 0.2 * Ghost), FLinearColor(Spark.R, Spark.G, Spark.B, Alpha * 0.4f / Ghost));
					}
				}
			}
			Spark.A = Alpha;
			Disc(Position, SparkRadius, Spark);
		}
	}
	return Layer;
}

namespace Veka
{
	FVekaElement Fireworks(float Rate, int32 Particles)
	{
		FVekaFireworks Primitive;
		Primitive.Rate = Rate;
		Primitive.Particles = Particles;
		return FVekaElement::Make(Primitive);
	}
}
