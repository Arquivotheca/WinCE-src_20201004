//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    devloadp.h
 *
 * Purpose: Device Loader internal definitions
 *
 */

#define MAX_LOAD_ORDER 255

#define REG_PATH_LEN 256    // Length of registry path string

//
// The RootKey value holds the top level key to look for devices to load
//
#define DEVLOAD_ROOTKEY_VALNAME      TEXT("RootKey")     // Root key
#define DEVLOAD_ROOTKEY_VALTYPE      REG_SZ

extern const TCHAR s_DllName_ValName[];
extern const TCHAR s_ActiveKey[];
extern const TCHAR s_BuiltInKey[];


//
// Prototypes for device notification stuff in pnp.c
// The DEVDETAIL structure is defined in sdk\inc\pnp.h
//
extern void   InitializeDeviceNotifications (void);
extern HANDLE RegisterDeviceNotifications (LPCGUID, HANDLE);
extern void   UnregisterDeviceNotifications (HANDLE);
extern void   PnpProcessInterfaces (LPCTSTR, BOOL);
extern void   PnpSendTo (HANDLE, DWORD);
extern BOOL UpdateAdvertisedInterface(LPCGUID, LPCWSTR, BOOL);
extern void AdvertisementSendTo(HANDLE);
extern void DeleteProcessAdvertisedInterfaces(DWORD, HPROCESS, HTHREAD);

// These are used to manage notifications in pnp.c
typedef struct NotifyListElement_ NotifyListElement;
typedef struct NotifyListElement_ {
    GUID guidDevClass;
    HANDLE hSend;
    NotifyListElement *pNext;
} NotifyListElement;

#define GUID_ISEQUAL(a,b) (memcmp(&(a),&(b),sizeof(a)) == 0)

#ifdef DEBUG
//
// Debug zones
//
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_FUNCTION   DEBUGZONE(2)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_ROOT       DEBUGZONE(4)
#define ZONE_PCMCIA     DEBUGZONE(5)
#define ZONE_ACTIVE     DEBUGZONE(6)
#define ZONE_RESMGR     DEBUGZONE(7)
#define ZONE_FSD        DEBUGZONE(8)
#define ZONE_DYING      DEBUGZONE(9)
#define ZONE_BOOTSEQ    DEBUGZONE(10)
#define ZONE_PNP            DEBUGZONE(11)

#endif  // DEBUG
