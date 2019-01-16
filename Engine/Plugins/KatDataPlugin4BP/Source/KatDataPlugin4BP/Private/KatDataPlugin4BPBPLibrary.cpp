// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KatDataPlugin4BPBPLibrary.h"
#include "KatDataPlugin4BP.h"

UKatDataPlugin4BPBPLibrary::UKatDataPlugin4BPBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

float UKatDataPlugin4BPBPLibrary::WalkPower_KatDataPlugin4BP()
{
	KatDataCore* kdc = KatDataCore::getInstance();
	kdc->KatGetFrame();
	return kdc->walkPower;
}

int32 UKatDataPlugin4BPBPLibrary::MoveDirection_KatDataPlugin4BP()
{
	KatDataCore* kdc = KatDataCore::getInstance();
	kdc->KatGetFrame();
	return kdc->moveDirection;
}

int32 UKatDataPlugin4BPBPLibrary::IsMoving_KatDataPlugin4BP()
{
	KatDataCore* kdc = KatDataCore::getInstance();
	kdc->KatGetFrame();
	return kdc->isMoving;
}

float UKatDataPlugin4BPBPLibrary::WalkDistance_KatDataPlugin4BP()
{
	KatDataCore* kdc = KatDataCore::getInstance();
	kdc->KatGetFrame();
	return kdc->walkDistance;
}

float UKatDataPlugin4BPBPLibrary::BodyYaw_KatDataPlugin4BP()
{
	KatDataCore* kdc = KatDataCore::getInstance();
	kdc->KatGetFrame();
	return kdc->bodyYaw;
}


