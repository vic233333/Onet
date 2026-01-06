// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnetBoardActor.generated.h"

class UOnetBoardComponent; // Forward declaration of UOnetBoardComponent

/**
 * Board "container" actor.
 * 
 * This actor primarily exists to hold the UOnetBoardComponent, which contains the game logic.
 * By encapsulating the board logic within an actor, we ensure proper lifecycle management
 * and integration with Unreal Engine's actor system.
 */
UCLASS()
class ONET_API AOnetBoardActor : public AActor
{
	GENERATED_BODY()

public:
	AOnetBoardActor();

	// Provides access to board logic component.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	UOnetBoardComponent* GetBoardComponent() const
	{
		UE_LOG(LogTemp, Display, TEXT("AOnetBoardActor::GetBoardComponent called.")); // Log when the function is called.
		return BoardComponent; // The return is a pointer to the board component.
	}

protected:
	// The logic brain of the board. Marked UPROPERTY so UE GC can track it.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Onet|Board", meta = (AllowPrivateAccess = true))
	TObjectPtr<UOnetBoardComponent> BoardComponent;

	// Root scene component, used as a stable root for the actor.
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
};
