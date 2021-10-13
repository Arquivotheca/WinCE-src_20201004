//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//
//--------------------------------------------------------------------------;

#ifndef __ERRORS__
#define __ERRORS__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef _AMOVIE_
#define AMOVIEAPI   DECLSPEC_IMPORT
#else
#define AMOVIEAPI
#endif

// codes 0-01ff are reserved for OLE
#define VFW_FIRST_CODE   0x200
#define MAX_ERROR_TEXT_LEN 160

#include <VFWMSGS.H>                    // includes all message definitions

#ifdef UNICODE
typedef BOOL (WINAPI* AMGETERRORTEXTPROCW)(HRESULT, WCHAR *, DWORD);
AMOVIEAPI DWORD WINAPI AMGetErrorTextW( HRESULT hr , WCHAR *pbuffer , DWORD MaxLen);
#define AMGetErrorText  AMGetErrorTextW
typedef AMGETERRORTEXTPROCW AMGETERRORTEXTPROC;
#else
typedef BOOL (WINAPI* AMGETERRORTEXTPROCA)(HRESULT, char *, DWORD);
AMOVIEAPI DWORD WINAPI AMGetErrorTextA( HRESULT hr , char *pbuffer , DWORD MaxLen);
#define AMGetErrorText  AMGetErrorTextA
typedef AMGETERRORTEXTPROCA AMGETERRORTEXTPROC;
#endif

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __ERRORS__
