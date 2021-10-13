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
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

BOOL NormalPerfTests(UsbClientDrv *pDriverPtr, UCHAR uEPType, int iID, int iPhy, BOOL fOUT, int iTiming, int iBlocking);


BOOL BulkIntr_Perf(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpOUTPipePoint, LPCUSB_ENDPOINT lpINPipePoint, UCHAR uType, 
    DWORD dwBlockSize, USHORT usRounds, int iPhy, PONE_THROUGHPUT pTP, BOOL fOUT, int iTiming, int iBlocking);

BOOL Isoch_Perf(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpOUTPipePoint,  LPCUSB_ENDPOINT lpINPipePoint, 
DWORD dwBlockSize, USHORT usRounds, BOOL bPhy, PONE_THROUGHPUT pTP, BOOL fOUT);

VOID TimeCounting(PTIME_REC pTimeRec, BOOL bStart);
double CalcAvgTime(TIME_REC TimeRec, USHORT usRounds);
double CalcAvgThroughput(TIME_REC TimeRec, USHORT usRounds, DWORD dwBlockSize);
DWORD WINAPI DummyNotify(LPVOID lpvNotifyParameter);
DWORD WINAPI ThroughputNotify(LPVOID lpvNotifyParameter);
VOID PrintOutPerfResults(TP_FIXEDPKSIZE PerfResults);

DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter);

#endif
