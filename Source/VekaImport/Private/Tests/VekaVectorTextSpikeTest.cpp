// Copyright TheMiniBoss. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaGlyphOutlines.h"
#include "Misc/Paths.h"

/**
 * Spike 2c: can LT Flode Neue's weights be interpolated (vector text with a continuous weight)?
 * Interpolation needs every weight to have the same outline structure for a glyph (same contours, same points).
 * The test logs a compatibility table; it fails only if the fonts cannot be read.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaVectorTextSpikeTest, "VekaUI.Spike.VectorText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaVectorTextSpikeTest::RunTest(const FString& Parameters)
{
	const FString Directory = Veka::Import::GetDefaultFontDirectory();
	const TCHAR* Weights[] = { TEXT("Hairline"), TEXT("Thin"), TEXT("ExtraLight"), TEXT("Light"), TEXT("News"), TEXT("Regular"),
		TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold"), TEXT("ExtraBold"), TEXT("Black"), TEXT("ExtraBlack") };
	const FString Glyphs = TEXT("AaBbCcegkmnoRSsty0123456789&@?%");

	// Outline of every glyph in every weight.
	TArray<TArray<FVekaGlyphOutlineInfo>> Infos;
	for (const TCHAR* Weight : Weights)
	{
		const FString File = FPaths::Combine(Directory, FString::Printf(TEXT("LTFlodeNeue-%s.otf"), Weight));
		TArray<FVekaGlyphOutlineInfo>& Row = Infos.AddDefaulted_GetRef();
		for (const TCHAR Glyph : Glyphs)
		{
			FVekaPath Path;
			FVekaGlyphOutlineInfo Info;
			if (!TestTrue(FString::Printf(TEXT("Load '%c' from %s"), Glyph, Weight), Veka::Import::LoadGlyphOutline(File, Glyph, Path, Info)))
			{
				return false;
			}
			TestFalse(FString::Printf(TEXT("Path of '%c' (%s)"), Glyph, Weight), Path.IsEmpty());
			Row.Add(Info);
		}
	}

	// Compatibility of each weight with its neighbours and with Regular.
	const int32 RegularIndex = 5;
	int32 CompatibleWithRegular = 0;
	int32 CompatibleNeighbours = 0;
	FString Table = TEXT("Glyph |");
	for (const TCHAR* Weight : Weights)
	{
		Table += FString::Printf(TEXT(" %s |"), Weight);
	}
	Table += TEXT("\n");
	for (int32 GlyphIndex = 0; GlyphIndex < Glyphs.Len(); ++GlyphIndex)
	{
		Table += FString::Printf(TEXT("%c |"), Glyphs[GlyphIndex]);
		for (int32 WeightIndex = 0; WeightIndex < UE_ARRAY_COUNT(Weights); ++WeightIndex)
		{
			const FVekaGlyphOutlineInfo& Info = Infos[WeightIndex][GlyphIndex];
			const bool bSameAsRegular = Info.IsCompatibleWith(Infos[RegularIndex][GlyphIndex]);
			CompatibleWithRegular += bSameAsRegular ? 1 : 0;
			if (WeightIndex > 0)
			{
				CompatibleNeighbours += Info.IsCompatibleWith(Infos[WeightIndex - 1][GlyphIndex]) ? 1 : 0;
			}
			Table += FString::Printf(TEXT(" %d/%d%s |"), Info.Contours, Info.Points, bSameAsRegular ? TEXT("") : TEXT("*"));
		}
		Table += TEXT("\n");
	}

	const int32 Total = Glyphs.Len() * UE_ARRAY_COUNT(Weights);
	const int32 NeighbourPairs = Glyphs.Len() * (UE_ARRAY_COUNT(Weights) - 1);
	const FString Summary = FString::Printf(TEXT("VekaSpike: %d/%d glyph outlines match Regular; %d/%d adjacent-weight pairs match (contours/points; * = differs)."),
		CompatibleWithRegular, Total, CompatibleNeighbours, NeighbourPairs);
	UE_LOG(LogTemp, Display, TEXT("%s\n%s"), *Summary, *Table);
	AddInfo(Summary);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
