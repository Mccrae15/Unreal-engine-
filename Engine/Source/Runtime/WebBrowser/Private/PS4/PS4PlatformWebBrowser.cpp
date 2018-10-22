// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformWebBrowser.h"
#define SUPPRESS_MONOLITHIC_HEADER_WARNINGS 1 // This is necessary because a game project may contain an older revision of the OnlineSubsystem plugin, which includes monolithic headers.
#undef SUPPRESS_MONOLITHIC_HEADER_WARNINGS
#if WITH_ENGINE
#include "Net/OnlineEngineInterface.h"
#endif
#include "Widgets/SLeafWidget.h"

// FVector2D MaxPos = AllottedGeometry.AbsolutePosition + AllottedGeometry.GetLocalSize();

class SPS4WebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SPS4WebBrowserWidget)
		: _InitialURL("about:blank")
	{ }

	SLATE_ARGUMENT(FString, InitialURL);
	
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;

	void LoadURL(const FString& NewURL);

	void Close();
	
	void HandleWebURLClosed(const FString& FinalUrl);

protected:
	void ShowWebURL(FIntPoint IntPos, FIntPoint IntSize) const;

	mutable bool bHasShownURL;
	FString CurrentURL;
};

void SPS4WebBrowserWidget::Construct(const FArguments& Args)
{
	bHasShownURL = false;
	CurrentURL = Args._InitialURL;
}

int32 SPS4WebBrowserWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if( bHasShownURL == false && !CurrentURL.IsEmpty() )
	{
		FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation();
		FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize());

		// Convert position to integer coordinates
		FIntPoint IntPos(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y));
		// Convert size to integer taking the rounding of position into account to avoid double round-down or double round-up causing a noticeable error.
		FIntPoint IntSize = FIntPoint(FMath::RoundToInt(Position.X + Size.X), FMath::RoundToInt(Size.Y + Position.Y)) - IntPos;
		ShowWebURL(IntPos, IntSize);
		bHasShownURL = true;
	}

	return LayerId;
}

void SPS4WebBrowserWidget::ShowWebURL(FIntPoint IntPos, FIntPoint IntSize) const
{
#if WITH_ENGINE
	if (!CurrentURL.IsEmpty())
	{
		UOnlineEngineInterface::FShowWebUrlParams Params;
		Params.bEmbedded = true;
		Params.bShowBackground = true;
		Params.bShowCloseButton = true;
		Params.bHideCursor = true;
		Params.OffsetX = IntPos.X;
		Params.OffsetY = IntPos.Y;
		Params.SizeX = IntSize.X;
		Params.SizeY = IntSize.Y;

		UOnlineEngineInterface::Get()->ShowWebURL(CurrentURL, Params, FOnlineShowWebUrlClosed::CreateRaw(this, &SPS4WebBrowserWidget::HandleWebURLClosed));
	}
#endif
}

void SPS4WebBrowserWidget::HandleWebURLClosed(const FString& FinalUrl)
{

}


FVector2D SPS4WebBrowserWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(1920, 1080);
}


void SPS4WebBrowserWidget::LoadURL(const FString& NewURL)
{
	Close();
	CurrentURL = NewURL;
	bHasShownURL = false;
}

void SPS4WebBrowserWidget::Close()
{
#if WITH_ENGINE
	UOnlineEngineInterface::Get()->CloseWebURL();
#endif

	CurrentURL.Empty();
	bHasShownURL = false;
}

FWebBrowserWindow::FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
{
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser(true);
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
}


TSharedRef<SWidget> FWebBrowserWindow::CreateWidget()
{
	TSharedRef<SPS4WebBrowserWidget> BrowserWidgetRef =
		SNew(SPS4WebBrowserWidget)
		.InitialURL(CurrentUrl);
 
	BrowserWidget = BrowserWidgetRef;
	return BrowserWidgetRef;
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
}

FSlateShaderResource* FWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return false;
}

bool FWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return EWebBrowserDocumentState::Loading;
}

FString FWebBrowserWindow::GetTitle() const
{
	return "";
}

FString FWebBrowserWindow::GetUrl() const
{
	return "";
}

bool FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FReply FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	
}

FReply FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FWebBrowserWindow::OnCaptureLost()
{
}

bool FWebBrowserWindow::CanGoBack() const
{
	return false;
}

void FWebBrowserWindow::GoBack()
{
}

bool FWebBrowserWindow::CanGoForward() const
{
	return false;
}

void FWebBrowserWindow::GoForward()
{
}

bool FWebBrowserWindow::IsLoading() const
{
	return false;
}

void FWebBrowserWindow::Reload()
{
}

void FWebBrowserWindow::StopLoad()
{
}

void FWebBrowserWindow::GetSource(TFunction<void(const FString&)> Callback) const
{
	Callback(FString());
}

int FWebBrowserWindow::GetLoadError()
{
	return 0;
}

void FWebBrowserWindow::SetIsDisabled(bool bValue)
{
}


void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
}

void FWebBrowserWindow::CloseBrowser(bool bForce)
{
	BrowserWidget->Close();
}

void FWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
}

void FWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
}