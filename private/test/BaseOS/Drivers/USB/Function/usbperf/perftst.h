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
//******************************************************************************

/*++

Module Name:  
    TransTst.h

--*/

#ifndef __PERFTST_H_
#define __PERFTST_H_
//#include "Usbperf.h"
#include "usbclass.h"

typedef struct  _TIME_REC{
    LARGE_INTEGER   startTime;
    LARGE_INTEGER   endTime;
    LARGE_INTEGER   perfFreq;
}TIME_REC, *PTIME_REC;

#define BEGIN_TIMING    TRUE
#define END_TIMING      FALSE

// define for transfer callback

typedef struct _PerfTransStatus {
    USB_TRANSFER hTransfer;
    BOOL fDone;
    PTIME_REC    pTimeRec;
    float flCPUPercentage;
    float flMemPercentage;
} PerfTransStatus,*PPerfTransStatus;

typedef struct _ONE_THROUGHPUT{
    DWORD   dwBlockSize;
    double    dbThroughput;
    float       flCPUUsage;
    float       flMemUsage;
}ONE_THROUGHPUT, *PONE_THROUGHPUT;

typedef struct _TP_FIXEDPKSIZE{
    UCHAR  uNumofSizes;
    USHORT  usPacketSize;
    ONE_THROUGHPUT  UnitTPs[64];
}TP_FIXEDPKSIZE, *PTP_FIXEDPKSIZE;

DWORD NormalPerfTests(UsbClientDrv *pDriverPtr, 
                UCHAR uEPType, 
                int iID, 
                int iPhy, 
                BOOL fOUT, 
                int iTiming,    // overloaded w/ thread priority
                int iBlocking,  // overloaded w/ Qparam and block size
                BOOL in_fHighSpeed,
                LPCTSTR in_szTestDescription);


DWORD BulkIntr_Perf(UsbClientDrv *pDriverPtr, 
              LPCUSB_ENDPOINT lpOUTPipePoint, 
              LPCUSB_ENDPOINT lpINPipePoint, 
              UCHAR uType,
              DWORD dwBlockSize, 
              UINT  uiRounds, 
              int iPhy, 
              PONE_THROUGHPUT pTP, 
              BOOL fOUT, 
              int iTiming, 
              int iBlocking, 
              UINT uiCallPrio,
              UINT uiFullQue,
              BOOL in_fHighSpeed,
              LPCTSTR in_szTestDescription);

DWORD Isoch_Perf(UsbClientDrv *pDriverPtr, 
           LPCUSB_ENDPOINT lpOUTPipePoint,  
           LPCUSB_ENDPOINT lpINPipePoint, 
           DWORD dwBlockSize, 
           UINT  uiRounds, 
           BOOL bPhy, 
           PONE_THROUGHPUT pTP, 
           BOOL fOUT,
           int iTiming, 
           UINT uiQparam, 
           UINT uiCallPrio,
           UINT uiFullQue,
           BOOL in_fHighSpeed,
           LPCTSTR in_szTestDescription);

VOID TimeCounting(PTIME_REC pTimeRec, BOOL bStart);
#define StartTimeCounting(_TIME_RECORD_PTR_) TimeCounting(_TIME_RECORD_PTR_,TRUE)
#define StopTimeCounting(_TIME_RECORD_PTR_) TimeCounting(_TIME_RECORD_PTR_,FALSE)

double CalcAvgTime(const TIME_REC& TimeRec, UINT usRounds);
double CalcAvgThroughput(const TIME_REC& TimeRec, UINT usRounds, DWORD dwBlockSize);
DWORD WINAPI DummyNotify(LPVOID lpvNotifyParameter);
DWORD WINAPI ThroughputNotify(LPVOID lpvNotifyParameter);
VOID PrintOutPerfResults(const TP_FIXEDPKSIZE& PerfResults,LPCTSTR szTestDescription);

DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter);

#endif
