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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Wireless LAN library utility funtions.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiRegHelper_
#define _DEFINED_WiFiRegHelper_
#pragma once

#include "windows.h"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a simple mechanism for capturing error-message output.
// Once one of these objects is created all messages sent to LogError
// by the current thread will be stored in the specified buffer.
//
class WiFiRegHelper
{
private:
    HKEY   m_RootKey;
    TCHAR  m_RootKeyName[MAX_PATH];
    HKEY   m_SubKey;
    TCHAR  m_SubKeyName[MAX_PATH];
    BOOL   m_DeleteKey;

public:
    
    WiFiRegHelper(void)
        : m_RootKey(NULL),
          m_SubKey(NULL),
          m_DeleteKey(FALSE)
    {
        m_RootKeyName[0] = 0;
        m_SubKeyName[0] = 0;
    }
    
   ~WiFiRegHelper(void);

    BOOL 
    Init(
        const TCHAR *pSubKeyName,
        BOOL         fDeleteKey = FALSE,
        HKEY         hRootKey   = HKEY_LOCAL_MACHINE,
        const TCHAR *pRootKeyName = TEXT("HKLM"));

    void 
    Cleanup(void);

    BOOL 
    OpenKey(
        DWORD *pErrCode = NULL);

    BOOL 
    CreateKey(
        DWORD *pErrCode = NULL);

    void 
    CloseKey(
        void);

    void 
    DeleteKey(
        DWORD *pErrCode = NULL);

    BOOL 
    DeleteKeyValue(
        const TCHAR *pValueName,
	    DWORD       *pErrCode = NULL);

    BOOL 
    ReadDwordValue(
        const TCHAR *pValueName,
        DWORD       *pDwordValue,
        DWORD       *pErrCode = NULL,
        DWORD        DefaultValue = (DWORD)-1);

    BOOL 
    SetDwordValue(
        const TCHAR *pValueName,
        DWORD        dwordValue,
        DWORD       *pErrCode = NULL);

    BOOL 
    ReadStrValue(
        const TCHAR *pValueName,
        char        *pStrValue,
        LONG        *pStrChars,
        DWORD       *pErrCode = NULL,
        const TCHAR *pDefaultValue = NULL);
    BOOL 
    ReadStrValue(
        const TCHAR *pValueName,
        WCHAR       *pStrValue,
        LONG        *pStrChars,
        DWORD       *pErrCode = NULL,
        const TCHAR *pDefaultValue = NULL);
    
    BOOL 
    SetStrValue(
        const TCHAR *pValueName,
        const char  *pStrValue,
        DWORD       *pErrCode = NULL);
    BOOL 
    SetStrValue(
        const TCHAR *pValueName,
        const WCHAR *pStrValue,
        DWORD       *pErrCode = NULL);

    BOOL 
    ReadBlobValue(
        const TCHAR *pValueName,
        BYTE        *pblobValue,
        LONG        *pblobSize,
        DWORD       *pErrCode = NULL,
        const BYTE  *pDefaultValue = NULL);

    BOOL 
    SetBlobValue(
        const TCHAR *pValueName,
        const BYTE  *pblobValue,
        LONG         blobSize,
        DWORD       *pErrCode = NULL);
	
};

};
};
#endif /* _DEFINED_WiFiRegHelper_ */
// ----------------------------------------------------------------------------

