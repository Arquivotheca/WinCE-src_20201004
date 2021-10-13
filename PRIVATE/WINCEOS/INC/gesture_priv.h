//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#define GF_POSTED_QUEUE     16              // Internal flag to direct posted Gesture to 
                                            // the posted queue rather than the input queue

struct GESTUREINFODATA
{
    UINT cbData;
    GESTUREINFO gestureInfo;
    BYTE rgExtraArguments[1];       // Note - variable length structure
};

#define MAX_GESTURE_EXTRA_ARGS_CB   1024
#define MIN_GESTUREINFODATA_SIZE_CB ((UINT)(FIELD_OFFSET(GESTUREINFODATA, rgExtraArguments[0])))
#define MAX_GESTUREINFODATA_SIZE_CB ((UINT)(MIN_GESTUREINFODATA_SIZE_CB + MAX_GESTURE_EXTRA_ARGS_CB))

#define REG_szTouchGestureSettingsRootKey   (HKEY_CURRENT_USER)
#define REG_szTouchGestureSettingsRegKey    TEXT("ControlPanel\\Gestures\\Touch")
#define REG_szTouchGestureTimeoutValue      TEXT("Timeout")
#define REG_szTouchGestureToleranceValue    TEXT("Tolerance")
#define REG_szTouchGestureLengthValue       TEXT("Length")
#define REG_szTouchGestureFlickMultiplier   TEXT("Multiplier")

/*
 * Gesture SPI settings
 */
#define SPI_GETGESTUREMETRICS       265
#define SPI_SETGESTUREMETRICS       266

typedef struct tagGESTUREMETRICS {
    UINT   cbSize; 
    DWORD  dwID;
    DWORD  dwTimeout; 
    DWORD  dwDistanceTolerance; 
    DWORD  dwAngularTolerance;
    DWORD  dwExtraInfo;
} GESTUREMETRICS, *LPGESTUREMETRICS; 

