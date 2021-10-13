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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

devInit.h

Abstract:

Device loader structures and defines

Notes: 


--*/

#pragma once
 
typedef enum {
    DEVICE_DDRIVER_STATUS_UNKNOWN = 0,
    DEVICE_DDRIVER_STATUS_DISABLED,
    DEVICE_DDRIVER_STATUS_INITIALIZING,
    DEVICE_DDRIVER_STATUS_DISABLING,
    DEVICE_DDRIVER_STATUS_ENABLE
} DEVICE_DDRIVER_STATUS;

#ifdef __cplusplus
extern "C" {
#endif
typedef struct _RegActivationValues_tag {
    TCHAR DevDll[DEVDLL_LEN];       // required
    TCHAR Prefix[PREFIX_CHARS + 1]; // optional, must be of length 3 when present
    WCHAR BusPrefix[PREFIX_CHARS + 1];    // optional, must agree with Prefix if both are present
    TCHAR Filter[DEVKEY_LEN];
    DWORD Index;                    // optional
    DWORD Flags;                    // optional
    DWORD Context;                  // optional
    BOOL bOverrideContext;          // TRUE only if Context is valid
    DWORD PostInitCode;             // optional
    DWORD BusPostInitCode;          // optional
    BOOL bHasPostInitCode;          // TRUE only if PostInitCode is valid
    BOOL bHasBusPostInitCode;       // TRUE only if BusPostInitCode is valid.
    TCHAR AccountId[128];
} RegActivationValues_t, *pRegActivationValues_t;
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
#include <Csync.h>
#include <Filter.h>

LPCTSTR ConvertStringToUDeviceSID(LPCTSTR szAccount, DWORD cchLen);

class DeviceContent : public DriverFilterMgr  {
public:
    DeviceContent();
    fsdev_t *  InitDeviceContent( LPCTSTR   RegPath, LPCREGINI lpReg, DWORD cReg, void const * const lpvParam );
    virtual ~DeviceContent();

    virtual BOOL EnableDevice(DWORD dwWaitTicks);
    virtual BOOL InitialEnable();
    virtual BOOL DisableDevice(DWORD dwWaitTicks);
    BOOL IsDriverEnabled() const;
    DeviceContent * AddRefToAccess();
    VOID    DeRefAccess();
    DWORD   GetAccessRefCount() const { return dwRefCnt; };
    DWORD   GetDeviceStatus() const { return m_DeviceStatus; };
    DWORD AddRef( void )
    {
        return (DWORD)InterlockedIncrement(&m_lRefCount);
    };
    DWORD DeRef( void )
    {
        LONG lReturn = InterlockedDecrement(&m_lRefCount);
        if( lReturn <= 0 ) {
            delete this;
        }
        return (DWORD)lReturn;
    }
    DWORD LoadLib(LPCTSTR RegPath);
    BOOL InsertToDmList();
    LPCTSTR GetDriverDllName() const { return m_DevDll; }
    LPCTSTR GetDriverPrefix() const { return m_Prefix; }
protected:
    LONG    m_DeviceStatus;
    LONG    m_lRefCount;
    HANDLE  m_hComplete;
    LPTSTR  m_DeviceNameString;
    LPTSTR  m_ActivationPath;
    DWORD   m_PostCode;
    DWORD   m_BusPostCode;

    DWORD   m_dwActivationParam;
    DWORD   m_dwActivateInfo;
    DWORD   m_dwDevIndex;
    TCHAR   m_DevDll[DEVDLL_LEN+1];       // required
    TCHAR   m_Prefix[MAXDEVICENAME]; // optional, must be of length 3 when present
    TCHAR   m_AccessPreFix[MAXDEVICENAME];
            
    DWORD CreateContent(
        LPCWSTR lpszPrefix,
        DWORD dwIndex,
        DWORD dwLegacyIndex,
        DWORD dwId,
        LPCWSTR lpszLib,
        DWORD dwFlags,
        LPCWSTR lpszBusPrefix,
        LPCWSTR lpszBusName,
        LPCWSTR lpszDeviceKey,
        HANDLE hParent
        );

    static BOOL IsThisDeviceUnique(WCHAR const * const pszPrefix, DWORD dwIndex, LPCWSTR pszBusName);
    BOOL RemoveDeviceOpenRef(BOOL fForce = FALSE );
};

inline DWORD GetCallerProcessId(void) 
{ 
    return ((DWORD)GetDirectCallerProcessId()); 
};

#endif


#ifdef __cplusplus
extern "C" {
#endif
typedef struct _BusDriverValues_tag {
    HANDLE hParent;
    WCHAR szBusName[MAXBUSNAME];
} BusDriverValues_t, *pBusDriverValues_t;

VOID    DeleteActiveKey( LPCWSTR ActivePath );
DWORD   RegReadActivationValues(LPCWSTR RegPath, pRegActivationValues_t pav);
DWORD   CreateActiveKey( __out_ecount(cchActivePath) LPWSTR pszActivePath,DWORD cchActivePath, LPDWORD pdwId, HKEY *phk );
DWORD   InitializeActiveKey( HKEY hkActive, LPCWSTR pszRegPath, LPCREGINI lpReg, DWORD cReg );
DWORD   ReadBusDriverValues(HKEY hkActive, pBusDriverValues_t pbdv);
DWORD   FinalizeActiveKey( HKEY hkActive, const fsdev_t *lpdev );
BOOL    I_CheckRegisterDeviceSafety (LPCWSTR lpszType, DWORD dwIndex,LPCWSTR lpszLib, PDWORD pdwFlags);
VOID    DevicePostInit(LPCTSTR DevName, DWORD  dwIoControlCode, HANDLE hActiveDevice, LPCWSTR RegPath );

LPCTSTR GetDevicePrefix(fsdev_t * pDevice);
LPCTSTR GetDeviceDllName(fsdev_t * pDevice);
fsdev_t * AddDeviceAccessRef(fsdev_t * pDevice);
void    DeDeviceAccessRef(fsdev_t * pDevice);
void    AddDeviceRef(fsdev_t * pDevice) ;
DWORD   DeDeviceRef(fsdev_t * pDevice);
HANDLE  I_RegisterDeviceEx(LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo );
HANDLE  I_ActivateDeviceEx( LPCTSTR   RegPath, LPCREGINI lpReg, DWORD     cReg,  LPVOID    lpvParam );
BOOL    I_DeregisterDevice(HANDLE hDevice);
BOOL    I_DeactivateDevice( HANDLE hDevice );
BOOL    I_EnableDevice( HANDLE hDevice , DWORD dwTicks);
BOOL    I_DisableDevice(HANDLE hDevice, DWORD dwTicks);
BOOL    I_GetDeviceDriverStatus(HANDLE hDevice, PDWORD pdwResult);
void      I_RemoveDevicesByProcId(const HPROCESS hProc,  BOOL fForce);

#ifdef __cplusplus
};
#endif


