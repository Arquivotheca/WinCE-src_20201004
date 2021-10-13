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
    usbclass.h

Abstract:
    USB driver access class.

Notes:

--*/
#ifndef __USBCLASS_H_
#define __USBCLASS_H_

#include <usbdi.h>
#include "syncobj.h"
#include "usbperf.h"

#define DEFAULT_USB_DRIVER TEXT("USBD.DLL")

class UsbClientDrv;

class UsbDriverArray : public CCriticalSection {
public:
    UsbDriverArray(BOOL bAutoDelete,DWORD arraySize=MAX_USB_CLIENT_DRIVER);
    ~UsbDriverArray();
    UsbClientDrv * operator[](int nIndex) const;
    UsbDriverArray& operator=(UsbClientDrv * oneDriver);
    BOOL AddClientDrv(UsbClientDrv * );
    BOOL RemoveClientDrv(UsbClientDrv *,BOOL bDelete );
    UsbClientDrv * GetAt(int nIndex) const;
    DWORD GetArraySize() { return dwArraySize; };
    DWORD GetCurAvailDevs() {return dwCurDevs; };
    BOOL IsEmpty();
    BOOL IsContainClientDrv(UsbClientDrv * pClientDriver);
private:
    UsbClientDrv ** arrayUsbClientDrv;
    BOOL autoDelete;
    DWORD dwArraySize;
       DWORD dwCurDevs;

};

class USBDriverClass : public UsbDriverArray{
public:
    USBDriverClass(BOOL bAutoDelete=TRUE) ;
    USBDriverClass(LPCTSTR lpDrvName,BOOL bAutoDelete=TRUE);
    ~USBDriverClass() { FreeLibrary(hInst);};

    //access function
    VOID GetUSBDVersion(LPDWORD lpdwMajorVersion, LPDWORD lpdwMinorVersion);
    BOOL RegisterClientDriverID(LPCWSTR szUniqueDriverId);
    BOOL UnRegisterClientDriverID(LPCWSTR szUniqueDriverId);
    BOOL RegisterClientSettings(LPCWSTR lpszDriverLibFile,
                            LPCWSTR lpszUniqueDriverId, LPCWSTR szReserved,
                            LPCUSB_DRIVER_SETTINGS lpDriverSettings);
    BOOL UnRegisterClientSettings(LPCWSTR lpszUniqueDriverId, LPCWSTR szReserved,
                              LPCUSB_DRIVER_SETTINGS lpDriverSettings);
    HKEY OpenClientRegistryKey(LPCWSTR szUniqueDriverId);

    BOOL IsOk() { return(!UsbDriverClassError);};
private:
    BOOL CreateUsbAccessHandle(HINSTANCE hInst);
    LPOPEN_CLIENT_REGISTRY_KEY          lpOpenClientRegistyKey;
    LPREGISTER_CLIENT_DRIVER_ID         lpRegisterClientDriverID;
    LPUN_REGISTER_CLIENT_DRIVER_ID      lpUnRegisterClientDriverID;
    LPREGISTER_CLIENT_SETTINGS          lpRegisterClientSettings;
    LPUN_REGISTER_CLIENT_SETTINGS       lpUnRegisterClientSettings;
    LPGET_USBD_VERSION                  lpGetUSBDVersion;
    BOOL UsbDriverClassError;
    HINSTANCE hInst;
};

extern USBDriverClass *pUsbDriver;
extern const   TCHAR   gcszTestDriverId[];

extern "C" void TRACE(LPCTSTR szFormat, ...);

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
    LPCUSB_INTERFACE GetInterCurface() { return lpInterface; };
    LPCWSTR GetUniqueDriverId() { return szUniqueDriverId; };

    void debug(LPCTSTR szFormat, ...);

private:
    LPUSB_INTERFACE lpInterface;
    LPWSTR szUniqueDriverId;

};

#define MAX_ISOCH_FRAME_LENGTH (0x400*3)
#define LIMITED_UHCI_BLOCK 0x400//64


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

    BOOL DisableDevice(BOOL fReset, BYTE bInterfaceNumber) {return (*lpUsbFuncs->lpDisableDevice)(hUsb, fReset, bInterfaceNumber);}


    // Gets info on a device
    LPCUSB_DEVICE GetDeviceInfo();

    //Device commands
    USB_TRANSFER IssueVendorTransfer(LPTRANSFER_NOTIFY_ROUTINE,LPVOID,DWORD dwFlags,
        LPCUSB_DEVICE_REQUEST lpControlHeader, LPVOID lpvBuffer,ULONG uBufferPhusicalAddress);
    USB_TRANSFER GetInterface(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, PUCHAR);
    USB_TRANSFER SetInterface(LPTRANSFER_NOTIFY_ROUTINE,LPVOID,DWORD, UCHAR, UCHAR);
    USB_TRANSFER GetDescriptor(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, UCHAR, WORD,
                                          WORD, LPVOID);
    USB_TRANSFER SetDescriptor(LPTRANSFER_NOTIFY_ROUTINE,LPVOID, DWORD, UCHAR, UCHAR, WORD,
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

private:
    USB_HANDLE hUsb;
    USB_ENDPOINT_DESCRIPTOR DefaultDeviceDesc;
    DWORD m_dwSyncFrameNumber;


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
