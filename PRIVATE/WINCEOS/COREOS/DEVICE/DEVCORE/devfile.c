//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <devload.h>
#include <console.h>
#include <errorrep.h>

#include "devmgrp.h"
#include "celogdev.h"
#include "devmgrif.h"
#include "devzones.h"

//
// This module contains PSL entry points for handling file I/O operations on 
// device drivers.
//

// This routine increments the refcount of an fsopendev_t as well as
// that of its associated device's fsdev_t.
void 
OpenAddRef(fsopendev_t *fsodev)
{
    InterlockedIncrement(&fsodev->dwOpenRefCnt);
    InterlockedIncrement(fsodev->lpdwDevRefCnt);
}

// This routine decrements the refcount of an fsopendev_t as well as
// that of its associated device's fsdev_t.
void 
OpenDelRef(fsopendev_t *fsodev)
{
    InterlockedDecrement(&fsodev->dwOpenRefCnt);
    InterlockedDecrement(fsodev->lpdwDevRefCnt);
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
    fsdev_t *lpdev;
    LPEXCEPTION_POINTERS pep;

    DEBUGCHK((dwAccess & DEVACCESS_BUSNAMESPACE) == 0); // this access bit should never be set externally

    EnterCriticalSection(&g_devcs);
    __try {
        // look up the device
        lpdev = FindDeviceByName(lpNew, nameType);

        // validate the name type against the capabilities of the driver
        if(lpdev != NULL && nameType == NtBus) {
            if((lpdev->wFlags & DF_ENABLE_BUSNAMES) == 0) {
                // don't allow accesses to the $bus namespace -- the driver needs to advertise
                // the DMCLASS_PROTECTEDBUSNAMESPACE interface first.
                DEBUGMSG(ZONE_WARNING, (_T("DEVICE!I_CreateDeviceHandle: can't open '%s' because the driver doesn't advertise '%s'\r\n"),
                    lpNew, DMCLASS_PROTECTEDBUSNAMESPACE));
                lpdev = NULL;
            } else {
                // notify the driver that its handle is being opened using a protected namespace
                dwAccess |= DEVACCESS_BUSNAMESPACE;
            }
        }

        // did we find the device?
        if(lpdev != NULL) {
            if (!(lpopendev = LocalAlloc(LPTR,sizeof(fsopendev_t)))) {
                dwErrCode = ERROR_OUTOFMEMORY;
                goto errret;
            }
            lpopendev->lpDev = lpdev;
            lpopendev->lpdwDevRefCnt = &lpdev->dwRefCnt;
            InterlockedIncrement(&lpdev->dwRefCnt);
            LeaveCriticalSection(&g_devcs);
            SetLastError(ERROR_SUCCESS);
            __try {
                lpopendev->dwOpenData = lpdev->fnOpen(lpdev->dwData,dwAccess,dwShareMode);
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
            EnterCriticalSection(&g_devcs);
            InterlockedDecrement(&lpdev->dwRefCnt);
            if ((!FindDeviceByHandle(lpdev)) || (!lpopendev->dwOpenData)) {
                LocalFree(lpopendev);
                // Don't set an error code, the driver should have done that.
                goto errret;
            }
            lpopendev->hProc = hProc;
            if (!(hDev = CreateAPIHandle(g_hDevFileApiHandle, lpopendev))) {
                hDev = INVALID_HANDLE_VALUE;
                dwErrCode = ERROR_OUTOFMEMORY;
                LocalFree(lpopendev);
                goto errret;
            }
            // OK, we managed to create the handle
            dwErrCode = 0;
            lpopendev->KHandle = hDev;
            lpopendev->nextptr = g_lpOpenDevs;
            g_lpOpenDevs = lpopendev;
        }
errret:
        if( dwErrCode ) {
            SetLastError( dwErrCode );
            // This debugmsg occurs too frequently on a system without a sound device (eg, CEPC)
            DEBUGMSG(0, (TEXT("ERROR (devfile.c:DM_CreateDeviceHandle): %s, %d\r\n"), lpNew, dwErrCode));
        }
        
    } 
    __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    LeaveCriticalSection(&g_devcs);
    
    return hDev;
}

// When an application calls CreateFile() on a device driver using the legacy 
// name format (e.g., "COM1:"), filesys calls this API to set up an open file 
// object.  This routine looks up the device, calls its open entry point, creates
// an fsopendev_t structure to maintains the open handle's state, and wraps 
// the structure using a call to CreateAPIHandle().  This is the value it
// passes back to filesys.  On failure, the value is INVALID_HANDLE_VALUE and
// the system "last error" value is set with more information.  This routine
// assumes that len == 5 and lpnew[4] == ':'.
HANDLE 
DM_CreateDeviceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc)
{
    HANDLE hDev;
    DWORD dwErrCode = ERROR_SUCCESS;
    WCHAR szDeviceName[MAXDEVICENAME + 1];

    if(PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        // untrusted caller can't set the auto-deregistered flag
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    
    if(lpNew == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // copy the device name
    szDeviceName[ARRAYSIZE(szDeviceName) - 1] = 0; // Intialize last as terminate.
    __try {
        wcsncpy(szDeviceName, lpNew, ARRAYSIZE(szDeviceName));
        if(szDeviceName[ARRAYSIZE(szDeviceName) - 1] != 0) { // If there is no terminate, device name is too long. return error.
            dwErrCode = ERROR_INVALID_PARAMETER;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwErrCode = ERROR_INVALID_PARAMETER;
    }
    if(dwErrCode != ERROR_SUCCESS) {
        SetLastError(dwErrCode);
        return INVALID_HANDLE_VALUE;
    }

    hDev = I_CreateDeviceHandle(szDeviceName, NtLegacy, dwAccess, dwShareMode, hProc);
   
    return hDev;
}

// This routine decrements the use count of an fsopendev_t structure 
// allocated by CreateDeviceHandle().  First it calls the associated
// driver's Close entry point.  If the refcount is zero, the
// structure is freed immediately.  If not, a thread is executing code
// in the driver using this handle; the fsopendev_t is put on the 
// "dying opens" queue to be freed later.
BOOL 
DM_DevCloseFileHandle(fsopendev_t *fsodev)
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
        if (lpdev = fsodev->lpDev) {
            if(lpdev->fnPreClose != NULL) {
                // increment reference counts to make sure that fnDeinit is not called before
                // fnPreClose -- this guarantees that the context handle passed to fnPreClose
                // is valid.
                OpenAddRef(fsodev);

                LeaveCriticalSection(&g_devcs);
                __try {
                    lpdev->fnPreClose(fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling pre-close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);

                // decrement reference counts if we incremented it before preClose
                if(lpdev->fnPreClose != NULL) {
                    OpenDelRef(fsodev);
                }
            } else {
                LeaveCriticalSection(&g_devcs);
                __try {
                    lpdev->fnClose(fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);
            }
        }
        
        if (fsodev->dwOpenRefCnt) {
            fsodev->nextptr = g_lpDyingOpens;
            g_lpDyingOpens = fsodev;
            SetEvent(g_hCleanEvt);
        } else {
            lpdev = fsodev->lpDev;
            if(lpdev != NULL && lpdev->fnPreClose != NULL) {
                OpenAddRef(fsodev);
                LeaveCriticalSection(&g_devcs);
                __try {
                    lpdev->fnClose(fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("DM_DevCloseFileHandle: exception calling final close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);
                OpenDelRef(fsodev);
            }
            LocalFree(fsodev);
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

// This routine implements ReadFile() for device drivers.  It calls
// the device's Read() entry point to obtain the data and manages
// the driver's refcount appropriately.
BOOL 
DM_DevReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    DWORD dodec = 0;
    LPEXCEPTION_POINTERS pep;
    
    buffer = MapCallerPtr(buffer,nBytesToRead);
    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                *lpNumBytesRead = 0xffffffff;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                *lpNumBytesRead = fsodev->lpDev->fnRead(fsodev->dwOpenData,buffer,nBytesToRead);
            }
            OpenDelRef(fsodev);
            dodec = 0;
            if (*lpNumBytesRead == 0xffffffff) {
                retval = FALSE;
                *lpNumBytesRead = 0;
            } else
                retval = TRUE;
        }
    } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
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
    DWORD dodec = 0;
    LPEXCEPTION_POINTERS pep;

    buffer =(LPCVOID)MapCallerPtr((LPVOID)buffer,nBytesToWrite);

    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                *lpNumBytesWritten = 0xffffffff;
                retval = FALSE;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                *lpNumBytesWritten = fsodev->lpDev->fnWrite(fsodev->dwOpenData,buffer,nBytesToWrite);
            }
            OpenDelRef(fsodev);
            dodec = 0;
            if (*lpNumBytesWritten == 0xffffffff)
                *lpNumBytesWritten = 0;
            else
                retval = TRUE;
        }
    } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

// This routine implements SetFilePointer() for device drivers.  It calls
// the device's Seek() entry point and manages the driver's refcount 
// appropriately.
DWORD DM_DevSetFilePointer(fsopendev_t *fsodev, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
    DWORD dwMoveMethod) {
    DWORD retval = 0xffffffff;
    DWORD dodec = 0;

    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                retval = 0xffffffff;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                retval = fsodev->lpDev->fnSeek(fsodev->dwOpenData,lDistanceToMove,dwMoveMethod);
            }
            OpenDelRef(fsodev);
            dodec = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
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
    DWORD dodec = 0;
    LPEXCEPTION_POINTERS pep;

    lpInBuf = MapCallerPtr(lpInBuf,nInBufSize);
    lpOutBuf = MapCallerPtr(lpOutBuf,nOutBufSize);
    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            if(dwIoControlCode == IOCTL_DEVICE_AUTODEREGISTER) {
                fsopendev_t *fsTrav;

                EnterCriticalSection(&g_devcs);

                // Only allow the auto-unload if we were loaded with RegisterDevice() and if
                // the caller is trusted.
                for(fsTrav = g_lpOpenDevs; fsTrav != NULL; fsTrav = fsTrav->nextptr) {
                    if(fsTrav == fsodev) {
                        break;
                    }
                }

                // do sanity checks
                if(fsTrav == NULL) {
                    // can't find the handle
                    SetLastError(ERROR_INVALID_HANDLE);
                } else if(PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
                    // untrusted caller can't set the auto-deregistered flag
                    SetLastError(ERROR_ACCESS_DENIED);
                } else if((fsodev->lpDev->wFlags & DF_REGISTERED) == 0) {
                    // only devices loaded by RegisterDevice() can be auto-deregistered
                    SetLastError(ERROR_INVALID_PARAMETER);
                } else {
                    // NOTE: This IOCTL is here to avoid a deadlock between (a) a DllMain calling
                    // DeregisterDevice (and hence trying to acquire the device CS *after* the 
                    // loader CS and (b) Various other paths in this file where we acquire the 
                    // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc). 
                    // See WinCE#4165. Hence this code path must NOT acquire the device CS!!
                    DEBUGMSG(ZONE_DYING, (L"DM_DevDeviceIoControl:IOCTL_DEVICE_AUTODEREGISTER. lpdev=%x\r\n", fsodev->lpDev));
                    fsodev->lpDev->wFlags |= DF_AUTO_DEREGISTER;
                    retval = TRUE;
                    // signal the main device (dying-devs-handler) thread
                    SetEvent(g_hCleanEvt);
                }

                LeaveCriticalSection(&g_devcs);
            }
            else {
                OpenAddRef(fsodev);
                dodec = 1;
                if (fsodev->lpDev->wFlags & DF_DYING) { //Dying Device.
                    retval = FALSE;
                    SetLastError(ERROR_DEVICE_REMOVED);
                }
                else {
                    retval = fsodev->lpDev->fnControl(fsodev->dwOpenData,dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned);
                }
                OpenDelRef(fsodev);
                dodec = 0;
            }
        }
    } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

// unimplemented handle-based PSL function
DWORD DM_DevGetFileSize(fsopendev_t *fsodev, LPDWORD lpFileSizeHigh) {
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

// unimplemented handle-based PSL function
BOOL DM_DevFlushFileBuffers(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevGetFileTime(fsopendev_t *fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevSetFileTime(fsopendev_t *fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

// unimplemented handle-based PSL function
BOOL DM_DevSetEndOfFile(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}


