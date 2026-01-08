// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnetBoardComponent.h"
#include "Blueprint/UserWidget.h"
#include "OnetBoardWidget.generated.h"

class UUniformGridPanel;
class UUniformGridComponent;
class UOnetTileWidget;

/**
 * 
 */
UCLASS()
class ONET_API UOnetBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Inject board logic component into this UI.
	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void InitializeWithBoard(UOnetBoardComponent* InBoard);

	UFUNCTION(BlueprintCallable, Category = "Onet|Board")
	void RebuildGrid();

protected:
	virtual void NativeOnInitialized() override;

	// Override to handle mouse button down events for background clicks.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Size of each tile (in pixels). Tiles will be square.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float TileSize = 80.0f;

	// Padding between tiles (in pixels).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float TilePadding = 4.0f;

private:
	// BindWidget requires a UniformGridPanel named exactly "GridPanel" in WBP_OnetBoard.
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUniformGridPanel> GridPanel;

	// Tile widget class to instantiate per cell (assigned in WBP_OnetBoard).
	UPROPERTY(EditDefaultsOnly, Category="Onet|UI")
	TSubclassOf<UOnetTileWidget> TileWidgetClass;

	// Logic reference.
	UPROPERTY()
	TObjectPtr<UOnetBoardComponent> Board;

	// Cached tile widgets (size = Width * Height).
	UPROPERTY()
	TArray<TObjectPtr<UOnetTileWidget>> TileWidgets;

	// Cached selection state form board events.
	int32 SelectedX = -1;
	int32 SelectedY = -1;
	bool bHasSelection = false;

protected:
	// Blueprint implementable event for drawing the connection path.
	// This allows designers to implement custom path drawing and animation in Blueprint.
	UFUNCTION(BlueprintImplementableEvent, Category = "Onet|UI")
	void DrawConnectionPath(const TArray<FIntPoint>& Path);

	// Blueprint implementable event for showing match failed feedback.
	UFUNCTION(BlueprintImplementableEvent, Category = "Onet|UI")
	void ShowMatchFailedFeedback();

private:
	void RefreshAllTiles();

	UFUNCTION()
	void HandleBoardChanged();

	UFUNCTION()
	void HandleSelectionChanged(const bool bHasFirstSelection, const FIntPoint FirstSelection);

	UFUNCTION()
	void HandleMatchSuccessful(const TArray<FIntPoint>& Path);

	UFUNCTION()
	void HandleMatchFailed();
};
