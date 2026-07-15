#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShowDownShopPreviewActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UShowDownCharacterSkinCatalog;

/**
 * Lightweight, non-replicated character used only while the shop is open.
 * It deliberately owns just a mesh so opening the shop cannot trigger any
 * gameplay character logic, sockets, weapons, cards, or network state.
 */
UCLASS(Blueprintable)
class SHOWDOWN_API AShowDownShopPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AShowDownShopPreviewActor();

	void SetSkinCatalog(UShowDownCharacterSkinCatalog* InSkinCatalog);

	UFUNCTION(BlueprintCallable, Category = "ShowDown|Shop Preview")
	bool SetPreviewSkin(const FString& RequestedSkinId);

	UFUNCTION(BlueprintPure, Category = "ShowDown|Shop Preview")
	FString GetResolvedSkinId() const { return ResolvedSkinId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Shop Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShowDown|Shop Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	/** Optional catalog containing per-skin meshes and shop animations. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ShowDown|Shop Preview")
	TObjectPtr<UShowDownCharacterSkinCatalog> SkinCatalog;

private:
	UPROPERTY(Transient)
	FString ResolvedSkinId;
};
