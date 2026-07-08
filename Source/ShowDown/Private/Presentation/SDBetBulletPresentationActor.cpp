#include "Presentation/SDBetBulletPresentationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FColor MakeTextColor(const FLinearColor& Color)
	{
		return Color.ToFColor(true);
	}

	FLinearColor GetLaneBaseColor(const FSDBetBulletLaneState& LaneState)
	{
		if (LaneState.bRouletteTarget)
		{
			return FLinearColor(1.0f, 0.13f, 0.08f, 1.0f);
		}

		if (LaneState.bCurrentTurn)
		{
			return FLinearColor(0.05f, 0.85f, 1.0f, 1.0f);
		}

		if (LaneState.bFolded)
		{
			return FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);
		}

		return FLinearColor(1.0f, 0.72f, 0.18f, 1.0f);
	}

	FString GetActionText(EShowDownBetAction Action)
	{
		switch (Action)
		{
		case EShowDownBetAction::Check:
			return TEXT("체크");
		case EShowDownBetAction::Call:
			return TEXT("콜");
		case EShowDownBetAction::Raise:
			return TEXT("레이즈");
		case EShowDownBetAction::Fold:
			return TEXT("폴드");
		default:
			return TEXT("");
		}
	}
}

ASDBetBulletPresentationActor::ASDBetBulletPresentationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(12.0f);
	SetMinNetUpdateFrequency(4.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BulletMeshAsset = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BulletMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BulletMaterialFinder.Succeeded())
	{
		BulletMaterial = BulletMaterialFinder.Object;
	}

	HeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderText"));
	HeaderText->SetupAttachment(Root);
	SetTextComponentDefaults(HeaderText, 18.0f);

	for (int32 LaneIndex = 0; LaneIndex < MaxLanes; ++LaneIndex)
	{
		UTextRenderComponent* StatusText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneStatusText_%d"), LaneIndex));
		StatusText->SetupAttachment(Root);
		SetTextComponentDefaults(StatusText, 17.0f);
		LaneStatusTexts.Add(StatusText);

		UTextRenderComponent* NameText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneNameText_%d"), LaneIndex));
		NameText->SetupAttachment(Root);
		SetTextComponentDefaults(NameText, 15.0f);
		LaneNameTexts.Add(NameText);

		UTextRenderComponent* ActionText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneActionText_%d"), LaneIndex));
		ActionText->SetupAttachment(Root);
		SetTextComponentDefaults(ActionText, 14.0f);
		LaneActionTexts.Add(ActionText);

		for (int32 BulletIndex = 0; BulletIndex < MaxBulletsPerLane; ++BulletIndex)
		{
			UStaticMeshComponent* BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(
				*FString::Printf(TEXT("LaneBullet_%d_%d"), LaneIndex, BulletIndex));
			BulletMesh->SetupAttachment(Root);
			BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BulletMesh->SetCollisionObjectType(ECC_WorldDynamic);
			BulletMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			BulletMesh->SetCanEverAffectNavigation(false);
			if (BulletMeshAsset)
			{
				BulletMesh->SetStaticMesh(BulletMeshAsset);
			}
			if (BulletMaterial)
			{
				BulletMesh->SetMaterial(0, BulletMaterial);
			}
			BulletMeshes.Add(BulletMesh);
		}
	}

	LastSeenActionRevisions.Init(0, MaxLanes);
	LaneActionPulseRemaining.Init(0.0f, MaxLanes);
}

void ASDBetBulletPresentationActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisuals();
}

void ASDBetBulletPresentationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bNeedsTick = false;
	for (int32 LaneIndex = 0; LaneIndex < MaxLanes; ++LaneIndex)
	{
		if (!LaneActionPulseRemaining.IsValidIndex(LaneIndex)
			|| LaneActionPulseRemaining[LaneIndex] <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		LaneActionPulseRemaining[LaneIndex] = FMath::Max(0.0f, LaneActionPulseRemaining[LaneIndex] - DeltaSeconds);
		bNeedsTick = bNeedsTick || LaneActionPulseRemaining[LaneIndex] > KINDA_SMALL_NUMBER;
		if (!PresentationState.Lanes.IsValidIndex(LaneIndex))
		{
			continue;
		}

		const float Alpha = LaneActionPulseRemaining[LaneIndex] / FMath::Max(0.01f, ActionPulseSeconds);
		const float Pulse = 1.0f + FMath::Sin(Alpha * PI * 3.0f) * 0.12f * Alpha;
		for (int32 BulletIndex = 0; BulletIndex < MaxBulletsPerLane; ++BulletIndex)
		{
			const int32 MeshIndex = GetBulletMeshIndex(LaneIndex, BulletIndex);
			if (BulletMeshes.IsValidIndex(MeshIndex) && BulletMeshes[MeshIndex] && BulletMeshes[MeshIndex]->IsVisible())
			{
				BulletMeshes[MeshIndex]->SetWorldScale3D(BulletScale * Pulse);
			}
		}
	}

	SetActorTickEnabled(bNeedsTick);
}

void ASDBetBulletPresentationActor::SetPresentationState(const FSDBetBulletPresentationState& NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	PresentationState = NewState;
	RefreshVisuals();
	ForceNetUpdate();
}

void ASDBetBulletPresentationActor::OnRep_PresentationState()
{
	RefreshVisuals();
}

void ASDBetBulletPresentationActor::RefreshVisuals()
{
	const bool bHasAnyLane = PresentationState.Lanes.Num() > 0;
	SetActorHiddenInGame(!bHasAnyLane);
	SetActorTickEnabled(false);

	if (HeaderText)
	{
		HeaderText->SetVisibility(bHasAnyLane);
		HeaderText->SetText(FText::FromString(PresentationState.StatusText));
		HeaderText->SetTextRenderColor(MakeTextColor(FLinearColor(1.0f, 0.9f, 0.68f, 1.0f)));

		if (bHasAnyLane)
		{
			FVector HeaderLocation = FVector::ZeroVector;
			for (const FSDBetBulletLaneState& LaneState : PresentationState.Lanes)
			{
				HeaderLocation += LaneState.WorldLocation;
			}
			HeaderLocation /= static_cast<float>(PresentationState.Lanes.Num());
			HeaderLocation.Z += HeaderHeight;

			const FRotator HeaderRotation = PresentationState.Lanes[0].WorldRotation;
			HeaderText->SetWorldLocationAndRotation(HeaderLocation, HeaderRotation);
		}
	}

	for (int32 LaneIndex = 0; LaneIndex < MaxLanes; ++LaneIndex)
	{
		const FSDBetBulletLaneState* LaneState = PresentationState.Lanes.IsValidIndex(LaneIndex)
			? &PresentationState.Lanes[LaneIndex]
			: nullptr;
		RefreshLaneVisual(LaneIndex, LaneState);
	}
}

void ASDBetBulletPresentationActor::RefreshLaneVisual(int32 LaneIndex, const FSDBetBulletLaneState* LaneState)
{
	if (!LaneState)
	{
		SetLaneComponentsVisible(LaneIndex, false);
		return;
	}

	SetLaneComponentsVisible(LaneIndex, true);

	const bool bLocalLane = IsLocalPlayerLane(*LaneState);
	const FLinearColor BaseColor = GetLaneBaseColor(*LaneState);
	const FColor TextColor = MakeTextColor(BaseColor);
	const FVector BaseLocation = LaneState->WorldLocation;
	const FRotator BaseRotation = LaneState->WorldRotation;
	const FVector RightDirection = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::Y);

	if (LaneStatusTexts.IsValidIndex(LaneIndex) && LaneStatusTexts[LaneIndex])
	{
		UTextRenderComponent* StatusText = LaneStatusTexts[LaneIndex];
		StatusText->SetText(FText::FromString(BuildLaneStatusText(*LaneState, bLocalLane)));
		StatusText->SetTextRenderColor(TextColor);
		StatusText->SetWorldLocationAndRotation(BaseLocation + FVector::UpVector * LaneStatusHeight, BaseRotation);
	}

	if (LaneNameTexts.IsValidIndex(LaneIndex) && LaneNameTexts[LaneIndex])
	{
		UTextRenderComponent* NameText = LaneNameTexts[LaneIndex];
		NameText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  BET %d"),
			*LaneState->DisplayName,
			FMath::Clamp(LaneState->BulletCount, 0, MaxBulletsPerLane))));
		NameText->SetTextRenderColor(TextColor);
		NameText->SetWorldLocationAndRotation(BaseLocation + FVector::UpVector * LaneNameHeight, BaseRotation);
	}

	if (LaneActionTexts.IsValidIndex(LaneIndex) && LaneActionTexts[LaneIndex])
	{
		UTextRenderComponent* ActionText = LaneActionTexts[LaneIndex];
		const FString BuiltActionText = BuildLaneActionText(*LaneState);
		ActionText->SetText(FText::FromString(BuiltActionText));
		ActionText->SetVisibility(!BuiltActionText.IsEmpty());
		ActionText->SetTextRenderColor(MakeTextColor(LaneState->bRouletteTarget
			? FLinearColor(1.0f, 0.05f, 0.02f, 1.0f)
			: FLinearColor(1.0f, 0.92f, 0.55f, 1.0f)));
		ActionText->SetWorldLocationAndRotation(BaseLocation + FVector::UpVector * LaneActionHeight, BaseRotation);
	}

	if (LastSeenActionRevisions.IsValidIndex(LaneIndex)
		&& LaneActionPulseRemaining.IsValidIndex(LaneIndex)
		&& LaneState->ActionRevision != LastSeenActionRevisions[LaneIndex])
	{
		LastSeenActionRevisions[LaneIndex] = LaneState->ActionRevision;
		LaneActionPulseRemaining[LaneIndex] = ActionPulseSeconds;
		SetActorTickEnabled(true);
	}

	const int32 ActiveBullets = FMath::Clamp(LaneState->BulletCount, 0, MaxBulletsPerLane);
	for (int32 BulletIndex = 0; BulletIndex < MaxBulletsPerLane; ++BulletIndex)
	{
		const int32 MeshIndex = GetBulletMeshIndex(LaneIndex, BulletIndex);
		if (!BulletMeshes.IsValidIndex(MeshIndex) || !BulletMeshes[MeshIndex])
		{
			continue;
		}

		UStaticMeshComponent* BulletMesh = BulletMeshes[MeshIndex];
		const bool bVisible = BulletIndex < ActiveBullets;
		BulletMesh->SetVisibility(bVisible);
		if (!bVisible)
		{
			continue;
		}

		const float CenteredIndex = static_cast<float>(BulletIndex) - (static_cast<float>(ActiveBullets) - 1.0f) * 0.5f;
		const FVector BulletLocation =
			BaseLocation
			+ FVector::UpVector * LaneBulletHeight
			+ RightDirection * BulletSpacing * CenteredIndex;
		BulletMesh->SetWorldLocationAndRotation(BulletLocation, BaseRotation);
		BulletMesh->SetWorldScale3D(BulletScale * (LaneState->bCurrentTurn ? CurrentTurnPulseScale : 1.0f));
		ApplyBulletColor(BulletMesh, BaseColor);
	}
}

void ASDBetBulletPresentationActor::SetTextComponentDefaults(UTextRenderComponent* TextComponent, float WorldSize) const
{
	if (!TextComponent)
	{
		return;
	}

	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetTextRenderColor(FColor::White);
	TextComponent->SetWorldSize(WorldSize);
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComponent->SetCanEverAffectNavigation(false);
}

void ASDBetBulletPresentationActor::ApplyBulletColor(UStaticMeshComponent* BulletMesh, const FLinearColor& Color) const
{
	if (!BulletMesh)
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = BulletMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!DynamicMaterial)
	{
		return;
	}

	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
}

void ASDBetBulletPresentationActor::SetLaneComponentsVisible(int32 LaneIndex, bool bVisible)
{
	if (LaneStatusTexts.IsValidIndex(LaneIndex) && LaneStatusTexts[LaneIndex])
	{
		LaneStatusTexts[LaneIndex]->SetVisibility(bVisible);
	}
	if (LaneNameTexts.IsValidIndex(LaneIndex) && LaneNameTexts[LaneIndex])
	{
		LaneNameTexts[LaneIndex]->SetVisibility(bVisible);
	}
	if (LaneActionTexts.IsValidIndex(LaneIndex) && LaneActionTexts[LaneIndex])
	{
		LaneActionTexts[LaneIndex]->SetVisibility(bVisible);
	}

	for (int32 BulletIndex = 0; BulletIndex < MaxBulletsPerLane; ++BulletIndex)
	{
		const int32 MeshIndex = GetBulletMeshIndex(LaneIndex, BulletIndex);
		if (BulletMeshes.IsValidIndex(MeshIndex) && BulletMeshes[MeshIndex])
		{
			BulletMeshes[MeshIndex]->SetVisibility(bVisible);
		}
	}
}

FString ASDBetBulletPresentationActor::BuildLaneStatusText(
	const FSDBetBulletLaneState& LaneState,
	bool bLocalLane) const
{
	if (LaneState.bRouletteTarget)
	{
		return bLocalLane
			? FString::Printf(TEXT("MY TARGET  %d"), LaneState.RouletteBulletCount)
			: FString::Printf(TEXT("TARGET  %d"), LaneState.RouletteBulletCount);
	}

	if (LaneState.bFolded)
	{
		return TEXT("FOLD");
	}

	if (LaneState.bCurrentTurn)
	{
		return bLocalLane ? TEXT("MY TURN") : TEXT("TURN");
	}

	return FString::Printf(TEXT("BET %d"), PresentationState.TableBet);
}

FString ASDBetBulletPresentationActor::BuildLaneActionText(const FSDBetBulletLaneState& LaneState) const
{
	if (LaneState.bRouletteTarget)
	{
		return FString::Printf(TEXT("LOAD %d"), FMath::Clamp(LaneState.RouletteBulletCount, 0, MaxBulletsPerLane));
	}

	return LaneState.ActionText;
}

bool ASDBetBulletPresentationActor::IsLocalPlayerLane(const FSDBetBulletLaneState& LaneState) const
{
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const ASDPlayerState* LocalPlayerState = LocalPlayerController
		? LocalPlayerController->GetPlayerState<ASDPlayerState>()
		: nullptr;
	const EShowDownPlayerSlot LocalSlot = LocalPlayerState
		? LocalPlayerState->ShowDownSlot
		: EShowDownPlayerSlot::Player1;

	return LaneState.Slot != EShowDownPlayerSlot::None && LaneState.Slot == LocalSlot;
}

int32 ASDBetBulletPresentationActor::GetBulletMeshIndex(int32 LaneIndex, int32 BulletIndex) const
{
	return LaneIndex * MaxBulletsPerLane + BulletIndex;
}

void ASDBetBulletPresentationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASDBetBulletPresentationActor, PresentationState);
}
