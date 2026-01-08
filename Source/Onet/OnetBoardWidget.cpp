// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetBoardWidget.h"
#include "OnetBoardComponent.h"
#include "OnetTileWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

void UOnetBoardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

/**
 * Handle mouse button down to detect clicks on empty areas.
 * This allows deselecting tiles by clicking on the background.
 */
FReply UOnetBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clear selection when clicking on the background (not on a tile).
	if (Board)
	{
		Board->ClearSelection();
	}

	// Return Unhandled so that child widgets (tiles) can still receive clicks.
	return FReply::Unhandled();
}

/**
 * Initialize the board widget with a board logic component.
 * @param InBoard - Board logic component to bind to this UI.
 */
void UOnetBoardWidget::InitializeWithBoard(UOnetBoardComponent* InBoard)
{
	Board = InBoard;

	if (!Board)
	{
		return;
	}

	// Subscribe to board events so UI updates can be event-driven.
	Board->OnBoardChanged.AddDynamic(this, &UOnetBoardWidget::HandleBoardChanged);
	Board->OnSelectionChanged.AddDynamic(this, &UOnetBoardWidget::HandleSelectionChanged);
	Board->OnMatchSuccessful.AddDynamic(this, &UOnetBoardWidget::HandleMatchSuccessful);
	Board->OnMatchFailed.AddDynamic(this, &UOnetBoardWidget::HandleMatchFailed);

	RebuildGrid();
	RefreshAllTiles();
}

void UOnetBoardWidget::RebuildGrid()
{
	if (!GridPanel || !Board || !TileWidgetClass)
	{
		return;
	}

	GridPanel->ClearChildren();
	TileWidgets.Reset();

	// Configure UniformGridPanel to have padding between tiles.
	GridPanel->SetSlotPadding(FMargin(TilePadding));

	const int32 W = Board->GetBoardWidth();
	const int32 H = Board->GetBoardHeight();

	TileWidgets.SetNum(W * H);

	for (int32 Y = 0; Y < H; Y++)
	{
		for (int32 X = 0; X < W; X++)
		{
			if (UOnetTileWidget* Tile = CreateWidget<UOnetTileWidget>(this, TileWidgetClass))
			{
				Tile->InitializeTile(X, Y);
				Tile->SetFixedSize(TileSize);

				// Each tile notifies the board when clicked.
				Tile->OnTileClicked.AddDynamic(Board, &UOnetBoardComponent::HandleTileClicked);

				// UniformGridPanel expects row, column (map Y - row, X - column)
				GridPanel->AddChildToUniformGrid(Tile, Y, X);

				TileWidgets[Y * W + X] = Tile;
			}
		}
	}
}

void UOnetBoardWidget::RefreshAllTiles()
{
	if (!Board)
	{
		return;
	}

	const int32 W = Board->GetBoardWidth();
	const int32 H = Board->GetBoardHeight();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			FOnetTile TileData;
			if (!Board->GetTile(X, Y, TileData))
			{
				continue;
			}
			
			const bool bIsSelected = bHasSelection && (X == SelectedX) && (Y == SelectedY);
			
			if (UOnetTileWidget* TileWidget = TileWidgets[Y * W + X])
			{
				TileWidget->SetTileVisual(TileData.bEmpty, TileData.TileTypeId, bIsSelected);
			}
		}
	}
}


void UOnetBoardWidget::HandleBoardChanged()
{
	RefreshAllTiles();
}

void UOnetBoardWidget::HandleSelectionChanged(const bool bHasFirstSelection, const FIntPoint FirstSelection)
{
	bHasSelection = bHasFirstSelection;
	SelectedX = bHasSelection ? FirstSelection.X : -1;
	SelectedY = bHasSelection ? FirstSelection.Y : -1;

	RefreshAllTiles();
}

void UOnetBoardWidget::HandleMatchSuccessful(const TArray<FIntPoint>& Path)
{
	// Call the Blueprint implementable event to draw the path.
	// Designers can implement custom drawing and animation in Blueprint.
	DrawConnectionPath(Path);
}

void UOnetBoardWidget::HandleMatchFailed()
{
	// Call the Blueprint implementable event to show feedback.
	ShowMatchFailedFeedback();
}
