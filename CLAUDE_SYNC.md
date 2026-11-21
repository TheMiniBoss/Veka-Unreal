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
| Phase | **3** en cours — 3.0 Document v2, 3a VekaLang (langage `.vk`), 3b Asset Editor, 3c onglet Code et **3d Mode Simple** fusionnés dans `C++` (Phase 2 ✅ terminée) |
| Branche active | `C++` |
| Code C++ | `VekaUI.uplugin` + 6 modules ; runtime complet de la Phase 2 + Document v2 + langage Veka (parser, printer, analyse, import `.vk`) + éditeur d'asset Veka View (Mode Simple, onglet Code, viewport, undo, vignettes) ; 84 tests automation ✅ |
| Prochaine étape | Phase 3 — jalon 3e Mode Test + Timeline |
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
