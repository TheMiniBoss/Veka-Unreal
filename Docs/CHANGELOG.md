# VekaUI changelog

The Veka Hub shows this file (Learn > Changelog). Newest first. `##` starts a release, `-` an entry.

## Fireworks — the birthday card challenge (2026-11-22)
- Fireworks component: bursts at random places, a flash, sparks that fly out, hang, twinkle and fade (Fireworks(rate:, particles:, radius:, size:)).
- Birthday card example in the Hub: the iOS 27 card with fireworks inside a frosted card, a glass button drops it in and out.
- New transitions in .vk: .move (drops in from above) and .blur (comes into focus).

## 4f — Performance and in-game demo (2026-11-22)
- Performance budgets met: 100 springs in under 0.1 ms per frame, full-screen glass in 0.26 ms on the GPU (1080p, High), no Veka work at rest.
- Animated visual values (offset, opacity, scale, background...) only repaint, without a new layout.
- stat VekaUI: the Veka cost on the game thread (runtime tick, views, animations, paint).
- In-game Liquid Glass HUD demo: Veka.HudDemo, the VekaDemoGameMode game mode, or -VekaHudDemo on the command line (Shipping too).

## 4e — Hub: examples, documentation, changelog (2026-11-22)
- Examples in the Hub: live previews, "Open as asset" creates the view in /Game/VekaExamples and opens it.
- Documentation in the Hub: a guide, and the reference of every component and modifier generated from the catalog.
- This changelog in the Hub.
- DesignLab tidied into cards; segmented pickers no longer wrap their labels.
- Blend mode demos set the mode on each shape (shapes blend with each other, like CSS mix-blend-mode).

## 4d — Veka Hub and DesignLab (2026-11-22)
- Veka Hub tab (Window > Tools, level editor toolbar, view editor toolbar), written in VekaUI.
- DesignLab: components, colors, symbols, typography, animations, effects and morph with live settings, Light / Dark and any theme.
- Effects see the content of their own host (the first effect drawn takes the capture).
- Overlapping children (ZStack) paint above each other.

## 4c — Blend modes and liquid distortion (2026-11-22)
- .blendMode(.Multiply ... .Luminosity) against what is behind (W3C formulas).
- .liquidDistortion(strength, scale:, speed:).

## 4b — Liquid Glass (2026-11-22)
- .glass(): refraction from the shape's signed distance field, chromatic aberration, rim lit from the pointer.
- .glassGroup(smoothness): glass shapes that melt together.

## 4a — Backdrop blur (2026-11-22)
- .backdropBlur(radius) and .frostedGlass(); one capture and one blur pyramid shared by the effects of a host.
- r.Veka.Backdrop.Quality (0 off, 1 low ... 4 epic).

## 4.0 — Backdrop spike (2026-11-22)
- Render graph backdrop chosen over the engine's Slate post buffers (0.2 ms full screen 1080p).

## 3g — Components and migration (2026-11-22)
- Component views (@Param / @Slot) checked, completed and listed by library in the palette.
- Veka Component Library: shared components and styles (.style(.Card)).
- Control tokens (ToggleAccent, SliderBackground...) and the Alpha 13 themes migrated to VT_Alpha13.

## 3f — Symbols and paints (2026-11-22)
- SVG import, Symbol Editor, Symbol Library; Veka Paint assets and the mesh gradient editor.

## 3e — Test mode and Timeline (2026-11-21)
- Live preview with states and a log, mock model, document timelines with a Timeline tab.

## 3d — Simple mode (2026-11-21)
- Outliner, palette, inspector, State panel; Simple and Code stay in sync.

## 3c — Code tab (2026-11-21)
- Veka code editor: colors, errors as you type, completion, formatting.

## 3b — Asset editor (2026-11-21)
- Veka View editor: viewport with devices, DPI, Light / Dark and themes; undo; thumbnails.

## 3a — The Veka language (2026-11-21)
- .vk files: parser, canonical printer, analysis against the schema, import.

## 3.0 — Document v2 (2026-11-21)
- Params, slots, components, events with payload, arrays and structs, source positions and comments.

## Phase 2 — Colora, Anima, typography, symbols, controls, morph, views (2026-11-20)
- Themes and tokens, paints (gradients, mesh), springs and timelines, LT Flode Neue, SF-like symbols, every Alpha 13 control, matched geometry, UVekaView and the Blueprint bridge.

## Phase 1 — Core (2026-11-20)
- Declarative elements, state, layout, shapes and strokes, Slate host and UMG widget.
