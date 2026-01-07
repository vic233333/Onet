// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OnetPlayerController.generated.h"

class UOnetBoardWidget;

/**
 * PlayerController handles input + UI boostrap.
 * For a UI-driven puzzle game, PlayerController is a natural place to:
 *  - show mouse cursor
 *  - set input mode to GameAndUI
 *  - create and own the main UI widget (e.g., OnetBoard)
 */
UCLASS()
class ONET_API AOnetPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOnetPlayerController();

protected:
	virtual void BeginPlay() override;

private:
	// Widget class assigned in Blueprint (e.g., WBP_OnetBoard).
	UPROPERTY(EditDefaultsOnly, Category="Onet|UI")
	TSubclassOf<UOnetBoardWidget> BoardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Onet|UI")
	TObjectPtr<UOnetBoardWidget> OnetBoardWidget;
};
