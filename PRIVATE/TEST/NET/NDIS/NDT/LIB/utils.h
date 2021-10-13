//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
