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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------

#include "tuxmain.h"
#include "debug.h"
#include <svsutil.hxx>
#include <strsafe.h>

extern CKato *g_pKato;

HANDLE hBklDevice = NULL;

HANDLE GetDeviceHandleFromGUID(LPCTSTR pszBusGuid)
{
    HANDLE hDevice = NULL;

    union {
        BYTE rgbGuidBuffer[sizeof(GUID) + 4]; // +4 since scanf writes longs
        GUID guidBus;
    } u = { 0 };
    LPGUID pguidBus = &u.guidBus;

    // Parse the GUID
    swscanf_s(pszBusGuid, SVSUTIL_GUID_FORMAT, SVSUTIL_PGUID_ELEMENTS(&pguidBus));

    // Get a handle to the bkl driver
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    if (FindFirstDevice(DeviceSearchByGuid, pguidBus, &di) != INVALID_HANDLE_VALUE) {
        hDevice = CreateFile(di.szLegacyName, 0, 0, NULL, 0, 0, NULL);
    }
    else {
        NKDbgPrintfW(_T("%s device not found!\r\n"), pszBusGuid);
    }

    return hDevice;
}

HRESULT SuiteSetup(void)
{
    HRESULT hr = S_OK;
    //Get Backlight Device Handle
    hBklDevice = GetDeviceHandleFromGUID(L"0007AE3D-413C-4e7e-8786-E2A696E73A6E");
    if (!hBklDevice || hBklDevice == INVALID_HANDLE_VALUE)
        hr = E_FAIL;

    return hr;
}

HRESULT SuiteTeardown(void)
{
    HRESULT hr = S_OK;
    if (hBklDevice)
        CloseHandle(hBklDevice);
    return hr;
}

//*****************************************************************************
TESTPROCAPI GetBklBrightness( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Functional/API Test: This tests the IOCTL_BKL_GET_BRIGHTNESS command.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;

    DWORD dwBrightness    = 0;
    DWORD dwBytesReturned = 0;

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        BOOL fRet = DeviceIoControl(hBklDevice,
                        IOCTL_BKL_GET_BRIGHTNESS,
                        NULL,
                        0,
                        (LPVOID) &dwBrightness,
                        sizeof(dwBrightness),
                        &dwBytesReturned,
                        NULL);
        SuiteTeardown();

        if (!fRet)    {
            dwRet = TPR_FAIL;
            Debug(_T("ERROR! Cannot call IOCTL_BKL_GET_BRIGHTNESS."), dwBrightness);
        }
        else {
            dwRet = TPR_PASS;
            NKDbgPrintfW(_T("IOCTL_BKL_GET_BRIGHTNESS: dwBrightness = %d"), dwBrightness);
        }
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    if( TPR_PASS == dwRet ) SUCCESS( "GetBklBrightness Test Passed" );
    return dwRet;
} // GetBklBrightness()

HRESULT getMinMax(BKL_CAPABILITIES_INFO &minMax, DWORD &dwBytesReturned) {
    minMax.dwMinBrightness = 0;
    BOOL fRet = DeviceIoControl(hBklDevice, IOCTL_BKL_GET_BRIGHTNESS_CAPABILITIES, NULL, 0,
                                &minMax, sizeof(minMax), &dwBytesReturned, NULL);
    if(!fRet)
        return E_FAIL;
    return S_OK;
}

//*****************************************************************************
TESTPROCAPI GetBklBrightnessCap( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Functional/API Test: This tests the IOCTL_BKL_GET_BRIGHTNESS_CAPABILITIES command.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;

    BKL_CAPABILITIES_INFO minMax;
    DWORD dwBytesReturned = sizeof(minMax);

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        hr = getMinMax(minMax, dwBytesReturned);
        SuiteTeardown();

        if (hr != S_OK) {
            dwRet = TPR_FAIL;
            Debug(_T("Cannot get min and max brightness values!"));
        }
        else {
            if(!minMax.dwMaxBrightness || minMax.dwMinBrightness >= minMax.dwMaxBrightness) {
                dwRet = TPR_FAIL;
                Debug(_T("Bad values! dwMaxBrightness = %d, dwMinBrightness = %d"), minMax.dwMaxBrightness, minMax.dwMinBrightness);
            }
            else {
                NKDbgPrintfW(_T("IOCTL_BKL_GET_BRIGHTNESS_CAPABILITIES: dwMaxBrightness = %d"), minMax.dwMaxBrightness);
                NKDbgPrintfW(_T("IOCTL_BKL_GET_BRIGHTNESS_CAPABILITIES: dwMinBrightness = %d"), minMax.dwMinBrightness);
            }
        }
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    

    if( TPR_PASS == dwRet ) SUCCESS( "GetBklBrightnessCap Test Passed" );
    return dwRet;
} // GetBklBrightnessCap()

//*****************************************************************************
TESTPROCAPI GetBklSettings( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Functional/API Test: This tests the IOCTL_BKL_GET_SETTINGS command.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;

    BKL_CAPABILITIES_INFO minMax;
    BKL_SETTINGS_INFO bklSettings;
    DWORD dwBytesReturned = sizeof(bklSettings);

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        hr = getMinMax(minMax, dwBytesReturned);
        BOOL fRet = DeviceIoControl(hBklDevice, IOCTL_BKL_GET_SETTINGS, NULL, 0,
                                    &bklSettings, sizeof(bklSettings), &dwBytesReturned, NULL);
        SuiteTeardown();

        if(!fRet ||
            bklSettings.dwBrightness_ExPower > minMax.dwMaxBrightness ||
            bklSettings.dwBrightness_ExPower < minMax.dwMinBrightness ||
            bklSettings.dwBrightness_Battery > minMax.dwMaxBrightness ||
            bklSettings.dwBrightness_Battery < minMax.dwMinBrightness)
        {
            dwRet = TPR_FAIL;
            Debug(_T("Bad values! dwMaxBrightness = %d, dwMinBrightness = %d"), minMax.dwMaxBrightness, minMax.dwMinBrightness);
        }
        else {
            NKDbgPrintfW(_T("dwBrightness_ExPower = %d"), bklSettings.dwBrightness_ExPower);
            NKDbgPrintfW(_T("dwBrightness_Battery = %d"), bklSettings.dwBrightness_Battery);
            NKDbgPrintfW(_T("fEnable_ExPower      = %d"), bklSettings.fEnable_ExPower);
            NKDbgPrintfW(_T("fEnable_Battery      = %d"), bklSettings.fEnable_Battery);
        }
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    

    if( TPR_PASS == dwRet ) SUCCESS( "GetBklSettings Test Passed" );
    return dwRet;
} // GetBklSettings()

//*****************************************************************************
TESTPROCAPI ForceBklUpdate( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Functional/API Test: This tests the IOCTL_BKL_FORCE_UPDATE command.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;

    DWORD dwOutBuffer     = 0;
    DWORD dwBytesReturned = 0;

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        BOOL fRet = DeviceIoControl(hBklDevice,
                        IOCTL_BKL_FORCE_UPDATE,
                        NULL,
                        0,
                        (LPVOID) &dwOutBuffer,
                        sizeof(dwOutBuffer),
                        &dwBytesReturned,
                        NULL);
        SuiteTeardown();

        if (!fRet) {
            dwRet = TPR_FAIL;
        }
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    

    if( TPR_PASS == dwRet ) SUCCESS( "ForceBklUpdate Test Passed" );
    return dwRet;
} // ForceBklUpdate()

//*****************************************************************************
TESTPROCAPI SetBklSettings( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Functional/API Test: This tests the IOCTL_BKL_SET_SETTINGS command.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;

    BKL_CAPABILITIES_INFO minMax;
    BKL_SETTINGS_INFO bklSettings;
    DWORD dwBytesReturned = sizeof(bklSettings);

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        hr = getMinMax(minMax, dwBytesReturned);
        BOOL fRet = DeviceIoControl(hBklDevice, IOCTL_BKL_GET_SETTINGS, NULL, 0,
                                    &bklSettings, sizeof(bklSettings), &dwBytesReturned, NULL);

        NKDbgPrintfW(_T("IOCTL_BKL_GET_SETTINGS: Original backlight settings = %d"), bklSettings);

        for (DWORD i = minMax.dwMinBrightness; i <= minMax.dwMaxBrightness; i++)
        {
            bklSettings.fEnable_ExPower = TRUE;
            bklSettings.dwBrightness_ExPower = i;
            bklSettings.fEnable_Battery = TRUE;
            bklSettings.dwBrightness_Battery = i;

            fRet = DeviceIoControl(hBklDevice, IOCTL_BKL_SET_SETTINGS,
                                &bklSettings, sizeof(bklSettings), NULL, 0, NULL, NULL);

            NKDbgPrintfW(_T("IOCTL_BKL_SET_SETTINGS: Setting backlight = %d"), bklSettings);

            if (!fRet) {
                dwRet = TPR_FAIL;
                break;
            }
            else {
                bklSettings.fEnable_ExPower = FALSE;
                bklSettings.dwBrightness_ExPower = 0;
                bklSettings.fEnable_Battery = FALSE;
                bklSettings.dwBrightness_Battery = 0;

                dwBytesReturned = sizeof(bklSettings);
                BOOL fRet = DeviceIoControl(hBklDevice, IOCTL_BKL_GET_SETTINGS, NULL, 0,
                                            &bklSettings, sizeof(bklSettings), &dwBytesReturned, NULL);

                if (!fRet ||
                    !bklSettings.fEnable_ExPower ||
                    bklSettings.dwBrightness_ExPower != i ||
                    !bklSettings.fEnable_Battery ||
                    bklSettings.dwBrightness_Battery != i)
                {
                    Debug(_T("ERROR! IOCTL_BKL_GET_SETTINGS: Backlight NOT correctly set = %d"), bklSettings);
                    dwRet = TPR_FAIL;
                    break;
                }
            }
            Sleep(100);
        }
        SuiteTeardown();
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    

    if( TPR_PASS == dwRet ) SUCCESS( "SetBklSettings Test Passed" );
    return dwRet;
} // SetBklSettings()

UCHAR GetPowerStates(void)
{
    POWER_CAPABILITIES PowerCaps;
    PowerCaps.DeviceDx = 0;
    DWORD dwBytesReturned = 0;
    BOOL fRet = DeviceIoControl(hBklDevice,
                                IOCTL_POWER_CAPABILITIES,
                                NULL,
                                0,
                                (LPVOID) &PowerCaps,
                                sizeof(PowerCaps),
                                &dwBytesReturned,
                                NULL);
    if (fRet && PowerCaps.DeviceDx != 0)
    {
        return PowerCaps.DeviceDx;
    }
    return 0;
}

//*****************************************************************************
TESTPROCAPI PwrMgrBklOps( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  This test function test the ability to query the NLED_SUPPORTS_INFO struct
//  from the driver. The test will return TPR_PASS if there are no LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet     = TPR_PASS;
    if(!g_bEnablePMtest) {
        dwRet = TPR_SKIP;
        NKDbgPrintfW(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~"));
        NKDbgPrintfW(_T("Please use the -c\"-p\" option to enable power manager tests."));
        NKDbgPrintfW(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~"));
        goto THE_END;
    }

    POWER_CAPABILITIES PowerCaps;
    PowerCaps.DeviceDx = 0;
    DWORD dwBytesReturned = 0;

    HRESULT hr = SuiteSetup();
    if(S_OK == hr) {
        UCHAR supportedStates = GetPowerStates();
        if (!supportedStates)
        {
            dwRet = TPR_FAIL;
        }
        NKDbgPrintfW(_T("IOCTL_POWER_CAPABILITIES: Supported Power States = %d"), supportedStates);
        CEDEVICE_POWER_STATE OrigState = D0;
        dwBytesReturned = 0;
        BOOL fRet = DeviceIoControl(hBklDevice,
                    IOCTL_POWER_GET,
                    NULL,
                    0,
                    (LPVOID) &OrigState,
                    sizeof(OrigState),
                    &dwBytesReturned,
                    NULL);
        if (!fRet)
        {
            dwRet = TPR_FAIL;
        }

        NKDbgPrintfW(_T("IOCTL_POWER_GET: Saved Original Power State = %d"), OrigState);

        CEDEVICE_POWER_STATE state = D0;
        for (int i=0; i <= (int)PwrDeviceMaximum; i++)
        {
            state = (CEDEVICE_POWER_STATE)i;
            if(VALID_DX(state) && (DX_MASK(state) & supportedStates))
            {
                dwBytesReturned = 0;
                fRet = DeviceIoControl(hBklDevice,
                                    IOCTL_POWER_QUERY,
                                    NULL,
                                    0,
                                    (LPVOID) &state,
                                    sizeof(state),
                                    &dwBytesReturned,
                                    NULL);
                if (!fRet)
                {
                    dwRet = TPR_FAIL;
                }
                else
                {
                    NKDbgPrintfW(_T("IOCTL_POWER_QUERY: Current Power State = %d"), state);
                    fRet = DeviceIoControl(hBklDevice,
                                    IOCTL_POWER_SET,
                                    NULL,
                                    0,
                                    (LPVOID) &state,
                                    sizeof(state),
                                    &dwBytesReturned,
                                    NULL);
                    if (!fRet ||
                        (CEDEVICE_POWER_STATE)i != state)
                    {
                        dwRet = TPR_FAIL;
                    }
                    else
                    {
                        NKDbgPrintfW(_T("IOCTL_POWER_SET: Set Power State = %d"), state);
                        fRet = DeviceIoControl(hBklDevice,
                                    IOCTL_POWER_GET,
                                    NULL,
                                    0,
                                    (LPVOID) &state,
                                    sizeof(state),
                                    &dwBytesReturned,
                                    NULL);
                        if (!fRet || (CEDEVICE_POWER_STATE)i != state)
                        {
                            Debug(_T("ERROR! IOCTL_POWER_GET: Power State NOT correctly set = %d"), state);
                            dwRet = TPR_FAIL;
                        }
                        else {
                            NKDbgPrintfW(_T("IOCTL_POWER_GET: Get Power State = %d"), state);
                        }
                    }
                }
                Sleep(1000);
            }
        }

        dwBytesReturned = 0;
        //restore original power state
        fRet = DeviceIoControl(hBklDevice,
                    IOCTL_POWER_SET,
                    NULL,
                    0,
                    (LPVOID) &OrigState,
                    sizeof(OrigState),
                    &dwBytesReturned,
                    NULL);
        NKDbgPrintfW(_T("IOCTL_POWER_SET: Return to Original Power State = %d"), OrigState);
        SuiteTeardown();
    }
    else {
        dwRet = TPR_FAIL;
        Debug(_T("ERROR! Cannot get backlight driver."));
    }

    
THE_END:
    if( TPR_PASS == dwRet ) SUCCESS( "PwrMgrBklOps Test Passed" );
    return dwRet;
} // PwrMgrBklOps()

