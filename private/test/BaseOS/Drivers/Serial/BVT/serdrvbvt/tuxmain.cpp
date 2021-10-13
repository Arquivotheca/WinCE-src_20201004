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

#include "TUXMAIN.H"
#include "TESTPROC.H"
#include "UTIL.H"
#include "ceddk.h"
#include "regmani.h"      // for CRegMani
    
// extern "C"
#ifdef __cplusplus
extern "C" {
#endif
// debugging
#define Debug NKDbgPrintfW

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

//Global Variable that Identifies  the COMM IR Port 
INT irCOMx = -1;

//COMx Mask which determines which ports are valid 
INT COMMask[10]={0};

//global variable for a specific COM Port 
INT COMx   = -1;

/* ------------------------------------------------------------------------
    Global Variables
------------------------------------------------------------------------ */
TCHAR       g_lpszCommPort[128];

// --------------------------------------------------------------------
// Tux testproc function table
//

FUNCTION_TABLE_ENTRY g_lpFTE[] = 
{
    TEXT("Serial Port Driver BVT"                                                 ),      0,      0,                0,  NULL,
    TEXT( "Send bytes at all supported bauds, data-bits, parity and stop-bits"    ),      1,      0,       BVT_BASE+1,  Tst_WritePort,
    TEXT( "Verify SetCommEvent() and GetCommEvent() operation"                    ),      1,      0,       BVT_BASE+2,  Tst_SetCommEvents,
    TEXT( "Test EscapeCommFunction()"                                             ),      1,      0,       BVT_BASE+3,  Tst_EscapeCommFunction,
    TEXT( "WaitCommEvent(EV_TXEMPTY) and send data"                               ),      1,      0,       BVT_BASE+4,  Tst_EventTxEmpty,
    TEXT( "SetCommBreak() and ClearCommBreak()"                                   ),      1,      0,       BVT_BASE+5,  Tst_CommBreak,
    TEXT( "Set event mask and wait for thread to clear it"                        ),      1,      0,       BVT_BASE+6,  Tst_WaitCommEvent,
    TEXT( "Set event mask and wait for thread to close comm port handle"          ),      1,      0,       BVT_BASE+7,  Tst_WaitCommEventAndClose,
    TEXT( "Set and verify receive timeout"                                        ),      1,      0,       BVT_BASE+8,  Tst_RecvTimeout,
    TEXT( "Verify SetCommState() with bad DCB fails and keeps prev DCB settings"  ),      1,      0,       BVT_BASE+9,  Tst_SetBadCommDCB,
    TEXT( "Verify Open/Close on port share"                                       ),      1,      0,      BVT_BASE+10,  Tst_OpenCloseShare,
    TEXT( "Verify Power Management on serial ports"                               ),      1,      0,      BVT_BASE+11,  Tst_PowerCaps,
    NULL,   0,  0,  0,  NULL
};

// --------------------------------------------------------------------
// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);    
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif


//The Help Display when the test runner types a wrong command Line 
void DisplayHelp()
{
    Debug(TEXT("Command line Usage :\n"));
    Debug(TEXT("Option -u to run  the test against all USB Serial Ports:\n"));
    Debug(TEXT("Option -i to run  the test against all IR Serial Ports \n"));
    Debug(TEXT("Option -p Followed by the Port name COMx: to specify the COM port to run against"));
}

static BOOL ProcessCmdLine( LPCTSTR cmdline)
{
    BOOL retval=FALSE;

    //TCHAR COMSEP = (TCHAR)':';
    while((TEXT('\0') != *cmdline))
    {
        while((CmdSpace == *cmdline)||(TEXT('\t') == *cmdline)) //skip spaces
               cmdline ++;

        if('\0' == *cmdline)
               continue;
        
        cmdline++;
    
        switch(*cmdline)
        {
              case CmdUsb:
                 retval = Util_QueryUSBSerialPort();
                if(FALSE == retval)
                {
                    g_pKato->Log( LOG_FAIL, TEXT("FAIL : USB Serial Port Not found \n"));
                }
                cmdline++;
                break;
                  
            case CmdPort:
                 cmdline++;
                 retval = Util_SetSerialPort(&cmdline,g_lpszCommPort);
                 if(FALSE == retval)
                 {
                     g_pKato->Log( LOG_FAIL, TEXT("FAIL : Setting Serial Port Failed \n"));
                 }
                 break;

            case CmdIR:
                 LOG(TEXT("**Find IR Serial port**"));
                 retval = Util_QueryCommIRRAWPort();
                 {
                    g_pKato->Log( LOG_FAIL, TEXT("FAIL : RAW IR port Not Found \n"));
                 }
                 cmdline++;
                 break;

            default:
                 DisplayHelp();
                 retval=FALSE;
                 return retval;
        }
                     
    }
    return retval;
}



// --------------------------------------------------------------------
void LOG(LPCWSTR szFmt, ...)
// --------------------------------------------------------------------
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
}

// --------------------------------------------------------------------
// This function should be remove by M4 - temporary add registry to enable device manager ACL.
// --------------------------------------------------------------------
BOOL ModifySecurityRegistry(BOOL bEnableSecurity)
{

    RegManipulate   RegMani(HKEY_LOCAL_MACHINE);
    //check if this key already exists, if so, return
    if(RegMani.IsAKeyValidate(DEVLOAD_DRIVERS_KEY) == FALSE){
        NKMSG(L"Key %s does not exist!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }
    BOOL bRet = TRUE;

// These are generic flags sitting in HKLM\Drivers - this will be remove afterward when the
// second check in in place 

    DWORD dwCurrentFlag = bEnableSecurity ? 1 : 0;
    
    bRet = RegMani.SetAValue(DEVLOAD_DRIVERS_KEY, 
                                        L"EnforceDevMgrSecurity", 
                                        REG_DWORD, 
                                        sizeof(DWORD), 
                                        (PBYTE)&dwCurrentFlag); 

    if(bRet == FALSE){
        NKMSG(L"Can not set EnforcedDevMgrSecurity on %s!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }           

    bRet = RegMani.SetAValue(DEVLOAD_DRIVERS_KEY, 
                                        L"udevicesecurity", 
                                        REG_DWORD, 
                                        sizeof(DWORD), 
                                        (PBYTE)&dwCurrentFlag); 

    if(bRet == FALSE){
        NKMSG(L"Can not set udevicesecurity on %s!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }      


    return bRet;
}

// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = NULL;
    LPSPS_END_TEST pET = NULL;
    LPTSTR lpszCmdLine = NULL;
    BOOL fRtn = TRUE;

    //To stop PreFast: local veriable could be const warnning 
    pBT = (pBT) ? NULL : pBT;
    pET = (pET) ? NULL : pET;

    switch (uMsg) {
    
        // --------------------------------------------------------------------
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

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            if (!ModifySecurityRegistry(FALSE))
                return SPR_FAIL;            

            return SPR_HANDLED;      

        // --------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
      
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            //Call the Intialization function which checks the serial Port Configuration 
            Debug(_T("Serial Port Config Called \r\n"));
              Util_SerialPortConfig();
            
            // handle command line parsing
            if( g_pShellInfo->szDllCmdLine && g_pShellInfo->szDllCmdLine[0] )
            {
                 DWORD dwSize = _tcslen(g_pShellInfo->szDllCmdLine)+1;
                 lpszCmdLine = new TCHAR[dwSize];
                 if( NULL == lpszCmdLine )
                 {
                          g_pKato->Log( LOG_FAIL, TEXT("FAIL in : Couldn't set test options"));
                          return SPR_FAIL;
            
                 } // if( NULL == lpszCmdLine )
            
                      _tcscpy_s( lpszCmdLine, dwSize, g_pShellInfo->szDllCmdLine );
            
                      fRtn =     ProcessCmdLine(lpszCmdLine);
                      delete[] lpszCmdLine;
                  }
            if( !fRtn )
            {
                g_pKato->Log( LOG_FAIL, TEXT("FAIL : Invalid Command Line Arguments\n"));
                return SPR_FAIL;
            
            } // end if( 

            //Enable security checks in Device Manager
            if (!ModifySecurityRegistry(TRUE))
                return SPR_FAIL;            

        return SPR_HANDLED;

        // --------------------------------------------------------------------
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
            
        // --------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));                       
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: SERDRVBVT.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: SERDRVBVT.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
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

        // --------------------------------------------------------------------
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

        // --------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;
        
        // --------------------------------------------------------------------
        // Message: SPM_SETUP_GROUP
        //
        case SPM_SETUP_GROUP:
            Debug(_T("ShellProc(SPM_SETUP_GROUP, ...) called\r\n"));
            return SPR_HANDLED;
            
        // --------------------------------------------------------------------
        // Message: SPM_TEARDOWN_GROUP
        //
        case SPM_TEARDOWN_GROUP:
            Debug(_T("ShellProc(SPM_TEARDOWN_GROUP, ...) called\r\n"));
            return SPR_HANDLED;
            
        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}
