//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth Test Client
// 
// 
// Module Name:
// 
//      ceport.h
// 
// Abstract:
// 
//      Contains constants to allow SDP portions taken from NT to compile on WinCE.
// 
// 
//------------------------------------------------------------------------------


#ifndef _CEPORT_H_
#define _CEPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#if ! defined (LONGLONG_SIZE)
#define LONGLONG_SIZE   (sizeof(LONGLONG))
#endif

#if ! defined (LONGLONG_MASK)
#define LONGLONG_MASK   (LONGLONG_SIZE - 1)
#endif

#if ! defined (SHORT_SIZE)
#define SHORT_SIZE  (sizeof(USHORT))
#endif

#if ! defined (SHORT_MASK)
#define SHORT_MASK  (SHORT_SIZE - 1)
#endif

#if ! defined (LONG_SIZE)
#define LONG_SIZE       (sizeof(LONG))
#endif

#if ! defined (LONG_MASK)
#define LONG_MASK       (LONG_SIZE - 1)
#endif

#if ! defined (ZERO_THIS)
#define ZERO_THIS() RtlZeroMemory(this, sizeof(*this))
#endif

#if defined (WINCE_EMULATION)
#include <assert.h>
#define ASSERT assert
typedef unsigned long ULONG_PTR, *PULONG_PTR;
typedef long LONG_PTR, *PLONG_PTR;

typedef void * KIRQL;
typedef void * PBRB;
#endif  // WINCE_EMULATION

typedef void * PIRP;
#ifndef PAGED_CODE
#define PAGED_CODE()
#endif


#define DBG_SDP_TRACE          DEBUG_SDP_TRACE
#define DBG_SDP_INFO           DEBUG_SDP_PACKETS
#define DBG_SDP_ERROR          DEBUG_ERROR
#define DBG_SDP_WARNING        DEBUG_WARN
#define SDP_DBG_UUID_ERROR     0
#define SDP_DBG_UUID_INFO      0
#define SDP_DBG_UUID_WARNING   0

#if !defined (NT_SUCCESS)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define STATUS_INSUFFICIENT_RESOURCES     ((NTSTATUS)0xC000009AL)
#define STATUS_REPARSE_POINT_NOT_RESOLVED ((NTSTATUS)0xC0000280L)
#define STATUS_NOT_FOUND                  ((NTSTATUS)0xC0000225L)
#define STATUS_INVALID_BUFFER_SIZE        ((NTSTATUS)0xC0000206L)
#define STATUS_INVALID_NETWORK_RESPONSE  ((NTSTATUS)0xC00000C3L)
#define STATUS_BUFFER_TOO_SMALL         ((NTSTATUS)0xC0000023L)
#define STATUS_DEVICE_PROTOCOL_ERROR     ((NTSTATUS)0xC0000186L)
#define STATUS_NO_MATCH                  ((NTSTATUS)0xC0000272L)
#define STATUS_OBJECT_NAME_COLLISION     ((NTSTATUS)0xC0000035L)
#define STATUS_TOO_MANY_GUIDS_REQUESTED  ((NTSTATUS)0xC0000082L)
#define WIN32_NO_STATUS
#endif // !NT_SUCCESS

#if !defined (STATUS_SUCCESS)
#define STATUS_SUCCESS                    ((NTSTATUS)0x00000000L)
#define STATUS_INVALID_PARAMETER          ((NTSTATUS)0xC000000DL)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#endif // !STATUS_SUCCESS


inline unsigned long DbgPrint(IN PCHAR DebugMessage, ...) { return 0; }


#define _DbgPrintF(mask,string)  
#define SdpPrint(mask,string)

typedef enum _SDP_DATABASE_EVENT_TYPE {
    SdpDatabaseEventNewRecord = 0, 
    SdpDatabaseEventUpdateRecord,
    SdpDatabaseEventRemoveRecord
} SDP_DATABASE_EVENT_TYPE, *PSDP_DATABASE_EVENT_TYPE;

//
// Determine if an argument is present by testing the value of the pointer
// to the argument value.
//
#ifndef ARGUMENT_PRESENT
#define ARGUMENT_PRESENT(ArgumentPointer)    (\
    (CHAR *)(ArgumentPointer) != (CHAR *)(NULL) )
#endif

typedef LONG NTSTATUS, *PNTSTATUS;
typedef struct _DEVICE_OBJECT *PDEVICE_OBJECT;

// RTL byte swapping functions stolen from ntrtl.h

//++
//
// VOID
// RtlRetrieveUshort (
//     PUSHORT DESTINATION_ADDRESS
//     PUSHORT SOURCE_ADDRESS
//     )
//
// Routine Description:
//
// This macro retrieves a USHORT value from the SOURCE address, avoiding
// alignment faults.  The DESTINATION address is assumed to be aligned.
//
// Arguments:
//
//     DESTINATION_ADDRESS - where to store USHORT value
//     SOURCE_ADDRESS - where to retrieve USHORT value from
//
// Return Value:
//
//     none.
//
//--

#define RtlRetrieveUshort(DEST_ADDRESS,SRC_ADDRESS)                   \
         if ((ULONG_PTR)SRC_ADDRESS & SHORT_MASK) {                       \
             ((PUCHAR) DEST_ADDRESS)[0] = ((PUCHAR) SRC_ADDRESS)[0];  \
             ((PUCHAR) DEST_ADDRESS)[1] = ((PUCHAR) SRC_ADDRESS)[1];  \
         }                                                            \
         else {                                                       \
             *((PUSHORT) DEST_ADDRESS) = *((PUSHORT) SRC_ADDRESS);    \
         }                                                            


//++
//
// VOID
// RtlRetrieveUlong (
//     PULONG DESTINATION_ADDRESS
//     PULONG SOURCE_ADDRESS
//     )
//
// Routine Description:
//
// This macro retrieves a ULONG value from the SOURCE address, avoiding
// alignment faults.  The DESTINATION address is assumed to be aligned.
//
// Arguments:
//
//     DESTINATION_ADDRESS - where to store ULONG value
//     SOURCE_ADDRESS - where to retrieve ULONG value from
//
// Return Value:
//
//     none.
//
// Note:
//     Depending on the machine, we might want to call retrieveushort in the
//     unaligned case.
//
//--

#define RtlRetrieveUlong(DEST_ADDRESS,SRC_ADDRESS)                    \
         if ((ULONG_PTR)SRC_ADDRESS & LONG_MASK) {                        \
             ((PUCHAR) DEST_ADDRESS)[0] = ((PUCHAR) SRC_ADDRESS)[0];  \
             ((PUCHAR) DEST_ADDRESS)[1] = ((PUCHAR) SRC_ADDRESS)[1];  \
             ((PUCHAR) DEST_ADDRESS)[2] = ((PUCHAR) SRC_ADDRESS)[2];  \
             ((PUCHAR) DEST_ADDRESS)[3] = ((PUCHAR) SRC_ADDRESS)[3];  \
         }                                                            \
         else {                                                       \
             *((PULONG) DEST_ADDRESS) = *((PULONG) SRC_ADDRESS);      \
         }
// end_ntddk end_wdm


__inline
USHORT
RtlUshortByteSwap(
    IN USHORT Source
    )
/*++

Routine Description:

    The RtlUshortByteSwap function exchanges bytes 0 and 1 of Source
    and returns the resulting USHORT.

Arguments:

    Source - 16-bit value to byteswap.

Return Value:

    Swapped 16-bit value.

--*/
{
    USHORT swapped;

    swapped = ((Source) << (8 * 1)) |
              ((Source) >> (8 * 1));

    return swapped;
}


inline
ULONG
RtlUlongByteSwap(
    IN ULONG Source
    )
/*++

Routine Description:

    The RtlUlongByteSwap function exchanges byte pairs 0:3 and 1:2 of
    Source and returns the resulting ULONG.

Arguments:

    Source - 32-bit value to byteswap.

Return Value:

    Swapped 32-bit value.

--*/
{
    ULONG swapped;

    swapped = ((Source)              << (8 * 3)) |
              ((Source & 0x0000FF00) << (8 * 1)) |
              ((Source & 0x00FF0000) >> (8 * 1)) |
              ((Source)              >> (8 * 3));

    return swapped;
}

#ifdef __cplusplus
}
#endif // __cplusplus


typedef LONG KPRIORITY;
typedef enum _POOL_TYPE {
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS,
    MaxPoolType


    } POOL_TYPE;

typedef struct _KEVENT {
          HANDLE        Event;
        } KEVENT, *PKEVENT;
// From ntdef.h
typedef enum _EVENT_TYPE {
    NotificationEvent,
    SynchronizationEvent
    } EVENT_TYPE;

__inline VOID KeInitializeEvent(PKEVENT Event, EVENT_TYPE Type, BOOLEAN State) {
    *((HANDLE *)Event) = CreateEvent(NULL,(Type == SynchronizationEvent)? TRUE:FALSE,State,NULL);
}


typedef enum _KWAIT_REASON {
    Executive,
    FreePage,
    PageIn,
    PoolAllocation,
    DelayExecution,
    Suspended,
    UserRequest,
    WrExecutive,
    WrFreePage,
    WrPageIn,
    WrPoolAllocation,
    WrDelayExecution,
    WrSuspended,
    WrUserRequest,
    WrEventPair,
    WrQueue,
    WrLpcReceive,
    WrLpcReply,
    WrVirtualMemory,
    WrPageOut,
    WrRendezvous,
    Spare2,
    Spare3,
    Spare4,
    Spare5,
    Spare6,
    WrKernel,
    MaximumWaitReason
    } KWAIT_REASON ;


typedef CCHAR KPROCESSOR_MODE;
#define KernelMode 0
__inline VOID KeWaitForSingleObject(PVOID Object,KWAIT_REASON WaitReason,
                                    KPROCESSOR_MODE WaitMode,BOOLEAN Alertable,PLARGE_INTEGER Timeout) {
    // WARNING - truncating timeout from 64 bit to 32 bit
    DWORD dwMs = Timeout ? Timeout->LowPart : INFINITE;
    ASSERT(Timeout->HighPart == 0);
    WaitForSingleObject(Object, dwMs);
}
// Memory routines
// define ExFreePool(mem) LocalFree(mem)
#define ExFreePool(mem) svsutil_Free(mem,g_pvAllocData)

// #define ExAllocatePoolWithTag(pool,size,tag) LocalAlloc(0,size)
#define ExAllocatePoolWithTag(pool,size,tag) svsutil_Alloc(size,g_pvAllocData)


typedef struct _DISPATCHER_HEADER {
    UCHAR Type;
    UCHAR Absolute;
    UCHAR Size;
    UCHAR Inserted;
    LONG SignalState;
    LIST_ENTRY WaitListHead;
} DISPATCHER_HEADER;

typedef void BThDevice;

typedef enum _SDP_SERVER_LOG_TYPE {
    SdpServerLogTypeServiceSearch = 0,
    SdpServerLogTypeAttributeSearch,
    SdpServerLogTypeServiceSearchAttribute,
    SdpServerLogTypeConnect,
    SdpServerLogTypeDisconnect,
    SdpServerLogTypeServiceSearchResponse,
    SdpServerLogTypeAttributeSearchResponse,
    SdpServerLogTypeServiceSearchAttributeResponse
} SDP_SERVER_LOG_TYPE;

#if defined (SDP_LOGGING)
#error SDP_LOGGING has been defined but shouldn't be for WinCE, no support
#endif

#endif _CEPORT_H_
