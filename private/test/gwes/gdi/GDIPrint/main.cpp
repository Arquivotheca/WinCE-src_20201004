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

#include "global.h"
#include "clparse.h"

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
}
#endif
  
//******************************************************************************
VOID Debug(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = {NULL};

   va_list pArgs; 
   va_start(pArgs, szFormat);
   _vsntprintf_s(szBuffer, countof(szBuffer), _TRUNCATE, szFormat, pArgs); 
   va_end(pArgs);
   szBuffer[1024-1]=0;
   _tcsncat_s(szBuffer, TEXT("\r\n"), countof(szBuffer) - _tcslen(szBuffer) - 1);

   OutputDebugString(szBuffer);
}

void
OutputCommandOptions()
{
    info(ECHO, TEXT(""));
    info(ECHO, TEXT("/? outputs the usage information."));
    info(ECHO, TEXT("/DrawToPrimary, for manual verification of the images drawn to the primary"));

    info(ECHO, TEXT(""));
    info(ECHO, TEXT(""));
    info(ECHO, TEXT("The following options are used for specifying the printer device manually instead,"));
    info(ECHO, TEXT("of using the print dialog.  If a Driver name, Device name, and Output name are not specified"));
    info(ECHO, TEXT("all of the options are ignored and a print dialog is requested if available."));
    info(ECHO, TEXT(""));
    info(ECHO, TEXT(""));
    info(ECHO, TEXT("/Driver <driver name.  For example, pcl.dll>"));
    info(ECHO, TEXT("/Device <device name.  For example Inkjet, Laser, or some other name which identifies the printer.>"));
    info(ECHO, TEXT("/Output <output device name.  For example LPT1:>"));
    info(ECHO, TEXT("/Copies <integer>"));
    info(ECHO, TEXT("/Length <integer>"));
    info(ECHO, TEXT("/Width <integer>"));
    info(ECHO, TEXT("/Scale <integer>"));
    info(ECHO, TEXT("/YResolution <integer>"));
    info(ECHO, TEXT("/Quality"));
    info(DETAIL, TEXT("Draft"));
    info(DETAIL, TEXT("High"));
    info(ECHO, TEXT("/Color"));
    info(DETAIL, TEXT("Monochome"));
    info(DETAIL, TEXT("Color"));
    info(ECHO, TEXT("/Orientation"));
    info(DETAIL, TEXT("Portrait"));
    info(DETAIL, TEXT("Landscape"));
    info(ECHO, TEXT("/Papersize"));
    info(DETAIL, TEXT("Letter"));
    info(DETAIL, TEXT("Legal"));
    info(DETAIL, TEXT("A4"));
    info(DETAIL, TEXT("B4"));
    info(DETAIL, TEXT("B5"));
    info(ECHO, TEXT(""));
}

bool
ProcessCommandLine()
{
    PHPARAMS php;
    bool bReturnValue = true;
    // temporary holders for the various strings we parse. 
    TCHAR tszTemp[1024];
    DWORD dwTemp;
    class CClParse cCLPparser(g_pShellInfo->szDllCmdLine);
    bool bOptionSet = false;

    memset(&php, 0, sizeof(PHPARAMS));

    // if the user requested command line usage
    // output command line usage and return false so the test suite doesn't continue.
    if(cCLPparser.GetOpt(TEXT("?")))
    {
        OutputCommandOptions();
        bReturnValue = false;
    }
    else
    {
        // if the user requests for the primary to be used.
        if(cCLPparser.GetOpt(TEXT("DrawToPrimary")))
            php.bDrawToPrimary = true;

        // retrieve the printer device name
        if(cCLPparser.GetOptString(TEXT("Driver"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            dwTemp = _tcslen(tszTemp) + 1;
            if(dwTemp > 0)
            {
                CheckNotRESULT(NULL, php.tszDriverName = new(TCHAR[dwTemp]));
                if(php.tszDriverName)
                    _tcsncpy_s(php.tszDriverName, dwTemp, tszTemp, dwTemp-1);
            }
        }

        // retrieve the device name
        if(cCLPparser.GetOptString(TEXT("Device"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            dwTemp = _tcslen(tszTemp) + 1;
            if(dwTemp > 0)
            {
                CheckNotRESULT(NULL, php.tszDeviceName= new(TCHAR[dwTemp]));
                if(php.tszDeviceName)
                    _tcsncpy_s(php.tszDeviceName, dwTemp, tszTemp, dwTemp-1);
            }
        }

        // retrieve the output device name
        if(cCLPparser.GetOptString(TEXT("Output"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            dwTemp = _tcslen(tszTemp) + 1;
            if(dwTemp > 0)
            {
                CheckNotRESULT(NULL, php.tszOutputName = new(TCHAR[dwTemp]));
                if(php.tszOutputName)
                    _tcsncpy_s(php.tszOutputName, dwTemp, tszTemp, dwTemp-1);
            }
        }

        // retrieve the copies
        if(cCLPparser.GetOptVal(TEXT("Copies"), &dwTemp))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_COPIES;
            php.dm.dmCopies = (short) dwTemp;
        }

        // retrieve the quality
        if(cCLPparser.GetOptString(TEXT("Quality"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_PRINTQUALITY;
            if(!_tcsncmp(tszTemp, TEXT("Draft"), countof(tszTemp)))
                php.dm.dmPrintQuality = DMRES_DRAFT;
            else if(!_tcsncmp(tszTemp, TEXT("High"), countof(tszTemp)))
                php.dm.dmPrintQuality = DMRES_HIGH;
            else
            {
                 info(FAIL, TEXT("Invalid print quality %s"), tszTemp);
                 bReturnValue = false;
            }
        }

        // retrieve color/b&w flag
        if(cCLPparser.GetOptString(TEXT("Color"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_COLOR;
            if(!_tcsncmp(tszTemp, TEXT("Monochrome"), countof(tszTemp)))
                php.dm.dmColor = DMCOLOR_MONOCHROME;
            else if(!_tcsncmp(tszTemp, TEXT("Color"), countof(tszTemp)))
                php.dm.dmColor = DMCOLOR_COLOR;
            else
            {
                 info(FAIL, TEXT("Invalid color %s"), tszTemp);
                 bReturnValue = false;
            }
        }

        // retrieve the orientation
        if(cCLPparser.GetOptString(TEXT("Orientation"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_ORIENTATION;
            if(!_tcsncmp(tszTemp, TEXT("Portrait"), countof(tszTemp)))
                php.dm.dmOrientation = DMORIENT_PORTRAIT;
            else if(!_tcsncmp(tszTemp, TEXT("Landscape"), countof(tszTemp)))
                php.dm.dmOrientation = DMORIENT_LANDSCAPE;
            else
            {
                 info(FAIL, TEXT("Invalid orientation %s"), tszTemp);
                 bReturnValue = false;
            }
        }

        // retrieve the paper size
        if(cCLPparser.GetOptString(TEXT("Papersize"), tszTemp, countof(tszTemp)))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_PAPERSIZE;
            if(!_tcsncmp(tszTemp, TEXT("Letter"), countof(tszTemp)))
                php.dm.dmPaperSize = DMPAPER_LETTER;
            else if(!_tcsncmp(tszTemp, TEXT("Legal"), countof(tszTemp)))
                php.dm.dmPaperSize = DMPAPER_LEGAL;
            else if(!_tcsncmp(tszTemp, TEXT("A4"), countof(tszTemp)))
                php.dm.dmPaperSize = DMPAPER_A4;
            else if(!_tcsncmp(tszTemp, TEXT("B4"), countof(tszTemp)))
                php.dm.dmPaperSize = DMPAPER_B4;
            else if(!_tcsncmp(tszTemp, TEXT("B5"), countof(tszTemp)))
                php.dm.dmPaperSize = DMPAPER_B5;
            else
            {
                 info(FAIL, TEXT("Invalid paper size %s"), tszTemp);
                 bReturnValue = false;
            }
        }

        // paper length
        if(cCLPparser.GetOptVal(TEXT("Length"), &dwTemp))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_PAPERLENGTH;
            php.dm.dmPaperLength = (short) dwTemp;
        }

        // paper width
        if(cCLPparser.GetOptVal(TEXT("Width"), &dwTemp))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_PAPERWIDTH;
            php.dm.dmPaperWidth = (short) dwTemp;
        }

        // scale
        if(cCLPparser.GetOptVal(TEXT("Scale"), &dwTemp))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_SCALE;
            php.dm.dmScale = (short) dwTemp;
        }

        // y resolution
        if(cCLPparser.GetOptVal(TEXT("YResolution"), &dwTemp))
        {
            bOptionSet=true;

            php.dm.dmFields |= DM_YRESOLUTION;
            php.dm.dmYResolution = (short) dwTemp;
        }

        if(bOptionSet)
        {
            if(!php.tszDeviceName || !php.tszDriverName || !php.tszOutputName)
            {
                info(DETAIL, TEXT(""));
                info(FAIL, TEXT("To use the command line options for specifying the printer or print parameters,"));
                info(FAIL, TEXT("you must have the driver name, device name and output name set"));
                info(DETAIL, TEXT(""));

                bReturnValue = false;
            }
        }

        // setup the print handler.
        // SetPrintHandlerParams grabs a copy of whatever it needs,
        // leaving the structure to be cleaned up by us.
        gPrintHandler.SetPrintHandlerParams(&php);

        // cleanup our allocations
        if(php.tszDeviceName)
            delete[] php.tszDeviceName;

        if(php.tszDriverName)
            delete[] php.tszDriverName;

        if(php.tszOutputName)
            delete[] php.tszOutputName;
    }
    return bReturnValue;
}

bool
initAll(void)
{
    return gPrintHandler.Initialize();
}

void
closeAll(void)
{
    // the destructor handles everything.
    //gPrintHandler.Cleanup();
}

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      //------------------------------------------------------------------------
      // Message: SPM_LOAD_DLL
      //
      // Sent once to the DLL immediately after it is loaded.  The spParam 
      // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
      // should set the fUnicode member of this structre to TRUE if the DLL is
      // built with the UNICODE flag set.  By setting this flag, Tux will ensure
      // that all strings passed to your DLL will be in UNICODE format, and all
      // strings within your function table will be processed by Tux as UNICODE.
      // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_LOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

         // If we are UNICODE, then tell Tux this by setting the following flag.
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
         #endif

         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         // Initialize our global critical section.
         InitializeCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

         // This is a good place to delete our global critical section.
         DeleteCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SHELL_INFO
      //
      // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
      // some useful information about its parent shell and environment.  The
      // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
      // The pointer to the structure may be stored for later use as it will
      // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
      // to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_SHELL_INFO: {
         Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         globalInst = g_pShellInfo->hLib;

        // if the process command line fails, then that means the user requested the command
        // line options, so exit the dll.
        if(!ProcessCommandLine())
        {
            info(ECHO, TEXT("User requested command line usage, or command line usage incorrect.  Failing the test suite to it won't continue"));
            return SPR_FAIL;
        }
            
         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
#ifdef UNDER_CE
            Debug(TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
#else
            MessageBox(g_pShellInfo->hWnd, g_pShellInfo->szDllCmdLine,
                       TEXT("TUXDEMO.DLL Command Line Arguments"), MB_OK);
#endif UNDER_CE
         }

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it's function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
         Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      //------------------------------------------------------------------------
      // Message: SPM_START_SCRIPT
      //
      // Sent to the DLL immediately before a script is started.  It is sent to
      // all Tux DLLs, including loaded Tux DLLs that are not in the script.
      // All DLLs will receive this message before the first TestProc() in the
      // script is called.
      //------------------------------------------------------------------------

      case SPM_START_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_STOP_SCRIPT
      //
      // Sent to the DLL when the script has stopped.  This message is sent when
      // the script reaches its end, or because the user pressed stopped prior
      // to the end of the script.  This message is sent to all Tux DLLs,
      // including loaded Tux DLLs that are not in the script.
      //------------------------------------------------------------------------

      case SPM_STOP_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_GROUP
      //
      // Sent to the DLL before a group of tests from that DLL is about to be
      // executed.  This gives the DLL a time to initialize or allocate data for
      // the tests to follow.  Only the DLL that is next to run receives this
      // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
      // message.  For global initialization and de-initialization, the DLL
      // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
      // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
      //------------------------------------------------------------------------

      case SPM_BEGIN_GROUP: {
         Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXDEMO.DLL"));

         if(!initAll())
            return SPR_FAIL;

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_GROUP
      //
      // Sent to the DLL after a group of tests from that DLL has completed
      // running.  This gives the DLL a time to cleanup after it has been run.
      // This message does not mean that the DLL will not be called again to run
      // tests; it just means that the next test to run belongs to a different
      // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
      // is active and when it is not active.
      //------------------------------------------------------------------------

      case SPM_END_GROUP: {
         Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));

        // this is a good place to do our final cleanups
         closeAll();

         g_pKato->EndLevel(TEXT("END GROUP: TUXDEMO.DLL"));

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_TEST
      //
      // Sent to the DLL immediately before a test executes.  This gives the DLL
      // a chance to perform any common action that occurs at the beginning of
      // each test, such as entering a new logging level.  The spParam parameter
      // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
      // the function table entry and some other useful information for the next
      // test to execute.  If the ShellProc function returns SPR_SKIP, then the
      // test case will not execute.
      //------------------------------------------------------------------------

      case SPM_BEGIN_TEST: {
         Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_TEST
      //
      // Sent to the DLL after a single test executes from the DLL.  This gives
      // the DLL a time perform any common action that occurs at the completion
      // of each test, such as exiting the current logging level.  The spParam
      // parameter will contain a pointer to a SPS_END_TEST structure, which
      // contains the function table entry and some other useful information for
      // the test that just completed.
      //------------------------------------------------------------------------

      case SPM_END_TEST: {
         Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_EXCEPTION
      //
      // Sent to the DLL whenever code execution in the DLL causes and exception
      // fault.  By default, Tux traps all exceptions that occur while executing
      // code inside a Tux DLL.
      //------------------------------------------------------------------------

      case SPM_EXCEPTION: {
         Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}
