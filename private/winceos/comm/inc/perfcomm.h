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

    perfcomm.h

Abstract:

    CEPerf header file defining symbols used in comm OS tree.
    Performance Measurement Definitions.


Environment:

    Kernel mode driver

--*/

#ifndef __PERFCOMM_H__
#define __PERFCOMM_H__

#ifdef __cplusplus 
extern "C" { 
#endif 

#define CEPERF_UNSAFE

#define PERFCOMM_DURATION(szName)                                               \
    { INVALID_HANDLE_VALUE, CEPERF_TYPE_DURATION, (szName),                     \
      CEPERF_RECORD_ABSOLUTE_CPUPERFCTR                                         \
      | CEPERF_DURATION_RECORD_MIN }                                            \

#define PERFCOMM_STATISTIC(szName)                                              \
    { INVALID_HANDLE_VALUE, CEPERF_TYPE_LOCALSTATISTIC, (szName), 0 }




#if defined(PERFCOMM_WINSOCK) // -----------------------------------------------
// WINSOCK definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\WINSOCK")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Durations */
    WINSOCK_DurSend,
    WINSOCK_DurSendTo,
    WINSOCK_DurRecv,
    WINSOCK_DurRecvFrom,
    /* Statistics */
    WINSOCK_StatSendTotalCalls,
    WINSOCK_StatSendTotalBytes,
    WINSOCK_StatRecvTotalCalls,
    WINSOCK_StatRecvTotalBytes,

    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST                                                   \
    /* Durations */                                                     \
    PERFCOMM_DURATION(TEXT("WINSOCK_DurSend")),                         \
    PERFCOMM_DURATION(TEXT("WINSOCK_DurSendTo")),                       \
    PERFCOMM_DURATION(TEXT("WINSOCK_DurRecv")),                         \
    PERFCOMM_DURATION(TEXT("WINSOCK_DurRecvFrom")),                     \
    /* Statistics */                                                    \
    PERFCOMM_STATISTIC(TEXT("WINSOCK_StatSendTotalCalls")),             \
    PERFCOMM_STATISTIC(TEXT("WINSOCK_StatSendTotalBytes")),             \
    PERFCOMM_STATISTIC(TEXT("WINSOCK_StatRecvTotalCalls")),             \
    PERFCOMM_STATISTIC(TEXT("WINSOCK_StatRecvTotalBytes"))


#elif defined(PERFCOMM_NATIVEWIFI) // --------------------------------------------

//
//  NATIVEWIFI definitions.
//

#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\NATIVEWIFI")


//
//  Must be in the same order as in PERFCOMM_LIST!
//

enum 
{
    NATIVEWIFI_PerfCommCost,
    NATIVEWIFI_PerfCommCaller,
    NATIVEWIFI_PerfCommCalled,
    NATIVEWIFI_PtReceivePacket,
    NATIVEWIFI_FlushIndicatePacket,
    NATIVEWIFI_PtReceivePacketInternal,
    NATIVEWIFI_Dot11ReceiveHandle,    
    NATIVEWIFI_DurationInNdisIndicateReceivePacket,
    NATIVEWIFI_MPSendPackets,
    NATIVEWIFI_DurationInDot11SendHandle,
    NATIVEWIFI_Dot11FlushIntermediateSendQueue,
    NATIVEWIFI_DurationInNdisSend,    
    NATIVEWIFI_StatTxFromProtocol,
    NATIVEWIFI_StatTxToMiniport,
    NATIVEWIFI_StatRxFromMiniport,
    NATIVEWIFI_StatRxToProtocol,    
    
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;


//
//  Must be in the same order as in PERFCOMM_ID!
//

#define PERFCOMM_LIST                                             \
    PERFCOMM_DURATION(TEXT("PerfCommCost")),                      \
    PERFCOMM_DURATION(TEXT("PerfCommCaller")),                    \
    PERFCOMM_DURATION(TEXT("PerfCommCalled")),                    \
    PERFCOMM_DURATION(TEXT("PtReceivePacket()")),                 \
    PERFCOMM_DURATION(TEXT("FlushIndicatePacket()")),             \
    PERFCOMM_DURATION(TEXT("PtReceivePacketInternal()")),         \
    PERFCOMM_DURATION(TEXT("DurInDot11ReceiveHandle()")),         \
    PERFCOMM_DURATION(TEXT("DurInNdisMIndRcvPkt()")),             \
    PERFCOMM_DURATION(TEXT("MPSendPackets()")),                   \
    PERFCOMM_DURATION(TEXT("DurInDot11SendHandle")),              \
    PERFCOMM_DURATION(TEXT("Dot11FlushIntermediateSendQ..()")),   \
    PERFCOMM_DURATION(TEXT("DurInNdisSend")),                     \
    PERFCOMM_STATISTIC(TEXT("NumberOfTxPacketsFromProtocol")),    \
    PERFCOMM_STATISTIC(TEXT("NumberOfTxPacketsToMiniport")),      \
    PERFCOMM_STATISTIC(TEXT("NumberOfRxPacketsFromMiniport")),    \
    PERFCOMM_STATISTIC(TEXT("NumberOfRxPacketsToProtocol"))
    
//
//  Callstack:
//  PtFlushIndicatePacket() +--> PtReceivePacketInternal()
//                          |
//                          +--> Dot11ReceiveHandle()
//                          |
//                          +--> DurInNdisMIndRcvPkt().
//
//  Reading the perf counters:
//  
//  FlushIndicatePacket()  includes PtReceivePacketInternal() + DurationInDot11ReceiveHandle().
//
//  Hence, total number of instructions executed in NWIFI in PtReceiveComplete is:
//
//  NWIFI_PtReceiveComplete() - NWIFI_DurInNdisMIndRcvPkt()
//  
    

#elif defined(PERFCOMM_AFD) // -------------------------------------------------
// AFD definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\AFD")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Durations */
    AFD_DurSend,
    AFD_DurRecv,
    /* Statistics */
    AFD_StatSendTotalCalls,
    AFD_StatSendTotalBytes,
    AFD_StatDgSendTotalCalls,
    AFD_StatDgSendTotalBytes,
    AFD_StatRecvTotalCalls,
    AFD_StatRecvTotalBytes,
    AFD_StatDgRecvTotalCalls,
    AFD_StatDgRecvTotalBytes,
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST                                       \
    /* Durations */                                         \
    PERFCOMM_DURATION(L"AFD_DurSend"),                      \
    PERFCOMM_DURATION(L"AFD_DurRecv"),                      \
    /* Statistics */                                        \
    PERFCOMM_STATISTIC(L"AFD_StatSendTotalCalls"),          \
    PERFCOMM_STATISTIC(L"AFD_StatSendTotalBytes"),          \
    PERFCOMM_STATISTIC(L"AFD_StatRecvTotalCalls"),          \
    PERFCOMM_STATISTIC(L"AFD_StatRecvTotalBytes"),          \
    PERFCOMM_STATISTIC(L"AFD_StatDgSendTotalCalls"),        \
    PERFCOMM_STATISTIC(L"AFD_StatDgSendTotalBytes"),        \
    PERFCOMM_STATISTIC(L"AFD_StatDgRecvTotalCalls"),        \
    PERFCOMM_STATISTIC(L"AFD_StatDgRecvTotalBytes")

#elif defined(PERFCOMM_CXPORT) // -------------------------------------------------
// AFD definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\CXPORT")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Durations */
    CXPORT_DurStartTimer,
    CXPORT_DurStopTimer,
    CXPORT_DurSchedEvent,
    CXPORT_DurRun,
    CXPORT_DurWorker,
    CXPORT_DurAlloc,
    CXPORT_DurFree,
    /* Statistics */
    CXPORT_StatTotalEvents,
    CXPORT_StatTotalTimers,
    CXPORT_StatTotalAllocBytes,
    CXPORT_StatTotalFreeBytes,
    CXPORT_StatTotalGCFreeBytes,
    CXPORT_StatTotalGCRuns,
    CXPORT_StatTotalAllocCacheHits,
    CXPORT_StatTotalFreeCacheHits,
    CXPORT_StatTotalAllocListHits,
    CXPORT_StatTotalFreeListHits,
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST                                          \
    /* Durations */                                            \
    PERFCOMM_DURATION(L"CXPORT_DurStartTimer"),                \
    PERFCOMM_DURATION(L"CXPORT_DurStopTimer"),                 \
    PERFCOMM_DURATION(L"CXPORT_DurSchedEvent"),                \
    PERFCOMM_DURATION(L"CXPORT_DurRun"),                       \
    PERFCOMM_DURATION(L"CXPORT_DurWorker"),                    \
    PERFCOMM_DURATION(L"CXPORT_DurAlloc"),                     \
    PERFCOMM_DURATION(L"CXPORT_DurFree"),                      \
    /* Statistics */                                           \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalEvents"),             \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalTimers"),             \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalAllocBytes"),         \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalFreeBytes"),          \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalGCFreeBytes"),        \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalGCRuns"),             \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalAllocCacheHits"),     \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalFreeCacheHits"),      \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalAllocListHits"),      \
    PERFCOMM_STATISTIC(L"CXPORT_StatTotalFreeListHits")

#elif defined(PERFCOMM_TCPSTK) // ----------------------------------------------
// TCPSTK definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\TCPSTK")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST

#elif defined(PERFCOMM_NDIS) // ------------------------------------------------
// NDIS definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\NDIS")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST

#elif defined(PERFCOMM_MBRIDGE) // ---------------------------------------------
// MBRIDGE definitions
#define PERFCOMM_SESSION_NAME  TEXT("SYSTEM\\COMM\\MBRIDGE")

// Must be in the same order as in PERFCOMM_LIST!
enum {
    /* Durations */
    MBRIDGE_DurBrdgFwdReceive,
    MBRIDGE_DurBrdgProtReceiveComplete,
    MBRIDGE_DurCEIndicateAllQueuedPackets,
    MBRIDGE_DurBrdgFwdSendPacket,
    MBRIDGE_DurNdisSendPackets,
    
    /* Always last */
    PERFCOMM_NUM_IDS
} PERFCOMM_ID;

// Must be in the same order as in PERFCOMM_ID!
#define PERFCOMM_LIST                                                   \
    /* Durations */                                                     \
    PERFCOMM_DURATION(TEXT("MBRIDGE_DurBrdgFwdReceive")),               \
    PERFCOMM_DURATION(TEXT("MBRIDGE_DurBrdgProtReceiveComplete")),      \
    PERFCOMM_DURATION(TEXT("MBRIDGE_DurCEIndicateAllQueuedPackets")),   \
    PERFCOMM_DURATION(TEXT("MBRIDGE_DurBrdgFwdSendPacket")),            \
    PERFCOMM_DURATION(TEXT("MBRIDGE_DurNdisSendPackets"))

#endif // ----------------------------------------------------------------------

#if defined(CEPERF_ENABLE) && defined(PERFCOMM_LIST) // -------------------------------------

#include <ceperf.h>

extern CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems[PERFCOMM_NUM_IDS];

extern VOID PerfCommInit();
extern VOID PerfCommDeInit();

#define PerfCommBegin(id)           CePerfBeginDuration(g_rgPerfItems[id].hTrackedItem)
#define PerfCommEnd(id)             CePerfEndDuration(g_rgPerfItems[id].hTrackedItem, NULL)
#define PerfCommEndError(id, error) CePerfEndDurationWithInformation(g_rgPerfItems[id].hTrackedItem, (error), NULL, 0, NULL)
#define PerfCommIncrStat(id)        CePerfIncrementStatistic(g_rgPerfItems[id].hTrackedItem, NULL)
#define PerfCommAddStat(id, incr)   CePerfAddStatistic(g_rgPerfItems[id].hTrackedItem, incr, NULL)

#else  // defined(CEPERF_ENABLE) && defined(PERFCOMM_LIST) ----------------------------------

#define PerfCommInit()                                   ((VOID)0)
#define PerfCommDeInit()                                 ((VOID)0)
#define PerfCommBegin(id)                                ((VOID)0)
#define PerfCommEnd(id)                                  ((VOID)0)
#define PerfCommEndError(id, error)                      ((VOID)0)
#define PerfCommIncrStat(id)                             ((VOID)0)
#define PerfCommAddStat(id, incr)                        ((VOID)0)

#endif // defined(CEPERF_ENABLE) && defined(PERFCOMM_LIST) ----------------------------------

#ifdef __cplusplus 
}
#endif 

#endif // __PERFCOMM_H__

