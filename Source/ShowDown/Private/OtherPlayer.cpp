// Fill out your copyright notice in the Description page of Project Settings.


#include "OtherPlayer.h"


// Sets default values
AOtherPlayer::AOtherPlayer()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AOtherPlayer::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AOtherPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

