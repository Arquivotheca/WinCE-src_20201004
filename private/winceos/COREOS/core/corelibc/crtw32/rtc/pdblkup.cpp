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
/***
*pdblkup.cpp - RTC support
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*
*Revision History:
*       07-28-98  JWM   Module incorporated into CRTs (from KFrei)
*       05-11-99  KBF   Error if RTC support define not enabled
*       05-26-99  KBF   Minor Cleanup, _RTC_ prefix added to GetSrcLine
*       11-30-99  PML   Compile /Wp64 clean.
*       06-20-00  KBF   Major mods to use PDBOpenValidate3 & MSPDB70
*       03-19-01  KBF   Fix buffer overruns (vs7#227306), eliminate all /GS
*                       checks (vs7#224261), use correct VS7 registry key.
*       03-28-01  PML   Protect against GetModuleFileName overflow (vs7#231284)
*       10-17-03  SJ    Change _RTC_GetSrcLine to use the new PDB API's
*       03-13-04  MSL   Assorted fixes for prefast
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       03-08-05  PAL   Correct boundary check before HeapAlloc call (VSW#435657).
*       03-23-05  MSL   Registry key change
*       05-23-05  PML   Don't store unencoded func ptrs (vsw#191114)
*       07-18-07  ATC   Fixing OACR warnings.
*
****/

#ifndef _RTC
#error  RunTime Check support not enabled!
#endif

#ifndef _WIN32_WCE
#include "rtcpriv.h"

#include <stdlib.h>
#include <limits.h>

#include "pdb.h"
#include "cvinfo.h"

#include "verstamp.h" // for RM?_AS_WIDE_STRING, remove if this can use _PROD_VER_something
#define REGISTRY_KEY_MASTER HKEY_LOCAL_MACHINE
#define MSPDB_FILENAME L"MSPDB" RMJ_AS_WIDE_STRING RMM_AS_WIDE_STRING

#if defined(_M_X64)

#define REGISTRY_KEY_NAME L"ProductDir"
#define REGISTRY_KEY_LOCATION L"SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\" RMJ_AS_WIDE_STRING L"." RMM_AS_WIDE_STRING L"\\Setup\\VC"
#define MSPDB_NAME L"bin\\amd64\\" MSPDB_FILENAME L".DLL"

#else

#define REGISTRY_KEY_NAME L"EnvironmentDirectory"
#define REGISTRY_KEY_LOCATION L"SOFTWARE\\Microsoft\\VisualStudio\\" RMJ_AS_WIDE_STRING L"." RMM_AS_WIDE_STRING L"\\Setup\\VS"
#define MSPDB_NAME MSPDB_FILENAME L".DLL"

#endif

static const wchar_t mspdbName[] = MSPDB_NAME;

static HINSTANCE mspdb  = 0;

#define declare(rettype, call_type, name, parms)\
    extern "C" { typedef rettype ( call_type * name ## Proc) parms; }
#define decldef(rettype, call_type, name, parms)\
    declare(rettype, call_type, name, parms)\
    static name ## Proc pfn ## name = 0

#define GetProcedure(lib, name) *(FARPROC *) &pfn ## name = GetProcAddress(lib, #name)

#define GetReqProcedure(lib, name, err) {if (!(GetProcedure(lib, name))) return err;}

 
/* PDB functions */
declare(BOOL, __cdecl, PDBOpenValidate5,
        (const wchar_t *, const wchar_t *, void *, PfnPDBQueryCallback, OUT EC *, OUT wchar_t *, size_t, OUT PDB **pppdb));

/* Registry Functions */
declare(LSTATUS, APIENTRY, RegOpenKeyExW,
        (_In_ HKEY hKey, _In_opt_ LPCWSTR lpSubKey, _Reserved_ DWORD ulOptions, _In_ REGSAM samDesired, _Out_ PHKEY phkResult));
declare(LSTATUS, APIENTRY, RegQueryValueExW,
        (_In_ HKEY hKey, _In_opt_ LPCWSTR lpValueName, _Reserved_ LPDWORD lpReserved, _Out_opt_ LPDWORD lpType,
         _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData, _Inout_opt_ LPDWORD lpcbData));
declare(LSTATUS, APIENTRY, RegCloseKey,
        (_In_ HKEY hKey));

static HINSTANCE
GetPdbDllFromInstallPath()
{
#ifdef _CORESYS
#define REGISTRY_DLL L"api-ms-win-core-registry-l1-1-0.dll"
#else
#define REGISTRY_DLL L"ADVAPI32.DLL"
#endif

    HMODULE hModReg = LoadLibraryExW(REGISTRY_DLL, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#ifndef _CORESYS    
    if (!hModReg && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom,
        // try one more time using default options
        hModReg = LoadLibraryW(REGISTRY_DLL);
    }
#endif
    if (!hModReg)
    {
        return NULL;
    }

    RegOpenKeyExWProc pfnRegOpenKeyExW;
    GetReqProcedure(hModReg, RegOpenKeyExW, 0);
    RegQueryValueExWProc pfnRegQueryValueExW;
    GetReqProcedure(hModReg, RegQueryValueExW, 0);
    RegCloseKeyProc pfnRegCloseKey;
    GetReqProcedure(hModReg, RegCloseKey, 0);

    HKEY hkey;
    LSTATUS err;

    err = (*pfnRegOpenKeyExW)(REGISTRY_KEY_MASTER, REGISTRY_KEY_LOCATION, 0, KEY_QUERY_VALUE, &hkey);
    if (err != ERROR_SUCCESS)
    {
        ::FreeLibrary(hModReg);
        return NULL;
    }
    
    DWORD type;
    wchar_t buf[MAX_PATH];
    DWORD cb = sizeof(buf);

    err = (*pfnRegQueryValueExW)(hkey, REGISTRY_KEY_NAME, NULL, &type, (LPBYTE)buf, &cb);

    (*pfnRegCloseKey)(hkey);
    ::FreeLibrary(hModReg);

    if (err != ERROR_SUCCESS)
    {
        return NULL;
    }

    if (type != REG_SZ)
    {
        // Not a string

        return NULL;
    }

    if ((cb & 1) != 0)
    {
        // Not a valid wide character string

        return NULL;
    }

    DWORD cch = cb / sizeof(wchar_t);

    if (cch < 2)
    {
        // Can't be a non-empty null terminated string

        return NULL;
    }

    if (buf[--cch] != L'\0')
    {
        // String isn't null terminated

        return NULL;
    }

    // Terminate with slash if not already.
    // This can't overflow since we are overwriting the null terminator we just backed up over

    if (buf[cch - 1] != L'\\')
    {
        buf[cch++] = L'\\';
    }

    if (UINT_MAX - cch < _countof(mspdbName) + 1)
    {
        return NULL;
    }

    if ((cch + _countof(mspdbName)) > _countof(buf))
    {
        return NULL;
    }

    for (size_t pos = 0; pos < _countof(mspdbName); pos++)
    {
        buf[cch + pos] = mspdbName[pos];
    }

    // buf should contain the full path of the dll
    HMODULE mspdbHandle = ::LoadLibraryExW(buf, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
#if !defined(_M_ARM)
    if (!mspdbHandle && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom,
        // try one more time using an alternate, but still secure, file search strategy
        mspdbHandle = ::LoadLibraryExW(buf, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
#endif

    return mspdbHandle;
}

#ifdef  _DEBUG

#define DLL_EXT L"DLL"
#define MSVCRD_FILENAME L"MSVCR" RMJ_AS_WIDE_STRING RMM_AS_WIDE_STRING L"D.dll"
static const wchar_t msvcrdFilename[] = MSVCRD_FILENAME;

static BOOL GetPdbDllPathFromFilePath(
    const wchar_t * sourcePath,
    wchar_t * pdbDllPath, /* out */
    size_t pdbDllPathSize)
{
    static const wchar_t dllExt[] = DLL_EXT;
    static const wchar_t mspdbFilename[] = MSPDB_FILENAME;

    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    wchar_t fileName[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];

    // Split the given source path, into path components
    if (0 != _wsplitpath_s(sourcePath, drive, _countof(drive), dir, _countof(dir), fileName, _countof(fileName), ext, _countof(ext)))
    {
        return FALSE;
    }

    // Copy replacement mspdb filename and extension
    if (0 != wcscpy_s(fileName, _countof(mspdbFilename), mspdbFilename))
    {
        return FALSE;
    }

    if (0 != wcscpy_s(ext, _countof(dllExt), dllExt))
    {
        return FALSE;
    }

    if (0 != _wmakepath_s(pdbDllPath, pdbDllPathSize, drive, dir, fileName, ext))
    {
        return FALSE;
    }

    return TRUE;
}

#endif // _DEBUG

static HINSTANCE
GetPdbDll()
{
    static bool alreadyTried = false;
    // If we already tried to load it, return
    if (alreadyTried)
        return NULL;
    alreadyTried = true;

    HINSTANCE moduleInstance = NULL;

    // First try loading for default installation path
    if (NULL != (moduleInstance = GetPdbDllFromInstallPath()))
    {
        return moduleInstance;
    }

#ifdef  _DEBUG

    // Then for debug only, try to load it from the same path as msvcr debug dll

    wchar_t sourcePath[MAX_PATH];
    wchar_t pdbPath[MAX_PATH];

    if (HMODULE moduleHandle = GetModuleHandleW(msvcrdFilename))
    {
        if (GetModuleFileNameW(moduleHandle, sourcePath, _countof(sourcePath)))
        {
            // The path now contains the full path of msvcr debug dll

            if (GetPdbDllPathFromFilePath(sourcePath, pdbPath, _countof(pdbPath)))
            {
                moduleInstance = ::LoadLibraryExW(pdbPath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
#if !defined(_M_ARM)
                if (!moduleInstance && GetLastError() == ERROR_INVALID_PARAMETER)
                {
                    // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 search options are not supported on this platfrom,
                    // try one more time using an alternate, but still secure, file search strategy
                    moduleInstance = ::LoadLibraryExW(pdbPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
                }
#endif
                if (moduleInstance)
                    return moduleInstance;
            }
        }
    }

    // Finally, try loading the dll from the application folder
    moduleInstance = LoadLibraryExW(MSPDB_FILENAME, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (moduleInstance)
        return moduleInstance;

#if !defined(_M_ARM)
    // The LoadLibraryEx search options, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32, are not supported
    // try an alternative search options
    if (!moduleInstance && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        if (GetModuleFileNameW(0, sourcePath, _countof(sourcePath)) &&
            GetPdbDllPathFromFilePath(sourcePath, pdbPath, _countof(pdbPath)) &&
            (NULL != (moduleInstance = LoadLibraryExW(pdbPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))))
            {
                return moduleInstance;
            }
    }
#endif

#endif //_DEBUG

    // mspdb dll was not found anywhere, just return NULL
    return NULL;
}

BOOL
_RTC_GetSrcLine(
    LPBYTE address,
    wchar_t * source,
    DWORD sourcelen,
    int* pline,
    wchar_t * moduleName,
    DWORD modulelen
    )
{
    *pline = 0;
    *source = 0;

    MEMORY_BASIC_INFORMATION mbi;

    // The address which we get from ReturnAddress is the address
    // of where the call returns to, so decrementing it will give
    // address of the offending line
    
    --address;
    
    if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0)
    {
        // Can not identify memory region containing address

        return FALSE;
    }

    if (GetModuleFileNameW(HMODULE(mbi.AllocationBase), moduleName, modulelen) == 0)
    {
        // Can not identify loaded module containing address
        return FALSE;
    }

    const IMAGE_DOS_HEADER *pDosHeader = (IMAGE_DOS_HEADER *) mbi.AllocationBase;

    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        // The image doesn't have an MS-DOS executable
        return FALSE;
    }

    if (pDosHeader->e_lfanew <= 0) {
        // There is no pointer to a PE header

        return FALSE;
    }

    IMAGE_NT_HEADERS *pNtHeader = (IMAGE_NT_HEADERS *) ((BYTE *) mbi.AllocationBase + pDosHeader->e_lfanew);

    if (pNtHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        // The file isn't a PE executable

        return FALSE;
    }

    unsigned NumberOfSections = pNtHeader->FileHeader.NumberOfSections;
    const IMAGE_SECTION_HEADER *rgSectionHeader = IMAGE_FIRST_SECTION(pNtHeader);

    DWORD rva = DWORD(address - (BYTE *) mbi.AllocationBase);

    unsigned isec=0;
    DWORD ibSec=0;

    for (isec = 0; isec < NumberOfSections; isec++)
    {
        if (rva < rgSectionHeader[isec].VirtualAddress)
        {
            continue;
        }

        ibSec = rva - rgSectionHeader[isec].VirtualAddress;

        if (rva >= rgSectionHeader[isec].Misc.VirtualSize)
        {
            continue;
        }

        break;
    }

    if (isec == NumberOfSections)
    {
        // Can't identify the section containing this address

        return FALSE;
    }

    isec++;                            // Section numbers are 1 based

    // Try to load the PDB DLL

    static bool PDBOK = false;

    if (!PDBOK)
    {
        if (mspdb || !(mspdb = GetPdbDll()))
        {
            // If we already loaded it before, there must be some missing API function

            return FALSE;
        }

        PDBOK = true;
    }

    // Get the proc address every time, for security - this is called only in
    // fatal error paths.

    PDBOpenValidate5Proc pfnPDBOpenValidate5;
    GetReqProcedure(mspdb, PDBOpenValidate5, 0);

    EC ec;
    PDB *ppdb;

    if (!(*pfnPDBOpenValidate5)(moduleName, NULL, NULL, 0, &ec, NULL, 0, &ppdb))
    {
        // Can't find the PDB

        return FALSE;
    }

    BOOL res = FALSE;

    if (ppdb->QueryInterfaceVersion() != PDB::intv)
    {
        // Can't find the PDB

        goto DONE1;
    }

    DBI *pdbi;

    if (!ppdb->OpenDBI(NULL, pdbRead, &pdbi))
    {
        // Can't open the DBI

        goto DONE1;
    }

    // Now get the Mod from the section index & the section-relative address

    Mod *pmod;

    if (!pdbi->QueryModFromAddr(isec, (long) ibSec, &pmod, NULL, NULL, NULL))
    {
        // Can't find the Mod and seccontrib containing this address

        goto DONE2;
    }

    EnumLines* penum = NULL;
    if(!pmod->GetEnumLines( &penum) || penum== NULL ) 
    {
        goto DONE3;
    }

    CV_Line_t* pLines = NULL;
    
    while(penum->next())
    {
        DWORD cbBlk;
        WORD segBlk;
        DWORD offBlk;
        unsigned long cLines;

        if(!penum->getLines( 
            NULL,
            &offBlk,        // offset part of address
            &segBlk,        // segment part of address
            &cbBlk,         // size of the section
            &cLines,        // number of lines (in/out)
            NULL           // pointer to buffer for line info
        ))
        {
            goto DONE4;
        }

        if ( (segBlk == isec) && (offBlk <= ibSec) && (ibSec < offBlk + cbBlk))
        {
            if(cLines == 0)
            {
                goto DONE4;
            }

            // We use HeapAlloc instead of new to remove CRT Dependency; also, protect from integer overflow
            if (cLines < (SIZE_MAX / sizeof(CV_Line_t)))
            {
                pLines = (CV_Line_t *)HeapAlloc(GetProcessHeap(), 0, sizeof(CV_Line_t) * cLines);
            }

            if ( pLines == NULL )
            {
                goto DONE4;
            }

            DWORD idFile;

            if(!penum->getLines(&idFile,NULL, NULL,NULL,&cLines,pLines))
            {
                goto DONE5;
            }

            DWORD ibSecRel = ibSec - offBlk;
            
            if(ibSecRel < pLines[0].offset)
            {
                goto DONE5;
            }
            
            unsigned int i;            
            for ( i = 1; i < cLines; i++ ) 
            {
                if (ibSecRel < pLines[i].offset)
                {
                    break;
                }
            }
            
            *pline = pLines[i - 1].linenumStart ;
            
            if(!pmod->QueryFileNameInfo( idFile, source, &sourcelen, NULL, NULL, NULL ))
            {
                goto DONE5;
            }

            res = TRUE;

            break;
        }


    }

DONE5:
    HeapFree(GetProcessHeap(), 0, pLines);
DONE4:
    penum->release();
DONE3:
    pmod->Close();
DONE2:
    pdbi->Close();
DONE1:
    ppdb->Close();
    return res;
}

#endif // _WIN32_WCE
