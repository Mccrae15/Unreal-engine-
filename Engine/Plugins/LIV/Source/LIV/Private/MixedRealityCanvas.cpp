#include "MixedRealityCanvas.h"
#include <Engine/Canvas.h>
#include <Materials/MaterialInstanceDynamic.h>
#include <UObject/ConstructorHelpers.h>
#include "SharedTextureProtocol.h"

UMixedRealityCanvas::UMixedRealityCanvas()
	: BackgroundTexture(nullptr)
	, ForegroundTexture(nullptr)
	, BG_Tonemapper(nullptr)
{
	OnCanvasRenderTargetUpdate.AddUniqueDynamic(this, &UMixedRealityCanvas::OnRender);
	bGPUSharedFlag = true;

#ifdef UE_4_17_OR_LATER
	const auto tonemapperPath = TEXT("/LIV/CustomTonemapper");
	const auto combineAlphaPath = TEXT("/LIV/CombineAlpha");
#else
	const auto tonemapperPath = TEXT("/LIV/Legacy/CustomTonemapper");
	const auto combineAlphaPath = TEXT("/LIV/Legacy/CombineAlpha");
#endif

	// Create a material to draw alpha with
	static ConstructorHelpers::FObjectFinder<UMaterial> Find_Tonemapper(tonemapperPath);
	if (Find_Tonemapper.Succeeded())
	{
		BG_Tonemapper = UMaterialInstanceDynamic::Create(
			Find_Tonemapper.Object,
			this,
			TEXT("BG_Tonemapper")
		);
		FG_Tonemapper = UMaterialInstanceDynamic::Create(
			Find_Tonemapper.Object,
			this,
			TEXT("FG_Tonemapper")
		);
	}

	static ConstructorHelpers::FObjectFinder<UMaterial> Find_CombineAlpha(combineAlphaPath);
	if (Find_CombineAlpha.Succeeded())
	{
		FG_CombineAlpha = UMaterialInstanceDynamic::Create(
			Find_CombineAlpha.Object,
			this,
			TEXT("FG_CombineAlpha")
		);
	}
}

void UMixedRealityCanvas::UpdateResource()
{
	SharedTextureProtocol::SubmitTexture(this);
	UCanvasRenderTarget2D::UpdateResource();
}

void UMixedRealityCanvas::UpdateMaterialsTextures() const
{
	if (BG_Tonemapper)
		BG_Tonemapper->SetTextureParameterValue(TEXT("Input"), BackgroundTexture);

	if (FG_CombineAlpha)
	{
		FG_CombineAlpha->SetTextureParameterValue(TEXT("InputAlpha"), ForegroundTexture);
		FG_CombineAlpha->SetTextureParameterValue(TEXT("Input"), PostprocessedForegroundTexture);
	}

	if (FG_Tonemapper)
		FG_Tonemapper->SetTextureParameterValue(TEXT("Input"), ForegroundTexture);
}

void UMixedRealityCanvas::OnRender(UCanvas* Canvas, int32 Width, int32 Height)
{
	const FVector2D quad_size{ static_cast<float>(Width), Height * 0.5f };

	if (bPostprocessBackground)
	{
		Canvas->K2_DrawTexture(
			BackgroundTexture,
			FVector2D{ 0.0f, quad_size.Y },
			quad_size,
			FVector2D::ZeroVector
		);
	}
	else
	{
		Canvas->K2_DrawMaterial(
			BG_Tonemapper,
			FVector2D{ 0.0f, quad_size.Y },
			quad_size,
			FVector2D::ZeroVector
		);
	}

	Canvas->K2_DrawMaterial(
		bPostprocessForeground ? FG_CombineAlpha : FG_Tonemapper,
		FVector2D::ZeroVector,
		quad_size,
		FVector2D::ZeroVector
	);
}
