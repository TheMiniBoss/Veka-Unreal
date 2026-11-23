// Copyright TheMiniBoss. All Rights Reserved.

#include "Migration/VekaAlpha13Migration.h"
#include "Colora/VekaTheme.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogVekaMigration, Log, All);

namespace Veka::Editor::Alpha13
{
	namespace
	{
		const TCHAR* MountRoot = TEXT("/Veka/");

		/** A struct value read by reflection: the Blueprint structs are only known by their (cleaned) field names. */
		struct FView
		{
			const UStruct* Struct = nullptr;
			const void* Memory = nullptr;

			bool IsValid() const { return Struct && Memory; }

			const FProperty* Find(const TCHAR* Name) const
			{
				if (!IsValid())
				{
					return nullptr;
				}
				for (TFieldIterator<FProperty> It(Struct); It; ++It)
				{
					if (CleanFieldName(It->GetName()) == Name)
					{
						return *It;
					}
				}
				return nullptr;
			}

			FView Child(const TCHAR* Name) const
			{
				const FStructProperty* Property = CastField<FStructProperty>(Find(Name));
				return Property ? FView{ Property->Struct, Property->ContainerPtrToValuePtr<void>(Memory) } : FView();
			}

			bool Color(const TCHAR* Name, FLinearColor& Out) const
			{
				const FStructProperty* Property = CastField<FStructProperty>(Find(Name));
				if (!Property || Property->Struct != TBaseStructure<FLinearColor>::Get())
				{
					return false;
				}
				Out = *Property->ContainerPtrToValuePtr<FLinearColor>(Memory);
				return true;
			}

			int32 Int(const TCHAR* Name, int32 Default) const
			{
				const FNumericProperty* Property = CastField<FNumericProperty>(Find(Name));
				return Property && Property->IsInteger() ? static_cast<int32>(Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Memory))) : Default;
			}

			/** Display name of an enum field ("" if there is none). */
			FString Enum(const TCHAR* Name) const
			{
				return EnumOf(Find(Name));
			}

			FString EnumOf(const FProperty* Property) const
			{
				const UEnum* Type = nullptr;
				int64 Value = 0;
				if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
				{
					Type = Byte->Enum;
					Value = Byte->GetPropertyValue_InContainer(Memory);
				}
				else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
				{
					Type = EnumProperty->GetEnum();
					Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Memory));
				}
				return Type ? Type->GetDisplayNameTextByValue(Value).ToString() : FString();
			}

			/** First enum field whose name contains Part ("Hierarch"). */
			FString EnumContaining(const TCHAR* Part) const
			{
				if (!IsValid())
				{
					return FString();
				}
				for (TFieldIterator<FProperty> It(Struct); It; ++It)
				{
					if (CleanFieldName(It->GetName()).Contains(Part) && (CastField<FByteProperty>(*It) || CastField<FEnumProperty>(*It)))
					{
						return EnumOf(*It);
					}
				}
				return FString();
			}

			/** First bool field ("Override?"); Default if there is none. */
			bool FirstBool(bool Default) const
			{
				if (!IsValid())
				{
					return Default;
				}
				for (TFieldIterator<FBoolProperty> It(Struct); It; ++It)
				{
					return It->GetPropertyValue_InContainer(Memory);
				}
				return Default;
			}
		};

		FView ViewOf(const UObject* Object, const FProperty* Property)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			return StructProperty ? FView{ StructProperty->Struct, StructProperty->ContainerPtrToValuePtr<void>(Object) } : FView();
		}

		bool IsBlueprintProperty(const FProperty* Property)
		{
			return Property->GetOwnerClass() && !Property->GetOwnerClass()->IsNative();
		}

		const UObject* LoadAlpha13(const TCHAR* Path)
		{
			const FString Name = FPackageName::GetShortName(Path);
			return LoadObject<UObject>(nullptr, *(FString(Path) + TEXT(".") + Name));
		}

		FString Describe(const FVekaPaint& Paint)
		{
			switch (Paint.Type)
			{
			case EVekaPaintType::Token:
				return FString::Printf(TEXT(".%s%s"), *Paint.Token.ToString(), Paint.Hierarchy == EVekaHierarchy::Primary ? TEXT("")
					: *FString::Printf(TEXT(" (%s)"), *StaticEnum<EVekaHierarchy>()->GetNameStringByValue(static_cast<int64>(Paint.Hierarchy))));
			case EVekaPaintType::Solid:
				return TEXT("#") + Paint.Color.ToFColorSRGB().ToHex();
			default:
				return FString::Printf(TEXT("%s gradient, %d stops"), *StaticEnum<EVekaPaintType>()->GetNameStringByValue(static_cast<int64>(Paint.Type)), Paint.Stops.Num());
			}
		}

		/** One stop of an Alpha 13 Colora setting: a preset (token, hierarchy) or a manual light / dark color. */
		void ReadStop(const FView& Position, bool bDark, FName& OutToken, EVekaHierarchy& OutHierarchy, FLinearColor& OutColor)
		{
			// S_VekaColoraColorAndHierarchical: a preset and its hierarchy, unless a manual color is switched on.
			const FView Color = Position.Child(TEXT("Color"));
			OutToken = TokenForPreset(Color.Enum(TEXT("ColorPreset")));
			OutHierarchy = HierarchyFromName(Color.EnumContaining(TEXT("Hierachical")));
			OutColor = FLinearColor::Transparent;
			for (const TCHAR* ManualName : { TEXT("ManualColor"), TEXT("ManualHierarchical") })
			{
				const FView Manual = Color.Child(ManualName);
				if (Manual.FirstBool(false) && Manual.Child(TEXT("ColoraColorDarkThemes")).Color(bDark ? TEXT("Dark") : TEXT("Light"), OutColor))
				{
					OutToken = NAME_None;
					return;
				}
			}
		}

		/** An Alpha 13 Colora setting (S_VekaColoraSettings) for one scheme. */
		FVekaPaint ReadSettings(const FView& Settings, bool bDark)
		{
			const FView Stops = Settings.Child(TEXT("Stops"));
			const int32 Count = FMath::Clamp(Stops.Int(TEXT("NumberOfStops"), 1), 1, 9);
			// ColoraMode: Color (the first stop), Gradient (every stop) or Mix (the first two, mixed).
			const FString Mode = Settings.Enum(TEXT("ColoraMode"));
			const FString Type = Settings.Child(TEXT("Gradient")).Enum(TEXT("GradientType"));
			const bool bRadial = Type.Contains(TEXT("Radial"));
			const bool bAngular = Type.Contains(TEXT("Angular"));
			const bool bGradient = Mode == TEXT("Gradient") && Count >= 2;
			const bool bMix = Mode == TEXT("Mix") && Count >= 2;

			auto StopPaint = [&](int32 Index, FVekaGradientStop* OutStop) -> FVekaPaint
			{
				FName Token;
				EVekaHierarchy Hierarchy;
				FLinearColor Color;
				ReadStop(Stops.Child(*FString::Printf(TEXT("Pos%d"), Index + 1)), bDark, Token, Hierarchy, Color);
				const float Location = Count > 1 ? static_cast<float>(Index) / (Count - 1) : 0.f;
				if (OutStop)
				{
					*OutStop = Token.IsNone() ? FVekaGradientStop(Color, Location) : FVekaGradientStop::FromToken(Token, Location, Hierarchy);
				}
				return Token.IsNone() ? FVekaPaint(Color) : FVekaPaint::FromToken(Token, Hierarchy);
			};

			if (bMix)
			{
				FVekaGradientStop A;
				FVekaGradientStop B;
				const FVekaPaint First = StopPaint(0, &A);
				StopPaint(1, &B);
				const FNumericProperty* MixProperty = CastField<FNumericProperty>(Settings.Find(TEXT("Mix")));
				const float Amount = MixProperty ? static_cast<float>(MixProperty->GetFloatingPointPropertyValue(MixProperty->ContainerPtrToValuePtr<void>(Settings.Memory))) : 0.5f;
				if (A.Token.IsNone() && B.Token.IsNone())
				{
					return FVekaPaint(FMath::Lerp(A.Color, B.Color, Amount));
				}
				return Amount < 0.5f ? First : StopPaint(1, nullptr);
			}
			if (!bGradient)
			{
				return StopPaint(0, nullptr);
			}
			TArray<FVekaGradientStop> GradientStops;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				StopPaint(Index, &GradientStops.AddDefaulted_GetRef());
			}
			if (bRadial)
			{
				return FVekaPaint::Radial(MoveTemp(GradientStops), 0.f, 0.5f);
			}
			if (bAngular)
			{
				return FVekaPaint::Angular(MoveTemp(GradientStops));
			}
			return FVekaPaint::Linear(MoveTemp(GradientStops));
		}

		/** A Colora setting, or an overridable one (nullptr view when it is not overridden: the control token keeps its default). */
		FView SettingsOf(const FView& Value, bool& bOverridden)
		{
			bOverridden = true;
			const FView Inner = Value.Child(TEXT("ColoraSettings"));
			if (Inner.IsValid())
			{
				bOverridden = Value.FirstBool(true);
				return Inner;
			}
			if (!Value.Child(TEXT("Stops")).IsValid())
			{
				return FView();
			}
			// A plain setting left at the struct's defaults was never set by the theme's author.
			const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Value.Struct);
			const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Value.Struct);
			if (UserStruct && ScriptStruct && UserStruct->GetDefaultInstance() && ScriptStruct->CompareScriptStruct(Value.Memory, UserStruct->GetDefaultInstance(), PPF_None))
			{
				bOverridden = false;
			}
			return Value;
		}
	}

	void FReport::Add(const FString& Line)
	{
		Lines.Add(Line);
		UE_LOG(LogVekaMigration, Display, TEXT("%s"), *Line);
	}

	FString CleanFieldName(const FString& Name)
	{
		// UserDefinedStruct fields: <Name>_<Index>_<32 hex GUID>.
		TArray<FString> Parts;
		Name.ParseIntoArray(Parts, TEXT("_"), false);
		if (Parts.Num() >= 3 && Parts.Last().Len() == 32 && Parts[Parts.Num() - 2].IsNumeric())
		{
			const bool bHex = Parts.Last().Find(TEXT("G")) == INDEX_NONE && FCString::Strspn(*Parts.Last(), TEXT("0123456789ABCDEFabcdef")) == 32;
			if (bHex)
			{
				return Name.Left(Name.Len() - Parts.Last().Len() - Parts[Parts.Num() - 2].Len() - 2);
			}
		}
		return Name;
	}

	FName TokenForPreset(const FString& PresetName)
	{
		FString Name = PresetName.Replace(TEXT(" "), TEXT(""));
		if (Name.IsEmpty() || Name.Contains(TEXT("Manual")) || Name.Contains(TEXT("Custom")) || Name == TEXT("None"))
		{
			return NAME_None;
		}
		static const TMap<FString, FString> Renamed = {
			{ TEXT("AccentColor"), TEXT("Accent") }, { TEXT("WindowBakground"), TEXT("WindowBackground") }, { TEXT("Quarternary"), TEXT("Quaternary") },
		};
		if (const FString* Found = Renamed.Find(Name))
		{
			Name = *Found;
		}
		return FName(*Name);
	}

	FName TokenForSlot(const FString& SubTheme, const FString& Slot)
	{
		FString Name = Slot;
		Name.RemoveFromEnd(TEXT("Color"));
		if (!SubTheme.IsEmpty())
		{
			// ToggleAccent in Toggle -> ToggleAccent; Knob in Slider -> SliderKnob.
			Name.RemoveFromStart(SubTheme);
			return FName(*(SubTheme + Name));
		}
		static const TMap<FString, FString> Root = {
			{ TEXT("Accent"), TEXT("ThemeAccent") }, { TEXT("Background"), TEXT("ThemeBackground") }, { TEXT("Text"), TEXT("ThemeText") },
			{ TEXT("Button"), TEXT("ButtonBackground") },
		};
		if (const FString* Found = Root.Find(Name))
		{
			return FName(**Found);
		}
		// GaugeAccent, ProgressBackground: already a control token name.
		for (const TCHAR* Part : { TEXT("Accent"), TEXT("Background"), TEXT("Text") })
		{
			if (Name.EndsWith(Part) && Name.Len() > FCString::Strlen(Part))
			{
				return FName(*Name);
			}
		}
		return FName(*(TEXT("Theme") + Name));
	}

	EVekaHierarchy HierarchyFromName(const FString& Name)
	{
		if (Name.Contains(TEXT("Secondary"))) return EVekaHierarchy::Secondary;
		if (Name.Contains(TEXT("Tertiary"))) return EVekaHierarchy::Tertiary;
		if (Name.Contains(TEXT("Quaternary")) || Name.Contains(TEXT("Quarternary"))) return EVekaHierarchy::Quaternary;
		if (Name.Contains(TEXT("Transparent"))) return EVekaHierarchy::Transparent;
		return EVekaHierarchy::Primary;
	}

	bool Mount(const FString& ContentDir)
	{
		FString Dir = FPaths::ConvertRelativePathToFull(ContentDir);
		FPaths::NormalizeDirectoryName(Dir);
		if (!FPaths::DirectoryExists(Dir))
		{
			UE_LOG(LogVekaMigration, Error, TEXT("Alpha 13 content not found: %s (export it with: git archive origin/Blueprint Content | tar -x -C Saved/VekaAlpha13)"), *Dir);
			return false;
		}
		if (!FPackageName::MountPointExists(MountRoot))
		{
			FPackageName::RegisterMountPoint(MountRoot, Dir / TEXT(""));
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ FString(MountRoot) }, true);
		return true;
	}

	FString DefaultContentDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("VekaAlpha13/Content");
	}

	void Dump(const FString& ContentDir)
	{
		if (!Mount(ContentDir))
		{
			return;
		}
		const TCHAR* Assets[] = {
			TEXT("/Veka/Colora/Colora_System"), TEXT("/Veka/Themes/Veka_Default"), TEXT("/Veka/Themes/Toggle/Toggle_Default"),
			TEXT("/Veka/Themes/Sliders/Slider_Default"), TEXT("/Veka/Themes/Menu/Menu_Default"), TEXT("/Veka/Themes/Picker/Picker_Default"),
		};
		for (const TCHAR* Path : Assets)
		{
			const UObject* Object = LoadAlpha13(Path);
			if (!Object)
			{
				UE_LOG(LogVekaMigration, Warning, TEXT("Could not load %s"), Path);
				continue;
			}
			UE_LOG(LogVekaMigration, Display, TEXT("== %s (%s)"), *Object->GetPathName(), *Object->GetClass()->GetPathName());
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				if (IsBlueprintProperty(*It))
				{
					FString Value;
					It->ExportTextItem_InContainer(Value, Object, nullptr, nullptr, PPF_None);
					UE_LOG(LogVekaMigration, Display, TEXT("  %s [%s] = %s"), *CleanFieldName(It->GetName()), *It->GetCPPType(), *Value);
				}
			}
		}
		for (const TCHAR* StructPath : { TEXT("/Veka/Colora/Struct/S_VekaColoraSettings.S_VekaColoraSettings"), TEXT("/Veka/Colora/Struct/S_VekaColoraStops.S_VekaColoraStops"),
			TEXT("/Veka/Colora/Struct/S_VekaColoraStopPoint.S_VekaColoraStopPoint"), TEXT("/Veka/Colora/Struct/S_VekaColoraColorAndHierarchical.S_VekaColoraColorAndHierarchical") })
		{
			if (const UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, StructPath))
			{
				TArray<FString> Fields;
				for (TFieldIterator<FProperty> It(Struct); It; ++It)
				{
					FString Default;
					if (const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct))
					{
						It->ExportTextItem_InContainer(Default, UserStruct->GetDefaultInstance(), nullptr, nullptr, PPF_None);
					}
					Fields.Add(FString::Printf(TEXT("%s [%s] = %s"), *CleanFieldName(It->GetName()), *It->GetCPPType(), *Default.Left(120)));
				}
				UE_LOG(LogVekaMigration, Display, TEXT("struct %s: %s"), *Struct->GetName(), *FString::Join(Fields, TEXT(" | ")));
			}
		}
		for (const TCHAR* EnumPath : { TEXT("/Veka/Colora/Enums/E_VekaColoraColorPreset.E_VekaColoraColorPreset"), TEXT("/Veka/Colora/Enums/E_VekaColoraGradientType.E_VekaColoraGradientType"),
			TEXT("/Veka/Colora/Enums/E_VekaColoraHIerarchicalPreset.E_VekaColoraHIerarchicalPreset"), TEXT("/Veka/Colora/Enums/E_VekaColoraMode.E_VekaColoraMode") })
		{
			if (const UEnum* Enum = LoadObject<UEnum>(nullptr, EnumPath))
			{
				TArray<FString> Names;
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
				{
					Names.Add(FString::Printf(TEXT("%s=%s"), *Enum->GetNameStringByIndex(Index), *Enum->GetDisplayNameTextByIndex(Index).ToString()));
				}
				UE_LOG(LogVekaMigration, Display, TEXT("enum %s: %s"), *Enum->GetName(), *FString::Join(Names, TEXT(", ")));
			}
		}
	}

	UVekaTheme* MigrateThemes(const FString& PackagePath, const FString& AssetName, bool bSave, FReport& Report)
	{
		const UObject* System = LoadAlpha13(TEXT("/Veka/Colora/Colora_System"));
		const UObject* Themes = LoadAlpha13(TEXT("/Veka/Themes/Veka_Default"));
		if (!System || !Themes)
		{
			Report.Add(TEXT("error: the Alpha 13 assets Colora_System and Veka_Default could not be loaded (Mount first)"));
			return nullptr;
		}

		const FString PackageName = PackagePath / AssetName;
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();
		if (UVekaTheme* Old = FindObject<UVekaTheme>(Package, *AssetName))
		{
			Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Old->ClearFlags(RF_Public | RF_Standalone);
			Old->MarkAsGarbage();
		}
		UVekaTheme* Theme = NewObject<UVekaTheme>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

		// Colora presets: one token each, light and dark.
		Report.Add(FString::Printf(TEXT("Colora presets (%s):"), *System->GetPathName()));
		for (TFieldIterator<FProperty> It(System->GetClass()); It; ++It)
		{
			if (!IsBlueprintProperty(*It))
			{
				continue;
			}
			const FView Value = ViewOf(System, *It);
			FLinearColor Light;
			FLinearColor Dark;
			if (!Value.Color(TEXT("Light"), Light) || !Value.Color(TEXT("Dark"), Dark))
			{
				continue;
			}
			const FName Token = TokenForPreset(CleanFieldName(It->GetName()));
			if (Token.IsNone())
			{
				continue;
			}
			Theme->Colors.Add(Token, FVekaThemeColor(FVekaPaint(Light), FVekaPaint(Dark)));
			++Report.ColorTokens;
			Report.Add(FString::Printf(TEXT("  %s = %s / %s"), *Token.ToString(), *Describe(FVekaPaint(Light)), *Describe(FVekaPaint(Dark))));
		}

		// Theme slots: the root theme, then its sub-themes (object properties pointing at Toggle_Default...).
		TArray<TPair<FString, const UObject*>> Sources = { { FString(), Themes } };
		for (TFieldIterator<FObjectPropertyBase> It(Themes->GetClass()); It; ++It)
		{
			if (IsBlueprintProperty(*It))
			{
				const UObject* Sub = It->GetObjectPropertyValue_InContainer(Themes);
				const FString SubName = CleanFieldName(It->GetName());
				if (Sub && SubName != TEXT("Font") && SubName != TEXT("Glyph"))
				{
					Sources.Add({ SubName, Sub });
				}
				else if (SubName == TEXT("Font") || SubName == TEXT("Glyph"))
				{
					Report.Add(FString::Printf(TEXT("skipped %s (%s): fonts and glyphs are set by the view in VekaUI"), *SubName, Sub ? *Sub->GetPathName() : TEXT("none")));
				}
			}
		}
		for (const TPair<FString, const UObject*>& Source : Sources)
		{
			Report.Add(FString::Printf(TEXT("Theme slots of %s:"), *Source.Value->GetPathName()));
			for (TFieldIterator<FProperty> It(Source.Value->GetClass()); It; ++It)
			{
				if (!IsBlueprintProperty(*It))
				{
					continue;
				}
				const FString Slot = CleanFieldName(It->GetName());
				const FView Value = ViewOf(Source.Value, *It);
				bool bOverridden = true;
				const FView Settings = SettingsOf(Value, bOverridden);
				if (Settings.IsValid())
				{
					const FName Token = TokenForSlot(Source.Key, Slot);
					if (!bOverridden)
					{
						Report.Add(FString::Printf(TEXT("  %s <- %s: not set (stays .%s)"), *Token.ToString(), *Slot, *Veka::Colora::GetControlTokenTarget(Token).ToString()));
						continue;
					}
					const FVekaPaint Light = ReadSettings(Settings, false);
					const FVekaPaint Dark = ReadSettings(Settings, true);
					if (Light.Type == EVekaPaintType::Token && Light.Token == Token)
					{
						continue;
					}
					Theme->Colors.Add(Token, FVekaThemeColor(Light, Dark));
					++Report.ControlTokens;
					Report.Add(FString::Printf(TEXT("  %s <- %s = %s / %s"), *Token.ToString(), *Slot, *Describe(Light), *Describe(Dark)));
					continue;
				}
				if (const FStructProperty* Vector = CastField<FStructProperty>(*It); Vector && Vector->Struct == TBaseStructure<FVector2D>::Get())
				{
					// SliderSize, KnobSize -> Slider.Width / Height, Slider.KnobWidth / KnobHeight.
					const FVector2D Size = *Vector->ContainerPtrToValuePtr<FVector2D>(Source.Value);
					FString Base = Slot;
					Base.RemoveFromEnd(TEXT("Size"));
					Base.RemoveFromStart(Source.Key);
					const FString Prefix = (Source.Key.IsEmpty() ? FString(TEXT("Theme")) : Source.Key) + TEXT(".") + Base;
					Theme->Metrics.Add(FName(*(Prefix + TEXT("Width"))), static_cast<float>(Size.X));
					Theme->Metrics.Add(FName(*(Prefix + TEXT("Height"))), static_cast<float>(Size.Y));
					Report.Metrics += 2;
					Report.Add(FString::Printf(TEXT("  %sWidth / %sHeight <- %s = %g x %g"), *Prefix, *Prefix, *Slot, Size.X, Size.Y));
					continue;
				}
				Report.Add(FString::Printf(TEXT("  skipped %s [%s]: no VekaUI theme slot"), *Slot, *It->GetCPPType()));
			}
		}
		FAssetRegistryModule::AssetCreated(Theme);
		Theme->MarkPackageDirty();
		if (bSave)
		{
			const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			const bool bSaved = UPackage::SavePackage(Package, Theme, *File, Args);
			Report.Add(FString::Printf(TEXT("%s %s"), bSaved ? TEXT("saved") : TEXT("error: could not save"), *File));
		}
		Report.Add(FString::Printf(TEXT("%s: %d Colora tokens, %d control tokens, %d metrics"), *Theme->GetPathName(), Report.ColorTokens, Report.ControlTokens, Report.Metrics));
		return Theme;
	}

	static FAutoConsoleCommand DumpCommand(TEXT("Veka.DumpAlpha13"), TEXT("Logs the Alpha 13 theme assets (Veka.DumpAlpha13 [ContentDir])."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { Dump(Args.IsEmpty() ? DefaultContentDir() : Args[0]); }));

	static FAutoConsoleCommand MigrateCommand(TEXT("Veka.MigrateAlpha13Themes"),
		TEXT("Writes the Alpha 13 themes into /VekaUI/Themes/VT_Alpha13 (Veka.MigrateAlpha13Themes [ContentDir] [quit])."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			FString Dir = DefaultContentDir();
			bool bQuit = false;
			for (const FString& Arg : Args)
			{
				if (Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase))
				{
					bQuit = true;
				}
				else
				{
					Dir = Arg;
				}
			}
			if (Mount(Dir))
			{
				FReport Report;
				MigrateThemes(TEXT("/VekaUI/Themes"), TEXT("VT_Alpha13"), true, Report);
			}
			if (bQuit)
			{
				RequestEngineExit(TEXT("Veka.MigrateAlpha13Themes"));
			}
		}));
}
