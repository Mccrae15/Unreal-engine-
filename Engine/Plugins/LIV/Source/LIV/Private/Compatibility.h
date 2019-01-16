#pragma once

#include <openvr.h>

class LIVCompatibility {

public:
	// See ETrackedDeviceClass in OpenVR
	static const int TrackedDeviceClassController = 2;
	static const int TrackedDeviceClassTracker = 3;

	// See ETrackedDeviceRole in OpenVR
	static const int TrackedDeviceRoleInvalid = 0;
	static const int TrackedDeviceRoleOptOut = 3;


	static vr::IVRCompositor* OpenVRCompositor()
	{
		if (OpenVRInterfaceCompositor == nullptr)
		{
			vr::EVRInitError InitError;
			OpenVRInterfaceCompositor = static_cast<vr::IVRCompositor*>(VR_GetGenericInterface(vr::IVRCompositor_Version, &InitError));
		}

		return OpenVRInterfaceCompositor;
	}

	static vr::IVRSystem* OpenVRSystem()
	{
		if (OpenVRInterfaceSystem == nullptr)
		{
			vr::EVRInitError InitError;
			OpenVRInterfaceSystem = static_cast<vr::IVRSystem*>(VR_GetGenericInterface(vr::IVRSystem_Version, &InitError));
		}

		return OpenVRInterfaceSystem;
	}

private:
	static vr::IVRCompositor* OpenVRInterfaceCompositor;
	static vr::IVRSystem* OpenVRInterfaceSystem;

};