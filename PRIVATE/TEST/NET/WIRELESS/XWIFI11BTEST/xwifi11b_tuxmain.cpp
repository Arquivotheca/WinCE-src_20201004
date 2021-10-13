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

#include "xwifi11b_tuxmain.h"
#include "xwifi11b_testproc.h"

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;


// Tux testproc function table
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    /*
   LPCTSTR  lpDescription; // description of test
   UINT     uDepth;        // depth of item in tree hierarchy
   DWORD    dwUserData;    // user defined data that will be passed to TestProc at runtime
   DWORD    dwUniqueID;    // uniquely identifies the test - used in loading/saving scripts
   TESTPROC lpTestProc;    // pointer to TestProc function to be called for this test
    */

    { _T("Test1"),     1, 33,                           9, Test1, },

    // 10 series = set up for test
    { _T("WiFi card detection by WZC"),     1, 0,       10, Test_CheckWiFiCard },
    { _T("reset preferred list"), 0, 0,                 11, Test_ResetPreferredList },
    { _T("Check WiFi UI pop-up when no preferred SSID (bind/unbind"), 1, 0, 20, Test_CheckUiWhenNoPreferredSsid },
    // 10 check if XWIFI11B1 is appeared as network adapter.
    // 11 clear all SSID
    // 12 add 10 SSID
    // 13 add 1 SSID
    // 14 remove 1 SSID

    // 100 series = repeat 100 times

    { _T("activate/deactivate card 100"), 1, 0,         100, Test_ActivateDeactivate100 },
    { _T("connect-to/disconnect-from SSID 100"), 1, 0,  101, Test_ConnectDisconnect100 },

    // repeat bind/unbind
    // wait for complete IP gain

    // repeat add/remove a non-preferred ssid
    // repeat add/remove 20 non-preferred ssids
    // repeat add/remove the preferred ssid

    // adapter power on/off
    // system power on/off
    // system power critical/normal


    // do the samething with the static IP
    /*
    static ip
    dhcp
    auto ip
    */

    // adhoc net tests
    { _T("XWIFI11B adhoc"), 1, 0,                       200, Test_Adhoc_Ssid, },

    /*
    starting an adhoc net
        short name
        max name
        strange name (random combination of max-32-char printable string)
    add multiple adhoc net, xwifi11b should connect most recent one
        add upto 100 adhoc nets

    authentication:open/shared
    encryption:no/wep-40/wep-104

    repeatedly add/remove adhoc net
    when configured correctly, wait for adhoc starts, then repeatedly
        insert/eject
        bind/unbind
        adapter power on/off
        , each time it should connect immediately to the adhoc net
    */

    NULL, 0, 0, 0, NULL
};




// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
    UNREFERENCED_PARAMETER(lpReserved);
    
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);
            Debug(TEXT("%s: Disabiling thread library calls\r\n"), MODULE_NAME);
			DisableThreadLibraryCalls((HMODULE)hInstance); // not interested in those here
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}


void ShowUsage();

// --------------------------------------------------------------------
void LOG(LPWSTR szFmt, ...)
// --------------------------------------------------------------------
{
#ifdef DEBUG
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
#endif
}


// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {
    
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL: 
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();
            return SPR_HANDLED;        

        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            return SPR_HANDLED;      

        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
      
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            // handle command line parsing

            if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
            {
                Debug(TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
                if(wcscmp(g_pShellInfo->szDllCmdLine, L"-?")==0)
                {
                    ShowUsage();
                    return SPR_FAIL;
                }
            }
            return SPR_HANDLED;

        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
            
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
            
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));                       

            return SPR_HANDLED;

        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            
            return SPR_HANDLED;

        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: XWIFI11B TEST"));
            
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: XWIFI11B TEST"));
            
            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}


// --------------------------------------------------------------------
// Internal functions

extern TCHAR optArg[];

BOOL ProcessCmdLine( LPCTSTR szCmdLine )
{
/*
    // parse command line
    int option = 0;

	LPCTSTR szOpt = TEXT("l:p:n:s:x:d:");
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case TEXT('l'):
			g_pszLocalRecvQueuePath = new TCHAR[_tcslen(optArg) + 1];
			memcpy(g_pszLocalRecvQueuePath, optArg, sizeof(TCHAR) * (_tcslen(optArg) + 1));
            Debug(TEXT("Local recv queue: \"%s\""), g_pszLocalRecvQueuePath);
            break;

		case TEXT('p'):
			g_pszPeerRecvQueuePath = new TCHAR[_tcslen(optArg) + 1];
			memcpy(g_pszPeerRecvQueuePath, optArg, sizeof(TCHAR) * (_tcslen(optArg) + 1));
            Debug(TEXT("Peer recv queue: \"%s\""), g_pszPeerRecvQueuePath);
			break;

		case TEXT('n'):
			g_nNumOfMessages = (UINT)_ttoi(optArg);
			if (g_nNumOfMessages == 0) {
				Debug(TEXT("Invalid number of messages. Number must be > 0"));
				return FALSE;
			}
			break;
		case TEXT('s'):
			// specifying addr of the server - so you're a client
			g_bIAmServer = FALSE;
            if(wcstombs(g_szServerAddressString,optArg,sizeof(g_szServerAddressString)) < 0) {
                g_pKato->Log(LOG_FAIL, TEXT("Invalid server address: %s"), optArg);
				return FALSE;
            }
            break;   

		case TEXT('x'):
			g_nServerPort = (USHORT)_ttoi(optArg);
			if (g_nServerPort == 0) {
				Debug(TEXT("Invalid port number. Port must be > 0."));
				return FALSE;
			}
			break;

        case TEXT('d'):
            g_nQFullSendDelay = _ttoi(optArg);
            break;

        default:
            // bad parameters
            Debug(TEXT("Unknown option '%c'"), option);
            return FALSE;
        }
    }
*/
    return TRUE;
}


LPCTSTR g_szTestId[] =
{
    _T("100 = install/uninstall xwifi11b [100 times]"),
    NULL
};

void ShowUsage()
{
    g_pKato->Log(LOG_COMMENT, _T("XWIFI11B: Usage: tux -o -d xwifi11btest -x #test -c\"-? -x ###\""));
    for(unsigned i=0; g_szTestId[i]; i++)
        g_pKato->Log(LOG_COMMENT, g_szTestId[i]);
}
