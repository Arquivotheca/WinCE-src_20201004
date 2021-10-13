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
#include <CRegEdit.h>
#include <reflector.h>
#include <devzones.h>
#include <errorrep.h>

#include "Udevice.hpp"
#include "CRegEdit.h"

UserDriver::UserDriver(FNDRIVERLOAD_PARAM& fnDriverLoad_Param,UserDriver *pNext)
:   m_pNext (pNext)
{
    m_pUserDriverFileList  = NULL;
    dwData = 0;
    // Load Driver here.
    m_hReflector = NULL;
    m_hUDriver = INVALID_HANDLE_VALUE ;

    memcpy(&m_fnDriverLoadParam,&fnDriverLoad_Param,sizeof(m_fnDriverLoadParam));

    m_pFilterPath = NULL;
    CRegistryEdit devReg(HKEY_LOCAL_MACHINE, fnDriverLoad_Param.DeviceRegPath, KEY_READ);
    if (devReg.IsKeyOpened()) {
        DWORD dwBuffSize = 0;
        if (devReg.GetRegSize(DEVLOAD_FILTER_VALNAME,dwBuffSize) && dwBuffSize) {
            dwBuffSize = (dwBuffSize +1)/sizeof(TCHAR); // Convert to unit.
            m_pFilterPath = new TCHAR[dwBuffSize];
            if (m_pFilterPath) {
                if (devReg.GetRegValue(DEVLOAD_FILTER_VALNAME,(PBYTE)m_pFilterPath, dwBuffSize*sizeof(TCHAR))){
                    m_pFilterPath[dwBuffSize-1]=0;
                }
                else
                    m_pFilterPath[0] = 0;
            }
            
        }
    }

}
BOOL UserDriver::LoadDriver()
{
    BOOL fReturn = FALSE;
    DEBUGMSG(ZONE_ACTIVE, (_T("UDEVICE!CreateDevice: loading driver DLL '%s'\r\n"), m_fnDriverLoadParam.DriverName));
    
    if (hLib == NULL ) {        
        DWORD dwStatus = ERROR_SUCCESS;
        if (m_pFilterPath!=NULL && m_pFilterPath[0]!=0) {
            CreateDriverFilter(m_pFilterPath);
        }
        hLib = (m_fnDriverLoadParam.dwFlags & DEVFLAGS_LOADLIBRARY) ? (::LoadLibrary(m_fnDriverLoadParam.DriverName)) : (::LoadDriver(m_fnDriverLoadParam.DriverName));
        if (!hLib) {
            DEBUGMSG(ZONE_WARNING, (_T("UDEVICE!CreateDevice: couldn't load '%s' -- error %d\r\n"), 
                m_fnDriverLoadParam.DriverName, GetLastError()));
            dwStatus = ERROR_FILE_NOT_FOUND;
        } else {
            LPCTSTR pEffType = m_fnDriverLoadParam.Prefix ;
            fnInit = (pInitFn)GetDMProcAddr(pEffType,L"Init",hLib);
            fnPreDeinit = (pPreDeinitFn)GetDMProcAddr(pEffType,L"PreDeinit",hLib);
            fnDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"Deinit",hLib);
            fnOpen = (pOpenFn)GetDMProcAddr(pEffType,L"Open",hLib);
            fnPreClose = (pPreCloseFn)GetDMProcAddr(pEffType,L"PreClose",hLib);
            fnClose = (pCloseFn)GetDMProcAddr(pEffType,L"Close",hLib);
            fnRead = (pReadFn)GetDMProcAddr(pEffType,L"Read",hLib);
            fnWrite = (pWriteFn)GetDMProcAddr(pEffType,L"Write",hLib);
            fnSeek = (pSeekFn)GetDMProcAddr(pEffType,L"Seek",hLib);
            fnControl = (pControlFn)GetDMProcAddr(pEffType,L"IOControl",hLib);
            fnPowerup = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerUp",hLib);
            fnPowerdn = (pPowerdnFn)GetDMProcAddr(pEffType,L"PowerDown",hLib);
            fnCancelIo = (pCancelIoFn)GetDMProcAddr(pEffType,L"Cancel",hLib);

            // Make sure that the driver has an init and deinit routine.  If it is named,
            // it must have open and close, plus at least one of the I/O routines (read, write
            // ioctl, and/or seek).  If a named driver has a pre-close routine, it must also 
            // have a pre-deinit routine.
            if (!(fnInit && fnDeinit) ||
                    (fnOpen && !fnClose) ||
                    (fnOpen && !fnRead && !fnWrite &&!fnSeek && !fnControl) ||
                    (fnPreClose && !fnPreDeinit)) {
                DEBUGMSG(ZONE_WARNING, (_T("UDEVICE!CreateDevice: illegal entry point combination in driver DLL '%s'\r\n"), 
                    m_fnDriverLoadParam.DriverName));
                dwStatus = ERROR_INVALID_FUNCTION;
            }
            else {
                //initialize filter drivers
                dwStatus = InitDriverFilters(m_fnDriverLoadParam.DeviceRegPath);
            }
        }
        DEBUGMSG(ZONE_ACTIVE,(L"UserDriver::LoadDriver: dwStatus = 0x%x",dwStatus));
        fReturn = (dwStatus == ERROR_SUCCESS);
    }
    if (m_pFilterPath) {
        delete m_pFilterPath;
        m_pFilterPath = NULL;
    }
    return fReturn;
}
BOOL RunThreadAndWaitForCompletion(LPTHREAD_START_ROUTINE lpStartAddr, LPVOID lpvThreadParam)
{
    BOOL bRet = FALSE;
    HANDLE hThread = CreateThread(NULL, 0, lpStartAddr, lpvThreadParam, 0, NULL);
    if (NULL != hThread) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(hThread,INFINITE)) {
            DWORD dwExitCode;
            if (GetExitCodeThread(hThread,&dwExitCode))
                bRet = (dwExitCode != 0);
        }
        CloseHandle(hThread);
    }
    return bRet;

}
DWORD WINAPI LoadDriverThread(LPVOID lpv)
{
    return ((UserDriver*)lpv)->LoadDriver();
}
DWORD WINAPI FreeDriverLibraryThread(LPVOID lpv)
{
    ((UserDriver*)lpv)->FreeDriverLibrary(TRUE);
    return (DWORD)TRUE;
}
BOOL UserDriver::LoadService()
{
    // We need to do the LoadLibrary() for the service on a worker thread, not directly on a PSL.
    // Reason is that a lot of DLL's that services like to link to tend to use TLS in their DllMain(),
    // which won't work on a PSL call.  Wininet is an example of non-PSL safe DLL.
    return RunThreadAndWaitForCompletion(LoadDriverThread,(LPVOID)this);
}
BOOL UserDriver::FreeServiceLibrary()
{
    // See UserDriver::LoadService() comments
    return RunThreadAndWaitForCompletion(FreeDriverLibraryThread,(LPVOID)this);
}
BOOL UserDriver::IsService()
{
    const WCHAR szServiceRegKeyName[] = L"services\\";
    // Indicates whether we are hosting a service DLL or standard user mode device driver
    return (0 == wcsnicmp(m_fnDriverLoadParam.DeviceRegPath,szServiceRegKeyName,_countof(szServiceRegKeyName) - 1));
}
UserDriver::~UserDriver()
{
    Lock();
    ASSERT(m_hReflector==NULL);
    if (hLib) {
        DriverDeinit() ;
        FreeDriverLibrary(FALSE);
    }
    while (m_pUserDriverFileList) {
        PDRIVERFILECONTENT pNext = m_pUserDriverFileList->pNextFile ;
        delete m_pUserDriverFileList;
        m_pUserDriverFileList = pNext;
    };
    if (m_pFilterPath)
        delete m_pFilterPath;
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
    if (fnInit && dwData == 0 ) {
        if (fnInitParam.dwInfo == 0 && m_hReflector == NULL) { // We have ActiveRegistry.
            DetermineReflectorHandle(fnInitParam.ActiveRegistry);
        }
        DWORD dwContent = (fnInitParam.dwInfo!=0 ?fnInitParam.dwInfo : (DWORD)fnInitParam.ActiveRegistry);
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnInitParam.dwCallerProcessId ;
        __try {
            dwData = DriverFilterMgr::DriverInit(dwContent,fnInitParam.lpvParam);
            bRet = (dwData!=0);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
        if (!bRet && m_hReflector) {
            CloseHandle(m_hReflector);
            m_hReflector = NULL;
        }
    }
    return bRet;
}
BOOL UserDriver::DriverDeinit()
{
    BOOL bRet = TRUE;
    Lock();
    DWORD dwInitData =dwData;
    dwData = 0 ;
    // If we get Deinit before close, we make sure all contents is deleted.
    while (m_pUserDriverFileList) {
        PDRIVERFILECONTENT pNext = m_pUserDriverFileList->pNextFile ;
        delete m_pUserDriverFileList;
        m_pUserDriverFileList = pNext;
    };
    Unlock();
    if (dwInitData!=0 && fnDeinit ) {
        __try {
            DriverFilterMgr::DriverDeinit(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    DWORD dwInitData = dwData;
    if (fnPreDeinit) {
        if (dwInitData!=0 ) {
            __try {
                DriverFilterMgr::DriverPreDeinit(dwInitData);
                bRet= (dwInitData!=0);
            }
            __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    DWORD dwInitData = dwData;
    if (dwInitData!=0 && fnPowerdn ) {
        __try {
            DriverFilterMgr::DriverPowerdn(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
       
    };
    return bRet;
}
BOOL UserDriver::DriverPowerUp()
{
    BOOL bRet = TRUE;
    DWORD dwInitData = dwData;
    if (dwInitData!=0 && fnPowerup ) {
        __try {
            DriverFilterMgr::DriverPowerup(dwInitData);
            bRet= (dwInitData!=0);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            bRet =  FALSE;
        }
       
    };
    return bRet;
}
DWORD  UserDriver::CreateDriverFile(FNOPEN_PARAM& fnOpenParam)
{
    DWORD dwReturn = 0 ;
    if (fnOpen) {
        DRIVERFILECONTENT * pdriverFile = new DRIVERFILECONTENT;
        if (pdriverFile ) {
            DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
            UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnOpenParam.dwCallerProcessId ;
            __try {
                dwReturn = DriverFilterMgr::DriverOpen(dwData,fnOpenParam.AccessCode,fnOpenParam.ShareMode);
            }
            __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    if (dwFileContent && fnClose) { 
        __try {
            bRet = DriverFilterMgr::DriverClose(dwFileContent);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
        if (fnPreClose!=NULL) {
            Unlock();
            __try {
                bRet = DriverFilterMgr::DriverPreClose(dwDriveFilerContent);
            }
            __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                bRet = FALSE ;
            }
            Lock();
        }
        else {
            pCur->dwFileContent = 0 ; // To prevent Close called again.
            if ( fnClose!=NULL ) { 
                Unlock();
                __try {
                    bRet = DriverFilterMgr::DriverClose(dwDriveFilerContent);
                }
                __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    if ((dwDriverFileContent = GetDriverFileContent(fnReadParam.dwContent))!=0 && fnRead) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnReadParam.dwCallerProcessId ;
        __try {
            fnReadParam.dwReturnBytes = DriverFilterMgr::DriverRead(dwDriverFileContent,fnReadParam.buffer,fnReadParam.nBytes,fnReadParam.hIoRef);
            bRet = TRUE;
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    if ((dwDriverFileContent = GetDriverFileContent(fnWriteParam.dwContent))!=0 && fnWrite) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnWriteParam.dwCallerProcessId ;
        __try {
            fnWriteParam.dwReturnBytes = DriverFilterMgr::DriverWrite(dwDriverFileContent,fnWriteParam.buffer,fnWriteParam.nBytes,fnWriteParam.hIoRef);
            bRet = TRUE;
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    if ((dwDriverFileContent = GetDriverFileContent(fnSeekParam.dwContent))!=0 && fnSeek) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnSeekParam.dwCallerProcessId ;
        __try {
            fnSeekParam.dwReturnBytes = DriverFilterMgr::DriverSeek(dwDriverFileContent,fnSeekParam.lDistanceToMove,fnSeekParam.dwMoveMethod);
            bRet = TRUE;
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
    if ((dwDriverFileContent = GetDriverFileContent(fnIoctlParam.dwContent))!=0 && fnControl ) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnIoctlParam.dwCallerProcessId ;
        __try {
            bRet = DriverFilterMgr::DriverControl(dwDriverFileContent,fnIoctlParam.dwIoControlCode,
                    (PBYTE)(fnIoctlParam.lpInBuf),fnIoctlParam.nInBufSize, 
                    (PBYTE)(fnIoctlParam.lpOutBuf),fnIoctlParam.nOutBufSize,
                    (fnIoctlParam.fUseBytesReturned?&fnIoctlParam.BytesReturned:NULL),fnIoctlParam.hIoRef);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE ;
        }
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = dwOldCaller;
    }
    return bRet;
}
BOOL UserDriver::CancelIo(FNCANCELIO_PARAM& fnCancelIoParam)
{
    BOOL bRet = FALSE;
    DWORD dwDriverFileContent;
    if ((dwDriverFileContent = GetDriverFileContent(fnCancelIoParam.dwContent))!=0 && fnControl ) {
        DWORD dwOldCaller = UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] ;
        UTlsPtr()[ PRETLS_DRIVER_DIRECT_CALLER ] = fnCancelIoParam.dwCallerProcessId ;
        __try {
            bRet = DriverFilterMgr::DriverCancelIo(dwDriverFileContent,fnCancelIoParam.hIoHandle);
        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
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
        VERIFY(SUCCEEDED(StringCchCopy(&fullname[4],_countof(fullname) - 4, lpszName)));
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

