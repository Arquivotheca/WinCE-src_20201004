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

#include <windows.h>
#include <devload.h>
#include <console.h>
#include <errorrep.h>

#include "devmgrp.h"
#include "celogdev.h"
#include "devmgrif.h"
#include "fiomgr.h"
#include "devzones.h"
#include "fiomgr.h"
#include "devinit.h"
#include "filter.h"

//
// This module contains PSL entry points for handling file I/O operations on 
// device drivers.
//

extern BOOL Reflector_Control(DWORD dwOpenData,DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,HANDLE hIoRef);

// This routine increments the refcount of an fsopendev_t as well as
// that of its associated device's fsdev_t.
BOOL  
OpenAddRef(fsopendev_t *fsodev)
{
    InterlockedIncrement(&fsodev->dwOpenRefCnt);
    if ( fsodev->lpDev  && AddDeviceAccessRef(fsodev->lpDev)!=NULL) {
        return TRUE;
    }
    else {
        InterlockedDecrement(&fsodev->dwOpenRefCnt);
        return FALSE;    
    }

}

// This routine decrements the refcount of an fsopendev_t as well as
// that of its associated device's fsdev_t.
void 
OpenDelRef(fsopendev_t *fsodev)
{
    InterlockedDecrement(&fsodev->dwOpenRefCnt);
    ASSERT (fsodev->lpDev ) ;
    DeDeviceAccessRef(fsodev->lpDev);
}
// 
// Get Access Exception Code Form Registry.
DWORD GetOldExceptionFromReg(HPROCESS hProc)
{
    DWORD dwReturn = 0; 
    TCHAR lpProcName[MAX_PATH];
    /* FIX OS function takes HPROCESS and HMODULE */
    if (GetModuleFileName( (HMODULE)hProc, lpProcName, _countof(lpProcName)) !=0) {
        HKEY rootKey;
        LPCTSTR lpName = lpProcName; 
        DWORD dwIndex;
        lpProcName[_countof(lpProcName) - 1]  = 0 ;
        // Get rid of path.
        for (dwIndex=0; dwIndex < _countof(lpProcName) && lpProcName[dwIndex]!=0; dwIndex ++) {
            if (lpProcName[dwIndex]==_T('\\')) {
                lpName = lpProcName + dwIndex + 1;
            }
        }
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEVLOAD_OLDACCESS_EXCEPTION_KEY, 0, 0, &rootKey)==ERROR_SUCCESS) {
            HKEY exceptionKey;
            if (RegOpenKeyEx(rootKey, lpName, 0, 0, &exceptionKey)==ERROR_SUCCESS) {
                DWORD dwRegValue;
                DWORD dwRegValueSize = sizeof(dwRegValue);
                DWORD dwRegType ;
                if (RegQueryValueEx( exceptionKey, DEVLOAD_EXCEPTION_FLAGS_VALNAME, NULL,&dwRegType , (LPBYTE)&dwRegValue, &dwRegValueSize) == ERROR_SUCCESS  &&
                        dwRegType == DEVLOAD_EXCEPTION_FLAGS_VALTYPE ) {
                    dwReturn = dwRegValue;
                }
                RegCloseKey( exceptionKey );
            }
            RegCloseKey( rootKey );
        }
    }
    return dwReturn;
}

//
// This routine opens a handle to a device based on its name.  The handle can be used
// to do I/O on the device using file operations.
//
HANDLE 
I_CreateDeviceHandle(LPCWSTR lpNew, NameSpaceType nameType, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc)
{
    HANDLE hDev = INVALID_HANDLE_VALUE;
    DWORD dwErrCode = ERROR_DEV_NOT_EXIST;
    fsopendev_t *lpopendev;
    fsdev_t *lpdev = NULL;
    fsdev_t *lpdevLocked;
    LPEXCEPTION_POINTERS pep;

    DEBUGCHK((dwAccess & DEVACCESS_BUSNAMESPACE) == 0); // this access bit should never be set externally

    
    // look up the device
    EnterCriticalSection(&g_devcs);
    lpdevLocked = FindDeviceByName(lpNew, nameType);
    if (lpdevLocked) {
        lpdev = AddDeviceAccessRef(lpdevLocked);
    };
    LeaveCriticalSection(&g_devcs);
    

    // validate the name type against the capabilities of the driver
    if(lpdev != NULL && nameType == NtBus) {
        if((lpdev->wFlags & DF_ENABLE_BUSNAMES) == 0) {
            // don't allow accesses to the $bus namespace -- the driver needs to advertise
            // the DMCLASS_PROTECTEDBUSNAMESPACE interface first.
            DEBUGMSG(ZONE_WARNING, (_T("DEVICE!I_CreateDeviceHandle: can't open '%s' because the driver doesn't advertise '%s'\r\n"),
                lpNew, DMCLASS_PROTECTEDBUSNAMESPACE));
            DeDeviceAccessRef(lpdev);
            lpdev = NULL;
        } else {
            // notify the driver that its handle is being opened using a protected namespace
            dwAccess |= DEVACCESS_BUSNAMESPACE;
        }
    }
    
    // did we find the device?
    if(lpdev != NULL) {
        lpopendev = CreateAsyncFileObject();
        if (lpopendev) {
            fsdev_t *lpInsertDevice = NULL;
            
            SetLastError(ERROR_SUCCESS);
            __try {
                lpopendev->dwOpenData = DriverOpen(lpdev,lpdev->dwData,dwAccess,dwShareMode);
                lpopendev->dwAccess = dwAccess ;
                lpopendev->dwAccessException = GetOldExceptionFromReg(hProc);
            }
            __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_INVALID_PARAMETER);
                lpopendev->dwOpenData = 0 ;
            }
            if(!lpopendev->dwOpenData) {
                dwErrCode = GetLastError();
                if(dwErrCode == ERROR_SUCCESS) {
                    // driver didn't fill it in, use a default
                    dwErrCode = ERROR_OPEN_FAILED;
                }
            }
            if (!lpopendev->dwOpenData) {
                DeleteAsncyFileObject(lpopendev);
                // Don't set an error code, the driver should have done that.
            }
            else {
                lpInsertDevice = FindDeviceByHandle(lpdev);
                if (lpInsertDevice) {
                    dwErrCode = ERROR_SUCCESS;
                    lpopendev->hProc = hProc;
                    hDev = CreateAPIHandle(g_hDevFileApiHandle, lpopendev);
                    if (hDev) {
                        // OK, we managed to create the handle
                        lpopendev->lpDev = lpInsertDevice ;
                        dwErrCode = 0;
                        lpopendev->KHandle = hDev;
                        // Insert into the list. No de reference the lpInsertDevice because it is inserted.
                        EnterCriticalSection(&g_devcs);
                        lpopendev->nextptr = g_lpOpenDevs;
                        g_lpOpenDevs = lpopendev;
                        LeaveCriticalSection(&g_devcs);
                    }
                    else {
                        DeDeviceRef(lpInsertDevice);
                        // Close the handle on Device Driver
                        __try {
                            if(lpdev->fnPreClose != NULL) {
                                DriverPreClose(lpdev,lpopendev->dwOpenData);
                            }
                            if (lpdev->fnClose !=NULL) {
                                DriverClose(lpdev,lpopendev->dwOpenData);
                            }
                        }
                        __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                            DEBUGMSG(ZONE_WARNING, (_T("I_CreateDeviceHandle: exception calling pre-close or close entry point\r\n")));
                        }
                        hDev = INVALID_HANDLE_VALUE;
                        dwErrCode = ERROR_OUTOFMEMORY;
                        DeleteAsncyFileObject(lpopendev);
                    }
                }
            }
        }
        DeDeviceAccessRef(lpdev);
    }
    if (lpdevLocked) {
        DeDeviceRef(lpdevLocked);
    }
    return hDev;
}

// This routine decrements the use count of an fsopendev_t structure 
// allocated by CreateDeviceHandle().  First it calls the associated
// driver's Close entry point.  If the refcount is zero, the
// structure is freed immediately.  If not, a thread is executing code
// in the driver using this handle; the fsopendev_t is put on the 
// "dying opens" queue to be freed later.
BOOL 
DM_DevPreCloseFileHandle(fsopendev_t *fsodev)
{
    BOOL retval;
    DWORD dwStatus = ERROR_SUCCESS;
    fsopendev_t *fsTrav;
    fsdev_t *lpdev;
    LPEXCEPTION_POINTERS pep;
    
    EnterCriticalSection(&g_devcs);

    // look for the device file handle in the list        
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
            DEBUGMSG(ZONE_WARNING, (TEXT("DM_DevCloseFileHandle: fsodev (0x%X) not in DEVICE.EXE list\r\n"), fsodev));
            dwStatus = ERROR_INVALID_HANDLE;
        }
    }

    // did we find the handle?
    if(dwStatus == ERROR_SUCCESS) {
        if ((lpdev = fsodev->lpDev)!=NULL && (fsodev->lpDev->wFlags & DF_DYING)==0 && OpenAddRef(fsodev)) {  
            // OpenAddRef(fsodev) has to be last because of  OpenDelRef;
            if(lpdev->fnPreClose != NULL) {
                // increment reference counts to make sure that fnDeinit is not called before
                // fnPreClose -- this guarantees that the context handle passed to fnPreClose
                // is valid.
                LeaveCriticalSection(&g_devcs);
                __try {
                    DriverPreClose(lpdev,fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling pre-close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);

            } else {
                LeaveCriticalSection(&g_devcs);
                __try {
                    DriverClose(lpdev,fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);
            }
            OpenDelRef(fsodev);
        }
        
    } 
    
    LeaveCriticalSection(&g_devcs);

    if(dwStatus == ERROR_SUCCESS) {
        retval = TRUE;
    } else {
        SetLastError(dwStatus);
        retval = FALSE;
    }

    return retval;
}
BOOL 
DM_DevCloseFileHandle(fsopendev_t *fsodev)
{
    fsdev_t *lpdev;
    LPEXCEPTION_POINTERS pep;
    
    ASSERT(fsodev->dwOpenRefCnt==0);
    lpdev = fsodev->lpDev;
    if(lpdev != NULL && (lpdev->wFlags & DF_DYING)==0 && lpdev->fnPreClose != NULL && OpenAddRef(fsodev)) {
        // OpenAddRef(fsodev) has to be last because of  OpenDelRef;
        __try {
            DriverClose(lpdev,fsodev->dwOpenData);
        }
        __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling final close entry point\r\n")));
        }
        OpenDelRef(fsodev);
    }
    if (lpdev) {
        DeDeviceRef(lpdev);
        fsodev->lpDev = NULL;
    }
    DeleteAsncyFileObject(fsodev);
    return TRUE;
}

// This routine implements ReadFile() for device drivers.  It calls
// the device's Read() entry point to obtain the data and manages
// the driver's refcount appropriately.
BOOL 
DM_DevReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    LPEXCEPTION_POINTERS pep;
    
    if (!fsodev->lpDev || !OpenAddRef(fsodev)) { 
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {
        __try {
            if ((fsodev->dwAccess & GENERIC_READ) == 0 && (fsodev->dwAccessException & GENERIC_READ)==0 ) {
                SetLastError(ERROR_INVALID_ACCESS);
            }
            else
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                *lpNumBytesRead = 0xffffffff;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                retval = DevReadFile(fsodev, buffer, nBytesToRead, lpNumBytesRead, lpOverlapped);
            }
        } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        OpenDelRef(fsodev);
    };
    return retval;
}

// This routine implements WriteFile() for device drivers.  It calls
// the device's Write() entry point to send the data and manages
// the driver's refcount appropriately.
BOOL 
DM_DevWriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    LPEXCEPTION_POINTERS pep;

    if (!fsodev->lpDev || !OpenAddRef(fsodev)){ 
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {
        __try {
            if ((fsodev->dwAccess & GENERIC_WRITE) == 0 && (fsodev->dwAccessException & GENERIC_WRITE) == 0 ) {
                SetLastError(ERROR_INVALID_ACCESS);
            }
            else
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                *lpNumBytesWritten = 0xffffffff;
                retval = FALSE;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                retval = DevWriteFile(fsodev, buffer, nBytesToWrite, lpNumBytesWritten, lpOverlapped);
            }
        } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        };
        OpenDelRef(fsodev);
    }
    return retval;
}

// This routine implements SetFilePointer() for device drivers.  It calls
// the device's Seek() entry point and manages the driver's refcount 
// appropriately.
DWORD DM_DevSetFilePointer(fsopendev_t *fsodev, LONG lDistanceToMove, LONG const*const lpDistanceToMoveHigh,
    DWORD dwMoveMethod) {
    DWORD retval = 0xffffffff;

    UNREFERENCED_PARAMETER(lpDistanceToMoveHigh);

    if (!fsodev->lpDev || !OpenAddRef(fsodev)) { 
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {
        if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
            retval = 0xffffffff;
            SetLastError(ERROR_DEVICE_REMOVED);
        }
        else {
            __try {
                retval = DriverSeek(fsodev->lpDev,fsodev->dwOpenData,lDistanceToMove,dwMoveMethod);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_INVALID_PARAMETER);
            }
        }
        OpenDelRef(fsodev);
    }
    return retval;
}

// This routine implements DeviceIoControl() for device drivers.  It calls
// the device's IOControl() entry point and manages the driver's refcount 
// appropriately.  Note that some IOCTLs (such as IOCTL_DEVICE_AUTODEREGISTER)
// are handled by the Device Manager and are not passed on to the driver.
BOOL 
DM_DevDeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    BOOL retval = FALSE;
    DWORD dwOldLastError = GetLastError();
    LPVOID lpMappedInBuf = lpInBuf;  
    HANDLE hCallerProcess = (HANDLE)GetCallerVMProcessId();
#ifdef DEBUG
    DWORD dwOldProtect;
#endif
    LPEXCEPTION_POINTERS pep;
    SetLastError (ERROR_GEN_FAILURE);

    if (!fsodev || !fsodev->lpDev || !OpenAddRef(fsodev)) {
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {
        ASSERT(dwIoControlCode != IOCTL_DEVICE_AUTODEREGISTER);

        if (dwIoControlCode == IOCTL_PSL_NOTIFY && 
                ((DWORD)GetDirectCallerProcessId() != GetCurrentProcessId() ) ) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
        else {
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                retval = FALSE;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                retval = TRUE;
                
                if (IsInSharedHeapRange(lpMappedInBuf, nInBufSize) 
                    && (fsodev->lpDev->fnControl != Reflector_Control)
                    && (GetDirectCallerProcessId() != GetCurrentProcessId())
                    && (nInBufSize)) {
                    // Call is from application to a kernel mode driver and [IN] argument is in shared heap range.
                    // Make a virtual alloc to the caller process to a r/o page to prevent overwrite of shared heap 
                    // area by kernel mode component.
                    lpMappedInBuf = VirtualAllocEx(hCallerProcess, NULL, nInBufSize, MEM_COMMIT, PAGE_READWRITE);
                    if (lpMappedInBuf) {

                        // Copy the original lpInBuf to lpMappedInBuf
                        if (CeSafeCopyMemory(lpMappedInBuf, lpInBuf, nInBufSize)) {
#ifdef DEBUG
                            // in debug builds to fault on write, set the protection to r/o on lpMappedInBuf
                            VERIFY(VirtualProtectEx(hCallerProcess, lpMappedInBuf, nInBufSize, PAGE_READONLY, &dwOldProtect));
#endif
                        } else {
                            VERIFY(VirtualFreeEx(hCallerProcess, lpMappedInBuf, 0, MEM_RELEASE));                            
                            retval = FALSE;
                        }

                    } else {
                        retval = FALSE;
                    }
                    
                }

                if (retval) {
                    __try {
                        retval = DevDeviceIoControl(fsodev,  dwIoControlCode, lpMappedInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
                    } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                        SetLastError(ERROR_INVALID_PARAMETER);
                    }
                    if (lpMappedInBuf != lpInBuf) {
                        // [IN] argument is virtual allocated in caller process space; free it.
                        VERIFY(VirtualFreeEx(hCallerProcess, lpMappedInBuf, 0, MEM_RELEASE));
                    }
                }
            }
        }
        OpenDelRef(fsodev);
    }

    if ((retval) && (GetLastError()==ERROR_GEN_FAILURE)) {
        SetLastError(dwOldLastError);
    }

    return retval;
}

// unimplemented handle-based PSL function
DWORD DM_DevGetFileSize(fsopendev_t *fsodev, DWORD const*const lpFileSizeHigh) {
    UNREFERENCED_PARAMETER(fsodev);
    UNREFERENCED_PARAMETER(lpFileSizeHigh);
    SetLastError(ERROR_INVALID_FUNCTION);
    return 0xffffffff;
}

// unimplemented handle-based PSL function
BOOL DM_DevGetDeviceInformationByFileHandle(fsopendev_t *fsodev, PDEVMGR_DEVICE_INFORMATION pdi) 
{
    fsopendev_t *fsTrav;
    DWORD dwStatus;
    BOOL fOk;
    
    if(fsodev == NULL || pdi == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection(&g_devcs);

    // look for the device file handle in the list
    dwStatus = ERROR_INVALID_HANDLE;
    for(fsTrav = g_lpOpenDevs; fsTrav != NULL; fsTrav = fsTrav->nextptr) {
        if(fsTrav == fsodev) {
            if(fsodev->lpDev != NULL) {
                dwStatus = ERROR_SUCCESS;
            }
            break;
        }
    }

    // load the information structure
    if(dwStatus == ERROR_SUCCESS) {
        dwStatus = I_GetDeviceInformation(fsodev->lpDev, pdi);
    }

    LeaveCriticalSection(&g_devcs);

    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }
    
    return fOk;
}

BOOL EX_DM_DevGetDeviceInformationByFileHandle(fsopendev_t *fsodev, PDEVMGR_DEVICE_INFORMATION pdi) 
{
    DEVMGR_DEVICE_INFORMATION di;
    BOOL fOk = FALSE;
    __try {
        if (pdi!=NULL && pdi->dwSize >= sizeof(di)) {
            di.dwSize = sizeof(di);
            fOk = DM_DevGetDeviceInformationByFileHandle(fsodev, &di ) ;
            if (fOk) {
                memcpy (pdi,&di,sizeof(di));
            }
        }
        else {
            SetLastError(ERROR_INVALID_PARAMETER);
            fOk = FALSE;
        }
            
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR,(TEXT("DEVICE! Exception calling EX_DM_DevGetDeviceInformationByFileHandle for\r\n")));
        DEBUGCHK(FALSE);
        SetLastError(ERROR_INVALID_PARAMETER);
        fOk = FALSE;
    }
    return fOk;

}
// unimplemented handle-based PSL function
BOOL DM_DevFlushFileBuffers(fsopendev_t *fsodev) {
    UNREFERENCED_PARAMETER(fsodev);
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevGetFileTime(fsopendev_t *fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite) {
    UNREFERENCED_PARAMETER(fsodev);
    UNREFERENCED_PARAMETER(lpCreation);
    UNREFERENCED_PARAMETER(lpLastAccess);
    UNREFERENCED_PARAMETER(lpLastWrite);

    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevSetFileTime(fsopendev_t *fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite) {
    UNREFERENCED_PARAMETER(fsodev);
    UNREFERENCED_PARAMETER(lpCreation);
    UNREFERENCED_PARAMETER(lpLastAccess);
    UNREFERENCED_PARAMETER(lpLastWrite);

    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevSetEndOfFile(fsopendev_t *fsodev) {
    UNREFERENCED_PARAMETER(fsodev);
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}
BOOL DM_DevCancelIo(fsopendev_t *fsodev)
{
    BOOL retval = FALSE;
    LPEXCEPTION_POINTERS pep;
    
    if (!fsodev->lpDev || !OpenAddRef(fsodev)) {
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {        
        __try {
            if (!(fsodev->lpDev->wFlags & DF_DYING) && fsodev->lpDev->fnCancelIo) {
                retval= DevCancelIo(fsodev);
            }
        } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        OpenDelRef(fsodev);
    };
    return retval;
}
BOOL DM_DevCancelIoEx(fsopendev_t *fsodev, LPOVERLAPPED lpOverlapped )
{
    BOOL retval = FALSE;
    LPEXCEPTION_POINTERS pep;
    
    if (!fsodev->lpDev || !OpenAddRef(fsodev)) {
        SetLastError(ERROR_INVALID_HANDLE_STATE);
    }
    else {
        __try {
            if (!(fsodev->lpDev->wFlags & DF_DYING) && fsodev->lpDev->fnCancelIo) {
                retval= DevCancelIoEx(fsodev, lpOverlapped);
            }
        } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
        }
        OpenDelRef(fsodev);
    };
    return retval;
}


