// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "Core/VekaElement.h"
#include "VekaDemoHUD.generated.h"

class SVekaHost;

namespace Veka::Samples
{
	/** The Phase 4 in-game demo: a Liquid Glass HUD (status, compass, minimap, objective, ammo, tab bar) for a 3D scene. */
	VEKAUI_API FVekaElement LiquidGlassHud();
}

/** Shows the Liquid Glass HUD over the game viewport while it plays (`Veka.HudDemo` toggles it in PIE). */
UCLASS()
class VEKAUI_API AVekaDemoHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	TSharedPtr<SVekaHost> Host;
};

/**
 * Demo game mode: any level with the Liquid Glass HUD. Set it as the level's GameMode Override, or play with
 * `?game=/Script/VekaUI.VekaDemoGameMode`.
 */
UCLASS()
class VEKAUI_API AVekaDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVekaDemoGameMode();
};
