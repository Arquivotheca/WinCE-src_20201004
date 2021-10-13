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

/*++


Module Name:

    kdioctl.c

Abstract:

    This module implements the IOControl (IOCTL) function exported by KD

Environment:

    WinCE

--*/

#include "kdp.h"
#include "kdfilter.h"

BOOL 
IOControl(
    DWORD   dwInstData,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned)
{
   BOOL fRequestSucceeded = FALSE;

   switch (dwIoControlCode)
   {
       case IOCTL_DBG_FEX_OFF:
       case IOCTL_DBG_FEX_ON:
       case IOCTL_DBG_FEX_GET_STATUS:
       case IOCTL_DBG_FEX_GET_MAX_RULES:
       case IOCTL_DBG_FEX_GET_RELEVANT_RULE_COUNT:
       case IOCTL_DBG_FEX_ADD_RULE:
       case IOCTL_DBG_FEX_REMOVE_RULE:
       case IOCTL_DBG_FEX_GET_RULE_INFO:
         fRequestSucceeded = KDFex_IOControl(dwInstData, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
         break;

       default:
         fRequestSucceeded = FALSE;
         break;
   }

   return fRequestSucceeded;
}

