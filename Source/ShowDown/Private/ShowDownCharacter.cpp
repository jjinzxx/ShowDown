#include "ShowDownCharacter.h"

#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "ShowDownCharacterAnimInstance.h"
#include "ShowDownGameStateBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ActionAnimationFallbackReturnDelay = 1.0f;

	FString GetAnimStateDebugName(EShowDownCharacterAnimState State)
	{
		switch (State)
		{
		case EShowDownCharacterAnimState::Shoot:
			return TEXT("Shoot");
		case EShowDownCharacterAnimState::Hit:
			return TEXT("Hit");
		case EShowDownCharacterAnimState::Idle:
		default:
			return TEXT("Idle");
		}
	}
}

AShowDownCharacter::AShowDownCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	CharacterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultMesh(
		TEXT("/Game/Assets/asd/Idle.Idle"));
	if (DefaultMesh.Succeeded())
	{
		CharacterMesh->SetSkeletalMesh(DefaultMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> DefaultIdleAnimation(
		TEXT("/Game/Assets/asd/Idle_Anim.Idle_Anim"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> DefaultAnimClass(
		TEXT("/Game/BluePrints/Characters/ABP_ShowDownCharacter"));
	if (DefaultAnimClass.Succeeded())
	{
		CharacterMesh->SetAnimInstanceClass(DefaultAnimClass.Class);
		CachedAnimBlueprintClass = DefaultAnimClass.Class;
	}
	else if (DefaultIdleAnimation.Succeeded())
	{
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CharacterMesh->PlayAnimation(DefaultIdleAnimation.Object, true);
	}

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	MovementComponent->MaxWalkSpeed = 240.0f;
}

void AShowDownCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	CacheAnimBlueprintClass();
	CacheBaseMeshTransform();
	PushAnimStateToAnimInstance();
}

void AShowDownCharacter::BeginPlay()
{
	Super::BeginPlay();
	CacheAnimBlueprintClass();
	CacheBaseMeshTransform();
	PushAnimStateToAnimInstance();
	BindToRouletteEvents();
}

void AShowDownCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromRouletteEvents();
	GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AShowDownCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShowDownCharacter, ReplicatedAnimState);
	DOREPLIFETIME(AShowDownCharacter, CharacterRole);
	DOREPLIFETIME(AShowDownCharacter, PlayerSlot);
	DOREPLIFETIME(AShowDownCharacter, CharacterDisplayName);
}

void AShowDownCharacter::SetCharacterAnimState(EShowDownCharacterAnimState NewState)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
		ApplyCharacterAnimState(NewState);
		return;
	}

	ServerSetCharacterAnimState(NewState);
}

void AShowDownCharacter::PlayCharacterActionAnim(
	EShowDownCharacterAnimState NewState,
	float Duration,
	bool bReturnToIdle)
{
	if (NewState == EShowDownCharacterAnimState::Idle)
	{
		SetCharacterAnimState(EShowDownCharacterAnimState::Idle);
		return;
	}

	if (HasAuthority())
	{
		const float AssignedAnimationDuration = GetAssignedActionAnimationDuration(NewState);
		const float ResolvedDuration = Duration > 0.0f
			? Duration
			: (AssignedAnimationDuration > 0.0f ? AssignedAnimationDuration : GetDefaultAnimDuration(NewState));
		GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
		ApplyCharacterAnimState(NewState);
		if (bReturnToIdle)
		{
			ScheduleAnimStateReset(ResolvedDuration);
		}
		return;
	}

	ServerPlayCharacterActionAnim(NewState, Duration, bReturnToIdle);
}

void AShowDownCharacter::PlayShootAnimation(float Duration)
{
	PlayCharacterActionAnim(EShowDownCharacterAnimState::Shoot, Duration, true);
}

void AShowDownCharacter::PlayHitAnimation(float Duration)
{
	PlayCharacterActionAnim(EShowDownCharacterAnimState::Hit, Duration, false);
}

void AShowDownCharacter::StartHitRagdoll()
{
	if (!HasAuthority())
	{
		ServerPlayCharacterActionAnim(EShowDownCharacterAnimState::Hit, -1.0f, false);
		return;
	}

	GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
	ApplyCharacterAnimState(EShowDownCharacterAnimState::Hit);
}

void AShowDownCharacter::ResetCharacterAnimState()
{
	SetCharacterAnimState(EShowDownCharacterAnimState::Idle);
}

void AShowDownCharacter::FinishCurrentCharacterActionAnim()
{
	NotifyCharacterActionAnimFinished(ReplicatedAnimState);
}

void AShowDownCharacter::NotifyCharacterActionAnimFinished(EShowDownCharacterAnimState FinishedState)
{
	if (FinishedState == EShowDownCharacterAnimState::Idle)
	{
		return;
	}

	if (HasAuthority())
	{
		FinishCharacterActionAnimIfCurrent(FinishedState);
		return;
	}

	if (ReplicatedAnimState == FinishedState)
	{
		FinishCharacterActionAnimIfCurrent(FinishedState);
	}

	ServerNotifyCharacterActionAnimFinished(FinishedState);
}

void AShowDownCharacter::SetCharacterIdentity(
	EShowDownCharacterRole NewRole,
	EShowDownPlayerSlot NewPlayerSlot,
	const FString& NewDisplayName)
{
	if (HasAuthority())
	{
		if (CharacterRole == NewRole
			&& PlayerSlot == NewPlayerSlot
			&& CharacterDisplayName == NewDisplayName)
		{
			return;
		}

		CharacterRole = NewRole;
		PlayerSlot = NewPlayerSlot;
		CharacterDisplayName = NewDisplayName;
		OnRep_Identity();
		ForceNetUpdate();
		return;
	}

	ServerSetCharacterIdentity(NewRole, NewPlayerSlot, NewDisplayName);
}

void AShowDownCharacter::ApplyIdentityFromPlayerState(ASDPlayerState* InPlayerState, EShowDownCharacterRole NewRole)
{
	if (!InPlayerState)
	{
		return;
	}

	SetCharacterIdentity(NewRole, InPlayerState->ShowDownSlot, InPlayerState->GetPlayerName());
}

void AShowDownCharacter::SetCharacterRole(EShowDownCharacterRole NewRole)
{
	SetCharacterIdentity(NewRole, PlayerSlot, CharacterDisplayName);
}

void AShowDownCharacter::SetPlayerSlot(EShowDownPlayerSlot NewPlayerSlot)
{
	SetCharacterIdentity(CharacterRole, NewPlayerSlot, CharacterDisplayName);
}

void AShowDownCharacter::SetCharacterDisplayName(const FString& NewDisplayName)
{
	SetCharacterIdentity(CharacterRole, PlayerSlot, NewDisplayName);
}

EShowDownPlayerSlot AShowDownCharacter::GetLocalPlayerSlot() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return EShowDownPlayerSlot::None;
	}

	const APlayerController* LocalPlayerController = World->GetFirstPlayerController();
	const ASDPlayerState* LocalPlayerState = LocalPlayerController
		? Cast<ASDPlayerState>(LocalPlayerController->PlayerState)
		: nullptr;

	return LocalPlayerState ? LocalPlayerState->ShowDownSlot : EShowDownPlayerSlot::None;
}

bool AShowDownCharacter::IsLocalPlayerCharacter() const
{
	if (IsLocallyControlled())
	{
		return true;
	}

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	return LocalPlayerSlot != EShowDownPlayerSlot::None && PlayerSlot == LocalPlayerSlot;
}

bool AShowDownCharacter::IsOpponentCharacterForLocalPlayer() const
{
	if (CharacterRole == EShowDownCharacterRole::Opponent)
	{
		return true;
	}

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	return LocalPlayerSlot != EShowDownPlayerSlot::None
		&& PlayerSlot != EShowDownPlayerSlot::None
		&& PlayerSlot != LocalPlayerSlot;
}

void AShowDownCharacter::OnRep_AnimState()
{
	StartActionVisual(ReplicatedAnimState);
	PushAnimStateToAnimInstance();
	OnCharacterAnimStateChanged(ReplicatedAnimState);
}

void AShowDownCharacter::OnRep_Identity()
{
	OnCharacterIdentityChanged();
}

void AShowDownCharacter::ServerSetCharacterAnimState_Implementation(EShowDownCharacterAnimState NewState)
{
	SetCharacterAnimState(NewState);
}

void AShowDownCharacter::ServerPlayCharacterActionAnim_Implementation(
	EShowDownCharacterAnimState NewState,
	float Duration,
	bool bReturnToIdle)
{
	PlayCharacterActionAnim(NewState, Duration, bReturnToIdle);
}

void AShowDownCharacter::ServerNotifyCharacterActionAnimFinished_Implementation(EShowDownCharacterAnimState FinishedState)
{
	NotifyCharacterActionAnimFinished(FinishedState);
}

void AShowDownCharacter::HandleRouletteStarted(EShowDownSide Target, int32 BulletCount)
{
	if (!HasAuthority() || !ShouldReactToSingleRouletteTarget(Target))
	{
		return;
	}

	PlayShootAnimation();
}

void AShowDownCharacter::HandleRouletteResult(EShowDownSide Target, bool bHit)
{
	if (!HasAuthority() || !bHit || !ShouldReactToSingleRouletteTarget(Target))
	{
		return;
	}

	PlayHitAnimation();
}

void AShowDownCharacter::HandleMultiplayerRouletteStarted(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount)
{
	if (!HasAuthority() || !ShouldReactToMultiplayerRouletteTarget(TargetSlot))
	{
		return;
	}

	PlayShootAnimation();
}

void AShowDownCharacter::HandleMultiplayerRouletteResult(
	EShowDownPlayerSlot TargetSlot,
	const FString& TargetName,
	int32 BulletCount,
	bool bHit,
	int32 RemainingLives)
{
	if (!HasAuthority() || !bHit || !ShouldReactToMultiplayerRouletteTarget(TargetSlot))
	{
		return;
	}

	PlayHitAnimation();
}

void AShowDownCharacter::ServerSetCharacterIdentity_Implementation(
	EShowDownCharacterRole NewRole,
	EShowDownPlayerSlot NewPlayerSlot,
	const FString& NewDisplayName)
{
	SetCharacterIdentity(NewRole, NewPlayerSlot, NewDisplayName);
}

void AShowDownCharacter::ApplyCharacterAnimState(EShowDownCharacterAnimState NewState)
{
	if (ReplicatedAnimState == NewState)
	{
		return;
	}

	ReplicatedAnimState = NewState;
	StartActionVisual(ReplicatedAnimState);
	PushAnimStateToAnimInstance();
	OnCharacterAnimStateChanged(ReplicatedAnimState);
	ForceNetUpdate();
}

void AShowDownCharacter::FinishCharacterActionAnimIfCurrent(EShowDownCharacterAnimState FinishedState)
{
	if (ReplicatedAnimState != FinishedState)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
	ApplyCharacterAnimState(EShowDownCharacterAnimState::Idle);
}

void AShowDownCharacter::BindToRouletteEvents()
{
	UWorld* World = GetWorld();
	AShowDownGameStateBase* ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	if (!ShowDownGameState)
	{
		return;
	}

	ShowDownGameState->OnRouletteStarted.AddUniqueDynamic(this, &AShowDownCharacter::HandleRouletteStarted);
	ShowDownGameState->OnRouletteResult.AddUniqueDynamic(this, &AShowDownCharacter::HandleRouletteResult);
	ShowDownGameState->OnMultiplayerRouletteStarted.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteStarted);
	ShowDownGameState->OnMultiplayerRouletteResult.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteResult);
}

void AShowDownCharacter::UnbindFromRouletteEvents()
{
	UWorld* World = GetWorld();
	AShowDownGameStateBase* ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	if (!ShowDownGameState)
	{
		return;
	}

	ShowDownGameState->OnRouletteStarted.RemoveDynamic(this, &AShowDownCharacter::HandleRouletteStarted);
	ShowDownGameState->OnRouletteResult.RemoveDynamic(this, &AShowDownCharacter::HandleRouletteResult);
	ShowDownGameState->OnMultiplayerRouletteStarted.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteStarted);
	ShowDownGameState->OnMultiplayerRouletteResult.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteResult);
}

void AShowDownCharacter::ScheduleAnimStateReset(float Duration)
{
	if (Duration <= 0.0f)
	{
		ResetCharacterAnimState();
		return;
	}

	GetWorldTimerManager().SetTimer(
		AnimStateResetTimerHandle,
		this,
		&AShowDownCharacter::ResetCharacterAnimState,
		Duration,
		false);
}

float AShowDownCharacter::GetDefaultAnimDuration(EShowDownCharacterAnimState State) const
{
	switch (State)
	{
	case EShowDownCharacterAnimState::Shoot:
		return ActionAnimationFallbackReturnDelay;
	case EShowDownCharacterAnimState::Hit:
		return 0.0f;
	case EShowDownCharacterAnimState::Idle:
	default:
		return 0.0f;
	}
}

bool AShowDownCharacter::ShouldReactToSingleRouletteTarget(EShowDownSide Target) const
{
	switch (Target)
	{
	case EShowDownSide::Player:
		return CharacterRole == EShowDownCharacterRole::Player || IsLocalPlayerCharacter();
	case EShowDownSide::Collector:
		return CharacterRole == EShowDownCharacterRole::Opponent;
	default:
		return false;
	}
}

bool AShowDownCharacter::ShouldReactToMultiplayerRouletteTarget(EShowDownPlayerSlot TargetSlot) const
{
	return TargetSlot != EShowDownPlayerSlot::None && PlayerSlot == TargetSlot;
}

UAnimationAsset* AShowDownCharacter::GetAssignedActionAnimation(EShowDownCharacterAnimState State) const
{
	switch (State)
	{
	case EShowDownCharacterAnimState::Shoot:
		return ShootAnimationAsset;
	case EShowDownCharacterAnimState::Hit:
	case EShowDownCharacterAnimState::Idle:
	default:
		return nullptr;
	}
}

bool AShowDownCharacter::CanPlayAssignedActionAnimation(const UAnimationAsset* AnimationAsset) const
{
	if (!AnimationAsset || !GetMesh() || !GetMesh()->GetSkeletalMeshAsset())
	{
		return false;
	}

	const USkeleton* AnimationSkeleton = AnimationAsset->GetSkeleton();
	const USkeleton* MeshSkeleton = GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
	if (!AnimationSkeleton || !MeshSkeleton)
	{
		return false;
	}

	return AnimationSkeleton == MeshSkeleton;
}

bool AShowDownCharacter::TryPlayAssignedActionAnimation(EShowDownCharacterAnimState State)
{
	if (State == EShowDownCharacterAnimState::Idle || !GetMesh())
	{
		return false;
	}

	UAnimationAsset* AnimationAsset = GetAssignedActionAnimation(State);
	if (!CanPlayAssignedActionAnimation(AnimationAsset))
	{
		return false;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->PlayAnimation(AnimationAsset, false);
	return true;
}

float AShowDownCharacter::GetAssignedActionAnimationDuration(EShowDownCharacterAnimState State) const
{
	const UAnimationAsset* AnimationAsset = GetAssignedActionAnimation(State);
	return CanPlayAssignedActionAnimation(AnimationAsset) ? AnimationAsset->GetPlayLength() : 0.0f;
}

void AShowDownCharacter::LogAssignedActionAnimationFailure(
	EShowDownCharacterAnimState State,
	const UAnimationAsset* AnimationAsset) const
{
	if (!AnimationAsset)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s has no %s Animation Asset assigned. Returning to Idle after %.1f second."),
			*GetName(),
			*GetAnimStateDebugName(State),
			ActionAnimationFallbackReturnDelay);
		return;
	}

	const USkeleton* AnimationSkeleton = AnimationAsset->GetSkeleton();
	const USkeletalMesh* MeshAsset = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* MeshSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;

	const FString Message = FString::Printf(
		TEXT("%s cannot play %s animation '%s'. Animation skeleton: %s, Mesh skeleton: %s. Returning to Idle after %.1f second."),
		*GetName(),
		*GetAnimStateDebugName(State),
		*AnimationAsset->GetName(),
		AnimationSkeleton ? *AnimationSkeleton->GetPathName() : TEXT("None"),
		MeshSkeleton ? *MeshSkeleton->GetPathName() : TEXT("None"),
		ActionAnimationFallbackReturnDelay);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Message);
	}
}

FName AShowDownCharacter::ResolveRagdollHitBoneName() const
{
	const USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return NAME_None;
	}

	if (RagdollHitBoneName != NAME_None && CharacterMesh->GetBoneIndex(RagdollHitBoneName) != INDEX_NONE)
	{
		return RagdollHitBoneName;
	}

	static const FName FallbackBoneNames[] =
	{
		TEXT("Head"),
		TEXT("head"),
		TEXT("Neck"),
		TEXT("neck"),
		TEXT("Spine2"),
		TEXT("spine2"),
		TEXT("Spine"),
		TEXT("spine"),
		TEXT("Hips"),
		TEXT("hips")
	};

	for (const FName BoneName : FallbackBoneNames)
	{
		if (CharacterMesh->GetBoneIndex(BoneName) != INDEX_NONE)
		{
			return BoneName;
		}
	}

	return NAME_None;
}

FVector AShowDownCharacter::GetRagdollHitImpulseDirection() const
{
	const FVector LocalDirection = RagdollHitLocalImpulseDirection.IsNearlyZero()
		? FVector(0.0f, -1.0f, 0.25f)
		: RagdollHitLocalImpulseDirection;

	return GetActorTransform().TransformVectorNoScale(LocalDirection).GetSafeNormal();
}

void AShowDownCharacter::StopRagdoll()
{
	if (!GetMesh() || !bRagdollActive)
	{
		return;
	}

	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetRelativeLocationAndRotation(BaseMeshRelativeLocation, BaseMeshRelativeRotation);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}

	bRagdollActive = false;
}

void AShowDownCharacter::CacheAnimBlueprintClass()
{
	if (!GetMesh() || CachedAnimBlueprintClass)
	{
		return;
	}

	CachedAnimBlueprintClass = GetMesh()->GetAnimClass();
}

void AShowDownCharacter::RestoreAnimBlueprintClass()
{
	if (!GetMesh() || !CachedAnimBlueprintClass)
	{
		return;
	}

	GetMesh()->SetAnimInstanceClass(CachedAnimBlueprintClass);
}

void AShowDownCharacter::StartActionVisual(EShowDownCharacterAnimState State)
{
	if (State == EShowDownCharacterAnimState::Idle)
	{
		StopActionVisuals();
		return;
	}

	if (State == EShowDownCharacterAnimState::Hit)
	{
		if (bRagdollActive || !GetMesh())
		{
			return;
		}

		StopActionVisuals();

		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->DisableMovement();
		}

		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();

		const FName HitBoneName = ResolveRagdollHitBoneName();
		const FVector Impulse = GetRagdollHitImpulseDirection() * FMath::Max(0.0f, RagdollHitImpulseStrength);
		if (HitBoneName != NAME_None)
		{
			GetMesh()->AddImpulse(Impulse, HitBoneName, true);
		}
		else
		{
			GetMesh()->AddImpulse(Impulse, NAME_None, true);
		}

		bRagdollActive = true;
		return;
	}

	if (TryPlayAssignedActionAnimation(State))
	{
		return;
	}

	LogAssignedActionAnimationFailure(State, GetAssignedActionAnimation(State));
}

void AShowDownCharacter::CacheBaseMeshTransform()
{
	if (!GetMesh())
	{
		return;
	}

	BaseMeshRelativeLocation = GetMesh()->GetRelativeLocation();
	BaseMeshRelativeRotation = GetMesh()->GetRelativeRotation();
}

void AShowDownCharacter::StopActionVisuals()
{
	StopRagdoll();

	if (GetMesh())
	{
		GetMesh()->SetRelativeLocationAndRotation(BaseMeshRelativeLocation, BaseMeshRelativeRotation);
	}
	RestoreAnimBlueprintClass();
}

void AShowDownCharacter::PushAnimStateToAnimInstance() const
{
	if (UShowDownCharacterAnimInstance* AnimInstance = Cast<UShowDownCharacterAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetCharacterAnimState(ReplicatedAnimState);
	}
}
