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

    kdp.h

Abstract:

    Private include file for the Kernel Debugger subcomponent

Environment:

    WinCE


--*/

#include <kernel.h>
#include <winerror.h>
#include <kdbgpublic.h>
#include <kdbgprivate.h>
#include <kdbgerrors.h>
#include <cpuid.h>
#include <string.h>
#include <kdpcpu.h>
#include <dbg.h>
#include <KitlProt.h>
#include <OsAxsFlexi.h>
#include <kdApi2Structs.h>
#include <osaxsprotocol.h>
#include <spinlock.h>

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
#define KDZONE_INTELWMMX        DEBUGZONE(11)   /* 0x0800 */
#define KDZONE_VIRTMEM          DEBUGZONE(12)   /* 0x1000 */
#define KDZONE_HANDLEEX         DEBUGZONE(13)   /* 0x2000 */
#define KDZONE_DBGMSG2KDAPI     DEBUGZONE(14)   /* 0x4000 */ // This flag indicate that KD Debug Messages must be re-routed to KDAPI to be displayed in the target debug output instead of the Serial output
#define KDZONE_ALERT            DEBUGZONE(15)   /* 0x8000 */

#define KDZONE_FLEXPTI          KDZONE_DBG

#define KDZONE_DEFAULT          (0x8000) // 

#define _O_RDONLY   0x0000  /* open for reading only */
#define _O_WRONLY   0x0001  /* open for writing only */
#define _O_RDWR     0x0002  /* open for reading and writing */
#define _O_APPEND   0x0008  /* writes done at eof */

#define _O_CREAT    0x0100  /* create and open file */
#define _O_TRUNC    0x0200  /* open and truncate */
#define _O_EXCL     0x0400  /* open only if file doesn't already exist */

// version of Kd.dll
#define CUR_KD_VER (600)

// ------------------------------- END of OS Access specifics --------------------------

extern BOOL g_fForceReload;
extern BOOL g_fKdbgRegistered;


// KdStub State Notification Flags
extern BOOL g_fDbgKdStateMemoryChanged; // Set this signal to TRUE to notify the host that target memory has changed and host-side must refresh


#define PAGE_ALIGN(Va)  ((ULONG)(Va) & ~(VM_PAGE_SIZE - 1))
#define BYTE_OFFSET(Va) ((ULONG)(Va) & (VM_PAGE_SIZE - 1))


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

BOOL HasWMMX();

void GetWMMXRegisters(PCONCAN_REGS);
void SetWMMXRegisters(PCONCAN_REGS);

#endif

typedef ULONG KSPIN_LOCK;

//
// Miscellaneous
//

#define BREAKPOINT_TABLE_SIZE (256) // TODO: move this in HDSTUB.h


//
// Define breakpoint table entry structure.
//

typedef struct _BREAKPOINT_ENTRY {
    PPROCESS pVM; // VM Associated with breakpoint.
    ULONG ThreadID;
    PVOID Address; // Address that the user specified for bp
    Breakpoint *BpHandle;
    WORD wRefCount;
    BYTE f16Bit : 1;
    BYTE fHardware : 1;
} BREAKPOINT_ENTRY, *PBREAKPOINT_ENTRY;


// Breakpoint special Handles for error passing

#define KD_BPHND_ROMBP_SUCCESS (1)
#define KD_BPHND_INVALID_GEN_ERR (0)
#define KD_BPHND_ROMBP_ERROR_INSUFFICIENT_PAGES (-1)
#define KD_BPHND_ERROR_COPY_FAILED (-2)
#define KD_BPHND_ROMBP_IN_KERNEL (-3)
#define KD_BPHND_ROMBP_ERROR_PAGING_OFF (-4)


// ROM Breakpoints structures

#define NB_ROM2RAM_PAGES (16)

extern BOOL g_fFilterExceptions;

BOOL FilterTrap(EXCEPTION_RECORD *, CONTEXT *, BOOL, BOOL*);


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


void KdpResetBps (void);

VOID
KdpReboot (
    IN BOOL fReboot
    );

ULONG KdpAddBreakpoint(PPROCESS ,ULONG, PVOID ,BOOL);

BOOL
KdpDeleteBreakpoint (
    IN ULONG Handle
    );

VOID
KdpDeleteAllBreakpoints (
    VOID
    );

DWORD
KdpFindBreakpoint(void* pvOffset);


USHORT
KdpReceivePacket (
    PSTRING MessageData,
    PULONG DataLength,
    GUID *pguidClient
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

VOID 
KdpSendDbgMessage(
    IN CHAR*  pszMessage,
    IN DWORD  dwMessageSize
    );

BOOL
KdpTrap (
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT * ContextRecord,
    BOOL SecondChance,
    BOOL *pfIgnore
    );

BOOL KdpContinue(DBGKD_COMMAND *pdbgkdCmdPacket, PSTRING AdditionalData);

BOOL
KdpReportExceptionNotif (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOL SecondChance
    );


BOOL
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
KdpAdvancedCmd(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    );

VOID
KdpGetContext(
    DBGKD_COMMAND *pdbgkdCmdPacket,
    PSTRING AdditionalData
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


VOID
KdpManipulateActionPoint(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
);


BOOL KdpInitMemAccess (void);
void KdpDeInitMemAccess (void);
BOOL KdpHandleKdApi(STRING *, BOOL *);
BOOL KdpRegisterKITL(CONTEXT *pContextRecord, BOOL SecondChance);

extern DWORD *g_pdwModLoadNotifDbg;

void KdpEnable(BOOL);
void EnableHDNotifs (BOOL fEnable);

BOOL KdpWasDeletedBreakpoint (DWORD FaultPC);


// Define external references.

extern int g_nTotalNumDistinctSwCodeBps;
extern UCHAR g_abMessageBuffer[KDP_MESSAGE_BUFFER_SIZE];
extern volatile BOOL g_fDbgConnected;
extern volatile SPINLOCK g_DebuggerSpinLock;
extern EXCEPTION_INFO g_exceptionInfo;

extern DEBUGGER_DATA *g_pDebuggerData;


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

void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx);

#ifdef MIPSII
#define Is16BitSupported         (kdpKData->dwArchFlags & MIPS_FLAG_MIPS16)
#elif defined (THUMBSUPPORT)
#define Is16BitSupported         (1)
#else
#define Is16BitSupported         (0)
#endif

