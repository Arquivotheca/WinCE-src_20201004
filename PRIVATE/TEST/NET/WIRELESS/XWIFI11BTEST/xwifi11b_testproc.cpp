//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "xwifi11b_testproc.h"

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




void
RunCmd
// run and wait upto 1 minutes
(
LPWSTR lpCmdLine1
)
{
    if(!lpCmdLine1)
        return;

    WCHAR lpCmdLine[256];
    wcscpy(lpCmdLine, lpCmdLine1);

    WCHAR szConsoleOutput[MAX_PATH] = L"";
    GetTempFileName(L"\\Temp", L"x", 0, szConsoleOutput);
    WCHAR szStdout[MAX_PATH];
    DWORD dwStdoutLen = MAX_PATH;
    GetStdioPathW(1, szStdout, &dwStdoutLen);
    SetStdioPathW(1, szConsoleOutput);

    // command
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
    }
    WaitForSingleObject(proc_info.hProcess, 1*60*1000);
    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);
    SetStdioPathW(1, szStdout);

    FILE *fp = _wfopen(szConsoleOutput, L"rt");
    if(!fp)
    {
        g_pKato->Log(LOG_COMMENT, L"[no output]");
        return;
    }
    WCHAR szLine1[256];
    while(!feof(fp))
    {
        if(fgetws(szLine1, 255, fp))
            g_pKato->Log(LOG_COMMENT, L"%s: %s", szCommand, szLine1);
    }
    fclose(fp);
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





// wireless related APIs

#include <eapol.h>
#include <wzcsapi.h>


WCHAR *g_szAuthModeString[] =
{
    L"Ndis802_11AuthModeOpen",
    L"Ndis802_11AuthModeShared",
    L"Ndis802_11AuthModeAutoSwitch",
    L"Ndis802_11AuthModeWPA",
    L"Ndis802_11AuthModeWPAPSK",
    L"Ndis802_11AuthModeWPANone",
};



void
AddSsidToThePreferredList
//
// insert given SSID to the preferred list
// zeroconfig will soon try to associate with this
//
(
IN WCHAR* szDeviceName, // [in] wireless network card device name, CISCO1
IN WCHAR* szPreferredSSID_withOption        // [in] SSID (name of the ad hoc wireless network) ex: CE-ADHOC1
                     //    options can come delimited by /
                     //    CEZZ           SSID=CEZZ, no WEP key (not private)
                     //    CEZZ/0/abcde   SSID=CEZZ, key index=0, key value="abcde"
                     //    MSFTWLAN/eap-tls/auto

    // first word = SSID
    // after ':', options how to connect the SSID
    // -adhoc means adhoc, otherwise infrastructure net
    // -wep:?, next one digit=key index 1-4, next word=key value
    // -wep:auto, key is given automatically by eap
    // -eap:*, eap methods (tls, peap)
    // -auth:*, authentication (open, shared)
    // -wpa:auto  WPA key is given automatically by EAP
    // -wpa:*, WPA with given key

    // CEADHOC1:-adhoc
    // CEOPEN
    // CESHARED:-wep:1:0x1234567890:-auth:shared
    // MSFTWLAN:-wep:auto-eap:tls
    // CE8021X:-wep:auto-eap:tls
    // CE8021X:-wep:auto-eap:peap
    // CEWPAPSK:-wpa:qwertyuiop
    // CEWPA:-wpa:auto-eap:tls
    // CEWPA:-wpa:auto-eap:peap

    //
    //    note on key
    //    1. key index is an integer ranging from 0 to 4
    //    2. key value could be given by either hexa value or ascii character
    //    3. you can use 40-bit (10-digits in hexa value or 5-chars in ascii char case) or
    //       104-bit (26-digits in hexa value or 13-chars in ascii char)
    //    4. if key value starts with 0x, it is considered as hexa value otherwise it is assumed ascii char
    //    5. 0x abcde

    // :-auth:
    // open, shared,wpa, wpapsk,wpanone
    // :-wep:auto-...
    // :no
    /*
typedef enum _NDIS_802_11_AUTHENTICATION_MODE
{
    Ndis802_11AuthModeOpen,
    Ndis802_11AuthModeShared,
    Ndis802_11AuthModeAutoSwitch,
    Ndis802_11AuthModeWPA,
    Ndis802_11AuthModeWPAPSK,
    Ndis802_11AuthModeWPANone,
    Ndis802_11AuthModeMax               // Not a real mode, defined as upper bound
} NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;
    */
)
{
    if(!szPreferredSSID_withOption || !*szPreferredSSID_withOption)
    {
        g_pKato->Log(LOG_COMMENT, L"NULL SSID");
        return;
    }

    WCHAR szSSID[64] = L"";
    for(UINT i=0; szPreferredSSID_withOption[i] && (i<MAX_SSID_LEN); i++)
    {
        szSSID[i] = szPreferredSSID_withOption[i];
        if(szSSID[i] == L'_')
            szSSID[i] = L' ';
        else if((szSSID[i] == L'/') || (szSSID[i] == L':'))
            break;
    }
    szSSID[i] = L'\0';
    g_pKato->Log(LOG_COMMENT, L"SSID = %s", szSSID);

    WZC_WLAN_CONFIG wzcConfig1;
    memset(&wzcConfig1, 0, sizeof(wzcConfig1));
    wzcConfig1.Length = sizeof(wzcConfig1);
    wzcConfig1.dwCtlFlags = 0;

    wzcConfig1.Ssid.SsidLength = i;
    for(i=0; i<wzcConfig1.Ssid.SsidLength; i++)
        wzcConfig1.Ssid.Ssid[i] = (char)szSSID[i];

    WCHAR *p;
    wzcConfig1.InfrastructureMode =
        wcsstr(szPreferredSSID_withOption, L":-adhoc") ? Ndis802_11IBSS : Ndis802_11Infrastructure;
    wzcConfig1.AuthenticationMode = Ndis802_11AuthModeOpen;
    if((p=wcsstr(szPreferredSSID_withOption, L":-auth"))!=NULL)
    {
        p += wcslen(L":-auth:");
        if(!_wcsnicmp(p, L"shared", 4))
            wzcConfig1.AuthenticationMode = Ndis802_11AuthModeShared;
        else if(!_wcsnicmp(p, L"wpapsk", 6))
            wzcConfig1.AuthenticationMode = Ndis802_11AuthModeWPAPSK;
        else if(!_wcsnicmp(p, L"wpanone", 6))
            wzcConfig1.AuthenticationMode = Ndis802_11AuthModeWPANone;
        else if(!_wcsnicmp(p, L"wpa", 3))
            wzcConfig1.AuthenticationMode = Ndis802_11AuthModeWPA;
        else
            g_pKato->Log(LOG_COMMENT, L"unknown auth");
    }
    g_pKato->Log(LOG_COMMENT, L"authentication : %s", g_szAuthModeString[wzcConfig1.AuthenticationMode]);

    wzcConfig1.KeyIndex = 0;                               // 0 is the per-client key, 1-N are the global keys
    wzcConfig1.KeyLength = 0;                              // length of key in bytes
    //wzcConfig1.KeyMaterial[WZCCTL_MAX_WEPK_MATERIAL];  // variable length depending on above field
    wzcConfig1.Privacy = Ndis802_11WEPDisabled;
    if((p=wcsstr(szPreferredSSID_withOption, L":-wep"))==NULL)
    {
        g_pKato->Log(LOG_COMMENT, L"no WEP");
    }
    else
    {
        g_pKato->Log(LOG_COMMENT,  L"WEP encrypted");
        wzcConfig1.Privacy = Ndis802_11WEPEnabled;
        wzcConfig1.dwCtlFlags |= WZCCTL_WEPK_PRESENT;
        p += wcslen(L":-wep:");
        if(iswdigit(*p))    // static key (ex, :-wep:1:0x1234567890)
        {
            wzcConfig1.KeyIndex = _wtoi(p);
            g_pKato->Log(LOG_COMMENT, L"key index = %d", wzcConfig1.KeyIndex);
            wzcConfig1.KeyIndex -= 1;
            p += 2;
            // check hexa string or normal ascii string
            BOOL bHexaWepKeyString = FALSE;
            if((*p==L'0') && (*(p+1)==L'x'))
            {
                p += 2;// hexa
                bHexaWepKeyString = TRUE;
            }
            for(i=0; p[i] && p[i]!=L':'; i++)
                wzcConfig1.KeyMaterial[i] = (UCHAR) p[i];
            wzcConfig1.KeyLength = i;

            if(bHexaWepKeyString)
            {
                g_pKato->Log(LOG_COMMENT, L"key value = %hs [hexa]", wzcConfig1.KeyMaterial);
                //wzcConfig1.dwCtlFlags |= WZCCTL_WEPK_XFORMAT;
                if(wzcConfig1.KeyLength == 10)
                   wzcConfig1.KeyLength = 5;
                else if(wzcConfig1.KeyLength == 26)
                   wzcConfig1.KeyLength = 13;
                else
                    g_pKato->Log(LOG_COMMENT, L"hexa WEP key should be either 10-digits or 26-digits");

                for(i=0; i<wzcConfig1.KeyLength; i++)
                    wzcConfig1.KeyMaterial[i] = HEX(wzcConfig1.KeyMaterial[i*2])<<4 | HEX(wzcConfig1.KeyMaterial[i*2 + 1]);
                for( ; i<WZCCTL_MAX_WEPK_MATERIAL; i++)
                    wzcConfig1.KeyMaterial[i] = 0;
            }
            else
            {
                g_pKato->Log(LOG_COMMENT, L"key value = %hs [ascii string]", wzcConfig1.KeyMaterial);
            }
            g_pKato->Log(LOG_COMMENT, L"key length = %d bytes", wzcConfig1.KeyLength);

            BYTE chFakeKeyMaterial[] = {0x56, 0x09, 0x08, 0x98, 0x4D, 0x08, 0x11, 0x66, 0x42, 0x03, 0x01, 0x67, 0x66 };
            for (i = 0; i < WZCCTL_MAX_WEPK_MATERIAL; i++)
                wzcConfig1.KeyMaterial[i] ^= chFakeKeyMaterial[(7*i)%13];
        }
        else if((p=wcsstr(p, L"-eap:"))!=NULL)    // auto
        {   // ex, CE8021X:-wep:auto-eap:tls
            g_pKato->Log(LOG_COMMENT, L"key will be given by EAP");

            p += wcslen(L"-eap:");

            wzcConfig1.EapolParams.dwEapFlags = EAPOL_ENABLED;
            wzcConfig1.EapolParams.dwEapType = DEFAULT_EAP_TYPE;
            wzcConfig1.EapolParams.bEnable8021x  = TRUE;

            if(_wcsnicmp(p, L"TLS", 3)==0)
                wzcConfig1.EapolParams.dwEapType = EAP_TYPE_TLS;
            else if(_wcsnicmp(p, L"PEAP", 4)==0)
                wzcConfig1.EapolParams.dwEapType = EAP_TYPE_PEAP;
            else if(_wcsnicmp(p, L"MD5", 3)==0)
                wzcConfig1.EapolParams.dwEapType = EAP_TYPE_MD5;
            else if(_wcsnicmp(p, L"MSCHAPv2", 6)==0)
                wzcConfig1.EapolParams.dwEapType = EAP_TYPE_MSCHAPv2;
            wzcConfig1.EapolParams.dwAuthDataLen = 0;
            wzcConfig1.EapolParams.pbAuthData    = 0;

            g_pKato->Log(LOG_COMMENT,
                L"EAP type = %02X (TLS:%02X, PEAP:%02X, MD5:%02X)",
                wzcConfig1.EapolParams.dwEapType,
                EAP_TYPE_TLS, EAP_TYPE_PEAP, EAP_TYPE_MD5);
        }
    }


    
    
    // start of actual "adding to the preferred list"

    DWORD dwOutFlags = 0;
    INTF_ENTRY Intf;
    memset(&Intf, 0x00, sizeof(INTF_ENTRY));
    Intf.wszGuid = szDeviceName;

    DWORD dwError = WZCQueryInterface(
                        NULL, 
                        INTF_ALL,
                        &Intf, 
                        &dwOutFlags);
    if(dwError)
    {
        if(dwError == 0x02)
            g_pKato->Log(LOG_FAIL, L"WZCQueryInterface() error : no %s device", szDeviceName);
        else
            g_pKato->Log(LOG_FAIL, L"WZCQueryInterface() error 0x%0X, 0x%0X", dwError, dwOutFlags);
        return;
    }

    WZC_802_11_CONFIG_LIST *pConfigList = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;
    if(!pConfigList)
    {
        DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST);
        WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);
        pNewConfigList->NumberOfItems = 1;
        pNewConfigList->Index = 0;
        memcpy(pNewConfigList->Config, &wzcConfig1, sizeof(wzcConfig1));
        Intf.rdStSSIDList.pData = (BYTE*)pNewConfigList;
        Intf.rdStSSIDList.dwDataLen = dwDataLen;
    }
    else
    {
        ULONG uiNumberOfItems = pConfigList->NumberOfItems;
        for(UINT i=0; i<uiNumberOfItems; i++)
        {
            if(memcmp(&wzcConfig1.Ssid, &pConfigList->Config[i].Ssid, sizeof(NDIS_802_11_SSID)) == 0)
            {
                g_pKato->Log(LOG_COMMENT, L"%s is already in the list", szSSID);
                return;
            }
        }
        g_pKato->Log(LOG_COMMENT, L"SSID List has [%d] entries.", uiNumberOfItems);
        uiNumberOfItems += 1;

        DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST) + uiNumberOfItems*sizeof(WZC_WLAN_CONFIG);
        WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);
        pNewConfigList->NumberOfItems = uiNumberOfItems;
        pNewConfigList->Index = uiNumberOfItems;
        if(pConfigList && pConfigList->NumberOfItems)
        {
            pNewConfigList->Index = pConfigList->Index;
            memcpy(pNewConfigList->Config, pConfigList->Config, (uiNumberOfItems)*sizeof(WZC_WLAN_CONFIG));
            LocalFree(pConfigList);
            pConfigList = NULL;
        }

        memcpy(pNewConfigList->Config + uiNumberOfItems - 1,
            &wzcConfig1, sizeof(wzcConfig1));
        Intf.rdStSSIDList.pData = (BYTE*)pNewConfigList;
        Intf.rdStSSIDList.dwDataLen = dwDataLen;
    }

    dwError = WZCSetInterface(NULL, INTF_ALL_FLAGS | INTF_PREFLIST, &Intf, &dwOutFlags);
    if(dwError)
        g_pKato->Log(LOG_FAIL, L"WZCSetInterface() error 0x%0X, 0x%0X", dwError, dwOutFlags);

    WZCDeleteIntfObj(&Intf);
}




void
ResetPreferredList
//
// reset the preferred list, so wireless will be disconnected
//
(
IN WCHAR* szDeviceName // [in] wireless network card device name
)
{
   DWORD dwOutFlags = 0;
   INTF_ENTRY Intf;
   memset(&Intf, 0x00, sizeof(INTF_ENTRY));
   Intf.wszGuid = szDeviceName;

   DWORD dwError = WZCQueryInterface(
                     NULL, 
                     INTF_ALL,
                     &Intf, 
                     &dwOutFlags);
   if(dwError)
   {
      if(dwError == 0x02)
         g_pKato->Log(LOG_FAIL, L"WZCQueryInterface() error : no %s device", szDeviceName);
      else
         g_pKato->Log(LOG_FAIL, L"WZCQueryInterface() error 0x%0X, 0x%0X", dwError, dwOutFlags);
      return;
   }

   g_pKato->Log(LOG_COMMENT, L"resetting preferred SSID list");
   WZC_802_11_CONFIG_LIST *pConfigList = (PWZC_802_11_CONFIG_LIST)Intf.rdStSSIDList.pData;
   if(!pConfigList || (pConfigList->NumberOfItems == 0))
   {
      g_pKato->Log(LOG_COMMENT, L"nothing in the preferred list");
      return;
   }

   ULONG NumberOfItems = 0;
   DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST)+NumberOfItems*sizeof(WZC_WLAN_CONFIG);
   WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);
   memcpy(pNewConfigList->Config, pConfigList, sizeof(WZC_WLAN_CONFIG));
   pNewConfigList->NumberOfItems = NumberOfItems;
   pNewConfigList->Index = NumberOfItems;
   Intf.rdStSSIDList.pData = (BYTE*)pNewConfigList;
   Intf.rdStSSIDList.dwDataLen = dwDataLen;
   LocalFree(pConfigList);
   dwError = WZCSetInterface(NULL, INTF_ALL_FLAGS | INTF_PREFLIST, &Intf, &dwOutFlags);

   if(dwError)
      g_pKato->Log(LOG_FAIL, L"WZCSetInterface() error 0x%0X, 0x%0X", dwError, dwOutFlags);

   WZCDeleteIntfObj(&Intf);
}






void
FindWirelessNetworkDevice
//
// enum wireless network device
// shows available wireless network card installed on the system
//
()
{
   INTFS_KEY_TABLE IntfsTable;
   IntfsTable.dwNumIntfs = 0;
   IntfsTable.pIntfs = NULL;

   DWORD dwError = WZCEnumInterfaces(NULL, &IntfsTable);

   if(dwError != ERROR_SUCCESS)
   {
      g_szWiFiAdapter1 = NULL;
      g_pKato->Log(LOG_FAIL, L"WZCEnumInterfaces() error 0x%0X", dwError);
      return;        
   }

   // Print the GUIDs
   // Note that in CE the GUIDs are simply the device instance name
   // i.e NE20001, CISCO1, etc..
   //

   if(!IntfsTable.dwNumIntfs)
   {
      g_szWiFiAdapter1 = NULL;
      g_pKato->Log(LOG_COMMENT, L"wireless device none.");
      return;
   }

#if 0
   for(unsigned int i = 0; i < IntfsTable.dwNumIntfs; i++)
      g_pKato->Log(LOG_COMMENT, L"wifi device [%d] = %s", i, IntfsTable.pIntfs[i].wszGuid);
#endif

   wcscpy(g_szWiFiAdapterNameActualData, IntfsTable.pIntfs[0].wszGuid);
   g_szWiFiAdapter1 = g_szWiFiAdapterNameActualData;
   LocalFree(IntfsTable.pIntfs);
}











TESTPROCAPI
Test1
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, _T("Test1"));
    //g_pKato->Log(LOG_FAIL, _T("LOG_FAIL"));


	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}



WCHAR *
NextOption
(
WCHAR *p,
LPCTSTR pOptionName
)
{
    p += wcslen(pOptionName) + 1;
    while(*p && iswspace(*p))
        p++;
    return p;
}


DWORD
GenerateRandomInterval(DWORD dwInterval)
{
    return Random()%dwInterval;
}





void
GetTuxOption
(
IN LPCTSTR szCmdLine,
IN LPCTSTR szOptionName,
IN OUT WCHAR *szOptionValue
)
{
    if(!szOptionValue)
        return;

    if(!szCmdLine || !szOptionName) {
        *szOptionValue = L'\0';
        return;
    }

    WCHAR *p = wcsstr(szCmdLine, szOptionName);
    if(p)
    {
        p = NextOption(p, szOptionName);
        if(p)
        {
            wcscpy(szOptionValue, p);
            for(p=szOptionValue; *p && !iswspace(*p); p++)
                ;
            *p = L'\0';
        }
    }
    else
        *szOptionValue = L'\0';
}




TESTPROCAPI
Test_CheckWiFiCard
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FindWirelessNetworkDevice();
    if(g_szWiFiAdapter1)
    {
        g_pKato->Log(LOG_COMMENT, L"found a wifi adapter, %s", g_szWiFiAdapter1);
        return TPR_PASS;
    }

    g_pKato->Log(LOG_COMMENT, L"wifi adapter not found");
    g_pKato->Log(LOG_COMMENT, L"try to install XWIFI11B and see if it registers");
    CardInsertNdisMiniportAdapter(L"XWIFI11B", NULL);
    for(int i=0; i<10; i++) // check for 10 seconds
    {
        Sleep(1000);
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1)
        {
            g_pKato->Log(LOG_COMMENT, L"found a WiFi adapter, %s", g_szWiFiAdapter1);
            break;
        }
    }
    CardEjectNdisMiniportAdapter(L"XWIFI11B1");
    g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 ejected (restore to the original no-card-inserted state)");
    if(g_szWiFiAdapter1)
        return TPR_PASS;
    g_pKato->Log(LOG_FAIL, L"forced to install XWIFI11B adapter but still it is not recognized as a wifi adapter");
    return TPR_FAIL;
}




TESTPROCAPI
Test_ResetPreferredList
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FindWirelessNetworkDevice();
    if(g_szWiFiAdapter1 == NULL)
    {
        g_pKato->Log(LOG_COMMENT, L"found 0 WiFi adapter");
        return TPR_PASS;
    }
    g_pKato->Log(LOG_COMMENT, L"found 1 wifi adapter, %s", g_szWiFiAdapter1);
    ResetPreferredList(g_szWiFiAdapter1);
    return TPR_PASS;
}





// check if UI comes when no preferred ssid
TESTPROCAPI
Test_CheckUiWhenNoPreferredSsid
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FindWirelessNetworkDevice();
    if(g_szWiFiAdapter1 == NULL)
    {
        g_pKato->Log(LOG_COMMENT, L"found 0 WiFi adapter");
        g_pKato->Log(LOG_COMMENT, L"try to install XWIFI11B and continue to test");
        CardInsertNdisMiniportAdapter(L"XWIFI11B", NULL);
        Sleep(1000);
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1 == NULL)
        {
            g_pKato->Log(LOG_FAIL, L"installing XWIFI11B failed");
            return TPR_FAIL;
        }
    }
    g_pKato->Log(LOG_COMMENT, L"found 1 wifi adapter, %s", g_szWiFiAdapter1);
    ResetPreferredList(g_szWiFiAdapter1);
    Sleep(1000);    // give 1 second more to show UI
    NdisConfigAdapterUnbind(g_szWiFiAdapter1);
    Sleep(1000);
    NdisConfigAdapterBind(g_szWiFiAdapter1);
    Sleep(1000);
    if(IsDialogWindowPoppedUp(g_szWiFiAdapter1))
        g_pKato->Log(LOG_COMMENT, L"found dlg [%s], pass", g_szWiFiAdapter1);
    else
    {
        g_pKato->Log(LOG_FAIL, L"not found dlg [%s], fail", g_szWiFiAdapter1);
        return TPR_FAIL;
    }

    // dialog should not come if preferred has something to connect
    AddSsidToThePreferredList(g_szWiFiAdapter1, L"ADHOC1");
    Sleep(1000);
    NdisConfigAdapterUnbind(g_szWiFiAdapter1);
    Sleep(1000);
    NdisConfigAdapterBind(g_szWiFiAdapter1);

    for(int i=0; i<10; i++) // check for 10 seconds
    {
        Sleep(1000);
        if(IsDialogWindowPoppedUp(g_szWiFiAdapter1))
        {
            g_pKato->Log(LOG_FAIL, L"the dlg [%s] is not supposed to come, but it popped up, fail", g_szWiFiAdapter1);
            return TPR_FAIL;
        }
    }

    ResetPreferredList(g_szWiFiAdapter1);
    return TPR_PASS;
}




#define DEFAULT_MAX_ITERATION 10


// repeatedly check if UI comes when no preferred ssid 10 times

TESTPROCAPI
Test_CheckUiWhenNoPreferredSsid_100
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FindWirelessNetworkDevice();
    if(g_szWiFiAdapter1 == NULL)
    {
        g_pKato->Log(LOG_COMMENT, L"found 0 WiFi adapter");
        g_pKato->Log(LOG_COMMENT, L"try to install XWIFI11B and continue to test");
        CardInsertNdisMiniportAdapter(L"XWIFI11B", NULL);
        Sleep(1000);
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1 == NULL)
        {
            g_pKato->Log(LOG_FAIL, L"installing XWIFI11B failed");
            return TPR_FAIL;
        }
    }
    g_pKato->Log(LOG_COMMENT, L"found 1 wifi adapter, %s", g_szWiFiAdapter1);
    ResetPreferredList(g_szWiFiAdapter1);
    Sleep(1000);    // give 1 second more to show UI

    for(unsigned n=0; n<DEFAULT_MAX_ITERATION; n++)
    {
        NdisConfigAdapterUnbind(g_szWiFiAdapter1);
        Sleep(1000);
        NdisConfigAdapterBind(g_szWiFiAdapter1);
        Sleep(5000);
        if(IsDialogWindowPoppedUp(g_szWiFiAdapter1))
            g_pKato->Log(LOG_COMMENT, L"found dlg [%s], pass", g_szWiFiAdapter1);
        else
        {
            g_pKato->Log(LOG_FAIL, L"not found dlg [%s], fail", g_szWiFiAdapter1);
            return TPR_FAIL;
        }
    }

    ResetPreferredList(g_szWiFiAdapter1);
    return TPR_PASS;
}


#define DEFAULT_INSERT_EJECT_INTERVAL 5

#define TEST_ID_WZC_UI          0x00010000
#define TEST_ID_WZC_UI_NONE     0x00020000

#define TEST_ID_IP_CHECK_BITS   0x00F00000
#define TEST_ID_IP_ZEROS        0x00100000
#define TEST_ID_IP              0x00200000
    // lower 16bit shows IP address
    // 169.254.x.x = A9.FE
    // 192.168.x.x = C0.A8
    // 10.10.x.x = 0A.0A
    // 100.100.x.x = 64.64

#define TEST_DW_ACTIVATION_NONE                      0
#define TEST_DW_ACTIVATION_BIND_UNBIND               1
#define TEST_DW_ACTIVATION_CARD_INSERT_EJECT         2
#define TEST_DW_ACTIVATION_CARD_PWR_ON_OFF           3
#define TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME 4




INT
_ActivateDeactivate100
// the NIC should exist when this routine is starting.
//
(
WCHAR *szAdapter,       // XWIFI11B  (not including 1)
UINT uiMaxIteration,    // 0=default, DEFAULT_MAX_ITERATION
int iInterval,    // 0=default interval (5 sec), -1=insert->random-wait->eject, -2=eject->random-wait->insert, otherwise sleep given uiInterval ms.
DWORD dwTestId,  // check with TEST_ID_IP_CHECK_BITS
    // higher 16bits.
    // 11111111 11111111
    //                 * .. check UI, expect to see WZC dialog
    //                * ... check UI, expect NOT to see WZC dialog
    //               ? .... not use this time
    //          **** ...... check IP when inserted
    //                      0000 : no test on IP
    //                      1111 : check IP (TEST_ID_IP_CHECK_BITS)
    //                      0001 : IP 0.0.0.0
    //                      0010 : IP
    //                              lower 16bits are IP address
    //                              169.254.x.x = A9.FE
    //                              192.168.x.x = C0.A8
    //                              10.10.x.x = 0A.0A
    //                              100.100.x.x = 64.64
    //
WCHAR *szCmd,        // command to run when connected (ex, ping gateway)
DWORD dwActivationMethod
    // this tells how to activate or deactivate this card.
    //
    // 0  none
    // 1  bind/unbind
    // 2  insert/eject card
    // 3  adapter power on/off
    // 4  system power on/off
    //
)
{
    g_pKato->Log(LOG_COMMENT, L"WiFi adapter = %s", szAdapter);
    WCHAR szAdapter1[128];
    swprintf(szAdapter1, L"%s1", szAdapter);
    if(!AdapterExist(szAdapter1))
    {
        g_pKato->Log(LOG_FAIL, L"no adapter named %s", szAdapter);
        return TPR_FAIL;
    }
    if(_wcsicmp(szAdapter1, L"XWIFI11B1"))
        g_pKato->Log(LOG_COMMENT, L"warning not XWIFI11B card, insert/eject may fail");

    if(!uiMaxIteration)
        uiMaxIteration = DEFAULT_MAX_ITERATION;
    DWORD dwInterval = 5*1000;
    BOOL bRandomTimeInterval_BetweenInsertEject = (iInterval == -1);
    BOOL bRandomTimeInterval_BetweenEjectInsert = (iInterval == -2);
    if(iInterval>0)
        dwInterval = iInterval;

    WCHAR szIpAddr[16] = L"169.254.x.x";
    if(dwTestId & TEST_ID_IP_CHECK_BITS)
        wsprintf(szIpAddr, L"%d.%d.x.x", (dwTestId>>8)&0xFF, dwTestId&0xFF);

    // card is inserted state at this point, so eject first.
    for(unsigned i=0; i<uiMaxIteration; i++)
    {
        g_pKato->Log(LOG_COMMENT, L"test iteration = %d", i);

        // de-activating card
        if(dwActivationMethod == TEST_DW_ACTIVATION_BIND_UNBIND) {
            g_pKato->Log(LOG_COMMENT, L"unbind");
            _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_UNBIND_ADAPTER);
        }
        else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_INSERT_EJECT) {
            g_pKato->Log(LOG_COMMENT, L"eject");
            CardEjectNdisMiniportAdapter(szAdapter1);
        }
        else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_PWR_ON_OFF) {
            g_pKato->Log(LOG_COMMENT, L"card power off");
            AdapterPowerOnOff(szAdapter1, 0);
        }
        else if(dwActivationMethod == TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME) {
            g_pKato->Log(LOG_COMMENT, L"system power suspend");
            SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE);
            goto SYSTEM_PWR_RECOVER;
        }

        // wait
        if(bRandomTimeInterval_BetweenEjectInsert)
        {
            DWORD dwT1 = GenerateRandomInterval(dwInterval);
            g_pKato->Log(LOG_COMMENT, L"** wait %d ms after deactivate", dwT1);
            Sleep(dwT1);
        }
        else
        {
            g_pKato->Log(LOG_COMMENT, L"** wait %d ms after deactivate", dwInterval);
            Sleep(dwInterval);

            if(dwTestId & (TEST_ID_WZC_UI | TEST_ID_WZC_UI_NONE))
            {
                if(IsDialogWindowPoppedUp(szAdapter1))
                {
                    g_pKato->Log(LOG_FAIL, L"expected to see UI(%s) disappeared, but still visible [FAIL]", szAdapter1);
                    return TPR_FAIL;
                }
            }
            else if(dwTestId & TEST_ID_IP_CHECK_BITS)
            {
                if(AdapterHasNoneZeroIpAddress(szAdapter1))
                {
                    g_pKato->Log(LOG_FAIL, L"expected to see no adapter, so no IP address, but still has IP [FAIL]");
                    return TPR_FAIL;
                }
            }
        } // bRandomTimeInterval_BetweenEjectInsert


        // activating card
        if(dwActivationMethod == TEST_DW_ACTIVATION_BIND_UNBIND) {
            g_pKato->Log(LOG_COMMENT, L"bind");
            _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_BIND_ADAPTER);
        }
        else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_INSERT_EJECT) {
            g_pKato->Log(LOG_COMMENT, L"insert");
            _CardInsertNdisMiniportAdapter(szAdapter);
        }
        else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_PWR_ON_OFF) {
            g_pKato->Log(LOG_COMMENT, L"card power on");
            AdapterPowerOnOff(szAdapter1, 1);
        }
//        else if(dwActivationMethod == TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME)
//            SetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE);
        SYSTEM_PWR_RECOVER:

        // wait
        if(bRandomTimeInterval_BetweenInsertEject)
        {
            DWORD dwT1 = GenerateRandomInterval(dwInterval);
            g_pKato->Log(LOG_COMMENT, L"** wait %d ms after activate", dwT1);
            Sleep(dwT1);
        }
        else
        {
            g_pKato->Log(LOG_COMMENT, L"** wait %d ms after activate", dwInterval);
            Sleep(dwInterval);

            if(dwTestId & TEST_ID_WZC_UI)
            {
                if(IsDialogWindowPoppedUp(szAdapter1))
                    g_pKato->Log(LOG_COMMENT, L"expected to see UI(%s), and found [PASS]", szAdapter1);
                else
                {
                    g_pKato->Log(LOG_FAIL, L"expected to see UI(%s), but not found [FAIL]", szAdapter1);
                    return TPR_FAIL;
                }
            }
            else if(dwTestId & TEST_ID_WZC_UI_NONE)
            {
                if(IsDialogWindowPoppedUp(szAdapter1))
                {
                    g_pKato->Log(LOG_FAIL, L"expected to see no UI(%s), but found [FAIL]", szAdapter1);
                    return TPR_FAIL;
                }
                else
                    g_pKato->Log(LOG_COMMENT, L"expected to see no UI(%s), and not found [PASS]", szAdapter1);
            }
            else if(dwTestId & TEST_ID_IP_CHECK_BITS)
            {
                if(dwTestId & TEST_ID_IP_ZEROS)
                {
                    if(AdapterHasNoneZeroIpAddress(szAdapter1))
                    {
                        g_pKato->Log(LOG_FAIL, L"but found an IP [FAIL]");
                        return TPR_FAIL;
                    }
                    g_pKato->Log(LOG_COMMENT, L"expected to have 0.0.0.0 IP [PASS]");
                }
                else if(dwTestId & TEST_ID_IP)
                {
                    if(AdapterHasIpAddress(szAdapter1, szIpAddr))
                        g_pKato->Log(LOG_COMMENT, L"expected to have IP %s [PASS]", szIpAddr);
                    else
                    {
                        g_pKato->Log(LOG_FAIL, L"expected to have IP %s, but has something else or no IP address [FAIL]", szIpAddr);
                        return TPR_FAIL;
                    }
                }
            }
        } // bRandomTimeInterval_BetweenInsertEject

        if(szCmd && *szCmd)
        {
            if(!_wcsnicmp(szCmd, L"quit", 4))
            {
                g_pKato->Log(LOG_COMMENT, L"quit-on-connect");
                return TPR_PASS;
            }
            RunTestCmd(szCmd);
        }
    }

    g_pKato->Log(LOG_COMMENT, L"tested %d times, all PASS", uiMaxIteration);
	return TPR_PASS;
}



// test command

/*

1. ADHOC-AUTOIP-OPEN-NOWEP-INSERT/EJECT-100
    d ntapconfig -card-insert xwifi11b
    tux -o -d xwifi11btest -x 100 -c "-test UI-NONE -n 10"

    d wadm XWIFI11B1 -add CEADHOC1 -adhoc
*/

/*
    tux -o -d xwifi11btest -x 100 -c "-test UI-NONE -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test UI -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test ZERO -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test AUTO -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test STATIC -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test DHCP -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test DHCP_192.168.x.x -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test IP -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test IP_10.10.x.x -n 10"
    tux -o -d xwifi11btest -x 100 -c "-test IP_10.10.x.x -n 10 -x bind/unbind"
*/
// option "-test UI-NONE", "UI", "ZERO_IP", "STATIC_IP", "AUTO_IP", "DHCP", "DHCP 192.168.x.x", "IP 23.45.x.x"
// { UI tests } | { IP tests } can be given together
// numeric value for the IP address can be given.
// if not given, followings will be used as a default
// AUTO_IP = 169.254.x.x
// DHCP = 10.10.x.x
// STATIC = 100.100.x.x
// -x option
// -x none
// -x bind/unbind
// -x insert/eject-card
// -x adapter-power-on/off
// -x system-power-on/off
//
// -cmd ping_10.10.0.1
//


TESTPROCAPI
Test_ActivateDeactivate100
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    int iInterval = 5000;  // 5 sec
    BOOL bRandomTimeInterval = FALSE;
    unsigned uiRepeatCount = DEFAULT_MAX_ITERATION;
    DWORD dwTestId = 0;
    DWORD dwActivationMethod = TEST_DW_ACTIVATION_NONE;
    WCHAR szCmd[128] = L"";

    if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
    {
        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-dt", szCmd);
        if(szCmd[0])
        {
            iInterval = _wtoi(szCmd);
            if(iInterval == 0)
                iInterval = 5000;
            if(wcsstr(szCmd, L"random-eject"))
                iInterval = -1;
            if(wcsstr(szCmd, L"random-insert"))
                iInterval = -2;
        }

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-n", szCmd);
        if(szCmd[0])
            uiRepeatCount = _wtoi(szCmd);

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-test", szCmd);
        if(szCmd[0])
        {
            if(!_wcsnicmp(szCmd, L"UI-NONE", 7))
                dwTestId = TEST_ID_WZC_UI_NONE;
            else if(!_wcsnicmp(szCmd, L"UI", 2))
                dwTestId = TEST_ID_WZC_UI;
            
            if(!_wcsnicmp(szCmd, L"ZERO", 4))
                dwTestId |= TEST_ID_IP_ZEROS;
            else
            {
                if(!_wcsnicmp(szCmd, L"STATIC", 6))
                    dwTestId |= TEST_ID_IP | 0x6464; // 100.100.x.x
                else if(!_wcsnicmp(szCmd, L"AUTO", 4))
                    dwTestId |= TEST_ID_IP | 0xA9FE; // 169.254.x.x
                else if(!_wcsnicmp(szCmd, L"DHCP", 4))
                    dwTestId |= TEST_ID_IP | 0x0A0A; // 10.10.x.x
                else if(!_wcsnicmp(szCmd, L"IP", 2))
                    dwTestId |= TEST_ID_IP | 0x0A0A; // 10.10.x.x

                if(dwTestId & 0x0000FFFF)   // if numeric IP is given
                {
                    WCHAR *p = szCmd;
                    while(*p && !iswdigit(*p))
                        p++;
                    if(*p)
                    {
                        dwTestId &= 0xFFFF0000;
                        dwTestId |= (_wtoi(p)&0xFF)<<8;
                        while(*p && (*p!=L'.'))
                            p++;
                        if(*p)
                        {
                            p++;
                            if(*p)
                                dwTestId |= _wtoi(p)&0xFF;
                        }
                    }
                }
            }
        } // if(p)  // -test

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-x", szCmd);
        if(szCmd[0])
        {
            if(!_wcsnicmp(szCmd, L"bind/unbind", 4))
                dwActivationMethod = TEST_DW_ACTIVATION_BIND_UNBIND;
            else if(!_wcsnicmp(szCmd, L"insert/eject", 4))
                dwActivationMethod = TEST_DW_ACTIVATION_CARD_INSERT_EJECT;
            else if(!_wcsnicmp(szCmd, L"adapter-power-on/off", 4))
                dwActivationMethod = TEST_DW_ACTIVATION_CARD_PWR_ON_OFF;
            else if(!_wcsnicmp(szCmd, L"system-power-on/off", 4))
                dwActivationMethod = TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME;
        }

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-cmd", szCmd);
    }
    if(bRandomTimeInterval)
        g_pKato->Log(LOG_COMMENT, L"random interval");
    g_pKato->Log(LOG_COMMENT, L"uiRepeatCount = %d", uiRepeatCount);

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1)
    {
        g_pKato->Log(LOG_FAIL, L"no WiFi adapter");
        g_pKato->Log(LOG_COMMENT, L"try to install XWIFI11B and continue to test");
        CardInsertNdisMiniportAdapter(L"XWIFI11B", NULL);
        Sleep(1000);
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1 == NULL)
        {
            g_pKato->Log(LOG_FAIL, L"installing XWIFI11B failed");
            return TPR_FAIL;
        }
    }
    g_pKato->Log(LOG_COMMENT, L"wifi adapter = %s", g_szWiFiAdapter1);

    if(dwActivationMethod == TEST_DW_ACTIVATION_NONE)
        g_pKato->Log(LOG_COMMENT, L"ACTIVATION_NONE");
    else if(dwActivationMethod == TEST_DW_ACTIVATION_BIND_UNBIND)
        g_pKato->Log(LOG_COMMENT, L"TEST_DW_ACTIVATION_BIND_UNBIND");
    else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_INSERT_EJECT)
        g_pKato->Log(LOG_COMMENT, L"TEST_DW_ACTIVATION_CARD_INSERT_EJECT");
    else if(dwActivationMethod == TEST_DW_ACTIVATION_CARD_PWR_ON_OFF)
        g_pKato->Log(LOG_COMMENT, L"TEST_DW_ACTIVATION_CARD_PWR_ON_OFF");
    else if(dwActivationMethod == TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME)
    {
        g_pKato->Log(LOG_COMMENT, L"TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME");

        DWORD dwWakeSrc = SYSINTR_RESCHED;
        if(!KernelIoControl(IOCTL_HAL_ENABLE_WAKE, &dwWakeSrc, sizeof(dwWakeSrc), NULL, 0, NULL))
        {
            g_pKato->Log(LOG_COMMENT, L"SUSPEND: !!! failed KernelIoControl(IOCTL_HAL_ENABLE_WAKE)\n"
                L"this device does not support this KernelIoControl");
            return TPR_PASS;
        }
    }
    else
        g_pKato->Log(LOG_COMMENT, L"TEST_DW_ACTIVATION_???");

    WCHAR szAdapter1[128];
    wcscpy(szAdapter1, g_szWiFiAdapter1);
    szAdapter1[wcslen(szAdapter1)-1] = L'\0';

    INT iTestResult = _ActivateDeactivate100
            (
                szAdapter1,       // XWIFI11B  (not including 1)
                uiRepeatCount,
                iInterval,    // 0=default interval (5 sec), -1=insert->random-wait->eject, -2=eject->random-wait->insert, otherwise sleep given uiInterval ms.
                dwTestId,
                szCmd,        // command to run when connected (ex, ping gateway)
                dwActivationMethod
            );

    if(dwActivationMethod == TEST_DW_ACTIVATION_SYSTEM_PWR_SUSPEND_RESUME)
    {
        DWORD dwWakeSrc = SYSINTR_RESCHED;
        if(!KernelIoControl(IOCTL_HAL_DISABLE_WAKE, &dwWakeSrc, sizeof(dwWakeSrc), NULL, 0, NULL))
            g_pKato->Log(LOG_COMMENT, L"SUSPEND: !!! failed KernelIoControl(IOCTL_HAL_ENABLE_WAKE)\n"
                L"this device does not support this KernelIoControl");
    }

    return iTestResult;
}




void
Stress_CE_TestNet
//
// CE test net in the OS Lab
// CEOPEN, CESHARED, CE8021X
// this net is radce.com domain (10.10.x.x)
// DC is radceroot
//
// when building for the longterm
// it connects to the CELT AP
//
(
WCHAR *szSSID_withOption,
DWORD dwTestId,
WCHAR *szCmd        // command to run when connected (ex, ping gateway)
)
{
    WCHAR szSSID[64];
    for(UINT i=0; szSSID_withOption[i] && (i<MAX_SSID_LEN); i++)
    {
        szSSID[i] = szSSID_withOption[i];
        if(szSSID[i] == L'_')
            szSSID[i] = L' ';
        else if((szSSID[i] == L'/') || (szSSID[i] == L':'))
            break;
    }
    szSSID[i] = L'\0';

    g_pKato->Log(LOG_COMMENT, L"*********** WiFi test on %s ***********", szSSID);

    if(!g_szWiFiAdapter1)
    {
        g_pKato->Log(LOG_FAIL, L"wireless adapter not found");
        return;
    }
    g_pKato->Log(LOG_COMMENT, L"wireless adapter found : %s", g_szWiFiAdapter1);
    g_pKato->Log(LOG_COMMENT, L"now connecting to %s", szSSID);

/*
    DIALOG_BOX_WATCHER Watch_UserLogon;    // for CE8021X
    HANDLE hThread1 = NULL;

    if(wcscmp(szSSID, L"CE8021X")==0)
    {
        if(!EnrollTo(L"radceroot"))
        {   // enrollment should be already done when it was connected to CEOPEN or CESHARED.
            g_pKato->Log(LOG_FAIL, L"enroll failed");
            g_pKato->Log(LOG_COMMENT, L"enroll will be handled later when CE is connected to the test net via CEOPEN or CESHARED");
            return;
        }

        Watch_UserLogon.hNotifyTerminateEvent = CreateEvent(NULL, TRUE, FALSE, L"watch-UserLogon");
        Watch_UserLogon.szTitleOfWindow = L"User Logon";
        Watch_UserLogon.szTodo = L"\t\tEAPTLS\tRADCE\n";    // wceqalab [tab] redmond [return]
        Watch_UserLogon.bKeepTry = TRUE;    // since sometimes, authenticate fails, then we should try again

        ResetEvent(Watch_UserLogon.hNotifyTerminateEvent);
        hThread1 = CreateThread(NULL, 0, &DialogBoxWatcherThread, &Watch_UserLogon, 0, NULL);
    }
*/

    AddSsidToThePreferredList(g_szWiFiAdapter1, szSSID_withOption);
    Sleep(1000);
    if(WaitForGainingNetworkConnection(g_szWiFiAdapter1))
    {
        g_pKato->Log(LOG_COMMENT, L"connected to %s", szSSID);
        if(dwTestId == 0)
            RunCmd(L"ipconfig /all");
        else if(dwTestId & TEST_ID_IP_CHECK_BITS)
        {
            if(dwTestId & TEST_ID_IP_ZEROS)
            {
                g_pKato->Log(LOG_COMMENT, L"expected to have 0.0.0.0 IP");
                if(AdapterHasNoneZeroIpAddress(g_szWiFiAdapter1))
                {
                    g_pKato->Log(LOG_FAIL, L"but found an IP [FAIL]");
                    return;
                }
            }
            else if(dwTestId & TEST_ID_IP)
            {
                WCHAR szIpAddr[16] = L"169.254.x.x";
                wsprintf(szIpAddr, L"%d.%d.x.x", (dwTestId>>8)&0xFF, dwTestId&0xFF);
                if(AdapterHasIpAddress(g_szWiFiAdapter1, szIpAddr))
                    g_pKato->Log(LOG_COMMENT, L"expected to have IP %s [PASS]", szIpAddr);
                else
                {
                    g_pKato->Log(LOG_FAIL, L"expected to have IP %s, but has something else or no IP address [FAIL]", szIpAddr);
                    return;
                }
            }
        }

/*
        if(hThread1)
        {
            SetEvent(Watch_UserLogon.hNotifyTerminateEvent);
            WaitForSingleObject(hThread1, INFINITE);
            hThread1 = NULL;
        }

        // things to do when connected
        EnrollTo(L"radceroot"); // do enroll when we are connected. the enroll result will be used when connecting CE8021X
        g_pKato->Log(LOG_COMMENT, L"ping to the test domain controller [radceroot] for the wireless network");
        WCHAR szPingArgs[128];
        wsprintf(szPingArgs, L"ping radceroot -n 15");
        RunCommand(L"d", szPingArgs);
*/
    }
    else
    {
        g_pKato->Log(LOG_FAIL, L"wireless adapter didn't resolve DHCP address");
/*
        if(hThread1)
        {
            SetEvent(Watch_UserLogon.hNotifyTerminateEvent);
            WaitForSingleObject(hThread1, INFINITE);
            hThread1 = NULL;
        }
*/
    }
    Sleep(5000);

    if(szCmd && *szCmd)
    {
        if(!_wcsnicmp(szCmd, L"quit", 4))
        {
            g_pKato->Log(LOG_COMMENT, L"quit-on-connect");
            return;
        }
        RunTestCmd(szCmd);
    }

    ResetPreferredList(g_szWiFiAdapter1);
    Sleep(1000);
    WaitForLoosingNetworkConnection(g_szWiFiAdapter1);
    g_pKato->Log(LOG_COMMENT, L"disconnected from %s", szSSID);
    Sleep(10000);
}






TESTPROCAPI
Test_ConnectDisconnect100
// example
/*
s tux -o -d xwifi11btest -x 101 -c "-ssid CEOPEN -n 5 -cmd ping_10.10.0.1_-l_1024"
s tux -o -d xwifi11btest -x 101 -c "-ssid CEOPEN -n 2 -cmd ping_radceroot"
s tux -o -d xwifi11btest -x 101 -c "-ssid CESHARED:-wep:1:0x1234567890:-auth:shared -n 2 -cmd ping_gateway_-l_512"

-cmd:quit-on-connect
   will force to quite after connecting, useful when testing something after connecting
*/
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    int iInterval = 5000;  // 5 sec
    BOOL bRandomTimeInterval = FALSE;
    unsigned uiRepeatCount = DEFAULT_MAX_ITERATION;
    WCHAR szSSID_withOption[128] = L"";
    WCHAR szCmd[128] = L"";
    DWORD dwTestId = 0;

    if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
    {
        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-dt", szCmd);
        if(szCmd[0])
        {
            iInterval = _wtoi(szCmd);
            if(iInterval == 0)
                iInterval = 5000;
            if(wcsstr(szCmd, L"random-eject"))
                iInterval = -1;
            if(wcsstr(szCmd, L"random-insert"))
                iInterval = -2;
        }

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-n", szCmd);
        if(szCmd[0])
            uiRepeatCount = _wtoi(szCmd);

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-ssid", szSSID_withOption);

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-test", szCmd);
        if(szCmd[0])
        {
            if(!_wcsnicmp(szCmd, L"UI-NONE", 7))
                dwTestId = TEST_ID_WZC_UI_NONE;
            else if(!_wcsnicmp(szCmd, L"UI", 2))
                dwTestId = TEST_ID_WZC_UI;
            
            if(!_wcsnicmp(szCmd, L"ZERO", 4))
                dwTestId |= TEST_ID_IP_ZEROS;
            else
            {
                if(!_wcsnicmp(szCmd, L"STATIC", 6))
                    dwTestId |= TEST_ID_IP | 0x6464; // 100.100.x.x
                else if(!_wcsnicmp(szCmd, L"AUTO", 4))
                    dwTestId |= TEST_ID_IP | 0xA9FE; // 169.254.x.x
                else if(!_wcsnicmp(szCmd, L"DHCP", 4))
                    dwTestId |= TEST_ID_IP | 0x0A0A; // 10.10.x.x
                else if(!_wcsnicmp(szCmd, L"IP", 2))
                    dwTestId |= TEST_ID_IP | 0x0A0A; // 10.10.x.x

                if(dwTestId & 0x0000FFFF)   // if numeric IP is given
                {
                    WCHAR *p = szCmd;
                    while(*p && !iswdigit(*p))
                        p++;
                    if(*p)
                    {
                        dwTestId &= 0xFFFF0000;
                        dwTestId |= (_wtoi(p)&0xFF)<<8;
                        while(*p && (*p!=L'.'))
                            p++;
                        if(*p)
                        {
                            p++;
                            if(*p)
                                dwTestId |= _wtoi(p)&0xFF;
                        }
                    }
                }
            }
        } // if(p)  // -test

        GetTuxOption(g_pShellInfo->szDllCmdLine, L"-cmd", szCmd);
    }

    if(!szSSID_withOption[0])
    {
        g_pKato->Log(LOG_FAIL, L"no SSID is given");
        return TPR_FAIL;
    }

    if(bRandomTimeInterval)
        g_pKato->Log(LOG_COMMENT, L"random interval");
    g_pKato->Log(LOG_COMMENT, L"uiRepeatCount = %d", uiRepeatCount);
    g_pKato->Log(LOG_COMMENT, L"SSID = %s", szSSID_withOption);

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1)
    {
        g_pKato->Log(LOG_FAIL, L"no WiFi adapter");
        /*
        g_pKato->Log(LOG_COMMENT, L"try to install XWIFI11B and continue to test");
        CardInsertNdisMiniportAdapter(L"XWIFI11B", NULL);
        Sleep(1000);
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1 == NULL)
        {
            g_pKato->Log(LOG_FAIL, L"installing XWIFI11B failed");
            return TPR_FAIL;
        }
        */
    }
    g_pKato->Log(LOG_COMMENT, L"wifi adapter = %s", g_szWiFiAdapter1);


    for(unsigned i=0; i<uiRepeatCount; i++)
    {
        Stress_CE_TestNet(szSSID_withOption, dwTestId, szCmd);
    }

    return g_pKato->GetVerbosityCount(LOG_FAIL) ? TPR_FAIL : TPR_PASS;
}
