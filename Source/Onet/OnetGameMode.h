// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OnetGameMode.generated.h"

class AOnetBoardActor;
class UOnetBoardComponent;

/**
 * GameMode controls how the match starts.
 * In single-player PIE, the PlayerController can access GameMode via GetAuthGameMode().
 * TODO: later add networking, clients cannot directly read GameMode; you'd move shared state to GameState.
 */
UCLASS()
class ONET_API AOnetGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOnetGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	UOnetBoardComponent* GetOnetBoardComponent() const;

	// Reinitialize the board with new parameters.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void ReinitializeBoard(const int32 NewWidth, const int32 NewHeight, const int32 NewNumTileTypes);

private:
	// Which BoardActor class to spawn.
	UPROPERTY(EditDefaultsOnly, Category = "Onet|Board")
	TSubclassOf<AOnetBoardActor> BoardActorClass;

	UPROPERTY()
	TObjectPtr<AOnetBoardActor> BoardActor;

	// Board parameters (tweakable in BP_OnetGameMode or defaults)
	UPROPERTY(EditAnywhere, Category = "Onet|Board")
	int32 BoardWidth = 10;

	UPROPERTY(EditAnywhere, Category = "Onet|Board")
	int32 BoardHeight = 8;

	UPROPERTY(EditAnywhere, Category = "Onet|Board")
	int32 NumTileTypes = 12;
};
