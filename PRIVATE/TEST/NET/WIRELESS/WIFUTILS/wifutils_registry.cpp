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
// Implementation of the WiFUtils class.
//
// ----------------------------------------------------------------------------

#include "WiFUtils.hpp"

#include <assert.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Loads an optional string value from the registry.
//
HRESULT
WiFUtils::
ReadRegString(
    IN  HKEY         HKey, 
    IN  const TCHAR *pConfigKey,
    IN  const TCHAR *pValueKey, 
    OUT ce::string  *pData, 
    IN  const TCHAR *pDefaultValue)
{
    ce::wstring wcBuffer;
    HRESULT hr = WiFUtils::ReadRegString(HKey, pConfigKey, pValueKey,
                                        &wcBuffer, pDefaultValue);
    if (SUCCEEDED(hr))
    {
        hr = WiFUtils::ConvertString(pData, wcBuffer);
        if (FAILED(hr))
        {
            LogError(TEXT("Error converting %s\\%s \"%.128ls\" to ASCII: %s"),
                     pValueKey, pConfigKey, &wcBuffer[0], HRESULTErrorText(hr));
        }
    }
    return hr;
}

HRESULT
WiFUtils::
ReadRegString(
    IN  HKEY         HKey, 
    IN  const TCHAR *pConfigKey,
    IN  const TCHAR *pValueKey, 
    OUT ce::wstring *pData, 
    IN  const TCHAR *pDefaultValue)
{
    DWORD result = ERROR_NOT_FOUND;

    if (HKey)
    {
        result = ce::RegQueryValue(HKey, pValueKey, *pData);
    }
    
    switch (result)
    {        
        case ERROR_SUCCESS:
#ifdef EXTRA_DEBUG
            LogDebug(TEXT("    setting %s to \"%ls\""),
                     pValueKey, &(*pData)[0]);
#endif
            return S_OK;
            
        case ERROR_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
            if (NULL != pDefaultValue)
            {
#ifdef EXTRA_DEBUG
                LogDebug(TEXT("    defaulting %s to \"%s\""), 
                         pValueKey, pDefaultValue);
#endif
                HRESULT hr = WiFUtils::ConvertString(pData, pDefaultValue);
                if (FAILED(hr))
                {
                    LogError(TEXT("Error converting %s\\%s")
                             TEXT(" default value \"%.128s\" to Unicode: %s"),
                             pValueKey, pConfigKey, pDefaultValue, 
                             HRESULTErrorText(hr));
                    return hr;
                }
                return ERROR_SUCCESS;
            }
            break;
    }
    
    LogError(TEXT("Cannot read reg value \"%s\\%s\": %s"),
             pConfigKey, pValueKey, Win32ErrorText(result));
    return HRESULT_FROM_WIN32(result);
}

// ----------------------------------------------------------------------------
//
// Loads an optional integer value from the registry.
//
HRESULT
WiFUtils::
ReadRegDword(
    IN  HKEY         HKey,
    IN  const TCHAR *pConfigKey,
    IN  const TCHAR *pValueKey,
    OUT DWORD       *pData,
    IN  DWORD        DefaultValue)
{
    DWORD dataSize = sizeof(DWORD);
    DWORD dataType = 0;
    DWORD result = ERROR_NOT_FOUND;

    if (HKey)
    {
        result = RegQueryValueEx(HKey, pValueKey, NULL, &dataType,
                                         (LPBYTE)pData, &dataSize);
    }
    switch (result)
    {
        case ERROR_SUCCESS:
            if (dataType == REG_DWORD && dataSize == sizeof(DWORD))
            {
#ifdef EXTRA_DEBUG
                LogDebug(TEXT("    setting %s to %u"), pValueKey, *pData);
#endif
                return S_OK;
            }
            else
            {
                LogError(TEXT("Bad data-type %d for \"%s\\%s\""),
                         dataType, pConfigKey, pValueKey);
                return E_INVALIDARG;
            }

        case ERROR_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
           *pData = DefaultValue;
#ifdef EXTRA_DEBUG
            LogDebug(TEXT("    defaulting %s to %u"),
                     pValueKey, *pData);
#endif
            return S_OK;
    }

    LogError(TEXT("Cannot read reg value \"%s\\%s\": %s"),
             pConfigKey, pValueKey, Win32ErrorText(result));
    return HRESULT_FROM_WIN32(result);
}

// ----------------------------------------------------------------------------
// 
// Writes the specified string value into the registry.
//
HRESULT
WiFUtils::
WriteRegString(
    IN HKEY              HKey, 
    IN const TCHAR      *pConfigKey,
    IN const TCHAR      *pValueKey,
    IN const ce::string &data)
{
    ce::wstring wcBuffer;
    HRESULT hr = WiFUtils::ConvertString(&wcBuffer, data, NULL,
                                                    data.length());
    if (FAILED(hr))
    {
        LogError(TEXT("Error converting %s\\%s \"%.128hs\" to Unicode: %s"),
                 pValueKey, pConfigKey, &data[0], HRESULTErrorText(hr));
    }
    else
    {
        hr = WriteRegString(HKey, pConfigKey, pValueKey, wcBuffer);
    }
    return hr;
}

HRESULT
WiFUtils::
WriteRegString(
    IN HKEY               HKey, 
    IN const TCHAR       *pConfigKey,
    IN const TCHAR       *pValueKey,
    IN const ce::wstring &data)
{
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("  updating %s to \"%ls\""), pValueKey, &data[0]);
#endif
    
    int length = (data.length() + 1) * sizeof(WCHAR);
    DWORD result = RegSetValueEx(HKey, pValueKey, 0, REG_SZ,
                                (const BYTE *)(&data[0]), length);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot update reg value \"%s\\%s\": %s"),
                 pConfigKey, pValueKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }
    
    return S_OK;
}

// ----------------------------------------------------------------------------
//
// Writes the specified integer value into the registry.
//
HRESULT
WiFUtils::
WriteRegDword(
    IN HKEY         HKey,
    IN const TCHAR *pConfigKey,
    IN const TCHAR *pValueKey,
    IN DWORD        data)
{
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("  updating %s to %u"), pValueKey, data);
#endif

    DWORD result = RegSetValueEx(HKey, pValueKey, 0, REG_DWORD,
                                (const BYTE *)&data, sizeof(DWORD));
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot update reg value \"%s\\%s\": %s"),
                 pConfigKey, pValueKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
