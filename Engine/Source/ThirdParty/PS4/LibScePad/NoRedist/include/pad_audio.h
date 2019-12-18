/* SIE CONFIDENTIAL
 * Pad Library for PC Games 2.0
 * Copyright (C) 2016 Sony Interactive Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef _SCE_PAD_AUDIO_H_
#define _SCE_PAD_AUDIO_H_

/*******************************************************************************/
/*E Type Definitions */
/*******************************************************************************/

/**
 *E  
 *   @brief Return value of scePadIsSupportedAudioFunction()
 *          if Connected Pad supports audio function.
 **/
#define SCE_PAD_SUPPORTED_AUDIO_FUNCTION 1

/**
 * @brief Definition for setting audio path
 *
 * This definition is used for specifying output audio path.
 */
typedef enum ScePadAudioOutPath {
	SCE_PAD_AUDIO_OUT_PATH_STEREO_HEADSET,          /* Stereo Headphone L,R channel  */
	SCE_PAD_AUDIO_OUT_PATH_MONO_HEADSET,            /* Mono Headphone   L channel only */
	SCE_PAD_AUDIO_OUT_PATH_MONO_HEADSET_SPEAKER,    /* Mono Headphone and Speaker    */
	SCE_PAD_AUDIO_OUT_PATH_SPEAKER,                 /* Speaker only  */
	SCE_PAD_AUDIO_OUT_PATH_OFF,                     /* Not output    */
} ScePadAudioOutPath;

/**
 *E  
 *   @brief Maximum volume gain by scePadSetVolumeGain function.
 **/
#define SCE_PAD_MAX_VOLUME_GAIN	127

/**
 * @brief Structure for setting volume/mic gain
 *
 * This structure is used for specifying each volumes/mic gain.
 */
typedef struct ScePadVolumeGain {
	uint8_t speakerVol;     /* Speaker volume: 0 ... 127 or 0xff (unspecified) */
	uint8_t jackVol;        /* HP Jack volume: 0 ... 127 or 0xff (unspecified) */
	uint8_t reserved;
	uint8_t micGain;        /* Mic gain      : 0 ... 127 or 0xff (unspecified) */
} ScePadVolumeGain;

/**
 * @brief Definition for setting volume/mic gain
 *
 * This definition is used for specifying each volumes/mic gain.
 */
typedef enum ScePadHeadsetType {
	SCE_PAD_HEADSET_TYPE_NONE,      /* not present                */
	SCE_PAD_HEADSET_TYPE_HEADPHONE, /* Headphone (without Mic)    */
	SCE_PAD_HEADSET_TYPE_HEADSET,   /* Headset (with Mic)         */
} ScePadHeadsetType;

/**
 * @brief Structure for audio jack status
 *
 * This structure is used for getting audio jack status.
 */
typedef struct ScePadJackState {
	ScePadHeadsetType headsetState;
	uint8_t reserve[12];
} ScePadJackState;


/*******************************************************************************/
/*E Functions */
/*******************************************************************************/

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/**
 *E  
 *  @brief Get whether the conncted Pad supports audio function.
 *
 *  @param [in]  handle : handle to access controller.
 *  
 *  @retval (== SCE_PAD_SUPPORTED_AUDIO_FUNCTION)    : connected Pad supports audio function.
 *          (== 0)                                   : connected Pad doesn't support audio function.
 *          (< 0)                                    : error codes
 *
 **/
int scePadIsSupportedAudioFunction(int32_t handle);

/**
 *E  
 *  @brief Specify the audio output path in the Pad.
 *
 *  @param [in]  handle : handle to access controller.
 *  @param [in]  path   : the parameter of the audio path.
 *  
 *  @retval (== SCE_OK) : success
 *          (< 0)       : error codes
 *
 **/
int scePadSetAudioOutPath(int32_t handle, ScePadAudioOutPath path);

/**
 *E  
 *  @brief Set the volume/gain on the Pad.
 *
 *  @param [in]  handle : handle to access controller.
 *  @param [in]  pGain  : the parameter of the volume/gain.
 *  
 *  @retval (== SCE_OK) : success
 *          (< 0)       : error codes
 *
 **/
int scePadSetVolumeGain(int32_t handle, ScePadVolumeGain *pGain);

/**
 *E  
 *  @brief Get the audio jack status on the Pad.
 *
 *  @param [in]  handle : handle to access controller.
 *  @param [out] pState : the status of the audio jack.
 *  
 *  @retval (== SCE_OK) : success
 *          (< 0)       : error codes
 *
 **/
int scePadGetJackState(int32_t handle, ScePadJackState *pState);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif /* _SCE_PAD_AUDIO_H_ */
