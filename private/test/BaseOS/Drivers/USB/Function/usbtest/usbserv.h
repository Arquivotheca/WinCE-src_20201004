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
    usbserv.h

Abstract:  
    USB driver service class.
    
Notes: 

--*/

#ifndef __USBSERV_H_
#define __USBSERV_H_
#include <usbdi.h>
#include "syncobj.h"
#include "usbclass.h"
#define countof(x) (sizeof(x)/sizeof(*(x)))

typedef struct _CHECK_RESULT {
      DWORD dwTotal;
      DWORD dwSuccess;
      DWORD dwLost;
      DWORD dwJunk;
      DWORD dwSeqError;
} CHECK_RESULT, *LPCHECK_RESULT;





class UsbBasic {
public:
      UsbBasic(LPCUSB_FUNCS UsbFuncsPtr,LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId,
                     LPCUSB_DRIVER_SETTINGS lpDriverSettings);
      ~UsbBasic();
      VOID GetUSBDVersion(LPDWORD lpdwMajorVersion, LPDWORD lpdwMinorVersion);
//Helper commands
    BOOL TranslateStringDesc(LPCUSB_STRING_DESCRIPTOR, LPWSTR,DWORD);
    LPCUSB_INTERFACE FindInterface(LPCUSB_DEVICE, UCHAR, UCHAR);
// attribute
    LPCUSB_FUNCS lpUsbFuncs;
      HKEY OpenClientRegistryKey();
      LPCUSB_FUNCS GetUsbFuncsTable(){ return lpUsbFuncs; };
      LPCUSB_INTERFACE GetInterCurface() { return lpInterface; };
      LPCWSTR GetUniqueDriverId() { return szUniqueDriverId; };

      void debug(LPCTSTR szFormat, ...);

private:
      LPUSB_INTERFACE lpInterface;
      LPWSTR szUniqueDriverId;

};

#define MAX_ISOCH_FRAME_LENGTH (0x400*3)
#define LIMITED_UHCI_BLOCK 0x400//64

class UsbTransfer {
public:
      UsbTransfer(USB_TRANSFER usbTransfer,LPCUSB_FUNCS lpUsbFuncs,
                  LPVOID lpBuffer=NULL,DWORD dwSize=0,DWORD dwPhysicalAddr=0) ;
      virtual ~UsbTransfer();
// get info on Transfers
      BOOL IsTransferComplete ();
      BOOL IsCompleteNoError();
      BOOL GetTransferStatus ( LPDWORD, LPDWORD);
      BOOL GetIsochResults(DWORD, LPDWORD, LPDWORD);
// transfer maniuplators
      BOOL AbortTransfer(DWORD dwFlags=0);
      BOOL CloseTransfer();
//
      VOID ReInitial(USB_TRANSFER usbTransfer,
                        LPVOID lpBuffer=NULL,DWORD dwSize=0,DWORD dwPhysicalAddr=0) ;
      USB_TRANSFER GetTransferHandle() { return hUsbTransfer; };
      USB_TRANSFER SetTransferHandle(USB_TRANSFER usbHandle){ return (hUsbTransfer=usbHandle); }
      virtual void CompleteNotify(){ 
            transferEvent.SetEvent();
      };
      virtual BOOL WaitForTransferComplete(DWORD dwTicks=INFINITE) {
            return transferEvent.Lock(dwTicks);
      };
      LPVOID SetPipe(LPVOID pPoint) { return (lpPipePoint=pPoint);};
      LPVOID GetPipe() { return lpPipePoint; };
      DWORD GetTransferID() { return dwID; };
      LPCUSB_FUNCS lpUsbFuncs;
      // Debug Function
      LPVOID GetAttachedBuffer() { return lpAttachedBuffer; };
      DWORD  GetAttachedSize() { return dwAttachedSize; };
      DWORD  GetAttachedPhysicalAddr() { return dwAttachedPhysicalAddr; };
private:
      CEvent transferEvent;
      USB_TRANSFER hUsbTransfer;
      LPVOID lpPipePoint;
      DWORD dwID;
      static DWORD dwCurrentID;
      LPVOID lpAttachedBuffer;
      DWORD dwAttachedSize;
      DWORD dwAttachedPhysicalAddr;
};

// define for transfer callback

typedef struct _TransStatus {
      USB_TRANSFER hTransfer;
      LPCUSB_FUNCS lpUsbFuncs;
      BOOL IsFinished;
      CEvent cEvent;
} TransStatus,*PTransStatus;

extern DWORD WINAPI TransferNotify(LPVOID lpvNotifyParameter);


class UsbClass : public UsbBasic {
public:
      UsbClass(USB_HANDLE usbHandle,LPCUSB_FUNCS UsbFuncsPtr,
                  LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId,
            LPCUSB_DRIVER_SETTINGS lpDriverSettings);
      ~UsbClass();

//USB Subsystem Commands
      BOOL GetFrameNumber(LPDWORD);
      BOOL GetFrameLength(LPUSHORT);

//Enables Device to Adjust the USB SOF period on OHCI or UHCI cards
      BOOL TakeFrameLengthControl();
      BOOL SetFrameLength(HANDLE, USHORT);
      BOOL ReleaseFrameLengthControl();

      BOOL DisableDevice(BOOL, BYTE); 
      BOOL SuspendDevice(BYTE);
      BOOL ResumeDevice(BYTE);


// Gets info on a device
      LPCUSB_DEVICE GetDeviceInfo();

//For Testing purpose only
      USB_HANDLE getUsbHandle(){return hUsb;};

//Device commands
      USB_TRANSFER IssueVendorTransfer(LPTRANSFER_NOTIFY_ROUTINE,LPVOID,DWORD dwFlags,
            LPCUSB_DEVICE_REQUEST lpControlHeader, LPVOID lpvBuffer,ULONG uBufferPhusicalAddress);
      USB_TRANSFER GetInterface(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, PUCHAR);
      USB_TRANSFER SetInterface(LPTRANSFER_NOTIFY_ROUTINE,LPVOID,DWORD, UCHAR, UCHAR);
      USB_TRANSFER GetDescriptor(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, UCHAR, WORD,
                                          WORD, LPVOID);
      USB_TRANSFER SetDescriptor(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, UCHAR, WORD,
                                          WORD, PVOID);
      USB_TRANSFER SetConfig(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, UCHAR, WORD,
                                          WORD, PVOID);
          USB_TRANSFER SetFeature(LPTRANSFER_NOTIFY_ROUTINE,LPVOID,DWORD, WORD, UCHAR);
      USB_TRANSFER ClearFeature(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, WORD, UCHAR);
      USB_TRANSFER GetStatus(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, LPWORD);
      USB_TRANSFER SyncFrame(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, LPWORD);

      // attribute of default pipe.
      USHORT GetMaxPacketSize( ) { return DefaultDeviceDesc.wMaxPacketSize; };
      UCHAR GetEndPointAttribute () { return (DefaultDeviceDesc.bmAttributes & USB_ENDPOINT_TYPE_MASK);};
      USB_HANDLE GetUsbHandle() const { return hUsb; };
// default pipe function
      BOOL ResetDefaultPipe();
      BOOL IsDefaultPipeHalted(LPBOOL lpHalted) ;
      BOOL SetSyncFramNumber() { return GetFrameNumber(&m_dwSyncFrameNumber); };
      DWORD GetSyncFramNumber() { return m_dwSyncFrameNumber; };
      USHORT  GetEP0PacketSize();

private:
      USB_HANDLE hUsb;
      USB_ENDPOINT_DESCRIPTOR DefaultDeviceDesc;
      DWORD m_dwSyncFrameNumber;
      
      
};

class UsbPipeService {
public:
      UsbPipeService(LPCUSB_FUNCS ptrUsbFuncs,USB_HANDLE usbHandle,LPCUSB_ENDPOINT_DESCRIPTOR lpEndPointDescriptor,BOOL Async=FALSE);
      ~UsbPipeService();
//pipe handling
//typedef BOOL (* LPABORT_PIPE_TRANSFERS)(USB_PIPE, DWORD);
      BOOL AbortPipeTransfers(DWORD dwFlags);
//typedef BOOL (* LPRESET_PIPE)(USB_PIPE);
      BOOL ResetPipe();
//typedef BOOL (* LPIS_PIPE_HALTED)(USB_PIPE, LPBOOL);
      BOOL IsPipeHalted(LPBOOL lpbHalted);

// It used by USBD

//Transfer commands
//typedef USB_TRANSFER (* LPISSUE_CONTROL_TRANSFER)(USB_PIPE,
//                                                  LPTRANSFER_NOTIFY_ROUTINE,
//                                                  LPVOID, DWORD, LPCVOID, DWORD,
//                                                  LPVOID, ULONG);
      virtual USB_TRANSFER IssueControlTransfer(UsbTransfer * uTransfer, LPCVOID lpvControlHeader,DWORD dwBufferSize,
            LPVOID lpvBuffer,ULONG uBufferPhysicalAddress);
//typedef USB_TRANSFER (* LPISSUE_BULK_TRANSFER)(USB_PIPE,
//                                               LPTRANSFER_NOTIFY_ROUTINE,
//                                               LPVOID, DWORD, DWORD, LPVOID,
//                                               ULONG);
      virtual USB_TRANSFER IssueBulkTransfer(UsbTransfer * uTransfer, DWORD dwBufferSize, LPVOID lpvBuffer,
            ULONG uBufferPhysicalAddress);

//typedef USB_TRANSFER (* LPISSUE_INTERRUPT_TRANSFER)(USB_PIPE,
//                                                    LPTRANSFER_NOTIFY_ROUTINE,
//                                                    LPVOID, DWORD, DWORD,
//                                                    LPVOID, ULONG);
      virtual USB_TRANSFER IssueInterruptTransfer(UsbTransfer * uTransfer, DWORD dwBufferSize, LPVOID lpvBuffer,
            ULONG uBufferPhysicalAddress);
 
//typedef USB_TRANSFER (* LPISSUE_ISOCH_TRANSFER)(USB_PIPE,
//                                                LPTRANSFER_NOTIFY_ROUTINE,
//                                                LPVOID, DWORD, DWORD, DWORD,
//                                                LPCDWORD, LPVOID, ULONG);
      virtual USB_TRANSFER IssueIsochTransfer(UsbTransfer * uTransfer, DWORD dwStartingFrame,DWORD dwFrames,
            LPCDWORD lpdwLengths,LPVOID lpvBuffer,
            ULONG uBufferPhysicalAddress);

//
      virtual DWORD ActionCompleteNotify(UsbTransfer *pTransfer);

public:
      DWORD GetPipeLastError() { return errorCode; };
      USB_PIPE GetPipeHandle() { return hPipe; };
      UCHAR GetEndPointAttribute () { return (DeviceDesc.bmAttributes & USB_ENDPOINT_TYPE_MASK);};
      USHORT GetMaxPacketSize( ) { return DeviceDesc.wMaxPacketSize; };
      BOOL isInPipe();
      BOOL isOutPipe();
      DWORD GetFlags() { return dwFlags; };
      DWORD SetFlags(DWORD newFlags) { return (dwFlags=newFlags); };
      DWORD SetErrorCode(DWORD aError) { return errorCode=aError; };
      LPCUSB_FUNCS GetUsbFuncPtr() const { return lpUsbFuncs; };
      LPUSB_ENDPOINT_DESCRIPTOR GetPipeDescriptorPtr () { return &DeviceDesc ; };
      BOOL FormatData(LPBYTE pDataBuffer, DWORD dwDataLen);
      BOOL CheckData(LPBYTE pDataBuffer, DWORD dwDataLen, LPCHECK_RESULT pResult);
      CMutex pipeMutex;
private:
      static DWORD WINAPI TransferNotify(LPVOID lpvNotifyParameter);
      USB_PIPE hPipe;
      DWORD dwFlags;
      LPCUSB_FUNCS lpUsbFuncs;
      DWORD errorCode;
      USB_ENDPOINT_DESCRIPTOR DeviceDesc;
      LPTRANSFER_NOTIFY_ROUTINE lpStartAddress;


};



#define MAX_PIPE 0x10
class UsbClientDrv : public UsbClass, public CMutex {
public:
      UsbClientDrv(USB_HANDLE usbHandle,LPCUSB_FUNCS UsbFuncsPtr,
                  LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId,
            LPCUSB_DRIVER_SETTINGS lpDriverSettings);
      virtual ~UsbClientDrv();
      virtual BOOL Initialization() { return TRUE; };
      LPCUSB_ENDPOINT GetDescriptorPoint(DWORD dwIndex);
      DWORD GetNumOfEndPoints() { return dwNumEndPoints; };

      //Class Service
private:
      DWORD dwNumEndPoints;
      LPUSB_ENDPOINT lpEndPoints;
};

#endif


