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

    flexptmi.cpp

Abstract:

    Implement the Flexi-PTMI interface for Osaxs

Environment:

    OsaxsH / OsaxsT

--*/


#include "osaxs_p.h"
#include <dwcedump.h>

// Reasonably large values to cope with normal cases and detect kernel list errors
enum
{
    MAX_THREADS_HANDLED   = (4096),
    MAX_PROCESSES_HANDLED = (1024),
    MAX_MODULES_HANDLED   = (1024)
};

// NOTE on format strings for field descriptors
// the printf format is supported except the following:
// -Exceptions:
//      -no I64 in the prefix
//      -no * for width
//      -no * for precision
// -Additions:
//      -%T{N=BitFieldNameN, M=BitFieldNameM...} for bitfield description
//          where bitnumbers (N and M) are in [0..63] and BitFieldNameN and BitFieldNameM are strings of char with no ","
//          if bitnumber in [0..31], the BitfieldName will be display for bitnumber == 1
//          if bitnumber in [32..63], the BitfieldName will be display for bitnumber == 0
//          Any non described bit will be ignored
//          Will display all set bitfield separated by a ,
//      -%N{N=EnumElementNameN, M=EnumElementNameM...} for enumeration description
//          where N and M are decimal DWORD value and EnumElementNameN and EnumElementNameM are strings of char with no ","
//          Any non described enum value will be ignored


/********** Descriptors */
BEGIN_FIELD_INFO_LIST (ProcessDesc)
    FIELD_INFO (pfiProcessSlot,      sizeof (BYTE),   L"ProcSlot#",   L"%u")      //1
    FIELD_INFO (pfiName,             CCH_PROCNAME,    L"Name",        L"%s")      //33
    FIELD_INFO (pfiHandle,           sizeof (HANDLE), L"hProcess",    L"0x%08lX") //37
    FIELD_INFO (pfiBasePtr,          sizeof (LPVOID), L"BasePtr",     L"0x%08lX") //41
    FIELD_INFO (pfiTlsUsageBitMaskH, sizeof (DWORD),  L"TlsUseH32b",  L"0x%08lX") //45
    FIELD_INFO (pfiTlsUsageBitMaskL, sizeof (DWORD),  L"TlsUseL32b",  L"0x%08lX") //49
    FIELD_INFO (pfiCurDbgZoneMasks,  sizeof (ULONG),  L"CurZoneMask", L"0x%08lX") //53
    FIELD_INFO (pfiStructAddr,       sizeof (LPVOID), L"pProcess",    L"0x%08lX") //57
    FIELD_INFO (pfiCmdLine,          CCH_CMDLINE,     L"CmdLine",     L"%s")      //185
END_FIELD_INFO_LIST ();

#pragma pack (push, 1)
struct FlexiProcess
{
    BYTE bSlot;
    char szName [CCH_PROCNAME];
    DWORD dwHandle;
    DWORD dwBasePtr;
    DWORD dwTlsUseHigh;
    DWORD dwTlsUseLow;
    DWORD dwZoneMask;
    DWORD dwAddr;
    char szCommandLine [CCH_CMDLINE];
};
#pragma pack (pop)

BEGIN_FIELD_INFO_LIST (ThreadDesc)
    FIELD_INFO (tfiStructAddr,                sizeof (LPVOID),    L"pThread",      L"0x%08lX")    //4
    FIELD_INFO (tfiRunState,                  sizeof (WORD),      L"RunState",     L"%T{4=Dying,5=Dead,6=Buried,7=Slpg,39=Awak,0=Rung,1=Runab,2=RunBlkd,3=RunNeeds}") //6
    FIELD_INFO (tfiInfo,                      sizeof (WORD),      L"InfoStatus",   L"%T{38=UMode,6=KMode,8=StkFlt,12=UsrBlkd,15=Profd}") //8
    FIELD_INFO (tfiHandle,                    sizeof (HANDLE),    L"hThread",      L"0x%08lX")    //12
    FIELD_INFO (tfiWaitState,                 sizeof (BYTE),      L"WaitState",    L"%N{0=Signalled,1=Processing,2=Blocked}") //13
    FIELD_INFO (tfiHandleCurrentProcessRunIn, sizeof (HANDLE),    L"hVMProc",   L"0x%08lX")    //17
    FIELD_INFO (tfiHandleOwnerProc,           sizeof (HANDLE),    L"hOwnerProc",   L"0x%08lX")    //21
    FIELD_INFO (tfiCurrentPriority,           sizeof (BYTE),      L"CurPrio",      L"%u")         //22
    FIELD_INFO (tfiBasePriority,              sizeof (BYTE),      L"BasePrio",     L"%u")         //23
    FIELD_INFO (tfiKernelTime,                sizeof (DWORD),     L"KernelTime",   L"%lu")        //27
    FIELD_INFO (tfiUserTime,                  sizeof (DWORD),     L"UserTime",     L"%lu")        //31
    FIELD_INFO (tfiQuantum,                   sizeof (DWORD),     L"Quantum",      L"%lu")        //35
    FIELD_INFO (tfiQuantumLeft,               sizeof (DWORD),     L"QuantumLeft",  L"%lu")       //39
    FIELD_INFO (tfiSleepCount,                sizeof (DWORD),     L"SleepCount",   L"%lu")        //43
    FIELD_INFO (tfiSuspendCount,              sizeof (BYTE),      L"SuspendCount", L"%u")         //44
    FIELD_INFO (tfiTlsPtr,                    sizeof (LPDWORD),   L"TlsPtr",       L"0x%08lX")    //48
    FIELD_INFO (tfiLastError,                 sizeof (DWORD),     L"LastError",    L"0x%08lX")    //52
    FIELD_INFO (tfiStackBase,                 sizeof (DWORD),     L"StackBase",    L"0x%08lX")    //56
    FIELD_INFO (tfiStackLowBound,             sizeof (DWORD),     L"StkLowBnd",    L"0x%08lX")    //60
    FIELD_INFO (tfiPC,                        sizeof (DWORD),     L"PC",           L"0x%08lX")    //64
    FIELD_INFO (tfiStkRetAddr,                sizeof (DWORD),     L"StkRetAddr",   L"0x%08lX")    //68
    FIELD_INFO (tfiHandleActiveProcess,       sizeof (HANDLE),    L"hActvProc",    L"0x%08lX")    //72
    FIELD_INFO (tfiStartAddr,                    sizeof (DWORD),    L"StartAddr",      L"0x%08lX")    //12
END_FIELD_INFO_LIST();

#pragma pack (push, 1)
struct FlexiThread
{
    DWORD dwAddr;
    WORD wRunState;
    WORD wInfo;
    DWORD dwHandle;
    BYTE bWaitState;
    DWORD dwVMProcHandle;
    DWORD dwOwnProcHandle;
    BYTE bCurPrio;
    BYTE bBasePrio;
    DWORD dwKernelTime;
    DWORD dwUserTime;
    DWORD dwQuantum;
    DWORD dwQuantumLeft;
    DWORD dwSleepCount;
    BYTE bSuspendCount;
    DWORD dwTlsPtr;
    DWORD dwLastError;
    DWORD dwStackBase;
    DWORD dwStackLowBound;
    DWORD dwCurrentPC;
    DWORD dwStkRetAddr;
    DWORD dwActvProcHandle;
    DWORD dwStartAddr;
};
#pragma pack (pop)

BEGIN_FIELD_INFO_LIST (ModuleDesc)
    FIELD_INFO (mfiName,             CCH_MODULENAME,    L"Name",             L"%s")            //32
    FIELD_INFO (mfiBasePointer,      sizeof(DWORD),     L"Base Ptr",         L"0x%08lX")       //36
    FIELD_INFO (mfiModuleSize,       sizeof(DWORD),     L"Size",             L"%lu")           //40
    FIELD_INFO (mfiRdWrDataStart,    sizeof(DWORD),     L"RW Data Start",    L"0x%08lX")       //44
    FIELD_INFO (mfiRdWrDataEnd,      sizeof(DWORD),     L"RW Data End",      L"0x%08lX")       //48
    FIELD_INFO (mfiTimeStamp,        sizeof(DWORD),     L"Timestamp",        L"0x%08lX")       //52
    FIELD_INFO (mfiPdbFormat,        sizeof(DWORD),     L"PDB Format",       L"%N{808534606=NB10,1396986706=RSDS}")           //56
    FIELD_INFO (mfiPdbGuid,          sizeof(GUID),      L"PDB Guid",         L"%U")            //72
    FIELD_INFO (mfiPdbAge,           sizeof(DWORD),     L"PDB Age",          L"%lu")           //76
    FIELD_INFO (mfiDllHandle,        sizeof(DWORD),     L"hDll",             L"0x%08lX")       //80
    FIELD_INFO (mfiInUse,            sizeof(DWORD),     L"In Use",           L"0x%08lX")       //84
    FIELD_INFO (mfiFlags,            sizeof(WORD),      L"Flags",            L"%T{0=NoDllRef,1=Data,3=SearchPath,15=Kernel}") //86
    FIELD_INFO (mfiStructAddr,       sizeof(DWORD),     L"pMod",             L"0x%08kX")       //90
    FIELD_INFO (mfiFileVersionMS,    sizeof(DWORD),     L"FileVersionMS",    L"0x%08lX")       //94
    FIELD_INFO (mfiFileVersionLS,    sizeof(DWORD),     L"FileVersionLS",    L"0x%08lX")       //98
    FIELD_INFO (mfiProductVersionMS, sizeof(DWORD),     L"ProductVersionMS", L"0x%08lX")       //102
    FIELD_INFO (mfiProductVersionLS, sizeof(DWORD),     L"ProductVersionLS", L"0x%08lX")       //106
    FIELD_INFO (mfiPdbName,          CCH_PDBNAME,       L"PDB Name",         L"%s")            //138
END_FIELD_INFO_LIST ();

#pragma pack (push, 1)
struct FlexiModule
{
    char szModuleName [CCH_MODULENAME];
    DWORD dwBasePointer;
    DWORD dwModuleSize;
    DWORD dwRdWrDataStart;
    DWORD dwRdWrDataEnd;
    DWORD dwTimeStamp;
    DWORD dwPdbFormat;
    GUID PdbGuid;
    DWORD dwPdbAge;
    DWORD dwDllHandle;
    DWORD dwInUse;
    WORD wFlags;
    DWORD dwAddr;
    DWORD dwFileVersionMS;
    DWORD dwFileVersionLS;
    DWORD dwProductVersionMS;
    DWORD dwProductVersionLS;
    char szPdbName [CCH_PDBNAME];
};
#pragma pack (pop)

BEGIN_FIELD_INFO_LIST(ModProcDesc)
    FIELD_INFO(mpfiProcessID, sizeof(DWORD), L"Process ID", L"%08lX")
    FIELD_INFO(mpfiModuleID, sizeof(DWORD), L"Module ID", L"%08lX")
    FIELD_INFO(mpfiRefCount, sizeof(DWORD), L"Module Ref Count", L"%lu")
END_FIELD_INFO_LIST();

#pragma pack(push, 1)
struct FlexiModuleProcess
{
    DWORD ProcessID;
    DWORD ModuleID;
    DWORD ModRefCnt;
};
#pragma pack(pop)

/*++

Routine Name:

    WriteFieldInfo

Routine Description:

    Dump field information to the output function

Arguments:

    pRequest - User request
    pbBuf    - Beginning of response buffer
    piBuf    - Ptr to index of current position in response buffer.
    cbBuf    - Total size of the response buffer.

Return values:

    S_OK : success,
    E_FAIL : general failure,
    E_OUTOFMEMORY : response buffer not big enough

--*/
HRESULT WriteFieldInfo (FLEXI_FIELD_INFO *pFields, const DWORD cFields, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    DWORD i;
    DWORD pNextStr;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WriteFieldInfo: FieldCnt:%d, Offset:%d\r\n",
        cFields, riOut));

    pNextStr = riOut + CB_FIELDINFO * cFields;
    DEBUGGERMSG (OXZONE_FLEXI, (L"  WriteFieldInfo: Strings start at: %d\r\n", pNextStr));
    
    for (i = 0; SUCCEEDED (hr) && i < cFields; i++)
    {
        hr = Write((DWORD)pFields[i].ul32Id, riOut, cbOut, pbOut);
        if (SUCCEEDED (hr))
            hr = Write((DWORD)pFields[i].ul32Size, riOut, cbOut, pbOut);
        if (SUCCEEDED (hr))
        {
            hr = Write(pNextStr, riOut, cbOut, pbOut);
            if (SUCCEEDED (hr))
                pNextStr += sizeof(DWORD) + (wcslen(pFields[i].szLabel) + 1) * sizeof(WCHAR);
        }
        if (SUCCEEDED (hr))
        {
            hr = Write(pNextStr, riOut, cbOut, pbOut);
            if (SUCCEEDED (hr))
                pNextStr += sizeof(DWORD) + (wcslen(pFields[i].szFormat) + 1) * sizeof(WCHAR);
        }
    }

    if (FAILED(hr))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  WriteFieldInfo: Failed to write structs: 0x%.08x\r\n", hr));
    }

    if (SUCCEEDED(hr))
    {
        for (i = 0; SUCCEEDED (hr) && i < cFields; i++)
        {
            DWORD sizeLabel = wcslen(pFields[i].szLabel) * sizeof(WCHAR);
            hr = Write(sizeLabel, riOut, cbOut, pbOut);
            if (SUCCEEDED(hr))
                hr = Write(pFields[i].szLabel, 
                    sizeLabel + (sizeof (WCHAR) * 1),
                    riOut, cbOut, pbOut);

            DWORD sizeFormat = wcslen(pFields[i].szFormat) * sizeof(WCHAR);
            if (SUCCEEDED(hr))
                hr = Write(sizeFormat, riOut, cbOut, pbOut);
            if (SUCCEEDED (hr))
                hr = Write(pFields[i].szFormat,
                    sizeFormat + (sizeof (WCHAR) * 1),
                    riOut, cbOut, pbOut);
        }
    }
    if (FAILED(hr))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  WriteFieldInfo: Failed to write strings: 0x%.08x\r\n", hr));
    }
        
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--WriteFieldInfo: 0x%.08x, Offset:%d\r\n", hr, riOut));
    return hr;
}


//
// Flexi Process Data
//
static HRESULT WriteProcessData (PROCESS *pProc, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    FlexiProcess proc = {0};
    HRESULT hr;

    if (pbOut)
    {
        proc.bSlot = pProc->bASID;

        if (pProc->lpszProcName)
        {
            DEBUGGERMSG (OXZONE_FLEXI, (L"  WriteProcessData: Proc Name Addr:0x%.08x\r\n",
                pProc->lpszProcName));
            __try
            {
                WideToAnsi (pProc->lpszProcName, proc.szName, CCH_PROCNAME);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                strcpy (proc.szName, "Unknown");
            }
        }
        else
            strcpy (proc.szName, "Unknown");
        DEBUGGERMSG (OXZONE_FLEXI, (L"  WriteProcessData: Name = %a\r\n", proc.szName));

        proc.dwHandle = pProc->dwId;
        proc.dwBasePtr = reinterpret_cast <DWORD> (pProc->BasePtr);
        proc.dwTlsUseLow = pProc->tlsLowUsed;
        proc.dwTlsUseHigh = pProc->tlsHighUsed;
        if (pProc->ZonePtr)
        {
            DBGPARAM dp;
            DBGRETAILMSG (OXZONE_MEM, (L"##WriteProcessData: Calling OsAxsReadMemory(%08X:%08X) to read DBGPARAM\r\n", pProc, pProc->ZonePtr));
            if (OsAxsReadMemory (&dp, pProc, pProc->ZonePtr, sizeof (DBGPARAM)) == sizeof (DBGPARAM))
            {
                proc.dwZoneMask = dp.ulZoneMask;
            }
            else
            {
                DEBUGGERMSG (OXZONE_ALERT, (L"  WriteProcessData: Failed to read proc 0x%08x ZonePtr 0x%08x\r\n", pProc, pProc->ZonePtr));        
            }
            
        }
        proc.dwAddr = reinterpret_cast <DWORD> (pProc);
        strcpy (proc.szCommandLine, "not available");
        hr = Write (&proc, sizeof (proc), riOut, cbOut, pbOut);
    }
    else
    {
        riOut += sizeof (proc);
        hr = S_OK;
    }
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--WriteProcessData: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalOneProcess (FLEXI_REQUEST const *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalOneProcess\r\n"));

    if (pRequest->dwRequest & FLEXI_FILTER_PROCESS_POINTER)
    {
        PPROCESS pProcess = g_pprcNK;
        PPROCESS pReqProcess = (PROCESS *) pRequest->dwHProc;

        hr = E_FAIL;
        do
        {
            if (pProcess == pReqProcess)
            {
                hr = WriteProcessData(pReqProcess, riOut, cbOut, pbOut);
                break;
            }

            pProcess = (PROCESS *)pProcess->prclist.pFwd;
        }
        while (pProcess != g_pprcNK);

        if (FAILED (hr))
        {
            DBGRETAILMSG (OXZONE_ALERT, (L"!!MarshalOneProcess: BAD PROCESS ADDRESS 0x%08x\r\n", pProcess));
            hr = E_FAIL;
        }
    }
    else
        hr = E_FAIL;

    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--MarshalOneProcess: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalProcessData (FLEXI_REQUEST const *pRequest, DWORD &riOut, 
    const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalProcessData: Offset:%d\r\n", riOut));

    if (pRequest->Proc.dwStart > pRequest->Proc.dwEnd)
        hr = E_INVALIDARG;

    DWORD cProcessElements = 0;
    PROCESS *pProcess = g_pprcNK;
    DWORD iProc = 0;
    
    while (SUCCEEDED (hr) && pProcess && (cProcessElements < MAX_PROCESSES_HANDLED) && (iProc <= pRequest->Proc.dwEnd))
    {
        if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) ||                 // Include all processes if filter on proc handle not set.
            (pProcess->dwId == pRequest->dwHProc) ||  // Otherwise only include requested process
            ((pRequest->dwRequest & FLEXI_FILTER_INC_PROC) &&               // & additional process if requested
             (pProcess->dwId == pRequest->dwHMod)) ||
            ((pRequest->dwRequest & FLEXI_FILTER_INC_NK) &&                 // & nk.exe if requested
             (pProcess == g_pprcNK)))
        {
            // Should we be calling OSAXSIsProcessValid here?
            if (iProc >= pRequest->Proc.dwStart)
            {
                hr = WriteProcessData (pProcess, riOut, cbOut, pbOut);
                if (SUCCEEDED(hr))
                    cProcessElements++;
            } 
            iProc++;
        } 

        pProcess = (PROCESS *)pProcess->prclist.pFwd;
        if (pProcess == g_pprcNK)
            break;
    }
    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cProcessElements : 0;

    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--MarshalProcessData: 0x%.08x, Cnt:%d, Offset:%d\r\n",
        hr, cProcessElements, riOut));
    return hr;
}


enum
{
    THREAD_STATUS_MASK = ((1<<DYING_SHIFT)|(1<<DEAD_SHIFT)|(1<<BURIED_SHIFT)|(1<<SLEEPING_SHIFT)),
    THREAD_INFO_MASK  = ((1<<TIMEMODE_SHIFT)|(1<<STACKFAULT_SHIFT)|(1<<USERBLOCK_SHIFT)|(1<<PROFILE_SHIFT)),
};
static HRESULT WriteThreadData (const THREAD *pThread, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    FlexiThread thrd = {0};
    HRESULT hr;
    DWORD       dwQuantum;
    BYTE        bCPrio;
    BYTE        bBPrio;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WriteThreadData: Thread=0x%.08x, Offset=%d\r\n", pThread, riOut));

    if (pbOut)
    {
        __try
        {
            bCPrio = pThread->bCPrio;
            bBPrio = pThread->bBPrio;
            dwQuantum = pThread->dwQuantum;

            thrd.dwAddr = reinterpret_cast <DWORD> (pThread);
            thrd.wRunState = ((pThread->wInfo & THREAD_STATUS_MASK) << 2) | (1 << GET_RUNSTATE (pThread));
            thrd.wInfo = static_cast <WORD> (pThread->wInfo & THREAD_INFO_MASK);
            thrd.dwHandle = pThread->dwId;
            thrd.dwStartAddr = pThread->dwStartAddr;
            thrd.bWaitState = pThread->bWaitState;
            if (pThread->pprcVM)
                thrd.dwVMProcHandle = pThread->pprcVM->dwId;
            if (pThread->pprcActv)
                thrd.dwActvProcHandle = pThread->pprcActv->dwId;
            thrd.dwOwnProcHandle = pThread->pprcOwner->dwId;
            thrd.bCurPrio = bCPrio;
            thrd.bBasePrio = bBPrio;
            thrd.dwKernelTime = pThread->dwKTime;
            thrd.dwUserTime = pThread->dwUTime;
            thrd.dwQuantum = dwQuantum;
            thrd.dwQuantumLeft = pThread->dwQuantLeft;
            thrd.dwSleepCount = pThread->dwWakeupTime;
            thrd.bSuspendCount = pThread->bSuspendCnt;
            thrd.dwTlsPtr = reinterpret_cast <DWORD> (pThread->tlsPtr);
            thrd.dwLastError = pThread->dwLastError;
            DBGRETAILMSG (OXZONE_MEM, (L"##WriteThreadData: Calling OsAxsReadMemory(%08X:%08X) to read pThread->tlsPtr[PRETLS_STACKBASE]\r\n",
                              pThread->pprcOwner, &pThread->tlsPtr [PRETLS_STACKBASE]));
            if (OsAxsReadMemory (&thrd.dwStackBase, pThread->pprcOwner, &pThread->tlsPtr [PRETLS_STACKBASE], sizeof (thrd.dwStackBase)) != sizeof (thrd.dwStackBase))
            {
                thrd.dwStackBase = 0xDEADBEEF;
            }
            
            DBGRETAILMSG (OXZONE_MEM, (L"##WriteThreadData: Calling OsAxsReadMemory(%08X:%08X) to read pThread->tlsPtr[PRETLS_STACKBOUND]\r\n",
                              pThread->pprcOwner, &pThread->tlsPtr [PRETLS_STACKBOUND]));
            if (OsAxsReadMemory (&thrd.dwStackLowBound, pThread->pprcOwner, &pThread->tlsPtr [PRETLS_STACKBOUND], sizeof (thrd.dwStackLowBound)) != sizeof (thrd.dwStackLowBound))
            {
                thrd.dwStackLowBound = 0xDEADBEEF;
            }
            
            thrd.dwCurrentPC = GetThreadProgramCounter (pThread);
            thrd.dwStkRetAddr = (DWORD) GetThreadRetAddr (pThread); 
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            DEBUGGERMSG (OXZONE_ALERT, (L"  WriteThreadData: INVALID THREAD!!! AV while accessing thread=0x%.08x, Offset=%d\r\n", pThread, riOut));
        }

        hr = Write (&thrd, sizeof (thrd), riOut, cbOut, pbOut);
    }
    else
    {
        riOut += sizeof (thrd);
        hr = S_OK;
    }
        
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--WriteThreadData: 0x%.08x\r\n", hr));
    return hr;
}


static HRESULT MarshalOneThread (FLEXI_REQUEST const *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalOneThread\r\n"));

    if (pRequest->dwRequest & FLEXI_FILTER_THREAD_POINTER)
    {
        PTHREAD pThrd = reinterpret_cast <PTHREAD> (pRequest->dwHThread);
        hr = WriteThreadData (pThrd, riOut, cbOut, pbOut);
    }
    else
        hr = E_FAIL;

    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--MarshalOneThread: hr = 0x%08x\r\n", hr));
    return hr;
}


/*++

Routine Name:
    
    MarshalThreadData

Routine Description:

    Dump all threads based on the ranges and handles provided by the request structure.

Arguments:

    pRequest   - Request structure
    riOut      - in/out Current offset within the response buffer.
    cbOut      - Size of the response buffer
    pbOut      - Pointer to the response buffer
    pcElements - Returns the count of elements that have been output.

Return Value:

    S_OK: Success,
    E_FAIL: General failure,
    E_OUTOFMEMORY: No more space in the buffer.
    
--*/
static HRESULT MarshalThreadData (FLEXI_REQUEST const *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut, 
    DWORD *pcElements)
{
    HRESULT hr = S_OK;
    DWORD cThreadElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalThreadData: Offset:%d\r\n", riOut));

    if ((pRequest->Proc.dwStart > pRequest->Proc.dwEnd) || (pRequest->Thread.dwStart > pRequest->Thread.dwEnd))
        hr = E_INVALIDARG;
    
    // Process index into g_pprcNK linked list.
    DWORD iProc = 0;
    // Count of processes returned.
    DWORD ProcCount = 0;
    // Thread index into all threads on the device.
    DWORD iThread = 0;
    PROCESS *pProcess = g_pprcNK;

    while (SUCCEEDED (hr) && (ProcCount < MAX_PROCESSES_HANDLED) && (iProc <= pRequest->Proc.dwEnd))
    {
        if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) || (pProcess->dwId == pRequest->dwHProc))
        {
            if (iProc >= pRequest->Proc.dwStart)
            {
                DEBUGGERMSG (OXZONE_FLEXI, (L"  MarshalThreadData: Getting threads for proc 0x%08x, %s\r\n", pProcess,
                    pProcess->lpszProcName));
                    
                PTHREAD pThread = (PTHREAD) pProcess->thrdList.pFwd;
                while (SUCCEEDED (hr) && pThread && (iThread <= pRequest->Thread.dwEnd) && 
                    (iThread < MAX_THREADS_HANDLED))
                {
                    if (!(pRequest->dwRequest & FLEXI_FILTER_THREAD_HANDLE) ||
                        (pThread->dwId == pRequest->dwHThread))
                    {
                        if (iThread >= pRequest->Thread.dwStart)
                        {
                            hr = WriteThreadData (pThread, riOut, cbOut, pbOut);
                            if (SUCCEEDED (hr))
                                cThreadElements++;
                        }
                        iThread++;
                    }
                    pThread = (PTHREAD) pThread->thLink.pFwd;
                    if (pThread == (PTHREAD) &pProcess->thrdList)
                        break;
                }
                ProcCount++;
            }
            iProc++;
        }

        pProcess = (PROCESS *) pProcess->prclist.pFwd;
        if (pProcess == g_pprcNK)
            break;
    }
    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cThreadElements : 0;
        
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--MarshalThreadData: 0x%.08x, Cnt:%d, Offset:%d\r\n",
        hr, cThreadElements, riOut));
    return hr;
}

static HRESULT WriteProcessDataAsModule (PROCESS *pProcess, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    FlexiModule mod = {0};
    VS_FIXEDFILEINFO vsFixedInfo = {0};
    HRESULT hr;

    DEBUGGERMSG(OXZONE_FLEXI, (L"++WriteProcessDataAsModule\r\n"));

    if (pbOut)
    {
        __try
        {
            WideToAnsi (pProcess->lpszProcName, mod.szModuleName, CCH_MODULENAME);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy (mod.szModuleName, "Unknown");
        }
        DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalOneProcessAsModule: Module Name = %a\r\n", mod.szModuleName));

        mod.dwBasePointer = reinterpret_cast <DWORD> (pProcess->BasePtr);
        mod.dwModuleSize = pProcess->e32.e32_vsize;
        if (pProcess == g_pprcNK)
        {
            DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalOneProcessAsModule: Handling NK RD/WR section\r\n"));
            const COPYentry *pce = reinterpret_cast<COPYentry*>(pTOC->ulCopyOffset);
            if (pce)
            {
                mod.dwRdWrDataStart = pce->ulDest;
                mod.dwRdWrDataEnd = pce->ulDest + pce->ulDestLen - 1;
            }
        }
        mod.dwTimeStamp = pProcess->e32.e32_timestamp;

        static ModuleSignature ms;

        // Ignore failure from GetDebugInfo, since some processes may have no debug info
        if (SUCCEEDED (GetDebugInfo (pProcess, FALSE, &ms)) )
        {
            mod.dwPdbFormat = ms.dwType;
            mod.PdbGuid = ms.guid;
            mod.dwPdbAge = ms.dwAge;
            memcpy(mod.szPdbName, ms.szPdbName, CCH_PDBNAME);
        }

        // HACK: Update dwInUse to be the system refcount for this module
        mod.dwInUse = 1;

        mod.dwAddr = reinterpret_cast <DWORD> (pProcess);

        if (SUCCEEDED(GetVersionInfo(pProcess, FALSE, &vsFixedInfo)))
        {
            mod.dwFileVersionMS = vsFixedInfo.dwFileVersionMS;
            mod.dwFileVersionLS = vsFixedInfo.dwFileVersionLS;
            mod.dwProductVersionMS = vsFixedInfo.dwProductVersionMS;
            mod.dwProductVersionLS = vsFixedInfo.dwProductVersionLS;
        }
        else
        {
            mod.dwFileVersionMS = (pProcess->e32.e32_cevermajor << 16) | (pProcess->e32.e32_ceverminor & 0xFFFF);
            mod.dwFileVersionLS = 0;
            mod.dwProductVersionMS = (pProcess->e32.e32_cevermajor << 16) | (pProcess->e32.e32_ceverminor & 0xFFFF);
            mod.dwProductVersionLS = 0;
        }

        mod.dwDllHandle = pProcess->dwId;

        hr = Write (&mod, sizeof (mod), riOut, cbOut, pbOut);
    }
    else
    {
        riOut += sizeof (mod);
        hr = S_OK;
    }
    DEBUGGERMSG(OXZONE_FLEXI, (L"--WriteProcessDataAsModule: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalProcessesAsModules (FLEXI_REQUEST const *pRequest, DWORD &rdwIdx, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;
    DWORD cModElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalProcessesAsModules: Idx:%d, Offset:%d\r\n", rdwIdx, riOut));
    
    if (pRequest->Mod.dwStart > pRequest->Mod.dwEnd)
        hr = E_INVALIDARG;

    if (!(pRequest->dwRequest & FLEXI_FILTER_MODULE_HANDLE))
    { // Processes don't have a module handle.

        PROCESS *pProcess = g_pprcNK;
        while (SUCCEEDED (hr) && (cModElements < MAX_MODULES_HANDLED) && (rdwIdx <= pRequest->Mod.dwEnd))
        {
            // Make sure process is not busy starting or dying (Check if using CoreDll.dll)
            // Except for Nk.exe, we always allow it
            if (pProcess == g_pprcNK || IsVMAllocationAllowed (pProcess))
            {
                if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) ||             // Include all processes if filter on proc handle not set.
                    (pProcess->dwId == pRequest->dwHProc) ||     // Otherwise only include request process
                    ((pRequest->dwRequest & FLEXI_FILTER_INC_PROC) &&            // & additional process if requested
                     (pProcess->dwId == pRequest->dwHMod)) ||
                    ((pRequest->dwRequest & FLEXI_FILTER_INC_NK) &&              // & nk.exe if requested
                     (pProcess == g_pprcNK)))
                { // Only output the process that matches this handle
                    if (rdwIdx >= pRequest->Mod.dwStart)
                    {
                        hr = WriteProcessDataAsModule (pProcess, riOut, cbOut, pbOut);
                        if (SUCCEEDED(hr))
                           cModElements++;
                    }
                    rdwIdx++;
                }
                else
                {
                    DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalProcessesAsModules: Proc %08X(%08X)'%s' does not match handle 0x%08x\r\n",
                                               pProcess, pProcess->dwId, pProcess->lpszProcName, pRequest->dwHProc));
                }
            }
            else
            {
                // When process is starting or dying we do not want it in the module list
                // Starting processes still have the load notification pending
                // Dying processes have already given an unload notification
                DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalProcessesAsModules: Process %08X(%08X)(%s) VM is disabled, skipping\r\n",
                                           pProcess, pProcess->dwId, pProcess->lpszProcName));
            }
            pProcess = (PROCESS *)pProcess->prclist.pFwd;
            if (pProcess == g_pprcNK)
                break;
        }
    }
    else
    {
        DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalProcessesAsModules: Skipping procs, module handle match.\r\n"));
    }

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cModElements : 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalProcessesAsModules: 0x%.08x, Idx:%d, Cnt:%d, Offset:%d\r\n",
        hr, rdwIdx, cModElements, riOut));
    return hr;
}


static HRESULT WriteModuleData (MODULE *pmod, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    FlexiModule mod = {0};
    VS_FIXEDFILEINFO vsFixedInfo = {0};
    HRESULT hr = S_OK;

    if (pmod->lpSelf == pmod)
    {
        if (pbOut)
        {
            __try
            {
                WideToAnsi (pmod->lpszModName, mod.szModuleName, CCH_MODULENAME);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                strcpy (mod.szModuleName, "Unknown");
            }
            DEBUGGERMSG (OXZONE_FLEXI, (L"  WriteModuleData: Module:%a\r\n", mod.szModuleName));

            mod.dwBasePointer = (DWORD) pmod->BasePtr;
            mod.dwModuleSize = pmod->e32.e32_vsize;

            mod.dwRdWrDataStart = 0;
            mod.dwRdWrDataEnd = 0;
            
            // TODO: Future -> Remove hard coded dll assumptions and start passing out the o32_ptr and e32
            bool Done = FALSE;
            for (DWORD O32Idx = 0; O32Idx < pmod->e32.e32_objcnt && !Done; O32Idx++)
            {
                // This is the beginning of a hack. The SH4 version of kernel.dll has a
                // KDATA directly in front of the DATA section so the plan of picking
                // the first writeable section doesn't work very well in that case.
                // So, for kernel.dll and SH4, we pick the next one. If we don't find our 
                // exact conditions we default to the previous logic.
                #if defined (SH4)
                wchar_t KernDll[] = L"kernel.dll";
                int KernDllSize = sizeof(KernDll)/sizeof(wchar_t)-1;
                DWORD NextO32;
                #endif
                
                if (pmod->o32_ptr && (pmod->o32_ptr[O32Idx].o32_flags & IMAGE_SCN_MEM_WRITE))
                {
                    
                    #if defined (SH4)
                    NextO32 = O32Idx + 1;
                    
                    if( (pmod->lpszModName) &&
                        (_wcsnicmp(KernDll, pmod->lpszModName, KernDllSize) == 0) && 
                        (NextO32 < pmod->e32.e32_objcnt) &&
                        (pmod->o32_ptr[NextO32].o32_flags & IMAGE_SCN_MEM_WRITE) )
                    {
                        mod.dwRdWrDataStart = pmod->o32_ptr[NextO32].o32_realaddr;
                        mod.dwRdWrDataEnd = pmod->o32_ptr[NextO32].o32_vsize + pmod->o32_ptr[NextO32].o32_realaddr;
                        Done = TRUE;
                    }
                    else
                    #endif
                    {
                    // Firstcd  Writeable section.  MAKE ASSUMPTION THIS IS THE DATA SECTION.
                    mod.dwRdWrDataStart = pmod->o32_ptr[O32Idx].o32_realaddr;
                    mod.dwRdWrDataEnd = pmod->o32_ptr[O32Idx].o32_vsize + pmod->o32_ptr[O32Idx].o32_realaddr;
                    Done = TRUE;
                    }
                }
            }
            
            mod.dwTimeStamp = pmod->e32.e32_timestamp;

            static ModuleSignature ms;

            // Ignore failure from GetDebugInfo, since some modules may have no debug info
            if (SUCCEEDED (GetDebugInfo(pmod, TRUE, &ms)) )
            {
                mod.dwPdbFormat = ms.dwType;
                mod.PdbGuid = ms.guid;
                mod.dwPdbAge = ms.dwAge;
                memcpy(mod.szPdbName, ms.szPdbName, CCH_PDBNAME);
            }

            mod.dwDllHandle = reinterpret_cast <DWORD> (pmod);

            // HACK: Changing this so that InUse is the NK refcount for the module
            mod.dwInUse = pmod->wRefCnt;

            mod.wFlags = pmod->wFlags;

            mod.dwAddr = reinterpret_cast <DWORD> (pmod);

            if (SUCCEEDED(GetVersionInfo(pmod, TRUE, &vsFixedInfo)))
            {
                mod.dwFileVersionMS = vsFixedInfo.dwFileVersionMS;
                mod.dwFileVersionLS = vsFixedInfo.dwFileVersionLS;
                mod.dwProductVersionMS = vsFixedInfo.dwProductVersionMS;
                mod.dwProductVersionLS =vsFixedInfo.dwProductVersionLS;
            }
            else
            {
                mod.dwFileVersionMS = (pmod->e32.e32_cevermajor << 16) | (pmod->e32.e32_ceverminor & 0xFFFF);
                mod.dwFileVersionLS = 0;
                mod.dwProductVersionMS = (pmod->e32.e32_cevermajor << 16) | (pmod->e32.e32_ceverminor & 0xFFFF);
                mod.dwProductVersionLS = 0;
            }

            hr = Write (&mod, sizeof (mod), riOut, cbOut, pbOut);
        }
        else
        {
            riOut += sizeof (mod);
            hr = S_OK;
        }
    }
    else
    {
        DBGRETAILMSG (OXZONE_ALERT, (L"!!WriteModuleData: 0x%08x is not pointing to a module.\r\n", pmod));
        hr = E_FAIL;
    }
    return hr;
}


static BOOL IsModuleLoadedByProcess (PPROCESS pProcess, PMODULE pModule)
{
    BOOL fFound = FALSE;
    if (pProcess)
    {
        PMODULELIST pmodlist = (PMODULELIST) pProcess->modList.pFwd;
        DWORD i = 0;
        while (!fFound
               && i < MAX_MODULES_HANDLED 
               && pmodlist 
               && pmodlist != (PMODULELIST) &pProcess->modList)
        {
            if (pmodlist->pMod == pModule)
            {
                fFound = TRUE;
            }
            else
            {
                pmodlist = (PMODULELIST) pmodlist->link.pFwd;
                ++i;
            }
        }
    }

    return fFound;
}


static HRESULT MarshalModules (FLEXI_REQUEST const *pRequest, DWORD &rdwIdx, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;
    PDLIST pmodlist = NULL;
    DWORD cModElements = 0;
    PPROCESS pProcessMatch = NULL;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalModules: Idx:%d, Offset:%d\r\n", rdwIdx, riOut));

    // If the user is interested in culling based on process, then find the module list that corresponds to list.
    if (pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE)
    {
        PPROCESS pprc = g_pprcNK;
        DWORD i = 0;
        do
        {
            if (pprc)
            {
                if (pprc->dwId == pRequest->dwHProc)
                {
                    pProcessMatch = pprc;
                }
                else
                {
                    pprc = (PPROCESS) pprc->prclist.pFwd;
                    ++i;
                }
            }
        }
        while (!pProcessMatch && i < MAX_PROCESSES_HANDLED && pprc != g_pprcNK);
    }

    pmodlist = g_pprcNK->modList.pFwd;
    while (SUCCEEDED (hr) && pmodlist && pmodlist != &g_pprcNK->modList
           && (rdwIdx <= pRequest->Mod.dwEnd)
           && (rdwIdx <= MAX_MODULES_HANDLED))
    {
        MODULE *pmod = ((PMODULELIST) pmodlist)->pMod;

        if (!(pmod->wFlags & LOAD_LIBRARY_AS_DATAFILE))
        {
            if (!(pRequest->dwRequest & FLEXI_FILTER_MODULE_HANDLE) || (reinterpret_cast<DWORD>(pmod) == pRequest->dwHMod))
            {
                if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) || IsModuleLoadedByProcess (pProcessMatch, pmod))
                {
                    if (rdwIdx >= pRequest->Mod.dwStart)
                    {
                        hr = WriteModuleData (pmod, riOut, cbOut, pbOut);
                        if (SUCCEEDED(hr))
                            cModElements++;
                    }
                    rdwIdx++;
                }
                else
                {
                    DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalModules: Mod 0x%.08x is not being used by proc 0x%.08x\r\n",
                                               pmod, pRequest->dwHProc));
                }
            }
            else
            {
                DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalModules: pmod 0x%.08x <> handle 0x%.08x\r\n",
                                           pmod, pRequest->dwHMod));
            }
        }
        pmodlist = pmodlist->pFwd; // Next module
    }

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cModElements : 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalModules: 0x%.08x, Idx:%d, Cnt:%d, Offset:%d\r\n",
        hr, rdwIdx, cModElements, riOut));
    return hr;
}


static HRESULT MarshalOneModule (FLEXI_REQUEST const *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalOneModule\r\n"));
    if (pRequest->dwRequest & FLEXI_FILTER_MODULE_POINTER)
    {
        void *pv = reinterpret_cast <void *> (pRequest->dwHMod);
        BOOL fProcess = FALSE;
        PPROCESS pprc = g_pprcNK;
        DWORD i = 0;

        // Determine whether the pointer in the request is a process pointer or not.
        do
        {
            if (pv == pprc)
            {
                fProcess = TRUE;
            }
            else
            {
                pprc = reinterpret_cast<PPROCESS>(pprc->prclist.pFwd);
                i++;
            }
        }
        while (!fProcess && (i < MAX_PROCESSES_HANDLED) && (pprc != g_pprcNK));

        if (fProcess)
            hr = WriteProcessDataAsModule (reinterpret_cast <PPROCESS> (pv), riOut, cbOut, pbOut);
        else
            hr = WriteModuleData (reinterpret_cast <MODULE *> (pv), riOut, cbOut, pbOut);
    }
    else
        hr = E_FAIL;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalOneModule: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalModuleData (FLEXI_REQUEST const *pRequest, DWORD &riOut, 
    const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr;
    DWORD iModule = 0;
    DWORD cProc = 0;
    DWORD cDll = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalModuleData: Offset:%d\r\n", riOut));
    hr = MarshalProcessesAsModules (pRequest, iModule, riOut, cbOut, pbOut, &cProc);
    if (SUCCEEDED (hr))
        hr = MarshalModules (pRequest, iModule, riOut, cbOut, pbOut, &cDll);

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? (cProc + cDll) : 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalModuleData: 0x%.08x, Offset:%d\r\n", hr, riOut));
    return hr;
}

static HRESULT WriteThreadRegisters(PTHREAD pThread, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr;
    CONTEXT context;

    hr = OsAxsGetThreadContext(pThread, &context, sizeof(context));

#if defined (ARM)
    if (SUCCEEDED(hr))
    {
        OsAxsArmIntContext ctx;

        ctx.Psr = context.Psr;
        ctx.R[0] = context.R0;
        ctx.R[1] = context.R1;
        ctx.R[2] = context.R2;
        ctx.R[3] = context.R3;
        ctx.R[4] = context.R4;
        ctx.R[5] = context.R5;
        ctx.R[6] = context.R6;
        ctx.R[7] = context.R7;
        ctx.R[8] = context.R8;
        ctx.R[9] = context.R9;
        ctx.R[10] = context.R10;
        ctx.R[11] = context.R11;
        ctx.R[12] = context.R12;
        ctx.Sp = context.Sp;
        ctx.Lr = context.Lr;
        ctx.Pc = context.Pc;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
    if(SUCCEEDED(hr) && (context.ContextFlags & CE_CONTEXT_FLOATING_POINT))
    {
        OsAxsArmVfpContext ctx;
        DWORD i;

        ctx.Fpscr = context.Fpscr;
        ctx.FpExc = context.FpExc;
        for (i = 0; i < OSAXS_NUM_ARM_VFP_REGS; i++)
            ctx.S[i] = context.S[i];
        for (i = 0; i < OSAXS_NUM_ARM_EXTRA_CONTROL_REGS; i++)
            ctx.FpExtra[i] = context.FpExtra[i];

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
#elif defined (MIPS)

    if(SUCCEEDED(hr))
    {
#ifdef _MIPS64
        OsAxsMips64IntContext ctx;
#else
        OsAxsMips32IntContext ctx;
#endif
        ctx.IntAt = context.IntAt;

        ctx.IntV0 = context.IntV0;
        ctx.IntV1 = context.IntV1;

        ctx.IntA0 = context.IntA0;
        ctx.IntA1 = context.IntA1;
        ctx.IntA2 = context.IntA2;
        ctx.IntA3 = context.IntA3;

        ctx.IntT0 = context.IntT0;
        ctx.IntT1 = context.IntT1;
        ctx.IntT2 = context.IntT2;
        ctx.IntT3 = context.IntT3;
        ctx.IntT4 = context.IntT4;
        ctx.IntT5 = context.IntT5;
        ctx.IntT6 = context.IntT6;
        ctx.IntT7 = context.IntT7;

        ctx.IntS0 = context.IntS0;
        ctx.IntS1 = context.IntS1;
        ctx.IntS2 = context.IntS2;
        ctx.IntS3 = context.IntS3;
        ctx.IntS4 = context.IntS4;
        ctx.IntS5 = context.IntS5;
        ctx.IntS6 = context.IntS6;
        ctx.IntS7 = context.IntS7;

        ctx.IntT8 = context.IntT8;
        ctx.IntT9 = context.IntT9;

        ctx.IntK0 = context.IntK0;
        ctx.IntK1 = context.IntK1;

        ctx.IntGp = context.IntGp;
        ctx.IntSp = context.IntSp;

        ctx.IntS8 = context.IntS8;

        ctx.IntRa = context.IntRa;

        ctx.IntLo = context.IntLo;
        ctx.IntHi = context.IntHi;

        ctx.IntFir = context.Fir;
        ctx.IntPsr = context.Psr;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }

#ifdef MIPS_HAS_FPU
    if(SUCCEEDED(hr) && (context.ContextFlags & CE_CONTEXT_FLOATING_POINT))
    {
#ifdef _MIPS64
        OsAxsMips64FpContext ctx;
#else
        OsAxsMips32FpContext ctx;
#endif
        ctx.Fsr = context.Fsr;
        ctx.FltF[0] = context.FltF0;
        ctx.FltF[1] = context.FltF1;
        ctx.FltF[2] = context.FltF2;
        ctx.FltF[3] = context.FltF3;
        ctx.FltF[4] = context.FltF4;
        ctx.FltF[5] = context.FltF5;
        ctx.FltF[6] = context.FltF6;
        ctx.FltF[7] = context.FltF7;
        ctx.FltF[8] = context.FltF8;
        ctx.FltF[9] = context.FltF9;
        ctx.FltF[10] = context.FltF10;
        ctx.FltF[11] = context.FltF11;
        ctx.FltF[12] = context.FltF12;
        ctx.FltF[13] = context.FltF13;
        ctx.FltF[14] = context.FltF14;
        ctx.FltF[15] = context.FltF15;
        ctx.FltF[16] = context.FltF16;
        ctx.FltF[17] = context.FltF17;
        ctx.FltF[18] = context.FltF18;
        ctx.FltF[19] = context.FltF19;
        ctx.FltF[20] = context.FltF20;
        ctx.FltF[21] = context.FltF21;
        ctx.FltF[22] = context.FltF22;
        ctx.FltF[23] = context.FltF23;
        ctx.FltF[24] = context.FltF24;
        ctx.FltF[25] = context.FltF25;
        ctx.FltF[26] = context.FltF26;
        ctx.FltF[27] = context.FltF27;
        ctx.FltF[28] = context.FltF28;
        ctx.FltF[29] = context.FltF29;
        ctx.FltF[30] = context.FltF30;
        ctx.FltF[31] = context.FltF31;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
#endif // MIPS_HAS_FPU

#elif defined (SHx)
    if(SUCCEEDED(hr))
    {
        OsAxsShxIntContext ctx;

        ctx.PR = context.PR;
        ctx.MACH = context.MACH;
        ctx.MACL = context.MACL;
        ctx.GBR = context.GBR;
        ctx.R[0] = context.R0;
        ctx.R[1] = context.R1;
        ctx.R[2] = context.R2;
        ctx.R[3] = context.R3;
        ctx.R[4] = context.R4;
        ctx.R[5] = context.R5;
        ctx.R[6] = context.R6;
        ctx.R[7] = context.R7;
        ctx.R[8] = context.R8;
        ctx.R[9] = context.R9;
        ctx.R[10] = context.R10;
        ctx.R[11] = context.R11;
        ctx.R[12] = context.R12;
        ctx.R[13] = context.R13;
        ctx.R[14] = context.R14;
        ctx.R[15] = context.R15;
        ctx.Fir = context.Fir;
        ctx.Psr = context.Psr;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }

#if defined (SH4)
    if(SUCCEEDED(hr) && (context.ContextFlags & CE_CONTEXT_FLOATING_POINT))
    {
        OsAxsSh4FpContext ctx;
        DWORD i;

        ctx.Fpscr = context.Fpscr;
        ctx.Fpul = context.Fpul;
        for(i = 0; i < OSAXS_NUM_SH4_F_REGS; i++)
            ctx.FRegs[i] = context.FRegs[i];
        for(i = 0; i < OSAXS_NUM_SH4_XF_REGS; i++)
            ctx.xFRegs[i] = context.xFRegs[i];

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
#endif

#elif defined (x86)
    if(SUCCEEDED(hr))
    {
        OsAxsX86IntContext ctx;

        ctx.Gs = context.SegGs;
        ctx.Fs = context.SegFs;
        ctx.Es = context.SegEs;
        ctx.Ds = context.SegDs;
        ctx.Edi = context.Edi;
        ctx.Esi = context.Esi;
        ctx.Ebx = context.Ebx;
        ctx.Edx = context.Edx;
        ctx.Ecx = context.Ecx;
        ctx.Eax = context.Eax;
        ctx.Ebp = context.Ebp;
        ctx.Eip = context.Eip;
        ctx.Cs = context.SegCs;
        ctx.Eflags = context.EFlags;
        ctx.Esp = context.Esp;
        ctx.Ss = context.SegSs;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
    if(SUCCEEDED(hr) && (context.ContextFlags & CE_CONTEXT_FLOATING_POINT))
    {
        OsAxsX86FpContext ctx;

        ctx.ControlWord = context.FloatSave.ControlWord;
        ctx.StatusWord = context.FloatSave.StatusWord;
        ctx.TagWord = context.FloatSave.TagWord;
        ctx.ErrorOffset = context.FloatSave.ErrorOffset;
        ctx.ErrorSelector = context.FloatSave.ErrorSelector;
        ctx.DataOffset = context.FloatSave.DataOffset;
        ctx.DataSelector = context.FloatSave.DataSelector;

        memcpy(&ctx.RegisterArea, &context.FloatSave.RegisterArea, sizeof (ctx.RegisterArea));

        ctx.Cr0NpxState = context.FloatSave.Cr0NpxState;

        hr = Write(&ctx, sizeof(ctx), riOut, cbOut, pbOut);
    }
#endif

    return hr;
}

static HRESULT WriteRegistersForAllThreads(FLEXI_REQUEST const *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;

    PTHREAD pThread;
    DWORD cThreadElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WriteRegistersForAllThreads: Off:%d\r\n", riOut));

    if ((pRequest->Proc.dwStart > pRequest->Proc.dwEnd) || (pRequest->Thread.dwStart > pRequest->Thread.dwEnd))
        hr = E_INVALIDARG;

    DWORD iProc = 0;
    DWORD iThread = 0;
    DWORD ProcCount = 0;

    PROCESS *pProcess = g_pprcNK;
    while (SUCCEEDED (hr) && (ProcCount < MAX_PROCESSES_HANDLED) && (iProc <= pRequest->Proc.dwEnd))
    {
        if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) || (pProcess->dwId == pRequest->dwHProc))
        {
            if (iProc >= pRequest->Proc.dwStart)
            {
                pThread = (PTHREAD) pProcess->thrdList.pFwd; 
                while (SUCCEEDED (hr) && pThread && iThread <= pRequest->Thread.dwEnd)
                {
                    if (!(pRequest->dwRequest & FLEXI_FILTER_THREAD_HANDLE) || (pThread->dwId == pRequest->dwHThread))
                    {
                        if (iThread >= pRequest->Thread.dwStart)
                        {
                            hr = Write ((DWORD)pThread->dwId, riOut, cbOut, pbOut);
                            if (SUCCEEDED (hr))
                            {
                                hr = WriteThreadRegisters(pThread, riOut, cbOut, pbOut);
                            }
                            cThreadElements++;    
                        }
                        iThread++;
                    }

                    pThread = (PTHREAD) pThread->thLink.pFwd;
                    if (pThread == (PTHREAD)&pProcess->thrdList)
                        break;
                }
                ProcCount++;
            }
            iProc++;
        }

        pProcess = (PROCESS *)pProcess->prclist.pFwd;
        if (pProcess == g_pprcNK)
            break;
    }

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cThreadElements : 0;

        DEBUGGERMSG (OXZONE_FLEXI, (L"--WriteRegistersForAllThreads: 0x%.08x, Off:%d, Cnt:%d\r\n", hr, riOut, cThreadElements));

    return hr;
}


static HRESULT MarshalModProcData(FLEXI_REQUEST const *request, DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    struct FlexiModuleProcess modproc;
    HRESULT hr;
    PPROCESS pprc;
    PMODULELIST pmodlist;
    DWORD iProc;
    DWORD iMod;
    BOOL matchProcHandle;
    BOOL matchProcPointer;

    DEBUGGERMSG(OXZONE_FLEXI, (L"++MarshalModProcData: %d of %d, 0x%08X\r\n", riOut, cbOut, pbOut));

    hr = S_OK;
    *pcElements = 0;

    iProc = 0;
    iMod = 0;
    pprc = g_pprcNK;
    do
    {
        // Terminate if we run out of processe range
        if(iProc > request->Proc.dwEnd)
            break;
        // Terminate if we run out of module range
        if (iMod > request->Mod.dwEnd)
            break;
        // Filter on process handle, loop around if handle doesn't match.
        if((request->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) && request->dwHProc != pprc->dwId)
            goto ContinueAdvanceNextProc;
        // Filter on process pointer, loop around if pointer doesn't match
        if((request->dwRequest & FLEXI_FILTER_PROCESS_POINTER) && pprc != (PPROCESS)request->dwHProc)
            goto ContinueAdvanceNextProc;
        // If still below starting handle, loop around.
        if(iProc < request->Proc.dwStart)
            goto ContinueAdvanceNextProcIdx;

        DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalModProcData: Processing Modules for '%s'\r\n", pprc->lpszProcName));

        // Since each process is also a module, we need to handle the current process as a module.
        matchProcHandle = !(request->dwRequest & FLEXI_FILTER_MODULE_HANDLE) || (pprc->dwId == request->dwHMod);
        matchProcPointer = !(request->dwRequest & FLEXI_FILTER_MODULE_POINTER) || (pprc == (PPROCESS)request->dwHMod);
        if(matchProcHandle && matchProcPointer)
        {
            if (iMod >= request->Mod.dwStart && iMod <= request->Mod.dwEnd)
            {
                // The process itself needs to be added to the output stream.
                DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalModProcData: Adding module '%s', refcount = 1\r\n", pprc->lpszProcName));
                modproc.ProcessID = pprc->dwId;
                modproc.ModuleID = pprc->dwId;
                modproc.ModRefCnt = 1;

                hr = Write(&modproc, sizeof(modproc), riOut, cbOut, pbOut);
                if(FAILED(hr))
                    goto Exit;

                ++(*pcElements);
            }

            // Increment the iMod pointer.  Each process counts as one.
            if (iMod <= request->Mod.dwEnd)
            {
                iMod++;
            }
        }

        // For each module in the process's module list.
        pmodlist = (PMODULELIST)pprc->modList.pFwd;
        while(pmodlist && pmodlist != (PMODULELIST)&pprc->modList)
        {
            // Index of module is beyond end of request.
            if(iMod > request->Mod.dwEnd)
                break;
            // Module is loaded as datafile (typically a MUI resource dll) and should be skipped
            if(pmodlist->wFlags & LOAD_LIBRARY_AS_DATAFILE)
                goto ContinueAdvanceNextMod;
            // Module handle doesn't match
            if((request->dwRequest & FLEXI_FILTER_MODULE_HANDLE) && pmodlist->pMod != (PMODULE)request->dwHMod)
                goto ContinueAdvanceNextMod;
            // Module pointer doesn't match
            if((request->dwRequest & FLEXI_FILTER_MODULE_POINTER) && pmodlist->pMod != (PMODULE)request->dwHMod)
                goto ContinueAdvanceNextMod;
            // Index of module is less than start of request.
            if(iMod < request->Mod.dwStart)
                goto ContinueAdvanceNextModIdx;

            DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalModProcData: Adding module '%s', refcount = %d\r\n", pmodlist->pMod->lpszModName, pmodlist->wRefCnt));
            // Output the data.
            modproc.ProcessID = pprc->dwId;
            modproc.ModuleID = (DWORD)pmodlist->pMod;
            modproc.ModRefCnt = pmodlist->wRefCnt;

            hr = Write(&modproc, sizeof(modproc), riOut, cbOut, pbOut);
            if(FAILED(hr))
                goto Exit;

            ++(*pcElements);

        ContinueAdvanceNextModIdx:
            // Advance the index of modules if it matches the handle or pointer match
            ++iMod;
        ContinueAdvanceNextMod:
            // Next module
            pmodlist = (PMODULELIST)pmodlist->link.pFwd;
        }

    ContinueAdvanceNextProcIdx:
        ++iProc;
    ContinueAdvanceNextProc:
        pprc = (PPROCESS)pprc->prclist.pFwd;
    }while(pprc != g_pprcNK);

Exit:
    DEBUGGERMSG(OXZONE_FLEXI, (L"--MarshalModProcData: hr=%08X, nelts=%d\r\n", hr, *pcElements));
    return hr;
}


struct RespSt
{
    DWORD dwHeaderMask;         // Mask for header request
    DWORD dwDescMask;           // Mask for description request
    DWORD dwDataMask;           // Mask for data request
    DWORD dwAddressMask;        // Mask for address request

    HRESULT (*pfnMarshal) (FLEXI_REQUEST const *pRequest, DWORD &riOut, 
        const DWORD cbOut, BYTE *pbOut, DWORD *pcElements);
    HRESULT (*pfnMarshalOne) (FLEXI_REQUEST const *pRequest, DWORD &riOut,
        const DWORD cbOut, BYTE *pbOut);

    FLEXI_FIELD_INFO *pFields;
    DWORD cFields;
};


/*++

Routine Name:

    WritePTMResponseHeader

Routine Description:

    Generate the appropriate flexi header for a particular request.  This
    header contains:
    * Size of Header (16-bit) 
    * Size of Field Info Record (16-bit) 
    * Number of Field Info Records (32-bit)
    * Number of Data Elements (32-bit)
    * Offset of Elements from start of buffer (32-bit)

Return Values:

    S_OK : Success,
    E_FAIL : General failure,
    E_OUTOFMEMORY : Output buffer is of insufficient size.

--*/
HRESULT WritePTMResponseHeader(DWORD cFieldInfo,
                               DWORD cElements,
                               DWORD dwElementOffset,
                               DWORD &riOut,
                               const DWORD cbOut,
                               BYTE *pbOut)
{
    HRESULT hr;
    CEDUMP_ELEMENT_LIST data;

    data.SizeOfHeader = sizeof(data);
    data.SizeOfFieldInfo = sizeof(CEDUMP_FIELD_INFO);
    data.NumberOfFieldInfo = cFieldInfo;
    data.NumberOfElements = cElements;
    data.Elements = dwElementOffset;

    hr = Write(&data, sizeof(data), riOut, cbOut, pbOut);
    return hr;
}


static
HRESULT WritePTMResponseHeader(FLEXI_REQUEST const *req,
                               struct RespSt const *resp,
                               DWORD cFieldInfo,
                               DWORD cElements,
                               DWORD dwElementOffset,
                               DWORD &virtualOut,
                               DWORD &riOut,
                               const DWORD cbOut,
                               BYTE *pbOut)
{
    HRESULT hr;
    DWORD iStart;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WritePTMResponseHeader: FieldCnt:%d, ElementCnt:%d, RVA:0x%.08x, Offset:%d\r\n",
        cFieldInfo, cElements, dwElementOffset, riOut));

    if(req->dwRequest & resp->dwHeaderMask)
    {
        iStart = riOut;
        hr = WritePTMResponseHeader(cFieldInfo, cElements, dwElementOffset, riOut, cbOut, pbOut);
        if(FAILED(hr))
            goto Exit;

        virtualOut += (riOut - iStart);
    }
    else
    {
        DEBUGGERMSG (OXZONE_FLEXI, (L" WritePTMResponseHeader: No header to write\r\n"));
        hr = S_OK;
    }

Exit:
    DEBUGGERMSG (OXZONE_FLEXI, (L"--WritePTMResponseHeader: 0x%.08x\r\n", hr));
    return hr;
}


static HRESULT WritePTMResponseFieldInfo(FLEXI_REQUEST const *req,
                                         struct RespSt const *resp,
                                         DWORD &iVirtualOut, 
                                         DWORD &riOut,
                                         const DWORD cbOut,
                                         BYTE *pbOut)
{
    HRESULT hr;
    BYTE *pbDescOut;
    DWORD iDescOut;
    DWORD iDescStart;

    pbDescOut = NULL;
    iDescOut = riOut;
    iDescStart = riOut;

    // If the user asks for field info, then it is ok to use the actual output buffer.
    if(req->dwRequest & resp->dwDescMask)
        pbDescOut = pbOut;
        
    // If the user didn't ask for header, we need to tell the user how many field
    // records are being outputed.  Write a 32-bit count.
    if (!(req->dwRequest & resp->dwHeaderMask))
    {
        hr = Write(resp->cFields, iDescOut, cbOut, pbDescOut);
        if(FAILED(hr))
            goto Exit;
    }

    hr = WriteFieldInfo(resp->pFields, resp->cFields, iDescOut, cbOut, pbDescOut);
    if(FAILED(hr))
        goto Exit;

    // Update the virtual out by incrementing by size of data output.
    // this allows the virtual out to be a completely different data
    // value from the actual out.  These two should be the same when
    // doing a complete write out.
    iVirtualOut += iDescOut - iDescStart;

    // If the descriptor is really going out, then remember it.
    if (req->dwRequest & resp->dwDescMask)
        riOut = iDescOut;

Exit:
    return hr;
}


static HRESULT WritePTMResponseData(FLEXI_REQUEST const *req,
                                    struct RespSt const *resp,
                                    DWORD &iElementStart,
                                    DWORD &cElements,
                                    DWORD &iVirtualOut,
                                    DWORD &riOut,
                                    const DWORD cbOut,
                                    BYTE *pbOut)
{
    HRESULT hr;
    BYTE *pbDataOut = NULL;
    DWORD iDataOut = riOut;
    DWORD iDataStart = riOut;
    DWORD iElementCnt;

    DEBUGGERMSG(OXZONE_FLEXI, (L"++WritePTMResponseData: Buffer Status:%d of %d, 0x%08X\r\n", riOut, cbOut, pbOut));

    // Header only requests use virtual offset.
    // Otherwise, use the actual offset.
    if ((req->dwRequest & resp->dwHeaderMask) && !(req->dwRequest & resp->dwDescMask) && !(req->dwRequest & resp->dwDataMask))
        iElementStart = iVirtualOut;
    else
        iElementStart = iDataOut;

    // If data is being written, then actually write to the buffer.
    if (req->dwRequest & resp->dwDataMask)
        pbDataOut = pbOut;

    // If no header is present, need to tell the user how many data elements
    // are in the output.
    if (!(req->dwRequest & resp->dwHeaderMask))
    {
        iElementCnt = iDataOut;
        hr = Write ((DWORD)0, iDataOut, cbOut, pbDataOut);
        if(FAILED(hr))
            goto Exit;
    }
    
    if ((resp->dwAddressMask & req->dwRequest) && resp->pfnMarshalOne)
    {
        hr = resp->pfnMarshalOne(req, iDataOut, cbOut, pbDataOut);
        cElements = 1;
    }
    else
        hr = resp->pfnMarshal(req, iDataOut, cbOut, pbDataOut, &cElements);

    if(FAILED(hr))
        goto Exit;

    // Write back the count if no header is present
    if (!(req->dwRequest & resp->dwHeaderMask))
    {
        hr = Write (cElements, iElementCnt, cbOut, pbDataOut); 
        if(FAILED(hr))
            goto Exit;
    }

    iVirtualOut += iDataOut - iDataStart;
    if (req->dwRequest & resp->dwDataMask)
        riOut = iDataOut;

Exit:
    DEBUGGERMSG(OXZONE_FLEXI, (L"--WritePTMResponseData: hr=%08X, Buffer Status:%d of %d\r\n", hr, riOut, cbOut));
    return hr;
}


/*++

Routine Name:

    WritePTMResponse

Routine Description:

    All flexible api requests have the same format.  This function is generic
    so that if this general format changes, only one change needs to be made.

    This function will write out the header, description, and data (if requested)
    in the proper format.

    On a header only output, the header will be populated with information 
    as if all sections were being written out.

Arguments:
    
    pRequest - Pointer to original request, which is needed for ranges.
    prsp     - Pointer to structure that contains appropriate masks and function
               pointers to call to get data.
    riOut    - in/out The index of the buffer.
    cbOut    - The size of the output buffer.
    pbOut    - The beginning of the output buffer.

Return Value:

    S_OK: Success,
    E_FAIL: Generic error,
    E_OUTOFMEMORY: Output buffer was of insufficient size.

--*/
static HRESULT WritePTMResponse (FLEXI_REQUEST *pRequest, struct RespSt *prsp, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    // Header information to keep track of data.
    DWORD iHeaderStart = riOut;     // Remember where the header is supposed to be.
    DWORD iElementStart = 0;        // Remember the ElementRVA
    DWORD cElements = 0;            // Remember the number of elements
    DWORD iVirtualOut = riOut;      // Keep track of the virtual offset for ElementRVA (header only req)
                                    // This will assume that all data is going out.
    DWORD FakeVirtualOut = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WritePTMResponse Offset:%d\r\n", riOut));
    
    hr = WritePTMResponseHeader(pRequest, prsp, 0, 0, 0, iVirtualOut, riOut, cbOut, pbOut);
    if(FAILED(hr))
        goto Exit;

    hr = WritePTMResponseFieldInfo(pRequest, prsp, iVirtualOut, riOut, cbOut, pbOut);
    if(FAILED(hr))
        goto Exit;

    hr = WritePTMResponseData(pRequest, prsp, iElementStart, cElements, iVirtualOut, riOut, cbOut, pbOut);
    if(FAILED(hr))
        goto Exit;

    hr = WritePTMResponseHeader(pRequest, prsp, prsp->cFields, cElements, iElementStart, FakeVirtualOut, iHeaderStart, cbOut, pbOut);

Exit:
    DEBUGGERMSG (OXZONE_FLEXI, (L"--WritePTMResponse: 0x%.08x\n", hr));
    return hr;
}




/*++

Routine Name:

    GetFPTMI

Routine Description:

    Handle Flexible Info request and package the response into a caller specified buffer.

Arguments:

    pRequest     - IN Request structure
    pcbOutBuffer - IN/OUT The size of the buffer / The size consumed by response.
    pbOutBuffer  - IN/OUT The response buffer.

Return Value:

    S_OK : success,
    E_FAIL : general failure,
    E_OUTOFMEMORY : buffer is too small, partial response contained within.

--*/
HRESULT GetFPTMI (FLEXI_REQUEST *pRequest, DWORD *pcbOutBuffer, BYTE *pbOutBuffer)
{
    HRESULT hr = S_OK;
    DWORD cbOutBuffer = pcbOutBuffer ? *pcbOutBuffer : 0;
    DWORD iOutBuffer = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++GetFPTMI: ReplyBuf = %d\r\n", cbOutBuffer));

    if (!pRequest || !pcbOutBuffer)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    DEBUGGERMSG (OXZONE_FLEXI, (L"++GetFPTMI: Request = 0x%08x\r\n", pRequest->dwRequest));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: Proc Idx Range 0x%08x - 0x%08x\r\n", pRequest->Proc.dwStart, pRequest->Proc.dwEnd));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: ProcHndPtr = 0x%08x\r\n", pRequest->dwHProc));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: Thread Idx Range 0x%08x - 0x%08x\r\n", pRequest->Thread.dwStart, pRequest->Thread.dwEnd));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: ThreadHndPtr = 0x%08x\r\n", pRequest->dwHThread));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: Mod Idx Range 0x%08x - 0x%08x\r\n", pRequest->Mod.dwStart, pRequest->Mod.dwEnd));        
    DEBUGGERMSG (OXZONE_FLEXI, (L"  GetFPTMI: ModHndPtr = 0x%08x\r\n", pRequest->dwHMod));        

    if (pRequest->dwRequest & FLEXI_REQ_PROCESS_ALL)
    {
        struct RespSt rsp = {FLEXI_REQ_PROCESS_HDR, FLEXI_REQ_PROCESS_DESC, FLEXI_REQ_PROCESS_DATA, FLEXI_FILTER_PROCESS_POINTER,
            MarshalProcessData, MarshalOneProcess, ProcessDesc, C_PROCESSFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
        if(FAILED(hr))
            goto Exit;
    }

    if (pRequest->dwRequest & FLEXI_REQ_THREAD_ALL)
    {
        struct RespSt rsp = {FLEXI_REQ_THREAD_HDR, FLEXI_REQ_THREAD_DESC, FLEXI_REQ_THREAD_DATA, FLEXI_FILTER_THREAD_POINTER,
            MarshalThreadData, MarshalOneThread, ThreadDesc, C_THREADFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
        if(FAILED(hr))
            goto Exit;
    }
    
    if (pRequest->dwRequest & FLEXI_REQ_MODULE_ALL)
    {
        struct RespSt rsp = {FLEXI_REQ_MODULE_HDR, FLEXI_REQ_MODULE_DESC, FLEXI_REQ_MODULE_DATA, FLEXI_FILTER_MODULE_POINTER,
            MarshalModuleData, MarshalOneModule, ModuleDesc, C_MODULEFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
        if(FAILED(hr))
            goto Exit;
    }

    if (pRequest->dwRequest & FLEXI_REQ_CONTEXT_ALL)
    {
        FLEXI_FIELD_INFO rgFieldInfo[2] =
        {
            {cfiThreadId, cfsThreadId, L"ThreadID", L"0x%.08x"},
#if defined (x86)
            {cfiX86IntCxt, cfsX86IntCxt, L"X86 Integer Context", L""},
#elif defined (ARM)
            {cfiArmIntCxt, cfsArmIntCxt, L"Arm Integer Context", L""},
#elif defined (MIPS)
#ifdef _MIPS64
            {cfiMips64IntCxt, cfsMips64IntCxt, L"Mips64 Integer Context", L""},
#else
            {cfiMips32IntCxt, cfsMips32IntCxt, L"Mips32 Integer Context", L""},
#endif
#elif defined (SHx)
            {cfiSHxIntCxt, cfsSHxIntCxt, L"SH Integer Context", L""}
#endif
        };

        struct RespSt rsp = {FLEXI_REQ_CONTEXT_HDR, FLEXI_REQ_CONTEXT_DESC, 
            FLEXI_REQ_CONTEXT_DATA, 0, WriteRegistersForAllThreads, 0, rgFieldInfo, 2};
        
        hr = WritePTMResponse(pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
        if(FAILED(hr))
            goto Exit;
    }

    if(pRequest->dwRequest & FLEXI_REQ_MODPROC_ALL)
    {
        struct RespSt rsp = {FLEXI_REQ_MODPROC_HDR,
                             FLEXI_REQ_MODPROC_DESC,
                             FLEXI_REQ_MODPROC_DATA,
                             0,
                             MarshalModProcData,
                             0,
                             ModProcDesc,
                             C_MODPROCFIELDS};
        hr = WritePTMResponse(pRequest,
                              &rsp,
                              iOutBuffer,
                              cbOutBuffer,
                              pbOutBuffer);
        if(FAILED(hr))
            goto Exit;
    }
    
    *pcbOutBuffer = iOutBuffer;
    
Exit:
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--GetFPTMI: hr=0x%08x, bytes=%d\r\n", hr, iOutBuffer));
    return hr;
}

