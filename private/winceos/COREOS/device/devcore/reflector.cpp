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
// This module is implementing User Mode Driver reflector.
//

#include <windows.h>
#include <devload.h>
#include "devzones.h"
#include <reflector.h>
#include <errorrep.h>
#include <giisr.h>
#include "Promgr.hpp"
#include "reflserv.hpp"
#include "iopck.hpp"

BOOL IsReflectValid(CReflector *pReflect)
{
    BOOL bReturn = FALSE;
    if (pReflect) {
        UserDriverProcessor * pUserDriverProc = FindUserProcBy(pReflect->GetUDP());
        if (pUserDriverProc) {
            CReflector *pExistReflect = pUserDriverProc->FindReflectorBy(pReflect);
            if (pExistReflect) {
                bReturn = TRUE;
                ASSERT(pExistReflect == pReflect) ;
                pExistReflect->DeRef();
            }
            pUserDriverProc->DeRef();
        }
    }
    return bReturn;
}

// Determine whether the registry key is under HKLM\Services\; i.e. should
// this be handled by services.exe?
extern "C" BOOL IsServicesRegKey(LPCWSTR lpszRegKeyName)
{
    static const WCHAR servicesBaseRegKey[] = L"services\\";
    return (0 == _wcsnicmp(lpszRegKeyName,servicesBaseRegKey,
                 (sizeof(servicesBaseRegKey)/sizeof(servicesBaseRegKey[0]))-1));
}


BOOL DeleteReflector(CReflector *pReflect) 
{
    BOOL bReturn = FALSE;
    if (pReflect) {
        UserDriverProcessor * pUserDriverProc = FindUserProcBy(pReflect->GetUDP());
        if (pUserDriverProc) {
            pUserDriverProc->DeleteReflector(pReflect);
            pUserDriverProc->DeRef();
            bReturn = TRUE;
        }
    }
    return bReturn;
}
CReflector * CreateReflector(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags )
{
    CRegistryEdit m_DeviceKey (HKEY_LOCAL_MACHINE, lpszDeviceKey, KEY_READ);

    DWORD dwUserProcGroupID = 0;
    LPCTSTR lpAccountId = NULL;
    CReflector * pReturn = NULL;
    UserDriverProcessor * pUserDriverProc = NULL;
    DWORD dwRetry = 2;
    if (dwFlags &DEVFLAGS_SAME_AS_CALLER) {
        pUserDriverProc = g_pUDPContainer->FindUserProcByProcID(GetCallerProcessId());
        if (pUserDriverProc) {
            pReturn = pUserDriverProc->CreateReflector(lpszDeviceKey, lpPreFix, lpDrvName,dwFlags);
            pUserDriverProc->DeRef();   
        }
    }
    else
    do {
        if (m_DeviceKey.IsKeyOpened() && m_DeviceKey.GetRegValue(DEVLOAD_USERPROCGROUP_VALNAME,(PBYTE)&dwUserProcGroupID, sizeof(dwUserProcGroupID))) {
            pUserDriverProc = FindUserProcByID(dwUserProcGroupID, NULL, TRUE);
        }
        else {
            if (!lpAccountId) {
                lpAccountId = UD_DEFAULT_ACCOUNTSID;
            }

            if ((dwFlags & DEVFLAGS_LOADINOWNGROUP) == 0) {
                // try to find an existing driver host for this accountsid
                pUserDriverProc = FindUserProcByAccount(lpAccountId, IsServicesRegKey(lpszDeviceKey));
            }

            if (!pUserDriverProc) {
                // create a new user-mode driver host process (udevice)
                pUserDriverProc = CreateUserProc(lpAccountId);
                if (pUserDriverProc)
                    pUserDriverProc->AddRef();
            }
            ASSERT(pUserDriverProc);
        };
        if (pUserDriverProc) {
            pReturn = pUserDriverProc->CreateReflector(lpszDeviceKey,lpPreFix, lpDrvName,dwFlags);
            pUserDriverProc->DeRef();   
        }
        if (pReturn==NULL)
            Sleep(1000);
    } while (dwRetry-- != 0 && pReturn == NULL);
    DEBUGMSG(ZONE_WARNING && pReturn==NULL,(L"CreateReflector : failed to create refelctor object"));
    return pReturn;
}

CReflector::CReflector(UserDriverProcessor * pUDP,LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName,DWORD dwFlags, CReflector * pNext)
:   m_pUDP(pUDP)
,   m_pNextReflector(pNext)
{
    m_pFileFolderList = NULL;
    m_pPhysicalMemoryWindowList = NULL;
    m_dwData = 0 ;
    m_fUserInit = FALSE ;
    m_hInterruptHandle = NULL;
    m_hIsrHandle = NULL;
    m_hUDriver = INVALID_HANDLE_VALUE ;
    m_DdkIsrInfo.cbSize = sizeof(m_DdkIsrInfo);
    m_DdkIsrInfo.dwSysintr = 0;
    m_DdkIsrInfo.dwIrq = 0;
    
    if (m_pUDP) {
        m_pUDP->AddRef();
        FNDRIVERLOAD_PARAM fnDriverLoad;
        fnDriverLoad.dwAccessKey = (DWORD)this;
        fnDriverLoad.dwFlags = dwFlags;
        BOOL fCpyOk = FALSE;
        __try {
            if (lpPreFix==NULL) { // Special case. for naked entry.
                fnDriverLoad.Prefix[0] = 0 ;
                fCpyOk = TRUE;
            }
            else 
                fCpyOk =SUCCEEDED(StringCbCopy(fnDriverLoad.Prefix,sizeof(fnDriverLoad.Prefix),lpPreFix));
            
            fCpyOk =(fCpyOk && SUCCEEDED(StringCbCopy(fnDriverLoad.DriverName,sizeof(fnDriverLoad.DriverName),lpDrvName)));
            fCpyOk =(fCpyOk && SUCCEEDED(StringCbCopy(fnDriverLoad.DeviceRegPath,sizeof(fnDriverLoad.DeviceRegPath),lpszDeviceKey)));
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fCpyOk = FALSE ;
        }

        if (fCpyOk) {
            FNDRIVERLOAD_RETURN driversReturn;
            driversReturn.dwDriverContext = 0 ;
            driversReturn.hDriversAccessHandle = INVALID_HANDLE_VALUE ;
            BOOL bRet = FnDriverLoad(fnDriverLoad,driversReturn);
            if (bRet) {
                m_dwData = driversReturn.dwDriverContext;
                if (driversReturn.hDriversAccessHandle!=NULL && driversReturn.hDriversAccessHandle!= INVALID_HANDLE_VALUE && m_hUDriver==INVALID_HANDLE_VALUE) {
                    bRet = DuplicateHandle((HANDLE)m_pUDP->GetUserDriverProcessorInfo().dwProcessId,driversReturn.hDriversAccessHandle,
                            GetCurrentProcess(),&m_hUDriver,
                            0,FALSE,DUPLICATE_SAME_ACCESS);
                    if (!bRet || m_hUDriver == 0 || m_hUDriver == INVALID_HANDLE_VALUE) { 
                        ASSERT(FALSE);
                        m_hUDriver = INVALID_HANDLE_VALUE;
                    }
                }
            }
            DEBUGMSG(ZONE_WARNING && !bRet,(L"CReflector: FnDriverLoad return FALSE!"));
        }
    }
}
CReflector::~CReflector()
{
    ASSERT(m_hInterruptHandle == NULL);
    ASSERT(m_pPhysicalMemoryWindowList == NULL);
    ASSERT(m_hIsrHandle == NULL);
    ASSERT(m_pFileFolderList == NULL);
    
    if (m_pUDP) {
        Lock();
        if (m_dwData) {
            VERIFY(FnDriverExit(m_dwData));
        }
        Unlock();
        m_pUDP->DeRef();
    }
    if (m_hUDriver !=NULL && m_hUDriver!=INVALID_HANDLE_VALUE) {
        CloseHandle(m_hUDriver);
        m_hUDriver = INVALID_HANDLE_VALUE ;
    }
}
BOOL CReflector::InitEx(DWORD dwInfo, LPVOID lpvParam)
{
    BOOL bRet = FALSE;

    Lock();

    if (m_pUDP) {
        FNINIT_PARAM fnInitParam;
        fnInitParam.dwCallerProcessId = GetCallerProcessId();
        fnInitParam.dwDriverContent = m_dwData;
        // Findout the m_dwInfo is Activete Registry or not.
        
        CRegistryEdit activeReg(HKEY_LOCAL_MACHINE, (LPCTSTR)dwInfo, KEY_ALL_ACCESS);

        if (activeReg.IsKeyOpened() &&
                (SUCCEEDED(StringCbCopy(fnInitParam.ActiveRegistry,sizeof(fnInitParam.ActiveRegistry),(LPCTSTR)dwInfo)))) {
            HANDLE hServiceHandle = REFL_CreateDeviceServiceHandle(this);
            if (hServiceHandle !=NULL ) {
                HANDLE hClientHandle = NULL;
                BOOL fResult = DuplicateHandle( GetCurrentProcess(),hServiceHandle,
                        (HANDLE)m_pUDP->GetUserDriverProcessorInfo().dwProcessId,
                        &hClientHandle, 0,FALSE,DUPLICATE_SAME_ACCESS);
                ASSERT(fResult);
                if (fResult) {
                    DWORD dwAccessKey = (DWORD)hClientHandle;
                    VERIFY(activeReg.RegSetValueEx(
                            DEVLOAD_UDRIVER_REF_HANDLE_VALNAME, 
                            DEVLOAD_UDRIVER_REF_HANDLE_VALTYPE,
                            (PBYTE)&dwAccessKey,sizeof(dwAccessKey)));
                }
                CloseHandle(hServiceHandle);
            }
            // It is copies Registry Correctly.
            fnInitParam.dwInfo = NULL;
        }
        else {
            fnInitParam.dwInfo = dwInfo;
        }
        CRegistryEdit deviceReg((LPCTSTR)dwInfo);
        if (deviceReg.IsKeyOpened()) {
            deviceReg.GetIsrInfo(&m_DdkIsrInfo);
            DDKWINDOWINFO   dwi;
            HANDLE hParentBus = CreateBusAccessHandle((LPCTSTR)dwInfo);
            deviceReg.GetWindowInfo(&dwi);
            InitAccessWindow(dwi,hParentBus);
            if (hParentBus)
                CloseBusAccessHandle(hParentBus);
        }

        fnInitParam.lpvParam = lpvParam;
        bRet = FnInit(fnInitParam) ;
    }

    Unlock();
    DEBUGMSG(ZONE_WARNING && !bRet,(L"CReflector::InitEx:  return FALSE!"));
    return bRet;
}
BOOL CReflector::DeInit()
{
    VERIFY(FnDeInit(m_dwData));
    Lock();
    if (m_hInterruptHandle) {
        CloseHandle(m_hInterruptHandle);
        m_hInterruptHandle = NULL;
    }
    while (m_pPhysicalMemoryWindowList) {
        CPhysMemoryWindow * pNext = m_pPhysicalMemoryWindowList->GetNextObject();
        delete m_pPhysicalMemoryWindowList;
        m_pPhysicalMemoryWindowList = pNext;
    }
    RefFreeIntChainHandler( m_hIsrHandle ) ;

    while (m_pFileFolderList) {
        CFileFolder * pNext = m_pFileFolderList->GetNextObject();
        delete m_pFileFolderList;
        m_pFileFolderList = pNext;
    }
    Unlock();
    
    return TRUE;
}

CFileFolder* CReflector::Open(DWORD AccessCode, DWORD ShareMode)
{
    Lock();
    CFileFolder * fileFolder = new CFileFolder(this,AccessCode, ShareMode, m_pFileFolderList ) ;
    if (fileFolder && !fileFolder->Init()) {
        delete fileFolder;
        fileFolder = NULL;
    }
    if (fileFolder)
        m_pFileFolderList = fileFolder ;
    Unlock();
    return fileFolder ;
}
BOOL CReflector::Close (CFileFolder* pFileFolder)
{
    BOOL bRet = FALSE;
    Lock();
    if (pFileFolder!=NULL && m_pFileFolderList!=NULL) {
        if (pFileFolder == m_pFileFolderList) { // It is first.
            m_pFileFolderList = m_pFileFolderList->GetNextObject();
            delete pFileFolder; 
            bRet = TRUE;
        }
        else {
            CFileFolder * pPrev =  m_pFileFolderList;
            while (pPrev->GetNextObject()!=NULL) {
                if ( pPrev->GetNextObject() == pFileFolder) {
                    pPrev->SetNextObject(pFileFolder->GetNextObject());
                    delete pFileFolder;
                    bRet = TRUE;
                    break;
                }
                else
                    pPrev = pPrev->GetNextObject();
            }
        }
    }
    Unlock();
    ASSERT(bRet);
    return bRet;
}
CFileFolder * CReflector::FindFileFolderBy( CFileFolder * pFileFolder)
{
    CFileFolder * pReturn = NULL;
    if (pFileFolder!=NULL && m_pFileFolderList!=NULL) {
        pReturn = m_pFileFolderList;
        while(pReturn != NULL) {
            if (pReturn == pFileFolder) {
                break;
            }
            else {
                pReturn = pReturn->GetNextObject();
            }
        }
    }
    return pReturn;
}
BOOL CReflector::SendIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned) 
{    
    PREFAST_ASSERT(m_pUDP);
    DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
    UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = GetCallerProcessId();
    BOOL fReturn = FALSE;
    if (m_hUDriver != INVALID_HANDLE_VALUE) 
        fReturn = DeviceIoControl(m_hUDriver, dwIoControlCode,lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned,NULL);
    else
        fReturn = m_pUDP->SendIoControl(dwIoControlCode,lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned);
    UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;    
    return fReturn ;
};
BOOL CReflector::IsBufferShouldMapped(LPCVOID ptr,BOOL fWrite) 
{
    if (GetCallerVMProcessId ()!= GetUDP()->GetUserDriverProcessorInfo().dwProcessId) {
        return TRUE;
    }
    else if (fWrite) {
        return ((DWORD) ptr >= VM_SHARED_HEAP_BASE);
    } 
    else {
        return ((DWORD) ptr >= VM_KMODE_BASE);
    }
}

BOOL CReflector::ReflService( DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED /*lpOverlapped*/) 
{
    // Need Implememnt service.
    //hProc is only from Kernel.
    BOOL bRet = FALSE;
    SetLastError (ERROR_INVALID_PARAMETER);    
    switch (dwIoControlCode) {
      case IOCTL_REF_INTERRUPT_INITIALIZE:
        Lock();
        if (lpInBuf && nInBufSize>=sizeof(REF_INTERRUPTINITIALIZE_PARAM) && m_hInterruptHandle == NULL ) {
            PREF_INTERRUPTINITIALIZE_PARAM pRefIntrInitParam = (PREF_INTERRUPTINITIALIZE_PARAM)lpInBuf;
            BOOL fResult = FALSE;
            if (pRefIntrInitParam->idInt == m_DdkIsrInfo.dwSysintr) {
                fResult = DuplicateHandle( (HANDLE) GetCallerVMProcessId(),pRefIntrInitParam->hEvent,
                        GetCurrentProcess(),&m_hInterruptHandle,
                        0,FALSE,DUPLICATE_SAME_ACCESS);
                ASSERT(fResult);
            }
            if (!fResult) 
                m_hInterruptHandle = NULL;
            else if (pRefIntrInitParam->pvData && pRefIntrInitParam->cbData) {
                PVOID pMarshed = NULL;
                PVOID pSrcBuffer = pRefIntrInitParam->pvData;
                DWORD dwSrcSize = pRefIntrInitParam->cbData;
                HRESULT hResult = CeOpenCallerBuffer(&pMarshed, pSrcBuffer, dwSrcSize , ARG_I_BIT,TRUE);
                if (SUCCEEDED(hResult) && pMarshed!=NULL) {
                    bRet = InterruptInitialize(pRefIntrInitParam->idInt,m_hInterruptHandle,pMarshed, dwSrcSize);
                    hResult = CeCloseCallerBuffer(pMarshed, pSrcBuffer, dwSrcSize, ARG_I_BIT);
                    ASSERT(hResult!=S_OK);
                }
            }
            else {
                bRet = InterruptInitialize(pRefIntrInitParam->idInt,m_hInterruptHandle,NULL, 0);
            }
        }
        Unlock();
        break;
      case IOCTL_REF_INTERRUPT_DONE:
        if (lpInBuf && nInBufSize>=sizeof(DWORD) &&
                *(PDWORD)lpInBuf == m_DdkIsrInfo.dwSysintr) {
            InterruptDone(*(PDWORD)lpInBuf);
            bRet = TRUE;
        }
        break;
      case IOCTL_REF_INTERRUPT_DISABLE:
        Lock();
        if (lpInBuf && nInBufSize>=sizeof(DWORD) && m_hInterruptHandle &&
                *(PDWORD)lpInBuf == m_DdkIsrInfo.dwSysintr) {
            InterruptDisable(*(PDWORD)lpInBuf);;
            CloseHandle(m_hInterruptHandle);
            m_hInterruptHandle = NULL;
            bRet = TRUE;
        }
        Unlock();
        break;
      case IOCTL_REF_INTERRUPT_MASK:
        if (lpInBuf && nInBufSize>=sizeof(REF_INTERRUPT_MASK_PARAM) ) {
            PREF_INTERRUPT_MASK_PARAM pIntrMaskParam = (PREF_INTERRUPT_MASK_PARAM)lpInBuf;
            if (pIntrMaskParam->idInt == m_DdkIsrInfo.dwSysintr) {
                InterruptMask(pIntrMaskParam->idInt,pIntrMaskParam->fDisable) ;
                bRet = TRUE;
            };
        }
        break;
      case IOCTL_REF_VIRTUAL_COPY:
        if (lpInBuf && nInBufSize>=sizeof(REF_VIRTUALCOPY_PARAM)) {
            PREF_VIRTUALCOPY_PARAM pVirtualCopyParam = (PREF_VIRTUALCOPY_PARAM)lpInBuf;
            bRet = RefVirtualCopy(pVirtualCopyParam->lpvDest,pVirtualCopyParam->lpvSrc,pVirtualCopyParam->cbSize,pVirtualCopyParam->fdwProtect) ;
        }
        break;
      case IOCTL_REF_MNMAPIOSPACE:
        if (lpInBuf && nInBufSize>=sizeof(REF_MNMAPIOSPACE_PARAM) && lpOutBuf && nOutBufSize>=sizeof(PVOID)){
            PREF_MNMAPIOSPACE_PARAM prefMnMapIoSpaceParam =(PREF_MNMAPIOSPACE_PARAM) lpInBuf;
            PVOID pVirAddr = RefMmMapIoSpace(prefMnMapIoSpaceParam->PhysicalAddress, prefMnMapIoSpaceParam->NumberOfBytes, prefMnMapIoSpaceParam->CacheEnable);
            *(PVOID *)lpOutBuf = pVirAddr;
            bRet= (pVirAddr!=NULL);
            if (lpBytesReturned)
                *lpBytesReturned = sizeof(PVOID);
        }
        break;
      case IOCTL_REF_MNUNMAPIOSPACE:
        if (lpInBuf && nInBufSize>=sizeof(REF_MNUNMAPIOSPACE_PARAM) ) {
            PVOID BaseAddress = ((PREF_MNUNMAPIOSPACE_PARAM)lpInBuf)->BaseAddress ;
            VirtualFreeEx(GetCallerProcess(),(PVOID)((ULONG)BaseAddress & ~(ULONG)(PAGE_SIZE - 1)), 0, MEM_RELEASE);
            bRet = TRUE;
        }
        break;
      case IOCTL_REF_DUPLCATE_HANDLE:
        if (lpInBuf && nInBufSize >= sizeof(REF_DUPLICATE_HANDLE_PARAM) &&
                lpOutBuf && nOutBufSize >= sizeof(HANDLE) && 
                UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ]!=0) {
            PREF_DUPLICATE_HANDLE_PARAM pDupHandleParam =(PREF_DUPLICATE_HANDLE_PARAM) lpInBuf;
            ASSERT(GetUDP()->GetUserDriverProcessorInfo().dwProcessId == GetDirectCallerProcessId());
            return DuplicateHandle((HANDLE)UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ],
                pDupHandleParam->DirectCallerHandle,
                (HANDLE)GetUDP()->GetUserDriverProcessorInfo().dwProcessId,(LPHANDLE)lpOutBuf,
                pDupHandleParam->dwDesiredAccess, pDupHandleParam->bInheritHandle, pDupHandleParam->dwOptions);
        }
        break;
      case IOCTL_REF_LOAD_INT_CHAIN_HANDLER:
        if (lpInBuf && nInBufSize >= sizeof(REF_LOAD_INT_CHAIN_HANDLER) &&
                lpOutBuf && nOutBufSize >= sizeof(HANDLE)) {
            PREF_LOAD_INT_CHAIN_HANDLER pLoadIntChainHandle = (PREF_LOAD_INT_CHAIN_HANDLER) lpInBuf;
            HANDLE hIsr = RefLoadIntChainHandler(pLoadIntChainHandle->szIISRDll,pLoadIntChainHandle->szIISREntry,pLoadIntChainHandle->bIRQ);
            if (hIsr) {
                *(HANDLE *)lpOutBuf = hIsr ;
                bRet = TRUE;
            }
        }
        break;
      case IOCTL_REF_FREE_INT_CHAIN_HANDLER:
        if (lpInBuf && nInBufSize >= sizeof(HANDLE)) {
            bRet = RefFreeIntChainHandler(*(HANDLE *)lpInBuf);
        }
        break;
      case IOCTL_REF_CREATE_STATIC_MAPPING:
        if (lpInBuf && nInBufSize >= sizeof(REF_CREATE_STATIC_MAPPING) &&
                lpOutBuf && nOutBufSize>=sizeof(PVOID)) {
            PREF_CREATE_STATIC_MAPPING pCreateStaticMapping = (PREF_CREATE_STATIC_MAPPING) lpInBuf; 
            PVOID pResult = RefCreateStaticMapping(pCreateStaticMapping->dwPhysBase,pCreateStaticMapping->dwSize);
            if (pResult) {
                *(PVOID *)lpOutBuf = pResult;
                bRet = TRUE;
            }
            
        }
        break;
      case IOCTL_REF_INT_CHAIN_HANDLER_IOCONTROL:
        if (lpInBuf && nInBufSize >= sizeof(REF_INT_CHAIN_HANDLER_IOCONTROL)) {
            PREF_INT_CHAIN_HANDLER_IOCONTROL pIntChainIoControl = (PREF_INT_CHAIN_HANDLER_IOCONTROL)lpInBuf;
            PVOID pIoInput = NULL;
            if (pIntChainIoControl->lpInBuf && pIntChainIoControl->nInBufSize ) {
                if (!SUCCEEDED(CeOpenCallerBuffer(&pIoInput,pIntChainIoControl->lpInBuf,pIntChainIoControl->nInBufSize,ARG_I_PTR,FALSE))){
                    pIoInput = NULL;
                }                    
            }
            bRet = RefIntChainHandlerIoControl(pIntChainIoControl->hLib,pIntChainIoControl->dwIoControlCode,pIoInput,pIntChainIoControl->nInBufSize,
                lpOutBuf,nOutBufSize,lpBytesReturned ) ;
            if (pIoInput) {
                VERIFY(SUCCEEDED(CeCloseCallerBuffer(pIoInput,pIntChainIoControl->lpInBuf,pIntChainIoControl->nInBufSize,ARG_I_PTR)));
            }
        }
        break;
      case IOCTL_REF_CREATE_ASYNC_IO_HANDLE:
        if (lpOutBuf && nOutBufSize>=sizeof(REF_CREATE_ASYNC_IO_HANDLE)) {
            __try {
                PREF_CREATE_ASYNC_IO_HANDLE pCreateAsyncIoHandle = (PREF_CREATE_ASYNC_IO_HANDLE)lpOutBuf;
                if (pCreateAsyncIoHandle->hIoRef && IsKModeAddr((DWORD)pCreateAsyncIoHandle->hIoRef)) {
                    IoPacket * pioPacket = (IoPacket *)pCreateAsyncIoHandle->hIoRef;
                    REF_CREATE_ASYNC_IO_HANDLE createAsyncIoHandle = {
                        pCreateAsyncIoHandle->hIoRef,NULL,NULL,NULL
                    };
                    bRet = FALSE;
                    createAsyncIoHandle.hCompletionHandle = 
                        pioPacket->CreateAsyncIoHandle(m_pUDP->GetUserDriverProcessorInfo().dwProcessId,
                            &createAsyncIoHandle.lpInBuf,&createAsyncIoHandle.lpOutBuf);
                    if (createAsyncIoHandle.hCompletionHandle !=NULL) {
                        *pCreateAsyncIoHandle = createAsyncIoHandle ;
                        bRet = TRUE;
                    }
                }
                else {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    bRet = NULL;
                }            
            } __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_INVALID_PARAMETER);
                bRet = NULL;
            }
        }
        break;
        
        
    }
    return bRet;
}
BOOL CReflector::InitAccessWindow( DDKWINDOWINFO& dwi, HANDLE hParentBus )
{
    Lock();
    for (DWORD dwIndex= 0; dwIndex < dwi.dwNumMemWindows && dwIndex < MAX_DEVICE_WINDOWS; dwIndex++) {
        PHYSICAL_ADDRESS PhysicalAddress = { dwi.memWindows[dwIndex].dwBase,0 };
        CPhysMemoryWindow * pNewWindows = new CPhysMemoryWindow(PhysicalAddress,dwi.memWindows[dwIndex].dwLen,
            0,(INTERFACE_TYPE)dwi.dwInterfaceType,dwi.dwBusNumber,hParentBus, m_pPhysicalMemoryWindowList);
        if (pNewWindows && !pNewWindows->Init()) {
            delete pNewWindows;
            pNewWindows = NULL;
        };
        if (pNewWindows)
            m_pPhysicalMemoryWindowList = pNewWindows;
    }
    for (dwIndex= 0; dwIndex < dwi.dwNumIoWindows&& dwIndex < MAX_DEVICE_WINDOWS; dwIndex++) {
        PHYSICAL_ADDRESS PhysicalAddress = { dwi.ioWindows[dwIndex].dwBase,0 };
        CPhysMemoryWindow * pNewWindows = new CPhysMemoryWindow(PhysicalAddress,dwi.ioWindows[dwIndex].dwLen,
            1,(INTERFACE_TYPE)dwi.dwInterfaceType,dwi.dwBusNumber,hParentBus, m_pPhysicalMemoryWindowList);
        if (pNewWindows && !pNewWindows->Init()) {
            delete pNewWindows;
            pNewWindows = NULL;
        };
        if (pNewWindows)
            m_pPhysicalMemoryWindowList = pNewWindows;
    }
    Unlock();
    return TRUE;
}
PVOID  CReflector::RefMmMapIoSpace (PHYSICAL_ADDRESS PhysicalAddress,ULONG NumberOfBytes, BOOLEAN CacheEnable)
{
    Lock();
    PVOID pMappedAddr = NULL;
    CPhysMemoryWindow * pCurWindow = m_pPhysicalMemoryWindowList;
    while (pMappedAddr == NULL && pCurWindow!=NULL) {
        pMappedAddr = pCurWindow->RefMmMapIoSpace(PhysicalAddress, NumberOfBytes,CacheEnable) ;
        if (pMappedAddr == NULL) { // Failed try next.
            pCurWindow = pCurWindow->GetNextObject();
        }
    }
    Unlock();
    return pMappedAddr ;
}
BOOL  CReflector::RefVirtualCopy(LPVOID lpvDest,LPVOID lpvSrc,DWORD cbSize, DWORD fdwProtect) 
{
    BOOL bRet = FALSE;
    if ((fdwProtect & PAGE_PHYSICAL)!= 0 && IsValidUsrPtr(lpvDest,cbSize,TRUE)) {
        PHYSICAL_ADDRESS pCallPh = { (DWORD)lpvSrc<<8, ((DWORD)lpvSrc>>24) & 0xff }; // Create 40-bit physical address.
        Lock();
        PVOID pMappedAddr = NULL;
        CPhysMemoryWindow * pCurWindow = m_pPhysicalMemoryWindowList;
        while (pMappedAddr == NULL && pCurWindow!=NULL) {
            if (pCurWindow->IsSystemAlignedAddrValid(pCallPh, cbSize)) {
                bRet = VirtualCopyEx ((HANDLE)GetCallerVMProcessId(), lpvDest, GetCurrentProcess(), lpvSrc, cbSize, fdwProtect);
                ASSERT(bRet);
                break;
            }
            else 
                pCurWindow = pCurWindow->GetNextObject() ;
        }
        Unlock();
    }
    return bRet ;

}
#define DEFAULT_DLL TEXT("giisr.dll")
#define DEFAULT_ENTRY TEXT("ISRHandler")
HANDLE CReflector::RefLoadIntChainHandler(LPCTSTR /*lpIsrDll*/, LPCTSTR /*lpIsrHandler*/, BYTE /*bIrq*/)
{
    HANDLE hRetHandle = NULL;
    if (m_hIsrHandle == NULL ) {
        if (m_DdkIsrInfo.dwIrq!=0 && m_DdkIsrInfo.dwIrq!=(DWORD)-1
                && _tcsicmp(DEFAULT_DLL, m_DdkIsrInfo.szIsrDll )==0 
                && _tcsicmp(DEFAULT_ENTRY, m_DdkIsrInfo.szIsrHandler)==0) {
        
            hRetHandle = m_hIsrHandle = LoadIntChainHandler(m_DdkIsrInfo.szIsrDll, m_DdkIsrInfo.szIsrHandler, (BYTE)m_DdkIsrInfo.dwIrq);
        }
    }
    return hRetHandle ;
}
BOOL CReflector::RefFreeIntChainHandler(HANDLE hRetHandle) 
{
    BOOL bReturn = FALSE;
    if (hRetHandle && hRetHandle == m_hIsrHandle) {
        FreeIntChainHandler(m_hIsrHandle);
        bReturn = TRUE;
        m_hIsrHandle = NULL;
    }
    return bReturn;
}
BOOL CReflector::IsValidStaticAddr(DWORD dwPhysAddr,DWORD dwSize, ULONG AddressSpace)
{
    BOOL fRet = FALSE;
    Lock();
    CPhysMemoryWindow * pCurWindow = m_pPhysicalMemoryWindowList;
    while (!fRet && pCurWindow!=NULL) {
        fRet = pCurWindow->IsValidStaticAddr(dwPhysAddr, dwSize,AddressSpace);
        pCurWindow = pCurWindow->GetNextObject() ;

    }
    Unlock();
    return fRet;
}
PVOID CReflector::RefCreateStaticMapping(DWORD dwPhysBase,DWORD dwAlignSize)
{    
    PVOID pResult = NULL ;
    Lock();
    CPhysMemoryWindow * pCurWindow = m_pPhysicalMemoryWindowList;
    while (!pResult && pCurWindow!=NULL) {
        pResult = pCurWindow->RefCreateStaticMapping(dwPhysBase, dwAlignSize);
        pCurWindow = pCurWindow->GetNextObject() ;
    }
    Unlock();
    return pResult;
}

BOOL CReflector::RefIntChainHandlerIoControl(HANDLE hRetHandle, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, const LPDWORD lpBytesReturned ) 
{
    BOOL fRet = FALSE;
    if (m_hIsrHandle != NULL &&  hRetHandle == m_hIsrHandle ) {
        if (IOCTL_GIISR_INFO == dwIoControlCode 
                && nInBufSize && nInBufSize>=sizeof(GIISR_INFO)
                && !lpOutBuf && !nOutBufSize && !lpBytesReturned) {
            GIISR_INFO Info ;
            __try {
                Info = *(GIISR_INFO *)lpInBuf;
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return FALSE; 
            }
            BOOL fPortIsOK = TRUE;
            if (Info.CheckPort) {
                fPortIsOK = IsValidStaticAddr(Info.PortAddr ,Info.PortSize, Info.PortIsIO );
                if (fPortIsOK && Info.UseMaskReg) {
                    fPortIsOK = IsValidStaticAddr(Info.MaskAddr ,Info.PortSize, Info.PortIsIO );
                }
            }
            if (fPortIsOK) {
                fRet = KernelLibIoControl(m_hIsrHandle,IOCTL_GIISR_INFO,&Info,sizeof(Info),NULL,0,NULL);
                ASSERT(fRet);
            }
        }
    }
    return fRet;
}

CPhysMemoryWindow::CPhysMemoryWindow( PHYSICAL_ADDRESS PhysicalAddress, ULONG NumberOfBytes,ULONG AddressSpace,INTERFACE_TYPE  InterfaceType,ULONG BusNumber, HANDLE hParentBus,CPhysMemoryWindow *pNext)
:   m_pNextFileFolder(pNext)
,   m_PhBusAddress(PhysicalAddress)
,   m_dwSize(NumberOfBytes)
,   m_AddressSpace(AddressSpace)
{
    m_PhSystemAddresss.QuadPart = 0 ;
    m_pStaticMappedUserPtr = NULL;
    m_dwStaticMappedLength = 0;
    TranslateBusAddr(hParentBus, InterfaceType,BusNumber,m_PhBusAddress,&m_AddressSpace,&m_PhSystemAddresss);
}
BOOL CPhysMemoryWindow::IsSystemAddrValid(PHYSICAL_ADDRESS phPhysAddr,DWORD dwSize)
{
    return (m_PhSystemAddresss.QuadPart!=0 && !m_AddressSpace &&
        phPhysAddr.QuadPart>= m_PhSystemAddresss.QuadPart &&
        phPhysAddr.QuadPart + dwSize <= m_PhSystemAddresss.QuadPart + m_dwSize);
}
BOOL CPhysMemoryWindow::IsValidStaticAddr(DWORD dwPhysAddr,DWORD dwSize,ULONG AddressSpace)
{
    if (AddressSpace) { // IO.
        PHYSICAL_ADDRESS phPhysAddr = { dwPhysAddr, 0 };
        return (m_PhSystemAddresss.QuadPart!=0 && m_AddressSpace!=0 &&
            phPhysAddr.QuadPart>= m_PhSystemAddresss.QuadPart &&
            phPhysAddr.QuadPart + dwSize <= (DWORD)m_PhSystemAddresss.QuadPart + m_dwSize);
    }
    else {
        return (m_pStaticMappedUserPtr!=0 && !m_AddressSpace &&
            dwPhysAddr >= (DWORD)m_pStaticMappedUserPtr &&
            dwPhysAddr + dwSize <= (DWORD)m_pStaticMappedUserPtr +  m_dwStaticMappedLength );
    }
}
BOOL CPhysMemoryWindow::IsSystemAlignedAddrValid(PHYSICAL_ADDRESS phPhysAddr,DWORD dwSize)
{
    PHYSICAL_ADDRESS validAlignAddr;
    validAlignAddr.QuadPart = m_PhSystemAddresss.QuadPart & ~(PAGE_SIZE - 1);
    DWORD validAlignSize = m_dwSize + (m_PhSystemAddresss.LowPart & (PAGE_SIZE - 1));
    
    return (validAlignAddr.QuadPart!=0 && !m_AddressSpace &&
        phPhysAddr.QuadPart>= validAlignAddr.QuadPart &&
        phPhysAddr.QuadPart + dwSize <= validAlignAddr.QuadPart + validAlignSize);
}
PVOID CPhysMemoryWindow::RefMmMapIoSpace (PHYSICAL_ADDRESS PhysicalAddress,ULONG NumberOfBytes, BOOLEAN CacheEnable)
{
    if (IsSystemAddrValid(PhysicalAddress,NumberOfBytes)) {
        PVOID   pVirtualAddress;
        ULONGLONG   SourcePhys;
        ULONG   SourceSize;
        BOOL    bSuccess;

        //
        // Page align source and adjust size to compensate
        //

        SourcePhys = PhysicalAddress.QuadPart & ~(PAGE_SIZE - 1);
        SourceSize = NumberOfBytes + (PhysicalAddress.LowPart & (PAGE_SIZE - 1));
        if (SourceSize < NumberOfBytes) // Check Integer overflow.
            return NULL;

        pVirtualAddress = VirtualAllocEx((HANDLE)GetCallerVMProcessId(),0, SourceSize, MEM_RESERVE, PAGE_NOACCESS);

        if (pVirtualAddress != NULL)   {
            bSuccess = VirtualCopyEx(
                GetCallerProcess(), pVirtualAddress, 
                GetCurrentProcess(),(PVOID)(SourcePhys >> 8), SourceSize,
                PAGE_PHYSICAL | PAGE_READWRITE | (CacheEnable ? 0 : PAGE_NOCACHE));

            if (bSuccess) {
                pVirtualAddress =(PVOID)((ULONG)pVirtualAddress + (PhysicalAddress.LowPart & (PAGE_SIZE - 1)));
            }
            else {
                VERIFY(VirtualFreeEx(GetCallerProcess(), pVirtualAddress, 0, MEM_RELEASE));
                pVirtualAddress = NULL;
            }
        }
        return pVirtualAddress;
    }
    else
        return NULL;
}
PVOID CPhysMemoryWindow::RefCreateStaticMapping(DWORD dwPhisAddrShift8, DWORD dwAlignSize)
{
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID pMappedPtr = NULL;
    PhysicalAddress.QuadPart = (((ULONGLONG)dwPhisAddrShift8)<<8);
    if (IsSystemAlignedAddrValid(PhysicalAddress,dwAlignSize) && m_pStaticMappedUserPtr==NULL ) { // we don't allow second mapping.
        // Let us figure out real static address driver may used.
        if ((pMappedPtr = CreateStaticMapping(dwPhisAddrShift8, dwAlignSize ))!=NULL) {
            if (PhysicalAddress.QuadPart <= m_PhSystemAddresss.QuadPart ) { 
                // the real offset is.
                m_pStaticMappedUserPtr = (PBYTE)pMappedPtr + m_PhSystemAddresss.QuadPart - PhysicalAddress.QuadPart ;
                if (dwAlignSize >= (DWORD)(m_PhSystemAddresss.QuadPart - PhysicalAddress.QuadPart))
                    m_dwStaticMappedLength = min (m_dwSize, dwAlignSize - (DWORD)(m_PhSystemAddresss.QuadPart - PhysicalAddress.QuadPart));
                else
                    m_dwStaticMappedLength = 0;
            }
            else {
                m_pStaticMappedUserPtr = pMappedPtr ;
                if (dwAlignSize>=(DWORD)(PhysicalAddress.QuadPart - m_PhSystemAddresss.QuadPart)) 
                    m_dwStaticMappedLength =min (m_dwSize, dwAlignSize - (DWORD)(PhysicalAddress.QuadPart - m_PhSystemAddresss.QuadPart));
                else
                    m_dwStaticMappedLength = 0;
            }
        }
    };
    return pMappedPtr;
}
CFileFolder::CFileFolder(CReflector * pReflector, DWORD AccessCode, DWORD ShareMode, CFileFolder *  pNextFileFolder ) 
:   m_pReflector(pReflector)
,   m_pNextFileFolder(pNextFileFolder)
{
    m_dwUserData = 0 ;
    FNOPEN_PARAM fnOpenParam;
    fnOpenParam.dwCallerProcessId = GetCallerProcessId();
    fnOpenParam.AccessCode = AccessCode ;
    fnOpenParam.ShareMode = ShareMode;
    DWORD dwReturnByte = 0;
    if (m_pReflector) {
        fnOpenParam.dwDriverContent = m_pReflector->GetDriverData();
        BOOL bRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_OPEN, &fnOpenParam, sizeof(fnOpenParam), &m_dwUserData, sizeof(m_dwUserData), &dwReturnByte);
        if (!bRet || dwReturnByte < sizeof(DWORD)) { // Failed.
            m_dwUserData = 0 ;
        }
    }    
}
CFileFolder::~CFileFolder()
{
    if (m_pReflector && m_dwUserData!=0) {
        VERIFY(m_pReflector->SendIoControl(IOCTL_USERDRIVER_CLOSE, &m_dwUserData , sizeof(m_dwUserData), NULL, NULL, NULL ));
    }
} 

LPVOID  CFileFolder::AllocCopyBuffer(LPVOID lpOrigBuffer, DWORD dwLength, BOOL fInput,LPVOID& KernelAddress)
{
    LPVOID lpReturn = NULL;
    KernelAddress = NULL;

    lpReturn = VirtualAllocCopyEx((HANDLE)GetCallerVMProcessId(),(HANDLE)m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,
        lpOrigBuffer,dwLength,PAGE_READWRITE );
    
    // If input and it can't mapped to readwrite, we try readonly again.
    if (fInput && lpReturn == NULL) {
        lpReturn = VirtualAllocCopyEx((HANDLE)GetCallerVMProcessId(),(HANDLE)m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,
            lpOrigBuffer,dwLength,PAGE_READONLY );
    }
    return lpReturn;
}
BOOL   CFileFolder::FreeCopyBuffer(LPVOID lpMappedBuffer, __in_bcount(dwLength) LPVOID lpOrigBuffer,DWORD dwLength, BOOL fInput, LPVOID KernelAddress)
{
    //Unmap the User Buffer.
    if (KernelAddress) {
        BOOL bFreeRet;
        if (lpMappedBuffer) {
            bFreeRet = VirtualFreeEx((HANDLE)m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,
                (LPVOID)((DWORD)lpMappedBuffer & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE) ;
            ASSERT(bFreeRet);
        }
        if (!fInput) { // Output
            // Copy Out
            __try {
                memcpy(lpOrigBuffer,KernelAddress,dwLength);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        bFreeRet = VirtualFree(KernelAddress,0, MEM_RELEASE);
        ASSERT(bFreeRet);
    }
    else if ( lpMappedBuffer) {
        VERIFY(VirtualFreeEx((HANDLE)m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,
            (LPVOID)((DWORD)lpMappedBuffer & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE));
    }
    return TRUE;
}

BOOL CFileFolder::PreClose()
{
    BOOL fRet = FALSE;
    if (m_dwUserData && m_pReflector) {
        fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_PRECLOSE, &m_dwUserData , sizeof(m_dwUserData), NULL, NULL, NULL );
    };
    return fRet;
}

DWORD CFileFolder::Read(LPVOID buffer, DWORD nBytesToRead,HANDLE hIoRef)
{
    IoPacket * pioPacket = (IoPacket *)hIoRef;
    if (m_dwUserData && m_pReflector && pioPacket ) {
        PVOID pMappedAddr = NULL;
        FNREAD_PARAM fnReadParam;
        fnReadParam.dwCallerProcessId = GetCallerProcessId();
        fnReadParam.nBytes= nBytesToRead;
        fnReadParam.dwContent = m_dwUserData;
        fnReadParam.dwReturnBytes = (DWORD)-1;
        fnReadParam.hIoRef = hIoRef;

        // Mapping the User Buffer.
        if (buffer!=NULL && nBytesToRead != 0 && m_pReflector->IsBufferShouldMapped(buffer,TRUE)) {// We have to mapped the point to user processor space.
            BOOL fResult = pioPacket->MapToAsyncBuffer(m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId, NULL,&pMappedAddr );
            if (fResult) {
                fnReadParam.buffer = pMappedAddr;
            }
            else {
                ASSERT(FALSE);
                fnReadParam.buffer = pMappedAddr =NULL;
            }
        }
        else {
            fnReadParam.buffer = buffer;
        }
            
        BOOL fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_READ, &fnReadParam , sizeof(FNREAD_PARAM), NULL, NULL, NULL );

        //Unmap the User Buffer.
        if (pMappedAddr) {
            pioPacket->UnMapAsyncBuffer();
        }

        return (fRet?fnReadParam.dwReturnBytes:(DWORD)-1);
    }
    else {
        SetLastError (ERROR_INVALID_HANDLE);
        return(ULONG)-1;
    }
}
DWORD CFileFolder::Write(LPCVOID buffer, DWORD nBytesToWrite,HANDLE hIoRef) 
{
    IoPacket * pioPacket = (IoPacket *)hIoRef;
    if (m_dwUserData && m_pReflector && pioPacket ) {
        PVOID pMappedAddr = NULL;
        FNWRITE_PARAM fnWriteParam;
        fnWriteParam.dwCallerProcessId = GetCallerProcessId();
        fnWriteParam.nBytes = nBytesToWrite;
        fnWriteParam.dwContent = m_dwUserData;
        fnWriteParam.dwReturnBytes = (DWORD)-1;
        fnWriteParam.hIoRef = hIoRef;

        // Mapping the User Buffer
        if (buffer!=NULL && nBytesToWrite != 0 && m_pReflector->IsBufferShouldMapped(buffer,FALSE)) { // We have to mapped the point to user processor space.
            BOOL fResult = pioPacket->MapToAsyncBuffer(m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,&pMappedAddr,NULL );
            if (fResult) {
                fnWriteParam.buffer = pMappedAddr;
            }
            else {
                ASSERT(FALSE);
                fnWriteParam.buffer = pMappedAddr =NULL;
            }
        }
        else {
            fnWriteParam.buffer = (LPVOID)buffer;
        }
            
        BOOL fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_WRITE, &fnWriteParam , sizeof(FNWRITE_PARAM), NULL, NULL, NULL );
        // Unmap it
        if (pMappedAddr) {
            pioPacket->UnMapAsyncBuffer();
        }
        
        return (fRet? fnWriteParam.dwReturnBytes: (DWORD)-1);
    }
    else {
        SetLastError (ERROR_INVALID_HANDLE);
        return(ULONG)-1;
    }
}
DWORD CFileFolder::SeekFn(long lDistanceToMove, DWORD dwMoveMethod) 
{
    if (m_dwUserData && m_pReflector) {
        FNSEEK_PARAM fnSeekParam;
        fnSeekParam.dwCallerProcessId = GetCallerProcessId();
        fnSeekParam.dwContent = m_dwUserData;
        fnSeekParam.lDistanceToMove = lDistanceToMove ;
        fnSeekParam.dwMoveMethod = dwMoveMethod;
        fnSeekParam.dwReturnBytes = (DWORD)-1;
        BOOL fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_SEEK, &fnSeekParam , sizeof(FNSEEK_PARAM), NULL, NULL, NULL );
        return (fRet? fnSeekParam.dwReturnBytes: (DWORD) -1 );
    }
    else {
        SetLastError (ERROR_INVALID_HANDLE);
        return(ULONG)-1;
    }
}
BOOL CFileFolder::Control(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,HANDLE hIoRef)
{
    IoPacket * pioPacket = (IoPacket *)hIoRef;
    if (m_dwUserData && m_pReflector && pioPacket ) {
        FNIOCTL_PARAM fnIoCtlPara;
        LPVOID lpMappedInBuf = NULL;
        LPVOID lpMappedOutBuf = NULL;
        fnIoCtlPara.dwCallerProcessId = GetCallerProcessId();
        fnIoCtlPara.dwContent = m_dwUserData;
        fnIoCtlPara.dwIoControlCode = dwIoControlCode;
        fnIoCtlPara.hIoRef = hIoRef;
            
        if (lpInBuf!=NULL && nInBufSize!=NULL && m_pReflector->IsBufferShouldMapped(lpInBuf,FALSE)) {
            BOOL fResult = pioPacket->MapToAsyncBuffer(m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,&lpMappedInBuf,NULL);
            if (fResult) {
                fnIoCtlPara.lpInBuf = lpMappedInBuf;
            }
            else {
                ASSERT(FALSE);
                fnIoCtlPara.lpInBuf = lpMappedInBuf = NULL;
            }
        }
        else
            fnIoCtlPara.lpInBuf = lpInBuf;
        fnIoCtlPara.nInBufSize = nInBufSize;
        
        if (lpOutBuf!=NULL && nOutBufSize && m_pReflector->IsBufferShouldMapped(lpOutBuf,TRUE) ) {
            BOOL fResult = pioPacket->MapToAsyncBuffer(m_pReflector->GetUDP()->GetUserDriverProcessorInfo().dwProcessId,NULL,&lpMappedOutBuf);
            if (fResult) {
                fnIoCtlPara.lpOutBuf = lpMappedOutBuf;
            }
            else {
                ASSERT(FALSE);
                fnIoCtlPara.lpOutBuf = lpMappedOutBuf = NULL;
            }
        }
        else
            fnIoCtlPara.lpOutBuf = lpOutBuf;
        fnIoCtlPara.nOutBufSize = nOutBufSize ;
        
        if (lpBytesReturned) {
            fnIoCtlPara.BytesReturned = *lpBytesReturned;
            fnIoCtlPara.fUseBytesReturned = TRUE;
        }
        else
            fnIoCtlPara.fUseBytesReturned = FALSE;
        
        BOOL fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_IOCTL,&fnIoCtlPara , sizeof(FNIOCTL_PARAM), NULL, 0, NULL );
        //Ummapped the buffer
        if (lpMappedInBuf) 
            pioPacket->UnMapAsyncBuffer();
        if (lpMappedOutBuf)
            pioPacket->UnMapAsyncBuffer();
        
        if (lpBytesReturned)
            *lpBytesReturned = fnIoCtlPara.BytesReturned ; 
        
        return fRet;
        
    }
    else {
        SetLastError (ERROR_INVALID_HANDLE);
        return( FALSE );
    }
}

BOOL CFileFolder::CancelIo(HANDLE hIoHandle)
{
    if (m_dwUserData && m_pReflector && hIoHandle ) {
        FNCANCELIO_PARAM fnCancelIo = {
            GetCallerProcessId(),
            m_dwUserData,
            hIoHandle
        };
        BOOL fRet = m_pReflector->SendIoControl(IOCTL_USERDRIVER_CANCELIO,&fnCancelIo , sizeof(fnCancelIo), NULL, 0, NULL );
        return fRet;
    }
    else {
        SetLastError (ERROR_INVALID_HANDLE);
        return( FALSE );
    }
}


extern "C" {
DWORD Reflector_Create(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags )
{
    return (DWORD)CreateReflector(lpszDeviceKey, lpPreFix, lpDrvName,dwFlags) ;
}

BOOL  Reflector_Delete(DWORD dwDataEx)
{
    return DeleteReflector((CReflector *)dwDataEx);
}
    
DWORD Reflector_InitEx( DWORD dwData, DWORD dwInfo, LPVOID lpvParam )
{
    if (dwData) {
        if (!((CReflector *)dwData)->InitEx(dwInfo,lpvParam )) { // Fails
            dwData = 0;
        }
    }
    return dwData;
}
BOOL Reflector_PreDeinit(DWORD dwData )
{
    return ((CReflector *)dwData)->PreDeinit();
}
BOOL Reflector_Deinit(DWORD dwData)
{
    return ((CReflector *)dwData)->DeInit();
}
DWORD Reflector_Open(DWORD dwData, DWORD AccessCode, DWORD ShareMode)
{
    ASSERT(IsReflectValid((CReflector *)dwData));
    return (DWORD)(((CReflector *)dwData)->Open(AccessCode, ShareMode));
}
BOOL Reflector_Close(DWORD dwOpenData)
{
    BOOL bReturn = FALSE;
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    if (IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector())) {
        bReturn = (((CFileFolder *)dwOpenData)->GetReflector())->Close((CFileFolder *)dwOpenData);
    }
    return bReturn;
}
BOOL Reflector_PreClose(DWORD dwOpenData)
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->PreClose();
}
DWORD Reflector_Read(DWORD dwOpenData,LPVOID buffer, DWORD nBytesToRead,HANDLE hIoRef) 
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->Read(buffer, nBytesToRead,hIoRef);
}
DWORD Reflector_Write(DWORD dwOpenData,LPCVOID buffer, DWORD nBytesToWrite,HANDLE hIoRef) 
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->Write(buffer, nBytesToWrite,hIoRef);
}
DWORD Reflector_SeekFn(DWORD dwOpenData,long lDistanceToMove, DWORD dwMoveMethod)
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->SeekFn(lDistanceToMove,dwMoveMethod);
}
BOOL Reflector_Control(DWORD dwOpenData,DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,HANDLE hIoRef)
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->Control(dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned,hIoRef);
}
void Reflector_Powerup(DWORD dwData) 
{
    ASSERT(IsReflectValid((CReflector *)dwData));
    ((CReflector *)dwData)->Powerup();
}
void Reflector_Powerdn(DWORD dwData) 
{
    ASSERT(IsReflectValid((CReflector *)dwData));
    ((CReflector *)dwData)->Powerdn();
}
BOOL Reflector_CancelIo(DWORD dwOpenData,HANDLE hIoHandle) 
{
    ASSERT(IsReflectValid(((CFileFolder *)dwOpenData)->GetReflector()) && (((CFileFolder *)dwOpenData)->GetReflector()->FindFileFolderBy((CFileFolder *)dwOpenData)));
    return ((CFileFolder *)dwOpenData)->CancelIo(hIoHandle);
}

PROCESS_INFORMATION  Reflector_GetOwnerProcId(DWORD dwDataEx)
{
    ASSERT(IsReflectValid((CReflector *)dwDataEx));
    return ((CReflector *)dwDataEx)->GetUDP()->GetUserDriverProcessorInfo();
}

LPCTSTR  Reflector_GetAccountId(DWORD dwDataEx)
{
    ASSERT(IsReflectValid((CReflector *)dwDataEx));
    return ((CReflector *)dwDataEx)->GetUDP()->GetAccountId();
}

};


