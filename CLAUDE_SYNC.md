# CLAUDE_SYNC — journal de bord partagé

Ce fichier relie deux sessions Claude qui travaillent sur VekaUI :

| Rôle | Où | Fait quoi |
|---|---|---|
| **Claude Cloud** | conteneur Linux, **sans Unreal** | conçoit et code le C++, écrit ici ce qu'il a codé, les fichiers créés et **ce qu'il faut compiler / tester** |
| **Claude Local** | PC Windows de TheMiniBoss, **avec UE 5.8** | lit ce fichier, compile (UnrealBuildTool / RunUAT), lance les tests, inspecte les `.uasset`, corrige les erreurs de compilation liées à l'API Unreal, met à jour ce fichier, `git push` |

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
| Chemin du plugin | `C:\Users\TheMiniBoss\Documents\Unreal Projects\VekaHost\Plugins\VekaUI` (clone de ce dépôt, branche `C++`) |
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
| Phase | **1 — Core** : code écrit par Cloud, **pas encore compilé** |
| Branche active | `cpp/phase-1-core` (à fusionner dans `C++` après D-002 vert) |
| Code C++ | `VekaUI.uplugin` + 6 modules ; runtime `VekaUI` complet pour la Phase 1 ; 12 tests automation |
| Prochaine étape Cloud | corriger selon le retour de D-002, puis Phase 2 (Colora + Anima complets, symboles, typo, contrôles) |
| Bloquant | D-002 (compilation + tests sur le PC) |

---

## Demandes ouvertes pour Claude Local

- [ ] **D-002 — Compiler et tester la Phase 1** (branche `cpp/phase-1-core`)
  1. `git fetch` puis `git checkout cpp/phase-1-core` dans `VekaHost/Plugins/VekaUI`. Le plugin s'appelle maintenant **`VekaUI.uplugin`**.
  2. Activer le plugin dans `VekaHost.uproject` : `"Plugins": [{ "Name": "VekaUI", "Enabled": true }]`.
  3. Fermer tous les éditeurs UE, puis compiler `VekaHostEditor Win64 Development`.
  4. Corriger les erreurs **d'API Unreal** seulement (cf. Règles §3), une par une, en notant chaque correction.
     Liste des points dont Cloud n'est pas sûr : voir l'entrée `CLOUD → Phase 1` ci-dessous.
  5. Lancer `Automation RunTests VekaUI` (commande de référence) et noter le résultat des 12 tests.
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
