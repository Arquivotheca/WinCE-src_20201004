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

#include "fakerilbvt_tuxmain.h"
#include "fakerilbvt_testproc.h"

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

    { _T("Booted OK?"),                     1,  0,     1, Test_Booted_OK, },

    { _T("Phone App exist?"),               1,  0x10,  2, Test_Phone_App, },   // 0x10=check phone app from registry
    { _T("Phone call?"),                    1,  0x11,  3, Test_Phone_App, },   // 0x11 = Can call through the phone?

    { _T("Phone connected?"),               1,  0x10,  4, Test_Phone_UI, },   // 0x12 = Connected? UI

    { _T("Network media connected state?"), 1,  0,     5, Test_Net_Media, },
    { _T("Bound to TCPIP?"),                1,  0,     6, Test_Net_ndisconfig, },

    { _T("Got IP address?"),                1,  0x10,  7, Test_Net_ipconfig, }, // 0x10 = ip addr
    { _T("Sub netmask?"),                   1,  0x11,  8, Test_Net_ipconfig, }, // 0x11 = subnet mask
    { _T("Got default gateway?"),           1,  0x12,  9, Test_Net_ipconfig, }, // 0x12 = default gateway

    { _T("Can ping 'gadget'?"),             1,  0x10, 100, Test_Net_ping, },  // 0x10 = ping 'gadget'
    { _T("Can ping 'gadget' 256 byte?"),    1,  0x11, 101, Test_Net_ping, },  // 0x11 = ping 'gadget' 256 byte
    { _T("Can ping 'gadget' 1k byte?"),     1,  0x12, 102, Test_Net_ping, },  // 0x12 = ping 'gadget' 1k byte
    { _T("Can ping 'gadget' 4k byte?"),     1,  0x13, 103, Test_Net_ping, },  // 0x13 = ping 'gadget' 4k byte
    { _T("Can ping 'gadget' 8k byte?"),     1,  0x14, 104, Test_Net_ping, },  // 0x14 = ping 'gadget' 8k byte
    { _T("Can ping 'gadget' 16k byte?"),    1,  0x15, 105, Test_Net_ping, },  // 0x15 = ping 'gadget' 16k byte
    { _T("Can ping 'aqzxcvarfgv'? [fail]"), 1,  0x26, 106, Test_Net_ping, },  // 0x26 = ping 'aqzxcvarfgv' [fail]

    { _T("Can ping 'cefaq'?"),              1,  0x17, 200, Test_Net_ping, },  // 0x17 = ping 'cefaq'
    { _T("http-ping http://cefaq"),         1,  0x10, 201, Test_Net_Http, },  // 0x10 = ping 'http://cefaq'
    { _T("http-ping http://cefaq"),         1,  0x11, 202, Test_Net_Http, },  // 0x11 = get 'http://cefaq'

    { _T("Phone disconnect?"),              1,  0x20, 300, Test_Phone_App, },   // 0x20=disconnect
    { _T("Got 0.0.0.0 IP address?"),        1,  0x21, 301, Test_Net_ipconfig, }, // 0x21 = ip addr should be 0.0.0.0

    { _T("Phone connected?"),               1,  0x21, 302, Test_Phone_UI, },   // 0x21 = disconnected? UI

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
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: FAKERIL-3G-NET TEST"));
            
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: FAKERIL-3G-NET TEST"));
            
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


void ShowUsage()
{
    g_pKato->Log(LOG_COMMENT, _T("FAKERIL-3G-NET-BVT: Usage: tux -o -d fakerilbvt -x #test"));
}
