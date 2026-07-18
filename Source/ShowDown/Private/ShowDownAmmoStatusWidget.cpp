#include "ShowDownAmmoStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UShowDownAmmoStatusWidget::SetAmmoStatus(
	int32 LiveRounds,
	int32 RemainingChambers,
	const FLinearColor& LiveRoundColor,
	const FLinearColor& BackgroundColor,
	float SlotDiameter,
	float SlotSpacing,
	bool bEmphasized)
{
	const int32 SafeRemainingChambers = FMath::Clamp(RemainingChambers, 0, ChamberSlotCount);
	const int32 SafeLiveRounds = FMath::Clamp(LiveRounds, 0, SafeRemainingChambers);
	const bool bBecameEmphasized = bHasCachedStatus && !bCachedEmphasized && bEmphasized;
	CachedSlotDiameter = FMath::Max(12.0f, SlotDiameter);
	CachedSlotSpacing = FMath::Max(0.0f, SlotSpacing);
	CachedBackgroundColor = BackgroundColor;
	bCachedEmphasized = bEmphasized;

	BuildDefaultWidget();
	if (SlotPulseElapsed.Num() != ChamberSlotCount)
	{
		SlotPulseElapsed.Init(SlotPulseDuration, ChamberSlotCount);
	}

	if (!bHasCachedStatus)
	{
		CommitAmmoStatus(SafeLiveRounds, SafeRemainingChambers, LiveRoundColor, false);
		bHasCachedStatus = true;
	}
	else if (bShotStatusCommitPending
		&& SafeLiveRounds == PendingLiveRounds
		&& SafeRemainingChambers == PendingRemainingChambers)
	{
		PendingLiveRoundColor = LiveRoundColor;
	}
	else
	{
		bShotStatusCommitPending = false;
		const bool bSingleShotTransition =
			CachedRemainingChambers - SafeRemainingChambers == 1
			&& (SafeLiveRounds == CachedLiveRounds
				|| SafeLiveRounds == CachedLiveRounds - 1);
		if (bSingleShotTransition)
		{
			PendingLiveRounds = SafeLiveRounds;
			PendingRemainingChambers = SafeRemainingChambers;
			PendingLiveRoundColor = LiveRoundColor;
			ShotStatusHoldElapsed = 0.0f;
			bShotStatusCommitPending = true;
		}
		else
		{
			const bool bSingleBulletLoaded =
				SafeRemainingChambers == CachedRemainingChambers
				&& SafeLiveRounds == CachedLiveRounds + 1;
			CommitAmmoStatus(
				SafeLiveRounds,
				SafeRemainingChambers,
				LiveRoundColor,
				bSingleBulletLoaded);
		}
	}

	if (bBecameEmphasized)
	{
		EmphasisPulseElapsed = 0.0f;
	}
	else if (!bCachedEmphasized)
	{
		EmphasisPulseElapsed = EmphasisPulseDuration;
	}

	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::ShowRaiseDelta(int32 AddedRounds)
{
	const int32 SafeAddedRounds = FMath::Clamp(AddedRounds, 0, ChamberSlotCount);
	if (SafeAddedRounds <= 0)
	{
		return;
	}

	BuildDefaultWidget();
	CachedRaiseDelta = SafeAddedRounds;
	bRaiseDeltaActive = true;
	bRaiseDeltaCompleting = false;
	RaiseDeltaElapsed = 0.0f;
	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::CompleteRaiseDelta()
{
	if (!bRaiseDeltaActive || bRaiseDeltaCompleting)
	{
		return;
	}

	bRaiseDeltaCompleting = true;
	RaiseDeltaElapsed = 0.0f;
	RefreshVisuals();
}

TSharedRef<SWidget> UShowDownAmmoStatusWidget::RebuildWidget()
{
	BuildDefaultWidget();
	return Super::RebuildWidget();
}

void UShowDownAmmoStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidget();
	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	bool bVisualChanged = false;
	if (bShotStatusCommitPending)
	{
		ShotStatusHoldElapsed += InDeltaTime;
		if (ShotStatusHoldElapsed >= ShotStatusHoldDuration)
		{
			bShotStatusCommitPending = false;
			CommitAmmoStatus(
				PendingLiveRounds,
				PendingRemainingChambers,
				PendingLiveRoundColor,
				true);
		}
		bVisualChanged = true;
	}
	if (RiskColorTransitionElapsed < RiskColorTransitionDuration)
	{
		RiskColorTransitionElapsed = FMath::Min(
			RiskColorTransitionDuration,
			RiskColorTransitionElapsed + InDeltaTime);
		const float Alpha = FMath::Clamp(
			RiskColorTransitionElapsed / RiskColorTransitionDuration,
			0.0f,
			1.0f);
		const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
		CurrentLiveRoundColor = FMath::Lerp(
			LiveRoundColorTransitionStart,
			TargetLiveRoundColor,
			EasedAlpha);
		bVisualChanged = true;
	}

	if (EmphasisPulseElapsed < EmphasisPulseDuration)
	{
		EmphasisPulseElapsed = FMath::Min(
			EmphasisPulseDuration,
			EmphasisPulseElapsed + InDeltaTime);
		bVisualChanged = true;
	}

	for (float& PulseElapsed : SlotPulseElapsed)
	{
		if (PulseElapsed < SlotPulseDuration)
		{
			PulseElapsed = FMath::Min(SlotPulseDuration, PulseElapsed + InDeltaTime);
			bVisualChanged = true;
		}
	}

	if (bRaiseDeltaActive)
	{
		RaiseDeltaElapsed += InDeltaTime;
		if (bRaiseDeltaCompleting && RaiseDeltaElapsed >= RaiseDeltaCompleteDuration)
		{
			bRaiseDeltaActive = false;
			bRaiseDeltaCompleting = false;
			CachedRaiseDelta = 0;
			RaiseDeltaElapsed = 0.0f;
		}
		bVisualChanged = true;
	}

	if (bVisualChanged)
	{
		RefreshVisuals();
	}
}

void UShowDownAmmoStatusWidget::BuildDefaultWidget()
{
	if (!WidgetTree || RootOverlay)
	{
		return;
	}

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("AmmoStatusRoot"));
	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AmmoStatusBackground"));
	Background->SetPadding(FMargin(14.0f, 8.0f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Background->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(Background))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Center);
		BackgroundSlot->SetVerticalAlignment(VAlign_Center);
	}

	StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AmmoStatusRow"));
	Background->SetContent(StatusRow);

	RaiseDeltaBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AmmoRaiseDeltaBadge"));
	RaiseDeltaBadge->SetPadding(FMargin(8.0f, 3.0f));
	RaiseDeltaBadge->SetHorizontalAlignment(HAlign_Center);
	RaiseDeltaBadge->SetVerticalAlignment(VAlign_Center);
	RaiseDeltaBadge->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	RaiseDeltaBadge->SetVisibility(ESlateVisibility::Collapsed);
	RaiseDeltaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AmmoRaiseDeltaText"));
	RaiseDeltaText->SetJustification(ETextJustify::Center);
	RaiseDeltaText->SetShadowOffset(FVector2D(0.0f, 2.0f));
	RaiseDeltaText->SetShadowColorAndOpacity(FLinearColor::Black);
	RaiseDeltaBadge->SetContent(RaiseDeltaText);
	if (UOverlaySlot* BadgeSlot = RootOverlay->AddChildToOverlay(RaiseDeltaBadge))
	{
		BadgeSlot->SetHorizontalAlignment(HAlign_Center);
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
	}

	SlotContainers.Reserve(ChamberSlotCount);
	SlotCircles.Reserve(ChamberSlotCount);
	SpentMarks.Reserve(ChamberSlotCount);
	for (int32 SlotIndex = 0; SlotIndex < ChamberSlotCount; ++SlotIndex)
	{
		USizeBox* SlotContainer = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("AmmoSlotContainer%d"), SlotIndex + 1));
		SlotContainer->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			*FString::Printf(TEXT("AmmoSlotOverlay%d"), SlotIndex + 1));
		SlotContainer->SetContent(SlotOverlay);

		UBorder* SlotCircle = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("AmmoSlotCircle%d"), SlotIndex + 1));
		SlotCircle->SetPadding(FMargin(0.0f));
		SlotCircle->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* CircleSlot = SlotOverlay->AddChildToOverlay(SlotCircle))
		{
			CircleSlot->SetHorizontalAlignment(HAlign_Fill);
			CircleSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* SpentMark = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("AmmoSlotSpentMark%d"), SlotIndex + 1));
		SpentMark->SetText(FText::FromString(TEXT("/")));
		SpentMark->SetJustification(ETextJustify::Center);
		SpentMark->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* MarkSlot = SlotOverlay->AddChildToOverlay(SpentMark))
		{
			MarkSlot->SetHorizontalAlignment(HAlign_Center);
			MarkSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* ContainerSlot = StatusRow->AddChildToHorizontalBox(SlotContainer))
		{
			ContainerSlot->SetHorizontalAlignment(HAlign_Center);
			ContainerSlot->SetVerticalAlignment(VAlign_Center);
			ContainerSlot->SetPadding(FMargin(SlotIndex > 0 ? CachedSlotSpacing : 0.0f, 0.0f, 0.0f, 0.0f));
		}

		SlotContainers.Add(SlotContainer);
		SlotCircles.Add(SlotCircle);
		SpentMarks.Add(SpentMark);
	}

	SlotPulseElapsed.Init(SlotPulseDuration, ChamberSlotCount);
	WidgetTree->RootWidget = RootOverlay;
	RefreshVisuals();
}

void UShowDownAmmoStatusWidget::CommitAmmoStatus(
	int32 LiveRounds,
	int32 RemainingChambers,
	const FLinearColor& LiveRoundColor,
	bool bAnimateSlotChanges)
{
	const int32 PreviousLiveRounds = CachedLiveRounds;
	const int32 PreviousRemainingChambers = CachedRemainingChambers;
	CachedRemainingChambers = FMath::Clamp(RemainingChambers, 0, ChamberSlotCount);
	CachedLiveRounds = FMath::Clamp(LiveRounds, 0, CachedRemainingChambers);

	TargetLiveRoundColor = LiveRoundColor;
	if (!bHasCachedStatus)
	{
		CurrentLiveRoundColor = TargetLiveRoundColor;
		LiveRoundColorTransitionStart = TargetLiveRoundColor;
		RiskColorTransitionElapsed = RiskColorTransitionDuration;
	}
	else if (!CurrentLiveRoundColor.Equals(TargetLiveRoundColor, KINDA_SMALL_NUMBER))
	{
		LiveRoundColorTransitionStart = CurrentLiveRoundColor;
		RiskColorTransitionElapsed = 0.0f;
	}

	if (!bAnimateSlotChanges || SlotPulseElapsed.Num() != ChamberSlotCount)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < ChamberSlotCount; ++SlotIndex)
	{
		const EShowDownAmmoSlotState PreviousState = ResolveSlotStateForCounts(
			SlotIndex,
			PreviousLiveRounds,
			PreviousRemainingChambers);
		if (PreviousState != ResolveSlotState(SlotIndex))
		{
			SlotPulseElapsed[SlotIndex] = 0.0f;
		}
	}
}

void UShowDownAmmoStatusWidget::RefreshVisuals()
{
	if (Background)
	{
		FLinearColor PanelColor = CachedBackgroundColor;
		PanelColor.A *= bCachedEmphasized ? 1.0f : 0.55f;
		FLinearColor PanelOutlineColor = CurrentLiveRoundColor;
		PanelOutlineColor.A = bCachedEmphasized ? 0.62f : 0.18f;
		Background->SetBrush(FSlateRoundedBoxBrush(
			PanelColor,
			12.0f,
			PanelOutlineColor,
			bCachedEmphasized ? 1.5f : 1.0f));
		Background->SetBrushColor(FLinearColor::White);
		Background->SetRenderScale(FVector2D(ResolvePanelScale()));
	}

	if (RaiseDeltaBadge && RaiseDeltaText)
	{
		if (!bRaiseDeltaActive || CachedRaiseDelta <= 0)
		{
			RaiseDeltaBadge->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			RaiseDeltaBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
			RaiseDeltaText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), CachedRaiseDelta)));
			FSlateFontInfo DeltaFont(
				FCoreStyle::GetDefaultFont(),
				FMath::RoundToInt(CachedSlotDiameter * 0.64f));
			DeltaFont.OutlineSettings.OutlineSize = 2;
			DeltaFont.OutlineSettings.OutlineColor = FLinearColor::Black;
			RaiseDeltaText->SetFont(DeltaFont);
			RaiseDeltaText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.68f, 0.20f, 1.0f)));
			RaiseDeltaBadge->SetBrush(FSlateRoundedBoxBrush(
				FLinearColor(0.055f, 0.03f, 0.012f, 0.96f),
				8.0f,
				FLinearColor(1.0f, 0.42f, 0.06f, 0.94f),
				1.5f));
			RaiseDeltaBadge->SetBrushColor(FLinearColor::White);

			float BadgeScale = 1.0f;
			float BadgeOpacity = 1.0f;
			if (!bRaiseDeltaCompleting)
			{
				const float AppearAlpha = FMath::Clamp(
					RaiseDeltaElapsed / RaiseDeltaAppearDuration,
					0.0f,
					1.0f);
				BadgeScale = FMath::InterpEaseOut(0.72f, 1.0f, AppearAlpha, 2.5f);
				BadgeOpacity = FMath::SmoothStep(0.0f, 1.0f, AppearAlpha);
			}
			else
			{
				const float CompleteAlpha = FMath::Clamp(
					RaiseDeltaElapsed / RaiseDeltaCompleteDuration,
					0.0f,
					1.0f);
				if (CompleteAlpha < 0.30f)
				{
					BadgeScale = FMath::InterpEaseOut(
						1.0f,
						1.18f,
						CompleteAlpha / 0.30f,
						2.0f);
				}
				else
				{
					const float FadeAlpha = (CompleteAlpha - 0.30f) / 0.70f;
					BadgeScale = FMath::InterpEaseInOut(1.18f, 0.88f, FadeAlpha, 2.0f);
					BadgeOpacity = 1.0f - FMath::SmoothStep(0.0f, 1.0f, FadeAlpha);
				}
			}

			const float RowWidth = CachedSlotDiameter * static_cast<float>(ChamberSlotCount)
				+ CachedSlotSpacing * static_cast<float>(ChamberSlotCount - 1);
			RaiseDeltaBadge->SetRenderTranslation(FVector2D(
				RowWidth * 0.5f + 10.0f,
				-CachedSlotDiameter * 0.58f));
			RaiseDeltaBadge->SetRenderScale(FVector2D(BadgeScale));
			RaiseDeltaBadge->SetRenderOpacity(BadgeOpacity);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < ChamberSlotCount; ++SlotIndex)
	{
		if (!SlotContainers.IsValidIndex(SlotIndex)
			|| !SlotCircles.IsValidIndex(SlotIndex)
			|| !SpentMarks.IsValidIndex(SlotIndex))
		{
			continue;
		}

		USizeBox* SlotContainer = SlotContainers[SlotIndex];
		UBorder* SlotCircle = SlotCircles[SlotIndex];
		UTextBlock* SpentMark = SpentMarks[SlotIndex];
		if (!SlotContainer || !SlotCircle || !SpentMark)
		{
			continue;
		}

		SlotContainer->SetWidthOverride(CachedSlotDiameter);
		SlotContainer->SetHeightOverride(CachedSlotDiameter);
		SlotContainer->SetRenderScale(FVector2D(ResolveSlotPulseScale(SlotIndex)));
		if (UHorizontalBoxSlot* ContainerSlot = Cast<UHorizontalBoxSlot>(SlotContainer->Slot))
		{
			ContainerSlot->SetPadding(FMargin(SlotIndex > 0 ? CachedSlotSpacing : 0.0f, 0.0f, 0.0f, 0.0f));
		}

		const EShowDownAmmoSlotState SlotState = ResolveSlotState(SlotIndex);
		FLinearColor FillColor;
		FLinearColor OutlineColor;
		float OutlineWidth = 2.0f;
		switch (SlotState)
		{
		case EShowDownAmmoSlotState::Live:
		{
			FillColor = CurrentLiveRoundColor;
			OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.92f);
			if (SlotPulseElapsed.IsValidIndex(SlotIndex)
				&& SlotPulseElapsed[SlotIndex] < SlotPulseDuration)
			{
				const float PulseAlpha = FMath::Clamp(
					SlotPulseElapsed[SlotIndex] / SlotPulseDuration,
					0.0f,
					1.0f);
				const FLinearColor MetallicFlash(1.0f, 0.84f, 0.52f, 1.0f);
				FillColor = FMath::Lerp(MetallicFlash, CurrentLiveRoundColor, FMath::SmoothStep(0.0f, 1.0f, PulseAlpha));
				OutlineColor = FMath::Lerp(
					FLinearColor(1.0f, 0.92f, 0.72f, 0.95f),
					OutlineColor,
					PulseAlpha);
			}
			break;
		}
		case EShowDownAmmoSlotState::Empty:
			FillColor = FLinearColor(0.025f, 0.028f, 0.035f, 0.72f);
			OutlineColor = FLinearColor(0.76f, 0.79f, 0.84f, 0.88f);
			OutlineWidth = 3.0f;
			break;
		case EShowDownAmmoSlotState::Spent:
		default:
			FillColor = FLinearColor(0.07f, 0.075f, 0.085f, 0.92f);
			OutlineColor = FLinearColor(0.26f, 0.28f, 0.32f, 0.78f);
			break;
		}

		SlotCircle->SetBrush(FSlateRoundedBoxBrush(
			FillColor,
			CachedSlotDiameter * 0.5f,
			OutlineColor,
			OutlineWidth));
		SlotCircle->SetBrushColor(FLinearColor::White);

		FSlateFontInfo SpentFont(FCoreStyle::GetDefaultFont(), FMath::RoundToInt(CachedSlotDiameter * 0.72f));
		SpentFont.OutlineSettings.OutlineSize = 1;
		SpentFont.OutlineSettings.OutlineColor = FLinearColor::Black;
		SpentMark->SetFont(SpentFont);
		SpentMark->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.60f, 0.64f, 0.92f)));
		SpentMark->SetVisibility(
			SlotState == EShowDownAmmoSlotState::Spent
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

EShowDownAmmoSlotState UShowDownAmmoStatusWidget::ResolveSlotState(int32 SlotIndex) const
{
	return ResolveSlotStateForCounts(SlotIndex, CachedLiveRounds, CachedRemainingChambers);
}

EShowDownAmmoSlotState UShowDownAmmoStatusWidget::ResolveSlotStateForCounts(
	int32 SlotIndex,
	int32 LiveRounds,
	int32 RemainingChambers)
{
	if (SlotIndex < 0 || SlotIndex >= ChamberSlotCount)
	{
		return EShowDownAmmoSlotState::Spent;
	}

	const int32 SafeRemainingChambers = FMath::Clamp(RemainingChambers, 0, ChamberSlotCount);
	const int32 SafeLiveRounds = FMath::Clamp(LiveRounds, 0, SafeRemainingChambers);
	if (SlotIndex < SafeLiveRounds)
	{
		return EShowDownAmmoSlotState::Live;
	}
	if (SlotIndex < SafeRemainingChambers)
	{
		return EShowDownAmmoSlotState::Empty;
	}
	return EShowDownAmmoSlotState::Spent;
}

float UShowDownAmmoStatusWidget::ResolveSlotPulseScale(int32 SlotIndex) const
{
	if (!SlotPulseElapsed.IsValidIndex(SlotIndex)
		|| SlotPulseElapsed[SlotIndex] >= SlotPulseDuration)
	{
		return 1.0f;
	}

	const float Alpha = FMath::Clamp(SlotPulseElapsed[SlotIndex] / SlotPulseDuration, 0.0f, 1.0f);
	if (ResolveSlotState(SlotIndex) == EShowDownAmmoSlotState::Live)
	{
		if (Alpha < 0.55f)
		{
			return FMath::InterpEaseOut(0.70f, 1.28f, Alpha / 0.55f, 2.5f);
		}
		return FMath::InterpEaseInOut(1.28f, 1.0f, (Alpha - 0.55f) / 0.45f, 2.0f);
	}

	return 1.0f + FMath::Sin(Alpha * PI) * 0.30f;
}

float UShowDownAmmoStatusWidget::ResolvePanelScale() const
{
	if (!bCachedEmphasized)
	{
		return 1.0f;
	}
	if (EmphasisPulseElapsed >= EmphasisPulseDuration)
	{
		return 1.08f;
	}

	const float Alpha = FMath::Clamp(EmphasisPulseElapsed / EmphasisPulseDuration, 0.0f, 1.0f);
	if (Alpha < 0.45f)
	{
		return FMath::InterpEaseOut(1.0f, 1.25f, Alpha / 0.45f, 2.5f);
	}
	return FMath::InterpEaseInOut(1.25f, 1.08f, (Alpha - 0.45f) / 0.55f, 2.0f);
}
