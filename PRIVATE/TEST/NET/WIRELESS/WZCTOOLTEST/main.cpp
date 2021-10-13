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


#if 0
wzctool usage:
options:
-e              Enumerate wireless cards.
-q <Card Name>  Query wireless card.
-c <Card Name> -ssid AP-SSID -auth open -encr wep -key 1/0x1234567890
  connect to AP-SSID with given parameters.     Use -c -? for detail.
-reset          Reset WZC configuration data. Wireless card will disconnect if it was connected.
-s <Card Name>  Set WZC variables.
  Use -s -? for detail.
-refresh        Refresh entries.
-registry       configure as registry.
  Use -registry -? for detail.
-?  shows help message
#endif



#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

#include <windows.h>
#include <winsock.h>
#include <winreg.h>
#include <tux.h>
#include <kato.h>
#include <KATOEX.H>
#include <auto_xxx.hxx>
#include <QADialog_t.hpp>
using namespace CENetworkQA;


#define	MAXLOGLENGTH		1024

SPS_SHELL_INFO *g_pShellInfo;
CKato          *g_pKato     = NULL;
CKato          *g_pKatoBare = NULL;
HINSTANCE       g_hInstDll  = NULL;
HINSTANCE       g_hInstExe  = NULL;
HINSTANCE       g_hInst     = NULL;

QADialog_t     *g_pDialog   = NULL;
static TCHAR    s_pOkCancelPrompt[128];

extern FUNCTION_TABLE_ENTRY g_lpFTE[];
void ParseCmdLine(IN const WCHAR *szCmdLine);

WCHAR g_szWiFiCard1[64] = L"";


BOOL
WINAPI
DllMain
(
HANDLE hInst,
DWORD dwReason,
LPVOID lpVoid
)
{
    g_hInstDll = (HINSTANCE)hInst;
    return TRUE;
}


SHELLPROCAPI
ShellProc
(
UINT	uMsg,
SPPARAM spParam
)
{
    switch (uMsg)
    {
    case SPM_REGISTER:
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
        #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
        #else
            return SPR_HANDLED;
        #endif
        break;

    case SPM_LOAD_DLL:
        g_pKato = (CKato *)KatoGetDefaultObject();
        g_pKatoBare	= (CKato *)KatoCreate(TEXT("KatoBare"));
        ((LPSPS_LOAD_DLL)spParam)->fUnicode	= TRUE;
        g_pDialog = new QADialog_t(g_pKato, TEXT("wzctooltest"), true);
        QADialog_t::FormatOkCancelPrompt(TEXT("wzctooltest.dll"), 1, 
                                         s_pOkCancelPrompt,
                                  sizeof(s_pOkCancelPrompt) / sizeof(TCHAR));
        break;

    case SPM_UNLOAD_DLL:
        if (g_pDialog != NULL) delete g_pDialog;
        break;

    case SPM_START_SCRIPT:
    case SPM_STOP_SCRIPT:
        break;

    case SPM_SHELL_INFO:
        g_pShellInfo= (LPSPS_SHELL_INFO)spParam;
        ParseCmdLine(g_pShellInfo->szDllCmdLine);
        g_hInst     = g_pShellInfo->hLib;
        g_hInstExe  = g_pShellInfo->hInstance;
        g_pShellInfo->fXML ? g_pKato->SetXML(true) : g_pKato->SetXML(false);
        srand((unsigned)GetTickCount());
        break;

    case SPM_BEGIN_GROUP:
    case SPM_END_GROUP:
        break;

    case SPM_BEGIN_TEST:
        g_pKato->BeginLevel(0,TEXT("SPM_BEGIN_TEST"));
        break;

    case SPM_END_TEST:
        g_pKato->EndLevel(TEXT("SPM_END_TEST"));
        break;

    case SPM_EXCEPTION:
        break;

    default:
        return SPR_NOT_HANDLED;

    }
    return SPR_HANDLED;
}




TESTPROCAPI
GetCode()
{
    if(g_pKato->GetVerbosityCount(LOG_FAIL))
        return TPR_FAIL;
    return TPR_PASS;
}





#define GETTICKCOUNT	dwTick	= GetTickCount();

BOOL g_TryXWIFI11B = FALSE;
BOOL g_ShowCmdOut = TRUE;   // FALSE

void
ParseCmdLine
(
IN const WCHAR *szCmdLine   // cmd line formats
)
{
    g_pKato->Log(LOG_COMMENT, L"cmd line: %s", szCmdLine);
    if(wcsstr(szCmdLine, L"xwifi11b") || wcsstr(szCmdLine, L"XWIFI11B"))
        {
        g_TryXWIFI11B = TRUE;
        g_pKato->Log(LOG_COMMENT, L"will try XWIFI11B card");
        }

    if(wcsstr(szCmdLine, L"-show cmdout"))
        {
        g_ShowCmdOut = TRUE;
        }

    // -i turns off interactive mode.
    if (wcsstr(szCmdLine, L"-i"))
        {
        g_pDialog->SetInteractive(false);
        }

    // Check for the "-ok-button" or "-cancel-button" command and set the
    // corresponding event. 
    g_pDialog->SetButtonEventFromCommandLine(szCmdLine);
}



BOOL
TestDescriptionIs
(
IN const WCHAR *szDescription,
IN const WCHAR *szText
)
{
    if(!szDescription || !szText)
        return FALSE;

    return (wcscmp(szDescription,szText)==0);
}


// we are going to keep only first 64k-chars of output.
#define COMMAND_BUFFER_CHARS 0x10000
WCHAR g_szCommandOutput[COMMAND_BUFFER_CHARS];


BOOL
RunCommand
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
    g_pKato->Log(LOG_COMMENT, L"command %s\n", szCommand);

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
                if((wcslen(g_szCommandOutput)+wcslen(szText)) >= COMMAND_BUFFER_CHARS)
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
    if (g_ShowCmdOut)
    {
        g_pKato->Log(LOG_COMMENT, L"g_szCommandOutput[] =");
        g_szCommandOutput[COMMAND_BUFFER_CHARS - 1] = L'\0';
        g_pDialog->Log(LOG_COMMENT, L"%s", g_szCommandOutput);
    }
}



BOOL
FileExist
// return TRUE if szFileName exists
(
const IN WCHAR *szFileName
)
{
    FILE *fp = _wfopen(szFileName, L"rb");
    fclose(fp);
    return (fp!=NULL);
}





#include <eapol.h>
#include <wzcsapi.h>
#include <ntddndis.h>

#define MAX_TRIAL 150   // 2-min-30-sec



BOOL
WirelessNetworkCardDetected
// enumerate wireless network cards detected by WZC
// return TRUE if WZC API returns szAdapter as a WIFI adapter
(
const WCHAR *szAdapter
)
{
    INTFS_KEY_TABLE IntfsTable;
    IntfsTable.dwNumIntfs = 0;
    IntfsTable.pIntfs = NULL;

    DWORD dwStatus = WZCEnumInterfaces(NULL, &IntfsTable);

    if(dwStatus != ERROR_SUCCESS)
        FALSE;        

    BOOL bWiFiDetected = FALSE;
    for(unsigned int i=0; (i<IntfsTable.dwNumIntfs) && !bWiFiDetected; i++)
    {
        if(wcscmp(IntfsTable.pIntfs[i].wszGuid, szAdapter)==0)
            bWiFiDetected = TRUE;
    }

    LocalFree(IntfsTable.pIntfs); // need to free memory allocated by WZC for us.
    return bWiFiDetected;
}   // WirelessNetworkCardDetected




BOOL
WaitForWiFiCardRemoval
// return TRUE if the WiFi adapter goes away within 300 secs (5 mins)
(
const WCHAR *szAdapter
)
{
    for(int i=0; i<MAX_TRIAL && WirelessNetworkCardDetected(szAdapter); i++)
        Sleep(1000);
    return (i < MAX_TRIAL); // time out
}


BOOL
WaitForWiFiCardDetection
// return TRUE if the WiFi adapter is detected within 300 secs
(
const WCHAR *szAdapter
)
{
    for(int i=0; i<MAX_TRIAL && !WirelessNetworkCardDetected(szAdapter); i++)
        Sleep(1000);
    return (i < MAX_TRIAL);
}




WCHAR *
GetWiFiCardName
// returns WiFi card name like L"XWIFI11B1"
// the WiFii card name is stored in g_szWiFiCard1 too.
(
)
{
    g_szWiFiCard1[0] = L'\0';
    INTFS_KEY_TABLE IntfsTable;
    IntfsTable.dwNumIntfs = 0;
    IntfsTable.pIntfs = NULL;

    DWORD dwStatus = WZCEnumInterfaces(NULL, &IntfsTable);

    if(dwStatus != ERROR_SUCCESS)
        FALSE;        

    if(IntfsTable.dwNumIntfs)
        wcsncpy(g_szWiFiCard1, IntfsTable.pIntfs[0].wszGuid, 32);
    LocalFree(IntfsTable.pIntfs); // need to free memory allocated by WZC for us.
    return g_szWiFiCard1;
}   // GetWiFiCardName





void
GetAvailableSsidList
// Query the WiFi card and return copy of the Intf.rdBSSIDList data structure.
// caller should free the returned memory object after done.
(
IN LPWSTR szWiFiCard,
OUT PRAW_DATA prdBSSIDList
)
{
    INTF_ENTRY Intf;
    DWORD dwOutFlags;
    memset(&Intf, 0x00, sizeof(INTF_ENTRY));
    Intf.wszGuid = szWiFiCard;
    
    DWORD dwStatus = WZCQueryInterface(
                        NULL,
                        INTF_ALL,
                        &Intf,
                        &dwOutFlags);

    if (dwStatus != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL, L"WZCQueryInterface() error 0x%08X\n", dwStatus);
        memset(prdBSSIDList, 0, sizeof(RAW_DATA));
        return;
    }

    if(!Intf.rdBSSIDList.dwDataLen)
    {
        memset(prdBSSIDList, 0, sizeof(RAW_DATA));
        return;
    }

    prdBSSIDList->dwDataLen = Intf.rdBSSIDList.dwDataLen;
    prdBSSIDList->pData = (LPBYTE)LocalAlloc(LPTR, prdBSSIDList->dwDataLen);
    memcpy(prdBSSIDList->pData, Intf.rdBSSIDList.pData, prdBSSIDList->dwDataLen);
    WZCDeleteIntfObj(&Intf);
    return;
}   // GetAvailableSsidList()




BOOL
SsidIsAvailable
// return TRUE if aSSID is visible [available]
(
IN LPWSTR szWiFiCard,
IN LPWSTR aSSID
)
{
    if(!aSSID)
        return FALSE;

    RAW_DATA rdBSSIDList;
    memset(&rdBSSIDList, 0, sizeof(RAW_DATA));
    GetAvailableSsidList(szWiFiCard, &rdBSSIDList);
    if (rdBSSIDList.dwDataLen == 0)
        return FALSE;
    BOOL bFound = FALSE;
    unsigned iSsidLen = wcslen(aSSID);
    char csz_aSSID[32];
    for(UINT i=0; i<iSsidLen; i++)
        csz_aSSID[i] = (char)aSSID[i];
    PWZC_802_11_CONFIG_LIST pConfigList = (PWZC_802_11_CONFIG_LIST)rdBSSIDList.pData;
    for (i = 0; (i<pConfigList->NumberOfItems) && !bFound; i++)
    {
        PWZC_WLAN_CONFIG pConfig = &(pConfigList->Config[i]);
        bFound = (iSsidLen == pConfig->Ssid.SsidLength) &&
                    (memcmp(csz_aSSID, pConfig->Ssid.Ssid, iSsidLen)==0);
    }
    LocalFree(rdBSSIDList.pData);
    return bFound;
}




WCHAR* g_szAuthenticationMode[] =
{
    L"Ndis802_11AuthModeOpen",
    L"Ndis802_11AuthModeShared",
    L"Ndis802_11AuthModeAutoSwitch",
};



void 
PrintSSID
// some RAW_DATA is a SSID, this function is for printing SSID
(
    PRAW_DATA prdSSID   // RAW SSID data
)
{
    if (prdSSID == NULL || prdSSID->dwDataLen == 0)
        wprintf(L"<NULL>");
    else
    {
        WCHAR szSsid[33];
        for (UINT i = 0; i < prdSSID->dwDataLen; i++)
            szSsid[i] = prdSSID->pData[i];
        szSsid[i] = L'\0';
        wprintf(L"%s", szSsid);
    }

}	//	PrintSSID()




WCHAR g_szSupportedRate[32];// = { L"1", L"2", L"5.5", L"11", L"" };   // Mbit/s

WCHAR*
SupportedRate
// rate values in WCHAR string
(
    IN BYTE ucbRawValue
)
{
    double fRate = ((double)(ucbRawValue & 0x7F)) * 0.5;
    swprintf(g_szSupportedRate, L"%.1f", fRate);
    return g_szSupportedRate;
}   // SupportedRate()



UINT
ChannelNumber
//
// calculate 802.11b channel number for given frequency
// return 1-14 based on the given ulFrequency_kHz
// return 0 for invalid frequency range
//
// 2412 MHz = ch-1
// 2417 MHz = ch-2
// 2422 MHz = ch-3
// 2427 MHz = ch-4
// 2432 MHz = ch-5
// 2437 MHz = ch-6
// 2442 MHz = ch-7
// 2447 MHz = ch-8
// 2452 MHz = ch-9
// 2457 MHz = ch-10
// 2462 MHz = ch-11
// 2467 MHz = ch-12
// 2472 MHz = ch-13
// 2484 MHz = ch-14
//
(
    IN ULONG ulFrequency_kHz // frequency in kHz
)
{
    ULONG ulFrequency_MHz = ulFrequency_kHz/1000;
    if((2412<=ulFrequency_MHz) && (ulFrequency_MHz<2484))
        return ((ulFrequency_MHz-2412)/5)+1;
    else if(ulFrequency_MHz==2484)
        return 14;
    return 0;   // invalid channel number
}   // ChannelNumber()


void
PrintConfigList
// print WZC configuration list
// used when printing [Available Networks] and [Preferred Networks]
(
    PRAW_DATA prdBSSIDList
)
{
    if (prdBSSIDList == NULL || prdBSSIDList->dwDataLen == 0)
    {
        wprintf(L"<NULL> entry.");
    }
    else
    {
        PWZC_802_11_CONFIG_LIST pConfigList = (PWZC_802_11_CONFIG_LIST)prdBSSIDList->pData;

        wprintf(L"[%d] entries.\n\n", pConfigList->NumberOfItems);

        for (UINT i = 0; i < pConfigList->NumberOfItems; i++)
        {
            PWZC_WLAN_CONFIG pConfig = &(pConfigList->Config[i]);

            wprintf(L"******** List Entry Number [%d] ********\n", i);
            wprintf(L"   Length                  = %d bytes.\n", pConfig->Length);
            wprintf(L"   dwCtlFlags              = 0x%08X\n", pConfig->dwCtlFlags);
            wprintf(L"   MacAddress              = %02X:%02X:%02X:%02X:%02X:%02X\n",
                            pConfig->MacAddress[0],
                            pConfig->MacAddress[1],
                            pConfig->MacAddress[2],
                            pConfig->MacAddress[3],
                            pConfig->MacAddress[4],
                            pConfig->MacAddress[5]);
            wprintf(L"   SSID                    = ");
            RAW_DATA rdBuffer;
            rdBuffer.dwDataLen = pConfig->Ssid.SsidLength;
            rdBuffer.pData     = pConfig->Ssid.Ssid;
            PrintSSID(&rdBuffer);
            wprintf(L"\n");

            wprintf(L"   Privacy                 = %d  ", pConfig->Privacy);
            if(pConfig->Privacy == 1)
                wprintf(L"Privacy disabled (wireless data is not encrypted)\n");
            else
                wprintf(L"Privacy enabled (wireless data is encrypted)\n");

            wprintf(L"   RSSI                    = %d dBm (0=excellent, -100=weak signal)\n", pConfig->Rssi);

            wprintf(L"   NetworkTypeInUse        = %s\n",
                (pConfig->NetworkTypeInUse == Ndis802_11FH) ? L"NDIS802_11FH" :
                (pConfig->NetworkTypeInUse == Ndis802_11DS) ?
                L"NDIS802_11DS" : L"<UNKNOWN! SHOULD NOT BE!!>");

            ////////////////////////////////////////////////////////////////////
            wprintf(L"   Configuration:\n");
            wprintf(L"      Struct Length    = %d\n", pConfig->Configuration.Length);
            wprintf(L"      BeaconPeriod     = %d kusec\n", pConfig->Configuration.BeaconPeriod);
            wprintf(L"      ATIMWindow       = %d kusec\n", pConfig->Configuration.ATIMWindow);
            wprintf(L"      DSConfig         = %d kHz (ch-%d)\n", pConfig->Configuration.DSConfig, ChannelNumber(pConfig->Configuration.DSConfig));
            wprintf(L"      FHConfig:\n");
            wprintf(L"         Struct Length = %d\n" ,pConfig->Configuration.FHConfig.Length);
            wprintf(L"         HopPattern    = %d\n", pConfig->Configuration.FHConfig.HopPattern);
            wprintf(L"         HopSet        = %d\n", pConfig->Configuration.FHConfig.HopSet);
            wprintf(L"         DwellTime     = %d\n", pConfig->Configuration.FHConfig.DwellTime);

            wprintf(L"   Infrastructure          = %s\n",
                        (pConfig->InfrastructureMode == Ndis802_11IBSS) ? L"NDIS802_11IBSS" :
                        (pConfig->InfrastructureMode == Ndis802_11Infrastructure) ? L"Ndis802_11Infrastructure" :
                        (pConfig->InfrastructureMode == Ndis802_11AutoUnknown) ? L"Ndis802_11AutoUnknown" :
                        L"<UNKNOWN! SHOULD NOT BE!!>");

            wprintf(L"   SupportedRates          = ");
            for(int j=0; j<8; j++)
            {
                if(pConfig->SupportedRates[j])
                    wprintf(L"%s,", SupportedRate(pConfig->SupportedRates[j]));
            }
            wprintf(L" (Mbit/s)\n");

            wprintf(L"   KeyIndex                = <not available> (beaconing packets don't have this info)\n"); // pConfig->KeyIndex
            wprintf(L"   KeyLength               = <not available> (beaconing packets don't have this info)\n"); // pConfig->KeyLength
            wprintf(L"   KeyMaterial             = <not available> (beaconing packets don't have this info)\n");

            ////////////////////////////////////////////////////////////////////

            wprintf(L"   Authentication          = %d  ", pConfig->AuthenticationMode);
            if(pConfig->AuthenticationMode < Ndis802_11AuthModeMax)
                wprintf(L"%s\n", g_szAuthenticationMode[pConfig->AuthenticationMode]);
            else
                wprintf(L"<unknown>\n");

            ////////////////////////////////////////////////////////////////////

            wprintf(L"   rdUserData length       = %d bytes.\n\n", pConfig->rdUserData.dwDataLen);
        }
    }
}   // PrintConfigList()



BOOL
CardInsert_XWIFI11B
//
// install XWIFI11B (test wifi card)
// XWIFI11B adapter is for testing purpose.
// this adapter doesn't need any hardware.
// simply following files are needed.
// XWIFI11B1 adapter comes when successfully installed.
// returns FALSE if installation fails.
//
()  // no arg
{
    if( !FileExist(L"\\release\\ntapconfig.exe") ||
        !FileExist(L"\\release\\xwifi11b.dll"))
    {
        g_pKato->Log(LOG_COMMENT, L"cannot find required files");
        return FALSE;
    }

    if(!RunCommand(L"ntapconfig -card-insert xwifi11b"))
    {
        g_pKato->Log(LOG_COMMENT, L"cannot run 'ntapconfig'");
        return FALSE;
    }

    if(WaitForWiFiCardDetection(L"XWIFI11B1"))
        g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 is inserted");
    else
        g_pKato->Log(LOG_COMMENT, L"WARNING XWIFI11B1 insertion failure");
    Sleep(1000);
    return TRUE;
}



BOOL
CardEject_XWIFI11B
//
// ejecting XWIFI11B1
// returns FALSE if card-ejection fails
//
()  // no arg
{
    if(!RunCommand(L"ntapconfig -card-eject xwifi11b1"))
    {
        g_pKato->Log(LOG_COMMENT, L"cannot run 'ntapconfig'");
        return FALSE;
    }

    if(WaitForWiFiCardRemoval(L"XWIFI11B1"))
        g_pKato->Log(LOG_COMMENT, L"XWIFI11B1 is ejected");
    else
        g_pKato->Log(LOG_COMMENT, L"WARNING XWIFI11B1 ejection failure");
    Sleep(1000);

    if(RegDeleteKey(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B1\\Parms\\TCPIP") != ERROR_SUCCESS)
        g_pKato->Log(LOG_COMMENT, L"failed to delete key [HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B1\\Parms\\TCPIP]");

    return TRUE;
}





WCHAR *g_szTestSsidSet1[] =
{
    L";",
    L"; Copyright (c) 2003 Microsoft Corporation.  All rights reserved.",
    L";",
    L";",
    L"; Use of this source code is subject to the terms of the Microsoft end-user",
    L"; license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.",
    L"; If you did not accept the terms of the EULA, you are not authorized to use",
    L"; this source code. For a copy of the EULA, please see the LICENSE.RTF on your",
    L"; install media.",
    L";",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-OPEN]",
	L"    \"MacAddress\"=\"00:40:00:00:00:00\"",
	L"    \"Privacy\"=dword:0",
	L"    \"Rssi\"=dword:79",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"DHCP\"=\"192.168.0.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-WEP40]",
	L"    \"MacAddress\"=\"00:40:00:00:00:11\"",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"AuthMode\"=dword:0",
	L"    \"Privacy\"=dword:1",
	L"    \"key-index\"=dword:1",
	L"    \"key\"=\"0x1234567890\"",
	L"    \"DHCP\"=\"192.168.0.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-WEP104]",
	L"    \"MacAddress\"=\"00:40:00:00:00:22\"",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"AuthMode\"=dword:0",
	L"    \"Privacy\"=dword:1",
	L"    \"key-index\"=dword:1",
	L"    \"key\"=\"qwertyuiopasd\"",
	L"    \"DHCP\"=\"192.168.0.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\MSFTWLAN]",
	L"    \"MacAddress\"=\"00:40:00:00:00:33\"",
	L"    \"Privacy\"=dword:1",
	L"    \"Rssi\"=dword:100",
	L"    \"InfrastructureMode\"=dword:1",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-ADHOC1]",
	L"    \"MacAddress\"=\"10:33:66:9D:62:F0\"",
	L"    \"Privacy\"=dword:0",
	L"    \"Rssi\"=dword:100",
	L"    \"InfrastructureMode\"=dword:0",
    NULL
};



void
SaveToRegistry
(
IN WCHAR** szRegData
)
{
    FILE *fp = fopen("tmp1.reg", "wt");
    if(!fp)
    {
        g_pKato->Log(LOG_FAIL, L"file creation error (%s)", "TestSsidSet1.reg");
    	return;
    }
    for(WCHAR **p=szRegData; *p; p++)
        fwprintf(fp, L"%s\n", *p);
    fclose(fp);

    WCHAR szCmdLine1[128];
    wsprintf(szCmdLine1, L"ntapconfig -wifi add tmp1.reg");
    RunCommand(szCmdLine1);
    DumpCommandOutputToTuxEcho();
    //RunCommand(L"ntapconfig -wifi refresh");  // refresh is already done by 'ntapconfig -wifi add' above line
}



void
AddTestSsidSet_XWIFI11B
// XWIFI11B gets available SSIDs from the registry
// HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST
(
)
{
    SaveToRegistry(g_szTestSsidSet1);

    // wait for SSID update
    for(int i=0; i<MAX_TRIAL; i++)
    {
        g_pKato->Log(LOG_COMMENT, L"checking SSID list");
        RunCommand(L"wzctool -q");

        WCHAR* sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
#if 0
        if(
            wcsstr(sz1, L"CE-OPEN") &&
            wcsstr(sz1, L"CE-WEP40") &&
            wcsstr(sz1, L"CE-WEP104") &&
            wcsstr(sz1, L"MSFTWLAN") &&
            wcsstr(sz1, L"CE-ADHOC1")
            )
#endif
        if(sz1 && wcsstr(sz1, L"CE-OPEN"))
            break;

        Sleep(1000);
    }
    if(i < MAX_TRIAL)
        g_pKato->Log(LOG_COMMENT, L"test SSID set ready");
    else
        g_pKato->Log(LOG_COMMENT, L"WARNING SSIDs add failed");
}



void
ClearTestSsidSet_XWIFI11B
(
)
{
    if(RegDeleteKey(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B-BSSID-LIST") == ERROR_SUCCESS)
        g_pKato->Log(LOG_COMMENT, L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST] deleted");
    else
        g_pKato->Log(LOG_COMMENT, L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST] deletion failed (this key might not exist)");
    RunCommand(L"ntapconfig -wifi refresh");

    DumpCommandOutputToTuxEcho();

    // wait for SSID update
    for(int i=0; i<MAX_TRIAL; i++)
    {
        RunCommand(L"wzctool -q");

        WCHAR* sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(sz1 && !wcsstr(sz1, L"CE-OPEN"))
            break;
        Sleep(1000);
    }
    if(i < MAX_TRIAL)
        g_pKato->Log(LOG_COMMENT, L"SSID list is deleted");
    else
        g_pKato->Log(LOG_COMMENT, L"WARNING SSIDs list deletion failed");
}





#include <Iphlpapi.h>


// use statically allocated memory block
// to reduce risk of mem leaking from this stress code.

#define MEM_BLOCK_8K_SIZE (1024*8)
UCHAR g_ucBulkMem[MEM_BLOCK_8K_SIZE];




BOOL
AdapterHasIpAddress
//
// return TRUE if adapter is bound to TCP/IP and has a valid IP address
//
(
WCHAR *szAdapter	// adapter name 'NE20001', 'CISCO1'
)
{
    ULONG ulSizeAdapterInfo = MEM_BLOCK_8K_SIZE;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)g_ucBulkMem;
    DWORD dwStatus = GetAdaptersInfo(pAdapterInfo, &ulSizeAdapterInfo);
    if (dwStatus != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_COMMENT, L"no adapter\n");
        return FALSE;
    }

    char szAdapter1[64];
    wcstombs(szAdapter1, szAdapter, 64);
    while(pAdapterInfo != NULL)
    {
        if(strcmp(szAdapter1, pAdapterInfo->AdapterName) == 0)
        {
            PIP_ADDR_STRING pAddressList = &(pAdapterInfo->IpAddressList);
            while (pAddressList != NULL)
            {
                g_pKato->Log(LOG_COMMENT, L"[%s] IpAddress ----> %hs\n", szAdapter, pAddressList->IpAddress.String);
                if(strcmp(pAddressList->IpAddress.String, "0.0.0.0") != 0)
                {
                    return /*bAdapterHasIpAddress =*/ TRUE;
                }
                pAddressList = pAddressList->Next;
            }
        }
        pAdapterInfo = pAdapterInfo->Next;
    }

    return FALSE;
}



BOOL
WaitForLoosingConnection
// return TRUE if the adapter loose connection within MAX_TRIAL secs
(
WCHAR *szAdapter
)
{
    for (int seconds = 0 ; seconds < MAX_TRIAL ; ++seconds)
    {
        if (!AdapterHasIpAddress(szAdapter))
        {
            return TRUE;
        }
        Sleep(1000);
    }
    return FALSE;
}


BOOL
WaitForGainingConnection
// return TRUE if the adapter gains connection within MAX_TRIAL secs
(
WCHAR *szAdapter
)
{
    ce::auto_local_mem promptCloser;
    size_t promptSize = wcslen(szAdapter) + 100;
    LPWSTR pPrompt = (LPWSTR) LocalAlloc(0, promptSize * sizeof(WCHAR));
    if (pPrompt == NULL)
    {
        pPrompt = szAdapter;
    }
    else
    {
        promptCloser = pPrompt;
        _snwprintf(pPrompt, promptSize-2,
                   L"Waiting for connection to \"%s\"", szAdapter);
        pPrompt[promptSize-1] = L'\0';
    }

    g_pDialog->SetSuppressLogPrompts(false);
    for (int seconds = 0 ; seconds < MAX_TRIAL / 10 ; ++seconds)
    {
        if (AdapterHasIpAddress(szAdapter))
        {
            return TRUE;
        }
        switch (g_pDialog->RunDialog(pPrompt, 1000 * 10, s_pOkCancelPrompt))
        {
            case QADialog_t::ResponseTimeout:
            case QADialog_t::ResponseOK:
                break;
            default:
                return FALSE;
        }
        g_pDialog->SetSuppressLogPrompts(true);
    }
    return FALSE;
}


int
WaitForAccessPointIsReady
// return values 0=not ready[timed out=5-mins], 1=yes ready, 2==no, not ready, user is asking to skip this test.
// check available SSID list at every 10-secs
(
LPWSTR szAdapter,
LPWSTR szSsidWantToSee
)
{
    static LPCWSTR BasePrompt = 
        L"note:\n"
        L"Test tool is waiting for the SSID to be prepared.\n" 
        L"It will wait up to 5 minutes.\n"
        L"Test tool will proceed to the next step when it sees %s"
        L" appear in the air.\n"
        L"If there is no preparation done by the 5-min limit, test tool"
        L" will report failure and skip this test.\n"
        L"Since tool-kit proceeds to the next step immediately when it"
        L" sees the SSID,\n"
        L"adjust all other necessary parameters first then change the"
        L" SSID to the one the tool is looking for.\n"
        L"If you change the SSID first, tool-kit may connect to the access"
        L" point while you are changing encryption parameters.\n";

    static LPCWSTR SsidPrompts[][2] = {
        { L"CE-OPEN",
          L"prepare an access point %s\n"
          L"SSID = %s\n"
          L"authentication=open, encryption=disabled\n"
          L"IP address = DHCP 192.168.0.x\n"
        },
        { L"CE-WEP40",
          L"prepare an access point %s\n"
          L"SSID = %s\n"
          L"authentication=open, encryption=WEP 40-bit"
          L" [key index=1, value=1234567890 (10-digit hexa value)]\n"
          L"IP address = DHCP 192.168.0.x\n"
        },
        { L"CE-WEP104",
          L"prepare an access point %s\n"
          L"SSID = %s\n"
          L"authentication=open, encryption=WEP 104-bit"
          L" [key index=1, value=qwertyuiopasd (13-chars)]\n"
          L"if your AP allows only hexa values for WEP key,"
          L" use 71 77 65 72 74 79 75 69 6F 70 61 73 64\n"
          L"IP address = DHCP 192.168.0.x\n"
        },
        { L"CE-ADHOC1",
          L"prepare an adhoc net %s.\n"
          L"adhoc net is a computer-to-computer wireless network.\n"
          L"use either another CE device or XP notebook to start %s\n"
          L"SSID = %s\n"
          L"authentication=open, encryption=disabled\n"
          L"there is no DHCP in typical adhoc net.\n"
        },
        { NULL,
          L"no special note on SSID %s\n"
        }
    };

    // Make sure there's a valid SSID.
    if (szSsidWantToSee == NULL || szSsidWantToSee[0] == L'\0')
    {
        szSsidWantToSee = L"unknown-SSID";
    }

    // Assemble the prompt for this SSID.
    LPCWSTR ssidBasePrompt;
    for (int spix = 0 ;; ++spix)
    {
        if (SsidPrompts[spix][0] == NULL ||
            wcscmp(szSsidWantToSee, SsidPrompts[spix][0]) == 0)
        {
            ssidBasePrompt = SsidPrompts[spix][1];
            break;
        }
    }
    size_t promptSize;
    promptSize = (wcslen(BasePrompt) + wcslen(ssidBasePrompt)
               + (wcslen(szSsidWantToSee) * 10))
               * sizeof(WCHAR);
    LPWSTR prompt = (LPWSTR) LocalAlloc(0, promptSize);
    if (prompt == NULL)
    {
        g_pKato->Log(LOG_FAIL,
                     L"Can't allocate %u bytes for usage prompt", 
                     promptSize);
        return 0;
    }
    promptSize = swprintf(prompt, BasePrompt,
                          szSsidWantToSee, szSsidWantToSee,
                          szSsidWantToSee, szSsidWantToSee);
    promptSize = swprintf(prompt + promptSize, ssidBasePrompt,
                          szSsidWantToSee, szSsidWantToSee,
                          szSsidWantToSee, szSsidWantToSee);

    // Prompt the user and wait for a response for the SSID.
    g_pDialog->SetSuppressLogPrompts(false);
    int iUserEvent = -1;
    for (int repeat = 0 ; iUserEvent < 0 ; ++repeat)
    {
        if (SsidIsAvailable(szAdapter, szSsidWantToSee))
        {
            iUserEvent = 1;
            break;
        }
        if (repeat >= 30)
        {
            iUserEvent = 0;
            break;
        }
        g_pKato->Log(LOG_COMMENT, L"waiting for %s", szSsidWantToSee);
        switch (g_pDialog->RunDialog(prompt, 10*1000, s_pOkCancelPrompt))
        {
            case QADialog_t::ResponseTimeout:
            case QADialog_t::ResponseOK:
                break;
            default:
                iUserEvent = 2;
                break;
        }
        g_pDialog->SetSuppressLogPrompts(true);
    }
 
    LocalFree(prompt);
    return iUserEvent;
}


// TEST PROCEDURE


TESTPROCAPI
TestProc_wzctool_check
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    // delete this registry for possible preconfigured SSID set
    // if wzctool finds this, it will try to configure as it is.
    RegDeleteKey(HKEY_CURRENT_USER, L"Comm\\WZCTOOL");

    if(!RunCommand(L"wzctool"))
        g_pKato->Log(LOG_FAIL, L"cannot run wzctool");
    else
        g_pKato->Log(LOG_COMMENT, L"wzctool runs fine");
	return GetCode();
}






TESTPROCAPI
TestProc_wzctool_help
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!RunCommand(L"wzctool -?"))
        g_pKato->Log(LOG_FAIL, L"cannot run wzctool");
    else
    {
        if(
            wcsstr(g_szCommandOutput, L"wzctool") &&
            wcsstr(g_szCommandOutput, L"help") &&
            wcsstr(g_szCommandOutput, L"usage")
            )
            g_pKato->Log(LOG_COMMENT, L"help message ok");
        else
            g_pKato->Log(LOG_FAIL, L"cannot get help wzctool");

        DumpCommandOutputToTuxEcho();
    }

	return GetCode();
}





TESTPROCAPI
TestProc_wzctool_enum
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!RunCommand(L"wzctool -e"))
        g_pKato->Log(LOG_FAIL, L"cannot run wzctool");
    else
    {
        if(wcsstr(g_szCommandOutput, L"WZCEnumInterfaces() error"))
            g_pKato->Log(LOG_FAIL, L"WZC API fail");

        if(wcsstr(g_szCommandOutput, L"no wireless card"))
        {
            g_pKato->Log(LOG_COMMENT, L"no wireless card (ok)");

            if(g_TryXWIFI11B)
            {
                if(CardInsert_XWIFI11B())
                {
                    RunCommand(L"wzctool -e");
                    if(wcsstr(g_szCommandOutput, L"WZCEnumInterfaces() error"))
                        g_pKato->Log(LOG_FAIL, L"WZC API fail");

                    if(wcsstr(g_szCommandOutput, L"no wireless card"))
                        g_pKato->Log(LOG_FAIL, L"wireless card not installed");
                    else if(!wcsstr(g_szCommandOutput, L"XWIFI11B1"))
                        g_pKato->Log(LOG_FAIL, L"XNWIFI11B1 should come, but it was not in the wifi card list");
                    else
                        g_pKato->Log(LOG_COMMENT, L"XNWIFI11B1 is listed as a wifi adatper. (ok)");

                    // keep the XWIFI11B1 card for future use
                }
                else
                    g_pKato->Log(LOG_COMMENT, L"avoiding 'ntapconfig -card-insert xwifi11b'");
            }
        }
        
        if(wcsstr(g_szCommandOutput, L"wifi-card"))
            g_pKato->Log(LOG_COMMENT, L"wifi card found");

        DumpCommandOutputToTuxEcho();
    }
	return GetCode();
}





TESTPROCAPI
TestProc_wzctool_query
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    WCHAR szCmdLine1[128];
    WCHAR *sz1;

    switch(lpFTE->dwUserData)
    {
    case 100:
        g_pKato->Log(LOG_COMMENT, L"wzctool -q  [without adapter-name arg] when no WIFI card exists");
        GetWiFiCardName();
        if(*g_szWiFiCard1)
        {
            g_pKato->Log(LOG_COMMENT, L"system has adapter %s", g_szWiFiCard1);
            if(g_TryXWIFI11B)
            {
                g_pKato->Log(LOG_COMMENT, L"ejecting XWIFI11B1 in order to test 'wzctool -q' on no-WIFI card case");
                CardEject_XWIFI11B();
            }
            GetWiFiCardName();
            if(*g_szWiFiCard1)
            {
                g_pKato->Log(LOG_COMMENT, L"system still has WIFI card %s", g_szWiFiCard1);
                break;
            }
        }

        // wzctool finds first wifi card, then query that
        if(!RunCommand(L"wzctool -q"))
            g_pKato->Log(LOG_FAIL, L"cannot run wzctool");

        // g_szCommandOutput = system has no wireless card.
        if(wcsstr(g_szCommandOutput, L"no wireless card"))
            g_pKato->Log(LOG_COMMENT, L"correct error message (ok)");
        else
            g_pKato->Log(LOG_FAIL, L"incorrect error message");
        break;

    case 101:
        g_pKato->Log(LOG_COMMENT, L"wzctool -q  [without adapter-name arg] when WIFI card exists");
        // wzctool finds first wifi card, then query that
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");

            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }
        if(!RunCommand(L"wzctool -q"))
            g_pKato->Log(LOG_FAIL, L"cannot run wzctool");

        if(wcsstr(g_szCommandOutput, L"wireless card found"))
            g_pKato->Log(LOG_COMMENT, L"correct 'found' message (ok)");
        else
            g_pKato->Log(LOG_FAIL, L"didn't have 'found' message");
        break;

    case 102:   // L"wzctool -q WIFI1"   WIFI1 is the active wifi card
        g_pKato->Log(LOG_COMMENT, L"wzctool -q XWIFI11B1 [with adapter-name arg]");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");

            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }
        g_pKato->Log(LOG_COMMENT, L"found active wifi = %s", g_szWiFiCard1);
        g_pKato->Log(LOG_COMMENT, L"now querying %s", g_szWiFiCard1);

        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            g_pKato->Log(LOG_FAIL, L"-q option error");
        else
            g_pKato->Log(LOG_COMMENT, L"-q option ok");
        break;

    case 103:   // L"wzctool -q XSWQAZ1"   XSWQAZ1 is non-existing wifi card
        g_pKato->Log(LOG_COMMENT, L"wzctool -q XSWQAZ1 [with non-exising adapter-name arg]");
        wsprintf(szCmdLine1, L"wzctool -q %s", L"XSWQAZ1");
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            g_pKato->Log(LOG_COMMENT, L"correct error message (ok)");
        else
            g_pKato->Log(LOG_FAIL, L"incorrect message.");
        break;

    case 104:
        g_pKato->Log(LOG_COMMENT, L"wzctool -q XWIFI11B1 with empty BSSID list");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");
            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }

        if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        {
            g_pKato->Log(LOG_COMMENT, L"SKIP: this test is only for XWIFI11B1 [system has %s]", g_szWiFiCard1);
            break;
        }

        g_pKato->Log(LOG_COMMENT, L"clearing test SSID set for XWIFI11B");
        ClearTestSsidSet_XWIFI11B();

        g_pKato->Log(LOG_COMMENT, L"found active wifi = %s", g_szWiFiCard1);
        g_pKato->Log(LOG_COMMENT, L"now querying %s", g_szWiFiCard1);
        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            g_pKato->Log(LOG_FAIL, L"-q option error");
        else
            g_pKato->Log(LOG_COMMENT, L"-q option ok");

        // following SSIDs should be visible
        sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(
            wcsstr(sz1, L"CE-OPEN") ||
            wcsstr(sz1, L"CE-WEP40") ||
            wcsstr(sz1, L"CE-WEP104") ||
            wcsstr(sz1, L"MSFTWLAN") ||
            wcsstr(sz1, L"CE-ADHOC1")
            )
            g_pKato->Log(LOG_FAIL, L"visible-SSIDs should be empty");
        else
            g_pKato->Log(LOG_COMMENT, L"visible-SSID-list is correct");
        break;

    case 105:
        // test only with XWIFI11B1
        g_pKato->Log(LOG_COMMENT, L"wzctool -q XWIFI11B1 with test set BSSID list");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");
            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }

        if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        {
            g_pKato->Log(LOG_COMMENT, L"SKIP: this test is only for XWIFI11B1 [system has %s]", g_szWiFiCard1);
            break;
        }

        g_pKato->Log(LOG_COMMENT, L"preparing test-SSID-set for XWIFI11B");
        AddTestSsidSet_XWIFI11B();

        g_pKato->Log(LOG_COMMENT, L"found active wifi = %s", g_szWiFiCard1);
        g_pKato->Log(LOG_COMMENT, L"now querying %s", g_szWiFiCard1);
        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            g_pKato->Log(LOG_FAIL, L"-q option error");
        else
            g_pKato->Log(LOG_COMMENT, L"-q option ok");

        // following SSIDs should be visible
        sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(
            wcsstr(sz1, L"CE-OPEN") &&
            wcsstr(sz1, L"CE-WEP40") &&
            wcsstr(sz1, L"CE-WEP104") &&
            wcsstr(sz1, L"MSFTWLAN") &&
            wcsstr(sz1, L"CE-ADHOC1")
            )
            g_pKato->Log(LOG_COMMENT, L"visible-SSID-list is correct");
        else
            g_pKato->Log(LOG_FAIL, L"visible-SSIDs should be empty");
        break;
    }

    DumpCommandOutputToTuxEcho();

	return GetCode();
}




TESTPROCAPI
TestProc_wzctool_connect
// test set
/*
[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-OPEN]
	"MacAddress"="00:40:00:00:00:00"
	"Privacy"=dword:0
	"Rssi"=dword:79
	"InfrastructureMode"=dword:1
	"DHCP"="192.168.0.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-WEP40]
	"MacAddress"="00:40:00:00:00:11"
	"InfrastructureMode"=dword:1
	"AuthMode"=dword:0
	"Privacy"=dword:1
	"key-index"=dword:1
	"key"="0x1234567890"
	"DHCP"="192.168.0.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-WEP104]
	"MacAddress"="00:40:00:00:00:22"
	"InfrastructureMode"=dword:1
	"AuthMode"=dword:0
	"Privacy"=dword:1
	"key-index"=dword:1
	"key"="qwertyuiopasd"
	"DHCP"="192.168.0.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\MSFTWLAN]
	"MacAddress"="00:40:00:00:00:33"
	"Privacy"=dword:1
	"Rssi"=dword:100
	"InfrastructureMode"=dword:1

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-ADHOC1]
	"MacAddress"="10:33:66:9D:62:F0"
	"Privacy"=dword:0
	"Rssi"=dword:100
	"InfrastructureMode"=dword:0
*/
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    GetWiFiCardName();
    if(!*g_szWiFiCard1)
    {
        g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
        else
        {
            return GetCode();
        }
    }

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(g_szWiFiCard1);

//    if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
//        g_pKato->Log(LOG_COMMENT, L"WARNING this test needs XWIFI11B1 [system has %s]", g_szWiFiCard1);

    // XWIFI11B1 case:
    // we need to make sure the correct DHCP/AUTOIP scenario, so should clean Parms\TCPIP registry
    // if not, previous cached IP address will come when connected.
    // [HKEY_LOCAL_MACHINE\Comm\XWIFI11B1\Parms\TCPIP]
    //   "EnableDHCP"=dword:1
    //   "AutoCfg"=dword:1
    LPWSTR szAdapterTcpIpReg = (LPWSTR)LocalAlloc(LMEM_FIXED, MAX_PATH);
    wsprintf(szAdapterTcpIpReg, L"COMM\\%s\\Parms\\TCPIP", g_szWiFiCard1);
    if(RegDeleteKey(HKEY_LOCAL_MACHINE, szAdapterTcpIpReg) != ERROR_SUCCESS)
        g_pKato->Log(LOG_COMMENT, L"WARNING cannot delete registry key [%s]", szAdapterTcpIpReg);
    HKEY hKey1 = NULL;
    if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szAdapterTcpIpReg, 0, NULL, 0, 0, NULL, &hKey1, NULL) == ERROR_SUCCESS) {
        DWORD dw1 = 1;
        RegSetValueEx(hKey1, L"EnableDHCP", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegSetValueEx(hKey1, L"AutoCfg", 0, REG_DWORD, (PBYTE)&dw1, sizeof(DWORD));
        RegCloseKey(hKey1);
    }
    LocalFree(szAdapterTcpIpReg);

    g_pKato->Log(LOG_COMMENT, L"WiFi card reset done. ([Preferred List] is cleared)");
    Sleep(1000);

    if(g_TryXWIFI11B)
        g_pKato->Log(LOG_COMMENT, L"preparing test-SSID-set for XWIFI11B");
//    AddTestSsidSet_XWIFI11B();

    WCHAR szCmdLine1[128];
    WCHAR *szIpAddrForm_NULL = L"0.0.0.0";  // zero IP when connection fails
    WCHAR *szIpAddrForm_DHCP = L"192.168."; // when correctly connected and got DHCP address
    WCHAR *szIpAddrForm_AUTO = L"169.254."; // when connected but failed getting DHCP offer
    WCHAR *szExpectedIpAddrForm = szIpAddrForm_NULL;
    int iSsidAvailability = 1;

    switch(lpFTE->dwUserData)
    {
    case 100:   // CE-OPEN
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-OPEN  [with all default opton]");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-OPEN");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-OPEN");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP;
        break;

    case 110:   // CE-WEP40
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 1/0x1234567890");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 1/0x1234567890");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP;
        break;

    case 111:   // CE-WEP40 with incorrect WEP key index
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 2/0x1234567890");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 2/0x1234567890");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 112:   // CE-WEP40 with incorrect WEP key value
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 1/0x0987654321");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 1/0x0987654321");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 113:   // CE-WEP40 without WEP-encryption
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-WEP40");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 120:   // CE-WEP104
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-WEP104 -auth open -encry wep key 1/qwertyuiopasd");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP104");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP104 -auth open -encr wep -key 1/qwertyuiopasd");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP;
        break;

    case 150:   // CE-ADHOC1
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-ADHOC1 -adhoc");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-ADHOC1");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC1 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 151:   // CE-ADHOC1 existing adhoc, but different combination
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-ADHOC1 -encry wep key 1/0x1234567890 -adhoc");
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-ADHOC1");
        if(iSsidAvailability != 1)
            break;
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC1 -encry wep key 1/0x1234567890 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 152:   // CE-ADHOC2 non-existing adhoc, so my WIFI-card should start this adhoc net
        g_pKato->Log(LOG_COMMENT, L"wzctool -c -ssid CE-ADHOC2 -adhoc");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC2 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;
    }

    if(iSsidAvailability == 0)  // timed out case
    {
        g_pKato->Log(LOG_COMMENT, L"waited for SSID appearance but it timed out");
        return GetCode();
    }
    else if(iSsidAvailability == 2) // skip requested
    {
        g_pKato->Log(LOG_COMMENT, L"user asked to skip");
        return GetCode();
    }

    DumpCommandOutputToTuxEcho();
    g_pKato->Log(LOG_COMMENT, L"waiting for %s gets an address", g_szWiFiCard1);
    WaitForGainingConnection(g_szWiFiCard1);

    RunCommand(L"ipconfig");
    DumpCommandOutputToTuxEcho();
    WCHAR *sz1 = wcsstr(g_szCommandOutput, g_szWiFiCard1);
    if(!sz1)
        g_pKato->Log(LOG_FAIL, L"no adapter [%s] found", g_szWiFiCard1);
    else
    {
        WCHAR *szIpAddr = sz1;
        sz1 = wcsstr(sz1, L"IP Address");
        if (sz1)
        {
            for (; *sz1 && !iswdigit(*sz1); sz1++) ;
            szIpAddr = sz1;
        }
        for(sz1 = szIpAddr ; *sz1 && (iswdigit(*sz1)||(*sz1==L'.')); sz1++) ;
        *sz1 = L'\0';
        g_pKato->Log(LOG_COMMENT, L"IP Address = %s", szIpAddr);
        if(wcscmp(szIpAddr, L"0.0.0.0")==0)
            g_pKato->Log(LOG_FAIL, L"WIFI1 has IP address 0.0.0.0");
        else
        {
            if(wcsncmp(szIpAddr, szExpectedIpAddrForm, wcslen(szExpectedIpAddrForm))==0)
                g_pKato->Log(LOG_COMMENT, L"expected %s, current IP =%s (pass)", szExpectedIpAddrForm, szIpAddr);
            else
                g_pKato->Log(LOG_FAIL, L"expected %s, current IP =%s (FAIL)", szExpectedIpAddrForm, szIpAddr);
        }
    }

	return GetCode();
}






TESTPROCAPI
TestProc_wzctool_reset
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    GetWiFiCardName();
    if(!*g_szWiFiCard1)
    {
        g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
    }

    if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        g_pKato->Log(LOG_COMMENT, L"WARNING this test needs XWIFI11B1 [has %s]", g_szWiFiCard1);

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(L"XWIFI11B1");
    DumpCommandOutputToTuxEcho();
    Sleep(1000);
	return GetCode();
}





WCHAR *g_szReg_CE_OPEN[] =
{
    L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL]",
	L"    \"ssid\"=\"CE-OPEN\"",
    NULL
};

WCHAR *g_szReg_CE_WEP40[] =
{
    L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL]",
	L"    \"ssid\"=\"CE-WEP40\"",
    L"    \"authentication\"=dword:0",
    L"    \"encryption\"=dword:0",
    L"    \"key\"=\"1/0x1234567890\"",
    NULL
};



TESTPROCAPI
TestProc_wzctool_registry
#if 0
    registry datail

    registry path  = HKEY_CURRENT_USER\Comm\WZCTOOL
    SSID           = REG_SZ (max 32 char)
    authentication = REG_DWORD
    encryption     = REG_DWORD
    key            = REG_SZ
    eap            = REG_SZ
    adhoc          = REG_DWORD


    authentication =
       0 = open (Ndis802_11AuthModeOpen)
       1 = shared-key (Ndis802_11AuthModeShared)
       4 = WPA-PSK (Ndis802_11AuthModeWPAPSK)
       5 = WPA-NONE (Ndis802_11AuthModeWPANone)
       3 = WPA (Ndis802_11AuthModeWPA)

    encryption =
       0 = WEP (Ndis802_11WEPEnabled)
       1 = no-encrption (Ndis802_11WEPDisabled)
       4 = TKIP (Ndis802_11Encryption2Enabled)

    eap =
       13 = TLS
       25 = PEAP
       4 = MD5

    adhoc =
       1 = adhoc net
       if "adhoc" value not exists or its value is 0 = inrastructure net

    registry example:

    HKEY_CURRENT_USER\Comm\WZCTOOL
       "SSID" = "CEWEP40"
       "authentication" = dword:0  ;OPEN-authentication
       "encryption" = dword:0      ;WEP-encryption
       "key" = "1/0x1234567890    ;key index=1, 40-bit key"
       "adhoc" = dword:0


    Note:
    Storing encryption key in registry as a clear text form could be a security concern.
    somebody could ready keys by scanning registry.
    possible way to avoid is encrypt the key values so that only your software can understand.
#endif
(
UINT uMsg,
TPPARAM tParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    GetWiFiCardName();
    if(!*g_szWiFiCard1)
    {
        g_pKato->Log(LOG_COMMENT, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                g_pKato->Log(LOG_COMMENT, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
    }

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(g_szWiFiCard1);
    g_pKato->Log(LOG_COMMENT, L"WiFi card reset done. ([Preferred List] is cleared)");
    Sleep(1000);

    if(g_TryXWIFI11B) {
        g_pKato->Log(LOG_COMMENT, L"preparing test-SSID-set for XWIFI11B");
        AddTestSsidSet_XWIFI11B();
    }

    if(RegDeleteKey(HKEY_CURRENT_USER, L"Comm\\WZCTOOL") == ERROR_SUCCESS)
        g_pKato->Log(LOG_COMMENT, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deleted");
    else
        g_pKato->Log(LOG_COMMENT, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deletion failed (this key might not exist)");
    BOOL bShouldHaveIpAddress = TRUE;
    int iSsidAvailability = 1;

    switch(lpFTE->dwUserData)
    {
    case 100:   // "without registry entry", this should have no connection at all
        g_pKato->Log(LOG_COMMENT, L"wzctool -registry (with empty set)");
        RunCommand(L"wzctool -registry");
        Sleep(10000);   // wait 10 sec, then check IP address
        bShouldHaveIpAddress = FALSE;

    case 101:
        if(!g_TryXWIFI11B) {
            g_pKato->Log(LOG_COMMENT, L"prepare an access point CE-OPEN");
            g_pKato->Log(LOG_COMMENT, L"SSID = CE-OPEN");
            g_pKato->Log(LOG_COMMENT, L"authentication=open, encryption=disabled");
            g_pKato->Log(LOG_COMMENT, L"IP address = DHCP 192.168.0.x");
            g_pKato->Log(LOG_COMMENT, L"");
        }
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-OPEN");
        if(iSsidAvailability != 1)
            break;
        g_pKato->Log(LOG_COMMENT, L"wzctool -registry (for CE-OPEN)");
        SaveToRegistry(g_szReg_CE_OPEN);
        RunCommand(L"wzctool -registry");
        break;

    case 102:
        if(!g_TryXWIFI11B) {
            g_pKato->Log(LOG_COMMENT, L"prepare an access point CE-WEP40");
            g_pKato->Log(LOG_COMMENT, L"SSID = CE-WEP40");
            g_pKato->Log(LOG_COMMENT, L"authentication=open, encryption=WEP 40-bit [key index=1, value=1234567890 (10-digit hexa value)]");
            g_pKato->Log(LOG_COMMENT, L"IP address = DHCP 192.168.0.x");
            g_pKato->Log(LOG_COMMENT, L"");
        }
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        g_pKato->Log(LOG_COMMENT, L"wzctool -registry (for CE_WEP40)");
        SaveToRegistry(g_szReg_CE_WEP40);
        RunCommand(L"wzctool -registry");
        break;

    case 103:
        if(!g_TryXWIFI11B) {
            g_pKato->Log(LOG_COMMENT, L"prepare an access point CE-WEP40");
            g_pKato->Log(LOG_COMMENT, L"SSID = CE-WEP40");
            g_pKato->Log(LOG_COMMENT, L"authentication=open, encryption=WEP 40-bit [key index=1, value=1234567890 (10-digit hexa value)]");
            g_pKato->Log(LOG_COMMENT, L"IP address = DHCP 192.168.0.x");
            g_pKato->Log(LOG_COMMENT, L"");
        }
        iSsidAvailability = WaitForAccessPointIsReady(g_szWiFiCard1, L"CE-WEP40");
        if(iSsidAvailability != 1)
            break;
        g_pKato->Log(LOG_COMMENT, L"wzctool (without any argument, with registry set for CE_WEP40)");
        SaveToRegistry(g_szReg_CE_WEP40);
        RunCommand(L"wzctool");
        break;
    }

    if(iSsidAvailability == 0)  // timed out case
    {
        g_pKato->Log(LOG_COMMENT, L"waited for SSID appearance but it timed out");
        return GetCode();
    }
    else if(iSsidAvailability == 2) // skip requested
    {
        g_pKato->Log(LOG_COMMENT, L"user asked to skip");
        return GetCode();
    }

    DumpCommandOutputToTuxEcho();

    if(bShouldHaveIpAddress)
    {
        WaitForGainingConnection(g_szWiFiCard1);
        Sleep(1000);
        RunCommand(L"ipconfig");
        DumpCommandOutputToTuxEcho();
        WCHAR *sz1 = wcsstr(g_szCommandOutput, g_szWiFiCard1);
        if(!sz1)
            g_pKato->Log(LOG_FAIL, L"no adapter [%s] found", g_szWiFiCard1);
        else
        {
            for(sz1 = wcsstr(sz1, L"IP Address"); *sz1 && !iswdigit(*sz1); sz1++) ;
            WCHAR *szIpAddr = sz1;
            for( ; *sz1 && (iswdigit(*sz1)||(*sz1==L'.')); sz1++) ;
            *sz1 = L'\0';
            g_pKato->Log(LOG_COMMENT, L"IP Address = %s", szIpAddr);
            if(wcscmp(sz1, L"0.0.0.0")==0)
                g_pKato->Log(LOG_FAIL, L"WIFI1 has IP address 0.0.0.0");
        }
    }
    else
    {
        if(AdapterHasIpAddress(g_szWiFiCard1))
            g_pKato->Log(LOG_FAIL, L"adapter %s should not have an IP address, but it has", g_szWiFiCard1);
        else
            g_pKato->Log(LOG_COMMENT, L"%s has no change (ok)", g_szWiFiCard1);
    }

    // remove the wzctool-preconfig registry
    if(RegDeleteKey(HKEY_CURRENT_USER, L"Comm\\WZCTOOL") == ERROR_SUCCESS)
        g_pKato->Log(LOG_COMMENT, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deleted");
    else
        g_pKato->Log(LOG_COMMENT, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deletion failed (this key might not exist)");
    return GetCode();
}




FUNCTION_TABLE_ENTRY g_lpFTE[] =
{      // comment
    { L"wzctool run",        1,   0,  1, TestProc_wzctool_check },
    { L"wzctool -?",         1,   0,  2, TestProc_wzctool_help },
    { L"wzctool -reset",     1,   0,  3, TestProc_wzctool_reset },
    { L"wzctool -e",         1,   0,  4, TestProc_wzctool_enum },

    { L"wzctool -q",                                 1, 100, 300, TestProc_wzctool_query }, // query when no wifi adapter exists
    { L"wzctool -q",                                 1, 101, 301, TestProc_wzctool_query }, // query when wifi adapter installed
    { L"wzctool -q WIFI1",                           1, 102, 302, TestProc_wzctool_query }, // query active wifi device
    { L"wzctool -q non-existing-wifi card",          1, 103, 303, TestProc_wzctool_query }, // query non-existing wifi device
    { L"wzctool -q XWIFI11B1 with empty BSSID list", 1, 104, 304, TestProc_wzctool_query }, // query XWIFI11B1 with empty BSSID list
    { L"wzctool -q XWIFI11B1 with test BSSID-set",   1, 105, 305, TestProc_wzctool_query }, // query XWIFI11B1 with test set BSSID list

    { L"wzctool -c -ssid CE-OPEN",    1, 100, 400, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-WEP40",  1, 110, 410, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-WEP40 (incorrect WEP key index)",  1, 111, 411, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-WEP40 (incorrect WEP key value)",  1, 112, 412, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-WEP40 (without WEP encryption)",   1, 113, 413, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-WEP104", 1, 120, 420, TestProc_wzctool_connect },
    //
    { L"wzctool -c -ssid CE-ADHOC1", 1, 150, 450, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-ADHOC1", 1, 151, 451, TestProc_wzctool_connect },
    { L"wzctool -c -ssid CE-ADHOC2", 1, 152, 452, TestProc_wzctool_connect },

    { L"wzctool -registry",   1, 100, 500, TestProc_wzctool_registry },  // "no registry" should do nothing
    { L"wzctool -registry",   1, 101, 501, TestProc_wzctool_registry },  // "good registry" should connect
    { L"wzctool -registry",   1, 102, 502, TestProc_wzctool_registry },  // "good registry" should connect
    { L"wzctool",             1, 103, 503, TestProc_wzctool_registry },  // without parameter but having registry

    { NULL,            0,   0, 0, NULL }
};
