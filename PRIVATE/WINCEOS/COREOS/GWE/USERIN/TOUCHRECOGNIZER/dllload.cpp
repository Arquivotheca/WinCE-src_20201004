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

#include <delayld.h>
#include <ehm.h>

/////////////////////////////////////////////////////////////////////////////
// OSSVCS.DLL
/////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
HINSTANCE g_hinstOSSVCS = NULL;

// HRESULT BOOL OnAssertionFail(eCodeType eType, DWORD dwCode, const TCHAR* pszFile, unsigned long ulLine, const TCHAR* pszMessage);
DELAY_LOAD_ORD(g_hinstOSSVCS, OSSVCS, BOOL, OnAssertionFail, 27,
    (eCodeType eType, DWORD dwCode, const TCHAR* pszFile, unsigned long ulLine, const TCHAR* pszMessage),
    (eType, dwCode, pszFile, ulLine, pszMessage))
#endif

