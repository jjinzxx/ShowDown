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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(14.0f);
	SetMinNetUpdateFrequency(4.0f);
	SetActorTickEnabled(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ASDBetActionPanelActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureButtons();
	RefreshVisuals();
}

void ASDBetActionPanelActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!PanelState.bVisible)
	{
		SetActorTickEnabled(false);
		return;
	}

	const EShowDownPlayerSlot CurrentLocalPlayerSlot = ResolveLocalPlayerSlot();
	const bool bShouldBeVisible =
		PanelState.TurnSlot != EShowDownPlayerSlot::None
		&& CurrentLocalPlayerSlot == PanelState.TurnSlot;

	if (CurrentLocalPlayerSlot != LastResolvedLocalPlayerSlot || bShouldBeVisible == IsHidden())
	{
		RefreshVisuals();
	}
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

	LastResolvedLocalPlayerSlot = ResolveLocalPlayerSlot();
	const bool bPanelVisible =
		PanelState.bVisible
		&& PanelState.TurnSlot != EShowDownPlayerSlot::None
		&& LastResolvedLocalPlayerSlot == PanelState.TurnSlot;
	SetActorHiddenInGame(!bPanelVisible);
	SetActorTickEnabled(PanelState.bVisible);

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
	const float LayoutScale = FMath::Max(0.1f, PanelVisualScale);
	const FRotator PanelRotation = PanelState.WorldRotation;
	const FVector RightDirection = FRotationMatrix(PanelRotation).GetUnitAxis(EAxis::Y);
	const FVector2D PrimarySize = GetButtonSize(ESDBetActionPanelButtonKind::Primary);
	const FVector2D FoldSize = GetButtonSize(ESDBetActionPanelButtonKind::Fold);
	const FVector2D StepSize = GetButtonSize(ESDBetActionPanelButtonKind::RaiseDown);
	const FVector2D RaiseSize = GetButtonSize(ESDBetActionPanelButtonKind::RaiseSubmit);
	const float BottomGap = 6.0f * LayoutScale;
	const float BottomTotalWidth = StepSize.X + BottomGap + RaiseSize.X + BottomGap + StepSize.X;
	const float TopGap = FMath::Max(6.0f * LayoutScale, BottomTotalWidth - FoldSize.X - PrimarySize.X);
	const float RaiseStepOffset = (RaiseSize.X * 0.5f) + BottomGap + (StepSize.X * 0.5f);

	float RightOffset = 0.0f;
	float HeightOffset = BottomRowHeight;

	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		RightOffset = -((FoldSize.X + TopGap) * 0.5f);
		HeightOffset = TopRowHeight;
		break;
	case ESDBetActionPanelButtonKind::Fold:
		RightOffset = (PrimarySize.X + TopGap) * 0.5f;
		HeightOffset = TopRowHeight;
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
		RightOffset = RaiseStepOffset;
		break;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		RightOffset = 0.0f;
		break;
	case ESDBetActionPanelButtonKind::RaiseUp:
		RightOffset = -RaiseStepOffset;
		break;
	default:
		break;
	}

	const FVector ButtonLocation =
		PanelState.WorldLocation
		+ RightDirection * RightOffset
		+ FVector::UpVector * HeightOffset * LayoutScale;
	return FTransform(PanelRotation, ButtonLocation);
}

FVector2D ASDBetActionPanelActor::GetButtonSize(ESDBetActionPanelButtonKind ButtonKind) const
{
	const float LayoutScale = FMath::Max(0.1f, PanelVisualScale);
	switch (ButtonKind)
	{
	case ESDBetActionPanelButtonKind::Primary:
		return FVector2D(PrimaryButtonWidth, ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseUp:
		return FVector2D(StepButtonWidth, ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::RaiseSubmit:
		return FVector2D(RaiseButtonWidth, ButtonHeight) * LayoutScale;
	case ESDBetActionPanelButtonKind::Fold:
		return FVector2D(FoldButtonWidth, ButtonHeight) * LayoutScale;
	default:
		return FVector2D(PrimaryButtonWidth, ButtonHeight) * LayoutScale;
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
			? FLinearColor(0.04f, 0.95f, 0.24f, 1.0f)
			: FLinearColor(0.04f, 0.95f, 0.24f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::RaiseDown:
	case ESDBetActionPanelButtonKind::RaiseSubmit:
	case ESDBetActionPanelButtonKind::RaiseUp:
		Color = FLinearColor(1.0f, 0.45f, 0.02f, 1.0f);
		break;
	case ESDBetActionPanelButtonKind::Fold:
		Color = FLinearColor(1.0f, 0.04f, 0.02f, 1.0f);
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
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);
	SetActorTickEnabled(false);

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

void ASDBetActionButtonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float TargetAlpha = bTargetVisible ? 1.0f : 0.0f;
	const float InterpSpeed = bTargetVisible ? 9.0f : 11.0f;
	VisualAlpha = FMath::FInterpTo(VisualAlpha, TargetAlpha, DeltaSeconds, InterpSpeed);
	if (FMath::IsNearlyEqual(VisualAlpha, TargetAlpha, 0.01f))
	{
		VisualAlpha = TargetAlpha;
	}

	const float TargetPressAlpha = bPointerPressed ? 1.0f : 0.0f;
	PressVisualAlpha = FMath::FInterpTo(PressVisualAlpha, TargetPressAlpha, DeltaSeconds, 18.0f);
	if (FMath::IsNearlyEqual(PressVisualAlpha, TargetPressAlpha, 0.01f))
	{
		PressVisualAlpha = TargetPressAlpha;
	}
	ApplyAnimatedVisuals();

	if (!bTargetVisible && VisualAlpha <= KINDA_SMALL_NUMBER && PressVisualAlpha <= KINDA_SMALL_NUMBER)
	{
		SetActorTickEnabled(false);
	}
	else if (bTargetVisible && VisualAlpha >= 1.0f && PressVisualAlpha <= KINDA_SMALL_NUMBER && !bPointerPressed)
	{
		SetActorTickEnabled(false);
	}
}

void ASDBetActionButtonActor::InitializeButton(
	ASDBetActionPanelActor* InOwnerPanel,
	ESDBetActionPanelButtonKind InButtonKind)
{
	OwnerPanel = InOwnerPanel;
	ButtonKind = InButtonKind;
}

void ASDBetActionButtonActor::BeginPointerPress()
{
	if (!bButtonEnabled)
	{
		return;
	}

	bPointerPressed = true;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();
}

void ASDBetActionButtonActor::CancelPointerPress()
{
	bPointerPressed = false;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();
}

void ASDBetActionButtonActor::ReleasePointerPress(AActor* Interactor, bool bCommit)
{
	bPointerPressed = false;
	SetActorTickEnabled(true);
	ApplyAnimatedVisuals();

	if (bCommit && OwnerPanel)
	{
		OwnerPanel->HandleButtonClicked(ButtonKind, Interactor);
	}
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
	if (bVisible && !bTargetVisible)
	{
		SetActorHiddenInGame(false);
	}

	bTargetVisible = bVisible;
	bButtonEnabled = bVisible && bEnabled;
	if (!bButtonEnabled)
	{
		bPointerPressed = false;
	}
	SetActorTickEnabled(true);

	if (ClickBounds)
	{
		ClickBounds->SetBoxExtent(FVector(
			FMath::Max(1.0f, Size.Y * 0.25f),
			FMath::Max(1.0f, Size.X * 0.5f),
			FMath::Max(1.0f, Size.Y * 0.5f)));
	}

	if (BackplateMesh)
	{
		CurrentPressDepth = FMath::Clamp(Size.Y * 0.35f, 0.75f, 2.4f);
		BackplateMesh->SetRelativeScale3D(FVector(
			FMath::Max(0.012f, Size.Y * 0.0025f),
			FMath::Max(1.0f, Size.X) / 100.0f,
			FMath::Max(1.0f, Size.Y) / 100.0f));
		ApplyBackplateColor(Color);
	}

	if (LabelText)
	{
		LabelText->SetVisibility(bVisible);
		LabelText->SetText(FText::FromString(Label));
		CurrentLabelColor = bEnabled
			? FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)
			: FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);
		LabelText->SetWorldSize(Label.Len() <= 2
			? FMath::Clamp(Size.Y * 0.68f, 1.8f, 9.5f)
			: FMath::Clamp(Size.Y * 0.42f, 1.6f, 6.5f));
	}

	ApplyAnimatedVisuals();
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
		ReleasePointerPress(Interactor, true);
	}
}

void ASDBetActionButtonActor::ApplyAnimatedVisuals()
{
	const bool bDrawVisible = bTargetVisible || VisualAlpha > KINDA_SMALL_NUMBER;
	SetActorHiddenInGame(!bDrawVisible);

	const float PressOffsetX = -CurrentPressDepth * FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(PressVisualAlpha, 0.0f, 1.0f));
	SetActorScale3D(FVector::OneVector);

	if (ClickBounds)
	{
		ClickBounds->SetCollisionEnabled(
			bButtonEnabled && VisualAlpha > 0.85f
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}

	if (BackplateMesh)
	{
		BackplateMesh->SetVisibility(bDrawVisible);
		BackplateMesh->SetRelativeLocation(FVector(PressOffsetX, 0.0f, 0.0f));
		if (BackplateMaterialInstance)
		{
			const FLinearColor FadeColor(
				CurrentBackplateColor.R * 0.08f,
				CurrentBackplateColor.G * 0.08f,
				CurrentBackplateColor.B * 0.08f,
				1.0f);
			FLinearColor AnimatedColor = FMath::Lerp(FadeColor, CurrentBackplateColor, FMath::Clamp(VisualAlpha, 0.0f, 1.0f));
			AnimatedColor.A = 1.0f;
			BackplateMaterialInstance->SetVectorParameterValue(TEXT("Color"), AnimatedColor);
			BackplateMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), AnimatedColor);
		}
	}
	if (LabelText)
	{
		LabelText->SetVisibility(bDrawVisible);
		LabelText->SetRelativeLocation(FVector(FMath::Max(0.55f, CurrentPressDepth * 1.25f) + PressOffsetX, 0.0f, 0.0f));
		const FLinearColor FadeTextColor(
			CurrentLabelColor.R * 0.12f,
			CurrentLabelColor.G * 0.12f,
			CurrentLabelColor.B * 0.12f,
			1.0f);
		FLinearColor TextColor = FMath::Lerp(FadeTextColor, CurrentLabelColor, FMath::Clamp(VisualAlpha, 0.0f, 1.0f));
		TextColor.A = 1.0f;
		LabelText->SetTextRenderColor(ToTextColor(TextColor));
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
		Color.R * 0.92f,
		Color.G * 0.92f,
		Color.B * 0.92f,
		1.0f);
	CurrentBackplateColor = PlateColor;

	const FLinearColor FadeColor(
		PlateColor.R * 0.08f,
		PlateColor.G * 0.08f,
		PlateColor.B * 0.08f,
		1.0f);
	FLinearColor AnimatedColor = FMath::Lerp(FadeColor, PlateColor, FMath::Clamp(VisualAlpha, 0.0f, 1.0f));
	AnimatedColor.A = 1.0f;
	BackplateMaterialInstance->SetVectorParameterValue(TEXT("Color"), AnimatedColor);
	BackplateMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), AnimatedColor);
}
