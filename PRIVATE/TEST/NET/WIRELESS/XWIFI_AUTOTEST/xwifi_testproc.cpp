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
#include "xwifi_testproc.h"

#include <windev.h>
#include <ndis.h>
#include <msgqueue.h>

extern CKato *g_pKato;
extern SPS_SHELL_INFO *g_pShellInfo;

#define HEX(c)  ((c)<='9'?(c)-'0':(c)<='F'?(c)-'A'+0xA:(c)-'a'+0xA)

#include <winuser.h>


BOOL
IsDialogWindowPoppedUp
// return TRUE if it found 'title' window
(
WCHAR *szTitle
)
{
    return FindWindow(NULL, szTitle)!=NULL;
}




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
IN OUT WCHAR *szMulti
)
{
    for(WCHAR *szCurrent = szMulti; *szCurrent; szCurrent++)
    {
        // Find the end of the current string
        szCurrent += wcslen(szCurrent);

        // Replace the string terminator with a space char
        *szCurrent = L' ';
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
            g_pKato->Log(LOG_COMMENT, L"NDIS DeviceIoControl result=%d", bResult);

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
    if(!szAdapter1 || (wcslen(szAdapter1)+2>256))
        return FALSE;

    WCHAR multiSz[256];
    wcscpy(multiSz, szAdapter1);
    multiSz[wcslen(multiSz)+1] = L'\0'; // Multi sz needs an extra null

    return DoNdisIOControl(
        dwCommand,
        multiSz,
        (wcslen(multiSz)+2) * sizeof(WCHAR),
        NULL,
        NULL);
}


#define NdisConfigAdapterBind(szAdapter1) \
    _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_BIND_ADAPTER);

#define NdisConfigAdapterUnbind(szAdapter1) \
    _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_UNBIND_ADAPTER);


#include "ndispwr.h"

#include <nkintr.h>



void
AdapterPowerOnOff
(
WCHAR *szAdapter,
BOOL bTurnPowerOn   // TRUE->will cause adapter power on
)
{
    if(!szAdapter || (wcslen(szAdapter)+2>256))
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
    if(!szAdapter || (wcslen(szAdapter)+2>256))
        return FALSE;

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



BOOL
_CardEjectNdisMiniportAdapter
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
    return _NdisConfigAdapterBind(szAdapter1, IOCTL_NDIS_DEREGISTER_ADAPTER);
}




BOOL SetRegistry_FromSzData(WCHAR **szRegistryData);



void
_InstallXWifiCard
(
WCHAR *pszStaticIpAddr   // L"100.100.0.2", NULL will give DHCP/AutoIp
)
{
#if 0
    WCHAR *szXwifiCardRegistry[] =
    {
        L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B]",
        L"    \"DisplayName\"=\"ndis XWIFI miniport driver\"",
        L"    \"Group\"=\"NDIS\"",
        L"    \"ImagePath\"=\"xwifi11b.dll\"",

        L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B\\Linkage]",
        L"    \"Route\"=multi_sz:\"XWIFI11B1\"",

        L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B1]",
        L"    \"DisplayName\"=\"ndis XWIFI miniport driver\"",
        L"    \"Group\"=\"NDIS\"",
        L"    \"ImagePath\"=\"xwifi11b.dll\"",

        L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B1\\Parms]",
        L"    \"BusNumber\"=dword:0",
        L"    \"BusType\"=dword:0",

        NULL
    };
    SetRegistry_FromSzData(szXwifiCardRegistry);

    if(!pszStaticIpAddr)
    {   //DHCP/AUTOIP
        WCHAR *szXwifiCardRegistry_DHCP_AUTO_IP[] =
        {
            L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B1\\Parms\\TCPIP]",
            L"   \"EnableDHCP\"=dword:1",
            L"   \"AutoCfg\"=dword:1",
            NULL
        };
        SetRegistry_FromSzData(szXwifiCardRegistry_DHCP_AUTO_IP);
    }
    else
    {   //STATIC
        WCHAR *szXwifiCardRegistry_STATIC_IP[] =
        {
            L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B1\\Parms\\TCPIP]",
            L"   \"EnableDHCP\"=dword:0",
            L"   \"AutoCfg\"=dword:0",
            L"   \"IpAddress\"=\"192.168.2.*\"",
            L"   \"Subnetmask\"=\"255.255.255.0\"",
            NULL
        };
        WCHAR szStaticIpEntry[128] = L"   \"IpAddress\"=\"";
        wcscat(szStaticIpEntry, pszStaticIpAddr);
        wcscat(szStaticIpEntry, L"\"");
        szXwifiCardRegistry_STATIC_IP[4] = szStaticIpEntry;
        SetRegistry_FromSzData(szXwifiCardRegistry_STATIC_IP);
    }
#else
    //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B]
    //    "DisplayName"="ndis XWIFI miniport driver"
    //    "Group"="NDIS"
    //    "ImagePath"="xwifi11b.dll"

    HKEY hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B", 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return;
    }
    WCHAR szDisplayName[] = L"ndis XWIFI miniport driver";
    RegSetValueEx(hKey1, L"DisplayName", 0, REG_SZ, (PBYTE)szDisplayName, (wcslen(szDisplayName)+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"Group", 0, REG_SZ, (PBYTE)L"NDIS", (wcslen(L"NDIS")+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"ImagePath", 0, REG_SZ, (PBYTE)L"xwifi11b.dll", (wcslen(L"xwifi11b.dll")+1)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B\Linkage]
    //    "Route"=multi_sz:"XWIFI11B1"
    hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B\\Linkage", 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        wprintf(L"RegCreateKeyEx() fail\n");
        return;
    }
    WCHAR szRoute[32] = L"XWIFI11B1";
    szRoute[wcslen(szRoute)+1] = L'\0';
    RegSetValueEx(hKey1, L"Route", 0, REG_MULTI_SZ, (PBYTE)szRoute, (wcslen(szRoute)+2)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B1]
    //    "DisplayName"="ndis XWIFI miniport driver"
    //    "Group"="NDIS"
    //    "ImagePath"="xwifi11b.dll"
    hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B1", 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return;
    }
    //WCHAR szDisplayName[] = L"ndis XWIFI miniport driver";
    RegSetValueEx(hKey1, L"DisplayName", 0, REG_SZ, (PBYTE)szDisplayName, (wcslen(szDisplayName)+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"Group", 0, REG_SZ, (PBYTE)L"NDIS", (wcslen(L"NDIS")+1)*sizeof(WCHAR));
    RegSetValueEx(hKey1, L"ImagePath", 0, REG_SZ, (PBYTE)L"xwifi11b.dll", (wcslen(L"xwifi11b.dll")+1)*sizeof(WCHAR));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B1\Parms]
    //    "BusNumber"=dword:0
    //    "BusType"=dword:0
    hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B1\\Parms", 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return;
    }
    DWORD dw1 = 0;
    RegSetValueEx(hKey1, L"BusNumber", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    RegSetValueEx(hKey1, L"BusType", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    RegCloseKey(hKey1);

    //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B1\Parms\TCPIP]
    hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B1\\Parms\\TCPIP", 0, NULL, 0, 0, NULL, &hKey1, NULL) != ERROR_SUCCESS) {
        g_pKato->Log(LOG_FAIL, L"RegCreateKeyEx() fail\n");
        return;
    }
    if(!pszStaticIpAddr)
    {
        //[HKEY_LOCAL_MACHINE\Comm\XWIFI11B1\Parms\TCPIP]
        //   "EnableDHCP"=dword:1
        //   "AutoCfg"=dword:1
        dw1 = 1;
        RegSetValueEx(hKey1, L"EnableDHCP", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"AutoCfg", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
    }
    else
    {
        //[HKEY_LOCAL_MACHINE\Comm\NDUMMY1\Parms\TCPIP]
        //   "EnableDHCP"=dword:0
        //   "AutoCfg"=dword:0
        //   "IpAddress"="192.168.0.1"
        //   "Subnetmask"="255.255.255.0"
        dw1 = 0;
        RegSetValueEx(hKey1, L"EnableDHCP", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"AutoCfg", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"IpAddress", 0, REG_SZ, (PBYTE)pszStaticIpAddr, (wcslen(pszStaticIpAddr)+1)*sizeof(WCHAR));
        RegSetValueEx(hKey1, L"Subnetmask", 0, REG_SZ, (PBYTE)L"255.255.255.0", (wcslen(L"255.255.255.0")+1)*sizeof(WCHAR));
    }
    RegCloseKey(hKey1);
#endif

    // ndisconfig adapter add ndummy ndummy1"
    WCHAR multiSz[64] = L"XWIFI11B*XWIFI11B1*";
    int iLen = wcslen(multiSz) + 1;
    for(WCHAR *p=multiSz; *p; p++)
    {
        if(*p == L'*')
            *p = L'\0';
    }
    DoNdisIOControl(
        IOCTL_NDIS_REGISTER_ADAPTER,
        multiSz,
        iLen * sizeof(WCHAR),
        NULL,
        NULL);
}



void
AskXwifiToRescanBssidList()
{
    // send command to XWIFI11B adapter driver
    MSGQUEUEOPTIONS mqo;
    memset (&mqo, 0, sizeof (MSGQUEUEOPTIONS));
    mqo.dwSize = sizeof(MSGQUEUEOPTIONS);
    mqo.dwFlags = MSGQUEUE_ALLOW_BROKEN;
    mqo.dwMaxMessages = 8;
    mqo.cbMaxMessage = 64;//MAX_XWIFI11B_COMMAND_SIZE;  // typical command will be ULONG, but for some future expansion keep is big
    mqo.bReadAccess = FALSE;

    HANDLE hMsgQ_Command = CreateMsgQueue(L"XWIFI11B-COMMAND-QUEUE", &mqo);
    if(hMsgQ_Command == NULL)
    {
        g_pKato->Log(LOG_FAIL, L"CreateMsgQueue failed");
    }
    else
    {
        DWORD dwCommand = 0x0002;
        if(!WriteMsgQueue(hMsgQ_Command, &dwCommand, sizeof(DWORD), 0, 0))
            g_pKato->Log(LOG_FAIL, L"WriteMsgQueue(hMsgQ_Command) error");
    }
}




#include <Iphlpapi.h>


// use statically allocated memory block
// to reduce risk of mem leaking from this stress code.

#define MEM_BLOCK_8K_SIZE (1024*8)

//#define USE_DYNAMIC_ALLOCATE_MEM_GetAdaptersInfo


#ifdef USE_DYNAMIC_ALLOCATE_MEM_GetAdaptersInfo

PIP_ADAPTER_INFO g_pAdapterInfo = NULL;

PIP_ADAPTER_INFO
GetAdaptersInfoWithMem
()
{
    ULONG ulSizeAdapterInfo = MEM_BLOCK_8K_SIZE;
    if(!g_pAdapterInfo)
        g_pAdapterInfo = (PIP_ADAPTER_INFO)LocalAlloc(LPTR, MEM_BLOCK_8K_SIZE);
    if(g_pAdapterInfo)
    {
        DWORD dwStatus = GetAdaptersInfo(g_pAdapterInfo, &ulSizeAdapterInfo);
        if (dwStatus != ERROR_SUCCESS)
        {
            LocalFree(g_pAdapterInfo);
            g_pAdapterInfo = NULL;
        }
    }
    return g_pAdapterInfo;
}

#else

UCHAR g_ucBulkMem[MEM_BLOCK_8K_SIZE];

PIP_ADAPTER_INFO
GetAdaptersInfoWithMem
()
{
    ULONG ulSizeAdapterInfo = MEM_BLOCK_8K_SIZE;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)g_ucBulkMem;
    DWORD dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
    if (dwStatus != ERROR_SUCCESS)
        return NULL;
    return pAdapterInfo;
}

#endif





PIP_ADAPTER_INFO
GetAdapterInfo
// returns IP_ADAPTER_INFO* of adapter1
// if not found (i.e. adapter1 is not installed), returns NULL
// always use globally allocated g_ucBulkMem block
(
IN WCHAR *szAdapter1	// adapter name 'NE20001', 'CISCO1'
)
{
    char szAdapter1A[64];
    wcstombs(szAdapter1A, szAdapter1, 64);
    for(PIP_ADAPTER_INFO pAdapterInfo=GetAdaptersInfoWithMem();
        pAdapterInfo != NULL;
        pAdapterInfo = pAdapterInfo->Next)
    {
        if(_stricmp(szAdapter1A, pAdapterInfo->AdapterName) == 0)
            return pAdapterInfo;
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




void
UnbindAllAdaptersExceptXwifi11b1()
{
    WCHAR szAdapterName[256];
    for(PIP_ADAPTER_INFO pAdapterInfo=GetAdaptersInfoWithMem();
        pAdapterInfo != NULL;
        pAdapterInfo = pAdapterInfo->Next)
    {
        g_pKato->Log(LOG_COMMENT, L"found adapter [%hs]", pAdapterInfo->AdapterName);
        if(
            (_stricmp("XWIFI11B1", pAdapterInfo->AdapterName) != 0) &&
            (_stricmp("VMINI1", pAdapterInfo->AdapterName) != 0) )    // unbinding VMINI1 kills KITL, bug 107775
        {
            mbstowcs(szAdapterName, pAdapterInfo->AdapterName, 256);
            g_pKato->Log(LOG_COMMENT, L"UNBIND adapter [%s]", szAdapterName);
            NdisConfigAdapterUnbind(szAdapterName);
        }
    }
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
    for(unsigned i=0; (i<15) && (pAddressList->IpAddress.String[i]); i++)
        szGatewayIpAddr[i] = (WCHAR)(pAddressList->IpAddress.String[i]);
    szGatewayIpAddr[i] = L'\0';
}



//UCHAR g_ucBulkMem[MEM_BLOCK_8K_SIZE];


BOOL
IpAddressRelease0Renew1
//
//
// return TRUE if adapter is bound to TCP/IP and has a valid IP address
//
(
IN WCHAR *szAdapter1,	// adapter name 'NE20001', 'CISCO1'
IN UINT iCmd    // 0=release, 1=renew
)
{
    if(!szAdapter1)
        return FALSE;

#if 0
    DWORD dwOutBufLen = 0;
    if(GetInterfaceInfo(NULL, &dwOutBufLen)!=NO_ERROR)
        return FALSE;
    PIP_INTERFACE_INFO pIfTable = (PIP_INTERFACE_INFO)LocalAlloc(LPTR, dwOutBufLen);
#endif
    DWORD dwOutBufLen = 0;
    if(GetInterfaceInfo(NULL, &dwOutBufLen)==NO_ERROR)  // no adapter case
        return FALSE;

    PIP_INTERFACE_INFO pIfTable = (PIP_INTERFACE_INFO) LocalAlloc(LPTR, dwOutBufLen);
    if(!pIfTable)
        return FALSE;

    if(GetInterfaceInfo(pIfTable, &dwOutBufLen)==NO_ERROR)
    {
        for(LONG i=0; i<pIfTable->NumAdapters; i++)
        {
            PIP_ADAPTER_INDEX_MAP pAdapterI = &pIfTable->Adapter[i];
            if(_wcsicmp(pAdapterI->Name, szAdapter1)==0)
            {
                DWORD dwR1 = 0;
                switch(iCmd)
                {
                case 0: // release
                    dwR1 = IpReleaseAddress(pAdapterI);
                    g_pKato->Log(LOG_COMMENT, L"ip released : %s", pAdapterI->Name);
                    break;

                case 1: // renew
                    dwR1 = IpReleaseAddress(pAdapterI);
                    // if I don't call IpReleaseAddress(),
                    // the autoip doesn't give 169.254 address when calling IpRenewAddress()
                    // need further investigation later.
                    dwR1 = IpRenewAddress(pAdapterI);
                    g_pKato->Log(LOG_COMMENT, L"ip renewed : %s", pAdapterI->Name);
                    break;
                }
                LocalFree(pIfTable);
                return (dwR1==NO_ERROR);
            }
        }
    }
    else
    {
        g_pKato->Log(LOG_COMMENT, L"ip-addr release/renew failed due to GetInterfaceInfo() failure");
    }
    LocalFree(pIfTable);
    return FALSE;
}






WCHAR g_szCommandOutput[0x10000];
// we are going to keep only first 64k-chars of output.


BOOL
RunCmd
// return TRUE if ran successfully.
// output value goes to the g_szCommandOutput
(
IN WCHAR *lpCmdLine1
)
{
    WCHAR lpCmdLine[MAX_PATH];
    memset(lpCmdLine, 0, sizeof(lpCmdLine));
    wcsncpy(lpCmdLine, lpCmdLine1, MAX_PATH-1);

    g_szCommandOutput[0] = L'\0';

    LPWSTR szCommand = lpCmdLine;
    while(*szCommand && _istspace(*szCommand))   // remove leading white-space
        szCommand++;
    g_pKato->Log(LOG_COMMENT, L"command %s", szCommand);

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

    WCHAR szDebugConsoleName[MAX_PATH];
    GetTempFileName(L"\\Temp", L"z", 0, szDebugConsoleName);

    WCHAR szStdout[MAX_PATH];
    DWORD dwStdoutLen = MAX_PATH;
    GetStdioPathW(1, szStdout, &dwStdoutLen);
    SetStdioPathW(1, szDebugConsoleName);

    // run command
    PROCESS_INFORMATION proc_info;
    memset((void*)&proc_info, 0, sizeof(proc_info));
    if(!CreateProcess(szCommand, szArg, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &proc_info))
    {
        g_pKato->Log(LOG_FAIL, L"Cannot run '%s'. CreateProcess() failed, error=0x%08X", szCommand, GetLastError());
        SetStdioPathW(1, szStdout);
        DeleteFile(szDebugConsoleName);
        return FALSE;
    }
    WaitForSingleObject(proc_info.hProcess, INFINITE);
    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    SetStdioPathW(1, szStdout);

    FILE *fp = _wfopen(szDebugConsoleName, L"r");
    if(fp != NULL)
    {
        WCHAR szText[1024];
        while(!feof(fp))
        {
            if(fgetws(szText, 1024, fp))
            {
                if((wcslen(g_szCommandOutput)+wcslen(szText)) >= 0x10000)
                    break;
                wcscat(g_szCommandOutput, szText);
            }
        }
        fclose(fp);
    }
    else
        g_pKato->Log(LOG_COMMENT, L"no output");
    DeleteFile(szDebugConsoleName);
    return TRUE;
}





void
DumpCommandOutputToTuxEcho
// dump g_szCommandOutput to the Tux Echo output
()
{
    g_pKato->Log(LOG_COMMENT, L"g_szCommandOutput[] =");
    for(WCHAR *p1=g_szCommandOutput; *p1 && ((p1-g_szCommandOutput)<sizeof(g_szCommandOutput)); )
    {
        for(WCHAR *p2=p1; *p2 && (*p2 & 0xFFE0) && ((p2-g_szCommandOutput)<sizeof(g_szCommandOutput)); p2++)
            ;
        WCHAR wchr1 = *p2;
        *p2 = L'\0';
        g_pKato->Log(LOG_COMMENT, L"%s", p1);
        *p2 = wchr1;
        for(; *p2 && !(*p2 & 0xFFE0); p2++)
            ;
        p1 = p2;
    }
}




BOOL
SetRegistry_FromSzData
(
WCHAR **szRegistryData
)
{
    HANDLE hMutex_BssidListRegistry = CreateMutex(NULL, TRUE, L"XWIFI11B-BSSID-LIST");
    WaitForSingleObject(hMutex_BssidListRegistry, INFINITE);

    HKEY hKey1 = NULL;
    WCHAR szText1[512];
    for(; *szRegistryData; szRegistryData++)
    {
        wcscpy(szText1, *szRegistryData);
        WCHAR *sz1 = szText1;

        while(*sz1 && iswspace(*sz1))    // skip leading white space
            sz1++;
        if(!*sz1)   // empty line
            continue;

        if(*sz1 == L'[') // registry key name
        {
            if(hKey1)
                RegCloseKey(hKey1);
            hKey1 = NULL;
            WCHAR *sz2 = wcschr(sz1, L']');
            WCHAR *szKeyName = wcschr(sz1, L'\\');
            if(!sz2 ||
                _wcsnicmp(sz1, L"[HKEY_", wcslen(L"[HKEY_")) ||
                !szKeyName)
            {
                g_pKato->Log(LOG_COMMENT, L"error: %s\n", sz1);
                continue;
            }
            *sz2 = L'\0';
            szKeyName++;
            DWORD dwDisposition;
            if(_wcsnicmp(sz1, L"[HKEY_LOCAL_MACHINE", wcslen(L"[HKEY_LOCAL_MACHINE"))==0)
            {
                if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, NULL, 0, 0, NULL, &hKey1, &dwDisposition) == ERROR_SUCCESS)
                    g_pKato->Log(LOG_COMMENT, L"registry key HKEY_LOCAL_MACHINE\\%s created [%s]", szKeyName, dwDisposition==REG_CREATED_NEW_KEY ? L"new" : L"existing");
                else
                    g_pKato->Log(LOG_COMMENT, L"RegCreateKeyEx(HKEY_LOCAL_MACHINE %s) fail\n", szKeyName);
            }

            if(_wcsnicmp(sz1, L"[HKEY_CURRENT_USER", wcslen(L"[HKEY_CURRENT_USER"))==0)
            {
                if(RegCreateKeyEx(HKEY_CURRENT_USER, szKeyName, 0, NULL, 0, 0, NULL, &hKey1, &dwDisposition) == ERROR_SUCCESS)
                    g_pKato->Log(LOG_COMMENT, L"registry key HKEY_CURRENT_USER\\%s created [%s]", szKeyName, dwDisposition==REG_CREATED_NEW_KEY ? L"new" : L"existing");
                else
                    g_pKato->Log(LOG_COMMENT, L"RegCreateKeyEx(HKEY_CURRENT_USER %s) fail\n", szKeyName);
            }
        }
        else if(*sz1 == L'"') // registry value name
        {
            if(hKey1 == NULL)
            {
                g_pKato->Log(LOG_COMMENT, L"ignoring : %s\n", sz1);
                continue;
            }
            sz1++;
            WCHAR *szVariableName = sz1;
            WCHAR *szVariableNameEnd = wcschr(szVariableName, L'"');
            WCHAR *szVariableValue = wcschr(szVariableNameEnd+1, L'=');
            if(!szVariableNameEnd || !szVariableValue)
            {
                g_pKato->Log(LOG_COMMENT, L"error: %s\n", sz1);
                continue;
            }
            *szVariableNameEnd = L'\0';
            szVariableValue++;
            while(iswspace(*szVariableValue)) // skip possible white spaces
                szVariableValue++;
            if(*szVariableValue == L'"')
            {   // case: "ImagePath"="ndummy.dll"
                szVariableValue++;
                WCHAR *szVariableValueEnd = wcschr(szVariableValue, L'"');
                if(szVariableValueEnd)
                    *szVariableValueEnd = L'\0';
                if(ERROR_SUCCESS !=
                    RegSetValueEx(hKey1, szVariableName, 0, REG_SZ, (PBYTE)szVariableValue, (wcslen(szVariableValue)+1)*sizeof(WCHAR)))
                    g_pKato->Log(LOG_COMMENT, L"error RegSetValueEx(%s,string:%s)\n", szVariableName, szVariableValue);
            }
            else if(0==_wcsnicmp(szVariableValue, L"dword:", wcslen(L"dword:")))
            {   // case: "BusNumber"=dword:0
                DWORD dw1 = 0;
                szVariableValue = wcschr(szVariableValue, L':');
                if(szVariableValue)
                    dw1 = _wtoi(szVariableValue+1);
                if(ERROR_SUCCESS !=
                    RegSetValueEx(hKey1, szVariableName, 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD)))
                    g_pKato->Log(LOG_COMMENT, L"error RegSetValueEx(%s,dword:%d)\n", szVariableName, dw1);
            }
            else if(0==_wcsnicmp(szVariableValue, L"multi_sz:", wcslen(L"multi_sz:")))
            {   // case: "Route"=multi_sz:"NDUMMY1","1113"
                szVariableValue = wcschr(szVariableValue, L'"');
                if(!szVariableValue)
                {
                    g_pKato->Log(LOG_COMMENT, L"error: %s\n", sz1);
                    continue;
                }
                WCHAR* wszMultiSzVariableValue = szVariableValue + 1;
                WCHAR *pStart = wszMultiSzVariableValue;
                WCHAR *pEnd = pStart;
                BOOL bError = FALSE;
                while(TRUE && (pEnd-szText1<(sizeof(szText1)/sizeof(szText1[0]))))
                {
                    pEnd = wcschr(pStart, L'"');
                    if(!pEnd)
                    {
                        bError = TRUE;
                        break;
                    }

                    *pEnd = L'\0';   // NDUMMY1#,"1113"
                    pEnd++;

                    pStart = wcschr(pEnd, L'"');
                    if(!pStart)
                        break;
                    wcscpy(pEnd, pStart+1); // NDUMMY1#1113"
                }
                if(bError)
                {
                    g_pKato->Log(LOG_COMMENT, L"error: %s\n", sz1);
                    continue;
                }
                *pEnd = L'\0';  // one more NULL char
                if(ERROR_SUCCESS !=
                    RegSetValueEx(hKey1, szVariableName, 0, REG_MULTI_SZ, (PBYTE)wszMultiSzVariableValue, (pEnd-wszMultiSzVariableValue+1)*sizeof(WCHAR)))
                    g_pKato->Log(LOG_COMMENT, L"error RegSetValueEx(%s,multi_sz:%s)\n", szVariableName, szVariableValue);
            }
            else
                g_pKato->Log(LOG_COMMENT, L"ignoring : %s\n", sz1);
        }
        else if(*sz1 == L';') // comment, ignore
        {
            ;
        }
        else
        {
            g_pKato->Log(LOG_COMMENT, L"ignoring : %s\n", sz1);
        }
    }
    if(hKey1)
        RegCloseKey(hKey1);
    ReleaseMutex(hMutex_BssidListRegistry);
    CloseHandle(hMutex_BssidListRegistry);
    return TRUE;
}




WCHAR *g_szBasicSsidSet[] =
{
    //AD-NO-ENCRYPTION -adhoc
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-NO-ENCRYPTION]",
    L"   \"InfrastructureMode\"=dword:0",

    // AD-WEP40-OPEN -adhoc -wep 1/0x1234567890
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP40-OPEN]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:0",

    // AD-WEP40-SHARED -adhoc -auth shared -wep 1/0x1234567890
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP40-SHARED]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:1",

    // AD-WEP104-OPEN -adhoc -wep 1/0x12345678901234567890123456
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP104-OPEN]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:0",

    // AD-WEP104-SHARED -adhoc -wep 1/0x12345678901234567890123456
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP104-SHARED]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:1",

    // NO-ENCRYPTION(DHCP)
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\NO-ENCRYPTION(DHCP)]",
    L"   \"InfrastructureMode\"=dword:1",
    L"   \"Privacy\"=dword:0",
    L"   \"AuthMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",

    // WEP40-OPEN(DHCP)
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\WEP40-OPEN(DHCP)]",
    L"   \"InfrastructureMode\"=dword:1",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",

    // WEP104-OPEN(DHCP)
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\WEP104-OPEN(DHCP)]",
    L"   \"InfrastructureMode\"=dword:1",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",

    // WEP40-SHARED(DHCP)
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\WEP40-SHARED(DHCP)]",
    L"   \"InfrastructureMode\"=dword:1",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:1",
    L"   \"dhcp\"=\"10.10.0.*\"",

    // WEP104-SHARED(DHCP)
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\WEP104-SHARED(DHCP)]",
    L"   \"InfrastructureMode\"=dword:1",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:1",
    L"   \"dhcp\"=\"10.10.0.*\"",

    NULL,
};




WCHAR *g_szAdditionalSsidSet_AD_NO_ENCRYPTION_DHCP[] =
{
    //AD-NO-ENCRYPTION(DHCP) -adhoc
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-NO-ENCRYPTION(DHCP)]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",
    NULL
};

WCHAR *g_szAdditionalSsidSet_AD_WEP40_OPEN_DHCP[] =
{
    // AD-WEP40-OPEN(DHCP) -adhoc -wep 1/0x1234567890
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP40-OPEN(DHCP)]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",
    NULL
};


WCHAR *g_szAdditionalSsidSet_AD_WEP40_SHARED_DHCP[] =
{
    // AD-WEP40-SHARED(DHCP) -adhoc -auth shared -wep 1/0x1234567890
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP40-SHARED(DHCP)]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x1234567890\"",
    L"   \"AuthMode\"=dword:1",
    L"   \"dhcp\"=\"10.10.0.*\"",
    NULL
};

WCHAR *g_szAdditionalSsidSet_AD_WEP104_OPEN_DHCP[] =
{
    // AD-WEP104-OPEN(DHCP) -adhoc -wep 1/0x12345678901234567890123456
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP104-OPEN(DHCP)]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:0",
    L"   \"dhcp\"=\"10.10.0.*\"",
    NULL
};

WCHAR *g_szAdditionalSsidSet_AD_WEP104_SHARED_DHCP[] =
{
    // AD-WEP104-SHARED(DHCP) -adhoc -wep 1/0x12345678901234567890123456
    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\AD-WEP104-SHARED(DHCP)]",
    L"   \"InfrastructureMode\"=dword:0",
    L"   \"Privacy\"=dword:1",
    L"   \"key-index\"=dword:1",
    L"   \"key\"=\"0x12345678901234567890123456\"",
    L"   \"AuthMode\"=dword:1",
    L"   \"dhcp\"=\"10.10.0.*\"",
    NULL
};



void Prepare_AdditionalTestSsidSet
// test SSID set
(
WCHAR *szTestSsid
)
{
    // add more SSIDs depends on the test case
    // adhoc net
    // 1. predefined adhoc nets don't have DHCP, we need to add DHCP enabled adhoc nets if test requires.
    // "-ssid WEP104-SHARED(DHCP)::-auth:shared:"
    if(wcsstr(szTestSsid, L"-adhoc") && wcsstr(szTestSsid, L"(DHCP)"))
    {
        if(!_wcsnicmp(szTestSsid, L"AD-NO-ENCRYPTION(DHCP)", wcslen(L"AD-NO-ENCRYPTION(DHCP)")))
        {
            if(!SetRegistry_FromSzData(g_szAdditionalSsidSet_AD_NO_ENCRYPTION_DHCP))
                g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        }
        else if(!_wcsnicmp(szTestSsid, L"AD-WEP40-OPEN(DHCP)", wcslen(L"AD-WEP40-OPEN(DHCP)")))
        {
            if(!SetRegistry_FromSzData(g_szAdditionalSsidSet_AD_WEP40_OPEN_DHCP))
                g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        }
        else if(!_wcsnicmp(szTestSsid, L"AD-WEP40-SHARED(DHCP)", wcslen(L"AD-WEP40-SHARED(DHCP)")))
        {
            if(!SetRegistry_FromSzData(g_szAdditionalSsidSet_AD_WEP40_SHARED_DHCP))
                g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        }
        else if(!_wcsnicmp(szTestSsid, L"AD-WEP104-OPEN(DHCP)", wcslen(L"AD-WEP104-OPEN(DHCP)")))
        {
            if(!SetRegistry_FromSzData(g_szAdditionalSsidSet_AD_WEP104_OPEN_DHCP))
                g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        }
        else if(!_wcsnicmp(szTestSsid, L"AD-WEP104-SHARED(DHCP)", wcslen(L"AD-WEP104-SHARED(DHCP)")))
        {
            if(!SetRegistry_FromSzData(g_szAdditionalSsidSet_AD_WEP104_SHARED_DHCP))
                g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        }
        else
            g_pKato->Log(LOG_COMMENT, L"not adding any registry for this SSID [%s]", szTestSsid);
    }
}



BOOL Prepare_TestSsidSet
// test SSID set
(
WCHAR *szTestSsid
)
{
    static BOOL bTestSetAlreadyPrepared = FALSE;
    if(bTestSetAlreadyPrepared)
        return TRUE;

    if(!SetRegistry_FromSzData(g_szBasicSsidSet))
        g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
    Prepare_AdditionalTestSsidSet(szTestSsid);
    bTestSetAlreadyPrepared = TRUE;
    return TRUE;
}




BOOL
RemoveKeyFromRegistryHKLM
// remove key and sub-keys from the HKEY_LOCAL_MACHINE registry
// return TRUE if key was deleted [or not exist]
(
WCHAR *szKeyToDelete
)
{
    if(!szKeyToDelete)
        return FALSE;

    HANDLE hMutex_BssidListRegistry = NULL;
    if(wcsstr(szKeyToDelete, L"Comm\\XWIFI11B-BSSID-LIST")!=NULL)
    {
        hMutex_BssidListRegistry = CreateMutex(NULL, TRUE, L"XWIFI11B-BSSID-LIST");
        WaitForSingleObject(hMutex_BssidListRegistry, INFINITE);
    }

    BOOL bResult = FALSE;
    for(unsigned iRetry=0; iRetry<20; iRetry++)
    {
        HKEY hKey1 = NULL;
        if(ERROR_SUCCESS!=RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyToDelete, 0, 0, &hKey1))
        {
            g_pKato->Log(LOG_COMMENT, L"[HKLM\\%s] not exist. ok", szKeyToDelete);
            bResult = TRUE;
            break;
        }
        RegCloseKey(hKey1);

        if(ERROR_SUCCESS==RegDeleteKey(HKEY_LOCAL_MACHINE, szKeyToDelete))
        {
            g_pKato->Log(LOG_COMMENT, L"registry key [HKEY_LOCAL_MACHINE\\%s] deleted", szKeyToDelete);
            bResult = TRUE;
            break;
        }
        g_pKato->Log(LOG_COMMENT, L"deleting [HKEY_LOCAL_MACHINE\\%s] failed [%d]", szKeyToDelete, iRetry);
    
        // randomize sleep time, sleep 1(+/-).5 sec
        DWORD dw1 = Random();

        if(dw1 & 0x0200)    // 0x200=512
            dw1 = 1000 - (dw1&0x01FF);
        else
            dw1 = 1000 + (dw1&0x01FF);

        g_pKato->Log(LOG_COMMENT, L"sleep %d ms and retry", dw1);
        Sleep(dw1);
    }
    if(bResult == FALSE)
        g_pKato->Log(LOG_FAIL, L"failed to delete registry key [HKEY_LOCAL_MACHINE\\%s]", szKeyToDelete);
    if(hMutex_BssidListRegistry)
    {
        ReleaseMutex(hMutex_BssidListRegistry);
        CloseHandle(hMutex_BssidListRegistry);
    }
    return bResult;
}




void
PowerOff_ConnectedAp
(
WCHAR* pszTestSsid
)
{
    if(!pszTestSsid)
        return;

    unsigned iSsidLen = wcslen(pszTestSsid);
    WCHAR *szOptionMark = wcschr(pszTestSsid, L':');
    if(szOptionMark)
        iSsidLen = szOptionMark - pszTestSsid + 1;

    // L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\NO-ENCRYPTION(DHCP)]",
    WCHAR szRegData[MAX_PATH] = L"Comm\\XWIFI11B-BSSID-LIST\\";
    wcsncat(szRegData, pszTestSsid, iSsidLen);
    szRegData[wcslen(L"Comm\\XWIFI11B-BSSID-LIST")+iSsidLen] = L'\0';
    RemoveKeyFromRegistryHKLM(szRegData);
    Sleep(500);
    AskXwifiToRescanBssidList();
    Sleep(500);
}




void
PowerOn_ConnectedAp
(
WCHAR* pszTestSsid
)
{
    if(!pszTestSsid)
        return;

    unsigned iSsidLen = wcslen(pszTestSsid);
    WCHAR *szOptionMark = wcschr(pszTestSsid, L':');
    if(szOptionMark)
        iSsidLen = szOptionMark - pszTestSsid;
    if(iSsidLen>32)
        iSsidLen = 32;
    WCHAR szSSID1[36];
    wcsncpy(szSSID1, pszTestSsid, iSsidLen);
    szSSID1[iSsidLen] = L'\0';

    for(WCHAR **szRegistryData = g_szBasicSsidSet; *szRegistryData; szRegistryData++)
        if(wcsstr(*szRegistryData, szSSID1)!=NULL)
            break;
    if(*szRegistryData)
    {
        szRegistryData++;
        for(WCHAR **szRegistryData_End = szRegistryData; *szRegistryData_End; szRegistryData_End++)
            if(wcsstr(*szRegistryData_End, L"[HKEY_LOCAL_MACHINE")!=NULL)
                break;
        WCHAR *szRegistryData_End_Original = *szRegistryData_End;
        *szRegistryData_End = NULL;
        szRegistryData--;
        if(!SetRegistry_FromSzData(szRegistryData))
            g_pKato->Log(LOG_COMMENT, L"failed to Prepare_TestSsidSet");
        *szRegistryData_End = szRegistryData_End_Original;
    }
    else    // this AP entry does not exist
    {
        Prepare_AdditionalTestSsidSet(pszTestSsid);
    }
    Sleep(500);
    AskXwifiToRescanBssidList();
    Sleep(500);
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
        if(!_wcsnicmp(p, L"open", 4))
            wzcConfig1.AuthenticationMode = Ndis802_11AuthModeOpen;
        else if(!_wcsnicmp(p, L"shared", 4))
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
                WZCDeleteIntfObj(&Intf);
                return;
            }
        }
        g_pKato->Log(LOG_COMMENT, L"SSID List has [%d] entries.", uiNumberOfItems);
        uiNumberOfItems += 1;

        DWORD dwDataLen = sizeof(WZC_802_11_CONFIG_LIST) + uiNumberOfItems*sizeof(WZC_WLAN_CONFIG);
        WZC_802_11_CONFIG_LIST *pNewConfigList = (WZC_802_11_CONFIG_LIST *)LocalAlloc(LPTR, dwDataLen);
        pNewConfigList->NumberOfItems = uiNumberOfItems;
        pNewConfigList->Index = uiNumberOfItems;
        if(pConfigList)
        {
            if(pConfigList->NumberOfItems)
            {
                pNewConfigList->Index = pConfigList->Index;
                memcpy(pNewConfigList->Config, pConfigList->Config, (uiNumberOfItems)*sizeof(WZC_WLAN_CONFIG));
            }
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
    }
    else
    {
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
    }
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
    if(g_szWiFiAdapter1)
        return;

    INTFS_KEY_TABLE IntfsTable;
    IntfsTable.dwNumIntfs = 0;
    IntfsTable.pIntfs = NULL;

    DWORD dwError = WZCEnumInterfaces(NULL, &IntfsTable);

    if(dwError != ERROR_SUCCESS)
    {
        g_szWiFiAdapter1 = NULL;
        g_pKato->Log(LOG_FAIL, L"WZCEnumInterfaces() error 0x%08X", dwError);
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

    DEBUGCHK(IntfsTable.pIntfs[0].wszGuid);
    wcscpy(g_szWiFiAdapterNameActualData, IntfsTable.pIntfs[0].wszGuid);
    g_szWiFiAdapter1 = g_szWiFiAdapterNameActualData;
    LocalFree(IntfsTable.pIntfs);
}










#define ID_MSG_DO_SOMETHING              0x00001000

#define ID_IF1_COMMAND_BITS              0x00000F00
#define ID_IF1_COMMAND_INSERT_CARD       0x00000100
#define ID_IF1_COMMAND_CHECK_IPHLP_API   0x00000200
#define ID_IF1_COMMAND_CLEAR_TEST_SSID   0x00000300
#define ID_IF1_COMMAND_PREPARE_TEST_SSID 0x00000400
#define ID_IF1_COMMAND_CHECK_IPCONFIG    0x00000500
#define ID_IF1_COMMAND_CHECK_NDISCONFIG  0x00000600
#define ID_IF1_COMMAND_CHECK_PING        0x00000700
#define ID_IF1_COMMAND_QUERY_SSID        0x00000800



TESTPROCAPI
IF1
// test initial things. check if card functions basic features
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, L"IF1: Initial Test");
    //g_pKato->Log(LOG_FAIL, L"LOG_FAIL");

    if(!(lpFTE->dwUserData & ID_MSG_DO_SOMETHING))
    {
        g_pKato->Log(LOG_COMMENT, L"no command asked to do");
        return TPR_PASS;
    }

    DWORD dwCommandId = lpFTE->dwUserData & ID_IF1_COMMAND_BITS;
    WCHAR *pszStaticIpAddr;
    WCHAR *pszTestSsid = NULL;
    if(GetOption(argc, argv, L"ssid", &pszTestSsid)>=0)
        g_pKato->Log(LOG_COMMENT, L"-ssid option = %s", pszTestSsid);

    switch(dwCommandId)
    {
    case ID_IF1_COMMAND_INSERT_CARD:
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1)
        {
            g_pKato->Log(LOG_COMMENT, L"found a wifi adapter, %s", g_szWiFiAdapter1);
            if(_wcsnicmp(g_szWiFiAdapter1, L"XWIFI11B1", wcslen(L"XWIFI11B1"))==0)
            {
                g_pKato->Log(LOG_COMMENT, L"force to eject XWIFI11B1");
                _CardEjectNdisMiniportAdapter(L"XWIFI11B1");
                // wait for card goes off
                for(unsigned i=0; i<100; i++)
                {
                    g_pKato->Log(LOG_COMMENT, L"checking XWIFI11B1 NIC");
                    Sleep(1000);
                    if(!AdapterExist(L"XWIFI11B1"))
                        break;
                }
                if(AdapterExist(L"XWIFI11B1"))
                    g_pKato->Log(LOG_FAIL, L"tried to eject XWIFI11B1 card, but it is still in the system");
                else
                    g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 card removed ok");

                // wait for UI goes off
                for(i=0; i<30; i++)
                {
                    g_pKato->Log(LOG_COMMENT, L"checking XWIFI11B1 UI");
                    Sleep(1000);
                    if(!IsDialogWindowPoppedUp(L"XWIFI11B1"))
                        break;
                }
                if(!IsDialogWindowPoppedUp(L"XWIFI11B1"))
                    g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 UI is NOT visible as expected");
                else
                    g_pKato->Log(LOG_FAIL, L"XWIFI11B1 UI is still visible [FAIL]");

                // remove registry key
                if( !RemoveKeyFromRegistryHKLM(L"Comm\\XWIFI11B") ||
                    !RemoveKeyFromRegistryHKLM(L"Comm\\XWIFI11B1"))
                    g_pKato->Log(LOG_FAIL, L"cannot clean registry for XWIFI11B1");
                else
                    g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 registry cleaned");
                g_szWiFiAdapter1 = NULL;
            }
            else
            {
                g_pKato->Log(LOG_COMMENT, L"WARNING: wifi adapter [%s] found.", g_szWiFiAdapter1);
                g_pKato->Log(LOG_COMMENT, L"test will continue...");
                g_pKato->Log(LOG_COMMENT, L"some test cases may fail since these tests are programmed to work with XWIFI11B1");
                g_pKato->Log(LOG_COMMENT, L"please remove the [%s] card and restart test", g_szWiFiAdapter1);
                break;
            }
        }
        else
            g_pKato->Log(LOG_COMMENT, L"no wifi adapter found");

        DEBUGCHK(!g_szWiFiAdapter1);

        pszStaticIpAddr = NULL;
        if(GetOption(argc, argv, L"static", &pszStaticIpAddr)>=0)
        {   // check if arg has "-static ip:100.100.0.2" option
            g_pKato->Log(LOG_COMMENT, L"inserting XWIFI card with a static %s", pszStaticIpAddr);
            while(*pszStaticIpAddr && !isdigit(*pszStaticIpAddr))
                pszStaticIpAddr++;
        }
        _InstallXWifiCard(pszStaticIpAddr);

        for(int i=0; i<10; i++)
        {
            Sleep(1000);
            FindWirelessNetworkDevice();
            if(g_szWiFiAdapter1)
                break;
        }
        if(g_szWiFiAdapter1)
            g_pKato->Log(LOG_COMMENT, L"found a wifi adapter, %s", g_szWiFiAdapter1);
        else
            g_pKato->Log(LOG_FAIL, L"cannot find a wifi adapter");

        break;

    case ID_IF1_COMMAND_CHECK_IPHLP_API:
        g_pKato->Log(LOG_COMMENT, L"Unbind all adapters except XWIFI11B1");
        UnbindAllAdaptersExceptXwifi11b1();
        Sleep(1000);

        // check NDIS
        if(IsAdapterInstalled(L"XWIFI11B1"))
            g_pKato->Log(LOG_COMMENT, L"NDIS reports XWIFI11B1 adapter");
        else
            g_pKato->Log(LOG_FAIL, L"NDIS doesn't report XWIFI11B1 adapter");

        // check IPHELPER API
        if(AdapterExist(L"XWIFI11B1"))
            g_pKato->Log(LOG_COMMENT, L"IP-HELPER API GetAdaptersInfo() reports XWIFI11B1 adapter");
        else
            g_pKato->Log(LOG_FAIL, L"IP-HELPER API GetAdaptersInfo() doesn't report XWIFI11B1 adapter");
        break;

    case ID_IF1_COMMAND_CLEAR_TEST_SSID:
        RemoveKeyFromRegistryHKLM(L"Comm\\XWIFI11B-BSSID-LIST");
        break;

    case ID_IF1_COMMAND_PREPARE_TEST_SSID:
        Prepare_TestSsidSet(pszTestSsid);
        break;

    case ID_IF1_COMMAND_QUERY_SSID:
        RunCmd(L"wzctool -q");
        DumpCommandOutputToTuxEcho();
        break;

    case ID_IF1_COMMAND_CHECK_IPCONFIG:
        if(RunCmd(L"ipconfig"))
        {
            BOOL bXwifi11b1Listed = FALSE;
            for(WCHAR *szAdapter = wcsstr(g_szCommandOutput, L"Ethernet adapter [");
                szAdapter && !bXwifi11b1Listed;
                szAdapter = wcsstr(szAdapter, L"Ethernet adapter ["))
            {
                szAdapter += wcslen(L"Ethernet adapter [");
                if(_wcsnicmp(szAdapter, L"XWIFI11B1", wcslen(L"XWIFI11B1"))==0)
                    bXwifi11b1Listed = TRUE;
            }
            if(bXwifi11b1Listed)
                g_pKato->Log(LOG_COMMENT, L"ipconfig shows XWIFI11B1");
            else
                g_pKato->Log(LOG_FAIL, L"ipconfig not shows XWIFI11B1");
            DumpCommandOutputToTuxEcho();
        }
        else
            g_pKato->Log(LOG_FAIL, L"fail to run ipconfig.exe");
        break;

    case ID_IF1_COMMAND_CHECK_NDISCONFIG:
        RunCmd(L"ndisconfig");
        DumpCommandOutputToTuxEcho();
        break;

    case ID_IF1_COMMAND_CHECK_PING:
        break;

    default:
        g_pKato->Log(LOG_COMMENT, L"unknown command 0x%08X", lpFTE->dwUserData);
    }

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}





#define ID_F1_COMMAND_BITS             0x00000F00
#define ID_F1_COMMAND_RESET            0x00000000
#define ID_F1_COMMAND_CONNECT          0x00000100
#define ID_F1_COMMAND_BIND             0x00000200
#define ID_F1_COMMAND_UNBIND           0x00000300
#define ID_F1_COMMAND_INSERT           0x00000400
#define ID_F1_COMMAND_EJECT            0x00000500
#define ID_F1_COMMAND_ADAPTER_PWR_UP   0x00000600
#define ID_F1_COMMAND_ADAPTER_PWR_DOWN 0x00000700
#define ID_F1_COMMAND_AP_PWR_ON        0x00000800
#define ID_F1_COMMAND_AP_PWR_OFF       0x00000900
#define ID_F1_COMMAND_IP_RENEW         0x00000A00
#define ID_F1_COMMAND_IP_RELEASE       0x00000B00
#define ID_F1_COMMAND_ROAM12           0x00000C00
#define ID_F1_COMMAND_ROAM21           0x00000D00


#define ID_F1_TEST_IP_BITS               0x000000F0
#define ID_F1_CHECK_AND_WAIT_FOR_ZERO_IP 0x00000020
#define ID_F1_CHECK_AND_WAIT_FOR_SOME_IP 0x00000030


#define ID_F1_TEST_UI_BITS 0x000000F
#define ID_F1_CHECK_AND_WAIT_FOR_UI_OFF  0x00000002
#define ID_F1_CHECK_AND_WAIT_FOR_UI_ON   0x00000003


extern FUNCTION_TABLE_ENTRY g_lpFTE[];




TESTPROCAPI
F1
// functional test
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, L"Functional Test");
    //g_pKato->Log(LOG_FAIL, L"LOG_FAIL");

    if(!(lpFTE->dwUserData & ID_MSG_DO_SOMETHING))
    {
        g_pKato->Log(LOG_COMMENT, L"no command asked to do");
        return TPR_PASS;
    }

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
        g_pKato->Log(LOG_COMMENT, L"no wifi adapter found");

    DWORD dwT_Command = GetTickCount();
    DWORD dwCommandId = lpFTE->dwUserData & ID_F1_COMMAND_BITS;
    WCHAR *pszTestSsid;
    if(GetOption(argc, argv, L"ssid", &pszTestSsid)>=0)
        g_pKato->Log(LOG_COMMENT, L"-ssid option = %s", pszTestSsid);

    switch(dwCommandId)
    {
    case ID_F1_COMMAND_RESET:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_RESET");
        ResetPreferredList(L"XWIFI11B1");
        break;

    case ID_F1_COMMAND_CONNECT:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_CONNECT");
        AddSsidToThePreferredList(L"XWIFI11B1", pszTestSsid);
        break;

    case ID_F1_COMMAND_UNBIND:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_UNBIND");
        //RunCmd(L"ndisconfig adapter unbind XWIFI11B1");
        NdisConfigAdapterUnbind(L"XWIFI11B1");
        break;

    case ID_F1_COMMAND_BIND:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_BIND");
        //RunCmd(L"ndisconfig adapter bind XWIFI11B1");
        NdisConfigAdapterBind(L"XWIFI11B1");
        break;

    case ID_F1_COMMAND_INSERT:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_INSERT");
        g_szWiFiAdapter1 = NULL;
        FindWirelessNetworkDevice();
        if(g_szWiFiAdapter1)
            g_pKato->Log(LOG_COMMENT, L"found a wifi adapter, %s", g_szWiFiAdapter1);
        else
            _CardInsertNdisMiniportAdapter(L"XWIFI11B");
        break;

    case ID_F1_COMMAND_EJECT:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_EJECT");
        _CardEjectNdisMiniportAdapter(L"XWIFI11B1");
        break;

    case ID_F1_COMMAND_ADAPTER_PWR_UP:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_ADAPTER_PWR_UP");
        AdapterPowerOnOff(L"XWIFI11B1", 1);
        break;

    case ID_F1_COMMAND_ADAPTER_PWR_DOWN:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_ADAPTER_PWR_DOWN");
        AdapterPowerOnOff(L"XWIFI11B1", 0);
        break;

    case ID_F1_COMMAND_AP_PWR_ON:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_AP_PWR_ON");
        PowerOn_ConnectedAp(pszTestSsid);
        break;

    case ID_F1_COMMAND_AP_PWR_OFF:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_AP_PWR_OFF");
        PowerOff_ConnectedAp(pszTestSsid);
        break;

    case ID_F1_COMMAND_IP_RENEW:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_IP_RENEW");
        if(WasOption(argc, argv, L"static")>=0)
            g_pKato->Log(LOG_COMMENT, L"skip 'ipconfig /renew' when static IP");
        else
            IpAddressRelease0Renew1(g_szWiFiAdapter1, 1);  //RunCmd(L"ipconfig /renew");
        break;

    case ID_F1_COMMAND_IP_RELEASE:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_IP_RELEASE");
        if(WasOption(argc, argv, L"static")>=0)
            g_pKato->Log(LOG_COMMENT, L"skip 'ipconfig /release' when static IP");
        else
            IpAddressRelease0Renew1(g_szWiFiAdapter1, 0);  //RunCmd(L"ipconfig /release");
        break;

    case ID_F1_COMMAND_ROAM12:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_ROAM12");
        g_pKato->Log(LOG_COMMENT, L"skip, test not prepared yet");
        break;

    case ID_F1_COMMAND_ROAM21:
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_ROAM21");
        g_pKato->Log(LOG_COMMENT, L"skip, test not prepared yet");
        break;

    default:
        g_pKato->Log(LOG_COMMENT, L"unknown command 0x%08X", lpFTE->dwUserData);
    }

    Sleep(1000);    // give little bit time to finish 

    // now, test ip

    DWORD dwTestIp = lpFTE->dwUserData & ID_F1_TEST_IP_BITS;
    if((WasOption(argc, argv, L"static")>=0) &&
        ((ID_F1_COMMAND_IP_RENEW==dwCommandId) || (ID_F1_COMMAND_IP_RELEASE==dwCommandId))
        )
        goto TEST_UI;

    WCHAR *pszTestArg = NULL;
    if(dwTestIp)
    {
        g_pKato->Log(LOG_COMMENT, L"IP-ADDR check");
        if(GetOption(argc, argv, L"test", &pszTestArg)>=0)
            g_pKato->Log(LOG_COMMENT, L"-test option = %s", pszTestArg);

        if(wcsstr(pszTestSsid, L"-adhoc"))  // special situation on adhoc net
        {
            if(dwCommandId==ID_F1_COMMAND_CONNECT)
            {
                if(dwTestIp==ID_F1_CHECK_AND_WAIT_FOR_ZERO_IP)
                {
                    dwTestIp = ID_F1_CHECK_AND_WAIT_FOR_SOME_IP;    // since adhoc net always connects
                    g_pKato->Log(LOG_COMMENT, L"adhoc net always connects since it starts by itself if there is none.");
                }
            }
            if(dwCommandId==ID_F1_COMMAND_AP_PWR_OFF)
            {
                if(lpFTE>g_lpFTE)
                {
                    dwTestIp = (lpFTE-1)->dwUserData & ID_F1_TEST_IP_BITS;
                    g_pKato->Log(LOG_COMMENT, L"IP addr remains as it was in adhoc net when the first STA leaves");
                }
            }
        }
        else    // infra net
        {
            if( pszTestArg &&
                (_wcsnicmp(pszTestArg, L"ip:0.0.", wcslen(L"ip:0.0."))==0) &&
                (dwTestIp==ID_F1_CHECK_AND_WAIT_FOR_SOME_IP))
                dwTestIp = ID_F1_CHECK_AND_WAIT_FOR_ZERO_IP;
        }

        WCHAR *szTestIpAddr = L"100.100.x.x";
        HANDLE m_hAddrChange = NULL;

        switch(dwTestIp)
        {
        case ID_F1_CHECK_AND_WAIT_FOR_ZERO_IP:
            g_pKato->Log(LOG_COMMENT, L"ID_F1_CHECK_AND_WAIT_FOR_ZERO_IP");
            g_szWiFiAdapter1 = NULL;
            FindWirelessNetworkDevice();
            if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
            {
                g_pKato->Log(LOG_COMMENT, L"no wifi adapter found [so NULL IP]");
                break;
            }
            if(NotifyAddrChange(&m_hAddrChange,NULL) != NO_ERROR)
            {
                g_pKato->Log(LOG_FAIL, L"NotifyAddrChange() failure");
                break;
            }
            for(unsigned iRetry=0; iRetry<5; iRetry++)
            {
                WaitForSingleObject(m_hAddrChange, 20*1000);
                if(AdapterHasNoneZeroIpAddress(g_szWiFiAdapter1))
                    g_pKato->Log(LOG_COMMENT, L"expected to have IP 0.0.0.0, but has something else [FAIL] %d", iRetry);
                else
                    break;
            }
            if(AdapterHasNoneZeroIpAddress(L"XWIFI11B1"))
                g_pKato->Log(LOG_FAIL, L"XWIFI11B1 has non-zero IP address");
            else
            {
                g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 has 0.0.0.0 IP address as expected");
                g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
            }
            break;

        case ID_F1_CHECK_AND_WAIT_FOR_SOME_IP:
            g_pKato->Log(LOG_COMMENT, L"ID_F1_CHECK_AND_WAIT_FOR_SOME_IP");

            for(unsigned iRetry=0; iRetry<10; iRetry++)
            {
                g_szWiFiAdapter1 = NULL;
                FindWirelessNetworkDevice();
                if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
                {
                    g_pKato->Log(LOG_COMMENT, L"no wifi adapter found");
                    Sleep(2000);
                }
                else
                    break;
            }
            if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
            {
                g_pKato->Log(LOG_FAIL, L"checked several times, but no wifi adapter found");
                break;
            }

            if(_wcsnicmp(pszTestArg, L"ip:10.10.", wcslen(L"ip:10.10."))==0)
                szTestIpAddr = L"10.10.x.x";
            else if(_wcsnicmp(pszTestArg, L"ip:auto", wcslen(L"ip:auto"))==0)
                szTestIpAddr = L"169.254.x.x";
            if(NotifyAddrChange(&m_hAddrChange,NULL) != NO_ERROR)
            {
                g_pKato->Log(LOG_FAIL, L"NotifyAddrChange() failure");
                break;
            }
            for(iRetry=0; iRetry<5; iRetry++)
            {
                WaitForSingleObject(m_hAddrChange, 20*1000);
                if(!AdapterHasIpAddress(g_szWiFiAdapter1, szTestIpAddr))
                    g_pKato->Log(LOG_COMMENT, L"expected to have IP %s, but has something else or no IP address [FAIL] %d", szTestIpAddr, iRetry);
                else
                    break;
            }
            if(AdapterHasIpAddress(g_szWiFiAdapter1, szTestIpAddr))
            {
                g_pKato->Log(LOG_COMMENT, L"%s has IP address %s as expected", g_szWiFiAdapter1, szTestIpAddr);
                g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
            }
            else
            {
                // some case, adhoc net simply starts its own network and go to the auto ip.
                // this should be considered as ok.
                if(
                    wcsstr(pszTestSsid, L"-adhoc") &&
                    AdapterHasIpAddress(g_szWiFiAdapter1, L"169.254.x.x"))
                {
                    g_pKato->Log(LOG_COMMENT, L"%s is expected to have IP address %s but it has 169.254.x.x", g_szWiFiAdapter1, szTestIpAddr);
                    g_pKato->Log(LOG_COMMENT, L"special situation in adhoc net: this adhoc is supposed to connect to a DHCP-adhoc net, but some how it failed to connect [so started its own adhoc net]");
                    g_pKato->Log(LOG_COMMENT, L"so it has no DHCP server in this network, therefore it gets an autoip 169.254.x.x");
                    g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
                }
                else
                    g_pKato->Log(LOG_FAIL, L"XWIFI11B1 has 0.0.0.0 IP address");
            }
            break;

        default:
            g_pKato->Log(LOG_COMMENT, L"unknown IP test id 0x%08X", lpFTE->dwUserData);
        }
        if(m_hAddrChange)
        {
            CloseHandle(m_hAddrChange);
            m_hAddrChange = NULL;
        }
    }
    else
        g_pKato->Log(LOG_COMMENT, L"no IP check requested");

    // test UI
TEST_UI:
    DWORD dwTestUi = lpFTE->dwUserData & ID_F1_TEST_UI_BITS;
    if(dwTestUi)
    {
        g_pKato->Log(LOG_COMMENT, L"UI check");

        // should clean if roaming becomes possible
        switch(dwCommandId)
        {
        case ID_F1_COMMAND_ROAM12:
        case ID_F1_COMMAND_ROAM21:
            g_pKato->Log(LOG_COMMENT, L"skipping, test not prepared yet");
            goto SKIP_TESTING_UI;
        }

        if(wcsstr(pszTestSsid, L"-adhoc"))
        {
            switch(dwCommandId)
            {
            case ID_F1_COMMAND_AP_PWR_ON:
            case ID_F1_COMMAND_AP_PWR_OFF:
            case ID_F1_COMMAND_ROAM12:
            case ID_F1_COMMAND_ROAM21:
                g_pKato->Log(LOG_COMMENT, L"skipping test in adhoc");
                goto SKIP_TESTING_UI;
                break;
            }
        }
        else    // special case in infra net
        {
            // if wifi is not connected to any, and if AP goes OFF in this case, it is still not-connected state, --> no change
            g_pKato->Log(LOG_COMMENT, L"special case in infra net");
            switch(dwCommandId)
            {
            case ID_F1_COMMAND_AP_PWR_ON:
            case ID_F1_COMMAND_AP_PWR_OFF:
                if( pszTestArg &&
                    (_wcsnicmp(pszTestArg, L"ip:0.0.", wcslen(L"ip:0.0."))==0) )
                {
                    g_pKato->Log(LOG_COMMENT, L"this case [expected ip:0.0.0.0] is expected to fail to connect no matter what AP condition is.");
                    g_pKato->Log(LOG_COMMENT, L"UI will not be changed since network will always remain as not-connected state.");
                    goto SKIP_TESTING_UI;
                }
                break;
            }
        }

        switch(dwTestUi)
        {
        case ID_F1_CHECK_AND_WAIT_FOR_UI_ON:
            g_pKato->Log(LOG_COMMENT, L"ID_F1_CHECK_AND_WAIT_FOR_UI_ON");
            for(unsigned iRetry=0; iRetry<10 && !IsDialogWindowPoppedUp(L"XWIFI11B1"); iRetry++)
                Sleep(1000);
            if(IsDialogWindowPoppedUp(L"XWIFI11B1"))
            {
                g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 UI is visible as expected");
                g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
            }
            else
                g_pKato->Log(LOG_FAIL, L"XWIFI11B1 UI is NOT visible [fail]");
            break;

        case ID_F1_CHECK_AND_WAIT_FOR_UI_OFF:
            g_pKato->Log(LOG_COMMENT, L"ID_F1_CHECK_AND_WAIT_FOR_UI_OFF");
            for(unsigned iRetry=0; iRetry<10 && IsDialogWindowPoppedUp(L"XWIFI11B1"); iRetry++)
                Sleep(1000);
            if(!IsDialogWindowPoppedUp(L"XWIFI11B1"))
            {
                g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 UI is NOT visible as expected");
                g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
            }
            else
                g_pKato->Log(LOG_FAIL, L"XWIFI11B1 UI is visible [fail]");
            break;

        default:
            g_pKato->Log(LOG_COMMENT, L"unknown UI test id 0x%08X", lpFTE->dwUserData);
        }
    }
    else
        g_pKato->Log(LOG_COMMENT, L"no UI check requested");

SKIP_TESTING_UI:

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}





#define ID_F2_COMMAND_BITS             0xF0000000
#define ID_F2_COMMAND_NOTHING          0x00000000
#define ID_F2_COMMAND_BROADCAST_UDP    0x10000000

#define ID_F2_COMMAND_NO_OF_PACKETS    0x000F0000
#define ID_F2_COMMAND_PACKET_SIZE_MASK 0x0000FFFF

#define UDP_PORT_ECHO 7




TESTPROCAPI
F2
// functional test
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    g_pKato->Log(LOG_COMMENT, L"Functional Test 2 : packet send");

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
        g_pKato->Log(LOG_COMMENT, L"no wifi adapter found");

    DWORD dwT_Command = GetTickCount();
    DWORD dwCommandId = lpFTE->dwUserData & ID_F2_COMMAND_BITS;
    DWORD dwNoOfPackets = (lpFTE->dwUserData & ID_F2_COMMAND_NO_OF_PACKETS) >> 16;
    DWORD dwPacketSize = lpFTE->dwUserData & ID_F2_COMMAND_PACKET_SIZE_MASK;
    char *ucbData = (char*)LocalAlloc(LPTR, dwPacketSize);
    g_pKato->Log(LOG_COMMENT, L"send %d packets of %d bytes", dwNoOfPackets, dwPacketSize);

    switch(dwCommandId)
    {
    case ID_F2_COMMAND_BROADCAST_UDP:
        {
            SOCKADDR_IN	addr1;
            memset((char *)&addr1, 0, sizeof(addr1));
            addr1.sin_family = AF_INET;
            addr1.sin_port = htons(UDP_PORT_ECHO);
            addr1.sin_addr.S_un.S_addr = INADDR_BROADCAST;

            SOCKET Sock1 = socket(AF_INET, SOCK_DGRAM, 0);
            if(INVALID_SOCKET == Sock1)
            {
                g_pKato->Log(LOG_FAIL, L"socket() error %d", WSAGetLastError());
                break;
            }

            BOOL bEnableBroadcast = TRUE;
            setsockopt(Sock1, SOL_SOCKET, SO_BROADCAST,
                (const char*)&bEnableBroadcast, sizeof(bEnableBroadcast));

            //int Status = bind(Sock, (SOCKADDR *)&SockAddr_Local, sizeof(SockAddr_Local));
            // I don't need to receive any, so don't bind
            for(unsigned i=0; i<dwNoOfPackets; i++)
            {
                g_pKato->Log(LOG_COMMENT, L"send 1 packet %d bytes", dwPacketSize);
                if(SOCKET_ERROR == sendto(Sock1, ucbData, dwPacketSize, 0, (sockaddr*)&addr1, sizeof(addr1)))
                    g_pKato->Log(LOG_FAIL, L"UDP sending error %d", WSAGetLastError());
                Sleep(1000);
            }
            closesocket(Sock1);
            shutdown(Sock1, 0);
        }
        break;

    default:
        g_pKato->Log(LOG_COMMENT, L"unknown UI test id 0x%08X", lpFTE->dwUserData);
    }
    g_pKato->Log(LOG_COMMENT, L"delta time = %d ms", GetTickCount() - dwT_Command);
    LocalFree(ucbData);

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}



TESTPROCAPI
F3
// stability test
// skip test if -stability option is not given
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    BOOL bStabilityOptionGiven = WasOption(argc, argv, L"stability")>0;

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1 || !*g_szWiFiAdapter1)
        g_pKato->Log(LOG_COMMENT, L"no wifi adapter found");

    if(!(lpFTE->dwUserData & 0x00001000))
    {
        g_pKato->Log(LOG_COMMENT, L"skip stability test");
        return TPR_PASS;
    }

    g_pKato->Log(LOG_COMMENT, L"stability test");
    // do something
    // wait [] ms
    // undo
    // wait for finishing undo

    // do something
    DWORD dwCommand = (lpFTE->dwUserData>>8) & 0x000F;

    switch(dwCommand)
    {
    case 0: // do nothing
        break;

    case 1: // card-insert->eject test
        g_pKato->Log(LOG_COMMENT, L"card insert");
        if(bStabilityOptionGiven)
        {
            ;   // card insert action here
        }
        break;

    case 2: // wireless-connect->disconnect test
        g_pKato->Log(LOG_COMMENT, L"wireless-connect");
        if(bStabilityOptionGiven)
        {
            ;   // connect action here
        }
        break;

    default: // for future expansion
        g_pKato->Log(LOG_COMMENT, L"unknown command");
    }

    // wait [] ms
    DWORD dwSleep = lpFTE->dwUserData & 0x000F;
    DWORD dwWaitTable[16] = { 0, 1, 10, 50, 100, 500, 1000, 2000, 5000, 30000 };
    if(dwWaitTable[dwSleep])
    {
        g_pKato->Log(LOG_COMMENT, L"sleep time = %d ms", dwWaitTable[dwSleep]);
        if(bStabilityOptionGiven)
            Sleep(dwWaitTable[dwSleep]);
    }

    // undo
    switch(dwCommand)
    {
    case 0: // do nothing
        break;

    case 1: // card-insert->eject test
        g_pKato->Log(LOG_COMMENT, L"eject the card");
        if(bStabilityOptionGiven)
        {
            ;   // card eject action here
        }
        break;

    case 2: // wireless-connect->disconnect test
        g_pKato->Log(LOG_COMMENT, L"disconnect");
        if(bStabilityOptionGiven)
        {
            ;    // wireless disconnect action here
        }
        break;

    default: // for future expansion
        g_pKato->Log(LOG_COMMENT, L"unknown command");
    }

    if(bStabilityOptionGiven)
    {
        // wait for finishing undo
        for(unsigned iRetry=0; (iRetry<30) && AdapterHasNoneZeroIpAddress(g_szWiFiAdapter1); iRetry++)
        {
            g_pKato->Log(LOG_COMMENT, L"waiting for finishing undo task");
            Sleep(2000);
        }
        if(AdapterHasNoneZeroIpAddress(g_szWiFiAdapter1))
            g_pKato->Log(LOG_FAIL, L"adapter %s still has an IP address", g_szWiFiAdapter1);
    }

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}





TESTPROCAPI
F_Stress
// functional test
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(WasOption(argc, argv, L"stress")<0)
        return TPR_PASS;

    g_pKato->Log(LOG_COMMENT, L"Stress Test 1 : card insert/eject");

    FindWirelessNetworkDevice();
    if(g_szWiFiAdapter1 && *g_szWiFiAdapter1)
    {
        if(_wcsicmp(g_szWiFiAdapter1, L"XWIFI11B1")!=0)
        {
            g_pKato->Log(LOG_FAIL, L"non XWIFI driver found");
            return TPR_FAIL;
        }
        g_pKato->Log(LOG_COMMENT, L"XWIFI adapter found");
        g_pKato->Log(LOG_COMMENT, L"ID_F1_COMMAND_EJECT");
        _CardEjectNdisMiniportAdapter(L"XWIFI11B1");
        Sleep(5000);
    }

    for(unsigned idt=0; idt<3*60*1000; idt++)
    {
        g_pKato->Log(LOG_COMMENT, L"XWIFI insert/eject test dt = %d ms", idt);
        _CardInsertNdisMiniportAdapter(L"XWIFI11B");
        Sleep(idt);
        _CardEjectNdisMiniportAdapter(L"XWIFI11B1");
        Sleep(5000);
        g_pKato->Log(LOG_COMMENT, L"XWIFI insert/eject test done");
    }

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}
