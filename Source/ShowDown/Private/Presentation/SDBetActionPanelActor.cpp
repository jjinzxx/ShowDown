#include "Presentation/SDBetActionPanelActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SDPlayerState.h"
#include "ShowDownPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const ESDBetActionPanelButtonKind ButtonKinds[] =
	{
		ESDBetActionPanelButtonKind::Primary,
		ESDBetActionPanelButtonKind::RaiseDown,
		ESDBetActionPanelButtonKind::RaiseSubmit,
		ESDBetActionPanelButtonKind::RaiseUp,
		ESDBetActionPanelButtonKind::Fold
	};

	FColor ToTextColor(const FLinearColor& Color)
	{
		return Color.ToFColor(true);
	}

	FLinearColor DimColor(const FLinearColor& Color)
	{
		return FLinearColor(
			Color.R * 0.28f,
			Color.G * 0.28f,
			Color.B * 0.28f,
			0.72f);
	}
}

ASDBetActionPanelActor::ASDBetActionPanelActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(14.0f);
	SetMinNetUpdateFrequency(4.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ASDBetActionPanelActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureButtons();
	RefreshVisuals();
}

void ASDBetActionPanelActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ASDBetActionButtonActor* ButtonActor : ButtonActors)
	{
		if (IsValid(ButtonActor))
		{
			ButtonActor->Destroy();
		}
	}
	ButtonActors.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASDBetActionPanelActor::SetPanelState(const FSDBetActionPanelState& NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	PanelState = NewState;
	RefreshVisuals();
	ForceNetUpdate();
}

void ASDBetActionPanelActor::OnRep_PanelState()
{
	RefreshVisuals();
}

void ASDBetActionPanelActor::EnsureButtons()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ButtonActors.Num() != ButtonCount)
	{
		ButtonActors.SetNum(ButtonCount);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* SpawnClass = ButtonActorClass
		? ButtonActorClass.Get()
		: ASDBetActionButtonActor::StaticClass();
	if (!SpawnClass)
	{
		return;
	}

	for (ESDBetActionPanelButtonKind ButtonKind : ButtonKinds)
	{
		const int32 ButtonIndex = GetButtonIndex(ButtonKind);
		if (!ButtonActors.IsValidIndex(ButtonIndex) || IsValid(ButtonActors[ButtonIndex]))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASDBetActionButtonActor* ButtonActor = World->SpawnActor<ASDBetActionButtonActor>(
			SpawnClass,
			FTransform::Identity,
			SpawnParams);
		if (!ButtonActor)
		{
			continue;
		}

		ButtonActor->InitializeButton(this, ButtonKind);
		ButtonActors[ButtonIndex] = ButtonActor;
	}
}

void ASDBetActionPanelActor::RefreshVisuals()
{
	EnsureButtons();
	RefreshSelectedRaiseTarget();

	const bool bPanelVisible = IsPanelVisibleForLocalPlayer();
	SetActorHiddenInGame(!bPanelVisible);

	for (ESDBetActionPanelButtonKind ButtonKind : ButtonKinds)
	{
		const int32 ButtonIndex = GetButtonIndex(ButtonKind);
		if (!ButtonActors.IsValidIndex(ButtonIndex) || !ButtonActors[ButtonIndex])
		{
			continue;
		}

		const bool bEnabled = CanPressButton(ButtonKind);
		ButtonActors[ButtonIndex]->SetButtonState(
			BuildButtonLabel(ButtonKind),
			GetButtonColor(ButtonKind, bEnabled),
			GetButtonSize(ButtonKind),
			bPanelVisible,
			bEnabled,
			BuildButtonTransform(ButtonKind));
	}
}

void ASDBetActionPanelActor::RefreshSelectedRaiseTarget()
{
	const int32 ClampedTarget = ClampRaiseTarget(SelectedRaiseTarget);
	const bool bStateChanged =
		LastSeenRevision != PanelState.Revision
		|| LastSeenTurnSlot != PanelState.TurnSlot
		|| LastSeenTableBet != PanelState.TableBet
		|| LastSeenCurrentPlayerBet != PanelState.CurrentPlayerBet;

	if (bStateChanged || ClampedTarget != SelectedRaiseTarget)
	{
		SelectedRaiseTarget = GetDefaultRaiseTarget();
	}

	LastSeenRevision = PanelState.Revision;
	LastSeenTurnSlot = PanelState.TurnSlot;
	LastSeenTableBet = PanelState.TableBet;
	LastSeenCurrentPlayerBet = PanelState.CurrentPlayerBet;
}

void ASDBetActionPanelActor::AdjustSelectedRaiseTarget(int32 Delta)
{
	SelectedRaiseTarget = ClampRaiseTarget(SelectedRaiseTarget + Delta);
	RefreshVisuals();
}

int32 ASDBetActionPanelActor::GetDefaultRaiseTarget() const
{
	const int32 MinimumUsefulRaise = FMath::Max(PanelState.TableBet + 1, PanelState.CurrentPlayerBet + 1);
	return ClampRaiseTarget(FMath::Max(PanelState.MinRaiseTarget, MinimumUsefulRaise));
}

int32 ASDBetActionPanelActor::ClampRaiseTarget(int32 TargetBet) const
{
	const int32 MinTarget = FMath::Clamp(PanelState.MinRaiseTarget, 1, 6);
	const int32 MaxTarget = FMath::Clamp(FMath::Max(PanelState.MaxRaiseTarget, MinTarget), MinTarget, 6);
	return FMath::Clamp(TargetBet, MinTarget, MaxTarget);
}

EShowDownPlayerSlot ASDBetActionPanelActor::ResolveLocalPlayerSlot() const
{
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const ASDPlayerState* LocalPlayerState = LocalPlayerController
		? LocalPlayerController->GetPlayerState<ASDPlayerState>()
		: nullptr;

	if (PanelState.bMultiplayer)
	{
		return LocalPlayerState ? LocalPlayerState->ShowDownSlot : EShowDownPlayerSlot::None;
	}

	return EShowDownPlayerSlot::Player1;
}

bool ASDBetActionPanelActor::IsPanelVisibleForLocalPlayer() const
{
	return PanelState.bVisible
		&& PanelState.TurnSlot != EShowDownPlayerSlot::None
		&& ResolveLocalPlayerSlot() == PanelState.TurnSlot;
}

bool ASDBetActionPanelActor::CanPressButton(ESDBetActionPanelButtonKind ButtonKind) const
{
	if (!IsPanelVisibleForLocalPlayer())
	{
		return false;
	}

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return true;
	case ESDBetActionPanelButtonKind::RaiseDown:
		return PanelState.bCanRaise && SelectedRaiseTarget > ClampRaiseTarget(PanelState.MinRaiseTarget);
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return PanelState.bCanRaise && SelectedRaiseTarget > PanelState.TableBet;
	case ESDBetActionPanelButtonKind::RaiseUp:
		return PanelState.bCanRaise && SelectedRaiseTarget < ClampRaiseTarget(PanelState.MaxRaiseTarget);
	case ESDBetActionPanelButtonKind::Fold:
		return PanelState.bCanFold;
	default:
		return false;
	}
}

void ASDBetActionPanelActor::HandleButtonClicked(ESDBetActionPanelButtonKind ButtonKind, AActor* Interactor)
{
	if (!CanPressButton(ButtonKind))
	{
		return;
	}

	if (ButtonKind == ESDBetActionPanelButtonKind::RaiseDown)
	{
		AdjustSelectedRaiseTarget(-1);
		return;
	}
	if (ButtonKind == ESDBetActionPanelButtonKind::RaiseUp)
	{
		AdjustSelectedRaiseTarget(1);
		return;
	}

	AShowDownPlayerController* PlayerController = Cast<AShowDownPlayerController>(Interactor);
	if (!PlayerController)
	{
		PlayerController = Cast<AShowDownPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	if (!PlayerController)
	{
		return;
	}

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		PlayerController->RequestPlayerCheck();
		break;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		PlayerController->RequestPlayerRaiseTo(SelectedRaiseTarget);
		break;
	case ESDBetActionPanelButtonKind::Fold:
		PlayerController->RequestPlayerFold();
		break;
	default:
		break;
	}
}

FTransform ASDBetActionPanelActor::BuildButtonTransform(ESDBetActionPanelButtonKind ButtonKind) const
{
	const FRotator PanelRotation = PanelState.WorldRotation;
	const FVector RightDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::Y);

	float RightOffset = 0.0f;
	float HeightOffset = BottomRowHeight;

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		RightOffset = -48.0f;
		HeightOffset = TopRowHeight;
		break;
	case ESDBetActionPanelButtonKind::Fold:
		RightOffset = 48.0f;
		HeightOffset = TopRowHeight;
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
		RightOffset = -74.0f;
		break;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		RightOffset = 0.0f;
		break;
	case ESDBetActionPanelButtonKind::RaiseUp:
		RightOffset = 74.0f;
		break;
	default:
		break;
	}

	const FVector ButtonLocation =
		PanelState.WorldLocation
		+ RightDirection * RightOffset
		+ FVector::UpVector * HeightOffset;
	return FTransform(PanelRotation, ButtonLocation);
}

FVector2D ASDBetActionPanelActor::GetButtonSize(ESDBetActionPanelButtonKind ButtonKind) const
{
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return FVector2D(PrimaryButtonWidth, ButtonHeight);
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseUp:
		return FVector2D(StepButtonWidth, ButtonHeight);
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return FVector2D(RaiseButtonWidth, ButtonHeight);
	case ESDBetActionPanelButtonKind::Fold:
		return FVector2D(FoldButtonWidth, ButtonHeight);
	default:
		return FVector2D(PrimaryButtonWidth, ButtonHeight);
	}
}

FString ASDBetActionPanelActor::BuildButtonLabel(ESDBetActionPanelButtonKind ButtonKind) const
{
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return PanelState.CurrentPlayerBet < PanelState.TableBet
			? FString::Printf(TEXT("CALL %d"), PanelState.TableBet)
			: FString(TEXT("CHECK"));
	case ESDBetActionPanelButtonKind::RaiseDown:
		return TEXT("-");
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return FString::Printf(TEXT("RAISE TO %d"), SelectedRaiseTarget);
	case ESDBetActionPanelButtonKind::RaiseUp:
		return TEXT("+");
	case ESDBetActionPanelButtonKind::Fold:
		return TEXT("FOLD");
	default:
		return FString();
	}
}

FLinearColor ASDBetActionPanelActor::GetButtonColor(
	ESDBetActionPanelButtonKind ButtonKind,
	bool bEnabled) const
{
	FLinearColor Color = FLinearColor::White;
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		Color = PanelState.CurrentPlayerBet < PanelState.TableBet
			? FLinearColor(0.08f, 0.78f, 1.0f, 1.0f)
			: FLinearColor(0.18f, 1.0f, 0.64f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseSubmit:
	case ESDBetActionPanelButtonKind::RaiseUp:
		Color = FLinearColor(1.0f, 0.66f, 0.12f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::Fold:
		Color = FLinearColor(1.0f, 0.16f, 0.1f, 1.0f);
		break;
	default:
		break;
	}

	return bEnabled ? Color : DimColor(Color);
}

int32 ASDBetActionPanelActor::GetButtonIndex(ESDBetActionPanelButtonKind ButtonKind) const
{
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return 0;
	case ESDBetActionPanelButtonKind::RaiseDown:
		return 1;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return 2;
	case ESDBetActionPanelButtonKind::RaiseUp:
		return 3;
	case ESDBetActionPanelButtonKind::Fold:
		return 4;
	default:
		return INDEX_NONE;
	}
}

void ASDBetActionPanelActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASDBetActionPanelActor, PanelState);
}

ASDBetActionButtonActor::ASDBetActionButtonActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ClickBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBounds"));
	ClickBounds->SetupAttachment(Root);
	ClickBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickBounds->SetCollisionObjectType(ECC_WorldDynamic);
	ClickBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBounds->SetCanEverAffectNavigation(false);

	BackplateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackplateMesh"));
	BackplateMesh->SetupAttachment(Root);
	BackplateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackplateMesh->SetCanEverAffectNavigation(false);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
	LabelText->SetupAttachment(Root);
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetWorldSize(13.0f);
	LabelText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelText->SetCanEverAffectNavigation(false);
	LabelText->SetRelativeLocation(FVector(3.5f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BackplateMeshAsset = CubeMeshFinder.Object;
		BackplateMesh->SetStaticMesh(BackplateMeshAsset);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialFinder.Succeeded())
	{
		BackplateMesh->SetMaterial(0, MaterialFinder.Object);
	}

	SetActorHiddenInGame(true);
}

void ASDBetActionButtonActor::InitializeButton(
	ASDBetActionPanelActor* InOwnerPanel,
	ESDBetActionPanelButtonKind InButtonKind)
{
	OwnerPanel = InOwnerPanel;
	ButtonKind = InButtonKind;
}

void ASDBetActionButtonActor::SetButtonState(
	const FString& Label,
	const FLinearColor& Color,
	const FVector2D& Size,
	bool bVisible,
	bool bEnabled,
	const FTransform& WorldTransform)
{
	SetActorTransform(WorldTransform);
	SetActorHiddenInGame(!bVisible);
	bButtonEnabled = bVisible && bEnabled;

	if (ClickBounds)
	{
		ClickBounds->SetBoxExtent(FVector(8.0f, FMath::Max(1.0f, Size.X * 0.5f), FMath::Max(1.0f, Size.Y * 0.5f)));
		ClickBounds->SetCollisionEnabled(bButtonEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (BackplateMesh)
	{
		BackplateMesh->SetVisibility(bVisible);
		BackplateMesh->SetRelativeScale3D(FVector(
			0.035f,
			FMath::Max(1.0f, Size.X) / 100.0f,
			FMath::Max(1.0f, Size.Y) / 100.0f));
		ApplyBackplateColor(Color);
	}

	if (LabelText)
	{
		LabelText->SetVisibility(bVisible);
		LabelText->SetText(FText::FromString(Label));
		LabelText->SetTextRenderColor(ToTextColor(bEnabled ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f, 1.0f)));
		LabelText->SetWorldSize(Label.Len() <= 2 ? 19.0f : 12.0f);
	}
}

bool ASDBetActionButtonActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bButtonEnabled
		&& OwnerPanel
		&& OwnerPanel->CanPressButton(ButtonKind);
}

void ASDBetActionButtonActor::Interact_Implementation(AActor* Interactor)
{
	if (OwnerPanel)
	{
		OwnerPanel->HandleButtonClicked(ButtonKind, Interactor);
	}
}

void ASDBetActionButtonActor::ApplyBackplateColor(const FLinearColor& Color)
{
	if (!BackplateMesh)
	{
		return;
	}

	if (!BackplateMaterialInstance)
	{
		BackplateMaterialInstance = BackplateMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!BackplateMaterialInstance)
	{
		return;
	}

	const FLinearColor PlateColor(
		Color.R * 0.22f,
		Color.G * 0.22f,
		Color.B * 0.22f,
		0.78f);
	BackplateMaterialInstance->SetVectorParameterValue(TEXT("Color"), PlateColor);
	BackplateMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), PlateColor);
}
