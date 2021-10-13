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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
//
//
// Reg utility for changing the driver registry to load the test driver
//
//

#include "globals.h"
#include <windows.h>
#include <main.h>

//////////////////////////////////////////////////////////////////////////////////////////////
// The tasks performed by setDriverName are outlined by a global variable, g_bMemDrvExists,
// and a function argument, szNewName, as follows:
// 1) When g_bMemDrvExists is true and szNewName is non-null, the following tasks are perfomed:
//    The driver's registry key is opened and its values are set.
//
// 2) When g_bMemDrvExists is true and szNewName is null, the following tasks are perfomed:
//    The driver's registry key is opened and its old-persisted values are restored.
//
// 3) When g_bMemDrvExists is false and szNewName is non-null, the following tasks are perfomed:
//    A new driver's registry key is created and its values are set (name, ioctl, kernel mode)
// 
// 4) When g_bMemDrvExists is false and szNewName is null, the following tasks are perfomed:
//    Either Class driver, client driver, or the driver registry key is deleted.
//
// Return Values:
//    Success:  ERROR_SUCCESS
//    Failure:  Nonzero erro code defined in Winerror.h
//
//////////////////////////////////////////////////////////////////////////////////////////////
LONG setDriverName(LPCTSTR szNewName, BOOL bInUserMode)
{
    LONG rVal = 0;
    LONG rVal2 = 0;
    DWORD dwSize = 0;
    DWORD dwtemp = 0;
    DWORD dwIOCTL = 4;
    DWORD dwDisp = 0;
    HKEY hKey = NULL;
    LPCTSTR lptstrMemPrefix = SDTST_DEVNAME;
    DWORD   dwMemIndex  = SDTST_DEVINDEX;

    if (g_bMemDrvExists)
    {
        switch (g_deviceType)
        {
            case SDDEVICE_SD:
                rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class"),0,0,&hKey);
                break;

            case SDDEVICE_SDHC:
                rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity"),0,0,&hKey);
                break;

            case SDDEVICE_MMC:
                rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class"),0,0,&hKey);
                break;

            case SDDEVICE_MMCHC:
                rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity"),0,0,&hKey);
                break;

            default:
                rVal = FALSE;
        }

        if (rVal != ERROR_SUCCESS)
        {
            dwtemp = GetLastError();
            Debug(TEXT("Registry Error: Unable to open SD /MMC registry."));
            Debug(TEXT("\tError Code: %d returned."),rVal);
            goto DONE;
        }

        dwSize = 256 * sizeof(TCHAR);
        // Set the new Driver Name
        if (szNewName != NULL)
        {
            // Set Driver Name
            rVal = RegQueryValueEx(hKey, TEXT("Dll"),NULL,NULL,(LPBYTE)g_szMemDrvName, &dwSize);
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to query Value \"DLL\" in registry. This is the name of the Driver DLL."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            rVal = RegSetValueEx(hKey, TEXT("Dll"),NULL,REG_SZ,(LPBYTE)szNewName,(_tcslen(szNewName) + 1) * sizeof(TCHAR));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"DLL\" in registry to test driver."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            // Set Driver Prefix
            rVal = RegQueryValueEx(hKey, TEXT("Prefix"),NULL,NULL,(LPBYTE)g_szMemPrefix, &dwSize);
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to query Value \"Prefix\" in registry."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            rVal = RegSetValueEx(hKey, TEXT("Prefix"),NULL,REG_SZ,(LPBYTE)lptstrMemPrefix, 4 * sizeof(TCHAR));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"PREFIX\" in registry to %s."), lptstrMemPrefix);
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            // Set Driver Index
            rVal = RegQueryValueEx(hKey, TEXT("Index"), NULL, NULL, (LPBYTE)&g_dwMemIndex, &dwSize);
            if (rVal == ERROR_SUCCESS)
            {
                //The Index value is set so, we save for later restore and set it with the SDTst version
                rVal = RegSetValueEx(hKey, TEXT("Index"), NULL, REG_DWORD, (LPBYTE)&dwMemIndex, sizeof(DWORD));
                if (rVal != ERROR_SUCCESS)
                {
                    Debug(TEXT("Registry Error: Unable to set Value \"Index\" in registry to %d."), dwMemIndex);
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
            else
            {
                //The index value is not present so, we use the default(1). No need to modify the registry
                Debug(TEXT("Registry Warning: Unable to query Value \"Index\" in registry."));
                Debug(TEXT("\tError Code: %d returned. Using default index (1)"),rVal);
                g_dwMemIndex = MAX_UINT32;
            }

            //leave a flag for the driver to enable/disable softblock
            if(g_UseSoftBlock)
            {
                Debug(TEXT("Trying to set Value \"UseSoftBlock\" in registry for test driver."));

                DWORD sb = 1;
                rVal = RegSetValueEx(hKey, TEXT("UseSoftBlock"),NULL,REG_DWORD,(LPBYTE)&sb,sizeof(DWORD));
                if (rVal != ERROR_SUCCESS)
                {
                    Debug(TEXT("Registry Error: Unable to set Value \"UseSoftBlock\" in registry for test driver."));
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
            else
            {
                rVal = RegDeleteValue(hKey, TEXT("UseSoftBlock"));

            }

            // set IOCTL
            rVal = RegSetValueEx(hKey, TEXT("IOCTL"),NULL,REG_DWORD,(LPBYTE)&dwIOCTL, sizeof(dwIOCTL));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"IOCTL\" in registry to %u."), dwIOCTL);
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

        }
        // Restore the old driver name
        else
        {
            // Set the Driver Name
            rVal = RegSetValueEx(hKey, TEXT("Dll"),NULL,REG_SZ,(LPBYTE)g_szMemDrvName,(_tcslen(g_szMemDrvName) + 1) * sizeof(TCHAR));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"DLL\" in registry to the original driver."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            // Set Driver Prefix
            rVal = RegSetValueEx(hKey, TEXT("Prefix"),NULL,REG_SZ,(LPBYTE)g_szMemPrefix, (_tcslen(g_szMemPrefix) + 1) * sizeof(TCHAR));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"PREFIX\" in registry to DSK."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            if (g_dwMemIndex != MAX_UINT32)
            {
                // We didn't used the default, then we must restore the original Driver Index
                rVal = RegSetValueEx(hKey, TEXT("Index"), NULL, REG_DWORD, (LPBYTE)&g_dwMemIndex, sizeof(REG_DWORD));
                if (rVal != ERROR_SUCCESS)
                {
                    Debug(TEXT("Registry Error: Unable to set Value \"Index\" in registry to %d."), g_dwMemIndex);
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }

            rVal = RegDeleteValue(hKey, TEXT("IOCTL"));
            if ( (rVal != ERROR_SUCCESS)&& (rVal != ERROR_FILE_NOT_FOUND))
            {
                Debug(TEXT("Registry Error: Unable to remove Value \"IOCTL\" in registry ."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }
            rVal = ERROR_SUCCESS;

            // set User/Kernel Mode Flag
            DWORD dwFlag = 0;
            dwSize = sizeof(DWORD);
            rVal = RegQueryValueEx(hKey, TEXT("Flags"),NULL, NULL,(LPBYTE)&dwFlag, &dwSize);

            if (!bInUserMode)
            {
                rVal = RegDeleteValue(hKey, TEXT("Flags"));
                if ( (rVal != ERROR_SUCCESS) && (rVal != ERROR_FILE_NOT_FOUND))
                {
                    Debug(TEXT("Registry Error: Unable to remove Value \"Flags\" in registry."));
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
                rVal = ERROR_SUCCESS;
            }
            else
            {
                dwFlag &= 10;
                // Test driver will be in Kernel mode
                rVal = RegSetValueEx(hKey, TEXT("Flags"),NULL,REG_DWORD,(LPBYTE)&dwFlag, sizeof(DWORD));
                if (rVal != ERROR_SUCCESS)
                {
                    Debug(TEXT("Registry Error: Unable to set Value \"Flags\" in registry %u."), dwFlag);
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
        }
    }
    else
    {
        if (szNewName == NULL)
        {
            if (g_bClassExists)
            {
                switch (g_deviceType)
                {
                    case SDDEVICE_SD:
                        rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class"));
                        break;

                    case SDDEVICE_SDHC:
                        rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity"));
                        break;

                    case SDDEVICE_MMC:
                        rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class"));
                        break;

                    case SDDEVICE_MMCHC:
                        rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity"));
                        break;

                    default:
                        rVal = FALSE;
                }

                if (rVal != ERROR_SUCCESS)
                {
                    dwtemp = GetLastError();
                    Debug(TEXT("Registry Error: Unable to delete SD/ MMC in registry."));
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
            else if (g_bClientDrvExists)
            {
                rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class"));
                if (rVal != ERROR_SUCCESS)
                {
                    dwtemp = GetLastError();
                    Debug(TEXT("Registry Error: Unable to delete \"HKEY_LOCAL_MACHINE\\Drivers\\SDCARD\\ClientDrivers\\Class\" in registry."));
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
            else
            {
                rVal = RegDeleteKey(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers"));
                if (rVal != ERROR_SUCCESS)
                {
                    dwtemp = GetLastError();
                    Debug(TEXT("Registry Error: Unable to delete \"HKEY_LOCAL_MACHINE\\Drivers\\SDCARD\\ClientDrivers\" in registry."));
                    Debug(TEXT("\tError Code: %d returned."),rVal);
                    goto DONE;
                }
            }
        }
        else
        {
            switch (g_deviceType)
            {
                case SDDEVICE_SD:
                    rVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class"),0,NULL,0,0,NULL,&hKey,&dwDisp);
                    break;

                case SDDEVICE_SDHC:
                    rVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity"),0,NULL,0,0,NULL,&hKey,&dwDisp);
                    break;

                case SDDEVICE_MMC:
                    rVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class"),0,NULL,0,0,NULL,&hKey,&dwDisp);
                    break;

                case SDDEVICE_MMCHC:
                    rVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity"),0,NULL,0,0,NULL,&hKey,&dwDisp);
                    break;

                default:
                    rVal = FALSE;
            }

            if (rVal != ERROR_SUCCESS)
            {
                dwtemp = GetLastError();
                Debug(TEXT("Registry Error: Unable to create SD/MMC in registry."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            rVal = RegSetValueEx(hKey, TEXT("Dll"),NULL,REG_SZ,(LPBYTE)szNewName,(_tcslen(szNewName) + 1) * sizeof(TCHAR));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"DLL\" in registry to test driver."));
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }

            rVal = RegSetValueEx(hKey, TEXT("IOCTL"),NULL,REG_DWORD,(LPBYTE)&dwIOCTL, sizeof(dwIOCTL));
            if (rVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to set Value \"IOCTL\" in registry to %u."), dwIOCTL);
                Debug(TEXT("\tError Code: %d returned."),rVal);
                goto DONE;
            }
        }
    }

DONE:
    if (hKey)
    {
        rVal2 = RegCloseKey(hKey);
        if (rVal2 != ERROR_SUCCESS)
        {
            Debug(TEXT("Registry Error: Unable to close the registry key."));
            Debug(TEXT("\tError Code: %d returned."),rVal2);
            if (rVal == ERROR_SUCCESS)
            {
                rVal = rVal2;
            }
        }
    }

    return rVal;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// The tasks performed by isMemoryDriverStandard are:
// Based on the g_deviceType, open the driver's registry key and query its Dll's value.
// If the Dll's value is equal to "SDMemory.dll" then the driver is standard.
//
// Return Values:
//    Success:  TRUE
//    Failure:  FALSE 
//
// Output argument (*rVal):
//     ERROR_SUCCESS or nonzero erro code defined in Winerror.h
//
//////////////////////////////////////////////////////////////////////////////////////////////
BOOL isMemoryDriverStandard(LONG *rVal)
{
    HKEY hKey = NULL;
    DWORD dwLen = 256;
    BOOL bRet = FALSE;
    TCHAR tcBuff[256];

    switch (g_deviceType)
    {
        case SDDEVICE_SD:
            *rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class"),0,0,&hKey);
            break;

        case SDDEVICE_SDHC:
            *rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity"),0,0,&hKey);
            break;

        case SDDEVICE_MMC:
            *rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class"),0,0,&hKey);
            break;

        case SDDEVICE_MMCHC:
            *rVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity"),0,0,&hKey);
            break;

        default:
            *rVal = FALSE;
    }

    if (*rVal != ERROR_SUCCESS)
    {
        Debug(TEXT("Registry Error: Unable to open SD/MMC in registry"));
        Debug(TEXT("\tError Code: %d returned."),rVal);
        return bRet;
    }

    dwLen = 256 * sizeof(TCHAR);
    *rVal = RegQueryValueEx(hKey, TEXT("Dll"),NULL,NULL,(LPBYTE)tcBuff, &dwLen);
    if (*rVal != ERROR_SUCCESS)
    {
        Debug(TEXT("Registry Error: Unable to query Value \"DLL\" in registry. This is the name of the Driver DLL."));
        Debug(TEXT("\tError Code: %d returned."),rVal);
        RegCloseKey(hKey);
        return bRet;
    }

    tcBuff[255] = TCHAR('\0');
    if (_tcscmp(tcBuff, _T("SDMemory.dll"))==0)
    {
        bRet = TRUE;
    }

    *rVal = RegCloseKey(hKey);
    if (*rVal != ERROR_SUCCESS)
    {
        Debug(TEXT("Registry Error: Unable to close the registry key."));
        Debug(TEXT("\tError Code: %d returned."),rVal);
    }
    
    return bRet;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// InitClientRegStateVars accesses four global variables: it reads g_deviceType value and sets
// the value of g_bMemDrvExists, g_bClassExists, and g_bClientDrvExists to true/false.
//
// It starts by settings the three global driver-flags to true, then  clear (set to false)
// each flag if its associated registry key entry is not present in the registry. It returns
// once a key is found in the registry, otherwise, it continues until all three are read.
//
//////////////////////////////////////////////////////////////////////////////////////////////
void InitClientRegStateVars()
{
    HKEY hKey = NULL;
    LONG lVal;
    g_bMemDrvExists = TRUE;
    g_bClassExists = TRUE;
    g_bClientDrvExists = TRUE;

    switch (g_deviceType)
    {
        case SDDEVICE_SD:
            lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class"),0,0,&hKey);
            break;

        case SDDEVICE_SDHC:
            lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity"),0,0,&hKey);
            break;

        case SDDEVICE_MMC:
            lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class"),0,0,&hKey);
            break;

        case SDDEVICE_MMCHC:
            lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity"),0,0,&hKey);
            break;

        default:
            lVal = FALSE;
    }

    if (lVal != ERROR_SUCCESS)
    {
        Debug(TEXT("Registry Error: Unable to open SD/MMC in registry"));
        Debug(TEXT("\tError Code: %d returned."), lVal);

        g_bMemDrvExists= FALSE;
        lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class"),0,0,&hKey);
        if (lVal != ERROR_SUCCESS)
        {
            Debug(TEXT("Registry Error: Unable to open \"HKEY_LOCAL_MACHINE\\Drivers\\SDCARD\\ClientDrivers\\Class\" in registry"));
            Debug(TEXT("\tError Code: %d returned."),lVal);

            g_bClassExists= FALSE;
            lVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("\\Drivers\\SDCARD\\ClientDrivers"),0,0,&hKey);
            if (lVal != ERROR_SUCCESS)
            {
                Debug(TEXT("Registry Error: Unable to open \"HKEY_LOCAL_MACHINE\\Drivers\\SDCARD\\ClientDrivers\" in registry"));
                Debug(TEXT("\tError Code: %d returned."),lVal);
                g_bClientDrvExists= FALSE;

            }
        }
    }

    if (hKey)
    {
        lVal = RegCloseKey(hKey);
        if (lVal != ERROR_SUCCESS)
        {
            Debug(TEXT("Registry Error: Unable to close the registry key."));
            Debug(TEXT("\tError Code: %d returned."),lVal);
        }
    }

}
