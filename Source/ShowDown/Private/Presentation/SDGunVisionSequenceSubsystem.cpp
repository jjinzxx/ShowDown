#include "Presentation/SDGunVisionSequenceSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "Presentation/SDVisionDirector.h"
#include "ShowDownGameStateBase.h"

namespace
{
	constexpr float RaiseToTensionDuration = 0.45f;
	constexpr float TensionDarknessStrength = 0.60f;
	constexpr float PeakDarknessStrength = 1.0f;
	constexpr float LivePeakHoldDuration = 0.10f;
	constexpr float LiveSettleDuration = 0.18f;
	constexpr float EmptyPeakHoldDuration = 0.06f;
	constexpr float EmptyReliefDuration = 0.12f;
	constexpr float PresentationFinishDuration = 0.45f;
}

void USDGunVisionGunBinding::Initialize(
	USDGunVisionSequenceSubsystem* InOwner,
	ASDSelfShotGunActor* InGun)
{
	Shutdown();
	Owner = InOwner;
	Gun = InGun;

	if (!IsValid(InGun))
	{
		return;
	}

	InGun->OnGunRaised.AddUniqueDynamic(this, &USDGunVisionGunBinding::HandleGunRaised);
	InGun->OnGunFired.AddUniqueDynamic(this, &USDGunVisionGunBinding::HandleGunFired);
	InGun->OnGunEmptyFired.AddUniqueDynamic(this, &USDGunVisionGunBinding::HandleGunEmptyFired);
	InGun->OnGunPresentationFinished.AddUniqueDynamic(
		this,
		&USDGunVisionGunBinding::HandleGunPresentationFinished);
	InGun->OnEndPlay.AddUniqueDynamic(this, &USDGunVisionGunBinding::HandleGunEndPlay);
	InGun->OnDestroyed.AddUniqueDynamic(this, &USDGunVisionGunBinding::HandleGunDestroyed);
}

void USDGunVisionGunBinding::Shutdown()
{
	if (ASDSelfShotGunActor* BoundGun = Gun.Get())
	{
		BoundGun->OnGunRaised.RemoveDynamic(this, &USDGunVisionGunBinding::HandleGunRaised);
		BoundGun->OnGunFired.RemoveDynamic(this, &USDGunVisionGunBinding::HandleGunFired);
		BoundGun->OnGunEmptyFired.RemoveDynamic(this, &USDGunVisionGunBinding::HandleGunEmptyFired);
		BoundGun->OnGunPresentationFinished.RemoveDynamic(
			this,
			&USDGunVisionGunBinding::HandleGunPresentationFinished);
		BoundGun->OnEndPlay.RemoveDynamic(this, &USDGunVisionGunBinding::HandleGunEndPlay);
		BoundGun->OnDestroyed.RemoveDynamic(this, &USDGunVisionGunBinding::HandleGunDestroyed);
	}

	Gun.Reset();
	Owner.Reset();
}

void USDGunVisionGunBinding::HandleGunRaised()
{
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunRaised(Gun.Get());
	}
}

void USDGunVisionGunBinding::HandleGunFired()
{
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunFired(Gun.Get());
	}
}

void USDGunVisionGunBinding::HandleGunEmptyFired()
{
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunEmptyFired(Gun.Get());
	}
}

void USDGunVisionGunBinding::HandleGunPresentationFinished()
{
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunPresentationFinished(Gun.Get());
	}
}

void USDGunVisionGunBinding::HandleGunDestroyed(AActor* DestroyedActor)
{
	ASDSelfShotGunActor* DestroyedGun = Cast<ASDSelfShotGunActor>(DestroyedActor);
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunUnavailable(DestroyedGun);
	}

	Gun.Reset();
}

void USDGunVisionGunBinding::HandleGunEndPlay(
	AActor* Actor,
	EEndPlayReason::Type EndPlayReason)
{
	ASDSelfShotGunActor* EndingGun = Cast<ASDSelfShotGunActor>(Actor);
	if (USDGunVisionSequenceSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleGunUnavailable(EndingGun);
	}

	Gun.Reset();
}

void USDGunVisionSequenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&USDGunVisionSequenceSubsystem::HandleActorSpawned));
	}
}

void USDGunVisionSequenceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld(); World && ActorSpawnedDelegateHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	}
	ActorSpawnedDelegateHandle.Reset();

	if (AShowDownGameStateBase* GameState = BoundGameState.Get())
	{
		GameState->OnPhaseChanged.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandlePhaseChanged);
	}
	BoundGameState.Reset();

	for (USDGunVisionGunBinding* Binding : GunBindings)
	{
		if (Binding)
		{
			Binding->Shutdown();
		}
	}
	GunBindings.Reset();
	VisionDirectors.Reset();
	PendingVisionDirectorSync.Reset();
	ActiveGun.Reset();

	Super::Deinitialize();
}

void USDGunVisionSequenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWorldHasBegunPlay = true;
	RefreshExistingBindings();
	SynchronizePendingVisionDirectors();
}

void USDGunVisionSequenceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SynchronizePendingVisionDirectors();

	SequenceElapsedTime += FMath::Max(0.0f, DeltaTime);
	switch (SequenceState)
	{
	case ESequenceState::RaiseToTension:
		if (SequenceElapsedTime >= SequenceStageDuration)
		{
			SequenceState = ESequenceState::RampToPeak;
			SequenceElapsedTime = 0.0f;
			SequenceStageDuration = FMath::Max(0.0f, ShotResolveDelay - SequenceStageDuration);
			if (SequenceStageDuration <= KINDA_SMALL_NUMBER)
			{
				SetDarknessImmediate(PeakDarknessStrength);
			}
			else
			{
				BlendDarkness(
					PeakDarknessStrength,
					SequenceStageDuration,
					ESDVisionBlendEase::EaseIn,
					2.0f);
			}
		}
		break;

	case ESequenceState::LivePeakHold:
		if (SequenceElapsedTime >= LivePeakHoldDuration)
		{
			SequenceState = ESequenceState::LiveAftermath;
			SequenceElapsedTime = 0.0f;
			SequenceStageDuration = LiveSettleDuration;
			BlendDarkness(
				TensionDarknessStrength,
				LiveSettleDuration,
				ESDVisionBlendEase::EaseOut,
				2.0f);
		}
		break;

	case ESequenceState::EmptyPeakHold:
		if (SequenceElapsedTime >= EmptyPeakHoldDuration)
		{
			SequenceState = ESequenceState::AwaitingFinish;
			SequenceElapsedTime = 0.0f;
			SequenceStageDuration = EmptyReliefDuration;
			BlendDarkness(
				0.0f,
				EmptyReliefDuration,
				ESDVisionBlendEase::EaseOut,
				2.0f);
		}
		break;

	default:
		break;
	}
}

TStatId USDGunVisionSequenceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USDGunVisionSequenceSubsystem, STATGROUP_Tickables);
}

bool USDGunVisionSequenceSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

void USDGunVisionSequenceSubsystem::RefreshExistingBindings()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASDVisionDirector> It(World); It; ++It)
	{
		RegisterVisionDirector(*It);
	}

	for (TActorIterator<ASDSelfShotGunActor> It(World); It; ++It)
	{
		BindGun(*It);
	}

	BindGameState(World->GetGameState<AShowDownGameStateBase>());
}

void USDGunVisionSequenceSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	if (ASDVisionDirector* VisionDirector = Cast<ASDVisionDirector>(SpawnedActor))
	{
		RegisterVisionDirector(VisionDirector);
	}
	else if (ASDSelfShotGunActor* GunActor = Cast<ASDSelfShotGunActor>(SpawnedActor))
	{
		BindGun(GunActor);
	}
	else if (AShowDownGameStateBase* GameState = Cast<AShowDownGameStateBase>(SpawnedActor))
	{
		BindGameState(GameState);
	}
}

void USDGunVisionSequenceSubsystem::RegisterVisionDirector(ASDVisionDirector* VisionDirector)
{
	if (!IsValid(VisionDirector) || VisionDirectors.Contains(VisionDirector))
	{
		return;
	}

	VisionDirectors.Add(VisionDirector);
	PendingVisionDirectorSync.Add(VisionDirector);
}

void USDGunVisionSequenceSubsystem::BindGun(ASDSelfShotGunActor* GunActor)
{
	if (!IsValid(GunActor))
	{
		return;
	}

	for (const USDGunVisionGunBinding* Binding : GunBindings)
	{
		if (Binding && Binding->GetGun() == GunActor)
		{
			return;
		}
	}

	USDGunVisionGunBinding* Binding = NewObject<USDGunVisionGunBinding>(this);
	Binding->Initialize(this, GunActor);
	GunBindings.Add(Binding);
}

void USDGunVisionSequenceSubsystem::BindGameState(AShowDownGameStateBase* GameState)
{
	if (BoundGameState.Get() == GameState)
	{
		return;
	}

	if (AShowDownGameStateBase* PreviousGameState = BoundGameState.Get())
	{
		PreviousGameState->OnPhaseChanged.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandlePhaseChanged);
	}

	BoundGameState = GameState;
	if (GameState)
	{
		GameState->OnPhaseChanged.AddUniqueDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandlePhaseChanged);
		HandlePhaseChanged(GameState->CurrentPhase);
	}
}

bool USDGunVisionSequenceSubsystem::HasLocalPresentationView() const
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController
			&& PlayerController->IsLocalController()
			&& PlayerController->GetLocalPlayer())
		{
			return true;
		}
	}

	return false;
}

void USDGunVisionSequenceSubsystem::SynchronizePendingVisionDirectors()
{
	if (PendingVisionDirectorSync.IsEmpty()
		|| !bWorldHasBegunPlay
		|| !HasLocalPresentationView())
	{
		return;
	}

	for (const TWeakObjectPtr<ASDVisionDirector>& PendingDirector : PendingVisionDirectorSync)
	{
		if (ASDVisionDirector* VisionDirector = PendingDirector.Get())
		{
			VisionDirector->SetDarknessStrength(DesiredDarknessStrength);
		}
	}
	PendingVisionDirectorSync.Reset();
}

void USDGunVisionSequenceSubsystem::SetDarknessImmediate(float Strength)
{
	DesiredDarknessStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (!HasLocalPresentationView())
	{
		return;
	}

	for (int32 Index = VisionDirectors.Num() - 1; Index >= 0; --Index)
	{
		if (ASDVisionDirector* VisionDirector = VisionDirectors[Index].Get())
		{
			VisionDirector->SetDarknessStrength(DesiredDarknessStrength);
		}
		else
		{
			VisionDirectors.RemoveAtSwap(Index);
		}
	}
}

void USDGunVisionSequenceSubsystem::BlendDarkness(
	float TargetStrength,
	float Duration,
	ESDVisionBlendEase EaseMode,
	float EaseExponent)
{
	DesiredDarknessStrength = FMath::Clamp(TargetStrength, 0.0f, 1.0f);
	if (!HasLocalPresentationView())
	{
		return;
	}

	for (int32 Index = VisionDirectors.Num() - 1; Index >= 0; --Index)
	{
		if (ASDVisionDirector* VisionDirector = VisionDirectors[Index].Get())
		{
			VisionDirector->BlendToDarknessStrength(
				DesiredDarknessStrength,
				FMath::Max(0.0f, Duration),
				EaseMode,
				FMath::Max(1.0f, EaseExponent));
		}
		else
		{
			VisionDirectors.RemoveAtSwap(Index);
		}
	}
}

bool USDGunVisionSequenceSubsystem::IsRoulettePhase() const
{
	const AShowDownGameStateBase* GameState = BoundGameState.Get();
	return GameState && GameState->CurrentPhase == EShowDownPhase::Roulette;
}

void USDGunVisionSequenceSubsystem::ResetToIdle(bool bImmediate)
{
	SequenceState = ESequenceState::Idle;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = 0.0f;
	ShotResolveDelay = 0.0f;
	ActiveGun.Reset();

	if (bImmediate)
	{
		SetDarknessImmediate(0.0f);
	}
	else
	{
		BlendDarkness(
			0.0f,
			PresentationFinishDuration,
			ESDVisionBlendEase::EaseOut,
			2.0f);
	}
}

void USDGunVisionSequenceSubsystem::HandleGunRaised(ASDSelfShotGunActor* GunActor)
{
	if (!IsRoulettePhase())
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor))
	{
		return;
	}

	ActiveGun = GunActor;
	SequenceState = ESequenceState::RaiseToTension;
	SequenceElapsedTime = 0.0f;
	ShotResolveDelay = FMath::Max(0.0f, GunActor->GetShotResolveDelay());
	SequenceStageDuration = FMath::Min(RaiseToTensionDuration, ShotResolveDelay);

	if (SequenceStageDuration <= KINDA_SMALL_NUMBER)
	{
		SetDarknessImmediate(TensionDarknessStrength);
	}
	else
	{
		BlendDarkness(
			TensionDarknessStrength,
			SequenceStageDuration,
			ESDVisionBlendEase::EaseOut,
			2.0f);
	}
}

void USDGunVisionSequenceSubsystem::HandleGunFired(ASDSelfShotGunActor* GunActor)
{
	if (!IsRoulettePhase())
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor) || (ActiveGun.IsValid() && ActiveGun.Get() != GunActor))
	{
		return;
	}

	ActiveGun = GunActor;
	SequenceState = ESequenceState::LivePeakHold;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = LivePeakHoldDuration;
	SetDarknessImmediate(PeakDarknessStrength);
}

void USDGunVisionSequenceSubsystem::HandleGunEmptyFired(ASDSelfShotGunActor* GunActor)
{
	if (!IsRoulettePhase())
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor) || (ActiveGun.IsValid() && ActiveGun.Get() != GunActor))
	{
		return;
	}

	ActiveGun = GunActor;
	SequenceState = ESequenceState::EmptyPeakHold;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = EmptyPeakHoldDuration;
	SetDarknessImmediate(PeakDarknessStrength);
}

void USDGunVisionSequenceSubsystem::HandleGunPresentationFinished(ASDSelfShotGunActor* GunActor)
{
	if (ActiveGun.IsValid() && ActiveGun.Get() != GunActor)
	{
		return;
	}

	ResetToIdle(false);
}

void USDGunVisionSequenceSubsystem::HandleGunUnavailable(ASDSelfShotGunActor* GunActor)
{
	if (!ActiveGun.IsValid() || ActiveGun.Get() == GunActor)
	{
		ResetToIdle(true);
	}
}

void USDGunVisionSequenceSubsystem::HandlePhaseChanged(EShowDownPhase NewPhase)
{
	if (NewPhase != EShowDownPhase::Roulette)
	{
		ResetToIdle(true);
	}
}
