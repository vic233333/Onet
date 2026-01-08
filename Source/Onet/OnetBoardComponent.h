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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnetSelectionChanged, bool, bHasFirstSelection,
                                             FIntPoint, FirstSelection);

/**
 * Match successful event: fired when two tiles are successfully matched.
 * Passes the path that connects the two tiles for animation purposes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnetMatchSuccessful, const TArray<FIntPoint>&, Path);

/**
 * Match failed event: fired when two tiles cannot be matched.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnetMatchFailed);

/**
 * Shuffle performed event: notifies listeners when a shuffle occurs.
 * RemainingUses tells UI how many manual/auto shuffles are left.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnetShufflePerformed, int32, RemainingUses, bool, bAutoTriggered);

/**
 * Hint event: broadcast when a hint is generated or cleared.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnetHintUpdated, bool, bHasHint, FIntPoint, First, FIntPoint, Second);

/**
 * Wild link primed state change.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnetWildStateChanged, bool, bWildReady);

/**
 * Board cleared event: fired when all tiles are removed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnetBoardCleared);

/**
 * Deadlock event when no moves remain and no shuffle charges are left.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnetNoMovesRemain);

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

	// Clear current selection.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void ClearSelection();

	// Check if two tiles can be linked with at most 2 turns.
	// Returns true if a valid path exists, and optionally returns the path.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	bool CanLink(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<FIntPoint>& OutPath) const;

	// Shuffle remaining tiles. Consumes one of the limited charges (auto or manual).
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	bool RequestShuffle();

	// Generate and broadcast a hint pair (if available).
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	bool RequestHint();

	// Prime a wild link so the next valid pair of identical tiles ignores path rules.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	bool ActivateWildLink();

	// Remaining manual/auto shuffles.
	UFUNCTION(BlueprintPure, Category = "Onet|Board")
	int32 GetRemainingShuffleUses() const { return RemainingShuffleUses; }

	// Maximum shuffles allowed per game.
	UFUNCTION(BlueprintPure, Category = "Onet|Board")
	int32 GetMaxShuffleUses() const { return MaxShuffleUses; }

	// Whether the wild link is primed for the next match.
	UFUNCTION(BlueprintPure, Category = "Onet|Board")
	bool IsWildLinkPrimed() const { return bWildLinkPrimed; }

	// Query the current hint pair (if any).
	UFUNCTION(BlueprintPure, Category = "Onet|Board")
	bool HasActiveHint(FIntPoint& OutFirst, FIntPoint& OutSecond) const;

	// Fired when the board changes (tiles removed, etc.)
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetBoardChanged OnBoardChanged;

	// Fired when selection changes.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetSelectionChanged OnSelectionChanged;

	// Fired when two tiles are successfully matched (with path for animation).
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetMatchSuccessful OnMatchSuccessful;

	// Fired when match attempt fails.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetMatchFailed OnMatchFailed;

	// Fired whenever a shuffle happens (manual or auto).
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetShufflePerformed OnShufflePerformed;

	// Fired when hint pair is generated or cleared.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetHintUpdated OnHintUpdated;

	// Fired when wild link primed state changes.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetWildStateChanged OnWildStateChanged;

	// Fired when all tiles have been cleared.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetBoardCleared OnBoardCleared;

	// Fired when no moves remain and no shuffle charges are left.
	UPROPERTY(BlueprintAssignable, Category = "Onet|Board")
	FOnetNoMovesRemain OnNoMovesRemain;

private:
	// Logical dimensions (what UI sees)
	int32 Width = 0;
	int32 Height = 0;

	// Physical dimensions (includes padding)
	// PhysicalWidth = Width + 2, PhysicalHeight = Height + 2
	// The outer ring is always empty, allowing paths to go around the board edges.
	int32 PhysicalWidth = 0;
	int32 PhysicalHeight = 0;

	// Store tile in 1D array for simplicity and better cache-friendliness.
	// Index = PhysY * PhysicalWidth + PhysX (using physical coordinates)
	UPROPERTY()
	TArray<FOnetTile> Tiles;

	// Simple selection state for MVP: one "first selection" remembered.
	bool bHasFirstSelection = false;
	FIntPoint FirstSelection = FIntPoint(-1, -1);

	// Delay before removing matched tiles (in seconds).
	// This allows time for the connection line animation to play.
	UPROPERTY(EditDefaultsOnly, Category = "Onet|Board")
	float TileRemovalDelay = 0.5f;

	// Timer handle for delayed tile removal.
	FTimerHandle TileRemovalTimerHandle;

	// Tiles pending removal (stored as logical coordinates).
	FIntPoint PendingRemovalTile1 = FIntPoint(-1, -1);
	FIntPoint PendingRemovalTile2 = FIntPoint(-1, -1);

	// Flag to prevent new selections while processing a match.
	bool bIsProcessingMatch = false;

	// Max shuffle uses per game (manual + auto).
	UPROPERTY(EditAnywhere, Category = "Onet|Board")
	int32 MaxShuffleUses = 3;

	// Remaining shuffle charges.
	int32 RemainingShuffleUses = 0;

	// Wild link primed flag.
	bool bWildLinkPrimed = false;

	// Current hint pair.
	bool bHasHintPair = false;
	FIntPoint HintTileA = FIntPoint(-1, -1);
	FIntPoint HintTileB = FIntPoint(-1, -1);

	// Guard to avoid recursive deadlock checks.
	bool bResolvingDeadlock = false;

	// Convert logical coordinate to physical coordinate (add padding offset)
	FIntPoint LogicalToPhysical(const FIntPoint& Logical) const
	{
		return FIntPoint(Logical.X + 1, Logical.Y + 1);
	}

	// Convert logical coordinates to physical index in Tiles array
	int32 LogicalToPhysicalIndex(const int32 X, const int32 Y) const
	{
		return (Y + 1) * PhysicalWidth + (X + 1);
	}

	// Convert physical coordinates to index in Tiles array
	int32 PhysicalToIndex(const int32 PhysX, const int32 PhysY) const
	{
		return PhysY * PhysicalWidth + PhysX;
	}

	// Check if physical coordinates are in bounds
	bool IsPhysicalInBounds(const int32 PhysX, const int32 PhysY) const
	{
		return PhysX >= 0 && PhysX < PhysicalWidth && PhysY >= 0 && PhysY < PhysicalHeight;
	}

	// Check if logical coordinates are in bounds
	bool IsInBonds(const int32 X, const int32 Y) const
	{
		return X >= 0 && X < Width && Y >= 0 && Y < Height;
	}

	// Called by timer to actually remove the matched tiles.
	void RemoveMatchedTiles();

	// Shuffle tiles implementation.
	bool ShuffleInternal(bool bAutoTriggered);

	// Check whether the board has any valid moves; auto-shuffle if allowed.
	void CheckForDeadlockAndShuffleIfNeeded();

	// Search the board for any valid match.
	bool FindFirstAvailableMatch(FIntPoint& OutTileA, FIntPoint& OutTileB, TArray<FIntPoint>& OutPath) const;

	// Clear cached hint state and notify UI if needed.
	void ClearHintState();

	// Return true if all logical tiles are empty.
	bool IsBoardCleared() const;
};
