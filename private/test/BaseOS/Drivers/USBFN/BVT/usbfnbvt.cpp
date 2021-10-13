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

//
// Created by:  Dollys
// Modified by:  shivss
//
#include "tuxmain.h"


DWORD g_Suspend = SUSPEND_RESUME_DISABLED ;

// ----------------------------------------------------------------------------
//
// Debugger
//
// ----------------------------------------------------------------------------
#ifdef DEBUG

DBGPARAM dpCurSettings =
{
    TEXT("USB FN BVT"),
    NULL,
    0
};

#endif  // DEBUG

//******************************************************************************
//***** Test Helper Functions
//******************************************************************************
//
//******************************************************************************
//***** Helper function GetUfnController
//***** Purpose: Gets the USB FN controller's handle
//***** Params: None
//***** Returns: Handle if successful,INVALID_HANDLE_VALUE otherwise.
//******************************************************************************
HANDLE GetUfnController()
{
    HANDLE hRet = INVALID_HANDLE_VALUE;
    HANDLE hUfn = NULL;
    ce::auto_handle hDev;
    union {
        BYTE rgbGuidBuffer[sizeof(GUID) + 4]; // +4 since scanf writes longs
        GUID guidBus;
    } u = { 0 };

    LPGUID pguidBus = &u.guidBus;
    LPCTSTR pszBusGuid = _T("E2BDC372-598F-4619-BC50-54B3F7848D35");

    // Parse the GUID
    DWORD dwErr = _stscanf_s(pszBusGuid, SVSUTIL_GUID_FORMAT, SVSUTIL_PGUID_ELEMENTS(&pguidBus));
    if (dwErr == 0 || dwErr == EOF)
    {
        hRet=INVALID_HANDLE_VALUE;
        goto EndGetUfnController;
    }

    // Get a handle to the bus driver
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    hDev = FindFirstDevice(DeviceSearchByGuid, pguidBus, &di);

    if (hDev != INVALID_HANDLE_VALUE)
    {
        hUfn = CreateFile(di.szBusName, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, 0, NULL);
        hRet=hUfn;
    }
    else
    {
        hRet=INVALID_HANDLE_VALUE;
        LOG(_T("No available UsbFn controller!\r\n"));
    }
EndGetUfnController:
    return hRet;
}

//******************************************************************************
//***** Helper function CloseUfnController
//***** Purpose: Closes the USB FN controller's handle
//***** Params: Handle to be closed.
//***** Returns: None.
//******************************************************************************
VOID CloseUfnController( HANDLE hUfn )
{
    if(hUfn != INVALID_HANDLE_VALUE)
        CloseHandle(hUfn);
}

//******************************************************************************
//***** Helper function ChangeClient
//***** Purpose: Changes the USB FN client
//***** Params: USB FN controller handle, New Client str, and defualt/current choice for ioctl call
//***** Returns: DWORD status.
//******************************************************************************
DWORD ChangeClient( HANDLE hUfn, LPCTSTR pszNewClient, BOOL fCurrent )
{
    DWORD dwRet = ERROR_SUCCESS;

    if(hUfn == INVALID_HANDLE_VALUE || pszNewClient == NULL)
    {
        dwRet = ERROR_INVALID_PARAMETER;
        goto endCloseUfnController;
    }


    UFN_CLIENT_NAME ucn;
    _tcscpy_s(ucn.szName,(_countof(ucn.szName)) , pszNewClient);
    BOOL fSuccess = DeviceIoControl(hUfn,
                                                   (fCurrent == TRUE)?IOCTL_UFN_CHANGE_CURRENT_CLIENT:IOCTL_UFN_CHANGE_DEFAULT_CLIENT,
                                                   &ucn, sizeof(ucn), NULL, 0, NULL, NULL);

    if (fSuccess) {
        dwRet= ERROR_SUCCESS;
    }
    else
    {
        dwRet = GetLastError();
        FAILLOG(_T("%s call: could not change to client \"%s\" \r\n"),
                     (fCurrent == TRUE)?_T("IOCTL_UFN_CHANGE_CURRENT_CLIENT"):_T("IOCTL_UFN_CHANGE_DEFAULT_CLIENT"),
                     pszNewClient);
    }
endCloseUfnController:
    return dwRet;

}



//******************************************************************************
//***** Helper function RemoveRegEntryForUfnDevice
//***** Purpose: Remove registry entry for test function client
//***** Params: New Client str, client dll name, vendor id, product id, and prefix
//***** Returns: BOOL status.
//******************************************************************************
BOOL RemoveRegEntryForUfnDevice(LPCTSTR pszUFNName)
{
    BOOL bRet = TRUE;
    HKEY hKey = NULL;
    if(pszUFNName == NULL )
    {
        bRet = FALSE;
        goto endRemoveRegEntryForUfnDevice;
    }

    TCHAR   szRegPath[MAX_PATH] = _T("\\Drivers\\USB\\FunctionDrivers\\");

    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, 0, &hKey)
                == ERROR_SUCCESS)
        {
            if(RegDeleteKey(hKey, pszUFNName) == ERROR_SUCCESS)
            {
                bRet = TRUE;
            }
            RegCloseKey(hKey);
        }


endRemoveRegEntryForUfnDevice:

     return bRet;

}

//******************************************************************************
//***** Helper function AddRegEntryForUfnDevice
//***** Purpose: Adds registry entry for test function client
//***** Params: New Client str, client dll name, vendor id, product id, and prefix
//***** Returns: BOOL status.
//******************************************************************************
BOOL AddRegEntryForUfnDevice(LPCTSTR pszUFNName, LPCTSTR pszDllName, DWORD dwVendorID, DWORD dwProdID, LPCTSTR pszPrefixName)
{
    BOOL bRet = TRUE;
    HKEY hKey = NULL;
    if(pszUFNName == NULL || pszDllName == NULL)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    TCHAR   szRegPath[MAX_PATH] = _T("\\Drivers\\USB\\FunctionDrivers\\");

    //check length
    if((wcsnlen(pszUFNName,MAX_PATH-1)+wcsnlen(szRegPath,MAX_PATH-1))  > (MAX_PATH-1))
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    wcscat_s(szRegPath, MAX_PATH-1 , pszUFNName);

    DWORD dwTemp;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, NULL, 0, 0,
                       NULL, &hKey, &dwTemp) != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    DWORD status;
    //add dll name
    status = RegSetValueEx(hKey,
                            DEVLOAD_DLLNAME_VALNAME,
                            0,
                            DEVLOAD_DLLNAME_VALTYPE,
                            (PBYTE)pszDllName,
                            sizeof(TCHAR)*(_tcsnlen(pszDllName,(MAX_PATH-1)) + 1));
    if (status != ERROR_SUCCESS)
    {

        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    //add prefix name
    if(pszPrefixName != NULL){
        status = RegSetValueEx(hKey,
                                DEVLOAD_PREFIX_VALNAME,
                                0,
                                DEVLOAD_PREFIX_VALTYPE,
                                (PBYTE)pszPrefixName,
                                sizeof(TCHAR)*(_tcsnlen(pszPrefixName,MAX_PATH-1) + 1));
        if (status != ERROR_SUCCESS)
        {
            bRet = FALSE;
            goto endAddRegEntryForUfnDevice;
        }
    }

    // add vendor ID
    DWORD dwVal = dwVendorID;
    status = RegSetValueEx(hKey,
                            _T("idVendor"),
                            0,
                            REG_DWORD,
                            (PBYTE)&dwVal,
                            sizeof(DWORD));
    if (status != ERROR_SUCCESS)
    {
       bRet = FALSE;
       goto endAddRegEntryForUfnDevice;
    }

    // add product ID
    dwVal = dwProdID;
    status = RegSetValueEx(hKey,
                            _T("idProduct"),
                            0,
                            REG_DWORD,
                            (PBYTE)&dwVal,
                            sizeof(DWORD));
    if (status != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    //add bcdDevice value
    dwVal = 0;
    status = RegSetValueEx(hKey,
                            _T("bcdDevice"),
                            0,
                            REG_DWORD,
                            (PBYTE)&dwVal,
                            sizeof(DWORD));
    if (status != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    //manufacturer name
    TCHAR szManuName[]= _T("Generic Manufacturer");
    status = RegSetValueEx(hKey,
                            _T("Manufacturer"),
                            0,
                            REG_SZ,
                            (PBYTE)szManuName,
                            sizeof(TCHAR)*(wcsnlen_s(szManuName, _countof(szManuName))+1));
    if (status != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

    //product name
    TCHAR szProductName[]= _T("Loopback Test Device");
    status = RegSetValueEx(hKey,
                            _T("Product"),
                            0,
                            REG_SZ,
                            (PBYTE)szProductName,
                            sizeof(TCHAR)*(wcsnlen_s(szProductName,_countof(szProductName))+1));
    if (status != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto endAddRegEntryForUfnDevice;
    }

endAddRegEntryForUfnDevice:

    if(NULL != hKey)
        RegCloseKey(hKey);
	//TODO : Veirfy the sleep is enough on various platforms
    Sleep(300);
    return bRet;

}
//******************************************************************************
//***** Helper function SetupUfnTestReg
//***** Purpose: Calls the  AddRegEntryForUfnDevice for adding USBFNBVT as test function client
//***** Params: None
//***** Returns: BOOL status.
//******************************************************************************
BOOL SetupUfnTestReg(){
    BOOL bRet;
    LOG(_T("USBFNBVT Client   driver reg entry will be added!"));
    bRet = AddRegEntryForUfnDevice(_T("USBFNBVT"), _T("UsbFnBvt.dll"), 0x045e, 0xffe0, _T("BVT"));
    return bRet;

}


//******************************************************************************
//***** Helper function CleanUfnTestReg
//***** Purpose: Deletes the Test Registry Created
//***** Params: None
//***** Returns: BOOL status.
//******************************************************************************
BOOL CleanUfnTest()
{
    BOOL bRet;
    LOG(_T("USBFNBVT Client   driver reg entry will be deleted!"));
    bRet = RemoveRegEntryForUfnDevice(_T("USBFNBVT"));
    return bRet;

}


//******************************************************************************
//***** Helper function GetCurrentClient
//***** Purpose: Gets the current/default client set.
//***** Params: Handle to usb fn controller, ptr to client info struct, choice of defualt /current
//***** Returns: BOOL status.
//******************************************************************************
BOOL GetCurrentClient(HANDLE hUfn, PUFN_CLIENT_INFO puci, BOOL fCurrent)
{
    BOOL bRet = TRUE;
    if(hUfn == NULL || puci == NULL)
    {
        bRet = FALSE;
        goto endGetCurrentClient;
    }

    DWORD cbActual = 0;
    BOOL bStatus = DeviceIoControl(hUfn,
                                (fCurrent == TRUE)?IOCTL_UFN_GET_CURRENT_CLIENT:IOCTL_UFN_GET_DEFAULT_CLIENT,
                                 NULL, 0, puci, sizeof(*puci), &cbActual, NULL);
    if(bStatus == FALSE)
    {
        if(fCurrent)
        {
            FAIL("IOCTL_UFN_GET_CURRENT_CLIENT call failed!");
        }
        else
        {
            FAIL("IOCTL_UFN_GET_DEFAULT_CLIENT call failed!");
        }
        bRet = FALSE;
        goto endGetCurrentClient;
    }

    if(cbActual != sizeof(UFN_CLIENT_INFO)){
        FAILLOG(_T("%s call, expected return size %d, actual return size %d"),
                        (fCurrent == TRUE)?_T("IOCTL_UFN_GET_CURRENT_CLIENT"):_T("IOCTL_UFN_GET_DEFAULT_CLIENT"),
                         sizeof(UFN_CLIENT_INFO), cbActual);
        bRet = FALSE;
    }
endGetCurrentClient:
    return bRet;
}

//******************************************************************************
//***** Test Functions return TPR_PASS/TPR_FAIL
//******************************************************************************

//******************************************************************************
//***** Test function TestUnloadReloadUfnBusDrv
//***** Purpose: Tests load/unload of the usb fn bus driver.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************
TESTPROCAPI TestUnloadReloadUfnBusDrv( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    HANDLE hUfn = NULL, hUfnParent = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestUnloadReloadUfnBusDrv;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn)){
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

    //Get Ufn controller driver's parent's handle
    if(GetDeviceInformationByFileHandle(hUfn, &di) == FALSE)
    {
        FAIL("GetDeviceInformationByDeviceHandle call failed!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }
    CloseHandle(hUfn);

    if(di.hParentDevice == NULL){
        FAIL("UFN host controller driver does not have a valid parent device handle!!!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    if(di.szBusName[0] == 0)
    {
        FAIL("UFN host controller driver does not have a valid bus name!!!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    //Get Ufn controller driver's parent's device information
    DEVMGR_DEVICE_INFORMATION diPar;
    memset(&diPar, 0, sizeof(diPar));
    diPar.dwSize = sizeof(diPar);
    if(GetDeviceInformationByDeviceHandle(di.hParentDevice, &diPar) == FALSE)
    {
        FAIL("GetDeviceInformationByDeviceHandle call failed!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    if(diPar.szBusName[0] == 0)
    {
        FAIL("UFN host controller driver's parent does not have a valid bus name!!!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    hUfnParent = CreateFile(diPar.szBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hUfnParent == INVALID_HANDLE_VALUE)
    {
        FAILLOG(_T("Open bus enum device %s failed!"), diPar.szBusName);
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }


    //now try to deactivate UFN controller driver
    BOOL fRet = DeviceIoControl(hUfnParent,
                                 IOCTL_BUS_DEACTIVATE_CHILD,
                                 LPVOID(di.szBusName),
                                 (wcsnlen(di.szBusName,MAX_PATH-1)+1)*sizeof(TCHAR),
                                 NULL, 0, NULL, NULL);

    if(fRet != TRUE)
    {
        ERR("IOCTL_BUS_DEACTIVATE_CHILD call failed!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }
	//TODO : Veirfy the sleep is enough on various platforms
    Sleep(5000);
    //verify htat we should not be able to access UFN controller driver now
    hUfn = GetUfnController();
    if(!INVALID_HANDLE(hUfn))
    {
        ERR("should NOT be able to access UFN controller driver now!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

    //now try to reactivate UFN controller driver
    fRet = DeviceIoControl(hUfnParent,
                             IOCTL_BUS_ACTIVATE_CHILD,
                             LPVOID(di.szBusName),
                             (wcsnlen(di.szBusName,MAX_PATH-1)+1)*sizeof(TCHAR),
                             NULL, 0, NULL, NULL);

    if(fRet != TRUE)
    {
        ERR("IOCTL_BUS_ACTIVATE_CHILD call failed!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }
    //TODO : Veirfy the sleep is enough on various platforms
    Sleep(5000);
    //verify that we should be able to access UFN controller driver now
    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn)){
        ERR("Should be able to access UFN controller driver now!");
        dwRet = TPR_FAIL;
        goto endTestUnloadReloadUfnBusDrv;
    }

endTestUnloadReloadUfnBusDrv:
    if(NULL!=hUfnParent)
        CloseHandle(hUfnParent);
    if(NULL!=hUfn)
        CloseHandle(hUfn);

    return dwRet;

}


//******************************************************************************
//***** Test function TestEnumClient
//***** Purpose: Tests enumeration of all available FN clients.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************

TESTPROCAPI TestEnumClient( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    ce::auto_handle hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestEnumClient;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn))
    {
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestEnumClient;
    }

    //get ready
    BOOL fRet = DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS_SETUP, NULL, 0, NULL, 0, NULL, NULL);
    if(fRet != TRUE)
    {
        ERR("IOCTL_UNF_ENUMERATE_CLIENTS_SETUP call failed!");
        dwRet = TPR_FAIL;
        goto endTestEnumClient;
    }

    //enum clients
    UFN_CLIENT_INFO uci;
    DWORD cbActual;
    fRet= DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS, NULL, 0, &uci, sizeof(uci), &cbActual, NULL);
    while (fRet) {
        if(cbActual != sizeof(uci))
        {
            FAILLOG(_T("IOCTL_UFN_ENUMERATE_CLIENTS Call: expected size is %d, actual return size is %d"), sizeof(uci), cbActual);
            dwRet = TPR_FAIL;
            goto endTestEnumClient;
        }
        LOG(_T("Available Client %s - \"%s\"\r\n"), uci.szName, uci.szDescription);
        fRet= DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS, NULL, 0, &uci, sizeof(uci), &cbActual, NULL);
    }

    if(GetLastError() != ERROR_NO_MORE_ITEMS)
    {
        FAILLOG(_T("IOCTL_UFN_ENUMERATE_CLIENTS Call: expected error return is ERROR_NO_MORE_ITEM, actual return is 0x%x"), GetLastError());
        dwRet = TPR_FAIL;
        goto endTestEnumClient;
    }
endTestEnumClient:
    if(NULL!=hUfn)
        CloseHandle(hUfn);
    return dwRet;

}

//******************************************************************************
//***** Test function TestGetSetCurrentClient
//***** Purpose: Tests get /set of current Client.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************
// --------------------------------------------------------------------
TESTPROCAPI
TestGetSetCurrentClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    HANDLE hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestGetSetCurrentClient;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn))
    {
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }

    //get current client
    UFN_CLIENT_INFO uci;
    if(GetCurrentClient(hUfn, &uci, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }
    if (uci.szName[0])
    {
        LOG(_T("Current client is %s - \"%s\"\r\n"), uci.szName, uci.szDescription);
    }
    else {
        LOG(_T("There is no current client\r\n"));
        goto SETUP_TESTCLIENT; //skip client cleanup stage
    }

    //cleanup current client
    DWORD dwErr = ChangeClient(hUfn, _T(""), TRUE);
    if(dwErr != ERROR_SUCCESS){
        FAIL("ChangeClient failed!");
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }

    //verify there's no current client
    UFN_CLIENT_INFO uci2;
    if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }
    if (uci2.szName[0])
    {
        LOG(_T("Current client is %s, but supposed to be null\r\n"), uci2.szName);
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }

SETUP_TESTCLIENT:

    LOG(_T("Now change to client USBFNBVT"));

    dwErr = ChangeClient(hUfn, _T("USBFNBVT"), TRUE);
    if(dwErr != ERROR_SUCCESS)
    {
        FAIL("ChangeClient failed!");
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }

    //verify current client
    if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }
    if (!uci2.szName[0]  || wcscmp(_T("USBFNBVT"), uci2.szName))
        {
            FAILLOG(_T("Current client is %s, but supposed to be USBFNBVT\r\n"), uci2.szName[0]?uci2.szName:_T("n/a"));
            dwRet = TPR_FAIL;
            goto endTestGetSetCurrentClient;
        }
	//TODO : Veirfy the sleep is enough on various platforms
    Sleep(4000);
    LOG(_T("Now restore the original client "));

    //now restore original client
    dwErr = ChangeClient(hUfn, uci.szName, TRUE);
    if(dwErr != ERROR_SUCCESS)
    {
        FAIL("ChangeClient failed!");
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }

    //verify current client
    if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetCurrentClient;
    }
    if ((uci2.szName[0]  && wcscmp(uci.szName, uci2.szName)) ||
            (uci.szName[0] == 0 && uci2.szName[0] != 0))
        {
            FAILLOG(_T("Current client is %s, but supposed to be %s\r\n"),
                            uci2.szName[0]?uci2.szName:_T("n/a"),  uci.szName[0]?uci.szName:_T("n/a"));
            dwRet = TPR_FAIL;
            goto endTestGetSetCurrentClient;
        }
endTestGetSetCurrentClient:
    if(NULL!=hUfn)
        CloseHandle(hUfn);
    return dwRet;

}

//******************************************************************************
//***** Test function TestGetSetDefaultClient
//***** Purpose: Tests get /set of default Client.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************
// --------------------------------------------------------------------
TESTPROCAPI
TestGetSetDefaultClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    HANDLE hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestGetSetDefaultClient;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn)){
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }

    //get current deafult client
    UFN_CLIENT_INFO uci;
    if(GetCurrentClient(hUfn, &uci, FALSE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }
    if (uci.szName[0]) {
        LOG(_T("Current default client is %s - \"%s\"\r\n"), uci.szName, uci.szDescription);
    }
    else {
        LOG(_T("There is no current default client\r\n"));
    }

    LOG(_T("Now change default client to USBFNBVT"));
    DWORD  dwErr = ChangeClient(hUfn, _T("USBFNBVT"), FALSE);
    if(dwErr != ERROR_SUCCESS)
    {
        FAIL("Change default Client failed!");
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }

    //verify current client
    UFN_CLIENT_INFO uci2;
    if(GetCurrentClient(hUfn, &uci2, FALSE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }
    if (!uci2.szName[0]  || wcscmp(_T("USBFNBVT"), uci2.szName))
    {
        LOG(_T("Current default client is %s, but supposed to be USBFNBVT\r\n"), uci2.szName[0]?uci2.szName:_T("n/a"));
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }


    //now restore original client
   //TODO : Veirfy the sleep is enough on various platforms
    Sleep(4000);
    LOG(_T("Now restore original default client"));
    dwErr = ChangeClient(hUfn, uci.szName[0]?uci.szName:_T(""), FALSE);
    if(dwErr != ERROR_SUCCESS)
    {
        FAIL("Change Default Client failed!");
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }

    //verify current client
    if(GetCurrentClient(hUfn, &uci2, FALSE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }
    if(uci.szName[0] == 0 && uci2.szName[0] != 0)
    {
        LOG(_T("Current client is %s, but supposed to be NULL\r\n"), uci2.szName);
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }

    if (!uci2.szName[0]  || wcscmp(uci.szName, uci2.szName))
    {
        LOG(_T("Current client is %s, but supposed to be %s\r\n"), uci2.szName[0]?uci2.szName:_T("n/a"), uci.szName);
        dwRet = TPR_FAIL;
        goto endTestGetSetDefaultClient;
    }
endTestGetSetDefaultClient:
    if( NULL != hUfn )
        CloseHandle(hUfn);
    return dwRet;

}


//******************************************************************************
//***** Test function TestIoctlAdditional
//***** Purpose: Tests send invalid params for ioctl call.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************

// --------------------------------------------------------------------
TESTPROCAPI
TestIoctlAdditional(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    HANDLE hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestIoctlAdditional;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn))
    {
        FAIL("Can not get handle for Ufn driver!");
        dwRet=TPR_FAIL;
        goto endTestIoctlAdditional;
    }

    //wrong code, but should not cause any problem
    __try{
        DeviceIoControl(hUfn, 0xFFFF, NULL, 0, NULL, 0, NULL, NULL);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        FAIL("Using invalid IOCTL code cause excpetion!");
        dwRet=TPR_FAIL;
        goto endTestIoctlAdditional;
    }
endTestIoctlAdditional:
    if( NULL != hUfn )
        CloseHandle(hUfn);
    return dwRet;

}
//******************************************************************************
//***** Test function TestEnumChangeClient
//***** Purpose: Tests enumerates all Clients and sets it the Current client
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************
// --------------------------------------------------------------------
TESTPROCAPI
TestEnumChangeClient(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwRet = TPR_PASS;
    ce::auto_handle hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestEnumAllClients;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn))
    {
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }
    //get current client
    UFN_CLIENT_INFO currentUci;
    if(GetCurrentClient(hUfn, &currentUci, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }
    if (currentUci.szName[0]) {
            LOG(_T("Current client is %s - \"%s\"\r\n"), currentUci.szName, currentUci.szDescription);
        }
        else {
            LOG(_T("There is no current client\r\n"));
        }

    //get ready
    BOOL fRet = DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS_SETUP, NULL, 0, NULL, 0, NULL, NULL);
    if(fRet != TRUE)
    {
        ERR("IOCTL_UNF_ENUMERATE_CLIENTS_SETUP call failed!");
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }

    //enumerate  and test changing to each of the clients
    UFN_CLIENT_INFO uci,uci2;
    DWORD cbActual,dwErr;
    fRet= DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS, NULL, 0, &uci, sizeof(uci), &cbActual, NULL);
    while (fRet)
    {
        if(cbActual != sizeof(uci))
        {
            FAILLOG(_T("IOCTL_UFN_ENUMERATE_CLIENTS Call: expected size is %d, actual return size is %d"), sizeof(uci), cbActual);
            dwRet = TPR_FAIL;
            goto endTestEnumAllClients;
        }

        LOG(_T("Available Client %s - \"%s\"\r\n"), uci.szName, (uci.szDescription[0]?uci.szDescription:_T(" ")));
           LOG(_T("Now change to client %s"),uci.szName);

        dwErr = ChangeClient(hUfn,uci.szName, TRUE);
        if(dwErr != ERROR_SUCCESS)
        {
            FAIL("ChangeClient failed!");
            dwRet = TPR_FAIL;
            goto endTestEnumAllClients;
        }

        //verify current client
        if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
        {
            dwRet = TPR_FAIL;
            goto endTestEnumAllClients;
        }
        if (!uci2.szName[0]  || wcscmp(uci.szName, uci2.szName))
            {
                FAILLOG(_T("Current client is %s, but supposed to be USBFNBVT\r\n"), uci2.szName[0]?uci2.szName:_T("n/a"));
                dwRet = TPR_FAIL;
                goto endTestEnumAllClients;
            }
		fRet= DeviceIoControl(hUfn, IOCTL_UFN_ENUMERATE_AVAILABLE_CLIENTS, NULL, 0, &uci, sizeof(uci), &cbActual, NULL);
    }//end while

    if(GetLastError() != ERROR_NO_MORE_ITEMS)
    {
        FAILLOG(_T("IOCTL_UFN_ENUMERATE_CLIENTS Call: expected error return is ERROR_NO_MORE_ITEM, actual return is 0x%x"), GetLastError());
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }
    LOG(_T("Now restore the original client "));

    //now restore original client
    dwErr = ChangeClient(hUfn, currentUci.szName, TRUE);
    if(dwErr != ERROR_SUCCESS)
    {
        FAIL("ChangeClient failed!");
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }

    //verify current client
    if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestEnumAllClients;
    }
    if ((uci2.szName[0]  && wcscmp(currentUci.szName, uci2.szName)) ||
            (currentUci.szName[0] == 0 && uci2.szName[0] != 0))
    {
            FAILLOG(_T("Current client is %s, but supposed to be %s\r\n"),
                            uci2.szName[0]?uci2.szName:_T("n/a"),  currentUci.szName[0]?currentUci.szName:_T("n/a"));
            dwRet = TPR_FAIL;
            goto endTestEnumAllClients;
    }
endTestEnumAllClients:
    if( NULL != hUfn )
        CloseHandle(hUfn);
    return dwRet;

}
//******************************************************************************
//***** Test function TestCurrentAndDefaultClientSuspendResume
//***** Purpose: Tests Default and current Clients post suspend and resume.
//***** Params: Tux test params
//***** Returns: Tux test Status.
//******************************************************************************
// --------------------------------------------------------------------
TESTPROCAPI
TestCurrentAndDefaultClientSuspendResume(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
	UNREFERENCED_PARAMETER(tpParam);
	UNREFERENCED_PARAMETER(lpFTE);
     if(g_Suspend == SUSPEND_RESUME_DISABLED )
   {
   	  LOG(TEXT("Suspend /Resume Not Enabled through Command Line"));
         return TPR_SKIP;
   }

    DWORD dwRet = TPR_PASS;
    HANDLE hUfn = NULL;
    if( TPM_EXECUTE != uMsg )
    {
        dwRet = TPR_NOT_HANDLED;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }

    hUfn = GetUfnController();
    if(INVALID_HANDLE(hUfn))
    {
        FAIL("Can not get handle for Ufn driver!");
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }

    UFN_CLIENT_INFO currentUci;
    //get current client
    if(GetCurrentClient(hUfn, &currentUci, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }
    if (currentUci.szName[0])
        {
            LOG(_T("Current client is %s - \"%s\"\r\n"), currentUci.szName, currentUci.szDescription);
        }
    else
        {
            LOG(_T("There is no current client\r\n"));
        }

    //get current deafult client
    UFN_CLIENT_INFO defaultUci;
    if(GetCurrentClient(hUfn, &defaultUci, FALSE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }
    if (defaultUci.szName[0])
    {
            LOG(_T("Current default client is %s - \"%s\"\r\n"), defaultUci.szName, defaultUci.szDescription);
    }
    else
    {
            LOG(_T("There is no current default client\r\n"));
    }

    //suspend-resume
    if(ERROR_NOT_SUPPORTED == TestSuspendAndResume())
    {
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }

    //verify default client
    UFN_CLIENT_INFO uci1,uci2;
    if(GetCurrentClient(hUfn, &uci1, FALSE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }
    if (!uci1.szName[0]  || wcscmp(defaultUci.szName, uci1.szName))
    {
        LOG(_T("Current default client is %s, but supposed to be %s\r\n"), uci1.szName[0]?uci1.szName:_T("n/a"),defaultUci.szName[0]?defaultUci.szName:_T("n/a"));
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }
    else
    {
        LOG(_T("Verified Current Default client is as expected %s\r\n"),uci1.szName[0]?uci1.szName:_T("n/a"));
    }

    //verify current client
    if(GetCurrentClient(hUfn, &uci2, TRUE) == FALSE)
    {
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }

    if ((uci2.szName[0]  && wcscmp(currentUci.szName, uci2.szName)) ||
        (currentUci.szName[0] == 0 && uci2.szName[0] != 0))
    {
        FAILLOG(_T("Current client is %s, but supposed to be %s\r\n"),
                        uci2.szName[0]?uci2.szName:_T("n/a"),  currentUci.szName[0]?currentUci.szName:_T("n/a"));
        dwRet = TPR_FAIL;
        goto endTestCurrentAndDefaultClientSuspendResume;
    }
    else
    {
        LOG(_T("Verified Current Function Client is as expected %s\r\n"),uci2.szName[0]?uci2.szName:_T("n/a"));
    }
endTestCurrentAndDefaultClientSuspendResume:
    if( NULL != hUfn )
        CloseHandle(hUfn);
    return dwRet;

}

