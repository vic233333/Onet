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

void UOnetBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check if path display duration has elapsed.
	if (bShowPath)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - PathStartTime >= PathDisplayDuration)
		{
			ClearPath();
		}
	}
}

int32 UOnetBoardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                     const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                     int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Call parent paint first.
	int32 Result = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle,
	                                  bParentEnabled);

	// Draw the connection path if visible.
	if (bShowPath && ActivePathPoints.Num() >= 2)
	{
		// Draw lines between consecutive points.
		for (int32 i = 0; i < ActivePathPoints.Num() - 1; ++i)
		{
			const FVector2D Start = ActivePathPoints[i];
			const FVector2D End = ActivePathPoints[i + 1];

			TArray<FVector2D> LinePoints;
			LinePoints.Add(Start);
			LinePoints.Add(End);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				PathColor,
				true,
				PathThickness
			);
		}
	}

	return Result;
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


/**
 * Handlers for board events to update the UI.
 */
void UOnetBoardWidget::HandleBoardChanged()
{
	if (!Board || !GridPanel)
	{
		return;
	}

	// Check if grid size has changed.
	const int32 ExpectedTileCount = Board->GetBoardWidth() * Board->GetBoardHeight();
	if (TileWidgets.Num() != ExpectedTileCount)
	{
		RebuildGrid();
	}

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
	// Draw the connection path in C++.
	DrawConnectionPath(Path);
}

void UOnetBoardWidget::HandleMatchFailed()
{
	// TODO: Add visual feedback for failed match (e.g., screen shake, sound).
	UE_LOG(LogTemp, Warning, TEXT("Match failed!"));
}

FVector2D UOnetBoardWidget::GridToScreenPosition(const FIntPoint& GridCoord) const
{
	// Try to get the actual widget position if available
	if (Board)
	{
		const int32 Index = GridCoord.Y * Board->GetBoardWidth() + GridCoord.X;
		if (TileWidgets.IsValidIndex(Index))
		{
			if (UOnetTileWidget* Tile = TileWidgets[Index])
			{
				const FGeometry& TileGeometry = Tile->GetCachedGeometry();
				// Ensure the geometry is valid (size > 0)
				if (TileGeometry.GetLocalSize().X > 0)
				{
					const FGeometry& BoardGeometry = GetCachedGeometry();
					const FVector2D TileCenterAbsolute = TileGeometry.GetAbsolutePosition() + (TileGeometry.GetAbsoluteSize() * 0.5f);
					return BoardGeometry.AbsoluteToLocal(TileCenterAbsolute);
				}
			}
		}
	}

	// Fallback: Calculate the total space each cell occupies (tile size + padding).
	const float CellSize = TileSize + TilePadding;
    
	// Calculate the center of the cell in screen coordinates.
	const float ScreenX = GridCoord.X * CellSize + TileSize / 2.0f + TilePadding;
	const float ScreenY = GridCoord.Y * CellSize + TileSize / 2.0f + TilePadding;
    
	return FVector2D(ScreenX, ScreenY);
}

void UOnetBoardWidget::DrawConnectionPath(const TArray<FIntPoint>& Path)
{
	// Clear previous path.
	ActivePathPoints.Empty();

	// Convert grid coordinates to screen coordinates.
	for (const FIntPoint& GridCoord : Path)
	{
		ActivePathPoints.Add(GridToScreenPosition(GridCoord));
	}

	// Start displaying the path.
	bShowPath = true;
	PathStartTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("DrawConnectionPath: %d points"), ActivePathPoints.Num());
}

void UOnetBoardWidget::ClearPath()
{
	bShowPath = false;
	ActivePathPoints.Empty();
}
