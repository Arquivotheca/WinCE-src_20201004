//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
NT Support routines.
ntddi.h
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __NTPROTO__
#define __NTPROTO__

#include <ntddk.h>
#include <ndis.h>

/*
//
// Structure used for representing Global Properties
//
typedef struct _NtProtoGlobal {

    ULONG               ulSignature;

    // Initialization sequence flag
    ULONG               ulInitSeqFlag;

    ULONG               ulNumCreates;

    // Global Properties - maintained for reference (to return in Get calls)
    ULONG               ulRecvDelay;        // millisecs
    ULONG               ulSendDelay;        // millisecs
    ULONG               ulSimulateSendPktLoss;  // Percentage
    ULONG               ulSimulateRecvPktLoss;  // Percentage
    BOOLEAN             fSimulateResLimit;
    BOOLEAN             fDontRecv;
    BOOLEAN             fDontSend;


    NDIS_HANDLE         ProtHandle;
    NDIS_HANDLE         MiniportHandle;

    PATMSM_ADAPTER      pAdapterList;
    ULONG               ulAdapterCount;
    PDRIVER_OBJECT      pDriverObject;
    PDEVICE_OBJECT      pDeviceObject;

    // General purpose Lock
    NDIS_SPIN_LOCK      Lock;

}NTPROTO_GLOBAL, *PNTPROTO_GLOBAL;

*/

#if (!defined(NDIS_PROTOCOL_MAJOR_VERSION) && !defined(NDIS_PROTOCOL_MINOR_VERSION))
#if (defined(NDIS51))
#define NDIS_PROTOCOL_MAJOR_VERSION 5
#define NDIS_PROTOCOL_MINOR_VERSION 1
#elif (defined(NDIS50))
#define NDIS_PROTOCOL_MAJOR_VERSION 5
#define NDIS_PROTOCOL_MINOR_VERSION 0
#endif
#endif


#define NTPROTO_DEVICE_NAME     L"\\Device\\NDP"
#define NTPROTO_SYMBOLIC_NAME   L"\\DosDevices\\NDP"

#define TraceIn(_X_)			
#define TraceOut(_X_)

#ifdef			DBG

#define NDP_GET_ENTRY_IRQL(_Irql) \
            _Irql = KeGetCurrentIrql()

#define NDP_CHECK_EXIT_IRQL(_EntryIrql, _ExitIrql) \
        {                                       \
            _ExitIrql = KeGetCurrentIrql();     \
            if (_ExitIrql != _EntryIrql)        \
            {                                   \
                DbgPrint("File %s, Line %d, Exit IRQ %d != Entry IRQ %d\n", \
                        __FILE__, __LINE__, _ExitIrql, _EntryIrql);         \
                DbgBreakPoint();                \
            }                                   \
        }

#else		  //DBG		

#define NDP_GET_ENTRY_IRQL(_Irql)		
#define NDP_CHECK_EXIT_IRQL(_EntryIrql, _ExitIrql)		

#endif		  //DBG

NTSTATUS DriverEntry(
   IN  PDRIVER_OBJECT      pDriverObject,
   IN  PUNICODE_STRING     pRegistryPath
   );
VOID NtProtoInitializeGlobal(
   IN  PDRIVER_OBJECT      pDriverObject
   );
VOID NtProtoCleanupGlobal();
VOID NtProtoUnload(
   IN  PDRIVER_OBJECT              pDriverObject
   );
NTSTATUS NtProtoDispatch(
   IN  PDEVICE_OBJECT  pDeviceObject,
   IN  PIRP            pIrp
   );
VOID NtProtoShutDown();




#endif 