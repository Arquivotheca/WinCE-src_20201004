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

#include "windows.h"
#include "coredll.h"
#include "pehdr.h"
#include "loader.h"
#include "loader_core.h"

#define DBGRSRC     DBGSHELL

typedef struct VERHEAD {
    WORD wTotLen;
    WORD wValLen;
    WORD wType;         /* always 0 */
    WCHAR szKey[(sizeof("VS_VERSION_INFO")+3)&~03];
    VS_FIXEDFILEINFO vsf;
} VERHEAD ;

typedef struct resroot_t {
    DWORD flags;
    DWORD timestamp;
    DWORD version;
    WORD  numnameents;
    WORD  numidents;
} resroot_t;

typedef struct resent_t {
    DWORD id;
    DWORD rva;
} resent_t;

typedef struct VERBLOCK {
    WORD wTotLen;
    WORD wValLen;
    WORD wType;
    WCHAR szKey[1];
} VERBLOCK ;

#pragma pack(2)
typedef struct {
    WORD idReserved;
    WORD idType;
    WORD idCount;
} ICONHEADER;

typedef struct ResourceDirectory {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD wOrdinal;     /* points to an RT_ICON resource */
} RESDIR;

typedef struct {
    ICONHEADER ih;
    RESDIR rgdir[1];  /* may really be > 1 */
} HEADER_AND_DIR, *PHEADER_AND_DIR;

#define DWORDUP(x) (((x)+3)&~03)

extern "C"
HRSRC
WINAPI
FindResourceForDisplayConfiguration_I(
    __in_z LPCWSTR pwszModuleBase,
    __out HMODULE *phModule,
    __in_z LPCWSTR pwszName,
    __in_z LPCWSTR pwszType,
    DWORD dwDisplayConfigurationID
);

#ifdef KCOREDLL

#define NKGetCallerResInfo  WIN32_CALL(PCInfo, GetCallerResInfo, (LPDWORD pdwBase, PUSERMODULELIST *ppModMui))

extern PUSERMODULELIST (* FindModule) (HMODULE hMod);

#define DoLoadLibrary(pszFileName, dwFlags)     ((PUSERMODULELIST) NKLoadLibraryEx (pszFileName, dwFlags, NULL))
#define DoFreeLibrary(pMod)                     NKFreeLibrary ((HMODULE) pMod)

// kernel DLL uses kernel's loader CS directly
#define LockLoader()
#define UnlockLoader()

#else  // KCOREDLL

extern PUSERMODULELIST g_pExeMUI;

extern DWORD g_dwExeBase;
extern struct info g_resExe;   // save the resource information of the exe

#define LockLoader()      LockProcCS()
#define UnlockLoader()    UnlockProcCS()

PUSERMODULELIST
DoLoadLibrary (
    LPCWSTR pszFileName,
    DWORD dwFlags
    );

BOOL
DoFreeLibrary (
    PUSERMODULELIST pMod
    );

PUSERMODULELIST FindModule (HMODULE hMod);

PUSERMODULELIST FindModByResource (HRSRC hRsrc);


#endif // KCOREDLL

static BOOL IsValidProcess (HANDLE hProc)
{

#ifdef KCOREDLL
    return !hProc || ((DWORD) hProc == GetCallerVMProcessId ());
#else
    return !hProc || ((DWORD) hProc == GetCurrentProcessId ()) || (hProc == hActiveProc);
#endif
}

static PCInfo GetResourcInformation (HMODULE hMod, PUSERMODULELIST *ppModMui, LPDWORD pdwModBase)
{
    PCInfo pResInfo = NULL;

    if (IsValidProcess (hMod)) {
        // process

#ifdef KCOREDLL
        pResInfo    = NKGetCallerResInfo (pdwModBase, ppModMui);
#else
        *ppModMui   = g_pExeMUI;
        *pdwModBase = g_dwExeBase;
        pResInfo    = &g_resExe;
#endif
    } else {
        PUSERMODULELIST pMod;

        LockLoader ();
        pMod = FindModule (hMod);
        UnlockLoader ();

        if (pMod) {
            *ppModMui   = pMod->pmodResource;
            *pdwModBase = pMod->dwBaseAddr;
            pResInfo    = GetResInfo (pMod);
        }
    }
    return pResInfo;
}

//------------------------------------------------------------------------------
// Compare a Unicode string and a Pascal string
//------------------------------------------------------------------------------
static int StrCmpPascW (LPCWSTR ustr, LPCWSTR pstr)
{
    int loop = (int)*pstr++;
    while (*ustr && loop--)
        if (towlower (*ustr++) != towlower (*pstr++))
            return 1;
    return (!*ustr && !loop ? 0 : 1);
}


//------------------------------------------------------------------------------
// Find first entry (from curr pointer)
//------------------------------------------------------------------------------
static DWORD FindFirst (DWORD curr)
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(  curr
                                    + sizeof(resroot_t)
                                    + rootptr->numnameents*sizeof(resent_t));
    return resptr->rva;
}


//------------------------------------------------------------------------------
// Search for a named type
//------------------------------------------------------------------------------
static DWORD FindTypeByName (DWORD vbase, DWORD curr, LPCWSTR str)
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t));
    int count = rootptr->numnameents;
    while (count--) {
        if (!str || !StrCmpPascW (str, (LPCWSTR)(vbase+(resptr->id&0x7fffffff)))) {
            return resptr->rva;
        }
        resptr++;
    }
    return 0;
}


//------------------------------------------------------------------------------
// Search for a numbered type
//------------------------------------------------------------------------------
static DWORD FindTypeByNum (DWORD curr, DWORD id)
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t) +
        rootptr->numnameents*sizeof(resent_t));
    int count = rootptr->numidents;
    while (count--) {
        if (!id || (resptr->id == id)) {
            return resptr->rva;
        }
        resptr++;
    }
    return 0;
}


//------------------------------------------------------------------------------
// Find type entry (by name or number)
//------------------------------------------------------------------------------
static DWORD FindType (DWORD vbase, DWORD curr, LPCWSTR str)
{
    DWORD res;
    if (((DWORD)str) >> 16) {
        res = (str[0] == (WCHAR)'#')
            ? FindTypeByNum  (curr, _wtol (str+1))
            : FindTypeByName (vbase, curr, str);

    } else if (0 == (res = FindTypeByNum (curr, (DWORD)str)) && !str) {
        res = FindTypeByName (vbase, curr, str);
    }
    return res;
}

//------------------------------------------------------------------------------
// DoFindResource - worker function to find a resource
//------------------------------------------------------------------------------
static HRSRC DoFindResource (
        PCInfo      pResInfo,
        LPCWSTR     lpszName,
        LPCWSTR     lpszType
        )
{
    HRSRC hrsrc = NULL;
    DWORD vbase;
    DEBUGMSG (DBGRSRC && pResInfo, (L"DoFindResource: resource base = %8.8lx\n", pResInfo->rva));


    if (pResInfo && (0 != (vbase = pResInfo->rva))) {
        DWORD trva;

        if (   (0 != (trva = FindType  (vbase, vbase, (LPCWSTR)lpszType)))
            && (0 != (trva = FindType  (vbase, vbase + (trva & 0x7fffffff), (LPCWSTR)lpszName)))
            && (0 != (trva = FindFirst (vbase + (trva & 0x7fffffff))))) {
            hrsrc = (HRSRC) (vbase + (trva & 0x7fffffff));
        }
    }
    DEBUGMSG (DBGRSRC, (L"DoFindResource: return %8.8lx\n", hrsrc));
    return hrsrc;
}


extern "C" HRSRC WINAPI FindResourceW (HMODULE hModule, LPCWSTR lpszName, LPCWSTR lpszType)
{
    PUSERMODULELIST pModMui;
    DWORD    dwModBase;
    HRSRC    hRsrc = NULL;
    PCInfo   pResInfo = GetResourcInformation (hModule, &pModMui, &dwModBase);

    __try {
        if (pResInfo
            && (NULL == (hRsrc = DoFindResource (GetResInfo (pModMui), lpszName, lpszType)))) {
            hRsrc = DoFindResource (pResInfo, lpszName, lpszType);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // empty
    }

    if (!hRsrc) {
        SetLastError (pResInfo? ERROR_RESOURCE_NAME_NOT_FOUND : ERROR_INVALID_HANDLE);
    }

    return hRsrc;
}

extern "C"
HRSRC
WINAPI
FindResourceForDisplayConfiguration(
    LPCWSTR pwszModuleBase,
    HMODULE *phModule,
    LPCWSTR pwszName,
    LPCWSTR pwszType,
    DWORD dwDisplayConfigurationID
)
{
    return FindResourceForDisplayConfiguration_I(pwszModuleBase, phModule, pwszName, pwszType, dwDisplayConfigurationID);
}

extern "C" HGLOBAL WINAPI LoadResource (HMODULE hModule, HRSRC hrsrc)
{
    HGLOBAL hRet = NULL;
    PCInfo  pResInfo = NULL;

    if (hrsrc) {
        PUSERMODULELIST pModMui;
        DWORD    dwModBase;

        pResInfo = GetResourcInformation (hModule, &pModMui, &dwModBase);

        if (pResInfo) {
            __try {
                // try base module
                if (IsResValid (pResInfo, hrsrc)) {
                    hRet = (HGLOBAL) (dwModBase + ((resdata_t *) hrsrc)->rva);

                // try MUI if not in base module
                } else if (IsResValid ((pResInfo = GetResInfo (pModMui)), hrsrc)) {
                    hRet = (HGLOBAL) (pModMui->dwBaseAddr + ((resdata_t *) hrsrc)->rva);
                }
#ifndef KCOREDLL
                // user mode - hModule is NULL, try find it in the module list.
                else if (!hModule && (NULL != (pModMui = FindModByResource (hrsrc)))) {
                    hRet = (HGLOBAL) (pModMui->dwBaseAddr + ((resdata_t *) hrsrc)->rva);
                    pResInfo = GetResInfo (pModMui);
                }
#endif
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // empty
            }
        }
    }

    if (hRet && !IsResValid (pResInfo, (HRSRC)hRet)) {
        // invalid resource addr
        hRet = NULL;
    }

    if (!hRet) {
        SetLastError (ERROR_INVALID_HANDLE);
    }

    return hRet;
}

extern "C" int WINAPI LoadStringW (HINSTANCE hInstance, UINT wID, LPWSTR lpBuffer, int nBufMax)
{
    int   nRet = 0;
    DWORD dwErr = ERROR_INVALID_PARAMETER;

    DEBUGMSG (DBGRSRC, (L"LoadStringW: 0x%8.8lx 0x%x 0x%8.8lx %d\n", hInstance, wID, lpBuffer, nBufMax));

    if (!nBufMax || lpBuffer) {

        PUSERMODULELIST pModMui;
        PCInfo          pResInfo;
        DWORD           dwModBase = 0;

        __try {

            pResInfo = GetResourcInformation (hInstance, &pModMui, &dwModBase);

            if (pResInfo) {
                // try MUI DLL 1st
                HRSRC hRsrc = DoFindResource (GetResInfo (pModMui), MAKEINTRESOURCE((wID >> 4) + 1), RT_STRING);

                if (hRsrc) {
                    // resource found in MUI
                    pResInfo  = GetResInfo (pModMui);
                    dwModBase = pModMui->dwBaseAddr;
                } else {
                    // try base DLL/EXE
                    hRsrc = DoFindResource (pResInfo, MAKEINTRESOURCE((wID >> 4) + 1), RT_STRING);
                }
                
                if (!hRsrc) {
                    // resource not found
                    dwErr = ERROR_RESOURCE_NAME_NOT_FOUND;
                    
                } else {
                    // resource found
                    LPCWSTR lpsz = (LPCWSTR) (dwModBase + ((resdata_t *) hRsrc)->rva);
                    int     cch;

                    // Move past the other strings in this segment.
                    wID &= 0x0F;
                    for (;;) {
                        cch = *lpsz++;
                        if (wID-- == 0)
                            break;
                        lpsz += cch;
                    }

                    if (lpBuffer) {
                        // buffer specified, copy string or string pointer base on nBufMax

                        if (nBufMax --) {
                            // none-zero buffer max, copy the string
                            if (cch > nBufMax) {
                                cch = nBufMax;
                            }
                            // Copy the string into the buffer
                            memcpy (lpBuffer, lpsz, cch * sizeof(WCHAR));

                            lpBuffer[cch] = 0;
                            
                        } else {
                            // NT special - zero buffer max, copy the string pointer
                            *(LPCWSTR *) lpBuffer = lpsz;
                        }

                        // return value always the length of the string
                        nRet = cch;
                        
                    } else {
                        // CE special, return string pointer unless it's zero-lengthed.
                        DEBUGCHK (!nBufMax);
                        C_ASSERT (sizeof (int) == sizeof (LPCWSTR));
                        if (cch) {
                            nRet = (int) lpsz;
                        }
                    }

                    dwErr = 0;

                }
            }

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // empty, dwErr already set
        }
    }
    if (dwErr) {
        SetLastError (dwErr);
    }
    DEBUGMSG (DBGRSRC, (L"LoadStringW Exits: %d\n", nRet));
    return nRet;
}

extern "C" DWORD WINAPI SizeofResource (HMODULE hModule, HRSRC hRsrc)
{
    DWORD dwRet = 0;
    __try {
        dwRet = ((resdata_t *)hRsrc)->size;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_INVALID_HANDLE);
    }
    return dwRet;
}

static LPBYTE DoExtractResrouce (PUSERMODULELIST pMod, LPCWSTR lpszName, LPCWSTR lpszType, LPDWORD pdwSize)
{
    HRSRC   hrsrc;
    LPBYTE  pRet    = NULL;

    __try {
        hrsrc = DoFindResource (GetResInfo (pMod->pmodResource), lpszName, lpszType);  // try MUI first
        DWORD   cbRsrc;

        if (hrsrc) {
            // in MUI
            DEBUGCHK (pMod->pmodResource);
            pMod = pMod->pmodResource;

        } else if (NULL == (hrsrc = DoFindResource (GetResInfo (pMod), lpszName, lpszType))) {
            // not in base either
            SetLastError (ERROR_RESOURCE_NAME_NOT_FOUND);
        }

        if (hrsrc && ((int) (cbRsrc = ((resdata_t *)hrsrc)->size) > 0)) {

            pRet = (LPBYTE) LocalAlloc (LMEM_FIXED, cbRsrc);
            if (!pRet) {
                SetLastError (ERROR_NOT_ENOUGH_MEMORY);
            } else {
                memcpy (pRet, (LPVOID) (pMod->dwBaseAddr + ((resdata_t *) hrsrc)->rva), cbRsrc);
                if (pdwSize) {
                    *pdwSize = cbRsrc;
                }
            }
        } else {
            SetLastError (ERROR_RESOURCE_NAME_NOT_FOUND);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (pRet) {
            LocalFree (pRet);
            pRet = NULL;
        }
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    return pRet;
}

extern "C" LPVOID ExtractResourceWithSize (LPCWSTR lpszFile, LPCWSTR lpszName, LPCWSTR lpszType, LPDWORD pdwSize)
{
    PUSERMODULELIST pMod;
    LPVOID  prsrc = NULL;

    LockLoader ();
    pMod = DoLoadLibrary (lpszFile, LOAD_LIBRARY_AS_DATAFILE);
    UnlockLoader ();

    if (pMod) {
        prsrc = DoExtractResrouce (pMod, lpszName, lpszType, pdwSize);

        LockLoader ();
        DoFreeLibrary (pMod);
        UnlockLoader ();
    }
    if (!prsrc) {
        SetLastError (ERROR_RESOURCE_NAME_NOT_FOUND);
    }
    return prsrc;
}

extern "C" LPVOID ExtractResource (LPCWSTR lpszFile, LPCWSTR lpszName, LPCWSTR lpszType)
{
    return ExtractResourceWithSize (lpszFile, lpszName, lpszType, NULL);
}


extern "C" BOOL WINAPI VerQueryValueW (const LPVOID pvblk, LPWSTR lpSubBlock, LPVOID * lplpBuffer, PUINT puLen)
{
    VERBLOCK *pBlock = (VERBLOCK *) pvblk;
    LPWSTR lpSubBlockOrg, lpEndBlock, lpEndSubBlock, lpStart;
    WCHAR cEndBlock, cTemp;
    BOOL bString;
    DWORD dwTotBlockLen, dwHeadLen, dwNameLen;
    int nCmp;

    *puLen = 0;
    if ((int)pBlock->wTotLen < sizeof(VERBLOCK)) {
        SetLastError (ERROR_INVALID_DATA);
        return FALSE;
    }
    dwNameLen = wcslen(lpSubBlock);
    if (NULL == (lpSubBlockOrg = (LPWSTR)LocalAlloc(LMEM_FIXED,(dwNameLen+1)*sizeof(WCHAR)))) {
        // last error already set by LocalAlloc
        return FALSE;
    }
    memcpy(lpSubBlockOrg,lpSubBlock, dwNameLen*sizeof(WCHAR));
    lpSubBlockOrg[dwNameLen] = 0;   // EOS
    lpSubBlock = lpSubBlockOrg;
    lpEndBlock = (LPWSTR)((LPBYTE)pBlock + pBlock->wTotLen - sizeof(WCHAR));
    cEndBlock = *lpEndBlock;
    *lpEndBlock = 0;
    bString = FALSE;
    while (*lpSubBlock) {
        while (*lpSubBlock == L'\\')
            ++lpSubBlock;
        if (*lpSubBlock) {
            dwTotBlockLen = (DWORD)((LPSTR)lpEndBlock - (LPSTR)pBlock + sizeof(WCHAR));
            if (((int)dwTotBlockLen < sizeof(VERBLOCK)) || (pBlock->wTotLen > (WORD)dwTotBlockLen))
                goto NotFound;
            dwHeadLen = DWORDUP(sizeof(VERBLOCK) - sizeof(WCHAR) + (wcslen(pBlock->szKey) + 1) * sizeof(WCHAR)) + DWORDUP(pBlock->wValLen);
            if (dwHeadLen > pBlock->wTotLen)
                goto NotFound;
            lpEndSubBlock = (LPWSTR)((LPBYTE)pBlock + pBlock->wTotLen);
            pBlock = (VERBLOCK*)((LPBYTE)pBlock+dwHeadLen);
            for (lpStart=lpSubBlock; *lpSubBlock && (*lpSubBlock!=L'\\'); lpSubBlock++)
                ;
            cTemp = *lpSubBlock;
            *lpSubBlock = 0;
            nCmp = 1;
            while (((int)pBlock->wTotLen > sizeof(VERBLOCK)) && ((int)pBlock->wTotLen <= (LPBYTE)lpEndSubBlock-(LPBYTE)pBlock)) {
                if (0 == (nCmp=wcsicmp(lpStart, pBlock->szKey)))
                    break;
                pBlock=(VERBLOCK*)((LPBYTE)pBlock+DWORDUP(pBlock->wTotLen));
            }
            *lpSubBlock = cTemp;
            if (nCmp)
                goto NotFound;
        }
    }
    *puLen = pBlock->wValLen;
    lpStart = (LPWSTR)((LPBYTE)pBlock+DWORDUP((sizeof(VERBLOCK)-sizeof(WCHAR))+(wcslen(pBlock->szKey)+1)*sizeof(WCHAR)));
    *lplpBuffer = lpStart < (LPWSTR)((LPBYTE)pBlock+pBlock->wTotLen) ? lpStart : (LPWSTR)(pBlock->szKey+wcslen(pBlock->szKey));
    bString = pBlock->wType;
    *lpEndBlock = cEndBlock;
    LocalFree(lpSubBlockOrg);
    return (TRUE);
NotFound:
    SetLastError (ERROR_RESOURCE_NAME_NOT_FOUND);
    *lpEndBlock = cEndBlock;
    LocalFree(lpSubBlockOrg);
    return (FALSE);
}

extern "C" DWORD WINAPI GetFileVersionInfoSizeW (LPWSTR lpFilename, LPDWORD lpdwHandle)
{
    DWORD   dwRet = 0;
    VERHEAD *pVerHead;

    DEBUGMSG(DBGRSRC, (L"GetFileVersionInfoSizeW entry: %8.8lx %8.8lx\r\n",lpFilename, lpdwHandle));

    if (lpdwHandle) {
        *lpdwHandle = 0;
    }

    pVerHead = (VERHEAD*) ExtractResourceWithSize (lpFilename, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, &dwRet);
    if (pVerHead) {
        if (((DWORD)pVerHead->wTotLen > dwRet) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE))
            SetLastError (ERROR_INVALID_DATA);
        else
            dwRet = pVerHead->wTotLen;
        LocalFree (pVerHead);
    }

    DEBUGMSG(DBGRSRC, (L"GetFileVersionInfoSizeW exit: %8.8lx\r\n",dwRet));
    return dwRet;
}

extern "C" BOOL WINAPI GetFileVersionInfoW (LPWSTR lpFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    BOOL bRet = FALSE;
    DWORD dwLength = 0;
    VERHEAD *pVerHead;

    DEBUGMSG(DBGRSRC,(L"GetFileVersionInfoW entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",lpFilename, dwHandle, dwLen, lpData));

    pVerHead = (VERHEAD *)ExtractResourceWithSize (lpFilename, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, &dwLength);
    if (pVerHead) {
        if (((DWORD)pVerHead->wTotLen > dwLength) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE)) {
            SetLastError (ERROR_INVALID_DATA);

        } else {

            if (dwLen > pVerHead->wTotLen) {
                dwLen = pVerHead->wTotLen;
            }
            memcpy (lpData, pVerHead, dwLen);
            ((VERHEAD*)lpData)->wTotLen = (WORD) dwLen;
            bRet = TRUE;

        }
        LocalFree(pVerHead);
    } else {
        SetLastError (ERROR_INVALID_DATA);
    }
    DEBUGMSG(DBGRSRC,(L"GetFileVersionInfoW exit: %d\r\n",bRet));
    return bRet;;
}

typedef void (* KEICB_t)(LPVOID, PHEADER_AND_DIR,PWORD);

extern "C" UINT KernExtractIconsWithSize (
    LPCWSTR lpszFile,
    int nIconIndex,
    LPBYTE * pIconLarge,
    LPDWORD pcbIconLarge,
    LPBYTE * pIconSmall,
    LPDWORD pcbIconSmall,
    CALLBACKINFO * pcbi)
{
    PUSERMODULELIST pMod;
    PUSERMODULELIST pModRes;
    UINT    nRet = 0;

    LockLoader ();
    pMod = DoLoadLibrary (lpszFile, LOAD_LIBRARY_AS_DATAFILE);
    UnlockLoader ();

    pModRes = pMod;
    if (pModRes) {
        PHEADER_AND_DIR phd;
        HRSRC hrsrc = DoFindResource (GetResInfo (pMod->pmodResource), MAKEINTRESOURCE(nIconIndex), RT_GROUP_ICON);

        if (hrsrc) {
            DEBUGCHK (pMod->pmodResource);
            pModRes = pMod->pmodResource;
        } else {
            hrsrc = DoFindResource (GetResInfo (pMod), MAKEINTRESOURCE(nIconIndex), RT_GROUP_ICON);
        }

        if (hrsrc && (NULL != (phd = (PHEADER_AND_DIR) (pModRes->dwBaseAddr + ((resdata_t *) hrsrc)->rva)))) {
            if (phd->ih.idCount) {
                WORD  index[2] = {0, 1};
                ((KEICB_t)pcbi->pfn)(pcbi->pvArg0, phd, index);
                if (pIconLarge && (NULL != (*pIconLarge = DoExtractResrouce (pModRes, MAKEINTRESOURCE(phd->rgdir[index[0]].wOrdinal), RT_ICON, pcbIconLarge))))
                    nRet++;
                if (pIconSmall && (NULL != (*pIconSmall = DoExtractResrouce (pModRes, MAKEINTRESOURCE(phd->rgdir[index[1]].wOrdinal), RT_ICON, pcbIconSmall))))
                    nRet++;
            }
        }
        LockLoader ();
        DoFreeLibrary (pMod);
        UnlockLoader ();
    }
    if (!nRet) {
        SetLastError (ERROR_RESOURCE_NAME_NOT_FOUND);
    }
    return nRet;
}

extern "C" UINT KernExtractIcons (LPCWSTR lpszFile, int nIconIndex, LPBYTE * pIconLarge, LPBYTE * pIconSmall, CALLBACKINFO * pcbi)
{
    return KernExtractIconsWithSize(lpszFile, nIconIndex, pIconLarge, NULL, pIconSmall, NULL, pcbi);
}


