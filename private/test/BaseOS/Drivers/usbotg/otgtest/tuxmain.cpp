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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name: 
    Tuxmain.cpp

Abstract:  
    USB OTG Test tux routines 

Author:
    shivss


Functions:

Notes: 
--*/// --------------------------------------------------------------------

#include <WINDOWS.H>
#include <otgtest.h>
#include <clparse.h>
#include<tchar.h>


#define __THIS_FILE__ TEXT("OtgTest.cpp")

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;


OTGTest  g_OtgTestContext;



////////////////////////////////////////////////////////////////////////////////
// TUX Function table
extern "C"{
    
FUNCTION_TABLE_ENTRY g_lpFTE[] = { 
    {TEXT("USB OTG Test Suite"),            0,  0,    0, NULL},
    { TEXT("OTG API Test"),                 1,  0, 1001, OTG_OpenCloseTest } ,
    { TEXT("OTG Request Drop Test"),        1,  0, 1003, OTG_RequestDropTest } ,
    { TEXT("OTG Suspend Resume Test"),      1,  0, 1004, OTG_SuspedResumeTest } ,
    { TEXT("OTG Power Management Test"),    1,  0, 1005, OTG_PowerManagementTest } ,
    { TEXT("OTG Load Unload Test"),         1,  0, 1002, OTG_LoadUnloadTest } ,
    { NULL, 0, 0, 0, NULL } 
};
}




////////////////////////////////////////////////////////////////////////////////
// DeviceDx2String
//  Convert a bitmask Device Dx specification into a string containing D0, D1,
//  D2, D3, and D4 values as specified by the bitmask.
//
// Parameters:
//  deviceDx        Device Dx  itmask specifiying D0-D4 support.
//  szDeviceDx      Output string buffer.
//  cchDeviceDx     Length of szDeviceDx buffer, in WCHARs.
//
// Return value:
//  Pointer to the input buffer szDeviceDx. If the Device Dx bitmask specifies
//  no supported DXs, the returned string will be empty.

LPWSTR DeviceDx2String(const UCHAR deviceDx, LPWSTR szDeviceDx, const DWORD cchDeviceDx)
{
    // start the string off empty (in case no DX are supported
    VERIFY(SUCCEEDED(StringCchCopy(szDeviceDx, cchDeviceDx, L"")));
    if(SupportedDx(D0, deviceDx))
    {
        // device supportds D0
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L"D0")));
    }
    if(SupportedDx(D1, deviceDx))
    {
        // device supportds D1
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D1")));
    }

    if(SupportedDx(D2, deviceDx))
    {
        // device supportds D2
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D2")));
    }

    if(SupportedDx(D3, deviceDx))
    {
        // device supportds D3
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D3")));
    }

    if(SupportedDx(D4, deviceDx))
    {
        // device supportds D4
        VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D4")));
    }

    // return a pointer to the input buffer so the function call can be embedded
    // into other string functions
    return szDeviceDx;
}



////////////////////////////////////////////////////////////////////////////////
// Util_LogPowerCapabilities
//  Log contents of POWER_CAPABILITIES structure (see MSDN).
//
// Parameters:
//  ppwrCaps        pointer to POWER_CAPABILITIES structure to be logged.
//
// Return value:
//  None

void LogPowerCapabilities(const POWER_CAPABILITIES *ppwrCaps)
{
    CEDEVICE_POWER_STATE dx;
    WCHAR szDeviceDx[32];
    if(ppwrCaps)
    {
         g_pKato->Log(LOG_DETAIL,_T("POWER_CAPABILITIES"));
         g_pKato->Log(LOG_DETAIL,_T(" DeviceDx:    %s"), DeviceDx2String(ppwrCaps->DeviceDx, szDeviceDx, 32));
         g_pKato->Log(LOG_DETAIL,_T(" WakeFromDx:  %s"), DeviceDx2String(ppwrCaps->WakeFromDx, szDeviceDx, 32));
         g_pKato->Log(LOG_DETAIL,_T("  InrushDx:    %s"), DeviceDx2String(ppwrCaps->InrushDx, szDeviceDx, 32));
         
        for(dx = D0; dx < PwrDeviceMaximum; 
            dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
        {
            // log max power consumption for supported DXs
            if(SupportedDx(dx, ppwrCaps->DeviceDx))
            {
                 g_pKato->Log(LOG_DETAIL,_T("  Power D%u:    %u mW"), dx, ppwrCaps->Power[dx]);
            }
        }

        for(dx = D0; dx < PwrDeviceMaximum; 
            dx = (CEDEVICE_POWER_STATE)((DWORD)dx + 1))
        {
            // log max latency for supported DXs
            if(SupportedDx(dx, ppwrCaps->DeviceDx))
            {
                 g_pKato->Log(LOG_DETAIL,_T("  Latency D%u:  %u mW"), dx, ppwrCaps->Latency[dx]);
            }
        }

        // currently, the only flag is POWER_CAP_PARENT
         g_pKato->Log(LOG_DETAIL,_T("  Flags:       0x%08x"), ppwrCaps->Flags);
    }
}


////////////////////////////////////////////////////////////////////////////////
//QueryOTGport
//
//Returns the number of OTG Ports If not specified in the command line 
// 
// 
//  
// Return value:
// DWORD 
DWORD QueryOTGPortCount()
{
     const OTGTest *pContext = &g_OtgTestContext;
     return  pContext->GetOTGCount();
}



////////////////////////////////////////////////////////////////////////////////
//GetOTGportInfo
//
//Returns the DEVMGR_DEVICE_INFORMATION of the OTG Port 
// 
//Parameter
//OTG Port Handle ,Pointer to DEVMGR_DEVICE_INFORMATION 
//  
// Return value:
// DWORD 
BOOL  GetOTGportInfo(const HANDLE hFile ,DEVMGR_DEVICE_INFORMATION* pddi  )
{
       BOOL bRet = TRUE;

       
     if(!GetDeviceInformationByFileHandle(hFile, pddi))
     {
            g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d: GetDeviceInformationByFileHandle failed "),
                                  __THIS_FILE__, __LINE__ );
         bRet = FALSE;
      }
    
     return bRet;
}

////////////////////////////////////////////////////////////////////////////////
//GetOTGportIndex
//Convert Device Name to Index 
//
// 
//Parameter
//Pointer to DEVMGR_DEVICE_INFORMATION 
//  
// Return value:
// Returns the Index of the OTG  Port


DWORD  GetOTGportIndex(const  TCHAR* lpszDeviceName )
{
    if(!lpszDeviceName)
        return (DWORD)-1;

    return(*(lpszDeviceName+3) -'0');
}

////////////////////////////////////////////////////////////////////////////////
//InitOTGTest
//Initialize OTG Test  
// 
// 
//  
// Return value:
// BOOL 
BOOL InitOTGTest()
{
     OTGTest * pContext  = &g_OtgTestContext;
      return ( pContext->InitializeOTGTest() && pContext->InitializeClientDrivers());
}

////////////////////////////////////////////////////////////////////////////////
// InitializeCmdLine
// Process Command Line.
//
// Parameters:
//  pszCmdLine         Command Line String .
//  
// Return value:
// VOID

void InitializeCmdLine(LPCTSTR /* pszCmdLine */) 
{
     return;
}



////////////////////////////////////////////////////////////////////////////////
// ShellProc
//  Processes messages from the TUX shell.
//
// Parameters:
//  uMsg            Message code.
//  spParam         Additional message-dependent data.
//
// Return value:
//  Depends on the message.

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST pBT ;
    LPSPS_END_TEST pET ;
    
    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // Sent once to the DLL immediately after it is loaded. The spParam
        // parameter will contain a pointer to a SPS_LOAD_DLL structure. The
        // DLL should set the fUnicode member of this structure to TRUE if the
        // DLL is built with the UNICODE flag set. If you set this flag, Tux
        // will ensure that all strings passed to your DLL will be in UNICODE
        // format, and all strings within your function table will be processed
        // by Tux as UNICODE. The DLL may return SPR_FAIL to prevent the DLL
        // from continuing to load.
        NKDbgPrintfW(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
        g_pKato = (CKato*)KatoGetDefaultObject();

        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        NKDbgPrintfW(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        break;

    case SPM_SHELL_INFO:
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        NKDbgPrintfW(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info for later use.
                // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        if(g_pShellInfo->szDllCmdLine)
        {
            InitializeCmdLine(g_pShellInfo->szDllCmdLine);
        }
 

        break;
       
    case SPM_REGISTER:
        // This is the only ShellProc() message that a DLL is required to
        // handle (except for SPM_LOAD_DLL if you are UNICODE). This message is
        // sent once to the DLL immediately after the SPM_SHELL_INFO message to
        // query the DLL for its function table. The spParam will contain a
        // pointer to a SPS_REGISTER structure. The DLL should store its
        // function table in the lpFunctionTable member of the SPS_REGISTER
        // structure. The DLL may return SPR_FAIL to prevent the DLL from
        // continuing to load.
        NKDbgPrintfW(TEXT("ShellProc(SPM_REGISTER, ...) called"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // Sent to the DLL immediately before a script is started. It is sent
        // to all Tux DLLs, including loaded Tux DLLs that are not in the
        // script. All DLLs will receive this message before the first
        // TestProc() in the script is called.
        NKDbgPrintfW(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // Sent to the DLL when the script has stopped. This message is sent
        // when the script reaches its end, or because the user pressed
        // stopped prior to the end of the script. This message is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        NKDbgPrintfW(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow. Only the DLL that is next to run
        // receives this message. The prior DLL, if any, will first receive
        // a SPM_END_GROUP message. For global initialization and
        // de-initialization, the DLL should probably use SPM_START_SCRIPT
        // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        NKDbgPrintfW(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
                //Detect OTG Port 
        if(!InitOTGTest())
        {
            NKDbgPrintfW(TEXT("ShellProc(SPM_BEGIN_GROUP) InitOTGTest Failed "));        
            return TPR_NOT_HANDLED ;
        }       
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        NKDbgPrintfW(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));


              
        break;

    case SPM_BEGIN_TEST:
        // Sent to the DLL immediately before a test executes. This gives
        // the DLL a chance to perform any common action that occurs at the
        // beginning of each test, such as entering a new logging level.
        // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
        // structure, which contains the function table entry and some other
        // useful information for the next test to execute. If the ShellProc
        // function returns SPR_SKIP, then the test case will not execute.
        NKDbgPrintfW(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
        // Start our logging level.
        pBT = (LPSPS_BEGIN_TEST)spParam;
        g_pKato->BeginLevel(
            pBT->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
            pBT->lpFTE->lpDescription,
            pBT->dwThreadCount,
            pBT->dwRandomSeed);
        break;

    case SPM_END_TEST:
        // Sent to the DLL after a single test executes from the DLL.
        // This gives the DLL a time perform any common action that occurs at
        // the completion of each test, such as exiting the current logging
        // level. The spParam parameter will contain a pointer to a
        // SPS_END_TEST structure, which contains the function table entry and
        // some other useful information for the test that just completed. If
        // the ShellProc returned SPR_SKIP for a given test case, then the
        // ShellProc() will not receive a SPM_END_TEST for that given test case.
        NKDbgPrintfW(TEXT("ShellProc(SPM_END_TEST, ...) called"));
        // End our logging level.
        pET  = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(
            TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
        break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        NKDbgPrintfW(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
        g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
        break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}



////////////////////////////////////////////////////////////////////////////////
// DllMain
//  Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Parameters:
//  hInstance       Module instance of the DLL.
//  dwReason        Reason for the function call.
//  lpReserved      Reserved for future use.
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL WINAPI DllMain(HANDLE /* hInstance*/, ULONG /* dwReason*/, LPVOID /* lpReserved */)
{
    // Any initialization code goes here.

    return TRUE;
}

