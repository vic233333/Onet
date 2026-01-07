// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetGameMode.h"
#include "OnetBoardActor.h"
#include "OnetBoardComponent.h"
#include "OnetPlayerController.h"

/**
 * Constructor: Sets default classes for PlayerController and BoardActor.
 */
AOnetGameMode::AOnetGameMode()
{
	// Use our custom PlayerController class so we can create the UI and show mouse cursor.
	PlayerControllerClass = AOnetPlayerController::StaticClass();

	// Default board actor class (can be overridden in a Blueprint GameMode)
	BoardActorClass = AOnetBoardActor::StaticClass();
}

/**
 * Called when the game starts or when spawned.
 * Spawns the BoardActor and initializes the board component.
 */
void AOnetGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Spawn the BoardActor at the origin with no rotation.
	if (BoardActorClass)
	{
		BoardActor = GetWorld()->SpawnActor<AOnetBoardActor>(BoardActorClass);

		if (UOnetBoardComponent* BoardComp = BoardActor->GetBoardComponent())
		{
			// Initialize the board with specified parameters.
			BoardComp->InitializeBoard(BoardWidth, BoardHeight, NumTileTypes);
		}
	}
}

/**
 * Provides access to the OnetBoardComponent.
 * @return - Pointer to the UOnetBoardComponent, or nullptr if not available.
 */
UOnetBoardComponent* AOnetGameMode::GetOnetBoardComponent() const
{
	return BoardActor ? BoardActor->GetBoardComponent() : nullptr;
}
