#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownRankWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;

// 랭킹 화면에서 "뒤로(메인메뉴)" 요청을 알리는 이벤트입니다.
// HubFlowManager가 이 이벤트를 받아 메인메뉴로 되돌립니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownRankBackRequested);

// 플레이어의 랭크 점수(와 닉네임)를 표시하는 위젯입니다.
// 데이터는 SupabaseSubsystem(로그인 시 로드된 캐시)에서 읽고,
// OnPlayerDataLoaded를 구독해 점수가 갱신되면 자동으로 새로고침합니다.
//
// 사용법(나중에 에셋으로 화면을 만들 때):
//  1) WBP_Rank를 만들고 부모 클래스를 UShowDownRankWidget으로 지정
//  2) 그 안에 Text_RankScore / Text_RankNickname 이름의 TextBlock을 배치하면 자동 연결
//  - 이름이 없어도(BindWidgetOptional) 크래시 없이 동작하며 해당 표시만 생략됩니다.
//  - 즉 데이터 로딩/갱신 로직은 전부 이 C++에 있고, 디자인은 WBP가 담당합니다.
UCLASS()
class SHOWDOWN_API UShowDownRankWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	// 랭킹 화면을 닫고 메인메뉴로 돌아가 달라는 요청 이벤트입니다.
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Rank")
	FOnShowDownRankBackRequested OnBackRequested;

	// 현재 캐시된 내 랭크 점수/닉네임을 즉시 화면에 반영합니다.
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Rank")
	void RefreshRank();

	// 내 데이터 + 전체 리더보드를 서버에서 다시 불러옵니다.
	UFUNCTION(BlueprintCallable, Category = "ShowDown|Rank")
	void RequestRankRefresh();

	// 디자이너/연출 파트가 BindWidget 없이도 값을 쓰고 싶을 때를 위한 읽기 함수.
	UFUNCTION(BlueprintPure, Category = "ShowDown|Rank")
	int32 GetRankScore() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	bool bLeaderboardRequestInFlight = false;

	// WBP_Rank 안의 점수 표시 TextBlock과 연결됩니다(있을 때만).
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_RankScore;

	// WBP_Rank 안의 닉네임 표시 TextBlock과 연결됩니다(있을 때만).
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_RankNickname;

	// WBP_Rank 안의 "뒤로" 버튼과 연결됩니다(있을 때만).
	// 방법 B로 별도 화면으로 띄울 때 메뉴로 돌아가려면 이 버튼이 필요합니다.
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_Back;

	// WBP_Rank 안의 리더보드 목록 컨테이너(세로 박스)와 연결됩니다(있을 때만).
	// 여기에 "순위. 닉네임   점수" 형태의 줄을 코드에서 채워 넣습니다.
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* Box_Entries;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Status;

	// 리더보드 목록을 Box_Entries에 다시 그립니다.
	void PopulateLeaderboard();
	void SetStatusMessage(const FString& Message, const FLinearColor& Color);

	UFUNCTION()
	void HandlePlayerDataLoaded(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleLeaderboardLoaded(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleBackClicked();
};
