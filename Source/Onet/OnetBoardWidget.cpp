// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetBoardWidget.h"
#include "OnetBoardComponent.h"
#include "OnetTileWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"

void UOnetBoardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ShuffleButton)
	{
		ShuffleButton->OnClicked.AddDynamic(this, &UOnetBoardWidget::HandleShuffleClicked);
	}

	if (WildLinkButton)
	{
		WildLinkButton->OnClicked.AddDynamic(this, &UOnetBoardWidget::HandleWildLinkClicked);
	}

	if (HintButton)
	{
		HintButton->OnClicked.AddDynamic(this, &UOnetBoardWidget::HandleHintClicked);
	}
}

void UOnetBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Keep layout responsive and tiles square.
	UpdateAutoLayout(MyGeometry);

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
	if (bShowPath && ActivePathGridPoints.Num() >= 2)
	{
		FVector2D Origin;
		FVector2D Step;
		if (!ComputeGridMetrics(Origin, Step))
		{
			return Result;
		}

		// Draw lines between consecutive points.
		for (int32 i = 0; i < ActivePathGridPoints.Num() - 1; ++i)
		{
			const FVector2D Start = Origin + FVector2D(ActivePathGridPoints[i].X * Step.X,
			                                           ActivePathGridPoints[i].Y * Step.Y);
			const FVector2D End = Origin + FVector2D(ActivePathGridPoints[i + 1].X * Step.X,
			                                         ActivePathGridPoints[i + 1].Y * Step.Y);

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

	if (CompletionWidget)
	{
		CompletionWidget->RemoveFromParent();
		CompletionWidget = nullptr;
	}

	// Subscribe to board events so UI updates can be event-driven.
	Board->OnBoardChanged.AddDynamic(this, &UOnetBoardWidget::HandleBoardChanged);
	Board->OnSelectionChanged.AddDynamic(this, &UOnetBoardWidget::HandleSelectionChanged);
	Board->OnMatchSuccessful.AddDynamic(this, &UOnetBoardWidget::HandleMatchSuccessful);
	Board->OnMatchFailed.AddDynamic(this, &UOnetBoardWidget::HandleMatchFailed);
	Board->OnShufflePerformed.AddDynamic(this, &UOnetBoardWidget::HandleShuffleUpdated);
	Board->OnHintUpdated.AddDynamic(this, &UOnetBoardWidget::HandleHintUpdated);
	Board->OnWildStateChanged.AddDynamic(this, &UOnetBoardWidget::HandleWildStateChanged);
	Board->OnBoardCleared.AddDynamic(this, &UOnetBoardWidget::HandleBoardCleared);
	Board->OnNoMovesRemain.AddDynamic(this, &UOnetBoardWidget::HandleNoMovesRemain);

	CachedRemainingShuffles = Board->GetRemainingShuffleUses();
	CachedMaxShuffles = Board->GetMaxShuffleUses();
	bWildLinkPrimed = Board->IsWildLinkPrimed();
	FIntPoint ExistingHintA;
	FIntPoint ExistingHintB;
	bHasHintTiles = Board->HasActiveHint(ExistingHintA, ExistingHintB);
	HintTileA = ExistingHintA;
	HintTileB = ExistingHintB;

	RebuildGrid();
	RefreshAllTiles();
	UpdateActionButtons();
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
	GridPanel->SetMinDesiredSlotWidth(TileSize);
	GridPanel->SetMinDesiredSlotHeight(TileSize);

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
				if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(GridPanel->AddChildToUniformGrid(Tile, Y, X)))
				{
					GridSlot->SetHorizontalAlignment(HAlign_Fill);
					GridSlot->SetVerticalAlignment(VAlign_Fill);
				}

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
			const bool bIsHintTile = bHasHintTiles && ((X == HintTileA.X && Y == HintTileA.Y) ||
				(X == HintTileB.X && Y == HintTileB.Y));

			if (UOnetTileWidget* TileWidget = TileWidgets[Y * W + X])
			{
				TileWidget->SetTileVisual(TileData.bEmpty, TileData.TileTypeId, bIsSelected, bIsHintTile);
			}
		}
	}
}

void UOnetBoardWidget::UpdateActionButtons()
{
	if (Board)
	{
		CachedRemainingShuffles = Board->GetRemainingShuffleUses();
		CachedMaxShuffles = Board->GetMaxShuffleUses();
	}

	const int32 DisplayMax = CachedMaxShuffles > 0 ? CachedMaxShuffles : FMath::Max(CachedRemainingShuffles, 0);

	if (ShuffleButton)
	{
		ShuffleButton->SetIsEnabled(Board != nullptr && CachedRemainingShuffles > 0);
	}

	if (ShuffleCountText)
	{
		ShuffleCountText->SetText(FText::Format(
			NSLOCTEXT("Onet", "ShuffleCountLabel", "Shuffle {0}/{1}"),
			FText::AsNumber(CachedRemainingShuffles),
			FText::AsNumber(DisplayMax)));
	}

	if (WildLinkButton)
	{
		WildLinkButton->SetIsEnabled(Board != nullptr && !bWildLinkPrimed);
	}

	if (HintButton)
	{
		HintButton->SetIsEnabled(Board != nullptr);
	}
}

void UOnetBoardWidget::ShowCompletionScreen()
{
	if (!CompletionWidget && CompletionWidgetClass)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			CompletionWidget = CreateWidget<UUserWidget>(PC, CompletionWidgetClass);
			if (CompletionWidget)
			{
				CompletionWidget->AddToViewport(100);
			}
		}
	}

	if (!CompletionWidget && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("关卡完成！"));
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
	UpdateActionButtons();
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

void UOnetBoardWidget::HandleShuffleClicked()
{
	if (Board)
	{
		Board->RequestShuffle();
	}
}

void UOnetBoardWidget::HandleWildLinkClicked()
{
	if (Board)
	{
		Board->ActivateWildLink();
	}
}

void UOnetBoardWidget::HandleHintClicked()
{
	if (Board)
	{
		Board->RequestHint();
	}
}

void UOnetBoardWidget::HandleShuffleUpdated(int32 RemainingUses, bool bAutoTriggered)
{
	CachedRemainingShuffles = RemainingUses;
	if (Board)
	{
		CachedMaxShuffles = Board->GetMaxShuffleUses();
	}
	UpdateActionButtons();

	if (bAutoTriggered && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("A dead end has been detected; the deck has been automatically shuffled."));
	}
}

void UOnetBoardWidget::HandleHintUpdated(bool bHasHint, FIntPoint First, FIntPoint Second)
{
	bHasHintTiles = bHasHint;
	HintTileA = First;
	HintTileB = Second;
	RefreshAllTiles();
}

void UOnetBoardWidget::HandleWildStateChanged(bool bWildReady)
{
	bWildLinkPrimed = bWildReady;
	UpdateActionButtons();
}

void UOnetBoardWidget::HandleBoardCleared()
{
	bHasHintTiles = false;
	bWildLinkPrimed = false;
	RefreshAllTiles();
	UpdateActionButtons();
	ShowCompletionScreen();
}

void UOnetBoardWidget::HandleNoMovesRemain()
{
	UpdateActionButtons();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("No moves available and no more shuffles."));
	}
}

FVector2D UOnetBoardWidget::GridToScreenPosition(const FIntPoint& GridCoord) const
{
	FVector2D Origin;
	FVector2D Step;
	if (ComputeGridMetrics(Origin, Step))
	{
		return Origin + FVector2D(GridCoord.X * Step.X, GridCoord.Y * Step.Y);
	}

	// Fallback: assume uniform spacing based on current TileSize/TilePadding.
	const float CellSize = TileSize + (TilePadding * 2.0f);
	return FVector2D(
		(GridCoord.X * CellSize) + (CellSize * 0.5f),
		(GridCoord.Y * CellSize) + (CellSize * 0.5f));
}

void UOnetBoardWidget::DrawConnectionPath(const TArray<FIntPoint>& Path)
{
	// Clear previous path.
	ActivePathGridPoints = Path;

	// Start displaying the path.
	bShowPath = ActivePathGridPoints.Num() >= 2;
	PathStartTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("DrawConnectionPath: %d points"), ActivePathGridPoints.Num());
}

void UOnetBoardWidget::ClearPath()
{
	bShowPath = false;
	ActivePathGridPoints.Empty();
}

void UOnetBoardWidget::UpdateAutoLayout(const FGeometry& MyGeometry)
{
	if (!GridPanel || !Board)
	{
		return;
	}

	const int32 W = Board->GetBoardWidth();
	const int32 H = Board->GetBoardHeight();
	if (W <= 0 || H <= 0)
	{
		return;
	}

	// Prefer viewport size for responsive scaling; fallback to allotted geometry.
	FVector2D ViewportSize = MyGeometry.GetLocalSize();
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D OutViewportSize;
			ViewportClient->GetViewportSize(OutViewportSize);
			if (OutViewportSize.X > 0 && OutViewportSize.Y > 0)
			{
				ViewportSize = OutViewportSize;
			}
		}
	}

	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	// Reserve a margin and compute tile size that keeps tiles square.
	const float MaxBoardWidth = ViewportSize.X * 0.9f;
	const float MaxBoardHeight = ViewportSize.Y * 0.9f;

	const float PaddingXPerTile = TilePadding * 2.0f;
	const float PaddingYPerTile = TilePadding * 2.0f;

	const float AvailableWidthForTiles = MaxBoardWidth - (PaddingXPerTile * W);
	const float AvailableHeightForTiles = MaxBoardHeight - (PaddingYPerTile * H);

	const float CandidateTileWidth = AvailableWidthForTiles / static_cast<float>(W);
	const float CandidateTileHeight = AvailableHeightForTiles / static_cast<float>(H);
	const float NewTileSize = FMath::Max(4.0f, FMath::Min(CandidateTileWidth, CandidateTileHeight));

	if (!FMath::IsNearlyEqual(NewTileSize, TileSize))
	{
		TileSize = NewTileSize;

		GridPanel->SetSlotPadding(FMargin(TilePadding));
		GridPanel->SetMinDesiredSlotWidth(TileSize);
		GridPanel->SetMinDesiredSlotHeight(TileSize);

		for (UOnetTileWidget* Tile : TileWidgets)
		{
			if (!Tile)
			{
				continue;
			}

			Tile->SetFixedSize(TileSize);

			if (UUniformGridSlot* TileSlot = Cast<UUniformGridSlot>(Tile->Slot))
			{
				TileSlot->SetHorizontalAlignment(HAlign_Fill);
				TileSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		// Resize + center the board so aspect ratio matches the logical grid.
		const float BoardWidthPx = (TileSize + PaddingXPerTile) * W;
		const float BoardHeightPx = (TileSize + PaddingYPerTile) * H;
		SetDesiredSizeInViewport(FVector2D(BoardWidthPx, BoardHeightPx));
		SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		SetAnchorsInViewport(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	}
}

bool UOnetBoardWidget::ComputeGridMetrics(FVector2D& OutOrigin, FVector2D& OutStep) const
{
	OutOrigin = FVector2D::ZeroVector;
	OutStep = FVector2D(TileSize + TilePadding * 2.0f, TileSize + TilePadding * 2.0f);

	if (!Board || TileWidgets.Num() == 0)
	{
		return false;
	}

	const int32 W = Board->GetBoardWidth();
	const int32 H = Board->GetBoardHeight();
	if (W <= 0 || H <= 0)
	{
		return false;
	}

	const FGeometry& BoardGeometry = GetCachedGeometry();
	if (BoardGeometry.GetLocalSize().IsNearlyZero())
	{
		return false;
	}

	// Use the (0,0) tile as origin.
	if (!TileWidgets.IsValidIndex(0) || !TileWidgets[0])
	{
		return false;
	}

	const FGeometry BaseGeometry = TileWidgets[0]->GetCachedGeometry();
	if (BaseGeometry.GetLocalSize().IsNearlyZero())
	{
		return false;
	}

	const FVector2D BaseCenterAbsolute = BaseGeometry.GetAbsolutePosition() + (BaseGeometry.GetAbsoluteSize() * 0.5f);
	OutOrigin = BoardGeometry.AbsoluteToLocal(BaseCenterAbsolute);

	// Measure step in X using neighbor (1,0) if available.
	if (W > 1 && TileWidgets.IsValidIndex(1) && TileWidgets[1])
	{
		const FGeometry NeighborGeo = TileWidgets[1]->GetCachedGeometry();
		if (!NeighborGeo.GetLocalSize().IsNearlyZero())
		{
			const FVector2D NeighborCenterAbsolute = NeighborGeo.GetAbsolutePosition() + (NeighborGeo.GetAbsoluteSize()
				* 0.5f);
			const FVector2D NeighborCenterLocal = BoardGeometry.AbsoluteToLocal(NeighborCenterAbsolute);
			OutStep.X = NeighborCenterLocal.X - OutOrigin.X;
		}
	}

	// Measure step in Y using neighbor (0,1) if available.
	if (H > 1 && TileWidgets.IsValidIndex(W) && TileWidgets[W])
	{
		const FGeometry NeighborGeo = TileWidgets[W]->GetCachedGeometry();
		if (!NeighborGeo.GetLocalSize().IsNearlyZero())
		{
			const FVector2D NeighborCenterAbsolute = NeighborGeo.GetAbsolutePosition() + (NeighborGeo.GetAbsoluteSize()
				* 0.5f);
			const FVector2D NeighborCenterLocal = BoardGeometry.AbsoluteToLocal(NeighborCenterAbsolute);
			OutStep.Y = NeighborCenterLocal.Y - OutOrigin.Y;
		}
	}

	OutStep.X = FMath::Max(OutStep.X, 1.0f);
	OutStep.Y = FMath::Max(OutStep.Y, 1.0f);
	return true;
}
