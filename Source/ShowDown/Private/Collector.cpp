#include "Collector.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

ACollector::ACollector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	rootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(rootComp);

	c_HandCard = CreateDefaultSubobject<USceneComponent>(TEXT("CollectorHandCardSlot"));
	c_HandCard->SetupAttachment(rootComp);

	c_HeadCard = CreateDefaultSubobject<USceneComponent>(TEXT("CollectorHeadCardSlot"));
	c_HeadCard->SetupAttachment(rootComp);
}

void ACollector::BeginPlay()
{
	Super::BeginPlay();
}

void ACollector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActionSpinActive)
	{
		return;
	}

	USceneComponent* Target = ActiveActionSpinTarget.Get();
	if (!Target)
	{
		bActionSpinActive = false;
		QueuedActionSpinCount = 0;
		SetActorTickEnabled(false);
		return;
	}

	ActionSpinElapsed += DeltaTime;
	const float Duration = FMath::Max(ActionSpinDuration, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(ActionSpinElapsed / Duration, 0.0f, 1.0f);
	const FVector SpinAxis = ActionSpinAxisLocal.IsNearlyZero()
		? FVector::UpVector
		: ActionSpinAxisLocal.GetSafeNormal();
	const float SpinRadians = Alpha * TWO_PI * FMath::Max(ActionSpinTurns, 1);
	const FQuat SpinRotation(SpinAxis, SpinRadians);

	Target->SetRelativeRotation(ActionSpinStartRotation * SpinRotation);

	if (Alpha < 1.0f)
	{
		return;
	}

	Target->SetRelativeRotation(ActionSpinStartRotation);
	bActionSpinActive = false;
	ActionSpinElapsed = 0.0f;
	ActiveActionSpinTarget = nullptr;

	if (QueuedActionSpinCount > 0)
	{
		--QueuedActionSpinCount;
		StartActionSpin();
		return;
	}

	SetActorTickEnabled(false);
}

void ACollector::PlayActionSpin()
{
	if (!bEnableActionSpin)
	{
		return;
	}

	if (bActionSpinActive)
	{
		QueuedActionSpinCount = FMath::Clamp(QueuedActionSpinCount + 1, 0, MaxQueuedActionSpins);
		return;
	}

	StartActionSpin();
}

float ACollector::GetActionSpinTotalSeconds() const
{
	return FMath::Max(0.0f, ActionSpinDuration) + FMath::Max(0.0f, ActionSpinHoldSeconds);
}

USceneComponent* ACollector::ResolveActionSpinTarget() const
{
	if (ActionSpinTarget)
	{
		return ActionSpinTarget;
	}

	TArray<USceneComponent*> SceneComponents;
	GetComponents(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component && Component->ComponentHasTag(ActionSpinTargetTag))
		{
			return Component;
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!Component
			|| Component == rootComp.Get()
			|| Component == c_HandCard.Get()
			|| Component == c_HeadCard.Get()
			|| !Component->IsVisible())
		{
			continue;
		}

		return Component;
	}

	return rootComp;
}

void ACollector::StartActionSpin()
{
	USceneComponent* Target = ResolveActionSpinTarget();
	if (!Target)
	{
		return;
	}

	ActiveActionSpinTarget = Target;
	ActionSpinStartRotation = Target->GetRelativeRotation().Quaternion();
	ActionSpinElapsed = 0.0f;
	bActionSpinActive = true;
	SetActorTickEnabled(true);
}
