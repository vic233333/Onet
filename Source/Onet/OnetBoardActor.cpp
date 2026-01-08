// Copyright 2026 Xinchen Shen. All Rights Reserved.


#include "OnetBoardActor.h"
#include "OnetBoardComponent.h"
#include "Components/SceneComponent.h"

AOnetBoardActor::AOnetBoardActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// The actor does not need to tick every frame since it will listen to events.

	// Always provide a root component for Actor transforms and editor stability.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); // Create a scene component to serve as the root.
	SetRootComponent(Root);

	UE_LOG(LogTemp, Display, TEXT("AOnetBoardActor: Creating BoardComponent."));
	// Log when creating the board component.
	BoardComponent = CreateDefaultSubobject<UOnetBoardComponent>(TEXT("BoardComponent"));
}

// Called when the game starts or when spawned
// void AOnetBoardActor::BeginPlay()
// {
// 	Super::BeginPlay();
// 	
// }
//
// // Called every frame
// void AOnetBoardActor::Tick(float DeltaTime)
// {
// 	Super::Tick(DeltaTime);
//
// }
