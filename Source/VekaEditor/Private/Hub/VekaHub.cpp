// Copyright TheMiniBoss. All Rights Reserved.

#include "Hub/VekaHub.h"
#include "VekaBuilders.h"
#include "Core/VekaView.h"
#include "Core/VekaPaint.h"
#include "Colora/VekaTheme.h"
#include "Controls/VekaControls.h"
#include "Modifiers/VekaEffectModifiers.h"
#include "Symbols/VekaSymbol.h"
#include "Samples/VekaPlayground.h"
#include "Slate/SVekaHost.h"
#include "StructUtils/InstancedStruct.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "VekaHub"

namespace Veka::Editor::Hub
{
	const FName TabId(TEXT("VekaHub"));

	FText SectionTitle(ESection Section)
	{
		switch (Section)
		{
		case ESection::Components: return LOCTEXT("Components", "Components");
		case ESection::Colors: return LOCTEXT("Colors", "Colors");
		case ESection::Symbols: return LOCTEXT("Symbols", "Symbols");
		case ESection::Typography: return LOCTEXT("Typography", "Typography");
		case ESection::Animations: return LOCTEXT("Animations", "Animations");
		case ESection::Effects: return LOCTEXT("Effects", "Effects");
		case ESection::Morph: return LOCTEXT("Morph", "Morph");
		case ESection::Examples: return LOCTEXT("Examples", "Examples");
		case ESection::Documentation: return LOCTEXT("Documentation", "Documentation");
		case ESection::Changelog: return LOCTEXT("Changelog", "Changelog");
		default: return FText::GetEmpty();
		}
	}

	namespace
	{
		FVekaPaint Token(const TCHAR* Name, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary)
		{
			return FVekaPaint::FromToken(FName(Name), Hierarchy);
		}

		/** A card holding a row of live settings above a preview. */
		FVekaElement SettingsCard(TArray<FVekaElement> Settings)
		{
			using namespace Veka;
			return HStack(20.f).Children(MoveTemp(Settings))
				.Padding(FMargin(16.f, 12.f))
				.FillWidth(FVekaAlignment::Leading())
				.Background(Token(TEXT("Fill"), EVekaHierarchy::Tertiary), 12.f);
		}

		FVekaElement Labeled(const FText& Title, FVekaElement Control)
		{
			using namespace Veka;
			return VStack(4.f, EVekaHAlign::Leading)(Text(Title).Font(EVekaTextStyle::Caption1).Opacity(0.7f), MoveTemp(Control));
		}

		TArray<FVekaPickerOption> Options(std::initializer_list<FText> Titles)
		{
			TArray<FVekaPickerOption> Out;
			for (const FText& Title : Titles)
			{
				Out.Add(FVekaPickerOption(Title));
			}
			return Out;
		}

		FVekaElement Swatch(FName Token, const FString& Caption)
		{
			using namespace Veka;
			return VStack(4.f)(
				RoundedRectangle(10.f).Fill(FVekaPaint::FromToken(Token)).Frame(88.f, 48.f).Border(FVekaPaint::FromToken(TEXT("Separator")), 1.f, 10.f),
				Text(Caption).Font(EVekaTextStyle::Caption2).Opacity(0.8f)
			);
		}

		/** Colorful stage the effects are shown over (mesh gradient, stripes, a word). */
		FVekaElement EffectsStage()
		{
			using namespace Veka;
			FVekaMeshGradient Mesh = FVekaMeshGradient::Grid(3, 3, {
				Hex(TEXT("#1B1464FF")), Hex(TEXT("#6A1B9AFF")), Hex(TEXT("#0D47A1FF")),
				Hex(TEXT("#FF6F61FF")), Hex(TEXT("#FFC75FFF")), Hex(TEXT("#00C9A7FF")),
				Hex(TEXT("#845EC2FF")), Hex(TEXT("#D65DB1FF")), Hex(TEXT("#2C73D2FF")) });
			TArray<FVekaElement> Stripes;
			for (int32 Index = 0; Index < 24; ++Index)
			{
				Stripes.Add(Rectangle().Fill(FLinearColor(1.f, 1.f, 1.f, Index % 2 ? 0.35f : 0.f)).Frame(20.f, 300.f));
			}
			return ZStack()(
				RoundedRectangle(20.f, true).Fill(FVekaPaint::FromMesh(Mesh)),
				HStack(10.f).Children(MoveTemp(Stripes)).Rotation(18.f),
				Text(TEXT("VEKA")).FontSize(120.f).FontWeight(EVekaFontWeight::Black).Opacity(0.85f)
			)
			.Clip()
			.Frame(860.0, 280.0);
		}

		class FVekaHubView : public FVekaView
		{
			VEKA_VIEW(FVekaHubView)

		public:
			FVekaHubView(ESection InInitial, TArray<UVekaTheme*> InThemes) : InitialSection(InInitial)
			{
				for (UVekaTheme* Theme : InThemes)
				{
					Themes.Add(Theme);
				}
			}

			virtual void AddReferencedObjects(FReferenceCollector& Collector) override
			{
				Collector.AddReferencedObjects(Themes);
			}

			virtual FVekaElement Body(FVekaContext& Ctx) const override
			{
				using namespace Veka;
				const TVekaState<int32> Section = Ctx.State<int32>(TEXT("Section"), static_cast<int32>(InitialSection));
				const TVekaState<bool> Light = Ctx.State<bool>(TEXT("Light"), false);
				const TVekaState<int32> ThemeIndex = Ctx.State<int32>(TEXT("Theme"), 0);

				TArray<FVekaPickerOption> ThemeOptions = { FVekaPickerOption(LOCTEXT("BuiltInTheme", "Veka (built-in)")) };
				for (const TObjectPtr<UVekaTheme>& Theme : Themes)
				{
					ThemeOptions.Add(FVekaPickerOption(FText::FromName(Theme ? Theme->GetFName() : NAME_None)));
				}
				UVekaTheme* Theme = Themes.IsValidIndex(ThemeIndex.Get() - 1) ? Themes[ThemeIndex.Get() - 1].Get() : nullptr;
				const ESection Current = static_cast<ESection>(FMath::Clamp(Section.Get(), 0, static_cast<int32>(ESection::Count) - 1));

				return HStack(0.f, EVekaVAlign::Top)(
					Sidebar(Section),
					Divider(),
					VStack(16.f, EVekaHAlign::Leading)(
						HStack(16.f)(
							Text(SectionTitle(Current)).Font(EVekaTextStyle::LargeTitle),
							Spacer(),
							Toggle(LOCTEXT("LightMode", "Light"), Light).Frame(130.0, -1.0),
							Picker(LOCTEXT("Theme", "Theme"), ThemeIndex, ThemeOptions).Frame(240.0, -1.0)
						).FillWidth(),
						ScrollView(EVekaAxis::Vertical)(
							Content(Ctx, Current).Padding(FMargin(0.f, 0.f, 16.f, 24.f)).FillWidth(FVekaAlignment::TopLeading())
						)
					)
					.Padding(FMargin(24.f, 20.f, 8.f, 0.f))
					.FlexFrame(0.0, Infinity, 0.0, Infinity, FVekaAlignment::TopLeading())
				)
				.Background(Token(TEXT("Background")))
				.Foreground(Token(TEXT("Foreground")))
				.ColorScheme(Light.Get() ? EVekaColorScheme::Light : EVekaColorScheme::Dark)
				.Theme(Theme);
			}

		private:
			FVekaElement Sidebar(const TVekaState<int32>& Section) const
			{
				using namespace Veka;
				auto Row = [&Section](ESection Item, FName Symbol, bool bSoon)
				{
					const bool bSelected = Section.Get() == static_cast<int32>(Item);
					FVekaElement Label = HStack(10.f)(
						Veka::Symbol(Symbol).Frame(18.f, 18.f),
						Text(SectionTitle(Item)),
						Spacer(),
						bSoon ? Text(TEXT("4e")).Font(EVekaTextStyle::Caption2).Opacity(0.5f) : Spacer(0.f)
					)
					.Padding(FMargin(12.f, 7.f))
					.FillWidth(FVekaAlignment::Leading())
					.Background(bSelected ? Token(TEXT("Selection")) : FVekaPaint(FLinearColor::Transparent), 8.f);
					return MoveTemp(Label).OnTap([Section, Item]() { Section.Set(static_cast<int32>(Item)); });
				};
				auto Header = [](const FText& Title)
				{
					return Text(Title).Font(EVekaTextStyle::Footnote).Opacity(0.55f).Padding(FMargin(12.f, 14.f, 12.f, 4.f));
				};
				return VStack(2.f, EVekaHAlign::Leading)(
					HStack(10.f)(Veka::Symbol(TEXT("veka.logo")).Font(EVekaTextStyle::Title2), Text(TEXT("Veka Hub")).Font(EVekaTextStyle::Title3))
						.Padding(FMargin(12.f, 16.f, 12.f, 6.f)),
					Header(LOCTEXT("DesignLab", "DesignLab")),
					Row(ESection::Components, TEXT("slider.horizontal.3"), false),
					Row(ESection::Colors, TEXT("paintpalette"), false),
					Row(ESection::Symbols, TEXT("star"), false),
					Row(ESection::Typography, TEXT("textformat.size"), false),
					Row(ESection::Animations, TEXT("wand.and.stars"), false),
					Row(ESection::Effects, TEXT("sparkles"), false),
					Row(ESection::Morph, TEXT("drop"), false),
					Header(LOCTEXT("Learn", "Learn")),
					Row(ESection::Examples, TEXT("square.grid.2x2"), true),
					Row(ESection::Documentation, TEXT("book"), true),
					Row(ESection::Changelog, TEXT("clock"), true)
				)
				.Padding(8.f)
				.FlexFrame(230.0, 230.0, 0.0, Infinity, FVekaAlignment::TopLeading())
				.Background(Token(TEXT("WindowBackground")));
			}

			FVekaElement Content(FVekaContext& Ctx, ESection Current) const
			{
				using namespace Veka;
				switch (Current)
				{
				case ESection::Components:
				{
					const TVekaState<int32> Size = Ctx.State<int32>(TEXT("Components.Size"), 2);
					const TVekaState<bool> Off = Ctx.State<bool>(TEXT("Components.Disabled"), false);
					const EVekaControlSize Sizes[] = { EVekaControlSize::Mini, EVekaControlSize::Small, EVekaControlSize::Regular, EVekaControlSize::Large };
					return VStack(16.f, EVekaHAlign::Leading)(
						SettingsCard({
							Labeled(LOCTEXT("ControlSize", "Control size"), Picker(FText::GetEmpty(), Size,
								Options({ LOCTEXT("Mini", "Mini"), LOCTEXT("Small", "Small"), LOCTEXT("Regular", "Regular"), LOCTEXT("Large", "Large") }))
								.PickerStyle(EVekaPickerStyle::Segmented).Frame(360.0, -1.0)),
							Labeled(LOCTEXT("State", "State"), Toggle(LOCTEXT("Disabled", "Disabled"), Off).Frame(160.0, -1.0))
						}),
						Samples::ControlsGallery().ControlSize(Sizes[FMath::Clamp(Size.Get(), 0, 3)]).Disabled(Off.Get())
					);
				}
				case ESection::Colors:
				{
					TArray<FVekaElement> Tokens;
					for (const FName Name : Veka::Colora::DefaultTokenNames())
					{
						Tokens.Add(Swatch(Name, Name.ToString()));
					}
					TArray<FVekaElement> Controls;
					for (const FName Name : Veka::Colora::ControlTokenNames())
					{
						Controls.Add(Swatch(Name, Name.ToString()));
					}
					return VStack(16.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("ColorTokens", "Colora tokens (switch Light / Dark and the theme above)")).Font(EVekaTextStyle::Headline),
						Grid({ FVekaGridItem::Adaptive(96.f) }, 10.f, 10.f).Children(MoveTemp(Tokens)).FillWidth(),
						Text(LOCTEXT("ControlTokens", "Control tokens (a theme restyles every toggle, slider...)")).Font(EVekaTextStyle::Headline),
						Grid({ FVekaGridItem::Adaptive(96.f) }, 10.f, 10.f).Children(MoveTemp(Controls)).FillWidth(),
						Text(LOCTEXT("Paints", "Paints")).Font(EVekaTextStyle::Headline),
						Samples::ColoraGallery()
					);
				}
				case ESection::Symbols:
				{
					const TVekaState<float> Weight = Ctx.State<float>(TEXT("Symbols.Weight"), 0.4f);
					const TVekaState<int32> Mode = Ctx.State<int32>(TEXT("Symbols.Mode"), 1);
					const EVekaSymbolRenderingMode Modes[] = { EVekaSymbolRenderingMode::Monochrome, EVekaSymbolRenderingMode::Hierarchical,
						EVekaSymbolRenderingMode::Palette, EVekaSymbolRenderingMode::Multicolor };
					TArray<FName> Names = Veka::Symbols::GetAllNames();
					Names.SetNum(FMath::Min(Names.Num(), 96));
					TArray<FVekaElement> Cells;
					for (const FName Name : Names)
					{
						Cells.Add(VStack(6.f)(
							Veka::Symbol(Name).Font(EVekaTextStyle::Title1).SymbolWeight(Weight.Get()).SymbolRenderingMode(Modes[FMath::Clamp(Mode.Get(), 0, 3)]),
							Text(Name.ToString()).Font(EVekaTextStyle::Caption2).Opacity(0.7f)
						).Frame(96.0, 72.0));
					}
					return VStack(16.f, EVekaHAlign::Leading)(
						SettingsCard({
							Labeled(LOCTEXT("Weight", "Weight"), Slider(Weight, 0.f, 1.f).Frame(240.0, -1.0)),
							Labeled(LOCTEXT("RenderingMode", "Rendering mode"), Picker(FText::GetEmpty(), Mode,
								Options({ LOCTEXT("Monochrome", "Monochrome"), LOCTEXT("Hierarchical", "Hierarchical"), LOCTEXT("Palette", "Palette"), LOCTEXT("Multicolor", "Multicolor") }))
								.PickerStyle(EVekaPickerStyle::Segmented).Frame(440.0, -1.0))
						}),
						Grid({ FVekaGridItem::Adaptive(96.f) }, 8.f, 8.f).Children(MoveTemp(Cells)).FillWidth(),
						Samples::SymbolsGallery()
					);
				}
				case ESection::Typography:
				{
					const TVekaState<float> Size = Ctx.State<float>(TEXT("Typography.Size"), 28.f);
					const TVekaState<int32> Weight = Ctx.State<int32>(TEXT("Typography.Weight"), 3);
					const EVekaFontWeight Weights[] = { EVekaFontWeight::Thin, EVekaFontWeight::Light, EVekaFontWeight::Regular, EVekaFontWeight::Medium,
						EVekaFontWeight::SemiBold, EVekaFontWeight::Bold, EVekaFontWeight::Black };
					return VStack(16.f, EVekaHAlign::Leading)(
						SettingsCard({
							Labeled(LOCTEXT("Size", "Size"), Slider(Size, 10.f, 72.f, 1.f).Frame(240.0, -1.0)),
							Labeled(LOCTEXT("FontWeight", "Weight"), Picker(FText::GetEmpty(), Weight,
								Options({ LOCTEXT("Thin", "Thin"), LOCTEXT("LightW", "Light"), LOCTEXT("RegularW", "Regular"), LOCTEXT("Medium", "Medium"),
									LOCTEXT("SemiBold", "SemiBold"), LOCTEXT("Bold", "Bold"), LOCTEXT("Black", "Black") }))
								.PickerStyle(EVekaPickerStyle::Segmented).Frame(560.0, -1.0))
						}),
						Text(LOCTEXT("Pangram", "Sphinx of black quartz, judge my vow.")).FontSize(Size.Get()).FontWeight(Weights[FMath::Clamp(Weight.Get(), 0, 6)]),
						Samples::TypographyGallery()
					);
				}
				case ESection::Animations:
					return Samples::AnimaGallery();
				case ESection::Effects:
				{
					const TVekaState<float> Blur = Ctx.State<float>(TEXT("Effects.Blur"), 24.f);
					const TVekaState<float> Refraction = Ctx.State<float>(TEXT("Effects.Refraction"), 18.f);
					const TVekaState<float> Aberration = Ctx.State<float>(TEXT("Effects.Aberration"), 0.35f);
					const TVekaState<float> Highlight = Ctx.State<float>(TEXT("Effects.Highlight"), 0.6f);
					const TVekaState<float> Distortion = Ctx.State<float>(TEXT("Effects.Distortion"), 10.f);
					const TVekaState<int32> Blend = Ctx.State<int32>(TEXT("Effects.Blend"), 0);
					const UEnum* BlendModes = StaticEnum<EVekaBlendMode>();
					TArray<FVekaPickerOption> BlendOptions;
					for (int32 Index = static_cast<int32>(EVekaBlendMode::Multiply); Index < BlendModes->NumEnums() - 1; ++Index)
					{
						BlendOptions.Add(FVekaPickerOption(FText::FromString(BlendModes->GetNameStringByIndex(Index))));
					}
					const EVekaBlendMode Mode = static_cast<EVekaBlendMode>(static_cast<int32>(EVekaBlendMode::Multiply) + FMath::Clamp(Blend.Get(), 0, BlendOptions.Num() - 1));

					FVekaGlassMaterial Material;
					Material.Refraction = Refraction.Get();
					Material.Aberration = Aberration.Get();
					Material.Highlight = Highlight.Get();
					FVekaLiquidDistortionModifier Liquid;
					Liquid.Strength = Distortion.Get();
					Liquid.CornerRadii = FVekaCornerRadii(24.f);
					auto Caption = [](const FText& Title, const FText& Code)
					{
						return VStack(2.f, EVekaHAlign::Leading)(Text(Title).Font(EVekaTextStyle::Headline), Text(Code).Font(EVekaTextStyle::Caption1).Opacity(0.8f))
							.Foreground(FLinearColor::White);
					};
					const FVekaElement Panels = HStack(18.f)(
						Caption(LOCTEXT("Backdrop", "Backdrop"), FText::Format(LOCTEXT("BackdropCode", ".backdropBlur({0})"), FText::AsNumber(FMath::RoundToInt(Blur.Get()))))
							.Padding(16.f).Frame(190.0, 150.0, FVekaAlignment::TopLeading()).BackdropBlur(Blur.Get(), FVekaCornerRadii(24.f)),
						Caption(LOCTEXT("Glass", "Glass"), LOCTEXT("GlassCode", ".glass()"))
							.Padding(16.f).Frame(190.0, 150.0, FVekaAlignment::TopLeading()).Glass(FVekaCornerRadii(36.f), Material),
						Caption(LOCTEXT("Distortion", "Distortion"), LOCTEXT("DistortionCode", ".liquidDistortion()"))
							.Padding(16.f).Frame(190.0, 150.0, FVekaAlignment::TopLeading()).AddModifier(FInstancedStruct::Make(Liquid)),
						VStack(8.f)(
							ZStack()(
								Circle().Fill(FLinearColor(0.95f, 0.25f, 0.2f)).Frame(90.f, 90.f).Offset(FVector2D(-22.0, 0.0)),
								Circle().Fill(FLinearColor(0.15f, 0.45f, 0.95f)).Frame(90.f, 90.f).Offset(FVector2D(22.0, 0.0))
							).Frame(190.0, 110.0).BlendMode(Mode),
							Text(FText::FromString(BlendModes->GetNameStringByValue(static_cast<int64>(Mode)))).Font(EVekaTextStyle::Headline).Foreground(FLinearColor::White)
						)
					);
					return VStack(16.f, EVekaHAlign::Leading)(
						SettingsCard({
							Labeled(LOCTEXT("BlurSetting", "Blur"), Slider(Blur, 0.f, 60.f).Frame(150.0, -1.0)),
							Labeled(LOCTEXT("RefractionSetting", "Refraction"), Slider(Refraction, 0.f, 40.f).Frame(150.0, -1.0)),
							Labeled(LOCTEXT("AberrationSetting", "Aberration"), Slider(Aberration, 0.f, 1.f).Frame(150.0, -1.0)),
							Labeled(LOCTEXT("HighlightSetting", "Highlight"), Slider(Highlight, 0.f, 1.f).Frame(150.0, -1.0)),
							Labeled(LOCTEXT("DistortionSetting", "Distortion"), Slider(Distortion, 0.f, 30.f).Frame(150.0, -1.0)),
							Labeled(LOCTEXT("BlendSetting", "Blend mode"), Picker(FText::GetEmpty(), Blend, BlendOptions).Frame(180.0, -1.0))
						}),
						ZStack()(EffectsStage(), Panels),
						Text(LOCTEXT("EffectsNote", "Every effect of a host shares one capture of what is behind it (r.Veka.Backdrop.Quality, 0 = off)."))
							.Font(EVekaTextStyle::Footnote).Opacity(0.7f)
					);
				}
				case ESection::Morph:
					return Samples::MorphGallery();
				default:
					return VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("Soon", "Arrives with milestone 4e.")).Font(EVekaTextStyle::Title2),
						Text(LOCTEXT("SoonDetail", "Examples you can open as assets, documentation generated from the schema, and the changelog.")).Opacity(0.7f)
					);
				}
			}

			ESection InitialSection = ESection::Components;
			TArray<TObjectPtr<UVekaTheme>> Themes;
		};

		TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("HubTab", "Veka Hub"))
				[
					SNew(SVekaHost).Root(MakeHub(ESection::Components, GatherThemes()))
				];
		}

		void OpenHub()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(TabId);
		}

		void ExtendToolbar()
		{
			UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User"));
			FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("VekaUI"));
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("VekaHub"), FUIAction(FExecuteAction::CreateStatic(&OpenHub)),
				LOCTEXT("HubButton", "Veka Hub"), LOCTEXT("HubButtonTooltip", "Open the Veka Hub (DesignLab: components, colors, symbols, effects...)"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout")));
		}
	}

	FVekaElement MakeHub(ESection InitialSection, TArray<UVekaTheme*> Themes)
	{
		return Veka::View<FVekaHubView>(InitialSection, MoveTemp(Themes));
	}

	TArray<UVekaTheme*> GatherThemes()
	{
		TArray<UVekaTheme*> Out;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UVekaTheme::StaticClass()->GetClassPathName(), Assets, true);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Asset : Assets)
		{
			if (UVekaTheme* Theme = Cast<UVekaTheme>(Asset.GetAsset()))
			{
				Out.Add(Theme);
			}
		}
		return Out;
	}

	void Register()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateStatic(&SpawnTab))
			.SetDisplayName(LOCTEXT("HubTab", "Veka Hub"))
			.SetTooltipText(LOCTEXT("HubTooltip", "DesignLab: every Veka component, color, symbol, text style, animation and effect, with live settings."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&ExtendToolbar));
	}

	void Unregister()
	{
		if (UToolMenus* Menus = UToolMenus::TryGet())
		{
			Menus->RemoveEntry(TEXT("LevelEditor.LevelEditorToolBar.User"), TEXT("VekaUI"), TEXT("VekaHub"));
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
		}
	}
}

#undef LOCTEXT_NAMESPACE
