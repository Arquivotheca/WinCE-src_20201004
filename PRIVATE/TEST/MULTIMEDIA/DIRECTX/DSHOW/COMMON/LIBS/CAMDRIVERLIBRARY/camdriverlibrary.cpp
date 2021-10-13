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
#include "CamDriverLibrary.h"

CCamDriverLibrary::CCamDriverLibrary()
{
    CDriverEnumerator::CDriverEnumerator();
    m_nNullDriverIndex = -1;
}

CCamDriverLibrary::~CCamDriverLibrary()
{
}

HRESULT
CCamDriverLibrary::SetupNULLCameraDriver()
{
    HRESULT hr = S_FALSE;
    HKEY hkeyFillLibrary = NULL;
    DWORD dwDataType;
    TCHAR tszPrefix[MAX_DEVCLASS_NAMELEN];
    DWORD dwDataSize = sizeof(tszPrefix);
    DWORD dwIndex;
    DWORD dwIndexSize = sizeof(DWORD);
    m_nNullDriverIndex = -1;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("Drivers\\BuiltIn\\SampleCam"), 
                            0, // Reseved. Must == 0.
                            0, // Must == 0.
                            &hkeyFillLibrary) == ERROR_SUCCESS)
    {
        if(RegQueryValueEx(hkeyFillLibrary,
                                TEXT("Prefix"),
                                NULL,
                                &dwDataType,
                                (LPBYTE) tszPrefix,
                                &dwDataSize) == ERROR_SUCCESS)
        {
            if(RegQueryValueEx(hkeyFillLibrary,
                                    TEXT("Index"),
                                    NULL,
                                    &dwDataType,
                                    (LPBYTE) &dwIndex,
                                    &dwIndexSize) == ERROR_SUCCESS)
            {
                // combine the prefix and the index to create the device name
                _sntprintf(tszPrefix, dwDataSize, TEXT("%s%d:"), tszPrefix, dwIndex);

                // find the device from the enumeration and set the null driver index
                m_nNullDriverIndex = GetDeviceIndex(tszPrefix);

                if(m_nNullDriverIndex != -1)
                    hr = S_OK;
            }
        }
        RegCloseKey(hkeyFillLibrary);
    }

    return hr;
}

int
CCamDriverLibrary::GetNULLCameraDriverIndex()
{
    return m_nNullDriverIndex;
}


HRESULT
CCamDriverLibrary::AddNULLCameraRegKey(TCHAR *tszKey, TCHAR *tszData)
{
    HRESULT hr = E_FAIL;
    HKEY hKeyDriver = NULL;
    DWORD dwDisposition;

    if(tszKey && tszData)
    {
        // if we can open the key, then set up the filler function.
        //  if we can't then we're not using the NULL driver.
        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("Drivers\\Capture\\SampleCam"), 
                            0, // Reseved. Must == 0.
                            L"",
                            REG_OPTION_VOLATILE, // Must == 0.
                            0,
                            NULL,
                            &hKeyDriver,
                            &dwDisposition) == ERROR_SUCCESS)
        {
            if(RegSetValueEx(hKeyDriver,
                                        tszKey,
                                        0,
                                        REG_SZ,
                                        (BYTE *) tszData, 
                                        _tcslen(tszData) * sizeof(TCHAR)) == ERROR_SUCCESS)
            {
                hr = S_OK;
            }
            RegCloseKey(hKeyDriver);
        }
    }

    return hr;
}

HRESULT
CCamDriverLibrary::AddNULLCameraRegKey(TCHAR *tszKey, DWORD dwData)
{
    HRESULT hr = E_FAIL;
    HKEY hKeyDriver = NULL;
    DWORD dwDisposition;

    if(tszKey)
    {
        // if we can open the key, then set up the filler function.
        //  if we can't then we're not using the NULL driver.
        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("Drivers\\Capture\\SampleCam"), 
                            0, // Reseved. Must == 0.
                            L"",
                            REG_OPTION_VOLATILE, // Must == 0.
                            0,
                            NULL,
                            &hKeyDriver,
                            &dwDisposition) == ERROR_SUCCESS)
    {
            if(RegSetValueEx(hKeyDriver,
                                        tszKey,
                                        0,
                                        REG_DWORD,
                                        (BYTE *) &dwData, 
                                        sizeof(DWORD)) == ERROR_SUCCESS)
            {
                hr = S_OK;
            }
            RegCloseKey(hKeyDriver);
        }
    }

    return hr;
}

