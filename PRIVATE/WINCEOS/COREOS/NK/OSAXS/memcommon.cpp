//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

void *MapPtrInProc (void *pv, PROCESS *pProc)
{
    if ((DWORD)pv >> VA_SECTION)
        return pv;
    else
        return (void *)((DWORD)FetchProcStruct (pProc)->dwVMBase | (DWORD)pv);
}

static HRESULT ReadNB10Record( PROCESS *pProc, DWORD dwAddr, ModuleSignature *pms )
{
    HRESULT hr = S_OK;
    static DWORD dwOffset;
    DWORD dwRead;

    dwOffset = 0;
    dwRead = SafeMoveMemory( pProc, &dwOffset, (void *)dwAddr, sizeof( dwOffset ));
    if( dwRead != sizeof( dwOffset ) || dwOffset != 0 )
    {
        DEBUGGERMSG( OXZONE_ALERT, (L"!!ReadNB10Record: Unreadable/Bad offset\r\n") );
        hr = E_FAIL;
    }

    if( SUCCEEDED( hr ))
    {
        // NB10 has a 4-byte timestamp.  Zero fill the guid and cram the time stamp into the first 4 bytes.
        dwAddr += sizeof( dwOffset );
        memset( &pms->guid, 0, sizeof( pms->guid ));
        dwRead = SafeMoveMemory( pProc, &pms->guid, (void *)dwAddr, sizeof( DWORD ));
        if( dwRead != sizeof( DWORD ))
        {
            DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadNB10Record: Unable to read NB10 signature.\r\n" ));
            hr = E_FAIL;
        }
    }

    if( SUCCEEDED( hr ))
    {
        dwAddr += sizeof( DWORD );
        dwRead = SafeMoveMemory( pProc, &pms->dwAge, (void *)dwAddr, sizeof( pms->dwAge ));
        if( dwRead != sizeof( pms->dwAge ))
        {
            DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadNB10Record: Unable to read NB10 Age.\r\n" ));
            hr = E_FAIL;
        }
    }

    return hr;
}

static HRESULT ReadRSDSRecord( PROCESS *pProc, DWORD dwAddr, ModuleSignature *pms )
{
    HRESULT hr = S_OK;
    DWORD dwRead;

    dwRead = SafeMoveMemory( pProc, &pms->guid, (void *)dwAddr, sizeof( pms->guid ));
    if( dwRead != sizeof( pms->guid ))
    {
        DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadRSDSRecord: Unable to read GUID\r\n" ));
        hr = E_FAIL;
    }

    if( SUCCEEDED( hr ))
    {
        dwAddr += sizeof( pms->guid );
        dwRead = SafeMoveMemory( pProc, &pms->dwAge, (void *)dwAddr, sizeof( pms->dwAge ));
        if( dwRead != sizeof( pms->dwAge ))
        {
            DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadRSDSRecord: Unable to read Age\r\n" ));
            hr = E_FAIL;
        }
    }
    return hr;
}

static HRESULT ReadCVRecord( PROCESS *pProc, DWORD dwDbgRecAddr, ModuleSignature *pms )
{
    HRESULT hr = S_OK;
    DWORD dwRead;

    dwRead = SafeMoveMemory( pProc, &pms->dwType, (void *)dwDbgRecAddr, sizeof( pms->dwType ));
    if( dwRead == sizeof( pms->dwType ))
    {
        dwDbgRecAddr += sizeof( pms->dwType );
        switch( pms->dwType )
        {
        case '01BN':
            hr = ReadNB10Record( pProc, dwDbgRecAddr, pms );
            break;

        case 'SDSR':
            hr = ReadRSDSRecord( pProc, dwDbgRecAddr, pms );
            break;

        default:
            DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadCVRecord: Invalid type code.\r\n" ));
            hr = E_FAIL;
            break;
        }
    }
    else
    {
        DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadCVRecord: Unable to read type code.\r\n" ));
        hr = E_FAIL;
    }
    return hr;
}


static HRESULT ReadDebugDirectory( PROCESS *pProc, DWORD dwDbgDirAddr, o32_lite *o32_ptr, ModuleSignature *pms )
{
    HRESULT hr = S_OK;
    static IMAGE_DEBUG_DIRECTORY idd;
    DWORD dwRead;

    DEBUGGERMSG(OXZONE_CV_INFO, (L"++ReadDebugDirectory: in pProc=0x%08x, DbgDirAddr=0x%08x, o32ptr=0x%08x\r\n", pProc, dwDbgDirAddr, o32_ptr));

    memset( &idd, 0, sizeof( idd ));
    dwRead = SafeMoveMemory( pProc, &idd, (void *)dwDbgDirAddr, sizeof( idd ));
    if( dwRead != sizeof( idd ))
    {
        DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadDebugDirectory: Unable to read debug directory for process %d\r\n", pProc? pProc->procnum : -1 ));
        hr = E_FAIL;
    }

    if( SUCCEEDED( hr ) && idd.Type != IMAGE_DEBUG_TYPE_CODEVIEW )
    {
        DEBUGGERMSG( OXZONE_ALERT, ( L"!!ReadDebugDirectory: Debug directory is not CV type.\r\n" ));
        hr = E_FAIL;
    }

    if( SUCCEEDED( hr ))
    {
        DWORD dwDebugRecordAddress;
        
        if( o32_ptr )
        {
            // Computing the address of the CV record for Modules and Processes
            dwDebugRecordAddress = idd.AddressOfRawData + o32_ptr->o32_realaddr - o32_ptr->o32_rva;

            // Processes need to have VMBase added in.  (Unlike Dll's.)
            if( pProc )
                dwDebugRecordAddress += pProc->dwVMBase;
        }
        else
        {
            // Handling NK.EXE case (which is perfectly fixed up.)
            dwDebugRecordAddress = (DWORD)pProc->BasePtr + idd.AddressOfRawData;
        }

        hr = ReadCVRecord( pProc, dwDebugRecordAddress, pms );
    }

    DEBUGGERMSG(OXZONE_CV_INFO, (L"--ReadDebugDirectory: hr=0x%08x\r\n", hr));

    return hr;
}

HRESULT GetProcessDebugInfo( PROCESS *pProc, ModuleSignature *pms )
{
    HRESULT hr = E_FAIL;
    DWORD dwDebugDirRva = (DWORD)pProc->e32.e32_unit[E32_EXTRA_DBGDIR_RVA].rva;
    DWORD dwDbgDirAddr = 0;
    DWORD iO32;

    DEBUGGERMSG(OXZONE_CV_INFO, (L"++GetProcessDebugInfo: pProc=0x%08x, dwDbgDirRva=0x%08x\r\n", pProc, dwDebugDirRva));

    memset(pms, 0, sizeof(*pms));

    PMODULE pCoreDll = (PMODULE)hCoreDll;
    DWORD dwInUse = (1 << pProc->procnum);

    if (!pCoreDll || !(dwInUse & pCoreDll->inuse))
    {
        // When process is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
        DEBUGGERMSG(OXZONE_ALERT, (L"  GetProcessDebugInfo: Process ID 0x%08X (%s) not using CoreDll.dll, may be in startup or shutdown.\r\n", pProc->hProc, pProc->lpszProcName));
        goto Exit;
    }

    if (pProc->procnum == 0)
    {
        // Handling NK.EXE case (which is perfectly fixed up.)
        dwDbgDirAddr = (DWORD)pProc->BasePtr + dwDebugDirRva; 
        hr = ReadDebugDirectory( pProc, dwDbgDirAddr, NULL, pms );
    }
    else
    {
        // Looping to find the right section of the exe that holds the Debug Directory's RVA
        for( iO32 = 0; iO32 < pProc->e32.e32_objcnt; iO32++ )
        {
            __try
            {
                if( pProc->o32_ptr && pProc->o32_ptr[iO32].o32_rva == dwDebugDirRva )
                {
                    dwDbgDirAddr = pProc->dwVMBase + pProc->o32_ptr[iO32].o32_realaddr;
                    hr = ReadDebugDirectory( pProc, dwDbgDirAddr, &pProc->o32_ptr[iO32], pms );
                    break;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                DBGRETAILMSG(OXZONE_ALERT, (L"  GetProcessDebugInfo: Access Violation!! pProc->o32_ptr=0x%08X, iO32=%u\r\n", pProc->o32_ptr, iO32));
                hr = E_FAIL;
                break;
            }
        }
    }

Exit:
    
    DEBUGGERMSG(OXZONE_CV_INFO, (L"--GetProcessDebugInfo: hr=0x%08X\r\n", hr));

    return hr;
}

HRESULT GetModuleDebugInfo( MODULE *pMod, ModuleSignature *pms )
{
    HRESULT hr = E_FAIL;
    DWORD dwDebugDirRva = (DWORD)pMod->e32.e32_unit[E32_EXTRA_DBGDIR_RVA].rva;
    DWORD dwDbgDirAddr = 0;
    DWORD iO32;

    DEBUGGERMSG( OXZONE_CV_INFO, ( L"++GetModuleDebugInfo: lpszModName = %s\r\n", pMod->lpszModName));

    if (!pMod->inuse)
    {
        hr = E_FAIL;
        DEBUGGERMSG( OXZONE_CV_INFO, ( L"  GetModuleDebugInfo: Inuse flag for module not set, returning ...\r\n"));
        goto Exit;
    }

    memset(pms, 0, sizeof(*pms));

    // Loop through module sections to find the section that holds the Debug Directory information.
    for( iO32 = 0; iO32 < pMod->e32.e32_objcnt; iO32++ )
    {
        if( pMod->o32_ptr && pMod->o32_ptr[iO32].o32_rva == dwDebugDirRva )
        {
            dwDbgDirAddr = pMod->o32_ptr[iO32].o32_realaddr;
            hr = ReadDebugDirectory( NULL, dwDbgDirAddr, &pMod->o32_ptr[iO32], pms );
            if( FAILED( hr ))
            {
                DEBUGGERMSG( OXZONE_ALERT, ( L"!!GetModuleDebugInfo: ReadDebugDirectory failed, hr = 0x%08X\r\n", hr ));
            }
            break;
        }
    }

Exit:

    DEBUGGERMSG( OXZONE_CV_INFO, ( L"--GetModuleDebugInfo: hr = 0x%08X\r\n", hr));
    
    return hr;
}


