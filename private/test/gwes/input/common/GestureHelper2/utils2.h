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

#ifndef __UTILS2_H__
#define __UTILS2_H__

#include <testdefs.h>

void WINAPIV DebugDump(const wchar_t* szFmt, ...);
TCHAR *GetCoreGestureName(DWORD dwGestureID);
TCHAR *GetGestureFlagsName(DWORD dwFlags);

HRESULT SetGestureMetrics(DWORD dwGestureInfo, DWORD dwDistance, DWORD dwTimeout, DWORD dwWindowInit = 0);
HRESULT GetGestureMetrics(DWORD dwGestureInfo, DWORD &dwDistance, DWORD &dwTimeout);
HRESULT EnableGestures_Helper(HWND hwnd, ULONGLONG ullTGFGestures, DWORD dwScope, ULONGLONG *pPreviousState);
HRESULT DisableGestures_Helper(HWND hwnd, ULONGLONG ullTGFGestures, DWORD dwScope, ULONGLONG *pPreviousState);
HRESULT RestoreInitialGesturesState(HWND hwnd, DWORD dwScope, ULONGLONG ullInitialState);
BOOL    TGFHasGesture(ULONGLONG ullTGFValue, DWORD dwGestureID);
TCHAR*  GetTGFToGestureCombinationString(ULONGLONG ullTGFCombo);
TCHAR*  GetGestureScopeName(DWORD dwScope);

void info(infoType iType, LPCTSTR szFormat, ...);
TESTPROCAPI getCode(void);


class CImpersonateAutoRevert
{
public:
    // Impersonate hToken
    CImpersonateAutoRevert(HANDLE hToken);
    // Create a token from pszAccountName, then impersonate
    CImpersonateAutoRevert(LPCWSTR pszAccountName);
    // Revert on destructor
    ~CImpersonateAutoRevert();
    
    BOOL Revert();

    static TCHAR* GetAccountFriendlyName(const TCHAR *szAccountSID);

private:
    BOOL m_fImpersonated;
};

#endif //__UTILS_H__