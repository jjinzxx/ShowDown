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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		PlateMeshAsset = CubeMeshFinder.Object;
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
		UStaticMeshComponent* PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("LanePlate_%d"), LaneIndex));
		PlateMesh->SetupAttachment(Root);
		PlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlateMesh->SetCollisionObjectType(ECC_WorldDynamic);
		PlateMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		PlateMesh->SetCanEverAffectNavigation(false);
		PlateMesh->SetCastShadow(false);
		if (PlateMeshAsset)
		{
			PlateMesh->SetStaticMesh(PlateMeshAsset);
		}
		if (BulletMaterial)
		{
			PlateMesh->SetMaterial(0, BulletMaterial);
		}
		LanePlateMeshes.Add(PlateMesh);

		UStaticMeshComponent* AccentMesh = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("LaneAccent_%d"), LaneIndex));
		AccentMesh->SetupAttachment(Root);
		AccentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AccentMesh->SetCollisionObjectType(ECC_WorldDynamic);
		AccentMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		AccentMesh->SetCanEverAffectNavigation(false);
		AccentMesh->SetCastShadow(false);
		if (PlateMeshAsset)
		{
			AccentMesh->SetStaticMesh(PlateMeshAsset);
		}
		if (BulletMaterial)
		{
			AccentMesh->SetMaterial(0, BulletMaterial);
		}
		LaneAccentMeshes.Add(AccentMesh);

		UTextRenderComponent* StatusText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneStatusText_%d"), LaneIndex));
		StatusText->SetupAttachment(Root);
		SetTextComponentDefaults(StatusText, 5.2f);
		LaneStatusTexts.Add(StatusText);

		UTextRenderComponent* NameText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneNameText_%d"), LaneIndex));
		NameText->SetupAttachment(Root);
		SetTextComponentDefaults(NameText, 4.2f);
		LaneNameTexts.Add(NameText);

		UTextRenderComponent* ActionText = CreateDefaultSubobject<UTextRenderComponent>(
			*FString::Printf(TEXT("LaneActionText_%d"), LaneIndex));
		ActionText->SetupAttachment(Root);
		SetTextComponentDefaults(ActionText, 4.6f);
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
			BulletMesh->SetCastShadow(false);
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
		HeaderText->SetVisibility(false);
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
	const FLinearColor PlateColor = LaneState->bFolded
		? FLinearColor(0.035f, 0.037f, 0.042f, 1.0f)
		: FLinearColor(0.012f, 0.015f, 0.021f, 1.0f);
	const FVector BaseLocation = LaneState->WorldLocation;
	const FRotator BaseRotation = LaneState->WorldRotation;
	const FVector ForwardDirection = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(BaseRotation).GetUnitAxis(EAxis::Y);
	const int32 ActiveBullets = FMath::Clamp(
		LaneState->bRouletteTarget ? LaneState->RouletteBulletCount : LaneState->BulletCount,
		0,
		MaxBulletsPerLane);
	const FString BuiltStatusText = BuildLaneStatusText(*LaneState, bLocalLane);
	const FString BuiltActionText = BuildLaneActionText(*LaneState);
	const float PlateWidth = FMath::Clamp(45.0f + static_cast<float>(ActiveBullets) * 3.6f, 48.0f, 68.0f);
	const float PlateHeight = BuiltActionText.IsEmpty() ? 13.5f : 18.0f;
	const FVector TextLift = ForwardDirection * 1.4f;

	if (LanePlateMeshes.IsValidIndex(LaneIndex) && LanePlateMeshes[LaneIndex])
	{
		UStaticMeshComponent* PlateMesh = LanePlateMeshes[LaneIndex];
		PlateMesh->SetWorldLocationAndRotation(BaseLocation, BaseRotation);
		PlateMesh->SetWorldScale3D(FVector(0.010f, PlateWidth / 100.0f, PlateHeight / 100.0f));
		ApplyBulletColor(PlateMesh, PlateColor);
	}

	if (LaneAccentMeshes.IsValidIndex(LaneIndex) && LaneAccentMeshes[LaneIndex])
	{
		UStaticMeshComponent* AccentMesh = LaneAccentMeshes[LaneIndex];
		const FVector AccentLocation =
			BaseLocation
			+ FVector::UpVector * (PlateHeight * 0.5f - 0.8f)
			+ ForwardDirection * 0.8f;
		AccentMesh->SetWorldLocationAndRotation(AccentLocation, BaseRotation);
		AccentMesh->SetWorldScale3D(FVector(0.012f, PlateWidth / 100.0f, 0.012f));
		ApplyBulletColor(AccentMesh, BaseColor);
	}

	if (LaneStatusTexts.IsValidIndex(LaneIndex) && LaneStatusTexts[LaneIndex])
	{
		UTextRenderComponent* StatusText = LaneStatusTexts[LaneIndex];
		StatusText->SetText(FText::FromString(BuiltStatusText));
		StatusText->SetVisibility(!BuiltStatusText.IsEmpty());
		StatusText->SetTextRenderColor(MakeTextColor(BaseColor));
		StatusText->SetWorldLocationAndRotation(
			BaseLocation + FVector::UpVector * LaneStatusHeight + TextLift,
			BaseRotation);
	}

	if (LaneNameTexts.IsValidIndex(LaneIndex) && LaneNameTexts[LaneIndex])
	{
		UTextRenderComponent* NameText = LaneNameTexts[LaneIndex];
		const FString DisplayName = LaneState->DisplayName.TrimStartAndEnd().Left(14);
		NameText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %d/%d"),
			*DisplayName,
			FMath::Clamp(LaneState->BulletCount, 0, MaxBulletsPerLane),
			FMath::Clamp(PresentationState.TableBet, 0, MaxBulletsPerLane))));
		NameText->SetTextRenderColor(MakeTextColor(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f)));
		NameText->SetWorldLocationAndRotation(
			BaseLocation + FVector::UpVector * LaneNameHeight + TextLift,
			BaseRotation);
	}

	if (LaneActionTexts.IsValidIndex(LaneIndex) && LaneActionTexts[LaneIndex])
	{
		UTextRenderComponent* ActionText = LaneActionTexts[LaneIndex];
		ActionText->SetText(FText::FromString(BuiltActionText));
		ActionText->SetVisibility(!BuiltActionText.IsEmpty());
		ActionText->SetTextRenderColor(MakeTextColor(LaneState->bRouletteTarget
			? FLinearColor(1.0f, 0.22f, 0.12f, 1.0f)
			: FLinearColor(1.0f, 0.86f, 0.48f, 1.0f)));
		ActionText->SetWorldLocationAndRotation(
			BaseLocation + FVector::UpVector * LaneActionHeight + TextLift,
			BaseRotation);
	}

	if (LastSeenActionRevisions.IsValidIndex(LaneIndex)
		&& LaneActionPulseRemaining.IsValidIndex(LaneIndex)
		&& LaneState->ActionRevision != LastSeenActionRevisions[LaneIndex])
	{
		LastSeenActionRevisions[LaneIndex] = LaneState->ActionRevision;
		LaneActionPulseRemaining[LaneIndex] = ActionPulseSeconds;
		SetActorTickEnabled(true);
	}

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
			+ ForwardDirection * 2.2f
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
	if (LanePlateMeshes.IsValidIndex(LaneIndex) && LanePlateMeshes[LaneIndex])
	{
		LanePlateMeshes[LaneIndex]->SetVisibility(bVisible);
	}
	if (LaneAccentMeshes.IsValidIndex(LaneIndex) && LaneAccentMeshes[LaneIndex])
	{
		LaneAccentMeshes[LaneIndex]->SetVisibility(bVisible);
	}
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
			? FString::Printf(TEXT("YOUR TARGET %d"), LaneState.RouletteBulletCount)
			: FString::Printf(TEXT("TARGET %d"), LaneState.RouletteBulletCount);
	}

	if (LaneState.bFolded)
	{
		return TEXT("FOLD");
	}

	if (LaneState.bCurrentTurn)
	{
		return bLocalLane ? TEXT("YOUR TURN") : TEXT("TURN");
	}

	return FString();
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
