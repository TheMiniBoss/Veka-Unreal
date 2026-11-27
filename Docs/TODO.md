# TODO — Conversion de Veka en C++

> ⚠️ **Plan remplacé** par [`VEKAUI_PHASE0.md`](VEKAUI_PHASE0.md) §12. Une nouvelle TODO sera écrite après validation de la Phase 0.
> La section **À faire maintenant** ci-dessous est à jour ; tout ce qui suit « Ancien plan » est l'historique.

## À faire maintenant (après la Phase 4, 2026-11-22)

### Langage `.vk` : des effets sans C++
Règle de TheMiniBoss (2026-11-27) : **tout doit être faisable en `.vk`** ; s'il manque une brique, on l'ajoute au langage.
- [x] Fonctions maths dans les expressions : `sin`, `cos`, `abs`, `sqrt`, `pow`, `clamp`, `lerp`… — fait le 2026-11-27
- [x] Hasard déterministe : `random(seed)` et `random(seed, min, max)` — fait le 2026-11-27
- [x] Une valeur de temps qui avance : `time` — fait le 2026-11-27
- [x] `let`, `hsv(...)`, `rgba(...)` calculé — fait le 2026-11-27
- [x] Refaire `Fireworks` 100 % `.vk` et supprimer le C++ — fait le 2026-11-27 (2,0 ms / image)
- [ ] Perf : ne réévaluer chaque image que les nœuds qui lisent `time` (aujourd'hui toute la vue : ~2 ms pour 150 étincelles ; estimé ~1 ms)
- [ ] Perf : un changement de remplissage / offset / opacité venu d'une réévaluation ne devrait que repeindre (comme les animations implicites depuis 4f)
- [ ] Composant générique « émetteur de particules » réglable dans l'inspecteur (si le besoin revient)

### Rendu
- [ ] **(P)** Galerie d'exemples du Hub : le verre de `VV_ExampleBirthdayCard` y paraît sombre. Un hôte ne prend qu'une capture par image (au premier effet dessiné) et tous les aperçus partagent l'hôte du Hub. Pistes : un modificateur qui isole un sous-arbre (sa propre capture), ou une nouvelle capture quand un effet est sur une couche Slate au-dessus de la précédente
- [x] Les effets (backdrop, glass) débordent d'un `.clip` : ils ignorent le clip Slate (vu dans la galerie d'exemples, et au zoom de l'éditeur de vue) — corrigé le 2026-11-27
- [ ] Primitives animées : relancer les images quand une propriété change (`NeedsAnimationFrame` est lu avant le paint, donc un `active: false → true` ne redémarre rien)

### Hub
- [ ] Recharger les exemples sans fermer et rouvrir l'onglet (les fichiers `.vk` sont gardés en cache)
- [ ] Aperçus d'exemples : réduire à l'échelle les vues plus grandes que la carte (300 px de haut, ~270 px de large)

### Livraison
- [ ] **(P)** Vérifier la licence LT Flode Neue avant de diffuser le paquet Shipping (`VekaHost/Packaged/Windows`)
- [ ] Revoir à l'œil les teintes assombries du fond de `VV_ExampleBirthdayCard` (vérifiées seulement par les tests)

---

# Ancien plan (conversion Blueprint → C++, remplacé)

Référence : [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md) (les `§` renvoient à ce document).
Source : branche `Blueprint` @ `f6d5cfd` (Alpha 13). Cible : branche `C++`.

Légende : `[ ]` à faire · `[x]` fait · **(P)** prioritaire / bloquant.

---

## P0 — Préparation

### Décisions (§11)
- [ ] **(P)** Valider : widgets Slate natifs enveloppés UMG (vs `UUserWidget` C++)
- [ ] **(P)** Valider la version moteur cible (5.8 recommandé) et re-sauver les assets dans cette version
- [ ] Valider : sous-thèmes en structs inline dans `UVekaTheme` (vs data assets séparés)
- [ ] Valider : module `VekaWeb` séparé
- [ ] Valider : correction des noms (`Environnement`, `Prominant`, `Hierachical`, `Exlamation`, `Quarternary`, `WindowBakground`, `Asymetric`…)
- [ ] Valider : suppression des BPI par famille (pas de `IVekaColoraTarget` au départ)

### Extraction depuis la branche `Blueprint` (éditeur ouvert)
- [ ] **(P)** Script Python éditeur : exporter en JSON toutes les valeurs des data assets (`Veka_Default`, `Veka_LightDefault`, `Colora_System`, `Glyph_Default`, `Menu_Default`, `Picker_Default`, `Slider_Default`, `Toggle_Default`, `Badge_Default`, `FONT_LTFlode`, 19 `Glyph_*`) → `Docs/Migration/*.json`
- [ ] **(P)** Lister les noms exacts des paramètres scalar/vector/texture de chaque matériau et MI (`M_VekaShapes`, `M_VekaShapesMask`, `M_VekaText`, `M_VekaGlyph`, `M_VekaGlyphMaskToIcon`, `M_VekaProgress*`, `M_VekaColorPickerSlider`, `M_SlicerImage`, `M_VekaMedia`, `M_SDFPreview`, `M_VekaFontRedacted`) → `Docs/Migration/MaterialParams.md`
- [ ] **(P)** Recopier les formules des graphs critiques : SDF du `Veka_Interactor` (`CheckShapeHit`, `AutoRadius`, `AutoRadii`, `GetSafeRadii`, continuous), easings + spring de `Veka_Anima`, `Veka_Colora::SetupMaterial` (ordre et mapping des paramètres), `BFL_Veka` (`HexToColor`, `GetDominantColor`, `PositionToUV`), conversions des color pickers (RGB/HSV/Luminosity/Saturation/Grayscale), `PathBuilderUI` (Bézier quad/cubic) → `Docs/Migration/Formulas.md`
- [ ] Capturer chaque onglet de la fenêtre Documentation en dark et light (référence visuelle pour la recette) → `Docs/Migration/Screenshots/`
- [ ] Noter les valeurs par défaut des propriétés exposées de chaque widget (taille, radius, padding, couleurs…)

### Squelette
- [ ] **(P)** Ajouter `Modules` dans `Veka.uplugin` (`Veka` Runtime `PreDefault`, `VekaEditor` Editor, `VekaWeb` Runtime optionnel)
- [ ] **(P)** Créer `Source/Veka/Veka.Build.cs`, `VekaModule.cpp`, `Veka.h`
- [ ] **(P)** Créer `Source/VekaEditor/VekaEditor.Build.cs`, `VekaEditorModule.cpp`
- [ ] Créer `Source/VekaWeb/` si validé
- [ ] Mettre à jour `.gitignore` (`Binaries/`, `Intermediate/`, `*.sln`, `.vs/`, `DerivedDataCache/`)
- [ ] Vérifier que le plugin vide compile dans un projet hôte (Win64 Development Editor)
- [ ] CI : `RunUAT BuildPlugin` + tests automation (GitHub Actions ou script local documenté)
- [ ] Script de contrôle « zéro Blueprint » qui scanne `Content/` (critère d'acceptation 1)

---

## P1 — Socle runtime (`Source/Veka`)

### Types
- [ ] **(P)** `VekaEnums.h` : les 35 enums du §5.1 avec `UMETA(DisplayName)`
- [ ] **(P)** `VekaColoraTypes.h` : `FVekaColor`, `FVekaColorRef`, `FVekaPosition`, `FVekaStopPosition`, `FVekaColorStop`, `FVekaLinearGradient`, `FVekaAngularGradient`, `FVekaRadialGradient`, `FVekaMatrixGradient`, `FVekaGradient`, `FVekaColoraSettings` (stops en `TArray`, max 9)
- [ ] `VekaFontTypes.h` : `FVekaFontAppearance`, `FVekaTypefaceEntry`
- [ ] `VekaGlyphTypes.h` : `FVekaGlyphRef`, `FVekaGlyphAppearance`, `FVekaBadge`, `FVekaCustomBadge`
- [ ] `VekaMenuTypes.h` : `FVekaMenuSection`, `FVekaSubMenu`
- [ ] `VekaPathTypes.h` : `FVekaPathCommand`, `FVekaSubPath`
- [ ] Remplacer les 13 `Veka_Overridable*` par le pattern `bOverride_X` + `InlineEditConditionToggle` (§6.7)

### Data assets
- [ ] **(P)** `UVekaColorPalette` (26 presets dark/light + alpha par hiérarchie, `GetColor`, `GetHierarchyAlpha`)
- [ ] **(P)** `UVekaFontFamily` (police par `EVekaTypeface` + italique, `GetFont(FVekaFontAppearance)`)
- [ ] **(P)** `UVekaGlyph` (IconFill, couleurs multicolor, badge, taille/position badge)
- [ ] **(P)** `UVekaTheme` (couleurs par rôle/composant, police, apparence police, sous-thèmes Glyph/Badge/Menu/Picker/Slider/Toggle, `Resolve…` helpers)
- [ ] Sous-thèmes (structs ou assets selon décision) : Glyph, Badge, Menu, Picker, Slider, Toggle
- [ ] `GetPrimaryAssetId` + Asset Manager (types `VekaTheme`, `VekaGlyph`) pour le chargement async

### Services
- [ ] **(P)** `UVekaSettings` (`UDeveloperSettings`) : DarkTheme, LightTheme, Palette, bUseDarkTheme
- [ ] **(P)** `UVekaSubsystem` (`UEngineSubsystem`) : cache, `GetActiveTheme`, `GetPalette`, `IsDark`, `SetDarkMode`, `SetTheme`, `OnThemeChanged`
- [ ] Rafraîchir le subsystem quand les settings changent en éditeur (`PostEditChangeProperty`)

### Colora
- [ ] **(P)** `VekaMaterialParams.h` : constantes `FName` des paramètres (depuis l'extraction P0)
- [ ] **(P)** `FVekaColora::ResolveColor`, `ResolveStops`, `PositionToUV`, `ApplyToMaterial(MID, Settings, bDark)`
- [ ] `VekaColorMath.h` : Hex ↔ couleur, RGB ↔ HSV, sRGB ↔ OkLab/OkLCH, CMYK, couleur dominante, luminance

### Animation
- [ ] **(P)** `VekaEasing::Evaluate` (Linear, EaseIn/Out/InOut, Circ ×3, Default, Custom curve)
- [ ] Spring (`FFloatSpringState`, stiffness / damping / mass, comme `FloatSpringInterp`)
- [ ] **(P)** `FVekaAnimator` : durée, délai, repeat, auto-reverse, vitesse ; piloté par `RegisterActiveTimer` ; callbacks valeur + fin
- [ ] Préparer `EVekaTransition` / `EVekaTransitionMode` (API, même si pas encore branché partout)

### SDF / hit-test
- [ ] **(P)** `FVekaSDF` : Circle, Box (radii uneven), RoundedBox continuous, Capsule, Ellipse, polygone Custom (≤16 points)
- [ ] `AutoRadius`, `AutoRadii`, `GetSafeRadii`

### Librairie Blueprint
- [ ] `UVekaBlueprintLibrary` : CreateStops (remplace Create2..9Stops), CreateGradient, CreateLinear/Angular/Radial/MatrixGradient, CreateMix, HexToColor, GetDominantColor, PositionPresetToVector2D, SetAlpha, helpers menu (CreateMenuButton/Divider/Label/SubMenu), GetVekaTheme, IsDarkMode, SetDarkMode

### Tests P1
- [ ] `Veka.Easing.*` (bornes 0/1, monotonie, spring converge)
- [ ] `Veka.SDF.*` (points dedans/dehors/bord pour chaque forme)
- [ ] `Veka.Color.*` (hex, HSV, OkLCH aller-retour, dominante)
- [ ] `Veka.Theme.*` (résolution override vs thème, dark/light)

---

## P2 — Assets non-logiques (`Content/`)

- [ ] **(P)** Ré-ajouter depuis `Blueprint` : `Widgets/**/Materials/*`, `Widgets/Shapes/SDF/*`, `Colora/Materials/*`, `Colora/Gradients/**`, `MF_VekaCircleProgress`, `MFI_VekaCircleProgress`, `Widgets/Interaction/Material/*`, `Widgets/Images/Material/*`, `Fonts/Materials/*`
- [ ] **(P)** Ré-ajouter polices : `Fonts/LTFlode/**`, `Fonts/Redacted/**`
- [ ] **(P)** Ré-ajouter textures : `Glyphs/**` (textures seulement), `Logo/*`, `Utility/Docs/DesignLab/Wallpaper/*`, `Widgets/Symbols/Grid`
- [ ] Vérifier qu'aucun matériau ne référence un asset BP supprimé (`E_VekaBooleanMaterial`, structs…) ; corriger
- [ ] Ranger dans la nouvelle arborescence (`Content/Materials`, `Fonts`, `Glyphs`, `Textures`) + redirecteurs nettoyés
- [ ] **(P)** Script d'import JSON → créer `DA_VekaPalette_System`, `DA_VekaFont_LTFlode`, `DA_VekaGlyph_*` (19), `DA_VekaTheme_Dark`, `DA_VekaTheme_Light` sur les classes C++
- [ ] Renseigner `DefaultVeka.ini` (thèmes et palette par défaut)
- [ ] Test : chaque nom de `VekaMaterialParams.h` existe dans son matériau (`Veka.Materials.Params`)
- [ ] Décider du sort des PNG `Resources/Glyphs/*` (sources d'import ou suppression)

---

## P3 — Primitives

### Base
- [ ] **(P)** `SVekaWidgetBase` / contexte de style : accès thème, dark/light, abonnement `OnThemeChanged`, MID helper, animator
- [ ] Convention `UVeka*` : `RebuildWidget`, `SynchronizeProperties`, `ReleaseSlateResources`, `GetPaletteCategory() = "Veka"`, aperçu `IsDesignTime()`

### Shape (+ Interactor)
- [ ] **(P)** `SVekaShape` : rendu brush matériau, 7 modes, radius/radii, continuous, outline (mode + épaisseur), highlight (couleur, taille, épaisseur), custom points
- [ ] **(P)** Hit-test SDF + events : click, double-click, press, release, hover, unhover, right-click, capture
- [ ] Contenu (slot) : padding, alignement H/V, masque du contenu (`M_VekaShapesMask`)
- [ ] Mode Simple / Colora ; `SetColoraSettings`, `SetColoraMatrix`, `SetGradientMatrixPoint`
- [ ] Matériau d'effet (retainer) ; aperçu SDF debug
- [ ] `UVekaShape` (UMG, `UContentWidget`) + delegates BlueprintAssignable

### Text
- [ ] **(P)** `SVekaText` / `UVekaText` : police thème + graisse + italique, casse, troncature fin/milieu, wrap, justification, line spacing, taille
- [ ] Couleur Colora via matériau de police (`M_VekaText`), mode redacted (`M_VekaFontRedacted`)

### Symbol
- [ ] **(P)** `SVekaSymbol` / `UVekaSymbol` : preset/custom, 4 rendering modes, filled, gradient, variantes + valeur, taille
- [ ] Badges : preset (via thème badge) et custom (taille, position, couleur, icône)

### Path, Divider
- [ ] `SVekaPath` / `UVekaPath` : MoveTo, LineTo, QuadCurveTo, CubicCurveTo, Close ; résolution ; épaisseur ; couleur par sous-chemin
- [ ] `SVekaDivider` / `UVekaDivider` : couleur, taille, padding, highlight

### Tests P3
- [ ] Tests d'interaction (clic hors forme ignoré, clic dans la forme reçu)
- [ ] Widget de démo qui affiche toutes les primitives

---

## P4 — Composites simples

- [ ] `UVekaLabel` : TitleAndIcon / Title / Icon, padding, police, glyph appearance
- [ ] `UVekaButton` : 4 styles (Plain, Borderless, Bordered, BorderedProminent), désactivé (masque couleur), press/release color mask, glyphe + texte, 6 events
- [ ] `UVekaToggle` + `SVekaSwitch` (knob animé), `SVekaCheckBox` (glyphe check, forme), `SVekaToggleButton`
- [ ] Comportements Toggle / RadioStrict / RadioFlexible + groupe radio (`OnRadioActivated`)
- [ ] `UVekaSlider` : drag, clic sur la piste, step, min/max, labels min/max, suffixe, max digits, knob (taille, forme, couleur), animation, thème slider
- [ ] `UVekaColorSlider` : piste dégradée (`M_VekaColorPickerSlider`, 2 ou 3 couleurs, mode hue), champ texte lié
- [ ] `UVekaStepper` : +/-, min/max/step, appui long qui répète (`PressDelay`), désactivé
- [ ] `UVekaTextField` : Text/Number/Percent/Currency, styles Plain/Rectangle/Rounded/Capsule, hint, couleurs focus/hover, commit/change
- [ ] `UVekaImage` : brush, glyphe, None/Fit/Fill, ratio, 9-slice (`M_SlicerImage`)
- [ ] `UVekaUnavailable` : titre, description, glyphe
- [ ] `UVekaEffect` : retainer + matériau, enable/disable, request render
- [ ] `UVekaKeyBindButton` : définir le besoin (vide en Alpha 13) ou retirer

---

## P5 — Composites avancés

### Picker
- [ ] `UVekaPicker` (type Menu / Segmented / Palette / Radio, options `TArray<FText>` ou `FVekaMenuSection`, index, label mode, taille)
- [ ] `SVekaPickerMenu` (popup, rows, glyphe de thème, placement centré au-dessus)
- [ ] `SVekaPickerSegmented` (indicateur animé qui glisse vers la sélection)
- [ ] `SVekaPickerPalette` (colonnes, placement centré en dessous)
- [ ] `SVekaPickerRadio` (orientation V/H, forme radio du thème)

### Menus
- [ ] `SVekaMenu` / `UVekaMenu` (bouton + popup, `MenuArrowGlyph`)
- [ ] Rows : Button, Divider, Label, Picker, SubMenu (sous-menu à droite, `SubMenuGlyph`)
- [ ] `UVekaContextMenu` + ouverture au clic droit depuis n'importe quel widget Veka (`OpenContextMenu`)
- [ ] Animations open/close, fermeture clic extérieur / Échap

### Color picker
- [ ] `UVekaColorPicker` (pastille + popup centrée, OK / Annuler, `OnColorChanged`)
- [ ] Modes : RGB (champ hex, 3 plages), HSV, Hue, Saturation, Luminosity, Grayscale
- [ ] OkLCH et CMYK : au minimum l'état « Unavailable » comme Alpha 13 ; implémentation complète si temps
- [ ] Slider d'opacité commun

### Gauge / Progress
- [ ] `UVekaGauge` : Linear, LinearAlt, Circular, CircularAlt (angle, arc), labels min/max/valeur, glyphe
- [ ] `UVekaProgress` : Linear (`M_VekaProgressView`), Circular (`M_VekaProgressCircular`), Lazy (indéterminé, animation), titre, pourcentage

### Autres
- [ ] `UVekaExplodedView` (Veka_3DViews : spacing, transform, interp, open/close)
- [ ] `UVekaMedia` (MediaPlayer, source, autoplay, loop, `M_VekaMedia`, `OnMediaOpened`)
- [ ] `UVekaWeb` dans `VekaWeb` (LoadURL, LoadString, ExecuteJavascript, GetURL, GetTitle, events)

---

## P6 — Éditeur (`Source/VekaEditor`)

- [ ] **(P)** `FVekaEditorStyle` (`FSlateStyleSet`, icônes depuis `Resources/`)
- [ ] **(P)** `FVekaEditorCommands` + `UToolMenus` : combo « Veka » dans la toolbar du Level Editor et du Widget Blueprint Editor (Documentation, Glyph Library, Gradient Matrix Editor, About)
- [ ] Spawners d'onglets nomades + entrée `Tools › Veka`
- [ ] `SVekaDocumentationTab` : onglets Buttons, Iconography, Indicators, Media, Menus, Pickers, Shapes, Texts, ValueInputs avec contrôles live (reprendre les options des `Veka_EditorDocs*`)
- [ ] Design Lab (Buttons : style, label, vue éclatée)
- [ ] About : version (lue depuis le `.uplugin`), logo, animation
- [ ] `SVekaGlyphLibraryTab` : liste Asset Registry des `UVekaGlyph`, filtres (rendering, variant, filled, badge, fond), aperçu, éditeur (badge pos X/Y, taille, opacité), dirty + Apply/Reset avec `FScopedTransaction`
- [ ] `SVekaGradientMatrixTab` : 9 points draggables, sliders (angle, distortion scale/strength, fill blur, num points), fill empty space, auto-play seed, copie
- [ ] `IPropertyTypeCustomization` : `FVekaColorRef` (pastille dark/light), `FVekaColoraSettings` (aperçu dégradé), `FVekaGlyphRef` (aperçu glyphe)
- [ ] Thumbnail renderer `UVekaGlyph` (optionnel)
- [ ] Factories « Veka Theme », « Veka Glyph », « Veka Color Palette » dans le menu Add du Content Browser

---

## P7 — Qualité et livraison

- [ ] Profilage : 200 widgets au repos, aucun coût de tick Veka ; Global Invalidation activée OK
- [ ] Audit GC : aucun UObject Veka créé par frame
- [ ] Focus clavier/gamepad + `AccessibleText` sur les widgets interactifs
- [ ] Build `RunUAT BuildPlugin` Win64 Development + Shipping sans warning ; non-unity build OK
- [ ] Projet de démo packagé : clic, hover, slider, picker, menu contextuel, color picker, animation, changement de thème à chaud
- [ ] Recette visuelle : comparer avec les captures P0 (dark + light)
- [ ] Script « zéro Blueprint » vert
- [ ] README : installation, premiers pas, liste des widgets
- [ ] CHANGELOG + guide de migration (tableau BP → C++ du §5, propriétés renommées)
- [ ] Mettre `VersionName` (« Alpha 14 » ou autre), `Description`, `DocsURL` dans `Veka.uplugin`
- [ ] Supprimer `Docs/Migration/` temporaire si plus utile (ou le garder en archive)
- [ ] Tag git de la version

---

## Hors périmètre (roadmap après parité)

- [ ] Onglets de doc qui ont un bouton mais pas de page en Alpha 13 (à confirmer dans l'éditeur) : Alert, Animation, Colors, ConfirmationDialog, Effects, Images, Materials, Notification, Presentations, Responsive, ScrollView, Search, Separators, TabView, Tip, UserInput
- [ ] Transitions `EVekaTransition` (Opacity, Slide, Move, Push, Offset, Blur, Identity, Scale) en modes Single/Asymmetric/Combined sur tous les widgets
- [ ] Portage des matériaux SDF/Colora en shaders `.usf`
- [ ] Implémentation complète OkLCH / CMYK dans le color picker si pas fait en P5
