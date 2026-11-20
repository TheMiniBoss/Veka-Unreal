// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaPlayground.h"
#include "VekaBuilders.h"

namespace VekaPlaygroundPrivate
{
	const FLinearColor Background = Veka::Hex(TEXT("#16161AFF"));
	const FLinearColor Card = Veka::Hex(TEXT("#24242BFF"));
	const FLinearColor Accent = Veka::Hex(TEXT("#4C7DFFFF"));
	const FLinearColor Muted = Veka::Hex(TEXT("#9A9AA8FF"));

	FVekaElement Section(const FText& Title, FVekaElement Content)
	{
		using namespace Veka;
		return VStack(10, EVekaHAlign::Leading)(
			Text(Title).Font(Fonts::Headline()),
			MoveTemp(Content)
		)
		.Padding(16)
		.FillWidth(FVekaAlignment::TopLeading())
		.Background(Card, 14);
	}

	/** Counter with @State and a tap gesture. */
	class FCounterView : public FVekaView
	{
		VEKA_VIEW(FCounterView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Count = Ctx.State<int32>(TEXT("Count"), 0);
			const TVekaState<bool> Pressed = Ctx.State<bool>(TEXT("Pressed"), false);

			return HStack(12)(
				Text(FText::Format(INVTEXT("Tapped {0} times"), FText::AsNumber(Count.Get()))),
				Spacer(),
				Text(INVTEXT("+1"))
					.Font(Fonts::Bold(14))
					.Padding(FMargin(16, 8))
					.Background(Accent, 10)
					.Scale(Pressed.Get() ? 0.94f : 1.f)
					.OnPress([Pressed](bool bDown) { Pressed.Set(bDown); })
					.OnTap([Count]() { Count.Set(Count.Get() + 1); })
			);
		}
	};

	/** A dot driven by a spring when tapped. */
	class FSpringView : public FVekaView
	{
		VEKA_VIEW(FSpringView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<float> Progress = Ctx.State<float>(TEXT("Progress"), 0.f);
			const TVekaState<bool> bRight = Ctx.State<bool>(TEXT("Right"), false);

			return VStack(8, EVekaHAlign::Leading)(
				ZStack(FVekaAlignment::Leading())(
					Capsule().Fill(FLinearColor(1, 1, 1, 0.08f)).Frame(260, 36),
					Circle()
						.Fill(Accent)
						.Frame(28, 28)
						.Padding(4)
						.Offset(FVector2D(Progress.Get() * 224.0, 0.0))
				)
				.OnTap([Progress, bRight]()
				{
					const bool bNext = !bRight.Get();
					bRight.Set(bNext);
					Progress.Animate(bNext ? 1.f : 0.f, FVekaAnimationSpec::Bouncy());
				}),
				Text(INVTEXT("Tap the track: spring (response 0.5, damping 0.7)")).Foreground(Muted).Font(Fonts::Caption())
			);
		}
	};

	/** Strokes: dashed, trimmed (animated) and a path from SVG. */
	class FStrokeView : public FVekaView
	{
		VEKA_VIEW(FStrokeView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<float> Trim = Ctx.State<float>(TEXT("Trim"), 1.f);

			FVekaStrokeStyle Dashed(FVekaPaint(Accent), 2.f);
			Dashed.Dash = { 6.f, 4.f };

			return HStack(16)(
				RoundedRectangle(12).Stroke(Dashed).Frame(70, 70),
				Circle().Stroke(FVekaPaint(FLinearColor(0.3f, 0.85f, 0.5f)), 4).Trim(0.f, Trim.Get()).Frame(70, 70)
					.OnTap([Trim]() { Trim.Animate(Trim.Get() > 0.5f ? 0.f : 1.f, FVekaAnimationSpec::EaseInOut(0.8)); }),
				SvgPath(TEXT("M35 2 L43 26 L68 26 L48 41 L56 66 L35 51 L14 66 L22 41 L2 26 L27 26 Z"))
					.Fill(FLinearColor(1.f, 0.75f, 0.2f))
					.Frame(70, 70),
				Ellipse().Fill(FVekaPaint::Linear(Accent, FLinearColor(0.85f, 0.3f, 0.9f))).Frame(90, 60)
			);
		}
	};
}

namespace Veka::Samples
{
	FVekaElement Playground()
	{
		using namespace VekaPlaygroundPrivate;
		const FLinearColor Swatches[] = {
			Hex(TEXT("#FF5E57FF")), Hex(TEXT("#FFA84CFF")), Hex(TEXT("#FFD84CFF")), Hex(TEXT("#5CD65CFF")),
			Hex(TEXT("#4CD9C9FF")), Hex(TEXT("#4C7DFFFF")), Hex(TEXT("#8E5CFFFF")), Hex(TEXT("#FF5CB8FF")),
		};

		return ScrollView(EVekaAxis::Vertical)(
			VStack(16, EVekaHAlign::Leading)(
				Text(INVTEXT("VekaUI - Phase 1 Playground")).Font(Fonts::LargeTitle()),
				Text(INVTEXT("Declarative Slate-first views: one Stack for H/V/Z, modifiers, state, springs, vector shapes and strokes. "
					"This paragraph wraps to the available width."))
					.Foreground(Muted),

				Section(INVTEXT("Shapes (exact hit-testing)"),
					HStack(12)(
						Rectangle().Fill(Swatches[0]).Frame(56, 56),
						RoundedRectangle(14).Fill(Swatches[1]).Frame(56, 56),
						RoundedRectangle(14, true).Fill(Swatches[2]).Frame(56, 56),
						Capsule().Fill(Swatches[3]).Frame(90, 40),
						Circle().Fill(Swatches[4]).Frame(56, 56),
						Ellipse().Fill(Swatches[5]).Frame(80, 50)
					)),

				Section(INVTEXT("State + gestures"), View<FCounterView>()),
				Section(INVTEXT("Anima: spring"), View<FSpringView>()),
				Section(INVTEXT("Strokes (tap the ring to animate the trim)"), View<FStrokeView>()),

				Section(INVTEXT("Grid (adaptive columns)"),
					Grid({ FVekaGridItem::Adaptive(64.f) }, 8.f, 8.f)(
						ForEach(16, [&Swatches](int32 Index)
						{
							return RoundedRectangle(10)
								.Fill(Swatches[Index % 8])
								.FlexFrame(-1, Veka::Infinity, 44, 44)
								.Shadow(FLinearColor(0, 0, 0, 0.35f), 6, FVector2D(0, 2), 10);
						})
					)),

				Section(INVTEXT("Layout: Spacer and alignment"),
					VStack(8)(
						HStack()(Text(INVTEXT("Leading")), Spacer(), Text(INVTEXT("Trailing"))).Padding(8).Border(Muted, 1, 8),
						HStack()(Spacer(), Text(INVTEXT("Centered")), Spacer()).Padding(8).Border(Muted, 1, 8)
					).FillWidth())
			)
			.Padding(24)
			.FillWidth(FVekaAlignment::TopLeading())
		)
		.Background(Background);
	}
}
