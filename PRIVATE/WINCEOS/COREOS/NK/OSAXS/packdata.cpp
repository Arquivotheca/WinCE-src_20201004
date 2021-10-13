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

    PackData.cpp

Abstract:

    This is the device side implementation of PackData.  The point of this
    file is to take a process / thread / module structure and render it down
    into a nice structure that the caller can use.  These functions are also
    used to make porting to host side easier.

--*/

#include "osaxs_p.h"


HRESULT PackProcess (PROCESS *pProc, OSAXS_PORTABLE_PROCESS *pPortProc)
{
    HRESULT hr;
    static ModuleSignature ms;

    DEBUGGERMSG(OXZONE_PACKDATA, (L"++PackProcess @0x%08x\r\n", pProc));
    
    hr = S_OK;
    memset (pPortProc, 0, sizeof (*pPortProc));

    pPortProc->bSlot = pProc->procnum;
    if (pProc->lpszProcName)
    {
        __try
        {
            WideToAnsi (pProc->lpszProcName, pPortProc->szName, CCH_PROCNAME);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy (pPortProc->szName, "Unknown");
        }
    }
    else
        strcpy (pPortProc->szName, "Unknown");
    pPortProc->dwVMBase = pProc->dwVMBase;
    pPortProc->dwAccessKey = pProc->aky;
    pPortProc->bTrustLevel = pProc->bTrustLevel;
    pPortProc->dwHandle = (DWORD) pProc->hProc;
    pPortProc->dwBasePtr = (DWORD) pProc->BasePtr;
    pPortProc->dwTlsUseLow = pProc->tlsLowUsed;
    pPortProc->dwTlsUseHigh = pProc->tlsHighUsed;
    if (pProc->ZonePtr)
    {
        __try
        {
            pPortProc->dwZoneMask = pProc->ZonePtr->ulZoneMask;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pPortProc->dwZoneMask = 0;
        }
    }
    else
        pPortProc->dwZoneMask = 0;

    pPortProc->dwAddr = (DWORD) pProc;
    if (pProc->pcmdline)
    {
        __try
        {
            WideToAnsi (pProc->pcmdline, pPortProc->szCommandLine, CCH_CMDLINE);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy (pPortProc->szCommandLine, "Unknown");
        }
    }
    else
        strcpy (pPortProc->szCommandLine, "Unknown");

    DEBUGGERMSG(OXZONE_PACKDATA, (L"--PackProcess: hr=0x%08x", hr));
    return hr;
}


HRESULT PackModule (PROCESS *pProc, OSAXS_PORTABLE_MODULE *pPortMod)
{
    static ModuleSignature ms;

    DEBUGGERMSG(OXZONE_PACKDATA, (L"++PackModule @0x%08x (process)\r\n", pProc));
    
    memset(pPortMod, 0, sizeof (*pPortMod));
    __try
    {
        WideToAnsi (pProc->lpszProcName, pPortMod->szModuleName, CCH_MODULENAME);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        strcpy (pPortMod->szModuleName, "Unknown");
    }

    pPortMod->dwBasePointer = ((DWORD) pProc->BasePtr == 0x10000)?
        (DWORD) pProc->BasePtr | (DWORD) pProc->dwVMBase :
        (DWORD) pProc->BasePtr;
    pPortMod->dwModuleSize = pProc->e32.e32_vsize;

    if (!pProc->procnum && g_OsaxsData.pRomHdr && g_OsaxsData.pRomHdr->ulCopyOffset)
    {
        COPYentry *pcopyEntry = (COPYentry *) g_OsaxsData.pRomHdr->ulCopyOffset;
        pPortMod->dwRdWrDataStart = pcopyEntry->ulDest;
        pPortMod->dwRdWrDataEnd = pcopyEntry->ulDest + pcopyEntry->ulDestLen - 1;
    }
    else
    {
        pPortMod->dwRdWrDataStart = 0;
        pPortMod->dwRdWrDataEnd = 0;
    }

    pPortMod->dwTimeStamp = pProc->e32.e32_timestamp;
    if (SUCCEEDED (GetProcessDebugInfo (pProc, &ms)))
    {
        pPortMod->dwPdbFormat = ms.dwType;
        pPortMod->PdbGuid = ms.guid;
        pPortMod->dwPdbAge = ms.dwAge;
    }
    pPortMod->dwDllHandle = 0;
    pPortMod->dwInUse = 1 << pProc->procnum;
    pPortMod->wFlags = 0;
    pPortMod->bTrustLevel = pProc->bTrustLevel;
    pPortMod->rgwRefCount[pProc->procnum] = 1;
    pPortMod->dwStructAddr = reinterpret_cast<DWORD>(pProc);

    DBGRETAILMSG (OXZONE_PACKDATA, (L"--PackModule (process)\r\n"));
    
    return S_OK;
}


HRESULT PackModule (MODULE *pMod, OSAXS_PORTABLE_MODULE *pPortMod)
{
    HRESULT hr;
    static ModuleSignature ms;

    DEBUGGERMSG(OXZONE_PACKDATA, (L"++PackModule @0x%08x (module)\r\n", pMod));

    if (pMod == pMod->lpSelf)
    {
        DEBUGGERMSG(OXZONE_PACKDATA, (L"  PackModule module validated\r\n"));
        
        memset(pPortMod, 0, sizeof(*pPortMod));
        
        __try
        {
            WideToAnsi (pMod->lpszModName, pPortMod->szModuleName, CCH_MODULENAME);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy (pPortMod->szModuleName, "Unknown");
        }
        pPortMod->dwBasePointer = ZeroPtr (pMod->BasePtr);
        pPortMod->dwModuleSize = pMod->e32.e32_vsize;
        pPortMod->dwRdWrDataStart = pMod->rwLow;
        pPortMod->dwRdWrDataEnd = pMod->rwHigh;
        pPortMod->dwTimeStamp = pMod->e32.e32_timestamp;
        hr = GetModuleDebugInfo (pMod, &ms);
        if (SUCCEEDED (hr))
        {
            pPortMod->dwPdbFormat = ms.dwType;
            pPortMod->PdbGuid = ms.guid;
            pPortMod->dwPdbAge = ms.dwAge;
        }
        pPortMod->dwDllHandle = (DWORD) pMod;
        pPortMod->dwInUse = pMod->inuse;
        pPortMod->wFlags = pMod->wFlags;
        memcpy (pPortMod->rgwRefCount, pMod->refcnt, sizeof (pMod->refcnt));

        pPortMod->dwStructAddr = reinterpret_cast<DWORD>(pMod);
        
        hr = S_OK;
    }
    else
    {
        DBGRETAILMSG (1, (L"  PackModule (module) Not a module.  lpSelf test failed.\r\n"));
        hr = E_FAIL;
    }

    DBGRETAILMSG (OXZONE_PACKDATA || FAILED (hr), (L"--PackModule (module) hr = 0x%08x\r\n", hr));
    return hr;
}

#define THREAD_STATUS_MASK ((1<<DYING_SHIFT)|(1<<DEAD_SHIFT)|(1<<BURIED_SHIFT)|(1<<SLEEPING_SHIFT))
#define THREAD_INFO_MASK ((1<<TIMEMODE_SHIFT)|(1<<STACKFAULT_SHIFT)|(1<<USERBLOCK_SHIFT)|(1<<PROFILE_SHIFT))
HRESULT PackThread(THREAD *pThread, OSAXS_PORTABLE_THREAD *pPortThread)
{
    HRESULT hr = S_OK;

    pPortThread->dwAddr = (DWORD)pThread;
    pPortThread->wRunState = ((pThread->wInfo & THREAD_STATUS_MASK) << 2) | (1 << GET_RUNSTATE(pThread));
    pPortThread->wInfo = pThread->wInfo & THREAD_INFO_MASK;
    pPortThread->dwHandle = (DWORD)pThread->hTh;
    pPortThread->bWaitState = pThread->bWaitState;
    pPortThread->dwAccessKey = pThread->aky;
    pPortThread->dwCurProcHandle = (DWORD)pThread->pProc->hProc;
    pPortThread->dwOwnProcHandle = (DWORD)pThread->pOwnerProc->hProc;
    pPortThread->bCurPrio = pThread->bCPrio;
    pPortThread->bBasePrio = pThread->bBPrio;
    pPortThread->dwKernelTime = pThread->dwKernTime;
    pPortThread->dwUserTime = pThread->dwUserTime;
    pPortThread->dwQuantum = pThread->dwQuantum;
    pPortThread->dwQuantumLeft = pThread->dwQuantLeft;
    pPortThread->dwSleepCount = pThread->dwWakeupTime;
    pPortThread->bSuspendCount = pThread->bSuspendCnt;
    pPortThread->dwTlsPtr = (DWORD)pThread->tlsPtr;
    pPortThread->dwLastError = pThread->dwLastError;
    pPortThread->dwStackBase = pThread->tlsPtr[PRETLS_STACKBASE];
    pPortThread->dwStackLowBound = pThread->tlsPtr[PRETLS_STACKBOUND];
    pPortThread->dwCreationTimeHi = pThread->ftCreate.dwHighDateTime;
    pPortThread->dwCreationTimeLo = pThread->ftCreate.dwLowDateTime;
    pPortThread->dwCurrentPC = GetThreadProgramCounter(pThread);

    return hr;
}


HRESULT PackThreadContext (PTHREAD pThread, void *pvCtx, DWORD *pcbCtx)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_PACKDATA, (L"++PackThreadContext: pTh=0x%08x, cb=%d\r\n", pThread, *pcbCtx));

    if (
#if defined (ARM)
            *pcbCtx <= sizeof (OsAxsArmIntContext)
#elif defined (MIPS)
#   ifdef _MIPS64
            *pcbCtx <= sizeof (OsAxsMips64IntContext)
#   else
            *pcbCtx <= sizeof (OsAxsMips32IntContext)
#   endif
#elif defined (SHx)
            *pcbCtx <= sizeof (OsAxsShxIntContext)
#elif defined (x86)
            *pcbCtx <= sizeof (OsAxsX86IntContext)
#else
#error "Unknown CPU type"
#endif
       )
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED (hr))
    {
#if defined (ARM)
        OsAxsArmIntContext *pCtx = reinterpret_cast <OsAxsArmIntContext *> (pvCtx);
        *pcbCtx = sizeof (OsAxsArmIntContext);

        pCtx->Psr = pThread->ctx.Psr;
        pCtx->R[0] = pThread->ctx.R0;
        pCtx->R[1] = pThread->ctx.R1;
        pCtx->R[2] = pThread->ctx.R2;
        pCtx->R[3] = pThread->ctx.R3;
        pCtx->R[4] = pThread->ctx.R4;
        pCtx->R[5] = pThread->ctx.R5;
        pCtx->R[6] = pThread->ctx.R6;
        pCtx->R[7] = pThread->ctx.R7;
        pCtx->R[8] = pThread->ctx.R8;
        pCtx->R[9] = pThread->ctx.R9;
        pCtx->R[10] = pThread->ctx.R10;
        pCtx->R[11] = pThread->ctx.R11;
        pCtx->R[12] = pThread->ctx.R12;
        pCtx->Sp = pThread->ctx.Sp;
        pCtx->Lr = pThread->ctx.Lr;
        pCtx->Pc = pThread->ctx.Pc;

#elif defined (MIPS)
#   ifdef _MIPS64
        OsAxsMips64IntContext *pCtx = reinterpret_cast <OsAxsMips64IntContext *> (pvCtx);
        *pcbCtx = sizeof (OsAxsMips64IntContext);
#   else
        OsAxsMips32IntContext *pCtx = reinterpret_cast <OsAxsMips32IntContext *> (pvCtx);
        *pcbCtx = sizeof (OsAxsMips32IntContext);
#   endif

        pCtx->IntAt = pThread->ctx.IntAt;

        pCtx->IntV0 = pThread->ctx.IntV0;
        pCtx->IntV1 = pThread->ctx.IntV1;

        pCtx->IntA0 = pThread->ctx.IntA0;
        pCtx->IntA1 = pThread->ctx.IntA1;
        pCtx->IntA2 = pThread->ctx.IntA2;
        pCtx->IntA3 = pThread->ctx.IntA3;

        pCtx->IntT0 = pThread->ctx.IntT0;
        pCtx->IntT1 = pThread->ctx.IntT1;
        pCtx->IntT2 = pThread->ctx.IntT2;
        pCtx->IntT3 = pThread->ctx.IntT3;
        pCtx->IntT4 = pThread->ctx.IntT4;
        pCtx->IntT5 = pThread->ctx.IntT5;
        pCtx->IntT6 = pThread->ctx.IntT6;
        pCtx->IntT7 = pThread->ctx.IntT7;

        pCtx->IntS0 = pThread->ctx.IntS0;
        pCtx->IntS1 = pThread->ctx.IntS1;
        pCtx->IntS2 = pThread->ctx.IntS2;
        pCtx->IntS3 = pThread->ctx.IntS3;
        pCtx->IntS4 = pThread->ctx.IntS4;
        pCtx->IntS5 = pThread->ctx.IntS5;
        pCtx->IntS6 = pThread->ctx.IntS6;
        pCtx->IntS7 = pThread->ctx.IntS7;

        pCtx->IntT8 = pThread->ctx.IntT8;
        pCtx->IntT9 = pThread->ctx.IntT9;

        pCtx->IntK0 = pThread->ctx.IntK0;
        pCtx->IntK1 = pThread->ctx.IntK1;

        pCtx->IntGp = pThread->ctx.IntGp;
        pCtx->IntSp = pThread->ctx.IntSp;

        pCtx->IntS8 = pThread->ctx.IntS8;

        pCtx->IntRa = pThread->ctx.IntRa;

        pCtx->IntLo = pThread->ctx.IntLo;
        pCtx->IntHi = pThread->ctx.IntHi;

        pCtx->IntFir = pThread->ctx.Fir;
        pCtx->IntPsr = pThread->ctx.Psr;

#elif defined (SHx)
        OsAxsShxIntContext *pCtx = reinterpret_cast <OsAxsShxIntContext *> (pvCtx);
        *pcbCtx = sizeof (OsAxsShxIntContext);
        
        pCtx->PR = pThread->ctx.PR;
        pCtx->MACH = pThread->ctx.MACH;
        pCtx->MACL = pThread->ctx.MACL;
        pCtx->GBR = pThread->ctx.GBR;
        pCtx->R[0] = pThread->ctx.R0;
        pCtx->R[1] = pThread->ctx.R1;
        pCtx->R[2] = pThread->ctx.R2;
        pCtx->R[3] = pThread->ctx.R3;
        pCtx->R[4] = pThread->ctx.R4;
        pCtx->R[5] = pThread->ctx.R5;
        pCtx->R[6] = pThread->ctx.R6;
        pCtx->R[7] = pThread->ctx.R7;
        pCtx->R[8] = pThread->ctx.R8;
        pCtx->R[9] = pThread->ctx.R9;
        pCtx->R[10] = pThread->ctx.R10;
        pCtx->R[11] = pThread->ctx.R11;
        pCtx->R[12] = pThread->ctx.R12;
        pCtx->R[13] = pThread->ctx.R13;
        pCtx->R[14] = pThread->ctx.R14;
        pCtx->R[15] = pThread->ctx.R15;
        pCtx->Fir = pThread->ctx.Fir;
        pCtx->Psr = pThread->ctx.Psr;

#elif defined (x86)
        OsAxsX86IntContext *pCtx = reinterpret_cast <OsAxsX86IntContext *> (pvCtx);
        *pcbCtx = sizeof (OsAxsX86IntContext);

        pCtx->Gs = pThread->ctx.TcxGs;
        pCtx->Fs = pThread->ctx.TcxFs;
        pCtx->Es = pThread->ctx.TcxEs;
        pCtx->Ds = pThread->ctx.TcxDs;
        pCtx->Edi = pThread->ctx.TcxEdi;
        pCtx->Esi = pThread->ctx.TcxEsi;
        pCtx->Ebx = pThread->ctx.TcxEbx;
        pCtx->Edx = pThread->ctx.TcxEdx;
        pCtx->Ecx = pThread->ctx.TcxEcx;
        pCtx->Eax = pThread->ctx.TcxEax;
        pCtx->Ebp = pThread->ctx.TcxEbp;
        pCtx->Eip = pThread->ctx.TcxEip;
        pCtx->Cs = pThread->ctx.TcxCs;
        pCtx->Eflags = pThread->ctx.TcxEFlags;
        pCtx->Esp = pThread->ctx.TcxEsp;
        pCtx->Ss = pThread->ctx.TcxSs;

#else
#   error "Unknown CPU type"
#endif
    }

    DEBUGGERMSG (OXZONE_PACKDATA || FAILED (hr), (L"--PackThreadContext: hr=0x%08x, cb=%d\r\n", hr, *pcbCtx));
    return hr;
}


HRESULT UnpackThreadContext (PTHREAD pThread, void *pvCtx, DWORD cbCtx)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_PACKDATA, (L"++UnpackThreadContext: pTh=0x%08x, cb=%d\r\n", pThread, cbCtx));

    if (
#if defined (ARM)
            cbCtx != sizeof (OsAxsArmIntContext)
#elif defined (MIPS)
#   ifdef _MIPS64
            cbCtx != sizeof (OsAxsMips64IntContext)
#   else
            cbCtx != sizeof (OsAxsMips32IntContext)
#   endif
#elif defined (SHx)
            cbCtx != sizeof (OsAxsShxIntContext)
#elif defined (x86)
            cbCtx != sizeof (OsAxsX86IntContext)
#else
#   error "Unknown CPU type"
#endif
       )
    {
        hr = E_FAIL;
    }

    if (SUCCEEDED (hr))
    {
#if defined (ARM)
        OsAxsArmIntContext *pCtx = reinterpret_cast <OsAxsArmIntContext *> (pvCtx);

        pThread->ctx.Psr = pCtx->Psr;
        pThread->ctx.R0 = pCtx->R[0];
        pThread->ctx.R1 = pCtx->R[1];
        pThread->ctx.R2 = pCtx->R[2];
        pThread->ctx.R3 = pCtx->R[3];
        pThread->ctx.R4 = pCtx->R[4];
        pThread->ctx.R5 = pCtx->R[5];
        pThread->ctx.R6 = pCtx->R[6];
        pThread->ctx.R7 = pCtx->R[7];
        pThread->ctx.R8 = pCtx->R[8];
        pThread->ctx.R9 = pCtx->R[9];
        pThread->ctx.R10 = pCtx->R[10];
        pThread->ctx.R11 = pCtx->R[11];
        pThread->ctx.R12 = pCtx->R[12];
        pThread->ctx.Sp = pCtx->Sp;
        pThread->ctx.Lr = pCtx->Lr;
        pThread->ctx.Pc = pCtx->Pc;

#elif defined (MIPS)
#   ifdef _MIPS64
        OsAxsMips64IntContext *pCtx = reinterpret_cast <OsAxsMips64IntContext *> (pvCtx);
#   else
        OsAxsMips32IntContext *pCtx = reinterpret_cast <OsAxsMips32IntContext *> (pvCtx);
#   endif

        pThread->ctx.IntAt = pCtx->IntAt;

        pThread->ctx.IntV0 = pCtx->IntV0;
        pThread->ctx.IntV1 = pCtx->IntV1;

        pThread->ctx.IntA0 = pCtx->IntA0;
        pThread->ctx.IntA1 = pCtx->IntA1;
        pThread->ctx.IntA2 = pCtx->IntA2;
        pThread->ctx.IntA3 = pCtx->IntA3;

        pThread->ctx.IntT0 = pCtx->IntT0;
        pThread->ctx.IntT1 = pCtx->IntT1;
        pThread->ctx.IntT2 = pCtx->IntT2;
        pThread->ctx.IntT3 = pCtx->IntT3;
        pThread->ctx.IntT4 = pCtx->IntT4;
        pThread->ctx.IntT5 = pCtx->IntT5;
        pThread->ctx.IntT6 = pCtx->IntT6;
        pThread->ctx.IntT7 = pCtx->IntT7;

        pThread->ctx.IntS0 = pCtx->IntS0;
        pThread->ctx.IntS1 = pCtx->IntS1;
        pThread->ctx.IntS2 = pCtx->IntS2;
        pThread->ctx.IntS3 = pCtx->IntS3;
        pThread->ctx.IntS4 = pCtx->IntS4;
        pThread->ctx.IntS5 = pCtx->IntS5;
        pThread->ctx.IntS6 = pCtx->IntS6;
        pThread->ctx.IntS7 = pCtx->IntS7;

        pThread->ctx.IntT8 = pCtx->IntT8;
        pThread->ctx.IntT9 = pCtx->IntT9;

        pThread->ctx.IntK0 = pCtx->IntK0;
        pThread->ctx.IntK1 = pCtx->IntK1;

        pThread->ctx.IntGp = pCtx->IntGp;
        pThread->ctx.IntSp = pCtx->IntSp;

        pThread->ctx.IntS8 = pCtx->IntS8;

        pThread->ctx.IntRa = pCtx->IntRa;

        pThread->ctx.IntLo = pCtx->IntLo;
        pThread->ctx.IntHi = pCtx->IntHi;

        pThread->ctx.Fir = pCtx->IntFir;
        pThread->ctx.Psr = pCtx->IntPsr;

#elif defined (SHx)
        OsAxsShxIntContext *pCtx = reinterpret_cast <OsAxsShxIntContext *> (pvCtx);
        
        pThread->ctx.PR = pCtx->PR;
        pThread->ctx.MACH = pCtx->MACH;
        pThread->ctx.MACL = pCtx->MACL;
        pThread->ctx.GBR = pCtx->GBR;
        pThread->ctx.R0 = pCtx->R[0];
        pThread->ctx.R1 = pCtx->R[1];
        pThread->ctx.R2 = pCtx->R[2];
        pThread->ctx.R3 = pCtx->R[3];
        pThread->ctx.R4 = pCtx->R[4];
        pThread->ctx.R5 = pCtx->R[5];
        pThread->ctx.R6 = pCtx->R[6];
        pThread->ctx.R7 = pCtx->R[7];
        pThread->ctx.R8 = pCtx->R[8];
        pThread->ctx.R9 = pCtx->R[9];
        pThread->ctx.R10 = pCtx->R[10];
        pThread->ctx.R11 = pCtx->R[11];
        pThread->ctx.R12 = pCtx->R[12];
        pThread->ctx.R13 = pCtx->R[13];
        pThread->ctx.R14 = pCtx->R[14];
        pThread->ctx.R15 = pCtx->R[15];
        pThread->ctx.Fir = pCtx->Fir;
        pThread->ctx.Psr = pCtx->Psr;

#elif defined (x86)
        OsAxsX86IntContext *pCtx = reinterpret_cast <OsAxsX86IntContext *> (pvCtx);

        pThread->ctx.TcxGs = pCtx->Gs;
        pThread->ctx.TcxFs = pCtx->Fs;
        pThread->ctx.TcxEs = pCtx->Es;
        pThread->ctx.TcxDs = pCtx->Ds;
        pThread->ctx.TcxEdi = pCtx->Edi;
        pThread->ctx.TcxEsi = pCtx->Esi;
        pThread->ctx.TcxEbx = pCtx->Ebx;
        pThread->ctx.TcxEdx = pCtx->Edx;
        pThread->ctx.TcxEcx = pCtx->Ecx;
        pThread->ctx.TcxEax = pCtx->Eax;
        pThread->ctx.TcxEbp = pCtx->Ebp;
        pThread->ctx.TcxEip = pCtx->Eip;
        pThread->ctx.TcxCs = pCtx->Cs;
        pThread->ctx.TcxEFlags = pCtx->Eflags;
        pThread->ctx.TcxEsp = pCtx->Esp;
        pThread->ctx.TcxSs = pCtx->Ss;

#else
#   error "Unknown CPU type"
#endif
    }

    DEBUGGERMSG (OXZONE_PACKDATA, (L"--UnpackThreadContext: hr=0x%08x\r\n", hr));
    return hr;
}


HRESULT GetStruct (OSAXS_GETOSSTRUCT *pStruct, void *pvDestination, DWORD *pcb)
{
    HRESULT hr = E_FAIL;

    switch (pStruct->dwStructType)
    {
        case OSAXS_STRUCT_MODULE:
            if (*pcb >= sizeof (OSAXS_PORTABLE_MODULE))
            {
                *pcb = sizeof (OSAXS_PORTABLE_MODULE);
                if (pStruct->StructAddr)
                {
                    if (pStruct->StructAddr >= (DWORD) &ProcArray[0] && pStruct->StructAddr < (DWORD) &ProcArray[MAX_PROCESSES])
                        hr = PackModule ((PROCESS *) pStruct->StructAddr, (OSAXS_PORTABLE_MODULE *) pvDestination);
                    else
                        hr = PackModule ((MODULE *) pStruct->StructAddr, (OSAXS_PORTABLE_MODULE *) pvDestination);
                }
                else if (pStruct->dwStructHandle)
                {
                    hr = PackModule ((MODULE *) pStruct->dwStructHandle, (OSAXS_PORTABLE_MODULE *) pvDestination);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                *pcb = 0;
            }
            break;

        case OSAXS_STRUCT_PROCESS:
            if (*pcb >= sizeof(OSAXS_PORTABLE_PROCESS))
            {
                *pcb = sizeof(OSAXS_PORTABLE_PROCESS);
                if (pStruct->StructAddr)
                {
                    hr = PackProcess(reinterpret_cast<PROCESS*>(pStruct->StructAddr),
                                     reinterpret_cast<OSAXS_PORTABLE_PROCESS*>(pvDestination));
                }
                else if (pStruct->dwStructHandle)
                {
                    DWORD iProc;
                    hr = E_FAIL;
                    for (iProc = 0; iProc < MAX_PROCESSES; iProc++)
                    {
                        if (ProcArray[iProc].dwVMBase &&
                              reinterpret_cast<DWORD>(ProcArray[iProc].hProc) == pStruct->dwStructHandle)
                        {
                            hr = PackProcess(&ProcArray[iProc],
                                             reinterpret_cast<OSAXS_PORTABLE_PROCESS*>(pvDestination));
                            break;
                        }
                    }
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                *pcb = 0;
            }
            break;

        case OSAXS_STRUCT_THREAD:
            if (*pcb >= sizeof (OSAXS_PORTABLE_THREAD))
            {
                *pcb = sizeof (OSAXS_PORTABLE_THREAD);
                if (pStruct->StructAddr)
                {
                    hr = PackThread (reinterpret_cast <THREAD *> (pStruct->StructAddr),
                            reinterpret_cast <OSAXS_PORTABLE_THREAD *> (pvDestination));
                }
                else if (pStruct->dwStructHandle)
                {
                    hr = E_FAIL;
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
                *pcb = 0;
            }
            break;

        default:
            hr = E_FAIL;
            break;
    }
    return hr;
}


HRESULT GetModuleO32Data (DWORD dwAddrModule, DWORD *pdwNumO32Data, void *pvResult, DWORD *pcbResult)
{
    HRESULT hr = S_OK;
    o32_lite *o32_ptr = NULL;
    DWORD co32 = 0;

    DEBUGGERMSG (OXZONE_PACKDATA, (L"++GetModuleO32Data: 0x%08x\r\n", dwAddrModule));
    __try
    {
        if ((dwAddrModule >= reinterpret_cast <DWORD> (&g_OsaxsData.pProcArray [0])) && 
                (dwAddrModule < reinterpret_cast <DWORD> (&g_OsaxsData.pProcArray [MAX_PROCESSES])))
        {
            PROCESS *pProcess = reinterpret_cast <PROCESS *> (dwAddrModule);
            PMODULE pCoreDll = (PMODULE)hCoreDll;
            DWORD dwInUse = (1 << pProcess->procnum);

            if (!pCoreDll || !(dwInUse & pCoreDll->inuse))
            {
                DBGRETAILMSG (1, (L"  GetModuleO32Data: Bad process struct @0x%08x\r\n", pProcess));
                hr = E_FAIL;
            }
            else
            {
                co32 = pProcess->e32.e32_objcnt;
                o32_ptr = pProcess->o32_ptr;
                DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: Process data.\r\n"));
            }
        }
        else
        {
            Module *pMod = reinterpret_cast <MODULE *> (dwAddrModule);
            if (pMod && pMod->lpSelf == pMod)
            {
                if (pMod->inuse)
                {
                    co32 = pMod->e32.e32_objcnt;
                    o32_ptr = pMod->o32_ptr;

                    DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: Module data.\r\n"));
                }
                else
                {
                    DBGRETAILMSG (1, (L"  GetModuleO32Data: Module is not being used, inuse=0\r\n"));
                    hr = E_FAIL;
                }
            }
            else
            {
                DEBUGGERMSG (1, (L"  GetModuleO32Data: Not a module 0x%08x\r\n", dwAddrModule));
                hr = E_FAIL;
            }
        }

        if (SUCCEEDED (hr))
        {
            // copy out the o32_ptr data.
            size_t cbO32Data = sizeof (o32_lite) * co32;

            DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: num o32_lite=%d, addr o32_lite=0x%08x\r\n", co32, o32_ptr));

            *pdwNumO32Data = co32;
            if (cbO32Data)
            {
                if (*pcbResult >= cbO32Data)
                {
                    if (o32_ptr)
                    {
                        SafeMoveMemory (pCurProc, pvResult, o32_ptr, cbO32Data);
                    }
                    else
                    {
                        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetModuleO32Data: o32_ptr is null?\r\n"));
                        cbO32Data = 0;
                        *pdwNumO32Data = 0;
                    }
                }
                else
                {
                    DBGRETAILMSG (1, (L"  GetModuleO32Data: Not enough room in the result buffer!\r\n"));
                    hr = E_OUTOFMEMORY;
                }
            }

            if (SUCCEEDED (hr))
            {
                *pcbResult = cbO32Data;
            }
            else
            {
                *pcbResult = 0;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DBGRETAILMSG (1, (L"  GetModuleO32Data: Unexpected exception.\r\n"));
        hr = E_FAIL;
    }
    DBGRETAILMSG (OXZONE_PACKDATA || FAILED (hr), (L"--GetModuleO32Data: hr=0x%08x, pmod=0x%08x\r\n", hr, dwAddrModule));
    return hr;
}

#if defined(x86)
PVOID __inline GetRegistrationHead(void)
{
    __asm mov eax, dword ptr fs:[0];
}

HRESULT GetExceptionRegistration(DWORD *pdwBuff)
{
    DEBUGGERMSG (OXZONE_PACKDATA, (L"++GetExceptionRegistration\r\n"));
    HRESULT hr = E_FAIL;

    // This is only valid for x86.  It is used by the debugger for unwinding through exception handlers.
    if (pdwBuff)
    {
        CALLSTACK* pStack = NULL;
        PVOID pRegistration = NULL;
        PVOID pvCallStack = pdwBuff + sizeof(DWORD);

        memcpy(&pStack, pvCallStack, sizeof(VOID*));
        if (pStack)
        {
            // We are at a PSL boundary and need to look up the next registration
            // pointer -- don't trust the pointer we got from the host
            pRegistration = (VOID*)pStack->extra;
            pStack = pStack->pcstkNext;
        }
        else
        {
            // Request for the registration head pointer
            pRegistration = GetRegistrationHead();
            pStack = pCurThread->pcstkTop;
        }

        memcpy(pdwBuff, &pRegistration, sizeof(VOID*));
        memcpy(pvCallStack, &pStack, sizeof(VOID*));
        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetExceptionRegistration:  Registration Ptr: %8.8lx pCallStack: %8.8lx\r\n", (DWORD)pRegistration, (DWORD)pStack));
        hr = S_OK;
    }
    else
    {
        DEBUGGERMSG (OXZONE_PACKDATA, (L"  GetExceptionRegistration:  Invalid parameter\r\n", hr));
    }

    DEBUGGERMSG (OXZONE_PACKDATA, (L"--GetExceptionRegistration:  hr=0x%08x\r\n", hr));
    return hr;
}
#endif

