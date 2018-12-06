#pragma once
 
#include "Engine.h"
#include "Modules/ModuleManager.h"
#include "UnrealEd.h"
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"

class FSlateStyleSet;

class F3dRudderEditorModule: public IModuleInterface, public FTickableEditorObject
{
public:

	static ns3dRudder::CSdk* s_pSdk;

	// Module
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Settings
	void RegisterSettings();
	void UnregisterSettings(); 

	// Details custom
	void RegisterCustomizations();
	void UnregisterCustomizations();

	// Style custom
	void RegisterStyle();
	void unRegisterStyle();

	// Tickable
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const
	{
		return true;
	}
	virtual TStatId GetStatId() const
	{
		return TStatId();
	};

	// Editor
	void UpdateViewportCamera(const FVector& translation, float fRotation);

private:

	/** Slate styleset used by this module. **/
	TSharedPtr< FSlateStyleSet > StyleSet;
};