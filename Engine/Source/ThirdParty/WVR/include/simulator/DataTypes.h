#ifndef DATATYPES_H
#define DATATYPES_H

#include "include/wvr/wvr.h"
#include "include/wvr/wvr_types.h"
#include "include/wvr/wvr_device.h"
#include "include/wvr/wvr_events.h"

#define SIMULATOR_DEVICE_COUNT WVR_DEVICE_COUNT_LEVEL_1
#define SIMULATOR_RENDER_MODEL_LENGTH 64
#define CHECK_LIFE_KEY 1234567890
#define WIFI_SUPPORT 0

static WVR_InputAttribute pressIds[] = {
    { WVR_InputId_Alias1_System     , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_Menu       , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_Grip       , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_DPad_Left  , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_DPad_Up    , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_DPad_Right , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_DPad_Down  , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_Volume_Up  , WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_Volume_Down, WVR_InputType_Button, WVR_AnalogType_None },
    { WVR_InputId_Alias1_Digital_Trigger, WVR_InputType_Button | WVR_InputType_Analog, WVR_AnalogType_1D },
    { WVR_InputId_Alias1_Touchpad   , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_2D },
    { WVR_InputId_Alias1_Trigger    , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_1D },
};

static WVR_InputAttribute touchIds[] = {
    { WVR_InputId_Alias1_Touchpad   , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_2D },
    { WVR_InputId_Alias1_Trigger    , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_1D },
};

static WVR_InputAttribute axisIds[] = {
    { WVR_InputId_Alias1_Touchpad   , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_2D },
    { WVR_InputId_Alias1_Trigger    , WVR_InputType_Button | WVR_InputType_Analog   , WVR_AnalogType_1D },
};

typedef struct SIM_Command {
    char cmd[3];
    int  arg1;
    char arg2[16];
} SIM_Command_t;

typedef struct HMDStatus {
    bool connected;
    WVR_NumDoF dof;
    float batteryPercentage;
} HMDStatus_t;

typedef struct ButtonState {
    WVR_InputId source;
    bool press;
} ButtonState_t;

typedef struct TouchState {
    WVR_InputId source;
    bool press;
} TouchState_t;

typedef struct AxisState {
    WVR_InputId source;
    WVR_Axis_t axis;
} AxisState_t;

typedef struct DeviceStatus {
    bool connected;
    WVR_NumDoF dof;
    float batteryPercentage;

    ButtonState buttonState[sizeof(pressIds)/sizeof(WVR_InputAttribute)];
    TouchState touchState[sizeof(touchIds)/sizeof(WVR_InputAttribute)];
    AxisState axisValue[sizeof(axisIds)/sizeof(WVR_InputAttribute)];
    char renderModelName[SIMULATOR_RENDER_MODEL_LENGTH];
} DeviceStatus_t;

typedef struct SIM_AllData {
    WVR_DevicePosePair_t mPose[SIMULATOR_DEVICE_COUNT];
    HMDStatus_t mHMD;
    DeviceStatus_t mDevice[SIMULATOR_DEVICE_COUNT - 1];
    bool hasEvent;
    WVR_Event_t mEvent;
} SIM_AllData_t;


#endif //DATATYPES_H
