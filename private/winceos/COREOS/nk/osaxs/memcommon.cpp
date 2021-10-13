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
/*++

Module Name:

    memcommon.cpp

Abstract:

    Common memory functions shared between host and target side.

Environment:

    OsaxsT, OsaxsH

--*/

#include "osaxs_p.h"

#define DWORDUP(x) (((x)+3)&~03)

typedef struct VERHEAD
{
    WORD wTotLen;
    WORD wValLen;
    WORD wType;         /* always 0 */
    WCHAR szKey[(sizeof("VS_VERSION_INFO")+3)&~03];
    VS_FIXEDFILEINFO vsf;
} VERHEAD ;

typedef struct resroot_t 
{
    DWORD flags;
    DWORD timestamp;
    DWORD version;
    WORD  numnameents;
    WORD  numidents;
} resroot_t;

typedef struct resent_t 
{
    DWORD id;
    DWORD rva;
} resent_t;


void ReadPdbName(PROCESS *pVM, DWORD dwAddr, DWORD dwEndAddress, ModuleSignature *pms)
{
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ReadPdbName: 0x%08x proc 0x%08x\r\n", dwAddr, pVM));

    DWORD AddrEnd = dwAddr;
    DWORD dwAddrPdbPath = dwAddr;

    memset(&pms->szPdbName, 0, sizeof(pms->szPdbName));

    // Get just the name of the PDB, bypass the path by looking for the last '\'
    char PdbNameBuf[_MAX_PATH];
    BOOL TerminateEarly = FALSE;    
    while (!TerminateEarly && AddrEnd < dwEndAddress)
    {
        DWORD ToRead = dwEndAddress - AddrEnd;
        if (ToRead > sizeof (PdbNameBuf))
            ToRead = sizeof (PdbNameBuf);
        DBGRETAILMSG (OXZONE_MEM, (L"##ReadPdbName: OsAxsReadMemory (%08X:%08X)\r\n", pVM, AddrEnd));
        DWORD BytesRead = OsAxsReadMemory (PdbNameBuf, pVM, reinterpret_cast<void *>(AddrEnd), ToRead);
        if (BytesRead)
        {
            DWORD i = 0;
            while (PdbNameBuf[i] && i < BytesRead)
            {
                if (PdbNameBuf[i] == '\\')
                {
                    dwAddrPdbPath = AddrEnd + 1;
                }
                ++ AddrEnd;
                ++ i;
            }

            if ((i < BytesRead) && (i < _MAX_PATH) && PdbNameBuf[i] == '\0')
                TerminateEarly = TRUE;
        }

        if (BytesRead != ToRead)
        {
            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!ReadPdbName: Read 0x%08x, Expected %d, read %d\r\n", dwAddr, ToRead, BytesRead));
            TerminateEarly = TRUE;
        }
    }
    
    if (AddrEnd > dwAddr)
    {
        DWORD dwPdbNameSize = min(AddrEnd-dwAddrPdbPath+1, sizeof(pms->szPdbName));

        DBGRETAILMSG (OXZONE_MEM, (L"##ReadPdbName: OsAxsReadMemory (%08X:%08X), read PDB name\r\n", pVM, dwAddrPdbPath));
        OsAxsReadMemory(&pms->szPdbName, pVM, (void *)dwAddrPdbPath, dwPdbNameSize);
        pms->szPdbName[sizeof(pms->szPdbName) - 1] = 0;
    }
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ReadPdbName\r\n"));    
}


HRESULT ReadNB10Record(PROCESS *pVM, DWORD dwAddr, DWORD dwEndAddress, ModuleSignature *pms )
{
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ReadNB10Record: 0x%08x proc 0x%08x\r\n", dwAddr, pVM));

    HRESULT hr = S_OK;
    DWORD dwOffset;

    if (OsAxsReadMemory (&dwOffset, pVM, (void *)dwAddr, sizeof(dwOffset)) != sizeof (dwOffset))
        hr = E_FAIL;

    if (SUCCEEDED (hr))
    {
        dwAddr += sizeof( dwOffset );
        // NB10 has a 4-byte timestamp.  Zero fill the guid and place the time stamp into the first 4 bytes.
        memset(&pms->guid, 0, sizeof(pms->guid));
        if (OsAxsReadMemory (&pms->guid.Data1, pVM, reinterpret_cast<void *>(dwAddr), sizeof (pms->guid.Data1)) != sizeof (pms->guid.Data1))
            hr = E_FAIL;
    }

    if (SUCCEEDED (hr))
    {
        dwAddr += sizeof(pms->guid.Data1);
        if (OsAxsReadMemory(&pms->dwAge, pVM, (void *)dwAddr, sizeof(pms->dwAge)) != sizeof (pms->dwAge))
            hr = E_FAIL;
    }

    if (SUCCEEDED (hr))
    {
        dwAddr += sizeof(pms->dwAge);

        // Copy over the PDB name without the path
        ReadPdbName(pVM, dwAddr, dwEndAddress, pms);
    }

    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ReadNB10Record -- hresult 0x%08x\r\n", hr));    
    return hr;
}


HRESULT ReadRSDSRecord(PROCESS *pVM, DWORD dwAddr, DWORD dwEndAddress, ModuleSignature *pms)
{
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ReadRSDSRecord: 0x%08x proc 0x%08x\r\n", dwAddr, pVM));
    
    HRESULT hr = S_OK;

    DBGRETAILMSG (OXZONE_MEM, (L"##ReadRSDSRecord: OsAxsReadMemory (%08X:%08X), PDB GUID\r\n", pVM, dwAddr));
    if (OsAxsReadMemory(&pms->guid, pVM, (void *)dwAddr, sizeof(GUID)) != sizeof (GUID))
        hr = E_FAIL;

    if (SUCCEEDED (hr))
    {
        dwAddr += sizeof(pms->guid);
    
        DBGRETAILMSG (OXZONE_MEM, (L"##ReadRSDSRecord: OsAxsReadMemory (%08X:%08X), PDB Age\r\n", pVM, dwAddr));
        if (OsAxsReadMemory (&pms->dwAge, pVM, (void *)dwAddr, sizeof(DWORD)) != sizeof (DWORD))
            hr = E_FAIL;
    }

    if (SUCCEEDED (hr))
    {
        dwAddr += sizeof(pms->dwAge);

        // Copy over the PDB name without the path
        ReadPdbName(pVM, dwAddr, dwEndAddress, pms);
    }
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ReadRSDSRecord -- hresult 0x%08x\r\n", hr));
    return hr;
}


HRESULT ReadCVRecord(PROCESS *pVM, DWORD dwDbgRecAddr, DWORD dwEndAddr, ModuleSignature *pms)
{
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ReadCVRecord: 0x%08x proc 0x%08x\r\n", dwDbgRecAddr, pVM));

    HRESULT hr = S_OK;

    DBGRETAILMSG (OXZONE_MEM, (L"##ReadCVRecord: OsAxsReadMemory (%08X:%08X), CV type\r\n", pVM, dwDbgRecAddr));
    if (OsAxsReadMemory (&pms->dwType, pVM, (void *)dwDbgRecAddr, sizeof (pms->dwType)) == sizeof (pms->dwType))
    {
        dwDbgRecAddr += sizeof(pms->dwType);
        switch( pms->dwType )
        {
            case '01BN':
                hr = ReadNB10Record(pVM, dwDbgRecAddr, dwEndAddr, pms);
                break;

            case 'SDSR':
                hr = ReadRSDSRecord(pVM, dwDbgRecAddr, dwEndAddr, pms);
                break;

            default:
                DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadCVRecord: Invalid type code.\r\n" ));
                hr = E_FAIL;
                break;
        }
    }
    else
    {
        DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadCVRecord: Unable to read CV record @0x%08x.\r\n", dwDbgRecAddr));    
        hr = E_FAIL;
    }
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ReadCVRecord -- hresult 0x%08x\r\n", hr));    
    return hr;
}


HRESULT ReadDebugDirectory(PROCESS *pVM, LPBYTE lpDeb, DWORD o32rva, ModuleSignature *pms )
{
    HRESULT hr = S_OK;
    IMAGE_DEBUG_DIRECTORY idd;
    DWORD dwDebugRecordAddress;

    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ReadDebugDirectory: in lpDeb=0x%08x, o32rva=0x%08x\r\n", lpDeb, o32rva));

    DBGRETAILMSG (OXZONE_MEM, (L"##ReadDebugDirectory: OsAxsReadMemory (%08X:%08X)\r\n", pVM, lpDeb));
    if (OsAxsReadMemory (&idd, pVM, lpDeb, sizeof (idd)) == sizeof (idd))
    {
        if (idd.Type != IMAGE_DEBUG_TYPE_CODEVIEW)
        {
            DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadDebugDirectory: Debug directory is not CV type.\r\n" ));
            hr = E_FAIL;
            goto Exit;
        }

        if (idd.AddressOfRawData)
        {
            dwDebugRecordAddress = idd.AddressOfRawData + (DWORD)lpDeb - o32rva;

            hr = ReadCVRecord(pVM, dwDebugRecordAddress, dwDebugRecordAddress + idd.SizeOfData - 1, pms);
            if (FAILED(hr))
            {
                DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadDebugDirectory: ReadCVRecord failed, hr=0x%08X\r\n", hr));
                hr = E_FAIL;
                goto Exit;
            }
        }
        else
        {
            DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadDebugDirectory: AddressOfRawData == 0, CV debug info not in image.\r\n"));
            hr = E_FAIL;
            goto Exit;
        }    
    }
    else
    {
        DEBUGGERMSG( OXZONE_CV_INFO, ( L"  MemCommon!ReadDebugDirectory: Unable to read IMAGE_DEBUG_DIRECTORY @0x%08x\r\n", lpDeb));    
        hr = E_FAIL;
    }


Exit:
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ReadDebugDirectory: hr=0x%08x\r\n", hr));

    return hr;
}


/*----------------------------------------------------------------------------
    GetDebugInfo

    Get debug info for a process or module.
----------------------------------------------------------------------------*/
HRESULT GetDebugInfo(void* pModOrProc, BOOL fMod, ModuleSignature *pms)
{
    HRESULT  hRes            = E_FAIL;
    BOOL     fDebugInfoFound = FALSE;
    DWORD    o32rva = 0;
    LPBYTE   lpDeb = NULL;
    LPBYTE   BasePtr = NULL;
    e32_lite *pe32l = NULL;
    PPROCESS pProc = NULL;
    PMODULE  pMod = NULL;

    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!GetDebugInfo: Enter\r\n"));

    if (NULL == pms)
    {
        DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetDebugInfo: pms==NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    if (!fMod)
    { // a process
        pProc = (PPROCESS) pModOrProc;

        // Make sure process is not busy starting or dying (Check if using CoreDll.dll)
        // Except for Nk.exe, we always allow it
        if (!OSAXSIsProcessValid(pProc) && pProc != g_pprcNK)
        {
            // When process is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
            DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetDebugInfo: Process ID 0x%08X (%s) has state %d, may be in startup or shutdown.\r\n",
                                       pProc->dwId, pProc->lpszProcName, pProc->bState));
            hRes = E_FAIL;
            goto Exit;
        }
        
        BasePtr = (LPBYTE) pProc->BasePtr;
        pe32l = &pProc->e32;
    }
    
    else if ((pMod = (PMODULE) pModOrProc) == ((PMODULE) pModOrProc)->lpSelf)
    { // a module
        if (!OSAXSIsModuleValid(pMod))
        {
            // When a module is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetDebugInfo: Inuse flag for module (%s) not set, returning ...\r\n", pMod->lpszModName));
            hRes = E_FAIL;
            goto Exit;
        }
        
        BasePtr = (LPBYTE) pMod->BasePtr;
        pe32l = &pMod->e32;
    }

    if (pe32l != NULL && pe32l->e32_unit[DEB].rva != NULL && BasePtr != NULL)
    {
        // Get the address of the start of the debug directory.
        // NOTE this code is different from the same code in GetVersionInfo.
        // Apparently for e32_unit[RES], the rva field got fixed up with the actual virtual address.
        // For e32_unit[DBG], the rva field is unfixed up and still needs +BasePtr;
        o32rva = pe32l->e32_unit[DEB].rva;
        lpDeb = &BasePtr[pe32l->e32_unit[DEB].rva];

        if (lpDeb)
        {
            __try
            {
                hRes = ReadDebugDirectory(pProc, lpDeb, o32rva, pms);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetDebugInfo: ReadDebugDirectory failed for %s, hRes=0x%08X\r\n", 
                                              pMod ? pMod->lpszModName : pProc->lpszProcName, 
                                              hRes));
                    hRes = E_FAIL;
                    goto Exit;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                DBGRETAILMSG(OXZONE_CV_INFO, (L"  MemCommon!GetDebugInfo: Access Violation in ReadDebugDirectory!! lpDeb=0x%08X, o32rva=0x%08X\r\n", lpDeb, o32rva));
                hRes = E_FAIL;
                goto Exit;
            }
            
            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetDebugInfo: Type=%c%c%c%c, Age=%u, guid.Data1=0x%08X, Mod=%s\r\n", 
                                        (CHAR)(pms->dwType), (CHAR)(pms->dwType>>8), (CHAR)(pms->dwType>>16), (CHAR)(pms->dwType>>24),
                                        pms->dwAge,
                                        pms->guid.Data1,
                                        pMod ? pMod->lpszModName : pProc->lpszProcName));
            fDebugInfoFound = TRUE;
        }
        else
        {
            DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetDebugInfo: Debug table not found.\r\n"));
        }
    }

    if (!fDebugInfoFound)
    {
        DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetDebugInfo: No Debug Info for %s\r\n", pMod ? pMod->lpszModName : pProc->lpszProcName));
        hRes = E_FAIL;
        goto Exit;
    }
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!GetDebugInfo: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


//------------------------------------------------------------------------------
// Find type entry by number
//------------------------------------------------------------------------------
DWORD FindTypeByNum(PROCESS *pVM, LPBYTE curr, DWORD id) 
{
    DWORD dwRva = 0;
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!FindTypeByNum (curr=0x%08x, id=%d)\r\n", curr, id));

    resroot_t root;
    DWORD bytesread = OsAxsReadMemory (&root, pVM, curr, sizeof (resroot_t));
    if (bytesread == sizeof (resroot_t))
    {
        BOOL FoundEntry = FALSE;
        resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t) + root.numnameents*sizeof(resent_t));
        while (!FoundEntry && root.numidents > 0)
        {
            resent_t res;
            bytesread = OsAxsReadMemory (&res, pVM, resptr, sizeof (resent_t));
            if (bytesread == sizeof (resent_t))
            {
                if (!id || (res.id == id))
                {
                    KD_ASSERT (!IsSecureVa (res.rva));
                    dwRva = res.rva;
                    FoundEntry = TRUE;
                }
                ++resptr;
                --root.numidents;
            }
            else
            {
                DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!FindTypeByName: Failed to read entry, read %d\r\n", bytesread));
                // Assume that the page is invalid.  Advance the pointer until
                // it can hit the next page and hopefully it can read it.
                void *NextPage = (void *)(PAGEALIGN_DOWN((DWORD) resptr) + VM_PAGE_SIZE);
                while (resptr < NextPage && root.numidents > 0)
                {
                    ++resptr;
                    --root.numidents;
                }
            }
        }
    }
    else
    {
        DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!FindTypeByNum: Failed to read resroot, read %d\r\n", bytesread));
    }

    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!FindTypeByNum: Leave, dwRva=0x%08X\r\n", dwRva));
    return dwRva;
}


//------------------------------------------------------------------------------
// Find first entry (from curr pointer)
//------------------------------------------------------------------------------
DWORD FindFirst(PROCESS *pVM, LPBYTE curr) 
{
    DWORD dwRva = 0;
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!FindFirst: Enter\r\n"));

    resroot_t root;
    if (OsAxsReadMemory (&root, pVM, curr, sizeof (resroot_t)) == sizeof (resroot_t))
    {
        resent_t *resptr = (resent_t *)(curr + sizeof(resroot_t) + root.numnameents*sizeof(resent_t));

        resent_t res;
        if (OsAxsReadMemory (&res, pVM, resptr, sizeof (resent_t)) == sizeof (resent_t))
        {
            dwRva = res.rva;
        }
    }

    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!FindFirst: Leave, dwRva=0x%08X\r\n", dwRva));

    return dwRva;
}

const DWORD dwMSB = 0x80000000ul;

//------------------------------------------------------------------------------
// Extract a resource
//------------------------------------------------------------------------------
LPVOID ExtractOneResource(
    PROCESS *pVM,
    LPBYTE lpres,
    LPCWSTR lpszName,
    LPCWSTR lpszType,
    DWORD o32rva,
    LPDWORD pdwSize
    ) 
{
    DWORD trva = 0;
    LPVOID lpData = NULL;

    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!ExtractOneResource VM=%08X(%s), lpres=%08X, name=%08X, type=%08X, o32rva=%d: Enter\r\n",
                                pVM, pVM? pVM->lpszProcName:L"(noname)", lpres, lpszName, lpszType, o32rva));

    trva = FindTypeByNum(pVM, lpres, (DWORD)lpszType);
    if(trva == NULL)
    {
        goto Exit;
    }
    trva = FindTypeByNum(pVM, lpres+(trva&~dwMSB),(DWORD)lpszName);
    if(trva == NULL)
    {
        goto Exit;
    }
    trva = FindFirst(pVM, lpres+(trva&~dwMSB));
    if(trva == NULL || (trva & dwMSB))
    {
        goto Exit;
    }

    trva = (ULONG)(lpres + trva);

    resdata_t res;
    if (OsAxsReadMemory (&res, pVM, reinterpret_cast<void *>(trva), sizeof (resdata_t)) == sizeof (res))
    {
        if (pdwSize)
        {
            *pdwSize = res.size;
        }

        lpData = lpres + (res.rva - o32rva);
    }
    
Exit:
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!ExtractOneResource: Leave, lpData=0x%08X\r\n", lpData));
    return lpData;
}


typedef struct VERBLOCK {
    WORD wTotLen;
    WORD wValLen;
    WORD wType;
    WCHAR szKey[1];
} VERBLOCK ;


/*----------------------------------------------------------------------------
    GetVersionInfo

    Get version info for a process or module.
----------------------------------------------------------------------------*/
HRESULT GetVersionInfo(void* pModOrProc, BOOL fMod, VS_FIXEDFILEINFO *pvsFixedInfo)
{
    VERHEAD  VerHeadBuf[2];
    BOOL     fVersionInfoFound = FALSE;
    DWORD    o32rva    = 0;
    LPBYTE   lpres     = NULL;
    VERHEAD  *pVerHead = reinterpret_cast<VERHEAD *>(VerHeadBuf);
    LPWSTR   lpStart = NULL;
    DWORD    dwTemp = 0;
    LPBYTE   BasePtr = NULL;
    e32_lite *pe32l = NULL;
    PPROCESS pProc = NULL;
    PMODULE  pMod = NULL;
    HRESULT  hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_CV_INFO,(L"++MemCommon!GetVersionInfo: Enter\r\n"));

    if (!fMod)
    {
        pProc = (PPROCESS) pModOrProc;

        if (!OSAXSIsProcessValid(pProc))
        {
            // When process is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
            DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetVersionInfo: Process ID 0x%08X (%s) bState = %d, may be in startup or shutdown.\r\n",
                                       pProc->dwId, pProc->lpszProcName, pProc->bState));
            hRes = E_FAIL;
            goto Exit;
        }
        
        BasePtr = (LPBYTE) pProc->BasePtr;
        pe32l = &pProc->e32;
    }
    else if ((pMod = (LPMODULE) pModOrProc) == ((LPMODULE)pModOrProc)->lpSelf)
    {
        
        // This data is read only, so use the NK page table for access.
        pProc = g_pprcNK;

        if (!OSAXSIsModuleValid(pMod))
        {
            // When a module is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
            DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetVersionInfo: Module (%s) invalid, returning ...\r\n", pMod->lpszModName));
            hRes = E_FAIL;
            goto Exit;
        }

        BasePtr = (LPBYTE) pMod->BasePtr;
        pe32l = &pMod->e32;
    }

    if (pe32l != NULL && pe32l->e32_unit[RES].rva != NULL && BasePtr != NULL)
    {
        // Get the address of the start of the resources.
        // NOTE this code is different from the same code in GetDebugInfo.
        // Apparently for e32_unit[RES], the rva field got fixed up with the actual virtual address.
        // For e32_unit[DBG], the rva field is unfixed up and still needs +BasePtr;
        o32rva = reinterpret_cast<LPBYTE>(pe32l->e32_unit[RES].rva) - BasePtr;
        lpres  = reinterpret_cast<LPBYTE>(pe32l->e32_unit[RES].rva);

        if (lpres)
        {
            __try
            {
                void *VerHeadVa;
                VerHeadVa = (void *)ExtractOneResource(pProc, lpres, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, o32rva, &dwTemp);
                if (VerHeadVa)
                {
                    DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Read VERHEAD @%08X\r\n", VerHeadVa));
                    size_t bytesread = OsAxsReadMemory(pVerHead, pProc, VerHeadVa, sizeof(VerHeadBuf));
                    if (bytesread >= sizeof(*pVerHead))
                    {
                        if (((DWORD)pVerHead->wTotLen > dwTemp) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE))
                        {
                            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Bad Version resource data\r\n"));
                            hRes = E_FAIL;
                            goto Exit;
                        }
                        else
                        {
                            void *pFixedInfo;
                            void * const pEndOfStruct = reinterpret_cast<BYTE*>(VerHeadVa) + pVerHead->wTotLen;
                            void * const pEndOfString = reinterpret_cast<BYTE*>(VerHeadVa) + offsetof(VERHEAD, szKey) + wcslen(pVerHead->szKey);

                            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Ver Len=%u, Ver Key=%s\r\n", pVerHead->wTotLen, pVerHead->szKey));
                            lpStart = (LPWSTR)((LPBYTE)VerHeadVa+DWORDUP((sizeof(VERBLOCK)-sizeof(WCHAR))+(wcslen(pVerHead->szKey)+1)*sizeof(WCHAR)));

                            if (reinterpret_cast<void *>(lpStart) < pEndOfStruct)
                            {
                                DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Start is before end of struct @%08X\r\n", pEndOfStruct));
                                pFixedInfo = lpStart;
                            }
                            else
                            {
                                DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Start after end of struct @%08X, change start to %08X\r\n",
                                                           pEndOfStruct, pEndOfString));
                                pFixedInfo = pEndOfString;
                            }

                            DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Read FIXEDINFO @%08X\r\n", pFixedInfo));
                            bytesread = OsAxsReadMemory(pvsFixedInfo, pProc, pFixedInfo, sizeof(*pvsFixedInfo));
                            if (bytesread == sizeof(*pvsFixedInfo))
                            {
                                fVersionInfoFound = TRUE;
                            }
                        }
                    }
                    else
                    {
                        DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: didn't read enough bytes for VERHEAD (%d)\r\n",
                                                    bytesread));
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                DBGRETAILMSG(OXZONE_CV_INFO, (L"  MemCommon!GetVersionInfo: Access Violation in ExtractOneResource!! lpres=0x%08X, o32rva=0x%08X\r\n", lpres, o32rva));
                hRes = E_FAIL;
                goto Exit;
            }
        }
        else
        {
            DEBUGGERMSG(OXZONE_CV_INFO, (L"  MemCommon!GetVersionInfo: Resource table not found.\r\n"));
        }
          
    }

    if (!fVersionInfoFound)
    {
        DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: No Version Info\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }


    DEBUGGERMSG(OXZONE_CV_INFO,(L"  MemCommon!GetVersionInfo: Version found %u,%u,%u,%u\r\n",
                                pvsFixedInfo->dwFileVersionMS >> 16,
                                pvsFixedInfo->dwFileVersionMS & 0xFFFF,
                                pvsFixedInfo->dwFileVersionLS >> 16,
                                pvsFixedInfo->dwFileVersionLS & 0xFFFF));

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_CV_INFO,(L"--MemCommon!GetVersionInfo: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

