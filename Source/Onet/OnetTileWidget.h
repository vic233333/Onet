// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnetTileWidget.generated.h"

class UButton;
class UTextBlock;

// Notify listeners when a tile is clicked; carries grid coordinates.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnetTileClicked, int32, X, int32, Y);

/**
 * A single tile widget (View element).
 * It does NOT decide game rules.
 * It only:
 *  - display the tile data (empty/type/selected)
 *  - emits click events with its coordinates
 */
UCLASS()
class ONET_API UOnetTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called after widget creation to assign its grid coordinate.
	void InitializeTile(const int32 InX, const int32 InY);

	// Set fixed size for the tile to make it square.
	void SetFixedSize(float Size);

	// Update visuals based on board data.
	UFUNCTION(BlueprintCallable, Category = "Onet|Tile")
	void SetTileVisual(bool bIsEmpty, int32 TileTypeId, bool bIsSelected) const;

	// UI event for parent widget to subscribe to.
	UPROPERTY(BlueprintAssignable, Category="Onet|Events")
	FOnetTileClicked OnTileClicked;

	// Bind native events after the widget is constructed.
	virtual void NativeOnInitialized() override;

private:
	float FixedTileSize = 80.0f;
	
protected:
	/**
	 * BindWidget means:
	 *  - In the Widget Blueprint (WBP_OnetTile), you must have a UButton named "TileButton".
	 *  - UE will auto-assign the UButton pointer to this member when the widget is created.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> TileButton;
	
	// Label for debugging (show tileTypedId as a number).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	// Colors for tile states (can be customized in Blueprint).
	UPROPERTY(EditDefaultsOnly, Category="Onet|Tile")
	FLinearColor NormalColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category="Onet|Tile")
	FLinearColor SelectedColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); // Yellow highlight

	int32 X = -1;
	int32 Y = -1;
	
	UFUNCTION()
	void HandleButtonClicked();
};
