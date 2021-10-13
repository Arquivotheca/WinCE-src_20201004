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

#define UFN_BUSDRV_NOACTION             0x00
#define UFN_CLNDRV_NOCONFIG             0x00

#define UFN_BUSDRV_MASK                 0x03
#define UFN_CLNDRV_MASK                 0x0F
#define UFN_COMP_MASK                   0x10

#define UFN_BUSDRV_FORCEFS              0x01
#define UFN_BUSDRV_CLEARFS              0x02
#define UFN_BUSDRV_RELOAD               0x03

#define UFN_CLNDRV_NET2280              0x01
#define UFN_CLNDRV_SC2410               0x02
#define UFN_CLNDRV_COTULLA              0x03
#define UFN_CLNDRV_ALCHEMY              0x04
#define UFN_CLNDRV_MAINSTONE            0x05
#define UFN_CLNDRV_OMAP730              0x06
#define UFN_CLNDRV_THOR                 0x07
#define UFN_CLNDRV_CONFIG1              0x08
#define UFN_CLNDRV_CONFIG2              0x09
#define UFN_CLNDRV_NEWCONFIG            0x0A
#define UFN_CLNDRV_CONFIGPERF           0x0B




HANDLE
GetUfnController(
    )
{
    HANDLE hUfn = NULL;
    union {
        BYTE rgbGuidBuffer[sizeof(GUID) + 4]; // +4 since scanf writes longs
        GUID guidBus;
    } u = { 0 };
    LPGUID pguidBus = &u.guidBus;
    LPCTSTR pszBusGuid = _T("E2BDC372-598F-4619-BC50-54B3F7848D35");

    // Parse the GUID
    int iErr = _stscanf_s(pszBusGuid, SVSUTIL_GUID_FORMAT, SVSUTIL_PGUID_ELEMENTS(&pguidBus));
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
    LPCTSTR pszNewClient    )
{

    if(hUfn == INVALID_HANDLE_VALUE || pszNewClient == NULL)
        return ERROR_INVALID_PARAMETER;


    DWORD dwRet = ERROR_SUCCESS;
    UFN_CLIENT_NAME ucn;
    _tcscpy_s(ucn.szName, _countof(ucn.szName), pszNewClient);
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


BOOL CreateClientDriverList(VOID)
{
    TCHAR szRegPath[MAX_PATH] = _T("\\Drivers\\USB\\FunctionDrivers");
    HKEY hKey = NULL;
    HKEY hSubKey= NULL;
    BOOL bRet = FALSE ;

     if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegPath, 0, 0, &hKey) != ERROR_SUCCESS)
     {
        bRet=  FALSE;
        goto Error;
     }

     int i = 0 ;
     TCHAR szSubKey[30];
     DWORD dwSize = sizeof(szSubKey);

     while(RegEnumKeyEx(hKey,i,szSubKey,&dwSize,NULL,NULL,NULL,NULL) ==ERROR_SUCCESS)
     {
        if (RegOpenKeyEx(hKey, szSubKey, 0, 0, &hSubKey) != ERROR_SUCCESS)
        {
            bRet=  FALSE;
            goto Error;
        }

        if(_tcscmp(szSubKey,_T("CompositeFN"))== 0 )
        {
            TCHAR szClient[] =_T("USBTest");
            if(RegSetValueEx(hSubKey,
                                        _T("ClientDriverList"),
                                        0,
                                        REG_MULTI_SZ,
                                        (PBYTE)&szClient,
                                        sizeof(szClient)) != ERROR_SUCCESS)
            {
                bRet = FALSE;
                goto Error;
            }
            bRet = TRUE;
        }


        dwSize = sizeof(szSubKey);
        i++;

     }



Error:
      if(hKey)
          RegCloseKey(hKey);
      if(hSubKey)
          RegCloseKey(hSubKey);

      return bRet;

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

    wcscpy_s(szRegPath, _countof(szRegPath), _T("\\Drivers\\BuiltIn\\PCI\\Instance\\NET22801"));
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
AddRegEntryForUfnDevice(LPCTSTR pszUFNName, LPCTSTR pszDllName, DWORD dwVendorID, DWORD dwProdID,
                                                     LPCTSTR pszPrefixName,BOOL fComposite)
{
    if(pszUFNName == NULL || pszDllName == NULL)
        return FALSE;

    TCHAR   szRegPath[MAX_PATH] = _T("\\Drivers\\USB\\FunctionDrivers\\");

    //check length
    if((wcslen(pszUFNName)+wcslen(szRegPath))  > MAX_PATH){
        return FALSE;
    }

    wcscat_s(szRegPath, _countof(szRegPath), pszUFNName);

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
                                        _T("idVendor"),
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
                                        _T("idProduct"),
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    //add bcdDevice value
    dwVal = 0;
    status = RegSetValueEx(hKey,
                                        _T("bcdDevice"),
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwVal,
                                        sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    //manufacturer name
    TCHAR szManuName[]= _T("Generic Manufacturer");
    status = RegSetValueEx(hKey,
                                        _T("Manufacturer"),
                                        0,
                                        REG_SZ,
                                        (PBYTE)szManuName,
                                        sizeof(TCHAR)*(wcslen(szManuName)+1));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    //product name
    TCHAR szProductName[]= _T("Loopback Test Device");
    status = RegSetValueEx(hKey,
                                        _T("Product"),
                                        0,
                                        REG_SZ,
                                        (PBYTE)szProductName,
                                        sizeof(TCHAR)*(wcslen(szProductName)+1));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }


    DWORD dwDefaultClient =1 ;

    if(fComposite)
    {
        status = RegSetValueEx(hKey,
                                        _T("CompositeFn_DefaultClient"),
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&dwDefaultClient,
                                        sizeof(DWORD));
        if (status != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }

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
    wcscpy_s(szNet2280Name, _countof(szNet2280Name), &diPar.szBusName[5]);

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

BOOL ParseCmdLine(TCHAR * pCmdLine, PDWORD pdwFSSetting, PDWORD pdwDrv)
{

    if(pCmdLine == NULL || pdwFSSetting == NULL || pdwDrv == NULL)
        return FALSE;

    //skip blank spaces
    while((pCmdLine[0]) == _T(' ') && pCmdLine[0] != 0){
        pCmdLine++;
    }

    if(pCmdLine[0] == 0) //empty command line
        return FALSE;


   while(*pCmdLine!='\0')
   {
        //search "-"
        if(pCmdLine[0] != _T('-')) {//syntax error //
            return FALSE;
        }
        pCmdLine ++;

        //skip blank spaces
        while((pCmdLine[0]) == _T(' ')){
            pCmdLine++;
        }

        switch(pCmdLine[0])
        {
            case _T('f'):
            case _T('F'):
                //we can force busdriver to be reloaded only on netchip2280
                *pdwFSSetting |= UFN_BUSDRV_FORCEFS;
                break;
            case _T('h'):
            case _T('H'):
                //Clear Full Speed if previously set
                *pdwFSSetting |= UFN_BUSDRV_CLEARFS;
                break;
            case _T('1'):
                *pdwDrv |= UFN_CLNDRV_CONFIG1;
                break;
            case _T('2'):
                *pdwDrv |= UFN_CLNDRV_CONFIG2;
                break;
            case _T('3'):
                *pdwDrv |= UFN_CLNDRV_NEWCONFIG;
                break;
            case _T('c'):
            case _T('C'):
#ifndef LOAD_DEFAULT_COMPOSITE_FN
                *pdwDrv |= UFN_COMP_MASK;
#else
                *pdwDrv &= ~UFN_COMP_MASK;
#endif
                break;
            case _T('r'):
            case _T('R'):
                //we can force busdriver to be reloaded only on netchip2280
                *pdwFSSetting |= UFN_BUSDRV_RELOAD;
                break;
            case _T('n'):
            case _T('N'):
                //netchip2280
                *pdwDrv |= UFN_CLNDRV_NET2280;
                if(pCmdLine[1] == _T('f') || pCmdLine[1] == _T('F')){
                    *pdwFSSetting |= UFN_BUSDRV_FORCEFS;
                }
                else if (pCmdLine[1] == _T('c') || pCmdLine[1] == _T('C')){
                    *pdwFSSetting |= UFN_BUSDRV_CLEARFS;
                }
                break;
            case _T('a'):
            case _T('A'):
                //Alchemy
                *pdwDrv |= UFN_CLNDRV_ALCHEMY;
                break;
            case _T('s'):
            case _T('S'):
                //Samsung SC2410
                *pdwDrv |= UFN_CLNDRV_SC2410;
                break;
            case _T('x'):
            case _T('X'):
                //Xscale cotulla
                *pdwDrv |= UFN_CLNDRV_COTULLA;
                break;
            case _T('m'):
            case _T('M'):
                //Mainstone
                *pdwDrv |= UFN_CLNDRV_MAINSTONE;
                break;
            case _T('o'):
            case _T('O'):
                //Omap730
                *pdwDrv |= UFN_CLNDRV_OMAP730;
                return TRUE;
            case _T('t'):
            case _T('T'):
                //Thor
                *pdwDrv |= UFN_CLNDRV_THOR;
                break;
            case _T('P'):
            case _T('p'):
                // Loopback driver for USBPerf tests
                *pdwDrv |= UFN_CLNDRV_CONFIGPERF;
                break;
            default:
                //default values
                *pdwFSSetting = UFN_BUSDRV_NOACTION;
                *pdwDrv = UFN_CLNDRV_CONFIG1;
                break;
        }

        pCmdLine++;

        //skip blank spaces following each param
        while((pCmdLine[0]) == _T(' ')){
            pCmdLine++;
        }
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
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    DWORD dwFSSetting = UFN_BUSDRV_NOACTION;
#ifndef LOAD_DEFAULT_COMPOSITE_FN
    DWORD dwWhichDrv = UFN_CLNDRV_NOCONFIG;
#else
    DWORD dwWhichDrv = UFN_CLNDRV_NOCONFIG | UFN_COMP_MASK;
#endif
    BOOL bComposite = FALSE;

    //If Command Line is Present
    if(lpCmdLine != NULL && lpCmdLine[0] != 0)
    {
      if(!ParseCmdLine(lpCmdLine, &dwFSSetting, &dwWhichDrv))
      {
        NKDbgPrintfW(_T("Invalid Command Line parameters %s"), lpCmdLine);
        return -1;
      }
    }
    else
#ifndef LOAD_DEFAULT_COMPOSITE_FN
       dwWhichDrv =UFN_CLNDRV_CONFIG1;
#else
       dwWhichDrv =UFN_CLNDRV_CONFIG1 | UFN_COMP_MASK;
#endif


    //setup ForceFullSpeed value if neccessary
    BOOL bRet = TRUE;

    switch(dwFSSetting & UFN_BUSDRV_MASK)
    {
        case UFN_BUSDRV_FORCEFS:
            NKDbgPrintfW(_T("ForceFullSpeed "));
            bRet = AddForceFullSpeed(TRUE);
            break;
        case UFN_BUSDRV_CLEARFS:
            NKDbgPrintfW(_T("Clear FullSpeed "));
            bRet = AddForceFullSpeed(FALSE);
            break;
        case UFN_BUSDRV_RELOAD:
            NKDbgPrintfW(_T("Bus Driver will reload "));
            break;
        default:
            break;
    }

    if(bRet == FALSE){
        NKDbgPrintfW(_T("Can not set or reset ForceFullSpeed Item in registry!"));
        return -1;
    }

    if(dwWhichDrv & UFN_COMP_MASK)
   {
        bComposite = TRUE;
   }

    switch (dwWhichDrv & UFN_CLNDRV_MASK){
        case UFN_CLNDRV_NET2280:
            NKDbgPrintfW(_T("NetChip2280 dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("net2280lpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_COTULLA:
            NKDbgPrintfW(_T("Xscale (Cotulla) dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("cotullalpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_SC2410:
            NKDbgPrintfW(_T("Samsung (SC2410) dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("sc2410lpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_MAINSTONE:
            NKDbgPrintfW(_T("Mainstone dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("ms2lpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_ALCHEMY:
            NKDbgPrintfW(_T("Alchemy dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("alchlpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_OMAP730:
            NKDbgPrintfW(_T("OMAP730  dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("om730lpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_THOR:
            NKDbgPrintfW(_T("THOR dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("thorlpbk.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_CONFIG1:
         NKDbgPrintfW(_T("LpBkCfg1  dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("LpBkCfg1.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_CONFIG2:
         NKDbgPrintfW(_T("LpBkCfg2 dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("LpBkCfg2.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_NEWCONFIG:
            NKDbgPrintfW(_T("LpBkNewCfg dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("LpBkNewCfg.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        case UFN_CLNDRV_CONFIGPERF:
            NKDbgPrintfW(_T("LpBkPerfCfg dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("LpBkPerfCfg.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
        default:
            NKDbgPrintfW(_T("LpBkCfg1  dataloopback driver will be loaded!"));
            bRet = AddRegEntryForUfnDevice(_T("USBTest"), _T("LpBkCfg1.dll"), 0x045e, 0xffe0, _T("UFL"),bComposite);
            break;
    }

    //get ufn bus driver handle
    ce::auto_handle hUfn = GetUfnController();
    if (hUfn == INVALID_HANDLE_VALUE) {
        return -1;
    }


    //reload ufn bus driver if needed (-r\-R option).
    if((dwFSSetting & UFN_BUSDRV_MASK) == UFN_BUSDRV_RELOAD){
        Sleep(2000);
        ReloadNet2280Drv();
        Sleep(2000);

        //redo the switch to make sure correct client driver is loaded
        hUfn = GetUfnController();
        if (hUfn == INVALID_HANDLE_VALUE) {
            return -1;
        }
    }

   //switch to the desired data loopback driver
   if(bComposite)
   {
        if(! CreateClientDriverList())
        {
           NKDbgPrintfW(_T("CreateClientDriverList() Failed !"));
           return 0;
        }

        ChangeClient(hUfn, _T("CompositeFN"));
   }
   else
   {
       ChangeClient(hUfn, _T("USBTest"));
   }

   return 0;
}

