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

    KdApi2Structs.h: Declaration of the CE KdApi2 structures

Abstract:

    The following declared structures are used by CE KdApi2 commands (Kernel Debugger interface)

Environment:

    WinCE


--*/

#pragma once


#pragma pack(push, 4)


#define KDBG_PACKET_VERSION (2)
#define KDAPI_PROTOCOL_VERSION (18)


// Kdbg protocol version number. this should be incremented if the
// protocol changes.
// ---- KD_PACKET_HEADER.dwRevision allocations ----
    // V1: until Jan 2004
    // V2: Simplified packet format with client GUID and simpler control (previous one redondant with KITL)

// KdApi protocol version number.  this should be incremented if the
// protocol changes.
// ---- DBGKD_COMMAND/DBGKD_NOTIF.dwSubVersionId allocations ----
    // V1: ?
    // V2: KdStub until Aug 99
    // V3: Temporary version (unused) for Processor Type (postpone to V5)
    // V4: Aug 99 - New Stack Walk: requires extra param (Frame Address) for TranslateRA
    // V5: Add ManipulateBreakpont
    // V6: Support returning PrevSP
    // V7: 256 BP + CPU Family
    // V8: New Module un/load notification with data section (Use DbgKdLoadSymbolsStateChange2) + Reset flag in case of exception too (sc.Thread)
    // V9: ARM VFP10 Support
    // V10: Intel Concan Support
    // V11: Return process handle for current call stack frame
    // V12: Support for Safe (non-forced) VM R/W and KdStub side sanitization
    // V13: Frame Pointer Optimization (FPO) disabled for x86 retail base functions (Will allow PB to unwind FPO frames)
    // V14: Return file time stamp data with load notifications
    // V15: Return hDll, dwInUse, wFlags, and bTrustLevel with load notifications
    // V17: Radically new protocol for eXDI2
    // V18: No more Mod load/unload notif + resync


///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
// KD PACKETS
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////


// {8AE754FC-7157-408c-A39B-469D1D7A8D2D}
static const GUID GUID_KDDBGCLIENT_KDSTUB =
{ 0x8ae754fc, 0x7157, 0x408c, { 0xa3, 0x9b, 0x46, 0x9d, 0x1d, 0x7a, 0x8d, 0x2d } };


// {FD3C9C6A-4E08-44ff-8943-7FEE44FE4FD2}
static const GUID GUID_KDDBGCLIENT_OSAXS =
{ 0xfd3c9c6a, 0x4e08, 0x44ff, { 0x89, 0x43, 0x7f, 0xee, 0x44, 0xfe, 0x4f, 0xd2 } };


typedef struct _KD_PACKET_HEADER
{
    DWORD dwRevision; // Packet format Revision
    GUID  guidClient;
    WORD  wPacketType;
    WORD  wDataByteCount;
    BYTE VPad[8]; // zero'ed
} KD_PACKET_HEADER;


// Packet Types

#define PACKET_TYPE_UNUSED                  (0)
#define PACKET_TYPE_KD_NOTIF                (1)
#define PACKET_TYPE_KD_CMD                  (2)
#define PACKET_TYPE_KD_RESYNC               (3)
#define PACKET_TYPE_MAX                     (3)


///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
// KD API
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// Api Numbers for state change
// Packet type = PACKET_TYPE_KD_NOTIF
/////////////////////////////////////////////////////////

#define DbgKdExceptionNotif           (0x00003030L)


// TODO: Change this with eXDI2 style
#define DBGKD_VERS_FLAG_FPU         (0x0004)      // Target CPU Supports FPU
#define DBGKD_VERS_FLAG_DSP         (0x0008)      // Target CPU Supports DSP
#define DBGKD_VERS_FLAG_MULTIMEDIA  (0x0010)      // Target CPU supports Multimedia


typedef struct _DBGKD_TGTVERSION
{
    DWORD dwCpuFamily; // CPUF_X86, CPUF_SHX, CPUF_MIPS, CPUF_ARM, CPUF_UNK
    DWORD dwCpuCapablilityFlags;
    WORD  wMajorOsVersion;
    WORD  wMinorOsVersion;
    DWORD dwBuildNumber;
    DWORD dwNkCEProcessorType;
    BYTE VPad[16]; // zero'ed
} DBGKD_TGTVERSION;


typedef struct _DBGKD_NBBPAVAIL
{
    DWORD dwNbHwCodeBpAvail;
    DWORD dwNbSwCodeBpAvail;
    DWORD dwNbHwDataBpAvail;
    DWORD dwNbSwDataBpAvail;
    BYTE VPad[8]; // zero'ed
} DBGKD_NBBPAVAIL;


typedef EXCEPTION_RECORD CE_EXCEPTION_RECORD;


typedef struct _CONCAN_REGS
{
    ULONGLONG Wregs[16]; // coprocessor 0 has 16 64-bit registers
    DWORD ConRegs[16]; // coprocessor 1 has 16 32-bit registers
} CONCAN_REGS;


typedef struct _DBGKM_EXCEPTION
{
    CE_EXCEPTION_RECORD ExceptionRecord;
    DWORD dwFirstChance;
    BYTE VPad[16]; // zero'ed
    CONTEXT Context;
#ifdef ARM
    // PB will automatically cat this structure into Context.  If Context changes to include the
    //   concan registers, then remove this #if .. #endif block.
    CONCAN_REGS ConcanRegs;
#endif
} DBGKM_EXCEPTION;


typedef struct _DBGKD_NOTIF
{
    DWORD dwNewState;
    DWORD dwSubVersionId; // Compatible sub version - can be different - both side should try to be backward/forward compatible
    DWORD dwKdpFlags;
    WORD wNumberOfCpus;
    BYTE VPad[18]; // zero'ed
    DBGKD_TGTVERSION TgtVerInfo;
    DBGKD_NBBPAVAIL NbBpAvail;
    union
    {
        DBGKM_EXCEPTION Exception; // Only one type of notification currently
    } u;
} DBGKD_NOTIF;


/////////////////////////////////////////////////////////
// Api Numbers for state manipulation
// Packet type = PACKET_TYPE_KD_CMD
/////////////////////////////////////////////////////////

#define DbgKdReadVirtualMemoryApi           (0x00003130L)
#define DbgKdWriteVirtualMemoryApi          (0x00003131L)
#define DbgKdSetContextApi                  (0x00003133L) // TODO: Merge with Continue
#define DbgKdContinueApi                    (0x00003136L)
#define DbgKdReadControlSpaceApi            (0x00003137L) // TODO: remove
#define DbgKdWriteControlSpaceApi           (0x00003138L) // TODO: remove
#define DbgKdReadIoSpaceApi                 (0x00003139L)
#define DbgKdWriteIoSpaceApi                (0x0000313AL)
#define DbgKdReadPhysicalMemoryApi          (0x0000313DL)
#define DbgKdWritePhysicalMemoryApi         (0x0000313EL)
#define DbgKdManipulateBreakpoint           (0x00003154L)
#define DbgKdTerminateApi                   (0x00003155L)


// Response is a read memory message with data following
typedef struct _DBGKD_READ_MEMORY
{
    __int64 qwTgtAddress;
    DWORD dwTransferCount;
    DWORD dwActualBytesRead;
} DBGKD_READ_MEMORY;

// Data follows directly
typedef struct _DBGKD_WRITE_MEMORY
{
    __int64 qwTgtAddress;
    DWORD dwTransferCount;
    DWORD dwActualBytesWritten;
} DBGKD_WRITE_MEMORY;


typedef struct _DBGKD_CONTINUE
{
    DWORD dwContinueStatus;
} DBGKD_CONTINUE;

typedef struct _DBGKD_READ_WRITE_IO
{
    DWORD dwDataSize;                     // 1, 2, 4
    __int64 qwTgtIoAddress;
    DWORD dwDataValue;
} DBGKD_READ_WRITE_IO;


#define DBGKD_MBP_FLAG_SET          (0x00000001) // Set BP (otherwise Clear BP)
#define DBGKD_MBP_FLAG_DATABP       (0X00000002) // Data Breakpoint (otherwise Code Breakpoint)

typedef struct _DBGKD_MANIPULATE_BREAKPOINT
{
    DWORD dwBpCount;
    DWORD dwMBpFlags;
    BYTE VPad[8]; // zero'ed
    DBGKD_NBBPAVAIL NbBpAvail; // Set on response
} DBGKD_MANIPULATE_BREAKPOINT;


#define DBGKD_BP_FLAG_SOFTWARE      (0x0001) // Specify it as Software BP (otherwize Hardware BP)
#define DBGKD_BP_FLAG_PHYSICAL      (0x0002) // Address mean Physical address (otherwise Virtual address)


#define DBGKD_EXECMODE_32BIT (0) // 32 bit execution mode
#define DBGKD_EXECMODE_16BIT (1) // 16 bit execution mode


typedef struct _DBGKD_CODE_BREAKPOINT_ELEM1
{
    DWORD dwAddress;
    DWORD dwBpHandle;
    WORD wBpFlags;
    WORD wBpExecMode;
} DBGKD_CODE_BREAKPOINT_ELEM1;


#define DBGKD_DBP_FLAG_WRITE        (0x0100) // Trigger on Write Data BP
#define DBGKD_DBP_FLAG_READ         (0x0200) // Trigger on Write Data BP
#define DBGKD_DBP_FLAG_REGISTER     (0x0400) // Address is CPU Register index


typedef struct _DBGKD_DATA_BREAKPOINT_ELEM1
{
    DWORD dwAddress;
    DWORD dwAddressMask;
    DWORD dwBpHandle;
    DWORD dwData;
    DWORD dwDataMask;
    WORD wBpFlags;
    BYTE bWidth;
    BYTE bMemoryMap;
} DBGKD_DATA_BREAKPOINT_ELEM1;


typedef struct _DBGKD_COMMAND
{
    DWORD dwApiNumber;
    DWORD dwSubVersionId; // Compatible sub version - can be different - both side should try to be backward/forward compatible
    DWORD dwReturnStatus;
    DWORD dwKdpFlags;
    BYTE VPad[16]; // zero'ed
    union
    {
        DBGKD_READ_MEMORY ReadMemory;
        DBGKD_WRITE_MEMORY WriteMemory;
        DBGKD_CONTINUE Continue;
        DBGKD_READ_WRITE_IO ReadWriteIo;
        DBGKD_MANIPULATE_BREAKPOINT ManipulateBreakPoint;
        BYTE VPad[48]; // zero'ed
    } u;
} DBGKD_COMMAND;


// Mask for dwKdpFlags
#define DBGKD_STATE_MEMORY_CHANGED  (0x00000001) // KdStub notified the host of a target memory change (for DBGKD_COMMAND only)
#define DBGKD_STATE_DID_RESET       (0x00000002) // KdStub indicates 1st packet since reset (for DBGKD_NOTIF packet only)


// This is the maximum amount of data that can be transferred in a packet
#define KDP_MESSAGE_BUFFER_SIZE (16000)

#define PACKET_DATA_MAX_SIZE (KDP_MESSAGE_BUFFER_SIZE + sizeof (DBGKD_COMMAND))


#pragma pack(pop)

