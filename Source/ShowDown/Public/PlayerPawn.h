// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "ShowDownTypes.h"


#include "PlayerPawn.generated.h"

class ACard;


UCLASS()
class SHOWDOWN_API APlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	APlayerPawn();

	
	//루트 컴포넌트
	UPROPERTY(VisibleAnywhere, Category= Components)
	USceneComponent* rootComp;
	
	//카메라 컴포넌트 선언 
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* cameraComp;
	//손패 카드 슬롯 위치 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = CardSlot)
	class USceneComponent* PlayerHandCard;
	
	//상대가 나에게 주는 카드 위치 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = CardSlot)
	class USceneComponent* PlayerHeadCard;
	
	//IMC 선택 필드 선언
	UPROPERTY(EditDefaultsOnly, Category = Input)
	class UInputMappingContext* imc_SD;
	
	//카드 선택 액션
	UPROPERTY(EditDefaultsOnly, Category = Input)
	class UInputAction* ia_Select;
	
	//카메라 무브
	UPROPERTY(EditDefaultsOnly, Category = Input)
	class UInputAction* ia_LookUp;//상하
	UPROPERTY(EditDefaultsOnly, Category = Input)
	class UInputAction* ia_Turn;//좌우

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_CheckOrCall;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_Raise;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_Fold;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_RaiseTo2;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_RaiseTo3;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_RaiseTo4;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_RaiseTo5;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Betting")
	class UInputAction* ia_RaiseTo6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bEnableLegacyKeyboardBetHotkeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Input")
	bool bUseMousePositionLookFallback = false;

	UPROPERTY(EditDefaultsOnly, Category = "ShowDown|Chat")
	TSubclassOf<class UShowDownChatWidget> ChatWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Chat")
	FKey ToggleChatKey = EKeys::Enter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Chat")
	FKey CloseChatKey = EKeys::Escape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Voice")
	bool bEnableVoicePushToTalk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Voice")
	FKey VoicePushToTalkKey = EKeys::T;
	
	//카메라 회전 감도
	UPROPERTY(EditAnywhere, Category = Camera)
	float LookSensitivity = 0.08f;
	
	//카메라 회전 각도 제한
	//상하
	UPROPERTY(EditAnywhere, Category = Camera)
	float MinPitch = -35.0f;
	UPROPERTY(EditAnywhere, Category = Camera)
	float MaxPitch = 35.0f;
	//좌우
	UPROPERTY(EditAnywhere, Category = Camera)
	float MinYaw = -45.0f;
	UPROPERTY(EditAnywhere, Category = Camera)
	float MaxYaw = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShowDown|Debug")
	bool bEnableDebugWinCommand = false;
	/*
	//
	UPROPERTY(VisibleAnywhere, Category="Table")
	USceneComponent* TableFocusPoint;
	*/
	
	//모드베이스 참조
	UPROPERTY()
	class AShowDownGameModeBase* ModeBase;
	
	//카드참조
	UPROPERTY()
	class ACard* HandCard;
	UPROPERTY()
	class ACard* CurrentSelectedCard;
	

	// 디버그용: 콜렉터 상대 승리를 강제로 발생시킵니다.
	// 콘솔(`키)에서 SDWin 입력 시 실제 승리와 동일한 OnGameOver(Player) 흐름을 타
	// 보상 지급과 허브 복귀까지 테스트할 수 있습니다.
	UFUNCTION(Exec)
	void SDWin();

	//카드 감지
	void InputSelect(const FInputActionValue& InputValue);
	//카메라 회전
	void LookUp(const FInputActionValue& InputValue);
	void Turn(const FInputActionValue& InputValue);
	//마우스 아래에 있는 카드 탐지
	void TraceCardUnderCursor();
	//선택된 카드를 다시 누르면 제출
	void SelectCard(ACard* SelectedCard);
	//선택된 카드를 게임 모드 베이스를 통해 전달
	void SubmitSelectedCard(ACard* SelectedCard);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void ToggleChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void OpenChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void CloseChat();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Chat")
	void SubmitDialogueInput(const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerCheck();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerRaise();

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerRaiseTo(int32 BulletCount);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Betting")
	void RequestPlayerFold();

	UFUNCTION(Server, Reliable)
	void ServerSubmitDialogueInput(const FString& Text, const FString& SenderName);

	UFUNCTION(Server, Reliable)
	void ServerSubmitSelectedCard(ACard* SelectedCard);

	UFUNCTION(Server, Reliable)
	void ServerPlayerCheck();

	UFUNCTION(Server, Reliable)
	void ServerPlayerRaise();

	UFUNCTION(Server, Reliable)
	void ServerPlayerRaiseTo(int32 BulletCount);

	UFUNCTION(Server, Reliable)
	void ServerPlayerFold();
protected:
	// Called when the game starts or when spawned
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	//IMC등록
	void AddInputMappingContext();
	void ApplyCameraInput(float YawInput, float PitchInput);
	void SubmitPlayerBetAction(EShowDownBetAction Action, int32 TargetBet);
	class AShowDownGameModeBase* ResolveGameMode();
	void InputCheckOrCall(const FInputActionValue& InputValue);
	void InputRaise(const FInputActionValue& InputValue);
	void InputFold(const FInputActionValue& InputValue);
	void InputRaiseTo2(const FInputActionValue& InputValue);
	void InputRaiseTo3(const FInputActionValue& InputValue);
	void InputRaiseTo4(const FInputActionValue& InputValue);
	void InputRaiseTo5(const FInputActionValue& InputValue);
	void InputRaiseTo6(const FInputActionValue& InputValue);

	bool bHasPreviousMousePosition = false;
	float PreviousMouseX = 0.0f;
	float PreviousMouseY = 0.0f;
	bool bChatOpen = false;

	UPROPERTY()
	class UShowDownChatWidget* ChatWidget = nullptr;
	
	void HandleBettingHotkeys();
	void HandleVoicePushToTalkInput();
	void EnsureChatWidget();
	void ApplyChatInputMode(bool bOpen);
	FString GetChatSenderName() const;
};
