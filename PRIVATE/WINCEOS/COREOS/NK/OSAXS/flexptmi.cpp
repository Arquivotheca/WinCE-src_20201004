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

    flexptmi.cpp

Abstract:

    Implement the Flexi-PTMI interface for Osaxs

Environment:

    OsaxsH / OsaxsT

--*/


#include "osaxs_p.h"


/********** Descriptors */
BEGIN_FIELD_INFO_LIST (ProcessDesc)
    FIELD_INFO (pfiProcessSlot,      sizeof (BYTE),   L"ProcSlot#",   L"%u")      //1
    FIELD_INFO (pfiName,             CCH_PROCNAME,    L"Name",        L"%s")      //33
    FIELD_INFO (pfiStartOfAddrSpace, sizeof (DWORD),  L"VMBase",      L"0x%08lX") //37
    FIELD_INFO (pfiDefaultAccessKey, sizeof (ULONG),  L"AccessKey",   L"0x%08lX") //41
    FIELD_INFO (pfiTrustLevel,       sizeof (BYTE),   L"TrustLevel",  L"%N{0=None,1=Run,2=Full}") //42
    FIELD_INFO (pfiHandle,           sizeof (HANDLE), L"hProcess",    L"0x%08lX") //46
    FIELD_INFO (pfiBasePtr,          sizeof (LPVOID), L"BasePtr",     L"0x%08lX") //50
    FIELD_INFO (pfiTlsUsageBitMaskL, sizeof (DWORD),  L"TlsUseL32b",  L"0x%08lX") //54
    FIELD_INFO (pfiTlsUsageBitMaskH, sizeof (DWORD),  L"TlsUseH32b",  L"0x%08lX") //58
    FIELD_INFO (pfiCurDbgZoneMasks,  sizeof (ULONG),  L"CurZoneMask", L"0x%08lX") //62
    FIELD_INFO (pfiStructAddr,       sizeof (LPVOID), L"pProcess",    L"0x%08lX") //66
    FIELD_INFO (pfiCmdLine,          CCH_CMDLINE,     L"CmdLine",     L"%s")      //194
END_FIELD_INFO_LIST ();

#pragma pack (push, 1)
struct FlexiProcess
{
    BYTE bSlot;
    char szName [CCH_PROCNAME];
    DWORD dwVMBase;
    DWORD dwAccessKey;
    BYTE bTrustLevel;
    DWORD dwHandle;
    DWORD dwBasePtr;
    DWORD dwTlsUseLow;
    DWORD dwTlsUseHigh;
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
    FIELD_INFO (tfiAddrSpaceAccessKey,        sizeof (ACCESSKEY), L"AccessKey",    L"0x%08lX")    //17
    FIELD_INFO (tfiHandleCurrentProcessRunIn, sizeof (HANDLE),    L"hCurProcIn",   L"0x%08lX")    //21
    FIELD_INFO (tfiHandleOwnerProc,           sizeof (HANDLE),    L"hOwnerProc",   L"0x%08lX")    //25
    FIELD_INFO (tfiCurrentPriority,           sizeof (BYTE),      L"CurPrio",      L"%u")         //26
    FIELD_INFO (tfiBasePriority,              sizeof (BYTE),      L"BasePrio",     L"%u")         //27
    FIELD_INFO (tfiKernelTime,                sizeof (DWORD),     L"KernelTime",   L"%lu")        //31
    FIELD_INFO (tfiUserTime,                  sizeof (DWORD),     L"UserTime",     L"%lu")        //35
    FIELD_INFO (tfiQuantum,                   sizeof (DWORD),     L"Quantum",      L"%lu")        //39
    FIELD_INFO (tfiQuantumLeft,               sizeof (DWORD),     L"QuantuLeft",   L"%lu")        //43
    FIELD_INFO (tfiSleepCount,                sizeof (DWORD),     L"SleepCount",   L"%lu")        //47
    FIELD_INFO (tfiSuspendCount,              sizeof (BYTE),      L"SuspendCount", L"%u")         //48
    FIELD_INFO (tfiTlsPtr,                    sizeof (LPDWORD),   L"TlsPtr",       L"0x%08lX")    //52
    FIELD_INFO (tfiLastError,                 sizeof (DWORD),     L"LastError",    L"0x%08lX")    //56
    FIELD_INFO (tfiStackBase,                 sizeof (DWORD),     L"StackBase",    L"0x%08lX")    //60
    FIELD_INFO (tfiStackLowBound,             sizeof (DWORD),     L"StkLowBnd",    L"0x%08lX")    //64
    FIELD_INFO (tfiCreationTimeMSW,           sizeof (DWORD),     L"CreatTimeH",   L"0x%08lX")    //68
    FIELD_INFO (tfiCreationTimeLSW,           sizeof (DWORD),     L"CreatTimeL",   L"0x%08lX")    //72
    FIELD_INFO (tfiPC,                        sizeof (DWORD),     L"PC",           L"0x%08lX")    //76
    FIELD_INFO (tfiNcrPtr,                    sizeof (DWORD),     L"NcrPtr",       L"0x%08lX")    //80
    FIELD_INFO (tfiStkRetAddr,                sizeof (DWORD),     L"StkRetAddr",   L"0x%08lX")    //84
END_FIELD_INFO_LIST();

#pragma pack (push, 1)
struct FlexiThread
{
    DWORD dwAddr;
    WORD wRunState;
    WORD wInfo;
    DWORD dwHandle;
    BYTE bWaitState;
    DWORD dwAccessKey;
    DWORD dwCurProcHandle;
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
    DWORD dwCreationTimeHi;
    DWORD dwCreationTimeLo;
    DWORD dwCurrentPC;
    DWORD dwNcrPtr;
    DWORD dwStkRetAddr;
};
#pragma pack (pop)

BEGIN_FIELD_INFO_LIST (ModuleDesc)
    FIELD_INFO (mfiName,          CCH_MODULENAME, L"Name",          L"%s")            //32
    FIELD_INFO (mfiBasePointer,   sizeof(DWORD),  L"Base Ptr",      L"0x%08lX")       //36
    FIELD_INFO (mfiModuleSize,    sizeof(DWORD),  L"Size",          L"%lu")           //40
    FIELD_INFO (mfiRdWrDataStart, sizeof(DWORD),  L"RW Data Start", L"0x%08lX")       //44
    FIELD_INFO (mfiRdWrDataEnd,   sizeof(DWORD),  L"RW Data End",   L"0x%08lX")       //48
    FIELD_INFO (mfiTimeStamp,     sizeof(DWORD),  L"Timestamp",     L"0x%08lX")       //52
    FIELD_INFO (mfiPdbFormat,     sizeof(DWORD),  L"PDB Format",    L"%N{808534606=NB10,1396986706=RSDS}")              //56
    FIELD_INFO (mfiPdbGuid,       sizeof(GUID),   L"PDB Guid",      L"%U")            //72
    FIELD_INFO (mfiPdbAge,        sizeof(DWORD),  L"PDB Age",       L"%lu")           //76
    FIELD_INFO (mfiDllHandle,     sizeof(DWORD),  L"hDll",          L"0x%08lX")       //80
    FIELD_INFO (mfiInUse,         sizeof(DWORD),  L"In Use",        L"0x%08lX")       //84
    FIELD_INFO (mfiFlags,         sizeof(WORD),   L"Flags",         L"%T{0=NoDllRef,1=Data,3=SearchPath,15=Kernel}") //86
    FIELD_INFO (mfiTrustLevel,    sizeof(BYTE),   L"Trust Level",   L"%d")            //87
    FIELD_INFO (mfiRefCount,      CB_MODREFCNT,   L"RefCount",      L"")              //151
    FIELD_INFO (mfiStructAddr,    sizeof(DWORD),  L"Pointer",       L"0x%08kX")
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
    BYTE bTrustLevel;
    WORD rgwRefCount [MAX_PROCESSES];
    DWORD dwAddr;
};
#pragma pack (pop)


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
                pNextStr += sizeof(DWORD) + (kdbgwcslen(pFields[i].szLabel) + 1) * sizeof(WCHAR);
        }
        if (SUCCEEDED (hr))
        {
            hr = Write(pNextStr, riOut, cbOut, pbOut);
            if (SUCCEEDED (hr))
                pNextStr += sizeof(DWORD) + (kdbgwcslen(pFields[i].szFormat) + 1) * sizeof(WCHAR);
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
            DWORD sizeLabel = kdbgwcslen(pFields[i].szLabel) * sizeof(WCHAR);
            hr = Write(sizeLabel, riOut, cbOut, pbOut);
            if (SUCCEEDED(hr))
                hr = Write(pFields[i].szLabel, 
                    sizeLabel + (sizeof (WCHAR) * 1),
                    riOut, cbOut, pbOut);

            DWORD sizeFormat = kdbgwcslen(pFields[i].szFormat) * sizeof(WCHAR);
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
    proc.bSlot = pProc->procnum;

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

    proc.dwVMBase = pProc->dwVMBase;
    proc.dwAccessKey = pProc->aky;
    proc.bTrustLevel = pProc->bTrustLevel;
    proc.dwHandle = reinterpret_cast <DWORD> (pProc->hProc);
    proc.dwBasePtr = reinterpret_cast <DWORD> (pProc->BasePtr);
    proc.dwTlsUseLow = pProc->tlsLowUsed;
    proc.dwTlsUseHigh = pProc->tlsHighUsed;
    if (pProc->ZonePtr)
    {
        __try
        {
            proc.dwZoneMask = pProc->ZonePtr->ulZoneMask;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            proc.dwZoneMask = 0;
        }
    }
    proc.dwAddr = reinterpret_cast <DWORD> (pProc);
    if (pProc->pcmdline)
    {
        __try
        {
            WideToAnsi (pProc->pcmdline, proc.szCommandLine, CCH_CMDLINE);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy (proc.szCommandLine, "Unknown");
        }
    }
    else
        strcpy (proc.szCommandLine, "Unknown");

    HRESULT hr = Write (&proc, sizeof (proc), riOut, cbOut, pbOut);

    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--WriteProcessData: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalOneProcess (FLEXI_REQUEST *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalOneProcess\r\n"));

    if (pRequest->dwRequest & FLEXI_FILTER_PROCESS_POINTER)
    {
        PPROCESS pProcess = reinterpret_cast <PPROCESS> (pRequest->dwHProc);
        PPROCESS pProcArray = FetchProcArray ();

        if (pProcess >= pProcArray && pProcess < &pProcArray [MAX_PROCESSES])
        {
            hr = WriteProcessData (pProcess, riOut, cbOut, pbOut);
        }
        else
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


static HRESULT MarshalProcessData (FLEXI_REQUEST *pRequest, DWORD &riOut, 
    const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;
    PROCESS *pProcArray;
    DWORD i;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalProcessData: Offset:%d\r\n", riOut));

    pProcArray = FetchProcArray();
    if (!pProcArray)
        hr = E_FAIL;

    if (pRequest->Proc.dwStart > pRequest->Proc.dwEnd)
        hr = E_INVALIDARG;

    DWORD iProc = 0;
    DWORD cProcessElements = 0;
    for (i = 0;
        SUCCEEDED (hr) && (i < MAX_PROCESSES) && (iProc <= pRequest->Proc.dwEnd);
        i++)
    {
        if (pProcArray[i].dwVMBase)
        {
            if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) ||
                (reinterpret_cast<DWORD>(pProcArray[i].hProc) == pRequest->dwHProc))
            {
                if (iProc >= pRequest->Proc.dwStart)
                {
                    hr = WriteProcessData (&pProcArray[i], riOut, cbOut, pbOut);
                    if (SUCCEEDED(hr))
                        cProcessElements++;
                } 
                iProc++;
            } 
            else
            {
                DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalProcessData: Proc #%d does not match proc handle 0x%.08x\r\n",
                                           i, pRequest->dwHProc));
            }
        }
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
    ACCESSKEY   aKey;
    DWORD       dwQuantum;
    BYTE        bCPrio;
    BYTE        bBPrio;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WriteThreadData: Thread=0x%.08x, Offset=%d\r\n", pThread, riOut));

    if ((pThread == pCurThread) && (g_psvdThread) && (g_psvdThread->fSaved))
    { // WARNING - SPECIAL CASE: If thread is current thread we need to use saved value instead
        aKey = g_psvdThread->aky;
        bCPrio = g_psvdThread->bCPrio;
        bBPrio = g_psvdThread->bBPrio;
        dwQuantum = g_psvdThread->dwQuantum;
    }
    else
    {
        aKey = pThread->aky;
        bCPrio = pThread->bCPrio;
        bBPrio = pThread->bBPrio;
        dwQuantum = pThread->dwQuantum;
    }

    thrd.dwAddr = reinterpret_cast <DWORD> (pThread);
    thrd.wRunState = ((pThread->wInfo & THREAD_STATUS_MASK) << 2) | (1 << GET_RUNSTATE (pThread));
    thrd.wInfo = static_cast <WORD> (pThread->wInfo & THREAD_INFO_MASK);
    thrd.dwHandle = reinterpret_cast <DWORD> (pThread->hTh);
    thrd.bWaitState = pThread->bWaitState;
    thrd.dwAccessKey = aKey;
    thrd.dwCurProcHandle = reinterpret_cast <DWORD> (pThread->pProc->hProc);
    thrd.dwOwnProcHandle = reinterpret_cast <DWORD> (pThread->pOwnerProc->hProc);
    thrd.bCurPrio = bCPrio;
    thrd.bBasePrio = bBPrio;
    thrd.dwKernelTime = pThread->dwKernTime;
    thrd.dwUserTime = pThread->dwUserTime;
    thrd.dwQuantum = dwQuantum;
    thrd.dwQuantumLeft = pThread->dwQuantLeft;
    thrd.dwSleepCount = pThread->dwWakeupTime;
    thrd.bSuspendCount = pThread->bSuspendCnt;
    thrd.dwTlsPtr = reinterpret_cast <DWORD> (pThread->tlsPtr);
    thrd.dwLastError = pThread->dwLastError;
    thrd.dwStackBase = pThread->tlsPtr [PRETLS_STACKBASE];
    thrd.dwStackLowBound = pThread->tlsPtr [PRETLS_STACKBOUND];
    thrd.dwCreationTimeHi = pThread->ftCreate.dwHighDateTime;
    thrd.dwCreationTimeLo = pThread->ftCreate.dwLowDateTime;
    thrd.dwCurrentPC = GetThreadProgramCounter (pThread);
#if defined(x86) 
    {
        DWORD dwStackBase = pThread->tlsPtr[PRETLS_STACKBASE];
        DWORD dwStackSize = 0;

        if (pThread->tlsPtr != pThread->tlsNonSecure)
        {
            // Secure stack always created with fixed stack size
            dwStackSize = CNP_STACK_SIZE;
        }
        else
        {
            // Normal stack can be created with specific stack size
            dwStackSize = pThread->dwOrigStkSize;
        }
        
        thrd.dwNcrPtr = reinterpret_cast <DWORD> (NCRPTR (dwStackBase, dwStackSize));
    }
#endif
    thrd.dwStkRetAddr = reinterpret_cast <DWORD> (pThread->pcstkTop ? pThread->pcstkTop->retAddr : 0); 

    hr = Write (&thrd, sizeof (thrd), riOut, cbOut, pbOut);
        
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--WriteThreadData: 0x%.08x\r\n", hr));
    return hr;
}


static HRESULT MarshalOneThread (FLEXI_REQUEST *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
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
static HRESULT MarshalThreadData (FLEXI_REQUEST *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut, 
    DWORD *pcElements)
{
    HRESULT hr = S_OK;
    DWORD cThreadElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalThreadData: Offset:%d\r\n", riOut));

    PROCESS *pProcArray = FetchProcArray();
    if (!pProcArray)
        hr = E_FAIL;

    if ((pRequest->Proc.dwStart > pRequest->Proc.dwEnd) || (pRequest->Thread.dwStart > pRequest->Thread.dwEnd))
        hr = E_INVALIDARG;
    
    DWORD iProcSlot = 0;
    DWORD iProc = 0;
    DWORD iThread = 0;

    for (iProcSlot = 0;
        SUCCEEDED (hr) && iProcSlot < MAX_PROCESSES && iProc <= pRequest->Proc.dwEnd;
        iProcSlot++)
    {
        if (pProcArray[iProcSlot].dwVMBase)
        {
            if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) ||
                (reinterpret_cast<DWORD>(pProcArray[iProcSlot].hProc) == pRequest->dwHProc))
            {
                if (iProc >= pRequest->Proc.dwStart)
                {
                    DEBUGGERMSG (OXZONE_FLEXI, (L"  MarshalThreadData: Getting threads for proc %d\r\n", iProcSlot));
                    PTHREAD pThread = pProcArray[iProcSlot].pTh;
                    while (SUCCEEDED (hr) && pThread && iThread <= pRequest->Thread.dwEnd)
                    {
                        if (!(pRequest->dwRequest & FLEXI_FILTER_THREAD_HANDLE) ||
                            (reinterpret_cast<DWORD>(pThread->hTh) == pRequest->dwHThread))
                        {
                            if (iThread >= pRequest->Thread.dwStart)
                            {
                                hr = WriteThreadData (pThread, riOut, cbOut, pbOut);
                                if (SUCCEEDED (hr))
                                    cThreadElements++;
                            }
                            iThread++;
                        }
                        else
                        {
                            DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalThreadData: Thread 0x%.08x does not match handle 0x%.08x\r\n",
                                                       pThread, pRequest->dwHThread));
                        }
                        pThread = pThread->pNextInProc;
                    }
                }
                iProc++;
            }
            else
            {
                DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalThreadData: Proc #%d does not match proc handle 0x%.08x\r\n",
                                           iProcSlot, pRequest->dwHProc));
            }
        }
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

    DEBUGGERMSG(OXZONE_FLEXI, (L"++WriteProcessDataAsModule\r\n"));
    __try
    {
        WideToAnsi (pProcess->lpszProcName, mod.szModuleName, CCH_MODULENAME);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        strcpy (mod.szModuleName, "Unknown");
    }
    DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalOneProcessAsModule: Module Name = %a\r\n", mod.szModuleName));

    mod.dwBasePointer = (reinterpret_cast <DWORD> (pProcess->BasePtr) == 0x10000) ?
        (reinterpret_cast <DWORD> (pProcess->BasePtr) | pProcess->dwVMBase) :
        reinterpret_cast <DWORD> (pProcess->BasePtr);
    mod.dwModuleSize = pProcess->e32.e32_vsize;
    if (!pProcess->procnum)
    {
        DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalOneProcessAsModule: Handling NK RD/WR section\r\n"));
        const ROMHDR *pTOC = FetchTOC ();
        if (pTOC)
        {
            const COPYentry *pce = FetchCopyEntry (pTOC->ulCopyOffset);
            if (pce)
            {
                mod.dwRdWrDataStart = pce->ulDest;
                mod.dwRdWrDataEnd = pce->ulDest + pce->ulDestLen - 1;
            }
        }
    }
    mod.dwTimeStamp = pProcess->e32.e32_timestamp;

    static ModuleSignature ms;

    // Ignore failure from GetProcessDebugInfo, since some processes may have no debug info
    if (SUCCEEDED (GetProcessDebugInfo (pProcess, &ms)))
    {
        mod.dwPdbFormat = ms.dwType;
        mod.PdbGuid = ms.guid;
        mod.dwPdbAge = ms.dwAge;
    }

    mod.dwInUse = 1 << pProcess->procnum;
    mod.bTrustLevel = pProcess->bTrustLevel;
    mod.rgwRefCount [pProcess->procnum] = 1;
    mod.dwAddr = reinterpret_cast <DWORD> (pProcess);

    HRESULT hr = Write (&mod, sizeof (mod), riOut, cbOut, pbOut);
    DEBUGGERMSG(OXZONE_FLEXI, (L"--WriteProcessDataAsModule: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalProcessesAsModules (FLEXI_REQUEST *pRequest, DWORD &rdwIdx, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;
    DWORD iProc;
    PROCESS *pProcArray;
    DWORD cModElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalProcessesAsModules: Idx:%d, Offset:%d\r\n", rdwIdx, riOut));
    
    if (pRequest->Mod.dwStart > pRequest->Mod.dwEnd)
        hr = E_INVALIDARG;

    if (SUCCEEDED (hr))
    {
        pProcArray = FetchProcArray ();
        if (!pProcArray)
            hr = E_FAIL;
    }

    if (!(pRequest->dwRequest & FLEXI_FILTER_MODULE_HANDLE))
    { // Processes don't have a module handle.
        for (iProc = 0;
            SUCCEEDED (hr) && iProc < MAX_PROCESSES && rdwIdx <= pRequest->Mod.dwEnd;
            iProc++)
        {
            PROCESS *pProc = &pProcArray[iProc];
            if (pProc->dwVMBase)
            {
                PMODULE pCoreDll = (PMODULE)hCoreDll;
                DWORD dwInUse = (1 << pProc->procnum);

                // Make sure process is not busy starting or dying (Check if using CoreDll.dll)
                if (pCoreDll && (dwInUse & pCoreDll->inuse))
                {
                    if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) ||
                        (reinterpret_cast<DWORD>(pProc->hProc) == pRequest->dwHProc))
                    { // Only output the process that matches this handle
                        if (rdwIdx >= pRequest->Mod.dwStart)
                        {
                            hr = WriteProcessDataAsModule (pProc, riOut, cbOut, pbOut);
                            if (SUCCEEDED(hr))
                               cModElements++;
                        }
                        rdwIdx++;
                    }
                    else
                    {
                        DEBUGGERMSG(OXZONE_FLEXI, (L"  MarshalProcessesAsModules: Proc #%d does not match handle 0x%.08x\r\n",
                                                   iProc, pRequest->dwHProc));
                    }
                }
                else
                {
                    // When process is starting or dying we do not want it in the module list
                    // Starting processes still have the load notification pending
                    // Dying processes have already given an unload notification
                    DEBUGGERMSG(OXZONE_ALERT, (L"  MarshalProcessesAsModules: Process ID 0x%08X (%s) not using CoreDll.dll, may be in startup or shutdown.\r\n", pProc->hProc, pProc->lpszProcName));
                }
            }
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
    HRESULT hr = S_OK;

    if (pmod->lpSelf == pmod)
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

        mod.dwBasePointer = static_cast <DWORD> (ZeroPtr (pmod->BasePtr));
        mod.dwModuleSize = pmod->e32.e32_vsize;
        mod.dwRdWrDataStart = pmod->rwLow;
        mod.dwRdWrDataEnd = pmod->rwHigh;
        mod.dwTimeStamp = pmod->e32.e32_timestamp;

        static ModuleSignature ms;

        // Ignore failure from GetModuleDebugInfo, since some modules may have no debug info
        if (SUCCEEDED (GetModuleDebugInfo( pmod, &ms )))
        {
            mod.dwPdbFormat = ms.dwType;
            mod.PdbGuid = ms.guid;
            mod.dwPdbAge = ms.dwAge;
        }

        mod.dwDllHandle = reinterpret_cast <DWORD> (pmod);
        mod.dwInUse = pmod->inuse;
        mod.wFlags = pmod->wFlags;
        mod.bTrustLevel = pmod->bTrustLevel;

        memcpy (mod.rgwRefCount, pmod->refcnt, sizeof (mod.rgwRefCount));
        mod.dwAddr = reinterpret_cast <DWORD> (pmod);

        hr = Write (&mod, sizeof (mod), riOut, cbOut, pbOut);
    }
    else
    {
        DBGRETAILMSG (OXZONE_ALERT, (L"!!WriteModuleData: 0x%08x is not pointing to a module.\r\n", pmod));
        hr = E_FAIL;
    }
    return hr;
}



static HRESULT MarshalModules (FLEXI_REQUEST *pRequest, DWORD &rdwIdx, 
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;
    MODULE *pmod = NULL;
    DWORD dwProcMask = 0xFFFFFFFF;
    struct KDataStruct *pKData;
    DWORD cModElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalModules: Idx:%d, Offset:%d\r\n", rdwIdx, riOut));

    pKData = FetchKData ();
    if (!pKData)
        hr = E_FAIL;

    if (SUCCEEDED (hr))
        pmod = (MODULE *)pKData->aInfo[KINX_MODULES];

    // If the user is interested in culling based on process, then find the process
    // that corresponds to this handle.
    if (pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE)
    {
        DWORD i;
        PROCESS *pProcArray = FetchProcArray();

        // By default fail, unless the process can be found.
        hr = E_FAIL;

        for (i = 0; i < MAX_PROCESSES; i++)
        {
            if (pProcArray[i].dwVMBase)
            {
                if (reinterpret_cast<DWORD>(pProcArray[i].hProc) == pRequest->dwHProc)
                {
                    hr = S_OK;
                    dwProcMask = 1 << pProcArray[i].procnum;
                    break;
                }
            }
        }
    }

    while (SUCCEEDED (hr) && pmod && rdwIdx <= pRequest->Mod.dwEnd)
    {
        if (!(pRequest->dwRequest & FLEXI_FILTER_MODULE_HANDLE) || (reinterpret_cast<DWORD>(pmod) == pRequest->dwHMod))
        {
            if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) || (pmod->inuse & dwProcMask))
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
        pmod = pmod->pMod;
    }

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cModElements : 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalModules: 0x%.08x, Idx:%d, Cnt:%d, Offset:%d\r\n",
        hr, rdwIdx, cModElements, riOut));
    return hr;
}


static HRESULT MarshalOneModule (FLEXI_REQUEST *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++MarshalOneModule\r\n"));
    if (pRequest->dwRequest & FLEXI_FILTER_MODULE_POINTER)
    {
        PPROCESS pProcArray = FetchProcArray ();
        void *pv = reinterpret_cast <void *> (pRequest->dwHMod);

        if (pv >= pProcArray && pv < &pProcArray [MAX_PROCESSES])
            hr = WriteProcessDataAsModule (reinterpret_cast <PPROCESS> (pv), riOut, cbOut, pbOut);
        else
            hr = WriteModuleData (reinterpret_cast <MODULE *> (pv), riOut, cbOut, pbOut);
    }
    else
        hr = E_FAIL;

    DEBUGGERMSG (OXZONE_FLEXI, (L"--MarshalOneModule: hr=0x%08x\r\n", hr));
    return hr;
}


static HRESULT MarshalModuleData (FLEXI_REQUEST *pRequest, DWORD &riOut, 
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

Arguments:
    
    cFieldInfo      - Count of Field Records
    cElements       - Count of Data Elements
    dwElementOffset - Offset of start of Data Elements
    riOut           - Referenced current offset in output buffer.
    cbOut           - Size of the output buffer in bytes.
    pbOut           - Pointer to the start of the output buffer.

Return Values:

    S_OK : Success,
    E_FAIL : General failure,
    E_OUTOFMEMORY : Output buffer is of insufficient size.

--*/
HRESULT WritePTMResponseHeader (DWORD cFieldInfo, DWORD cElements, DWORD dwElementOffset,
    DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WritePTMResponseHeader: FieldCnt:%d, ElementCnt:%d, RVA:0x%.08x, Offset:%d\r\n",
        cFieldInfo, cElements, dwElementOffset, riOut));

    hr = Write((WORD)CB_FLEXIPTM_HEADER, riOut, cbOut, pbOut);
    if (SUCCEEDED (hr))
        hr = Write((WORD)CB_FIELDINFO, riOut, cbOut, pbOut);
    if (SUCCEEDED (hr))
        hr = Write(cFieldInfo, riOut, cbOut, pbOut);
    if (SUCCEEDED (hr))
        hr = Write(cElements, riOut, cbOut, pbOut);
    if (SUCCEEDED (hr))
        hr = Write(dwElementOffset, riOut, cbOut, pbOut);

    DEBUGGERMSG (OXZONE_FLEXI, (L"--WritePTMResponseHeader: 0x%.08x\r\n", hr));
    return hr;
}


struct RespSt
{
    DWORD dwHeaderMask;         // Mask for header request
    DWORD dwDescMask;           // Mask for description request
    DWORD dwDataMask;           // Mask for data request
    DWORD dwAddressMask;        // Mask for address request

    HRESULT (*pfnMarshal) (FLEXI_REQUEST *pRequest, DWORD &riOut, 
        const DWORD cbOut, BYTE *pbOut, DWORD *pcElements);
    HRESULT (*pfnMarshalOne) (FLEXI_REQUEST *pRequest, DWORD &riOut,
        const DWORD cbOut, BYTE *pbOut);

    FLEXI_FIELD_INFO *pFields;
    DWORD cFields;
};


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

    // Deterime based on bitflag in request structure which pieces of data to output
    const BOOL fHeader = (pRequest->dwRequest & prsp->dwHeaderMask)? TRUE : FALSE;
    const BOOL fDesc = (pRequest->dwRequest & prsp->dwDescMask)? TRUE : FALSE;
    const BOOL fData = (pRequest->dwRequest & prsp->dwDataMask)? TRUE : FALSE;

    // Header information to keep track of data.
    DWORD iHeaderStart = riOut;     // Remember where the header is supposed to be.
    DWORD iElementStart = 0;        // Remember the ElementRVA
    DWORD cElements = 0;            // Remember the number of elements
    DWORD cFields = 0;              // Remember the number of fields.
    DWORD iVirtualOut = riOut;      // Keep track of the virtual offset for ElementRVA (header only req)
                                    // This will assume that all data is going out.

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WritePTMResponse, Hdr:%d, Desc:%d, Data:%d, Offset:%d\r\n",
        fHeader, fDesc, fData, riOut));
    
    // If the user asks for header, we need to run through the request once
    // to generate the header information.
    if (SUCCEEDED (hr) && fHeader)
    {
        DWORD iStart = riOut;
        hr = WritePTMResponseHeader (cFields, 0, 0, riOut, cbOut, pbOut);
        if (SUCCEEDED(hr))
        {
            // Update the virtual out.
            iVirtualOut += riOut - iStart;
        }
    }

    if (SUCCEEDED (hr))
    {
        BYTE *pbDescOut = NULL;
        DWORD iDescOut = riOut;
        DWORD iDescStart = riOut;

        // If the user asks for field info, then it is ok to use the actual output buffer.
        if (fDesc)
            pbDescOut = pbOut;
            
        // If the user didn't ask for header, we need to tell the user how many field
        // records are being outputed.  Write a 32-bit count.
        if (!fHeader)
            hr = Write (prsp->cFields, iDescOut, cbOut, pbDescOut);
        if (SUCCEEDED (hr))
            hr = WriteFieldInfo (prsp->pFields, prsp->cFields, iDescOut, cbOut, pbDescOut);
        if (SUCCEEDED (hr))
        {
            cFields = prsp->cFields;

            // Update the virtual out by incrementing by size of data output.
            // this allows the virtual out to be a completely different data
            // value from the actual out.  These two should be the same when
            // doing a complete write out.
            iVirtualOut += iDescOut - iDescStart;

            // If the descriptor is really going out, then remember it.
            if (fDesc)
                riOut = iDescOut;
        }
    }

    if (SUCCEEDED (hr))
    {
        BYTE *pbDataOut = NULL;
        DWORD iDataOut = riOut;
        DWORD iDataStart = riOut;

        // Header only requests use virtual offset.
        // Otherwise, use the actual offset.
        if (fHeader && !fDesc && !fData)
            iElementStart = iVirtualOut;
        else
            iElementStart = iDataOut;

        // If data is being written, then actually write to the buffer.
        if (fData)
            pbDataOut = pbOut;

        // If no header is present, need to tell the user how many data elements
        // are in the output.
        DWORD iElementCnt;
        if (!fHeader)
        {
            iElementCnt = iDataOut;
            hr = Write ((DWORD)0, iDataOut, cbOut, pbDataOut);
        }
        
        if (SUCCEEDED (hr))
        {
            if (prsp->dwAddressMask & pRequest->dwRequest)
            {
                // Get a structure at a caller specified address
                hr = prsp->pfnMarshalOne (pRequest, iDataOut, cbOut, pbDataOut);
                if (SUCCEEDED (hr))
                    cElements = 1;
            }
            else
            {
                // Bulk get structures based on index.
                hr = prsp->pfnMarshal (pRequest, iDataOut, cbOut, pbDataOut, &cElements);
            }
        }

        // Write back the count if no header is present
        if (SUCCEEDED (hr) && !fHeader)
            hr = Write (cElements, iElementCnt, cbOut, pbDataOut); 

        if (SUCCEEDED(hr))
        {
            iVirtualOut += iDataOut - iDataStart;
            if (fData)
                riOut = iDataOut;
        }
    }

    // Patch in header information if necessary.
    if (SUCCEEDED (hr) && fHeader)
        hr = WritePTMResponseHeader (cFields, cElements, iElementStart, iHeaderStart, cbOut, pbOut);

    DEBUGGERMSG (OXZONE_FLEXI, (L"--WritePTMResponse: 0x%.08x\n", hr));
    return hr;
}


static HRESULT WriteRegistersForThread(CPUCONTEXT *pCtx, DWORD &riOut, const DWORD cbOut, BYTE *pbOut)
{
    HRESULT hr = S_OK;
    
#if defined(x86) // note:  size = 16 * sizeof(DWORD)
    OsAxsX86IntContext ctx;

    ctx.Gs = pCtx->TcxGs;
    ctx.Fs = pCtx->TcxFs;
    ctx.Es = pCtx->TcxEs;
    ctx.Ds = pCtx->TcxDs;
    ctx.Edi = pCtx->TcxEdi;
    ctx.Esi = pCtx->TcxEsi;
    ctx.Ebx = pCtx->TcxEbx;
    ctx.Edx = pCtx->TcxEdx;
    ctx.Ecx = pCtx->TcxEcx;
    ctx.Eax = pCtx->TcxEax;
    ctx.Ebp = pCtx->TcxEbp;
    ctx.Eip = pCtx->TcxEip;
    ctx.Cs = pCtx->TcxCs;
    ctx.Eflags = pCtx->TcxEFlags;
    ctx.Esp = pCtx->TcxEsp;
    ctx.Ss = pCtx->TcxSs;

    hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);

#elif defined (SH4) // note:  size = 22 * sizeof(DWORD)
    OsAxsShxIntContext ctx;

    ctx.PR = pCtx->PR;
    ctx.MACH = pCtx->MACH;
    ctx.MACL = pCtx->MACL;
    ctx.GBR = pCtx->GBR;
    ctx.R[0] = pCtx->R0;
    ctx.R[1] = pCtx->R1;
    ctx.R[2] = pCtx->R2;
    ctx.R[3] = pCtx->R3;
    ctx.R[4] = pCtx->R4;
    ctx.R[5] = pCtx->R5;
    ctx.R[6] = pCtx->R6;
    ctx.R[7] = pCtx->R7;
    ctx.R[8] = pCtx->R8;
    ctx.R[9] = pCtx->R9;
    ctx.R[10] = pCtx->R10;
    ctx.R[11] = pCtx->R11;
    ctx.R[12] = pCtx->R12;
    ctx.R[13] = pCtx->R13;
    ctx.R[14] = pCtx->R14;
    ctx.R[15] = pCtx->R15;
    ctx.Fir = pCtx->Fir;
    ctx.Psr = pCtx->Psr;

    hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    
#elif defined (ARM)
    OsAxsArmIntContext ctx;

    ctx.Psr = pCtx->Psr;
    ctx.R[0] = pCtx->R0;
    ctx.R[1] = pCtx->R1;
    ctx.R[2] = pCtx->R2;
    ctx.R[3] = pCtx->R3;
    ctx.R[4] = pCtx->R4;
    ctx.R[5] = pCtx->R5;
    ctx.R[6] = pCtx->R6;
    ctx.R[7] = pCtx->R7;
    ctx.R[8] = pCtx->R8;
    ctx.R[9] = pCtx->R9;
    ctx.R[10] = pCtx->R10;
    ctx.R[11] = pCtx->R11;
    ctx.R[12] = pCtx->R12;
    ctx.Sp = pCtx->Sp;
    ctx.Lr = pCtx->Lr;
    ctx.Pc = pCtx->Pc;

    hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);

#elif defined (MIPS)

#ifdef _MIPS64
    OsAxsMips64IntContext ctx;
#else
    OsAxsMips32IntContext ctx;
#endif

    ctx.IntAt = pCtx->IntAt;

    ctx.IntV0 = pCtx->IntV0;
    ctx.IntV1 = pCtx->IntV1;

    ctx.IntA0 = pCtx->IntA0;
    ctx.IntA1 = pCtx->IntA1;
    ctx.IntA2 = pCtx->IntA2;
    ctx.IntA3 = pCtx->IntA3;

    ctx.IntT0 = pCtx->IntT0;
    ctx.IntT1 = pCtx->IntT1;
    ctx.IntT2 = pCtx->IntT2;
    ctx.IntT3 = pCtx->IntT3;
    ctx.IntT4 = pCtx->IntT4;
    ctx.IntT5 = pCtx->IntT5;
    ctx.IntT6 = pCtx->IntT6;
    ctx.IntT7 = pCtx->IntT7;

    ctx.IntS0 = pCtx->IntS0;
    ctx.IntS1 = pCtx->IntS1;
    ctx.IntS2 = pCtx->IntS2;
    ctx.IntS3 = pCtx->IntS3;
    ctx.IntS4 = pCtx->IntS4;
    ctx.IntS5 = pCtx->IntS5;
    ctx.IntS6 = pCtx->IntS6;
    ctx.IntS7 = pCtx->IntS7;

    ctx.IntT8 = pCtx->IntT8;
    ctx.IntT9 = pCtx->IntT9;

    ctx.IntK0 = pCtx->IntK0;
    ctx.IntK1 = pCtx->IntK1;

    ctx.IntGp = pCtx->IntGp;
    ctx.IntSp = pCtx->IntSp;

    ctx.IntS8 = pCtx->IntS8;

    ctx.IntRa = pCtx->IntRa;

    ctx.IntLo = pCtx->IntLo;
    ctx.IntHi = pCtx->IntHi;

    ctx.IntFir = pCtx->Fir;
    ctx.IntPsr = pCtx->Psr;

    hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);

#endif /*#if defined (MIPS) */
    return hr;
}


static HRESULT WriteRegistersForAllThreads(FLEXI_REQUEST *pRequest, DWORD &riOut, const DWORD cbOut, BYTE *pbOut, DWORD *pcElements)
{
    HRESULT hr = S_OK;

    PROCESS *pProcArray;
    PTHREAD pThread;
    DWORD cThreadElements = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++WriteRegistersForAllThreads: Off:%d\r\n", riOut));

    pProcArray = FetchProcArray();
    if (!pProcArray)
        hr = E_FAIL;

    if ((pRequest->Proc.dwStart > pRequest->Proc.dwEnd) || (pRequest->Thread.dwStart > pRequest->Thread.dwEnd))
        hr = E_INVALIDARG;
    

    DWORD iProc = 0;
    DWORD iThread = 0;
    DWORD iProcSlot = 0;
    for (iProcSlot = 0; SUCCEEDED (hr) && iProcSlot < MAX_PROCESSES && iProc <= pRequest->Proc.dwEnd; iProcSlot++)
    {
        if (pProcArray[iProcSlot].dwVMBase)
        {
            if (!(pRequest->dwRequest & FLEXI_FILTER_PROCESS_HANDLE) || (reinterpret_cast<DWORD>(pProcArray[iProcSlot].hProc) == pRequest->dwHProc))
            {
                if (iProc >= pRequest->Proc.dwStart)
                {
                    pThread = pProcArray[iProcSlot].pTh;
                    while (SUCCEEDED (hr) && pThread && iThread <= pRequest->Thread.dwEnd)
                    {
                        if (!(pRequest->dwRequest & FLEXI_FILTER_THREAD_HANDLE) || (reinterpret_cast<DWORD>(pThread->hTh) == pRequest->dwHThread))
                        {
                            if (iThread >= pRequest->Thread.dwStart)
                            {
                                hr = Write ((DWORD)pThread->hTh, riOut, cbOut, pbOut);
                                if (SUCCEEDED (hr))
                                {
                                    if (! OsAxsIsCurThread (pThread))
                                        hr = WriteRegistersForThread (&pThread->ctx, riOut, cbOut, pbOut);
                                    else
                                        hr = WriteExceptionContext (riOut, cbOut, pbOut, TRUE);
                                }
                                cThreadElements++;    
                            }
                            iThread++;
                        }
                        else
                        {
                            DEBUGGERMSG(OXZONE_FLEXI, (L"  WriteRegistersForAllThreads: Thread 0x%.08x does not match handle 0x%.08x\r\n",
                                                       pThread, pRequest->dwHThread));
                        }
                        pThread = pThread->pNextInProc;
                    }
                }
                iProc++;
            }
            else
            {
                DEBUGGERMSG(OXZONE_FLEXI, (L"  WriteRegistersForAllThreads: Proc #%d does not match proc handle 0x%.08x\r\n",
                                           iProcSlot, pRequest->dwHProc));
            }
        }
    }

    if (pcElements)
        *pcElements = (SUCCEEDED (hr))? cThreadElements : 0;

        DEBUGGERMSG (OXZONE_FLEXI, (L"--WriteRegistersForAllThreads: 0x%.08x, Off:%d, Cnt:%d\r\n", hr, riOut, cThreadElements));

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
    DWORD cbOutBuffer = *pcbOutBuffer;
    DWORD iOutBuffer = 0;

    DEBUGGERMSG (OXZONE_FLEXI, (L"++GetFPTMI: ReplyBuf = %d\r\n", cbOutBuffer));

    if (!pRequest || !pcbOutBuffer)
        hr = E_INVALIDARG;

    if (SUCCEEDED (hr) && (pRequest->dwRequest & FLEXI_REQ_PROCESS_ALL))
    {
        struct RespSt rsp = {FLEXI_REQ_PROCESS_HDR, FLEXI_REQ_PROCESS_DESC, FLEXI_REQ_PROCESS_DATA, FLEXI_FILTER_PROCESS_POINTER,
            MarshalProcessData, MarshalOneProcess, ProcessDesc, C_PROCESSFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
    }
    if (SUCCEEDED (hr) && (pRequest->dwRequest & FLEXI_REQ_THREAD_ALL))
    {
        struct RespSt rsp = {FLEXI_REQ_THREAD_HDR, FLEXI_REQ_THREAD_DESC, FLEXI_REQ_THREAD_DATA, FLEXI_FILTER_THREAD_POINTER,
            MarshalThreadData, MarshalOneThread, ThreadDesc, C_THREADFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
    }
    if (SUCCEEDED (hr) && (pRequest->dwRequest & FLEXI_REQ_MODULE_ALL))
    {
        struct RespSt rsp = {FLEXI_REQ_MODULE_HDR, FLEXI_REQ_MODULE_DESC, FLEXI_REQ_MODULE_DATA, FLEXI_FILTER_MODULE_POINTER,
            MarshalModuleData, MarshalOneModule, ModuleDesc, C_MODULEFIELDS};
        hr = WritePTMResponse (pRequest, &rsp, iOutBuffer, cbOutBuffer, pbOutBuffer);
    }
    else if (SUCCEEDED(hr) && (pRequest->dwRequest & FLEXI_REQ_CONTEXT_ALL))
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
    }
    
    if (SUCCEEDED (hr))
        *pcbOutBuffer = iOutBuffer;
    
    DEBUGGERMSG (OXZONE_FLEXI || FAILED (hr), (L"--GetFPTMI: hr=0x%08x, bytes=%d\r\n", hr, iOutBuffer));
    return hr;
}

