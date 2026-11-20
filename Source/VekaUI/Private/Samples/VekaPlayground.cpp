// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaPlayground.h"
#include "VekaBuilders.h"
#include "Colora/VekaTheme.h"
#include "Modifiers/VekaColoraModifiers.h"
#include "Engine/Texture2D.h"
#include "Anima/VekaImplicit.h"
#include "Anima/VekaTimeline.h"
#include "Symbols/VekaSymbolView.h"
#include "Controls/VekaControls.h"

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

	/** Rolling counter: NumericText content transition + monospaced digits. */
	class FCounterRollView : public FVekaView
	{
		VEKA_VIEW(FCounterRollView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Value = Ctx.State<int32>(TEXT("Value"), 1280);
			return VStack(8, EVekaHAlign::Leading)(
				Text(FText::AsNumber(Value.Get())).Font(EVekaTextStyle::LargeTitle).MonospacedDigit()
					.ContentTransition(EVekaContentTransition::NumericText, 0.45f),
				HStack(8)(
					Button(INVTEXT("+1"), [Value]() { Value.Set(Value.Get() + 1); }),
					Button(INVTEXT("+137"), [Value]() { Value.Set(Value.Get() + 137); }),
					Button(INVTEXT("-58"), [Value]() { Value.Set(Value.Get() - 58); })
				),
				Text(INVTEXT("contentTransition(.numericText) + monospacedDigit")).Foreground(Muted).Font(EVekaTextStyle::Caption1)
			);
		}
	};

	/** Effects replay when the text changes. */
	class FTextEffectsView : public FVekaView
	{
		VEKA_VIEW(FTextEffectsView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Round = Ctx.State<int32>(TEXT("Round"), 0);
			const FText Line = Round.Get() % 2 == 0 ? INVTEXT("Welcome to VekaUI") : INVTEXT("Glyph by glyph");

			FVekaTextEffect Appear;
			Appear.Kind = EVekaTextEffectKind::Appear;
			FVekaTextEffect Typewriter;
			Typewriter.Kind = EVekaTextEffectKind::Typewriter;
			Typewriter.Stagger = 0.05f;
			FVekaTextEffect Wave;
			Wave.Kind = EVekaTextEffectKind::Wave;
			Wave.Amplitude = 4.f;

			return VStack(8, EVekaHAlign::Leading)(
				Button(INVTEXT("Replay"), [Round]() { Round.Set(Round.Get() + 1); }),
				Text(Line).Font(EVekaTextStyle::Title2).TextEffect(Appear),
				Text(Line).Font(EVekaTextStyle::Title2).TextEffect(Typewriter),
				Text(INVTEXT("Wave wave wave")).Font(EVekaTextStyle::Title2).TextEffect(Wave)
					.Foreground(FVekaPaint::Linear(Hex(TEXT("#FF5CB8FF")), Hex(TEXT("#4C7DFFFF"))))
			);
		}
	};

	/** Global text scale of a subtree (accessibility). */
	class FTextScaleView : public FVekaView
	{
		VEKA_VIEW(FTextScaleView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<float> Scale = Ctx.State<float>(TEXT("Scale"), 1.f);
			return VStack(6, EVekaHAlign::Leading)(
				HStack(8)(
					Button(INVTEXT("A-"), [Scale]() { Scale.Set(FMath::Max(Scale.Get() - 0.15f, 0.7f)); }),
					Button(INVTEXT("A+"), [Scale]() { Scale.Set(FMath::Min(Scale.Get() + 0.15f, 1.9f)); }),
					Text(FText::Format(INVTEXT("Text scale x{0}"), FText::AsNumber(Scale.Get()))).Foreground(Muted).Font(EVekaTextStyle::Caption1)
				),
				VStack(2, EVekaHAlign::Leading)(
					Text(INVTEXT("Headline")).Font(EVekaTextStyle::Headline),
					Text(INVTEXT("Body text follows the global text size.")),
					Text(INVTEXT("Footnote")).Font(EVekaTextStyle::Footnote).Foreground(Muted)
				).TextScale(Scale.Get()).Animation(FVekaAnimationSpec::Smooth(), Scale.Get())
			);
		}
	};

	FVekaElement TypographySection()
	{
		using namespace Veka;
		const TPair<EVekaTextStyle, const TCHAR*> Styles[] = {
			{ EVekaTextStyle::LargeTitle, TEXT("Large Title") }, { EVekaTextStyle::Title1, TEXT("Title 1") }, { EVekaTextStyle::Title2, TEXT("Title 2") },
			{ EVekaTextStyle::Title3, TEXT("Title 3") }, { EVekaTextStyle::Headline, TEXT("Headline") }, { EVekaTextStyle::Body, TEXT("Body") },
			{ EVekaTextStyle::Callout, TEXT("Callout") }, { EVekaTextStyle::Subheadline, TEXT("Subheadline") }, { EVekaTextStyle::Footnote, TEXT("Footnote") },
			{ EVekaTextStyle::Caption1, TEXT("Caption 1") }, { EVekaTextStyle::Caption2, TEXT("Caption 2") } };
		TArray<FVekaElement> StyleRows;
		for (const TPair<EVekaTextStyle, const TCHAR*>& Style : Styles)
		{
			StyleRows.Add(Text(Style.Value).Font(Style.Key));
		}

		const TCHAR* WeightNames[] = { TEXT("Hairline"), TEXT("Thin"), TEXT("ExtraLight"), TEXT("Light"), TEXT("News"), TEXT("Regular"),
			TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold"), TEXT("ExtraBold"), TEXT("Black"), TEXT("ExtraBlack") };
		TArray<FVekaElement> Weights;
		TArray<FVekaElement> Italics;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Weights.Add(VStack(2)(
				Text(TEXT("Ag")).Font(EVekaTextStyle::Title1).FontWeight(static_cast<EVekaFontWeight>(Index)),
				Text(WeightNames[Index]).Font(EVekaTextStyle::Caption2).Foreground(Muted)));
			Italics.Add(Text(TEXT("Ag")).Font(EVekaTextStyle::Title3).FontWeight(static_cast<EVekaFontWeight>(Index)).Italic());
		}

		const FString Long = TEXT("The quick brown fox jumps over the lazy dog near the riverbank");
		return VStack(18, EVekaHAlign::Leading)(
			HStack(40, EVekaVAlign::Top)(
				VStack(4, EVekaHAlign::Leading).Children(MoveTemp(StyleRows)),
				VStack(14, EVekaHAlign::Leading)(
					HStack(14).Children(MoveTemp(Weights)),
					HStack(22).Children(MoveTemp(Italics)),
					Text(INVTEXT("LT Flode Neue: 12 weights + italics. Default family of Veka.")).Foreground(Muted).Font(EVekaTextStyle::Caption1),
					Text(INVTEXT("Tracking 0.25 uppercase")).TextCase(EVekaTextCase::Uppercase).Tracking(0.25f).Font(EVekaTextStyle::Footnote).FontWeight(EVekaFontWeight::SemiBold),
					Text(INVTEXT("Gradient text")).Font(EVekaTextStyle::LargeTitle).FontWeight(EVekaFontWeight::Black)
						.Foreground(FVekaPaint::Linear(Hex(TEXT("#FF5E57FF")), Hex(TEXT("#8E5CFFFF")))),
					HStack(12)(
						Text(Long).LineLimit(1).Truncation(EVekaTextTruncation::Head).Frame(170, 20, FVekaAlignment::Leading()),
						Text(Long).LineLimit(1).Truncation(EVekaTextTruncation::Middle).Frame(170, 20, FVekaAlignment::Leading()),
						Text(Long).LineLimit(1).Truncation(EVekaTextTruncation::Tail).Frame(170, 20, FVekaAlignment::Leading())
					),
					Text(INVTEXT("Head / middle / tail truncation, then minimumScaleFactor(0.6):")).Foreground(Muted).Font(EVekaTextStyle::Caption1),
					Text(INVTEXT("This line shrinks to fit before it truncates")).LineLimit(1).MinimumScaleFactor(0.6f).Frame(250, 20, FVekaAlignment::Leading())
						.Background(FLinearColor(1, 1, 1, 0.05f), 4)
				)
			),
			HStack(40, EVekaVAlign::Top)(View<FCounterRollView>(), View<FTextEffectsView>(), View<FTextScaleView>())
		);
	}

	// --- Symbols ---------------------------------------------------------------------------------------------------

	FVekaElement Caption(const TCHAR* Value)
	{
		using namespace Veka;
		return Text(Value).Font(EVekaTextStyle::Caption2).Foreground(Muted);
	}

	FVekaElement Captioned(FVekaElement Content, const TCHAR* Value)
	{
		using namespace Veka;
		return VStack(4)(MoveTemp(Content), Caption(Value));
	}

	/** Variable value: tap to step 0 -> 1. */
	class FSymbolValueView : public FVekaView
	{
		VEKA_VIEW(FSymbolValueView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Step = Ctx.State<int32>(TEXT("Step"), 2);
			const float Value = Step.Get() / 4.f;
			return VStack(6, EVekaHAlign::Leading)(
				HStack(18)(
					Symbol(TEXT("wifi"), Value), Symbol(TEXT("speaker.wave.3"), Value), Symbol(TEXT("cellularbars"), Value),
					Symbol(TEXT("battery"), Value), Symbol(TEXT("waveform"), Value), Symbol(TEXT("chart.bar"), Value)
				).Font(EVekaTextStyle::Title1),
				Text(FText::Format(INVTEXT("Variable value {0} (tap)"), FText::AsNumber(Value))).Font(EVekaTextStyle::Caption2).Foreground(Muted)
			)
			.Padding(8)
			.Background(FLinearColor(1, 1, 1, 0.04f), 8)
			.OnTap([Step]() { Step.Set((Step.Get() + 1) % 5); });
		}
	};

	/** Every effect: tap a tile to play it; indefinite ones toggle. */
	class FSymbolEffectsView : public FVekaView
	{
		VEKA_VIEW(FSymbolEffectsView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			struct FEntry { EVekaSymbolEffectKind Kind; const TCHAR* Symbol; const TCHAR* Name; bool bToggle; };
			const FEntry Entries[] = {
				{ EVekaSymbolEffectKind::Bounce, TEXT("bell"), TEXT("Bounce"), false },
				{ EVekaSymbolEffectKind::Wiggle, TEXT("bell"), TEXT("Wiggle"), false },
				{ EVekaSymbolEffectKind::Appear, TEXT("star"), TEXT("Appear"), false },
				{ EVekaSymbolEffectKind::DrawOn, TEXT("checkmark.circle"), TEXT("Draw on"), false },
				{ EVekaSymbolEffectKind::Scale, TEXT("heart"), TEXT("Scale"), false },
				{ EVekaSymbolEffectKind::Pulse, TEXT("record"), TEXT("Pulse"), true },
				{ EVekaSymbolEffectKind::VariableColor, TEXT("wifi"), TEXT("Variable color"), true },
				{ EVekaSymbolEffectKind::Rotate, TEXT("gearshape"), TEXT("Rotate"), true },
				{ EVekaSymbolEffectKind::Breathe, TEXT("heart"), TEXT("Breathe"), true },
				{ EVekaSymbolEffectKind::Disappear, TEXT("cloud"), TEXT("Disappear"), true },
			};
			TArray<FVekaElement> Tiles;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Entries); ++Index)
			{
				const FEntry& Entry = Entries[Index];
				const TVekaState<int32> Count = Ctx.State<int32>(FName(*FString::Printf(TEXT("Effect%d"), Index)), 0);
				FVekaSymbolEffect Effect;
				Effect.Kind = Entry.Kind;
				FVekaElement Glyph = Symbol(Entry.Symbol).Font(EVekaTextStyle::LargeTitle).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical);
				Glyph = Entry.bToggle ? MoveTemp(Glyph).SymbolEffect(Effect, Count.Get() % 2 == 1) : MoveTemp(Glyph).SymbolEffectOnChange(Effect, Count.Get());
				Tiles.Add(VStack(6)(
					MoveTemp(Glyph).Frame(56, 48),
					Caption(Entry.bToggle ? *FString::Printf(TEXT("%s (%s)"), Entry.Name, Count.Get() % 2 == 1 ? TEXT("on") : TEXT("off")) : Entry.Name))
					.Padding(8)
					.Background(FLinearColor(1, 1, 1, 0.04f), 8)
					.OnTap([Count]() { Count.Set(Count.Get() + 1); }));
			}
			return HStack(10).Children(MoveTemp(Tiles));
		}
	};

	/** Replace and magic replace (tap). */
	class FSymbolReplaceView : public FVekaView
	{
		VEKA_VIEW(FSymbolReplaceView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<bool> Toggled = Ctx.State<bool>(TEXT("Toggled"), false);
			const bool bOn = Toggled.Get();
			return HStack(24)(
				Captioned(Symbol(bOn ? TEXT("pause") : TEXT("play")).SymbolReplace(EVekaSymbolReplace::Replace).Frame(48, 40), TEXT("Replace")),
				Captioned(Symbol(bOn ? TEXT("bell.slash") : TEXT("bell")).SymbolReplace(EVekaSymbolReplace::MagicReplace, 0.45f).Frame(48, 40), TEXT("Magic replace")),
				Captioned(Symbol(bOn ? TEXT("heart.fill") : TEXT("heart")).SymbolRenderingMode(EVekaSymbolRenderingMode::Multicolor)
					.SymbolReplace(EVekaSymbolReplace::MagicReplace).Frame(48, 40), TEXT("Fill (magic)")),
				Captioned(Symbol(bOn ? TEXT("speaker") : TEXT("speaker.wave.3")).SymbolReplace(EVekaSymbolReplace::Replace).Frame(48, 40), TEXT("Different symbol"))
			)
			.Font(EVekaTextStyle::Title1)
			.Padding(8)
			.Background(FLinearColor(1, 1, 1, 0.04f), 8)
			.OnTap([Toggled]() { Toggled.Set(!Toggled.Get()); });
		}
	};

	FVekaElement SymbolsSection()
	{
		using namespace Veka;

		// Gallery of the built-in library.
		const TArray<FName> Names = Veka::Symbols::GetBuiltInNames();
		TArray<FVekaElement> Gallery;
		for (const FName Name : Names)
		{
			Gallery.Add(Symbol(Name).FlexFrame(-1, Veka::Infinity, 34, 34));
		}

		// Weights follow the font.
		const TCHAR* WeightNames[] = { TEXT("Hairline"), TEXT("Thin"), TEXT("ExtraLight"), TEXT("Light"), TEXT("News"), TEXT("Regular"),
			TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold"), TEXT("ExtraBold"), TEXT("Black"), TEXT("ExtraBlack") };
		TArray<FVekaElement> Weights;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Weights.Add(VStack(4)(
				HStack(3)(Symbol(TEXT("gearshape")), Text(TEXT("Ag"))).Font(EVekaTextStyle::Title2).FontWeight(static_cast<EVekaFontWeight>(Index)),
				Caption(WeightNames[Index])));
		}

		const TCHAR* Variants[] = { TEXT("bell"), TEXT("bell.fill"), TEXT("bell.slash"), TEXT("bell.slash.fill"), TEXT("checkmark.circle"),
			TEXT("checkmark.circle.fill"), TEXT("heart.square"), TEXT("heart.square.fill"), TEXT("envelope.fill"), TEXT("folder.fill"), TEXT("person.circle.fill") };
		TArray<FVekaElement> VariantTiles;
		for (const TCHAR* Variant : Variants)
		{
			VariantTiles.Add(Captioned(Symbol(Variant).Font(EVekaTextStyle::Title1), Variant));
		}

		const TPair<EVekaSymbolRenderingMode, const TCHAR*> Modes[] = {
			{ EVekaSymbolRenderingMode::Monochrome, TEXT("Monochrome") }, { EVekaSymbolRenderingMode::Hierarchical, TEXT("Hierarchical") },
			{ EVekaSymbolRenderingMode::Palette, TEXT("Palette") }, { EVekaSymbolRenderingMode::Multicolor, TEXT("Multicolor") } };
		TArray<FVekaElement> ModeColumns;
		for (const TPair<EVekaSymbolRenderingMode, const TCHAR*>& Mode : Modes)
		{
			FVekaElement Row = HStack(12)(
				Symbol(TEXT("exclamationmark.triangle")), Symbol(TEXT("cloud.rain")), Symbol(TEXT("sun")), Symbol(TEXT("person.2")), Symbol(TEXT("paintpalette"))
			).Font(EVekaTextStyle::Title1);
			Row = Mode.Key == EVekaSymbolRenderingMode::Palette
				? MoveTemp(Row).SymbolPalette({ FVekaPaint::FromToken(TEXT("Orange")), FVekaPaint::FromToken(TEXT("Teal")), FVekaPaint::FromToken(TEXT("Purple")) })
				: MoveTemp(Row).SymbolRenderingMode(Mode.Key);
			ModeColumns.Add(Captioned(MoveTemp(Row), Mode.Value));
		}

		TArray<FVekaElement> Aliases;
		TArray<FName> AliasNames;
		Veka::Symbols::GetAlpha13Aliases().GenerateKeyArray(AliasNames);
		for (const FName Alias : AliasNames)
		{
			Aliases.Add(Captioned(Symbol(Alias).Font(EVekaTextStyle::Title2).SymbolRenderingMode(EVekaSymbolRenderingMode::Hierarchical), *Alias.ToString()));
		}

		return VStack(18, EVekaHAlign::Leading)(
			Text(FText::Format(INVTEXT("{0} built-in symbols (stroke skeletons on a 24 grid: any weight, no masters)"), FText::AsNumber(Names.Num())))
				.Font(EVekaTextStyle::Caption1).Foreground(Muted),
			Grid({ FVekaGridItem::Adaptive(34.f) }, 6.f, 6.f).Children(MoveTemp(Gallery)).Font(EVekaTextStyle::Title3),
			HStack(14).Children(MoveTemp(Weights)),
			HStack(28, EVekaVAlign::Bottom)(
				Captioned(HStack(4)(Symbol(TEXT("cloud")).SymbolScale(EVekaSymbolScale::Small), Text(TEXT("Small"))), TEXT("SymbolScale")),
				HStack(4)(Symbol(TEXT("cloud")), Text(TEXT("Medium"))),
				HStack(4)(Symbol(TEXT("cloud")).SymbolScale(EVekaSymbolScale::Large), Text(TEXT("Large"))),
				Label(INVTEXT("Label: symbol + title"), TEXT("paperplane")).Foreground(Accent),
				Captioned(HStack(18)(Symbol(TEXT("bell")).SymbolBadge(3), Symbol(TEXT("envelope")).SymbolBadge(128), Symbol(TEXT("bubble")).SymbolBadge(-1))
					.Font(EVekaTextStyle::Title1), TEXT("Badges"))
			).Font(EVekaTextStyle::Title3),
			HStack(16).Children(MoveTemp(VariantTiles)),
			HStack(30).Children(MoveTemp(ModeColumns)),
			HStack(30, EVekaVAlign::Top)(View<FSymbolValueView>(), View<FSymbolReplaceView>()),
			View<FSymbolEffectsView>(),
			Caption(TEXT("Alpha 13 glyphs (names kept as aliases):")),
			HStack(14).Children(MoveTemp(Aliases))
		);
	}

	// --- Controls ----------------------------------------------------------------------------------------------------

	FVekaElement Column(const TCHAR* Title, FVekaElement Content)
	{
		using namespace Veka;
		return VStack(10, EVekaHAlign::Leading)(
			Text(Title).Font(EVekaTextStyle::Caption1).FontWeight(EVekaFontWeight::SemiBold).Foreground(Muted),
			MoveTemp(Content)
		);
	}

	class FControlsView : public FVekaView
	{
		VEKA_VIEW(FControlsView)

	public:
		virtual FVekaElement Body(FVekaContext& Ctx) const override
		{
			using namespace Veka;
			const TVekaState<int32> Clicks = Ctx.State<int32>(TEXT("Clicks"), 0);
			const TVekaState<bool> WiFi = Ctx.State<bool>(TEXT("WiFi"), true);
			const TVekaState<bool> Bluetooth = Ctx.State<bool>(TEXT("Bluetooth"), false);
			const TVekaState<bool> Bold = Ctx.State<bool>(TEXT("Bold"), true);
			const TVekaState<bool> Disabled = Ctx.State<bool>(TEXT("Disabled"), false);
			const TVekaState<float> Volume = Ctx.State<float>(TEXT("Volume"), 0.6f);
			const TVekaState<float> Rating = Ctx.State<float>(TEXT("Rating"), 0.5f);
			const TVekaState<float> Count = Ctx.State<float>(TEXT("Count"), 3.f);
			const TVekaState<int32> View = Ctx.State<int32>(TEXT("View"), 1);
			const TVekaState<int32> Size = Ctx.State<int32>(TEXT("Size"), 0);
			const TVekaState<int32> Align = Ctx.State<int32>(TEXT("Align"), 0);
			const TVekaState<int32> Swatch = Ctx.State<int32>(TEXT("Swatch"), 2);
			const TVekaState<int32> Sort = Ctx.State<int32>(TEXT("Sort"), 0);
			const TVekaState<FKey> Jump = Ctx.State<FKey>(TEXT("Jump"), EKeys::SpaceBar);
			const TVekaState<FString> Name = Ctx.State<FString>(TEXT("Name"), FString());
			const TVekaState<FString> Password = Ctx.State<FString>(TEXT("Password"), FString());
			const TVekaState<double> Price = Ctx.State<double>(TEXT("Price"), 1499.9);
			const TVekaState<double> Share = Ctx.State<double>(TEXT("Share"), 0.35);
			const TVekaState<FLinearColor> Color = Ctx.State<FLinearColor>(TEXT("Color"), FLinearColor(FColor(255, 94, 87)));
			const TVekaState<FString> LastAction = Ctx.State<FString>(TEXT("LastAction"), FString(TEXT("none")));

			auto Log = [LastAction](const TCHAR* Action) { return [LastAction, Value = FString(Action)]() { LastAction.Set(Value); }; };

			const FVekaElement Buttons = Column(TEXT("BUTTONS"), VStack(10, EVekaHAlign::Leading)(
				HStack(10)(
					Veka::Button(INVTEXT("Plain"), [Clicks]() { Clicks.Set(Clicks.Get() + 1); }).ButtonStyle(EVekaButtonStyleKind::Plain),
					Veka::Button(INVTEXT("Borderless"), [Clicks]() { Clicks.Set(Clicks.Get() + 1); }).ButtonStyle(EVekaButtonStyleKind::Borderless),
					Veka::Button(INVTEXT("Bordered"), [Clicks]() { Clicks.Set(Clicks.Get() + 1); }),
					Veka::Button(INVTEXT("Prominent"), TEXT("paperplane"), [Clicks]() { Clicks.Set(Clicks.Get() + 1); }).ButtonStyle(EVekaButtonStyleKind::BorderedProminent)
				),
				HStack(10)(
					Veka::Button(INVTEXT("Delete"), TEXT("trash"), [Clicks]() { Clicks.Set(0); }, EVekaButtonRole::Destructive),
					Veka::Button(INVTEXT("Delete"), [Clicks]() { Clicks.Set(0); }, EVekaButtonRole::Destructive).ButtonStyle(EVekaButtonStyleKind::BorderedProminent),
					Veka::Button(INVTEXT("Disabled"), []() {}).Disabled(),
					Text(FText::Format(INVTEXT("{0} clicks"), FText::AsNumber(Clicks.Get()))).Foreground(Muted).MonospacedDigit()
				),
				HStack(8, EVekaVAlign::Center)(
					Veka::Button(INVTEXT("Mini"), []() {}).ControlSize(EVekaControlSize::Mini),
					Veka::Button(INVTEXT("Small"), []() {}).ControlSize(EVekaControlSize::Small),
					Veka::Button(INVTEXT("Regular"), []() {}),
					Veka::Button(INVTEXT("Large"), []() {}).ControlSize(EVekaControlSize::Large)
				).ButtonStyle(EVekaButtonStyleKind::BorderedProminent),
				HStack(10)(Text(TEXT("Jump key")), KeyBindButton(Jump))
			));

			const FVekaElement Toggles = Column(TEXT("TOGGLES"), VStack(10, EVekaHAlign::Leading)(
				Toggle(Label(INVTEXT("Wi-Fi"), TEXT("wifi")), WiFi).Frame(240, -1),
				Toggle(Label(INVTEXT("Bluetooth"), TEXT("bluetooth")), Bluetooth).Frame(240, -1),
				Toggle(INVTEXT("Checkbox"), Bold).ToggleStyle(EVekaToggleStyleKind::Checkbox),
				HStack(8)(
					Toggle(Symbol(TEXT("textformat.size")).Frame(16, 16), Bold).ToggleStyle(EVekaToggleStyleKind::Button),
					Toggle(INVTEXT("Disable all"), Disabled).ToggleStyle(EVekaToggleStyleKind::Button)
				)
			));

			const FVekaElement Values = Column(TEXT("SLIDERS, STEPPER, GAUGES, PROGRESS"), VStack(10, EVekaHAlign::Leading)(
				HStack(10)(Symbol(TEXT("speaker")), Slider(Volume).Frame(200, -1), Symbol(TEXT("speaker.wave.3"), Volume.Get()),
					Text(FText::AsPercent(Volume.Get())).MonospacedDigit().Foreground(Muted).Frame(44, 18, FVekaAlignment::Trailing())),
				HStack(10)(Text(TEXT("Steps of 0.25")).Foreground(Muted), Slider(Rating, 0.f, 1.f, 0.25f).Frame(160, -1)),
				Stepper(INVTEXT("Players"), Count, 1.f, 8.f).Frame(300, -1),
				HStack(22, EVekaVAlign::Top)(
					Gauge(Volume.Get(), 0.f, 1.f, INVTEXT("Linear"), FText::GetEmpty(), INVTEXT("0"), INVTEXT("100")).Frame(120, -1),
					Gauge(Volume.Get(), 0.f, 1.f, INVTEXT("Linear alt."), FText::GetEmpty(), INVTEXT("0"), INVTEXT("100")).GaugeStyle(EVekaGaugeStyle::LinearAlt).Frame(120, -1),
					Gauge(Volume.Get(), 0.f, 1.f, INVTEXT("Circular")).GaugeStyle(EVekaGaugeStyle::Circular),
					Gauge(Volume.Get(), 0.f, 1.f, INVTEXT("Circular alt."), FText::AsNumber(FMath::RoundToInt32(Volume.Get() * 100.f)), INVTEXT("0"), INVTEXT("100"))
						.GaugeStyle(EVekaGaugeStyle::CircularAlt)
				),
				HStack(22)(
					ProgressView(Volume.Get(), INVTEXT("Downloading")).Frame(140, -1),
					ProgressView(-1.f, INVTEXT("Waiting")).Frame(140, -1),
					ProgressView(Volume.Get()).ProgressViewStyle(EVekaProgressStyle::Circular),
					ProgressView().ProgressViewStyle(EVekaProgressStyle::Circular),
					ProgressView(-1.f, INVTEXT("Lazy")).ProgressViewStyle(EVekaProgressStyle::Lazy)
				)
			));

			const TArray<FVekaPickerOption> Views = { FVekaPickerOption(INVTEXT("List"), TEXT("list.bullet")), FVekaPickerOption(INVTEXT("Grid"), TEXT("square.grid.2x2")),
				FVekaPickerOption(INVTEXT("Gallery"), TEXT("photo")) };
			const TArray<FVekaPickerOption> Sizes = { FVekaPickerOption(INVTEXT("Small")), FVekaPickerOption(INVTEXT("Medium")), FVekaPickerOption(INVTEXT("Large")) };
			const TArray<FVekaPickerOption> Aligns = { FVekaPickerOption(FText::GetEmpty(), TEXT("text.alignleft")), FVekaPickerOption(FText::GetEmpty(), TEXT("text.aligncenter")),
				FVekaPickerOption(FText::GetEmpty(), TEXT("text.alignright")) };
			const TArray<FVekaPickerOption> Swatches = { FVekaPickerOption::Swatch(Hex(TEXT("#FF5E57FF"))), FVekaPickerOption::Swatch(Hex(TEXT("#FFA84CFF"))),
				FVekaPickerOption::Swatch(Hex(TEXT("#5CD65CFF"))), FVekaPickerOption::Swatch(Hex(TEXT("#4C7DFFFF"))), FVekaPickerOption::Swatch(Hex(TEXT("#8E5CFFFF"))) };

			const TArray<FVekaMenuItem> FileMenu = {
				FVekaMenuItem::Button(INVTEXT("New"), Log(TEXT("New")), TEXT("doc"), INVTEXT("Ctrl+N")),
				FVekaMenuItem::Button(INVTEXT("Open..."), Log(TEXT("Open")), TEXT("folder"), INVTEXT("Ctrl+O")),
				FVekaMenuItem::SubMenu(INVTEXT("Share"), {
					FVekaMenuItem::Button(INVTEXT("Mail"), Log(TEXT("Mail")), TEXT("envelope")),
					FVekaMenuItem::Button(INVTEXT("Messages"), Log(TEXT("Messages")), TEXT("bubble")) }, TEXT("square.and.arrow.up")),
				FVekaMenuItem::Divider(),
				FVekaMenuItem::Picker(INVTEXT("Sort by"), Sort, { FVekaPickerOption(INVTEXT("Name")), FVekaPickerOption(INVTEXT("Date")), FVekaPickerOption(INVTEXT("Size")) }),
				FVekaMenuItem::Divider(),
				FVekaMenuItem::Button(INVTEXT("Archive"), Log(TEXT("Archive")), TEXT("archivebox")).Enabled(false),
				FVekaMenuItem::Button(INVTEXT("Delete"), Log(TEXT("Delete")), TEXT("trash")).Destructive() };

			const FVekaElement Choices = Column(TEXT("PICKERS, MENUS"), VStack(12, EVekaHAlign::Leading)(
				Picker(INVTEXT("View"), View, Views).PickerStyle(EVekaPickerStyle::Segmented).Frame(400, -1),
				HStack(30, EVekaVAlign::Top)(
					Picker(INVTEXT("Size"), Size, Sizes).PickerStyle(EVekaPickerStyle::Radio),
					VStack(10, EVekaHAlign::Leading)(
						Picker(FText::GetEmpty(), Align, Aligns, true).PickerStyle(EVekaPickerStyle::Radio, EVekaAxis::Horizontal),
						Picker(FText::GetEmpty(), Swatch, Swatches).PickerStyle(EVekaPickerStyle::Palette),
						Picker(INVTEXT("Size"), Size, Sizes).Frame(200, -1)
					)
				),
				HStack(12)(
					Menu(INVTEXT("File"), FileMenu),
					Text(INVTEXT("Right-click me")).Padding(FMargin(14, 8)).Background(FLinearColor(1, 1, 1, 0.06f), 8).ContextMenu(FileMenu),
					Text(FText::Format(INVTEXT("Last: {0}"), FText::FromString(LastAction.Get()))).Foreground(Muted)
				)
			));

			const FVekaElement Fields = Column(TEXT("TEXT FIELDS"), VStack(8, EVekaHAlign::Leading)(
				HStack(8)(
					TextField(INVTEXT("Plain"), Name).TextFieldStyle(EVekaTextFieldStyle::Plain).Frame(120, -1),
					TextField(INVTEXT("Rectangle"), Name).TextFieldStyle(EVekaTextFieldStyle::Rectangle).Frame(120, -1),
					TextField(INVTEXT("Rounded"), Name).Frame(120, -1),
					TextField(INVTEXT("Capsule"), Name).TextFieldStyle(EVekaTextFieldStyle::Capsule).Frame(120, -1)
				),
				HStack(8)(
					SecureField(INVTEXT("Password"), Password).Frame(150, -1),
					NumberField(INVTEXT("Price"), Price, EVekaTextFieldType::Currency).Frame(120, -1),
					NumberField(INVTEXT("Share"), Share, EVekaTextFieldType::Percent).Frame(90, -1),
					Text(Name.Get().IsEmpty() ? INVTEXT("Type a name") : FText::Format(INVTEXT("Hello {0}"), FText::FromString(Name.Get()))).Foreground(Muted)
				)
			));

			const FVekaElement Colors = Column(TEXT("COLOR"), HStack(24, EVekaVAlign::Top)(
				VStack(10, EVekaHAlign::Leading)(
					ColorPicker(INVTEXT("Accent"), Color).Frame(200, -1),
					RoundedRectangle(12).Fill(Color.Get()).Frame(200, 60)
				),
				ColorPanel(Color, EVekaColorPickerMode::HSV),
				ColorPanel(Color, EVekaColorPickerMode::OkLCH, false, EVekaColorChannelRange::Unit)
			));

			const FVekaElement Misc = Column(TEXT("LABELS, DIVIDER, EMPTY STATE, MEDIA"), HStack(24, EVekaVAlign::Top)(
				VStack(8, EVekaHAlign::Leading)(
					Label(INVTEXT("Title and icon"), TEXT("star")),
					Label(INVTEXT("Title only"), TEXT("star")).LabelStyle(EVekaLabelStyle::Title),
					HStack(8)(Label(INVTEXT("Icon"), TEXT("star")).LabelStyle(EVekaLabelStyle::Icon), Divider().Frame(1, 16), Text(TEXT("icon only")).Foreground(Muted)),
					Divider().Frame(180, 1)
				),
				ContentUnavailable(TEXT("magnifyingglass"), INVTEXT("No results"), INVTEXT("Try another search."), Veka::Button(INVTEXT("Clear search"), []() {}))
					.Background(FLinearColor(1, 1, 1, 0.04f), 12),
				MediaView(nullptr, nullptr).Frame(220, 150)
			));

			// "Disable all" disables every control but its own toggle column.
			const bool bOff = Disabled.Get();
			return VStack(22, EVekaHAlign::Leading)(
				HStack(40, EVekaVAlign::Top)(Buttons.Disabled(bOff), Toggles),
				Values.Disabled(bOff),
				HStack(40, EVekaVAlign::Top)(Choices.Disabled(bOff), Fields.Disabled(bOff)),
				Colors.Disabled(bOff),
				Misc
			);
		}
	};

	FVekaElement ControlsSection()
	{
		return Veka::View<FControlsView>();
	}

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
				Text(INVTEXT("VekaUI - Playground (Phase 1 + Colora + Anima + Typography + Symbols + Controls)")).Font(Fonts::LargeTitle()),
				Text(INVTEXT("Declarative Slate-first views: one Stack for H/V/Z, modifiers, state, springs, vector shapes and strokes. "
					"This paragraph wraps to the available width."))
					.Foreground(Muted),

				Section(INVTEXT("Controls: buttons, toggles, sliders, stepper, pickers, menus, text fields, color, gauges, progress (Tab / arrows / Enter work)"), ControlsSection()),

				Section(INVTEXT("Symbols: library, weights, scales, variants, rendering modes, variable value, effects, replace"), SymbolsSection()),

				Section(INVTEXT("Typography: LT Flode Neue, styles, weights, truncation, numeric text, glyph effects"), TypographySection()),

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
