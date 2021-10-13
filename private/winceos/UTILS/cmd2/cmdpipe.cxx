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
#include <intsafe.h>

#include "cmd.hxx"
#include "cmdpipe.hxx"
#include "cmdrsc.h"

extern CmdState *pCmdState;
//
//  Code section
//
int cmdpipe_CreateNewFile (TCHAR *lpBuffer, int iBufSize) {
    TCHAR *lpTempDir = pCmdState->ehHash.hash_get (TEXT("TEMP"));

    if (lpTempDir)
        StringCchCopy (lpBuffer, iBufSize, lpTempDir);
    else
        StringCchCopy (lpBuffer, iBufSize, TEXT("\\"));

    TCHAR *lpEndBuf = _tcschr (lpBuffer, TEXT('\0'));
    
    ASSERT (lpEndBuf != lpBuffer);

    if (lpEndBuf - lpBuffer + 14 > iBufSize)
        return FALSE;

    if (*(lpEndBuf-1) != TEXT('\\'))
        *lpEndBuf++ = TEXT('\\');

    memcpy (lpEndBuf, TEXT("cmdpipe.000"), sizeof(TEXT("cmdpipe.000")));
    lpEndBuf += 8;

    for (int i = 0 ; i < 999 ; ++i) {
        lpEndBuf[0] = (TCHAR)(i / 100 + TEXT('0'));
        lpEndBuf[1] = (TCHAR)((i / 10) % 10 + TEXT('0'));
        lpEndBuf[2] = (TCHAR)(i % 10 + TEXT('0'));

        HANDLE hFile = CreateFile (lpBuffer, GENERIC_WRITE, 0, NULL,
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle (hFile);
            return TRUE;
        }
    }

    return FALSE;
}

int cmdpipe_CheckForPipes (PipeDescriptor *&ppd) {
    TCHAR           *lpCmd   = pCmdState->lpBufCmd;
    TCHAR           *lpStart = lpCmd;
    int             inQuotes = FALSE;
    int             iRedirIn = FALSE;
    int             iRedirOut= FALSE;
    PipeDescriptor  *pPDCur  = NULL;

    ppd = NULL;

    for ( ; ; ++lpCmd) {
        if (*lpCmd == TEXT('"'))
            inQuotes = ! inQuotes;
        else if (*lpCmd == TEXT('>')) {
            if ((lpCmd == lpStart) || (*(lpCmd - 1) != TEXT('2')))
                iRedirOut = TRUE;
        } else if (*lpCmd == TEXT('<'))
            iRedirIn = TRUE;
        else if ((*lpCmd == TEXT('\0')) || (((*lpCmd == TEXT('|')) ||
                            (*lpCmd == TEXT('&'))) && (! inQuotes))) {
            int fEnd = FALSE;

            if (*lpCmd == TEXT('\0')) {
                if (! pPDCur)
                    return CMD_SUCCESS;

                fEnd = TRUE;
            }

            int fOutput = (*lpCmd == TEXT('|'));

            if ((lpCmd == lpStart) || (iRedirIn && pPDCur && pPDCur->lpFileName) || (iRedirOut && fOutput)) {
                cmdutil_Complain (IDS_ERROR_BADSYNTAX, TEXT("|"));
                cmdpipe_Release (ppd);
                return CMD_ERROR;
            }

            PipeDescriptor *pPDNew = (PipeDescriptor *)cmdutil_Alloc (sizeof(PipeDescriptor));

            *lpCmd               = TEXT('\0');

            iRedirIn = iRedirOut = FALSE;
            pPDNew->lpCommand    = cmdutil_tcsdup (lpStart);
            pPDNew->iCommandSize = lpCmd - lpStart + 1;

            lpStart            = lpCmd + 1;

            pPDNew->pPrev = pPDCur;

            if (pPDCur)
                pPDCur->pNext = pPDNew;
            else
                ppd = pPDNew;

            pPDCur = pPDNew;

            if (fOutput) {
                TCHAR sBuffer[CMD_BUF_REG];

                if (! cmdpipe_CreateNewFile (sBuffer, CMD_BUF_REG)) {
                    cmdutil_Complain (IDS_ERROR_PIPE_CANT);
                
                    cmdpipe_Release (ppd);
                
                    return CMD_ERROR;
                }
                pPDNew->lpFileName = cmdutil_tcsdup (sBuffer);
            } else {
                pPDNew->lpFileName = NULL;

                if (fEnd)
                    break;
            }
        }
    }
    
    return CMD_SUCCESS;
}

int cmdpipe_Recharge (PipeDescriptor *pPD) {
    if (! pPD)
        return CMD_SUCCESS;

    int iFileOutSize = pPD->lpFileName ? _tcslen (pPD->lpFileName) : 0;
    int iFileInSize = (pPD->pPrev && pPD->pPrev->lpFileName) ? _tcslen (pPD->pPrev->lpFileName) : 0;
    UINT iSz = 0;

        if (FAILED(CeUIntAdd4(pPD->iCommandSize, iFileInSize, iFileOutSize, 6, &iSz)) || ((int)iSz < 0))
            return CMD_ERROR;

    if (pCmdState->iBufCmdChars < (int)iSz) {
        pCmdState->iBufCmdChars = iSz;
        pCmdState->lpBufCmd = (TCHAR *)cmdutil_ReAlloc (pCmdState->lpBufCmd, pCmdState->iBufCmdChars * sizeof (TCHAR));
    }

    memcpy (pCmdState->lpBufCmd, pPD->lpCommand, pPD->iCommandSize * sizeof(TCHAR));
    
    int iCurrentOffset = pPD->iCommandSize - 1;

    if (iFileOutSize > 0) {
        memcpy (&pCmdState->lpBufCmd[iCurrentOffset], TEXT(" > "), sizeof(TCHAR) * 3);
        iCurrentOffset += 3;
        memcpy (&pCmdState->lpBufCmd[iCurrentOffset], pPD->lpFileName, (iFileOutSize + 1) * sizeof(TCHAR));
        iCurrentOffset += iFileOutSize;
    }

    if (iFileInSize > 0) {
        memcpy (&pCmdState->lpBufCmd[iCurrentOffset], TEXT(" < "), sizeof(TCHAR) * 3);
        iCurrentOffset += 3;
        memcpy (&pCmdState->lpBufCmd[iCurrentOffset], pPD->pPrev->lpFileName, (iFileInSize + 1) * sizeof(TCHAR));
        iCurrentOffset += iFileInSize;
    }

    ASSERT (iCurrentOffset < pCmdState->iBufCmdChars);

    if (pPD->pPrev && pPD->pPrev->pPrev) {  // Retire the prev prev output... no longer needed.
        TCHAR *lpFileName = pPD->pPrev->pPrev->lpFileName;
        if (lpFileName)
            DeleteFile (lpFileName);
        
        pPD->pPrev->pPrev->uiPipeFlags |= PIPE_DELETED;
    }

    return CMD_SUCCESS;
}

int cmdpipe_Release (PipeDescriptor *pPD) {
    while (pPD) {
        PipeDescriptor *pNext = pPD->pNext;
        
        if (pPD->lpFileName) {
            if (! (pPD->uiPipeFlags & PIPE_DELETED))
                DeleteFile (pPD->lpFileName);
            cmdutil_Free (pPD->lpFileName);
        }

        cmdutil_Free (pPD->lpCommand);
        cmdutil_Free (pPD);

        pPD = pNext;
    }
    return CMD_SUCCESS;
}

