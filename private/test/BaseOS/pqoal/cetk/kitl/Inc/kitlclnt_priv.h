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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 1995-2000 Microsoft Corporation

Module Name:   KitlClnt_priv.h

Abstract:  
    This contains declarations the KITL client routines.
Functions:


Notes: 
    This is a copy of the kitlclnt.h file found in the Tools tree at:
        %_WINCEROOT%\Tools\platman\source\kitl\inc

--*/
#ifndef _KITLCLNT_H_
#define _KITLCLNT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef UINT KITLID;
#define INVALID_KITLID  ((KITLID) -1)

//////////////////////////////////
// global initializations
//
BOOL KITLInitLibrary (LPCWSTR pszRegKeyRoot);
BOOL KITLDeInitLibrary (void);

//////////////////////////////////
// KITL client functions
//

// client registration/de-registration
KITLID KITLRegisterClient (LPCWSTR pszDevName, LPCWSTR pszSvcName, DWORD dwTimeout, DWORD dwFlags);
BOOL KITLDeRegisterClient (KITLID id);

// send/recv data
BOOL KITLSend (KITLID id, LPCVOID pData, DWORD cbData, DWORD dwTimeout);
BOOL KITLRecv (KITLID id, LPVOID pBuffer, DWORD *pcbBuffer, DWORD dwTimeout);

// config related
BOOL KITLWaitForSvcConnect (KITLID id, DWORD dwTimeout);

// data availability
BOOL KITLIsDataAvailable (KITLID id, DWORD *pcbBuffer);

#ifdef __cplusplus
}
#endif

#endif _KITLCLNT_H_
