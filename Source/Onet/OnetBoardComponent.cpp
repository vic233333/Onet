// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetBoardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	// Ensure minimum logical dimensions of 1x1
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

	// Set physical dimensions with padding (1 tile border on all sides)
	PhysicalWidth = Width + 2;
	PhysicalHeight = Height + 2;
	const int32 PhysicalNumCells = PhysicalWidth * PhysicalHeight;

	// Allocate physical board (includes padding)
	Tiles.SetNum(PhysicalNumCells);

	// Initialize all tiles to empty first (including padding)
	for (int32 i = 0; i < PhysicalNumCells; ++i)
	{
		Tiles[i].TileTypeId = INDEX_NONE;
		Tiles[i].bEmpty = true;
	}

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

	// Populate only the inner logical region (skip padding)
	int32 BagIndex = 0;
	for (int32 LogicY = 0; LogicY < Height; ++LogicY)
	{
		for (int32 LogicX = 0; LogicX < Width; ++LogicX)
		{
			const int32 PhysIndex = LogicalToPhysicalIndex(LogicX, LogicY);
			Tiles[PhysIndex].TileTypeId = TypeBag[BagIndex];
			Tiles[PhysIndex].bEmpty = false;
			BagIndex++;
		}
	}

	// Reset selection state
	bHasFirstSelection = false;
	FirstSelection = FIntPoint(-1, -1);

	// Reset utility states.
	RemainingShuffleUses = MaxShuffleUses;
	bWildLinkPrimed = false;
	ClearHintState();

	// Notify listeners (UI) to build/refresh.
	OnBoardChanged.Broadcast();
	OnSelectionChanged.Broadcast(false, FirstSelection);

	UE_LOG(LogTemp, Log, TEXT("Board initialized: %dx%d (physical: %dx%d) with %d unique tile types."),
	       Width, Height, PhysicalWidth, PhysicalHeight, NumUniqueTypes);

	// Ensure the starting layout has available moves (auto shuffle if needed).
	CheckForDeadlockAndShuffleIfNeeded();
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
	// Check logical bounds
	if (!IsInBonds(X, Y))
	{
		return false; // Out of bounds
	}

	// Convert to physical index and retrieve tile
	const int32 PhysIndex = LogicalToPhysicalIndex(X, Y);
	OutTile = Tiles[PhysIndex];
	return true;
}

bool UOnetBoardComponent::RequestShuffle()
{
	const bool bResult = ShuffleInternal(false);
	if (bResult)
	{
		CheckForDeadlockAndShuffleIfNeeded();
	}
	return bResult;
}

bool UOnetBoardComponent::RequestHint()
{
	if (bIsProcessingMatch || IsBoardCleared())
	{
		return false;
	}

	ClearHintState();

	FIntPoint TileA;
	FIntPoint TileB;
	TArray<FIntPoint> Path;
	if (FindFirstAvailableMatch(TileA, TileB, Path))
	{
		bHasHintPair = true;
		HintTileA = TileA;
		HintTileB = TileB;
		OnHintUpdated.Broadcast(true, HintTileA, HintTileB);
		return true;
	}

	OnHintUpdated.Broadcast(false, FIntPoint(-1, -1), FIntPoint(-1, -1));
	return false;
}

bool UOnetBoardComponent::ActivateWildLink()
{
	if (IsBoardCleared())
	{
		return false;
	}

	if (bWildLinkPrimed)
	{
		return true;
	}

	bWildLinkPrimed = true;
	OnWildStateChanged.Broadcast(true);
	return true;
}

bool UOnetBoardComponent::HasActiveHint(FIntPoint& OutFirst, FIntPoint& OutSecond) const
{
	OutFirst = HintTileA;
	OutSecond = HintTileB;
	return bHasHintPair;
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
bool UOnetBoardComponent::CanLink(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2,
                                  TArray<FIntPoint>& OutPath) const
{
	OutPath.Empty();

	UE_LOG(LogTemp, Warning, TEXT("CanLink called: (%d,%d) -> (%d,%d)"), X1, Y1, X2, Y2);

	// Same position is not a valid link.
	if (X1 == X2 && Y1 == Y2)
	{
		return false;
	}

	// Check if both tiles are in logical bounds and not empty.
	if (!IsInBonds(X1, Y1) || !IsInBonds(X2, Y2))
	{
		return false;
	}

	const int32 Index1 = LogicalToPhysicalIndex(X1, Y1);
	const int32 Index2 = LogicalToPhysicalIndex(X2, Y2);

	if (Tiles[Index1].bEmpty || Tiles[Index2].bEmpty)
	{
		return false;
	}

	// Check if tiles have the same type.
	if (Tiles[Index1].TileTypeId != Tiles[Index2].TileTypeId)
	{
		return false;
	}

	// Convert logical coordinates to physical for pathfinding
	const FIntPoint PhysStart = LogicalToPhysical(FIntPoint(X1, Y1));
	const FIntPoint PhysEnd = LogicalToPhysical(FIntPoint(X2, Y2));

	// BFS structure: Position, Direction, Turns, Path
	struct FPathNode
	{
		FIntPoint Position; // Physical coordinates
		int32 Direction; // 0=Right, 1=Down, 2=Left, 3=Up, -1=Start
		int32 Turns;
		TArray<FIntPoint> Path; // Physical coordinates

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
		FIntPoint(1, 0), // Right
		FIntPoint(0, 1), // Down
		FIntPoint(-1, 0), // Left
		FIntPoint(0, -1) // Up
	};

	TQueue<FPathNode> Queue;
	TMap<TPair<FIntPoint, int32>, int32> VisitedMinTurns; // (Position, Direction) -> Min turns seen

	// Start from the first tile with no initial direction (using physical coordinates).
	// Direction -1 means "no direction yet", so the first move won't count as a turn.
	TArray<FIntPoint> InitialPath;
	InitialPath.Add(PhysStart);
	Queue.Enqueue(FPathNode(PhysStart, -1, 0, InitialPath));

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

			// Check if next position is in physical bounds.
			if (!IsPhysicalInBounds(NextPos.X, NextPos.Y))
			{
				continue;
			}

			const int32 NextIndex = PhysicalToIndex(NextPos.X, NextPos.Y);

			// Check if we reached the target.
			if (NextPos == PhysEnd)
			{
				// Found a valid path! Convert back to logical coordinates for UI.
				TArray<FIntPoint> FinalPath;
				FinalPath.Reserve(Current.Path.Num() + 1);
				for (const FIntPoint& PhysPos : Current.Path)
				{
					FinalPath.Add(FIntPoint(PhysPos.X - 1, PhysPos.Y - 1)); // Convert to logical
				}
				FinalPath.Add(FIntPoint(NextPos.X - 1, NextPos.Y - 1)); // Convert to logical
				OutPath = FinalPath;
				return true;
			}

			// Can only move through empty tiles (not the start or end).
			if (!Tiles[NextIndex].bEmpty)
			{
				continue;
			}

			// Check if we've already visited this state with fewer or equal turns.
			const TPair<FIntPoint, int32> State(NextPos, NewDir);
			if (int32* ExistingTurns = VisitedMinTurns.Find(State))
			{
				if (NewTurns >= *ExistingTurns)
				{
					continue;
				}
			}

			VisitedMinTurns.Add(State, NewTurns);

			// Add to queue.
			TArray<FIntPoint> NewPath = Current.Path;
			NewPath.Add(NextPos);
			Queue.Enqueue(FPathNode(NextPos, NewDir, NewTurns, NewPath));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("CanLink: No path found after BFS"));

	// No valid path found.
	return false;
}

void UOnetBoardComponent::HandleTileClicked(const int32 X, const int32 Y)
{
	UE_LOG(LogTemp, Warning, TEXT("Tile clicked: (%d, %d)"), X, Y);

	// Prevent new clicks while processing a match (during animation).
	if (bIsProcessingMatch)
	{
		return;
	}

	if (!IsInBonds(X, Y) || Tiles.Num() == 0)
	{
		return;
	}

	const int32 Index = LogicalToPhysicalIndex(X, Y); // Convert logical (X, Y) to physical index.

	// Checking an empty tile does nothing.
	if (Tiles[Index].bEmpty)
	{
		return;
	}

	// Record clicked position (in logical coordinates).
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
	bool bCanLink = false;
	bool bConsumedWild = false;

	const int32 FirstIndex = LogicalToPhysicalIndex(FirstSelection.X, FirstSelection.Y);
	const int32 SecondIndex = Index;
	const bool bTilesMatch = Tiles[FirstIndex].TileTypeId == Tiles[SecondIndex].TileTypeId;

	if (bWildLinkPrimed && bTilesMatch)
	{
		// Wild link ignores pathfinding rules for a matching pair.
		Path.Add(FirstSelection);
		Path.Add(Clicked);
		bCanLink = true;
		bConsumedWild = true;
	}
	else
	{
		bCanLink = CanLink(FirstSelection.X, FirstSelection.Y, X, Y, Path);
	}

	if (bCanLink)
	{
		UE_LOG(LogTemp, Log, TEXT("Match successful! Path has %d points."), Path.Num());

		// Set flag to prevent new clicks during animation.
		bIsProcessingMatch = true;

		// Store tiles to remove after delay.
		PendingRemovalTile1 = FirstSelection;
		PendingRemovalTile2 = FIntPoint(X, Y);

		// Broadcast match successful event with the path for animation.
		// UI will draw the connection line.
		OnMatchSuccessful.Broadcast(Path);

		// Set timer to remove tiles after delay (allows animation to play).
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TileRemovalTimerHandle,
				this,
				&UOnetBoardComponent::RemoveMatchedTiles,
				TileRemovalDelay,
				false
			);
		}

		if (bConsumedWild)
		{
			bWildLinkPrimed = false;
			OnWildStateChanged.Broadcast(false);
		}
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

/**
 * Called by timer to actually remove the matched tiles.
 * This is delayed to allow the connection line animation to play.
 */
void UOnetBoardComponent::RemoveMatchedTiles()
{
	// Remove the matched tiles.
	const int32 Index1 = LogicalToPhysicalIndex(PendingRemovalTile1.X, PendingRemovalTile1.Y);
	const int32 Index2 = LogicalToPhysicalIndex(PendingRemovalTile2.X, PendingRemovalTile2.Y);

	Tiles[Index1].bEmpty = true;
	Tiles[Index2].bEmpty = true;

	// Clear pending removal data.
	PendingRemovalTile1 = FIntPoint(-1, -1);
	PendingRemovalTile2 = FIntPoint(-1, -1);

	// Clear processing flag to allow new clicks.
	bIsProcessingMatch = false;

	// Clear any pending hint since board state changed.
	ClearHintState();

	// Notify UI to refresh and hide the tiles.
	OnBoardChanged.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("Matched tiles removed."));

	if (IsBoardCleared())
	{
		OnBoardCleared.Broadcast();
	}
	else
	{
		CheckForDeadlockAndShuffleIfNeeded();
	}
}

bool UOnetBoardComponent::ShuffleInternal(const bool bAutoTriggered)
{
	if (Width <= 0 || Height <= 0 || Tiles.Num() == 0)
	{
		return false;
	}

	if (RemainingShuffleUses <= 0)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TileRemovalTimerHandle);
	}

	bIsProcessingMatch = false;
	bHasFirstSelection = false;
	FirstSelection = FIntPoint(-1, -1);
	PendingRemovalTile1 = FIntPoint(-1, -1);
	PendingRemovalTile2 = FIntPoint(-1, -1);

	ClearHintState();

	// Collect remaining tile types and logical slots.
	TArray<int32> RemainingTypes;
	TArray<FIntPoint> LogicalSlots;
	RemainingTypes.Reserve(Width * Height);
	LogicalSlots.Reserve(Width * Height);

	for (int32 LogicY = 0; LogicY < Height; ++LogicY)
	{
		for (int32 LogicX = 0; LogicX < Width; ++LogicX)
		{
			const int32 PhysIndex = LogicalToPhysicalIndex(LogicX, LogicY);
			if (!Tiles[PhysIndex].bEmpty)
			{
				RemainingTypes.Add(Tiles[PhysIndex].TileTypeId);
			}

			// Reset tile to empty before reassigning.
			Tiles[PhysIndex].TileTypeId = INDEX_NONE;
			Tiles[PhysIndex].bEmpty = true;
			LogicalSlots.Add(FIntPoint(LogicX, LogicY));
		}
	}

	// Shuffle types and slot order.
	for (int32 i = RemainingTypes.Num() - 1; i > 0; --i)
	{
		const int32 SwapIndex = FMath::RandRange(0, i);
		if (i != SwapIndex)
		{
			RemainingTypes.Swap(i, SwapIndex);
		}
	}

	for (int32 i = LogicalSlots.Num() - 1; i > 0; --i)
	{
		const int32 SwapIndex = FMath::RandRange(0, i);
		if (i != SwapIndex)
		{
			LogicalSlots.Swap(i, SwapIndex);
		}
	}

	// Refill board.
	const int32 NumToPlace = FMath::Min(RemainingTypes.Num(), LogicalSlots.Num());
	for (int32 i = 0; i < NumToPlace; ++i)
	{
		const FIntPoint& Slot = LogicalSlots[i];
		const int32 PhysIndex = LogicalToPhysicalIndex(Slot.X, Slot.Y);
		Tiles[PhysIndex].TileTypeId = RemainingTypes[i];
		Tiles[PhysIndex].bEmpty = false;
	}

	RemainingShuffleUses = FMath::Max(0, RemainingShuffleUses - 1);

	// Notify UI.
	OnBoardChanged.Broadcast();
	OnSelectionChanged.Broadcast(false, FIntPoint(-1, -1));
	OnShufflePerformed.Broadcast(RemainingShuffleUses, bAutoTriggered);

	UE_LOG(LogTemp, Log, TEXT("Shuffle performed. Remaining: %d (auto: %s)"), RemainingShuffleUses,
	       bAutoTriggered ? TEXT("true") : TEXT("false"));

	return true;
}

void UOnetBoardComponent::CheckForDeadlockAndShuffleIfNeeded()
{
	if (bResolvingDeadlock || IsBoardCleared())
	{
		return;
	}

	// Local guard to restore flag when scope exits.
	struct FResolveGuard
	{
		bool& Ref;
		bool Prev;

		explicit FResolveGuard(bool& InRef)
			: Ref(InRef)
			  , Prev(InRef)
		{
			Ref = true;
		}

		~FResolveGuard()
		{
			Ref = Prev;
		}
	} ResolveGuard(bResolvingDeadlock);

	while (!IsBoardCleared())
	{
		FIntPoint TileA;
		FIntPoint TileB;
		TArray<FIntPoint> Path;
		if (FindFirstAvailableMatch(TileA, TileB, Path))
		{
			break; // At least one move exists.
		}

		if (!ShuffleInternal(true))
		{
			OnNoMovesRemain.Broadcast();
			break;
		}
	}
}

bool UOnetBoardComponent::FindFirstAvailableMatch(FIntPoint& OutTileA, FIntPoint& OutTileB,
                                                  TArray<FIntPoint>& OutPath) const
{
	OutTileA = FIntPoint(-1, -1);
	OutTileB = FIntPoint(-1, -1);
	OutPath.Empty();

	if (Width <= 0 || Height <= 0 || IsBoardCleared())
	{
		return false;
	}

	// Group tiles by type to minimize checks.
	TMap<int32, TArray<FIntPoint>> TilesByType;
	for (int32 LogicY = 0; LogicY < Height; ++LogicY)
	{
		for (int32 LogicX = 0; LogicX < Width; ++LogicX)
		{
			const int32 PhysIndex = LogicalToPhysicalIndex(LogicX, LogicY);
			if (!Tiles[PhysIndex].bEmpty)
			{
				TilesByType.FindOrAdd(Tiles[PhysIndex].TileTypeId).Add(FIntPoint(LogicX, LogicY));
			}
		}
	}

	for (const TPair<int32, TArray<FIntPoint>>& Entry : TilesByType)
	{
		const TArray<FIntPoint>& Positions = Entry.Value;
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			for (int32 j = i + 1; j < Positions.Num(); ++j)
			{
				TArray<FIntPoint> CandidatePath;
				if (CanLink(Positions[i].X, Positions[i].Y, Positions[j].X, Positions[j].Y, CandidatePath))
				{
					OutTileA = Positions[i];
					OutTileB = Positions[j];
					OutPath = CandidatePath;
					return true;
				}
			}
		}
	}

	return false;
}

void UOnetBoardComponent::ClearHintState()
{
	if (bHasHintPair)
	{
		bHasHintPair = false;
		HintTileA = FIntPoint(-1, -1);
		HintTileB = FIntPoint(-1, -1);
		OnHintUpdated.Broadcast(false, HintTileA, HintTileB);
	}
}

bool UOnetBoardComponent::IsBoardCleared() const
{
	if (Width <= 0 || Height <= 0)
	{
		return true;
	}

	for (int32 LogicY = 0; LogicY < Height; ++LogicY)
	{
		for (int32 LogicX = 0; LogicX < Width; ++LogicX)
		{
			const int32 PhysIndex = LogicalToPhysicalIndex(LogicX, LogicY);
			if (!Tiles[PhysIndex].bEmpty)
			{
				return false;
			}
		}
	}

	return true;
}
