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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
//
// --------------------------------------------------------------------
//
//
// Main Tux
//
//

#include "main.h"
#include "globals.h"
#include "reg.h"
#include "dlg.h"
#include "resource.h"
#include <sdcardddk.h>
#include <sdcommon.h>
#include <sddetect.h>
#include <storemgr.h>
#include <sd_tst_cmn.h>
#include <cmn_drv_trans.h>

#define MAX_NUMBER_OF_SLOTS     2

BOOL bEventCreateAttempted = FALSE;

BOOL bReset = FALSE;

//////////////////////////////////////////////////////////////////////////////////////////////
// IsCardPresent looks up and returns the values of dwHostIndex and dwSlotIndex for the
// devType passed argument.
//
// Return Values:
//  on Success: TRUE
//  on Failure: FALSE
//
//////////////////////////////////////////////////////////////////////////////////////////////

BOOL IsCardPresent(const SDTST_DEVICE_TYPE devType, DWORD& dwHostIndex, DWORD& dwSlotIndex)
{
    switch (devType)
    {
        case SDDEVICE_SD:
            return getFirstDeviceSlotInfo(Device_SD_Memory, FALSE, dwHostIndex, dwSlotIndex);

        case SDDEVICE_SDHC:
            return getFirstDeviceSlotInfo(Device_SD_Memory, TRUE, dwHostIndex, dwSlotIndex);

        case SDDEVICE_MMC:
            return getFirstDeviceSlotInfo(Device_MMC, FALSE, dwHostIndex, dwSlotIndex);

        case SDDEVICE_MMCHC:
            return getFirstDeviceSlotInfo(Device_MMC, TRUE, dwHostIndex, dwSlotIndex);
    }

    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IsCardPresent
//  Checks for the present of one of the following:  SDMemory, SDHCMemory, MMC, MMCHC, or SDIO.
//
// Return value:
//  TRUE    if one exists
//  FALSE   otherwise
//////////////////////////////////////////////////////////////////////////////////////////////

BOOL IsCardPresent( )
{
    return (hasSDMemory() || hasSDHCMemory() || hasMMC() || hasMMCHC() || hasSDIO());
}

void ProcessCmdLine(LPCTSTR szCmdLine );
////////////////////////////////////////////////////////////////////////////////
// DllMain
//  Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Parameters:
//  hInstance    Module instance of the DLL.
//  dwReason        Reason for the function call.
//  lpReserved    Reserved for future use.
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID /*lpReserved*/)
{
    if ( dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = (HINSTANCE)hInstance;
        g_bMemDrvExists= TRUE;
        g_DriverLoadedEvent = CreateEvent(NULL, FALSE,  FALSE, EVNT_NAME);
        if (g_DriverLoadedEvent == NULL)
        {
            return FALSE;
        }
        bEventCreateAttempted = TRUE;
    }

    if ( dwReason == DLL_PROCESS_DETACH )
    {
        ResetEvent(g_DriverLoadedEvent);
        CloseHandle(g_DriverLoadedEvent);
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like wrapping around OutputDebugString.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...            Additional arguments.
//
// Return value:
//  None.
////////////////////////////////////////////////////////////////////////////////

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("TUXTEST: ");
    TCHAR   szBuffer[1024];
    int c;

    va_list pArgs;
    va_start(pArgs, szFormat);
    StringCchCopy(szBuffer, _countof(szBuffer), szHeader);
    StringCchVPrintf(szBuffer + _countof(szHeader) - 1, (_countof(szBuffer) - sizeof(szHeader) + 1), szFormat, pArgs);
    va_end(pArgs);

    if ( _tcslen(szBuffer) < (_countof(szBuffer) - 3))
    {
        c = _tcslen(szBuffer);
        szBuffer[c] = TCHAR('\r');
        szBuffer[c+1] = TCHAR('\n');
        szBuffer[c+2] = TCHAR('\0');
    }
    else
    {
        szBuffer[1021] = TCHAR('\r');
        szBuffer[1022] = TCHAR('\n');
        szBuffer[1023] = TCHAR('\0');
    }

    OutputDebugString(szBuffer);
}


////////////////////////////////////////////////////////////////////////////////
// ShellProc
//  Processes messages from the TUX shell.
//
// Parameters:
//  uMsg            Message code.
//  spParam        Additional message-dependent data.
//
// Return value:
//  Depends on the message.

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST pBT;
    LPSPS_END_TEST    pET;
    HWND hDlg = NULL;
    int iRet = SPR_HANDLED;
    BOOL rVal;
    LONG lErr = ERROR_SUCCESS;
    MSG msg;
    static BOOL bSDTestDriverLoaded = FALSE;
    DWORD dwWait = 0;
    BOOL regChanged = FALSE;

    DEVMGR_DEVICE_INFORMATION di; //information about Host Controller device
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

    switch (uMsg)
    {
        case SPM_LOAD_DLL:
            // This message is sent once to the DLL immediately after it is loaded
            // Do module global initialization here:

            Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));
            g_ManualInsertEject = false;
            g_UseSoftBlock = false;
            g_deviceType = SDDEVICE_UKN;
            g_UsingCombo = false;
            g_pKato = NULL;
            g_pKato = (CKato*)KatoGetDefaultObject();
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            bSDTestDriverLoaded = FALSE;
            return SPR_HANDLED;

        case SPM_UNLOAD_DLL:
            // This message is sent once to the DLL immediately before it is unloaded. Do module cleanup here:

            rVal = TRUE;
            Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            if (!bSDTestDriverLoaded)
            {
                return SPR_HANDLED;
            }

            //This is forced here so that the user will never have a system where my driver is loaded, but tests are not running
            if( !g_ManualInsertEject )
            {
                // non-manual eject card and non-standard memory driver
                if ( !isMemoryDriverStandard(&lErr) && lErr == ERROR_SUCCESS )
                {
                    if ( !UnloadSDHostController(di, g_sdHostIndex + 1) )
                    {
                        g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to unload the SDHC Driver.  Exiting..."));
                        iRet = SPR_FAIL;
                        goto LOAD_ERR;
                    }

                    //Cleanup/Restore the registry: Restore old driver's registry key values
                    lErr = setDriverName(NULL);
                    if ( lErr != ERROR_SUCCESS )
                    {
                        g_pKato->Comment(0, TEXT("Unable to restore driver name in registry... Exiting..."));
                        iRet =  SPR_FAIL;
                        return iRet;
                    }

                    //reload the host controller, with the original sdmemory driver
                    if ( !LoadingDriver(di) )
                    {
                        g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to reload the SDHC Driver.  Exiting..."));
                        iRet = SPR_FAIL;
                        return iRet;
                    }
                }
            }
            else
            {
                // manual eject card and non-standard memory driver
                if ( !isMemoryDriverStandard(&lErr) && lErr == ERROR_SUCCESS )
                {
                    rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                    while ( rVal )
                    {
                        if ( hDlg == NULL )
                        {
                            hDlg = CreateDialogParam(
                                g_hInst,
                                MAKEINTRESOURCE(IDD_Msg_Dlg),
                                NULL,&MessageProc,
                                (LPARAM)TEXT("Please Eject the card from the SD Slot to restore the original memory driver!!!")
                                );
                        }
                        g_pKato->Comment(1, TEXT("Please Eject the card from the SD Slot!!!"));
                        g_pKato->Comment(1, TEXT("The Original Memory Driver cannot be restored until you do."));
                        Sleep(500);
                        rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                    }

                    if ( hDlg != NULL )
                    {
                        DestroyWindow (hDlg);
                    }

                     while ( rVal )
                    {
                        if (hDlg != NULL )
                        {
                            hDlg = CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_Msg_Dlg),NULL,&MessageProc, (LPARAM)TEXT("Please Eject the card from the SD Slot to restore the original memory driver!!!"));
                        }
                        else
                        {
                            g_pKato->Comment(1, TEXT("Waiting on card Ejection..."));
                        }
                        Sleep(500);
                        rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                    }

                    if ( hDlg != NULL )
                    {
                        DestroyWindow (hDlg);
                    }

                    //Cleanup/Restore the registry: Restore old driver's registry key values
                    lErr = setDriverName(NULL);
                    if ( lErr != ERROR_SUCCESS )
                    {
                        g_pKato->Comment(0, TEXT("Unable restore driver name in registry..."));
                        iRet =  SPR_FAIL;
                    }
                }
            }
            return iRet;


        case SPM_SHELL_INFO:
        {
            // This message is sent once to the DLL immediately following the SPM_LOAD_DLL message,
            // to give the DLL information about its parent shell, Tux.
            // The SPS_SHELL_INFO structure pointed to by spParam is persistent in memory for the entire time
            // that the Tux DLL is loaded in memory. You may store the pointer for later access to the structure.

            Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
            if( g_pShellInfo &&  g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                g_pKato->Comment(0, TEXT("Command Line Options = \"%s\""), g_pShellInfo->szDllCmdLine);
                ProcessCmdLine(g_pShellInfo->szDllCmdLine);
            }

            rVal = TRUE;
            BOOL bRegOK = TRUE;
            LPCTSTR str=_T("SDTst.dll");

            //Check to see if the stanard memory driver is in the registry. If this fails it
            //might be because you are in the middle of a test run. In order to prevent
            //multiple runs simultaneously, the load of the test dll will fail
            g_pKato->Comment(0, TEXT("Verifing that the current memory driver is a real memory driver."));
            g_pKato->Comment(0, TEXT("This is done to verify that we do not attempt to load the test driver more than once."));

            InitClientRegStateVars();
            if ( g_bMemDrvExists)
            {
                bRegOK = isMemoryDriverStandard(&lErr);
                if ( !bRegOK)
                {
                    if ( lErr != ERROR_SUCCESS )
                    {
                        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Strange error when trying to handle registry."));
                        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Exiting..."));
                    }
                    else
                    {
                        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Memory Driver name in registry does not match expected value.\n\tPerhaps you are already running tests?"));
                        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Exiting..."));
                    }
                    return SPR_FAIL;
                }
            }

           if(g_ManualInsertEject)
             {
               ////////////////////////////////
               // We need to manually insert an SD card to continue, the reason is the host may not be designed to be
               // unloaded and reloadable. If that is the case, client need to manually insert/eject an SD peripheral
               // in order to load the test driver for testing purposes.
               ////////////////////////////////
                if ( IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex) )
                {
                    hDlg = CreateDialogParam(
                        g_hInst,
                        MAKEINTRESOURCE(IDD_Msg_Dlg),
                        NULL,MessageProc,
                        (LPARAM)TEXT("Please Eject the card from the SD Slot!!!\nThe Tests Cannot proceed until you do.")
                        );
                    do
                    {
                        if ( hDlg != NULL )
                        {
                            while(PeekMessage(&msg, hDlg, 0, 0, PM_REMOVE))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }
                        g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL):Please Eject the card from the SD Slot!!!"));
                        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):The Tests Cannot proceed until you do."));

                        Sleep(500);
                        rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                    } while ( rVal );
                }

                // at this point, slot should be empty
                if ( hDlg != NULL )
                {
                    DestroyWindow (hDlg);
                }

                //Loading Memory Drivers
                //First change the registry so that the new driver name can be found
                lErr = setDriverName(str);
                if ( lErr != ERROR_SUCCESS )
                {
                    g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to Switch out the Memory Driver in the Registry, Exiting..."));
                    iRet = SPR_FAIL;
                    goto LOAD_ERR;
                }
                else
                {
                    regChanged = TRUE;
                }

                //Next Prompt for memory Card insertion. (This code will not always need to be called once
                //MacAllan changes are completed. The user is prompted until a memory card is inserted.
                hDlg = CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_Msg_Dlg),NULL,&MessageProc, (LPARAM)TEXT("Please insert an SD/MMC Memory or SDIO card into the SD slot.\n"));
                if ( hDlg != NULL )
                {
                    while(PeekMessage(&msg, hDlg, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }

                g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL):Please insert an SD/MMC Memory or SDIO card into the SDIO slot.\n"));
                Sleep(500);
                rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                while ( !rVal )
                {
                    if ( hDlg == NULL )
                    {
                        hDlg = CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_Msg_Dlg),NULL,&MessageProc, (LPARAM)TEXT("Please insert an SD/MMC Memory or SDIO card into the SD slot.\n"));
                    }
                    if ( !rVal )
                    {
                        g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL):Waiting on Card Insertion..."));
                    }
                    if ( hDlg != NULL )
                    {
                        while( PeekMessage(&msg, hDlg, 0, 0, PM_REMOVE) )
                        {
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                    }
                    Sleep(500);
                    rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                }
                if ( !rVal )
                {
                    g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Failure in detecting memory card's presence."));
                    iRet = SPR_FAIL;
                    goto LOAD_ERR;
                }
             }
            else
            {
                //automatically unload the HC
                ////////////////////////////////
                //4    Unloading any SD/SDIO Drivers
                 g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Loading new test driver. "));
                 g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):This involves unloading any old SD Drivers, then changing the registry, and then loading the new test driver"));

                 rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                 if( !rVal )
                  {
                         g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL): Please insert an %s into the SD slot.\n"), TranslateDeviceType(g_deviceType));
                  }

                 while  ( !rVal )
                 {
                     if ( hDlg != NULL )
                     {
                         hDlg = CreateDialogParam(
                             g_hInst,MAKEINTRESOURCE(IDD_Msg_Dlg),
                             NULL,
                             &MessageProc,
                             (LPARAM)TEXT("Please insert a card into the SDIO slot.\n")
                             );
                     }
                     if ( !rVal )
                     {
                         g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL):Waiting on Card Insertion..."));
                     }
                     if ( hDlg != NULL )
                     {
                         while( PeekMessage(&msg, hDlg, 0, 0, PM_REMOVE) )
                         {
                             TranslateMessage(&msg);
                             DispatchMessage(&msg);
                         }
                     }

                     Sleep(500);
                     rVal = IsCardPresent(g_deviceType, g_sdHostIndex, g_sdSlotIndex);
                 }
                 if ( !rVal )
                 {
                     g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Failure in detecting memory card's presence."));
                     iRet = SPR_FAIL;
                     goto LOAD_ERR;
                 }

                 /////////////////////////
                 //found a card
                 g_pKato->Comment(1, TEXT("ShellProc(SPM_LOAD_DLL):Found a card of type %s in the SD Slot!!!"), TranslateDeviceType(g_deviceType));
                 g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Attempting to unload the SDHC Driver."));

                 if ( !UnloadSDHostController(di, g_sdHostIndex + 1) )
                 {
                         g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to unload the SD Host Controller Driver. Exiting..."));
                         iRet = SPR_FAIL;
                         goto DONE_LOAD;
                 }

                 //4 Loading Memory Drivers
                 //First change the registry so that the new driver name can be found
                 lErr = setDriverName(str);
                 if ( lErr != ERROR_SUCCESS )
                 {
                     g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to Switch out the Memory Driver in the Registry, Exiting..."));
                     iRet = SPR_FAIL;
                     goto LOAD_ERR;
                 }
                 else
                 {
                     regChanged = TRUE;
                     //We need to restore the driver's registry entries when we're done.
                     bSDTestDriverLoaded = TRUE;
                 }

                 //now re-load the host controller driver
                 if ( !LoadingDriver(di))
                 {
                     g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to reload the SD Host Driver.  Exiting..."));
                     iRet = SPR_FAIL;
                     goto LOAD_ERR;
                 }
              ///////////////////////////////
            }//end else if g_ManualInsertEject
            goto DONE_LOAD;
LOAD_ERR:
            if ( regChanged )
            {
                //4 Try to fix the registry
                //If the driver failed to load, but the registr is changed, change it back.
                lErr = setDriverName(NULL);
                if ( lErr != ERROR_SUCCESS )
                {
                    g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable restore driver name in registry..."));
                }
            }
DONE_LOAD:

            if ( hDlg != NULL )
            {
                DestroyWindow (hDlg);
            }

            return iRet;
        }

        case SPM_REGISTER:
            // This message is sent once to the DLL immediately following the SPM_SHELL_INFO message
            // to query the DLL for its function table.
            // This is the only ShellProc message that a DLL is required to handle.

            Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            return SPR_HANDLED | SPF_UNICODE;

        case SPM_START_SCRIPT:
            // This message is sent to the DLL immediately before a script is started. It is sent to
            // all loaded DLLs, including loaded DLLs that are not in the script. All DLLs receive
            // this message before the first TestProc in the script is called.

            Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
            g_hTstDriver = NULL;
            if ( !bEventCreateAttempted )
            {
                Debug(TEXT("Atempting to start Script before SPM_LOAD_DLL or if it failed"));
                return SPR_NOT_HANDLED;
            }

            dwWait = WaitForSingleObject(g_DriverLoadedEvent, 100000);
            if ( dwWait == WAIT_TIMEOUT)
            {
                g_pKato->Comment(1, TEXT("*** ShellProc(SPM_START_SCRIPT): Failure to load test driver. Never signaled Test Driver loaded. Timed Out!!! ***"));
                return SPR_FAIL;
            }

            g_pKato->Comment(1, TEXT("*** ShellProc(SPM_START_SCRIPT): Test driver loaded successfully ***"));
            StringCchCopy(g_szDiskName, _countof(g_szDiskName), SDTST_DEVNAME);
            StringCchCat(g_szDiskName, _countof(g_szDiskName), TEXT("1:\0"));

            g_hTstDriver = CreateFile(g_szDiskName, GENERIC_READ | GENERIC_WRITE,  0,  NULL, OPEN_EXISTING, 0, NULL);
            if ( g_hTstDriver == INVALID_HANDLE_VALUE )
            {
                DWORD gle;
                gle = GetLastError();
                g_pKato->Comment(1, TEXT("Failure to get handle to test driver"));
                return SPR_FAIL;
            }
            break;

        case SPM_STOP_SCRIPT:
            // This message is sent to the DLL after the script has stopped. It is sent when the script
            // reaches its end, or because the user pressed stop prior to the end of the script.
            // This message is sent to all loaded DLLs, including loaded DLLs that are not in the script.

            Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
            if ( g_hTstDriver != INVALID_HANDLE_VALUE)
            {
                CloseHandle(g_hTstDriver);
            }
            return SPR_HANDLED;

        case SPM_BEGIN_GROUP:
            // This message is sent to the DLL before a single test or group of tests from that DLL is
            // executed. This gives the DLL time to initialize or allocate data for the tests to follow.
            // Only the DLL that is scheduled to run next receives this message. The prior DLL, if any,
            // first receives a SPM_END_GROUP message.

            Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: SDMEMTUX.DLL"));
            return SPR_HANDLED;

        case SPM_END_GROUP:
            // This message is sent to the DLL after a group of tests from that DLL has completed running.
            // This gives the DLL time to clean up after it has run. This message does not imply that the
            // DLL will not be called again; it signifies that the next test to run belongs to a different
            // DLL or that there are no more tests to run.

            Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(TEXT("END GROUP: SDMEMTUX.DLL"));
           return SPR_HANDLED;

        case SPM_BEGIN_TEST:
            // This message is sent to the DLL immediately before a single test executes. This gives
            // the DLL time to perform any common actions that occur at the beginning of each test,
            // such as entering the current logging level.

            Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(
                pBT->lpFTE->dwUniqueID,
                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                pBT->lpFTE->lpDescription,
                pBT->dwThreadCount,
                pBT->dwRandomSeed);
          return SPR_HANDLED;

        case SPM_END_TEST:
            // This message is sent to the DLL after a single test case from the DLL executes. This gives
            // the DLL time to perform any common actions that occur at the completion of each test case,
            // such as exiting the current logging level.

            Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));
            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(
                TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                pET->lpFTE->lpDescription,
                pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
           return SPR_HANDLED;

        case SPM_EXCEPTION:
            // This message is sent to the DLL whenever code execution in the DLL causes an exception fault.
            // Tux traps all exceptions that occur while executing code inside a test DLL.

            Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
          return SPR_HANDLED;
     }

    return SPR_NOT_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////////////////
// ProcessCmdLine parses the szCmdLine argument for the following elements:
//   1) Memory Test Driver: MMC, MMCHC, SD, or SDHC
//   2) Manual
//   3) Softblock
// And sets the following module globals respectively: g_deviceType, g_ManualInsertEject,
// and g_UseSoftBlock.
//
// Parameters:
//   szCmdLine      Test module command line argument (e.g., Tux -c "szCmdLine")
//
////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCmdLine(LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);
    TCHAR temp[MAX_PATH];

    // default to run SD
        g_deviceType=SDDEVICE_SD;

    if(cmdLine.GetOptString(_T("device"), temp, MAX_PATH))
    {
        if (  _tcscmp(TEXT("MMC"), temp) == 0 || _tcscmp(TEXT("mmc"), temp) == 0 )
        {
            g_deviceType=SDDEVICE_MMC;
        }
        else if (  _tcscmp(TEXT("MMCHC"), temp) == 0 || _tcscmp(TEXT("mmchc"), temp) == 0 )
        {
            g_deviceType=SDDEVICE_MMCHC;
        }
        else if (  _tcscmp(TEXT("SD"), temp) == 0 || _tcscmp(TEXT("sd"), temp) == 0 )
        {
            g_deviceType=SDDEVICE_SD;
        }
        else if (  _tcscmp(TEXT("SDHC"), temp) == 0 || _tcscmp(TEXT("sdhc"), temp) == 0 )
        {
            g_deviceType=SDDEVICE_SDHC;
        }
         else
        {
            g_deviceType=SDDEVICE_UKN;
        }
    }

    g_ManualInsertEject = cmdLine.GetOpt(_T("manual"));
    g_UseSoftBlock = cmdLine.GetOpt(_T("softblock"));
}
