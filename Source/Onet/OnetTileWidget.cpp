// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetTileWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

/**
 * Bind the tile's grid coordinates when created.
 */
void UOnetTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bind the UMG button click event to our handler.
	if (TileButton)
	{
		TileButton->OnClicked.AddDynamic(this, &UOnetTileWidget::HandleButtonClicked);
	}
}

/**
 * Initialize the tile with its grid coordinates.
 * @param InX - X coordinate of the tile.
 * @param InY - Y coordinate of the tile.
 */
void UOnetTileWidget::InitializeTile(const int32 InX, const int32 InY)
{
	X = InX;
	Y = InY;
}

/**
 * Set fixed size for the tile to make it square.
 * @param Size - The width and height of the tile in pixels.
 */
void UOnetTileWidget::SetFixedSize(const float Size)
{
	FixedTileSize = Size;

	// Force the widget to use the specified size.
	if (TileButton)
	{
		TileButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
}

/**
 * Broadcast tile clicked event with its coordinates.
 */
void UOnetTileWidget::HandleButtonClicked()
{
	// Emit an event that tells parent widget which tile was clicked.
	OnTileClicked.Broadcast(X, Y);
}

/**
 * Update the tile's visual representation based on its state.
 * @param bIsEmpty - Whether the tile is empty.
 * @param TileTypeId - The type identifier of the tile.
 * @param bIsSelected - Whether the tile is currently selected.
 */
void UOnetTileWidget::SetTileVisual(const bool bIsEmpty, const int32 TileTypeId, bool bIsSelected) const
{
	// Hide empty tiles completely.
	if (bIsEmpty)
	{
		const_cast<UOnetTileWidget*>(this)->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Show non-empty tiles.
	const_cast<UOnetTileWidget*>(this)->SetVisibility(ESlateVisibility::Visible);

	if (TileButton)
	{
		// Disable interaction for empty tiles.
		TileButton->SetIsEnabled(!bIsEmpty);

		// Highlight selected tiles.
		if (!bIsEmpty)
		{
			TileButton->SetBackgroundColor(bIsSelected ? SelectedColor : NormalColor);
		}
	}

	if (LabelText)
	{
		// Show the type id for debugging. Later you replace this with icon images.
		LabelText->SetText(bIsEmpty ? FText::GetEmpty() : FText::AsNumber(TileTypeId));

		// Make selected text more visible.
		if (!bIsEmpty && bIsSelected)
		{
			LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		}
		else
		{
			LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}
