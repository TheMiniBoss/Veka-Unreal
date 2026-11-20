# VekaUI — Phase 0 : audit et architecture proposée

> **Statut : proposition, en attente de validation.** Aucun code n'est écrit avant ton accord.
> Ce document **remplace** l'architecture du [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md)
> (qui visait un portage 1-pour-1 des widgets UMG). L'inventaire de ce cahier (§4, annexes)
> reste valable et sert de base à l'audit ci-dessous.

| | |
|---|---|
| Source auditée | branche `Blueprint` @ `f6d5cfd` — Veka Alpha 13, 335 `.uasset` (**lecture seule**) |
| Branche de travail | `C++` (+ branches de phase, cf. §16) |
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
| Anima | `animation(spec, value)`, `transition(...)`, `contentTransition(...)`, `timeline(...)`, `matchedGeometry(id)`, `morphGroup(id, blend)` |
| Traits | `stroke(paint, width, dash, cap, join)`, `strokeBorder`, `trim(from, to)` |
| Symboles / texte | `symbolRenderingMode`, `symbolVariant`, `symbolEffect`, `imageScale`, `fontWeight`, `textStyle` |
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
| `MeshGradient` | **vrai dégradé maillé** (remplace le « Matrix » Alpha 13, cf. §6.5) |
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
  seulement en dernier recours. → spike technique en Phase 1 (cf. §15).

---

### 6.5 Mesh gradient (remplace le Gradient Matrix)

Le « Gradient Matrix » d'Alpha 13 simulait un maillage avec 9 points, du bruit et du flou,
à cause des limites du Blueprint. Il est remplacé par un **vrai mesh gradient** (comme
Illustrator / `MeshGradient` de SwiftUI) :

- Grille **W × H** de points de contrôle (pas limitée à 3×3), chaque point avec position,
  **couleur** (ou token Colora) et **poignées de Bézier** optionnelles (patchs de Coons).
- Interpolation des couleurs en OkLab (défaut), linéaire ou sRGB ; option de lissage (bicubique).
- Rendu : subdivision des patchs en triangles avec couleurs par sommet
  (`MakeCustomVerts`), mise en cache, retessellation seulement si la grille change.
- **Tout est animable** (points, poignées, couleurs) → fonds vivants, auras, « blobs ».
- Éditeur dédié dans le Veka Editor (poignées, ajout/suppression de lignes/colonnes, pipette,
  presets) qui remplace l'EUW Gradient Matrix. Les réglages `Matrix` d'Alpha 13 ne sont pas
  convertis (formules différentes) — un preset proche peut être recréé à la main.

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

## 8. Vecteurs, traits et morphing de formes

### 8.1 Noyau vectoriel `FVekaPath`

Une seule représentation vectorielle pour **tout** : formes, traits, symboles, texte vectoriel,
masques, chemins de morph.

- Commandes : `MoveTo`, `LineTo`, `QuadTo`, `CubicTo`, `ArcTo`, `Close` ; sous-chemins ; règle de
  remplissage `NonZero` / `EvenOdd`.
- Constructeurs : `Rect`, `RoundedRect` (rayons par coin, style **continuous** / squircle),
  `Capsule`, `Circle`, `Ellipse`, `Polygon`, `Star`, `Arc`, depuis **SVG `d=`**, depuis un glyphe
  de police (contours FreeType).
- Opérations : transformation, bounds, longueur, point/tangente à une distance, **trim**
  (`From`/`To`), aplatissement adaptatif, **booléens** (union, soustraction, intersection, xor —
  via `GeometryCore` du moteur ou Clipper2, licence Boost, à évaluer), offset.
- `FVekaShape` (interface) : `Path(Rect)` + SDF analytique quand il existe (rect, capsule,
  cercle, ellipse) pour le hit-test exact et les effets. Toute forme custom = une classe qui
  renvoie un `FVekaPath` (cf. §11).

### 8.2 Traits (strokes)

| Propriété | Détail |
|---|---|
| Épaisseur | constante, ou **variable le long du trait** (profil) |
| Alignement | intérieur / centré / extérieur |
| Extrémités / jointures | `Butt`, `Round`, `Square` / `Miter` (+ limite), `Round`, `Bevel` |
| Pointillés | motif `TArray<float>` + phase **animable** (marching ants) |
| Peinture | n'importe quel `FVekaPaint` (dégradé le long ou à travers le trait) |
| Trim animable | `.trim(from, to)` → effet « dessin à la main », progress circulaire, signatures |
| Vues | `Line`, `Polyline`, `Path`, `.stroke(...)` / `.strokeBorder(...)` sur toute forme, `fill` + `stroke` ensemble |

Rendu : tessellation CPU (remplissage + expansion du trait) avec **antialiasing analytique** dans
le shader, résultat **mis en cache** tant que le chemin ne change pas ; seules les animations de
géométrie retessellent.

### 8.3 Morphing et fusion « goutte » (style Liquid Glass)

Deux mécanismes complémentaires :

1. **Morph de géométrie** — `matchedGeometry(id, in: namespace)` : quand une vue disparaît et
   qu'une autre avec le même id apparaît (bouton → picker, pastille → carte), le moteur anime
   **cadre + rayons + paint** de l'une vers l'autre avec un ressort, puis le contenu fait un
   fondu croisé. Pour deux chemins arbitraires : ré-échantillonnage à N points, alignement des
   points de départ, interpolation (ou interpolation des SDF).
2. **Fusion SDF (metaballs)** — conteneur `ShapeGroup(blend: r) { ... }` / modifier
   `.morphGroup(id, blend:)` : les formes d'un même groupe sont rendues **en un seul passage**
   avec une union lisse (`smooth-min`) de leurs SDF. Une forme qui sort d'une autre « s'étire
   comme une goutte » puis se détache quand la distance dépasse le rayon de fusion. Animable
   (position, taille, rayon de fusion). Combiné au glass (§12), c'est l'effet Liquid Glass d'Apple.

Le hit-test suit la forme fusionnée (même SDF).

## 9. Symboles (refonte complète type SF Symbols)

### 9.1 Format source : SVG « Veka Symbol Template »

- Import de **SVG** (chemins, groupes, transforms, `fill`/`stroke`), avec un **gabarit** comme SF
  Symbols : lignes guides `Baseline`, `CapHeight`, marges gauche/droite, et **3 masters**
  (Ultralight, Regular, Black) aux chemins compatibles (même nombre de points).
- Si un seul master est fourni : épaisseur simulée par **offset du contour** (moins précis,
  signalé dans l'éditeur).
- **Calques nommés** avec rôle : `primary`, `secondary`, `tertiary`, plus un **ordre** pour les
  animations par calque, et des **ancres** (point de rotation, point de rebond).
- Variantes : `.fill`, `.circle`, `.square`, `.slash` — dessinées ou **générées** (enclosure et
  barre oblique générées par booléens).
- Asset `UVekaSymbol` : chemins des masters, calques, guides, variantes, métadonnées (nom,
  catégories, mots-clés pour la recherche). Un `UVekaSymbolLibrary` regroupe un jeu de symboles
  (la bibliothèque Veka par défaut + celles des utilisateurs).

### 9.2 Taille et épaisseur automatiques

- **9 graisses** (Ultralight → Black) obtenues par **interpolation** des masters — n'importe
  quelle valeur intermédiaire est possible et **animable**.
- **3 échelles** (Small, Medium, Large) relatives à la police.
- Par défaut, un symbole **hérite de la police de son environnement** : taille, graisse et
  alignement sur la ligne de base / cap height du texte voisin
  (`Label("Wi-Fi", symbol: "wifi")` ou symbole dans un `Text` : tout s'aligne tout seul).
- Rendu net à toute taille : chemins vectoriels tessellés + cache par (symbole, graisse, taille
  arrondie, variante) ; option atlas MSDF pour les très grandes quantités (listes, HUD).

### 9.3 Modes de rendu

`Monochrome`, `Hierarchical` (une teinte, opacités par calque), `Palette` (une couleur par rôle),
`Multicolor` (couleurs du symbole), avec n'importe quel `FVekaPaint` (dégradés compris).
**Valeur variable** (0–1) qui allume les calques progressivement (wifi, volume, batterie).

### 9.4 Animations de symboles

| Effet | Détail |
|---|---|
| `Appear` / `Disappear` | par calque ou en entier, échelle + opacité |
| `Bounce` | rebond haut/bas, par calque décalé |
| `Scale` | up/down, maintenu tant qu'un état est vrai |
| `Pulse` | opacité pulsée |
| `VariableColor` | itératif / cumulatif, inversé, en boucle |
| `Replace` | down-up, up-up, off-up, et **Magic Replace** (morph des calques communs, ex. `bell` → `bell.slash`) |
| `Wiggle`, `Rotate`, `Breathe` | autour des ancres |
| Graisse / variante | interpolation continue (ex. outline → fill) |

Déclenchement : discret (`.symbolEffect(.bounce, value: count)`), continu
(`.symbolEffect(.pulse, isActive: loading)`), ou transition de contenu
(`.contentTransition(.symbolEffect(.replace))`). Paramétrage Anima (ressort, vitesse, répétition).

### 9.5 Éditeur et aperçus (Veka Editor)

- **Symbol Library** : grille de recherche (nom, mot-clé, catégorie), aperçu en direct dans
  toutes les graisses × échelles × modes de rendu × light/dark, copie du nom / du code DSL.
- **Symbol Editor** : import SVG (drag & drop), validation du gabarit (masters compatibles,
  guides), édition des calques (rôles, ordre, ancres), génération des variantes, réglages de
  marges, **aperçu d'animation** de chaque effet, comparaison côte à côte avec du texte.
- Export / ré-import SVG, import en lot d'un dossier.
- Migration des 19 glyphes Alpha 13 (textures + badges) : ré-import à partir des sources SVG si tu
  les as, sinon vectorisation à refaire (à confirmer, cf. §17).

## 10. Typographie (même logique que les symboles)

- **Styles de texte sémantiques** (tokens du thème) : `LargeTitle`, `Title1-3`, `Headline`,
  `Body`, `Callout`, `Subheadline`, `Footnote`, `Caption1-2` → taille, graisse, interlignage,
  approche (tracking) **calculés automatiquement**, avec mise à l'échelle globale (réglage
  d'accessibilité « taille du texte ») et **tracking optique** selon la taille.
- `UVekaFontFamily` : faces par graisse (12 graisses Alpha 13) et italique, police de repli,
  chiffres tabulaires, et support des **polices variables** (axes `wght`, `wdth`, `opsz`) si
  possible (cf. risque ci-dessous).
- Modifiers : `.font(.title)`, `.fontWeight`, `.fontWidth`, `.italic`, `.tracking`,
  `.lineSpacing`, `.textCase`, `.monospacedDigit`, `.lineLimit`, `.truncation(.middle)`,
  `.multilineAlignment`, `.minimumScaleFactor`.
- **Animations de texte** : taille et couleur animables ; `.contentTransition(.numericText)`
  (chiffres qui roulent), interpolation de contenu, effets **par glyphe** (apparition décalée,
  vague, machine à écrire) via un `TextRenderer` personnalisable.
- **Deux chemins de rendu** :
  - *Slate* (défaut) : rapide, `FSlateFontInfo`, graisses discrètes ;
  - *Vectoriel* : contours FreeType → `FVekaPath` → même moteur que les symboles ; permet
    l'**interpolation de graisse** (polices variables), le morph de glyphes, les traits sur le texte.
- ⚠️ Risque : Slate ne gère pas les axes des polices variables. Le chemin vectoriel passe par
  FreeType (embarqué dans le moteur) ; à valider par un spike en Phase 2.

## 11. Extensibilité (DA personnalisable)

Objectif : un studio peut créer **sa propre direction artistique** sans toucher au code de Veka.

| Niveau | Comment | Pour qui |
|---|---|---|
| **Thème** | `UVekaTheme` : tokens couleur, paints, typo, rayons, espacements, ombres, animations par défaut | designer, sans code |
| **Styles de composants** | chaque contrôle (Button, Toggle, Slider, Picker, Menu…) délègue son apparence à un **Style** (`.buttonStyle(MyStyle)`), qui est une vue (DSL ou C++) recevant une *configuration* (`label`, `isPressed`, `isOn`, `isHovered`, `isFocused`, `isEnabled`, `value`…). Le look Veka par défaut n'est qu'**un** jeu de styles parmi d'autres | designer (DSL / Mode Simple) |
| **Composants composés** | une `UVekaView` avec `@Param`, `@Slot` (contenu injecté) et `@Event` devient un composant réutilisable, **ajouté automatiquement au registre** (palette, auto-complétion, inspecteur) | designer / intégrateur |
| **Primitives C++** | dériver `FVekaPrimitive` (mesure, paint, hit-test, propriétés animables), déclarer avec `VEKA_REGISTER_COMPONENT(...)` | programmeur |
| **Conteneurs C++** | dériver `FVekaLayout` (`SizeThatFits` / `PlaceChildren`, comme le protocole `Layout` de SwiftUI) — `Stack`, `Grid`, etc. sont eux-mêmes écrits ainsi | programmeur |
| **Modifiers C++** | dériver `FVekaModifier` (layout, paint, hit-test, environnement), enregistré dans le schéma | programmeur |
| **Formes C++** | implémenter `FVekaShape` (`Path(Rect)` + SDF optionnel) | programmeur |
| **Effets / shaders** | `FVekaEffect` + shader `.usf` ou matériau UI, branché au backdrop si besoin | technical artist |

Règles :
- **Le framework utilise sa propre API d'extension** : aucun composant Veka n'a d'accès privilégié.
  Si Veka peut le faire, un utilisateur aussi.
- Tout ce qui est enregistré dans le schéma apparaît **automatiquement** dans le DSL, le Mode
  Simple, l'auto-complétion, la doc et le DesignLab (parité garantie).
- Les composants et styles peuvent être **packagés** en bibliothèques (`UVekaComponentLibrary`)
  partageables entre projets.

---

## 12. Rendu avancé (Phase 4) : Backdrop et Liquid Glass

### 12.1 Capture du backdrop

Besoin : lire, sous le widget, l'image déjà composée (scène 3D + UI dessinée avant).

| Option | Principe | Avis |
|---|---|---|
| A. Post-buffers Slate du moteur (`SlateRHIPostBufferProcessor`, utilisés par `SBackgroundBlur` / UMG Post Buffer) | Slate copie le backbuffer à un point donné et l'expose aux matériaux UI | **à vérifier en 5.8** ; si dispo, la voie la plus simple et la plus sûre |
| B. Élément Slate custom (`ICustomSlateElement`) + RDG | au moment du draw, copie de la zone du backbuffer, passes RDG (downsample, blur), composite | contrôle total, coût d'une passe par groupe d'éléments |
| C. `FSceneViewExtension` | capture la scene color avant l'UI | ne voit que la 3D, pas l'UI en dessous (OK pour un HUD sur la scène) |

**Proposition** : spike A vs B au début de la Phase 4 ; dans tous les cas **une seule capture +
une pyramide de flou (dual Kawase) par frame et par couche**, partagée par tous les widgets
glass de la couche (pas une capture par widget).

### 12.2 Effets

`backdropBlur(radius)`, `frostedGlass(tint, blur, noise)`, `glass(...)` (style Liquid Glass :
réfraction basée sur la **normale du SDF** de la forme — on réutilise nos SDF —, reflet
spéculaire sur le bord, aberration chromatique, teinte), `liquidDistortion(time, strength)`,
réaction au mouvement (pointeur / inclinaison / vitesse de scroll). Shaders `.usf` dans
`VekaUIRender/Shaders`, qualité réglable (scalabilité) et fallback sans backdrop (teinte unie).

---

## 13. Threading (stratégie)

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

## 14. Veka Editor (Phase 3)

### 14.1 Modèle unique

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

### 14.2 Le DSL (VekaLang)

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

### 14.3 Éditeur

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

## 15. Modules et assets

| Module | Type / LoadingPhase | Contenu | Dépendances |
|---|---|---|---|
| `VekaUI` | Runtime / PreDefault | Element, Node, Reconciler, State/Binding/Environment, Schema, layout, modifiers, primitives et contrôles, **Colora**, **Anima**, interpréteur d'AST, `SVekaHost`, `UVekaHostWidget` (bridge UMG) | Core, CoreUObject, Engine, Slate, SlateCore, UMG, InputCore, DeveloperSettings |
| `VekaUIRender` | Runtime / **PostConfigInit** (shaders) | shaders `.usf`, backdrop, blur, glass, éléments Slate custom | RenderCore, RHI, Renderer, SlateRHIRenderer, Projects |
| `VekaLang` | **UncookedOnly** | lexer, parser, printer, analyse, complétion, formateur | VekaUI |
| `VekaImport` | **UncookedOnly** | import SVG (symboles, formes), validation du gabarit symboles, contours de polices pour le texte vectoriel offline | VekaUI, (FreeType du moteur) |
| `VekaEditor` | Editor | Asset Editor `UVekaView`, Symbol Library + Symbol Editor, éditeur de mesh gradient, viewport, outliner, inspecteur, éditeur de code, timeline, Hub, factories, thumbnails | VekaUI, VekaLang, UnrealEd, ToolMenus, PropertyEditor, AssetTools, Slate… |
| `VekaWeb` | Runtime (optionnel) | `WebView` | VekaUI, WebBrowser |

Les modules `VekaColora` / `VekaAnima` séparés ont été écartés : ils sont trop liés au core
(environnement, modifiers, nœuds). Ils existent comme **dossiers et namespaces** (`Veka::Colora`,
`Veka::Anima`) dans `VekaUI` — séparables plus tard si besoin.

**Assets** : `UVekaView`, `UVekaTheme`, `UVekaTimeline`, `UVekaAnimationPreset` (ressorts/courbes
nommés), `UVekaSymbol` + `UVekaSymbolLibrary` (ex-`Veka_Glyph`, désormais vectoriels),
`UVekaFontFamily`, `UVekaComponentLibrary` (composants et styles partageables). Les polices et logos d'Alpha 13 sont
ré-importés (les glyphes passent en SVG, cf. §9) ; les **matériaux** d'Alpha 13 ne sont pas repris tels quels
(remplacés par le rendu §6.4 / shaders `.usf`), mais leurs formules SDF et Colora servent de
référence.

---

## 16. Plan de migration et exécution

### 16.1 Branches

Git interdit d'avoir `C++` **et** `C++/quelque-chose` en même temps (conflit de refs). Proposition :
`C++` reste la branche d'intégration, une branche par phase : `cpp/phase-1-core`,
`cpp/phase-2-colora-anima`, `cpp/phase-3-editor`, `cpp/phase-4-render-hub`, fusionnées dans `C++`
après validation.

### 16.2 Phases

| Phase | Contenu | Livrable / critère |
|---|---|---|
| **0** | Ce document + réponses aux questions §17 | ✅ validation |
| **1** — Core | `VekaUI.uplugin` + 6 modules (vides sauf VekaUI) ; Element / Node / Reconciler ; State / Binding / Environment ; Schema ; `Stack`, `Spacer`, `Grid`, `ScrollView` ; noyau vectoriel `FVekaPath` + formes + **traits** ; primitives `Text`, `Image`, `Shape`, `Color` ; API d'extension (`FVekaPrimitive`, `FVekaLayout`, `FVekaModifier`, `FVekaShape`) utilisée par les composants Veka eux-mêmes ; modifiers layout + apparence de base ; `SVekaHost` + `UVekaHostWidget` ; scheduler Anima minimal ; spike rendu des formes ; tests automation | une vue C++ s'affiche dans un Widget Blueprint et dans un onglet Slate ; une primitive et un conteneur custom écrits « comme un utilisateur » ; tests verts ; compile Win64 Editor + Shipping (Mac si possible) |
| **2** — Colora + Anima | thèmes/tokens/schémas, `FVekaPaint` complet dont **mesh gradient**, OkLCH, ajustements ; **symboles** runtime (masters, graisses, échelles, modes de rendu, animations) ; **typographie** (styles sémantiques, animations de texte, spike texte vectoriel / polices variables) ; **morph** (`matchedGeometry`, `ShapeGroup` fusion SDF) ; Styles de composants ; Anima complet (courbes, ressorts, implicite, transitions, layout, timelines) ; contrôles Alpha 13 (Button, Toggle, Slider, Stepper, Picker, Menu, ContextMenu, TextField, ColorPicker, Gauge, Progress, Label, Divider, Media) ; `UVekaView` + interpréteur | parité fonctionnelle Alpha 13 en C++ ; démo de toutes les animations |
| **3** — Veka Editor | VekaLang (parse/print/analyse/complétion) ; Asset Editor : outliner, inspecteur, viewport, code, state, timeline ; Mode Test ; import SVG + **Symbol Library / Symbol Editor** avec aperçus ; éditeur de mesh gradient ; composants composés en asset (`@Param`/`@Slot`) ; migration des thèmes Alpha 13 | aller-retour Simple ⇄ Avancé sans perte sur la suite de tests ; une vue complète faite à 100 % en Mode Simple et une à 100 % en code |
| **4** — Rendu + Hub | backdrop, blur, glass (y compris **glass fusionné / goutte** via `ShapeGroup`), réfraction, aberration, distorsion, blend modes ; Hub : DesignLab, Exemples, Doc, Changelog | Liquid Glass in-game sur scène 3D ; Hub complet ; budgets perf §13 tenus |

### 16.3 Validation de la compilation

Je n'ai pas Unreal Engine dans mon conteneur. **Décision** : ton PC (Windows, UE 5.8) est
**rattaché à cette session** (Claude desktop › Remote Control, dossier du projet hôte) :

- je lance UBT / `RunUAT BuildPlugin` et les tests automation **directement sur ton PC**, je lis
  les logs et je corrige, sans aller-retour manuel ;
- si besoin j'ouvre l'éditeur (commandlets, scripts Python éditeur, captures d'écran) ;
- la logique sans moteur (parser VekaLang, maths couleur/ressorts/SDF/chemins) est aussi testée
  ici avec un compilateur standard ;
- un runner CI self-hosted reste possible plus tard pour un contrôle propre à chaque push.

À préparer de ton côté au début de la Phase 1 : un **projet hôte** UE 5.8 C++ (ex. `VekaHost`)
avec ce dépôt cloné dans `Plugins/VekaUI`, et Remote Control activé sur ce dossier.

---

## 17. Questions ouvertes

### Décidé (2026-11-20)

| # | Question | Réponse |
|---|---|---|
| 1 | Compilation | **PC Windows rattaché à la session** (builds + tests directs), tests sans moteur ici ; runner CI optionnel plus tard |
| 2 | Nom | **Renommer en `VekaUI`** (`VekaUI.uplugin`, modules `VekaUI`, `VekaUIRender`, `VekaLang`, `VekaImport`, `VekaEditor`, `VekaWeb`) |
| 3 | Logique DSL / Mode Simple | **(b)** : affichage, bindings, `if`/`else`, `ForEach`, actions simples (affectation, toggle, `withAnimation`, `emit`) — pas de fonctions utilisateur |
| 4 | Données du jeu | `SetState` depuis Blueprint **et** `UVekaViewModel` observé |
| 5 | Composants Alpha 13 | **Tous** recréés en Phase 2 |
| 6 | Blend modes | avec le backdrop en Phase 4 ; Normal et Add avant |
| 7 | Plateformes | **Windows** en priorité, **Mac** si possible, le reste en option |
| 8 | Version moteur | **UE 5.8 minimum** |
| 9 | Éditeur de code | uniquement **dans le Veka Editor** (Unreal), pas d'intégration externe |
| 10 | Branche `Blueprint` | **lecture seule** : interdiction d'y écrire ; elle sert de référence |
| 11 | Symboles | refonte complète type **SF Symbols** : SVG, masters et graisses, échelles, alignement auto sur la police, modes de rendu, animations, éditeur et aperçus (§9) ; même logique pour la typo (§10) |
| 12 | Gradient Matrix | remplacé par un **vrai mesh gradient** (§6.5) |
| 13 | Personnalisation | tout extensible : thèmes, styles, composants composés, primitives / conteneurs / modifiers / formes / effets custom (§11) |
| 14 | Morph | `matchedGeometry` + fusion SDF « goutte » (§8.3) |
| 15 | Traits | **strokes / lignes** complets (§8.2) |

### Reste à confirmer

1. **Sources des 19 glyphes Alpha 13** : as-tu les SVG d'origine ? Sinon, on les redessine en SVG
   (ou on part d'un nouveau jeu de symboles).
2. **Jeu de symboles de base** : combien de symboles Veka livrer par défaut (ex. ~150 essentiels :
   flèches, chevrons, check, croix, réglages, recherche, utilisateur, son, wifi, batterie…) ?
3. **Police par défaut** : on garde LTFlode Neue (licence compatible avec une distribution du
   plugin ?) ou une police variable libre (ex. Inter, licence OFL) pour profiter de
   l'interpolation de graisse ?
