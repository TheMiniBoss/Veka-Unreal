// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPaint.h"

class UVekaTheme;

/**
 * Migration of the Alpha 13 (Blueprint) themes to UVekaTheme. The Alpha 13 content is read from a copy of the old
 * plugin's Content folder (by default Saved/VekaAlpha13/Content, exported read-only from the Blueprint branch with
 * `git archive origin/Blueprint Content`), mounted at /Veka/:
 *  - Colora_System (Veka_ColoraPreset): every Colora preset, light and dark -> the theme's tokens (Accent, Blue...);
 *  - Veka_Default (Veka_Themes) and its sub-themes (Toggle, Slider, Menu, Picker): the colors of each control -> the
 *    control tokens (ToggleAccent, SliderBackground...), as a preset token, a color or a gradient;
 *  - slider sizes -> metrics (Slider.Width...).
 * Glyphs, badges, fonts and shapes of the Alpha 13 themes have no theme slot in VekaUI (symbols and fonts are set
 * by the view): they are listed in the report.
 */
namespace Veka::Editor::Alpha13
{
	struct FReport
	{
		TArray<FString> Lines;
		int32 ColorTokens = 0;
		int32 ControlTokens = 0;
		int32 Metrics = 0;
		void Add(const FString& Line);
	};

	/** Mounts the Alpha 13 content at /Veka/ and scans it. */
	bool Mount(const FString& ContentDir);
	FString DefaultContentDir();
	void Dump(const FString& ContentDir);

	/** "Light_2_1C5CAC2F46DA51ACA06E0089A90FD3C8" -> "Light" (Blueprint struct field names). */
	FString CleanFieldName(const FString& Name);
	/** Alpha 13 preset name -> Veka token ("Accent Color" -> Accent, "WindowBakground" -> WindowBackground). None: manual color. */
	FName TokenForPreset(const FString& PresetName);
	/** Alpha 13 theme slot -> control token ("ToggleAccentColor" in Toggle -> ToggleAccent, "TextColor" -> ThemeText). */
	FName TokenForSlot(const FString& SubTheme, const FString& Slot);
	EVekaHierarchy HierarchyFromName(const FString& Name);

	/**
	 * Reads the mounted Alpha 13 themes and writes them into a new UVekaTheme PackagePath/AssetName (replacing it).
	 * bSave writes the package to disk. Returns nullptr if the Alpha 13 assets cannot be loaded.
	 */
	UVekaTheme* MigrateThemes(const FString& PackagePath, const FString& AssetName, bool bSave, FReport& Report);
}
