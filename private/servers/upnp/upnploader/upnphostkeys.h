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
typedef enum 
{
    UPNPREG_DISABLED = 0,
    UPNPREG_STARTING,
    UPNPREG_STARTED,
    UPNPREG_INVALID
} UPNPREG_STATE;

// named event used for signalling the hosting process
#define UPNPLOADEREVENTNAME         L"UPnPDeviceHost"

// registry key under which all the registered UPnP devices are listed
#define UPNPDEVICESKEY              L"Comm\\UPnPDevices"

// value used to control the hosting process
#define UPNPLOADERSIGNALVALUE       L"Control"

// parameter values for registered devices
#define UPNPSTATEVALUE              L"State"
#define UPNPPROGIDVALUE             L"ProgId"
#define UPNPINITSTRINGVALUE         L"InitString"
#define UPNPXMLDESCVALUE            L"XMLDesc"
#define UPNPXMLDESCFILE             L"XMLDescFile"
#define UPNPRESOURCEPATHVALUE       L"ResourcePath"
#define UPNPLIFETIME                L"LifeTime"
#define UPNPUDN                     L"UDN"
