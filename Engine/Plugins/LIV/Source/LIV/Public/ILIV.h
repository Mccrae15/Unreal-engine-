#pragma once

#include <Modules/ModuleInterface.h>
#include <Modules/ModuleManager.h>

class ILIV : public IModuleInterface
{

public:
	/**
	 * Singleton-like access to LIV's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though. LIV might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading LIV on demand if needed
	 */
	static ILIV& Get()
	{
		return FModuleManager::LoadModuleChecked<ILIV>("LIV");
	}

	/**
	 * Checks to see if LIV is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if LIV is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("LIV");
	}
};
