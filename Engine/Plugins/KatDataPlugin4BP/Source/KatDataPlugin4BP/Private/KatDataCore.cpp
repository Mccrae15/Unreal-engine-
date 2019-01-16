#include "KatDataCore.h"
//#include "IPluginManager.h"

KatDataCore* KatDataCore::instance = nullptr;

bool needRestore = false;

typedef void(*_katInit)(int count);
typedef bool(*_katGetData)(int count, int &a, double&, int &b, int &c, float &d);
typedef int(*_katLaunch)();
typedef void(*_katHalt)();
typedef bool(*_katCheckForLaunch)();
void * KATDll;
_katInit katIni;
_katLaunch katLaunch;
_katHalt katHalt;
_katGetData katGetData;
_katCheckForLaunch katCheckForLaunch;



KatDataCore * KatDataCore::getInstance()
{
	if (instance == nullptr)
	{
		instance = new KatDataCore();
	}
	return instance;
}

void KatDataCore::KatInit(int32 count)
{
	if (katIni != NULL)
	{
		katIni(count);
		UE_LOG(LogTemp, Warning, TEXT("KatInit...Starting."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KatInit...Error."));
	}
}

void KatDataCore::KatLaunch()
{
	if (katLaunch != NULL)
	{
		katLaunch();
		UE_LOG(LogTemp, Warning, TEXT("KatLaunch...Starting."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KatLaunch...Error."));
	}
}

void KatDataCore::KatHalt()
{
	if (katHalt != NULL)
	{
		katHalt();
	}
}

bool KatDataCore::KatCheckForLaunch()
{
	if (!katCheckForLaunch())
		UE_LOG(LogTemp, Warning, TEXT("Runtime is not available."))
		return  katCheckForLaunch();
}

//Update
void KatDataCore::KatGetFrame(int32 count)
{
	//return false if something wrong.
	if (!checkState())
	{
		UE_LOG(LogTemp, Warning, TEXT("Restore the KatDataCore.."));
		Initialization();
		autoStart();
	}

	int bodyYaw1 = 0, moveDirection1 = 0, isMoving1 = 0;
	double walkPower1 = 0;
	float distancer1 = 0;

	if (katGetData != NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Update..."));
		bool temprout = katGetData(count, bodyYaw1, walkPower1, moveDirection1, isMoving1, distancer1);
		if (temprout)
		{
			bodyYaw = ((float)bodyYaw1 / 1024) * 360;
			isMoving = isMoving1;
			moveDirection = moveDirection1;
			walkPower = (float)walkPower1;
			walkDistance = distancer1;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("katGetData...Null."));
	}
	//
}


//Constructor
KatDataCore::KatDataCore()
{
	Initialization();
	autoStart();
}

KatDataCore::~KatDataCore()
{
	if (instance != nullptr)
	{
		delete instance;
	}
	destructMembers();
}

//initializing if constructing or disconnecting.
void KatDataCore::Initialization()
{
	destructMembers();

	UE_LOG(LogTemp, Warning, TEXT("KatDataCore Start..."));
	FString filePath = FPaths::Combine(*FPaths::ProjectPluginsDir(), TEXT("KatDataPlugin4BP/ThirdParty/WalkerBase.dll"));
	if (filePath.IsEmpty())
	{
		filePath = FPaths::Combine(*FPaths::ProjectPluginsDir(), TEXT("WalkerBase.dll"));
	}
	if (FPaths::FileExists(filePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *filePath);
		KATDll = FPlatformProcess::GetDllHandle(*filePath);
		if (KATDll != NULL)
		{
			katIni = NULL; // Local DLL function pointer.
			FString procName = "Ini"; // The exact name of the DLL function.
			FString procLog = "";
			katIni = (_katInit)FPlatformProcess::GetDllExport(KATDll, *procName); // Export the DLL function.

			if (katIni != NULL)
			{
				procLog = "KatInit export suc";
				UE_LOG(LogTemp, Warning, TEXT("%s"), *procLog);
			}

			katLaunch = NULL;
			procName = "Launch";
			katLaunch = (_katLaunch)FPlatformProcess::GetDllExport(KATDll, *procName);

			if (katLaunch != NULL)
			{
				procLog = "KatStart export suc";
				UE_LOG(LogTemp, Warning, TEXT("%s"), *procLog);
			}

			katHalt = NULL;
			procName = "Halt";
			katHalt = (_katHalt)FPlatformProcess::GetDllExport(KATDll, *procName);

			if (katHalt != NULL)
			{
				procLog = "KatStop export suc";
				UE_LOG(LogTemp, Warning, TEXT("%s"), *procLog);
			}

			katGetData = NULL;
			procName = "GetWalkerData";
			katGetData = (_katGetData)FPlatformProcess::GetDllExport(KATDll, *procName);

			if (katGetData != NULL)
			{
				procLog = "KatGetData export suc";
				UE_LOG(LogTemp, Warning, TEXT("%s"), *procLog);
			}
			katCheckForLaunch = NULL;
			procName = "CheckForLaunch";
			katCheckForLaunch = (_katCheckForLaunch)FPlatformProcess::GetDllExport(KATDll, *procName);

			if (katCheckForLaunch != NULL)
			{
				procLog = "katCheckForLaunch export suc";
				UE_LOG(LogTemp, Warning, TEXT("%s"), *procLog);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Search DLLPath Error!"));
	}
}


void KatDataCore::destructMembers()
{
	katIni = NULL;
	katLaunch = NULL;
	katGetData = NULL;
	if (katHalt != NULL)
	{
		katHalt();
	}
	katHalt = NULL;
	if (KATDll != NULL)
	{
		FPlatformProcess::FreeDllHandle(KATDll);
		KATDll = NULL;
	}
}

//Automatic Starting
void KatDataCore::autoStart()
{
	KatInit(1);
	KatLaunch();
}

bool KatDataCore::checkState()
{
	//check all mapping of funciton  and KatCheckForLaunch()
	if (katIni&&
		katLaunch&&
		katHalt&&
		katGetData&&
		katCheckForLaunch&&
		KatCheckForLaunch()
		)
	{
		//all process is done
		return true;
	}
	else
	{
		//something wrong
		return false;
	}

}
