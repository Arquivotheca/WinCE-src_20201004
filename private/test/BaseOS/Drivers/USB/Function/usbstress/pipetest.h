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
    PipeTest.h

Abstract:  
    USB driver Pipe Test.
    
Notes: 

--*/

#ifndef __PIPETEST_H_
#define __PIPETEST_H_
#include "UsbServ.h"
#include "Usbstress.h"
#include "syncobj.h"
#include "MThread.h"
#include <usbdi.h>


#define MAX_ISOCH_BUFFERS 0x400
#define MAX_ENDPOINTS 8 

#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)

class UsbClientDrv;
// define for IsochTransfer Sync delay.
#define TRANSFER_OUT_DELAY 500
#define TRANSFER_IN_DELAY 1000
typedef enum {
    ISSUECONTROLTRANSFER,
    ISSUEBULKTRANSFER,
    ISSUEINTERRUPTTRANSFER,
    ISSUEISOCHTRANSFER,
    ISSUETRANSFER
} PipeCommandType;
typedef struct _PipeCommandBlock {
    DWORD PipeCommandBlockLength;
    PipeCommandType ePipeCommandType;
    DWORD dwFlags;
    DWORD dwStartingFrame, dwFrames;    // for Isochron
    LPCVOID lpvControlHeader;
    DWORD dwBufferSize;
    LPVOID lpvBuffer;
    ULONG uBufferPhysicalAddress;
} PipeCommandBlock, *PPipeCommandBlock, *LPPipeCommandBlock;


class CommandSyncPipe:public UsbPipeService {
      public:
    CommandSyncPipe(UsbClientDrv * usbClientDriver, LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor):pDriverPtr(usbClientDriver), UsbPipeService(usbClientDriver->lpUsbFuncs, usbClientDriver->GetUsbHandle(), lpEndPointDescriptor) {
        errorCode = 0;
    };
    BOOL SetEndpoint(BOOL bStalled);
    BOOL IssueCommand(PUSB_DEVICE_REQUEST pControlHead, LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr);
    virtual DWORD GetErrorCode() {
        return (errorCode != 0 ? errorCode : GetPipeLastError());
    };
      private:
    DWORD errorCode;
    UsbClientDrv *pDriverPtr;
    int iDevID;
    TCHAR szCEDeviceName[10];
};


class SyncPipeThread:public MiniThread, public UsbPipeService {
      public:
      SyncPipeThread(UsbClientDrv * usbClientDriver, LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor, int iID, LPVOID lpBuffer, DWORD dwBufferSize, DWORD dwBlockSize, CSemaphore * pSync = NULL, DWORD dwPhysicalAddr = 0):
    MiniThread(0, THREAD_PRIORITY_NORMAL, TRUE),
            pDriverPtr(usbClientDriver),
            UsbPipeService(usbClientDriver->lpUsbFuncs, usbClientDriver->GetUsbHandle(), lpEndPointDescriptor),
            lpDataBuffer(lpBuffer),
            dwDataSize(dwBufferSize),
            m_dwBlockSize(dwBlockSize),
            m_pSync(pSync),
            iDevID(iID),
            m_dwPhysicalAddr(dwPhysicalAddr)
    {
        DEBUGMSG(DBG_FUNC, (TEXT("SyncPipeThread addr@%lx length=%lx"), lpDataBuffer, dwBufferSize));
        UCHAR uAddr = (lpEndPointDescriptor != NULL) ? lpEndPointDescriptor->bEndpointAddress : 0xFF;
        _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d (Pipe0x%x):"), iID, uAddr);

         m_bShortStress = FALSE;
         m_bUseLengthArray = FALSE;
         m_pusTransLens = NULL;
         errorCode = 0;
    };
    BOOL SetEndpoint(BOOL bStalled);
    virtual BOOL SubmitRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr);
    virtual BOOL SubmitIsochRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, PDWORD pdwLengths);
    virtual DWORD GetErrorCode() {
        return (errorCode != 0 ? errorCode : GetPipeLastError());
    };
    VOID SetBlockSize(DWORD dwSize) {
        m_dwBlockSize = dwSize;
    };
    VOID SetShortStress(BOOL bSS) {
        m_bShortStress = bSS;
    };
    //only used for short transfer test
    VOID SetShortBlockSize(DWORD dwSize) {
        m_dwBlockSize = dwSize;
        dwDataSize = dwSize * 15;
    };
    VOID SetTransLens(BOOL bUseLenArr, PUSHORT pusLenArr) {
        m_bUseLengthArray = bUseLenArr;
        if(bUseLenArr == FALSE)
            m_pusTransLens = NULL;
        if(bUseLenArr == TRUE && pusLenArr == NULL) {
            m_pusTransLens = NULL;
            m_bUseLengthArray = FALSE;
        } else
            m_pusTransLens = pusLenArr;
    };

      private:
    virtual DWORD ThreadRun();
    VOID RunShortStress();
    VOID RunwithLengthsArray();
    BOOL ValidateLengthsArray();
    LPVOID lpDataBuffer;
    DWORD m_dwPhysicalAddr;
    DWORD dwDataSize;
    DWORD m_dwBlockSize;
    DWORD m_dwCurrentSyncFrame;
    CSemaphore *m_pSync;
    DWORD errorCode;
    UsbClientDrv *pDriverPtr;
    int iDevID;
    TCHAR szCEDeviceName[20];
    BOOL m_bShortStress;
    BOOL m_bUseLengthArray;
    PUSHORT m_pusTransLens;
};
class AsyncTransFIFO;

#define MAX_FIFO_SIZE 0x40
class AsyncTransFIFO:public MiniThread {
      public:
    AsyncTransFIFO(LPCUSB_FUNCS lpUsbFuncs, DWORD dwFifoSize = MAX_FIFO_SIZE);
    ~AsyncTransFIFO();

    BOOL IsEmpty() {
        return (dwOutput == dwInput);
    };
    BOOL IsFull() {
        return (NextPosition(dwInput) == dwOutput);
    };
    BOOL IsSignalEmpty() {
        return (dwSignal == dwInput);
    };
    BOOL IsHeadSignaled() {
        return (dwOutput != dwSignal);
    };
    DWORD CountTransferFromUnsignaled(UsbTransfer * pTransfer);
    void WaitAndLockPositionToken() {
        cSemaphore->Lock();
    };
    void ReleasePositionToken() {
        cSemaphore->Unlock();
    };
    BOOL AddTransferHandle(UsbTransfer * pTransfer);
    UsbTransfer *RemoveTransferHandle();
    BOOL SignalAndAdvance();
    UsbTransfer *FirstTransferHandle() {
        UsbTransfer *lpTransRet = NULL;
        cMutex.Lock(10000);

        if(!IsEmpty())
            lpTransRet = TransferArray[dwOutput];

        cMutex.Unlock();
        return lpTransRet;
    };
    DWORD GetErrorCode() {
        return errorCode;
    };
    UsbPipeService *SetPipe(UsbPipeService * pPipe) {
        return m_pPipe = pPipe;
    };
    UsbPipeService *GetPipe() {
        return m_pPipe;
    };
      private:
    LPCUSB_FUNCS m_lpUsbFuncs;
    UsbPipeService *m_pPipe;
    CMutex cMutex;
    CSemaphore *cSemaphore;
    DWORD dwArraySize;
    UsbTransfer *TransferArray[MAX_FIFO_SIZE];
    DWORD dwOutput, dwInput, dwSignal;
    DWORD dwNumTrans;

    TCHAR szCEDeviceName[10];
    DWORD NextPosition(DWORD Position) {
        if(++Position >= dwArraySize)
            Position = 0;
        return Position;
    };

    virtual DWORD ThreadRun();
    DWORD errorCode;
};

class AsyncUsbPipeService:public UsbPipeService {
      public:
    AsyncUsbPipeService(UsbClientDrv * usbClientDrv, LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor, int iID):UsbPipeService(usbClientDrv->lpUsbFuncs, usbClientDrv->GetUsbHandle(), lpEndPointDescriptor, TRUE), pDriverPtr(usbClientDrv), transStorage(usbClientDrv->lpUsbFuncs, 0x40)
    {
        DEBUGMSG(DBG_FUNC, (TEXT("+ASyncPipeThread")));
        iDevID = iID;
        UCHAR uAddr = (lpEndPointDescriptor != NULL) ? lpEndPointDescriptor->bEndpointAddress : 0xFF;
        _stprintf_s(szCEDeviceName, _countof(szCEDeviceName), _T("Dev%2d (Pipe0x%x):"), iDevID, uAddr);
         pdwLengths = NULL;
        if(GetEndPointAttribute() == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            pdwLengths = new DWORD[MAX_ISOCH_BUFFERS];
            if(pdwLengths == NULL)
                DEBUGMSG(DBG_ERR, (TEXT("Fail at Isoch Transfer, new returned NULL for pdwLengths")));
        }
        transStorage.SetPipe(this);
        transStorage.ThreadStart();
    };
    ~AsyncUsbPipeService() {
        if(!transStorage.ThreadTerminated(30000)) {
            transStorage.ForceTerminated();
        }
        if(pdwLengths != NULL)
            delete[]pdwLengths;
    };
    virtual DWORD ActionCompleteNotify(UsbTransfer * pTransfer);
    virtual BOOL SubmitRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, BOOL fShortOK = FALSE);
    virtual BOOL SubmitIsochRequest(LPVOID lpBuffer, DWORD dwSize, DWORD dwPhysicalAddr, PDWORD pdwLengths);
    BOOL WaitForComplete(DWORD dwTicks) {
        DWORD dwStartTicks = GetTickCount();

        while(GetTickCount() - dwStartTicks <= dwTicks) {
            if(transStorage.IsEmpty())
                return TRUE;
            Sleep(100);
        }
        if(transStorage.IsEmpty())
            return TRUE;
        else {
            DEBUGMSG(DBG_DETAIL, (TEXT("WaitForComplete Time Out")));
            return FALSE;
        }
    }
    DWORD GetErrorCode() {
        return transStorage.GetErrorCode();
    }
    LPTSTR GetDeviceString() {
        return szCEDeviceName;
    }
    DWORD m_dwCurrentSyncFrame;
      private:
    UsbClientDrv * pDriverPtr;
    AsyncTransFIFO transStorage;
    int iDevID;
    TCHAR szCEDeviceName[20];
      protected:
    PDWORD pdwLengths;
};


class AsyncPipeThread:public MiniThread, public AsyncUsbPipeService {
      public:
      AsyncPipeThread(UsbClientDrv * usbClientDrv, LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor, int iID, LPVOID lpBuffer, DWORD dwBufferSize, DWORD dwBlockSize, CSemaphore * pSync = NULL, DWORD dwPhysicalAddr = 0):MiniThread(0, THREAD_PRIORITY_NORMAL, TRUE),
        AsyncUsbPipeService(usbClientDrv, lpEndPointDescriptor, iID),
        pDriverPtr(usbClientDrv), lpDataBuffer(lpBuffer), dwDataSize(dwBufferSize), m_dwBlockSize(dwBlockSize), m_pSync(pSync), m_dwPhysicalAddr(dwPhysicalAddr), iDevID(iID) {
        DEBUGMSG(DBG_FUNC, (TEXT("AsyncPipeThread addr@%lx length=%lx"), lpBuffer, dwBufferSize));
        szCEDeviceName = GetDeviceString();
        errorCode = 0;
        m_bUseLengthArray = FALSE;
        m_pusTransLens = NULL;
    };
    virtual DWORD GetErrorCode() {
        if(errorCode)
            return errorCode;
        else
            return AsyncUsbPipeService::GetErrorCode();
    };
    BOOL SetEndpoint(BOOL bStalled);
    virtual DWORD ActionCompleteNotify(UsbTransfer * pTransfer);
    DWORD CallThreadRun() {
        return AsyncPipeThread::ThreadRun();
    };
    VOID SetBlockSize(DWORD dwSize) {
        m_dwBlockSize = dwSize;
    };
    //only used for short transfer test
    VOID SetShortBlockSize(DWORD dwSize) {
        m_dwBlockSize = dwSize;
        dwDataSize = dwSize * 15;
    };
    VOID SetTransLens(BOOL bUseLenArr, PUSHORT pusLenArr) {
        m_bUseLengthArray = bUseLenArr;
        if(bUseLenArr == FALSE)
            m_pusTransLens = NULL;
        if(bUseLenArr == TRUE && pusLenArr == NULL) {
            m_pusTransLens = NULL;
            m_bUseLengthArray = FALSE;
        } else
            m_pusTransLens = pusLenArr;
    };

      private:
    virtual DWORD ThreadRun();
    VOID RunwithLengthsArray();
    BOOL ValidateLengthsArray();
    LPVOID lpDataBuffer;
    DWORD dwDataSize;
    DWORD m_dwPhysicalAddr;
    DWORD m_dwBlockSize;
    CSemaphore *m_pSync;
    DWORD errorCode;
    UsbClientDrv *pDriverPtr;
    int iDevID;
    LPTSTR szCEDeviceName;
    BOOL m_bUseLengthArray;
    PUSHORT m_pusTransLens;
};

typedef DWORD (* LPUN_STRESS_TRANSFER)(UsbClientDrv *, int, DWORD, DWORD,BOOL*);

class StressTransfer : public MiniThread {
    public:
        StressTransfer(LPUN_STRESS_TRANSFER _sTransferFunc, UsbClientDrv * _usbClientDriver, int _iID, DWORD _dwSeed, DWORD _dwTransLen)
            :MiniThread(0, THREAD_PRIORITY_NORMAL, TRUE),sTransferFunc(_sTransferFunc),    pDriverPtr(_usbClientDriver),iID(_iID),dwSeed(_dwSeed),dwDataTransfer(_dwTransLen) {
            hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
        };
        ~StressTransfer() {
            if(!INVALID_HANDLE(hEvent))
                CloseHandle(hEvent);
        }

        DWORD dwLastExit;
        HANDLE hEvent;
                    
    private:
        virtual DWORD ThreadRun();
        UsbClientDrv *pDriverPtr;
        int iID;
        DWORD dwSeed;
        DWORD dwDataTransfer;
        LPUN_STRESS_TRANSFER sTransferFunc;
};


extern DWORD PipeBlockTransfer(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID, DWORD dwtotalLength);
extern BOOL PipeNormalPhyMemory(UsbClientDrv * pDriverPtr, PPhysMemNode PMemN, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID);
extern BOOL PipeAlignPhyMemory(UsbClientDrv * pDriverPtr, PPhysMemNode PMemN, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID);
extern DWORD PipeVaryLenTransfer(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pOutEP, LPCUSB_ENDPOINT pInEP, BOOL fAsync, int iID, WORD wNumPackets, DWORD dwSeed, PDWORD pdwDataTransferred);
extern DWORD EP0Transfer(UsbClientDrv * pDriverPtr, BOOL fStall, DWORD dwNumPackets, DWORD dwSeed, PDWORD pdwDataTransferred);
extern DWORD Serial_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer ,BOOL* pfTerminated  );
extern DWORD RNDIS_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer ,BOOL* pfTerminated);
extern DWORD Storage_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer,BOOL* pFTerminated);
extern DWORD Overload_Transfer(UsbClientDrv * pDriverPtr, int iID, DWORD dwSeed, DWORD dwDataTransfer,BOOL* pFTerminated);

#endif
