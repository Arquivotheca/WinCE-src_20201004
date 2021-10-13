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
//
//    ddxexcept.cpp - DDx general exception analysis
//


#include "osaxs_p.h"
#include "DwCeDump.h"
#include "diagnose.h"


void DumpDDxLog(void);

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
DiagnoseStackOverflow (
                       DWORD dwExceptionCode
                       )
{
    UINT    index = g_nDiagnoses;
    DDX_DIAGNOSIS_ID diag;


    // Check to see if this is an instance of a known diagnosis

    diag.Type       = Type_Stack;
    diag.SubType    = (dwExceptionCode == STATUS_STACK_OVERFLOW) ? Stack_Overflow : Stack_Overflow_Warning;
    diag.pProcess   = pCurThread ? pCurThread->pprcOwner : NULL;
    diag.dwThreadId = pCurThread ? pCurThread->dwId : NULL;
    diag.dwData     = NULL;

    if (IsKnownDiagnosis(&diag))
        return DDxResult_Duplicate;

    AddPersistentDiagnosis(&diag);


    // Record the diagnosis

    BeginDDxLogging();

    g_nDiagnoses++;

    g_ceDmpDiagnoses[index].Type       = diag.Type;
    g_ceDmpDiagnoses[index].SubType    = diag.SubType;
    g_ceDmpDiagnoses[index].Scope      = Scope_Application; // TODO: Is Kernel thread?  Then scope is System
    g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
    g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
    g_ceDmpDiagnoses[index].Confidence = Depth_Cause;
    g_ceDmpDiagnoses[index].pProcess   = (ULONG32) diag.pProcess;
    g_ceDmpDiagnoses[index].ProcessId  = (ULONG32)(diag.pProcess ? diag.pProcess->dwId : NULL);
    g_ceDmpDiagnoses[index].pThread    = (ULONG32) pCurThread;

    AddAffectedThread(pCurThread);


    // Description

    // TODO: Need some actual values here:
    //       - Stack limit on this device
    //       - Stack range for the thread

    DWORD dwAddr = DD_ExceptionState.exception->ExceptionInformation[1];

    if (dwExceptionCode == STATUS_STACK_OVERFLOW_WARNING)
    {
        ddxlog(L"The %s thread 0x%08x is within one page of exceeding the system defined stack size limit.\n", GetThreadProcName(pCurThread), pCurThread);
        ddxlog(L"Lowest stack address: 0x%08x\n", dwAddr);
    }
    else
    {
        ddxlog(L"The %s thread 0x%08x has exceeded the system defined stack size limit.\n", GetThreadProcName(pCurThread), pCurThread);
        ddxlog(L"Lowest stack address: 0x%08x\n", dwAddr);
    }


    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();


    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, NULL, NULL);

    if (FAILED(hr))
        return DDxResult_Error;

    return DDxResult_Positive;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
IsHandle (
          DWORD dwVal
          )
{
    PHDATA phdata = 0;
    PHNDLTABLE phdtbl = pActvProc->phndtbl;

    if (phdtbl)
    {
        phdata = h2pHDATA((HANDLE) dwVal, phdtbl);

        if (phdata)
        {
            LPCWSTR pwszType = L"unknown type";

            if (phdata->pci)
            {
                switch (phdata->pci->type)
                {
                case SH_CURTHREAD:
                    pwszType = L"Thread";
                    break;
                case SH_CURPROC:
                    pwszType = L"Process";
                    break;
                case HT_EVENT :
                    pwszType = L"Event";
                    break;
                case HT_MUTEX :
                    pwszType = L"Mutex";
                    break;
                case HT_FILE:
                    pwszType = L"File";
                    break;
                case HT_FIND:
                    pwszType = L"FindFirst";
                    break;
                case HT_DBFILE:
                    pwszType = L"DataBase";
                    break;
                case HT_DBFIND:
                    pwszType = L"Database find";
                    break;
                case HT_SOCKET:
                    pwszType = L"Socket";
                    break;
                case HT_CRITSEC:
                    pwszType = L"Crtical section";
                    break;
                case HT_SEMAPHORE:
                    pwszType = L"Semaphore";
                    break;
                case HT_FSMAP:
                    pwszType = L"Mapped file";
                    break;
                case HT_WNETENUM:
                    pwszType = L"Net Resource Enumeration";
                    break;
                case HT_AFSVOLUME:
                    pwszType = L"Filesys volume";
                    break;
                case HT_NAMESPACE:
                    pwszType = L"Namespace";
                    break;
                default:
                    break;

                }
            }

            ddxlog(L"  Handle - %s\n", pwszType);
            ddxlog(L"     Object address: 0x%08x\n", phdata->pvObj);
            ddxlog(L"     Ref count:      %d\n", phdata->dwRefCnt);

            if (phdata->pName)
                ddxlog(L"     Name:           %s\n", phdata->pName->name);

            return TRUE;
        }
    }

    return FALSE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
Eval (
      LPCWSTR wszName,
      DWORD dw
      )
{
    ddxlog(L" %-12s: 0x%08x\n", wszName, dw);

    //if (IsOnStack(dw))
    //{
    //  // If this points to the stack continue evaluation 
    //  // of the value at this stack address.

    //  dw = *((DWORD*)dw);
    //}

    if (IsHandle(dw))
    {
    }
    else if (IsHeapItem(dw))
    {
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
EvaluateRegisters (
                   void
                   )
{
    CONTEXT * pExceptionCtx = DD_ExceptionState.context;

#if defined (ARM)

    Eval(L"R0", pExceptionCtx->R0);
    Eval(L"R1", pExceptionCtx->R1);
    Eval(L"R2", pExceptionCtx->R2);
    Eval(L"R3", pExceptionCtx->R3);
    Eval(L"R4", pExceptionCtx->R4);
    Eval(L"R5", pExceptionCtx->R5);
    Eval(L"R6", pExceptionCtx->R6);
    Eval(L"R7", pExceptionCtx->R7);
    Eval(L"R8", pExceptionCtx->R8);
    Eval(L"R9", pExceptionCtx->R9);
    Eval(L"R10", pExceptionCtx->R10);
    Eval(L"R11", pExceptionCtx->R11);
    Eval(L"R12", pExceptionCtx->R12);

    //Eval(L"Sp", pExceptionCtx->Sp);
    //Eval(L"Lr", pExceptionCtx->Lr);
    //Eval(L"Pc", pExceptionCtx->Pc);
    //Eval(L"Psr", pExceptionCtx->Psr);


    // TODO: Fp registers??

#elif defined (MIPS)


    Eval(L"IntAt", (DWORD) pExceptionCtx->IntAt);

    Eval(L"IntV0", (DWORD) pExceptionCtx->IntV0);
    Eval(L"IntV1", (DWORD) pExceptionCtx->IntV1);

    Eval(L"IntA0", (DWORD) pExceptionCtx->IntA0);
    Eval(L"IntA1", (DWORD) pExceptionCtx->IntA1);
    Eval(L"IntA2", (DWORD) pExceptionCtx->IntA2);
    Eval(L"IntA3", (DWORD) pExceptionCtx->IntA3);

    Eval(L"IntT0", (DWORD) pExceptionCtx->IntT0);
    Eval(L"IntT1", (DWORD) pExceptionCtx->IntT1);
    Eval(L"IntT2", (DWORD) pExceptionCtx->IntT2);
    Eval(L"IntT3", (DWORD) pExceptionCtx->IntT3);
    Eval(L"IntT4", (DWORD) pExceptionCtx->IntT4);
    Eval(L"IntT5", (DWORD) pExceptionCtx->IntT5);
    Eval(L"IntT6", (DWORD) pExceptionCtx->IntT6);
    Eval(L"IntT7", (DWORD) pExceptionCtx->IntT7);

    Eval(L"IntS0", (DWORD) pExceptionCtx->IntS0);
    Eval(L"IntS1", (DWORD) pExceptionCtx->IntS1);
    Eval(L"IntS2", (DWORD) pExceptionCtx->IntS2);
    Eval(L"IntS3", (DWORD) pExceptionCtx->IntS3);
    Eval(L"IntS4", (DWORD) pExceptionCtx->IntS4);
    Eval(L"IntS5", (DWORD) pExceptionCtx->IntS5);
    Eval(L"IntS6", (DWORD) pExceptionCtx->IntS6);
    Eval(L"IntS7", (DWORD) pExceptionCtx->IntS7);

    Eval(L"IntT8", (DWORD) pExceptionCtx->IntT8);
    Eval(L"IntT9", (DWORD) pExceptionCtx->IntT9);

    Eval(L"IntK0", (DWORD) pExceptionCtx->IntK0);
    Eval(L"IntK1", (DWORD) pExceptionCtx->IntK1);

    Eval(L"IntGp", (DWORD) pExceptionCtx->IntGp);
    Eval(L"IntSp", (DWORD) pExceptionCtx->IntSp);

    Eval(L"IntS8", (DWORD) pExceptionCtx->IntS8);

    /*Eval(L"IntRa", (DWORD) pExceptionCtx->IntRa);

    Eval(L"IntLo", (DWORD) pExceptionCtx->IntLo);
    Eval(L"IntHi", (DWORD) pExceptionCtx->IntHi);

    Eval(L"Fir", (DWORD) pExceptionCtx->Fir);
    Eval(L"Psr", (DWORD) pExceptionCtx->Psr);*/

#elif defined (SHx)

    Eval(L"R0", pExceptionCtx->R0);
    Eval(L"R1", pExceptionCtx->R1);
    Eval(L"R2", pExceptionCtx->R2);
    Eval(L"R3", pExceptionCtx->R3);
    Eval(L"R4", pExceptionCtx->R4);
    Eval(L"R5", pExceptionCtx->R5);
    Eval(L"R6", pExceptionCtx->R6);
    Eval(L"R7", pExceptionCtx->R7);
    Eval(L"R8)", pExceptionCtx->R8);
    Eval(L"R9", pExceptionCtx->R9);
    Eval(L"R10", pExceptionCtx->R10);
    Eval(L"R11", pExceptionCtx->R11);
    Eval(L"R12", pExceptionCtx->R12);
    Eval(L"R13", pExceptionCtx->R13);
    Eval(L"R14", pExceptionCtx->R14);
    Eval(L"R15", pExceptionCtx->R15);

    //Eval(L"PR", pExceptionCtx->PR);
    //Eval(L"MACH", pExceptionCtx->MACH);
    //Eval(L"MACL", pExceptionCtx->MACL);
    //Eval(L"GBR", pExceptionCtx->GBR);
    //Eval(L"Fir", pExceptionCtx->Fir);
    //Eval(L"Psr", pExceptionCtx->Psr);

#elif defined (x86)

    Eval(L"Edi", pExceptionCtx->Edi);
    Eval(L"Esi", pExceptionCtx->Esi);
    Eval(L"Ebx", pExceptionCtx->Ebx);
    Eval(L"Edx", pExceptionCtx->Edx);
    Eval(L"Ecx", pExceptionCtx->Ecx);
    Eval(L"Eax", pExceptionCtx->Eax);
    Eval(L"Ebp", pExceptionCtx->Ebp);
    Eval(L"Eip", pExceptionCtx->Eip);
    Eval(L"Esp", pExceptionCtx->Esp);

#endif

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LPCWSTR
GetExceptionTypeStr(
                    DWORD dwExceptionCode
                    )
{
    switch (dwExceptionCode)
    {
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        return L"Divide by Zero";
    default:
    case STATUS_ACCESS_VIOLATION:
        return L"Access Violation";
    }
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
GenerateExceptionDiagnosis (
                            DWORD dwExceptionCode,
                            DWORD dwEA,
                            DWORD dwPC
                            )
{
    THREAD* pth   = pCurThread;
    UINT    index = g_nDiagnoses;

    // Record the diagnosis

    BeginDDxLogging();

    g_nDiagnoses++;


    switch (dwExceptionCode)
    {
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        g_ceDmpDiagnoses[index].SubType = Ex_Div0;
        break;

    case STATUS_ACCESS_VIOLATION:
        g_ceDmpDiagnoses[index].SubType = (dwEA == NULL) ? Ex_Null_Deref : Ex_AV;
        break;

    default:
        g_ceDmpDiagnoses[index].SubType = Ex_Unknown;
        break;
    }

    PPROCESS pprc = (pth ? pth->pprcOwner : NULL);

    g_ceDmpDiagnoses[index].Type       = Type_Exception;
    g_ceDmpDiagnoses[index].Scope      = Scope_Application;
    g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
    g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
    g_ceDmpDiagnoses[index].Confidence = Confidence_Certain;
    g_ceDmpDiagnoses[index].pProcess   = (ULONG32) pprc;
    g_ceDmpDiagnoses[index].ProcessId  = (ULONG32)(pprc ? pprc->dwId : NULL);
    g_ceDmpDiagnoses[index].pThread   = (ULONG32) pth;


    ddxlog(L"An unhandled exception occurred at the address 0x%08x\r\n", dwEA);
    ddxlog(L"Exception Code: 0x%08x (%s)\r\n", dwExceptionCode, GetExceptionTypeStr(dwExceptionCode));
    ddxlog(L"Current PC:     0x%08x\r\n", dwPC);  //TODO: Find in module

    // TODO: Improve: EvaluateRegisters();
    // TODO: PERF:    EvaluateStack

    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();

    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pth, pth->pprcActv);

    return (SUCCEEDED(hr));
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
GenerateFreeModuleDiagnosis (
                             DWORD dwAddr,
                             FREE_MOD_DATA* pFreeMod
                             )
{
    THREAD* pth   = pCurThread;
    UINT    index = g_nDiagnoses;
    DDX_DIAGNOSIS_ID diag;

    if (!pFreeMod)
        return FALSE;

    // Check to see if this is an instance of a known diagnosis

    diag.Type       = Type_Exception;
    diag.SubType    = Ex_FreeModule;
    diag.pProcess   = pth ? pth->pprcOwner : NULL;
    diag.dwThreadId = pth ? pth->dwId : NULL;
    diag.dwData     = (DWORD) pFreeMod->pMod;

    if (IsKnownDiagnosis(&diag))
        return FALSE;

    AddPersistentDiagnosis(&diag);


    // Record the diagnosis

    BeginDDxLogging();

    g_nDiagnoses++;

    PPROCESS pprc = (pth ? pth->pprcOwner : NULL);

    g_ceDmpDiagnoses[index].Type       = Type_Exception;
    g_ceDmpDiagnoses[index].SubType    = Ex_FreeModule;
    g_ceDmpDiagnoses[index].Scope      = Scope_Application;
    g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
    g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
    g_ceDmpDiagnoses[index].Confidence = Confidence_Certain;
    g_ceDmpDiagnoses[index].pProcess   = (ULONG32)pprc;
    g_ceDmpDiagnoses[index].ProcessId  = (ULONG32)(pprc ? pprc->dwId : NULL);
    g_ceDmpDiagnoses[index].pThread   = (ULONG32) pth;


    ddxlog(L"The current PC (0x%08x) is an address in a module that has been unloaded\r\n", dwAddr);
    ddxlog(L"from the owner process\r\n");
    ddxlog(L"\r\n");
    ddxlog(L" Module                       : %s\r\n",     pFreeMod->wszName);
    ddxlog(L" Module Ptr                   : 0x%08x\r\n", pFreeMod->pMod);
    ddxlog(L" Active Process (owns module) : %s\r\n",     pth->pprcActv ? pth->pprcActv->lpszProcName : L"<unknonw>");
    ddxlog(L" Owner Process (owns thread)  : %s\r\n",     pth->pprcOwner ? pth->pprcOwner->lpszProcName : L"<unknonw>");
    ddxlog(L" Time when module released    : 0x%08x\r\n", pFreeMod->dwTimeStamp);
    ddxlog(L" Current time                 : 0x%08x\r\n", ((PNKGLOBAL)(g_pKData->pNk))->dwCurMSec);


    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();

    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pth, pth->pprcActv);

    return (SUCCEEDED(hr));
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
IsAddrInFreeModule (
                    DWORD dwAddr
                    )
{
    FREE_MOD_DATA FreeMod = {0};

    if (IsAddrInModule(pVMProc, dwAddr))
    {
        return FALSE;
    }

    if ((pActvProc != pVMProc) &&
        IsAddrInModule(pActvProc, dwAddr))
    {
        return FALSE;
    }

    if (NKKernelLibIoControl != NULL && g_hProbeDll)
    {
        if (NKKernelLibIoControl(
            g_hProbeDll, 
            IOCTL_DDX_PROBE_IS_FREE_MOD, 
            (LPVOID) dwAddr, sizeof(DWORD), 
            (LPVOID) &FreeMod, sizeof(FREE_MOD_DATA), 
            NULL))
        {
            return GenerateFreeModuleDiagnosis(dwAddr, &FreeMod);
        } 
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
DiagnoseException (
                   DWORD dwExceptionCode
                   )
{
    DDxResult_e res = DDxResult_Inconclusive;

    DWORD dwPC = CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context);
    DWORD dwEA = (DWORD) DD_ExceptionState.exception->ExceptionAddress;

    // TODO:  Need to evaluate perf hit
    res = DiagnoseHeap(dwExceptionCode);

    if (res == DDxResult_Positive)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!Found heap corruption while diagnosing AV\r\n"));
        return res;
    }

    // Check for calls into unloaded modules
    if (dwPC == dwEA)
    {
        if (IsAddrInFreeModule(dwPC))
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!Found address in free module\r\n"));
            return DDxResult_Positive;
        }
    }

    GenerateExceptionDiagnosis(dwExceptionCode, dwEA, dwPC);

    return DDxResult_Positive;
}
