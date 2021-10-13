//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
typedef enum {
    UPNPREG_DISABLED = 0,
    UPNPREG_STARTING,
    UPNPREG_STARTED,
    UPNPREG_INVALID
} UPNPREG_STATE;

// named event used for signalling the hosting process
#define UPNPLOADEREVENTNAME			L"UPnPDeviceHost"

// registry key under which all the registered UPnP devices are listed
#define UPNPDEVICESKEY				L"Comm\\UPnPDevices"

// value used to control the hosting process
#define UPNPLOADERSIGNALVALUE		L"Control"

// parameter values for registered devices
#define UPNPSTATEVALUE				L"State"
#define UPNPPROGIDVALUE				L"ProgId"
#define UPNPINITSTRINGVALUE			L"InitString"
#define UPNPXMLDESCVALUE			L"XMLDesc"
#define UPNPRESOURCEPATHVALUE		L"ResourcePath"
#define UPNPLIFETIME				L"LifeTime"


