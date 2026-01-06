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

	const FOnetTile FirstTile = Tiles[ToIndex(FirstSelection.X, FirstSelection.Y)];
	const FOnetTile SecondTile = Tiles[Index];

	const bool bSameType = (FirstTile.TileTypeId == SecondTile.TileTypeId);

	if (bSameType)
	{
		// MVP behavior: remove if same type, no path check yet.
		// TODO: replace this with CanLink function
		Tiles[ToIndex(FirstSelection.X, FirstSelection.Y)].bEmpty = true;
		Tiles[Index].bEmpty = true;

		// Notify UI to refresh.
		OnBoardChanged.Broadcast();
	}

	// Reset selection after the second click for simple UX.
	bHasFirstSelection = false;
	FirstSelection = FIntPoint(-1, -1);
	OnSelectionChanged.Broadcast(false, FirstSelection);
}
