// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vector/VekaPath.h"

/** Outline statistics of one glyph (interpolation needs the same structure in every master). */
struct FVekaGlyphOutlineInfo
{
	int32 Contours = 0;
	int32 Points = 0;
	/** Per-contour on/off point counts, to compare structures. */
	TArray<int32> PointsPerContour;
	/** Advance width in font units. */
	int32 Advance = 0;
	int32 UnitsPerEm = 1000;

	bool IsCompatibleWith(const FVekaGlyphOutlineInfo& Other) const { return Contours == Other.Contours && PointsPerContour == Other.PointsPerContour; }
};

namespace Veka::Import
{
	/**
	 * Loads a glyph's outline from an OTF / TTF file with FreeType (unscaled font units, y up converted to y down,
	 * so 1 em = UnitsPerEm). Used for vector text and to build symbols from fonts.
	 */
	VEKAIMPORT_API bool LoadGlyphOutline(const FString& FontFile, TCHAR Character, FVekaPath& OutPath, FVekaGlyphOutlineInfo& OutInfo);

	/** Directory of the built-in LT Flode Neue files. */
	VEKAIMPORT_API FString GetDefaultFontDirectory();
}
