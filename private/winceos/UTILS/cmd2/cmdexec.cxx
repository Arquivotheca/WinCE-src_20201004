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

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>

#if ! defined (UNDER_CE)
#include <io.h>
#endif

#if defined (UNDER_CE)
#include <windev.h>
#include <console.h>
#endif

#include <intsafe.h>

#include "cmd.hxx"
#include "cmdpipe.hxx"
#include "cmdrsc.h"

//
//  Externs
//
extern CmdState *pCmdState;
static TCHAR    *gpSort;        // If ever multithreading, protect me with CS
static int      gfOptions;
//
//  Static globals
//
int cmdexec_FindCommand (TCHAR *lpName);
int cmdexec_TryProgram (TCHAR *lpName, TCHAR *lpArgs, int fDetach);
void cmdexec_ParseCmdAndArgs (TCHAR *lpStart);
void cmdexec_ResetFields (void);
int cmdexec_FactorCommand (void);
int cmdexec_Dispatch (void);

int cmdexec_MergeDirOptions (PointerList *ppl, int &, int &, int &, TCHAR *&);

int cmdexec_CmdCd   (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdMd   (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdRd   (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdIf   (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdCls  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdSet  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdPwd  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdDel  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdDir  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdRen  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdRem  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdExit (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdHelp (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdEcho (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdPath (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdType (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdTime (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdDate (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdMove (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdCopy (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdCall (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdGoto (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdStart (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdPause (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdShift (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdTitle (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdPrompt (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdAttrib (TCHAR *lpName, TCHAR *lpArgs);

int cmdexec_CmdDebug  (TCHAR *lpName, TCHAR *lpArgs);
int cmdexec_CmdDump   (TCHAR *lpName, TCHAR *lpArgs);

Command aAllCommands[] = {
    { TEXT("ATTRIB"),   IDS_HELP_CMD_ATTRIB,    IDS_HELP_EXT_ATTRIB,    cmdexec_CmdAttrib   },
    { TEXT("CALL"),     IDS_HELP_CMD_CALL,      IDS_HELP_EXT_CALL,      cmdexec_CmdCall     },
    { TEXT("CD"),       IDS_HELP_CMD_CD,        IDS_HELP_EXT_CD,        cmdexec_CmdCd       },
    { TEXT("CHDIR"),    IDS_HELP_CMD_CHDIR,     IDS_HELP_EXT_CD,        cmdexec_CmdCd       },
    { TEXT("CLS"),      IDS_HELP_CMD_CLS,       IDS_HELP_EXT_CLS,       cmdexec_CmdCls      },
    { TEXT("COPY"),     IDS_HELP_CMD_COPY,      IDS_HELP_EXT_COPY,      cmdexec_CmdCopy     },
    { TEXT("DATE"),     IDS_HELP_CMD_DATE,      IDS_HELP_EXT_DATE,      cmdexec_CmdDate     },
    { TEXT("DEL"),      IDS_HELP_CMD_DEL,       IDS_HELP_EXT_DEL,       cmdexec_CmdDel      },
    { TEXT("DIR"),      IDS_HELP_CMD_DIR,       IDS_HELP_EXT_DIR,       cmdexec_CmdDir      },
    { TEXT("ECHO"),     IDS_HELP_CMD_ECHO,      IDS_HELP_EXT_ECHO,      cmdexec_CmdEcho     },
    { TEXT("ERASE"),    IDS_HELP_CMD_ERASE,     IDS_HELP_EXT_DEL,       cmdexec_CmdDel      },
    { TEXT("EXIT"),     IDS_HELP_CMD_EXIT,      IDS_HELP_EXT_EXIT,      cmdexec_CmdExit     },
    { TEXT("HELP"),     IDS_HELP_CMD_HELP,      IDS_HELP_EXT_HELP,      cmdexec_CmdHelp     },
    { TEXT("GOTO"),     IDS_HELP_CMD_GOTO,      IDS_HELP_EXT_GOTO,      cmdexec_CmdGoto     },
    { TEXT("IF"),       IDS_HELP_CMD_IF,        IDS_HELP_EXT_IF,        cmdexec_CmdIf       },
    { TEXT("MD"),       IDS_HELP_CMD_MD,        IDS_HELP_EXT_MD,        cmdexec_CmdMd       },
    { TEXT("MKDIR"),    IDS_HELP_CMD_MKDIR,     IDS_HELP_EXT_MD,        cmdexec_CmdMd       },
    { TEXT("MOVE"),     IDS_HELP_CMD_MOVE,      IDS_HELP_EXT_MOVE,      cmdexec_CmdMove     },
    { TEXT("PATH"),     IDS_HELP_CMD_PATH,      IDS_HELP_EXT_PATH,      cmdexec_CmdPath     },
    { TEXT("PAUSE"),    IDS_HELP_CMD_PAUSE,     IDS_HELP_EXT_PAUSE,     cmdexec_CmdPause    },
    { TEXT("PROMPT"),   IDS_HELP_CMD_PROMPT,    IDS_HELP_EXT_PROMPT,    cmdexec_CmdPrompt   },
    { TEXT("PWD"),      IDS_HELP_CMD_PWD,       IDS_HELP_EXT_PWD,       cmdexec_CmdPwd      },
    { TEXT("RD"),       IDS_HELP_CMD_RD,        IDS_HELP_EXT_RD,        cmdexec_CmdRd       },
    { TEXT("REM"),      IDS_HELP_CMD_REM,       IDS_HELP_EXT_REM,       cmdexec_CmdRem      },
    { TEXT("REN"),      IDS_HELP_CMD_REN,       IDS_HELP_EXT_REN,       cmdexec_CmdRen      },
    { TEXT("RENAME"),   IDS_HELP_CMD_RENAME,    IDS_HELP_EXT_REN,       cmdexec_CmdRen      },
    { TEXT("RMDIR"),    IDS_HELP_CMD_RMDIR,     IDS_HELP_EXT_RD,        cmdexec_CmdRd       },
    { TEXT("SET"),      IDS_HELP_CMD_SET,       IDS_HELP_EXT_SET,       cmdexec_CmdSet      },
    { TEXT("SHIFT"),    IDS_HELP_CMD_SHIFT,     IDS_HELP_EXT_SHIFT,     cmdexec_CmdShift    },
    { TEXT("START"),    IDS_HELP_CMD_START,     IDS_HELP_EXT_START,     cmdexec_CmdStart    },
    { TEXT("TIME"),     IDS_HELP_CMD_TIME,      IDS_HELP_EXT_TIME,      cmdexec_CmdTime     },
    { TEXT("TITLE"),    IDS_HELP_CMD_TITLE,     IDS_HELP_EXT_TITLE,     cmdexec_CmdTitle    },
    { TEXT("TYPE"),     IDS_HELP_CMD_TYPE,      IDS_HELP_EXT_TYPE,      cmdexec_CmdType     },
    { TEXT("SYSDEBUG"), 0,                      0,                      cmdexec_CmdDebug    },
    { TEXT("SYSDUMP"),  0,                      0,                      cmdexec_CmdDump     }
};

#define CMD_COMMAND_NUMBER (sizeof(aAllCommands) / sizeof(Command))

//
//  COMMAND IMPLEMENTATION SECTION
//
int cmdexec_CmdCls (TCHAR *lpName, TCHAR *lpArgs) {
    USE_CONIOCTL_CALLS
    CeClearConsoleScreen(_fileno(stdout));
    return CMD_SUCCESS;
}

int cmdexec_CmdGoto (TCHAR *lpName, TCHAR *lpArgs) {
    if (*lpArgs == TEXT(':'))
        ++lpArgs;

    if ((*lpArgs != TEXT('\0')) && pCmdState->lpCommandInput) {
        if (_tcsicmp (lpArgs, TEXT("EOF")) == 0) {
            fseek (pCmdState->fpCommandInput, 0, SEEK_END);
            return CMD_SUCCESS;
        }

        fseek (pCmdState->fpCommandInput, 0, SEEK_SET);

        unsigned int uiEchoOn = pCmdState->uiFlags & CMD_ECHO_ON;
        pCmdState->uiFlags &= ~CMD_ECHO_ON;

        while (cmd_GetInput()) {
            if ((pCmdState->lpBufInput[0] == TEXT(':')) &&
                            (_tcsicmp (&pCmdState->lpBufInput[1], lpArgs) == 0)) {              
                pCmdState->uiFlags |= uiEchoOn;
                return CMD_SUCCESS;
            }
        }

        pCmdState->uiFlags |= uiEchoOn;
    }

    cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, lpName, lpArgs);
    return CMD_ERROR;
}

int cmdexec_CmdIf (TCHAR *lpName, TCHAR *lpArgs) {
    int fNeg = FALSE;

    TCHAR *lpToken = lpArgs;
    lpArgs += cmdutil_CountNonWhite (lpArgs);

    TCHAR tcSav = *lpArgs;

    *lpArgs = TEXT('\0');

    if (_tcsicmp(lpToken, TEXT("NOT")) == 0) {
        fNeg = TRUE;
        ++lpArgs;
        lpArgs += cmdutil_CountWhite (lpArgs);
        lpToken = lpArgs;
    } else
        *lpArgs = tcSav;

    lpArgs = lpToken + cmdutil_CountNonWhiteWithQuotes (lpToken);

    tcSav = *lpArgs;
    *lpArgs = TEXT('\0');

    //
    //  Parse it differently for now. Compute condition...
    //
    //  lpArgs and tcSav should be well defined on the exit of the loop...
    //
    int fCond;

    if (_tcsicmp (lpToken, TEXT("DEFINED")) == 0) {             // Defined
        *lpArgs = tcSav;

        lpArgs += cmdutil_CountWhite (lpArgs);
        lpToken = lpArgs;

        lpArgs  += cmdutil_CountNonWhiteWithQuotes (lpArgs);
        
        tcSav = *lpArgs;
        *lpArgs = TEXT('\0');

        _tcsupr_s (lpToken, _tcslen(lpToken)+1);

        fCond = (pCmdState->ehHash.hash_get (lpToken) != NULL);
    } else if (_tcsicmp (lpToken, TEXT("EXIST")) == 0) {        // exist
        *lpArgs = tcSav;

        lpArgs += cmdutil_CountWhite (lpArgs);
        lpToken = lpArgs;

        lpArgs  += cmdutil_CountNonWhiteWithQuotes (lpArgs);
        
        tcSav = *lpArgs;
        *lpArgs = TEXT('\0');

        cmdutil_RemoveQuotes (lpToken);

        TCHAR sBuffer[CMD_BUF_REG];
        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, lpToken, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpToken);
            return CMD_ERROR;
        }

        fCond = cmdutil_ExistsFile (sBuffer);
    } else if (_tcsicmp (lpToken, TEXT("ERRORLEVEL")) == 0) {   // errorlevel
        *lpArgs = tcSav;

        lpArgs += cmdutil_CountWhite (lpArgs);
        lpToken = lpArgs;

        lpArgs  += cmdutil_CountNonWhiteWithQuotes (lpArgs);
        
        tcSav = *lpArgs;
        *lpArgs = TEXT('\0');

        if ((lpArgs - lpToken != 1) || (*lpToken < TEXT('0')) || (*lpToken > TEXT('9'))) {
            cmdutil_Complain (IDS_ERROR_IF_BADERRLEVEL, lpName, lpToken);
            return CMD_ERROR;
        }
        
        int iError = *lpToken - TEXT('0');
        fCond = iError <= pCmdState->iError;
    } else {                                                    // must be string comp.
        // The token can contain the whole expression (string1==string2)
        // or only part of it (string1== string2)
        // or only the first string (string1 == string2)
        TCHAR *lpEQEQ = _tcsstr (lpToken, TEXT("=="));

        if (lpEQEQ) {
            *lpEQEQ = TEXT('\0');
            *lpArgs = tcSav;
        } else if (tcSav == TEXT('\0')) {
            cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
            return CMD_ERROR;
        } else {
            ++lpArgs;
            lpEQEQ = _tcsstr (lpArgs, TEXT("=="));
            if (! lpEQEQ) {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                return CMD_ERROR;
            }
        }

        lpArgs = lpEQEQ + 2;

        lpArgs += cmdutil_CountWhite (lpArgs);
        TCHAR *lpToken2 = lpArgs;

        lpArgs  += cmdutil_CountNonWhiteWithQuotes (lpArgs);
        
        tcSav = *lpArgs;
        *lpArgs = TEXT('\0');

        fCond = _tcscmp (lpToken, lpToken2) == 0;
    }

    if (((! fCond) && (! fNeg)) || (fCond && fNeg))
        return CMD_SUCCESS;

    *lpArgs = tcSav;
    lpArgs += cmdutil_CountWhite (lpArgs);

    int iSz = _tcslen (lpArgs) + 1;

    if (iSz == 1)
        return CMD_SUCCESS;

    ASSERT ((lpArgs >= pCmdState->lpBufCmd) && (lpArgs + iSz < pCmdState->lpBufCmd + pCmdState->iBufCmdChars));

    memmove (pCmdState->lpBufCmd, lpArgs, iSz * sizeof(TCHAR));

    cmdexec_ResetFields ();

    if (! cmdexec_FactorCommand ())
        return CMD_SUCCESS;

    return cmdexec_Dispatch ();
}

int cmdexec_CmdStartRunCmd (TCHAR *lpCmdStart, TCHAR *lpArgStart) {
    TCHAR szCmdName[MAX_PATH];
    if (! GetModuleFileName (pCmdState->hInstance, szCmdName, MAX_PATH))
        return CMD_SUCCESS;

    TCHAR *pszCmdName = _tcsrchr (szCmdName, TEXT('\\'));
    if (! pszCmdName)
        pszCmdName = szCmdName;
    else
        ++pszCmdName;

    size_t cchCommandLine = 0;
    TCHAR* pszCommandLine = NULL;
    if (*lpCmdStart != TEXT('\0')) {
        cchCommandLine = sizeof(TCHAR) * (5 + _tcslen(lpCmdStart) + _tcslen(lpArgStart));
        pszCommandLine = (TCHAR *)cmdutil_Alloc (cchCommandLine);
    }

    if (pszCommandLine) {
        pszCommandLine[0] = TEXT('/');
        pszCommandLine[1] = TEXT('K');
        pszCommandLine[2] = TEXT(' ');
        StringCchCat(pszCommandLine, cchCommandLine, lpCmdStart);
        StringCchCat(pszCommandLine, cchCommandLine, TEXT(" "));
        StringCchCat(pszCommandLine, cchCommandLine, lpArgStart);
    }

    int iRet = cmdexec_TryProgram (pszCmdName, pszCommandLine ? pszCommandLine : TEXT(""), TRUE);

    if (pszCommandLine)
        cmdutil_Free (pszCommandLine);

    return iRet;
}

int cmdexec_CmdStart (TCHAR *lpName, TCHAR *lpArgs) {
    cmdexec_ParseCmdAndArgs (lpArgs);

    if ((*pCmdState->lpCmdStart == TEXT('\0')) || (cmdexec_FindCommand (pCmdState->lpCmdStart) != -1))
        return cmdexec_CmdStartRunCmd (pCmdState->lpCmdStart, pCmdState->lpArgStart);

    return cmdexec_TryProgram (pCmdState->lpCmdStart, pCmdState->lpArgStart, TRUE);
}

int cmdexec_CmdCall (TCHAR *lpName, TCHAR *lpArgs) {
    cmdexec_ParseCmdAndArgs (lpArgs);

    if (*pCmdState->lpCmdStart == TEXT('\0'))
        return CMD_SUCCESS;

    int iResult = cmdexec_TryProgram (pCmdState->lpCmdStart, pCmdState->lpArgStart, FALSE);

    pCmdState->uiFlags &= ~CMD_EXEC_SKIPDOWN;

    return iResult;
}

int cmdexec_CmdDump (TCHAR *lpName, TCHAR *lpArgs) {
    _ftprintf (stderr, TEXT("\n\nCMD STATE DUMP:\n"));
    _ftprintf (stderr, TEXT("ECHO %s\n"),
            pCmdState->uiFlags & CMD_ECHO_ON ? TEXT("ON") : TEXT("OFF"));
    _ftprintf (stderr, TEXT("CTRLC %s\n"),
            pCmdState->uiFlags & CMD_EXEC_CTRLC ? TEXT("ACTIVE") : TEXT("NONE"));
    _ftprintf (stderr, TEXT("EXIT %s\n"),
            pCmdState->uiFlags & CMD_EXEC_EXIT ? TEXT("ACTIVE") : TEXT("NONE"));
    _ftprintf (stderr, TEXT("ERROR: %d\n"), pCmdState->iError);
    _ftprintf (stderr, TEXT("FCTRLC: %d\n"), pCmdState->fCtrlC);
    _ftprintf (stderr, TEXT("SCREEN: %dx%d\n"), pCmdState->xMax, pCmdState->yMax);
    _ftprintf (stderr, TEXT("BUFFERS: input %d cmd %d   proc %d\n"), pCmdState->iBufInputChars,
                pCmdState->iBufCmdChars, pCmdState->iBufProcChars);
    _ftprintf (stderr, TEXT("CURRENT REDIRECTIONS:\n"));
    _ftprintf (stderr, TEXT("\tSTDIN  = [%s]\n"), pCmdState->lpIO[CMDIO_IN] ? pCmdState->lpIO[CMDIO_IN] : TEXT("NULL"));
    _ftprintf (stderr, TEXT("\tSTDOUT = [%s]\n"), pCmdState->lpIO[CMDIO_OUT] ? pCmdState->lpIO[CMDIO_OUT] : TEXT("NULL"));
    _ftprintf (stderr, TEXT("\tSTDERR = [%s]\n"), pCmdState->lpIO[CMDIO_ERR] ? pCmdState->lpIO[CMDIO_ERR] : TEXT("NULL"));

    return pCmdState->iError;
}

int cmdexec_CmdDebug (TCHAR *lpName, TCHAR *lpArgs) {
#if defined (UNDER_CE)
    DebugBreak ();
#endif
    return CMD_SUCCESS;
}

int cmdexec_CmdShift (TCHAR *lpName, TCHAR *lpArgs) {
    if (pCmdState->lpCommandInput && pCmdState->pplBatchArgs)
        pCmdState->pplBatchArgs = pCmdState->pplBatchArgs->pNext;

    return CMD_SUCCESS;
}

int cmdexec_CmdRem (TCHAR *lpName, TCHAR *lpArgs) {
    return CMD_SUCCESS;
}

int cmdexec_CmdPause (TCHAR *lpName, TCHAR *lpArgs) {
    cmdutil_Confirm (IDS_ANYKEY);
    return CMD_SUCCESS;
}

struct CopyHelperParms {
    TCHAR   *lpBuffer;
    int     iDirLen;
    int     iBufSize;
    int     *piCopied;
};

struct CopyHelperParms2 {
    HANDLE hFile;
    int *piCopied;
};

int cmdexec_CmdCopyHelperToFile (TCHAR *lpFileName, void *pvData) {
    if (cmdutil_IsInvisible (lpFileName)) {
        cmdutil_Complain (IDS_ERROR_COPY_CANT1, lpFileName);
        return CMD_SUCCESS;
    }

    HANDLE hFile = ((CopyHelperParms2 *)pvData)->hFile;
    HANDLE hSource = CreateFile (lpFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);


    if (hSource == INVALID_HANDLE_VALUE) {
        cmdutil_Complain (IDS_ERROR_GENERAL_CANTOPEN, TEXT("COPY"), lpFileName);
        return CMD_ERROR;
    }               

    char cBuffer[4*CMD_BUF_HUGE];
    for ( ; ; ) {
        DWORD dwActualBytes = 0;
        ReadFile (hSource, cBuffer, sizeof(cBuffer), &dwActualBytes, NULL);
        
        if (! dwActualBytes)
            break;

        DWORD dwActuallyWritten = 0;

        if (! WriteFile(hFile, cBuffer, dwActualBytes, &dwActuallyWritten, NULL)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_IOFAILED);
            return CMD_ERROR;
        }
    }

    FILETIME ftCreat, ftAccess, ftWrite;

    GetFileTime (hSource, &ftCreat, &ftAccess, &ftWrite);
    SetFileTime (hFile,  &ftCreat, &ftAccess, &ftWrite);

    CloseHandle (hSource);
    
    ++*((CopyHelperParms2 *)pvData)->piCopied;
    return CMD_SUCCESS;
}

int cmdexec_CmdCopyHelperToDir (TCHAR *lpFileName, void *pvData) 
{
    if (cmdutil_IsInvisible (lpFileName)) {
        cmdutil_Complain (IDS_ERROR_COPY_CANT1, lpFileName);
        return CMD_SUCCESS;
    }

    CopyHelperParms *pCHP = (CopyHelperParms *)pvData;
    TCHAR *lpShortName = _tcsrchr (lpFileName, TEXT('\\'));

    ASSERT (lpShortName);
    ASSERT (pCHP->iDirLen > 0);

    if (pCHP->lpBuffer[pCHP->iDirLen - 1] == TEXT('\\'))
        ++lpShortName;

    int iSize = _tcslen (lpShortName) + 1;
    
    UINT uLen = 0;    
    if (FAILED(UIntAdd(iSize, pCHP->iDirLen, &uLen)) || ((int)uLen > pCHP->iBufSize)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_PATHTOOBIG, lpFileName);
        return CMD_ERROR_TOOBIG;
    }

    memcpy (&pCHP->lpBuffer[pCHP->iDirLen], lpShortName, sizeof(TCHAR) * iSize);

    if (_tcsicmp (lpFileName, pCHP->lpBuffer) == 0) {
        cmdutil_Complain (IDS_ERROR_COPY_SELF, lpFileName);
        return CMD_ERROR;
    }

    int fRes = CopyFile (lpFileName, pCHP->lpBuffer, FALSE);
    pCHP->lpBuffer[pCHP->iDirLen] = TEXT('\0');

    if (! fRes) {
        cmdutil_Complain (IDS_ERROR_COPY_CANT, lpFileName, pCHP->lpBuffer);
        return CMD_ERROR;
    }

    ++*pCHP->piCopied;
    return CMD_SUCCESS;
}

void cmdexec_CmdCopyRemovePluses (TCHAR *lpArgs) {
    int pInQuotes = FALSE;

    while (*lpArgs) {
        if (*lpArgs == TEXT('"'))
            pInQuotes = !pInQuotes;
        else if (*lpArgs == TEXT('+'))
            *lpArgs = TEXT(' ');
        ++lpArgs;
    }
}

int cmdexec_CmdCopy (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;

    cmdexec_CmdCopyRemovePluses (lpArgs);

    int iNum = cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);
    int iCopied = 0;

    PointerList *pplRun;

    if (pplOptions || (! pplFiles) || ((iNum & CMD_PARSER_FILENUM) < 1)) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplFiles);
        cmdutil_FixedFreePointerList (pplOptions);
        return CMD_ERROR;
    }

    TCHAR *lpTarget = TEXT(".");

    if ((iNum & CMD_PARSER_FILENUM) > 1) {
        pplRun = pplFiles;

        while (pplRun) {
            lpTarget = (TCHAR *)pplRun->pData;
            pplRun   = pplRun->pNext;
        }
    }

    TCHAR sBufDest[CMD_BUF_REG];

    if (! cmdutil_FormPath (sBufDest, CMD_BUF_REG, lpTarget, pCmdState->sCwd)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpTarget);
        cmdutil_FixedFreePointerList (pplFiles);
        
        return CMD_ERROR;
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;
    CopyHelperParms  chp;
    CopyHelperParms2 chp2;

    if (! cmdutil_ExistsDir (sBufDest)) {
        pplRun = pplFiles;

        while (pplRun && (pplRun->pData != lpTarget)) {
            TCHAR sBuffer[CMD_BUF_REG];
            if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
                cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
                return CMD_ERROR;
            }

            if (_tcsicmp (sBuffer, sBufDest) == 0) {
                cmdutil_Complain (IDS_ERROR_COPY_SELF, sBufDest);
                return CMD_ERROR;
            }
            //"copy file1 file2".  If file2 exists and file1 does not,
            //file2 will be deleted.  If the source file (file1) is not found do 
            //not delete the destination file (file2).  Note that multiple source
            //files may be specified as in "copy a.txt+b.txt c.txt".
            if (FALSE == cmdutil_ExistsFile (sBuffer)) {
                cmdutil_Complain (IDS_ERROR_FNF, sBuffer);
                return CMD_ERROR;
            }

            pplRun = pplRun->pNext;
        }

        //"copy file1 file2".  If file2 exixts, it will be overwritten without asking
        //the user because of CREATE_ALWAYS.
        if ((cmdutil_ExistsFile (sBufDest) && cmdutil_IsInvisible (sBufDest)) ||
            (INVALID_HANDLE_VALUE == (hFile = CreateFile (sBufDest, GENERIC_WRITE,
                                        0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)))) {
            cmdutil_Complain (IDS_ERROR_GENERAL_CANTOPEN, lpName, sBufDest);
            return CMD_ERROR;
        }
        
        chp2.hFile = hFile;
        chp2.piCopied = &iCopied;
    } else {
        chp.lpBuffer = sBufDest;
        chp.iDirLen  = _tcslen (sBufDest);
        chp.iBufSize = CMD_BUF_REG;
        chp.piCopied = &iCopied;
    }

    pplRun = pplFiles;
    int iRes = CMD_SUCCESS;

    while (pplRun && (pplRun->pData != lpTarget)) {
        TCHAR sBuffer[CMD_BUF_REG];
        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }

        if (hFile != INVALID_HANDLE_VALUE)
            iRes = cmdutil_ExecuteForPattern (sBuffer, CMD_BUF_REG, &chp2,
                                    FALSE, cmdexec_CmdCopyHelperToFile);
        else
            iRes = cmdutil_ExecuteForPattern (sBuffer, CMD_BUF_REG, &chp,
                                    FALSE, cmdexec_CmdCopyHelperToDir);

        if (iRes == CMD_ERROR_FNF)
            cmdutil_Complain (IDS_ERROR_FNF, sBuffer);

        if (iRes != CMD_SUCCESS)
            break;

        pplRun = pplRun->pNext;
    }

    cmdutil_FixedFreePointerList (pplFiles);

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle (hFile);

    if (iRes == CMD_SUCCESS)
        cmdutil_Complain (IDS_CMD_COPY_STATS, iCopied);
    
    if ((hFile != INVALID_HANDLE_VALUE) && ((! iCopied) || (iRes != CMD_SUCCESS)))
        DeleteFile (sBufDest);

    return iRes;
}

int cmdexec_CmdPrompt (TCHAR *lpName, TCHAR *lpArgs) {
    int iSize = _tcslen (lpArgs) + 1;
    
    if (iSize == 1)
        return CMD_SUCCESS;

    if (iSize > CMD_BUF_SMALL) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        return CMD_ERROR;
    }

    memcpy (pCmdState->sPrompt, lpArgs, iSize * sizeof(TCHAR));
    
    return CMD_SUCCESS;
}

int cmdexec_CmdRen (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int iNum = cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    if (pplOptions || ((iNum & CMD_PARSER_FILENUM) != 2)) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplOptions);
        cmdutil_FixedFreePointerList (pplFiles);

        return CMD_ERROR;
    }

    TCHAR *lpSource = (TCHAR *)pplFiles->pData;
    TCHAR *lpDest   = (TCHAR *)pplFiles->pNext->pData;

    cmdutil_FixedFreePointerList (pplFiles);

    TCHAR sBufSrc[CMD_BUF_REG];

    if (! cmdutil_FormPath (sBufSrc, CMD_BUF_REG, lpSource, pCmdState->sCwd)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpSource);
        return CMD_ERROR;
    }

    TCHAR sBufDst[CMD_BUF_REG];

    if (_tcschr (lpDest, TEXT('\\'))) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpDest);
        return CMD_ERROR;
    } else {
        TCHAR *lpDirSrc = _tcsrchr (sBufSrc, TEXT('\\'));

        if (lpDirSrc == sBufSrc)
            ++lpDirSrc;

        TCHAR c = *lpDirSrc;
        *lpDirSrc = L'\0';

        if (! cmdutil_FormPath (sBufDst, CMD_BUF_REG, lpDest, sBufSrc)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpDest);
            return CMD_ERROR;
        }

        *lpDirSrc = c;
    }

    TCHAR *lp1 = _tcsrchr (sBufSrc, TEXT('\\'));
    TCHAR *lp2 = _tcsrchr (sBufDst, TEXT('\\'));

    if ((! lp1) || (! lp2) || ((lp1 - sBufSrc) != (lp2 - sBufDst)) ||
        _tcsnicmp (sBufSrc, sBufDst, lp1 - sBufSrc) ||
        cmdutil_IsInvisible (sBufSrc) ||
        (! MoveFile (sBufSrc, sBufDst))) {
        cmdutil_Complain (IDS_ERROR_MOVE_CANT, sBufSrc, sBufDst);
        return CMD_ERROR;
    }
    return CMD_SUCCESS;
}

struct MoveHelperParms {
    TCHAR   *lpBuffer;
    int     iDirLen;
    int     iBufSize;
};

int cmdexec_MoveHelper (TCHAR *lpFileName, void *pvData) 
{
    if (cmdutil_IsInvisible (lpFileName)) {
        cmdutil_Complain (IDS_ERROR_MOVE_CANT1, lpFileName);
        return CMD_SUCCESS;
    }

    MoveHelperParms *pMHP = (MoveHelperParms *)pvData;
    TCHAR *lpShortName = _tcsrchr (lpFileName, TEXT('\\'));

    ASSERT (lpShortName);
    ASSERT (pMHP->iDirLen > 0);

    if (pMHP->lpBuffer[pMHP->iDirLen - 1] == TEXT('\\'))
        ++lpShortName;

    int iSize = _tcslen (lpShortName) + 1;

    UINT uLen = 0;
    if (FAILED(UIntAdd(iSize, pMHP->iDirLen, &uLen)) || ((int)uLen > pMHP->iBufSize)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_PATHTOOBIG, lpFileName);
        return CMD_ERROR_TOOBIG;
    }

    memcpy (&pMHP->lpBuffer[pMHP->iDirLen], lpShortName, sizeof(TCHAR) * iSize);

    int fRes = MoveFile (lpFileName, pMHP->lpBuffer);
    pMHP->lpBuffer[pMHP->iDirLen] = TEXT('\0');

    if (! fRes) {
        cmdutil_Complain (IDS_ERROR_MOVE_CANT, lpFileName, pMHP->lpBuffer);
        return CMD_ERROR;
    }

    return CMD_SUCCESS;
}

int cmdexec_CmdMove (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int iNum = cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    if (pplOptions || ((iNum & CMD_PARSER_FILENUM) != 2)) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplOptions);
        cmdutil_FixedFreePointerList (pplFiles);

        return CMD_ERROR;
    }

    TCHAR *lpSource = (TCHAR *)pplFiles->pData;
    TCHAR *lpDest   = (TCHAR *)pplFiles->pNext->pData;

    cmdutil_FixedFreePointerList (pplFiles);

    if (cmdutil_HasWildCards (lpDest)) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        return CMD_ERROR;
    }

    TCHAR sBufSrc[CMD_BUF_REG];

    if (! cmdutil_FormPath (sBufSrc, CMD_BUF_REG, lpSource, pCmdState->sCwd)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpSource);
        return CMD_ERROR;
    }

    TCHAR sBufDst[CMD_BUF_REG];

    if (! cmdutil_FormPath (sBufDst, CMD_BUF_REG, lpDest, pCmdState->sCwd)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpDest);
        return CMD_ERROR;
    }

    MoveHelperParms mhp;

    mhp.lpBuffer = sBufDst;
    mhp.iDirLen  = _tcslen (sBufDst);
    mhp.iBufSize = CMD_BUF_REG;

    if (cmdutil_HasWildCards (lpSource)) {
        if (! cmdutil_ExistsDir (sBufDst)) {
            cmdutil_Complain (IDS_ERROR_MOVE_MULTSOURCES, sBufDst);
            return CMD_ERROR;
        }

        int iRes = cmdutil_ExecuteForPattern (sBufSrc, CMD_BUF_REG, &mhp,
                                                            FALSE, cmdexec_MoveHelper);

        if (iRes == CMD_ERROR_FNF)
            cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, lpName, sBufSrc);

        return iRes;
    }

    if (cmdutil_ExistsDir (sBufSrc)) {
        if (cmdutil_ExistsDir (sBufDst)) {
            TCHAR *lpShortName = _tcsrchr (sBufSrc, TEXT('\\'));

            ASSERT (lpShortName);
            int cDestLen  = _tcslen (sBufDst);

            ASSERT (cDestLen > 0);

            if (sBufDst[cDestLen - 1] == TEXT('\\'))
                ++lpShortName;

            int cShortLen = _tcslen (lpShortName) + 1;

            if (cShortLen + cDestLen < CMD_BUF_REG)
                memcpy (&sBufDst[cDestLen], lpShortName, sizeof(WCHAR) * cShortLen);
        }

        if (cmdutil_IsInvisible (sBufSrc) || (! MoveFile (sBufSrc, sBufDst))) {
            cmdutil_Complain (IDS_ERROR_MOVE_CANT, sBufSrc, sBufDst);
            return CMD_ERROR;
        }

        return CMD_SUCCESS;
    }

    if (cmdutil_ExistsDir (sBufDst))
        return cmdexec_MoveHelper (sBufSrc, &mhp);
    else {
        if (cmdutil_IsInvisible (sBufSrc) || (! MoveFile (sBufSrc, sBufDst))) {
            cmdutil_Complain (IDS_ERROR_MOVE_CANT, sBufSrc, sBufDst);
            return CMD_ERROR;
        }
    }
    
    return CMD_SUCCESS;
}

#if defined (SHOWERRORS)
#error Locale formatting is ignored. Fix later.
#endif
void cmdexec_DisplayTime (SYSTEMTIME *pST, int fLongFormat) {
    TCHAR   sBuffer[CMD_BUF_SMALL];

    if (fLongFormat) {
        LoadString (pCmdState->hInstance, IDS_TIME_PREFIX, sBuffer, CMD_BUF_SMALL);
        _fputts (sBuffer, STDOUT);
    }

    GetTimeFormat (LOCALE_USER_DEFAULT, 0, pST, NULL, sBuffer, CMD_BUF_SMALL);
    _fputts (sBuffer, STDOUT);
    //_ftprintf (STDOUT, TEXT(" %02d:%02d:%02d.%02d\n"), pST->wHour, pST->wMinute, pST->wSecond,
    //                                  pST->wMilliseconds / 10);

    if (fLongFormat) {
        LoadString (pCmdState->hInstance, IDS_TIME_POSTFIX, sBuffer, CMD_BUF_SMALL);
        _fputts (sBuffer, STDOUT);
    } else
        _fputts (TEXT("\n"), STDOUT);
}

void cmdexec_DisplayDate (SYSTEMTIME *pST, int fLongFormat) {
    TCHAR   sBuffer[CMD_BUF_SMALL];

    if (fLongFormat) {
        LoadString (pCmdState->hInstance, IDS_DATE_PREFIX, sBuffer, CMD_BUF_SMALL);
        _fputts (sBuffer, STDOUT);
    }

    GetDateFormat (LOCALE_USER_DEFAULT, DATE_LONGDATE, pST, NULL, sBuffer, CMD_BUF_SMALL);
    _fputts (sBuffer, STDOUT);
    //  _ftprintf (STDOUT, TEXT("%s %02d/%02d/%04d\n"), cmdutil_DayOfWeek(pST->wDayOfWeek), pST->wMonth, pST->wDay, pST->wYear);

    if (fLongFormat) {
        LoadString (pCmdState->hInstance, IDS_DATE_POSTFIX, sBuffer, CMD_BUF_SMALL);
        TCHAR sBuffer2[CMD_BUF_TINY];

        TCHAR szDateFormat[2];
        int iForm = IDS_DATE_FORM3;

        if ((! GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, szDateFormat, 2)) || (szDateFormat[0] == TEXT('0')))
            iForm = IDS_DATE_FORM1;
        else if (szDateFormat[0] == TEXT('1'))
            iForm = IDS_DATE_FORM2;

        LoadString (pCmdState->hInstance, iForm, sBuffer2, CMD_BUF_TINY);
        _ftprintf (STDOUT, sBuffer, sBuffer2);
    } else
        _fputts (TEXT("\n"), STDOUT);
}

int cmdexec_Get2Digits (TCHAR *&pDigit) {
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return -1;
    
    int iRes = *pDigit - TEXT('0');

    ++pDigit;
    
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return iRes;

    iRes = iRes * 10 + *pDigit - TEXT('0');
    ++pDigit;
    
    return iRes;
}

int cmdexec_Get4Digits (TCHAR *&pDigit) {
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return -1;
    
    int iRes = *pDigit - TEXT('0');

    ++pDigit;
    
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return iRes;

    iRes = iRes * 10 + *pDigit - TEXT('0');
    ++pDigit;
    
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return iRes;

    iRes = iRes * 10 + *pDigit - TEXT('0');
    ++pDigit;
    
    if (*pDigit < TEXT('0') || *pDigit > TEXT('9'))
        return iRes;

    iRes = iRes * 10 + *pDigit - TEXT('0');
    ++pDigit;
    
    return iRes;
}

int cmdexec_CmdTime (TCHAR *lpName, TCHAR *lpArgs) {
    SYSTEMTIME st;
    GetLocalTime (&st);

    if (_tcsicmp (lpArgs, TEXT("/T")) == 0) {
        cmdexec_DisplayTime (&st, FALSE);
        return CMD_SUCCESS;
    }

    TCHAR sBuffer[CMD_BUF_REG];

    if (*lpArgs == TEXT('\0')) {
        cmdexec_DisplayTime (&st, TRUE);
        _fgetts (sBuffer, CMD_BUF_REG, STDIN);
        lpArgs = sBuffer;
    }

    if (*lpArgs == TEXT('\n'))
        return CMD_SUCCESS;

    int iHours = cmdexec_Get2Digits (lpArgs);

    for ( ; ; ) {
        if (iHours > 23 || iHours < 0)
            break;

        if (*lpArgs != TEXT(':'))
            break;

        ++lpArgs;

        int iMinutes = cmdexec_Get2Digits (lpArgs);

        if (iMinutes > 59 || iMinutes < 0)
            break;

        int iSeconds;
        if (*lpArgs == TEXT(':')) {
            ++lpArgs;
            iSeconds = cmdexec_Get2Digits (lpArgs);

            if (iSeconds > 59 || iSeconds < 0)
                break;
            
            if ((*lpArgs != TEXT('\n')) && (*lpArgs != TEXT('\0')) && (*lpArgs != TEXT('.')))
                break;

        } else if ((*lpArgs == TEXT('\n')) || (*lpArgs == TEXT('\0')))
            iSeconds = 0;
        else
            break;

        GetLocalTime (&st);
        
        st.wHour            = (WORD)iHours;
        st.wMinute          = (WORD)iMinutes;
        st.wSecond          = (WORD)iSeconds;
        st.wMilliseconds    = 0;
        if (SetLocalTime (&st))
            return CMD_SUCCESS;
    }

    cmdutil_Complain (IDS_ERROR_TIME_BAD);
    return CMD_ERROR;
}

int cmdexec_CmdDate (TCHAR *lpName, TCHAR *lpArgs) {
    SYSTEMTIME st;
    GetLocalTime (&st);

    if (_tcsicmp (lpArgs, TEXT("/T")) == 0) {
        cmdexec_DisplayDate (&st, FALSE);
        return CMD_SUCCESS;
    }

    TCHAR sBuffer[CMD_BUF_REG];

    if (*lpArgs == TEXT('\0')) {
        cmdexec_DisplayDate (&st, TRUE);
        _fgetts (sBuffer, CMD_BUF_REG, STDIN);
        lpArgs = sBuffer;
    }

    if (*lpArgs == TEXT('\n'))
        return CMD_SUCCESS;

    TCHAR szDateFormat[2];
    if (! GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, szDateFormat, 2))
        szDateFormat[0] = TEXT('0');

    for ( ; ; ) {
        int iMonth = -1;
        int iDay   = -1;
        int iYear  = -1;

        if (szDateFormat[0] == TEXT('0')) {
            iMonth = cmdexec_Get2Digits (lpArgs);

            if ((*lpArgs != TEXT('/')) && (*lpArgs != TEXT('-')))
                break;

            ++lpArgs;

            iDay = cmdexec_Get2Digits (lpArgs);
        
            if ((*lpArgs == TEXT('/')) || (*lpArgs == TEXT('-'))) {
                ++lpArgs;
                iYear = cmdexec_Get4Digits (lpArgs);

                if (iYear < 0)
                    break;
            }
        } else if (szDateFormat[0] == TEXT('1')) {
            iDay = cmdexec_Get2Digits (lpArgs);

            if ((*lpArgs != TEXT('/')) && (*lpArgs != TEXT('-')))
                break;

            ++lpArgs;

            iMonth = cmdexec_Get2Digits (lpArgs);
        
            if ((*lpArgs == TEXT('/')) || (*lpArgs == TEXT('-'))) {
                ++lpArgs;
                iYear = cmdexec_Get4Digits (lpArgs);

                if (iYear < 0)
                    break;
            }
        } else {
            iYear = cmdexec_Get4Digits (lpArgs);

            if ((*lpArgs != TEXT('/')) && (*lpArgs != TEXT('-')))
                break;

            ++lpArgs;

            iMonth = cmdexec_Get2Digits (lpArgs);

            if ((*lpArgs != TEXT('/')) && (*lpArgs != TEXT('-'))) {
                iDay   = iMonth;
                iMonth = iYear;
                iYear  = -1;
            } else {
                ++lpArgs;

                iDay = cmdexec_Get2Digits (lpArgs);
            }
        }

        if ((*lpArgs != TEXT('\n')) && (*lpArgs != TEXT('\0')))
            break;

        if (iMonth > 12 || iMonth < 1)
            break;

        if (iDay < 1 || iDay > 31)
            break;

        if (iYear >= 0) {
            if (iYear < 79)
                iYear += 2000;
            else if (iYear < 100)
                iYear += 2000;
        }
        
        GetLocalTime (&st);
        
        if (iYear >= 0)
            st.wYear = (WORD)iYear;

        st.wMonth   = (WORD)iMonth;
        st.wDay     = (WORD)iDay;

        if (SetLocalTime (&st))
            return CMD_SUCCESS;
    }

    cmdutil_Complain (IDS_ERROR_DATE_BAD);
    return CMD_ERROR;
}

struct AttribPack {
    DWORD fAttribIn;
    DWORD fAttribOut;
};

int cmdexec_AttribHelper (TCHAR *lpName, void *pData) {
    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    AttribPack *pAP = (AttribPack *)pData;

    DWORD fAttrib = GetFileAttributes (lpName);
        
    if (fAttrib == -1) {
        cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, TEXT("ATTRIB"), lpName);
        return CMD_ERROR;
    }
        
    if (pAP->fAttribIn | pAP->fAttribOut) {
        fAttrib |= pAP->fAttribIn;
        fAttrib &= ~pAP->fAttribOut;

        if (! SetFileAttributes (lpName, fAttrib)) {
            cmdutil_Complain (IDS_ERROR_ATTRIB_CANT, lpName);
            return CMD_ERROR;
        }
    } else {
        TCHAR sAttribs[CMD_BUF_COMMAND];
        memcpy (sAttribs, TEXT("        "), sizeof(TEXT("        ")));

        if (fAttrib & FILE_ATTRIBUTE_ARCHIVE)
            sAttribs[0] = TEXT('A');

        if (fAttrib & FILE_ATTRIBUTE_HIDDEN)
            sAttribs[4] = TEXT('H');

        if (fAttrib & FILE_ATTRIBUTE_SYSTEM)
            sAttribs[3] = TEXT('S');

        if (fAttrib & FILE_ATTRIBUTE_READONLY)
            sAttribs[5] = TEXT('R');

        _ftprintf (STDOUT, TEXT("%s %s\n"), sAttribs, lpName);
    }

    return CMD_SUCCESS;
}

int cmdexec_CmdAttrib (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    

    int fRecursion = FALSE;
    AttribPack ap;

    cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);
    
    ap.fAttribIn  = 0;
    ap.fAttribOut = 0;
    
    TCHAR *lpPattern = NULL;

    if (pplOptions) {
        if (pplOptions->pNext || (_tcsicmp ((TCHAR *)pplOptions->pData, TEXT("/S")) != 0)) {
            cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
            cmdutil_FixedFreePointerList (pplOptions);
            cmdutil_FixedFreePointerList (pplFiles);
            return CMD_ERROR;
        }
        fRecursion = TRUE;
        cmdutil_FixedFreePointerList (pplOptions);
    }

    if (pplFiles) {
        PointerList *pplRun = pplFiles;
        while (pplRun) {
            DWORD *piWhatType;
            TCHAR *lpString = (TCHAR *)pplRun->pData;

            if (*lpString == TEXT('-'))
                piWhatType = &ap.fAttribOut;
            else if (*lpString == TEXT('+'))
                piWhatType = &ap.fAttribIn;
            else if (lpPattern) {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                cmdutil_FixedFreePointerList (pplFiles);
                return CMD_ERROR;
            } else {
                lpPattern = lpString;
                pplRun = pplRun->pNext;
                continue;
            }

            ++lpString;

            if (lpString[1] != TEXT('\0')) {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                cmdutil_FixedFreePointerList (pplFiles);
                return CMD_ERROR;
            }

            switch (*lpString) {
                case TEXT('R'):
                case TEXT('r'):
                    *piWhatType |= FILE_ATTRIBUTE_READONLY;
                    break;

                case TEXT('A'):
                case TEXT('a'):
                    *piWhatType |= FILE_ATTRIBUTE_ARCHIVE;
                    break;

                case TEXT('S'):
                case TEXT('s'):
                    *piWhatType |= FILE_ATTRIBUTE_SYSTEM;
                    break;

                case TEXT('H'):
                case TEXT('h'):
                    *piWhatType |= FILE_ATTRIBUTE_HIDDEN;
                    break;

                default:
                    cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                    cmdutil_FixedFreePointerList (pplFiles);
                    return CMD_ERROR;
            }
            pplRun = pplRun->pNext;

        }
        cmdutil_FixedFreePointerList (pplFiles);
    }

    if (! lpPattern)
        lpPattern = PATTERN_ALL + 1;

    TCHAR sBuffer[CMD_BUF_REG];

    if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, lpPattern, pCmdState->sCwd)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpPattern);
        return CMD_ERROR;
    }

    if ((! cmdutil_HasWildCards(lpPattern)) && cmdutil_ExistsDir (sBuffer))
        return cmdexec_AttribHelper (sBuffer, &ap);

    int iRes = cmdutil_ExecuteForPattern (sBuffer, CMD_BUF_REG, &ap,
                                            fRecursion,
                                            cmdexec_AttribHelper);
    
    if (iRes == CMD_ERROR_FNF)
        cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, lpName, sBuffer);

    return iRes;
}

void cmdexec_MaintainPause (int &iCurrentLine, int iIncrement, int fOnNewLine) {
    if (iCurrentLine + iIncrement + 3 > pCmdState->yMax) {
        if (! fOnNewLine)
            _fputts (TEXT("\n"), STDOUT);

        cmdutil_Confirm (IDS_ANYKEY);
        iCurrentLine = 0;
    }
    iCurrentLine += iIncrement;
}

int cmdexec_MergeDirOptions
(
PointerList *pplExplicitOpts,
int &fRecurse,
unsigned int &fAttribIn,
unsigned int &fAttribOut,
TCHAR *&lpSort
) {
    fRecurse   = 0;
    fAttribIn  = 0;
    fAttribOut = 0;
    lpSort = NULL;

    PointerList *pplCmd = NULL;
    
    TCHAR *pBuffer = NULL;
    if ( ! cmdutil_GetOptionListFromEnv (pBuffer, pplCmd, TEXT("DIRCMD")))
        return CMD_DIR_ERROR;

    PointerList *ppl = pplCmd;

    if (! ppl) {
        ppl = pplExplicitOpts;
        pplExplicitOpts = NULL;
    }

    int fResult = CMD_DIR_SEP;

    while (ppl) {
        TCHAR *pOption = ((TCHAR *)ppl->pData);
        if (*pOption)
            pOption++;

        _tcsupr_s (pOption, _tcslen(pOption)+1);

        switch (*pOption) {
            case TEXT('P'):
                fResult |= CMD_DIR_PAUSE;
                break;

            case TEXT('W'):
                fResult |= CMD_DIR_WIDE;
                break;

            case TEXT('A'):
                ++pOption;
                if (*pOption == TEXT(':'))
                    ++pOption;

                if (*pOption == TEXT('\0')) {
                    fAttribOut = 0;
                    fAttribIn  = 0xffffffff;
                    break;
                }

                while (*pOption != TEXT('\0')) {
                    unsigned int *pAttrib = &fAttribIn;
                    
                    if (*pOption == TEXT('-')) {
                        pAttrib = &fAttribOut;
                        ++pOption;
                    }

                    switch (*pOption) {
                        case TEXT('D'):
                            *pAttrib |= FILE_ATTRIBUTE_DIRECTORY;
                            break;
                        case TEXT('H'):
                            *pAttrib |= FILE_ATTRIBUTE_HIDDEN;
                            break;
                        case TEXT('S'):
                            *pAttrib |= FILE_ATTRIBUTE_SYSTEM;
                            break;
                        case TEXT('R'):
                            *pAttrib |= FILE_ATTRIBUTE_READONLY;
                            break;
                        case TEXT('A'):
                            *pAttrib |= FILE_ATTRIBUTE_ARCHIVE;
                            break;
                        default:
                            cmdutil_Complain (IDS_ERROR_GENERAL_BADOPTION, (TCHAR *)ppl->pData);
                            fResult = CMD_DIR_ERROR;
                            goto label_out;
                    }

                    ++pOption;
                }
                break;

            case TEXT('O'):
                {
                    ++pOption;
                    if (*pOption == TEXT(':'))
                        ++pOption;

                    WCHAR *p = pOption;
                    int fNeg = FALSE;

                    while (*p) {
                        if (((*p == '-') && fNeg) || (! wcschr (L"NESDG-", towupper (*p)))) {
                            cmdutil_Complain (IDS_ERROR_GENERAL_BADOPTION, (TCHAR *)ppl->pData);
                            fResult = CMD_DIR_ERROR;
                            goto label_out;
                        }

                        fNeg = (*p == '-');

                        ++p;
                    }

                    lpSort = (*pOption == TEXT('\0')) ? cmdutil_tcsdup (TEXT("GN")) : cmdutil_tcsdup (pOption);
                    break;
                }

            case TEXT('T'):
                ++pOption;

                if (*pOption == TEXT(':'))
                    ++pOption;

                if (*pOption == TEXT('C'))
                    fResult |= CMD_DIR_TIME_CREAT;
                else if (*pOption == TEXT('A'))
                    fResult |= CMD_DIR_TIME_ACC;
                else if (*pOption == TEXT('W'))
                    fResult |= CMD_DIR_TIME_WRITE;
                else if (*pOption == TEXT('\0'))
                    fResult |= CMD_DIR_TIME_WRITE;
                else {
                    cmdutil_Complain (IDS_ERROR_GENERAL_BADOPTION, (TCHAR *)ppl->pData);
                    fResult = CMD_DIR_ERROR;
                    goto label_out;
                }
                break;

            case TEXT('S'):
                fRecurse = TRUE;
                break;

            case TEXT('B'):
                fResult |= CMD_DIR_BARE;
                break;

            case TEXT('L'):
                fResult |= CMD_DIR_LOWER;
                break;

            case TEXT('C'):
                fResult |= CMD_DIR_SEP;
                break;
            
            case TEXT('-'):
                if (*(pOption + 1) == TEXT('C')) {
                    fResult &= ~CMD_DIR_SEP;
                    break;
                }
                //
                //  Fall through
                //
            default:
                cmdutil_Complain (IDS_ERROR_GENERAL_BADOPTION, pOption);
                fResult = CMD_DIR_ERROR;
                goto label_out;
        }

        ppl = ppl->pNext;
        
        if ((! ppl) && pplExplicitOpts) {
            ppl = pplExplicitOpts;
            pplExplicitOpts = NULL;
        }
    }

    if (fResult & CMD_DIR_BARE)
        fResult &= ~CMD_DIR_WIDE;

    fAttribOut &= ~fAttribIn;

    if (fAttribIn == 0) {
        fAttribIn  = 0xffffffff;

        if (fAttribOut == 0)
            fAttribOut = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    }

label_out:  
    cmdutil_FixedFreePointerList (pplCmd);

    if (pBuffer)
        cmdutil_Free (pBuffer);

    return fResult;
}

int __cdecl cmdexec_DirCompareFiles (const void *elem1, const void *elem2) {
    WIN32_FIND_DATA *pWFD1 = *(WIN32_FIND_DATA **)elem1;
    WIN32_FIND_DATA *pWFD2 = *(WIN32_FIND_DATA **)elem2;
    LARGE_INTEGER liFileSize1 = {0};
    LARGE_INTEGER liFileSize2 = {0};

    TCHAR *lpSort = gpSort;
    while (*lpSort != TEXT('\0')) {
        int fReverse = 1;
        
        if (*lpSort == TEXT('-')) {
            fReverse = -1;
            ++lpSort;
        }

        switch (*lpSort) {
            case TEXT('N'):
                return fReverse * _tcsicmp (pWFD1->cFileName, pWFD2->cFileName);

            case TEXT('E'): {
                    TCHAR *lpExt1 = _tcschr (pWFD1->cFileName, TEXT('.'));
                    TCHAR *lpExt2 = _tcschr (pWFD2->cFileName, TEXT('.'));
                    
                    if ((! lpExt1) && (! lpExt2))
                        break;

                    if (! lpExt1)
                        return -fReverse;
                    
                    if (! lpExt2)
                        return fReverse;

                    int iRes = fReverse * _tcsicmp (lpExt1, lpExt2);

                    if (iRes != 0)
                        return iRes;
                }
                break;

            case TEXT('G'):
                if ((pWFD1->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        (! (pWFD2->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
                    return -fReverse;

                if ((pWFD2->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        (! (pWFD1->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
                    return fReverse;
                break;

            case TEXT('S'):
                liFileSize1.LowPart = pWFD1->nFileSizeLow;
                liFileSize1.HighPart = pWFD1->nFileSizeHigh;
                liFileSize2.LowPart = pWFD2->nFileSizeLow;
                liFileSize2.HighPart = pWFD2->nFileSizeHigh;
                if (liFileSize1.QuadPart < liFileSize2.QuadPart) {
                    return -fReverse;
                }
                if (liFileSize1.QuadPart > liFileSize2.QuadPart) {
                    return fReverse;
                }
                break;

            case TEXT('D'): {
                    int iRes = 0;
                    if (gfOptions & CMD_DIR_TIME_CREAT)
                        iRes = CompareFileTime (&pWFD1->ftCreationTime, &pWFD2->ftCreationTime);
                    else if (gfOptions & CMD_DIR_TIME_ACC)
                        iRes = CompareFileTime (&pWFD1->ftLastAccessTime, &pWFD2->ftLastAccessTime);
                    else
                        iRes = CompareFileTime (&pWFD1->ftLastWriteTime, &pWFD2->ftLastWriteTime);

                    if (iRes)
                        return iRes * fReverse;
                }
                break;

            default:
                return 0;
        }

        ++lpSort;
    }

    return 0;
}

struct DirPrintHelperParms {
    TCHAR   *lpBaseName;
    int     iFilesPerLine;
    int     iSpacePerFile;
    int     iCurrentOffset;
    int     fOptions;
    int     fRecursion;
    int     f24hr;
    int     iDateType;
};

int cmdexec_HasTrailingSlash (TCHAR *lpszDirName) {
    TCHAR *lpszEnd = _tcschr (lpszDirName, TEXT('\0'));
    if (lpszEnd == lpszDirName)
        return FALSE;

    --lpszEnd;

    if (*lpszEnd == TEXT('\\'))
        return TRUE;

    return FALSE;
}

void cmdexec_DirPrintFile (DirPrintHelperParms *pDPHP, WIN32_FIND_DATA *pWFD, int &iCurrentLine) {
    if (pDPHP->fOptions & CMD_DIR_LOWER)
        _tcslwr_s (pWFD->cFileName, _countof(pWFD->cFileName));

    if (pDPHP->fOptions & CMD_DIR_WIDE) {
        int iAddedSize = _tcslen (pWFD->cFileName);
        if (pWFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            iAddedSize += 2;
            _fputts (TEXT("["), STDOUT);
            _fputts (pWFD->cFileName, STDOUT);
            _fputts (TEXT("]"), STDOUT);
        } else
            _fputts (pWFD->cFileName, STDOUT);
        
        ++pDPHP->iCurrentOffset;
        if (pDPHP->iCurrentOffset >= pDPHP->iFilesPerLine) {
            pDPHP->iCurrentOffset = 0;
            _fputts (TEXT("\n"), STDOUT);
            
            if (pDPHP->fOptions & CMD_DIR_PAUSE)
                cmdexec_MaintainPause (iCurrentLine, 1, TRUE);
        } else {
            while (iAddedSize++ < pDPHP->iSpacePerFile)
                _fputts (TEXT(" "), STDOUT);
        }
    } else if (pDPHP->fOptions & CMD_DIR_BARE) {
        if (! cmdutil_HierarchyMarker(pWFD->cFileName)) {
            if (pDPHP->fRecursion) {
                _fputts (pDPHP->lpBaseName, STDOUT);
                if (! cmdexec_HasTrailingSlash (pDPHP->lpBaseName))
                    _fputts (TEXT("\\"), STDOUT);
            }

            _fputts (pWFD->cFileName, STDOUT);
            _fputts (TEXT("\n"), STDOUT);

            if (pDPHP->fOptions & CMD_DIR_PAUSE)
                cmdexec_MaintainPause (iCurrentLine, 1, TRUE);
        }
    } else {
        SYSTEMTIME  stm;
        FILETIME    ftm;
        if (pDPHP->fOptions & CMD_DIR_TIME_ACC)
            FileTimeToLocalFileTime (&pWFD->ftLastAccessTime, &ftm);
        else if (pDPHP->fOptions & CMD_DIR_TIME_CREAT)
            FileTimeToLocalFileTime (&pWFD->ftCreationTime, &ftm);
        else
            FileTimeToLocalFileTime (&pWFD->ftLastWriteTime, &ftm);

        FileTimeToSystemTime (&ftm, &stm);

        TCHAR sSize[CMD_BUF_INTEGER];
        LARGE_INTEGER liFileSize = {0};
        liFileSize.LowPart  = pWFD->nFileSizeLow;
        liFileSize.HighPart = pWFD->nFileSizeHigh;
        if (pWFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            StringCchPrintf(sSize, _countof(sSize), TEXT("%18s"), TEXT(" "));
        }
        else {
            StringCchPrintf(sSize, _countof(sSize), TEXT("%18I64d"), liFileSize.QuadPart);
        }

        TCHAR szDate[CMD_BUF_TINY];
        if (pDPHP->iDateType == 1)
            StringCchPrintf(szDate, _countof(szDate), TEXT("%02d/%02d/%02d"), stm.wDay, stm.wMonth, stm.wYear % 100);
        else if (pDPHP->iDateType == 2)
            StringCchPrintf(szDate, _countof(szDate), TEXT("%02d/%02d/%02d"), stm.wYear % 100, stm.wMonth, stm.wDay);
        else
            StringCchPrintf(szDate, _countof(szDate), TEXT("%02d/%02d/%02d"), stm.wMonth, stm.wDay, stm.wYear % 100);

        TCHAR szTime[CMD_BUF_TINY];
        if (pDPHP->f24hr)
            StringCchPrintf(szTime, _countof(szTime), TEXT("%02d:%02d "), stm.wHour, stm.wMinute);
        else{
          WORD hour = stm.wHour % 12;

      // check for 12am
          if(!hour) 
            hour = 12;
          
            StringCchPrintf(szTime, _countof(szTime), TEXT("%02d:%02d%c"), hour,
                                            stm.wMinute, stm.wHour >= 12 ? TEXT('p') : TEXT('a'));
        }

        _ftprintf (STDOUT, TEXT("%s  %s    %s %s %s\n"), szDate, szTime,
                    pWFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? TEXT("<DIR>") : TEXT("     "),
                    sSize, pWFD->cFileName);

        if (pDPHP->fOptions & CMD_DIR_PAUSE)
            cmdexec_MaintainPause (iCurrentLine, 1, TRUE);
    }
}

struct DirHelperParms {
    TCHAR           *lpFilePattern;
    TCHAR           *lpSort;
    int             iBufSize;
    int             iPatternSize;
    int             fOptions;
    int             fRecursion;
    int             f24hr;
    int             iDateType;
    int             iCurrentLine;
    unsigned int    fAttribIn;
    unsigned int    fAttribOut;

        unsigned int     uiTotalDirs;
        unsigned int     uiTotalFiles;
        unsigned __int64 ui64TotalFileSize;
};


//
//  Note: lpName can be "" for root directory, since pattern already carries
//  the required slash part...
//
int cmdexec_DirHelper (TCHAR *lpName, void *pvData) {
    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    DirHelperParms *pDHP = (DirHelperParms *)pvData;
    TCHAR   *lpNameEnd = _tcschr (lpName, TEXT('\0'));
    int     iSize = lpNameEnd - lpName;

    if (iSize + pDHP->iPatternSize > pDHP->iBufSize) {
        cmdutil_Complain (IDS_ERROR_GENERAL_PATHTOOBIG, lpName);
        return CMD_ERROR_TOOBIG;
    }

    if (lpNameEnd - lpName <= 1)
        memcpy (lpName, pDHP->lpFilePattern, sizeof(TCHAR) * pDHP->iPatternSize);
    else {
        if (*(lpNameEnd - 1) == TEXT('\\'))
            --lpNameEnd;

        memcpy (lpNameEnd, pDHP->lpFilePattern, sizeof(TCHAR) * pDHP->iPatternSize);
    }
    
    PointerList *pplListOfFiles = NULL;
    int         iNumOfFiles = 0;
    unsigned __int64    ui64TotalSize  = 0;

    DirPrintHelperParms dphp;

    dphp.iCurrentOffset = 0;
    dphp.iFilesPerLine  = 1;
    dphp.iSpacePerFile  = pCmdState->xMax;
    dphp.fOptions       = pDHP->fOptions;
    dphp.fRecursion     = pDHP->fRecursion;
    dphp.f24hr          = pDHP->f24hr;
    dphp.iDateType      = pDHP->iDateType;
    dphp.lpBaseName     = lpName;

    if (pDHP->fOptions & CMD_DIR_LOWER)
        _tcslwr_s (dphp.lpBaseName, _tcslen(dphp.lpBaseName)+1);

    int fHeaderPrinted = FALSE;

    WIN32_FIND_DATA wfd;

    HANDLE hSearch = FindFirstFile (lpName, &wfd);
    
    *lpNameEnd = TEXT('\0');

    if ((! (pDHP->fOptions & CMD_DIR_BARE)) && (! pDHP->fRecursion)) {
        fHeaderPrinted = TRUE;

        if (pDHP->fOptions & CMD_DIR_PAUSE)
            cmdexec_MaintainPause (pDHP->iCurrentLine, 3, FALSE);

        TCHAR sHeader[CMD_BUF_SMALL];
        LoadString (pCmdState->hInstance, IDS_DIR_HEADER, sHeader, CMD_BUF_SMALL);

        if (pDHP->uiTotalDirs > 0)
            _fputts (TEXT("\n"), STDOUT);

        _fputts (sHeader, STDOUT);
        _fputts (*lpName == TEXT('\0') ? TEXT("\\") : lpName, STDOUT);
        _fputts (TEXT("\n\n"), STDOUT);
    }

    if (hSearch == INVALID_HANDLE_VALUE) {
        if (fHeaderPrinted) {
            TCHAR sBody[CMD_BUF_SMALL];
            LoadString (pCmdState->hInstance, IDS_DIR_FNF, sBody, CMD_BUF_SMALL);
            _fputts (sBody, STDOUT);
        }
        
        return CMD_SUCCESS;
    }
    
    int iMaxNameLen = 1;
    
    do {
        if (((pDHP->fAttribIn == 0xffffffff) || ((wfd.dwFileAttributes & pDHP->fAttribIn) == pDHP->fAttribIn)) &&
             ((wfd.dwFileAttributes & pDHP->fAttribOut) == 0)) {
            if ((! fHeaderPrinted) && (! (pDHP->fOptions & CMD_DIR_BARE))) {
                fHeaderPrinted = TRUE;

                if (pDHP->fOptions & CMD_DIR_PAUSE)
                    cmdexec_MaintainPause (pDHP->iCurrentLine, 3, FALSE);

                if (pCmdState->fCtrlC)
                    break;

                TCHAR sHeader[CMD_BUF_SMALL];
                LoadString (pCmdState->hInstance, IDS_DIR_HEADER, sHeader, CMD_BUF_SMALL);
                _fputts (sHeader, STDOUT);
                _fputts (*lpName == TEXT('\0') ? TEXT("\\") : lpName, STDOUT);
                _fputts (TEXT("\n\n"), STDOUT);
            }

            ++iNumOfFiles;
            LARGE_INTEGER liFileSize = {0};
            liFileSize.LowPart  = wfd.nFileSizeLow;
            liFileSize.HighPart = wfd.nFileSizeHigh;
            ui64TotalSize += liFileSize.QuadPart;

            if (! (pDHP->lpSort || (pDHP->fOptions & CMD_DIR_WIDE)))
                cmdexec_DirPrintFile (&dphp, &wfd, pDHP->iCurrentLine);
            else {
                int iNameLength = _tcslen (wfd.cFileName) + 4;
                
                if (iMaxNameLen < iNameLength)
                    iMaxNameLen = iNameLength;

                WIN32_FIND_DATA *pData = (WIN32_FIND_DATA *)cmdutil_Alloc (sizeof(WIN32_FIND_DATA));
                memcpy (pData, &wfd, sizeof (WIN32_FIND_DATA));
                pplListOfFiles = cmdutil_NewPointerListItem (pData, pplListOfFiles);
            }
        }
    } while ((! pCmdState->fCtrlC) && FindNextFile (hSearch, &wfd));

    FindClose (hSearch);
    
    if (iMaxNameLen > pCmdState->xMax)
        iMaxNameLen = pCmdState->xMax;

    dphp.iFilesPerLine = pCmdState->xMax / iMaxNameLen;
    dphp.iSpacePerFile = pCmdState->xMax / dphp.iFilesPerLine;

    if (pplListOfFiles) {   // Sort it here...
        WIN32_FIND_DATA **ppWFD = (WIN32_FIND_DATA **)cmdutil_Alloc (sizeof(WIN32_FIND_DATA *) * iNumOfFiles);
        PointerList *pplRun = pplListOfFiles;
        
        int i = 0;

        while (pplRun) {
            ppWFD[i++] = (WIN32_FIND_DATA *)pplRun->pData;
            pplRun = pplRun->pNext;
        }

        ASSERT (i == iNumOfFiles);

        cmdutil_FixedFreePointerList (pplListOfFiles);

        //
        //  Sort it here...
        //
        if (pDHP->lpSort) {
            gpSort      = pDHP->lpSort;
            gfOptions   = pDHP->fOptions;
            qsort (ppWFD, iNumOfFiles, sizeof (WIN32_FIND_DATA *), cmdexec_DirCompareFiles);
            gpSort      = NULL;
            gfOptions   = 0;
        }

        //
        //  And - finally !!! - output...
        //
        for (i = 0 ; i < iNumOfFiles; ++i) {
            if (! pCmdState->fCtrlC)
                cmdexec_DirPrintFile (&dphp, ppWFD[i], pDHP->iCurrentLine);

            cmdutil_Free (ppWFD[i]);
        }

        cmdutil_Free (ppWFD);
    }

    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    if (fHeaderPrinted) {
        if (pDHP->fOptions & CMD_DIR_PAUSE)
            cmdexec_MaintainPause (pDHP->iCurrentLine, 3, FALSE);

        TCHAR sFooter[CMD_BUF_SMALL];
        LoadString (pCmdState->hInstance, IDS_DIR_FOOTER, sFooter, CMD_BUF_SMALL);
        _ftprintf (STDOUT, sFooter, iNumOfFiles, ui64TotalSize);
    }

    pDHP->uiTotalDirs += 1;
    pDHP->uiTotalFiles += iNumOfFiles;
    pDHP->ui64TotalFileSize += ui64TotalSize;

    return CMD_SUCCESS;
}

int cmdexec_DoDirForMask
(
TCHAR *lpBuffer,
int iBufSize,
int fOptions,
int fRecursion,
unsigned int fAttribIn,
unsigned int fAttribOut,
TCHAR *lpSort,
unsigned int *piTotalFiles,
unsigned int *piTotalDirs,
unsigned __int64 *pi64TotalFileSize,
unsigned __int64 *pi64FreeSpace
) {
    *pi64FreeSpace = 0;

    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    DirHelperParms dhp;
    dhp.fOptions        = fOptions;
    dhp.iBufSize        = iBufSize;
    dhp.fRecursion      = fRecursion;
    dhp.fAttribIn       = fAttribIn;
    dhp.fAttribOut      = fAttribOut;
    dhp.lpSort          = lpSort;
    dhp.iCurrentLine    = 0;
    dhp.f24hr           = 0;
    dhp.iDateType       = 0;
        dhp.uiTotalDirs     = 0;
        dhp.uiTotalFiles    = 0;
        dhp.ui64TotalFileSize = 0;

    TCHAR szLocaleInfo[2];

    if (GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_ITIME, szLocaleInfo, 2) && (szLocaleInfo[0] == TEXT('1')))
        dhp.f24hr = TRUE;

    if (GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IDATE, szLocaleInfo, 2)) {
        if (szLocaleInfo[0] == TEXT('1'))
            dhp.iDateType = 1;
        else if (szLocaleInfo[0] == TEXT('2'))
            dhp.iDateType = 2;
    }

    int iRet = CMD_SUCCESS;
    ULARGE_INTEGER liFree, liTotal;

    if ((! cmdutil_HasWildCards(lpBuffer)) && cmdutil_ExistsDir (lpBuffer)) {
        dhp.lpFilePattern = PATTERN_ALL;
        dhp.iPatternSize  = PATTERN_ALL_CHARS;

        if ((*pi64FreeSpace == 0) && (! GetDiskFreeSpaceEx (lpBuffer, &liFree, &liTotal, (PULARGE_INTEGER)pi64FreeSpace)))
            *pi64FreeSpace = 0;

        if (fRecursion) {
            TCHAR *pSep = _tcschr (lpBuffer, TEXT('\0'));
            
            if (pSep - lpBuffer <= 1)
                pSep = lpBuffer;
            else if (*(pSep-1) == TEXT('\\'))
                --pSep;

            iRet = cmdutil_ExecuteForPatternRecursively (lpBuffer, iBufSize, pSep, PATTERN_ALL,
                            PATTERN_ALL_CHARS, &dhp, DIRORDER_PRE, cmdexec_DirHelper, NULL);
        } else
            iRet = cmdexec_DirHelper (lpBuffer, &dhp);
    } else {
        TCHAR sPattern[CMD_BUF_REG];
        TCHAR *pSep = _tcsrchr (lpBuffer, TEXT('\\'));

        ASSERT (pSep);
    
        int iPatternSize = _tcslen (pSep) + 1;
            if (iPatternSize > CMD_BUF_REG)
                    return CMD_ERROR;

        memcpy (sPattern, pSep, sizeof (TCHAR) * iPatternSize);
        dhp.lpFilePattern = sPattern;
        dhp.iPatternSize  = iPatternSize;

        *pSep = TEXT('\0');

        if ((*pi64FreeSpace == 0) && (! GetDiskFreeSpaceEx (lpBuffer, &liFree, &liTotal, (PULARGE_INTEGER)pi64FreeSpace)))
            *pi64FreeSpace = 0;

        if (fRecursion)
            iRet = cmdutil_ExecuteForPatternRecursively (lpBuffer, iBufSize, pSep,
                    PATTERN_ALL, PATTERN_ALL_CHARS, &dhp, DIRORDER_PRE, cmdexec_DirHelper, NULL);
        else
            iRet = cmdexec_DirHelper (lpBuffer, &dhp);
    }

    *piTotalFiles += dhp.uiTotalFiles;
    *piTotalDirs += dhp.uiTotalDirs;
    *pi64TotalFileSize += dhp.ui64TotalFileSize;

    return iRet;
}

int cmdexec_CmdDir (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int fRecursion = FALSE;
    int fOptions   = 0;
    unsigned int fAttribIn  = 0;
    unsigned int fAttribOut = 0;

    unsigned int uiTotalFiles = 0;
        unsigned int uiTotalDirs  = 0;
        unsigned __int64 ui64TotalSize = 0;
        unsigned __int64 ui64DiskFree  = 0;

    cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    TCHAR *lpSort = NULL;

    PointerList *pplRun;

    fOptions = cmdexec_MergeDirOptions (pplOptions, fRecursion, fAttribIn, fAttribOut, lpSort);

    cmdutil_FixedFreePointerList (pplOptions);

    if (fOptions == CMD_DIR_ERROR) {
        cmdutil_FixedFreePointerList (pplFiles);
        if (lpSort)
            cmdutil_Free (lpSort);
        return CMD_ERROR;
    }

    if (! pplFiles) {
        TCHAR sBuffer[CMD_BUF_REG];
        StringCchCopy(sBuffer, _countof(sBuffer), pCmdState->sCwd);

        int iRes = cmdexec_DoDirForMask (sBuffer, CMD_BUF_REG, fOptions, fRecursion, fAttribIn, fAttribOut, lpSort, &uiTotalFiles, &uiTotalDirs, &ui64TotalSize, &ui64DiskFree);
        
        if (lpSort)
            cmdutil_Free (lpSort);

        if ((iRes == CMD_SUCCESS) && (! (fOptions & CMD_DIR_BARE))) {
            TCHAR sFooter[CMD_BUF_REG];
            if (uiTotalDirs > 1) {
                LoadString (pCmdState->hInstance, IDS_DIR_FOOTER_GR, sFooter, CMD_BUF_REG);
                _ftprintf (STDOUT, sFooter, uiTotalFiles, ui64TotalSize, uiTotalDirs, ui64DiskFree);
            } else {
                LoadString (pCmdState->hInstance, IDS_DIR_FOOTER_GU, sFooter, CMD_BUF_REG);
                _ftprintf (STDOUT, sFooter, uiTotalDirs, ui64DiskFree);
            }
        }

        return iRes;
    }

    pplRun = pplFiles;
    int iRes = CMD_SUCCESS;

    while (pplRun) {
        TCHAR sBuffer[CMD_BUF_REG];
        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }

        iRes = cmdexec_DoDirForMask (sBuffer, CMD_BUF_REG, fOptions, fRecursion, fAttribIn, fAttribOut, lpSort, &uiTotalFiles, &uiTotalDirs, &ui64TotalSize, &ui64DiskFree);

        if (pCmdState->fCtrlC)
            break;

        if (iRes != CMD_SUCCESS) {
            cmdutil_Complain (IDS_ERROR_FNF, lpName);
            break;
        }

        pplRun = pplRun->pNext;
    }

    if (lpSort)
        cmdutil_Free (lpSort);

    if ((iRes == CMD_SUCCESS) && (! (fOptions & CMD_DIR_BARE))) {
        TCHAR sFooter[CMD_BUF_REG];
        if (uiTotalDirs > 1) {
            LoadString (pCmdState->hInstance, IDS_DIR_FOOTER_GR, sFooter, CMD_BUF_REG);
            _ftprintf (STDOUT, sFooter, uiTotalFiles, ui64TotalSize, uiTotalDirs, ui64DiskFree);
        } else {
            LoadString (pCmdState->hInstance, IDS_DIR_FOOTER_GU, sFooter, CMD_BUF_REG);
            _ftprintf (STDOUT, sFooter, uiTotalDirs, ui64DiskFree);
        }
    }

    cmdutil_FixedFreePointerList (pplFiles);
    return iRes;
}


int cmdexec_CmdMd (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    if (pplOptions || (! pplFiles)) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplFiles);
        cmdutil_FixedFreePointerList (pplOptions);
        return CMD_ERROR;
    }

    PointerList *pplRun = pplFiles;
    
    while (pplRun) {
        TCHAR sBuffer[MAX_PATH - 12]; // filename (8 chars) + . + extension (3 characters)
        if (! cmdutil_FormPath (sBuffer, sizeof(sBuffer) / sizeof(sBuffer[0]), (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
            cmdutil_FixedFreePointerList (pplFiles);
            return CMD_ERROR;
        }
    
        if (cmdutil_ExistsDir (sBuffer)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_ALREADYEXISTS, lpName, (TCHAR *)pplRun->pData);
            cmdutil_FixedFreePointerList (pplFiles);
            return CMD_ERROR;
        }
    
        TCHAR *lpSentinel = _tcschr (sBuffer, TEXT('\0'));
    
        ASSERT (sBuffer[0] == TEXT('\\'));

        for ( ; ; ) {
            TCHAR *lpRunner   = _tcsrchr (sBuffer, TEXT('\\'));
            *lpRunner = TEXT('\0');
            if (lpRunner == sBuffer || cmdutil_ExistsDir (sBuffer)) {
                for ( ; ; ) {
                    *lpRunner = TEXT('\\');
                    if (! CreateDirectory (sBuffer, NULL)) {
                        cmdutil_Complain (IDS_ERROR_MD_CANTMAKE, lpName, (TCHAR *)pplRun->pData);
                        cmdutil_FixedFreePointerList (pplFiles);
                        return CMD_ERROR;
                    }

                    lpRunner = _tcschr (lpRunner, TEXT('\0'));

                    if (lpRunner == lpSentinel)
                        break;
                }
            
                break;
            }
        }

        pplRun = pplRun->pNext;
    }

    cmdutil_FixedFreePointerList (pplFiles);
    return CMD_SUCCESS;
}

int cmdexec_CmdRd (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int fRecursion = FALSE;
    int fQuietOk   = FALSE;

    PointerList *pplRun;

    cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    if (pplOptions) {
        PointerList *pplRun = pplOptions;

        while (pplRun) {
            if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/S")) == 0) && (! fRecursion))
                fRecursion = TRUE;
            else if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/Q")) == 0) && (! fQuietOk))
                fQuietOk = TRUE;
            else {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                cmdutil_FixedFreePointerList (pplOptions);
                cmdutil_FixedFreePointerList (pplFiles);
                return CMD_ERROR;
            }
            pplRun = pplRun->pNext;
        }
        cmdutil_FixedFreePointerList (pplOptions);
    }

    if (! pplFiles) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplFiles);
        return CMD_ERROR;
    }

    pplRun = pplFiles;
    int iRes = CMD_SUCCESS;

    while (pplRun) {
        TCHAR sBuffer[CMD_BUF_REG];
        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }
    
        if (! cmdutil_ExistsDir (sBuffer)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, lpName, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }

        if (wcsnicmp (sBuffer, pCmdState->sCwd, wcslen (sBuffer)) == 0) {
            cmdutil_Complain (IDS_ERROR_CANTREMOVE, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }

        if (fRecursion) {
            if (fQuietOk || cmdutil_Confirm (IDS_AREYOUSURE, lpName, sBuffer)) {
                int iRes2 = cmdutil_DeleteAll (sBuffer, CMD_BUF_REG);
                if (iRes2 != CMD_SUCCESS) {
                    if (iRes2 == CMD_ERROR_FNF)
                        cmdutil_Complain (IDS_ERROR_DEL_FNF);
                    else if (iRes2 == CMD_ERROR_TOOBIG)
                        cmdutil_Complain (IDS_ERROR_DEL_TOOBIG);
                    else
                        cmdutil_Complain (IDS_ERROR_CANTREMOVE, lpName);

                    iRes = iRes2;
                }
            }
        } else {
            if (! RemoveDirectory (sBuffer)) {
                cmdutil_Complain (IDS_ERROR_RD_CANTREMOVE, lpName, (TCHAR *)sBuffer);
                iRes = CMD_ERROR;
            }
        }

        pplRun = pplRun->pNext;
    }

    cmdutil_FixedFreePointerList (pplFiles);
    return iRes;
}

int cmdexec_CmdDelHelper (TCHAR *lpName, void *vFlags) {
    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    int fFlags = (int)vFlags;

    if ((fFlags & CMD_DEL_ASK) && (! cmdutil_Confirm (IDS_AREYOUSURE,
                                                TEXT("del"), lpName)))
        return CMD_SUCCESS;

    if (fFlags & CMD_DEL_FORCE)
        SetFileAttributes (lpName, GetFileAttributes (lpName) & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));

    if (cmdutil_IsInvisible (lpName) || (! DeleteFile (lpName)))
        cmdutil_Complain (IDS_ERROR_CANTREMOVE, lpName);

    return CMD_SUCCESS;
}

int cmdexec_CmdDel (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int fRecursion = FALSE;
    int fQuietOk   = FALSE;
    int fAttribs   = 0;

    PointerList *pplRun;

    cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);

    if (pplOptions) {
        PointerList *pplRun = pplOptions;

        while (pplRun) {
            if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/S")) == 0) && (! fRecursion))
                fRecursion = TRUE;
            else if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/Q")) == 0) && (! fQuietOk))
                fQuietOk = TRUE;
            else if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/P")) == 0) && (! (fAttribs & CMD_DEL_ASK)))
                fAttribs |= CMD_DEL_ASK;
            else if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/F")) == 0) && (! (fAttribs & CMD_DEL_FORCE)))
                fAttribs |= CMD_DEL_FORCE;
            else {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                cmdutil_FixedFreePointerList (pplOptions);
                cmdutil_FixedFreePointerList (pplFiles);
                return CMD_ERROR;
            }
            pplRun = pplRun->pNext;
        }
        cmdutil_FixedFreePointerList (pplOptions);
    }

    if (! pplFiles) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplFiles);
        return CMD_ERROR;
    }

    pplRun = pplFiles;
    int iRes = CMD_SUCCESS;

    while (pplRun) {
        TCHAR sBuffer[CMD_BUF_REG];
        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplRun->pData);
            iRes = CMD_ERROR;
            break;
        }

        if (cmdutil_ExistsDir(sBuffer))
            StringCchCat(sBuffer, CMD_BUF_REG, PATTERN_ALL);

        if ((! fQuietOk) && cmdutil_MaskAll (sBuffer) &&
                (! cmdutil_Confirm (IDS_AREYOUSURE, lpName, sBuffer))) {
            pplRun = pplRun->pNext;
            continue;
        }

        iRes = cmdutil_ExecuteForPattern (sBuffer, CMD_BUF_REG, (void *)fAttribs,
                                    fRecursion, cmdexec_CmdDelHelper);

        if (iRes == CMD_ERROR_FNF) {
            // re-read the buffer as it might have been updated in ExecuteForPattern
            if (cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd))
                        cmdutil_Complain (IDS_ERROR_FNF, sBuffer);
                }

        if (iRes != CMD_SUCCESS)
            break;

        pplRun = pplRun->pNext;
    }

    cmdutil_FixedFreePointerList (pplFiles);
    return iRes;
}

int cmdexec_CmdTitle (TCHAR *lpName, TCHAR *lpArgs) {
#if defined (UNDER_CE)
    if (*lpArgs == TEXT('\0'))
        return CMD_SUCCESS;

    USE_CONIOCTL_CALLS

    // can define titles up to length 80
    char szTitleBuf[CMD_BUF_REG];

    if (WideCharToMultiByte(CP_ACP, 0, lpArgs, -1, szTitleBuf, CMD_BUF_REG, NULL, NULL) != 0) {
        szTitleBuf[CMD_BUF_REG - 1] = '\0';
        if (CeSetConsoleTitleA(_fileno(gIO[CMDIO_OUT]), szTitleBuf))
            return CMD_SUCCESS;
    }
#endif

    cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
    return CMD_ERROR;
}

int cmdexec_CmdTypeHelper (TCHAR *lpName, void *vFlags) {
    int fFlags = (int)vFlags;    

#pragma warning ( push )
#pragma warning ( disable : 4996 )
// Allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
    FILE *fp = _tfopen (lpName, TEXT("rt"));
#pragma warning ( pop )
    
    if (!fp) {
        cmdutil_Complain (IDS_ERROR_FNF, lpName);
        return CMD_ERROR;
    }

    if (fFlags != CMD_TYPE_SINGLE)
        _ftprintf (STDOUT, TEXT("%s:\n\n"), lpName);

    unsigned short fUnicodeMarker = 0;
    
    if ((1 == fread (&fUnicodeMarker, 2, 1, fp)) && (fUnicodeMarker == 0xfeff)) {
        fclose (fp);

#pragma warning ( push )
#pragma warning ( disable : 4996 )
// Allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
        fp = _tfopen (lpName, TEXT("rb"));
#pragma warning ( pop )

        if (!fp) {
            cmdutil_Complain (IDS_ERROR_FNF, lpName);
            return CMD_ERROR;
        }

        fread (&fUnicodeMarker, 2, 1, fp);  // Skip the marker

        while ((! feof (fp)) && (! pCmdState->fCtrlC)) {
            WCHAR wBuffer[CMD_BUF_SMALL + 1];
            int iChars = fread (wBuffer, 1, sizeof(WCHAR) * CMD_BUF_SMALL, fp) >> 1;
            if (iChars < 0)
                break;

            //
            //  Do our own petty CR LF -> LF translation...
            //
            if ((iChars == CMD_BUF_SMALL) && (wBuffer[iChars - 1] == '\r')) {
                fseek (fp, -2, SEEK_CUR);
                --iChars;
            }

            ASSERT (iChars <= CMD_BUF_SMALL);
            wBuffer[iChars] = L'\0';

            WCHAR *p = wBuffer;
            while (p - wBuffer < iChars) {
                WCHAR *p2 = wcschr (p, L'\r');
                if (p2 && *(p2 + 1) == L'\n') {
                    *p2 = L'\n';
                    *(p2 + 1) = L'\0';
                }

                fputws (p, STDOUT);
                p += wcslen(p) + 1;
            }
        }
    } else {
        fseek (fp, 0, SEEK_SET);

        while ((! feof (fp)) && (! pCmdState->fCtrlC)) {
            TCHAR   sBuffer[CMD_BUF_HUGE];
            if (! _fgetts (sBuffer, CMD_BUF_HUGE, fp))
                break;

            _fputts (sBuffer, STDOUT);
        }
    }

    fclose (fp);

    _fputts (TEXT("\n"), STDOUT);

    if (pCmdState->fCtrlC)
        return CMD_ERROR_CTRLC;

    return CMD_SUCCESS;
}

int cmdexec_CmdType (TCHAR *lpName, TCHAR *lpArgs) {
    PointerList *pplFiles, *pplOptions;
    
    int iNum = cmdutil_ParseCommandParameters (pplFiles, pplOptions, lpArgs);
    int fRecursion = FALSE;

    PointerList *pplRun;

    if (pplOptions) {
        PointerList *pplRun = pplOptions;

        while (pplRun) {
            if ((_tcsicmp ((TCHAR *)pplRun->pData, TEXT("/S")) == 0) && (! fRecursion))
                fRecursion = TRUE;
            else {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
                cmdutil_FixedFreePointerList (pplOptions);
                cmdutil_FixedFreePointerList (pplFiles);
                return CMD_ERROR;
            }
            pplRun = pplRun->pNext;
        }
        cmdutil_FixedFreePointerList (pplOptions);
    }

    if (! pplFiles) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        cmdutil_FixedFreePointerList (pplFiles);
        return CMD_ERROR;
    }

    if (iNum == 1) {
        TCHAR sBuffer[CMD_BUF_REG];

        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplFiles->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplFiles->pData);
            cmdutil_FixedFreePointerList (pplFiles);
            return CMD_ERROR;
        }
        
        cmdutil_FixedFreePointerList (pplFiles);

        return cmdexec_CmdTypeHelper (sBuffer, (void *)CMD_TYPE_SINGLE);
    }

    int iRes = CMD_SUCCESS;
    pplRun = pplFiles;

    while (pplRun) {
        TCHAR   sBuffer[CMD_BUF_REG];

        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, (TCHAR *)pplRun->pData, pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, (TCHAR *)pplFiles->pData);
            iRes = CMD_ERROR;
            break;
        }

        iRes = cmdutil_ExecuteForPattern (sBuffer, CMD_BUF_REG, (void *)CMD_TYPE_MULTIPLE,
                                                        fRecursion, cmdexec_CmdTypeHelper);

        if (iRes == CMD_ERROR_FNF)
            cmdutil_Complain (IDS_ERROR_FNF, sBuffer);

        if (iRes != CMD_SUCCESS)
            break;

        pplRun = pplRun->pNext;
    }

    cmdutil_FixedFreePointerList (pplFiles);
    return iRes;
}

int cmdexec_CmdExit (TCHAR *lpName, TCHAR *lpArgs) {
    if (*lpArgs != TEXT('\0')) {
        cmdutil_Complain (IDS_ERROR_BADSYNTAX, lpName);
        return CMD_ERROR;
    }
    if (!gRunForever)
    {
        /* 'exit' isn't an error when runforever is set - it just does nothing */
        pCmdState->uiFlags |= CMD_EXEC_EXIT;
    }

    return CMD_SUCCESS;
}


int cmdexec_CmdHelp (TCHAR *lpName, TCHAR *lpArgs) {
    TCHAR sBuffer[CMD_BUF_HUGE];
    if (*lpArgs != TEXT('\0')) {
        if (_tcsicmp (lpArgs, TEXT("CMD")) == 0) {
            LoadString (pCmdState->hInstance, IDS_HELP_CMD, sBuffer, CMD_BUF_HUGE);
            _fputts (sBuffer, STDOUT);
            return CMD_SUCCESS;
        }

        int iIndex = cmdexec_FindCommand (lpArgs);
        
        if ((iIndex < 0) || (! aAllCommands[iIndex].iMsgExtHelp)) {
            cmdutil_Complain (IDS_ERROR_HELP_NOTFOUND, lpArgs);
            return CMD_ERROR;
        }

        LoadString (pCmdState->hInstance, aAllCommands[iIndex].iMsgExtHelp, sBuffer, CMD_BUF_HUGE);
        _fputts (sBuffer, STDOUT);
        
        return CMD_SUCCESS;
    }

    LoadString (pCmdState->hInstance, IDS_HELP_START, sBuffer, CMD_BUF_HUGE);
    _ftprintf (STDOUT, TEXT("%s\n"), sBuffer);

    for (int i = 0 ; (i < CMD_COMMAND_NUMBER) && (! pCmdState->fCtrlC); ++i) {
        if (aAllCommands[i].iMsgHelp) {
            LoadString (pCmdState->hInstance, aAllCommands[i].iMsgHelp, sBuffer, CMD_BUF_HUGE);
            _ftprintf (STDOUT, TEXT("\t%s\t%s\n"), aAllCommands[i].lpCmdName, sBuffer);
        }
    }

    LoadString (pCmdState->hInstance, IDS_HELP_END, sBuffer, CMD_BUF_HUGE);
    _ftprintf (STDOUT, TEXT("%s\n"), sBuffer);
    return CMD_SUCCESS;
}

int cmdexec_EnvFilter (TCHAR *s1, TCHAR *s2) {
    while (*s1 != TEXT('\0') && (*s1 == *s2)) {
        ++s1;
        ++s2;
    }

    if (*s1 != TEXT('\0'))
        return FALSE;

    return TRUE;
}

int cmdexec_CmdSet (TCHAR *lpName, TCHAR *lpArgs) {
    TCHAR *lpBody = _tcschr (lpArgs, TEXT('='));
    
    if (lpBody) {
        *lpBody = TEXT('\0');

        ++lpBody;
        
        _tcsupr_s (lpArgs, _tcslen(lpArgs)+1);
        
        pCmdState->ehHash.hash_set (lpArgs, (*lpBody == TEXT('\0')) ? NULL : lpBody);
        
        return CMD_SUCCESS;
    }

    _tcsupr_s (lpArgs, _tcslen(lpArgs)+1);

    lpBody = pCmdState->ehHash.build ();
    while ((*lpBody) && (! pCmdState->fCtrlC)) {
        if (cmdexec_EnvFilter (lpArgs, lpBody)) {
            _fputts (lpBody, STDOUT);
            _fputts (TEXT("\n"), STDOUT);
        }
        lpBody = _tcschr (lpBody, TEXT('\0'));
        ++lpBody;
    }

    return CMD_SUCCESS;
}

int cmdexec_CmdPath (TCHAR *lpName, TCHAR *lpArgs) {
    if (*lpArgs != TEXT('\0')) {
        pCmdState->ehHash.hash_set (TEXT("PATH"), lpArgs);
        return CMD_SUCCESS;
    }

    TCHAR *lpPath = pCmdState->ehHash.hash_get (TEXT("PATH"));
    if (lpPath)
        _fputts (lpPath, STDOUT);

    _fputts (TEXT("\n"), STDOUT);
    return CMD_SUCCESS;
}

int cmdexec_CmdEcho (TCHAR *lpName, TCHAR *lpArgs) {
    if (_tcsicmp(lpArgs, TEXT("ON")) == 0) {
        pCmdState->uiFlags |= CMD_ECHO_ON;
        return CMD_SUCCESS;
    }

    if (_tcsicmp(lpArgs, TEXT("OFF")) == 0) {
        pCmdState->uiFlags &= ~CMD_ECHO_ON;
        return CMD_SUCCESS;
    }

    if (*lpArgs == TEXT('\0')) {
        TCHAR sBuffer[CMD_BUF_SMALL];

        LoadString (pCmdState->hInstance, (pCmdState->uiFlags & CMD_ECHO_ON) ?
                                IDS_ECHOON : IDS_ECHOOFF, sBuffer, CMD_BUF_SMALL);
        
        _fputts (sBuffer, STDOUT);
        
        return CMD_SUCCESS;
    }

    // If argument string starts without space, the first symbol is ignored
    if (wcsnicmp (lpArgs - 4, L"echo", 4) == 0)
        ++lpArgs;

    _fputts (lpArgs, STDOUT);
    _fputts (TEXT("\n"), STDOUT);
    return CMD_SUCCESS;
}

int cmdexec_CmdPwd (TCHAR *lpName, TCHAR *lpArgs) {
    _ftprintf (STDOUT, TEXT("%s\n"), pCmdState->sCwd);

    return CMD_SUCCESS;
}

int cmdexec_CmdCd (TCHAR *lpName, TCHAR *lpArgs) {
    if (*lpArgs == TEXT('\0'))
        return cmdexec_CmdPwd (lpName, lpArgs);

    cmdutil_RemoveQuotes (lpArgs);

    TCHAR sBuffer[CMD_BUF_REG];
    if ((! cmdutil_FormPath (sBuffer, CMD_BUF_REG, lpArgs, pCmdState->sCwd)) ||
        cmdutil_HasWildCards (sBuffer)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, lpName, lpArgs);
        return CMD_ERROR;
    }
    
    if (! cmdutil_ExistsDir (sBuffer)) {
        cmdutil_Complain (IDS_ERROR_GENERAL_NOTFOUND, lpName, lpArgs);
        return CMD_ERROR;
    }
    
    StringCchCopy(pCmdState->sCwd, _countof(pCmdState->sCwd), sBuffer);

    return CMD_SUCCESS;
}
//
//  Increase output buffer
//
static void cmdexec_IncreaseCmdBuffer (void) 
{
    UINT uLen = 0;
    int iNewSize = pCmdState->iBufCmdChars + CMD_BUF_INCREMENT;
    if (SUCCEEDED(UIntMult(iNewSize, sizeof (TCHAR), &uLen))) {
        pCmdState->iBufCmdChars = iNewSize;
        pCmdState->lpBufCmd = (TCHAR *)cmdutil_ReAlloc (pCmdState->lpBufCmd, uLen);
    }
}

//
//  First stage of conditioning - expand macros
//
int cmdexec_IsBatchArg (TCHAR *lpName) {
    //
    //  If not in batch, ignore attempts to get arguments.
    //
    if (! pCmdState->lpCommandInput)
        return FALSE;

    if ((*lpName < TEXT('0')) || (*lpName > TEXT('9')))
        return FALSE;

    return TRUE;
}

TCHAR *cmdexec_BatchArgVal (TCHAR *lpName) {
    ASSERT ((*lpName >= TEXT('0')) && (*lpName <= TEXT('9')));
    int iNum = *lpName - TEXT('0');
    PointerList *ppl = pCmdState->pplBatchArgs;
    
    for (int i = 0 ; i < iNum && ppl ; ++i, ppl = ppl->pNext)
        ;

    if (! ppl)
        return TEXT("");

    return (TCHAR *)ppl->pData;
}

void cmdexec_ExpandMacros (void) {
    int nCmdChars = 0;
    TCHAR   *lpCurrentChar = pCmdState->lpBufInput;
    
    while (*lpCurrentChar) {
        if (nCmdChars + 1 >= pCmdState->iBufCmdChars) {
            cmdexec_IncreaseCmdBuffer ();
            continue;
        }

        if (*lpCurrentChar == TEXT('%')) {
            TCHAR *lpStart    = lpCurrentChar + 1;
            int   fIsBatchArg = cmdexec_IsBatchArg (lpStart);
            TCHAR *lpEnd      = fIsBatchArg ? NULL : _tcschr (lpStart, TEXT('%'));

            if (lpEnd || fIsBatchArg) {
                if (lpEnd) {
                    *lpEnd = TEXT('\0');
                    _tcsupr_s (lpStart, _tcslen(lpStart)+1);
                }

                TCHAR *lpBody = fIsBatchArg ? cmdexec_BatchArgVal (lpStart) :
                                            pCmdState->ehHash.hash_get (lpStart);
                
                if (lpEnd)
                    *lpEnd = TEXT('%');
                
                if (lpBody) {
                    int sz = _tcslen (lpBody);
                    while (nCmdChars + sz + 1 >= pCmdState->iBufCmdChars)
                        cmdexec_IncreaseCmdBuffer ();
                    
                    memcpy (&pCmdState->lpBufCmd[nCmdChars], lpBody, sz * sizeof(TCHAR));
                    lpCurrentChar = lpEnd ? lpEnd + 1 : lpStart + 1;
                    nCmdChars += sz;
                    continue;
                }
            }
        }
        
        pCmdState->lpBufCmd[nCmdChars++] = *lpCurrentChar++;
    }
    
    pCmdState->lpBufCmd[nCmdChars] = TEXT('\0');
}

//
//  Factor out command and arguments
//
static int cmdexec_FindCommandByStart (TCHAR *lpName) {
    for (int i = 0 ; i < CMD_COMMAND_NUMBER ; ++i) {
        int iCmdLen = wcslen (aAllCommands[i].lpCmdName);
        if (_tcsnicmp (lpName, aAllCommands[i].lpCmdName, iCmdLen) == 0) {
            if ((lpName[iCmdLen] == '\0') ||
                wcschr (L".,<>/;|[]+=(& \\\t\n\r", lpName[iCmdLen]))
                return i;
        }
    }

    return -1;
}

//
//  Attention - important assumption in this function is that lpArgStart
//  
void cmdexec_ParseCmdAndArgs (TCHAR *lpStart) {
    ASSERT (lpStart);

    lpStart += cmdutil_CountWhite (lpStart);
    
    if (*lpStart == TEXT('@'))
        ++lpStart;

    int iCmd = cmdexec_FindCommandByStart (lpStart);

    if (iCmd >= 0) {
        pCmdState->lpCmdStart = aAllCommands[iCmd].lpCmdName;
        pCmdState->lpArgStart = lpStart + wcslen (pCmdState->lpCmdStart);
        pCmdState->lpArgStart += cmdutil_CountWhite (pCmdState->lpArgStart);

        return;
    }

    TCHAR *lpEnd = lpStart + cmdutil_CountNonWhiteWithQuotes (lpStart);

    if (*lpEnd != TEXT('\0')) {
        *lpEnd++ = TEXT('\0');
        lpEnd += cmdutil_CountWhite (lpEnd);
    }

    cmdutil_RemoveQuotes (lpStart);

    pCmdState->lpCmdStart = lpStart;
    pCmdState->lpArgStart = lpEnd;
}

int cmdexec_FactorCommand (void) {
    if (! pCmdState->lpBufCmd[0])
        return FALSE;

    cmdexec_ParseCmdAndArgs (pCmdState->lpBufCmd);

    TCHAR *lpStart = pCmdState->lpCmdStart;
    TCHAR *lpEnd   = pCmdState->lpArgStart;

    if ((*lpStart == TEXT('\0')) || (*lpStart == TEXT(':')))
        return FALSE;

    TCHAR *lpIn  = _tcschr (lpEnd, TEXT('<'));
    
    TCHAR *lpOut = NULL;
    TCHAR *lpErr = NULL;
    
    TCHAR *lp1, *lp2;

    lp1 = _tcschr (lpEnd, TEXT('>'));
    
    if (lp1) {
        if (*(lp1 + 1) == TEXT('>'))
            lp2 = _tcschr (lp1 + 2, TEXT('>'));
        else
            lp2 = _tcschr (lp1 + 1, TEXT('>'));
    } else
        lp2 = NULL;

    if (lp1 && (lp1 != lpEnd) && (*(lp1 - 1) == TEXT('2')))
        lpErr = lp1 - 1;
    else
        lpOut = lp1;

    if (lp2 && (lp2 != lpEnd) && (*(lp2 - 1) == TEXT('2'))) {
        if (lpErr) {
            cmdutil_Complain (IDS_ERROR_GENERAL_DOUBLEERR);
            return FALSE;
        }
        lpErr = lp2 - 1;
    } else if (lp2) {
        if (lpOut) {
            cmdutil_Complain (IDS_ERROR_GENERAL_DOUBLEOUT);
            return FALSE;
        }
        lpOut = lp2;
    }

    if (lpIn) {
        *lpIn = TEXT('\0');
        ++lpIn;
        pCmdState->iIORedirType[CMDIO_IN] = 0;
    }

    if (lpOut) {
        *lpOut = TEXT('\0');

        if (*(lpOut + 1) == TEXT('>')) {
            lpOut += 2;
            pCmdState->iIORedirType[CMDIO_OUT] = 0;
        } else {
            pCmdState->iIORedirType[CMDIO_OUT] = 1;
            lpOut += 1;
        }
    }

    if (lpErr) {
        *lpErr = TEXT('\0');

        if (*(lpErr + 2) == TEXT('>')) {
            lpErr += 3;
            pCmdState->iIORedirType[CMDIO_ERR] = 0;
        } else {
            pCmdState->iIORedirType[CMDIO_ERR] = 1;
            lpErr += 2;
        }
    }

    if (lpIn) {
        lpIn += cmdutil_CountWhite (lpIn);
        if (*lpIn == TEXT('\0')) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADSTDIN);
            return FALSE;
        }
        cmdutil_Trim (lpIn);
        cmdutil_RemoveQuotes (lpIn);
        pCmdState->lpIORedir[CMDIO_IN] = lpIn;
    }

    if (lpOut) {
        lpOut += cmdutil_CountWhite (lpOut);
        if (*lpOut == TEXT('\0')) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADSTDOUT);
            return FALSE;
        }
        cmdutil_Trim (lpOut);
        cmdutil_RemoveQuotes (lpOut);
        pCmdState->lpIORedir[CMDIO_OUT] = lpOut;
    }

    if (lpErr) {
        lpErr += cmdutil_CountWhite (lpErr);
        if (*lpErr == TEXT('\0')) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADSTDERR);
            return FALSE;
        }
        cmdutil_Trim (lpErr);
        cmdutil_RemoveQuotes (lpErr);
        pCmdState->lpIORedir[CMDIO_ERR] = lpErr;
    }

    cmdutil_Trim (lpEnd);

    return TRUE;
}

//
//  Locate command index.
//
int cmdexec_FindCommand (TCHAR *lpName) {
    for (int i = 0 ; i < CMD_COMMAND_NUMBER ; ++i) {
        if (_tcsicmp (lpName, aAllCommands[i].lpCmdName) == 0)
            return i;
    }

    return -1;
}

//
//  Unknown command: assume to be a program or batch file
//
int cmdexec_TryProgram (TCHAR *lpName, TCHAR *lpArgs, int fDetach) {
    if (pCmdState->iBufProcChars == 0) {
        pCmdState->lpBufProc = (TCHAR *)cmdutil_Alloc (CMD_BUF_HUGE*sizeof(TCHAR));
        pCmdState->iBufProcChars = CMD_BUF_HUGE;
    };

    int iFindRes = cmdutil_FindInPath (pCmdState->lpBufProc, pCmdState->iBufProcChars, lpName);
    
    while (iFindRes == CMD_FOUND_NONE) {    // Still we want to try execute-only FS
        TCHAR *lpNameToken = _tcsrchr (lpName, TEXT('\\'));
        if (! lpNameToken) {
            lpNameToken = _tcschr (lpName, TEXT('.'));
            int nChar = _tcslen (lpName);

            if (lpNameToken && (_tcsicmp(lpNameToken, TEXT(".exe")) == 0)) {
                if (nChar < pCmdState->iBufProcChars) {
                    iFindRes = CMD_FOUND_EXE;
                    memcpy (pCmdState->lpBufProc, lpName, sizeof(TCHAR) * (nChar + 1));
                    break;
                }
            } else if (! lpNameToken) {
                if (nChar < pCmdState->iBufProcChars - 5) {
                    iFindRes = CMD_FOUND_EXE;
                    memcpy (pCmdState->lpBufProc, lpName, sizeof(TCHAR) * nChar);
                    memcpy (&pCmdState->lpBufProc[nChar], TEXT(".exe"), sizeof(TEXT(".exe")));
                    break;
                }
            }
        }
        cmdutil_Complain (IDS_ERROR_FNF, lpName);
        return CMD_ERROR;
    }

    if (iFindRes == CMD_FOUND_BATCH) {          // Execute as batch file
        if (fDetach)
            return cmdexec_CmdStartRunCmd (pCmdState->lpBufProc, lpArgs);

        TCHAR *lpCurrentFile = pCmdState->lpCommandInput;
        long  lFilePos = lpCurrentFile ? ftell (pCmdState->fpCommandInput) : 0;

        if (lpCurrentFile)
            fclose (pCmdState->fpCommandInput);

#pragma warning ( push )
#pragma warning ( disable : 4996 )
// allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
        pCmdState->fpCommandInput = _tfopen (pCmdState->lpBufProc, TEXT("rt"));
#pragma warning ( pop ) 
        if (pCmdState->fpCommandInput) {
            FILE  *fpInput = pCmdState->fpCommandInput;
            
            TCHAR *lpInput = pCmdState->lpCommandInput = cmdutil_tcsdup (pCmdState->lpBufProc);
            TCHAR *lpBatchArgs = (*lpArgs == TEXT('\0')) ? NULL : cmdutil_tcsdup(lpArgs);
            
            PointerList *pplBatchArgs = pCmdState->pplBatchArgs;
            PointerList *pplBatchArgsNew = NULL;
            
            if (lpBatchArgs)
                cmdutil_ParseParamsUniform (pplBatchArgsNew, lpBatchArgs);

            TCHAR *lpArg0 = cmdutil_tcsdup (lpName);
            pCmdState->pplBatchArgs = pplBatchArgsNew = cmdutil_NewPointerListItem (lpArg0, pplBatchArgsNew);

            //
            //  Execute program...
            //
            cmd_InputLoop ();

            ASSERT (pCmdState->fpCommandInput);
            ASSERT (pCmdState->lpCommandInput == lpInput);
            
            fclose (fpInput);

            cmdutil_Free (lpInput);

            cmdutil_FixedFreePointerList (pplBatchArgsNew);
            pCmdState->pplBatchArgs = pplBatchArgs;

            cmdutil_Free (lpArg0);
            if (lpBatchArgs)
                cmdutil_Free (lpBatchArgs);
        }

        if (lpCurrentFile) {
#pragma warning ( push )
#pragma warning ( disable : 4996 )
// Allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
            pCmdState->fpCommandInput = _tfopen (lpCurrentFile, TEXT("rt"));
#pragma warning ( pop )
            if (!pCmdState->fpCommandInput)
                cmdutil_Complain (IDS_ERROR_GENERAL_IOBROKEN);
            else
                fseek (pCmdState->fpCommandInput, lFilePos, SEEK_SET);
        } else
            pCmdState->fpCommandInput = NULL;
        
        pCmdState->lpCommandInput = lpCurrentFile;

        pCmdState->uiFlags |= CMD_EXEC_SKIPDOWN;

        return CMD_SUCCESS;
    } else if (iFindRes == CMD_FOUND_EXE) {     // Create process       
        int iArgSize  = _tcslen (lpArgs) + 1;
        int iBufSize  = _tcslen (pCmdState->lpBufProc) + 1;
#if defined (UNDER_CE)
        int iNeedSize = iArgSize + iBufSize + 1;
#else
        int iNeedSize = iArgSize + 2 * iBufSize + 1;
#endif

        if (iNeedSize > pCmdState->iBufProcChars) {
            pCmdState->lpBufProc = (TCHAR *)cmdutil_ReAlloc (pCmdState->lpBufProc,
                                                sizeof (TCHAR) * iNeedSize);
            pCmdState->iBufProcChars = iNeedSize;
        }

#if defined (UNDER_CE)
        memcpy (&pCmdState->lpBufProc[iBufSize], lpArgs, sizeof(TCHAR) * iArgSize);
#else
        memcpy (&pCmdState->lpBufProc[iBufSize], pCmdState->lpBufProc,
                                                        sizeof(TCHAR) * iBufSize);
        
        if (*lpArgs != TEXT('\0')) {
            pCmdState->lpBufProc[2 * iBufSize - 1] = TEXT(' ');
            memcpy (&pCmdState->lpBufProc[2 * iBufSize], lpArgs, sizeof(TCHAR) * iArgSize);
        }
#endif

        pCmdState->ehHash.build ();

#if defined (UNDER_CE)
    int fShareHandles  = FALSE;
    int fCreationFlag  = fDetach ? CREATE_NEW_CONSOLE : 0;
    TCHAR    *lpStdioNames[3];

        for (int i = 0 ; i < 3 ; ++i) {
            if (pCmdState->fpIO[i] != gIO[i]) {
                if (i != CMDIO_IN)
                    fclose (pCmdState->fpIO[i]);

                TCHAR sBuffer[CMD_BUF_REG];
                DWORD dwBufLen = CMD_BUF_REG;
                GetStdioPathW(i, sBuffer, &dwBufLen);
                lpStdioNames[i] = cmdutil_tcsdup (sBuffer);
                SetStdioPathW(i, pCmdState->lpIO[i]);
            } else if (fDetach) {
                TCHAR sBuffer[CMD_BUF_REG];
                DWORD dwBufLen = CMD_BUF_REG;
                GetStdioPathW(i, sBuffer, &dwBufLen);
                lpStdioNames[i] = cmdutil_tcsdup (sBuffer);
                SetStdioPathW(i, TEXT(""));
            } else
                lpStdioNames[i] = NULL;
        }
#else
        int fShareHandles = TRUE;
        int fCreationFlag = fDetach ? CREATE_NEW_CONSOLE : 0;
        int hOrgStdio[3];

        for (int i = 0 ; i < 3 ; ++i) {
            if (pCmdState->fpIO[i] != gIO[i]) {
                hOrgStdio[i] = dup (_fileno(gIO[i]));
                dup2 (_fileno(pCmdState->fpIO[i]), _fileno(gIO[i]));
            }
        }
#endif

        BOOL bCP;
        HANDLE hProcess;
        PROCESS_INFORMATION pi;
        STARTUPINFO     si;

        memset (&pi, 0, sizeof(pi));
        memset (&si, 0, sizeof(si));
        si.cb = sizeof(si);

        // The original code used ShellExecuteEx to launch the application.
        // This won't work anymore as ShellExecuteEx returns a pseudo-handle
        // to the process launched and we can't use a pseudo-handle to
        // terminate a process.
        bCP = CreateProcess (pCmdState->lpBufProc, &pCmdState->lpBufProc[iBufSize],
                    NULL, NULL, fShareHandles, fCreationFlag, NULL,
                    NULL, &si, &pi);
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
        if (bCP) {
            CloseHandle (pi.hThread);
            CloseHandle (pi.hProcess);
        }

#if defined (UNDER_CE)
        for (i = 0 ; i < 3 ; ++i) {
            if ((pCmdState->fpIO[i] != gIO[i]) && (i != CMDIO_IN))
#pragma warning ( push )
#pragma warning ( disable : 4996 )
// allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
                pCmdState->fpIO[i] = _tfopen (pCmdState->lpIO[i], TEXT("at"));
#pragma warning ( pop )

            if (lpStdioNames[i]) {
                SetStdioPathW(i, lpStdioNames[i]);
                cmdutil_Free (lpStdioNames[i]);
            }
        }
#else
        for (i = 0 ; i < 3 ; ++i) {
            if (pCmdState->fpIO[i] != gIO[i]) {
                dup2 (hOrgStdio[i], _fileno(gIO[i]));
                close (hOrgStdio[i]);
            }
        }
#endif

        if (! bCP) {
            cmdutil_Complain (IDS_ERROR_CANTEXECUTE, pCmdState->lpBufProc);
            return CMD_ERROR;
        }
        DWORD dwExitCode = CMD_SUCCESS;

        if (! fDetach) {
            pCmdState->hExecutingProc = hProcess;
            WaitForSingleObject (hProcess, INFINITE);
            pCmdState->hExecutingProc = NULL;
            GetExitCodeProcess (hProcess, &dwExitCode);
        }

        CloseHandle (hProcess);

        pCmdState->iError = dwExitCode;

        return dwExitCode;
    } else {                                    // Use shell to create the process
        cmdutil_Complain (IDS_ERROR_DONTKNOWHOW, lpName);
    }

    return CMD_ERROR;       
}

//
//  Reset per-command fields of the State
//
void cmdexec_ResetFields (void) {
    pCmdState->lpCmdStart = NULL;
    pCmdState->lpArgStart = NULL;

    memset (pCmdState->lpIORedir, 0, sizeof(pCmdState->lpIORedir));
    memset (pCmdState->iIORedirType, 0, sizeof(pCmdState->iIORedirType));

    pCmdState->fCtrlC     = FALSE;

    if (! pCmdState->lpCommandInput)
        pCmdState->uiFlags |= CMD_ECHO_ON;

    pCmdState->uiFlags &= ~CMD_EXEC_SKIPDOWN;
}
//
//  Command interpretation
//
int cmdexec_Dispatch (void) {
    int   fIOReassigned[3];
    TCHAR *lpSavName[3];

    fIOReassigned[0] = fIOReassigned[1] = fIOReassigned[2] = FALSE;
    lpSavName[0] = lpSavName[1] = lpSavName[2] = NULL;

    long lInFilePos  = 0;
    int  fRedirError = FALSE;
    int  iRedirError = FALSE;

    for (int i = 0 ; i < 3 ; ++i) {
        if (! pCmdState->lpIORedir[i])
            continue;

        TCHAR sBuffer[CMD_BUF_REG];

        if (! cmdutil_FormPath (sBuffer, CMD_BUF_REG, pCmdState->lpIORedir[i], pCmdState->sCwd)) {
            cmdutil_Complain (IDS_ERROR_GENERAL_BADNAME, pCmdState->lpCmdStart, pCmdState->lpIORedir[i]);
            fRedirError = TRUE;
            break;
        }

        if ((i == CMDIO_IN) && (pCmdState->fpIO[i] != stdin))
            lInFilePos = ftell (pCmdState->fpIO[i]);

#pragma warning ( push )
#pragma warning ( disable : 4996 )
// allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
        FILE *fp = _tfopen (sBuffer, gIOM[i][pCmdState->iIORedirType[i]]);
#pragma warning ( pop )
        
        if (!fp) {
            cmdutil_Complain (i == CMDIO_IN ? IDS_ERROR_GENERAL_CANTSTDIN :
                    i == CMDIO_OUT ? IDS_ERROR_GENERAL_CANTSTDOUT :
                    IDS_ERROR_GENERAL_CANTSTDERR, sBuffer);
            iRedirError = TRUE;
            break;
        }

        if (pCmdState->fpIO[i] != gIO[i])
            fclose (pCmdState->fpIO[i]);

        fIOReassigned[i]   = TRUE;
        lpSavName[i]       = pCmdState->lpIO[i];
        pCmdState->lpIO[i] = cmdutil_tcsdup (sBuffer);
        pCmdState->fpIO[i] = fp;
    }

    int iIndex = cmdexec_FindCommand (pCmdState->lpCmdStart);
    int iResult = CMD_ERROR;

    if (! iRedirError) {
        if (iIndex == -1)
            iResult = cmdexec_TryProgram (pCmdState->lpCmdStart, pCmdState->lpArgStart, FALSE);
        else if (_tcscmp (pCmdState->lpArgStart, TEXT("/?")) == 0)
            iResult = cmdexec_CmdHelp (TEXT("help"), pCmdState->lpCmdStart);
        else
            iResult = aAllCommands[iIndex].proc (pCmdState->lpCmdStart, pCmdState->lpArgStart);
    }

    for (i = 0 ; i < 3 ; ++i) {
        if (! fIOReassigned[i])
            continue;

        if (pCmdState->fpIO[i] != gIO[i])
            fclose (pCmdState->fpIO[i]);

        if (pCmdState->lpIO[i])
            cmdutil_Free (pCmdState->lpIO[i]);

        pCmdState->lpIO[i] = lpSavName[i];

        if (! lpSavName[i]) {
            pCmdState->fpIO[i] = gIO[i];
            continue;
        }
        
#pragma warning ( push )
#pragma warning ( disable : 4996 )
// allow fopen here since fopen_s opens for exclusive access and we want to
// support full file sharing for compatibility.
        pCmdState->fpIO[i] = _tfopen (pCmdState->lpIO[i], gIOM[i][0]);
#pragma warning ( pop )
        
        if (! pCmdState->fpIO[i]) {
            pCmdState->fpIO[i] = gIO[i];
            
            if (pCmdState->lpIO[i]) {
                cmdutil_Free (pCmdState->lpIO[i]);
                pCmdState->lpIO[i] = NULL;
            }

            cmdutil_Complain (IDS_ERROR_GENERAL_IOBROKEN);
            continue;
        }

        if (i == CMDIO_IN)
            fseek (pCmdState->fpIO[i], lInFilePos, SEEK_SET);
    }

    return iResult;
}

int cmdexec_Execute (void) {
    cmdexec_ExpandMacros ();
    
    PipeDescriptor *pPDHead;

    if (cmdpipe_CheckForPipes (pPDHead) != CMD_SUCCESS)
        return CMD_ERROR;

    PipeDescriptor *pPD = pPDHead;

    int iRes = CMD_SUCCESS;
    
    do {
        if (cmdpipe_Recharge (pPD) != CMD_SUCCESS)
            break;

        cmdexec_ResetFields ();

        if (! cmdexec_FactorCommand ())
            return CMD_SUCCESS;

        iRes = cmdexec_Dispatch ();
    } while (pPD && (NULL != (pPD = pPD->pNext)));

    cmdpipe_Release (pPDHead);

    return iRes;
}

