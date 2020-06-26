#pragma once
#include "DataTypes.h"

typedef enum {
    SIM_InitError_None = 0,
    SIM_InitError_WSAStartUp_Failed = 1,
    SIM_InitError_Already_Inited = 2,
    SIM_InitError_Device_Not_Found = 3,
    SIM_InitError_Can_Not_Connect_Server = 4,
    SIM_InitError_IPAddress_Null = 5,

} SIM_InitError;

typedef enum {
    SIM_ConnectType_USB,
    SIM_ConnectType_Wifi
} SIM_ConnectType;

extern "C" __declspec(dllexport) SIM_InitError WVR_Init_S(SIM_ConnectType type, const char* IP);
extern "C" __declspec(dllexport) void WVR_Quit_S();

extern "C" __declspec(dllexport) void WVR_GetSyncPose_S(WVR_PoseOriginModel originModel, WVR_DevicePosePair_t* retPose, uint32_t PoseCount);
extern "C" __declspec(dllexport) bool WVR_GetInputButtonState_S(WVR_DeviceType type, WVR_InputId id);
extern "C" __declspec(dllexport) bool WVR_GetInputTouchState_S(WVR_DeviceType type, WVR_InputId id);
extern "C" __declspec(dllexport) WVR_Axis_t WVR_GetInputAnalogAxis_S(WVR_DeviceType type, WVR_InputId id);
extern "C" __declspec(dllexport) bool WVR_IsDeviceConnected_S(WVR_DeviceType type);
extern "C" __declspec(dllexport) uint32_t WVR_GetParameters_S(WVR_DeviceType type, const char* pchValue, char* retValue, uint32_t unBufferSize);
extern "C" __declspec(dllexport) WVR_NumDoF WVR_GetDegreeOfFreedom_S(WVR_DeviceType type);
extern "C" __declspec(dllexport) float WVR_GetDeviceBatteryPercentage_S(WVR_DeviceType type);
extern "C" __declspec(dllexport) bool WVR_PollEventQueue_S(WVR_Event_t* event);
typedef void(__stdcall *PrintLog)(const char* str);
extern "C" __declspec(dllexport)  void SetPrintCallback(PrintLog callback);

void MPrint(const char* str);
