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



#define	MAXLOGLENGTH		1024

typedef enum{
    FAIL = 1,    
    ECHO = 2,    
    PASS = 3,
    ATTN = 4
} infoType;



SPS_SHELL_INFO  *gpShellInfo;
CKato           *gpKato		= NULL;
CKato           *gpKatoBare	= NULL;
HINSTANCE       ghInstDll	= NULL;
HINSTANCE       ghInstExe	= NULL;
HINSTANCE       ghInst		= NULL;

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
    ghInstDll = (HINSTANCE)hInst;
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
        gpKato = (CKato *)KatoGetDefaultObject();
        gpKatoBare	= (CKato *)KatoCreate(TEXT("KatoBare"));
        ((LPSPS_LOAD_DLL)spParam)->fUnicode	= TRUE;
        break;

    case SPM_UNLOAD_DLL:
        break;

    case SPM_START_SCRIPT:
    case SPM_STOP_SCRIPT:
        break;

    case SPM_SHELL_INFO:
        gpShellInfo= (LPSPS_SHELL_INFO)spParam;
        ParseCmdLine(gpShellInfo->szDllCmdLine);
        ghInst     = gpShellInfo->hLib;
        ghInstExe  = gpShellInfo->hInstance;
        gpShellInfo->fXML ? gpKato->SetXML(true) : gpKato->SetXML(false);
        srand((unsigned)GetTickCount());
        break;

    case SPM_BEGIN_GROUP:
    case SPM_END_GROUP:
        break;

    case SPM_BEGIN_TEST:
        gpKato->BeginLevel(0,TEXT("SPM_BEGIN_TEST"));
        break;

    case SPM_END_TEST:
        gpKato->EndLevel(TEXT("SPM_END_TEST"));
        break;

    case SPM_EXCEPTION:
        break;

    default:
        return SPR_NOT_HANDLED;

    }
    return SPR_HANDLED;
}




TESTPROCAPI
GetCode
(
void
)
{
    if(gpKato->GetVerbosityCount((DWORD)FAIL))
        return TPR_FAIL;
    return TPR_PASS;
}



void
LogInfo
(
infoType	iType,
LPCTSTR		szFormat,
...
)
{
    TCHAR	szBuffer[MAXLOGLENGTH];

    if(!gpKato)
        return;

    switch(iType)
    {
    case FAIL:
        _tcscpy(szBuffer,TEXT("FAIL:"));
        break;

    case ECHO:
        _tcscpy(szBuffer,TEXT("ECHO:"));
        break;

    case PASS:
        _tcscpy(szBuffer,TEXT("PASS:"));
        break;

    case ATTN:
        _tcscpy(szBuffer,TEXT("ATTN:"));
        break;
    }

    va_list pArgs; 
    va_start(pArgs, szFormat);
    //	windows printf's don't support floats and doubles
    //	now we use the crt's printf
    //wvsprintf(szBuffer+_tcslen(szBuffer), szFormat, pArgs);
    _vstprintf( szBuffer+_tcslen(szBuffer), szFormat, pArgs);
    va_end(pArgs);

    switch(iType)
    {
    case FAIL:
        gpKato->Log(FAIL,szBuffer);
        break;

    case ECHO:
        gpKato->Log(ECHO,szBuffer);
        break;

    case PASS:
        gpKato->Log(PASS,szBuffer);
        break;

    case ATTN:
        gpKato->Log(ATTN,szBuffer);
        break;
    }
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
    gpKato->Log(ECHO, L"cmd line: %s", szCmdLine);
    if(wcsstr(szCmdLine, L"xwifi11b") || wcsstr(szCmdLine, L"XWIFI11B"))
    {
        g_TryXWIFI11B = TRUE;
        gpKato->Log(ECHO, L"will try XWIFI11B card");
    }

    if(wcsstr(szCmdLine, L"-show cmdout"))
        g_ShowCmdOut = TRUE;
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


WCHAR g_szCommandOutput[0x10000];
// we are going to keep only first 64k-chars of output.


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
    LogInfo(ECHO, L"command %s\n", szCommand);

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
        LogInfo(FAIL, L"Cannot run '%s'. CreateProcess() failed, error=0x%08X", szCommand, GetLastError());
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
        LogInfo(ECHO, L"no output");
    DeleteFile(szDebugConsoleName);
    return TRUE;
}



void
DumpTextBufferToTuxEcho
// used to dump ;
(
IN WCHAR *szText
)
{
    if(!g_ShowCmdOut)
        return;

    LogInfo(ECHO, L"g_szCommandOutput[] =");
    for(WCHAR *p1=szText; *p1; )
    {
        for(WCHAR *p2=p1; *p2 && (*p2 & 0xFFE0); p2++)
            ;
        WCHAR wchr1 = *p2;
        *p2 = L'\0';
        LogInfo(ECHO, L"%s", p1);
        *p2 = wchr1;
        for(; *p2 && !(*p2 & 0xFFE0); p2++)
            ;
        p1 = p2;
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
        LogInfo(ECHO, L"cannot find required files");
        return FALSE;
    }

    if(!RunCommand(L"ntapconfig -card-insert xwifi11b"))
    {
        LogInfo(ECHO, L"cannot run 'ntapconfig'");
        return FALSE;
    }

    if(WaitForWiFiCardDetection(L"XWIFI11B1"))
        LogInfo(ECHO, L"XWIFI11B1 is inserted");
    else
        LogInfo(ECHO, L"WARNING XWIFI11B1 insertion failure");
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
        LogInfo(ECHO, L"cannot run 'ntapconfig'");
        return FALSE;
    }

    if(WaitForWiFiCardRemoval(L"XWIFI11B1"))
        LogInfo(ECHO, L"XWIFI11B1 is ejected");
    else
        LogInfo(ECHO, L"WARNING XWIFI11B1 ejection failure");
    Sleep(1000);
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

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CEOPEN]",
	L"    \"MacAddress\"=\"00:40:00:00:00:00\"",
	L"    \"Privacy\"=dword:0",
	L"    \"Rssi\"=dword:79",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"DHCP\"=\"100.100.*.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-WEP40]",
	L"    \"MacAddress\"=\"00:40:00:00:00:11\"",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"AuthMode\"=dword:0",
	L"    \"Privacy\"=dword:1",
	L"    \"key-index\"=dword:1",
	L"    \"key\"=\"0x1234567890\"",
	L"    \"DHCP\"=\"100.100.*.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CE-WEP104]",
	L"    \"MacAddress\"=\"00:40:00:00:00:22\"",
	L"    \"InfrastructureMode\"=dword:1",
	L"    \"AuthMode\"=dword:0",
	L"    \"Privacy\"=dword:1",
	L"    \"key-index\"=dword:1",
	L"    \"key\"=\"qwertyuiopasd\"",
	L"    \"DHCP\"=\"100.101.*.*\"",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\MSFTWLAN]",
	L"    \"MacAddress\"=\"00:40:00:00:00:33\"",
	L"    \"Privacy\"=dword:1",
	L"    \"Rssi\"=dword:100",
	L"    \"InfrastructureMode\"=dword:1",

    L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST\\CEADHOC1]",
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
        LogInfo(FAIL, L"file creation error (%s)", "TestSsidSet1.reg");
    	return;
    }
    for(WCHAR **p=szRegData; *p; p++)
        fwprintf(fp, L"%s\n", *p);
    fclose(fp);

    WCHAR szCmdLine1[128];
    wsprintf(szCmdLine1, L"ntapconfig -wifi add tmp1.reg");
    RunCommand(szCmdLine1);
    DumpTextBufferToTuxEcho(g_szCommandOutput);
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
        LogInfo(ECHO, L"checking SSID list");
        RunCommand(L"wzctool -q");

        WCHAR* sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
#if 0
        if(
            wcsstr(sz1, L"CEOPEN") &&
            wcsstr(sz1, L"CE-WEP40") &&
            wcsstr(sz1, L"CE-WEP104") &&
            wcsstr(sz1, L"MSFTWLAN") &&
            wcsstr(sz1, L"CEADHOC1")
            )
#endif
        if(sz1 && wcsstr(sz1, L"CEOPEN"))
            break;

        Sleep(1000);
    }
    if(i < MAX_TRIAL)
        LogInfo(ECHO, L"test SSID set ready");
    else
        LogInfo(ECHO, L"WARNING SSIDs add failed");
}



void
ClearTestSsidSet_XWIFI11B
(
)
{
    if(RegDeleteKey(HKEY_LOCAL_MACHINE, L"Comm\\XWIFI11B-BSSID-LIST") == ERROR_SUCCESS)
        LogInfo(ECHO, L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST] deleted");
    else
        LogInfo(ECHO, L"[HKEY_LOCAL_MACHINE\\Comm\\XWIFI11B-BSSID-LIST] deletion failed (this key might not exist)");
    RunCommand(L"ntapconfig -wifi refresh");

    DumpTextBufferToTuxEcho(g_szCommandOutput);

    // wait for SSID update
    for(int i=0; i<MAX_TRIAL; i++)
    {
        RunCommand(L"wzctool -q");

        WCHAR* sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(sz1 && !wcsstr(sz1, L"CEOPEN"))
            break;
        Sleep(1000);
    }
    if(i < MAX_TRIAL)
        LogInfo(ECHO, L"SSID list is deleted");
    else
        LogInfo(ECHO, L"WARNING SSIDs list deletion failed");
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
        LogInfo(ECHO, L"no adapter\n");
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
                LogInfo(ECHO, L"[%s] IpAddress ----> %hs\n", szAdapter, pAddressList->IpAddress.String);
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
// return TRUE if the adapter loose connection within 300 secs (5 mins)
(
WCHAR *szAdapter
)
{
    for(int i=0; i<MAX_TRIAL && AdapterHasIpAddress(szAdapter); i++)
        Sleep(1000);
    return (i < MAX_TRIAL); // time out
}


BOOL
WaitForGainingConnection
// return TRUE if the adapter gains connection within 30 secs
(
WCHAR *szAdapter
)
{
    for(int i=0; i<MAX_TRIAL && !AdapterHasIpAddress(szAdapter); i++)
        Sleep(1000);
    return (i < MAX_TRIAL);
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
        LogInfo(FAIL, L"cannot run wzctool");
    else
        LogInfo(ECHO, L"wzctool runs fine");
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
        LogInfo(FAIL, L"cannot run wzctool");
    else
    {
        if(
            wcsstr(g_szCommandOutput, L"wzctool") &&
            wcsstr(g_szCommandOutput, L"help") &&
            wcsstr(g_szCommandOutput, L"usage")
            )
            LogInfo(ECHO, L"help message ok");
        else
            LogInfo(FAIL, L"cannot get help wzctool");

        DumpTextBufferToTuxEcho(g_szCommandOutput);
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
        LogInfo(FAIL, L"cannot run wzctool");
    else
    {
        if(wcsstr(g_szCommandOutput, L"WZCEnumInterfaces() error"))
            LogInfo(FAIL, L"WZC API fail");

        if(wcsstr(g_szCommandOutput, L"no wireless card"))
        {
            LogInfo(ECHO, L"no wireless card (ok)");

            if(g_TryXWIFI11B)
            {
                if(CardInsert_XWIFI11B())
                {
                    RunCommand(L"wzctool -e");
                    if(wcsstr(g_szCommandOutput, L"WZCEnumInterfaces() error"))
                        LogInfo(FAIL, L"WZC API fail");

                    if(wcsstr(g_szCommandOutput, L"no wireless card"))
                        LogInfo(FAIL, L"wireless card not installed");
                    else if(!wcsstr(g_szCommandOutput, L"XWIFI11B1"))
                        LogInfo(FAIL, L"XNWIFI11B1 should come, but it was not in the wifi card list");
                    else
                        LogInfo(ECHO, L"XNWIFI11B1 is listed as a wifi adatper. (ok)");

                    // keep the XWIFI11B1 card for future use
                }
                else
                    LogInfo(ECHO, L"avoiding 'ntapconfig -card-insert xwifi11b'");
            }
        }
        
        if(wcsstr(g_szCommandOutput, L"wifi-card"))
            LogInfo(ECHO, L"wifi card found");

        DumpTextBufferToTuxEcho(g_szCommandOutput);
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
        LogInfo(ECHO, L"wzctool -q  [without adapter-name arg] when no WIFI card exists");
        GetWiFiCardName();
        if(*g_szWiFiCard1)
        {
            LogInfo(ECHO, L"system has adapter %s", g_szWiFiCard1);
            if(wcscmp(g_szWiFiCard1, L"XWIFI11B1")==0)
            {
                LogInfo(ECHO, L"ejecting XWIFI11B1 in order to test 'wzctool -q' on no-WIFI card case");
                CardEject_XWIFI11B();
            }
            GetWiFiCardName();
            if(*g_szWiFiCard1)
            {
                LogInfo(ECHO, L"system still has WIFI card %s", g_szWiFiCard1);
                break;
            }
        }

        // wzctool finds first wifi card, then query that
        if(!RunCommand(L"wzctool -q"))
            LogInfo(FAIL, L"cannot run wzctool");

        // g_szCommandOutput = system has no wireless card.
        if(wcsstr(g_szCommandOutput, L"no wireless card"))
            LogInfo(ECHO, L"correct error message (ok)");
        else
            LogInfo(FAIL, L"incorrect error message");
        break;

    case 101:
        LogInfo(ECHO, L"wzctool -q  [without adapter-name arg] when WIFI card exists");
        // wzctool finds first wifi card, then query that
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            LogInfo(ECHO, L"cannot find WIFI card");

            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }
        if(!RunCommand(L"wzctool -q"))
            LogInfo(FAIL, L"cannot run wzctool");

        if(wcsstr(g_szCommandOutput, L"wireless card found"))
            LogInfo(ECHO, L"correct 'found' message (ok)");
        else
            LogInfo(FAIL, L"didn't have 'found' message");
        break;

    case 102:   // L"wzctool -q WIFI1"   WIFI1 is the active wifi card
        LogInfo(ECHO, L"wzctool -q XWIFI11B1 [with adapter-name arg]");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            LogInfo(ECHO, L"cannot find WIFI card");

            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }
        LogInfo(ECHO, L"found active wifi = %s", g_szWiFiCard1);
        LogInfo(ECHO, L"now querying %s", g_szWiFiCard1);

        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            LogInfo(FAIL, L"-q option error");
        else
            LogInfo(ECHO, L"-q option ok");
        break;

    case 103:   // L"wzctool -q XSWQAZ1"   XSWQAZ1 is non-existing wifi card
        LogInfo(ECHO, L"wzctool -q XSWQAZ1 [with non-exising adapter-name arg]");
        wsprintf(szCmdLine1, L"wzctool -q %s", L"XSWQAZ1");
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            LogInfo(ECHO, L"correct error message (ok)");
        else
            LogInfo(FAIL, L"incorrect message.");
        break;

    case 104:
        LogInfo(ECHO, L"wzctool -q XWIFI11B1 with empty BSSID list");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            LogInfo(ECHO, L"cannot find WIFI card");
            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }

        if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        {
            LogInfo(ECHO, L"WARNING this test needs XWIFI11B1 [has %s]", g_szWiFiCard1);
            break;
        }

        LogInfo(ECHO, L"clearing test SSID set for XWIFI11B");
        ClearTestSsidSet_XWIFI11B();

        LogInfo(ECHO, L"found active wifi = %s", g_szWiFiCard1);
        LogInfo(ECHO, L"now querying %s", g_szWiFiCard1);
        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            LogInfo(FAIL, L"-q option error");
        else
            LogInfo(ECHO, L"-q option ok");

        // following SSIDs should be visible
        sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(
            wcsstr(sz1, L"CEOPEN") ||
            wcsstr(sz1, L"CE-WEP40") ||
            wcsstr(sz1, L"CE-WEP104") ||
            wcsstr(sz1, L"MSFTWLAN") ||
            wcsstr(sz1, L"CEADHOC1")
            )
            LogInfo(FAIL, L"visible-SSIDs should be empty");
        else
            LogInfo(ECHO, L"visible-SSID-list is correct");
        break;

    case 105:
        // test only with XWIFI11B1
        LogInfo(ECHO, L"wzctool -q XWIFI11B1 with test set BSSID list");
        GetWiFiCardName();
        if(!*g_szWiFiCard1)
        {
            LogInfo(ECHO, L"cannot find WIFI card");
            if(g_TryXWIFI11B)
            {
                CardInsert_XWIFI11B();
                GetWiFiCardName();
                if(!*g_szWiFiCard1)
                {
                    LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                    break;
                }
            }
        }

        if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        {
            LogInfo(ECHO, L"WARNING this test needs XWIFI11B1");
            break;
        }

        LogInfo(ECHO, L"preparing test-SSID-set for XWIFI11B");
        AddTestSsidSet_XWIFI11B();

        LogInfo(ECHO, L"found active wifi = %s", g_szWiFiCard1);
        LogInfo(ECHO, L"now querying %s", g_szWiFiCard1);
        wsprintf(szCmdLine1, L"wzctool -q %s", g_szWiFiCard1);
        RunCommand(szCmdLine1);

        if(wcsstr(g_szCommandOutput, L"error"))
            LogInfo(FAIL, L"-q option error");
        else
            LogInfo(ECHO, L"-q option ok");

        // following SSIDs should be visible
        sz1 = wcsstr(g_szCommandOutput, L"[Available Networks]");
        if(
            wcsstr(sz1, L"CEOPEN") &&
            wcsstr(sz1, L"CE-WEP40") &&
            wcsstr(sz1, L"CE-WEP104") &&
            wcsstr(sz1, L"MSFTWLAN") &&
            wcsstr(sz1, L"CEADHOC1")
            )
            LogInfo(ECHO, L"visible-SSID-list is correct");
        else
            LogInfo(FAIL, L"visible-SSIDs should be empty");
        break;
    }

    DumpTextBufferToTuxEcho(g_szCommandOutput);

	return GetCode();
}




TESTPROCAPI
TestProc_wzctool_connect
// test set
/*
[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CEOPEN]
	"MacAddress"="00:40:00:00:00:00"
	"Privacy"=dword:0
	"Rssi"=dword:79
	"InfrastructureMode"=dword:1
	"DHCP"="100.100.*.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-WEP40]
	"MacAddress"="00:40:00:00:00:11"
	"InfrastructureMode"=dword:1
	"AuthMode"=dword:0
	"Privacy"=dword:1
	"key-index"=dword:1
	"key"="0x1234567890"
	"DHCP"="100.100.*.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CE-WEP104]
	"MacAddress"="00:40:00:00:00:22"
	"InfrastructureMode"=dword:1
	"AuthMode"=dword:0
	"Privacy"=dword:1
	"key-index"=dword:1
	"key"="qwertyuiopasd"
	"DHCP"="100.101.*.*"

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\MSFTWLAN]
	"MacAddress"="00:40:00:00:00:33"
	"Privacy"=dword:1
	"Rssi"=dword:100
	"InfrastructureMode"=dword:1

[HKEY_LOCAL_MACHINE\Comm\XWIFI11B-BSSID-LIST\CEADHOC1]
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
        LogInfo(ECHO, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
    }

    if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        LogInfo(ECHO, L"WARNING this test needs XWIFI11B1 [has %s]", g_szWiFiCard1);

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(L"XWIFI11B1");
    LogInfo(ECHO, L"WiFi card reset done. ([Preferred List] is cleared)");
    Sleep(1000);

    LogInfo(ECHO, L"preparing test-SSID-set for XWIFI11B");
    AddTestSsidSet_XWIFI11B();

    WCHAR szCmdLine1[128];
    WCHAR *szIpAddrForm_NULL = L"0.0.0.0";  // zero IP when connection fails
    WCHAR *szIpAddrForm_DHCP = L"100.100."; // when correctly connected and got DHCP address
    WCHAR *szIpAddrForm_DHCP_100_101 = L"100.101."; // when correctly connected and got DHCP address from CE-WEP104
    WCHAR *szIpAddrForm_AUTO = L"169.254."; // when connected but failed getting DHCP offer
    WCHAR *szExpectedIpAddrForm = szIpAddrForm_NULL;

    switch(lpFTE->dwUserData)
    {
    case 100:   // CEOPEN
        LogInfo(ECHO, L"wzctool -c -ssid CEOPEN  [with all default opton]");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CEOPEN");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP;
        break;

    case 110:   // CE-WEP40
        LogInfo(ECHO, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 1/0x1234567890");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 1/0x1234567890");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP;
        break;

    case 111:   // CE-WEP40 with incorrect WEP key index
        LogInfo(ECHO, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 2/0x1234567890");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 2/0x1234567890");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 112:   // CE-WEP40 with incorrect WEP key value
        LogInfo(ECHO, L"wzctool -c -ssid CE-WEP40 -auth open -encry wep key 1/0x0987654321");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40 -auth open -encr wep -key 1/0x0987654321");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 113:   // CE-WEP40 without WEP-encryption
        LogInfo(ECHO, L"wzctool -c -ssid CE-WEP40");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP40");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 120:   // CE-WEP104
        LogInfo(ECHO, L"wzctool -c -ssid CE-WEP104 -auth open -encry wep key 1/qwertyuiopasd");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-WEP104 -auth open -encr wep -key 1/qwertyuiopasd");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_DHCP_100_101;
        break;

    case 150:   // CE-ADHOC1
        LogInfo(ECHO, L"wzctool -c -ssid CE-ADHOC1 -adhoc");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC1 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 151:   // CE-ADHOC1 existing adhoc, but different combination
        LogInfo(ECHO, L"wzctool -c -ssid CE-ADHOC1 -encry wep key 1/0x1234567890 -adhoc");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC1 -encry wep key 1/0x1234567890 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;

    case 152:   // CE-ADHOC2 non-existing adhoc, so my WIFI-card should start this adhoc net
        LogInfo(ECHO, L"wzctool -c -ssid CE-ADHOC2 -adhoc");
        wsprintf(szCmdLine1, L"wzctool -c -ssid CE-ADHOC2 -adhoc");
        RunCommand(szCmdLine1);
        szExpectedIpAddrForm = szIpAddrForm_AUTO;
        break;
    }
    DumpTextBufferToTuxEcho(g_szCommandOutput);
    LogInfo(ECHO, L"waiting for XWIFI11B1 gets an address");
    WaitForGainingConnection(L"XWIFI11B1");

    RunCommand(L"ipconfig");
    DumpTextBufferToTuxEcho(g_szCommandOutput);
    WCHAR *sz1 = wcsstr(g_szCommandOutput, L"[XWIFI11B1]");
    if(!sz1)
        LogInfo(FAIL, L"no adapter [XWIFI11B1] found");
    else
    {
        for(sz1 = wcsstr(sz1, L"IP Address"); *sz1 && !iswdigit(*sz1); sz1++) ;
        WCHAR *szIpAddr = sz1;
        for( ; *sz1 && (iswdigit(*sz1)||(*sz1==L'.')); sz1++) ;
        *sz1 = L'\0';
        LogInfo(ECHO, L"IP Address = %s", szIpAddr);
        if(wcscmp(sz1, L"0.0.0.0")==0)
            LogInfo(FAIL, L"WIFI1 has IP address 0.0.0.0");
        else
        {
            if(wcsncmp(szIpAddr, szExpectedIpAddrForm, wcslen(szExpectedIpAddrForm))==0)
                LogInfo(ECHO, L"expected %s, current IP =%s (pass)", szExpectedIpAddrForm, szIpAddr);
            else
                LogInfo(FAIL, L"expected %s, current IP =%s (FAIL)", szExpectedIpAddrForm, szIpAddr);
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
        LogInfo(ECHO, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
    }

    if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        LogInfo(ECHO, L"WARNING this test needs XWIFI11B1 [has %s]", g_szWiFiCard1);

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(L"XWIFI11B1");
    DumpTextBufferToTuxEcho(g_szCommandOutput);
    Sleep(1000);
	return GetCode();
}





WCHAR *g_szReg_CEOPEN[] =
{
    L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL]",
	L"    \"ssid\"=\"CEOPEN\"",
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
        LogInfo(ECHO, L"cannot find WIFI card");
        if(g_TryXWIFI11B)
        {
            CardInsert_XWIFI11B();
            GetWiFiCardName();
            if(!*g_szWiFiCard1)
            {
                LogInfo(ECHO, L"tried to install XWIFI11B, but this also failed. So, skipping test");
                return GetCode();
            }
        }
    }

    if(wcscmp(g_szWiFiCard1, L"XWIFI11B1"))
        LogInfo(ECHO, L"WARNING this test needs XWIFI11B1 [has %s]", g_szWiFiCard1);

    RunCommand(L"wzctool -reset");
    WaitForLoosingConnection(L"XWIFI11B1");
    LogInfo(ECHO, L"WiFi card reset done. ([Preferred List] is cleared)");
    Sleep(1000);

    LogInfo(ECHO, L"preparing test-SSID-set for XWIFI11B");
    AddTestSsidSet_XWIFI11B();

    if(RegDeleteKey(HKEY_CURRENT_USER, L"Comm\\WZCTOOL") == ERROR_SUCCESS)
        LogInfo(ECHO, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deleted");
    else
        LogInfo(ECHO, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deletion failed (this key might not exist)");
    BOOL bShouldHaveIpAddress = TRUE;

    switch(lpFTE->dwUserData)
    {
    case 100:   // "without registry entry", this should have no connection at all
        LogInfo(ECHO, L"wzctool -registry (with empty set)");
        RunCommand(L"wzctool -registry");
        Sleep(10000);   // wait 10 sec, then check IP address
        bShouldHaveIpAddress = FALSE;

    case 101:
        SaveToRegistry(g_szReg_CEOPEN);
        LogInfo(ECHO, L"wzctool -registry (for CEOPEN)");
        RunCommand(L"wzctool -registry");
        break;

    case 102:
        SaveToRegistry(g_szReg_CE_WEP40);
        LogInfo(ECHO, L"wzctool -registry (for CE_WEP40)");
        RunCommand(L"wzctool -registry");
        break;

    case 103:
        SaveToRegistry(g_szReg_CE_WEP40);
        LogInfo(ECHO, L"wzctool (without any argument, with registry set for CE_WEP40)");
        RunCommand(L"wzctool");
        break;
    }

    DumpTextBufferToTuxEcho(g_szCommandOutput);

    if(bShouldHaveIpAddress)
    {
        WaitForGainingConnection(L"XWIFI11B1");
        Sleep(1000);
        RunCommand(L"ipconfig");
        DumpTextBufferToTuxEcho(g_szCommandOutput);
        WCHAR *sz1 = wcsstr(g_szCommandOutput, L"[XWIFI11B1]");
        if(!sz1)
            LogInfo(FAIL, L"no adapter [XWIFI11B1] found");
        else
        {
            for(sz1 = wcsstr(sz1, L"IP Address"); *sz1 && !iswdigit(*sz1); sz1++) ;
            WCHAR *szIpAddr = sz1;
            for( ; *sz1 && (iswdigit(*sz1)||(*sz1==L'.')); sz1++) ;
            *sz1 = L'\0';
            LogInfo(ECHO, L"IP Address = %s", szIpAddr);
            if(wcscmp(sz1, L"0.0.0.0")==0)
                LogInfo(FAIL, L"WIFI1 has IP address 0.0.0.0");
        }
    }
    else
    {
        if(AdapterHasIpAddress(L"XWIFI11B1"))
            LogInfo(FAIL, L"adapter XWIFI11B1 should not have an IP address, but it has");
        else
            LogInfo(ECHO, L"XWIFI11B1 has no change (ok)");
    }

    // remove the wzctool-preconfig registry
    if(RegDeleteKey(HKEY_CURRENT_USER, L"Comm\\WZCTOOL") == ERROR_SUCCESS)
        LogInfo(ECHO, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deleted");
    else
        LogInfo(ECHO, L"[HKEY_CURRENT_USER\\Comm\\WZCTOOL] deletion failed (this key might not exist)");
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

    { L"wzctool -c -ssid CEOPEN",    1, 100, 400, TestProc_wzctool_connect },
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
