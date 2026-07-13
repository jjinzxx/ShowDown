#if WITH_DEV_AUTOMATION_TESTS

#include "BettingSystem.h"
#include "CardSystem.h"
#include "CollectorAISystem.h"
#include "Misc/AutomationTest.h"
#include "Presentation/SDCardRevealLayout.h"
#include "Presentation/SDSelfShotGunActor.h"
#include "RoundResolver.h"
#include "RouletteSystem.h"
#include "ShowDownCharacterSkinCatalog.h"

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
	"ShowDown.Core.CharacterSkinCatalog",
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

	for (const FString SkinId : { FString(TEXT("robot")), FString(TEXT("hoodman")), FString(TEXT("micu")) })
	{
		TestTrue(
			*FString::Printf(TEXT("Built-in skin '%s' is registered"), *SkinId),
			UShowDownCharacterSkinCatalog::FindBuiltInSkinDefinition(SkinId, Definition));
		TestFalse(
			*FString::Printf(TEXT("Built-in skin '%s' has a mesh reference"), *SkinId),
			Definition.SkeletalMesh.IsNull());
	}

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

	return true;
}

#endif
