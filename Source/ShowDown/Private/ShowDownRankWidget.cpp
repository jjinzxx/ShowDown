#include "ShowDownRankWidget.h"

#include "SupabaseSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UShowDownRankWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 점수가 서버에서 갱신되면(로그인 직후, 보상 지급 후 등) 자동으로 새로고침합니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnPlayerDataLoaded.AddUniqueDynamic(this, &UShowDownRankWidget::HandlePlayerDataLoaded);
			SupabaseSubsystem->OnLeaderboardLoaded.AddUniqueDynamic(this, &UShowDownRankWidget::HandleLeaderboardLoaded);

			// 화면이 뜰 때 전체 리더보드를 서버에서 불러옵니다.
			bLeaderboardRequestInFlight = true;
			SetStatusMessage(TEXT("Loading rankings..."), FLinearColor::Yellow);
			SupabaseSubsystem->LoadLeaderboard();
		}
	}

	// "뒤로" 버튼이 있으면 클릭 시 메인메뉴 복귀 요청을 보냅니다.
	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &UShowDownRankWidget::HandleBackClicked);
	}

	// 위젯이 뜰 때 현재 캐시된 내 점수를 즉시 표시합니다(리더보드는 응답 오면 채워짐).
	RefreshRank();
	if (!bLeaderboardRequestInFlight)
	{
		PopulateLeaderboard();
	}
}

void UShowDownRankWidget::NativeDestruct()
{
	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &UShowDownRankWidget::HandleBackClicked);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnPlayerDataLoaded.RemoveDynamic(this, &UShowDownRankWidget::HandlePlayerDataLoaded);
			SupabaseSubsystem->OnLeaderboardLoaded.RemoveDynamic(this, &UShowDownRankWidget::HandleLeaderboardLoaded);
		}
	}

	Super::NativeDestruct();
}

void UShowDownRankWidget::RefreshRank()
{
	UGameInstance* GameInstance = GetGameInstance();
	USupabaseSubsystem* SupabaseSubsystem = GameInstance
		? GameInstance->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SupabaseSubsystem)
	{
		return;
	}

	if (Text_RankScore)
	{
		Text_RankScore->SetText(
			FText::FromString(FString::Printf(TEXT("내 점수: %d"), SupabaseSubsystem->GetScore()))
		);
	}

	if (Text_RankNickname)
	{
		Text_RankNickname->SetText(FText::FromString(SupabaseSubsystem->GetNickname()));
	}
}

void UShowDownRankWidget::RequestRankRefresh()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			if (bLeaderboardRequestInFlight)
			{
				SetStatusMessage(TEXT("Ranking refresh is already in progress."), FLinearColor::Yellow);
				return;
			}

			bLeaderboardRequestInFlight = true;
			SetStatusMessage(TEXT("Loading rankings..."), FLinearColor::Yellow);
			SupabaseSubsystem->LoadPlayerData();
			SupabaseSubsystem->LoadLeaderboard();
		}
	}
}

int32 UShowDownRankWidget::GetRankScore() const
{
	UGameInstance* GameInstance = GetGameInstance();
	USupabaseSubsystem* SupabaseSubsystem = GameInstance
		? GameInstance->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	return SupabaseSubsystem ? SupabaseSubsystem->GetScore() : 0;
}

void UShowDownRankWidget::PopulateLeaderboard()
{
	// 목록을 담을 세로 박스가 WBP에 없으면 표시를 생략합니다(크래시 방지).
	if (!Box_Entries)
	{
		return;
	}

	Box_Entries->ClearChildren();

	UGameInstance* GameInstance = GetGameInstance();
	USupabaseSubsystem* SupabaseSubsystem = GameInstance
		? GameInstance->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SupabaseSubsystem || !WidgetTree)
	{
		return;
	}

	// "순위. 닉네임   점수" 한 줄씩 코드로 만들어 박스에 추가합니다.
	for (const FShowDownRankEntry& Entry : SupabaseSubsystem->GetLeaderboard())
	{
		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (!Row)
		{
			continue;
		}

		Row->SetText(FText::FromString(FString::Printf(
			TEXT("%d.  %s   %d"),
			Entry.Rank,
			*Entry.Nickname,
			Entry.Score)));

		Box_Entries->AddChildToVerticalBox(Row);
	}

	if (SupabaseSubsystem->GetLeaderboard().Num() == 0)
	{
		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (Row)
		{
			Row->SetText(FText::FromString(TEXT("No ranking data yet.")));
			Row->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Box_Entries->AddChildToVerticalBox(Row);
		}
	}
}

void UShowDownRankWidget::SetStatusMessage(const FString& Message, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
		return;
	}

	if (Box_Entries && WidgetTree)
	{
		Box_Entries->ClearChildren();

		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (Row)
		{
			Row->SetText(FText::FromString(Message));
			Row->SetColorAndOpacity(FSlateColor(Color));
			Box_Entries->AddChildToVerticalBox(Row);
		}
	}
}

void UShowDownRankWidget::HandlePlayerDataLoaded(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		RefreshRank();
	}
}

void UShowDownRankWidget::HandleLeaderboardLoaded(bool bSuccess, const FString& Message)
{
	if (Message == TEXT("Loading rankings..."))
	{
		SetStatusMessage(Message, FLinearColor::Yellow);
		return;
	}

	bLeaderboardRequestInFlight = false;

	if (bSuccess)
	{
		SetStatusMessage(Message, FLinearColor::Green);
		PopulateLeaderboard();
		return;
	}

	SetStatusMessage(Message, FLinearColor::Red);
}

void UShowDownRankWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
