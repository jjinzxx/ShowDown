#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShowDownCharacterSkinCatalog.h"
#include "ShowDownShopPreviewActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

/**
 * Lightweight, non-replicated character used by hub presentation screens.
 * It deliberately owns just a mesh so opening the shop cannot trigger any
 * gameplay character logic, sockets, weapons, cards, or network state.
 */
UCLASS(Blueprintable, meta = (DisplayName = "ShowDown Character Preview Actor"))
class SHOWDOWN_API AShowDownShopPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AShowDownShopPreviewActor();

	void SetSkinCatalog(UShowDownCharacterSkinCatalog* InSkinCatalog);
	UShowDownCharacterSkinCatalog* GetSkinCatalog() const { return SkinCatalog; }

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Preview")
	void SetPreviewContext(EShowDownCharacterPreviewContext InPreviewContext);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Preview")
	bool SetPreviewSkin(const FString& RequestedSkinId);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Preview")
	bool ActivatePreview(const FString& RequestedSkinId);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Character Preview")
	void DeactivatePreview();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ShowDown|Character Preview")
	void RefreshEditorPreview();

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Preview")
	FString GetResolvedSkinId() const { return ResolvedSkinId; }

	UFUNCTION(BlueprintPure, Category = "ShowDown|Character Preview")
	bool IsPreviewActive() const { return bPreviewActive; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	/** Optional catalog used both by editor preview and runtime presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	TObjectPtr<UShowDownCharacterSkinCatalog> SkinCatalog;

	/** Chooses the independently-authored shop or main-menu animation profile. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	EShowDownCharacterPreviewContext PreviewContext =
		EShowDownCharacterPreviewContext::Shop;

	/** Skin shown in the editor viewport while composing the camera shot. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ShowDown|Character Preview")
	FString EditorPreviewSkinId = TEXT("robot");

private:
	UPROPERTY(Transient)
	FString ResolvedSkinId;

	bool bPreviewActive = false;

	void ResetAnimationState();
};
