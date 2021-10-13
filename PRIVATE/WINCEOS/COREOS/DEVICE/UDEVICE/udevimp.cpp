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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//
// This module is implementing User Mode Driver reflector.
//


#include <windows.h>
#include <devload.h>
#include <CRegEdit.h>
#include <reflector.h>
#include <devzones.h>
#include "Udevice.hpp"

UserDriver::UserDriver(FNDRIVERLOAD_PARAM& fnDriverLoad_Param,UserDriver *pNext)
:   m_fnDriverLoadParam (fnDriverLoad_Param)
,   m_pNext (pNext)
{
    m_pUserDriverFileList  = NULL;
    
    m_hLib = NULL;
    
    m_fnInit= NULL;
    m_fnPreDeinit = NULL;
    m_fnDeinit = NULL; 
    m_fnOpen = NULL;
    m_fnPreClose = NULL;
    m_fnClose = NULL;
    m_fnRead = NULL;
    m_fnWrite  = NULL;
    m_fnSeek = NULL;
    m_fnControl = NULL ;
    m_fnPowerup = NULL ;
    m_fnPowerdn = NULL;       // optional, can be NULL
    
    m_dwInitData = 0;
    // Load Driver here.
    m_hReflector = NULL;
    m_hUDriver = INVALID_HANDLE_VALUE ;

}
BOOL UserDriver::LoadDriver()
{
    DEBUGMSG(ZONE_ACTIVE, (_T("UDEVICE!CreateDevice: loading driver DLL '%s'\r\n"), m_fnDriverLoadParam.DriverName));
    if (m_hLib == NULL ) {
        DWORD dwStatus = ERROR_SUCCESS;
        m_hLib = (m_fnDriverLoadParam.dwFlags & DEVFLAGS_LOADLIBRARY) ? (::LoadLibrary(m_fnDriverLoadParam.DriverName)) : (::LoadDriver(m_fnDriverLoadParam.DriverName));
        if (!m_hLib) {
            DEBUGMSG(ZONE_WARNING, (_T("UDEVICE!CreateDevice: couldn't load '%s' -- error %d\r\n"), 
                m_fnDriverLoadParam.DriverName, GetLastError()));
            dwStatus = ERROR_FILE_NOT_FOUND;
        } else {
            LPCTSTR pEffType = m_fnDriverLoadParam.Prefix ;
            m_fnInit = (pInitFn)GetDMProcAddr(pEffType,L"Init",m_hLib);
            m_fnPreDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"PreDeinit",m_hLib);
            m_fnDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"Deinit",m_hLib);
            m_fnOpen = (pOpenFn)GetDMProcAddr(pEffType,L"Open",m_hLib);
            m_fnPreClose = (pCloseFn)GetDMProcAddr(pEffType,L"PreClose",m_hLib);
            m_fnClose = (pCloseFn)GetDMProcAddr(pEffType,L"Close",m_hLib);
            m_fnRead = (pReadFn)GetDMProcAddr(pEffType,L"Read",m_hLib);
            m_fnWrite = (pWriteFn)GetDMProcAddr(pEffType,L"Write",m_hLib);
            m_fnSeek = (pSeekFn)GetDMProcAddr(pEffType,L"Seek",m_hLib);
            m_fnControl = (pControlFn)GetDMProcAddr(pEffType,L"IOControl",m_hLib);
            m_fnPowerup = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerUp",m_hLib);
            m_fnPowerdn = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerDown",m_hLib);

            // Make sure that the driver has an init and deinit routine.  If it is named,
            // it must have open and close, plus at least one of the I/O routines (read, write
            // ioctl, and/or seek).  If a named driver has a pre-close routine, it must also 
            // have a pre-deinit routine.
            if (!(m_fnInit && m_fnDeinit) ||
                    (m_fnOpen && !m_fnClose) ||
                    (!m_fnRead && !m_fnWrite &&!m_fnSeek && !m_fnControl) ||
                    (m_fnPreClose && !m_fnPreDeinit)) {
                DEBUGMSG(ZONE_WARNING, (_T("UDEVICE!CreateDevice: illegal entry point combination in driver DLL '%s'\r\n"), 
                    m_fnDriverLoadParam.DriverName));
                dwStatus = ERROR_INVALID_FUNCTION;
            }
        }
        DEBUGMSG(ZONE_ACTIVE,(L"UserDriver::LoadDriver: dwStatus = 0x%x",dwStatus));
        return (dwStatus == ERROR_SUCCESS);
    }
    return FALSE;
}
UserDriver::~UserDriver()
{
    Lock();
    ASSERT(m_hReflector==NULL);
    if (m_hLib) {
        DriverDeinit() ;
        FreeDriverLibrary();
    }
    while (m_pUserDriverFileList) {
        PDRIVERFILECONTENT pNext = m_pUserDriverFileList->pNextFile ;
        delete m_pUserDriverFileList;
        m_pUserDriverFileList = pNext;
    };
    Unlock();
}

void UserDriver::DetermineReflectorHandle(const TCHAR *ActiveRegistry)
{
    ASSERT(m_hReflector == NULL);

    CRegistryEdit activeReg(HKEY_LOCAL_MACHINE, ActiveRegistry);
    if (activeReg.IsKeyOpened()) {
        DWORD dwHandle;
        DWORD dwLen = sizeof(dwHandle);
        DWORD dwType;
        if (activeReg.RegQueryValueEx(DEVLOAD_UDRIVER_REF_HANDLE_VALNAME, &dwType, (PBYTE)&dwHandle, &dwLen) && 
                dwType == DEVLOAD_UDRIVER_REF_HANDLE_VALTYPE) {
            m_hReflector = (HANDLE)dwHandle;
        }                
    }
}

BOOL UserDriver::DriverInit(FNINIT_PARAM& fnInitParam)
{
    BOOL bRet =  FALSE;
    if (m_fnInit && m_dwInitData == 0 ) {
        if (fnInitParam.dwInfo == 0 && m_hReflector == NULL) { // We have ActiveRegistry.
            DetermineReflectorHandle(fnInitParam.ActiveRegistry);
        }
        DWORD dwContent = (fnInitParam.dwInfo!=0 ?fnInitParam.dwInfo : (DWORD)fnInitParam.ActiveRegistry);
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnInitParam.dwCallerProcessId ;
        __try {
            m_dwInitData = m_fnInit(dwContent,fnInitParam.lpvParam);
            bRet = (m_dwInitData!=0);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
        if (!bRet && m_hReflector) {
            CloseHandle(m_hReflector);
            m_hReflector = NULL;
        }
    }
    ASSERT(bRet);
    return bRet;
}
BOOL UserDriver::DriverDeinit()
{
    BOOL bRet = TRUE;
    Lock();
    DWORD dwInitData =m_dwInitData;
    m_dwInitData = 0 ;
    // If we get Deinit before close, we make sure all contents is deleted.
    while (m_pUserDriverFileList) {
        PDRIVERFILECONTENT pNext = m_pUserDriverFileList->pNextFile ;
        delete m_pUserDriverFileList;
        m_pUserDriverFileList = pNext;
    };
    Unlock();
    if (dwInitData!=0 && m_fnDeinit ) {
        __try {
            m_fnDeinit(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
        if (m_hReflector!=NULL) {
            CloseHandle(m_hReflector);
            m_hReflector = NULL ;
        }
    };
    return bRet;
}
BOOL UserDriver::DriverPreDeinit()
{
    BOOL bRet = TRUE;
    DWORD dwInitData = m_dwInitData;
    if (m_fnPreDeinit) {
        if (dwInitData!=0 ) {
            __try {
                m_fnPreDeinit(dwInitData);
                bRet= (dwInitData!=0);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                bRet =  FALSE;
            }
           
        };
    }
    else 
        bRet = DriverDeinit();
    return bRet;
    
}
BOOL UserDriver::DriverPowerDown()
{
    BOOL bRet = TRUE;
    DWORD dwInitData = m_dwInitData;
    if (dwInitData!=0 && m_fnPowerdn ) {
        __try {
            m_fnPowerdn(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
       
    };
    return bRet;
}
BOOL UserDriver::DriverPowerUp()
{
    BOOL bRet = TRUE;
    DWORD dwInitData = m_dwInitData;
    if (dwInitData!=0 && m_fnPowerup ) {
        __try {
            m_fnPowerup(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
       
    };
    return bRet;
}
DWORD  UserDriver::CreateDriverFile(FNOPEN_PARAM& fnOpenParam)
{
    DWORD dwReturn = 0 ;
    if (m_fnOpen) {
        DRIVERFILECONTENT * pdriverFile = new DRIVERFILECONTENT;
        if (pdriverFile ) {
            DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
            UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnOpenParam.dwCallerProcessId ;
            __try {
                dwReturn = m_fnOpen(m_dwInitData,fnOpenParam.AccessCode,fnOpenParam.ShareMode);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                dwReturn = 0 ;
            }
            UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
            if (dwReturn!=0) { // Insert FileContents to the list.
                Lock();
                pdriverFile->dwFileContent = dwReturn;
                pdriverFile->pNextFile = m_pUserDriverFileList ;
                m_pUserDriverFileList = pdriverFile;
                dwReturn = (DWORD) pdriverFile ;
                Unlock();
            }
            else {
                delete pdriverFile;
                pdriverFile = 0;
            }
        }
        else {
            SetLastError(ERROR_OUTOFMEMORY);
        }
    }
    else {
        SetLastError(ERROR_INVALID_FUNCTION);
    }
    return dwReturn;
}
BOOL UserDriver::DeleteDriverFile(DWORD dwFileContent)
{
    Lock();
    DRIVERFILECONTENT * pPrev = NULL;
    DRIVERFILECONTENT * pCur = m_pUserDriverFileList;
    while (pCur!=NULL) {
        if (pCur == (DRIVERFILECONTENT *)dwFileContent) {
            break;
        }
        else {
            pPrev = pCur;
            pCur = pCur->pNextFile ;
        }
    }
    if (pCur) { // We found it.
        dwFileContent = pCur->dwFileContent ;
        if (pPrev!=NULL)
            pPrev->pNextFile = pCur->pNextFile;
        else
            m_pUserDriverFileList = pCur->pNextFile ;
        delete pCur;
    }
    else 
        dwFileContent = 0; // Don't do anything.
    Unlock();
    BOOL bRet = TRUE ;
    SetLastError(ERROR_GEN_FAILURE);
    if (dwFileContent && m_fnClose) { 
        __try {
            bRet = m_fnClose(dwFileContent);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
    }
    return bRet;
}

BOOL UserDriver::IsContainFileContent(DWORD dwFileContent)
{
    Lock();
    DRIVERFILECONTENT * pCur = m_pUserDriverFileList;
    while (pCur!=NULL) {
        if (pCur == (DRIVERFILECONTENT * )dwFileContent) {
            break;
        }
        else {
            pCur = pCur->pNextFile ;
        }
    }
    Unlock();
    return  (pCur!=NULL);
}
DWORD   UserDriver::GetDriverFileContent(DWORD dwFileContent)
{
    DWORD dwDriverFileContents = 0; 
    Lock();
    DRIVERFILECONTENT * pCur = m_pUserDriverFileList;
    while (pCur!=NULL) {
        if (pCur == (DRIVERFILECONTENT * )dwFileContent) {
            dwDriverFileContents = pCur->dwFileContent ;
            break;
        }
        else {
            pCur = pCur->pNextFile ;
        }
    }
    Unlock();
    return dwDriverFileContents ;
}

BOOL UserDriver::PreClose(DWORD dwFileContent)
{
    BOOL bRet = FALSE;
    Lock();
    DRIVERFILECONTENT * pCur = m_pUserDriverFileList;
    while (pCur!=NULL) {
        if (pCur == (DRIVERFILECONTENT * )dwFileContent) {
            break;
        }
        else {
            pCur = pCur->pNextFile ;
        }
    }
    DWORD dwDriveFilerContent;
    if (pCur!=NULL && (dwDriveFilerContent=pCur->dwFileContent)!=0 ) {
        if (m_fnPreClose!=NULL) {
            Unlock();
            __try {
                bRet = m_fnPreClose(dwDriveFilerContent);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                bRet = FALSE ;
            }
            Lock();
        }
        else {
            pCur->dwFileContent = 0 ; // To prevent Close called again.
            if ( m_fnClose!=NULL ) { 
                Unlock();
                __try {
                    bRet = m_fnClose(dwDriveFilerContent);
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    bRet = FALSE ;
                }
                Lock();
            }
        }
    }
    Unlock();
    return bRet;
}
BOOL UserDriver::Read(FNREAD_PARAM& fnReadParam)
{
    BOOL bRet = FALSE;
    DWORD dwDriverFileContent;
    if ((dwDriverFileContent = GetDriverFileContent(fnReadParam.dwContent))!=0 && m_fnRead) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnReadParam.dwCallerProcessId ;
        __try {
            fnReadParam.dwReturnBytes = m_fnRead(dwDriverFileContent,fnReadParam.buffer,fnReadParam.nBytes);
            bRet = TRUE;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
    }
    if (bRet==FALSE)
        fnReadParam.dwReturnBytes = (DWORD)-1;
    return bRet;

}
BOOL UserDriver::Write(FNWRITE_PARAM& fnWriteParam)
{
    BOOL bRet = FALSE;
    DWORD dwDriverFileContent;
    if ((dwDriverFileContent = GetDriverFileContent(fnWriteParam.dwContent))!=0 && m_fnWrite) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnWriteParam.dwCallerProcessId ;
        __try {
            fnWriteParam.dwReturnBytes = m_fnWrite(dwDriverFileContent,fnWriteParam.buffer,fnWriteParam.nBytes);
            bRet = TRUE;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
    }
    if (bRet==FALSE)
        fnWriteParam.dwReturnBytes = (DWORD)-1;
    return bRet;
}
BOOL UserDriver::Seek(FNSEEK_PARAM& fnSeekParam)
{
    BOOL bRet = FALSE;
    DWORD dwDriverFileContent;
    if ((dwDriverFileContent = GetDriverFileContent(fnSeekParam.dwContent))!=0 && m_fnSeek) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnSeekParam.dwCallerProcessId ;
        __try {
            fnSeekParam.dwReturnBytes = m_fnSeek(dwDriverFileContent,fnSeekParam.lDistanceToMove,fnSeekParam.dwMoveMethod);
            bRet = TRUE;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
    }
    if (bRet==FALSE)
        fnSeekParam.dwReturnBytes = (DWORD)-1;
    return bRet;
}
BOOL UserDriver::Ioctl(FNIOCTL_PARAM& fnIoctlParam)
{
    BOOL bRet = FALSE;
    DWORD dwDriverFileContent;
    if ((dwDriverFileContent = GetDriverFileContent(fnIoctlParam.dwContent))!=0 && m_fnControl ) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnIoctlParam.dwCallerProcessId ;
        __try {
            bRet = m_fnControl(dwDriverFileContent,fnIoctlParam.dwIoControlCode,
                    (PBYTE)(fnIoctlParam.lpInBuf),fnIoctlParam.nInBufSize, 
                    (PBYTE)(fnIoctlParam.lpOutBuf),fnIoctlParam.nOutBufSize,
                    (fnIoctlParam.fUseBytesReturned?&fnIoctlParam.BytesReturned:NULL));
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
    }
    return bRet;
}
// This routine looks for a device driver entry point.  The "type" parameter
// is the driver's prefix or NULL; the lpszName parameter is the undecorated
// name of the entry point; and the hInst parameter is the DLL instance handle
// of the driver's DLL.
PFNVOID UserDriver::GetDMProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) 
{
    LPCWSTR s;
    WCHAR fullname[32];
    if (type != NULL && *type != L'\0') {
        memcpy(fullname,type,3*sizeof(WCHAR));
        fullname[3] = L'_';
        wcscpy(&fullname[4],lpszName);
        s = fullname;
    } else {
        s = lpszName;
    }
    return (PFNVOID)GetProcAddress(hInst, s);
}

BOOL UserDriverContainer::InsertNewDriverObject(UserDriver *pNewDriver)
{
    if (InsertObjectBy(pNewDriver) == NULL) { // For some reason insertaion fails.
        ASSERT(FALSE);
        return FALSE;
    }

    return TRUE;
}
BOOL UserDriverContainer::DeleteDriverObject(UserDriver * pDriver)
{
    BOOL bRet =  (RemoveObjectBy(pDriver)!=NULL);
    ASSERT(bRet);
    return bRet;
}

UserDriver * UserDriverContainer::FindDriverByFileContent(DWORD dwFileContent)
{
    Lock();
    UserDriver *   pReturn = NULL;
    UserDriver *   pCur = m_rgLinkList;
    while (pCur) {
        if (pCur->IsContainFileContent(dwFileContent) ) {
            pReturn = pCur;
            break;
        }
        else {
            pCur = pCur->GetNextObject();
        }
    }
    if (pReturn) {
        pReturn->AddRef();
    }
    Unlock();
    return pReturn;
}     

// Returns a pointer to the first driver in the list, for enumeration
// purposes.  Assumes that UserDriverContainer is held
UserDriver * UserDriverContainer::GetFirstDriver()
{
    return m_rgLinkList;
}

