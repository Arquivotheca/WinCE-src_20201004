//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#undef LocalAlloc


#define DWORDUP(x) (((x)+3)&~03)

extern CRITICAL_SECTION LLcs, RFBcs, ModListcs;
extern HANDLE hCoreDll;

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

typedef struct resdata_t {
    DWORD rva;
    DWORD size;
    DWORD codepage;
    DWORD unused;
} resdata_t;

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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
atoiW(
    LPWSTR str
    ) 
{
    DWORD retval = 0;
    while (*str) {
        retval = retval * 10 + (*str-(WCHAR)'0');
        str++;
    }
    return retval;
}

extern WCHAR lowerW(WCHAR);
HANDLE FindResourcePart2(PMODULE pMod, LPBYTE BasePtr, e32_lite* eptr, LPCWSTR lpszName, LPCWSTR lpszType);



//------------------------------------------------------------------------------
// Compare a Unicode string and a Pascal string
//------------------------------------------------------------------------------
int 
StrCmpPascW(
    LPWSTR ustr,
    LPWSTR pstr
    ) 
{
    int loop = (int)*pstr++;
    while (*ustr && loop--)
        if (lowerW(*ustr++) != lowerW(*pstr++))
            return 1;
    return (!*ustr && !loop ? 0 : 1);
}



//------------------------------------------------------------------------------
// Compare an Ascii string and a Pascal string
//------------------------------------------------------------------------------
int 
StrCmpAPascW(
    LPSTR astr,
    LPWSTR pstr
    ) 
{
    int loop = (int)*pstr++;
    while (*astr && loop--)
        if (lowerW((WCHAR)*astr++) != lowerW(*pstr++))
            return 1;
    return (!*astr && !loop ? 0 : 1);
}




//------------------------------------------------------------------------------
// Search for a named type
//------------------------------------------------------------------------------
DWORD 
FindTypeByName(
    LPBYTE BasePtr,
    LPBYTE curr,
    LPWSTR str
    ) 
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t));
    int count = rootptr->numnameents;
    while (count--) {
        if (!str || !StrCmpPascW(str,(LPWSTR)(BasePtr+(resptr->id&0x7fffffff)))) {
            DEBUGCHK (!IsSecureVa(resptr->rva));
            return resptr->rva;
        }
        resptr++;
    }
    return 0;
}




//------------------------------------------------------------------------------
// Search for a numbered type
//------------------------------------------------------------------------------
DWORD 
FindTypeByNum(
    LPBYTE curr,
    DWORD id
    ) 
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t) +
        rootptr->numnameents*sizeof(resent_t));
    int count = rootptr->numidents;
    while (count--) {
        if (!id || (resptr->id == id)) {
            DEBUGCHK (!IsSecureVa(resptr->rva));
            return resptr->rva;
        }
        resptr++;
    }
    return 0;
}




//------------------------------------------------------------------------------
// Find first entry (from curr pointer)
//------------------------------------------------------------------------------
DWORD 
FindFirst(
    LPBYTE curr
    ) 
{
    resroot_t *rootptr = (resroot_t *)curr;
    resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t) +
        rootptr->numnameents*sizeof(resent_t));
    return resptr->rva;
}




//------------------------------------------------------------------------------
// Find type entry (by name or number)
//------------------------------------------------------------------------------
DWORD 
FindType(
    LPBYTE BasePtr,
    LPBYTE curr,
    LPWSTR str
    ) 
{
    DWORD res;
    if (((DWORD)str)>>16)
        if (str[0] == (WCHAR)'#')
            return FindTypeByNum(curr, atoiW(str+1));
        else
            return FindTypeByName(BasePtr, curr, str);
    if (!(res = FindTypeByNum(curr, (DWORD)str)) && !str)
        res = FindTypeByName(BasePtr, curr, str);
    return res;
}

// global array that keeps our current UI language, and default UI language
LANGID g_rgwUILangs[6]; // at most 5 langs, one extra for array terminator
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
        if( ERROR_SUCCESS!=RegQueryValueExW(hkey, szValName, (LPDWORD)L"MUI", &dwType, (LPBYTE)&dwValue, &dwSize) ||
            (dwType != REG_DWORD) || !dwValue ) {
                DEBUGMSG(1,(L"InitMUI: Failed to read regkey %s\r\n", szValName));
                return;
        }
    }               
    langid1 = (LANGID)dwValue;
    langid2 = MAKELANGID(PRIMARYLANGID(langid1), SUBLANG_DEFAULT);
    if(langid2 == langid1)
        langid2 = 0;
    DEBUGMSG(1,(L"InitMUI: Got Langs=%x %x\r\n", langid1, langid2));
    
    // add them uniquely to array
    for(i=0; ;i++) {
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

    // if MUI disabled (or not yet inited, e.g. call by filesys.exe), or the module doesn't have
    // resources, then fail immediately
    if ((g_EnableMUI != 1) || !eptr->e32_unit[RES].rva) {
        return NULL;
    }
    // we must be holding LLcs already
    DEBUGCHK (LLcs.OwnerThread == hCurThread);

    pMod = (!((DWORD) hModule & 3) && IsValidModule((PMODULE)hModule))? (PMODULE)hModule : 0;
    
    // See if module contains a reference to a MUI (resource type=222, id=1). If found,
    // the resource contains a basename to which we append .XXXX.MUI, where XXXX is the language code
    if ((hRsrc = FindResourcePart2(pMod, BasePtr, eptr, MAKEINTRESOURCE(ID_MUI), MAKEINTRESOURCE(RT_MUI)))
        && (iLen = ((resdata_t *)hRsrc)->size)
        && (iLen < sizeof(WCHAR)*(MAX_PATH-10)) ) {
        memcpy(szPath, (BasePtr + ((resdata_t *)hRsrc)->rva), iLen);
        iLen /= sizeof(WCHAR);
        szPath[iLen]=0;
        DEBUGMSG(1,(L"LoadMUI: Found indirection (%s, %d)\r\n", szPath, iLen)); 
    } 
    // otherwise search for local MUI dll, based on module path name
    else  if (!(iLen = GetModuleFileName(hModule, szPath, MAX_PATH-10))) {
        DEBUGCHK(FALSE);
        return NULL;
    }

    for(i=0; g_rgwUILangs[i]; i++)
    {
        // try to find local DLL in each of current/default/english languages
        dwLangID = g_rgwUILangs[i];
        NKwvsprintfW(szPath+iLen, TEXT(".%04X.MUI"), (LPVOID)&dwLangID, (MAX_PATH-iLen));
        DEBUGMSG (1,(L"LoadMUI: Trying %s\r\n",szPath));
        if (pMod = (PMODULE) LoadOneLibraryW (szPath, LLIB_NO_MUI, LOAD_LIBRARY_AS_DATAFILE)) { // dont call dllentry/resolve refs etc
            DEBUGMSG (1,(L"LoadMUI: Loaded %s\r\n", szPath)); 
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
    PMODULE pMod = (PMODULE) hCoreDll;
    
    DEBUGCHK(!g_EnableMUI); // this must be called only once
    if(!SystemAPISets[SH_FILESYS_APIS]) {
        g_EnableMUI = (DWORD)-1;    // if this config has no filesystem then MUI is disabled
        return;
    }
    
    // check if MUI is enabled
    dwSize = sizeof(DWORD);
    if( ERROR_SUCCESS!=RegQueryValueExW(HKEY_LOCAL_MACHINE, L"Enable",(LPDWORD)L"MUI", &dwType, (LPBYTE)&dwValue, &dwSize) ||
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

    // at this point, the only processes running should be NK and filesys.
    // We need to synchronize the ref-count of coredll and it's MUI
    EnterCriticalSection (&LLcs);
    DEBUGCHK (!(pMod->inuse & ~3));
    if (pMod->pmodResource = LoadMUI (pMod, pMod->BasePtr, &pMod->e32)) {
        pMod->pmodResource->refcnt[0] = pMod->refcnt[0];
        pMod->pmodResource->refcnt[1] = pMod->refcnt[1];
        pMod->pmodResource->inuse = pMod->inuse;
    }
    LeaveCriticalSection (&LLcs);
}





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
FindResourcePart2(
    PMODULE pMod,
    LPBYTE BasePtr,
    e32_lite* eptr,
    LPCWSTR lpszName,
    LPCWSTR lpszType
    ) 
{
    DWORD trva;
    if (!eptr->e32_unit[RES].rva) return 0;
    if (pMod && (FT_ROMIMAGE == pMod->oe.filetype)) {
        int loop;
        o32_lite *optr = pMod->o32_ptr;
        for (loop = 0; loop < eptr->e32_objcnt; loop++, optr ++) {
            if (optr->o32_rva == eptr->e32_unit[RES].rva)
                break;
        }
        if (loop == eptr->e32_objcnt) {
            DEBUGMSG (ZONE_LOADER2, (L"FindRsc2 Failed: module '%s', eptr->e32_unit[RES].rva = %8.8lx\n",
                pMod->lpszModName, eptr->e32_unit[RES].rva));
            return 0;
        }
        BasePtr = (LPBYTE) optr->o32_realaddr;
    } else {
        BasePtr += eptr->e32_unit[RES].rva;
    }
    DEBUGMSG (ZONE_LOADER2, (L"FindRsc2: BasePtr = %8.8lx\n", BasePtr));
    if (!(trva = FindType(BasePtr, BasePtr, (LPWSTR)lpszType)) || 
        !(trva = FindType(BasePtr, BasePtr+(trva&0x7fffffff),(LPWSTR)lpszName)) ||
        !(trva = FindFirst(BasePtr+(trva&0x7fffffff)))) {
            return 0;
    }
    DEBUGMSG (ZONE_LOADER2, (L"FindRsc2 return %8.8lx\n", (BasePtr+(trva&0x7fffffff))));
    return (HANDLE)(BasePtr+(trva&0x7fffffff));
}


//------------------------------------------------------------------------------
// Win32 FindResource
//------------------------------------------------------------------------------
HANDLE 
SC_FindResource(
    HANDLE hModule,
    LPCWSTR lpszName,
    LPCWSTR lpszType
    ) 
{
    LPBYTE BasePtr;
    e32_lite *eptr;
    PPROCESS pProc;
    PMODULE pmodRes, pMod = 0;
    HANDLE  hRet;
    
    DEBUGMSG(ZONE_ENTRY,(L"SC_FindResource entry: %8.8lx %8.8lx %8.8lx\r\n",hModule,lpszName,lpszType));
    if (hModule == GetCurrentProcess())
        hModule = hCurProc;
    // Get Base & e32 ptr for current process or module as approp
    // also get MUI resource dll, loading it if neccesary
    if (pProc = HandleToProc(hModule)) { // a process
        BasePtr = (LPBYTE)MapPtrProc(pProc->BasePtr,pProc);
        eptr = &pProc->e32;
        pmodRes = pProc->pmodResource;

    } else if (IsValidModule(pMod = (LPMODULE)hModule)) { // a module
        BasePtr = (LPBYTE)MapPtrProc(pMod->BasePtr,&ProcArray[0]);
        eptr = &pMod->e32;
        pmodRes = pMod->pmodResource;

    } else {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_ENTRY,(L"SC_FindResource exit: %8.8lx\r\n",0));
        return 0;
    }
    // If we have a MUI dll try to load from it first
    hRet = 0;
    if(pmodRes) {
        //DEBUGMSG(1,(L"SC_FindResource: Trying MUI %08x\r\n",pmodRes));
        DEBUGCHK(IsValidModule(pmodRes)); // at this point pmodRes is a valid PMODULE
        hRet = FindResourcePart2 (pmodRes, (LPBYTE) pmodRes->BasePtr, &(pmodRes->e32), lpszName, lpszType);
    }
    // if no MUI or failed to find resource, try the module itself
    if(!hRet) {
        //DEBUGMSG(1,(L"SC_FindResource: Trying self\r\n"));
        hRet = FindResourcePart2(pMod, BasePtr, eptr, lpszName, lpszType);
    }
    if(!hRet) 
        KSetLastError(pCurThread,ERROR_RESOURCE_NAME_NOT_FOUND);
    DEBUGMSG(ZONE_ENTRY,(L"SC_FindResource exit: %8.8lx\r\n",hRet));
    return hRet;
}




//------------------------------------------------------------------------------
// Win32 SizeofResource
//------------------------------------------------------------------------------
DWORD 
SC_SizeofResource(
    HANDLE hModule,
    HANDLE hRsrc
    ) 
{
    DWORD dwSize = 0;
    DEBUGMSG(ZONE_ENTRY,(L"SC_SizeofResource entry: %8.8lx %8.8lx\r\n",hModule,hRsrc));
    if (hRsrc) {
        __try {
            dwSize = ((resdata_t *)hRsrc)->size;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (!dwSize) {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_SizeofResource exit: %8.8lx\r\n", dwSize));
    return dwSize;
}


static HANDLE DoLoadResource(HANDLE hModule, HANDLE hRsrc) 
{
    DWORD pnum;
    PMODULE pModRes = 0, pMod = (PMODULE) hModule;
    LPBYTE BasePtr;
    PPROCESS pProc = 0;
    HANDLE h = 0;

    if (hModule == GetCurrentProcess())
        hModule = hCurProc;
    if (!hRsrc) {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        return 0;
    }

    if (IsModCodeAddr (hRsrc) || IsInResouceSection (hRsrc)) {
        // this is a resource from a module. need to handle it differently because the
        // section might (and almost always) be moved.
        o32_lite *optr;
        int     loop;
        
        if (!(pMod = (PMODULE) hModule)) {
            // find the module if hModule is NULL
            EnterCriticalSection(&ModListcs);
            pMod = pModList;
            for (pMod = pModList; pMod; pMod = pMod->pMod)
                if (((DWORD) hRsrc >= (DWORD) pMod->BasePtr)
                    && ((DWORD) hRsrc < (DWORD) pMod->BasePtr+pMod->e32.e32_vsize))
                    break;
            LeaveCriticalSection(&ModListcs);
        } else if (pProc = HandleToProc (hModule)) {
            pMod = pProc->pmodResource;
        } else if (IsValidModule (pMod)) {

            if ((pModRes = pMod->pmodResource)
                && (hRsrc >= pModRes->BasePtr)
                && ((LPBYTE) hRsrc < (LPBYTE) pModRes->BasePtr + pModRes->e32.e32_vsize)) {
                
                pMod = pModRes;
            }
        }

        if (!IsValidModule (pMod)) {
            KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
            return 0;
        }
        
        optr = pMod->o32_ptr;
        for (loop = 0; loop < pMod->e32.e32_objcnt; loop ++, optr ++) {
            if (((DWORD) hRsrc >= optr->o32_realaddr)
                && ((DWORD) hRsrc < optr->o32_realaddr + optr->o32_vsize)) {
                // this is the section we're in
                h = (HANDLE) (((resdata_t *)hRsrc)->rva - optr->o32_rva + optr->o32_realaddr);
                break;
            }
        }
        return h;
        
    }
    
    if (!hModule) {
        pnum = IsSecureVa (hRsrc)? 1 : ((DWORD)hRsrc >> VA_SECTION);
        DEBUGCHK (pnum <= MAX_PROCESSES);
        if (pnum-- && ProcArray[pnum].dwVMBase) {
            if (!pnum) {
                // slot 1 address, find the real module
                EnterCriticalSection(&ModListcs);
                pMod = pModList;
                pnum = ZeroPtr(hRsrc);
                for (pMod = pModList; pMod; pMod = pMod->pMod)
                    if ((pnum >= ZeroPtr(pMod->BasePtr)) && (pnum < ZeroPtr(pMod->BasePtr)+pMod->e32.e32_vsize))
                        break;
                LeaveCriticalSection(&ModListcs);
                if (!pMod) {
                    KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
                    return 0;
                }
                BasePtr = MapPtrProc(pMod->BasePtr,&ProcArray[0]);
            } else {
                pProc = &ProcArray[pnum];
                BasePtr = MapPtrProc(pProc->BasePtr, pProc);
            }
        } else {
            KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
            return 0;
        }
    } else if (pProc = HandleToProc(hModule)) { // a process
        pModRes = pProc->pmodResource;
        BasePtr = (LPBYTE)MapPtrProc(pProc->BasePtr, pProc);
    } else if (IsValidModule(pMod)) { // a module
        pModRes = pMod->pmodResource;
        BasePtr = (LPBYTE)MapPtrProc(pMod->BasePtr,&ProcArray[0]);
    } else {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        return 0;
    }
    // check if it came out of the MUI resource dll
    if (pModRes
        && (ZeroPtr(hRsrc) >= ZeroPtr(pModRes->BasePtr))
        && (ZeroPtr(hRsrc) < ZeroPtr(pModRes->BasePtr)+pModRes->e32.e32_vsize)) {
        
        BasePtr = (LPBYTE) pModRes->BasePtr;
    }

    h = (HANDLE) (BasePtr + ((resdata_t *)hRsrc)->rva);
    if (ZeroPtr (h) >= (DWORD) DllLoadBase)
        h = (HANDLE) ZeroPtr (h);

    return h;
}

//------------------------------------------------------------------------------
// Win32 LoadResource
//------------------------------------------------------------------------------
HANDLE 
SC_LoadResource(
    HANDLE hModule,
    HANDLE hRsrc
    ) 
{
    HANDLE h;
    DEBUGMSG(ZONE_ENTRY,(L"SC_LoadResource entry: %8.8lx %8.8lx\r\n",hModule,hRsrc));
    __try {
        h = DoLoadResource (hModule, hRsrc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        h = NULL;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_LoadResource exit: %8.8lx\r\n", h));
    return h;
}

DWORD Decompress(LPBYTE BufIn, DWORD InSize, LPBYTE BufOut, DWORD OutSize, DWORD skip);
DWORD DecompressROM(LPBYTE BufIn, DWORD InSize, LPBYTE BufOut, DWORD OutSize, DWORD skip);

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

BOOL ReadExtImageInfo (HANDLE hf, DWORD dwCode, DWORD cbSize, LPVOID pBuf);

static LPBYTE FindResourceSection (openexe_t *oeptr, DWORD *o32rva)
{
    e32_lite e32l;
    int      loop;
    LPBYTE   lpres = NULL;
    DWORD    cbRead;
    
    
    if (LoadE32 (oeptr, &e32l, NULL, NULL, TRUE, TRUE, NULL)    // loadE32 failed
        || !e32l.e32_unit[RES].rva) {                           // no resource section
         return NULL;
    }

    switch (oeptr->filetype) {
    case FT_ROMIMAGE: {
        o32_rom  *o32rp;
        for (loop = 0; loop < e32l.e32_objcnt; loop++) {
            o32rp = (o32_rom *)(oeptr->tocptr->ulO32Offset+loop*sizeof(o32_rom));
            if ((o32rp->o32_rva == e32l.e32_unit[RES].rva) && (o32rp->o32_psize)) {

                // if it's uncompressed, just directly pointing to ROM
                if (!(o32rp->o32_flags & IMAGE_SCN_COMPRESSED)) {
                    *o32rva = o32rp->o32_rva;
                    return  (LPBYTE) (o32rp->o32_dataptr);
                }
                    
                // compressed. allocate memory to hold the resource
                if (!(lpres = (LPBYTE)LocalAlloc (LMEM_FIXED, o32rp->o32_vsize))) {
                    return NULL;
                }
                cbRead = DecompressROM ((LPVOID)(o32rp->o32_dataptr), o32rp->o32_psize, lpres, o32rp->o32_vsize, 0);
                if (cbRead && (cbRead != CEDECOMPRESS_FAILED)) {
                    // memset the rest of the section to 0 if read less than vsize
                    if (cbRead < o32rp->o32_vsize)
                        memset (lpres+cbRead, 0, o32rp->o32_vsize-cbRead);
                    *o32rva = o32rp->o32_rva;
                    return lpres;
                }                
                break;  // loop
            }
        }
        break;  // switch
    }
    case FT_OBJSTORE: {
        o32_obj  o32;
        for (loop = 0; loop < e32l.e32_objcnt; loop++) {
            SetFilePointer (oeptr->hf, oeptr->offset+sizeof(e32_exe)+loop*sizeof(o32_obj), 0, FILE_BEGIN);
            if (ReadFile (oeptr->hf, (LPBYTE)&o32, sizeof(o32_obj), &cbRead, 0)
                && (o32.o32_rva == e32l.e32_unit[RES].rva)) {
                
                if (!(lpres = (LPBYTE)LocalAlloc(LMEM_FIXED, o32.o32_vsize))) {
                    return NULL;
                }

                SetFilePointer (oeptr->hf, o32.o32_dataptr, 0, FILE_BEGIN);
                if (ReadFile (oeptr->hf, lpres, min(o32.o32_psize,o32.o32_vsize), &cbRead, 0)) {
                    *o32rva = o32.o32_rva;
                    return lpres;
                }
                break;  // loop
            }
        }
        break;  // switch
    }
    case FT_EXTIMAGE: {
        o32_lite *optr = (o32_lite *) _alloca (e32l.e32_objcnt * sizeof(o32_lite));
        if (!optr
            || !ReadExtImageInfo (oeptr->hf, IOCTL_BIN_GET_O32, e32l.e32_objcnt * sizeof(o32_lite), optr)) {
            // failed to get O32 info, return FALSE
            return NULL;
        }
        for (loop = 0; loop < e32l.e32_objcnt; loop++, optr ++) {
            if (optr->o32_rva == e32l.e32_unit[RES].rva) {
                if (!(lpres = (LPBYTE)LocalAlloc(LMEM_FIXED, optr->o32_vsize))) {
                    return NULL;
                }

                cbRead = optr->o32_vsize;
                if (!(optr->o32_flags & IMAGE_SCN_COMPRESSED) && (cbRead > optr->o32_psize))
                    cbRead = optr->o32_psize;

                // Read the section, always use vsize
                if (ReadFileWithSeek (oeptr->hf, lpres, cbRead, &cbRead, 0, optr->o32_dataptr, 0)) {
                    // memset the rest of the section to 0 if read less than vsize
                    if (cbRead < optr->o32_vsize)
                        memset (lpres+cbRead, 0, optr->o32_vsize-cbRead);
                    *o32rva = optr->o32_rva;
                    return lpres;
                }
                break;  // loop
            }
        }
        break;  // switch
    }
    default:
        DEBUGCHK (0);
    }

    // error case if reach here
    if (lpres)
        LocalFree (lpres);
    
    return NULL;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE 
OpenResourceFile(
    LPCWSTR lpszFile,
    BOOL *pInRom,
    DWORD *o32rva
    ) 
{
    openexe_t oe;
    LPBYTE lpres = NULL;
    BOOL   fResult = FALSE;
    
    __try {
        CALLBACKINFO cbi;
        OEinfo_t OEinfo;
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)SafeOpenExe;
        cbi.pvArg0 = MapPtr(lpszFile);
        if (fResult = PerformCallBack(&cbi, MapPtr(&oe), 0, 0, &OEinfo)) {
            lpres = FindResourceSection (&oe, o32rva);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }

    if (fResult)
        CloseExe (&oe);
    
    if (!lpres)
        KSetLastError (pCurThread, ERROR_RESOURCE_NAME_NOT_FOUND);
    else
        *pInRom = IsKernelVa (lpres);

    DEBUGMSG(ZONE_ENTRY,(L"SC_ExtractResource exit: lpres = %8.8lx\r\n", lpres));
    return lpres;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
ExtractOneResource(
    LPBYTE lpres,
    LPCWSTR lpszName,
    LPCWSTR lpszType,
    DWORD o32rva,
    LPDWORD pdwSize
    ) 
{
    LPBYTE lpres2;
    DWORD trva;
    if (!(trva = FindType(lpres, lpres, (LPWSTR)lpszType)) ||
       !(trva = FindType(lpres, lpres+(trva&0x7fffffff),(LPWSTR)lpszName)) ||
       !(trva = FindFirst(lpres+(trva&0x7fffffff))) ||
       (trva & 0x80000000)) {
        KSetLastError(pCurThread,ERROR_RESOURCE_NAME_NOT_FOUND);
        return 0;
    }
    trva = (ULONG)(lpres + trva);
    if (!(lpres2 = (LPBYTE)LocalAlloc(LMEM_FIXED,((resdata_t *)trva)->size))) {
        KSetLastError(pCurThread,ERROR_RESOURCE_NAME_NOT_FOUND);
        return 0;
    }
    

    if (pdwSize)
        *pdwSize = ((resdata_t *)trva)->size;
    DEBUGMSG (ZONE_LOADER2, (L"ExtractOneResource: memcpy (%8.8lx, %8.8lx, %8.8lx)\n",
        lpres2,lpres+((resdata_t *)trva)->rva-o32rva,((resdata_t *)trva)->size));
    memcpy(lpres2,lpres+((resdata_t *)trva)->rva-o32rva,((resdata_t *)trva)->size);
    return lpres2;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
SC_ExtractResource(
    LPCWSTR lpszFile,
    LPCWSTR lpszName,
    LPCWSTR lpszType
    ) 
{
    LPBYTE lpres, lpres2;
    BOOL inRom;
    DWORD o32rva;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ExtractResource entry: %8.8lx %8.8lx %8.8lx\r\n",lpszFile,lpszName,lpszType));
    if (!(lpres = OpenResourceFile(lpszFile, &inRom, &o32rva))) {
        DEBUGMSG(ZONE_ENTRY,(L"SC_ExtractResource exit: %8.8lx\r\n",0));
        return 0;
    }
    lpres2 = ExtractOneResource(lpres,lpszName,lpszType,o32rva,0);
    if (!inRom)
        LocalFree(lpres);
    DEBUGMSG(ZONE_ENTRY,(L"SC_ExtractResource exit: %8.8lx\r\n",lpres2));
    return lpres2;
}

typedef void (* KEICB_t)(LPVOID, PHEADER_AND_DIR,PWORD);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT 
SC_KernExtractIcons(
    LPCWSTR lpszFile,
    int nIconIndex,
    LPBYTE *pIconLarge,
    LPBYTE *pIconSmall,
    CALLBACKINFO *pcbi
    ) 
{
    LPBYTE lpres;
    BOOL inRom;
    UINT nRet = 0;
    PHEADER_AND_DIR phd;
    DWORD o32rva;
    WORD index[2];

    TRUSTED_API (L"SC_KernExtractIcons", 0);
    if (!(lpres = OpenResourceFile(lpszFile, &inRom, &o32rva)))
        return 0;
    if (phd = (PHEADER_AND_DIR)ExtractOneResource(lpres, MAKEINTRESOURCE(nIconIndex), RT_GROUP_ICON, o32rva,0)) {
        if (phd->ih.idCount) {
            ((KEICB_t)pcbi->pfn)(pcbi->pvArg0,phd,index);
            if (pIconLarge && (*pIconLarge = ExtractOneResource(lpres, MAKEINTRESOURCE(phd->rgdir[index[0]].wOrdinal), RT_ICON, o32rva,0)))
                nRet++;
            if (pIconSmall && (*pIconSmall = ExtractOneResource(lpres, MAKEINTRESOURCE(phd->rgdir[index[1]].wOrdinal), RT_ICON, o32rva,0)))
                nRet++;
        }
        LocalFree(phd);
    }
    if (!inRom)
        LocalFree(lpres);
    return nRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_VerQueryValueW(
    VERBLOCK *pBlock,
    LPWSTR lpSubBlock,
    LPVOID *lplpBuffer,
    PUINT puLen
    ) 
{
    LPWSTR lpSubBlockOrg, lpEndBlock, lpEndSubBlock, lpStart;
    WCHAR cEndBlock, cTemp;
    BOOL bString;
    DWORD dwTotBlockLen, dwHeadLen;
    int nCmp;

    // validate parameters
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && (!SC_MapPtrWithSize (pBlock, sizeof (VERBLOCK), hCurProc)
            || !SC_MapPtrWithSize (puLen, sizeof (UINT), hCurProc)
            || !SC_MapPtrWithSize (lplpBuffer, sizeof (LPVOID), hCurProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_DATA);
        return FALSE;
    }
    
    *puLen = 0;
    if ((int)pBlock->wTotLen < sizeof(VERBLOCK)) {
        KSetLastError(pCurThread,ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!(lpSubBlockOrg = (LPWSTR)LocalAlloc(LPTR,(strlenW(lpSubBlock)+1)*sizeof(WCHAR)))) {
        KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    kstrcpyW(lpSubBlockOrg,lpSubBlock);
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
            dwHeadLen = DWORDUP(sizeof(VERBLOCK) - sizeof(WCHAR) + (strlenW(pBlock->szKey) + 1) * sizeof(WCHAR)) + DWORDUP(pBlock->wValLen);
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
                if (!(nCmp=kstrcmpi(lpStart, pBlock->szKey)))
                    break;
                pBlock=(VERBLOCK*)((LPBYTE)pBlock+DWORDUP(pBlock->wTotLen));
            }
            *lpSubBlock = cTemp;
            if (nCmp)
                goto NotFound;
        }
    }
    *puLen = pBlock->wValLen;
    lpStart = (LPWSTR)((LPBYTE)pBlock+DWORDUP((sizeof(VERBLOCK)-sizeof(WCHAR))+(strlenW(pBlock->szKey)+1)*sizeof(WCHAR)));
    *lplpBuffer = lpStart < (LPWSTR)((LPBYTE)pBlock+pBlock->wTotLen) ? lpStart : (LPWSTR)(pBlock->szKey+strlenW(pBlock->szKey));
    bString = pBlock->wType;
    *lpEndBlock = cEndBlock;
    LocalFree(lpSubBlockOrg);
    return (TRUE);
NotFound:
    KSetLastError(pCurThread,ERROR_RESOURCE_NAME_NOT_FOUND);
    *lpEndBlock = cEndBlock;
    LocalFree(lpSubBlockOrg);
    return (FALSE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetFileVersionInfoSizeW(
    LPWSTR lpFilename,
    LPDWORD lpdwHandle
    ) 
{
    LPBYTE lpres;
    BOOL inRom;
    DWORD o32rva, dwRet = 0;
    VERHEAD *pVerHead;
    if (lpdwHandle) {
        // validate parameters
        if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) && !SC_MapPtrWithSize (lpdwHandle, sizeof (DWORD), hCurProc)) {
            KSetLastError(pCurThread,ERROR_INVALID_DATA);
            return FALSE;
        }
        *lpdwHandle = 0;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFileVersionInfoSizeW entry: %8.8lx %8.8lx\r\n",lpFilename, lpdwHandle));
    if (lpres = OpenResourceFile(lpFilename, &inRom, &o32rva)) {
        if (pVerHead = (VERHEAD *)ExtractOneResource(lpres, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, o32rva, &dwRet)) {
            if (((DWORD)pVerHead->wTotLen > dwRet) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE))
                KSetLastError(pCurThread,ERROR_INVALID_DATA);
            else
                dwRet = pVerHead->wTotLen;
            LocalFree(pVerHead);
        }
        if (!inRom)
            LocalFree(lpres);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFileVersionInfoSizeW exit: %8.8lx\r\n",dwRet));
    return dwRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetFileVersionInfoW(
    LPWSTR lpFilename,
    DWORD dwHandle,
    DWORD dwLen,
    LPVOID lpData
    ) 
{
    LPBYTE lpres;
    BOOL inRom, bRet = FALSE;
    DWORD o32rva, dwTemp;
    VERHEAD *pVerHead;
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) && !SC_MapPtrWithSize (lpData, dwLen, hCurProc)) {
        KSetLastError(pCurThread,ERROR_INVALID_DATA);
        return FALSE;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFileVersionInfoW entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",lpFilename, dwHandle, dwLen, lpData));
    if (dwLen < sizeof(((VERHEAD*)lpData)->wTotLen))
        KSetLastError(pCurThread,ERROR_INSUFFICIENT_BUFFER);
    else if (lpres = OpenResourceFile(lpFilename, &inRom, &o32rva)) {
        if (pVerHead = (VERHEAD *)ExtractOneResource(lpres, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, o32rva, &dwTemp)) {
            if (((DWORD)pVerHead->wTotLen > dwTemp) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE))
                KSetLastError(pCurThread,ERROR_INVALID_DATA);
            else {
                dwTemp = (DWORD)pVerHead->wTotLen;
                if (dwTemp > dwLen)
                    dwTemp = dwLen;
                memcpy(lpData,pVerHead,dwTemp);
                ((VERHEAD*)lpData)->wTotLen = (WORD)dwTemp;
                bRet = TRUE;
            }
            LocalFree(pVerHead);
        }
        if (!inRom)
            LocalFree(lpres);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetFileVersionInfoSizeW exit: %d\r\n",bRet));
    return bRet;;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
AToUCopy(
    WCHAR *wptr,
    LPBYTE aptr,
    int maxlen
    ) 
{
    int len;
    for (len = 1; (len<maxlen) && *aptr; len++)
        *wptr++ = (WCHAR)*aptr++;
    *wptr = 0;
}

#define COMP_BLOCK_SIZE PAGE_SIZE

BYTE lpDecBuf[COMP_BLOCK_SIZE];
DWORD cfile=0xffffffff,cblock=0xffffffff;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_GetRomFileBytes(
    DWORD type,
    DWORD count,
    DWORD pos,
    LPVOID buffer,
    DWORD nBytesToRead
    ) 
{
    TOCentry *tocptr;
    FILESentry *filesptr;
    DWORD dwBlock,bRead,csize, fullsize, dwGlobalFileIndex;
    LPVOID buffercopy;
    ROMChain_t *pROM = ROMChain;
    DWORD  dwResult;

    dwGlobalFileIndex = count;

    // verify the buffer given by user.
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && !SC_MapPtrWithSize (buffer, nBytesToRead, hCurProc)) {
        return 0;
    }

//  DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileBytes entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
//      type,count,pos,buffer,nBytesToRead));
    
    if (type == 2) {
        while (pROM && (count >= pROM->pTOC->numfiles)) {
            count -= pROM->pTOC->numfiles;
            pROM = pROM->pNext;
        }
        if (!pROM) {
//          DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileBytes exit: %8.8lx\r\n",0));
            return 0;
        }
        tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
        filesptr = &(((FILESentry *)tocptr)[count]);
        if (pos > filesptr->nRealFileSize) {
//          DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileBytes exit: %8.8lx\r\n",0));
            return 0;
        }
        if (pos + nBytesToRead > filesptr->nRealFileSize) {
            nBytesToRead = filesptr->nRealFileSize - pos;
        }
        if (!nBytesToRead) {
            fullsize = 0;
        } else {
            fullsize = nBytesToRead;
            buffercopy = buffer;
            LockPages(buffercopy, fullsize, 0, LOCKFLAG_WRITE);
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
                    if ((dwGlobalFileIndex != cfile) || (dwBlock != cblock)) {
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
                                LeaveCriticalSection(&RFBcs);
                                UnlockPages(buffercopy, fullsize);
                                return 0;
                            }
                        } else {
                            dwResult = DecompressROM((LPBYTE)filesptr->ulLoadOffset,
                                                  filesptr->nCompFileSize, lpDecBuf,
                                                  csize, dwBlock*COMP_BLOCK_SIZE);
                            if ((dwResult == CEDECOMPRESS_FAILED) || (dwResult == 0)) {
                                LeaveCriticalSection(&RFBcs);
                                UnlockPages(buffercopy, fullsize);
                                return 0;
                            }
                            cfile = dwGlobalFileIndex;
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
            }
            LeaveCriticalSection(&RFBcs);
            UnlockPages(buffercopy, fullsize);
        }

//      DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileBytes exit: %8.8lx\r\n",fullsize));
        return fullsize;
    }

//  DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileBytes exit: %8.8lx\r\n",0));
    return 0;
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


// NOTE: we do not need to validate the pointers if the following functions are called directly
//       -- the thread is in KMode already, it can do anything it wants.

const PFNVOID ExtraMethods[] = {
    (LPVOID)SC_EventModify,         // 000
    (LPVOID)SC_ReleaseMutex,        // 001
    (LPVOID)SC_CreateAPIHandle,     // 002
    (LPVOID)SC_MapViewOfFile,       // 003
    (LPVOID)SC_ThreadGetPrio,       // 004
    (LPVOID)SC_ThreadSetPrio,       // 005
    (LPVOID)SC_SelectObject,        // 006
    (LPVOID)SC_PatBlt,              // 007
    (LPVOID)SC_GetTextExtentExPointW,// 008
    (LPVOID)SC_GetSysColorBrush,    // 009
    (LPVOID)SC_SetBkColor,          // 010
    (LPVOID)SC_GetParent,           // 011
    (LPVOID)SC_InvalidateRect,      // 012
    (LPVOID)NKRegOpenKeyExW,        // 013
    (LPVOID)NKRegQueryValueExW,     // 014
    (LPVOID)SC_CreateFileW,         // 015
    (LPVOID)SC_ReadFile,            // 016
    (LPVOID)SC_OpenDatabaseEx,      // 017
    (LPVOID)SC_SeekDatabase,        // 018
    (LPVOID)SC_ReadRecordPropsEx,   // 019
    (LPVOID)NKRegCreateKeyExW,      // 020
    (LPVOID)SC_DeviceIoControl,     // 021
    (LPVOID)SC_CloseHandle,         // 022
    (LPVOID)NKRegCloseKey,          // 023
    (LPVOID)SC_ClientToScreen,      // 024
    (LPVOID)SC_DefWindowProcW,      // 025
    (LPVOID)SC_GetClipCursor,       // 026
    (LPVOID)SC_GetDC,               // 027
    (LPVOID)SC_GetFocus,            // 028
    (LPVOID)SC_GetMessageW,         // 029
    (LPVOID)SC_GetWindow,           // 030
    (LPVOID)SC_PeekMessageW,        // 031
    (LPVOID)SC_ReleaseDC,           // 032
    (LPVOID)SC_SendMessageW,        // 033
    (LPVOID)SC_SetScrollInfo,       // 034
    (LPVOID)SC_SetWindowLongW,      // 035
    (LPVOID)SC_SetWindowPos,        // 036
    (LPVOID)SC_CreateSolidBrush,    // 037
    (LPVOID)SC_DeleteMenu,          // 038
    (LPVOID)SC_DeleteObject,        // 039
    (LPVOID)SC_DrawTextW,           // 040
    (LPVOID)SC_ExtTextOutW,         // 041
    (LPVOID)SC_FillRect,            // 042
    (LPVOID)SC_GetAsyncKeyState,    // 043
    (LPVOID)SC_GetDlgCtrlID,        // 044
    (LPVOID)SC_GetStockObject,      // 045
    (LPVOID)SC_GetSystemMetrics,    // 046
    (LPVOID)SC_RegisterClassWStub,  // 047
    (LPVOID)SC_RegisterClipboardFormatW, // 048
    (LPVOID)SC_SetBkMode,           // 049
    (LPVOID)SC_SetTextColor,        // 050
    (LPVOID)SC_TransparentImage,    // 051
    (LPVOID)SC_IsDialogMessageW,    // 052
    (LPVOID)SC_PostMessageW,        // 053
    (LPVOID)SC_IsWindowVisible,     // 054
    (LPVOID)SC_GetKeyState,         // 055
    (LPVOID)SC_BeginPaint,          // 056
    (LPVOID)SC_EndPaint,            // 057
    (LPVOID)SC_PerformCallBack4,    // 058
    (LPVOID)SC_CeWriteRecordProps,  // 059
    (LPVOID)SC_ReadFileWithSeek,    // 060
    (LPVOID)SC_WriteFileWithSeek,   // 061
};

ERRFALSE(sizeof(ExtraMethods) == sizeof(ExtraMethods[0])*62);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetRomFileInfo(
    DWORD type,
    LPWIN32_FIND_DATA lpfd,
    DWORD count
    ) 
{
    TOCentry *tocptr;
    FILESentry *filesptr;
    ROMChain_t *pROM = ROMChain;
    extern const PFNVOID Win32Methods[];
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo entry: %8.8lx %8.8lx %8.8lx\r\n",type,lpfd,count));

    // validate user pointer
    switch (type) {
        case 1:
        case 2:
            if (!SC_MapPtrWithSize(lpfd, sizeof(WIN32_FIND_DATAW), hCurProc)) {
                return FALSE;
            }
            break;
        case 3:
            if (!SC_MapPtrWithSize((LPVOID)count, sizeof(DWORD), hCurProc)) {
                return FALSE;
            }
            // fall through to check lpfd
        case 4:
            if (!SC_MapPtrWithSize(lpfd, sizeof(DWORD), hCurProc)) {
                return FALSE;
            }
            break;
    }
    switch (type) {
        case 1:
            // MODULES section of ROM
            while (pROM && (count >= pROM->pTOC->nummods)) {
                count -= pROM->pTOC->nummods;
                pROM = pROM->pNext;
            }
            if (!pROM) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",FALSE));
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
            DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",TRUE));
            return TRUE;
        case 2:
            // FILES section of ROM
            while (pROM && (count >= pROM->pTOC->numfiles)) {
                count -= pROM->pTOC->numfiles;
                pROM = pROM->pNext;
            }
            if (!pROM) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",FALSE));
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
            DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",TRUE));
            return TRUE;
        case 3:
            *(LPVOID *)lpfd = Win32Methods;
            
            *(BOOL UNALIGNED *)count = bAllKMode;
            return TRUE;
        case 4:
            *(LPVOID *)lpfd = ExtraMethods;
            return TRUE;
        case 0xffffffff:
            for (count = 0; count < pROM->pTOC->nummods; count++) {
                tocptr = &(((TOCentry *)&pROM->pTOC[1])[count]);
                if (!StrCmpAPascW(tocptr->lpszFileName,lpfd->cFileName)) {
                    DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",TRUE));
                    return TRUE;
                }
            }
            for (count = 0; count < pROM->pTOC->numfiles; count++) {
                tocptr = &(((TOCentry *)&pROM->pTOC[1])[pROM->pTOC->nummods]);
                filesptr = &(((FILESentry *)tocptr)[count]);
                if (!StrCmpAPascW(filesptr->lpszFileName,lpfd->cFileName)) {
                    DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",TRUE));
                    return TRUE;
                }
            }
            DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",FALSE));
            return FALSE;
    }       
    DEBUGCHK(0);
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetRomFileInfo exit: %8.8lx\r\n",FALSE));
    return FALSE;
}


/*
@doc BOTH EXTERNAL WINCEUSER

@func int       | LoadString| Load a string resource

@parm HINSTANCE | hinst     | Identifies an instance of the module whose
executable file contains the string resource.

@parm UINT      | wID       | Specifies the integer identifier of the string to
be loaded.

@parm LPTSTR    | lpBuffer  | Points to the buffer to receive the string. 

@parm int       | cchBuffer | Specifies the size of the buffer, in characters.
The string is truncated and null terminated if it is longer than the number of
characters specified.
                
@comm Follows the Win32 reference description with the following modification.
@comm If <p lpBuffer> is NULL, <p cchBuffer> is ignored and the return value is 
a pointer to the requested string, which is read-only.
@comm The length of the string, including any null terminator if present, can be
      found in the word preceding the string.

@devnote All string resources are Unicode.
*/
/* This code was modified from Win95. */
int SC_LoadStringW(
    HINSTANCE hInstance, 
    UINT wID, 
    LPWSTR lpBuffer, 
    int nBufMax)
{
    HRSRC  hResInfo;
    HANDLE  hStringSeg;
    LPWSTR   lpsz;
    int     cch;
    int     Ret =0;

    DEBUGMSG (ZONE_ENTRY, (L"SC_LoadStringW: 0x%8.8lx 0x%x 0x%8.8lx %d\n", 
        hInstance, wID, lpBuffer, nBufMax));
    // Make sure the parms are valid.
    if (lpBuffer && (nBufMax < 0 || !nBufMax--))
        return(0);
    cch = 0;

    // check parameters
    if (lpBuffer && !SC_MapPtrWithSize (lpBuffer, nBufMax, hCurProc)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return 0;
    }

    __try {
        // String Tables are broken up into 16 string segments.  Find the segment
        // containing the string we are interested in.
        if ((hResInfo = SC_FindResource(hInstance, MAKEINTRESOURCE((wID >> 4) + 1), RT_STRING))
            && (hStringSeg = SC_LoadResource(hInstance, hResInfo))) {

            // map the resouce to the secure section if it's a DLL
            // since it's always going to be there
            if (ZeroPtr (hStringSeg) >= (DWORD) DllLoadBase)
                lpsz = (LPWSTR) MapPtrProc (ZeroPtr (hStringSeg), ProcArray);
            else
                lpsz = (LPWSTR) hStringSeg;

            // Move past the other strings in this segment.
            wID &= 0x0F;
            while (TRUE) {
                cch = *lpsz++;
                if (wID-- == 0)
                    break;
                lpsz += cch;
            }

            if (lpBuffer) {
                // Don't copy more than the max allowed.
                if (cch > nBufMax)
                    cch = nBufMax;
                // Copy the string into the buffer
                memcpy (lpBuffer, lpsz, cch * sizeof(TCHAR));
                
            } else if (cch) { // don't return zero-length strings
                // The return value is the string itself

                // return a zero based address.
                Ret = ZeroPtr (lpsz);
            }
        }
        //else failed

        if (lpBuffer) { // if we are not returning the string as the return value
            // Append a NULL
            lpBuffer[Ret = cch] = 0;
        }
    }__except (EXCEPTION_EXECUTE_HANDLER) {
    
        // Ret is already 0.
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG (ZONE_ENTRY, (L"SC_LoadStringW Exits: %d\n", Ret));
    return Ret;
}


