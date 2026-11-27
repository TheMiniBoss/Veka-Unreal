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
#include "View/VekaViewAsset.h"
#include "View/VekaCatalog.h"
#include "VekaLanguage.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Styling/CoreStyle.h"
#include "CodeEditor/VekaCodeAssist.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

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

		/** A titled card (same look as the galleries' cards). */
		FVekaElement HubCard(const FText& Title, FVekaElement Content)
		{
			using namespace Veka;
			return VStack(12.f, EVekaHAlign::Leading)(Text(Title).Font(EVekaTextStyle::Headline), MoveTemp(Content))
				.Padding(18.f)
				.FlexFrame(0.0, Infinity, 0.0, -1.0, FVekaAlignment::TopLeading())
				.Background(Token(TEXT("Fill"), EVekaHierarchy::Quaternary), 16.f)
				.Border(Token(TEXT("Separator")), 1.f, 16.f);
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
				Text(Caption).Font(EVekaTextStyle::Caption2).Opacity(0.8f).LineLimit(1).MinimumScaleFactor(0.6f).Frame(92.0, -1.0)
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
				for (const FExample& Example : GetExamples())
				{
					FParsedExample& Parsed = Examples.AddDefaulted_GetRef();
					Parsed.Example = Example;
					FVekaParseOptions Options;
					Options.ResolveComponent = Veka::Lang::MakeAssetResolver();
					const FVekaParseResult Result = Veka::Lang::Parse(Example.Source, Options);
					Parsed.bValid = !Result.HasErrors();
					Parsed.Document = Result.Document;
					Parsed.Store = MakeShared<FVekaViewStore>();
				}
				Changelog = GetChangelog();
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
					Row(ESection::Examples, TEXT("square.grid.2x2"), false),
					Row(ESection::Documentation, TEXT("book"), false),
					Row(ESection::Changelog, TEXT("clock"), false)
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
								.PickerStyle(EVekaPickerStyle::Segmented)),
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
						HubCard(LOCTEXT("ColorTokens", "Colora tokens (switch Light / Dark and the theme above)"),
							Grid({ FVekaGridItem::Adaptive(96.f) }, 10.f, 10.f).Children(MoveTemp(Tokens)).FillWidth()),
						HubCard(LOCTEXT("ControlTokens", "Control tokens (a theme restyles every toggle, slider...)"),
							Grid({ FVekaGridItem::Adaptive(96.f) }, 10.f, 10.f).Children(MoveTemp(Controls)).FillWidth()),
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
							Text(Name.ToString()).Font(EVekaTextStyle::Caption2).Opacity(0.7f).LineLimit(1).MinimumScaleFactor(0.6f).Frame(92.0, -1.0)
						).Frame(96.0, 72.0));
					}
					return VStack(16.f, EVekaHAlign::Leading)(
						SettingsCard({
							Labeled(LOCTEXT("Weight", "Weight"), Slider(Weight, 0.f, 1.f).Frame(240.0, -1.0)),
							Labeled(LOCTEXT("RenderingMode", "Rendering mode"), Picker(FText::GetEmpty(), Mode,
								Options({ LOCTEXT("Monochrome", "Monochrome"), LOCTEXT("Hierarchical", "Hierarchical"), LOCTEXT("Palette", "Palette"), LOCTEXT("Multicolor", "Multicolor") }))
								.PickerStyle(EVekaPickerStyle::Segmented))
						}),
						HubCard(LOCTEXT("SymbolGrid", "Symbols of the project and the library (live weight and rendering mode)"),
							Grid({ FVekaGridItem::Adaptive(96.f) }, 8.f, 8.f).Children(MoveTemp(Cells)).FillWidth()),
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
								.PickerStyle(EVekaPickerStyle::Segmented))
						}),
						HubCard(LOCTEXT("LiveSample", "Live sample"),
							Text(LOCTEXT("Pangram", "Sphinx of black quartz, judge my vow.")).FontSize(Size.Get()).FontWeight(Weights[FMath::Clamp(Weight.Get(), 0, 6)])),
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
								Circle().Fill(FLinearColor(0.95f, 0.25f, 0.2f)).Frame(90.f, 90.f).BlendMode(Mode).Offset(FVector2D(-22.0, 0.0)),
								Circle().Fill(FLinearColor(0.15f, 0.45f, 0.95f)).Frame(90.f, 90.f).BlendMode(Mode).Offset(FVector2D(22.0, 0.0))
							).Frame(190.0, 110.0),
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
						HubCard(LOCTEXT("EffectsStage", "Backdrop, glass, distortion and blend over a stage"), VStack(10.f, EVekaHAlign::Leading)(
							ZStack()(EffectsStage(), Panels),
							Text(LOCTEXT("EffectsNote", "The effects of a host share one capture of what is under them (r.Veka.Backdrop.Quality, 0 = off). A blend mode on a shape blends it with everything under it, the other shapes included."))
								.Font(EVekaTextStyle::Footnote).Opacity(0.7f)
						))
					);
				}
				case ESection::Morph:
					return Samples::MorphGallery();
				case ESection::Examples:
					return ExamplesPage();
				case ESection::Documentation:
					return DocumentationPage(Ctx);
				default:
					return ChangelogPage();
				}
			}

			struct FParsedExample
			{
				FExample Example;
				FVekaDocument Document;
				TSharedPtr<FVekaViewStore> Store;
				bool bValid = false;
			};

			FVekaElement ExamplesPage() const;
			FVekaElement DocumentationPage(FVekaContext& Ctx) const;
			FVekaElement ChangelogPage() const;

			ESection InitialSection = ESection::Components;
			TArray<TObjectPtr<UVekaTheme>> Themes;
			TArray<FParsedExample> Examples;
			TArray<FChangelogRelease> Changelog;
		};

		FSlateFontInfo CodeFont()
		{
			return FCoreStyle::GetDefaultFontStyle("Mono", 11);
		}

		/** A block of Veka code, colored like the Code tab (on its dark background, in Light mode too), indentation kept. */
		FVekaElement Code(const FString& Source)
		{
			using namespace Veka;
			const FSlateFontInfo Font = CodeFont();
			float SpaceWidth = 6.6f;
			if (FSlateApplication::IsInitialized())
			{
				SpaceWidth = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(TEXT("          "), Font).X / 10.f;
			}
			const float LineHeight = Font.Size * 1.45f;

			TArray<FString> Lines;
			Source.Replace(TEXT("\t"), TEXT("    ")).ParseIntoArrayLines(Lines, false);
			TArray<FVekaElement> Rows;
			for (const FString& Line : Lines)
			{
				// Spaces become fixed-width gaps (text layout would trim them); the rest keeps its token color.
				TArray<FVekaElement> Parts;
				auto AddText = [&](int32 Start, int32 End, EVekaCodeToken Kind)
				{
					int32 Index = Start;
					while (Index < End)
					{
						int32 Next = Index;
						const bool bSpace = FChar::IsWhitespace(Line[Index]);
						while (Next < End && FChar::IsWhitespace(Line[Next]) == bSpace)
						{
							++Next;
						}
						Parts.Add(bSpace
							? Empty().Frame(SpaceWidth * (Next - Index), 1.f)
							: Text(Line.Mid(Index, Next - Index)).Font(Font).Foreground(FVekaPaint(TokenColor(Kind))).LineLimit(1));
						Index = Next;
					}
				};
				int32 Cursor = 0;
				for (const FVekaCodeSpan& Span : TokenizeLine(Line))
				{
					AddText(Cursor, Span.Start, EVekaCodeToken::Plain);
					AddText(Span.Start, Span.Start + Span.Length, Span.Kind);
					Cursor = Span.Start + Span.Length;
				}
				AddText(Cursor, Line.Len(), EVekaCodeToken::Plain);
				Rows.Add(HStack(0.f).Children(MoveTemp(Parts)).Frame(-1.0, LineHeight, FVekaAlignment::Leading()));
			}
			return VStack(0.f, EVekaHAlign::Leading).Children(MoveTemp(Rows))
				.Padding(FMargin(16.f, 12.f))
				.FillWidth(FVekaAlignment::Leading())
				.Background(FVekaPaint(FLinearColor(0.035f, 0.038f, 0.048f)), 10.f)
				.Border(FVekaPaint(FLinearColor(1.f, 1.f, 1.f, 0.08f)), 1.f, 10.f);
		}

		FString KindName(const FVekaCatalogArgument& Argument)
		{
			switch (Argument.Kind)
			{
			case EVekaArgumentKind::Number: return TEXT("Number");
			case EVekaArgumentKind::Bool: return TEXT("Bool");
			case EVekaArgumentKind::Text: return TEXT("Text (localizable)");
			case EVekaArgumentKind::Name: return TEXT("Name");
			case EVekaArgumentKind::Paint: return TEXT("Paint (color, hex, Colora token, paint asset)");
			case EVekaArgumentKind::Binding: return TEXT("State (written back)");
			case EVekaArgumentKind::Style: return TEXT("Shared style name");
			case EVekaArgumentKind::Enum:
			{
				TArray<FString> Values;
				for (int32 Index = 0; Argument.Enum && Index < Argument.Enum->NumEnums() - 1; ++Index)
				{
					if (!Argument.Enum->HasMetaData(TEXT("Hidden"), Index))
					{
						Values.Add(TEXT(".") + Argument.Enum->GetNameStringByIndex(Index));
					}
				}
				return FString::Join(Values, TEXT(" "));
			}
			default: return TEXT("Any");
			}
		}

		/** One catalog entry: its call, arguments and what it accepts. */
		FVekaElement ReferenceCard(const FVekaCatalogEntry& Entry, bool bModifier)
		{
			using namespace Veka;
			TArray<FString> Signature;
			TArray<FVekaElement> Rows;
			for (const FVekaCatalogArgument& Argument : Entry.Arguments)
			{
				const bool bPrimary = Argument.Name == Entry.PrimaryArgument;
				Signature.Add(bPrimary ? Argument.Name.ToString() : Argument.Name.ToString() + TEXT(":"));
				Rows.Add(HStack(10.f, EVekaVAlign::Top)(
					Text(Argument.Name.ToString() + (bPrimary ? TEXT(" (first)") : TEXT(""))).Font(CodeFont()).Frame(150.0, -1.0, FVekaAlignment::TopLeading()),
					Text(KindName(Argument)).Font(EVekaTextStyle::Footnote).Opacity(0.8f).FlexFrame(0.0, Infinity, -1.0, -1.0, FVekaAlignment::TopLeading())
				));
			}
			const FString Call = (bModifier ? TEXT(".") : TEXT("")) + Entry.Name.ToString() + (Entry.Arguments.IsEmpty() ? FString() : TEXT("(") + FString::Join(Signature, TEXT(", ")) + TEXT(")"))
				+ (Entry.bContainer ? TEXT(" { ... }") : TEXT("")) + (Entry.bActions && !Entry.bContainer ? TEXT(" { actions }") : TEXT(""));
			TArray<FString> Notes;
			if (Entry.bContainer)
			{
				Notes.Add(TEXT("holds views"));
			}
			if (Entry.bActions)
			{
				Notes.Add(TEXT("runs actions"));
			}
			TArray<FVekaElement> Parts = { Code(Call) };
			if (!Notes.IsEmpty())
			{
				Parts.Add(Text(FString::Join(Notes, TEXT(" \u00B7 "))).Font(EVekaTextStyle::Caption1).Opacity(0.7f));
			}
			Parts.Add(VStack(6.f, EVekaHAlign::Leading).Children(MoveTemp(Rows)));
			return HubCard(FText::FromName(Entry.Name), VStack(8.f, EVekaHAlign::Leading).Children(MoveTemp(Parts)));
		}

		FVekaElement FVekaHubView::ExamplesPage() const
		{
			using namespace Veka;
			TArray<FVekaElement> Cards;
			for (const FParsedExample& Parsed : Examples)
			{
				const FExample Example = Parsed.Example;
				FVekaElement Preview = Parsed.bValid
					? Veka::Interpreter::MakeView(Parsed.Document, Parsed.Store.ToSharedRef())
					: Text(LOCTEXT("ExampleBroken", "This example does not parse.")).Foreground(Token(TEXT("Red")));
				Cards.Add(HubCard(Example.Title, VStack(12.f, EVekaHAlign::Leading)(
					Text(Example.Description).Font(EVekaTextStyle::Footnote).Opacity(0.75f),
					MoveTemp(Preview)
						.FlexFrame(0.0, Infinity, 300.0, 300.0, FVekaAlignment())
						.Background(Token(TEXT("Fill"), EVekaHierarchy::Tertiary), 12.f)
						.Clip(),
					HStack(10.f)(
						Text(Example.Name.ToString()).Font(CodeFont()).Opacity(0.7f),
						Spacer(),
						Veka::Button(LOCTEXT("OpenExample", "Open as asset"), TEXT("square.and.arrow.up"), [Example]() { OpenExample(Example); })
							.ButtonStyle(EVekaButtonStyleKind::BorderedProminent)
					)
				)));
			}
			return VStack(16.f, EVekaHAlign::Leading)(
				Text(LOCTEXT("ExamplesIntro", "Live examples written in Veka. \"Open as asset\" creates the view in /Game/VekaExamples and opens it in the view editor (save it to keep it)."))
					.Font(EVekaTextStyle::Body).Opacity(0.8f),
				Grid({ FVekaGridItem::Adaptive(440.f) }, 16.f, 16.f, FVekaAlignment::TopLeading()).Children(MoveTemp(Cards)).FillWidth(FVekaAlignment::TopLeading())
			);
		}

		FVekaElement FVekaHubView::DocumentationPage(FVekaContext& Ctx) const
		{
			using namespace Veka;
			const TVekaState<int32> Page = Ctx.State<int32>(TEXT("Docs.Page"), 0);
			const TVekaState<FString> Search = Ctx.State<FString>(TEXT("Docs.Search"), FString());
			const FString Query = Search.Get().TrimStartAndEnd();

			FVekaElement Body;
			if (Page.Get() == 0)
			{
				Body = Grid({ FVekaGridItem::Adaptive(520.f) }, 16.f, 16.f, FVekaAlignment::TopLeading())(
					HubCard(LOCTEXT("GuideFirst", "A first view"), VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("GuideFirstText", "A view is a tree of components, each followed by its modifiers. Create one with Content Browser > Add > User Interface > Veka View, or import a .vk file.")).Font(EVekaTextStyle::Footnote),
						Code(TEXT("view Hello {\n    VStack(spacing: 12) {\n        Text(\"Hello Veka\")\n            .font(.Title2)\n        Symbol(#\"star.fill\")\n            .foreground(.Yellow)\n    }\n    .padding(24)\n}"))
					)),
					HubCard(LOCTEXT("GuideState", "States and actions"), VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("GuideStateText", "@State declares a value of the view; controls bind to it, actions change it, withAnimation animates what depends on it.")).Font(EVekaTextStyle::Footnote),
						Code(TEXT("@State var count: Int = 0\n\nButton(\"Tapped \\(count) times\") {\n    withAnimation {\n        count += 1\n    }\n}\nToggle(\"Sound\", isOn: sound)"))
					)),
					HubCard(LOCTEXT("GuideComponents", "Components"), VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("GuideComponentsText", "Any view is a component of another: @Param gives it arguments, @Slot a place for content, @Event a way to talk back. Component libraries list them in the palette and share styles.")).Font(EVekaTextStyle::Footnote),
						Code(TEXT("view Badge {\n    @Param var title: Text = \"New\"\n    @Slot default\n    HStack { Text(title)  Slot(default) }\n}\n\nBadge(title: \"Sale\") { Symbol(#\"tag\") }\n    .style(.Card)"))
					)),
					HubCard(LOCTEXT("GuideEffects", "Effects"), VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("GuideEffectsText", "Backdrops read what is under the node (the 3D scene, the UI): one capture is shared by the effects of a host. r.Veka.Backdrop.Quality sets the quality (0 = off).")).Font(EVekaTextStyle::Footnote),
						Code(TEXT(".backdropBlur(24, cornerRadius: 16)\n.frostedGlass(blur: 30)\n.glass(28)\n.glassGroup(16)\n.liquidDistortion(10)\n.blendMode(.Multiply)"))
					)),
					HubCard(LOCTEXT("GuideThemes", "Colors and themes"), VStack(10.f, EVekaHAlign::Leading)(
						Text(LOCTEXT("GuideThemesText", "Colora tokens (.Accent, .WindowBackground...) resolve from the theme and the Light / Dark scheme of the subtree. A UVekaTheme overrides tokens, metrics and text styles; control tokens restyle every control.")).Font(EVekaTextStyle::Footnote),
						Code(TEXT(".background(.WindowBackground, radius: 12)\n.foreground(.Accent)\n.background(.Aurora)   // a Veka Paint asset"))
					))
				).FillWidth(FVekaAlignment::TopLeading());
			}
			else
			{
				const bool bModifiers = Page.Get() == 2;
				TArray<FVekaElement> Cards;
				for (const FVekaCatalogEntry& Entry : bModifiers ? Veka::Catalog::Modifiers() : Veka::Catalog::Components())
				{
					if (Query.IsEmpty() || Entry.Name.ToString().Contains(Query))
					{
						Cards.Add(ReferenceCard(Entry, bModifiers));
					}
				}
				const int32 Count = Cards.Num();
				Body = VStack(10.f, EVekaHAlign::Leading)(
					Text(FText::Format(LOCTEXT("ReferenceCount", "{0} entries, generated from the catalog the language, the inspector and completion use."), FText::AsNumber(Count)))
						.Font(EVekaTextStyle::Footnote).Opacity(0.7f),
					Grid({ FVekaGridItem::Adaptive(400.f) }, 14.f, 14.f, FVekaAlignment::TopLeading()).Children(MoveTemp(Cards)).FillWidth(FVekaAlignment::TopLeading())
				);
			}
			return VStack(16.f, EVekaHAlign::Leading)(
				HStack(16.f)(
					Picker(FText::GetEmpty(), Page, Options({ LOCTEXT("Guide", "Guide"), LOCTEXT("ComponentsRef", "Components"), LOCTEXT("ModifiersRef", "Modifiers") }))
						.PickerStyle(EVekaPickerStyle::Segmented),
					Page.Get() == 0 ? Spacer(0.f) : TextField(LOCTEXT("SearchDocs", "Search"), Search).Frame(240.0, -1.0)
				),
				MoveTemp(Body)
			);
		}

		FVekaElement FVekaHubView::ChangelogPage() const
		{
			using namespace Veka;
			TArray<FVekaElement> Releases;
			for (const FChangelogRelease& Release : Changelog)
			{
				TArray<FVekaElement> Entries;
				for (const FString& Entry : Release.Entries)
				{
					Entries.Add(HStack(8.f, EVekaVAlign::Top)(Text(TEXT("\u2022")).Opacity(0.6f), Text(Entry).Font(EVekaTextStyle::Footnote).FlexFrame(0.0, Infinity, -1.0, -1.0, FVekaAlignment::TopLeading())));
				}
				Releases.Add(HubCard(FText::FromString(Release.Title), VStack(6.f, EVekaHAlign::Leading).Children(MoveTemp(Entries))));
			}
			if (Releases.IsEmpty())
			{
				Releases.Add(Text(LOCTEXT("NoChangelog", "Docs/CHANGELOG.md was not found in the plugin.")));
			}
			return VStack(14.f, EVekaHAlign::Leading).Children(MoveTemp(Releases));
		}

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

	namespace
	{
		FString PluginDir()
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VekaUI"));
			return Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
		}
	}

	TArray<FExample> GetExamples()
	{
		TArray<FExample> Out;
		const FString Root = PluginDir() / TEXT("Samples/Veka");
		TArray<FString> Files = { Root / TEXT("VV_Starter.vk") };
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *(Root / TEXT("Examples/*.vk")), true, false);
		Found.Sort();
		for (const FString& File : Found)
		{
			Files.Add(Root / TEXT("Examples") / File);
		}
		for (const FString& File : Files)
		{
			FString Source;
			if (!FFileHelper::LoadFileToString(Source, *File))
			{
				continue;
			}
			FExample& Example = Out.AddDefaulted_GetRef();
			Example.Name = FName(*FPaths::GetBaseFilename(File));
			FString Title = Example.Name.ToString();
			Title.RemoveFromStart(TEXT("VV_Example"));
			Title.RemoveFromStart(TEXT("VV_"));
			Example.Title = FText::FromString(Title);
			Example.Source = Source;
			// The first comment line describes the example.
			TArray<FString> Lines;
			Source.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				const FString Trimmed = Line.TrimStart();
				if (Trimmed.StartsWith(TEXT("//")))
				{
					Example.Description = FText::FromString(Trimmed.Mid(2).TrimStart());
					break;
				}
			}
			if (Example.Description.IsEmpty())
			{
				Example.Description = LOCTEXT("StarterDescription", "The starter view: a title and a button that counts its taps.");
			}
		}
		return Out;
	}

	UVekaView* OpenExample(const FExample& Example, bool bOpenEditor)
	{
		const FString PackageName = TEXT("/Game/VekaExamples/") + Example.Name.ToString();
		UVekaView* View = LoadObject<UVekaView>(nullptr, *(PackageName + TEXT(".") + Example.Name.ToString()), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!View)
		{
			FVekaParseOptions Options;
			Options.ResolveComponent = Veka::Lang::MakeAssetResolver();
			const FVekaParseResult Result = Veka::Lang::Parse(Example.Source, Options);
			if (Result.HasErrors())
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*PackageName);
			View = NewObject<UVekaView>(Package, Example.Name, RF_Public | RF_Standalone | RF_Transactional);
			View->Document = Result.Document;
#if WITH_EDITORONLY_DATA
			View->SourceText = Example.Source;
#endif
			FAssetRegistryModule::AssetCreated(View);
			View->MarkPackageDirty();
		}
		if (bOpenEditor && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(View);
		}
		return View;
	}

	TArray<FChangelogRelease> GetChangelog()
	{
		TArray<FChangelogRelease> Out;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(PluginDir() / TEXT("Docs/CHANGELOG.md"))))
		{
			return Out;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.StartsWith(TEXT("## ")))
			{
				Out.AddDefaulted_GetRef().Title = Trimmed.Mid(3);
			}
			else if (Trimmed.StartsWith(TEXT("- ")) && !Out.IsEmpty())
			{
				Out.Last().Entries.Add(Trimmed.Mid(2));
			}
		}
		return Out;
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
