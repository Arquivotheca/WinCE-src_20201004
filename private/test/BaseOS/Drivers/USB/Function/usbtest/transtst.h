//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//******************************************************************************
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

#ifndef __TRANSTST_H_
#define __TRANSTST_H_
#include "UsbServ.h" 
#include "Usbtest.h"
#include "syncobj.h"
#include "MThread.h"


class TransferThread : public MiniThread,public UsbPipeService {

public:
    TransferThread(UsbClientDrv * pDriverPtr,
        LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor,
        LPVOID lpBuffer,DWORD dwBufferSize,BOOL Async=FALSE) :
            MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
            UsbPipeService(pDriverPtr->lpUsbFuncs,pDriverPtr->GetUsbHandle(),
                lpEndPointDescriptor,Async),
            lpDataBuffer(lpBuffer),
            dwDataSize(dwBufferSize),
            pDriver(pDriverPtr),
            aTransfer(0,pDriverPtr->lpUsbFuncs),
            bTransfer(0,pDriverPtr->lpUsbFuncs)
    {
        errorCode=0; aSyncMode=Async;
    };
    virtual ~TransferThread();
    virtual BOOL SubmitRequest(LPVOID lpBuffer,DWORD dwSize);
    virtual DWORD GetErrorCode() {return  errorCode!=0?errorCode:GetPipeLastError(); };
    virtual DWORD ActionCompleteNotify(UsbTransfer *pTransfer);
    virtual BOOL EmptyBuffer(DWORD dwTicks);

    // test item
    virtual BOOL TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);
    virtual BOOL CheckPreTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);
    virtual BOOL CheckPostTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);

    CEvent cEvent;
    DWORD errorCode;
    static USB_DEVICE_REQUEST InRequest;
    static USB_DEVICE_REQUEST OutRequest;
       BOOL SetEndpoint(BOOL bStalled);
 
private:
    virtual DWORD ThreadRun();

    LPVOID lpDataBuffer;
    DWORD dwDataSize;
    UsbClientDrv * pDriver;
    BOOL aSyncMode;
    USB_TRANSFER hTransfer;
    UsbTransfer aTransfer;
    UsbTransfer bTransfer;
};

class AbortTransferThread : public TransferThread {
public:
    AbortTransferThread(UsbClientDrv * pDriverPtr,LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor,
        LPVOID lpBuffer,DWORD dwBufferSize,BOOL Async=FALSE) :
        TransferThread(pDriverPtr,lpEndPointDescriptor,lpBuffer,dwBufferSize,Async)
    {
            aSyncMode=Async;
    }
    virtual BOOL TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);
    virtual BOOL CheckPostTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);
private:
    BOOL aSyncMode;
};

class CloseTransferThread : public TransferThread {
public:
    CloseTransferThread(UsbClientDrv * pDriverPtr,LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor,
        LPVOID lpBuffer,DWORD dwBufferSize) :
        TransferThread(pDriverPtr,lpEndPointDescriptor,lpBuffer,dwBufferSize,TRUE)
    {
    }
    virtual BOOL TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength=0,PDWORD pdwLegnthBlock=NULL);
};


BOOL SinglePairTrans(UsbClientDrv *pDriverPtr, DWORD dwTransType, BOOL fAsync, int iID);
BOOL TransNormalForCommand(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pInEP, LPCUSB_ENDPOINT pOutEP, BOOL ASync,DWORD dwObjectNumber, int iID);

BOOL SinglePairSpecialTrans(UsbClientDrv *pDriverPtr, int iID, int iCaseID);
BOOL TransCase1TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID);
BOOL TransCase2TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID);
BOOL TransCase3TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID);
BOOL TransCase4TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID);


BOOL 
IssueSyncedTransfer(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, USB_PIPE hPipe,  USHORT usSize, PBYTE pBuf, BOOL fOUT);
BOOL 
TransWithDeviceSideStalls(UsbClientDrv *pDriverPtr, int iID,  int iCaseID);
BOOL 
ShortTransferExercise(UsbClientDrv *pDriverPtr, int iID, int iCaseID);
BOOL 
IssueSyncedShortTransfer(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, USB_PIPE hPipe,  
                      USHORT usIssueSize, USHORT usExpRetSize, PBYTE pBuf, BOOL fOUT);

#endif
