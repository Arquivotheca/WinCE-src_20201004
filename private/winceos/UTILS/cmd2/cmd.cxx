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

#include <windows.h>
#include <stdlib.h>

#if ! defined (UNDER_CE)
#include <stdio.h>
#include <direct.h>
#endif

#if defined (UNDER_CE)
#include <windev.h>
#include <console.h>
#include <bldver.h>
#endif

#include <intsafe.h>

#include "cmd.hxx"
#include "cmdrsc.h"

CmdState    *pCmdState;
TCHAR       gYes[CMD_BUF_COMMAND];
FILE        *gIO[3];
TCHAR       *gIOM[3][2];
HANDLE      g_hControlCThread;
HANDLE      g_hControlCEvent;
HANDLE      g_hShutdownEvent;

BOOL     gRunForever = FALSE;

// Debug Zones.

#ifdef DEBUG
  DBGPARAM dpCurSettings = {
    TEXT("CMD"), {
    TEXT("Main Trace"), 0}
  }; 
  #define ZONE_TRACE    DEBUGZONE(0)
#endif

//
//  Control handler
//
#if defined (UNDER_CE)
int cmd_ControlCHandler(void)
{
    if (pCmdState->hExecutingProc)
        TerminateProcess (pCmdState->hExecutingProc, 0);

    pCmdState->fCtrlC = TRUE;

    return FALSE;
}
#else
BOOL WINAPI cmd_InterruptHandler (DWORD dwCtrl) {
    if (dwCtrl == CTRL_C_EVENT) {
        if (pCmdState->hExecutingProc)
            TerminateProcess (pCmdState->hExecutingProc, 0);

        pCmdState->fCtrlC = TRUE;
        return TRUE;
    }

    return FALSE;
}
#endif

DWORD WINAPI ControlCHandlerThread(LPVOID lpv)
{
    HANDLE h[] = {g_hControlCEvent, g_hShutdownEvent};
    DWORD dwWait;

    for (;;) {
        // wait for control-c character
        dwWait = WaitForMultipleObjects(sizeof(h) / sizeof(h[0]), h, FALSE, INFINITE);
        if (dwWait != WAIT_OBJECT_0)
            break;

        // pass the control-c command to the handler
        cmd_ControlCHandler();
    }

    return 0;
}


//
//  Interpreter initialization
//
static int cmd_Initialize (void) {
    gIO[CMDIO_IN]  = stdin;
    gIO[CMDIO_OUT] = stdout;
    gIO[CMDIO_ERR] = stderr;
    gIOM[CMDIO_IN][0] = TEXT("rt");
    gIOM[CMDIO_IN][1] = TEXT("rt");
    gIOM[CMDIO_OUT][0] = TEXT("at");
    gIOM[CMDIO_OUT][1] = TEXT("wt");
    gIOM[CMDIO_ERR][0] = TEXT("at");
    gIOM[CMDIO_ERR][1] = TEXT("wt");

    pCmdState = (CmdState *)cmdutil_Alloc (sizeof (*pCmdState));

    pCmdState->fpIO[CMDIO_IN]  = stdin;
    pCmdState->fpIO[CMDIO_OUT] = stdout;
    pCmdState->fpIO[CMDIO_ERR] = stderr;

    pCmdState->hInstance     = GetModuleHandle (NULL);
    pCmdState->uiFlags       = CMD_ECHO_ON;
    pCmdState->iBufInputChars= CMD_BUF_REG;
    pCmdState->lpBufInput    = (TCHAR *)cmdutil_Alloc (pCmdState->iBufInputChars * sizeof(TCHAR));
    pCmdState->iBufCmdChars  = CMD_BUF_REG;
    pCmdState->lpBufCmd      = (TCHAR *)cmdutil_Alloc (pCmdState->iBufInputChars * sizeof(TCHAR));

    pCmdState->pListMem      = cmdutil_GetFMem (sizeof(PointerList));

    memcpy (pCmdState->sPrompt, TEXT("$P$G$S"), sizeof(TEXT("$P$G$S")));

    memcpy (pCmdState->sCwd, TEXT("\\"), sizeof(TEXT("\\")));

    LoadString (pCmdState->hInstance, IDS_YES, gYes, CMD_BUF_COMMAND);

#if defined (UNDER_CE)
    if (WAIT_OBJECT_0 == WaitForAPIReady (SH_SHELL, 0)) {
        HMODULE hCoreDll = LoadLibrary (TEXT("coredll.dll"));
        if (hCoreDll) {
            pCmdState->ShellExecuteEx = (SEE_t)GetProcAddress (hCoreDll, TEXT("ShellExecuteEx"));
            if (! pCmdState->ShellExecuteEx)
                FreeLibrary (hCoreDll);
        }
    }

    WCHAR szStdPath[MAX_PATH + 1];
    DWORD dwLen = MAX_PATH;

    //
    //  If STDOUT is not set, load the console
    //
    if (! (GetStdioPathW(1, szStdPath, &dwLen) && dwLen && szStdPath[0])) {
        _ftprintf (stdout, TEXT(" "));  // Rude hack to load the console

        if(ferror(stdout)) {
            TCHAR szBuffer[CMD_BUF_REG], szHeader[CMD_BUF_SMALL];

            LoadString (pCmdState->hInstance, IDS_ERROR_START_CANT, szBuffer, CMD_BUF_REG);
            LoadString (pCmdState->hInstance, IDS_ERROR_TITLE, szHeader, CMD_BUF_SMALL);
#if defined (UNDER_CE)

            HMODULE hLib = LoadLibrary (L"coredll.dll");
            if (hLib) {
                typedef int (*MessageBoxW_t)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
                MessageBoxW_t pmb = (MessageBoxW_t)GetProcAddress (hLib, L"MessageBoxW");
                if (pmb)
                    pmb (NULL, szBuffer, szHeader, MB_OK | MB_ICONSTOP);

                FreeLibrary (hLib);
            }

#else
            MessageBox(NULL, szBuffer, szHeader, MB_OK | MB_ICONSTOP);
#endif
            return FALSE;
        }
    }

    //
    //  If STDERR is not set, make the same as STDOUT
    //
    dwLen = MAX_PATH;
    if (! (GetStdioPathW(2, szStdPath, &dwLen) && dwLen && szStdPath[0])) {
        dwLen = MAX_PATH;
        if (GetStdioPathW(1, szStdPath, &dwLen) && dwLen && szStdPath[0]) {
            szStdPath[dwLen] = L'\0';
            SetStdioPathW (2, szStdPath);
        }
    }

    //
    //  Note, if STDIN is not set, it will be loaded later.
    //  If we are not dealing with console, logic below will gracefully fail...
    //
    
    USE_CONIOCTL_CALLS

    CeGetConsoleRows(_fileno(stdout), &(pCmdState->yMax));
    CeGetConsoleCols(_fileno(stdout), &(pCmdState->xMax));

    if (pCmdState->yMax <= 0)
        pCmdState->yMax = INT_MAX;

    if (pCmdState->xMax <= 0)
        pCmdState->xMax = 80;

    // create a thread and correcponding events to monitor control-c characters
    g_hControlCEvent = CreateEvent(NULL, 0, 0, 0);
    if (!g_hControlCEvent)
        return FALSE;

    // If a device does not support Control-C, it is not a fatal error
    if ( CeSetControlCEvent(_fileno(stdout), &g_hControlCEvent) ) {
        g_hShutdownEvent = CreateEvent(NULL, 0, 0, 0);
        if (!g_hShutdownEvent) {
            return FALSE;
        }
        
        g_hControlCThread = CreateThread(NULL, 0, ControlCHandlerThread, NULL, 0, NULL);
        if (!g_hControlCThread) {
            return FALSE;
        }
    }
    else {
        // The Control-C event isn't needed in this case.
        CloseHandle(g_hControlCEvent);
        g_hControlCEvent = NULL;
    }

    WCHAR szTitleBuf1[CMD_BUF_TINY];
    char szTitleBuf2[CMD_BUF_TINY];

    if (LoadStringW (pCmdState->hInstance, IDS_CMD_TITLE, szTitleBuf1, CMD_BUF_TINY) && (WideCharToMultiByte(CP_ACP, 0, szTitleBuf1, -1, szTitleBuf2, CMD_BUF_TINY, NULL, NULL) != 0)) {
        szTitleBuf2[CMD_BUF_TINY - 1] = '\0';
        CeSetConsoleTitleA(_fileno(gIO[CMDIO_OUT]), szTitleBuf2);
    }
#else
    HANDLE hConsoleOutput    = GetStdHandle (STD_OUTPUT_HANDLE);
    
    CONSOLE_SCREEN_BUFFER_INFO  CSBI;
    GetConsoleScreenBufferInfo (hConsoleOutput, &CSBI);

    pCmdState->xMax          = CSBI.dwSize.X;
    pCmdState->yMax          = CSBI.dwSize.Y;

    SetConsoleCtrlHandler (cmd_InterruptHandler, TRUE);

    void *pvEnv = GetEnvironmentStrings ();
    
    if (pvEnv) {
        TCHAR *lpEnvVar = (TCHAR *)pvEnv;
        while (*lpEnvVar != TEXT('\0')) {
            TCHAR sVarName[CMD_BUF_REG];
            TCHAR *lpBody = _tcschr (lpEnvVar, TEXT('='));
            if (lpBody && (lpBody != lpEnvVar) && (lpBody - lpEnvVar < CMD_BUF_REG)) {
                int iSize = lpBody - lpEnvVar;
                memcpy (sVarName, lpEnvVar, iSize * sizeof(TCHAR));
                sVarName[iSize] = TEXT('\0');

                ++lpBody;
        
                _tcsupr (sVarName);

                if (*lpBody != TEXT('\0'))
                    pCmdState->ehHash.hash_set (sVarName, lpBody);
            }
            lpEnvVar = _tcschr (lpEnvVar, TEXT('\0'));
            ++lpEnvVar;
        }

        FreeEnvironmentStrings ((TCHAR *)pvEnv);
    }

#endif

    return TRUE;
}

static void cmd_IncreaseInputBuffer (void) 
{
    UINT uLen = 0;
    int iNewSize = pCmdState->iBufInputChars + CMD_BUF_INCREMENT;
    if (SUCCEEDED(UIntMult(iNewSize, sizeof (TCHAR), &uLen))) {
        pCmdState->iBufInputChars = iNewSize;
        pCmdState->lpBufInput = (TCHAR *)cmdutil_ReAlloc (pCmdState->lpBufInput, uLen);
    }
}

//
//  Parse startup args of the processor
//
//  Supported arguments /Q - turn echo off
//                      /C - execute command and return
//                      /K - execute command and continue
//                      /X - do not allow exit command - only valid with /K (run forever)
//
static unsigned int cmd_ParseStartupArgs (int argc, TCHAR **argv) {
    int             fAcc    = FALSE;
    int             fNoExit = FALSE;
    int             fStayAround = FALSE;
    unsigned int    uiFlags = 0;
    int             iChars  = 0;

    for (int i = 1 ; i < argc ; ++i) {
        if (fAcc) {
            int sz = _tcslen (argv[i]);
            while (iChars + sz + 4 > pCmdState->iBufInputChars)
                cmd_IncreaseInputBuffer ();

            if (iChars > 0) {
                pCmdState->lpBufInput[iChars] = TEXT(' ');
                ++iChars;
            }

            int fNeedsQuotes = cmdutil_NeedQuotes (argv[i]);

            if (fNeedsQuotes) {
                pCmdState->lpBufInput[iChars] = TEXT('"');
                ++iChars;
            }

            memcpy (pCmdState->lpBufInput + iChars, argv[i], sz * sizeof (TCHAR));
            iChars += sz;

            if (fNeedsQuotes) {
                pCmdState->lpBufInput[iChars] = TEXT('"');
                ++iChars;
            }

            pCmdState->lpBufInput[iChars] = TEXT('\0');
        } else if ((argv[i][0] == TEXT('/')) && ((argv[i][1] == TEXT('c')) ||
                                (argv[i][1] == TEXT('C')))) {
            uiFlags |= CMD_EXEC_ONCE;
            fAcc = TRUE;
        } else if ((argv[i][0] == TEXT('/')) && ((argv[i][1] == TEXT('k')) ||
                                (argv[i][1] == TEXT('K')))) {
            fAcc = TRUE;
            fStayAround = TRUE;
        } else if ((argv[i][0] == TEXT('/')) && ((argv[i][1] == TEXT('x')) ||
                                (argv[i][1] == TEXT('X')))) {
            fNoExit = TRUE;
        } else if ((argv[i][0] == TEXT('/')) && ((argv[i][1] == TEXT('q')) ||
                                (argv[i][1] == TEXT('Q')))) {
            uiFlags |= CMD_EXEC_QUIET;
        } else if ((argv[i][0] == TEXT('/')) && (argv[i][1] == TEXT('?'))) {
            TCHAR sBuffer[CMD_BUF_HUGE];
            LoadString (pCmdState->hInstance, IDS_HELP_CMD, sBuffer, CMD_BUF_HUGE);
            _fputts (sBuffer, STDOUT);
            return CMD_PARSE_ERROR;
        } else {
            TCHAR sBuffer[CMD_BUF_HUGE];
            LoadString (pCmdState->hInstance, IDS_ERROR_GENERAL_BADOPTION, sBuffer, CMD_BUF_HUGE);
            _ftprintf (STDOUT, sBuffer, argv[i]);
            return CMD_PARSE_ERROR;
        }
    }

    if ((fStayAround) && (!(uiFlags & CMD_EXEC_ONCE)))
        gRunForever = fNoExit;

    pCmdState->lpBufInput[iChars] = TEXT('\0');

    return uiFlags;
}

//
//  Output prompt
//
void cmd_Prompt (void) {
    TCHAR *lpPattern = pCmdState->sPrompt;
    while (*lpPattern != TEXT('\0')) {
        if (*lpPattern == TEXT('$')) {
            ++lpPattern;

            switch (_ttoupper(*lpPattern)) {
                case TEXT('$'):
                    _fputtc (TEXT('$'), STDOUT);
                    break;
                case TEXT('A'):
                    _fputtc (TEXT('&'), STDOUT);
                    break;
                case TEXT('B'):
                    _fputtc (TEXT('|'), STDOUT);
                    break;
                case TEXT('C'):
                    _fputtc (TEXT('('), STDOUT);
                    break;
                case TEXT('D'): {
                    SYSTEMTIME st;
                    GetLocalTime (&st);

                    TCHAR szDateFormat[2];
                    if (! GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, szDateFormat, 2))
                        szDateFormat[0] = TEXT('0');

                    TCHAR *lpFormat = TEXT("ddd yyyy/MM/dd");       // ddd yyyy/mm/dd
                    if (szDateFormat[0] == TEXT('0'))
                        lpFormat = TEXT("ddd MM/dd/yyyy");        // ddd mm/dd/yy
                    else if (szDateFormat[0] == TEXT('1'))
                        lpFormat = TEXT("dddd dd/MM/yyyy");        // ddd dd/mm/yy

                    TCHAR sBuffer[CMD_BUF_SMALL];

                    GetDateFormat (LOCALE_USER_DEFAULT, 0, &st, lpFormat, sBuffer, CMD_BUF_SMALL);
                    _fputts (sBuffer, STDOUT);
                    }
                    break;
                case TEXT('E'):
                    _fputtc (TEXT('\033'), STDOUT);
                    break;
                case TEXT('F'):
                    _fputtc (TEXT(')'), STDOUT);
                    break;
                case TEXT('G'):
                    _fputtc (TEXT('>'), STDOUT);
                    break;
                case TEXT('L'):
                    _fputtc (TEXT('<'), STDOUT);
                    break;
                case TEXT('P'):
                    _fputts (pCmdState->sCwd, STDOUT);
                    break;
                case TEXT('Q'):
                    _fputtc (TEXT('='), STDOUT);
                    break;
                case TEXT('S'):
                    _fputtc (TEXT(' '), STDOUT);
                    break;
                case TEXT('T'): {
                    SYSTEMTIME st;
                    GetLocalTime (&st);
                    TCHAR sBuffer[CMD_BUF_SMALL];
                    GetTimeFormat (LOCALE_USER_DEFAULT, 0, &st, NULL, sBuffer, CMD_BUF_SMALL);
                    _fputts (sBuffer, STDOUT);
                    }
                    break;
                case TEXT('_'):
                    _fputtc (TEXT('\n'), STDOUT);
                    break;
                
                default:
                    continue;
            }

        } else
            _fputtc (*lpPattern, STDOUT);

        ++lpPattern;
    }
}

//
//  Collect input...
//
int cmd_GetInput (void) {
    int iCurrentPos = 0;
    int iRes = FALSE;

    pCmdState->lpBufInput[0] = TEXT('\0');

    if (! pCmdState->fpCommandInput) {
        cmd_Prompt ();
    }

    FILE *fpIn = pCmdState->fpCommandInput ? pCmdState->fpCommandInput : stdin;

    for ( ; ; ) {
        PREFAST_DEBUGCHK (pCmdState->iBufInputChars >= iCurrentPos);
        if (! _fgetts (&pCmdState->lpBufInput[iCurrentPos],
                pCmdState->iBufInputChars - iCurrentPos, fpIn)) {
            iRes = iCurrentPos > 0;
            break;
        }

        int nChars = _tcslen (&pCmdState->lpBufInput[iCurrentPos]);
        iCurrentPos += nChars;

        ASSERT (iCurrentPos < pCmdState->iBufInputChars);

        //
        //  Did the input fit or do we need to grow the buffer?
        //
        if (pCmdState->lpBufInput[iCurrentPos - 1] != TEXT('\n') && (! feof (fpIn))) {
            ASSERT (iCurrentPos == pCmdState->iBufInputChars - 1);
            cmd_IncreaseInputBuffer ();
            continue;
        }

        if (pCmdState->lpBufInput[iCurrentPos - 1] == TEXT('\n'))
            pCmdState->lpBufInput[iCurrentPos - 1] = TEXT('\0');

        iRes = TRUE;
        break;
    }

    //
    //  Batch file? Echo on?
    //
    if (pCmdState->fpCommandInput && (pCmdState->uiFlags & CMD_ECHO_ON)) {
        if ((pCmdState->lpBufInput[0] != TEXT('@')) && (pCmdState->lpBufInput[0] != TEXT(':'))) {
            cmd_Prompt ();
            _fputts (pCmdState->lpBufInput, STDOUT);
            _fputts (TEXT("\n"), STDOUT);
        }
    }

    return iRes;
}

//
//  Input loop processing...
//
void cmd_InputLoop (void) {
    while (! (pCmdState->uiFlags & CMD_EXEC_EXIT)) {
        if (! cmd_GetInput ())
            return;

        cmdexec_Execute ();

        if (pCmdState->fpCommandInput && ((pCmdState->uiFlags & CMD_EXEC_SKIPDOWN) || pCmdState->fCtrlC))
            return;
    }
}

//
// cleanup
//
void cmd_Uninitialize()
{
    DWORD dwWait;

    // shut down the control-c thread
    if (g_hShutdownEvent) {
        SetEvent(g_hShutdownEvent);
        CloseHandle(g_hShutdownEvent);
    }

    if (g_hControlCEvent) {
        SetEvent(g_hControlCEvent);
        CloseHandle(g_hControlCEvent);
    }

    if (g_hControlCThread) {
        dwWait = WaitForSingleObject(g_hControlCThread, 3000);
        DEBUGCHK(WAIT_OBJECT_0 == dwWait);
        CloseHandle(g_hControlCThread);
    }
}

//
//  Main
//
int _tmain (int argc, TCHAR **argv) 
{
    unsigned int iCmd = 0;
    int iReturn = 0;

    if (cmd_Initialize ()) {

        iCmd = cmd_ParseStartupArgs (argc, argv);
        if (iCmd & CMD_PARSE_ERROR) {
            iReturn = CMD_ERROR;

        } else if (! (iCmd & CMD_EXEC_QUIET)) {
            TCHAR   sBuffer[CMD_BUF_REG];
            if (LoadString (pCmdState->hInstance, IDS_STARTMESSAGE, sBuffer, CMD_BUF_REG))
                _ftprintf (stdout, sBuffer, CE_MAJOR_VER, CE_MINOR_VER);
            else
                iReturn = CMD_ERROR;
        }

        if (iReturn != CMD_ERROR) {
            cmdexec_Execute ();
            if (! (iCmd & CMD_EXEC_ONCE))
                cmd_InputLoop ();
        }
    }

    cmd_Uninitialize();
    return iReturn;
}
