/* SCE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 02.508.820
* Copyright (C) 2015 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef INC_MOVE_CLIENT_H
#define INC_MOVE_CLIENT_H

#ifndef HMDSERVER_DLL_IMPORT
#define HMDSERVER_DLL_IMPORT __declspec(dllimport)
#endif

#define HMDCLIENT_VERSION "3.150.28"
#define HMDCLIENT_MIN_VERSION_NUMBER 28

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

// Type translations
typedef __int64 system_time_t;
typedef float float4[4];
typedef float float3[3];

typedef unsigned __int8 u8_t;
typedef unsigned __int16 u16_t;
typedef unsigned __int32 u32_t;
typedef __int32 i32_t;
typedef unsigned __int64 u64_t;
typedef __int64 i64_t;

#define MOVE_SERVER_MAX_GEMS 4
#define MOVE_SERVER_MAX_NAVS 7
#define CELL_PAD_MAX_CODES 64

// Move structures
typedef struct _NavPadInfo {
	uint32_t port_status[MOVE_SERVER_MAX_NAVS];

} NavPadInfo, *LPNavPadInfo;

typedef struct _NavPadData {
	int32_t len;
	uint16_t button[CELL_PAD_MAX_CODES];

} NavPadData, *LPNavPadData;

typedef struct _MovePadData {
	uint16_t digital_buttons;
	uint16_t analog_trigger;

} MovePadData, *LPMovePadData;

typedef struct _MoveState {
	float4 pos;
	float4 vel;
	float4 accel;
	float4 quat;
	float4 angvel;
	float4 angaccel;
	float4 handle_pos;
	float4 handle_vel;
	float4 handle_accel;
	MovePadData pad;
	system_time_t timestamp;
	float temperature;
	float camera_pitch_angle;
	uint32_t tracking_flags;

} MoveState, *LPMoveState;


// Header structure
typedef struct _MoveServerPacketHeader
{
	uint32_t magic;
	uint32_t move_me_server_version;
	uint32_t packet_code;
	uint32_t packet_length;
	uint32_t packet_index;

} MoveServerPacketHeader, *LPMoveServerPacketHeader;


// Standard packet structures
typedef struct _MoveServerConfig
{
	int32_t num_image_slices;
	int32_t image_slice_format;

} MoveServerConfig, *LPMoveServerConfig;

typedef struct _MoveConnectionConfig
{
	uint32_t ms_delay_between_standard_packets;
	uint32_t ms_delay_between_camera_frame_packets;
	uint32_t camera_frame_packet_paused;

} MoveConnectionConfig, *LPMoveConnectionConfig;

typedef struct _MoveStatus
{
	uint32_t connected; // connected==1 means the controller is ready to be used
	uint32_t code;
	uint64_t flags;

} MoveStatus, *LPMoveStatus;

typedef struct _MoveImageState {
	system_time_t frame_timestamp; 
	system_time_t timestamp;
	float u;
	float v;
	float r;
	float projectionx;
	float projectiony;
	float distance;
	unsigned char visible;
	unsigned char r_valid;

} MoveImageState, *LPMoveImageState;

typedef struct _MoveSphereState {
	uint32_t tracking;
	uint32_t tracking_hue;
	float r;
	float g;
	float b;
	
} MoveSphereState, *LPMoveSphereState;

typedef struct _MoveCameraState {
	int32_t exposure;
	float exposure_time;
	float gain;
	float pitch_angle;
	float pitch_angle_estimate;
	
} MoveCameraState, *LPMoveCameraState;

typedef struct _MovePointerState
{
	uint32_t valid;
	float normalized_x;
	float normalized_y;

} MovePointerState, *LPMovePointerState;

typedef struct _MovePositionPointerState
{
	uint32_t valid;
	float normalized_x;
	float normalized_y;

} MovePositionPointerState, *LPMovePositionPointerState;


typedef struct _MoveServerPacket
{
	MoveServerPacketHeader header;
	MoveServerConfig server_config;
	MoveConnectionConfig client_config;
	MoveStatus status[MOVE_SERVER_MAX_GEMS];
	MoveState state[MOVE_SERVER_MAX_GEMS];
	MoveImageState image_state[MOVE_SERVER_MAX_GEMS];
	MovePointerState pointer_state[MOVE_SERVER_MAX_GEMS];
	NavPadInfo pad_info;
	NavPadData pad_data[MOVE_SERVER_MAX_NAVS];
	MoveSphereState sphere_state[MOVE_SERVER_MAX_GEMS];
	MoveCameraState camera_state;
	MovePositionPointerState position_pointer_state[MOVE_SERVER_MAX_GEMS];

} MoveServerPacket, *LPMoveServerPacket;

typedef struct _HMDServerPacketHeader
{
	MoveServerPacketHeader header;
	MoveServerConfig server_config;
	MoveConnectionConfig client_config;
} HMDServerPacketHeader, *LPHMDServerPacketHeader;

typedef struct _HMDServerPlayAreaWarningInfo
{
	uint32_t isOutOfPlayArea;
	uint32_t isDisanceDataValid;
	float distanceFromVerticalBoundary;
	float distanceFromHorizontalBoundary;
} HMDServerPlayAreaWarningInfo, *LPHMDServerPlayAreaWarningInfo;

typedef struct _HMDServerTrackerStates
{
	MoveStatus status[MOVE_SERVER_MAX_GEMS];
	MoveState state[MOVE_SERVER_MAX_GEMS];
	MoveImageState image_state[MOVE_SERVER_MAX_GEMS];
	MovePointerState pointer_state[MOVE_SERVER_MAX_GEMS];
	NavPadInfo pad_info;
	NavPadData pad_data[MOVE_SERVER_MAX_NAVS];
	MoveSphereState sphere_state[MOVE_SERVER_MAX_GEMS];
	MoveCameraState camera_state;
	MovePositionPointerState position_pointer_state[MOVE_SERVER_MAX_GEMS];
} HMDServerTackerStates, *LPHMDServerTrackerStates;

typedef struct _HMDServerPacket
{
	HMDServerPacketHeader header;
	HMDServerTackerStates tracker_states;
	HMDServerPlayAreaWarningInfo play_area_warning_info;
} HMDServerPacket, *LPHMDServerPacket;

// New for HMDs:
typedef struct _HMDServerHMDInfo // equivalent to SceHmdDeviceInfo for 1700
{
	uint32_t status;
	uint32_t modelId;
	struct DeviceInfo
	{
		// For Images
		struct FieldOfView
		{
			float tanOut;
			float tanIn;
			float tanTop;
			float tanBottom;
		}fieldOfView;
		struct PanelResolution
		{
			uint32_t width;
			uint32_t height;
		}panelResolution;
		uint32_t drawType;
		// For Tracking
		uint32_t  trackingSupportedType;
	}deviceInfo;
} HMDServerHMDInfo, *LPHMDServerHMDInfo;

typedef struct _HMDServerHMDInfo2000
{
	uint32_t status;
	uint32_t modelId;
	struct DeviceInfo
	{
		// For Images
		struct FieldOfView
		{
			float tanOut;
			float tanIn;
			float tanTop;
			float tanBottom;
		}fieldOfView;
		struct PanelResolution
		{
			uint32_t width;
			uint32_t height;
		}panelResolution;
		uint32_t drawType;
		// For Tracking
		uint32_t  trackingSupportedType;
		float4 orientation; // pantoscopic-tilt
	}deviceInfo;
} HMDServerHMDInfo2000, *LPHMDServerHMDInfo2000;

typedef struct _HMDServerHMDViewStatus // equivalent to SceHmdViewStatus for 1700
{
	struct EyeStatus
	{
		float3 position;
	} left, right;
} HMDServerHMDViewStatus, *LPHMDServerHMDViewStatus;

typedef struct _HMDServerHMDDistortionMapInfo
{
	uint32_t width;
	uint32_t height;
	uint32_t slices;
	uint32_t distortion_map_type; // 2: half, 4: float
	uint32_t slice_size_bytes; // equal to pitch * height * sizeof(distortion_map_type)
} HMDServerHMDDistortionMapInfo, *LPHMDServerHMDDistortionMapInfo;

typedef struct _HMDServerHMDDistortionParams
{
	float coefficient_red[5];
	float coefficient_green[5];
	float coefficient_blue[5];
	float display_offset_left_x;
	float display_offset_left_y;
	float display_offset_right_x;
	float display_offset_right_y;
} HMDServerHMDDistortionParams, *LPHMDServerHMDDistortionParams;


// Camera packet structure
typedef struct _MoveServerCameraFrameSlicePacket MoveServerCameraFrameSlicePacket, *LPMoveServerCameraFrameSlicePacket;

typedef struct _hmdclient hmdclient;
typedef hmdclient* hmdclient_handle;

enum HMDEye
{
	HMD_EYE_LEFT = 0,
	HMD_EYE_RIGHT = 1,
};

#define HMDCLIENT_ERROR_SUCCESS 0
#define HMDCLIENT_ERROR_INVALID_ARGUMENTS (-2)
#define HMDCLIENT_ERROR_INVALID_HANDLE (-4)
#define HMDCLIENT_ERROR_BUFFER_TOO_SMALL (-3)
#define HMDCLIENT_ERROR_NOT_READY (-1)
#define HMDCLIENT_ERROR_NOT_SUPPORTED (-5)
#define MOVEME_ERROR_NOT_CONNECTED (-1)
#define MOVEME_ERROR_ALREADY_CONNECTED (-1)
#define MOVEME_ERROR_COULD_NOT_CONNECT (-2)
#define MOVEME_ERROR_SUCCESS 0

// Prototypes

HMDSERVER_DLL_IMPORT
int movemeConnect(const char *lpRemoteAddress, const char *lpPort, uint16_t udp_port = 0);
typedef int(*FPMovemeConnect)(const char*, const char*, uint16_t);
HMDSERVER_DLL_IMPORT
int movemeDisconnect();
typedef int(*FPMovemeDisconnect)();
HMDSERVER_DLL_IMPORT
int movemePause();
typedef int(*FPMovemePause)();
HMDSERVER_DLL_IMPORT
int movemeResume();
typedef int(*FPMovemeResume)();
HMDSERVER_DLL_IMPORT
int movemeUpdateDelay(uint32_t delayMs); // in ms between updates
typedef int(*FPMovemeUpdateDelay)(uint32_t);
HMDSERVER_DLL_IMPORT
int movemeRumble(uint32_t gem_num, uint32_t rumble);
typedef int(*FPMovemeRumble)(uint32_t, uint32_t);
HMDSERVER_DLL_IMPORT
int movemeLatencyOffset(int32_t offset);
typedef int(*FPMovemeLatencyOffset)(int32_t);
HMDSERVER_DLL_IMPORT
int movemeGetPacket(LPMoveServerPacket lpPacket);
typedef int(*FPMovemeGetPacket)(LPMoveServerPacket);

// New for HMDS...
HMDSERVER_DLL_IMPORT
hmdclient_handle hmdclientConnect(const char* lpRemoteAddress, const char* lpPort, uint16_t udp_port = 0);
typedef hmdclient_handle(*FPHmdclientConnect)(const char*, const char*, uint16_t);
HMDSERVER_DLL_IMPORT
int hmdclientDisconnect(hmdclient_handle handle);
typedef int(*FPHmdclientDisconnect)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientRequestHMDInfo(hmdclient_handle handle);
typedef int(*FPHmdclientRequestHMDInfo)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientRequestHMDViewStatus(hmdclient_handle handle);
typedef int(*FPHmdclientRequestHMDViewStatus)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientRequestHMDDistortionMapSlice(hmdclient_handle handle, uint16_t eye, uint16_t slice);		// eye:0=left, 1=right, Starting slice:0-N
typedef int(*FPHmdclientRequestHMDDistortionMapSlice)(hmdclient_handle,uint16_t,uint16_t);
HMDSERVER_DLL_IMPORT
int hmdclientRequestHMDDistortionMapInfo(hmdclient_handle handle);
typedef int(*FPHmdclientRequestHMDDistortionMapInfo)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientRequestServerVersion(hmdclient_handle handle);
typedef int(*FPHmdclientRequestServerVersion)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientPause(hmdclient_handle handle);
typedef int(*FPHmdclientPause)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientResume(hmdclient_handle handle);
typedef int(*FPHmdclientResume)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientUpdateDelay(hmdclient_handle handle, uint32_t delayMs);
typedef int(*FPHmdclientUpdateDelay)(hmdclient_handle, uint32_t);
HMDSERVER_DLL_IMPORT
int hmdclientRumble(hmdclient_handle handle, uint32_t gem_num, uint32_t rumble);
typedef int(*FPHmdclientRumble)(hmdclient_handle, uint32_t, uint32_t);
HMDSERVER_DLL_IMPORT
int hmdclientLatencyOffset(hmdclient_handle handle, int32_t offset);
typedef int(*FPHmdclientLatencyOffset)(hmdclient_handle, int32_t);
HMDSERVER_DLL_IMPORT
int hmdclientGetPacket(hmdclient_handle handle, LPHMDServerPacket lpPacket);
typedef int(*FPHmdclientGetPacket)(hmdclient_handle, LPHMDServerPacket);
HMDSERVER_DLL_IMPORT
int hmdclientGetTrackerStates(hmdclient_handle handle, LPHMDServerTrackerStates lpTrackerStates);
typedef int(*FPHmdclientGetTrackerStates)(hmdclient_handle, LPHMDServerTrackerStates);
HMDSERVER_DLL_IMPORT
int hmdclientGetPlayAreaWarningInfo(hmdclient_handle handle, LPHMDServerPlayAreaWarningInfo lpPlayAreaWarningInfo);
typedef int(*FPHmdclientGetPlayAreaWarningInfo)(hmdclient_handle handle, LPHMDServerPlayAreaWarningInfo lpPlayAreaWarningInfo);

HMDSERVER_DLL_IMPORT
int hmdclientGetHMDInfo(hmdclient_handle handle, LPHMDServerHMDInfo info);
typedef int(*FPHmdclientGetHMDInfo)(hmdclient_handle, LPHMDServerHMDInfo info);
HMDSERVER_DLL_IMPORT
int hmdclientGetHMDInfo2000(hmdclient_handle handle, LPHMDServerHMDInfo2000 info);
typedef int(*FPHmdclientGetHMDInfo2000)(hmdclient_handle, LPHMDServerHMDInfo2000 info);
HMDSERVER_DLL_IMPORT
int hmdclientGetHMDViewStatus(hmdclient_handle handle, LPHMDServerHMDViewStatus status);
typedef int(*FPHmdClientGetHMDViewStatus)(hmdclient_handle handle, LPHMDServerHMDViewStatus);
HMDSERVER_DLL_IMPORT
int hmdclientGetHMDDistortionMapInfo(hmdclient_handle handle, LPHMDServerHMDDistortionMapInfo info);
typedef int(*FPHmdClientGetHMDDistortionMapInfo)(hmdclient_handle,LPHMDServerHMDDistortionMapInfo);
HMDSERVER_DLL_IMPORT
int hmdclientGetHMDDistortionMapSlice(hmdclient_handle handle, uint16_t eye, uint16_t slice, void* buf, size_t bufferLen);
typedef int(*FPHmdClientGetHMDDistortionMapSlice)(hmdclient_handle, uint16_t, uint16_t, void*, size_t);
HMDSERVER_DLL_IMPORT
int hmdclientGetServerVersionLength(hmdclient_handle handle);
typedef int(*FPHmdclientGetServerVersionLength)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientGetServerVersion(hmdclient_handle handle, char* buf, size_t bufferLen);
typedef int(*FPHmdclientGetServerVersion)(hmdclient_handle, char*, size_t);
HMDSERVER_DLL_IMPORT
int hmdclientRequestRecalibrate(hmdclient_handle handle);
typedef int(*FPHmdclientRequestRecalibrate)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientRequestChangePredictionTime(hmdclient_handle handle, uint32_t prediction_usec);
typedef int(*FPHmdclientRequestChangePreditionTime)(hmdclient_handle, uint32_t);
HMDSERVER_DLL_IMPORT
int hmdclientRequestHMDDistortionParams(hmdclient_handle handle);
typedef int(*FPHmdclientRequestHMDDistortionParams)(hmdclient_handle);
HMDSERVER_DLL_IMPORT
int hmdclientGetHMDDistortionParams(hmdclient_handle handle, LPHMDServerHMDDistortionParams params);
typedef int(*FPHmdclientGetHMDDistortionParams)(hmdclient_handle, LPHMDServerHMDDistortionParams);

// returns >0 if recenter event received, 0 if none, <0 on error
HMDSERVER_DLL_IMPORT
int hmdclientReceiveHmdRecenterEvent(hmdclient_handle handle);
typedef int(*FPHmdclientReceiveHmdRecenterEvent)(hmdclient_handle handle);

// Callback function pointers
typedef int (*FPUpdateSuccess)(LPMoveServerPacket);
typedef int (*FPUpdateFailure)(int);

typedef int (*FPUpdateCameraSuccess)(LPMoveServerCameraFrameSlicePacket);
typedef int (*FPUpdateCameraFailure)(int);


// Struct to hold callback and gem state to update
typedef struct _MoveStateDeferred {
	FPUpdateSuccess fpUpdateSuccess;
	FPUpdateFailure fpUpdateFailure;
	FPUpdateCameraSuccess fpUpdateCameraSuccess;
	FPUpdateCameraFailure fpUpdateCameraFailure;
	LPMoveServerPacket lpMoveServerPacket;
	LPMoveServerCameraFrameSlicePacket lpMoveServerCameraFrameSlicePacket;

} MoveStateDeferred, *LPMoveStateDeferred;

HMDSERVER_DLL_IMPORT int movemeConnectDeferredUpdate(const char* remoteAdress, const char* port, LPMoveStateDeferred);

#ifdef __cplusplus
}
#endif

#endif  // ... INC_MOVE_CLIENT_H
