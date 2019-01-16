#pragma once

#include <CoreMinimal.h>
#include <Engine/CanvasRenderTarget2D.h>
#include <Materials/MaterialInstanceDynamic.h>
#include "MixedRealityCanvas.generated.h"

UCLASS()
class LIV_API UMixedRealityCanvas : public UCanvasRenderTarget2D
{
	GENERATED_BODY()

public:
	UMixedRealityCanvas();

	void UpdateResource() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "LIV")
		UTextureRenderTarget2D* BackgroundTexture;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "LIV")
		UTextureRenderTarget2D* ForegroundTexture;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "LIV")
		UTextureRenderTarget2D* PostprocessedForegroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LIV")
		UMaterialInstanceDynamic* BG_Tonemapper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LIV")
		UMaterialInstanceDynamic* FG_Tonemapper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LIV")
		UMaterialInstanceDynamic* FG_CombineAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LIV")
		bool bPostprocessBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LIV")
		bool bPostprocessForeground;

	void UpdateMaterialsTextures() const;

protected:
	UFUNCTION()
		void OnRender(UCanvas* Canvas, int32 Width, int32 Height);
};
