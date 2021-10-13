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
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  

PipeTest.cpp

Abstract:  
    USB Driver Pipe Test .

Functions:

Notes: 

--*/


#define __THIS_FILE__   TEXT("PipeTest.cpp")
#include <windows.h>
#include <ceddk.h>
#include "PipeTest.h"
#include "usbstress.h"

#define MAX_TRANS_SIZE 65536

USB_DEVICE_REQUEST InRequest = { USB_REQUEST_DEVICE_TO_HOST | USB_REQUEST_VENDOR | USB_REQUEST_FOR_ENDPOINT, 0xff, 0, 0, 0
};
USB_DEVICE_REQUEST OutRequest = { USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_VENDOR | USB_REQUEST_FOR_ENDPOINT, 0xff, 0, 0, 0
};

#define USB_GETSTATUS_DEVICE_STALL 1


BOOL CommandSyncPipe::SetEndpoint(BOOL bStalled)
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +CommandSyncPipe::SetEndpoint\n"), szCEDeviceName));
    BYTE bEndpointAddr = GetPipeDescriptorPtr()->bEndpointAddress;
    USB_TRANSFER hTransfer;
    if(bStalled)
        hTransfer = pDriverPtr->SetFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    else
        hTransfer = pDriverPtr->ClearFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    if(hTransfer == NULL) {
        ResetPipe();
        return FALSE;
    };
    UsbTransfer featureTransfer(hTransfer, GetUsbFuncPtr());
    if(featureTransfer.IsTransferComplete()
       && featureTransfer.IsCompleteNoError()) {
        WORD wStatus = 0;
        hTransfer = pDriverPtr->GetStatus(NULL, NULL, USB_SEND_TO_ENDPOINT, bEndpointAddr, &wStatus);
        if(hTransfer == NULL) {
            ResetPipe();
            return FALSE;
        };
        UsbTransfer statusTransfer(hTransfer, GetUsbFuncPtr());
        if(statusTransfer.IsTransferComplete()
           && statusTransfer.IsCompleteNoError()) {
            if(bStalled == ((wStatus & USB_GETSTATUS_DEVICE_STALL) != 0))
                return TRUE;
        } else
            ResetPipe();
    } else
        ResetPipe();
    ASSERT(FALSE);
    return FALSE;
}


BOOL CommandSyncPipe::IssueCommand(PUSB_DEVICE_REQUEST pControlHead, LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr)
{
    USB_TRANSFER hTransfer = 0;
    UsbTransfer aTransfer(hTransfer, GetUsbFuncPtr());
    DEBUGMSG(DBG_FUNC, (TEXT("%s +CommandSyncPipe::IssueCommand\n"), szCEDeviceName));
    DEBUGMSG(DBG_CONTROL, (TEXT("%s CommandSyncPipe::IssueCommand( flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, GetFlags(), lpBuffer, dwSize));
    pControlHead->wLength = (WORD) dwSize;
    hTransfer = IssueControlTransfer(&aTransfer, &pControlHead, dwSize, lpBuffer, dwPhysicalAddr);
    DEBUGMSG(DBG_DETAIL, (TEXT("%s CommandSyncPipe::IssueCommand, hTransfer %lx\n"), szCEDeviceName, hTransfer));

    if(!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Submit Transfer"), szCEDeviceName);
        return FALSE;
    }

    BOOL bReturn = aTransfer.WaitForTransferComplete();
    ASSERT(bReturn);

    if(!aTransfer.IsCompleteNoError()) {
        // somthing wrong
        DWORD dwError;
        DWORD dwByteTransfer;
        DWORD rtError;
        rtError = (DWORD) aTransfer.GetTransferStatus(&dwByteTransfer, &dwError);
        DEBUGMSG(DBG_DETAIL, (TEXT("%s CommandSyncPipe::IssueCommand Fail at Transfer.,Error(%ld),TransferedByte(%ld),Return(%ld)\n"), szCEDeviceName, dwError, dwByteTransfer, rtError));
        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,Error(%ld),TransferedByte(%ld),Return(%ld)"), szCEDeviceName, dwError, dwByteTransfer, rtError);
        bReturn = FALSE;
    };
    DEBUGMSG(DBG_FUNC, (TEXT("%s -CommandSyncPipe::IssueCommand\n"), szCEDeviceName));
    return bReturn;
}


BOOL SyncPipeThread::SubmitRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr)
{
    USB_TRANSFER hTransfer = 0;
    UsbTransfer aTransfer(hTransfer, GetUsbFuncPtr());
    if(m_bUseLengthArray == TRUE && m_pusTransLens != NULL) {
        SetFlags(GetFlags() | USB_SHORT_TRANSFER_OK);
    }
    DEBUGMSG(DBG_FUNC, (TEXT("%s +Sync::SubmitRequest\n"), szCEDeviceName));
    switch (GetEndPointAttribute()) {
        case USB_ENDPOINT_TYPE_BULK:
            DEBUGMSG(DBG_BULK, (TEXT("%s Sync::SubmitRequest(Bulk)( %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
            hTransfer = IssueBulkTransfer(&aTransfer, dwSize, lpBuffer, dwPhysicalAddr);
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            DEBUGMSG(DBG_INTERRUPT, (TEXT("%s Sync::SubmitRequest(Intr)( %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
            hTransfer = IssueInterruptTransfer(&aTransfer, dwSize, lpBuffer, dwPhysicalAddr);
            break;
        case USB_ENDPOINT_TYPE_CONTROL:
            {
                DEBUGMSG(DBG_CONTROL, (TEXT("%s Sync::SubmitRequest(Ctrl)( %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
                USB_DEVICE_REQUEST ControlHead = (isInPipe()? InRequest : OutRequest);
                ControlHead.wLength = (WORD) dwSize;
                hTransfer = IssueControlTransfer(&aTransfer, &ControlHead, dwSize, lpBuffer, dwPhysicalAddr);
            };
            break;
        default:
            ASSERT(FALSE);
    };
    DEBUGMSG(DBG_DETAIL, (TEXT("%s Sync::SubmitRequest %s, hTransfer %lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), hTransfer));
    if(!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Submit Transfer"), szCEDeviceName);
        return FALSE;
    };
    BOOL bReturn = aTransfer.WaitForTransferComplete();
    ASSERT(bReturn);

    DEBUGMSG(DBG_DETAIL, (TEXT("%s Sync::SubmitRequest %s\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write")));
    if(aTransfer.IsCompleteNoError()) {
        bReturn = TRUE;
    } else {        // somthing wrong
        DWORD dwError;
        DWORD dwByteTransfer;
        DWORD rtError;
        rtError = (DWORD) aTransfer.GetTransferStatus(&dwByteTransfer, &dwError);
        DEBUGMSG(DBG_DETAIL, (TEXT("%s Sync::SubmitRequest Fail at Transfer.,Error(%ld),TransferedByte(%ld),Return(%ld)\n"), szCEDeviceName, dwError, dwByteTransfer, rtError));
        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,Error(%ld),TransferedByte(%ld),Return(%ld)"), szCEDeviceName, dwError, dwByteTransfer, rtError);
        bReturn = FALSE;
    };
    DEBUGMSG(DBG_FUNC, (TEXT("%s -Sync::SubmitRequest\n"), szCEDeviceName));
    return bReturn;
}

#define SYNC_MODE_DELAY 20
#define ASYNC_MODE_DELAY 64
BOOL SyncPipeThread::SubmitIsochRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, PDWORD pBlockLength)
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +Sync::SubmitIsochRequest\n"), szCEDeviceName));
    BOOL bReturn = TRUE;
    if(m_pSync)
        SetFlags(GetFlags());
    PDWORD pdwErrors;
    WORD oneBlockLength = GetMaxPacketSize();
    DWORD dwStartFrame = 0;
    ASSERT(oneBlockLength);

    oneBlockLength = min(oneBlockLength, MAX_ISOCH_FRAME_LENGTH);
    DWORD numBlock = (dwSize + oneBlockLength - 1) / oneBlockLength;

    pdwErrors = new DWORD[numBlock];
    DWORD curDataSize = dwSize;
    for(DWORD blockNumber = 0; blockNumber < numBlock; blockNumber++)
        if(curDataSize > oneBlockLength)
            pBlockLength[blockNumber] = oneBlockLength;
        else {
            pBlockLength[blockNumber] = curDataSize;
            blockNumber++;
            break;
        };
    DEBUGMSG(DBG_ISOCH, (TEXT("%s IssueIsochTransfer %s flag(%lx),addr@%lx, blockNumber=%lx,numBlock=%lx,firstBlockLength=%lx,startFrame=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, blockNumber, numBlock, pBlockLength[0], m_dwCurrentSyncFrame));

    UsbTransfer hTransfer(0, GetUsbFuncPtr());
    if(m_pSync && !pDriverPtr->GetFrameNumber(&dwStartFrame)) {    // try again
        if(!pDriverPtr->GetFrameNumber(&dwStartFrame)) {
            g_pKato->Log(LOG_COMMENT, TEXT("%s Warning Could not get Frame at Isoch Transfer"), szCEDeviceName);
            SetFlags(GetFlags() | USB_START_ISOCH_ASAP);
        }
    }
    if(m_bUseLengthArray == TRUE && m_pusTransLens != NULL) {
        SetFlags(GetFlags() | USB_SHORT_TRANSFER_OK);
    }
    dwStartFrame += SYNC_MODE_DELAY;
    IssueIsochTransfer(&hTransfer, dwStartFrame, blockNumber, pBlockLength, lpBuffer, dwPhysicalAddr);
    bReturn = hTransfer.WaitForTransferComplete();
    ASSERT(bReturn);
    m_dwCurrentSyncFrame = dwStartFrame + blockNumber;
    if(!hTransfer.IsTransferComplete()) {    // ERROR
        // BOOL rtError = hTransfer.GetIsochResults(blockNumber, pBlockLength, pdwErrors);
        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Isoch Transfer"), szCEDeviceName);
        DEBUGMSG(DBG_ERR, (TEXT("%s Fail at Isoch Transfer\n"), szCEDeviceName));
        errorCode = ERROR_USB_PIPE_TRANSFER;
        bReturn = FALSE;
        hTransfer.AbortTransfer(0);
    }
    delete[]pdwErrors;
    DEBUGMSG(DBG_FUNC, (TEXT("%s -Sync::SubmitIsochRequest\n"), szCEDeviceName));
    return bReturn;
};

#define MAX_BLOCKS 0x1000
#define ISOCH_TRANS_DELAY 64
DWORD SyncPipeThread::ThreadRun()
{

    if(m_bShortStress == TRUE) {
        RunShortStress();
        return 0;
    }

    if(m_bUseLengthArray == TRUE && m_pusTransLens != NULL) {
        RunwithLengthsArray();
        return 0;
    }
    PDWORD pdwLengths = NULL;
    DEBUGMSG(DBG_FUNC, (TEXT("%s +SyncPipeThread::ThreadRun\n"), szCEDeviceName));
    errorCode = GetPipeLastError();
    if(errorCode == 0) {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s SyncPipeThread::ThreadRun GetEndPointAttr %x\n"), szCEDeviceName, GetEndPointAttribute()));
        switch (GetEndPointAttribute()) {
            case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                {
                    //   Allocate arrays of buffer lengths
                    pdwLengths = new DWORD[MAX_ISOCH_BUFFERS];

                    DWORD curDataSize = dwDataSize;
                    DWORD oneBlockLength = min(m_dwBlockSize,
                                   LIMITED_UHCI_BLOCK * (DWORD) GetMaxPacketSize());
                    m_dwCurrentSyncFrame = pDriverPtr->GetSyncFramNumber();
                    m_dwCurrentSyncFrame += (isInPipe()? TRANSFER_IN_DELAY : TRANSFER_OUT_DELAY);
                    do {
                        DWORD dLength = min(curDataSize, oneBlockLength);
                        if(m_pSync && isInPipe())
                            m_pSync->Lock();

                        if(!SubmitIsochRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, dLength, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize, pdwLengths)) {

                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else
                            curDataSize -= dLength;
                        if(m_pSync && isOutPipe())
                            m_pSync->Unlock();

                    }
                    while(!IsTerminated() && curDataSize);
                    if(curDataSize) {
                        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at IsochTransfer,InCompleted, Left(%ld)Byte"), szCEDeviceName, curDataSize);
                        errorCode = ERROR_USB_PIPE_TRANSFER;
                    };
                };
                break;
            case USB_ENDPOINT_TYPE_BULK:
            case USB_ENDPOINT_TYPE_INTERRUPT:
            case USB_ENDPOINT_TYPE_CONTROL:
                {
                    DWORD curDataSize = dwDataSize;
                    DWORD dwFactor = LIMITED_UHCI_BLOCK;
                    DWORD dwcount = 0;
                    if(GetEndPointAttribute() != USB_ENDPOINT_TYPE_BULK)
                        dwFactor = 1;

                    DEBUGMSG(DBG_FUNC, (TEXT("%s Max Packet Size %u\n"), szCEDeviceName, GetMaxPacketSize()));
                    do {
                        DWORD dLength = min(m_dwBlockSize, curDataSize);
                        dLength = min(dLength, dwFactor * GetMaxPacketSize());
                        dwcount++;

                        if(!SubmitRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, dLength, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize)) {
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else
                            curDataSize -= dLength;
                    }
                    while(!IsTerminated() && curDataSize);
                    if(curDataSize) {
                        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,InCompleted, Left(%ld)Byte"), szCEDeviceName, curDataSize);
                        errorCode = ERROR_USB_PIPE_TRANSFER;
                    };
                };
                break;
            default:
                ASSERT(FALSE);
        };
    };
    //   Free arrays of buffer lengths and errors
    if(GetEndPointAttribute() == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if(pdwLengths != NULL)
            delete[]pdwLengths;
    }

    DEBUGMSG(DBG_FUNC, (TEXT("%s -SyncPipeThread::ThreadRun\n"), szCEDeviceName));
    return 0;
};

VOID SyncPipeThread::RunShortStress()
{
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d"), iDevID);
    DWORD dwMaxPacketSize = GetMaxPacketSize();

    DEBUGMSG(DBG_FUNC, (TEXT("%s +SyncPipeThread::RunShortStress\n"), szCEDeviceName));
    errorCode = GetPipeLastError();
    if(errorCode == 0) {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s SyncPipeThread::RunShortStress GetEndPointAttr %x\n"), szCEDeviceName, GetEndPointAttribute()));
        switch (GetEndPointAttribute()) {
            case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                {
                    DWORD pdwLengths[1];
                    m_dwCurrentSyncFrame = pDriverPtr->GetSyncFramNumber();
                    m_dwCurrentSyncFrame += (isInPipe()? TRANSFER_IN_DELAY : TRANSFER_OUT_DELAY);
                    DWORD dwCurrPos = 0;
                    for(DWORD dwLen = 1; dwLen < dwMaxPacketSize; dwLen++) {
                        if(m_pSync && isInPipe())
                            m_pSync->Lock();
                        if(!SubmitIsochRequest((LPBYTE) lpDataBuffer + dwCurrPos, dwLen, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwCurrPos, pdwLengths)) {

                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else
                            dwCurrPos += dwLen;
                        if(m_pSync && isOutPipe())
                            m_pSync->Unlock();
                    }
                }
                break;
            case USB_ENDPOINT_TYPE_BULK:
            case USB_ENDPOINT_TYPE_INTERRUPT:
            case USB_ENDPOINT_TYPE_CONTROL:
                {
                    DWORD dwCurrPos = 0;
                    for(DWORD dwLen = 1; dwLen < dwMaxPacketSize; dwLen++) {
                        if(!SubmitRequest((LPBYTE) lpDataBuffer + dwCurrPos, dwLen, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwCurrPos)) {
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else
                            dwCurrPos += dwLen;
                    }
                }
                break;
            default:
                ASSERT(FALSE);
        };
    };
    //   Free arrays of buffer lengths and errors

    DEBUGMSG(DBG_FUNC, (TEXT("%s -SyncPipeThread::RunShortStress\n"), szCEDeviceName));
};


BOOL SyncPipeThread::ValidateLengthsArray()
{
    DWORD dwTotal = 0;
    int iLenIndex = 0;
    while(dwTotal < dwDataSize) {
        DWORD dwPlannedLen = 0xFFFF;
        __try {
            dwPlannedLen = (DWORD) m_pusTransLens[iLenIndex];
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(DBG_ERR, (TEXT("%s access lengths array at offset %d cause AV!"), szCEDeviceName, iLenIndex));
            break;
        }

        dwTotal += dwPlannedLen;
        iLenIndex++;
    }

    if(dwTotal != dwDataSize) {
        DEBUGMSG(DBG_ERR, (TEXT("%s Lengths Array is not valid!"), szCEDeviceName));
        return FALSE;
    }

    return TRUE;
}

void SyncPipeThread::RunwithLengthsArray()
{

    if(ValidateLengthsArray() == FALSE) {
        DEBUGMSG(DBG_ERR, (TEXT("%s lengths array is not valid!TEST QUIT!!!"), szCEDeviceName));
        return;
    }

    DEBUGMSG(DBG_FUNC, (TEXT("%s +SyncPipeThread::RunwithLengthsArray\n"), szCEDeviceName));
    errorCode = GetPipeLastError();
    int iLenIndex = 0;
    if(errorCode == 0) {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s SyncPipeThread::ThreadRun GetEndPointAttr %x\n"), szCEDeviceName, GetEndPointAttribute()));
        switch (GetEndPointAttribute()) {
            case USB_ENDPOINT_TYPE_BULK:
            case USB_ENDPOINT_TYPE_INTERRUPT:
            case USB_ENDPOINT_TYPE_CONTROL:
                {
                    DWORD curDataSize = dwDataSize;
                    DWORD dwFactor = LIMITED_UHCI_BLOCK;
                    DWORD dwcount = 0;
                    if(GetEndPointAttribute() != USB_ENDPOINT_TYPE_BULK)
                        dwFactor = 1;

                    DEBUGMSG(DBG_FUNC, (TEXT("%s Max Packet Size %u\n"), szCEDeviceName, GetMaxPacketSize()));
                    do {
                        DWORD dLength = (DWORD) m_pusTransLens[iLenIndex];
                        if(dLength == 0 && isInPipe()) {
                            dLength = 4;    //we can not issue 0-len IN transfer
                        }
                        dwcount++;

                        if(!SubmitRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, dLength, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize)) {
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else {
                            dLength = (DWORD) m_pusTransLens[iLenIndex];
                            curDataSize -= dLength;
                        }

                        iLenIndex++;
                    }
                    while(!IsTerminated() && curDataSize);
                    if(curDataSize) {
                        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,InCompleted, Left(%ld)Byte"), szCEDeviceName, curDataSize);
                        errorCode = ERROR_USB_PIPE_TRANSFER;
                    };
                };
                break;
            default:
                ASSERT(FALSE);
        };
    };

    DEBUGMSG(DBG_FUNC, (TEXT("%s -SyncPipeThread::ThreadRun\n"), szCEDeviceName));
    return;
};

BOOL SyncPipeThread::SetEndpoint(BOOL bStalled)
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +SyncPipeThread::SetEndpoint\n"), szCEDeviceName));
    BYTE bEndpointAddr = GetPipeDescriptorPtr()->bEndpointAddress;
    USB_TRANSFER hTransfer;
    if(bStalled)
        hTransfer = pDriverPtr->SetFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    else
        hTransfer = pDriverPtr->ClearFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    if(hTransfer == NULL) {
        ResetPipe();
        return FALSE;
    };
    UsbTransfer featureTransfer(hTransfer, GetUsbFuncPtr());
    if(featureTransfer.IsTransferComplete()
       && featureTransfer.IsCompleteNoError()) {
        WORD wStatus = 0;
        hTransfer = pDriverPtr->GetStatus(NULL, NULL, USB_SEND_TO_ENDPOINT, bEndpointAddr, &wStatus);
        if(hTransfer == NULL) {
            ResetPipe();
            return FALSE;
        };
        UsbTransfer statusTransfer(hTransfer, GetUsbFuncPtr());
        if(statusTransfer.IsTransferComplete()
           && statusTransfer.IsCompleteNoError()) {
            if(bStalled == ((wStatus & USB_GETSTATUS_DEVICE_STALL) != 0))
                return TRUE;
        } else
            ResetPipe();
    } else
        ResetPipe();
    ASSERT(FALSE);
    return FALSE;
}

AsyncTransFIFO::AsyncTransFIFO(LPCUSB_FUNCS lpUsbFuncs, DWORD dwFifoSize):
m_lpUsbFuncs(lpUsbFuncs), MiniThread(0, THREAD_PRIORITY_ABOVE_NORMAL, TRUE)
{
    //not used here by now
    wcscpy_s(szCEDeviceName, _countof(szCEDeviceName), _T(""));

    DEBUGMSG(DBG_FUNC, (TEXT("%s +AsyncTransFIFO::AsyncTransFIFO\n"), szCEDeviceName));
    dwOutput = dwInput = dwSignal = 0;
    dwNumTrans = 0;
    dwArraySize = min(dwFifoSize, MAX_FIFO_SIZE);
    cSemaphore = new CSemaphore(dwArraySize - 1, dwArraySize - 1);
    errorCode = 0;
    m_pPipe = NULL;
    DEBUGMSG(DBG_FUNC, (TEXT("%s -AsyncTransFIFO::AsyncTransFIFO\n"), szCEDeviceName));

}

AsyncTransFIFO::~AsyncTransFIFO()
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s AsyncTransFIFO::destroy"), szCEDeviceName));
    ThreadTerminated(INFINITE);
    UsbTransfer *pTransfer;
    while((pTransfer = FirstTransferHandle()) != NULL) {
        DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO Not Empty. Something Wrong\n"), szCEDeviceName));
        if(pTransfer->GetTransferHandle()) {
            DEBUGMSG(DBG_EVENTS, (TEXT("%s AsyncTransFIFO Wait for Signal, Transfer%lx\n"), szCEDeviceName, pTransfer->GetTransferID()));
            BOOL bReturn = pTransfer->WaitForTransferComplete(60000);    // second time out.
            ASSERT(bReturn);
            if(!bReturn) {
                DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO:: Timeout for waiting Transfer(%lx) Complete"), szCEDeviceName, pTransfer->GetTransferID()));
            }
            DEBUGMSG(DBG_EVENTS, (TEXT("%s AsyncTransFIFO Wait return,Transfer %lx \n"), szCEDeviceName, pTransfer->GetTransferID()));
        } else {
            DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO::~AsyncTransFIFO Empty Transfer(%lx) Complete\n"), szCEDeviceName, pTransfer->GetTransferID()));
            ASSERT(FALSE);
        };

        RemoveTransferHandle();
        ReleasePositionToken();
        delete pTransfer;
    };

    DEBUGMSG(DBG_DETAIL, (TEXT("%s AsyncTransFIFO %lx Transfer Submitted.\n"), szCEDeviceName, dwNumTrans));
    ASSERT(cSemaphore->GetLockedCount() == 0);
    if(cSemaphore->GetLockedCount()) {
        DEBUGMSG(DBG_ERR, (TEXT("%s cSemaphore->GetLockedCount %ld\n"), szCEDeviceName, cSemaphore->GetLockedCount()));
    }
    delete cSemaphore;
}

DWORD AsyncTransFIFO::CountTransferFromUnsignaled(UsbTransfer * pTransfer)
{
    if(IsSignalEmpty()) {
        ASSERT(FALSE);
        return (DWORD) - 1;
    } else {
        DWORD dwCount = 0;
        cMutex.Lock();
        for(DWORD dwSignPos = dwSignal; dwSignPos != dwInput; dwSignPos = NextPosition(dwSignPos)) {
            if(pTransfer == TransferArray[dwSignPos])
                break;
            dwCount++;
        };
        cMutex.Unlock();
        if(dwSignPos == dwInput)
            return (DWORD) - 1;
        else
            return dwCount;
    };

}

BOOL AsyncTransFIFO::AddTransferHandle(UsbTransfer * pTransfer)
{

    if(IsFull()) {
        DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO Full. Something Wrong\n"), szCEDeviceName));
        ASSERT(FALSE);
        return FALSE;
    } else {
        dwNumTrans++;
        cMutex.Lock();
        TransferArray[dwInput] = pTransfer;
        dwInput = NextPosition(dwInput);
        cMutex.Unlock();
        return TRUE;
    };
}

UsbTransfer *AsyncTransFIFO::RemoveTransferHandle()
{
    if(IsEmpty()) {
        ASSERT(FALSE);
        return 0;
    } else {
        BOOL bFlag = cMutex.Lock(1000);
        ASSERT(bFlag);
        UsbTransfer *aTransfer = TransferArray[dwOutput];
        if(dwSignal == dwOutput)    // signal in first. advance automatically.
            dwSignal = NextPosition(dwSignal);
        dwOutput = NextPosition(dwOutput);
        if(bFlag)
            cMutex.Unlock();
        return aTransfer;
    };

}

BOOL AsyncTransFIFO::SignalAndAdvance()
{
    if(IsSignalEmpty()) {
        ASSERT(FALSE);
        return FALSE;
    } else {
        cMutex.Lock();
        UsbTransfer *aTransfer = TransferArray[dwSignal];
        dwSignal = NextPosition(dwSignal);
        DEBUGMSG(DBG_EVENTS, (TEXT("%s AsyncTransFIFO Event Signaled,Transfer %lx \n"), szCEDeviceName, aTransfer->GetTransferID()));
        aTransfer->CompleteNotify();
        cMutex.Unlock();
        return TRUE;
    };
}

DWORD AsyncTransFIFO::ThreadRun()
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +AsyncTransFIFO::ThreadRun \n"), szCEDeviceName));
    UsbTransfer *pTransfer = 0;
    DWORD dwCnt = 0;
    while(!IsTerminated()) {
        pTransfer = FirstTransferHandle();
        if(pTransfer) {
            DEBUGMSG(DBG_EVENTS, (TEXT("%s AsyncTransFIFO Wait return,Transfer %lx \n"), szCEDeviceName, pTransfer->GetTransferID()));
            BOOL bReturn = pTransfer->WaitForTransferComplete(60000);    // second time out.
            if(!bReturn) {
                DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO:: Timeout for waiting Transfer(%lx) Complete"), szCEDeviceName, pTransfer->GetTransferID()));
            }
            pTransfer = RemoveTransferHandle();
            ReleasePositionToken();
            m_pPipe->pipeMutex.Lock();    // It need to make sure the handle is returned.
            if(pTransfer->GetTransferHandle()) {
                DEBUGMSG(DBG_EVENTS, (TEXT("%s AsyncTransFIFO Wait for Signal, Transfer%lx\n"), szCEDeviceName, pTransfer->GetTransferID()));
            } else {
                DEBUGMSG(DBG_ERR, (TEXT("%s AsyncTransFIFO:: Empty Transfer(%lx) Complete\n"), szCEDeviceName, pTransfer->GetTransferID()));
                ASSERT(FALSE);
            };

            if(pTransfer->GetTransferHandle()) {
                if(!pTransfer->IsCompleteNoError()) {
                    errorCode = ERROR_USB_PIPE_TRANSFER;
                    g_pKato->Log(LOG_FAIL, TEXT("%s IsTransferComplete Fail @ AsyncTransFIFO::ThreadRun"), szCEDeviceName);
                }
            }
            m_pPipe->pipeMutex.Unlock();
            delete pTransfer;
        } else {
            if(dwCnt >= 20) {
                DEBUGMSG(DBG_FUNC, (TEXT("%s AsyncTransFIFO:: ThreadRUN  Sleep....\n"), szCEDeviceName));
                dwCnt = 0;
            } else
                dwCnt++;
            Sleep(100);    // yeild
        }
    }
    DEBUGMSG(DBG_FUNC, (TEXT("%s -AsyncTransFIFO::ThreadRun \n"), szCEDeviceName));
    return TRUE;

}

BOOL AsyncUsbPipeService::SubmitIsochRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, PDWORD pBlockLength)
{

    DEBUGMSG(DBG_FUNC, (TEXT("%s +Async::SubmitIsochRequest %s flag(%lx),addr@%lx, length=%lx, pLengths=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize, pBlockLength));
    WORD oneBlockLength = GetMaxPacketSize();
    oneBlockLength = min(oneBlockLength, MAX_ISOCH_FRAME_LENGTH);

    ASSERT(oneBlockLength);
    DWORD curDataSize = dwSize;
    for(DWORD blockNumber = 0; blockNumber < MAX_BLOCKS; blockNumber++)
        if(curDataSize > oneBlockLength) {
            pBlockLength[blockNumber] = oneBlockLength;
            curDataSize -= oneBlockLength;
        } else {
            pBlockLength[blockNumber] = curDataSize;
            curDataSize = 0;
            blockNumber++;
            break;
        };
    DEBUGMSG(DBG_ISOCH, (TEXT("%s Async::SubmitIsochRequest %s flag(%lx),addr@%lx, length=%lx, pLengths=%lx, pblockNum=%lx,startFrame=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize, pBlockLength, blockNumber, m_dwCurrentSyncFrame));
    transStorage.WaitAndLockPositionToken();
    UsbTransfer *pTransfer = new UsbTransfer(0, GetUsbFuncPtr(), lpBuffer, dwSize, dwPhysicalAddr);
    ASSERT(pTransfer);
    transStorage.AddTransferHandle(pTransfer);
    IssueIsochTransfer(pTransfer, m_dwCurrentSyncFrame, blockNumber, pBlockLength, lpBuffer, dwPhysicalAddr);
    m_dwCurrentSyncFrame += blockNumber + ASYNC_MODE_DELAY;

    DEBUGMSG(DBG_FUNC, (TEXT("%s -Async::SubmitIsochRequest\n"), szCEDeviceName));
    return TRUE;
};


BOOL AsyncUsbPipeService::SubmitRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, BOOL bShortOK)
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +Async::SubmitRequest\n"), szCEDeviceName));
    UsbTransfer *pTransfer = new UsbTransfer(0, GetUsbFuncPtr(), lpBuffer, dwSize, dwPhysicalAddr);
    if(bShortOK) {
        SetFlags(GetFlags() | USB_SHORT_TRANSFER_OK);
    }
    ASSERT(pTransfer);
    transStorage.WaitAndLockPositionToken();
    transStorage.AddTransferHandle(pTransfer);
    switch (GetEndPointAttribute()) {
        case USB_ENDPOINT_TYPE_BULK:
            DEBUGMSG(DBG_BULK, (TEXT("%s Async::SubmitRequest(BULK) %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
            IssueBulkTransfer(pTransfer, dwSize, lpBuffer, dwPhysicalAddr);
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            DEBUGMSG(DBG_INTERRUPT, (TEXT("%s Async::SubmitRequest(INTERRUPT) %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
            IssueInterruptTransfer(pTransfer, dwSize, lpBuffer, dwPhysicalAddr);
            break;
        case USB_ENDPOINT_TYPE_CONTROL:
            {
                USB_DEVICE_REQUEST ControlHead = (isInPipe()? InRequest : OutRequest);
                ControlHead.wLength = (WORD) dwSize;
                DEBUGMSG(DBG_CONTROL, (TEXT("%s Async::SubmitRequest(CONTROL) %s flag(%lx),addr@%lx, length=%lx\n"), szCEDeviceName, isInPipe()? TEXT("Read") : TEXT("Write"), GetFlags(), lpBuffer, dwSize));
                IssueControlTransfer(pTransfer, &ControlHead, dwSize, lpBuffer, dwPhysicalAddr);
            };
            break;
        default:
            ASSERT(FALSE);
    };
    DEBUGMSG(DBG_FUNC, (TEXT("%s -Async::SubmitRequest\n"), szCEDeviceName));
    return TRUE;
};

DWORD AsyncUsbPipeService::ActionCompleteNotify(UsbTransfer * pTransfer)
{
    if(pTransfer == NULL)
        return 0;

    DWORD rtValue = TRUE;
    TCHAR *tszDir;

    tszDir = (isInPipe()? TEXT("IN") : TEXT("OUT"));
    ASSERT(pTransfer);
    DWORD dwCount = transStorage.CountTransferFromUnsignaled(pTransfer);
    ASSERT(dwCount == 0);
    if(dwCount == DWORD(-1)) {    // it is not in the list.
        ASSERT(FALSE);
        rtValue = FALSE;
    };
    // for debug
    switch (GetEndPointAttribute()) {
        case USB_ENDPOINT_TYPE_BULK:
            DEBUGMSG(DBG_BULK, (TEXT("%s Bulk Transfer Callback TransferID %lx \n"), szCEDeviceName, pTransfer->GetTransferID()));
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            DEBUGMSG(DBG_INTERRUPT, (TEXT("%s Interrupt Transfer Callback TransferID %lx \n"), szCEDeviceName, pTransfer->GetTransferID()));
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            DEBUGMSG(DBG_ISOCH, (TEXT("%s Isoch Transfer Callback TransferID %lx \n"), szCEDeviceName, pTransfer->GetTransferID()));
            break;
        default:
            ASSERT(FALSE);
            break;

    }
    while(dwCount != (DWORD) - 1 && rtValue == TRUE) {
        if(!transStorage.SignalAndAdvance()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Fail. Fifo out of order !!!"), szCEDeviceName);
            rtValue = FALSE;
        };
        dwCount--;
    };
    // Debugging Code.
/*    PDWORD pdwBuffer=(PDWORD)pTransfer->GetAttachedBuffer();
    DWORD  dwSize=pTransfer->GetAttachedSize();
        if((dwSize % sizeof(DWORD)) != 0)//will check data later if the size is not DWORD-aligned
            return rtValue;

    if (dwSize>=sizeof(DWORD) && pdwBuffer!=NULL && GetEndPointAttribute()!=USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        DWORD dwPattern=*pdwBuffer;
        for (DWORD dwIndex=0;dwIndex<dwSize/sizeof(DWORD);dwIndex++) {
            if (*pdwBuffer!=dwPattern) {
                DEBUGMSG(DBG_ERR,(TEXT("%s !!!Data Corruption!!! dwBuffer %u, dwPattern %u, dwIndex %u"), szCEDeviceName,
                    *pdwBuffer, dwPattern, dwIndex));
//                ASSERT(*pdwBuffer==dwPattern);
                break;
            }
            pdwBuffer++;
            dwPattern++;
        }
    }
                                      */// Endof Debugging Code;
    DEBUGMSG(DBG_ISOCH, (TEXT("%s Callback TransferID %lx:Return \n"), szCEDeviceName, pTransfer->GetTransferID()));
    return rtValue;
}


DWORD AsyncPipeThread::ActionCompleteNotify(UsbTransfer * pTransfer)
{
    if(AsyncUsbPipeService::ActionCompleteNotify(pTransfer)) {
        if(m_pSync && isOutPipe())
            m_pSync->Unlock();
        return TRUE;
    } else
        return FALSE;
}

BOOL AsyncPipeThread::SetEndpoint(BOOL bStalled)
{
    DEBUGMSG(DBG_FUNC, (TEXT("%s +AsyncPipeThread::SetEndpoint\n"), szCEDeviceName));
    BYTE bEndpointAddr = GetPipeDescriptorPtr()->bEndpointAddress;
    USB_TRANSFER hTransfer;
    if(bStalled)
        hTransfer = pDriverPtr->SetFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    else
        hTransfer = pDriverPtr->ClearFeature(NULL, NULL, USB_SEND_TO_ENDPOINT, USB_FEATURE_ENDPOINT_STALL, bEndpointAddr);
    if(hTransfer == NULL) {
        ResetPipe();
        return FALSE;
    };
    UsbTransfer featureTransfer(hTransfer, GetUsbFuncPtr());
    if(featureTransfer.IsTransferComplete()
       && featureTransfer.IsCompleteNoError()) {
        WORD wStatus = 0;
        hTransfer = pDriverPtr->GetStatus(NULL, NULL, USB_SEND_TO_ENDPOINT, bEndpointAddr, &wStatus);
        if(hTransfer == NULL) {
            ResetPipe();
            return FALSE;
        };
        UsbTransfer statusTransfer(hTransfer, GetUsbFuncPtr());
        if(statusTransfer.IsTransferComplete()
           && statusTransfer.IsCompleteNoError()) {
            if(bStalled == ((wStatus & USB_GETSTATUS_DEVICE_STALL) != 0))
                return TRUE;
        } else
            ResetPipe();
    } else
        ResetPipe();
    ASSERT(FALSE);
    return FALSE;
}

DWORD AsyncPipeThread::ThreadRun()
{
    g_pKato->Log(LOG_COMMENT, TEXT("%s +AsyncPipeThread::ThreadRun\n"), szCEDeviceName);

    if(m_bUseLengthArray == TRUE && m_pusTransLens != NULL) {
        RunwithLengthsArray();
        return 0;
    }

    errorCode = GetPipeLastError();
    if(errorCode == 0) {
        switch (GetEndPointAttribute()) {
            case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                {
                    //   Allocate arrays of buffer lengths
                    if(m_pSync)
                        SetFlags(GetFlags() | USB_START_ISOCH_ASAP);

                    m_dwCurrentSyncFrame = pDriverPtr->GetSyncFramNumber();
                    m_dwCurrentSyncFrame += (isInPipe()? TRANSFER_IN_DELAY : TRANSFER_OUT_DELAY);
                    DWORD curDataSize = dwDataSize;
                    DWORD oneDataBlock = min(m_dwBlockSize,
                                 LIMITED_UHCI_BLOCK * (DWORD) GetMaxPacketSize());

                    do {
                        oneDataBlock = min(oneDataBlock, curDataSize);
                        if(m_pSync && isInPipe())
                            m_pSync->Lock();
                        if(!SubmitIsochRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, oneDataBlock, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize, pdwLengths)) {
                            g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Isoch Transfer"), szCEDeviceName);
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            break;
                        };
                        curDataSize -= oneDataBlock;
                        Sleep(ISOCH_TRANS_DELAY);
                    }
                    while(curDataSize);
                };
                break;
            case USB_ENDPOINT_TYPE_BULK:
            case USB_ENDPOINT_TYPE_INTERRUPT:
            case USB_ENDPOINT_TYPE_CONTROL:
                {
                    DWORD curDataSize = dwDataSize;
                    DWORD oneBlockLength = m_dwBlockSize;
                    DWORD dwFactor = LIMITED_UHCI_BLOCK;
                    if(GetEndPointAttribute() != USB_ENDPOINT_TYPE_BULK)
                        dwFactor = 1;

                    do {
                        DWORD dLength = min(curDataSize, oneBlockLength);
                        dLength = min(dLength, dwFactor * GetMaxPacketSize());

                        if(m_pSync && isOutPipe())
                            m_pSync->Lock();
                        if(!SubmitRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, dLength, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize)) {
                            g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Submit Transfer"), szCEDeviceName);
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            break;
                        } else
                            curDataSize -= dLength;
                        if(m_pSync && isInPipe())
                            m_pSync->Unlock();
                    }
                    while(!IsTerminated() && curDataSize);

                    if(curDataSize) {
                        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,InCompleted, Left(%ld)Byte"), szCEDeviceName, curDataSize);
                        errorCode = ERROR_USB_PIPE_TRANSFER;
                    };
                };
                break;
            default:
                ASSERT(FALSE);
        };
    };

    g_pKato->Log(LOG_COMMENT, TEXT("%s -AsyncPipeThread::ThreadRun\n"), szCEDeviceName);
    return 0;
};

BOOL AsyncPipeThread::ValidateLengthsArray()
{
    DWORD dwTotal = 0;
    int iLenIndex = 0;
    while(dwTotal < dwDataSize) {
        DWORD dwPlannedLen = 0xFFFF;
        __try {
            dwPlannedLen = (DWORD) m_pusTransLens[iLenIndex];
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(DBG_ERR, (TEXT("%s access lengths array at offset %d cause AV!"), szCEDeviceName, iLenIndex));
            break;
        }

        dwTotal += dwPlannedLen;
        iLenIndex++;
    }

    if(dwTotal != dwDataSize) {
        DEBUGMSG(DBG_ERR, (TEXT("%s Lengths Array is not valid!"), szCEDeviceName));
        return FALSE;
    }

    return TRUE;
}


void AsyncPipeThread::RunwithLengthsArray()
{

    if(ValidateLengthsArray() == FALSE) {
        DEBUGMSG(DBG_ERR, (TEXT("%s lengths array is not valid!TEST QUIT!!!"), szCEDeviceName));
        return;
    }

    DEBUGMSG(DBG_FUNC, (TEXT("%s +AsyncPipeThread::RunwithLengthsArray\n"), szCEDeviceName));
    errorCode = GetPipeLastError();
    int iLenIndex = 0;
    if(errorCode == 0) {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s AsyncPipeThread::ThreadRun GetEndPointAttr %x\n"), szCEDeviceName, GetEndPointAttribute()));
        switch (GetEndPointAttribute()) {
            case USB_ENDPOINT_TYPE_BULK:
            case USB_ENDPOINT_TYPE_INTERRUPT:
            case USB_ENDPOINT_TYPE_CONTROL:
                {
                    DWORD curDataSize = dwDataSize;
                    DWORD dwFactor = LIMITED_UHCI_BLOCK;
                    DWORD dwcount = 0;
                    if(GetEndPointAttribute() != USB_ENDPOINT_TYPE_BULK)
                        dwFactor = 1;

                    DEBUGMSG(DBG_FUNC, (TEXT("%s Max Packet Size %u\n"), szCEDeviceName, GetMaxPacketSize()));
                    do {
                        DWORD dLength = (DWORD) m_pusTransLens[iLenIndex];
                        if(dLength == 0 && isInPipe()) {
                            dLength = 4;    //we can not issue 0-len IN transfer
                        }
                        dwcount++;

                        if(!SubmitRequest((LPBYTE) lpDataBuffer + dwDataSize - curDataSize, dLength, m_dwPhysicalAddr == 0 ? 0 : m_dwPhysicalAddr + dwDataSize - curDataSize, TRUE)) {
                            errorCode = ERROR_USB_PIPE_TRANSFER;
                            DEBUGMSG(DBG_DETAIL, (TEXT("%s SubmitRequest Return FALSE"), szCEDeviceName));
                            break;
                        } else {
                            dLength = (DWORD) m_pusTransLens[iLenIndex];
                            curDataSize -= dLength;
                        }

                        iLenIndex++;
                    }
                    while(!IsTerminated() && curDataSize);
                    if(curDataSize) {
                        g_pKato->Log(LOG_FAIL, TEXT("%s Fail at Transfer,InCompleted, Left(%ld)Byte"), szCEDeviceName, curDataSize);
                        errorCode = ERROR_USB_PIPE_TRANSFER;
                    };
                };
                break;
            default:
                ASSERT(FALSE);
        };
    };

    DEBUGMSG(DBG_FUNC, (TEXT("%s -AsyncPipeThread::ThreadRun\n"), szCEDeviceName));
    return;
};

DWORD StressTransfer::ThreadRun() {
    if(!pDriverPtr || !sTransferFunc || INVALID_HANDLE(hEvent)) {
        dwLastExit = TPR_FAIL;
        return TPR_FAIL;
    }

    while(!bTerminated) {
        dwLastExit = (*sTransferFunc)(pDriverPtr,iID,dwSeed,dwDataTransfer,&bTerminated);
        if(dwLastExit != TPR_PASS)
      {
            SetEvent(hEvent);
         }
     }
    return TPR_PASS;
}

BOOL GetPipe(UsbClientDrv * pDriverPtr, int iID, LPCUSB_ENDPOINT * pInEP, LPCUSB_ENDPOINT * pOutEP, UCHAR uEPType)
{
    if(pDriverPtr == NULL || iID < 0 || iID > 99 || pInEP == NULL || pOutEP == NULL || g_pTstDevLpbkInfo[iID] == NULL)
        return FALSE;
    UCHAR uEPIndex = 0;
    UCHAR uTotalPairs = g_pTstDevLpbkInfo[iID]->uNumOfPipePairs;
    TCHAR szCEDeviceName[10] = { 0 };
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);
    while(uEPIndex < uTotalPairs)
    {
        //first find out the loopback pair's IN/OUT enpoint addresses
        if(((g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucType == uEPType)
            && g_pTstDevPipeStatus[iID][uEPIndex])
           || (g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucType != uEPType)) {
            uEPIndex++;
            continue;
        }
        for(INT i = 0; i < (INT) pDriverPtr->GetNumOfEndPoints(); i++) {
            LPCUSB_ENDPOINT curPoint = pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if(curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucInEp) {
                *pInEP = curPoint;
            } else if(curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucOutEp) {
                *pOutEP = curPoint;
            }
        }
        if(!(*pInEP && *pOutEP)) {    //one or both endpoints are missing
            g_pKato->Log(LOG_FAIL, TEXT("%s Can not find Pipe Pair"), szCEDeviceName);
            uEPIndex++;
            continue;
        }
        g_pTstDevPipeStatus[iID][uEPIndex] = TRUE;
        return TRUE;

    }
    return FALSE;
}

BOOL FreePipe(UsbClientDrv * pDriverPtr, int iID, LPCUSB_ENDPOINT pInEP, LPCUSB_ENDPOINT pOutEP)
{
    if(pDriverPtr == NULL || iID < 0 || iID > 99 || pInEP == NULL || pOutEP == NULL || g_pTstDevLpbkInfo[iID] == NULL)
        return FALSE;
    UCHAR uIndex = 0;
    TCHAR szCEDeviceName[10] = { 0 };
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);
    UCHAR uTotalPairs = g_pTstDevLpbkInfo[iID]->uNumOfPipePairs;
    while(uIndex < uTotalPairs) {
        //first find out the loopback pair's IN/OUT enpoint addresses
        if(g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucInEp == pInEP->Descriptor.bEndpointAddress && g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucOutEp == pOutEP->Descriptor.bEndpointAddress) {
            g_pTstDevPipeStatus[iID][uIndex] = FALSE;
            return TRUE;
        }
        uIndex++;
    }
    return FALSE;

}


DWORD Overload_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer,BOOL*pFterminated)
{

    DWORD dwRet = TPR_PASS;
    DWORD totalTransferred = 0;
    DWORD dwSingleTrans = 0;
    DWORD dwStatus = 0;
    UINT  uiRand = 0;

    if(pDriverPtr == NULL || iID < 0 || iID > 99)
        return TPR_SKIP;

    srand(dwSeed);

    DWORD dwNumofPipes = pDriverPtr->GetNumOfEndPoints();
    LPCUSB_ENDPOINT p_EndPoints[MAX_ENDPOINTS] = {NULL};
    
    
    TCHAR szCEDeviceName[10] = { 0 };

    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);

    DEBUGMSG(DBG_FUNC, (TEXT("%s +Overload_Transfer with ID=%d Seed=%d TransferLen=%d\n"), szCEDeviceName, iID, dwSeed, dwDataTransfer));

    UCHAR uEPIndex = 0;

        for(UINT uPairIndex= 0;uPairIndex<g_pTstDevLpbkInfo[iID]->uNumOfPipePairs; uPairIndex ++)
        {
   
              for(INT i = 0; i < (INT) pDriverPtr->GetNumOfEndPoints(); i++)
        {
            LPCUSB_ENDPOINT curPoint = pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if(curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uPairIndex].ucInEp) {
                p_EndPoints[uEPIndex++] = curPoint;
            }
            else if(curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uPairIndex].ucOutEp) {
                p_EndPoints[uEPIndex++]= curPoint;
            }
        }
        if(!(p_EndPoints[uEPIndex-1] && p_EndPoints[uEPIndex-2])) {    //one or both endpoints are missing
            g_pKato->Log(LOG_FAIL, TEXT("%s Can not find Pipe Pair"), szCEDeviceName);
            uEPIndex++;
            continue;
        }
        g_pTstDevPipeStatus[iID][uPairIndex] = TRUE;
            
       }



       while((dwStatus == 0 || dwStatus == TPR_PASS) && totalTransferred < dwDataTransfer && !*pFterminated) 
    {
        rand_s(&uiRand);
        DWORD dwTypeToExecute = uiRand % 1000;
        totalTransferred += dwSingleTrans;
        dwSingleTrans = 0;
        if(dwTypeToExecute < 100)
        {    // 10% of time is in ep0 traffic
            rand_s(&uiRand);
            dwStatus = EP0Transfer(pDriverPtr, dwTypeToExecute % 2, (dwTypeToExecute % 32) + 16, uiRand, &dwSingleTrans);
        } 
        else if(dwTypeToExecute < 550) 
          {    // split remaining into variable length and full size packet transfers

               for(UINT uEPIndex= 0;uEPIndex<pDriverPtr->GetNumOfEndPoints() ; uEPIndex +=2 )
               {
                     rand_s(&uiRand);
                     dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[uEPIndex+1], p_EndPoints[uEPIndex], dwTypeToExecute % 2, iID, (dwTypeToExecute % 15) + 25, uiRand, &dwSingleTrans);
               }
        }
        else 
        {
            dwSingleTrans = (dwTypeToExecute * 4);
            dwSingleTrans += (512 - (dwTypeToExecute % 512));
        
               for(UINT uEPIndex= 0;uEPIndex<pDriverPtr->GetNumOfEndPoints() ; uEPIndex+= 2)
                dwStatus = PipeBlockTransfer(pDriverPtr, p_EndPoints[uEPIndex+1], p_EndPoints[uEPIndex], dwTypeToExecute % 2, iID, dwSingleTrans);
            
        }
        
    }

    g_pKato->Log(LOG_COMMENT, _T("Total transferred = %d bytes\n"), totalTransferred);
    dwStatus = (dwStatus == TPR_PASS && totalTransferred >= dwDataTransfer) ? TPR_PASS : TPR_FAIL;


 
          for(UINT i = 0;i<dwNumofPipes ; i += 2)
          {
                 if(p_EndPoints[i] != NULL)
             {
            if(!FreePipe(pDriverPtr, iID, p_EndPoints[i], p_EndPoints[i + 1]))
            ASSERT(0);    // Got invalid endpoint addresses somehow
            p_EndPoints[i] = p_EndPoints[i + 1] = NULL;
              }

          }

    DEBUGMSG(DBG_FUNC, (TEXT("%s -Overload_Transfer\n"), szCEDeviceName));
    return dwRet;
    
}

DWORD RNDIS_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer, BOOL*pFterminated )
{

    DWORD dwRet = TPR_FAIL;
    DWORD totalTransferred = 0;
    DWORD dwSingleTrans = 0;
    DWORD dwStatus = 0;
    DWORD dwNumofPipes = pDriverPtr->GetNumOfEndPoints();
    UINT  uiRand = 0;

       LPCUSB_ENDPOINT p_EndPoints[MAX_ENDPOINTS] = {NULL}; 

    
    if(pDriverPtr == NULL || iID < 0 || iID > 99)
        return TPR_SKIP;

    srand(dwSeed);
    TCHAR szCEDeviceName[10] = { 0 };
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);

    DEBUGMSG(DBG_FUNC, (TEXT("%s +Rndis_Transfer with ID=%d Seed=%d TransferLen=%d\n"), szCEDeviceName, iID, dwSeed, dwDataTransfer));

    if(!GetPipe(pDriverPtr, iID, &p_EndPoints[0], &p_EndPoints[1], USB_ENDPOINT_TYPE_BULK)) {
        g_pKato->Log(LOG_FAIL, _T("Could not open endpoint type bulk for rndis transfer, returning.\n"));
        dwRet = TPR_SKIP;
        goto EXIT;
    }
    if(!GetPipe(pDriverPtr, iID, &p_EndPoints[2], &p_EndPoints[3], USB_ENDPOINT_TYPE_INTERRUPT)) {
        g_pKato->Log(LOG_FAIL, _T("Could not open endpoint type interrupt for rndis transfer, returning.\n"));
        dwRet = TPR_SKIP;
        goto EXIT;
    }

         
     
    while((dwStatus == 0 || dwStatus == TPR_PASS) && totalTransferred < dwDataTransfer && !*pFterminated) 
    {

        rand_s(&uiRand);
        DWORD dwTypeToExecute = uiRand % 1000;
        totalTransferred += dwSingleTrans;
        dwSingleTrans = 0;
        if(dwTypeToExecute < 300) {    // 30% of time is in ep0 traffic
            rand_s(&uiRand);
            dwStatus = EP0Transfer(pDriverPtr, dwTypeToExecute % 2, (dwTypeToExecute % 128) + 128, uiRand, &dwSingleTrans);
        } else if(dwTypeToExecute < 700) {    // another 40% in various data transfer, always signaled by interrupt end
            rand_s(&uiRand);
            dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[1], p_EndPoints[0], dwTypeToExecute % 2, iID, (dwTypeToExecute % 50) + 25, uiRand, &dwSingleTrans);
            rand_s(&uiRand);
            dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[3], p_EndPoints[2], dwTypeToExecute % 2, iID, 5, uiRand, &dwSingleTrans);
        } else {    // remaining traffic for normal transfers
            dwSingleTrans = (dwTypeToExecute * 4);
            dwSingleTrans += (512 - (dwTypeToExecute % 512));
            dwStatus = PipeBlockTransfer(pDriverPtr, p_EndPoints[1], p_EndPoints[0], dwTypeToExecute % 2, iID, dwSingleTrans);
            rand_s(&uiRand);
            dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[3], p_EndPoints[2], dwTypeToExecute % 2, iID, 5, uiRand, &dwSingleTrans);
        }

        Sleep(1000);

        
    }

    g_pKato->Log(LOG_COMMENT, _T("Total transferred = %d bytes\n"), totalTransferred);
    dwRet = (dwStatus == TPR_PASS && totalTransferred >= dwDataTransfer) ? TPR_PASS : TPR_FAIL;


      EXIT:

     for(UINT i = 0; i < dwNumofPipes; i += 2)

     if(p_EndPoints[i] != NULL)
     {
            if(!FreePipe(pDriverPtr, iID, p_EndPoints[i], p_EndPoints[i + 1]))
            ASSERT(0);    // Got invalid endpoint addresses somehow
            p_EndPoints[i] = p_EndPoints[i + 1] = NULL;
      }

       
      DEBUGMSG(DBG_FUNC, (TEXT("%s -Rndis_Transfer\n"), szCEDeviceName));
    
    return dwRet;
}


DWORD Storage_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer,BOOL* pfTerminated)
{

    DWORD dwRet = TPR_FAIL;
    DWORD totalTransferred = 0;
    DWORD dwSingleTrans = 0;
    DWORD dwStatus = 0;
    UINT  uiRand = 0;

    // DWORD dwNumofPipes = pDriverPtr->GetNumOfEndPoints();

    LPCUSB_ENDPOINT p_EndPoints[MAX_ENDPOINTS] = {NULL};


    if(pDriverPtr == NULL || iID < 0 || iID > 99)
        return TPR_SKIP;

    srand(dwSeed);
    TCHAR szCEDeviceName[10] = { 0 };
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);

    DEBUGMSG(DBG_FUNC, (TEXT("%s +Storage_Transfer with ID=%d Seed=%d TransferLen=%d\n"), szCEDeviceName, iID, dwSeed, dwDataTransfer));

    if(!GetPipe(pDriverPtr, iID, &p_EndPoints[0], &p_EndPoints[1], USB_ENDPOINT_TYPE_BULK)) {
        g_pKato->Log(LOG_FAIL, _T("Could not open endpoint type bulk for storage transfer, returning.\n"));
        dwRet = TPR_SKIP;
        goto EXIT;
    }

    while((dwStatus == 0 || dwStatus == TPR_PASS) && totalTransferred < dwDataTransfer && !*pfTerminated) {
        rand_s(&uiRand);
        DWORD dwTypeToExecute = uiRand % 1000;
        totalTransferred += dwSingleTrans;
        dwSingleTrans = 0;
        if(dwTypeToExecute < 200) {    // 20% of time is in ep0 traffic
            rand_s(&uiRand);
            dwStatus = EP0Transfer(pDriverPtr, dwTypeToExecute % 2, (dwTypeToExecute % 32) + 16, uiRand, &dwSingleTrans);
        } else if(dwTypeToExecute < 600) {    // split remaining into variable length and full size packet transfers
            rand_s(&uiRand);
            dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[1], p_EndPoints[0], dwTypeToExecute % 2, iID, (dwTypeToExecute % 50) + 25, uiRand, &dwSingleTrans);
        } else {
            dwSingleTrans = (dwTypeToExecute * 4);
            dwSingleTrans += (512 - (dwTypeToExecute % 512));
            dwStatus = PipeBlockTransfer(pDriverPtr, p_EndPoints[1], p_EndPoints[0], dwTypeToExecute % 2, iID, dwSingleTrans);
        }

    }

    g_pKato->Log(LOG_COMMENT, _T("Total transferred = %d bytes\n"), totalTransferred);
    dwRet = (dwStatus == TPR_PASS && totalTransferred >= dwDataTransfer) ? TPR_PASS : TPR_FAIL;


      EXIT:

   if(pDriverPtr)
   {
      
    if(p_EndPoints[0] != NULL) 
        if(!FreePipe(pDriverPtr, iID, p_EndPoints[0], p_EndPoints[1]))
            ASSERT(0);    // Got invalid endpoint addresses somehow

    DEBUGMSG(DBG_FUNC, (TEXT("%s -Storage_Transfer\n"), szCEDeviceName));
   }

    
      
    return dwRet;
}


DWORD Serial_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer ,BOOL* pfTerminated)
{

    DWORD dwRet = TPR_FAIL;
    DWORD totalTransferred = 0;
    DWORD dwSingleTrans = 0;
    DWORD dwStatus = 0;
    DWORD dwNumofPipes = pDriverPtr->GetNumOfEndPoints();
    UINT  uiRand = 0;

   LPCUSB_ENDPOINT p_EndPoints[MAX_ENDPOINTS] = {NULL};


    if(pDriverPtr == NULL || iID < 0 || iID > 99)
        return TPR_SKIP;

    srand(dwSeed);
    TCHAR szCEDeviceName[10] = { 0 };
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d:"), iID);

    DEBUGMSG(DBG_FUNC, (TEXT("%s +Serial_Transfer with ID=%d Seed=%d TransferLen=%d\n"), szCEDeviceName, iID, dwSeed, dwDataTransfer));

    if(!GetPipe(pDriverPtr, iID, &p_EndPoints[0], &p_EndPoints[1], USB_ENDPOINT_TYPE_BULK)) {
        g_pKato->Log(LOG_FAIL, _T("Could not open endpoint type bulk for serial transfer, returning.\n"));
        dwRet = TPR_SKIP;
        goto EXIT;
    }
    if(!GetPipe(pDriverPtr, iID, &p_EndPoints[2], &p_EndPoints[3], USB_ENDPOINT_TYPE_INTERRUPT)) {
        g_pKato->Log(LOG_FAIL, _T("Could not open endpoint type interrupt for serial transfer, returning.\n"));
        dwRet = TPR_SKIP;
        goto EXIT;
    }

    while((dwStatus == 0 || dwStatus == TPR_PASS) && totalTransferred < dwDataTransfer && !*pfTerminated) 
    {
        rand_s(&uiRand);
        DWORD dwTypeToExecute = uiRand % 1000;
        totalTransferred += dwSingleTrans;
        dwSingleTrans = 0;
        if(dwTypeToExecute < 200) {
            rand_s(&uiRand);
            dwStatus = EP0Transfer(pDriverPtr, dwTypeToExecute % 2, (dwTypeToExecute % 128) + 128, uiRand, &dwSingleTrans);
        } else if(dwTypeToExecute < 500) {
            rand_s(&uiRand);
            DWORD dwPipe = (uiRand % 2) * 2;
            rand_s(&uiRand);
            dwStatus = PipeVaryLenTransfer(pDriverPtr, p_EndPoints[dwPipe + 1], p_EndPoints[dwPipe], dwTypeToExecute % 2, iID, (dwTypeToExecute % 50) + 25, uiRand, &dwSingleTrans);
        } else {
            rand_s(&uiRand);
            DWORD dwPipe = (uiRand % 2) * 2;
            dwSingleTrans = (dwTypeToExecute * 4);
            dwSingleTrans += (512 - (dwTypeToExecute % 512));
            dwStatus = PipeBlockTransfer(pDriverPtr, p_EndPoints[dwPipe + 1], p_EndPoints[dwPipe], dwTypeToExecute % 2, iID, dwSingleTrans);
        }

    }

    g_pKato->Log(LOG_COMMENT, _T("Total transferred = %d bytes\n"), totalTransferred);
    dwRet = (dwStatus == TPR_PASS && totalTransferred >= dwDataTransfer) ? TPR_PASS : TPR_FAIL;


      EXIT:

     if(pDriverPtr)
     {
         for(UINT i = 0; i < dwNumofPipes; i += 2)
        if(p_EndPoints[i] != NULL) {
            if(!FreePipe(pDriverPtr, iID, p_EndPoints[i], p_EndPoints[i + 1]))
                ASSERT(0);    // Got invalid endpoint addresses somehow
            p_EndPoints[i] = p_EndPoints[i + 1] = NULL;
        }

    DEBUGMSG(DBG_FUNC, (TEXT("%s -Serial_Transfer\n"), szCEDeviceName));

         }

    
    return dwRet;
}

BOOL g_bIsochLimit = FALSE;

#define EP0TEST_MAX_DATASIZE    512
#define DEVICE_TO_HOST_MASK 0x80
DWORD EP0Transfer(UsbClientDrv * pDriverPtr, BOOL fStall, DWORD dwNumPackets, DWORD dwSeed, PDWORD pdwDataTransferred)
{

    if(pDriverPtr == NULL || dwNumPackets == 0 || pdwDataTransferred == NULL)
        return TPR_SKIP;

    USB_TDEVICE_REQUEST utdr = { 0 };
    BYTE pBuf[EP0TEST_MAX_DATASIZE] = { 0 };
    BYTE bStartVal = 0;
    BYTE bOdd = 0;
    USHORT usLen = 0;
    BOOL fRet = FALSE;
    BYTE bRetry = 0;
    const BYTE bRetryTimes = (fStall == TRUE) ? 6 : 1;
    UINT uiRand = 0;

    *pdwDataTransferred = 0;
    srand(dwSeed);
    for(uint i = 0; i < dwNumPackets; i++) {
        rand_s(&uiRand);
        bStartVal = uiRand % 256;
        rand_s(&uiRand);
        bOdd = uiRand % 2;
        rand_s(&uiRand);
        usLen = uiRand % 256;
        if(usLen == 0)
            usLen = 1;
        RETAILMSG(1,(_T("EP0Test: No. %d run, data size = %d, %s transfer \r\n"), i, usLen, (bOdd == 0) ? _T("IN") : _T("OUT")));
        *pdwDataTransferred += usLen;
        if(bOdd == 0) {    //in transfer
            utdr.bmRequestType = USB_REQUEST_CLASS | DEVICE_TO_HOST_MASK;
            utdr.bRequest = (fStall == TRUE) ? TEST_REQUEST_EP0TESTINST : TEST_REQUEST_EP0TESTIN;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            bRetry = 0;
            while(SendVendorTransfer(pDriverPtr, TRUE, &utdr, pBuf, NULL) == FALSE) {
                bRetry++;
                if(bRetry < bRetryTimes) {
                    g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed in No. %d try!"), bRetry);
                    continue;
                } else {
                    g_pKato->Log(LOG_FAIL, TEXT("Send IN transfer command failed!"));
                    goto EXIT;
                }
            }
            for(int j = 0; j < usLen; j++) {
                if(pBuf[j] != bStartVal) {
                    g_pKato->Log(LOG_FAIL, TEXT("EP0 IN: at offset %d, expected value = %d, real value = %d!"), j, bStartVal, pBuf[j]);
                    goto EXIT;
                }
                bStartVal++;
            }
        } else {    //out transfer
            utdr.bmRequestType = USB_REQUEST_CLASS;
            utdr.bRequest = (fStall == TRUE) ? TEST_REQUEST_EP0TESTOUTST : TEST_REQUEST_EP0TESTOUT;
            utdr.wValue = bStartVal;
            utdr.wLength = usLen;
            for(int j = 0; j < usLen; j++) {
                pBuf[j] = bStartVal++;
            }
            bRetry = 0;
            while(SendVendorTransfer(pDriverPtr, FALSE, &utdr, pBuf, NULL) == FALSE) {
                bRetry++;
                if(bRetry < bRetryTimes) {
                    g_pKato->Log(LOG_FAIL, TEXT("Send OUT transfer command failed in No. %d try!"), bRetry);
                    continue;
                } else {
                    g_pKato->Log(LOG_FAIL, TEXT("Send OUT transfer command failed!"));
                    goto EXIT;
                }
            }
        }
        memset(pBuf, 0, sizeof(pBuf));
    }

    fRet = TRUE;

      EXIT:

    g_pKato->Log(LOG_DETAIL, TEXT("!!!PLEASE CHECK FUNCTION SIDE OUTPUT TO SEE WHETHER THERE ARE SOME OUT TRANSFER ERRORS!!!"));

    return (fRet == TRUE) ? TPR_PASS : TPR_FAIL;
}


//This function will issue several rounds of transfers on a given pipe pair 
DWORD PipeBlockTransfer(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID, DWORD dwtotalLength)
{
    if(pDriverPtr == NULL || pOutEP == NULL || pInEP == NULL)
        return (DWORD) TPR_SKIP;

    TCHAR szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = { 0 };
    SetDeviceName(szCEDeviceName, (USHORT) iID, pOutEP, pInEP);
    DEBUGMSG(DBG_FUNC, (TEXT("%s +PipeBlockTransfer\n"), szCEDeviceName));


    DWORD dwblockLength;
    DWORD dwPattern = Random();
    DWORD index;
    DWORD dwMaxPacketSize;
    DWORD dwRet = 0;
    UCHAR uEPType = pOutEP->Descriptor.bmAttributes;

    //here we are assuming we have same maxpacket size on both in/out pipes
    dwMaxPacketSize = pOutEP->Descriptor.wMaxPacketSize;
    if(dwMaxPacketSize == 0) {
        g_pKato->Log(LOG_FAIL, TEXT("%s invalid Packet size for the pipe!"), szCEDeviceName);
        return TPR_FAIL;
    }
    //calculate correct packetsize if there are multiple packets per microframe
    DWORD dwMulti = (dwMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11;
    dwMaxPacketSize = dwMaxPacketSize & PACKETS_PART_MASK;
    if(dwMulti == 1)
        dwMaxPacketSize *= 2;
    else if(dwMulti == 2)
        dwMaxPacketSize *= 3;

    //make data buffer ready
    DWORD dwOldTotalLen = dwtotalLength / sizeof(DWORD);
    PDWORD InDataBlock = new DWORD[dwOldTotalLen];
    if(InDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        return (DWORD) TPR_SKIP;
    }
    PDWORD OutDataBlock = new DWORD[dwOldTotalLen];
    if(OutDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        delete[]InDataBlock;
        return (DWORD) TPR_SKIP;
    }
    for(index = 0; index < dwOldTotalLen; index++) {
        OutDataBlock[index] = dwPattern++;
    }
    dwtotalLength -= (dwtotalLength % dwMaxPacketSize);
    //make ready for 
    USB_TDEVICE_REQUEST utdr = { 0 };
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_DATALPBK;
    utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
    USB_TDEVICE_DATALPBK utdl = { 0 };
    utdl.uOutEP = pOutEP->Descriptor.bEndpointAddress;

    dwblockLength = dwMaxPacketSize;

    CSemaphore *pSyncObj = new CSemaphore(0, 0x2000);
    ASSERT(pSyncObj);
    memset(InDataBlock, 0, dwOldTotalLen);

    //if necessary, we need to keep the blocklength to be exactly the same as maxpacket size 
    //in isoch transfer.
    if((uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS)
       && (g_bIsochLimit == TRUE) && (dwblockLength != dwMaxPacketSize)) {
        dwblockLength = dwMaxPacketSize;
    }

    if(pDriverPtr->SetSyncFramNumber() != TRUE)
        g_pKato->Log(LOG_FAIL, TEXT("%s SetFrameNumber return FALSE."), szCEDeviceName);

    if(fAsync) {
        AsyncPipeThread *InPipe = new AsyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                  (PVOID) InDataBlock, dwtotalLength,
                                  dwblockLength, pSyncObj);
        AsyncPipeThread *OutPipe = new AsyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                   (PVOID) OutDataBlock, dwtotalLength,
                                   dwblockLength, pSyncObj);

        //send command to device side
        RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
        utdl.wBlockSize = (USHORT) (dwblockLength);
        utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
        RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

        OutPipe->ResetPipe();
        InPipe->ResetPipe();

        OutPipe->SetEndpoint(TRUE);
        OutPipe->SetEndpoint(FALSE);
        InPipe->SetEndpoint(TRUE);
        InPipe->SetEndpoint(FALSE);

        //issue command to device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete OutPipe;
            delete InPipe;
            delete[]InDataBlock;
            delete[]OutDataBlock;
            return TPR_FAIL;
        }

        BOOL bCheckData = FALSE;
        if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
        }

        Sleep(500);
        OutPipe->ThreadStart();
        Sleep(300);
        InPipe->ThreadStart();

        if(!OutPipe->WaitThreadComplete(240 * 1000)
           || !OutPipe->WaitForComplete(120 * 1000)) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Out Pipe Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else if(!InPipe->WaitThreadComplete(120 * 1000)
              || !InPipe->WaitForComplete(120 * 1000)) {
            g_pKato->Log(LOG_FAIL, TEXT("%s In Pipe Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else if(InPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s InPipe Error Code (%d)"), szCEDeviceName, InPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        } else if(bCheckData) {    //no error check data
            CHECK_RESULT m_Result;
            if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                    g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Async Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                    dwRet = TPR_FAIL;
                }
            }
        }
        if(OutPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s OutPipe Error Code (%d)"), szCEDeviceName, OutPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        }
        delete InPipe;
        delete OutPipe;
    } else {
        SyncPipeThread *InPipe = new SyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                (PVOID) InDataBlock, dwtotalLength,
                                dwblockLength, pSyncObj);
        SyncPipeThread *OutPipe = new SyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                 (PVOID) OutDataBlock, dwtotalLength,
                                 dwblockLength, pSyncObj);

        RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
        utdl.wBlockSize = (USHORT) (dwblockLength);
        utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
        RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

        OutPipe->ResetPipe();
        InPipe->ResetPipe();

        OutPipe->SetEndpoint(TRUE);
        OutPipe->SetEndpoint(FALSE);
        InPipe->SetEndpoint(TRUE);
        InPipe->SetEndpoint(FALSE);

        //issue command to device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete OutPipe;
            delete InPipe;
            delete[]InDataBlock;
            delete[]OutDataBlock;
            return TPR_FAIL;
        }

        BOOL bCheckData = FALSE;
        if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
        }

        Sleep(400);
        OutPipe->ThreadStart();
        Sleep(400);
        InPipe->ThreadStart();

        if(OutPipe->WaitThreadComplete(120 * 1000) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else {
            if(InPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                DEBUGMSG(DBG_ERR, (TEXT("%s Test Time Out\n"), szCEDeviceName));
                g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                dwRet = TPR_FAIL;
            }
        }
        if(InPipe->GetErrorCode() || OutPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Error Happens InPipe %lx, OutPipe %lx"), szCEDeviceName, InPipe->GetErrorCode(), OutPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        } else if(bCheckData) {    //no error, check data
            CHECK_RESULT m_Result;
            if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                    g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Sync Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                    dwRet = TPR_FAIL;
                }
            }
        }

        delete OutPipe;
        delete InPipe;
        DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 13\n"), szCEDeviceName));
    }

    if(dwRet == 0) {
        for(index = 0; index < dwtotalLength / 4; index++) {
            if(OutDataBlock[index] != InDataBlock[index]) {
                if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    DEBUGMSG(DBG_WARN | DBG_ISOCH, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                } else {
                    g_pKato->Log(LOG_FAIL, TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index);
                    DEBUGMSG(DBG_ERR, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                    dwRet = TPR_FAIL;
                    break;
                }
            }
        }
    }

    if(pSyncObj) {
        ASSERT(pSyncObj->GetLockedCount() == 0);
        if(!pSyncObj->GetLockedCount()) {
            DEBUGMSG(DBG_WARN, (TEXT("%s pSyncObject->GetLockedCount %d \n"), szCEDeviceName, pSyncObj->GetLockedCount()));
        };
        delete pSyncObj;
    }


    delete[]InDataBlock;
    delete[]OutDataBlock;
    DEBUGMSG(DBG_FUNC, (TEXT("%s -PipeBlockTransfer\n"), szCEDeviceName));
    return (dwRet == 0 ? TPR_PASS : dwRet);
};

DWORD PipeVaryLenTransfer(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID, WORD wNumPackets, DWORD dwSeed, PDWORD pdwDataTransferred)
{
    if(pDriverPtr == NULL || pOutEP == NULL || pInEP == NULL || wNumPackets > 96 || pdwDataTransferred == NULL)
        return (DWORD) TPR_SKIP;

    TCHAR szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = { 0 };
    SetDeviceName(szCEDeviceName, (USHORT) iID, pOutEP, pInEP);
    DEBUGMSG(DBG_FUNC, (TEXT("%s +PipeVaryLenTransfer\n"), szCEDeviceName));

    DWORD dwblockLength = 0, dwtotalLength = 0;
    // DWORD dwPattern = Random();
    DWORD index = 0;
    DWORD dwMaxPacketSize = 0;
    DWORD dwRet = 0;
    UINT  uiRand = 0;
    // UCHAR uEPType = pOutEP->Descriptor.bmAttributes;
    srand(dwSeed);

    //here we are assuming we have same maxpacket size on both in/out pipes
    dwMaxPacketSize = pOutEP->Descriptor.wMaxPacketSize;
    if(dwMaxPacketSize == 0) {
        g_pKato->Log(LOG_FAIL, TEXT("%s invalid Packet size for the pipe!"), szCEDeviceName);
        return TPR_FAIL;
    }
    //calculate correct packetsize if there are multiple packets per microframe
    DWORD dwMulti = (dwMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11;
    PUSHORT pusLens = new USHORT[wNumPackets];
    dwMaxPacketSize = dwMaxPacketSize & PACKETS_PART_MASK;
    if(dwMulti == 1)
        dwMaxPacketSize *= 2;
    else if(dwMulti == 2)
        dwMaxPacketSize *= 3;

    //make data buffer ready
    PDWORD InDataBlock = new DWORD[(wNumPackets * dwMaxPacketSize) / sizeof(DWORD)];
    if(InDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        return (DWORD) TPR_FAIL;
    }
    PDWORD OutDataBlock = new DWORD[(wNumPackets * dwMaxPacketSize) / sizeof(DWORD)];
    if(OutDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        delete[]InDataBlock;
        return (DWORD) TPR_FAIL;
    }
    //make ready for vendor transfer
    USB_TDEVICE_REQUEST utdr = { 0 };
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_VARYLENTRAN;
    utdr.wLength = sizeof(USB_TDEVICE_VARYLENLPBK) + sizeof(USHORT) * (wNumPackets - 1);
    PBYTE pTCBuf = new BYTE[utdr.wLength];
    if(pTCBuf == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        delete[]InDataBlock;
        delete[]OutDataBlock;
        return (DWORD) TPR_FAIL;
    }
    memset(pTCBuf, 0, utdr.wLength);
    PUSB_TDEVICE_VARYLENLPBK putvll = (PUSB_TDEVICE_VARYLENLPBK) pTCBuf;
    putvll->uOutEP = pOutEP->Descriptor.bEndpointAddress;
    putvll->wNumofPackets = wNumPackets;

    for(uint k = 0; k < wNumPackets; k++) {
        rand_s(&uiRand);
        putvll->pusTransLens[k] = (USHORT) ((uiRand % dwMaxPacketSize) + 1);
        pusLens[k] = putvll->pusTransLens[k];
        dwtotalLength += putvll->pusTransLens[k];
    }
    PBYTE pTemp = (PBYTE) OutDataBlock;
    BYTE StartVal = 0x1;
    for(index = 0; index < dwtotalLength; index++) {
        pTemp[index] = StartVal++;
    }
    *pdwDataTransferred = dwtotalLength;
    dwblockLength = dwMaxPacketSize;
    CSemaphore *pSyncObj = new CSemaphore(0, 0x2000);
    ASSERT(pSyncObj);
    memset(InDataBlock, 0, dwtotalLength);

    if(pDriverPtr->SetSyncFramNumber() != TRUE)
         g_pKato->Log(LOG_FAIL, TEXT("%s SetFrameNumber return FALSE."), szCEDeviceName);
    if(fAsync) {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test,Async, Check Point at 4\n"), szCEDeviceName));
        AsyncPipeThread *InPipe = new AsyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                  (PVOID) InDataBlock, dwtotalLength,
                                  dwblockLength, pSyncObj);
        AsyncPipeThread *OutPipe = new AsyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                   (PVOID) OutDataBlock, dwtotalLength,
                                   dwblockLength, pSyncObj);

        //send command to device side
        RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));

        //set pipe thread to use specified transfer length array
        InPipe->SetTransLens(TRUE, pusLens);
        OutPipe->SetTransLens(TRUE, pusLens);

        RETAILMSG(1,(_T("%s transfer lengths are varied, number of packets = %d"), szCEDeviceName, putvll->wNumofPackets));

        InPipe->ResetPipe();
        OutPipe->ResetPipe();

        OutPipe->SetEndpoint(TRUE);
        OutPipe->SetEndpoint(FALSE);
        InPipe->SetEndpoint(TRUE);
        InPipe->SetEndpoint(FALSE);

        //issue command to device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) putvll, NULL) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete OutPipe;
            delete InPipe;
            delete[]InDataBlock;
            delete[]OutDataBlock;
            return TPR_FAIL;
        }

        DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 5\n"), szCEDeviceName));

        OutPipe->ThreadStart();
        Sleep(30);
        InPipe->ThreadStart();

        if(!OutPipe->WaitThreadComplete(240 * 1000)
           || !OutPipe->WaitForComplete(120 * 1000)) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Out Pipe Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else if(!InPipe->WaitThreadComplete(120 * 1000)
              || !InPipe->WaitForComplete(120 * 1000)) {
            g_pKato->Log(LOG_FAIL, TEXT("%s In Pipe Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else if(InPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s InPipe Error Code (%d)"), szCEDeviceName, InPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        }
        if(OutPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s OutPipe Error Code (%d)"), szCEDeviceName, OutPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        }
        delete InPipe;
        delete OutPipe;
    } else {
        DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test,Sync, Check Point at 4\n"), szCEDeviceName));
        SyncPipeThread *InPipe = new SyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                (PVOID) InDataBlock, dwtotalLength,
                                dwblockLength, pSyncObj);
        SyncPipeThread *OutPipe = new SyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                 (PVOID) OutDataBlock, dwtotalLength,
                                 dwblockLength, pSyncObj);

        RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));

        //set pipe thread to use specified transfer length array
        InPipe->SetTransLens(TRUE, pusLens);
        OutPipe->SetTransLens(TRUE, pusLens);

        RETAILMSG(1,(_T("%s transfer lengths are varied, number of packets = %d"), szCEDeviceName, putvll->wNumofPackets));

        InPipe->ResetPipe();
        OutPipe->ResetPipe();

        OutPipe->SetEndpoint(TRUE);
        OutPipe->SetEndpoint(FALSE);
        InPipe->SetEndpoint(TRUE);
        InPipe->SetEndpoint(FALSE);

        //issue command to device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) putvll, NULL) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete OutPipe;
            delete InPipe;
            delete[]InDataBlock;
            delete[]OutDataBlock;
            delete[]pTCBuf;
            return TPR_FAIL;
        }

        OutPipe->ThreadStart();
        Sleep(30);
        InPipe->ThreadStart();

        if(OutPipe->WaitThreadComplete(120 * 1000) == FALSE) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
            dwRet = TPR_FAIL;
        } else {
            if(InPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                DEBUGMSG(DBG_ERR, (TEXT("%s Test Time Out\n"), szCEDeviceName));
                g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                dwRet = TPR_FAIL;
            }
        }
        if(InPipe->GetErrorCode() || OutPipe->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL, TEXT("%s Error Happens InPipe %lx, OutPipe %lx"), szCEDeviceName, InPipe->GetErrorCode(), OutPipe->GetErrorCode());
            dwRet = TPR_FAIL;
        }

        delete OutPipe;
        delete InPipe;
    }

    if(dwRet == 0) {    //check data
        PBYTE pInBuff = (PBYTE) InDataBlock;
        PBYTE pOutBuff = (PBYTE) OutDataBlock;
        for(int iIndex = 0; iIndex < utdr.wLength; iIndex++) {
            if(pInBuff[iIndex] != pOutBuff[iIndex]) {
                g_pKato->Log(LOG_FAIL, TEXT("%s short transfer In/Out data show difference start at byte %d :"), szCEDeviceName, iIndex);
                g_pKato->Log(LOG_FAIL, TEXT("%s Out byte is: 0x%x, in byte is: 0x%x"), szCEDeviceName, pOutBuff[iIndex], pInBuff[iIndex]);
                dwRet = TPR_FAIL;
                break;
            }
        }
    }

    if(pSyncObj) {
        ASSERT(pSyncObj->GetLockedCount() == 0);
        if(!pSyncObj->GetLockedCount()) {
            DEBUGMSG(DBG_WARN, (TEXT("%s pSyncObject->GetLockedCount %d \n"), szCEDeviceName, pSyncObj->GetLockedCount()));
        };
        delete pSyncObj;
    }

    delete[]InDataBlock;
    delete[]OutDataBlock;
    delete[]pTCBuf;
    DEBUGMSG(DBG_FUNC, (TEXT("%s -PipeVaryLenTransfer\n"), szCEDeviceName));
    return (dwRet == 0 ? TPR_PASS : dwRet);
}

//following APIs are for phymem allocation/free
LONG GetFreePhysMem(PPhysMemNode PMemN, DWORD dwPages)
{
    LONG lCount = 0;
    if(dwPages == 0 || PMemN == NULL)
        return -1;
    for(LONG i = 0; i < MY_PHYS_MEM_SIZE; i++) {
        if(PMemN[i].lSize != 0)
            lCount = 0;
        else {
            lCount++;
            if((DWORD) lCount == dwPages)
                return (i + 1 - lCount);
        }
    }
    return -1;
}

void MarkPhysMemAsUsed(PPhysMemNode PMemN, DWORD dwPages, LONG lIndex, DWORD dwVirtAddress)
{
    LONG i;
    if(dwPages == 0 || PMemN == NULL || lIndex < 0)
        goto FailMarkPMem;

    if(PMemN[lIndex].lSize != 0)
        goto FailMarkPMem;

    PMemN[lIndex].lSize = dwPages--;
    PMemN[lIndex].dwVirtMem = dwVirtAddress;
    for(i = lIndex + 1; i < MY_PHYS_MEM_SIZE; i++) {
        if(dwPages == 0 || PMemN[i].lSize != 0)
            break;
        PMemN[i].lSize = -1;
        PMemN[i].dwVirtMem = dwVirtAddress;
        dwPages--;
    }
    if(dwPages == 0)
        return;

      FailMarkPMem:
    DEBUGMSG(DBG_ERR, (TEXT("MarkPhysMemAsUsed failed \n")));
    return;
}

PVOID AllocPMemory(PDWORD pdwPhAddr, PDWORD pdwPageLength, int iID)
{

    TCHAR szCEDeviceName[10];
    _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d"), iID);

    PVOID InDataBlock;
    DWORD dwPages = *pdwPageLength / PAGE_SIZE;

    if(*pdwPageLength == 0) {
        g_pKato->Log(LOG_FAIL, TEXT("%s AllocPMemory tried to allocate NO MEMORY\n"), szCEDeviceName, *pdwPageLength);
        return NULL;
    }
    if((dwPages * PAGE_SIZE) < (*pdwPageLength))
        dwPages += 1;

    InDataBlock = AllocPhysMem(dwPages * PAGE_SIZE, PAGE_READWRITE | PAGE_NOCACHE, 0,    // alignment
                   0,    // Reserved
                   pdwPhAddr);
    if(InDataBlock == NULL) {
        DEBUGMSG(DBG_ERR, (TEXT("%s AllocPMemory failed to allocate a memory page\n"), szCEDeviceName));
        *pdwPageLength = 0;
    } else {
        *pdwPageLength = dwPages * PAGE_SIZE;
        DEBUGMSG(DBG_DETAIL, (TEXT("%s AllocPMemory allocated %u bytes of Memory\n"), szCEDeviceName, *pdwPageLength));
        g_pKato->Log(LOG_DETAIL, TEXT("%s AllocPMemory allocated %u bytes of Memory\n"), szCEDeviceName, *pdwPageLength);
    }

    return InDataBlock;
}

//This function is almost like PipeNormalForCommand(), the only difference is that this function passes
//physical memory pointer down to USBD
BOOL PipeNormalPhyMemory(UsbClientDrv * pDriverPtr, PPhysMemNode PMemN, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID)
{
    UNREFERENCED_PARAMETER(PMemN);
    if(pDriverPtr == NULL || pOutEP == NULL || pInEP == NULL)
        return (DWORD) - 1;

    TCHAR szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = { 0 };
    SetDeviceName(szCEDeviceName, (USHORT) iID, pOutEP, pInEP);
    DEBUGMSG(DBG_FUNC, (TEXT("%s +PipeNormalPhyMemory\n"), szCEDeviceName));

    PDWORD InDataBlock, OutDataBlock;
    DWORD InDataPhAddr, OutDataPhAddr;
    DWORD dwtotalLength, dwblockLength;
    DWORD dwPattern = Random();
    DWORD index;
    DWORD dwRet = 0;
    DWORD dwMaxPacketSize;
    UCHAR uEPType = pOutEP->Descriptor.bmAttributes;

    //here we are assuming we have same maxpacket size on both in/out pipes
    dwMaxPacketSize = pOutEP->Descriptor.wMaxPacketSize;
    if(dwMaxPacketSize == 0) {
        g_pKato->Log(LOG_FAIL, TEXT("%s invalid Packet size for the pipe!"), szCEDeviceName);
        return dwRet;
    }
    //calculate correct packetsize if there are multiple packets per microframe
    DWORD dwMulti = (dwMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11;
    dwMaxPacketSize = dwMaxPacketSize & PACKETS_PART_MASK;
    if(dwMulti == 1)
        dwMaxPacketSize *= 2;
    else if(dwMulti == 2)
        dwMaxPacketSize *= 3;

    dwtotalLength = dwMaxPacketSize * DATA_LOOP_TIMES;
    DEBUGMSG(DBG_DETAIL | DBG_ISOCH, (TEXT("%s Pipe Normal PhyMemory, total data len %u\n"), szCEDeviceName, dwtotalLength));
    DWORD dwRealLen = dwtotalLength;

    InDataBlock = (PDWORD) AllocPMemory(&InDataPhAddr, &dwRealLen, iID);
    OutDataBlock = (PDWORD) AllocPMemory(&OutDataPhAddr, &dwRealLen, iID);
    if(InDataBlock == NULL || OutDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        FreePhysMem(InDataBlock);
        FreePhysMem(OutDataBlock);
    }

    for(index = 0; index < dwtotalLength / sizeof(DWORD); index++) {
        OutDataBlock[index] = dwPattern++;
    }

    //make ready for 
    USB_TDEVICE_REQUEST utdr = { 0 };
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_DATALPBK;
    utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
    USB_TDEVICE_DATALPBK utdl = { 0 };
    utdl.uOutEP = pOutEP->Descriptor.bEndpointAddress;

    //we expect 4 loops
    dwblockLength = dwMaxPacketSize;
    for(int i = 0; i < 3 && dwRet == 0; i++) {
        CSemaphore *pSyncObj = new CSemaphore(0, 0x2000);
        ASSERT(pSyncObj);
        memset(InDataBlock, 0, dwtotalLength);

        //if necessary, we need to keep the blocklength to be exactly the same as maxpacket size 
        //in isoch transfer.
        if((uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS)
           && (g_bIsochLimit == TRUE) && (dwblockLength != dwMaxPacketSize)) {
            dwblockLength = dwMaxPacketSize;
        }

        if(pDriverPtr->SetSyncFramNumber() != TRUE)
            g_pKato->Log(LOG_FAIL, TEXT("%s SetFrameNumber return FALSE."), szCEDeviceName);

        if(fAsync) {
            AsyncPipeThread *InPipe = new AsyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                      (PVOID) InDataBlock, dwtotalLength,
                                      dwblockLength, pSyncObj, InDataPhAddr);
            AsyncPipeThread *OutPipe = new AsyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                       (PVOID) OutDataBlock, dwtotalLength,
                                       dwblockLength, pSyncObj, OutDataPhAddr);

            //send command to device side
            RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
            utdl.wBlockSize = (USHORT) (dwblockLength);
            utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
            RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

            InPipe->ResetPipe();
            OutPipe->ResetPipe();

            OutPipe->SetEndpoint(TRUE);
            OutPipe->SetEndpoint(FALSE);
            InPipe->SetEndpoint(TRUE);
            InPipe->SetEndpoint(FALSE);

            //issue command to device
            if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
                g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
                delete OutPipe;
                delete InPipe;
                delete[]InDataBlock;
                delete[]OutDataBlock;
                return -1;
            }

            DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 5\n"), szCEDeviceName));
            BOOL bCheckData = FALSE;
            if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
            }

            OutPipe->ThreadStart();
            Sleep(30);
            InPipe->ThreadStart();

            if(!OutPipe->WaitThreadComplete(240 * 1000)
               || !OutPipe->WaitForComplete(120 * 1000)) {
                g_pKato->Log(LOG_FAIL, TEXT("%s Out Pipe Test Time Out"), szCEDeviceName);
                dwRet = (DWORD)-1;
            } else if(!InPipe->WaitThreadComplete(120 * 1000)
                  || !InPipe->WaitForComplete(120 * 1000)) {
                g_pKato->Log(LOG_FAIL, TEXT("%s In Pipe Test Time Out"), szCEDeviceName);
                dwRet = (DWORD)-1;
            } else if(InPipe->GetErrorCode()) {
                g_pKato->Log(LOG_FAIL, TEXT("%s InPipe Error Code (%d)"), szCEDeviceName, InPipe->GetErrorCode());
                dwRet = (DWORD)-1;
            } else if(bCheckData) {    //no error check data
                CHECK_RESULT m_Result;
                if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                    g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                    if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                        g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Async Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                        dwRet = (DWORD)-1;
                    }
                }
            }
            if(OutPipe->GetErrorCode()) {
                g_pKato->Log(LOG_FAIL, TEXT("%s OutPipe Error Code (%d)"), szCEDeviceName, OutPipe->GetErrorCode());
                dwRet = (DWORD)-1;
            }
            delete InPipe;
            delete OutPipe;
        } else {
            SyncPipeThread *InPipe = new SyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                    (PVOID) InDataBlock, dwtotalLength,
                                    dwblockLength, pSyncObj, InDataPhAddr);
            SyncPipeThread *OutPipe = new SyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                     (PVOID) OutDataBlock, dwtotalLength,
                                     dwblockLength, pSyncObj, OutDataPhAddr);

            RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
            utdl.wBlockSize = (USHORT) (dwblockLength);
            utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
            RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

            InPipe->ResetPipe();
            OutPipe->ResetPipe();

            OutPipe->SetEndpoint(TRUE);
            OutPipe->SetEndpoint(FALSE);
            InPipe->SetEndpoint(TRUE);
            InPipe->SetEndpoint(FALSE);

            //issue command to device
            if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
                g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
                delete OutPipe;
                delete InPipe;
                delete[]InDataBlock;
                delete[]OutDataBlock;
                return -1;
            }

            BOOL bCheckData = FALSE;
            if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
            }

            OutPipe->ThreadStart();
            Sleep(30);
            InPipe->ThreadStart();

            if(OutPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                dwRet = (DWORD)-1;
            } else {
                if(InPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                    DEBUGMSG(DBG_ERR, (TEXT("%s Test Time Out\n"), szCEDeviceName));
                    g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                    dwRet = (DWORD)-1;
                }
            }
            if(InPipe->GetErrorCode() || OutPipe->GetErrorCode()) {
                g_pKato->Log(LOG_FAIL, TEXT("%s Error Happens InPipe %lx, OutPipe %lx"), szCEDeviceName, InPipe->GetErrorCode(), OutPipe->GetErrorCode());
                dwRet = (DWORD)-1;
            } else if(bCheckData) {    //no error, check data
                CHECK_RESULT m_Result;
                if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                    g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                    if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                        g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Sync Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                        dwRet = (DWORD)-1;
                    }
                }
            }

            delete OutPipe;
            delete InPipe;
        }

        if(dwRet == 0) {
            for(index = 0; index < dwtotalLength / 4; index++) {
                if(OutDataBlock[index] != InDataBlock[index]) {
                    if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                        DEBUGMSG(DBG_WARN | DBG_ISOCH, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                    } else {
                        g_pKato->Log(LOG_FAIL, TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index);
                        DEBUGMSG(DBG_ERR, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                        dwRet = (DWORD)-1;
                        break;
                    }
                }
            }
        }

        if(pSyncObj) {
            ASSERT(pSyncObj->GetLockedCount() == 0);
            if(!pSyncObj->GetLockedCount()) {
                DEBUGMSG(DBG_WARN, (TEXT("%s pSyncObject->GetLockedCount %d \n"), szCEDeviceName, pSyncObj->GetLockedCount()));
            };
            delete pSyncObj;
        }

        if(dwRet == 0) {
            dwblockLength *= 4;
        }
    }

    FreePhysMem(InDataBlock);
    FreePhysMem(OutDataBlock);

    DEBUGMSG(DBG_FUNC, (TEXT("%s -PipeNormalPhyMemory\n"), szCEDeviceName));
    return dwRet;
};

//This is complementary test category to PipePhyMemory() -- not only it uses physical memory,
//but also it uses a part of allocated phy memory -- it passes the phy mem pointer in a
//base+offset style
BOOL PipeAlignPhyMemory(UsbClientDrv * pDriverPtr, PPhysMemNode PMemN, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID)
{
    UNREFERENCED_PARAMETER(PMemN);
    if(pDriverPtr == NULL || pOutEP == NULL || pInEP == NULL)
        return (DWORD) - 1;

    TCHAR szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = { 0 };
    SetDeviceName(szCEDeviceName, (USHORT) iID, pOutEP, pInEP);
    DEBUGMSG(DBG_FUNC, (TEXT("%s +PipeAlignPhyMemory\n"), szCEDeviceName));

    PDWORD InDataBlock, OutDataBlock;
    PDWORD SInDataBlock, SOutDataBlock;
    DWORD InDataPhAddr, OutDataPhAddr;
    DWORD dwtotalLength, dwblockLength;
    DWORD index;
    DWORD dwRet = 0;
    DWORD dwMaxPacketSize;
    UCHAR uEPType = pOutEP->Descriptor.bmAttributes;

    //here we are assuming we have same maxpacket size on both in/out pipes
    dwMaxPacketSize = pOutEP->Descriptor.wMaxPacketSize;
    if(dwMaxPacketSize == 0) {
        g_pKato->Log(LOG_FAIL, TEXT("%s invalid Packet size for the pipe!"), szCEDeviceName);
        return dwRet;
    }
    //calculate correct packetsize if there are multiple packets per microframe
    DWORD dwMulti = (dwMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11;
    dwMaxPacketSize = dwMaxPacketSize & PACKETS_PART_MASK;
    if(dwMulti == 1)
        dwMaxPacketSize *= 2;
    else if(dwMulti == 2)
        dwMaxPacketSize *= 3;

    DWORD dwLen = dwMaxPacketSize * DATA_LOOP_TIMES * 2;
    dwtotalLength = dwLen / 2;

    DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Alignl PhyMemory, total data len %u\n"), szCEDeviceName, dwtotalLength));

    SInDataBlock = (PDWORD) AllocPMemory(&InDataPhAddr, &dwLen, iID);
    SOutDataBlock = (PDWORD) AllocPMemory(&OutDataPhAddr, &dwLen, iID);
    if(SInDataBlock == NULL || SOutDataBlock == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        FreePhysMem(SInDataBlock);
        FreePhysMem(SOutDataBlock);
    }
    //try different offsets in the physical mem block
    for(DWORD dwStride = dwtotalLength / 4; dwStride < dwtotalLength && dwRet == 0; dwStride += dwtotalLength / 4) {

        InDataBlock = SInDataBlock + dwStride / 4;
        OutDataBlock = SOutDataBlock + dwStride / 4;
        DWORD dwPattern = Random();
        for(index = 0; index < dwtotalLength / sizeof(DWORD); index++) {
            OutDataBlock[index] = dwPattern++;
        }

        //make ready for 
        USB_TDEVICE_REQUEST utdr = { 0 };
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_REQUEST_DATALPBK;
        utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
        USB_TDEVICE_DATALPBK utdl = { 0 };
        utdl.uOutEP = pOutEP->Descriptor.bEndpointAddress;

        //we expect 4 loops
        dwblockLength = dwMaxPacketSize;
        for(int i = 0; i < 3 && dwRet == 0; i++) {
            CSemaphore *pSyncObj = new CSemaphore(0, 0x2000);
            ASSERT(pSyncObj);
            memset(InDataBlock, 0, dwtotalLength);

            //if necessary, we need to keep the blocklength to be exactly the same as maxpacket size 
            //in isoch transfer.
            if((uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS)
               && (g_bIsochLimit == TRUE)
               && (dwblockLength != dwMaxPacketSize)) {
                dwblockLength = dwMaxPacketSize;
            }

            if(pDriverPtr->SetSyncFramNumber() != TRUE)
                g_pKato->Log(LOG_FAIL, TEXT("%s SetFrameNumber return FALSE."), szCEDeviceName);

            if(fAsync) {
                AsyncPipeThread *InPipe = new AsyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                          (PVOID) InDataBlock, dwtotalLength,
                                          dwblockLength, pSyncObj,
                                          InDataPhAddr + dwStride);
                AsyncPipeThread *OutPipe = new AsyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                           (PVOID) OutDataBlock, dwtotalLength,
                                           dwblockLength, pSyncObj,
                                           OutDataPhAddr + dwStride);

                //send command to device side
                RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
                utdl.wBlockSize = (USHORT) (dwblockLength);
                utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
                RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

                InPipe->ResetPipe();
                OutPipe->ResetPipe();

                OutPipe->SetEndpoint(TRUE);
                OutPipe->SetEndpoint(FALSE);
                InPipe->SetEndpoint(TRUE);
                InPipe->SetEndpoint(FALSE);

                //issue command to device
                if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
                    delete OutPipe;
                    delete InPipe;
                    delete[]InDataBlock;
                    delete[]OutDataBlock;
                    return -1;
                }

                DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 5\n"), szCEDeviceName));
                BOOL bCheckData = FALSE;
                if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
                }

                OutPipe->ThreadStart();
                Sleep(30);
                InPipe->ThreadStart();

                if(!OutPipe->WaitThreadComplete(240 * 1000)
                   || !OutPipe->WaitForComplete(120 * 1000)) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s Out Pipe Test Time Out"), szCEDeviceName);
                    dwRet = (DWORD)-1;
                } else if(!InPipe->WaitThreadComplete(120 * 1000)
                      || !InPipe->WaitForComplete(120 * 1000)) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s In Pipe Test Time Out"), szCEDeviceName);
                    dwRet = (DWORD)-1;
                } else if(InPipe->GetErrorCode()) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s InPipe Error Code (%d)"), szCEDeviceName, InPipe->GetErrorCode());
                    dwRet = (DWORD)-1;
                } else if(bCheckData) {    //no error check data
                    CHECK_RESULT m_Result;
                    if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                        g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                        if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                            g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Async Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                            dwRet = (DWORD)-1;
                        }
                    }
                }
                if(OutPipe->GetErrorCode()) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s OutPipe Error Code (%d)"), szCEDeviceName, OutPipe->GetErrorCode());
                    dwRet = (DWORD)-1;
                }
                delete InPipe;
                delete OutPipe;
            } else {
                SyncPipeThread *InPipe = new SyncPipeThread(pDriverPtr, &(pInEP->Descriptor), iID,
                                        (PVOID) InDataBlock, dwtotalLength,
                                        dwblockLength, pSyncObj,
                                        InDataPhAddr + dwStride);
                SyncPipeThread *OutPipe = new SyncPipeThread(pDriverPtr, &(pOutEP->Descriptor), iID,
                                         (PVOID) OutDataBlock, dwtotalLength,
                                         dwblockLength, pSyncObj,
                                         OutDataPhAddr + dwStride);

                RETAILMSG(1,(_T("%s MaxPacketSize is: %d"), szCEDeviceName, dwMaxPacketSize));
                utdl.wBlockSize = (USHORT) (dwblockLength);
                utdl.wNumofBlocks = (USHORT) (dwtotalLength / (utdl.wBlockSize));
                RETAILMSG(1,(_T("%s size = %d, rounds = %d"), szCEDeviceName, utdl.wBlockSize, utdl.wNumofBlocks));

                InPipe->ResetPipe();
                OutPipe->ResetPipe();

                OutPipe->SetEndpoint(TRUE);
                OutPipe->SetEndpoint(FALSE);
                InPipe->SetEndpoint(TRUE);
                InPipe->SetEndpoint(FALSE);

                //issue command to device
                if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID) & utdl, NULL) == FALSE) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
                    delete OutPipe;
                    delete InPipe;
                    delete[]InDataBlock;
                    delete[]OutDataBlock;
                    return -1;
                }

                BOOL bCheckData = FALSE;
                if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    bCheckData = OutPipe->FormatData((LPBYTE) OutDataBlock, dwtotalLength);
                }

                OutPipe->ThreadStart();
                Sleep(30);
                InPipe->ThreadStart();

                if(OutPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                    dwRet = (DWORD)-1;
                } else {
                    DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 11a\n"), szCEDeviceName));
                    if(InPipe->WaitThreadComplete(120 * 1000) == FALSE) {
                        DEBUGMSG(DBG_ERR, (TEXT("%s Test Time Out\n"), szCEDeviceName));
                        g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
                        dwRet = (DWORD)-1;
                    }
                }
                if(InPipe->GetErrorCode() || OutPipe->GetErrorCode()) {
                    g_pKato->Log(LOG_FAIL, TEXT("%s Error Happens InPipe %lx, OutPipe %lx"), szCEDeviceName, InPipe->GetErrorCode(), OutPipe->GetErrorCode());
                    dwRet = (DWORD)-1;
                } else if(bCheckData) {    //no error, check data
                    CHECK_RESULT m_Result;
                    if(InPipe->CheckData((LPBYTE) InDataBlock, dwtotalLength, &m_Result)) {
                        g_pKato->Log(LOG_COMMENT, TEXT("%s  Loopback Transfer Total(%ld),Success(%ld),Lost(%ld),Junk(%ld),SeqError(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess, m_Result.dwLost, m_Result.dwJunk, m_Result.dwSeqError);
                        if(m_Result.dwTotal - m_Result.dwSuccess > m_Result.dwTotal / 2) {    // error big than 10 persent
                            g_pKato->Log(LOG_FAIL, TEXT("%s Too many error! Sync Total(%ld),Success(%ld)"), szCEDeviceName, m_Result.dwTotal, m_Result.dwSuccess);
                            dwRet = (DWORD)-1;
                        }
                    }
                }

                delete OutPipe;
                delete InPipe;
                DEBUGMSG(DBG_DETAIL, (TEXT("%s Pipe Nomal Test, Check Point at 13\n"), szCEDeviceName));
            }

            if(dwRet == 0) {
                for(index = 0; index < dwtotalLength / 4; index++) {
                    if(OutDataBlock[index] != InDataBlock[index]) {
                        if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                            DEBUGMSG(DBG_WARN | DBG_ISOCH, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                        } else {
                            g_pKato->Log(LOG_FAIL, TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index);
                            DEBUGMSG(DBG_ERR, (TEXT("%s OutData(%lx) not equal to InData(%lx) at Position(%lx)\n"), szCEDeviceName, OutDataBlock[index], InDataBlock[index], index));
                            dwRet = (DWORD)-1;
                            break;
                        }
                    }
                }
            }

            if(pSyncObj) {
                ASSERT(pSyncObj->GetLockedCount() == 0);
                if(!pSyncObj->GetLockedCount()) {
                    DEBUGMSG(DBG_WARN, (TEXT("%s pSyncObject->GetLockedCount %d \n"), szCEDeviceName, pSyncObj->GetLockedCount()));
                };
                delete pSyncObj;
            }

            if(dwRet == 0) {
                dwblockLength *= 4;
            }
        }
    }

    FreePhysMem(SInDataBlock);
    FreePhysMem(SOutDataBlock);

    DEBUGMSG(DBG_FUNC, (TEXT("%s -PipeNormalPhyMemory\n"), szCEDeviceName));
    return dwRet;
}


VOID SetDeviceName(LPTSTR Buff, USHORT uID, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP)
{
    if(Buff == NULL || (pOutEP == NULL && pInEP == NULL))
        return;

    if(uID > 99)
        return;

    wcsncpy_s(Buff, SET_DEV_NAME_BUFF_SIZE, TEXT("Dev"), 3);
    Buff[3] = _T('0') + uID / 10;
    Buff[4] = _T('0') + uID % 10;
    Buff[5] = _T(':');

    UCHAR uOutEp = 0xFF;
    UCHAR uInEp = 0xFF;
    UCHAR uAttribute = 4;
    if(pOutEP != NULL) {
        uOutEp = pOutEP->Descriptor.bEndpointAddress;
        uAttribute = pOutEP->Descriptor.bmAttributes;
    }
    if(pInEP != NULL) {
        uInEp = pInEP->Descriptor.bEndpointAddress;
        if(uAttribute == 4)
            uAttribute = pInEP->Descriptor.bmAttributes;
    }

    _stprintf_s(&Buff[5], SET_DEV_NAME_BUFF_SIZE-5, _T("(OUT(0x%x)-IN(0x%x),%s)"), uOutEp, uInEp, g_szEPTypeStrings[uAttribute]);

    Buff[SET_DEV_NAME_BUFF_SIZE-1] = TCHAR(0);
}
