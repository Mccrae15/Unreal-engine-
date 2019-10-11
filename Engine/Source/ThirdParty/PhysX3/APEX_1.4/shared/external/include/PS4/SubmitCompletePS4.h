/*
 * Copyright (c) 2008-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef SUBMITCOMPLETEEPS4_H
#define SUBMITCOMPLETEEPS4_H

#include <gnm/platform.h>

//https://ps4.scedev.net/technotes/view/98/1
//Must be called every frame (at least every 5 seconds) even if nothing submitted.
//Call this after fetchResults to avoid triggering an exception.
#define SUBMIT_COMPLETE sce::Gnm::submitDone();

#endif //SUBMITCOMPLETEEPS4_H
