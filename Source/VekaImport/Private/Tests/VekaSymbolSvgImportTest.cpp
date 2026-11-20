// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaSymbolSvgImport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaSymbolSvgImportTest, "VekaUI.Symbols.SvgImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaSymbolSvgImportTest::RunTest(const FString& Parameters)
{
	const FString Svg = TEXT(
		"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 48 48\">"
		"<circle class=\"secondary\" cx=\"24\" cy=\"24\" r=\"20\" fill=\"none\" stroke=\"#000\"/>"
		"<path d=\"M14 24 L22 32 L34 16\" fill=\"none\"/>"
		"<rect x=\"20\" y=\"4\" width=\"8\" height=\"4\" fill=\"#000\"/>"
		"<title>ignored</title>"
		"</svg>");

	FVekaSymbolDefinition Definition;
	FString Error;
	if (!TestTrue(TEXT("Import"), Veka::Import::ImportSymbolSvg(Svg, TEXT("imported.check"), Definition, &Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Three shapes"), Definition.Shapes.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Role from class"), Definition.Shapes[0].Role, EVekaSymbolRole::Secondary);
	TestEqual(TEXT("Default role"), Definition.Shapes[1].Role, EVekaSymbolRole::Primary);
	TestEqual(TEXT("Unfilled -> stroke"), Definition.Shapes[1].Kind, EVekaSymbolShapeKind::Stroke);
	TestEqual(TEXT("Filled -> fill"), Definition.Shapes[2].Kind, EVekaSymbolShapeKind::Fill);

	// The 48 viewBox maps to the 24 grid: the circle is centered at 12 with radius 10.
	TArray<FVekaPolyline> Circle;
	Definition.Shapes[0].Path.Flatten(Circle, 0.05);
	bool bOnCircle = !Circle.IsEmpty();
	for (const FVekaPolyline& Polyline : Circle)
	{
		for (const FVector2D& Point : Polyline.Points)
		{
			bOnCircle &= FMath::IsNearlyEqual(FVector2D::Distance(Point, FVector2D(12.0, 12.0)), 10.0, 0.1);
		}
	}
	TestTrue(TEXT("Scaled to the grid"), bOnCircle);

	FVekaSymbolDefinition Empty;
	TestFalse(TEXT("No <svg>"), Veka::Import::ImportSymbolSvg(TEXT("<html/>"), TEXT("x"), Empty));
	TestFalse(TEXT("Nothing drawable"), Veka::Import::ImportSymbolSvg(TEXT("<svg viewBox=\"0 0 24 24\"><title>x</title></svg>"), TEXT("x"), Empty));
	return true;
}

#endif
