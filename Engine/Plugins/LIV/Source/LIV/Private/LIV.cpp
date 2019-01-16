#include "PCH.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ILIV.h"

class FLIV : public ILIV
{
	void StartupModule() override;
	void ShutdownModule() override;
};
IMPLEMENT_MODULE(FLIV, LIV)

void FLIV::StartupModule() { }
void FLIV::ShutdownModule() { }
