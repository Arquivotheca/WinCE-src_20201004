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

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define MM_WOM_SETSECONDARYGAINCLASS   (WM_USER)
#define MM_WOM_SETSECONDARYGAINLIMIT   (WM_USER+1)

// send sound through speaker if dw1 is TRUE, send through default if FALSE
#define MM_WOM_FORCESPEAKER   (WM_USER+2)

#ifdef __cplusplus
}
#endif // __cplusplus

