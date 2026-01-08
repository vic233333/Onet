// Copyright 2026 Xinchen Shen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnetBoardComponent.h"
#include "Blueprint/UserWidget.h"
#include "OnetBoardWidget.generated.h"

class UUniformGridPanel;
class UUniformGridComponent;
class UOnetTileWidget;
class UButton;
class UTextBlock;
class UUserWidget;

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

	// Convert grid coordinate to screen position (top-left of tile).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Onet|UI")
	FVector2D GridToScreenPosition(const FIntPoint& GridCoord) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Override to handle mouse button down events for background clicks.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Size of each tile (in pixels). Tiles will be square.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float TileSize = 80.0f;

	// Padding between tiles (in pixels).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float TilePadding = 4.0f;

	// Color of the connection line.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	FLinearColor PathColor = FLinearColor::Green;

	// Thickness of the connection line.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float PathThickness = 4.0f;

	// How long the path is displayed (in seconds).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Onet|UI")
	float PathDisplayDuration = 0.5f;

private:
	// BindWidget requires a UniformGridPanel named exactly "GridPanel" in WBP_OnetBoard.
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUniformGridPanel> GridPanel;

	// Optional action buttons (placed in WBP_OnetBoard).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ShuffleButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> WildLinkButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HintButton;

	// Optional text to show shuffle uses.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShuffleCountText;

	// Optional completion screen class.
	UPROPERTY(EditDefaultsOnly, Category = "Onet|UI")
	TSubclassOf<UUserWidget> CompletionWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> CompletionWidget;

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

	// Hint highlight state.
	bool bHasHintTiles = false;
	FIntPoint HintTileA = FIntPoint(-1, -1);
	FIntPoint HintTileB = FIntPoint(-1, -1);

	// Cached action state.
	int32 CachedRemainingShuffles = 0;
	int32 CachedMaxShuffles = 0;
	bool bWildLinkPrimed = false;

	// Path drawing state.
	bool bShowPath = false;
	TArray<FIntPoint> ActivePathGridPoints;
	float PathStartTime = 0.0f;

private:
	void RefreshAllTiles();
	void UpdateActionButtons();
	void ShowCompletionScreen();

	// Draw the connection path (called from C++, not Blueprint).
	void DrawConnectionPath(const TArray<FIntPoint>& Path);

	// Clear the path after display duration.
	void ClearPath();

	// Recalculate slot sizes / desired board size to keep tiles square and fit the viewport.
	void UpdateAutoLayout(const FGeometry& MyGeometry);

	// Derive origin and per-cell step in local space (supports outer padding coords).
	bool ComputeGridMetrics(FVector2D& OutOrigin, FVector2D& OutStep) const;

	UFUNCTION()
	void HandleBoardChanged();

	UFUNCTION()
	void HandleSelectionChanged(const bool bHasFirstSelection, const FIntPoint FirstSelection);

	UFUNCTION()
	void HandleMatchSuccessful(const TArray<FIntPoint>& Path);

	UFUNCTION()
	void HandleMatchFailed();

	UFUNCTION()
	void HandleShuffleClicked();

	UFUNCTION()
	void HandleWildLinkClicked();

	UFUNCTION()
	void HandleHintClicked();

	UFUNCTION()
	void HandleShuffleUpdated(int32 RemainingUses, bool bAutoTriggered);

	UFUNCTION()
	void HandleHintUpdated(bool bHasHint, FIntPoint First, FIntPoint Second);

	UFUNCTION()
	void HandleWildStateChanged(bool bWildReady);

	UFUNCTION()
	void HandleBoardCleared();

	UFUNCTION()
	void HandleNoMovesRemain();
};
