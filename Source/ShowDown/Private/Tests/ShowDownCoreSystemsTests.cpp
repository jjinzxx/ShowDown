#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Audio/ShowDownAudioConfig.h"
#include "Audio/ShowDownAudioSubsystem.h"
#include "BettingSystem.h"
#include "CardSystem.h"
#include "CollectorAISystem.h"
#include "Misc/AutomationTest.h"
#include "Presentation/SDCardRevealLayout.h"
#include "Presentation/SDGunVisionSequenceSubsystem.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "Presentation/SDVisionDirector.h"
#include "RoundResolver.h"
#include "RouletteSystem.h"
#include "SDMultiplayerRoundFlow.h"
#include "SDPlayerState.h"
#include "ShowDownCharacter.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownGameModeBase.h"
#include "ShowDownGameStateBase.h"
#include "ShowDownShopWidget.h"
#include "ShowDownTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

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
	FShowDownMultiplayerRoundFlowTest,
	"ShowDown.Core.MultiplayerRoundFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownMultiplayerRoundFlowTest::RunTest(const FString& Parameters)
{
	using namespace ShowDownMultiplayerRoundFlow;
	using namespace ShowDownTableCinematics;

	TestEqual(TEXT("No player slot has no cinematic mask bit"),
		PlayerSlotToMask(EShowDownPlayerSlot::None), static_cast<uint8>(0));
	TestEqual(TEXT("Player one uses the first cinematic mask bit"),
		PlayerSlotToMask(EShowDownPlayerSlot::Player1), static_cast<uint8>(1));
	TestEqual(TEXT("Player four uses the fourth cinematic mask bit"),
		PlayerSlotToMask(EShowDownPlayerSlot::Player4), static_cast<uint8>(8));
	const uint8 AlternatingPlayerMask = PlayerSlotToMask(EShowDownPlayerSlot::Player2)
		| PlayerSlotToMask(EShowDownPlayerSlot::Player4);
	TestTrue(TEXT("Cinematic mask includes its selected player"),
		IsPlayerSlotInMask(AlternatingPlayerMask, EShowDownPlayerSlot::Player2));
	TestFalse(TEXT("Cinematic mask excludes an unselected player"),
		IsPlayerSlotInMask(AlternatingPlayerMask, EShowDownPlayerSlot::Player3));
	const uint8 SinglePlayerMask = SingleSideToMask(EShowDownSide::Player);
	const uint8 SingleCollectorMask = SingleSideToMask(EShowDownSide::Collector);
	TestTrue(TEXT("Single-player cinematic mask selects the player side"),
		IsSingleSideInMask(SinglePlayerMask, EShowDownSide::Player));
	TestFalse(TEXT("Single-player cinematic mask excludes the Collector side"),
		IsSingleSideInMask(SinglePlayerMask, EShowDownSide::Collector));
	TestTrue(TEXT("Single-player side bits do not overlap multiplayer seats"),
		(SingleCollectorMask & AlternatingPlayerMask) == 0);

	TestEqual(
		TEXT("No active player ends the round"),
		ResolvePostBetDecision(0, false),
		ESDMultiplayerPostBetDecision::EndRound);
	TestEqual(
		TEXT("A lone survivor ends without exposing their card"),
		ResolvePostBetDecision(1, true),
		ESDMultiplayerPostBetDecision::EndRound);
	TestEqual(
		TEXT("Multiple unfinished players continue betting"),
		ResolvePostBetDecision(3, false),
		ESDMultiplayerPostBetDecision::ContinueBetting);
	TestEqual(
		TEXT("Multiple completed players enter showdown"),
		ResolvePostBetDecision(2, true),
		ESDMultiplayerPostBetDecision::RevealCards);

	TestEqual(
		TEXT("A longer reveal presentation extends the cinematic beat"),
		ResolveRevealCompletionDelay(2.25f, 1.5f),
		2.25f);
	TestEqual(
		TEXT("A valid reveal presentation owns the completion timing"),
		ResolveRevealCompletionDelay(0.5f, 1.5f),
		0.5f);
	TestEqual(
		TEXT("Negative fallback timing input is clamped"),
		ResolveRevealCompletionDelay(-1.0f, -2.0f),
		0.0f);
	TestEqual(
		TEXT("The fallback beat is used when no reveal presentation runs"),
		ResolveRevealCompletionDelay(0.0f, 2.1f),
		2.1f);
	TestEqual(
		TEXT("A long reveal presentation keeps its full duration"),
		ResolveRevealCompletionDelay(5.5f, 2.1f),
		5.5f);

	const TArray<EShowDownPlayerSlot> AllAliveSlots = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player2,
		EShowDownPlayerSlot::Player3,
		EShowDownPlayerSlot::Player4
	};
	const TArray<EShowDownPlayerSlot> AliveAfterPlayer4 = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player2,
		EShowDownPlayerSlot::Player3
	};
	const TArray<EShowDownPlayerSlot> AliveWithoutPlayer2 = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player3,
		EShowDownPlayerSlot::Player4
	};
	TestEqual(
		TEXT("A draw keeps the current round leader"),
		ResolveNextRoundLeaderSlot(
			EShowDownPlayerSlot::None,
			EShowDownPlayerSlot::Player3,
			AllAliveSlots),
		EShowDownPlayerSlot::Player3);
	TestEqual(
		TEXT("A surviving preferred loser leads the next round"),
		ResolveNextRoundLeaderSlot(
			EShowDownPlayerSlot::Player2,
			EShowDownPlayerSlot::Player3,
			AllAliveSlots),
		EShowDownPlayerSlot::Player2);
	TestEqual(
		TEXT("An eliminated preferred leader advances after its table seat"),
		ResolveNextRoundLeaderSlot(
			EShowDownPlayerSlot::Player4,
			EShowDownPlayerSlot::Player3,
			AliveAfterPlayer4),
		EShowDownPlayerSlot::Player1);
	TestEqual(
		TEXT("A disconnected leader advances in physical table order"),
		ResolveNextRoundLeaderSlot(
			EShowDownPlayerSlot::Player2,
			EShowDownPlayerSlot::None,
			AliveWithoutPlayer2),
		EShowDownPlayerSlot::Player4);

	TestEqual(
		TEXT("A solo player cannot restart a multiplayer match"),
		ResolveRestartDecision(1, 1),
		ESDMultiplayerRestartDecision::NotEnoughPlayers);
	TestEqual(
		TEXT("A partial restart vote waits for the remaining players"),
		ResolveRestartDecision(3, 2),
		ESDMultiplayerRestartDecision::WaitingForVotes);
	TestEqual(
		TEXT("Every eligible player voting restarts the match"),
		ResolveRestartDecision(2, 2),
		ESDMultiplayerRestartDecision::RestartMatch);
	TestFalse(
		TEXT("An open lobby accepts a new controller"),
		ShouldRejectNewPlayerJoin(false, false));
	TestTrue(
		TEXT("The hosted-game transition rejects a late join"),
		ShouldRejectNewPlayerJoin(false, true));
	TestTrue(
		TEXT("A running match rejects a late join"),
		ShouldRejectNewPlayerJoin(true, false));
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

	TArray<int32> FirstDealCards;
	TestTrue(TEXT("Ten cards can be dealt while keeping the reserve deck"), CardSystem->DealCards(10, FirstDealCards));
	TestEqual(TEXT("The first deal returns ten cards"), FirstDealCards.Num(), 10);
	TestEqual(TEXT("Four cards remain after the first deal"), CardSystem->GetRemainingCardCount(), 4);

	TArray<int32> SecondDealCards;
	TestTrue(TEXT("The four reserve cards can be dealt afterwards"), CardSystem->DealCards(4, SecondDealCards));
	TestEqual(TEXT("The second deal returns four cards"), SecondDealCards.Num(), 4);
	TestEqual(TEXT("The deck is empty after both deals"), CardSystem->GetRemainingCardCount(), 0);

	TArray<int32> RankCounts;
	RankCounts.Init(0, 8);
	TArray<int32> AllDealtCards = FirstDealCards;
	AllDealtCards.Append(SecondDealCards);
	TestEqual(TEXT("Both deals return all fourteen cards in total"), AllDealtCards.Num(), 14);
	for (const int32 Rank : AllDealtCards)
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

	TestFalse(TEXT("Dealing from an empty deck fails"), CardSystem->DealCards(1, SecondDealCards));
	TestEqual(TEXT("A failed deal clears the output"), SecondDealCards.Num(), 0);
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

	const TArray<FVector> OppositeTwoPlayerDirections = {
		-FVector::ForwardVector,
		FVector::ForwardVector,
	};
	const float OppositeTwoPlayerRadius = ShowDownCardRevealLayout::ResolveRadialCenterDistance(
		CenterDistance,
		OppositeTwoPlayerDirections);
	TestTrue(
		TEXT("Two opposite reveal cards split the configured neighboring-card gap"),
		FMath::IsNearlyEqual(OppositeTwoPlayerRadius, CenterDistance * 0.5f));
	TestTrue(
		TEXT("Two opposite reveal cards keep the configured center-to-center gap"),
		FMath::IsNearlyEqual(
			FVector::Distance(
				OppositeTwoPlayerDirections[0] * OppositeTwoPlayerRadius,
				OppositeTwoPlayerDirections[1] * OppositeTwoPlayerRadius),
			CenterDistance));

	const TArray<FVector> AdjacentTwoPlayerDirections = {
		-FVector::ForwardVector,
		FVector::RightVector,
	};
	const float AdjacentTwoPlayerRadius = ShowDownCardRevealLayout::ResolveRadialCenterDistance(
		CenterDistance,
		AdjacentTwoPlayerDirections);
	TestTrue(
		TEXT("Two adjacent reveal cards use the ninety-degree chord radius"),
		FMath::IsNearlyEqual(
			AdjacentTwoPlayerRadius,
			CenterDistance / FMath::Sqrt(2.0f)));
	TestTrue(
		TEXT("Two adjacent reveal cards keep the configured center-to-center gap"),
		FMath::IsNearlyEqual(
			FVector::Distance(
				AdjacentTwoPlayerDirections[0] * AdjacentTwoPlayerRadius,
				AdjacentTwoPlayerDirections[1] * AdjacentTwoPlayerRadius),
			CenterDistance,
			0.01f));

	const TArray<FVector> CardinalThreePlayerDirections = {
		-FVector::ForwardVector,
		FVector::ForwardVector,
		FVector::RightVector,
	};
	const float CardinalThreePlayerRadius = ShowDownCardRevealLayout::ResolveRadialCenterDistance(
		CenterDistance,
		CardinalThreePlayerDirections);
	TestTrue(
		TEXT("Three cardinal reveal cards use the nearest ninety-degree seat pair"),
		FMath::IsNearlyEqual(
			CardinalThreePlayerRadius,
			CenterDistance / FMath::Sqrt(2.0f)));
	TestTrue(
		TEXT("Three cardinal reveal cards keep the configured nearest-neighbor gap"),
		FMath::IsNearlyEqual(
			FVector::Distance(
				CardinalThreePlayerDirections[0] * CardinalThreePlayerRadius,
				CardinalThreePlayerDirections[2] * CardinalThreePlayerRadius),
			CenterDistance,
			0.01f));

	const TArray<FVector> CardinalFourPlayerDirections = {
		-FVector::ForwardVector,
		FVector::ForwardVector,
		FVector::RightVector,
		-FVector::RightVector,
	};
	const float CardinalFourPlayerRadius = ShowDownCardRevealLayout::ResolveRadialCenterDistance(
		CenterDistance,
		CardinalFourPlayerDirections);
	TestTrue(
		TEXT("Four cardinal reveal cards use the compact square radius"),
		FMath::IsNearlyEqual(
			CardinalFourPlayerRadius,
			CenterDistance / FMath::Sqrt(2.0f)));
	TestTrue(
		TEXT("Four cardinal reveal cards keep the configured nearest-neighbor gap"),
		FMath::IsNearlyEqual(
			FVector::Distance(
				CardinalFourPlayerDirections[0] * CardinalFourPlayerRadius,
				CardinalFourPlayerDirections[2] * CardinalFourPlayerRadius),
			CenterDistance,
			0.01f));

	TestEqual(
		TEXT("A single reveal card stays at table center"),
		ShowDownCardRevealLayout::ResolveRadialCenterDistance(
			CenterDistance,
			{FVector::ForwardVector}),
		0.0f);

	const TArray<FVector> DuplicateAndInvalidDirections = {
		-FVector::ForwardVector,
		-FVector::ForwardVector * 3.0f,
		FVector::ZeroVector,
		FVector::UpVector,
		FVector::ForwardVector,
	};
	TestTrue(
		TEXT("Duplicate and invalid directions do not change a valid opposite-seat radius"),
		FMath::IsNearlyEqual(
			ShowDownCardRevealLayout::ResolveRadialCenterDistance(
				CenterDistance,
				DuplicateAndInvalidDirections),
			OppositeTwoPlayerRadius));
	TestEqual(
		TEXT("Duplicate and invalid directions without a second seat stay centered"),
		ShowDownCardRevealLayout::ResolveRadialCenterDistance(
			CenterDistance,
			{-FVector::ForwardVector, -FVector::ForwardVector * 2.0f, FVector::ZeroVector}),
		0.0f);

	const TArray<FVector> ReorderedCardinalDirections = {
		-FVector::RightVector,
		FVector::RightVector,
		FVector::ForwardVector,
		-FVector::ForwardVector,
	};
	TestTrue(
		TEXT("Reveal radius is invariant to seat direction order"),
		FMath::IsNearlyEqual(
			ShowDownCardRevealLayout::ResolveRadialCenterDistance(
				CenterDistance,
				ReorderedCardinalDirections),
			CardinalFourPlayerRadius));

	FTransform OppositeLeftTransform;
	FTransform OppositeRightTransform;
	const bool bBuiltOppositeLeft = ShowDownCardRevealLayout::TryBuildRadialTransform(
		TableCenter,
		TableCenter + FVector(-100.0f, 0.0f, 0.0f),
		OppositeTwoPlayerRadius,
		0.0f,
		HeightOffset,
		RotationOffset,
		Scale,
		OppositeLeftTransform);
	const bool bBuiltOppositeRight = ShowDownCardRevealLayout::TryBuildRadialTransform(
		TableCenter,
		TableCenter + FVector(100.0f, 0.0f, 0.0f),
		OppositeTwoPlayerRadius,
		0.0f,
		HeightOffset,
		RotationOffset,
		Scale,
		OppositeRightTransform);
	TestTrue(TEXT("Two opposite reveal transforms resolve"), bBuiltOppositeLeft && bBuiltOppositeRight);
	if (bBuiltOppositeLeft && bBuiltOppositeRight)
	{
		TestEqual(
			TEXT("Two-player multiplayer gap matches single-player spacing"),
			FVector::Distance(
				OppositeLeftTransform.GetLocation(),
				OppositeRightTransform.GetLocation()),
			static_cast<double>(CenterDistance));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownCharacterSkinCatalogTest,
	"ShowDown.Core.CharacterSkinCatalog.ResolveAndAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownCharacterSkinCatalogTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Robot is the permanent default character skin"),
		UShowDownCharacterSkinCatalog::GetDefaultSkinId(),
		FString(TEXT("robot")));
	TestEqual(
		TEXT("Product-style robot aliases resolve to the runtime id"),
		UShowDownCharacterSkinCatalog::CanonicalizeSkinId(TEXT("  CHARACTER_ROBOT  ")),
		FString(TEXT("robot")));
	TestEqual(
		TEXT("Miku's product-style alias resolves independently from Micu"),
		UShowDownCharacterSkinCatalog::CanonicalizeSkinId(TEXT("character_miku")),
		FString(TEXT("miku")));
	TestTrue(
		TEXT("Skin ids with path separators are rejected before replication"),
		UShowDownCharacterSkinCatalog::CanonicalizeSkinId(TEXT("unsafe/skin")).IsEmpty());
	TestTrue(
		TEXT("Oversized skin ids are rejected before replication"),
		UShowDownCharacterSkinCatalog::CanonicalizeSkinId(
			FString::ChrN(65, TEXT('a'))).IsEmpty());
	TestEqual(
		TEXT("Unknown replicated ids resolve to the safe default"),
		UShowDownCharacterSkinCatalog::NormalizeKnownSkinId(nullptr, TEXT("not_in_this_build")),
		FString(TEXT("robot")));

	FShowDownCharacterSkinDefinition Definition;
	FString ResolvedSkinId;
	TestTrue(
		TEXT("An empty selection resolves to a built-in default"),
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			nullptr,
			FString(),
			Definition,
			ResolvedSkinId));
	TestEqual(TEXT("An empty selection resolves to robot"), ResolvedSkinId, FString(TEXT("robot")));
	TestFalse(TEXT("The default robot mesh reference is configured"), Definition.SkeletalMesh.IsNull());

	TestTrue(
		TEXT("Unknown backend ids still resolve safely"),
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			nullptr,
			TEXT("not_in_this_build"),
			Definition,
			ResolvedSkinId));
	TestEqual(TEXT("Unknown ids fall back to robot"), ResolvedSkinId, FString(TEXT("robot")));

	const TArray<FString> BuiltInSkinIds =
	{
		TEXT("robot"),
		TEXT("hoodman"),
		TEXT("micu"),
		TEXT("miku")
	};
	const TArray<EShowDownCharacterSkinRarity> BuiltInRarities =
	{
		EShowDownCharacterSkinRarity::Common,
		EShowDownCharacterSkinRarity::Rare,
		EShowDownCharacterSkinRarity::Epic,
		EShowDownCharacterSkinRarity::Legendary
	};

	TSet<FString> BuiltInShopPreviewAnimations;
	FString BuiltInMikuMainMenuAnimationPath;
	for (int32 SkinIndex = 0; SkinIndex < BuiltInSkinIds.Num(); ++SkinIndex)
	{
		const FString& SkinId = BuiltInSkinIds[SkinIndex];
		TestTrue(
			*FString::Printf(TEXT("Built-in skin '%s' is registered"), *SkinId),
			UShowDownCharacterSkinCatalog::FindBuiltInSkinDefinition(SkinId, Definition));
		TestFalse(
			*FString::Printf(TEXT("Built-in skin '%s' has a mesh reference"), *SkinId),
			Definition.SkeletalMesh.IsNull());
		TestTrue(
			*FString::Printf(TEXT("Built-in skin '%s' has a shop preview animation"), *SkinId),
			Definition.ShopPreview.AnimationMode
				== EShowDownShopPreviewAnimationMode::SingleAnimation
			&& !Definition.ShopPreview.Animation.IsNull());
		TestTrue(
			*FString::Printf(TEXT("Built-in skin '%s' has a main-menu preview animation"), *SkinId),
			Definition.MainMenuPreview.AnimationMode
				== EShowDownShopPreviewAnimationMode::SingleAnimation
			&& !Definition.MainMenuPreview.Animation.IsNull());
		USkeletalMesh* PreviewMesh = Definition.SkeletalMesh.LoadSynchronous();
		UAnimationAsset* ShopAnimation = Definition.ShopPreview.Animation.LoadSynchronous();
		TestNotNull(
			*FString::Printf(TEXT("Built-in skin '%s' preview mesh loads"), *SkinId),
			PreviewMesh);
		TestNotNull(
			*FString::Printf(TEXT("Built-in skin '%s' shop animation loads"), *SkinId),
			ShopAnimation);
		if (PreviewMesh && ShopAnimation)
		{
			const USkeleton* MeshSkeleton = PreviewMesh->GetSkeleton();
			const USkeleton* AnimationSkeleton = ShopAnimation->GetSkeleton();
			TestNotNull(
				*FString::Printf(TEXT("Built-in skin '%s' mesh has a skeleton"), *SkinId),
				MeshSkeleton);
			TestNotNull(
				*FString::Printf(TEXT("Built-in skin '%s' animation has a skeleton"), *SkinId),
				AnimationSkeleton);
			if (MeshSkeleton && AnimationSkeleton)
			{
				TestTrue(
					*FString::Printf(TEXT("Built-in skin '%s' animation is compatible with its mesh"), *SkinId),
					AnimationSkeleton->IsCompatibleMesh(PreviewMesh, false));
			}
		}
		TestEqual(
			*FString::Printf(TEXT("Built-in skin '%s' has its authored rarity"), *SkinId),
			Definition.Rarity,
			BuiltInRarities[SkinIndex]);
		if (!Definition.ShopPreview.Animation.IsNull())
		{
			BuiltInShopPreviewAnimations.Add(
				Definition.ShopPreview.Animation.ToSoftObjectPath().ToString());
		}
		if (SkinId == TEXT("miku"))
		{
			BuiltInMikuMainMenuAnimationPath =
				Definition.MainMenuPreview.Animation.ToSoftObjectPath().ToString();
		}
	}
	TestEqual(
		TEXT("Each built-in skin uses a distinct single-node preview animation"),
		BuiltInShopPreviewAnimations.Num(),
		4);

	UShowDownCharacterSkinCatalog* AnimationOverrideCatalog =
		NewObject<UShowDownCharacterSkinCatalog>();
	FShowDownCharacterSkinDefinition AnimationOverride;
	AnimationOverride.SkinId = TEXT("miku");
	AnimationOverride.ShopPreview.AnimationMode =
		EShowDownShopPreviewAnimationMode::SingleAnimation;
	AnimationOverride.ShopPreview.Animation = TSoftObjectPtr<UAnimationAsset>(
		FSoftObjectPath(TEXT("/Game/Character/Animation/selectCard.selectCard")));
	AnimationOverride.ShopPreview.bLoop = false;
	AnimationOverride.ShopPreview.PlayRate = 0.75f;
	AnimationOverrideCatalog->Skins.Add(AnimationOverride);

	TestTrue(
		TEXT("An editor catalog can override only a built-in skin's preview animation"),
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			AnimationOverrideCatalog,
			TEXT("miku"),
			Definition,
			ResolvedSkinId));
	TestFalse(TEXT("Animation-only overrides retain the built-in mesh"), Definition.SkeletalMesh.IsNull());
	TestEqual(
		TEXT("Animation-only overrides replace the built-in preview animation"),
		Definition.ShopPreview.Animation.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/Character/Animation/selectCard.selectCard")));
	TestFalse(TEXT("Animation-only overrides retain their loop setting"), Definition.ShopPreview.bLoop);
	TestEqual(
		TEXT("Animation-only overrides retain their play rate"),
		Definition.ShopPreview.PlayRate,
		0.75f);
	TestEqual(
		TEXT("A shop-only override preserves the independent built-in main-menu animation"),
		Definition.MainMenuPreview.Animation.ToSoftObjectPath().ToString(),
		BuiltInMikuMainMenuAnimationPath);
	TestEqual(
		TEXT("An unspecified rarity preserves the built-in rarity"),
		Definition.Rarity,
		EShowDownCharacterSkinRarity::Legendary);

	FShowDownCharacterSkinDefinition& ReferencePoseOverride =
		AnimationOverrideCatalog->Skins[0];
	ReferencePoseOverride.ShopPreview.AnimationMode =
		EShowDownShopPreviewAnimationMode::ReferencePose;
	ReferencePoseOverride.ShopPreview.Animation.Reset();
	ReferencePoseOverride.MainMenuPreview.AnimationMode =
		EShowDownShopPreviewAnimationMode::SingleAnimation;
	ReferencePoseOverride.MainMenuPreview.Animation = TSoftObjectPtr<UAnimationAsset>(
		FSoftObjectPath(TEXT("/Game/Character/Animation/selectCard.selectCard")));
	ReferencePoseOverride.MainMenuPreview.bLoop = false;
	ReferencePoseOverride.MainMenuPreview.PlayRate = 1.25f;
	ReferencePoseOverride.Rarity = EShowDownCharacterSkinRarity::Rare;
	TestTrue(
		TEXT("A catalog can independently override both preview contexts"),
		UShowDownCharacterSkinCatalog::ResolveSkinDefinition(
			AnimationOverrideCatalog,
			TEXT("miku"),
			Definition,
			ResolvedSkinId));
	TestEqual(
		TEXT("Shop reference-pose overrides are retained"),
		Definition.ShopPreview.AnimationMode,
		EShowDownShopPreviewAnimationMode::ReferencePose);
	TestTrue(
		TEXT("Shop reference-pose overrides clear the built-in animation asset"),
		Definition.ShopPreview.Animation.IsNull());
	TestEqual(
		TEXT("The main-menu profile is resolved independently from the shop profile"),
		Definition.MainMenuPreview.Animation.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/Character/Animation/selectCard.selectCard")));
	TestFalse(
		TEXT("The main-menu profile retains its independent loop setting"),
		Definition.MainMenuPreview.bLoop);
	TestEqual(
		TEXT("The main-menu profile retains its independent play rate"),
		Definition.MainMenuPreview.PlayRate,
		1.25f);
	TestEqual(
		TEXT("A catalog rarity override replaces the built-in rarity"),
		Definition.Rarity,
		EShowDownCharacterSkinRarity::Rare);
	TestTrue(
		TEXT("The shop context accessor returns the shop profile"),
		&UShowDownCharacterSkinCatalog::GetPreviewProfile(
			Definition,
			EShowDownCharacterPreviewContext::Shop)
			== &Definition.ShopPreview);
	TestTrue(
		TEXT("The main-menu context accessor returns the main-menu profile"),
		&UShowDownCharacterSkinCatalog::GetPreviewProfile(
			Definition,
			EShowDownCharacterPreviewContext::MainMenu)
			== &Definition.MainMenuPreview);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownCharacterSkinCatalogOrderTest,
	"ShowDown.Core.CharacterSkinCatalog.Order",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownCharacterSkinCatalogOrderTest::RunTest(const FString& Parameters)
{
	UShowDownCharacterSkinCatalog* Catalog = NewObject<UShowDownCharacterSkinCatalog>();
	TestNotNull(TEXT("A character skin catalog can be created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FShowDownCharacterSkinDefinition CustomGold;
	CustomGold.SkinId = TEXT("custom_gold");
	CustomGold.DisplayName = FText::FromString(TEXT("Custom Gold"));
	Catalog->Skins.Add(CustomGold);
	TestEqual(
		TEXT("A catalog-authored custom id survives replication validation"),
		UShowDownCharacterSkinCatalog::NormalizeKnownSkinId(Catalog, TEXT(" CUSTOM_GOLD ")),
		FString(TEXT("custom_gold")));

	FShowDownCharacterSkinDefinition MikuOverride;
	MikuOverride.SkinId = TEXT("character_miku");
	MikuOverride.Rarity = EShowDownCharacterSkinRarity::Rare;
	Catalog->Skins.Add(MikuOverride);

	FShowDownCharacterSkinDefinition CustomBlue;
	CustomBlue.SkinId = TEXT("custom_blue");
	CustomBlue.DisplayName = FText::FromString(TEXT("Custom Blue"));
	CustomBlue.Rarity = EShowDownCharacterSkinRarity::Epic;
	Catalog->Skins.Add(CustomBlue);
	Catalog->Skins.Add(CustomGold);
	Catalog->Skins.AddDefaulted();

	TArray<FShowDownCharacterSkinDefinition> OrderedDefinitions;
	UShowDownCharacterSkinCatalog::GetOrderedSkinDefinitions(Catalog, OrderedDefinitions);

	const TArray<FString> ExpectedOrder =
	{
		TEXT("custom_gold"),
		TEXT("miku"),
		TEXT("custom_blue"),
		TEXT("robot"),
		TEXT("hoodman"),
		TEXT("micu")
	};
	TestEqual(
		TEXT("Catalog order is retained, duplicates are removed, and missing built-ins are appended"),
		OrderedDefinitions.Num(),
		ExpectedOrder.Num());
	for (int32 Index = 0; Index < FMath::Min(OrderedDefinitions.Num(), ExpectedOrder.Num()); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Ordered skin %d has the expected stable id"), Index),
			OrderedDefinitions[Index].SkinId,
			ExpectedOrder[Index]);
	}

	if (OrderedDefinitions.Num() >= 3)
	{
		TestEqual(
			TEXT("A custom skin with unspecified rarity falls back to Common"),
			OrderedDefinitions[0].Rarity,
			EShowDownCharacterSkinRarity::Common);
		TestEqual(
			TEXT("A built-in catalog entry keeps its explicit rarity override"),
			OrderedDefinitions[1].Rarity,
			EShowDownCharacterSkinRarity::Rare);
		TestEqual(
			TEXT("A custom skin keeps its explicit rarity"),
			OrderedDefinitions[2].Rarity,
			EShowDownCharacterSkinRarity::Epic);
	}

	TArray<FShowDownCharacterSkinDefinition> BuiltInOnlyDefinitions;
	UShowDownCharacterSkinCatalog::GetOrderedSkinDefinitions(
		nullptr,
		BuiltInOnlyDefinitions);
	TestEqual(
		TEXT("A missing editor catalog still exposes all built-in skins"),
		BuiltInOnlyDefinitions.Num(),
		4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownShopPrimaryActionStateTest,
	"ShowDown.Core.Shop.PrimaryActionState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownShopPrimaryActionStateTest::RunTest(const FString& Parameters)
{
	struct FActionStateCase
	{
		const TCHAR* Label;
		bool bHasSnapshot;
		bool bLoadInFlight;
		bool bLoadFailed;
		bool bHasProduct;
		bool bOwned;
		bool bEquipped;
		bool bPurchaseInFlight;
		bool bEquipInFlight;
		EShowDownShopPrimaryActionState ExpectedState;
	};

	const FActionStateCase Cases[] =
	{
		{ TEXT("Initial data load shows Loading"), false, false, false, true, false, false, false, false, EShowDownShopPrimaryActionState::Loading },
		{ TEXT("A failed first load disables the action"), false, false, true, true, false, false, false, false, EShowDownShopPrimaryActionState::Unavailable },
		{ TEXT("An active refresh temporarily shows Loading"), true, true, false, true, true, true, false, false, EShowDownShopPrimaryActionState::Loading },
		{ TEXT("A catalog-only skin without a server product is unavailable"), true, false, false, false, false, false, false, false, EShowDownShopPrimaryActionState::Unavailable },
		{ TEXT("An unowned product can be purchased"), true, false, false, true, false, false, false, false, EShowDownShopPrimaryActionState::Purchase },
		{ TEXT("An owned product can be equipped"), true, false, false, true, true, false, false, false, EShowDownShopPrimaryActionState::Equip },
		{ TEXT("The equipped product is disabled as already equipped"), true, false, false, true, true, true, false, false, EShowDownShopPrimaryActionState::Equipped },
		{ TEXT("Purchase progress has priority over all background state"), true, true, false, true, true, true, true, true, EShowDownShopPrimaryActionState::Purchasing },
		{ TEXT("Equip progress has priority over cosmetic refresh"), true, true, false, true, true, false, false, true, EShowDownShopPrimaryActionState::Equipping }
	};

	for (const FActionStateCase& TestCase : Cases)
	{
		TestEqual(
			TestCase.Label,
			UShowDownShopWidget::ResolvePrimaryActionState(
				TestCase.bHasSnapshot,
				TestCase.bLoadInFlight,
				TestCase.bLoadFailed,
				TestCase.bHasProduct,
				TestCase.bOwned,
				TestCase.bEquipped,
				TestCase.bPurchaseInFlight,
				TestCase.bEquipInFlight),
			TestCase.ExpectedState);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownShopSelectionWrapTest,
	"ShowDown.Core.Shop.SelectionWrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownShopSelectionWrapTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("An empty catalog has no valid selection"),
		UShowDownShopWidget::WrapSelectionIndex(0, 1, 0),
		INDEX_NONE);
	TestEqual(
		TEXT("A negative item count has no valid selection"),
		UShowDownShopWidget::WrapSelectionIndex(0, -1, -1),
		INDEX_NONE);
	TestEqual(
		TEXT("Forward navigation initializes an invalid selection at the first item"),
		UShowDownShopWidget::WrapSelectionIndex(INDEX_NONE, 1, 4),
		0);
	TestEqual(
		TEXT("Backward navigation initializes an invalid selection at the last item"),
		UShowDownShopWidget::WrapSelectionIndex(INDEX_NONE, -1, 4),
		3);
	TestEqual(
		TEXT("Next wraps the final item to the first item"),
		UShowDownShopWidget::WrapSelectionIndex(3, 1, 4),
		0);
	TestEqual(
		TEXT("Previous wraps the first item to the final item"),
		UShowDownShopWidget::WrapSelectionIndex(0, -1, 4),
		3);
	TestEqual(
		TEXT("Large positive offsets wrap repeatedly"),
		UShowDownShopWidget::WrapSelectionIndex(1, 9, 4),
		2);
	TestEqual(
		TEXT("Large negative offsets wrap repeatedly"),
		UShowDownShopWidget::WrapSelectionIndex(2, -7, 4),
		3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownRoundCinematicDefaultsTest,
	"ShowDown.Core.RoundCinematicDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownRoundCinematicDefaultsTest::RunTest(const FString& Parameters)
{
	const AShowDownGameModeBase* GameMode = GetDefault<AShowDownGameModeBase>();
	TestNotNull(TEXT("ShowDown game mode defaults are available"), GameMode);
	if (!GameMode)
	{
		return false;
	}

	auto TestFloatDefault = [this, GameMode](const TCHAR* PropertyName, float ExpectedValue)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(
			AShowDownGameModeBase::StaticClass(),
			FName(PropertyName));
		TestNotNull(*FString::Printf(TEXT("%s is reflected"), PropertyName), Property);
		if (Property)
		{
			TestEqual(
				*FString::Printf(TEXT("%s keeps the authored cinematic default"), PropertyName),
				Property->GetPropertyValue_InContainer(GameMode),
				ExpectedValue);
		}
	};

	TestFloatDefault(TEXT("MultiplayerBetActionIntervalSeconds"), 0.6f);
	TestFloatDefault(TEXT("CardRevealHoldSeconds"), 0.6f);
	TestFloatDefault(TEXT("RoundCinematicBetFocusHoldSeconds"), 1.4f);
	TestFloatDefault(TEXT("RoundCinematicFinalBetToBlackoutSeconds"), 2.8f);
	TestFloatDefault(TEXT("RoundCinematicCollectorTurnLeadInSeconds"), 1.4f);
	TestFloatDefault(TEXT("RoundCinematicBlackoutToTableSpotlightSeconds"), 1.4f);
	TestFloatDefault(TEXT("RoundCinematicTableSpotlightToRevealSeconds"), 0.7f);
	TestFloatDefault(TEXT("RoundCinematicRevealToLoserSpotlightSeconds"), 2.1f);
	TestFloatDefault(TEXT("RoundCinematicLoserSpotlightHoldSeconds"), 1.5f);
	TestFloatDefault(TEXT("CollectorCardSelectionDelaySeconds"), 2.1f);
	TestFloatDefault(TEXT("RoundCinematicPostShotProgressHoldSeconds"), 3.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownGunShotCameraTest,
	"ShowDown.Core.GunShotCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownGunShotCameraTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Only a live hit targeting the local player enables the camera"),
		ASDSelfShotGunActor::ShouldUseGunShotCamera(true, true));
	TestFalse(
		TEXT("A live hit on another player leaves this camera unchanged"),
		ASDSelfShotGunActor::ShouldUseGunShotCamera(true, false));
	TestFalse(
		TEXT("An empty chamber never enables the target camera"),
		ASDSelfShotGunActor::ShouldUseGunShotCamera(false, true));
	TestTrue(
		TEXT("A final live hit keeps only the victim on the table overview"),
		ASDSelfShotGunActor::ShouldUseEliminationTableOverview(true, true, 0));
	TestFalse(
		TEXT("A surviving victim still returns to first person"),
		ASDSelfShotGunActor::ShouldUseEliminationTableOverview(true, true, 1));
	TestFalse(
		TEXT("An observer never enters the eliminated player's overview"),
		ASDSelfShotGunActor::ShouldUseEliminationTableOverview(true, false, 0));
	TestFalse(
		TEXT("An empty chamber never enters elimination overview"),
		ASDSelfShotGunActor::ShouldUseEliminationTableOverview(false, true, 0));

	const TArray<EShowDownPlayerSlot> PlayerSlots = {
		EShowDownPlayerSlot::Player1,
		EShowDownPlayerSlot::Player2,
		EShowDownPlayerSlot::Player3,
		EShowDownPlayerSlot::Player4
	};
	for (const EShowDownPlayerSlot TargetSlot : PlayerSlots)
	{
		for (const EShowDownPlayerSlot LocalSlot : PlayerSlots)
		{
			TestEqual(
				*FString::Printf(
					TEXT("Target slot %d changes only matching local slot %d"),
					static_cast<int32>(TargetSlot),
					static_cast<int32>(LocalSlot)),
				ASDSelfShotGunActor::IsGunShotTargetLocalPlayer(TargetSlot, LocalSlot),
				TargetSlot == LocalSlot);
		}
	}
	TestFalse(
		TEXT("An unassigned target never owns a local gun-shot camera"),
		ASDSelfShotGunActor::IsGunShotTargetLocalPlayer(
			EShowDownPlayerSlot::None,
			EShowDownPlayerSlot::Player1));

	const FTransform PlayerOneTransform(
		FRotator(0.0f, 35.0f, 0.0f),
		FVector(120.0f, -80.0f, 10.0f));
	const FVector CameraRelativeLocation(145.0f, -55.0f, 92.0f);
	const FQuat CameraRelativeRotation = FRotator(-8.0f, 165.0f, 2.0f).Quaternion();
	const FTransform PlayerOneCameraTransform(
		PlayerOneTransform.GetRotation() * CameraRelativeRotation,
		PlayerOneTransform.TransformPosition(CameraRelativeLocation),
		FVector(1.0f));

	const FTransform PlayerOneResult = ASDSelfShotGunActor::BuildSeatRelativeGunShotCameraTransform(
		PlayerOneCameraTransform,
		PlayerOneTransform,
		PlayerOneTransform);
	TestTrue(
		TEXT("Player 1 keeps the exact authored camera location"),
		PlayerOneResult.GetLocation().Equals(PlayerOneCameraTransform.GetLocation(), 0.01f));
	TestTrue(
		TEXT("Player 1 keeps the exact authored camera rotation"),
		PlayerOneResult.GetRotation().AngularDistance(PlayerOneCameraTransform.GetRotation()) < 0.001f);

	const FTransform PlayerThreeTransform(
		FRotator(0.0f, -105.0f, 0.0f),
		FVector(-230.0f, 310.0f, 10.0f));
	const FTransform PlayerThreeResult = ASDSelfShotGunActor::BuildSeatRelativeGunShotCameraTransform(
		PlayerOneCameraTransform,
		PlayerOneTransform,
		PlayerThreeTransform);
	const FVector ExpectedPlayerThreeLocation = PlayerThreeTransform.TransformPosition(CameraRelativeLocation);
	const FQuat ExpectedPlayerThreeRotation = PlayerThreeTransform.GetRotation() * CameraRelativeRotation;
	TestTrue(
		TEXT("Player 3 receives the same character-relative camera offset"),
		PlayerThreeResult.GetLocation().Equals(ExpectedPlayerThreeLocation, 0.01f));
	TestTrue(
		TEXT("Player 3 receives the same character-relative camera rotation"),
		PlayerThreeResult.GetRotation().AngularDistance(ExpectedPlayerThreeRotation) < 0.001f);

	const FVector TableCenter(40.0f, -25.0f, 10.0f);
	const float SeatDistance = 300.0f;
	const float OverviewBackDistance = 90.0f;
	const float OverviewHeight = 135.0f;
	const float OverviewLookAtHeight = 28.0f;
	const FVector SeatOffsets[] = {
		FVector(-SeatDistance, 0.0f, 0.0f),
		FVector(SeatDistance, 0.0f, 0.0f),
		FVector(0.0f, SeatDistance, 0.0f),
		FVector(0.0f, -SeatDistance, 0.0f)
	};
	for (int32 SeatIndex = 0; SeatIndex < UE_ARRAY_COUNT(SeatOffsets); ++SeatIndex)
	{
		const FVector SeatLocation = TableCenter + SeatOffsets[SeatIndex];
		const FVector DirectionToTable = (TableCenter - SeatLocation).GetSafeNormal2D();
		const FTransform Overview = ASDSelfShotGunActor::BuildEliminationTableOverviewTransform(
			TableCenter,
			FTransform(FRotator::ZeroRotator, SeatLocation),
			OverviewBackDistance,
			OverviewHeight,
			OverviewLookAtHeight);
		const FVector ExpectedLocation = SeatLocation
			- DirectionToTable * OverviewBackDistance
			+ FVector::UpVector * OverviewHeight;
		TestTrue(
			*FString::Printf(TEXT("Seat %d overview stays behind and above its eliminated player"), SeatIndex + 1),
			Overview.GetLocation().Equals(ExpectedLocation, 0.01f));

		const FVector ExpectedLookDirection = (
			TableCenter + FVector::UpVector * OverviewLookAtHeight - ExpectedLocation).GetSafeNormal();
		TestTrue(
			*FString::Printf(TEXT("Seat %d overview looks back toward the shared table"), SeatIndex + 1),
			FVector::DotProduct(Overview.GetUnitAxis(EAxis::X), ExpectedLookDirection) > 0.999f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownHitRecoveryTimingTest,
	"ShowDown.Core.HitRecoveryTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownHitRecoveryTimingTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Default recovery keeps the body down, masks reset, and settles before play resumes"),
		AShowDownCharacter::CalculateHitRecoveryPresentationDuration(1.0f, 0.5f, 0.55f),
		2.05f);
	TestEqual(
		TEXT("Invalid negative timing cannot remove the minimum conceal pulse"),
		AShowDownCharacter::CalculateHitRecoveryPresentationDuration(-1.0f, -1.0f, -1.0f),
		0.1f);
	TestEqual(
		TEXT("Queued multiplayer shots wait for recovery measured from the impact"),
		AShowDownGameModeBase::CalculateRoulettePresentationFinishDelay(1.45f, 3.0f, 2.05f),
		3.5f);
	TestEqual(
		TEXT("A longer gun camera still owns the presentation finish"),
		AShowDownGameModeBase::CalculateRoulettePresentationFinishDelay(1.0f, 4.0f, 2.0f),
		4.0f);
	TestTrue(
		TEXT("A queued multiplayer shot reserves five seconds after its real result"),
		FMath::IsNearlyEqual(
			AShowDownGameModeBase::CalculateRouletteProgressionFinishDelay(1.45f, 3.5f, 5.0f),
			6.45f));
	TestEqual(
		TEXT("A longer presentation can still own multiplayer progression"),
		AShowDownGameModeBase::CalculateRouletteProgressionFinishDelay(1.0f, 8.0f, 5.0f),
		8.0f);
	TestEqual(
		TEXT("Negative multiplayer progression timing is clamped"),
		AShowDownGameModeBase::CalculateRouletteProgressionFinishDelay(-1.0f, -2.0f, -3.0f),
		0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownVisionDirectorBlendTest,
	"ShowDown.Core.VisionDirectorBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownVisionDirectorBlendTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Linear easing clamps values below zero"),
		ASDVisionDirector::EvaluateVisionBlendEase(-1.0f, ESDVisionBlendEase::Linear, 2.0f),
		0.0f);
	TestEqual(
		TEXT("Linear easing clamps values above one"),
		ASDVisionDirector::EvaluateVisionBlendEase(2.0f, ESDVisionBlendEase::Linear, 2.0f),
		1.0f);
	TestTrue(
		TEXT("Ease-in uses the configured exponent"),
		FMath::IsNearlyEqual(
			ASDVisionDirector::EvaluateVisionBlendEase(0.5f, ESDVisionBlendEase::EaseIn, 2.0f),
			0.25f));
	TestTrue(
		TEXT("Ease-out uses the configured exponent"),
		FMath::IsNearlyEqual(
			ASDVisionDirector::EvaluateVisionBlendEase(0.5f, ESDVisionBlendEase::EaseOut, 2.0f),
			0.75f));
	TestTrue(
		TEXT("Ease-in-out is symmetric around its midpoint"),
		FMath::IsNearlyEqual(
			ASDVisionDirector::EvaluateVisionBlendEase(0.25f, ESDVisionBlendEase::EaseInOut, 2.0f),
			0.125f));

	ASDVisionDirector* VisionDirector = NewObject<ASDVisionDirector>();
	TestNotNull(TEXT("Vision director can be created without a game world"), VisionDirector);
	if (!VisionDirector)
	{
		return false;
	}
	TestEqual(TEXT("Gameplay vision defaults to the requested table radius"), VisionDirector->GetTableVisionRadius(), 150.0f);
	TestEqual(TEXT("Gameplay vision defaults to the requested feather"), VisionDirector->GetTableVisionFeather(), 100.0f);
	TestEqual(TEXT("The optional wide-view radius keeps its authored default"), VisionDirector->GetIntroWideVisionRadius(), 5000.0f);
	TestEqual(TEXT("Match entry keeps a consistent feather"), VisionDirector->GetIntroWideVisionFeather(), 100.0f);
	TestEqual(TEXT("Gameplay darkness rests at the cinematic baseline"), VisionDirector->GetDarknessStrength(), 1.0f);

	VisionDirector->SetVisionAlpha(-1.0f);
	TestEqual(TEXT("Immediate vision alpha clamps below zero"), VisionDirector->GetVisionAlpha(), 0.0f);
	VisionDirector->BlendToVisionAlpha(1.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestTrue(TEXT("A positive-duration alpha transition starts blending"), VisionDirector->IsVisionBlending());
	TestEqual(TEXT("The alpha transition exposes its clamped target"), VisionDirector->GetVisionBlendTargetAlpha(), 1.0f);
	VisionDirector->Tick(0.25f);
	TestTrue(
		TEXT("A linear alpha transition advances by elapsed duration"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionAlpha(), 0.25f));

	const float AlphaBeforeRetarget = VisionDirector->GetVisionAlpha();
	VisionDirector->BlendToVisionAlpha(0.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestTrue(
		TEXT("Retargeting alpha preserves the currently displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionAlpha(), AlphaBeforeRetarget));
	VisionDirector->Tick(0.5f);
	TestTrue(
		TEXT("A retargeted alpha transition starts from the displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionAlpha(), 0.125f));

	VisionDirector->CancelVisionBlend();
	const float CancelledAlpha = VisionDirector->GetVisionAlpha();
	TestFalse(TEXT("Cancelling alpha leaves no active transition"), VisionDirector->IsVisionBlending());
	VisionDirector->Tick(1.0f);
	TestTrue(
		TEXT("Cancelled alpha remains at the displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionAlpha(), CancelledAlpha));

	VisionDirector->BlendToWideVision(1.0f, ESDVisionBlendEase::Linear, 2.0f);
	VisionDirector->CompleteVisionBlend();
	TestFalse(TEXT("Completing alpha clears the active transition"), VisionDirector->IsVisionBlending());
	TestEqual(TEXT("Completing alpha applies its destination"), VisionDirector->GetVisionAlpha(), 1.0f);
	VisionDirector->BlendToFocusedVision(0.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestFalse(TEXT("A zero-duration alpha transition completes immediately"), VisionDirector->IsVisionBlending());
	TestEqual(TEXT("A zero-duration alpha transition applies its destination"), VisionDirector->GetVisionAlpha(), 0.0f);
	VisionDirector->BlendToVisionAlpha(0.5f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	VisionDirector->SetVisionAlpha(2.0f);
	TestFalse(TEXT("Immediate alpha application cancels an active transition"), VisionDirector->IsVisionBlending());
	TestEqual(TEXT("Immediate vision alpha clamps above one"), VisionDirector->GetVisionAlpha(), 1.0f);

	VisionDirector->SetDarknessStrength(-1.0f);
	TestEqual(TEXT("Immediate darkness clamps below zero"), VisionDirector->GetDarknessStrength(), 0.0f);
	VisionDirector->BlendToDarknessStrength(1.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestTrue(TEXT("A positive-duration darkness transition starts blending"), VisionDirector->IsDarknessStrengthBlending());
	TestEqual(TEXT("The darkness transition exposes its clamped target"), VisionDirector->GetDarknessStrengthBlendTarget(), 1.0f);
	VisionDirector->Tick(0.25f);
	TestTrue(
		TEXT("A linear darkness transition advances by elapsed duration"),
		FMath::IsNearlyEqual(VisionDirector->GetDarknessStrength(), 0.25f));

	const float DarknessBeforeRetarget = VisionDirector->GetDarknessStrength();
	VisionDirector->BlendToDarknessStrength(0.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestTrue(
		TEXT("Retargeting darkness preserves the currently displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetDarknessStrength(), DarknessBeforeRetarget));
	VisionDirector->Tick(0.5f);
	TestTrue(
		TEXT("A retargeted darkness transition starts from the displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetDarknessStrength(), 0.125f));

	VisionDirector->CancelDarknessStrengthBlend();
	const float CancelledDarkness = VisionDirector->GetDarknessStrength();
	TestFalse(TEXT("Cancelling darkness leaves no active transition"), VisionDirector->IsDarknessStrengthBlending());
	VisionDirector->Tick(1.0f);
	TestTrue(
		TEXT("Cancelled darkness remains at the displayed value"),
		FMath::IsNearlyEqual(VisionDirector->GetDarknessStrength(), CancelledDarkness));

	VisionDirector->BlendToDarknessStrength(2.0f, 0.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestFalse(TEXT("A zero-duration darkness transition completes immediately"), VisionDirector->IsDarknessStrengthBlending());
	TestEqual(TEXT("A zero-duration darkness transition clamps its destination"), VisionDirector->GetDarknessStrength(), 1.0f);

	VisionDirector->SetDarknessStrength(0.6f);
	VisionDirector->BlendToDarknessStrength(0.8f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	VisionDirector->Tick(0.25f);
	const float DarknessBeforeAlphaBlend = VisionDirector->GetDarknessStrength();
	VisionDirector->BlendToVisionAlpha(0.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestFalse(TEXT("Starting alpha cancels the direct darkness transition"), VisionDirector->IsDarknessStrengthBlending());
	TestTrue(TEXT("Starting alpha preserves current darkness"), FMath::IsNearlyEqual(
		VisionDirector->GetDarknessStrength(), DarknessBeforeAlphaBlend));
	VisionDirector->BlendToDarknessStrength(0.2f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestFalse(TEXT("Starting direct darkness cancels the alpha transition"), VisionDirector->IsVisionBlending());
	TestTrue(TEXT("Starting direct darkness preserves current darkness"), FMath::IsNearlyEqual(
		VisionDirector->GetDarknessStrength(), DarknessBeforeAlphaBlend));

	VisionDirector->CompleteDarknessStrengthBlend();
	TestFalse(TEXT("Completing darkness clears the active transition"), VisionDirector->IsDarknessStrengthBlending());
	TestEqual(TEXT("Completing darkness applies its destination"), VisionDirector->GetDarknessStrength(), 0.2f);

	VisionDirector->SetVisionRange(-100.0f, -10.0f);
	TestEqual(TEXT("Immediate range clamps radius to zero"), VisionDirector->GetVisionRadius(), 0.0f);
	TestEqual(TEXT("Immediate range keeps a nonzero feather"), VisionDirector->GetVisionFeather(), 1.0f);
	VisionDirector->SetDarknessStrength(0.5f);
	VisionDirector->BlendToVisionRange(
		1000.0f,
		201.0f,
		2.0f,
		ESDVisionBlendEase::Linear,
		2.0f);
	VisionDirector->BlendToDarknessStrength(
		1.0f,
		1.0f,
		ESDVisionBlendEase::Linear,
		2.0f);
	TestTrue(TEXT("Range and darkness can blend at the same time"),
		VisionDirector->IsVisionRangeBlending() && VisionDirector->IsDarknessStrengthBlending());
	VisionDirector->Tick(0.5f);
	TestTrue(TEXT("Independent range advances on its own duration"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionRadius(), 250.0f));
	TestTrue(TEXT("Independent feather advances with range"),
		FMath::IsNearlyEqual(VisionDirector->GetVisionFeather(), 51.0f));
	TestTrue(TEXT("Darkness keeps advancing while range moves"),
		FMath::IsNearlyEqual(VisionDirector->GetDarknessStrength(), 0.75f));
	VisionDirector->CompleteVisionRangeBlend();
	TestFalse(TEXT("Completing range clears only its transition"), VisionDirector->IsVisionRangeBlending());
	TestTrue(TEXT("Completing range leaves darkness active"), VisionDirector->IsDarknessStrengthBlending());
	TestEqual(TEXT("Completing range applies its radius destination"), VisionDirector->GetVisionRadius(), 1000.0f);
	TestEqual(TEXT("Completing range applies its feather destination"), VisionDirector->GetVisionFeather(), 201.0f);

	VisionDirector->BlendToVisionAlpha(1.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	VisionDirector->BlendToVisionRange(400.0f, 80.0f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestFalse(TEXT("Starting direct range cancels the alpha-owned geometry transition"),
		VisionDirector->IsVisionBlending());
	VisionDirector->BlendToDarknessStrength(0.4f, 1.0f, ESDVisionBlendEase::Linear, 2.0f);
	TestTrue(TEXT("Starting direct darkness preserves the independent range transition"),
		VisionDirector->IsVisionRangeBlending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownPlayerLifecycleTest,
	"ShowDown.Core.PlayerLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownPlayerLifecycleTest::RunTest(const FString& Parameters)
{
	ASDPlayerState* PreviousState = NewObject<ASDPlayerState>();
	ASDPlayerState* CopiedState = NewObject<ASDPlayerState>();
	ASDPlayerState* RestoredState = NewObject<ASDPlayerState>();
	TestNotNull(TEXT("Previous player state can be created"), PreviousState);
	TestNotNull(TEXT("Copied player state can be created"), CopiedState);
	TestNotNull(TEXT("Restored player state can be created"), RestoredState);
	if (!PreviousState || !CopiedState || !RestoredState)
	{
		return false;
	}

	PreviousState->ShowDownSlot = EShowDownPlayerSlot::Player3;
	PreviousState->bReady = true;
	PreviousState->bHostPlayer = true;
	PreviousState->CopyProperties(CopiedState);
	TestEqual(
		TEXT("Seamless travel copies the authoritative seat"),
		CopiedState->ShowDownSlot,
		EShowDownPlayerSlot::Player3);
	TestTrue(TEXT("Seamless travel copies ready state"), CopiedState->bReady);
	TestTrue(TEXT("Seamless travel copies host state"), CopiedState->bHostPlayer);

	RestoredState->OverrideWith(CopiedState);
	TestEqual(
		TEXT("Reconnect restore keeps the authoritative seat"),
		RestoredState->ShowDownSlot,
		EShowDownPlayerSlot::Player3);
	TestTrue(TEXT("Reconnect restore keeps ready state"), RestoredState->bReady);
	TestTrue(TEXT("Reconnect restore keeps host state"), RestoredState->bHostPlayer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownGunVisionSequenceTimingTest,
	"ShowDown.Core.GunVisionSequenceTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownGunVisionSequenceTimingTest::RunTest(const FString& Parameters)
{
	USDGunVisionSequenceSubsystem* SequenceSubsystem = NewObject<USDGunVisionSequenceSubsystem>();
	AShowDownGameStateBase* GameState = NewObject<AShowDownGameStateBase>();
	ASDSelfShotGunActor* GunActor = NewObject<ASDSelfShotGunActor>();
	TestNotNull(TEXT("Gun vision sequence subsystem can be created"), SequenceSubsystem);
	TestNotNull(TEXT("Gun vision sequence test game state can be created"), GameState);
	TestNotNull(TEXT("Gun vision sequence test gun can be created"), GunActor);
	if (!SequenceSubsystem || !GameState || !GunActor)
	{
		return false;
	}

	SequenceSubsystem->bMatchPresentationActivated = false;
	SequenceSubsystem->bTableSpotlightEnabled = true;
	SequenceSubsystem->bZeroDarknessSpotlightEnabled = false;
	SequenceSubsystem->ApplyPhasePresentationPolicy(EShowDownPhase::None);
	TestFalse(
		TEXT("The in-game startup policy keeps SpotLight6 off"),
		SequenceSubsystem->bTableSpotlightEnabled);
	TestTrue(
		TEXT("The in-game startup policy begins with SpotLight7 on"),
		SequenceSubsystem->bZeroDarknessSpotlightEnabled);

	SequenceSubsystem->bTableSpotlightEnabled = true;
	SequenceSubsystem->bZeroDarknessSpotlightEnabled = false;
	SequenceSubsystem->ApplyPhasePresentationPolicy(EShowDownPhase::SelectCard);
	TestFalse(
		TEXT("Card selection explicitly keeps SpotLight6 off"),
		SequenceSubsystem->bTableSpotlightEnabled);

	SequenceSubsystem->bTableSpotlightEnabled = false;
	SequenceSubsystem->bZeroDarknessSpotlightEnabled = true;
	SequenceSubsystem->HandleTableCinematicCue(ESDTableCinematicCue::PreRevealBlackout, 0);
	TestTrue(
		TEXT("Pre-reveal blackout enables SpotLight6 immediately"),
		SequenceSubsystem->bTableSpotlightEnabled);
	TestFalse(
		TEXT("Pre-reveal blackout disables SpotLight7"),
		SequenceSubsystem->bZeroDarknessSpotlightEnabled);

	GameState->CurrentPhase = EShowDownPhase::Roulette;
	SequenceSubsystem->BoundGameState = GameState;
	SequenceSubsystem->DesiredDarknessStrength = 1.0f;
	SequenceSubsystem->bZeroDarknessSpotlightEnabled = false;
	const uint8 TargetMask = ShowDownTableCinematics::PlayerSlotToMask(
		EShowDownPlayerSlot::Player1);
	SequenceSubsystem->ActiveTargetSpotlightMask = TargetMask;

	SequenceSubsystem->HandleTableCinematicCue(
		ESDTableCinematicCue::TriggerPullStarted,
		TargetMask);
	TestEqual(
		TEXT("Starting trigger travel does not change darkness"),
		SequenceSubsystem->DesiredDarknessStrength,
		1.0f);
	TestFalse(
		TEXT("Starting trigger travel does not enable the bright spotlight"),
		SequenceSubsystem->bZeroDarknessSpotlightEnabled);
	TestEqual(
		TEXT("Starting trigger travel keeps the red loser light active"),
		SequenceSubsystem->ActiveTargetSpotlightMask,
		TargetMask);

	SequenceSubsystem->HandleTableCinematicCue(
		ESDTableCinematicCue::TriggerPullCompleted,
		0);
	TestEqual(
		TEXT("Full trigger travel clears the red loser light"),
		SequenceSubsystem->ActiveTargetSpotlightMask,
		static_cast<uint8>(0));
	TestEqual(
		TEXT("The full-pull cue itself waits for the firing callback to change darkness"),
		SequenceSubsystem->DesiredDarknessStrength,
		1.0f);

	SequenceSubsystem->HandleGunFired(GunActor);
	TestEqual(
		TEXT("The real live-fire event changes darkness to zero"),
		SequenceSubsystem->DesiredDarknessStrength,
		0.0f);
	TestTrue(
		TEXT("The real live-fire event starts the post-shot hold"),
		SequenceSubsystem->bPostShotBrightHoldActive);
	TestTrue(
		TEXT("The real live-fire event enables the bright spotlight"),
		SequenceSubsystem->bZeroDarknessSpotlightEnabled);

	SequenceSubsystem->HandleGunPresentationFinished(GunActor);
	TestEqual(
		TEXT("Darkness stays at zero after the local presentation finishes"),
		SequenceSubsystem->DesiredDarknessStrength,
		0.0f);
	TestTrue(
		TEXT("The post-shot hold waits for an authoritative progression event"),
		SequenceSubsystem->bPostShotBrightHoldActive);

	SequenceSubsystem->HandleTableCinematicCue(ESDTableCinematicCue::Reset, 0);
	TestFalse(
		TEXT("The replicated reset releases the post-shot hold"),
		SequenceSubsystem->bPostShotBrightHoldActive);
	TestEqual(
		TEXT("The replicated reset restores the next-game darkness target"),
		SequenceSubsystem->DesiredDarknessStrength,
		1.0f);

	GameState->CurrentPhase = EShowDownPhase::Betting;
	GunActor->bMultiplayerRoulettePresentationActive = true;
	SequenceSubsystem->DesiredDarknessStrength = 1.0f;
	SequenceSubsystem->HandleGunRaised(GunActor);
	SequenceSubsystem->HandleGunFired(GunActor);
	TestEqual(
		TEXT("A multiplayer presentation does not wait for phase replication to brighten"),
		SequenceSubsystem->DesiredDarknessStrength,
		0.0f);

	GunActor->PendingMultiplayerRoulettePresentations.Add(
		{ EShowDownPlayerSlot::Player3, false });
	GunActor->HandleTableCinematicCue(ESDTableCinematicCue::Reset, 0);
	TestFalse(
		TEXT("A reliable reset invalidates the active multiplayer gun presentation"),
		GunActor->bMultiplayerRoulettePresentationActive);
	TestTrue(
		TEXT("A reliable reset clears delayed client gun presentations immediately"),
		GunActor->PendingMultiplayerRoulettePresentations.IsEmpty());

	GunActor->bMultiplayerRoulettePresentationActive = true;
	GunActor->PendingMultiplayerRoulettePresentations.Add(
		{ EShowDownPlayerSlot::Player3, false });
	GunActor->HandleGamePhaseChanged(EShowDownPhase::RoundEnd);
	TestTrue(
		TEXT("Leaving roulette clears delayed client gun presentations"),
		GunActor->PendingMultiplayerRoulettePresentations.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShowDownAudioConfigTest,
	"ShowDown.Core.AudioConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShowDownAudioConfigTest::RunTest(const FString& Parameters)
{
	const UShowDownAudioConfig* AudioConfig = LoadObject<UShowDownAudioConfig>(
		nullptr,
		TEXT("/Game/Audio/DA_ShowDownAudioConfig.DA_ShowDownAudioConfig"));
	TestNotNull(TEXT("The central ShowDown audio config asset loads"), AudioConfig);
	if (!AudioConfig)
	{
		return false;
	}

	auto TestSoundPath = [this](const TCHAR* Label, const USoundBase* Sound, const TCHAR* ExpectedPath)
	{
		TestNotNull(Label, Sound);
		if (Sound)
		{
			TestEqual(Label, Sound->GetPathName(), FString(ExpectedPath));
		}
	};

	TestSoundPath(
		TEXT("Crowd bed uses the imported ambience"),
		AudioConfig->CrowdBedSound,
		TEXT("/Game/Audio/SW_Crowd_Bed.SW_Crowd_Bed"));
	TestSoundPath(
		TEXT("Crowd shock uses the imported reaction"),
		AudioConfig->CrowdShockedSound,
		TEXT("/Game/Audio/SW_Crowd_Shocked.SW_Crowd_Shocked"));
	TestSoundPath(
		TEXT("Live shots use the imported metal hit layer"),
		AudioConfig->GunHitLayerSound,
		TEXT("/Game/Audio/SW_Gun_HitMetal.SW_Gun_HitMetal"));
	TestSoundPath(
		TEXT("UI buttons use the imported click"),
		AudioConfig->ButtonClickSound,
		TEXT("/Game/Audio/SW_UI_ButtonClick.SW_UI_ButtonClick"));
	TestSoundPath(
		TEXT("BGM uses the imported Boogie Down track"),
		AudioConfig->BackgroundMusicSound,
		TEXT("/Game/Audio/SW_BGM_BoogieDown.SW_BGM_BoogieDown"));
	TestSoundPath(
		TEXT("Spotlight transitions use the imported spotlight cue"),
		AudioConfig->SpotlightTransitionSound,
		TEXT("/Game/Audio/SW_Spotlight.SW_Spotlight"));
	TestSoundPath(
		TEXT("The red loser spotlight uses the imported warning cue"),
		AudioConfig->LoserSpotlightWarningSound,
		TEXT("/Game/Audio/SW_LoserWarning.SW_LoserWarning"));

	const USoundWave* CrowdBedWave = Cast<USoundWave>(AudioConfig->CrowdBedSound);
	const USoundWave* BackgroundMusicWave = Cast<USoundWave>(AudioConfig->BackgroundMusicSound);
	const USoundWave* LoserWarningWave = Cast<USoundWave>(AudioConfig->LoserSpotlightWarningSound);
	TestTrue(TEXT("Crowd bed is configured to loop"), CrowdBedWave && CrowdBedWave->IsLooping());
	TestTrue(TEXT("Background music is configured to loop"), BackgroundMusicWave && BackgroundMusicWave->IsLooping());
	TestTrue(
		TEXT("The loser spotlight warning is an imported non-looping SoundWave"),
		LoserWarningWave && !LoserWarningWave->IsLooping());
	TestTrue(
		TEXT("The loser spotlight warning contains playable audio"),
		LoserWarningWave && LoserWarningWave->GetDuration() > 0.0f);
	TestTrue(
		TEXT("Background music has an audible configured volume"),
		AudioConfig->BackgroundMusicVolume > 0.0f);

	TestEqual(
		TEXT("Full user music volume resolves to the authored BGM target"),
		UShowDownAudioSubsystem::CalculateMusicTargetVolume(
			AudioConfig->BackgroundMusicVolume,
			1.0f,
			1.0f),
		AudioConfig->BackgroundMusicVolume);
	TestEqual(
		TEXT("Muted user music resolves to a zero restart target"),
		UShowDownAudioSubsystem::CalculateMusicTargetVolume(
			AudioConfig->BackgroundMusicVolume,
			0.0f,
			1.0f),
		0.0f);
	TestTrue(
		TEXT("The idle crowd bed has an audible configured volume"),
		AudioConfig->CrowdIdleVolume > 0.0f);
	TestTrue(
		TEXT("Crowd idle stays quieter than the empty-chamber boost"),
		AudioConfig->CrowdIdleVolume < AudioConfig->CrowdEmptyBoostVolume);
	TestTrue(
		TEXT("Live-round crowd shock is delayed after the gunshot"),
		AudioConfig->CrowdShockDelay > 0.0f);
	TestTrue(
		TEXT("Spotlight transitions have an audible configured volume"),
		AudioConfig->SpotlightTransitionVolume > 0.0f);
	TestTrue(
		TEXT("The loser spotlight warning has an audible configured volume"),
		AudioConfig->LoserSpotlightWarningVolume > 0.0f);
	return true;
}

#endif
