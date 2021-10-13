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
/*
 *              NK Kernel resource loader code
 *
 *
 * Module Name:
 *
 *              heap.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel resource loader
 *
 *
 */

/* Straight-forward implementation of the resouce API's.  Since resources are
   read-only, we always return the slot 1 (kernel) copy of the resource, which
   is visable to all processes. */

#include "kernel.h"


#define DWORDUP(x) (((x)+3)&~03)

extern CRITICAL_SECTION RFBcs;

//------------------------------------------------------------------------------
// Compare an Ascii string and a Pascal string
//------------------------------------------------------------------------------
int 
StrCmpAPascW(
    LPCSTR astr,
    LPCWSTR pstr
    ) 
{
    int loop = (int)*pstr++;
    while (*astr && loop--)
        if (NKtowlower((WCHAR)*astr++) != NKtowlower(*pstr++))
            return 1;
    return (!*astr && !loop ? 0 : 1);
}




/* xref ref regset
; MUI Register setttins :
;   HKLM\MUI\Enable - enable MUI or not
;   HKLM\MUI\SysLang - system default langid
;   HKCU\MUI\CurLang - langid for current user

[HKEY_LOCAL_MACHINE\MUI]
   ; Update the enable field to enable MUI
   ;"Enable"=dword:1
   ; Update the SysLang field to set system default langid
   ;"SysLang"=dword:0409

[HKEY_CURRENT_USER\MUI]
   ; Update the CurLang field to set user default langid
   ;"CurLang"=dword:0409
*/


#define MAX_MUI_LANG    6
// global array that keeps our current UI language, and default UI language
LANGID g_rgwUILangs[MAX_MUI_LANG]; // at most 5 langs, one extra for array terminator
// 0 == current UI lang
// 1 == MAKELANGID(PRIMARYLANGID(g_rgwUILangs[0]), SUBLANG_DEFAULT)
// 2 == default UI lang
// 3 == MAKELANGID(PRIMARYLANGID(g_rgwUILangs[2]), SUBLANG_DEFAULT)
// 4 == 0x0409 (US English) 
// The array may be smaller since duplicates are eliminated

// Flag that enables/disables MUI globally 0==not inited, 1==inited&enabled 2==inited&disabled
DWORD g_EnableMUI;

// saved path to system MUI DLL. sizeof(\\windows\\ceosXXXX.mui)
//WCHAR g_szSystemMUI[22];



//------------------------------------------------------------------------------
// Read language setting & add uniquely to array
//------------------------------------------------------------------------------
void 
AddLangUnique(
    DWORD dwValue,
    HKEY hkey,
    LPCTSTR szValName
    )
{
    int i;
    DWORD  dwType, dwSize;
    LANGID langid1, langid2;

    if(!dwValue)    {
        dwSize = sizeof(DWORD);
        // load a langid, skip if error
        if( ERROR_SUCCESS!=NKRegQueryValueExW (hkey, szValName, (LPDWORD)L"MUI", &dwType, (LPBYTE)&dwValue, &dwSize) ||
            (dwType != REG_DWORD) || !dwValue ) {
                DEBUGMSG(1,(L"InitMUI: Failed to read regkey %s\r\n", szValName));
                return;
        }
    }               
    langid1 = (LANGID)dwValue;
    langid2 = MAKELANGID(PRIMARYLANGID(langid1), SUBLANG_DEFAULT);
    if(langid2 == langid1)
        langid2 = 0;
    DEBUGMSG(ZONE_LOADER1,(L"InitMUI: Got Langs=%x %x\r\n", langid1, langid2));
    
    // add them uniquely to array
    for(i=0; i < MAX_MUI_LANG; i++) {
        if(!g_rgwUILangs[i]) {
            g_rgwUILangs[i] = langid1;
            g_rgwUILangs[i+1] = langid2;
            break;
        } else if(g_rgwUILangs[i]==langid1) {
            langid1 = 0;
        } else if(g_rgwUILangs[i]==langid2) {
            langid2 = 0;
        }
    }
}




//------------------------------------------------------------------------------
// Called to load MUI resource DLL. NOT in a critsec. Must be re-entrant.
//------------------------------------------------------------------------------
PMODULE LoadMUI (HANDLE hModule, LPBYTE BasePtr, e32_lite* eptr)
{
    WCHAR   szPath[MAX_PATH];
    int     iLen, i;
    DWORD dwLangID;
    HANDLE hRsrc;
    PMODULE pMod;
    DWORD   dwFlags = LOAD_LIBRARY_AS_DATAFILE;

    // if MUI disabled (or not yet inited, e.g. call by filesys.exe), or the module doesn't have
    // resources, then fail immediately
    if ((g_EnableMUI != 1) || !eptr->e32_unit[RES].rva) {
        return NULL;
    }
    // we must be holding LLcs already
    DEBUGCHK (OwnLoaderLock (g_pprcNK));

    // See if module contains a reference to a MUI (resource type=222, id=1). If found,
    // the resource contains a basename to which we append .XXXX.MUI, where XXXX is the language code
    if (g_pfnFindResource
        && (NULL != (hRsrc = (* g_pfnFindResource) (hModule, MAKEINTRESOURCE(ID_MUI), MAKEINTRESOURCE(RT_MUI))))
        && (0 != (iLen = ((resdata_t *)hRsrc)->size))
        && (iLen < sizeof(WCHAR)*(MAX_PATH-10)) ) {
        memcpy(szPath, (BasePtr + ((resdata_t *)hRsrc)->rva), iLen);
        iLen /= sizeof(WCHAR);
        szPath[iLen]=0;
        DEBUGMSG(ZONE_LOADER1,(L"LoadMUI: Found indirection (%s, %d)\r\n", szPath, iLen)); 
    } 
    // otherwise search for local MUI dll, based on module path name
    else  if (0 == (iLen = PROCGetModName (pActvProc, (PMODULE) hModule, szPath, MAX_PATH-10))) {
        DEBUGCHK(FALSE);
        return NULL;

    } else {
        // remove "k." from the mui dll name if found; at this time dlls loaded in kernel and user
        // use the same resources; in future if we change this, then this code should be updated.
        LPCWSTR pPlainName = szPath + iLen;
        while ((pPlainName > szPath) && (*(pPlainName-1) != (WCHAR)'\\') && (*(pPlainName-1) != (WCHAR)'/')) {
            pPlainName --;
        }

        if ((pPlainName >= szPath) && !NKwcsnicmp(pPlainName, L"k.", 2)) {
            // edit the string in-place removing "k." from the name.
            DWORD idx = pPlainName - szPath;
            for (; idx < (NKwcslen(szPath) - 2); ++idx) {
                szPath[idx] = szPath[idx+2];
            }
            szPath[idx] = 0;
            iLen -= 2;
        }
    }        

    if (IsKModeAddr ((DWORD) BasePtr)) {
        dwFlags |= LOAD_LIBRARY_IN_KERNEL;
    }

    for(i=0; g_rgwUILangs[i]; i++)
    {
        // try to find local DLL in each of current/default/english languages
        dwLangID = g_rgwUILangs[i];
        NKwvsprintfW(szPath+iLen, TEXT(".%04X.MUI"), (LPVOID)&dwLangID, (MAX_PATH-iLen));
        DEBUGMSG (ZONE_LOADER1,(L"LoadMUI: Trying %s\r\n",szPath));
        pMod = LoadOneLibrary (szPath, LLIB_NO_MUI, dwFlags);
        if (pMod) { // dont call dllentry/resolve refs etc
            DEBUGMSG (ZONE_LOADER1,(L"LoadMUI: Loaded %s\r\n", szPath)); 
            return pMod;
        }
    }
    return NULL;

}   

//------------------------------------------------------------------------------
// Initialize MUI language globals -- called only once during system startup, from RunApps
//------------------------------------------------------------------------------
void 
InitMUILanguages(void)
{
    DWORD  dwType, dwSize, dwValue;
    
    DEBUGCHK(!g_EnableMUI); // this must be called only once
    if(!SystemAPISets[SH_FILESYS_APIS]) {
        g_EnableMUI = (DWORD)-1;    // if this config has no filesystem then MUI is disabled
        return;
    }
    
    // check if MUI is enabled
    dwSize = sizeof(DWORD);
    if( ERROR_SUCCESS!=NKRegQueryValueExW (HKEY_LOCAL_MACHINE, L"Enable",(LPDWORD)L"MUI", &dwType, (LPBYTE)&dwValue, &dwSize) ||
        (dwType != REG_DWORD) || !dwValue ) {
            DEBUGMSG(1,(L"InitMUI: DISABLED (%d)\r\n", dwValue));
            g_EnableMUI = (DWORD)-1; // disable MUI
            return;
    }

    // memset(g_rgwUILangs, 0, sizeof(g_rgwUILangs)); // not reqd
    // load Current User language, skip if error
    AddLangUnique(0, HKEY_CURRENT_USER, L"CurLang");
    // load System Default language, skip if error
    AddLangUnique(0, HKEY_LOCAL_MACHINE, L"SysLang");
    // Do not load English as default since the default should be whatever built inside the executable.
    // AddLangUnique(0x0409, 0, 0);
    
    DEBUGMSG(1,(L"InitMUI: Langs=%x %x %x %x %x %x\r\n", g_rgwUILangs[0], g_rgwUILangs[1], 
        g_rgwUILangs[2], g_rgwUILangs[3], g_rgwUILangs[4], g_rgwUILangs[5]));


    g_EnableMUI = 1;    // set this as the last thing before exit
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
AToUCopy(
    WCHAR *wptr,
    LPCSTR aptr,
    int maxlen
    ) 
{
    int len;
    for (len = 1; (len<maxlen) && *aptr; len++)
        *wptr++ = (WCHAR)*aptr++;
    *wptr = 0;
}

#define COMP_BLOCK_SIZE VM_PAGE_SIZE

BYTE lpDecBuf[COMP_BLOCK_SIZE];
DWORD cfile=0xffffffff,cblock=0xffffffff;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
NKGetRomFileBytes(
    DWORD type,
    DWORD count,
    DWORD pos,
    LPVOID buffer,
    DWORD nBytesToRead
    ) 
{
    FILESentry *filesptr = NULL;
    DWORD      cbRet = 0;

    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileBytes entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        type, count, pos, buffer, nBytesToRead));

    if (2 == type) {
        DWORD      dwCurCount = count;
        ROMChain_t *pROM = ROMChain;
        TOCentry   *tocptr;
        while (pROM && (dwCurCount >= pROM->pTOC->numfiles)) {
            dwCurCount -= pROM->pTOC->numfiles;
            pROM = pROM->pNext;
        }

        if (pROM) {
            tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
            filesptr = &(((FILESentry *)tocptr)[dwCurCount]);
        }
    }

    if (filesptr && (pos <= filesptr->nRealFileSize)) {

        if (pos + nBytesToRead > filesptr->nRealFileSize) {
            nBytesToRead = filesptr->nRealFileSize - pos;
        }
        if (nBytesToRead && VMLockPages(pVMProc, buffer, nBytesToRead, 0, LOCKFLAG_WRITE)) {

            DWORD dwBlock,bRead,csize;
            LPVOID buffercopy;
            DWORD  dwResult;
            
            cbRet = nBytesToRead;
            buffercopy = buffer;

            EnterCriticalSection(&RFBcs);
            if (filesptr->nRealFileSize == filesptr->nCompFileSize) {
                memcpy(buffer, (LPBYTE)filesptr->ulLoadOffset+pos, nBytesToRead);
            } else if (!filesptr->nCompFileSize) {
                memset(buffer, 0, nBytesToRead);
            } else {
                while (nBytesToRead) {
                    dwBlock = pos / COMP_BLOCK_SIZE;
                    bRead = nBytesToRead;
                    if ((pos + bRead-1) / COMP_BLOCK_SIZE != dwBlock) {
                        bRead = COMP_BLOCK_SIZE - (pos & (COMP_BLOCK_SIZE-1));
                    }
                    if ((count != cfile) || (dwBlock != cblock)) {
                        if (dwBlock == filesptr->nRealFileSize / COMP_BLOCK_SIZE) {
                            csize = filesptr->nRealFileSize - (COMP_BLOCK_SIZE*dwBlock);
                        } else {
                            csize = COMP_BLOCK_SIZE;
                        }
                        if (bRead == csize) {
                            dwResult = DecompressROM((LPBYTE)filesptr->ulLoadOffset,
                                                  filesptr->nCompFileSize, buffer,
                                                  csize, dwBlock*COMP_BLOCK_SIZE);
                            if ((dwResult == CEDECOMPRESS_FAILED) || (dwResult == 0)) {
                                break;
                            }
                        } else {
                            dwResult = DecompressROM((LPBYTE)filesptr->ulLoadOffset,
                                                  filesptr->nCompFileSize, lpDecBuf,
                                                  csize, dwBlock*COMP_BLOCK_SIZE);
                            if ((dwResult == CEDECOMPRESS_FAILED) || (dwResult == 0)) {
                                break;
                            }
                            cfile = count;
                            cblock = dwBlock;
                            memcpy(buffer, lpDecBuf + (pos & (COMP_BLOCK_SIZE-1)), bRead);
                        }
                    } else {
                        memcpy(buffer, lpDecBuf + (pos & (COMP_BLOCK_SIZE-1)), bRead);
                    }
                    buffer = (LPVOID)((LPBYTE)buffer+bRead);
                    nBytesToRead -= bRead;
                    pos += bRead;
                }

                if (nBytesToRead) {
                    // failed
                    cbRet = 0;
                }
            }
            LeaveCriticalSection(&RFBcs);
            VMUnlockPages (pVMProc, buffercopy, cbRet);

        }

    }

    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileBytes exit: %8.8lx\r\n", cbRet));
    return cbRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Return address and size of uncompressed ROM file.  Used to map directly from ROM.
BOOL
FindROMFile(
    DWORD   type,
    DWORD   count,
    LPVOID* ppvAddr,    // Ptr to return physical address of file
    DWORD*  pdwLen      // Ptr to return size of file
    ) 
{
    ROMChain_t *pROM = ROMChain;
    TOCentry *tocptr;
    FILESentry *filesptr;
    
    if (type == 2) {
        while (pROM && (count >= pROM->pTOC->numfiles)) {
            count -= pROM->pTOC->numfiles;
            pROM = pROM->pNext;
        }
        if (pROM) {
            tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
            filesptr = &(((FILESentry *)tocptr)[count]);
            if (filesptr->nRealFileSize == filesptr->nCompFileSize) {
                *ppvAddr = (LPVOID)filesptr->ulLoadOffset;
                *pdwLen = filesptr->nRealFileSize;
                return TRUE;
            }
        }
    }
    
    return FALSE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKGetRomFileInfo(
    DWORD type,
    LPWIN32_FIND_DATA lpfd,
    DWORD count
    ) 
{
    TOCentry *tocptr;
    FILESentry *filesptr;
    ROMChain_t *pROM = ROMChain;
    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo entry: %8.8lx %8.8lx %8.8lx\r\n",type,lpfd,count));

    switch (type) {
        case 1:
            // MODULES section of ROM
            while (pROM && (count >= pROM->pTOC->nummods)) {
                count -= pROM->pTOC->nummods;
                pROM = pROM->pNext;
            }
            if (!pROM) {
                DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",FALSE));
                return FALSE;
            }
            tocptr = &(((TOCentry *)&pROM->pTOC[1])[count]);
            lpfd->dwFileAttributes = (tocptr->dwFileAttributes | FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE) & ~FILE_ATTRIBUTE_ROMSTATICREF;
            lpfd->ftCreationTime = tocptr->ftTime;
            lpfd->ftLastAccessTime = tocptr->ftTime;
            lpfd->ftLastWriteTime = tocptr->ftTime;
            lpfd->nFileSizeHigh = 0;
            lpfd->nFileSizeLow = tocptr->nFileSize;
            AToUCopy(lpfd->cFileName,tocptr->lpszFileName,MAX_PATH);
            DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",TRUE));
            return TRUE;
        case 2:
            // FILES section of ROM
            while (pROM && (count >= pROM->pTOC->numfiles)) {
                count -= pROM->pTOC->numfiles;
                pROM = pROM->pNext;
            }
            if (!pROM) {
                DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",FALSE));
                return FALSE;
            }
            tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
            filesptr = &(((FILESentry *)tocptr)[count]);
            lpfd->dwFileAttributes = filesptr->dwFileAttributes | FILE_ATTRIBUTE_INROM;
            lpfd->ftCreationTime = filesptr->ftTime;
            lpfd->ftLastAccessTime = filesptr->ftTime;
            lpfd->ftLastWriteTime = filesptr->ftTime;
            lpfd->nFileSizeHigh = 0;
            lpfd->nFileSizeLow = filesptr->nRealFileSize;
            AToUCopy(lpfd->cFileName,filesptr->lpszFileName,MAX_PATH);
            DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",TRUE));
            return TRUE;
        case 3:
            DEBUGCHK (0);
            return FALSE;
        case 4:
//            *(LPVOID *)lpfd = KmodeEntries();
//            return TRUE;
            DEBUGCHK (0);
            return FALSE;
        case 0xffffffff:
            for (count = 0; count < pROM->pTOC->nummods; count++) {
                tocptr = &(((TOCentry *)&pROM->pTOC[1])[count]);
                if (!StrCmpAPascW(tocptr->lpszFileName,lpfd->cFileName)) {
                    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",TRUE));
                    return TRUE;
                }
            }
            for (count = 0; count < pROM->pTOC->numfiles; count++) {
                tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
                filesptr = &(((FILESentry *)tocptr)[count]);
                if (!StrCmpAPascW(filesptr->lpszFileName,lpfd->cFileName)) {
                    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",TRUE));
                    return TRUE;
                }
            }
            DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",FALSE));
            return FALSE;
    }       
    DEBUGCHK(0);
    DEBUGMSG(ZONE_ENTRY,(L"NKGetRomFileInfo exit: %8.8lx\r\n",FALSE));
    return FALSE;
}

