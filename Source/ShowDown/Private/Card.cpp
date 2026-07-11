#include "Card.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "ShowDownPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ACard::ACard()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);
	NetPriority = 2.0f;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComp);

	CardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh"));
	CardMesh->SetupAttachment(VisualRoot);
	CardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CardMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CardMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// A native fallback keeps multiplayer cards visible even without a BP_Card
	// override. The cube is flattened into an upright playing-card shape.
	if (CardMeshAsset.Succeeded())
	{
		CardMesh->SetStaticMesh(CardMeshAsset.Object);
		CardMesh->SetRelativeScale3D(FVector(0.03f, 0.45f, 0.65f));
	}


	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(RootComp);
	InteractionBounds->SetBoxExtent(InteractionBoundsExtent);
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	CardText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CardText"));
	CardText->SetupAttachment(VisualRoot);
	CardText->SetHorizontalAlignment(EHTA_Center);
	CardText->SetVerticalAlignment(EVRTA_TextCenter);
	CardText->SetTextRenderColor(FColor::Black);
	CardText->SetWorldSize(30.0f);
	CardText->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
}

void ACard::ConfigureInteractionComponents()
{
	if (CardMesh)
	{
		CardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		if (VisualRoot && CardMesh->GetAttachParent() != VisualRoot)
		{
			CardMesh->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	if (CardText && VisualRoot && CardText->GetAttachParent() != VisualRoot)
	{
		CardText->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (InteractionBounds)
	{
		InteractionBounds->SetBoxExtent(InteractionBoundsExtent);
		InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
		InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		if (RootComp && InteractionBounds->GetAttachParent() != RootComp)
		{
			InteractionBounds->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void ACard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureInteractionComponents();
}

void ACard::BeginPlay()
{
	Super::BeginPlay();

	ConfigureInteractionComponents();
	if (CardText)
	{
		BaseCardTextRelativeRotation = CardText->GetRelativeRotation().Quaternion();
	}
	DefaultLocation = GetActorLocation();
	TargetLocation = DefaultLocation;
	CurrentVisualWorldOffset = FVector::ZeroVector;
	TargetVisualWorldOffset = FVector::ZeroVector;
	CurrentLocalViewerOrientationAlpha = 0.0f;
	TargetLocalViewerOrientationAlpha = 0.0f;
	DefaultRotation = GetActorRotation();
	TargetRotation = DefaultRotation;
	if (HasAuthority())
	{
		ReplicatedMovementTarget.Location = DefaultLocation;
		ReplicatedMovementTarget.Rotation = DefaultRotation;
		ReplicatedMovementTarget.MotionDuration = SlotAttachDuration;
		ReplicatedMovementTarget.ArcHeight = SlotAttachArcHeight;
		ReplicatedMovementTarget.OvershootDistance = SlotAttachOvershootDistance;
		ReplicatedMovementTarget.bUseSettleMotion = true;
		ReplicatedMovementTarget.bOrientToLocalViewer = false;
		ReplicatedMovementTarget.VisualScaleMultiplier = TargetVisualScaleMultiplier;
		ReplicatedMovementTarget.ServerStartTime = GetWorld() && GetWorld()->GetGameState()
			? GetWorld()->GetGameState()->GetServerWorldTimeSeconds()
			: -1.0f;
	}
	if (!HasAuthority() && ReplicatedMovementTarget.Revision != 0)
	{
		TargetVisualScaleMultiplier = FMath::Max(0.1f, ReplicatedMovementTarget.VisualScaleMultiplier);
	}
	if (VisualRoot)
	{
		BaseVisualRootScale = VisualRoot->GetRelativeScale3D();
		CurrentVisualScaleMultiplier = TargetVisualScaleMultiplier;
		VisualScaleStartMultiplier = TargetVisualScaleMultiplier;
		bVisualScaleMotionActive = false;
		VisualScaleElapsedTime = 0.0f;
		VisualRoot->SetRelativeScale3D(BaseVisualRootScale * CurrentVisualScaleMultiplier);
	}
	RefreshVisual();
	SetActorTickEnabled(false);

	if (!HasAuthority() && ReplicatedMovementTarget.Revision != 0)
	{
		ApplyMovementTarget(
			FTransform(ReplicatedMovementTarget.Rotation, ReplicatedMovementTarget.Location),
			ReplicatedMovementTarget.bUseSlotAttachMotion,
			ReplicatedMovementTarget.MotionDuration,
			ReplicatedMovementTarget.ArcHeight,
			ReplicatedMovementTarget.OvershootDistance,
			ReplicatedMovementTarget.bUseSettleMotion,
			ReplicatedMovementTarget.bOrientToLocalViewer,
			ReplicatedMovementTarget.ServerStartTime);
	}
}

void ACard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateTargetTransform();

	CurrentVisualWorldOffset = FMath::VInterpTo(CurrentVisualWorldOffset, TargetVisualWorldOffset, DeltaTime, MoveSpeed);
	FVector VisualWorldOffset = CurrentVisualWorldOffset;
	FRotator VisualRelativeRotation = FRotator::ZeroRotator;
	UpdateVisualScale(DeltaTime);
	UpdateSlotAttachSettle(DeltaTime, VisualWorldOffset, VisualRelativeRotation);
	UpdateLocalViewerOrientation(DeltaTime);

	if (VisualRoot)
	{
		const FVector VisualRelativeOffset = GetActorTransform().InverseTransformVectorNoScale(VisualWorldOffset);
		VisualRoot->SetRelativeLocation(VisualRelativeOffset);
		VisualRoot->SetRelativeRotation(VisualRelativeRotation);
		VisualRoot->SetRelativeScale3D(BaseVisualRootScale * CurrentVisualScaleMultiplier);
	}
	if (CardText)
	{
		const FQuat LocalViewerRotation = FQuat::Slerp(
			FQuat::Identity,
			BuildLocalViewerVisualRotation(),
			CurrentLocalViewerOrientationAlpha).GetNormalized();
		CardText->SetRelativeRotation((LocalViewerRotation * BaseCardTextRelativeRotation).GetNormalized());
	}

	if (bSlotAttachMotionActive)
	{
		UpdateSlotAttachMotion(DeltaTime);
		return;
	}

	if (RootComp && RootComp->GetAttachParent())
	{
		if (!bVisualScaleMotionActive
			&& !bSlotAttachSettleActive
			&& FMath::IsNearlyEqual(CurrentLocalViewerOrientationAlpha, TargetLocalViewerOrientationAlpha))
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaTime, MoveSpeed);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, MoveSpeed);
	SetActorLocationAndRotation(NewLocation, NewRotation);

	const bool bVisualAtTarget = CurrentVisualWorldOffset.Equals(TargetVisualWorldOffset, 0.1f);
	const bool bActorAtTarget = GetActorLocation().Equals(TargetLocation, 0.1f)
		&& GetActorRotation().Equals(TargetRotation, 0.1f);
	if (bVisualAtTarget
		&& bActorAtTarget
		&& !bVisualScaleMotionActive
		&& !bSlotAttachSettleActive
		&& FMath::IsNearlyEqual(CurrentLocalViewerOrientationAlpha, TargetLocalViewerOrientationAlpha))
	{
		CurrentVisualWorldOffset = TargetVisualWorldOffset;
		if (VisualRoot)
		{
			const FVector VisualRelativeOffset = GetActorTransform().InverseTransformVectorNoScale(CurrentVisualWorldOffset);
			VisualRoot->SetRelativeLocation(VisualRelativeOffset);
		}
		SetActorLocationAndRotation(TargetLocation, TargetRotation);
		AttachToPendingSlot();
		SetActorTickEnabled(false);
	}
}

void ACard::SetCard(int32 NewRank)
{
	const int32 ClampedRank = FMath::Clamp(NewRank, 1, 7);
	if (Rank == ClampedRank && bHasCachedVisual)
	{
		return;
	}

	Rank = ClampedRank;
	RefreshVisual();
	ForceNetUpdate();
}

void ACard::SetFaceUp(bool bNewFaceUp)
{
	if (bFaceUp == bNewFaceUp && bHasCachedVisual)
	{
		return;
	}

	bFaceUp = bNewFaceUp;
	RefreshVisual();
	ForceNetUpdate();
}

void ACard::SetHiddenFromSlot(EShowDownPlayerSlot NewHiddenFromSlot)
{
	if (HiddenFromSlot == NewHiddenFromSlot && bHasCachedVisual)
	{
		return;
	}

	HiddenFromSlot = NewHiddenFromSlot;
	RefreshVisual();
	ForceNetUpdate();
}

void ACard::SetHandOwnerSlot(EShowDownPlayerSlot NewHandOwnerSlot)
{
	if (HandOwnerSlot == NewHandOwnerSlot && bHasCachedVisual)
	{
		return;
	}

	HandOwnerSlot = NewHandOwnerSlot;
	RefreshVisual();
	ForceNetUpdate();
}

void ACard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACard, Rank);
	DOREPLIFETIME(ACard, bSelectable);
	DOREPLIFETIME(ACard, bFaceUp);
	DOREPLIFETIME(ACard, HiddenFromSlot);
	DOREPLIFETIME(ACard, HandOwnerSlot);
	DOREPLIFETIME(ACard, ReplicatedMovementTarget);
}

void ACard::OnRep_CardVisual()
{
	RefreshVisual();
}

void ACard::OnRep_Selectable()
{
	if (bSelectable)
	{
		return;
	}

	bSelected = false;
	bHovered = false;
	UpdateTargetTransform();
	EnableMotionTick();
}

void ACard::OnRep_MovementTarget()
{
	if (!HasActorBegunPlay())
	{
		return;
	}
	ApplyMovementTarget(
		FTransform(ReplicatedMovementTarget.Rotation, ReplicatedMovementTarget.Location),
		ReplicatedMovementTarget.bUseSlotAttachMotion,
		ReplicatedMovementTarget.MotionDuration,
		ReplicatedMovementTarget.ArcHeight,
		ReplicatedMovementTarget.OvershootDistance,
		ReplicatedMovementTarget.bUseSettleMotion,
		ReplicatedMovementTarget.bOrientToLocalViewer,
		ReplicatedMovementTarget.ServerStartTime);
	ApplySynchronizedVisualScale(
		ReplicatedMovementTarget.VisualScaleMultiplier,
		ReplicatedMovementTarget.ServerStartTime,
		ReplicatedMovementTarget.MotionDuration);
}

void ACard::RefreshVisual()
{
	if (!CardText)
	{
		return;
	}

	bool bVisibleToLocalPlayer = true;
	const ASDPlayerState* LocalPlayerState = nullptr;
	if (const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		LocalPlayerState = LocalPlayerController->GetPlayerState<ASDPlayerState>();
	}

	if (HiddenFromSlot != EShowDownPlayerSlot::None && LocalPlayerState)
	{
		bVisibleToLocalPlayer = bVisibleToLocalPlayer && LocalPlayerState->ShowDownSlot != HiddenFromSlot;
	}

	const bool bShouldShowText = bFaceUp && bVisibleToLocalPlayer;
	if (!bHasCachedVisual || bCachedVisualVisible != bShouldShowText)
	{
		CardText->SetVisibility(bShouldShowText);
		bCachedVisualVisible = bShouldShowText;
	}

	if (!bHasCachedVisual || CachedVisualRank != Rank)
	{
		CardText->SetText(FText::AsNumber(Rank));
		CachedVisualRank = Rank;
	}

	bHasCachedVisual = true;
}

void ACard::SelectCard(bool bNewSelected)
{
	bSelected = bNewSelected && bSelectable;
	UpdateTargetTransform();
	EnableMotionTick();
}

bool ACard::IsSelected() const
{
	return bSelected;
}

void ACard::SetHovered(bool bNewHovered)
{
	bHovered = bNewHovered && bSelectable;
	UpdateTargetTransform();
	EnableMotionTick();
}

bool ACard::IsHovered() const
{
	return bHovered;
}

void ACard::SetSelectable(bool bNewSelectable)
{
	if (bSelectable == bNewSelectable)
	{
		return;
	}

	bSelectable = bNewSelectable;
	if (!bSelectable)
	{
		bSelected = false;
		bHovered = false;
		UpdateTargetTransform();
		EnableMotionTick();
	}
	ForceNetUpdate();
}

bool ACard::IsCardSelectable() const
{
	return bSelectable;
}

bool ACard::IsCardSelectableForSlot(EShowDownPlayerSlot PlayerSlot) const
{
	if (!bSelectable)
	{
		return false;
	}

	if (HandOwnerSlot == EShowDownPlayerSlot::None)
	{
		return true;
	}

	return PlayerSlot != EShowDownPlayerSlot::None && HandOwnerSlot == PlayerSlot;
}

void ACard::MoveToSlot(USceneComponent* Slot, bool bNewFaceUp)
{
	MoveToSlotComponent(Slot, bNewFaceUp, FRotator::ZeroRotator);
}

void ACard::MoveToSlotWithRotationOffset(USceneComponent* Slot, bool bNewFaceUp, FRotator RotationOffset)
{
	if (!Slot)
	{
		return;
	}

	MoveToSlotComponent(Slot, bNewFaceUp, RotationOffset);
}

void ACard::MoveToSlotComponent(USceneComponent* Slot, bool bNewFaceUp, FRotator RotationOffset)
{
	if (!Slot)
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	PendingSlotAttachComponent = Slot;
	PendingSlotAttachRotationOffset = RotationOffset;

	FTransform SlotTransform = Slot->GetComponentTransform();
	SlotTransform.SetRotation((SlotTransform.GetRotation() * RotationOffset.Quaternion()).GetNormalized());
	SlotTransform.SetScale3D(GetActorScale3D());
	MoveToSlotTransform(SlotTransform, bNewFaceUp, ForeheadSlotAttachTargetScale);
}

void ACard::MoveToSlotTransform(const FTransform& SlotTransform, bool bNewFaceUp, float VisualScaleMultiplier)
{
	bSelected = false;
	bHovered = false;
	SetSelectable(false);
	SetFaceUp(bNewFaceUp);
	SetTargetVisualScaleMultiplier(VisualScaleMultiplier);
	ApplyMovementTarget(SlotTransform, bUseSlotAttachMotion);
	PublishMovementTarget(SlotTransform, bUseSlotAttachMotion);

}

float ACard::GetSlotAttachMotionTotalSeconds() const
{
	return bUseSlotAttachMotion
		? FMath::Max(0.0f, SlotAttachDuration) + FMath::Max(0.0f, SlotAttachSettleDuration)
		: 0.0f;
}

void ACard::MoveToRevealTransform(const FTransform& RevealTransform, float VisualScaleMultiplier)
{
	ClearPendingSlotAttachment();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bSelected = false;
	bHovered = false;
	SetSelectable(false);
	SetHiddenFromSlot(EShowDownPlayerSlot::None);
	SetFaceUp(true);
	SetTargetVisualScaleMultiplier(VisualScaleMultiplier);
	ApplyMovementTarget(RevealTransform, bUseSlotAttachMotion);
	PublishMovementTarget(RevealTransform, bUseSlotAttachMotion);
}

float ACard::GetRevealMotionTotalSeconds() const
{
	return GetSlotAttachMotionTotalSeconds();
}

void ACard::MoveToHandTransform(const FTransform& NewTransform)
{
	ClearPendingSlotAttachment();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bVisualScaleMotionActive = false;
	VisualScaleElapsedTime = 0.0f;
	SetTargetVisualScaleMultiplier(1.0f);
	if (VisualRoot)
	{
		VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
	}

	ApplyMovementTarget(NewTransform, false);
	PublishMovementTarget(NewTransform, false);
}

void ACard::MoveToPresentationTransform(
	const FTransform& NewTransform,
	float VisualScaleMultiplier,
	float MotionDuration,
	float ArcHeight,
	bool bUseSettleMotion,
	bool bOrientToLocalViewer)
{
	ClearPendingSlotAttachment();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bSelected = false;
	bHovered = false;
	SetSelectable(false);
	const float SafeDuration = FMath::Max(0.05f, MotionDuration);
	const float SafeArcHeight = FMath::Max(0.0f, ArcHeight);
	ApplyMovementTarget(NewTransform, true, SafeDuration, SafeArcHeight, 0.0f, bUseSettleMotion, bOrientToLocalViewer);
	SetTargetVisualScaleMultiplier(VisualScaleMultiplier);
	PublishMovementTarget(NewTransform, true, SafeDuration, SafeArcHeight, 0.0f, bUseSettleMotion, bOrientToLocalViewer);
}

void ACard::EnableMotionTick()
{
	SetActorTickEnabled(true);
}

void ACard::PublishMovementTarget(
	const FTransform& NewTransform,
	bool bPlaySlotAttachMotion,
	float MotionDuration,
	float ArcHeight,
	float OvershootDistance,
	bool bUseSettleMotion,
	bool bOrientToLocalViewer,
	float ServerStartTime)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedMovementTarget.Location = NewTransform.GetLocation();
	ReplicatedMovementTarget.Rotation = NewTransform.GetRotation().Rotator();
	ReplicatedMovementTarget.bUseSlotAttachMotion = bPlaySlotAttachMotion;
	ReplicatedMovementTarget.MotionDuration = MotionDuration >= 0.0f ? MotionDuration : SlotAttachDuration;
	ReplicatedMovementTarget.ArcHeight = ArcHeight >= 0.0f ? ArcHeight : SlotAttachArcHeight;
	ReplicatedMovementTarget.OvershootDistance = OvershootDistance >= 0.0f ? OvershootDistance : SlotAttachOvershootDistance;
	ReplicatedMovementTarget.bUseSettleMotion = bUseSettleMotion;
	ReplicatedMovementTarget.bOrientToLocalViewer = bOrientToLocalViewer;
	ReplicatedMovementTarget.VisualScaleMultiplier = TargetVisualScaleMultiplier;
	if (ServerStartTime >= 0.0f)
	{
		ReplicatedMovementTarget.ServerStartTime = ServerStartTime;
	}
	else if (GetWorld() && GetWorld()->GetGameState())
	{
		ReplicatedMovementTarget.ServerStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	}
	else
	{
		ReplicatedMovementTarget.ServerStartTime = -1.0f;
	}
	++ReplicatedMovementTarget.Revision;
	if (ReplicatedMovementTarget.Revision == 0)
	{
		ReplicatedMovementTarget.Revision = 1;
	}
	ForceNetUpdate();
}

void ACard::ApplyMovementTarget(
	const FTransform& NewTransform,
	bool bPlaySlotAttachMotion,
	float MotionDuration,
	float ArcHeight,
	float OvershootDistance,
	bool bUseSettleMotion,
	bool bOrientToLocalViewer,
	float ServerStartTime)
{
	ResetTravelMotionState();
	ActiveSlotAttachDuration = MotionDuration >= 0.0f ? MotionDuration : SlotAttachDuration;
	ActiveSlotAttachArcHeight = ArcHeight >= 0.0f ? ArcHeight : SlotAttachArcHeight;
	ActiveSlotAttachOvershootDistance = OvershootDistance >= 0.0f ? OvershootDistance : SlotAttachOvershootDistance;
	bActiveSlotAttachSettleMotion = bUseSettleMotion;
	bActiveOrientToLocalViewer = bOrientToLocalViewer;
	TargetLocalViewerOrientationAlpha = bActiveOrientToLocalViewer ? 1.0f : 0.0f;
	DefaultLocation = NewTransform.GetLocation();
	DefaultRotation = NewTransform.GetRotation().Rotator();
	UpdateTargetTransform();
	EnableMotionTick();

	if (bPlaySlotAttachMotion)
	{
		StartSlotAttachMotion(FTransform(DefaultRotation, DefaultLocation));
		if (!HasAuthority() && ServerStartTime >= 0.0f && GetWorld() && GetWorld()->GetGameState())
		{
			const float ServerNow = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
			const float ElapsedBeforeReceipt = FMath::Max(0.0f, ServerNow - ServerStartTime);
			if (ElapsedBeforeReceipt > KINDA_SMALL_NUMBER)
			{
				const float MotionDurationSeconds = FMath::Max(0.05f, ActiveSlotAttachDuration);
				UpdateSlotAttachMotion(FMath::Min(ElapsedBeforeReceipt, MotionDurationSeconds));
				UpdateLocalViewerOrientation(FMath::Min(ElapsedBeforeReceipt, MotionDurationSeconds));
				if (bSlotAttachSettleActive && ElapsedBeforeReceipt > MotionDurationSeconds)
				{
					SlotAttachSettleElapsedTime = FMath::Min(
						ElapsedBeforeReceipt - MotionDurationSeconds,
						FMath::Max(0.0f, SlotAttachSettleDuration));
					if (SlotAttachSettleElapsedTime >= SlotAttachSettleDuration)
					{
						bSlotAttachSettleActive = false;
					}
				}
			}
		}
	}
}

void ACard::AttachToPendingSlot()
{
	USceneComponent* Slot = PendingSlotAttachComponent.Get();
	if (!Slot)
	{
		ClearPendingSlotAttachment();
		return;
	}

	FTransform SlotTransform = Slot->GetComponentTransform();
	SlotTransform.SetRotation((SlotTransform.GetRotation() * PendingSlotAttachRotationOffset.Quaternion()).GetNormalized());
	SlotTransform.SetScale3D(GetActorScale3D());
	SetActorTransform(SlotTransform, false, nullptr, ETeleportType::TeleportPhysics);
	AttachToComponent(Slot, FAttachmentTransformRules::KeepWorldTransform);
	ClearPendingSlotAttachment();
	ForceNetUpdate();
}

void ACard::ClearPendingSlotAttachment()
{
	PendingSlotAttachComponent.Reset();
	PendingSlotAttachRotationOffset = FRotator::ZeroRotator;
}

void ACard::ResetTravelMotionState()
{
	bSlotAttachMotionActive = false;
	bSlotAttachSettleActive = false;
	SlotAttachElapsedTime = 0.0f;
	SlotAttachSettleElapsedTime = 0.0f;
}

void ACard::UpdateTargetTransform()
{
	TargetRotation = DefaultRotation;
	TargetLocation = DefaultLocation;

	if (bSelected)
	{
		TargetVisualWorldOffset = SelectedOffset;
		return;
	}

	if (bHovered)
	{
		TargetVisualWorldOffset = HoverOffset;
		return;
	}

	TargetVisualWorldOffset = FVector::ZeroVector;
}

void ACard::StartSlotAttachMotion(const FTransform& TargetTransform)
{
	SlotAttachStartLocation = GetActorLocation();
	SlotAttachTargetLocation = TargetTransform.GetLocation();
	SlotAttachTravelDirection = (SlotAttachTargetLocation - SlotAttachStartLocation).GetSafeNormal();
	if (SlotAttachTravelDirection.IsNearlyZero())
	{
		SlotAttachTravelDirection = GetActorForwardVector();
	}

	SlotAttachStartRotation = GetActorQuat();
	SlotAttachTargetRotation = TargetTransform.GetRotation();
	SlotAttachElapsedTime = 0.0f;
	SlotAttachSettleElapsedTime = 0.0f;
	bSlotAttachMotionActive = true;
	bSlotAttachSettleActive = false;
}

void ACard::UpdateSlotAttachMotion(float DeltaTime)
{
	SlotAttachElapsedTime += DeltaTime;
	const float Duration = FMath::Max(0.05f, ActiveSlotAttachDuration);
	const float Alpha = FMath::Clamp(SlotAttachElapsedTime / Duration, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	const float ArcAlpha = FMath::Sin(Alpha * PI);
	const float OvershootAlpha = FMath::Clamp((Alpha - 0.72f) / 0.28f, 0.0f, 1.0f);
	const float OvershootAmount = FMath::Sin(OvershootAlpha * PI) * ActiveSlotAttachOvershootDistance;

	const FVector ArcOffset = FVector::UpVector * ActiveSlotAttachArcHeight * ArcAlpha;
	const FVector OvershootOffset = SlotAttachTravelDirection * OvershootAmount;
	const FVector NewLocation =
		FMath::Lerp(SlotAttachStartLocation, SlotAttachTargetLocation, EasedAlpha)
		+ ArcOffset
		+ OvershootOffset;

	const FQuat BaseRotation = FQuat::Slerp(SlotAttachStartRotation, SlotAttachTargetRotation, EasedAlpha).GetNormalized();
	const FRotator FlightRotation = ScaleRotator(SlotAttachFlightRotationAmplitude, ArcAlpha);
	const FQuat NewRotation = (BaseRotation * FlightRotation.Quaternion()).GetNormalized();
	SetActorLocationAndRotation(NewLocation, NewRotation);

	if (Alpha >= 1.0f)
	{
		bSlotAttachMotionActive = false;
		bSlotAttachSettleActive = bActiveSlotAttachSettleMotion && SlotAttachSettleDuration > KINDA_SMALL_NUMBER;
		SlotAttachSettleElapsedTime = 0.0f;
		SetActorLocationAndRotation(SlotAttachTargetLocation, SlotAttachTargetRotation);
		AttachToPendingSlot();
	}
}

void ACard::UpdateSlotAttachSettle(float DeltaTime, FVector& InOutVisualWorldOffset, FRotator& OutVisualRelativeRotation)
{
	if (!bSlotAttachSettleActive)
	{
		return;
	}

	SlotAttachSettleElapsedTime += DeltaTime;
	const float Duration = FMath::Max(0.01f, SlotAttachSettleDuration);
	const float Alpha = FMath::Clamp(SlotAttachSettleElapsedTime / Duration, 0.0f, 1.0f);
	const float Decay = 1.0f - Alpha;
	const float Wave = FMath::Sin(Alpha * PI * SlotAttachSettleOscillations) * Decay;

	InOutVisualWorldOffset += FVector::UpVector * SlotAttachSettleLocationAmplitude * Wave;
	OutVisualRelativeRotation = ScaleRotator(SlotAttachSettleRotationAmplitude, Wave);

	if (Alpha >= 1.0f)
	{
		bSlotAttachSettleActive = false;
		SlotAttachSettleElapsedTime = 0.0f;
		OutVisualRelativeRotation = FRotator::ZeroRotator;
	}
}

void ACard::SetTargetVisualScaleMultiplier(float NewTargetScaleMultiplier)
{
	const float ClampedTargetScale = FMath::Max(0.1f, NewTargetScaleMultiplier);
	if (FMath::IsNearlyEqual(TargetVisualScaleMultiplier, ClampedTargetScale))
	{
		return;
	}

	TargetVisualScaleMultiplier = ClampedTargetScale;
	StartVisualScaleMotion(TargetVisualScaleMultiplier);
}

void ACard::StartVisualScaleMotion(float NewTargetScaleMultiplier)
{
	VisualScaleStartMultiplier = CurrentVisualScaleMultiplier;
	VisualScaleElapsedTime = 0.0f;
	bVisualScaleMotionActive = !FMath::IsNearlyEqual(CurrentVisualScaleMultiplier, NewTargetScaleMultiplier);

	if (!bVisualScaleMotionActive)
	{
		CurrentVisualScaleMultiplier = NewTargetScaleMultiplier;
	}
}

void ACard::ApplySynchronizedVisualScale(
	float NewTargetScaleMultiplier,
	float ServerStartTime,
	float MotionDuration)
{
	TargetVisualScaleMultiplier = FMath::Max(0.1f, NewTargetScaleMultiplier);
	StartVisualScaleMotion(TargetVisualScaleMultiplier);
	if (!bVisualScaleMotionActive
		|| ServerStartTime < 0.0f
		|| !GetWorld()
		|| !GetWorld()->GetGameState())
	{
		return;
	}

	const float Duration = FMath::Max(0.05f, MotionDuration);
	VisualScaleElapsedTime = FMath::Clamp(
		GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - ServerStartTime,
		0.0f,
		Duration);
	const float Alpha = FMath::Clamp(VisualScaleElapsedTime / Duration, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	CurrentVisualScaleMultiplier = FMath::Lerp(
		VisualScaleStartMultiplier,
		TargetVisualScaleMultiplier,
		EasedAlpha);
	if (Alpha >= 1.0f)
	{
		CurrentVisualScaleMultiplier = TargetVisualScaleMultiplier;
		bVisualScaleMotionActive = false;
		VisualScaleElapsedTime = 0.0f;
	}
}

void ACard::UpdateVisualScale(float DeltaTime)
{
	if (!bVisualScaleMotionActive)
	{
		return;
	}

	VisualScaleElapsedTime += DeltaTime;
	const float Duration = FMath::Max(0.05f, ActiveSlotAttachDuration);
	const float Alpha = FMath::Clamp(VisualScaleElapsedTime / Duration, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	CurrentVisualScaleMultiplier = FMath::Lerp(VisualScaleStartMultiplier, TargetVisualScaleMultiplier, EasedAlpha);

	if (Alpha >= 1.0f)
	{
		CurrentVisualScaleMultiplier = TargetVisualScaleMultiplier;
		bVisualScaleMotionActive = false;
		VisualScaleElapsedTime = 0.0f;
	}
}

FQuat ACard::BuildLocalViewerVisualRotation() const
{
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const APlayerCameraManager* CameraManager = LocalPlayerController
		? LocalPlayerController->PlayerCameraManager
		: nullptr;
	if (!CameraManager)
	{
		return FQuat::Identity;
	}

	const FQuat ActorRotation = GetActorQuat();
	const FVector RotationAxis = ActorRotation.RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector CurrentLongAxis = ActorRotation.RotateVector(FVector::UpVector).GetSafeNormal();
	FVector DesiredLongAxis = GetActorLocation() - CameraManager->GetCameraLocation();
	DesiredLongAxis -= RotationAxis * FVector::DotProduct(DesiredLongAxis, RotationAxis);
	DesiredLongAxis = DesiredLongAxis.GetSafeNormal();
	if (DesiredLongAxis.IsNearlyZero() || CurrentLongAxis.IsNearlyZero())
	{
		return FQuat::Identity;
	}

	const float SinAngle = FVector::DotProduct(
		RotationAxis,
		FVector::CrossProduct(CurrentLongAxis, DesiredLongAxis));
	const float CosAngle = FMath::Clamp(FVector::DotProduct(CurrentLongAxis, DesiredLongAxis), -1.0f, 1.0f);
	return FQuat(FVector::ForwardVector, FMath::Atan2(SinAngle, CosAngle));
}

void ACard::UpdateLocalViewerOrientation(float DeltaTime)
{
	const float Duration = FMath::Max(0.05f, ActiveSlotAttachDuration);
	CurrentLocalViewerOrientationAlpha = FMath::FInterpConstantTo(
		CurrentLocalViewerOrientationAlpha,
		TargetLocalViewerOrientationAlpha,
		DeltaTime,
		1.0f / Duration);
}

FRotator ACard::ScaleRotator(const FRotator& Rotator, float Scale) const
{
	return FRotator(Rotator.Pitch * Scale, Rotator.Yaw * Scale, Rotator.Roll * Scale);
}

bool ACard::CanInteract_Implementation(AActor* Interactor) const
{
	if (HandOwnerSlot == EShowDownPlayerSlot::None)
	{
		return bSelectable;
	}

	AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(Interactor);
	const ASDPlayerState* ShowDownPlayerState = ShowDownController
		? ShowDownController->GetPlayerState<ASDPlayerState>()
		: nullptr;

	return ShowDownPlayerState && IsCardSelectableForSlot(ShowDownPlayerState->ShowDownSlot);
}

void ACard::Interact_Implementation(AActor* Interactor)
{
	if (!bSelectable)
	{
		return;
	}

	AShowDownPlayerController* ShowDownController = Cast<AShowDownPlayerController>(Interactor);
	if (!ShowDownController)
	{
		ShowDownController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	if (ShowDownController && CanInteract_Implementation(ShowDownController))
	{
		ShowDownController->SubmitSelectedCard(this);
	}
}
