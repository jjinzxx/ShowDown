#include "ShowDownCharacter.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "ShowDownCharacterAnimInstance.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownNameTagWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ActionAnimationFallbackReturnDelay = 1.0f;
	const FName NameTagSharedLayerName(TEXT("ShowDownCharacterNameTags"));
	constexpr int32 NameTagLayerZOrder = 50;

	FString GetAnimStateDebugName(EShowDownCharacterAnimState State)
	{
		switch (State)
		{
		case EShowDownCharacterAnimState::SelectCard:
			return TEXT("SelectCard");
		case EShowDownCharacterAnimState::Betting:
			return TEXT("Betting");
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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = true;
	// Inactive table characters are hidden and collisionless. Unreal normally
	// drops such actors from network relevancy before their inactive state can
	// reach joining clients, leaving the map-authored visible copy behind.
	// There are only four fixed table characters, so always-relevant replication
	// is cheap and keeps identity/visibility/name-tag state authoritative.
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	ApplyPresentationCollisionSettings();

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	CharacterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultRobotMesh(
		TEXT("/Game/Character/Robot/robot.robot"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BuiltInHoodmanMeshFinder(
		TEXT("/Game/Character/hoodman_default_/hoodman.hoodman"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BuiltInMicuMeshFinder(
		TEXT("/Game/Character/micu/Tut_Hip_Hop_Dance__1_.Tut_Hip_Hop_Dance__1_"));
	BuiltInRobotMesh = DefaultRobotMesh.Object;
	BuiltInHoodmanMesh = BuiltInHoodmanMeshFinder.Object;
	BuiltInMicuMesh = BuiltInMicuMeshFinder.Object;
	if (BuiltInRobotMesh)
	{
		CharacterMesh->SetSkeletalMesh(BuiltInRobotMesh);
	}

	RevolverPresentationAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RevolverPresentationAnchor"));
	RevolverPresentationAnchor->SetupAttachment(CharacterMesh, TEXT("Head"));
	RevolverPresentationAnchor->SetRelativeLocation(FVector(34.0f, 26.0f, -18.0f));
	RevolverPresentationAnchor->SetRelativeRotation(FRotator::ZeroRotator);

	ForeheadCardAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ForeheadCardAnchor"));
	ForeheadCardAnchor->SetupAttachment(CharacterMesh, TEXT("Head"));
	ForeheadCardAnchor->SetRelativeLocation(FVector(18.0f, 0.0f, 8.0f));
	ForeheadCardAnchor->SetRelativeRotation(FRotator::ZeroRotator);

	NameTagWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameTag"));
	NameTagWidgetComponent->SetupAttachment(GetCapsuleComponent());
	NameTagWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 92.0f));
	NameTagWidgetComponent->SetWidgetClass(UShowDownNameTagWidget::StaticClass());
	NameTagWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameTagWidgetComponent->SetInitialSharedLayerName(NameTagSharedLayerName);
	NameTagWidgetComponent->SetInitialLayerZOrder(NameTagLayerZOrder);
	NameTagWidgetComponent->SetDrawAtDesiredSize(true);
	NameTagWidgetComponent->SetDrawSize(FVector2D(260.0f, 64.0f));
	NameTagWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	NameTagWidgetComponent->SetTickWhenOffscreen(true);
	NameTagWidgetComponent->SetManuallyRedraw(false);
	NameTagWidgetComponent->SetRedrawTime(0.0f);
	NameTagWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameTagWidgetComponent->SetGenerateOverlapEvents(false);
	NameTagWidgetComponent->SetVisibility(false);

	WorldLivesAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("WorldLivesAnchor"));
	WorldLivesAnchor->SetupAttachment(GetCapsuleComponent());
	WorldLivesAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
	WorldLivesAnchor->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	WorldLivesShadowText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorldLivesShadow"));
	WorldLivesShadowText->SetupAttachment(WorldLivesAnchor);
	WorldLivesShadowText->SetRelativeLocation(FVector(-0.20f, 0.0f, 0.0f));
	WorldLivesShadowText->SetHorizontalAlignment(EHTA_Left);
	WorldLivesShadowText->SetVerticalAlignment(EVRTA_TextCenter);
	WorldLivesShadowText->SetWorldSize(WorldLivesTextSize);
	WorldLivesShadowText->SetTextRenderColor(FColor(8, 0, 0, 230));
	WorldLivesShadowText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldLivesShadowText->SetCastShadow(false);
	WorldLivesShadowText->bAlwaysRenderAsText = true;
	WorldLivesShadowText->SetVisibility(false);

	WorldLivesText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorldLives"));
	WorldLivesText->SetupAttachment(WorldLivesAnchor);
	WorldLivesText->SetRelativeLocation(FVector::ZeroVector);
	WorldLivesText->SetHorizontalAlignment(EHTA_Left);
	WorldLivesText->SetVerticalAlignment(EVRTA_TextCenter);
	WorldLivesText->SetWorldSize(WorldLivesTextSize);
	WorldLivesText->SetTextRenderColor(FColor(245, 24, 48, 255));
	WorldLivesText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldLivesText->SetCastShadow(false);
	WorldLivesText->bAlwaysRenderAsText = true;
	WorldLivesText->SetVisibility(false);

	UFont* WorldStatusFontObject = nullptr;
	UMaterialInterface* WorldStatusOpaqueMaterialObject = nullptr;
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> WorldLivesFont(
			TEXT("/Game/UI/Font/F_ShowDownWorldHearts.F_ShowDownWorldHearts"));
		if (WorldLivesFont.Succeeded())
		{
			WorldLivesShadowText->SetFont(WorldLivesFont.Object);
			WorldLivesText->SetFont(WorldLivesFont.Object);
		}

		static ConstructorHelpers::FObjectFinder<UFont> WorldStatusFont(
			TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
		if (WorldStatusFont.Succeeded())
		{
			WorldStatusFontObject = WorldStatusFont.Object;
		}

		static ConstructorHelpers::FObjectFinder<UMaterialInterface> WorldStatusOpaqueMaterial(
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque"));
		if (WorldStatusOpaqueMaterial.Succeeded())
		{
			WorldStatusOpaqueMaterialObject = WorldStatusOpaqueMaterial.Object;
			WorldLivesShadowText->SetTextMaterial(WorldStatusOpaqueMaterialObject);
			WorldLivesText->SetTextMaterial(WorldStatusOpaqueMaterialObject);
		}
	}

	BetStatusAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BetStatusAnchor"));
	BetStatusAnchorComponent->SetupAttachment(GetCapsuleComponent());
	BetStatusAnchorComponent->SetRelativeLocation(FVector(0.0f, 80.0f, 65.0f));
	BetStatusAnchorComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	auto ConfigureWorldStatusText = [this, WorldStatusFontObject, WorldStatusOpaqueMaterialObject](
		UTextRenderComponent* TextComponent,
		const FColor& Color)
	{
		TextComponent->SetupAttachment(BetStatusAnchorComponent);
		TextComponent->SetHorizontalAlignment(EHTA_Left);
		TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
		TextComponent->SetTextRenderColor(Color);
		TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TextComponent->SetCastShadow(false);
		TextComponent->bAlwaysRenderAsText = true;
		TextComponent->SetVisibility(false);
		if (WorldStatusFontObject)
		{
			TextComponent->SetFont(WorldStatusFontObject);
		}
		if (WorldStatusOpaqueMaterialObject)
		{
			TextComponent->SetTextMaterial(WorldStatusOpaqueMaterialObject);
		}
	};

	BetStatusValueText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BetStatusValue"));
	ConfigureWorldStatusText(BetStatusValueText, FColor(255, 220, 55, 255));
	BetStatusValueText->SetRelativeLocation(FVector::ZeroVector);

	BetStatusActionText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BetStatusAction"));
	ConfigureWorldStatusText(BetStatusActionText, FColor(40, 255, 90, 255));
	BetStatusActionText->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));

	// A component-only reset effect works for every skin and every authored seat.
	// It follows the character capsule, so no per-slot Blueprint setup is needed.
	HitResetPulseLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HitResetPulseLight"));
	HitResetPulseLight->SetupAttachment(GetCapsuleComponent());
	HitResetPulseLight->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	HitResetPulseLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	HitResetPulseLight->SetInnerConeAngle(22.0f);
	HitResetPulseLight->SetOuterConeAngle(42.0f);
	HitResetPulseLight->SetAttenuationRadius(HitResetPulseRadius);
	HitResetPulseLight->SetLightColor(HitResetPulseColor);
	HitResetPulseLight->SetIntensity(0.0f);
	HitResetPulseLight->SetCastShadows(false);
	HitResetPulseLight->SetVisibility(false);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	MovementComponent->MaxWalkSpeed = 240.0f;
	MovementComponent->bEnablePhysicsInteraction = false;
	MovementComponent->DisableMovement();
}

void AShowDownCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHitRecoveryPresentation();
	UpdateWorldLifeLostPulse(DeltaSeconds);

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	if (LastPresentationLocalPlayerSlot != LocalPlayerSlot)
	{
		LastPresentationLocalPlayerSlot = LocalPlayerSlot;
		RefreshNameTag();
		RefreshWorldBetStatus();
	}
	else
	{
		SyncNameTagVisibility();
	}
}

void AShowDownCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (WorldLivesText)
	{
		WorldLivesBaseRelativeScale = WorldLivesText->GetRelativeScale3D();
	}
	ApplyCharacterSkin();
	CacheAnimBlueprintClass();
	CacheBaseMeshTransform();
	PushAnimStateToAnimInstance();
	RefreshNameTag();
	RefreshWorldBetStatus();
}

void AShowDownCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterSkin();
	CacheAnimBlueprintClass();
	CacheBaseMeshTransform();
	PushAnimStateToAnimInstance();
	ApplyCharacterSceneActive();
	RefreshNameTag();
	RefreshWorldBetStatus();
	BindToRouletteEvents();
}

void AShowDownCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromRouletteEvents();
	GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
	SetHitResetPulseStrength(0.0f);
	Super::EndPlay(EndPlayReason);
}

void AShowDownCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShowDownCharacter, ReplicatedAnimState);
	DOREPLIFETIME(AShowDownCharacter, CharacterSkinId);
	DOREPLIFETIME(AShowDownCharacter, CharacterRole);
	DOREPLIFETIME(AShowDownCharacter, PlayerSlot);
	DOREPLIFETIME(AShowDownCharacter, CharacterDisplayName);
	DOREPLIFETIME(AShowDownCharacter, CharacterLives);
	DOREPLIFETIME(AShowDownCharacter, bVoiceTalking);
	DOREPLIFETIME(AShowDownCharacter, ReplicatedPlayerViewRotation);
	DOREPLIFETIME(AShowDownCharacter, bCharacterSceneActive);
	DOREPLIFETIME(AShowDownCharacter, ReplicatedBetStatusPresentation);
	DOREPLIFETIME(AShowDownCharacter, HitRecoveryPresentationState);
}

void AShowDownCharacter::SetCharacterSkinId(const FString& NewSkinId)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Ignoring non-authoritative skin change on %s. Submit the selection through the owning PlayerController."),
			*GetName());
		return;
	}

	const FString PreviousSkinId = CharacterSkinId;
	FShowDownCharacterSkinDefinition Definition;
	FString ResolvedSkinId;
	UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
		CharacterSkinCatalog,
		NewSkinId,
		Definition,
		ResolvedSkinId);

	CharacterSkinId = ResolvedSkinId;
	ApplyCharacterSkin();
	if (CharacterSkinId != PreviousSkinId)
	{
		ForceNetUpdate();
	}
}

FString AShowDownCharacter::GetDefaultCharacterSkinId()
{
	return UShowDownCharacterSkinCatalog::GetDefaultSkinId();
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
	// Hit recovery has one authoritative clock; the optional legacy animation
	// duration is intentionally ignored so it cannot reintroduce a second timer.
	(void)Duration;
	StartHitRecoveryPresentation(CharacterLives <= 0);
}

void AShowDownCharacter::PlaySelectCardAnimation(float Duration)
{
	PlayCharacterActionAnim(EShowDownCharacterAnimState::SelectCard, Duration, true);
}

void AShowDownCharacter::PlayBettingAnimation(float Duration)
{
	PlayCharacterActionAnim(EShowDownCharacterAnimState::Betting, Duration, true);
}

void AShowDownCharacter::SetPlayerViewRotation(FRotator ViewRotation)
{
	ViewRotation.Normalize();
	ViewRotation.Roll = 0.0f;
	if (HasAuthority() && ReplicatedPlayerViewRotation.Equals(ViewRotation, 0.1f))
	{
		return;
	}

	ApplyPlayerViewRotation(ViewRotation);
	if (HasAuthority())
	{
		ReplicatedPlayerViewRotation = ViewRotation;
		ForceNetUpdate();
	}
}

FTransform AShowDownCharacter::GetRevolverPresentationTransform() const
{
	return RevolverPresentationAnchor
		? RevolverPresentationAnchor->GetComponentTransform()
		: GetActorTransform();
}

void AShowDownCharacter::StartHitRagdoll()
{
	StartHitRecoveryPresentation(CharacterLives <= 0);
}

void AShowDownCharacter::StartHitRecoveryPresentation(bool bFinalElimination)
{
	if (!HasAuthority())
	{
		// Result presentation is server-authored. Map-placed characters are not
		// client-owned, so clients wait for HitRecoveryPresentationState instead
		// of issuing an RPC that would be dropped for lack of an owning connection.
		return;
	}

	// Duplicate result notifications must not restart the fall. A later result
	// may, however, upgrade the same hit to a final elimination after lives have
	// reached zero.
	if (HitRecoveryPresentationState.bActive)
	{
		if (bFinalElimination && !HitRecoveryPresentationState.bFinalElimination)
		{
			HitRecoveryPresentationState.bFinalElimination = true;
			ForceNetUpdate();
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
	bPendingSceneDeactivateAfterHitRecovery = false;
	HitRecoveryPresentationState.Sequence = HitRecoveryPresentationState.Sequence == MAX_int32
		? 1
		: HitRecoveryPresentationState.Sequence + 1;
	HitRecoveryPresentationState.ServerStartTimeSeconds = GetSynchronizedServerTimeSeconds();
	HitRecoveryPresentationState.bActive = true;
	HitRecoveryPresentationState.bFinalElimination = bFinalElimination;
	BeginLocalHitRecoveryPresentation();
	ApplyCharacterAnimState(EShowDownCharacterAnimState::Hit);
	if (!bRagdollActive)
	{
		StartActionVisual(EShowDownCharacterAnimState::Hit);
	}
	ForceNetUpdate();
}

void AShowDownCharacter::CancelHitRecoveryPresentation(bool bRevealCharacter)
{
	if (!HasAuthority())
	{
		// Cancellation follows the same authority rule as presentation start.
		return;
	}

	HitRecoveryPresentationState.Sequence = HitRecoveryPresentationState.Sequence == MAX_int32
		? 1
		: HitRecoveryPresentationState.Sequence + 1;
	HitRecoveryPresentationState.ServerStartTimeSeconds = GetSynchronizedServerTimeSeconds();
	HitRecoveryPresentationState.bActive = false;
	// Preserve the requested terminal visibility in the replicated state. This
	// keeps Cancel(false) from being concealed only on the authority while remote
	// clients interpret the inactive presentation as a surviving character.
	HitRecoveryPresentationState.bFinalElimination = !bRevealCharacter;
	bPendingSceneDeactivateAfterHitRecovery = false;
	bHitRecoveryRagdollReset = true;
	bHitRecoverySurvivorRevealed = bRevealCharacter;
	LocalHitRecoverySequence = HitRecoveryPresentationState.Sequence;
	SetHitResetPulseStrength(0.0f);
	SetHitRecoveryVisualConcealed(!bRevealCharacter);
	ApplyCharacterAnimState(EShowDownCharacterAnimState::Idle);
	StopActionVisuals();
	ForceNetUpdate();
}

float AShowDownCharacter::GetHitRecoveryPresentationDuration() const
{
	return CalculateHitRecoveryPresentationDuration(
		HitDownedHoldDuration,
		HitResetPulseDuration,
		HitRecoveryRevealDuration);
}

float AShowDownCharacter::CalculateHitRecoveryPresentationDuration(
	float DownedHoldDuration,
	float ResetPulseDuration,
	float RecoveryRevealDuration)
{
	return FMath::Max(0.0f, DownedHoldDuration)
		+ FMath::Max(0.1f, ResetPulseDuration)
		+ FMath::Max(0.0f, RecoveryRevealDuration);
}

float AShowDownCharacter::GetHitRecoveryPresentationRemainingTime() const
{
	if (!HitRecoveryPresentationState.bActive)
	{
		return 0.0f;
	}

	const float Elapsed = FMath::Max(
		0.0f,
		GetSynchronizedServerTimeSeconds() - HitRecoveryPresentationState.ServerStartTimeSeconds);
	return FMath::Max(0.0f, GetHitRecoveryPresentationDuration() - Elapsed);
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

void AShowDownCharacter::SetCharacterLives(int32 NewLives)
{
	const int32 ClampedLives = FMath::Max(0, NewLives);
	const int32 PreviousLives = CharacterLives;
	const bool bShouldClearElimination = ClampedLives > 0
		&& HitRecoveryPresentationState.bFinalElimination;
	if (CharacterLives == ClampedLives)
	{
		if (bShouldClearElimination)
		{
			if (HasAuthority())
			{
				CancelHitRecoveryPresentation(true);
			}
			else
			{
				SetHitRecoveryVisualConcealed(false);
			}
		}
		RefreshNameTag();
		return;
	}

	CharacterLives = ClampedLives;
	if (HasAuthority()
		&& HitRecoveryPresentationState.bActive
		&& CharacterLives <= 0
		&& !HitRecoveryPresentationState.bFinalElimination)
	{
		HitRecoveryPresentationState.bFinalElimination = true;
	}
	else if (bShouldClearElimination)
	{
		if (HasAuthority())
		{
			CancelHitRecoveryPresentation(true);
		}
		else
		{
			SetHitRecoveryVisualConcealed(false);
		}
	}
	HandleCharacterLivesChanged(PreviousLives);
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AShowDownCharacter::SetVoiceTalking(bool bNewVoiceTalking)
{
	if (!HasAuthority() || bVoiceTalking == bNewVoiceTalking)
	{
		return;
	}

	bVoiceTalking = bNewVoiceTalking;
	RefreshNameTag();
	ForceNetUpdate();
}

void AShowDownCharacter::SetBetStatusPresentation(
	bool bVisible,
	const FString& DisplayName,
	const FString& StatusText,
	int32 BulletCount,
	int32 MaxBulletCount,
	bool bNeedsToMatchBet,
	const FLinearColor& AccentColor)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 NewMaxBulletCount = FMath::Clamp(MaxBulletCount, 1, 12);
	const int32 NewBulletCount = FMath::Clamp(BulletCount, 0, NewMaxBulletCount);
	const FString NewDisplayName = DisplayName.Left(32);
	const FString NewStatusText = StatusText.Left(32);
	if (ReplicatedBetStatusPresentation.bVisible == bVisible
		&& ReplicatedBetStatusPresentation.DisplayName == NewDisplayName
		&& ReplicatedBetStatusPresentation.StatusText == NewStatusText
		&& ReplicatedBetStatusPresentation.BulletCount == NewBulletCount
		&& ReplicatedBetStatusPresentation.MaxBulletCount == NewMaxBulletCount
		&& ReplicatedBetStatusPresentation.bNeedsToMatchBet == bNeedsToMatchBet
		&& ReplicatedBetStatusPresentation.AccentColor.Equals(AccentColor))
	{
		return;
	}

	ReplicatedBetStatusPresentation.bVisible = bVisible;
	ReplicatedBetStatusPresentation.DisplayName = NewDisplayName;
	ReplicatedBetStatusPresentation.StatusText = NewStatusText;
	ReplicatedBetStatusPresentation.MaxBulletCount = NewMaxBulletCount;
	ReplicatedBetStatusPresentation.BulletCount = NewBulletCount;
	ReplicatedBetStatusPresentation.bNeedsToMatchBet = bNeedsToMatchBet;
	ReplicatedBetStatusPresentation.AccentColor = AccentColor;
	OnRep_BetStatusPresentation();
	ForceNetUpdate();
}

void AShowDownCharacter::ClearBetStatusPresentation()
{
	SetBetStatusPresentation(false, FString(), FString(), 0, 6, false, FLinearColor::White);
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
	if (!bCharacterSceneActive || CharacterRole != EShowDownCharacterRole::Player)
	{
		return false;
	}

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	if (LocalPlayerSlot != EShowDownPlayerSlot::None)
	{
		return PlayerSlot == LocalPlayerSlot;
	}

	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Standalone)
	{
		return false;
	}

	return PlayerSlot == EShowDownPlayerSlot::None || PlayerSlot == EShowDownPlayerSlot::Player1;
}

bool AShowDownCharacter::IsOpponentCharacterForLocalPlayer() const
{
	if (!bCharacterSceneActive)
	{
		return false;
	}

	if (CharacterRole == EShowDownCharacterRole::Opponent)
	{
		return true;
	}

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	return LocalPlayerSlot != EShowDownPlayerSlot::None
		&& PlayerSlot != EShowDownPlayerSlot::None
		&& PlayerSlot != LocalPlayerSlot;
}

void AShowDownCharacter::SetCharacterSceneActive(bool bNewActive)
{
	if (!bNewActive
		&& HasAuthority()
		&& HitRecoveryPresentationState.bActive
		&& HitRecoveryPresentationState.bFinalElimination)
	{
		// Let every viewer see the common pulse/conceal peak before GameMode turns
		// the eliminated seat off. The pending request is honored at presentation end.
		bPendingSceneDeactivateAfterHitRecovery = true;
		return;
	}

	if (bNewActive
		&& CharacterLives > 0
		&& HitRecoveryPresentationState.bFinalElimination)
	{
		if (HasAuthority())
		{
			CancelHitRecoveryPresentation(true);
		}
		else
		{
			SetHitRecoveryVisualConcealed(false);
		}
	}

	if (bCharacterSceneActive == bNewActive)
	{
		ApplyCharacterSceneActive();
		return;
	}

	const EShowDownCharacterAnimState PreviousAnimState = ReplicatedAnimState;
	bCharacterSceneActive = bNewActive;
	if (!bCharacterSceneActive)
	{
		GetWorldTimerManager().ClearTimer(AnimStateResetTimerHandle);
		ReplicatedAnimState = EShowDownCharacterAnimState::Idle;
	}

	ApplyCharacterSceneActive();
	if (!bCharacterSceneActive && PreviousAnimState != ReplicatedAnimState)
	{
		PushAnimStateToAnimInstance();
		OnCharacterAnimStateChanged(ReplicatedAnimState);
	}

	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AShowDownCharacter::OnRep_AnimState()
{
	StartActionVisual(ReplicatedAnimState);
	PushAnimStateToAnimInstance();
	OnCharacterAnimStateChanged(ReplicatedAnimState);
}

void AShowDownCharacter::OnRep_CharacterSkinId()
{
	ApplyCharacterSkin();
}

void AShowDownCharacter::OnRep_Identity()
{
	RefreshNameTag();
	OnCharacterIdentityChanged();
}

void AShowDownCharacter::OnRep_CharacterLives(int32 PreviousLives)
{
	HandleCharacterLivesChanged(PreviousLives);
}

void AShowDownCharacter::OnRep_ViewRotation()
{
	ApplyPlayerViewRotation(ReplicatedPlayerViewRotation);
}

void AShowDownCharacter::OnRep_SceneActive()
{
	ApplyCharacterSceneActive();
	RefreshNameTag();
}

void AShowDownCharacter::OnRep_BetStatusPresentation()
{
	RefreshWorldBetStatus();
}

void AShowDownCharacter::OnRep_HitRecoveryPresentationState()
{
	if (HitRecoveryPresentationState.bActive)
	{
		// A non-final hit can be upgraded to a final elimination without starting
		// a new sequence. Preserve the current conceal/ragdoll phase in that case.
		if (LocalHitRecoverySequence != HitRecoveryPresentationState.Sequence)
		{
			BeginLocalHitRecoveryPresentation();
		}
		UpdateHitRecoveryPresentation();
		return;
	}

	LocalHitRecoverySequence = HitRecoveryPresentationState.Sequence;
	bHitRecoveryRagdollReset = true;
	bHitRecoverySurvivorRevealed = !HitRecoveryPresentationState.bFinalElimination;
	SetHitResetPulseStrength(0.0f);
	StopActionVisuals();
	SetHitRecoveryVisualConcealed(HitRecoveryPresentationState.bFinalElimination);
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

	// The single-player result arrives immediately before its life-change event,
	// so one remaining life means this impact is the final elimination.
	StartHitRecoveryPresentation(CharacterLives <= 1);
}

void AShowDownCharacter::HandleLifeChanged(EShowDownSide Target, int32 Life)
{
	if (ShouldReactToSingleRouletteTarget(Target))
	{
		SetCharacterLives(Life);
	}
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
	if (PlayerSlot != EShowDownPlayerSlot::None && PlayerSlot == TargetSlot)
	{
		SetCharacterLives(RemainingLives);
	}

	if (!HasAuthority() || !bHit || !ShouldReactToMultiplayerRouletteTarget(TargetSlot))
	{
		return;
	}

	StartHitRecoveryPresentation(RemainingLives <= 0);
}

void AShowDownCharacter::HandleCardSelected(EShowDownSide Side)
{
	if (!HasAuthority() || !ShouldReactToSingleRouletteTarget(Side))
	{
		return;
	}

	PlaySelectCardAnimation();
}

void AShowDownCharacter::HandleBetActionCommitted(EShowDownSide Side, EShowDownBetAction Action, int32 TargetBet)
{
	if (!HasAuthority() || !ShouldReactToSingleRouletteTarget(Side))
	{
		return;
	}

	PlayBettingAnimation();
}

void AShowDownCharacter::HandleMultiplayerCardSelected(EShowDownPlayerSlot Slot)
{
	if (!HasAuthority() || !ShouldReactToMultiplayerRouletteTarget(Slot))
	{
		return;
	}

	PlaySelectCardAnimation();
}

void AShowDownCharacter::HandleMultiplayerBetActionCommitted(
	EShowDownPlayerSlot Slot,
	EShowDownBetAction Action,
	int32 TargetBet)
{
	if (!HasAuthority() || !ShouldReactToMultiplayerRouletteTarget(Slot))
	{
		return;
	}

	PlayBettingAnimation();
}

void AShowDownCharacter::ServerSetCharacterIdentity_Implementation(
	EShowDownCharacterRole NewRole,
	EShowDownPlayerSlot NewPlayerSlot,
	const FString& NewDisplayName)
{
	SetCharacterIdentity(NewRole, NewPlayerSlot, NewDisplayName);
}

void AShowDownCharacter::ApplyCharacterSkin()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		CharacterSkinId = UShowDownCharacterSkinCatalog::GetDefaultSkinId();
		return;
	}

	FShowDownCharacterSkinDefinition Definition;
	FString ResolvedSkinId;
	UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
		CharacterSkinCatalog,
		CharacterSkinId,
		Definition,
		ResolvedSkinId);

	USkeletalMesh* ResolvedMesh = Definition.SkeletalMesh.LoadSynchronous();
	if (!ResolvedMesh)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Character skin '%s' on %s could not load. Falling back to the built-in robot."),
			*ResolvedSkinId,
			*GetName());

		ResolvedSkinId = UShowDownCharacterSkinCatalog::GetDefaultSkinId();
		if (UShowDownCharacterSkinCatalog::FindBuiltInSkinDefinition(ResolvedSkinId, Definition))
		{
			ResolvedMesh = Definition.SkeletalMesh.LoadSynchronous();
		}
	}

	CharacterSkinId = ResolvedSkinId;
	if (!ResolvedMesh)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Default robot character mesh could not load for %s. Keeping its current mesh."),
			*GetName());
		return;
	}

	const bool bMeshChanged = CharacterMesh->GetSkeletalMeshAsset() != ResolvedMesh;
	const bool bSkinChanged = AppliedCharacterSkinId != ResolvedSkinId;
	if (!bMeshChanged && !bSkinChanged)
	{
		return;
	}

	if (bRagdollActive)
	{
		StopRagdoll();
	}

	CacheAnimBlueprintClass();
	if (bMeshChanged)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			if (ActiveActionMontage)
			{
				AnimInstance->Montage_Stop(ActionAnimationBlendOutTime, ActiveActionMontage);
			}
		}
		ActiveActionMontage = nullptr;
		CharacterMesh->SetSkeletalMesh(ResolvedMesh, true);
		RestoreAnimBlueprintClass();
		PushAnimStateToAnimInstance();
	}

	AppliedCharacterSkinId = ResolvedSkinId;
	OnCharacterSkinChanged(ResolvedSkinId);
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

	ShowDownGameState->OnCardSelected.AddUniqueDynamic(this, &AShowDownCharacter::HandleCardSelected);
	ShowDownGameState->OnBetActionCommitted.AddUniqueDynamic(this, &AShowDownCharacter::HandleBetActionCommitted);
	ShowDownGameState->OnRouletteStarted.AddUniqueDynamic(this, &AShowDownCharacter::HandleRouletteStarted);
	ShowDownGameState->OnRouletteResult.AddUniqueDynamic(this, &AShowDownCharacter::HandleRouletteResult);
	ShowDownGameState->OnLifeChanged.AddUniqueDynamic(this, &AShowDownCharacter::HandleLifeChanged);
	ShowDownGameState->OnMultiplayerCardSelected.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerCardSelected);
	ShowDownGameState->OnMultiplayerBetActionCommitted.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerBetActionCommitted);
	ShowDownGameState->OnMultiplayerRouletteStarted.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteStarted);
	ShowDownGameState->OnMultiplayerRouletteResult.AddUniqueDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteResult);
	ShowDownGameState->OnNameTagRoundStatusChanged.AddUniqueDynamic(this, &AShowDownCharacter::HandleNameTagRoundStatusChanged);
	ShowDownGameState->OnChatMessageReceived.AddUniqueDynamic(this, &AShowDownCharacter::HandleChatMessageReceived);
}

void AShowDownCharacter::UnbindFromRouletteEvents()
{
	UWorld* World = GetWorld();
	AShowDownGameStateBase* ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	if (!ShowDownGameState)
	{
		return;
	}

	ShowDownGameState->OnCardSelected.RemoveDynamic(this, &AShowDownCharacter::HandleCardSelected);
	ShowDownGameState->OnBetActionCommitted.RemoveDynamic(this, &AShowDownCharacter::HandleBetActionCommitted);
	ShowDownGameState->OnRouletteStarted.RemoveDynamic(this, &AShowDownCharacter::HandleRouletteStarted);
	ShowDownGameState->OnRouletteResult.RemoveDynamic(this, &AShowDownCharacter::HandleRouletteResult);
	ShowDownGameState->OnLifeChanged.RemoveDynamic(this, &AShowDownCharacter::HandleLifeChanged);
	ShowDownGameState->OnMultiplayerCardSelected.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerCardSelected);
	ShowDownGameState->OnMultiplayerBetActionCommitted.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerBetActionCommitted);
	ShowDownGameState->OnMultiplayerRouletteStarted.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteStarted);
	ShowDownGameState->OnMultiplayerRouletteResult.RemoveDynamic(this, &AShowDownCharacter::HandleMultiplayerRouletteResult);
	ShowDownGameState->OnNameTagRoundStatusChanged.RemoveDynamic(this, &AShowDownCharacter::HandleNameTagRoundStatusChanged);
	ShowDownGameState->OnChatMessageReceived.RemoveDynamic(this, &AShowDownCharacter::HandleChatMessageReceived);
}

void AShowDownCharacter::HandleNameTagRoundStatusChanged()
{
	RefreshNameTag();
}

void AShowDownCharacter::HandleChatMessageReceived(const FString& SenderName, const FString& Message)
{
	if (!NameTagWidgetComponent || !ShouldShowOverheadChatMessage(SenderName))
	{
		return;
	}

	const FString TrimmedMessage = Message.TrimStartAndEnd();
	if (TrimmedMessage.IsEmpty())
	{
		return;
	}

	RefreshNameTag();
	if (UShowDownNameTagWidget* NameTagWidget = Cast<UShowDownNameTagWidget>(NameTagWidgetComponent->GetUserWidgetObject()))
	{
		NameTagWidget->ShowOverheadChatMessage(FText::FromString(TrimmedMessage));
	}
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
	case EShowDownCharacterAnimState::SelectCard:
	case EShowDownCharacterAnimState::Betting:
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
	if (!bCharacterSceneActive)
	{
		return false;
	}

	switch (Target)
	{
	case EShowDownSide::Player:
		return CharacterRole == EShowDownCharacterRole::Player;
	case EShowDownSide::Collector:
		return CharacterRole == EShowDownCharacterRole::Opponent;
	default:
		return false;
	}
}

bool AShowDownCharacter::ShouldReactToMultiplayerRouletteTarget(EShowDownPlayerSlot TargetSlot) const
{
	if (!bCharacterSceneActive)
	{
		return false;
	}

	return TargetSlot != EShowDownPlayerSlot::None && PlayerSlot == TargetSlot;
}

UAnimationAsset* AShowDownCharacter::GetAssignedActionAnimation(EShowDownCharacterAnimState State) const
{
	switch (State)
	{
	case EShowDownCharacterAnimState::SelectCard:
		return SelectCardAnimationAsset;
	case EShowDownCharacterAnimState::Betting:
		return BettingAnimationAsset;
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

	RestoreAnimBlueprintClass();
	if (UAnimSequenceBase* SequenceAsset = Cast<UAnimSequenceBase>(AnimationAsset))
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (ActiveActionMontage)
			{
				AnimInstance->Montage_Stop(ActionAnimationBlendOutTime, ActiveActionMontage);
				ActiveActionMontage = nullptr;
			}

			ActiveActionMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
				SequenceAsset,
				ActionMontageSlotName,
				ActionAnimationBlendInTime,
				ActionAnimationBlendOutTime);
			if (ActiveActionMontage)
			{
				return true;
			}
		}
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
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyPresentationCollisionSettings();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	bRagdollActive = false;
}

void AShowDownCharacter::BeginLocalHitRecoveryPresentation()
{
	LocalHitRecoverySequence = HitRecoveryPresentationState.Sequence;
	bHitRecoveryRagdollReset = false;
	bHitRecoverySurvivorRevealed = false;
	SetHitResetPulseStrength(0.0f);
	SetHitRecoveryVisualConcealed(false);
}

void AShowDownCharacter::UpdateHitRecoveryPresentation()
{
	if (!HitRecoveryPresentationState.bActive)
	{
		return;
	}

	if (LocalHitRecoverySequence != HitRecoveryPresentationState.Sequence)
	{
		BeginLocalHitRecoveryPresentation();
	}

	const float DownedHold = FMath::Max(0.0f, HitDownedHoldDuration);
	const float PulseDuration = FMath::Max(0.1f, HitResetPulseDuration);
	const float PulseStartTime = DownedHold;
	const float ConcealTime = PulseStartTime + PulseDuration * 0.5f;
	const float RevealTime = PulseStartTime + PulseDuration * 0.85f;
	const float PulseEndTime = PulseStartTime + PulseDuration;
	const float Elapsed = FMath::Max(
		0.0f,
		GetSynchronizedServerTimeSeconds() - HitRecoveryPresentationState.ServerStartTimeSeconds);

	if (Elapsed >= PulseStartTime && Elapsed < PulseEndTime)
	{
		const float PulseAlpha = FMath::Clamp((Elapsed - PulseStartTime) / PulseDuration, 0.0f, 1.0f);
		SetHitResetPulseStrength(FMath::Sin(PI * PulseAlpha));
	}
	else
	{
		SetHitResetPulseStrength(0.0f);
	}

	if (!bHitRecoveryStatusConcealed
		&& Elapsed >= FMath::Min(DownedHold, FMath::Max(0.05f, WorldLifeLostPulseDuration)))
	{
		// Show the old heart count long enough for one clear pop, then remove
		// floating status from above a body that has fallen away from its seat.
		SetHitRecoveryStatusConcealed(true);
	}

	if (!bHitRecoveryRagdollReset && Elapsed >= ConcealTime)
	{
		// Physics is returned to the authored seat transform only while the common
		// light peak hides the mesh. Observers never see the old five-second snap.
		SetHitRecoveryVisualConcealed(true);
		bHitRecoveryRagdollReset = true;
		if (HasAuthority())
		{
			ApplyCharacterAnimState(EShowDownCharacterAnimState::Idle);
		}
		else
		{
			StopActionVisuals();
		}
	}

	if (HitRecoveryPresentationState.bFinalElimination)
	{
		if (bHitRecoveryRagdollReset && bHitRecoverySurvivorRevealed)
		{
			bHitRecoverySurvivorRevealed = false;
			SetHitRecoveryVisualConcealed(true);
		}
	}
	else if (bHitRecoveryRagdollReset
		&& !bHitRecoverySurvivorRevealed
		&& Elapsed >= RevealTime)
	{
		bHitRecoverySurvivorRevealed = true;
		SetHitRecoveryVisualConcealed(false);
	}

	if (HasAuthority() && Elapsed >= GetHitRecoveryPresentationDuration())
	{
		CompleteHitRecoveryPresentationAuthority();
	}
}

void AShowDownCharacter::CompleteHitRecoveryPresentationAuthority()
{
	if (!HasAuthority() || !HitRecoveryPresentationState.bActive)
	{
		return;
	}

	const bool bFinalElimination = HitRecoveryPresentationState.bFinalElimination;
	const bool bDeactivateScene = bPendingSceneDeactivateAfterHitRecovery;
	HitRecoveryPresentationState.bActive = false;
	bPendingSceneDeactivateAfterHitRecovery = false;
	bHitRecoveryRagdollReset = true;
	bHitRecoverySurvivorRevealed = !bFinalElimination;
	SetHitResetPulseStrength(0.0f);
	SetHitRecoveryVisualConcealed(bFinalElimination);
	ApplyCharacterAnimState(EShowDownCharacterAnimState::Idle);
	StopActionVisuals();
	ForceNetUpdate();

	if (bDeactivateScene)
	{
		SetCharacterSceneActive(false);
	}
}

void AShowDownCharacter::SetHitRecoveryVisualConcealed(bool bConcealed)
{
	bHitRecoveryVisualConcealed = bConcealed;
	SetHitRecoveryStatusConcealed(bConcealed);
	const bool bShowMesh = bCharacterSceneActive && !bHitRecoveryVisualConcealed;
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetHiddenInGame(!bShowMesh, true);
		CharacterMesh->SetVisibility(bShowMesh, true);
	}

	// Status visibility is refreshed by SetHitRecoveryStatusConcealed.
}

void AShowDownCharacter::SetHitRecoveryStatusConcealed(bool bConcealed)
{
	if (bHitRecoveryStatusConcealed == bConcealed && bNameTagVisibilityInitialized)
	{
		return;
	}

	bHitRecoveryStatusConcealed = bConcealed;
	bNameTagVisibilityInitialized = false;
	RefreshNameTag();
	RefreshWorldBetStatus();
}

void AShowDownCharacter::SetHitResetPulseStrength(float Strength)
{
	if (!HitResetPulseLight || IsRunningDedicatedServer())
	{
		return;
	}

	const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	const bool bVisible = bCharacterSceneActive
		&& HitRecoveryPresentationState.bActive
		&& ClampedStrength > KINDA_SMALL_NUMBER;
	HitResetPulseLight->SetLightColor(HitResetPulseColor);
	HitResetPulseLight->SetAttenuationRadius(FMath::Max(50.0f, HitResetPulseRadius));
	HitResetPulseLight->SetIntensity(FMath::Max(0.0f, HitResetPulsePeakIntensity) * ClampedStrength);
	HitResetPulseLight->SetVisibility(bVisible, true);
	HitResetPulseLight->SetHiddenInGame(!bVisible, true);
}

float AShowDownCharacter::GetSynchronizedServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void AShowDownCharacter::StartWorldLifeLostPulse(int32 PreviousLives)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (bWorldLifeLostPulseActive && WorldLifeLostDisplayedLives == FMath::Max(0, PreviousLives))
	{
		return;
	}

	WorldLifeLostDisplayedLives = FMath::Max(0, PreviousLives);
	WorldLifeLostPulseElapsedTime = 0.0f;
	bWorldLifeLostPulseActive = WorldLifeLostDisplayedLives > 0;
	ResetWorldLifeLostPulseVisual();
}

void AShowDownCharacter::UpdateWorldLifeLostPulse(float DeltaSeconds)
{
	if (!bWorldLifeLostPulseActive || !WorldLivesText || IsRunningDedicatedServer())
	{
		return;
	}

	WorldLifeLostPulseElapsedTime += FMath::Max(0.0f, DeltaSeconds);
	const float SafeDuration = FMath::Max(0.05f, WorldLifeLostPulseDuration);
	const float Alpha = FMath::Clamp(WorldLifeLostPulseElapsedTime / SafeDuration, 0.0f, 1.0f);
	const float Pulse = FMath::Sin(PI * Alpha);
	WorldLivesText->SetRelativeScale3D(
		WorldLivesBaseRelativeScale * (1.0f + FMath::Max(0.0f, WorldLifeLostPulseScale) * Pulse));
	WorldLivesText->SetTextRenderColor(FLinearColor::LerpUsingHSV(
		FLinearColor(1.0f, 0.92f, 0.92f, 1.0f),
		FLinearColor(245.0f / 255.0f, 24.0f / 255.0f, 48.0f / 255.0f, 1.0f),
		Alpha).ToFColor(true));

	if (Alpha < 1.0f)
	{
		return;
	}

	bWorldLifeLostPulseActive = false;
	WorldLifeLostDisplayedLives = INDEX_NONE;
	ResetWorldLifeLostPulseVisual();
	RefreshWorldLives();
}

void AShowDownCharacter::ResetWorldLifeLostPulseVisual()
{
	if (!WorldLivesText)
	{
		return;
	}

	WorldLivesText->SetRelativeScale3D(WorldLivesBaseRelativeScale);
	WorldLivesText->SetTextRenderColor(FColor(245, 24, 48, 255));
}

void AShowDownCharacter::HandleCharacterLivesChanged(int32 PreviousLives)
{
	if (CharacterLives < PreviousLives)
	{
		StartWorldLifeLostPulse(PreviousLives);
	}
	else if (CharacterLives > PreviousLives)
	{
		bWorldLifeLostPulseActive = false;
		WorldLifeLostDisplayedLives = INDEX_NONE;
		ResetWorldLifeLostPulseVisual();
	}

	RefreshNameTag();
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
	if (!bCharacterSceneActive)
	{
		StopActionVisuals();
		return;
	}

	if (State == EShowDownCharacterAnimState::Idle)
	{
		// Replicated Idle can arrive a frame before this client's synchronized
		// presentation tick reaches the conceal peak. Hide first so the ragdoll-to-
		// authored-pose reset is never exposed by packet or frame ordering.
		if (HitRecoveryPresentationState.bActive && !bHitRecoveryRagdollReset)
		{
			SetHitRecoveryVisualConcealed(true);
			bHitRecoveryRagdollReset = true;
		}
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
	if (GetMesh())
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (ActiveActionMontage)
			{
				AnimInstance->Montage_Stop(ActionAnimationBlendOutTime, ActiveActionMontage);
				ActiveActionMontage = nullptr;
			}
		}
	}

	StopRagdoll();

	if (GetMesh())
	{
		GetMesh()->SetRelativeLocationAndRotation(BaseMeshRelativeLocation, BaseMeshRelativeRotation);
	}
	RestoreAnimBlueprintClass();
}

void AShowDownCharacter::PushAnimStateToAnimInstance() const
{
	const USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	if (UShowDownCharacterAnimInstance* AnimInstance = Cast<UShowDownCharacterAnimInstance>(CharacterMesh->GetAnimInstance()))
	{
		AnimInstance->SetCharacterAnimState(ReplicatedAnimState);
	}
}

FName AShowDownCharacter::ResolvePlayerCameraAttachName() const
{
	const USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return PlayerCameraAttachName;
	}

	if (PlayerCameraAttachName != NAME_None
		&& (CharacterMesh->DoesSocketExist(PlayerCameraAttachName)
			|| CharacterMesh->GetBoneIndex(PlayerCameraAttachName) != INDEX_NONE))
	{
		return PlayerCameraAttachName;
	}

	static const FName FallbackAttachNames[] =
	{
		TEXT("Head"),
		TEXT("head"),
		TEXT("Neck"),
		TEXT("neck"),
		TEXT("Spine2"),
		TEXT("spine2"),
		TEXT("Spine"),
		TEXT("spine")
	};

	for (const FName AttachName : FallbackAttachNames)
	{
		if (CharacterMesh->DoesSocketExist(AttachName) || CharacterMesh->GetBoneIndex(AttachName) != INDEX_NONE)
		{
			return AttachName;
		}
	}

	return NAME_None;
}

void AShowDownCharacter::ApplyPlayerViewRotation(FRotator ViewRotation)
{
	ViewRotation.Roll = 0.0f;
	ReplicatedPlayerViewRotation = ViewRotation;

	const FRotator ActorRotation = GetActorRotation();
	HeadLookPitch = FMath::Clamp(
		-FRotator::NormalizeAxis(ViewRotation.Pitch - ActorRotation.Pitch),
		-MaxHeadLookPitch,
		MaxHeadLookPitch);
	HeadLookYaw = FMath::Clamp(
		FRotator::NormalizeAxis(ViewRotation.Yaw - ActorRotation.Yaw),
		-MaxHeadLookYaw,
		MaxHeadLookYaw);
}

void AShowDownCharacter::ApplyCharacterSceneActive()
{
	const bool bActive = bCharacterSceneActive;

	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);

	if (!bActive)
	{
		StopActionVisuals();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ApplyPresentationCollisionSettings();
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		const bool bShowMesh = bActive && !bHitRecoveryVisualConcealed;
		CharacterMesh->SetHiddenInGame(!bShowMesh, true);
		CharacterMesh->SetVisibility(bShowMesh, true);

		if (!bActive)
		{
			CharacterMesh->SetAllBodiesSimulatePhysics(false);
			CharacterMesh->SetSimulatePhysics(false);
			CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else if (!bRagdollActive)
		{
			CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}
	if (!bActive)
	{
		SetHitResetPulseStrength(0.0f);
	}

	RefreshNameTag();
	RefreshWorldBetStatus();
}

void AShowDownCharacter::ApplyPresentationCollisionSettings()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetGenerateOverlapEvents(false);
}

void AShowDownCharacter::RefreshNameTag()
{
	if (!NameTagWidgetComponent)
	{
		return;
	}

	// Location is authored on the component. Do not overwrite it here: designers
	// must be able to move the name tag in the Blueprint/component editor.
	BindNameTagToLocalPlayer();
	NameTagWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameTagWidgetComponent->SetInitialSharedLayerName(NameTagSharedLayerName);
	NameTagWidgetComponent->SetInitialLayerZOrder(NameTagLayerZOrder);
	NameTagWidgetComponent->SetTickWhenOffscreen(true);
	NameTagWidgetComponent->SetManuallyRedraw(false);
	NameTagWidgetComponent->SetRedrawTime(0.0f);
	if (NameTagWidgetComponent->GetWidgetClass() != UShowDownNameTagWidget::StaticClass())
	{
		NameTagWidgetComponent->SetWidgetClass(UShowDownNameTagWidget::StaticClass());
	}

	NameTagWidgetComponent->InitWidget();
	ApplyNameTagWidgetContent();

	SyncNameTagVisibility();
	RefreshWorldLives();
}

void AShowDownCharacter::SyncNameTagVisibility()
{
	if (!NameTagWidgetComponent)
	{
		return;
	}

	BindNameTagToLocalPlayer();

	const bool bVisible = ShouldShowNameTag();
	const bool bVisibleWidgetMissing = bVisible
		&& NameTagWidgetComponent->GetUserWidgetObject() == nullptr;
	const bool bHiddenInGameMismatch = NameTagWidgetComponent->bHiddenInGame == bVisible;
	const bool bVisibleTickNeedsRepair = bVisible
		&& (!NameTagWidgetComponent->IsComponentTickEnabled()
			|| !NameTagWidgetComponent->GetTickWhenOffscreen()
			|| NameTagWidgetComponent->GetManuallyRedraw());
	if (!bNameTagVisibilityInitialized
		|| bLastNameTagVisible != bVisible
		|| NameTagWidgetComponent->IsVisible() != bVisible
		|| bHiddenInGameMismatch
		|| bVisibleWidgetMissing
		|| bVisibleTickNeedsRepair)
	{
		NameTagWidgetComponent->SetVisibility(bVisible, true);
		NameTagWidgetComponent->SetHiddenInGame(!bVisible, true);
		if (bVisible)
		{
			NameTagWidgetComponent->SetComponentTickEnabled(true);
			NameTagWidgetComponent->SetTickWhenOffscreen(true);
			NameTagWidgetComponent->SetManuallyRedraw(false);
			NameTagWidgetComponent->SetRedrawTime(0.0f);
			NameTagWidgetComponent->RequestRenderUpdate();
		}
		bLastNameTagVisible = bVisible;
		bNameTagVisibilityInitialized = true;
	}

	if (bVisible)
	{
		// Replicated multiplayer characters can reach PostInitializeComponents
		// before Slate/local-player screen layers are ready. Retry creation here
		// and immediately populate the new widget instead of leaving it empty.
		const bool bHadWidget = NameTagWidgetComponent->GetUserWidgetObject() != nullptr;
		NameTagWidgetComponent->InitWidget();
		if (!bHadWidget && NameTagWidgetComponent->GetUserWidgetObject())
		{
			ApplyNameTagWidgetContent();
		}
		NameTagWidgetComponent->SetComponentTickEnabled(true);
		NameTagWidgetComponent->RequestRenderUpdate();
	}
}

void AShowDownCharacter::BindNameTagToLocalPlayer()
{
	if (!NameTagWidgetComponent || IsRunningDedicatedServer())
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	if (LocalPlayer)
	{
		// Screen-space widget components fall back to the first local player when
		// OwnerPlayer is null. During multiplayer travel that fallback can happen
		// before the local PlayerController is attached, leaving bAddedToScreen
		// associated with a stale game layer. Explicit ownership removes the stale
		// registration once and lets the component add itself to the ready layer.
		NameTagWidgetComponent->SetOwnerPlayer(LocalPlayer);
	}
}

void AShowDownCharacter::ApplyNameTagWidgetContent()
{
	if (UShowDownNameTagWidget* NameTagWidget = NameTagWidgetComponent
		? Cast<UShowDownNameTagWidget>(NameTagWidgetComponent->GetUserWidgetObject())
		: nullptr)
	{
		NameTagWidget->SetDisplayName(FText::FromString(ResolveNameTagDisplayName()));
		NameTagWidget->SetStatusText(FText::GetEmpty());
		NameTagWidget->SetTurnActive(IsNameTagTurnActive());
		NameTagWidget->SetSpeakingIndicatorVisible(bVoiceTalking);
	}
}

void AShowDownCharacter::RefreshWorldLives()
{
	if (!WorldLivesAnchor || !WorldLivesText || !WorldLivesShadowText)
	{
		return;
	}

	// Text children stay canonical; designers position/rotate WorldLivesAnchor.
	// The anchor remains attached to the character and therefore follows seat yaw.
	WorldLivesText->SetRelativeLocation(FVector::ZeroVector);
	WorldLivesText->SetRelativeRotation(FRotator::ZeroRotator);
	WorldLivesShadowText->SetRelativeLocation(FVector(-0.2f, 0.0f, 0.0f));
	WorldLivesShadowText->SetRelativeRotation(FRotator::ZeroRotator);

	if (!IsRunningDedicatedServer())
	{
		if (UFont* HeartFont = LoadObject<UFont>(
			nullptr,
			TEXT("/Game/UI/Font/F_ShowDownWorldHearts.F_ShowDownWorldHearts")))
		{
			WorldLivesText->SetFont(HeartFont);
			WorldLivesShadowText->SetFont(HeartFont);
		}
		if (UMaterialInterface* OpaqueTextMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")))
		{
			WorldLivesText->SetTextMaterial(OpaqueTextMaterial);
			WorldLivesShadowText->SetTextMaterial(OpaqueTextMaterial);
		}
	}
	const int32 DisplayedLives = bWorldLifeLostPulseActive
		? FMath::Max(CharacterLives, WorldLifeLostDisplayedLives)
		: CharacterLives;
	FString Hearts;
	for (int32 LifeIndex = 0; LifeIndex < DisplayedLives; ++LifeIndex)
	{
		Hearts.AppendChar(static_cast<TCHAR>(0x2665));
	}

	const FText HeartsText = FText::FromString(Hearts);
	WorldLivesText->SetText(HeartsText);
	WorldLivesShadowText->SetText(FText::GetEmpty());
	const float HeartSize = FMath::Clamp(WorldLivesTextSize, 4.0f, 16.0f);
	WorldLivesText->SetWorldSize(HeartSize);
	WorldLivesShadowText->SetWorldSize(HeartSize);
	WorldLivesText->SetHorizontalAlignment(EHTA_Left);
	WorldLivesShadowText->SetHorizontalAlignment(EHTA_Left);
	// Overhead presentation belongs to characters the local player can see.
	// Rendering it for the local first-person character leaves orphaned hearts/status
	// in the middle of the screen while the local name tag is intentionally hidden.
	const bool bVisible = ShouldShowNameTag() && DisplayedLives > 0;
	WorldLivesText->SetVisibility(bVisible, true);
	WorldLivesText->SetHiddenInGame(!bVisible, true);
	// The old enlarged duplicate produced a soft/doubled silhouette. The opaque
	// text material is crisp enough on its own, so keep the legacy component off.
	WorldLivesShadowText->SetVisibility(false, true);
	WorldLivesShadowText->SetHiddenInGame(true, true);
}

void AShowDownCharacter::RefreshWorldBetStatus()
{
	if (!BetStatusAnchorComponent
		|| !BetStatusValueText
		|| !BetStatusActionText)
	{
		return;
	}

	// Text children stay canonical; designers position/rotate BetStatusAnchor.
	BetStatusValueText->SetRelativeLocation(FVector::ZeroVector);
	BetStatusValueText->SetRelativeRotation(FRotator::ZeroRotator);
	BetStatusActionText->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));
	BetStatusActionText->SetRelativeRotation(FRotator::ZeroRotator);

	if (!IsRunningDedicatedServer())
	{
		if (UFont* StatusFont = LoadObject<UFont>(
			nullptr,
			TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField")))
		{
			BetStatusValueText->SetFont(StatusFont);
			BetStatusActionText->SetFont(StatusFont);
		}
		if (UMaterialInterface* OpaqueTextMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")))
		{
			BetStatusValueText->SetTextMaterial(OpaqueTextMaterial);
			BetStatusActionText->SetTextMaterial(OpaqueTextMaterial);
		}
	}
	const float ValueSize = FMath::Clamp(BetStatusValueTextSize, 4.0f, 10.0f);
	const float ActionSize = FMath::Clamp(BetStatusActionTextSize, 4.0f, 12.0f);
	BetStatusValueText->SetWorldSize(ValueSize);
	BetStatusActionText->SetWorldSize(ActionSize);
	BetStatusValueText->SetHorizontalAlignment(EHTA_Left);
	BetStatusActionText->SetHorizontalAlignment(EHTA_Left);

	const FString ValueString = FString::Printf(
		TEXT("%d/%d"),
		FMath::Clamp(ReplicatedBetStatusPresentation.BulletCount, 0, ReplicatedBetStatusPresentation.MaxBulletCount),
		FMath::Max(1, ReplicatedBetStatusPresentation.MaxBulletCount));
	const FText Value = FText::FromString(ValueString);
	const FText Action = FText::FromString(ReplicatedBetStatusPresentation.StatusText.TrimStartAndEnd());
	BetStatusValueText->SetText(Value);
	BetStatusActionText->SetText(Action);

	const bool bFolded = ReplicatedBetStatusPresentation.StatusText.StartsWith(
		TEXT("FOLD"),
		ESearchCase::IgnoreCase);
	const FLinearColor ValueColor = bFolded
		? FLinearColor(0.78f, 0.16f, 0.14f, 1.0f)
		: (ReplicatedBetStatusPresentation.bNeedsToMatchBet
			? FLinearColor::White
			: FLinearColor(0.12f, 1.0f, 0.28f, 1.0f));
	BetStatusValueText->SetTextRenderColor(ValueColor.ToFColor(true));
	BetStatusActionText->SetTextRenderColor(ReplicatedBetStatusPresentation.AccentColor.ToFColor(true));

	const bool bVisible =
		ShouldShowNameTag()
		&& ReplicatedBetStatusPresentation.bVisible
		&& !ReplicatedBetStatusPresentation.DisplayName.TrimStartAndEnd().IsEmpty();
	const bool bActionVisible = bVisible && !ReplicatedBetStatusPresentation.StatusText.TrimStartAndEnd().IsEmpty();
	BetStatusValueText->SetVisibility(bVisible, true);
	BetStatusValueText->SetHiddenInGame(!bVisible, true);
	BetStatusActionText->SetVisibility(bActionVisible, true);
	BetStatusActionText->SetHiddenInGame(!bActionVisible, true);
}

FString AShowDownCharacter::ResolveNameTagDisplayName() const
{
	const FString TrimmedName = CharacterDisplayName.TrimStartAndEnd();
	if (!TrimmedName.IsEmpty())
	{
		return TrimmedName.Left(32);
	}

	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Standalone && CharacterRole == EShowDownCharacterRole::Opponent)
	{
		return TEXT("김윤아");
	}

	if (PlayerSlot != EShowDownPlayerSlot::None)
	{
		return FString::Printf(TEXT("Player %d"), static_cast<int32>(PlayerSlot));
	}

	return FString();
}

FString AShowDownCharacter::ResolveNameTagStatusText() const
{
	const UWorld* World = GetWorld();
	const AShowDownGameStateBase* ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	if (!ShowDownGameState)
	{
		return FString();
	}

	int32 LoadedBulletCount = ShowDownGameState->NameTagLoadedBulletCount;
	if (ShowDownGameState->IsMultiplayerMatch())
	{
		for (const FShowDownNameTagPlayerBetState& PlayerBet : ShowDownGameState->NameTagPlayerBets)
		{
			if (PlayerBet.Slot == PlayerSlot)
			{
				LoadedBulletCount = PlayerBet.LoadedBulletCount;
				break;
			}
		}
	}
	else if (CharacterRole == EShowDownCharacterRole::Opponent)
	{
		LoadedBulletCount = ShowDownGameState->NameTagCollectorLoadedBulletCount;
	}
	else if (CharacterRole == EShowDownCharacterRole::Player)
	{
		LoadedBulletCount = ShowDownGameState->NameTagPlayerLoadedBulletCount;
	}

	return FString::Printf(TEXT("%d발"), FMath::Clamp(LoadedBulletCount, 0, 6));
}

bool AShowDownCharacter::IsNameTagTurnActive() const
{
	const UWorld* World = GetWorld();
	const AShowDownGameStateBase* ShowDownGameState = World ? World->GetGameState<AShowDownGameStateBase>() : nullptr;
	if (!ShowDownGameState || ShowDownGameState->NameTagTurnSlot == EShowDownPlayerSlot::None)
	{
		return false;
	}

	if (ShowDownGameState->IsMultiplayerMatch())
	{
		return PlayerSlot != EShowDownPlayerSlot::None && PlayerSlot == ShowDownGameState->NameTagTurnSlot;
	}

	if (CharacterRole == EShowDownCharacterRole::Opponent)
	{
		return ShowDownGameState->NameTagTurnSide == EShowDownSide::Collector;
	}

	if (CharacterRole == EShowDownCharacterRole::Player)
	{
		return ShowDownGameState->NameTagTurnSide == EShowDownSide::Player;
	}

	return false;
}

bool AShowDownCharacter::ShouldShowNameTag() const
{
	if (!bCharacterSceneActive || bHitRecoveryVisualConcealed || bHitRecoveryStatusConcealed)
	{
		return false;
	}

	const FString DisplayName = ResolveNameTagDisplayName();
	if (DisplayName.IsEmpty())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Standalone)
	{
		return CharacterRole == EShowDownCharacterRole::Opponent;
	}

	if (PlayerSlot == EShowDownPlayerSlot::None
		|| (CharacterRole != EShowDownCharacterRole::Player
			&& CharacterRole != EShowDownCharacterRole::Unassigned))
	{
		return false;
	}

	const EShowDownPlayerSlot LocalPlayerSlot = GetLocalPlayerSlot();
	return LocalPlayerSlot == EShowDownPlayerSlot::None || PlayerSlot != LocalPlayerSlot;
}

bool AShowDownCharacter::ShouldShowOverheadChatMessage(const FString& SenderName) const
{
	if (!ShouldShowNameTag())
	{
		return false;
	}

	const FString TrimmedSenderName = SenderName.TrimStartAndEnd();
	if (TrimmedSenderName.IsEmpty())
	{
		return false;
	}

	if (CharacterRole == EShowDownCharacterRole::Opponent
		&& (TrimmedSenderName.Equals(TEXT("Collector"), ESearchCase::IgnoreCase)
			|| TrimmedSenderName.Equals(TEXT("김윤아"), ESearchCase::IgnoreCase)))
	{
		return true;
	}

	const FString DisplayName = ResolveNameTagDisplayName().TrimStartAndEnd();
	if (!DisplayName.IsEmpty() && TrimmedSenderName.Equals(DisplayName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString IdentityName = CharacterDisplayName.TrimStartAndEnd().Left(32);
	if (!IdentityName.IsEmpty() && TrimmedSenderName.Equals(IdentityName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (PlayerSlot != EShowDownPlayerSlot::None)
	{
		const FString FallbackPlayerName = FString::Printf(TEXT("Player %d"), static_cast<int32>(PlayerSlot));
		return TrimmedSenderName.Equals(FallbackPlayerName, ESearchCase::IgnoreCase);
	}

	return false;
}
