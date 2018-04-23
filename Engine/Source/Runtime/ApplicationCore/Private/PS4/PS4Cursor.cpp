// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Cursor.h"
#include "PS4Application.h"

FPS4Cursor::FPS4Cursor() 
	: CurrentType(EMouseCursor::None)
	, CurrentPosition(FVector2D::ZeroVector)
	, CursorClipRect()
	, bShow(false)
{	
}

void FPS4Cursor::SetPosition( const int32 X, const int32 Y )
{
	FVector2D NewPosition(X, Y);
	UpdateCursorClipping(NewPosition);
	
	CurrentPosition = NewPosition;
}

void FPS4Cursor::SetType( const EMouseCursor::Type InNewCursor )
{
	CurrentType = InNewCursor;
}

void FPS4Cursor::GetSize(int32& Width, int32& Height) const
{
	Width = 32;
	Height = 32;
}

void FPS4Cursor::Show(bool bInShow)
{
	bShow = bInShow;
}

void FPS4Cursor::Lock(const RECT* const Bounds)
{
	if (Bounds == NULL)
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

		// The PS4 cursor should never leave the screen
		CursorClipRect.Min = FIntPoint::ZeroValue;
		CursorClipRect.Max.X = DisplayMetrics.PrimaryDisplayWidth - 1;
		CursorClipRect.Max.Y = DisplayMetrics.PrimaryDisplayHeight - 1;
	}
	else
	{
		CursorClipRect.Min.X = FMath::TruncToInt(Bounds->left);
		CursorClipRect.Min.Y = FMath::TruncToInt(Bounds->top);
		CursorClipRect.Max.X = FMath::TruncToInt(Bounds->right) - 1;
		CursorClipRect.Max.Y = FMath::TruncToInt(Bounds->bottom) - 1;
	}

	FVector2D Position = GetPosition();
	if (UpdateCursorClipping(Position))
	{
		SetPosition(Position.X, Position.Y);
	}
}

bool FPS4Cursor::UpdateCursorClipping(FVector2D& CursorPosition)
{
	bool bAdjusted = false;

	if (CursorClipRect.Area() > 0)
	{
		if (CursorPosition.X < CursorClipRect.Min.X)
		{
			CursorPosition.X = CursorClipRect.Min.X;
			bAdjusted = true;
		}
		else if (CursorPosition.X > CursorClipRect.Max.X)
		{
			CursorPosition.X = CursorClipRect.Max.X;
			bAdjusted = true;
		}

		if (CursorPosition.Y < CursorClipRect.Min.Y)
		{
			CursorPosition.Y = CursorClipRect.Min.Y;
			bAdjusted = true;
		}
		else if (CursorPosition.Y > CursorClipRect.Max.Y)
		{
			CursorPosition.Y = CursorClipRect.Max.Y;
			bAdjusted = true;
		}
	}

	return bAdjusted;
}
