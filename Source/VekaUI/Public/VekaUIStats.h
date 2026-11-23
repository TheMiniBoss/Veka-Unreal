// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

/**
 * `stat VekaUI`: the Game Thread cost of VekaUI (Phase 4f, budgets of VEKAUI_PHASE0.md §13). GPU: `stat gpu` ("Veka
 * Backdrop": captures, blurs, glass, blends).
 */
DECLARE_STATS_GROUP(TEXT("VekaUI"), STATGROUP_VekaUI, STATCAT_Advanced);

/** A runtime's frame: evaluation of dirty views, animations, timelines. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Runtime tick"), STAT_VekaRuntimeTick, STATGROUP_VekaUI, VEKAUI_API);
/** Re-evaluation of dirty views (Body, reconcile). */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Evaluate views"), STAT_VekaEvaluate, STATGROUP_VekaUI, VEKAUI_API);
/** Springs, curves and implicit animations. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Animations"), STAT_VekaAnimations, STATGROUP_VekaUI, VEKAUI_API);
/** A host's paint (layout of the nodes and their drawing, tessellation caches). */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Paint"), STAT_VekaPaint, STATGROUP_VekaUI, VEKAUI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Hosts painted"), STAT_VekaHostsPainted, STATGROUP_VekaUI, VEKAUI_API);
