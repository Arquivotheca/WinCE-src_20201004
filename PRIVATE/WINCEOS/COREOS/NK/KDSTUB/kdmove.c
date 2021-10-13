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

    kdmove.c

Abstract:

    This module contains code to implement the portable kernel debugger
    memory mover.

Revision History:

--*/

#include "kdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpMoveMemory)
#pragma alloc_text(PAGEKD, KdpQuickMoveMemory)
#endif
#define NOISE 1

void FlushDCache(void);


ULONG
KdpMoveMemory (
    IN PCHAR Destination,
    IN PCHAR Source,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine moves data to or from the message buffer and returns the
    actual length of the information that was moved. As data is moved, checks
    are made to ensure that the data is resident in memory and a page fault
    will not occur. If a page fault would occur, then the move is truncated.

Arguments:

    Destination  - Supplies a pointer to destination of the move operation.

    Source - Supplies a pointer to the source of the move operation.

    Length - Supplies the length of the move operation.

Return Value:

    The actual length of the move is returned as the fucntion value.

--*/

{

    PVOID Address1;
    PVOID Address2;
    ULONG ActualLength;

    DEBUGGERMSG(KDZONE_MOVE, (L"++KdpMoveMemory\r\n"));
    
    // Move the source information to the destination address.

#if   defined(MIPS)
    FlushDCache();
#elif defined(SHx)
    FlushCache();
#elif defined(ARM)
    FlushDCache();
    FlushICache();
#endif

    KeSweepCurrentIcache();

    ActualLength = Length;

    Address1 = (PVOID)MmDbgWriteCheck((PVOID)Destination);

    Address2 = (PVOID)MmDbgReadCheck((PVOID)Source);

    DEBUGGERMSG(KDZONE_MOVE, (L"KdMove Dst %8.8lx (was %8.8lx) Src %8.8lx (was %8.8lx) length %8.8lx\r\n",
        Address1, Destination, Address2, Source, ActualLength));

    //
    // This code is the most common cause of KD exceptions because the OS lies and says
    // we can safely access memory.  We use SEH as a last resort.
    //
    __try
    {
        while (Length > 0) {

            //
            // Check to determine if the move will succeed before actually performing
            // the operation.
            // 
            if ((Address1 == NULL) || (Address2 == NULL)) {
                break;
            }
            *(PCHAR)Address1 = *(PCHAR)Address2;
            Destination += 1;
            Source += 1;
            Length -= 1;
            Address1 = (PVOID)MmDbgWriteCheck((PVOID)Destination);
            Address2 = (PVOID)MmDbgReadCheck((PVOID)Source);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Length = ActualLength;
    }

    //
    // Flush the instruction cache in case the write was into the instruction
    // stream.
    //

#if   defined(MIPS)
    FlushDCache();
#elif defined(SHx)
    FlushCache();
#elif defined(ARM)
    FlushDCache();
    FlushICache();
#endif

    KeSweepCurrentIcache();

    DEBUGGERMSG(KDZONE_MOVE, (L"--KdpMoveMemory\r\n"));
    return ActualLength - Length;
}

VOID
KdpQuickMoveMemory (
    IN PCHAR Destination,
    IN PCHAR Source,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine does the exact same thing as RtlMoveMemory, BUT it is
    private to the debugger.  This allows users to set breakpoints and
    watch points in RtlMoveMemory without risk of recursive debugger
    entry.

    N.B.  UNLIKE KdpMoveMemory, this routine does NOT check for accessability
      and may fault!  Use it ONLY in the debugger and ONLY where you
      could use RtlMoveMemory.

Arguments:

    Destination  - Supplies a pointer to destination of the move operation.

    Source - Supplies a pointer to the source of the move operation.

    Length - Supplies the length of the move operation.

Return Value:

    None.

--*/
{
    while (Length--)
        *Destination++ = *Source++;
}
