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

UsbServ.cpp

Abstract:  
    USB Driver Service.

Functions:

Notes: 

--*/

#define __THIS_FILE__   TEXT("UsbServ.cpp")

#include <windows.h>
#include <memory.h>
#include "usbstress.h"
#include "UsbServ.h"
UsbBasic::UsbBasic(LPCUSB_FUNCS UsbFuncsPtr, LPCUSB_INTERFACE pInterface, LPCWSTR uniqueDriverId, LPCUSB_DRIVER_SETTINGS lpDriverSettings):lpUsbFuncs(UsbFuncsPtr)
{
    if(UsbFuncsPtr == NULL || pInterface == NULL || uniqueDriverId == NULL || lpDriverSettings == NULL)
        return;

    lpInterface = (LPUSB_INTERFACE) malloc(sizeof(USB_INTERFACE));
    memcpy(lpInterface, pInterface, sizeof(USB_INTERFACE));
    DWORD dwLen = wcslen(uniqueDriverId) + 1;
    szUniqueDriverId = (LPWSTR) malloc(sizeof(WCHAR) * dwLen);
    wcscpy_s(szUniqueDriverId, dwLen, uniqueDriverId);
}

UsbBasic::~UsbBasic()
{
    free(lpInterface);
    free(szUniqueDriverId);
}

void
 UsbBasic::debug(LPCTSTR szFormat, ...)
{
    TCHAR szBuffer[1024] = TEXT("USBClass: ");

    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer+10, _countof(szBuffer)-10, _countof(szBuffer)-13, szFormat, pArgs);
    va_end(pArgs);

    _tcscat_s(szBuffer, _countof(szBuffer), TEXT("\r\n"));

    OutputDebugString(szBuffer);
}


VOID UsbBasic::GetUSBDVersion(LPDWORD lpdwMajorVersion, LPDWORD lpdwMinorVersion)
{
    if(lpUsbFuncs != NULL)
        (*lpUsbFuncs->lpGetUSBDVersion) (lpdwMajorVersion, lpdwMinorVersion);
}

//Helper commands
BOOL UsbBasic::TranslateStringDesc(LPCUSB_STRING_DESCRIPTOR lpStringDesc, LPWSTR szString, DWORD cchStringLength)
{
    if(lpUsbFuncs != NULL)
        return (*lpUsbFuncs->lpTranslateStringDesc) (lpStringDesc, szString, cchStringLength);

    return FALSE;

}

LPCUSB_INTERFACE UsbBasic::FindInterface(LPCUSB_DEVICE lpUsbDevice, UCHAR bInterfaceNumber, UCHAR bAlternateSetting)
{
    if(lpUsbFuncs != NULL)
        return (*lpUsbFuncs->lpFindInterface) (lpUsbDevice, bInterfaceNumber, bAlternateSetting);

    return NULL;

};

//---------------------------------Usb Transfer--------------------------------
DWORD UsbTransfer::dwCurrentID = 0;
UsbTransfer::UsbTransfer(USB_TRANSFER usbTransfer, LPCUSB_FUNCS pUsbFuncs, LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr):
hUsbTransfer(usbTransfer), lpUsbFuncs(pUsbFuncs), transferEvent(FALSE, TRUE, NULL, NULL), lpAttachedBuffer(lpBuffer), dwAttachedSize(dwSize), dwAttachedPhysicalAddr(dwPhysicalAddr)
{
    ASSERT(pUsbFuncs);
    ++dwCurrentID;
    dwID = dwCurrentID;
    transferEvent.ResetEvent();
}

UsbTransfer::~UsbTransfer()
{
    if(hUsbTransfer)
        CloseTransfer();
};

VOID UsbTransfer::ReInitial(USB_TRANSFER usbTransfer, LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr)
{
    if(hUsbTransfer)
        CloseTransfer();
    hUsbTransfer = usbTransfer;
    transferEvent.ResetEvent();
    ++dwCurrentID;
    dwID = dwCurrentID;
    lpAttachedBuffer = lpBuffer;
    dwAttachedSize = dwSize;
    dwAttachedPhysicalAddr = dwPhysicalAddr;
}

// get info on Transfers
BOOL UsbTransfer::IsTransferComplete()
{
    ASSERT(hUsbTransfer);
    return (*lpUsbFuncs->lpIsTransferComplete) (hUsbTransfer);
}

BOOL UsbTransfer::IsCompleteNoError()
{
    if(hUsbTransfer && IsTransferComplete()) {
        DWORD dwBytesTransfered = 0;
        DWORD dwError;
        if(GetTransferStatus(&dwBytesTransfered, &dwError)
           && dwError == USB_NO_ERROR)
            return TRUE;
        else {
            DEBUGMSG(DBG_ERR, (TEXT("GetTransferStatus return %d!"), dwError));
        }
    }
    return FALSE;
}

BOOL UsbTransfer::GetTransferStatus(LPDWORD lpdwBytesTransfered, LPDWORD lpdwError)
{
    ASSERT(hUsbTransfer);
    return (*lpUsbFuncs->lpGetTransferStatus) (hUsbTransfer, lpdwBytesTransfered, lpdwError);
}

BOOL UsbTransfer::GetIsochResults(DWORD cFrames, LPDWORD lpdwBytesTransfered, LPDWORD lpdwErrors)
{
    ASSERT(hUsbTransfer);
    return (*lpUsbFuncs->lpGetIsochResults) (hUsbTransfer, cFrames, lpdwBytesTransfered, lpdwErrors);

}

// transfer maniuplators
BOOL UsbTransfer::AbortTransfer(DWORD dwFlags)
{
    ASSERT(hUsbTransfer);
    return (*lpUsbFuncs->lpAbortTransfer) (hUsbTransfer, dwFlags);
}

BOOL UsbTransfer::CloseTransfer()
{
    ASSERT(hUsbTransfer);
    BOOL rt = (*lpUsbFuncs->lpCloseTransfer) (hUsbTransfer);
    hUsbTransfer = NULL;
    return rt;
}


// transfer callback.

DWORD WINAPI TransferNotify(LPVOID lpvNotifyParameter)
{
    PTransStatus pTransStatus = (PTransStatus) lpvNotifyParameter;
    if(!pTransStatus)
        return (DWORD)-1;
       //Fixing Transfer Notify race Condition 
    if((*(pTransStatus->lpUsbFuncs->lpIsTransferComplete))
           (pTransStatus->hTransfer))
            pTransStatus->IsFinished = TRUE;
    else
            pTransStatus->IsFinished = FALSE;

    return 1;
}

//------------------------------------Usb Class-------------------------------------

UsbClass::UsbClass(USB_HANDLE usbHandle, LPCUSB_FUNCS UsbFuncsPtr, LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId, LPCUSB_DRIVER_SETTINGS lpDriverSettings):
UsbBasic(UsbFuncsPtr, lpInterface, szUniqueDriverId, lpDriverSettings), hUsb(usbHandle)
{
    DEBUGMSG(DBG_FUNC, (TEXT("UsbClass")));
    if(lpInterface == NULL)
        return;
    m_dwSyncFrameNumber = 0;
    DefaultDeviceDesc = (lpInterface->lpEndpoints)->Descriptor;
};

UsbClass::~UsbClass()
{

}


BOOL UsbClass::DisableDevice(BOOL fReset, BYTE bInterfaceNumber)
{
    return (*lpUsbFuncs->lpDisableDevice) (hUsb, fReset, bInterfaceNumber);
}

BOOL UsbClass::SuspendDevice(BYTE bInterfaceNumber)
{
    return (*lpUsbFuncs->lpSuspendDevice) (hUsb, bInterfaceNumber);
}

BOOL UsbClass::ResumeDevice(BYTE bInterfaceNumber)
{
    return (*lpUsbFuncs->lpResumeDevice) (hUsb, bInterfaceNumber);
}



//USB Subsystem Commands
BOOL UsbClass::GetFrameNumber(LPDWORD lpdwFrameNumber)
{
       if(lpUsbFuncs != NULL)
        return (*lpUsbFuncs->lpGetFrameNumber) (hUsb, lpdwFrameNumber);
    else 
        return FALSE;
}

BOOL UsbClass::GetFrameLength(LPUSHORT lpuFrameLength)
{
    return (*lpUsbFuncs->lpGetFrameLength) (hUsb, lpuFrameLength);
};

//Enables Device to Adjust the USB SOF period on OHCI or UHCI cards
BOOL UsbClass::TakeFrameLengthControl()
{
    return (*lpUsbFuncs->lpTakeFrameLengthControl) (hUsb);
}

BOOL UsbClass::SetFrameLength(HANDLE hEvent, USHORT uFrameLength)
{
    return (*lpUsbFuncs->lpSetFrameLength) (hUsb, hEvent, uFrameLength);
}

BOOL UsbClass::ReleaseFrameLengthControl()
{
    return (*lpUsbFuncs->lpReleaseFrameLengthControl) (hUsb);
}

// Gets info on a device
LPCUSB_DEVICE UsbClass::GetDeviceInfo()
{
    return (*lpUsbFuncs->lpGetDeviceInfo) (hUsb);
};

USHORT UsbClass::GetEP0PacketSize()
{
    LPCUSB_DEVICE lpDevInfo = (*lpUsbFuncs->lpGetDeviceInfo) (hUsb);
    if(lpDevInfo != NULL) {
        return lpDevInfo->Descriptor.bMaxPacketSize0;
    } else {
        return 0xFFFF;
    }

};


//Device commands
USB_TRANSFER UsbClass::IssueVendorTransfer(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, LPCUSB_DEVICE_REQUEST lpControlHeader, LPVOID lpvBuffer, ULONG uBufferPhysicalAddress)
{
    return (*lpUsbFuncs->lpIssueVendorTransfer) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, lpControlHeader, lpvBuffer, uBufferPhysicalAddress);
}

USB_TRANSFER UsbClass::GetInterface(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bInterfaceNumber, PUCHAR lpvAlternateSetting)
{
    return (*lpUsbFuncs->lpGetInterface) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bInterfaceNumber, lpvAlternateSetting);
}

USB_TRANSFER UsbClass::SetInterface(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bInterfaceNumber, UCHAR bAlternateSetting)
{
    return (*lpUsbFuncs->lpSetInterface) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bInterfaceNumber, bAlternateSetting);
}

USB_TRANSFER UsbClass::GetDescriptor(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bType, UCHAR bIndex, WORD wLanguage, WORD wLength, LPVOID lpvBuffer)
{
    return (*lpUsbFuncs->lpGetDescriptor) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bType, bIndex, wLanguage, wLength, lpvBuffer);
}

USB_TRANSFER UsbClass::SetDescriptor(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bType, UCHAR bIndex, WORD wLanguage, WORD wLength, LPVOID lpvBuffer)
{
    return (*lpUsbFuncs->lpSetDescriptor) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bType, bIndex, wLanguage, wLength, lpvBuffer);
}

USB_TRANSFER UsbClass::SetFeature(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, WORD wFeature, UCHAR bIndex)
{
    return (*lpUsbFuncs->lpSetFeature) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, wFeature, bIndex);
}

USB_TRANSFER UsbClass::ClearFeature(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, WORD wFeature, UCHAR bIndex)
{
    return (*lpUsbFuncs->lpClearFeature) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, wFeature, bIndex);
}

USB_TRANSFER UsbClass::GetStatus(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bIndex, LPWORD lpwStatus)
{
    return (*lpUsbFuncs->lpGetStatus) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bIndex, lpwStatus);
}

USB_TRANSFER UsbClass::SyncFrame(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress, LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bEndPoint, LPWORD lpwFrame)
{
    return (*lpUsbFuncs->lpSyncFrame) (hUsb, ((dwFlags & USB_NO_WAIT) != 0) ? lpNodifyAddress : NULL, lpNodifyParameter, dwFlags, bEndPoint, lpwFrame);
}

//-----default pipe----------------------
BOOL UsbClass::ResetDefaultPipe()
{
    return (*lpUsbFuncs->lpResetDefaultPipe) (hUsb);
}

BOOL UsbClass::IsDefaultPipeHalted(LPBOOL lpbHalted)
{
    return (*lpUsbFuncs->lpIsDefaultPipeHalted) (hUsb, lpbHalted);
}

//-----------------------------------UsbPipeService--------------------------------

UsbPipeService::UsbPipeService(LPCUSB_FUNCS ptrUsbFuncs, USB_HANDLE usbHandle, LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor, BOOL Async)
{
    DEBUGMSG(DBG_FUNC, (TEXT("UsbPipeService")));

    if(ptrUsbFuncs == NULL || usbHandle == NULL || lpEndPointDescriptor == NULL)
        return;

    errorCode = 0;
    hPipe = NULL;
    lpUsbFuncs = ptrUsbFuncs;
    DeviceDesc = *lpEndPointDescriptor;
    dwFlags = 0;        //USB_SEND_TO_ENDPOINT;
    if(Async) {
        dwFlags |= USB_NO_WAIT;
        lpStartAddress = &UsbPipeService::TransferNotify;
    } else
        lpStartAddress = NULL;

    if(lpUsbFuncs) {
        hPipe = (*(lpUsbFuncs->lpOpenPipe)) (usbHandle, lpEndPointDescriptor);
        if(hPipe == NULL) {
            DEBUGMSG(DBG_ERR, (TEXT("can not open pipe usbHandle=%lx,Descriptor=%lx"), usbHandle, lpEndPointDescriptor));
            errorCode = ERROR_USB_PIPE_OPEN;
        };
        if(isInPipe())
            dwFlags |= USB_IN_TRANSFER;
        if(isOutPipe())
            dwFlags |= USB_OUT_TRANSFER;
    } else {
        DEBUGMSG(DBG_ERR, (TEXT(" usb func is NULL!!!")));
        errorCode = ERROR_USB_NULL_FUNC;
    };
    DEBUGMSG(DBG_FUNC, (TEXT("End of UsbPipeService")));

}

UsbPipeService::~UsbPipeService()
{
    errorCode = 0;
    if(hPipe) {
        if(!(*(lpUsbFuncs->lpClosePipe)) (hPipe))
            errorCode = ERROR_USB_PIPE_CLOSE;
    };
};


BOOL UsbPipeService::AbortPipeTransfers(DWORD dwFlags)
{
    ASSERT(hPipe);
    if((*lpUsbFuncs->lpAbortPipeTransfers) (hPipe, dwFlags))
        return TRUE;
    else {
        errorCode = ERROR_USB_PIPE;
        return FALSE;
    };

}

BOOL UsbPipeService::ResetPipe()
{
    ASSERT(hPipe);
    if((*lpUsbFuncs->lpResetPipe) (hPipe))
        return TRUE;
    else {
        errorCode = ERROR_USB_PIPE;
        return FALSE;
    };
}

BOOL UsbPipeService::IsPipeHalted(LPBOOL lpbHalted)
{
    ASSERT(hPipe);
    if((*lpUsbFuncs->lpIsPipeHalted) (hPipe, lpbHalted))
        return TRUE;
    else {
        errorCode = ERROR_USB_PIPE;
        return FALSE;
    };
}

#define ISSUE_RETRY 5
#define RETRY_SLEEP 20
USB_TRANSFER UsbPipeService::IssueControlTransfer(UsbTransfer * uTransfer, LPCVOID lpvControlHeader, DWORD dwBufferSize, LPVOID lpvBuffer, ULONG uBufferPhysicalAddress)
{
    if(uTransfer == NULL || hPipe == NULL)
        return NULL;
    USB_TRANSFER hTransfer;
    DWORD dwRetry = 0;
    do {
        pipeMutex.Lock();
        DEBUGMSG(DBG_CONTROL, (TEXT("+IssueControlTransfer(buffersize %lx,buffer %lx, phAddr %lx) TransferID %lx \n"), dwBufferSize, lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
        uTransfer->SetPipe(this);
        hTransfer = (*lpUsbFuncs->lpIssueControlTransfer) (hPipe, lpStartAddress, uTransfer, dwFlags, lpvControlHeader, dwBufferSize, lpvBuffer, uBufferPhysicalAddress);
        DEBUGMSG(DBG_CONTROL, (TEXT("-IssueControlTransfer(TransferID %lx \n"), uTransfer->GetTransferID()));
        uTransfer->SetTransferHandle(hTransfer);
        pipeMutex.Unlock();
        if(hTransfer == NULL) {
            DEBUGMSG(DBG_CONTROL | DBG_WARN, (TEXT("IssueControlTransfer(buffersize %lx,buffer %lx, phAddr %lx) Failed, TransferID %lx \n"), dwBufferSize, lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
            Sleep(RETRY_SLEEP);
        };
    }
    while(hTransfer == NULL && (dwRetry++ < ISSUE_RETRY));

    if(hTransfer == NULL) {
        errorCode = ERROR_USB_PIPE_TRANSFER;
        ASSERT(FALSE);
    } else if((dwFlags & USB_NO_WAIT) == 0)
        ActionCompleteNotify(uTransfer);
    return hTransfer;
}

USB_TRANSFER UsbPipeService::IssueBulkTransfer(UsbTransfer * uTransfer, DWORD dwBufferSize, LPVOID lpvBuffer, ULONG uBufferPhysicalAddress)
{
    if(uTransfer == NULL || hPipe == NULL)
        return NULL;

    USB_TRANSFER hTransfer;
    DWORD dwRetry = 0;
    do {
        pipeMutex.Lock();
        DEBUGMSG(DBG_BULK, (TEXT("+IssueBulkTransfer(buffersize %lx,buffer %lx, dwFirstData %lx, phAddr %lx) TransferID %lx \n"), dwBufferSize, lpvBuffer, *(PDWORD) lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
        uTransfer->SetPipe(this);
        hTransfer = (*lpUsbFuncs->lpIssueBulkTransfer) (hPipe, lpStartAddress, uTransfer, dwFlags, dwBufferSize, (LPVOID) lpvBuffer, uBufferPhysicalAddress);
        DEBUGMSG(DBG_BULK, (TEXT("-IssueBulkTransfer TransferID %lx \n"), uTransfer->GetTransferID()));
        uTransfer->SetTransferHandle(hTransfer);
        pipeMutex.Unlock();
        if(hTransfer == NULL) {
            DEBUGMSG(DBG_BULK | DBG_WARN, (TEXT("IssueBulkTransfer(buffersize %lx,buffer %lx, phAddr %lx) Failed, TransferID %lx \n"), dwBufferSize, lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
            Sleep(RETRY_SLEEP);
        };
    }
    while(hTransfer == NULL && (dwRetry++ < ISSUE_RETRY));

    if(hTransfer == NULL) {
        errorCode = ERROR_USB_PIPE_TRANSFER;
        ASSERT(FALSE);
    } else {
        DEBUGMSG(DBG_BULK, (TEXT("Transfer IssueBulkTransfer, hTransfer=%u, TransferID %lx \n"), hTransfer, uTransfer->GetTransferID()));
        if((dwFlags & USB_NO_WAIT) == 0) {
            ActionCompleteNotify(uTransfer);
        }
    }
    return hTransfer;
}

USB_TRANSFER UsbPipeService::IssueInterruptTransfer(UsbTransfer * uTransfer, DWORD dwBufferSize, LPVOID lpvBuffer, ULONG uBufferPhysicalAddress)
{
    if(uTransfer == NULL || hPipe == NULL)
        return NULL;
    USB_TRANSFER hTransfer;
    DWORD dwRetry = 0;
    do {
        pipeMutex.Lock();
        DEBUGMSG(DBG_INTERRUPT, (TEXT("+IssueInterruptTransfer(buffersize %lx,buffer %lx, phAddr %lx) TransferID %lx \n"), dwBufferSize, lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
        uTransfer->SetPipe(this);
        hTransfer = (*lpUsbFuncs->lpIssueInterruptTransfer) (hPipe, lpStartAddress, uTransfer, dwFlags, dwBufferSize, lpvBuffer, uBufferPhysicalAddress);
        DEBUGMSG(DBG_INTERRUPT, (TEXT("-IssueInterruptTransfer TransferID %lx \n"), uTransfer->GetTransferID()));
        uTransfer->SetTransferHandle(hTransfer);
        pipeMutex.Unlock();
        if(hTransfer == NULL) {
            DEBUGMSG(DBG_INTERRUPT | DBG_WARN, (TEXT("+IssueInterruptTransfer(buffersize %lx,buffer %lx, phAddr %lx) Failed, TransferID %lx \n"), dwBufferSize, lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
            Sleep(RETRY_SLEEP);
        };
    }
    while(hTransfer == NULL && (dwRetry++ < ISSUE_RETRY));

    if(hTransfer == NULL) {
        errorCode = ERROR_USB_PIPE_TRANSFER;
        ASSERT(FALSE);
    } else if((dwFlags & USB_NO_WAIT) == 0)
        ActionCompleteNotify(uTransfer);
    return hTransfer;

}

USB_TRANSFER UsbPipeService::IssueIsochTransfer(UsbTransfer * uTransfer, DWORD dwStartingFrame, DWORD dwFrames, LPCDWORD lpdwLengths, LPVOID lpvBuffer, ULONG uBufferPhysicalAddress)
{
    if(uTransfer == NULL || hPipe == NULL)
        return NULL;
    DWORD dwRetry = 0;
    USB_TRANSFER hTransfer;
    do {
        pipeMutex.Lock();
        DEBUGMSG(DBG_ISOCH, (TEXT("+IssueIsochTransfer(buffer %lx, phAddr %lx) TransferID %lx \n"), lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
        uTransfer->SetPipe(this);
        hTransfer = (*lpUsbFuncs->lpIssueIsochTransfer) (hPipe, lpStartAddress, uTransfer, dwFlags, dwStartingFrame, dwFrames, lpdwLengths, lpvBuffer, uBufferPhysicalAddress);
        DEBUGMSG(DBG_ISOCH, (TEXT("+IssueIsochTransfer startingframe=%d, dwFrames=%d \n"), dwStartingFrame, dwFrames));
        DEBUGMSG(DBG_ISOCH, (TEXT("-IssueIsochTransfer TransferID %lx \n"), uTransfer->GetTransferID()));
        uTransfer->SetTransferHandle(hTransfer);
        pipeMutex.Unlock();
        if(hTransfer == NULL) {
            DEBUGMSG(DBG_ISOCH | DBG_WARN, (TEXT("+IssueIsochTransfer(buffer %lx, phAddr %lx) TransferID %lx, failed, retrying \n"), lpvBuffer, uBufferPhysicalAddress, uTransfer->GetTransferID()));
            Sleep(RETRY_SLEEP);
        };
    }
    while(hTransfer == NULL && (dwRetry++ < ISSUE_RETRY));

    if(hTransfer == NULL) {
        errorCode = ERROR_USB_PIPE_TRANSFER;
        ASSERT(FALSE);
    } else if((dwFlags & USB_NO_WAIT) == 0)
        ActionCompleteNotify(uTransfer);
    return hTransfer;

};

BOOL UsbPipeService::isInPipe()
{
    return USB_ENDPOINT_DIRECTION_IN(DeviceDesc.bEndpointAddress);
}

BOOL UsbPipeService::isOutPipe()
{
    return USB_ENDPOINT_DIRECTION_OUT(DeviceDesc.bEndpointAddress);
}

DWORD WINAPI UsbPipeService::TransferNotify(LPVOID lpvNotifyParameter)
{
    UsbPipeService *pPipeService = (UsbPipeService *) (((UsbTransfer *) lpvNotifyParameter)->GetPipe());
    return pPipeService->ActionCompleteNotify((UsbTransfer *) lpvNotifyParameter);
}

DWORD UsbPipeService::ActionCompleteNotify(UsbTransfer * uTransfer)
{
    uTransfer->CompleteNotify();
    return TRUE;
}

BOOL UsbPipeService::FormatData(LPBYTE pDataBuffer, DWORD dwDataLen)
{
    DWORD dwPacketSize = GetMaxPacketSize();
    ASSERT(dwPacketSize);
    ASSERT(pDataBuffer);
    ASSERT(dwDataLen);
    WORD wSeqNumber = 0;
    if(dwPacketSize >= 4 && (dwPacketSize & 3) == 0) {
        while(dwDataLen >= dwPacketSize) {
            WORD *pwDataBuffer = (WORD *) pDataBuffer;
            *pwDataBuffer = wSeqNumber++;
            *(pwDataBuffer + 1) = 0;
            WORD wCheckSum = 0;
            for(DWORD i = 0; i < dwPacketSize / 2; i++)
                wCheckSum += *(pwDataBuffer + i);
            *(pwDataBuffer + 1) = ~wCheckSum;
            // advance to next packet.
            dwDataLen -= dwPacketSize;
            pDataBuffer += dwPacketSize;
        }
        return TRUE;
    } else
        return FALSE;
}

BOOL UsbPipeService::CheckData(LPBYTE pDataBuffer, DWORD dwDataLen, LPCHECK_RESULT pResult)
{
    DWORD dwPacketSize = GetMaxPacketSize();
    ASSERT(pResult);
    ASSERT(dwPacketSize);
    ASSERT(pDataBuffer);
    ASSERT(dwDataLen);

    WORD wSeqNumber = 0;
    if(dwPacketSize >= 4 && (dwPacketSize & 3) == 0) {
        DWORD dwSeqMax = dwDataLen / dwPacketSize;
        PBYTE pbFlag = (PBYTE) malloc(dwSeqMax);
        if(pbFlag == NULL)    // no memory
            return FALSE;
        memset(pResult, 0, sizeof(CHECK_RESULT));
        memset(pbFlag, 0, dwSeqMax);
        while(dwDataLen >= dwPacketSize) {
            (pResult->dwTotal)++;
            WORD *pwDataBuffer = (WORD *) pDataBuffer;
            WORD wCheckSum = 0;
            for(DWORD i = 0; i < dwPacketSize / 2; i++)
                wCheckSum += *(pwDataBuffer + i);
            if(wCheckSum != (WORD) (-1))    // check sum fail.
                pResult->dwJunk++;
            else {
                if(wSeqNumber > *pwDataBuffer) {
                    pResult->dwSeqError++;
                    wSeqNumber = *(pwDataBuffer);
                } else {
                    wSeqNumber = *pwDataBuffer;
                    if(wSeqNumber < (WORD) dwSeqMax && pbFlag[wSeqNumber] == 0) {
                        pbFlag[wSeqNumber] = 1;
                        pResult->dwSuccess++;
                        wSeqNumber++;
                    } else
                        pResult->dwSeqError++;
                };
            };
            dwDataLen -= dwPacketSize;
            pDataBuffer += dwPacketSize;
        }
        for(DWORD dwIndex = 0; dwIndex < dwSeqMax; dwIndex++) {
            if(pbFlag[dwIndex] == 0)    // un-touched.
                pResult->dwLost++;
        }
        free(pbFlag);
        return TRUE;
    } else
        return FALSE;
}

UsbClientDrv::UsbClientDrv(USB_HANDLE usbHandle, LPCUSB_FUNCS UsbFuncsPtr, LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId, LPCUSB_DRIVER_SETTINGS lpDriverSettings):
UsbClass(usbHandle, UsbFuncsPtr, lpInterface, szUniqueDriverId, lpDriverSettings)
{
    DEBUGMSG(DBG_FUNC, (TEXT("UsbClientDrv")));
    if(lpInterface == NULL)
        return;

    dwNumEndPoints = lpInterface->Descriptor.bNumEndpoints;
    lpEndPoints = (LPUSB_ENDPOINT) malloc(sizeof(USB_ENDPOINT) * dwNumEndPoints);
    if(lpEndPoints == NULL)
        return;
    memcpy((PVOID) lpEndPoints, (PVOID) lpInterface->lpEndpoints, sizeof(USB_ENDPOINT) * dwNumEndPoints);
    DEBUGMSG(DBG_FUNC, (TEXT("End of UsbClientDrv")));

}

LPCUSB_ENDPOINT UsbClientDrv::GetDescriptorPoint(DWORD dwIndex)
{
    if(dwIndex < dwNumEndPoints)
        return (lpEndPoints + dwIndex);
    else
        return NULL;
}

UsbClientDrv::~UsbClientDrv()
{
    if(lpEndPoints)
        free((PVOID) lpEndPoints);
};
