#include "Presentation/SDGunVisionSequenceSubsystem.h"

#include "Audio/ShowDownAudioSubsystem.h"
#include "Components/SpotLightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "Presentation/SDVisionDirector.h"
#include "ShowDownGameStateBase.h"

namespace
{
	constexpr float BaseDarknessStrength = 1.0f;
	constexpr float BetFocusDarknessStrength = 0.72f;
	constexpr float BetFocusDarknessBlendDuration = 0.18f;
	constexpr float MatchEntryBeatDelay = 1.0f;
	constexpr float IntroCollapseDuration = 0.50f;
	constexpr float PreRevealDarkenDuration = 0.18f;
	constexpr float RaiseToTensionDuration = 0.35f;
	constexpr float TensionDarknessStrength = 1.0f;
	constexpr float PeakDarknessStrength = 1.0f;
	constexpr float LivePeakHoldDuration = 0.10f;
	constexpr float LiveSettleDuration = 0.20f;
	constexpr float EmptyPeakHoldDuration = 0.05f;
	constexpr float EmptyReliefDuration = 0.22f;
	constexpr float PresentationFinishDuration = 0.35f;
	const FName TableSpotlightActorTag(TEXT("ShowDownTableSpotlight"));
	const FName LegacyTableSpotlightActorName(TEXT("SpotLight6"));
	const FName ZeroDarknessSpotlightActorTag(TEXT("ShowDownZeroDarknessSpotlight"));
	const FName LegacyZeroDarknessSpotlightActorName(TEXT("SpotLight7"));

	bool IsTableSpotlightActor(const ASpotLight* Spotlight)
	{
		if (!IsValid(Spotlight))
		{
			return false;
		}

		if (Spotlight->ActorHasTag(TableSpotlightActorTag)
			|| Spotlight->GetFName() == LegacyTableSpotlightActorName)
		{
			return true;
		}

#if WITH_EDITOR
		return Spotlight->GetActorLabel() == LegacyTableSpotlightActorName.ToString();
#else
		return false;
#endif
	}

	bool IsZeroDarknessSpotlightActor(const ASpotLight* Spotlight)
	{
		if (!IsValid(Spotlight))
		{
			return false;
		}

		if (Spotlight->ActorHasTag(ZeroDarknessSpotlightActorTag)
			|| Spotlight->GetFName() == LegacyZeroDarknessSpotlightActorName)
		{
			return true;
		}

#if WITH_EDITOR
		return Spotlight->GetActorLabel() == LegacyZeroDarknessSpotlightActorName.ToString();
#else
		return false;
#endif
	}
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
		GameState->OnTableCinematicCue.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleTableCinematicCue);
		GameState->OnNameTagRoundStatusChanged.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleNameTagRoundStatusChanged);
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
	TableSpotlight.Reset();
	ZeroDarknessSpotlight.Reset();

	Super::Deinitialize();
}

void USDGunVisionSequenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWorldHasBegunPlay = true;
	SetTableSpotlightEnabled(false);
	// The in-game presentation starts under SpotLight7. Hub teardown explicitly
	// disables it through ResetMatchPresentationForHub.
	SetZeroDarknessSpotlightEnabled(true);
	// Bind the replicated phase only after establishing the startup light state.
	RefreshExistingBindings();
	SynchronizePendingVisionDirectors();
}

void USDGunVisionSequenceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SynchronizePendingVisionDirectors();
	AdvanceIntroSequence(DeltaTime);

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
				BaseDarknessStrength,
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
	ResolveTableSpotlight();
	ResolveZeroDarknessSpotlight();
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
	else if (ASpotLight* Spotlight = Cast<ASpotLight>(SpawnedActor); IsTableSpotlightActor(Spotlight))
	{
		const bool bRestoreEnabled = bTableSpotlightEnabled;
		TableSpotlight = Spotlight;
		SetTableSpotlightEnabled(bRestoreEnabled);
	}
	else if (ASpotLight* ZeroDarknessLight = Cast<ASpotLight>(SpawnedActor);
		IsZeroDarknessSpotlightActor(ZeroDarknessLight))
	{
		const bool bRestoreEnabled = bZeroDarknessSpotlightEnabled;
		ZeroDarknessSpotlight = ZeroDarknessLight;
		SetZeroDarknessSpotlightEnabled(bRestoreEnabled);
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
		PreviousGameState->OnTableCinematicCue.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleTableCinematicCue);
		PreviousGameState->OnNameTagRoundStatusChanged.RemoveDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleNameTagRoundStatusChanged);
	}

	BoundGameState = GameState;
	bTurnSpotlightStateInitialized = false;
	if (GameState)
	{
		GameState->OnPhaseChanged.AddUniqueDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandlePhaseChanged);
		GameState->OnTableCinematicCue.AddUniqueDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleTableCinematicCue);
		GameState->OnNameTagRoundStatusChanged.AddUniqueDynamic(
			this,
			&USDGunVisionSequenceSubsystem::HandleNameTagRoundStatusChanged);
		HandlePhaseChanged(GameState->CurrentPhase);
		RefreshTurnSpotlightSoundState();
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
			switch (IntroSequenceState)
		{
		case EIntroSequenceState::WaitingForBeat:
		case EIntroSequenceState::Collapsing:
		case EIntroSequenceState::Idle:
		default:
			VisionDirector->SetVisionRange(
					VisionDirector->GetTableVisionRadius(),
					VisionDirector->GetTableVisionFeather());
				break;
			}
		}
	}
	PendingVisionDirectorSync.Reset();
}

void USDGunVisionSequenceSubsystem::QueueMatchEntryPresentation()
{
	if (bMatchPresentationActivated || IntroSequenceState != EIntroSequenceState::Idle)
	{
		return;
	}

	ResetToIdle(true);
	bMatchPresentationActivated = true;
	bInitialDealPresentationActive = false;
	bPostShotBrightHoldActive = false;
	IntroSequenceState = EIntroSequenceState::Idle;
	IntroSequenceElapsedTime = 0.0f;
	SetDarknessImmediate(0.0f);
	SetVisionRangeImmediateToTable();
}

void USDGunVisionSequenceSubsystem::ResetMatchPresentationForHub()
{
	IntroSequenceState = EIntroSequenceState::Idle;
	IntroSequenceElapsedTime = 0.0f;
	bMatchPresentationActivated = false;
	bInitialDealPresentationActive = false;
	bPostShotBrightHoldActive = false;
	ResetToIdle(true);
	SetVisionRangeImmediateToTable();
	SetTableSpotlightEnabled(false);
	SetZeroDarknessSpotlightEnabled(false);
}

void USDGunVisionSequenceSubsystem::StartMatchIntro()
{
	bMatchPresentationActivated = true;
	bPostShotBrightHoldActive = false;
	SequenceState = ESequenceState::Idle;
	IntroSequenceState = EIntroSequenceState::Idle;
	IntroSequenceElapsedTime = 0.0f;
	SetDarknessImmediate(0.0f);
	SetVisionRangeImmediateToTable();
}

void USDGunVisionSequenceSubsystem::AdvanceIntroSequence(float DeltaTime)
{
	if (IntroSequenceState == EIntroSequenceState::Idle)
	{
		return;
	}

	IntroSequenceElapsedTime += FMath::Max(0.0f, DeltaTime);
	switch (IntroSequenceState)
	{
	case EIntroSequenceState::WaitingForBeat:
		if (IntroSequenceElapsedTime >= MatchEntryBeatDelay)
		{
			StartMatchIntro();
		}
		break;
	case EIntroSequenceState::Collapsing:
		if (IntroSequenceElapsedTime >= IntroCollapseDuration)
		{
			IntroSequenceState = EIntroSequenceState::Idle;
			IntroSequenceElapsedTime = 0.0f;
		}
		break;
	case EIntroSequenceState::Idle:
	default:
		break;
	}
}

void USDGunVisionSequenceSubsystem::SetVisionRangeImmediateToTable()
{
	if (!HasLocalPresentationView())
	{
		return;
	}

	for (int32 Index = VisionDirectors.Num() - 1; Index >= 0; --Index)
	{
		if (ASDVisionDirector* VisionDirector = VisionDirectors[Index].Get())
		{
			VisionDirector->SetVisionRange(
				VisionDirector->GetTableVisionRadius(),
				VisionDirector->GetTableVisionFeather());
		}
		else
		{
			VisionDirectors.RemoveAtSwap(Index);
		}
	}
}

void USDGunVisionSequenceSubsystem::SetVisionRangeImmediateToIntroWide()
{
	if (!HasLocalPresentationView())
	{
		return;
	}

	for (int32 Index = VisionDirectors.Num() - 1; Index >= 0; --Index)
	{
		if (ASDVisionDirector* VisionDirector = VisionDirectors[Index].Get())
		{
			VisionDirector->SetVisionRange(
				VisionDirector->GetIntroWideVisionRadius(),
				VisionDirector->GetIntroWideVisionFeather());
		}
		else
		{
			VisionDirectors.RemoveAtSwap(Index);
		}
	}
}

void USDGunVisionSequenceSubsystem::BlendVisionRangeToTable(
	float Duration,
	ESDVisionBlendEase EaseMode)
{
	if (!HasLocalPresentationView())
	{
		return;
	}

	for (int32 Index = VisionDirectors.Num() - 1; Index >= 0; --Index)
	{
		if (ASDVisionDirector* VisionDirector = VisionDirectors[Index].Get())
		{
			VisionDirector->BlendToVisionRange(
				VisionDirector->GetTableVisionRadius(),
				VisionDirector->GetTableVisionFeather(),
				Duration,
				EaseMode,
				2.0f);
		}
		else
		{
			VisionDirectors.RemoveAtSwap(Index);
		}
	}
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

ASpotLight* USDGunVisionSequenceSubsystem::ResolveTableSpotlight()
{
	if (ASpotLight* CachedSpotlight = TableSpotlight.Get())
	{
		return CachedSpotlight;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ASpotLight> It(World); It; ++It)
	{
		if (!IsTableSpotlightActor(*It))
		{
			continue;
		}

		TableSpotlight = *It;
		if (const USpotLightComponent* Light = Cast<USpotLightComponent>(It->GetLightComponent());
			Light && !Light->bAffectsWorld)
		{
			UE_LOG(LogTemp, Error,
				TEXT("SpotLight6 has Affects World disabled. Keep it enabled and use runtime visibility for the reveal light."));
		}
		return *It;
	}

	return nullptr;
}

ASpotLight* USDGunVisionSequenceSubsystem::ResolveZeroDarknessSpotlight()
{
	if (ASpotLight* CachedSpotlight = ZeroDarknessSpotlight.Get())
	{
		return CachedSpotlight;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ASpotLight> It(World); It; ++It)
	{
		if (!IsZeroDarknessSpotlightActor(*It))
		{
			continue;
		}

		ZeroDarknessSpotlight = *It;
		if (const USpotLightComponent* Light = Cast<USpotLightComponent>(It->GetLightComponent());
			Light && !Light->bAffectsWorld)
		{
			UE_LOG(LogTemp, Error,
				TEXT("SpotLight7 has Affects World disabled. Keep it enabled and use runtime visibility for the zero-darkness light."));
		}
		return *It;
	}

	return nullptr;
}

bool USDGunVisionSequenceSubsystem::SetTableSpotlightEnabled(bool bEnabled)
{
	ASpotLight* Spotlight = ResolveTableSpotlight();
	USpotLightComponent* Light = Spotlight
		? Cast<USpotLightComponent>(Spotlight->GetLightComponent())
		: nullptr;
	if (!Spotlight || !Light)
	{
		// Keep the requested state. A streamed map light can appear after its cue,
		// and HandleActorSpawned must be able to restore that state immediately.
		bTableSpotlightEnabled = bEnabled;
		return false;
	}

	if (Light->Mobility == EComponentMobility::Static)
	{
		Light->SetMobility(EComponentMobility::Movable);
	}

	const bool bWasEnabled = Light->IsVisible()
		&& !Light->bHiddenInGame
		&& !Spotlight->IsHidden();
	Light->SetVisibility(bEnabled, true);
	Light->SetHiddenInGame(!bEnabled, true);
	Spotlight->SetActorHiddenInGame(!bEnabled);
	bTableSpotlightEnabled = bEnabled;
	return bWasEnabled != bEnabled;
}

bool USDGunVisionSequenceSubsystem::SetZeroDarknessSpotlightEnabled(bool bEnabled)
{
	const bool bTableSpotlightChanged = bEnabled
		? SetTableSpotlightEnabled(false)
		: false;
	ASpotLight* Spotlight = ResolveZeroDarknessSpotlight();
	USpotLightComponent* Light = Spotlight
		? Cast<USpotLightComponent>(Spotlight->GetLightComponent())
		: nullptr;
	if (!Spotlight || !Light)
	{
		bZeroDarknessSpotlightEnabled = bEnabled;
		return bTableSpotlightChanged;
	}

	if (Light->Mobility == EComponentMobility::Static)
	{
		Light->SetMobility(EComponentMobility::Movable);
	}

	const bool bWasEnabled = Light->IsVisible()
		&& !Light->bHiddenInGame
		&& !Spotlight->IsHidden();
	Light->SetVisibility(bEnabled, true);
	Light->SetHiddenInGame(!bEnabled, true);
	Spotlight->SetActorHiddenInGame(!bEnabled);
	bZeroDarknessSpotlightEnabled = bEnabled;
	return bTableSpotlightChanged || bWasEnabled != bEnabled;
}

void USDGunVisionSequenceSubsystem::PlaySpotlightTransitionSound() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UShowDownAudioSubsystem* AudioSubsystem =
				GameInstance->GetSubsystem<UShowDownAudioSubsystem>())
			{
				AudioSubsystem->NotifySpotlightChanged();
			}
		}
	}
}

void USDGunVisionSequenceSubsystem::PlayLoserSpotlightWarningSound() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UShowDownAudioSubsystem* AudioSubsystem =
				GameInstance->GetSubsystem<UShowDownAudioSubsystem>())
			{
				AudioSubsystem->NotifyLoserSpotlightShown();
			}
		}
	}
}

void USDGunVisionSequenceSubsystem::RefreshTurnSpotlightSoundState()
{
	const AShowDownGameStateBase* GameState = BoundGameState.Get();
	if (!GameState)
	{
		bTurnSpotlightStateInitialized = false;
		return;
	}

	const bool bTurnPhase = GameState->CurrentPhase == EShowDownPhase::Betting
		|| GameState->CurrentPhase == EShowDownPhase::SelectCard;
	const bool bVisible = bTurnPhase
		&& GameState->NameTagTurnSlot != EShowDownPlayerSlot::None;
	const bool bChanged = bTurnSpotlightStateInitialized
		&& (bVisible != bLastTurnSpotlightVisible
			|| (bVisible
				&& (GameState->NameTagTurnSide != LastTurnSpotlightSide
					|| GameState->NameTagTurnSlot != LastTurnSpotlightSlot)));

	bTurnSpotlightStateInitialized = true;
	bLastTurnSpotlightVisible = bVisible;
	LastTurnSpotlightSide = GameState->NameTagTurnSide;
	LastTurnSpotlightSlot = GameState->NameTagTurnSlot;
	if (bChanged)
	{
		PlaySpotlightTransitionSound();
	}
}

void USDGunVisionSequenceSubsystem::HandleNameTagRoundStatusChanged()
{
	RefreshTurnSpotlightSoundState();
}

void USDGunVisionSequenceSubsystem::ApplyPhasePresentationPolicy(EShowDownPhase Phase)
{
	if (bInitialDealPresentationActive || bPostShotBrightHoldActive)
	{
		return;
	}

	bool bSpotlightChanged = false;
	switch (Phase)
	{
	case EShowDownPhase::SelectCard:
		bMatchPresentationActivated = true;
		SetVisionRangeImmediateToTable();
		SetDarknessImmediate(0.0f);
		bSpotlightChanged |= SetTableSpotlightEnabled(false);
		bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(true);
		break;

	case EShowDownPhase::Betting:
		bMatchPresentationActivated = true;
		SetVisionRangeImmediateToTable();
		SetDarknessImmediate(BaseDarknessStrength);
		bSpotlightChanged |= SetTableSpotlightEnabled(false);
		bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(false);
		break;

	case EShowDownPhase::Reveal:
	case EShowDownPhase::Roulette:
		return;

	case EShowDownPhase::None:
	default:
		bSpotlightChanged |= SetTableSpotlightEnabled(false);
		// Preserve SpotLight7 while the in-game world is waiting for its first
		// match cue. Once a match has run, non-gameplay phases return both lights off.
		bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(!bMatchPresentationActivated);
		ResetToIdle(false);
		break;
	}

	if (bSpotlightChanged)
	{
		PlaySpotlightTransitionSound();
	}
}

bool USDGunVisionSequenceSubsystem::IsRoulettePhase() const
{
	const AShowDownGameStateBase* GameState = BoundGameState.Get();
	return GameState && GameState->CurrentPhase == EShowDownPhase::Roulette;
}

bool USDGunVisionSequenceSubsystem::IsRoulettePresentation(
	const ASDSelfShotGunActor* GunActor) const
{
	// The reliable multiplayer presentation RPC can be processed before the
	// separately replicated phase property on a delayed client. The gun's local
	// scripted-shot context is authoritative for these presentation callbacks.
	if (IsValid(GunActor) && GunActor->IsMultiplayerRoulettePresentation())
	{
		return true;
	}

	// Networked shots must retain the explicit presentation context. A reliable
	// Reset can arrive before phase replication and invalidate a delayed local shot.
	const UWorld* World = GetWorld();
	return (!World || World->GetNetMode() == NM_Standalone) && IsRoulettePhase();
}

void USDGunVisionSequenceSubsystem::ResetToIdle(bool bImmediate)
{
	SequenceState = ESequenceState::Idle;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = 0.0f;
	ShotResolveDelay = 0.0f;
	ActiveGun.Reset();
	const float RestingDarknessStrength = bMatchPresentationActivated
		? BaseDarknessStrength
		: 0.0f;

	if (bImmediate)
	{
		SetDarknessImmediate(RestingDarknessStrength);
	}
	else
	{
		BlendDarkness(
			RestingDarknessStrength,
			PresentationFinishDuration,
			ESDVisionBlendEase::EaseOut,
			2.0f);
	}
}

void USDGunVisionSequenceSubsystem::HandleGunRaised(ASDSelfShotGunActor* GunActor)
{
	if (!IsRoulettePresentation(GunActor))
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor))
	{
		return;
	}

	bMatchPresentationActivated = true;
	bPostShotBrightHoldActive = false;
	if (SetZeroDarknessSpotlightEnabled(false))
	{
		PlaySpotlightTransitionSound();
	}
	IntroSequenceState = EIntroSequenceState::Idle;
	IntroSequenceElapsedTime = 0.0f;
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
	if (!IsRoulettePresentation(GunActor))
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor) || (ActiveGun.IsValid() && ActiveGun.Get() != GunActor))
	{
		return;
	}

	bMatchPresentationActivated = true;
	ActiveGun = GunActor;
	// FireGun emits this delegate on the real shot/result frame. That event owns
	// the bright post-shot hold so pulling the trigger cannot reveal the scene
	// before the gun actually fires.
	SequenceState = ESequenceState::Idle;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = 0.0f;
	bPostShotBrightHoldActive = true;
	ActiveTargetSpotlightMask = 0;
	SetDarknessImmediate(0.0f);
	// The red loser light and SpotLight7 switch on the firing frame without the
	// generic spotlight transition sound; the gun's own live-shot sound owns it.
	SetZeroDarknessSpotlightEnabled(true);
}

void USDGunVisionSequenceSubsystem::HandleGunEmptyFired(ASDSelfShotGunActor* GunActor)
{
	if (!IsRoulettePresentation(GunActor))
	{
		ResetToIdle(true);
		return;
	}

	if (!IsValid(GunActor) || (ActiveGun.IsValid() && ActiveGun.Get() != GunActor))
	{
		return;
	}

	bMatchPresentationActivated = true;
	ActiveGun = GunActor;
	SequenceState = ESequenceState::Idle;
	SequenceElapsedTime = 0.0f;
	SequenceStageDuration = 0.0f;
	bPostShotBrightHoldActive = true;
	ActiveTargetSpotlightMask = 0;
	SetDarknessImmediate(0.0f);
	// Empty chambers use their dedicated click sound and likewise suppress the
	// generic spotlight transition sound on the full-pull frame.
	SetZeroDarknessSpotlightEnabled(true);
}

void USDGunVisionSequenceSubsystem::HandleGunPresentationFinished(ASDSelfShotGunActor* GunActor)
{
	if (ActiveGun.IsValid() && ActiveGun.Get() != GunActor)
	{
		return;
	}

	if (bPostShotBrightHoldActive)
	{
		SequenceState = ESequenceState::Idle;
		SequenceElapsedTime = 0.0f;
		SequenceStageDuration = 0.0f;
		ShotResolveDelay = 0.0f;
		ActiveGun.Reset();
		return;
	}

	ResetToIdle(false);
}

void USDGunVisionSequenceSubsystem::HandleGunUnavailable(ASDSelfShotGunActor* GunActor)
{
	if (!ActiveGun.IsValid() || ActiveGun.Get() == GunActor)
	{
		if (bPostShotBrightHoldActive)
		{
			SequenceState = ESequenceState::Idle;
			SequenceElapsedTime = 0.0f;
			SequenceStageDuration = 0.0f;
			ShotResolveDelay = 0.0f;
			ActiveGun.Reset();
		}
		else
		{
			ResetToIdle(true);
		}
	}
}

void USDGunVisionSequenceSubsystem::HandlePhaseChanged(EShowDownPhase NewPhase)
{
	if (bPostShotBrightHoldActive && NewPhase != EShowDownPhase::Roulette)
	{
		// Phase replication is the server-authoritative next-game boundary. Do
		// not let a per-client timer restore Darkness ahead of that boundary.
		bPostShotBrightHoldActive = false;
	}
	RefreshTurnSpotlightSoundState();
	ApplyPhasePresentationPolicy(NewPhase);
}

void USDGunVisionSequenceSubsystem::HandleTableCinematicCue(
	ESDTableCinematicCue Cue,
	uint8 PlayerSlotMask)
{
	switch (Cue)
	{
	case ESDTableCinematicCue::MatchIntro:
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			QueueMatchEntryPresentation();
			bSpotlightChanged |= SetTableSpotlightEnabled(false);
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(true);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		break;

	case ESDTableCinematicCue::PreRevealBlackout:
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(false);
			// SpotLight6 now comes on with the blackout instead of two seconds later.
			bSpotlightChanged |= SetTableSpotlightEnabled(true);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		bMatchPresentationActivated = true;
		bInitialDealPresentationActive = false;
		bPostShotBrightHoldActive = false;
		IntroSequenceState = EIntroSequenceState::Idle;
		IntroSequenceElapsedTime = 0.0f;
		BlendVisionRangeToTable(0.45f, ESDVisionBlendEase::EaseOut);
		BlendDarkness(
			PeakDarknessStrength,
			PreRevealDarkenDuration,
			ESDVisionBlendEase::EaseInOut,
			2.4f);
		break;

	case ESDTableCinematicCue::RevealStarted:
		bMatchPresentationActivated = true;
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(false);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		SetDarknessImmediate(PeakDarknessStrength);
		break;

	case ESDTableCinematicCue::LoserSpotlight:
		bMatchPresentationActivated = true;
		SetDarknessImmediate(PeakDarknessStrength);
		{
			const bool bTargetsChanged = ActiveTargetSpotlightMask != PlayerSlotMask;
			ActiveTargetSpotlightMask = PlayerSlotMask;
			const bool bSpotlightChanged = SetTableSpotlightEnabled(false)
				| SetZeroDarknessSpotlightEnabled(false)
				| bTargetsChanged;
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
			if (bTargetsChanged && PlayerSlotMask != 0)
			{
				PlayLoserSpotlightWarningSound();
			}
		}
		break;

	case ESDTableCinematicCue::TableSpotlightOn:
		if (!bZeroDarknessSpotlightEnabled && SetTableSpotlightEnabled(true))
		{
			PlaySpotlightTransitionSound();
		}
		break;
	case ESDTableCinematicCue::TableSpotlightOff:
		if (SetTableSpotlightEnabled(false))
		{
			PlaySpotlightTransitionSound();
		}
		break;

	case ESDTableCinematicCue::BetFocusStarted:
		bMatchPresentationActivated = true;
		BlendDarkness(
			BetFocusDarknessStrength,
			BetFocusDarknessBlendDuration,
			ESDVisionBlendEase::EaseOut,
			2.0f);
		break;

	case ESDTableCinematicCue::BetFocusEnded:
		BlendDarkness(
			BaseDarknessStrength,
			BetFocusDarknessBlendDuration,
			ESDVisionBlendEase::EaseIn,
			2.0f);
		break;

	case ESDTableCinematicCue::TriggerPullStarted:
		// Keep the red loser light unchanged while the trigger is travelling.
		break;

	case ESDTableCinematicCue::TriggerPullCompleted:
		// The gun emits this local cue on the exact full-pull frame. Clear the red
		// target light silently; HandleGunFired/HandleGunEmptyFired owns darkness
		// and SpotLight7 on that same frame.
		ActiveTargetSpotlightMask = 0;
		SetTableSpotlightEnabled(false);
		break;

	case ESDTableCinematicCue::InitialDealStarted:
		bMatchPresentationActivated = true;
		bInitialDealPresentationActive = true;
		bPostShotBrightHoldActive = false;
		IntroSequenceState = EIntroSequenceState::Idle;
		IntroSequenceElapsedTime = 0.0f;
		SetVisionRangeImmediateToTable();
		SetDarknessImmediate(0.0f);
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			bSpotlightChanged |= SetTableSpotlightEnabled(false);
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(true);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		break;

	case ESDTableCinematicCue::InitialDealFinished:
		bInitialDealPresentationActive = false;
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			bSpotlightChanged |= SetTableSpotlightEnabled(false);
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(false);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		BlendDarkness(
			BaseDarknessStrength,
			PresentationFinishDuration,
			ESDVisionBlendEase::EaseInOut,
			2.0f);
		break;

	case ESDTableCinematicCue::Reset:
	default:
		bInitialDealPresentationActive = false;
		bPostShotBrightHoldActive = false;
		IntroSequenceState = EIntroSequenceState::Idle;
		IntroSequenceElapsedTime = 0.0f;
		BlendVisionRangeToTable(0.6f, ESDVisionBlendEase::EaseOut);
		{
			bool bSpotlightChanged = ActiveTargetSpotlightMask != 0;
			ActiveTargetSpotlightMask = 0;
			bSpotlightChanged |= SetTableSpotlightEnabled(false);
			bSpotlightChanged |= SetZeroDarknessSpotlightEnabled(false);
			if (bSpotlightChanged)
			{
				PlaySpotlightTransitionSound();
			}
		}
		ResetToIdle(false);
		if (const AShowDownGameStateBase* GameState = BoundGameState.Get())
		{
			ApplyPhasePresentationPolicy(GameState->CurrentPhase);
		}
		break;
	}
}
