//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
////////////////////////////////////////////////////////////////////////////////
//
//
//  Include file: usbdev.h
//
//                  Includes for Tux usb device handler
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __USBDEV_H__
#define __USBDEV_H__

//******************************************************************************
//***** Include files
//******************************************************************************

#include    <usbdi.h>

//******************************************************************************
//***** Defines, typedefs,...
//******************************************************************************

typedef struct __usb_dev {
    struct __usb_dev        *next;              // for chaining usb dev structs
    HANDLE                  hHeap;              // we make our own heap
    USB_HANDLE              hDevice;            // by USBAttach()
    LPCUSB_FUNCS            lpUsbFuncs;         // by USBAttach()
    LPCUSB_INTERFACE        lpInterface;        // by USBAttach()
    LPCWSTR                 szUniqueDriverId;   // by loader.cpp
    LPCUSB_DRIVER_SETTINGS  lpDriverSettings;   // by loader.cpp
    BOOL                    fAbort;             // device removed
    BOOL                    *pfAbort;           // use when not sure if copy
    LONG                    lRefCount;          // structure reference count
    LONG                    *plRefCount;        // use when not sure if copy
    LONG                    lCplRefCount;       // structure reference count cpl
    LONG                    *plCplRefCount;     // use when not sure if copy
    HANDLE                  hEvent;             // for USBNotify()
    HANDLE                  hThread;            // extra, user data
    DWORD                   dwThreadId;         // extra, user data
    DWORD                   dwData;             // extra, user data
    PVOID                   pvPtr;              // extra, user data
} USB_DEV, *LPUSB_DEV;

//******************************************************************************
//***** Global function declarations
//******************************************************************************

LPUSB_DEV           USBOpenUsbDev           ( void );
BOOL                USBCloseUsbDev          ( LPUSB_DEV lpUsbDev );

//******************************************************************************
//***** Global variables
//******************************************************************************

//******************************************************************************

#endif  //__USBDEV_H__