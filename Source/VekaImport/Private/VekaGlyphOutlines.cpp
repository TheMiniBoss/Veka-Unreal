// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaGlyphOutlines.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
THIRD_PARTY_INCLUDES_END

namespace
{
	struct FDecomposeContext
	{
		FVekaPath* Path = nullptr;
		double Scale = 1.0;
		FVector2D Last = FVector2D::ZeroVector;
	};

	FVector2D ToPoint(const FT_Vector* Vector, double Scale)
	{
		// Font units are y up; Veka is y down.
		return FVector2D(Vector->x * Scale, -Vector->y * Scale);
	}

	int MoveTo(const FT_Vector* To, void* User)
	{
		FDecomposeContext& Ctx = *static_cast<FDecomposeContext*>(User);
		if (!Ctx.Path->IsEmpty())
		{
			Ctx.Path->Close();
		}
		Ctx.Last = ToPoint(To, Ctx.Scale);
		Ctx.Path->MoveTo(Ctx.Last);
		return 0;
	}

	int LineTo(const FT_Vector* To, void* User)
	{
		FDecomposeContext& Ctx = *static_cast<FDecomposeContext*>(User);
		Ctx.Last = ToPoint(To, Ctx.Scale);
		Ctx.Path->LineTo(Ctx.Last);
		return 0;
	}

	int ConicTo(const FT_Vector* Control, const FT_Vector* To, void* User)
	{
		FDecomposeContext& Ctx = *static_cast<FDecomposeContext*>(User);
		Ctx.Last = ToPoint(To, Ctx.Scale);
		Ctx.Path->QuadTo(ToPoint(Control, Ctx.Scale), Ctx.Last);
		return 0;
	}

	int CubicTo(const FT_Vector* Control1, const FT_Vector* Control2, const FT_Vector* To, void* User)
	{
		FDecomposeContext& Ctx = *static_cast<FDecomposeContext*>(User);
		Ctx.Last = ToPoint(To, Ctx.Scale);
		Ctx.Path->CubicTo(ToPoint(Control1, Ctx.Scale), ToPoint(Control2, Ctx.Scale), Ctx.Last);
		return 0;
	}

	FT_Library GetLibrary()
	{
		static FT_Library Library = []()
		{
			FT_Library Created = nullptr;
			FT_Init_FreeType(&Created);
			return Created;
		}();
		return Library;
	}
}

namespace Veka::Import
{
	FString GetDefaultFontDirectory()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VekaUI"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Fonts/LTFlodeNeue")) : FString();
	}

	bool LoadGlyphOutline(const FString& FontFile, TCHAR Character, FVekaPath& OutPath, FVekaGlyphOutlineInfo& OutInfo)
	{
		FT_Library Library = GetLibrary();
		TArray<uint8> Bytes;
		if (!Library || !FFileHelper::LoadFileToArray(Bytes, *FontFile))
		{
			return false;
		}

		FT_Face Face = nullptr;
		if (FT_New_Memory_Face(Library, Bytes.GetData(), Bytes.Num(), 0, &Face) != 0)
		{
			return false;
		}

		bool bOk = false;
		if (FT_Load_Char(Face, Character, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0 && Face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
		{
			const FT_Outline& Outline = Face->glyph->outline;
			OutInfo.Contours = Outline.n_contours;
			OutInfo.Points = Outline.n_points;
			OutInfo.PointsPerContour.Reset();
			int32 Start = 0;
			for (int32 Contour = 0; Contour < Outline.n_contours; ++Contour)
			{
				const int32 End = Outline.contours[Contour];
				OutInfo.PointsPerContour.Add(End - Start + 1);
				Start = End + 1;
			}
			OutInfo.Advance = static_cast<int32>(Face->glyph->metrics.horiAdvance);
			OutInfo.UnitsPerEm = Face->units_per_EM;

			OutPath = FVekaPath();
			FDecomposeContext Context;
			Context.Path = &OutPath;
			Context.Scale = 1.0;
			FT_Outline_Funcs Funcs;
			Funcs.move_to = &MoveTo;
			Funcs.line_to = &LineTo;
			Funcs.conic_to = &ConicTo;
			Funcs.cubic_to = &CubicTo;
			Funcs.shift = 0;
			Funcs.delta = 0;
			FT_Outline OutlineCopy = Outline;
			bOk = FT_Outline_Decompose(&OutlineCopy, &Funcs, &Context) == 0;
			if (bOk && !OutPath.IsEmpty())
			{
				OutPath.Close();
			}
			OutPath.FillRule = (Outline.flags & FT_OUTLINE_EVEN_ODD_FILL) ? EVekaFillRule::EvenOdd : EVekaFillRule::NonZero;
		}
		FT_Done_Face(Face);
		return bOk;
	}
}
