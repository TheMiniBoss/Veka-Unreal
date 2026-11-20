# VekaUI

Declarative, Slate-first UI framework for Unreal Engine 5.8+, inspired by SwiftUI.
Successor of the Blueprint plugin "Veka" (Alpha 13, branch `Blueprint`, read-only).

> Status: **Phase 1 (core)** — see `Docs/VEKAUI_PHASE0.md` for the architecture and `CLAUDE_SYNC.md` for the build log.

## Modules

| Module | Type | Content |
|---|---|---|
| `VekaUI` | Runtime | element / node / reconciler, state, environment, schema, layout (Stack, Grid, ScrollView, Spacer), primitives (Text, Image, Shape), vector paths and strokes, modifiers, Anima scheduler, `SVekaHost`, `UVekaHostWidget` |
| `VekaUIRender` | Runtime (PostConfigInit) | shader directory mapping (backdrop / glass in Phase 4) |
| `VekaLang` | UncookedOnly | DSL (Phase 3) |
| `VekaImport` | UncookedOnly | SVG import (Phase 2/3) |
| `VekaEditor` | Editor | Phase 1: `Window > Tools > VekaUI Playground` tab |
| `VekaWeb` | Runtime | WebView (Phase 2) |

## Quick look

```cpp
#include "VekaUI.h"

FVekaElement MakeCard()
{
    using namespace Veka;
    return VStack(12, EVekaHAlign::Leading)(
        Text(INVTEXT("Hello")).Font(Fonts::Title()),
        HStack()(Circle().Fill(FLinearColor::Green).Frame(12, 12), Text(INVTEXT("Online")), Spacer())
    )
    .Padding(16)
    .Background(FLinearColor(0.1f, 0.1f, 0.12f), 12);
}

// Slate:  SNew(SVekaHost).Root(MakeCard())
// UMG:    add a "Veka Host" widget and set ViewClass to a UVekaViewSource subclass.
```

Composite views with state:

```cpp
class FCounter : public FVekaView
{
    VEKA_VIEW(FCounter)
public:
    virtual FVekaElement Body(FVekaContext& Ctx) const override
    {
        const TVekaState<int32> Count = Ctx.State<int32>(TEXT("Count"), 0);
        return Veka::Text(FText::AsNumber(Count.Get())).Padding(8)
            .OnTap([Count]() { Count.Set(Count.Get() + 1); });
    }
};
```

## Extending

Any `USTRUCT` deriving from `FVekaPrimitive` (component / container) or `FVekaModifier` is a first-class
Veka type: Veka's own components use the same API. Visual values must be `UPROPERTY`s.
