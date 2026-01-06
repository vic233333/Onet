// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnetBoardComponent.generated.h"

/**
 * A single tile on the Onet game board.
 * 
 * Members:
 * - TileTypeId: An integer representing the type of the tile. If not defined, it defaults to INDEX_NONE.
 * - bEmpty: A boolean indicating whether the tile is empty (true) or occupied (false).
 */
USTRUCT(BlueprintType)
struct FOnetTile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Onet|Board")
	int32 TileTypeId = INDEX_NONE; // Type identifier for the tile. If it is not defined, it is set to INDEX_NONE.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Onet|Board")
	bool bEmpty = true; // Indicates whether the tile is empty or occupied.
};

/**
 * Board changed event: UI can listen to this event to update the display.
 * Dynamic multicast makes it bindable in Blueprints.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnetBoardChanged);

/**
 * Selection changed event.
 * We expose both: whether selection exists, and the coordinates of the second selection (if any).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnetBoardComponent, bool, bHasFirstSelection,
                                             FIntPoint, bHasSecondSelection);

/**
 * Board component that contains the game logic for Onet.
 * 
 * Responsibilities:
 * - Store board data (tiles)
 * - Handle selection state machine
 * - Apply match/remove MVP
 * - Broadcast events when the board changes so UI can update
 * 
 * Non-responsibilities:
 * - No UI widgets or rendering code
 * - No animation code
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ONET_API UOnetBoardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnetBoardComponent();

	// Initialize board with size and number of unique tile types.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void InitializeBoard(int32 InWidth, int32 InHeight, int32 InNumTileTypes);

	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	int32 GetBoardWidth() const { return Width; }

	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	int32 GetBoardHeight() const { return Height; }

	// Read a tile at (X, Y). Returns false if out of bounds.
	// UI uses this to render the board.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	bool GetTile(int32 X, int32 Y, FOnetTile& OutTile) const;
	
	// Handle a tile click at (X, Y).
	// This drives the selection state machine and match logic.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void HandleTileClicked(int32 X, int32 Y);

	// Fired when the board changes (tiles removed, etc.)
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetBoardChanged OnBoardChanged;

	// Fired when selection changes.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetBoardComponent OnSelectionChanged;

private:
	int32 Width = 0;
	int32 Height = 0;

	// Store tile in 1D array for simplicity and better cache-friendliness.
	// Index = Y * Width + X
	UPROPERTY()
	TArray<FOnetTile> Tiles;

	// Simple selection state for MVP: one "first selection" remembered.
	bool bHasFirstSelection = false;
	FIntPoint FirstSelection = FIntPoint(-1, -1);

	int32 ToIndex(const int32 X, const int32 Y) const { return Y * Width + X; }

	bool IsInBonds(const int32 X, const int32 Y) const
	{
		return X >= 0 && X < Width && Y >= 0 && Y < Height;
	}
};
