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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __UTILS_H
#define __UTILS_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

HRESULT Open();
HRESULT Close();

HRESULT StartIoControl(
   DWORD dwCode, PVOID pvInpBuffer, DWORD cbInpBuffer, PVOID pvOutBuffer, 
   DWORD cbOutBuffer, DWORD *pcbOutBuffer, PVOID* ppvOverlapped
);
HRESULT StopIoControl(PVOID* ppvOverlapped);
HRESULT WaitForIoControl(PVOID* ppvOverlapped, DWORD dwTimeout);

HRESULT LoadAdapter(LPCTSTR szMiniport);
HRESULT UnloadAdapter(LPCTSTR szMiniport);

HRESULT QueryAdapters(LPTSTR mszAdapters, DWORD dwSize);
HRESULT QueryProtocols(LPTSTR mszProtocols, DWORD dwSize);
HRESULT QueryBindings(LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize);

HRESULT UnbindProtocol(LPCTSTR szMiniport, LPCTSTR szProtocol);
HRESULT BindProtocol(LPCTSTR szMiniport, LPCTSTR szProtocol);

HRESULT WriteVerifyFlag(LPCTSTR szMiniport, DWORD dwFlag);
HRESULT DeleteVerifyFlag(LPCTSTR szMiniport);

//------------------------------------------------------------------------------

LPTSTR lstrdup(LPCTSTR sz);
LPTSTR lstrchr(LPCTSTR sz, TCHAR c);
INT lstrcmpX(LPCTSTR sz1, LPCTSTR sz2);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
