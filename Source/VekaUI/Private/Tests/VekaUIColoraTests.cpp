// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaUI.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags VekaColoraTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	bool ColorNear(const FLinearColor& A, const FLinearColor& B, float Tolerance = 2.0e-3f)
	{
		return FMath::Abs(A.R - B.R) <= Tolerance && FMath::Abs(A.G - B.G) <= Tolerance && FMath::Abs(A.B - B.B) <= Tolerance && FMath::Abs(A.A - B.A) <= Tolerance;
	}

	FVekaEnvironment EnvWith(EVekaColorScheme Scheme, const UVekaTheme* Theme = nullptr)
	{
		FVekaEnvironment Env = VekaEnv::MakeDefault();
		Env.Set<EVekaColorScheme>(VekaEnv::ColorScheme(), Scheme);
		Env.Set<const UVekaTheme*>(VekaEnv::Theme(), Theme);
		return Env;
	}

	const FLinearColor Red(1.f, 0.f, 0.f, 1.f);
	const FLinearColor Blue(0.f, 0.f, 1.f, 1.f);
	const FLinearColor Green(0.f, 1.f, 0.f, 1.f);
	const FLinearColor White(1.f, 1.f, 1.f, 1.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraOkLabTest, "VekaUI.Colora.OkLab", VekaColoraTestFlags)
bool FVekaColoraOkLabTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Colora;

	// Reference values from https://bottosson.github.io/posts/oklab/ (linear sRGB primaries).
	const FVekaOkLab LabRed = LinearToOkLab(Red);
	TestEqual(TEXT("Red L"), LabRed.L, 0.627955f, 1.0e-3f);
	TestEqual(TEXT("Red a"), LabRed.A, 0.224863f, 1.0e-3f);
	TestEqual(TEXT("Red b"), LabRed.B, 0.125846f, 1.0e-3f);

	const FVekaOkLab LabGreen = LinearToOkLab(Green);
	TestEqual(TEXT("Green L"), LabGreen.L, 0.866440f, 1.0e-3f);
	TestEqual(TEXT("Green a"), LabGreen.A, -0.233888f, 1.0e-3f);

	const FVekaOkLab LabBlue = LinearToOkLab(Blue);
	TestEqual(TEXT("Blue b"), LabBlue.B, -0.311528f, 1.0e-3f);

	const FVekaOkLab LabWhite = LinearToOkLab(White);
	TestEqual(TEXT("White L"), LabWhite.L, 1.f, 1.0e-3f);
	TestEqual(TEXT("White is neutral"), FMath::Abs(LabWhite.A) + FMath::Abs(LabWhite.B), 0.f, 1.0e-3f);

	const FLinearColor Sample(0.2f, 0.5f, 0.8f, 1.f);
	TestTrue(TEXT("Oklab round trip"), ColorNear(OkLabToLinear(LinearToOkLab(Sample)), Sample));
	TestTrue(TEXT("OkLCH round trip"), ColorNear(OkLabToLinear(OkLCHToOkLab(OkLabToOkLCH(LinearToOkLab(Sample)))), Sample));
	TestEqual(TEXT("sRGB round trip"), SRGBToLinear(LinearToSRGB(0.3f)), 0.3f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraInterpolationTest, "VekaUI.Colora.Interpolation", VekaColoraTestFlags)
bool FVekaColoraInterpolationTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Colora;

	TestTrue(TEXT("T = 0 gives A"), ColorNear(Lerp(Red, Blue, 0.f, EVekaColorSpace::OkLab), Red));
	TestTrue(TEXT("T = 1 gives B"), ColorNear(Lerp(Red, Blue, 1.f, EVekaColorSpace::OkLCH), Blue));

	// OkLCH keeps chroma at the midpoint; sRGB / Oklab pass closer to gray.
	const float ChromaLCH = OkLabToOkLCH(LinearToOkLab(Lerp(Red, Blue, 0.5f, EVekaColorSpace::OkLCH))).C;
	const float ChromaLab = OkLabToOkLCH(LinearToOkLab(Lerp(Red, Blue, 0.5f, EVekaColorSpace::OkLab))).C;
	TestTrue(TEXT("OkLCH midpoint is more saturated than Oklab"), ChromaLCH > ChromaLab + 0.02f);

	// Hue takes the shortest way (350 -> 10 goes through 0, not 180).
	const FVector4f A(0.5f, 0.1f, 350.f, 1.f);
	const FVector4f B(0.5f, 0.1f, 10.f, 1.f);
	const float Hue = LerpInSpace(A, B, 0.5f, EVekaColorSpace::OkLCH).Z;
	TestTrue(TEXT("Shortest hue path"), Hue < 1.f || Hue > 359.f);

	// Premultiplied: fading to transparent keeps the color (no dark fringe).
	const FLinearColor Half = Lerp(Red, FLinearColor(0.f, 0.f, 0.f, 0.f), 0.5f, EVekaColorSpace::Linear);
	TestTrue(TEXT("Fade keeps the hue"), ColorNear(Half, FLinearColor(1.f, 0.f, 0.f, 0.5f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraMatrixTest, "VekaUI.Colora.ColorMatrix", VekaColoraTestFlags)
bool FVekaColoraMatrixTest::RunTest(const FString& Parameters)
{
	const FLinearColor Sample(0.6f, 0.2f, 0.05f, 1.f);

	const FLinearColor Gray = FVekaColorMatrix::Make(EVekaColorAdjust::Saturation, 0.f).Apply(Sample);
	TestTrue(TEXT("Saturation 0 is gray"), FMath::IsNearlyEqual(Gray.R, Gray.G, 1.0e-4f) && FMath::IsNearlyEqual(Gray.G, Gray.B, 1.0e-4f));
	TestTrue(TEXT("Grayscale 1 == saturation 0"), ColorNear(FVekaColorMatrix::Make(EVekaColorAdjust::Grayscale, 1.f).Apply(Sample), Gray));

	TestTrue(TEXT("Hue rotation 360 is identity"), ColorNear(FVekaColorMatrix::Make(EVekaColorAdjust::HueRotation, 360.f).Apply(Sample), Sample, 1.0e-3f));
	TestTrue(TEXT("Saturation 1 is identity"), FVekaColorMatrix::Make(EVekaColorAdjust::Saturation, 1.f).IsIdentity());

	const FLinearColor Brighter = FVekaColorMatrix::Make(EVekaColorAdjust::Brightness, 0.2f).Apply(FLinearColor::Black);
	TestEqual(TEXT("Brightness adds in sRGB"), Veka::Colora::LinearToSRGB(Brighter.R), 0.2f, 1.0e-3f);

	const FVekaColorMatrix Inner = FVekaColorMatrix::Make(EVekaColorAdjust::Brightness, 0.1f);
	const FVekaColorMatrix Outer = FVekaColorMatrix::Make(EVekaColorAdjust::Contrast, 1.5f);
	TestTrue(TEXT("Then = inner, then outer"), ColorNear(Inner.Then(Outer).Apply(Sample), Outer.Apply(Inner.Apply(Sample)), 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraTokensTest, "VekaUI.Colora.Tokens", VekaColoraTestFlags)
bool FVekaColoraTokensTest::RunTest(const FString& Parameters)
{
	using namespace Veka::Colora;

	TestEqual(TEXT("26 built-in tokens"), DefaultTokenNames().Num(), 26);

	const FLinearColor AccentDark = ResolveColor(TEXT("Accent"), EnvWith(EVekaColorScheme::Dark));
	const FLinearColor AccentLight = ResolveColor(TEXT("Accent"), EnvWith(EVekaColorScheme::Light));
	TestFalse(TEXT("Accent depends on the scheme"), ColorNear(AccentDark, AccentLight));
	TestEqual(TEXT("Secondary hierarchy = 60 %"), ResolveColor(TEXT("Accent"), EnvWith(EVekaColorScheme::Dark), EVekaHierarchy::Secondary).A, 0.6f, 1.0e-4f);

	// Theme chain: child overrides Accent, inherits Brand from its parent, falls back to built-ins for the rest.
	UVekaTheme* Parent = NewObject<UVekaTheme>(GetTransientPackage());
	Parent->Colors.Add(TEXT("Brand"), FVekaThemeColor(Red, Green));
	Parent->Metrics.Add(TEXT("Radius.Card"), 18.f);
	UVekaTheme* Child = NewObject<UVekaTheme>(GetTransientPackage());
	Child->Parent = Parent;
	Child->Colors.Add(TEXT("Accent"), FVekaThemeColor(Blue, Blue));
	// A token can be a gradient, and can refer to another token.
	Child->Colors.Add(TEXT("Hero"), FVekaThemeColor(FVekaPaint::Linear({ FVekaGradientStop::FromToken(TEXT("Brand"), 0.f), FVekaGradientStop(White, 1.f) }), FVekaPaint::FromToken(TEXT("Brand"))));

	const FVekaEnvironment LightEnv = EnvWith(EVekaColorScheme::Light, Child);
	const FVekaEnvironment DarkEnv = EnvWith(EVekaColorScheme::Dark, Child);
	TestTrue(TEXT("Child override"), ColorNear(ResolveColor(TEXT("Accent"), LightEnv), Blue));
	TestTrue(TEXT("Inherited from parent (light)"), ColorNear(ResolveColor(TEXT("Brand"), LightEnv), Red));
	TestTrue(TEXT("Inherited from parent (dark)"), ColorNear(ResolveColor(TEXT("Brand"), DarkEnv), Green));
	TestFalse(TEXT("Built-in fallback"), ColorNear(ResolveColor(TEXT("Orange"), LightEnv), FLinearColor(1.f, 0.f, 1.f, 1.f)));
	TestEqual(TEXT("Metric from parent"), Metric(TEXT("Radius.Card"), LightEnv, 4.f), 18.f, 1.0e-4f);
	TestEqual(TEXT("Metric default"), Metric(TEXT("Radius.Unknown"), LightEnv, 4.f), 4.f, 1.0e-4f);

	const FVekaPaint Hero = Resolve(FVekaPaint::FromToken(TEXT("Hero"), EVekaHierarchy::Tertiary), LightEnv);
	if (TestEqual(TEXT("Gradient token"), Hero.Type, EVekaPaintType::LinearGradient) && TestEqual(TEXT("Two stops"), Hero.Stops.Num(), 2))
	{
		TestTrue(TEXT("Stop token resolved"), Hero.Stops[0].Token.IsNone() && ColorNear(FLinearColor(Hero.Stops[0].Color.R, Hero.Stops[0].Color.G, Hero.Stops[0].Color.B, 1.f), Red));
		TestEqual(TEXT("Hierarchy applied to stops"), Hero.Stops[1].Color.A, 0.3f, 1.0e-4f);
	}
	TestTrue(TEXT("Token to token (dark)"), ColorNear(Resolve(FVekaPaint::FromToken(TEXT("Hero")), DarkEnv).Color, Green));

	AddExpectedError(TEXT("unknown Colora token"), EAutomationExpectedErrorFlags::Contains, 0);
	TestTrue(TEXT("Unknown token is magenta"), ColorNear(ResolveColor(TEXT("NoSuchToken_Test"), LightEnv), FLinearColor(1.f, 0.f, 1.f, 1.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraGradientTest, "VekaUI.Colora.GradientSampling", VekaColoraTestFlags)
bool FVekaColoraGradientTest::RunTest(const FString& Parameters)
{
	using Veka::Colora::FPaintSampler;
	const FVekaRect Bounds(FVector2D(10.0, 20.0), FVector2D(200.0, 100.0));

	const FPaintSampler LinearSampler(FVekaPaint::Linear(Red, Blue), Bounds);
	TestTrue(TEXT("Linear start"), ColorNear(LinearSampler.Sample(FVector2D(10.0, 70.0)), Red));
	TestTrue(TEXT("Linear end"), ColorNear(LinearSampler.Sample(FVector2D(210.0, 70.0)), Blue));
	TestTrue(TEXT("Linear clamps before start"), ColorNear(LinearSampler.Sample(FVector2D(0.0, 70.0)), Red));
	TestEqual(TEXT("Linear midpoint t"), LinearSampler.GetGradientT(FVector2D(110.0, 30.0)), 0.5f, 1.0e-4f);

	const FPaintSampler Diagonal(FVekaPaint::Linear(Red, Blue, FVector2D(0.0, 0.0), FVector2D(1.0, 1.0)), Bounds);
	TestEqual(TEXT("Diagonal center t"), Diagonal.GetGradientT(Bounds.Center()), 0.5f, 1.0e-4f);

	const FPaintSampler RadialSampler(FVekaPaint::Radial({ FVekaGradientStop(Red, 0.f), FVekaGradientStop(Blue, 1.f) }, 10.f, 50.f), Bounds);
	TestEqual(TEXT("Radial inner radius"), RadialSampler.GetGradientT(Bounds.Center() + FVector2D(10.0, 0.0)), 0.f, 1.0e-4f);
	TestEqual(TEXT("Radial middle"), RadialSampler.GetGradientT(Bounds.Center() + FVector2D(0.0, 30.0)), 0.5f, 1.0e-4f);

	const FPaintSampler Elliptic(FVekaPaint::Elliptical({ FVekaGradientStop(Red, 0.f), FVekaGradientStop(Blue, 1.f) }), Bounds);
	TestEqual(TEXT("Elliptical reaches 1 on the horizontal edge"), Elliptic.GetGradientT(FVector2D(210.0, 70.0)), 1.f, 1.0e-4f);
	TestEqual(TEXT("Elliptical reaches 1 on the vertical edge"), Elliptic.GetGradientT(FVector2D(110.0, 20.0)), 1.f, 1.0e-4f);

	const FPaintSampler AngularSampler(FVekaPaint::Angular({ FVekaGradientStop(Red, 0.f), FVekaGradientStop(Blue, 1.f) }), Bounds);
	TestEqual(TEXT("Angular starts on +X"), AngularSampler.GetGradientT(Bounds.Center() + FVector2D(40.0, 0.0)), 0.f, 1.0e-4f);
	TestEqual(TEXT("Angular is clockwise (down = 90 deg)"), AngularSampler.GetGradientT(Bounds.Center() + FVector2D(0.0, 40.0)), 0.25f, 1.0e-4f);

	const FPaintSampler Partial(FVekaPaint::Angular({ FVekaGradientStop(Red, 0.f), FVekaGradientStop(Blue, 1.f) }, 0.f, 90.f), Bounds);
	TestEqual(TEXT("Partial sweep clamps near the end"), Partial.GetGradientT(Bounds.Center() + FVector2D(-10.0, 40.0)), 1.f, 1.0e-4f);
	TestEqual(TEXT("Partial sweep clamps near the start"), Partial.GetGradientT(Bounds.Center() + FVector2D(40.0, -10.0)), 0.f, 1.0e-4f);

	// Three stops: the middle one is hit exactly.
	const FPaintSampler ThreeStops(FVekaPaint::Linear({ FVekaGradientStop(Red, 0.f), FVekaGradientStop(Green, 0.25f), FVekaGradientStop(Blue, 1.f) }), Bounds);
	TestTrue(TEXT("Middle stop"), ColorNear(ThreeStops.Sample(FVector2D(60.0, 70.0)), Green));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraMeshTest, "VekaUI.Colora.MeshGradient", VekaColoraTestFlags)
bool FVekaColoraMeshTest::RunTest(const FString& Parameters)
{
	using Veka::Colora::FPaintSampler;
	const FVekaRect Bounds(FVector2D::ZeroVector, FVector2D(100.0, 100.0));

	const FVekaMeshGradient Quad = FVekaMeshGradient::Grid(2, 2, { Red, Green, Blue, White }, false);
	const FPaintSampler QuadSampler(FVekaPaint::FromMesh(Quad), Bounds);
	TestTrue(TEXT("Corner TL"), ColorNear(QuadSampler.Sample(FVector2D(0.0, 0.0)), Red));
	TestTrue(TEXT("Corner TR"), ColorNear(QuadSampler.Sample(FVector2D(100.0, 0.0)), Green));
	TestTrue(TEXT("Corner BL"), ColorNear(QuadSampler.Sample(FVector2D(0.0, 100.0)), Blue));
	TestTrue(TEXT("Corner BR"), ColorNear(QuadSampler.Sample(FVector2D(100.0, 100.0)), White));

	// 3x3 grid, smooth, with the center point moved: sampling at the moved position gives the center color.
	TArray<FLinearColor> Colors;
	Colors.Init(Blue, 9);
	Colors[4] = Red;
	FVekaMeshGradient Deformed = FVekaMeshGradient::Grid(3, 3, Colors, true);
	Deformed.At(1, 1).Position = FVector2D(0.7, 0.35);
	const FPaintSampler DeformedSampler(FVekaPaint::FromMesh(Deformed), Bounds);
	TestTrue(TEXT("Moved point keeps its color"), ColorNear(DeformedSampler.Sample(FVector2D(70.0, 35.0)), Red, 1.0e-2f));
	TestTrue(TEXT("Corners unaffected"), ColorNear(DeformedSampler.Sample(FVector2D(0.0, 100.0)), Blue, 1.0e-2f));

	// Tokens in mesh points are resolved.
	FVekaMeshGradient Tokens = FVekaMeshGradient::Grid(2, 2, { Red, Red, Red, Red });
	Tokens.At(0, 0).Token = TEXT("White");
	const FVekaPaint Resolved = Veka::Colora::Resolve(FVekaPaint::FromMesh(Tokens), EnvWith(EVekaColorScheme::Dark));
	TestTrue(TEXT("Mesh token resolved"), ColorNear(Resolved.Mesh.At(0, 0).Color, White));
	return true;
}

namespace
{
	FLinearColor ForegroundOf(const TSharedPtr<FVekaNode>& Node)
	{
		return VekaEnv::GetForegroundColor(Node->GetEnvironment());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraModifiersTest, "VekaUI.Colora.Modifiers", VekaColoraTestFlags)
bool FVekaColoraModifiersTest::RunTest(const FString& Parameters)
{
	using namespace Veka;

	// Token foreground follows the scheme set above it.
	TSharedRef<FVekaRuntime> Runtime = MakeShared<FVekaRuntime>();
	Runtime->SetRoot(VStack()(Text(TEXT("A")).Foreground(FVekaPaint::FromToken(TEXT("Accent")))).ColorScheme(EVekaColorScheme::Light));
	const TSharedPtr<FVekaNode> TextNode = Runtime->GetRootNode()->GetChildren()[0];
	FLinearColor ExpectedLight;
	Veka::Colora::GetDefaultColor(TEXT("Accent"), EVekaColorScheme::Light, ExpectedLight);
	TestTrue(TEXT("Token foreground in light scheme"), ColorNear(ForegroundOf(TextNode), ExpectedLight));

	// Inner modifiers override outer ones on the same node (SwiftUI order).
	Runtime->SetRoot(Text(TEXT("B")).Foreground(Red).Foreground(Blue));
	TestTrue(TEXT("Inner foreground wins"), ColorNear(ForegroundOf(Runtime->GetRootNode()), Red));

	// Adjustments compose: inner saturation 0, then outer brightness.
	Runtime->SetRoot(VStack()(Text(TEXT("C")).Saturation(0.f)).Brightness(0.1f));
	const FVekaColorMatrix* Matrix = VekaEnv::GetColorMatrix(Runtime->GetRootNode()->GetChildren()[0]->GetEnvironment());
	if (TestNotNull(TEXT("Matrix in environment"), Matrix))
	{
		const FVekaColorMatrix Expected = FVekaColorMatrix::Make(EVekaColorAdjust::Saturation, 0.f).Then(FVekaColorMatrix::Make(EVekaColorAdjust::Brightness, 0.1f));
		TestTrue(TEXT("Saturation then brightness"), *Matrix == Expected);
	}

	Runtime->SetRoot(Text(TEXT("D")).BlendMode(EVekaBlendMode::Add));
	TestEqual(TEXT("Blend mode in environment"), VekaEnv::GetBlendMode(Runtime->GetRootNode()->GetEnvironment()), EVekaBlendMode::Add);
	return true;
}

namespace
{
	struct FVekaColoraPaintCapture
	{
		TSharedRef<SWindow> Window;
		FSlateWindowElementList Elements;
		FHittestGrid HittestGrid;

		FVekaColoraPaintCapture(const TSharedRef<SWidget>& Widget, const FVector2f& Size)
			: Window(SNew(SWindow).ClientSize(Size).CreateTitleBar(false))
			, Elements(Window)
		{
			HittestGrid.SetHittestArea(FVector2f::ZeroVector, Size);
			const FPaintArgs Args(nullptr, HittestGrid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
			Widget->SlatePrepass(1.f);
			Widget->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0.f, 0.f, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
		}

		const FSlateDrawElementArray<FSlateCustomVertsElement>& CustomVerts() const
		{
			return Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_CustomVerts>();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaColoraRenderTest, "VekaUI.Colora.Render", VekaColoraTestFlags)
bool FVekaColoraRenderTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		AddInfo(TEXT("No Slate renderer: skipped."));
		return true;
	}

	using namespace Veka;
	TSharedRef<SVekaHost> Host = SNew(SVekaHost).Root(Rectangle().Fill(FVekaPaint::Linear(Red, Blue).InSpace(EVekaColorSpace::Linear)).Frame(200, 100));
	const FVekaColoraPaintCapture Capture(Host, FVector2f(200.f, 100.f));
	if (!TestEqual(TEXT("One tessellated gradient"), Capture.CustomVerts().Num(), 1))
	{
		return false;
	}

	const FSlateCustomVertsElement& Element = Capture.CustomVerts()[0];
	TestTrue(TEXT("Grid subdivision (more than a quad)"), Element.Vertices.Num() > 20);
	const FSlateVertex* Left = nullptr;
	const FSlateVertex* Right = nullptr;
	for (const FSlateVertex& Vertex : Element.Vertices)
	{
		if (Vertex.Color.A < 255)
		{
			continue; // Antialiasing fringe.
		}
		if (!Left || Vertex.Position.X < Left->Position.X)
		{
			Left = &Vertex;
		}
		if (!Right || Vertex.Position.X > Right->Position.X)
		{
			Right = &Vertex;
		}
	}
	if (TestNotNull(TEXT("Left vertex"), Left) && TestNotNull(TEXT("Right vertex"), Right))
	{
		TestTrue(TEXT("Left is red"), Left->Color.R > 240 && Left->Color.B < 15);
		TestTrue(TEXT("Right is blue"), Right->Color.B > 240 && Right->Color.R < 15);
	}
	TestTrue(TEXT("Antialiasing fringe present"), Element.Vertices.ContainsByPredicate([](const FSlateVertex& Vertex) { return Vertex.Color.A == 0; }));

	// Additive: premultiplied colors with zero alpha, premultiplied blend state.
	TSharedRef<SVekaHost> AddHost = SNew(SVekaHost).Root(Circle().Fill(Red).Frame(50, 50).BlendMode(EVekaBlendMode::Add));
	const FVekaColoraPaintCapture AddCapture(AddHost, FVector2f(50.f, 50.f));
	if (TestEqual(TEXT("Additive circle tessellated"), AddCapture.CustomVerts().Num(), 1))
	{
		const FSlateCustomVertsElement& AddElement = AddCapture.CustomVerts()[0];
		TestTrue(TEXT("Premultiplied blend"), EnumHasAnyFlags(AddElement.GetDrawEffects(), ESlateDrawEffect::PreMultipliedAlpha));
		TestTrue(TEXT("Zero alpha"), !AddElement.Vertices.ContainsByPredicate([](const FSlateVertex& Vertex) { return Vertex.Color.A != 0; }));
		TestTrue(TEXT("Red light"), AddElement.Vertices.ContainsByPredicate([](const FSlateVertex& Vertex) { return Vertex.Color.R > 240; }));
	}

	// Adjustments reach the vertex colors: saturation 0 turns the gradient gray.
	TSharedRef<SVekaHost> GrayHost = SNew(SVekaHost).Root(Rectangle().Fill(FVekaPaint::Linear(Red, Blue)).Frame(100, 40).Saturation(0.f));
	const FVekaColoraPaintCapture GrayCapture(GrayHost, FVector2f(100.f, 40.f));
	if (TestEqual(TEXT("Gray gradient tessellated"), GrayCapture.CustomVerts().Num(), 1))
	{
		TestFalse(TEXT("All vertices gray"), GrayCapture.CustomVerts()[0].Vertices.ContainsByPredicate([](const FSlateVertex& Vertex)
		{
			return FMath::Abs(Vertex.Color.R - Vertex.Color.G) > 2 || FMath::Abs(Vertex.Color.G - Vertex.Color.B) > 2;
		}));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
