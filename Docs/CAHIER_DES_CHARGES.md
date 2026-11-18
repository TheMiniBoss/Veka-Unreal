# Cahier des charges — Conversion de Veka en C++ natif

| | |
|---|---|
| Projet | Veka (plugin UI pour Unreal Engine) |
| Version source | Alpha 13 (`Veka.uplugin`, `VersionName: "Alpha 13"`) |
| Source analysée | branche `Blueprint`, commit `f6d5cfd` (« Add Veka Alpha 13 plugin assets ») |
| Branche cible | `C++` |
| Moteur | `EngineVersion: 5.8.0` dans le `.uplugin` (les `.uasset` sont sauvés avec `++UE5+Release-5.7`) |
| Document compagnon | [`TODO.md`](TODO.md) — liste des tâches |
| Date | 2026-11-18 |

---

## 1. Contexte

Veka est une librairie de composants UI « design system » pour UMG : boutons, textes,
labels, symboles (glyphes), formes SDF, toggles, sliders, steppers, pickers, menus,
color picker, jauges, progress, images, média, web, effets, etc.

Aujourd'hui **tout est en Blueprint** : 335 `.uasset`, aucun module C++
(pas de dossier `Source/`, pas de `Modules` dans le `.uplugin`).
La logique vit dans des Widget Blueprints, des Blueprint Interfaces, une Blueprint
Function Library, une Macro Library, des UserDefinedStruct/Enum et des Primary Data Assets
Blueprint.

La branche `C++` a déjà été créée à partir de `Blueprint`, puis tout le dossier
`Content/` a été supprimé (commit `0102046` « remove bp in c++ branch »). Des PNG de
glyphes ont été ajoutés dans `Resources/Glyphs/`.

## 2. Objectifs

1. **Zéro logique Blueprint** : toute la logique (widgets, thèmes, couleurs, animation,
   outils éditeur) est écrite en C++.
2. **Parité fonctionnelle** avec Alpha 13 : chaque widget, enum, struct, réglage et
   outil existant a un équivalent C++ (voir mapping §5).
3. **Utilisable depuis Blueprint et le Designer UMG** : les widgets apparaissent dans la
   palette UMG, leurs propriétés sont éditables, l'aperçu en Designer marche, l'API
   publique est `BlueprintCallable`, les events sont des delegates assignables.
4. **Meilleures performances** : pas de `Tick` permanent, pas d'`UObject` créé par widget
   à chaque construction, compatible avec l'invalidation Slate.
5. **Code propre et maintenable** : conventions Epic, nommage corrigé, structures
   « fixes » (Pos1..Pos9, P1..P16, Create2Stops..Create9Stops) remplacées par des tableaux.

### Ce que « full C++ » veut dire ici

- **Oui** : aucune classe Blueprint, aucun graph, aucun UserDefinedStruct/Enum, aucun
  Widget Blueprint, aucune Editor Utility Blueprint.
- **Non** : « zéro `.uasset` ». Certains assets **ne sont pas de la logique** et restent
  des assets : matériaux et material functions (SDF, Colora, glyph, progress…),
  textures (glyphes, logos, wallpapers), polices (`UFont` / `UFontFace`), et les
  *instances* de data assets par défaut (thème Dark/Light, preset Colora, glyphes).
  Leurs **classes** deviennent C++. Le portage des matériaux en shaders `.usf` est une
  option future (hors périmètre, cf. §11).

## 3. Périmètre

### Dans le périmètre

- Module runtime `Veka` : types, data assets, services, animation, Colora, tous les widgets.
- Module éditeur `VekaEditor` : barre d'outils, fenêtre Documentation / Design Lab,
  Glyph Library, Gradient Matrix Editor, About, personnalisations de détails.
- Ré-import / recréation des assets non-logiques nécessaires (matériaux, textures, polices,
  data assets par défaut) dans `Content/`, rebranchés sur les classes C++.
- Outil de migration des valeurs des data assets Blueprint vers les nouvelles classes.
- Tests d'automatisation et page de démo.

### Hors périmètre

- Nouveaux composants listés dans la doc mais pas encore faits en Alpha 13
  (boutons d'onglet sans page de contenu : Alert, ConfirmationDialog, Notification,
  Presentations, Responsive, ScrollView, Search, TabView, Tip, UserInput, Animation, Colors,
  Effects, Materials… — liste à confirmer dans l'éditeur).
  Ils restent dans la roadmap.
- Réécriture des matériaux en HLSL/USF.
- Compatibilité binaire avec les projets qui utilisent déjà les WBP Veka
  (Alpha → rupture acceptée, mais documentée, cf. §8.4).

---

## 4. État des lieux (branche `Blueprint`)

### 4.1 Inventaire chiffré

| Type d'asset | Nombre | Devient |
|---|---:|---|
| Widget Blueprint (runtime + docs) | 80 | classes C++ `UWidget`/`SWidget` (runtime) et Slate (éditeur) |
| UserDefinedEnum | 36 | `UENUM` |
| UserDefinedStruct | 40 (dont 13 « Overridable ») | `USTRUCT` |
| Blueprint Interface | 6 | supprimées ou `UINTERFACE` (cf. §6.10) |
| Blueprint Function Library | 1 (`BFL_Veka`) | `UVekaBlueprintLibrary` + helpers C++ |
| Blueprint Macro Library | 1 (`Veka_MacroExtensions`) | supprimée (boucles C++) |
| Blueprint Object | 3 (`Veka_Anima`, `Veka_Colora`, `Veka_Environnement`) | classes/structs C++ |
| Classes Primary Data Asset BP | 10 | `UPrimaryDataAsset` C++ |
| Instances de data assets | ~30 (thèmes, presets, 19 glyphes…) | restent des assets, reparentées |
| Editor Utility Blueprint | 7 | module `VekaEditor` (UToolMenus) |
| Editor Utility Widget | 3 | onglets Slate (`SDockTab`) |
| Material | 14 | restent des assets |
| Material Instance | 14 | restent des assets |
| Material Function | 20 | restent des assets |
| Material Function Instance | 11 | restent des assets |
| Font / FontFace | 2 / 25 (LTFlode Neue + Redacted) | restent des assets |
| Textures | ~33 (glyphes, logos, wallpapers, grid) | restent des assets |

### 4.2 Architecture actuelle

```
Veka_Environnement (Object BP)  ── contient ──>  Themes (S_VekaEnvironnementThemes)
                                                  Colora (S_VekaEnvironnementThemesColora : Preset + UseDarkThemes?)
        │
        ▼ GetEnvironnement / GetThemesEnv / GetColoraEnv
Veka_WidgetCore (UserWidget BP, parent de presque tous les widgets)
  ├─ Anima  (Veka_Anima, spawn + tick)        → OnAnimate / OnAnimateFinished
  ├─ Colora (Veka_Colora, spawn par widget)    → pousse les params Colora dans un MID
  ├─ ContextMenu (Veka_ContextMenu, créé au clic droit)
  └─ Glyphes par thème (Info / Warning / Error / Check)

Primitives : Veka_Interactor (hit-test SDF + events souris)
             Veka_Shape (forme SDF matériau, utilise Interactor)
             Veka_Text (texte + matériau de police + Colora)
             Veka_Symbol (glyphe matériau, badges, rendering modes)
Composites : Label, Button, Toggle(+Switch/CheckBox/ToggleButton), Slider, ColorSlider,
             Stepper, Picker(+Menu/Segmented/Palette/Radio), Menu, ContextMenu,
             ColorPicker(+8 modes), Gauge(+4 variantes), Progress(+3 variantes),
             Divider, Image, Media, Web, TextField, Effect, 3DViews, Unavailable,
             KeyBindButton, PathUI/PathBuilderUI
Thèmes (Primary Data Assets BP) : Veka_Themes → Veka_Font, Veka_ThemesGlyph,
             Veka_ThemesMenu, Veka_ThemesPicker, Veka_ThemesSlider, Veka_ThemesToggle,
             Veka_ThemesBadgePreset ; Veka_Glyph ; Veka_ColoraPreset
Éditeur : EUB_* (barre d'outils), EUW_EditorVeka (Docs), EUW_VekaEditorGlyphLibrary,
             EUW_VekaEditorGradientMatrix
```

Les familles « à variantes » (Toggle, Picker, Gauge, Progress, ColorPicker) utilisent un
widget « façade » qui crée le bon sous-widget selon un enum (`ToggleType`, `PickerType`,
`GaugeType`, `ProgressType`, `ColorPickerMode`) et lui parle via une Blueprint Interface
(`BPI_Toggle`, `BPI_VekaPicker`, `BPI_VekaGauge`, `BPI_Progress`, `BPI_VekaColorPicker`).

### 4.3 Systèmes, en détail

#### Environnement — `Veka_Environnement`
Objet BP avec deux variables : `Themes` (`S_VekaEnvironnementThemes { Themes }`) et
`Colora` (`S_VekaEnvironnementThemesColora { Preset, UseDarkThemes? }`). Valeurs par défaut
`Themes_Adhafera` / `Colora_Adhafera`. Contient le `Build Number` « Alpha 13 ».
Accédé partout via `GetEnvironnement`, `GetThemesEnv`, `GetColoraEnv`.

#### Thèmes — `Veka_Themes` (Primary Data Asset)
- Couleurs par catégorie (en `S_VekaColoraColor` ou `S_VekaColoraSettings` overridable) :
  `AccentColor`, `BackgroundColor`, `TextColor`, `ButtonColor`, `GaugeAccentColor`,
  `GaugeBackgroundColor`, `MenuAccentColor`, `MenuBackgroundColor`, `PickerAccentColor`,
  `PickerBackgroundColor`, `ProgressAccentColor`, `ProgressBackgroundColor`,
  `SliderAccentColor`, `SliderBackgroundColor`, `SliderTextColor`, `ToggleAccentColor`,
  `ToggleBackgroundColor`.
- `Font` (`Veka_Font`, défaut `FONT_LTFlode`), `FontAppearance` (`S_VekaFontAppearance`).
- Sous-thèmes : `Glyph` (`Glyph_Default`), `Menu` (`Menu_Default`), `Picker`
  (`Picker_Default`), `Slider` (`Slider_Default`), `Toggle` (`Toggle_Default`).
- Fonctions de sélection : `SelectButtonColor`, `SelectGaugeColor`, `SelectMenuColor`,
  `SelectPickerColor`, `SelectPickerThemesColor`, `SelectProgressColor`,
  `SelectSliderColor`, `SelectSliderTextColor`, `SelectToggleColor`
  (enum `E_VekaThemesColorPreset { Accent, Background }`).
- Instances : `Veka_Default` (dark), `Veka_LightDefault` (light).

Sous-thèmes :

| Classe BP | Propriétés |
|---|---|
| `Veka_ThemesGlyph` | `InfoGlyph`, `WarningGlyph`, `ErrorGlyph`, `CheckGlyph`, `GlyphAppearance`, `Rendering`, `BadgePreset` |
| `Veka_ThemesBadgePreset` | glyphes de badge : `Remove`, `Warning`, `Exclamation`, `Interrogation`, `Clock`, `Check`, `ArrowRight/Left/Up/Down`, `Ellipsis` |
| `Veka_ThemesMenu` | `MenuAccentColor`, `MenuBackgroundColor`, `MenuArrowGlyph`, `SubMenuGlyph` |
| `Veka_ThemesPicker` | `PickerAccentColor`, `PickerBackgroundColor`, `PickerMenuGlyph`, `PickerPaletteGlyph`, `PickerRadioShape` |
| `Veka_ThemesSlider` | `SliderAccentColor`, `SliderBackgroundColor`, `SliderTextColor`, `SliderFont`, `SliderFontAppearance`, `SliderShapeMode`, `SliderSize`, `KnobColor`, `KnobShapeMode`, `KnobSize` |
| `Veka_ThemesToggle` | `ToggleAccentColor`, `ToggleBackgroundColor`, `ToggleCheckGlyph`, `ToggleCheckboxShape`, `ToggleSwitchShape` |

#### Couleurs — Colora
- `Veka_ColoraPreset` (Primary Data Asset, instance `Colora_System`) : une couleur
  `S_VekaColoraColor { Dark, Light }` par entrée de `E_VekaColoraColorPreset` (26 entrées :
  AccentColor, Black, Blue, …, Link, Fill) ; `GetColorByPreset`, `GetHierarchicalByPreset`
  (alpha par niveau `Primary/Secondary/Tertiary/Quaternary/Transparent`).
- `S_VekaColoraSettings { ColoraMode, Gradient, Mix, Stops }` : mode `Color | Gradient | Mix`.
- Dégradés : `S_VekaColoraGradient { GradientType, Linear, Angular, Radial, Matrix,
  CenterPoint, EndPoint, EndAngle, EndRadius, Preset }` ; types `Angular | Linear | Matrix | Radial`.
  - Linear `{ StartPoint, EndPoint }`, Angular `{ StartAngle, EndAngle }`,
    Radial `{ StartRadius, EndRadius, ForceRadial? }`,
    Matrix `{ Angle, Blend, DistortionScale, DistortionStrength, FillBlur, FillEmptySpace, Seed, UseOkLCH }`.
- Stops : `S_VekaColoraStops { NumberOfStops, Pos1..Pos9 }`, chaque stop
  `S_VekaColoraStopPoint { Color (S_VekaColoraColorAndHierarchical), Position (S_VekaColoraStopPosition { Stop, ForMatrix }) }`.
- Position : `S_VekaColoraPosition { Preset (E_VekaPosition 9 points), ManualPosition }`.
- `Veka_Colora` (Object BP, **spawné par widget**) : `SetupMaterial` pousse tous les
  paramètres vers un `MaterialInstanceDynamic` (`SetScalarParameterValue` /
  `SetVectorParameterValue`), gère `IncrementSeed`.
- `BPI_VekaColora` : `SetColoraMode`, `SetColoraMix`, `SetColoraStops`,
  `SetColoraLinear*`, `SetColoraAngular*`, `SetColoraRadial*`, `SetColoraElliptical*`,
  `SetColoraMatrix*` (Angle, Blend, DistortionScale, DistortionStrength, FillBlur,
  FillEmptySpace, Seed, UseOKLCH).
- Matériaux : `MF_VekaColora`, `MF_VekaColoraLinear/Angular/Radial`,
  `MF_VekaColoraGradientStops`, `MF_VekaGradientMatrix`, `MF_VekaLinearGradient2/3`,
  `MF_VekaSimpleGradient`.

#### Animation — `Veka_Anima`
Objet BP tické (`SetupAnimaTick`, `AnimateTick`) qui gère une map d'états
`S_VekaAnimaState { Anim, Animate?, AutoReverse?, CurrentDelay, CurrentRepeat,
CustomEasings, Delay, Duration, EasingsType, Repeat, Speed }`. Easings :
`Default, Linear, Ease In, Ease Out, Ease In Out, Circ Ease In/Out/In Out, Spring,
Custom Ease` (spring = `FloatSpringInterp` : Stiffness, CriticalDampingFactor, Mass ;
custom = `CurveFloat`). Events `OnAnima`, `OnFinished`.
Enums `E_VekaAnimaTransitions` (Opacity, Slide, Move, Push, Offset, Blur, Identity, Scale)
et `E_VekaAnimaTransitionMode` (Single, Asymetric, Combined) existent mais ne sont pas
encore branchés partout.

#### Glyphes / Symboles
- `Veka_Glyph` (Primary Data Asset) : `IconFill` (Texture2D), `MulticolorPrimary/
  Secondary/Tertiary`, `Badge`, `BadgeSize`, `BadgePositionX/Y`. 19 instances `Glyph_*`
  (ArrowUp/Down/Left/Right, ArrowUpDown, Check, Clock, Eraser, Exclamation, Info,
  Interrogation, KeylineGrid, Loading, Logo, Placeholder, Test, Thunder, Triangle, Warning…).
- `S_VekaGlyph { Preset (Info|Warning|Error|Check|Custom|None), CustomGlyph }`.
- `S_VekaGlyphAppearance { Filled?, Gradient?, Primary, Secondary, Tertiary, Rendering,
  Variant, VariantValue }` ; rendering `Monochrome | Hierarchical | Multicolor | Palette` ;
  variant `Default | Circle | Slash | Slash Circle`.
- Badges : `S_VekaBadge`, `S_VekaCustomBadge`, `E_VekaGlyphBadgePreset` (None, Add, Remove,
  Gear, Interrogation, Warning, Exlamation, Clock, Check, Arrow Right/Left/Up/Down,
  Ellipsis, Custom).
- Matériaux : `M_VekaGlyph`, `M_VekaGlyphMaskToIcon`, `MF_VekaBadge`.

#### Polices
- `Veka_Font` (Primary Data Asset, instance `FONT_LTFlode`) : une `Font` par graisse
  `E_VekaTypeface` (Hairline → Extra Black, 12 valeurs) + italique.
- `S_VekaFontAppearance { Italic?, LineSpacing, TextCase, Typeface }`.
- Polices `LTFlode` (24 faces) et `Redacted` (texte masqué, matériau `M_VekaFontRedacted`).

#### Variables « overridable »
13 structs `{ Override?, Valeur }` : ColoraColor, ColoraColorandHierarchical,
ColoraSettings, Glyph, GlyphAppearance, ShapeMode, Float, Integer, LinearColor,
Vector2D, Font, FontAppearance. Sert à « prendre la valeur du thème sauf si surchargée ».

#### Bibliothèque `BFL_Veka`
`Create2Stops` … `Create9Stops`, `CreateGradient`, `CreateLinearGradient`,
`CreateAngularGradient`, `CreateRadialGradient`, `CreateMatrixGradient`, `CreateMix`,
`HexToColor` (+ `HexCharToInteger`, `HexPairToFloat`), `GetDominantColor`,
`GetColorDarkThemes`, `SelectDarkThemesColor`, `SelectOverridableLinearColor`,
`SelectPosition`, `PositionPresetToVector2D`, `PositionToUV`, `SetAlpha`, `CustomGlyph`,
`SelectGlyphPreset`, `SelectCustomGlyph`, `CreateMenuButton`, `CreateMenuDivider`,
`CreateMenuLabel`, `CreateMenuSubMenu`, `CreateEmptySubMenu`, `MenuSectionToSubMenu`,
`EmptyMap`, `IsEditor`.

#### Widgets
Voir l'inventaire API complet en annexe D. Points clés :

- `Veka_WidgetCore` : base commune. Accès environnement/thèmes/Colora, `ActivateAnima`,
  `DeactivateAnima`, `SetupAnimaTick`, `CreateContextMenu` / `DestroyContextMenu`,
  `GetWidgetPosition`, `GetWidgetSize`, `GetGlyphPresetByThemes`, `SetColoraSettings`.
  Events : `OnAnimate`, `OnAnimateFinished`, `OnContextMenuSelected`, `OnSelected`.
- `Veka_Interactor` : hit-test SDF exact (Circle, Capsule, Ellipse, Box arrondi, Custom),
  `AutoRadius`, `AutoRadii`, `GetSafeRadii`, capture souris, events `OnClick`,
  `OnDoubleClick`, `OnPress`, `OnRelease`, `OnHover`, `OnUnhover`, `OnRightClick`,
  aperçu SDF debug (`M_SDFPreview`).
- `Veka_Shape` : forme SDF (`E_VekaShapeMode` : Rectangle, Circle, RoundedRectangle,
  Capsule, Ellipse, UnevenRoundedRectangle, Custom ; `E_VekaShapeOutlineMode` Fill/Outline/Both ;
  `E_VekaShapeGradientMode` Simple/Colora), radius/radii, « continuous corners », outline,
  highlight, custom points (`S_VekaShapeCustomPoint` P1..P16), masque du contenu,
  retainer + matériau d'effet. Relaye les events de l'Interactor.
- `Veka_Text` : texte avec police thème, graisse, italique, casse (`E_VekaTextCase`),
  troncature (`E_VekaTruncation` Disabled/End/Middle), wrap, justification, line spacing,
  « redacted », couleur Colora via matériau de police.
- `Veka_Symbol` : glyphe matériau + rendering modes + variantes + badge.
- Composites : Label (titre + icône, `E_VekaLabelMode`), Button (`E_VekaButtonStyle` Plain,
  Borderless, Bordered, Bordered Prominant), Toggle (Switch/Button/Checkbox ;
  comportement Toggle/Radio Strict/Radio Flexible), Slider (knob, step, labels min/max,
  suffixe, animé), ColorSlider, Stepper (appui long qui répète), Picker (Menu, Segmented
  animé, Palette, Radio), Menu / ContextMenu / MenuRow / MenuButton (sections Button,
  Divider, Label, Picker, SubMenu via `S_VekaMenuSection`), ColorPicker (RGB, HSV, okLCH,
  Hue, Luminosity, Saturation, Grayscale, CMYK ; LCH et CMYK sont marqués « Unavailable »),
  Gauge (Linear, Linear Alt., Circular, Circular Alt.), Progress (Linear, Circular, Lazy),
  Divider, Image (`E_VekaImageContentMode` None/Fit/Fill, 9-slice), Media (MediaPlayer),
  Web (WebBrowser), TextField (`E_VekaTextFieldType` Text/Number/Percent/Currency,
  `E_VekaTextFieldStyle` Plain/Rectangle/Rounded/Capsule), Effect (RetainerBox + matériau),
  3DViews (vue éclatée), Unavailable (placeholder), KeyBindButton (vide), PathUI /
  PathBuilderUI (tracé vectoriel MoveTo/LineTo/QuadCurveTo/CubicCurveTo/Close via `OnPaint`).
- `FPS` : widget de test à la racine de `Content/` (pas un composant).

#### Outils éditeur
- Barre d'outils : `EUB_Veka_AddToolBar` (menu « Veka » dans Level Editor et Widget
  Blueprint Editor : Documentation, Gradient Matrix Editor…), `EUB_Veka_RemoveToolbar`,
  `EUB_Veka_Button` (ouvre un EUW via `EditorUtilitySubsystem::SpawnAndRegisterTab`).
- `EUW_EditorVeka` : fenêtre « Veka Documentation » : onglets Documentation (Buttons,
  Iconography, Indicators, Media, Menus, Pickers, Shapes, Texts, ValueInputs), Design Lab
  (Buttons), About (version + animation).
- `EUW_VekaEditorGlyphLibrary` : parcourt l'Asset Registry (`Veka_Glyph`), aperçu, édition
  badge/position/taille/opacité, « dirty » + Apply (`SetEditorProperty`).
- `EUW_VekaEditorGradientMatrix` : éditeur de gradient matrix (9 points déplaçables,
  sliders Angle/Distortion/FillBlur, AutoPlay du seed, copie).

### 4.4 Défauts constatés à corriger pendant la conversion

| Problème | Correction voulue |
|---|---|
| `Create2Stops` … `Create9Stops`, `Pos1..Pos9`, `P1..P16`, `NumberOfStops` | `TArray` + `ClampMax` (9 stops, 16 points) |
| `Veka_Colora` et `Veka_Anima` créés par widget (`SpawnObject`) | structs C++ légers possédés par le widget |
| Tick permanent pour l'animation | active timer Slate, actif seulement pendant l'animation |
| `GetPlayerController` utilisé dans des widgets (casse en éditeur / sans PC) | jamais de dépendance au PlayerController |
| Fautes : `Prominant`, `WindowBakground`, `HIerarchical`, `Hierachical`, `Exlamation`, `Quarternary`, `Asymetric`, `Environnement`, `S_VekMenuSubMenu`, `MinLAbel` | noms corrigés côté C++ ; `DisplayName` conservé si utile |
| Restes : `NewFunction`, `NewEventDispatcher`, `TempGraph`, `SetupAnimaTick6`, `FPS`, `Grid` | supprimés |
| Nommage incohérent : `VekaPickerButton`, `PickerPalette`, `PathUI`, `E_Path`, `S_SubPath` | tout préfixé `Veka` |
| Interfaces BP par famille + widget façade | variantes gérées en interne par le widget C++ |
| Version moteur : `.uplugin` 5.8 / assets 5.7 | une seule version cible, fixée (cf. §11) |
| Glyphes en PNG dans `Resources/Glyphs` (branche `C++`) | `Resources/` n'est pas cooké : les glyphes runtime doivent rester des `UTexture2D` dans `Content/` |

---

## 5. Correspondance Blueprint → C++

Préfixes Epic : `U` (UObject), `F` (struct), `E` (enum), `S` (Slate), `I` (interface).
Préfixe produit : `Veka`.

### 5.1 Enums (`Public/Types/VekaEnums.h` ou un header par système)

| Blueprint | C++ | Valeurs |
|---|---|---|
| `E_VekaPosition` | `EVekaPosition` | TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight |
| `E_VekaAnimaEasings` | `EVekaEasing` | Default, Linear, EaseIn, EaseOut, EaseInOut, CircEaseIn, CircEaseOut, CircEaseInOut, Spring, Custom |
| `E_VekaAnimaTransitionMode` | `EVekaTransitionMode` | Single, Asymmetric, Combined |
| `E_VekaAnimaTransitions` | `EVekaTransition` | Opacity, Slide, Move, Push, Offset, Blur, Identity, Scale |
| `E_VekaThemesColorPreset` | `EVekaThemeColorRole` | Accent, Background |
| `E_VekaColoraColorPreset` | `EVekaColorPreset` | AccentColor, Black, Blue, Brown, Clear, Cyan, Gray, Green, Indigo, Mint, Orange, Pink, Purple, Red, Teal, White, Yellow, Tint, Background, WindowBackground, Foreground, Separator, Selection, Placeholder, Link, Fill |
| `E_VekaColoraGradientType` | `EVekaGradientType` | Angular, Linear, Matrix, Radial |
| `E_VekaColoraHIerarchicalPreset` | `EVekaHierarchy` | Primary, Secondary, Tertiary, Quaternary, Transparent |
| `E_VekaColoraMode` | `EVekaColoraMode` | Color, Gradient, Mix |
| `E_VekaBooleanMaterial` | *(supprimé — usage interne matériau)* | Off |
| `E_VekaLabelMode` | `EVekaLabelMode` | TitleAndIcon, Title, Icon |
| `E_VekaTextStyle` | `EVekaTextStyle` | Primary, Secondary, Tertiary |
| `E_VekaButtonStyle` | `EVekaButtonStyle` | Plain, Borderless, Bordered, BorderedProminent |
| `E_VekaGaugeType` | `EVekaGaugeType` | Linear, LinearAlt, CircularAlt, Circular |
| `E_Path` | `EVekaPathCommand` | MoveTo, LineTo, QuadCurveTo, CubicCurveTo, Close |
| `E_VekaPickerType` | `EVekaPickerType` | Menu, Segmented, Palette, Radio |
| `E_VekaRadioOrientation` | `EVekaOrientation` *(ou `EOrientation` moteur)* | Vertical, Horizontal |
| `E_VekaMenuSectionType` | `EVekaMenuSectionType` | Button, Divider, Label, Picker, SubMenu |
| `E_VekaToggleBehavior` | `EVekaToggleBehavior` | Toggle, RadioStrict, RadioFlexible |
| `E_VekaToggleType` | `EVekaToggleType` | Switch, Button, Checkbox |
| `E_VekaShapeGradientMode` | `EVekaShapeFillMode` | Simple, Colora |
| `E_VekaShapeMode` | `EVekaShape` | Rectangle, Circle, RoundedRectangle, Capsule, Ellipse, UnevenRoundedRectangle, Custom |
| `E_VekaShapeOutlineMode` | `EVekaOutlineMode` | Fill, Outline, Both |
| `E_VekaTextCase` | `EVekaTextCase` | Default, Lowercase, Uppercase |
| `E_VekaTruncation` | `EVekaTruncation` | Disabled, End, Middle |
| `E_VekaColorPickerMode` | `EVekaColorPickerMode` | RGB, HSV, OkLCH, Hue, Luminosity, Saturation, Grayscale, CMYK |
| `E_VekaColorPickerRGBMode` | `EVekaRGBRange` | Normalized (0–1), Byte (0–255), Percent (0–100 %) |
| `E_VekaImageContentMode` | `EVekaContentMode` | None, Fit, Fill |
| `E_VekaTextFieldStyle` | `EVekaTextFieldStyle` | Plain, Rectangle, Rounded, Capsule |
| `E_VekaTextFieldType` | `EVekaTextFieldType` | Text, Number, Percent, Currency |
| `E_VekaGlyphBadgePreset` | `EVekaBadgePreset` | None, Add, Remove, Gear, Interrogation, Warning, Exclamation, Clock, Check, ArrowRight, ArrowLeft, ArrowUp, ArrowDown, Ellipsis, Custom |
| `E_VekaGlyphPreset` | `EVekaGlyphPreset` | Info, Warning, Error, Check, Custom, None |
| `E_VekaGlyphRenderingMode` | `EVekaGlyphRendering` | Monochrome, Hierarchical, Multicolor, Palette |
| `E_VekaGlyphVariant` | `EVekaGlyphVariant` | Default, Circle, Slash, SlashCircle |
| `E_VekaProgressType` | `EVekaProgressType` | Linear, Circular, Lazy |
| `E_VekaTypeface` | `EVekaTypeface` | Hairline, Thin, ExtraLight, Light, Regular, News, Medium, SemiBold, Bold, ExtraBold, Black, ExtraBlack |

Chaque valeur garde un `UMETA(DisplayName = "...")` lisible (ex. « Linear Alt. »).

### 5.2 Structs

| Blueprint | C++ | Champs (C++) |
|---|---|---|
| `S_VekaColoraColor` | `FVekaColor` | `Dark`, `Light` (`FLinearColor`) + `Resolve(bool bDark)` |
| `S_VekaColoraColorAndHierarchical` | `FVekaColorRef` | `Preset`, `Hierarchy`, `ManualColor` (`FVekaColor`), `ManualHierarchy` (float) ; preset `Custom` = manuel |
| `S_VekaColoraPosition` | `FVekaPosition` | `Preset` (`EVekaPosition`), `bManual`, `ManualPosition` (`FVector2D`) |
| `S_VekaColoraStopPosition` | `FVekaStopPosition` | `Stop` (float 0–1), `MatrixPosition` (`FVector2D`) |
| `S_VekaColoraStopPoint` | `FVekaColorStop` | `Color` (`FVekaColorRef`), `Position` (`FVekaStopPosition`) |
| `S_VekaColoraStops` | *(fusionné)* | `TArray<FVekaColorStop> Stops` (max 9) dans `FVekaColoraSettings` |
| `S_VekaColoraGradientLinear` | `FVekaLinearGradient` | `StartPoint`, `EndPoint` (`FVekaPosition`) |
| `S_VekaColoraGradientAngular` | `FVekaAngularGradient` | `StartAngle`, `EndAngle` |
| `S_VekaColoraGradientRadial` | `FVekaRadialGradient` | `StartRadius`, `EndRadius`, `bForceRadial` |
| `S_VekaColoraGradientMatrix` | `FVekaMatrixGradient` | `Angle`, `Blend`, `DistortionScale`, `DistortionStrength`, `FillBlur`, `bFillEmptySpace`, `Seed`, `bUseOkLCH` |
| `S_VekaColoraGradient` | `FVekaGradient` | `Type`, `Linear`, `Angular`, `Radial`, `Matrix`, `CenterPoint` |
| `S_VekaColoraSettings` | `FVekaColoraSettings` | `Mode`, `Gradient`, `Mix` (float), `Stops` |
| `S_VekaEnvironnementThemes` / `…Colora` | *(supprimés)* | remplacés par `UVekaSettings` (§6.3) |
| `S_VekaAnimaState` | `FVekaAnimationState` (interne) | `Duration`, `Delay`, `Repeat`, `bAutoReverse`, `Speed`, `Easing`, `CustomCurve`, état courant |
| `S_VekaFontAppearance` | `FVekaFontAppearance` | `Typeface`, `bItalic`, `LineSpacing`, `TextCase` |
| `S_VekaFontTypeface` | `FVekaTypefaceEntry` | `Typeface`, `Font` |
| `S_VekaGlyph` | `FVekaGlyphRef` | `Preset`, `CustomGlyph` (`TObjectPtr<UVekaGlyph>`) |
| `S_VekaGlyphAppearance` | `FVekaGlyphAppearance` | `bFilled`, `bGradient`, `Primary`, `Secondary`, `Tertiary` (`FVekaColorRef`), `Rendering`, `Variant`, `VariantValue` |
| `S_VekaBadge` | `FVekaBadge` | `Icon`, `IconColor`, `BadgeColor`, `bGradient`, `OverrideIconSize` |
| `S_VekaCustomBadge` | `FVekaCustomBadge` | `Badge`, `Icon`, `BadgeColor`, `BadgeSize`, `Position` (`FVector2D`), `bGradient` |
| `S_VekaMenuSection` | `FVekaMenuSection` | `Id` (`FName`), `Label` (`FText`), `Type`, `PickerOptions`, `bShowPickerGlyph`, `SubMenu` (`TArray<FVekaMenuSection>` via index/ID), `Glyph` |
| `S_VekMenuSubMenu` | `FVekaSubMenu` | `TArray<FVekaMenuSection> Sections` |
| `S_VekaShapeCustomPoint` | *(fusionné)* | `TArray<FVector2D> CustomPoints` (max 16) |
| `S_PathInstruction` | `FVekaPathCommand` | `Command`, `Points`, `Color` |
| `S_SubPath` | `FVekaSubPath` (interne) | `Points`, `Color` |
| `S_VekaEditorToolBar` | *(éditeur, supprimé)* | géré par `FVekaEditorModule` |
| `S_VekaEditorDirtySymbols` | *(éditeur, interne)* | `TSet<TObjectPtr<UVekaGlyph>>` |
| `Veka_Overridable*` (13) | **pattern UE** `bOverride_X` + `X` | `UPROPERTY(meta=(InlineEditConditionToggle))` comme `FPostProcessSettings` |

### 5.3 Data assets

| Blueprint | C++ (`UPrimaryDataAsset`) | Notes |
|---|---|---|
| `Veka_Themes` | `UVekaTheme` | couleurs par rôle + sous-thèmes + police ; `GetColor(EVekaThemeColorRole, EVekaComponent)` remplace les 9 `Select*Color` |
| `Veka_ThemesGlyph` | `UVekaGlyphTheme` | |
| `Veka_ThemesBadgePreset` | `UVekaBadgeTheme` | `TMap<EVekaBadgePreset, TObjectPtr<UVekaGlyph>>` |
| `Veka_ThemesMenu` | `UVekaMenuTheme` | |
| `Veka_ThemesPicker` | `UVekaPickerTheme` | |
| `Veka_ThemesSlider` | `UVekaSliderTheme` | |
| `Veka_ThemesToggle` | `UVekaToggleTheme` | |
| `Veka_ColoraPreset` | `UVekaColorPalette` | `TMap<EVekaColorPreset, FVekaColor>` + `TMap<EVekaHierarchy, float>` |
| `Veka_Font` | `UVekaFontFamily` | `TMap<EVekaTypeface, TObjectPtr<UFont>>` + italiques |
| `Veka_Glyph` | `UVekaGlyph` | `IconFill`, couleurs multicolor, badge par défaut ; `AssetBundle` pour chargement async |

Sous-thèmes : `UPrimaryDataAsset` séparés (réutilisables) **ou** structs inline dans
`UVekaTheme` — choix §11 (recommandé : structs inline, moins d'assets).

### 5.4 Objets, librairies, interfaces

| Blueprint | C++ |
|---|---|
| `Veka_Environnement` | `UVekaSettings` (`UDeveloperSettings`, Project Settings › Plugins › Veka) + `UVekaSubsystem` (`UEngineSubsystem`) |
| `Veka_Colora` | `FVekaColora` (helpers statiques) : `ApplyToMaterial(UMaterialInstanceDynamic*, const FVekaColoraSettings&, bool bDark)` |
| `Veka_Anima` | `FVekaAnimator` (struct possédée par le widget) + `VekaEasing::Evaluate(EVekaEasing, float)` |
| `BFL_Veka` | `UVekaBlueprintLibrary` (`UBlueprintFunctionLibrary`) |
| `Veka_MacroExtensions` | supprimé |
| `BPI_VekaColora` | `IVekaColoraTarget` (`UINTERFACE`, optionnel) ou API directe sur `UVekaShape`/`UVekaText` |
| `BPI_Toggle`, `BPI_VekaPicker`, `BPI_VekaGauge`, `BPI_Progress`, `BPI_VekaColorPicker` | supprimées : la variante est un détail interne du widget C++ |

### 5.5 Widgets runtime

Chaque widget public = une classe UMG `UVeka*` (dérivée de `UWidget`/`UContentWidget`/
`UPanelWidget`) qui construit un widget Slate `SVeka*` dans `RebuildWidget()` et pousse
ses propriétés dans `SynchronizeProperties()`.

| Blueprint | UMG (`UVeka…`) | Slate (`SVeka…`) | Type |
|---|---|---|---|
| `Veka_WidgetCore` | *(pas de classe UMG commune)* `FVekaWidgetStyleContext` + mixin | `SVekaWidgetBase` | base |
| `Veka_Interactor` | intégré à `UVekaShape` (+ `UVekaInteractionArea` si besoin seul) | `SVekaInteractor` / `FVekaSDF` | primitive |
| `Veka_Shape` | `UVekaShape` (`UContentWidget`) | `SVekaShape` | primitive |
| `Veka_Text` | `UVekaText` | `SVekaText` (sur `STextBlock`) | primitive |
| `Veka_Symbol` | `UVekaSymbol` | `SVekaSymbol` | primitive |
| `PathBuilderUI` / `PathUI` | `UVekaPath` | `SVekaPath` (`OnPaint`, `MakeLines`/`MakeCustomVerts`) | primitive |
| `Veka_Divider` | `UVekaDivider` | `SVekaDivider` | simple |
| `Veka_Label` | `UVekaLabel` | `SVekaLabel` | composite |
| `Veka_Button` | `UVekaButton` | `SVekaButton` | composite |
| `Veka_KeyBindButton` | `UVekaKeyBindButton` *(à définir — vide en Alpha 13)* | — | composite |
| `Veka_Toggle` + `Veka_Switch`, `Veka_CheckBox`, `Veka_ToggleButton` | `UVekaToggle` (`ToggleType`) | `SVekaSwitch`, `SVekaCheckBox`, `SVekaToggleButton` | famille |
| `Veka_Slider` | `UVekaSlider` | `SVekaSlider` | composite |
| `Veka_ColorSlider` | `UVekaColorSlider` | `SVekaColorSlider` | composite |
| `Veka_Stepper` | `UVekaStepper` | `SVekaStepper` | composite |
| `Veka_Picker` + Menu, Segmented, Palette, Radio, MenuRow, PaletteColumn, RadioButton, PickerButton | `UVekaPicker` (`PickerType`) | `SVekaPickerMenu`, `SVekaPickerSegmented`, `SVekaPickerPalette`, `SVekaPickerRadio` (+ items) | famille |
| `Veka_Menu`, `Veka_MenuButton`, `Veka_MenuRow` | `UVekaMenu` | `SVekaMenu`, `SVekaMenuRow`, `SVekaMenuButton` | composite |
| `Veka_ContextMenu` | `UVekaContextMenu` + `UVekaBlueprintLibrary::OpenContextMenu` | `SVekaContextMenu` (via `FSlateApplication::PushMenu`) | composite |
| `Veka_ColorPicker` + `Veka_ColorPickerMenu` + 8 modes | `UVekaColorPicker` | `SVekaColorPicker`, `SVekaColorPickerPopup`, `SVekaColorPicker{RGB,HSV,OkLCH,Hue,Luminosity,Saturation,Grayscale,CMYK}` | famille |
| `Veka_Gauge` + 4 variantes | `UVekaGauge` (`GaugeType`) | `SVekaGaugeLinear`, `…LinearAlt`, `…Circular`, `…CircularAlt` | famille |
| `Veka_Progress` + 3 variantes | `UVekaProgress` (`ProgressType`) | `SVekaProgressLinear`, `…Circular`, `…Lazy` | famille |
| `Veka_Image` | `UVekaImage` | `SVekaImage` | composite |
| `Veka_Media` | `UVekaMedia` | `SVekaMedia` | composite |
| `Veka_Web` | `UVekaWeb` (module optionnel `VekaWeb` dépendant de `WebBrowserWidget`) | `SWebBrowser` | composite |
| `Veka_TextField` | `UVekaTextField` | `SVekaTextField` (sur `SEditableTextBox`) | composite |
| `Veka_Effect` | `UVekaEffect` (`UContentWidget`) | `SVekaEffect` (sur `SRetainerWidget`) | composite |
| `Veka_3DViews` | `UVekaExplodedView` (`UPanelWidget`) | `SVekaExplodedView` | panel |
| `Veka_Unavailable` | `UVekaUnavailable` | `SVekaUnavailable` | composite |
| `FPS` | supprimé (démo) | — | — |

### 5.6 Outils éditeur (`VekaEditor`)

| Blueprint | C++ |
|---|---|
| `EUB_Veka_AddToolBar`, `EUB_Veka_RemoveToolbar`, `EUB_Veka_Button`, `EUB_Veka_LevelEditor(Button)`, `EUB_Veka_WidgetBlueprintEditor(Button)` | `FVekaEditorModule::RegisterMenus()` via `UToolMenus` (LevelEditor.LevelEditorToolBar + toolbar du Widget Blueprint Editor), `FVekaEditorCommands` (`TCommands`), `FVekaEditorStyle` (`FSlateStyleSet`, icône `Resources/Icon128.png`) |
| `EUW_EditorVeka` + `Veka_EditorDocs*`, `Veka_EditorDesignLab*`, `Veka_EditorAbout`, `Veka_EditorTitle`, `Veka_EditorBackground` | onglet nomade `SVekaDocumentationTab` (Slate, utilise les `SVeka*` runtime) |
| `EUW_VekaEditorGlyphLibrary` + `Veka_EditorGlyphLibrary`, `…GlyphPreview`, `…GlyphEditor` | onglet `SVekaGlyphLibraryTab` (Asset Registry + `FScopedTransaction` + `MarkPackageDirty`) |
| `EUW_VekaEditorGradientMatrix` + `Veka_EditorGradientPoint` | onglet `SVekaGradientMatrixTab` |
| *(nouveau)* | `IDetailCustomization` / `IPropertyTypeCustomization` pour `FVekaColoraSettings`, `FVekaColorRef`, `FVekaGlyphRef` (aperçu couleur/glyphe) |
| *(nouveau)* | `UThumbnailRenderer` pour `UVekaGlyph` (optionnel) |

---

## 6. Architecture cible

### 6.1 Arborescence

```
Veka/
├─ Veka.uplugin                (Modules: Veka Runtime, VekaEditor Editor[, VekaWeb Runtime])
├─ Config/
│  └─ DefaultVeka.ini           (réglages par défaut, CoreRedirects si utiles)
├─ Content/                     (assets non-logiques uniquement)
│  ├─ Materials/  Fonts/  Glyphs/  Textures/
│  └─ Defaults/   (DA_VekaTheme_Dark, DA_VekaTheme_Light, DA_VekaPalette_System, …)
├─ Resources/                   (icônes éditeur uniquement)
├─ Docs/                        (ce document, TODO)
└─ Source/
   ├─ Veka/
   │  ├─ Veka.Build.cs
   │  ├─ Public/
   │  │  ├─ Veka.h
   │  │  ├─ Core/        VekaSettings.h, VekaSubsystem.h, VekaTypes.h, VekaEnums.h
   │  │  ├─ Theme/       VekaTheme.h, VekaColorPalette.h, VekaFontFamily.h, VekaGlyph.h
   │  │  ├─ Colora/      VekaColoraTypes.h, VekaColora.h
   │  │  ├─ Animation/   VekaAnimator.h, VekaEasing.h
   │  │  ├─ Widgets/     UVeka*.h (UMG)
   │  │  ├─ Slate/       SVeka*.h
   │  │  └─ Library/     VekaBlueprintLibrary.h
   │  └─ Private/        (même découpage, + Tests/)
   ├─ VekaEditor/
   │  ├─ VekaEditor.Build.cs
   │  ├─ Public/ Private/   (Module, Commands, Style, Tabs/, Customizations/, Migration/)
   └─ VekaWeb/            (optionnel : isole la dépendance WebBrowserWidget)
```

### 6.2 Modules et dépendances

| Module | Type / LoadingPhase | Dépendances principales |
|---|---|---|
| `Veka` | Runtime / `PreDefault` (dispo pour le Designer UMG) | Core, CoreUObject, Engine, Slate, SlateCore, UMG, InputCore, DeveloperSettings, MediaAssets (pour Media), RenderCore (si besoin) |
| `VekaWeb` | Runtime / Default (optionnel) | Veka, WebBrowser, WebBrowserWidget |
| `VekaEditor` | Editor / Default | Veka, UnrealEd, ToolMenus, EditorStyle/EditorFramework, PropertyEditor, AssetRegistry, Projects, WorkspaceMenuStructure, UMGEditor |

Le runtime ne dépend **jamais** d'un module éditeur (code `WITH_EDITOR` seulement pour les
métadonnées et la preview).

### 6.3 Environnement global : `UVekaSettings` + `UVekaSubsystem`

- `UVekaSettings : UDeveloperSettings` (`config=Veka, defaultconfig`) :
  `TSoftObjectPtr<UVekaTheme> DarkTheme`, `LightTheme`,
  `TSoftObjectPtr<UVekaColorPalette> Palette`, `bool bUseDarkTheme`, `FString BuildName`
  (« Alpha 13 » → version du `.uplugin`).
- `UVekaSubsystem : UEngineSubsystem` : charge et cache le thème actif, expose
  `GetActiveTheme()`, `GetPalette()`, `IsDark()`, `SetDarkMode(bool)`, `SetTheme(UVekaTheme*)`
  et le delegate `OnThemeChanged`. Marche en éditeur, en PIE et en jeu packagé, **sans
  PlayerController**.
- Chaque widget peut surcharger le thème (`TObjectPtr<UVekaTheme> ThemeOverride`).

### 6.4 Widgets : UMG + Slate

- Primitives et composites sont des **widgets Slate natifs** enveloppés par une classe UMG.
  Pas de Widget Blueprint requis. L'utilisateur peut quand même faire un WBP qui *contient*
  des widgets Veka.
- `RebuildWidget()` crée le `SVeka*`, `SynchronizeProperties()` pousse les `UPROPERTY`,
  `ReleaseSlateResources()` libère.
- Les attributs Slate utilisent `TSlateAttribute` / `SLATE_ATTRIBUTE` pour rester
  compatibles avec l'invalidation (`EInvalidateWidgetReason`).
- L'aperçu Designer (`IsDesignTime()`) remplace la logique `PreConstruct` des WBP.
- Les familles à variantes (Toggle, Picker, Gauge, Progress, ColorPicker) changent de
  sous-widget Slate quand l'enum change : plus de `CreateWidget` + interface BP.
- Rendu des formes : brush matériau (`FSlateMaterialBrush`/`FSlateBrush` avec
  `UMaterialInstanceDynamic`) dessiné par `FSlateDrawElement::MakeBox`. Le MID est créé
  une fois par widget et mis à jour seulement quand une propriété change.
- Hit-test : `FVekaSDF` (fonctions C++ pures : Circle, Box arrondi uneven, Capsule,
  Ellipse, polygone Custom, « continuous corners ») utilisé dans `OnMouseButtonDown`,
  `OnMouseMove`, etc. Même formules que les material functions `MF_*SDF`.

### 6.5 Animation : `FVekaAnimator`

- Struct possédée par le widget Slate, pilotée par `RegisterActiveTimer` : le timer existe
  seulement pendant une animation, puis se retire (`EActiveTimerReturnType::Stop`).
- Paramètres : durée, délai, répétitions, auto-reverse, vitesse, easing, courbe custom
  (`UCurveFloat`), spring (`FFloatSpringState` : stiffness, damping, mass).
- `VekaEasing::Evaluate(EVekaEasing, Alpha)` : fonctions pures, testées unitairement.
- Events UMG : `OnAnimate(float Value)`, `OnAnimateFinished()`.

### 6.6 Colora : `FVekaColora`

- Fonctions statiques sans état : résolution des couleurs (preset + hiérarchie +
  dark/light), conversion position preset → UV, création de stops, puis
  `ApplyToMaterial(MID, Settings, bDark)`.
- Les **noms de paramètres matériau** sont centralisés dans `VekaMaterialParams.h`
  (`static const FName`) et doivent correspondre exactement aux matériaux existants
  (ex. `ColoraMode`, `GradientType`, `Color1..Color9`, `LinearStartPoint`,
  `LinearEndPoint`, `AngularStartAngle`, `AngularEndAngle`, `CenterPoint`,
  `RadialStartRadius`, `RadialEndRadius`, `ForceRadial`, `MatrixSeed`,
  `MatrixDistortionScale`, `MatrixDistortionStrength`, `MatrixDistortionBlend`,
  `FillEmptySpace`, `Use OKLCH Color`, `WidgetSize`, `Radius`, `Continuous`, `Shape`,
  `HighlightColor`, `HighlightSize`, `CustomNumPoints`… — liste exacte à extraire, TODO P0).
- `HexToColor`, `GetDominantColor`, conversions RGB/HSV/OkLCH/CMYK en C++ pur
  (`VekaColorMath.h`), testées.

### 6.7 Overridable

Pattern moteur standard (comme `FPostProcessSettings`) :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veka|Style", meta=(InlineEditConditionToggle))
uint8 bOverride_Color : 1;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veka|Style", meta=(EditCondition="bOverride_Color"))
FVekaColorRef Color;
```

Résolution centralisée : `Theme->Resolve(bOverride_Color, Color, EVekaThemeColorRole::Accent, EVekaComponent::Button)`.

### 6.8 Exposition Blueprint

- Toutes les fonctions publiques listées en annexe D : `UFUNCTION(BlueprintCallable,
  Category="Veka|<Widget>")`, getters `BlueprintPure`.
- Propriétés : `EditAnywhere, BlueprintReadWrite` + `BlueprintSetter`/`BlueprintGetter`
  quand un setter doit invalider le Slate.
- Events : `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` + `UPROPERTY(BlueprintAssignable)`
  (OnClicked, OnDoubleClicked, OnPressed, OnReleased, OnHovered, OnUnhovered,
  OnRightClicked, OnValueChanged, OnCommitted, OnSelectionChanged, OnToggled, OnColorChanged,
  OnContextMenuSelected, OnUrlChanged, OnMediaOpened, OnAnimate, OnAnimateFinished).
- Catégorie de palette UMG : `Veka`.

### 6.9 Menus et menu contextuel

`FVekaMenuSection` décrit l'arbre (boutons, séparateurs, labels, pickers, sous-menus).
Ouverture via `FSlateApplication::PushMenu` / `MenuAnchor` Slate, placement
(`CenteredAboveAnchor`, `CenteredBelowAnchor`, `MenuRight`, `Center`) conservé.
Fermeture au clic extérieur et à Échap. Animation open/close via `FVekaAnimator`
(remplace les `WidgetAnimation` OpenAnim/CloseAnim).

### 6.10 Interfaces

Les BPI par famille disparaissent (détail interne). On garde au plus un
`UINTERFACE(BlueprintType)` `IVekaColoraTarget` si on veut piloter Colora de façon
générique depuis Blueprint. Décision §11.

### 6.11 Éditeur

- Bouton / combo « Veka » dans la toolbar Level Editor et Widget Blueprint Editor :
  Documentation, Glyph Library, Gradient Matrix Editor, About.
- Onglets nomades Slate (`FGlobalTabmanager::RegisterNomadTabSpawner`) dans
  `Tools › Veka`.
- Glyph Library : liste les `UVekaGlyph` via `IAssetRegistry::GetAssetsByClass`,
  édite avec `FScopedTransaction` (Undo/Redo), marque dirty, bouton Apply/Reset.
- Gradient Matrix Editor : 9 points draggables, sliders, auto-play du seed, copie des
  réglages (`FPlatformApplicationMisc::ClipboardCopy` d'un texte importable).
- Personnalisations de détails pour les structs Colora/Glyph.
- Outil de migration (commandlet ou action de menu) — cf. §8.

---

## 7. Exigences

### 7.1 Exigences fonctionnelles (parité)

| ID | Exigence |
|---|---|
| EF-01 | Thème global dark/light réglable dans Project Settings et changeable à chaud ; tous les widgets se mettent à jour (`OnThemeChanged`). |
| EF-02 | Palette Colora : 26 presets × dark/light, 5 niveaux de hiérarchie, couleur manuelle. |
| EF-03 | Colora : modes Color / Gradient / Mix ; dégradés Linear, Angular, Radial, Matrix ; jusqu'à 9 stops ; positions preset (9) ou manuelles ; OkLCH. |
| EF-04 | Formes : 7 modes, radius / radii, continuous, outline (Fill/Outline/Both + épaisseur), highlight, masque de contenu, padding et alignement du contenu, matériau d'effet. |
| EF-05 | Hit-test exact sur la forme (pas sur le rectangle), events souris complets dont double-clic et clic droit, capture pendant le drag. |
| EF-06 | Texte : police de thème, 12 graisses + italique, casse, troncature (fin/milieu), wrap, justification, line spacing, mode redacted, couleur Colora. |
| EF-07 | Symboles : presets (Info, Warning, Error, Check, Custom, None), 4 rendering modes, 4 variantes + valeur, gradient, badge preset ou custom (taille, position, couleur). |
| EF-08 | Label, Button (4 styles, désactivé, glyphe + texte, tous les events), Toggle (3 types × 3 comportements, radio groupé), Slider, ColorSlider, Stepper (répétition à l'appui long), Divider. |
| EF-09 | Picker 4 types avec options, index, label mode, orientation radio, taille ; Segmented animé. |
| EF-10 | Menu et menu contextuel avec sections Button/Divider/Label/Picker/SubMenu, glyphes, placement. |
| EF-11 | Color Picker : popup, 8 modes (LCH/CMYK au minimum au niveau Alpha 13 = « Unavailable »), champ hex, opacité, plages RGB 0–1 / 0–255 / %, OK/Annuler. |
| EF-12 | Gauge 4 variantes et Progress 3 variantes : min/max/valeur, pourcentage, titre, couleurs, glyphe. |
| EF-13 | Image (fit/fill/none, ratio, 9-slice, glyphe), Media (source, autoplay, loop), Web (URL, HTML, JS, events), TextField (4 types, 4 styles, hint, commit), Effect (matériau retainer), ExplodedView, Unavailable, Path vectoriel. |
| EF-14 | Animation générique (easings, spring, courbe custom, délai, repeat, auto-reverse) utilisable par tous les widgets. |
| EF-15 | Outils éditeur : toolbar Veka, Documentation + Design Lab + About, Glyph Library, Gradient Matrix Editor. |
| EF-16 | Librairie Blueprint : création de gradients/stops/mix, hex → couleur, couleur dominante, helpers menu, position preset → vecteur. |

### 7.2 Exigences non fonctionnelles

| ID | Exigence |
|---|---|
| ENF-01 | **Aucun** asset Blueprint (classe, interface, struct, enum, widget, EUB, EUW) dans la branche `C++` à la livraison. |
| ENF-02 | Pas de `Tick` permanent ; animations par active timer ; un widget au repos coûte 0 ms de logique par frame. |
| ENF-03 | Pas d'`UObject` créé par widget hors MID (et MID réutilisé). |
| ENF-04 | Compatible invalidation / Global Invalidation Slate. |
| ENF-05 | Compile sans warning en Development et Shipping, Win64 (+ Mac/Linux si dispo), avec `bUseUnity=false` en CI pour vérifier l'IWYU. |
| ENF-06 | Packaging : le plugin se package (`RunUAT BuildPlugin`) ; aucun chemin `Resources/` utilisé au runtime. |
| ENF-07 | Code conforme au Coding Standard Epic ; `TObjectPtr`, `TSoftObjectPtr`, pas de raw `UObject*` en `UPROPERTY`. |
| ENF-08 | Textes affichés en `FText` (localisables) ; noms internes en `FName`. |
| ENF-09 | Designer UMG : chaque widget visible dans la palette « Veka », aperçu correct à la conception, tooltips sur les propriétés. |
| ENF-10 | Accessibilité : focus clavier/gamepad sur les widgets interactifs, `AccessibleText`. |
| ENF-11 | Tests d'automatisation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST` / Spec) pour : easings, SDF hit-test, conversions couleur, hex, résolution de thème, Colora → MID. |
| ENF-12 | Documentation : en-têtes commentés, README d'utilisation, CHANGELOG des renommages. |

---

## 8. Stratégie de migration

### 8.1 Phases (détail dans `TODO.md`)

| Phase | Contenu | Sortie |
|---|---|---|
| P0 | Préparation : décisions §11, extraction exacte des paramètres matériaux et des valeurs des data assets, squelette des modules, CI | plugin vide qui compile |
| P1 | Socle : enums, structs, data assets, settings, subsystem, Colora, animation, librairie, maths couleur/SDF + tests | socle testé |
| P2 | Assets non-logiques : ré-ajout matériaux/MF/textures/polices dans `Content/`, recréation des data assets par défaut sur les classes C++ | thème Dark/Light fonctionnel |
| P3 | Primitives : Shape (+SDF/Interactor), Text, Symbol, Path, Divider | primitives dans le Designer |
| P4 | Composites simples : Label, Button, Toggle (3), Slider, ColorSlider, Stepper, TextField, Image, Unavailable, Effect | |
| P5 | Composites avancés : Picker (4), Menu / ContextMenu, Gauge (4), Progress (3), ColorPicker (8 modes), ExplodedView, Media, Web | parité runtime |
| P6 | Éditeur : toolbar, Documentation/Design Lab/About, Glyph Library, Gradient Matrix, customizations, migration | parité éditeur |
| P7 | Qualité : perf, packaging, tests, docs, nettoyage, tag de version | release « Alpha 14 (C++) » |

### 8.2 Migration des données

Les instances de data assets (thèmes, palette, glyphes, police) ont des valeurs à garder.
Les UserDefinedStruct BP ne peuvent pas être redirigés proprement vers des `USTRUCT` C++
(noms de champs suffixés `_N_GUID`). Méthode :

1. Sur la branche `Blueprint` (le BP est encore là), exporter les valeurs avec un script
   Python éditeur (`unreal.EditorAssetLibrary` + `get_editor_property`) en JSON.
2. Sur la branche `C++`, un commandlet / script Python lit le JSON et crée les nouveaux
   assets (`DA_VekaTheme_Dark`, `DA_VekaTheme_Light`, `DA_VekaPalette_System`,
   `DA_VekaFont_LTFlode`, `DA_VekaGlyph_*`).
3. Vérification visuelle côte à côte (captures de la page Documentation).

### 8.3 Assets non-logiques à ré-ajouter

La branche `C++` a supprimé tout `Content/`. À remettre (depuis `Blueprint`) :
matériaux (14), instances (14), material functions (20 + 11 instances), polices (2 + 25
faces), textures (glyphes `Content/Glyphs/*`, logos, wallpapers, grid). Les matériaux ne
contiennent pas de logique BP mais certains utilisent des nœuds `Custom` (HLSL) et
`E_VekaBooleanMaterial` : vérifier qu'aucun ne référence un asset BP supprimé.

### 8.4 Rupture pour les utilisateurs

- Les WBP utilisateurs qui contiennent des widgets Veka BP devront les remplacer par les
  widgets C++ (même noms affichés, propriétés proches). Fournir un tableau de
  correspondance (annexe D + §5) dans le CHANGELOG.
- `CoreRedirects` dans `Config/DefaultVeka.ini` pour les cas possibles (ex. anciens noms
  d'enum C++ si on renomme plus tard). Les classes BP → C++ ne sont pas redirigeables
  automatiquement : documenté.

---

## 9. Critères d'acceptation

1. La branche `C++` ne contient aucun asset de type Blueprint (vérifié par un script
   qui scanne `Content/` : aucun `BlueprintGeneratedClass`, `UserDefinedStruct`,
   `UserDefinedEnum`, `WidgetBlueprint`, `EditorUtility*`).
2. `RunUAT BuildPlugin` réussit pour Win64 Development + Shipping, sans warning.
3. Tous les tests d'automatisation `Veka.*` passent.
4. La page Documentation (onglet éditeur Slate) montre chaque composant listé en §5.5,
   avec les mêmes options que les onglets Alpha 13 ; captures comparées dark et light.
5. Un widget de démo en jeu packagé fonctionne (clic, hover, slider, picker, menu
   contextuel, color picker, animation, changement de thème à chaud).
6. Profilage : écran de 200 widgets Veka au repos → aucune fonction Veka dans
   `Stat Slate`/Insights hors paint ; pas de GC lié à Veka.
7. Glyph Library et Gradient Matrix Editor éditent et sauvent des assets avec Undo/Redo.

---

## 10. Risques

| Risque | Impact | Parade |
|---|---|---|
| Logique cachée dans les graphs BP non lisible hors éditeur (formules SDF, mapping de couleurs, ordre des paramètres) | régressions visuelles | ouvrir chaque BP dans l'éditeur sur `Blueprint` pendant le portage ; comparer en captures ; tests SDF |
| Noms de paramètres matériaux mal recopiés | widgets vides/blancs | constantes centralisées + test qui vérifie que chaque nom existe dans le matériau (`GetAllScalarParameterInfo`) |
| Valeurs des data assets perdues | thème différent | export JSON avant suppression (§8.2) |
| Version moteur (5.7 vs 5.8) | assets non chargeables | fixer la version cible en P0, re-sauver les assets dans cette version |
| `WebBrowserWidget` alourdit le runtime | taille build, plateformes | module séparé `VekaWeb` optionnel |
| Glyphes en PNG dans `Resources/` | absents en jeu packagé | les garder en `UTexture2D` dans `Content/Glyphs` |
| Charge de travail ColorPicker (8 modes) et menus | retard | livrer RGB/HSV/Hue/Sat/Lum/Gray d'abord ; LCH/CMYK restent « Unavailable » comme en Alpha 13 |
| Rupture pour projets utilisateurs | adoption | CHANGELOG + tableau de correspondance + guide de migration |

---

## 11. Décisions à prendre (proposition par défaut en gras)

1. **Widgets Slate natifs enveloppés UMG** vs classes C++ `UUserWidget` construisant un
   `WidgetTree` en code. (Slate : plus rapide, vrai « full C++ » ; UUserWidget : portage
   plus direct depuis les WBP.)
2. Sous-thèmes : **structs inline dans `UVekaTheme`** vs data assets séparés.
3. Matériaux : **garder les assets existants** vs portage `.usf` (plus tard).
4. Version moteur cible : **5.8** (comme le `.uplugin`) vs 5.7.
5. Module `VekaWeb` séparé : **oui**.
6. Correction des noms (ex. `Environnement` → `Environment`, `Prominant` → `Prominent`) :
   **oui**, avec `DisplayName` conservé si besoin.
7. `IVekaColoraTarget` : **non** au départ (API directe sur les widgets).
8. Onglets de doc encore vides (Alert, TabView, …) : **hors périmètre**.

---

## Annexe A — Inventaire des fichiers analysés

Dossier `Content/` de la branche `Blueprint` (335 `.uasset`) :
`Animation/` (Anima + 3 enums + 1 struct), `Colora/` (Colora, preset, BPI, 4 enums,
13 structs, 10 matériaux/MF), `Themes/` (Veka_Themes + 6 sous-thèmes + instances
Default/LightDefault), `Fonts/` (Veka_Font, LTFlode, Redacted, enum, 2 structs,
matériau redacted), `Glyphs/` (Veka_Glyph + 19 glyphes + textures), `Widgets/` (tous les
composants, enums, structs, matériaux), `Utility/` (docs, menus éditeur, glyph library,
gradient matrix, overridable variables, misc), racine (`BFL_Veka`, `Veka_MacroExtensions`,
`Veka_Environnement`, `E_VekaPosition`, `S_VekaEnvironnement*`, `MF/MFI_VekaCircleProgress`,
`FPS`), `Logo/`. Plus `Resources/` (icônes, logos, wallpapers, keyline grid).

## Annexe B — Hiérarchie de dépendances des widgets (extrait)

```
Shape ← Interactor
Text ; Symbol ← Glyph
Label ← Text + Symbol
Button ← Shape + Label
Divider ← Shape
Toggle ← {Switch ← Shape+Text, CheckBox ← Shape+Symbol+Text, ToggleButton ← Button+Label}
Slider ← Shape + Interactor + Label + Progress ; ColorSlider ← Slider + TextField + Progress
Stepper ← Button + Shape + Text
Picker ← {Menu ← PickerMenuRow ← PickerButton ← Button, Segmented ← PickerButton + Anima,
          Palette ← PaletteColumn ← PickerButton, Radio ← RadioButton ← Shape+Text}
Menu ← MenuRow ← MenuButton ← Shape+Label+Symbol (+ContextMenu pour les sous-menus)
ColorPicker ← ColorPickerMenu ← {RGB, HSV, LCH, Hue, Luminosity, Saturation, Grayscale, CMYK} ← ColorSlider
Gauge ← {Linear ← ProgressView, LinearAlt ← Shape, Circular ← Progress, CircularAlt ← ProgressCircular}
Progress ← {ProgressView, ProgressCircular, ProgressLazy}
```

## Annexe C — Matériaux runtime

| Matériau | Usage |
|---|---|
| `M_VekaShapes` / `MI_VekaShapes` | formes SDF + Colora + highlight + glow (`Custom` HLSL) |
| `M_VekaShapesMask` / `MI_VekaShapesMask` | masque du contenu |
| `M_VekaText` / `MI_VekaText` | matériau de police Colora |
| `M_VekaFontRedacted` | texte masqué |
| `M_VekaGlyph`, `M_VekaGlyphMaskToIcon` | glyphes (masques RGBA, badge, variantes) |
| `M_VekaProgressView(+Background)`, `M_VekaProgressCircular(+Background)` | barres / anneaux |
| `M_VekaColorPickerSlider` | piste dégradée des sliders couleur |
| `M_SlicerImage` | image 9-slice |
| `M_VekaMedia` | texture média dans une forme |
| `M_SDFPreview` | aperçu debug du hit-test |
| MF : `MF_SDF`, `MF_BoxSDF`, `MF_CircleSDF`, `MF_CapsuleSDF`, `MF_EllipseSDF`, `MF_RoundedSDF`, `MF_CustomSDF`, `MF_HighlightSDF`, `MF_VekaColora*`, `MF_VekaGradientMatrix`, `MF_VekaLinearGradient2/3`, `MF_VekaSimpleGradient`, `MF_VekaBadge`, `MF_VekaProgressView`, `MF_VekaCircleProgress` | |

## Annexe D — API publique Alpha 13 à reproduire (par widget)

Extraite des graphs Blueprint (fonctions `Set*`/`Get*`/events). Les noms C++ peuvent être
simplifiés mais chaque capacité doit exister.

| Widget | API |
|---|---|
| WidgetCore | ActivateAnima, DeactivateAnima, CreateContextMenu, GetWidgetPosition, GetWidgetSize, GetGlyphPresetByThemes, SetColoraSettings ; events OnAnimate, OnAnimateFinished, OnContextMenuSelected |
| Interactor | SetShape, SetSize, SetRadius, SetRadii, SetContinuous, SetCustomShapePoint, SetOutline, SetOutlineThickness, SetPadding, SetContentPadding, SetContentAlignment, SetSDFPreview, SetSDFPreviewColor, AutoRadius, AutoRadii ; OnClick, OnDoubleClick, OnPress, OnRelease, OnHover, OnUnhover, OnRightClick |
| Shape | + SetColor, SetColoraSettings, SetColoraMatrix, SetGradientMode, SetGradientMatrixPoint, SetHighlight, SetHighlightColor, SetHighlightThickness, SetMaskContentSlot, SetEnabled, GetColor, GetRadius, GetRadii, GetShape, GetSize, GetEffectMaterial |
| Text | SetText, SetFont, SetFontWeight, SetBold, SetItalic, SetTextCase, SetTruncation, SetWrap, SetJustification, SetAlignment, SetLineSpacing, SetSize, SetColor, SetColoraSettings, SetRedacted, SetFontMaterial, SetTextOverflowPolicy, SetTextTransformPolicy |
| Symbol | SetGlyph, SetAppearance, SetFilled, SetGradient, SetVariant, SetVariantValue, SetSize, SetMonochromeColor, SetHierarchicalColor, SetMulticolorColor, SetPaletteColor, SetBadge, SetCustomBadge, SetBadgeSize, SetBadgePosition, GetGlyph, GetAppearance, GetRendering |
| Label | SetLabel, SetText, SetGlyph, SetAppearance, SetSymbolAppearance, SetFont, SetFontWeight, SetItalic, SetTextCase, SetColor, SetFilled, SetVariant, SetSize, SetPadding |
| Button | SetText, SetLabel, SetGlyph, SetGlyphAndAppearance, SetSymbolAppearance, SetStyle, SetColor, SetFont, SetFontWeight, SetFilled, SetVariant, SetSize, SetRadius, SetDisabled ; OnClicked, OnDoubleClicked, OnPressed, OnReleased, OnHovered, OnUnhovered |
| Toggle (+Switch/CheckBox/ToggleButton) | SetIsOn, SetToggleType, SetBehavior, SetColor, SetActiveColor, SetText, SetShowLabel, SetSize, SetGlyph (checkbox), SetShape ; IsOn (event), OnRadioActivated |
| Slider | SetValue, SetMinValue, SetMaxValue, SetStep, SetMaxDigits, SetLabel, SetMinimalLabel, SetMaximalLabel, SetShowLabel, SetSuffix, SetText, SetTextColor, SetFont, SetFontWeight, SetGlyph, SetSymbolAppearance, SetColor, SetFillColor, SetBackgroundColor, SetKnobColor, SetSize, SetPadding, SetContentPadding ; OnValueUpdated |
| ColorSlider | SetValue, SetMaximalValue, SetColor, SetColorA/B/C, SetHueMode, SetHueAlpha, SetBackgroundColor, SetOutline, SetOutlineThickness, SetText, SetHintText, SetJustification, InitGradient ; OnChange, OnCommit |
| Stepper | SetValue, SetMinValue, SetMaxValue, SetStep, SetText, SetDisabled ; OnValueUpdated (+ press/release/click/double-click) |
| Picker (4 types) | SetPickerType, SetOptions, SetPickerIndex, SetLabelMode, SetRadioOrientation, SetSize, SetColor, SetGlyph, SetText/SetTextByIndex, SetRadius, ToggleOpen ; OnSelection |
| Menu / ContextMenu | SetText, SetGlyph, SetColor, ToggleOpen, OpenSubMenu, (sections) ; OnSelected, OnClick |
| ColorPicker | SetColor, GetColor, SetSize, SetRadius, ToggleOpen ; popup : SetColor, SetOpacity, mode, RGB range, hex ; OnColorChanged |
| Gauge (4) | SetGaugeType, SetValue/SetCurrentValue, SetMinValue, SetMaxValue, SetFillColor, SetBackgroundColor, SetGlyph, (Circular Alt : SetAngle, SetArc) |
| Progress (3) | SetProgressType, SetValue, SetMinValue, SetMaxValue, SetPercent, SetShowPercent, SetTitle, SetFillColor, SetBackgroundColor, SetSize, SetRadii |
| Divider | SetColor, SetSize, SetPadding, SetCustomPadding |
| Image | SetBrush, SetGlyph, SetGlyphAndAppearance, SetAppearance, SetContentMode, SetAspectRatio, SetStretch, SetSlicedMode, SetSize |
| TextField | SetText, GetText, SetHintText, SetType, SetStyle, SetShape, SetFont, SetFontSize, SetTextCase, SetTextColor, SetBackgroundColor, SetOutlineColor, SetJustification, SetMaxSize, SetMinDesiredWidth, SetMaxDesiredWidth, SetSize ; OnChange, OnCommit |
| Media | OpenSource, SetMediaPlayer, SetLooping, (Autoplay, Loop) ; OnMediaOpened |
| Web | LoadURL, LoadString, ExecuteJavascript, GetURL, GetTitle ; OnUrlChanged, OnBeforePopup, OnConsoleMessage |
| Effect | SetEffect, SetEffectMaterial, GetEffectMaterial, EnableEffect, DisableEffect, RequestRender |
| ExplodedView (3DViews) | Open (bool), Spacing, Transform (angle, scale, shear, translation), InterpSpeed |
| Unavailable | SetText (titre, description), SetGlyph, SetAppearance, SetColor |
| Path | Instructions (`TArray<FVekaPathCommand>`), Resolution, Thickness, Color |
| BlueprintLibrary | CreateStops, CreateGradient, CreateLinear/Angular/Radial/MatrixGradient, CreateMix, HexToColor, GetDominantColor, PositionPresetToVector2D, SetAlpha, CreateMenuButton/Divider/Label/SubMenu, OpenContextMenu |
