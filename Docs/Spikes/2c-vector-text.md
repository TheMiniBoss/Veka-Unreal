# Spike 2c — texte vectoriel et interpolation de graisse (LT Flode Neue)

**Question** : peut-on animer la graisse du texte en continu (Regular → Bold) avec LT Flode Neue, qui n'est pas une
police variable (12 faces statiques + italiques) ?

**Méthode** : `VekaImport` lit les contours des glyphes avec FreeType (moteur, module `FreeType2`) et les convertit en
`FVekaPath` (`Veka::Import::LoadGlyphOutline`). Le test `VekaUI.Spike.VectorText` compare, pour 31 glyphes et les 12
graisses, le nombre de contours et de points par contour. Deux faces sont **interpolables** pour un glyphe si cette
structure est identique (même condition que les masters de SF Symbols ou d'une police variable).

## Résultats (2026-11-20, LT Flode Neue 4.000)

- Format : OpenType **CFF** (`OTTO`, courbes cubiques), 1000 unités par em.
- **352 / 372** contours (95 %) ont la même structure que la Regular.
- **330 / 341** paires de graisses **voisines** sont compatibles (97 %).
- Différences (structure ≠ Regular) :

| Glyphe | Graisses qui diffèrent |
|---|---|
| `A` | ExtraBold, Black, ExtraBlack (26 points au lieu de 20) |
| `c` | Thin |
| `s` | Hairline |
| `1` | ExtraLight, Light, SemiBold (13 points) ; Bold → ExtraBlack (19 points) |
| `3` | ExtraBlack |
| `&` | Bold → ExtraBlack |
| `%` | Hairline → ExtraLight |

Table complète : log du test (`VekaSpike:`).

## Conclusion

1. **L'interpolation continue de graisse est faisable** pour presque tous les glyphes : la famille a manifestement été
   dessinée à partir de masters compatibles.
2. Pour les glyphes incompatibles, interpoler **entre les deux faces compatibles les plus proches** et faire un fondu au
   passage de la frontière (ex. `A` : interpolation continue jusqu'à Bold, fondu Bold ↔ ExtraBold).
3. Le **chemin Slate** (défaut) garde les 12 graisses discrètes : il est net, rapide et gère le shaping. Le **chemin
   vectoriel** (contours FreeType → `FVekaPath` → tessellateur Colora) sert aux animations de graisse, au morph de
   glyphes et aux traits sur le texte. Il devra mettre en cache les glyphes tessellés par (glyphe, graisse arrondie,
   taille).
4. Les mêmes contours servent aux **symboles** (2d) : un symbole peut être dessiné dans le gabarit Veka avec 3 masters
   compatibles, exactement comme ces faces.

## Suite

- 2d : rendu des chemins de glyphes (remplissage non nul / pair-impair, trous) dans le tessellateur Colora, avec cache.
- Interpolation : correspondance point à point (même ordre), interpolation linéaire des coordonnées et des avances.
- Licence : LT Flode Neue est une police commerciale (LyonsType, « All rights reserved ») : vérifier la licence avant
  toute distribution du plugin avec ces fichiers.
