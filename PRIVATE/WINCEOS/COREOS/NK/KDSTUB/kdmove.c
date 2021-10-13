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


void FlushDCache(void);

BOOL kdpIsInaccessible(LPVOID pvAddr)
{
    BOOL fRet = FALSE;   // Assume FALSE in case IOCTL not implemented

    DEBUGGERMSG(KDZONE_MOVE, (L"++kdpIsInaccessible, pvAddr=0x%08X\r\n", pvAddr));
    // Check if the memory is Inaccessible for read/write
    fRet = KDIoControl (KD_IOCTL_MEMORY_INACCESSIBLE, pvAddr, sizeof(pvAddr));
    DEBUGGERMSG(KDZONE_MOVE, (L"--kdpIsInaccessible, fRet=%s\r\n", (fRet ? L"TRUE" : L"FALSE") ));
    return fRet;
}

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

    DWORD dwTgtAddr;
    DWORD dwSrcAddr;
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

    KeSweepCurrentIcache ();

    ActualLength = Length;
    //
    // This code is the most common cause of KD exceptions because the OS lies and says
    // we can safely access memory.  We use SEH as a last resort.
    //
    __try
    {
        // Get Debuggee context, potentially statically mapped kernel address equivalent if accessible
        dwTgtAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (Destination, FALSE);
        dwSrcAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (Source, FALSE);
        if (kdpIsROM ((void *) dwTgtAddr, 1))
        {

            DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: target address is in ROM (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                      Destination, dwTgtAddr));
            dwTgtAddr = 0; // Don't even try to write to ROM (will except on some platforms)
        }
        else if (kdpIsInaccessible ((void *) dwTgtAddr))
        {
            DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: target address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                      Destination, dwTgtAddr));
            dwTgtAddr = 0; // Don't even try to write to Inaccessible memory (will except on some platforms)
        }
        else if (kdpIsInaccessible ((void *) dwSrcAddr))
        {
            DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: source address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                      Source, dwSrcAddr));
            dwSrcAddr = 0; // Don't even try to read from Inaccessible memory (will except on some platforms)
        }
        else
        {
            DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: Dst 0x%08X (was 0x%08X) Src 0x%08X (was 0x%08X) length 0x%08X\r\n",
                                      dwTgtAddr, Destination, dwSrcAddr, Source, ActualLength));
        }

        while (Length && dwTgtAddr && dwSrcAddr) 
        {
            *(char *) dwTgtAddr = *(char *) dwSrcAddr; // Actual byte per byte copy

            Destination++;
            Source++;
            Length--;
            
            if (Length)
            {
                if ((DWORD) Destination & (PAGE_SIZE - 1))
                { // Did not cross page boundary
                    dwTgtAddr++;
                }
                else
                { // Crossed page boundary
                    dwTgtAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (Destination, FALSE);
                    if (kdpIsROM ((void *) dwTgtAddr, 1))
                    {
                        DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: target address is in ROM (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                  Destination, dwTgtAddr));
                        dwTgtAddr = 0; // Don't even try to write to ROM (will except on some platforms)
                    }
                    else if (kdpIsInaccessible ((void *) dwTgtAddr))
                    {
                        DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: target address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                  Destination, dwTgtAddr));
                        dwTgtAddr = 0; // Don't even try to write to Inaccessible memory (will except on some platforms)
                    }
                    else
                    {
                        DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: New Dst Page - Dst 0x%08X (was 0x%08X) Src 0x%08X (was 0x%08X) length 0x%08X\r\n",
                            dwTgtAddr, Destination, dwSrcAddr, Source, ActualLength));
                    }
                }
                
                if ((DWORD) Source & (PAGE_SIZE - 1))
                { // Did not cross page boundary
                    dwSrcAddr++;
                }
                else
                { // Crossed page boundary
                    dwSrcAddr = (DWORD) MapToDebuggeeCtxKernEquivIfAcc (Source, FALSE);
                    if (kdpIsInaccessible ((void *) dwSrcAddr))
                    {
                        DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: source address is Inaccessible (Addr=0x%08X,KAddr=0x%08X). Abort Mem Copy.\r\n",
                                                  Source, dwSrcAddr));
                        dwSrcAddr = 0; // Don't even try to read from Inaccessible memory (will except on some platforms)
                    }
                    else
                    {
                        DEBUGGERMSG (KDZONE_MOVE, (L"  KdpMoveMemory: New Src Page - Dst 0x%08X (was 0x%08X) Src 0x%08X (was 0x%08X) length 0x%08X\r\n",
                            dwTgtAddr, Destination, dwSrcAddr, Source, ActualLength));
                    }
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(KDZONE_MOVE, (L"  KdpMoveMemory: Exception encountered\r\n"));
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

