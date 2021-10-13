//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

//
// Created by:  Steve Schrock
// Modified by:  Wei Chen
//
#include <windows.h>
#include <svsutil.hxx>
#include <auto_xxx.hxx>
#include <usbfnioctl.h>
#include <string.h>
#include <devload.h>

#define NET2280_REG_FORCE_FULL_SPEED_VAL    _T("ForceFullSpeed")

#define UFN_BUSDRV_NOACTION     0
#define UFN_BUSDRV_FORCEFS      1
#define UFN_BUSDRV_CLRFORCEFS   2

#define UFN_CLNDRV_NET2280      1
#define UFN_CLNDRV_SC2410   2
#define UFN_CLNDRV_COTULLA  3
#define UFN_CLNDRV_ALCHEMY  4
#define UFN_CLNDRV_MAINSTONE    5

HANDLE
GetUfnController(
    )
{
    HANDLE hUfn = NULL;
    BYTE rgbGuidBuffer[sizeof(GUID) + 4]; // +4 since scanf writes longs
    LPGUID pguidBus = (LPGUID) rgbGuidBuffer;
    LPCTSTR pszBusGuid = _T("E2BDC372-598F-4619-BC50-54B3F7848D35");

    // Parse the GUID
    int iErr = _stscanf(pszBusGuid, SVSUTIL_GUID_FORMAT, SVSUTIL_PGUID_ELEMENTS(&pguidBus));
    if (iErr == 0 || iErr == EOF)
        return INVALID_HANDLE_VALUE;    

    // Get a handle to the bus driver
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);
    ce::auto_handle hf = FindFirstDevice(DeviceSearchByGuid, pguidBus, &di);

    if (hf != INVALID_HANDLE_VALUE) {
        hUfn = CreateFile(di.szBusName, GENERIC_READ, FILE_SHARE_READ, NULL, 
            OPEN_EXISTING, 0, NULL);
    }
    else {
        NKDbgPrintfW(_T("No available UsbFn controller!\r\n"));
    }

    return hUfn;
}


VOID
CloseUfnController(
    HANDLE hUfn
    )
{
    if(hUfn != INVALID_HANDLE_VALUE)
        CloseHandle(hUfn);
}


DWORD
ChangeClient(
    HANDLE hUfn,
    LPCTSTR pszNewClient
    )
{
    
    if(hUfn == INVALID_HANDLE_VALUE || pszNewClient == NULL)
        return ERROR_INVALID_PARAMETER;
        

    DWORD dwRet = ERROR_SUCCESS;
    UFN_CLIENT_NAME ucn;
    _tcscpy(ucn.szName, pszNewClient);
    BOOL fSuccess = DeviceIoControl(hUfn, IOCTL_UFN_CHANGE_CURRENT_CLIENT, &ucn, sizeof(ucn), NULL, 0, NULL, NULL);
    
    if (fSuccess) {
        UFN_CLIENT_INFO uci;
        memset(&uci, 0, sizeof(uci));

        DWORD cbActual;
        fSuccess = DeviceIoControl(hUfn, IOCTL_UFN_GET_CURRENT_CLIENT, NULL, 0, &uci, sizeof(uci), &cbActual, NULL);
        if(fSuccess == FALSE || _tcsicmp(uci.szName, pszNewClient) != 0)
            return ERROR_GEN_FAILURE;

        if (uci.szName[0]) {
            NKDbgPrintfW(_T("Changed to client \"%s\"\r\n"), uci.szName);
        }
        else {
            NKDbgPrintfW(_T("There is now no current client\r\n"));
        }   
    }
    else {
        dwRet = GetLastError();
        NKDbgPrintfW(_T("Could not change to client \"%s\" error %d\r\n"), pszNewClient, dwRet);
    }

    return dwRet;
}


BOOL
AddForceFullSpeed(BOOL bFull){

    TCHAR   szRegPath[MAX_PATH] = _T("\\Drivers\\BuiltIn\\PCI\\Template\\NET2280");
    HKEY hKey = NULL;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, 0, &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }
    
    DWORD status;
    DWORD dwVal = 1;
    if(bFull == FALSE){
        dwVal = 0;
    }
    status = RegSetValueEx(hKey,
                                        NET2280_REG_FORCE_FULL_SPEED_VAL,
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }


    RegCloseKey(hKey);

    wcscpy(szRegPath,  _T("\\Drivers\\BuiltIn\\PCI\\Instance\\NET22801"));
    hKey = NULL;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, 0, &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }
    
    status = RegSetValueEx(hKey,
                                        NET2280_REG_FORCE_FULL_SPEED_VAL,
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }


    RegCloseKey(hKey);


    Sleep(300);

    return TRUE;

}


BOOL
AddRegEntryForUfnDevice(LPCTSTR pszUFNName, LPCTSTR pszDllName, DWORD dwVendorID, DWORD dwProdID, LPCTSTR pszPrefixName){
    if(pszUFNName == NULL || pszDllName == NULL)
        return FALSE;

    TCHAR   szRegPath[MAX_PATH] = _T("\\Drivers\\USB\\FunctionDrivers\\");

    //check length
    if((wcslen(pszUFNName)+wcslen(szRegPath))  > MAX_PATH){
        return FALSE;
    }

    wcscat(szRegPath, pszUFNName);

    DWORD dwTemp;
    HKEY hKey = NULL;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, NULL, 0, 0,
                       NULL, &hKey, &dwTemp) != ERROR_SUCCESS) {
        return FALSE;
    }
    
    DWORD status;
    //add dll name
    status = RegSetValueEx(hKey,
                                        DEVLOAD_DLLNAME_VALNAME,
                                        0,
                                        DEVLOAD_DLLNAME_VALTYPE,
                                        (PBYTE)pszDllName,
                                        sizeof(TCHAR)*(_tcslen(pszDllName) + 1));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    //add prefix name
    if(pszPrefixName != NULL){
        status = RegSetValueEx(hKey,
                                            DEVLOAD_PREFIX_VALNAME,
                                            0,
                                            DEVLOAD_PREFIX_VALTYPE,
                                            (PBYTE)pszPrefixName,
                                            sizeof(TCHAR)*(_tcslen(pszPrefixName) + 1));
        if (status != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return FALSE;
        }
    }
    
    // add vendor ID
    DWORD dwVal = dwVendorID;
    status = RegSetValueEx(hKey,
                                        _T("Vendor"),
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
       return FALSE;
    }

    // add product ID
    dwVal = dwProdID;
    status = RegSetValueEx(hKey,
                                        _T("Product"),
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    RegCloseKey(hKey);
    Sleep(300);

    return TRUE;

}

BOOL
ReloadNet2280Drv(){

    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

       //try to open loopback driver handle
    HANDLE hFile = CreateFile(L"UFL1:", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hFile == INVALID_HANDLE_VALUE){
        NKDbgPrintfW(_T("Open device UFL1: failed!"));
        return FALSE;
    }

    if(GetDeviceInformationByFileHandle(hFile, &di) == FALSE){
        NKDbgPrintfW(_T("GetDeviceInformationByDeviceHandle call failed!"));
        CloseHandle(hFile);
        return FALSE;
    }

    CloseHandle(hFile);
    
    if(di.hParentDevice == NULL){
        NKDbgPrintfW(_T("UFL1: does not have a valid parent device handle!!!"));
        return FALSE;
    }

    DEVMGR_DEVICE_INFORMATION diPar;
    memset(&diPar, 0, sizeof(diPar));
    diPar.dwSize = sizeof(diPar);
    //get net2280's handle
    if(GetDeviceInformationByDeviceHandle(di.hParentDevice, &diPar) == FALSE){
        NKDbgPrintfW(_T("GetDeviceInformationByDeviceHandle call failed!"));
        return FALSE;
    }

    if(diPar.hParentDevice == NULL){
        NKDbgPrintfW(_T("net2280 does not have a valid parent device handle!!!"));
        return FALSE;
    }
    if(diPar.szBusName[0] == 0){
        NKDbgPrintfW(_T("net2280 does not have a valid bus name!!!"));
        return FALSE;
    }

    TCHAR szNet2280Name[64] = {0};
    wcscpy(szNet2280Name, &diPar.szBusName[5]);

    //get net2280's parent device handle
    HANDLE hBusEnum = diPar.hParentDevice;
    memset(&diPar, 0, sizeof(diPar));
    diPar.dwSize = sizeof(diPar);
    if(GetDeviceInformationByDeviceHandle(hBusEnum, &diPar) == FALSE){
        NKDbgPrintfW(_T("GetDeviceInformationByDeviceHandle call failed!"));
        return FALSE;
    }

    if(diPar.szBusName[0] == 0){
        NKDbgPrintfW(_T("net2280's parent device does not have a valid bus name!!!"));
        return FALSE;
    }
    
    //try open file handle for busenum
    HANDLE hBusEnumFile = CreateFile(diPar.szBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hBusEnumFile == INVALID_HANDLE_VALUE){
        NKDbgPrintfW(_T("Open bus enum device %s failed!"), diPar.szBusName);
        return FALSE;
    }
    

    //now try to deactivate net2280
    BOOL bRet = DeviceIoControl(hBusEnumFile, 
                                                     IOCTL_BUS_DEACTIVATE_CHILD, 
                                                     LPVOID(szNet2280Name), 
                                                     (wcslen(szNet2280Name)+1)*sizeof(TCHAR), 
                                                     NULL, 0, NULL, NULL);

    if(bRet != TRUE){
        NKDbgPrintfW(_T("IOCTL_BUS_DEACTIVATE_CHILD call failed!"));
        CloseHandle(hBusEnumFile);
        return FALSE;
    }

    Sleep(2000);

    //now try to activate net2280
    bRet = DeviceIoControl(hBusEnumFile, 
                                                     IOCTL_BUS_ACTIVATE_CHILD, 
                                                     LPVOID(szNet2280Name), 
                                                     (wcslen(szNet2280Name)+1)*sizeof(TCHAR), 
                                                     NULL, 0, NULL, NULL);

    CloseHandle(hBusEnumFile);

    if(bRet != TRUE){
        NKDbgPrintfW(_T("IOCTL_BUS_ACTIVATE_CHILD call failed!"));
        return FALSE;
    }

    Sleep(3000);
    return TRUE;

}

BOOL ParseCmdLine(TCHAR * pCmdLine, PUCHAR puFSSetting, PUCHAR puDrv)
{
	
    if(pCmdLine == NULL || puFSSetting == NULL || puDrv == NULL) 
        return FALSE;

    //set default values first
    *puFSSetting = UFN_BUSDRV_NOACTION;
    *puDrv = UFN_CLNDRV_NET2280;
    
    //skip blank spaces
    while((pCmdLine[0]) == _T(' ') && pCmdLine[0] != 0){
        pCmdLine++;
    }

    if(pCmdLine[0] == 0) //empty command line
        return FALSE;

    //search "-"
    if(pCmdLine[0] != _T('-')) {//syntax error //
        return FALSE;
    }

    pCmdLine ++;
    //skip blank spaces
    while((pCmdLine[0]) == _T(' ')){
        pCmdLine++;
    }

    switch(pCmdLine[0]){
        case _T('f'):
        case _T('F'): 
            //we can force busdriver to be reloaded only on netchip2280
            if(*puDrv == UFN_CLNDRV_NET2280){
                *puFSSetting = UFN_BUSDRV_FORCEFS;
            }
            return TRUE;
        case _T('c'):
        case _T('C'): 
            //we can force busdriver to be reloaded only on netchip2280
            if(*puDrv == UFN_CLNDRV_NET2280){
                *puFSSetting = UFN_BUSDRV_CLRFORCEFS;
            }
            return TRUE;
        case _T('n'):
        case _T('N'): 
            //netchip2280
            *puDrv = UFN_CLNDRV_NET2280;
            if(pCmdLine[1] == _T('f') || pCmdLine[1] == _T('F')){
                *puFSSetting = UFN_BUSDRV_FORCEFS;
            }
            else if (pCmdLine[1] == _T('c') || pCmdLine[1] == _T('C')){
                *puFSSetting = UFN_BUSDRV_CLRFORCEFS;
            }
            return TRUE;
        case _T('a'):
        case _T('A'): 
            //Alchemy
            *puDrv = UFN_CLNDRV_ALCHEMY;
            return TRUE;
        case _T('s'):
        case _T('S'): 
            //Samsung SC2410
            *puDrv = UFN_CLNDRV_SC2410;
            return TRUE;
        case _T('x'):
        case _T('X'): 
            //Xscale cotulla
            *puDrv = UFN_CLNDRV_COTULLA;
            return TRUE;
        case _T('m'):
        case _T('M'): 
            //Mainstone
            *puDrv = UFN_CLNDRV_MAINSTONE;
            return TRUE;
                
        default:
        	return TRUE;
        }

    return TRUE;
}


int
WINAPI
WinMain (
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPTSTR      lpCmdLine,
    int         nCmdShow
    )
{

    UCHAR uFSSetting = UFN_BUSDRV_NOACTION;
    UCHAR uWhichDrv = UFN_CLNDRV_NET2280;

    //parse command line
    ParseCmdLine(lpCmdLine, &uFSSetting, &uWhichDrv);

    //setup ForceFullSpeed value if neccessary
    BOOL bRet = TRUE;
    if(uFSSetting == UFN_BUSDRV_FORCEFS){
        bRet = AddForceFullSpeed(TRUE); 
    }
    else if(uFSSetting == UFN_BUSDRV_CLRFORCEFS){
        bRet = AddForceFullSpeed(FALSE);
    }
    
    if(bRet == FALSE){
        NKDbgPrintfW(_T("Can not set or reset ForceFullSpeed Item in registry!"));
        return -1;
    }

    switch (uWhichDrv){
        case UFN_CLNDRV_NET2280:
            NKDbgPrintfW(_T("NetChip2280 dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("net2280lpbk.dll"), 0x17cc, 0x2280, _T("UFL"));
            break;
        case UFN_CLNDRV_COTULLA:
            NKDbgPrintfW(_T("Xscale (Cotulla) dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("cotullalpbk.dll"), 0x045e, 0x0301, _T("UFL"));
            break;
        case UFN_CLNDRV_SC2410:
            NKDbgPrintfW(_T("Samsung (SC2410) dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("sc2410lpbk.dll"), 0x045e, 0x00ce, _T("UFL"));
            break;
        case UFN_CLNDRV_MAINSTONE:
            NKDbgPrintfW(_T("Mainstone dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("ms2lpbk.dll"), 0x045e, 0x00ce, _T("UFL"));
            break;
        case UFN_CLNDRV_ALCHEMY:
            NKDbgPrintfW(_T("Alchemy dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("alchlpbk.dll"), 0x0, 0x0, _T("UFL"));
            break;
        default://this could not happen
            return -1;
    }

    //get ufn bus driver handle
    ce::auto_handle hUfn = GetUfnController();
    if (hUfn == INVALID_HANDLE_VALUE) {
        return -1;
    }

    //swith to the desired data loopback driver
    ChangeClient(hUfn, _T("USBTest"));

    //reload ufn bus driver if needed. 
    if(uFSSetting != UFN_BUSDRV_NOACTION){
        Sleep(2000);
        ReloadNet2280Drv();
        Sleep(2000);

        //redo the switch to make sure correct client driver is loaded
        hUfn = GetUfnController();
        if (hUfn == INVALID_HANDLE_VALUE) {
            return -1;
        }
        ChangeClient(hUfn, _T("USBTest"));
    } 
    
    return 0;
}

