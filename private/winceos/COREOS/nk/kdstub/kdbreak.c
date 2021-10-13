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

    kdbreak.c

Abstract:

    This module implements machine dependent functions to add and delete
    breakpoints from the kernel debugger breakpoint table.

Revision History:

--*/

#include "kdp.h"

// The following variables are global for a reason.  Do not move them to the stack or bad things happen
// when flushing instructions.
KDP_BREAKPOINT_TYPE Content;
KDP_BREAKPOINT_TYPE ReadBack;

BREAKPOINT_ENTRY g_aBreakpointTable [BREAKPOINT_TABLE_SIZE];


// This int holds the total number of distinct SW Code breakpoints that are set or need to be set
int g_nTotalNumDistinctSwCodeBps;

BOOL g_fForceTestRomBp = FALSE; // Warning: test purpose only


HRESULT KdpInstantiateSwBreakpoint(ULONG Idx)
{
    MEMADDR addr;
    HRESULT hr;

    DEBUGGERMSG(KDZONE_SWBP, (L" KdpInstantiateSwBP %d\r\n", Idx));
    
    SetVirtAddr(&addr, g_aBreakpointTable[Idx].pVM,
            g_aBreakpointTable[Idx].Address);
    
    hr = DD_AddVMBreakpoint(&addr,
                            g_aBreakpointTable[Idx].ThreadID,
                            g_aBreakpointTable[Idx].f16Bit,
                            &g_aBreakpointTable[Idx].BpHandle);
    if (FAILED(hr))
        goto Error;

    hr = S_OK;
Exit:
    DEBUGGERMSG(KDZONE_SWBP, (L"-KdpInstantiateBP, bph=%08X\r\n", g_aBreakpointTable[Idx].BpHandle));
    return hr;
Error:
    DEBUGGERMSG(KDZONE_ALERT, (L" KdpInstantiateSwBP %d failed %08X, (%08X::%08X)\r\n",
                               Idx, hr,
                               g_aBreakpointTable[Idx].pVM,
                               g_aBreakpointTable[Idx].Address));
    goto Exit;
}


/*++

Routine Description:

    This routine adds an entry to the breakpoint table and returns a handle
    to the breakpoint table entry.

Arguments:

    [in] pVM - VM for the address.
    [in] Address - Supplies the address where to set the breakpoint.

Return Value:

    A value of zero is returned if the specified address is already in the
    breakpoint table, there are no free entries in the breakpoint table, the
    specified address is not correctly aligned, or the specified address is
    not valid. Otherwise, the index of the assigned breakpoint table entry
    plus one is returned as the function value.

--*/

ULONG KdpAddBreakpoint (PPROCESS pVM, ULONG ThreadID, PVOID Address, BOOL Mode16Bit)
{
    HRESULT hr;
    int bphRet = KD_BPHND_INVALID_GEN_ERR;
    ULONG Index;
    ULONG i;

    int bphFreeEntry = KD_BPHND_INVALID_GEN_ERR;
    
    DEBUGGERMSG (KDZONE_SWBP, (L"++KdpAddBreakpoint (%08X, %08X, 0x%08X)\r\n", pVM, ThreadID, Address));

    // Find an empty spot or identical entry in the table of BP
    for (Index = 0; (Index < BREAKPOINT_TABLE_SIZE) && (bphRet == KD_BPHND_INVALID_GEN_ERR); Index++)
    {
        if ((bphFreeEntry == KD_BPHND_INVALID_GEN_ERR) && !(g_aBreakpointTable [Index].wRefCount))
        {
            bphFreeEntry = Index + 1; // remember 1st free entry (+1 to convert index to handle)
        }
        if (g_aBreakpointTable [Index].wRefCount &&
            (g_aBreakpointTable[Index].ThreadID == ThreadID) &&
            (g_aBreakpointTable [Index].Address == Address) &&
            (g_aBreakpointTable[Index].pVM == pVM))
        {
            // Then we have a dup entry
            DEBUGGERMSG (KDZONE_ALERT, (L"  KdpAddBreakpoint: Dup - Found existing BP (0x%08X) - should not happening if host-side aliasing / ref count! Risk of improper single steping!!!!\r\n", Address));
            bphRet = Index + 1;
            ++(g_aBreakpointTable [Index].wRefCount);
            goto Exit;
        }
    }

    if (bphFreeEntry == KD_BPHND_INVALID_GEN_ERR)
    {
        bphRet = KD_BPHND_INVALID_GEN_ERR;
        goto Error;
    }

    i = bphFreeEntry - 1;

    memset(&g_aBreakpointTable[i], 0, sizeof(g_aBreakpointTable[i]));
    g_aBreakpointTable[i].wRefCount = 1;
    g_aBreakpointTable[i].Address = Address;
    g_aBreakpointTable[i].pVM = pVM;
    g_aBreakpointTable[i].ThreadID = ThreadID;
    g_aBreakpointTable[i].f16Bit = Mode16Bit? TRUE : FALSE;

    hr = KdpInstantiateSwBreakpoint(i);
    if (SUCCEEDED(hr))
    {
        bphRet = bphFreeEntry;
    }
    else
    {
        switch (hr)
        {
        case KDBG_E_ROMBPKERNEL:   bphRet = KD_BPHND_ROMBP_IN_KERNEL; break;
        case KDBG_E_ROMBPCOPY:     bphRet = KD_BPHND_ERROR_COPY_FAILED; break;
        case KDBG_E_ROMBPRESOURCE: bphRet = KD_BPHND_ROMBP_ERROR_INSUFFICIENT_PAGES; break;
        default:                   bphRet = KD_BPHND_INVALID_GEN_ERR; break;
        }
        memset(&g_aBreakpointTable[i], 0, sizeof(g_aBreakpointTable[i]));
        goto Error;
    }

Exit:
    DEBUGGERMSG (KDZONE_SWBP, (L"--KdpAddBreakpoint (%08x)\r\n", bphRet));
    return bphRet;
Error:
    DBGRETAILMSG(KDZONE_ALERT, (L"!!KDPAddBP, Failed %08X\r\n", bphRet));
    goto Exit;
}


/*++

Routine Description:

    This routine deletes an entry from the breakpoint table.

--*/

BOOL KdpDeleteBreakpoint (ULONG Index)
{
    KD_BPINFO bpInfo;
    BOOL fRet;

    DEBUGGERMSG (KDZONE_SWBP, (L"++KdpDeleteBreakpoint %i\r\n", Index));

    // If the specified handle is not valid, then return FALSE.
    if ((Index >= BREAKPOINT_TABLE_SIZE) ||
        !(g_aBreakpointTable[Index].wRefCount))
    {
        DEBUGGERMSG (KDZONE_SWBP, (L"  KdpDeleteBreakpoint: Invalid handle\r\n"));
        fRet = FALSE;
        goto Error;
    }

    --g_aBreakpointTable[Index].wRefCount;
    if (!g_aBreakpointTable[Index].wRefCount)
    {
        if (g_aBreakpointTable[Index].fHardware)
        {
            bpInfo.nVersion = 1;
            bpInfo.ulHandle = (ULONG)g_aBreakpointTable[Index].BpHandle;
            bpInfo.ulFlags = 0;
            bpInfo.ulAddress = 0;
            DD_IoControl(KD_IOCTL_CLEAR_CBP, &bpInfo, sizeof(bpInfo));
        }
        else
        {
            HRESULT hr;
            DEBUGGERMSG (KDZONE_SWBP, (L"  KdpDeleteBreakpoint call HD breakpoints\r\n"));
            hr = DD_DeleteVMBreakpoint(g_aBreakpointTable[Index].BpHandle);
            KDASSERT(SUCCEEDED(hr));
            if (FAILED(hr))
            {
                DEBUGGERMSG(KDZONE_ALERT, (L" KdpDelBP BP%d, bph=%08X\r\n",
                                           Index,
                                           g_aBreakpointTable[Index].BpHandle));
            }
        }

        memset(&g_aBreakpointTable[Index], 0, sizeof(g_aBreakpointTable[Index]));
    }

    fRet = TRUE;
Exit:
    DEBUGGERMSG (KDZONE_SWBP, (L"--KdpDeleteBreakpoint (%d)\r\n", (int) fRet));
    return fRet;
Error:
    goto Exit;
}


/*++

Routine Description:

    Call KdpDeleteBreakpoint on every breakpoint handle and ignore the result.  If all
    goes well, this should remove all breakpoints in the system and allow it to
    execute normally.

--*/

VOID KdpDeleteAllBreakpoints (VOID)
{
    ULONG i;

    DEBUGGERMSG (KDZONE_SWBP, (L"++KdpDeleteAllBreakpoints\r\n"));
    // Brute force delete every breakpoint by forcing them all down to
    // 0 refcount.
    for (i = 0; i < BREAKPOINT_TABLE_SIZE; ++i)
    {
        while (g_aBreakpointTable[i].wRefCount > 0)
        {
            KdpDeleteBreakpoint(i);
        }
    }
    memset (g_aBreakpointTable, 0, sizeof (g_aBreakpointTable)); // clean up (necessary for 1st time init)
    g_nTotalNumDistinctSwCodeBps = 0;
    DEBUGGERMSG(KDZONE_SWBP, (L"--KdpDeleteAllBreakpoints\r\n"));
}


BOOL KdpFindUnusedBp(ULONG *idx)
{
    ULONG i;
    for (i = 0; i < BREAKPOINT_TABLE_SIZE; ++i)
    {
        if (g_aBreakpointTable[i].wRefCount == 0)
        {
            *idx = i;
            return TRUE;
        }
    }
    return FALSE;
}

ULONG KdpSetHwCodeBp(ULONG Address)
{
    KD_BPINFO bpInfo = {0};
    ULONG i;

    for (i = 0; i < BREAKPOINT_TABLE_SIZE; ++i)
    {
        if (g_aBreakpointTable[i].wRefCount &&
            g_aBreakpointTable[i].fHardware &&
            g_aBreakpointTable[i].Address == (void *)Address)
        {
            ++g_aBreakpointTable[i].wRefCount;
            return i + 1;
        }
    }

    if (KdpFindUnusedBp(&i))
    {
        bpInfo.nVersion = 1;
        bpInfo.ulAddress = Address; // TODO: Use physical address or switch VA HW BP with context
        bpInfo.ulFlags = 0;
        if (DD_IoControl (KD_IOCTL_SET_CBP, &bpInfo, sizeof (KD_BPINFO)))
        {
            memset(&g_aBreakpointTable[i], 0, sizeof(g_aBreakpointTable[i]));
            g_aBreakpointTable[i].Address = (void *) Address;
            g_aBreakpointTable[i].BpHandle = (void *) bpInfo.ulHandle;
            g_aBreakpointTable[i].wRefCount = 1;
            g_aBreakpointTable[i].fHardware = TRUE;
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: Set  Hard CBP Address = %08X dwBpHandle=%08X\r\n", Address, i + 1));
            return i + 1;
        }
    }

    return 0;
}


VOID
KdpManipulateBreakPoint(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )
/*++

Routine Description:

    This function is called in response of a manipulate breakpoint
    message.  Its function is to query/write/set a breakpoint
    and return a handle to the breakpoint.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/
{    
    DBGKD_MANIPULATE_BREAKPOINT *a = &pdbgkdCmdPacket->u.ManipulateBreakPoint;
    STRING MessageHeader;
    BOOL fSuccess = FALSE;
    DWORD i;
    HRESULT hr = E_FAIL;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpManipulateBreakPoint\r\n"));

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    if (a->dwMBpFlags & DBGKD_MBP_FLAG_DATABP)
    {
        DBGKD_DATA_BREAKPOINT_ELEM1 *b = (DBGKD_DATA_BREAKPOINT_ELEM1 *)AdditionalData->Buffer;

        if (AdditionalData->Length != (sizeof(*b) * a->dwBpCount))
        {
            pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: Length mismatch\r\n"));
            goto RespondAndExit;
        }

        //NOTE: PB's kdstub should never send more than 1 data bp.
        //But, we handle more than 1 bp, just in case we find this functionality useful in the future.
        for (i = 0; i < a->dwBpCount; ++i)            
        {
            if(a->dwMBpFlags & DBGKD_MBP_FLAG_SET)
            {
                PPROCESS pVM = NULL;

                if (a->dwProcessHandle)
                {
                    pVM = DD_HandleToProcess((HANDLE)a->dwProcessHandle);
                }

                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: DATA BP 0x%08x::0x%08X 0x%08x MBPFlag=%08x, bpFlags=%08X dwBpHandle=%08X\r\n", pVM, b[i].dwAddress, b[i].dwAddressMask, a->dwMBpFlags, b[i].wBpFlags, b[i].dwBpHandle));

                hr = DD_AddDataBreakpoint(pVM, b[i].dwAddress, b[i].dwAddressMask, !(b[i].wBpFlags & DBGKD_BP_FLAG_SOFTWARE), (b[i].wBpFlags & DBGKD_DBP_FLAG_READ), (b[i].wBpFlags & DBGKD_DBP_FLAG_WRITE), &b[i].dwBpHandle);
            }
            else
            {
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: DATABP handle: 0x%x\r\n", b[i].dwBpHandle));
                hr = DD_DeleteDataBreakpoint(b[i].dwBpHandle);
            }                 

            pdbgkdCmdPacket->dwReturnStatus = (DWORD) hr;
            fSuccess = (SUCCEEDED(hr) ? TRUE : FALSE);

            if(FAILED(hr))
            {
                //FOR BC: we return hr in dwBpHandle as well as dwReturnStatus
                b[i].dwBpHandle = (DWORD)hr;
            }
            
        }        

    }
    else
    {
        DBGKD_CODE_BREAKPOINT_ELEM1 *b = (DBGKD_CODE_BREAKPOINT_ELEM1 *) AdditionalData->Buffer;

        if (AdditionalData->Length != (sizeof(*b) * a->dwBpCount))
        {
            pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: Length mismatch\r\n"));
            goto RespondAndExit;
        }
        
        for (i = 0; i < a->dwBpCount; ++i)
        {
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: Address = %08X Flags=%08X dwBpHandle=%08X\r\n", b[i].dwAddress, b[i].wBpFlags, b[i].dwBpHandle));
            if (a->dwMBpFlags & DBGKD_MBP_FLAG_SET)
            {
                b[i].dwBpHandle = 0;
                if ((b[i].wBpFlags != DBGKD_BP_FLAG_SOFTWARE) && OEMKDIoControl)
                    b[i].dwBpHandle = KdpSetHwCodeBp(b[i].dwAddress);
                
                if (b[i].dwBpHandle == 0)
                {
                    PPROCESS pVM = pVMProc;

                    if (a->dwProcessHandle)
                    {
                        pVM = DD_HandleToProcess((HANDLE)a->dwProcessHandle);
                    }

                    DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: VM=%08X, pVMProc=%08X\r\n,",
                                    pVM, pVMProc));
                    pVM = DD_GetProperVM(pVM, (void *)b[i].dwAddress);
                    DEBUGGERMSG(KDZONE_BREAK, (L"  KdpManipulateBreakPoint: ProperVM=%08X Addr=%08X\r\n", pVM, b[i].dwAddress));

                    b[i].dwBpHandle = KdpAddBreakpoint(pVM, 
                                                       a->dwThreadHandle,
                                                       (void*)b[i].dwAddress,
                                                       b[i].wBpExecMode == DBGKD_EXECMODE_16BIT);
                }
                fSuccess = fSuccess || b[i].dwBpHandle;
            }
            else
            {
                fSuccess = KdpDeleteBreakpoint(b[i].dwBpHandle - 1) || fSuccess;
            }
        }

        if (fSuccess)
        {
            pdbgkdCmdPacket->dwReturnStatus = (DWORD) (fSuccess? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);
        }
        
    }

RespondAndExit:
    a->NbBpAvail.dwNbHwCodeBpAvail = 0; // TODO: Get this from OAL
    a->NbBpAvail.dwNbSwCodeBpAvail = BREAKPOINT_TABLE_SIZE - g_nTotalNumDistinctSwCodeBps;
    a->NbBpAvail.dwNbHwDataBpAvail = 0; // TODO: Get this from OAL
    a->NbBpAvail.dwNbSwDataBpAvail = 0;
    DEBUGGERMSG (KDZONE_BREAK || !fSuccess, (L"--KdpManipulateBreakPoint Status = 0x%08x\r\n", pdbgkdCmdPacket->dwReturnStatus));
    KdpSendKdApiCmdPacket (&MessageHeader, AdditionalData);
    return;
}


DWORD KdpFindBreakpoint(void* pvOffset)
{
    DWORD i = 0;
    PPROCESS pVM = DD_GetProperVM(NULL, pvOffset);

    DEBUGGERMSG (KDZONE_SWBP, (L"++KdpFindBreakPoint (0x%08x)\n", pvOffset));

    for (i = 0; i < BREAKPOINT_TABLE_SIZE; ++i)
    {
        if (g_aBreakpointTable[i].wRefCount &&
            (pVM == g_aBreakpointTable[i].pVM) && 
            (pvOffset == g_aBreakpointTable[i].Address))
        {
            DEBUGGERMSG (KDZONE_SWBP, (L"--KdpFindBreakPoint: (%u)\n", (i+1)));
            return i + 1;
        }
    }

    DEBUGGERMSG (KDZONE_SWBP, (L"--KdpFindBreakPoint: Breakpoint not found\n"));
    return 0;
}

