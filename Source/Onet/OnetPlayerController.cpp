// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetPlayerController.h"
#include "OnetBoardWidget.h"
#include "OnetGameMode.h"
#include "OnetBoardComponent.h"

/**
 * Set up PlayerController to show mouse cursor for UI interaction.
 */
AOnetPlayerController::AOnetPlayerController()
{
	// Required for clicking UMG buttons without extra setup.
	bShowMouseCursor = true;
}

/**
 * Called when the game starts or when spawned.
 * Creates and adds the OnetBoardWidget to the viewport.
 */
void AOnetPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Allow both game input and UI interaction.
	// It can also be set to UI only if the game does not require direct input.
	const FInputModeGameAndUI InputMode;
	SetInputMode(InputMode);

	// Create and display the main board widget.
	if (BoardWidgetClass)
	{
		if (UOnetBoardWidget* BoardWidget = CreateWidget<UOnetBoardWidget>(this, BoardWidgetClass))
		{
			BoardWidget->AddToViewport();

			// Get board component from GameMode and pass it to the widget if needed.
			if (AOnetGameMode* GM = GetWorld()->GetAuthGameMode<AOnetGameMode>())
			{
				if (UOnetBoardComponent* BoardComp = GM->GetOnetBoardComponent())
				{
					BoardWidget->InitializeWithBoard(BoardComp);
				}
			}
		}
	}
}
