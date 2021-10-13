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
/* File:    devInit.cpp
 *
 * Purpose: WinCE device manager initialization and built-in device management
 *
 */
#include <windows.h>
#include <tchar.h>
#include <devload.h>
#include <errorrep.h>
#include <sync.hxx>
#include <sid.h>
#include <cregedit.h>

#include "devmgrp.h"
#include "Reflector.h"
#include "devzones.h"
#include "devinit.h"

static BOOL IsDriverDisabled(LPCWSTR RegPath)
{
    DWORD dwStatus;
    BOOL  fDisabled = FALSE;
    HKEY hDisableActive = NULL;
    HKEY hActiveKey = NULL;

    dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEVLOAD_DISABLED_ICLASS_KEY, 0,
                    0, &hDisableActive);
    if (dwStatus == ERROR_SUCCESS)
    { 
        dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPath, 0, 0, &hActiveKey);
        if (dwStatus == ERROR_SUCCESS) 
        { 
            DWORD cbSize = 0;
            // Figure out the size.
            dwStatus = RegQueryValueEx(hActiveKey, DEVLOAD_ICLASS_VALNAME, NULL,
                            NULL, NULL, &cbSize);
            if (dwStatus == ERROR_SUCCESS)
            {
                LPWSTR pszIClass = NULL;
                LPWSTR pszIClassAlloc = NULL;
                DWORD cbBufferSize = ((cbSize+ sizeof(*pszIClass) -1)/sizeof(*pszIClass) + 1)*sizeof(*pszIClass);
                if (cbSize && cbSize< cbBufferSize)
                {
                    pszIClass = pszIClassAlloc = (LPWSTR) malloc(cbBufferSize);
                }
                if (pszIClass &&
                    cbBufferSize &&
                    (dwStatus = RegQueryValueEx(hActiveKey,
                                                DEVLOAD_ICLASS_VALNAME,
                                                NULL,
                                                NULL,
                                                (LPBYTE) pszIClass,
                                                &cbBufferSize))== ERROR_SUCCESS)
                {
                    cbBufferSize /= sizeof(*pszIClass);
                    pszIClass[cbBufferSize - 1] = 0;  //enforce null termination
                    while (!fDisabled && cbBufferSize && *pszIClass!=0) {
                        GUID guidClass;
                        LPWSTR pszName = wcschr(pszIClass, L'=');
                        DWORD cbLen;
                        if (pszName)
                        { // We found this '='
                            *pszName = 0 ;// replace with 0
                            pszName ++;
                        }
                        else
                        {
                            pszName =pszIClass + _tcslen(pszIClass) + 1;
                        }
                        if (ConvertStringToGuid(pszIClass, &guidClass))
                        { // We found IClass.
                            DWORD dwType;
                            DWORD dwValue = 0;
                            DWORD cbLenKey = sizeof(dwValue);
                            if ((dwStatus = RegQueryValueEx(hDisableActive,
                                                            pszIClass ,
                                                            NULL,
                                                            &dwType,
                                                            (LPBYTE) &dwValue,
                                                            &cbLenKey))
                                                == ERROR_SUCCESS &&
                                 dwType==REG_DWORD)
                            {
                                fDisabled = (dwValue!=0);
                            }
                        }

                        // Prepare for next
                        cbLen =(DWORD)(pszName-pszIClass);
                        pszIClass = pszName;
                        if (cbBufferSize > cbLen  ) {
                            cbBufferSize-=cbLen;
                        }
                        else {
                            cbBufferSize = 0 ;
                            break;
                        }                            
                    }
                }
                if (pszIClassAlloc)
                    free(pszIClassAlloc);
            }
            RegCloseKey( hActiveKey ); 
        }
        RegCloseKey( hDisableActive ); 
    }

    return fDisabled;
}

// This routine looks for a device driver entry point.  The "type" parameter
// is the driver's prefix or NULL; the lpszName parameter is the undecorated
// name of the entry point; and the hInst parameter is the DLL instance handle
// of the driver's DLL.
static PFNVOID GetDMProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) {
    LPCWSTR s;
    WCHAR fullname[32];
    if (type != NULL && *type != L'\0') {
        memcpy(fullname,type,3*sizeof(WCHAR));
        fullname[3] = L'_';
        VERIFY(SUCCEEDED(StringCchCopy(&fullname[4], _countof(fullname) - 4,lpszName)));
        s = fullname;
    } else {
        s = lpszName;
    }
    return (PFNVOID)GetProcAddress(hInst, s);
}
// Placeholder for entry point not exported by a driver.
static BOOL DevFileNotSupportedBool (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

// Placeholder for entry point not exported by a driver.
static DWORD DevFileNotSupportedDword (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return (DWORD)-1;
}



extern "C" fsdev_t * AddDeviceAccessRef(fsdev_t * pDevice)
{
    DeviceContent * pCur = (DeviceContent *)pDevice;
    if (pCur) {
        return pCur->AddRefToAccess();
    }
    else
        return NULL;
    
}
extern "C" void DeDeviceAccessRef(fsdev_t * pDevice)
{
    DEBUGCHK(pDevice);
    ((DeviceContent *)pDevice)->DeRefAccess();
}
extern "C" void AddDeviceRef(fsdev_t * pDevice) 
{
    DEBUGCHK(pDevice);
    ((DeviceContent *)pDevice)->AddRef();
}
extern "C" DWORD DeDeviceRef(fsdev_t * pDevice)
{
    DEBUGCHK(pDevice);
    return ((DeviceContent *)pDevice)->DeRef();
}

// This routine unloads a driver that was loaded with I_RegisterDeviceEx().
// It returns TRUE if successful, or FALSE if there's a problem.  If there's
// a problem it calls SetLastError() to pass back additional information.
extern "C" BOOL 
I_DeregisterDevice(HANDLE hDevice)
{
    DeviceContent *lpdev;
    DWORD dwStatus = ERROR_NOT_FOUND;

    // validate the handle
    lpdev = (DeviceContent *)FindDeviceByHandle((fsdev_t *)hDevice);
    if(lpdev) {
        // If flags no unload return ACCESS_DENIED.
        // This flags may also come from the reigstry check RegisterDeviceEx.
        if (lpdev->dwFlags & DEVFLAGS_NOUNLOAD ) {
            dwStatus = ERROR_ACCESS_DENIED;
        }
        // Signal an error if this device was not registered *or* if the
        // deactivating flag is not set.  This routine is invoked during
        // DeactivateDevice() processing, in which case it is valid for the
        // deactivating flag to be set.
        else if((lpdev->wFlags & DF_REGISTERED) == 0 && (lpdev->wFlags & DF_DEACTIVATING) == 0) {
            DEBUGMSG(ZONE_WARNING, 
                (_T("I_DeregisterDevice: rejecting attempt to DeregisterDevice() device 0x%08x loaded with ActivateDevice()\r\n"),
                lpdev));
        } else {
            dwStatus = ERROR_SUCCESS;
        }
        if (dwStatus == ERROR_SUCCESS) {
            
            lpdev->DisableDevice(INFINITE);
            
            // remove from the list of active devices
            EnterCriticalSection (&g_devcs);
            RemoveDeviceFromSearchList(lpdev);
            RemoveEntryList((PLIST_ENTRY)(fsdev_t *)lpdev);
            DeDeviceRef(lpdev);
            LeaveCriticalSection (&g_devcs);
        }
        DeDeviceRef(lpdev);
    }
    if(dwStatus != ERROR_SUCCESS) {
        DEBUGCHK(dwStatus != ERROR_SUCCESS);
        SetLastError(dwStatus);
        return FALSE;
    }
    else
        return TRUE;
}

// This routine unloads all drivers that owned by the given process
// if user choose fForce to true, this routine will directly free the 
// driver instance, otherwise, it will call DeDeviceRef() to free it
extern "C" void
I_RemoveDevicesByProcId(const HPROCESS hProc,  BOOL /*fForce*/) {

    fsdev_t* lpTrav;
    fsdev_t* lpDel;
    LIST_ENTRY RemovedDevList;
    
    InitializeListHead(&RemovedDevList);
   
    //remove this process' drivers from active driver list
    EnterCriticalSection(&g_devcs);
    lpTrav = (fsdev_t *)g_DevChain.Flink;
    while(lpTrav != (fsdev_t *)&g_DevChain){
        if (lpTrav->dwDataEx) {//only user mode driver could reside in a different process other than kernel
            //get process info
            PROCESS_INFORMATION pi = Reflector_GetOwnerProcId(lpTrav->dwDataEx);
            if (pi.dwProcessId == (DWORD)hProc) {//found one driver owned by this process
            DEBUGMSG(ZONE_WARNING |ZONE_DYING, 
                        (_T("I_RemoveDevicesByProcId: driver instance 0x%p associated with proc 0x%d will be removed\r\n"),
                        lpTrav, hProc));
                //remove it
                lpDel = lpTrav;
                lpTrav = (fsdev_t *)(lpTrav->list.Blink);
                RemoveDeviceFromSearchList(lpDel);
                RemoveEntryList((PLIST_ENTRY)lpDel);
                InsertTailList(&RemovedDevList, (PLIST_ENTRY)lpDel);
            }
        }
        lpTrav = (fsdev_t *)(lpTrav->list.Flink);
    }
    LeaveCriticalSection (&g_devcs);

    if (!IsListEmpty(&RemovedDevList)) {
        // deinit these device instances
        lpDel = (fsdev_t *)RemovedDevList.Flink;
        do {
            // Tell the device to deinit() nicely.
            VERIFY(((DeviceContent*)lpDel)->DisableDevice(0));

            lpDel = (fsdev_t *)(lpDel->list.Flink);
        } while (lpDel != (fsdev_t *)&RemovedDevList);

        // DeRef the device, if there are no trapped threads, this will cause a delete
        while(!IsListEmpty(&RemovedDevList)){
            lpDel = (fsdev_t *)RemovedDevList.Flink;
            RemoveEntryList((PLIST_ENTRY)lpDel);

            DWORD Refs = DeDeviceRef(lpDel);

            // if this assert fires then likely a thread is still trapped in the driver
            //  it should have been released by predeinit(), check the call stacks for any
            //  still in ((DeviceContent*)lpDel)->m_DevDll
            ASSERT(Refs == 0);
            DBG_UNREFERENCED_LOCAL_VARIABLE(Refs);
        }
    }

}

//
// Function to RegisterDevice a device driver and add it to the active device list
// in HLM\Drivers\Active and then signal the system that a new device is available.
//
// Return the HANDLE from RegisterDevice.
//
#define MAX_RETRY_COUNT 3
extern "C" 
HANDLE I_ActivateDeviceEx( LPCTSTR   RegPath, LPCREGINI lpReg, DWORD     cReg,  LPVOID    lpvParam )
{
    DeviceContent * hDevice = NULL;
    LONG   lRetry = MAX_RETRY_COUNT;
    
    while (hDevice == NULL ) {        
        hDevice = new DeviceContent();
        
        if (hDevice == NULL ) { // No Memory.
            SetLastError(ERROR_OUTOFMEMORY);
            break;
        }  
        AddDeviceRef(hDevice);

        BOOL fFailed = (hDevice->InitDeviceContent(RegPath,lpReg,cReg, lpvParam)==NULL); // creates prefix here
        if (!fFailed) {
            DWORD dwStatus = hDevice->LoadLib(RegPath); // calls into driver here
            if (dwStatus != ERROR_SUCCESS) {
                fFailed = TRUE;
                SetLastError(dwStatus);
            }
        }
        if (fFailed) { // No need to retry because it hard fail
            // remove from candidates list
            if (IsListEmpty((PLIST_ENTRY)(fsdev_t *)hDevice) == FALSE)
                RemoveEntryLockedList((PLIST_ENTRY)(fsdev_t *)hDevice, &g_devcs);

            ASSERT(IsListEmpty((PLIST_ENTRY)(fsdev_t *)hDevice));
            DeDeviceRef(hDevice);
            hDevice = NULL;
            break;
        }
        else { // Succeeded, We can insert to DM List
            if (!hDevice->InsertToDmList()) { // add to g_DevChain here
                DeDeviceRef(hDevice);
                hDevice = NULL;
                if ((lRetry--) <= 0) { // We failed even after try. Do know what cause it  failed.
                    SetLastError(ERROR_GEN_FAILURE);
                    break;
                }
            }
        }
    }
    if (hDevice) {
        BOOL fUnload = ((hDevice->dwFlags & DEVFLAGS_UNLOAD)!=0);

        // mark driver disabled if disabledIclass regkey says so
        if (IsDriverDisabled(RegPath))
        {
            DEBUGMSG(ZONE_ACTIVE | ZONE_WARNING, (
                L"DevMgr: ActivateDevice: Loading %s device as Disabled\r\n",
                RegPath
            ));
            hDevice->dwFlags |= DEVFLAGS_LOADDISABLED;
        }
        
        BOOL fEnableOk = hDevice->InitialEnable();
        DeDeviceRef(hDevice);
        if ( fUnload || fEnableOk == FALSE ) {
            I_DeactivateDevice((HANDLE)(fsdev_t *)hDevice);
            hDevice = NULL;
            if (fUnload)
                SetLastError(ERROR_SUCCESS); // for BC.
        }
    }
    return (HANDLE)(fsdev_t *)hDevice;

}

//
// Function to DeregisterDevice a device, remove its active key, and announce
// the device's removal to the system.
//
extern "C" BOOL 
I_DeactivateDevice(
    HANDLE hDevice
    )
{
    fsdev_t *lpdev = (fsdev_t *) hDevice;
    DWORD dwStatus = ERROR_NOT_FOUND;

    // validate the device handle
    if((lpdev = FindDeviceByHandle(lpdev))!=NULL) {
        // If flags no unload return ACCESS_DENIED.
        if (lpdev->dwFlags & DEVFLAGS_NOUNLOAD ) {
            dwStatus = ERROR_ACCESS_DENIED;
        }
        // make sure the device was activated (not registered) and isn't deactivating
        else if((lpdev->wFlags & DF_REGISTERED) == 0 && (lpdev->wFlags & DF_DEACTIVATING) == 0) {
            lpdev->wFlags |= DF_DEACTIVATING;
            dwStatus = ERROR_SUCCESS;
        }
        // unload the driver
        if (dwStatus == ERROR_SUCCESS) {
            I_DeregisterDevice(lpdev);
        };
        DeDeviceRef(lpdev);        
    }

    // return false if this wasn't a valid device or if it's already
    // being deactivated
    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        return FALSE;
    }
    return TRUE;
}
extern "C" BOOL I_EnableDevice( HANDLE hDevice , DWORD dwTicks)
{
    fsdev_t *lpdev = (fsdev_t *) hDevice;
    if((lpdev = FindDeviceByHandle(lpdev))!=NULL)
    {
        BOOL fResult = FALSE;
        if (!IsDriverDisabled(((DeviceContent *)lpdev)->pszDeviceKey))
        {
            fResult = ((DeviceContent *)lpdev)->EnableDevice(dwTicks);
        }
        else
        {
            fResult = FALSE;
            SetLastError(ERROR_ACCESS_DENIED);
        }
        DeDeviceRef(lpdev);
        return fResult;
    }
    else {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    
}
extern "C" BOOL I_DisableDevice(HANDLE hDevice, DWORD dwTicks)
{
    fsdev_t *lpdev = (fsdev_t *) hDevice;
    if((lpdev = FindDeviceByHandle(lpdev))!=NULL) 
    {
        BOOL fResult = FALSE;
        fResult = ((DeviceContent *)lpdev)->DisableDevice(dwTicks);
        DeDeviceRef(lpdev);
        return fResult;
    }
    else 
    {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
}
extern "C" BOOL I_GetDeviceDriverStatus(HANDLE hDevice, PDWORD pdwResult)
{
    fsdev_t *lpdev = (fsdev_t *) hDevice;
    if((lpdev = FindDeviceByHandle(lpdev))!=NULL) {
        BOOL fResult = ((DeviceContent const *)lpdev)->IsDriverEnabled();
        DeDeviceRef(lpdev);
        if (pdwResult) {
            *pdwResult = (fResult?1:0);
        }
        return TRUE;
    }
    else {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
}
DeviceContent::DeviceContent()
{
    memset((fsdev_t *)this, 0, sizeof(fsdev_t));
    InitializeListHead((PLIST_ENTRY)(fsdev_t *)this);
    m_lRefCount = 0;
    m_hComplete = CreateEvent(NULL,TRUE,TRUE,NULL);
    dwActivateId = GetCallerProcessId();
    m_DeviceNameString = NULL;
    m_ActivationPath = NULL;
    m_DeviceStatus = DEVICE_DDRIVER_STATUS_UNKNOWN;
    m_PostCode = 0;
    m_BusPostCode = 0 ;
    m_DevDll[0] = m_Prefix[0] = m_AccessPreFix[0] = 0 ;

}
DeviceContent::~DeviceContent()
{
    ASSERT(m_lRefCount == 0); 
    if (this->dwId != ID_REGISTERED_DEVICE && m_ActivationPath!=NULL ) {
        DeleteActiveKey(m_ActivationPath);
    }
    RemoveDeviceOpenRef(TRUE);
    if(this->hLib != NULL) {
        ASSERT(this->dwDataEx == 0);
        FreeLibrary(this->hLib);
    }
    if (this->dwDataEx!=0) {
        ASSERT(this->hLib ==NULL );
        Reflector_Delete(this->dwDataEx);
    }
    if (m_hComplete) {
        CloseHandle(m_hComplete);
    };
    if (m_DeviceNameString)
        delete m_DeviceNameString;
    if (m_ActivationPath)
        delete m_ActivationPath;
    
};

BOOL DeviceContent::InitialEnable()
{
    if ((dwFlags & DEVFLAGS_LOADDISABLED)==0) 
        return EnableDevice(INFINITE);
    else
        return TRUE;
}

BOOL DeviceContent::IsDriverEnabled() const
{
    return (m_DeviceStatus == DEVICE_DDRIVER_STATUS_ENABLE);
}

BOOL DeviceContent::RemoveDeviceOpenRef(BOOL fForce )
{
    fsopendev_t *lpopen;
    EnterCriticalSection(&g_devcs);
    lpopen = g_lpOpenDevs;      // check the active handles list
    const fsdev_t * lpdev = this;
    while (lpopen) {
        if (lpopen->lpDev == lpdev) {
            if (fForce) {
                lpopen->lpDev = NULL;
                DeRef();
                RETAILMSG(1, (L"WARNING: Forcing open handle to '%s'/'%s' closed", lpdev->pszDeviceName, lpdev->pszBusName));
            }
            else {
                lpopen->lpDev->wFlags |= DF_DYING;
            }
        }
        lpopen = lpopen->nextptr;
    }
    lpopen = g_lpDyingOpens;    // check the dying handles list
    while (lpopen) {
        if (lpopen->lpDev == lpdev) {
            if (fForce) {
                lpopen->lpDev = NULL;
                DeRef();         
                RETAILMSG(1, (L"WARNING: Forcing dying handle to '%s'/'%s' closed", lpdev->pszDeviceName, lpdev->pszBusName));
            }
            else {
                lpopen->lpDev->wFlags |= DF_DYING;
            }
        }
        lpopen = lpopen->nextptr;
    }
    LeaveCriticalSection(&g_devcs);
    return TRUE;
}

fsdev_t * DeviceContent::InitDeviceContent( LPCTSTR   RegPath, LPCREGINI lpReg, DWORD cReg, void const*const lpvParam )
{
    DWORD status, dwId, dwLegacyIndex;
    TCHAR ActivePath[REG_PATH_LEN];
    RegActivationValues_t av;
    BusDriverValues_t bdv;
    HKEY hkActive = NULL;
    
    DEBUGMSG(ZONE_ACTIVE,(TEXT("DEVICE!I_ActivateDeviceEx: starting HLM\\%s\r\n"), RegPath));
    // Read activation values from the device key
    status = ::RegReadActivationValues(RegPath, &av);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR, (
            L"DEVICE!I_ActivateDeviceEx: can't find all required activation "
            L"values in '%s'\r\n",
            RegPath
        ));
        SetLastError(ERROR_BAD_CONFIGURATION);
        return NULL;
    }
    
    // are we supposed to load the driver?
    if (av.Flags & DEVFLAGS_NOLOAD) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: not loading %s with DEVFLAGS_NOLOAD\n"),
            RegPath));
        // not supposed to load the driver, return NULL with ERROR_SUCCESS
        SetLastError(ERROR_SUCCESS);
        return NULL;
    }
    if ((av.Flags & DEVFLAGS_BOOTPHASE_1) && (g_BootPhase > 1)) { // BOOTPHASE_1=loaded in phase 1 and never again
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: skipping %s in BootPhase %d\n"),
            RegPath, g_BootPhase));
        // wrong boot phase for this driver, return NULL with ERROR_SUCCESS
        SetLastError(ERROR_SUCCESS);
        return NULL;
    }

    if( (status= CreateActiveKey(ActivePath, _countof(ActivePath), &dwId, &hkActive)) == ERROR_SUCCESS &&
         (status= InitializeActiveKey(hkActive, RegPath, lpReg, cReg)) == ERROR_SUCCESS &&
         (status = ReadBusDriverValues(hkActive, &bdv)) == ERROR_SUCCESS ) {
        DEBUGCHK(hkActive != NULL);
        DEBUGCHK(ActivePath[0] != 0);
        DEBUGCHK(m_ActivationPath==NULL);
        ActivePath[REG_PATH_LEN-1] = 0;
        DWORD dwUnit = _tcslen(ActivePath)+ 1;
        m_ActivationPath = new TCHAR [dwUnit ];
        if (m_ActivationPath && SUCCEEDED(StringCchCopy(m_ActivationPath, dwUnit,ActivePath ))) {
            BOOL fIsitUniq = TRUE;

            EnterCriticalSection(&g_devcs);
            // Is this a named device?
            dwLegacyIndex = av.Index;
            if(av.Prefix[0] == 0) {
                // Device is not accessible via the $device or legacy namespaces -- check 
                // support for the $bus namespace.  It needs to have a bus name provided
                // by the bus driver and a prefix that will allow us to get entry points to
                // create handles.
                if(bdv.szBusName[0]) {
                    fIsitUniq = IsThisDeviceUnique(av.Prefix, av.Index, bdv.szBusName);
                }
            } else {
                // this is a named device, was an index specified?
                fIsitUniq = FALSE;
                if(av.Index != -1) {
                    // yes, make sure there are no duplicates
                    fIsitUniq = IsThisDeviceUnique(av.Prefix, av.Index, bdv.szBusName);
                } else {
                    // Create a new index and allocate a candidate device to reserve the 
                    // name/index combination.  Start at index 1 and count up.  Index 10 will
                    // appear as instance index 0 in the legacy namespace.
                    for(av.Index = 1; av.Index < MAXDEVICEINDEX; av.Index++) {
                        if ((fIsitUniq = IsThisDeviceUnique(av.Prefix, av.Index, bdv.szBusName)) == TRUE) {
                            if(av.Index == 10) {
                                dwLegacyIndex = 0;
                            } else {
                                dwLegacyIndex = av.Index;
                            }
                            break;
                        }
                    }
                }
            }

            if (fIsitUniq) {

                status = CreateContent(av.Prefix, av.Index, dwLegacyIndex, dwId, av.DevDll, av.Flags,
                    av.BusPrefix, bdv.szBusName, RegPath, bdv.hParent);

                m_PostCode = (av.bHasPostInitCode? av.PostInitCode: 0) ;
                m_BusPostCode = (av.bHasBusPostInitCode? av.BusPostInitCode: 0 );

                InsertTailList(&g_DevCandidateChain, (PLIST_ENTRY)(fsdev_t *)this);
            }
            else {
                status = ERROR_DEVICE_IN_USE;
            }
            LeaveCriticalSection(&g_devcs);

            if (status == ERROR_SUCCESS) {
                status = FinalizeActiveKey(hkActive, this);
                if (status != ERROR_SUCCESS) {
                    RemoveEntryLockedList((PLIST_ENTRY)(fsdev_t *)this, &g_devcs);
                }
            }
        }
    }
    if (hkActive) {
        // close the registry key before passing its path to the device driver
        RegCloseKey(hkActive);
        hkActive = NULL;
    }
    if (status == ERROR_SUCCESS) {
        m_DeviceStatus = DEVICE_DDRIVER_STATUS_DISABLED;
        m_dwActivateInfo = (av.bOverrideContext == FALSE?(DWORD)m_ActivationPath : av.Context);
        m_dwActivationParam = (DWORD)lpvParam;
        if ((av.Filter[0]!=0) && ((dwFlags & DEVFLAGS_LOAD_AS_USERPROC)==0) ) { // for kernel mode Filter.
            VERIFY(CreateDriverFilter(av.Filter));
        }
        return this;
    }
    else {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR, (TEXT("DEVICE!InitDeviceContent: can not Init Device content for '%s,error code %d'\r\n"),
            RegPath,status));
        SetLastError(status);
        //ASSERT(FALSE);
        return NULL;
    }
    
}
BOOL DeviceContent::InsertToDmList()
{
    BOOL fNotInList = TRUE;
    EnterCriticalSection (&g_devcs);
    if (pszDeviceName != NULL && FindDeviceByName(pszDeviceName, NtDevice)) {
        fNotInList = FALSE;
    }
    if (fNotInList && pszBusName != NULL && FindDeviceByName(pszBusName,NtBus)) {
        fNotInList = FALSE;
    }
    if (fNotInList) {
        EnterCriticalSection(&g_devcs);
        RemoveEntryList((PLIST_ENTRY)(fsdev_t *)this);
        InsertTailList(&g_DevChain, (PLIST_ENTRY)(fsdev_t *)this);
        LeaveCriticalSection(&g_devcs);

        AddDeviceRef(this);
    }
    LeaveCriticalSection (&g_devcs);
    return fNotInList;
}
#pragma prefast(disable: 25053, "cast type 'pOpenFn' and function 'DevFileNotSupportedBool' have different parameter count.")
DWORD DeviceContent::LoadLib(LPCTSTR RegPath)
{
    DWORD dwStatus = ERROR_SUCCESS;
    if ((dwFlags & DEVFLAGS_LOAD_AS_USERPROC)) {
        if (g_BootPhase<2) {
            ASSERT(FALSE);
            DEBUGMSG(ZONE_ERROR, (_T("DEVICE!CreateDevice: Cannot load user mode drivers before boot phase 2.\r\n")));
            dwStatus = ERROR_BAD_CONFIGURATION;
        } else if (!(this->dwDataEx)) {
            this->hLib = NULL;
            this->dwDataEx  = Reflector_Create(this->pszDeviceKey, m_Prefix, m_DevDll, dwFlags );
            if (this->dwDataEx != 0 ) {
                this->fnInit = NULL;
                this->fnInitEx = (pInitExFn)Reflector_InitEx;
                this->fnPreDeinit = (pPreDeinitFn)Reflector_PreDeinit;
                this->fnDeinit = (pDeinitFn)Reflector_Deinit;
                this->fnOpen = (pOpenFn)Reflector_Open;
                this->fnPreClose = (pPreCloseFn)Reflector_PreClose;
                this->fnClose = (pCloseFn)Reflector_Close;
                this->fnRead = (pReadFn)Reflector_Read;
                this->fnWrite = (pWriteFn)Reflector_Write;
                this->fnSeek = (pSeekFn)Reflector_SeekFn;
                this->fnControl = (pControlFn)Reflector_Control;
                this->fnPowerup = (pPowerupFn)Reflector_Powerup;
                this->fnPowerdn = (pPowerdnFn)Reflector_Powerdn;
                this->fnCancelIo = (pCancelIoFn)Reflector_CancelIo;
            } 
            else {
                DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: couldn't load(%s) to user mode!!\r\n"),m_DevDll));
                dwStatus = ERROR_FILE_NOT_FOUND;
            }
        }
    }
    else {
        if (!(this->hLib )){
            LPCTSTR pEffType = m_Prefix;
            LPCTSTR lpszLib =  m_DevDll;
            DEBUGMSG(ZONE_ACTIVE, (_T("DEVICE!CreateDevice: loading driver DLL '%s'\r\n"), lpszLib));
            this->hLib =
                (dwFlags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(lpszLib) : LoadDriver(lpszLib);
            if (!this->hLib) {
                DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: couldn't load '%s' -- error %d\r\n"), 
                    lpszLib, GetLastError()));
                dwStatus = ERROR_FILE_NOT_FOUND;
            } else {    
                this->fnInitEx = NULL;
                this->dwDataEx = 0;
                this->fnInit = (pInitFn)GetDMProcAddr(pEffType,L"Init",this->hLib);
                this->fnPreDeinit = (pPreDeinitFn)GetDMProcAddr(pEffType,L"PreDeinit",this->hLib);
                this->fnDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"Deinit",this->hLib);
                this->fnOpen = (pOpenFn)GetDMProcAddr(pEffType,L"Open",this->hLib);
                this->fnPreClose = (pPreCloseFn)GetDMProcAddr(pEffType,L"PreClose",this->hLib);
                this->fnClose = (pCloseFn)GetDMProcAddr(pEffType,L"Close",this->hLib);
                this->fnRead = (pReadFn)GetDMProcAddr(pEffType,L"Read",this->hLib);
                this->fnWrite = (pWriteFn)GetDMProcAddr(pEffType,L"Write",this->hLib);
                this->fnSeek = (pSeekFn)GetDMProcAddr(pEffType,L"Seek",this->hLib);
                this->fnControl = (pControlFn)GetDMProcAddr(pEffType,L"IOControl",this->hLib);
                this->fnPowerup = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerUp",this->hLib);
                this->fnPowerdn = (pPowerdnFn)GetDMProcAddr(pEffType,L"PowerDown",this->hLib);
                this->fnCancelIo = (pCancelIoFn)GetDMProcAddr(pEffType,L"Cancel",this->hLib);

                // Make sure that the driver has an init and deinit routine.  If it is named,
                // it must have open and close, plus at least one of the I/O routines (read, write
                // ioctl, and/or seek).  If a named driver has a pre-close routine, it must also 
                // have a pre-deinit routine.
                if (!(this->fnInit && this->fnDeinit) ||
                    this->pszDeviceName != NULL && (!this->fnOpen || 
                                 !this->fnClose ||
                                 (!this->fnRead && !this->fnWrite &&
                                  !this->fnSeek && !this->fnControl) ||
                                 (this->fnPreClose && !this->fnPreDeinit))) {
                    DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: illegal entry point combination in driver DLL '%s'\r\n"), 
                        lpszLib));
                    dwStatus = ERROR_INVALID_FUNCTION;
                }

                if (!this->fnOpen) this->fnOpen = (pOpenFn) DevFileNotSupportedBool;
                if (!this->fnClose) this->fnClose = (pCloseFn) DevFileNotSupportedBool;
                if (!this->fnControl) this->fnControl = (pControlFn) DevFileNotSupportedBool;
                if (!this->fnRead) this->fnRead = (pReadFn) DevFileNotSupportedDword;
                if (!this->fnWrite) this->fnWrite = (pWriteFn) DevFileNotSupportedDword;
                if (!this->fnSeek) this->fnSeek = (pSeekFn) DevFileNotSupportedDword;

                //initialize filter drivers
                if (dwStatus == ERROR_SUCCESS) 
                    dwStatus = InitDriverFilters(RegPath);
            }
        }
    }

    if (dwStatus != ERROR_SUCCESS) {
        RETAILMSG(1, (
            L"ERROR! DEVMGR: Failed to load '%s'. dwStatus=0x%x\r\n",
            RegPath, dwStatus
        ));
    }
        
    return dwStatus;
}
BOOL DeviceContent::EnableDevice(DWORD dwWaitTicks)
{
    DWORD retval = ERROR_SUCCESS ;
    if (WaitForSingleObject( m_hComplete, dwWaitTicks) == WAIT_OBJECT_0) {
        Lock();
        BOOL fEnable = FALSE;
        if (m_DeviceStatus == DEVICE_DDRIVER_STATUS_DISABLED) {
            ResetEvent(m_hComplete);
            m_DeviceStatus = DEVICE_DDRIVER_STATUS_INITIALIZING ;
            fEnable = TRUE;
        }
        Unlock();
        if (fEnable ) {
            // call the driver's init entry point
            LPEXCEPTION_POINTERS pep;
            __try {
                DEBUGMSG(ZONE_ACTIVE, (_T("DEVICE!LaunchDevice: calling Init() for device 0x%08x\r\n"), this));
                if (this->fnInit!=NULL)
                    this->dwData = DriverInit(m_dwActivateInfo,(LPVOID)m_dwActivationParam);
                else if (this->fnInitEx!=NULL)
                    this->dwData = this->fnInitEx(this->dwDataEx,m_dwActivateInfo,(LPVOID)m_dwActivationParam);
                else
                    this->dwData = 0 ;
                if (!(this->dwData)) {
                    retval = ERROR_OPEN_FAILED;
                }
            } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG(ZONE_WARNING, (_T("DEVICE!LaunchDevice: exception in Init for device 0x%08x\r\n"), this));
                retval = ERROR_INVALID_PARAMETER;
                this->dwData = 0 ;
            }
            Lock();
            if (retval == ERROR_SUCCESS  && this->dwData!= 0) {
                this->wFlags &= ~DF_DYING;
                m_DeviceStatus = DEVICE_DDRIVER_STATUS_ENABLE ;
                // Are we supposed to unload this driver right away?  If so, the caller will 
                // free its resources.
            }
            else {
                m_DeviceStatus = DEVICE_DDRIVER_STATUS_DISABLED;
                this->dwData = 0;
            }
            Unlock();
            SetEvent(m_hComplete);
            if (retval == ERROR_SUCCESS && this->dwId != ID_REGISTERED_DEVICE) {
                // advertise the device's existence to the outside world
                PnpAdvertiseInterfaces(this);
                
                // Only stream interfaces can use receive ioctls.
                if (m_PostCode) {
                    WCHAR szDeviceName[64];
                    // Call the new device's IOCTL_DEVICE_INITIALIZED 
                    DEBUGCHK(this->pszDeviceName != NULL);
                    DEBUGCHK(this->pszDeviceName[0] != 0);
                    VERIFY(SUCCEEDED(StringCchPrintf(szDeviceName, 64, L"$device\\%s", this->pszDeviceName)));
                    DevicePostInit(szDeviceName, m_PostCode, (fsdev_t * )this, this->pszDeviceKey );
                }
                if (m_BusPostCode) { // This has bus PostInitCode.
                    WCHAR szDeviceName[64];
                    VERIFY(SUCCEEDED(StringCchPrintf(szDeviceName, 64 , L"$bus\\%s", this->pszBusName)));
                    DEBUGCHK(wcslen(this->pszBusName)<64-6);
                    DevicePostInit(szDeviceName, m_BusPostCode, (fsdev_t * )this, this->pszDeviceKey);
                }
            }
        }
        else if (m_DeviceStatus!=DEVICE_DDRIVER_STATUS_ENABLE ){
            retval = ERROR_BUSY;
        };
    }
    else { // Timeout.
        retval = WAIT_TIMEOUT ;
    }
    if ( retval != ERROR_SUCCESS) {
        SetLastError(retval);
        return FALSE;
    }
    else
        return TRUE;
}
BOOL DeviceContent::DisableDevice(DWORD dwWaitTicks)
{
    DWORD retval = ERROR_SUCCESS ;
    if (WaitForSingleObject( m_hComplete, dwWaitTicks) == WAIT_OBJECT_0) {
        Lock();
        BOOL fDisable = FALSE;
        if (AddRefToAccess()) {
            ResetEvent(m_hComplete);
            m_DeviceStatus = DEVICE_DDRIVER_STATUS_DISABLING;
            fDisable = TRUE;
        }
        Unlock();
        if (fDisable) {
            RemoveDeviceOpenRef();

            PnpDeadvertiseInterfaces(this);

            LPEXCEPTION_POINTERS pep;
            __try {
                if(this->fnPreDeinit != NULL) {
                    DriverPreDeinit(this->dwData);
                } else {
                    LONG ldwData = InterlockedExchange( (LPLONG)&(this->dwData) , 0 );
                    if (ldwData) {
                        DriverDeinit((DWORD)ldwData);
                    }
                }
            }
            __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG(ZONE_WARNING, (_T("I_DeregisterDevice: exception in deinit entry point\r\n")));
            }
            DeRefAccess() ;
        };
    }
    else { // Timeout.
        retval = WAIT_TIMEOUT ;
    }
    if ( retval != ERROR_SUCCESS) {
        SetLastError(retval);
        return FALSE;
    }
    else
        return TRUE;
}
DeviceContent * DeviceContent::AddRefToAccess() 
{
    DeviceContent * pReturn = NULL;
    Lock();
    if (m_DeviceStatus == DEVICE_DDRIVER_STATUS_ENABLE) {
        AddRef();
        this->dwRefCnt++;
        pReturn = this;
    }
    Unlock();
    return pReturn;
}
VOID    DeviceContent::DeRefAccess() 
{
    Lock();
    BOOL fDeinit = FALSE;
    DEBUGCHK(dwRefCnt!=0);
    dwRefCnt--;
    if (!dwRefCnt && m_DeviceStatus == DEVICE_DDRIVER_STATUS_DISABLING ) {
        fDeinit = TRUE ;
    }
    Unlock();
    if (fDeinit) {
        LONG ldwData = InterlockedExchange( (LPLONG)&(this->dwData) , 0 );
        if (ldwData) {
            LPEXCEPTION_POINTERS pep;
            __try {
                DriverDeinit(ldwData);
            }
            __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                DEBUGMSG(ZONE_WARNING, (_T("I_DeregisterDevice: exception in final deinit entry point\r\n")));
            }
        }
        m_DeviceStatus = DEVICE_DDRIVER_STATUS_DISABLED ;
        RemoveDeviceOpenRef(TRUE);
        SetEvent(m_hComplete);                
    }
    DeRef();
    
}
DWORD DeviceContent::CreateContent(
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
)
{
    DWORD dwSize;
    DWORD dwStatus = ERROR_SUCCESS;
    WCHAR szDeviceName[MAXDEVICENAME];
    WCHAR szLegacyName[MAXDEVICENAME];

    DEBUGCHK(lpszPrefix != NULL);
    DEBUGCHK(lpszLib != NULL);
    DEBUGCHK(wcslen(lpszPrefix) <= 3);
    DEBUGCHK(dwLegacyIndex == dwIndex || (dwLegacyIndex == 0 && dwIndex == 10));
    DEBUGCHK(lpszBusName != NULL);

    // figure out how much memory to allocate
    dwSize = 0;
    m_DeviceNameString = NULL;
    this->dwActivateId = GetCallerProcessId();
    m_dwDevIndex = dwIndex;
    if (lpszPrefix==NULL  || !SUCCEEDED(StringCchCopy(m_AccessPreFix,_countof(m_AccessPreFix),lpszPrefix))) {
        m_AccessPreFix[0] = 0;
    }
    
    // is the device named?
    if(lpszPrefix[0] == 0) {
        // unnamed device
        szDeviceName[0] = 0;
    } else {
        // named device, allocate room for its names
        VERIFY(SUCCEEDED(StringCchPrintf(szDeviceName,MAXDEVICENAME,TEXT("%s%u"), lpszPrefix, dwIndex)));
        if(dwLegacyIndex <= 9) {
            // allocate room for name and null
            VERIFY(SUCCEEDED(StringCchPrintf(szLegacyName, MAXDEVICENAME, L"%s%u:", lpszPrefix, dwLegacyIndex)));
            dwSize += (wcslen(szLegacyName) + 1);
        }

        // allocate room for name and null        
        dwSize += (wcslen(szDeviceName) + 1) ;
    }

    // If the bus driver didn't allocate a name the device may still support the 
    // bus name interface -- use its device name (if present) just in case.
    if(lpszBusName[0] == 0 && szDeviceName[0] != 0) {
        lpszBusName = szDeviceName;
    }
    
    // allocate room for the bus name
    if(lpszBusName[0] != 0) {
        dwSize += (wcslen(lpszBusName) + 1) ;
    }

    // make room to store the device key as well
    if(lpszDeviceKey != NULL) {
        dwSize += (wcslen(lpszDeviceKey) + 1) ;
    }
    
    // allocate the structure
    ASSERT(m_DeviceNameString == NULL);
    m_DeviceNameString = new TCHAR [dwSize];
    if (!m_DeviceNameString) {
        DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: couldn't allocate device structure\r\n")));
        dwStatus = ERROR_OUTOFMEMORY;
    } else {
        LPCWSTR pEffType = NULL;
        LPTSTR psz = m_DeviceNameString;
        DWORD cchSizeLeft = dwSize;
        this->dwId = dwId;
        this->wFlags = 0;            
        this->dwFlags = dwFlags;
        //TODO: Security check for using hParent 
        this->hParent = hParent;
        if (lpszPrefix[0] != 0) {
            if(dwLegacyIndex <= 9) {
                this->pszLegacyName = psz;
                VERIFY(SUCCEEDED(StringCchCopy(this->pszLegacyName, cchSizeLeft, szLegacyName)));
                cchSizeLeft -= wcslen(this->pszLegacyName) + 1;
                psz += wcslen(this->pszLegacyName) + 1;
            }
            this->pszDeviceName = psz;
            VERIFY(SUCCEEDED(StringCchCopy(this->pszDeviceName, cchSizeLeft, szDeviceName)));
            cchSizeLeft -= wcslen(this->pszDeviceName) + 1;
            psz += wcslen(this->pszDeviceName) + 1;
        }
        if(lpszBusName[0] != 0) {
            this->pszBusName = psz;
            VERIFY(SUCCEEDED(StringCchCopy(this->pszBusName, cchSizeLeft, lpszBusName)));
            cchSizeLeft -= wcslen(lpszBusName) + 1;
            psz += wcslen(lpszBusName) + 1;
        }
        if(lpszDeviceKey != NULL) {
            this->pszDeviceKey= psz;
            VERIFY(SUCCEEDED(StringCchCopy(this->pszDeviceKey, cchSizeLeft, lpszDeviceKey)));
            cchSizeLeft -= wcslen(lpszDeviceKey) + 1;
            psz += wcslen(lpszDeviceKey) + 1;
        }
        if((dwFlags & DEVFLAGS_NAKEDENTRIES) == 0) {
            if(lpszPrefix[0] != 0) {
                DEBUGCHK(lpszBusPrefix[0] == 0 || wcsicmp(lpszBusPrefix, lpszPrefix) == 0);
                pEffType = lpszPrefix;      // use standard prefix decoration
            } else if(lpszBusPrefix[0] != 0 && this->pszBusName != NULL) {
                pEffType = lpszBusPrefix;   // no standard prefix, use bus prefix decoration
            } else {
                if(this->pszDeviceName != NULL) {
                    // device is expected to have a device or bus name, but we don't know
                    // how to look for its entry points
                    DEBUGMSG(ZONE_ACTIVE || ZONE_ERROR, 
                        (_T("DEVICE!CreateDevice: no entry point information for '%s' can't load '%s'\r\n"), 
                        lpszLib, this->pszDeviceName));
                    dwStatus = ERROR_INVALID_FUNCTION;
                }
            }
        }
        if (lpszLib==NULL || !SUCCEEDED(StringCchCopy(m_DevDll,_countof(m_DevDll), lpszLib ))) {
            m_DevDll[0] = 0 ;
        }
        if (pEffType==NULL || !SUCCEEDED(StringCchCopy(m_Prefix,_countof(m_Prefix), pEffType ))) {
            m_Prefix[0] = 0 ;
        }

        if ((dwFlags & DEVFLAGS_SAME_AS_CALLER)) {
            if (GetCurrentProcessId()==GetDirectCallerProcessId())  // Kernel mode
                dwFlags &= ~DEVFLAGS_LOAD_AS_USERPROC;
            else
                dwFlags |= DEVFLAGS_LOAD_AS_USERPROC;
        }
        this->hLib = NULL;
        this->dwDataEx = 0 ;
        this->fnInit = NULL;
    }
    return dwStatus;
    // did everything go ok?
}
BOOL DeviceContent::IsThisDeviceUnique(WCHAR const * const pszPrefix, DWORD dwIndex, LPCWSTR pszBusName)
{
    WCHAR szDeviceName[MAXDEVICENAME];
    BOOL fDuplicate = FALSE;
    fsdev_t * pDevice = NULL ;
    
    DEBUGCHK(pszPrefix != NULL);
    DEBUGCHK(pszBusName != NULL);

    // format the device name
    if(pszPrefix[0] != 0) 
        VERIFY(SUCCEEDED(StringCchPrintf(szDeviceName,MAXDEVICENAME,TEXT("%s%u"), pszPrefix, dwIndex)));
    else
        szDeviceName[0] = 0;

    // if the device has a standard name, check for conflicts
    if(pszPrefix[0] != 0) {
        DEBUGCHK(wcslen(pszPrefix) == PREFIX_CHARS);
        if((pDevice = FindDeviceByName(szDeviceName, NtDevice)) != NULL) {
            fDuplicate = TRUE;
            DeDeviceRef(pDevice);
        }
        if((pDevice = FindDeviceCandidateByName(szDeviceName, NtDevice)) != NULL) {
            fDuplicate = TRUE;
            DeDeviceRef(pDevice);
        }
    }

    // if the device has a bus name, check for conflicts
    if(!fDuplicate) {
        if(pszBusName[0] != 0) {
            if((pDevice = FindDeviceByName(pszBusName, NtBus)) != NULL) {
                fDuplicate = TRUE;
                DeDeviceRef(pDevice);
            }
        }
    }

    return !fDuplicate;
}
