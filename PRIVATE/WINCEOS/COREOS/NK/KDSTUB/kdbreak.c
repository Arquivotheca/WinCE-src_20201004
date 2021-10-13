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

    kdbreak.c

Abstract:

    This module implements machine dependent functions to add and delete
    breakpoints from the kernel debugger breakpoint table.

Revision History:

--*/

#include "kdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpAddBreakpoint)
#pragma alloc_text(PAGEKD, KdpDeleteBreakpoint)
#endif

// The following variables are global for a reason.  Do not move them to the stack or bad things happen
// when flushing instructions.
KDP_BREAKPOINT_TYPE Content;
KDP_BREAKPOINT_TYPE KContent;
KDP_BREAKPOINT_TYPE ReadBack;


BOOLEAN KdpWriteVerify2(IN VOID* Address, IN VOID* KAddress, VOID* pbData, ULONG nSize)
{
    BOOLEAN fRet = FALSE;
    BOOLEAN fMcRet = FALSE;
    ULONG nRbSize;
    ULONG nMmSize;
    ULONG nAddr;
    PVOID rgAddr[2] = { Address, KAddress };

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpWriteVerify2 (%8.8lx, %8.8lx, size=%d)\r\n", Address, KAddress, nSize));

    for (nAddr = 0; nAddr < 2; nAddr++)
    {
        if (rgAddr[nAddr])
        {
            nMmSize = KdpMoveMemory((PCHAR)rgAddr[nAddr], pbData, nSize);
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpWriteVerify2: %s %d byte%s at %8.8lx\r\n",
                                       (nMmSize == nSize) ? L"Wrote" : L"Failed to write", nSize, 
                                       (nMmSize == 1) ? L"" : L"s", rgAddr[nAddr]));

            ReadBack = 0;
            nRbSize = KdpMoveMemory((PCHAR)&ReadBack, (PCHAR)rgAddr[nAddr], nSize);
            if (nRbSize != nSize)
            {
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpWriteVerify2: Failed to read back %d byte%s at %8.8lx (read %d)\r\n",
                                           nSize, (nMmSize == 1) ? L"" : L"s", nRbSize));
            }
            else
            {
                fMcRet = !memcmp(&ReadBack, pbData, nSize);
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpWriteVerify2: Write%s successful", fMcRet ? L"" : L" NOT")); 
                if (fMcRet)
                {
                    DEBUGGERMSG(KDZONE_BREAK, (L"\r\n"));
                    fRet = TRUE;
                }
                else
                {
                    DEBUGGERMSG(KDZONE_BREAK, (L" (read %x)\r\n", (DWORD)ReadBack));
                }
            }
        }
    }

    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpWriteVerify2 (%d)\r\n", (int)fRet));

    return fRet;
}


ULONG KdpAddBreakpoint(IN PVOID Address)

/*++

Routine Description:

    This routine adds an entry to the breakpoint table and returns a handle
    to the breakpoint table entry.

Arguments:

    Address - Supplies the address where to set the breakpoint.

Return Value:

    A value of zero is returned if the specified address is already in the
    breakpoint table, there are no free entries in the breakpoint table, the
    specified address is not correctly aligned, or the specified address is
    not valid. Otherwise, the index of the assigned breakpoint table entry
    plus one is returned as the function value.

--*/

{
    ULONG Handle = 0;
    ULONG Index;
    BOOLEAN Accessible = FALSE;
    BOOLEAN Mode16Bit = FALSE;                          // used for Thumb and MIPS16
    BOOLEAN KAccessible = FALSE;
    PVOID KAddress = NULL;
    KDP_BREAKPOINT_TYPE KdpBreakpointInstruction = KDP_BREAKPOINT_VALUE;
    ULONG Length = sizeof(KDP_BREAKPOINT_TYPE);
    BOOLEAN BPSet = FALSE;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpAddBreakpoint (%8.8lx)\r\n", Address));

    //
    //  update the breakpoint Instruction and Length if stopped within
    //  16-bit code. (16-bit code indicated by LSB of Address)
    //

#if defined(MIPSII) || defined(THUMBSUPPORT)
    if (Is16BitSupported && (((ULONG)Address & 1) != 0)) {
        DEBUGGERMSG( KDZONE_BREAK,(L"  KdpAddBreakpoint: 16 Bit breakpoint\r\n"));
        Length = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
        KdpBreakpointInstruction = KDP_BREAKPOINT_16BIT_VALUE;
        Address = (PVOID) ((ULONG)Address & ~1);
        Mode16Bit = TRUE;
    }
#endif

    Content = 0;
    KContent = 0;

    //
    // If the specified address is not properly aligned, then return zero.
    //
    if (((ULONG)Address & (Length-1)) != 0) {
        DEBUGGERMSG(KDZONE_BREAK, (L"  KdpAddBreakpoint: Address not aligned\r\n"));
        Handle = 0;
        goto exit;
    }

    if (!IsKernelVa(Address) && (ZeroPtr(Address) >= (ULONG)DllLoadBase))
    { // If Addr is not physical and Address is in DLL shared space then Get Kernel Address
        DEBUGGERMSG( KDZONE_BREAK,(L"  KdpAddBreakpoint: Is Dll %8.8lx", Address));
        KAddress = (PVOID)(ZeroPtr(Address) + kdProcArray[0].dwVMBase); // Get NK.EXE based address
        DEBUGGERMSG( KDZONE_BREAK,(L" converted to %8.8lx\r\n", KAddress));
    }

    // Get the instruction to be replaced. If the instruction cannot be read,
    // then mark breakpoint as not accessible.
    if (KdpMoveMemory((PCHAR)&Content, (PCHAR)Address, Length ) != Length) {
        Accessible = FALSE;
    } else {
        DEBUGGERMSG(KDZONE_BREAK,(L"  KdpAddBreakpoint: Successfully read %8.8lx at %8.8lx\r\n", (DWORD)Content, Address));
        Accessible = TRUE;
    }

    // if we got a Kernel Address: try to get its instruction
    if (KAddress != NULL) {
        if (KdpMoveMemory((PCHAR)&KContent, (PCHAR)KAddress, Length ) != Length) {
            KAccessible = FALSE;
        } else {
            DEBUGGERMSG(KDZONE_BREAK,(L"  KdpAddBreakpoint: Successfully read %8.8lx at %8.8lx\r\n", (DWORD)Content, KAddress));
            KAccessible = TRUE;
        }
        if (Content != KContent) {
            // assert(FALSE);
            // if contents are different
            DEBUGGERMSG(KDZONE_BREAK,(L"  KdpAddBreakpoint: Content %8.8lx != KContent at %8.8lx\r\n", (DWORD)Content, (DWORD)KContent, KAddress));
            if (!Content) {
                Content = KContent;
                DEBUGGERMSG(KDZONE_BREAK,(L"  KdpAddBreakpoint: Set Content to %8.8lx\r\n", (DWORD)KContent));
            }
        }
    }

    // Search the breakpoint table for a free entry and check if the specified
    // address is already in the breakpoint table.
    if (Content == KdpBreakpointInstruction) {
        DEBUGGERMSG(KDZONE_BREAK,(L"  KdpAddBreakpoint: Found existing BP %8.8lx\r\n", Address));
        for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++) {
            if (KdpBreakpointTable[Index].Address == Address ||
                (KAddress != NULL && KdpBreakpointTable[Index].KAddress == KAddress)) {
                Handle = Index + 1;
                goto exit;
            }
        }
    }

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if (KdpBreakpointTable[Index].Flags == 0 ) {
            Handle = Index + 1;
            break;
        }
    }

    // If a free entry was found, then write breakpoint and return the handle
    // value plus one. Otherwise, return zero.
    if (Handle) {
        if ( Accessible || KAccessible) {

            // If the specified address is not write-accessible, then return zero.
            if (!SafeDbgVerify(Address, DV_SETBP)) {
                Address = NULL;
                Accessible = FALSE;
            }
            if (KAddress != NULL && !SafeDbgVerify(KAddress, DV_SETBP)) {
                KAddress = NULL;
                KAccessible = FALSE;
            }

            if (!Accessible || !KAccessible) {
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpAddBreakpoint: At least one address not writable %8.8lx %8.8lx\r\n", Address, KAddress));
            }
            
            if (Accessible || KAccessible) {
                BPSet = KdpWriteVerify2(Address, KAddress, &KdpBreakpointInstruction, Length);
                if (BPSet)
                {
                    KdpBreakpointTable[Handle - 1].Address = Address;
                    KdpBreakpointTable[Handle - 1].KAddress = KAddress;
                    KdpBreakpointTable[Handle - 1].Content = Content;
                    KdpBreakpointTable[Handle - 1].Flags = KD_BREAKPOINT_IN_USE;

                    if (Mode16Bit) {
                        KdpBreakpointTable[Handle - 1].Flags |= KD_BREAKPOINT_16BIT;
                    }
                } else {

                    // write failed
                    DEBUGGERMSG(KDZONE_BREAK, (L"  KdpAddBreakpoint: Breakpoint could not be set\r\n"));
                    Handle = 0;
                }
            } else {

                // memory is not accessible
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpAddBreakpoint: Address not accessible\r\n"));
                Handle = 0;
            }
        }
    }
exit:
    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpAddBreakpoint: (handle = %8.8lx)\r\n", Handle));
    
    return Handle;
}


BOOLEAN KdpDeleteBreakpoint(IN ULONG Handle)
/*++

Routine Description:

    This routine deletes an entry from the breakpoint table.

Arguments:

    Handle - Supplies the index plus one of the breakpoint table entry
        which is to be deleted.

Return Value:

    A value of FALSE is returned if the specified handle is not a valid
    value or the breakpoint cannot be deleted because the old instruction
    cannot be replaced. Otherwise, a value of TRUE is returned.

--*/
{
    BOOLEAN fRet = FALSE;
    KDP_BREAKPOINT_TYPE KdpBreakpointInstruction = KDP_BREAKPOINT_VALUE;
    ULONG Length = sizeof(KDP_BREAKPOINT_TYPE);
    ULONG Index;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpDeleteBreakpoint (%8.8lx)\r\n", Handle));

    // If the specified handle is not valid, then return FALSE.
    if ((Handle == 0) || (Handle > BREAKPOINT_TABLE_SIZE)) {
        DEBUGGERMSG(KDZONE_BREAK, (L"  KdpDeleteBreakpoint: Invalid handle\r\n"));
        return FALSE;
    }

    // Handles are 1-based; 0 is an invalid handle value
    Index = Handle - 1;

    // If the specified breakpoint table is not valid, then return FALSE.
    if (KdpBreakpointTable[Index].Flags == 0) {
        DEBUGGERMSG(KDZONE_BREAK, (L"  KdpDeleteBreakpoint: No such BP\r\n"));
        goto exit;
    }

#if defined(MIPSII) || defined(THUMBSUPPORT)
    // Determine the breakpoint instruction and size
    if (Is16BitSupported && (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_16BIT)) {
        Length = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
        KdpBreakpointInstruction = KDP_BREAKPOINT_16BIT_VALUE;
    }
#endif

    // Replace the instruction contents.
    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) {

        DEBUGGERMSG(KDZONE_BREAK, (L"  KdpDeleteBreakpoint: Restoring instruction %8.8lx\r\n", KdpBreakpointTable[Index].Address));

        fRet = KdpWriteVerify2(KdpBreakpointTable[Index].Address, KdpBreakpointTable[Index].KAddress,
                               (CHAR*)&KdpBreakpointTable[Index].Content, Length);
        if (fRet)
        {
            // Release lock on memory.
            // if (KdpBreakpointTable[Index].Address != NULL)
            //     SafeDbgVerify((PCHAR)KdpBreakpointTable[Index].Address, DV_CLEARBP);
            if (KdpBreakpointTable[Index].KAddress != NULL)
                SafeDbgVerify((PCHAR)KdpBreakpointTable[Index].KAddress, DV_CLEARBP);
            //
            // Delete breakpoint table entry and return TRUE.
            //

            KdpBreakpointTable[Index].Flags    = 0;
            KdpBreakpointTable[Index].Content  = 0;
            KdpBreakpointTable[Index].Address  = NULL;
            KdpBreakpointTable[Index].KAddress = NULL;
        } else {
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpDeleteBreakpoint: Restore failed!\r\n"));
        }
    }
    
exit:    
    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpDeleteBreakpoint (%d)\r\n", (int)fRet));
    
    return fRet;
}


VOID KdpDeleteAllBreakpoints(VOID)
/*++

Routine Description:

    Call KdpDeleteBreakpoint on every breakpoint handle and ignore the result.  If all
    goes well, this should remove all breakpoints in the system and allow it to
    execute normally.

--*/
{
    ULONG i;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpDeleteAllBreakpoints\r\n"));

    // handles are 1-based
    for (i = 1; i <= BREAKPOINT_TABLE_SIZE; i++) { 
        KdpDeleteBreakpoint(i);
    }

    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpDeleteAllBreakpoints\r\n"));
}


BOOLEAN KdpSuspendBreakpoint(IN ULONG Index)
/*++

Routine Description:

    Suspend the specified breakpoint by restoring its instruction
    
Arguments:

    Index - index of the breakpoint to suspend (NOTE: NOT THE HANDLE)

Return Value:

    TRUE - the breakpoint was suspended
    FALSE - otherwise

--*/
{
    BOOLEAN fRet = TRUE;
    KDP_BREAKPOINT_TYPE KdpBreakpointInstruction = KDP_BREAKPOINT_VALUE;
    ULONG Length = sizeof(KDP_BREAKPOINT_TYPE);

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpSuspendBreakpoint (%8.8lx)\r\n", Index + 1));

    // Determine the breakpoint instruction and size
#if defined(MIPSII) || defined(THUMBSUPPORT)
    if (Is16BitSupported && (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_16BIT))
    {
        Length = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
        KdpBreakpointInstruction = KDP_BREAKPOINT_16BIT_VALUE;
    }
#endif        
    // Replace the instruction contents.
    if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE)
    {
        DEBUGGERMSG(KDZONE_BREAK, (L"  KdpSuspendBreakpoint: Suspending BP %8.8lx\r\n", KdpBreakpointTable[Index].Address));

        fRet = KdpWriteVerify2(KdpBreakpointTable[Index].Address, KdpBreakpointTable[Index].KAddress,
                               (CHAR*)&KdpBreakpointTable[Index].Content, Length);
        if (fRet) {
            KdpBreakpointTable[Index].Flags |= KD_BREAKPOINT_SUSPENDED;
        } else {
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpSuspendBreakpoint: Suspend failed!\r\n"));
        }
    }

    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpSuspendBreakpoint (%d)\r\n", (int)fRet));

    return fRet;
}

VOID KdpSuspendAllBreakpoints(VOID)
/*++

Routine Description:

    Suspend all breakpoints that are currently in use and not already suspened.

--*/
{
    ULONG Index;
    
    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpSuspendAllBreakpoints\r\n"));

    // Examine each entry in the table in turn
    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++)
    {
        // Make sure breakpoint is in use and not already suspended
        if ((KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
            !(KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_SUSPENDED))
        {
            KdpSuspendBreakpoint(Index);
        }
    }
    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpSuspendAllBreakpoints\r\n"));
}

VOID KdpReinstateSuspendedBreakpoints(VOID)
/*++

Routine Description:

    Reinstate any breakpoints that were suspended by KdpSuspendBreakpointIfHitByKd.  It is assumed
    this function will be called shortly before exitting KdpTrap.

--*/
{
    ULONG Length;
    ULONG Index;
    BOOLEAN fRet;
    
    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpReinstateSuspendedBreakpoints\r\n"));

    // Examine each entry in the table in turn
    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++)
    {
        if ((KdpBreakpointTable[Index].Flags & (KD_BREAKPOINT_IN_USE | KD_BREAKPOINT_SUSPENDED)) ==
            (KD_BREAKPOINT_IN_USE | KD_BREAKPOINT_SUSPENDED))
        {
            Length  = sizeof(KDP_BREAKPOINT_TYPE);
            Content = KDP_BREAKPOINT_VALUE;

#if defined(MIPSII) || defined(THUMBSUPPORT)
            if (Is16BitSupported && (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_16BIT))
            {
                Length  = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
                Content = KDP_BREAKPOINT_16BIT_VALUE;
            }
#endif
            DEBUGGERMSG(KDZONE_BREAK, (L"  KdpReinstateSuspendedBreakpoints: Reinstating BP %8.8lx\r\n", KdpBreakpointTable[Index].Address));

            fRet = KdpWriteVerify2(KdpBreakpointTable[Index].Address, KdpBreakpointTable[Index].KAddress,
                                   &Content, Length);
            if (fRet) {
                KdpBreakpointTable[Index].Flags &= ~KD_BREAKPOINT_SUSPENDED;
            } else {
                DEBUGGERMSG(KDZONE_BREAK, (L"  KdpReinstateSuspendedBreakpoints: Reinstate failed!\r\n",
                                           KdpBreakpointTable[Index].Address));
            }
        }
    }
    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpReinstateSuspendedBreakpoints\r\n"));
}


BOOLEAN KdpSuspendBreakpointIfHitByKd(VOID* Address)
/*++

Routine Description:

    Suspend the breakpoint, if one exists, at the specified location.  It is assumed this routine
    will be called if the debugger "trips" over one of its own breakpoints which was set in code
    intended to be shared by KdStub.  This should allow stepping through such code.

    Note: currently, this function will not suspend more than one (1) breakpoint, under the
    assumption that KdpAddBreakpoint will not instantiate multiple breakpoints at the same location.
    
Arguments:

    Address - the address of the instruction that was just hit

Return Value:

    TRUE - there was a breakpoint at that location and it was suspended
    FALSE - otherwise

--*/
{
    BOOLEAN fRet = FALSE;
    ULONG Index;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpSuspendBreakpointIfHitByKd (%8.8lx)\r\n", (DWORD)Address));

    if (Is16BitSupported) {
        Address = (VOID*)(((DWORD)Address) & ~1);
    }
    
    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++)
    {
        if ((KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
            ((KdpBreakpointTable[Index].Address  == Address) ||
             (KdpBreakpointTable[Index].KAddress == Address)))
        {
            fRet = KdpSuspendBreakpoint(Index);
            break;
        }
    }

    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpSuspendBreakpointIfHitByKd (%d)\r\n", (int)fRet));

    return fRet;
}


BOOL KdpSanitize(BYTE* pbClean, VOID* pvAddrMem, ULONG nSize, BOOL fAlwaysCopy)
/*++

Routine Description:

    Sanitize a region of memory.  If the specified region of memory contains a breakpoint, write the data
    into the output buffer as it would appear if there were no breakpoints.

    The memory address (pvAddrMem) is "slotized" relative to the current process' address space before being
    compared with addresses of breakpoints.
    
Arguments:

    pbClean:     Output buffer to which sanitized memory is written
    pvAddrMem:   First virtual address of the region of memory to be cleaned.  This must be the actual
                 address of the memory (so it can be matched against the breakpoint table) and it must
                 be readable.
    nSize:       Size of both the output buffer and the memory to be cleaned
    fAlwaysCopy: Copy the memory into the output buffer regardless of whether the memory contains a
                 breakpoint.  Normally, the memory is only copied if a breakpoint was present.
    
Return Value:

    TRUE  - there was a breakpoint in the specified range and it has been sanitized in the output buffer
    FALSE - otherwise

--*/
{
    BOOL fRet = FALSE;
    BYTE* pbAddrMemSlotted = (BYTE*)MapPtrInProc(pvAddrMem, pCurProc);
    ULONG iBp;

    DEBUGGERMSG(KDZONE_BREAK, (L"++KdpSanitize (clean=%8.8lx, mem=(%8.8lx-%8.8lx), size=%d, alwayscopy=%d)\r\n", (DWORD)pbClean, (DWORD)pvAddrMem, (DWORD)pvAddrMem + nSize - 1, nSize, (DWORD)fAlwaysCopy));

    if (fAlwaysCopy)
    {
        // initialize the buffer
        memcpy(pbClean, pvAddrMem, nSize);
    }

    for (iBp = 0; iBp < BREAKPOINT_TABLE_SIZE; iBp++)
    {
        if ((KdpBreakpointTable[iBp].Flags & KD_BREAKPOINT_IN_USE) &&
            !(KdpBreakpointTable[iBp].Flags & KD_BREAKPOINT_SUSPENDED))
        {
            ULONG i;
            BYTE* rgpbAddr[2] = { (BYTE*)KdpBreakpointTable[iBp].Address, (BYTE*)KdpBreakpointTable[iBp].KAddress };
            ULONG nBpSize = sizeof(KDP_BREAKPOINT_TYPE);

#if defined(MIPSII) || defined(THUMBSUPPORT)
            if (Is16BitSupported && (KdpBreakpointTable[iBp].Flags & KD_BREAKPOINT_16BIT))
            {
                nBpSize = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
            }
#endif
            for (i = 0; i < 2; i++)
            {
                if ((pbAddrMemSlotted < (rgpbAddr[i] + nBpSize)) &&
                    ((pbAddrMemSlotted + nSize) > rgpbAddr[i]))
                {
                    ULONG nCommonBytes = nBpSize;
                    BYTE* pbSrc = (BYTE*)&KdpBreakpointTable[iBp].Content;
                    BYTE* pbDst = pbClean + (rgpbAddr[i] - pbAddrMemSlotted);
                    LONG  lDiff;

                    DEBUGGERMSG(KDZONE_BREAK, (L"  KdpSanitize: cleaning BP h=%d, addr=%8.8lx\r\n", iBp + 1, rgpbAddr[i]));

                    // check for memory buffer beginning in the middle of the breakpoint
                    lDiff = pbAddrMemSlotted - rgpbAddr[i];
                    if (lDiff > 0)
                    {
                        pbSrc  += lDiff;
                        pbDst  += lDiff;
                        nCommonBytes -= lDiff;
                    }
                    
                    // check for memory buffer ending in the middle of the breakpoint
                    lDiff = (rgpbAddr[i] + nBpSize) - (pbAddrMemSlotted + nSize);
                    if (lDiff > 0)
                    {
                        nCommonBytes -= lDiff;
                    }
                    
                    KD_ASSERT((nCommonBytes > 0) && (nCommonBytes <= nBpSize));
                    KD_ASSERT((pbClean <= pbDst) && ((pbDst + nCommonBytes) <= (pbClean + nSize)));

                    DEBUGGERMSG(KDZONE_BREAK, (L"  KdpSanitize: copy %d byte(s) from %8.8lx to %8.8lx\r\n", nCommonBytes, (DWORD)pbSrc, (DWORD)pbDst));

                    // initialize the clean buffer if it hasn't been done by now
                    if (!fRet && !fAlwaysCopy) memcpy(pbClean, pvAddrMem, nSize);

                    memcpy(pbDst, pbSrc, nCommonBytes);

                    fRet = TRUE;
                }
            }
        }
        
    }

    DEBUGGERMSG(KDZONE_BREAK, (L"--KdpSanitize (%d)\r\n", (DWORD)fRet));

    return fRet;
}

