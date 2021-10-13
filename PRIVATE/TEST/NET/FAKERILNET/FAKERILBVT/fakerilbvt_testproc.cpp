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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>
#include "fakerilbvt_testproc.h"

#include <windev.h>
#include <ndis.h>
#include <msgqueue.h>

extern CKato *g_pKato;
extern SPS_SHELL_INFO *g_pShellInfo;

#define HEX(c)  ((c)<='9'?(c)-'0':(c)<='F'?(c)-'A'+0xA:(c)-'a'+0xA)

WCHAR g_szWiFiAdapterNameActualData[32];
WCHAR* g_szWiFiAdapter1 = NULL;


/*
g_pKato->Log(LOG_FAIL,
g_pKato->Log(LOG_ABORT,
g_pKato->Log(LOG_DETAIL,
g_pKato->Log(LOG_COMMENT
g_pKato->Log(LOG_SKIP
g_pKato->Log(LOG_NOT_IMPLEMENTED
*/


WCHAR *
MultiSzToSz
//
//	Turn a multi_sz string into a single string, with spaces between
//	the components of the string.
//
(
IN WCHAR *szMulti
)
{
    WCHAR *szCurrent = szMulti;

    while (*szCurrent != L'\0')
    {
        // Find the end of the current string
        szCurrent += wcslen(szCurrent);

        // Replace the string terminator with a space char
        *szCurrent = L' ';

        // Advance to the next component of the multi_sz
        szCurrent++;
    }

    return szMulti;
}





BOOL
DoNdisIOControl
//
//	Execute an NDIS IO control operation.
//
(
IN DWORD   dwCommand,
IN LPVOID  pInBuffer,
IN DWORD   cbInBuffer,
IN LPVOID  pOutBuffer,
IN DWORD   *pcbOutBuffer OPTIONAL
)
{
    HANDLE	hNdis;
    BOOL	bResult = FALSE;
    DWORD	cbOutBuffer;

    hNdis = CreateFile(DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_ALWAYS, 0, NULL);

    if (hNdis != INVALID_HANDLE_VALUE)
    {
        cbOutBuffer = 0;
        if (pcbOutBuffer)
            cbOutBuffer = *pcbOutBuffer;

        bResult = DeviceIoControl(hNdis,
                        dwCommand,
                        pInBuffer,
                        cbInBuffer,
                        pOutBuffer,
                        cbOutBuffer,
                        &cbOutBuffer,
                        NULL);

        if (bResult == FALSE)
            wprintf(L"IoControl result=%d", bResult);

        if (pcbOutBuffer)
            *pcbOutBuffer = cbOutBuffer;

        CloseHandle(hNdis);
    }
    else
    {
        g_pKato->Log(LOG_FAIL, L"CreateFile of '%s' failed, error=0x%X (%d)\n", DD_NDIS_DEVICE_NAME, GetLastError(), GetLastError());
    }

    return bResult;
}



BOOL
IsAdapterInstalled
// return TRUE if adapter 'XWIFI11B1' is installed.
(
IN WCHAR *szAdapter
)
{
    WCHAR	multiSzBuffer[256];
    DWORD	cbBuffer = sizeof(multiSzBuffer);

    if(!DoNdisIOControl(IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, &multiSzBuffer[0], &cbBuffer))
        return FALSE;
    MultiSzToSz(multiSzBuffer);
    return wcsstr(multiSzBuffer, szAdapter)!=NULL;
}



BOOL
_NdisConfigAdapterBind
//
// dwCommand =
// IOCTL_NDIS_BIND_ADAPTER
// IOCTL_NDIS_UNBIND_ADAPTER
(
IN WCHAR *szAdapter1,
IN DWORD dwCommand
)
{
    WCHAR multiSz[256];
    wsprintf(multiSz, L"%s", szAdapter1);
    multiSz[wcslen(multiSz)+1] = L'\0'; // Multi sz needs an extra null

    return DoNdisIOControl(
        dwCommand,
        multiSz,
        (wcslen(multiSz)+2) * sizeof(WCHAR),
        NULL,
        NULL);
}


#define NdisConfigAdapterBind(szAdapter) \
    _NdisConfigAdapterBind(szAdapter, IOCTL_NDIS_BIND_ADAPTER);

#define NdisConfigAdapterUnbind(szAdapter) \
    _NdisConfigAdapterBind(szAdapter, IOCTL_NDIS_UNBIND_ADAPTER);


#include "ndispwr.h"

#include <nkintr.h>



void
AdapterPowerOnOff
(
WCHAR *szAdapter,
BOOL bTurnPowerOn   // TRUE->will cause adapter power on
)
{
    if(!szAdapter)
        return;

    WCHAR multiSz[256];
    DWORD cbBuffer = sizeof(multiSz);
  
    wcscpy(multiSz, szAdapter);
    multiSz[wcslen(multiSz)+1] = L'\0'; // Multi sz needs an extra null

    // Inform ndispwr.dll on the power state of this adapter..
    HANDLE hNdisPwr = CreateFile(
                        NDISPWR_DEVICE_NAME,
                        0x00,
                        0x00,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                        (HANDLE)INVALID_HANDLE_VALUE);	

    if (hNdisPwr != INVALID_HANDLE_VALUE)
    {
        NDISPWR_SAVEPOWERSTATE SavePowerState;
        SavePowerState.pwcAdapterName = szAdapter;
        SavePowerState.CePowerState = bTurnPowerOn ? PwrDeviceUnspecified : D4;

        DeviceIoControl(
            hNdisPwr,
            IOCTL_NPW_SAVE_POWER_STATE,
            &SavePowerState,
            sizeof(NDISPWR_SAVEPOWERSTATE),
            NULL,
            0x00,
            NULL,
            NULL);    

        CloseHandle(hNdisPwr);
    }

    // Determine if we support power managment  
    CEDEVICE_POWER_STATE Dx = PwrDeviceUnspecified;
    WCHAR szPwrName[MAX_PATH];
    wsprintf(szPwrName, L"%s\\%s", PMCLASS_NDIS_MINIPORT, szAdapter);
    szPwrName[wcslen(szPwrName)+1] = 0;

    GetDevicePower(szPwrName, POWER_NAME, &Dx);
    BOOL bSupportsPowerManagment = (PwrDeviceUnspecified < Dx && PwrDeviceMaximum > Dx);

    
    if(!bSupportsPowerManagment)
        OutputDebugString(L"not fSupportsPowerManagment");

    // Issue the PwrDeviceUnspecified or D4 to the adapter if it supports power management.
    if (bSupportsPowerManagment)
    {
        if(ERROR_SUCCESS !=
            SetDevicePower(
                szPwrName, POWER_NAME,
                bTurnPowerOn ? PwrDeviceUnspecified : D4)
        )
            OutputDebugString(L"SetDevicePower() error");
    }

    DoNdisIOControl(
        bTurnPowerOn ? IOCTL_NDIS_BIND_ADAPTER : IOCTL_NDIS_UNBIND_ADAPTER,
        multiSz,
        (wcslen(multiSz)+2) * sizeof(WCHAR),
        NULL,
        NULL);
}






BOOL
_CardInsertNdisMiniportAdapter
// card insert
// same as actual card insert
// the ndis driver will be installed, and will start network operation
// all related UI will come up as expected
(
IN TCHAR* szAdapter
)
{
    g_pKato->Log(LOG_COMMENT, L"inserting %s\n", szAdapter);

    // ndisconfig adapter add ndummy ndummy1"
    WCHAR multiSz[256];
    wsprintf(multiSz, L"%s*%s1*", szAdapter, szAdapter);
    int iLen = wcslen(multiSz) + 1;
    for(WCHAR *p=multiSz; *p; p++)
    {
        if(*p == L'*')
            *p = L'\0';
    }
    return DoNdisIOControl(
        IOCTL_NDIS_REGISTER_ADAPTER,
        multiSz,
        iLen * sizeof(WCHAR),
        NULL,
        NULL);
}


int
CardInsertNdisMiniportAdapter
// card insert
// same as actual card insert
// the ndis driver will be installed, and will start network operation
// all related UI will come up as expected
(
IN TCHAR* szAdapter,
IN TCHAR* szIpAddress   // NULL=dhcp, otherwise this should be something like L"192.168.3.4"
)
{
    //for(WCHAR *p=szAdapter; *p; p++)
    //    *p = toupper(*p);

    g_pKato->Log(LOG_COMMENT, L"inserting %s\n", szAdapter);
    g_pKato->Log(LOG_COMMENT, L"default registry setting for %s\n", szAdapter);
    WCHAR szDriverFileName[MAX_PATH];
    wsprintf(szDriverFileName, L"%s.dll", szAdapter);
    g_pKato->Log(LOG_COMMENT, L"driver name = %s", szDriverFileName);

    //[HKEY_LOCAL_MACHINE\Comm\NDUMMY]
    //    "DisplayName"="ndis dummy miniport driver"
    //    "Group"="NDIS"
    //    "ImagePath"="ndummy.dll"
    WCHAR szText1[MAX_PATH];
    wsprintf(szText1, L"Comm\\%s", szAdapter);
    HKEY hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szText1, 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail");
        return -1;
    }
    wsprintf(szText1, L"NDIS %s miniport driver", szAdapter);
    RegSetValueEx(hKey1, L"DisplayName", 0, REG_SZ, (PBYTE)szText1, (wcslen(szText1)+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"Group", 0, REG_SZ, (PBYTE)L"NDIS", (wcslen(L"NDIS")+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"ImagePath", 0, REG_SZ, (PBYTE)szDriverFileName, (wcslen(szDriverFileName)+1)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\NDUMMY\Linkage]
    //    "Route"=multi_sz:"NDUMMY1"
    wsprintf(szText1, L"Comm\\%s\\Linkage", szAdapter);
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szText1, 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return -1;
    }
    wsprintf(szText1, L"%s1", szAdapter);
    szText1[wcslen(szText1)+1] = L'\0';
    RegSetValueEx(hKey1, L"Route", 0, REG_MULTI_SZ, (PBYTE)szText1, (wcslen(szText1)+2)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\NDUMMY1]
    //    "DisplayName"="ndis dummy miniport driver"
    //    "Group"="NDIS"
    //    "ImagePath"="ndummy.dll"
    wsprintf(szText1, L"Comm\\%s1", szAdapter);
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szText1, 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return -1;
    }
    wsprintf(szText1, L"NDIS %s miniport driver", szAdapter);
    RegSetValueEx(hKey1, L"DisplayName", 0, REG_SZ, (PBYTE)szText1, (wcslen(szText1)+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"Group", 0, REG_SZ, (PBYTE)L"NDIS", (wcslen(L"NDIS")+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"ImagePath", 0, REG_SZ, (PBYTE)szDriverFileName, (wcslen(szDriverFileName)+1)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\NDUMMY1\Parms]
    //    "BusNumber"=dword:0
    //    "BusType"=dword:0
    wsprintf(szText1, L"Comm\\%s1\\Parms", szAdapter);
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szText1, 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return -1;
    }
    DWORD dw1 = 0;
    RegSetValueEx(hKey1, L"BusNumber", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    RegSetValueEx(hKey1, L"BusType", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\NDUMMY1\Parms\TCPIP]
    //   "EnableDHCP"=dword:0
    //   "AutoCfg"=dword:0
    //   "IpAddress"="192.168.0.1"
    //   "Subnetmask"="255.255.0.0"
    wsprintf(szText1, L"Comm\\%s1\\Parms\\TCPIP", szAdapter);
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szText1, 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return -1;
    }

    if(szIpAddress)
    {   // static IP
        dw1 = 0;
        RegSetValueEx(hKey1, L"EnableDHCP", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"AutoCfg", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));

        g_pKato->Log(LOG_COMMENT, L"static IP = %s\n", szIpAddress);

        RegSetValueEx(hKey1, L"IpAddress", 0, REG_SZ, (PBYTE)szIpAddress, (wcslen(szIpAddress)+1)*sizeof(WCHAR));
        RegSetValueEx(hKey1, L"Subnetmask", 0, REG_SZ, (PBYTE)L"255.255.0.0", (wcslen(L"255.255.0.0")+1)*sizeof(WCHAR));
    }
    else
    {
        dw1 = 1;
        RegSetValueEx(hKey1, L"EnableDHCP", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"AutoCfg", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    }
    RegCloseKey(hKey1);

    _CardInsertNdisMiniportAdapter(szAdapter);
    return 0;
}





int
CardEjectNdisMiniportAdapter
// eject card, so the network adapter will disappear from the system
// same as actual card ejection
(
IN WCHAR *szAdapter1
)
{
    g_pKato->Log(LOG_COMMENT, L"ejecting %s\n", szAdapter1);

    // ndisconfig adapter unbind xwifi11b1
    _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_UNBIND_ADAPTER);

    // ndisconfig adapter del xwifi11b1
    _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_DEREGISTER_ADAPTER);

    return 0;
}






#include <Iphlpapi.h>


// use statically allocated memory block
// to reduce risk of mem leaking from this stress code.

#define MEM_BLOCK_8K_SIZE (1024*8)
UCHAR g_ucBulkMem[MEM_BLOCK_8K_SIZE];




void
PrintAdaptersInfo
()
{
    ULONG ulSizeAdapterInfo = MEM_BLOCK_8K_SIZE;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)g_ucBulkMem;
    DWORD dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
    if (dwStatus != ERROR_SUCCESS)
    {
        wprintf(L"no adapter\n");
        return;
    }

    wprintf(L"adapter info\n");
    while (pAdapterInfo != NULL)
    {
        wprintf(L"%hs :\n", pAdapterInfo->AdapterName);
        PIP_ADDR_STRING pAddressList = &(pAdapterInfo->IpAddressList);
        do
        {
            wprintf(L"   IpAddress = %hs\n", pAddressList->IpAddress.String);
            pAddressList = pAddressList->Next;
        } while (pAddressList != NULL);

        pAdapterInfo = pAdapterInfo->Next;
        wprintf(L"********\n");
    }
}





PIP_ADAPTER_INFO
GetAdapterInfo
// returns IP_ADAPTER_INFO* of adapter1
// if not found (i.e. adapter1 is not installed), returns NULL
// always use globally allocated g_ucBulkMem block
(
IN WCHAR *szAdapter1	// adapter name 'NE20001', 'CISCO1'
)
{
    ULONG ulSizeAdapterInfo = MEM_BLOCK_8K_SIZE;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)g_ucBulkMem;
    DWORD dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
    if (dwStatus != ERROR_SUCCESS)
        return NULL;
    char szAdapter1A[64];
    wcstombs(szAdapter1A, szAdapter1, 64);
    while(pAdapterInfo != NULL)
    {
        if(_stricmp(szAdapter1A, pAdapterInfo->AdapterName) == 0)
            return pAdapterInfo;
        pAdapterInfo = pAdapterInfo->Next;
    }
    return NULL;
}



BOOL
AdapterExist
// return TRUE if the given adapter exists
(
IN WCHAR *szAdapter1	// adapter name 'NE20001', 'CISCO1'
)
{
    return GetAdapterInfo(szAdapter1) != NULL;
}




BOOL
AdapterHasNoneZeroIpAddress
//
// return TRUE if adapter is bound to TCP/IP and has a valid IP address
//
(
IN WCHAR *szAdapter1	// adapter name 'NE20001', 'CISCO1'
)
{
    PIP_ADAPTER_INFO pAdapterInfo = GetAdapterInfo(szAdapter1);
    if(!pAdapterInfo)
        return FALSE;
    PIP_ADDR_STRING pAddressList = &(pAdapterInfo->IpAddressList);
    while (pAddressList != NULL)
    {
        if(strcmp(pAddressList->IpAddress.String, "0.0.0.0") != 0)
        {
            //wprintf(L"IpAddress ----> %hs\n", pAddressList->IpAddress.String);
//                    g_pKato->Log(LOG_COMMENT, L"%s has IpAddress ----> %hs", szAdapter1, pAddressList->IpAddress.String);
            return /*bAdapterHasIpAddress =*/ TRUE;
        }
        pAddressList = pAddressList->Next;
    }
    return FALSE;
}



BOOL
AdapterHasIpAddress
//
// return TRUE if adapter is bound to TCP/IP and has a valid IP address
//
(
IN WCHAR *szAdapter1,  // adapter name 'NE20001', 'CISCO1'
IN WCHAR *szIpAddr // IP address looking for, example:L"10.10.0.7", L"169.254.x.x"
)
{
    PIP_ADAPTER_INFO pAdapterInfo = GetAdapterInfo(szAdapter1);
    if(!pAdapterInfo)
        return FALSE;

    char szIpAddrA[64];
    wcstombs(szIpAddrA, szIpAddr, 64);
    char *pXdotX = strstr(szIpAddrA, ".x.x");
    BOOL bRangeOfAddr = (pXdotX!=NULL);
    if(bRangeOfAddr)
        *pXdotX = '\0';
    UINT uiCompareLen = strlen(szIpAddrA);

    PIP_ADDR_STRING pAddressList = &(pAdapterInfo->IpAddressList);
    while (pAddressList != NULL)
    {
        if(strncmp(pAddressList->IpAddress.String, szIpAddrA, uiCompareLen) == 0)
            return TRUE;
        pAddressList = pAddressList->Next;
    }
    return FALSE;
}



void
GetAdapterDefaultGatewayIpAddress
// return 1st gateway ip address
(
IN WCHAR *szAdapter1,  // adapter name 'NE20001', 'CISCO1'
IN OUT WCHAR szGatewayIpAddr[16]  // should be big enough to hold L"123.123.123.123"
)
{
    PIP_ADAPTER_INFO pAdapterInfo = GetAdapterInfo(szAdapter1);
    if(!pAdapterInfo)
    {
        *szGatewayIpAddr = L'\0';
        return;
    }

    PIP_ADDR_STRING pAddressList = &(pAdapterInfo->GatewayList);
    wsprintf(szGatewayIpAddr, L"%hs", pAddressList->IpAddress.String);
}





#define MAX_TRIAL 3000



BOOL
WaitForLoosingNetworkConnection
// return TRUE if the adapter loose connection within 300 secs (5 mins)
(
WCHAR *szAdapter1
)
{
    g_pKato->Log(LOG_COMMENT, L"WaitForLoosingNetworkConnection");
    for(unsigned i=0; i<MAX_TRIAL && AdapterHasNoneZeroIpAddress(szAdapter1); i++)
        Sleep(500);
    return (i < MAX_TRIAL); // time out
}


BOOL
WaitForGainingNetworkConnection
// return TRUE if the adapter gains connection within 30 secs
(
WCHAR *szAdapter1
)
{
    g_pKato->Log(LOG_COMMENT, L"WaitForGainingNetworkConnection");
    for(unsigned i=0; i<MAX_TRIAL && !AdapterHasNoneZeroIpAddress(szAdapter1); i++)
        Sleep(500);
    return (i < MAX_TRIAL);
}


#define SZ_FAKERIL_CMD_OUTPUT_FILE_NAME L"\\fakerilbvt-cmd-out.txt"

BOOL
RunCmd
// run and wait upto 10 minutes to finish
// return FALSE if fails.
(
LPWSTR lpCmdLine1
)
{
    if(!lpCmdLine1)
        return FALSE;

    // empty out the previous output text file
    FILE *fp0 = _wfopen(SZ_FAKERIL_CMD_OUTPUT_FILE_NAME, L"wt");
    fputws(L"", fp0);
    fclose(fp0);

    WCHAR szStdout[MAX_PATH];
    DWORD dwStdoutLen = MAX_PATH;
    GetStdioPathW(1, szStdout, &dwStdoutLen);
    SetStdioPathW(1, SZ_FAKERIL_CMD_OUTPUT_FILE_NAME);

    // command
    WCHAR lpCmdLine[256];
    wcscpy(lpCmdLine, lpCmdLine1);

    LPWSTR szCommand = lpCmdLine;
    while(*szCommand && _istspace(*szCommand))   // remove leading white-space
        szCommand++;
    //NKDbgPrintfW(L"d %s", szCommand);

    // arg
    LPWSTR szArg = NULL;       // separating command and arguments
    for(LPWSTR s=szCommand; *s; s++)
    {
        if(iswspace(*s))       // assume the first white-space delimits command and arguments
        {
            *s = L'\0';
            szArg = s + 1;
            break;
        }
    }

    // run command
    PROCESS_INFORMATION proc_info;
    memset((void*)&proc_info, 0, sizeof(proc_info));
    if(!CreateProcess(szCommand, szArg, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &proc_info))
    {
        DWORD dwLastError = ERROR_FILE_NOT_FOUND; //GetLastError();
        if(dwLastError==0x02)
            NKDbgPrintfW(L"error: %s [0x%X] not found (or missing dll)", szCommand, dwLastError);
        else
            NKDbgPrintfW(L"error: %s [0x%X]", szCommand, dwLastError);
        return FALSE;
    }
    WaitForSingleObject(proc_info.hProcess, 10*60*1000);
    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);
    SetStdioPathW(1, szStdout);

    FILE *fp = _wfopen(SZ_FAKERIL_CMD_OUTPUT_FILE_NAME, L"rt");
    if(!fp)
    {
        g_pKato->Log(LOG_COMMENT, L"[no output]");
        return TRUE;
    }
    WCHAR szLine1[256];
    while(!feof(fp))
    {
        if(fgetws(szLine1, 255, fp))
            g_pKato->Log(LOG_COMMENT, L"%s: %s", szCommand, szLine1);
    }
    fclose(fp);
    return TRUE;
}



void
RunTestCmd
(
WCHAR *szCmd
)
{
    if(szCmd && *szCmd)
    {
        WCHAR szNewCmd1[128];
        wcscpy(szNewCmd1, szCmd);

        if(!_wcsnicmp(szCmd, L"ping", 4))
        {
            if(wcsstr(szCmd, L"gateway"))
            {   // special case 'ping gateway'
                WCHAR szGatewayIpAddr[16];
                GetAdapterDefaultGatewayIpAddress(g_szWiFiAdapter1, szGatewayIpAddr);
                g_pKato->Log(LOG_COMMENT, L"gateway is %s", szGatewayIpAddr);

                wcscpy(wcsstr(szNewCmd1, L"gateway"), szGatewayIpAddr);
                wcscat(szNewCmd1, wcsstr(szCmd, L"gateway")+wcslen(L"gateway"));
            }
        }
        for(WCHAR *p=szNewCmd1; *p; p++) {
            if(*p==L'_')    *p = L' ';
        }

        g_pKato->Log(LOG_COMMENT, L"run command: %s", szNewCmd1);
        RunCmd(szNewCmd1);
    }
}





void __keybd_event(DWORD _K1, DWORD _K2)
{
    keybd_event((BYTE)_K1, (BYTE)MapVirtualKey(_K1, 0), 0, 0);
    if(_K2)
        __keybd_event(_K2);
    keybd_event((BYTE)_K1, (BYTE)MapVirtualKey(_K1, 0), KEYEVENTF_KEYUP, 0);
}







TESTPROCAPI
Test_Booted_OK
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Booted_OK"));
    g_pKato->Log(LOG_COMMENT, _T("PASS [as long as this test starts, OS booted fine]"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));


	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}



TESTPROCAPI
Test_Phone_UI
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Phone_UI"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));


	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}



TESTPROCAPI
Test_Phone_App
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Phone_App"));

    // find phone app from the registry
    WCHAR szCmd[256] = L"cprog";
    HKEY hKey1;
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Shell\\Rai\\:MSCPROG", 0, 0, &hKey1) == ERROR_SUCCESS)
    {
        DWORD dwType;
        DWORD dwcbData = sizeof(szCmd);
        if(RegQueryValueEx(hKey1, L"1", NULL, &dwType, (LPBYTE)szCmd, &dwcbData) == ERROR_SUCCESS)
        {
            if(dwType == REG_SZ)
            {
                g_pKato->Log(LOG_COMMENT, L"szCmd [from the registry] = %s", szCmd);
            }
        }
        RegCloseKey(hKey1);
    }

    g_pKato->Log(LOG_COMMENT, L"run phone command = %s", szCmd);
    WCHAR *szArg = NULL;
    PROCESS_INFORMATION proc_info;
    memset((void*)&proc_info, 0, sizeof(proc_info));
    if(!CreateProcess(szCmd, szArg, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &proc_info))
    {
        DWORD dwLastError = GetLastError();
#if 0
        LPVOID lpMsgBuf;
        FormatMessage(
             FORMAT_MESSAGE_ALLOCATE_BUFFER |
             FORMAT_MESSAGE_FROM_SYSTEM |
             FORMAT_MESSAGE_IGNORE_INSERTS,
             NULL,
             dwLastError,
             0, // Default language
             (LPTSTR) &lpMsgBuf,
             100,
             NULL 
        );

        if(lpMsgBuf)
        {
            g_pKato->Log(LOG_FAIL, L"error: %s [0x%X] %s", szCmd, dwLastError, (LPCTSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }
        else
#endif
        {   // system does not have FormatMessage component, we need SYSGEN_FMTMSG
            // so I provide some of my own messages.
            if(dwLastError==0x02)
                g_pKato->Log(LOG_FAIL, L"error: %s [0x%X] not found (or missing dll)", szCmd, dwLastError);
            else
                g_pKato->Log(LOG_FAIL, L"error: %s [0x%X]", szCmd, dwLastError);
        }
        return TPR_FAIL;
    }

    if(WaitForSingleObject(proc_info.hProcess, 1000) != WAIT_TIMEOUT)
    {   // launched but quited immediately --> means there was the first app.
        g_pKato->Log(LOG_COMMENT, L"this Phone is the second app. The first app is now in the foreground.");
        CloseHandle(proc_info.hProcess);
        CloseHandle(proc_info.hThread);
    }

    // now phone app will be in foreground
    // check it is completely activated
    WCHAR szWndTitle[256];
    for(int i=0; i<10; i++)
    {
        HWND hWndPhone = GetForegroundWindow();
        int n = GetWindowText(hWndPhone, szWndTitle, 256);
        szWndTitle[n] = L'\0';
        //wprintf(L"GetWindowText[%d] = %s\n", n, szWndTitle);

        if(wcscmp(szWndTitle, L"Phone") == 0)
            break;
        g_pKato->Log(LOG_COMMENT, L"warning: Phone not found");
        Sleep(50);
    }
    if(wcscmp(szWndTitle, L"Phone") != 0)
    {
        g_pKato->Log(LOG_FAIL, L"error: Phone does not start");
        return TPR_FAIL;
    }

    g_pKato->Log(LOG_COMMENT, L"Phone started.");   // and "Phone" window found

    if(lpFTE->dwUserData == 0x10)    // 0x10=check phone app from registry
        return TPR_PASS;

    if(lpFTE->dwUserData == 0x11)   // 0x11 = Can call through the phone?
    {
        // "1 -> [Enter]" calls '1'
        __keybd_event(L'1');
        __keybd_event(VK_RETURN);
        Sleep(5000);
        g_pKato->Log(LOG_COMMENT, L"Phone connected");
    }
    else if(lpFTE->dwUserData == 0x11)   // 0x20=disconnect
    {
        __keybd_event(VK_F4);
        g_pKato->Log(LOG_COMMENT, L"Phone disconnected");
    }

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}


#define MAX_10K_WCHAR 10000


TESTPROCAPI
Test_Net_ipconfig
// Got IP address?, // 0x10 = ip addr
// Sub netmask?"), // 0x11 = subnet mask
// Got default gateway?"), // 0x12 = default gateway
// Got 0.0.0.0 IP address?"), // 0x21 = ip addr should be 0.0.0.0
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Net_ipconfig"));

    if(!RunCmd(L"ipconfig"))
    {
        g_pKato->Log(LOG_FAIL, L"cannot run ipconfig");
        return TPR_FAIL;
    }

    WCHAR *szCmdOutText = (WCHAR*)LocalAlloc(LPTR, MAX_10K_WCHAR*sizeof(WCHAR));
    if(!szCmdOutText)
    {
        g_pKato->Log(LOG_FAIL, L"[memory error] szCmdOutText = (WCHAR*)LocalAlloc");
        return TPR_FAIL;
    }
    *szCmdOutText = L'\0';
    FILE *fp = _wfopen(SZ_FAKERIL_CMD_OUTPUT_FILE_NAME, L"rt");
    if(!fp)
    {
        g_pKato->Log(LOG_FAIL, L"[no output]");
    }
    else
    {
        WCHAR szLine1[256];
        while(!feof(fp))
        {
            if(fgetws(szLine1, 255, fp))
            {
                //g_pKato->Log(LOG_COMMENT, L"%s: %s", szCommand, szLine1);
                if(wcslen(szCmdOutText)+wcslen(szLine1)>=MAX_10K_WCHAR)
                    break;
                wcscat(szCmdOutText, szLine1);
            }
        }
        fclose(fp);
    }

/*
output file [text] should like this

[case 1] when no adapter exists

Windows IP configuration 

No IPv4 adapter found.




[case 2]

Ethernet adapter [XWIFI11B1]: 
	 IP Address ........ : 0.0.0.0
	 Subnet Mask ....... : 0.0.0.0

Tunnel adapter []: 
	 Interface Number .. : 4

Tunnel adapter [6to4 Pseudo-Interface]: 
	 Interface Number .. : 3

Tunnel adapter [Automatic Tunneling Pseudo-Interface]: 
	 Interface Number .. : 2


  
[case 3]

Ethernet adapter [XWIFI11B1]: 
	 IP Address ........ : 10.10.0.7
	 Subnet Mask ....... : 255.255.0.0
	 IP Address ........ : fe80::20a:41ff:feee:9a6d%6
	 Default Gateway ... : 10.10.0.1

Tunnel adapter []: 
	 Interface Number .. : 4

Tunnel adapter [6to4 Pseudo-Interface]: 
	 Interface Number .. : 3

Tunnel adapter [Automatic Tunneling Pseudo-Interface]: 
	 Interface Number .. : 2
	 IP Address ........ : fe80::5efe:10.10.0.7%2

	 DNS Servers........ : 10.10.0.1

*/

    if(wcsstr(szCmdOutText, L"No IPv4 adapter found"))
    {
        g_pKato->Log(LOG_FAIL, L"No IPv4 adapter found");
        LocalFree(szCmdOutText);
		return TPR_FAIL;
    }

    WCHAR *szT1 = NULL;
    switch(lpFTE->dwUserData)
    {
    case 0x10:  // Got IP address?, // 0x10 = ip addr
        szT1 = wcsstr(szCmdOutText, L"IP Address");
        while(*szT1 && (*szT1>=L' ') && !isdigit(*szT1))
            szT1++;
        if(*szT1)
        {
            g_pKato->Log(LOG_COMMENT, L"addr found = %s", szT1);
            if(wcscmp(szT1, L"0.0.0.0")==0)
                g_pKato->Log(LOG_FAIL, L"IP addr is NULL [fail]");
        }
        else
            g_pKato->Log(LOG_FAIL, L"No addr found");
        break;

    case 0x11:  // Sub netmask?", // 0x11 = subnet mask
        szT1 = wcsstr(szCmdOutText, L"Subnet Mask");
        while(*szT1 && (*szT1>=L' ') && !isdigit(*szT1))
            szT1++;
        if(*szT1)
            g_pKato->Log(LOG_COMMENT, L"'Subnet Mask' found = %s", szT1);
        else
            g_pKato->Log(LOG_FAIL, L"No 'Subnet Mask' found");
        break;

    case 0x12:  // Got default gateway?"), // 0x12 = default gateway
        szT1 = wcsstr(szCmdOutText, L"Default Gateway");
        while(*szT1 && (*szT1>=L' ') && !isdigit(*szT1))
            szT1++;
        if(*szT1)
        {
            g_pKato->Log(LOG_COMMENT, L"'Default Gateway' found = %s", szT1);
            if(wcscmp(szT1, L"0.0.0.0")==0)
                g_pKato->Log(LOG_FAIL, L"'Default Gateway' is NULL [fail]");
        }
        else
            g_pKato->Log(LOG_FAIL, L"No 'Default Gateway' found");
        break;

    case 0x21:  // Got 0.0.0.0 IP address?"), // 0x21 = ip addr should be 0.0.0.0
        szT1 = wcsstr(szCmdOutText, L"IP Address");
        while(*szT1 && (*szT1>=L' ') && !isdigit(*szT1))
            szT1++;
        if(*szT1)
        {
            g_pKato->Log(LOG_COMMENT, L"addr found = %s", szT1);
            if(wcscmp(szT1, L"0.0.0.0")==0)
                g_pKato->Log(LOG_COMMENT, L"IP addr is NULL [expected]");
            else
                g_pKato->Log(LOG_FAIL, L"IP addr is not NULL [fail]");
        }
        else
            g_pKato->Log(LOG_FAIL, L"No addr found");
        break;
    }

    LocalFree(szCmdOutText);
	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}




TESTPROCAPI
Test_Net_ping
/*
// 0x10 = ping 'gadget'
// 0x11 = ping 'gadget' 256 byte
// 0x12 = ping 'gadget' 1k byte
// 0x13 = ping 'gadget' 4k byte
// 0x14 = ping 'gadget' 8k byte
// 0x15 = ping 'gadget' 16k byte
// 0x26 = ping 'aqzxcvarfgv' [fail]
*/
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Net_ping"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));

    WCHAR* szPingCmd[] =
    {
        L"ping gadget",
        L"ping gadget -l 256",
        L"ping gadget -l 1024",
        L"ping gadget -l 4096",
        L"ping gadget -l 8192",
        L"ping gadget -l 16000",
        L"ping aqzxcvarfgv",
        L"ping cefaq",
    };
    UINT dwCmdId = lpFTE->dwUserData & 0x0F;
    if(dwCmdId > 6)
        dwCmdId = 0;

    if(!RunCmd(szPingCmd[dwCmdId]))
    {
        g_pKato->Log(LOG_FAIL, L"cannot run ping");
        return TPR_FAIL;
    }

    WCHAR *szCmdOutText = (WCHAR*)LocalAlloc(LPTR, MAX_10K_WCHAR*sizeof(WCHAR));
    if(!szCmdOutText)
    {
        g_pKato->Log(LOG_FAIL, L"[memory error] szCmdOutText = (WCHAR*)LocalAlloc");
        return TPR_FAIL;
    }
    *szCmdOutText = L'\0';
    FILE *fp = _wfopen(SZ_FAKERIL_CMD_OUTPUT_FILE_NAME, L"rt");
    if(!fp)
    {
        g_pKato->Log(LOG_FAIL, L"[no output]");
    }
    else
    {
        WCHAR szLine1[256];
        while(!feof(fp))
        {
            if(fgetws(szLine1, 255, fp))
            {
                //g_pKato->Log(LOG_COMMENT, L"%s: %s", szCommand, szLine1);
                if(wcslen(szCmdOutText)+wcslen(szLine1)>=MAX_10K_WCHAR)
                    break;
                wcscat(szCmdOutText, szLine1);
            }
        }
        fclose(fp);
    }
/*

[case 1]
ping gadget

Pinging Host gadget.redmond.corp.microsoft.com [157.54.6.93]
Reply from 157.54.6.93: Echo size=32 time=10ms TTL=128
Reply from 157.54.6.93: Echo size=32 time<1ms TTL=128
Reply from 157.54.6.93: Echo size=32 time<1ms TTL=128
Reply from 157.54.6.93: Echo size=32 time<1ms TTL=128


[case 2]
ping aqzxcvarfgv
Bad IP address (null).

*/
    WCHAR *szT1 = NULL;

    switch(lpFTE->dwUserData)
    {
    case 0x10:  // 0x10 = ping 'gadget'
    case 0x11:  // 0x11 = ping 'gadget' 256 byte
    case 0x12:  // 0x12 = ping 'gadget' 1k byte
    case 0x13:  // 0x13 = ping 'gadget' 4k byte
    case 0x14:  // 0x14 = ping 'gadget' 8k byte
    case 0x15:  // 0x15 = ping 'gadget' 16k byte
    case 0x17:  // 0x17 = ping 'cefaq'
        szT1 = wcsstr(szCmdOutText, L"Reply");
        if(szT1)
            g_pKato->Log(LOG_COMMENT, L"got reply [PASS]");
        else
            g_pKato->Log(LOG_FAIL, L"no reply [FALI]");
        break;

    case 0x26:  // 0x26 = ping 'aqzxcvarfgv' [fail]
        szT1 = wcsstr(szCmdOutText, L"Bad IP address (null)");
        if(szT1)
            g_pKato->Log(LOG_COMMENT, L"got 'Bad IP address (null)' [PASS]");
        else
            g_pKato->Log(LOG_FAIL, L"cannot find 'Bad IP address (null)' [FALI] [check the output]");
        break;
    }

    LocalFree(szCmdOutText);
	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}



#include <wininet.h>



TESTPROCAPI
Test_Net_Http
/*
// 0x10 = ping 'http://cefaq'
// 0x11 = get 'http://cefaq' page
*/
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, L"Test_Net_Http");
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));

	HINTERNET   hInternetSession;
	DWORD   dwBytesAvailable = 10000, dwBytesRead;

	hInternetSession = InternetOpen(TEXT("TEST IE"), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternetSession == NULL) 
	{
		g_pKato->Log(LOG_FAIL, L"InternetOpen() Failed.");
        return TPR_FAIL;
    }

	// Open the internet URL
	HINTERNET hInternet;

	hInternet = InternetOpenUrl(hInternetSession, L"http://cefaq", NULL, 0, 
		// Options for the HTTP request
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 
		0);
	if (hInternet == NULL) 
	{
		g_pKato->Log(LOG_FAIL, L"InternetOpenUrl failed: %d", GetLastError());
        InternetCloseHandle(hInternetSession);
        return TPR_FAIL;
	}

    if(lpFTE->dwUserData == 0x10)   // 0x10 = ping 'http://cefaq'
    {
        g_pKato->Log(LOG_COMMENT, L"Test_Net_Http ping [PASS]");
        g_pKato->Log(LOG_COMMENT, L"checked InternetOpenUrl()");
    }
    else if(lpFTE->dwUserData == 0x11)  // 0x11 = get 'http://cefaq'
    {
    	char *aBuffer = (char *)LocalAlloc(LPTR, dwBytesAvailable+1);
	    if (aBuffer == NULL) 
    	{
	    	g_pKato->Log(LOG_FAIL, L"Malloc(aBuffer) Failed");
            InternetCloseHandle(hInternetSession);
            return TPR_FAIL;
	    }

	    // Read from the webserver.
	    InternetReadFile(hInternet, aBuffer, dwBytesAvailable, &dwBytesRead);
	    aBuffer[dwBytesRead] = '\0';

        // check basic html syntax to make sure we got web content
        if(strstr(aBuffer, "<html>") && strstr(aBuffer, "<body>"))
            g_pKato->Log(LOG_COMMENT, L"Test_Net_Http loading page [PASS]");
        else
            g_pKato->Log(LOG_FAIL, L"Test_Net_Http loading page [FAIL]");
        LocalFree(aBuffer);
    }

	InternetCloseHandle(hInternetSession);

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}




TESTPROCAPI
Test_Net_ndisconfig
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_ndisconfig"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));

    if(!RunCmd(L"ndisconfig"))
    {
        g_pKato->Log(LOG_FAIL, L"cannot run ndisconfig");
        return TPR_FAIL;
    }

    WCHAR *szCmdOutText = (WCHAR*)LocalAlloc(LPTR, MAX_10K_WCHAR*sizeof(WCHAR));
    if(!szCmdOutText)
    {
        g_pKato->Log(LOG_FAIL, L"[memory error] szCmdOutText = (WCHAR*)LocalAlloc");
        return TPR_FAIL;
    }
    *szCmdOutText = L'\0';
    FILE *fp = _wfopen(SZ_FAKERIL_CMD_OUTPUT_FILE_NAME, L"rt");
    if(!fp)
    {
        g_pKato->Log(LOG_FAIL, L"[no output]");
    }
    else
    {
        WCHAR szLine1[256];
        while(!feof(fp))
        {
            if(fgetws(szLine1, 255, fp))
            {
                //g_pKato->Log(LOG_COMMENT, L"%s: %s", szCommand, szLine1);
                if(wcslen(szCmdOutText)+wcslen(szLine1)>=MAX_10K_WCHAR)
                    break;
                wcscat(szCmdOutText, szLine1);
            }
        }
        fclose(fp);
    }
/*
output file [text] should like this

Protocols: NDISUIO PPP TCPIP IRDA TCPIP6 NATIVEWIFI 
Adapters: DC211401 L2TP1 PPTP1 ASYNCMAC1
     Adapter Bindings
     ------- --------
     DC211401 NDISUIO TCPIP TCPIP6
     L2TP1 PPP
     PPTP1 PPP
     ASYNCMAC1 PPP 
*/

    WCHAR *szT1 = wcsstr(szCmdOutText, L"Adapters:");
    if(!szT1)
        g_pKato->Log(LOG_FAIL, L"ndisconfig not reporting adapters");
    WCHAR szT2[128];
    memcpy(szT2, szT1+wcslen(L"Adapters: "), 256);
    for(WCHAR *p=szT2; (*p>=L' ') && (p-szT2<128); p++) ;
    *p = L'\0';
    g_pKato->Log(LOG_COMMENT, L"adapters = %s", szT2);

    szT1 = wcsstr(szCmdOutText, L"Bindings");
    if(!szT1)
        g_pKato->Log(LOG_FAIL, L"ndisconfig not reporting bindings");

    BOOL bTcpIpBindingFound = FALSE;
    while(p!=szT2)
    {
        p--;
        while((*p==L' ') && (p!=szT2))
        {
            *p = L'\0';
            p--;
        }
        while((*p!=L' ') && (p!=szT2))
            p--;
        if(*p == L' ')
            p++;

        // p points last adapter among many found
        g_pKato->Log(LOG_COMMENT, L"checking adapter %s", p);
        szT1 = wcsstr(szCmdOutText, L"Bindings");
        szT1 = wcsstr(szT1, p);
        for(WCHAR *pEol = szT1; *pEol && *pEol>=L' '; pEol++) ;
        WCHAR wc1_backup = *pEol;
        *pEol = L'\0';
        if(wcsstr(szT1, L"TCPIP"))
        {
            bTcpIpBindingFound = TRUE;
            g_pKato->Log(LOG_COMMENT, L"adapter %s has TCPIP binding", p);
        }
        *pEol = wc1_backup;
        *p = L'\0';
    }

    if(bTcpIpBindingFound)
        g_pKato->Log(LOG_COMMENT, L"tcp binding found");
    else
        g_pKato->Log(LOG_FAIL, L"tcp binding NOT found");

    LocalFree(szCmdOutText);
	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}





TESTPROCAPI
Test_Net_Media
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test_Phone_UI"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));


	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}
