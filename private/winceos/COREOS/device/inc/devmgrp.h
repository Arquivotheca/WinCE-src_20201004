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

//
// This file contains internal definitions for the Device Manager.
//

#pragma once

#include <linklist.h>
#include <pnp.h>    // only for DEVDETAIL

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#ifdef _PREFAST_
#ifdef DEBUGCHK
#undef DEBUGCHK
#endif // DEBUGCHK
#define DEBUGCHK(exp) PREFAST_ASSUME(exp)
#endif // _PREFAST_

#ifndef ARRAYSTORAGE
#define ARRAYSTORAGE(a)         (_countof(a) * sizeof(a[0]))
#endif

//
// The RootKey value holds the top level key to look for devices to load
//
#define DEVLOAD_ROOTKEY_VALNAME      TEXT("RootKey")     // Root key
#define DEVLOAD_ROOTKEY_VALTYPE      REG_SZ
#define MAX_LOAD_ORDER              255
#define REG_PATH_LEN                256    // Length of registry path string
#define PREFIX_CHARS                3
#define MAXDEVICENAME               32      // really only need 3-char prefix and up to 10 digits, plus NULL
#define MAXBUSNAME                  MAX_PATH
#define MAXDEVICEINDEX              65535
#define ID_REGISTERED_DEVICE        0xFFFFFFFF

// prototypes for device driver entry points
typedef DWORD (* pInitFn)(DWORD,LPVOID);
typedef DWORD (*pInitExFn)(DWORD,DWORD,LPVOID);
typedef BOOL (* pPreDeinitFn)(DWORD);
typedef BOOL (* pDeinitFn)(DWORD);
typedef DWORD (* pOpenFn)(DWORD,DWORD,DWORD);
typedef BOOL (* pPreCloseFn)(DWORD);
typedef BOOL (* pCloseFn)(DWORD);
typedef DWORD (* pReadFn)(DWORD,LPVOID,DWORD,HANDLE);
typedef DWORD (* pWriteFn)(DWORD,LPCVOID,DWORD,HANDLE);
typedef DWORD (* pSeekFn)(DWORD,long,DWORD);
typedef BOOL (* pControlFn)(DWORD,DWORD,PBYTE,DWORD,PBYTE,DWORD,PDWORD,HANDLE);
typedef void (* pPowerupFn)(DWORD);
typedef void (* pPowerdnFn)(DWORD);
typedef BOOL (* pCancelIoFn)(DWORD, HANDLE);

// ceddk timer calibration routine
typedef void (*pCalibrateStallFn)(VOID);

//
// This structure keeps track of advertised interfaces
//
typedef struct fsinterface_t {
    struct fsinterface_t *pNext;
    GUID guidClass;     // interface type
    LPCWSTR pszName;    // name associated with the interface
    DWORD ioctl;       // ioctrl
    DWORD ioctlmask;   // ioctrl mask
} fsinterface_t;

// state flags for the fsdev_t device driver structure
#define DF_DEACTIVATING             0x0001
#define DF_AUTO_DEREGISTER          0x0002
#define DF_REGISTERED               0x0004
#define DF_TRUSTED_LOADER           0x0008
#define DF_ENABLE_BUSNAMES          0x0010
#define DF_DYING                    0x8000

//
// Structure to track device drivers
//
typedef struct fsdev_t {
    // pointers to other fsdev_t structures
    LIST_ENTRY list;

    // driver entry points
    pInitFn fnInit;             // required
    pInitExFn fnInitEx;         // replace fnInit if it is not NULL
    pPreDeinitFn fnPreDeinit;   // optional
    pDeinitFn fnDeinit;         // required
    pOpenFn fnOpen;             // if present, need close and at least one of read/write/seek/control
    pPreCloseFn fnPreClose;     // optional even if close is present
    pCloseFn fnClose;           // required if open is present
    pReadFn fnRead;
    pWriteFn fnWrite;
    pSeekFn fnSeek;
    pControlFn fnControl;
    pPowerupFn fnPowerup;       // optional, can be NULL
    pPowerdnFn fnPowerdn;       // optional, can be NULL
    pCancelIoFn fnCancelIo;     // optional, can be NULL

    // instance information
    DWORD dwData;               // instance data returned from Init() entry point
    HINSTANCE hLib;             // driver dll handle
    DWORD dwId;                 // active key ID
    WORD wFlags;                // DF_XXX state flags
    DWORD dwRefCnt;             // use count for this device structure
    HANDLE hParent;             // parent device driver, or NULL
    fsinterface_t *pInterfaces; // interfaces advertised by this device instance
    LPWSTR pszLegacyName;       // e.g., "COM1:"
    LPWSTR pszDeviceName;       // e.g., "Serial1" in $device namespace
    LPWSTR pszBusName;          // e.g., "Builtin_1_1" in $bus namespace
    LPWSTR pszDeviceKey;        // NULL if not an activated device
    DWORD dwActivateId;         // thread ID for the thread calling ActivateDevice() (only valid during Init())
    DWORD dwFlags;              // Driver's loading Flag.
    DWORD dwDataEx;
} fsdev_t;

typedef struct fsopendevaccess_t {
    struct fsopendevaccess_t *nextptr;    // next one in linked list
    fsinterface_t *pFSInterface;
    LRESULT dwAccess;
} fsopendevaccess_t;

//
// Note on reference counts:
// lpdwDevRefCnt points to the ref count of the associated fsdev_t. While fsdev_t.dwRefCnt is not 0, then
// the fsdev_t will not be freed.
// dwOpenRefCnt is the ref count for the fsopendev_t structure. While it is not 0, the fsopendev_t will
// not be freed.
// There were situations where the fsopendev_t structure was freed while a thread was blocked inside
// a DeviceIoControl call. dwOpenRefCnt will prevent this.
//
typedef struct fsopendev_t {
    struct fsopendev_t *nextptr;    // next one in linked list
    DWORD dwOpenData;
    fsdev_t *lpDev;
    volatile LONG dwOpenRefCnt;     // ref count for this structure 
    HANDLE KHandle;         // kernel handle pointing to this structure
    HPROCESS hProc;         // process owning this handle
    DWORD dwAccess;         // Access Right passed in by Open
    DWORD dwAccessException; // Access Exception Flags.
    struct fsopendevaccess_t *pOpenDevAccess;
} fsopendev_t;

//
// This structure keeps track of devices that are in the process of being loaded.
//
typedef struct fscandidatedev_t {
    LIST_ENTRY list;
    WCHAR szDeviceName[MAXDEVICENAME];
    WCHAR szBusName[MAXBUSNAME];
} fscandidatedev_t;

// Device Management prototypes
void DevMgrPowerOffHandler(BOOL bOff);
void DeleteDevice(fsdev_t *lpdev);
HANDLE I_ActivateDeviceEx(LPCTSTR RegPath, LPCREGINI Registry, DWORD cRegEntries, LPVOID lpvParam);
void DevloadPostInit(void);
BOOL I_DeactivateDevice(HANDLE hDevice);
fsdev_t *FindDeviceByHandle(fsdev_t * lpdev);
fsdev_t *FindDeviceByName(LPCWSTR pszName, NameSpaceType nameType);
fsdev_t *FindDeviceCandidateByName(LPCWSTR pszName, NameSpaceType nameType);
void InitDevices(LPCTSTR lpBusName);
void DevloadInit(void);
void PnpAdvertiseInterfaces (fsdev_t *);
void PnpDeadvertiseInterfaces (fsdev_t *);
void InitializePnPNotifications (void);
HANDLE I_CreateDeviceHandle(LPCWSTR lpNew, NameSpaceType nameType, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc);
DWORD I_GetDeviceInformation(fsdev_t *lpdev, PDEVMGR_DEVICE_INFORMATION pdi);
BOOL ConvertStringToGuid (LPCTSTR pwszGuid, GUID *pGuid);
BOOL ConvertGuidToString (__out_ecount(cchGuid) LPTSTR pszGuid, int cchGuid, const GUID *pGuid);
fsinterface_t *FindDeviceInterface(fsdev_t *lpdev, const GUID *pClass);
DWORD I_AdvertiseDeviceInterface(fsdev_t *lpdev, const GUID *pClass, LPCWSTR pszName, BOOL fAdd);

// Device Manager filesystem prototypes
BOOL InitDeviceFilesystems(void);
void RemoveDeviceFromSearchList(fsdev_t *lpdev);

// Resource Manager API prototypes
void ResourceInitModule (void);
void ResourceInitFromRegistry (LPCTSTR key);

#ifndef _DEVMGRDATA_DEF
extern CRITICAL_SECTION g_devcs;
extern LIST_ENTRY g_DevChain;
extern LIST_ENTRY g_DevCandidateChain;
extern LIST_ENTRY g_ActivatingDevs;
extern LIST_ENTRY g_DyingDevs;
extern LIST_ENTRY g_CandidateDevs;
extern fsopendev_t *g_lpOpenDevs;
extern fsopendev_t *g_lpDyingOpens;
extern HANDLE g_hDevApiHandle, g_hDevFileApiHandle, g_hCleanEvt;
extern DWORD g_BootPhase;
extern BOOL g_bSystemInitd;
extern const TCHAR s_DllName_ValName[];
extern const TCHAR s_ActiveKey[];
extern const TCHAR s_BuiltInKey[];
#endif  // _DEVMGRDATA_DEF

#ifdef __cplusplus
}
#endif  // __cplusplus

