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
////////////////////////////////////////////////////////////////////////////////
//
//
//  Include file: usbdev.h
//
//                  Includes for Tux usb device handler
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

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




