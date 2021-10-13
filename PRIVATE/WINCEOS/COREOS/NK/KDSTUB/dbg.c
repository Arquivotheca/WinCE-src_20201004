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

#include "kdp.h"

///#define V_ADDR(addr)    VerifyAccess((addr), VERIFY_KERNEL_OK, (ACCESSKEY)-1)
#define V_ADDR(addr)    (addr)

extern LPVOID ThreadStopFunc;
const DWORD dwKStart=(DWORD)-1, dwNewKStart=(DWORD)-1;  //KPage start and new start addresses
const DWORD dwKEnd=(DWORD)-1, dwNewKEnd=(DWORD)-1;      //KPage end and new end addresses

int g_cFrameInCallStack = 0;

static const CHAR lpszUnk[]="UNKNOWN";
static const CHAR lpszKernName[]="NK.EXE";

// Handle Explorer help
const char g_szHandleExHelp[] =
"Handle Explorer (Target Side Command):\n" \
"\thl ?                      - Request help on Handle Explorer commands\n" \
"\thl query [P:pid] [T:type] - Display information about handles in process and/or of a specific typeid\n" \
"\thl info hid               - Display information about a single handle\n" \
"\thl delete P:pid H:hid     - Delete a handle from a process (DANGEROUS)\n\n" \
"PID and Type ID should be decimal values. Handle ID should be hexadecimal.\n" \
"T00: Win32     T01: Thread    T02: Process   T03: [Unused]\n" \
"T04: Event     T05: Mutex     T06: API Set   T07: File\n" \
"T08: Find      T09: DB File   T10: DB Find   T11: Socket\n" \
"T12: Interface T13: Semaphore T14: FS Map    T15: WNet Enum\n";

// Map Handle Explorer IDs -> names
const LPCSTR g_apszHandleFields[] =
{"Handle",          // KD_HDATA_HANDLE
 "Access Key",      // KD_HDATA_AKY
 "Ref Count",       // KD_HDATA_REFCNT
 "Type",            // KD_HDATA_TYPE
 "Name",            // KD_HDATA_NAME
 "Suspend Cnt",     // KD_HDATA_THREAD_SUSPEND
 "Owner Proc",      // KD_HDATA_THREAD_PROC
 "Base Prio",       // KD_HDATA_THREAD_BPRIO
 "Cur Prio",        // KD_HDATA_THREAD_CPRIO
 "Kernel Time",     // KD_HDATA_THREAD_KTIME
 "User Time",       // KD_HDATA_THREAD_UTIME
 "PID",             // KD_HDATA_PROC_PID
 "Trust Level",     // KD_HDATA_PROC_TRUST
 "VMBase",          // KD_HDATA_PROC_VMBASE
 "Base Ptr",        // KD_HDATA_PROC_BASEPTR
 "Commandline",     // KD_HDATA_PROC_CMDLINE
 "State",           // KD_HDATA_EVENT_STATE
 "Man. Reset",      // KD_HDATA_EVENT_RESET
 "Lock Count",      // KD_HDATA_MUTEX_LOCKCNT
 "Owner",           // KD_HDATA_MUTEX_OWNER
 "Count",           // KD_HDATA_SEM_COUNT
 "Max Count",       // KD_HDATA_SEM_MAXCOUNT
};

// Stackwalk state globals
PCALLSTACK pStk        = NULL;
PPROCESS   pLastProc   = NULL;
PTHREAD    pWalkThread = NULL;
BOOL       g_fTopFrame = TRUE;

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

    if ((*pMaxBytes) < wLen) {
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
    (*pOutBufIndex) += wLen;
    (*pMaxBytes) -= wLen;

    return TRUE;
}

// Like library function, returns number of chars copied or -1 on overflow
int snprintf(LPSTR pszBuf, UINT cBufLen, LPCWSTR pszFmt, ...)
{
    WCHAR szTempString[256];
    UINT cOutLen, cStrLen;

    // Do nothing if no buffer
    if (cBufLen == 0)
        return -1;

    NKwvsprintfW(szTempString, pszFmt, (&pszFmt) + 1, lengthof(szTempString));
    cStrLen = kdbgwcslen(szTempString);
    cOutLen = min(cBufLen - 1, cStrLen);
    szTempString[cOutLen] = L'\0';
    kdbgWtoA(szTempString, pszBuf);

    return (cBufLen > cStrLen ? cOutLen : -1);
}

// Like library function
unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    unsigned long oflow_mul = (unsigned long)(ULONG_MAX / (unsigned long) base);
    unsigned long oflow_add = (unsigned long)(ULONG_MAX % (unsigned long) base);
    unsigned long i = 0;
    unsigned long n;

    // Defensive programming
    if (base < 2 || base > 36)
    {
        DEBUGGERMSG(KDZONE_DBG, (L"Invalid base (%u) passed to strtoul!\r\n", base));
        return ULONG_MAX;
    }

    while((*nptr >= '0' && *nptr <= '9') || // numeric
          (*nptr >= 'a' && *nptr <= 'z') || // lower-case alpha
          (*nptr >= 'A' && *nptr <= 'Z'))   // upper-case alpha
    {
        if (*nptr >= '0' && *nptr <= '9')
            n = (*nptr - '0');
        else if (*nptr >= 'a' && *nptr <= 'z')
            n = (10 + *nptr - 'a');
        else // if (*nptr >= 'A' && *nptr <= 'Z')
            n = (10 + *nptr - 'A');

        // Validate input
        if (n >= (unsigned long) base)
            break;

        // Check for overflow
        if (i > oflow_mul || (i == oflow_mul && n > oflow_add))
        {
            i = ULONG_MAX;
            break;
        }

        i = (i * base) + n;
        nptr++;
    }

    if (endptr != NULL)
        *endptr = (char *) nptr;

    return i;
}

// We get isspace in headers but not imports, so we need to roll our own
int Kdp_isspace(char ch)
{
    return (ch == ' ' || (ch > 9 && ch < 13) ? 1 : 0);
}

/*++

Routine Name:

    ParseParamString

Routine Description:

    Part of the FlexPTI parser. This routine parses a parameter string into
    individual parameters.

Arguments:

    pszParamString  - [in/out] Pointer to parameter string

Return Value:

    Number of parameters parsed. On return, pszParamString will contain the
    address of the first parameter.

--*/
static UINT ParseParamString(LPSTR *ppszParamString)
{
    LPSTR pszCurPos = *ppszParamString;
    UINT nParams = 0;
    LPSTR pszCopyPos;

    // Skip leading whitespace
    while(Kdp_isspace(*pszCurPos))
        pszCurPos++;

    // Store start of parameter string
    *ppszParamString = pszCurPos;

    while(*pszCurPos != '\0')
    {
        pszCopyPos = pszCurPos;

        // Find first whitespace outside of quotes. As an added bonus, we copy
        // over quotes here so """f""o"o becomes foo.
        while ((pszCopyPos <= pszCurPos) &&
               (*pszCurPos != '\0') &&                 // not end of string and
               ((!Kdp_isspace(*pszCurPos)) ||         // haven't found a space or
                ((pszCurPos - pszCopyPos) % 2 != 0)))    // in the middle of quotes
        {
            // If we have a quote, this desyncs pszCopyPos and pszCurPos so we
            // start copying characters backward. The loop test prevents us
            // from exiting between quotes unless we hit the end of the string.
            if (*pszCurPos != '"')
            {
                *pszCopyPos = *pszCurPos;
                pszCopyPos++;
            }

            pszCurPos++;
        }

        // Null-terminate the end of the parameter
        while(pszCopyPos < pszCurPos)
           *pszCopyPos++ = '\0';

        // Skip trailing whitespace
        while(Kdp_isspace(*pszCurPos))
            *pszCurPos++ = '\0';

        nParams++;
    }

    return nParams;
}

/*++

Routine Name:

    GetNextParam

Routine Description:

    Part of the FlexPTI parser. This routine finds the offset of the next
    parameter in the parameter string. The parser splits the parameter string
    into individual parameters (like argc/argv). To avoid memory allocation, it
    leaves the parameters in-place and nulls out characters in the middle.

    It is imperative that the caller verify that there is another parameter in
    the string or this function may crash.

Arguments:

    pszCurParam     - [in]     Pointer to current parameter

Return Value:

    A pointer to the next parameter string is returned.

--*/
static LPCSTR GetNextParam(LPCSTR pszCurParam)
{
    // Find end of current string
    while(*pszCurParam != '\0')
        pszCurParam++;

    // Find start of next string
    while(*pszCurParam == '\0')
        pszCurParam++;

    return pszCurParam;
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

BOOL GetModuleInfo (PROCESS *pProc, DWORD dwStructAddr, KD_MODULE_INFO *pkmodi, BOOL fRedundant, BOOL fUnloadSymbols)
{
    LPMODULE lpMod;
    static BOOL PhysUpd;

    DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nGetting name and base for module %8.8lx \r\n"), dwStructAddr));

    // set default to not ROM DLL
    pkmodi->dwDllRwEnd = 0;
    pkmodi->dwDllRwStart = 0xFFFFFFFF;

    // set default to no TimeStamp data
    pkmodi->dwTimeStamp = 0;

    // set defaults for v15 protocol data
    pkmodi->hDll = NULL;
    pkmodi->dwInUse = 0;
    pkmodi->wFlags = 0;
    pkmodi->bTrustLevel = 0;

    // Check for the kernel first
    if (&(kdProcArray [0]) == (PROCESS *) dwStructAddr)
    {
        if (PhysUpd && fRedundant)
        {
            DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nReturning redundant\r\n")));
            return FALSE;
        }

        pkmodi->szName = (LPCHAR) lpszKernName;
        pkmodi->ImageBase    = (ULONG) kdProcArray[0].BasePtr;
        pkmodi->ImageSize    = kdProcArray[0].e32.e32_vsize;
        pkmodi->dwDllRwStart = ((COPYentry *)(pTOC->ulCopyOffset))->ulDest;
        pkmodi->dwDllRwEnd   = pkmodi->dwDllRwStart + ((COPYentry *)(pTOC->ulCopyOffset))->ulDestLen - 1;
        pkmodi->dwTimeStamp  = kdProcArray[0].e32.e32_timestamp;
        pkmodi->hDll         = NULL;
        pkmodi->dwInUse      = (1 << kdProcArray[0].procnum);
        pkmodi->wFlags       = 0;
        pkmodi->bTrustLevel  = kdProcArray[0].bTrustLevel;

        PhysUpd=TRUE;
        DEBUGGERMSG(KDZONE_DBG, (L"Returning kernel %8.8lx-%8.8lx %a (TimeStamp: %8.8lx)\r\n", pkmodi->ImageBase, pkmodi->ImageBase+pkmodi->ImageSize, pkmodi->szName, pkmodi->dwTimeStamp));

        return TRUE;
    }

    DEBUGGERMSG(KDZONE_DBG,
            (TEXT("\r\nMy address %8.8lx, adjusted %8.8lx, Base: %8.8lx, Base+Size: %8.8lx\r\n"),
            dwStructAddr, (ULONG)MapPtrInProc(dwStructAddr, pProc), (long)pProc->BasePtr,
            MapPtrInProc((long)pProc->BasePtr+pProc->e32.e32_vsize, pProc)));

    if ((PROCESS *) dwStructAddr == pProc)
    { // Process
        pkmodi->szName       = GetWin32ExeName (pProc);
        pkmodi->ImageBase    = (UINT) MapPtrInProc (pProc->BasePtr, pProc);
        pkmodi->ImageSize    = pProc->e32.e32_vsize;
        pkmodi->dwTimeStamp  = pProc->e32.e32_timestamp;
        pkmodi->hDll         = NULL;
        pkmodi->dwInUse      = (1 << pProc->procnum);
        pkmodi->wFlags       = 0;
        pkmodi->bTrustLevel  = pProc->bTrustLevel;
        DEBUGGERMSG(KDZONE_DBG, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx, TimeStamp: %8.8lx of Executable\r\n"),
                    pkmodi->szName, pkmodi->ImageBase, pkmodi->ImageSize, pkmodi->dwTimeStamp));
    }
    else if ((void *) dwStructAddr == ((MODULE *) dwStructAddr)->lpSelf)
    { // DLL
        lpMod = (MODULE *) dwStructAddr;
        if ((lpMod->DbgFlags & DBG_SYMBOLS_LOADED) &&
            fRedundant && !fUnloadSymbols)
        {
            DEBUGGERMSG(KDZONE_DBG, (TEXT("\r\nReturing redundant\r\n")));
        }

        kdbgWtoA(lpMod->lpszModName,lpszModuleName);
        pkmodi->szName       = lpszModuleName;
        pkmodi->ImageBase    = ZeroPtr(lpMod->BasePtr);
        pkmodi->ImageSize    = lpMod->e32.e32_vsize;
        pkmodi->dwDllRwStart = lpMod->rwLow;
        pkmodi->dwDllRwEnd   = lpMod->rwHigh;
        pkmodi->dwTimeStamp  = lpMod->e32.e32_timestamp;
        pkmodi->hDll         = (HMODULE) lpMod;
        pkmodi->dwInUse      = lpMod->inuse;
        pkmodi->wFlags       = lpMod->wFlags;
        pkmodi->bTrustLevel  = lpMod->bTrustLevel;
    }
    else
    {
        DEBUGGERMSG(KDZONE_DBG, (TEXT("No module associated with address %8.8lx\r\n"), dwStructAddr));
        return FALSE;
    }

    DEBUGGERMSG(KDZONE_DBG, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx, TimeStamp: %8.8lx, Handle: %8.8lX of DLL\r\n"),
                pkmodi->szName, pkmodi->ImageBase, pkmodi->ImageSize, pkmodi->dwTimeStamp, pkmodi->hDll));

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
#define tfiPC                           (22L) // Program Counter / Instruction Pointer
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
    },
    {
        tfiPC,
        sizeof (DWORD),
        "PC",
        "0x%08lX"
    }
};


#define PROC_DESC_NB_FIELDS (sizeof (ProcessDescriptorTable) / sizeof (PROC_THREAD_INFO_FIELD))
#define THREAD_DESC_NB_FIELDS (sizeof (ThreadDescriptorTable) / sizeof (PROC_THREAD_INFO_FIELD))


#define AppendImmByteToOutBuf_M(outbuf,immbyte,outbidx) {(outbuf) [(outbidx)++] = (immbyte);}
#define AppendObjToOutBuf_M(outbuf,obj,outbidx) {memcpy (&((outbuf) [(outbidx)]), &(obj), sizeof (obj)); (outbidx) += sizeof (obj);}
#define AppendStringZToOutBuf_M(outbuf,sz,outbidx) {memcpy (&((outbuf) [(outbidx)]), sz, (strlen (sz) + 1)); (outbidx) += (strlen (sz) + 1);}
#define AppendErrorToOutBuf_M(outbuf,err,outbidx,len) AppendErrorToOutBuf(outbuf, &outbidx, len, err)

void AppendErrorToOutBuf(LPSTR pszOutBuf, UINT *pnOutIndex, UINT nLen, NTSTATUS status)
{
    // Normalize buffer/length to skip block
    pszOutBuf += *pnOutIndex;
    nLen -= *pnOutIndex;

    switch(status)
    {
    case STATUS_ACCESS_VIOLATION:
        strncpy(pszOutBuf, "ERROR: Access Violation", nLen);
        pszOutBuf[nLen - 1] = '\0';
        break;
    case STATUS_DATATYPE_MISALIGNMENT:
        strncpy(pszOutBuf, "ERROR: Datatype Misalignment", nLen);
        pszOutBuf[nLen - 1] = '\0';
        break;
    case STATUS_INVALID_PARAMETER:
        strncpy(pszOutBuf, "ERROR: Invalid Parameter", nLen);
        pszOutBuf[nLen - 1] = '\0';
        break;
    case STATUS_BUFFER_TOO_SMALL:
        strncpy(pszOutBuf, "ERROR: Buffer too small", nLen);
        pszOutBuf[nLen - 1] = '\0';
        break;
    case STATUS_INTERNAL_ERROR:
        strncpy(pszOutBuf, "ERROR: Internal error. Check COM1 debug output", nLen);
        pszOutBuf[nLen - 1] = '\0';
        break;
    default:
        snprintf(pszOutBuf, nLen, L"ERROR: Unknown Error (%08lX)", status);
        break;
    }

    // Update buffer index
    *pnOutIndex += strlen(pszOutBuf);
}

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


    sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Critical Section Info (shows synchronization object status)\n"));

    for (nProc = 0; nProc < MAX_PROCESSES; nProc++) {
        pProc = &(kdProcArray[nProc]);

        if (!pProc->dwVMBase) {
            // No process in this slot
            continue;
        }

        sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT(" pProcess : 0x%08X %s\n"), pProc, pProc->lpszProcName);
        pThread = pProc->pTh; // Get process main thread

        ThreadNum = 1;
        while (pThread)
        { // walk list of threads attached to process (until we reach the index ThreadOrdNum)
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\tpThread 0x%08X\n"), pThread);

            pProxy = pThread->lpProxy;

            if (pProxy) {
                while (pProxy) {

                    switch(pProxy->bType) {
                        case HT_CRITSEC : {
                            LPCRIT lpCrit = (LPCRIT) pProxy->pObject;
                            PTHREAD pOwnerThread = HandleToThread(lpCrit->lpcs->OwnerThread);
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tpCritSect 0x%08X (pOwner 0x%X)\n"), lpCrit->lpcs, pOwnerThread);
                            break;
                        }

                        case HT_EVENT : {
                            LPEVENT lpEvent = (LPEVENT) pProxy->pObject;
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\thEvent 0x%08X\n"), lpEvent->hNext);
                            break;
                        }

                        default : {
                            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tUnknown object type %d (0x%08X)\n"), pProxy->bType, pProxy->pObject);
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
    (*pOutBufIndex)++;

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
    DBGKD_COMMAND dbgkdCmdPacket;
    STRING strIO;
    dbgkdCmdPacket.u.ReadWriteIo.qwTgtIoAddress = dwAddress;
    dbgkdCmdPacket.u.ReadWriteIo.dwDataSize = dwSize;
    dbgkdCmdPacket.u.ReadWriteIo.dwDataValue = dwData;
    strIO.Length = 0;
    strIO.Buffer = 0;
    strIO.MaximumLength = 0;

    if (fIORead)
    {
        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Read: Address=0x%08X, Size=%u  ==>  "),dwAddress,dwSize);
        KdpReadIoSpace (&dbgkdCmdPacket, &strIO, FALSE);
    }
    else
    {
        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Write: Address=0x%08X, Size=%u, Data=0x%08X  ==>  "),dwAddress,dwSize,dwData);
        KdpWriteIoSpace (&dbgkdCmdPacket, &strIO, FALSE);
    }

    switch (dbgkdCmdPacket.dwReturnStatus)
    {
        case STATUS_SUCCESS:
            if (fIORead)
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success: Data=0x%08X\n"),dbgkdCmdPacket.u.ReadWriteIo.dwDataValue);
            }
            else
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success\n"));
            }
            break;
        case STATUS_ACCESS_VIOLATION:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Access Violation\n"));
            break;
        case STATUS_DATATYPE_MISALIGNMENT:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Datatype Misalignment\n"));
            break;
        case STATUS_INVALID_PARAMETER:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Invalid Parameter\n"));
            break;
        default:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Unknown Error\n"));
            break;
    }

    // Include the NULL
    (*pOutBufIndex)++;

    return TRUE;
} // End of MarshalCriticalSectionInfo

/*++

Routine Name:

    PrintHandleList

Routine Description:

    Processes a handle request. This is called from the hl command in FlexPTI.
    Formats a response in ASCII and stores in the buffer.

Arguments:

    pszBuffer       - [in]     Pointer to buffer
    cchBufLen       - [in]     Length of buffer
    pHDD            - [in]     Pointer to handle description buffer
    pHGD            - [in]     Pointer to handle data buffer

--*/
void PrintHandleList(LPSTR pszBuffer, UINT cchBufLen, PDBGKD_HANDLE_DESC_DATA pHDD, PDBGKD_HANDLE_GET_DATA pHGD)
{
    // Some arbitrary format constants
    const char pszFieldEndStr[] = "  ";
    const char pszLineEndStr[] = "\n";
    const char pszBlockEndStr[] = "\n";
    const UINT cchColWidth = 11;

    // Compute some constants that make life easier
    const PDBGKD_HANDLE_FIELD_DESC pFieldDesc = pHDD->out.pFieldDesc;
    const PDBGKD_HANDLE_FIELD_DATA pFieldData = pHGD->out.pFields;
    const LPSTR pszEndPtr = pszBuffer + cchBufLen - lengthof(pszBlockEndStr);
    const UINT cFieldsPerLine = pHDD->out.cFields;
    const UINT cFields = pHGD->out.cFields;

    // This is the length of the "fixed" part of the line. The last field is
    // allowed to be variable length and exceed the fixed column width of 11
    // chars. This includes the line trailier, too.
    const UINT cchFixedLineLen = ((cchColWidth + lengthof(pszFieldEndStr)) * (cFieldsPerLine - 1)) +
                                 lengthof(pszLineEndStr);

    // Some locals
    NTSTATUS status = STATUS_SUCCESS;
    PDBGKD_HANDLE_FIELD_DATA pCurField;
    LPSTR pszLineStart;
    UINT i, j, cchLen, cHandles;

    // Defensive programming
    if (cFieldsPerLine == 0)
    {
        DEBUGGERMSG(KDZONE_HANDLEEX, (L"  ERROR: 0 fields per line!\r\n"));
        status = STATUS_INTERNAL_ERROR;
        goto Error;
    }

    // Defensive programming
    if ((cFields % cFieldsPerLine) != 0)
    {
        DEBUGGERMSG(KDZONE_HANDLEEX, (L"  ERROR: Query returned odd number of fields (%u fields total, %u fields/handle)\r\n", cFields, cFieldsPerLine));
        status = STATUS_INTERNAL_ERROR;
        goto Error;
    }

    cHandles = cFields / cFieldsPerLine;
    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Printing handle data: %u fields, %u handles\r\n", cFieldsPerLine, cHandles));

    // Special case when we print no handles. We want the output to look nice,
    // so skip headers and print the message.
    if (cHandles == 0)
    {
        const char pszNoHandles[] = "No handles matched your query.";

        if (cchBufLen < (lengthof(pszNoHandles) - 1))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Error;
        }

        memcpy(pszBuffer, pszNoHandles, sizeof(pszNoHandles) - 1);
        pszBuffer += (lengthof(pszNoHandles) - 1);
        goto Done;
    }

    // Length of headers
    i = cchFixedLineLen +
        strlen(g_apszHandleFields[pFieldDesc[cFieldsPerLine - 1].nFieldId]);

    // Do we have enough buffer space for headers?
    if ((UINT)(pszEndPtr - pszBuffer) < i)
    {
        DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Buffer too small: need %u bytes, have %u bytes\r\n", i + 2, pszEndPtr - pszBuffer));
        status = STATUS_BUFFER_TOO_SMALL;
        goto Error;
    }

    // Write out headers
    for(i = 0, pszLineStart = pszBuffer; i < cFieldsPerLine; i++)
    {
        const LPCSTR pszFieldName = g_apszHandleFields[pFieldDesc[i].nFieldId];

        cchLen = strlen(pszFieldName);
        memcpy(pszBuffer, pszFieldName, cchLen);
        pszBuffer += cchLen;

        if (i < (cFieldsPerLine - 1))
        {
            // Defensive programming
            if (cchLen > cchColWidth)
            {
                DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Description for field ID %u is too long! (%u chars, %u max)\r\n", pFieldDesc[i].nFieldId, cchLen, cchColWidth));
                status = STATUS_INTERNAL_ERROR;
                goto Error;
            }

            if (cchLen < cchColWidth)
            {
                memset(pszBuffer, ' ', cchColWidth - cchLen);
                pszBuffer += (cchColWidth - cchLen);
            }

            memcpy(pszBuffer, pszFieldEndStr, sizeof(pszFieldEndStr) - 1);
            pszBuffer += (lengthof(pszFieldEndStr) - 1);
        }
    }

    memcpy(pszBuffer, pszLineEndStr, sizeof(pszLineEndStr) - 1);
    pszBuffer += (lengthof(pszLineEndStr) - 1);
    pCurField = pFieldData;

    // Each line has 1 handle on it
    for(i = 0, pszLineStart = pszBuffer; i < cHandles; i++, pszLineStart = pszBuffer)
    {
        if ((UINT)(pszEndPtr - pszBuffer) < cchFixedLineLen)
            break;

        for(j = 0; j < cFieldsPerLine; j++, pCurField++)
        {
            cchLen = 0;
            if (pCurField->fValid)
            {
                switch(pFieldDesc[j].nType)
                {
                case KD_FIELD_UINT:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%u", pCurField->nData);
                    break;
                case KD_FIELD_SINT:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%i", pCurField->nData);
                    break;
                case KD_FIELD_CHAR:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%c", (WCHAR) pCurField->nData);
                    break;
                case KD_FIELD_WCHAR:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%c", pCurField->nData);
                    break;
                case KD_FIELD_CHAR_STR:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%a", pCurField->nData);
                    break;
                case KD_FIELD_WCHAR_STR:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%s", pCurField->nData);
                    break;
                case KD_FIELD_PTR:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%08p", pCurField->nData);
                    break;
                case KD_FIELD_BOOL:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%s", (pCurField->nData == 0 ? L"False" : L"True"));
                    break;
                case KD_FIELD_HANDLE:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%08p", pCurField->nData);
                    break;
                case KD_FIELD_BITS:
                    cchLen = snprintf(pszBuffer, (UINT)(pszEndPtr - pszBuffer),
                                      L"%08lX", pCurField->nData);
                    break;
                default:
                    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Invalid type ID %u\r\n", pFieldDesc[j].nType));
                    status = STATUS_INTERNAL_ERROR;
                    goto Error;
                }
            }

            // Pad out to full width and insert trailing spaces if not on the
            // last column.
            if (j < (cFieldsPerLine - 1))
            {
                // On long fields, terminate with "..." to let the user know there
                // was more data.
                if (cchLen > cchColWidth)
                {
                    pszBuffer[cchColWidth - 3] = '.';
                    pszBuffer[cchColWidth - 2] = '.';
                    pszBuffer[cchColWidth - 1] = '.';
                }
                else if (cchLen < cchColWidth)
                {
                    memset(pszBuffer + cchLen, ' ', cchColWidth - cchLen);
                }

                pszBuffer += cchColWidth;
                memcpy(pszBuffer, pszFieldEndStr, sizeof(pszFieldEndStr) - 1);
                pszBuffer += (lengthof(pszFieldEndStr) - 1);
            }
        }

        if (cchLen == -1)
            break;

        pszBuffer += cchLen;
        if ((UINT)(pszEndPtr - pszBuffer) < (lengthof(pszLineEndStr) - 1))
            break;

        memcpy(pszBuffer, pszLineEndStr, sizeof(pszLineEndStr) - 1);
        pszBuffer += (lengthof(pszLineEndStr) - 1);
    }

    // Back up to the previous line. If we completed successfully, then
    // pszBuffer == pszLineStart and the pointer isn't moved.
    pszBuffer = pszLineStart;

    // Did we actually get any lines into the buffer?
    if (i < 1)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto Error;
    }

    // Try to tell the user there's more data. It sucks that they can't
    // really do anything about it.
    if (i < cHandles)
    {
        const char szMoreString[] = "[More]\n";

        if ((UINT)(pszEndPtr - pszBuffer) >= (lengthof(szMoreString) - 1))
        {
            memcpy(pszBuffer, szMoreString, sizeof(szMoreString) - 1);
            pszBuffer += (lengthof(szMoreString) - 1);
        }
    }

    // Append trailer
Done:
    strcpy(pszBuffer, pszBlockEndStr);

    return;

Error:
    // Backtrack. Note that pszEndPtr already had the block suffix subtracted,
    // so we have to add this back in.
    pszBuffer = pszEndPtr - cchBufLen + lengthof(pszBlockEndStr);

    // Append error message
    i = 0;
    AppendErrorToOutBuf_M(pszBuffer, status, i, cchBufLen);
}

// Buffers used in MarshalHandleQuery. The rest of the code will automatically
// pick up changes to these buffers.
char rgDummyDesc[sizeof(DBGKD_HANDLE_DESC_DATA) + (sizeof(DBGKD_HANDLE_FIELD_DESC) * 16)];
char rgDummyData[sizeof(DBGKD_HANDLE_GET_DATA) + (sizeof(DBGKD_HANDLE_FIELD_DATA) * 128)];

/*++

Routine Name:

    MarshalHandleQuery

Routine Description:

    Processes a handle request. This is called from the hl command in FlexPTI.
    Null-terminated text for the end user is stored in the buffer.

Arguments:

    pszBuffer       - [in]     Pointer to buffer
    cBufLen         - [in]     Length of buffer
    pszParams       - [in]     Pointer to first parameter
    cParams         - [in]     Number of parameters

Return Value:

    On success: TRUE
    On error:   FALSE, no additional information is available

--*/
static BOOL MarshalHandleQuery(LPSTR pszBuffer, UINT cBufLen, LPCSTR pszParams, UINT cParams)
{
    const LPSTR pszEndPtr = pszBuffer + cBufLen;
    NTSTATUS status;
    UINT i;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"++MarshalHandleQuery\r\n"));

    if (cParams >= 1 && !strcmp(pszParams, "query"))
    {
        // "hl query [P:pid] [T:type]" - List handles matching filter
        const PDBGKD_HANDLE_DESC_DATA pHDD = (PDBGKD_HANDLE_DESC_DATA) rgDummyDesc;
        const PDBGKD_HANDLE_GET_DATA pHGD = (PDBGKD_HANDLE_GET_DATA) rgDummyData;
        UINT32 nPIDMask = 0;
        UINT32 nAPIMask = 0;

        cParams--;
        while(cParams > 0)
        {
            pszParams = GetNextParam(pszParams);
            cParams--;

            if (pszParams[0] == 'p' && pszParams[1] == ':')
            {
                i = strtoul(pszParams + 2, NULL, 10);

                if (i >= MAX_PROCESSES || kdProcArray[i].dwVMBase == 0)
                {
                    status = STATUS_INVALID_PARAMETER;
                    goto Error;
                }

                nPIDMask |= (1 << i);
            }
            else if (pszParams[0] == 't' && pszParams[1] == ':')
            {
                i = strtoul(pszParams + 2, NULL, 10);

                if (i >= 32)
                {
                    status = STATUS_INVALID_PARAMETER;
                    goto Error;
                }

                nAPIMask |= (1 << i);
            }
            else
            {
                status = STATUS_INVALID_PARAMETER;
                goto Error;
            }
        }

        // If no procs specified, search all
        if (nPIDMask == 0)
            nPIDMask = -1;

        // If no APIs specified, search all
        if (nAPIMask == 0)
            nAPIMask = -1;

        // Run the queries
        pHDD->in.nPIDFilter = pHGD->in.nPIDFilter = nPIDMask;
        pHDD->in.nAPIFilter = pHGD->in.nAPIFilter = nAPIMask;
        pHGD->in.hStart = NULL;

        status = KdQueryHandleFields(pHDD, sizeof(rgDummyDesc));
        if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
            goto Error;

        status = KdQueryHandleList(pHGD, sizeof(rgDummyData));
        if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
            goto Error;

        PrintHandleList(pszBuffer, cBufLen, pHDD, pHGD);
    }
    else if (cParams >= 1 && !strcmp(pszParams, "info"))
    {
        // "hl info H:hid" - Query handle info
        const PDBGKD_HANDLE_DESC_DATA pHDD = (PDBGKD_HANDLE_DESC_DATA) rgDummyDesc;
        const PDBGKD_HANDLE_GET_DATA pHGD = (PDBGKD_HANDLE_GET_DATA) rgDummyData;
        HANDLE hHandle;
	HDATA *phHandle;

        if (cParams < 2)
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        // Validate handle parameter
        pszParams = GetNextParam(pszParams);
        if (pszParams[0] != 'h' || pszParams[1] != ':')
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        hHandle = (HANDLE) strtoul(pszParams + 2, NULL, 16);
	phHandle = KdHandleToPtr(hHandle);

        if (phHandle == NULL)
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        pHDD->in.nPIDFilter = -1;
        pHDD->in.nAPIFilter = 1 << phHandle->pci->type;
        status = KdQueryHandleFields(pHDD, sizeof(rgDummyDesc));
        if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
            goto Error;

        status = KdQueryOneHandle(hHandle, pHGD, sizeof(rgDummyData));
        if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
            goto Error;

        PrintHandleList(pszBuffer, cBufLen, pHDD, pHGD);
    }
    else if (cParams >= 1 && !strcmp(pszParams, "delete"))
    {
        // "hl delete P:pid H:hid" - Delete handle
        PPROCESS pCurProcSave;
        PTHREAD pCurThrdSave;
        UINT32 nPID;
        HANDLE hHandle;
        HDATA *phHandle;
        DWORD dwLastError;

        if (cParams < 2)
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        // Validate PID parameter
        pszParams = GetNextParam(pszParams);
        if (pszParams[0] != 'p' || pszParams[1] != ':')
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        nPID = strtoul(pszParams + 2, NULL, 10);

        if (nPID >= MAX_PROCESSES || kdProcArray[nPID].dwVMBase == 0)
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        // Validate handle parameter
        pszParams = GetNextParam(pszParams);
        if (pszParams[0] != 'h' || pszParams[1] != ':')
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        hHandle = (HANDLE) strtoul(pszParams + 2, NULL, 16);
        phHandle = KdHandleToPtr(hHandle);

        if (phHandle == NULL || KdGetProcHandleRef(phHandle, nPID) == 0)
        {
            status = STATUS_INVALID_PARAMETER;
            goto Error;
        }

        DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Deleting handle %08p (ptr=%08p) from process %lu\r\n", hHandle, phHandle, nPID));

        // Save current thread & process
        pCurProcSave = kdpKData->pCurPrc;
        pCurThrdSave = kdpKData->pCurThd;
        dwLastError = pCurThrdSave->dwLastError;

        // Switch to any thread in target process
        kdpKData->pCurPrc = &kdProcArray[nPID];
        kdpKData->pCurThd = kdProcArray[nPID].pTh;

        // Free handle and switch back
        __try
        {
            // TODO: Change this to manipulate data directly
            if (KdCloseHandle (hHandle))
                snprintf(pszBuffer, pszEndPtr - pszBuffer, L"CloseHandle succeeded.");
            else
                snprintf(pszBuffer, pszEndPtr - pszBuffer, L"CloseHandle failed.");
        }
        __except(1)
        {
            snprintf(pszBuffer, pszEndPtr - pszBuffer,
                     L"WARNING: CloseHandle crashed with error %08lX, system may be in unstable state\r\n",
                     GetExceptionCode());
        }

        pCurThrdSave->dwLastError = dwLastError;
        kdpKData->pCurPrc = pCurProcSave;
        kdpKData->pCurThd = pCurThrdSave;

        DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Success\r\n"));
    }
    else
    {
        // "hl ?" - Give information about hl command

        // User entered an unknown command, so issue a warning
        if (cParams >= 1 && strcmp(pszParams, "?") != 0)
            pszBuffer += snprintf(pszBuffer, pszEndPtr - pszBuffer, L"Unknown command '%a'\n", pszParams);

        if ((pszEndPtr - pszBuffer) < (lengthof(g_szHandleExHelp) - 1))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Error;
        }

        memcpy(pszBuffer, g_szHandleExHelp, sizeof(g_szHandleExHelp));
        pszBuffer += (sizeof(g_szHandleExHelp) - 1);
    }

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"--MarshalHandleQuery\r\n"));

    return TRUE;

Error:
    // we assume that status was initialized
    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Got an error: %08lX\r\n", status));

    i = 0;
    AppendErrorToOutBuf_M(pszBuffer, status, i, pszEndPtr - pszBuffer);
    DEBUGGERMSG(KDZONE_HANDLEEX, (L"--MarshalHandleQuery\r\n"));

    // Sigh. I want strlen.
    while(*pszBuffer != '\0')
        pszBuffer++;

    return ((pszEndPtr - pszBuffer) > 1 ? TRUE : FALSE);
}

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
    BYTE        bBPrio;
    DWORD       dwPC;

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

        if ((pThread == pCurThread) && (g_svdThread.fSaved))
        { // WARNING - SPECIAL CASE: If thread is current thread we need to use saved value instead
            aKey = g_svdThread.aky;
            bCPrio = g_svdThread.bCPrio;
            bBPrio = g_svdThread.bBPrio;
            dwQuantum = g_svdThread.dwQuantum;
        }
        else
        {
            aKey = pThread->aky;
            bCPrio = pThread->bCPrio;
            bBPrio = pThread->bBPrio;
            dwQuantum = pThread->dwQuantum;
        }
        AppendObjToOutBuf_M (OutBuf,    pThread,                            *pOutBufIndex); // tfiStructAddr
        ThreadState = (WORD)((pThread->wInfo & ((1<<DYING_SHIFT)|(1<<DEAD_SHIFT)|(1<<BURIED_SHIFT)|(1<<SLEEPING_SHIFT))) << 2) + (1 << GET_RUNSTATE (pThread));
        AppendObjToOutBuf_M (OutBuf,    ThreadState,                        *pOutBufIndex); // tfiRunState
        ThreadInfo = (WORD)(pThread->wInfo & ((1<<TIMEMODE_SHIFT)|(1<<STACKFAULT_SHIFT)|(1<<USERBLOCK_SHIFT)|(1<<PROFILE_SHIFT)));
        AppendObjToOutBuf_M (OutBuf,    ThreadInfo,                         *pOutBufIndex); // tfiInfo
        AppendObjToOutBuf_M (OutBuf,    pThread->hTh,                       *pOutBufIndex); // tfiHandle
        AppendObjToOutBuf_M (OutBuf,    pThread->bWaitState,                *pOutBufIndex); // tfiWaitState
        AppendObjToOutBuf_M (OutBuf,    aKey,                               *pOutBufIndex); // tfiAddrSpaceAccessKey
        AppendObjToOutBuf_M (OutBuf,    pThread->pProc->hProc,              *pOutBufIndex); // tfiHandleCurrentProcessRunIn
        AppendObjToOutBuf_M (OutBuf,    pThread->pOwnerProc->hProc,         *pOutBufIndex); // tfiHandleOwnerProc
        AppendObjToOutBuf_M (OutBuf,    bCPrio,                             *pOutBufIndex); // tfiCurrentPriority
        AppendObjToOutBuf_M (OutBuf,    bBPrio,                             *pOutBufIndex); // tfiCurrentPriority
        AppendObjToOutBuf_M (OutBuf,    pThread->dwKernTime,                *pOutBufIndex); // tfiKernelTime
        AppendObjToOutBuf_M (OutBuf,    pThread->dwUserTime,                *pOutBufIndex); // tfiUserTime
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
        if (pThread == pCurThread)
        {
            dwPC = CONTEXT_TO_PROGRAM_COUNTER (g_pctxException); // For current thread context's PC is not correct (in CaptureContext)
        }
        else
        {
            dwPC = GetThreadIP (pThread);
        }
        AppendObjToOutBuf_M (OutBuf,   dwPC,                                *pOutBufIndex); // tfiPC
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

CHAR szUnknownCommandError [] =
"Unknown target side command.\n"\
"\n";

CHAR szTargetSideParmHelp [] =
"Target Side Parameter usage: [?] | [cs] | [ior] | [iow] | [fvmp] | [hl]\n"\
" * ?                     -> this help\n"\
" * cs                    -> show Critical Section Info (and any synchronization object status)\n"\
" * ior address,size      -> read IO from 'address' for 'size' bytes (size = 1,2, or 4)\n"\
" * iow address,size,data -> write IO 'data' to 'address' for 'size' bytes (size = 1,2, or 4)\n"\
" * fvmp <state>          -> get or set Forced VM Paging state. Specify 'state' to set (state = ON, OFF)\n"\
" * hl                    -> query/manipulate handle information. Use \"hl ?\" for more information\n"\
"\n";

CHAR szFVMP_ON [] =
"Forced VM Paging is ON.\n"\
"\n";

CHAR szFVMP_OFF [] =
"Forced VM Paging is OFF.\n"\
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
    BOOL                    fRequestHelpOnTargetSideParam = FALSE;
    CHAR                    ProcName [32];
    BOOL                    fDisplayCriticalSections = FALSE;
    BOOL                    fIOR = FALSE;
    BOOL                    fIOW = FALSE;
    BOOL                    fDispForceVmPaging = FALSE;
    BOOL                    fSkipProcessBlock = FALSE;
    BOOL                    fHandleEx = FALSE;
    BOOL                    fUnknownCommand = FALSE;
    DWORD                   dwIOAddress = 0;
    DWORD                   dwIOSize = 0;
    DWORD                   dwIOData = 0;
    LPCSTR                  pszParams = NULL;
    UINT                    cParams = 0;

    DEBUGGERMSG(KDZONE_FLEXPTI, (L"++FlexiPTI (Len=%i,MaxLen=%i)\r\n", pParamBuf->Length, pParamBuf->MaximumLength));

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

    if (InpParamLength > sizeof(PROC_THREAD_INFO_PARAM))
    {// any bytes in optional Param Info String? Yes
        CHAR * szExtraParamInfo = ((PPROC_THREAD_INFO_PARAM) (pParamBuf->Buffer))->szExtraParamInfo;

        if (strncmp (szExtraParamInfo, "?", 1) == 0)
        { // Return help string on Target side parameters
            fRequestHelpOnTargetSideParam = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else if (strncmp (szExtraParamInfo, "cs", 2) == 0) // Added : JOHNELD 08/29/99
        { // Display critical section information
            fDisplayCriticalSections = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else if (strncmp (szExtraParamInfo, "procname=", 9) == 0)
        { // Return only Process with ProcName
            strncpy (ProcName, szExtraParamInfo + 9, 31);
            ProcName [31] = 0;
        }
        else if (strncmp (szExtraParamInfo, "ior", 3) == 0)
        { // Read I/O
            fIOR = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else if (strncmp (szExtraParamInfo, "iow", 3) == 0)
        { // Write I/O
            fIOW = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else if (strncmp (szExtraParamInfo, "fvmp", 4) == 0)
        { // Forced VM Paging
            fDispForceVmPaging = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else if (strncmp (szExtraParamInfo, "hl", 2) == 0)
        { // Handle Explorer command
            fHandleEx = TRUE;
            fSkipProcessBlock = TRUE;
        }
        else
        { // Return error and help string on Target side parameters
            fUnknownCommand = TRUE;
            fSkipProcessBlock = TRUE;
        }

        if (fIOR || fIOW)
        { // Parse parameters
            CHAR * szIOParam = szExtraParamInfo + 3;

            dwIOAddress = (DWORD) Kdstrtoul (szIOParam, 0);
            while ((*szIOParam) && (*szIOParam++ != ',')) ;
            if (*szIOParam)
            {
                dwIOSize = (DWORD) Kdstrtoul (szIOParam, 0);
                if (fIOW)
                {
                    while ((*szIOParam) && (*szIOParam++ != ',')) ;
                    if (*szIOParam)
                    {
                        dwIOData = (DWORD) Kdstrtoul (szIOParam, 0);
                    }
                }
            }
        }

        if (fDispForceVmPaging)
        { // Parse parameters
            CHAR * szFVMPParam = szExtraParamInfo + 4;

            // Skip whitespace
            while (((*szFVMPParam) == ' ') || (((*szFVMPParam) >= 0x9) && ((*szFVMPParam) <= 0xD))) szFVMPParam++;

            if (*szFVMPParam)
            { // parameter present -> set the state
                BOOL fPrevFpState = *(g_kdKernData.pfForcedPaging);
                if (strncmp (szFVMPParam, "on", 2) == 0)
                {
                    *(g_kdKernData.pfForcedPaging) = TRUE;
                }
                else if (strncmp (szFVMPParam, "off", 3) == 0)
                {
                    *(g_kdKernData.pfForcedPaging) = FALSE;
                }
                g_fDbgKdStateMemoryChanged = (*(g_kdKernData.pfForcedPaging) != fPrevFpState);
            }
        }

        // At this point, older code has its parameters. New code should use
        // this common parser. It is a little awkward to avoid memory
        // allocation. I write nulls over the buffer to separate params. To
        // traverse, use the GetNextParam() function.
        //
        // By the way, if you use this parser, examine your parameters before
        // you start on output. Output occupies the same buffer.
        //
        DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Parsing '%a'\r\n", szExtraParamInfo));
        cParams = ParseParamString(&szExtraParamInfo);
        pszParams = szExtraParamInfo;
        DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Have %d params\r\n", cParams));
    }

    memcpy (&InpParams, pParamBuf->Buffer, sizeof (InpParams)); // Save the input parameters
    // Warning: before this point do not write to pParamBuf->Buffer (input params not saved)

    SetReturnStatus_M (PtirstResponseBufferTooShort); // Default return status is buffer too short (common error)

    //
    // Provide all Extra Raw Strings
    //
    // I think a "raw string" is supposed to be output for non-FlexPTI stuff.
    // Stuff that goes piggybacking on FlexPTI does all of its output here.
    //
    if (InpParams.dwSubServiceMask & PtissmskAllExtraRawStrings)
    { // Provide Extra Raw Strings
        if (fUnknownCommand || fRequestHelpOnTargetSideParam || fDisplayCriticalSections || fIOR || fIOW || fDispForceVmPaging || fHandleEx)
        {
            DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Provide Extra Raw String (OutBufIdx=%i)\r\n", OutBufIndex));

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

            if (fUnknownCommand)
            { // Unknown command, display error message:
                if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szUnknownCommandError)))
                {
                    AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                                szUnknownCommandError,
                                                OutBufIndex);
                    fRequestHelpOnTargetSideParam = TRUE;
                    OutBufIndex--; // remove null termination to append help
                }
                else
                { // Buffer not large enough: exit with error
                    return FALSE;
                }
            }

            if (fRequestHelpOnTargetSideParam)
            { // Help on Target side param requested:
                if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szTargetSideParmHelp)))
                {
                    AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                                szTargetSideParmHelp,
                                                OutBufIndex);
                }
                else
                { // Buffer not large enough: exit with error
                    return FALSE;
                }
            }

            if (fDisplayCriticalSections)
            {
                if (!MarshalCriticalSectionInfo (   pParamBuf->Buffer,
                                                    &OutBufIndex,
                                                    (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
                { // Buffer not large enough: exit with error
                    return FALSE;
                }
            }

            if (fIOR || fIOW)
            {
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
            }

            if (fDispForceVmPaging)
            {
                if (*(g_kdKernData.pfForcedPaging))
                {
                    if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szFVMP_ON)))
                    {
                        AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                                    szFVMP_ON,
                                                    OutBufIndex);
                    }
                    else
                    { // Buffer not large enough: exit with error
                        return FALSE;
                    }
                }
                else
                {
                    if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szFVMP_OFF)))
                    {
                        AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                                    szFVMP_OFF,
                                                    OutBufIndex);
                    }
                    else
                    { // Buffer not large enough: exit with error
                        return FALSE;
                    }
                }
            }

            if (fHandleEx)
            {
                // Skip the "hl" parameter
                pszParams = GetNextParam(pszParams);
                cParams--;

                if (!MarshalHandleQuery(pParamBuf->Buffer + OutBufIndex, pParamBuf->MaximumLength - OutBufIndex, pszParams, cParams))
                {
                    SetReturnStatus_M(PtirstResponseBufferTooShort);
                    return FALSE;
                }

                OutBufIndex += strlen(pParamBuf->Buffer + OutBufIndex);
            }

            //
            // NOTE: Add your Extra Raw Strings here
            //

            // Reannotate end position of block just after block tag
            memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));
        }
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
            DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Provide Process Info Descriptor (OutBufIdx=%i)\r\n", OutBufIndex));
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
            DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Provide Thread Info Descriptor (OutBufIdx=%i)\r\n", OutBufIndex));
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
                        DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Provide Process Data (slot# = %i, OutBufIdx=%i)\r\n", dwProcessIdx, OutBufIndex));
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
                                DEBUGGERMSG(KDZONE_FLEXPTI, (L"FlexiPTI: Provide Thread Data (index# = %i,OutBufIdx=%i)\r\n", dwThreadIdx, OutBufIndex));
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
    DEBUGGERMSG(KDZONE_FLEXPTI, (L"--FlexiPTI (OK - OutBufIdx=%i)\r\n", OutBufIndex));
    return TRUE;
} // End of GetProcessAndThreadInfo


// Get statically mapped kernel address and attempt to read (and page-in if allowed)
void* SafeDbgVerify (void * pvAddr, BOOL fProbeOnly)
{
    DWORD dwLastError;
    void * pvRet;

    dwLastError = KGetLastError (pCurThread);

    // Only do something if no one (not even the current thread) is holding VAcs
    // if (0 == VAcs.OwnerThread)
    // {
        pvRet = g_kdKernData.pDbgVerify (pvAddr, fProbeOnly,   NULL);
    // }
    // else
    // {
    //     DEBUGGERMSG(KDZONE_ALERT, (L"  WARNING: VAcs is taken; debugger functionality will be reduced\r\n"));
    //     pvRet = 0;
    // }

    KSetLastError (pCurThread, dwLastError);

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
            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: Found in Dll (%s)\r\n", pMod->lpszModName));
            break;
        }

        pMod=pMod->pMod;
    }

    return pMod;
}


PROCESS *g_pFocusProcOverride = NULL;



// Map an address to a Debuggee context (default or override), and converted to a potentially
// statically mapped kernel address equivalent, if accessible (NULL if not accessible - in case of
// non-Forced Paged-in).
//
// Debuggee process space is by default based on current process (or prior in chain for thread migration)
// or can be overriden if client debugger sets focus on different process than current
// If in EXE range in slot 0, remap to Debuggee slot
// If in DLL R/O range, remap to NK
// If in DLL R/W range, remap to debuggee process
//
// If Forced Page-in mode ON, the page will be paged-in if not already done
// If Forced Page-in mode OFF, returns NULL

void * MapToDebuggeeCtxKernEquivIfAcc (BYTE * pbAddr, BOOL fProbeOnly)
{
    void *pvRet = NULL;
    ULONG loop;
    PROCESS *pFocusProc = pCurProc;

    DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"++MapToDebuggeeCtxKernEquivIfAcc (0x%08X)\r\n", pbAddr));

    {
        PVOID Temp = 0;
        LPMODULE lpMod;
        PCALLSTACK  pCurStk;
        if (((ULONG) pbAddr >= (DWORD) DllLoadBase) && ((ULONG) pbAddr < (2 << VA_SECTION)))
        { // pbAddr is in a DLL
            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: found in dll\r\n"));
            lpMod = ModuleFromAddress ((ULONG) pbAddr);
            if (lpMod)
            {
                // We now have the module.  Loop through to find out what section
                // this address is in.  If it is in a read only section map it into
                // the zero slot.  Otherwise try to find a process space to map it into.
                for (loop=0; loop < lpMod->e32.e32_objcnt; loop++)
                {
                    if ((ZeroPtr(pbAddr) >= ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)) &&
                        (ZeroPtr(pbAddr) < ZeroPtr(lpMod->o32_ptr[loop].o32_realaddr)+lpMod->o32_ptr[loop].o32_vsize))
                    {
                        if ((lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_READ) &&
                             !(lpMod->o32_ptr[loop].o32_flags & IMAGE_SCN_MEM_WRITE))
                        { // This is the read only section, map the pointer in NK
                            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: read only section, map in NK (VM nk = %08X)\r\n", kdProcArray->dwVMBase));
                            Temp = MapPtrProc(pbAddr,kdProcArray);
                        }
                        else
                        { // Read/Write section:
                            //now have to search for the process to map the pointer into.
                            if (g_pFocusProcOverride) // Context Override in use and DLL Used by Context override Process
                            {
                                pFocusProc = g_pFocusProcOverride;
                                DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: read/write section, map in ProcOverride (VM=%08X)\r\n", pFocusProc->dwVMBase));
                            }
                            else
                            {
                                DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: read/write section, map in CurProc (VM=%08X)\r\n", pFocusProc->dwVMBase));
                            }

                            if ((lpMod->inuse & pFocusProc->aky) // DLL Used by current process
                                    || !(pCurThread->pcstkTop))
                            { // Map in current process
                                Temp = (PVOID)((DWORD)pbAddr|pFocusProc->dwVMBase);
                                DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: DLL used by process\r\n"));
                            }
                            else
                            { // RW section but not used by current process (PSL case):
                                DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: DLL not used by process (walk back stack lists)\r\n"));
                                pCurStk=pCurThread->pcstkTop;
                                while (pCurStk)
                                { // Walk back all Cross-process calls on current thread to find process using that dll
                                    if ((pCurStk->pprcLast != (PPROCESS)USER_MODE) && (pCurStk->pprcLast != (PPROCESS)KERNEL_MODE))
                                    {
                                        if (lpMod->inuse & pCurStk->pprcLast->aky)
                                        {
                                            Temp = MapPtrInProc (pbAddr, pCurStk->pprcLast);
                                            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: map in pCurStk->pprcLast (VM=%08X)\r\n", pCurStk->pprcLast->dwVMBase));
                                            break;
                                        }
                                    }
                                    pCurStk=pCurStk->pcstkNext;
                                }
                            }
                        }
                    }
                }
            }
            else
            { // Map in NK
                Temp = MapPtrInProc (pbAddr, kdProcArray);
                DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: Unknown module, map in NK (VM=%08X)\r\n", kdProcArray->dwVMBase));
            }
        }
        else if (((ULONG) pbAddr < (DWORD) DllLoadBase) && g_pFocusProcOverride)
        { // pbAddr is in Slot0 Proc and override ON
            Temp = (PVOID)((DWORD)pbAddr|g_pFocusProcOverride->dwVMBase);
            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: Slot 0 process space, map in ProcOverride (VM=%08X)\r\n", g_pFocusProcOverride->dwVMBase));
        }
        else
        { // In non slot 0 Proc
            DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: found in fully qualified (non slot 0) user exe or kernel space\r\n"));
            Temp = pbAddr; // No changes
        }

        DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc: Temp = %08X\r\n", Temp));
        pvRet = SafeDbgVerify (Temp, fProbeOnly); // Get statically mapped kernel address and attempt to read (and page-in if allowed)
    }

    DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"--MapToDebuggeeCtxKernEquivIfAcc (ret=0x%08X)\r\n", pvRet));
    return pvRet;
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
    DEBUGGERMSG(KDZONE_STACKW, (L"    TranslateRA pStk=%8.8lX, pCThd=%8.8lX, pWlkThd=%8.8lX, pLstPrc=%8.8lX\r\n", pStk, pCurThread, pWalkThread, pLastProc));

    if (g_fTopFrame)
    {
        g_cFrameInCallStack = 0;
        g_fTopFrame = 0;
    }

    __try
    {
        if (!pRA || !pSP || !pdwProcHandle)
        {
            DEBUGGERMSG (KDZONE_ALERT, (L"  TranslateRA ERROR: Invalid Parameter\r\n"));
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
                DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA (in RA = %8.8lX, Frame#%i)\r\n", ulOldRA, g_cFrameInCallStack));

                if ((pStk) && (1 == g_cFrameInCallStack) && (g_dwExceptionCode == STATUS_INVALID_SYSTEM_SERVICE))
                { // case we get a raised exception in ObjectCall - 2nd frame unwinding
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA RaiseException in ObjectCall\r\n"));

                    ulTemp = (ULONG) pStk->retAddr;
                    ulNewRA = ZeroPtr (ulTemp);
                    *pSP = dwStackFrameAddr + 0x24; // This works for ARM

                }
                if (((SYSCALL_RETURN == ulOldRA) || (DIRECT_RETURN == ulOldRA) || ((DWORD) MD_CBRtn == ulOldRA)) &&
                    pStk)
                {
                    // PSL IPC style: Check for mode not process

                    dwOffsetSP = (pStk->dwPrcInfo & CST_CALLBACK) ? CALLEE_SAVED_REGS : 0;
                    dwPrevSP = pStk->dwPrevSP ? (pStk->dwPrevSP + dwOffsetSP) : 0;

                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA PSL\r\n"));
                    DEBUGGERMSG(KDZONE_STACKW, (L"  TranslateRA pprcLast=%8.8lX, retAddr=%8.8lX, pcstkNext=%8.8lX dwPrevSP=%8.8lX dwOffsetSP=%8.8lX\r\n", pStk->pprcLast, pStk->retAddr, pStk->pcstkNext, pStk->dwPrevSP, dwOffsetSP));

                    *pSP = dwPrevSP;

                    ulTemp = (ULONG)pStk->retAddr;
                    ulNewRA = ZeroPtr(ulTemp);

                    if (dwStackFrameAddr)
                    {
                        if ((DWORD)pStk->pprcLast > 0x10000uL)
                        {
                            pLastProc = pStk->pprcLast;
                        }

                        pStk = pStk->pcstkNext;
                    }
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
    ++g_cFrameInCallStack;

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
// TODO: Use this for Read/Write Physical address
#if 0 // Not used for now
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL VirtualToPhysical(
    LPVOID lpvAddrVirtual,
    DWORD* pdwAddrPhysical
    )
{
    int ixSection = 0, ixBlock = 0, ixPage = 0, ixOffset = 0;
    int ixFirstBlock = 0;
    PSECTION pscn;
    DWORD dwAddr = 0;
    BOOL fRet = FALSE;

    DEBUGGERMSG(KDZONE_VIRTMEM, (TEXT("++VirtualToPhysical: virt addr=0x%08X\r\n"), (DWORD) lpvAddrVirtual));

    dwAddr = (DWORD) lpvAddrVirtual;

    // Get a pointer to the section, and check that it's valid
    ixSection = (dwAddr >> VA_SECTION) & SECTION_MASK;
    pscn = IsSecureVa (dwAddr) ? &NKSection : SectionTable [ixSection];

    if (NULL_SECTION == pscn)
    {
        DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT("  VirtualToPhysical: Address section pointer is NULL!\r\n")));
    }
    else
    {
        ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK; // Obtain the block number, and check the first block
        ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK; // Obtain the page number
        ixOffset = dwAddr & (PAGE_SIZE - 1); // Obtain the page offset number

        // Calculate the final physical address
        *pdwAddrPhysical = (DWORD) (PFNfrom256 (((*pscn) [ixBlock])->aPages [ixPage]) + ixOffset);
        fRet = TRUE;
        DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT ("  VirtualToPhysical: phys addr=0x%08X.\r\n"), *pdwAddrPhysical));
    }

    DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT("--VirtualToPhysical\r\n")));
    return fRet;
}
#endif // 0


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KdpRemapVirtualMemory(
    LPVOID lpvAddrVirtualSrc, // Actual virtual address to be remapped
    LPVOID lpvAddrVirtualTgt, // Actual virtual address of the new mapping (to be duplicated)
    DWORD* pdwData
    )
{
    int ixSection;
    PSECTION pscn;
    DWORD dwAddr = (DWORD) lpvAddrVirtualSrc;
    DWORD dwFlags;
    BOOL fRet = FALSE;

    DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT("++KdpRemapVirtualMemory: va to remap=0x%08X, va to be dup=0x%08X\r\n"), (DWORD) lpvAddrVirtualSrc, (DWORD) lpvAddrVirtualTgt));

    // We don't want to remap a secure or kernel virtual address, so fail in those cases
    if (IsSecureVa (dwAddr) || IsKernelVa (dwAddr))
    {
        DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT ("  KdpRemapVirtualMemory: Address is secure and/or in kernel space!\r\n")));
    }

    // Get a pointer to the section, and check that its valid
    ixSection = (dwAddr >> VA_SECTION) & SECTION_MASK;
    pscn = SectionTable [ixSection];
    if ((NULL_SECTION == pscn) || (&NKSection == pscn))
    {
        DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT ("  KdpRemapVirtualMemory: Address section pointer is NULL or Kernel\r\n")));
    }
    else
    {
        // TODO: Change this to manipulate page table directly
        // Decommit the blocks in the source, since VirtualCopy requires only reserved blocks
        KdVirtualFree (lpvAddrVirtualSrc, PAGE_SIZE, MEM_DECOMMIT);
        // Get the page access flag
        if (pdwData && *pdwData)
        {
            dwFlags = *pdwData;
        }
        else
        {
            dwFlags = PAGE_EXECUTE_READWRITE;
        }

        // TODO: Change this to manipulate page table directly
        // Call VirtualCopy to perform the work, we need to duplicate the mapping of the target virtual page to source (now unmapped) virtual page
        if (DoVirtualCopy (lpvAddrVirtualSrc, lpvAddrVirtualTgt, PAGE_SIZE, dwFlags))
        {
            // Flush the cache and TLB entries at this point
            kdpInvalidateRange (lpvAddrVirtualSrc, PAGE_SIZE);
            fRet = TRUE;
            DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT ("  KdpRemapVirtualMemory: succeeded\r\n")));
        }
        else
        {
            DEBUGGERMSG (KDZONE_ALERT, (TEXT("  KdpRemapVirtualMemory: DoVirtualCopy failed (Src=0x%08X, Tgt=0x%08X)\r\n"), lpvAddrVirtualSrc, lpvAddrVirtualTgt));
        }
    }

    DEBUGGERMSG (KDZONE_VIRTMEM, (TEXT ("--KdpRemapVirtualMemory\r\n")));
    return fRet;
}


/*++

Routine Name:

    KdpHandlePageIn

Routine Description:

    This function is called by the OS upon a successful page-in.
    The function pointer pfnNKLogPageIn is set to this function.
    Other functions in KD that need to perform work when a page
    in occurs are called within this function.

Arguments:

    ulAddr - the starting address of the page just paged in
    bWrite - TRUE if page is write-accessible, else FALSE

Return Value:

    None

--*/

BOOL
KdpHandlePageIn(
    IN ULONG ulAddress,
    IN ULONG ulNumPages,
    IN BOOL bWrite
    )
{
    DEBUGGERMSG (KDZONE_TRAP && KDZONE_VIRTMEM, (L"++KdpHandlePageIn(addr=0x%08x, pages=%d, writeable=%d)\r\n", ulAddress, ulNumPages, bWrite));
    // Call any KD functions that are interested in page-in's
    KdpHandlePageInBreakpoints (ulAddress, ulNumPages);
    DEBUGGERMSG (KDZONE_TRAP && KDZONE_VIRTMEM, (L"--KdpHandlePageIn\r\n"));
    return FALSE;
}


ULONG ByteToNum (BYTE b)
{
    switch (b) {
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
