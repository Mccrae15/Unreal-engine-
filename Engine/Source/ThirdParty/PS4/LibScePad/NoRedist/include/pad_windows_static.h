/* SIE CONFIDENTIAL
 * Pad Library for PC Games 2.0
 * Copyright (C) 2016 Sony Interactive Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef _SCE_PAD_WINDOWS_STATIC_H
#define _SCE_PAD_WINDOWS_STATIC_H

/*******************************************************************************/
/*E Functions */
/*******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 *E  
 *   @brief Structure for initialization.
 *
 **/
typedef struct ScePadInit2Param{
	void* (*allocFunc)(size_t memorySize); /*E Function pointer of memory allocation function  */
	void (*freeFunc)(void* pMemory);  /*E Function pointer of memory free function        */
} ScePadInit2Param;

/**
 *E
 *  @brief Initialize function
 *
 *  This function initialize the libpad.
 *  When you use unique memory allocation/free function, please call this function instead of scePadInit
 *  
 *  @param [in]  param : It is a parameter for memory allocation/free function.
 *                       Please specify function pointer of memory allocation/free function.
 *
 *  @retval (==SCE_OK) : success
 *          (< SCE_OK) : error codes
 **/
int scePadInit2(ScePadInit2Param* pParam);

/**
 *E  
 *  @brief Terminate function
 *
 *  This function terminate the libpad.
 *  When finish your application, please call this function.
 *
 *  @retval none
 *
 **/
void scePadTerminate(void);


#ifdef __cplusplus
}
#endif


#endif	/* _SCE_PAD_WINDOWS_STATIC_H */
