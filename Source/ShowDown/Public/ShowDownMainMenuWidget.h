#pragma once

#include "CoreMinimal.h"
#include "ShowDownUserWidget.h"
#include "ShowDownMainMenuWidget.generated.h"

class UTextBlock;
class UEditableTextBox;
class UButton;
class UShowDownShopWidget;

// MainMenu 버튼이 눌렸다는 요청 이벤트입니다.
// 실제 화면 전환, 카메라 이동, 레벨 이동은 HubFlowManager가 담당할 수 있습니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShowDownMainMenuRequest);

UCLASS()
class SHOWDOWN_API UShowDownMainMenuWidget : public UShowDownUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownMainMenuRequest OnSinglePlayRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownMainMenuRequest OnMultiplayerRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownMainMenuRequest OnShopRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownMainMenuRequest OnRankingRequested;

	UPROPERTY(BlueprintAssignable, Category = "ShowDown|Flow")
	FOnShowDownMainMenuRequest OnQuitRequested;

	// true면 기존처럼 MainMenuWidget이 직접 ShopWidget 생성/종료 처리를 합니다.
	// FlowManager가 연출 흐름을 맡을 때는 false로 꺼서 요청 이벤트만 보내게 합니다.
	void SetUseLegacyNavigation(bool bInUseLegacyNavigation);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|UI")
	void ShowStatusMessage(const FString& Message, const FLinearColor& Color);

protected:
	// 위젯이 화면에 생성되면 SupabaseSubsystem에서 현재 유저 데이터를 읽어 UI에 표시합니다.
	virtual void NativeConstruct() override;
	
	// 위젯이 제거될 때 Supabase 이벤트 연결을 해제합니다.
	// 이벤트가 남아 있으면 위젯이 사라진 뒤에도 함수가 호출될 수 있습니다.
	virtual void NativeDestruct() override;

	// Rebuilds the presentation layer at runtime while preserving the existing
	// Blueprint class and all C++ menu bindings.
	void BuildFigmaLayout();

private:
	bool bUseLegacyNavigation = true;
	bool bEditingNickname = false;

	// 현재 닉네임을 표시하는 텍스트입니다.
	// WBP_MainMenu 안의 위젯 이름과 정확히 같아야 자동 연결됩니다.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Nickname;

	// 현재 보유 coin을 표시하는 텍스트입니다.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Coin;

	// 현재 랭크 score를 표시하는 텍스트입니다.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Score;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Status;

	// 새 닉네임을 입력하는 입력창입니다.
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EditableTextBox_Nickname;

	// 닉네임 변경 버튼입니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_ChangeNickname;

	// 싱글 플레이 시작 버튼입니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_SinglePlay;

	// 멀티플레이 버튼입니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_Multiplayer;

	// 상점 버튼입니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_Shop;

	// 랭킹(점수 확인) 버튼입니다. WBP에서 Shop 버튼 위에 배치합니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_Ranking;

	// 게임 종료 버튼입니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Button_Quit;

	// 에셋 없이 C++에서 생성하는 데모 상점 위젯입니다.
	UPROPERTY()
	UShowDownShopWidget* ShopWidget;

	// SupabaseSubsystem에 저장된 nickname, coin, score를 화면에 반영합니다.
	void RefreshPlayerInfo();
	void SetStatusMessage(const FString& Message, const FLinearColor& Color);
	void SetNicknameEditing(bool bEditing);

	// Change Nickname 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleChangeNicknameClicked();

	// Single Play 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleSinglePlayClicked();

	// Multiplayer 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleMultiplayerClicked();

	// Shop 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleShopClicked();

	// Ranking 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleRankingClicked();

	// Quit 버튼을 눌렀을 때 실행됩니다.
	UFUNCTION()
	void HandleQuitClicked();
	
	// SupabaseSubsystem에서 닉네임 변경 결과를 알려주면 실행됩니다.
	// 성공하면 화면의 nickname / coin / score 표시를 다시 갱신합니다.
	UFUNCTION()
	void HandleNicknameUpdated(bool bSuccess, const FString& Message);

	// 로그인 직후 Supabase에서 플레이어 데이터가 늦게 도착하면 실행됩니다.
	// profile, wallet, rank 중 하나라도 성공적으로 로드될 때마다 화면 표시를 갱신합니다.
	UFUNCTION()
	void HandlePlayerDataLoaded(bool bSuccess, const FString& Message);

	// 상점 스킨, 보유 스킨, 장착 스킨 데이터가 로드되면 실행됩니다.
	UFUNCTION()
	void HandleCosmeticDataLoaded(bool bSuccess, const FString& Message);
};
