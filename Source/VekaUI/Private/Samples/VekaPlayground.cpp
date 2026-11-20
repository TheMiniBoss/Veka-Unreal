// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaPlayground.h"
#include "VekaBuilders.h"
#include "Colora/VekaTheme.h"
#include "Modifiers/VekaColoraModifiers.h"
#include "Engine/Texture2D.h"
#include "Anima/VekaImplicit.h"
#include "Anima/VekaTimeline.h"

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


	FVekaGradientStop Stop(const TCHAR* Hex, float Location) { return FVekaGradientStop(Veka::Hex(Hex), Location); }

	FVekaElement Tile(const FVekaPaint& Paint, const FText& Caption, float Radius = 14.f)
	{
		using namespace Veka;
		return VStack(6)(
			RoundedRectangle(Radius).Fill(Paint).Frame(110, 76),
			Text(Caption).Foreground(Muted).Font(Fonts::Caption())
		);
	}

	/** Mesh gradient whose inner points drift (tap to start / stop). */
	class FMeshView : public FVekaView
	{
		VEKA_VIEW(FMeshView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<float> Phase = Ctx.State<float>(TEXT("Phase"), 0.f);
			const TVekaState<bool> Running = Ctx.State<bool>(TEXT("Running"), false);

			FVekaMeshGradient Mesh = FVekaMeshGradient::Grid(3, 3, {
				Hex(TEXT("#1B1464FF")), Hex(TEXT("#6A1B9AFF")), Hex(TEXT("#0D47A1FF")),
				Hex(TEXT("#FF6F61FF")), Hex(TEXT("#FFC75FFF")), Hex(TEXT("#00C9A7FF")),
				Hex(TEXT("#845EC2FF")), Hex(TEXT("#D65DB1FF")), Hex(TEXT("#2C73D2FF")) });
			const float P = Phase.Get();
			Mesh.At(1, 1).Position = FVector2D(0.5 + 0.22 * FMath::Sin(P * UE_TWO_PI), 0.5 + 0.18 * FMath::Cos(P * UE_TWO_PI * 2.0));
			Mesh.At(1, 0).Position.X = 0.5 + 0.2 * FMath::Cos(P * UE_TWO_PI);
			Mesh.At(1, 2).Position.X = 0.5 - 0.2 * FMath::Cos(P * UE_TWO_PI);
			Mesh.At(0, 1).Position.Y = 0.5 + 0.2 * FMath::Sin(P * UE_TWO_PI);
			Mesh.At(2, 1).Position.Y = 0.5 - 0.2 * FMath::Sin(P * UE_TWO_PI);

			return VStack(6, EVekaHAlign::Leading)(
				RoundedRectangle(18, true).Fill(FVekaPaint::FromMesh(Mesh)).Frame(260, 150)
					.OnTap([Phase, Running]()
					{
						const bool bNext = !Running.Get();
						Running.Set(bNext);
						Phase.Animate(bNext ? 1.f : 0.f, bNext ? FVekaAnimationSpec::Linear(6.0).RepeatForever(false) : FVekaAnimationSpec::Smooth());
					}),
				Text(INVTEXT("Mesh gradient 3x3 (Oklab, bicubic). Tap to animate the points.")).Foreground(Muted).Font(Fonts::Caption())
			);
		}
	};

	/** Token card that switches its own color scheme. */
	class FSchemeView : public FVekaView
	{
		VEKA_VIEW(FSchemeView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Light = Ctx.State<bool>(TEXT("Light"), false);
			const FName Swatches[] = { TEXT("Red"), TEXT("Orange"), TEXT("Yellow"), TEXT("Green"), TEXT("Mint"), TEXT("Teal"), TEXT("Cyan"),
				TEXT("Blue"), TEXT("Indigo"), TEXT("Purple"), TEXT("Pink"), TEXT("Brown"), TEXT("Gray") };

			TArray<FVekaElement> Dots;
			for (const FName& Token : Swatches)
			{
				Dots.Add(Circle().Fill(FVekaPaint::FromToken(Token)).Frame(22, 22));
			}

			return VStack(10, EVekaHAlign::Leading)(
				HStack(8)(
					Text(Light.Get() ? INVTEXT("Light scheme") : INVTEXT("Dark scheme")).Font(Fonts::Headline()),
					Spacer(),
					Text(INVTEXT("Switch")).Font(Fonts::Bold(13)).Foreground(FVekaPaint::FromToken(TEXT("White")))
						.Padding(FMargin(12, 6)).Background(FVekaPaint::FromToken(TEXT("Accent")), 8)
						.OnTap([Light]() { Light.Set(!Light.Get()); })
				),
				HStack(6).Children(MoveTemp(Dots)),
				HStack(6)(
					Capsule().Fill(FVekaPaint::FromToken(TEXT("Accent"))).Frame(60, 10),
					Capsule().Fill(FVekaPaint::FromToken(TEXT("Accent"), EVekaHierarchy::Secondary)).Frame(60, 10),
					Capsule().Fill(FVekaPaint::FromToken(TEXT("Accent"), EVekaHierarchy::Tertiary)).Frame(60, 10),
					Capsule().Fill(FVekaPaint::FromToken(TEXT("Accent"), EVekaHierarchy::Quaternary)).Frame(60, 10)
				),
				Text(INVTEXT("Tokens resolve from the theme and the color scheme of this subtree; hierarchy = primary .. quaternary."))
					.Foreground(FVekaPaint::FromToken(TEXT("Foreground"), EVekaHierarchy::Secondary)).Font(Fonts::Caption())
			)
			.Padding(14)
			.FillWidth(FVekaAlignment::TopLeading())
			.Background(FVekaPaint::FromToken(TEXT("WindowBackground")), 12)
			.Border(FVekaPaint::FromToken(TEXT("Separator")), 1, 12)
			.Foreground(FVekaPaint::FromToken(TEXT("Foreground")))
			.ColorScheme(Light.Get() ? EVekaColorScheme::Light : EVekaColorScheme::Dark);
		}
	};

	FVekaElement ColoraSection()
	{
		using namespace Veka;
		const TArray<FVekaGradientStop> Sunset = { Stop(TEXT("#FF5F6DFF"), 0.f), Stop(TEXT("#FFC371FF"), 1.f) };
		const TArray<FVekaGradientStop> Rainbow = {
			Stop(TEXT("#FF3B30FF"), 0.f), Stop(TEXT("#FFCC00FF"), 0.17f), Stop(TEXT("#34C759FF"), 0.33f), Stop(TEXT("#00C7BEFF"), 0.5f),
			Stop(TEXT("#007AFFFF"), 0.67f), Stop(TEXT("#AF52DEFF"), 0.83f), Stop(TEXT("#FF3B30FF"), 1.f) };
		const FVekaPaint Hero = FVekaPaint::Linear(Sunset, FVector2D(0, 0), FVector2D(1, 1));

		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

		return VStack(16, EVekaHAlign::Leading)(
			HStack(14)(
				Tile(Hero, INVTEXT("Linear 2D")),
				Tile(FVekaPaint::Radial({ Stop(TEXT("#FFFFFFFF"), 0.f), Stop(TEXT("#4C7DFFFF"), 0.45f), Stop(TEXT("#16161AFF"), 1.f) }, 0.f, 60.f, FVector2D(0.3, 0.3)), INVTEXT("Radial")),
				Tile(FVekaPaint::Elliptical({ Stop(TEXT("#FFD84CFF"), 0.f), Stop(TEXT("#FF5CB8FF"), 1.f) }), INVTEXT("Elliptical")),
				Tile(FVekaPaint::Angular(Rainbow), INVTEXT("Angular"), 38.f),
				Tile(Texture ? FVekaPaint::FromImage(Texture) : FVekaPaint(Muted), INVTEXT("Image"))
			),
			VStack(6, EVekaHAlign::Leading)(
				Capsule().Fill(FVekaPaint::Linear(Hex(TEXT("#FF0000FF")), Hex(TEXT("#0000FFFF"))).InSpace(EVekaColorSpace::SRGB)).Frame(320, 16),
				Capsule().Fill(FVekaPaint::Linear(Hex(TEXT("#FF0000FF")), Hex(TEXT("#0000FFFF"))).InSpace(EVekaColorSpace::OkLab)).Frame(320, 16),
				Capsule().Fill(FVekaPaint::Linear(Hex(TEXT("#FF0000FF")), Hex(TEXT("#0000FFFF"))).InSpace(EVekaColorSpace::OkLCH)).Frame(320, 16),
				Text(INVTEXT("Red to blue in sRGB, Oklab, OkLCH")).Foreground(Muted).Font(Fonts::Caption())
			),
			HStack(14)(
				Tile(Hero, INVTEXT("Original")),
				Tile(Hero, INVTEXT("Saturation 0")).Saturation(0.f),
				Tile(Hero, INVTEXT("Hue +120")).HueRotation(120.f),
				Tile(Hero, INVTEXT("Brightness +0.2")).Brightness(0.2f),
				Tile(Hero, INVTEXT("Contrast 1.6")).Contrast(1.6f)
			),
			HStack(24)(
				ZStack()(
					Circle().Fill(Hex(TEXT("#FF0000FF"))).Frame(70, 70).Offset(FVector2D(-20, 12)),
					Circle().Fill(Hex(TEXT("#00FF00FF"))).Frame(70, 70).Offset(FVector2D(20, 12)),
					Circle().Fill(Hex(TEXT("#0000FFFF"))).Frame(70, 70).Offset(FVector2D(0, -22))
				).BlendMode(EVekaBlendMode::Add).Frame(130, 120),
				Circle().Stroke(FVekaPaint::Angular(Rainbow), 8).Frame(100, 100),
				View<FMeshView>()
			),
			View<FSchemeView>()
		);
	}


	FVekaElement Button(const FText& Label, TFunction<void()> Action)
	{
		using namespace Veka;
		return Text(Label).Font(Fonts::Bold(13)).Foreground(FLinearColor::White)
			.Padding(FMargin(12, 6)).Background(FVekaPaint::FromToken(TEXT("Accent")), 8)
			.OnTap(MoveTemp(Action));
	}

	/** Implicit animation: every property of the card follows one bool. */
	class FImplicitCardView : public FVekaView
	{
		VEKA_VIEW(FImplicitCardView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Expanded = Ctx.State<bool>(TEXT("Expanded"), false);
			const bool bOn = Expanded.Get();
			return VStack(6, EVekaHAlign::Leading)(
				ZStack()(
					RoundedRectangle(bOn ? 36.f : 10.f).Fill(bOn ? Hex(TEXT("#FF5CB8FF")) : Hex(TEXT("#4C7DFFFF"))),
					Text(bOn ? INVTEXT("Expanded") : INVTEXT("Tap me")).Font(Fonts::Bold(bOn ? 22 : 14)).Foreground(FLinearColor::White)
				)
				.Frame(bOn ? 260.0 : 140.0, bOn ? 120.0 : 60.0)
				.Rotation(bOn ? 3.f : 0.f)
				.Offset(FVector2D(bOn ? 20.0 : 0.0, 0.0))
				.Animation(FVekaAnimationSpec::Bouncy(), bOn)
				.OnTap([Expanded]() { Expanded.Set(!Expanded.Get()); }),
				Text(INVTEXT(".Animation(Bouncy, expanded): size, radius, color, font size, rotation, offset.")).Foreground(Muted).Font(Fonts::Caption())
			);
		}
	};

	/** WithAnimation + insertion / removal transitions + layout transitions. */
	class FAnimatedListView : public FVekaView
	{
		VEKA_VIEW(FAnimatedListView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<TArray<int32>> Items = Ctx.State<TArray<int32>>(TEXT("Items"), TArray<int32>{ 1, 2, 3 });
			const TVekaState<int32> Next = Ctx.State<int32>(TEXT("Next"), 4);
			const TArray<FLinearColor> Colors = { Hex(TEXT("#FF5E57FF")), Hex(TEXT("#FFA84CFF")), Hex(TEXT("#5CD65CFF")), Hex(TEXT("#4CD9C9FF")), Hex(TEXT("#8E5CFFFF")) };

			return VStack(8, EVekaHAlign::Leading)(
				HStack(8)(
					Button(INVTEXT("Add on top"), [Items, Next]()
					{
						WithAnimation(FVekaAnimationSpec::Snappy(), [&]
						{
							Items.Modify([&](TArray<int32>& List) { List.Insert(Next.Get(), 0); });
							Next.Set(Next.Get() + 1);
						});
					}),
					Text(INVTEXT("Tap a row to remove it")).Foreground(Muted).Font(Fonts::Caption())
				),
				VStack(6, EVekaHAlign::Leading)(
					ForEach(Items.Get(), [](int32 Item) { return Item; }, [Items, Colors](int32 Item)
					{
						return Text(FText::Format(INVTEXT("Row {0}"), FText::AsNumber(Item))).Font(Fonts::Bold(13)).Foreground(FLinearColor::White)
							.Padding(FMargin(12, 6)).Frame(220, 30, FVekaAlignment::Leading())
							.Background(Colors[Item % 5], 8)
							.Transition(FVekaTransition::Asymmetric(FVekaTransition::Push(EVekaEdge::Top), FVekaTransition::Combined(FVekaTransition::Scale(0.6f), FVekaTransition::Opacity())))
							.OnTap([Items, Item]()
							{
								WithAnimation(FVekaAnimationSpec::Smooth(), [&] { Items.Modify([Item](TArray<int32>& List) { List.Remove(Item); }); });
							});
					})
				)
			);
		}
	};

	/** The Alpha 13 transitions, shown / hidden together. */
	class FTransitionGalleryView : public FVekaView
	{
		VEKA_VIEW(FTransitionGalleryView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Shown = Ctx.State<bool>(TEXT("Shown"), true);
			const bool bShown = Shown.Get();
			auto Box = [bShown](const TCHAR* Name, const FVekaTransition& Transition, const TCHAR* Color)
			{
				return VStack(4)(
					ZStack()(
						RoundedRectangle(10).Fill(FLinearColor(1, 1, 1, 0.05f)).Frame(64, 64),
						If(bShown, RoundedRectangle(10).Fill(Hex(Color)).Frame(64, 64).Transition(Transition))
					),
					Text(Name).Foreground(Muted).Font(Fonts::Caption())
				);
			};
			return VStack(10, EVekaHAlign::Leading)(
				Button(bShown ? INVTEXT("Hide") : INVTEXT("Show"), [Shown]() { WithAnimation(FVekaAnimationSpec::EaseInOut(0.6), [&] { Shown.Set(!Shown.Get()); }); }),
				HStack(14)(
					Box(TEXT("Opacity"), FVekaTransition::Opacity(), TEXT("#FF5E57FF")),
					Box(TEXT("Scale"), FVekaTransition::Scale(), TEXT("#FFA84CFF")),
					Box(TEXT("Slide"), FVekaTransition::Slide(), TEXT("#FFD84CFF")),
					Box(TEXT("Move top"), FVekaTransition::Move(EVekaEdge::Top), TEXT("#5CD65CFF")),
					Box(TEXT("Push bottom"), FVekaTransition::Push(EVekaEdge::Bottom), TEXT("#4CD9C9FF")),
					Box(TEXT("Offset"), FVekaTransition::Offset(FVector2D(40, -30)), TEXT("#4C7DFFFF")),
					Box(TEXT("Blur*"), FVekaTransition::Blur(), TEXT("#8E5CFFFF")),
					Box(TEXT("Asymmetric"), FVekaTransition::Asymmetric(FVekaTransition::Scale(), FVekaTransition::Move(EVekaEdge::Bottom)), TEXT("#FF5CB8FF"))
				),
				Text(INVTEXT("* Blur is approximated by opacity until the render module (Phase 4).")).Foreground(Muted).Font(Fonts::Caption())
			);
		}
	};

	/** Same move with different curves. */
	class FCurvesView : public FVekaView
	{
		VEKA_VIEW(FCurvesView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Right = Ctx.State<bool>(TEXT("Right"), false);
			const bool bRight = Right.Get();
			auto Row = [bRight](const TCHAR* Name, const FVekaAnimationSpec& Spec)
			{
				return HStack(10)(
					Text(Name).Foreground(Muted).Font(Fonts::Caption()).Frame(130, 16, FVekaAlignment::Leading()),
					ZStack(FVekaAlignment::Leading())(
						Capsule().Fill(FLinearColor(1, 1, 1, 0.06f)).Frame(240, 16),
						Circle().Fill(Accent).Frame(16, 16).Offset(FVector2D(bRight ? 224.0 : 0.0, 0.0)).Animation(Spec, bRight)
					)
				);
			};
			return VStack(6, EVekaHAlign::Leading)(
				Button(INVTEXT("Go"), [Right]() { Right.Set(!Right.Get()); }),
				Row(TEXT("Linear 0.8 s"), FVekaAnimationSpec::Linear(0.8)),
				Row(TEXT("EaseInOut 0.8 s"), FVekaAnimationSpec::EaseInOut(0.8)),
				Row(TEXT("Timing (0.7,0,0.2,1)"), FVekaAnimationSpec::Timing(0.7, 0.0, 0.2, 1.0, 0.8)),
				Row(TEXT("Spring smooth"), FVekaAnimationSpec::Smooth()),
				Row(TEXT("Spring bouncy"), FVekaAnimationSpec::Bouncy()),
				Row(TEXT("Delayed 0.3 s"), FVekaAnimationSpec::EaseInOut(0.5).Delayed(0.3))
			);
		}
	};

	/** Timeline: keyframed tracks + events, read in Body. */
	class FTimelineView : public FVekaView
	{
		VEKA_VIEW(FTimelineView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			FVekaTimelineData Data;
			Data.AddTrack(TEXT("X"), { FVekaTimelineKey(0.f, 0.f), FVekaTimelineKey(1.f, 220.f, EVekaAnimationCurve::EaseIn), FVekaTimelineKey(1.6f, 120.f), FVekaTimelineKey(2.4f, 0.f) });
			Data.AddTrack(TEXT("Size"), { FVekaTimelineKey(0.f, 18.f), FVekaTimelineKey(1.f, 34.f), FVekaTimelineKey(2.4f, 18.f) });
			Data.AddEvent(1.f, TEXT("Peak")).AddEvent(2.4f, TEXT("Done"));

			const TVekaState<FName> LastEvent = Ctx.State<FName>(TEXT("LastEvent"), NAME_None);
			const FVekaTimelineHandle Timeline = Ctx.Timeline(TEXT("Demo"), Data);
			Timeline.OnEvent([LastEvent](FName Event) { LastEvent.Set(Event); });
			const float Size = Timeline.Value(TEXT("Size"));

			return VStack(8, EVekaHAlign::Leading)(
				HStack(8)(
					Button(Timeline.IsPlaying() ? INVTEXT("Pause") : INVTEXT("Play"), [Timeline]() { if (Timeline.IsPlaying()) { Timeline.Pause(); } else { Timeline.Play(); } }),
					Button(INVTEXT("Reverse"), [Timeline]() { Timeline.Reverse(); }),
					Button(INVTEXT("Stop"), [Timeline]() { Timeline.Stop(); }),
					Text(FText::Format(INVTEXT("t = {0} s, last event: {1}"), FText::AsNumber(FMath::RoundToFloat(Timeline.GetTime() * 100.f) / 100.f), FText::FromName(LastEvent.Get())))
						.Foreground(Muted).Font(Fonts::Caption())
				),
				ZStack(FVekaAlignment::Leading())(
					Capsule().Fill(FLinearColor(1, 1, 1, 0.06f)).Frame(260, 40),
					Circle().Fill(FVekaPaint::Angular({ FVekaGradientStop(Hex(TEXT("#FF5E57FF")), 0.f), FVekaGradientStop(Hex(TEXT("#4C7DFFFF")), 0.5f), FVekaGradientStop(Hex(TEXT("#FF5E57FF")), 1.f) }))
						.Frame(Size, Size).Offset(FVector2D(Timeline.Value(TEXT("X")), 0.0))
				)
			);
		}
	};

	FVekaElement AnimaSection()
	{
		using namespace Veka;
		return VStack(18, EVekaHAlign::Leading)(
			HStack(30, EVekaVAlign::Top)(View<FImplicitCardView>(), View<FAnimatedListView>()),
			View<FTransitionGalleryView>(),
			HStack(30, EVekaVAlign::Top)(View<FCurvesView>(), View<FTimelineView>())
		);
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
				Text(INVTEXT("VekaUI - Playground (Phase 1 + Colora + Anima)")).Font(Fonts::LargeTitle()),
				Text(INVTEXT("Declarative Slate-first views: one Stack for H/V/Z, modifiers, state, springs, vector shapes and strokes. "
					"This paragraph wraps to the available width."))
					.Foreground(Muted),

				Section(INVTEXT("Anima: implicit, WithAnimation, transitions, layout, curves, timeline"), AnimaSection()),

				Section(INVTEXT("Colora: paints, color spaces, adjustments, additive blend, tokens"), ColoraSection()),

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
