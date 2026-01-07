// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetGameMode.h"
#include "OnetBoardActor.h"
#include "OnetBoardComponent.h"
#include "OnetPlayerController.h"
#include "UObject/ConstructorHelpers.h"

/**
 * Constructor: Sets default classes for PlayerController and BoardActor.
 */
AOnetGameMode::AOnetGameMode()
{
	// Use our custom PlayerController class so we can create the UI and show mouse cursor.
	// PlayerControllerClass = AOnetPlayerController::StaticClass();

	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/Widget/BP_OnetPlayerContorller"));
	if (PlayerControllerBPClass.Class != NULL)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
	else
	{
		PlayerControllerClass = AOnetPlayerController::StaticClass();
	}

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
	UE_LOG(LogTemp, Warning, TEXT("AOnetGameMode::BeginPlay called.")); // Log when BeginPlay is called.

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 5.f, FColor::Green,
			TEXT("Onet Game Mode Started"));
	}

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
