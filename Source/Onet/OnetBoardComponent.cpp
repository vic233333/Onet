// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetBoardComponent.h"

UOnetBoardComponent::UOnetBoardComponent()
{
	// Set tick to false. We will use event-driven updates instead.
	PrimaryComponentTick.bCanEverTick = false;
}

/**
 * Initialize the board with given dimensions and number of tile types.
 * 
 * @param InWidth - Width of the board in tiles.
 * @param InHeight - Height of the board in tiles.
 * @param InNumTileTypes - Number of unique tile types to use.
 */
void UOnetBoardComponent::InitializeBoard(const int32 InWidth, const int32 InHeight, const int32 InNumTileTypes)
{
	// Ensure minimum dimensions of 1x1
	Width = FMath::Max(1, InWidth);
	Height = FMath::Max(1, InHeight);

	int32 NumCells = Width * Height;

	// Onet game requires pairs, so total cells should be even.
	// For MVP, if odd, we shrink the board by one row to make it even.
	if (NumCells % 2 != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("NumCells must be a multiple of 2. Shrinking height by 1 for MVP."));
		Height = FMath::Max(1, Height - 1);
		NumCells = Width * Height;
	}

	Tiles.SetNum(NumCells);

	// Each pair occupies 2 cells.
	const int32 NumPairs = NumCells / 2;

	// Clamp unique types so that each type has at least one pair.
	const int32 NumUniqueTypes = FMath::Clamp(InNumTileTypes, 1, NumPairs);

	// Build a bag of tile types: each type appears exactly twice.
	// Then shuffle it to randomize placement.
	TArray<int32> TypeBag;
	TypeBag.Reserve(NumCells);

	for (int32 i = 0; i < NumPairs; i++)
	{
		const int32 TypeId = i % NumUniqueTypes;
		// Add twice for the pair
		TypeBag.Add(TypeId);
		TypeBag.Add(TypeId);
	}

	// Fisher-Yates shuffle
	for (int32 i = TypeBag.Num() - 1; i > 0; i--)
	{
		const int32 SwapIndex = FMath::RandRange(0, i);
		if (i != SwapIndex) // Avoid unnecessary swap
		{
			TypeBag.Swap(i, SwapIndex);
		}
	}

	// Populate Tiles from shuffled TypeBag
	for (int32 i = 0; i < NumCells; ++i)
	{
		Tiles[i].TileTypeId = TypeBag[i];
		Tiles[i].bEmpty = false;
	}

	// Reset selection state
	bHasFirstSelection = false;
	FirstSelection = FIntPoint(-1, -1);

	// Notify listeners (UI) to build/refresh.
	OnBoardChanged.Broadcast();
	OnSelectionChanged.Broadcast(false, FirstSelection);

	UE_LOG(LogTemp, Log, TEXT("Board initialized: %dx%d with %d unique tile types."), Width, Height, NumUniqueTypes);
}

/**
 * Read a tile at (X, Y). Returns false if out of bounds.
 * 
 * @param X - X coordinate of the tile.
 * @param Y - Y coordinate of the tile.
 * @param OutTile - Output parameter to receive the tile data.
 * @return - True if the tile is valid and returned, false if out of bounds.
 */
bool UOnetBoardComponent::GetTile(const int32 X, const int32 Y, FOnetTile& OutTile) const
{
	if (X < 0 || X >= Width || Y < 0 || Y >= Height)
	{
		return false; // Out of bounds
	}

	OutTile = Tiles[ToIndex(X, Y)];
	return true;
}

/**
 * Clear the current selection.
 */
void UOnetBoardComponent::ClearSelection()
{
	if (bHasFirstSelection)
	{
		bHasFirstSelection = false;
		FirstSelection = FIntPoint(-1, -1);
		OnSelectionChanged.Broadcast(false, FirstSelection);
	}
}

/**
 * Check if two tiles can be linked with at most 2 turns using BFS.
 * The path can only go through empty tiles (or the start/end tiles).
 *
 * @param X1, Y1 - Coordinates of the first tile.
 * @param X2, Y2 - Coordinates of the second tile.
 * @param OutPath - Output parameter to receive the path points.
 * @return True if a valid path exists with at most 2 turns.
 */
bool UOnetBoardComponent::CanLink(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TArray<FIntPoint>& OutPath) const
{
	OutPath.Empty();

	// Same position is not a valid link.
	if (X1 == X2 && Y1 == Y2)
	{
		return false;
	}

	// Check if both tiles are in bounds and not empty.
	if (!IsInBonds(X1, Y1) || !IsInBonds(X2, Y2))
	{
		return false;
	}

	const int32 Index1 = ToIndex(X1, Y1);
	const int32 Index2 = ToIndex(X2, Y2);

	if (Tiles[Index1].bEmpty || Tiles[Index2].bEmpty)
	{
		return false;
	}

	// Check if tiles have the same type.
	if (Tiles[Index1].TileTypeId != Tiles[Index2].TileTypeId)
	{
		return false;
	}

	// BFS structure: Position, Direction, Turns, Path
	struct FPathNode
	{
		FIntPoint Position;
		int32 Direction; // 0=Right, 1=Down, 2=Left, 3=Up, -1=Start
		int32 Turns;
		TArray<FIntPoint> Path;

		// Default constructor for TQueue::Dequeue
		FPathNode()
			: Position(FIntPoint::ZeroValue), Direction(-1), Turns(0)
		{
		}

		FPathNode(const FIntPoint& InPos, const int32 InDir, const int32 InTurns, const TArray<FIntPoint>& InPath)
			: Position(InPos), Direction(InDir), Turns(InTurns), Path(InPath)
		{
		}
	};

	// Direction vectors: Right, Down, Left, Up
	const FIntPoint Directions[] = {
		FIntPoint(1, 0),   // Right
		FIntPoint(0, 1),   // Down
		FIntPoint(-1, 0),  // Left
		FIntPoint(0, -1)   // Up
	};

	TQueue<FPathNode> Queue;
	TSet<TPair<FIntPoint, int32>> Visited; // (Position, Direction)

	// Start from the first tile in all four directions.
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		TArray<FIntPoint> InitialPath;
		InitialPath.Add(FIntPoint(X1, Y1));
		Queue.Enqueue(FPathNode(FIntPoint(X1, Y1), Dir, 0, InitialPath));
	}

	while (!Queue.IsEmpty())
	{
		FPathNode Current;
		Queue.Dequeue(Current);

		// Try moving in all four directions.
		for (int32 NewDir = 0; NewDir < 4; ++NewDir)
		{
			const FIntPoint NextPos = Current.Position + Directions[NewDir];

			// Calculate turns: if direction changes, increment turn count.
			int32 NewTurns = Current.Turns;
			if (Current.Direction != -1 && Current.Direction != NewDir)
			{
				NewTurns++;
			}

			// Exceeded maximum turns.
			if (NewTurns > 2)
			{
				continue;
			}

			// Check if next position is in bounds.
			if (!IsInBonds(NextPos.X, NextPos.Y))
			{
				continue;
			}

			const int32 NextIndex = ToIndex(NextPos.X, NextPos.Y);

			// Check if we reached the target.
			if (NextPos.X == X2 && NextPos.Y == Y2)
			{
				// Found a valid path!
				TArray<FIntPoint> FinalPath = Current.Path;
				FinalPath.Add(NextPos);
				OutPath = FinalPath;
				return true;
			}

			// Can only move through empty tiles (not the start or end).
			if (!Tiles[NextIndex].bEmpty)
			{
				continue;
			}

			// Check if we've already visited this state.
			const TPair<FIntPoint, int32> State(NextPos, NewDir);
			if (Visited.Contains(State))
			{
				continue;
			}

			Visited.Add(State);

			// Add to queue.
			TArray<FIntPoint> NewPath = Current.Path;
			NewPath.Add(NextPos);
			Queue.Enqueue(FPathNode(NextPos, NewDir, NewTurns, NewPath));
		}
	}

	// No valid path found.
	return false;
}

void UOnetBoardComponent::HandleTileClicked(const int32 X, const int32 Y)
{
	if (!IsInBonds(X, Y) || Tiles.Num() == 0)
	{
		return;
	}

	const int32 Index = ToIndex(X, Y); // Convert (X, Y) to 1D index.

	// Checking an empty tile does nothing.
	if (Tiles[Index].bEmpty)
	{
		return;
	}

	// Record clicked position.
	const FIntPoint Clicked(X, Y);

	// State machine: first click selects, second click attempts match.
	if (!bHasFirstSelection)
	{
		bHasFirstSelection = true;
		FirstSelection = Clicked;

		// UI can highlight the first selection.
		OnSelectionChanged.Broadcast(true, FirstSelection);
		return;
	}

	// Clicking the same tile again cancels selection.
	if (Clicked == FirstSelection)
	{
		bHasFirstSelection = false;
		FirstSelection = FIntPoint(-1, -1);

		// UI clears selection highlight.
		OnSelectionChanged.Broadcast(false, FirstSelection);
		return;
	}

	// Check if the two tiles can be linked.
	TArray<FIntPoint> Path;
	const bool bCanLink = CanLink(FirstSelection.X, FirstSelection.Y, X, Y, Path);

	if (bCanLink)
	{
		UE_LOG(LogTemp, Log, TEXT("Match successful! Path has %d points."), Path.Num());

		// Broadcast match successful event with the path for animation.
		OnMatchSuccessful.Broadcast(Path);

		// Remove the matched tiles.
		Tiles[ToIndex(FirstSelection.X, FirstSelection.Y)].bEmpty = true;
		Tiles[Index].bEmpty = true;

		// Notify UI to refresh.
		OnBoardChanged.Broadcast();
	}
	else
	{
		// Match failed.
		UE_LOG(LogTemp, Warning, TEXT("Match failed: no valid path."));

		// Broadcast match failed event for feedback.
		OnMatchFailed.Broadcast();
	}

	// Reset selection after the second click for simple UX.
	bHasFirstSelection = false;
	FirstSelection = FIntPoint(-1, -1);
	OnSelectionChanged.Broadcast(false, FirstSelection);
}
