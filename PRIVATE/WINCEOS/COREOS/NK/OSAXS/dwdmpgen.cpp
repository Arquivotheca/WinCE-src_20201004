//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <bldver.h>
#include "osaxs_p.h"
//#include "kdp.h"

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

#define STACK_FRAMES_BUFFERED  20
#define MAX_STREAMS (ceStreamBucketParameters - ceStreamNull)
#define DISABLEFAULTS()        (UTlsPtr()[TLSSLOT_KERNEL] |= (TLSKERN_NOFAULTMSG|TLSKERN_NOFAULT))
#define ENABLEFAULTS()         (UTlsPtr()[TLSSLOT_KERNEL] &= ~(TLSKERN_NOFAULTMSG|TLSKERN_NOFAULT))

#define SET_FRAME_COUNT(ThreadCount, FrameCount) \
    {if (ThreadCount < dwMaxThreadEntries) (*((WORD*)g_bBuffer + ThreadCount) = (WORD)FrameCount);} 
#define CHECK_FRAME_COUNT(ThreadCount, FrameCount) \
    ((ThreadCount < dwMaxThreadEntries) ? (*((WORD*)g_bBuffer + ThreadCount) == (WORD)FrameCount) : TRUE)
#define GET_FRAME_COUNT(ThreadCount) \
    ((ThreadCount < dwMaxThreadEntries) ? (*((WORD*)g_bBuffer + ThreadCount)) : -1) 

#define VIRTUAL_MEMORY  TRUE
#define PHYSICAL_MEMORY FALSE

typedef struct _MEMORY_BLOCK 
{
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
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
DWORD g_dwLastError;
BOOL  g_fFirstChanceException;

PTHREAD g_pDmpThread = NULL;
PPROCESS g_pDmpProc = NULL;

PEXCEPTION_RECORD g_pExceptionRecord = NULL;
PCONTEXT          g_pContextException = NULL;

HANDLE g_hEventDumpFileReady = NULL;

WCHAR g_wzOEMString[MAX_PATH];
DWORD g_dwOEMStringSize = 0;
WCHAR g_wzPlatformType[MAX_PATH];
DWORD g_dwPlatformTypeSize = 0;
PLATFORMVERSION g_platformVersion[4];
DWORD g_dwPlatformVersionSize = 0;

typedef struct VERHEAD
{
    WORD wTotLen;
    WORD wValLen;
    WORD wType;         /* always 0 */
    WCHAR szKey[(sizeof("VS_VERSION_INFO")+3)&~03];
    VS_FIXEDFILEINFO vsf;
} VERHEAD ;

typedef struct resroot_t 
{
    DWORD flags;
    DWORD timestamp;
    DWORD version;
    WORD  numnameents;
    WORD  numidents;
} resroot_t;

typedef struct resent_t 
{
    DWORD id;
    DWORD rva;
} resent_t;

typedef struct resdata_t 
{
    DWORD rva;
    DWORD size;
    DWORD codepage;
    DWORD unused;
} resdata_t;

#define DWORDUP(x) (((x)+3)&~03)

BOOL WINAPI EventModify(HANDLE hEvent, DWORD func)
{
    // Used by SetEvent, ResetEvent, and PulseEvent
    return g_OsaxsData.pEventModify(hEvent, func);
}

VOID* GetObjectPtrByType(HANDLE h, int type)
{
    return g_OsaxsData.pfnGetObjectPtrByType(h, type);
}

/*----------------------------------------------------------------------------
    WatsonReadData

    Reads data from the input stream, either reserved memory or KDBG.
----------------------------------------------------------------------------*/
HRESULT WatsonReadData(DWORD dwOffset, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    HRESULT hRes = E_FAIL;
    DWORD dwCRCLoop;
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WatsonReadData: Enter\r\n"));

    KD_ASSERT((dwOffset + dwDataSize) <= g_dwReservedMemorySize);
    if ((dwOffset + dwDataSize) > g_dwReservedMemorySize)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WatsonReadData: Trying to read beyond size of reserved memory, Reserved Size=0x%08X, dwOffset=0x%08X, dwDataSize=0x%08X\r\n", g_dwReservedMemorySize, dwOffset, dwDataSize));
        hRes = E_OUTOFMEMORY; 
        goto Exit;
    }
    
    if (pfnNKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_READWATSON, NULL, dwOffset, pDataBuffer, dwDataSize, NULL) != dwDataSize)
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
HRESULT WatsonWriteData(DWORD dwOffset, PVOID pDataBuffer, DWORD dwDataSize, BOOL fCalculateCRC)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteLoop;
    BYTE  bDataOut;
    BOOL  fExceptionEncountered = FALSE;
    DWORD dwWriteLoopError;
    DWORD dwExceptionCount = 0;
    PVOID pAddr;
    
    if ((dwOffset + dwDataSize) > g_dwReservedMemorySize)
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

#ifdef DEBUG        
        // At this point we should only have valid addresses that are currently paged in
        if ((0 == dwWriteLoop) || ((DWORD)pAddr & (PAGE_SIZE-1)))
        {
            // Validate the page is paged in each time we move to a new page
            KD_ASSERT(NULL != SafeDbgVerify(pAddr, TRUE /* Probe Only */));
        }
#endif // DEBUG
        
        // Just in case we may be reading from invalid memory, catch possible exceptions

        // Prevent hitting exceptions in the debugger
        DISABLEFAULTS();
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
        ENABLEFAULTS();

        // Write out byte of memory
        if (pfnNKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_WRITEWATSON, &bDataOut, 1, NULL, (dwOffset+dwWriteLoop), NULL) != 1)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: KernelLibIoControl failed, pDataBuffer=0x%08X, dwOffset=0x%08X, dwWriteLoop=0x%08X\r\n", pDataBuffer, dwOffset, dwWriteLoop));
            hRes = E_FAIL; 
            goto Exit;
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
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WatsonWriteData: Exception encountered while reading memory, pDataBuffer=0x%08X, First Exception Offset=0x%08X, Exception Count=%u\r\n", 
                                  pDataBuffer, dwWriteLoopError, dwExceptionCount));
    }
        
    hRes = S_OK;

Exit:

    return hRes;
}

//------------------------------------------------------------------------------
// Find type entry by number
//------------------------------------------------------------------------------
DWORD FindTypeByNum(LPBYTE curr, DWORD id) 
{
    DWORD dwRva = 0;
    resroot_t *rootptr;
    resent_t *resptr;
    int count;
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!FindTypeByNum: Enter\r\n"));

    rootptr = (resroot_t *)curr;
    resptr = (resent_t *)(curr + sizeof(resroot_t) +
                          rootptr->numnameents*sizeof(resent_t));
    count = rootptr->numidents;

    while (count--)
    {
        if (!id || (resptr->id == id))
        {
            KD_ASSERT (!IsSecureVa(resptr->rva));
            dwRva = resptr->rva;
            break;
        }
        resptr++;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!FindTypeByNum: Leave, dwRva=0x%08X\r\n", dwRva));

    return dwRva;
}

//------------------------------------------------------------------------------
// Find first entry (from curr pointer)
//------------------------------------------------------------------------------
DWORD FindFirst(LPBYTE curr) 
{
    DWORD dwRva = 0;
    resroot_t *rootptr;
    resent_t *resptr;
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!FindFirst: Enter\r\n"));

    rootptr = (resroot_t *)curr;
    resptr = (resent_t *)(curr + sizeof(resroot_t) +
                          rootptr->numnameents*sizeof(resent_t));

    dwRva = resptr->rva;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!FindFirst: Leave, dwRva=0x%08X\r\n", dwRva));

    return dwRva;
}

//------------------------------------------------------------------------------
// Extract a resource
//------------------------------------------------------------------------------
LPVOID ExtractOneResource(
    LPBYTE lpres,
    LPCWSTR lpszName,
    LPCWSTR lpszType,
    DWORD o32rva,
    LPDWORD pdwSize
    ) 
{
    DWORD trva;
    LPVOID lpData = NULL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!ExtractOneResource: Enter\r\n"));

    if (!(trva = FindTypeByNum(lpres, (DWORD)lpszType)) ||
       !(trva = FindTypeByNum(lpres+(trva&0x7fffffff),(DWORD)lpszName)) ||
       !(trva = FindFirst(lpres+(trva&0x7fffffff))) ||
       (trva & 0x80000000))
    {
        lpData = NULL;
        goto Exit;
    }
    trva = (ULONG)(lpres + trva);

    if (pdwSize)
        *pdwSize = ((resdata_t *)trva)->size;
    lpData =  (lpres+((resdata_t *)trva)->rva-o32rva);

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!ExtractOneResource: Leave, lpData=0x%08X\r\n", lpData));
    return lpData;
}


/*----------------------------------------------------------------------------
    GetVersionInfo

    Get version info for a process or module.
----------------------------------------------------------------------------*/
HRESULT GetVersionInfo(HANDLE hModule, VS_FIXEDFILEINFO **ppvsFixedInfo)
{
    HRESULT  hRes = E_FAIL;
    int      loop;
    BOOL     fVersionInfoFound = FALSE;
    DWORD    o32rva;
    LPBYTE   lpres = NULL;
    VERHEAD *pVerHead = NULL;
    LPWSTR   lpStart;
    DWORD    dwTemp;
    LPBYTE   BasePtr;
    e32_lite *pe32l = NULL;
    o32_lite *po32_ptr;
    PPROCESS pProc;
    PMODULE  pMod = NULL;
    PPROCESS pProcArray;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!GetVersionInfo: Enter\r\n"));
    
    pProcArray = FetchProcArray();
    if (NULL == pProcArray)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetVersionInfo: FetchProcArray returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    if (pProc = HandleToProc(hModule))
    { // a process
        PMODULE pCoreDll = (PMODULE)hCoreDll;
        DWORD dwInUse = (1 << pProc->procnum);
        
        if (!pCoreDll || !(dwInUse & pCoreDll->inuse))
        {
            // When process is starting or dying we can not access the o32_ptr, since it may be pointing at invalid memory
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetVersionInfo: Process ID 0x%08X (%s) not using CoreDll.dll, may be in startup or shutdown.\r\n", pProc->hProc, pProc->lpszProcName));
            hRes = E_FAIL;
            goto Exit;
        }
        
        BasePtr = (LPBYTE)MapPtrProc(pProc->BasePtr, pProc);
        pe32l = &pProc->e32;
    }
    else if ((pMod = (LPMODULE)hModule)->lpSelf == pMod)
    { // a module
        if (!pMod->inuse)
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetVersionInfo: Inuse flag for module not set, returning ...\r\n"));
            hRes = E_FAIL;
            goto Exit;
        }
        
        BasePtr = (LPBYTE)MapPtrProc(pMod->BasePtr, &pProcArray[0]);
        pe32l = &pMod->e32;
    }

    if (pe32l && pe32l->e32_unit[RES].rva)
    {
        if (pMod && pMod->rwHigh)
        {
            // need to find the right section if RO/RW section are split
            po32_ptr = pMod->o32_ptr;
            for (loop = 0; loop < pe32l->e32_objcnt; loop++)
            {
                if ((po32_ptr) && (po32_ptr[loop].o32_rva == pe32l->e32_unit[RES].rva) && (po32_ptr[loop].o32_psize))
                {
                    if ((po32_ptr[loop].o32_flags & IMAGE_SCN_COMPRESSED))
                    {
                        // if compressed, fail
                        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetVersionInfo: Version resource is compressed\r\n"));
                        hRes = E_FAIL;
                        goto Exit;
                    }
                    o32rva = po32_ptr[loop].o32_rva;
                    lpres = (LPBYTE) (po32_ptr[loop].o32_realaddr);
                    break;
                }
            }
        }
        else 
        {
            o32rva = pe32l->e32_unit[RES].rva;
            lpres = BasePtr + pe32l->e32_unit[RES].rva;
        }

        if (lpres)
        {
            pVerHead = (VERHEAD *)ExtractOneResource(lpres, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO, o32rva, &dwTemp);
            if (pVerHead)
            {
                if (((DWORD)pVerHead->wTotLen > dwTemp) || (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetVersionInfo: Bad Version resource data\r\n"));
                    hRes = E_FAIL;
                    goto Exit;
                }
                else
                {
                    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetVersionInfo: Ver Len=%u, Ver Key=%s\r\n", pVerHead->wTotLen, pVerHead->szKey));
                    lpStart = (LPWSTR)((LPBYTE)pVerHead+DWORDUP((sizeof(VERBLOCK)-sizeof(WCHAR))+(kdbgwcslen(pVerHead->szKey)+1)*sizeof(WCHAR)));
                    *ppvsFixedInfo = (VS_FIXEDFILEINFO*)(lpStart < (LPWSTR)((LPBYTE)pVerHead+pVerHead->wTotLen) ? lpStart : (LPWSTR)(pVerHead->szKey+kdbgwcslen(pVerHead->szKey)));
                    fVersionInfoFound = TRUE;
                }
            }
        }
    }

    if (!fVersionInfoFound)
    {
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetVersionInfo: No Version Info\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!GetVersionInfo: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    GetBucketParameters

    Get the bucket parameters from exception information.
----------------------------------------------------------------------------*/
HRESULT GetBucketParameters(PCEDUMP_BUCKET_PARAMETERS pceDmpBucketParameters)
{
    HRESULT hRes = E_FAIL;

    DWORD dwPC, dwExcAddr, dwPCSlot, dwPCOriginal;
    DWORD dwImageBase, dwImageSize;
    PPROCESS pProcess, pProcessOriginal;
    PTHREAD  pThread;
    PMODULE  pMod;
    BOOL fFoundMod = FALSE;
    VS_FIXEDFILEINFO * pvsFixedInfo = NULL;
    KDataStruct *pKData;
    PPROCESS pProcArray;
    DWORD dwProcessNumber;
    CallSnapshotEx csexFrames[STACK_FRAMES_BUFFERED] = {0};
    DWORD dwFramesReturned, dwFrameCount;
    BOOL  fDifferentPCorProcess = FALSE;
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!GetBucketParameters: Enter\r\n"));

    pProcArray = FetchProcArray();
    if (NULL == pProcArray)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: FetchProcArray returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pKData = FetchKData ();
    if (NULL == pKData)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: FetchKData returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pceDmpBucketParameters->SizeOfHeader = sizeof(CEDUMP_BUCKET_PARAMETERS);
    if (g_fCaptureDumpFileOnDeviceCalled)
    {
        pceDmpBucketParameters->EventType = (DWORD)WATSON_SNAPSHOT_EVENT;
    }
    else
    {
        pceDmpBucketParameters->EventType = (DWORD)WATSON_EXCEPTION_EVENT;
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: hCurProc=0x%08X, hCurThread=0x%08X, hOwnerProc=0x%08X\r\n",
                                hCurProc, hCurThread, (DWORD)((PROCESS *)(pCurThread->pOwnerProc)->hProc)));

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: hDmpProc=0x%08X, hDmpThread=0x%08X, hOwnerProc=0x%08X\r\n",
                                g_pDmpProc->hProc, g_pDmpThread->hTh, (DWORD)((PROCESS *)(g_pDmpThread->pOwnerProc)->hProc)));

    pProcessOriginal = pProcess = g_pDmpProc;
    pThread = g_pDmpThread;

    if (pThread == pCurThread)
    {
        // Default to the Exception PC
        dwPCOriginal = dwPC = CONTEXT_TO_PROGRAM_COUNTER (g_pContextException);
        dwExcAddr = (DWORD) g_pExceptionRecord->ExceptionAddress;

#ifdef DEBUG
        if ((dwExcAddr < dwPC) || (dwExcAddr > (dwPC+4)))
        {
            DEBUGGERMSG (OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: WARNING: Exception frame PC (0x%08X) is different than Exception address (0x%08X).\r\n", dwPC, dwExcAddr));
        }
#endif // DEBUG
        
        // Check if CaptureDumpFileOnDevice API called for current thread
        if (g_fCaptureDumpFileOnDeviceCalled)
        {
            // Unwind two frames from point of exception (CaptureDumpFileOnDevice API + Calling function)
            dwFramesReturned = GetCallStack(pThread->hTh, 2, csexFrames, STACKSNAP_EXTENDED_INFO, 0);
            if (dwFramesReturned != 2)
            {
                DEBUGGERMSG (OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: GetCallStack did not unwind 2 frames for CaptureDumpFileOnDevice API, dwFramesReturned=%u.\r\n", dwFramesReturned));
                // Don't fail, just go with the exception PC
            }
            else
            {
                // Get the PC and process of the frame calling CaptureDumpFileOnDevice API
                // this will provide bucketing data for the module that called the API
                dwFrameCount = 1;
                dwPC = csexFrames[dwFrameCount].dwReturnAddr;
                pProcess = HandleToProc((HANDLE)csexFrames[dwFrameCount].dwCurProc);
                ++ dwFrameCount;
            }
        }
    }
    else
    {
        // We should only get here when CaptureDumpFileOnDevice API is called for another thread
        KD_ASSERT(g_fCaptureDumpFileOnDeviceCalled);

        dwPCOriginal = dwPC = GetThreadIP(pThread);

        // Check if PC in Nk.exe (Process 0) & Nk.exe not owner of thread
        // If PC is in Nk.exe then we try unwind out of NK.exe & Coredll.dll
        // This should give us better bucket parameters, otherwise we always get Nk.EXE + KCall offset for buckets
        dwImageBase = (DWORD) pProcArray[0].BasePtr;
        if (dwImageBase < 0x01FFFFFF)
        {
            dwImageBase |= pProcArray[0].dwVMBase;
        }
        dwImageSize = pProcArray[0].e32.e32_vsize;

        if ((dwImageBase <= dwPC) && (dwPC < (dwImageBase + dwImageSize)) && (pThread->pOwnerProc != &pProcArray[0]))
        {
            // Unwind frames from current PC 
            dwFramesReturned = GetCallStack(pThread->hTh, STACK_FRAMES_BUFFERED, csexFrames, STACKSNAP_EXTENDED_INFO, 0);
            if (dwFramesReturned <= 0)
            {
                DEBUGGERMSG (OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: GetCallStack did not unwind frames from Nk.exe, dwFramesReturned=%u.\r\n", dwFramesReturned));
                // Don't fail, just go with the NK.exe PC
            }
            else
            {
                BOOL fFramesLeft = TRUE;

                // Walk down frames until we are out of nk.exe (Process 0)
                
                // Start at top frame
                dwFrameCount = 0;
                
                while ((dwImageBase <= dwPC) && (dwPC < (dwImageBase + dwImageSize)) && (fFramesLeft))
                {
                    if (dwFrameCount < dwFramesReturned)
                    {
                        dwPC = csexFrames[dwFrameCount].dwReturnAddr;
                        pProcess = HandleToProc((HANDLE)csexFrames[dwFrameCount].dwCurProc);
                        ++ dwFrameCount;
                    }
                    else
                    {
                        fFramesLeft = FALSE;
                    }
                }

                // Now Walk down frames until we are out of coredll.dll 
                // If we don't have any in coredll.dll, then we still continue
                // since coredll.dll may not always be in the call stack, 
                // especially with retail x86.
                if (fFramesLeft)
                {
                    pMod = (MODULE *)pKData->aInfo[KINX_MODULES];
                    
                    while (pMod && (pMod->lpSelf == pMod))
                    {
                        if (kdbgwcsnicmp(pMod->lpszModName, L"coredll.dll", kdbgwcslen(pMod->lpszModName)) == 0)
                        {
                            // Coredll.dll found
                            dwImageBase = (DWORD) ZeroPtr(pMod->BasePtr);
                            dwImageSize = (DWORD) pMod->e32.e32_vsize;
                            dwPCSlot = ZeroPtr(dwPC);

                            while ((dwImageBase <= dwPCSlot) && (dwPCSlot < (dwImageBase + dwImageSize)) && (fFramesLeft))
                            {
                                if (dwFrameCount < dwFramesReturned)
                                {
                                    dwPC = csexFrames[dwFrameCount].dwReturnAddr;
                                    dwPCSlot = ZeroPtr(dwPC);
                                    pProcess = HandleToProc((HANDLE)csexFrames[dwFrameCount].dwCurProc);
                                    ++ dwFrameCount;
                                }
                                else
                                {
                                    fFramesLeft = FALSE;
                                }
                            }
                            break;
                        }
                        pMod = pMod->pMod;
                    }
                }

                if (!fFramesLeft)
                {
                    // Ran out of frames, in which case revert to the original PC & Process
                    DEBUGGERMSG (OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: WARNING: Walking out of Nk.exe walked to many frames, reverting to orignal PC, dwFrameCount=%u.\r\n", dwFrameCount));
                    dwPC = dwPCOriginal;
                    pProcess = pProcessOriginal;
                }
            }
        }
    }

    if ((dwPC != dwPCOriginal) || (pProcess != pProcessOriginal))
    {
        DEBUGGERMSG (OXZONE_DWDMPGEN, (L"  DwDmpGen!GetBucketParameters: CaptureDumpFileOnDevice API called, using different PC or Process\r\n"));
        DEBUGGERMSG (OXZONE_DWDMPGEN, (L"  DwDmpGen!GetBucketParameters: New PC=0x%08X, Original PC=0x%08X, New Process=0x%08X, Original Process=0x%08X\r\n",
                                      dwPC, dwPCOriginal, (pProcess ? pProcess->hProc : 0), pProcessOriginal->hProc));
        fDifferentPCorProcess = TRUE;
    }

    while (!fFoundMod)
    {
        if (dwPC && pProcess)
        {
            dwPC = (DWORD) MapPtrProc (dwPC, pProcess);
            dwPCSlot = ZeroPtr(dwPC);

            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: PC=0x%08X, ZeroPtr PC=0x%08X\r\n", dwPC, dwPCSlot));

            // Get the bucket parameters for the Application
            pceDmpBucketParameters->AppName  = (DWORD)pProcess->lpszProcName;
            pceDmpBucketParameters->AppStamp = pProcess->e32.e32_timestamp;

            hRes = GetVersionInfo(pProcess->hProc, &pvsFixedInfo);
            if (SUCCEEDED(hRes))
            {
                pceDmpBucketParameters->AppVerMS = pvsFixedInfo->dwFileVersionMS;
                pceDmpBucketParameters->AppVerLS = pvsFixedInfo->dwFileVersionLS;
                pceDmpBucketParameters->fDebug   = (pvsFixedInfo->dwFileFlags & VS_FF_DEBUG)?1:0;
            }
            else
            {
                pceDmpBucketParameters->AppVerMS = (pProcess->e32.e32_cevermajor << 16) | (pProcess->e32.e32_ceverminor & 0xFFFF);
                pceDmpBucketParameters->AppVerLS = 0;
                pceDmpBucketParameters->fDebug   = 0;
            }

            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: AppName=%s, AppStamp=0x%08X, AppVer=%d.%d.%d.%d, fDebug=%u\r\n", 
                                        (LPWSTR)pceDmpBucketParameters->AppName,
                                        pceDmpBucketParameters->AppStamp,
                                        (WORD)(pceDmpBucketParameters->AppVerMS >> 16),
                                        (WORD)(pceDmpBucketParameters->AppVerMS & 0xFFFF),
                                        (WORD)(pceDmpBucketParameters->AppVerLS >> 16),
                                        (WORD)(pceDmpBucketParameters->AppVerLS & 0xFFFF),
                                        pceDmpBucketParameters->fDebug));

            // Get the bucket parameters for the Module, may be in a process or a DLL
            
            // Iterate through all processes to find which one corresponds to Program Counter
            for (dwProcessNumber = 0; ((dwProcessNumber < MAX_PROCESSES) && (FALSE == fFoundMod)); dwProcessNumber++)
            {
                // Check for valid process
                if (pProcArray[dwProcessNumber].dwVMBase)
                {
                    dwImageBase = (DWORD) pProcArray[dwProcessNumber].BasePtr;
                    if (dwImageBase < 0x01FFFFFF)
                    {
                        dwImageBase |= pProcArray[dwProcessNumber].dwVMBase;
                    }
                    dwImageSize = pProcArray[dwProcessNumber].e32.e32_vsize;
            
                    if ((dwImageBase <= dwPC) && (dwPC < (dwImageBase + dwImageSize)))
                    {
                        // Correct process found
                        // Exception occured within process, so exception module is a process
                        fFoundMod = TRUE;
                        pceDmpBucketParameters->ModName  = (DWORD)pProcArray[dwProcessNumber].lpszProcName;
                        pceDmpBucketParameters->ModStamp = pProcArray[dwProcessNumber].e32.e32_timestamp;
                        pceDmpBucketParameters->Offset = dwPC - dwImageBase;

                        hRes = GetVersionInfo(pProcArray[dwProcessNumber].hProc, &pvsFixedInfo);
                        if (SUCCEEDED(hRes))
                        {
                            pceDmpBucketParameters->ModVerMS = pvsFixedInfo->dwFileVersionMS;
                            pceDmpBucketParameters->ModVerLS = pvsFixedInfo->dwFileVersionLS;
                        }
                        else
                        {
                            pceDmpBucketParameters->ModVerMS = (pProcArray[dwProcessNumber].e32.e32_cevermajor << 16) | (pProcArray[dwProcessNumber].e32.e32_ceverminor & 0xFFFF);
                            pceDmpBucketParameters->ModVerLS = 0;
                        }
                    }
                }
            }

            if (FALSE == fFoundMod)
            {
                // Exception occured in a loaded DLL, find correct module
                pMod = (MODULE *)pKData->aInfo[KINX_MODULES];

                while (pMod && (pMod->lpSelf == pMod))
                {
                    dwImageBase = (DWORD) ZeroPtr(pMod->BasePtr);
                    dwImageSize = (DWORD) pMod->e32.e32_vsize;
                    if ((dwImageBase <= dwPCSlot) && (dwPCSlot < (dwImageBase + dwImageSize)))
                    {
                        // Correct module found
                        fFoundMod = TRUE;
                        pceDmpBucketParameters->ModName  = (DWORD)pMod->lpszModName;
                        pceDmpBucketParameters->ModStamp = pMod->e32.e32_timestamp;
                        pceDmpBucketParameters->Offset = dwPCSlot - dwImageBase;
                        hRes = GetVersionInfo((HANDLE)pMod, &pvsFixedInfo);
                        if (SUCCEEDED(hRes))
                        {
                            pceDmpBucketParameters->ModVerMS = pvsFixedInfo->dwFileVersionMS;
                            pceDmpBucketParameters->ModVerLS = pvsFixedInfo->dwFileVersionLS;
                        }
                        else
                        {
                            pceDmpBucketParameters->ModVerMS = (pMod->e32.e32_cevermajor << 16) | (pMod->e32.e32_ceverminor & 0xFFFF);
                            pceDmpBucketParameters->ModVerLS = 0;
                        }
                        break;
                    }
                    pMod = pMod->pMod;
                }
            }
        }

        if (fFoundMod)
        {
            // Module found
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: ModName=%s, ModStamp=0x%08X, ModVer=%d.%d.%d.%d, Offset=0x%08X\r\n", 
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
                pProcess = pProcessOriginal;
                fDifferentPCorProcess = FALSE;
            }
            else
            {
                // Still no module found, error
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!GetBucketParameters: Could not find module where exception occured\r\n"));
                hRes = E_FAIL;
                goto Exit;
            }
        }
    }

    // Get the bucket parameters for the Owner Application
    pProcess = pThread->pOwnerProc;
    pceDmpBucketParameters->OwnerName  = (DWORD)pProcess->lpszProcName;
    pceDmpBucketParameters->OwnerStamp = pProcess->e32.e32_timestamp;
    
    hRes = GetVersionInfo(pProcess->hProc, &pvsFixedInfo);
    if (SUCCEEDED(hRes))
    {
        pceDmpBucketParameters->OwnerVerMS = pvsFixedInfo->dwFileVersionMS;
        pceDmpBucketParameters->OwnerVerLS = pvsFixedInfo->dwFileVersionLS;
    }
    else
    {
        pceDmpBucketParameters->OwnerVerMS = (pProcess->e32.e32_cevermajor << 16) | (pProcess->e32.e32_ceverminor & 0xFFFF);
        pceDmpBucketParameters->OwnerVerLS = 0;
    }
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!GetBucketParameters: OwnerName=%s, OwnerStamp=0x%08X, OwnerVer=%d.%d.%d.%d\r\n", 
                                (LPWSTR)pceDmpBucketParameters->OwnerName,
                                pceDmpBucketParameters->OwnerStamp,
                                (WORD)(pceDmpBucketParameters->OwnerVerMS >> 16),
                                (WORD)(pceDmpBucketParameters->OwnerVerMS & 0xFFFF),
                                (WORD)(pceDmpBucketParameters->OwnerVerLS >> 16),
                                (WORD)(pceDmpBucketParameters->OwnerVerLS & 0xFFFF)));
    
    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!GetBucketParameters: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    GetCaptureDumpFileParameters

    Get Parameters passed to the CaptureDumpFileOnDevice API.
----------------------------------------------------------------------------*/
HRESULT GetCaptureDumpFileParameters()
{
    HRESULT hRes = E_FAIL;
    HANDLE hDumpProc;
    HANDLE hDumpThread;
    BOOL   fTrustedApp;
    BYTE   bTrustLevel;
    PEXCEPTION_POINTERS pep;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!GetCaptureDumpFileParameters: Enter\r\n"));

    // Initialise global dump parameters to current process and thread
    g_pDmpThread = pCurThread;
    g_pDmpProc = pCurProc;

    // Check if CaptureDumpFileOnDevice API called
    if (g_fCaptureDumpFileOnDeviceCalled)
    {
        // Get the process and thread to dump 
        hDumpProc = (HANDLE)g_pExceptionRecord->ExceptionInformation[0];
        hDumpThread = (HANDLE)g_pExceptionRecord->ExceptionInformation[1];
        bTrustLevel = (BYTE)g_pExceptionRecord->ExceptionInformation[3];

        if ((hDumpProc == (HANDLE)-1) && (hDumpThread == (HANDLE)-1))
        {
            // When Process ID & Thread ID are both -1 then we are passing in exception pointers to use for the dump
            pep = (PEXCEPTION_POINTERS)g_pExceptionRecord->ExceptionInformation[2];
                        
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: API call to ReportFault detected, Exception Pointers=0x%08X\r\n",
                                      pep));
            if (NULL == pep)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Exception Pointers == NULL\r\n"));
                hRes = E_INVALIDARG;
                goto Exit;
            }

            g_pExceptionRecord = pep->ExceptionRecord;
            g_pContextException = pep->ContextRecord;
            g_fCaptureDumpFileOnDeviceCalled = FALSE;
            g_fReportFaultCalled = TRUE;
            
            hRes = S_OK;
            goto Exit;
        }

        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: API call to CaptureDumpFileOnDevice, dwProcessId=0x%08X, dwThreadId=0x%08X\r\n",
                                  hDumpProc, hDumpThread));

        KD_ASSERT(bTrustLevel == pCurProc->bTrustLevel);

        if ((KERN_TRUST_FULL != bTrustLevel) || (KERN_TRUST_FULL != pCurProc->bTrustLevel))
        {   
            // If untrusted apps call the CaptureDumpFileOnDevice API the requested process defaults to 0 (current process)
            fTrustedApp = FALSE;
            hDumpProc = 0;
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Untrusted API call to CaptureDumpFileOnDevice, dwProcessId forced to zero\r\n",
                                      hDumpProc, hDumpThread));
        }
        else
        {
            fTrustedApp = TRUE;
        }

        // Default to current process if the process ID was not set
        if (0 == hDumpProc)
        {
            hDumpProc = hCurProc;
        }

        // Validate the passed in thread ID, if set
        if (hDumpThread != 0)
        {
            g_pDmpThread = HandleToThread(hDumpThread);
            if (NULL == g_pDmpThread)
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid Thread Id passed to CaptureDumpFileOnDevice, dwThreadId=0x%08X\r\n", hDumpThread));
                hRes = E_INVALIDARG;
                goto Exit;
            }
        }

        // Validate the passed in Process ID
        g_pDmpProc = HandleToProc(hDumpProc);
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
            if (g_pDmpThread->pOwnerProc != g_pDmpProc)
            {
                if (!fTrustedApp)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Un-Trusted Process does not own Thread passed to CaptureDumpFileOnDevice, dwProcessId=0x%08X, dwThreadId=0x%08X\r\n",
                                              hDumpProc, hDumpThread));
                    hRes = E_ACCESSDENIED;
                    goto Exit;
                }

                // Make sure thread is at least running in specified process for trusted apps
                if (g_pDmpThread->pProc != g_pDmpProc)
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
            if (g_pDmpProc == pCurProc)
            {
                // For the current process use the current thread
                g_pDmpThread = pCurThread;
            }
            else
            {
                // For any other process use the last created thread
                g_pDmpThread = g_pDmpProc->pTh;
            }
        }

        // Now make sure we have the current process for thread (may be different to owner)
        g_pDmpProc = g_pDmpThread->pProc;

        // Check if pointer to Extra Upload Files passed in
        if (NULL != g_pExceptionRecord->ExceptionInformation[2])
        {
            DWORD dwExtraFilesDirectoryLen = kdbgwcslen(g_watsonDumpSettings.wzExtraFilesDirectory);
            DWORD dwExtraFilesPathLen = kdbgwcslen((LPWSTR)g_pExceptionRecord->ExceptionInformation[2]);

            if (dwExtraFilesDirectoryLen && g_watsonDumpSettings.wzExtraFilesDirectory[dwExtraFilesDirectoryLen-1] == L'\\')
            {
                // Compare without the final delimiter
                --dwExtraFilesDirectoryLen;
            }
            
            // Path for extra upload files passed to CaptureDumpFileOnDevice API
            if ((dwExtraFilesDirectoryLen > 0) &&                                    // Check if allowed extra files
                (dwExtraFilesPathLen < MAX_PATH) &&                                  // Make sure path specified by user is valid length
                (dwExtraFilesPathLen >= dwExtraFilesDirectoryLen+2) &&               // Make sure path specified by user contains allowed path + delimiter + extra character
                (kdbgwcsnicmp((LPWSTR)g_pExceptionRecord->ExceptionInformation[2],   // Make sure start of path is same as allowed
                           g_watsonDumpSettings.wzExtraFilesDirectory,
                           dwExtraFilesDirectoryLen) == 0) &&
                (((LPWSTR)g_pExceptionRecord->ExceptionInformation[2])[dwExtraFilesDirectoryLen] == L'\\')) // Make sure the delimiter is in the correct place
            {
                // Store this in the dump settings for use by DwXfer.dll
                memcpy(g_watsonDumpSettings.wzExtraFilesPath, 
                       (PVOID)g_pExceptionRecord->ExceptionInformation[2],   
                       ((kdbgwcslen((LPWSTR)g_pExceptionRecord->ExceptionInformation[2])+1) * sizeof(WCHAR)));

                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: API call to CaptureDumpFileOnDevice, pwzExtraFilesPath='%s'\r\n",
                                          (LPWSTR)g_pExceptionRecord->ExceptionInformation[2]));
            }
            else
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!GetCaptureDumpFileParameters: Invalid path passed to CaptureDumpFileOnDevice, pwzExtraFilesPath='%s', Allowed Path='%s'+'\\'+'[FolderName]'\r\n",
                                          (LPWSTR)g_pExceptionRecord->ExceptionInformation[2], g_watsonDumpSettings.wzExtraFilesDirectory));
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

/*----------------------------------------------------------------------------
    MemoryBlocksAdd

    Add a new memory block for saving to dump file.
    
    This function assumes the global 'g_memoryBlocks' has been initialized 
    to all 0xFF before starting a new dump.
----------------------------------------------------------------------------*/
HRESULT MemoryBlocksAdd(DWORD dwMemoryStartAdd, DWORD dwMemorySizeAdd, BOOL fVirtual, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryBlock;
    DWORD dwFirstEmpty;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    DWORD dwMemoryEnd;
    DWORD dwBlockAdded;
    DWORD dwMemoryEndAdd;
    DWORD dwMemoryBlocksMax;
    MEMORY_BLOCK *pmemoryBlocks;
    DWORD *pdwMemorySizeTotal;
    DWORD *pdwMemoryBlocksTotal;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"++DwDmpGen!MemoryBlocksAdd: Enter, fVirtual=%u, Start=0x%08X, End=0x%08X, Size=0x%08X\r\n", 
                                 fVirtual, dwMemoryStartAdd, (dwMemoryStartAdd + dwMemorySizeAdd - 1) ,dwMemorySizeAdd));

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

    dwFirstEmpty = dwMemoryBlocksMax;
    dwBlockAdded = dwMemoryBlocksMax;

    for (dwMemoryBlock = 0; (dwMemoryBlock < dwFirstEmpty); ++dwMemoryBlock)
    {
        dwMemorySize = pmemoryBlocks[dwMemoryBlock].dwMemorySize;

        // Check if block currently in use
        if (EMPTY_BLOCK != dwMemorySize)
        {
            dwMemoryStart = pmemoryBlocks[dwMemoryBlock].dwMemoryStart;
            dwMemoryEnd = dwMemoryStart + dwMemorySize;
            
            // Check if start of new block is within current block (or immediately follows)
            if ((dwMemoryStartAdd >= dwMemoryStart) &&
                (dwMemoryStartAdd <= dwMemoryEnd))
            {
                // We should only have one match
                KD_ASSERT(dwMemoryBlocksMax == dwBlockAdded);
                
                // Get the combined memory end position
                dwMemoryEndAdd = max(dwMemoryEnd, (dwMemoryStartAdd + dwMemorySizeAdd));

                // Remove the old size from total
                *pdwMemorySizeTotal -= dwMemorySize;
                
                // Calculate the new size
                dwMemorySizeAdd = dwMemoryEndAdd - dwMemoryStart;

                // Set the new size
                pmemoryBlocks[dwMemoryBlock].dwMemorySize = dwMemorySizeAdd;

                // Add new size to total
                *pdwMemorySizeTotal += dwMemorySizeAdd;

                // Update the new start to point to current start
                dwMemoryStartAdd = dwMemoryStart;

                KD_ASSERT(dwMemorySizeAdd == dwMemoryEndAdd - dwMemoryStartAdd);

                // Save where block added 
                dwBlockAdded = dwMemoryBlock;
            }
        }
        else 
        {
            KD_ASSERT(dwMemoryBlocksMax == dwFirstEmpty);
            dwFirstEmpty = dwMemoryBlock;
        }
    }

    // If block not added, then add it if there is space for it
    if ((dwMemoryBlocksMax == dwBlockAdded) && (dwFirstEmpty < dwMemoryBlocksMax))
    {
        pmemoryBlocks[dwFirstEmpty].dwMemoryStart = dwMemoryStartAdd;
        pmemoryBlocks[dwFirstEmpty].dwMemorySize = dwMemorySizeAdd;
        dwMemoryEndAdd = dwMemoryStartAdd + dwMemorySizeAdd;
        
        ++ (*pdwMemoryBlocksTotal);
        *pdwMemorySizeTotal += dwMemorySizeAdd;

        // Save where block added
        dwBlockAdded = dwFirstEmpty;
        
        ++ dwFirstEmpty;
    }

    // If we added a block, then we must check all existing blocks for possible overlap
    if (dwBlockAdded < dwMemoryBlocksMax)
    {
        // The first Empty should be after the one added
        KD_ASSERT(dwFirstEmpty > dwBlockAdded);
        
        for (dwMemoryBlock = 0; (dwMemoryBlock < dwFirstEmpty); ++dwMemoryBlock)
        {
            // Don't check for overlap with self
            if (dwMemoryBlock != dwBlockAdded)
            {
                // Get info for current block
                dwMemorySize = pmemoryBlocks[dwMemoryBlock].dwMemorySize;
                dwMemoryStart = pmemoryBlocks[dwMemoryBlock].dwMemoryStart;
                dwMemoryEnd = dwMemoryStart + dwMemorySize;
            
                // All blocks less than dwFirstEmpty should be in use
                KD_ASSERT (EMPTY_BLOCK != dwMemorySize);
                
                // Check if start of current block is within new/updated block (or immediately follows)
                if ((dwMemoryStart >= dwMemoryStartAdd) &&
                    (dwMemoryStart <= dwMemoryEndAdd))
                {
                    // Get the combined memory end position
                    dwMemoryEndAdd = max(dwMemoryEndAdd, (dwMemoryStart + dwMemorySize));
        
                    // Remove the added size + current size from total
                    *pdwMemorySizeTotal -= (dwMemorySizeAdd + dwMemorySize);
                    
                    // Calculate the new combined size
                    dwMemorySizeAdd = dwMemoryEndAdd - dwMemoryStartAdd;
        
                    // Set the new combined size
                    pmemoryBlocks[dwBlockAdded].dwMemorySize = dwMemorySizeAdd;
        
                    // Add new combined size to total
                    *pdwMemorySizeTotal += dwMemorySizeAdd;

        
                    // We must now remove the current block 
                    --(*pdwMemoryBlocksTotal);

                    // Move last block to current position (Thereby removing current block)
                    pmemoryBlocks[dwMemoryBlock].dwMemoryStart= pmemoryBlocks[dwFirstEmpty-1].dwMemoryStart;
                    pmemoryBlocks[dwMemoryBlock].dwMemorySize = pmemoryBlocks[dwFirstEmpty-1].dwMemorySize;

                    // Invalidate last block
                    pmemoryBlocks[dwFirstEmpty-1].dwMemoryStart = 0;
                    pmemoryBlocks[dwFirstEmpty-1].dwMemorySize = EMPTY_BLOCK;

                    // Update the end pointer
                    -- dwFirstEmpty;

                    // Check if the new added block was the last, in which case it is now moved
                    if (dwBlockAdded == dwFirstEmpty)
                    {
                        dwBlockAdded = dwMemoryBlock;
                    }
                }
            }
        }
    }

    // Check if no blocks added
    if (dwMemoryBlocksMax == dwBlockAdded)
    {
        // If we get here, then we have reached MAX_MEMORY_BLOCKS
        KD_ASSERT(dwMemoryBlock == dwMemoryBlocksMax);
        
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryBlocksAdd: No free memory block found (Total blocks=%u)\r\n", dwMemoryBlock));
        hRes = E_FAIL;
        goto Exit;
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryBlocksAdd: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddAllPhysical

    Add all physical memory sections for entire device.
----------------------------------------------------------------------------*/
HRESULT MemoryAddAllPhysical(BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart = 0;
    DWORD dwMemorySize = 0;
    DWORD dwSection;
    MEMORYINFO *pMemoryInfo;
    ROMHDR *pTOC;
    KDataStruct *pKData;
    fslog_t *pLogPtr;
    DWORD MainMemoryEndAddress;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddAllPhysical: Enter\r\n"));

    pKData = FetchKData ();
    if (NULL == pKData)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddAllPhysical: FetchKData returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pTOC = FetchTOC();
    if (NULL == pTOC )
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddAllPhysical: FetchTOC returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pMemoryInfo = FetchMemoryInfo();
    if (NULL == pMemoryInfo)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddAllPhysical: FetchMemoryInfo returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    pLogPtr = FetchLogPtr();
    if (NULL == pLogPtr)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddAllPhysical: FetchLogPtr returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

#if defined(ARM)
    // Dump Arm FirstPT memory
    dwMemoryStart = (DWORD)ArmHigh->firstPT;
    dwMemorySize  = sizeof(ArmHigh->firstPT);

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllPhysical: Adding Arm FirstPT memory block\r\n"));
    
    hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, PHYSICAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllPhysical: MemoryBlocksAdd failed adding Arm FirstPT memory block, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }
#endif

    // Dump KData memory
    dwMemoryStart = (DWORD)pKData;
    dwMemorySize  = sizeof(KDataStruct);

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllPhysical: Adding KData memory block\r\n"));
    
    hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, PHYSICAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllPhysical: MemoryBlocksAdd failed adding KData memory block, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Dump Nk.bin Code memory
    dwMemoryStart = pTOC->physfirst;
    dwMemorySize  = pTOC->physlast - pTOC->physfirst;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllPhysical: Adding nk.bin memory block\r\n"));
    
    hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, PHYSICAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllPhysical: MemoryBlocksAdd failed adding nk.bin memory block, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Dump base RAM section
    KD_ASSERT(pLogPtr->fsmemblk[0].startptr == PAGEALIGN_UP(pTOC->ulRAMFree+MemForPT) + 4096);
    KD_ASSERT((pTOC->ulRAMStart <= pTOC->ulRAMFree) && (pTOC->ulRAMFree < pTOC->ulRAMEnd));
   
    MainMemoryEndAddress = pLogPtr->fsmemblk[0].startptr + pLogPtr->fsmemblk[0].extension + pLogPtr->fsmemblk[0].length;
    dwMemoryStart = pTOC->ulRAMStart;
    dwMemorySize  = MainMemoryEndAddress - pTOC->ulRAMStart;

    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllPhysical: Adding base RAM memory block\r\n"));
    
    hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, PHYSICAL_MEMORY, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllPhysical: MemoryBlocksAdd failed adding base RAM memory block, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Dump any extension RAM sections    
    for (dwSection = 1; dwSection < pMemoryInfo->cFi; ++ dwSection)
    {
        dwMemoryStart = (DWORD) (pLogPtr->fsmemblk[dwSection].startptr);
        dwMemorySize  = (DWORD) (pLogPtr->fsmemblk[dwSection].extension + pLogPtr->fsmemblk[dwSection].length);
        if (dwMemorySize > 0)
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddAllPhysical: Adding extension RAM memory block%u\r\n", dwSection));
            
            hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, PHYSICAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddAllPhysical: MemoryBlocksAdd failed adding extension RAM memory block%u, hRes=0x%08X\r\n", dwSection, hRes));
                goto Exit;
            }
        }
    }        

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddAllPhysical: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddProcessHeap

    Add all R/W sections for passed in process.
----------------------------------------------------------------------------*/
HRESULT MemoryAddProcessHeap(PPROCESS pProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart = 0;
    DWORD dwMemorySize = 0;
    DWORD ixSection, ixBlock, ixPage;
    PSECTION pscn;
    MEMBLOCK *pmb;
    DWORD    ulPTE;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddProcessHeap: Enter\r\n"));

    KD_ASSERT(NULL != pProcess);
    if (NULL == pProcess)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if (pProcess->dwVMBase)
    {
        if (pProcess->procnum)
        {
            ixSection = pProcess->procnum + RESERVED_SECTIONS;
            pscn = SectionTable[ixSection];
            KD_ASSERT(pscn != &NKSection);
        }
        else
        {
            // Special case for nk.exe 
            pscn = &NKSection;
            ixSection = SECURE_SECTION;
        }

        KD_ASSERT(pscn != NULL_SECTION);

        for (ixBlock = 0 ; ixBlock < BLOCK_MASK+1 ; ++ixBlock)
        {
            if (((pmb = (*pscn)[ixBlock]) != NULL_BLOCK) && (RESERVED_BLOCK != pmb))
            {
                for (ixPage = 0 ; ixPage < PAGES_PER_BLOCK ; ++ixPage)
                {
                    ulPTE = pmb->aPages[ixPage];
                    if ((ulPTE & PG_VALID_MASK)                     // valid page
                        && IsPageWritable (ulPTE))                  // R/W
                    {
                        // Found a writable memory page, assume it is part of the heap (or globals)
                        dwMemoryStart = (DWORD) (ixSection << VA_SECTION) | (ixBlock << VA_BLOCK) | (ixPage << VA_PAGE);
                        dwMemorySize = PAGE_SIZE;
                        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddProcessHeap: Adding '%s' heap memory -> ProcessID=0x%08X\r\n", 
                                                     pProcess->lpszProcName, pProcess->hProc));
                        
                        hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                        if (FAILED(hRes))
                        {
                            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessHeap: MemoryBlocksAdd failed adding '%s' heap memory, ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                                      pProcess->lpszProcName, pProcess->hProc, hRes));
                            goto Exit;
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
HRESULT MemoryAddProcessGlobals(PPROCESS pProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwO32count;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    ROMHDR *pTOC;
    COPYentry *pce;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddProcessGlobals: Enter\r\n"));

    KD_ASSERT(NULL != pProcess);
    if (NULL == pProcess)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if (0 == pProcess->procnum)
    {
        // Special case for nk.exe
        if (TRUE == g_fDumpNkGlobals)
        {
            pTOC = FetchTOC();
            if (NULL != pTOC)
            {
                pce = FetchCopyEntry (pTOC->ulCopyOffset);
                if (NULL != pce)
                {
                    // Indicate that nk.exe globals dumped (optimization needs to know)
                    g_fNkGlobalsDumped = TRUE;
                    
                    dwMemoryStart = pce->ulDest;
                    dwMemorySize = pce->ulDestLen;
                    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddProcessGlobals: Adding '%s' global variable memory -> ProcessID=0x%08X\r\n", 
                                                 pProcess->lpszProcName, pProcess->hProc));
                    
                    hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                    if (FAILED(hRes))
                    {
                        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessGlobals: MemoryBlocksAdd failed adding '%s' global variable memory -> ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                                  pProcess->lpszProcName, pProcess->hProc, hRes));
                        goto Exit;
                    }
                }
            }
        }
    }
    else
    {
        // All other processes
        for (dwO32count=0; dwO32count < pProcess->e32.e32_objcnt; dwO32count++)
        {
            if (pProcess->o32_ptr && (pProcess->o32_ptr[dwO32count].o32_flags & IMAGE_SCN_MEM_WRITE))
            {
                // Found a writable memory section
                dwMemoryStart = (DWORD) MapPtrProc(pProcess->o32_ptr[dwO32count].o32_realaddr, pProcess);
                dwMemorySize = pProcess->o32_ptr[dwO32count].o32_vsize;
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddProcessGlobals: Adding '%s' global variable memory%u -> ProcessID=0x%08X\r\n", 
                                             pProcess->lpszProcName, dwO32count, pProcess->hProc));
                
                hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddProcessGlobals: MemoryBlocksAdd failed adding '%s' global variable memory%u -> ProcessID=0x%08X, hRes=0x%08X\r\n", 
                                              pProcess->lpszProcName, dwO32count, pProcess->hProc, hRes));
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
    MemoryAddModuleGlobals

    Add the global memory sections for passed in process.
----------------------------------------------------------------------------*/
HRESULT MemoryAddModuleGlobals(PMODULE pMod, HPROCESS hFrameProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwO32count = 0;
    DWORD dwMemoryStart = 0;
    DWORD dwMemorySize = 0;
    PPROCESS pProcess = NULL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddModuleGlobals: Enter\r\n"));

    KD_ASSERT(NULL != pMod);
    if (NULL == pMod)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    pProcess = HandleToProc(hFrameProcess);
    if (NULL == pProcess)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddModuleGlobals: HandleToProc returned NULL\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    if (pMod->rwLow >= pMod->rwHigh)
    {
        for (dwO32count=0; dwO32count < pMod->e32.e32_objcnt; dwO32count++)
        {
            if (pMod->o32_ptr && (pMod->o32_ptr[dwO32count].o32_flags & IMAGE_SCN_MEM_WRITE))
            {
                // Found a writable memory section
                dwMemoryStart = (DWORD) MapPtrProc(ZeroPtr(pMod->o32_ptr[dwO32count].o32_realaddr), pProcess);
                dwMemorySize = pMod->o32_ptr[dwO32count].o32_vsize;
                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddModuleGlobals: Adding '%s' global variable memory%u for '%s'\r\n", 
                                             pMod->lpszModName, dwO32count, pProcess->lpszProcName));
                
                hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddModuleGlobals: MemoryBlocksAdd failed adding '%s' global variable memory%u for '%s', hRes=0x%08X\r\n", 
                                              pMod->lpszModName, dwO32count, pProcess->lpszProcName, hRes));
                    goto Exit;
                }
            }
        }
    }
    else
    {
        // RW data relocated
        dwMemoryStart = pMod->rwLow;
        dwMemorySize  = pMod->rwHigh - pMod->rwLow;
        
        // Get the memory mapped to the process associated with the frame
        dwMemoryStart = (DWORD)MapPtrProc(ZeroPtr(dwMemoryStart), pProcess);
        DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!MemoryAddModuleGlobals: Adding '%s' global variable memory for '%s'\r\n", 
                                     pMod->lpszModName, pProcess->lpszProcName));
        
        // Add the memory to the dump file
        hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!MemoryAddModuleGlobals: MemoryBlocksAdd failed adding '%s' global variable memory for '%s', hRes=0x%08X\r\n", 
                                      pMod->lpszModName, pProcess->lpszProcName, hRes));
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddModuleGlobals: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    MemoryAddGlobals

    Locate the process or module associated with the Return Address and
    add the global memory for that process or module.
----------------------------------------------------------------------------*/
HRESULT MemoryAddGlobals(CEDUMP_TYPE ceDumpType, DWORD dwReturnAddress, HPROCESS hFrameProcess, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    BOOL fGlobalsAdded = FALSE;
    BOOL fAddAnyGlobal = FALSE;
    DWORD dwProcessNumber;
    PPROCESS pProcArray;
    DWORD dwImageBase;
    DWORD dwImageSize;
    KDataStruct *pKData;
    PMODULE  pMod;
    DWORD dwReturnAddressSlot;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddGlobals: Enter\r\n"));

    switch (ceDumpType)
    {
        case ceDumpTypeComplete:
        case ceDumpTypeContext:
            // No globals added for these yet
            break;
            
        case ceDumpTypeSystem:
            // Add global for any module or process found that matches Return Address
            fAddAnyGlobal = TRUE;
            break;
            
        default:
            // This should not happen
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
            KD_ASSERT(FALSE); 
            hRes = E_FAIL;
            goto Exit;
    }

    if (TRUE == fAddAnyGlobal)
    {
        pProcArray = FetchProcArray();
        if (NULL == pProcArray)
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: FetchProcArray returned NULL\r\n"));
            hRes = E_FAIL;
            goto Exit;
        }

        // Iterate through all processes to find which one corresponds to Return Address
        for (dwProcessNumber = 0; ((dwProcessNumber < MAX_PROCESSES) && (FALSE == fGlobalsAdded)); dwProcessNumber++)
        {
            // Check for valid process
            if (pProcArray[dwProcessNumber].dwVMBase)
            {
                dwImageBase = (DWORD) pProcArray[dwProcessNumber].BasePtr;
                if (dwImageBase < 0x01FFFFFF)
                {
                    dwImageBase |= pProcArray[dwProcessNumber].dwVMBase;
                }
                dwImageSize = pProcArray[dwProcessNumber].e32.e32_vsize;

                if ((dwImageBase <= dwReturnAddress) && (dwReturnAddress < (dwImageBase + dwImageSize)))
                {
                    // Correct process found
                    hRes = MemoryAddProcessGlobals(&pProcArray[dwProcessNumber], fWrite);
                    if (FAILED(hRes))
                    {
                        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: MemoryAddProcessGlobals failed adding Process Global memory, hRes=0x%08X\r\n", hRes));
                        goto Exit;
                    }
                    fGlobalsAdded = TRUE;
                }
            }
        }

        if (FALSE == fGlobalsAdded)
        {
            // Look through module list to find which one corresponds to Return Address
            pKData = FetchKData ();
            if (NULL == pKData)
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: FetchKData returned NULL\r\n"));
                hRes = E_FAIL;
                goto Exit;
            }

            // Work in Slot 0 address space
            dwReturnAddressSlot = ZeroPtr(dwReturnAddress);

            pMod = (MODULE *)pKData->aInfo[KINX_MODULES];

            while (pMod && (pMod->lpSelf == pMod) && (FALSE == fGlobalsAdded))
            {
                dwImageBase = (DWORD) ZeroPtr(pMod->BasePtr);
                dwImageSize = (DWORD) pMod->e32.e32_vsize;
                if ((dwImageBase <= dwReturnAddressSlot) && (dwReturnAddressSlot < (dwImageBase + dwImageSize)))
                {
                    // Correct module found
                    hRes = MemoryAddModuleGlobals(pMod, hFrameProcess, fWrite);
                    if (FAILED(hRes))
                    {
                        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: MemoryAddModuleGlobals failed adding Module Global memory, hRes=0x%08X\r\n", hRes));
                        goto Exit;
                    }
                    fGlobalsAdded = TRUE;
                }
                pMod = pMod->pMod;
            }
        }
        
        if (FALSE == fGlobalsAdded)
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!MemoryAddGlobals: No global memory added for Return Address=0x%08X\r\n", dwReturnAddress));
            // Don't fail, just display the alert
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!MemoryAddGlobals: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}


/*----------------------------------------------------------------------------
    MemoryAddIPCode

    Add code surrounding the Instruction Pointer (IP). Limit the address 
    range to pages that are currently paged in.
----------------------------------------------------------------------------*/
HRESULT MemoryAddIPCode(DWORD dwMemoryIPBefore, DWORD dwMemoryIPAfter, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    DWORD dwMemoryEnd;
    DWORD dwIP;
    DWORD dwIPCheck;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!MemoryAddIPCode: Enter\r\n"));

    if (g_pDmpThread == pCurThread)
    {
        dwIP = CONTEXT_TO_PROGRAM_COUNTER (g_pContextException);
        dwIP = (DWORD) MapPtrProc (dwIP, g_pDmpProc);
    }
    else
    {
        dwIP = GetThreadIP (g_pDmpThread);
        dwIP = (DWORD) MapPtrProc (dwIP, g_pDmpProc);
    }
    
    // Address range wanted
    dwMemoryStart = dwIP - dwMemoryIPBefore;
    dwMemorySize = dwMemoryIPBefore + dwMemoryIPAfter;
    dwMemoryEnd = dwMemoryStart + dwMemorySize - 1;

    // Find lower limit for which range is valid
    dwIPCheck = dwIP;

    while (dwIPCheck >= dwMemoryStart)
    {
        if (NULL != SafeDbgVerify((PVOID)dwIPCheck, TRUE /* Probe Only */))
        {
            -- dwIPCheck;
        }
        else
        {
            dwMemoryStart = dwIPCheck+1;
        }
    }

    // Find upper limit for which range is valid
    dwIPCheck = dwIP;

    while (dwIPCheck <= dwMemoryEnd)
    {
        if (NULL != SafeDbgVerify((PVOID)dwIPCheck, TRUE /* Probe Only */))
        {
            ++ dwIPCheck;
        }
        else
        {
            dwMemoryEnd = dwIPCheck-1;
        }
    }

    if (dwMemoryStart <= dwMemoryEnd)
    {
        dwMemorySize = dwMemoryEnd - dwMemoryStart + 1;
        
        DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!MemoryAddIPCode: Add memory surrounding Instruction Pointer -> IP=0x%08X\r\n", dwIP));
        
        hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
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

/*----------------------------------------------------------------------------
    WriteCeHeader

    Write the dump file header.
----------------------------------------------------------------------------*/
HRESULT WriteCeHeader(DWORD dwNumberOfStreams, CEDUMP_TYPE ceDumpType, DWORD *pdwFileOffset, BOOL fWrite)
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
HRESULT WriteCeDirectory(DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwAlignment;

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

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeDirectory: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeDirectory: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamElementList

    Write the Element List Stream to the dump file.
    This is used to write out the list of Modules, Process, or Threads.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamElementList(DWORD dwStreamType, CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
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
    DWORD dwAlignment;
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
                dwRequestFilter = FLEXI_FILTER_THREAD_HANDLE;
                flexiRequest.dwHThread = (DWORD)g_pDmpThread->hTh;
            }
            else
            {
                // Only include owner process, threads for owner process, and modules for owner process
                dwRequestFilter = FLEXI_FILTER_PROCESS_HANDLE;
                flexiRequest.dwHProc = (DWORD)g_pDmpThread->pOwnerProc->hProc;
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
            flexiRequest.Mod.dwEnd   = -1;
            pflexiRange = &flexiRequest.Mod;
            break;

        case ceStreamProcessList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Process List\r\n"));
            pceDmpElementList = &g_ceDmpProcessList;
            dwRequestHdr  = FLEXI_REQ_PROCESS_HDR;
            dwRequestDesc = FLEXI_REQ_PROCESS_DESC;
            dwRequestData = FLEXI_REQ_PROCESS_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = -1;
            pflexiRange = &flexiRequest.Proc;
            break;

        case ceStreamThreadList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Thread List\r\n"));
            pceDmpElementList = &g_ceDmpThreadList;
            dwRequestHdr  = FLEXI_REQ_THREAD_HDR;
            dwRequestDesc = FLEXI_REQ_THREAD_DESC;
            dwRequestData = FLEXI_REQ_THREAD_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = -1;
            flexiRequest.Thread.dwStart = 0;
            flexiRequest.Thread.dwEnd   = -1;
            pflexiRange = &flexiRequest.Thread;
            break;

        case ceStreamThreadContextList:
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!WriteCeStreamElementList: Generating Thread Context List\r\n"));
            pceDmpElementList = &g_ceDmpThreadContextList;
            dwRequestHdr  = FLEXI_REQ_CONTEXT_HDR;
            dwRequestDesc = FLEXI_REQ_CONTEXT_DESC;
            dwRequestData = FLEXI_REQ_CONTEXT_DATA;
            flexiRequest.Proc.dwStart = 0;
            flexiRequest.Proc.dwEnd   = -1;
            flexiRequest.Thread.dwStart = 0;
            flexiRequest.Thread.dwEnd   = -1;
            pflexiRange = &flexiRequest.Thread;
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
                flexiRequest.Proc.dwEnd   = -1;
                flexiRequest.Thread.dwStart = 0;
                flexiRequest.Thread.dwEnd   = -1;
                pflexiRange = &flexiRequest.Thread;
                // Only include context of dump thread
                dwRequestFilter = FLEXI_FILTER_THREAD_HANDLE;
                flexiRequest.dwHThread = (DWORD)g_pDmpThread->hTh;
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
        dwFieldStringsSize += 4 + ((kdbgwcslen((WCHAR*)&g_bBuffer[pceDmpFieldInfo->FieldLabel+4]) + 1) * sizeof(WCHAR));
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
        dwFieldStringsSize += 4 + ((kdbgwcslen((WCHAR*)&g_bBuffer[pceDmpFieldInfo->FieldFormat+4]) + 1) * sizeof(WCHAR));
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

            KD_ASSERT(dwFlexiSizeRead == dwElementSize + sizeof(CEDUMP_ELEMENT_LIST));

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

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamElementList: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamElementList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamSystemInfo

    Write the System Information Stream to the dump file.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamSystemInfo(MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    SYSTEM_INFO sysi;
    DWORD dwAlignment;
    RVA rvaData;
    DWORD dwWriteOffset;
    ROMHDR *pTOC;

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

    pTOC = FetchTOC();
    if (NULL == pTOC )
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
        
        pfnGetSystemInfo (&sysi);
        g_ceDmpSystemInfo.SizeOfHeader          = sizeof(CEDUMP_SYSTEM_INFO);
        g_ceDmpSystemInfo.ProcessorArchitecture = sysi.wProcessorArchitecture;
        g_ceDmpSystemInfo.NumberOfProcessors    = sysi.dwNumberOfProcessors;
        g_ceDmpSystemInfo.ProcessorType         = sysi.dwProcessorType;
        g_ceDmpSystemInfo.ProcessorLevel        = sysi.wProcessorLevel;
        g_ceDmpSystemInfo.ProcessorRevision     = sysi.wProcessorRevision;
        g_ceDmpSystemInfo.ProcessorFamily       = TARGET_CODE_CPU;
        g_ceDmpSystemInfo.MajorVersion = CE_MAJOR_VER;
        g_ceDmpSystemInfo.MinorVersion = CE_MINOR_VER;
        g_ceDmpSystemInfo.BuildNumber  = CE_BUILD_VER;
        g_ceDmpSystemInfo.PlatformId   = VER_PLATFORM_WIN32_CE; 
        g_ceDmpSystemInfo.LCID         = (UserKInfo[KINX_NLS_SYSLOC]);
        g_ceDmpSystemInfo.Machine      = pTOC->usCPUType;

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

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamSystemInfo: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamSystemInfo: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamException

    Write the Exception Stream to the dump file.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamException(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    MINIDUMP_DIRECTORY ceDmpDirectory;
    DWORD dwAlignment;

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
        BOOL fFirstChanceException = g_fFirstChanceException && ((!g_fCaptureDumpFileOnDeviceCalled) && (!g_fReportFaultCalled));
        dwWriteOffset = 0;
        
        // Write out Exception Stream header
        g_ceDmpExceptionStream.SizeOfHeader = sizeof(CEDUMP_EXCEPTION_STREAM);
        g_ceDmpExceptionStream.SizeOfException = sizeof(CEDUMP_EXCEPTION);
        g_ceDmpExceptionStream.SizeOfThreadContext = sizeof(CEDUMP_ELEMENT_LIST);
        g_ceDmpExceptionStream.Flags  = (fFirstChanceException            ? CEDUMP_EXCEPTION_FIRST_CHANCE : 0);          
        g_ceDmpExceptionStream.Flags |= (g_fCaptureDumpFileOnDeviceCalled ? CEDUMP_EXCEPTION_CAPTURE_DUMP_FILE_API : 0); 
        g_ceDmpExceptionStream.Flags |= (g_fReportFaultCalled             ? CEDUMP_EXCEPTION_REPORT_FAULT_API : 0);      
        
        if (!g_fCaptureDumpFileOnDeviceCalled)
        {
            // This is the thread that caused exception and it's associated processes
            g_ceDmpExceptionStream.CurrentProcessId = (ULONG32)hCurProc; 
            g_ceDmpExceptionStream.ThreadId         = (ULONG32)hCurThread;
            g_ceDmpExceptionStream.OwnerProcessId   = (ULONG32)pCurThread->pOwnerProc->hProc;

            // For exception or Report Fault these are all 0
            g_ceDmpExceptionStream.CaptureAPICurrentProcessId = (ULONG32)0;  
            g_ceDmpExceptionStream.CaptureAPIThreadId         = (ULONG32)0; 
            g_ceDmpExceptionStream.CaptureAPIOwnerProcessId   = (ULONG32)0; 
        }
        else
        {
            // Use the focus thread and its associated processes when CaptureDumpFileOnDevice called
            g_ceDmpExceptionStream.CurrentProcessId = (ULONG32)g_pDmpThread->pProc->hProc; 
            g_ceDmpExceptionStream.ThreadId         = (ULONG32)g_pDmpThread->hTh;
            g_ceDmpExceptionStream.OwnerProcessId   = (ULONG32)g_pDmpThread->pOwnerProc->hProc;

            // This is the thread that called the CaptureDumpFileOnDevice API and it's associated processes
            g_ceDmpExceptionStream.CaptureAPICurrentProcessId = (ULONG32)hCurProc;  
            g_ceDmpExceptionStream.CaptureAPIThreadId         = (ULONG32)hCurThread; 
            g_ceDmpExceptionStream.CaptureAPIOwnerProcessId   = (ULONG32)pCurThread->pOwnerProc->hProc; 
        }

        fExceptionMatchesFocusThread = ((HANDLE)g_ceDmpExceptionStream.ThreadId == g_pDmpThread->hTh);
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
            memcpy(&g_ceDmpException, g_pExceptionRecord, sizeof(CEDUMP_EXCEPTION));
        }
        else
        {
            // Fake a breakpoint exception when CaptureDumpFileOnDevice called
            memset(&g_ceDmpException, 0, sizeof(CEDUMP_EXCEPTION));
            g_ceDmpException.ExceptionCode = STATUS_BREAKPOINT;
            if (g_pDmpThread == pCurThread)
            {
                // For the current thread we still use the exception address
                g_ceDmpException.ExceptionAddress = (ULONG32)g_pExceptionRecord->ExceptionAddress;
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

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamException: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamException: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamThreadCallStackList

    Write the list of Thread Call Stacks.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamThreadCallStackList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
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
    CallSnapshotEx csexFrames[STACK_FRAMES_BUFFERED] = {0};
    PPROCESS pProcArray = NULL;
    PTHREAD pThread = NULL;
    DWORD dwProcessNumber = 0;
    DWORD dwMaxStackFrames = 0;
    DWORD dwMaxProcesses = 0;
    BOOL  fCurrentThreadOnly = FALSE;
    DWORD dwGetCallStackFlags = STACKSNAP_EXTENDED_INFO | STACKSNAP_RETURN_FRAMES_ON_ERROR;
    DWORD dwSkipFrames = 0;
    DWORD dwFramesReturned = 0;
    DWORD dwParams = 0;
    DWORD dwAlignment;
    DWORD dwMaxThreadEntries = sizeof(g_bBuffer)/sizeof(WORD);

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
            dwMaxProcesses = 1;
            dwNumberOfProcesses = 1;
            dwMaxStackFrames = MAX_STACK_FRAMES_CONTEXT;
            pProcArray = g_pDmpProc;
            dwNumberOfThreads = 1;
            break;
            
        case ceDumpTypeSystem:
            // Include all call stacks from all threads in all processes
            dwMaxProcesses = MAX_PROCESSES;
            dwMaxStackFrames = MAX_STACK_FRAMES_SYSTEM;

            pProcArray = FetchProcArray();
            if (NULL == pProcArray)
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: FetchProcArray returned NULL\r\n"));
                hRes = E_FAIL;
                goto Exit;
            }

            // Iterate through all process & threads to get total number of threads
            for (dwProcessNumber = 0; dwProcessNumber < dwMaxProcesses; dwProcessNumber++)
            {
                // Check for valid process
                if (pProcArray[dwProcessNumber].dwVMBase)
                {
                    ++ dwNumberOfProcesses;
                    
                    pThread = pProcArray[dwProcessNumber].pTh;
                    while (pThread)
                    {
                        // Valid thread found, increment the number of threads
                        ++ dwNumberOfThreads;
                        pThread = pThread->pNextInProc;
                    }
                }
            }

            break;
            
        default:
            // This should not happen
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: Bad Dump Type detected, ceDumpType=%u\r\n", ceDumpType));
            KD_ASSERT(FALSE); 
            hRes = E_FAIL;
            goto Exit;
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
    for (dwProcessNumber = 0; dwProcessNumber < dwMaxProcesses; dwProcessNumber++)
    {
        if ((pProcArray[dwProcessNumber].dwVMBase) || (fCurrentThreadOnly))
        {
            if (fCurrentThreadOnly)
            {
                // We are only doing the current Thread's call stack
                KD_ASSERT(1 == dwNumberOfThreads);
                KD_ASSERT(1 == dwMaxProcesses);
                pThread = g_pDmpThread; // Point to the Dump thread
            }
            else
            {
                pThread = pProcArray[dwProcessNumber].pTh;
            }

            while (pThread)
            {
                // Valid thread found, increment the thread count
                ++ dwThreadCount;

                dwNumberOfFrames = 0;
                dwFramesReturned = STACK_FRAMES_BUFFERED;
                
                // Repeatedly collect frames until either max reached or no more frames available
                while ((dwNumberOfFrames < dwMaxStackFrames) && (STACK_FRAMES_BUFFERED == dwFramesReturned))
                {
                    dwFramesReturned = GetCallStack(pThread->hTh, STACK_FRAMES_BUFFERED, csexFrames, dwGetCallStackFlags, dwNumberOfFrames);
                    dwNumberOfFrames += dwFramesReturned;
                }

                // Limit number of frames to the max allowed
                dwNumberOfFrames = min (dwNumberOfFrames, dwMaxStackFrames);

                if (fWrite)
                {
                    ceDumpThreadCallStack.ProcessId = (ULONG32)pThread->pOwnerProc->hProc;
                    ceDumpThreadCallStack.ThreadId = (ULONG32)pThread->hTh;
                    ceDumpThreadCallStack.SizeOfFrame = sizeof(CEDUMP_THREAD_CALL_STACK_FRAME);
                    ceDumpThreadCallStack.NumberOfFrames = (WORD)dwNumberOfFrames;
                    ceDumpThreadCallStack.StackFrames = dwFileOffsetStackFrames;
                    
                    hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpThreadCallStack, sizeof(CEDUMP_THREAD_CALL_STACK), TRUE);
                    if (FAILED(hRes))
                    {
                        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing Thread Call Stack header, hRes=0x%08X\r\n", hRes));
                        goto Exit;
                    }
                    dwWriteOffset += sizeof(CEDUMP_THREAD_CALL_STACK);
                }

                SET_FRAME_COUNT(dwThreadCount, dwNumberOfFrames);

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
                    pThread = pThread->pNextInProc;
                }
            }
        }
    }

    KD_ASSERT(dwThreadCount == dwNumberOfThreads);
    KD_ASSERT(fWrite ? (dwWriteOffset == pceDmpDirectory->Location.DataSize) : TRUE);

    *pdwFileOffset += pceDmpDirectory->Location.DataSize;

    dwThreadCount = 0;
    dwWriteOffset = 0;

    // Write out all Thread Call Stack Frames by iterating through all processes and threads
    for (dwProcessNumber = 0; dwProcessNumber < dwMaxProcesses; dwProcessNumber++)
    {
        if ((pProcArray[dwProcessNumber].dwVMBase) || (fCurrentThreadOnly))
        {
            if (fCurrentThreadOnly)
            {
                // We are only doing the current Thread's call stack
                KD_ASSERT(1 == dwNumberOfThreads);
                KD_ASSERT(1 == dwMaxProcesses);
                pThread = g_pDmpThread; // Point to the Dump thread
            }
            else
            {
                pThread = pProcArray[dwProcessNumber].pTh;
            }

            while (pThread)
            {
                // Valid thread found, increment the thread count
                ++ dwThreadCount;

                dwNumberOfFrames = 0;
                dwFramesReturned = STACK_FRAMES_BUFFERED;
                
                // Repeatedly write frames until either max reached or no more frames available
                while ((dwNumberOfFrames < dwMaxStackFrames) && (STACK_FRAMES_BUFFERED == dwFramesReturned))
                {
                    dwFramesReturned = GetCallStack(pThread->hTh, STACK_FRAMES_BUFFERED, csexFrames, dwGetCallStackFlags, dwNumberOfFrames);
                    DWORD dwGetCallStackError = pfnGetLastError();

                    for (dwFrameCount = 0;  (dwFrameCount < dwFramesReturned) && (dwNumberOfFrames < dwMaxStackFrames); ++ dwFrameCount)
                    {
                        if (fWrite)
                        {
                            memset(&ceDumpThreadCallStackFrame, 0, sizeof(ceDumpThreadCallStackFrame));
                            ceDumpThreadCallStackFrame.ReturnAddr = csexFrames[dwFrameCount].dwReturnAddr;
                            ceDumpThreadCallStackFrame.FramePtr   = csexFrames[dwFrameCount].dwFramePtr;
                            ceDumpThreadCallStackFrame.ProcessId  = csexFrames[dwFrameCount].dwCurProc;
                            ceDumpThreadCallStackFrame.Flags      = (dwGetCallStackError ? CEDUMP_CALL_STACK_ERROR_DETECTED : 0);
                            for (dwParams = 0; dwParams < 4; ++dwParams)
                            {
                                ceDumpThreadCallStackFrame.Params[dwParams] = csexFrames[dwFrameCount].dwParams[dwParams];
                            }
                            
                            hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpThreadCallStackFrame, sizeof(CEDUMP_THREAD_CALL_STACK_FRAME), TRUE);
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
                            hRes = MemoryAddGlobals(ceDumpType, csexFrames[dwFrameCount].dwReturnAddr, (HPROCESS)csexFrames[dwFrameCount].dwCurProc, fWrite);
                            if (FAILED(hRes))
                            {
                                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: MemoryAddGlobals failed adding global memory for ReturnAddr=0x%08X, hRes=0x%08X\r\n", hRes));
                                goto Exit;
                            }
                        }

                        ++ dwNumberOfFrames;
                        dwWriteOffset += sizeof(CEDUMP_THREAD_CALL_STACK_FRAME);
                    }
                }

                if (!CHECK_FRAME_COUNT(dwThreadCount, dwNumberOfFrames))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamThreadCallStackList: Frame Count Check failure, ThreadID=0x%08X, ExpectedFrames=%u, ActualFrames=%u\r\n", 
                                              pThread->hTh, GET_FRAME_COUNT(dwThreadCount), dwNumberOfFrames));
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
                    pThread = pThread->pNextInProc;
                }
            }
        }
    }

    KD_ASSERT(dwThreadCount == dwNumberOfThreads);
    KD_ASSERT(dwFileOffsetStackFrames == (*pdwFileOffset) + dwWriteOffset);

    *pdwFileOffset += dwWriteOffset;

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamThreadCallStackList: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamThreadCallStackList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamMemoryList

    Write the list of Memory dumps.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamMemoryList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fVirtual, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    DWORD dwFileOffsetMemoryDumps;
    DWORD dwMemoryBlockCount;
    DWORD dwMemoryBlock;
    DWORD dwMemorySize;
    DWORD dwMemorySizeTotal;
    MINIDUMP_MEMORY_DESCRIPTOR ceDumpMemoryDescriptor;
    DWORD dwAlignment;
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
    pceDmpDirectory->Location.DataSize = sizeof(CEDUMP_MEMORY_LIST) +
                                         (sizeof(MINIDUMP_MEMORY_DESCRIPTOR) * (*pdwMemoryBlocksTotal)); 
    pceDmpDirectory->Location.Rva      = *pdwFileOffset;

    // Write out Memory List header info
    if (fWrite)
    {
        dwWriteOffset = 0;
        dwFileOffsetMemoryDumps = (*pdwFileOffset) + pceDmpDirectory->Location.DataSize;
        
        // Write out the Memory List Stream header
        pceDmpMemoryList->SizeOfHeader = sizeof(CEDUMP_MEMORY_LIST);
        pceDmpMemoryList->SizeOfEntry  = sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
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
                
                hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, &ceDumpMemoryDescriptor, sizeof(MINIDUMP_MEMORY_DESCRIPTOR), TRUE);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryList: WatsonWriteData failed writing memory descriptor, Index=%u, Count=%u, hRes=0x%08X\r\n", 
                                              dwMemoryBlock, dwMemoryBlockCount, hRes));
                    goto Exit;
                }
                dwWriteOffset += sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
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
                hRes = WatsonWriteData((*pdwFileOffset) + dwWriteOffset, (PVOID)pmemoryBlocks[dwMemoryBlock].dwMemoryStart, dwMemorySize, TRUE);
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

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryList: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamMemoryList: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteCeStreamMemoryVirtualList

    Write the list of Virtual Memory dumps.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamMemoryVirtualList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMemoryStart;
    DWORD dwMemorySize;
    BOOL fCurrentProcessGlobals = FALSE;
    BOOL fCurrentProcessIPMemory = FALSE;
    BOOL fCurrentThreadStackMemory = FALSE;
    BOOL fCurrentProcessHeap = FALSE;
    DWORD dwStackBase, dwStackSize, dwStackBound, dwStackTop;
    DWORD dwMemoryIPBefore;
    DWORD dwMemoryIPAfter;
    DWORD dwFileOffsetEstimate;
    DWORD dwFileSpaceLeft; 
    DWORD dwMaxFileSize = (-1);

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

            // Include stack memory of current thread (faulting thread)
            // This will also include the TLS data which is on the stack
            fCurrentThreadStackMemory = TRUE;

            break;
            
        case ceDumpTypeComplete:
            // ** Fall through **
        case ceDumpTypeContext:
            // Amount of memory to save before and after Instruction Pointer
            fCurrentProcessIPMemory = TRUE;
            dwMemoryIPBefore = MEMORY_IP_BEFORE_CONTEXT;
            dwMemoryIPAfter = MEMORY_IP_AFTER_CONTEXT;

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

        // **********************************************************************
        // *** The stack memory must be the last virtual memory added, since  ***
        // *** it may be sized down to fit in the Context dump size limit     ***
        // **********************************************************************

        if (fCurrentThreadStackMemory)
        {
            
            // Calculate offset to include Memory List header and Memory descriptors 
            dwFileOffsetEstimate = (*pdwFileOffset) + 
                                   (sizeof(CEDUMP_MEMORY_LIST) * 2) + // Virtual + Physical list
                                   (sizeof(MINIDUMP_MEMORY_DESCRIPTOR) * (g_dwMemoryVirtualBlocksTotal+1)); // +1 for non-secure stack memory descriptor
                                   
            // Calculate offset to include total memory added so far                       
            dwFileOffsetEstimate += g_dwMemoryVirtualSizeTotal;
            
            // Calculate file space left for adding non-secure stack memory 
            dwFileSpaceLeft = dwMaxFileSize - dwFileOffsetEstimate;

            // Memory for non-secure stack (may be a fiber)
            dwStackBase = g_pDmpThread->tlsNonSecure[PRETLS_STACKBASE];
            dwStackBound = g_pDmpThread->tlsNonSecure[PRETLS_STACKBOUND]; // Memory is commited to this point

            if (dwStackBase == g_pDmpThread->dwOrigBase)
            {
                // Normal thread
                dwStackSize = g_pDmpThread->dwOrigStkSize;
            }
            else
            {
                // Fiber thread
                dwStackSize = g_pDmpThread->tlsNonSecure[PRETLS_PROCSTKSIZE];
                KD_ASSERT(dwStackSize == g_pDmpThread->pOwnerProc->e32.e32_stackmax);
            }

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

            hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryBlocksAdd failed adding non-secure Stack memory, hRes=0x%08X\r\n", hRes));
                goto Exit;
            }

            if (g_pDmpThread->tlsNonSecure != g_pDmpThread->tlsSecure)
            {
                // Calculate new File space left to limit size of secure stack
                // We don't simply add the previous memory block size, since it may have been
                // combined with another, so we work it out again from the begining
                
                // Calculate offset to include Memory List header and Memory descriptors 
                dwFileOffsetEstimate = (*pdwFileOffset) + 
                                       (sizeof(CEDUMP_MEMORY_LIST) * 2) + // Virtual + Physical list
                                       (sizeof(MINIDUMP_MEMORY_DESCRIPTOR) * (g_dwMemoryVirtualBlocksTotal+1)); // +1 for secure stack memory descriptor
                                       
                // Calculate offset to include total memory added so far (This will now include the non-secure stack)                       
                dwFileOffsetEstimate += g_dwMemoryVirtualSizeTotal;
                
                // Calculate file space left for adding secure stack memory 
                dwFileSpaceLeft = dwMaxFileSize - dwFileOffsetEstimate;

                // Write the decriptor for secure stack (may be a fiber?)
                dwStackBase = g_pDmpThread->tlsSecure[PRETLS_STACKBASE];
                dwStackBound = g_pDmpThread->tlsSecure[PRETLS_STACKBOUND]; // Memory is commited to this point

                // Secure stack size is always fixed
                dwStackSize = CNP_STACK_SIZE;
                
                dwStackTop = (dwStackBase + dwStackSize);
                
                // Save the stack between the Bound and Top
                dwMemoryStart = dwStackBound;
                dwMemorySize = dwStackTop - dwStackBound;

                // Limit memory size to amount of file space left
                if (dwMemorySize > dwFileSpaceLeft)
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Limiting Secure Stack memory to fit in dump file -> Old Size=0x%08X, New Size=0x%08X\r\n", dwMemorySize, dwFileSpaceLeft));
                    dwMemorySize = dwFileSpaceLeft;
                }

                DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: Add Secure Stack memory\r\n"));
                
                hRes = MemoryBlocksAdd(dwMemoryStart, dwMemorySize, VIRTUAL_MEMORY, fWrite);
                if (FAILED(hRes))
                {
                    DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!WriteCeStreamMemoryVirtualList: MemoryBlocksAdd failed adding secure Stack memory, hRes=0x%08X\r\n", hRes));
                    goto Exit;
                }
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
HRESULT WriteCeStreamMemoryPhysicalList(CEDUMP_TYPE ceDumpType, MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
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
            hRes = MemoryAddAllPhysical(fWrite);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamMemoryPhysicalList: MemoryAddAllPhysical failed, hRes=0x%08X\r\n", hRes));
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
    WriteCeStreamBucketParameters

    Write the Bucket Parameters Stream to the dump file.
----------------------------------------------------------------------------*/
HRESULT WriteCeStreamBucketParameters(MINIDUMP_DIRECTORY *pceDmpDirectory, DWORD *pdwFileOffset, BOOL fWrite)
{
    HRESULT hRes = E_FAIL;
    DWORD dwWriteOffset;
    LPWSTR lpwzEventType;
    LPWSTR lpwzAppName;
    LPWSTR lpwzModName;
    LPWSTR lpwzOwnerName;
    RVA    rvaString;
    DWORD  dwStringSize;
    DWORD dwAlignment;

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

    hRes = GetBucketParameters(&g_ceDmpBucketParameters);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: GetBucketParameters failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Convert String pointers to RVAs
    rvaString = pceDmpDirectory->Location.Rva + pceDmpDirectory->Location.DataSize;

    // Event Type 
    lpwzEventType = (LPWSTR)g_ceDmpBucketParameters.EventType;
    g_ceDmpBucketParameters.EventType = rvaString;
    dwStringSize = kdbgwcslen(lpwzEventType) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // App Name
    lpwzAppName = (LPWSTR)g_ceDmpBucketParameters.AppName;
    g_ceDmpBucketParameters.AppName = rvaString;
    dwStringSize = kdbgwcslen(lpwzAppName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // Mod Name
    lpwzModName = (LPWSTR)g_ceDmpBucketParameters.ModName;
    g_ceDmpBucketParameters.ModName = rvaString;
    dwStringSize = kdbgwcslen(lpwzModName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));

    // Owner Name
    lpwzOwnerName = (LPWSTR)g_ceDmpBucketParameters.OwnerName;
    g_ceDmpBucketParameters.OwnerName = rvaString;
    dwStringSize = kdbgwcslen(lpwzOwnerName) * sizeof(WCHAR);
    rvaString += sizeof(dwStringSize) + dwStringSize + (1 * sizeof(WCHAR));
    
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
        dwStringSize = kdbgwcslen(lpwzEventType) * sizeof(WCHAR);
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
        dwStringSize = kdbgwcslen(lpwzAppName) * sizeof(WCHAR);
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
        dwStringSize = kdbgwcslen(lpwzModName) * sizeof(WCHAR);
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
        dwStringSize = kdbgwcslen(lpwzOwnerName) * sizeof(WCHAR);
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

        KD_ASSERT(rvaString == ((*pdwFileOffset) + dwWriteOffset));
    }

    *pdwFileOffset = rvaString;

    // Make sure we are aligned on exit
    dwAlignment = (*pdwFileOffset) % CEDUMP_ALIGNMENT;
    if (dwAlignment > 0)
    {
        dwAlignment = CEDUMP_ALIGNMENT - dwAlignment;
        if (fWrite)
        {
            hRes = WatsonWriteData(*pdwFileOffset, &g_bBufferAlignment, dwAlignment, TRUE);
            if (FAILED(hRes))
            {
                DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteCeStreamBucketParameters: WatsonWriteData failed writing %u alignment bytes, hRes=0x%08X\r\n", dwAlignment, hRes));
                goto Exit;
            }
        }
        *pdwFileOffset += dwAlignment;
    }

    hRes = S_OK;

Exit:

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!WriteCeStreamBucketParameters: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteDumpFileContents

    Write the contents of the dump file.
----------------------------------------------------------------------------*/
HRESULT WriteDumpFileContents(CEDUMP_TYPE ceDumpType, DWORD *pdwFileOffset, BOOL fWrite)
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
        memset(&g_ceDmpBucketParameters, 0, sizeof(g_ceDmpBucketParameters));
        memset(&g_memoryVirtualBlocks, 0xFF, sizeof(g_memoryVirtualBlocks));
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

    // Add the Process List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamProcessList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Process list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Thread List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamThreadList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Thread list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Thread Context List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamElementList(ceStreamThreadContextList, ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamElementList failed writing Thread Context list, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Thread Call Stack List stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamThreadCallStackList(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamThreadCallStackList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Bucket Parameters stream
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamBucketParameters(&g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamBucketParameters failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

    // Add the Memory Virtual List stream 
    // *** MEMORY VIRTUAL LIST MUST BE 2ND LAST STREAM, Context/Complete dump will trim the virtual memory list to fit into 64K limit *** 
    if (++dwStream >= MAX_STREAMS) goto ErrMaxStreams;

    hRes = WriteCeStreamMemoryVirtualList(ceDumpType, &g_ceDmpDirectory[dwStream], pdwFileOffset, fWrite);
    if (FAILED(hRes))
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!WriteDumpFileContents: WriteCeStreamMemoryVirtualList failed, hRes=0x%08X\r\n", hRes));
        goto Exit;
    }

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
HRESULT OptimizeDumpFileContents(CEDUMP_TYPE *pceDumpType, DWORD dwMaxDumpFileSize)
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
            dwFileOffset = (-1);
        }

        dwMaxDumpFileSizeForType = (ceDumpTypeContext == ceDumpTypeOptimized) ? WATSON_CONTEXT_DUMPFILE_SIZE : dwMaxDumpFileSize;

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

    hRes = S_OK;
    
Exit:
    
    DEBUGGERMSG(OXZONE_DWDMPGEN, (L"--DwDmpGen!OptimizeDumpFileContents: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}

/*----------------------------------------------------------------------------
    WriteDumpFile

    Write out the entire dump file.
----------------------------------------------------------------------------*/
HRESULT WriteDumpFile(CEDUMP_TYPE ceDumpType, DWORD dwMaxDumpFileSize)
{
    HRESULT hRes = E_FAIL;
    DWORD dwFileOffset = 0;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!WriteDumpFile: Enter\r\n"));

    // Optimize dump file contents, this may change the type
    hRes = OptimizeDumpFileContents(&ceDumpType, dwMaxDumpFileSize);
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
HRESULT ReadSettings(CEDUMP_TYPE *pceDumpType, DWORD *pdwMaxDumpFileSize)
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
    g_dwReservedMemorySize = pfnNKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_GETWATSONSIZE, NULL, 0, NULL, 0, NULL);
    if (g_dwReservedMemorySize < WATSON_MINIMUM_RESERVED_MEMORY)
    {
        // Reserved memory is allocated by adding 'dwNKDrWatsonSize = [size to reserve];' to OEMInit().
        // The size of memory to reserve must be a multiple of PAGE_SIZE bytes
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
            SetEvent(g_hEventDumpFileReady);
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
HRESULT WriteSettings()
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
HRESULT Initialize(CEDUMP_TYPE *pceDumpType, DWORD *pdwMaxDumpFileSize, BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!Initialize: Enter\r\n"));

    if (FALSE == fOnTheFly)
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
HRESULT Shutdown(BOOL fOnTheFly)
{
    HRESULT hRes = E_FAIL;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!Shutdown: Enter\r\n"));

    if (FALSE == fOnTheFly)
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

/*----------------------------------------------------------------------------
    CaptureDumpFile

    Capture dump file to persitent storage.
----------------------------------------------------------------------------*/
BOOL CaptureDumpFile(PEXCEPTION_RECORD pExceptionRecord, PCONTEXT pContextRecord, BOOLEAN fSecondChance)
{
    HRESULT hRes = E_FAIL;
    DWORD dwMaxDumpFileSize = 0;
    CEDUMP_TYPE ceDumpType = ceDumpTypeUndefined;
    BOOL fCallOAL = FALSE;
    BOOL fHandled = FALSE;
    BOOL fPassedInterLock = FALSE;
    SAVED_THREAD_STATE svdThread = {0};

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++DwDmpGen!CaptureDumpFile: Enter\r\n"));

    SWITCHKEY (svdThread.aky, 0xffffffff);

    // This must be first check, otherwise g_lThreadsInPostMortemCapture will get out of sync
    if (InterlockedIncrement(&g_lThreadsInPostMortemCapture) > 1)
    {
        DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: Another thread already running Post Portem Capture code (Threads = %u)\r\n", g_lThreadsInPostMortemCapture));
        hRes = E_FAIL;
        goto Exit;
    }

    // Indicate we passed the InterlockedIncrement check
    fPassedInterLock = TRUE;

    // Prevent other threads from running
    svdThread.bCPrio = (BYTE)GET_CPRIO (pCurThread);
    svdThread.bBPrio = (BYTE)GET_BPRIO (pCurThread);   // Save this for threads list, but don't change value for Watson
    svdThread.dwQuantum = pCurThread->dwQuantum;
    svdThread.fSaved = TRUE;

    pCurThread->dwQuantum = 0;
    SET_CPRIO (pCurThread, 0);

    // Global variable initialization must be done after the InterlockedIncrement to prevent changing by re-entrancy
    g_psvdThread = &svdThread;
    g_pExceptionRecord = pExceptionRecord;
    g_pContextException = pContextRecord;
    g_fFirstChanceException = !fSecondChance;

    // Init to no error, will be set during processing
    g_dwLastError = ERROR_SUCCESS;
    
    __try
    {
        if ((NULL == g_pExceptionRecord) || (NULL == g_pContextException))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFile: Invalid parameter\r\n"));
            hRes = E_INVALIDARG;
            goto Exit;
        }

        // Don't create dumps for debug breaks
        if (STATUS_BREAKPOINT == g_pExceptionRecord->ExceptionCode)
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: Breakpoint exception @0x%08X, exiting ...\r\n", CONTEXT_TO_PROGRAM_COUNTER (g_pContextException)));
            hRes = E_FAIL;
            goto Exit;
        }

        hRes = Initialize(&ceDumpType, &dwMaxDumpFileSize, FALSE /* fOnTheFly */);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_DWDMPGEN, (L"  DwDmpGen!CaptureDumpFile: Initialize failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        // Indicate if CaptureDumpFileOnDevice API called
        g_fCaptureDumpFileOnDeviceCalled = CAPTUREDUMPFILEONDEVICE_CALLED(g_pExceptionRecord, *ppCaptureDumpFileOnDevice);

        if (g_fFirstChanceException)
        {
            // 1st chance exception
            if (g_fCaptureDumpFileOnDeviceCalled)
            {
                // We only handle the exception when CaptureDumpFileOnDevice called & 1st chance
                fHandled = TRUE;
            }
            else if (!g_watsonDumpSettings.fDumpFirstChance)
            {
                // 1st chance exception dump disabled
                DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: First chance exception, exiting ...\r\n"));
                hRes = E_FAIL;
                goto Exit;
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
                goto Exit;
            }
        }

        if (pfnOEMKDIoControl != NULL)
        {
            // Let KDIoControl know dump is about to start
            KCall ((PKFN)pfnOEMKDIoControl, KD_IOCTL_DMPGEN_START, NULL, g_dwReservedMemorySize);

            // Make sure we call again to say we are finished
            fCallOAL = TRUE;
        }

        // Set the process ID and Thread ID for dumping
        // This function may change the state of g_fCaptureDumpFileOnDeviceCalled, g_pExceptionRecord, & g_pContextException
        hRes = GetCaptureDumpFileParameters();
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: GetCaptureDumpFileParameters failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        // Save the pointer to the exception context, this may have been changed by GetCaptureDumpFileParameters
        hRes = SaveExceptionContext(g_pContextException, &svdThread, NULL, NULL);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFile: SaveExceptionContext failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        // Flush the FPU registers before writing the dump file
        KdpFlushExtendedContext (pContextRecord);

        // Write the dump file to reserved memory
        hRes = WriteDumpFile(ceDumpType, dwMaxDumpFileSize);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: WriteDumpFile failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        hRes = Shutdown(FALSE /* fOnTheFly */);
        if (FAILED(hRes))
        {
            DEBUGGERMSG(OXZONE_ALERT,(L"  DwDmpGen!CaptureDumpFile: Shutdown failed, hRes=0x%08X\r\n", hRes));
            goto Exit;
        }

        DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFile: Dump File captured in reserved memory\r\n"));
        if (g_hEventDumpFileReady)
        {
            // Tell DwXfer.dll that a dump file is ready
            DEBUGGERMSG(OXZONE_DWDMPGEN,(L"  DwDmpGen!CaptureDumpFile: Set 'Dump File Ready' Event\r\n"));
            SetEvent(g_hEventDumpFileReady);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(OXZONE_ALERT, (L"  DwDmpGen!CaptureDumpFile: Exception occured capturing dump file, quiting ...\r\n"));
        hRes = E_FAIL;
        goto Exit;
    }

    hRes = S_OK;

Exit:    

    if (fCallOAL && (pfnOEMKDIoControl != NULL))
    {
        KCall((PKFN)pfnOEMKDIoControl, KD_IOCTL_DMPGEN_END, NULL, hRes);
    }

    if (fPassedInterLock)
    {
        // Set global pointers to NULL, this will allow KdStub to override settings
        g_psvdThread = NULL;
        g_pExceptionRecord = NULL;
        g_pContextException = NULL;

        // Allow other threads to run again
        pCurThread->dwQuantum = svdThread.dwQuantum;
        SET_CPRIO (pCurThread, svdThread.bCPrio);
        svdThread.fSaved = FALSE;
    }

    InterlockedDecrement(&g_lThreadsInPostMortemCapture);

    KD_ASSERT(GETCURKEY() == 0xffffffff);

    SETCURKEY (svdThread.aky);
        
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--DwDmpGen!CaptureDumpFile: Leave, hRes=0x%08X\r\n", hRes));

    // Check for errors when using CaptureDumpFileOnDevice API
    if ((g_fCaptureDumpFileOnDeviceCalled || g_fReportFaultCalled) && fHandled && (hRes != S_OK))
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
        pfnSetLastError(g_dwLastError);
        DBGRETAILMSG(1, (L"  DwDmpGen!CaptureDumpFile: Error detected during capture, hRes=0x%08X, SetLastError=%u\r\n", hRes, g_dwLastError));
    }

    return ((S_OK == hRes) ? fHandled : FALSE);
}

