//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

touchgesture.h

Abstract:  

    Private entry points, defines and types for the Windows Mobile Gesture API.
    
Notes: 


--*/

#ifndef _TOUCHGESTURE_
#define _TOUCHGESTURE_

#ifdef __cplusplus
extern "C"
{
#endif

typedef HRESULT (*PFN_SETTOUCHRECOGNIZERTIMER)(DWORD* pdwTimerId, DWORD dwElapse, TIMERPROC pfnTimerProc);
typedef HRESULT (*PFN_KILLTOUCHRECOGNIZERTIMER)(DWORD dwTimerId);

typedef struct tagTOUCHRECOGNIZERAPIINIT {
    PFN_SETTOUCHRECOGNIZERTIMER  pfnSetTimer;
    PFN_KILLTOUCHRECOGNIZERTIMER pfnKillTimer;
} TOUCHRECOGNIZERAPIINIT;

#ifdef __cplusplus
} // extern "C"
#endif

#endif  //  _TOUCHGESTURE_

