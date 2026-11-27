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
| Dépôt GitHub | **`https://github.com/TheMiniBoss/Veka-Unreal`** depuis le 2026-11-27 (`origin` du plugin et du clone `Documents\GitHub\Veka`). L'ancien `TheMiniBoss/Veka` est maintenant **un autre projet** (Veka natif en Rust sur `main`) : ne pas y pousser ; ses branches `C++` et `cpp/fix-effects-clip` sont des restes d'un push du 2026-11-27 (copies identiques dans `Veka-Unreal`). |
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
| Phase | **4** ✅ terminée (4.0 Spike backdrop, 4a Backdrop + flou, 4b Glass, 4c Blend modes + distorsion, 4d Hub + DesignLab, 4e Hub Learn, 4f Perf + démo in-game fusionnés ; budgets §13 tenus) ; **3** ✅ terminée — 3.0 Document v2, 3a VekaLang (langage `.vk`), 3b Asset Editor, 3c onglet Code, 3d Mode Simple, 3e Mode Test + Timeline, 3f Symboles et peintures et **3g Composants et migration** fusionnés dans `C++` (Phase 2 ✅ terminée) |
| Branche active | `C++` |
| Code C++ | `VekaUI.uplugin` + 6 modules ; runtime complet de la Phase 2 + Document v2 + langage Veka (parser, printer, analyse, import `.vk`) + éditeur d'asset Veka View (Mode Simple, Code, Test, Timeline, viewport, undo, vignettes) + Symbol Editor / Library, import SVG, Veka Paint (mesh gradient) + bibliothèques de composants, styles partagés, tokens de contrôle, thème Alpha 13 migré + rendu (backdrop, glass, blend modes, distorsion) + Veka Hub + démo HUD in-game + stats `stat VekaUI` + langage : `let`, `time`, fonctions maths / `random` / `hsv` + `Canvas` (`Dot`, `Line`, `Glow`, `blend: .Add`) ; 106 tests automation ✅ |
| Prochaine étape | Retour de TheMiniBoss sur `VV_Fireworks` (Canvas `.vk`, 2026-11-27) ; puis TODO : perf du Canvas (réinterprété à chaque image), verre sombre de la galerie du Hub |
| Bloquant | licence LT Flode Neue à vérifier avant toute diffusion (police commerciale) ; clics répétés sur « +1 » à vérifier à la souris |

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

### 2026-11-27 — CLOUD + LOCAL → « Open as asset » propose de mettre à jour une copie périmée

- **Constat de TheMiniBoss** (capture) : « Unknown 'Fireworks' » dans `/Game/VekaExamples/VV_ExampleBirthdayCard`. Cette
  copie date du 22 (« Open as asset ») et appelait encore le composant C++ `Fireworks`, supprimé depuis ; « Open as
  asset » réutilisait la copie existante sans regarder l'exemple.
- **Branche** : `cpp/example-copy-update`. `Hub::IsExampleCopyOutdated` (document de la copie ≠ document de l'exemple,
  `Veka::Lang::StructurallyEqual`) et `Hub::UpdateExampleCopy` (via `Veka::Editor::ApplyDocument` : une étape
  annulable, les éditeurs ouverts se rafraîchissent, code de l'onglet Code = celui de l'exemple). « Open as asset » sur
  une copie périmée : dialogue Oui (mettre à jour) / Non (ouvrir la copie telle quelle) / Annuler. Jamais remplacé sans
  demander.
- **Validé** : build éditeur, 106 tests (105 + 1 avertissement attendu) ; `HubLearn` couvre copie fraîche, copie
  périmée repérée, mise à jour, code repris. La copie de TheMiniBoss n'est **pas** modifiée : à lui de répondre au
  dialogue (Hub > Examples > Birthday card > Open as asset > Oui, puis enregistrer).

### 2026-11-27 — CLOUD + LOCAL → `Canvas` en `.vk` : les feux d'artifice SwiftUI refaits sans C++

- **Demande de TheMiniBoss** (avec `ContentView.swift` et `Fireworks.swift`, Kavsoft : `TimelineView` + `Canvas`,
  `.plusLighter`, 6 gerbes × 180 étincelles, fusées à traînée, flash radial) : « refait entièrement le fireworks mais
  cette fois-ci uniquement en .vk, interdiction de C++ ; s'il manque des choses dans .vk tu as le droit de les ajouter ».
- **Branche** : `cpp/vk-canvas`. Le C++ ajouté est **générique** (langage + primitive de dessin) ; l'effet lui-même est
  100 % `.vk` (`Samples/Veka/Examples/VV_Fireworks.vk`, porté ligne à ligne depuis le Swift).
- **Langage** :
  - `Canvas(blend: .Add) { ... }` : zone de dessin ; dans son bloc, `width` / `height` = sa taille (locaux du parser).
    `Dot(x, y, radius, color, opacity)`, `Line(x1, y1, x2, y2, width, color, fromOpacity, opacity)` (couleur qui
    s'efface le long du trait), `Glow(x, y, radius, color, opacity)` (lumière radiale douce). `let`, `if`, `ForEach`
    marchent dedans. `Dot` / `Line` / `Glow` hors d'un `Canvas` : avertissement, rien dessiné.
  - `array.count`.
- **Runtime** : primitive `FVekaCanvas` (`Primitives/VekaCanvas.*`) + `FVekaCanvasPainter` ; l'interpréteur fournit
  `FInterpretedCanvasPainter`, qui réinterprète le sous-arbre **au paint** avec `time` = secondes depuis la création du
  store (`FVekaViewStore::StartSeconds`). Un `time` lu seulement dans un canvas ne démarre pas l'horloge de la vue :
  **aucune réévaluation**, seul le widget du canvas se repeint (état `NeedsAnimationFrame`, comme les symboles animés).
  Points = boîtes arrondies (256 brosses au quart de pixel), traits = `MakeLines` à couleurs par point, glow = 24
  anneaux pondérés (chute (1 − d/R)², sans marches visibles). Additif = `PreMultipliedAlpha`, couleur prémultipliée.
- **Correctif trouvé en test** : en additif, alpha 0 ⇒ Slate **jette** la boîte (seuls les traits passaient, 0 pixel
  allumé). Alpha = 1/255 (assombrit le dessous de moins de 0,4 %).
- **Éditeur** : palette, famille « Drawing » (rose) : Canvas (paintbrush), Dot, Line, Glow ; docs du catalogue.
- **Validé** : build éditeur, **106 tests** (105 + 1 avertissement attendu). Nouveau `VekaUI.Editor.CanvasFireworks` :
  l'exemple analyse propre, aucune réévaluation, le canvas demande des images, > 100 points peints, des pixels allumés ;
  perf « 3,2 ms paint / image, 925 points ». Image rendue : `Docs/Sync/Screens/vk-canvas-fireworks.png` (aussi écrite
  par le test dans `Saved/VekaUI/CanvasFireworks.png`).
- **Pas vu à l'écran en direct** : la fenêtre de l'éditeur n'a pas pu passer au premier plan (bureau au premier plan,
  accès refusé) ; le rendu est vérifié par le test (même chemin de paint Slate) et l'animation par l'état du canvas.
  À regarder : Hub > Examples > VV_Fireworks.

### 2026-11-27 — CLOUD + LOCAL → inspecteur, palette et liste des vues plus clairs (UX, suite)

- **Demande de TheMiniBoss** (avec une maquette d'inspecteur style Figma : sections titrées +/−, champs en grille
  libellé au-dessus, unité « PX » dans le champ, contrôles segmentés, pastilles) : « encore plus simple […] plus
  friendly et compréhensible, ajoute des explications (en tooltip) ; pareil pour la palette (icônes) et la Views ;
  pour tout, plus intuitif et plus beau ».
- **Branche** : `cpp/ux-inspector`, fusionnée dans `C++` (`965b074`).
- **Mots du catalogue** (`View/VekaCatalogDocs.cpp`, `Veka::Catalog::FindDoc` / `ArgumentLabel`) : pour chaque composant
  et modificateur un titre simple (« Vertical stack », « Size » pour `.frame`, « Content color » pour `.foreground`…),
  une icône (symbole de la bibliothèque intégrée), une famille et une phrase ; pour chaque argument un libellé, une
  phrase et une unité (px, pt, ×, °).
- **Look partagé** (`SimpleMode/VekaEditorLook.*`) : icône = petit `SVekaHost` avec un `Symbol`, couleur par famille
  (Layout bleu, Content blanc, Controls vert, Shapes orange, Logique violet, Vues du projet turquoise), `Section`,
  `Field` (libellé au-dessus + tooltip), `Pair`, `IconButton`, `WithUnit`, `Segmented`, `DescribeNode`.
- **Inspecteur** : en-tête (icône, titre, nom dans le code s'il diffère, phrase) + « Name in the Views list » ; section
  Settings / Parameters ; champs courts (nombres, booléens) deux par deux ; « fx » = écrire en code, « × » = revenir au
  défaut ; enums ≤ 4 valeurs en boutons segmentés (alignements avec icônes), sinon menu avec les noms d'affichage ;
  chaque modificateur = une section (titre, icône, tooltip, ↑ ↓ −) ; « Add a modifier » par famille avec icônes.
  If / Repeat / let / Slot : sections avec libellés clairs (« Show when », « Repeat for », « Item name »…).
- **Palette** : icône sur pastille de couleur, nom simple, description en dessous, tooltip ; recherche sur le nom,
  le nom du code et la description. **Views** : icône, nom (ou étiquette) + détail (texte affiché, symbole, « 3 views »,
  condition), tooltip = ce que c'est + comment le renommer / déplacer.
- **Validé** : build éditeur, 105 tests (104 + 1 avertissement attendu). Vu à l'écran : liste des vues, en-tête et
  sections de l'inspecteur sur un Text (`Docs/Sync/Screens/ux-inspector-views.jpg`, `ux-inspector-panel.jpg`).
  **Pas vu à l'écran** : un inspecteur riche (paires, boutons segmentés, unités) et la palette en popup — TheMiniBoss
  utilisait le PC (autres fenêtres au premier plan), arrêt du pilotage de l'écran pour ne pas cliquer chez lui.
- **Vérifié ensuite** (`ux-inspector-modifiers.jpg`, `ux-palette-popup.jpg`) : inspecteur d'une VStack (Spacing en px,
  Alignment en 3 boutons à icônes, Padding et Frosted glass en sections, paires), palette « + Add » (familles, pastilles,
  descriptions). **Deux correctifs** : (1) cliquer une ligne déjà sélectionnée ouvrait le renommage et, au commit, le
  nom affiché (« Vertical stack ») devenait une étiquette `//` — le commit sans changement (ou du nom du type) est
  ignoré, vide = retour au type ; l'étiquette posée par erreur sur `VV_ExampleGlassHud` a été annulée (asset non
  réenregistré, toujours celui du 22) ; (2) champs numériques vides : une vraie case avec « Auto » (au lieu de
  « default » sans cadre).

### 2026-11-27 — CLOUD + LOCAL → éditeur de vue simplifié (UX)

- **Demande de TheMiniBoss** : « refais tout l'UX pour simplifier l'expérience utilisateur et l'éditeur car ça fait un
  peu bordel » ; il a choisi « Fonce, propose » (pas de questions préalables).
- **Constat** (avant : `Docs/Sync/Screens/ux-editor-before.jpg`) : 6 zones dont une pile de 4 onglets à droite
  (Inspector, State, Test, Details), Palette en panneau permanent, deux barres d'outils (~15 boutons : Refresh, Reset
  state, Fit, Symbols, Hub ; écran, DPI, Light, Dark, thème, Select, Bounds, Padding), aperçu en 1080p où une carte est un
  point au milieu d'un écran noir.
- **Après** (`ux-editor-design.jpg`, `ux-editor-test.jpg`, branche `cpp/ux-editor`) :
  - **3 colonnes** (layout `Standalone_VekaViewEditor_Layout_v5`, les anciens layouts sont remplacés) : « Views »
    (l'outliner, avec un bouton **+ Add** = la palette en popup : recherche, un clic ajoute et ferme) | aperçu au-dessus
    de Code / Timeline | **un seul panneau** à droite.
  - **Panneau de droite qui suit le contexte** (`MakeSidePanel`, `SWidgetSwitcher`) : en test → panneau Test ; une
    sélection → inspecteur ; rien de sélectionné → « This view » (états, params, événements, slots, modèle) + « Asset
    settings » (le Details, replié). Les onglets State / Test / Details n'existent plus ; Palette reste dans Window.
  - **Une barre d'outils** : **Test / Stop** (bascule conception ↔ test, remplace le bouton Select de l'aperçu et le
    Play du panneau Test), Reset, Fit, Hub. Refresh supprimé (l'aperçu se rafraîchit seul), Symbols supprimé (Tools / Hub).
  - **Aperçu** : nouvel écran **Content** par défaut (taille naturelle de la vue + 48 px de marge, DPI 1, recalculé à
    chaque modification ; `FVekaPreviewSettings::bFitContent`, sauvé dans la config) ; le menu d'écran contient Content,
    les écrans de jeu et l'échelle UI (sous-menu) ; Bounds / Padding sous « Show ». Barre : écran, Light / Dark, thème, Show.
- **Tests** : 105 (104 verts + 1 avec l'avertissement attendu) ; les tests d'aperçu fixent leur écran (le défaut
  Content ne s'applique qu'avec les réglages persistés). Vérifié à l'œil : ajout par + Add (popup), sélection →
  inspecteur, Test → Stop + panneau Test, retour.
- **Piège** : avec un éditeur d'asset ouvert en onglet, `taskkill` ne ferme pas UE (aucune boîte) ; fermer l'onglet
  puis File > Exit de l'onglet du niveau.

### 2026-11-27 — CLOUD + LOCAL → tout en `.vk` : `let`, `time`, fonctions ; feux d'artifice sans C++ ; doc colorée

- **Demandes de TheMiniBoss** : (1) le code de la doc avec couleurs et indentation ; (2) « supprime le challenge des
  fireworks en C++ car à partir de maintenant tout doit être possible dans le .vk ; s'il manque des éléments il faut
  les ajouter ».
- **Branches** : `cpp/doc-code-colors` puis `cpp/vk-expressions` (partie de la première), fusionnées dans `C++`.
- **Doc du Hub** : les blocs de code passent par `Veka::Editor::TokenizeLine` (le même découpage que l'onglet Code) ;
  couleurs partagées (`Veka::Editor::TokenColor`, l'onglet Code les utilise aussi), fond sombre en Light comme en Dark,
  espaces = largeurs fixes (l'indentation reste), lignes longues dans un défilement horizontal (plus de « … »).
  Nouvelle carte du guide : « Math, random and time ».
- **Langage `.vk`** :
  - `let nom = valeur` (nœud `EVekaAstNodeKind::Let`) : visible des vues suivantes du bloc et de leurs enfants ; outliner,
    inspecteur (« Value »), complétion et `LocalsAt` le connaissent ; hors du bloc → « Unknown name ».
  - `time` (`EVekaExprKind::Time`) : secondes depuis l'apparition de la vue ; une timeline interne `__clock` (1 h en
    boucle) qui joue tant que le document lit `time` → vue réévaluée à chaque image.
  - Fonctions (`EVekaExprKind::Call`, table `Veka::Catalog::Functions()`) : `sin cos tan atan2 abs sqrt pow exp floor
    ceil round fract sign clamp lerp smoothstep random(seed [, bas, haut])`, `pi`, et couleurs `hsv(teinte, s, v [, a])`,
    `rgba(...)` à canaux calculés (constants → littéral comme avant). Mauvais nombre d'arguments → « Call it as … ».
    `random` = SplitMix64 des bits de la graine (même graine → même nombre, moyenne 0,5 vérifiée sur 1000 graines).
- **Feux d'artifice 100 % `.vk`** (`VV_ExampleBirthdayCard.vk`) : 4 gerbes qui renaissent à un nouvel endroit et une
  nouvelle couleur à chaque cycle (`floor(cycle)` dans la graine), éclair, 36 étincelles par gerbe (angle + `random`,
  portée en `sqrt`, ralentissement cubique, gravité `7·t²`, scintillement `sin`, fondu `smoothstep`, couleur `hsv` du
  blanc à la teinte). **Composant C++ `FVekaFireworks` et son test supprimés.** `.move` / `.blur` restent (langage).
- **Perf de la réévaluation complète** (la vue des feux : ~165 nœuds chaque image) : **6,6 → 2,0 ms** par image.
  - interpréteur : chemin numérique sans `FVekaValue` (`EvalNumber`, mêmes règles Int / Float), noms comparés en
    `FName` mis en cache (`VEKA_FNAME`) au lieu de 52 comparaisons de chaînes, ids `item`/`then`/`else` sans `Printf` ;
    `CallFunction` sans `ToString` ;
  - réconciliation : clés des enfants hachées (plus de chaîne + `FName` par élément), `SyncWidget` sauté si rien n'a
    changé, environnement en copy-on-write (copie = pointeur), et surtout `FVekaImplicitAnimator::UpdateNode` sans
    transaction ni animation en cours = simple copie (avant : parcours par réflexion de chaque champ, 1,8 ms).
  - Reste (TODO) : ne réévaluer que les nœuds qui lisent `time` ; repeindre seulement quand seul l'aspect change.
- **Tests** : `VekaUI.Lang.Expressions` (aller-retour, analyse, erreurs, valeurs des fonctions),
  `VekaUI.Editor.FireworksInVeka` (étincelles colorées sur la carte en pixels, coût logué, garde < 4 ms).
- **Validé** : build éditeur, **105 tests** (104 verts + 1 avec l'avertissement attendu ; ressorts 0,082 ms, budgets
  4f tenus). Vérifié dans l'éditeur : `Docs/Sync/Screens/vk-doc-code.jpg`, `vk-fireworks.jpg`.
- **Dépôt** : nouvelle adresse `TheMiniBoss/Veka-Unreal` (voir Environnement Local).
- **Ancien asset** : `/Game/VekaExamples/VV_ExampleBirthdayCard` (sauvegardé par TheMiniBoss le 22) appelle encore
  `Fireworks(...)` → « unknown component » ; le supprimer, puis « Open as asset » recrée la version `.vk`.

### 2026-11-27 — CLOUD + LOCAL → correctif : les effets GPU respectent le clip Slate (zoom de l'éditeur de vue)

- **Bug signalé par TheMiniBoss** : en zoomant dans l'éditeur de vue, « le widget sort de sa zone et se met par-dessus
  tout Unreal ». Le canevas clippe bien (`ClipToBounds`), mais les éléments custom de VekaUIRender (backdrop,
  frosted, glass, distorsion, blend modes) dessinaient au GPU sans tenir compte du clip Slate.
- **Correctif** (`cpp/fix-effects-clip`) : chaque effet note le clip Slate actif au paint
  (`Veka::Render::CurrentClip`, boîte englobante du scissor / des stencils) dans `FVekaBackdropParams::Clip`,
  `FVekaGlassParams::Clip`, `FVekaBlendParams::Clip` ; au rendu, la zone dessinée (viewport de la passe, région du
  blend) est coupée à ce clip. Les shaders lisent des positions absolues : rien d'autre à changer. Règle aussi les
  effets qui débordaient d'un `.clip` (galerie du Hub).
- **Test** : `VekaUI.Render.EffectsClip` (backdrop rouge de 200 px dans un cadre clippé de 100 px : rouge dedans, rien
  dehors). Vérifié à l'œil : exemple de la carte zoomé très fort, le flou remplit le viewport sans en sortir.

### 2026-11-22 — CLOUD + LOCAL → défi : carte d'anniversaire « feux d'artifice » (iOS 27) refaite en Veka

- **Demande de TheMiniBoss** : refaire avec Veka la vidéo Kavsoft « Apple's Birthday Card Fireworks Animation Using
  SwiftUI » (iOS 27 : une carte givrée « Birthday · Found in Contacts / Today, Mar 13 » où de petits feux d'artifice
  éclatent ; le bouton « Show Birthday Card » la fait apparaître). En SwiftUI : `Canvas` + `TimelineView`.
- **Branche** : `cpp/challenge-fireworks` (partie de `C++` @ `25521ce`), fusionnée dans `C++`. BuildPlugin non lancé.
- **Composant `Fireworks`** (`Primitives/VekaFireworks.*`), écrit avec l'API d'extension (`FVekaPrimitive`, comme Text
  ou Shape) — l'équivalent Veka de `Canvas` + `TimelineView` : son état (`FVekaPrimitiveState`) garde les gerbes et
  demande une image à chaque frame (`NeedsAnimationFrame`). Gerbes à des endroits et intervalles aléatoires (≈ `rate`
  par seconde), éclair blanc, étincelles en sphère projetée (plus denses au bord), blanches puis colorées (6 paires de
  couleurs), qui ralentissent, flottent, scintillent, tombent un peu (`gravity`) et s'éteignent ; halo + petite traînée.
  Arguments : `rate`, `particles`, `radius`, `size`, `duration`, `gravity`, `seed`. Schéma : `Fireworks(...)` en `.vk`
  sans autre câblage, palette > Effects. Ne prend pas les clics. Builder C++ `Veka::Fireworks(Rate, Particles)`.
  - Piège Slate : un `FSlateRoundedBoxBrush` dont le rayon dépasse la moitié de la boîte **ne dessine rien** (pas de
    bornage) → une brosse par quart de pixel de rayon (128 brosses statiques).
- **Exemple** `Samples/Veka/Examples/VV_ExampleBirthdayCard.vk` (Hub > Learn > Examples) : fond « plis violets »
  flouté (`backdropBlur`), carte `frostedGlass` + bordure avec le texte et `Fireworks` par-dessus, bouton `glass`
  « Show / Hide Birthday Card » (`withAnimation { shown.toggle() }`, la carte entre et sort par le haut).
- **Transitions `.vk`** : `.move` (opacité + arrivée par le haut) et `.blur` (flou + échelle 0,9 + opacité), aussi
  dans l'inspecteur du Mode Simple.
- **Tests** : `VekaUI.Effects.Fireworks` (schéma, pas de clic, image par frame ; pixels : rien au départ, étincelles
  colorées un instant plus tard) ; l'exemple passe `VekaUI.Editor.HubLearn` (analyse propre).
- **Validé** : build éditeur, **103 tests** (102 verts + 1 avec l'avertissement attendu). Vérifié dans l'éditeur de vue
  (`Docs/Sync/Screens/challenge-fireworks.jpg`) : gerbes or, cyan, vertes, roses sur la carte givrée ; Hide → la carte
  sort par le haut, Show → elle revient et les feux repartent. Teintes du fond assombries ensuite (non revérifiées à
  l'œil, l'exemple passe les tests).
- **Limite trouvée (avant ce défi)** : dans la galerie d'exemples du Hub, le verre de la carte apparaît sombre. Tous les
  aperçus sont dans **un seul hôte**, qui ne prend **qu'une capture par image** (au premier effet dessiné) : un effet
  d'un autre aperçu (GlassHud) sur une couche Slate plus basse la déclenche probablement avant que le fond de la carte
  soit dessiné. Correct dans l'éditeur de vue (un hôte par vue). Piste : une capture par « groupe » (modificateur qui
  isole un sous-arbre) ou une nouvelle capture quand un effet est sur une couche au-dessus de la précédente.

### 2026-11-22 — CLOUD + LOCAL → jalon 4f Perf + démo in-game (fin de la Phase 4)

- **Branche** : `cpp/phase-4f-perf` (partie de `C++` @ `f3f4ebf`), fusionnée dans `C++`. BuildPlugin non lancé.
- **Budgets §13, mesurés** (PC Local, UE 5.8.4, Development, 1920×1080) :

  | Budget | Mesure | Comment |
  |---|---|---|
  | écran de 500 nœuds au repos = 0 CPU Veka | **0** : pas de tick (l'hôte s'arrête au repos ; tick forcé 0,0001 ms) ; peinture **0,002 ms** si Slate met l'hôte en cache (`SInvalidationPanel` / invalidation globale) | test `VekaUI.Perf.Budgets` |
  | 100 ressorts < 0,1 ms Game Thread | **0,074–0,088 ms** en moyenne (pire image 0,12–0,15 ms), au départ 0,337 ms | test `VekaUI.Perf.Budgets` (assertion < 0,1 ms) |
  | glass plein écran < 1 ms GPU en High | **0,26 ms** (max 0,28) — `Veka.GlassDemo full`, qualité 3 = High par défaut | `stat gpu`, ligne « Veka Backdrop » |
  | (info) HUD de démo complet | GPU 0,17 ms ; Game Thread : Paint 0,54 ms, Runtime tick 0,02 ms | `stat gpu` + `stat VekaUI` |

  Limite connue : sans cache Slate, un hôte est repeint à chaque image par le viewport (500 Text ≈ 4,3 ms de
  prépasse + peinture). Le tick Veka reste à 0 ; c'est le coût Slate habituel d'un widget non invalidé. Un hôte
  avec du glass doit de toute façon repeindre (la scène derrière bouge).
- **Corrections du chemin d'animation** (ressorts 0,337 → ~0,08 ms) :
  - planificateur : callbacks `OnUpdate` / `OnComplete` partagés (`TSharedPtr<TFunction>`) au lieu d'une copie par
    image, appels en `TInlineAllocator<128>` ;
  - animations implicites : nœuds touchés dédupliqués (`TSet`) ; `RefreshEnvironment` seulement si un modificateur
    d'environnement change (`FVekaModifier::ModifiesEnvironment()`, vrai sur les 11 modificateurs concernés) ;
  - **modificateurs visuels** (`Background`, `Border`, `Shadow`, `Opacity`, `Clip`, `Offset`, `Scale`, `Rotation`) :
    `AffectsLayout()` = faux, une valeur animée ne refait plus la mise en page : `FVekaNode::SyncPresentation()`
    (transformation de rendu + repeinte) au lieu de `InvalidateLayout` + `SyncWidget`.
- **Stats** : `stat VekaUI` (`VekaUIStats.h`) — Runtime tick, Evaluate views, Animations, Paint, Hosts painted.
  GPU : `stat gpu`, « Veka Backdrop ».
- **Démo in-game** (`Samples/VekaDemoHUD.*`) : `Veka::Samples::LiquidGlassHud()` — état (glass), boussole (glass),
  mini-carte et objectif (frosted), munitions (glass), barre d'onglets en `GlassGroup` dont la sélection glisse
  en ressort et une goutte « + » qui se détache et se recolle (pilotée par un ticker toutes les 1,6 s).
  `AVekaDemoHUD` + `AVekaDemoGameMode` (GameMode Override d'un niveau, ou `?game=/Script/VekaUI.VekaDemoGameMode`),
  commande `Veka.HudDemo` (PIE / jeu). `Veka.GlassDemo full` : une vitre plein écran pour la mesure GPU.
- **Tests** : `VekaUI.Perf.Budgets` (nouveau) ; `VekaUI.Editor.HubLearn` vérifie l'ordre du changelog (4e avant
  4d) au lieu d'exiger « 4e » en tête (cassait à chaque nouveau jalon).
- **Validé** : build éditeur, **102 tests** (101 verts + 1 avec l'avertissement attendu). Vérifié en jeu
  (`-game`, 1080p) : `Docs/Sync/Screens/phase4f-hud.jpg` (HUD sur la scène 3D, sélection qui glisse, goutte qui
  se détache), `phase4f-glass-full.jpg`.
- **Démo jouable en Shipping** (lancée à la demande de TheMiniBoss) : `BuildCookRun` Win64 **Shipping**, carte
  `/Engine/Maps/Templates/OpenWorld`, archivé dans `VekaHost/Packaged/Windows` (936 Mo, hors dépôt) ; build 4 min 22 s,
  0 warning de compilation. Lancer **`Play Veka HUD Demo.bat`** (= `VekaHost.exe -VekaHudDemo`).
  - Piège : en Shipping, l'URL de carte de la ligne de commande est ignorée, donc `?game=…VekaDemoGameMode` aussi
    (le jeu démarrait sans HUD ; le même paquet en Development l'affichait). Ajout de l'option **`-VekaHudDemo`**
    (`FDelayedAutoRegisterHelper` + `PostLoadMapWithWorld`) : le HUD sur toute carte de jeu, sans game mode.
  - Les polices LT Flode Neue sont bien dans le paquet (`RuntimeDependencies` de `VekaUI.Build.cs`).
  - Vérifié : `Docs/Sync/Screens/phase4f-shipping.jpg` (HUD complet sur la scène, sélection et goutte animées).
  - L'éditeur doit être fermé pour packager (Live Coding bloque le build de la cible éditeur que lance UAT).
  - Licence LT Flode Neue toujours à vérifier avant de diffuser ce paquet.
- **Phase 4 terminée** : Liquid Glass in-game sur scène 3D ✅ (Shipping compris), Hub complet ✅, budgets §13 tenus ✅.

### 2026-11-22 — CLOUD + LOCAL → jalon 4e Hub : Exemples, Doc, Changelog (+ retours sur 4d)

- **Branche** : `cpp/phase-4e-hub-learn` (partie de `C++` @ `22600ab`), fusionnée dans `C++`. BuildPlugin non lancé.
- **Retours de TheMiniBoss sur 4d, corrigés** :
  - **pickers segmentés qui coupaient leurs libellés** (« Monochro/me », « Regula/r ») : libellés sur une ligne
    (`LineLimit(1)`) et largeur naturelle dans le Hub (plus de largeur fixe trop étroite).
  - **rangement** : chaque galerie est en **cartes titrées** dans une grille adaptative (`Card` / `Cards` dans
    `VekaPlayground.cpp`) — Controls (Buttons, Toggles, Sliders…, Pickers and menus, Text fields, Color, Labels…),
    Colora, Typography, Symbols, Anima, Morph ; les sections du Hub aussi (`HubCard`). Les couleurs des galeries
    suivent maintenant les tokens (fond, cartes, texte secondaire) : Light / Dark et thèmes s'y appliquent.
  - **blend modes « faux »** : la démo mettait `.blendMode` sur le **groupe** des deux cercles (le bleu se peignait
    normalement sur le rouge, puis le groupe se multipliait avec le fond). Le mode est maintenant sur **chaque
    cercle** (comme `mix-blend-mode` en CSS) : le chevauchement Multiply devient quasi noir (rouge × bleu). Testé en
    pixels : deux carrés Multiply superposés = fond × couleur 1 × couleur 2 (à 4/255).
  - libellés des pastilles de couleur et des symboles sur une ligne (réduits au besoin).
- **Exemples** (Learn > Examples) : `VV_Starter` + `Samples/Veka/Examples/` (`VV_ExampleSettings`,
  `VV_ExampleGlassHud`, `VV_ExamplePlayer`) ; aperçus **vivants** (document parsé, un store par exemple : les
  boutons et interrupteurs marchent dans l'aperçu), description = premier commentaire du fichier ; **« Open as
  asset »** crée la vue dans `/Game/VekaExamples` (non sauvegardée) et l'ouvre dans l'éditeur de vue.
- **Documentation** (Learn > Documentation) : **Guide** (première vue, états et actions, composants, effets,
  couleurs et thèmes, avec du code) et **référence générée depuis le catalogue** (Components, Modifiers : appel,
  arguments, nature — nombre, texte, peinture, état, valeurs d'enum —, conteneur / actions), avec recherche.
- **Changelog** (Learn > Changelog) : lu dans **`Docs/CHANGELOG.md`** (nouveau, en anglais, une entrée par jalon).
- **Tests** : `VekaUI.Editor.HubLearn` (exemples trouvés, tous propres à l'analyse, « Open as asset » crée la vue au
  bon endroit puis la retrouve, changelog lu) ; `VekaUI.Render.BlendModes` étendu (chevauchement).
- **Validé** : build éditeur, **101 tests** (100 verts + 1 avec l'avertissement attendu). Vérifié dans l'éditeur
  (`Docs/Sync/Screens/phase4e-*.jpg`) : cartes, pickers sur une ligne, Multiply sombre au chevauchement, quatre
  exemples (Play → Pause dans l'aperçu), guide, référence (20 composants), changelog.
- **Reste de la Phase 4** : 4f (démo in-game + budgets perf §13).

### 2026-11-22 — CLOUD + LOCAL → jalon 4d Hub + DesignLab

- **Branche** : `cpp/phase-4d-hub` (partie de `C++` @ `5e9e56f`), fusionnée dans `C++`. BuildPlugin non lancé.
- **Veka Hub** (`VekaEditor/Private/Hub/VekaHub.*`) : onglet nomade **écrit en VekaUI** (une `FVekaView` avec état),
  ouvert par **Window > Tools > Veka Hub**, un **bouton de la barre d'outils du Level Editor**
  (`LevelEditor.LevelEditorToolBar.User`) et un bouton **Hub** dans la barre de l'éditeur de vue.
  - Barre latérale (DesignLab : Components, Colors, Symbols, Typography, Animations, Effects, Morph ; Learn :
    Examples, Documentation, Changelog marqués « 4e »), en-tête **Light / Dark** et **sélecteur de thème** (Veka
    intégré + tous les `UVekaTheme` du projet, dont `VT_Alpha13`).
  - Réglages en direct : taille de contrôle + désactivé (Components) ; graisse + mode de rendu sur une grille de
    96 symboles (Symbols) ; taille + graisse d'un pangramme (Typography) ; **flou, réfraction, aberration, reflet,
    distorsion, blend mode** sur une scène colorée (Effects) ; grilles des tokens Colora et des tokens de contrôle
    résolus dans le thème et le schéma courants (Colors). Les galeries du playground sont exposées
    (`Veka::Samples::ControlsGallery()`, `ColoraGallery`, `SymbolsGallery`, `TypographyGallery`, `AnimaGallery`,
    `MorphGallery`).
- **Deux corrections de rendu trouvées en vérifiant le Hub** :
  - **la capture d'une couche est prise par le premier effet dessiné** (et non plus au début du host) : les effets
    voient le contenu du host peint avant eux (le cas courant : un panneau glass au-dessus de l'interface elle-même).
    Le host n'ajoute plus d'élément de capture.
  - **enfants qui se chevauchent** (`ZStack`, offsets) : chacun est peint au-dessus du précédent
    (`SVekaNodeWidget::PaintContent`) ; avant, tous partaient de la même couche et les couches profondes d'un enfant
    passaient sur l'enfant suivant (effets cachés, et blend en carré noir). Les piles sans chevauchement gardent des
    couches partagées (meilleur batching).
- **Tests** : `VekaUI.Editor.Hub` (onglet enregistré, les dix sections dessinent, les effets émettent leurs éléments
  GPU, `VT_Alpha13` proposé, symboles de la barre latérale existants) et `VekaUI.Render.OverlapLayers` ; décomptes
  d'éléments des tests de backdrop mis à jour (plus d'élément de capture séparé).
- **Validé** : build éditeur, **100 tests** (99 verts + 1 avec l'avertissement attendu). Vérifié dans l'éditeur
  (`Docs/Sync/Screens/phase4d-hub-components.jpg`, `phase4d-hub-effects.jpg`, `phase4d-hub-colors-alpha13-light.jpg`) :
  bouton de barre d'outils → Hub ; Effects : backdrop, glass, distorsion et Multiply corrects sur la scène, le
  curseur de réfraction agit en direct ; Colors en Light avec `VT_Alpha13` (Indigo #6155F5, Selection opaque…).
- **Limites** : dans une fenêtre flottante **maximisée**, la bande du haut est traitée comme barre de titre (les
  contrôles de l'en-tête n'y reçoivent pas les clics ; ancré ou non maximisé, ils marchent) ; sections Learn en 4e.

### 2026-11-22 — CLOUD + LOCAL → jalon 4c Blend modes + distorsion

- **Branche** : `cpp/phase-4c-blend` (partie de `C++` @ `a0abce0`), fusionnée dans `C++`. BuildPlugin non lancé
  (règle du 2026-11-22).
- **Blend modes contre le backdrop** : `EVekaBlendMode` gagne Multiply, Screen, Overlay, Darken, Lighten,
  ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Hue, Saturation, Color, Luminosity (formules
  W3C, `BlendPS`). Normal et Add restent dessinés directement (2a).
  - **Difficulté** : le back buffer est en **10:10:10:2** (`PF_A2B10G10R10`), son alpha est inutilisable pour retrouver
    la couverture du contenu. **Solution** : le contenu est **peint deux fois** — sur noir (N0 = a·C), puis sur blanc
    (N1 = a·C + 1 − a) — d'où `a = 1 − (N1 − N0)`, `C = N0 / a`, résultat `(1 − a)·B + a·Blend(B, C)`. Nouveau crochet
    de modificateur `PaintsContentTwice()` / `PaintBetween()` dans `SVekaNodeWidget::PaintLevel`.
  - Rendu : trois éléments par blend (`MakeBlendElements` : Begin = copie de B + remplissage noir, Between = copie de
    N0 + remplissage blanc, End = composite) ; zone = le rectangle du nœud ; à la qualité 0, dessin normal.
  - Langage : `.blendMode(.Multiply)` (catalogue, énumération complète dans l'inspecteur et la complétion).
- **Distorsion liquide** : `FVekaLiquidDistortionModifier` / `.liquidDistortion(strength, scale:, speed:,
  cornerRadius:)` — un backdrop sans flou dont le composite déplace l'échantillonnage par des vagues animées (deux
  couches de sinus croisés, atténuées au bord), dans la capture partagée de la couche.
- **Démos** : `Veka.BlendDemo` (une pastille par mode sur la scène) ; panneau « Distortion » ajouté à
  `Veka.BackdropDemo`.
- **Tests** : `VekaUI.Render.BlendModes` — éléments (3 par blend, aucun pour Normal / Add, 2 pour la distorsion) et
  **pixels réels** (rendu `FWidgetRenderer` dans une cible **linéaire**, relu) : Multiply, Screen, Darken, Lighten,
  Difference, Exclusion et Overlay égaux à la formule sur les valeurs stockées, à 3/255 près. (Avec une cible sRGB la
  relecture ré-encode les valeurs : piège noté.)
- **Validé** : build éditeur, **98 tests** (97 verts + 1 avec l'avertissement attendu). Vérifié en jeu
  (`Docs/Sync/Screens/phase4c-blend-game.jpg` : Multiply assombrit sur le ciel, Screen éclaircit, Difference / Hue /
  Saturation / Luminosity cohérents ; `phase4c-distortion-a/b.jpg` : le bord du nuage ondule d'une image à l'autre,
  effet discret à la force 14).
- **Limites** : le contenu d'un nœud en blend est peint deux fois (coût CPU / GPU double pour ce sous-arbre) ; ce qui
  dépasse du rectangle du nœud (ombres) n'est pas mélangé ; la distorsion ne redemande pas d'images d'elle-même (Slate
  repeint chaque image en jeu ; dans un éditeur au repos elle peut se figer).

### 2026-11-22 — CLOUD + LOCAL → jalon 4b Glass (Liquid Glass)

- **Branche** : `cpp/phase-4b-glass` (partie de `C++` @ `74d619e`), fusionnée dans `C++`.
- **Règle de validation (TheMiniBoss, 2026-11-22)** : plus de BuildPlugin Editor + Shipping à chaque jalon, sauf
  demande ou besoin ; un jalon est vert avec build éditeur + tests + vérification visuelle. **Non lancé pour 4b.**
- **Shader glass** (`VekaBackdrop.usf`, `GlassPS`) : champ de distance des formes (jusqu'à 8 rectangles arrondis,
  **minimum lisse** pour la fusion), normale par différences finies ; près du bord (le **bezel**) l'arrière-plan est
  lu plus loin le long de la normale (`Refraction · (1 − profondeur)²`) — le fond « coule » autour du bord comme dans
  un verre épais ; **aberration chromatique** (rouge, vert, bleu déplacés différemment) ; **reflet du bord**
  (`Highlight · rim³`, plus fort du côté de la lumière = **le pointeur**) ; flou léger, saturation, teinte.
- **Rendu** : `FVekaGlassParams` / `FVekaBackdropLayer::AddGlass` ; le glass partage la capture et la pyramide de la
  couche (marge de capture élargie pour la réfraction). Un `.glassGroup` dépose son élément avant ses enfants, puis
  ses enfants `.glass` lui donnent leurs formes pendant la même peinture (pile de groupes ; `PaintFront` referme).
- **Modificateurs** : `FVekaGlassModifier` (`.glass`, rayons + `FVekaGlassMaterial` : flou 3, réfraction 18, bezel
  18, aberration 0,35, reflet 0,6, teinte blanche 0,04, saturation 1,2) et `FVekaGlassGroupModifier` (`.glassGroup`,
  lissage 16). Builders `.Glass(radii[, material])`, `.GlassGroup(smoothness[, material])`. Repli à la qualité 0 :
  remplissage teinté léger (les enfants d'un groupe remplissent chacun).
- **Langage** : `.glass(cornerRadius, blur:, refraction:, bezel:, aberration:, highlight:, tint:, tintAmount:)` et
  `.glassGroup(smoothness, …)` (catalogue → analyse, complétion, inspecteur).
- **Démo** : **`Veka.GlassDemo [écart]`** (PIE ou jeu) — grand panneau glass + barre d'onglets et goutte « + » qui
  fusionnent.
- **Mesure** (jeu 1080p, `stat gpu`) : panneau 560 × 260 + groupe de deux formes = **0,11 ms** (capture comprise).
- **Tests** : `VekaUI.Render.Glass` (un glass = composite + capture ; groupe de deux = un seul glass ; glass +
  backdrop = une capture ; qualité 0 = remplissages ; `.glass(...)` / `.glassGroup(...)` depuis une vue).
- **Validé** : build éditeur, **97 tests** (96 verts + 1 avec l'avertissement attendu). Vérifié en jeu
  (`Docs/Sync/Screens/phase4b-glass-game.jpg`, `phase4b-glass-light-*.jpg`, `phase4b-stat-gpu.jpg`) : nuages
  courbés au bord, franges bleu / orange en bas, reflet qui suit le pointeur, pilule et goutte reliées par un col lisse.
- **Limites** : réaction au scroll pas encore faite (le pointeur oui) ; un widget mis en cache par l'invalidation
  Slate perd toujours son effet ; 8 formes au plus par groupe ; pas de reflet « spéculaire » d'environnement (seulement
  le bord).

### 2026-11-22 — CLOUD + LOCAL → jalon 4a Backdrop + flou

- **Branche** : `cpp/phase-4a-backdrop` (partie de `C++` @ `44235f9`), fusionnée dans `C++`.
- **Une capture par couche** (`VekaUIRender/Public/VekaBackdrop.h`) : `FVekaBackdropLayer` réunit tous les backdrops
  d'un `SVekaHost` pendant une frame. Le host peint son arbre une couche plus haut et, si des backdrops se sont
  inscrits, pose **un seul élément de capture** dans la couche libérée : copie de l'union des zones (avec marge), une
  pyramide dual Kawase jusqu'au niveau le plus haut demandé, niveaux convertis en textures externes (poolées). Chaque
  backdrop dessine ensuite son **composite** en mélangeant les **deux niveaux** autour de son rayon (`BlurLevel(r) =
  log2(1 + r / 1,5)`) : n'importe quel rayon, continu (animable sans saut). Hors host : capture individuelle.
- **Qualité** : `r.Veka.Backdrop.Quality` (scalability) 0 off, 1 low (niveaux échantillonnés tels quels, ≤ 4), 2
  medium (≤ 5), 3 high (défaut, ≤ 6), 4 epic (≤ 7). **Repli** à 0 : remplissage arrondi teinté (opacité ≥ 0,85), aucune
  capture. Aucune capture non plus si aucun backdrop n'est peint.
- **Modificateur** `FVekaBackdropModifier` : `Radius`, `CornerRadii`, `Tint` (**peinture Colora**, défaut
  `.WindowBackground`), `TintAmount`, `Saturation`, `Noise` (grain). Préréglage `FrostedGlass()` (flou 30, teinte 0,45,
  grain 0,035). Builders `.BackdropBlur(radius, radii[, tint, amount])` et `.FrostedGlass(radii[, tint, blur, noise])`.
- **Langage** : `.backdropBlur(radius, cornerRadius:, tint:, tintAmount:, saturation:)` et
  `.frostedGlass(blur:, cornerRadius:, tint:, noise:)` (catalogue → analyse, complétion, inspecteur typé, valeurs par
  défaut à l'ajout).
- **Retiré** : la piste A du spike (post-buffers moteur) et la dépendance `SlateRHIRenderer`. Démo renommée
  **`Veka.BackdropDemo [full]`** (trois rayons + une barre frostedGlass, une seule capture).
- **Mesures** (jeu 1920 × 1080, `stat gpu`) : 4 backdrops (rayons 6, 24, 60, frosted 30) = **0,12 ms** pour tout
  (une capture) ; plein écran (4.0) 0,20–0,22 ms.
- **Tests** : `VekaUI.Render.Backdrop` (niveaux continus, plafonds par qualité, une capture pour trois backdrops,
  aucune sans backdrop, repli à la qualité 0, préréglage frosted) et `VekaUI.Render.BackdropInViews`
  (`.frostedGlass(...)` d'une vue → modificateur avec flou, rayon, teinte jeton, grain).
- **Validé** : build éditeur, **96 tests** (95 verts + 1 avec l'avertissement attendu), BuildPlugin non-unity
  Editor + Shipping, 0 warning.
- **Vérifié** (`Docs/Sync/Screens/phase4a-demo-game.jpg`, `phase4a-editor-preview.jpg`) : en jeu, trois panneaux aux
  flous nettement différents + barre givrée sur le ciel ; dans l'éditeur de vue, `.frostedGlass(tint: .Accent)` tapé
  dans le code de `VV_Shop` dessine le carré arrondi teinté pile derrière le symbole, au zoom du viewport, et
  l'inspecteur affiche flou / rayon / teinte / grain. Démo retirée (`VV_Shop` non sauvegardé).
- **Limites** : un widget mis en cache par l'invalidation Slate (retainer, invalidation panel) rejoue ses éléments
  sans repeindre : son backdrop disparaît (à traiter avec le glass, 4b) ; les backdrops d'un même host ne se voient
  pas entre eux (même capture, comme prévu) ; la valeur du rayon n'est pas encore animée par `.animation` (le rendu le
  permet).

### 2026-11-22 — CLOUD + LOCAL → jalon 4.0 Spike backdrop

- **Branche** : `cpp/phase-4.0-backdrop` (partie de `C++` @ `fafd658`), fusionnée dans `C++`. Rapport complet :
  [`Docs/Spikes/4.0-backdrop.md`](Docs/Spikes/4.0-backdrop.md).
- **Construit** : modificateur `backdropBlur` (`FVekaBackdropModifier`, `Modifiers/VekaEffectModifiers.h`) avec les
  deux méthodes, et la commande **`Veka.BackdropSpike [rayon] [full]`** (PIE ou `-game`) qui les montre côte à côte
  sur la scène 3D.
  - **A** — post-buffers Slate du moteur (`SPostBufferUpdate` + processeur gaussien, render target échantillonnée).
  - **B** — élément Slate custom + render graph (`VekaUIRender`, `VekaBackdrop.h` / `VekaBackdrop.usf`) : copie de la
    zone, pyramide **dual Kawase**, composite dans le **SDF du rectangle arrondi** (teinte, saturation) ; stat GPU
    `Veka Backdrop`.
- **Résultats** : A ne capture rien dans l'éditeur (gris uni) et ne floute pas en jeu (copie nette) ; B floute
  partout. **B coûte 0,09 ms GPU** pour un panneau 380 × 240 et **0,20–0,22 ms en plein écran 1080p** (rayon 24) —
  budget §13 du glass : < 1 ms.
- **Décision : B.** Pièges corrigés : Slate ne garde qu'un **pointeur faible** vers un élément custom (on le garde
  vivant jusqu'au rendu, libéré par une commande de rendu) ; la matrice des éléments couvre **toute la texture de
  sortie** ; `RDG_EVENT_SCOPE_STAT` remplace `RDG_GPU_STAT_SCOPE` (5.8).
- **Test** : `VekaUI.Render.Backdrop` (niveaux de flou, un élément custom vivant par peinture).
- **Validé** : build éditeur, **95 tests** (94 verts + 1 avec l'avertissement attendu), BuildPlugin non-unity
  Editor + Shipping, 0 warning ; vérifié en PIE et en jeu (`Docs/Sync/Screens/phase4-0-*.jpg`).
- **Suite (4a)** : une capture + pyramide **par couche et par frame** partagée, niveaux de qualité, repli sans
  backdrop, `backdropBlur` / `frostedGlass` dans le langage ; retrait de la piste A et de la dépendance
  `SlateRHIRenderer`.

### 2026-11-22 — CLOUD → plan de la Phase 4 (proposition)

Phase 4 de `VEKAUI_PHASE0.md` §16.2 (Rendu + Hub), découpée comme les Phases 2 et 3 : **une branche par jalon**
(`cpp/phase-4x-…`), fusionnée dans `C++` quand elle est verte (build Editor + Shipping, tests, vérif dans l'éditeur).
Critère final (§16.2) : **Liquid Glass in-game sur une scène 3D**, **Hub complet**, **budgets perf §13 tenus**
(écran de 500 nœuds au repos = 0 coût CPU Veka ; 100 ressorts < 0,1 ms Game Thread ; glass plein écran < 1 ms GPU en
« High »).

État de départ : le rendu Veka est **tessellé sur CPU** (`FSlateDrawElement::MakeCustomVerts` + brush blanc,
`Colora/VekaPaintRenderer.cpp`) ; `VekaUIRender` ne fait que déclarer le dossier `Shaders/` ; blend modes Normal et
Add seulement.

| Jalon | Contenu | Dépend de | Critère « vert » |
|---|---|---|---|
| **4.0 Spike backdrop** | §12.1 : **A** post-buffers Slate du moteur (5.8 : `ESlatePostRT` 0–4, `USlatePostBufferProcessor`, `SPostBufferUpdate`, rendu dans un `UTextureRenderTarget2D`) contre **B** élément Slate custom (`ICustomSlateElement`) + RDG (copie de la zone, pyramide dual Kawase, composite) ; mesures GPU, comportement éditeur / PIE / jeu packagé, DPI, plusieurs fenêtres ; décision écrite | 3g | les deux pistes affichent un flou derrière un panneau Veka sur une scène 3D ; coût mesuré ; choix motivé |
| **4a Backdrop + flou** | voie choisie industrialisée : **une capture + une pyramide de flou par frame et par couche**, partagée par tous les widgets de la couche ; `.backdropBlur(radius)`, `.frostedGlass(tint, blur, noise)` ; scalabilité (Low / Medium / High / Epic) et repli sans backdrop (teinte unie) ; animable | 4.0 | panneau givré sur la scène 3D en éditeur et en jeu ; 0 capture si aucun widget n'en a besoin |
| **4b Glass** | `.glass(...)` façon Liquid Glass : **réfraction depuis la normale du SDF** de la forme (nos SDF de 2a / 2f), reflet spéculaire du bord, aberration chromatique, teinte ; **glass fusionné / goutte** via `ShapeGroup` ; réaction au pointeur / au scroll | 4a | une barre d'onglets glass qui se déforme et fusionne ; < 1 ms GPU plein écran en High |
| **4c Blend modes + distorsion** | Multiply, Screen, Overlay, SoftLight, Difference, Color… (avec le backdrop) ; `.liquidDistortion(time, strength)` ; `.blendMode` dans le langage, l'inspecteur et la complétion | 4a | chaque blend mode comparé à une référence ; distorsion animée |
| **4d Hub + DesignLab** | onglet nomade **Veka Hub** + bouton de toolbar, **écrit en VekaUI** ; DesignLab : tous les composants, palettes Colora (thèmes, dont `VT_Alpha13`), symboles, typographie, animations, effets, avec réglages en direct | 4b | le Hub s'ouvre ; chaque composant et effet visible et réglable |
| **4e Hub : Exemples, Doc, Changelog** | exemples ouvrables (crée l'asset), **doc générée en partie depuis le schéma** (composants, modificateurs, arguments) + pages écrites, changelog ; anglais d'abord, langue suivant l'éditeur | 4d | la doc couvre tout le catalogue ; un exemple devient un asset modifiable |
| **4f Perf + démo in-game** | niveau de démo (HUD Liquid Glass sur scène 3D), mesures des budgets §13 (Unreal Insights + `stat` dédiés), corrections | 4b, 4c | budgets tenus et notés ici ; démo jouable en Shipping |

Ordre conseillé : **4.0 → 4a → 4b → 4c → 4d → 4e → 4f** (4d peut commencer après 4a si on veut voir le Hub plus tôt).

### 2026-11-22 — CLOUD + LOCAL → jalon 3g Composants et migration

- **Branche** : `cpp/phase-3g-components` (partie de `C++` @ `55bd75c`), fusionnée dans `C++`.
- **Composants en asset** (le langage les gérait depuis 3a) :
  - **Analyse** : arguments inconnus (`VV_Badge has no parameter 'size' (title, count, tint)`), type d'un littéral
    contre le type du `@Param`, paramètre sans défaut oublié (avertissement), contenu pour un composant sans
    `@Slot`, `.inSlot(x)` vers un slot inconnu.
  - **Complétion** : les `@Param` d'une vue du projet (`title:`, avec type et défaut), sans ceux déjà donnés ; les
    peintures pour un paramètre `Color` ; les styles après `.style(.`.
  - **Palette** : composants groupés par bibliothèque (puis « Views of the project »), infobulle « paramètres ·
    slots » ; se met à jour toute seule (asset registry : vue ajoutée / supprimée / renommée, vue ou bibliothèque
    modifiée).
  - **Inspecteur** : sélecteur de style (`.style`) ; les paramètres et slots des composants existaient déjà (3d).
- **`UVekaComponentLibrary`** (`View/VekaComponentLibrary.h`, runtime) : `DisplayName`, `Components`, `Styles`.
  Content Browser > Add > User Interface > Veka Component Library (part avec un style `Card`).
  - **Styles partagés** `FVekaSharedStyle` : des modificateurs écrits en Veka (`.padding(16)` /
    `.background(.WindowBackground, radius: 16)`), compilés par l'éditeur (`Veka::Components::StyleCompiler`, car
    VekaLang est editor-only) dans `Compiled` (sérialisé, donc utilisable en jeu packagé), erreurs dans
    `CompileErrors` avec la ligne du style. Une vue les applique avec **`.style(.Card)`** (modificateur `style`,
    argument de type `Style`) ; un style peut en utiliser un autre (profondeur 8) ; style inconnu = avertissement.
- **Tokens de contrôle** (`Colora/VekaTheme.h`) : `ThemeAccent`, `ThemeBackground`, `ThemeText`,
  `ButtonBackground`, `ToggleAccent/Background`, `SliderAccent/Background/Knob/Text`, `ProgressAccent/Background`,
  `GaugeAccent/Background`, `MenuAccent/Background`, `PickerAccent/Background`. Ce sont des **alias** (ToggleAccent →
  ThemeAccent → Tint) tant qu'un thème ne les définit pas : sans thème, rien ne change visuellement. Les contrôles
  2e les utilisent ; le slider lit aussi les métriques `Slider.Height` / `Slider.KnobHeight`. Noms **sans point**
  (`.Theme.Background` serait lu comme un accès membre en Veka). `IsKnownToken` accepte aussi les tokens des thèmes
  chargés (plus d'erreur « not a Colora token » pour un token défini par un thème).
- **Migration des thèmes Alpha 13** (`Migration/VekaAlpha13Migration.*`) :
  - la branche `Blueprint` est lue **en lecture seule** : `git archive origin/Blueprint Content Veka.uplugin` vers
    `VekaHost/Saved/VekaAlpha13/` (hors dépôt), monté en `/Veka/` ; les structs Blueprint sont lus par réflexion
    (noms de champs sans suffixe GUID).
  - commande **`Veka.MigrateAlpha13Themes [dossier] [quit]`** → **`/VekaUI/Themes/VT_Alpha13`** (dans le plugin,
    commité) ; `Veka.DumpAlpha13` affiche les valeurs brutes.
  - résultat : les **30 presets Colora** de `Colora_System` avec leurs valeurs exactes clair / sombre (Accent
    #007AFF / #0A84FF, Indigo #6155F5, Primary…Quaternary, WindowBackground blanc / noir…) ; `ThemeBackground =
    .Background` (Veka_Default) ; `Slider.Width/Height = 256 × 8`, `Slider.KnobWidth/Height = 32 × 16`. Les autres
    emplacements d'Alpha 13 étaient restés à leurs valeurs par défaut (non surchargés) : ils gardent le défaut du
    token de contrôle. `Veka_Default` et `Veka_LightDefault` sont identiques dans Alpha 13 (le clair / sombre vient
    des presets).
  - non migrés (pas d'emplacement de thème dans VekaUI, listés dans le rapport) : glyphes, badges, police
    (LT Flode Neue est déjà la police par défaut), formes (`E_VekaShapeMode`).
  - Lecture des réglages Colora : `ColoraMode` Color / Gradient / Mix, preset + `HierachicalPreset`, couleur
    manuelle seulement si son « override » est coché ; un réglage resté à la valeur par défaut du struct = non défini.
- **Tests** (`VekaComponentTests.cpp`, 3 nouveaux) : `Components.Params` (analyse, complétion, palette),
  `Components.Library` (factory, compilation et erreurs de style, `.style` appliqué = +32 px, avertissement,
  groupe de palette), `Components.Alpha13Theme` (conversion des noms, alias sans thème, valeurs de `VT_Alpha13`,
  métriques).
- **Validé** : build éditeur, **94 tests** (93 verts + 1 avec l'avertissement attendu), BuildPlugin non-unity
  Editor + Shipping, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3g-palette-library.jpg`, `phase3g-alpha13-dark.jpg`,
  `phase3g-alpha13-light.jpg`) : bibliothèque `VL_Demo` créée par le menu Add, `VV_ShopItem` ajouté → la palette de
  `VV_Shop` le range aussitôt sous « Demo Kit » avec l'infobulle des paramètres et du slot ; `.style(.Card)` tapé
  dans le code encadre le symbole ; le sélecteur de thème du viewport applique `VT_Alpha13` (fonds noir / blanc
  purs d'Alpha 13 au lieu de #1C1C1E / #F2F2F7). Démo retirée ensuite (`VV_Shop` non sauvegardé, `VL_Demo`
  supprimé).
- **Limites** : les modificateurs n'ont pas de position source (une erreur d'analyse dans un style est donnée sans
  ligne) ; les dégradés Alpha 13 « Matrix » deviendraient linéaires (aucun n'est utilisé par les thèmes) ; les
  positions de stops Alpha 13 ne sont pas reprises (stops répartis).

### 2026-11-22 — LOCAL → correctif 3f : plantage en dessinant un symbole `.fill` modifié

- **Branche** : `cpp/fix-symbol-knockouts` (partie de `C++` @ `1af47aa`), fusionnée dans `C++`.
- **Symptôme** (signalé par TheMiniBoss) : `Array index out of bounds: 1 into an array of size 1` dans
  `FSymbolPainter::Draw` (`VekaSymbolView.cpp:432`), en peignant un symbole.
- **Cause** : `FVekaSymbolState::Knockouts` (formes dessinées dans un contour rempli, variante `.fill`) était mis en
  cache par définition **sans** la révision : un symbole qui gagne une couche (Details, annulation, réimport, éditeur de
  symbole) gardait l'ancien tableau, plus court.
- **Correctif** : le cache retient la révision `Veka::Symbols::GetRevision()` et le nombre de couches, et se recalcule
  sinon ; la lecture vérifie l'index par sécurité.
- **Test** : `VekaUI.Editor.Symbols.Editor` dessine `<logo>.fill`, ajoute deux couches, redessine, puis n'en garde
  qu'une. Sans le correctif, le test reproduit l'assertion (`index 4 into an array of size 4`) ; avec, il passe.
- **Validé** : build éditeur, **91 tests** (90 verts + 1 avec l'avertissement attendu), BuildPlugin non-unity
  Editor + Shipping, 0 warning.

### 2026-11-22 — CLOUD + LOCAL → jalon 3f Symboles et peintures

- **Branche** : `cpp/phase-3f-symbols` (partie de `C++` @ `e19eb3f`), fusionnée dans `C++`.
- **Runtime** :
  - `UVekaSymbol` autonome : s'enregistre au chargement (`Veka::Symbols::RegisterSymbol`, nom = `Definition.Name` ou
    nom d'asset) et se trouve par son nom comme un symbole intégré (`Symbol(#"VekaRocket")`, variantes `.fill`,
    `.circle`...) ; `NotifyChanged` vide le cache de géométrie et incrémente une **révision** incluse dans la clé du cache
    de tessellation des symboles (une édition est redessinée) ; `GetAllNames`.
  - **`UVekaPaintAsset`** (« Veka Paint ») : une `FVekaPaint` nommée (mesh gradient, dégradé, image...) utilisable là où
    va un jeton Colora : `.background(.Aurora)` (interpréteur, analyse « not a Colora token nor a paint asset »,
    complétion, menu de couleurs de l'inspecteur avec la couleur moyenne).
- **Import SVG** (`Veka::Editor::Svg::ImportSymbol`, factory « Veka Symbol (from SVG) ») : path / rect / circle /
  ellipse / line / polyline / polygon, groupes (style et transform translate / scale / matrix hérités ; rotation /
  skew ignorés avec warning), ajusté dans la grille 24 × 24 (viewBox centrée) ; élément rempli → couche Fill, trait seul
  → couche Stroke (épaisseur → `StrokeScale` relatif au poids Regular : suit le poids) ; rôles `data-role` / classe ou
  opacité ; couches variables `data-variable` ; couleur explicite gardée pour Multicolor ; text / image / use ignorés
  avec warning ; commentaires et DOCTYPE retirés avant le parseur XML. Exemple : `Samples/Svg/VekaRocket.svg`.
- **Symbol Editor** (double-clic sur une Veka Symbol) : canevas sur la **grille keyline** (unités, zone vive 3..21,
  cercle, axes) au poids choisi, couches teintées par rôle (sélection en orange) ; liste des couches (réordonner,
  supprimer) ; propriétés (type, rôle, couche variable → `VariableSteps`, fillable, stroke scale, couleur Multicolor) ;
  **aperçus** : 9 graisses, 4 modes de rendu, 3 échelles, variantes, valeur variable. Chaque édition = un undo.
  Création : Add > User Interface > Veka Symbol.
- **Éditeur de peinture** (double-clic sur une Veka Paint ; Add > Veka Paint démarre en mesh 3 × 3) : le mesh gradient
  dessiné en direct, **points déplacés à la souris** sur le treillis (aperçu pendant le glisser, écriture au relâcher),
  double-clic ou pastille = sélecteur de couleur, jeton Colora par point, x / y numériques, + / - colonne et ligne
  (rééchantillonné : rien ne saute), lisse, espace de couleur, grille régulière ; autres types dans Details (ou
  convertis en mesh).
- **Symbol Library** (Window > Tools > Veka Symbol Library, bouton **Symbols** dans l'éditeur de vue) : recherche (nom,
  mot-clé, catégorie), catégories, tuiles avec aperçu, panneau de détail (graisses, modes, échelles, variantes,
  couches variables), Copy name (`#"nom"`).
- **Sélecteurs** : le sélecteur de symboles de l'inspecteur et la complétion listent aussi les symboles du projet ; les
  peintures du projet dans le menu de couleurs et la complétion. Vignettes des symboles et des peintures. L'éditeur
  charge toutes les Veka Symbol / Veka Paint au démarrage ; l'éditeur de vue se rafraîchit quand l'une change.
- **Tests** : 91 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Editor.Symbols.{Import, Editor}`,
  `VekaUI.Editor.Paint.Mesh` : import (5 couches, Fill / Stroke, couleur gardée, ajustement à la grille, rôle, stroke
  scale, tertiaire par opacité, couches variables, warning `<text>`, non-SVG refusé) ; **un SVG importé en
  `UVekaSymbol` utilisé dans une vue** (trouvé par nom, variantes, mise en page, vignette) ; éditions de couches avec
  undo, géométrie recalculée, suppression, recherche de la bibliothèque, ouverture du Symbol Editor ; peinture (mesh
  3 × 3, trouvée par nom, valide dans `.fill`, une vue la dessine en mesh, rééchantillonnage 4 × 3 gardant les coins,
  déplacement borné, point sous la souris, **glisser = aperçu puis écriture au relâcher**, undo, 4 × 4, vignette,
  ouverture de l'éditeur).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3f-symbol-editor.jpg`, `phase3f-mesh-editor.jpg`,
  `phase3f-rocket-card.jpg`, `phase3f-library.jpg`) : `VekaRocket.svg` importé (vignette = la fusée), Symbol Editor
  (grille, couches par rôle, aperçus graisses / modes / échelles / variantes) ; Veka Paint `Aurora` créée, point
  central déplacé à la souris (0,74 ; 0,29) ; vue `VV_RocketCard` en code : `Symbol(#"VekaRocket")` sur
  `.background(.Aurora, radius: 28)` rendue ; bibliothèque (catégorie Imported, détail de la fusée). Assets de
  démonstration non sauvegardés. Correction : le canevas du Symbol Editor se met à l'échelle (il masquait le curseur
  de poids dans une petite fenêtre).
- **Limites** : pas d'édition de tracés point par point dans le Symbol Editor (les formes viennent du SVG ou du code) ;
  pas de poignées de Bézier dans le mesh gradient (positions et couleurs seulement) ; en jeu, un symbole / une
  peinture doit être chargé (référencé ou via une bibliothèque) pour être trouvé par nom.

### 2026-11-21 — CLOUD + LOCAL → jalon 3e Mode Test + Timeline

- **Branche** : `cpp/phase-3e-test` (partie de `C++` @ `583e598`), fusionnée dans `C++`.
- **Timelines dans le document** (`FVekaDocumentTimeline` : nom, `FVekaTimelineData` — pistes, clés, longueur,
  boucle, vitesse —, **cues** `FVekaTimelineCue` = actions à un instant, `bAutoPlay`) ; expression `Timeline`
  (`Intro.fade` lit la piste maintenant) ; actions `Play` / `Pause` / `Stop` / `Reverse` / `Seek`. `Validate` vérifie
  timelines lues / pilotées et actions des cues.
- **Langage** :
  ```
  @Timeline Intro(length: 1.0, autoplay: true) {
      track fade { 0.0: 0.0, 0.4: 1.0 .EaseOut }
      at 0.5 {
          done = true
      }
  }
  ...  .opacity(Intro.fade)   Button("Replay") { Intro.stop()  Intro.play()  Pulse.seek(0.25) }
  ```
  Parser (réglages, clés avec courbe optionnelle, nombres négatifs, cues), printer canonique (flottants au plus court
  en simple précision), comparaison structurelle, analyse (« Timeline X has no track 'y' »), complétion (`@Timeline`,
  pistes et contrôles après `Intro.`), coloration (`track`).
- **Interpréteur** : chaque timeline est un joueur gardé par le nœud de la vue (`Ctx.Timeline`) ; les cues deviennent
  des événements qui exécutent leurs actions ; autoplay une fois par store ; le store expose `Timelines` (l'onglet
  Timeline les pilote) et `OnValueChanged` (journal du Mode Test). Composants aussi.
- **Mode Test** (onglet **Test**) : **Play** (les clics, survol et focus vont à la vue, sans PIE), états **en direct**
  (lecture et écriture), **journal** horodaté des `emit` et des changements d'état (Clear log, Reset state), **modèle
  mock** éditable (Details de l'instance de prévisualisation, non sauvegardée).
- **Onglet Timeline** (à côté de Code) : choisir / ajouter / renommer (partout) / retirer une timeline, longueur,
  vitesse, boucle, autoplay ; transport Play / Pause / Stop / Reverse avec temps courant ; pistes (ajouter, renommer —
  les lectures `Intro.piste` suivent —, retirer) ; **voies** : règle (clic / glisser = **scrub**), courbe de chaque
  piste, clés (double-clic = ajouter à la valeur courante, glisser = déplacer, Suppr), marqueurs de cues, tête de
  lecture rouge ; éditeur de la clé sélectionnée (temps, valeur, courbe). Temps arrondis au centième. Chaque édition
  = un undo, la tête de lecture est gardée.
- **Disposition** v4 : Outliner + Palette | Viewport + (Code, Timeline) | Inspector / State / **Test** / Details.
- **Tests** : 88 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Lang.Timeline` (parse, valeurs,
  courbes, cues, contrôles, impression identique au source, aller-retour, 4 erreurs, piste inconnue),
  `VekaUI.View.Timeline` (autoplay, tête à 0,3 s, piste entre ses clés, cue exécutée à 0,5 s, bouton qui relance et
  `seek`), **`VekaUI.Editor.Test.Shop`** (la boutique en Play avec un modèle mock à 30 or : affichage, Buy de la potion
  journalisé `emit Buy(25)` + `lastBought = …`, lanterne trop chère muette, journal vidé), **`VekaUI.Editor.Timeline.Panel`**
  (une animation faite à l'onglet Timeline : timeline, piste, clés ajoutée / déplacée / modifiée / supprimée, lue par
  `.opacity(Intro.value)`, scrub à 0,25 s → valeur 0,2, tête gardée après édition, undo).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3e-timeline.jpg`, `phase3e-test-log.jpg`, `phase3e-shop-play.jpg`) :
  vue neuve `VV_TimelineDemo` (non sauvegardée) — timeline et piste ajoutées, clé double-cliquée à 0,5 s, valeur 1,
  `.opacity(Intro.value)` en code (le curseur sélectionne le Text), Play (tête 0,40 → 1,00 s), scrub à 0,20 s (texte à
  moitié transparent) ; Test : deux taps → `count = 1`, `count = 2` ; **`VV_Shop` en Play avec l'or mock à 30** :
  « Aria 30 gold », Oak shield et Lantern désactivés, Buy de la potion → `lastBought = "Health potion"`,
  `emit Buy(25)`, Lantern muette. `VV_Shop` non modifié (le modèle mock est une instance de prévisualisation).
- **Limites** : les actions des cues s'écrivent en code (l'onglet Timeline les montre en marqueurs) ; pistes de nombres
  uniquement (les couleurs / vecteurs passent par des expressions sur plusieurs pistes) ; pas d'asset `UVekaTimeline`
  partagé dans le langage (timelines intégrées à la vue).

### 2026-11-21 — CLOUD + LOCAL → jalon 3d Mode Simple

- **Branche** : `cpp/phase-3d-simple` (partie de `C++` @ `f58dc48`), fusionnée dans `C++`.
- **Éditions du document** (`Veka::Editor::Doc`, `SimpleMode/VekaDocumentEdit`) : insérer (valeurs par défaut utiles :
  un Toggle se lie au premier état Bool…), supprimer, déplacer (jamais dans soi-même), dupliquer, envelopper dans une
  pile, libellé (= premier commentaire du nœud, `// Libellé` en code), arguments, pile de modificateurs (ajout avec
  défauts, retrait, réordonnancement), actions, déclarations (états, params, événements, slots ; **renommage partout** :
  lectures, cibles d'actions, `emit`, nœuds `Slot`), `Compact` (plus de nœuds / expressions orphelins). Chemins de nœuds
  (`Path` / `FromPath`) : la sélection survit aux éditions et aux re-parses. Tout passe par
  `FVekaViewEditorContext::Edit` → une transaction (`ApplyDocument`), refusé si le code a des erreurs.
- **VekaLang** : `ParseExpression` (une expression dans la portée du document + variables locales, rien de modifié en
  cas d'erreur), `ParseValue`, `PrintExpression`, `PrintValue` ; un diagnostic sans position n'affiche plus « 0:0 ».
- **Outliner** : arbre (branche `else` des `if`), glisser-déposer (déplacer, ou déposer une entrée de la palette ; au-
  dessus / dessous / dedans selon la cible), renommer (double-clic / F2), multi-sélection, Suppr, Ctrl+D, menu
  contextuel (ajouter, envelopper dans VStack / HStack / ZStack, dupliquer, supprimer).
- **Palette** : Layout, Content, Controls, Shapes, Logic (`if`, `ForEach`, `Slot`) et les vues du projet ; recherche ;
  glisser sur l'outliner ou double-cliquer (dans le conteneur sélectionné, sinon après la sélection).
- **Inspecteur** (généré depuis le catalogue / les params du composant) : libellé ; arguments avec éditeurs typés —
  nombre, case, menu d'enum, **jetons Colora avec pastilles + sélecteur de couleur**, **sélecteur de symboles avec
  aperçus et recherche**, liaison à un état, transitions (Anima : opacity / scale / slide), événement de `.on` — et
  « Edit as code » sur chaque argument (expression Veka ; une expression fausse est refusée et ne change rien) ;
  condition d'un `if`, collection et variables d'un `ForEach`, slot d'un `Slot`, « In slot » dans un composant ; actions
  (set / toggle / add / emit, cible, valeur, animé) ; pile de modificateurs (ajouter, monter / descendre, retirer).
- **Onglet State** : états, params, événements, slots — ajouter, renommer (partout), type, valeur initiale, retirer ;
  **valeurs mock** du modèle de prévisualisation (Details de l'instance, non sauvegardées).
- **Sélection synchronisée** viewport ⇄ outliner ⇄ code : les éléments construits par l'interpréteur portent leur nœud
  (`FVekaElement::Source`, document d'origine = l'asset, pas ses composants) ; clic dans le viewport en mode **Select**
  (Ctrl+clic ajoute ; contour orange, survol plus clair ; Select désactivé = utiliser la vue) ; le curseur du code
  sélectionne la vue de sa ligne ; une sélection ailleurs déplace le curseur du code.
- **Lecture seule** tant que le code a des erreurs (le texte n'est jamais écrasé).
- **Disposition** v3 : Outliner + Palette | Viewport + Code | Inspector / State / Details.
- **Tests** : 84 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Editor.Simple.{Build,
  RoundTrip, Context, Pick, Panels}` : **une vue complète faite uniquement avec les éditions du Mode Simple** (états,
  événement, Text interpolé, Button à actions, Toggle lié, if / else, modificateurs réordonnés, déplacement, duplication,
  enveloppe, renommage partout), valide, et **Simple → Code → Simple sans perte** ; la boutique éditée en Simple garde
  commentaires, composants et chemins ; une édition = un undo, sélection de ce qui a été créé, lecture seule ;
  clic viewport → le bon nœud ; panneaux du vrai éditeur (outliner complet, édition de texte / enum / expression dans
  l'inspecteur reflétée dans le code, expression fausse refusée, lecture seule pendant une erreur de code).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3d-select.jpg`, `phase3d-colora.jpg`, `phase3d-symbols.jpg`,
  `phase3d-drag.jpg`), sur une vue neuve `VV_SimpleDemo` (non sauvegardée) : texte édité dans l'inspecteur ; Toggle
  ajouté depuis la palette ; état ajouté puis renommé `wifi` dans State, lié à `isOn` ; `.background` ajouté, jeton
  Indigo choisi dans les pastilles ; clic sur le bouton dans le viewport → sélection partout ; symbole `star` choisi dans
  le sélecteur (aperçu) ; Button glissé au-dessus de Text ; Ctrl+Z annule le déplacement. Trois défauts trouvés et
  corrigés : la sélection se perdait après une édition (rafraîchissement de l'arbre pris pour un clic), « 0:0 » dans les
  warnings, liaison affichée en code quand elle tenait une constante.
- **Assets** : `VV_Shop` sauvegardé avec les modifications de TheMiniBoss (« Save and close »).
- **Limites** : la sélection est un chemin dans l'arbre (après un undo qui déplace un nœud, elle désigne ce qui est à cet
  endroit) ; pas de glisser-déposer dans le viewport ; arguments des primitives du schéma non listés (seulement ceux
  déjà donnés) ; un coin de l'écran est couvert par l'overlay Razer (clics perdus pendant la vérification à la souris).

### 2026-11-21 — CLOUD + LOCAL → jalon 3c Mode Avancé (onglet Code)

- **Branche** : `cpp/phase-3c-code` (partie de `C++` @ `49ce0be`), fusionnée dans `C++`.
- **Onglet Code** (`SVekaCodeEditor`, nouvelle disposition Code | Viewport | Details) : la vue en code Veka, police
  mono, **coloration** (mots-clés, déclarations `@`, composants, modificateurs, valeurs `.enum`, chaînes, nombres,
  couleurs hex, commentaires — `Veka::Editor::TokenizeLine`).
- **Parse à la frappe** : 150 ms après la dernière touche (synchrone : un parse de la boutique prend bien moins d'une
  frame). Code valide → `ApplyDocument` (un pas d'undo par pause, le viewport suit). Code invalide → **le texte reste**,
  erreurs **soulignées en rouge** (warnings en orange) et listées sous l'éditeur (clic = aller à la ligne), barre
  d'état « N errors — the view keeps its last valid version » : le document garde sa dernière version valide.
- **Jamais de texte perdu** : si le document change ailleurs (undo, Details, autre outil) pendant que le code a des
  modifications non appliquées, le code est gardé et un bouton « Reload from document » apparaît.
- **Mise en page conservée** : `UVekaView::SourceText` (editor-only) garde le code tel qu'écrit (lignes vides,
  espacement), dans la même transaction que le document (undo / redo rendent les deux) ; réaffiché tant qu'il se
  parse vers le document, sinon le document est imprimé. Les autres outils (`ApplyDocument` sans texte) l'effacent.
- **Complétion** (`Veka::Editor::ComputeCompletion`, popup au curseur, Ctrl+Espace ou automatique après `.`, `@` ou
  deux lettres ; Haut / Bas, Entrée / Tab, Échap) : composants, vues du projet et mots-clés en début de ligne ;
  modificateurs après une vue ; noms d'arguments dans un appel (sauf ceux déjà donnés) ; valeurs d'enum et jetons
  Colora après `.` dans un argument ; déclarations après `@` ; noms de symboles dans une chaîne de `Symbol` ; états,
  paramètres, propriétés du modèle de prévisualisation après `model.` ; `toggle()` sur un état.
- **Édition** : Tab = 4 espaces, Entrée garde l'indentation (+1 niveau après `{`), bouton **Format** (forme canonique).
- **Tests** : 79 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Editor.Code.{Tokenize,
  Completion, Apply, Shop}` : couleurs d'une ligne ; 15 cas de complétion ; application après la pause, mise en page
  gardée, undo / redo du document et du code, erreur qui n'efface rien (texte gardé, document intact, message avec sa
  ligne), changement extérieur pendant une erreur, Format ; **la boutique écrite entièrement en code** dans une vue
  vide = le document de la boutique (composant `VV_ShopItem` du projet résolu par nom).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3c-code-error.jpg`, `phase3c-code-completion.jpg`,
  `phase3c-code-enum.jpg`) : coloration de `VV_Shop` ; `Spacer(min: cnt)` → soulignement, « 1 error — the view keeps
  its last valid version », liste « 19:25 error: Unknown name 'cnt' », viewport inchangé ; corrigé → « Applied » ;
  `.fo` → popup des modificateurs au curseur ; `fontWeight(.` → les 12 graisses ; `Bo` + Entrée → appliqué.
- **Assets d'exemple** : l'éditeur s'est fermé sans sauvegarder des modifications non enregistrées de `VV_Shop` /
  `WBP_VekaShop` (demande de TheMiniBoss : « Save them ») ; restaurés depuis l'autosave UE de 17:03
  (`Saved/Autosaves`), anciennes versions gardées dans `Saved/VekaSamplesBackup_20261121`.
- **Limites** : Ctrl+Z dans la zone de code annule le texte caractère par caractère (l'undo de l'éditeur, menu Edit,
  annule par pause) ; pas de numéros de ligne ; parse synchrone (assez rapide pour les vues actuelles).

### 2026-11-21 — CLOUD + LOCAL → jalon 3b Asset Editor (coque)

- **Branche** : `cpp/phase-3b-editor` (partie de `C++` @ `da1ab39`), fusionnée dans `C++`.
- **Ouverture** : `UAssetDefinition_VekaView` (nom « Veka View », couleur, catégorie User Interface) — double-clic sur
  une Veka View → `FVekaViewEditorToolkit` (fenêtre d'éditeur d'asset standard, onglets dockables **Viewport** et
  **Details**, barre d'outils Refresh / Reset state / Fit).
- **Viewport** (`SVekaViewPreview`) : la vue tourne en vrai (boutons, toggles, états ; le dernier événement émis est
  affiché dans la barre d'état) sur un **écran** au choix — 1080p, 1440p, 4K, Ultrawide (3440×1440), Mobile
  (1170×2532) — mis en page à pixels / **DPI** (« auto » = la courbe DPI du projet, comme le jeu, ou 0,5 à 3 fixe) ;
  **zoom** à la molette autour du curseur, **pan** clic droit / milieu, F ou Fit pour cadrer (l'écran suit la taille du
  viewport tant qu'on n'a pas zoomé à la main) ; **Light / Dark** et **thème Colora** (sélecteur d'asset `UVekaTheme`) ;
  overlays **Bounds** (contour de chaque vue) et **Padding** (zones de padding en vert). Réglages gardés dans
  `EditorPerProjectUserSettings`.
- **Modèle de prévisualisation** : `UVekaView::PreviewModelClass` (editor-only) — le viewport et la vignette
  instancient ce view model, donc `model.X` a des valeurs (la boutique montre Aria, 3 articles). `VV_Shop` l'a
  (`UVekaSampleViewModel`). Les états modifiés à la main dans le viewport sont gardés quand le document change ; un état
  resté à sa valeur déclarée suit la nouvelle valeur déclarée.
- **Undo / redo** : `Veka::Editor::ApplyDocument(View, Document, Description)` = une transaction (`FScopedTransaction`
  + `Modify`) ; toutes les éditions des outils 3c / 3d passeront par là. Le toolkit est un `FEditorUndoClient` et
  écoute `OnObjectPropertyChanged` (la vue ou une vue utilisée comme composant) → viewport rafraîchi. Les vues sont
  marquées `RF_Transactional` à l'ouverture.
- **Vignettes** : `UVekaViewThumbnailRenderer` — la vue elle-même (avec son modèle de prévisualisation, en sombre),
  rendue par `FWidgetRenderer` à sa taille naturelle puis mise à l'échelle.
- **Exemple** : le fond de la boutique est maintenant le jeton Colora `WindowBackground` (au lieu de `#24242B`) pour être
  lisible en clair ; `Veka.CreateSampleAssets` notifie les éditeurs ouverts.
- **Tests** : 75 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Editor.{Preview, Undo, Toolkit,
  Thumbnail}` : 5 écrans (taille logique = pixels / DPI du projet, vue mise en page à cette taille, écran cadré dans le
  viewport), DPI fixe, schéma clair / sombre jusqu'à la couleur du texte, overlays (contours, bandes de padding), états
  gardés / suivis / remis à zéro ; undo / redo d'une édition ; ouverture par le même chemin que le double-clic, édition
  et undo qui rafraîchissent le viewport ; vignette rendue (plus de 20 couleurs).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3b-editor-light.jpg`, `phase3b-editor-dark-overlays.jpg`) :
  double-clic sur `VV_Shop`, les 5 écrans, clair / sombre, overlays, recadrage au redimensionnement, clic sur « Buy »
  (« Last bought: Health potion », barre d'état « last event: Buy(25) »), case cochée dans Details puis Ctrl+Z
  (la case et le viewport reviennent). Vignettes de `VV_Shop` / `VV_ShopItem` visibles dans le Content Browser.
- **Limites** : le panneau Details montre le document brut (l'arbre et le code arrivent en 3c / 3d) ; pas de
  sélection dans le viewport ; l'écran Mobile suit la courbe DPI du projet (1,08), pas une densité de téléphone.

### 2026-11-21 — CLOUD + LOCAL → jalon 3a VekaLang (langage Veka `.vk`)

- **Branche** : `cpp/phase-3a-lang` (partie de `C++` @ `93ad476`), fusionnée dans `C++`.
- **Catalogue** (`View/VekaCatalog.h`, module VekaUI) : les composants et modificateurs que l'interpréteur connaît par
  nom, avec pour chaque argument son genre (nombre, booléen, texte, nom, peinture, enum + `UEnum`, **binding** = doit
  être un état), l'argument **principal** (écrit sans nom : `Text("Hi")`, `.padding(16)`), conteneur / actions. Sert au
  parser, au printer, à l'analyse, et plus tard à la complétion (3c).
- **Module `VekaLang`** (UncookedOnly) — `VekaLanguage.h` : `Veka::Lang::Parse`, `Print`, `Analyze`,
  `StructurallyEqual`, `MakeAssetResolver`.
  - **Lexer** : identifiants, nombres, `"texte"` (Text localisable), `#"chaîne"` (String), `#RRGGBB[AA]`, échappements
    `\n` `\t` `\"` `\\`, interpolation `\( expr )`, commentaires `//` attachés au jeton suivant.
  - **Parser** récursif descendant → `FVekaDocument` : `view Nom { … }`, `@State var`, `@Param var`, `@Slot`,
    `@Event nom(Type)`, éléments + arguments (principal sans nom ; un composant à un seul paramètre aussi), blocs
    d'enfants ou d'actions, `} action { … }`, `if / else if / else`, `ForEach(expr) { item, index in … }`,
    `Slot(nom)`, modificateurs `.nom(args) { actions }`, `.inSlot(x)`, `.on(evt) { … payload … }` ; actions `=`, `+=`,
    `-=`, `.toggle()`, `emit E(expr)`, `withAnimation { }` ; expressions avec priorités, ternaire, `!`, `-`, `.champ`,
    `[i]`, `min / max`, `format("{1} {0}", …)`, `rgba()`, `vec2()`, tableaux / structs constants, `model.X`. Noms
    résolus local → param → état ; composants par nom d'asset (registre d'assets). Positions ligne / colonne sur nœuds
    et expressions. **Récupération d'erreurs** : une erreur abandonne la vue fautive et reprend à la ligne suivante, donc
    toutes les erreurs du fichier sont rapportées et le reste du document est gardé.
  - **Textes localisables** : namespace `Veka.<vue>` (ou `FVekaParseOptions::TextNamespace`), clé = MD5 du texte
    source (stable d'un parse à l'autre). Messages d'erreur en `LOCTEXT` (suivent la langue de l'éditeur).
  - **Printer canonique** : indentation 4 espaces, argument principal sans nom, un modificateur par ligne (sous une vue
    d'une ligne : indenté ; après `}` : aligné), `else if`, interpolation quand le motif est `{0} {1}…` dans l'ordre,
    sinon `format(…)`, flottants au plus court qui relit la même valeur.
  - **Analyse** : composant / modificateur / argument inconnu (catalogue puis schéma par réflexion), valeur d'enum (avec
    la liste des valeurs), jeton Colora, nombre attendu, binding qui n'est pas un état (warning), enfants ou actions là
    où il n'en faut pas, + `FVekaDocument::Validate`.
- **Import** : `UVekaViewImportFactory` (VekaEditor) — glisser un `.vk` dans le Content Browser crée une Veka View ;
  erreurs et warnings dans le log d'import avec `fichier:ligne:colonne` ; un fichier en erreur ne crée / ne remplace rien.
- **Exemples** : `Samples/Veka/VV_Starter.vk`, `VV_ShopItem.vk`, `VV_Shop.vk` = `Print` des documents d'exemple
  (fichiers de référence des tests).
- **Tests** : 71 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.Lang.{RoundTrip.Samples,
  RoundTrip.Corpus, Golden, Errors, Analyze, Localization, Import}` : `Parse(Print(doc)) == doc` et `Print` point fixe
  sur les 3 exemples et un corpus (toutes les formes de déclaration, d'action, d'opérateur et de littéral, commentaires,
  positions) ; texte imprimé == fichiers `.vk` (sinon `.actual` écrit à côté) ; 12 erreurs avec message, ligne et
  colonne + récupération (3 erreurs dans un fichier) ; 10 diagnostics d'analyse exacts ; clés de traduction ; import.
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3a-import.jpg`) : Import de `Samples/Veka/VV_Shop.vk` dans
  `/Game/VekaSamples` (remplace `VV_Shop`) ; le Designer de `WBP_VekaShop` affiche la boutique issue du texte
  (composant `VV_ShopItem` résolu par nom, badge NEW, article trop cher désactivé). L'asset importé n'est pas
  enregistré (contenu identique ; `Veka.CreateSampleAssets` le recrée).
- **Limites** : pas de réimport automatique (le chemin du fichier source n'est pas stocké, prévu avec l'onglet Code) ;
  le type déclaré d'un `@State` n'est pas vérifié contre sa valeur initiale ; les diagnostics des modificateurs
  pointent la ligne de la vue (pas de position par modificateur).

### 2026-11-21 — CLOUD + LOCAL → jalon 3.0 Document v2

- **Branche** : `cpp/phase-3.0-document` (partie de `C++` @ `f8274d9`), fusionnée dans `C++`.
- **`FVekaValue`** : nouveaux types **Text** (`FText` localisable : les littéraux des vues sont rassemblés et traduits
  par le Localization Dashboard), **Array** et **Struct** (`GetItem`, `GetField`, comparaison et hash par contenu).
  UHT refuse un struct qui contient un tableau de lui-même : les éléments sont stockés en `FInstancedStruct` (chacun un
  `FVekaValue`), sérialisés pareil. Réflexion : un `TArray` devient un Array, un struct quelconque un Struct de ses
  champs, un `FText` reste un Text (les items du modèle arrivent dans la vue).
- **`FVekaDocument` format 2** : `@Param` (`FVekaParamDeclaration` : nom, type, valeur par défaut), `@Slot`
  (`FVekaSlotDeclaration`), événements typés (`FVekaEventDeclaration` : nom + type du payload), commentaires (document
  et nœuds), positions source (`FVekaSourceRange` sur nœuds et expressions, remplies par le parser en 3a).
  Expressions `Param`, `Member` (`item.name`), opérateur `Index` ; format avec motif **Text** → `FText::Format`
  (motif traduisible, nombres formatés selon la culture). Nœuds : `Slot` (contenu injecté) et nœud **composant**
  (`Component` = une autre `UVekaView` : arguments → paramètres, enfants → slots via `SlotName`, modificateur `on`
  → gestion de ses événements avec la variable locale `payload`). `ForEach` sur un tableau (variable = l'élément,
  `IndexVariable` = l'index).
- **Version du format** : `FormatVersion` + `Upgrade()` (format 1 → 2 : les noms d'événements deviennent des
  déclarations), appelé au chargement (`UVekaView::PostLoad`) et à l'interprétation. `Validate()` vérifie aussi
  paramètres, slots, et pour un composant : ses paramètres, slots et événements.
- **Interpréteur** : chaque instance de composant est une vue avec **son propre store** (états par instance, gardés
  quand les paramètres changent), le modèle du parent, ses slots construits dans la portée de l'appelant ; un événement
  sans `on` remonte au parent (donc à l'hôte). Garde contre la récursion (16 niveaux).
- **Exemple** : composant `ShopItem` (params title / price / symbol, slot `badge`, événement `buy(Int)`) et boutique
  `ForEach(model.Items)` : un composant par article, badge « NEW » dans le slot, article désactivé si trop cher,
  `on buy` qui note « Last bought » puis renvoie `Buy` à l'hôte. Textes en `LOCTEXT` (namespace `VekaSamples`).
  `Veka.CreateSampleAssets` crée `VV_ShopItem`, `VV_Shop` et `WBP_VekaShop` dont le **graphe Blueprint est câblé** :
  `VekaHost.OnVekaEvent` → `UVekaSampleLibrary::HandleShopEvent(VekaHost, EventName, Payload)` (retire l'or du modèle).
  La factory « Veka View » démarre maintenant d'un petit document (titre + compteur).
- **Tests** : 64/64 ✅ (1 avec le warning attendu « unknown component ») — nouveaux `VekaUI.View.{ValueV2, DocumentV2,
  Components, Shop, SampleBlueprint}` : tableaux / structs / textes et réflexion du modèle ; mise à niveau (contre-
  épreuve : document courant intact) ; validation des paramètres, slots et composants ; deux instances d'un composant à
  état (état séparé, absent du store de l'appelant, gardé quand le paramètre change) ; boutique (3 composants, badge
  seulement pour l'article nouveau, article désactivé muet, événement géré par l'appelant puis transmis avec le prix,
  action dans la portée de l'appelant) ; **le vrai `WBP_VekaShop`** créé avec `CreateWidget` : un tap sur « Buy »
  passe par son graphe Blueprint et retire 25 or (120 → 95). Le bug « Upgrade ne déplaçait rien » a été trouvé par
  ces tests.
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase3.0-components.jpg`) : Designer de `WBP_VekaShop` avec les 3
  composants, le badge et l'article désactivé. Capture du graphe non faite : une fenêtre invisible (overlay Razer)
  bloquait les clics sur le haut droit de l'éditeur ; le câblage est prouvé par le test `SampleBlueprint`.
- **Limites** : composants référencés par pointeur d'asset (le parser 3a les résoudra par nom) ; pas de valeur de
  retour / binding vers un état du parent (`$value`) pour les paramètres d'un composant.

### 2026-11-21 — CLOUD → décisions pour la Phase 3 (réponses de TheMiniBoss)

- **Plan de la Phase 3 validé** (« go »), ordre 3.0 → 3a → 3b → 3c → 3d → 3e → 3f → 3g.
- **Nom du langage : « Veka »**, fichiers **`.vk`** (le module C++ reste `VekaLang`).
- **Syntaxe** : celle de `VEKAUI_PHASE0.md` §14.2, avec `VStack` / `HStack` / `ZStack` en raccourcis (laissé à Claude).
- **Langue** : anglais d'abord, mais traduisible. L'éditeur et les messages du langage passent par le système de
  localisation d'Unreal (`LOCTEXT` / `FText`) : ils suivent la langue de l'éditeur dès qu'une traduction existe. Les
  textes des vues (littéraux du langage) sont stockés en `FText` dans les assets, donc rassemblés et traduits par le
  Localization Dashboard comme le reste du jeu.
- **Priorité** : le code avant le Mode Simple (laissé à Claude).

### 2026-11-21 — CLOUD → plan de la Phase 3 (proposition, à valider)

Phase 3 de `VEKAUI_PHASE0.md` §16.2 (Veka Editor), découpée comme la Phase 2 : **une branche par jalon**
(`cpp/phase-3a-lang`, …), fusionnée dans `C++` quand elle est verte (compile Editor + Shipping, tests, vérif dans
l'éditeur). Critère final de la phase (§16.2) : aller-retour Mode Simple ⇄ Mode Avancé sans perte sur toute la suite
de tests ; une vue complète faite à 100 % en Mode Simple et une à 100 % en code.

| Jalon | Contenu | Dépend de | Critère « vert » |
|---|---|---|---|
| **3.0 Document v2** | ce qui manque au modèle avant d'écrire un langage dessus : `@Param` (paramètres typés avec valeur par défaut), `@Slot` (contenu injecté), une vue utilisée comme composant d'une autre (`ShopItem(title: …)`), événements avec payload typé ; `FVekaValue` tableaux et structs (`ForEach(items)`, `item.name`), accès membre ; positions source et commentaires (trivia) dans l'AST ; version du format + mise à niveau ; graphe Blueprint d'exemple câblé (`SetState` / `OnVekaEvent`) | 2g | une vue asset en utilise une autre avec paramètres et slot ; ancien format chargé et converti ; tests |
| **3a VekaLang** | module `VekaLang` (UncookedOnly) : lexer, parser récursif descendant → `FVekaDocument`, **printer canonique**, récupération d'erreurs (ligne / colonne), analyse sémantique contre le **schéma** (composant / argument / modificateur inconnu, type incorrect, jeton Colora, état ou événement inexistant) ; syntaxe de `VEKAUI_PHASE0.md` §14.2 | 3.0 | `parse(print(doc)) == doc` sur tous les documents de test + un corpus ; commentaires conservés ; messages d'erreur testés |
| **3b Asset Editor (coque)** | `FVekaViewEditorToolkit` : ouverture au double-clic, onglets dockables, **Viewport** (résolutions 1080p / 1440p / 4K / ultrawide / mobile, DPI, zoom / pan, Light / Dark / thème, overlays bounds / padding), barre d'outils, **undo / redo** (`FScopedTransaction` sur le document), vignettes d'assets | 3.0 | `VV_Shop` s'ouvre, s'affiche dans chaque résolution et schéma ; undo / redo |
| **3c Mode Avancé (onglet Code)** | éditeur de texte avec coloration, parse à la frappe (≈150 ms), erreurs en ligne, l'arbre garde la dernière version valide (jamais de texte perdu), **complétion** depuis le schéma (composants, modificateurs, arguments, valeurs d'enum, jetons Colora, symboles, états), formatage | 3a, 3b | la vue boutique réécrite entièrement en code ; une erreur n'efface rien |
| **3d Mode Simple** | **Outliner** (arbre, glisser-déposer, renommer, multi-sélection, palette de composants), **Inspecteur** généré depuis le schéma (arguments, pile de modificateurs ajoutables / réordonnables, sélecteurs visuels Colora / Anima / symboles / enums), onglet **State** (états, paramètres, événements, valeurs mock) ; sélection synchronisée viewport ⇄ outliner ⇄ code | 3b, 3c | une vue complète faite 100 % en Simple ; aller-retour Simple ⇄ Code sans perte |
| **3e Mode Test + Timeline** | aperçu interactif sans PIE (clics, survol, focus, états, `emit` journalisés, modèle mock éditable) ; onglet **Timeline** (pistes, keyframes, lecture, scrub) sur les timelines du document | 3d | la boutique testée en Play avec un modèle mock ; une animation faite à la timeline |
| **3f Symboles et peintures** | **Symbol Library** (recherche, catégories, aperçus en graisses / modes / échelles) ; **Symbol Editor** (import SVG en couches et rôles, grille keyline, aperçu des graisses, couches variables) ; éditeur de **mesh gradient** (points, couleurs) ; sélecteurs dans l'inspecteur | 3b (indépendant de 3c / 3d) | un SVG importé en `UVekaSymbol` et utilisé dans une vue ; un mesh gradient édité à la souris |
| **3g Composants et migration** | composants composés en asset (`@Param` / `@Slot`) ajoutés automatiquement à la palette, à la complétion et à l'inspecteur ; `UVekaComponentLibrary` (composants et styles partagés) ; **migration des thèmes Alpha 13** (structs Colora de la branche `Blueprint`, en lecture seule) vers `UVekaTheme` | 3d | un composant asset utilisé depuis la palette ; les thèmes Alpha 13 reproduits |

Ordre conseillé : **3.0 → 3a → 3b → 3c → 3d → 3e → 3f → 3g** (3f peut venir juste après 3b ; le code passe avant le
Mode Simple parce que le parser et le printer sont la base de l'aller-retour).

**Questions pour TheMiniBoss** :
1. **Syntaxe du DSL** : celle de la Phase 0 (`Stack(.vertical, spacing: 12) { … }`, `$isOn`, `.padding(16)`), avec
   `VStack` / `HStack` / `ZStack` en raccourcis comme dans le C++ ? Ou une autre préférence ?
2. **Langue de l'éditeur** (libellés, messages d'erreur, doc) : anglais comme Unreal, ou français ?
3. **Priorité** : garder le code avant le Mode Simple (proposé), ou voir le Mode Simple plus tôt quitte à le refaire
   un peu ?

### 2026-11-21 — CLOUD + LOCAL → jalon 2g UVekaView + bridge Blueprint (fin de la Phase 2)

- **Branche** : `cpp/phase-2g-view` (partie de `C++` @ `d42cfc5`), fusionnée dans `C++`.
- **`FVekaValue`** (`View/VekaValue.h`) : variant typé (bool, int, float, string, couleur, vector2) avec conversions
  souples (1 == 1.0, nombre « vrai » si non nul, affichage court : `72` et non `72.000003`), lecture / écriture de
  propriétés par réflexion (nombres, chaînes, noms, textes, enums par nom, couleurs, vecteurs, marges).
  `UVekaValueLibrary` : nœuds Blueprint (Make / To, autocast, égalité).
- **`FVekaDocument`** (`View/VekaDocument.h`) : l'AST d'une vue, en tableaux plats sérialisables (nœuds, expressions,
  arguments nommés, modificateurs, actions, déclarations d'état et d'événements) + `Validate()` (indices, états et
  événements inconnus). Expressions : littéral, état, propriété du modèle, variable de boucle, unaires, binaires
  (arithmétique, comparaisons, et / ou, min / max), ternaire, format `{0}`. Actions : Set, Toggle, Increment, Emit
  (option animée). Contrôle : `If` / else, `ForEach`. **`FVekaDocumentBuilder`** pour écrire un document en C++
  (le parser du DSL arrive avec VekaLang en Phase 3).
- **Interpréteur** (`Veka::Interpreter::MakeView`) : la vue est une `FVekaView` comme les vues C++ (ré-évaluée
  seulement quand son état change). Composants nommés (piles, Text, Symbol, Label, Button, Toggle, Slider, Stepper,
  TextField, ProgressView, Gauge, Divider, Spacer, formes), modificateurs nommés (padding, frame, background,
  foreground, border, opacity, clip, offset, scale, rotation, font, fontWeight, fontSize, disabled, buttonStyle,
  toggleStyle, controlSize, symbolRenderingMode, animation, transition, onTap) ; **tout autre composant ou modificateur
  du schéma** est créé par réflexion (arguments → propriétés, peintures depuis une couleur, un hex ou un jeton). Un
  argument lié à un état (`isOn: gift`) devient un `TVekaBinding`. Composant inconnu : texte rouge « Unknown 'X' » +
  warning.
- **Asset `UVekaView`** (un `UVekaViewSource`) + **`FVekaViewStore`** (états, modèle, événements d'une vue montée).
  **`UVekaViewModel`** (Blueprintable, EditInlineNew) : la vue lit ses propriétés par nom (`model.Gold`) ;
  `NotifyChanged`, `GetValue` / `SetValue` par nom ; les changements faits par de simples nœuds « Set » sont vus seuls
  (contrôle 10 fois par seconde).
- **Veka Host (UMG)** : propriétés `View`, `InitialState`, `ViewModel` (instancié : on choisit la classe et on édite
  ses valeurs dans les Details), fonctions Blueprint `SetState`, `GetState`, `SetView`, `SetViewModel`, événement
  **`OnVekaEvent(EventName, Payload)`**.
- **Éditeur** : factory « Veka View » (Content Browser, catégorie UI) ; commande console **`Veka.CreateSampleAssets`**
  qui crée `/Game/VekaSamples/VV_Shop` (la vue d'exemple) et `WBP_VekaShop` (Widget Blueprint avec un Veka Host qui
  l'affiche, modèle `Veka Sample Shop Model`). Ces assets sont dans le projet hôte, pas dans le dépôt : relancer la
  commande pour les recréer (en ligne de commande : `-ExecCmds="Veka.CreateSampleAssets,QUIT_EDITOR"`, virgule et non
  point-virgule).
- **Vue d'exemple « boutique »** (`Veka::Samples::MakeShopDocument`) : nom et or du joueur (modèle), barre de vie,
  stepper « Potions » et toggle « Gift wrap » (état), ligne qui apparaît avec le toggle, bouton « Buy N for M gold »
  qui émet `Buy` et se désactive quand l'or manque (« Not enough gold »).
- **Tests** : 60/60 ✅ — nouveaux `VekaUI.View.{Value, Document, Interpreter, HostBridge}` : conversions et réflexion
  (enum inconnu refusé sans modifier la valeur) ; validation (contre-épreuves : état inconnu lu / écrit, événement non
  déclaré, mauvais indice) ; interpréteur (tap qui incrémente, If / else, ForEach et variable de boucle, changement
  externe, emit évalué avant l'action suivante, primitive du schéma par réflexion, composant inconnu) ; bridge : la vue
  dans un `UVekaHostWidget`, `SetState` / `GetState` appelés par réflexion comme un nœud Blueprint, `OnVekaEvent` reçu
  par un `UFUNCTION` via délégué dynamique, bouton désactivé qui n'émet rien (contre-épreuve), modèle changé par
  `NotifyChanged`, par `SetValue` et silencieusement (polling).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning (une première tentative a trouvé un include manquant, `FMargin`, invisible en build unity : corrigé).
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase2g-*.jpg`) : `WBP_VekaShop` ouvert, le Designer affiche la vue
  asset avec les valeurs du modèle ; changer Gold à 20 et le nom à « Nova » dans les Details met à jour l'aperçu
  (bouton désactivé, « Not enough gold »).
- **Limites connues** :
  - Pas de parser : les documents s'écrivent en C++ ou dans les Details (tableaux plats, peu pratique) jusqu'à VekaLang.
  - Pas encore de `@Param` / `@Slot` (composants composés) ni de tableaux / structs dans `FVekaValue`.
  - Le graphe Blueprint de `WBP_VekaShop` est vide : `SetState` / `OnVekaEvent` sont testés par réflexion, pas câblés
    dans un graphe d'exemple.

### 2026-11-20 — CLOUD + LOCAL → jalon 2f Morph

- **Branche** : `cpp/phase-2f-morph` (partie de `C++` @ `9e46a91`), fusionnée dans `C++`.
- **`matchedGeometry`** (`.MatchedGeometry(id, namespace)`, SwiftUI matchedGeometryEffect) : le runtime retient le cadre
  (en espace racine) de chaque vue qui porte un id. Quand une vue avec le même id apparaît pendant une animation
  (`WithAnimation`, `.Animation`), son cadre part de celui de l'autre et s'anime vers le sien (`AnimateRect`) ; la vue
  qui part (fantôme de transition) vole vers le nouveau cadre en s'effaçant. Le contenu garde sa mise en page finale
  pendant que le cadre s'anime (comportement 2c) : ajouter `.Clip()` sur la carte.
- **`ShapeGroup`** (`Veka::ShapeGroup(lissage, remplissage)(...)`) : les formes du sous-arbre ne se dessinent plus une à
  une ; leurs champs de distance signée sont fusionnés par un **smooth-min** polynomial, le contour est extrait par
  **marching squares** (cellules de 1,5 px, croisements interpolés, chaînage en boucles fermées), puis rempli par le
  renderer Colora (antialiasing, dégradés, cache). Les formes sont capturées à chaque mise en page, avec leurs offsets,
  échelles et rotations (`.Offset` anime la fusion) ; le contour n'est recalculé que si une forme bouge. **Hit-test
  fusionné** : un clic dans le pont entre deux formes touche le groupe. Formes : rectangle, arrondi, capsule, cercle,
  ellipse, et chemins (distance au polygone aplati). Le texte et les symboles se dessinent normalement par-dessus.
  Ce n'est pas un shader : tout est sur CPU, ce qui suffit pour une UI (pas besoin du module de rendu).
- **Playground** (section Morph) : bouton « Open the card » → carte (et retour par la croix), goutte qui se détache d'un
  blob et revient (ressort), cercle qui fond dans une capsule (« île »), blob rouge à glisser qui fusionne avec les
  autres.
- **Tests** : 56/56 ✅ — nouveaux `VekaUI.Morph.{Field, Contour, ShapeGroup, MatchedGeometry}` : smooth-min (min loin,
  pont près, k = 0), cercle extrait à 1 % d'aire près, deux cercles = 2 boucles sans lissage / 1 avec ; les formes d'un
  groupe ne se dessinent pas elles-mêmes (contre-épreuve hors groupe) ; clic dans le pont (contre-épreuve sans
  lissage) ; carte qui part du cadre du bouton, grandit puis atteint le sien (contre-épreuves : sans animation, sans id).
- **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** (`Docs/Sync/Screens/phase2f-morph*.jpg`) : ouverture et fermeture de la carte (captures
  pendant l'animation), goutte qui s'étire et se détache, fusion de l'île, glisser du blob.
- **Limites connues** :
  - Les clics n'atteignent un groupe que dans ses limites de mise en page (Slate) : une forme décalée en dehors par
    `.Offset` se dessine mais ne reçoit pas de clic hors de ces limites.
  - Les transformations des conteneurs intermédiaires d'un groupe sont ignorées (seules celles des formes comptent).
  - `.morphGroup` comme modificateur sur des vues quelconques (fonds de boutons) n'est pas fait : seules les formes
    fusionnent.

### 2026-11-20 — LOCAL → correctifs 2e : lenteur (rendu et interaction), champs qui clignotent, teinte à 360°, picker sur 2 lignes

- **Signalé par TheMiniBoss** dans le Playground : ça rame beaucoup ; le texte des champs clignote dès qu'un autre
  widget change ; le curseur H revient à 0 (tout à gauche) quand on le met au max (360°) ; le picker « Grayscale » du
  panneau couleur passe sur 2 lignes.
- **Lenteur — cause mesurée** (test de diagnostic temporaire, rendu réel) : un rendu complet du Playground coûtait
  **31 ms** et, depuis 2e, des éléments animés en continu (progressions indéterminées, spinner Lazy) le font repeindre à
  chaque image. Tout était re-tessellé à chaque image, et la conversion sRGB était faite pour chaque sommet.
  - `EmitMesh` : chemin rapide pour les couleurs unies (une conversion sRGB, puis seulement l'alpha) ; handle du pinceau
    blanc mis en cache.
  - **Cache de tessellation** dans le renderer (`FillPolylines` / `StrokePolylines` mémorisés par hash des points et des
    paramètres, purge des entrées inutilisées) et API `FVekaTessellation` / `TessellateFill` / `TessellateStroke` /
    `DrawTessellation` ; les symboles gardent leurs maillages tant que rien ne change.
  - Traits : aux virages doux (< 25°) les segments partagent leurs rangées (courbes : 4 sommets par point au lieu d'un
    segment + une jointure) ; aplatissement des symboles à 0,12 unité. Sommets du Playground : 252 000 → 109 000.
  - Résultat : **31 → ~10 ms** par rendu complet du Playground (205 symboles : 11,4 → 2,1 ms).
- **Lenteur à l'interaction (2e signalement : « ça rame encore quand un widget est interagi »)** — mesuré : un simple
  survol ré-évaluait 2 nœuds mais l'image suivante faisait **92 500 mesures (~80 ms au lieu de 10)**, et chaque image
  d'animation (survol, pression) recommençait. Cause : `InvalidateLayout` vidait le cache de mesure de **tous les
  ancêtres** jusqu'à la racine, même quand la taille ne changeait pas ; la re-mesure partait de la racine et, avec un
  cache de 8 propositions par nœud (les piles imbriquées en demandent plus), se démultipliait. Correctif : le nœud est
  re-mesuré avec les propositions déjà vues et on ne remonte au parent que si une taille change ; cache porté à 32.
  Résultat : survol 80 → 12 ms, animation de pression 83 → 10 ms par image, tap qui ré-évalue toute la vue Contrôles
  83 → 10 ms (+ 3 ms d'évaluation). Test `VekaUI.Layout.LocalInvalidation` (contre-épreuve : un libellé qui s'allonge
  pousse toujours son voisin, et revient).
- **Champs qui clignotent — cause** : chaque ré-évaluation crée un nouveau `FText` (INVTEXT) ; la comparaison par
  identité échouait et `SetHintText` réinitialisait la mise en page du `SEditableText`. Mesuré : 2 appels par
  ré-évaluation avant, 0 après (comparaison par contenu).
- **Teinte à 360° → 0 — cause** : la valeur du curseur était relue depuis la couleur, et 360° = 0° (même chose pour une
  teinte à saturation nulle). Le panneau garde les valeurs de canaux qu'il a posées tant qu'elles donnent la couleur
  actuelle. Test `VekaUI.Controls.HueKeptAt360` (glisser réel jusqu'au bout, libellé « 360° »).
- **Picker sur 2 lignes — cause** : la pile partageait l'espace à parts égales même quand tout tenait (le picker
  recevait 89,6 px pour 100,6 nécessaires, le Spacer en gardait 33). Quand tout tient, chaque vue reçoit sa taille
  idéale et les vues extensibles (Spacer, slider, champ, cadre infini) se partagent le reste ; les parts égales ne
  servent plus que quand la place manque. Les pistes et champs de texte s'étendent comme dans SwiftUI. Cas ajouté à
  `VekaUI.Controls.LabelsOnOneLine`.
- **Tests** : 52/52 ✅ (avec un vrai renderer). **BuildPlugin** Win64 : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérifié dans l'éditeur** : H glissé au-delà du max reste à 360° ; mode Grayscale sur une ligne ; texte saisi
  inchangé quand on bascule un autre contrôle.
- **Limite** : l'éditeur repeint toute la fenêtre tant qu'un élément s'anime (pas d'invalidation Slate globale dans
  l'éditeur) ; le coût par image est réduit, pas supprimé. Piste suivante si besoin : une racine d'invalidation par
  hôte Veka.

### 2026-11-20 — CLOUD + LOCAL → jalon 2e Contrôles + Styles

- **Branche** : `cpp/phase-2e-controls` (partie de `C++` @ `22d6056`), fusionnée dans `C++`.
- **Parité Alpha 13** : les enums des widgets Blueprint ont été lus (lecture seule, `git show`) et repris tels quels :
  boutons Plain / Borderless / Bordered / Bordered Prominent (+ KeyBind), toggles Switch / Button / Checkbox (comportements
  Toggle / Radio Strict / Radio Flexible → `Picker` avec ou sans `bAllowsEmpty`), pickers Menu / Segmented / Palette /
  Radio (vertical ou horizontal), jauges Linear / Linear Alt / Circular / Circular Alt, progression Linear / Circular /
  Lazy, champs de texte Plain / Rectangle / Rounded / Capsule × Text / Number / Percent / Currency, sélecteur de couleur
  RGB / HSV / okLCH / Hue / Luminosity / Saturation / Grayscale / CMYK (affichage 0-1 / 0-255 / %), Label Title and
  Icon / Title / Icon, menus Button / Divider / Label / Picker / SubMenu, Divider, Media, Unavailable.
- **Cœur (interaction)** :
  - Gestes ajoutés : glisser (`.OnDrag`, phases Began / Changed / Ended ; un glisser n'est pas un tap), clic droit
    (`.OnSecondaryTap`), **focus clavier** (`.Focusable(OnKey, OnFocus)` : Tab / Shift+Tab via la navigation Slate,
    Entrée / Espace / bouton A activent le tap, anneau de focus seulement au clavier), `.Disabled()` (environnement,
    aucune entrée, contrôles grisés ; un sous-arbre ne peut pas réactiver ce qu'un parent désactive).
  - Un primitif peut héberger un **widget Slate natif** (`GetNativeWidget`) : le champ de texte est un `SEditableText`
    dans un champ dessiné par Veka.
  - **Popups** : `Veka::Popup::Present(contenu, Below | Right | AtCursor)` ouvre un `SVekaHost` dans un menu Slate ancré
    au contrôle du geste en cours, avec son environnement ; `DismissAll()`.
  - `TVekaBinding` (déjà dans `VekaState.h`) se construit maintenant depuis un `TVekaState` : `Toggle(Titre, EtatOn)`.
- **Styles** (`Controls/VekaControlStyles.h`) : comme SwiftUI, un style de bouton ou de toggle est un USTRUCT avec
  `MakeBody(configuration)` (isPressed, isHovered, isFocused, isEnabled, role, controlSize / isOn) ; styles maison
  possibles (`.ButtonStyle(FMonStyle())`). Les autres styles sont des enums d'environnement : `.ControlSize`,
  `.PickerStyle`, `.TextFieldStyle`, `.GaugeStyle`, `.ProgressViewStyle`, `.LabelStyle`.
- **Contrôles** (`Controls/VekaControls.h`) : `Button` (rôles Normal / Destructive / Cancel), `KeyBindButton`, `Toggle`,
  `Slider` (pas, flèches, Origine / Fin), `ColorSlider`, `Stepper`, `Picker`, `Menu`, `MenuContent`, `.ContextMenu`,
  `TextField`, `SecureField`, `NumberField` (analyse « 1 234,5 », « 1,234.25 », « 25 % » ; mise en forme
  `1 499,90 €`), `ColorPicker` (pastille → panneau en popup), `ColorPanel`, `Gauge`, `ProgressView` (indéterminé animé),
  `Divider`, `MediaView` (lecteur + scrubber, module `MediaAssets`), `ContentUnavailable`, `Label` (suit `.LabelStyle`).
  Primitifs : `FVekaTrack` (piste linéaire ou anneau, remplissage, curseur / repère, segment indéterminé),
  `FVekaTextInput`, `FVekaDivider`, `FVekaMediaScrubber`.
- **Bugs de layout trouvés à l'écran et corrigés** (ils touchaient tout le runtime) :
  1. Au placement, une pile rejouait la proposition mesurée ; pour un libellé d'un mot, 0 et l'infini donnent la même
     taille, et la proposition dégénérée (0) était choisie → libellé coupé sur deux lignes. `FindContentProposal`
     préfère maintenant une proposition qui contient la taille.
  2. Les tailles sont fractionnaires et Slate aligne la géométrie au pixel : un texte recevait sa largeur moins une
     fraction de pixel et passait à la ligne. Tolérance d'un pixel pour la coupure et la troncature.
  3. Dans une pile, un `Spacer` recevait sa part comme les autres vues : un libellé à côté d'un Spacer était coupé en
     deux. Les Spacers passent en dernier et ne réservent que leur longueur minimale.
  4. `.Frame` à la racine d'un hôte était ignoré (la racine reçoit tout l'hôte). Le placement passe par la proposition
     intérieure du cadre.
  Test de régression `VekaUI.Controls.LabelsOnOneLine` avec contre-épreuve (un cadre étroit coupe toujours).
- **Tests** : 50/50 ✅ — nouveaux : `VekaUI.Controls.{Styles, ColorMath, Interaction, Render, LabelsOnOneLine}`.
  `Interaction` pilote les vrais widgets : tap, Entrée, Tab non consommé, bouton désactivé (contre-épreuve),
  toggle, glisser de slider (valeur et pas), flèches / Fin, tap contre glisser, stepper borné, picker segmenté, radio
  flexible, liaison de touche, champs texte et nombres (dont saisie invalide), clic droit (le clic gauche n'en est pas
  un), dividers selon la pile. Un test 2d (`Symbols.Metrics`) lisait une référence vers un environnement temporaire :
  corrigé.
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérif visuelle** (`Docs/Sync/Screens/phase2e-controls*.jpg`), à la souris et au clavier : styles et tailles de
  boutons, compteur, Tab puis Entrée, switch / checkbox / bouton, glisser du slider (les jauges et le symbole suivent),
  stepper, pickers, menu File avec sous-menu Share, raccourcis, picker « Sort by », Archive désactivé, Delete rouge,
  menu contextuel au curseur, saisie dans les 4 styles de champ (liés), prix « 42,5 » → « 42,50 € », panneaux de
  couleur HSV / okLCH, « Disable all ».
- **Limites connues** :
  - Le menu picker et les menus sont des popups Slate : ils ne s'ouvrent pas dans les tests headless (`Present`
    renvoie false hors geste, testé).
  - L'ordre de Tab est celui de la navigation Slate (géométrique), pas l'ordre du code.
  - `MediaView` n'a été vérifié qu'avec son état vide (pas de média dans le projet hôte).
  - Pas encore de sélection de date / heure ni de champ multiligne (non présents dans Alpha 13).

### 2026-11-20 — CLOUD + LOCAL → jalon 2d Symboles

- **Branche** : `cpp/phase-2d-symbols` (partie de `C++` @ `03a8fed`), fusionnée dans `C++`.
- **Choix (réponses de TheMiniBoss)** : pas de SVG d'origine → les 19 glyphes Alpha 13 sont **redessinés** ; « ~150
  essentiels » → **205** symboles intégrés (catégories Arrows, Math, Status, Shapes, Weather, People, Communication,
  Interface, Media, Connectivity, Devices, Commerce, Objects, Tools, Text, Places, Veka), tous dessinés pour Veka.
- **Modèle** : un symbole est un **squelette de traits** sur une grille de 24 (zone utile 3..21, voir `keylinegrid`). La
  graisse est l'épaisseur du trait → les 12 graisses (et toute valeur entre deux) sans masters, animables en continu.
  Trait = `lerp(0.45, 3.4, poids^0.85)` unités ; graisse prise de la police (Hairline 0 … ExtraBlack 1) ou `.SymbolWeight`.
  Taille = taille de la police × 4/3 × échelle (Small 0.95, Medium 1.2, Large 1.5).
- **Codé** :
  - Moteur de traits (`VekaPaintRenderer`) : bouts Butt / Round / Square, jointures Miter / Round / Bevel (côté
    extérieur), frange AA de 1 px, traits plus fins que 1 px estompés, points (trait de longueur nulle, bout rond).
    `FVekaStrokeStyle` (Cap, Join, MiterLimit) est maintenant respecté par toutes les formes.
  - `Symbols/VekaSymbol.h` : `FVekaSymbolDefinition` (formes avec rôle primaire / secondaire / tertiaire, couche variable,
    contour remplissable, couleur multicolore), assets `UVekaSymbol` / `UVekaSymbolLibrary` (`RegisterLibrary`, cherchées
    avant les symboles intégrés), `Resolve("bell.slash.fill")` (suffixes `.fill .circle .square .slash`), alias Alpha 13
    (`Info`, `Warning`, `Check`, `Exclamation`, `Interrogation`, `Clock`, `Arrows`, `ArrowUpDown`, `Thunder`, `Triangle`,
    `Eraser`, `Loading`, `Logo`, `Placeholder`, `Test`, `KeylineGrid`).
  - Primitive `FVekaSymbolView` + `Veka::Symbol(name, value)`, `Veka::Label(title, symbol)`. Style d'environnement
    `FVekaSymbolStyle` : `.SymbolRenderingMode` (Monochrome / Hierarchical 100-60-30 % / Palette / Multicolor),
    `.SymbolVariant` (s'additionne), `.SymbolScale`, `.SymbolPalette`, `.SymbolWeight`, `.SymbolEffect(effet, actif)`,
    `.SymbolEffectOnChange(effet, valeur)`, `.SymbolReplace(Replace | MagicReplace)` ; sur l'élément : `.SymbolValue`,
    `.SymbolBadge` (nombre, « 99+ », point).
  - Variantes : `.fill` (les traits intérieurs deviennent des **découpes** couleur `Background`, ou la 2e couleur de
    palette), cercle / carré englobant, barre oblique avec espace découpé.
  - Valeur variable : couches éteintes à 25 %. Effets : Appear, Disappear, Bounce, Pulse, Scale, VariableColor, Wiggle,
    Rotate, Breathe, DrawOn, par couche ou en bloc, vitesse ; repaint continu seulement pendant l'effet.
    Remplacement : Replace (l'ancien rétrécit, le nouveau surgit) et MagicReplace (même symbole : seule la barre se dessine).
  - Nom inconnu → symbole `placeholder` à 60 %, pour que le trou se voie.
  - `VekaImport` : `Veka::Import::ImportSymbolSvg` (path, circle, rect, line, polyline, polygon ; viewBox → grille 24 ;
    rôle par `class` / `id` / `data-role`).
  - `FResolvedText::Weight` ajouté (les symboles suivent la graisse résolue).
- **Tests** : 45/45 ✅ — nouveaux : `VekaUI.Symbols.{Library, Resolve, Metrics, Effects, Render, SvgImport}` (205 symboles
  dans la grille, alias, suffixes, contre-épreuves noms / suffixes inconnus ; trait plus épais avec la graisse, taille
  avec la police ; modes de rendu par couleur des sommets ; valeur variable ; badge ; jetons multicolores).
- **Bug trouvé à l'écran et corrigé** : `.Color(token)` placé après une forme ne la colorait pas (cœur blanc en
  Multicolor). Il colore maintenant la forme qui vient d'être ajoutée (si elle n'a pas déjà un jeton) ; test ajouté.
  Graisses légères d'abord trop lourdes à côté du texte Hairline / Thin : courbe de trait ajustée.
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning.
- **Vérif visuelle** (`Docs/Sync/Screens/phase2d-symbols*.jpg`) : galerie des 205 symboles, 12 graisses à côté de « Ag »,
  échelles, Label, badges, variantes, 4 modes de rendu ; à la souris : Draw on, Pulse, Rotate, Replace / MagicReplace,
  valeur variable.
- **Limites connues** :
  - Les découpes (`.fill`, barre, anneau du badge) peignent la couleur `Background` : correctes sur un fond uni, visibles
    sur une image ou un dégradé (un vrai masque viendra avec le module de rendu).
  - Pas encore d'interpolation entre deux symboles différents (le MagicReplace ne gère que les variantes du même symbole).
  - L'import SVG ne lit pas les transformations (`transform=`), les groupes ni les styles CSS.

### 2026-11-20 — LOCAL → correctif 2c : la graisse s'appliquait mal sur un style

- **Signalé par TheMiniBoss** : dans le Playground, les 12 aperçus « Ag » (Hairline → ExtraBlack) étaient identiques.
- **Cause** : `.Font(style)` placé à l'intérieur de `.FontWeight(...)` réinitialisait la graisse (choix de conception
  de 2c). Le Playground écrit `.Font(Title1).FontWeight(w)` → toutes les colonnes prenaient la graisse de Title1 (Bold).
- **Correctif** (`Private/Typography/VekaTypography.cpp`) : comme dans SwiftUI, la graisse et l'italique sont des valeurs
  indépendantes appliquées par-dessus le style, quel que soit l'ordre des modifiers ; un style remplace seulement la
  taille et une police Slate explicite. Test `VekaUI.Typography.Resolve` mis à jour (graisse extérieure sur style
  intérieur, avec italique).
- **Vérifié à l'écran** : les 12 graisses et leurs italiques sont distinctes. Tests 39/39 ✅.

### 2026-11-20 — CLOUD + LOCAL → jalon 2c Typographie

- **Branche** : `cpp/phase-2c-typo` (partie de `C++` @ `411c7d7`), fusionnée dans `C++`.
- **Police par défaut LT Flode Neue** : les 24 faces (12 graisses + italiques) ont été **extraites des `.uasset`
  `UFontFace` de la branche `Blueprint`** (lecture seule, `git show`) vers `Resources/Fonts/LTFlodeNeue/*.otf`, sans
  copier les assets (leurs chemins de package auraient cassé). Police composite Slate construite au démarrage, police
  du moteur en repli pour les caractères absents. Embarquée au packaging (`RuntimeDependencies`, UFS).
  ⚠️ **Licence** : « Copyright (c) 2025 by LyonsType. All rights reserved. » — police commerciale, à vérifier avant
  toute diffusion publique du plugin.
- **Codé** :
  - `Typography/VekaFont.h` : `EVekaFontWeight` (12 graisses), `UVekaFontFamily` (asset : faces par graisse,
    upright + italique, graisse la plus proche si absente), `Veka::Typography::MakeFont`.
  - `Typography/VekaTypography.h` : 11 **styles sémantiques** (LargeTitle → Caption2), surchargeables par thème
    (`UVekaTheme::TextStyles`, `UVekaTheme::FontFamily`) ; **tracking optique** selon la taille ; échelle de texte
    globale (accessibilité). La typographie est une valeur d'**environnement** posée champ par champ par les modifiers :
    `.Font(style)`, `.FontWeight`, `.Italic`, `.FontSize`, `.FontFamily`, `.Tracking`, `.LineSpacing`, `.TextCase`,
    `.MonospacedDigit`, `.LineLimit`, `.Truncation(Head | Middle | Tail)`, `.MultilineAlignment`,
    `.MinimumScaleFactor`, `.ContentTransition`, `.TextEffect`, `.TextScale`. `.Font(FSlateFontInfo)` (Phase 1)
    fonctionne toujours. Tout s'anime avec Anima (taille, échelle, tracking…).
  - `FVekaText` réécrit : troncature tête / milieu / fin (la tête montre la fin du texte), réduction avant troncature,
    chiffres à chasse fixe, **texte en dégradé** (couleur par glyphe, `.Foreground(paint)`), `contentTransition`
    `Opacity` (fondu) et **`NumericText`** (chiffres qui roulent vers le haut ou le bas selon le sens du nombre),
    **effets par glyphe** Appear / Typewriter / Wave (`Veka::TextEffects`, repaint continu seulement pendant l'effet).
    Les propriétés de mise en page de `FVekaText` (LineLimit, Alignment, Truncation, LineSpacing) passent dans
    l'environnement.
  - `Fonts::Regular / Bold / LargeTitle / Title / Headline / Body / Caption` utilisent maintenant LT Flode Neue : tout
    le Playground est en LT Flode.
- **Spike texte vectoriel** (`VekaImport` + FreeType, `Docs/Spikes/2c-vector-text.md`) : `LoadGlyphOutline` lit les
  contours en `FVekaPath`. **95 %** des contours (352/372) ont la même structure que la Regular, 97 % des paires de
  graisses voisines sont compatibles → l'interpolation continue de graisse est faisable (fondu entre faces voisines
  pour les quelques glyphes incompatibles).
- **Bugs de layout trouvés et corrigés en route** (ils touchaient aussi 2b) :
  1. Un `Text` mesuré avec une largeur de 0 ne se coupait pas → tous les textes paraissaient rigides et les piles
     comprimaient les plus longs (« +137 » coupé dans son bouton). Test `VekaUI.Layout.TextInStacks`.
  2. Au placement, une pile redistribuait l'espace à partir de sa taille finale au lieu de la proposition utilisée à
     la mesure → parts différentes. Elle réutilise maintenant la proposition mesurée (`FVekaNode::FindContentProposal`).
  3. Une valeur animée qui change l'environnement (échelle du texte) n'effaçait pas les caches de mesure des enfants.
     Test `VekaUI.Typography.AnimatedTextScale` (contre-épreuve : échoue sans le correctif).
  4. Pendant une transition de layout, un conteneur re-plaçait ses enfants à la taille animée → colonnes voisines
     comprimées, texte re-coupé (c'était aussi le libellé « Show » coupé en 2b). Le fond suit la taille animée, le
     contenu garde la mise en page finale. Test `VekaUI.Anima.LayoutKeepsFinalContent`.
- **Tests** : 39/39 ✅ — nouveaux : `VekaUI.Typography.{Fonts, Resolve, Truncation, Effects, GlyphRendering,
  AnimatedTextScale}`, `VekaUI.Layout.TextInStacks`, `VekaUI.Anima.LayoutKeepsFinalContent`, `VekaUI.Spike.VectorText`.
- **BuildPlugin** Win64 (Development + Shipping) : ✅ `BUILD SUCCESSFUL`, 0 warning, les 24 polices sont dans le paquet.
- **Vérif visuelle** (`Docs/Sync/Screens/phase2c-typography.png`) : échelle des styles, 12 graisses + italiques,
  tracking / majuscules, texte en dégradé, troncatures, réduction, compteur roulant (1 280 → 1 417), Appear,
  Typewriter, Wave, échelle de texte A- / A+ sans casser la rangée.
- **Limites connues** :
  - Le séparateur de milliers français (U+202F) n'existe ni dans LT Flode ni dans Roboto : remplacé à l'affichage par
    une espace insécable.
  - Le chemin vectoriel (rendu des glyphes en `FVekaPath`, interpolation de graisse) n'est pas encore branché au
    rendu : prévu avec les symboles (2d).
  - `OpenType features` (ligatures discrétionnaires, `tnum`) non accessibles via Slate : `MonospacedDigit` est fait à la
    main, glyphe par glyphe.

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
