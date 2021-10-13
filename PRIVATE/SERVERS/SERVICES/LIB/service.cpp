//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    service.c
 *
 * Purpose: WinCE service manager
 *
 */


#include <windows.h>
#include <service.h>
#include <cardserv.h>
#include <devload.h>
#include <console.h>
#include "serv.h"

// disable 0 length array warning
#pragma warning (disable : 4200)
#include <celog.h>

CRITICAL_SECTION g_ServCS;

// List of services themselves, with service instance handle.
LIST_ENTRY g_ActiveServiceList;

// List of services whose resources have been allocated, but have not been
// added to the g_ActiveServiceList list yet during RegisterDeviceEx.
LIST_ENTRY g_NewServiceList;  

// List of dying services.
LIST_ENTRY g_DyingServiceList;

fsopendev_t *g_lpOpenDevs;
HANDLE g_hDevApiHandle, g_hDevFileApiHandle, g_hCleanEvt;

BOOL ServerLoadInit(void);


#if ! defined (SHIP_BUILD)
// Write load/unload service info to CELog?
BOOL g_fUseCELog;
#endif

#ifndef TARGET_NT
// services.exe specific APIs.
const PFNVOID FDevApiMethods[] = {
//xref ApiSetStart
    (PFNVOID)SERV_ProcNotify,
    (PFNVOID)0,
    (PFNVOID)SERV_ActivateService,
    (PFNVOID)SERV_RegisterService,
    (PFNVOID)SERV_DeregisterService,
    (PFNVOID)SERV_CloseAllServiceHandles,
    (PFNVOID)SERV_CreateServiceHandle,
    (PFNVOID)SERV_GetServiceByIndex,
    (PFNVOID)SERV_ServiceIoControl,
    (PFNVOID)SERV_ServiceAddPort,
    (PFNVOID)SERV_ServiceUnbindPorts,
    (PFNVOID)SERV_EnumServices,
    (PFNVOID)SERV_GetServiceHandle,
    (PFNVOID)SERV_ServiceClosePort,
    
//xref ApiSetEnd
};

#define NUM_FDEV_APIS (sizeof(FDevApiMethods)/sizeof(FDevApiMethods[0]))

const DWORD FDevApiSigs[NUM_FDEV_APIS] = {
    FNSIG3(DW, DW, DW),       // ProcNotify
    FNSIG0(),
    FNSIG2(PTR, DW),          // ActivateService
    FNSIG4(PTR, DW, PTR, DW), // RegisterService
    FNSIG1(DW),               // DeregisterService
    FNSIG1(DW),               // CloseAllServiceHandles
    FNSIG4(PTR, DW, DW, DW),  // CreateServiceHandle
    FNSIG2(DW, PTR),          // GetServiceByIndex
    FNSIG8(DW, DW, PTR, DW, PTR, DW, PTR, PTR), // ServiceIoControl,
    FNSIG5(DW, PTR, DW, DW, PTR),  // ServiceAddPort
    FNSIG1(DW),               // ServiceUnbindPorts
    FNSIG3(PTR,PTR,PTR),      // EnumServices
    FNSIG3(PTR,PTR,PTR)       // GetServiceHandle
};

// Communication from filesys methods to services.exe
const PFNVOID DevFileApiMethods[] = {
    (PFNVOID)SERV_CloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)SERV_ReadFile,
    (PFNVOID)SERV_WriteFile,
    (PFNVOID)SERV_GetFileSize,
    (PFNVOID)SERV_SetFilePointer,
    (PFNVOID)SERV_GetFileInformationByHandle,
    (PFNVOID)SERV_FlushFileBuffers,
    (PFNVOID)SERV_GetFileTime,
    (PFNVOID)SERV_SetFileTime,
    (PFNVOID)SERV_SetEndOfFile,
    (PFNVOID)SERV_DeviceIoControl,
};

#define NUM_FAPIS (sizeof(DevFileApiMethods)/sizeof(DevFileApiMethods[0]))

const DWORD DevFileApiSigs[NUM_FAPIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,PTR,DW,PTR,PTR),  // ReadFile
    FNSIG5(DW,PTR,DW,PTR,PTR),  // WriteFile
    FNSIG2(DW,PTR),             // GetFileSize
    FNSIG4(DW,DW,PTR,DW),       // SetFilePointer
    FNSIG2(DW,PTR),             // GetFileInformationByHandle
    FNSIG1(DW),                 // FlushFileBuffers
    FNSIG4(DW,PTR,PTR,PTR),     // GetFileTime
    FNSIG4(DW,PTR,PTR,PTR),     // SetFileTime
    FNSIG1(DW),                 // SetEndOfFile,
    FNSIG8(DW, DW, PTR, DW, PTR, DW, PTR, PTR), // DeviceIoControl
};
#endif // TARGET_NT

PFNVOID ServicesGetProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) {
    WCHAR szFcnName[MAX_PATH];
    memcpy(szFcnName,type,3*sizeof(WCHAR));
    szFcnName[3] = L'_';
    wcscpy(&szFcnName[4],lpszName);

    return (PFNVOID)GetProcAddress(hInst, szFcnName);
}

static BOOL NotSupportedBool (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

static DWORD NotSupportedDword (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return -1;
}

// We need to spin a thread to load and free libraries because it is unsafe
// to do it under the context of a PSL caller thread.
DWORD WINAPI ServicesFreeLibraryThread(LPVOID lpv) {
    FreeLibrary((HINSTANCE) lpv);
    return 0;
}

typedef DWORD (WINAPI *PFN_SERVICE_THREAD)(LPVOID lpv);


// A number of operatations, in particular anything using TLS, must not
// be performed in the context of a process calling into services.exe via a PSL
// but must instead be called with services.exe as the owner.  To make this
// happen we create a thread in services.exe and block until its completed.

// By convention, pFunc should return ERROR_SUCCESS on successful operation and an error otherwise.
DWORD ServiceSpinThreadAndWait(PFN_SERVICE_THREAD pFunc, LPVOID lpv) {
	HANDLE hThread = CreateThread(NULL,0,pFunc,lpv,0,NULL);

	if (hThread) {
        DWORD dwRetVal;
        WaitForSingleObject(hThread,INFINITE);

        if (GetExitCodeThread(hThread,&dwRetVal)) {
            CloseHandle(hThread);
            return dwRetVal;
        }
        else {
        	DEBUGMSG(ZONE_ERROR,(L"SERVICES: GetExitCodeThread failed, GLE=0x%08x\r\n",GetLastError()));
            DEBUGCHK(0);
            CloseHandle(hThread);
            goto errret;
        }
	}
#if defined (DEBUG)
	else {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: Unable to spin a LoadLibrary thread, forced to load library on current thread.  GLE=0x%08x\r\n",GetLastError()));
	}
#endif

errret:
    DWORD dwErr = GetLastError();
    
    if (dwErr == 0) {
        // Getting to this block indicates a system error, sys should've set an err code.
        // If it did not we still return err, because calling function relies on return
        // to determine flow of control.
        DEBUGCHK(0);
        dwErr = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    }

	return dwErr;
}

#if ! defined (SHIP_BUILD)
#define ServicesCeLog(args)  if (g_fUseCELog) RETAILCELOGMSG(1,args)
#else
#define ServicesCeLog(args)  ((void)0)
#endif // SHIP_BUILD

void FreeService(fsdev_t *lpdev, BOOL fRemoveFromDyingList, BOOL fSpinThread)  {
    EnterCriticalSection(&g_ServCS);
    DEBUGCHK(! IsValidService(lpdev));

    if (fRemoveFromDyingList) {
        DEBUGCHK( IsValidService(lpdev,&g_DyingServiceList));
        RemoveEntryList((PLIST_ENTRY)lpdev);
    }
    LeaveCriticalSection(&g_ServCS);

    if (lpdev->hLib && fSpinThread) {
        // when being called from a PSL, spin a thread so that services.exe owns
        // library unload.  This is to keep TLS from getting confused.
        ServiceSpinThreadAndWait(ServicesFreeLibraryThread, lpdev->hLib);
    }
    else if (lpdev->hLib) {
        // we're being called from DeregisterServiceThread or another thread
        // that services.exe owns, so it's safe to unload in this context without spinning
        // a new thread.
        VERIFY_SERVICES_EXE_IS_OWNER();
        FreeLibrary(lpdev->hLib);
    }

    if (lpdev->szDllName)
        LocalFree(lpdev->szDllName);
    if (lpdev->szRegPath)
        LocalFree(lpdev->szRegPath);

    LocalFree(lpdev);
}

fsdev_t *FindPrefixInList(LPCWSTR szPrefix, DWORD dwIndex, PLIST_ENTRY pList) {
    fsdev_t *lpTrav;

    for (lpTrav = (fsdev_t *)pList->Flink;
         lpTrav != (fsdev_t *)pList;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
         if (!_wcsnicmp(lpTrav->type,szPrefix,sizeof(lpTrav->type)/sizeof(WCHAR)) && (lpTrav->index == dwIndex)) {
              return lpTrav;
         }
    }
    return NULL;
}


//      @func   HANDLE | RegisterService | Register a new service
//  @parm       LPCWSTR | lpszType | service id (SER, PAR, AUD, etc) - must be 3 characters long
//  @parm       DWORD | dwIndex | index between 0 and 9, ie: COM2 would be 2
//  @parm       LPCWSTR | lpszLib | dll containing driver code
//  @parm       DWORD | dwInfo | instance information
//      @rdesc  Returns a handle to a service, or 0 for failure.  This handle should
//                      only be passed to DeRegisterService.
//      @comm   For stream based services, the services will be .dll files.
//                      Each service is initialized by
//                      a call to the RegisterService API (performed by the server process).
//                      The lpszLib parameter is what will be to open the service.  The
//                      lpszType is a three character string which is used to identify the 
//                      function entry points in the DLL, so that multiple services can exist
//                      in one DLL.  The lpszLib is the name of the DLL which contains the
//                      entry points.  Finally, dwInfo is passed in to the init routine. 
//                      Most likely, the server process will read this information out of a
//                      table or registry in order to initialize all devices at startup time.
//      @xref <f DeregisterService>
//

HANDLE
SERV_RegisterService(
    LPCWSTR lpszType,
    DWORD   dwIndex,
    LPCWSTR lpszLib,
    DWORD   dwInfo
    )
{
    return RegisterServiceEx(
                lpszType,
                dwIndex,
                lpszLib,
                dwInfo,
                DEVFLAGS_NONE,
                NULL
                );
}

// Does actual work of loading service DLL into memory and calling xxx_Init.
// MUST be done in context of services.exe as owner process, not PSL.
DWORD WINAPI RegisterServiceThread(LPVOID lpv) {
    DWORD dwErr = ERROR_SUCCESS;
    fsdev_t *lpdev   = (fsdev_t*) lpv;

    VERIFY_SERVICES_EXE_IS_OWNER();

    // load library and get function pointers setup.
    lpdev->hLib = (lpdev->dwFlags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(lpdev->szDllName) : LoadDriver(lpdev->szDllName);
    if (!lpdev->hLib) {
        dwErr = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    lpdev->fnInit = (pInitFn)ServicesGetProcAddr(lpdev->type,L"Init",lpdev->hLib);
    lpdev->fnDeinit = (pDeinitFn)ServicesGetProcAddr(lpdev->type,L"Deinit",lpdev->hLib);
    lpdev->fnOpen = (pOpenFn)ServicesGetProcAddr(lpdev->type,L"Open",lpdev->hLib);
    lpdev->fnClose = (pCloseFn)ServicesGetProcAddr(lpdev->type,L"Close",lpdev->hLib);
    lpdev->fnRead = (pReadFn)ServicesGetProcAddr(lpdev->type,L"Read",lpdev->hLib);
    lpdev->fnWrite = (pWriteFn)ServicesGetProcAddr(lpdev->type,L"Write",lpdev->hLib);
    lpdev->fnSeek = (pSeekFn)ServicesGetProcAddr(lpdev->type,L"Seek",lpdev->hLib);
    lpdev->fnControl = (pControlFn)ServicesGetProcAddr(lpdev->type,L"IOControl",lpdev->hLib);

    if (!(lpdev->fnInit && lpdev->fnDeinit && lpdev->fnControl)) {
        dwErr = ERROR_INVALID_FUNCTION;
        goto done;
    }

    if (!lpdev->fnOpen) lpdev->fnOpen = (pOpenFn) NotSupportedBool;
    if (!lpdev->fnClose) lpdev->fnClose = (pCloseFn) NotSupportedBool;
    if (!lpdev->fnControl) lpdev->fnControl = (pControlFn) NotSupportedBool;
    if (!lpdev->fnRead) lpdev->fnRead = (pReadFn) NotSupportedDword;
    if (!lpdev->fnWrite) lpdev->fnWrite = (pWriteFn) NotSupportedDword;
    if (!lpdev->fnSeek) lpdev->fnSeek = (pSeekFn) NotSupportedDword;

    lpdev->dwRefCnt = 1;

    // check for uniqueness
    EnterCriticalSection (&g_ServCS);
    if (FindPrefixInList(lpdev->type,lpdev->index,&g_ActiveServiceList) || FindPrefixInList(lpdev->type,lpdev->index,&g_NewServiceList) ||
        FindPrefixInList(lpdev->type,lpdev->index,&g_DyingServiceList)) {
        LeaveCriticalSection(&g_ServCS);
        dwErr = ERROR_DEVICE_IN_USE;
        goto done;
    }

    InsertTailList(&g_NewServiceList,(PLIST_ENTRY)lpdev);
    LeaveCriticalSection(&g_ServCS);

    // call xxx_Init
    __try {
		lpdev->dwData = lpdev->fnInit(lpdev->dwContext);

        if (!(lpdev->dwData)) {
            dwErr = ERROR_OPEN_FAILED;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (lpdev->dwFlags & DEVFLAGS_UNLOAD && (dwErr == ERROR_SUCCESS)) {
        // No app can set DEVFLAGS_UNLOAD (only settable through reg config),
        // err code isn't important as long as it causes library to get freed.
        dwErr = ERROR_INTERNAL_ERROR;
    }

    // regardless of dwErr code, fall through to this code to take service of g_NewServiceList.
    EnterCriticalSection(&g_ServCS);
    DEBUGCHK(IsValidService(lpdev,&g_NewServiceList));
    RemoveEntryList((PLIST_ENTRY)lpdev);

    if (ERROR_SUCCESS == dwErr) {
        InsertTailList(&g_ActiveServiceList, (PLIST_ENTRY)lpdev);
    }
    LeaveCriticalSection(&g_ServCS);

done:
    if (ERROR_SUCCESS != dwErr && lpdev->hLib) {
        // Free the library here (because we'd have to spin up another thread
        // in RegisterService call to FreeService otherwise) but don't free lpdev 
        // memory structures, let RegisterServiceEx handle this since it created it.
        FreeLibrary(lpdev->hLib);
        lpdev->hLib = 0;
    }
    return dwErr;
}

HANDLE
RegisterServiceEx(
    LPCWSTR lpszType,
    DWORD dwIndex,
    LPCWSTR lpszLib,
    DWORD dwInfo,
    DWORD dwFlags,
    LPCWSTR lpszFullRegPath
    )
{
    HANDLE hDev = 0;
    fsdev_t *lpdev = 0;
    DWORD dwErr = ERROR_SUCCESS;
    BOOL  fInCreateDevList = FALSE;

    ServicesCeLog((L"SERVICES: +RegisterService(%.5s,%d,%.16s,%d)",lpszType,dwIndex,lpszLib,dwInfo));

    lpszType = (LPWSTR) ValidatePWSTR((LPWSTR)lpszType);
    lpszLib  = (LPWSTR) ValidatePWSTR((LPWSTR)lpszLib);
    // lpszFullRegPath can only be set internally be services.exe so always trust it.

    if (!lpszType || !lpszLib || dwIndex > 9) {
        dwErr = ERROR_INVALID_PARAMETER;
        goto errret;
    }

    if (!(lpdev = (fsdev_t*) LocalAlloc(LPTR,sizeof(fsdev_t)))) {
        dwErr = ERROR_OUTOFMEMORY;
        goto errret;
    }
    
    __try {
        // populate lpdev structure.
        memset(lpdev, 0, sizeof(fsdev_t));
        memcpy(lpdev->type,lpszType,sizeof(lpdev->type));
        lpdev->index     = dwIndex;
        lpdev->dwContext = dwInfo;
        lpdev->dwFlags   = dwFlags;

        lpdev->dwDllNameLen = wcslen(lpszLib);
        if (NULL == (lpdev->szDllName = (WCHAR *) LocalAlloc(LMEM_FIXED,sizeof(WCHAR)*(lpdev->dwDllNameLen+1)))) {
            dwErr = ERROR_OUTOFMEMORY;
            goto errret;
        }
        wcscpy(lpdev->szDllName,lpszLib);

        if (lpszFullRegPath) {
            lpdev->dwRegPathLen = wcslen(lpszFullRegPath);
            if (NULL == (lpdev->szRegPath = (WCHAR*) LocalAlloc(LMEM_FIXED,(lpdev->dwRegPathLen+1)*sizeof(WCHAR)))) {
                dwErr = ERROR_OUTOFMEMORY;
                goto errret;
            }
            wcscpy(lpdev->szRegPath,lpszFullRegPath);
        }

        // if services.exe owns the thread already, don't bother creating a worker.
        if ((DWORD)GetOwnerProcess() == GetCurrentProcessId())
            dwErr = RegisterServiceThread(lpdev);
        else
            dwErr = ServiceSpinThreadAndWait(RegisterServiceThread,lpdev);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

errret:
    if (dwErr) {
        if (lpdev)
            FreeService(lpdev,FALSE);

        SetLastError(dwErr);
  	    DEBUGMSG(ZONE_ERROR,(L"SERVICES!RegisterService fails, GLE=0x%08x\r\n",dwErr));
    }
    else {
        DEBUGCHK(lpdev && lpdev->szDllName && lpdev->hLib);
        hDev = (HANDLE)lpdev;
    }

    ServicesCeLog((L"SERVICES: -RegisterService returns hDev=0x%08x, LastError=0x%08x",hDev,dwErr));
    return hDev;
}


//
// IsValidService - function to check if passed in Service handle is actually a
// valid registered Service.
//
// Return TRUE if it is a valid registered Service, FALSE otherwise.
//
BOOL IsValidService(fsdev_t * lpdev,PLIST_ENTRY pList) {
    fsdev_t * lpTrav;
    PLIST_ENTRY pSearchList = pList ? pList : &g_ActiveServiceList;

    for (lpTrav = (fsdev_t *)pSearchList->Flink;
         lpTrav != (fsdev_t *)pSearchList;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
        if (lpTrav == lpdev) {
            return TRUE;
        }
    }
    return FALSE;
}

//
// IsValidFileHandle - called to make sure passed file handle is valid.
//
BOOL IsValidFileHandle(fsopendev_t *fsodev) {
    fsopendev_t *lpTrav = g_lpOpenDevs;
    while (lpTrav) {
         if (lpTrav == fsodev) {
             DEBUGCHK(lpTrav->lpDev);
             return TRUE;
         }
         lpTrav = lpTrav->nextptr;
    }
    return FALSE;
}

void OpenAddRef(fsopendev_t *fsodev) {
    fsodev->dwOpenRefCnt++;
    fsodev->lpDev->dwRefCnt++;
}

void OpenDelRef(fsopendev_t *fsodev) {
    fsodev->lpDev->dwRefCnt--;
    if (fsodev->lpDev->dwRefCnt == 0) {
        LeaveCriticalSection(&g_ServCS);
        FreeService(fsodev->lpDev);
        EnterCriticalSection(&g_ServCS);
    }

    fsodev->dwOpenRefCnt--;
    if (fsodev->dwOpenRefCnt == 0) {
        DEBUGCHK(!IsValidFileHandle(fsodev));
        LocalFree(fsodev);  
    }
}

BOOL WillServiceDeinit(fsdev_t *lpdev) {
    DWORD dwBufOut = 1;

    // Before calling xxx_Deinit on a service, we call it with IOCTL_SERVICE_CAN_DEINIT.  If 
    // the service returns TRUE (indicating it processed the response) and if it set dwBufOut=0,
    // then we won't dereference service or call xxx_Deinit on it.
    if (InternalServiceIOControl(lpdev,IOCTL_SERVICE_QUERY_CAN_DEINIT,0,0,(PBYTE)&dwBufOut,sizeof(dwBufOut))) {
	    return (dwBufOut == 0) ? FALSE : TRUE;
    }
    return TRUE;
}

// Calls xxx_DeInit on a services and unloads it (iff the ref count is 0).
// Must be done in context of services.exe as owner, not from a PSL call.

DWORD WINAPI DeregisterServiceThread(LPVOID lpv) {
    fsdev_t *lpdev = (fsdev_t*) lpv;

    VERIFY_SERVICES_EXE_IS_OWNER();

    __try {
        lpdev->fnDeinit(lpdev->dwData);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!DeregisterService:xxx_Deinit on handle 0x%08x throws exception 0x%08x\r\n",lpdev,GetExceptionCode()));
    }

    EnterCriticalSection(&g_ServCS);
    DEBUGCHK(IsValidService(lpdev,&g_DyingServiceList));

    lpdev->dwRefCnt--;
    if (lpdev->dwRefCnt == 0) {
        LeaveCriticalSection(&g_ServCS);
        FreeService(lpdev,TRUE,FALSE);
    } else {
        DEBUGMSG(ZONE_DYING, (L"SERVICES!DeregisterService:Not all references to service(0x%08x) have been removed\r\n",lpdev));
        LeaveCriticalSection(&g_ServCS);
    }

    return ERROR_SUCCESS;
}

//      @func   BOOL | DeregisterService | Deregister a registered Service
//      @parm   HANDLE | hDevice | handle to registered service, from RegisterService
//      @rdesc  Returns TRUE for success, FALSE for failure
//      @comm   DeregisterService can be used if a service is being shut down.
//                      An example would be:<nl>
//                      <tab>DeregisterService(h1);<nl>
//                      where h1 was returned from a call to RegisterService.
//      @xref <f RegisterService>
BOOL SERV_DeregisterService(HANDLE hDevice)
{
    fsdev_t *lpdev;
    BOOL retval = FALSE;

    ServicesCeLog((L"SERVICES: +DeregisterService(0x%08x)",hDevice));

    EnterCriticalSection(&g_ServCS);
    __try {
        lpdev = (fsdev_t *)hDevice;
        if (!IsValidService(lpdev)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            LeaveCriticalSection(&g_ServCS);
            goto errret;
        }

        if (! WillServiceDeinit(lpdev)) {
            SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
            LeaveCriticalSection(&g_ServCS);
            goto errret;
        }
        
        RemoveEntryList((PLIST_ENTRY)lpdev);
        InsertTailList(&g_DyingServiceList,(PLIST_ENTRY)lpdev);

        UnloadSuperServices(lpdev,TRUE,TRUE);  // releases critical section and doesn't reclaim it.

        // if services.exe owns the thread already, don't bother creating a worker.
        if ((DWORD)GetOwnerProcess() == GetCurrentProcessId())
            retval = (ERROR_SUCCESS == DeregisterServiceThread(lpdev));
        else
            retval = (ERROR_SUCCESS == ServiceSpinThreadAndWait(DeregisterServiceThread,lpdev));

errret:
        ;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // no need to leave critical section here because it's 
        // been freed after UnloadSuperServices(), only a .exe
        // (not garbage application data or buggy service) could cause an AV 
        // to get us to this block before then.
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    ServicesCeLog((L"SERVICES: -DeregisterService() returns %d",retval));
    return retval;
}

#define DEVICE_NAME_SIZE 6  // i.e. "COM1:" (includes space for 0 terminator)

void FormatDeviceName(LPWSTR lpszName, fsdev_t * lpdev)
{
    memcpy(lpszName, lpdev->type, sizeof(lpdev->type));
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+0] = (WCHAR)(L'0' + lpdev->index);
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+1] = L':';
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+2] = 0;
}

BOOL SERV_GetServiceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    fsdev_t *lpdev;
    BOOL bRet = FALSE;

    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        lpFindFileData = (LPWIN32_FIND_DATA) MapCallerPtr(lpFindFileData,sizeof(WIN32_FIND_DATA));
        if (!lpFindFileData) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    ENTER_DEVICE_FUNCTION {
        lpdev = (fsdev_t *)g_ActiveServiceList.Flink;
        while (dwIndex && lpdev != (fsdev_t *)&g_ActiveServiceList) {
            dwIndex--;
            lpdev = (fsdev_t *)lpdev->list.Flink;
        }
        if (lpdev != (fsdev_t *)&g_ActiveServiceList) {
            lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            *(__int64 *)&lpFindFileData->ftCreationTime = 0;
            *(__int64 *)&lpFindFileData->ftLastAccessTime = 0;
            *(__int64 *)&lpFindFileData->ftLastWriteTime = 0;
            lpFindFileData->nFileSizeHigh = 0;
            lpFindFileData->nFileSizeLow = 0;
            lpFindFileData->dwOID = 0xffffffff;
            FormatDeviceName(lpFindFileData->cFileName, lpdev);
            bRet = TRUE;
        }
    } EXIT_DEVICE_FUNCTION;
    return bRet;
}

// assumes len == 5 and lpnew[4] == ':'
HANDLE SERV_CreateServiceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc) {
    HANDLE hDev = INVALID_HANDLE_VALUE;
    DWORD dwErrCode = ERROR_DEV_NOT_EXIST;
    fsopendev_t *lpopendev = NULL;
    fsdev_t *lpdev = NULL;
    BOOL    fDoDec = FALSE;
    BOOL    fFreeDev = FALSE;

    if (NULL == (lpNew = (LPWSTR) ValidatePWSTR((LPWSTR)lpNew))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    EnterCriticalSection(&g_ServCS);
    __try {
        for (lpdev = (fsdev_t *)g_ActiveServiceList.Flink;
             lpdev != (fsdev_t *)&g_ActiveServiceList;
             lpdev = (fsdev_t *)lpdev->list.Flink) {
            if (!_wcsnicmp(lpNew,lpdev->type,
                     sizeof(lpdev->type)/sizeof(WCHAR))) {
                if ((DWORD)(lpNew[3]-L'0') == lpdev->index) {
                    if (!(lpopendev = (fsopendev_t *) LocalAlloc(LPTR,sizeof(fsopendev_t)))) {
                        dwErrCode = ERROR_OUTOFMEMORY;
                        goto errret;
                    }
                    lpopendev->lpDev = lpdev;
                    lpdev->dwRefCnt++;
                    fDoDec = TRUE;
                    LeaveCriticalSection(&g_ServCS);

                    __try {
                         lpopendev->dwOpenData = lpdev->fnOpen(lpdev->dwData,dwAccess,dwShareMode);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        DEBUGMSG(ZONE_ERROR,(L"SERVICES!CreateHandle:xxx_Open(%s) throws exception 0x%08x\r\n",lpNew,GetExceptionCode()));
                        lpopendev->dwOpenData = 0;
                    }

                    EnterCriticalSection(&g_ServCS);
                    fDoDec = FALSE;
                    lpdev->dwRefCnt--;
                    if ((!lpopendev->dwOpenData) || (!IsValidService(lpdev))) {
                        fFreeDev = (lpdev->dwRefCnt == 0);
                        dwErrCode = ERROR_OPEN_FAILED;
                        goto errret;
                    }
                    if (!(hDev = CreateAPIHandle(g_hDevFileApiHandle, lpopendev))) {
                        hDev = INVALID_HANDLE_VALUE;
                        dwErrCode = ERROR_OUTOFMEMORY;
                        goto errret;
                    }
                    // OK, we managed to create the handle
                    dwErrCode = 0;
                    lpopendev->hProc = hProc;
                    lpopendev->KHandle = hDev;
                    lpopendev->dwOpenRefCnt = 1;
                    lpopendev->nextptr = g_lpOpenDevs;
                    g_lpOpenDevs = lpopendev;
                    break;
                }
            }
        }
errret:
        if( dwErrCode ) {
            if (lpopendev)
                LocalFree(lpopendev);

            SetLastError( dwErrCode );
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
        if (lpopendev)
            LocalFree(lpopendev);
        if (fDoDec) {
            EnterCriticalSection(&g_ServCS);
            lpdev->dwRefCnt--;
            fFreeDev = (lpdev->dwRefCnt == 0);
            LeaveCriticalSection(&g_ServCS);
        }
    }
    LeaveCriticalSection(&g_ServCS);

    if (fFreeDev) {
        FreeService(lpdev);
    }
    return hDev;
}

BOOL SERV_CloseFileHandle(fsopendev_t *fsodev)
{
    BOOL retval = FALSE;
    fsopendev_t *fsTrav;

    EnterCriticalSection(&g_ServCS);
    if (g_lpOpenDevs == fsodev)
        g_lpOpenDevs = fsodev->nextptr;
    else {
        fsTrav = g_lpOpenDevs;
        while (fsTrav != NULL && fsTrav->nextptr != fsodev) {
            fsTrav = fsTrav->nextptr;
        }
        if (fsTrav != NULL) {
            fsTrav->nextptr = fsodev->nextptr;
        } else {
            LeaveCriticalSection(&g_ServCS);
            DEBUGMSG(ZONE_ERROR, (L"SERVICES!CloseHandle fails, fsodev (0x%08x) not in list\r\n", fsodev));
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }
    DEBUGCHK(fsodev->dwOpenRefCnt);

    if (IsValidService(fsodev->lpDev)) {
        OpenAddRef(fsodev);
        LeaveCriticalSection(&g_ServCS);
        __try {
            retval = fsodev->lpDev->fnClose(fsodev->dwOpenData);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(ZONE_ERROR,(L"SERVICES!CloseHandle:xxx_Close on handle 0x%08x throws exception 0x%08x\r\n",fsodev,GetExceptionCode()));
        }

        EnterCriticalSection(&g_ServCS);
        DEBUGCHK(fsodev->dwOpenRefCnt > 1);
        OpenDelRef(fsodev);
    }
    fsodev->dwOpenRefCnt--;

    if (!fsodev->dwOpenRefCnt) {
        LocalFree(fsodev);
    }
    LeaveCriticalSection(&g_ServCS);
    return retval;
}

// If we don't trust a pointer we can't do a strlen on it.  If first few bytes are
// legit assume rest of it's OK, too.  Since services.exe never writes out to
// strings (all write buffers require size) this is safe.
PSTR ValidatePSTR(const PSTR psz, const DWORD cbLen) {
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        return (PSTR) MapCallerPtr(psz,cbLen);
    }
    return psz;
}

PWSTR ValidatePWSTR(const PWSTR psz, const DWORD cbLen) {
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        return (PWSTR) MapCallerPtr(psz,cbLen);
    }
    return psz;
}

HANDLE SERV_ActivateService(LPCWSTR lpszDevKey, DWORD dwClientInfo) {
    if (NULL == (lpszDevKey = (LPWSTR)ValidatePWSTR((LPWSTR)lpszDevKey))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    return StartOneService(lpszDevKey,dwClientInfo,FALSE);
}


// The return buffer is filled with a double-NULL terminated list of
// NULL terminated strings of the device names, for instance:
// HTP0:\\0TEL0:\\0TEL1:\\0\\0
BOOL SERV_EnumServices(PBYTE pBuffer, DWORD *pdwServiceEntries, DWORD *pdwBufferLen) {
    DWORD      dwRet = ERROR_INVALID_PARAMETER;
    fsdev_t    *pTrav;
    DWORD      cbBytesRequired = 0;
    DWORD      dwCurrent       = 0;
    DWORD      dwServiceEntries = 0;
    ServiceEnumInfo *pEnumTrav;
    WCHAR      *szWriteTrav;
    DWORD      i =0;

    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pdwBufferLen = (PDWORD) MapCallerPtr(pdwBufferLen,sizeof(DWORD));
        pdwServiceEntries = (PDWORD) MapCallerPtr(pdwServiceEntries,sizeof(DWORD));
    }

    if (!pdwServiceEntries || !pdwBufferLen) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (pBuffer && *pdwBufferLen && (PSLGetCallerTrust() != OEM_CERTIFY_TRUST)) {
        pBuffer = (PBYTE) MapCallerPtr(pBuffer,*pdwBufferLen);

        if (!pBuffer) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

	EnterCriticalSection(&g_ServCS);
    for (pTrav = (fsdev_t *)g_ActiveServiceList.Flink;
         pTrav != (fsdev_t *)&g_ActiveServiceList;
         pTrav = (fsdev_t *)pTrav->list.Flink) {
         cbBytesRequired += (1+pTrav->dwDllNameLen)*sizeof(WCHAR);
         dwServiceEntries++;
    }
    cbBytesRequired += (sizeof(ServiceEnumInfo) * dwServiceEntries);

    *pdwServiceEntries = dwServiceEntries;

    if (!pBuffer) {
        *pdwBufferLen = cbBytesRequired;
        dwRet = ERROR_INSUFFICIENT_BUFFER;
        goto done;
    }
    else if (*pdwBufferLen < cbBytesRequired) {
        *pdwBufferLen = cbBytesRequired;
        dwRet = ERROR_MORE_DATA;
        goto done;
    }
    *pdwBufferLen = cbBytesRequired;

    pEnumTrav   = (ServiceEnumInfo*) pBuffer;
    szWriteTrav = (WCHAR*) (pBuffer + (dwServiceEntries * sizeof(ServiceEnumInfo)));

    for (pTrav = (fsdev_t *)g_ActiveServiceList.Flink;
         pTrav != (fsdev_t *)&g_ActiveServiceList;
         pTrav = (fsdev_t *)pTrav->list.Flink) {

         FormatDeviceName(pEnumTrav->szPrefix,pTrav);
         pEnumTrav->hServiceHandle = (HANDLE) pTrav;

         wcscpy(szWriteTrav,pTrav->szDllName);
         pEnumTrav->szDllName = szWriteTrav;
         szWriteTrav += (pTrav->dwDllNameLen + 1);

         pEnumTrav++;
    }

    // save IOControls until the end because the g_ActiveServiceList may change
    // between calls since we have to yield CS, however pEnumTrav has all the
    // info we need.
    pEnumTrav = (ServiceEnumInfo*) pBuffer;

    for (i = 0; i < dwServiceEntries; i++) {
        DWORD dwServiceState;

    	if (IsValidService((fsdev_t *)pEnumTrav->hServiceHandle)) {
            if (InternalServiceIOControl((fsdev_t *)pEnumTrav->hServiceHandle,IOCTL_SERVICE_STATUS,NULL,0,(PBYTE)&dwServiceState,sizeof(dwServiceState)))
                pEnumTrav->dwServiceState = dwServiceState;
            else
                pEnumTrav->dwServiceState = SERVICE_STATE_UNKNOWN;
    	}
    	else {
            // service has unloaded between calls.  Though we could remove this entry
            // from return list, probably not worth the effort.  Users should realize
            // that since services.exe is dynamic, services can change without warning.
            pEnumTrav->dwServiceState = SERVICE_STATE_UNKNOWN;
    	}
        pEnumTrav++;
    }
    
    DEBUGCHK((CHAR*)pEnumTrav == (CHAR*)(pBuffer + (dwServiceEntries * sizeof(ServiceEnumInfo))));

    dwRet = ERROR_SUCCESS;
done:
	if (dwRet)
	    SetLastError(dwRet);

	LeaveCriticalSection(&g_ServCS);
	return (dwRet == ERROR_SUCCESS) ? TRUE : FALSE;
}

// Takes a Service Prefix identifier and returns a service handle value
HANDLE SERV_GetServiceHandle(LPWSTR szPrefix, LPWSTR szDllName, DWORD *pdwDllBuf) {
    HANDLE  hService = INVALID_HANDLE_VALUE;
    DWORD   dwIndex = szPrefix ? szPrefix[3] - L'0' : 0;
    fsdev_t *pDevice;

    szPrefix = ValidatePWSTR(szPrefix,6);

    if (!szPrefix   || !szPrefix[0]         || !szPrefix[1]   || !szPrefix[2] ||
        dwIndex > 9 ||  szPrefix[4] != L':' ||  szPrefix[5])    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return hService;
    }

    if (szDllName && pdwDllBuf && (PSLGetCallerTrust() != OEM_CERTIFY_TRUST)) {
    	pdwDllBuf = (PDWORD) MapCallerPtr(pdwDllBuf,sizeof(DWORD));
    	if (pdwDllBuf)
    		szDllName = (LPWSTR) MapCallerPtr(szDllName,*pdwDllBuf);

    	if (! (szDllName && pdwDllBuf)) {
    		SetLastError(ERROR_INVALID_PARAMETER);
    		return hService;
    	}
    }

    EnterCriticalSection(&g_ServCS);
    if (pDevice = FindPrefixInList(szPrefix,dwIndex,&g_ActiveServiceList)) {
        // szDllName is completly optional, no error if it's NULL
        if (szDllName) {
            if (!pdwDllBuf) {
                SetLastError(ERROR_INVALID_PARAMETER);
            }
            else if (*pdwDllBuf < (pDevice->dwDllNameLen+1)*sizeof(WCHAR)) {
                SetLastError(ERROR_MORE_DATA);
            }
            else {
                wcscpy(szDllName,pDevice->szDllName);
                hService = (HANDLE) pDevice;
            }
            if (pdwDllBuf)
                *pdwDllBuf = (pDevice->dwDllNameLen+1)*sizeof(WCHAR);
        }
        else
            hService = pDevice;
    }
    else {
        SetLastError(ERROR_NOT_FOUND);
    }
    LeaveCriticalSection(&g_ServCS);

    return hService;
}

BOOL SERV_ReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    DWORD dodec = 0;

    EnterCriticalSection(&g_ServCS);
    if (! IsValidFileHandle(fsodev) || !IsValidService(fsodev->lpDev)) {
        LeaveCriticalSection(&g_ServCS);
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!ReadFile (fsdev=0x%08x) fails, handle not valid\r\n",fsodev));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    OpenAddRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    __try {
        *lpNumBytesRead = fsodev->lpDev->fnRead(fsodev->dwOpenData,buffer,nBytesToRead);
        if (*lpNumBytesRead == 0xffffffff) {
            retval = FALSE;
            *lpNumBytesRead = 0;
        } else
            retval = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!xxx_ReadFile throws exception 0x%08x\r\n",GetExceptionCode()));
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    EnterCriticalSection(&g_ServCS);
    OpenDelRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    DEBUGCHK(*lpNumBytesRead <= nBytesToRead);

    return retval;
}

BOOL SERV_WriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    DWORD dodec = 0;

    EnterCriticalSection(&g_ServCS);
    if (! IsValidFileHandle(fsodev) || !IsValidService(fsodev->lpDev)) {
        LeaveCriticalSection(&g_ServCS);
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!WriteFile (fsdev=0x%08x) fails, handle not valid\r\n",fsodev));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    OpenAddRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    __try {
        *lpNumBytesWritten = fsodev->lpDev->fnWrite(fsodev->dwOpenData,buffer,nBytesToWrite);
        if (*lpNumBytesWritten == 0xffffffff) {
            retval = FALSE;
            *lpNumBytesWritten = 0;
        } else
            retval = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!xxx_WriteFile throws exception 0x%08x\r\n",GetExceptionCode()));
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    EnterCriticalSection(&g_ServCS);
    OpenDelRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    return retval;
}

DWORD SERV_SetFilePointer(fsopendev_t *fsodev, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
    DWORD dwMoveMethod) {
    DWORD retval = 0xffffffff;

    EnterCriticalSection(&g_ServCS);
    if (! IsValidFileHandle(fsodev) || !IsValidService(fsodev->lpDev)) {
        LeaveCriticalSection(&g_ServCS);
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!SetFilePointer (fsdev=0x%08x) fails, handle not valid\r\n",fsodev));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    OpenAddRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    __try {
        retval = fsodev->lpDev->fnSeek(fsodev->dwOpenData,lDistanceToMove,dwMoveMethod);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!xxx_SetFilePointer throws exception 0x%08x\r\n",GetExceptionCode()));
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    EnterCriticalSection(&g_ServCS);
    OpenDelRef(fsodev);
    LeaveCriticalSection(&g_ServCS);

    return retval;
}

// This IOControl helper is used for app generated IO Control calls (not
// internal to services.exe).  Handles special case of starting and stopping service.
// Assumes that the service ref count has been increment and that lock is held when called.
BOOL ServIOControl(fsdev_t *lpdev, DWORD dwData, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned) {
    BOOL fRet = FALSE;

    LeaveCriticalSection(&g_ServCS);
    __try {
        fRet = lpdev->fnControl(dwData,dwIoControlCode,(PBYTE)lpInBuf,nInBufSize,(PBYTE)lpOutBuf,nOutBufSize,lpBytesReturned);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!xxx_DeviceIOControl throws exception 0x%08x\r\n",GetExceptionCode()));
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    if (fRet && (lpdev->dwContext & SERVICE_INIT_STOPPED) && (dwIoControlCode == IOCTL_SERVICE_STOP)) {
        UnloadSuperServices(lpdev,FALSE,FALSE);
    }

    if (fRet && lpdev->szRegPath && (lpdev->dwContext & SERVICE_INIT_STOPPED) && (dwIoControlCode == IOCTL_SERVICE_START)) {
        StartSuperService(lpdev->szRegPath,(HANDLE) lpdev,FALSE);
    }

    EnterCriticalSection(&g_ServCS);
    return fRet;
}


// SERV_DeviceIoControl exepcts a valid file handle to be passed. (i.e. it
// was opened via CreateFile("XXX#:",...).
BOOL SERV_DeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    BOOL retval = FALSE;
    DWORD dodec = 0;

    EnterCriticalSection(&g_ServCS);
    if (! IsValidFileHandle(fsodev) || !IsValidService(fsodev->lpDev)) {
        LeaveCriticalSection(&g_ServCS);
        DEBUGMSG(ZONE_ERROR,(L"SERVICES!SetFilePointer (fsdev=0x%08x) fails, handle not valid\r\n",fsodev));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(dwIoControlCode == IOCTL_DEVICE_AUTODEREGISTER) {
        // NOTE: This IOCTL is here to avoid a deadlock between (a) a DllMain calling
        // DeregisterDevice (and hence trying to acquire the device CS *after* the 
        // loader CS and (b) Various other paths in this file where we acquire the 
        // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc). 
        // See WinCE#4165. Hence this code path must NOT acquire the device CS!!
        DEBUGMSG(ZONE_DYING, (L"SERV_DeviceIoControl:IOCTL_DEVICE_AUTODEREGISTER. lpdev=%x\r\n", fsodev->lpDev));
        fsodev->lpDev->wFlags |= DEVFLAGS_AUTO_DEREGISTER;
        retval = TRUE;
        // signal the main device (dying-devs-handler) thread
        SetEvent(g_hCleanEvt);
    }
    else {
        OpenAddRef(fsodev);
        retval = ServIOControl(fsodev->lpDev,fsodev->dwOpenData,dwIoControlCode,(PBYTE)lpInBuf,nInBufSize,(PBYTE)lpOutBuf,nOutBufSize,lpBytesReturned);
        OpenDelRef(fsodev);
    }
    LeaveCriticalSection(&g_ServCS);

    return retval;
}

// Expects a service handle to be passed (i.e. value returned by GetServiceHandle,
// RegisterDevice, or ActivateDevice).  This function will be called by programmer
// whereas SERV_DeviceIoControl must be called into via the file system handler.
// This function is implemented for backward compatibility reasons, as versions of CE
// prior to 4.0 do not have the filesys support for services.exe.
BOOL SERV_ServiceIoControl(HANDLE hService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    fsdev_t *lpdev = (fsdev_t *) hService;
    BOOL     fRet;
    BOOL     fFreeService;
    
    EnterCriticalSection(&g_ServCS);
    if (!IsValidService(lpdev)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        LeaveCriticalSection(&g_ServCS);
        return FALSE;
    }
    lpdev->dwRefCnt++;

    fRet = ServIOControl(lpdev,lpdev->dwData,dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);

    lpdev->dwRefCnt--;
    fFreeService = (lpdev->dwRefCnt == 0);
    LeaveCriticalSection(&g_ServCS);

    if (fFreeService)
        FreeService(lpdev);

    return fRet;
}

DWORD SERV_GetFileSize(fsopendev_t *fsodev, LPDWORD lpFileSizeHigh) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return 0xffffffff;
}

BOOL SERV_GetFileInformationByHandle(fsopendev_t *fsodev, LPBY_HANDLE_FILE_INFORMATION lpFileInfo) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL SERV_FlushFileBuffers(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL SERV_GetFileTime(fsopendev_t *fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL SERV_SetFileTime(fsopendev_t *fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL SERV_SetEndOfFile(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

void NotifyAllServices(DWORD dwIOCtl, BYTE *pInData, DWORD cbInData) {
    fsdev_t *   pTrav;
    int         icalls, i;
    HANDLE      h[20];
    HANDLE      *ph;

    EnterCriticalSection(&g_ServCS);

    // create a list of all services we need to send IOCTL to.  We can't just
    // run through list because on each call to the xxx_IOControl services.exe
    // releases its critical section, so list could be different between multiple calls.
    icalls = 0;

    for (pTrav = (fsdev_t *)g_ActiveServiceList.Flink;
         pTrav != (fsdev_t *)&g_ActiveServiceList;
         pTrav = (fsdev_t *)pTrav->list.Flink) {
         icalls++;
    }

    if (icalls < (sizeof(h) / sizeof(h[0])))
        ph = h;
    else {
        ph = (HANDLE *)LocalAlloc (LMEM_FIXED, icalls * sizeof(HANDLE));
        if (ph == NULL) { // Run out memory. Do as much as we can
            ph = h;
            icalls= sizeof(h) / sizeof(h[0]);
        }
    }

    i = 0;

    for (pTrav = (fsdev_t *)g_ActiveServiceList.Flink;
         pTrav != (fsdev_t *)&g_ActiveServiceList;
         pTrav = (fsdev_t *)pTrav->list.Flink) 
    {
         if (i == icalls)
             break;

         ph[i++] = (fsdev_t *)pTrav;
    }
    LeaveCriticalSection (&g_ServCS);

    for (i = 0 ; i < icalls ; ++i) {
        // Use SERV_ServiceIoControl rather than faster InternalServiceIOControl because we don't own
        // critical section at this point, SERV_ServiceIoControl will verify that the service hasn't been unloaded since call started.
        SERV_ServiceIoControl (ph[i], dwIOCtl, pInData, cbInData, NULL, 0, NULL, NULL);
    }

    if (ph != h)
        LocalFree (ph);
}

static void DevPSLNotify (DWORD flags, HPROCESS proc, HTHREAD thread) {
    fsopendev_t *fsodev;
    int         icalls, i;
    HANDLE      h[20];
    HANDLE      *ph;

    DEVICE_PSL_NOTIFY pslPacket;

    EnterCriticalSection(&g_ServCS);

    fsodev = g_lpOpenDevs;

    icalls = 0;

    while (fsodev) {
        if (fsodev->hProc == proc)
            ++icalls;
        fsodev = fsodev->nextptr;
    }

    if (icalls < sizeof(h) / sizeof(h[0]))
        ph = h;
    else {
        ph = (HANDLE *)LocalAlloc (LMEM_FIXED, icalls * sizeof(HANDLE));
        if (ph==NULL) { // Run out memory. Do as much as we can
            ph = h;
            icalls= sizeof(h) / sizeof(h[0]);
        };
    };

    i = 0;
    fsodev = g_lpOpenDevs;

    while (fsodev && i < icalls) {
        if (fsodev->hProc == proc)
            ph[i++] = fsodev->KHandle;
        fsodev = fsodev->nextptr;
    }

    LeaveCriticalSection (&g_ServCS);

    pslPacket.dwSize  = sizeof(pslPacket);
    pslPacket.dwFlags = flags;
    pslPacket.hProc   = proc;
    pslPacket.hThread = thread;

    for (i = 0 ; i < icalls ; ++i)
        DeviceIoControl (ph[i], IOCTL_PSL_NOTIFY, (LPVOID)&pslPacket, sizeof(pslPacket), NULL, 0, NULL, NULL);

    if (ph != h)
        LocalFree (ph);
}

void SERV_ProcNotify(DWORD flags, HPROCESS proc, HTHREAD thread) {
    switch (flags) {
#ifndef TARGET_NT
        case DLL_PROCESS_EXITING:
            DevPSLNotify (flags, proc, thread);
            break;
//      case DLL_SYSTEM_STARTED:
//          break;
#endif // TARGET_NT
    }

    return;
}

void SERV_CloseAllServiceHandles(HPROCESS proc) {
#if TARGET_NT
    ENTER_DEVICE_FUNCTION {
#endif
    fsopendev_t *fsodev = g_lpOpenDevs;
    while (fsodev) {
        if (fsodev->hProc == proc) {
#if TARGET_NT
            SERV_CloseFileHandle(fsodev);
#else
            CloseHandle(fsodev->KHandle);
#endif
            fsodev = g_lpOpenDevs;
        } else
            fsodev = fsodev->nextptr;
    }
#if TARGET_NT
    } EXIT_DEVICE_FUNCTION;
#endif
}

//
// ProcessAutoDeregisterDevs - called from the main thread as part of periodic clean up.
//
// Auto deregister devices are devices that need to be deregistered in a context that will
// avoid a deadlock between the device CS and the loader CS.
// A service gets marked as auto-deregister when DeviceIoControl is called with
// dwControlCode==IOCTL_DEVICE_AUTODEREGISTER.
// An example of an auto deregister device is the console device driver.
//
void ProcessAutoDeregisterDevs(void)
{
    BOOL bFreedOne;
    fsdev_t* lpdev;

    do {
        EnterCriticalSection(&g_ServCS);
        bFreedOne = FALSE;
        // walking the chain of *active* devs, looking for ALL that want to deregister
        // don't care about refcount. That's handled by deregister & the dying-devs list
        DEBUGMSG(ZONE_DYING, (L"SERVICES!ProcessAutoDeregisterDevs About to walk main list\r\n"));
        for (lpdev = (fsdev_t *)g_ActiveServiceList.Flink;
             lpdev != (fsdev_t *)&g_ActiveServiceList;
             lpdev = (fsdev_t *)(lpdev->list.Flink)) {
            if (lpdev->wFlags & DEVFLAGS_AUTO_DEREGISTER) {
                LeaveCriticalSection(&g_ServCS);
                // don't remove from list. DeregisterService will do that
                DEBUGMSG(ZONE_DYING, (L"SERVICES!ProcessAutoDeregisterDevs FOUND auto-deregister lpdev=%x\r\n", lpdev));
                SERV_DeregisterService((HANDLE)lpdev);
                bFreedOne = TRUE;
                break;
                // break out of inner loop, because after letting go of critsec &
                // freeing lpdev, our loop traversal is not valid anymore. So start over
            }
        }
        if (lpdev == (fsdev_t *)&g_ActiveServiceList) {
            LeaveCriticalSection(&g_ServCS);
            break;
        }
    } while (bFreedOne);
    // keep looping as long as we have something to do

}   // ProcessAutoDeregisterDevs


void DeInitServicesDataStructures(void) {
    DeleteCriticalSection(&g_ServCS);
    CloseHandle(g_hCleanEvt);
}

void InitServicesDataStructures(void) {
    InitializeListHead(&g_ActiveServiceList);
    InitializeListHead(&g_NewServiceList);
    InitializeListHead(&g_DyingServiceList);
    InitializeCriticalSection(&g_ServCS);
    g_hCleanEvt = CreateEvent(0,0,0,0);
}

// int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmShow)
int wmain (int argc, WCHAR **argv)
{
	if (IsAPIReady(SH_SERVICES)) {
		return HandleServicesCommandLine(argc,argv);
    }
    
    DEBUGMSG(ZONE_INIT, (L"SERVICES: Starting\r\n"));

    InitServicesDataStructures();

    // required for main instance of services.exe.
    g_hDevApiHandle = CreateAPISet("SRVS", NUM_FDEV_APIS, FDevApiMethods, FDevApiSigs);
    RegisterAPISet(g_hDevApiHandle, SH_SERVICES);
    g_hDevFileApiHandle = CreateAPISet("W32S", NUM_FAPIS, DevFileApiMethods, DevFileApiSigs);
    RegisterAPISet(g_hDevFileApiHandle, HT_FILE | REGISTER_APISET_TYPE);

    if (! ServerLoadInit())
    	return 1;

    SignalStarted(argc >=2 ? _wtol(argv[1]) : 0);

    while (1) {
        WaitForSingleObject(g_hCleanEvt, INFINITE);
        ProcessAutoDeregisterDevs();
    }
    return 1;
}

#if defined (SDK_BUILD)
#define SERVICES_CMDLINE_BUFSIZE     4096
#define SERVICES_MAX_CMD_ARGS        10

BOOL CrackCmdLine(LPWSTR lpCmdLine, int *argc, WCHAR *argv[SERVICES_MAX_CMD_ARGS], WCHAR *szCmdLineBuf) {
	WCHAR *szTrav = szCmdLineBuf;
    BOOL  fDone = FALSE;

    // Crack cmd line so that it's in argv/argc format.
    argv[0] = SERVICE_SERVICES_EXE_PROCNAME;
    if (wcslen(lpCmdLine) > SERVICES_CMDLINE_BUFSIZE) {
        RETAILMSG(1,(L"SERVICES!Services command line passed is too large, must be < %d characters)",SERVICES_CMDLINE_BUFSIZE));
        return FALSE;
    }
    wcscpy(szCmdLineBuf,lpCmdLine);

    while (!fDone && (*argc < SERVICES_MAX_CMD_ARGS)) {
        int iPos;

        while (szTrav[0] == 0 && szTrav[0] == L'\t' && szTrav[0] == L' ') 
            ++szTrav;

        if (szTrav[0] == 0)
            break;
        
        iPos  = wcscspn(szTrav,L" \t");
        fDone = (szTrav[iPos] == 0);

        szTrav[iPos] = 0;
        argv[(*argc)++] = szTrav;
        szTrav += (iPos + 1);
	}

	return TRUE;
}

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nShowCmd)
{
	WCHAR *argv[SERVICES_MAX_CMD_ARGS];
	int argc = 1;
	WCHAR szCmdLineBuf[SERVICES_CMDLINE_BUFSIZE];

    if (! IsAPIReady(SH_SERVICES)) {
        return wmain(0,0);
    }

    if (! CrackCmdLine(lpCmdLine,&argc,argv,szCmdLineBuf))
    	return 0;

	return HandleServicesCommandLine(argc,argv);
}
#endif  // SDK_BUILD
