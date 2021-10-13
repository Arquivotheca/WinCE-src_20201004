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

#include <windows.h>
#include <devload.h>
#include <extfile.h>

#include "devmgrp.h"
#include "devmgrif.h"
#include "devzones.h"

#ifdef DEBUG
#define SETFNAME(x)     LPCWSTR pszFname = L##x
#else   // DEBUG
#define SETFNAME(x)
#endif  // DEBUG

#if !defined(INVALID_AFS)
#if OID_FIRST_AFS != 0
#define INVALID_AFS     0
#else
#define INVALID_AFS     -1
#endif
#endif

// file system types
#define FST_LEGACY              1       // e.g., "com1:"
#define FST_DEVICE              2       // rooted in $device
#define FST_BUS                 3       // rooted in $bus

// this structure manages state for file searches
typedef struct _SearchState_tag {
    DeviceSearchType searchType;        // how to look for a match
    fsdev_t *lpdev;                     // last device examined
    struct _SearchState_tag *pNext;     // linked list of search state structures
    union {
        WCHAR szName[MAX_PATH];         // name or bus name
        GUID guidClass;                 // advertised interfaces
        HANDLE hParent;                 // parent bus driver (or NULL)
    } u;
} DEVICESEARCHSTATE, *PDEVICESEARCHSTATE;

// filesystem APIs
BOOL WINAPI DEVFS_CloseVolume(DWORD dwContext);
HANDLE WINAPI DEVFS_CreateFileW(DWORD dwContext, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode, PSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD SecurityDescriptorSize);
HANDLE WINAPI DEVFS_FindFirstFileW(DWORD dwContext, HANDLE hProc, PCWSTR pwsFileSpec, PDEVMGR_DEVICE_INFORMATION pdi, DWORD dwDevInfoSize);

// file searching APIs
BOOL WINAPI DEVFS_FindClose(PDEVICESEARCHSTATE pss);
BOOL WINAPI DEVFS_FindNextFileW(PDEVICESEARCHSTATE pss, PDEVMGR_DEVICE_INFORMATION pdi);
BOOL WINAPI DEVFS_ExFindNextFileW(PDEVICESEARCHSTATE pss, PDEVMGR_DEVICE_INFORMATION pdi);

// stub functions
DWORD DEVFS_StubFunction(void);


// Device Manager filesystem APIs
static CONST PFNVOID gpfnDevFSAPIs[] = {
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)NULL,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_CreateFileW,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_StubFunction,
        (PFNVOID)DEVFS_FindFirstFileW,
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
};

static CONST ULONGLONG gDevFSSigs[] = {
        FNSIG1(DW),                                     // CloseVolume
        FNSIG0(),                                       //
        FNSIG0(),                                       // CreateDirectoryW
        FNSIG0(),                                       // RemoveDirectoryW
        FNSIG0(),                                       // GetFileAttributesW
        FNSIG0(),                                       // SetFileAttributesW
        FNSIG11(DW, DW,  I_WSTR, DW, DW, I_PDW, DW, DW, DW, I_PTR, DW),  // CreateFileW
        FNSIG0(),                                       // DeleteFileW
        FNSIG0(),                                       // MoveFileW
        FNSIG5(DW, DW,  I_WSTR, IO_PTR, DW),            // FindFirstFileW
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
        FNSIG0(),                                       // FsIoControlW 
        FNSIG0(),                                       // SetFileSecurityW
        FNSIG0(),                                       // GetFileSecurityW
};

static CONST PFNVOID gpfnDevFindAPIs[] = {
        (PFNVOID)DEVFS_FindClose,
        (PFNVOID)NULL,
        (PFNVOID)DEVFS_FindNextFileW,
};
static CONST PFNVOID gpfnExDevFindAPIs[] = {
        (PFNVOID)DEVFS_FindClose,
        (PFNVOID)NULL,
        (PFNVOID)DEVFS_FindNextFileW,
};

static CONST ULONGLONG gDevFindSigs[] = {
        FNSIG1(DW),                                     // FindClose
        FNSIG0(),                                       //
        FNSIG2(DW, IO_PDW)                              // FindNextFileW
};

// global variables
HANDLE ghDevFSAPI;
HANDLE ghDevFileAPI;
HANDLE ghDevFindAPI;
PDEVICESEARCHSTATE gpDeviceSearchList;
extern const PFNVOID DevFileApiMethods[12];
extern const PFNVOID ExDevFileApiMethods[12];
extern const ULONGLONG DevFileApiSigs[12];

// This routine is called when a device is deactivated.  It is responsible
// for removing the device from any searches currently underway.  The caller
// of this routine must hold the device manager critical section.
void 
RemoveDeviceFromSearchList(fsdev_t *lpdev)
{
    PDEVICESEARCHSTATE pssTrav;

    for(pssTrav = gpDeviceSearchList; pssTrav != NULL; pssTrav = pssTrav->pNext) {
        if(pssTrav->lpdev == lpdev) {
            pssTrav->lpdev = (fsdev_t *) lpdev->list.Flink;
        }
    }
}

// This routine searches for a device matching the search criteria in the
// pss parameter.  If it finds one, it returns information about it in pdi.
// The caller to this routine must hold the device manager critical section.
static DWORD
I_FindNextDevice(PDEVICESEARCHSTATE pss, PDEVMGR_DEVICE_INFORMATION pdi)
{
    DWORD dwStatus = ERROR_SUCCESS;
    fsdev_t *lpdev;

    DEBUGCHK(pss != NULL);
    DEBUGCHK(pdi != NULL);

    // figure out where to start searching    
    DEBUGCHK(pss->searchType == DeviceSearchByBusName || pss->searchType == DeviceSearchByDeviceName 
        || pss->searchType == DeviceSearchByGuid || pss->searchType == DeviceSearchByLegacyName 
        || pss->searchType == DeviceSearchByParent);
    if(pss->lpdev == NULL) {
        // start search with the first device
        lpdev = (fsdev_t *) g_DevChain.Flink;
    } else {
        // pick up where we left off
        lpdev = pss->lpdev;
    }
    
    // look for a match
    while(lpdev != (fsdev_t *) &g_DevChain) {
        // is this device a match?
        if(pss->searchType == DeviceSearchByDeviceName) {
            if(lpdev->pszDeviceName != NULL) {
                if(MatchesWildcardMask(wcslen(pss->u.szName), pss->u.szName, wcslen(lpdev->pszDeviceName), lpdev->pszDeviceName)) {
                    break;
                }
            } else if(pss->u.szName[0] == 0) {
                break;
            }
        }
        else if(pss->searchType == DeviceSearchByBusName) {
            if(lpdev->pszBusName != NULL) {
                if(MatchesWildcardMask(wcslen(pss->u.szName), pss->u.szName, wcslen(lpdev->pszBusName), lpdev->pszBusName)) {
                    break;
                }
            } else if(pss->u.szName[0] == 0) {
                break;
            }
        }
        else if(pss->searchType == DeviceSearchByLegacyName) {
            if(lpdev->pszLegacyName != NULL) {
                if(MatchesWildcardMask(wcslen(pss->u.szName), pss->u.szName, wcslen(lpdev->pszLegacyName), lpdev->pszLegacyName)) {
                    break;
                }
            } else if(pss->u.szName[0] == 0) {
                break;
            }
        }
        else if(pss->searchType == DeviceSearchByParent) {
            if(lpdev->hParent == pss->u.hParent) {
                break;
            }
        }
        else if(pss->searchType == DeviceSearchByGuid) {
            if(FindDeviceInterface(lpdev, &pss->u.guidClass) != NULL) {
                break;
            }
        }
        
        // on to the next device structure
        lpdev = (fsdev_t *) lpdev->list.Flink;
    }

    if(lpdev == (fsdev_t *) &g_DevChain) {
        dwStatus = ERROR_NO_MORE_FILES;
    } else {
        __try {
            // pass back device information to the caller
            dwStatus = I_GetDeviceInformation(lpdev, pdi);
            if(dwStatus == ERROR_SUCCESS) {
                pss->lpdev = (fsdev_t *) lpdev->list.Flink;  // record where to start the next search
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            dwStatus = ERROR_INVALID_PARAMETER;
        }
    }

    return dwStatus;
}
// This routine deallocates resources associated with a filesystem.  In general
// it shouldn't be called unless there's an error during initialization.
BOOL WINAPI 
DEVFS_CloseVolume(DWORD dwContext)
{
    SETFNAME("DEVFS_CloseVolume");
    DEBUGMSG(ZONE_FSD, (_T("%s: closing volume context %d\r\n"), pszFname, dwContext));
    DEBUGCHK(dwContext == FST_DEVICE || dwContext == FST_BUS);
    return TRUE;
}

// This routine opens a file handle for a device and returns it if
// successful.  During the process of opening this handle the device's
// Open() entry point is invoked, so the device has the opportunity to
// create context information for this handle.  This routine creates
// a handle structure for use by the device manager as well.
HANDLE WINAPI 
DEVFS_CreateFileW(DWORD dwContext, HANDLE hProc, PCWSTR pwsFileName, 
                  DWORD dwAccess, DWORD dwShareMode, PSECURITY_ATTRIBUTES pSecurityAttributes, 
                  DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile,
                  PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD SecurityDescriptorSize)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD dwStatus = ERROR_SUCCESS;
    SETFNAME("DEVFS_CreateFileW");

    DEBUGCHK(pwsFileName != NULL);

    DEBUGMSG(ZONE_FSD, (_T("+%s('%s'): context %d, hProc 0x%08x, access 0x%x, share 0x%x, sa 0x%08x, cr/disp 0x%x, flags 0x%x, hTemplate 0x%08x\r\n"),
        pszFname, pwsFileName, dwContext, hProc, dwAccess, dwShareMode, pSecurityAttributes, 
        dwCreate, dwFlagsAndAttributes, hTemplateFile));

    if(pwsFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // skip the leading directory separator
    DEBUGCHK(*pwsFileName == L'\\');
    pwsFileName++;

    // start checking parameters -- filesys has already done basic validation
    if((dwContext != FST_DEVICE && dwContext != FST_BUS)
    || pSecurityAttributes != NULL
    || !(dwCreate == OPEN_EXISTING || dwCreate == OPEN_ALWAYS) ) {
        dwStatus = ERROR_INVALID_PARAMETER;
    } else if(wcschr(pwsFileName, L'\\') != NULL) {
        dwStatus = ERROR_BAD_PATHNAME;
    }

    // ok to open the device?
    if(dwStatus == ERROR_SUCCESS) {
        NameSpaceType nameType;
        if(dwContext == FST_BUS) {
            nameType = NtBus;
        } else {
            nameType = NtDevice;
        }
        h = I_CreateDeviceHandle(pwsFileName, nameType, dwAccess, dwShareMode, hProc);
        DEBUGMSG(h == INVALID_HANDLE_VALUE && ZONE_WARNING, 
            (_T("%s: I_CreateDeviceHandle('%s') failed for name type '%s'\r\n"), 
            pszFname, pwsFileName, nameType == NtBus ? L"bus" : L"device"));
    }

    // pass back error code if there was a problem
    if(dwStatus != ERROR_SUCCESS) {
        DEBUGCHK(h == INVALID_HANDLE_VALUE);
        SetLastError(dwStatus);
    }

    DEBUGMSG(ZONE_FSD, (_T("-%s: returning handle 0x%08x, error status is %d\r\n"), 
        pszFname, h, dwStatus));
    
    return h;
}

//
// This routine is used to enumerate activated devices.  It can also be used to
// find devices loaded with RegisterDevice().  Even though we are registered
// as a filesys mount point, we don't want to support the "standard" file search
// because the data returned in WIN32_FIND_DATA is pretty much useless -- so 
// we screen out these calls by looking for certain patterns in the name string
// and by checking the size of the pfd parameter.
//
HANDLE WINAPI DEVFS_FindFirstFileW(DWORD dwContext, HANDLE hProc, PCWSTR pwsFileSpec, PDEVMGR_DEVICE_INFORMATION pdi, DWORD dwDevInfoSize)
{
    PDEVICESEARCHSTATE pss = NULL;
    DWORD dwStatus = ERROR_INVALID_PARAMETER;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    PCWSTR pwsSearchParam;
    DWORD dwLen;

    // NOTE: FS_FindFirstFile did not validate pd, so it is 
    // the responsibility of every AFS to validate it.
/*  Security TODO: Probably we should copy PDI on user mode caller.
    if (OEM_CERTIFY_TRUST != CeGetProcessTrust(hProc)) {
        pdi = MapPtrToProcWithSize(pdi, sizeof(DEVMGR_DEVICE_INFORMATION), hProc);
    }
*/    
    if(pdi == NULL || pwsFileSpec == NULL || *pwsFileSpec != L'\\' || dwContext != FST_DEVICE ||
       dwDevInfoSize != sizeof(DEVMGR_DEVICE_INFORMATION)) {
        goto done;
    }
    pwsFileSpec++;                      // skip leading separator

    // set up search state structure
    pss = LocalAlloc(0, sizeof(*pss));
    pss->lpdev = NULL;
    pwsSearchParam = wcschr(pwsFileSpec, L'+');
    if(pwsSearchParam == NULL) {
        goto done;
    }
    pwsSearchParam++;
    dwLen = (pwsSearchParam - pwsFileSpec) - 1;     // get the length of the search type without the +
    DEBUGCHK(dwLen != 0);
    if(wcsncmp(pwsFileSpec, L"byLegacyName", dwLen) == 0) {
        pss->searchType = DeviceSearchByLegacyName;
        wcsncpy(pss->u.szName, pwsSearchParam, ARRAYSIZE(pss->u.szName));
        pss->u.szName[ARRAYSIZE(pss->u.szName) - 1] = 0;
    } 
    else if(wcsncmp(pwsFileSpec, L"byDeviceName", dwLen) == 0) {
        pss->searchType = DeviceSearchByDeviceName;
        wcsncpy(pss->u.szName, pwsSearchParam, ARRAYSIZE(pss->u.szName));
        pss->u.szName[ARRAYSIZE(pss->u.szName) - 1] = 0;
    } 
    else if(wcsncmp(pwsFileSpec, L"byBusName", dwLen) == 0) {
        pss->searchType = DeviceSearchByBusName;
        wcsncpy(pss->u.szName, pwsSearchParam, ARRAYSIZE(pss->u.szName));
        pss->u.szName[ARRAYSIZE(pss->u.szName) - 1] = 0;
    } 
    else if(wcsncmp(pwsFileSpec, L"byInterfaceClass", dwLen) == 0) {
        pss->searchType = DeviceSearchByGuid;
        if(!ConvertStringToGuid (pwsSearchParam, &pss->u.guidClass)) {
            goto done;
        }
    } 
    else if(wcsncmp(pwsFileSpec, L"byParent", dwLen) == 0) {
        PWSTR pwsEnd;
        pss->searchType = DeviceSearchByParent;
        pss->u.hParent = (HANDLE) wcstol(pwsSearchParam, &pwsEnd, 16);
        if(*pwsEnd != 0) {
            goto done;
        }
    } else {
        goto done;
    }

    EnterCriticalSection(&g_devcs);
    
    // Add this search structure to the list -- do this now so
    // we are notified if the currently searched device is deactivated
    // while we are creating the API handle.
    pss->pNext = gpDeviceSearchList;
    gpDeviceSearchList = pss;

    // search for the first match
    __try {
        dwStatus = I_FindNextDevice(pss, pdi);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwStatus = ERROR_INVALID_PARAMETER;
    }

    LeaveCriticalSection(&g_devcs);

    // if we found a match then set up appropriately to continue
    // searching with the find-next function.
    if(dwStatus == ERROR_SUCCESS) {
        hFind = CreateAPIHandle(ghDevFindAPI, pss);
        if (hFind == NULL) {
            dwStatus = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

done:
    if(dwStatus != ERROR_SUCCESS) {
        if(pss != NULL) {
            // remove it from the list
            if(!DEVFS_FindClose(pss)) {
                LocalFree(pss);         // free memory
            }
        }
        SetLastError(dwStatus);
        hFind = INVALID_HANDLE_VALUE;
    }
    return hFind;
}

// ------------- file searching functions ------------------------

BOOL 
DEVFS_FindNextFileW(PDEVICESEARCHSTATE pss, PDEVMGR_DEVICE_INFORMATION pdi)
{
    DWORD dwStatus = ERROR_SUCCESS;
    PDEVICESEARCHSTATE pssTrav;

    if(pdi == NULL) {
        dwStatus = ERROR_INVALID_PARAMETER;
        goto done;
    }
    EnterCriticalSection(&g_devcs);

    // validate the search parameter
    for(pssTrav = gpDeviceSearchList; pssTrav != NULL; pssTrav = pssTrav->pNext) {
        if(pssTrav == pss) {
            break;
        }
    }
    if(pssTrav != pss) {
        dwStatus = ERROR_INVALID_HANDLE;
    } else {
        __try {
            dwStatus = I_FindNextDevice(pss, pdi);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            dwStatus = ERROR_INVALID_PARAMETER;
        }
    }
    LeaveCriticalSection(&g_devcs);

done:
    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
    }
    return(dwStatus == ERROR_SUCCESS ? TRUE : FALSE);
}

BOOL 
DEVFS_FindClose(PDEVICESEARCHSTATE pss)
{
    DWORD dwStatus = ERROR_SUCCESS;
    PDEVICESEARCHSTATE pssTrav, pssPrev;

    DEBUGCHK(pss != NULL);

    EnterCriticalSection(&g_devcs);

    // validate the search state parameter
    pssPrev = NULL;
    for(pssTrav = gpDeviceSearchList; pssTrav != NULL; pssTrav = pssTrav->pNext) {
        if(pssTrav == pss) {
            break;
        }
        pssPrev = pssTrav;
    }

    // did we find it?
    if(pssTrav != pss) {
        dwStatus = ERROR_INVALID_HANDLE;
    } else {
        // remove it from the list
        if(pssPrev == NULL) {
            gpDeviceSearchList = pss->pNext;    // first in list
        } else {
            pssPrev->pNext = pss->pNext;        // middle or end of list
        }
    }
    
    LeaveCriticalSection(&g_devcs);

    // free resources if present
    if(dwStatus == ERROR_SUCCESS) {
        LocalFree(pss);
    } else {
        SetLastError(dwStatus);
    }

    return(dwStatus == ERROR_SUCCESS ? TRUE : FALSE);
}


// ------------- miscellaneous functions ------------------------

DWORD DEVFS_StubFunction(void)
{
    return 0;
}


// this routine registers the filesystems used to access devices
BOOL
InitDeviceFilesystems(void)
{
    BOOL fOk = TRUE;
    SETFNAME("InitDeviceFileSystems");

    DEBUGMSG(ZONE_INIT, (_T("+%s\r\n"), pszFname));

    // register API sets
    ghDevFSAPI = CreateAPISet("DMFS", ARRAYSIZE(gpfnDevFSAPIs), (const PFNVOID *) gpfnDevFSAPIs, gDevFSSigs);
    RegisterAPISet(ghDevFSAPI, HT_AFSVOLUME | REGISTER_APISET_TYPE);
    
    ghDevFileAPI = CreateAPISet("DMFA", ARRAYSIZE(ExDevFileApiMethods), (const PFNVOID *) ExDevFileApiMethods, DevFileApiSigs);
    RegisterDirectMethods(ghDevFileAPI, DevFileApiMethods );
    RegisterAPISet(ghDevFileAPI, HT_FILE | REGISTER_APISET_TYPE);
    
    ghDevFindAPI = CreateAPISet("DMFF", ARRAYSIZE(gpfnExDevFindAPIs), (const PFNVOID *) gpfnExDevFindAPIs, gDevFindSigs);
    RegisterDirectMethods(ghDevFindAPI, gpfnDevFindAPIs );
    RegisterAPISet(ghDevFindAPI, HT_FIND | REGISTER_APISET_TYPE);
    gpDeviceSearchList = NULL;

    // did we succeed?
    if(ghDevFSAPI == NULL || ghDevFileAPI == NULL || ghDevFindAPI == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("%s: couldn't register API sets\r\n"), pszFname));
        if(ghDevFSAPI != NULL) CloseHandle(ghDevFSAPI);
        if(ghDevFileAPI != NULL) CloseHandle(ghDevFileAPI);
        if(ghDevFindAPI != NULL) CloseHandle(ghDevFindAPI);
        fOk = FALSE;
    }
    if(fOk) {
        struct {
            LPCWSTR pszFSName;
            DWORD dwFSContext;
            int iFSIndex;
        } fsInfo[] = {
            { L"$device", FST_DEVICE, INVALID_AFS },
            { L"$bus", FST_BUS, INVALID_AFS },
        };
        int i;

        // register filesystems
        for(i = 0; fOk && i < ARRAYSIZE(fsInfo); i++) {
            // register filesystems
            fsInfo[i].iFSIndex = RegisterAFSName(fsInfo[i].pszFSName);
            if(fsInfo[i].iFSIndex == INVALID_AFS) {
                DEBUGMSG(ZONE_ERROR, (_T("%s: RegisterAFSName('%s') failed %d\r\n"), pszFname, 
                    fsInfo[i].pszFSName, GetLastError()));
                fOk = FALSE;
            } else {
                fOk = RegisterAFSEx(fsInfo[i].iFSIndex, ghDevFSAPI, fsInfo[i].dwFSContext, AFS_VERSION, AFS_FLAG_HIDDEN);
                DEBUGMSG(!fOk && ZONE_ERROR, (_T("%s: RegisterAFSEx() failed for '%s' (error %d)\r\n"),
                    pszFname, fsInfo[i].pszFSName, GetLastError()));
                if(!fOk) {
                    DeregisterAFSName(fsInfo[i].iFSIndex);
                }
            }
        }
        
        // did we succeed?
        if(!fOk) {
            DEBUGCHK(i > 0);
            while(i > 0) {
                i--;
                DeregisterAFS(fsInfo[i].iFSIndex);
                DeregisterAFSName(fsInfo[i].iFSIndex);
            }
        }
    }
    
    DEBUGMSG(ZONE_INIT, (_T("-%s: status is %d\r\n"), pszFname, fOk));
    DEBUGCHK(fOk);
    return fOk;
}
