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

#include <bldver.h>
#include "osaxs_p.h"

// Nkshx.h defines these, so get rid of them before including DwCeDump.h
#ifdef ProcessorLevel
#undef ProcessorLevel
#endif
#ifdef ProcessorRevision
#undef ProcessorRevision
#endif

#include "DwCeDump.h"
#define  WATSON_BINARY_DUMP TRUE
#include "DwPublic.h"
#include "DwPrivate.h"
#include "DwDmpGen.h"
#include "diagnose.h"
#include <intsafe.h>





enum
{
    STACK_FRAMES_BUFFERED = 20,
    MAX_STREAMS           = (ceStreamLastStream - ceStreamNull),
    MAX_THREAD_ENTRIES    = FLEX_PTMI_BUFFER_SIZE / sizeof (WORD),
};

enum
{
    VIRTUAL_MEMORY  = TRUE,
    PHYSICAL_MEMORY = FALSE
};

enum
{
#ifdef ARM
    // The ARM architecture is special in that each page directory entry actually points to 1MB instead of 4MB for the other
    // architectures. This means that each page table takes up 4 page directory entries.
    VM_PD_SHIFT = 20,   // ARM actually does a >> 22 << 2, make sure this isn't messed up on arm...
#else
    VM_PD_SHIFT = 22,   // Seems to be the case for all platforms
#endif
    VM_PT_SHIFT = 12,
    VM_PD_SIZE  = (VM_PAGE_OFST_MASK | PG_SECTION_OFST_MSAK) + 1,
};


typedef struct _MEMORY_BLOCK 
{
    DWORD   dwMemoryStart;
    DWORD   dwMemorySize;
    PROCESS *VMProcess;
} MEMORY_BLOCK;

MINIDUMP_HEADER         g_ceDmpHeader;
MINIDUMP_DIRECTORY      g_ceDmpDirectory[MAX_STREAMS];
CEDUMP_SYSTEM_INFO      g_ceDmpSystemInfo;
CEDUMP_EXCEPTION_STREAM g_ceDmpExceptionStream;
CEDUMP_EXCEPTION        g_ceDmpException;
CEDUMP_ELEMENT_LIST     g_ceDmpExceptionContext;
CEDUMP_ELEMENT_LIST     g_ceDmpModuleList;
CEDUMP_ELEMENT_LIST     g_ceDmpProcessList;
CEDUMP_ELEMENT_LIST     g_ceDmpThreadList;
CEDUMP_ELEMENT_LIST     g_ceDmpThreadContextList;
CEDUMP_ELEMENT_LIST     g_ceDmpProcessModuleList;
CEDUMP_THREAD_CALL_STACK_LIST   g_ceDmpThreadCallStackList;
CEDUMP_MEMORY_LIST              g_ceDmpMemoryVirtualList;
CEDUMP_MEMORY_LIST              g_ceDmpMemoryPhysicalList;
CEDUMP_BUCKET_PARAMETERS        g_ceDmpBucketParameters;

WATSON_DUMP_SETTINGS g_watsonDumpSettings = {0};

BYTE         g_bBuffer[FLEX_PTMI_BUFFER_SIZE];
MEMORY_BLOCK g_memoryVirtualBlocks[MAX_MEMORY_VIRTUAL_BLOCKS];
DWORD        g_dwMemoryVirtualBlocksTotal = 0;
DWORD        g_dwMemoryVirtualSizeTotal = 0;
MEMORY_BLOCK g_memoryPhysicalBlocks[MAX_MEMORY_PHYSICAL_BLOCKS];
DWORD        g_dwMemoryPhysicalBlocksTotal = 0;
DWORD        g_dwMemoryPhysicalSizeTotal = 0;
BYTE         g_bBufferAlignment[CEDUMP_ALIGNMENT] = {0};

DWORD g_dwReservedMemorySize = 0;
DWORD g_dwSegmentIdx = 0;
DWORD g_dwWrittenOffset;
DWORD g_dwWrittenCRC;
DWORD g_dwReadOffset;
DWORD g_dwReadCRC;
LONG  g_lThreadsInPostMortemCapture = 0;
BOOL  g_fDumpHeapData = FALSE;
BOOL  g_fDumpNkGlobals = FALSE;
BOOL  g_fNkGlobalsDumped = FALSE;
BOOL  g_fCaptureDumpFileOnDeviceCalled = FALSE;
BOOL  g_fReportFaultCalled = FALSE;
BOOL  g_fStackBufferOverrun = FALSE;
DWORD g_dwLastError;
DWORD g_dwDumpFileSize = 0;
CEDUMP_TYPE g_ceDumpType = ceDumpTypeUndefined;

BOOL g_hostCapture = FALSE;

BYTE *g_pKdpBuffer = NULL;
DWORD g_dwKdpBufferSize = 0;
DWORD g_dwKdpBufferOffset = 0;

// See if g_pDmpProc needs to be split into g_pDmpActvProc and g_pDmpVMProc
PTHREAD g_pDmpThread = NULL;
PPROCESS g_pDmpProc = NULL;

HANDLE g_hEventDumpFileReady = NULL;

WCHAR g_wzOEMString[MAX_PATH];
DWORD g_dwOEMStringSize = 0;
WCHAR g_wzPlatformType[MAX_PATH];
DWORD g_dwPlatformTypeSize = 0;
PLATFORMVERSION g_platformVersion[4];
DWORD g_dwPlatformVersionSize = 0;

// Send a message to the host to keep the kitl stream active.
#define PING_HOST_IF(doit)                 \
    do{                                 \
        if(g_hostCapture && (doit)){    \
            OsAxsSendKeepAlive();       \
        }                               \
    }while(FALSE)

// Ping the host every X modules|threads|processes|iterations, etc.
#define PING_INTERVAL     (32)


// Local copy of exception record and context record when using ReportFault
// This is to make sure untrusted apps do not change the contents after they have
// been validated
EXCEPTION_RECORD g_exceptionRecordReportFault = {0};
CONTEXT g_contextReportFault = {0};


static void __forceinline DisableFaults ()
{
    UTlsPtr () [TLSSLOT_KERNEL] |= (TLSKERN_NOFAULTMSG | TLSKERN_NOFAULT);
}

static void __forceinline EnableFaults ()
{
    UTlsPtr () [TLSSLOT_KERNEL] &= ~(TLSKERN_NOFAULTMSG|TLSKERN_NOFAULT);
}

static void __forceinline SetFrameCount (DWORD ThreadCount, DWORD FrameCount)
{
    if (ThreadCount < MAX_THREAD_ENTRIES)
        reinterpret_cast<WORD*>(g_bBuffer)[ThreadCount] = static_cast<WORD> (FrameCount);
}

static bool __forceinline CheckFrameCount (DWORD ThreadCount, DWORD FrameCount)
{
    if (ThreadCount < MAX_THREAD_ENTRIES)
        return reinterpret_cast<WORD*>(g_bBuffer)[ThreadCount] == static_cast<WORD> (FrameCount);
    return true;
}

static int __forceinline GetFrameCount (DWORD ThreadCount)
{
    if (ThreadCount < MAX_THREAD_ENTRIES)
        return reinterpret_cast<WORD*>(g_bBuffer)[ThreadCount];
    return -1;
}


static void __forceinline SafeSetEvent(HANDLE hEvent)
{
    // We call EVNTModify directly to bypass the handle table lock.
    // We are in a non-preeemptable state so:
    // 1.  We can safely use the handle PHDATA (handle is never closed)
    // 2.  Otherwise, if we try to take the lock and there is a contention we could deadlock (or worse).

    PHDATA phd = h2pHDATA (hEvent, g_pprcNK->phndtbl);
    PEVENT lpe = GetEventPtr (phd);
    
    if (lpe) 
    {
        EVNTModify (lpe, EVENT_SET);
    }
}

/*----------------------------------------------------------------------------
    WatsonWriteKDBG

    Write out byte of data to KDBG buffer and send when full.
----------------------------------------------------------------------------*/
static HRESULT WatsonWriteKDBG(BYTE bDataOut, BOOL fFlush)
{
    HRESULT hRes = E_FAIL;
    
    if (!g_dwKdpBufferSize)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteKDBG: g_dwKdpBufferSize is zero\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }
    
    if (g_dwKdpBufferOffset >= g_dwKdpBufferSize)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteKDBG: g_dwKdpBufferOffset (%u) >= g_dwKdpBufferSize (%u)\r\n", g_dwKdpBufferOffset, g_dwKdpBufferSize));
        hRes = E_FAIL; 
        goto Exit;
    }

    if (!fFlush)
    {
        g_pKdpBuffer[g_dwKdpBufferOffset] = bDataOut;
        ++ g_dwKdpBufferOffset;
    }

    if ((g_dwKdpBufferOffset >= g_dwKdpBufferSize) || fFlush)
    {
        STRING ResponseHeader;
        STRING AdditionalData;
        OSAXS_COMMAND OsAxsCmd;

        OsAxsCmd.dwApi = OSAXS_API_GET_WATSON_DUMP_FILE;
        OsAxsCmd.dwVersion = OSAXS_PROTOCOL_LATEST_VERSION;
        OsAxsCmd.hr = S_OK;
        OsAxsCmd.u.GetWatsonOTF.dwDumpType = (DWORD)g_ceDumpType;
        OsAxsCmd.u.GetWatsonOTF.dw64MaxDumpFileSize = g_dwDumpFileSize;
        OsAxsCmd.u.GetWatsonOTF.dwSegmentBufferSize = g_dwKdpBufferOffset;
        OsAxsCmd.u.GetWatsonOTF.dw64SegmentIndex = g_dwSegmentIdx;
        g_dwSegmentIdx += g_dwKdpBufferOffset;
        OsAxsCmd.u.GetWatsonOTF.fDumpfileComplete = FALSE;
        
        ResponseHeader.Length = sizeof (OsAxsCmd);
        ResponseHeader.Buffer = (PCHAR) &OsAxsCmd;
        AdditionalData.Length = (USHORT) g_dwKdpBufferOffset;
        AdditionalData.Buffer = (PCHAR) g_pKdpBuffer;
        
        g_dwKdpBufferOffset = 0;

        BOOL fContinue = OsAxsSendResponse(&ResponseHeader, &AdditionalData);
        if (!fContinue)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: g_pKdpSendOsAxsResponse returned false, aborting ... \r\n"));
            hRes = E_FAIL; 
            g_dwKdpBufferSize = 0;
            goto Exit;
        }
    }

    hRes = S_OK;

Exit:

    return hRes;
}

/*----------------------------------------------------------------------------
    WatsonReadData

    Reads data from the input stream, either reserved memory or KDBG.
----------------------------------------------------------------------------*/
static HRESULT WatsonReadData(DWORD dwOffset, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    HRESULT hRes = E_FAIL;
    DWORD dwCRCLoop = 0;
    DWORD dwBytesRead = 0;
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WatsonReadData: Enter\r\n"));

    if (((dwOffset + dwDataSize) > g_dwReservedMemorySize) || // Check for buffer overflow
        (dwOffset > (dwOffset + dwDataSize)))                 // Check for integer overflow
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WatsonReadData: Trying to read beyond size of reserved memory, Reserved Size=0x%08X, dwOffset=0x%08X, dwDataSize=0x%08X\r\n", g_dwReservedMemorySize, dwOffset, dwDataSize));
        hRes = E_OUTOFMEMORY; 
        goto Exit;
    }
    
    dwBytesRead = (DWORD)NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_READWATSON, NULL, dwOffset, pDataBuffer, dwDataSize, NULL);

    if (dwBytesRead != dwDataSize)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WatsonReadData: KernelLibIoControl failed, pDataBuffer=0x%08X, dwOffset=0x%08X, dwDataSize=0x%08X\r\n", pDataBuffer, dwOffset, dwDataSize));
        hRes = E_FAIL; 
        goto Exit;
    }

    if (fCalculateCRC)
    {
        // Make sure we are reading in sequentially
        KD_ASSERT(g_dwReadOffset == dwOffset);
        if (g_dwReadOffset != dwOffset)
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WatsonReadData: Dump file not read sequentially, Expected Offset=%u, Actual Offset=%u\r\n", g_dwReadOffset, dwOffset));
            hRes = E_FAIL; 
            goto Exit;
        }

        for (dwCRCLoop = 0; dwCRCLoop < dwDataSize; ++dwCRCLoop)
        {
            // Add up all the bytes in the dump file (Simple CRC)
            g_dwReadCRC += *((BYTE *)pDataBuffer + dwCRCLoop);
        }

        g_dwReadOffset += dwDataSize;
    }
    
    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WatsonReadData: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WatsonWriteData

    Writes the data to the output stream, either reserved memory or KDBG.
----------------------------------------------------------------------------*/
static HRESULT WatsonWriteData(DWORD dwOffset, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteLoop = 0;
    BYTE  bDataOut;
    BOOL  fExceptionEncountered = FALSE;
    DWORD dwWriteLoopError = 0;
    DWORD dwExceptionCount = 0;
    PVOID pAddr;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"WatsonWriteData: dwOffset=0x%08X pDataBuffer=0x%08x, dwDataSize=0x%08X, fCRC=%d\r\n", dwOffset, pDataBuffer, dwDataSize, fCalculateCRC));

    if (((dwOffset + dwDataSize) > g_dwReservedMemorySize) || // Check for buffer overflow
        (dwOffset > (dwOffset + dwDataSize)))                 // Check for integer overflow
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Trying to write beyond size of reserved memory, Reserved Size=0x%08X, dwOffset=0x%08X, dwDataSize=0x%08X\r\n", g_dwReservedMemorySize, dwOffset, dwDataSize));
        hRes = E_OUTOFMEMORY;
        goto Exit;
    }
    
    if (fCalculateCRC)
    {
        // Make sure we are writing out sequentially
        KD_ASSERT(g_dwWrittenOffset == dwOffset);
        if (g_dwWrittenOffset != dwOffset)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Data not written sequentially, Expected Offset=%u, Actual Offset=%u\r\n", g_dwWrittenOffset, dwOffset));
            hRes = E_FAIL; 
            goto Exit;
        }
        g_dwWrittenOffset += dwDataSize;
    }

    for (dwWriteLoop = 0; dwWriteLoop < dwDataSize; ++dwWriteLoop)
    {
        pAddr = (PVOID)((BYTE *)pDataBuffer + dwWriteLoop);
        // Just in case we may be reading from invalid memory, catch possible exceptions
        // Prevent hitting exceptions in the debugger
//TODO: MICCHEN: We should leave faults on so we can see where we got an exception.
        DisableFaults ();
        __try
        {
            bDataOut = *((BYTE *)pAddr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (FALSE == fExceptionEncountered)
            {
                dwWriteLoopError = dwWriteLoop;
                fExceptionEncountered = TRUE;
            }
            bDataOut = 0;
            ++ dwExceptionCount;
        }
        // Enable hitting exceptions in the debugger
        EnableFaults ();

        if (g_hostCapture)
        {
            hRes = WatsonWriteKDBG(bDataOut, FALSE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: WatsonWriteKDBG failed, hRes=0x%08X, pDataBuffer=0x%08X, dwOffset=0x%08X, dwWriteLoop=0x%08X\r\n", hRes, pDataBuffer, dwOffset, dwWriteLoop));
                hRes = E_FAIL;
                goto Exit;
            }
        }
        else
        {
            // Write out byte of memory to reserved memory location
            if (NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_WRITEWATSON, &bDataOut, 1, NULL, (dwOffset+dwWriteLoop), NULL) != 1)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: KernelLibIoControl failed, pDataBuffer=0x%08X, dwOffset=0x%08X, dwWriteLoop=0x%08X\r\n", pDataBuffer, dwOffset, dwWriteLoop));
                hRes = E_FAIL; 
                goto Exit;
            }
        }

        if (fCalculateCRC)
        {
            // Add up all the bytes written so far (Simple CRC)
            g_dwWrittenCRC += bDataOut;
        }

    }
    
    if (TRUE == fExceptionEncountered)
    {
        // This is not an error, just a warning (We should try avoid reading invalid memory)
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Exception while reading memory, Buffer=0x%08X, 1st Offset=0x%08X, #Exception=%u\r\n", 
                                  pDataBuffer, dwWriteLoopError, dwExceptionCount));
    }
        
    hRes = S_OK;

Exit:
    return hRes;
}


/*----------------------------------------------------------------------------
    WatsonWritePhysicalBufferData

    Writes the data of a physical buffer to the output stream, either reserved memory or KDBG.
----------------------------------------------------------------------------*/
static HRESULT WatsonWritePhysicalBufferData(DWORD dwOffset, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteLoop = 0;
    BYTE  bDataOut = 0;
    PBYTE pbTempPage = NULL;
    BOOL  fExceptionEncountered = FALSE;
    DWORD dwWriteLoopError = 0;
    DWORD dwExceptionCount = 0;
    PVOID pAddr;
    MEMADDR PhysicalSource;
    BOOL fError = FALSE;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"WatsonWritePhysicalBufferData: dwOffset=0x%08X pDataBuffer=0x%08x, dwDataSize=0x%08X, fCRC=%d\r\n", dwOffset, pDataBuffer, dwDataSize, fCalculateCRC));

    if (((dwOffset + dwDataSize) > g_dwReservedMemorySize) || // Check for buffer overflow
        (dwOffset > (dwOffset + dwDataSize)))                 // Check for integer overflow
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Trying to write beyond size of reserved memory, Reserved Size=0x%08X, dwOffset=0x%08X, dwDataSize=0x%08X\r\n", g_dwReservedMemorySize, dwOffset, dwDataSize));
        hRes = E_OUTOFMEMORY;
        goto Exit;
    }
    
    if (fCalculateCRC)
    {
        // Make sure we are writing out sequentially
        KD_ASSERT(g_dwWrittenOffset == dwOffset);
        if (g_dwWrittenOffset != dwOffset)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Data not written sequentially, Expected Offset=%u, Actual Offset=%u\r\n", g_dwWrittenOffset, dwOffset));
            hRes = E_FAIL; 
            goto Exit;
        }
        g_dwWrittenOffset += dwDataSize;
    }

    pAddr = pDataBuffer;
    DWORD dwBytesOnPage = (PAGEALIGN_DOWN((DWORD)pAddr) + VM_PAGE_SIZE) - (DWORD)pAddr;        
    DWORD dwBytesToWrite = min(dwBytesOnPage, dwDataSize);

    do
    {
        //Map the physical page in
        SetPhysAddr(&PhysicalSource, pAddr);
        pbTempPage = (PBYTE)DD_MapAddr(&PhysicalSource);
        for(DWORD i=0; i<dwBytesToWrite; i++)
        {            
            if(pbTempPage)
            {
                DisableFaults ();
                __try
                {
                    bDataOut = pbTempPage[i];
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    fError = TRUE;
                }            
                // Enable hitting exceptions in the debugger
                EnableFaults ();
            }
            else
            {
                fError = TRUE;
            }
            
            if(fError)
            {
                if (FALSE == fExceptionEncountered)
                {
                    dwWriteLoopError = dwWriteLoop;
                    fExceptionEncountered = TRUE;
                }
                bDataOut = 0;
                ++ dwExceptionCount;
            }

            if (g_hostCapture)
            {
                hRes = WatsonWriteKDBG(bDataOut, FALSE);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: WatsonWriteKDBG failed, hRes=0x%08X, pDataBuffer=0x%08X, dwOffset=0x%08X, dwWriteLoop=0x%08X\r\n", hRes, pDataBuffer, dwOffset, dwWriteLoop));
                    DD_UnMapAddr(&PhysicalSource);        
                    hRes = E_FAIL;
                    goto Exit;
                }
            }
            else
            {
                // Write out byte of memory to reserved memory location
                if (NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_WRITEWATSON, &bDataOut, 1, NULL, (dwOffset+dwWriteLoop), NULL) != 1)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: KernelLibIoControl failed, pDataBuffer=0x%08X, dwOffset=0x%08X, dwWriteLoop=0x%08X\r\n", pDataBuffer, dwOffset, dwWriteLoop));
                    DD_UnMapAddr(&PhysicalSource);        
                    hRes = E_FAIL; 
                    goto Exit;
                }
            }

            if (fCalculateCRC)
            {
                // Add up all the bytes written so far (Simple CRC)
                g_dwWrittenCRC += bDataOut;
            }
            
        }

        DD_UnMapAddr(&PhysicalSource);        
        dwWriteLoop += dwBytesToWrite;
        pAddr = (PVOID)((BYTE *)pDataBuffer + dwWriteLoop);               
        
        dwBytesToWrite = min(VM_PAGE_SIZE, dwDataSize - dwWriteLoop);

    }
    while(dwWriteLoop < dwDataSize);
    
    if (TRUE == fExceptionEncountered)
    {
        // This is not an error, just a warning (We should try avoid reading invalid memory)
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Exception while reading memory, Buffer=0x%08X, 1st Offset=0x%08X, #Exception=%u\r\n", 
                                  pDataBuffer, dwWriteLoopError, dwExceptionCount));
    }
        
    hRes = S_OK;

Exit:
    return hRes;    
}


static HRESULT WatsonWriteData(DWORD dwOffset, PPROCESS VMProcess, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    PPROCESS PreviousVM = pVMProc;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"++DwDmpGen!WatsonWriteData: %08X(%s) %08X, %d bytes\r\n", VMProcess,
                                   VMProcess? VMProcess->lpszProcName : L"(no proc)",
                                   pDataBuffer,
                                   dwDataSize));

    // If necessary, switch the VM to the VMProcess
    // It is interesting to note that despite the fact that we will restore the VM afterwards,
    // this operation is potentially destructive to the state of the OS.  Basically, if the 
    // previous pVMProc was null, then on restore, g_pKData->pVMPrc will not be updated.
    if (VMProcess != pVMProc && reinterpret_cast<DWORD> (pDataBuffer) < VM_KMODE_BASE)
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WatsonWriteData: Switch VM from 0x%08x(%s) to 0x%08X(%s)\r\n", 
                                       pVMProc, pVMProc? pVMProc->lpszProcName:L"noname",
                                       VMProcess, VMProcess->lpszProcName));
        PreviousVM = SwitchVM(VMProcess);
    }

    // Do the memory access / copy.
    HRESULT hr = WatsonWriteData (dwOffset, pDataBuffer, dwDataSize, fCalculateCRC);

    // If necessary, switch the VM back.
    if (PreviousVM != pVMProc && reinterpret_cast<DWORD> (pDataBuffer) < VM_KMODE_BASE)
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WatsonWriteData: Restore VM from 0x%08X(%s) to 0x%08x(%s)\r\n",
                                       pVMProc, pVMProc? pVMProc->lpszProcName:L"noname",
                                       PreviousVM, PreviousVM->lpszProcName));
        SwitchVM(PreviousVM);
    }

    return hr;
}


static DWORD __forceinline DWGetBucketEventType ()
{
    if (g_hostCapture)
    {
        return (DWORD)WATSON_HOSTCAPTURE_EVENT;
    }
    else if (g_fCaptureDumpFileOnDeviceCalled)
    {
        return (DWORD)WATSON_SNAPSHOT_EVENT;
    }
    else if (g_fStackBufferOverrun)
    {
        return (DWORD)WATSON_BEX_EVENT;
    }

    // Do we have a positive diagnosis?
    if (g_nDiagnoses)
    {
        // Bucket on first diagnosis in list.  
        switch(g_ceDmpDiagnoses[0].Type)
        {
        case Type_Deadlock:
            return (DWORD) WATSON_DEADLOCK_EVENT;

        case Type_Starvation:
            return (DWORD) WATSON_STARVATION_EVENT;

        case Type_LowMemory:
            return (DWORD) WATSON_LOW_MEMORY_EVENT;

        case Type_HeapCorruption:
            return (DWORD) WATSON_HEAP_CORRUPTION_EVENT;

        case Type_Stack:
            return (DWORD) WATSON_STACK_EVENT;

        case Type_UnrefedObject:
            return (DWORD) WATSON_UNREFED_OBJECT_EVENT;

        case Type_AbandonedSync:
            return (DWORD) WATSON_ABANDONED_SYNC_EVENT;

        default:
            break;
        }
    }

    // Standard unhandled exception
    return (DWORD)WATSON_EXCEPTION_EVENT;
}


static BOOL DWWalkDownStack (CallSnapshot3 const *csexFrames, DWORD &dwFrameCount, DWORD const dwFramesReturned,
    DWORD const dwImageBase, DWORD const dwImageSize, DWORD &dwPC, PPROCESS &pProcess)
{
    BOOL fFramesLeft = TRUE;
    
    while ((dwImageBase <= dwPC) && (dwPC < (dwImageBase + dwImageSize)) && (dwImageBase < (dwImageBase + dwImageSize)) && (fFramesLeft))
    {
        if (dwFrameCount < dwFramesReturned)
        {
            dwPC = csexFrames[dwFrameCount].dwReturnAddr;
            pProcess = OsAxsHandleToProcess((HANDLE)csexFrames[dwFrameCount].dwVMProc);
            ++ dwFrameCount;
        }
        else
        {
            fFramesLeft = FALSE;
        }
    }

    return fFramesLeft;
}


//
// Given a process and a program counter in it, return bucket parameter fields.
//
void GetBucketParametersFromProcess (PPROCESS Process,
                                     DWORD& ModName,
                                     ULONG32& ModStamp,
                                     ULONG32& ModVersionMS,
                                     ULONG32& ModVersionLS,
                                     ULONG32& DebugFlag)
{
    ModName  = (DWORD) Process->lpszProcName;
    ModStamp = Process->e32.e32_timestamp;

    VS_FIXEDFILEINFO vsFixedInfo = {0};
    HRESULT hr = GetVersionInfo (Process, FALSE, &vsFixedInfo);
    if (SUCCEEDED (hr))
    {
        ModVersionMS = vsFixedInfo.dwFileVersionMS;
        ModVersionLS = vsFixedInfo.dwFileVersionLS;
        DebugFlag    = (vsFixedInfo.dwFileFlags & VS_FF_DEBUG)? TRUE : FALSE;
    }
    else
    {
        ModVersionMS = (Process->e32.e32_cevermajor << 16) | 
                       (Process->e32.e32_ceverminor & 0xFFFF);
        ModVersionLS = 0;
        DebugFlag    = FALSE;
    }
}


/*----------------------------------------------------------------------------
    GetBucketParameters

    Get the bucket parameters from exception information.
----------------------------------------------------------------------------*/
HRESULT GetBucketParameters(PCEDUMP_BUCKET_PARAMETERS pceDmpBucketParameters, PTHREAD pDmpThread, PPROCESS pDmpProc)
{
    HRESULT hRes = E_FAIL;

    DWORD dwPC, dwExcAddr, dwPCOriginal;
    PPROCESS pVMProcessOriginal, pVMProcess;
    PTHREAD  pThread;
    BOOL fFoundMod = FALSE;
    VS_FIXEDFILEINFO vsFixedInfo = {0};
    CallSnapshot3 csexFrames[STACK_FRAMES_BUFFERED] = {0};
    DWORD dwFramesReturned, dwFrameCount;
    BOOL  fDifferentPCorProcess = FALSE;
    ULONG32 UnusedFlag;
    BOOL fDDxBreak = IsDDxBreak();
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!GetBucketParameters: Enter\r\n"));

    ZeroMemory((void*)pceDmpBucketParameters, sizeof(CEDUMP_BUCKET_PARAMETERS));

    pceDmpBucketParameters->SizeOfHeader = sizeof(CEDUMP_BUCKET_PARAMETERS);
    pceDmpBucketParameters->EventType = DWGetBucketEventType ();

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: hCurProc=0x%08X, hCurThread=0x%08X, hOwnerProc=0x%08X\r\n",
                                GetPCB()->pCurPrc->dwId, GetPCB()->pCurThd->dwId, (DWORD)((PROCESS *)(GetPCB()->pCurThd->pprcOwner)->dwId)));

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: hDmpProc=0x%08X, hDmpThread=0x%08X, hOwnerProc=0x%08X\r\n",
                                g_pDmpProc->dwId, g_pDmpThread->dwId, (DWORD)((PROCESS *)(g_pDmpThread->pprcOwner)->dwId)));

    pVMProcessOriginal = pVMProcess = pDmpProc ? pDmpProc : g_pDmpProc;
    pThread = pDmpThread ? pDmpThread : g_pDmpThread;

    if ((pThread == pCurThread) && !fDDxBreak)
    {
        // Default to the Exception PC
        dwPCOriginal = dwPC = CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context);
        dwExcAddr = (DWORD) DD_ExceptionState.exception->ExceptionAddress;
        KD_ASSERT (pVMProcess == pVMProc);
        
#ifdef DEBUG
        if ((dwExcAddr < dwPC) || (dwExcAddr > (dwPC+4)))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: WARNING: Exception frame PC (0x%08X) is different than Exception address (0x%08X).\r\n", dwPC, dwExcAddr));
        }
#endif // DEBUG
        
        // Check if CaptureDumpFileOnDevice API called for current thread
        if (g_fCaptureDumpFileOnDeviceCalled)
        {
            // Unwind two frames from point of exception (CaptureDumpFileOnDevice API + Calling function)
            dwFramesReturned = GetCallStack(pThread, 2, csexFrames, STACKSNAP_NEW_VM, 0);
            if (dwFramesReturned != 2)
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: GetCallStack did not unwind 2 frames for CaptureDumpFileOnDevice API, dwFramesReturned=%u.\r\n", dwFramesReturned));
                // Don't fail, just go with the exception PC

            }
            else
            {
                // Get the PC and process of the frame calling CaptureDumpFileOnDevice API
                // this will provide bucketing data for the module that called the API
                dwFrameCount = 1;
                dwPC = csexFrames[dwFrameCount].dwReturnAddr;
                ++ dwFrameCount;
            }
        }
    }
    else
    {
        // We should only get here when CaptureDumpFileOnDevice API is called for another thread
        // or we are in a DDx break
        KD_ASSERT(g_fCaptureDumpFileOnDeviceCalled || fDDxBreak);

        dwPCOriginal = dwPC = GetThreadIP(pThread);

        // Check if PC in Nk.exe (Process 0) & Nk.exe not owner of thread
        // If PC is in Nk.exe then we try unwind out of NK.exe & Coredll.dll
        // This should give us better bucket parameters, otherwise we always get Nk.EXE + KCall offset for buckets
        DWORD const NKImageBase = reinterpret_cast<DWORD> (g_pprcNK->BasePtr);
        DWORD const NKImageSize = g_pprcNK->e32.e32_vsize;
        DWORD const NKImageEnd = NKImageBase + NKImageSize;

        // Make sure that the dump process matches the dump thread's vm.
        KD_ASSERT (pVMProcess == pThread->pprcVM);

        if (NKImageBase <= dwPC && dwPC < NKImageEnd && NKImageBase < NKImageEnd && pThread->pprcOwner != g_pprcNK)
        {
            // Unwind frames from current PC 
            dwFramesReturned = GetCallStack (pThread, STACK_FRAMES_BUFFERED, csexFrames, STACKSNAP_NEW_VM, 0);
            if (dwFramesReturned <= 0)
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: GetCallStack did not unwind frames from Nk.exe, dwFramesReturned=%u.\r\n", dwFramesReturned));
                // Don't fail, just go with the NK.exe PC
            }
            else
            {
                BOOL fFramesLeft = TRUE;

                // Walk down frames until we are out of nk.exe (Process 0)
                
                // Start at top frame
                dwFrameCount = 0;

                fFramesLeft = DWWalkDownStack (csexFrames, dwFrameCount, dwFramesReturned, NKImageBase, NKImageSize, dwPC, pVMProcess);

                // Now Walk down frames until we are out of coredll.dll 
                // If we don't have any in coredll.dll, then we still continue
                // since coredll.dll may not always be in the call stack, 
                // especially with retail x86.
                if (fFramesLeft)
                {
                    PMODULELIST pModList = reinterpret_cast<PMODULELIST> (g_pprcNK->modList.pFwd);
                    
                    while (pModList && pModList != reinterpret_cast<PMODULELIST> (&g_pprcNK->modList))
                    {
                        if (pModList->pMod && pModList->pMod == pModList->pMod->lpSelf)
                        {
                            if (wcsnicmp(pModList->pMod->lpszModName, DDX_PROBE_DLL, wcslen(pModList->pMod->lpszModName)) == 0)
                            {
                                // probe.dll found
                                fFramesLeft = DWWalkDownStack (csexFrames, dwFrameCount, dwFramesReturned,
                                                               reinterpret_cast<DWORD> (pModList->pMod->BasePtr), pModList->pMod->e32.e32_vsize,
                                                               dwPC, pVMProcess);
                                //break;
                            }

                            if (wcsnicmp(pModList->pMod->lpszModName, L"kernel.dll", wcslen(pModList->pMod->lpszModName)) == 0)
                            {
                                // kernel.dll found
                                fFramesLeft = DWWalkDownStack (csexFrames, dwFrameCount, dwFramesReturned,
                                                               reinterpret_cast<DWORD> (pModList->pMod->BasePtr), pModList->pMod->e32.e32_vsize,
                                                               dwPC, pVMProcess);
                                //break;
                            }

                            if (wcsnicmp(pModList->pMod->lpszModName, L"coredll.dll", wcslen(pModList->pMod->lpszModName)) == 0)
                            {
                                // Coredll.dll found
                                fFramesLeft = DWWalkDownStack (csexFrames, dwFrameCount, dwFramesReturned,
                                                               reinterpret_cast<DWORD> (pModList->pMod->BasePtr), pModList->pMod->e32.e32_vsize,
                                                               dwPC, pVMProcess);
                                break;
                            }
                        }
                        else
                        {
                            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetBucketParameters: Invalid module? 0X%08X on 0X%08X\r\n", pModList->pMod, pModList));
                        }
                        pModList = reinterpret_cast<PMODULELIST> (pModList->link.pFwd);
                    }
                }

                if (!fFramesLeft)
                {
                    // Ran out of frames, in which case revert to the original PC & Process
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: WARNING: Walking out of Nk.exe walked to many frames, reverting to orignal PC, dwFrameCount=%u.\r\n", dwFrameCount));
                    dwPC = dwPCOriginal;
                    pVMProcess = pVMProcessOriginal;
                }
            }
        }
    }

    if ((dwPC != dwPCOriginal) || (pVMProcess != pVMProcessOriginal))
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!GetBucketParameters: CaptureDumpFileOnDevice API called, using different PC or Process\r\n"));
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!GetBucketParameters: New PC=0x%08X, Original PC=0x%08X, New Process=0x%08X, Original Process=0x%08X\r\n",
                                      dwPC, dwPCOriginal, (pVMProcess ? pVMProcess->dwId: 0), pVMProcessOriginal->dwId));
        fDifferentPCorProcess = TRUE;
    }

    while (!fFoundMod)
    {
        if (pVMProcess)
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: PC=0x%08X\r\n", dwPC));

            // Get the bucket parameters for the Application
            GetBucketParametersFromProcess (pVMProcess,
                                            pceDmpBucketParameters->AppName,
                                            pceDmpBucketParameters->AppStamp,
                                            pceDmpBucketParameters->AppVerMS,
                                            pceDmpBucketParameters->AppVerLS,
                                            pceDmpBucketParameters->fDebug);

            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: AppName=%s, AppStamp=0x%08X, AppVer=%u.%u.%u.%u, fDebug=%u\r\n", 
                                        (LPWSTR)pceDmpBucketParameters->AppName,
                                        pceDmpBucketParameters->AppStamp,
                                        (WORD)(pceDmpBucketParameters->AppVerMS >> 16),
                                        (WORD)(pceDmpBucketParameters->AppVerMS & 0xFFFF),
                                        (WORD)(pceDmpBucketParameters->AppVerLS >> 16),
                                        (WORD)(pceDmpBucketParameters->AppVerLS & 0xFFFF),
                                        pceDmpBucketParameters->fDebug));

            // Get the bucket parameters for the Module, may be in a process or a DLL
            if (dwPC < VM_DLL_BASE)
            {
                DWORD const ImageBase = reinterpret_cast<DWORD> (pVMProcess->BasePtr);
                DWORD const ImageSize = pVMProcess->e32.e32_vsize;
                DWORD const ImageEnd  = ImageBase + ImageSize;
        
                if ((ImageBase <= dwPC) && (dwPC < ImageEnd) && (ImageBase < ImageEnd))
                {
                    // Correct process found
                    // Exception occured within process, so exception module is a process
                    fFoundMod = TRUE;
                    GetBucketParametersFromProcess (pVMProcess,
                                                    pceDmpBucketParameters->ModName,
                                                    pceDmpBucketParameters->ModStamp,
                                                    pceDmpBucketParameters->ModVerMS,
                                                    pceDmpBucketParameters->ModVerLS,
                                                    UnusedFlag);
                    pceDmpBucketParameters->Offset = dwPC - ImageBase;
                }
            }
            else
            {
                // Exception occured in a loaded DLL, find correct module
                PMODULELIST pModList = reinterpret_cast<PMODULELIST> (g_pprcNK->modList.pFwd);

                while (pModList && (pModList != reinterpret_cast<PMODULELIST> (&g_pprcNK->modList)))
                {
                    if (pModList->pMod && pModList->pMod == pModList->pMod->lpSelf)
                    {
                        DWORD const ImageBase = reinterpret_cast<DWORD> (pModList->pMod->BasePtr);
                        DWORD const ImageSize = static_cast<DWORD> (pModList->pMod->e32.e32_vsize);
                        DWORD const ImageEnd = ImageBase + ImageSize;
                        
                        if (ImageBase <= dwPC && dwPC < ImageEnd && ImageBase < ImageEnd)
                        {
                            // Correct module found
                            fFoundMod = TRUE;
                            pceDmpBucketParameters->ModName  = (DWORD)pModList->pMod->lpszModName;
                            pceDmpBucketParameters->ModStamp = pModList->pMod->e32.e32_timestamp;
                            pceDmpBucketParameters->Offset = dwPC - ImageBase;
                            hRes = GetVersionInfo(pModList->pMod, TRUE, &vsFixedInfo);
                            if (SUCCEEDED(hRes))
                            {
                                pceDmpBucketParameters->ModVerMS = vsFixedInfo.dwFileVersionMS;
                                pceDmpBucketParameters->ModVerLS = vsFixedInfo.dwFileVersionLS;
                            }
                            else
                            {
                                pceDmpBucketParameters->ModVerMS = (pModList->pMod->e32.e32_cevermajor << 16) 
                                                                   | (pModList->pMod->e32.e32_ceverminor & 0xFFFF);
                                pceDmpBucketParameters->ModVerLS = 0;
                            }
                            break;
                        }
                    }

                    pModList = reinterpret_cast<PMODULELIST> (pModList->link.pFwd);
                }
            }
        }

        if (fFoundMod)
        {
            // Module found
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: ModName=%s, ModStamp=0x%08X, ModVer=%u.%u.%u.%u, Offset=0x%08X\r\n", 
                                        (LPWSTR)pceDmpBucketParameters->ModName,
                                        pceDmpBucketParameters->ModStamp,
                                        (WORD)(pceDmpBucketParameters->ModVerMS >> 16),
                                        (WORD)(pceDmpBucketParameters->ModVerMS & 0xFFFF),
                                        (WORD)(pceDmpBucketParameters->ModVerLS >> 16),
                                        (WORD)(pceDmpBucketParameters->ModVerLS & 0xFFFF),
                                        pceDmpBucketParameters->Offset));
        }
        else
        {
            // No module found
            if (fDifferentPCorProcess)
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: Could not find module where exception occured, trying original PC and Process\r\n"));
                dwPC = dwPCOriginal;
                pVMProcess = pVMProcessOriginal;
                fDifferentPCorProcess = FALSE;
            }
            else
            {
                // Still no module found.  Default to the current process.
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: Could not find module where exception occured.  Bad PC.\r\n"));

                GetBucketParametersFromProcess (pVMProcess,
                                                pceDmpBucketParameters->ModName,
                                                pceDmpBucketParameters->ModStamp,
                                                pceDmpBucketParameters->ModVerMS,
                                                pceDmpBucketParameters->ModVerLS,
                                                UnusedFlag);

                // Do not change PC because it does not correspond to any known module / process.
                pceDmpBucketParameters->Offset = dwPC;
                fFoundMod = TRUE;
            }
        }
    }

    // Get the bucket parameters for the Owner Application
    GetBucketParametersFromProcess (pThread->pprcOwner,
                                    pceDmpBucketParameters->OwnerName,
                                    pceDmpBucketParameters->OwnerStamp,
                                    pceDmpBucketParameters->OwnerVerMS,
                                    pceDmpBucketParameters->OwnerVerLS,
                                    UnusedFlag);

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: OwnerName=%s, OwnerStamp=0x%08X, OwnerVer=%u.%u.%u.%u\r\n", 
                                (LPWSTR)pceDmpBucketParameters->OwnerName,
                                pceDmpBucketParameters->OwnerStamp,
                                (WORD)(pceDmpBucketParameters->OwnerVerMS >> 16),
                                (WORD)(pceDmpBucketParameters->OwnerVerMS & 0xFFFF),
                                (WORD)(pceDmpBucketParameters->OwnerVerLS >> 16),
                                (WORD)(pceDmpBucketParameters->OwnerVerLS & 0xFFFF)));
    
    hRes = S_OK;
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!GetBucketParameters: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}



/*----------------------------------------------------------------------------
    GetCaptureDumpFileParameters

    Get Parameters passed to the CaptureDumpFileOnDevice API.
----------------------------------------------------------------------------*/
static HRESULT GetCaptureDumpFileParameters(EXCEPTION_INFO * pexceptionInfo)
{
    HRESULT hRes = E_FAIL;
    HANDLE hDumpProc;
    HANDLE hDumpThread;
    BOOL   fTrustedApp;
    PEXCEPTION_RECORD pExceptionRecord = pexceptionInfo->exception;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!GetCaptureDumpFileParameters: Enter\r\n"));

    // Check if CaptureDumpFileOnDevice API called
    if (g_fCaptureDumpFileOnDeviceCalled)
    {
        // Get the process and thread to dump 
        hDumpProc   = reinterpret_cast<HANDLE> (pExceptionRecord->ExceptionInformation[0]);
        hDumpThread = reinterpret_cast<HANDLE> (pExceptionRecord->ExceptionInformation[1]);
        fTrustedApp = static_cast<BOOL> (pExceptionRecord->ExceptionInformation[3]);

        if (hDumpProc == reinterpret_cast<HANDLE> (-1) && hDumpThread == reinterpret_cast<HANDLE> (-1))
        {
            // When Process ID & Thread ID are both -1 then we are passing in exception pointers from ReportFault
            g_fCaptureDumpFileOnDeviceCalled = FALSE;
            g_fReportFaultCalled = TRUE;

            EXCEPTION_POINTERS ep;
            PVOID    pIP;
            PPROCESS pVM = DD_GetProperVM(NULL, (void *)pExceptionRecord->ExceptionInformation[2]);

            if(OsAxsReadMemory(&ep, pVM, (void *)pExceptionRecord->ExceptionInformation[2], sizeof(ep)) != sizeof(ep))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Exception Pointers Pointer (0x%08X) NULL or unsafe\r\n", pExceptionRecord->ExceptionInformation[2]));
                hRes = E_INVALIDARG;
                goto Exit;
            }

            pVM = DD_GetProperVM(NULL, ep.ExceptionRecord);
            if(OsAxsReadMemory(&g_exceptionRecordReportFault, pVM, ep.ExceptionRecord, sizeof(g_exceptionRecordReportFault)) != sizeof(g_exceptionRecordReportFault))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Exception Record Pointer (0x%08X) NULL or unsafe\r\n", ep.ExceptionRecord));
                hRes = E_INVALIDARG;
                goto Exit;                
            }

            pVM = DD_GetProperVM(NULL, ep.ContextRecord);
            if(OsAxsReadMemory(&g_contextReportFault, pVM, ep.ContextRecord, sizeof(g_contextReportFault)) != sizeof(g_contextReportFault))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Context Record Pointer (0x%08X) NULL or unsafe\r\n", ep.ContextRecord));
                hRes = E_INVALIDARG;
                goto Exit;
            }
                        
            // Point to local copy of exception & context records for use in rest of dump file processing
            pexceptionInfo->exception = &g_exceptionRecordReportFault;
            pexceptionInfo->context = &g_contextReportFault;

            // Validate the Instruction Pointer from the context record is within the applications valid memory space            
            pIP = (LPVOID) CONTEXT_TO_PROGRAM_COUNTER (pexceptionInfo->context);
            if (NULL == pIP)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Instruction Pointer (0x%08X) NULL or unsafe\r\n", 
                                          CONTEXT_TO_PROGRAM_COUNTER (pexceptionInfo->context)));
                hRes = E_INVALIDARG;
                goto Exit;
            }

            g_fStackBufferOverrun = (STATUS_STACK_BUFFER_OVERRUN == pexceptionInfo->exception->ExceptionCode);

            if (g_fStackBufferOverrun)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Security check called ReportFault, creating %s event type\r\n",
                                          WATSON_BEX_EVENT));
            }

            hRes = S_OK;
            goto Exit;
        }

        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: API call to CaptureDumpFileOnDevice, dwProcessId=0x%08X, dwThreadId=0x%08X\r\n",
                                  hDumpProc, hDumpThread));

        if (fTrustedApp == FALSE || hDumpProc == 0)
        {   
            // If untrusted apps call the CaptureDumpFileOnDevice API, the requested process defaults to the current process.
            // Or, if the CaptureDumpFileOnDevice API was called with "0", fill in the full process ID
            hDumpProc = (HANDLE) GetPCB()->pCurPrc->dwId;
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: fTrustedApp==%d, dwProcessId being used is 0\r\n",
                                      fTrustedApp));
        }
        
        // Validate the passed in thread ID, if set
        if (hDumpThread != 0)
        {
            g_pDmpThread = OsAxsHandleToThread(hDumpThread);
            if (NULL == g_pDmpThread)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid Thread Id passed to CaptureDumpFileOnDevice, dwThreadId=0x%08X\r\n", hDumpThread));
                hRes = E_INVALIDARG;
                goto Exit;
            }
        }

        // Validate the passed in Process ID
        g_pDmpProc = OsAxsHandleToProcess(hDumpProc);
        if (NULL == g_pDmpProc)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid Process Id passed to CaptureDumpFileOnDevice, dwProcessId=0x%08X\r\n", hDumpProc));
            hRes = E_INVALIDARG;
            goto Exit;
        }

        // Check if a thread was specified
        if (hDumpThread > 0)
        {
            // Make sure thread is owned by the specified process for untrusted apps
            if (g_pDmpThread->pprcOwner != g_pDmpProc)
            {
                if (!fTrustedApp)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Un-Trusted Process does not own Thread passed to CaptureDumpFileOnDevice, dwProcessId=0x%08X, dwThreadId=0x%08X\r\n",
                                              hDumpProc, hDumpThread));
                    hRes = E_ACCESSDENIED;
                    goto Exit;
                }
                
                // Make sure thread is at least running in specified process for trusted apps
                if (g_pDmpThread->pprcVM != g_pDmpProc)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Thread is not owned by or running in Process passed to CaptureDumpFileOnDevice, dwProcessId=0x%08X, dwThreadId=0x%08X\r\n",
                                              hDumpProc, hDumpThread));
                    hRes = E_INVALIDARG;
                    goto Exit;
                }
            }
        }
        else
        {
            // No thread specified
            if (g_pDmpProc == GetPCB()->pCurPrc)
            {
                // For the current process use the current thread
                g_pDmpThread = pCurThread;
            }
            else
            {
                // For any other process use the last created thread
                g_pDmpThread = reinterpret_cast<PTHREAD> (g_pDmpProc->thrdList.pFwd);

                // Process with no thread.  This is bad, very bad.
                KD_ASSERT (g_pDmpThread != reinterpret_cast<PTHREAD> (&g_pDmpProc->thrdList));
            }
        }

        // Now make sure we have the current process for thread (may be different to owner)
        // Note: We may need to introduce the g_pDmpVMProc and g_pDmpActvProc...
        if (g_pDmpThread->pprcVM)
        {
            g_pDmpProc = g_pDmpThread->pprcVM;
        }
        else if (g_pDmpThread->pprcActv)
        {
            g_pDmpProc = g_pDmpThread->pprcActv;
        }
        else if (g_pDmpThread->pprcOwner)
        {
            g_pDmpProc = g_pDmpThread->pprcOwner;
        }
        else
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Dump Thread may be invalid, no process available.\r\n"));
            g_pDmpProc = g_pprcNK;
        }

        // Check if pointer to Extra Upload Files passed in
        if (pExceptionRecord->ExceptionInformation[2])
        {
            WCHAR wzExtraFilesPath[MAX_PATH];
            
            LPVOID pwzUserExtraFilesPath = reinterpret_cast<void*> (pExceptionRecord->ExceptionInformation[2]);
            DWORD dwMaxBytesAvailable = 0;  //NOTE: this is the number of BYTES that are valid (i.e. won't cause an exception) in the string passed in by the user (pExceptionRecord->ExceptionInformation[2]);
            dwMaxBytesAvailable = OsAxsReadMemory(wzExtraFilesPath, pVMProc, pwzUserExtraFilesPath, sizeof(wzExtraFilesPath));
            if(dwMaxBytesAvailable == 0)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Extra Files Path Pointer (0x%08X) NULL or unsafe\r\n", pwzUserExtraFilesPath));
                hRes = E_INVALIDARG;
                goto Exit;                
            }
            DWORD dwMaxWChars = dwMaxBytesAvailable/2;  

            size_t dwExtraFilesPathLen = wcsnlen_s(wzExtraFilesPath, dwMaxWChars);
            if(dwExtraFilesPathLen == dwMaxWChars)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid Extra Files Path String (0x%08X) NULL or unsafe\r\n", pwzUserExtraFilesPath));
                hRes = E_INVALIDARG;
                goto Exit;
            }
            
            DWORD dwExtraFilesDirectoryLen = wcslen(g_watsonDumpSettings.wzExtraFilesDirectory);

            if (dwExtraFilesDirectoryLen && g_watsonDumpSettings.wzExtraFilesDirectory[dwExtraFilesDirectoryLen-1] == L'\\')
            {
                // Compare without the final delimiter
                --dwExtraFilesDirectoryLen;
            }

            // Validate & copy path for extra upload files passed to CaptureDumpFileOnDevice API
            if ((dwExtraFilesDirectoryLen > 0) &&                                    // Check if allowed extra files
                (dwExtraFilesPathLen < MAX_PATH) &&                                  // Make sure path specified by user is valid length
                (dwExtraFilesPathLen >= dwExtraFilesDirectoryLen+2) &&               // Make sure path specified by user contains allowed path + delimiter + extra character
                (wcsnicmp(wzExtraFilesPath,                                     // Make sure start of path is same as allowed
                              g_watsonDumpSettings.wzExtraFilesDirectory,
                              dwExtraFilesDirectoryLen) == 0) &&
                (wzExtraFilesPath[dwExtraFilesDirectoryLen] == L'\\'))              // Make sure the delimiter is in the correct place
            {
                // Store this in the dump settings for use by DwXfer.dll 
                // DwXfer.dll will re-validate the path using CeGetCanonicalPathNameW
                memcpy(g_watsonDumpSettings.wzExtraFilesPath, 
                       wzExtraFilesPath,   
                       ((dwExtraFilesPathLen+1) * sizeof(WCHAR)));

                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: API call to CaptureDumpFileOnDevice, pwzExtraFilesPath='%s'\r\n",
                                          wzExtraFilesPath));
            }
            else
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid path passed to CaptureDumpFileOnDevice, pwzExtraFilesPath='%s', Allowed Path='%s\\[FolderName]'\r\n",
                                          wzExtraFilesPath, g_watsonDumpSettings.wzExtraFilesDirectory));
                g_dwLastError = ERROR_BAD_PATHNAME;
                hRes = E_FAIL;
                goto Exit;
            }
        }
    }
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!GetCaptureDumpFileParameters: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


// Scan through the memory block list and return the index of the first one that
// is free.  By design, all of the free blocks should be contiguous at the end of
// the list of blocks.
static DWORD DWFirstFreeMemoryBlock (MEMORY_BLOCK *MemoryBlocks, DWORD NumBlocks)
{
    for (DWORD i = 0; i < NumBlocks; ++i)
    {
        if (MemoryBlocks[i].dwMemorySize == EMPTY_BLOCK)
        {
            return i;
        }
    }

    return NumBlocks;
}


// Given a memory range (in NewBlock) find a block in the current list of memory
// blocks that overlaps with the new block.  The definition of overlap is the newblock
// starts within the range of an existing block.
static DWORD DWFindOverlapBlock (MEMORY_BLOCK *MemoryBlocks, DWORD LastBlock, MEMORY_BLOCK const &NewBlock)
{
    DWORD const MemStart = NewBlock.dwMemoryStart;
    
    for (DWORD i = 0; i < LastBlock; ++i)
    {
        DWORD const BlockStart = MemoryBlocks[i].dwMemoryStart;
        DWORD const BlockSize  = MemoryBlocks[i].dwMemorySize;
        DWORD const BlockEnd   = BlockStart + BlockSize;
        
        if (BlockSize != EMPTY_BLOCK)
        {
            if (NewBlock.VMProcess == MemoryBlocks[i].VMProcess && BlockStart <= MemStart && MemStart <= BlockEnd)
            {
                // Found the block.
                return i;
            }
        }
        else
        {
            // Should have been filtered out already...
            KD_ASSERT (FALSE);
        }
    }
    return LastBlock;
}


// This scans through the entire list to make sure that as a result of updating the memory range
// to see if any existing block overlaps.  If there is an overlap, the existing block is removed
// and the new block gets updated to cover the total range.
//
// This function makes sure that all of the free blocks are contiguous at the end of the block
// array by swapping in a working block into the hole opened up by the block update.
static void DWMemoryBlockOverlap (MEMORY_BLOCK *MemoryBlocks, DWORD NewBlock, DWORD EndBlock,
                                  DWORD &TotalMemorySize, DWORD &TotalMemoryBlocks)
{
    DWORD MemoryStart = MemoryBlocks[NewBlock].dwMemoryStart;
    DWORD MemorySize  = MemoryBlocks[NewBlock].dwMemorySize;
    DWORD MemoryEnd   = MemoryStart + MemorySize;
    DWORD CurrentBlock = 0;
    
    while (CurrentBlock < EndBlock)
    {
        // Don't check for overlap with self
        if (CurrentBlock != NewBlock)
        {
            // Get info for current block
            DWORD const BlockSize  = MemoryBlocks[CurrentBlock].dwMemorySize;
            DWORD const BlockStart = MemoryBlocks[CurrentBlock].dwMemoryStart;
            DWORD const BlockEnd   = BlockStart + BlockSize;
        
            // All blocks less than dwFirstEmpty should be in use
            KD_ASSERT (EMPTY_BLOCK != BlockSize);
            
            // Check if start of current block is within new/updated block (or immediately follows)
            if (MemoryBlocks[NewBlock].VMProcess == MemoryBlocks[CurrentBlock].VMProcess 
                && MemoryStart <= BlockStart && BlockStart <= MemoryEnd)
            {
                // Get the combined memory end position
                MemoryEnd = max (MemoryEnd, BlockEnd);
    
                // Remove the added size + current size from total
                TotalMemorySize -= (MemorySize + BlockStart);
                
                // Calculate the new combined size
                MemorySize = MemoryEnd - MemoryStart;
    
                // Set the new combined size
                MemoryBlocks[NewBlock].dwMemorySize = MemorySize;
    
                // Add new combined size to total
                TotalMemorySize += MemorySize;
                
                // We must now remove the current block 
                --TotalMemoryBlocks;

                // Move last block to current position (Thereby removing current block)
                DWORD const LastBlock  = EndBlock - 1;
                MemoryBlocks[CurrentBlock].dwMemoryStart= MemoryBlocks[LastBlock].dwMemoryStart;
                MemoryBlocks[CurrentBlock].dwMemorySize = MemoryBlocks[LastBlock].dwMemorySize;

                // Invalidate last block
                MemoryBlocks[LastBlock].dwMemoryStart = 0;
                MemoryBlocks[LastBlock].dwMemorySize = (DWORD) EMPTY_BLOCK;

                // Update the end pointer
                -- EndBlock;
                
                // Check if the new added block was the last, in which case it is now moved
                if (NewBlock == EndBlock)
                {
                    NewBlock = CurrentBlock;
                }
            }
        }
        ++CurrentBlock;
    }
}


/*----------------------------------------------------------------------------
    MemoryBlocksAdd

    Add a new memory block for saving to dump file.
    
    This function assumes the global 'g_memoryBlocks' has been initialized 
    to all 0xFF before starting a new dump.

    TODO: Consider probing memory to make sure that the dump code can access
    the data (might be a non-issue)?
----------------------------------------------------------------------------*/
static HRESULT MemoryBlocksAdd(PROCESS *VMProcess, DWORD dwMemoryStartAdd, DWORD dwMemorySizeAdd, BOOL fVirtual, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwFirstEmpty;
    DWORD dwBlockAdded;
    DWORD dwMemoryEndAdd;
    DWORD dwMemoryBlocksMax;
    MEMORY_BLOCK *pmemoryBlocks;
    DWORD *pdwMemorySizeTotal;
    DWORD *pdwMemoryBlocksTotal;

    MEMORY_BLOCK NewBlock;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"++DwDmpGen!MemoryBlocksAdd: Enter, fVirtual=%u, Start=0x%08X, End=0x%08X, Size=0x%08X\r\n", 
                                 fVirtual, dwMemoryStartAdd, (dwMemoryStartAdd + dwMemorySizeAdd - 1) ,dwMemorySizeAdd));

    if (fVirtual)
    {
        // All virtual memory better be qualified.
        KD_ASSERT (VMProcess);
    }

    if (dwMemoryStartAdd >= VM_KMODE_BASE)
    {
        KD_ASSERT (VMProcess == g_pprcNK);
        VMProcess = g_pprcNK;
    }

    // Make sure we only add these during the optimization phase, not the write phase
    KD_ASSERT(FALSE == fWrite);
    if (fWrite)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryBlocksAdd: Memory blocks should not be added during the write phase\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    if (TRUE == fVirtual)
    {
        dwMemoryBlocksMax = MAX_MEMORY_VIRTUAL_BLOCKS;
        pmemoryBlocks = g_memoryVirtualBlocks;
        pdwMemorySizeTotal = &g_dwMemoryVirtualSizeTotal;
        pdwMemoryBlocksTotal = &g_dwMemoryVirtualBlocksTotal;
    }
    else
    {
        dwMemoryBlocksMax = MAX_MEMORY_PHYSICAL_BLOCKS;
        pmemoryBlocks = g_memoryPhysicalBlocks;
        pdwMemorySizeTotal = &g_dwMemoryPhysicalSizeTotal;
        pdwMemoryBlocksTotal = &g_dwMemoryPhysicalBlocksTotal;
    }

    dwFirstEmpty = DWFirstFreeMemoryBlock (pmemoryBlocks, dwMemoryBlocksMax);

    NewBlock.dwMemoryStart = dwMemoryStartAdd;
    NewBlock.dwMemorySize  = dwMemorySizeAdd;
    NewBlock.VMProcess     = VMProcess;
    
    dwBlockAdded = DWFindOverlapBlock (pmemoryBlocks, dwFirstEmpty, NewBlock);
    if (dwBlockAdded < dwFirstEmpty)
    {
        // Overlaps with existing block
        DWORD const BlockStart = pmemoryBlocks[dwBlockAdded].dwMemoryStart;
        DWORD const BlockSize  = pmemoryBlocks[dwBlockAdded].dwMemorySize;
        DWORD const BlockEnd   = BlockStart + BlockSize;
        
        // Get the combined memory end position
        DWORD const BlockNewEnd = max(BlockEnd, (dwMemoryStartAdd + dwMemorySizeAdd));

        // Remove the old size from total
        *pdwMemorySizeTotal -= BlockSize;
        
        // Calculate the new size
        DWORD const BlockNewSize  = BlockNewEnd - BlockStart;

        // Set the new size
        pmemoryBlocks[dwBlockAdded].dwMemorySize = BlockNewSize;

        // Add new size to total
        *pdwMemorySizeTotal += BlockNewSize;

        KD_ASSERT(BlockNewSize == BlockNewEnd - BlockStart);

        DWMemoryBlockOverlap (pmemoryBlocks, dwBlockAdded, dwFirstEmpty, *pdwMemorySizeTotal, *pdwMemoryBlocksTotal);
        hRes = S_OK;
    }
    else if (dwFirstEmpty < dwMemoryBlocksMax)
    {
        // New Block
        pmemoryBlocks[dwFirstEmpty].dwMemoryStart = dwMemoryStartAdd;
        pmemoryBlocks[dwFirstEmpty].dwMemorySize  = dwMemorySizeAdd;
        pmemoryBlocks[dwFirstEmpty].VMProcess     = VMProcess;
        dwMemoryEndAdd = dwMemoryStartAdd + dwMemorySizeAdd;
        
        ++ (*pdwMemoryBlocksTotal);
        *pdwMemorySizeTotal += dwMemorySizeAdd;

        // Save where block added
        dwBlockAdded = dwFirstEmpty;
        
        ++ dwFirstEmpty;
        DWMemoryBlockOverlap (pmemoryBlocks, dwBlockAdded, dwFirstEmpty, *pdwMemorySizeTotal, *pdwMemoryBlocksTotal);
        hRes = S_OK;
    }
    else
    {
        // Out of room.
        
        // If we get here, then we have reached MAX_MEMORY_BLOCKS
        KD_ASSERT(dwFirstEmpty == dwMemoryBlocksMax);
        
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryBlocksAdd: No free memory block found (Total blocks=%u)\r\n", dwFirstEmpty));
        hRes = E_FAIL;
        goto Exit;
    }

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryBlocksAdd: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddAll

    Add all virtual and physical memory sections for entire device.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddAll(BOOL fVirtual, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart = 0;
    DWORD dwMemorySize = 0;
    DWORD dwSection;
    const mem_t *pfsmemblk;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddAllVirtual: Enter %d\r\n", fVirtual));

    if(fVirtual)
    {
#if defined(ARM)
        // Dump Arm FirstPT memory
        dwMemoryStart = (DWORD)ArmHigh->firstPT;
        dwMemorySize  = sizeof(ArmHigh->firstPT);

        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllVirtual: Adding Arm FirstPT memory block\r\n"));
        
        hRes = MemoryBlocksAdd(g_pprcNK, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllVirtual: MemoryBlocksAdd failed adding Arm FirstPT memory block, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
#endif

        // Dump Nk.bin Code memory
        dwMemoryStart = pTOC->physfirst;
        dwMemorySize  = pTOC->physlast - pTOC->physfirst;

        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllVirtual: Adding nk.bin memory block\r\n"));

        hRes = MemoryBlocksAdd(g_pprcNK, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllVirtual: MemoryBlocksAdd failed adding nk.bin memory block, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        // Dump base RAM section   
        dwMemoryStart = pTOC->ulRAMStart;
        if((LogPtr->nMemblks > 0) && dwMemoryStart < LogPtr->fsmemblk[0].BaseAddr)
        {
            dwMemorySize  = LogPtr->fsmemblk[0].BaseAddr - pTOC->ulRAMStart;        
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllVirtual: Adding base RAM memory block\r\n"));
            
            hRes = MemoryBlocksAdd(g_pprcNK, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllVirtual: MemoryBlocksAdd failed adding base RAM memory block, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }

        }
    }

    // Dump any extension RAM sections    
    for (dwSection = 0; dwSection < LogPtr->nMemblks; ++ dwSection)
    {
        pfsmemblk = &LogPtr->fsmemblk[dwSection];

        if (pfsmemblk->Length > 0)
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllVirtual: Adding extension RAM memory block%u\r\n", dwSection));

            if(!fVirtual && pfsmemblk->ExtnAndAttrib & DYNAMIC_MAPPED_RAM)
            {
                dwMemoryStart = PFN2PA(PFNfrom256(pfsmemblk->BaseAddr));
            }
            else if(fVirtual && !(pfsmemblk->ExtnAndAttrib & DYNAMIC_MAPPED_RAM))
            {
                dwMemoryStart = pfsmemblk->BaseAddr;
            }
            else
            {
                //continue
                continue;
            }
            dwMemorySize = pfsmemblk->Length;

            hRes = MemoryBlocksAdd(g_pprcNK, dwMemoryStart, dwMemorySize, fVirtual, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllVirtual: MemoryBlocksAdd failed adding extension RAM memory block%u, hRes=0x%08X\r\n", dwSection, hRes));
                goto Exit;
            }
        }
    }        

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddAllVirtual: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


/*----------------------------------------------------------------------------
    MemoryAddProcessHeap

    Add all R/W sections for passed in process.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddProcessHeap(PPROCESS pProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD MaxPDIndex = 0;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddProcessHeap: Enter\r\n"));

    KD_ASSERT(NULL != pProcess);
    if (NULL == pProcess)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if (!OSAXSIsProcessValid(pProcess))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  MemoryAddProcessHeap: Process 0x%08x not processed, bState = %d\r\n", pProcess->bState));
        hRes = S_OK;
        goto Exit;
    }

    if (!pProcess->ppdir)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  MemoryAddProcessHeap: Process 0x%08x lacks a pagetable directory!\r\n", pProcess->bState));
        hRes = E_FAIL;
        goto Exit;
    }

    // If the scanned process is a usermode process, then this loop only needs to
    // iterate over 00000000 to 7FFFFFFF.  Stop the scan at the start of the
    // kernel mode base.  This is to avoid the ability of ARMV6 to automagically
    // switch to the kernel pagetable entries for VA with the MSB set.
    //
    // Otherwise, just scan the entire page directory.
    if (pProcess != g_pprcNK)
    {
        MaxPDIndex = VA2PDIDX(VM_KMODE_BASE);
    }
    else
    {
        MaxPDIndex = VM_NUM_PD_ENTRIES;
    }


    // Not a standard ++ on increment because ARM needs +=4 when traversing
    // the page directory entry array.
    for (DWORD ipd = 0; ipd < MaxPDIndex; ipd = NextPDEntry(ipd))
    {
        DWORD const pde = pProcess->ppdir->pte[ipd];
        if (IsPDEntryValid(pde))
        {
            if (IsSectionMapped (pde))
            {
                // Large page mapped
                DWORD pfn = PFNfromEntry (pde);
                if (IsPageReadable (pfn) && IsPageWritable (pfn))
                {
                    DWORD va = (ipd << VM_PD_SHIFT);
                    hRes = MemoryBlocksAdd (pProcess, va, VM_PD_SIZE, VIRTUAL_MEMORY, fWrite);
                    if (FAILED(hRes))
                    {
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessHeap: MemoryBlocksAdd failed adding '%s' heap memory (large page), ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                                  pProcess->lpszProcName, pProcess->dwId, hRes));
                        goto Exit;
                    }
                }
            }
            else
            {
                // Go to second level page table.
                PPAGETABLE ppt = GetPageTable(pProcess->ppdir, ipd);
                if (ppt)
                {
                    for (DWORD ipt = 0; ipt < VM_NUM_PT_ENTRIES; ++ipt)
                    {
                        DWORD pfn = PFNfromEntry (ppt->pte[ipt]);
                        if (IsPageReadable (pfn) && IsPageWritable (pfn))
                        {
                            DWORD va = (ipd << VM_PD_SHIFT) | (ipt << VM_PT_SHIFT);
                            hRes = MemoryBlocksAdd (pProcess, va, VM_PAGE_SIZE, VIRTUAL_MEMORY, fWrite);
                            if (FAILED(hRes))
                            {
                                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessHeap: MemoryBlocksAdd failed adding '%s' heap memory (normal page), ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                                          pProcess->lpszProcName, pProcess->dwId, hRes));
                                goto Exit;
                            }
                        }
                    }
                }

            }
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddProcessHeap: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddProcessGlobals

    Add the global memory sections for passed in process.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddProcessGlobals(PPROCESS pProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;


    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddProcessGlobals: Enter\r\n"));

    KD_ASSERT(NULL != pProcess);
    if (NULL == pProcess)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if (pProcess == g_pprcNK)
    {
        if (TRUE == g_fDumpNkGlobals)
        {
            COPYentry *pce = reinterpret_cast<COPYentry*> (pTOC->ulCopyOffset);
            if (pce)
            {
                // Indicate that nk.exe globals dumped (optimization needs to know)
                g_fNkGlobalsDumped = TRUE;
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddProcessGlobals: Adding '%s' global variable memory -> ProcessID=0x%08X\r\n", 
                                             pProcess->lpszProcName, pProcess->dwId));
                
                hRes = MemoryBlocksAdd(g_pprcNK, pce->ulDest, pce->ulDestLen, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessGlobals: MemoryBlocksAdd failed adding '%s' global variable memory -> ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                              pProcess->lpszProcName, pProcess->dwId, hRes));
                    goto Exit;
                }
            }
        }
    }
    else
    {
        // All other processes
        for (DWORD dwO32count=0; dwO32count < pProcess->e32.e32_objcnt; dwO32count++)
        {
            if (pProcess->o32_ptr && (pProcess->o32_ptr[dwO32count].o32_flags & IMAGE_SCN_MEM_WRITE))
            {
                // Found a writable memory section
                DWORD const dwMemoryStart = (DWORD) pProcess->o32_ptr[dwO32count].o32_realaddr;
                DWORD const dwMemorySize = pProcess->o32_ptr[dwO32count].o32_vsize;
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddProcessGlobals: Adding '%s' global variable memory%u -> ProcessID=0x%08X\r\n", 
                                             pProcess->lpszProcName, dwO32count, pProcess->dwId));
                
                hRes = MemoryBlocksAdd(pProcess, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessGlobals: MemoryBlocksAdd failed adding '%s' global variable memory%u -> ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                              pProcess->lpszProcName, dwO32count, pProcess->dwId, hRes));
                    goto Exit;
                }
            }
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddProcessGlobals: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddDllGlobals

    Add the global memory sections for passed in process.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddDllGlobals(PMODULE pMod, HPROCESS hFrameProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    PPROCESS pProcess = NULL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddDllGlobals: Enter\r\n"));

    KD_ASSERT(pMod);
    if (!pMod)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    pProcess = OsAxsHandleToProcess (hFrameProcess);
    if (!pProcess)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddDllGlobals: HandleToProc returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    for (DWORD dwO32count=0; dwO32count < pMod->e32.e32_objcnt; dwO32count++)
    {
        if (pMod->o32_ptr && (pMod->o32_ptr[dwO32count].o32_flags & IMAGE_SCN_MEM_WRITE))
        {
            // Found a writable memory section
            DWORD const dwMemoryStart = (DWORD) pMod->o32_ptr[dwO32count].o32_realaddr;
            DWORD const dwMemorySize = pMod->o32_ptr[dwO32count].o32_vsize;
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddDllGlobals: Adding '%s' global variable memory%u for '%s'\r\n", 
                                         pMod->lpszModName, dwO32count, pProcess->lpszProcName));
            
            hRes = MemoryBlocksAdd(pProcess, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddDllGlobals: MemoryBlocksAdd failed adding '%s' global variable memory%u for '%s', hRes=0x%08X\r\n", 
                                          pMod->lpszModName, dwO32count, pProcess->lpszProcName, hRes));
                goto Exit;
            }
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddDllGlobals: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


static HRESULT DWAddAnyGlobal (DWORD ReturnAddress, HANDLE hProc, BOOL fWrite)
{
    HRESULT hr;
    PROCESS *pProcess;

    pProcess = OsAxsHandleToProcess (hProc);
    if (!pProcess)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DWAddAnyGlobal: Invalid Process Handle 0x%08x\r\n", hProc));
        hr = E_FAIL;
        goto Exit;
    }

    if (ReturnAddress < VM_DLL_BASE)
    {
        DWORD const ExeBase = reinterpret_cast<DWORD> (pProcess->BasePtr);
        DWORD const ExeSize = pProcess->e32.e32_vsize;
        DWORD const ExeEnd  = ExeBase + ExeSize;

        if (ExeBase <= ReturnAddress && ReturnAddress < ExeEnd && ExeBase < ExeEnd)
        {
            hr = MemoryAddProcessGlobals (pProcess, fWrite);
            if (FAILED (hr))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DWAddAnyGlobal: MemoryAddProcessGlobals failed adding Process Global memory, hRes=0x%08X\r\n", hr));
                goto Exit;
            }

            hr = S_OK;
            goto Exit;
        }
    }

    PMODULELIST pModList = reinterpret_cast<PMODULELIST> (g_pprcNK->modList.pFwd);
    while (pModList && pModList != reinterpret_cast<PMODULELIST> (&g_pprcNK->modList))
    {
        if (pModList->pMod && pModList->pMod == pModList->pMod->lpSelf)
        {
            DWORD const DllBase = reinterpret_cast<DWORD> (pModList->pMod->BasePtr);
            DWORD const DllSize = pModList->pMod->e32.e32_vsize;
            DWORD const DllEnd  = DllBase + DllSize;

            if (DllBase <= ReturnAddress && ReturnAddress < DllEnd && DllBase < DllEnd)
            {
                hr = MemoryAddDllGlobals (pModList->pMod, hProc, fWrite);
                if (FAILED (hr))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DWAddAnyGlobal: MemoryAddDllGlobals failed adding Module Global memory, hRes=0x%08X\r\n", hr));
                    goto Exit;
                }

                hr = S_OK;
                goto Exit;
            }
        }

        pModList = reinterpret_cast<PMODULELIST> (pModList->link.pFwd);
    }

    DEBUGGERMSG(OXZONE_ALERT,(L"  DWAddAnyGlobal: No global memory added for Return Address=0x%08X\r\n", ReturnAddress));
    // Don't fail, just display the alert
    hr = S_OK;

Exit:
    return hr;
}


/*----------------------------------------------------------------------------
    MemoryAddGlobals

    Locate the process or module associated with the Return Address and
    add the global memory for that process or module.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddGlobals(CEDUMP_TYPE ceDumpType, DWORD dwReturnAddress, HPROCESS hFrameProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddGlobals: Enter\r\n"));

    switch (ceDumpType)
    {
    case ceDumpTypeComplete:
    case ceDumpTypeContext:
        // No globals added for these yet
        hRes = S_OK;
        break;
        
    case ceDumpTypeSystem:
        // Add global for any module or process found that matches Return Address
        hRes = DWAddAnyGlobal (dwReturnAddress, hFrameProcess, fWrite);
        break;
        
    default:
        // This should not happen
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
        KD_ASSERT(FALSE); 
        hRes = E_FAIL;
        goto Exit;
    }

Exit:
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddGlobals: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


/*----------------------------------------------------------------------------
    MemoryAddIPCode

    Add code surrounding the Instruction Pointer (IP). Limit the address 
    range to pages that are currently paged in.
----------------------------------------------------------------------------*/
static HRESULT MemoryAddIPCode(DWORD dwMemoryIPBefore, DWORD dwMemoryIPAfter, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    DWORD dwMemoryEnd;
    DWORD dwIP;
    DWORD dwIPCheck;
    PROCESS *VMProcess;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddIPCode: Enter\r\n"));

    if (g_pDmpThread == pCurThread)
    {
        dwIP = CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context);
        dwIP = (DWORD) dwIP;
    }
    else
    {
        dwIP = GetThreadIP (g_pDmpThread);
        dwIP = (DWORD) dwIP;
    }

    if (dwIP >= VM_KMODE_BASE)
    {
        VMProcess = g_pprcNK;
    }
    else
    {
        VMProcess = g_pDmpThread->pprcVM;
    }
    
    // Address range wanted
    dwMemorySize = dwMemoryIPBefore + dwMemoryIPAfter;

    hRes = DWordSub (dwIP, dwMemoryIPBefore, &dwMemoryStart);
    if (SUCCEEDED (hRes))
    {
        hRes = DWordAdd (dwMemoryStart, dwMemorySize, &dwMemoryEnd);
        if (SUCCEEDED (hRes))
        {
            // Normal case.  Everything worked out.
            dwMemoryEnd -= 1;
        }
        else
        {
            // Overflow case.  Bound on high end and shift the starting point
            // down.
            dwMemoryEnd = MAXDWORD;
            dwMemoryStart = dwMemoryEnd - dwMemorySize + 1;
        }
    }
    else
    {
        // underflow condition  bound lower end to 0 and shift the end point up.
        dwMemoryStart = 0;
        dwMemoryEnd = dwMemorySize - 1;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!MemoryAddIPCode: Start = %08X, End = %08X, Size = %d\r\n",
                                  dwMemoryStart, dwMemoryEnd, dwMemorySize));

    
    // Find lower limit for which range is valid
    dwIPCheck = dwIP;

    while (dwIPCheck >= dwMemoryStart)
    {
        BYTE bDummy;
        if(OsAxsReadMemory(&bDummy, VMProcess, (PVOID)dwIPCheck, 1) == 1)       
        {
            hRes = DWordSub (dwIPCheck, 1, &dwIPCheck);
            if (FAILED (hRes))
            {
                dwMemoryStart = 0;
                break;
            }
        }
        else
        {
            hRes = DWordAdd (dwIPCheck, 1, &dwMemoryStart);
            if (FAILED (hRes))
            {
                dwMemoryStart = MAXDWORD;
                break;
            }
        }
    }

    // Find upper limit for which range is valid
    dwIPCheck = dwIP;

    while (dwIPCheck <= dwMemoryEnd)
    {
        BYTE bDummy;
        if(OsAxsReadMemory(&bDummy, VMProcess, (PVOID)dwIPCheck, 1) == 1)       
        {
            hRes = DWordAdd (dwIPCheck, 1, &dwIPCheck);
            if (FAILED (hRes))
            {
                dwMemoryEnd = MAXDWORD;
                break;
            }
        }
        else
        {
            hRes = DWordSub (dwIPCheck, 1 ,&dwMemoryEnd);
            if (FAILED (hRes))
            {
                dwMemoryEnd = 0;
                break;
            }
        }
    }

    if (dwMemoryStart <= dwMemoryEnd)
    {
        dwMemorySize = dwMemoryEnd - dwMemoryStart + 1;
        
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!MemoryAddIPCode: Add memory surrounding Instruction Pointer -> IP=0x%08X\r\n", dwIP));
        
        hRes = MemoryBlocksAdd(VMProcess, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddIPCode: MemoryBlocksAdd failed adding memory surrounding Instruction Pointer, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddIPCode: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


static HRESULT __forceinline DWAlignFile (DWORD &FileOffset, BOOL fWrite)
{
    DWORD Alignment = FileOffset % CEDUMP_ALIGNMENT;
    if (Alignment > 0)
    {
        Alignment = CEDUMP_ALIGNMENT - Alignment;
        if (fWrite)
        {
            HRESULT hr = WatsonWriteData(FileOffset, &g_bBufferAlignment, Alignment, TRUE);
            if (FAILED (hr))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", Alignment, hr));
                return hr;
            }
        }
        FileOffset += Alignment;
    }
    return S_OK;
}


/*----------------------------------------------------------------------------
    WriteCeHeader

    Write the dump file header.
----------------------------------------------------------------------------*/
static HRESULT WriteCeHeader(DWORD dwNumberOfStreams, CEDUMP_TYPE ceDumpType, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeHeader: Enter\r\n"));

    g_ceDmpHeader.NumberOfStreams = dwNumberOfStreams;
    g_ceDmpHeader.StreamDirectoryRva = *pdwFileOffset + sizeof(MINIDUMP_HEADER);

    if (fWrite)
    {
        switch(ceDumpType)
        {
            case ceDumpTypeComplete: 
                g_ceDmpHeader.Signature = CEDUMP_SIGNATURE_COMPLETE; 
                break;
            case ceDumpTypeSystem:
                g_ceDmpHeader.Signature = CEDUMP_SIGNATURE_SYSTEM; 
                break;
            case ceDumpTypeContext:
                g_ceDmpHeader.Signature = CEDUMP_SIGNATURE_CONTEXT; 
                break;
            case ceDumpTypeUndefined:
            default:
                // This should not happen
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeHeader: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
                KD_ASSERT(FALSE); 
                hRes = E_FAIL;
                goto Exit;
        }
        g_ceDmpHeader.Version = CEDUMP_VERSION;

        hRes = WatsonWriteData(*pdwFileOffset, &g_ceDmpHeader, sizeof(MINIDUMP_HEADER), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeHeader: WatsonWriteData failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    *pdwFileOffset += sizeof(MINIDUMP_HEADER);

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeHeader: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeDirectory

    Write the dump file directory.
----------------------------------------------------------------------------*/
static HRESULT WriteCeDirectory(DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeDirectory: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeDirectory: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    if (fWrite)
    {
        hRes = WatsonWriteData(*pdwFileOffset, &g_ceDmpDirectory, (g_ceDmpHeader.NumberOfStreams * sizeof(MINIDUMP_DIRECTORY)), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeDirectory: WatsonWriteData failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    *pdwFileOffset += (g_ceDmpHeader.NumberOfStreams * sizeof(MINIDUMP_DIRECTORY));

    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeDirectory: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamElementList

    Write the Element List Stream to the dump file.
    This is used to write out the list of Modules, Process, or Threads.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamElementList(DWORD dwStreamType, CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    DWORD dwFieldInfoCount;
    DWORD dwElementCount;
    DWORD dwElementSize = 0;
    PCEDUMP_ELEMENT_LIST pceDmpElementList = NULL;
    PCEDUMP_FIELD_INFO pceDmpFieldInfo = NULL;
    FLEXI_REQUEST flexiRequest = {0};
    FLEXI_RANGE*  pflexiRange = NULL;
    DWORD dwRequestHdr = 0;
    DWORD dwRequestDesc = 0;
    DWORD dwRequestData = 0;
    DWORD dwRequestFilter = 0;
    DWORD dwFieldStringsSize = 0;
    DWORD dwFieldInfoTotalSize = 0;
    DWORD dwFlexiSizeRead = 0;
    PVOID pElementData = NULL;
    BOOL  fExceptionContext = FALSE;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamElementList: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamElementList: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamElementList: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    switch (ceDumpType)
    {
        case ceDumpTypeSystem:
            // Include data for all process, threads, and modules
            dwRequestFilter = 0;
            break;
            
        case ceDumpTypeComplete:
            // ** Fall through **
        case ceDumpTypeContext:
            if (ceStreamThreadContextList == dwStreamType)
            {
                // Only include context of dump thread
                dwRequestFilter = (DWORD) FLEXI_FILTER_THREAD_HANDLE;
                flexiRequest.dwHThread = (DWORD)g_pDmpThread->dwId;
            }
            else
            {
                // Only include owner process, threads for owner process, and modules for owner process
                dwRequestFilter = (DWORD)(FLEXI_FILTER_PROCESS_HANDLE | FLEXI_FILTER_INC_PROC);
                flexiRequest.dwHProc = (DWORD)g_pDmpThread->pprcOwner->dwId;
                // Also include the VM proc.
                if (g_pDmpThread->pprcVM)
                {
                    flexiRequest.dwHMod = (DWORD)g_pDmpThread->pprcVM->dwId;
                }
            }
            break;
            
        default:
            // This should not happen
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
            KD_ASSERT(FALSE); 
            hRes = E_FAIL;
            goto Exit;
    }

    switch (dwStreamType)
    {
        case ceStreamModuleList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Module List\r\n"));
            pceDmpElementList = &g_ceDmpModuleList;
            dwRequestHdr  = FLEXI_REQ_MODULE_HDR;
            dwRequestDesc = FLEXI_REQ_MODULE_DESC;
            dwRequestData = FLEXI_REQ_MODULE_DATA;
            flexiRequest.Mod.dwStart = 0;
            flexiRequest.Mod.dwEnd   = (DWORD)(-1);
            pflexiRange = &flexiRequest.Mod;
            // Always include nk.exe and dump thread's current vm process in modules list, even for context dumps
            dwRequestFilter |= (DWORD)(FLEXI_FILTER_INC_NK | FLEXI_FILTER_INC_PROC);
            if (g_pDmpThread->pprcVM)
            {
                flexiRequest.dwHMod = (DWORD)g_pDmpThread->pprcVM->dwId;
            }
            break;

        case ceStreamProcessList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Process List\r\n"));
            pceDmpElementList = &g_ceDmpProcessList;
            dwRequestHdr  = FLEXI_REQ_PROCESS_HDR;
            dwRequestDesc = FLEXI_REQ_PROCESS_DESC;
            dwRequestData = FLEXI_REQ_PROCESS_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = (DWORD)(-1);
            pflexiRange = &flexiRequest.Proc;
            // Always include nk.exe and dump thread's current vm process in process list, even for context dumps
            dwRequestFilter |= (DWORD)(FLEXI_FILTER_INC_NK | FLEXI_FILTER_INC_PROC);
            if (g_pDmpThread->pprcVM)
            {
                flexiRequest.dwHMod = (DWORD)g_pDmpThread->pprcVM->dwId;
            }
            break;

        case ceStreamThreadList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Thread List\r\n"));
            pceDmpElementList = &g_ceDmpThreadList;
            dwRequestHdr  = FLEXI_REQ_THREAD_HDR;
            dwRequestDesc = FLEXI_REQ_THREAD_DESC;
            dwRequestData = FLEXI_REQ_THREAD_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = (DWORD)(-1);
            flexiRequest.Thread.dwStart = 0;
            flexiRequest.Thread.dwEnd   = (DWORD)(-1);
            pflexiRange = &flexiRequest.Thread;
            break;

        case ceStreamThreadContextList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Thread Context List\r\n"));
            pceDmpElementList = &g_ceDmpThreadContextList;
            dwRequestHdr  = FLEXI_REQ_CONTEXT_HDR;
            dwRequestDesc = FLEXI_REQ_CONTEXT_DESC;
            dwRequestData = FLEXI_REQ_CONTEXT_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = (DWORD)(-1);
            flexiRequest.Thread.dwStart = 0;
            flexiRequest.Thread.dwEnd   = (DWORD)(-1);
            pflexiRange = &flexiRequest.Thread;
            break;

        case ceStreamProcessModuleMap:
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  WriteCeStreamElementList: Generating Process-Module Map\r\n"));
            pceDmpElementList         = &g_ceDmpProcessModuleList;
            dwRequestHdr              = FLEXI_REQ_MODPROC_HDR;
            dwRequestDesc             = FLEXI_REQ_MODPROC_DESC;
            dwRequestData             = FLEXI_REQ_MODPROC_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = (DWORD)(-1);
            flexiRequest.Mod.dwStart  = 0;
            flexiRequest.Mod.dwEnd    = (DWORD)(-1);
            pflexiRange = &flexiRequest.Mod;
            break;

        case ceStreamException:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Thread Context for Exception\r\n"));
            pceDmpElementList = &g_ceDmpExceptionContext;
            if (g_pDmpThread == pCurThread)
            {
                // Write exception context when current thread (even if CaptureDumpFileOnDevice called)
                dwRequestHdr  = GET_CTX_HDR;
                dwRequestDesc = GET_CTX_DESC;
                dwRequestData = GET_CTX;
                pflexiRange = &flexiRequest.Thread;
                fExceptionContext = TRUE;
            }
            else
            {
                KD_ASSERT(g_fCaptureDumpFileOnDeviceCalled);
                // Write context data for focus thread when CaptureDumpFileOnDevice called
                dwRequestHdr  = FLEXI_REQ_CONTEXT_HDR;
                dwRequestDesc = FLEXI_REQ_CONTEXT_DESC;
                dwRequestData = FLEXI_REQ_CONTEXT_DATA;
                flexiRequest.Proc.dwStart = 0;
                flexiRequest.Proc.dwEnd   = (DWORD)(-1);
                flexiRequest.Thread.dwStart = 0;
                flexiRequest.Thread.dwEnd   = (DWORD)(-1);
                pflexiRange = &flexiRequest.Thread;
                // Only include context of dump thread
                dwRequestFilter = (DWORD)FLEXI_FILTER_THREAD_HANDLE;
                flexiRequest.dwHThread = (DWORD)g_pDmpThread->dwId;
            }
            break;
            
        default:
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: Unknown stream type=0x%08X\r\n", dwStreamType));
            hRes = E_FAIL;
            goto Exit;
    }

    KD_ASSERT((NULL != pceDmpElementList) && (NULL != pflexiRange));
    if ((NULL == pceDmpElementList) || (NULL == pflexiRange))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: Pointer to element list or flexi range not initialized, dwStreamType=0x%08X\r\n", dwStreamType));
        hRes = E_FAIL;
        goto Exit;
    }

    // Get the number of Elements & field info from FPTMI
    flexiRequest.dwRequest = dwRequestHdr | dwRequestDesc | dwRequestFilter;
    dwFlexiSizeRead = sizeof(g_bBuffer);

    if (!fExceptionContext)
    {
        hRes = GetFPTMI(&flexiRequest, &dwFlexiSizeRead, g_bBuffer);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: GetFPTMI failed getting header info, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
            goto Exit;
        }
    }
    else
    {
        hRes = GetExceptionContext(flexiRequest.dwRequest, &dwFlexiSizeRead, g_bBuffer);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: GetExceptionContext failed getting header info, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
            goto Exit;
        }
    }

    // Transfer header info to element list struct
    memcpy(pceDmpElementList, g_bBuffer, sizeof(CEDUMP_ELEMENT_LIST));

    // Make sure header info is correct
    if ((pceDmpElementList->SizeOfHeader != sizeof(CEDUMP_ELEMENT_LIST)) ||
        (pceDmpElementList->SizeOfFieldInfo != sizeof(CEDUMP_FIELD_INFO)))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: Element list SizeOfHeader (%u vs %u) or SizeOfFieldInfo (%u vs %u) is incorrect\r\n", 
                                 pceDmpElementList->SizeOfHeader, 
                                 sizeof(CEDUMP_ELEMENT_LIST),
                                 pceDmpElementList->SizeOfFieldInfo,
                                 sizeof(CEDUMP_FIELD_INFO)));
        hRes = E_FAIL;
        goto Exit;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Number of FieldInfo=%u, Number of Elements=%u\r\n", 
                             pceDmpElementList->NumberOfFieldInfo, 
                             pceDmpElementList->NumberOfElements));

    // Init the directory info
    pceDmpDirectory->StreamType        = dwStreamType;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_ELEMENT_LIST) + pceDmpElementList->NumberOfFieldInfo * sizeof(CEDUMP_FIELD_INFO);
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    // Fixup the Field Info RVAs & calculate element size
    pceDmpFieldInfo = (PCEDUMP_FIELD_INFO)&g_bBuffer[sizeof(CEDUMP_ELEMENT_LIST)];
    
    for (dwFieldInfoCount = 0; dwFieldInfoCount < pceDmpElementList->NumberOfFieldInfo; ++dwFieldInfoCount)
    {
        // Check to make sure the Field info is within the buffer range
        if ((BYTE*)(pceDmpFieldInfo + 1) > ((BYTE*)g_bBuffer + sizeof(g_bBuffer)))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: FieldInfo pointer is beyond buffer, FieldInfoCount=%u, NumberOfFieldInfo=%u\r\n", 
                                     dwFieldInfoCount, pceDmpElementList->NumberOfFieldInfo));
            hRes = E_FAIL;
            goto Exit;
        }

        // Check to make sure the FieldLabel RVA is in the expected location
        if (pceDmpFieldInfo->FieldLabel != (dwFieldStringsSize + pceDmpDirectory->Location.DataSize))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: FieldLabel RVA (%u) does not match expected RVA (%u), FieldInfoCount=%u\r\n", 
                                     pceDmpFieldInfo->FieldLabel, (dwFieldStringsSize + pceDmpDirectory->Location.DataSize), dwFieldInfoCount));
            hRes = E_FAIL;
            goto Exit;
        }

        // Make sure the FieldLabel RVA is within the buffer
        if (pceDmpFieldInfo->FieldLabel >= sizeof(g_bBuffer))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: FieldLabel RVA (%u) points beyond end of buffer (%u)\r\n", 
                                     pceDmpFieldInfo->FieldLabel, sizeof(g_bBuffer)));
            hRes = E_FAIL;
            goto Exit;
        }
        dwFieldStringsSize += 4 + ((wcslen((WCHAR*)&g_bBuffer[pceDmpFieldInfo->FieldLabel+4]) + 1) * sizeof(WCHAR));
        pceDmpFieldInfo->FieldLabel += pceDmpDirectory->Location.Rva;
        
        // Check to make sure the FieldFormat RVA is in the expected location
        if (pceDmpFieldInfo->FieldFormat != (dwFieldStringsSize + pceDmpDirectory->Location.DataSize))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: FieldFormat RVA (%u) does not match expected RVA (%u), FieldInfoCount=%u\r\n", 
                                     pceDmpFieldInfo->FieldFormat, (dwFieldStringsSize + pceDmpDirectory->Location.DataSize), dwFieldInfoCount));
            hRes = E_FAIL;
            goto Exit;
        }

        // Make sure the FieldFormat RVA is within the buffer
        if (pceDmpFieldInfo->FieldFormat >= sizeof(g_bBuffer))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: FieldFormat RVA (%u) points beyond end of buffer (%u)\r\n", 
                                     pceDmpFieldInfo->FieldFormat, sizeof(g_bBuffer)));
            hRes = E_FAIL;
            goto Exit;
        }
        dwFieldStringsSize += 4 + ((wcslen((WCHAR*)&g_bBuffer[pceDmpFieldInfo->FieldFormat+4]) + 1) * sizeof(WCHAR));
        pceDmpFieldInfo->FieldFormat += pceDmpDirectory->Location.Rva;

        // Increment the element size with the size of the field
        dwElementSize += pceDmpFieldInfo->FieldSize;
        
        pceDmpFieldInfo += 1;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Size of each Element=%u\r\n", dwElementSize));

    KD_ASSERT(dwFlexiSizeRead == (pceDmpDirectory->Location.DataSize + dwFieldStringsSize));
    
    // Write out the Element list header info
    if (fWrite)
    {
        dwWriteOffset = 0;
        
        // Write out Element List Stream header
        pceDmpElementList->Elements = pceDmpDirectory->Location.Rva + pceDmpDirectory->Location.DataSize + dwFieldStringsSize;
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, pceDmpElementList, sizeof(CEDUMP_ELEMENT_LIST), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: WatsonWriteData failed writing Element List Stream header, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
            goto Exit;
        }
        dwWriteOffset += sizeof(CEDUMP_ELEMENT_LIST);

        // Write out Element List Field Info descriptors
        pceDmpFieldInfo = (PCEDUMP_FIELD_INFO)&g_bBuffer[sizeof(CEDUMP_ELEMENT_LIST)];
        dwFieldInfoTotalSize = (pceDmpElementList->NumberOfFieldInfo * sizeof(CEDUMP_FIELD_INFO)) + dwFieldStringsSize;

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, pceDmpFieldInfo, dwFieldInfoTotalSize, TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: WatsonWriteData failed writing Element Field Info, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
            goto Exit;
        }
        dwWriteOffset += dwFieldInfoTotalSize;

        KD_ASSERT(dwWriteOffset == (pceDmpDirectory->Location.DataSize + dwFieldStringsSize));
    }

    *pdwFileOffset += (pceDmpDirectory->Location.DataSize + dwFieldStringsSize);

    // Write out the list of Elements
    if (TRUE == fWrite)
    {
        // Make sure the element data will fit in the buffer
        if (dwElementSize > sizeof(g_bBuffer))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: Element size (%u) is larger than buffer (%u), dwStreamType=0x%08X\r\n", 
                                     dwElementSize, sizeof(g_bBuffer), dwStreamType));
            hRes = E_FAIL;
            goto Exit;
        }
        
        dwWriteOffset = 0;

        // Write out list of Elements
        for (dwElementCount = 0; dwElementCount < pceDmpElementList->NumberOfElements; ++dwElementCount)
        {
            // Get the number of Elements & field info from FPTMI
            flexiRequest.dwRequest = dwRequestHdr | dwRequestData | dwRequestFilter;
            dwFlexiSizeRead = sizeof(g_bBuffer);
            
            if (!fExceptionContext)
            {
                pflexiRange->dwStart = dwElementCount;
                pflexiRange->dwEnd   = dwElementCount;

                hRes = GetFPTMI(&flexiRequest, &dwFlexiSizeRead, g_bBuffer);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: GetFPTMI failed getting element data, hRes=0x%08X, dwStreamType=0x%08X, Element=%u\r\n", hRes, dwStreamType, dwElementCount));
                    goto Exit;
                }
            }
            else
            {
                hRes = GetExceptionContext(flexiRequest.dwRequest, &dwFlexiSizeRead, g_bBuffer);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: GetExceptionContext failed getting element data, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
                    goto Exit;
                }
            }

            // BUGBUG: We always hit this assert with no discernible affect
            //KD_ASSERT(dwFlexiSizeRead == dwElementSize + sizeof(CEDUMP_ELEMENT_LIST));

            pElementData = (PVOID)&g_bBuffer[sizeof(CEDUMP_ELEMENT_LIST)];
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, pElementData, dwElementSize, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: WatsonWriteData failed writing list of Elements, hRes=0x%08X, dwStreamType=0x%08X\r\n", hRes, dwStreamType));
                goto Exit;
            }
            dwWriteOffset += dwElementSize;
        }

        KD_ASSERT(dwWriteOffset == (pceDmpElementList->NumberOfElements * dwElementSize));
    }

    *pdwFileOffset += (pceDmpElementList->NumberOfElements * dwElementSize);

    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamElementList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamSystemInfo

    Write the System Information Stream to the dump file.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamSystemInfo(MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    SYSTEM_INFO sysi;
    RVA rvaData;
    DWORD dwWriteOffset;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamSystemInfo: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamSystemInfo: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamSystemInfo: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    if (!pTOC)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: FetchTOC returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pceDmpDirectory->StreamType        = ceStreamSystemInfo;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_SYSTEM_INFO);
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    // Location for first RVA Data
    rvaData = pceDmpDirectory->Location.Rva + pceDmpDirectory->Location.DataSize;

    // Setup OEM String RVA if valid
    if (g_dwOEMStringSize >= sizeof(WCHAR))
    {
        // Setup OEM String RVA 
        g_ceDmpSystemInfo.OEMStringRva = rvaData;
        rvaData += sizeof(g_dwOEMStringSize) + g_dwOEMStringSize;
    }
    else
    {
        g_ceDmpSystemInfo.OEMStringRva = 0;
    }

    // Setup PlatformType String RVA if valid
    if (g_dwPlatformTypeSize >= sizeof(WCHAR))
    {
        g_ceDmpSystemInfo.PlatformTypeRva = rvaData;
        rvaData += sizeof(g_dwPlatformTypeSize) + g_dwPlatformTypeSize;
    }
    else
    {
        g_ceDmpSystemInfo.PlatformTypeRva = 0;
    }

    // Setup PlatformVersion Data RVA if valid
    if (g_dwPlatformVersionSize)
    {
        g_ceDmpSystemInfo.Platforms = g_dwPlatformVersionSize / sizeof(PLATFORMVERSION);
        g_ceDmpSystemInfo.PlatformVersion = rvaData;
        rvaData += g_dwPlatformVersionSize;
    }
    else
    {
        g_ceDmpSystemInfo.Platforms = 0;
        g_ceDmpSystemInfo.PlatformVersion = 0;
    }
    
    if (fWrite)
    {
        dwWriteOffset = 0;
        GetSystemInfo(&sysi);
        
        g_ceDmpSystemInfo.BuildNumber           = 0;  // This will be added at dump file transfer time to avoid the risky call into GetVersionEx
        g_ceDmpSystemInfo.MajorVersion          = CE_MAJOR_VER;
        g_ceDmpSystemInfo.MinorVersion          = CE_MINOR_VER;
        g_ceDmpSystemInfo.SizeOfHeader          = sizeof(CEDUMP_SYSTEM_INFO);
        g_ceDmpSystemInfo.ProcessorArchitecture = sysi.wProcessorArchitecture;
        g_ceDmpSystemInfo.NumberOfProcessors    = sysi.dwNumberOfProcessors;
        g_ceDmpSystemInfo.ProcessorType         = sysi.dwProcessorType;
        g_ceDmpSystemInfo.ProcessorLevel        = sysi.wProcessorLevel;
        g_ceDmpSystemInfo.ProcessorRevision     = sysi.wProcessorRevision;
        g_ceDmpSystemInfo.ProcessorFamily       = TARGET_CODE_CPU;
        g_ceDmpSystemInfo.PlatformId            = VER_PLATFORM_WIN32_CE; 
        g_ceDmpSystemInfo.LCID                  = (UserKInfo[KINX_NLS_SYSLOC]);
        g_ceDmpSystemInfo.Machine               = pTOC->usCPUType;
        g_ceDmpSystemInfo.InstructionSet        = dwCEInstructionSet;
        g_ceDmpSystemInfo.PtrKData              = (DWORD)g_pKData;

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpSystemInfo, sizeof(CEDUMP_SYSTEM_INFO), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += pceDmpDirectory->Location.DataSize;

        if (g_ceDmpSystemInfo.OEMStringRva)
        {
            DWORD dwOEMStringSizeBStr = g_dwOEMStringSize - sizeof(WCHAR);
            
            // Write OEM string size without terminating NULL
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwOEMStringSizeBStr, sizeof(dwOEMStringSizeBStr), TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing OEM string size, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += sizeof(dwOEMStringSizeBStr);

            // Write OEM string
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, g_wzOEMString, g_dwOEMStringSize, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing OEM string, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += g_dwOEMStringSize;
        }


        if (g_ceDmpSystemInfo.PlatformTypeRva)
        {
            DWORD dwPlatformTypeSizeBStr = g_dwPlatformTypeSize - sizeof(WCHAR);

            // Write PlatformType string size without terminating NULL
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwPlatformTypeSizeBStr, sizeof(dwPlatformTypeSizeBStr), TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing PlatformType string size, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += sizeof(dwPlatformTypeSizeBStr);

            // Write PlatformType string
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, g_wzPlatformType, g_dwPlatformTypeSize, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing PlatformType string, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += g_dwPlatformTypeSize;
        }

        if (g_ceDmpSystemInfo.PlatformVersion)
        {
            // Write PlatformVersion data
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_platformVersion, g_dwPlatformVersionSize, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing PlatformVersion data, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += g_dwPlatformVersionSize;
        }
        
        KD_ASSERT(rvaData == ((*pdwFileOffset) + dwWriteOffset));
    }
    
    *pdwFileOffset = rvaData;

    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamSystemInfo: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamException

    Write the Exception Stream to the dump file.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamException(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    MINIDUMP_DIRECTORY ceDmpDirectory;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamException: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamException: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamException: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    pceDmpDirectory->StreamType        = ceStreamException;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_EXCEPTION_STREAM) +
                                         sizeof(CEDUMP_EXCEPTION);
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    if (fWrite)
    {
        BOOL fExceptionMatchesFocusThread;
        BOOL fFirstChanceException = (!DD_ExceptionState.secondChance) && ((!g_fCaptureDumpFileOnDeviceCalled) && (!g_fReportFaultCalled));
        dwWriteOffset = 0;

        memset (&g_ceDmpExceptionStream, 0, sizeof (g_ceDmpExceptionStream));
        
        // Write out Exception Stream header
        g_ceDmpExceptionStream.SizeOfHeader = sizeof (g_ceDmpExceptionStream);
        g_ceDmpExceptionStream.SizeOfException = sizeof(CEDUMP_EXCEPTION);
        g_ceDmpExceptionStream.SizeOfThreadContext = sizeof(CEDUMP_ELEMENT_LIST);
        g_ceDmpExceptionStream.Flags  = (fFirstChanceException            ? CEDUMP_EXCEPTION_FIRST_CHANCE : 0);          
        g_ceDmpExceptionStream.Flags |= (g_fCaptureDumpFileOnDeviceCalled ? CEDUMP_EXCEPTION_CAPTURE_DUMP_FILE_API : 0); 
        g_ceDmpExceptionStream.Flags |= (g_fReportFaultCalled             ? CEDUMP_EXCEPTION_REPORT_FAULT_API : 0);     
        g_ceDmpExceptionStream.Flags |= (g_fStackBufferOverrun            ? CEDUMP_EXCEPTION_STACK_BUFFER_OVERRUN : 0);


        // Capture the Exception Record first.
        // The information for all of the Exception entries can be derived from g_pDmpThread for exception capture
        // as well as user CaptureDumpFileOnDevice().
        //
        // Using thread structure because Kernel API's SwitchVM() and SwitchActiveProcess() aren't completely 
        // reversible operations. For example, for a kernel thread without a usermode VM, a SwitchVM call will result
        // in pVMProc becoming non-null, even after a second call to SwitchVM to restore the VM.  Both SwitchVM and
        // SwitchActiveProcess will properly restore null pointers to the current thread, so we're better off trusting
        // the thread more than KData.
        if (!g_fCaptureDumpFileOnDeviceCalled)
        {
            KD_ASSERT(pCurThread == g_pDmpThread);
        }

        if (g_pDmpThread)
        {
            if (g_pDmpThread->pprcVM)
            {
                g_ceDmpExceptionStream.VMProcessId              = static_cast<ULONG32> (g_pDmpThread->pprcVM->dwId);
            }
            if (g_pDmpThread->pprcActv)
            {
                g_ceDmpExceptionStream.ActvProcessId            = static_cast<ULONG32> (g_pDmpThread->pprcActv->dwId); 
            }
            g_ceDmpExceptionStream.ThreadId                     = static_cast<ULONG32> (g_pDmpThread->dwId);
            if (g_pDmpThread->pprcOwner)
            {
                g_ceDmpExceptionStream.OwnerProcessId           = static_cast<ULONG32> (g_pDmpThread->pprcOwner->dwId);
            }
        }

        // If CaptureDumpFileOnDevice() is called, capture details of the current thread.
        if (g_fCaptureDumpFileOnDeviceCalled)
        {
            if (pCurThread)
            {
                g_ceDmpExceptionStream.CaptureAPIThreadId           = static_cast<ULONG32> (pCurThread->dwId);
                if (pCurThread->pprcVM)
                {
                    g_ceDmpExceptionStream.CaptureAPIVMProcessId    = static_cast<ULONG32> (pCurThread->pprcVM->dwId);
                }
                if (pCurThread->pprcActv)
                {
                    g_ceDmpExceptionStream.CaptureAPIActvProcessId  = static_cast<ULONG32> (pCurThread->pprcActv->dwId);
                }
                if (pCurThread->pprcOwner)
                {
                    g_ceDmpExceptionStream.CaptureAPIOwnerProcessId = static_cast<ULONG32> (pCurThread->pprcOwner->dwId); 
                }
            }
            else
            {
                DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamException: pCurThread == NULL\r\n"));
            }
        }


        fExceptionMatchesFocusThread = (g_ceDmpExceptionStream.ThreadId == g_pDmpThread->dwId);
        g_ceDmpExceptionStream.Flags |= (fExceptionMatchesFocusThread ? CEDUMP_EXCEPTION_MATCHES_FOCUS_THREAD : 0);  

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpExceptionStream, sizeof(CEDUMP_EXCEPTION_STREAM), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamException: WatsonWriteData failed writing Exception Stream header, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(CEDUMP_EXCEPTION_STREAM);

        // Write out Exception record
        KD_ASSERT(sizeof(EXCEPTION_RECORD) == sizeof(CEDUMP_EXCEPTION));
        if (sizeof(EXCEPTION_RECORD) != sizeof(CEDUMP_EXCEPTION))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamException: Exception record size does not much dump record size\r\n"));
            hRes = E_FAIL;
            goto Exit;
        }

        if (!g_fCaptureDumpFileOnDeviceCalled)
        {
            // Copy over exception record for exception dumps
            memcpy(&g_ceDmpException, DD_ExceptionState.exception, sizeof(CEDUMP_EXCEPTION));
        }
        else
        {
            // Fake a breakpoint exception when CaptureDumpFileOnDevice called
            memset(&g_ceDmpException, 0, sizeof(CEDUMP_EXCEPTION));
            g_ceDmpException.ExceptionCode = (DWORD)STATUS_BREAKPOINT;
            if (g_pDmpThread == pCurThread)
            {
                // For the current thread we still use the exception address
                g_ceDmpException.ExceptionAddress = (ULONG32)DD_ExceptionState.exception->ExceptionAddress;
            }
            else
            {
                g_ceDmpException.ExceptionAddress = GetThreadIP(g_pDmpThread);
            }
        }

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpException, sizeof(CEDUMP_EXCEPTION), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamException: WatsonWriteData failed writing Exception Record, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(CEDUMP_EXCEPTION);

        KD_ASSERT(dwWriteOffset == pceDmpDirectory->Location.DataSize);
    }

    *pdwFileOffset += pceDmpDirectory->Location.DataSize;

    // We need to set this before calling WriteCeStreamElementList to avoid an error
    // with the offset check
    ceDmpDirectory.Location.Rva = *pdwFileOffset;
    
    // Write out Context of faulting thread
    hRes = WriteCeStreamElementList(ceStreamException, ceDumpType, &ceDmpDirectory, pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamException: WriteCeStreamElementList failed writing Thread Context for exception, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Header Data size for the Context list
    pceDmpDirectory->Location.DataSize += ceDmpDirectory.Location.DataSize;
    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamException: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamThreadCallStackList

    Write the list of Thread Call Stacks.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamThreadCallStackList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset = 0;
    DWORD dwFileOffsetStackFrames = 0;
    DWORD dwNumberOfThreads = 0;
    DWORD dwThreadCount = 0;
    DWORD dwNumberOfFrames = 0;
    DWORD dwNumberOfProcesses = 0;
    DWORD dwFrameCount = 0;
    CEDUMP_THREAD_CALL_STACK ceDumpThreadCallStack = {0};
    CEDUMP_THREAD_CALL_STACK_FRAME ceDumpThreadCallStackFrame = {0};
    CallSnapshot3 csexFrames[STACK_FRAMES_BUFFERED] = {0};

    PPROCESS pFirstProcess, pProcess;
    PTHREAD pThread = NULL;
    DWORD dwProcessNumber = 0;
    DWORD dwMaxStackFrames = 0;
    BOOL  fCurrentThreadOnly = FALSE;

    DWORD const dwGetCallStackFlags = STACKSNAP_NEW_VM | STACKSNAP_RETURN_FRAMES_ON_ERROR;
    DWORD dwFramesReturned = 0;
    DWORD dwParams = 0;

    // Used to find frame count mismatch between writting headers and frames
    memset(g_bBuffer, 0, sizeof(g_bBuffer));

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamThreadCallStackList: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    switch (ceDumpType)
    {
    case ceDumpTypeComplete:
    case ceDumpTypeContext:
        // We only include the current thread call stack for Complete & Context dumps
        fCurrentThreadOnly = TRUE;
        dwNumberOfProcesses = 1;
        dwMaxStackFrames = MAX_STACK_FRAMES_CONTEXT;
        pFirstProcess = g_pDmpProc;
        dwNumberOfThreads = 1;
        break;
        
    case ceDumpTypeSystem:
        {
            // Include all call stacks from all threads in all processes
            dwMaxStackFrames = MAX_STACK_FRAMES_SYSTEM;
            PROCESS *pProcess = g_pprcNK;
            do
            {
                ++dwNumberOfProcesses;

                if (IsProcessAlive (pProcess))
                {
                    THREAD *pthd = reinterpret_cast<THREAD*> (pProcess->thrdList.pFwd);
                    while (pthd && pthd != reinterpret_cast<THREAD*> (&pProcess->thrdList))
                    {
                        ++dwNumberOfThreads;
                        pthd = reinterpret_cast<THREAD*> (pthd->thLink.pFwd);
                    }
                }
                pProcess = reinterpret_cast<PROCESS*> (pProcess->prclist.pFwd);
            } while (pProcess != g_pprcNK);

            // Reset the pProcess to g_pprcNK;
            pFirstProcess = g_pprcNK;
            break;
        }
    
    default:
        // This should not happen
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
        KD_ASSERT(FALSE); 
        hRes = E_FAIL;
        goto Exit;
        break;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: Generating Thread Call Stack List\r\n"));
    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: Number of processes=%u, Number of Threads=%u, Max Frames=%u\r\n",
                                 dwNumberOfProcesses, dwNumberOfThreads, dwMaxStackFrames));

    // We should only have WORDs worth of frames
    KD_ASSERT((dwMaxStackFrames >> 16) == 0);
    dwMaxStackFrames = (WORD)dwMaxStackFrames;

    pceDmpDirectory->StreamType        = ceStreamThreadCallStackList;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_THREAD_CALL_STACK_LIST) +
                                         (sizeof(CEDUMP_THREAD_CALL_STACK) * dwNumberOfThreads); 
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    // Offset to where stack frames are written
    dwFileOffsetStackFrames = (*pdwFileOffset) + pceDmpDirectory->Location.DataSize;

    // Write out Thread Call Stack List header info
    if (fWrite)
    {
        dwWriteOffset = 0;
        
        // Write out the Thread Context List Stream header
        g_ceDmpThreadCallStackList.SizeOfHeader = sizeof(CEDUMP_THREAD_CALL_STACK_LIST);
        g_ceDmpThreadCallStackList.SizeOfEntry = sizeof(CEDUMP_THREAD_CALL_STACK);
        g_ceDmpThreadCallStackList.NumberOfEntries = dwNumberOfThreads;

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpThreadCallStackList, sizeof(CEDUMP_THREAD_CALL_STACK_LIST), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing Thread Call Stack List Stream header, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(CEDUMP_THREAD_CALL_STACK_LIST);
    }

    // Write out List of Thread Call Stack headers by iterating through all processes and threads
    pProcess = pFirstProcess;
    dwProcessNumber = 0;
    DWORD nCallStackWalk = 0;
    while (dwProcessNumber < dwNumberOfProcesses && pProcess)
    {
        if (fCurrentThreadOnly)
        {
            // We are only doing the current Thread's call stack
            KD_ASSERT(1 == dwNumberOfThreads);
            KD_ASSERT(1 == dwNumberOfProcesses);
            pThread = g_pDmpThread; // Point to the Dump thread
        }
        else
        {
            pThread = reinterpret_cast<PTHREAD> (pProcess->thrdList.pFwd);
        }

        while (pThread && pThread != reinterpret_cast<PTHREAD> (&pProcess->thrdList))
        {
            // Valid thread found, increment the thread count
            ++ dwThreadCount;

            dwNumberOfFrames = 0;
            dwFramesReturned = STACK_FRAMES_BUFFERED;
            
            // Repeatedly collect frames until either max reached or no more frames available
            while ((dwNumberOfFrames < dwMaxStackFrames) && (STACK_FRAMES_BUFFERED == dwFramesReturned))
            {
                nCallStackWalk++;
                PING_HOST_IF(!fCurrentThreadOnly && !fWrite &&
                    (nCallStackWalk%PING_INTERVAL) != 0);
                dwFramesReturned = GetCallStack(pThread, STACK_FRAMES_BUFFERED, csexFrames, dwGetCallStackFlags, dwNumberOfFrames);
                dwNumberOfFrames += dwFramesReturned;
            }

            // Limit number of frames to the max allowed
            dwNumberOfFrames = min (dwNumberOfFrames, dwMaxStackFrames);

            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: Got %d frames for thread 0x%08x\r\n",
                                           dwNumberOfFrames, pThread));
            if (fWrite)
            {
                ceDumpThreadCallStack.ProcessId      = (ULONG32)pThread->pprcOwner->dwId;
                ceDumpThreadCallStack.ThreadId       = (ULONG32)pThread->dwId;
                ceDumpThreadCallStack.SizeOfFrame    = sizeof(CEDUMP_THREAD_CALL_STACK_FRAME);
                ceDumpThreadCallStack.NumberOfFrames = (WORD)dwNumberOfFrames;
                ceDumpThreadCallStack.StackFrames    = dwFileOffsetStackFrames;

                hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpThreadCallStack, sizeof(ceDumpThreadCallStack), TRUE);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing Thread Call Stack header, hRes=0x%08X\r\n", hRes));
                    goto Exit;
                }
                dwWriteOffset += sizeof(ceDumpThreadCallStack);
            }

            SetFrameCount (dwThreadCount, dwNumberOfFrames);

            dwFileOffsetStackFrames += (sizeof(CEDUMP_THREAD_CALL_STACK_FRAME) * dwNumberOfFrames);

            // Check if we are only doing the current thread
            if (fCurrentThreadOnly)
            {
                // Only doing current thread, so break out of while loop
                pThread = NULL; 
            }
            else
            {
                // Doing all threads, so point to next one
                pThread = reinterpret_cast<PTHREAD> (pThread->thLink.pFwd);

            }
        }

        ++dwProcessNumber;
        pProcess = reinterpret_cast<PPROCESS> (pProcess->prclist.pFwd);
    }

    KD_ASSERT(dwThreadCount == dwNumberOfThreads);
    KD_ASSERT(fWrite ? (dwWriteOffset == pceDmpDirectory->Location.DataSize) : TRUE);

    *pdwFileOffset += pceDmpDirectory->Location.DataSize;

    dwThreadCount = 0;
    dwWriteOffset = 0;

    // Write out all Thread Call Stack Frames by iterating through all processes and threads
    pProcess = pFirstProcess;
    dwProcessNumber = 0;
    while (dwProcessNumber < dwNumberOfProcesses && pProcess)
    {
        if (fCurrentThreadOnly)
        {
            // We are only doing the current Thread's call stack
            KD_ASSERT(1 == dwNumberOfThreads);
            KD_ASSERT(1 == dwNumberOfProcesses);
            pThread = g_pDmpThread; // Point to the Dump thread
        }
        else
        {
            pThread = reinterpret_cast<PTHREAD> (pProcess->thrdList.pFwd);
        }

        while (pThread && pThread != reinterpret_cast<PTHREAD> (&pProcess->thrdList))
        {
            // Valid thread found, increment the thread count
            ++ dwThreadCount;


            dwNumberOfFrames = 0;
            dwFramesReturned = STACK_FRAMES_BUFFERED;
            
            // Repeatedly write frames until either max reached or no more frames available
            while ((dwNumberOfFrames < dwMaxStackFrames) && (STACK_FRAMES_BUFFERED == dwFramesReturned))
            {
                nCallStackWalk++;
                PING_HOST_IF(!fCurrentThreadOnly && !fWrite &&
                    (nCallStackWalk%PING_INTERVAL) != 0);
                dwFramesReturned = GetCallStack(pThread, STACK_FRAMES_BUFFERED, csexFrames, dwGetCallStackFlags, dwNumberOfFrames);
                DWORD dwGetCallStackError = GetLastError();

                for (dwFrameCount = 0;  (dwFrameCount < dwFramesReturned) && (dwNumberOfFrames < dwMaxStackFrames); ++ dwFrameCount)
                {
                    if (fWrite)
                    {
                        memset(&ceDumpThreadCallStackFrame, 0, sizeof(ceDumpThreadCallStackFrame));
                        ceDumpThreadCallStackFrame.ReturnAddr    = csexFrames[dwFrameCount].dwReturnAddr;
                        ceDumpThreadCallStackFrame.FramePtr      = csexFrames[dwFrameCount].dwFramePtr;
                        ceDumpThreadCallStackFrame.VMProcessId   = csexFrames[dwFrameCount].dwVMProc;
                        ceDumpThreadCallStackFrame.ActvProcessId = csexFrames[dwFrameCount].dwActvProc;
                        ceDumpThreadCallStackFrame.Flags         = (dwGetCallStackError ? CEDUMP_CALL_STACK_ERROR_DETECTED : 0);

                        for (dwParams = 0; dwParams < 4; ++dwParams)
                        {
                            ceDumpThreadCallStackFrame.Params[dwParams] = csexFrames[dwFrameCount].dwParams[dwParams];
                        }

                        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpThreadCallStackFrame, sizeof(ceDumpThreadCallStackFrame),
                           TRUE);
                        if (FAILED(hRes))
                        {
                            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing Thread Call Stack Frame data, hRes=0x%08X\r\n", hRes));
                            goto Exit;
                        }
                    }

                    // Add global memory for processes & modules in the call stack of the Dump thread
                    if ((pThread == g_pDmpThread) && (FALSE == fWrite))
                    {
                        // Add global memory of process / module associated with Return Address (Depends on CeDumpType)
                        hRes = MemoryAddGlobals(ceDumpType, csexFrames[dwFrameCount].dwReturnAddr, (HPROCESS)csexFrames[dwFrameCount].dwVMProc, fWrite);
                        if (FAILED(hRes))
                        {
                            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: MemoryAddGlobals failed adding global memory for ReturnAddr=0x%08X, hRes=0x%08X\r\n", hRes));
                            goto Exit;
                        }
                    }

                    ++ dwNumberOfFrames;
                    dwWriteOffset += sizeof (ceDumpThreadCallStackFrame);
                }
            }

            if (!CheckFrameCount (dwThreadCount, dwNumberOfFrames))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: Frame Count Check failure, Thread=%08X, ThreadID=0x%08X, ExpectedFrames=%u, ActualFrames=%u\r\n", 
                                           pThread, pThread->dwId, GetFrameCount (dwThreadCount), dwNumberOfFrames));
                hRes = E_FAIL;
                goto Exit;
            }

            // Check if we are only doing the current thread
            if (fCurrentThreadOnly)
            {
                // Only doing current thread, so break out of while loop
                pThread = NULL; 
            }
            else
            {
                // Doing all threads, so point to next one
                pThread = reinterpret_cast<PTHREAD> (pThread->thLink.pFwd);
            }
        }

        ++dwProcessNumber;
        pProcess = reinterpret_cast<PPROCESS> (pProcess->prclist.pFwd);
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  WriteCeStreamThreadCallStackList: Number of Threads = %d, Expected = %d\r\n", dwThreadCount, dwNumberOfThreads));
    KD_ASSERT(dwThreadCount == dwNumberOfThreads);
    KD_ASSERT(dwFileOffsetStackFrames == (*pdwFileOffset) + dwWriteOffset);

    *pdwFileOffset += dwWriteOffset;

    // Make sure we are aligned on exit
    hRes = DWAlignFile(*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamThreadCallStackList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamMemoryList

    Write the list of Memory dumps.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamMemoryList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fVirtual, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset = 0;
    DWORD dwFileOffsetMemoryDumps = 0;
    DWORD dwMemoryBlockCount;
    DWORD dwMemoryBlock;
    DWORD dwMemorySize;
    DWORD dwMemorySizeTotal;
    CEDUMP_MEMORY_DESCRIPTOR ceDumpMemoryDescriptor;
    DWORD dwMemoryBlocksMax;
    MEMORY_BLOCK *pmemoryBlocks;
    DWORD *pdwMemoryBlocksTotal;
    DWORD *pdwMemorySizeTotal;
    DWORD dwStreamType;
    PCEDUMP_MEMORY_LIST pceDmpMemoryList;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamMemoryList: Enter, fVirtual=%u\r\n", fVirtual));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryList: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryList: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    if (TRUE == fVirtual)
    {
        // Virutal Memory
        dwMemoryBlocksMax = MAX_MEMORY_VIRTUAL_BLOCKS;
        pmemoryBlocks = g_memoryVirtualBlocks;
        pdwMemorySizeTotal = &g_dwMemoryVirtualSizeTotal;
        pdwMemoryBlocksTotal = &g_dwMemoryVirtualBlocksTotal;
        dwStreamType = ceStreamMemoryVirtualList;
        pceDmpMemoryList = &g_ceDmpMemoryVirtualList;
    }
    else
    {
        // Physical Memory
        dwMemoryBlocksMax = MAX_MEMORY_PHYSICAL_BLOCKS;
        pmemoryBlocks = g_memoryPhysicalBlocks;
        pdwMemorySizeTotal = &g_dwMemoryPhysicalSizeTotal;
        pdwMemoryBlocksTotal = &g_dwMemoryPhysicalBlocksTotal;
        dwStreamType = ceStreamMemoryPhysicalList;
        pceDmpMemoryList = &g_ceDmpMemoryPhysicalList;
    }

    dwMemoryBlockCount = 0;
    dwMemorySizeTotal = 0;
    
    for (dwMemoryBlock = 0; dwMemoryBlock < dwMemoryBlocksMax; ++dwMemoryBlock)
    {
        if (pmemoryBlocks[dwMemoryBlock].dwMemorySize != EMPTY_BLOCK)
        {
            ++ dwMemoryBlockCount;
            dwMemorySizeTotal += pmemoryBlocks[dwMemoryBlock].dwMemorySize;
        }
    }

    KD_ASSERT(dwMemoryBlockCount == (*pdwMemoryBlocksTotal));
    KD_ASSERT(dwMemorySizeTotal == (*pdwMemorySizeTotal));
    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryList: Writing %u memory blocks for a total of %u bytes\r\n", dwMemoryBlockCount, dwMemorySizeTotal));

    pceDmpDirectory->StreamType        = dwStreamType;
    pceDmpDirectory->Location.DataSize = sizeof (CEDUMP_MEMORY_LIST) +
                                         (sizeof (ceDumpMemoryDescriptor) * (*pdwMemoryBlocksTotal)); 
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    // Write out Memory List header info
    if (fWrite)
    {
        dwWriteOffset = 0;
        dwFileOffsetMemoryDumps = (*pdwFileOffset) + pceDmpDirectory->Location.DataSize;
        
        // Write out the Memory List Stream header
        pceDmpMemoryList->SizeOfHeader = sizeof (*pceDmpMemoryList);
        pceDmpMemoryList->SizeOfEntry  = sizeof (ceDumpMemoryDescriptor);
        pceDmpMemoryList->NumberOfEntries = (*pdwMemoryBlocksTotal);

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, pceDmpMemoryList, sizeof(CEDUMP_MEMORY_LIST), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryList: WatsonWriteData failed writing Memory List Stream header, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(CEDUMP_MEMORY_LIST);
    }

    // Write out Memory Descriptors
    dwMemoryBlockCount = 0;

    for (dwMemoryBlock = 0; (dwMemoryBlock < dwMemoryBlocksMax) && (dwMemoryBlockCount < (*pdwMemoryBlocksTotal)); ++dwMemoryBlock)
    {
        dwMemorySize = pmemoryBlocks[dwMemoryBlock].dwMemorySize;
        
        if (dwMemorySize != EMPTY_BLOCK)
        {
            if (fWrite)
            {
                ceDumpMemoryDescriptor.StartOfMemoryRange = (DWORD64)pmemoryBlocks[dwMemoryBlock].dwMemoryStart;
                ceDumpMemoryDescriptor.Memory.DataSize = dwMemorySize;
                ceDumpMemoryDescriptor.Memory.Rva = dwFileOffsetMemoryDumps;
                ceDumpMemoryDescriptor.VMProcessID = pmemoryBlocks[dwMemoryBlock].VMProcess?
                                                        pmemoryBlocks[dwMemoryBlock].VMProcess->dwId : 0;

                hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpMemoryDescriptor, sizeof (ceDumpMemoryDescriptor), TRUE);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryList: WatsonWriteData failed writing memory descriptor, Index=%u, Count=%u, hRes=0x%08X\r\n", 
                                              dwMemoryBlock, dwMemoryBlockCount, hRes));
                    goto Exit;
                }
                dwWriteOffset += sizeof (ceDumpMemoryDescriptor);
            }
            dwFileOffsetMemoryDumps += dwMemorySize;
            ++ dwMemoryBlockCount;
        }
    }

    KD_ASSERT(fWrite ? (dwWriteOffset == pceDmpDirectory->Location.DataSize) : TRUE);

    *pdwFileOffset += pceDmpDirectory->Location.DataSize;

    // Write out Memory Blocks 
    dwWriteOffset = 0;
    dwMemoryBlockCount = 0;

    for (dwMemoryBlock = 0; (dwMemoryBlock < dwMemoryBlocksMax) && (dwMemoryBlockCount < (*pdwMemoryBlocksTotal)); ++dwMemoryBlock)
    {
        dwMemorySize = pmemoryBlocks[dwMemoryBlock].dwMemorySize;
        if (dwMemorySize != EMPTY_BLOCK)
        {
            if (fWrite)
            {
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryList: Writing memory block %u, Start=0x%08X, Size=0x%08X\r\n", 
                                             dwMemoryBlock, pmemoryBlocks[dwMemoryBlock].dwMemoryStart, dwMemorySize));

                if (fVirtual)
                {
                    hRes = WatsonWriteData ((*pdwFileOffset) + dwWriteOffset,
                                            pmemoryBlocks[dwMemoryBlock].VMProcess,
                                            (PVOID)pmemoryBlocks[dwMemoryBlock].dwMemoryStart,
                                            dwMemorySize,
                                            TRUE);
                }
                else
                {
                    hRes = WatsonWritePhysicalBufferData((*pdwFileOffset) + dwWriteOffset, (PVOID)pmemoryBlocks[dwMemoryBlock].dwMemoryStart, dwMemorySize, TRUE);
                }
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryList: WatsonWriteData failed writing memory block, Index=%u, Count=%u, hRes=0x%08X\r\n", 
                                              dwMemoryBlock, dwMemoryBlockCount, hRes));
                    goto Exit;
                }
            }
            dwWriteOffset += dwMemorySize;
            ++ dwMemoryBlockCount;
        }
    }

    KD_ASSERT(fWrite ? (dwFileOffsetMemoryDumps == (*pdwFileOffset) + dwWriteOffset) : TRUE);

    *pdwFileOffset += dwWriteOffset;
    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamMemoryList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamMemoryVirtualList

    Write the list of Virtual Memory dumps.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamMemoryVirtualList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    BOOL fCurrentProcessGlobals = FALSE;
    BOOL fCurrentProcessIPMemory = FALSE;
    BOOL fCurrentThreadStackMemory = FALSE;
    BOOL fCurrentProcessHeap = FALSE;
    BOOL fAllVirtualMemory = FALSE;
    BOOL fKData = FALSE;
    DWORD dwStackBase, dwStackSize, dwStackBound, dwStackTop;
    DWORD dwMemoryIPBefore;
    DWORD dwMemoryIPAfter;
    DWORD dwFileOffsetEstimate;
    DWORD dwFileSpaceLeft; 
    DWORD dwMaxFileSize = (DWORD)(-1);

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamMemoryVirtualList: Enter\r\n"));

    switch (ceDumpType)
    {
        case ceDumpTypeSystem:
            // Include heap data of current process
            fCurrentProcessHeap = g_fDumpHeapData;
            
            // Include global variables of current process 
            fCurrentProcessGlobals = TRUE;

            // Amount of memory to save before and after Instruction Pointer
            fCurrentProcessIPMemory = TRUE;
            dwMemoryIPBefore = MEMORY_IP_BEFORE_SYSTEM;
            dwMemoryIPAfter = MEMORY_IP_AFTER_SYSTEM;

            // Include KData section
            fKData = TRUE;

            // Include stack memory of current thread (faulting thread)
            // This will also include the TLS data which is on the stack
            fCurrentThreadStackMemory = TRUE;

            break;
            
        case ceDumpTypeComplete:
            // The complete dump includes all Virtual memory
            fAllVirtualMemory = TRUE;

            // ** Fall through **

        case ceDumpTypeContext:
            // Amount of memory to save before and after Instruction Pointer
            fCurrentProcessIPMemory = TRUE;
            dwMemoryIPBefore = MEMORY_IP_BEFORE_CONTEXT;
            dwMemoryIPAfter = MEMORY_IP_AFTER_CONTEXT;

            // Include KData section
            fKData = TRUE;

            // Include stack memory of current thread (faulting thread)
            // This will also include the TLS data which is on the stack
            fCurrentThreadStackMemory = TRUE;

            // Limit max file size for writing stack dump of crashing thread
            dwMaxFileSize = WATSON_CONTEXT_DUMPFILE_SIZE;
            
            break;
            
        default:
            // This should not happen
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
            KD_ASSERT(FALSE); 
            hRes = E_FAIL;
            goto Exit;
    }

    if (!fWrite)
    {
        if (fCurrentProcessHeap)
        {
            hRes = MemoryAddProcessHeap(g_pDmpProc, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryAddProcessHeap failed adding Process Heap memory, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        if (fCurrentProcessGlobals)
        {
            hRes = MemoryAddProcessGlobals(g_pDmpProc, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryAddProcessGlobals failed adding Process Global memory, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        if (fCurrentProcessIPMemory)
        {
            hRes = MemoryAddIPCode(dwMemoryIPBefore, dwMemoryIPAfter, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryAddIPCode failed adding memory surrounding Instruction Pointer, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        if (fKData)
        {
            dwMemoryStart = (DWORD)g_pKData;
            dwMemorySize  = sizeof(KDataStruct);
            
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Adding KData memory block\r\n"));
            
            hRes = MemoryBlocksAdd (g_pprcNK, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryBlocksAdd failed adding KData memory block, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        // **********************************************************************
        // *** The stack memory must be the last virtual memory added, since  ***
        // *** it may be sized down to fit in the Context dump size limit.    ***
        // *** The only exception is fAllVirtualMemory, since it is only      ***
        // *** for complete dumps.                                            ***
        // **********************************************************************

        if (fCurrentThreadStackMemory)
        {
            
            // Calculate offset to include Memory List header and Memory descriptors 
            dwFileOffsetEstimate = (*pdwFileOffset) + 
                                   (sizeof(CEDUMP_MEMORY_LIST) * 2) + // Virtual + Physical list
                                   (sizeof(CEDUMP_MEMORY_DESCRIPTOR) * (g_dwMemoryVirtualBlocksTotal+1)); // +1 for non-secure stack memory descriptor
                                   
            // Calculate offset to include total memory added so far                       
            dwFileOffsetEstimate += g_dwMemoryVirtualSizeTotal;
            
            // Calculate file space left for adding non-secure stack memory 
            dwFileSpaceLeft = dwMaxFileSize - dwFileOffsetEstimate;

            // Memory for non-secure stack (may be a fiber)
            if (g_pDmpThread->pprcVM != 0)
            {
                // If the thread does not have a VM, then it is a NK.EXE/kernel only thread and will not
                // have a non-secure stack / tlsNonSecure.
             
                if (OsAxsReadMemory(&dwStackBase, g_pDmpThread->pprcVM,
                                    &g_pDmpThread->tlsNonSecure[PRETLS_STACKBASE], sizeof(dwStackBase)) == sizeof(dwStackBase) &&
                    OsAxsReadMemory(&dwStackBound, g_pDmpThread->pprcVM,
                                    &g_pDmpThread->tlsNonSecure[PRETLS_STACKBOUND], sizeof(dwStackBound)) == sizeof(dwStackBound))
                {
                    BOOL WriteNonSecureStack = TRUE;

                    if (dwStackBase == g_pDmpThread->dwOrigBase)
                    {
                        // Normal thread
                        dwStackSize = g_pDmpThread->dwOrigStkSize;
                    }
                    else
                    {
                        // Fiber thread
                        if (OsAxsReadMemory(&dwStackSize, g_pDmpThread->pprcVM,
                                &g_pDmpThread->tlsNonSecure[PRETLS_STACKSIZE], sizeof(dwStackSize)) != sizeof(dwStackSize))
                        {
                            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  Thread %08X failed to read tlsNonSecure(%08X) in VM(%08X) for fiber\r\n", g_pDmpThread,
                                        g_pDmpThread->tlsNonSecure, g_pDmpThread->pprcVM));
                            WriteNonSecureStack = FALSE;
                        }
                        else
                        {
                            KD_ASSERT(dwStackSize == g_pDmpThread->pprcOwner->e32.e32_stackmax);
                        }
                    }

                    if (WriteNonSecureStack)
                    {
                        dwStackTop = (dwStackBase + dwStackSize);

                        // Save the stack between the Bound and Top
                        dwMemoryStart = dwStackBound;
                        dwMemorySize = dwStackTop - dwStackBound;

                        // Limit memory size to amount of file space left
                        if (dwMemorySize > dwFileSpaceLeft)
                        {
                            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Limiting Non-secure Stack memory to fit in dump file -> Old Size=0x%08X, New Size=0x%08X\r\n", dwMemorySize, dwFileSpaceLeft));
                            dwMemorySize = dwFileSpaceLeft;
                        }

                        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Add Non-secure Stack memory\r\n"));

                        hRes = MemoryBlocksAdd(g_pDmpThread->pprcVM, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                        if (FAILED(hRes))
                        {
                            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryBlocksAdd failed adding non-secure Stack memory, hRes=0x%08X\r\n", hRes));
                            goto Exit;
                        }
                    }
                }
                else
                {
                    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  Thread %08X failed to read tlsNonSecure(%08X) in VM(%08X)\r\n", g_pDmpThread,
                                g_pDmpThread->tlsNonSecure, g_pDmpThread->pprcVM));
                }
            }
            else
            {
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  Thread %08X does not have a VM\r\n", g_pDmpThread));
            }

            if (g_pDmpThread->tlsNonSecure != g_pDmpThread->tlsSecure)
            {
                // Calculate new File space left to limit size of secure stack
                // We don't simply add the previous memory block size, since it may have been
                // combined with another, so we work it out again from the begining
                
                // Calculate offset to include Memory List header and Memory descriptors 
                dwFileOffsetEstimate = (*pdwFileOffset) + 
                                       (sizeof(CEDUMP_MEMORY_LIST) * 2) + // Virtual + Physical list
                                       (sizeof(CEDUMP_MEMORY_DESCRIPTOR) * (g_dwMemoryVirtualBlocksTotal+1)); // +1 for secure stack memory descriptor
                                       
                // Calculate offset to include total memory added so far (This will now include the non-secure stack)                       
                dwFileOffsetEstimate += g_dwMemoryVirtualSizeTotal;
                
                // Calculate file space left for adding secure stack memory 
                dwFileSpaceLeft = dwMaxFileSize - dwFileOffsetEstimate;

                // Write the decriptor for secure stack (may be a fiber?)
                dwStackBase = g_pDmpThread->tlsSecure[PRETLS_STACKBASE];
                dwStackBound = g_pDmpThread->tlsSecure[PRETLS_STACKBOUND]; // Memory is commited to this point

                // Secure stack size is always fixed
                dwStackSize = KRN_STACK_SIZE;
                
                dwStackTop = (dwStackBase + dwStackSize);
                
                // Save the stack between the Bound and Top
                if (dwStackBound)
                {
                    dwMemoryStart = dwStackBound;
                    dwMemorySize = dwStackTop - dwStackBound;
                }
                else
                {
                    dwMemoryStart = dwStackBase;
                    dwMemorySize  = dwStackSize;
                }

                // Limit memory size to amount of file space left
                if (dwMemorySize > dwFileSpaceLeft)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Limiting Secure Stack memory to fit in dump file -> Old Size=0x%08X, New Size=0x%08X\r\n", dwMemorySize, dwFileSpaceLeft));
                    dwMemorySize = dwFileSpaceLeft;
                }

                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Add Secure Stack memory\r\n"));

                PPROCESS SecureStackVM;
                if (dwMemoryStart < VM_KMODE_BASE)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Secure Stack for thread in user mode?\r\n"));
                    SecureStackVM = g_pDmpThread->pprcVM;
                }
                else
                {
                    SecureStackVM = g_pprcNK;
                }
                
                hRes = MemoryBlocksAdd (SecureStackVM, dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryBlocksAdd failed adding secure Stack memory, hRes=0x%08X\r\n", hRes));
                    goto Exit;
                }
            }
        }
        
        if (fAllVirtualMemory)
        {
            hRes = MemoryAddAll(VIRTUAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryAddAllVirtual failed, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }
    }

    hRes = WriteCeStreamMemoryList(ceDumpType, pceDmpDirectory, pdwFileOffset, VIRTUAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: WriteCeStreamMemoryList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamMemoryVirtualList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamMemoryPhysicalList

    Write the list of Physical Memory dumps.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamMemoryPhysicalList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    BOOL  fAllPhysicalMemory = FALSE;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamMemoryPhysicalList: Enter\r\n"));

    switch (ceDumpType)
    {
        case ceDumpTypeComplete:
            // The complete dump includes all physical memory
            fAllPhysicalMemory = TRUE;
            break;
            
        case ceDumpTypeSystem:
            break;
            
        case ceDumpTypeContext:
            break;
            
        default:
            // This should not happen
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryPhysicalList: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
            KD_ASSERT(FALSE); 
            hRes = E_FAIL;
            goto Exit;
    }

    if (!fWrite)
    {
        if (fAllPhysicalMemory)
        {
            hRes = MemoryAddAll(PHYSICAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryAddAllVirtual failed, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }
    }

    hRes = WriteCeStreamMemoryList(ceDumpType, pceDmpDirectory, pdwFileOffset, PHYSICAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: WriteCeStreamMemoryList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamMemoryPhysicalList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}



/*----------------------------------------------------------------------------
    WriteHeapItem

----------------------------------------------------------------------------*/
HRESULT WriteHeapItem(CEDUMP_HEAP_ITEM_DATA* pItem, DWORD* pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    DWORD dwBegin1;
    DWORD dwSize1;
    DWORD dwBegin2;
    DWORD dwSize2;

    if (pItem)
    {
        // Get block sizes
        if (pItem->pBeginTruncateOffset == 0)
        {
            // Whole item, no truncation
            dwBegin1 = pItem->pMem;
            dwSize1  = pItem->cbSize;
            
            dwBegin2 = 0;
            dwSize2  = 0;
        }
        else
        {
            dwBegin1 = pItem->pMem;
            dwSize1  = pItem->pBeginTruncateOffset;
            
            dwBegin2 = pItem->pMem + pItem->pEndTruncateOffset;
            dwSize2  = pItem->cbSize - pItem->pEndTruncateOffset;
        }

        dwWriteOffset = 0;

        if (fWrite)
        {
            // Write first block
            hRes = WatsonWriteData((*pdwFileOffset), (PVOID)dwBegin1, dwSize1, TRUE);
            
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteHeapItem: WatsonWriteData failed writing 1st block, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }
        else
        {
            hRes = S_OK;
        }

        dwWriteOffset += dwSize1;

        // Write second block (if necessary)
        if (dwBegin2 && dwSize2)
        {
            if (fWrite)
            {
                hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, (PVOID)dwBegin2, dwSize2, TRUE);
                
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteHeapItem: WatsonWriteData failed writing 1st block, hRes=0x%08X\r\n", hRes));
                    goto Exit;
                }
            }

            dwWriteOffset += dwSize2;
        }
Exit:

        *pdwFileOffset += dwWriteOffset;
    }

    return hRes;
}



/*----------------------------------------------------------------------------
    WriteHeapCorruptionData


----------------------------------------------------------------------------*/
HRESULT WriteHeapCorruptionData(UINT i, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwFileOffsetData = *pdwFileOffset;
    
    CEDUMP_HEAP_CORRUPTION_DATA* phcd = (CEDUMP_HEAP_CORRUPTION_DATA*) g_ceDmpDiagnoses[i].pDataTemp;

    if (phcd)
    {
        dwFileOffsetData += sizeof(CEDUMP_HEAP_CORRUPTION_DATA);

        phcd->CorruptItem.Item.Rva  = dwFileOffsetData;
        
        dwFileOffsetData += phcd->CorruptItem.Item.DataSize;

        phcd->PreviousItem.Item.Rva = dwFileOffsetData;

        if (fWrite)
        {
            // Write meta data
            hRes = WatsonWriteData((*pdwFileOffset), phcd, sizeof(CEDUMP_HEAP_CORRUPTION_DATA), TRUE);
            
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteHeapCorruptionData: WatsonWriteData failed writing CEDUMP_HEAP_CORRUPTION_DATA, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        *pdwFileOffset += sizeof(CEDUMP_HEAP_CORRUPTION_DATA);


        // Write raw data (item 1)

        hRes = WriteHeapItem(&phcd->CorruptItem, pdwFileOffset, fWrite);
            
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteHeapCorruptionData: failed writing Corrupt Item, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }


        // Write raw data (item 2)

        hRes = WriteHeapItem(&phcd->PreviousItem, pdwFileOffset, fWrite);
        
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteHeapCorruptionData: failed writing Corrupt Item, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

Exit:
    return hRes;
}



/*----------------------------------------------------------------------------
    WriteOOMData


----------------------------------------------------------------------------*/
HRESULT WriteOOMData(UINT i, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    CEDUMP_HEAP_INFO* pHeapInfo = NULL;
    
    CEDUMP_SYS_MEMINFO* pmi = (CEDUMP_SYS_MEMINFO*) g_ceDmpDiagnoses[i].pDataTemp;

    if (pmi)
    {
        // System Memory Info

        if (fWrite)
        {
            // Don't update until write pass.  We're overloading HeapInfo.Rva to
            // point to local data until write time.
            
            pHeapInfo = (CEDUMP_HEAP_INFO*) pmi->HeapInfo.Rva;
            pmi->HeapInfo.Rva = *pdwFileOffset + pmi->cbSize;


            // Write meta data
            hRes = WatsonWriteData((*pdwFileOffset), pmi, pmi->cbSize, TRUE);
            
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteOOMData: WatsonWriteData failed writing CEDUMP_MEMINFO, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        *pdwFileOffset += pmi->cbSize;


        // Heap list

        if (fWrite)
        {
            // Write meta data
            hRes = WatsonWriteData((*pdwFileOffset), pHeapInfo, pmi->HeapInfo.DataSize, TRUE);
            
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteOOMData: WatsonWriteData failed writing CEDUMP_HEAP_INFO, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
        }

        *pdwFileOffset += pmi->HeapInfo.DataSize;
    }

Exit:
    return hRes;
}


/*----------------------------------------------------------------------------
    WriteCeStreamDiagnosisList

    Write the list of DDx diagnoses
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamDiagnosisList(MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    DWORD dwFileOffsetDescription;
    DWORD dwFileOffsetData;
    DWORD dwStartDescription;
    DWORD dwMemorySize;
    DWORD dwMemorySizeDesc;
    DWORD dwMemorySizeData;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamDiagnosisList: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamDiagnosisList: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamDiagnosisList: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }


    pceDmpDirectory->StreamType        = ceStreamDiagnosisList;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_DIAGNOSIS_LIST) +
                                         (sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR) * (g_nDiagnoses)); 
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;


    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamDiagnosisList: Writing %u diagnoses for a total of %u bytes\r\n", g_nDiagnoses, pceDmpDirectory->Location.DataSize));

   
    // Write out Diagnosis List header info

    dwWriteOffset = 0;
    dwFileOffsetDescription = (*pdwFileOffset) + pceDmpDirectory->Location.DataSize;
    dwFileOffsetData = dwFileOffsetDescription;

    for (UINT i = 0; (i < g_nDiagnoses); i++)
    {
        dwFileOffsetData += g_ceDmpDiagnoses[i].Description.DataSize;
    }

    if (fWrite)
    {     
        // Write out the Diagnosis List Stream header
        g_ceDmpDiagnosisList.SizeOfHeader = sizeof(CEDUMP_DIAGNOSIS_LIST);
        g_ceDmpDiagnosisList.SizeOfEntry  = sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR);
        g_ceDmpDiagnosisList.NumberOfEntries = g_nDiagnoses;

        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpDiagnosisList, sizeof(CEDUMP_DIAGNOSIS_LIST), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamDiagnosisList: WatsonWriteData failed writing Diagnosis List Stream header, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    dwWriteOffset += sizeof(CEDUMP_DIAGNOSIS_LIST);

    // Write out Diagnosis Descriptors

    for (UINT i = 0; (i < g_nDiagnoses); i++)
    {
        dwMemorySizeDesc = g_ceDmpDiagnoses[i].Description.DataSize;
        dwMemorySizeData = g_ceDmpDiagnoses[i].Data.DataSize;

        
        g_ceDmpDiagnoses[i].Description.Rva = dwFileOffsetDescription;
        g_ceDmpDiagnoses[i].Data.Rva = dwFileOffsetData;
        
        if (fWrite)
        {    
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpDiagnoses[i], sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR), TRUE);
            
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamDiagnosisList: WatsonWriteData failed writing diagnosis descriptor, Index=%u, hRes=0x%08X\r\n", 
                                          i, hRes));
                goto Exit;
            }
        }
        
        dwWriteOffset += sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR);

        dwFileOffsetDescription += dwMemorySizeDesc;
        dwFileOffsetData += dwMemorySizeData;
    }

    KD_ASSERT(fWrite ? (dwWriteOffset == pceDmpDirectory->Location.DataSize) : TRUE);

    *pdwFileOffset += pceDmpDirectory->Location.DataSize;


    // Write out Descriptions 

    dwWriteOffset = 0;
    dwStartDescription = (DWORD) GetDDxLogHead();

    for (UINT i = 0; (i < g_nDiagnoses); i++)
    {
        dwMemorySize = g_ceDmpDiagnoses[i].Description.DataSize;
        
        if (fWrite)
        {
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, (PVOID)dwStartDescription, dwMemorySize, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamDiagnosisList: WatsonWriteData failed writing memory block, start=0x%08x, size=%u, hRes=0x%08X\r\n", 
                                          dwStartDescription, dwMemorySize, hRes));
                goto Exit;
            }
        }

        dwWriteOffset += dwMemorySize;
        dwStartDescription += dwMemorySize;
    }

    KD_ASSERT(fWrite ? (dwFileOffsetDescription == (*pdwFileOffset) + dwWriteOffset) : TRUE);

    *pdwFileOffset += dwWriteOffset;


    // Write out issue-specific data

    for (UINT i = 0; (i < g_nDiagnoses); i++)
    {
        switch (g_ceDmpDiagnoses[i].Type)
        {
            case Type_HeapCorruption:
                hRes = WriteHeapCorruptionData(i, pdwFileOffset, fWrite);
                break;
                
            case Type_LowMemory: 
                hRes = WriteOOMData(i, pdwFileOffset, fWrite);
                break;

            case Type_Starvation:
                // TODO: 

            case Type_WaitCursor:
                // TODO: 

            case Type_Deadlock: 
                // Fall through, no extra data ...
            default:
                break;
        }
    }

    // Make sure we are aligned on exit
    hRes = DWAlignFile(*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamDiagnosisList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


/*----------------------------------------------------------------------------
    WriteCeStreamBucketParameters

    Write the Bucket Parameters Stream to the dump file.
----------------------------------------------------------------------------*/
static HRESULT WriteCeStreamBucketParameters(MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD  dwWriteOffset;
    LPWSTR lpwzEventType;
    LPWSTR lpwzAppName;
    LPWSTR lpwzModName;
    LPWSTR lpwzOwnerName;
    RVA    rvaString;
    DWORD  dwStringSize;
    int    i, istr;
    LPWSTR lpwzExStrings[NUM_EX_PARAM_DESCRIPTORS];

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteCeStreamBucketParameters: Enter\r\n"));

    // Make sure stream is correctly aligned
    KD_ASSERT(((*pdwFileOffset)%CEDUMP_ALIGNMENT) == 0);
    if (((*pdwFileOffset)%CEDUMP_ALIGNMENT) != 0)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamBucketParameters: File offset (%u) not %u byte aligned\r\n",
                                  *pdwFileOffset, CEDUMP_ALIGNMENT));
        hRes = E_FAIL;
        goto Exit;
    }

    // On write pass *pdwFileOffset should match Location.Rva
    KD_ASSERT(fWrite ? pceDmpDirectory->Location.Rva == *pdwFileOffset : TRUE);
    if (fWrite && (pceDmpDirectory->Location.Rva != *pdwFileOffset))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamBucketParameters: File offset (%u) on write pass does not match expected value (%u)\r\n",
                                  *pdwFileOffset, pceDmpDirectory->Location.Rva));
        hRes = E_FAIL;
        goto Exit;
    }

    pceDmpDirectory->StreamType        = ceStreamBucketParameters;
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_BUCKET_PARAMETERS);
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;


    //
    // If called via the CaptureDumpFileOnDevice API or 
    // if DDx has produced no positive diagnosis, then 
    // fill in the standard bucket params.
    //
    // If there is a postive diagnosis DDx will have 
    // already filled this structure.
    //
    if (g_fCaptureDumpFileOnDeviceCalled || (g_nDiagnoses == 0))
    {
        hRes = GetBucketParameters(&g_ceDmpBucketParameters, g_pDmpThread, g_pDmpProc);

        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: GetBucketParameters failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }
    else
    {
        memcpy((void*)&g_ceDmpBucketParameters, (void*)&g_ceDmpDDxBucketParameters, sizeof(CEDUMP_BUCKET_PARAMETERS));
        g_ceDmpBucketParameters.EventType = DWGetBucketEventType ();
    }

    // Convert String pointers to RVAs
    rvaString = pceDmpDirectory->Location.Rva + pceDmpDirectory->Location.DataSize;

    // Event Type 
    lpwzEventType = (LPWSTR)g_ceDmpBucketParameters.EventType;
    g_ceDmpBucketParameters.EventType = rvaString;
    dwStringSize = wcslen(lpwzEventType) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // App Name
    lpwzAppName = (LPWSTR)g_ceDmpBucketParameters.AppName;
    g_ceDmpBucketParameters.AppName = rvaString;
    dwStringSize = wcslen(lpwzAppName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // Mod Name
    lpwzModName = (LPWSTR)g_ceDmpBucketParameters.ModName;
    g_ceDmpBucketParameters.ModName = rvaString;
    dwStringSize = wcslen(lpwzModName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // Owner Name
    lpwzOwnerName = (LPWSTR)g_ceDmpBucketParameters.OwnerName;
    g_ceDmpBucketParameters.OwnerName = rvaString;
    dwStringSize = wcslen(lpwzOwnerName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // Extended parameters
    istr = 0;
    for (i = 0; i < NUM_EX_PARAM_DESCRIPTORS; i++)
    {
        if ((g_ceDmpBucketParameters.ExParam[i].Type == BUCKET_PARAM_PROC_NAME) ||
            (g_ceDmpBucketParameters.ExParam[i].Type == BUCKET_PARAM_MOD_NAME))
        {
            lpwzExStrings[istr] = (LPWSTR)g_ceDmpBucketParameters.ExParam[i].Data;
            g_ceDmpBucketParameters.ExParam[i].Data = rvaString;
            dwStringSize = wcslen(lpwzExStrings[istr]) * sizeof(WCHAR);
            rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));
            
            istr++;
        }
    }
    
    if (fWrite)
    {
        dwWriteOffset = 0;

        // Write Bucket Parameters stream header
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &g_ceDmpBucketParameters, sizeof(CEDUMP_BUCKET_PARAMETERS), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Bucket Parameters stream header, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += pceDmpDirectory->Location.DataSize;

        // Write Event Type string size
        dwStringSize =wcslen(lpwzEventType) * sizeof(WCHAR);
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwStringSize, sizeof(dwStringSize), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Event Type string size, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwStringSize);

        // Write Event Type string
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, lpwzEventType, (dwStringSize + (1 * sizeof(WCHAR))), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Event Type string, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += (dwStringSize + (1 * sizeof(WCHAR)));

        // Write App Name string size
        dwStringSize = wcslen(lpwzAppName) * sizeof(WCHAR);
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwStringSize, sizeof(dwStringSize), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing App Name string size, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwStringSize);

        // Write App Name string
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, lpwzAppName, (dwStringSize + (1 * sizeof(WCHAR))), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing App Name string, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += (dwStringSize + (1 * sizeof(WCHAR)));

        // Write Mod Name string size
        dwStringSize = wcslen(lpwzModName) * sizeof(WCHAR);
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwStringSize, sizeof(dwStringSize), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Mod Name string size, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwStringSize);

        // Write Mod Name string
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, lpwzModName, (dwStringSize + (1 * sizeof(WCHAR))), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Mod Name string, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += (dwStringSize + (1 * sizeof(WCHAR)));

        // Write Owner Name string size
        dwStringSize = wcslen(lpwzOwnerName) * sizeof(WCHAR);
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwStringSize, sizeof(dwStringSize), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Owner Name string size, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwStringSize);

        // Write Owner Name string
        hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, lpwzOwnerName, (dwStringSize + (1 * sizeof(WCHAR))), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing Owner Name string, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += (dwStringSize + (1 * sizeof(WCHAR)));

        
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamBucketParameters: *pdwFileOffset(4) = 0x%08x\r\n", (*pdwFileOffset) + dwWriteOffset));

        // Extended parameters
        for (i = 0; i < istr; i++)
        {
            // Write string size
            dwStringSize = wcslen(lpwzExStrings[i]) * sizeof(WCHAR);
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &dwStringSize, sizeof(dwStringSize), TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing extended param string size, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += sizeof(dwStringSize);

            // Write string
            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, lpwzExStrings[i], (dwStringSize + (1 * sizeof(WCHAR))), TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing extended param string, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }
            dwWriteOffset += (dwStringSize + (1 * sizeof(WCHAR)));

            
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamBucketParameters: *pdwFileOffset(5.%i) = 0x%08x\r\n", i, (*pdwFileOffset) + dwWriteOffset));
        }

        KD_ASSERT(rvaString == ((*pdwFileOffset) + dwWriteOffset));
    }

    *pdwFileOffset = rvaString;
    hRes = DWAlignFile (*pdwFileOffset, fWrite);

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamBucketParameters: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteDumpFileContents

    Write the contents of the dump file.
----------------------------------------------------------------------------*/
static HRESULT WriteDumpFileContents(CEDUMP_TYPE ceDumpType, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD   dwNumberOfStreams = MAX_STREAMS;
    DWORD   dwStream = 0;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteDumpFileContents: Enter, ceDumpType=%u\r\n", ceDumpType));

    if (!fWrite)
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: *************************\r\n"));
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: *** Optimization Pass ***\r\n"));
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: *************************\r\n"));
        // Clear these only during optimization pass 
        memset(&g_ceDmpHeader, 0, sizeof(g_ceDmpHeader));
        memset(&g_ceDmpDirectory, 0, sizeof(g_ceDmpDirectory));
        memset(&g_ceDmpSystemInfo, 0, sizeof(g_ceDmpSystemInfo));
        memset(&g_ceDmpExceptionStream, 0, sizeof(g_ceDmpExceptionStream));
        memset(&g_ceDmpException, 0, sizeof(g_ceDmpException));
        memset(&g_ceDmpExceptionContext, 0, sizeof(g_ceDmpExceptionContext));
        memset(&g_ceDmpModuleList, 0, sizeof(g_ceDmpModuleList));
        memset(&g_ceDmpProcessList, 0, sizeof(g_ceDmpProcessList));
        memset(&g_ceDmpThreadList, 0, sizeof(g_ceDmpThreadList));
        memset(&g_ceDmpThreadContextList, 0, sizeof(g_ceDmpThreadContextList));
        memset(&g_ceDmpThreadCallStackList, 0, sizeof(g_ceDmpThreadCallStackList));
        memset(&g_ceDmpMemoryVirtualList, 0, sizeof(g_ceDmpMemoryVirtualList));
        memset(&g_ceDmpMemoryPhysicalList, 0, sizeof(g_ceDmpMemoryPhysicalList));
        // TODO: Make consistent  memset(&g_ceDmpBucketParameters, 0, sizeof(g_ceDmpBucketParameters));
        memset(&g_memoryVirtualBlocks, 0xFF, sizeof(g_memoryVirtualBlocks));
        memset (&g_ceDmpProcessModuleList, 0, sizeof (g_ceDmpProcessModuleList));
        g_dwMemoryVirtualBlocksTotal = 0;
        g_dwMemoryVirtualSizeTotal = 0;
        memset(&g_memoryPhysicalBlocks, 0xFF, sizeof(g_memoryPhysicalBlocks));
        g_dwMemoryPhysicalBlocksTotal = 0;
        g_dwMemoryPhysicalSizeTotal = 0;
    }
    else
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: *************************\r\n"));
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: ***  Generation Pass  ***\r\n"));
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteDumpFileContents: *************************\r\n"));
        // Initialize the Dump File CRC only during write pass
        g_dwWrittenOffset = 0;
        g_dwWrittenCRC = 0;
    }

    PING_HOST_IF(!fWrite);

    // Add the dump header
    hRes = WriteCeHeader(dwNumberOfStreams, ceDumpType, pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeHeader failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the directory
    hRes = WriteCeDirectory(pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeDirectory failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the SystemInfo stream (First stream)
    dwStream = 0;

    hRes = WriteCeStreamSystemInfo(&g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamSystemInfo failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Exception stream (Must be second stream)
    // *** EXCEPTION STREAM MUST BE 2ND STREAM, It may change the primary process and thread *** 
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamException(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamException failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Module List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamModuleList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Module list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Process List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamProcessList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Process list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Thread List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamThreadList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Thread list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Thread Context List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamThreadContextList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Thread Context list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Thread Call Stack List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamThreadCallStackList(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamThreadCallStackList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Bucket Parameters stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamBucketParameters(&g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamBucketParameters failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Process-Module Map stream.
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList (ceStreamProcessModuleMap, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED (hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  WriteDumpFileContents: WriteCeStreamElementList failed writing Process-Module map, hRes=0x%08x\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Diagnosis List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamDiagnosisList(&g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamDiagnosisList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Memory Virtual List stream 
    // *** MEMORY VIRTUAL LIST MUST BE 2ND LAST STREAM, Context/Complete dump will trim the virtual memory list to fit into 64K limit *** 
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamMemoryVirtualList(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamMemoryVirtualList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    PING_HOST_IF(!fWrite);

    // Add the Memory Physical List stream 
    // *** MEMORY PHYSICAL LIST MUST BE LAST STREAM, Complete dump needs the above virtual memory list to fit into initial 64K limit *** 
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamMemoryPhysicalList(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamMemoryPhysicalList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    KD_ASSERT((dwStream+1) == dwNumberOfStreams);
    if ((dwStream+1) != dwNumberOfStreams)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: Incorrect number of streams written, Expected=%u, Actual=%u\r\n", dwNumberOfStreams, dwStream+1));
        hRes = E_FAIL;
        goto Exit;
    }

    hRes = S_OK;

    goto Exit;

ErrMaxStreams:

    KD_ASSERT(FALSE);  // dwStream >= MAX_STREAMS 
    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: Exceeded max streams allowed, Max Streams=%u, Stream No=%u\r\n", MAX_STREAMS, dwStream));
    hRes = E_FAIL;
    
Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteDumpFileContents: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    OptimizeDumpFileContents

    Optimize the contents of the dump file.
----------------------------------------------------------------------------*/
static HRESULT OptimizeDumpFileContents(CEDUMP_TYPE *pceDumpType, DWORD dwMaxDumpFileSize, BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;
    DWORD   dwFileOffset = 0;
    BOOL    fFileOptimized = FALSE;
    CEDUMP_TYPE ceDumpTypeOptimized = *pceDumpType;
    DWORD   dwMaxDumpFileSizeForType;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"++DwDmpGen!OptimizeDumpFileContents: Enter\r\n"));

    // For System dump type, first try with heap data + nk.exe globals (if in call stack), then without
    g_fDumpHeapData = TRUE;
    g_fDumpNkGlobals = TRUE;

    while ((FALSE == fFileOptimized) && (ceDumpTypeOptimized != ceDumpTypeUndefined))
    {
        // Calculate the size of the dump file contents (Call write routine with fWrite=FALSE)
        dwFileOffset = 0;

        // Set to false for each dump attempt, so we can see if this was added during pass
        g_fNkGlobalsDumped = FALSE;

        // Init to no error for each optimization pass
        g_dwLastError = ERROR_SUCCESS;
        
        hRes = WriteDumpFileContents(ceDumpTypeOptimized, &dwFileOffset, FALSE /* fWrite */);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: WriteDumpFileContents failed, will try smaller dump type, hRes=0x%08X\r\n", hRes));

            // This will force a smaller dump type to be tried
            dwFileOffset = (DWORD)(-1);
        }

        // Determine what's the maximum size of the dump file.
        dwMaxDumpFileSizeForType = dwMaxDumpFileSize;
        if (ceDumpTypeContext == ceDumpTypeOptimized &&
            !fOnTheFly)
        {
            // If Watson is attempting to capture a context dump,
            // limit the captured data to 64KiB max.
            // EXCEPTION, if capturing on the fly, ignore this
            // maximum size.
            dwMaxDumpFileSizeForType = WATSON_CONTEXT_DUMPFILE_SIZE;
        }

        if (dwFileOffset <= dwMaxDumpFileSizeForType)
        {
            fFileOptimized = TRUE;
        }
        else
        {
            switch(ceDumpTypeOptimized)
            {
                case ceDumpTypeComplete:
                    ceDumpTypeOptimized = ceDumpTypeSystem;
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Complete Dump too large (%u), Max Dump Size = %u\r\n", dwFileOffset, dwMaxDumpFileSizeForType));
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Switching to System Dump.\r\n"));
                    break;
                case ceDumpTypeSystem:
                    if (TRUE == g_fDumpHeapData)
                    {
                        // If we had heap data, try system dump again without heap data
                        g_fDumpHeapData = FALSE;

                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: System Dump with heap data too large (%u), Max Dump Size = %u\r\n", dwFileOffset, dwMaxDumpFileSizeForType));
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Removing heap data from System Dump.\r\n"));
                    }
                    else if ((TRUE == g_fDumpNkGlobals) && (TRUE == g_fNkGlobalsDumped))
                    {
                        // If we added nk.exe globals, try system dump again without nk.exe globals 
                        g_fDumpNkGlobals = FALSE;

                        // Add heap data again, it may fit this time
                        g_fDumpHeapData = TRUE;
                        
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: System Dump with nk.exe globals too large (%u), Max Dump Size = %u\r\n", dwFileOffset, dwMaxDumpFileSizeForType));
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Removing nk.exe globals & adding heap data back to System Dump.\r\n"));
                    }
                    else
                    {
                        ceDumpTypeOptimized = ceDumpTypeContext;
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: System Dump too large (%u), Max Dump Size = %u\r\n", dwFileOffset, dwMaxDumpFileSizeForType));
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Switching to Context Dump.\r\n"));
                    }
                    break;
                case ceDumpTypeContext:
                    // We should not get here, since the Context dump should always limit itself to WATSON_MINIMUM_DUMPFILE_SIZE
                    KD_ASSERT(dwFileOffset <= WATSON_CONTEXT_DUMPFILE_SIZE); 
                    ceDumpTypeOptimized = ceDumpTypeUndefined;
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Context Dump too large (%u), Max Dump Size = %u\r\n", dwFileOffset, dwMaxDumpFileSizeForType));
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Switching to Undefined Dump (None).\r\n"));
                    break;
                case ceDumpTypeUndefined:
                default:
                    // This should not happen
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!OptimizeDumpFileContents: Bad Optimized Dump Type detected, ceDumpType=%u\r\n", ceDumpTypeOptimized));
                    KD_ASSERT(FALSE); 
                    hRes = E_FAIL;
                    goto Exit;
            }
        }
    }

    if (FALSE == fFileOptimized)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Dump file optimization failed, Max Size=%u, Optimized Size = %u\r\n", dwMaxDumpFileSize, dwFileOffset));
        if (SUCCEEDED(hRes))
        {
            // Only set this if not already set to a failure condition
            hRes = E_FAIL;
        }
        goto Exit;
    }

    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Dump File optimized, Requested Type = %u, Optimized Type = %u\r\n", *pceDumpType, ceDumpTypeOptimized));
    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!OptimizeDumpFileContents: Dump File optimized, Max Size = %u, Optimized Size = %u\r\n", dwMaxDumpFileSize, dwFileOffset));

    *pceDumpType = ceDumpTypeOptimized;
    g_ceDumpType = ceDumpTypeOptimized;
    g_dwDumpFileSize = dwFileOffset;

    hRes = S_OK;
    
Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"--DwDmpGen!OptimizeDumpFileContents: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteDumpFile

    Write out the entire dump file.
----------------------------------------------------------------------------*/
static HRESULT WriteDumpFile(CEDUMP_TYPE ceDumpType, DWORD dwMaxDumpFileSize, BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;
    DWORD dwFileOffset = 0;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteDumpFile: Enter\r\n"));

    // Optimize dump file contents, this may change the type
    hRes = OptimizeDumpFileContents(&ceDumpType, dwMaxDumpFileSize, fOnTheFly);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFile: OptimizeDumpFileContents failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Write dump file contents (Call write routine with fWrite=TRUE)
    dwFileOffset = 0;
    hRes = WriteDumpFileContents(ceDumpType, &dwFileOffset, TRUE /* fWrite */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFile: WriteDumpFileContents failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    if (fOnTheFly)
    {
        // Flush any remaining data to host
        hRes = WatsonWriteKDBG(0, TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFile: WatsonWriteKDBG failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteDumpFile: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    ReadSettings

    Read registry settings stored in reserved memory.

    Return Values:
        S_OK    - No problems.
        E_XXX   - Errors.
----------------------------------------------------------------------------*/
static HRESULT ReadSettings(CEDUMP_TYPE *pceDumpType, DWORD *pdwMaxDumpFileSize)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMaxDumpFileSize;
    DWORD dwSettingsOffset;
    DWORD dwSettingsCRC;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!ReadSettings: Enter\r\n"));

    KD_ASSERT((NULL != pceDumpType) && (NULL != pdwMaxDumpFileSize));
    if ((NULL == pceDumpType) && (NULL == pdwMaxDumpFileSize))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!ReadSettings: Bad parameter passed in\r\n"));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    // Get the amount of memory reserved for dump files
    g_dwReservedMemorySize = NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_GETWATSONSIZE, NULL, 0, NULL, 0, NULL);
    if (g_dwReservedMemorySize < WATSON_MINIMUM_RESERVED_MEMORY)
    {
        // Reserved memory is allocated by adding 'dwNKDrWatsonSize = [size to reserve];' to OEMInit().
        // The size of memory to reserve must be a multiple of VM_PAGE_SIZE bytes
        // WATSON_MINIMUM_RESERVED_MEMORY_RECOMMENDED is minimum recommended, however
        // absolute minimum to reserve is WATSON_MINIMUM_RESERVED_MEMORY
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!ReadSettings: Insufficient memory reserved for dump capture! Currently Reserved = %u, Minimum Required = %u\r\n", 
                                     g_dwReservedMemorySize,
                                     WATSON_MINIMUM_RESERVED_MEMORY));
        hRes = E_OUTOFMEMORY;
        goto Exit;
    }
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Reserved Memory Size=%u\r\n", g_dwReservedMemorySize));

    // Maximum dump file size is the reserved memory less the space for registry settings
    dwMaxDumpFileSize = g_dwReservedMemorySize - WATSON_REGISTRY_SETTINGS_SIZE;

    // Make sure the settings will fit in the space reserved for the settings
    KD_ASSERT((sizeof(WATSON_DUMP_SETTINGS) + sizeof(DWORD))<= WATSON_REGISTRY_SETTINGS_SIZE);

    // The Watson dump settings are at the end of the reserved memory
    dwSettingsOffset = g_dwReservedMemorySize - WATSON_REGISTRY_SETTINGS_SIZE;

    // Reset the read offset and CRC
    g_dwReadOffset = dwSettingsOffset;
    g_dwReadCRC = 0;

    // Read in the dump settings
    hRes = WatsonReadData(dwSettingsOffset, &g_watsonDumpSettings, sizeof(WATSON_DUMP_SETTINGS), TRUE /* Calculate CRC */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!ReadSettings: WatsonReadData failed reading dump settings, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    dwSettingsOffset += sizeof (WATSON_DUMP_SETTINGS);
    
    // Read in the CRC to make sure the settings are valid
    hRes = WatsonReadData(dwSettingsOffset, &dwSettingsCRC, sizeof(DWORD), FALSE /* Don't calculate CRC */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!ReadSettings: WatsonReadData failed getting dump settings CRC, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Check if Header is correct size and CRC is valid
    if ((dwSettingsCRC != g_dwReadCRC) || (g_watsonDumpSettings.dwSizeOfHeader != sizeof(WATSON_DUMP_SETTINGS)))
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!ReadSettings: Dump Settings CRC or size failure!, Expected CRC=0x%08X, Actual CRC=0x%08X, Expected Size=%u, Actual Size=%u\r\n", 
                                     dwSettingsCRC, g_dwReadCRC, sizeof(WATSON_DUMP_SETTINGS), g_watsonDumpSettings.dwSizeOfHeader));
        g_dwLastError = ERROR_CRC;
        hRes = E_FAIL;
        goto Exit;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Dump Settings validated, CRC=0x%08X, Header Size=%u, Max Dump Size=%u\r\n", 
                                dwSettingsCRC, g_watsonDumpSettings.dwSizeOfHeader, g_watsonDumpSettings.dwMaxDumpFileSize));
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Dump Settings validated, Dump CRC=0x%08X, Dump Size=%u\r\n", 
                                g_watsonDumpSettings.dwDumpFileCRC, g_watsonDumpSettings.dwDumpWritten));

    if (!g_watsonDumpSettings.fDumpEnabled)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!ReadSettings: Dump generation disabled, quiting ... \r\n"));
        g_dwLastError = ERROR_SERVICE_DISABLED;
        hRes = E_FAIL;
        goto Exit;
    }

    if (g_watsonDumpSettings.dwDumpWritten > 0)
    {
        // We check for > 0, not >= WATSON_MINIMUM_DUMPFILE_SIZE, since the written size could be smaller than the defined minimum
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!ReadSettings: Dump file (%u Bytes) already in reserved memory, quiting ... \r\n", g_watsonDumpSettings.dwDumpWritten));
        if (g_hEventDumpFileReady)
        {
            // Tell DwXfer.dll that a dump file is ready, just in case it missed the event last time?
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Set 'Dump File Ready' Event\r\n"));
            SafeSetEvent(g_hEventDumpFileReady);
        }
        g_dwLastError = ERROR_ALREADY_EXISTS;
        hRes = E_FAIL;
        goto Exit;
    }

    // Use the registry setting for Dump Type if it is valid, otherwise default
    if ((g_watsonDumpSettings.ceDumpType >= ceDumpTypeContext) && (g_watsonDumpSettings.ceDumpType <= ceDumpTypeComplete))
    {
        *pceDumpType = g_watsonDumpSettings.ceDumpType;
    }
    else
    {
        // Default setting
        *pceDumpType = (CEDUMP_TYPE)WATSON_REGVALUE_DUMP_TYPE_DEFAULT_DATA;
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Invalid Dump Type (%u) specified, using default System Dump (%u)\r\n", g_watsonDumpSettings.ceDumpType, *pceDumpType));
    }
    
    // Use the registry setting for Dump File Size if it is valid, otherwise default
    if ((g_watsonDumpSettings.dwMaxDumpFileSize >= WATSON_CONTEXT_DUMPFILE_SIZE) && (g_watsonDumpSettings.dwMaxDumpFileSize <= dwMaxDumpFileSize))
    {
        *pdwMaxDumpFileSize = g_watsonDumpSettings.dwMaxDumpFileSize;
    }
    else
    {
        // Default to max size allowed by reserved memory
        *pdwMaxDumpFileSize = dwMaxDumpFileSize;
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!ReadSettings: Invalid Max Dump File Size (%u) specified, using default (%u)\r\n", g_watsonDumpSettings.dwMaxDumpFileSize, (*pdwMaxDumpFileSize)));
    }
    
    hRes = S_OK;
    
Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!ReadSettings: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteSettings

    Write settings to reserved memory.
----------------------------------------------------------------------------*/
static HRESULT WriteSettings()
{
    HRESULT hRes = E_FAIL;
    DWORD dwSettingsOffset;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteSettings: Enter\r\n"));

    // g_dwReservedMemorySize should already be initialized correctly
    KD_ASSERT(g_dwReservedMemorySize >= WATSON_MINIMUM_RESERVED_MEMORY);
    if (g_dwReservedMemorySize < WATSON_MINIMUM_RESERVED_MEMORY)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteSettings: Reserved memory variable not initialized correctly! Current Value = %u, Minimum Required = %u\r\n", 
                                 g_dwReservedMemorySize,
                                 WATSON_MINIMUM_RESERVED_MEMORY));
        hRes = E_FAIL;
        goto Exit;
    }

    // Make sure the settings will fit in the space reserved for the settings
    KD_ASSERT((sizeof(WATSON_DUMP_SETTINGS) + sizeof(DWORD)) <= WATSON_REGISTRY_SETTINGS_SIZE);

    // The Watson dump settings are at the end of the reserved memory
    dwSettingsOffset = g_dwReservedMemorySize - WATSON_REGISTRY_SETTINGS_SIZE;

    // Reset the write offset and CRC
    g_dwWrittenOffset = dwSettingsOffset;
    g_dwWrittenCRC = 0;

    // Write the dump settings
    hRes = WatsonWriteData(dwSettingsOffset, &g_watsonDumpSettings, sizeof(WATSON_DUMP_SETTINGS), TRUE /* Calculate CRC */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteSettings: WatsonWriteData failed writing dump settings, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    dwSettingsOffset += sizeof (WATSON_DUMP_SETTINGS);
    
    // Write the CRC to make sure the settings are valid
    hRes = WatsonWriteData(dwSettingsOffset, &g_dwWrittenCRC, sizeof(DWORD), FALSE /* Don't calculate CRC */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteSettings: WatsonWriteData failed writing dump settings CRC, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }
    
    hRes = S_OK;
    
Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteSettings: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    Initialize

    Initialize any variables, read registry, etc.

    Return Values:
        S_OK    - No problems.
        E_XXX   - Errors.
----------------------------------------------------------------------------*/
static HRESULT Initialize(CEDUMP_TYPE *pceDumpType, DWORD *pdwMaxDumpFileSize, BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!Initialize: Enter\r\n"));

    // Initialise global dump parameters to current process and thread
    g_pDmpThread = pCurThread;
    g_pDmpProc = GetPCB()->pCurPrc;

    g_hostCapture = fOnTheFly;

    if (fOnTheFly)
    {
        // This indicates the max we can write out, in this case the max size to send back to host
        g_dwReservedMemorySize = *pdwMaxDumpFileSize;
    }
    else
    {
        hRes = ReadSettings(pceDumpType, pdwMaxDumpFileSize);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!Initialize: ReadSettings failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!Initialize: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    Shutdown

    Do any cleanup required.
----------------------------------------------------------------------------*/
static HRESULT Shutdown(BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!Shutdown: Enter\r\n"));

    if (fOnTheFly)
    {
        DWORD dwWriteOffset = 0;
        DWORD dwDumpFileSize = g_dwWrittenOffset;
        DWORD dwDumpFileCRC = g_dwWrittenCRC;
        
        // Reset the write offset and CRC
        g_dwWrittenOffset = 0;
        g_dwWrittenCRC = 0;

        // Reset the KDBG buffer offset
        g_dwKdpBufferOffset = 0;

        hRes = WatsonWriteData(dwWriteOffset, &dwDumpFileSize, sizeof(dwDumpFileSize), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!Shutdown: WatsonWriteData failed writing dwDumpFileSize, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwDumpFileSize);
        
        hRes = WatsonWriteData(dwWriteOffset, &dwDumpFileCRC, sizeof(dwDumpFileCRC), TRUE);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!Shutdown: WatsonWriteData failed writing dwDumpFileCRC, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(dwDumpFileCRC);
        
        // Write the CRC to make sure the settings are valid
        hRes = WatsonWriteData(dwWriteOffset, &g_dwWrittenCRC, sizeof(DWORD), FALSE /* Don't calculate CRC */);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!Shutdown: WatsonWriteData failed writing CRC, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
        dwWriteOffset += sizeof(DWORD);

        g_dwKdpBufferSize = dwWriteOffset;  // Amount of result data to return to host
    }
    else
    {
        g_watsonDumpSettings.dwDumpWritten = g_dwWrittenOffset;
        g_watsonDumpSettings.dwDumpFileCRC = g_dwWrittenCRC;

        hRes = WriteSettings();
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!Shutdown: WriteSettings failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!Shutdown: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


#define CAPTUREDUMPFILEFNLENGTH 0x200

// RaiseException(STATUS_CRASH_DUMP, ...) is used to programmatically capture dump files.
// This macro makes sure this exception was raised by CaptureDumpFileOnDevice.
// Make sure the extent of the ExceptionAddress includes the whole functin for worst
// case condition (MIPSIV Debug), i.e. (DWORD)pCaptureDumpFileOnDevice + 0x200
BOOL WasCaptureDumpFileOnDeviceCalled(EXCEPTION_RECORD *pExceptionRecord)
{
    if (pExceptionRecord->ExceptionCode != STATUS_CRASH_DUMP)
        return FALSE;

    if (pExceptionRecord->NumberParameters != 5)
        return FALSE;

    if ((DWORD)pExceptionRecord->ExceptionInformation[4] != (DWORD)*ppCaptureDumpFileOnDevice &&
        (DWORD)pExceptionRecord->ExceptionInformation[4] != (DWORD)*ppKCaptureDumpFileOnDevice)
        return FALSE;

    if (((DWORD)*ppCaptureDumpFileOnDevice <= (DWORD)pExceptionRecord->ExceptionAddress) &&
        ((DWORD)pExceptionRecord->ExceptionAddress <= ((DWORD)*ppCaptureDumpFileOnDevice + CAPTUREDUMPFILEFNLENGTH)))
        return TRUE;

    if (((DWORD)*ppKCaptureDumpFileOnDevice <= (DWORD)pExceptionRecord->ExceptionAddress) &&
        ((DWORD)pExceptionRecord->ExceptionAddress <= ((DWORD)*ppKCaptureDumpFileOnDevice + CAPTUREDUMPFILEFNLENGTH)))
        return TRUE;

    return FALSE;
}


HRESULT DoCapture(CEDUMP_TYPE ceDumpType, DWORD dwMaxDumpFileSize)
{
    HRESULT hr;
    
    __try
    {
        if (OEMKDIoControl != NULL)
        {
            // Let KDIoControl know dump is about to start
            KCall((PKFN)OEMKDIoControl, KD_IOCTL_DMPGEN_START, NULL, g_dwReservedMemorySize);
        }


        // Set the process ID and Thread ID for dumping
        // This function may change the state of g_fCaptureDumpFileOnDeviceCalled
        hr = GetCaptureDumpFileParameters(&DD_ExceptionState);
        if (SUCCEEDED(hr))
        {
            hr = WriteDumpFile(ceDumpType, dwMaxDumpFileSize, FALSE /* fOnTheFly */);
            if (SUCCEEDED(hr))
            {
                hr = Shutdown(FALSE /* fOnTheFly */);
                if (FAILED(hr))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: Shutdown failed, hRes=0x%08X\r\n", hr));
                }
            }
            else
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: WriteDumpFile failed, hRes=0x%08X\r\n", hr));
            }
        }
        else
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: GetCaptureDumpFileParameters failed, hRes=0x%08X\r\n", hr));
        }


        if (OEMKDIoControl != NULL)
        {
            KCall((PKFN)OEMKDIoControl, KD_IOCTL_DMPGEN_END, NULL, hr);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFile: Exception occured capturing dump file, quiting ...\r\n"));
        hr = E_FAIL;
    }

    return hr;
}



/*----------------------------------------------------------------------------
    CaptureDumpFile

    Capture dump file to persitent storage
----------------------------------------------------------------------------*/
BOOL CaptureDumpFile(PEXCEPTION_RECORD pExceptionRecord, PCONTEXT pContextRecord, BOOL fSecondChance, BOOL *pfIgnoreOnContinue)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMaxDumpFileSize = 0;
    CEDUMP_TYPE ceDumpType = ceDumpTypeUndefined;
    BOOL fPassedInterLock = FALSE;
    BOOL fDiagnosis = FALSE;

    KDASSERT(pExceptionRecord);
    KDASSERT(pContextRecord);

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!CaptureDumpFile: Enter\r\n"));

    PTHREAD pCurTh = pCurThread;
    
    // This must be first check, otherwise g_lThreadsInPostMortemCapture will get out of sync
    if (InterlockedIncrement(&g_lThreadsInPostMortemCapture) > 1)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: Another thread already running Post Portem Capture code (Threads = %u)\r\n", g_lThreadsInPostMortemCapture));
        hRes = E_FAIL;
        goto decrement_and_exit;
    }

    // Indicate we passed the InterlockedIncrement check
    fPassedInterLock = TRUE;

    // Global variable initialization must be done after the InterlockedIncrement to prevent changing by re-entrancy

    // Init to no error, will be set during processing
    g_dwLastError = ERROR_SUCCESS;

    if(fSecondChance == FALSE)
    {
        // First chance exception: we know this isn't the dup SEH resulting from
        // a crash in a PSL server being bubbled up, so we now allow dumps to be  
        // generated for this thread
        pCurTh->tlsPtr[TLSSLOT_KERNEL] &= ~TLSKERN_CAPTURED_PSL_CALL;
    }

    // Check if we've already captured the root of this exception in a PSL server somewhere
    if((pCurTh->tlsPtr[TLSSLOT_KERNEL] & TLSKERN_CAPTURED_PSL_CALL) != 0)
    {
        hRes = E_FAIL;
        goto decrement_and_exit;
    }

    OsAxsPushExceptionState(pExceptionRecord, pContextRecord, fSecondChance);

    // Don't create dumps for debug breaks
    if (STATUS_BREAKPOINT == pExceptionRecord->ExceptionCode)
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: Breakpoint exception @0x%08X, exiting ...\r\n", CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context)));
        hRes = E_FAIL;
        goto popstate_and_exit;
    }

    hRes = Initialize(&ceDumpType, &dwMaxDumpFileSize, FALSE /* fOnTheFly */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!CaptureDumpFile: Initialize failed, hRes=0x%08X\r\n", hRes));
        goto popstate_and_exit;
    }

    // Indicate if CaptureDumpFileOnDevice API called
    g_fCaptureDumpFileOnDeviceCalled = WasCaptureDumpFileOnDeviceCalled(pExceptionRecord);


    BOOL fIsDDxBreak = IsDDxBreak();

    // Handle DDX breaks
    if ((fIsDDxBreak || fSecondChance) && IsDDxEnabled()) 
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE,(L"  DwDmpGen!CaptureDumpFile: FOUND DDX BREAK\r\n"));
        
        if (Diagnose(pExceptionRecord->ExceptionCode))
        {
            fDiagnosis = TRUE;
            DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFile: ------- DDx POSITIVE DIAGNOSIS ---------------\r\n"));
        }
        else
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DwDmpGen!CaptureDumpFile: DDx exception @0x%08x, exiting ...\r\n", CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context)));
            hRes = E_FAIL;
            goto popstate_and_exit;
        }
    }
        

    if (!fSecondChance)
    {
        // 1st chance exception

        // Continue execution if this exception has been generated by Watson
        if (fIsDDxBreak || 
            fDiagnosis  ||
            g_fCaptureDumpFileOnDeviceCalled)
        {
            *pfIgnoreOnContinue = TRUE;
        }

        // Do not capture a dump unless we have a diagnosis or the Watson settings call for
        // a dump on first chance exceptions
        if (!fDiagnosis &&
            !g_fCaptureDumpFileOnDeviceCalled &&
            !g_watsonDumpSettings.fDumpFirstChance)
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: First chance exception, exiting ...\r\n"));
            hRes = E_FAIL;
            goto popstate_and_exit;
        }
    }
    else
    {
        // 2nd chance exception
        if (g_fCaptureDumpFileOnDeviceCalled)
        {
            // We should not get a second chance exception when calling CaptureDumpFileOnDevice
            KD_ASSERT(!g_fCaptureDumpFileOnDeviceCalled /* Should be false when fSecondChance==TRUE */);

            // Treat a 2nd chance in CaptureDumpFileOnDevice like a normal crash
            g_fCaptureDumpFileOnDeviceCalled = FALSE;
        }
        else if (g_watsonDumpSettings.fDumpFirstChance)
        {
            // 2nd chance exception with 1st chance exception dump enabled
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: Second chance exception with first chance dump enabled, exiting ...\r\n"));
            hRes = E_FAIL;
            goto popstate_and_exit;
        }
    }

    // At this point, fairly certain that dump is about to happen. Trigger a cache flush
    // to make sure that all aliased memory is properly propagated.
    // This is especially important on ARM, where most ARM hardware is running with
    // a virtual tagged cache and it's easy to get interesting problems.
    FlushCache();

    DD_CaptureExtraContext();
    

    // Make sure we don't have stale DDx info if we are capturing a dump file without 
    // a positive diagnosis

    if (!fDiagnosis)
        ResetDDx();
    
    hRes = DoCapture(ceDumpType, dwMaxDumpFileSize);

    if (SUCCEEDED(hRes))
    {
        DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFile: Dump File captured in reserved memory\r\n"));
        if (g_hEventDumpFileReady)
        {
            // Tell DwXfer.dll that a dump file is ready
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: Set 'Dump File Ready' Event\r\n"));

            // Need to switch to NK.EXE to make sure the right handle table is used for set event.
            SafeSetEvent(g_hEventDumpFileReady);
        }
    }

    if(g_fCaptureDumpFileOnDeviceCalled == TRUE && FAILED(hRes))
    {
        // If CaptureDumpFileOnDevice was called (and failed), we don't set
        // IgnoreOnContinue to TRUE.  This variable is what tells the kernel 
        // exception dispatcher (exdsptch.c) whether or not we've handled the 
        // exception.  Leaving it as FALSE causes the API to return FALSE to the 
        // caller from coredll.
        *pfIgnoreOnContinue = FALSE;
    }

    hRes = S_OK;

popstate_and_exit:
    OsAxsPopExceptionState();
decrement_and_exit:
    InterlockedDecrement(&g_lThreadsInPostMortemCapture);

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!CaptureDumpFile: Leave, hRes=0x%08X\r\n", hRes));

    // Check for errors when using CaptureDumpFileOnDevice API
    if ((g_fCaptureDumpFileOnDeviceCalled || g_fReportFaultCalled) && *pfIgnoreOnContinue && (hRes != S_OK))
    {
        if (ERROR_SUCCESS == g_dwLastError)
        {
            // If Last Error not already set, then set last error according to hRes
            switch (hRes)
            {
                case E_INVALIDARG:   g_dwLastError = ERROR_BAD_ARGUMENTS;  break;
                case E_OUTOFMEMORY:  g_dwLastError = ERROR_OUTOFMEMORY;    break;
                case E_ACCESSDENIED: g_dwLastError = ERROR_ACCESS_DENIED;  break;
                
                default:
                    g_dwLastError = ERROR_GEN_FAILURE;
                    break;
            }
        }
        SetLastError(g_dwLastError);
        DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFile: Error detected during capture, hRes=0x%08X, SetLastError=%u\r\n", hRes, g_dwLastError));
    }

    g_fStackBufferOverrun = FALSE;
    g_fCaptureDumpFileOnDeviceCalled = FALSE;
    g_fReportFaultCalled = FALSE;

    return SUCCEEDED(hRes);
}


/*++

Routine Name:

    CaptureDumpFileOnTheFly

Routine Description:

    Capture Dump File On-The-Fly request and serialize the via the caller specified buffer.

Arguments:

    pWatsonOtfRequest                - IN Request structure
    pbOutBuffer                      - IN/OUT The response buffer.
    pdwBufferSize                    - IN/OUT The size of the buffer / The size consumed by response.
    pKdpSendOsAxsResponse            - IN pointer to KDBG response sending function for serialized response

Return Value:

    S_OK : success,
    E_FAIL : general failure,
    E_INVALIDARG : Invalid argument to function

--*/

HRESULT CaptureDumpFileOnTheFly (OSAXS_API_WATSON_DUMP_OTF *pWatsonOtfRequest, BYTE *pbOutBuffer, DWORD *pdwBufferSize)
{
    HRESULT hRes = E_FAIL;
    CEDUMP_TYPE ceDumpType = ceDumpTypeUndefined;
    DWORD dwMaxDumpFileSize = 0;
    BOOL passedPmdGuard = FALSE;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"++DwDmpGen!CaptureDumpFileOnTheFly: ReplyBufSize = %u\r\n", pdwBufferSize ? *pdwBufferSize : 0));
 
    // Even though CaptureDumpFileOnTheFly is only invoked when interactively debugging, technically
    // there is a thread now in Post Mortem Capture.
    if (InterlockedIncrement(&g_lThreadsInPostMortemCapture) > 1)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFileOnTheFly: Another thread already running Post Portem Capture code (Threads = %u)\r\n", g_lThreadsInPostMortemCapture));
        hRes = E_FAIL;
        goto Exit;
    }

    if (!pWatsonOtfRequest || !pbOutBuffer || !pdwBufferSize || !(*pdwBufferSize))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFileOnTheFly: Invalid parameter\r\n"));
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if ((g_fStackBufferOverrun) ||
        (g_fCaptureDumpFileOnDeviceCalled) ||
        (g_fReportFaultCalled))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFileOnTheFly: Can not capture dump file on-the-fly while device capture in progress!\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    passedPmdGuard = TRUE;

    ceDumpType = (CEDUMP_TYPE)pWatsonOtfRequest->dwDumpType;
    dwMaxDumpFileSize = (pWatsonOtfRequest->dw64MaxDumpFileSize > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD) pWatsonOtfRequest->dw64MaxDumpFileSize;
    g_pKdpBuffer = pbOutBuffer;
    g_dwKdpBufferSize = *pdwBufferSize;
    g_dwKdpBufferOffset = 0;
    g_dwSegmentIdx = 0;

    hRes = Initialize(&ceDumpType, &dwMaxDumpFileSize, TRUE /* fOnTheFly */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!CaptureDumpFileOnTheFly: Initialize failed, hRes=0x%08X\r\n", hRes));
        hRes = E_FAIL;
        goto Exit;
    }

    // Write the dump file to host via g_pKdpSendOsAxsResponse
    hRes = WriteDumpFile(ceDumpType, dwMaxDumpFileSize, TRUE /* fOnTheFly */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFileOnTheFly: WriteDumpFile failed, hRes=0x%08X\r\n", hRes));
        hRes = E_FAIL;
        goto Exit;
    }

    // The dump file size should be the same as current written offset
    KD_ASSERT(g_dwDumpFileSize == g_dwWrittenOffset);

    pWatsonOtfRequest->dw64MaxDumpFileSize = g_dwDumpFileSize;

    // Will place some result data at g_pKdpBuffer and set g_dwKdpBufferSize to the size of data
    hRes = Shutdown(TRUE /* fOnTheFly */);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFileOnTheFly: Shutdown failed, hRes=0x%08X\r\n", hRes));
        hRes = E_FAIL;
        goto Exit;
    }

    pWatsonOtfRequest->dw64SegmentIndex = g_dwSegmentIdx; // Should be Unchanged
    pWatsonOtfRequest->fDumpfileComplete = TRUE;
    pWatsonOtfRequest->dwSegmentBufferSize = g_dwKdpBufferSize;

    DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFileOnTheFly: Dump File transfer complete, %u bytes\r\n", g_dwDumpFileSize));

    *pdwBufferSize = g_dwKdpBufferSize; // Return result data

Exit:

    if (passedPmdGuard)
    {
        g_pKdpBuffer = NULL;
        g_dwKdpBufferSize = 0;
    }

    InterlockedDecrement(&g_lThreadsInPostMortemCapture);
    
    DEBUGGERMSG (OXZONE_DWDMPGEN || FAILED (hRes), (L"--DwDmpGen!CaptureDumpFileOnTheFly: hRes=0x%08x\r\n", hRes));
    return hRes;
}
