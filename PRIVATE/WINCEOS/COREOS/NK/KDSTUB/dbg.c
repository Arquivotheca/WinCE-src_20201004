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

    dbg.c

Abstract:


Environment:

    WinCE

--*/

#ifdef DEBUG
#undef DEBUG
#pragma message("No!")
#endif

#include "kdp.h"

///#define V_ADDR(addr)    VerifyAccess((addr), VERIFY_KERNEL_OK, (ACCESSKEY)-1)
#define V_ADDR(addr)    (addr)

extern LPVOID ThreadStopFunc;
const DWORD dwKStart=(DWORD)-1, dwNewKStart=(DWORD)-1;  //KPage start and new start addresses
const DWORD dwKEnd=(DWORD)-1, dwNewKEnd=(DWORD)-1;      //KPage end and new end addresses

static const CHAR lpszUnk[]="UNKNOWN";
static const CHAR lpszKernName[]="NK.EXE";

// Stackwalk state globals
PCALLSTACK pStk        = NULL;
PPROCESS   pLastProc   = NULL;
PTHREAD    pWalkThread = NULL;

static WCHAR rgchBuf[256];

void kdbgWtoA(LPWSTR pWstr, LPCHAR pAstr) {
    while (*pAstr++ = (CHAR)*pWstr++)
        ;
}

void kdbgWtoAmax(LPWSTR pWstr, LPCHAR pAstr, int max) {
    while ((max--) && (*pAstr++ = (CHAR)*pWstr++))
        ;
}

DWORD kdbgwcslen(LPWSTR pWstr) {
    const wchar_t *eos = pWstr;
    while( *eos++ )
        ;
    return( (size_t)(eos - pWstr - 1) );
}

CHAR lpszModuleName[MAX_PATH];

LPCHAR GetWin32ExeName(PPROCESS pProc)
{
    if (pProc->lpszProcName) {
        kdbgWtoA(pProc->lpszProcName,lpszModuleName);
        return lpszModuleName;
    }
    return (LPCHAR)lpszUnk;
}

BOOL GetNameandImageBase(PPROCESS pProc, DWORD dwAddress, PNAMEANDBASE pnb, BOOL fRedundant, DWORD BreakCode)
{
    LPMODULE lpMod;
    static BOOL PhysUpd;

    DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nGetting name and base for address %8.8lx \r\n"), dwAddress));

	// set default to not ROM DLL
	pnb->dwDllRwEnd = 0;
	pnb->dwDllRwStart = 0xFFFFFFFF;
	
    // Check for the kernel first
    if (((DWORD)kdProcArray[0].BasePtr + 1) == dwAddress)
    {
        if (PhysUpd && fRedundant)
        {
            DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nReturning redundant\r\n")));
            return FALSE;
        }
        
        pnb->szName = (LPCHAR)lpszKernName;
        pnb->ImageBase    = (ULONG)kdProcArray[0].BasePtr;
        pnb->ImageSize    = kdProcArray[0].e32.e32_vsize;
        pnb->dwDllRwStart = ((COPYentry *)(pTOC->ulCopyOffset))->ulDest;
        pnb->dwDllRwEnd   = pnb->dwDllRwStart + ((COPYentry *)(pTOC->ulCopyOffset))->ulDestLen - 1;
            
        PhysUpd=TRUE;
        DEBUGGERMSG(KDZONE_DBG, (L"Returning kernel %8.8lx-%8.8lx %a\r\n", pnb->ImageBase, pnb->ImageBase+pnb->ImageSize, pnb->szName));
        
        return TRUE;
    }

    DEBUGGERMSG(KDZONE_DBG,
            (TEXT("\r\nMy address %8.8lx, adjusted %8.8lx, Base: %8.8lx, Base+Size: %8.8lx\r\n"),
            dwAddress, (ULONG)MapPtrInProc(dwAddress, pProc), (long)pProc->BasePtr,
            MapPtrInProc((long)pProc->BasePtr+pProc->e32.e32_vsize, pProc)));

    if (MapPtrInProc(dwAddress, pProc) >= MapPtrInProc((long)pProc->BasePtr, pProc) &&
        (MapPtrInProc(dwAddress, pProc) < MapPtrInProc((long)pProc->BasePtr+pProc->e32.e32_vsize, pProc)))
    { // Check the process array
        pnb->szName = GetWin32ExeName(pProc);
        pnb->ImageBase = (UINT)MapPtrInProc(pProc->BasePtr, pProc);
        pnb->ImageSize = pProc->e32.e32_vsize;
        DEBUGGERMSG(KDZONE_DBG, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx of Executable\r\n"),
                    pnb->szName, pnb->ImageBase, pnb->ImageSize));
    }
    else
    { // Check the module (DLL) list
	    lpMod = pModList;
	    while (lpMod)
	    {
	        if (dwAddress == ((DWORD) lpMod->BasePtr)+1)
	        {
	            break;
	        }
	        
	        lpMod = lpMod->pMod;
	    }

        if (lpMod)
        {
            if ((lpMod->DbgFlags & DBG_SYMBOLS_LOADED) &&
                (fRedundant) &&
                (BreakCode != DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT))
            {
                DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nReturing redundant\r\n")));
            }

            kdbgWtoA(lpMod->lpszModName,lpszModuleName);
            pnb->szName = lpszModuleName;
            pnb->ImageBase = ZeroPtr(lpMod->BasePtr);
            pnb->ImageSize = lpMod->e32.e32_vsize;
            pnb->dwDllRwStart = lpMod->rwLow;
            pnb->dwDllRwEnd = lpMod->rwHigh;
            lpMod->DbgFlags |=DBG_SYMBOLS_LOADED;
        }
        else
        {
            DEBUGGERMSG(KDZONE_DBG, (TEXT("No module associated with address %8.8lx\r\n"), dwAddress));
            return FALSE;
        }

        DEBUGGERMSG(KDZONE_DBG, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx of DLL\r\n"),
                    pnb->szName, pnb->ImageBase, pnb->ImageSize));
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////
//---- OLD STYLE STATIC PROCESS AND THREAD INFORMATION CODE ----//
//////////////////////////////////////////////////////////////////

// Structures used to retrieve debug information from the kernel
#define SIG_DBGPROCINFO         0x46780002 // should go away with HANDLE_PROCESS_INFO_REQUEST
#define SIG_DBGTHREADINFO       0x46780020 // should go away with HANDLE_PROCESS_INFO_REQUEST

// @struct DBGPROCINFO | Used to return info in <f GetDbgInfo>
typedef struct _DBGPROCINFO {
    DWORD   dwSig;              // @field Must be SIG_DBGPROCINFO
    LPVOID  lpProcess;          // @field Ptr to process structure
    ULONG   ulPid;              // @field PID of process
    DWORD   dwVMBase;           // @field Start of address space
    ULONG   ulAccessKey;        // @field Address space access permissions
    LPVOID  lpvBasePtr;         // @field BasePtr assigned to this process
    ULONG   ulCurZones;         // @field Cur zone mask in effect
    CHAR    rgbProcName[32];    // @field Process name in ASCII
} DBGPROCINFO, *LPDBGPROCINFO; 

// @struct DBGTHREADINFO | Used to return info in <f GetDbgInfo>
typedef struct _DBGTHREADINFO {
    DWORD   dwSig;          // @field Must be SIG_DBGTHREADINFO
    LPVOID  lpThread;       // @field Ptr to thread structure
    LPVOID  lpvRunQPtr;     // @field Ptr to RunQ if thread is blocked
    UINT    uThreadState;   // @field State of the thread
    ULONG   ulAccessKey;    // @field Cur access permissions
    LPVOID  hCurProcess;    // @field Handle to process currently in
    ULONG   ulSleepCount;   // @field Sleep time
    USHORT  usSuspendCount; // @field Suspend time
    USHORT  usPriority;     // @field Current priority
} DBGTHREADINFO, *LPDBGTHREADINFO; 

static DWORD MarshalThread(PTHREAD pthCur, LPBYTE lpbBuf, DWORD dwSize) { 
    LPDBGTHREADINFO lpdbgThread=(LPDBGTHREADINFO)lpbBuf;
    DWORD dwUsed=0;
    if (dwSize<sizeof(DBGTHREADINFO))
        goto done;
    // Fill fields
    lpdbgThread->dwSig = SIG_DBGTHREADINFO;
    lpdbgThread->lpThread = pthCur;
    lpdbgThread->lpvRunQPtr = 0;
    if (GET_SLEEPING(pthCur))
        lpdbgThread->uThreadState = 4 + (pthCur->lpProxy != 0)
                                    + (pthCur->bSuspendCnt != 0) * 2;
    else
        lpdbgThread->uThreadState = GET_RUNSTATE(pthCur);
    lpdbgThread->ulAccessKey = pthCur->aky;
    lpdbgThread->ulSleepCount = pthCur->dwWakeupTime;
    lpdbgThread->usSuspendCount = pthCur->bSuspendCnt;
    lpdbgThread->hCurProcess = pthCur->pProc->hProc;
    lpdbgThread->usPriority = pthCur->wInfo;
    // accounting
    dwUsed += sizeof(DBGTHREADINFO);
done:
    return dwUsed;
}

static DWORD MarshalProcess(PPROCESS pProc, LPBYTE lpbBuf, DWORD dwSize) { 
    LPDBGPROCINFO lpdbgProc = (LPDBGPROCINFO)lpbBuf;
    DWORD dwUsed=0;
    PTHREAD pthCur;
    if (dwSize<sizeof(DBGPROCINFO))
        goto done;
    // Fill fields
    lpdbgProc->dwSig = SIG_DBGPROCINFO;
    lpdbgProc->lpProcess = pProc;
    lpdbgProc->ulPid = pProc->dwVMBase;
    lpdbgProc->dwVMBase = pProc->dwVMBase;
    lpdbgProc->ulAccessKey = pProc->aky;
    lpdbgProc->lpvBasePtr = pProc->BasePtr;
    lpdbgProc->rgbProcName[0] = '\0';
    lpdbgProc->ulCurZones = pProc->ZonePtr?pProc->ZonePtr->ulZoneMask:0;
    if (pProc->lpszProcName) {
        int loop;
        LPWSTR pTrav1;
        LPSTR pTrav2;
        pTrav1 = pProc->lpszProcName;
        pTrav2 = lpdbgProc->rgbProcName;
        for (loop = 0; (loop < sizeof(lpdbgProc->rgbProcName)-1) && *pTrav1; loop++)
            *pTrav2++ = (BYTE)*pTrav1++;
        *pTrav2 = 0;
    }
    // accounting
    dwUsed += sizeof(DBGPROCINFO);
    // Check for threads
    for (pthCur=pProc->pTh; pthCur; pthCur=pthCur->pNextInProc)
        dwUsed += MarshalThread(pthCur, lpbBuf+dwUsed, dwSize-dwUsed);
done:
    return dwUsed;
}


/////////////////////////////////////////////////////
//---- FLEXIBLE PROCESS AND THREAD INFORMATION ----//
/////////////////////////////////////////////////////

typedef struct _PROC_THREAD_INFO_FIELD
{
    WORD    wIdentifier; // -1 is Custom field (identified by label then)
    WORD    wSize; // size of field in bytes
    PCHAR   szLabel; // field label (zero terminated string)
    PCHAR   szFormat; // string containing default format (printf style) to use for rendering field
} PROC_THREAD_INFO_FIELD;

// NOTE on format strings:
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

//////////////////////////////
// Process Descriptor Table //
//////////////////////////////

// Process Fields Identifiers
#define pfiStructAddr       (0L) // address to the process structure itself
#define pfiProcessSlot      (1L) // Slot number
#define pfiStartOfAddrSpace (2L) // VM Address space (slot) first address
#define pfiDefaultAccessKey (3L) // Default thread Access keys
#define pfiBasePtr          (4L) // First exe module load address
#define pfiCurDbgZoneMasks  (5L) // Current Debug Zone mask
#define pfiName             (6L) // EXE Name
#define pfiCmdLine          (7L) // Command line
#define pfiTrustLevel       (8L) // Trust level
#define pfiHandle           (9L) // Process handle
#define pfiTlsUsageBitMaskL (10L) // First 32 TLS slots usage bit mask
#define pfiTlsUsageBitMaskH (11L) // Second 32 TLS slots usage bit mask
#define pfiUserDefined      (-1L) // field identified by its label

PROC_THREAD_INFO_FIELD ProcessDescriptorTable [] =
{
    {
        pfiProcessSlot,
        sizeof (BYTE),
        "ProcSlot#",
        "%u"
    },
    {
        pfiName,
        32L,
        "Name",
        "%s"
    },
    {
        pfiStartOfAddrSpace,
        sizeof (DWORD),
        "VMBase",
        "0x%08lX"
    },
    {
        pfiDefaultAccessKey,
        sizeof (ULONG),
        "AccessKey",
        "0x%08lX"
    },
    {
        pfiTrustLevel,
        sizeof (BYTE),
        "TrustLevel",
        "%N{0=None,1=Run,2=Full}"
    },
    {
        pfiHandle,
        sizeof (HANDLE),
        "hProcess",
        "0x%08lX"
    },
    {
        pfiBasePtr,
        sizeof (LPVOID),
        "BasePtr",
        "0x%08lX"
    },
    {
        pfiTlsUsageBitMaskL,
        sizeof (DWORD),
        "TlsUseL32b",
        "0x%08lX"
    },
    {
        pfiTlsUsageBitMaskH,
        sizeof (DWORD),
        "TlsUseH32b",
        "0x%08lX"
    },
    {
        pfiCurDbgZoneMasks,
        sizeof (ULONG),
        "CurZoneMask",
        "0x%08lX"
    },
    {
        pfiStructAddr,
        sizeof (LPVOID),
        "pProcess",
        "0x%08lX"
    },
    {
        pfiCmdLine,
        128L,
        "CmdLine",
        "%s"
    },
};

/////////////////////////////
// Thread Descriptor Table //
/////////////////////////////

// Thread Fields Identifiers
#define tfiStructAddr                   (0L) // address to the thread structure itself
#define tfiRunState                     (1L) // Running / Sleeping / Blocked / Killed states of the thread
#define tfiAddrSpaceAccessKey           (2L) // Current access key for handles and memory access
#define tfiHandleCurrentProcessRunIn    (3L) // Current process running in
#define tfiSleepCount                   (4L) // Sleep count
#define tfiSuspendCount                 (5L) // Suspend count
#define tfiCurrentPriority              (6L) // Current priority
#define tfiInfo                         (7L) // Information status bits
#define tfiBasePriority                 (8L) // Base priority
#define tfiWaitState                    (9L) // Wait state
#define tfiHandleOwnerProc              (10L) // Handle to the process owning the thread
#define tfiTlsPtr                       (11L) // Thread local storage block pointer
#define tfiKernelTime                   (12L) // Accumulated time spend in kernel mode
#define tfiUserTime                     (13L) // Accumulated time spend in user mode
#define tfiHandle                       (14L) // Thread handle
#define tfiLastError                    (15L) // Last error
#define tfiStackBase                    (16L) // Stack base address
#define tfiStackLowBound                (17L) // Lower bound of commited stack space
#define tfiCreationTimeMSW              (18L) // MSW of Creation timestamp
#define tfiCreationTimeLSW              (19L) // LSW of Creation timestamp
#define tfiQuantum                      (20L) // Quantum
#define tfiQuantumLeft                  (21L) // Quantum left
#define tfiUserDefined                  (-1L) // field identified by its label

PROC_THREAD_INFO_FIELD ThreadDescriptorTable [] =
{
    {
        tfiStructAddr,
        sizeof (LPVOID),
        "pThread",
        "0x%08lX"
    },
    {
        tfiRunState,
        sizeof (WORD),
        "RunState",
        "%T{4=Dying,5=Dead,6=Buried,7=Slpg,39=Awak,0=Rung,1=Runab,2=RunBlkd,3=RunNeeds}"
    },
    {
        tfiInfo,
        sizeof (WORD),
        "InfoStatus",
        "%T{38=UMode,6=KMode,8=StkFlt,12=UsrBlkd,15=Profd}"
    },
    {
        tfiHandle,
        sizeof (HANDLE),
        "hThread",
        "0x%08lX"
    },
    {
        tfiWaitState,
        sizeof (BYTE),
        "WaitState",
        "%N{0=Signalled,1=Processing,2=Blocked}"
    },
    {
        tfiAddrSpaceAccessKey,
        sizeof (ACCESSKEY),
        "AccessKey",
        "0x%08lX"
    },
    {
        tfiHandleCurrentProcessRunIn,
        sizeof (HANDLE),
        "hCurProcIn",
        "0x%08lX"
    },
    {
        tfiHandleOwnerProc,
        sizeof (HANDLE),
        "hOwnerProc",
        "0x%08lX"
    },
    {
        tfiCurrentPriority,
        sizeof (BYTE),
        "CurPrio",
        "%u"
    },
    {
        tfiBasePriority,
        sizeof (BYTE),
        "BasePrio",
        "%u"
    },
    {
        tfiKernelTime,
        sizeof (DWORD),
        "KernelTime",
        "%lu"
    },
    {
        tfiUserTime,
        sizeof (DWORD),
        "UserTime",
        "%lu"
    },
    {
        tfiQuantum,
        sizeof (DWORD),
        "Quantum",
        "%lu"
    },
    {
        tfiQuantumLeft,
        sizeof (DWORD),
        "QuantuLeft",
        "%lu"
    },
    {
        tfiSleepCount,
        sizeof (DWORD),
        "SleepCount",
        "%lu"
    },
    {
        tfiSuspendCount,
        sizeof (BYTE),
        "SuspendCount",
        "%u"
    },    
    {
        tfiTlsPtr,
        sizeof (LPDWORD),
        "TlsPtr",
        "0x%08lX"
    },
    {
        tfiLastError,
        sizeof (DWORD),
        "LastError",
        "0x%08lX"
    },
    {
        tfiStackBase,
        sizeof (DWORD),
        "StackBase",
        "0x%08lX"
    },
    {
        tfiStackLowBound,
        sizeof (DWORD),
        "StkLowBnd",
        "0x%08lX"
    },
    {
        tfiCreationTimeMSW,
        sizeof (DWORD),
        "CreatTimeH",
        "0x%08lX"
    },
    {
        tfiCreationTimeLSW,
        sizeof (DWORD),
        "CreatTimeL",
        "0x%08lX"
    }
};


#define PROC_DESC_NB_FIELDS (sizeof (ProcessDescriptorTable) / sizeof (PROC_THREAD_INFO_FIELD))
#define THREAD_DESC_NB_FIELDS (sizeof (ThreadDescriptorTable) / sizeof (PROC_THREAD_INFO_FIELD))


#define AppendImmByteToOutBuf_M(outbuf,immbyte,outbidx) {(outbuf) [(outbidx)++] = (immbyte);}
#define AppendObjToOutBuf_M(outbuf,obj,outbidx) {memcpy (&((outbuf) [(outbidx)]), &(obj), sizeof (obj)); (outbidx) += sizeof (obj);}
#define AppendStringZToOutBuf_M(outbuf,sz,outbidx) {memcpy (&((outbuf) [(outbidx)]), sz, (strlen (sz) + 1)); (outbidx) += (strlen (sz) + 1);}


/*++

Routine Name:

    MarshalDescriptionTable

Routine Description:

    Copy (Process or Thread Info) Description Table in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    DescTable       - Supplies (Process or Thread Info) description table
    TblSize         - Supplies number of elements in the description table
    NbMaxOutByte    - Supplies maximum number of bytes that can be written in output buffer

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL MarshalDescriptionTable (IN OUT PCHAR OutBuf,
                              IN OUT PUSHORT pOutBufIndex,
                              IN PROC_THREAD_INFO_FIELD DescTable [],
                              IN USHORT TblSize,
                              IN USHORT NbMaxOutByte)

{
    USHORT  FieldIdx;
    USHORT  FieldDescSize;

    for (FieldIdx = 0; FieldIdx < TblSize; FieldIdx++)
    {
        FieldDescSize = sizeof (DescTable [FieldIdx].wIdentifier) +
                        sizeof (DescTable [FieldIdx].wSize) +
                        strlen (DescTable [FieldIdx].szLabel) +
                        strlen (DescTable [FieldIdx].szFormat);
        if (FieldDescSize <= NbMaxOutByte)
        { // Remaining buffer large enough for next field
            AppendObjToOutBuf_M (OutBuf, DescTable [FieldIdx].wIdentifier, *pOutBufIndex);
            AppendObjToOutBuf_M (OutBuf, DescTable [FieldIdx].wSize, *pOutBufIndex);
            AppendStringZToOutBuf_M (OutBuf, DescTable [FieldIdx].szLabel, *pOutBufIndex);
            AppendStringZToOutBuf_M (OutBuf, DescTable [FieldIdx].szFormat, *pOutBufIndex);
        }
        else
        { // Buffer not large enough: exit with error
            return FALSE;
        }
        NbMaxOutByte -= FieldDescSize;
    }
    return TRUE;
} // End of MarshalDescriptionTable


/*++

Routine Name:

    MarshalProcessInfoData

Routine Description:

    Copy Process Info Data in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    ProcessOrdNum   - Supplies Process Info ordinal number / Index
    NbMaxOutByte    - Supplies maximum number of bytes that can be written in output buffer

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL MarshalProcessInfoData (   IN OUT PCHAR OutBuf,
                                IN OUT PUSHORT pOutBufIndex,
                                IN DWORD ProcessOrdNum,
                                IN USHORT NbMaxOutByte)

{
    USHORT      FieldIdx;
    USHORT      FieldSize = 0;
    PPROCESS    pProc = &(kdProcArray [ProcessOrdNum]);
    ULONG       ZoneMask;
    BOOL        Result;
    CHAR        szProcName [32];
    CHAR        szProcCmdline [128];
    static const CHAR pszUnk[] = "Unknown";

    // FieldSize = SUM (ProcessDescriptorTable [0..PROC_DESC_NB_FIELDS[.wSize)
    for (FieldIdx = 0; FieldIdx < PROC_DESC_NB_FIELDS; FieldIdx++)
    {
        FieldSize += ProcessDescriptorTable [FieldIdx].wSize;
    }
    if (FieldSize <= NbMaxOutByte)
    { // Remaining buffer large enough for all process fields
        //////////////////////////////////////////
        // Insert Process Info Data fields now: //
        //////////////////////////////////////////
        AppendObjToOutBuf_M (OutBuf,    pProc->procnum,         *pOutBufIndex); // pfiProcessSlot
        if (pProc->lpszProcName)
        {
            __try
            {
                kdbgWtoAmax (pProc->lpszProcName, szProcName, sizeof (szProcName) - 1);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                memcpy(szProcName, pszUnk, sizeof(pszUnk));
            }
        }
        else
        {
            memcpy(szProcName, pszUnk, sizeof(pszUnk));
        }
        szProcName [sizeof (szProcName) - 1] = 0;
        AppendObjToOutBuf_M (OutBuf,    szProcName,             *pOutBufIndex); // pfiName
        AppendObjToOutBuf_M (OutBuf,    pProc->dwVMBase,        *pOutBufIndex); // pfiStartOfAddrSpace
        AppendObjToOutBuf_M (OutBuf,    pProc->aky,             *pOutBufIndex); // pfiDefaultAccessKey
        AppendObjToOutBuf_M (OutBuf,    pProc->bTrustLevel,       *pOutBufIndex); // pfiTrustLevel
        AppendObjToOutBuf_M (OutBuf,    pProc->hProc,           *pOutBufIndex); // pfiHandle
        AppendObjToOutBuf_M (OutBuf,    pProc->BasePtr,         *pOutBufIndex); // pfiBasePtr
        AppendObjToOutBuf_M (OutBuf,    pProc->tlsLowUsed,         *pOutBufIndex); // pfiTlsUsageBitMaskL
        AppendObjToOutBuf_M (OutBuf,    pProc->tlsHighUsed,         *pOutBufIndex); // pfiTlsUsageBitMaskH
        if (pProc->ZonePtr)
        {
            __try
            {
                ZoneMask = pProc->ZonePtr->ulZoneMask;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                ZoneMask = 0;
            }
        }
        else
        {
            ZoneMask = 0;
        }
        AppendObjToOutBuf_M (OutBuf,    ZoneMask,               *pOutBufIndex); // pfiCurDbgZoneMasks
        AppendObjToOutBuf_M (OutBuf,    pProc,                  *pOutBufIndex); // pfiStructAddr
        if (pProc->pcmdline)
        {
            __try
            {
                kdbgWtoAmax ((LPWSTR) pProc->pcmdline, szProcCmdline, sizeof (szProcCmdline) - 1);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                memcpy(szProcCmdline, pszUnk, sizeof(pszUnk));
            }
        }
        else
        {
            memcpy(szProcCmdline, pszUnk, sizeof(pszUnk));
        }
        szProcCmdline [sizeof (szProcCmdline) - 1] = 0;
        AppendObjToOutBuf_M (OutBuf,    szProcCmdline,          *pOutBufIndex); // pfiCmdLine
        Result = TRUE;
    }
    else
    { // Buffer not large enough: exit with error
        Result = FALSE;
    }
    return Result;
} // End of MarshalProcessInfoData




static BOOL sprintf_temp(
    PCHAR   pOutBuf, 
    PUSHORT pOutBufIndex,
    PUSHORT pMaxBytes,
    LPWSTR  lpszFmt, 
    ...
    ) 
{
    WCHAR szTempString[256];
    WORD wLen;

    //
    // Write to a temporary string
    //
    NKwvsprintfW(szTempString, lpszFmt,
        (LPVOID) ( ((DWORD)&lpszFmt) + sizeof(lpszFmt) ), 
        sizeof(szTempString) / sizeof(WCHAR));
    
    wLen = (WORD) kdbgwcslen(szTempString);
    
    if (*pMaxBytes < wLen) {
        // Not enough space
        return FALSE;
    }
    //
    // Convert to ASCII and write it to the output buffer
    //
    kdbgWtoA( szTempString, (CHAR*) &(pOutBuf[*pOutBufIndex]) );
    //
    // Don't include the NULL. Instead manually update index to include null at 
    // the very end.
    //
    *pOutBufIndex += wLen;
    *pMaxBytes -= wLen;

    return TRUE;
}

/*++

Routine Name:

    MarshalCriticalSectionInfo

Routine Description:

    Copy Critical Section Info in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    NbMaxOutByte    - Supplies maximum number of bytes that can be written in output buffer

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL 
MarshalCriticalSectionInfo (   
    IN OUT PCHAR OutBuf,
    IN OUT PUSHORT pOutBufIndex,
    IN USHORT nMaxBytes
    )
{
//    USHORT      FieldIdx;
//    USHORT      FieldSize = 0;
    PPROCESS    pProc;
    DWORD       nProc; //, nThread;
    PTHREAD     pThread;
    DWORD       ThreadNum = 0;
    LPPROXY     pProxy;
//    ULONG       ZoneMask;
//    BOOL        Result;
//    CHAR        szProcName [32];


    sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Critical Section Info (shows synchronization object status)\r\n"));
    
    for (nProc = 0; nProc < MAX_PROCESSES; nProc++) {
        pProc = &(kdProcArray[nProc]);
    
        if (!pProc->dwVMBase) {
            // No process in this slot
            continue;
        }

        sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT(" pProcess : 0x%08X %s\r\n"), pProc, pProc->lpszProcName);
        pThread = pProc->pTh; // Get process main thread
        
        ThreadNum = 1;
        while (pThread)
        { // walk list of threads attached to process (until we reach the index ThreadOrdNum)
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\tpThread 0x%08X\r\n"), pThread);
            
            pProxy = pThread->lpProxy;

            if (pProxy) {
                while (pProxy) {
                    
                    switch(pProxy->bType) {
                        case HT_CRITSEC : {
                            LPCRIT lpCrit = (LPCRIT) pProxy->pObject;
                            PTHREAD pOwnerThread = HandleToThread(lpCrit->lpcs->OwnerThread);
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tpCritSect 0x%08X (pOwner 0x%X)\r\n"), lpCrit->lpcs, pOwnerThread);
                            break;
                        }
    
                        case HT_EVENT : {
                            LPEVENT lpEvent = (LPEVENT) pProxy->pObject;
//                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\thEvent 0x%08X (name : %s)\r\n"), lpEvent->hNext, lpEvent->name->name ? lpEvent->name->name : TEXT("none"));
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\thEvent 0x%08X\r\n"), lpEvent->hNext);
                            break;
                        }
    
                        default : {
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tUnknown object type %d (0x%08X)\r\n"), pProxy->bType, pProxy->pObject);
                            break;
                        }
    
                    }
                    pProxy = pProxy->pThLinkNext;
                }
            } else {
                // Not blocked
            }
            
            pThread = pThread->pNextInProc;
            ThreadNum++;
        }


    }

    // Include the NULL
    *pOutBufIndex++;
    
    return TRUE;
} // End of MarshalCriticalSectionInfo

/*++

Routine Name:

    MarshalIOReadWrite

Routine Description:

    Copy Critical Section Info in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    nMaxBytes       - Supplies maximum number of bytes that can be written in output buffer
    fIORead         - TRUE indicate Read otherwise Write
    dwAddress       - Address to read /write
    dwSize          - Size to read /write
    dwData          - Data to write

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL 
MarshalIOReadWrite (   
    IN OUT PCHAR OutBuf,
    IN OUT PUSHORT pOutBufIndex,
    IN USHORT nMaxBytes,
    BOOL fIORead,
    DWORD dwAddress,
    DWORD dwSize,
    DWORD dwData
    )
{
    DBGKD_MANIPULATE_STATE dmsIO;
    STRING strIO;
    dmsIO.u.ReadWriteIo.IoAddress = (PVOID)dwAddress;
    dmsIO.u.ReadWriteIo.DataSize = dwSize;
    dmsIO.u.ReadWriteIo.DataValue = dwData;
    strIO.Length = 0;
    strIO.Buffer = 0;
    strIO.MaximumLength = 0;

    if (fIORead)
    {
        sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Read: Address=0x%08X, Size=%u  ==>  "),dwAddress,dwSize);
        KdpReadIoSpace(&dmsIO,&strIO,NULL,FALSE);
    }
    else
    {
        sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Write: Address=0x%08X, Size=%u, Data=0x%08X  ==>  "),dwAddress,dwSize,dwData);
        KdpWriteIoSpace(&dmsIO,&strIO,NULL,FALSE);
    }

    switch (dmsIO.ReturnStatus)
    {
        case STATUS_SUCCESS: 
            if (fIORead)
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success: Data=0x%08X\r\n"),dmsIO.u.ReadWriteIo.DataValue);
            }
            else
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success\r\n"));
            }
            break;
        case STATUS_ACCESS_VIOLATION:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Access Violation\r\n"));
            break;
        case STATUS_DATATYPE_MISALIGNMENT:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Datatype Misalignment\r\n"));
            break;
        case STATUS_INVALID_PARAMETER:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Invalid Parameter\r\n"));
            break;
        default:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Unknown Error\r\n"));
            break;
    }
    
    // Include the NULL
    *pOutBufIndex++;
    
    return TRUE;
} // End of MarshalCriticalSectionInfo



/*++

Routine Name:

    GetNumberOfThreadsAttachedToProc

Routine Description:

    Calculate the total number of thread(s) attached to the given process

Arguments:

    ProcessOrdNum   - Supplies Process Info ordinal number / Index

Return Value:

    Total number of thread(s) attached to this process.

--*/

DWORD GetNumberOfThreadsAttachedToProc (IN DWORD ProcessOrdNum)

{
    PTHREAD     pThread;
    DWORD       ThreadNum = 0;

    pThread = kdProcArray [ProcessOrdNum].pTh; // Get process main thread
    while (pThread)
    { // walk list of threads attached to process
        pThread = pThread->pNextInProc;
        ThreadNum++;
    }
    return ThreadNum;
} // End of GetNumberOfThreadsAttachedToProc


/*++

Routine Name:

    MarshalThreadInfoData

Routine Description:

    Copy Thread Info Data in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    ProcessOrdNum   - Supplies Process Info ordinal number / Index
    ThreadOrdNum    - Supplies Thread Info ordinal number / Index
    NbMaxOutByte    - Supplies maximum number of bytes that can be written in output buffer

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL MarshalThreadInfoData (IN OUT PCHAR OutBuf,
                            IN OUT PUSHORT pOutBufIndex,
                            IN DWORD ProcessOrdNum,
                            IN DWORD ThreadOrdNum,
                            IN USHORT NbMaxOutByte)

{
    PTHREAD     pThread;
    USHORT      FieldIdx;
    USHORT      FieldSize = 0;
    WORD        ThreadState;
    WORD        ThreadInfo;
    BOOL        Result;
    DWORD       aKey;
    DWORD       dwQuantum;
    BYTE        bCPrio;

    pThread = kdProcArray [ProcessOrdNum].pTh; // Get process main thread (this is index 0)
    while (ThreadOrdNum--)
    { // walk list of threads attached to process (until we reach the index ThreadOrdNum)
        pThread = pThread->pNextInProc;
    }

    // FieldSize = SUM (ThreadDescriptorTable [0..THREAD_DESC_NB_FIELDS[.wSize)
    for (FieldIdx = 0; FieldIdx < THREAD_DESC_NB_FIELDS; FieldIdx++)
    {
        FieldSize += ThreadDescriptorTable [FieldIdx].wSize;
    }

    if (FieldSize <= NbMaxOutByte)
    { // Remaining buffer large enough for all thread fields
        /////////////////////////////////////////
        // Insert Thread Info Data fields now: //
        /////////////////////////////////////////
        AppendObjToOutBuf_M (OutBuf,    pThread,                            *pOutBufIndex); // tfiStructAddr
        ThreadState = (WORD)((pThread->wInfo & ((1<<DYING_SHIFT)|(1<<DEAD_SHIFT)|(1<<BURIED_SHIFT)|(1<<SLEEPING_SHIFT))) << 2) + (1 << GET_RUNSTATE (pThread));
        AppendObjToOutBuf_M (OutBuf,    ThreadState,                        *pOutBufIndex); // tfiRunState
        ThreadInfo = (WORD)(pThread->wInfo & ((1<<TIMEMODE_SHIFT)|(1<<STACKFAULT_SHIFT)|(1<<USERBLOCK_SHIFT)|(1<<PROFILE_SHIFT)));
        AppendObjToOutBuf_M (OutBuf,    ThreadInfo,                         *pOutBufIndex); // tfiInfo
        AppendObjToOutBuf_M (OutBuf,    pThread->hTh,                       *pOutBufIndex); // tfiHandle
        AppendObjToOutBuf_M (OutBuf,    pThread->bWaitState,                *pOutBufIndex); // tfiWaitState
        if (pThread == pCurThread)
        { // WARNING - SPECIAL CASE: If thread is current thread we need to use saved value instead
            aKey = g_ulOldKey;
        }
        else
        {
            aKey = pThread->aky;
        }
        AppendObjToOutBuf_M (OutBuf,    aKey,                               *pOutBufIndex); // tfiAddrSpaceAccessKey
        AppendObjToOutBuf_M (OutBuf,    pThread->pProc->hProc,              *pOutBufIndex); // tfiHandleCurrentProcessRunIn
        AppendObjToOutBuf_M (OutBuf,    pThread->pOwnerProc->hProc,         *pOutBufIndex); // tfiHandleOwnerProc
        if ((pThread == pCurThread) && (INVALID_PRIO != *pcSavePrio))
        { // WARNING - SPECIAL CASE: If thread is current thread we need to use saved value instead
            bCPrio = *(BYTE *) pcSavePrio;
        }
        else
        {
            bCPrio = pThread->bCPrio;
        }
        AppendObjToOutBuf_M (OutBuf,    bCPrio,                             *pOutBufIndex); // tfiCurrentPriority        
        AppendObjToOutBuf_M (OutBuf,    pThread->bBPrio,                    *pOutBufIndex); // tfiBasePriority
        AppendObjToOutBuf_M (OutBuf,    pThread->dwKernTime,                *pOutBufIndex); // tfiKernelTime
        AppendObjToOutBuf_M (OutBuf,    pThread->dwUserTime,                *pOutBufIndex); // tfiUserTime
        if ((pThread == pCurThread) && (INVALID_PRIO != *pcSavePrio))
        { // WARNING - SPECIAL CASE: If thread is current thread we need to use saved value instead
            dwQuantum = *pdwSaveQuantum;
        }
        else
        {
            dwQuantum = pThread->dwQuantum;
        }
        AppendObjToOutBuf_M (OutBuf,    dwQuantum,                          *pOutBufIndex); // tfiQuantum
        AppendObjToOutBuf_M (OutBuf,    pThread->dwQuantLeft,               *pOutBufIndex); // tfiQuantumLeft
        AppendObjToOutBuf_M (OutBuf,    pThread->dwWakeupTime,              *pOutBufIndex); // tfiSleepCount
        AppendObjToOutBuf_M (OutBuf,    pThread->bSuspendCnt,               *pOutBufIndex); // tfiSuspendCount
        AppendObjToOutBuf_M (OutBuf,    pThread->tlsPtr,                    *pOutBufIndex); // tfiTlsPtr
        AppendObjToOutBuf_M (OutBuf,    pThread->dwLastError,               *pOutBufIndex); // tfiLastError
        AppendObjToOutBuf_M (OutBuf,    KSTKBASE(pThread),                  *pOutBufIndex); // tfiStackBase
        AppendObjToOutBuf_M (OutBuf,    KSTKBOUND(pThread),                 *pOutBufIndex); // tfiStackLowBound
        AppendObjToOutBuf_M (OutBuf,    pThread->ftCreate.dwHighDateTime,   *pOutBufIndex); // tfiCreationTimeMSW
        AppendObjToOutBuf_M (OutBuf,    pThread->ftCreate.dwLowDateTime,    *pOutBufIndex); // tfiCreationTimeLSW        
        Result = TRUE;
    }
    else
    { // Buffer not large enough: exit with error
        Result = FALSE;
    }
    
    return Result;
} // End of MarshalThreadInfoData


// Tags:
typedef enum _PROC_THREAD_INFO_TAGS
{
    PtitagsStartProcessInfoDescFieldV1,
    PtitagsStartThreadInfoDescFieldV1,
    PtitagsStartProcessInfoDataV1,
    PtitagsStartThreadInfoDataV1,
    PtitagsExtraRawString
} PROC_THREAD_INFO_TAGS; // Must have 256 values max (stored on a byte)

// Sub-services masks:
#define PtissmskProcessInfoDescriptor       (0x00000001uL)
#define PtissmskThreadInfoDescriptor        (0x00000002uL)
#define PtissmskProcessInfoData             (0x00000004uL)
#define PtissmskThreadInfoData              (0x00000008uL)
#define PtissmskAllExtraRawStrings          (0x00000010uL)

// Range of DWORD
typedef struct _DW_RANGE
{
    DWORD   dwFirst;
    DWORD   dwLast;
} DW_RANGE;

// Parameter structure
typedef struct _PROC_THREAD_INFO_PARAM
{
    DWORD       dwSubServiceMask; // 0..15
    DW_RANGE    dwrProcessOrdNum; // Ordinal number (not id) range (optional)
    DW_RANGE    dwrThreadOrdNum; // Ordinal number (not id) range (optional)
    CHAR        szExtraParamInfo []; // optional parameter string (open for future extensions)
} PROC_THREAD_INFO_PARAM, *PPROC_THREAD_INFO_PARAM;

// Return status:
typedef enum _PROC_THREAD_INFO_RETURN_STATUS
{
    PtirstOK,
    PtirstResponseBufferTooShort,
    PtirstInvalidInputParamValues,
    PtirstInvalidInputParamCount
} PROC_THREAD_INFO_RETURN_STATUS; // Must have 256 values max (stored on a byte)

#define SetReturnStatus_M(retstat) pParamBuf->Buffer [0] = (BYTE) (retstat);

CHAR TargetSideParmHelpString [] =
"Target Side Parameter usage: [?] | [cs]\n"\
" * ?                -> this help\n"\
" * cs               -> show Critical Section Info (and any synchronization object status)\n"\
" * ior address,size      -> read IO from 'address' for 'size' bytes (size = 1,2, or 4)\n"\
" * iow address,size,data -> write IO 'data' to 'address' for 'size' bytes (size = 1,2, or 4)\n"\
"\n";


kdbgcmpAandW(LPSTR pAstr, LPWSTR pWstr) {
    while (*pAstr && (*pAstr == (BYTE)*pWstr)) {
        pAstr++;
        pWstr++;
    }
    return (!*pWstr ? 0 : 1);
}

/*++

Routine Name:

    GetProcessAndThreadInfo

Routine Description:

    Handle the Process/Thread information request:
    Depending on incoming parameters, feed back with
    Process and / or Thread Information Descriptor or / and Process or / and  Thread Information Data
    in full blocks or in smaller pieces (per process / per thread)

Arguments:

    ParamBuf - Supplies and returns pointer to STRING (size + maxsize + raw buffer)

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL GetProcessAndThreadInfo (IN PSTRING pParamBuf)

{
    PROC_THREAD_INFO_PARAM  InpParams;
    USHORT                  InpParamLength;
    USHORT                  OutBufIndex = 1; // Skip return status (1st byte)
    DWORD                   dwProcessIdx, dwThreadIdx, dwLastThread;
    BOOL                    fProcessRangeGiven = FALSE;
    BOOL                    fThreadRangeGiven = FALSE;
    USHORT                  CurrentBlockEndReannotateIdx;
    BOOL                    RequestHelpOnTargetSideParam = FALSE;
    CHAR                    ProcName [32];
    BOOL                    fDisplayCriticalSections = FALSE;
    BOOL                    fIOR = FALSE;
    BOOL                    fIOW = FALSE;
    BOOL                    fSkipProcessBlock = FALSE;
    DWORD                   dwIOAddress = 0;
    DWORD                   dwIOSize = 0;
    DWORD                   dwIOData = 0;

    DEBUGGERMSG(KDZONE_DBG, (L"++FlexiPTI (Len=%i,MaxLen=%i)\r\n", pParamBuf->Length, pParamBuf->MaximumLength));
    InpParamLength = pParamBuf->Length;
    pParamBuf->Length = 1; // Default returned length is one for status byte
    if (InpParamLength < sizeof (DWORD)) // at least 1 parameter ?
    { // size not appropriate: exit with error
        SetReturnStatus_M (PtirstInvalidInputParamCount);
        return FALSE;
    }

    ProcName [0] = 0; // By default: no filtering on Process Name

    //
    // Parse optional parameter string
    //

    if (InpParamLength > (sizeof (DWORD) + sizeof (DW_RANGE) * 2))
    {// any bytes in optional Param Info String? Yes
        CHAR * szExtraParamInfo = ((PPROC_THREAD_INFO_PARAM) (pParamBuf->Buffer))->szExtraParamInfo;

        if (strncmp (szExtraParamInfo, "?", 1) == 0)
        { // Return help string on Target side parameters
            RequestHelpOnTargetSideParam = TRUE;
            fSkipProcessBlock = TRUE;
        }

        


        if (strncmp (szExtraParamInfo, "cs", 2) == 0)
        { // Display critical section information
            fDisplayCriticalSections = TRUE;
            fSkipProcessBlock = TRUE;
        }

        if (strncmp (szExtraParamInfo, "procname=", 9) == 0)
        { // Return only Process with ProcName
            strncpy (ProcName, szExtraParamInfo + 9, 31);
            ProcName [31] = 0;
        }

        if (strncmp (szExtraParamInfo, "ior", 3) == 0)
        { // Read I/O
            fIOR = TRUE;
            fSkipProcessBlock = TRUE;
        }

        if (strncmp (szExtraParamInfo, "iow", 3) == 0)
        { // Write I/O
            fIOW = TRUE;
            fSkipProcessBlock = TRUE;
        }

        if (fIOR || fIOW)
        {
            CHAR * szIOParam = szExtraParamInfo+3;
           
            dwIOAddress = (DWORD)Kdstrtoul(szIOParam,0);
            while ((*szIOParam) && (*szIOParam++ != ',')) ;
            if ((*szIOParam))
            {
                dwIOSize = (DWORD)Kdstrtoul(szIOParam,0);
                if (fIOW)
                {
                    while ((*szIOParam) && (*szIOParam++ != ',')) ;
                    if ((*szIOParam))
                    {
                        dwIOData = (DWORD)Kdstrtoul(szIOParam,0);
                    }
                }
            }
        }
    }
    
    // NOTE: Parse your Extra Parameters here

    memcpy (&InpParams, pParamBuf->Buffer, sizeof (InpParams)); // Save the input parameters

    // Warning: before this point do not write to pParamBuf->Buffer (input params not saved)

    SetReturnStatus_M (PtirstResponseBufferTooShort); // Default return status is buffer too short (common error)

    //
    // Provide all Extra Raw Strings
    //
    if (InpParams.dwSubServiceMask & PtissmskAllExtraRawStrings)
    { // Provide Extra Raw Strings
        DEBUGGERMSG(KDZONE_DBG, (L"FlexiPTI: Provide Extra Raw String (OutBufIdx=%i)\r\n", OutBufIndex));
        if (RequestHelpOnTargetSideParam)
        { // Help on Target side param requested:
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
            { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                            PtitagsExtraRawString,
                                            OutBufIndex);
                CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                OutBufIndex += 2; // skip size (will be written at the end of the block)
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (TargetSideParmHelpString)))
            {
                AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                            TargetSideParmHelpString,
                                            OutBufIndex);
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }
            
            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
       }

        if (fDisplayCriticalSections)
        { 

            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
            { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                            PtitagsExtraRawString,
                                            OutBufIndex);
                CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                OutBufIndex += 2; // skip size (will be written at the end of the block)
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            if (!MarshalCriticalSectionInfo (   pParamBuf->Buffer,
                                                &OutBufIndex,
                                                (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
        }

        if (fIOR || fIOW)
        { 

            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
            { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                            PtitagsExtraRawString,
                                            OutBufIndex);
                CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                OutBufIndex += 2; // skip size (will be written at the end of the block)
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            if (!MarshalIOReadWrite (   pParamBuf->Buffer,
                                        &OutBufIndex,
                                        (USHORT)(pParamBuf->MaximumLength - OutBufIndex),
                                        fIOR,
                                        dwIOAddress,
                                        dwIOSize,
                                        dwIOData))
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
        }
        
        // NOTE: Add your Extra Raw Strings here

    }

    // Refresh return buffer size (return at least Process Info Descriptor if it was requested)
    pParamBuf->Length = OutBufIndex;

    if (!fSkipProcessBlock) // Bypass all the other blocks if required by optional parameter
    {
        //
        // Provide Process Info Descriptor
        //
        if (InpParams.dwSubServiceMask & PtissmskProcessInfoDescriptor)
        { // Provide Process Info Descriptor
            DEBUGGERMSG(KDZONE_DBG, (L"FlexiPTI: Provide Process Info Descriptor (OutBufIdx=%i)\r\n", OutBufIndex));
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
            { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                            PtitagsStartProcessInfoDescFieldV1,
                                            OutBufIndex);
                CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                OutBufIndex += 2; // skip size (will be written at the end of the block)
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }
            if (!MarshalDescriptionTable (  pParamBuf->Buffer,
                                            &OutBufIndex,
                                            ProcessDescriptorTable,
                                            PROC_DESC_NB_FIELDS,
                                            (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
        }

        // Refresh return buffer size (return at least Process Info Descriptor if it was requested)
        pParamBuf->Length = OutBufIndex;

        //
        // Provide Thread Info Descriptor
        //
        if (InpParams.dwSubServiceMask & PtissmskThreadInfoDescriptor)
        { // Provide Thread Info Descriptor
            DEBUGGERMSG(KDZONE_DBG, (L"FlexiPTI: Provide Thread Info Descriptor (OutBufIdx=%i)\r\n", OutBufIndex));
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
            { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                            PtitagsStartThreadInfoDescFieldV1,
                                            OutBufIndex);
                CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                OutBufIndex += 2; // skip size (will be written at the end of the block)
            }
            else
            { // Buffer not large enough: exit with error
                return FALSE;
            }
            if (!MarshalDescriptionTable (  pParamBuf->Buffer,
                                            &OutBufIndex,
                                            ThreadDescriptorTable,
                                            THREAD_DESC_NB_FIELDS,
                                            (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
            { // Buffer not large enough: exit with error
                return FALSE;
            }

            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
        }

        // Refresh return buffer size (return at least Process and Thread Info Descriptor if it was requested)
        pParamBuf->Length = OutBufIndex;

        //
        // Provide Process and (optionally) Thread Info Data
        //
        if (InpParams.dwSubServiceMask & PtissmskProcessInfoData)
        {
            // Check parameters
            if (InpParamLength >= (sizeof (DWORD) + sizeof (DW_RANGE))) // at least 2 parameter ?
            {
                fProcessRangeGiven = TRUE;
            }
            if (InpParamLength >= (sizeof (DWORD) + 2 * sizeof (DW_RANGE))) // at least 3 parameter ?
            {
                fThreadRangeGiven = TRUE;
            }

            if (!fProcessRangeGiven)
            { // Default filling of Process index Range
                InpParams.dwrProcessOrdNum.dwFirst = 0uL;
                InpParams.dwrProcessOrdNum.dwLast = MAX_PROCESSES;
            }
            else
            { // Process index range passed as parameter: do overflow cliping
                // Clip InpParams.dwrProcessOrdNum.dwLast to (MAX_PROCESSES - 1) maximum
                if (InpParams.dwrProcessOrdNum.dwLast > (MAX_PROCESSES - 1))
                {
                    InpParams.dwrProcessOrdNum.dwLast = MAX_PROCESSES - 1;
                }
            }

            if (!fThreadRangeGiven)
            { // Default filling of Thread index Range
                InpParams.dwrThreadOrdNum.dwFirst = 0uL;
                InpParams.dwrThreadOrdNum.dwLast = 0xFFFFFFFFuL;
            }

            for (   dwProcessIdx = InpParams.dwrProcessOrdNum.dwFirst;
                    dwProcessIdx <= InpParams.dwrProcessOrdNum.dwLast;
                    dwProcessIdx++)
            { // loop thru process range [InpParams.dwrProcessOrdNum.dwFirst..
              //                          Min(InpParams.dwrProcessOrdNum.dwLast,(MAX_PROCESSES - 1))]
                if (kdProcArray [dwProcessIdx].dwVMBase)
                { // Only if VM Base Addr of process is valid (not null):
                    if ((ProcName [0] == 0) || (kdbgcmpAandW(ProcName, kdProcArray[dwProcessIdx].lpszProcName) == 0))
                    { // continue if no filter on ProcName or if ProcName is OK
                        DEBUGGERMSG(KDZONE_DBG, (L"FlexiPTI: Provide Process Data (slot# = %i, OutBufIdx=%i)\r\n", dwProcessIdx, OutBufIndex));
                        if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
                        { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                            AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                                        PtitagsStartProcessInfoDataV1,
                                                        OutBufIndex); // Insert Start Process Info Data TAG
                            CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                            OutBufIndex += 2; // skip size (will be written at the end of the block)
                        }
                        else
                        { // Buffer not large enough: exit with error
                            return FALSE;
                        }
                        if (pParamBuf->MaximumLength >= (OutBufIndex + sizeof (dwProcessIdx)))
                        { // enough space left in buffer
                            AppendObjToOutBuf_M (   pParamBuf->Buffer,
                                                    dwProcessIdx,
                                                    OutBufIndex); // Insert Process Index number
                        }
                        else
                        { // Buffer not large enough: exit with error
                            return FALSE;
                        }
                        if (!MarshalProcessInfoData (   pParamBuf->Buffer,
                                                        &OutBufIndex,
                                                        dwProcessIdx,
                                                        (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
                        { // Buffer not large enough: exit with error
                            return FALSE;
                        }

                        // Reannotate end position of block just after block tag
                        memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));

                        //
                        // Provide Thread Info Data
                        //
                        if (InpParams.dwSubServiceMask & PtissmskThreadInfoData)
                        {
                            dwThreadIdx = GetNumberOfThreadsAttachedToProc (dwProcessIdx);
                            dwLastThread = (InpParams.dwrThreadOrdNum.dwLast > (dwThreadIdx - 1)) ? (dwThreadIdx - 1) : InpParams.dwrThreadOrdNum.dwLast; // last thread is min (NumberOfThread, ParamLastThread)
                            for (   dwThreadIdx = InpParams.dwrThreadOrdNum.dwFirst;
                                    dwThreadIdx <= dwLastThread;
                                    dwThreadIdx++)
                            { // loop thru process range [InpParams.dwrThreadOrdNum.dwFirst..InpParams.dwrThreadOrdNum.dwLast]
                                DEBUGGERMSG(KDZONE_DBG, (L"FlexiPTI: Provide Thread Data (index# = %i,OutBufIdx=%i)\r\n", dwThreadIdx, OutBufIndex));
                                if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
                                { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
                                    AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                                                PtitagsStartThreadInfoDataV1,
                                                                OutBufIndex); // Insert Start Thread Info Data TAG
                                    CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
                                    OutBufIndex += 2; // skip size (will be written at the end of the block)
                                }
                                else
                                { // Buffer not large enough: exit with error
                                    return FALSE;
                                }
                                if (pParamBuf->MaximumLength >= (OutBufIndex + sizeof (dwThreadIdx)))
                                { // enough space left in buffer
                                    AppendObjToOutBuf_M (   pParamBuf->Buffer,
                                                            dwThreadIdx,
                                                            OutBufIndex); // Insert Thread Index number
                                }
                                else
                                { // Buffer not large enough: exit with error
                                    return FALSE;
                                }
                                if (!MarshalThreadInfoData (pParamBuf->Buffer,
                                                            &OutBufIndex,
                                                            dwProcessIdx,
                                                            dwThreadIdx,
                                                            (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
                                { // Buffer not large enough: exit with error
                                    return FALSE;
                                }

                                // Reannotate end position of block just after block tag
                                memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
                            }
                        }
                    }

                    // Refresh return buffer size (return at least previous blocks)
                    pParamBuf->Length = OutBufIndex;
                }
            }
        }
    }

    pParamBuf->Length = OutBufIndex; // defensive refresh of return buffer size (should be already OK)
    SetReturnStatus_M (PtirstOK);
    DEBUGGERMSG(KDZONE_DBG, (L"--FlexiPTI (OK - OutBufIdx=%i)\r\n", OutBufIndex));
    return TRUE;
} // End of GetProcessAndThreadInfo


PVOID SafeDbgVerify(PVOID pvAddr, int option) 
{
    DWORD dwLastError;
    PVOID pvRet;
    
    dwLastError = KGetLastError(pCurThread);

    // Only do something if no one (not even the current thread) is holding VAcs
    // if (0 == VAcs.OwnerThread)
    // {
        pvRet = g_kdKernData.pDbgVerify(pvAddr, option);
    // }
    // else
    // {
    //     DEBUGGERMSG(KDZONE_ALERT, (L"  WARNING: VAcs is taken; debugger functionality will be reduced\r\n"));
    //     pvRet = 0;
    // }

    KSetLastError(pCurThread, dwLastError);

    return pvRet;
}


LPMODULE ModuleFromAddress(DWORD dwAddr)
{
    DWORD dwAddrZeroed;  // Address zero-slotized
    LPMODULE pMod;

    // Zero slotize
    dwAddrZeroed = ZeroPtr(dwAddr);

    // Walk module list to find this address
    pMod = pModList;
    while (pMod)
    {
        if (
            // User dll
            ((dwAddrZeroed >= ZeroPtr ((DWORD)pMod->BasePtr)) &&
             (dwAddrZeroed <  ZeroPtr ((DWORD)pMod->BasePtr + pMod->e32.e32_vsize))) ||
            // Kernel dll
            (IsKernelVa (pMod->BasePtr) &&
             (dwAddr >= (DWORD)pMod->BasePtr) &&
             (dwAddr <  (DWORD)pMod->BasePtr + pMod->e32.e32_vsize)) ||
            // Additional relocated RW data section
            ((dwAddrZeroed >= ZeroPtr ((DWORD)pMod->rwLow)) &&
             (dwAddrZeroed < ZeroPtr ((DWORD)pMod->rwHigh)))
           )
        {
		    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: Found in Dll (%s)\r\n", pMod->lpszModName));
        	break;
		}
        
        pMod=pMod->pMod;
    }

    return pMod;
}


PROCESS *g_pFocusProcOverride = NULL;


ULONG VerifyAddress(PVOID Addr)
{
    ULONG ret=0;
    ULONG loop;
    PROCESS *pFocusProc = pCurProc;

    DEBUGGERMSG(KDZONE_VERIFY, (L"++VerifyAddress (0x%08X)\r\n", Addr));

	// Map TOC Copy entrie source address range to destination address range
    for (loop = 0; loop < pTOC->ulCopyEntries; loop++)
    { // Note: This should not be necessary
	    COPYentry *cptr;
	    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: pTOC entry=%i\r\n", loop));
        cptr = (COPYentry *)(pTOC->ulCopyOffset + loop*sizeof(COPYentry));
        if ((ULONG)Addr >= cptr->ulSource && (ULONG)Addr < cptr->ulSource+cptr->ulCopyLen)
        {
            ret = (((ULONG)Addr)-cptr->ulSource+cptr->ulDest);
		    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: found in PTOC entry\r\n"));
            break;
        }
    }

    if (!ret)
    {
        if (((ULONG)Addr >= dwKStart) && ((ULONG)Addr <= dwKEnd) && ((ULONG)Addr != (ULONG)-1))
	    { // Note: This should not be necessary either
		    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: found in relocated Kernel range\r\n"));
            ret = (ULONG)Addr-dwKStart+dwNewKStart;
        }
        else 
        {
		    PVOID Temp=0;
		    LPMODULE lpMod;
		    PCALLSTACK  pCurStk;
			if (((ULONG) Addr >= (DWORD)DllLoadBase) && ((ULONG) Addr < (2 << VA_SECTION)))
            { // Addr is in a DLL
			    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: found in dll\r\n"));
                lpMod = ModuleFromAddress ((ULONG) Addr);
                if (lpMod)
                {                    
                    // We now have the module.  Loop through to find out what section
                    // this address is in.  If it is in a read only section map it into
                    // the zero slot.  Otherwise try to find a process space to map it into.
                    for (loop=0; loop < lpMod->e32.e32_objcnt; loop++)
                    {
                        if ((ZeroPtr(Addr) >= ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)) &&
                            (ZeroPtr(Addr) < ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)+lpMod->o32_ptr[loop].o32_vsize))
                        {
                            if ((lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_READ) &&
                                 !(lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_WRITE))
                            { // This is the read only section, map the pointer in NK
							    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: read only section, map in NK (VM nk = %08X)\r\n", kdProcArray->dwVMBase));
                                Temp = MapPtrProc(Addr,kdProcArray);
                            } 
                            else 
                            { // Read/Write section:
                                //now have to search for the process to map the pointer into.
                                if (g_pFocusProcOverride) // Context Override in use and DLL Used by Context override Process
                                {                          	
                                	pFocusProc = g_pFocusProcOverride;
								    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: read/write section, map in ProcOverride (VM=%08X)\r\n", pFocusProc->dwVMBase));
                                }
                                else
                                {
								    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: read/write section, map in CurProc (VM=%08X)\r\n", pFocusProc->dwVMBase));
                                }
                                
                                if ((lpMod->inuse & pFocusProc->aky) // DLL Used by current process
                                        || !(pCurThread->pcstkTop)) 
                                { // Map in current process
                                    Temp = (PVOID)((DWORD)Addr|pFocusProc->dwVMBase);
								    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: DLL used by process\r\n"));
                                } 
                                else 
                                { // RW section but not used by current process (PSL case):
								    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: DLL not used by process (walk back stack lists)\r\n"));
                                    pCurStk=pCurThread->pcstkTop;
                                    while (pCurStk) 
                                    { // Walk back all Cross-process calls on current thread to find process using that dll
                                        if ((pCurStk->pprcLast != (PPROCESS)USER_MODE) && (pCurStk->pprcLast != (PPROCESS)KERNEL_MODE)) 
                                        {
                                            if (lpMod->inuse & pCurStk->pprcLast->aky)
                                            {
                                                Temp = MapPtrInProc (Addr, pCurStk->pprcLast);
											    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: map in pCurStk->pprcLast (VM=%08X)\r\n", pCurStk->pprcLast->dwVMBase));
                                                break;
                                            }
                                        }
                                        pCurStk=pCurStk->pcstkNext;
                                    }
                                    if (!pCurStk && !Temp)
                                    { // Not found, Map in NK by default then
                                        Temp = MapPtrInProc(Addr, kdProcArray);
									    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: Not found, map in NK (VM=%08X)\r\n", kdProcArray->dwVMBase));
                                    }
                                }
                            }
                        }
                    }
                }
                else
                { // Map in NK
	                Temp = MapPtrInProc(Addr,kdProcArray);
				    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: Unknown module, map in NK (VM=%08X)\r\n", kdProcArray->dwVMBase));
                }
            } 
			else if (((ULONG) Addr < (DWORD)DllLoadBase) && g_pFocusProcOverride)
            { // Addr is in Slot0 Proc and override ON
                Temp = (PVOID)((DWORD)Addr|g_pFocusProcOverride->dwVMBase);
			    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: Slot 0 process space, map in ProcOverride (VM=%08X)\r\n", g_pFocusProcOverride->dwVMBase));
			}
            else
            {
			    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: found process space\r\n"));
                Temp = Addr;
            }
		    DEBUGGERMSG(KDZONE_VERIFY, (L"  VerifyAddress: Temp = %08X\r\n", Temp));
            ret = (ULONG)SafeDbgVerify(Temp, 0);
        }
    }
    DEBUGGERMSG (KDZONE_VERIFY, (L"--VerifyAddress (ret=0x%08X)\r\n", ret));

    return ret;
}


BOOL TranslateRA (/* IN/OUT */ DWORD *pRA, 
                     /* OUT */ DWORD *pSP,
                     /* IN */ DWORD dwStackFrameAddr,
                     /* OUT */ DWORD *pdwProcHandle)
{
    BOOL fRet = TRUE;
    ULONG ulOldRA;
    ULONG ulNewRA;
    ULONG ulTemp;
    DWORD dwPrevSP;
    DWORD dwOffsetSP;

    DEBUGGERMSG(KDZONE_STACKW, (L"++TranslateRA (pRA=%8.8lX, dwFrameAddr=%8.8lX)\r\n", pRA, dwStackFrameAddr));
    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA pStk=%8.8lX, pCThd=%8.8lX, pWlkThd=%8.8lX, pLstPrc=%8.8lX\r\n", pStk, pCurThread, pWalkThread, pLastProc));

    __try
    {
        if (!pRA || !pSP || !pdwProcHandle)
        {
            DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA ERROR: Invalid Parameter\r\n"));
	        fRet = FALSE;
        }
        else
        {
        	*pSP = 0; // default is 0 (for "to be ignored")
            ulNewRA = ulOldRA  = *pRA;

            *pdwProcHandle = (DWORD)pLastProc->hProc;
            
            if (!ulOldRA)
            {
                DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA (in RA = NULL), no more frames\r\n"));
            }
            else
            {
                DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA (in RA = %8.8lX)\r\n", ulOldRA));

                if (((SYSCALL_RETURN == ulOldRA) || (DIRECT_RETURN  == ulOldRA) || ((DWORD) MD_CBRtn == ulOldRA)) &&
                    pStk)
                { 
                    // PSL IPC style: Check for mode not process

                    dwOffsetSP = (pStk->dwPrcInfo & CST_CALLBACK) ? CALLEE_SAVED_REGS : 0;
                    dwPrevSP = pStk->dwPrevSP ? (pStk->dwPrevSP + dwOffsetSP) : 0;

                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA PSL\r\n"));
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA pprcLast=%8.8lX, retAddr=%8.8lX, pcstkNext=%8.8lX dwPrevSP=%8.8lX dwOffsetSP=%8.8lX\r\n", pStk->pprcLast, pStk->retAddr, pStk->pcstkNext, pStk->dwPrevSP, dwOffsetSP));

                    *pSP = dwPrevSP;

                    if ((DWORD)pStk->pprcLast > 0x10000uL)
                    {
                        pLastProc = pStk->pprcLast;
                    }

                    ulTemp = (ULONG)pStk->retAddr;
                    ulNewRA = ZeroPtr(ulTemp);
                    pStk = pStk->pcstkNext;
                }
                else if (dwStackFrameAddr &&
                         pStk && 
                         ((DWORD)pStk < dwStackFrameAddr) && // Stack Frame Base pointer is now above cstk var (on the stack) so we changed process
                         !pStk->retAddr // Extra check (optional) that ret is NULL
                        )
                {
                    // New IPC style (SC_PerformCallBack4)
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA Callback4 RA=%8.8lX\r\n", ulOldRA));
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA pprcLast=%8.8lX, pcstkNext=%8.8lX\r\n", pStk->pprcLast, pStk->pcstkNext));

                    pLastProc = pStk->pprcLast;
                    ulTemp    = ulOldRA;
                    ulNewRA   = ZeroPtr(ulTemp);
                    pStk      = pStk->pcstkNext;        
                }
                else
                {
                    // Normal case:
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA Normal (in RA = out RA)\r\n"));
                }

                DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA "));
                if ((ulNewRA > (1 << VA_SECTION)) ||
                    (ulNewRA < (ULONG)DllLoadBase))
                {
                    // Address from a EXE: Slotize it to its proper process
                    DEBUGGERMSG(KDZONE_STACKW, (L"EXE: mapinproc\r\n"));

                    if (pLastProc)
                    {
                        ulNewRA = (ULONG)MapPtrInProc(ulNewRA, pLastProc);
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA *** ERROR pLastProc is NULL\r\n"));
                    }
                }
                else
                {
                    // Address from a DLL: Zero-slotize it because the debugger has only a zero-slot address of it
                    DEBUGGERMSG(KDZONE_STACKW, (L"DLL: zeroslotize\r\n"));

                    ulNewRA = (ULONG)ZeroPtr(ulNewRA);
                }

                *pRA = ulNewRA;
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA Old=%8.8lX -> New=%8.8lX\r\n", ulOldRA, ulNewRA));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA *** EXCEPTION ***\r\n"));

        fRet = FALSE;
    }

    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA pStk=%8.8lX, pCThd=%8.8lX, pWlkThd=%8.8lX, pLstPrc=%8.8lX\r\n", pStk, pCurThread, pWalkThread, pLastProc));
    DEBUGGERMSG(KDZONE_STACKW, (L"--TranslateRA (%s)\r\n", fRet ? L"TRUE" : L"FALSE"));
    
    return fRet;
}



BOOL TranslateAddress(PULONG lpAddr, PCONTEXT pCtx)
{
    ULONG ret;

    DEBUGGERMSG(KDZONE_DBG,(L"Getting jump location for %8.8lx (%8.8lx)\r\n", *lpAddr, pCurThread));

    if (*lpAddr == SYSCALL_RETURN || *lpAddr==DIRECT_RETURN) {
        ret = (DWORD)DBG_ReturnCheck(pCurThread);
        DEBUGGERMSG(KDZONE_DBG, (TEXT("DBG_ReturnCheck returned %8.8lx\r\n"), ret));
        if (ret) {
            if ((ZeroPtr(ret) > (1 << VA_SECTION)) || ZeroPtr(ret) < (DWORD)DllLoadBase)
                ret=(DWORD)MapPtrProc(ret, pCurThread->pcstkTop->pprcLast);
        }
        DEBUGGERMSG(KDZONE_DBG, (TEXT("Patching syscall return jump to %8.8lx\r\n"), ret));
    } else {
        ret = (ULONG)g_kdKernData.pDBG_CallCheck(pCurThread, *lpAddr, pCtx);
        DEBUGGERMSG(KDZONE_DBG, (TEXT("DBG_CallCheck returned %8.8lx\r\n"), ret));
        if (ret) {
            if ((ZeroPtr(ret) > (1 << VA_SECTION)) || ZeroPtr(ret) < (DWORD)DllLoadBase)
                ret=(DWORD)MapPtrProc(ret, pCurThread->pcstkTop->pprcLast);
            DEBUGGERMSG(KDZONE_DBG, (TEXT("Patching CallCheck to %8.8lx\r\n"), ret));
        } else {
            ret = *lpAddr;
            if ((ZeroPtr(ret) > (1 << VA_SECTION)) || ZeroPtr(ret) < (DWORD)DllLoadBase)
                ret=(DWORD)MapPtr(ret);
            DEBUGGERMSG(KDZONE_DBG, (TEXT("Patching Jump to %8.8lx\r\n"), ret));
        }
    }

   *lpAddr = ret;
    return TRUE;
}


#if 0

static PTHREAD pBreakThd;
static DWORD   BreakPC;
static DWORD   BreakMode;


BOOL SwitchToThread(PTHREAD pThd)
{
    //Check globals to find out if there is already one of these queued up.
    if (pBreakThd) {
        //If so clear old
        SetThreadIP(pBreakThd, BreakPC);
        SetThreadMode(pBreakThd, BreakMode);
    }

    //Store PC, mode and SR in global variables
    pBreakThd=pThd;
    BreakPC = GetThreadIP(pBreakThd);
    BreakMode = GetThreadMode(pBreakThd);

    //Set Thread to kernel mode
    SetThreadMode(pBreakThd, KERNEL_MODE);

    //Set PC to ThreadStopFunc
    SetThreadIP(pBreakThd, ThreadStopFunc);
    return TRUE;
}

void ClearThreadSwitch(PCONTEXT pCtx)
{
    //Check globals to find out if there is already one of these queued up.
    if (pBreakThd) {
        //If so clear old
        SetThreadIP(pBreakThd, BreakPC);
        SetThreadMode(pBreakThd, BreakMode);
        SetContextMode(pCtx, BreakMode);

        if ((ZeroPtr(BreakPC) > (1 << VA_SECTION)) || ZeroPtr(BreakPC) < (DWORD)DllLoadBase)
            BreakPC = (UINT)MapPtrProc(BreakPC, pCurProc);
        CONTEXT_TO_PROGRAM_COUNTER(pCtx) = BreakPC;
    }

    pBreakThd=0;
    BreakPC=0;
    BreakMode=0;
}

#endif

ULONG ByteToNum(BYTE b)
{
    switch(b) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '0':
        return b - '0';
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
        return b -'A' + 10;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
        return b - 'a' + 10;
    default:
        return 0xffffffff;
    }

    return 0;
}
        
ULONG Kdstrtoul(LPBYTE lpStr, ULONG radix)
{ // This function will parse lpStr until an invalid digit or end of string found
    CHAR c;
    ULONG Ret=0;

    // Only support radix 2 to 16 and 0 for auto
    if (radix != 1 && radix <= 16)
    {
        c = *lpStr++;

        // Skip whitespace
        while ((c==0x20) || (c>=0x09 && c<=0x0D))
        {
            c = *lpStr++;
        }

        // Auto detect radix if radix == 0
        if (!radix)
        {
            radix = (c != '0') ? 10 : ((*lpStr == 'x' || *lpStr == 'X')) ? 16 : 8;
        }

        // Skip Hex prefix if present
        if ((radix == 16) && (c == '0') && (*lpStr == 'x' || *lpStr == 'X'))
        {
            ++lpStr;
            c = *lpStr++;
        }

        // Convert string to number
        while (c)
        {
            DWORD dwTemp = ByteToNum(c);

            // Check for valid digits
            if (dwTemp <= radix)
            {
                Ret = (Ret * radix) + dwTemp;
                c = *lpStr++;
            }
            else
            {
                // Exit if invalid
                c = 0;
            }
        }
    }
  
    return Ret;
}

#if defined(x86)

EXCEPTION_DISPOSITION __cdecl  
_except_handler3
(
    PEXCEPTION_RECORD XRecord,
    void *Registration,
    PCONTEXT Context,
    PDISPATCHER_CONTEXT Dispatcher
)
{
    return g_kdKernData.p_except_handler3(XRecord, Registration, Context, Dispatcher);
}

BOOL __abnormal_termination(void)
{
	return g_kdKernData.p__abnormal_termination();
}



#else

EXCEPTION_DISPOSITION __C_specific_handler
(
    PEXCEPTION_RECORD ExceptionRecord,
    PVOID EstablisherFrame, 
    PCONTEXT ContextRecord, 
    PDISPATCHER_CONTEXT DispatcherContext
)
{
    return g_kdKernData.p__C_specific_handler(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}

#endif

VOID* GetObjectPtrByType
(
    HANDLE h,
    int type
)
{
    return g_kdKernData.pGetObjectPtrByType(h, type);
}

VOID WINAPI
InitializeCriticalSection(LPCRITICAL_SECTION lpcs)
{
    g_kdKernData.pInitializeCriticalSection(lpcs);
}

VOID WINAPI
DeleteCriticalSection(LPCRITICAL_SECTION lpcs)
{
    g_kdKernData.pDeleteCriticalSection(lpcs);
}

VOID WINAPI
EnterCriticalSection(LPCRITICAL_SECTION lpcs)
{
    g_kdKernData.pEnterCriticalSection(lpcs);
}

VOID WINAPI
LeaveCriticalSection(LPCRITICAL_SECTION lpcs)
{
    g_kdKernData.pLeaveCriticalSection(lpcs);
}

void FlushCache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS | CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

void FlushICache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS);
}

void FlushDCache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

// Flushing caches and contexts
#if defined(SHx)

#if !defined(SH3e) && !defined(SH4)
void DSPFlushContext(void) {
    g_kdKernData.DSPFlushContext();
}
#endif
#endif


#if defined(MIPS_HAS_FPU) || defined(SH4) || defined(x86) || defined(ARM)
void FPUFlushContext(void) {
    g_kdKernData.FPUFlushContext();
}
#endif

#undef LocalFree
// dummy function so the CRT links.  Should never be called
HLOCAL WINAPI
LocalFree(HLOCAL hMem)
{
    KD_ASSERT(FALSE);
    return hMem;
}
