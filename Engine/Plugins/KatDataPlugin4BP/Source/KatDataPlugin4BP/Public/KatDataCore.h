
#pragma once

//#include "CoreMinimal.h"
//#include "GameFramework/Actor.h"
#include "Engine.h"


//Core Library  based on walkerbase.dll
class KatDataCore
{
public:
	static KatDataCore  * getInstance();

	//Legacy Members
	float bodyYaw;

	float walkPower;

	int32 moveDirection;

	int32 isMoving;
	 
	float walkDistance;

	void KatInit(int32 count);

	void KatLaunch();

	void KatHalt();

	bool KatCheckForLaunch();

	//checking state is necessary whether Initializing succeed or not.
	void KatGetFrame(int32 count=0);

private:
	
	KatDataCore();
	~KatDataCore();

	KatDataCore(const KatDataCore& h) = default;

	static KatDataCore* instance;

	
	//init
	void Initialization();

	void destructMembers();

	void autoStart();

	bool checkState();
};
