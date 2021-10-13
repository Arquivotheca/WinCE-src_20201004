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

    
////////////////////////////////////////////////////////////////////////////////
/********************************** Headers *********************************/

#include "pwrioctls.h"
#include "pwrtestclass.h"
#include <clparse.h>



////////////////////////////////////////////////////////////////////////////////
/*************** Constants and Defines Local to This File *******************/
typedef HDC (WINAPI *PFN_CreateDCW)(LPCWSTR, LPCWSTR , LPCWSTR , CONST DEVMODEW *);
typedef BOOL (WINAPI *PFN_DeleteDC)(HDC);
typedef int (WINAPI *PFN_ExtEscape)(HDC, int, int, LPCSTR, int, LPSTR);

// global function pointers
static PFN_CreateDCW gpfnCreateDCW = NULL;
static PFN_DeleteDC gpfnDeleteDC = NULL;
static PFN_ExtEscape gpfnExtEscape = NULL;

BOOL g_fDisplay = FALSE;

////////////////////////////////////////////////////////////////////////////////
// LOG
// Printf-like wrapping around g_pKato->Log(LOG_DETAIL, ...)
//
// Parameters:
// szFormat     Formatting string (as in printf).
// ...          Additional arguments.
//
// Return value:
// None.
////////////////////////////////////////////////////////////////////////////////

void LOG(LPCWSTR szFormat, ...)
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFormat);
        g_pKato->LogV(LOG_DETAIL, szFormat, va);
        va_end(va);
    }
}


////////////////////////////////////////////////////////////////////////////////
// LogErr
// Printf-like wrapping around g_pKato->Log(LOG_DETAIL, ...)
// Prefixes the output with "!!ERROR!!-> "
//
// Parameters:
// szFormat     Formatting string (as in printf).
// ...          Additional arguments.
//
// Return value:
// None.
////////////////////////////////////////////////////////////////////////////////

VOID LogErr(LPCTSTR szFormat, ...)
{
    va_list va;
    va_start(va, szFormat);

    // Prepare buffer to hold output.
    TCHAR szBuffer[1024] = TEXT("!!ERROR!!-> ");

    // Print output to the buffer. Account for the space taken in the buffer by "!!ERROR!!-> "(12)
    _vsntprintf_s(szBuffer + 12, _countof(szBuffer) - 12, _TRUNCATE, szFormat, va);

    // Make sure that we null terminate
    szBuffer[_countof(szBuffer) - 1] = '\0';

    g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);
}



////////////////////////////////////////////////////////////////////////////////
/****************** Helper Functions **********************/

////////////////////////////////////////////////////////////////////////////////
// InitDisplayDevice
// Initializes the Display device globals
//
// Parameters:
// None
//
// Return value:
// TRUE if initialization successful, FALSE otherwise.
////////////////////////////////////////////////////////////////////////////////

BOOL InitDisplayDevice(void)
{
    if(gpfnCreateDCW == NULL)
    {
        HMODULE hmCoredll = (HMODULE) LoadLibrary(_T("coredll.dll"));
        if(hmCoredll == NULL)
        {
            LogErr(_T("LoadLibrary() failed on coredll.dll. GetLastError is %d."), GetLastError());
            return FALSE;
        }
        else
        {
            // get procedure addresses
            gpfnCreateDCW = (PFN_CreateDCW) GetProcAddress(hmCoredll, _T("CreateDCW"));
            gpfnDeleteDC = (PFN_DeleteDC) GetProcAddress(hmCoredll, _T("DeleteDC"));
            gpfnExtEscape = (PFN_ExtEscape) GetProcAddress(hmCoredll, _T("ExtEscape"));
            if(gpfnCreateDCW == NULL || gpfnExtEscape == NULL || gpfnDeleteDC == NULL)
            {
                LogErr(_T("Can't get APIs: pfnCreateDCW 0x%08x, pfnDeleteDC 0x%08x, pfnExtEscape 0x%08x"),
                    gpfnCreateDCW, gpfnDeleteDC, gpfnExtEscape);
                gpfnCreateDCW = NULL;
                gpfnDeleteDC = NULL;
                gpfnExtEscape = NULL;
                FreeLibrary(hmCoredll);
                return FALSE;
            }
        }
    }

    return TRUE ;
}

////////////////////////////////////////////////////////////////////////////////
// GetDeviceHandle
// Gets the Handle for the specified device
//
// Parameters:
// pszDeviceName      Pointer to string containing the device name
//
// Return value:
// Handle to the specified device.
////////////////////////////////////////////////////////////////////////////////

HANDLE GetDeviceHandle(const TCHAR* pszDeviceName)
{
    HANDLE hDevice = NULL;

    if(g_fDisplay)
    {
        // First initialize the display device
        if(!InitDisplayDevice())
        {
            LogErr(_T("Display device, %s initialization failed."), pszDeviceName);
            hDevice = INVALID_HANDLE_VALUE;
        }

        // If initialization successful, get the handle
        if((gpfnCreateDCW != NULL) && (gpfnDeleteDC != NULL) && (gpfnExtEscape != NULL))
        {
            hDevice = gpfnCreateDCW(pszDeviceName, NULL, NULL, NULL);
            if(hDevice == NULL)
            {
                LogErr(_T("CreateDCW for device '%s' failed."), pszDeviceName);
                hDevice = INVALID_HANDLE_VALUE;
            }
            else
            {
                // Determine whether the display driver really supports the PM IOCTLs
                DWORD dwIoctl[] = { IOCTL_POWER_CAPABILITIES, IOCTL_POWER_SET, IOCTL_POWER_GET };

                int i;
                for(i = 0; i < _countof(dwIoctl); i++)
                {
                    if(gpfnExtEscape((HDC)hDevice, QUERYESCSUPPORT, sizeof(dwIoctl[i]), (LPCSTR)&dwIoctl[i], 0, NULL) <= 0)
                        break;
                }

                if(i < _countof(dwIoctl))
                {
                    LogErr(_T("Device '%s' doesn't support all power manager control codes."), pszDeviceName);
                    gpfnDeleteDC((HDC) hDevice);
                    hDevice = INVALID_HANDLE_VALUE;
                }
            }
        }
    }
    else if((hDevice = CreateFile(pszDeviceName, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, NULL)) == NULL)
    {
        LogErr(_T("CreateFile for device '%s' failed."), pszDeviceName);
    }

    return hDevice;
}

////////////////////////////////////////////////////////////////////////////////
// CloseDeviceHandle
// Closes the Handle for the specified device
//
// Parameters:
// hDevice         Handle to the specified device
//
// Return value:
// True if successful, FALSE otherwise
////////////////////////////////////////////////////////////////////////////////

BOOL CloseDeviceHandle(HANDLE hDevice)
{
    if((hDevice) && (hDevice != INVALID_HANDLE_VALUE))
    {
        if(g_fDisplay)
        {
            if(!gpfnDeleteDC((HDC) hDevice))
                return FALSE;
        }
        else
        {
            if(!CloseHandle(hDevice))
                return FALSE;
        }
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// RequestDevice
// Calls the relevant Power IOCTL and gets the results.
//
// Parameters:
// hDevice          Handle to the device
// dwIoctlReq       Ioctl code
// pInBuf           Input to the Ioctl
// dwSizeInBug      Size of input buffer
// pOutBuf          Output buffer for the Ioctl
// dwSizeOutBuf     Size for output buffer
// pdwBytesRet      Pointer to DWORD containing bytes returned by the Ioctl
//
// Return value:
// TRUE if IOCTL call successful, FALSE otherwise.
////////////////////////////////////////////////////////////////////////////////

BOOL RequestDevice(HANDLE hDevice, DWORD dwIoctlReq, LPVOID pInBuf, DWORD dwSizeInBuf,
                   LPVOID pOutBuf, DWORD dwSizeOutBuf, LPDWORD pdwBytesRet, LPDWORD pdwLastErr)
{
    if(g_fDisplay)
    {

        if(gpfnExtEscape((HDC)hDevice, dwIoctlReq, dwSizeInBuf, (LPCSTR)pInBuf, dwSizeOutBuf, (LPSTR)pOutBuf) <= 0)
        {
            *pdwLastErr = GetLastError();
            return FALSE;
        }
        else
        {
            // Fill pdwBytesRet with the output size
            if(pdwBytesRet != NULL)
            {
                *pdwBytesRet = dwSizeOutBuf;
            }
            return TRUE ;
        }
    }
    else
    {
        if(!DeviceIoControl(hDevice, dwIoctlReq, pInBuf, dwSizeInBuf, pOutBuf, dwSizeOutBuf, pdwBytesRet, NULL))
        {
            *pdwLastErr = GetLastError();
            return FALSE;
        }
        return TRUE;
    }
}


/****************************** Helper Functions *****************************/
////////////////////////////////////////////////////////////////////////////////
// SupportedDx
// Checks if the Device Power State is supported by the device
//
// Parameters:
// dx               Device power state
// deviceDx         Mask specifiying D0-D4 support.
//
// Return value:
// TRUE if the given power state is supported, FALSE otherwise.
////////////////////////////////////////////////////////////////////////////////

inline BOOL SupportedDx(CEDEVICE_POWER_STATE dx, UCHAR deviceDx)
{
    return (DX_MASK(dx) & deviceDx);
}


////////////////////////////////////////////////////////////////////////////////
// DeviceDx2String
// Convert a bitmask Device Dx specification into a string containing D0, D1,
// D2, D3, and D4 values as specified by the bitmask.
//
// Parameters:
// deviceDx         Device Dx itmask specifiying D0-D4 support.
// szDeviceDx       Output string buffer.
// cchDeviceDx      Length of szDeviceDx buffer, in WCHARs.
//
// Return value:
// Pointer to the input buffer szDeviceDx. If the Device Dx bitmask specifies
// no supported DXs, the returned string will be empty.
////////////////////////////////////////////////////////////////////////////////

LPWSTR DeviceDx2String(UCHAR deviceDx, __out_ecount(cchDeviceDx) LPWSTR szDeviceDx, DWORD cchDeviceDx)
{
    // Start the string off empty (in case no DX is supported)
    VERIFY(SUCCEEDED(StringCchCopy(szDeviceDx, cchDeviceDx, L"")));

    // Check D0 support
    if(SupportedDx(D0, deviceDx))
    {
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L"D0")));
    }

    // Check D1 support
    if(SupportedDx(D1, deviceDx))
    {
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L" D1")));
    }

    // Check D2 support
    if(SupportedDx(D2, deviceDx))
    {
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L" D2")));
    }

    // Check D3 support
    if(SupportedDx(D3, deviceDx))
    {
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L" D3")));
    }

    // Check D4 support
    if(SupportedDx(D4, deviceDx))
    {
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L" D4")));
    }

    return szDeviceDx;
}


////////////////////////////////////////////////////////////////////////////////
// LogPowerCapabilities
// Log contents of POWER_CAPABILITIES structure
//
// Parameters:
// ppwrCaps       Pointer to POWER_CAPABILITIES structure to be logged
//
// Return value:
// None
////////////////////////////////////////////////////////////////////////////////

void LogPowerCapabilities(POWER_CAPABILITIES *ppwrCaps)
{
    CEDEVICE_POWER_STATE dx;
    WCHAR szDeviceDx[32];
    if(ppwrCaps)
    {
        LOG(_T("Following are the POWER_CAPABILITIES:"));
        LOG(_T(" DeviceDx: %s"), DeviceDx2String(ppwrCaps->DeviceDx, szDeviceDx, 32));
        LOG(_T(" WakeFromDx: %s"), DeviceDx2String(ppwrCaps->WakeFromDx, szDeviceDx, 32));
        LOG(_T(" InrushDx: %s"), DeviceDx2String(ppwrCaps->InrushDx, szDeviceDx, 32));

        for(dx = D0; dx < PwrDeviceMaximum;
            dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
        {
            //Log max power consumption for supported device state
            if(SupportedDx(dx, ppwrCaps->DeviceDx))
            {
                LOG(_T(" Power D%u: %u mW"), dx, ppwrCaps->Power[dx]);
            }
        }

        for(dx = D0; dx < PwrDeviceMaximum;
            dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
        {
            //Log max latency to return from supported device state to D0 state
            if(SupportedDx(dx, ppwrCaps->DeviceDx))
            {
                LOG(_T(" Latency D%u: %u ms"), dx, ppwrCaps->Latency[dx]);
            }
        }

        //Log flags
        LOG(_T(" Flags: 0x%08x"), ppwrCaps->Flags);
    }
}


////////////////////////////////////////////////////////////////////////////////
// InitializeCmdLine
// Process Command Line.
//
// Parameters:
// pszCmdLine       Command Line String .
//
// Return value:
// Void
////////////////////////////////////////////////////////////////////////////////

BOOL ParseCmdLine (TCHAR *pszDeviceName, DWORD dwLen)
{
    // Check inputs
    if((!pszDeviceName) || (dwLen == 0))
    {
        return FALSE;
    }

    CClParse cmdLine(g_pShellInfo->szDllCmdLine);

    // Check if the command line exists
    if(!g_pShellInfo->szDllCmdLine)
    {
        LOG(_T("No command line specified."));
        return FALSE;
    }

    // Get the device legacy name
    if(cmdLine.GetOptString(_T("devName"), pszDeviceName, dwLen) )
    {
        // Verify that the string returned is not empty
        TCHAR szEmptyStr = _T('\0');
        if(0 == _tcsncmp(pszDeviceName, &szEmptyStr, sizeof(szEmptyStr)))
        {
            LogErr(_T("Error parsing the device name. Verify that the device name is not more than 5 characters long."));
            LOG(_T(""));
            return FALSE;
        }

        LOG(_T("The command line specifies the device name as %s"), pszDeviceName);
        return TRUE;
    }

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
*
* TST_GetPwrManageableDevices
*
******************************************************************************/
/*
* This test prints out the list of power manageable devices.
* For each device, it prints out the power capabilities and verifies that
* power state D0 is supported.
*/
////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI TST_GetPwrManageableDevices(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    INT returnVal = TPR_PASS;

    POWER_CAPABILITIES pwrCaps = {0};
    DWORD dwBytesRet, dwLastErr;
    HANDLE hDevice = NULL;
    CPowerManagedDevices cPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceInfo;

    LOG(_T("")); //Blank line
    LOG( _T("This test will iterate through Power Managed devices and"));
    LOG( _T("for each it tries to get the power capabilities and displays them."));
    LOG( _T("Also, for each it verifies if the power state D0 is supported."));

    GUID  displayGUID;
    if(!ConvertStringToGuid(PMCLASS_DISPLAY, &displayGUID))
    {
        LogErr(_T("can't convert '%s' to GUID"), PMCLASS_DISPLAY);
        ASSERT(TRUE);
        returnVal = TPR_FAIL;
        goto CleanUp;
    }

    //Iterate through all the Power Managed devices
    while((pDeviceInfo = cPMDevices.GetNextPMDevice()) != NULL)
    {


        LOG(_T("")); //Blank line
        LOG(_T("Testing for device: %s"), pDeviceInfo->LegacyName);


        // Check if this is a display device
        g_fDisplay = FALSE;
        if(pDeviceInfo->guid == displayGUID)
        {
            LOG(_T("This is a Display device."));
            g_fDisplay = TRUE;
        }


        // Get handle for the device
        hDevice = GetDeviceHandle(pDeviceInfo->LegacyName);
        if(hDevice == INVALID_HANDLE_VALUE)
        {
            LogErr(_T("Could not get a handle to the device. Skip this device - %s."), pDeviceInfo->LegacyName);
            continue;
        }


        // Get the power capabilities of the device and check if it is a parent device
        LOG(_T("Sending IOCTL_POWER_CAPABILITIES to device %s"), pDeviceInfo->LegacyName);
        if(!RequestDevice(hDevice,IOCTL_POWER_CAPABILITIES, NULL, 0, &pwrCaps, sizeof(pwrCaps), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanUp;
        }
        else if(pwrCaps.Flags)
        {
            LOG(_T("Device %s is configured as Parent. Skip this device."), pDeviceInfo->LegacyName);
            goto CleanUp;
        }


        //Log the power capabilities
        LogPowerCapabilities(&pwrCaps);


        //Check if D0 is supported
        if(!SupportedDx(D0, pwrCaps.DeviceDx))
        {
            LogErr(_T("Device Power State D0 is not supported for this device."));
            LogErr( _T("D0, which is the Fully ON state, must be supported by all devices"));
            returnVal = TPR_FAIL;
        }
        else
        {
            LOG(_T("D0, which is the Fully ON state, is supported by this device."));
        }

CleanUp:
        if(!CloseDeviceHandle(hDevice))
        {
            LogErr(_T("Error closing handle to the device."));
            returnVal = TPR_FAIL;
        }
    }

    LOG(_T("")); //Blank line
    LOG(_T("***** End of Test *****"));

    return returnVal;

}


////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
*
* TST_GetDevPwrLevels
*
******************************************************************************/
/*
* This test will iterate through Power Managed devices and for each
* it tries to get the current power state and displays it. Also, for each
* it verifies if the current power state reported is a valid one (is one of D0 to D4)
* and is supported by the device as advertised by its POWER_CAPABILITIES.
*/
////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI TST_GetDevPwrLevels(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    INT returnVal = TPR_PASS;

    CEDEVICE_POWER_STATE dx;
    POWER_CAPABILITIES pwrCaps = {0};
    DWORD dwBytesRet, dwLastErr;
    HANDLE hDevice = NULL;
    CPowerManagedDevices cPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceInfo;


    LOG(_T("")); //Blank line
    LOG( _T("This test will iterate through Power Managed devices and"));
    LOG( _T("for each it tries to get the current power state and displays it."));
    LOG( _T("Also, for each it verifies if the current power state reported is"));
    LOG( _T("a valid one (is one of D0 to D4) and is supported by the device"));
    LOG( _T("as advertised by its POWER_CAPABILITIES."));

    GUID  displayGUID;
    if(!ConvertStringToGuid(PMCLASS_DISPLAY, &displayGUID))
    {
        LogErr(_T("can't convert '%s' to GUID"), PMCLASS_DISPLAY);
        ASSERT(TRUE);
        returnVal = TPR_FAIL;
        goto CleanUp;
    }

    //Iterate through all the Power Managed devices
    while((pDeviceInfo = cPMDevices.GetNextPMDevice()) != NULL)
    {

        LOG(_T("")); //Blank line
        LOG(_T("Testing for device: %s"), pDeviceInfo->LegacyName);


        // Check if this is a display device
        g_fDisplay = FALSE;
        if(pDeviceInfo->guid == displayGUID)
        {
            LOG(_T("This is a Display device."));
            g_fDisplay = TRUE;
        }


        // Get handle for the device
        hDevice = GetDeviceHandle(pDeviceInfo->LegacyName);
        if(hDevice == INVALID_HANDLE_VALUE)
        {
            LogErr(_T("Could not get a handle to the device. Skip this device."), pDeviceInfo->LegacyName);
            continue;
        }


        // Get the power capabilities of the device and check if it is a parent device
        LOG(_T("Sending IOCTL_POWER_CAPABILITIES to device %s"), pDeviceInfo->LegacyName);
        if(!RequestDevice(hDevice,IOCTL_POWER_CAPABILITIES, NULL, 0, &pwrCaps, sizeof(pwrCaps), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanUp;
        }
        else if(pwrCaps.Flags)
        {
            LOG(_T("Device %s is configured as Parent. Skip this device."), pDeviceInfo->LegacyName);
            goto CleanUp;
        }


        // Get the current device power state
        LOG(_T("Sending IOCTL_POWER_GET to device %s"), pDeviceInfo->LegacyName);

        dx = PwrDeviceUnspecified;

        if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &dx, sizeof(dx), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
        }
        else
        {
            // Verify that the power state returned is between D0-D4.
            if(VALID_DX(dx))
            {
                LOG(_T("Device %s reports a current power state of D%u"), pDeviceInfo->LegacyName, dx);

            }
            else if(PwrDeviceUnspecified == dx)
            {
                LogErr(_T("Device %s reports a current power state of PwrDeviceUnspecified."), pDeviceInfo->LegacyName, dx);
                returnVal = TPR_FAIL;
            }
            else
            {
                LogErr(_T("Device %s reports an invalid power state of D%u"), pDeviceInfo->LegacyName, dx);
                returnVal = TPR_FAIL;
            }

            // Verify that the power state returned is advertised
            // by the device in POWER_CAPABILITIES
            if(!SupportedDx(dx, pwrCaps.DeviceDx))
            {
                LogErr(_T("This state is NOT supported by the device as seen from its power caps(DeviceDx = %u)."), pwrCaps.DeviceDx);
                returnVal = TPR_FAIL;
            }
        }


CleanUp:
        if(!CloseDeviceHandle(hDevice))
        {
            LogErr(_T("Error closing handle to the device."));
            returnVal = TPR_FAIL;
        }
    }

    LOG(_T("")); //Blank line
    LOG(_T("***** End of Test *****"));

    return returnVal;

}


////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
*
* TST_SetDevPwrLevels
*
******************************************************************************/
/*
* This test will iterate through Power Managed devices and for each
* it cycles through all possible power states(D0 through D4) and tries
* to set the device to the power state. The test will then verify that a
* supported power state is set and an unsupported power state is not set.
*/
////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI TST_SetDevPwrLevels(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    INT returnVal = TPR_PASS;

    CEDEVICE_POWER_STATE dx, origDx, setDx, getDx;
    POWER_CAPABILITIES pwrCaps = {0};
    DWORD dwBytesRet, dwLastErr;
    HANDLE hDevice = NULL;
    CPowerManagedDevices cPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceInfo;


    LOG(_T("")); //Blank line
    LOG( _T("This test will iterate through Power Managed devices and"));
    LOG( _T("for each it cycles through all possible power states(D0 through D4)"));
    LOG( _T("and tries to set the device to the power state."));
    LOG( _T("The test will then verify that a supported power state is set"));
    LOG( _T("and an unsupported power state is not set."));

    GUID  displayGUID;
    if(!ConvertStringToGuid(PMCLASS_DISPLAY, &displayGUID))
    {
        LogErr(_T("can't convert '%s' to GUID"), PMCLASS_DISPLAY);
        ASSERT(TRUE);
        returnVal = TPR_FAIL;
        goto CleanUp;
    }


    //Iterate through all the Power Managed devices
    while((pDeviceInfo = cPMDevices.GetNextPMDevice()) != NULL)
    {

        LOG(_T("")); //Blank line
        LOG(_T("Testing for device: %s"), pDeviceInfo->LegacyName);

        // Check if this is a display device
        g_fDisplay = FALSE;
        if(pDeviceInfo->guid == displayGUID)
        {
            LOG(_T("This is a Display device."));
            g_fDisplay = TRUE;
        }

        // Get handle for the device
        hDevice = GetDeviceHandle(pDeviceInfo->LegacyName);
        if(hDevice == INVALID_HANDLE_VALUE)
        {
            LogErr(_T("Could not get a handle to the device. Skip this device."), pDeviceInfo->LegacyName);
            continue;
        }


        // Get the power capabilities of the device and check if it is a parent device
        LOG(_T("Sending IOCTL_POWER_CAPABILITIES to device %s"), pDeviceInfo->LegacyName);
        if(!RequestDevice(hDevice,IOCTL_POWER_CAPABILITIES, NULL, 0, &pwrCaps, sizeof(pwrCaps), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanUp;
        }
        else if(pwrCaps.Flags)
        {
            LOG(_T("Device %s is configured as Parent. Skip this device."), pDeviceInfo->LegacyName);
            goto CleanUp;
        }

        // Log supported power levels
        WCHAR szDeviceDx[32];
        LOG(_T("The supported power levels for this device are: %s"), DeviceDx2String(pwrCaps.DeviceDx, szDeviceDx, 32));


        // Query the starting device power state so we can set it back when the test is done (origDx)
        LOG(_T("Sending IOCTL_POWER_GET to device %s"), pDeviceInfo->LegacyName);

        origDx = PwrDeviceUnspecified;
        if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &origDx, sizeof(origDx), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanUp;

        }
        // Verify that the power state returned is between D0-D4.
        else if(!VALID_DX(origDx))
        {
            LogErr(_T("Device %s reports an invalid power state of D%u"), pDeviceInfo->LegacyName, origDx);
            returnVal = TPR_FAIL;
            goto CleanUp;
        }
        else
        {
            LOG(_T("Device %s reports a current power state of D%u"), pDeviceInfo->LegacyName, origDx);
        }


        // Iterate through all the Device Power States and verify setting supported/unsupported power states
        LOG(_T("We will iterate through all the Device Power States now."));
        LOG(_T("*************************************")); //blank line

        for(dx = D0; dx < PwrDeviceMaximum;dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
        {
            LOG(_T("Test power state D%u --->"), dx);
            setDx = dx;

            // Try to put the driver in the new power state (setDx)
            LOG(_T("Sending IOCTL_POWER_SET with power state D%u to the device %s"), setDx, pDeviceInfo->LegacyName);

            BOOL bret = RequestDevice(hDevice, IOCTL_POWER_SET, NULL, 0, &setDx, sizeof(setDx), &dwBytesRet, &dwLastErr);

            // Reset power level get value
            getDx = PwrDeviceUnspecified;

            // Check if the dx is a supported power state
            if(SupportedDx(dx, pwrCaps.DeviceDx))
            {
                if(!bret)
                {
                    LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
                    returnVal = TPR_FAIL;
                    continue;
                }

                LOG(_T("D%u is a supported state for this device, so we expect that the power level is set."), dx);

                // Query the power state to get the current state of the device (getDx)
                LOG(_T("Sending IOCTL_POWER_GET to the device %s"), pDeviceInfo->LegacyName);

                if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &getDx, sizeof(getDx), &dwBytesRet, &dwLastErr))
                {
                    LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
                    returnVal = TPR_FAIL;
                    goto CleanUp;
                }
                LOG(_T("Device %s reports a current power state of D%u"), pDeviceInfo->LegacyName, getDx);


                // Check if we got what we set
                if(getDx != dx)
                {
                    LogErr(_T("The device was set to D%u and what we got is D%u"), dx, getDx);
                    returnVal = TPR_FAIL;
                }
                else
                {
                    LOG(_T("The reported power state matches the power state that was set."));
                }
            }
            else
            {
                LOG(_T("D%u is an unsupported state for this device, so we expect that the power level is not set."), dx);

                // Query the power state to get the current state of the device (getDx)
                LOG(_T("Sending IOCTL_POWER_GET to the device %s"), pDeviceInfo->LegacyName);

                if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &getDx, sizeof(getDx), &dwBytesRet, &dwLastErr))
                {
                    LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
                    returnVal = TPR_FAIL;
                    goto CleanUp;
                }
                LOG(_T("Device %s reports a current power state of D%u"), pDeviceInfo->LegacyName, getDx);


                // Verify that the unsupported value was not set
                if(getDx == dx)
                {
                    LogErr(_T("The device was set to D%u, which is unexpected."), getDx);
                    returnVal = TPR_FAIL;
                }
            }

        }

        // Now put the driver back into its original power state
        LOG(_T("Sending IOCTL_POWER_SET with original power state D%u to device %s"), origDx, pDeviceInfo->LegacyName);

        if(!RequestDevice(hDevice, IOCTL_POWER_SET, NULL, 0, &origDx, sizeof(origDx), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
        }



CleanUp:
        if(!CloseDeviceHandle(hDevice))
        {
            LogErr(_T("Error closing handle to the device."));
            returnVal = TPR_FAIL;
        }
    }

    LOG(_T("")); //Blank line
    LOG(_T("***** End of Test *****"));

    return returnVal;

}


////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
*
* TST_PwrLevelsForUserGivenDev
*
******************************************************************************/
/*
* This test gets the user given device name from the command line.
* It prints out the power capabilities of the device and verifies that
* power state D0 is supported.
* It gets the current power state and verifies if it is a valid one, and
* if it is supported by the device as advertised by its Power_Capabilities.
* It cycles through all possible power states(D0 through D4) and tries
* to set the device to each of the power states. It then verifies that a
* supported power state is set and an unsupported power state is not set.
*/
////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI TST_PwrLevelsForUserGivenDev(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    INT returnVal = TPR_PASS;

    CEDEVICE_POWER_STATE dx, origDx, setDx, getDx;
    POWER_CAPABILITIES pwrCaps = {0};
    DWORD dwBytesRet, dwLastErr;
    HANDLE hDevice = NULL;
    CPowerManagedDevices cPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceInfo;
    TCHAR szLegacyName[MAX_DEV_NAME_LEN];


    LOG(_T("")); //Blank line
    LOG(_T("This test gets the user given device name and checks if it is a"));
    LOG(_T("power-manageable device. Then checks the following:"));
    LOG(_T("1. It prints out the power capabilities of the device and verifies that"));
    LOG(_T("power state D0 is supported."));
    LOG(_T("2. It gets the current power state and verifies if it is a valid one, and"));
    LOG(_T("if it is supported by the device as advertised by its Power_Capabilities."));
    LOG(_T("3. It cycles through all possible power states(D0 through D4) and tries"));
    LOG(_T("to set the device to each of the power states. It then verifies that a"));
    LOG(_T("supported power state is set and an unsupported power state is not set."));
    LOG(_T("")); //Blank line


    GUID  displayGUID;
    if(!ConvertStringToGuid(PMCLASS_DISPLAY, &displayGUID))
    {
        LogErr(_T("can't convert '%s' to GUID"), PMCLASS_DISPLAY);
        ASSERT(TRUE);
        returnVal = TPR_FAIL;
        goto CleanAndExit;
    }

    // Get the device name from the command line.
    if(!(ParseCmdLine(szLegacyName, MAX_DEV_NAME_LEN)))
    {
        LOG(_T("Error parsing the command line. Please provide a valid device legacy name to test."));
        LOG(_T("You can specify the device name using the options -devName <legacy name> or /devName <legacy name>"));
        LOG(_T("Ex: -c\"-devName WAV1:\""));
        returnVal = TPR_SKIP; //We will skip test if no device name is provided.
        goto CleanAndExit;
    }


    LOG(_T("")); //Blank line
    LOG(_T("Begin testing for device %s ..."), szLegacyName);
    LOG(_T("")); //Blank line


    // Find out if it is a display device. Look through our list of power managed devices
    BOOL found = FALSE;
    while((pDeviceInfo = cPMDevices.GetNextPMDevice()) != NULL)
    {
        // Compare device names.
        if(0 == _tcsncmp(pDeviceInfo->LegacyName, szLegacyName, sizeof(szLegacyName)))
        {
            // Found a match
            found = TRUE;
            break;
        }
    }
    if(!found)
    {
        // Is this is a PM device.
        LogErr(_T("This device is not associated with a Power Manager GUID class. Please check if"));
        LogErr(_T("this device is present on your system and if it is a Power Manageable device."));
        returnVal = TPR_FAIL;
        goto CleanAndExit;
    }

    // Check if this is a display device
    g_fDisplay = FALSE;
    if(pDeviceInfo->guid == displayGUID)
    {
        LOG(_T("This is a Display device."));
        g_fDisplay = TRUE;
    }

    // Get handle for the device
    hDevice = GetDeviceHandle(szLegacyName);
    if((!hDevice) || (hDevice == INVALID_HANDLE_VALUE))
    {
        LogErr(_T("Could not get a handle to the device."), szLegacyName);
        returnVal = TPR_FAIL;
        goto CleanAndExit;
    }


    //Step1: Print out power caps and verify that D0 is supported
    // Get the power capabilities of the device and check if it is a parent device
    LOG(_T("Sending IOCTL_POWER_CAPABILITIES to device %s"), szLegacyName);
    if(!RequestDevice(hDevice,IOCTL_POWER_CAPABILITIES, NULL, 0, &pwrCaps, sizeof(pwrCaps), &dwBytesRet, &dwLastErr))
    {
        LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
        returnVal = TPR_FAIL;
        goto CleanAndExit;
    }
    else if(pwrCaps.Flags)
    {
        LOG(_T("Device %s is configured as a Parent. Skipping test."), szLegacyName);
        returnVal = TPR_SKIP;
        goto CleanAndExit;
    }

    //Log the power capabilities
    LogPowerCapabilities(&pwrCaps);


    //Check if D0 is supported
    if(!SupportedDx(D0, pwrCaps.DeviceDx))
    {
        LogErr(_T("Device Power State D0 is not supported for this device."));
        LogErr( _T("D0, which is the Fully ON state, must be supported by all devices"));
        returnVal = TPR_FAIL;
    }
    else
    {
        LOG(_T("D0, which is the Fully ON state, is supported by this device."));
    }


    //Step2: Get current power state and verify that it is valid and supported by the device
    LOG(_T(""));//blank line
    // Get the current device power state (origDx)
    LOG(_T("Sending IOCTL_POWER_GET to device %s"), szLegacyName);

    origDx = PwrDeviceUnspecified;
    if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &origDx, sizeof(origDx), &dwBytesRet, &dwLastErr))
    {
        LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
        returnVal = TPR_FAIL;
        goto CleanAndExit;

    }
    // Verify that the power state returned is between D0-D4.
    else if(!VALID_DX(origDx))
    {
        LogErr(_T("Device %s reports an invalid power state of D%u"), szLegacyName, origDx);
        returnVal = TPR_FAIL;
        goto CleanAndExit;
    }
    else
    {
        LOG(_T("Device %s reports a current power state of D%u"), szLegacyName, origDx);

        // Verify that the power state returned is advertised by the device in POWER_CAPABILITIES
        if(!SupportedDx(origDx, pwrCaps.DeviceDx))
        {
            LogErr(_T("This state is NOT supported by the device as seen from its power caps(DeviceDx = %u)."), pwrCaps.DeviceDx);
            returnVal = TPR_FAIL;
        }
    }


    //Step3: Cycle through all the Device Power States and verify setting supported/unsupported power states
    LOG(_T("")); //blank line
    LOG(_T("We will iterate through all the Device Power States now."));
    for(dx = D0; dx < PwrDeviceMaximum;dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
    {
        LOG(_T("")); //blank line
        LOG(_T("Test power state D%u --->"), dx);
        setDx = dx;

        // Try to put the driver in the new power state (setDx)
        LOG(_T("Sending IOCTL_POWER_SET with power state D%u to device %s"), setDx, szLegacyName);

        if(!RequestDevice(hDevice, IOCTL_POWER_SET, NULL, 0, &setDx, sizeof(setDx), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanAndExit;
        }

        // Query the power state to get the current state of the device (getDx)
        LOG(_T("Sending IOCTL_POWER_GET to device %s"), szLegacyName);

        getDx = PwrDeviceUnspecified;
        if(!RequestDevice(hDevice, IOCTL_POWER_GET, NULL, 0, &getDx, sizeof(getDx), &dwBytesRet, &dwLastErr))
        {
            LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
            returnVal = TPR_FAIL;
            goto CleanAndExit;
        }

        // Verify that the power state returned is between D0-D4.
        if(!VALID_DX(getDx))
        {
            LogErr(_T("Device %s reports an invalid power state of D%u"), szLegacyName, getDx);
            returnVal = TPR_FAIL;
        }
        else
        {
            LOG(_T("Device %s reports a current power state of D%u"), szLegacyName, getDx);

            // Check if the dx is a supported power state
            if(SupportedDx(dx, pwrCaps.DeviceDx))
            {
                LOG(_T("D%u is a supported state for this device, so we expect that it is set."), dx);

                // Check if we got what we set
                if(getDx != dx)
                {
                    LogErr(_T("The device was set to D%u and what we got is D%u"), dx, getDx);
                    returnVal = TPR_FAIL;
                }
                else
                {
                    LOG(_T("The reported power state matches the power state that was set."));
                }
            }
            else
            {
                LOG(_T("D%u is an unsupported state for this device, so we expect that it is not set."), dx);

                // Verify that the unsupported value was not set
                if(getDx == dx)
                {
                    LogErr(_T("The device was set to D%u, which is not expected."), getDx);
                    returnVal = TPR_FAIL;
                }
            }
        }
    }

    // Now put the driver back into its original power state
    LOG(_T("")); //blank line
    LOG(_T("Sending IOCTL_POWER_SET with original power state D%u to device %s"), origDx, szLegacyName);

    if(!RequestDevice(hDevice, IOCTL_POWER_SET, NULL, 0, &origDx, sizeof(origDx), &dwBytesRet, &dwLastErr))
    {
        LogErr(_T("The Ioctl call failed. GetLastError is %d."), dwLastErr);
        returnVal = TPR_FAIL;
    }

CleanAndExit:
    if(!CloseDeviceHandle(hDevice))
    {
        LogErr(_T("Error closing handle to the device."));
        returnVal = TPR_FAIL;
    }

    LOG(_T("")); //Blank line
    LOG(_T("***** End of Test *****"));

    return returnVal;
}
