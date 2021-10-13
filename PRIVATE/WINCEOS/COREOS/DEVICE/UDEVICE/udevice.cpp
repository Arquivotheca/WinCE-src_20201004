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
#include <pwindbas.h>
#include <extfile.h>
#include "reflector.h"
#include "Udevice.hpp"

#ifdef DEBUG
//
// These defines must match the ZONE_* defines in devmgr.h
//
#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_BUILTIN    16
#define DBG_ACTIVE     64
#define DBG_RESMGR     128
#define DBG_FSD        256
#define DBG_DYING      512
#define DBG_BOOTSEQ 1024
#define DBG_PNP         2048
#define DBG_SERVICES    4096

DBGPARAM dpCurSettings = {
    TEXT("DEVLOAD"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Built-In Devices"),TEXT("Undefined"),TEXT("Active Devices"),TEXT("Resource Manager"),
    TEXT("File System Drvr"),TEXT("Dying Devs"),TEXT("Boot Sequence"),TEXT("PnP"),
    TEXT("Services"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
    DBG_ERROR | DBG_WARNING | DBG_INIT
};
#endif  // DEBUG
BOOL UD_DevCloseFileHandle(DWORD dwContext)
{
    UserDriver * pUserDriver = (UserDriver *)dwContext;
    if (pUserDriver) {
        pUserDriver->SetUDriverHandle( INVALID_HANDLE_VALUE ) ;
        pUserDriver->DeRef();
    }
    return TRUE;
}
extern "C" 
BOOL UD_DevDeviceIoControl(DWORD dwContent, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped);
const PFNVOID UdServApiMethods[] = {
    (PFNVOID)UD_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)UD_DevDeviceIoControl,
};

#define NUM_UD_SERV_APIS (sizeof(UdServApiMethods)/sizeof(UdServApiMethods[0]))

const ULONGLONG UdServApiSigs[NUM_UD_SERV_APIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),   // ReadFile
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),   // WriteFile
    FNSIG2(DW,O_PDW),                   // GetFileSize
    FNSIG4(DW,DW,O_PDW,DW),             // SetFilePointer
    FNSIG2(DW,O_PDW),                   // GetDeviceInformationByFileHandle
    FNSIG1(DW),                         // FlushFileBuffers
    FNSIG4(DW,O_PDW,O_PDW,O_PDW),       // GetFileTime
    FNSIG4(DW,IO_PDW,IO_PDW,IO_PDW),    // SetFileTime
    FNSIG1(DW),                         // SetEndOfFile,
    FNSIG8(DW, DW, IO_PTR, DW, IO_PTR, DW, O_PDW, IO_PDW), // DeviceIoControl
};

HANDLE ghDevFileApiHandle = INVALID_HANDLE_VALUE ;

BOOL HandleServicesIOCTL(FNIOCTL_PARAM& fnIoctlParam);

extern "C" 
BOOL DEVFS_IoControl(DWORD dwContent, HANDLE hProc, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped);
extern "C"
DWORD DEVFS_StubFunction(void)
{
    return 0;
}
// User Device Manager filesystem APIs
static CONST PFNVOID gpfnDevFSAPIs[] = {
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)NULL,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_IoControl,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
};

static CONST ULONGLONG gDevFSSigs[] = {
        FNSIG0(),                                       // CloseVolume
        FNSIG0(),                                       //
        FNSIG0(),                                       // CreateDirectoryW
        FNSIG0(),                                       // RemoveDirectoryW
        FNSIG0(),                                       // GetFileAttributesW
        FNSIG0(),                                       // SetFileAttributesW
        FNSIG0(),                                       // CreateFileW
        FNSIG0(),                                       // DeleteFileW
        FNSIG0(),                                       // MoveFileW
        FNSIG0(),                                       // FindFirstFileW
        FNSIG0(),                                       // CeRegisterFileSystemNotification
        FNSIG0(),                                       // CeOidGetInfo
        FNSIG0(),                                       // PrestoChangoFileName
        FNSIG0(),                                       // CloseAllFiles
        FNSIG0(),                                       // GetDiskFreeSpace
        FNSIG0(),                                       // Notify
        FNSIG0(),                                       // CeRegisterFileSystemFunction
        FNSIG0(),                                       // FindFirstChangeNotification
        FNSIG0(),                                       // FindNextChangeNotification
        FNSIG0(),                                       // FindCloseNotification
        FNSIG0(),                                       // CeGetFileNotificationInfo
        FNSIG9(DW, DW, DW, IO_PTR, DW, IO_PTR, DW, O_PDW, IO_PDW), // FsIoControlW 
        FNSIG0(),                                       // SetFileSecurityW
        FNSIG0(),                                       // GetFileSecurityW 
};

HANDLE ghDevFSAPI = NULL;
DWORD  giFSIndex = (DWORD)-1;
BOOL   gfRegisterOK = FALSE;
HANDLE ghExit = NULL;

UserDriverContainer * g_pUserDriverContainer = NULL;

BOOL RegisterAFSAPI (LPCTSTR VolString)
{
    g_pUserDriverContainer = new UserDriverContainer () ;
    
    ghDevFileApiHandle = CreateAPISet("W32D", NUM_UD_SERV_APIS, UdServApiMethods, UdServApiSigs );
    BOOL bReturn = FALSE;
    if (ghDevFileApiHandle!=INVALID_HANDLE_VALUE)
        bReturn =RegisterAPISet(ghDevFileApiHandle, HT_FILE | REGISTER_APISET_TYPE);
    ASSERT(ghDevFileApiHandle!=INVALID_HANDLE_VALUE && bReturn) ;
    
    ghDevFSAPI = CreateAPISet("UDFA", ARRAYSIZE(gpfnDevFSAPIs), (const PFNVOID *) gpfnDevFSAPIs, gDevFSSigs);
    RegisterAPISet (ghDevFSAPI, HT_AFSVOLUME | REGISTER_APISET_TYPE);
    ASSERT(ghDevFSAPI!=NULL);
    giFSIndex = RegisterAFSName(VolString);
    ASSERT(giFSIndex!=(DWORD)-1);
    if (ghDevFSAPI!=NULL && giFSIndex!=(DWORD)-1) {
        gfRegisterOK = RegisterAFSEx(giFSIndex, ghDevFSAPI, 0 , AFS_VERSION, AFS_FLAG_HIDDEN|AFS_FLAG_KMODE);
        ASSERT(gfRegisterOK);
    }
    if (gfRegisterOK)
        ghExit =  CreateEvent(NULL,TRUE,FALSE,NULL) ;
    ASSERT(ghExit!=NULL);
    
    return (g_pUserDriverContainer && gfRegisterOK && ghExit && ghDevFileApiHandle!=INVALID_HANDLE_VALUE && bReturn);
}
void  UnRegisterAFSAPI()
{
    if (ghDevFileApiHandle!= INVALID_HANDLE_VALUE)
        CloseHandle(ghDevFileApiHandle);
    
    if (giFSIndex!=(DWORD)-1) {
        DeregisterAFS(giFSIndex);
        DeregisterAFSName(giFSIndex);
    }
    if (ghDevFSAPI)
        CloseHandle(ghDevFSAPI);
    if (ghExit)
        CloseHandle(ghExit);
    if (g_pUserDriverContainer) {
        delete g_pUserDriverContainer;
        g_pUserDriverContainer = NULL;
    };
    
}
BOOL WaitForPrimaryThreadExit(DWORD dwTicks)
{
    return WaitForSingleObject(ghExit,dwTicks);
}

inline UserDriver * CreateDriverObject(FNDRIVERLOAD_PARAM& fnDriverLoadParam)
{
    if (g_pUserDriverContainer==NULL)
        return NULL;

    UserDriver* pReturnDriver = CreateUserModeDriver(fnDriverLoadParam) ; //new UserDriver(fnDriverLoadParam,NULL);
    if (pReturnDriver != NULL && !pReturnDriver->Init()) {
        delete pReturnDriver;
        pReturnDriver = NULL;
    }

    if (pReturnDriver && !g_pUserDriverContainer->InsertNewDriverObject(pReturnDriver)) {
        delete pReturnDriver;
        pReturnDriver = NULL;
    }
    return pReturnDriver;
}

inline BOOL DeleteDriverObject (UserDriver * pUserDriver)
{
    return (g_pUserDriverContainer!=NULL? g_pUserDriverContainer->DeleteDriverObject(pUserDriver) : FALSE);
}
inline BOOL HandleDeviceInit(FNINIT_PARAM& fnInitParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer) {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FoundObjectBy((UserDriver * )(fnInitParam.dwDriverContent));
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DriverInit(fnInitParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}

inline BOOL  HandleDeviceDeInit(UserDriver *pUserDriver)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer) {
        pUserDriver = g_pUserDriverContainer->FoundObjectBy(pUserDriver);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DriverDeinit();
            pUserDriver->DeRef();
        }
    }
    return bReturn ;
}
inline BOOL HandleDevicePreDeinit(UserDriver *pUserDriver)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer) {
        pUserDriver = g_pUserDriverContainer->FoundObjectBy(pUserDriver);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DriverPreDeinit();
            pUserDriver->DeRef();
        }
    }
    return bReturn ;
}
inline BOOL HandleDevicePowerDown(UserDriver *pUserDriver)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer) {
        pUserDriver = g_pUserDriverContainer->FoundObjectBy(pUserDriver);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DriverPowerDown();
            pUserDriver->DeRef();
        }
    }
    return bReturn ;
}
inline BOOL HandleDevicePowerUp(UserDriver *pUserDriver)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer) {
        pUserDriver = g_pUserDriverContainer->FoundObjectBy(pUserDriver);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DriverPowerUp();
            pUserDriver->DeRef();
        }
    }
    return bReturn ;
}
inline  DWORD HandleDeviceOpen(FNOPEN_PARAM& fnOpenParam)
{
    DWORD dwReturn = 0;
    if (g_pUserDriverContainer) {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FoundObjectBy((UserDriver *)fnOpenParam.dwDriverContent);
        if (pUserDriver!=NULL) {
            dwReturn = pUserDriver->CreateDriverFile(fnOpenParam);
            pUserDriver->DeRef();
        }
    }
    return dwReturn ;
}
inline BOOL HandleDeviceClose(DWORD dwFileContent)
{
    BOOL bReturn = TRUE; // This file can be cause by PreClose when there is no m_fnPreClose entry for driver. 
    if (g_pUserDriverContainer) {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(dwFileContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->DeleteDriverFile(dwFileContent);
            pUserDriver->DeRef();
        }
    }
    return bReturn ;
    
}
inline BOOL HandleDevicePreClose(DWORD dwFileContent)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(dwFileContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->PreClose(dwFileContent);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
inline BOOL HandleDeviceRead(FNREAD_PARAM& fnReadParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(fnReadParam.dwContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->Read(fnReadParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
inline BOOL HandleDeviceWrite(FNWRITE_PARAM& fnWriteParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(fnWriteParam.dwContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->Write(fnWriteParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
inline BOOL HandleDeviceSeek(FNSEEK_PARAM& fnSeekParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(fnSeekParam.dwContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->Seek(fnSeekParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
inline BOOL HandleDeviceIOCTL(FNIOCTL_PARAM& fnIoctlParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(fnIoctlParam.dwContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->Ioctl(fnIoctlParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
extern "C" 
BOOL UD_DevDeviceIoControl(DWORD dwContent, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped)
{
    DWORD dwOldLastError = GetLastError();
    SetLastError (ERROR_INVALID_PARAMETER);
    UserDriver * pUserDriver = (UserDriver *)dwContent;
    if (!pUserDriver) {
        ASSERT(FALSE);
        return FALSE;
    }
    BOOL bRet = FALSE;
    switch (dwIoControlCode) {
      case IOCTL_USERDRIVER_EXIT: 
        if (pUserDriver->GetUDriverHandle()!=INVALID_HANDLE_VALUE) {
            CloseHandle(pUserDriver->GetUDriverHandle());
            pUserDriver->SetUDriverHandle(INVALID_HANDLE_VALUE) ;
        }
        bRet =DeleteDriverObject(pUserDriver);
        break;
      case  IOCTL_USERDRIVER_INIT:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNINIT_PARAM)) {
            bRet = pUserDriver->DriverInit((*(PFNINIT_PARAM) pInBuf));
        }
        break;
      case IOCTL_USERDRIVER_DEINIT:
        bRet = pUserDriver->DriverDeinit();
        break;
      case IOCTL_USERDRIVER_PREDEINIT:
        bRet = pUserDriver->DriverPreDeinit() ;
        break;
      case IOCTL_USERDRIVER_POWERDOWN:
        bRet = pUserDriver->DriverPowerDown();
        break;
      case IOCTL_USERDRIVER_POWERUP:
        bRet = pUserDriver->DriverPowerUp();
        break;
      case IOCTL_USERDRIVER_OPEN: 
        if (pInBuf && nInBufSize>=sizeof(FNOPEN_PARAM) && pOutBuf && nOutBufSize>= sizeof(DWORD)) {
            DWORD dwOpenContent = pUserDriver->CreateDriverFile(*(PFNOPEN_PARAM)pInBuf);
            if (dwOpenContent!=0) {
                *(PDWORD)pOutBuf = dwOpenContent;
                bRet = TRUE;
                if (pBytesReturned)
                    *pBytesReturned = sizeof(DWORD);
            }
        }
        break;
      case IOCTL_USERDRIVER_CLOSE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(DWORD)) {
            bRet = pUserDriver->DeleteDriverFile((*(PDWORD)pInBuf));
        }
        break;
        

      case IOCTL_USERDRIVER_PRECLOSE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(DWORD)) {
            bRet = pUserDriver->PreClose(*(PDWORD)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_READ:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNREAD_PARAM)) {
            bRet = pUserDriver->Read(*(PFNREAD_PARAM)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_WRITE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNWRITE_PARAM)) {
            bRet = pUserDriver->Write(*(PFNWRITE_PARAM)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_SEEK:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNSEEK_PARAM)) {
            bRet = pUserDriver->Seek(*(PFNSEEK_PARAM)pInBuf) ;
        }
        break;
      case IOCTL_USERDRIVER_IOCTL:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNIOCTL_PARAM)) {
            bRet = pUserDriver->Ioctl(*(PFNIOCTL_PARAM)pInBuf);
        }
        break;
      default:
        ASSERT(FALSE);
        break;
    }

    //  if we succeeded and LastError is set to something other than 
    //  ERROR_INVALID_PARAMETER, then one of the subfunctions set it
    //  and it should be percolated up to the caller.  Otherwise we
    //  return the saved last error status.  This means a subfunction is
    //  not supposed to set a last error of ERROR_INVALID_PARAMETER
    //  and simultaneously succeed.
    if (bRet && (GetLastError() == ERROR_INVALID_PARAMETER))
        SetLastError(dwOldLastError);

    return bRet;
}
extern "C" 
BOOL DEVFS_IoControl(DWORD dwContent, HANDLE hProc, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped)
{
    //hProc is only from Kernel.
    BOOL bRet = FALSE;
    DWORD dwOldLastError = GetLastError();
    SetLastError (ERROR_INVALID_PARAMETER);
    // dwIoControlCode.
    switch (dwIoControlCode) {
      case IOCTL_USERDRIVER_LOAD:
        if (pInBuf && nInBufSize>=sizeof(FNDRIVERLOAD_PARAM) 
                && pOutBuf && nOutBufSize>= sizeof(FNDRIVERLOAD_RETURN)) {
            UserDriver * pUserDriver;
            pUserDriver = CreateDriverObject(*(PFNDRIVERLOAD_PARAM)pInBuf);

            if (pUserDriver) {
                ((PFNDRIVERLOAD_RETURN)pOutBuf)->dwDriverContext = (DWORD) pUserDriver ;
                // We also create Handle base API set.
                HANDLE hDev = CreateAPIHandle(ghDevFileApiHandle, pUserDriver );
                if (hDev!=NULL && hDev!=INVALID_HANDLE_VALUE) {
                    pUserDriver->SetUDriverHandle(hDev);
                    pUserDriver->AddRef();
                }
                ((PFNDRIVERLOAD_RETURN)pOutBuf)->hDriversAccessHandle = hDev;
                if (pBytesReturned)
                    *pBytesReturned = sizeof(FNDRIVERLOAD_RETURN);
            }
            bRet = (pUserDriver!=NULL);
        }
        break;
      case IOCTL_USERDRIVER_EXIT: 
        if (pInBuf && nInBufSize>=sizeof(UserDriver *)) {
            if (g_pUserDriverContainer) {
                UserDriver *  pUserDriver = g_pUserDriverContainer->FoundObjectBy(*(UserDriver **)pInBuf);
                if (pUserDriver!=NULL) {
                    if (pUserDriver->GetUDriverHandle()!=INVALID_HANDLE_VALUE) {
                        CloseHandle(pUserDriver->GetUDriverHandle());
                        pUserDriver->SetUDriverHandle(INVALID_HANDLE_VALUE) ;
                    }
                    pUserDriver->DeRef();
                }
            }
            bRet =DeleteDriverObject(*(UserDriver **)pInBuf);
            ASSERT(bRet);
        };
        break;
      case  IOCTL_USERDRIVER_INIT:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNINIT_PARAM)) {
            bRet = HandleDeviceInit(*(PFNINIT_PARAM) pInBuf);                
        }
        break;
      case IOCTL_USERDRIVER_DEINIT:
        if (pInBuf!=NULL && nInBufSize>= sizeof(UserDriver *) ){
            bRet = HandleDeviceDeInit(*(UserDriver **)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_PREDEINIT:
        if (pInBuf!=NULL && nInBufSize>= sizeof(UserDriver *)) {
            bRet = HandleDevicePreDeinit(*(UserDriver **)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_POWERDOWN:
        if (pInBuf!=NULL && nInBufSize>= sizeof(UserDriver *)) {
            bRet = HandleDevicePowerDown(*(UserDriver **)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_POWERUP:
        if (pInBuf!=NULL && nInBufSize>= sizeof(UserDriver *)) {
            bRet = HandleDevicePowerUp(*(UserDriver **)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_OPEN: 
        if (pInBuf && nInBufSize>=sizeof(FNOPEN_PARAM) && pOutBuf && nOutBufSize>= sizeof(DWORD)) {
            DWORD dwOpenContent = (DWORD)HandleDeviceOpen(*(PFNOPEN_PARAM)pInBuf);
            if (dwOpenContent!=0) {
                *(PDWORD)pOutBuf = dwOpenContent;
                bRet = TRUE;
                if (pBytesReturned)
                    *pBytesReturned = sizeof(DWORD);
            }
        }
        break;
      case IOCTL_USERDRIVER_CLOSE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(DWORD)) {
            bRet = HandleDeviceClose(*(PDWORD)pInBuf);
        }
        break;
        

      case IOCTL_USERDRIVER_PRECLOSE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(DWORD)) {
            bRet = HandleDevicePreClose(*(PDWORD)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_READ:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNREAD_PARAM)) {
            bRet = HandleDeviceRead(*(PFNREAD_PARAM)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_WRITE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNWRITE_PARAM)) {
            bRet = HandleDeviceWrite(*(PFNWRITE_PARAM)pInBuf);
        }
        break;
      case IOCTL_USERDRIVER_SEEK:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNSEEK_PARAM)) {
            bRet = HandleDeviceSeek(*(PFNSEEK_PARAM)pInBuf) ;
        }
        break;
      case IOCTL_USERDRIVER_IOCTL:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNIOCTL_PARAM)) {
            bRet = HandleDeviceIOCTL(*(PFNIOCTL_PARAM)pInBuf);
        }
        break;
      case IOCTL_SERVICES_IOCTL:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNIOCTL_PARAM)) {
            bRet = HandleServicesIOCTL(*(PFNIOCTL_PARAM)pInBuf);
        }
        break;
      case IOCTL_USERPROCESSOR_EXIT:
        if (ghExit!=NULL)
            SetEvent(ghExit); // tell main thread to said goodbye.
        break;
      case IOCTL_USERPROCESSOR_ALIVE:
        bRet = TRUE;
        break;
    }

    //  if we succeeded and LastError is set to something other than 
    //  ERROR_INVALID_PARAMETER, then one of the subfunctions set it
    //  and it should be percolated up to the caller.  Otherwise we
    //  return the saved last error status.  This means a subfunction is
    //  not supposed to set a last error of ERROR_INVALID_PARAMETER
    //  and simultaneously succeed.
    if (bRet && (GetLastError() == ERROR_INVALID_PARAMETER))
        SetLastError(dwOldLastError);

    return bRet;
}

