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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


#include "mq_cetk_tuxmain.h"
#include "mq_cetk_testproc.h"

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;


// Tux testproc function table

#if 0
typedef struct _FUNCTION_TABLE_ENTRY {
   LPCTSTR  lpDescription; // description of test
   UINT     uDepth;        // depth of item in tree hierarchy
   DWORD    dwUserData;    // user defined data that will be passed to TestProc at runtime
   DWORD    dwUniqueID;    // uniquely identifies the test - used in loading/saving scripts
   TESTPROC lpTestProc;    // pointer to TestProc function to be called for this test
} FUNCTION_TABLE_ENTRY, *LPFUNCTION_TABLE_ENTRY;
#endif

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    _T("MSMQ Create-Delete Queues"),     1, 0x00010000,      1000, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00010001,      1001, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00020002,      1002, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00020004,      1003, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00040008,      1004, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00040010,      1005, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00080020,      1006, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00080040,      1007, Test_CreateDeleteQueue,

    _T("MSMQ Create-Delete Queues"),     1, 0x00010003,      1008, Test_CreateDeleteQueue,  //PROPID_Q_JOURNAL+PROPID_Q_JOURNAL_QUOTA
    _T("MSMQ Create-Delete Queues"),     1, 0x00010005,      1009, Test_CreateDeleteQueue,  //PROPID_Q_JOURNAL+PROPID_Q_LABEL
    _T("MSMQ Create-Delete Queues"),     1, 0x0001000D,      1010, Test_CreateDeleteQueue,  //PROPID_Q_JOURNAL+PROPID_Q_LABEL+PROPID_Q_QUOTA
    _T("MSMQ Create-Delete Queues"),     1, 0x00010060,      1011, Test_CreateDeleteQueue,  //PROPID_Q_TRANSACTION+PROPID_Q_PRIV_LEVEL
    _T("MSMQ Create-Delete Queues"),     1, 0x00010070,      1012, Test_CreateDeleteQueue,  //PROPID_Q_AUTHENTICATE+PROPID_Q_TRANSACTION+PROPID_Q_PRIV_LEVEL
    _T("MSMQ Create-Delete Queues"),     1, 0x00010050,      1013, Test_CreateDeleteQueue,  //PROPID_Q_AUTHENTICATE+PROPID_Q_TRANSACTION
    _T("MSMQ Create-Delete Queues"),     1, 0x00010063,      1014, Test_CreateDeleteQueue,  //PROPID_Q_TRANSACTION+PROPID_Q_PRIV_LEVEL+PROPID_Q_JOURNAL+PROPID_Q_JOURNAL_QUOTA
    _T("MSMQ Create-Delete Queues"),     1, 0x00010075,      1015, Test_CreateDeleteQueue,  //PROPID_Q_AUTHENTICATE+PROPID_Q_TRANSACTION+PROPID_Q_PRIV_LEVEL+PROPID_Q_JOURNAL+PROPID_Q_LABEL
    _T("MSMQ Create-Delete Queues"),     1, 0x0001005D,      1016, Test_CreateDeleteQueue,  //PROPID_Q_AUTHENTICATE+PROPID_Q_TRANSACTION+PROPID_Q_JOURNAL+PROPID_Q_LABEL+PROPID_Q_QUOTA
    _T("MSMQ Create-Delete Queues"),     1, 0x00010013,      1017, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x00010025,      1018, Test_CreateDeleteQueue,
    _T("MSMQ Create-Delete Queues"),     1, 0x0001004D,      1019, Test_CreateDeleteQueue,

    _T("MSMQ Send-Receive Messages"),    1, 0x00000000,      2000, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000001,      2001, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000002,      2002, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000003,      2003, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000004,      2004, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000005,      2005, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000006,      2006, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000007,      2007, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000008,      2008, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000009,      2009, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000A,      2010, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000B,      2011, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000C,      2012, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000D,      2013, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000E,      2014, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x0000000F,      2015, Test_SendReceiveMessage,

    _T("MSMQ Send-Receive Messages"),    1, 0x00000F10,      2016, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F11,      2017, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F12,      2018, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F13,      2019, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F14,      2020, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F15,      2021, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F16,      2022, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F17,      2023, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F18,      2024, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F19,      2025, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1A,      2026, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1B,      2027, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1C,      2028, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1D,      2029, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1E,      2030, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000F1F,      2031, Test_SendReceiveMessage,

    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF0,      2032, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF1,      2033, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF2,      2034, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF3,      2035, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF4,      2036, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF5,      2037, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF6,      2038, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF7,      2039, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF8,      2040, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FF9,      2041, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFA,      2042, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFB,      2043, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFC,      2044, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFD,      2045, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFE,      2046, Test_SendReceiveMessage,
    _T("MSMQ Send-Receive Messages"),    1, 0x00000FFF,      2047, Test_SendReceiveMessage,

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
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: MSMQ TEST"));
            
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: MSMQ TEST"));
            
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


LPCTSTR g_szTestId[] =
{
    L"MSMQ CETK test",
    L"preparation steps",
    L"1. set a unique hostname",
    L"2. msmqadm register binary srmp trust",
    L"3. msmqadm start",
    L"4. run tux -o -d mq_cetk",
    NULL
};

void ShowUsage()
{
    g_pKato->Log(LOG_COMMENT, _T("MSMQ_CETK: Usage: tux -o -d msmq_cetk -x ###"));
    for(unsigned i=0; g_szTestId[i]; i++)
        g_pKato->Log(LOG_COMMENT, g_szTestId[i]);
}
