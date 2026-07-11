#include "ShowDownMainMenuWidget.h"

#include "SupabaseSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "ShowDownShopWidget.h"

void UShowDownMainMenuWidget::SetUseLegacyNavigation(bool bInUseLegacyNavigation)
{
	bUseLegacyNavigation = bInUseLegacyNavigation;
}

void UShowDownMainMenuWidget::ShowStatusMessage(const FString& Message, const FLinearColor& Color)
{
	SetStatusMessage(Message, Color);
}

void UShowDownMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// SupabaseSubsystem의 닉네임 변경 완료 이벤트를 MainMenu UI 함수에 연결합니다.
	// Change 버튼을 눌러 닉네임 변경이 끝나면 HandleNicknameUpdated가 호출됩니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnPlayerDataLoaded.AddUniqueDynamic(
				this,
				&UShowDownMainMenuWidget::HandlePlayerDataLoaded
			);

			SupabaseSubsystem->OnCosmeticDataLoaded.AddUniqueDynamic(
				this,
				&UShowDownMainMenuWidget::HandleCosmeticDataLoaded
			);

			SupabaseSubsystem->OnNicknameUpdated.AddUniqueDynamic(
				this,
				&UShowDownMainMenuWidget::HandleNicknameUpdated
			);
		}
	}

	// 버튼들이 WBP와 정상 연결되어 있으면 각각의 클릭 이벤트를 C++ 함수에 연결합니다.
	if (Button_ChangeNickname)
	{
		Button_ChangeNickname->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleChangeNicknameClicked);
	}

	if (Button_SinglePlay)
	{
		Button_SinglePlay->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleSinglePlayClicked);
	}

	if (Button_Multiplayer)
	{
		Button_Multiplayer->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleMultiplayerClicked);
	}

	if (Button_Shop)
	{
		Button_Shop->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleShopClicked);
	}

	if (Button_Ranking)
	{
		Button_Ranking->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleRankingClicked);
	}

	if (Button_Quit)
	{
		Button_Quit->OnClicked.AddUniqueDynamic(this, &UShowDownMainMenuWidget::HandleQuitClicked);
	}

	// 위젯이 처음 뜰 때 로그인 후 불러온 플레이어 정보를 표시합니다.
	RefreshPlayerInfo();
	SetStatusMessage(TEXT(""), FLinearColor::White);
}

void UShowDownMainMenuWidget::NativeDestruct()
{
	if (Button_ChangeNickname)
	{
		Button_ChangeNickname->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleChangeNicknameClicked);
	}
	if (Button_SinglePlay)
	{
		Button_SinglePlay->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleSinglePlayClicked);
	}
	if (Button_Multiplayer)
	{
		Button_Multiplayer->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleMultiplayerClicked);
	}
	if (Button_Shop)
	{
		Button_Shop->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleShopClicked);
	}
	if (Button_Ranking)
	{
		Button_Ranking->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleRankingClicked);
	}
	if (Button_Quit)
	{
		Button_Quit->OnClicked.RemoveDynamic(this, &UShowDownMainMenuWidget::HandleQuitClicked);
	}

	// 위젯이 사라질 때 닉네임 변경 이벤트 연결을 해제합니다.
	// 같은 위젯이 다시 생성될 때 이벤트가 중복으로 연결되는 것을 막습니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USupabaseSubsystem* SupabaseSubsystem = GameInstance->GetSubsystem<USupabaseSubsystem>())
		{
			SupabaseSubsystem->OnPlayerDataLoaded.RemoveDynamic(
				this,
				&UShowDownMainMenuWidget::HandlePlayerDataLoaded
			);

			SupabaseSubsystem->OnCosmeticDataLoaded.RemoveDynamic(
				this,
				&UShowDownMainMenuWidget::HandleCosmeticDataLoaded
			);

			SupabaseSubsystem->OnNicknameUpdated.RemoveDynamic(
				this,
				&UShowDownMainMenuWidget::HandleNicknameUpdated
			);
		}
	}

	Super::NativeDestruct();
}

void UShowDownMainMenuWidget::RefreshPlayerInfo()
{
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;

	if (!SupabaseSubsystem)
	{
		if (Text_Nickname)
		{
			Text_Nickname->SetText(FText::FromString(TEXT("Nickname: Unknown")));
		}

		if (Text_Coin)
		{
			Text_Coin->SetText(FText::FromString(TEXT("Coin: 0")));
		}

		if (Text_Score)
		{
			Text_Score->SetText(FText::FromString(TEXT("Score: 0")));
		}

		return;
	}

	if (Text_Nickname)
	{
		Text_Nickname->SetText(
			FText::FromString(FString::Printf(TEXT("Nickname: %s"), *SupabaseSubsystem->GetNickname()))
		);
	}

	if (Text_Coin)
	{
		Text_Coin->SetText(
			FText::FromString(FString::Printf(TEXT("Coin: %d"), SupabaseSubsystem->GetCoin()))
		);
	}

	if (Text_Score)
	{
		Text_Score->SetText(
			FText::FromString(FString::Printf(TEXT("Score: %d"), SupabaseSubsystem->GetScore()))
		);
	}
}

void UShowDownMainMenuWidget::SetStatusMessage(const FString& Message, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Message));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShowDownMainMenuWidget::HandleChangeNicknameClicked()
{
	// 닉네임 입력창에서 사용자가 입력한 새 닉네임을 읽어옵니다.
	// 입력창이 연결되지 않았다면 빈 문자열을 사용해 크래시를 막습니다.
	const FString NewNickname = EditableTextBox_Nickname
		? EditableTextBox_Nickname->GetText().ToString()
		: TEXT("");

	// SupabaseSubsystem에 닉네임 변경 요청을 맡깁니다.
	// 실제 HTTP PATCH 요청은 SupabaseSubsystem에서 처리합니다.
	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (SupabaseSubsystem)
	{
		SupabaseSubsystem->UpdateNickname(NewNickname);
	}
}

void UShowDownMainMenuWidget::HandleNicknameUpdated(bool bSuccess, const FString& Message)
{
	// 닉네임 변경 결과를 로그에 남깁니다.
	// 실패했다면 여기서 Message를 보면 대략적인 원인을 알 수 있습니다.
	UE_LOG(LogTemp, Log, TEXT("Nickname update result: %s"), *Message);

	// 성공했을 때만 화면 정보를 새로고침합니다.
	// SupabaseSubsystem 내부 Nickname 값이 이미 새 닉네임으로 갱신된 상태입니다.
	if (bSuccess)
	{
		SetStatusMessage(Message, FLinearColor::Green);
		RefreshPlayerInfo();

		// 닉네임 변경이 끝났으니 입력창을 비워 다음 입력을 준비합니다.
		if (EditableTextBox_Nickname)
		{
			EditableTextBox_Nickname->SetText(FText::GetEmpty());
		}
		return;
	}

	SetStatusMessage(Message, FLinearColor::Red);
}

void UShowDownMainMenuWidget::HandlePlayerDataLoaded(bool bSuccess, const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("Player data load result: %s"), *Message);

	if (Message == TEXT("Processing win reward..."))
	{
		SetStatusMessage(Message, FLinearColor::Yellow);
		return;
	}

	if (Message == TEXT("Reward is already being processed."))
	{
		SetStatusMessage(Message, FLinearColor::Yellow);
		return;
	}

	if (bSuccess)
	{
		RefreshPlayerInfo();
		if (Message == TEXT("Win reward granted.") || Message == TEXT("Win reward was already claimed."))
		{
			SetStatusMessage(Message, FLinearColor::Green);
		}
		return;
	}

	SetStatusMessage(Message, FLinearColor::Red);
}

void UShowDownMainMenuWidget::HandleCosmeticDataLoaded(bool bSuccess, const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("Cosmetic data load result: %s"), *Message);
}

void UShowDownMainMenuWidget::HandleSinglePlayClicked()
{
	OnSinglePlayRequested.Broadcast();

	if (!bUseLegacyNavigation)
	{
		return;
	}

	// 싱글 플레이 맵 이름은 팀에서 확정한 뒤 여기에 넣으면 됩니다.
	UE_LOG(LogTemp, Log, TEXT("Single Play clicked"));

	// 예시:
	// UGameplayStatics::OpenLevel(this, TEXT("L_SingleGame"));
}

void UShowDownMainMenuWidget::HandleRankingClicked()
{
	// 화면 전환(랭킹 위젯 생성/표시)은 HubFlowManager가 담당합니다.
	OnRankingRequested.Broadcast();
}

void UShowDownMainMenuWidget::HandleMultiplayerClicked()
{
	OnMultiplayerRequested.Broadcast();

	if (!bUseLegacyNavigation)
	{
		return;
	}

	// 멀티플레이 메뉴 또는 방 생성 화면으로 연결할 자리입니다.
	UE_LOG(LogTemp, Log, TEXT("Multiplayer clicked"));
}

void UShowDownMainMenuWidget::HandleShopClicked()
{
	OnShopRequested.Broadcast();

	if (!bUseLegacyNavigation)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Shop clicked"));

	USupabaseSubsystem* SupabaseSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USupabaseSubsystem>()
		: nullptr;
	if (SupabaseSubsystem)
	{
		SupabaseSubsystem->LoadCosmeticData();

		const TArray<FShowDownSkin> ShopSkins = SupabaseSubsystem->GetShopSkins();
		const TArray<FString> OwnedSkinIds = SupabaseSubsystem->GetOwnedSkinIds();

		UE_LOG(
			LogTemp,
			Log,
			TEXT("Shop data summary - skins: %d, owned: %d"),
			ShopSkins.Num(),
			OwnedSkinIds.Num()
		);

		for (const FShowDownSkin& Skin : ShopSkins)
		{
			const bool bOwned = SupabaseSubsystem->IsSkinOwned(Skin.Id);
			const bool bEquipped = SupabaseSubsystem->IsShopItemEquipped(Skin.Id);

			UE_LOG(
				LogTemp,
				Log,
				TEXT("Shop item: %s / %s / %s / %d / owned=%s / equipped=%s"),
				*Skin.Id,
				*Skin.Name,
				*Skin.Type,
				Skin.Price,
				bOwned ? TEXT("true") : TEXT("false"),
				bEquipped ? TEXT("true") : TEXT("false")
			);
		}
	}

	ShopWidget = CreateWidget<UShowDownShopWidget>(
		GetOwningPlayer(),
		UShowDownShopWidget::StaticClass()
	);

	if (ShopWidget)
	{
		ShopWidget->SetMainMenuWidget(this);
		SetVisibility(ESlateVisibility::Collapsed);
		ShopWidget->AddToViewport();
		ShopWidget->SetKeyboardFocus();
	}
}

void UShowDownMainMenuWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();

	if (!bUseLegacyNavigation)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Quit clicked"));

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(
			this,
			PlayerController,
			EQuitPreference::Quit,
			false
		);
	}
}
