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
    usbclass.cpp

Abstract:
    USB driver access class.

Author:
    davli

Modified:
    weichen

Functions:

Notes:

--*/
#define __THIS_FILE__   TEXT("UsbClass.cpp")

#include <windows.h>
#include "usbclass.h"
#include "usbperf.h"


extern "C"
void TRACE(LPCTSTR szFormat, ...) {
#ifdef DEBUG
    TCHAR szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer,_countof(szBuffer),_countof(szBuffer)-1,szFormat,pArgs);
    szBuffer[1023] = (TCHAR)0;
    va_end(pArgs);

    DEBUGMSG(1,(TEXT("USBTDRV:%s\n"),szBuffer));
#else
    UNREFERENCED_PARAMETER(szFormat);
#endif
}



UsbDriverArray::UsbDriverArray(BOOL bAutoDelete,DWORD arraySize) : autoDelete(bAutoDelete)
{
    TRACE(TEXT("UsbDriverArray Initial"));
    if (arraySize<5)
        arraySize=5;
    arrayUsbClientDrv= (UsbClientDrv **)malloc(sizeof(UsbClientDrv *)*arraySize);
    TRACE(TEXT("UsbDriverArray Initial addr=%lx,length=%ld"),arrayUsbClientDrv,arraySize);
    ASSERT(arrayUsbClientDrv);
    memset(arrayUsbClientDrv,0,sizeof(UsbClientDrv *)*arraySize);
    dwArraySize=arraySize;
    dwCurDevs = 0;
};
UsbDriverArray::~UsbDriverArray()
{
    if (autoDelete) {
        for (int i=0;i<(int)dwArraySize;i++) {
            if (arrayUsbClientDrv[i]!=NULL)
                delete arrayUsbClientDrv[i];
            arrayUsbClientDrv[i]=NULL;
        };
    }
    free(arrayUsbClientDrv);
}
UsbClientDrv * UsbDriverArray::operator[](int nIndex) const
{
    return GetAt(nIndex);
}
UsbDriverArray& UsbDriverArray::operator=(UsbClientDrv * oneDriver)
{
    AddClientDrv(oneDriver);
    return *this;
}

BOOL UsbDriverArray::AddClientDrv(UsbClientDrv * oneDriver)
{
    TRACE(TEXT("AddClientDrv addr=%lx,length=%ld"),arrayUsbClientDrv,dwArraySize);
    ASSERT(oneDriver);
    Lock();
    for (int i=0;i<(int)dwArraySize;i++){
        if (*(arrayUsbClientDrv+i)==NULL)
            break;
    };
    TRACE(TEXT("AddClientDrv position=%d"),i);
    BOOL bReturn;
    if ((DWORD)i<dwArraySize) {
        *(arrayUsbClientDrv+i)=oneDriver;
        TRACE(TEXT("AddClientDrv return success"));
        bReturn=TRUE;
        dwCurDevs ++;
    }
    else {
        TRACE(TEXT("AddClientDrv return failure"));
        bReturn=FALSE;
    };
    Unlock();
    return bReturn;
}
BOOL UsbDriverArray::RemoveClientDrv(UsbClientDrv * pClientDriver,BOOL bDelete)
{
    if (pClientDriver==NULL)
        return FALSE;
    Lock();
    for (int i=0;i<(int)dwArraySize;i++)
        if (arrayUsbClientDrv[i]==pClientDriver)
            break;
    BOOL bReturn;
    if ((DWORD)i<dwArraySize) {
        arrayUsbClientDrv[i]=NULL;
        if (bDelete)
            delete pClientDriver;
        bReturn=TRUE;
        dwCurDevs --;
    }
    else
        bReturn=FALSE;
    Unlock();
    return bReturn;
}
BOOL UsbDriverArray::IsContainClientDrv(UsbClientDrv * pClientDriver)
{
    for (int i=0;i<(int)dwArraySize;i++)
        if (arrayUsbClientDrv[i]==pClientDriver)
            break;
    BOOL bReturn=(i<(int)dwArraySize);
    return bReturn;
}
BOOL UsbDriverArray::IsEmpty()
{
    Lock();
    for (int i=0;i<(int)dwArraySize;i++)
        if (arrayUsbClientDrv[i]!=NULL)
            break;
    Unlock();
    if ((DWORD)i<dwArraySize)
        return TRUE;
    else
        return FALSE;
}

UsbClientDrv * UsbDriverArray::GetAt(int nIndex) const
{
    if ((DWORD)nIndex<dwArraySize)
        return arrayUsbClientDrv[nIndex];
    else
        return NULL;
}

BOOL USBDriverClass::CreateUsbAccessHandle(HINSTANCE hInst)
{
    lpOpenClientRegistyKey=(LPOPEN_CLIENT_REGISTRY_KEY)GetProcAddress(hInst,TEXT("OpenClientRegistryKey"));
    lpRegisterClientDriverID=(LPREGISTER_CLIENT_DRIVER_ID)GetProcAddress(hInst,TEXT("RegisterClientDriverID"));
    lpUnRegisterClientDriverID=(LPUN_REGISTER_CLIENT_DRIVER_ID)GetProcAddress(hInst,TEXT("UnRegisterClientDriverID"));
    lpRegisterClientSettings=(LPREGISTER_CLIENT_SETTINGS)GetProcAddress(hInst,TEXT("RegisterClientSettings"));
    lpUnRegisterClientSettings=(LPUN_REGISTER_CLIENT_SETTINGS)GetProcAddress(hInst,TEXT("UnRegisterClientSettings"));
    lpGetUSBDVersion=(LPGET_USBD_VERSION)GetProcAddress(hInst,TEXT("GetUSBDVersion"));
    if (lpOpenClientRegistyKey &&
            lpRegisterClientDriverID &&
            lpUnRegisterClientDriverID &&
            lpRegisterClientSettings &&
            lpUnRegisterClientSettings &&
            lpGetUSBDVersion ) {
        UsbDriverClassError=FALSE;
        return TRUE;
    }
    else {
        UsbDriverClassError=TRUE;
        TRACE(TEXT(" Usb Liberary Load Error"));
        return FALSE;
    };
};
USBDriverClass::USBDriverClass(BOOL bAutoDelete) :UsbDriverArray(bAutoDelete)
{
    TRACE(TEXT("USBDriverClass Initial"));
    CreateUsbAccessHandle(hInst=LoadLibrary(DEFAULT_USB_DRIVER));
}
USBDriverClass::USBDriverClass(LPCTSTR lpDrvName,BOOL bAutoDelete):UsbDriverArray(bAutoDelete)
{
    TRACE(TEXT("USBDriverClass Initial"));
    CreateUsbAccessHandle(hInst=LoadLibrary(lpDrvName));
}
VOID USBDriverClass::GetUSBDVersion(LPDWORD lpdwMajorVersion, LPDWORD lpdwMinorVersion)
{
    if(lpGetUSBDVersion == NULL)
        return;

    (*lpGetUSBDVersion)(lpdwMajorVersion,lpdwMinorVersion);
};
BOOL USBDriverClass::RegisterClientDriverID(LPCWSTR szUniqueDriverId)
{
    if(lpRegisterClientDriverID == NULL)
        return FALSE;
    return (*lpRegisterClientDriverID)(szUniqueDriverId);
};
BOOL USBDriverClass::UnRegisterClientDriverID(LPCWSTR szUniqueDriverId)
{
    if(lpUnRegisterClientDriverID == NULL)
        return FALSE;
    return (*lpUnRegisterClientDriverID)(szUniqueDriverId);
};
BOOL USBDriverClass::RegisterClientSettings(LPCWSTR lpszDriverLibFile,
                            LPCWSTR lpszUniqueDriverId, LPCWSTR szReserved,
                            LPCUSB_DRIVER_SETTINGS lpDriverSettings)
{
    if(lpRegisterClientSettings == NULL)
        return FALSE;
    return (*lpRegisterClientSettings)(lpszDriverLibFile,lpszUniqueDriverId,szReserved,lpDriverSettings);
}
BOOL USBDriverClass::UnRegisterClientSettings(LPCWSTR lpszUniqueDriverId, LPCWSTR szReserved,
                              LPCUSB_DRIVER_SETTINGS lpDriverSettings)
{
    if(lpUnRegisterClientSettings == NULL)
        return FALSE;
    return (*lpUnRegisterClientSettings)(lpszUniqueDriverId,szReserved,lpDriverSettings);
}
HKEY USBDriverClass::OpenClientRegistryKey(LPCWSTR szUniqueDriverId)
{
    if(lpOpenClientRegistyKey == NULL)
        return NULL;
    return (*lpOpenClientRegistyKey)(szUniqueDriverId);
};



UsbBasic::UsbBasic(LPCUSB_FUNCS UsbFuncsPtr,LPCUSB_INTERFACE pInterface, LPCWSTR uniqueDriverId,
                   LPCUSB_DRIVER_SETTINGS lpDriverSettings): lpUsbFuncs(UsbFuncsPtr)
{
    if(UsbFuncsPtr == NULL || pInterface == NULL || uniqueDriverId == NULL || lpDriverSettings == NULL)
        return;

    lpInterface=(LPUSB_INTERFACE) malloc(sizeof(USB_INTERFACE));
    memcpy(lpInterface,pInterface,sizeof(USB_INTERFACE));
    DWORD dwLen = wcslen(uniqueDriverId) + 1;
    szUniqueDriverId=(LPWSTR)malloc(sizeof(WCHAR)*dwLen);
    wcscpy_s(szUniqueDriverId,dwLen,uniqueDriverId);

}
UsbBasic::~UsbBasic()
{
    free(lpInterface);
    free(szUniqueDriverId);
}

#define countof(x) (sizeof(x)/sizeof(*(x)))

void UsbBasic::debug(LPCTSTR szFormat, ...) {
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
        (*lpUsbFuncs->lpGetUSBDVersion)(lpdwMajorVersion,lpdwMinorVersion);
}
//Helper commands
BOOL UsbBasic::TranslateStringDesc(LPCUSB_STRING_DESCRIPTOR lpStringDesc,
                                   LPWSTR szString,DWORD cchStringLength)
{
    if(lpUsbFuncs != NULL)
        return (*lpUsbFuncs->lpTranslateStringDesc)(lpStringDesc,szString,cchStringLength);

    return FALSE;

}
LPCUSB_INTERFACE UsbBasic::FindInterface(LPCUSB_DEVICE lpUsbDevice,
                                         UCHAR bInterfaceNumber, UCHAR bAlternateSetting)
{
    if(lpUsbFuncs != NULL)
        return (*lpUsbFuncs->lpFindInterface)(lpUsbDevice,bInterfaceNumber,bAlternateSetting);

       return NULL;

};

//------------------------------------Usb Class-------------------------------------

UsbClass::UsbClass(USB_HANDLE usbHandle,LPCUSB_FUNCS UsbFuncsPtr,
            LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId,
            LPCUSB_DRIVER_SETTINGS lpDriverSettings) :
        UsbBasic(UsbFuncsPtr,lpInterface,szUniqueDriverId,lpDriverSettings),
        hUsb(usbHandle)
{
    DEBUGMSG(DBG_FUNC,(TEXT("UsbClass")));
    if(lpInterface == NULL)
        return;
    m_dwSyncFrameNumber=0;
    DefaultDeviceDesc=(lpInterface->lpEndpoints)->Descriptor;
};
UsbClass::~UsbClass()
{

}

//USB Subsystem Commands
BOOL UsbClass::GetFrameNumber(LPDWORD lpdwFrameNumber)
{
    return (*lpUsbFuncs->lpGetFrameNumber)(hUsb,lpdwFrameNumber);
}
BOOL UsbClass::GetFrameLength(LPUSHORT lpuFrameLength)
{
    return (*lpUsbFuncs->lpGetFrameLength)(hUsb,lpuFrameLength);
};
//Enables Device to Adjust the USB SOF period on OHCI or UHCI cards
BOOL UsbClass::TakeFrameLengthControl()
{
    return (*lpUsbFuncs->lpTakeFrameLengthControl)(hUsb);
}
BOOL UsbClass::SetFrameLength(HANDLE hEvent, USHORT uFrameLength)
{
    return (*lpUsbFuncs->lpSetFrameLength)(hUsb,hEvent,uFrameLength);
}
BOOL UsbClass::ReleaseFrameLengthControl()
{
    return (*lpUsbFuncs->lpReleaseFrameLengthControl)(hUsb);
}

// Gets info on a device
LPCUSB_DEVICE UsbClass::GetDeviceInfo()
{
    return (*lpUsbFuncs->lpGetDeviceInfo)(hUsb);
};

//Device commands
USB_TRANSFER UsbClass::IssueVendorTransfer(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter,DWORD dwFlags,
        LPCUSB_DEVICE_REQUEST lpControlHeader, LPVOID lpvBuffer,ULONG uBufferPhysicalAddress)
{
    return (*lpUsbFuncs->lpIssueVendorTransfer)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,lpControlHeader,lpvBuffer,uBufferPhysicalAddress);
}
USB_TRANSFER UsbClass::GetInterface(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bInterfaceNumber, PUCHAR lpvAlternateSetting)
{
    return (*lpUsbFuncs->lpGetInterface)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
                dwFlags,bInterfaceNumber,lpvAlternateSetting);
}
USB_TRANSFER UsbClass::SetInterface(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter,DWORD dwFlags, UCHAR bInterfaceNumber, UCHAR bAlternateSetting)
{
    return (*lpUsbFuncs->lpSetInterface)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,bInterfaceNumber,bAlternateSetting);
}
USB_TRANSFER UsbClass::GetDescriptor(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bType, UCHAR bIndex, WORD wLanguage,
                                          WORD wLength, LPVOID lpvBuffer)
{
    return (*lpUsbFuncs->lpGetDescriptor)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,bType,bIndex,wLanguage,wLength,lpvBuffer);
}
USB_TRANSFER UsbClass::SetDescriptor(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bType, UCHAR bIndex, WORD wLanguage,
                                          WORD wLength, LPVOID lpvBuffer)
{
    return (*lpUsbFuncs->lpSetDescriptor)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,bType,bIndex,wLanguage,wLength,lpvBuffer);
}
USB_TRANSFER UsbClass::SetFeature(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter,DWORD dwFlags, WORD wFeature, UCHAR bIndex)
{
    return (*lpUsbFuncs->lpSetFeature)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,wFeature,bIndex);
}
USB_TRANSFER UsbClass::ClearFeature(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter,DWORD dwFlags, WORD wFeature, UCHAR bIndex)
{
    return (*lpUsbFuncs->lpClearFeature)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,wFeature,bIndex);
}
USB_TRANSFER UsbClass::GetStatus(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bIndex, LPWORD lpwStatus)
{
    return (*lpUsbFuncs->lpGetStatus)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,bIndex,lpwStatus);
}
USB_TRANSFER UsbClass::SyncFrame(LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress,LPVOID lpNodifyParameter, DWORD dwFlags, UCHAR bEndPoint, LPWORD lpwFrame)
{
    return (*lpUsbFuncs->lpSyncFrame)(hUsb,
            ((dwFlags & USB_NO_WAIT)!=0)?lpNodifyAddress:NULL,
            lpNodifyParameter,
            dwFlags,bEndPoint,lpwFrame);
}
//-----default pipe----------------------
BOOL UsbClass::ResetDefaultPipe()
{
    return (*lpUsbFuncs->lpResetDefaultPipe)(hUsb);
}
BOOL UsbClass::IsDefaultPipeHalted(LPBOOL lpbHalted)
{
    return (*lpUsbFuncs->lpIsDefaultPipeHalted)(hUsb,lpbHalted);
}

UsbClientDrv::UsbClientDrv(USB_HANDLE usbHandle,LPCUSB_FUNCS UsbFuncsPtr,
            LPCUSB_INTERFACE lpInterface, LPCWSTR szUniqueDriverId,
            LPCUSB_DRIVER_SETTINGS lpDriverSettings):
    UsbClass(usbHandle,UsbFuncsPtr,lpInterface,szUniqueDriverId,lpDriverSettings)
{

    TRACE(TEXT("UsbClientDrv"));
    if(lpInterface == NULL)
        return;

    dwNumEndPoints=lpInterface->Descriptor.bNumEndpoints;
    lpEndPoints=(LPUSB_ENDPOINT)malloc(sizeof(USB_ENDPOINT)*dwNumEndPoints);
    if(lpEndPoints == NULL)
        return;
    memcpy((PVOID)lpEndPoints,(PVOID)lpInterface->lpEndpoints,sizeof(USB_ENDPOINT)*dwNumEndPoints);
    TRACE(TEXT("End of UsbClientDrv"));
}

LPCUSB_ENDPOINT UsbClientDrv::GetDescriptorPoint(DWORD dwIndex)
{
    if (dwIndex<dwNumEndPoints)
        return (lpEndPoints+dwIndex);
    else
        return NULL;
}
UsbClientDrv::~UsbClientDrv()
{
    if (lpEndPoints)
        free((PVOID)lpEndPoints);
};


