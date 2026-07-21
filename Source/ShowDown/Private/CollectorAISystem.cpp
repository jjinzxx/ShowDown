#include "CollectorAISystem.h"
#include "Card.h"

UCollectorAISystem::UCollectorAISystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UCollectorAISystem::ChooseCardToGive(const TArray<int32>& HandRanks) const
{
	if (HandRanks.Num() == 0){return 0;}

	TArray<int32> SortedRanks = HandRanks;
	SortedRanks.Sort();

	const float RandomValue = FMath::FRand();
	switch (Settings.GiveStrategy)
	{
	case ECollectorGiveStrategy::Lowest:
		return SortedRanks[0];

	case ECollectorGiveStrategy::Highest:
		return SortedRanks.Last();

	case ECollectorGiveStrategy::Random:
		return HandRanks[FMath::RandRange(0, HandRanks.Num() - 1)];

	case ECollectorGiveStrategy::Weighted:
		{
			const float Bias = FMath::Clamp(Settings.LowBias, 0.0f, 1.0f);
			float TotalWeight = 0.0f;
			TArray<float> Weights;

			for (int32 Index = 0; Index < SortedRanks.Num(); ++Index)
			{
				const float Weight = FMath::Pow(1.0f - Bias + 0.000001f, static_cast<float>(Index));
				Weights.Add(Weight);
				TotalWeight += Weight;
			}

			float Pick = FMath::FRandRange(0.0f, TotalWeight);
			for (int32 Index = 0; Index < SortedRanks.Num(); ++Index)
			{
				Pick -= Weights[Index];
				if (Pick <= 0.0f)
				{
					return SortedRanks[Index];
				}
			}

			return SortedRanks.Last();
		}

	case ECollectorGiveStrategy::Mixed:
		if (RandomValue < 0.7f)
		{
			return SortedRanks[0];
		}
		if (RandomValue < 0.9f)
		{
			return SortedRanks[SortedRanks.Num() / 2];
		}
		return SortedRanks.Last();

	default:
		return SortedRanks[0];
	}
}

ACard* UCollectorAISystem::ChooseCardActorToGive(const TArray<ACard*>& HandCards) const
{
	TArray<int32> HandRanks;
	for (ACard* Card : HandCards)
	{
		if (Card)
		{
			HandRanks.Add(Card->Rank);
		}
	}

	const int32 ChosenRank = ChooseCardToGive(HandRanks);
	for (ACard* Card : HandCards)
	{
		if (!Card){continue;}
		if (Card->Rank == ChosenRank)
		{
			return Card;
		}
	}

	return nullptr;
}



EShowDownBetAction UCollectorAISystem::ChooseBetAction(float EstimatedWinChance, int32 CurrentBet) const
{
	if (EstimatedWinChance > 0.7f && CurrentBet < 6)
	{
		return EShowDownBetAction::Raise;
	}

	if (EstimatedWinChance < 0.35f && CurrentBet > 2)
	{
		return EShowDownBetAction::Fold;
	}

	return CurrentBet == 0 ? EShowDownBetAction::Check : EShowDownBetAction::Call;
}

EShowDownBetAction UCollectorAISystem::ChooseBetActionByGivenCard(int32 GivenCardRank, int32 CurrentBet) const
{
	if (GivenCardRank <= 0)
	{
		return CurrentBet == 0 ? EShowDownBetAction::Check : EShowDownBetAction::Call;
	}

	// 콜렉터는 자기가 플레이어에게 준 카드만 알고 판단합니다.
	if (GivenCardRank <= 3)
	{
		return CurrentBet < 6 ? EShowDownBetAction::Raise : EShowDownBetAction::Call;
	}

	return CurrentBet <= 1 ? EShowDownBetAction::Check : EShowDownBetAction::Call;
}

FCollectorBetDecision UCollectorAISystem::ChooseBetDecisionByModel(
	const TArray<ACard*>& OwnHandCards,
	int32 OpponentForeheadCard,
	int32 CurrentBet,
	int32 OwnCommittedBet,
	int32 MaxRaise,
	int32 RaisesLeft,
	bool bLastRaiserWasCollector) const
{
	const FCollectorLLMInfluence NoInfluence;
	return ChooseBetDecisionByModelWithInfluence(
		OwnHandCards,
		OpponentForeheadCard,
		CurrentBet,
		OwnCommittedBet,
		MaxRaise,
		RaisesLeft,
		bLastRaiserWasCollector,
		NoInfluence);
}

FCollectorBetDecision UCollectorAISystem::ChooseBetDecisionByModelWithInfluence(
	const TArray<ACard*>& OwnHandCards,
	int32 OpponentForeheadCard,
	int32 CurrentBet,
	int32 OwnCommittedBet,
	int32 MaxRaise,
	int32 RaisesLeft,
	bool bLastRaiserWasCollector,
	const FCollectorLLMInfluence& LLMInfluence) const
{
	FCollectorBetDecision Decision;
	const float EffectiveAggression = FMath::Clamp(Settings.Aggression + LLMInfluence.AggressionDelta, 0.0f, 1.0f);
	const float EffectiveBluffRate = FMath::Clamp(Settings.BluffRate + LLMInfluence.BluffRateDelta, 0.0f, 1.0f);
	const float EffectiveTimidity = FMath::Clamp(Settings.Timidity + LLMInfluence.TimidityDelta, 0.0f, 1.0f);

	const int32 ClampedCurrentBet = FMath::Clamp(CurrentBet, 1, 6);
	const int32 ClampedCommittedBet = FMath::Clamp(OwnCommittedBet, 0, 6);
	const int32 RaiseCap = FMath::Clamp(MaxRaise, ClampedCurrentBet, 6);

	TArray<int32> Counts;
	Counts.Init(0, 8);
	for (int32 Rank = 1; Rank <= 7; ++Rank)
	{
		Counts[Rank] = 2;
	}

	for (ACard* Card : OwnHandCards)
	{
		if (Card)
		{
			Counts[Card->Rank] = FMath::Max(0, Counts[Card->Rank] - 1);
		}
	}

	if (OpponentForeheadCard >= 1 && OpponentForeheadCard <= 7)
	{
		Counts[OpponentForeheadCard] = FMath::Max(0, Counts[OpponentForeheadCard] - 1);
	}

	float Total = 0.0f;
	float Win = 0.0f;
	float Tie = 0.0f;
	float Seven = 0.0f;
	for (int32 Rank = 1; Rank <= 7; ++Rank)
	{
		const float Count = static_cast<float>(Counts[Rank]);
		Total += Count;

		if (Rank > OpponentForeheadCard)
		{
			Win += Count;
		}
		else if (Rank == OpponentForeheadCard)
		{
			Tie += Count;
		}

		if (Rank == 7)
		{
			Seven += Count;
		}
	}

	const float PWin = Total > 0.0f ? Win / Total : 0.5f;
	const float PTie = Total > 0.0f ? Tie / Total : 0.0f;
	const float P7 = Total > 0.0f ? Seven / Total : 0.0f;
	const float PLose = FMath::Clamp(1.0f - PWin - PTie, 0.0f, 1.0f);
	const float Noise = FMath::FRandRange(-Settings.Noise, Settings.Noise);

	const float CallNet = PWin * (static_cast<float>(ClampedCurrentBet) / 6.0f)
		- (PLose + PTie) * (static_cast<float>(ClampedCurrentBet) / 6.0f)
		+ Noise;
	const float FoldNet = -(P7 * 1.0f + (1.0f - P7) * (static_cast<float>(ClampedCommittedBet) / 6.0f));
	const bool bMustRespond = ClampedCommittedBet < ClampedCurrentBet;

	if (bMustRespond && FoldNet > CallNet + EffectiveTimidity * 0.05f)
	{
		Decision.Action = EShowDownBetAction::Fold;
		return Decision;
	}

	const float Confidence = PWin - PLose;
	int32 TargetBet = 0;
	if (Confidence > 0.1f)
	{
		TargetBet = FMath::RoundToInt(
			static_cast<float>(ClampedCurrentBet)
			+ Confidence * EffectiveAggression * static_cast<float>(RaiseCap - ClampedCurrentBet));
	}

	const float BluffChance = EffectiveBluffRate * (1.0f - FMath::Max(0.0f, Confidence));
	if (FMath::FRand() < BluffChance && RaiseCap > ClampedCurrentBet)
	{
		TargetBet = FMath::Max(
			TargetBet,
			FMath::RandRange(ClampedCurrentBet + 1, RaiseCap));
	}

	TargetBet = FMath::Clamp(TargetBet, 0, RaiseCap);

	const float RaiseChance = bLastRaiserWasCollector
		? EffectiveBluffRate * 0.5f
		: FMath::Min(1.0f, 0.35f + FMath::Max(0.0f, Confidence) * EffectiveAggression * 1.2f);

	if (TargetBet > ClampedCurrentBet && RaisesLeft > 0 && FMath::FRand() < RaiseChance)
	{
		Decision.Action = EShowDownBetAction::Raise;
		Decision.TargetBet = TargetBet;
		return Decision;
	}

	Decision.Action = ClampedCommittedBet < ClampedCurrentBet ? EShowDownBetAction::Call : EShowDownBetAction::Check;
	return Decision;
}
