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
#include "tuxstuff.h"
#include "common.h"

extern CmdLineParams cmdParameters;
extern TCHAR lpszEntryName[RAS_MaxEntryName + 1];
extern RASConnection eRasConnection;

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
//CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

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

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpReserved);
    return TRUE;
}
#endif

#ifndef UNDER_CE

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpReserved);
    return TRUE;
}

#define DEBUGZONE(b)        (ZoneMask & (0x00000001<<b))
#define DEBUGMSG(exp, p)    ((exp) ? _tprintf p, 1 : 0)
//#define RETAILMSG(exp, p)    ((exp) ? _tprintf p, 1 : 0)
#define ASSERT assert

#endif


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
        RETAILMSG(1, (TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n")));

        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif

        // Get/Create our global logging object.
        //g_pKato = (CKato*)KatoGetDefaultObject();

        return SPR_HANDLED;
                       }

                       //------------------------------------------------------------------------
                       // Message: SPM_UNLOAD_DLL
                       //
                       // Sent once to the DLL immediately before it is unloaded.
                       //------------------------------------------------------------------------

    case SPM_UNLOAD_DLL: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\r\n")));

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
        LPTSTR szServer=NULL, szServerV6=NULL;
        TCHAR szCommandLine[MAX_PATH];
        int argc = 10;
        LPTSTR argv[10];

        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

        RETAILMSG(1, (TEXT("ShellProc(SPM_SHELL_INFO, ...) called\r\n")));

        // If there's a commandline, parse it
        if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
        {
            // Parse the command line
            StringCchCopy(szCommandLine, MAX_PATH, g_pShellInfo->szDllCmdLine);
            CommandLineToArgs(szCommandLine, &argc, argv);

            //
            // Initialize some values
            //
            COPY_STRINGS(cmdParameters.strDomain, TEXT("NONE"), MAX_CMD_PARAM);
            COPY_STRINGS(cmdParameters.strPhoneNum, TEXT("8828823"), MAX_CMD_PARAM);
            cmdParameters.dwDeviceID = 0;
            cmdParameters.dwRASAuthTypes = 0;

            //
            // Parse Command line
            //
            if(bParseCmdLine(argc, argv) == FALSE)
            {
                SHOW_HELP();
                return SPR_FAIL;
            }

            if(cmdParameters.dwRASAuthTypes == 0)
            {
                // Initialize the RAS Auth types to MSCHAP and MSCHAPv2 at least
                cmdParameters.dwRASAuthTypes |= (MSCHAP | MSCHAPv2);
            }
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_REGISTER, ...) called\r\n")));
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_START_SCRIPT, ...) called\r\n")));
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called\r\n")));
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n")));
        //g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: T_SCKCLI.DLL"));
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_END_GROUP, ...) called\r\n")));
        //g_pKato->EndLevel(TEXT("END GROUP: T_SCKCLI.DLL"));
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
        RETAILMSG(1, (TEXT("ShellProc(SPM_BEGIN_TEST, ...) called\r\n")));

        Sleep(1000);
        // Start our logging level.
        LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
        /*g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
        TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
        pBT->lpFTE->lpDescription, pBT->dwThreadCount,
        pBT->dwRandomSeed);
        */
        RETAILMSG( 1,
            ( TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u\r\n"),
            pBT->lpFTE->lpDescription, pBT->dwThreadCount,
            pBT->dwRandomSeed ) );

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
        RETAILMSG(1, (TEXT("ShellProc(SPM_END_TEST, ...) called\r\n")));

        // End our logging level.
        LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
        /*g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
        pET->lpFTE->lpDescription,
        pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
        pET->dwResult == TPR_PASS ? TEXT("PASSED") :
        pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
        pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
        */
        RETAILMSG( 1,
            ( TEXT("END TEST: \"%s\", %s, Time=%u.%03u\r\n"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000 ) );

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
        RETAILMSG(1, (TEXT("ShellProc(SPM_EXCEPTION, ...) called\r\n")));
        //g_pKato->Log(LOG_EXCEPTION, TEXT("### Exception occurred!"));
        return SPR_HANDLED;
                        }
    }

    return SPR_NOT_HANDLED;
}

//******************************************************************************
//***** Internal Functions
//******************************************************************************
//
// Parse the command line and initialize the req'd parameters.
// Return FALSE if any error happens, TRUE otherwise
//
BOOL bParseCmdLine(int argc, TCHAR* argv[])
{
    INT iCounter=0;

    while(iCounter < argc)
    {
        if(CMP_STRINGS(argv[iCounter], CMD_DEVICE_ID, MAX_CMD_PARAM))
        {
            cmdParameters.dwDeviceID = _ttol(argv[iCounter+1]);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_DOMAIN, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strDomain, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_PSK, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strPresharedKey, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_PASSWORD, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strPassword, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_PHONE_NUMBER, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strPhoneNum, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_SERVER_NAME, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strServerName, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_DCC_USER_NAME,  MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strDCCUserName, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_DCC_PASSWORD, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strDCCPassword, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_DCC_DOMAIN, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strDCCDomain, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
#if 0
        else if(CMP_STRINGS(argv[iCounter], CMD_SERVER_OPT, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strServOpt, argv[iCounter+1], MAX_CMD_PARAM);

            //
            // Parse this string and set the Auth types accordingly
            //
            if (bParseServOpt() == FALSE)
            {
                return FALSE;
            }

            iCounter++;
        }
#endif
        else if(CMP_STRINGS(argv[iCounter], CMD_USER_NAME, MAX_CMD_PARAM))
        {
            COPY_STRINGS(cmdParameters.strUserName, argv[iCounter+1], MAX_CMD_PARAM);

            iCounter++;
        }
        else if(CMP_STRINGS(argv[iCounter], CMD_HELP_REQD, MAX_CMD_PARAM))
        {
            return FALSE;
        }
        else
        {
            RASPrint(TEXT("Incorrect CMD Line Parameter: %s"), argv[iCounter]);
            return FALSE;
        }

        iCounter++;
    }

    return TRUE;
}

#if 0
//
// Parse the server auth options
//
BOOL bParseServOpt()
{
    TCHAR *strToken;
    TCHAR strTempServOpt[MAX_CMD_PARAM];

    _tcsncpy(strTempServOpt, cmdParameters.strServOpt, MAX_CMD_PARAM);
    strTempServOpt[MAX_CMD_PARAM-1]=0;

    TCHAR strSeparators[] = TEXT(", \n\0");

    strToken = _tcstok(strTempServOpt, strSeparators);
    while(strToken != NULL)
    {
        if (CMP_STRINGS(strToken, TXT_EAP, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= EAP;
        }
        else if (CMP_STRINGS(strToken, TXT_PAP, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= PAP;
        }
        else if (CMP_STRINGS(strToken, TXT_NODCC, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= NODCC;
        }
        else if (CMP_STRINGS(strToken, TXT_MSCHAP, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= MSCHAP;
        }
        else if (CMP_STRINGS(strToken, TXT_MD5CHAP, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= MD5CHAP;
        }
        else if (CMP_STRINGS(strToken, TXT_MSCHAPv2, MAX_CMD_PARAM))
        {
            cmdParameters.dwRASAuthTypes |= MSCHAPv2;
        }
        else
        {
            RASPrint(TEXT("Invalid Server Option: %s"), strToken);
            return FALSE;
        }

        strToken = _tcstok(NULL, strSeparators);
    }

    return TRUE;
}
#endif

//
// Set eRasConnection to the appropriate value depending on cmdParameters.dwDeviceID
//
BOOL bGetDeviceType()
{
    LPRASDEVINFO lpRasDevInfo = NULL;
    DWORD dwBufSize = 0;
    DWORD dwNumDevices = 0;
    DWORD dwDeviceID=0;
    DWORD dwErr;

    // find the buffer size needed
    dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
    if (!dwBufSize)
    {
        RASPrint(TEXT("Unable to find \"RasEnumDevices\" buffer size: %s"), dwErr);
        return FALSE;
    }

    lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
    if (!lpRasDevInfo)
    {
        RASPrint(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));

        return FALSE;
    }
    lpRasDevInfo->dwSize = sizeof(RASDEVINFO);

    dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices);
    if (dwErr)
    {
        RASPrint(TEXT("\"RasEnumDevices\" failed: %s"), dwErr);

        LocalFree(lpRasDevInfo);
        return dwErr;
    }

    //
    // Find the correct device ID for this connection type
    //
    while (dwDeviceID < dwNumDevices)
    {
#if EMBEDDED_ONLY
        if (eRasConnection == RAS_VPN_PPTP)
        {
            // Get the type
            if(CMP_STRINGS((lpRasDevInfo + dwDeviceID)->szDeviceType,
                RASDT_Vpn, RAS_MaxDeviceType+1))
            {
                //
                // L2TP or PPTP?
                //
                if (!_tcsstr((lpRasDevInfo +  dwDeviceID)->szDeviceName, TEXT("L2TP")))
                {
                    break;
                }
            }
        }
        else if (eRasConnection == RAS_VPN_L2TP)
        {
            // Get the type
            if(CMP_STRINGS((lpRasDevInfo + dwDeviceID)->szDeviceType,
                RASDT_Vpn, RAS_MaxDeviceType+1))
            {
                //
                // L2TP or PPTP?
                //
                if (_tcsstr((lpRasDevInfo + dwDeviceID)->szDeviceName, TEXT("L2TP")))
                {
                    break;

                }
            }
        }
        else if (eRasConnection == RAS_DCC_MODEM)
#endif
            if (eRasConnection == RAS_DCC_MODEM)
            {
                // Get the type
                if(CMP_STRINGS((lpRasDevInfo + dwDeviceID)->szDeviceType,
                    RASDT_Direct,
                    RAS_MaxDeviceType+1))
                {
                    // Derault device is "Serial Cable on" either COM1 or COM2
                    if (CMP_STRINGS((lpRasDevInfo + dwDeviceID)->szDeviceName, 
                        TEXT("Serial Cable on COM"), 
                        19))
                        break;
                }
            }

            dwDeviceID++;
    }

    //The Device ID was not found.
    if(dwDeviceID >= dwNumDevices)
    {
        return FALSE;
    }

    cmdParameters.dwDeviceID = dwDeviceID;
    LocalFree(lpRasDevInfo);
    return TRUE;
}
