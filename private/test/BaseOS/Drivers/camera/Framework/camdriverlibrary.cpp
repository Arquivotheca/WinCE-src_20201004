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
#include "CamDriverLibrary.h"

CCamDriverLibrary::CCamDriverLibrary()
{
    CDriverEnumerator::CDriverEnumerator();
    m_nTestDriverIndex = -1;
    m_hTestDriverHandle = INVALID_HANDLE_VALUE;
}

CCamDriverLibrary::~CCamDriverLibrary()
{
    Cleanup();
}


HRESULT
CCamDriverLibrary::SetupTestCameraDriver(TCHAR *tszXML, TCHAR *tszProfile)
{
    HRESULT hr = S_OK;

    // create test camera driver registry keys for activation
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("Prefix"), TESTDRIVER_PREFIX);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("Dll"), TESTDRIVER_DLL);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("Order"), TESTDRIVER_ORDER);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("Index"), TESTDRIVER_INDEX);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("IClass"), TESTDRIVER_ICLASS);
    if(FAILED(hr)) goto cleanup;

    hr = AddTestCameraRegKey(TESTDRIVER_PIN_PATH, TEXT("Prefix"), TESTDRIVER_PIN_PREFIX);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PIN_PATH, TEXT("Dll"), TESTDRIVER_DLL);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PIN_PATH, TEXT("Order"), TESTDRIVER_PIN_ORDER);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PIN_PATH, TEXT("Index"), TESTDRIVER_PIN_INDEX);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PIN_PATH, TEXT("IClass"), TESTDRIVER_PIN_ICLASS);
    if(FAILED(hr)) goto cleanup;

    // setup the config and profile registry keys
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("XMLConfig"), tszXML);
    if(FAILED(hr)) goto cleanup;
    hr = AddTestCameraRegKey(TESTDRIVER_PATH, TEXT("XMLProfile"), tszProfile);
    if(FAILED(hr)) goto cleanup;

    // activate test camera driver
    m_hTestDriverHandle = ActivateDevice(TESTDRIVER_PATH, NULL);
    if(INVALID_HANDLE_VALUE == m_hTestDriverHandle || NULL == m_hTestDriverHandle)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

cleanup:
    return hr;
}

HRESULT
CCamDriverLibrary::Init()
{
    HRESULT hr = CDriverEnumerator::Init(&CLSID_CameraDriver);

    if(SUCCEEDED(hr))
    {
        HRESULT hr = S_FALSE;
        HKEY hkeyFillLibrary = NULL;
        DWORD dwDataType;
        TCHAR tszPrefix[MAX_DEVCLASS_NAMELEN];
        DWORD dwDataSize = sizeof(tszPrefix);
        DWORD dwIndex;
        DWORD dwIndexSize = sizeof(DWORD);
        m_nTestDriverIndex = -1;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                TESTDRIVER_PATH, 
                                0, // Reseved. Must == 0.
                                0, // Must == 0.
                                &hkeyFillLibrary) == ERROR_SUCCESS)
        {
            if(RegQueryValueExW(hkeyFillLibrary,
                                    TEXT("Prefix"),
                                    NULL,
                                    &dwDataType,
                                    (LPBYTE) tszPrefix,
                                    &dwDataSize) == ERROR_SUCCESS)
            {
                if(RegQueryValueExW(hkeyFillLibrary,
                                        TEXT("Index"),
                                        NULL,
                                        &dwDataType,
                                        (LPBYTE) &dwIndex,
                                        &dwIndexSize) == ERROR_SUCCESS)
                {
                    // combine the prefix and the index to create the device name
                    _sntprintf(tszPrefix, dwDataSize, TEXT("%s%d:"), tszPrefix, dwIndex);

                    // find the device from the enumeration and set the null driver index
                    m_nTestDriverIndex = GetDeviceIndex(tszPrefix);

                    if(m_nTestDriverIndex != -1)
                        hr = S_OK;
                }
            }
            RegCloseKey(hkeyFillLibrary);
        }
    }

    return hr;
}

int
CCamDriverLibrary::GetTestCameraDriverIndex()
{
    return m_nTestDriverIndex;
}

HRESULT
CCamDriverLibrary::AddTestCameraRegKey(TCHAR *tszPath, TCHAR *tszKey, TCHAR *tszData)
{
    HRESULT hr = E_FAIL;
    HKEY hKeyDriver = NULL;
    DWORD dwDisposition;

    if(tszKey && tszData)
    {
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                            tszPath, 
                            0, // Reseved. Must == 0.
                            L"",
                            REG_OPTION_VOLATILE, // Must == 0.
                            0,
                            NULL,
                            &hKeyDriver,
                            &dwDisposition) == ERROR_SUCCESS)
        {
            if(RegSetValueExW(hKeyDriver,
                                        tszKey,
                                        0,
                                        REG_SZ,
                                        (BYTE *) tszData, 
                                        (_tcslen(tszData) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS)
            {
                hr = S_OK;
            }
            RegCloseKey(hKeyDriver);
        }
    }

    return hr;
}

HRESULT
CCamDriverLibrary::AddTestCameraRegKey(TCHAR *tszPath, TCHAR *tszKey, DWORD dwData)
{
    HRESULT hr = E_FAIL;
    HKEY hKeyDriver = NULL;
    DWORD dwDisposition;

    if(tszKey)
    {
        // if we can open the key, then set up the filler function.
        //  if we can't then we're not using the NULL driver.
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                            tszPath, 
                            0, // Reseved. Must == 0.
                            L"",
                            REG_OPTION_VOLATILE, // Must == 0.
                            0,
                            NULL,
                            &hKeyDriver,
                            &dwDisposition) == ERROR_SUCCESS)
        {
            if(RegSetValueExW(hKeyDriver,
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

HRESULT
CCamDriverLibrary::Cleanup()
{
    HRESULT hr = S_OK;
    
    if(INVALID_HANDLE_VALUE != m_hTestDriverHandle && NULL != m_hTestDriverHandle)
    {
        // deactivate test camera driver
        if(DeactivateDevice(m_hTestDriverHandle))
        {

            m_hTestDriverHandle = INVALID_HANDLE_VALUE;

            // cleanup test camera registry keys
            hr = HRESULT_FROM_WIN32(RegDeleteKey(HKEY_LOCAL_MACHINE, 
                                                                        TESTDRIVER_PATH));

            // cleanup test camera registry keys
            hr = HRESULT_FROM_WIN32(RegDeleteKey(HKEY_LOCAL_MACHINE, 
                                                                        TESTDRIVER_PIN_PATH));
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    return hr;
}

