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

    kdp.h

Abstract:

    Private include file for the Kernel Debugger subcomponent

Environment:

    WinCE


--*/

// Override kernel's KData.
#define KData (*g_kdKernData.pKData)

#include <winerror.h>

#include "kernel.h"
#include "cpuid.h"
#include "kdstub.h"
#include "hdstub.h"
#include "string.h"
#include "kdpcpu.h"
#include "dbg.h"
#include "KitlProt.h"
#include "osaxs.h"
#include "OsAxsFlexi.h"
#include "kdApi2Structs.h"
#include "osaxsprotocol.h"


// status Constants for Packet waiting
// TODO: remove this since we use KITL
#define KDP_PACKET_RECEIVED    0x0000
#define KDP_PACKET_RESEND      0x0001
#define KDP_PACKET_UNEXPECTED  0x0002
#define KDP_PACKET_NONE        0xFFFF


#ifdef SHx
// for SR_DSP_ENABLED and SR_FPU_DISABLED
#include "shx.h"
#endif

// Useful thing to have.
#define lengthof(x)                     (sizeof(x) / sizeof(*x))


extern DBGPARAM dpCurSettings;

#define KDZONE_INIT             DEBUGZONE(0)    /* 0x0001 */
#define KDZONE_TRAP             DEBUGZONE(1)    /* 0x0002 */
#define KDZONE_API              DEBUGZONE(2)    /* 0x0004 */
#define KDZONE_DBG              DEBUGZONE(3)    /* 0x0008 */
#define KDZONE_SWBP             DEBUGZONE(4)    /* 0x0010 */
#define KDZONE_BREAK            DEBUGZONE(5)    /* 0x0020 */
#define KDZONE_CTRL             DEBUGZONE(6)    /* 0x0040 */
#define KDZONE_MOVE             DEBUGZONE(7)    /* 0x0080 */
#define KDZONE_KERNCTXADDR      DEBUGZONE(8)    /* 0x0100 */
#define KDZONE_PACKET           DEBUGZONE(9)    /* 0x0200 */
#define KDZONE_STACKW           DEBUGZONE(10)   /* 0x0400 */
#define KDZONE_CONCAN           DEBUGZONE(11)   /* 0x0800 */
#define KDZONE_VIRTMEM          DEBUGZONE(12)   /* 0x1000 */
#define KDZONE_HANDLEEX         DEBUGZONE(13)   /* 0x2000 */
#define KDZONE_ALERT            DEBUGZONE(15)   /* 0x8000 */

#define KDZONE_FLEXPTI          KDZONE_DBG


#define KDZONE_DEFAULT          (0x8000) // KDZONE_ALERT

#define _O_RDONLY   0x0000  /* open for reading only */
#define _O_WRONLY   0x0001  /* open for writing only */
#define _O_RDWR     0x0002  /* open for reading and writing */
#define _O_APPEND   0x0008  /* writes done at eof */

#define _O_CREAT    0x0100  /* create and open file */
#define _O_TRUNC    0x0200  /* open and truncate */
#define _O_EXCL     0x0400  /* open only if file doesn't already exist */

extern VOID NKOtherPrintfW(LPWSTR lpszFmt, ...);
#define DEBUGGERPRINTF NKOtherPrintfW
#include "debuggermsg.h"


// version of Kd.dll
#define CUR_KD_VER (500)


// ------------------------------- OS Access specifics --------------------------


// DmKdReadControlSpace Api commands

#define HANDLE_PROCESS_INFO_REQUEST         (0) 
#define HANDLE_GET_NEXT_OFFSET_REQUEST      (1)
#define HANDLE_STACKWALK_REQUEST            (2)
#define HANDLE_THREADSTACK_REQUEST          (3)
#define HANDLE_THREADSTACK_TERMINATE        (4)
#define HANDLE_RELOAD_MODULES_REQUEST       (5)
#define HANDLE_RELOAD_MODULES_INFO          (6)
#define HANDLE_PROCESS_ZONE_REQUEST         (7)
#define HANDLE_PROCESS_THREAD_INFO_REQ      (10)
#define HANDLE_GETCURPROCTHREAD             (11)
#define HANDLE_GET_EXCEPTION_REGISTRATION   (12)
#define HANDLE_MODULE_REFCOUNT_REQUEST      (13)
#define HANDLE_DESC_HANDLE_DATA             (14)
#define HANDLE_GET_HANDLE_DATA              (15)

#include <pshpack1.h>

// DmKdReadControlSpace Api structures

// structures for HANDLE_RELOAD_MODULES_INFO protocol
typedef struct tagReloadModInfoBase
{
    DWORD dwBasePtr;
    DWORD dwModuleSize;
} DBGKD_RELOAD_MOD_INFO_BASE;

typedef struct tagReloadModInfoV8
{
    DWORD dwRwDataStart;
    DWORD dwRwDataEnd;
} DBGKD_RELOAD_MOD_INFO_V8;

typedef struct tagReloadModInfoV14
{
    DWORD dwTimeStamp;
} DBGKD_RELOAD_MOD_INFO_V14;

/*
    For processes:
        hDll            = NULL
        dwInUse         = 1 << pid
        wFlags          = 0
        bTrustLevel     = proc.bTrustLevel

    For modules:
        hDll            = &mod
        dwInUse         = mod.inuse
        wFlags          = mod.wFlags
        bTrustLevel     = mod.bTrustLevel
*/

typedef struct tagReloadModInfoV15
{
    HMODULE hDll;
    DWORD dwInUse;
    WORD wFlags;
    BYTE bTrustLevel;
} DBGKD_RELOAD_MOD_INFO_V15;

#include <poppack.h>

#include <pshpack4.h>

//
// structures for HANDLE_MODULE_REFCOUNT_REQUEST protocol
//
// also in:
// /tools/ide/debugger/dmcpp/kdapi.cpp and
// /tools/ide/debugger/odcpu/odlib/datamgr.cpp
typedef struct tagGetModuleRefCountProc
{
    WORD wRefCount;

    // This is not a string. It is an array of characters. It probably won't
    // be null-terminated.
    WCHAR szProcName[15];
} DBGKD_GET_MODULE_REFCNT_PROC;

typedef struct tagGetModuleRefCount
{
    UINT32 nProcs;

    // Array with length = nProcs
    DBGKD_GET_MODULE_REFCNT_PROC pGMRCP[];
} DBGKD_GET_MODULE_REFCNT;

// structures and defines for HANDLE_DESC_HANDLE_DATA

// DBGKD_HANDLE_FIELD_DESC.nType
#define KD_FIELD_UINT                   0   // unsigned int
#define KD_FIELD_SINT                   1   // signed int
#define KD_FIELD_CHAR                   2   // ASCII character
#define KD_FIELD_WCHAR                  3   // Unicode character
#define KD_FIELD_CHAR_STR               4   // ASCII string pointer
#define KD_FIELD_WCHAR_STR              5   // Unicode string pointer
#define KD_FIELD_PTR                    6   // Pointer (any type)
#define KD_FIELD_BOOL                   7   // Boolean (true/false)
#define KD_FIELD_HANDLE                 8   // Handle (any type)
#define KD_FIELD_BITS                   9   // bit array (size <= 32)

// Some useful aliases
#define KD_FIELD_INT                    KD_FIELD_SINT
#define KD_FIELD_BOOLEAN                KD_FIELD_BOOL
#define KD_FIELD_WIDE_STR               KD_FIELD_WCHAR_STR

// DBGKD_HANDLE_FIELD_DESC.nFieldId
//
// Minimal implementation requires KD_HDATA_HANDLE, KD_HDATA_AKY, and
// KD_HDATA_TYPE
//
#define KD_HDATA_HANDLE                 0   // Handle value
#define KD_HDATA_AKY                    1   // Handle access key
#define KD_HDATA_REFCNT                 2   // Total refs to handle in system
#define KD_HDATA_TYPE                   3   // Handle type
#define KD_HDATA_NAME                   4   // Handle name, NULL if none
#define KD_HDATA_THREAD_SUSPEND         5   // Thread suspend count
#define KD_HDATA_THREAD_PID             6   // Thread's parent process
#define KD_HDATA_THREAD_BPRIO           7   // Thread's base priority
#define KD_HDATA_THREAD_CPRIO           8   // Thread's current priority
#define KD_HDATA_THREAD_KTIME           9   // Thread's time spent in kmode
#define KD_HDATA_THREAD_UTIME           10  // Thread's time spent in user mode
#define KD_HDATA_PROC_PID               11  // Process's PID
#define KD_HDATA_PROC_TRUST             12  // Process's trust level
#define KD_HDATA_PROC_VMBASE            13  // ??
#define KD_HDATA_PROC_BASEPTR           14  // ??
#define KD_HDATA_PROC_CMDLINE           15  // Process's commandline
#define KD_HDATA_EVENT_STATE            16  // Event's current state
#define KD_HDATA_EVENT_RESET            17  // Event's manual reset property
#define KD_HDATA_MUTEX_LOCKCNT          18  // Mutex's lock count
#define KD_HDATA_MUTEX_OWNER            19  // Mutex's current owner
#define KD_HDATA_SEM_COUNT              20  // Semaphore's lock counter
#define KD_HDATA_SEM_MAXCOUNT           21  // Semaphore's maximum locks allowed
#define KD_HDATA_FILE_NAME              22  // File's name

typedef struct
{
    UINT16 nType;

    // This is a unique ID that maps in PB to the name of the field. There is
    // a table that correlates these to strings.
    UINT16 nFieldId;
} DBGKD_HANDLE_FIELD_DESC;

typedef union
{
    struct
    {
        // These are both bit arrays that filter out handle data. Note that
        // MAX_PROCESSES == 32 and NUM_SYSTEM_SETS == 32, so they're both
        // 32-bit values. If you don't want to filter, use -1 (all bits set)
        UINT32 nPIDFilter;
        UINT32 nAPIFilter;
    } in;

    struct
    {
        UINT32 cFields;

        // The length of the array goes up to the MTU for KITL. It holds the
        // common subset of properties shared by all the handles requested.
        DBGKD_HANDLE_FIELD_DESC pFieldDesc[];
    } out;
} DBGKD_HANDLE_DESC_DATA;

//
// structures and defines for HANDLE_GET_HANDLE_DATA
//

typedef struct
{
    // This is a unique ID that maps in PB to the name of the field. There is
    // a table that correlates these to string IDs.
    UINT16 nFieldId;

    // Determine whether field is valid. This happens sometimes, e.g. when a
    // thread is still referenced but the thread itself has died.
    BOOL fValid : 1;

    // The data
    UINT32 nData;
} DBGKD_HANDLE_FIELD_DATA;

typedef union
{
    struct
    {
        // These are both bit arrays that filter out handle data. Note that
        // MAX_PROCESSES == 32 and NUM_SYSTEM_SETS == 32, so they're both
        // 32-bit values. If you don't want to filter, use -1 (all bits set)
        UINT32 nPIDFilter;
        UINT32 nAPIFilter;

        // Index for continuation. Starts at NULL, should be copied from the
        // out part for each iteration.
        HANDLE hStart;
    } in;

    struct
    {
        // NULL if finished, otherwise this packet should be sent again with
        // out.hContinue copied into in.hStart
        HANDLE hContinue;

        // The length of pFields is expanded to fit the MTU. The order of the
        // fields follows the order of the handles in the kernel's handle list.
        // Fields belonging to the same handle will be clustered together in
        // identical order to the DESC_DATA query. If there is not sufficient
        // space to store complete data for a handle, it will not be stored.
        //
        // Note that cFields = cFieldsPerHandle * cHandles
        UINT32 cFields;
        DBGKD_HANDLE_FIELD_DATA pFields[];
    } out;
} DBGKD_HANDLE_GET_DATA;

typedef DBGKD_HANDLE_FIELD_DESC *PDBGKD_HANDLE_FIELD_DESC;
typedef const DBGKD_HANDLE_FIELD_DESC *PCDBGKD_HANDLE_FIELD_DESC;
typedef DBGKD_HANDLE_DESC_DATA *PDBGKD_HANDLE_DESC_DATA;
typedef const DBGKD_HANDLE_DESC_DATA *PCDBGKD_HANDLE_DESC_DATA;
typedef DBGKD_HANDLE_FIELD_DATA *PDBGKD_HANDLE_FIELD_DATA;
typedef const DBGKD_HANDLE_FIELD_DATA *PCDBGKD_HANDLE_FIELD_DATA;
typedef DBGKD_HANDLE_GET_DATA *PDBGKD_HANDLE_GET_DATA;
typedef const DBGKD_HANDLE_GET_DATA *PCDBGKD_HANDLE_GET_DATA;
#include <poppack.h>

//
// WriteControlSpace Api commands
//

#define HANDLE_PROCESS_SWITCH_REQUEST  0
#define HANDLE_THREAD_SWITCH_REQUEST   1
//#define HANDLE_STACKWALK_REQUEST       2
#define HANDLE_DELETE_HANDLE           3



// ------------------------------- END of OS Access specifics --------------------------



extern BOOL g_fForceReload;
extern BOOL g_fKdbgRegistered;


// KdStub State Notification Flags
extern BOOL g_fDbgKdStateMemoryChanged; // Set this signal to TRUE to notify the host that target memory has changed and host-side must refresh


#define PAGE_ALIGN(Va)  ((ULONG)(Va) & ~(PAGE_SIZE - 1))
#define BYTE_OFFSET(Va) ((ULONG)(Va) & (PAGE_SIZE - 1))


//
// Ke stub routines and definitions
//


#if defined(x86)

//
// There is no need to sweep the i386 cache because it is unified (no
// distinction is made between instruction and data entries).
//

#define KeSweepCurrentIcache()

#elif defined(SHx)

//
// There is no need to sweep the SH3 cache because it is unified (no
// distinction is made between instruction and data entries).
//

extern void FlushCache (void);

#define KeSweepCurrentIcache() FlushCache()

#else

extern void FlushICache (void);

#define KeSweepCurrentIcache() FlushICache()

#endif


#define VER_PRODUCTBUILD 0


#define STATUS_SYSTEM_BREAK             ((NTSTATUS)0x80000114L)


//
// TRAPA / BREAK immediate field value for breakpoints
//

#define DEBUGBREAK_STOP_BREAKPOINT         1

#define DEBUG_PROCESS_SWITCH_BREAKPOINT       2
#define DEBUG_THREAD_SWITCH_BREAKPOINT        3
#define DEBUG_BREAK_IN                        4
#define DEBUG_REGISTER_BREAKPOINT             5


#if defined (ARM)

// returns TRUE if Concan Coprocessors found and active,
BOOL DetectConcanCoprocessors ();

void GetConcanRegisters (PCONCAN_REGS);
void SetConcanRegisters (PCONCAN_REGS);

#endif

typedef ULONG KSPIN_LOCK;

//
// Miscellaneous
//

#if DBG

#define KD_ASSERT(exp)
    DEBUGGERMSG (KDZONE_ALERT, (L"**** KD_ASSERT ****" L##exp "\r\n"))

#else

#define KD_ASSERT(exp)

#endif


#define BREAKPOINT_TABLE_SIZE (256) // TODO: move this in HDSTUB.h


//
// Define breakpoint table entry structure.
//

// FLAGS
#define KD_BREAKPOINT_SUSPENDED             (0x01) // original instruction of SW BP is temporary restored (typically to prevent KD hitting that BP)
#define KD_BREAKPOINT_16BIT                 (0x02)
#define KD_BREAKPOINT_INROM                 (0x04) // Indicate that the BP is in ROM (this is useful only to detect duplicates using both current Address and KAddress)
#define KD_BREAKPOINT_WRITTEN               (0x08) // Indicate that the BP was written. (useful for delayed assembly breakpoints.)


typedef struct _BREAKPOINT_ENTRY {
    PVOID Address;                  // Address that the user specified for bp
    PVOID KAddr; // Address that the breakpoint was written to.  We need to keep this around
                 // for cases in which the virtual mapping for a module's memory is lost before
                 // the unload notification.
    WORD wRefCount;
    BYTE Flags;
    KDP_BREAKPOINT_TYPE Content;
} BREAKPOINT_ENTRY, *PBREAKPOINT_ENTRY;


// Breakpoint special Handles for error passing

#define KD_BPHND_ROMBP_SUCCESS (1)
#define KD_BPHND_INVALID_GEN_ERR (0)
#define KD_BPHND_ROMBP_ERROR_INSUFFICIENT_PAGES (-1)
#define KD_BPHND_ERROR_COPY_FAILED (-2)
#define KD_BPHND_ROMBP_ERROR_REMAP_FAILED (-3)


// ROM Breakpoints structures

#define NB_ROM2RAM_PAGES (10)

typedef struct _ROM2RAM_PAGE_ENTRY
{
    void* pvROMAddr;
    BYTE* pbRAMAddr;
    void* pvROMAddrKern;
    int nBPCount;
} ROM2RAM_PAGE_ENTRY;

extern ROM2RAM_PAGE_ENTRY g_aRom2RamPageTable [NB_ROM2RAM_PAGES];
extern BYTE g_abRom2RamDataPool [((NB_ROM2RAM_PAGES + 1) * PAGE_SIZE) - 1];


#if defined(SHx)
void LoadDebugSymbols(void);

//
// User Break Controller memory-mapped addresses
//
#if SH4
#define UBCBarA  0xFF200000        // 32 bit Break Address A
#define UBCBamrA 0xFF200004        // 8 bit  Break Address Mask A
#define UBCBbrA  0xFF200008        // 16 bit Break Bus Cycle A
#define UBCBasrA 0xFF000014        // 8 bit  Break ASID A
#define UBCBarB  0xFF20000C       // 32 bit Break Address B
#define UBCBamrB 0xFF200010       // 8 bit  Break Address Mask B
#define UBCBbrB  0xFF200014       // 16 bit Break Bus Cycle A
#define UBCBasrB 0xFF000018       // 8 bit  Break ASID B
#define UBCBdrB  0xFF200018       // 32 bit Break Data B
#define UBCBdmrB 0xFF20001C       // 32 bit Break Data Mask B
#define UBCBrcr  0xFF200020       // 16 bit Break Control Register
#else
#define UBCBarA    0xffffffb0
#define UBCBamrA   0xffffffb4
#define UBCBbrA    0xffffffb8
#define UBCBasrA   0xffffffe4
#define UBCBarB    0xffffffa0
#define UBCBamrB   0xffffffa4
#define UBCBbrB    0xffffffa8
#define UBCBasrB   0xffffffe8
#define UBCBdrB    0xffffff90
#define UBCBdmrB   0xffffff94
#define UBCBrcr    0xffffff98
#endif
#endif

#define READ_REGISTER_UCHAR(addr) (*(volatile unsigned char *)(addr))
#define READ_REGISTER_USHORT(addr) (*(volatile unsigned short *)(addr))
#define READ_REGISTER_ULONG(addr) (*(volatile unsigned long *)(addr))

#define WRITE_REGISTER_UCHAR(addr,val) (*(volatile unsigned char *)(addr) = (val))
#define WRITE_REGISTER_USHORT(addr,val) (*(volatile unsigned short *)(addr) = (val))
#define WRITE_REGISTER_ULONG(addr,val) (*(volatile unsigned long *)(addr) = (val))

//
// Define Kd function prototypes.
//
#if defined(MIPS_HAS_FPU) || defined(SH4) || defined(x86) || defined (ARM)
VOID FPUFlushContext (VOID);
#endif

#if defined(SHx) && !defined(SH3e) && !defined(SH4)
VOID DSPFlushContext (VOID);
#endif

void KdpResetBps (void);

VOID
KdpReboot (
    IN BOOL fReboot
    );

ULONG
KdpAddBreakpoint (
    IN PVOID Address
    );

BOOLEAN
KdpDeleteBreakpoint (
    IN ULONG Handle
    );

VOID
KdpDeleteAllBreakpoints (
    VOID
    );

ULONG
KdpMoveMemory (
    IN PVOID Destination,
    IN PVOID Source,
    IN ULONG Length
    );

HDATA *
KdHandleToPtr (
    IN HANDLE hHandle
    );

BOOL
KdValidateHandle (
    IN HANDLE hHandle
    );

BOOL
KdValidateHandlePtr (
    IN HDATA *phHandle
    );

UINT
KdGetProcHandleRef (
    IN HDATA *phHandle,
    IN UINT nPID
    );

NTSTATUS
KdQueryHandleFields (
    IN OUT DBGKD_HANDLE_DESC_DATA *pHandleFields,
    IN UINT nBufLen
    );

NTSTATUS
KdQueryOneHandle (
    IN HANDLE hHandle,
    OUT DBGKD_HANDLE_GET_DATA *pHandleBuffer,
    IN UINT nBufLen
    );

NTSTATUS
KdQueryHandleList (
    IN OUT DBGKD_HANDLE_GET_DATA *pHandleBuffer,
    IN UINT nBufLen
    );

USHORT
KdpReceiveCmdPacket (
    OUT PSTRING MessageHeader,
    OUT PSTRING MessageData,
    OUT PULONG DataLength,
    OUT GUID *pguidClient
    );

VOID
KdpSendPacket (
    IN WORD dwPacketType,
    IN GUID guidClient,
    IN PSTRING MessageHeader,
    IN PSTRING MessageData OPTIONAL
    );

VOID
KdpSendKdApiCmdPacket (
    IN PSTRING MessageHeader,
    IN PSTRING MessageData OPTIONAL
    );

ULONG
KdpTrap (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN CONTEXT * ContextRecord,
    IN BOOLEAN SecondChance
    );

BOOL KdpModLoad (DWORD);
BOOL KdpModUnload (DWORD);

BOOL
KdpSanitize(
    BYTE* pbClean,
    VOID* pvMem,
    ULONG nSize,
    BOOL fAlwaysCopy
    );

BOOLEAN
KdpReportExceptionNotif (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN SecondChance
    );


BOOLEAN
KdpSendNotifAndDoCmdLoop(
    IN PSTRING MessageHeader,
    IN PSTRING MessageData OPTIONAL
    );

VOID
KdpReadVirtualMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpWriteVirtualMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpReadPhysicalMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpWritePhysicalMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpSetContext(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpWriteBreakpoint(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpRestoreBreakpoint(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpReadControlSpace(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpWriteControlSpace(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpReadIoSpace(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData,
    IN BOOL fSendPacket
    );

VOID
KdpWriteIoSpace(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData,
    IN BOOL fSendPacket
    );

NTSTATUS
KdpWriteBreakPointEx(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpRestoreBreakPointEx(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpManipulateBreakPoint(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
);

VOID KdpSuspendAllBreakpoints(
    VOID
);

VOID
KdpReinstateSuspendedBreakpoints(
    VOID
);

BOOLEAN
KdpSuspendBreakpointIfHitByKd(
    IN VOID* Address
);

BOOL
KdpHandlePageIn(
    IN ULONG ulAddress,
    IN ULONG ulNumPages,
    IN BOOL bWrite
);


VOID
KdpHandlePageInBreakpoints(
    ULONG ulAddress,
    ULONG ulNumPages
);

void EnableHDNotifs (BOOL fEnable);


// Define external references.

extern int g_nTotalNumDistinctSwCodeBps;
extern UCHAR g_abMessageBuffer[KDP_MESSAGE_BUFFER_SIZE];
extern BOOL g_fDbgConnected;
extern CRITICAL_SECTION csDbg;
extern CONTEXT *g_pctxException;

// primary interface between nk and kd
extern KERNDATA g_kdKernData;
extern void (*g_pfnOutputDebugString)(char*, ...);

extern HDSTUB_DATA Hdstub;
extern HDSTUB_CLIENT g_KdstubClient;
extern SAVED_THREAD_STATE g_svdThread;

#define pTOC                        (g_kdKernData.pTOC)
#define kdpKData                    (g_kdKernData.pKData)
#define kdProcArray                 (g_kdKernData.pProcArray)
#define pHandleList                 (g_kdKernData.pHandleList)
#define pVAcs                       (g_kdKernData.pVAcs)
#define NullSection                 (*(g_kdKernData.pNullSection))
#define NKSection                   (*(g_kdKernData.pNKSection))
#define KCall                       (g_kdKernData.pKCall)
#define kdpInvalidateRange          (g_kdKernData.pInvalidateRange)
#define DoVirtualCopy               (g_kdKernData.pDoVirtualCopy)
#define KdVirtualFree               (g_kdKernData.pVirtualFree)
#define KdCloseHandle               (g_kdKernData.pCloseHandle)
#define kdpIsROM                    (g_kdKernData.pkdpIsROM)
#define KdCleanup                   (g_kdKernData.pKdCleanup)
#define KDEnableInt                 (g_kdKernData.pKDEnableInt)
#define pfnIsDesktopDbgrExist       (g_kdKernData.pfnIsDesktopDbgrExist)
#define NKwvsprintfW                (g_kdKernData.pNKwvsprintfW)
#define NKDbgPrintfW                (g_kdKernData.pNKDbgPrintfW)
#define pulHDEventFilter            (g_kdKernData.pulHDEventFilter)
#if defined(MIPS)
#define InterlockedDecrement        (g_kdKernData.pInterlockedDecrement)
#define InterlockedIncrement        (g_kdKernData.pInterlockedIncrement)
#endif
#if defined(ARM)
#define InSysCall                   (g_kdKernData.pInSysCall)
#endif
#if defined(x86)
#define MD_CBRtn                    (*(DWORD*)g_kdKernData.pMD_CBRtn)
#else
#define MD_CBRtn                    (g_kdKernData.pMD_CBRtn)
#endif

extern BOOL KDIoControl (DWORD dwIoControlCode, LPVOID lpBuf, DWORD nBufSize);


typedef struct {
    ULONG Addr;                 // pc address of breakpoint
    ULONG Flags;                // Flags bits
    ULONG Calls;                // # of times traced routine called
    ULONG CallsLastCheck;       // # of calls at last periodic (1s) check
    ULONG MaxCallsPerPeriod;
    ULONG MinInstructions;      // largest number of instructions for 1 call
    ULONG MaxInstructions;      // smallest # of instructions for 1 call
    ULONG TotalInstructions;    // total instructions for all calls
    ULONG Handle;               // handle in (regular) bpt table
    PVOID Thread;               // Thread that's skipping this BP
    ULONG ReturnAddress;        // return address (if not COUNTONLY)
} DBGKD_INTERNAL_BREAKPOINT, *PDBGKD_INTERNAL_BREAKPOINT;

#define MapPtrInProc(Ptr, Proc) (((DWORD)(Ptr)>>VA_SECTION) ? (LPVOID)(Ptr) : \
        (LPVOID)((DWORD)(Ptr)|(DWORD)Proc->dwVMBase))

void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx);

#ifdef MIPSII
#define Is16BitSupported         (kdpKData->fMIPS16Sup)
#elif defined (THUMBSUPPORT)
#define Is16BitSupported         (1)
#else
#define Is16BitSupported         (0)
#endif

