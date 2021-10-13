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
#include <windows.h>
#include <devload.h>
#include "KModeTUD.h"


#define TESTUTILDRIVE_REG_ENTRY  _T("\\Drivers\\BuiltIn\\TestUtilDrv")

HANDLE LoadTestUtilDriver(VOID){

    // Local variable declarations
    HANDLE hGen = INVALID_HANDLE_VALUE;

    //create regpath name
    HKEY hKDrv = NULL;
    DWORD dwDisp = 0;
    DWORD dwRet = ERROR_SUCCESS;
    dwRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY, 0, NULL, 0, 0, NULL,
                                        &hKDrv, &dwDisp);
    if(dwRet == ERROR_SUCCESS && hKDrv != NULL && dwDisp == REG_CREATED_NEW_KEY){//succeeded
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): RegEntry %s has been created!"), 
        TESTUTILDRIVE_REG_ENTRY));
    }
    else if(dwRet == ERROR_SUCCESS && hKDrv != NULL && dwDisp == REG_OPENED_EXISTING_KEY){//already exists
        RETAILMSG(1, (TEXT("Function LoadTestUtilDriver(): RegEntry %s already exists!, will try again"), 
        TESTUTILDRIVE_REG_ENTRY));
        RegCloseKey(hKDrv);
        hKDrv = NULL;
        return hGen;
    }
    else{
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): Create Regkey %s failed!"), 
        TESTUTILDRIVE_REG_ENTRY));
        return hGen;
    }

    //create values needed under this reg key

    // 1: Dll Name
    TCHAR   szDllName[32] = TEXT("TestUtilDrv.dll");
    dwRet = RegSetValueEx(hKDrv,
                                     DEVLOAD_DLLNAME_VALNAME,
                                     0,
                                     REG_SZ,
                                     (PBYTE)szDllName,
                                     (wcslen(szDllName)+1)*sizeof(TCHAR));

    if(dwRet != ERROR_SUCCESS){
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): Create Regkey value %s failed!"), DEVLOAD_DLLNAME_VALNAME));
        RegCloseKey(hKDrv);
        RegDeleteKey(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY);
        return INVALID_HANDLE_VALUE;
    }

    // 2: Prefix
    TCHAR szDevName[6] = TEXT("TUD");
    dwRet = RegSetValueEx(hKDrv,
                                     DEVLOAD_PREFIX_VALNAME,
                                     0,
                                     REG_SZ,
                                     (PBYTE)szDevName,
                                     (wcslen(szDevName)+1)*sizeof(TCHAR));

    if(dwRet != ERROR_SUCCESS){
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): Create Regkey value %s failed!"), DEVLOAD_PREFIX_VALNAME));
        RegCloseKey(hKDrv);
        RegDeleteKey(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY);
        return INVALID_HANDLE_VALUE;
    }


    // 3: Index, make sure only one instance exist in the system
    DWORD dwIndex = 1;
    dwRet = RegSetValueEx(hKDrv,
                                     DEVLOAD_INDEX_VALNAME,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)&dwIndex,
                                     sizeof(DWORD));

    if(dwRet != ERROR_SUCCESS){
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): Create Regkey value %s failed!"), DEVLOAD_INDEX_VALNAME));
        RegCloseKey(hKDrv);
        RegDeleteKey(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY);
        return INVALID_HANDLE_VALUE;
    }

    RegCloseKey(hKDrv);


    hGen = ActivateDeviceEx(TESTUTILDRIVE_REG_ENTRY, NULL, 0, NULL);
    if(hGen == NULL || hGen == INVALID_HANDLE_VALUE){
        RETAILMSG(1, (TEXT(" Function LoadTestUtilDriver(): ActivateDeviceEx call failed!dwErr = 0x%x"), GetLastError()));
        RegDeleteKey(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY);
        return INVALID_HANDLE_VALUE;
    }

    return hGen;
}


BOOL UnloadTestUtilDriver(HANDLE hGen)
{

    if(hGen == NULL)
        return FALSE;
    
    // Attempt to de-register the device driver
    BOOL   fRet = DeactivateDevice( hGen);

    if(fRet ==  TRUE){
        RegDeleteKey(HKEY_LOCAL_MACHINE, TESTUTILDRIVE_REG_ENTRY);
    }
    else{
        RETAILMSG(1, (TEXT("Unload TestUtilDrivd.dll failed, error = 0x%x!"), 
        GetLastError()));
    }

    return fRet;
}

HANDLE OpenTestUtilDriver(){

    // Open device
    HANDLE hFile = CreateFile(
                            TEXT("TUD1:"),
                            GENERIC_READ|GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            0 );

    if( INVALID_HANDLE_VALUE == hFile )
    {
        // Unable to open device.
        RETAILMSG(1, (TEXT("CreateFile(TUD1:) FAILed: GetLastError() = %ld"), GetLastError()));
    }

    return hFile;
}



