// "WaveVR SDK
// © 2017 HTC Corporation. All Rights Reserved.
//
// Unless otherwise required by copyright law and practice,
// upon the execution of HTC SDK license agreement,
// HTC grants you access to and use of the WaveVR SDK(s).
// You shall fully comply with all of HTC’s SDK license agreement terms and
// conditions signed by you and all SDK and API requirements,
// specifications, and documentation provided by HTC to You."


#ifndef wvr_system_h_
#define wvr_system_h_

#include "wvr_stdinc.h"

#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enum used for indicating the CPU/GPU performance levels
 *
*/
typedef enum {
    WVR_PerfLevel_System     = 0,            //!< System defined performance level (default)
    WVR_PerfLevel_Minimum    = 1,            //!< Minimum performance level
    WVR_PerfLevel_Medium     = 2,            //!< Medium performance level
    WVR_PerfLevel_Maximum    = 3,            //!< Maximum performance level
    WVR_PerfLevel_NumPerfLevels
} WVR_PerfLevel;

/**
 * @brief Function to check if input focus is captured by system.
 *
 * @return true when input focus is captured by system, otherwise return false.
 * @version API Level 1
*/
extern WVR_EXPORT bool WVR_IsInputFocusCapturedBySystem();

/**
 * @brief Set CPU and GPU performance level.
 *
 * This API is only supported on certain platform.
 * The API can be used to increase/decrease the CPU or GPU performance based on practical demands.
 *
 * @return true if set performance level success, otherwise return false.
 * @version API Level 2
 * @note Effective with Runtime version 2 or higher.
*/
extern WVR_EXPORT bool WVR_SetPerformanceLevels(WVR_PerfLevel cpuLevel, WVR_PerfLevel gpuLevel);

/**
 * @brief Enable adaptive render quailty and CPU/GPU performance level.
 *
 * The API can be used to allow WaveVR SDK runtime to adjust CPU/GPU perforamnce level automatically
 * and send WVR_EventType_RecommendedQuality_Lower/WVR_EventType_RecommendedQuality_Higer event
 * based on system workload.
 *
 * @return true if enableAdaptiveQuality success, otherwise return false.
 * @version API Level 4
 * @note Effective with Runtime version 4 or higher.
*/
extern WVR_EXPORT bool WVR_EnableAdaptiveQuality(bool enable);

/**
 * @brief Check adaptive render quailty enabled or not.
 *
 * The API can be used to allow WaveVR SDK runtime to check if adaptive quailty enabled.
 *
 * @return true if WVR_EnableAdaptiveQuality enabled, otherwise return false.
 * @version API Level 4
 * @note Effective with Runtime version 4 or higher.
*/
extern WVR_EXPORT bool WVR_IsAdaptiveQualityEnabled();

#ifdef __cplusplus
} /* extern "C" */
#endif
#include "close_code.h"

#endif /* wvr_system_h_ */

