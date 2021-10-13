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

#include <tchar.h>
#include <stdio.h>

#if ! defined (UNDER_CE)
#include <stddef.h>
#endif

#if defined (UNDER_CE)
#include <windev.h>
#include <console.h>
#endif

#include <intsafe.h>

#include "cmd.hxx"
#include "cmdrsc.h"

extern CmdState *pCmdState;

//
//  File has one of invisible attributes
//
int cmdutil_IsInvisible (TCHAR *lpName) {
    return (GetFileAttributes (lpName) & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)) != 0;
}

//
//  Console pause
//
void cmdutil_ConsolePause (void) {
#if defined (UNDER_CE)
    USE_CONIOCTL_CALLS
    HANDLE h = _fileno(stdin);

    // get & save current console mode. This also checks if the device is a console
    DWORD dwOldMode = 0;
    if(!CeGetConsoleMode(h, &dwOldMode))
        return;
        
    // set new input mode oldmode with echo-off & char-by-char
    DWORD dwNewMode = dwOldMode & (~(CECONSOLE_MODE_ECHO_INPUT | CECONSOLE_MODE_LINE_INPUT));
    CeSetConsoleMode(h, dwNewMode);

    fflush (stdin);
    fflush (stdout);

    CeFlushConsoleInput(h);

    // read one char directly from input device
    char  ch = 0;
    DWORD dwRead = 0;

    ReadFile(h, &ch, 1, &dwRead, NULL);

    // restore previous input mode
    if(dwOldMode) {
        CeFlushConsoleInput(h);
        CeSetConsoleMode(h, dwOldMode);
    }
        
    // Check for Ctrl-C and Esc or EOF to abort cmd
    if ((ch == 0x03) || (ch == 0x1B))
        pCmdState->fCtrlC = TRUE;
#else
    getch ();
#endif
}

//
//  Error processing
//
void cmdutil_Abort (int iFormat, ...) {
    TCHAR   format[CMD_BUF_BIG];

    LoadString (pCmdState->hInstance, iFormat, format, CMD_BUF_BIG);

    va_list args;
    va_start (args, iFormat);
    _vftprintf (STDERR, format, args);
    va_end (args);

    exit (CMD_ERROR);
}

//
//  Error processing
//
void cmdutil_Complain (int iFormat, ...) {
    TCHAR   format[CMD_BUF_BIG];

    LoadString (pCmdState->hInstance, iFormat, format, CMD_BUF_BIG);

    va_list args;
    va_start (args, iFormat);
    _vftprintf (STDERR, format, args);
    va_end (args);
}

//
//
//
int cmdutil_Confirm (int iFormat, ...) {
    TCHAR   format[CMD_BUF_BIG];

    LoadString (pCmdState->hInstance, iFormat, format, CMD_BUF_BIG);

    va_list args;
    va_start (args, iFormat);
    _vftprintf (STDOUT, format, args);
    va_end (args);
    if ((STDIN == stdin) && (iFormat == IDS_ANYKEY)) {
        cmdutil_ConsolePause ();
        _fputts (TEXT("\n"), STDOUT);
        return TRUE;
    }

    TCHAR sBuffer[CMD_BUF_COMMAND];
    _fgetts (sBuffer, CMD_BUF_COMMAND, STDIN);
    
    return _ttoupper(sBuffer[0]) == gYes[0];
}

//
//  Memory manager
//
void *cmdutil_Alloc (int iSize) {
    ASSERT (iSize > 0);

    void *res = LocalAlloc (LMEM_FIXED | LMEM_ZEROINIT, iSize);

    if (! res)
        cmdutil_Abort (IDS_ABORT_MEMORY);

    return res;
}

void *cmdutil_ReAlloc (void *lpBlock, int iNewSize) {
    ASSERT (iNewSize > 0);

    void *res = LocalReAlloc (lpBlock, iNewSize, LMEM_MOVEABLE);

    if (! res)
        cmdutil_Abort (IDS_ABORT_MEMORY);

    return res;
}

void cmdutil_Free (void *lpBlock) {
#ifdef DEBUG
    HLOCAL hRes;
#endif
    ASSERT (lpBlock);
#ifdef DEBUG
    hRes = 
#endif
        LocalFree(lpBlock);
    ASSERT (hRes == NULL);
}

TCHAR *cmdutil_tcsdup (TCHAR *lpStr) {
    int sz = (_tcslen (lpStr) + 1) * sizeof(TCHAR);
    TCHAR *lpDup = (TCHAR *)cmdutil_Alloc (sz);
    memcpy (lpDup, lpStr, sz);
    return lpDup;
}

//
//  Hash support
//
EnvEntry *EnvHash::hash_set (TCHAR *lpVarName, TCHAR *lpValue) {
    int iBucket = hashval (lpVarName);
    EnvEntry *pEERun  = eeHashTable[iBucket];
    EnvEntry *pEEPrev = NULL;

    ASSERT ((! lpValue) || (lpValue[0] != TEXT('\0')));

    while (pEERun) {
        if (_tcscmp (pEERun->lpName, lpVarName) == 0) {
            free_env ();

            if (! lpValue) {
                if (! pEEPrev)
                    eeHashTable[iBucket] = pEERun->pNextEE;
                else
                    pEEPrev->pNextEE = pEERun->pNextEE;

                cmdutil_Free (pEERun->lpName);
                cmdutil_Free (pEERun->lpValue);
                cmdutil_Free (pEERun);

                ASSERT (iEENum > 0);

                --iEENum;

                return NULL;
            }

            cmdutil_Free (pEERun->lpValue);
            pEERun->lpValue = cmdutil_tcsdup (lpValue);

            return pEERun;
        }
    
        pEEPrev = pEERun;
        pEERun  = pEERun->pNextEE;
    }

    if (! lpValue)
        return NULL;

    free_env ();

    pEERun = (EnvEntry *)cmdutil_Alloc (sizeof(*pEERun));

    pEERun->lpName  = cmdutil_tcsdup (lpVarName);
    pEERun->lpValue = cmdutil_tcsdup (lpValue);
    pEERun->pNextEE = eeHashTable[iBucket];

    eeHashTable[iBucket]  = pEERun;
    ++iEENum;

    return pEERun;
}

TCHAR *EnvHash::hash_get (TCHAR *lpVarName) {
    int iBucket = hashval (lpVarName);
    EnvEntry *pEERun  = eeHashTable[iBucket];

    while (pEERun) {
        if (_tcscmp (pEERun->lpName, lpVarName) == 0)
            return pEERun->lpValue;

        pEERun  = pEERun->pNextEE;
    }

    return NULL;
}

int __cdecl cmdutil_EECompare (const void *elem1, const void *elem2 ) {
    EnvEntry *pE1 = *(EnvEntry **)elem1;
    EnvEntry *pE2 = *(EnvEntry **)elem2;

    return _tcscmp (pE1->lpName, pE2->lpName);
}

TCHAR   *EnvHash::build (void) {
    if (lpEnv)
        return lpEnv;

    int iSize = 1;

    EnvEntry *pSortedBuf[CMD_EE_SORTED_SIZE];
    EnvEntry **ppEESorted = (iEENum < CMD_EE_SORTED_SIZE) ? pSortedBuf :
                        (EnvEntry **)cmdutil_Alloc (sizeof(EnvEntry *) * iEENum);

    int i = 0;

    for (int iBucket = 0 ; iBucket < CMD_EHASH_BUCKETS ; ++iBucket) {
        EnvEntry *pEERun = eeHashTable[iBucket];
        while (pEERun) {
            ppEESorted[i++] = pEERun;

            iSize += _tcslen (pEERun->lpName) + _tcslen (pEERun->lpValue) + 2;
            pEERun = pEERun->pNextEE;
        }
    }

    ASSERT (i == iEENum);

    qsort (ppEESorted, iEENum, sizeof (EnvEntry *), cmdutil_EECompare);

    TCHAR   *lpBase = lpEnv = (TCHAR *)cmdutil_Alloc (iSize * sizeof(TCHAR));

    for (i = 0 ; i < iEENum ; ++i) {
        int len = _tcslen (ppEESorted[i]->lpName);
        memcpy (lpBase, ppEESorted[i]->lpName, sizeof (TCHAR) * len);
        lpBase += len;

        *lpBase++ = TEXT('=');

        len = _tcslen (ppEESorted[i]->lpValue);
        memcpy (lpBase, ppEESorted[i]->lpValue, sizeof (TCHAR) * len);
        lpBase += len;

        *lpBase++ = TEXT('\0');
    }

    *lpBase++ = TEXT('\0');

    ASSERT (lpBase == lpEnv + iSize);

    if (ppEESorted != pSortedBuf)
        cmdutil_Free (ppEESorted);

    return lpEnv;
}

//
//  Form path from local directory and proposed extension
//
//  FIXLATER: too restrictive on sizes: requires that both Cwd and Offset fit in buffer
//  when offset is relative to Cwd. Technically, since .. can remove significant part of
//  original directory, this restriction can be lifted by more careful programming...
//  Same applies for quotes.
//
int cmdutil_FormPath (TCHAR *lpBuffer, int iBufSize, TCHAR *lpOffset, TCHAR *lpCwd) {
    ASSERT (*lpOffset != TEXT('\0'));

    int iOffsetSize = _tcslen (lpOffset) + 1;

    if (*lpOffset == TEXT('\\')) {
        if (iOffsetSize > iBufSize)
            return FALSE;
        
        memcpy (lpBuffer, lpOffset, sizeof(TCHAR) * iOffsetSize);
        
    } else {
        int iCwdSize = _tcslen (lpCwd);

        if (iOffsetSize + iCwdSize + 1 > iBufSize) //+1 is necessary for trailing backslash
            return FALSE;

        if (iCwdSize > 0) {
            memcpy (lpBuffer, lpCwd, sizeof(TCHAR) * iCwdSize);
            if (lpBuffer[iCwdSize - 1] != TEXT('\\')) {
                lpBuffer[iCwdSize] = TEXT('\\');
                ++iCwdSize;
            }
        }

        memcpy (&lpBuffer[iCwdSize], lpOffset, iOffsetSize* sizeof(TCHAR));
        
        iOffsetSize += iCwdSize;
    }

    if ((iOffsetSize > 1) && (lpBuffer[iOffsetSize - 1] == TEXT('\\')))
        lpBuffer[iOffsetSize - 1] = TEXT('\0');

    ASSERT (lpBuffer[0] == TEXT('\\'));

    //
    //  Strings concatenated. Now process dots...
    //
    if (lpBuffer[1] == TEXT('\\')) {    // Are we dealing with UNC name?
        lpBuffer += 2;                  // If yes, just skip machine name...
        iBufSize -= 2;
        while ((*lpBuffer != TEXT('\\')) && (*lpBuffer != TEXT('\0'))) {
            ++lpBuffer;
            --iBufSize;
        }

        if (*lpBuffer == TEXT('\0'))    // Where's the share name !?
            return FALSE;
        ++lpBuffer;
        --iBufSize;
                                        // Now skip the share
        while ((*lpBuffer != TEXT('\\')) && (*lpBuffer != TEXT('\0'))) {
            ++lpBuffer;
            --iBufSize;
        }
    }
    
    TCHAR *lpSource = lpBuffer;
    TCHAR *lpTarget = lpBuffer;

    while (*lpSource != TEXT('\0')) {
                if (*lpSource == TEXT('/'))
            *lpSource = TEXT('\\');

        ++lpSource;
    }

    lpSource = lpBuffer;

    while (*lpSource != TEXT('\0')) {
        if (*lpSource == TEXT ('"')) {
            ++lpSource;
            continue;
        }

        if (*lpSource != TEXT('\\')) {
            *lpTarget++ = *lpSource++;
            continue;
        }

        if ((*lpSource == TEXT('\\')) && (lpSource[1] == TEXT('\\'))) {
            ++lpSource;
            continue;
        }

        if ((*lpSource == TEXT('\\')) && (lpSource[1] == TEXT('.')) &&
                    (lpSource[2] == TEXT('.')) && ((lpSource[3] == TEXT('\\')) ||
                    (lpSource[3] == TEXT('\0')))) {
            lpSource += 3;

            if (lpTarget == lpBuffer) // Reached the beginning...
                return FALSE;
            
            *lpTarget = TEXT('\0');     // Clear the way...

            while (*lpTarget != TEXT('\\'))
                --lpTarget;

            ASSERT ((unsigned int)lpTarget >= (unsigned int)lpBuffer);

            *lpTarget = TEXT('\0');     // If necessary, we will make another run down...
            
            continue;
        }
        
        if ((*lpSource == TEXT('\\')) && (lpSource[1] == TEXT('.')) &&
                    ((lpSource[2] == TEXT('\\')) || (lpSource[2] == TEXT('\0')))) {
            lpSource += 2;

            continue;
        }

        *lpTarget++ = *lpSource++;
    }

    //
    //  We may end up having the root backslash erased. Reinstate it...
    //
    if (lpTarget == lpBuffer) {
        *lpTarget++ = TEXT('\\');
        *lpTarget++ = TEXT('\0');
    } else {
        *lpTarget++ = TEXT('\0');
        //  Erase final backslash if any...
        if ((lpTarget - lpBuffer > 2) && (*(lpTarget-2) == TEXT('\\'))) {
            --lpTarget;
            *(lpTarget - 1) = TEXT('\0');
        }
    }

    ASSERT (lpTarget - lpBuffer <= iBufSize);

    return TRUE;
}

//
//  Check if this file exists and it is a directory... Fully qualified name w/o trailing slash expected.
//
int cmdutil_ExistsDir (TCHAR *lpDirName) {
    if (lpDirName[0] == TEXT('\\') && lpDirName[1] == TEXT('\0'))
        return TRUE;

    DWORD dwAttrib = GetFileAttributes (lpDirName);
    if (dwAttrib == 0xffffffff)
        return FALSE;

    return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}

//
//  Check if this file exists and it is not a directory... Fully qualified name w/o trailing slash expected.
//
int cmdutil_ExistsFile (TCHAR *lpFileName) {
    DWORD dwAttrib = GetFileAttributes (lpFileName);
    if (dwAttrib == 0xffffffff)
        return FALSE;

    return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) ? FALSE : TRUE;
}

//
//  Service string functions
//
void cmdutil_RemoveQuotes (TCHAR *lpStart) {
    TCHAR *lpDest = lpStart;
    
    while (*lpStart != TEXT('\0')) {
        if (*lpStart == TEXT('"'))
            ++lpStart;
        else
            *lpDest++ = *lpStart++;
    }
    
    *lpDest = TEXT('\0');
}

int cmdutil_IsWildCard (TCHAR c) {
    return (c == TEXT('*')) || (c == TEXT('?'));
}

int cmdutil_CountNonWhite (TCHAR *lpString) {
    ASSERT (lpString);
    
    TCHAR *lpSavString = lpString;

    while ((*lpString != TEXT('\0')) && (! _istspace (*lpString)))
        ++lpString;

    return lpString - lpSavString;
}

int cmdutil_CountNonWhiteWithQuotes (TCHAR *lpString) {
    ASSERT (lpString);
    
    int inQuotes = FALSE;

    TCHAR *lpSavString = lpString;

    while ((*lpString != TEXT('\0')) && ((! _istspace (*lpString)) || inQuotes)) {
        if (*lpString == TEXT('"'))
            inQuotes = ! inQuotes;

        ++lpString;
    }

    return lpString - lpSavString;
}

int cmdutil_CountWhite (TCHAR *lpString) {
    ASSERT (lpString);

    TCHAR *lpSavString = lpString;

    while ((*lpString != TEXT('\0')) && _istspace (*lpString))
        ++lpString;

    return lpString - lpSavString;
}

void cmdutil_Trim (TCHAR *lpString) {
    ASSERT (lpString);
    
    int iWhiteCount = 0;

    while (*lpString != TEXT('\0')) {
        if (_istspace (*lpString))
            iWhiteCount++;
        else
            iWhiteCount = 0;
        
        ++lpString;
    }

    lpString -= iWhiteCount;
    
    ASSERT ((*lpString == TEXT('\0')) || _istspace(*lpString));

    *lpString = TEXT('\0');
}

int cmdutil_HasWildCards (TCHAR *lpString) {
    while (*lpString != TEXT('\0')) {
        if (cmdutil_IsWildCard (*lpString))
            return TRUE;
        
        ++lpString;
    }

    return FALSE;
}

int cmdutil_NeedQuotes (TCHAR *lpString) {
    while (*lpString != TEXT('\0')) {
        if (_istspace(*lpString) || (*lpString == TEXT('|')) || (*lpString == TEXT('&')))
            return TRUE;
        ++lpString;
    }

    return FALSE;
}
//
//  Fixedmem utilities (alignment is double. Not MT-safe within the same block!)
//
#define FIXEDMEM_INCREMENT  16
#define FIXEDMEM_MAGIC      'MEMF'

struct FixedMemBlock {
    FixedMemBlock   *pNext;
    double          vData;
};

struct FixedMemDescr {
    int             iSize;
    int             iMagic;
    void            *pvFreeList;
    FixedMemBlock   *pFirstBlock;
};

void *cmdutil_GetFMem (int size) {
    if (size < sizeof(void *))
        size = sizeof (void *);
    
    FixedMemDescr *pMD = (FixedMemDescr *)cmdutil_Alloc (sizeof(FixedMemDescr));
    pMD->iSize  = size;
    pMD->iMagic = FIXEDMEM_MAGIC;
    return (void *)pMD;
}

void *cmdutil_FMemAlloc (void *pvBlock) 
{
    FixedMemDescr   *pdBlock = (FixedMemDescr *)pvBlock;

    ASSERT (pdBlock->iMagic == FIXEDMEM_MAGIC);

    if (pdBlock->pvFreeList) {
        void *pData = pdBlock->pvFreeList;
        pdBlock->pvFreeList = *(void **)pdBlock->pvFreeList;

        return pData;
    }

    UINT uLen = 0;
    if (FAILED(UIntMult(pdBlock->iSize, FIXEDMEM_INCREMENT, &uLen)))
        return 0;
    
    UINT sz = 0;
    if (FAILED(UIntAdd(offsetof (FixedMemBlock, vData), uLen, &sz)))
        return 0;

    FixedMemBlock *pNewBlock = (FixedMemBlock *)cmdutil_Alloc (sz);
    pNewBlock->pNext = pdBlock->pFirstBlock;
    pdBlock->pFirstBlock = pNewBlock;

    pdBlock->pvFreeList = &pNewBlock->vData;

    unsigned char *pucRunner = (unsigned char *)pdBlock->pvFreeList;

    for (int i = 0 ; i < FIXEDMEM_INCREMENT - 2 ; ++i) {
        *(void **)pucRunner = pucRunner + pdBlock->iSize;
        pucRunner += pdBlock->iSize;
    }

    *(void **)pucRunner = NULL;

    return (void *)(pucRunner + pdBlock->iSize);
}

void cmdutil_FMemFree (void *pvBlock, void *pvData) {
    FixedMemDescr   *pdBlock = (FixedMemDescr *)pvBlock;

    ASSERT (pdBlock->iMagic == FIXEDMEM_MAGIC);

    *(void **)pvData = pdBlock->pvFreeList;

    pdBlock->pvFreeList = pvData;
}

void cmdutil_ReleaseFMem (void *pvBlock) {
    FixedMemDescr   *pdBlock = (FixedMemDescr *)pvBlock;

    ASSERT (pdBlock->iMagic == FIXEDMEM_MAGIC);

    FixedMemBlock *pbRunner = pdBlock->pFirstBlock;

    while (pbRunner) {
        FixedMemBlock *pbNext = pbRunner->pNext;
        cmdutil_Free (pbRunner);
        pbRunner = pbNext;
    }

    cmdutil_Free (pvBlock);
}

//
//  Fixedmem helper macros for pointer lists
//
PointerList *cmdutil_NewPointerListItem (void *pvData, PointerList *ppl) {
    PointerList *pplNew = (PointerList *)cmdutil_FMemAlloc (pCmdState->pListMem);
    pplNew->pNext = ppl;
    pplNew->pData = pvData;

    return pplNew;
}

void cmdutil_FixedFreePointerList (PointerList *ppl) {
    while (ppl) {
        PointerList *pplNext = ppl->pNext;
        cmdutil_FMemFree (pCmdState->pListMem, ppl);
        ppl = pplNext;
    }
}

PointerList *cmdutil_InvertPointerList (PointerList *ppl) {
    PointerList *pplNewHead = NULL;

    while (ppl) {
        PointerList *pplThisItem = ppl;
        ppl = ppl->pNext;
        pplThisItem->pNext = pplNewHead;
        pplNewHead = pplThisItem;
    }

    return pplNewHead;
}
//
//  Parse command arguments into pointer lists. Returns true if more than one file (or wildcards).
//  String is destroyed!
//
int cmdutil_ParseCommandParameters
(
PointerList *&pplFiles,
PointerList *&pplOptions,
TCHAR *pString
) {
    int iNums = 0;
    pplFiles = NULL;
    pplOptions = NULL;

    while (*(pString += cmdutil_CountWhite (pString)) != TEXT('\0')) {
        TCHAR *pszFileName = NULL;

        if (*pString == TEXT('/')) {
            pplOptions = cmdutil_NewPointerListItem (pString, pplOptions);
            pString += cmdutil_CountNonWhite (pString);
        } else {
            pszFileName = pString;
            pplFiles = cmdutil_NewPointerListItem (pString, pplFiles);
            ++iNums;

            int inQuotes = FALSE;

            while (*pString != TEXT('\0')) {
                if (*pString == TEXT('"'))
                    inQuotes = ! inQuotes;
                else {
                    if ((! inQuotes) && _istspace (*pString))
                        break;

                    if (cmdutil_IsWildCard (*pString))
                        iNums |= CMD_PARSER_HASWILDCARDS;
                }

                ++pString;
            }
        }

        if (*pString != TEXT('\0')) {
            *pString = TEXT('\0');
            ++pString;
        }

        if (pszFileName)
            cmdutil_RemoveQuotes (pszFileName);
    }

    pplFiles   = cmdutil_InvertPointerList (pplFiles);
    pplOptions = cmdutil_InvertPointerList (pplOptions);

    return iNums;
}

int cmdutil_ParseParamsUniform (PointerList *&pplList, TCHAR *pString) {
    int iNums = 0;
    pplList = NULL;

    while (*(pString += cmdutil_CountWhite (pString)) != TEXT('\0')) {
        pplList = cmdutil_NewPointerListItem (pString, pplList);
        pString += cmdutil_CountNonWhite (pString);

        ++iNums;

        if (*pString == TEXT('\0'))
            break;

        *pString = TEXT('\0');
        ++pString;
    }

    pplList   = cmdutil_InvertPointerList (pplList);

    return iNums;
}

int cmdutil_GetOptionListFromEnv
(
TCHAR       *&pBuffer,
PointerList *&pplOptions,
TCHAR       *pEnvName
) {
    pplOptions = NULL;
    pBuffer    = NULL;

    TCHAR *lpValue = pCmdState->ehHash.hash_get (pEnvName);

    if (! lpValue)
        return TRUE;

    pBuffer = cmdutil_tcsdup (lpValue);
    TCHAR *pString = pBuffer;

    while (*(pString += cmdutil_CountWhite (pString)) != TEXT('\0')) {
        if (*pString == TEXT('/')) {
            pplOptions = cmdutil_NewPointerListItem (pString, pplOptions);
            pString += cmdutil_CountNonWhite (pString);
        } else {
            cmdutil_Complain (IDS_ERROR_GENERAL_NONOPTION, pEnvName);
            cmdutil_Free (pBuffer);
            cmdutil_FixedFreePointerList (pplOptions);
            pBuffer    = NULL;
            pplOptions = NULL;
            return FALSE;
        }

        if (*pString != TEXT('\0')) {
            *pString = TEXT('\0');
            ++pString;
        }
    }

    pplOptions = cmdutil_InvertPointerList (pplOptions);

    return TRUE;
}

//
//  Helper function for ExecuteForPattern
//
//  lpPattern points to place for pattern to be copied to. Backslash separator
//  is the part of the pattern.
//
int cmdutil_MaskAll (TCHAR *lpBuffer) {
    TCHAR *lpPattern = _tcsrchr (lpBuffer, TEXT('\\'));
    if (! lpPattern)
        return FALSE;

    if (_tcscmp (lpPattern, TEXT("\\*.*")) == 0)
        return TRUE;

    if (_tcscmp (lpPattern, TEXT("\\*")) == 0)
        return TRUE;

    return FALSE;
}

int cmdutil_HierarchyMarker (TCHAR *name) {
    if (name[0] != TEXT('.'))
        return FALSE;

    if (name[1] == TEXT('\0'))
        return TRUE;

    if (name[1] != TEXT('.'))
        return FALSE;

    if (name[2] == TEXT('\0'))
        return TRUE;

    return FALSE;
}

int cmdutil_ExecuteForPatternRecursively
(
TCHAR   *lpBuf,
int     iBufSize,
TCHAR   *lpEndOffset,
TCHAR   *lpPattern,
int     iPatternSize,
void    *pvData,
int     iCallForDirs,
int     (*fpWhat)(TCHAR *lpFile, void *pvArg),
int     *pError // optional; could be NULL
) {
    ASSERT (lpBuf && lpEndOffset && lpPattern && fpWhat);
    ASSERT (iBufSize > 0);
    ASSERT (lpEndOffset - lpBuf < iBufSize);
    ASSERT ((iPatternSize > 0) && ((unsigned int)iPatternSize == _tcslen(lpPattern) + 1));

        if (pCmdState->fCtrlC) {
            if (pError && *pError) {
                // pError will be set to 0 even if we find
                // one file which matches the given pattern
                // in that case we don't need to overwrite
                // the error. pError == 0 is success.
                *pError = 0;
            }
        return CMD_ERROR_CTRLC;
        }

    if ((lpEndOffset - lpBuf + iPatternSize > iBufSize)
        || (lpEndOffset - lpBuf + (int)(PATTERN_ALL_CHARS) > iBufSize)) {
            if (pError && *pError)
                *pError = CMD_ERROR_TOOBIG;
            return CMD_ERROR_TOOBIG;
        }

    if (lpEndOffset == lpBuf) {
        lpBuf[0] = TEXT('\\');
        lpBuf[1] = TEXT('\0');
    } else
        *lpEndOffset = TEXT('\0');

    DWORD dwWhatAmI = GetFileAttributes (lpBuf);

    if ((dwWhatAmI == 0xffffffff) || (! (dwWhatAmI & FILE_ATTRIBUTE_DIRECTORY)))
        return CMD_SUCCESS;

    memcpy (lpEndOffset, lpPattern, iPatternSize * sizeof(TCHAR));

    int iLeft = iBufSize - (lpEndOffset - lpBuf + 1);
    int iResult = CMD_SUCCESS;

    WIN32_FIND_DATA wfd;
    HANDLE hSearch;
    
    if (iCallForDirs & DIRORDER_FILES) {
        hSearch = FindFirstFile (lpBuf, &wfd);
        if (hSearch != INVALID_HANDLE_VALUE) {
            do {
                int iFnSz = _tcslen (wfd.cFileName) + 1;
            
                if (iFnSz > iLeft) {
                    iResult = CMD_ERROR_TOOBIG;
                    break;
                }
            
                memcpy (lpEndOffset + 1, wfd.cFileName, iFnSz * sizeof(TCHAR));

                if (! (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (pError)
                        *pError = 0;
                                    iResult = fpWhat (lpBuf, pvData);
                                }
            } while ((iResult == CMD_SUCCESS) && FindNextFile (hSearch, &wfd));

            FindClose (hSearch);

            if (iResult != CMD_SUCCESS)
                return iResult;
        }
    }

    if (iCallForDirs & DIRORDER_PRE) {
        if (lpEndOffset == lpBuf) {
            lpBuf[0] = TEXT('\\');
            lpBuf[1] = TEXT('\0');
        } else
            *lpEndOffset = TEXT('\0');
        iResult = fpWhat (lpBuf, pvData);
    }

    memcpy (lpEndOffset, PATTERN_ALL, sizeof (PATTERN_ALL));

    hSearch = FindFirstFile (lpBuf, &wfd);
    
    if (hSearch != INVALID_HANDLE_VALUE) {
        do {
            if ((! (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ||
                cmdutil_HierarchyMarker (wfd.cFileName))
                continue;

            int iFnSz = _tcslen (wfd.cFileName) + 1;
            
            if (iFnSz > iLeft) {
                iResult = CMD_ERROR_TOOBIG;
                break;
            }

            memcpy (lpEndOffset + 1, wfd.cFileName, iFnSz * sizeof(TCHAR));

            cmdutil_ExecuteForPatternRecursively (lpBuf, iBufSize,
                            lpEndOffset + iFnSz, lpPattern, iPatternSize,
                            pvData, iCallForDirs, fpWhat, pError);
        } while ((iResult == CMD_SUCCESS) && FindNextFile (hSearch, &wfd));

        FindClose (hSearch);
    }

    if (iCallForDirs & DIRORDER_POST) {
        if (lpEndOffset == lpBuf) {
            lpBuf[0] = TEXT('\\');
            lpBuf[1] = TEXT('\0');
        } else
            *lpEndOffset = TEXT('\0');

        iResult = fpWhat (lpBuf, pvData);
    }

    return iResult;
}

//
//  General file recursion utility. lpName is destroyed (used as a buffer!)
//  Originally lpBuf contains a string with possibly embedded wildcards.
//  On function exit it's all garbaged.
//
int cmdutil_ExecuteForPattern
(
TCHAR *lpBuf,
int iBufSize,
void *pvData,
int fRecurse,
int (*fpWhat)(TCHAR *lpFile, void *pvArg)
) {
    TCHAR *lpPattern = _tcsrchr (lpBuf, TEXT('\\'));
    ASSERT (lpPattern);

    int iLeft = iBufSize - (lpPattern - lpBuf + 1);

    int iResult = CMD_SUCCESS;

    if (! fRecurse) {
        WIN32_FIND_DATA wfd;
        HANDLE hSearch = FindFirstFile (lpBuf, &wfd);
        if (hSearch == INVALID_HANDLE_VALUE)
            return CMD_ERROR_FNF;

        do {
            int iFnSz = _tcslen (wfd.cFileName) + 1;
            
            if (iFnSz > iLeft) {
                iResult = CMD_ERROR_TOOBIG;
                break;
            }
            
            memcpy (lpPattern + 1, wfd.cFileName, iFnSz * sizeof(TCHAR));

            if (! (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                iResult = fpWhat (lpBuf, pvData);
        } while ((iResult == CMD_SUCCESS) && FindNextFile (hSearch, &wfd));

        FindClose (hSearch);
        return iResult;
    }

    TCHAR sPattern[CMD_BUF_REG];

    StringCchCopy(sPattern, _countof(sPattern), lpPattern);
    int iError = CMD_ERROR_FNF;
    int iPatternSize = _tcslen (sPattern) + 1;

    iResult = cmdutil_ExecuteForPatternRecursively (lpBuf, iBufSize, lpPattern,
                sPattern, iPatternSize, pvData, DIRORDER_FILES, fpWhat, &iError);
    return ((0 == iError) ? iResult : iError);                  
}

//
//  Delete all files and the directory.
//
int cmdutil_HelperRemoveFiles (TCHAR *lpName, void *pNull) {
    ASSERT (lpName && (! pNull));
    if (! DeleteFile (lpName))
        cmdutil_Complain (IDS_ERROR_CANTREMOVE, lpName);

    return CMD_SUCCESS;
}

int cmdutil_HelperRemoveDirs (TCHAR *lpName, void *pNull) {
    ASSERT (lpName && (! pNull));
    if (! RemoveDirectory (lpName))
        cmdutil_Complain (IDS_ERROR_CANTREMOVE, lpName);

    return CMD_SUCCESS;
}

int cmdutil_DeleteAll (TCHAR *lpDirName, int iBufSize) {
    TCHAR *lpSentinel = lpDirName + _tcslen (lpDirName);
    
    ASSERT (*lpSentinel == TEXT('\0'));

    int iRes = cmdutil_ExecuteForPatternRecursively (lpDirName, iBufSize,
                lpSentinel, PATTERN_ALL, PATTERN_ALL_CHARS, NULL,
                DIRORDER_FILES, cmdutil_HelperRemoveFiles, NULL);

    if (iRes != CMD_SUCCESS)
        return iRes;

    iRes = cmdutil_ExecuteForPatternRecursively (lpDirName, iBufSize,
                lpSentinel, PATTERN_ALL, PATTERN_ALL_CHARS, NULL, DIRORDER_POST,
                cmdutil_HelperRemoveDirs, NULL);

    if (iRes != CMD_SUCCESS)
        return iRes;
    
    *lpSentinel = TEXT('\0');

    return CMD_SUCCESS;
}

//
//  Return string representation of day of week...
//
TCHAR *cmdutil_DayOfWeek (int iDay) {
    
    ASSERT (iDay >= 0 && iDay < 7);

    static TCHAR *ssDayOfWeek[7] = {TEXT("Sun"), TEXT("Mon"), TEXT("Tue"), TEXT("Wed"),
                                        TEXT("Thu"), TEXT("Fri"), TEXT("Sat") };

    return ssDayOfWeek[iDay];
}

//
//  Search path for file
//
int cmdutil_FileStatus (TCHAR *lpFileName, TCHAR *lpExtType) {
    if (! cmdutil_ExistsFile (lpFileName))
        return CMD_FOUND_NONE;

    if (_tcsicmp (lpExtType, TEXT(".EXE")) == 0)
        return CMD_FOUND_EXE;
    
    if (_tcsicmp (lpExtType, TEXT(".COM")) == 0)
        return CMD_FOUND_EXE;
    
    if (_tcsicmp (lpExtType, TEXT(".BAT")) == 0)
        return CMD_FOUND_BATCH;

    if (_tcsicmp (lpExtType, TEXT(".CMD")) == 0)
        return CMD_FOUND_BATCH;

    return CMD_FOUND_SHELLEXEC;
}

int cmdutil_IterateThroughExt (TCHAR *lpBuffer, int iBufSize, TCHAR *lpExtList) {
    TCHAR *lpBufEnd = _tcschr (lpBuffer, TEXT('\0'));
    
    while (*lpExtList != TEXT('\0')) {

        if (*lpExtList == TEXT(';')) {
            ++lpExtList;
            continue;
        }

        TCHAR *lpCurrentEnd = lpBufEnd;

        while ((*lpExtList != TEXT('\0')) && (*lpExtList != TEXT(';'))) {
            if (lpCurrentEnd - lpBuffer + 1 < iBufSize)
                *lpCurrentEnd++ = *lpExtList;
            
            lpExtList++;
        }
        
        *lpCurrentEnd = TEXT('\0');

        int iRes = cmdutil_FileStatus (lpBuffer, lpBufEnd);
        
        if (iRes != CMD_FOUND_NONE)
            return iRes;
    }

    return CMD_FOUND_NONE;
}

int cmdutil_FindInPath (TCHAR *lpBuffer, int iBufSize, TCHAR *lpName) {
    TCHAR *lpNamePure = _tcsrchr (lpName, TEXT('\\'));
    
    int fSearchPath = lpNamePure == NULL;
    
    if (fSearchPath)
        lpNamePure = lpName;
    else
        ++lpNamePure;

    TCHAR *lpExt = _tcsrchr (lpNamePure, TEXT('.'));

    int iRes = CMD_FOUND_NONE;

    if (! cmdutil_FormPath (lpBuffer, iBufSize, lpName, pCmdState->sCwd))
        return iRes;

    TCHAR *lpExtList = pCmdState->ehHash.hash_get (TEXT("PATHEXT"));

    if (! lpExtList)
        lpExtList = TEXT(".COM;.EXE;.BAT;.CMD");

    if (lpExt)
        iRes = cmdutil_FileStatus (lpBuffer, lpExt);

    if (iRes == CMD_FOUND_NONE)
        iRes = cmdutil_IterateThroughExt (lpBuffer, iBufSize, lpExtList);

    if ((iRes != CMD_FOUND_NONE) || (! fSearchPath))
        return iRes;

    //
    //  At this point we are only looking for pure name...
    //
    TCHAR *lpPath = pCmdState->ehHash.hash_get (TEXT("PATH"));

    if (! lpPath)
        return CMD_FOUND_NONE;

    int   iPureNameSize = _tcslen (lpNamePure) + 1;

    for ( ; ; ) {
        while (*lpPath == TEXT(';'))
            ++lpPath;

        if (*lpPath == TEXT('\0'))
            break;

        TCHAR *lpPathEnd = _tcschr (lpPath, TEXT(';'));
        
        if (lpPathEnd)
            *lpPathEnd = TEXT('\0');

        int iRes2 = *lpPath ? cmdutil_FormPath (lpBuffer, iBufSize, lpPath, pCmdState->sCwd) : FALSE;

        if (lpPathEnd)
            *lpPathEnd = TEXT(';');

        if (! iRes2) {
            cmdutil_Complain (IDS_ERROR_BADPATH);
            return CMD_FOUND_NONE;
        }

        TCHAR *lpNameEnd = _tcschr (lpBuffer, TEXT('\0'));

        ASSERT (lpNameEnd - lpBuffer <= iBufSize);
        ASSERT (lpNameEnd != lpBuffer);

        if ((lpNameEnd - lpBuffer) + iPureNameSize + 1 <= iBufSize) {
            if (*(lpNameEnd - 1) != TEXT('\\'))
                *lpNameEnd++ = TEXT('\\');
            memcpy (lpNameEnd, lpNamePure, sizeof(TCHAR) * iPureNameSize);

            iRes = CMD_FOUND_NONE;

            if (lpExt)
                iRes = cmdutil_FileStatus (lpBuffer, lpExt);

            if (iRes == CMD_FOUND_NONE)
                iRes = cmdutil_IterateThroughExt (lpBuffer, iBufSize, lpExtList);

            if ((iRes != CMD_FOUND_NONE) || (! fSearchPath))
                return iRes;
        }

        if (! lpPathEnd)
            break;

        lpPath = lpPathEnd + 1;
    }

    return CMD_FOUND_NONE;
}
