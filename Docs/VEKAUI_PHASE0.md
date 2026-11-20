# VekaUI — Phase 0 : audit et architecture proposée

> **Statut : proposition, en attente de validation.** Aucun code n'est écrit avant ton accord.
> Ce document **remplace** l'architecture du [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md)
> (qui visait un portage 1-pour-1 des widgets UMG). L'inventaire de ce cahier (§4, annexes)
> reste valable et sert de base à l'audit ci-dessous.

| | |
|---|---|
| Source auditée | branche `Blueprint` @ `f6d5cfd` — Veka Alpha 13, 335 `.uasset` |
| Branche de travail | `C++` (+ branches de phase, cf. §12) |
| Moteur | UE 5.8 (`.uplugin`) — assets sauvés en 5.7 |
| Cible | plugin **VekaUI** : framework UI déclaratif Slate-first + **Veka Editor** |

---

## 1. Audit de l'existant (résumé)

Inventaire complet : `CAHIER_DES_CHARGES.md` §4 et annexes A à D.

| Catégorie | Nombre | Contenu principal |
|---|---:|---|
| Widget Blueprints | 80 | ~45 composants runtime (Shape, Text, Symbol, Button, Toggle, Slider, Stepper, Picker ×4, Menu, ColorPicker ×8 modes, Gauge ×4, Progress ×3, Image, Media, Web, TextField, Effect, 3DViews…) + ~25 widgets de documentation éditeur |
| UserDefinedEnum | 36 | formes, easings, transitions, couleurs, glyphes, types de widgets… |
| UserDefinedStruct | 40 | Colora (13), glyphes (4), menus (2), police (2), path (2), anima (1), 13 « Overridable » |
| Blueprint Interfaces | 6 | `BPI_VekaColora`, `BPI_Toggle`, `BPI_VekaPicker`, `BPI_VekaGauge`, `BPI_Progress`, `BPI_VekaColorPicker` |
| Primary Data Assets (classes) | 10 | `Veka_Themes` + 6 sous-thèmes, `Veka_ColoraPreset`, `Veka_Font`, `Veka_Glyph` |
| Objets / libs | 5 | `Veka_Environnement`, `Veka_Anima`, `Veka_Colora`, `BFL_Veka`, `Veka_MacroExtensions` |
| Outils éditeur | 10 | 7 Editor Utility Blueprints (toolbar), 3 Editor Utility Widgets (Docs, Glyph Library, Gradient Matrix) |
| Assets non-logiques | ~150 | 59 matériaux/MF/MI/MFI (SDF, Colora, glyph, progress), 2 polices (25 faces), ~33 textures |

### Ce qui marche bien et qu'on garde (en idée)

- Les **formes SDF** avec hit-test exact (pas de clic dans les coins arrondis).
- **Colora** : couleurs sémantiques dark/light, hiérarchie (Primary → Quaternary), dégradés
  riches (linear, angular, radial, **matrix/mesh** avec distorsion et OkLCH).
- **Anima** : easings + ressort + courbe custom, et déjà un vocabulaire de **transitions**
  (Opacity, Slide, Move, Push, Offset, Blur, Identity, Scale en Single / Asymmetric / Combined)
  — exactement le modèle SwiftUI.
- **Glyphes** à rendering modes (Monochrome, Hierarchical, Multicolor, Palette), variantes et
  badges — très proche de SF Symbols.
- Un vrai effort d'**outillage** (doc intégrée, glyph library, gradient editor).

### Ce qui bloque et qu'on change

- Tout est **impératif** : chaque widget a 20 à 40 fonctions `Set*` qu'il faut appeler à la main.
- Chaque widget **crée ses propres objets** (`Veka_Colora`, `Veka_Anima` via `SpawnObject`) et
  certains **tickent en permanence**.
- Les familles (Toggle, Picker, Gauge…) passent par un widget « façade » + une interface BP +
  `CreateWidget` du bon enfant.
- Données à taille fixe (`Pos1..Pos9`, `P1..P16`, `Create2Stops..Create9Stops`).
- Pas de layout propre au framework : on dépend des panels UMG.

## 2. Anima aujourd'hui

```
Widget ──SpawnObject──> Veka_Anima (UObject BP)
                          ├─ TMap<int AnimID, S_VekaAnimaState>
                          │     Duration, Delay, Repeat, AutoReverse?, Speed,
                          │     EasingsType, CustomEasings (UCurveFloat), CurrentDelay, CurrentRepeat
                          ├─ SetupAnimaTick → AnimateTick (chaque frame)
                          │     Linear | EaseIn | EaseOut | EaseInOut | Circ×3 | Spring | Custom
                          │     Spring = FloatSpringInterp (Stiffness, CriticalDampingFactor, Mass)
                          └─ OnAnima(value) / OnFinished  ──> Widget::OnAnimate / OnAnimateFinished
```

- Anime **un float** (0→1) par ID ; le widget fait lui-même le lerp de ses propriétés.
- Pas de notion d'état : on lance `ActivateAnima` à la main.
- Les menus/pickers utilisent aussi des `UWidgetAnimation` (OpenAnim/CloseAnim) en parallèle.
- Enums de transitions présents mais pas branchés de façon générique.

**Limites** : un seul type de valeur, tick par widget, pas de séquence, pas d'animation
implicite sur changement d'état, pas d'interruption propre (reprise depuis la valeur courante).

## 3. Colora aujourd'hui

```
Veka_Environnement ─ Colora { Preset: Veka_ColoraPreset, UseDarkThemes? }
                    └ Themes  { Veka_Themes }
Veka_ColoraPreset : 26 presets (Accent, Blue, …, Background, Separator, Link, Fill)
                    × { Dark, Light } + alpha par hiérarchie (Primary…Transparent)
S_VekaColoraSettings { Mode: Color|Gradient|Mix, Gradient, Mix, Stops(≤9) }
   Gradient { Type: Linear|Angular|Radial|Matrix, Linear{Start,End}, Angular{Start,End},
              Radial{StartR,EndR,ForceRadial}, Matrix{Angle,Blend,Distortion×2,FillBlur,
              FillEmptySpace,Seed,UseOkLCH}, CenterPoint }
   Stop { Color: (Preset + Hierarchy | Manual Dark/Light), Position: (Stop | ForMatrix 2D) }
Veka_Themes : couleurs par composant (Button, Gauge, Menu, Picker, Progress, Slider, Toggle…)
              + police + sous-thèmes (glyph, menu, picker, slider, toggle, badge)
Veka_Colora (UObject par widget) → SetupMaterial → ~40 paramètres poussés dans un MID
```

**Limites** : thème global unique (pas de thème par sous-arbre), tokens liés aux composants
plutôt que sémantiques, pas de blend modes, pas d'ajustements (luminosité/saturation/contraste),
pas de matériaux / verre, un MID par widget.

---

## 4. Vue d'ensemble de VekaUI

```
          ┌──────────────── Authoring ────────────────┐
          │  C++ (API déclarative)   Veka Editor       │
          │  Veka::Stack(...)        Mode Simple ⇄ DSL │
          └──────────┬──────────────────────┬─────────┘
                     │                      │ (VekaLang : parse / print)
                     ▼                      ▼
              ┌────────────────────────────────────┐
              │  Element tree (descriptions, léger)│  ← évalué à chaque changement d'état
              └────────────────┬───────────────────┘
                               │ Reconciler (diff par type + identité)
                               ▼
              ┌────────────────────────────────────┐
              │  Node tree (retenu, porte l'état,  │
              │  les animations, le layout)        │
              └────────────────┬───────────────────┘
                               │ 1 nœud = 1 widget Slate (SVekaNode*) au maximum
                               ▼
                         Slate (layout + paint, invalidation)
                               │
                               ▼
                   VekaUIRender (RDG : backdrop, blur, glass)
        Hôtes : SVekaHost (Slate pur) · UVekaHostWidget (UMG legacy)
```

Trois idées clés :

1. **Un seul modèle intermédiaire (Element)** pour tout le monde. Une vue écrite en C++,
   une vue `UVekaView` (asset) et une vue tapée dans le DSL produisent le même arbre d'Elements.
2. **Un registre de schéma** (`FVekaSchema`) décrit chaque composant et chaque modifier
   (nom, paramètres typés, valeurs par défaut, doc, catégorie, enfants autorisés). Ce registre
   alimente **en même temps** : le parser DSL, la validation, l'auto-complétion, l'inspecteur
   du Mode Simple, la doc intégrée et le DesignLab. → **La parité Simple/Avancé est garantie par
   construction** : un modifier ajouté au registre existe automatiquement dans les deux modes.
3. **Réactivité par diff** (comme SwiftUI/React) : quand un `State` change, seule la vue qui le
   possède est ré-évaluée, le diff ne touche que les nœuds modifiés, Slate n'invalide que ce qui
   a changé.

---

## 5. Core : modèle déclaratif

### 5.1 API C++ (cible)

```cpp
#include "VekaUI.h"
using namespace Veka;

class FProfileCard : public FVekaView
{
    FVekaState<bool> IsOn{false};

    virtual FVekaElement Body(FVekaContext& Ctx) override
    {
        return Stack(EVekaAxis::Vertical, 12)
        (
            Text(INVTEXT("Hello")).Font(Font::Title).Foreground(Colora::Primary),
            Toggle(INVTEXT("Wi-Fi"), IsOn.Binding()),
            If(IsOn, Text(INVTEXT("Connecté")).Transition(Transition::Opacity()))
        )
        .Padding(16)
        .Background(Colora::Background)
        .CornerRadius(12)
        .Animation(Anima::Spring(), IsOn);
    }
};
```

- `FVekaElement` : valeur légère (type de composant + paramètres + modifiers + enfants + id).
- Modifiers chaînables : chaque appel ajoute une entrée à la liste de modifiers de l'Element.
- `If`, `ForEach(Collection, KeyFn, ItemFn)`, `Group`, `Each` : structures de contrôle.
- Identité : position structurelle par défaut, `.Id(Key)` explicite pour les listes.

### 5.2 `Stack` unique (réponse à « fusionner H/V/ZStack »)

**Oui, c'est possible et c'est ce qu'on propose** : un seul conteneur `Stack` avec
`Axis = Horizontal | Vertical | Depth`, `Spacing`, `Alignment` (2D), `Distribution`
(packed / fill / equal). `HStack`, `VStack`, `ZStack` restent des **alias** (DSL et C++) pour la
lisibilité — ils produisent le même nœud. Bonus : l'axe devient animable / responsive
(`Stack(axis: isCompact ? .vertical : .horizontal)` avec transition de layout).

### 5.3 Conteneurs et primitives

| Type | Éléments |
|---|---|
| Layout | `Stack` (+ alias), `Spacer`, `Grid` (colonnes fixes / flexibles / adaptatives), `ScrollView` (axes, indicateurs, inertie), `Overlay`/`Background` (modifiers), `GeometryReader`-like (`Layout` avec accès à la taille) |
| Primitives | `Text`, `Image`, `Symbol` (glyphes Veka), `Shape` (Rectangle, RoundedRectangle, Capsule, Circle, Ellipse, Path, Custom), `Color`/`Gradient` en tant que vues, `Divider` |
| Contrôles | `Button`, `Toggle` (switch / checkbox / button), `Slider`, `Stepper`, `Picker` (menu / segmented / palette / radio), `Menu`, `ContextMenu` (modifier), `TextField`, `ColorPicker`, `Gauge`, `ProgressView`, `Label` |
| Média / natif | `MediaView`, `WebView` (module optionnel), `NativeSlot` (embarquer n'importe quel `SWidget`/`UWidget`) |

### 5.4 Modifiers (liste initiale)

| Famille | Modifiers |
|---|---|
| Layout | `padding`, `frame(width/height/min/max/ideal, alignment)`, `alignment`, `offset`, `layoutPriority`, `aspectRatio`, `fixedSize`, `zIndex` |
| Forme / clip | `cornerRadius` (uniforme ou par coin, continuous), `clip(shape)`, `mask`, `border`, `stroke` |
| Apparence | `foreground(paint)`, `background(paint ou vue)`, `overlay(vue)`, `opacity`, `shadow(color, radius, offset)`, `blur`, `blendMode`, `brightness`, `saturation`, `contrast`, `hueRotation`, `grayscale` |
| Transform | `scale`, `rotation`, `skew`, `transform` (render transform, sans relayout) |
| Interaction | `onTap`, `onDoubleTap`, `onHover`, `onPress`, `onDrag`, `contextMenu`, `disabled`, `focusable`, `hitShape` |
| Environnement | `theme`, `colorScheme`, `font`, `tint`, `environment(key, value)` |
| Anima | `animation(spec, value)`, `transition(...)`, `timeline(...)`, `matchedGeometry(id)` (plus tard) |
| Effets (Phase 4) | `backdropBlur`, `glass(...)`, `refraction`, `chromaticAberration`, `liquidDistortion` |

**Implémentation performante** : un nœud ne crée **pas** un widget Slate par modifier.
Chaque nœud a un seul `SVekaNode` qui applique sa pile de modifiers résolue :
modifiers de layout dans `ComputeDesiredSize` / `OnArrangeChildren`, modifiers de paint dans
`OnPaint`, transforms via le render transform Slate. On évite les arbres Slate profonds.

### 5.5 State, Binding, Environment

| Concept | C++ | DSL |
|---|---|---|
| État local | `FVekaState<T>` (stocké dans le nœud, survit aux ré-évaluations) | `@State var count: Int = 0` |
| Liaison | `TVekaBinding<T>` (get/set, dérivable : `Binding.Map(...)`) | `$count` |
| Environnement | `FVekaEnvironment` (map typée clé → valeur, hérité par les enfants, surchargé par `.environment`) | `@Environment(\.colorScheme) var scheme` |
| Objet observable | `FVekaObservable` / binding vers propriétés `UObject` (réflexion UE) | `@Bind(model.Health) var health` |
| Événements vers l'hôte | delegate `OnVekaEvent(FName, FVekaValue)` sur l'hôte | `emit("Buy", item.id)` |

- `FVekaValue` : variant typé (bool, int, float, string/text, color, vector2, enum, paint,
  tableau, struct) utilisé par l'interpréteur des vues assets et par le bridge Blueprint.
- Un changement d'état marque la vue propriétaire « dirty » ; la ré-évaluation est **différée à
  la fin de la frame** (coalescing : 10 changements = 1 ré-évaluation).

### 5.6 Hôtes et bridge UMG (legacy)

- `SVekaHost` : widget Slate qui monte une vue (C++ ou `UVekaView`). Utilisable partout en Slate
  (y compris dans l'éditeur).
- `UVekaHostWidget : UWidget` : à poser dans n'importe quel Widget Blueprint / Canvas. Propriétés :
  `View` (asset `UVekaView` ou classe C++), `Theme`, valeurs d'état initiales. API Blueprint :
  `SetState(Name, Value)`, `GetState`, `BindObject`, event `OnVekaEvent`.
- `UVekaViewModel` (optionnel) : `UObject` Blueprint-friendly dont les propriétés sont observées
  par la vue → le jeu nourrit l'UI sans code impératif.

---

## 6. Colora (cible)

### 6.1 Tokens sémantiques et thèmes

- **Token** = nom sémantique (`Colora.primary`, `Colora.background`, `Colora.accent`,
  `Colora.separator`, `Colora.fill.secondary`…). On reprend les 26 presets et les 5 niveaux de
  hiérarchie d'Alpha 13 comme jeu de tokens par défaut.
- `UVekaTheme` (data asset) : token → `FVekaPaint`, **par schéma** (Light / Dark / variantes
  custom ex. HighContrast), + police, rayons, espacements (tokens de design, pas que couleurs).
- Héritage de thèmes (`ParentTheme`) : un thème custom ne surcharge que ce qui change.
- **Propagation** par l'Environment : `.theme(MyTheme)` ou `.colorScheme(.dark)` sur n'importe
  quel sous-arbre ; changement à chaud = mise à jour ciblée (seuls les nœuds qui lisent ce token).

### 6.2 `FVekaPaint` (types riches)

| Variante | Détail |
|---|---|
| `Solid` | `FLinearColor` ou token |
| `LinearGradient` | start/end (points unitaires ou presets), stops `TArray` (illimité côté C++, borné par le shader) |
| `RadialGradient` / `EllipticalGradient` | centre, rayons |
| `AngularGradient` | centre, angles |
| `MeshGradient` | reprise du « Matrix » Alpha 13 (points 2D, distorsion, seed, OkLCH) |
| `Material` | `UMaterialInterface` + paramètres |
| `Image` | texture, tiling, 9-slice |
| `Glass` (Phase 4) | backdrop + teinte + blur + réfraction |

Interpolation des couleurs au choix : sRGB, linéaire, **OkLab/OkLCH** (dégradés propres).

### 6.3 Blend modes et ajustements

- Ajustements **sur son propre contenu** (luminosité, saturation, contraste, hue, grayscale,
  opacité) : paramètres du shader de la primitive → coût quasi nul.
- Ajustements **sur un sous-arbre complet** : rendu du sous-arbre dans une texture (comme un
  retainer) puis composite → coût réel, à utiliser avec parcimonie (documenté).
- **Blend modes** (Multiply, Screen, Overlay, Add, SoftLight, Difference…) : ils ont besoin de
  lire ce qu'il y a **dessous**. Slate ne fait que l'alpha-blend → ils dépendent du pipeline
  **Backdrop** (Phase 4). En Phase 2 : `Normal` et `Add` (possible en blend additif) ; le reste
  arrive avec le backdrop. ⚠️ Point à valider.

### 6.4 Rendu des paints

- Rectangles / rounded / capsule / cercle en couleur unie + bordure : **brush RoundedBox natif de
  Slate** (batché, aucun MID, très rapide).
- Dégradés, mesh, continuous corners, formes custom : **shader Veka partagé** (`.usf` du module
  `VekaUIRender`, ou matériau UI unique) avec les paramètres passés par vertex
  (`FSlateDrawElement::MakeCustomVerts`) pour batcher plusieurs formes en un draw. MID par nœud
  seulement en dernier recours. → spike technique en Phase 1 (cf. §11).

---

## 7. Anima (cible)

### 7.1 Spécification d'animation

`FVekaAnimationSpec` = courbe + timing :

| Courbe | Paramètres |
|---|---|
| `Linear`, `EaseIn`, `EaseOut`, `EaseInOut`, `Circ*` (Alpha 13) | durée |
| `TimingCurve` | Bézier cubique (x1, y1, x2, y2) — comme CSS / SwiftUI |
| `Spring` | **masse, raideur, amortissement, vitesse initiale** ; aussi forme « response / dampingFraction » |
| `InterpolatingSpring`, `Bouncy`, `Snappy`, `Smooth` | presets |
| `Curve` | `UCurveFloat` / `UCurveVector` / `UCurveLinearColor` |

Modifiers de timing : `delay`, `speed`, `repeat(count | forever, autoreverse)`.

### 7.2 Animatable

Tout type qui sait s'interpoler : `float`, `FVector2D`, `FLinearColor` (dans l'espace choisi),
`FVekaTransform2D`, `FMargin`, `FVekaCornerRadii`, `FVekaPaint` (dégradé → dégradé), **rect de
layout** (pour les transitions de layout). Trait C++ `TVekaAnimatable<T>` (Lerp, Distance pour
les ressorts).

### 7.3 Implicite (façon `.animation(.spring())`)

Quand une ré-évaluation change une propriété animable d'un nœud qui a `.animation(spec, value)`
(ou qui est dans un `WithAnimation(spec, [&]{ ... })`), le nœud **n'applique pas la nouvelle
valeur d'un coup** : il crée une animation de la valeur *présentée* vers la nouvelle cible.
Interruption propre : une nouvelle cible repart de la valeur et de la **vitesse** courantes
(ressorts continus, pas de saut).

**Transitions** (insertion / suppression de nœuds) : reprise directe des enums Alpha 13 —
`Opacity`, `Slide`, `Move(edge)`, `Push(edge)`, `Offset`, `Blur`, `Scale`, `Identity`, combinables
(`Combined`) et asymétriques (`Asymmetric(insertion, removal)`). Un nœud supprimé reste affiché
le temps de sa transition de sortie.

**Transitions de layout** : quand le rect d'un nœud change (réordonnancement, changement d'axe
du Stack), on anime le rect présenté.

### 7.4 Séquentiel : Timelines

- `UVekaTimeline` (asset réutilisable **ou** intégrée à une `UVekaView`) : pistes ciblant
  `NodeId.PropertyPath`, keyframes typées avec courbe par segment, **événements** (nom + payload)
  et callbacks, marqueurs, boucles, vitesse.
- Contrôle : `Play`, `Pause`, `Seek`, `Reverse`, `Stop`, depuis le DSL (`timeline("Intro").play()`),
  le C++ ou Blueprint (via l'hôte).
- Une timeline peut aussi piloter un **état sémantique** (ex. passer `isOpen` à `true` à 0.3 s),
  ce qui déclenche les animations implicites → hybride état / keyframes.

### 7.5 Moteur

Un **scheduler central** (`FVekaAnimaScheduler`) par hôte, pas un tick par widget :
- toutes les animations actives dans des tableaux plats (data-oriented) ;
- un seul timer actif Slate (ou `FTSTicker`) qui vit **seulement** quand une animation tourne ;
- l'évaluation écrit les valeurs présentées puis invalide les nœuds concernés (paint seul si
  possible, layout seulement si une propriété de layout bouge).

---

## 8. Rendu avancé (Phase 4) : Backdrop et Liquid Glass

### 8.1 Capture du backdrop

Besoin : lire, sous le widget, l'image déjà composée (scène 3D + UI dessinée avant).

| Option | Principe | Avis |
|---|---|---|
| A. Post-buffers Slate du moteur (`SlateRHIPostBufferProcessor`, utilisés par `SBackgroundBlur` / UMG Post Buffer) | Slate copie le backbuffer à un point donné et l'expose aux matériaux UI | **à vérifier en 5.8** ; si dispo, la voie la plus simple et la plus sûre |
| B. Élément Slate custom (`ICustomSlateElement`) + RDG | au moment du draw, copie de la zone du backbuffer, passes RDG (downsample, blur), composite | contrôle total, coût d'une passe par groupe d'éléments |
| C. `FSceneViewExtension` | capture la scene color avant l'UI | ne voit que la 3D, pas l'UI en dessous (OK pour un HUD sur la scène) |

**Proposition** : spike A vs B au début de la Phase 4 ; dans tous les cas **une seule capture +
une pyramide de flou (dual Kawase) par frame et par couche**, partagée par tous les widgets
glass de la couche (pas une capture par widget).

### 8.2 Effets

`backdropBlur(radius)`, `frostedGlass(tint, blur, noise)`, `glass(...)` (style Liquid Glass :
réfraction basée sur la **normale du SDF** de la forme — on réutilise nos SDF —, reflet
spéculaire sur le bord, aberration chromatique, teinte), `liquidDistortion(time, strength)`,
réaction au mouvement (pointeur / inclinaison / vitesse de scroll). Shaders `.usf` dans
`VekaUIRender/Shaders`, qualité réglable (scalabilité) et fallback sans backdrop (teinte unie).

---

## 9. Threading (stratégie)

Contrainte : **Slate n'est pas thread-safe** — layout, création et paint des widgets se font sur
le Game Thread.

| Travail | Thread | Pourquoi |
|---|---|---|
| State, ré-évaluation, diff, layout | **Game Thread** | imposé par Slate ; rendu bon marché grâce au diff ciblé, au coalescing et à l'invalidation |
| Évaluation Anima | Game Thread, données plates ; `ParallelFor` (task graph) au-delà d'un seuil (ex. > 512 animations) | les ressorts sont du calcul pur, parallélisable sans risque ; en dessous du seuil, le coût de synchro dépasse le gain |
| Backdrop, flou, glass, distorsion | **Render Thread / RDG** | c'est du GPU ; aucun impact Game Thread |
| Parsing DSL, analyse, complétion (éditeur) | tâches async (`UE::Tasks`) | ne jamais bloquer l'éditeur sur un gros fichier |
| Chargement thèmes / glyphes / polices | async (Asset Manager / streamable) | |

**Pas de thread de layout dédié** au départ : il faudrait dupliquer l'arbre pour Slate et
synchroniser chaque frame, pour un gain nul sur des UI normales. Les structures sont pensées pour
le permettre plus tard (layout pur, sans accès Slate) si le profilage le justifie.

Budget visé : écran de 500 nœuds au repos = 0 coût CPU Veka ; 100 animations ressort simultanées
< 0,1 ms Game Thread ; glass plein écran < 1 ms GPU en qualité « High » (à mesurer).

---

## 10. Veka Editor (Phase 3)

### 10.1 Modèle unique

```
UVekaView (asset)
 ├─ FVekaDocument  ← SOURCE DE VÉRITÉ (AST : nœuds, modifiers, expressions, états, timelines)
 ├─ déclarations d'état / paramètres / événements
 └─ métadonnées éditeur (positions de commentaires, préférences de format)

Mode Simple  ──édite──>  FVekaDocument  <──parse──  Mode Avancé (texte)
     ▲                         │                          ▲
     └──── rafraîchit ─────────┴──── imprime (printer) ───┘
```

- **Mode Avancé → arbre** : parse à chaque frappe (debounce ~150 ms, async). Si le code est
  invalide, l'arbre garde la dernière version valide et les erreurs s'affichent en ligne ; le Mode
  Simple passe en lecture seule tant qu'il y a une erreur (pour ne jamais perdre de texte).
- **Arbre → Mode Avancé** : un *printer* canonique régénère le code. Les **commentaires** et les
  lignes vides sont rattachés aux nœuds (trivia) pour survivre aux allers-retours.
- Undo/Redo unifié (`FScopedTransaction` sur le document) quel que soit le mode.

### 10.2 Le DSL (VekaLang)

```swift
view ProfileCard {
    @State var isOn: Bool = false
    @Param var title: Text = "Hello"
    @Event buy(itemId: Int)

    Stack(.vertical, spacing: 12) {
        Text(title).font(.title).foreground(Colora.primary)
        Toggle("Wi-Fi", isOn: $isOn)
        if isOn {
            Text("Connecté").transition(.opacity.combined(.scale))
        }
        ForEach(items, id: \.id) { item in
            Button(item.name) { emit buy(item.id) }
        }
    }
    .padding(16)
    .background(Colora.background)
    .cornerRadius(12, style: .continuous)
    .animation(Anima.spring(stiffness: 300, damping: 20), value: isOn)
}
```

- Lexer + parser récursif descendant écrits à la main (pas de dépendance externe), AST avec
  positions source (erreurs, surlignage, complétion).
- Analyse sémantique contre le **registre de schéma** (§4) : composant inconnu, paramètre manquant,
  type incorrect, token Colora inexistant…
- Expressions : littéraux, variables, `$binding`, opérateurs, ternaire, interpolation de chaînes,
  accès membre. Actions : affectation, `toggle()`, `withAnimation { }`, `emit`, contrôle de
  timeline. Contrôle : `if / else`, `ForEach`.
- **Pas de compilation en bytecode au départ** : l'asset stocke l'AST et le runtime l'interprète
  (rapide, car on n'évalue qu'au changement d'état). Le parser n'est **pas** embarqué dans le jeu
  packagé (module UncookedOnly).

### 10.3 Éditeur

- Asset Editor `FVekaViewEditorToolkit` (onglets dockables) : **Outliner** (arbre, drag & drop,
  renommage, multi-sélection), **Viewport** (centre), **Inspecteur** (généré depuis le schéma :
  paramètres + pile de modifiers ajoutables/réordonnables, sélecteurs Colora/Anima visuels),
  **Code** (texte avec coloration, complétion, erreurs), **State** (variables et valeurs mock),
  **Timeline** (pistes / keyframes).
- **Viewport** : résolutions et ratios prédéfinis (1080p, 1440p, 4K, ultrawide, mobile), DPI,
  zoom/pan, switch Light/Dark/thème custom, overlays (bounds, padding, baseline).
- **Mode Test (Play)** : l'aperçu devient interactif (clics, hover, focus, états réactifs,
  timelines, `emit` journalisés), avec **données mockées** éditables, sans lancer le PIE.
- **Hub** (onglet nomade + bouton de la toolbar) : DesignLab (tous les composants, palettes et
  dégradés Colora actifs, courbes/ressorts Anima à tester), Galerie d'exemples (HUD, menus, cartes
  glass, inventaire — clonables en un clic dans le projet), Documentation (DSL, modifiers, guides,
  **générée en partie depuis le schéma**), Changelog. **Le Hub est lui-même écrit en VekaUI**
  (dogfooding).

---

## 11. Modules et assets

| Module | Type / LoadingPhase | Contenu | Dépendances |
|---|---|---|---|
| `VekaUI` | Runtime / PreDefault | Element, Node, Reconciler, State/Binding/Environment, Schema, layout, modifiers, primitives et contrôles, **Colora**, **Anima**, interpréteur d'AST, `SVekaHost`, `UVekaHostWidget` (bridge UMG) | Core, CoreUObject, Engine, Slate, SlateCore, UMG, InputCore, DeveloperSettings |
| `VekaUIRender` | Runtime / **PostConfigInit** (shaders) | shaders `.usf`, backdrop, blur, glass, éléments Slate custom | RenderCore, RHI, Renderer, SlateRHIRenderer, Projects |
| `VekaLang` | **UncookedOnly** | lexer, parser, printer, analyse, complétion, formateur | VekaUI |
| `VekaEditor` | Editor | Asset Editor `UVekaView`, viewport, outliner, inspecteur, éditeur de code, timeline, Hub, factories, thumbnails | VekaUI, VekaLang, UnrealEd, ToolMenus, PropertyEditor, AssetTools, Slate… |
| `VekaWeb` | Runtime (optionnel) | `WebView` | VekaUI, WebBrowser |

Les modules `VekaColora` / `VekaAnima` séparés ont été écartés : ils sont trop liés au core
(environnement, modifiers, nœuds). Ils existent comme **dossiers et namespaces** (`Veka::Colora`,
`Veka::Anima`) dans `VekaUI` — séparables plus tard si besoin.

**Assets** : `UVekaView`, `UVekaTheme`, `UVekaTimeline`, `UVekaAnimationPreset` (ressorts/courbes
nommés), `UVekaSymbol` (ex-`Veka_Glyph`), `UVekaFontFamily`. Les textures de glyphes, polices et
logos d'Alpha 13 sont ré-importés ; les **matériaux** d'Alpha 13 ne sont pas repris tels quels
(remplacés par le rendu §6.4 / shaders `.usf`), mais leurs formules SDF et Colora servent de
référence.

---

## 12. Plan de migration et exécution

### 12.1 Branches

Git interdit d'avoir `C++` **et** `C++/quelque-chose` en même temps (conflit de refs). Proposition :
`C++` reste la branche d'intégration, une branche par phase : `cpp/phase-1-core`,
`cpp/phase-2-colora-anima`, `cpp/phase-3-editor`, `cpp/phase-4-render-hub`, fusionnées dans `C++`
après validation.

### 12.2 Phases

| Phase | Contenu | Livrable / critère |
|---|---|---|
| **0** | Ce document + réponses aux questions §13 | ✅ validation |
| **1** — Core | `.uplugin` VekaUI + 5 modules (vides sauf VekaUI) ; Element / Node / Reconciler ; State / Binding / Environment ; Schema ; `Stack`, `Spacer`, `Grid`, `ScrollView` ; primitives `Text`, `Image`, `Shape`, `Symbol`, `Color` ; modifiers layout + apparence de base ; `SVekaHost` + `UVekaHostWidget` ; scheduler Anima minimal ; spike rendu des formes ; tests automation | une vue C++ s'affiche dans un Widget Blueprint et dans un onglet Slate ; tests verts ; compile Win64 Editor + Shipping |
| **2** — Colora + Anima | thèmes/tokens/schémas, `FVekaPaint` complet, OkLCH, ajustements ; Anima complet (courbes, ressorts, implicite, transitions, layout, timelines) ; contrôles Alpha 13 (Button, Toggle, Slider, Stepper, Picker, Menu, ContextMenu, TextField, ColorPicker, Gauge, Progress, Label, Divider, Media) ; `UVekaView` + interpréteur | parité fonctionnelle Alpha 13 en C++ ; démo de toutes les animations |
| **3** — Veka Editor | VekaLang (parse/print/analyse/complétion) ; Asset Editor : outliner, inspecteur, viewport, code, state, timeline ; Mode Test ; migration des glyphes/thèmes Alpha 13 | aller-retour Simple ⇄ Avancé sans perte sur la suite de tests ; une vue complète faite à 100 % en Mode Simple et une à 100 % en code |
| **4** — Rendu + Hub | backdrop, blur, glass, réfraction, aberration, distorsion, blend modes ; Hub : DesignLab, Exemples, Doc, Changelog | Liquid Glass in-game sur scène 3D ; Hub complet ; budgets perf §9 tenus |

### 12.3 Validation de la compilation ⚠️

**Je n'ai pas Unreal Engine dans mon environnement** (conteneur Linux sans moteur) : je ne peux
pas compiler ni lancer le code UE moi-même. Options (question §13) :

1. Tu compiles sur ta machine à chaque étape et tu me renvoies les erreurs (`Build.log`).
2. Un runner GitHub Actions **self-hosted** sur ta machine (avec UE 5.8) lance `RunUAT BuildPlugin`
   + les tests automation à chaque push → je lis les résultats via la PR.
3. La partie **sans moteur** (parser VekaLang, maths couleur/ressorts/SDF) est écrite pour pouvoir
   aussi être testée ici avec un compilateur standard, via une fine couche d'adaptation — ça
   attrape une partie des erreurs avant toi.

Proposition : **2 + 3** (sinon 1 + 3).

---

## 13. Questions ouvertes

1. **Compilation** : quelle option du §12.3 ?
2. **Nom** : le plugin devient-il `VekaUI` (fichier `VekaUI.uplugin`, dossier `VekaUI`) ou garde-t-on
   le plugin `Veka` avec des modules `VekaUI*` ?
3. **Logique dans le DSL / Mode Simple** : jusqu'où ? (a) affichage + bindings seulement ;
   (b) + `if` / `ForEach` / actions simples (affectations, toggle, emit) — **proposé** ;
   (c) langage complet avec fonctions.
4. **Données du jeu** : préfères-tu nourrir les vues par `SetState` depuis Blueprint, par un
   `UVekaViewModel` observé, ou les deux (**proposé**) ?
5. **Composants Alpha 13** : tous recréés en Phase 2 (**proposé**), ou seulement le cœur
   (Button, Toggle, Slider, Text, Symbol) et le reste plus tard ?
6. **Blend modes** : OK pour qu'ils arrivent avec le backdrop en Phase 4 (seuls Normal/Add avant) ?
7. **Plateformes** : Win64 seul, ou aussi Mac / Linux / consoles / mobile (impact sur les shaders
   et le backdrop) ?
8. **Version moteur** : 5.8 confirmé, et pas de support des versions antérieures ?
9. **Éditeur de code** : un éditeur Slate maison (coloration + complétion, **proposé**) suffit-il,
   ou veux-tu une intégration externe (VS Code / LSP) en plus ?
10. **Ancien contenu** : la branche `Blueprint` reste-t-elle la référence figée (on n'y touche
    plus), et les projets qui utilisent Alpha 13 doivent-ils pouvoir migrer (outil d'import des
    thèmes/glyphes) ?
