#if WITH_DEV_AUTOMATION_TESTS

#include "BettingSystem.h"
#include "CardSystem.h"
#include "CollectorAISystem.h"
#include "Misc/AutomationTest.h"
#include "Presentation/SDCardRevealLayout.h"
#include "RoundResolver.h"
#include "RouletteSystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownBettingSystemTest,
	"ShowDown.Core.Betting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownBettingSystemTest::RunTest(const FString& Parameters)
{
	UBettingSystem* BettingSystem = NewObject<UBettingSystem>();
	TestNotNull(TEXT("Betting system can be created"), BettingSystem);
	if (!BettingSystem)
	{
		return false;
	}

	BettingSystem->ResetBetting(-10);
	TestEqual(TEXT("Minimum bet is clamped to zero"), BettingSystem->GetCurrentBet(), 0);

	TestTrue(TEXT("A valid raise succeeds"), BettingSystem->RaiseTo(EShowDownSide::Player, 3));
	TestEqual(TEXT("A valid raise updates the table bet"), BettingSystem->GetCurrentBet(), 3);
	TestFalse(TEXT("A non-increasing raise is rejected"), BettingSystem->RaiseTo(EShowDownSide::Collector, 3));
	TestFalse(TEXT("A raise above chamber capacity is rejected"), BettingSystem->RaiseTo(EShowDownSide::Collector, 7));

	BettingSystem->ResetBetting(99);
	TestEqual(TEXT("Minimum bet is clamped to chamber capacity"), BettingSystem->GetCurrentBet(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownRoundResolverTest,
	"ShowDown.Core.RoundResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownRoundResolverTest::RunTest(const FString& Parameters)
{
	URoundResolver* RoundResolver = NewObject<URoundResolver>();
	TestNotNull(TEXT("Round resolver can be created"), RoundResolver);
	if (!RoundResolver)
	{
		return false;
	}

	TestEqual(
		TEXT("Higher player card wins"),
		RoundResolver->ResolveRevealedCards(7, 2),
		EShowDownRoundResult::PlayerWin);
	TestEqual(
		TEXT("Higher collector card wins"),
		RoundResolver->ResolveRevealedCards(1, 5),
		EShowDownRoundResult::CollectorWin);
	TestEqual(
		TEXT("Equal cards draw"),
		RoundResolver->ResolveRevealedCards(4, 4),
		EShowDownRoundResult::Draw);
	TestEqual(
		TEXT("Seven-card fold overrides a one-bullet bet with a full cylinder"),
		RoundResolver->GetFoldLoadCount(7, 1, true),
		6);
	TestEqual(
		TEXT("Seven-card fold keeps the committed bet when the rule is disabled"),
		RoundResolver->GetFoldLoadCount(7, 1, false),
		1);
	TestEqual(
		TEXT("Non-seven fold keeps the committed bet"),
		RoundResolver->GetFoldLoadCount(6, 1, true),
		1);
	TestEqual(
		TEXT("Normal fold load is clamped"),
		RoundResolver->GetFoldLoadCount(3, 9, true),
		6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownRouletteSystemTest,
	"ShowDown.Core.Roulette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownRouletteSystemTest::RunTest(const FString& Parameters)
{
	URouletteSystem* RouletteSystem = NewObject<URouletteSystem>();
	TestNotNull(TEXT("Roulette system can be created"), RouletteSystem);
	if (!RouletteSystem)
	{
		return false;
	}

	TestEqual(TEXT("Empty chamber chance is zero"), RouletteSystem->GetHitChance(0), 0.0f);
	TestEqual(TEXT("Full chamber chance is one"), RouletteSystem->GetHitChance(6), 1.0f);
	TestFalse(TEXT("Zero bullets can never hit"), RouletteSystem->RollRoulette(0));
	TestTrue(TEXT("Six bullets always hit"), RouletteSystem->RollRoulette(6));
	TestFalse(TEXT("Negative bullets are clamped to empty"), RouletteSystem->RollRoulette(-1));
	TestTrue(TEXT("Excess bullets are clamped to full"), RouletteSystem->RollRoulette(99));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownCardSystemTest,
	"ShowDown.Core.CardSystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownCardSystemTest::RunTest(const FString& Parameters)
{
	UCardSystem* CardSystem = NewObject<UCardSystem>();
	TestNotNull(TEXT("Card system can be created"), CardSystem);
	if (!CardSystem)
	{
		return false;
	}

	CardSystem->ResetDeck(2);
	TestEqual(TEXT("Two copies of seven ranks create fourteen cards"), CardSystem->GetRemainingCardCount(), 14);

	TArray<int32> DealtCards;
	TestTrue(TEXT("The complete deck can be dealt"), CardSystem->DealCards(14, DealtCards));
	TestEqual(TEXT("Dealing the complete deck returns fourteen cards"), DealtCards.Num(), 14);
	TestEqual(TEXT("The deck is empty after all cards are dealt"), CardSystem->GetRemainingCardCount(), 0);

	TArray<int32> RankCounts;
	RankCounts.Init(0, 8);
	for (const int32 Rank : DealtCards)
	{
		TestTrue(TEXT("Dealt ranks stay in the supported range"), Rank >= 1 && Rank <= 7);
		if (RankCounts.IsValidIndex(Rank))
		{
			++RankCounts[Rank];
		}
	}
	for (int32 Rank = 1; Rank <= 7; ++Rank)
	{
		TestEqual(*FString::Printf(TEXT("Rank %d keeps its configured copy count"), Rank), RankCounts[Rank], 2);
	}

	TestFalse(TEXT("Dealing from an empty deck fails"), CardSystem->DealCards(1, DealtCards));
	TestEqual(TEXT("A failed deal clears the output"), DealtCards.Num(), 0);
	CardSystem->ResetDeck(-1);
	TestEqual(TEXT("Negative deck copies clamp to an empty deck"), CardSystem->GetRemainingCardCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownCardRevealLayoutTest,
	"ShowDown.Core.CardRevealLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownCardRevealLayoutTest::RunTest(const FString& Parameters)
{
	const FVector TableCenter(100.0f, 200.0f, 20.0f);
	const float CenterDistance = 50.0f;
	const float HeightOffset = -6.0f;
	const FRotator RotationOffset(-90.0f, 0.0f, 0.0f);
	const FVector Scale(1.12f);

	struct FSeatCase
	{
		const TCHAR* Label;
		FVector SeatLocation;
		FVector ExpectedLocation;
		float ExpectedYaw;
	};

	const FSeatCase SeatCases[] = {
		{ TEXT("Player 1"), TableCenter + FVector(-100.0f, 0.0f, 0.0f), TableCenter + FVector(-50.0f, 0.0f, HeightOffset), 180.0f },
		{ TEXT("Player 2"), TableCenter + FVector(100.0f, 0.0f, 0.0f), TableCenter + FVector(50.0f, 0.0f, HeightOffset), 0.0f },
		{ TEXT("Player 3"), TableCenter + FVector(0.0f, 100.0f, 0.0f), TableCenter + FVector(0.0f, 50.0f, HeightOffset), 90.0f },
		{ TEXT("Player 4"), TableCenter + FVector(0.0f, -100.0f, 0.0f), TableCenter + FVector(0.0f, -50.0f, HeightOffset), -90.0f },
	};

	for (const FSeatCase& SeatCase : SeatCases)
	{
		FTransform Transform;
		TestTrue(
			*FString::Printf(TEXT("%s radial transform resolves"), SeatCase.Label),
			ShowDownCardRevealLayout::TryBuildRadialTransform(
				TableCenter,
				SeatCase.SeatLocation,
				CenterDistance,
				0.0f,
				HeightOffset,
				RotationOffset,
				Scale,
				Transform));
		TestTrue(
			*FString::Printf(TEXT("%s stays at the shared center distance"), SeatCase.Label),
			Transform.GetLocation().Equals(SeatCase.ExpectedLocation, KINDA_SMALL_NUMBER));

		const FQuat ExpectedRotation =
			(FRotator(0.0f, SeatCase.ExpectedYaw, 0.0f).Quaternion()
				* RotationOffset.Quaternion()).GetNormalized();
		TestTrue(
			*FString::Printf(TEXT("%s reveal card rotates with its seat"), SeatCase.Label),
			Transform.GetRotation().Equals(ExpectedRotation, KINDA_SMALL_NUMBER));
	}

	FTransform InvalidTransform;
	TestFalse(
		TEXT("A seat at the table center cannot define a radial reveal direction"),
		ShowDownCardRevealLayout::TryBuildRadialTransform(
			TableCenter,
			TableCenter,
			CenterDistance,
			0.0f,
			HeightOffset,
			RotationOffset,
			Scale,
			InvalidTransform));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownCollectorAISystemTest,
	"ShowDown.Core.CollectorAI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownCollectorAISystemTest::RunTest(const FString& Parameters)
{
	UCollectorAISystem* CollectorAI = NewObject<UCollectorAISystem>();
	TestNotNull(TEXT("Collector AI system can be created"), CollectorAI);
	if (!CollectorAI)
	{
		return false;
	}

	const TArray<int32> HandRanks = {5, 2, 7, 3};
	CollectorAI->Settings.GiveStrategy = ECollectorGiveStrategy::Lowest;
	TestEqual(TEXT("Lowest strategy chooses the lowest card"), CollectorAI->ChooseCardToGive(HandRanks), 2);
	CollectorAI->Settings.GiveStrategy = ECollectorGiveStrategy::Highest;
	TestEqual(TEXT("Highest strategy chooses the highest card"), CollectorAI->ChooseCardToGive(HandRanks), 7);
	TestEqual(TEXT("An empty hand has no card to give"), CollectorAI->ChooseCardToGive({}), 0);

	TestEqual(TEXT("High confidence raises below capacity"), CollectorAI->ChooseBetAction(0.8f, 2), EShowDownBetAction::Raise);
	TestEqual(TEXT("Low confidence folds against a large bet"), CollectorAI->ChooseBetAction(0.2f, 3), EShowDownBetAction::Fold);
	TestEqual(TEXT("A neutral opening action checks"), CollectorAI->ChooseBetAction(0.5f, 0), EShowDownBetAction::Check);
	TestEqual(TEXT("A neutral response calls an existing bet"), CollectorAI->ChooseBetAction(0.5f, 2), EShowDownBetAction::Call);
	return true;
}

#endif
