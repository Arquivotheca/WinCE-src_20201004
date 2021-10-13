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
#include <pwindbas.h>
#include <extfile.h>
#include <acctid.h>
#include "reflector.h"
#include "Udevice.hpp"

///=============================================================================
/// Global defines
///=============================================================================

HANDLE ghDevFileApiHandle = NULL;
HANDLE g_hUDevRefl = NULL;
HANDLE g_hUDevReflAPI = NULL;
HANDLE ghExit = NULL;
UserDriverContainer * g_pUserDriverContainer = NULL;


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

BOOL UD_DevCloseFileHandle(
    DWORD dwContext
);
extern "C"
BOOL UD_DevDeviceIoControl(
    DWORD dwContent,
    DWORD dwIoControlCode,
    PVOID pInBuf,
    DWORD nInBufSize,
    PVOID pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned, 
    OVERLAPPED *pOverlapped
);

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

extern "C"
BOOL UDevRefl_CloseFileHandle(
    DWORD dwContext
);

extern "C"
BOOL UDevRefl_DeviceIoControl(
    DWORD dwContent,
    DWORD dwIoControlCode,
    PVOID pInBuf,
    DWORD nInBufSize,
    PVOID pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned,
    OVERLAPPED *pOverlapped
);

const PFNVOID UDevReflApiMethods[] = {
    (PFNVOID)UDevRefl_CloseFileHandle,
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
    (PFNVOID)UDevRefl_DeviceIoControl,
};

#define NUM_UDEVMGR_SERV_APIS _countof(UDevReflApiMethods)

const ULONGLONG UDevReflApiSigs[NUM_UDEVMGR_SERV_APIS] = {
    FNSIG1(DW),                         // CloseFileHandle
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

///=============================================================================
/// Macros
///=============================================================================
                       

///=============================================================================
/// Local defines
///=============================================================================

///=============================================================================
/// External functions
///=============================================================================

BOOL HandleServicesIOCTL(FNIOCTL_PARAM& fnIoctlParam);

///=============================================================================
/// Static variables
///=============================================================================


///=============================================================================
/// Private Functions
///=============================================================================


///=============================================================================
/// Public Functions
///=============================================================================

BOOL RegisterAFSAPI (LPCTSTR VolString)
{
    BOOL fRegisterOK = FALSE;
    BOOL bReturn = FALSE;

    g_pUserDriverContainer = new UserDriverContainer () ;
    ASSERT(g_pUserDriverContainer);

    // API set used per driver
    //
    ghDevFileApiHandle = CreateAPISet(
                            "W32D",
                            NUM_UD_SERV_APIS,
                            UdServApiMethods,
                            UdServApiSigs);
                            
    if (ghDevFileApiHandle)
    {
        bReturn = RegisterAPISet(
                    ghDevFileApiHandle,
                    HT_FILE | REGISTER_APISET_TYPE);
    }
    ASSERT(ghDevFileApiHandle && bReturn) ;


    // API set used by the user-mode driver's manager
    //
    
    // create the api set
    g_hUDevReflAPI = CreateAPISet(
                        "UDEVMGR",
                        NUM_UDEVMGR_SERV_APIS,
                        UDevReflApiMethods,
                        UDevReflApiSigs);
    if (!g_hUDevReflAPI)
    {
        RETAILMSG(1, (
            L"ERROR! udevice: Failed to create API set. GetLastError=0x%x\r\n",
            GetLastError()
        ));
        ASSERT(FALSE);
        goto cleanup;
    }

    // register the api set as handle based api
    if (!RegisterAPISet (g_hUDevReflAPI, HT_FILE | REGISTER_APISET_TYPE))
    {
        RETAILMSG(1, (
            L"ERROR! udevice:Failed to register API set.GetLastError=0x%x\r\n",
            GetLastError()
        ));
        bReturn = FALSE;
        goto cleanup;
    }
    
    // create the handle to user-mode refl API set
    g_hUDevRefl = CreateAPIHandle(g_hUDevReflAPI, g_pUserDriverContainer);
    if (!g_hUDevRefl)
    {
        RETAILMSG(1, (
            L"ERROR! udevice:Failed to create API handle.GetLastError=0x%x\r\n",
            GetLastError()
        ));
        ASSERT(FALSE);
        goto cleanup;
    }

    DEBUGMSG(DBG_BOOTSEQ, (
        L"udevice: Registering udevice instance (%s) with devmgr.\r\n",
        VolString
    ));

    // send this handle to reflector in kernel
    fRegisterOK = REL_UDriverProcIoControl(
                        IOCTL_REF_REGISTER_UDEVMGR_HANDLE, //dwIoControlCode
                        &g_hUDevRefl,                      //lpInBuf
                        sizeof(g_hUDevRefl),               //nInBufSize
                        NULL,                              //lpOutBuf
                        0,                                 //nOutBufSize
                        NULL);                             //lpBytesReturned
    if (!fRegisterOK)
    {
        RETAILMSG(1, (
            L"ERROR! udevice:Failed to register handle with reflector\r\n"
        ));
        ASSERT(FALSE);
        goto cleanup;
    }

    ghExit =  CreateEvent(NULL,TRUE,FALSE,NULL);
    ASSERT(ghExit!=NULL);

cleanup:    
    return (g_pUserDriverContainer &&
            fRegisterOK &&
            ghExit &&
            g_hUDevRefl && 
            ghDevFileApiHandle &&
            bReturn);
}
void  UnRegisterAFSAPI()
{
    CloseHandle(ghDevFileApiHandle);
    CloseHandle(g_hUDevRefl);
    CloseHandle(g_hUDevReflAPI);
    CloseHandle(ghExit);

    if (g_pUserDriverContainer)
    {
        delete g_pUserDriverContainer;
        g_pUserDriverContainer = NULL;
    }
    
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
inline BOOL HandleDeviceIoCancel(FNCANCELIO_PARAM& fnIoCancelParam)
{
    BOOL bReturn = FALSE;
    if (g_pUserDriverContainer)  {
        UserDriver *  pUserDriver = g_pUserDriverContainer->FindDriverByFileContent(fnIoCancelParam.dwContent);
        if (pUserDriver!=NULL) {
            bReturn = pUserDriver->CancelIo(fnIoCancelParam);
            pUserDriver->DeRef();
        }
    }
    return bReturn;
}
extern "C" 
BOOL UD_DevDeviceIoControl(DWORD dwContent, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED * /*pOverlapped*/)
{
    DWORD dwOldLastError = GetLastError();
    UserDriver * pUserDriver = (UserDriver *)dwContent;
    if (!pUserDriver) {
        SetLastError(ERROR_INVALID_PARAMETER);
        ASSERT(FALSE);
        return FALSE;
    }
    SetLastError (0);
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
      case IOCTL_USERDRIVER_CANCELIO:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNCANCELIO_PARAM)) {
            bRet = pUserDriver->CancelIo(*(PFNCANCELIO_PARAM)pInBuf);
        }
        break;
      default:
        ASSERT(FALSE);
        break;
    }

    BOOL fCallSucceeded = FALSE;
    switch (dwIoControlCode) {
      case IOCTL_USERDRIVER_EXIT: 
      case IOCTL_USERDRIVER_INIT:
      case IOCTL_USERDRIVER_DEINIT:
      case IOCTL_USERDRIVER_PREDEINIT:
      case IOCTL_USERDRIVER_POWERDOWN:
      case IOCTL_USERDRIVER_POWERUP:
      case IOCTL_USERDRIVER_OPEN: 
      case IOCTL_USERDRIVER_CLOSE:       
      case IOCTL_USERDRIVER_PRECLOSE:
      case IOCTL_USERDRIVER_IOCTL:
      case IOCTL_USERDRIVER_CANCELIO:
          fCallSucceeded = bRet;
          break;

      case IOCTL_USERDRIVER_READ:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNREAD_PARAM)) {
            fCallSucceeded = bRet && (((PFNREAD_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      case IOCTL_USERDRIVER_WRITE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNWRITE_PARAM)) {
            fCallSucceeded = bRet && (((PFNWRITE_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      case IOCTL_USERDRIVER_SEEK:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNSEEK_PARAM)) {
            fCallSucceeded = bRet && (((PFNSEEK_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      
      default:
        ASSERT(FALSE);
        break;
    }

    /*
    Overall guiding principle: Don't set an error if there is no error.

    if API succeeds and no error code is set, set the last error before the API 
    call (reason: somebody downstream incorrectly calls SetLastError(0) on
    success; violating our guiding principle)

    If API() succeeds, lasterror() might still have been set. Udevice should 
    allow this to percolate up (Ideally should return the same errorcode as 
    before if the API succeeds. But some drivers violate this (one example is 
    network driver: WSS_IoControl which returns errorcode instead of BOOL for 
    xxx_Ioctl call, signaling a success but still setslasterror()).
    This is wrong and should be fixed in the downstream driver. But for now 
    we'll not enforce it.

    If API fails and nobody set any error, set ERROR_GEN_FAILURE (some driver 
    did not set an error code on failure)
    */
    
    if (fCallSucceeded) {
        if (GetLastError() == 0) {
            // api succeeded but no error code set
            SetLastError(dwOldLastError);
        }
        else {
            // api succeeded but downstream driver set an error code.
            // This is wrong and should be fixed in the downstream driver. But
            // for now we'll not enforce it.
        }
    }            
    else if (GetLastError() == 0) {
        // the driver failed, but didn't set an errorcode, set a default
        SetLastError(ERROR_GEN_FAILURE);
    }

    return bRet;
}

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
BOOL 
UDevRefl_DeviceIoControl(
    DWORD /*dwContent*/,
    DWORD dwIoControlCode,
    PVOID pInBuf,
    DWORD nInBufSize,
    PVOID pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned,
    OVERLAPPED * /*pOverlapped*/
)
{
    BOOL bRet = FALSE;
    DWORD dwOldLastError = GetLastError();
    SetLastError (0);

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
      case IOCTL_USERDRIVER_CANCELIO:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNCANCELIO_PARAM)) {
            bRet = HandleDeviceIoCancel(*(PFNCANCELIO_PARAM)pInBuf);
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

    BOOL fCallSucceeded = FALSE;
    switch (dwIoControlCode) {
      case IOCTL_USERDRIVER_LOAD:
      case IOCTL_USERDRIVER_EXIT: 
      case IOCTL_USERDRIVER_INIT:
      case IOCTL_USERDRIVER_DEINIT:
      case IOCTL_USERDRIVER_PREDEINIT:
      case IOCTL_USERDRIVER_POWERDOWN:
      case IOCTL_USERDRIVER_POWERUP:
      case IOCTL_USERDRIVER_OPEN: 
      case IOCTL_USERDRIVER_CLOSE:       
      case IOCTL_USERDRIVER_PRECLOSE:
      case IOCTL_USERDRIVER_IOCTL:
      case IOCTL_SERVICES_IOCTL:
      case IOCTL_USERDRIVER_CANCELIO:
      case IOCTL_USERPROCESSOR_EXIT:
      case IOCTL_USERPROCESSOR_ALIVE:
          fCallSucceeded = bRet;
          break;

      case IOCTL_USERDRIVER_READ:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNREAD_PARAM)) {
            fCallSucceeded = bRet && (((PFNREAD_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      case IOCTL_USERDRIVER_WRITE:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNWRITE_PARAM)) {
            fCallSucceeded = bRet && (((PFNWRITE_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      case IOCTL_USERDRIVER_SEEK:
        if (pInBuf!=NULL && nInBufSize>= sizeof(FNSEEK_PARAM)) {
            fCallSucceeded = bRet && (((PFNSEEK_PARAM)pInBuf)->dwReturnBytes != -1);
        }
        break;
      
      default:
        ASSERT(FALSE);
        break;
    }


    /*
    Overall guiding principle: Don't set an error if there is no error.

    if API succeeds and no error code is set, set the last error before the API 
    call (reason: somebody downstream incorrectly calls SetLastError(0) on
    success; violating our guiding principle)

    If API() succeeds, lasterror() might still have been set. Udevice should 
    allow this to percolate up (Ideally should return the same errorcode as 
    before if the API succeeds. But some drivers violate this (one example is 
    network driver: WSS_IoControl which returns errorcode instead of BOOL for 
    xxx_Ioctl call, signaling a success but still setslasterror()).
    This is wrong and should be fixed in the downstream driver. But for now 
    we'll not enforce it.

    If API fails and nobody set any error, set ERROR_GEN_FAILURE (some driver 
    did not set an error code on failure)
    */
    
    if (fCallSucceeded) {
        if (GetLastError() == 0) {
            // api succeeded but no error code set
            SetLastError(dwOldLastError);
        }
        else {
            // api succeeded but downstream driver set an error code.
            // This is wrong and should be fixed in the downstream driver. But
            // for now we'll not enforce it.
        }
    }            
    else if (GetLastError() == 0) {
        // the driver failed, but didn't set an errorcode, set a default
        SetLastError(ERROR_GEN_FAILURE);
    }

    return bRet;
}

BOOL UDevRefl_CloseFileHandle(DWORD /*dwContext*/)
{
    return TRUE;
}

