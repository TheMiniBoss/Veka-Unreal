# CLAUDE_SYNC — journal de bord partagé

Ce fichier relie deux sessions Claude qui travaillent sur VekaUI :

| Rôle | Où | Fait quoi |
|---|---|---|
| **Claude Cloud** | conteneur Linux, **sans Unreal** | conçoit et code le C++, écrit ici ce qu'il a codé, les fichiers créés et **ce qu'il faut compiler / tester** |
| **Claude Local** | PC Windows de TheMiniBoss, **avec UE 5.8** | lit ce fichier, compile (UnrealBuildTool / RunUAT), lance les tests, inspecte les `.uasset`, corrige les erreurs de compilation liées à l'API Unreal, met à jour ce fichier, `git push` |

> **Depuis le 2026-11-20 (D-002)** : une seule session Claude, sur le PC, tient **les deux rôles** (elle code
> et elle compile). Le journal et les préfixes `[cloud]` / `[local]` restent utilisés pour garder l'historique lisible.

Documents de référence : [`Docs/VEKAUI_PHASE0.md`](Docs/VEKAUI_PHASE0.md) (architecture validée),
[`Docs/CAHIER_DES_CHARGES.md`](Docs/CAHIER_DES_CHARGES.md) (inventaire Alpha 13).

---

## Règles

1. **Branches**
   - Intégration : `C++`. Travail : `cpp/phase-N-<sujet>` (ex. `cpp/phase-1-core`).
   - La branche **`Blueprint` est en lecture seule** pour les deux Claude : jamais de commit,
     de push ni de merge dessus.
   - Toujours `git pull` avant de commencer et avant de pousser. En cas de conflit : `git merge`,
     **jamais** de `push --force` ni de rebase d'une branche partagée.
2. **Qui écrit où dans ce fichier**
   - Claude Cloud ajoute des entrées `CLOUD →` dans le **Journal** et tient à jour **État actuel**.
   - Claude Local ajoute des entrées `LOCAL →` dans le **Journal** et coche les demandes.
   - On **ajoute** des entrées en haut du journal ; on ne réécrit pas les entrées de l'autre.
3. **Ce que Claude Local peut corriger seul**
   - Erreurs de compilation et de link dues à l'API Unreal (includes, noms de modules dans les
     `.Build.cs`, signatures qui ont changé en 5.8, macros UHT, warnings).
   - Corrections minimales, **sans changer le design**. Toute correction est listée dans l'entrée
     `LOCAL →` (fichier + raison).
4. **Ce que Claude Local signale sans le faire**
   - Changement d'architecture, d'API publique, de nommage, suppression de fonctionnalité.
   - → décrire le problème et la proposition dans l'entrée `LOCAL →`, section « Questions pour Cloud ».
5. **Commits** : petits, message clair, préfixe `[cloud]` ou `[local]`.
6. **Logs** : ne pas coller des logs entiers ici. Mettre les **20 premières erreurs** utiles,
   et au besoin le log complet dans `Docs/Sync/Logs/<date>-<sujet>.log` (fichier ignoré si trop gros).
7. **Secrets** : aucun chemin personnel sensible, token ou mot de passe dans ce fichier.

---

## Environnement Local (à remplir par Claude Local)

| Clé | Valeur |
|---|---|
| Version UE | 5.8.4 (build 58902052, Launcher) |
| Chemin moteur | `E:\Epic Games\UE_5.8` |
| Projet hôte | `C:\Users\TheMiniBoss\Documents\Unreal Projects\VekaHost\VekaHost.uproject` (module `VekaHost`, créé à la main, C++ vide) |
| Chemin du plugin | `C:\Users\TheMiniBoss\Documents\Unreal Projects\VekaHost\Plugins\VekaUI` (clone de ce dépôt ; plugin activé dans `VekaHost.uproject` depuis D-002) |
| Compilateur | UBT via `Build.bat` ; compilateur Visual Studio **non détecté** (« Unable to find Visual Studio SDK ») |
| Mac dispo ? | non |

### Commandes de référence (Windows)

Compiler l'éditeur du projet hôte :

```bat
"<UE>\Engine\Build\BatchFiles\Build.bat" VekaHostEditor Win64 Development -Project="<Host>\VekaHost.uproject" -WaitMutex
```

Packager le plugin seul (vérifie Development + Shipping, sans le projet) :

```bat
"<UE>\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="<Host>\Plugins\VekaUI\VekaUI.uplugin" -Package="<Temp>\VekaUI_Build" -TargetPlatforms=Win64 -Rocket
```

Lancer les tests d'automatisation VekaUI :

```bat
"<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<Host>\VekaHost.uproject" -ExecCmds="Automation RunTests VekaUI; Quit" -unattended -nopause -nullrhi -nosplash -log -ReportExportPath="<Temp>\VekaUI_Tests"
```

Tests de rendu (si demandé) : même commande **sans** `-nullrhi`.

---

## État actuel

| | |
|---|---|
| Phase | **2** — jalons **2.0**, **2a Colora**, **2b Anima** ✅ fusionnés dans `C++` ; prochain jalon **2c Typographie** |
| Branche active | `C++` (2c : `cpp/phase-2c-typo`, pas encore créée) |
| Code C++ | `VekaUI.uplugin` + 6 modules ; runtime Phase 1 + Colora + Anima ; 30/30 tests automation ✅ |
| Prochaine étape | 2c Typographie (LTFlode Neue) |
| Bloquant | aucun (point à vérifier à la main : clics répétés sur « +1 », voir `LOCAL → D-002`) |

---

## Demandes ouvertes pour Claude Local

- [x] **D-002 — Compiler et tester la Phase 1** (branche `cpp/phase-1-core`) — ✅ fait le 2026-11-20 (voir `LOCAL → D-002` ; étape 7 optionnelle non faite)
  1. `git fetch` puis `git checkout cpp/phase-1-core` dans `VekaHost/Plugins/VekaUI`. Le plugin s'appelle maintenant **`VekaUI.uplugin`**.
  2. Activer le plugin dans `VekaHost.uproject` : `"Plugins": [{ "Name": "VekaUI", "Enabled": true }]`.
  3. Fermer tous les éditeurs UE, puis compiler `VekaHostEditor Win64 Development`.
  4. Corriger les erreurs **d'API Unreal** seulement (cf. Règles §3), une par une, en notant chaque correction.
     Liste des points dont Cloud n'est pas sûr : voir l'entrée `CLOUD → Phase 1` ci-dessous.
  5. Lancer `Automation RunTests VekaUI` (commande de référence) et noter le résultat des 13 tests.
  6. Ouvrir l'éditeur `VekaHost`, menu **Window > Tools > VekaUI Playground** : vérifier l'affichage
     (formes, texte qui passe à la ligne, grille, scroll à la molette), cliquer « +1 », la piste du ressort,
     l'anneau (trim animé). Une capture d'écran dans `Docs/Sync/Screens/phase1-playground.png` si possible.
  7. Optionnel : dans un Widget Blueprint de test, ajouter un widget **Veka Host** avec
     `ViewClass = Veka Playground` et vérifier qu'il s'affiche dans le Designer.
  8. `BuildPlugin` Win64 (Development + Shipping) si l'éditeur compile.
  9. Entrée `LOCAL →`, cocher D-002, commit `[local] D-002 …`, push sur `cpp/phase-1-core`.

- [x] **D-001 — Préparer l'environnement** (avant tout code) — ✅ fait le 2026-11-20 (voir `LOCAL →` ci-dessous)
  1. Vérifier que UE **5.8** est installé et noter sa version exacte.
  2. Créer (ou réutiliser) un projet hôte **C++** vide `VekaHost` en UE 5.8.
  3. Cloner ce dépôt dans `VekaHost/Plugins/VekaUI` et faire `git checkout C++`.
  4. Vérifier que le projet hôte compile **sans** le plugin activé (commande « Compiler l'éditeur »).
  5. Remplir le tableau **Environnement Local** ci-dessus.
  6. Ajouter une entrée `LOCAL →` dans le journal, cocher D-001, commit `[local] D-001 environnement prêt`, push sur `C++`.

---

## Journal

<!-- Ajouter les nouvelles entrées EN HAUT, juste sous ce commentaire. -->

### 2026-11-20 — CLOUD + LOCAL → jalon 2b Anima

- **Branche** : `cpp/phase-2b-anima` (partie de `C++` @ `bceb7ad`), fusionnée dans `C++`.
- **Codé** :
  - **Animations implicites** (`Anima/VekaImplicit.h`) : un nœud garde les valeurs de son dernier élément (*cibles*) et
    celles qu'il dessine (*présentées*). Dans une transaction, chaque UPROPERTY `float` / `double` / `FLinearColor`
    modifiée du primitif et des modifiers (trouvée par réflexion, structs imbriquées et tableaux de même taille
    compris) s'anime de la valeur présentée vers la cible ; sinon elle saute. Donc : tailles de frame, rayons,
    couleurs et dégradés (Oklab), offset / scale / rotation, opacité, trim, padding, taille de police… sans code par
    propriété. Les valeurs « non définies » (-1 des frames) et infinies ne s'animent pas.
  - **Transactions** : `.Animation(spec, valeur)` (anime le nœud et son sous-arbre quand la valeur change) et
    `Veka::WithAnimation(spec, [&]{ ... })` (les vues re-évaluées à cause des états modifiés dans le bloc s'animent).
    Retarget sans saut (les ressorts gardent leur vitesse).
  - **Transitions** d'insertion / suppression (`Anima/VekaTransition.h`, enums Alpha 13) : Identity, Opacity, Scale,
    Slide, Move(bord), Push(bord), Offset, Blur (approximé par l'opacité jusqu'à la Phase 4), Combined, Asymmetric.
    `.Transition(...)`, Opacity par défaut. Un enfant supprimé reste dessiné à son dernier rect (sans input, sans
    re-évaluation) jusqu'à la fin de sa transition ; s'il revient pendant ce temps, le même nœud (et son état) est
    réutilisé.
  - **Transitions de layout** : dans le frame qui suit une transaction, les rects qui changent s'animent
    (`FVekaNode::PlaceAt`).
  - **Timelines** (`Anima/VekaTimeline.h`) : pistes à keyframes (easing par segment), événements, boucle, vitesse,
    Play / Pause / Stop / Seek / Reverse ; `Ctx.Timeline(nom, data)` dans un Body, valeurs lues avec `Value(piste)`.
    Asset `UVekaTimeline`. Le ciblage direct `NodeId.PropertyPath` viendra avec le Veka Editor (Phase 3).
  - **Spec** : courbe `Custom` (`UCurveFloat`, `FVekaAnimationSpec::FromCurve`). `TVekaState::Animate` accepte aussi
    `FVector2D`, `FLinearColor` (Oklab), `FMargin` (trait `TVekaAnimatable<T>`).
- **Fichiers clés** : `Private/Anima/VekaImplicit.cpp`, `Private/Core/VekaRuntime.cpp` (transactions, fantômes),
  `Private/Core/VekaNode.cpp` (`PlaceAt`, `RefreshEnvironment`, transitions dans le render transform),
  `Private/Slate/SVekaNodeWidget.cpp` (enfants sortants, opacité de transition).
- **Tests** : 30/30 ✅ — 6 nouveaux `VekaUI.Anima.*` : Implicit, WithAnimation (dont retarget), Transitions
  (suppression, insertion, retour pendant la sortie, sans transaction), LayoutTransition, Timeline (événements,
  inverse, seek, boucle), CustomCurveAndTypes.
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérif visuelle** (`Docs/Sync/Screens/phase2b-anima.png`) : carte implicite (taille, couleur, rayon, rotation,
  police), liste (ajout en haut avec Push, suppression avec Scale + Opacity, les autres lignes glissent), galerie des
  8 transitions (aller-retour), 6 courbes comparées, timeline (événements « Peak » puis « Done », arrêt à 2,4 s).
- **Limites connues** :
  - Pendant une transition de layout, un texte reçoit la taille animée : un bouton dont le libellé change peut couper
    son mot sur deux lignes pendant l'animation (SwiftUI garde le texte à sa taille finale).
  - Une valeur animée qui modifie l'environnement (ex. couleur de `.Foreground`) est propagée aux enfants sans
    re-exécuter leurs Body.
  - Animations de mesh gradient par point : passent par les tableaux de même taille (ok), pas de morph entre des
    grilles de tailles différentes.

### 2026-11-20 — CLOUD + LOCAL → jalon 2a Colora

- **Branche** : `cpp/phase-2a-colora` (partie de `C++` @ `9654c0d`), fusionnée dans `C++`.
- **Codé** :
  - **Maths couleur** (`Colora/VekaColor.h`) : sRGB ↔ linéaire, **Oklab / OkLCH**, interpolation par espace
    (`EVekaColorSpace` : SRGB, Linear, OkLab par défaut, OkLCH avec teinte au plus court), alpha prémultiplié (pas de
    frange sombre vers le transparent). `FVekaColorMatrix` 4×5 (luminosité, contraste, saturation, rotation de teinte,
    niveaux de gris), composable.
  - **Thèmes et tokens** (`Colora/VekaTheme.h`) : `UVekaTheme` (data asset : `Parent`, `Colors` token → paint Light/Dark,
    `Metrics`), 26 tokens intégrés (noms des presets Colora Alpha 13, valeurs de la palette système — les valeurs exactes
    d'Alpha 13 sont dans l'asset `Colora_System` de la branche `Blueprint`, à recaler si besoin), hiérarchie
    primary/secondary/tertiary/quaternary/transparent. Un token peut valoir un dégradé ou un autre token ; les stops et
    les points de mesh acceptent aussi des tokens. Résolution par l'Environment (thème + schéma) au moment du paint.
  - **`FVekaPaint` complet** : linéaire 2D (vrai, plus seulement horizontal/vertical), radial, elliptique, angulaire,
    **mesh gradient** (grille W×H, positions animables, interpolation bilinéaire ou bicubique Catmull-Rom, inversion des
    grilles déformées par Newton), image (étirée / tuilée / 9-slice, coins arrondis), matériau, token.
  - **Tessellateur Colora** (`Colora/VekaPaintRenderer.h`) : formes convexes couvertes par une grille découpée
    (Sutherland-Hodgman), non convexes triangulées puis grillées ; couleur par sommet calculée dans l'espace du paint ;
    **frange antialiasée de 1 px** ; traits en dégradé (couleur par point). Remplace la ligne d'1 px de la Phase 1.
  - **Modifiers** : `.Theme(...)`, `.Brightness/.Contrast/.Saturation/.HueRotation/.Grayscale` (via `.ColorAdjust`),
    `.BlendMode(Normal | Add)`, `.Foreground(...)` accepte un token. `UVekaHostWidget::Theme` + `SetTheme`.
  - **Additif** : couleurs prémultipliées avec alpha 0 + `ESlateDrawEffect::PreMultipliedAlpha` (vérifié à l'écran :
    R+V+B = blanc).
- **Correction de comportement (Phase 1)** : les modifiers d'environnement d'un même nœud s'appliquaient de l'intérieur
  vers l'extérieur, donc `.Foreground(Rouge).Foreground(Bleu)` donnait bleu. Ils s'appliquent maintenant de l'extérieur
  vers l'intérieur (le plus intérieur gagne, comme SwiftUI). Fichier : `Private/Core/VekaRuntime.cpp` (`EvaluateNode`).
- **Bugs trouvés en route** : ajout d'un élément d'un tableau à lui-même dans `StrokePolylines` (crash attrapé par le test
  UMG du Playground avant l'éditeur) ; encoches sur les traits fermés (points doublons aux jonctions de Bézier + point de
  fermeture non raccordé par `MakeLines`).
- **Tests** : 24/24 ✅ — 8 nouveaux `VekaUI.Colora.*` : OkLab (valeurs de référence d'Ottosson), Interpolation,
  ColorMatrix, Tokens (chaîne de thèmes, token → dégradé → token, métriques, token inconnu), GradientSampling,
  MeshGradient (grille déformée), Modifiers (ordre, composition), Render (sommets d'un dégradé, frange, additif,
  saturation 0).
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérif visuelle** : section Colora du Playground (`Docs/Sync/Screens/phase2a-colora.png`) — dégradés, comparaison
  sRGB / Oklab / OkLCH, ajustements, cercles additifs, anneau angulaire, mesh animé (tap), carte de tokens qui bascule
  clair / sombre.
- **Limites connues (pour plus tard)** :
  - La tessellation des dégradés est refaite à chaque frame (pas de cache) : à mettre en cache par forme + taille.
  - Dégradé angulaire : la couture de départ peut montrer une transition de ≤ 6 px si la première et la dernière couleur
    diffèrent.
  - Images et matériaux : les ajustements de couleur ne s'appliquent qu'à la teinte ; matériaux et textures sur un
    chemin quelconque sont dessinés dans le rectangle (masques = module de rendu, Phase 4).
  - Mesh gradient : pas encore de poignées de Bézier par point (éditeur de mesh, Phase 3).
  - Texte en dégradé : couleur moyenne pour l'instant (2c).

### 2026-11-20 — LOCAL → jalon 2.0 : tests du chemin Slate et du bridge UMG

- **Branche** : `cpp/phase-2.0-tests` (partie de `C++` @ `ede2f52`), fusionnée dans `C++`.
- **Nouveau fichier** : `Source/VekaUI/Private/Tests/VekaUISlateTests.cpp`. Les tests peignent dans une liste d'éléments
  hors écran (fenêtre non enregistrée) et appellent directement les handlers souris des widgets : ils tournent en
  `-nullrhi`, sans fenêtre OS.
  - `VekaUI.Slate.PaintModifiers` : peint une feuille avec padding + background + border ; vérifie le fond, et que la
    bordure a un remplissage **transparent**, la bonne couleur et la bonne épaisseur.
  - `VekaUI.Slate.UMGHostPlayground` : `UVekaHostWidget` + `ViewClass = UVekaPlaygroundViewSource` → `TakeWidget()`
    renvoie un `SVekaHost`, la racine existe, le Playground se peint (ce que fait le Designer d'un Widget Blueprint).
  - `VekaUI.Slate.TapAndDoubleClick` : sur le vrai « +1 » du Playground (onPress + onTap + scale), tap → 1,
    double-clic → 2, tap → 3, avec une frame (flush) entre l'appui et le relâchement.
- **Contre-épreuve** (bugs réintroduits temporairement) : sans le fix bordure → `PaintModifiers` échoue ; sans le fix
  double-clic → `TapAndDoubleClick` échoue ; sans l'`Arrange()` en tête de `OnPaint` → `PaintModifiers` reproduit le
  **crash exact** de l'éditeur (`GetModifierRect` hors bornes). Code restauré ensuite.
- **Tests** : 16/16 ✅. **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Pas fait** : vérification dans le Designer d'un vrai Widget Blueprint (remplacée par le test UMG ci-dessus) ;
  clics répétés sur « +1 » à la vraie souris → à faire par TheMiniBoss. Les handlers sont maintenant couverts par
  les tests ; si des clics se perdent encore à la main, le problème est en amont (routage Slate / OS).

### 2026-11-20 — CLOUD → décisions police et glyphes (réponses de TheMiniBoss)

- **Police par défaut : LTFlode Neue** (12 graisses Alpha 13). Licence de distribution à vérifier par
  TheMiniBoss avant une diffusion publique du plugin.
  → 2c : chemin Slate en graisses discrètes ; le spike texte vectoriel vérifie si les faces de LTFlode ont des
  contours compatibles pour interpoler la graisse (sinon fondu entre faces voisines). Les symboles (2d)
  s'alignent sur les métriques de LTFlode.
- **Pas de SVG d'origine pour les 19 glyphes** : ils sont redessinés en SVG au gabarit Veka en 2d (textures
  Alpha 13 de la branche `Blueprint`, en lecture seule, comme référence visuelle).
- Reste ouvert : nombre de symboles de base (proposition ~150) — ne bloque que la fin de 2d.
- Détail : `Docs/VEKAUI_PHASE0.md` §17 (décisions 16 et 17).

### 2026-11-20 — CLOUD → plan de la Phase 2 (proposition, à valider)

La Phase 2 de `VEKAUI_PHASE0.md` §16.2 est trop grosse pour une seule branche. Découpage proposé en 7 jalons,
**une branche par jalon** (`cpp/phase-2a-colora`, …), chacun fusionné dans `C++` quand il est vert
(compile Editor + Shipping, tests, vérif visuelle dans le Playground).

| Jalon | Contenu | Dépend de | Critère « vert » |
|---|---|---|---|
| **2.0 Dette Phase 1** | test automatisé du chemin Slate (fenêtre virtuelle, paint + clics synthétiques), test du Widget Blueprint `Veka Host`, vérif « +1 » à la souris | — | les bugs 2 et 4 de D-002 couverts par des tests |
| **2a Colora** | `UVekaTheme` (tokens sémantiques, schémas Light/Dark/custom, héritage), tokens dans l'Environment ; `FVekaPaint` complet (linéaire 2D vrai, radial, elliptique, angulaire, image/9-slice, material) ; OkLab/OkLCH ; ajustements par primitive ; **shader Veka partagé** (`VekaUIRender`, `MakeCustomVerts`, AA analytique, coins continus) ; **mesh gradient** (patchs de Coons, cache) ; blend Normal + Add | — | démo Colora dans le Playground ; tests maths couleur |
| **2b Anima** | `TVekaAnimatable<T>` (float, vec2, couleur, marges, rayons, paint, rect) ; implicite `.animation(spec, value)` + `WithAnimation` ; transitions d'insertion/suppression (enums Alpha 13, combinées, asymétriques) ; transitions de layout ; `UVekaTimeline` (pistes, keyframes, événements) ; `UCurve*` ; speed/repeat | 2a (paint animable) | démo de toutes les animations ; tests scheduler/interpolation |
| **2c Typographie** | styles sémantiques + `UVekaFontFamily` ; modifiers texte (weight, tracking, lineSpacing, textCase, truncation, minimumScaleFactor…) ; `.contentTransition(.numericText)` ; effets par glyphe ; **spike** texte vectoriel FreeType / polices variables | 2b | démo texte ; rapport du spike |
| **2d Symboles** | `UVekaSymbol` / `UVekaSymbolLibrary` ; masters interpolés (9 graisses), 3 échelles, alignement sur la police ; modes de rendu ; valeur variable ; effets (appear, bounce, pulse, replace / magic replace, wiggle…) ; import SVG minimal (l'éditeur = Phase 3) | 2c, réponses aux questions ci-dessous | jeu de symboles de base affiché dans toutes les graisses |
| **2e Contrôles + Styles** | `Button`, `Toggle`, `Slider`, `Stepper`, `Picker`, `Menu`, `ContextMenu`, `TextField`, `ColorPicker`, `Gauge`, `ProgressView`, `Label`, `Divider`, `MediaView` ; styles de composants (configuration `isPressed`/`isOn`/…) ; focus et clavier | 2b, 2c, 2d | parité Alpha 13 des contrôles |
| **2f Morph** | `matchedGeometry(id, namespace)` ; `ShapeGroup` / `.morphGroup` (fusion SDF smooth-min) avec hit-test fusionné | 2a (shader), 2b | démo bouton → carte et « goutte » |
| **2g UVekaView + bridge** | asset `UVekaView`, `FVekaValue`, interpréteur d'AST minimal (préparation de VekaLang en Phase 3) ; `SetState` Blueprint, `UVekaViewModel`, `OnVekaEvent` | 2e | une vue asset affichée dans un Widget Blueprint et pilotée depuis Blueprint |

Ordre conseillé : **2.0 → 2a → 2b → 2c → 2d → 2e → 2f → 2g** (2f peut passer avant 2e si on veut le « wow » plus tôt).

**Questions pour TheMiniBoss** (bloquent 2c / 2d, cf. Phase 0 §17 « Reste à confirmer ») :
1. As-tu les **SVG d'origine des 19 glyphes Alpha 13** ? Sinon on les redessine, ou on part d'un nouveau jeu.
2. Combien de **symboles de base** livrer (proposition : ~150 essentiels) ?
3. **Police par défaut** : LTFlode Neue (licence compatible avec la distribution du plugin ?) ou une police variable
   libre (ex. Inter, OFL) pour l'interpolation de graisse ?

### 2026-11-20 — LOCAL → D-002 Phase 1 compilée, testée, vérifiée dans l'éditeur

- **Demande** : D-002 — ✅ OK (étape 7 optionnelle « Widget Blueprint » non faite)
- **Branche / commit testé** : `cpp/phase-1-core` @ `873ba1f` → corrigé en `1f6e3ff`
- **Plugin** : activé dans `VekaHost.uproject` (`{ "Name": "VekaUI", "Enabled": true }`).
- **Compilation** : `VekaHostEditor Win64 Development` ✅ du premier coup — 6 modules, **0 erreur, 0 warning de code**.
  Aucun des 10 « points d'attention API UE 5.8 » de Cloud n'a posé problème (FInstancedStruct, USTRUCT virtuels,
  MakeGradient/MakeCustomVerts, FSlateVertex, TSlotlessChildren, ToPaintGeometry, FontMeasure, EAutomationTestFlags
  constexpr, FSlateRenderTransform, AddShaderSourceDirectoryMapping). Seul message : `[Upgrade] IncludeOrderVersion`
  sur la cible du **projet hôte** (pas le plugin).
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL` — UnrealEditor Development + UnrealGame Shipping, **0 warning**
  (fait sur le commit des corrections ci-dessous).
- **Tests** : 13/13 ✅ (`Automation RunTests VekaUI`). Les deux `LogAutomationTest: Error: Condition failed` du log
  viennent des auto-tests internes du moteur au démarrage, avant le chargement de VekaUI.
- **Corrections faites** (ce sont des bugs, pas de l'API UE — la session tient maintenant les deux rôles) :
  1. `Private/Vector/VekaPath.cpp` (`OpenPoints`) : `Points.Add(Points[0])` → crash
     `Attempting to use a container element which already comes from the container being modified` dans le test
     `VekaUI.Vector.PathOps`. Remplacé par `Points.Add(Polyline.Points[0])`.
  2. `Private/Slate/SVekaNodeWidget.cpp` : **crash de l'éditeur à l'ouverture du Playground**
     (`GetModifierRect` hors bornes). `Arrange()` (qui remplit `ModifierRects`) n'était appelé que par
     `OnArrangeChildren`, lui-même appelé **après** la peinture des modifiers, et jamais pour une feuille.
     → `OnPaint` appelle `Arrange()` en premier ; nouvelle fonction `ArrangeChildWidgets()` (sans ré-arranger) utilisée
     par `PaintContent` et `OnArrangeChildren`.
  3. `Private/Core/VekaPaint.cpp` (`DrawOutline`) : `.Border(...)` peignait un **rectangle blanc plein** qui cachait le
     contenu. Un élément RoundedBox Slate est rempli avec la teinte passée à `MakeBox` (la `TintColor` de la brush est
     ignorée) → teinte `FLinearColor::Transparent`.
  4. `Private/Slate/SVekaNodeWidget.{h,cpp}` : ajout de `OnMouseButtonDoubleClick` → `OnMouseButtonDown`. Slate envoie le
     2ᵉ clic d'une paire rapide en double-clic : sans ça, un tap sur deux était perdu (comme `SButton`).
- **Vérification visuelle** (onglet Playground, capture `Docs/Sync/Screens/phase1-playground.png`) :
  - ✅ formes, texte qui passe à la ligne, grille adaptative (passe à la ligne quand la fenêtre rétrécit), scroll molette,
    ressort sur la piste, trim animé de l'anneau (bascule plein ↔ vide : voulu), bordures Spacer/alignement (après fix 3).
  - ✅ « +1 » incrémente (logs : down → up → OnTap → état → re-évaluation).
  - ⚠️ **À vérifier à la main** : avec l'outil de clic automatique, certains clics répétés sur « +1 » n'atteignent
    **aucun** widget de la fenêtre (même pas la ScrollView racine, qui recevrait l'événement par bubbling si seul le
    hit-test de « +1 » ratait). L'événement semble perdu avant Slate → probablement un artefact de l'outil, pas de VekaUI.
    Cliquer 5-6 fois sur « +1 » à la vraie souris pour confirmer.
  - Note : en UE 5.8 l'onglet est dans le menu principal **Tools** (catégorie Tools), pas dans Window > Tools.
- **Erreurs non corrigées** : aucune.
- **Dette relevée pour la suite** :
  - Pas de test automatisé du chemin Slate (paint + entrée souris) : le crash 2 et le bug 4 n'étaient pas couverts.
    À ajouter (fenêtre virtuelle + `FSlateApplication::Process*Event` synthétiques).
  - `.Border(..., 8)` : rayon peu visible sur la capture, à revérifier avec la refonte du rendu des formes (2a).
- **Prochaine étape** : fusion dans `C++`, puis plan de la Phase 2 (entrée suivante).

### 2026-11-20 — CLOUD → Phase 1 (core) écrite, à compiler

- **Branche** : `cpp/phase-1-core` (partie de `C++` @ `95e3356`)
- **Réponses aux questions de Local (D-001)** : le projet hôte `VekaHost` créé à la main convient. Ne pas
  utiliser `Documents\Unreal Projects\Veka\`. Le jeton Epic vu dans `Demo.log` : ne jamais le committer.
- **Codé** :
  - Plugin renommé `VekaUI.uplugin` ; modules `VekaUI` (Runtime, Default), `VekaUIRender` (PostConfigInit,
    mappe `/Plugin/VekaUI` → `Shaders/`), `VekaLang` et `VekaImport` (UncookedOnly, vides), `VekaEditor`
    (onglet Playground), `VekaWeb` (vide).
  - Cœur déclaratif : `FVekaElement` (description légère, copy-on-write), `FVekaNode` (arbre retenu,
    état, layout), `FVekaRuntime` (réconciliateur par clé, re-évaluation groupée des vues sales, FGCObject),
    `FVekaView` + `VEKA_VIEW`, `FVekaContext::State<T>`, `TVekaState` / `TVekaBinding`, `FVekaEnvironment`.
  - API d'extension : `FVekaPrimitive` et `FVekaModifier` sont des USTRUCT polymorphes (virtuels) stockés
    dans `FInstancedStruct` ; `FVekaSchema` les découvre par réflexion.
  - Layout type SwiftUI (proposition → taille) : `FVekaStack` (H/V/Z en un seul type, priorités),
    `FVekaSpacer`, `FVekaGrid` (fixed / flexible / adaptive), `FVekaScrollView` (molette, indicateurs).
  - Primitives : `FVekaText` (retour à la ligne, limite de lignes, « … »), `FVekaImage`, `FVekaShapeView`
    (rect, arrondi, continuous, capsule, cercle, ellipse, path ; hit-test exact par SDF).
  - Vecteur : `FVekaPath` (Bézier, arcs, parseur SVG `d=`), trim, pointillés, triangulation, rendu
    (`MakeCustomVerts` + `MakeLines`).
  - Modifiers : padding, frame (fixe/flexible), layoutPriority, background, border, shadow (approx.),
    opacity, clip, offset/scale/rotation (render transform), foreground, font, colorScheme, onTap/onHover/onPress.
  - Anima minimal : `FVekaAnimationSpec` (courbes, Bézier, ressort SwiftUI response/dampingFraction ou
    masse/raideur/amortissement, delay, repeat, autoreverse), `FVekaAnimaScheduler` (un par runtime, retarget
    qui garde la vitesse), `TVekaState::Animate`.
  - Slate : `SVekaNodeWidget` (un seul widget par nœud, modifiers peints niveau par niveau),
    `SVekaHost` (active timer seulement quand il y a du travail). UMG : `UVekaHostWidget` + `UVekaViewSource`.
  - Démo : `Veka::Samples::Playground()`, `UVekaPlaygroundViewSource`, onglet éditeur.
  - Tests : `VekaUI.Anima.{Easing,Spring,Scheduler}`, `VekaUI.Vector.{Svg,PathOps,ShapeHitTest}`,
    `VekaUI.Layout.{Stack,Modifiers,Grid}`, `VekaUI.Core.{State,ReconcileKeys,Schema}`,
    `VekaUI.Primitives.TextLayout`.
- **Fichiers** : `VekaUI.uplugin`, `Shaders/Private/VekaCommon.ush`, `Source/**` (≈ 75 fichiers), `README.md`.
- **À compiler** : `VekaHostEditor Win64 Development` (puis BuildPlugin Shipping).
- **À tester** : D-002.
- **Points d'attention API UE 5.8** (Cloud n'a pas pu vérifier ; corriger si besoin, sans changer le design) :
  1. `FInstancedStruct` : include `StructUtils/InstancedStruct.h` (CoreUObject) ; `AddStructReferencedObjects`,
     `GetPtr<T>` / `GetMutablePtr<T>`, `operator==`.
  2. USTRUCT avec fonctions virtuelles (`FVekaPrimitive`, `FVekaModifier`) dans `FInstancedStruct`
     (comme `FTableRowBase`). Si UHT refuse quelque chose, le signaler avant de restructurer.
  3. `FSlateDrawElement::MakeLines` avec `TArray<FVector2f>`, `MakeGradient` (signature + orientation :
     vérifier qu'un dégradé « horizontal » va bien de gauche à droite), `MakeCustomVerts`, `MakeText`.
  4. Champs de `FSlateVertex` (`Position`, `TexCoords[4]`, `MaterialTexCoords`, `Color`, `SecondaryColor`,
     `PixelSize[2]`) et brush `GenericWhiteBox` de `FCoreStyle` pour les custom verts.
  5. `TSlotlessChildren<SWidget>` (constructeur `Children(this)`, `GetChildAt`, `Add`, `Empty`).
  6. `FGeometry::ToPaintGeometry(FVector2f, FSlateLayoutTransform)` et `MakeChild(...)`.
  7. `FSlateFontMeasure::Measure` / `GetMaxCharacterHeight`, `FSlateFontInfo::operator==`.
  8. `EAutomationTestFlags` combiné en `constexpr` dans les tests.
  9. `FSlateRenderTransform` : `FScale2f`, `FQuat2f`, `Concatenate`.
  10. `AddShaderSourceDirectoryMapping` (RenderCore, `ShaderCore.h`) depuis un module PostConfigInit.
- **Demande** : D-002.

### 2026-11-20 — LOCAL → D-001 environnement prêt

- **Demande** : D-001 — ✅ OK
- **Branche / commit testé** : `C++` @ `d3cb764`
- **Projet hôte créé** : `Documents\Unreal Projects\VekaHost\` (UE 5.8, module `VekaHost`, fichiers écrits à la main : `.uproject`, `Source/VekaHost.Target.cs`, `Source/VekaHostEditor.Target.cs`, `Source/VekaHost/*`, `Config/*`). Pas de plugin activé dans `.uproject`.
- **Plugin** : `Plugins\VekaUI` cloné depuis `https://github.com/TheMiniBoss/Veka.git` branche `C++`. Le `.uplugin` s'appelle encore `Veka.uplugin` (renommage `VekaUI.uplugin` = prochaine étape Cloud).
- **Compilation** : `VekaHostEditor Win64 Development`, **sans** plugin activé → **Result: Succeeded** (49 s, 0 erreur).
- **Corrections faites** :
  - Premier essai en `BuildSettingsVersion.V6` + `IncludeOrderVersion` : échec UBT (« modifies the values of properties … shared build products with UnrealEditor »). Passé en `V7`, sans `IncludeOrderVersion`, comme le template `Veka` existant. Fichiers : `Source/VekaHost*.Target.cs`.
- **Erreurs rencontrées (résolues)** :
  - `Unable to build while Live Coding is active` : l'éditeur `Demo` relancé avait Live Coding actif. Résolu en fermant l'éditeur (fermeture gracieuse), puis recompilé. **Règle** : toujours fermer tous les éditeurs UE avant de compiler.
- **Relance** : éditeur `VekaHost` relancé (titre de fenêtre « Unreal Engine 5.8 » juste après le lancement ; log projet non vérifié).
- **Erreurs non corrigées** : aucune
- **Questions pour Cloud** :
  1. Le projet `VekaHost` est une création manuelle (pas d'assistant UE). Ça convient, ou tu veux un projet généré par l'éditeur ?
  2. Il existe aussi un projet `Documents\Unreal Projects\Veka\` (module `Veka`, UE 5.8, non versionné). Je ne l'ai pas utilisé comme hôte pour éviter un conflit de nom avec le plugin `Veka`. OK ?
- **Prochaine étape** : Cloud crée `cpp/phase-1-core` et renomme le plugin en `VekaUI.uplugin` ; Local compilera ensuite le plugin dans `VekaHost` (activé dans `.uproject`).

### 2026-11-20 — LOCAL → premier build et analyse de l'environnement

- **Demande** : demande de l'utilisateur (hors D-001) — ⚠️ partiel : build OK, D-001 non fait
- **Environnement constaté** : UE **5.8.4** (`E:\Epic Games\UE_5.8`), projet `Demo\Demo.uproject` (module `Demo`, non suivi par git), Windows 11 25H2
- **Branche / commit testé** : `C++` @ `46d91d2` (pull fast-forward)
- **Procédure** : éditeur `Demo` sauvegardé/fermé (fermeture gracieuse, sans blocage), compilé, relancé
- **Compilation** : `DemoEditor Win64 Development` → **Result: Succeeded** (4,56 s, 0 erreur, 1 avertissement : « Unable to find Visual Studio SDK. Editor integration will be disabled » — environnement, sans impact sur le code)
- **Corrections faites** : aucune (aucune erreur de compilation)
- **Plugin Veka** : pas encore compilé. Le plugin n'est pas dans `Demo\Plugins\` ; il faut le cloner dans un projet hôte `VekaHost` (D-001).
- **Erreurs non corrigées** : aucune
- **Points de vigilance** :
  - `Demo\Saved\Logs\Demo.log` contient un jeton d'authentification Epic en clair (ligne `LogCsvProfiler … commandline`). Il ne doit pas être commité ni partagé ; à régénérer / nettoyer. Non copié ici.
  - `Demo\` n'est pas suivi par git (non ignoré) : à décider si on l'ajoute ou on l'ignore.
- **Questions pour Cloud** : faut-il que D-001 soit fait dans un projet `VekaHost` séparé (recommandé par le protocole) ou peut-on réutiliser `Demo` comme hôte ? Quel est le dossier attendu pour le clone du plugin ?
- **Prochaine étape** : D-001 (créer `VekaHost`, cloner le dépôt dans `Plugins\VekaUI`, compiler sans plugin).

### 2026-11-20 — CLOUD → création du journal

- **Fait** : Phase 0 (audit Alpha 13 + architecture VekaUI) dans `Docs/VEKAUI_PHASE0.md`, validée.
  Décisions clés : plugin renommé `VekaUI`, modules `VekaUI` / `VekaUIRender` / `VekaLang` /
  `VekaImport` / `VekaEditor` / `VekaWeb`, UE 5.8 minimum, Windows d'abord puis Mac.
- **Fichiers créés** : `CLAUDE_SYNC.md` (ce fichier). Aucun fichier C++.
- **À faire par Local** : D-001.

---

## Modèles d'entrées

```markdown
### AAAA-MM-JJ — CLOUD → <sujet>
- **Branche / commit** : `cpp/phase-1-core` @ `abc1234`
- **Codé** : <résumé en 2-5 lignes>
- **Fichiers créés / modifiés** :
  - `Source/VekaUI/Public/Core/VekaElement.h` (nouveau)
  - …
- **À compiler** : Editor Win64 Development (+ BuildPlugin Shipping si demandé)
- **À tester** : `Automation RunTests VekaUI.Core` ; vérif visuelle : <quoi regarder>
- **Points d'attention API UE** : <ce dont Cloud n'est pas sûr, ex. signature en 5.8>
- **Demande** : D-00X
```

```markdown
### AAAA-MM-JJ — LOCAL → <sujet>
- **Demande** : D-00X — ✅ OK / ⚠️ partiel / ❌ échec
- **Branche / commit testé** : `cpp/phase-1-core` @ `abc1234` → corrigé en `def5678`
- **Compilation** : Editor ✅ / Shipping ✅ (warnings : N)
- **Tests** : 12/12 ✅ (ou liste des échecs)
- **Corrections faites** (API UE seulement) :
  - `VekaHost.cpp:42` : include manquant `Framework/Application/SlateApplication.h`
- **Erreurs non corrigées** (20 max) :
  - `…`
- **Questions pour Cloud** : …
- **Captures / logs** : `Docs/Sync/Logs/…`
```
