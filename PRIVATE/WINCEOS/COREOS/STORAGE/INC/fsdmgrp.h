//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    fsdmgrp.h

Abstract:

    This file contains internal definitions for the WinCE FSD Manager (FSDMGR.DLL).

--*/

#ifndef FSDMGRP_H
#define FSDMGRP_H


#include <windows.h>
#include <tchar.h>
#include <types.h>
#include <excpt.h>
#include <memory.h>

#include <diskio.h>
#include <fsddbg.h>

#define HFSD    PFSD
#define HDSK    PDSK
#define HVOL    PVOL
#define PVOLUME PVOL
#define PFILE   PHDL
#define PSEARCH PHDL


typedef struct _FSD FSD, *PFSD;
typedef struct _DSK DSK, *PDSK;
typedef struct _VOL VOL, *PVOL;
typedef struct _HDL HDL, *PHDL;

#include <fsdmgr.h>             // has to be deferred until HDSK, HVOL, etc, are defined

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */







#define AFSAPI_CLOSEVOLUME                      0
#define AFSAPI_RESERVED                         1
#define AFSAPI_CREATEDIRECTORYW                 2
#define AFSAPI_REMOVEDIRECTORYW                 3
#define AFSAPI_GETFILEATTRIBUTESW               4
#define AFSAPI_SETFILEATTRIBUTESW               5
#define AFSAPI_CREATEFILEW                      6
#define AFSAPI_DELETEFILEW                      7
#define AFSAPI_MOVEFILEW                        8
#define AFSAPI_FINDFIRSTFILEW                   9
#define AFSAPI_REGISTERFILESYSTEMNOTIFICATION   10
#define AFSAPI_OIDGETINFO                       11
#define AFSAPI_PRESTOCHANGOFILENAME             12
#define AFSAPI_CLOSEALLFILES                    13
#define AFSAPI_GETDISKFREESPACE                 14
#define AFSAPI_NOTIFY                           15
#define AFSAPI_REGISTERFILESYSTEMFUNCTION       16
#define AFSAPI_FSIOCONTROL			21

#define FILEAPI_CLOSEFILE                       0
#define FILEAPI_RESERVED                        1
#define FILEAPI_READFILE                        2
#define FILEAPI_WRITEFILE                       3
#define FILEAPI_GETFILESIZE                     4
#define FILEAPI_SETFILEPOINTER                  5
#define FILEAPI_GETFILEINFORMATIONBYHANDLE      6
#define FILEAPI_FLUSHFILEBUFFERS                7
#define FILEAPI_GETFILETIME                     8
#define FILEAPI_SETFILETIME                     9
#define FILEAPI_SETENDOFFILE                    10
#define FILEAPI_DEVICEIOCONTROL                 11
#define FILEAPI_READFILEWITHSEEK                12
#define FILEAPI_WRITEFILEWITHSEEK               13

#define FINDAPI_FINDCLOSE                       0
#define FINDAPI_RESERVED                        1
#define FINDAPI_FINDNEXTFILEW                   2

#if !defined(INVALID_AFS)
#if OID_FIRST_AFS != 0
#define INVALID_AFS     0
#else
#define INVALID_AFS     -1
#endif
#endif




#define TEXTW(s)                L##s
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)           (sizeof(a)/sizeof(a[0]))
#endif


/*  Doubly linked lists:
 *
 *  Should be the first element of structure being linked.  It may be used
 *  as the head of a list anywhere in a structure which contains the list.
 *
 *  NOTE: the multiple definitions of the basic DLINK structure are here
 *  simply to provide additional information to the debugger regarding what
 *  kind of structure each link in a particular DLINK points to.
 */

typedef struct _DLINK DLINK, *PDLINK;

struct _DLINK {
    PDLINK      next;           // ptr to next item in list
    PDLINK      prev;           // ptr to previous item in list
};

typedef struct _FSD_DLINK {
    PFSD        pFSDNext;       // ptr to next item in list
    PFSD        pFSDPrev;       // ptr to previous item in list
} FSD_DLINK, *PFSD_DLINK;

typedef struct _HDL_DLINK {
    PHDL        pHdlNext;       // ptr to next item in list
    PHDL        pHdlPrev;       // ptr to previous item in list
} HDL_DLINK, *PHDL_DLINK;

typedef struct _VOL_DLINK {
    PVOL        pVolNext;       // ptr to the next item in list
    PVOL        pVolPrev;       // ptr to the previous item in list
} VOL_DLINK, *PVOL_DLINK;

typedef BOOL    (APIENTRY *PFNAPI)();
typedef BOOL    (APIENTRY *PFNMOUNTDISK)(PDSK pDsk);
typedef BOOL    (APIENTRY *PFNUNMOUNTDISK)(PDSK pDsk);
typedef DWORD   (APIENTRY *PFNRWDISK)(PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
typedef DWORD   (APIENTRY *PFNHOOKVOLUME)(HDSK hDsk, PFILTERHOOK pHook);
typedef BOOL    (APIENTRY *PFNUNHOOKVOLUME)(DWORD hVolume);  


#ifdef DEBUG
#define SIGFIELD                DWORD   dwSig;
#define INITSIG(p,s)            ((p)->dwSig = (s))
#define VALIDSIG(p,s)           ((p) && (p)->dwSig == (DWORD)(s))
#define FSD_SIG                 0x20445346      // "FSD "
#define DSK_SIG                 0x44445346      // "FSDD"
#define VOL_SIG                 0x56445346      // "FSDV"
#define HFILE_SIG               0x46445346      // "FSDF"
#define HSEARCH_SIG             0x53445346      // "FSDS"
#else
#define SIGFIELD
#define INITSIG(p,s)
#define VALIDSIG(p,s)           TRUE
#endif

/*  HDL structure:
 *
 *  This internal structure keeps track of all information on a HDL.
 */

#define HDL_FILE                0x00000001
#define HDL_SEARCH              0x00000002
#define HDL_CONSOLE             0x20000000
#define HDL_PSUEDO              0x40000000 // Handle was created using VOL:
#define HDL_INVALID             0x80000000


struct _HDL {
    HDL_DLINK   dlHdl;          // link in per-volume handle list
    PVOL        pVol;           // pointer back to owning VOL
    SIGFIELD
    DWORD       dwFlags;        // see HDL_* above
    DWORD       dwHdlData;      // dwHdlData provided to FSDMGR_CreateXXXXHandle
    HANDLE      hProc;          // originating process handle
    HANDLE      h;              // the actual handle created via CreateAPIHandle
    HANDLE      hNotify;        // Notification Handle
};

/*  VOL structure:
 *
 *  This internal structure keeps track of all information on a VOL.
 */

#define VOL_POWERDOWN           0x00000001
#define VOL_DETACHED            0x00000002
#define VOL_DEINIT              0x00000004
#define VOL_UNAVAILABLE         (VOL_POWERDOWN | VOL_DETACHED)

struct _VOL {
    VOL_DLINK   dlVol;          // link in global VOL list
    HDL_DLINK   dlHdlList;      // per-volume handle list
    PDSK        pDsk;           // pointer back to owning DSK
    SIGFIELD
    HANDLE      hNotifyHandle;   // Notification Handle
    HANDLE      hThreadExitEvent; // event to signal when all threads have left a deallocated volume
    DWORD       dwFlags;        // see VOL_* above
    DWORD       dwVolData;      // pVolume provided to FSDMGR_RegisterVolume
    int         iAFS;           // AFS number obtained by FSDMGR_RegisterVolume
    LONG        cThreads;       // count of threads accessing this volume
    WCHAR       wsVol[64];      // some reasonable amount of space to record name from FSDMGR_RegisterVolume
};

/*  DSK structure:
 *
 *  This internal structure keeps track of all information on a DSK.
 */

#define DSK_NONE                0x00000000
#define DSK_REMOUNTED           0x00000001
#define DSK_UNCERTAIN           0x00000002
#define DSK_NO_IOCTLS           0x00000004
#define DSK_NEW_IOCTLS          0x00000008
#define DSK_READONLY            0x00000010
#define DSK_CLOSED              0x00000020
#define DSK_MOUNTHIDDEN         0x00010000
#define DSK_MOUNTBOOTABLE       0x00020000
#define DSK_MOUNTROOTFS         0x00040000
#define DSK_MOUNTHIDEROM        0x00080000

#if DSK_REMOUNTED != 0x1
#error DSK_REMOUNTED is depended upon to be bit 0!
#endif

#if DSK_READONLY != FDI_READONLY
#error DSK_READONLY is depended upon to match FDI_READONLY!
#endif

struct _DSK {
    PVOL        pVol;           // Pointer to the volume
    DWORD       dwFilterVol;     // 
    PFSD        pFSD;           // pointer back to owning FSD
    SIGFIELD
    DWORD       dwFlags;        // see DSK_* above
    FSD_DISK_INFO fdi;          // info from initial FSDMGR_GetDiskInfo
    HANDLE      hDsk;           // disk handle we created in FSDMGR_InitEx
    HANDLE      hPartition;     // hPartition;  
    HANDLE      hActivityEvent; // Activity Timer Event;
    PDEVICEIOCONTROL pDeviceIoControl;
    struct _DSK *pNextDisk;
    struct _DSK *pPrevDisk;
    WCHAR       wsDsk[MAX_PATH];
};


#define FSD_FLAGS_PAGEABLE  0x00000001

/*  FSD structure:
 *
 *  This internal structure keeps track of all information on an FSD.
 */

struct _FSD {
    FSD_DLINK       dlFSD;      // link in global FSD list
    HMODULE         hFSD;       // module handle to FSD (given to us by DEVICE.EXE)
    HANDLE          hDsk;
    SIGFIELD
    PFNMOUNTDISK    pfnMountDisk;       // address of the FSD's FSD_MountDisk export
    PFNUNMOUNTDISK  pfnUnmountDisk;     // address of the FSD's FSD_UnmountDisk export
    PFNHOOKVOLUME   pfnHookVolume;      // address of the FILTER's FSD_HookVolume export
    PFNUNHOOKVOLUME pfnUnhookVolume;    // address of the FILTER's FSD_UnhookVolume export
    DWORD           dwFlags;
    TCHAR           szFileSysName[32];
    TCHAR           szRegKey[MAX_PATH];
    TCHAR           szReserved[MAX_PATH];
    PREFRESHVOLUME  pRefreshVolume;
    PFNAPI          apfnAFS[NUM_AFS_APIS];
    PFNAPI          apfnFile[NUM_FILE_APIS];
    PFNAPI          apfnFind[NUM_FIND_APIS];
    WCHAR           wsFSD[MAX_PATH];    // FSD's entry-point prefix
};

typedef struct _FSDINITDATA {
    HANDLE hFSD;
    HANDLE hDsk;
    HANDLE hPartition;
    HANDLE hActivityEvent;
    PDEVICEIOCONTROL pIoControl;
    DWORD dwFlags;
    TCHAR szRegKey[MAX_PATH]; // Max Profile Name Size
    TCHAR szFileSysName[32]; // Filesys Name Syse
    TCHAR szDiskName[MAX_PATH];
    PDSK  pNextDisk;
} FSDINITDATA, *PFSDINITDATA;

#define MAX_FSD_NAME_SIZE        128
#define MAX_ENTRYPOINT_NAME_SIZE 32



/*  INIT.C globals
 */

extern  HINSTANCE   hDLL;       // module handle of this DLL
extern  HANDLE      hAFSAPI;
extern  HANDLE      hFileAPI;
extern  HANDLE      hFindAPI;
extern  FSD_DLINK   dlFSDList;  // global FSD list
extern  VOL_DLINK   dlVOLList;  // global VOL list
extern  CRITICAL_SECTION csFSD; // global CS for this DLL
extern  HANDLE      g_hResumeEvent; // global event indicating that we have resmumed the device


/*  INIT.C functions (interfaces exported by FSDMGR.DLL to DEVICE.EXE)
 */

PDSK    FSDMGR_InitEx(HMODULE hFSD, PCWSTR pwsDsk, PVOID pReserved);
int     FSDMGR_DeinitEx(PDSK pDsk);
PDSK    FSDLoad( FSDINITDATA *pInitData);


DWORD Init(DWORD dwContext);
DWORD Deinit();
 
/*  ALLOC.C functions
 */

PFSD    AllocFSD(HMODULE hFSD, HANDLE hDsk);
BOOL    DeallocFSD(PFSD pFSD);
PDSK    AllocDisk(PFSD pFSD, PCWSTR pwsDsk, HANDLE hDsk, PDEVICEIOCONTROL pIoControl);
void    MarkDisk(PDSK pDsk, DWORD dwFlags);
void    UnmarkDisk(PDSK pDsk, DWORD dwFlags);
BOOL    DeallocDisk(PDSK pDsk);
PVOL    AllocVolume(PDSK pDsk, DWORD dwVolData);
BOOL    DeallocVolume(PVOL pVol);
BOOL    DeallocVolumeHandles(PVOL pVol);
HANDLE AllocFSDHandle(PVOL pVol, HANDLE hProc, HANDLE hNotify, DWORD dwHdlData, DWORD dwFlags);
void    DeallocFSDHandle(PHDL pHdl);
PDSK    InitEx(FSDINITDATA *pInitData);


/*  APIS.C functions
 */

#undef  FSDAPI
#define FSDAPI  FSDMGR
#include <fsdmgr.h>     // leverage the prototypes in fsdmgr.h to declare our functions

// FSDMGR doesn't expose all possible (past and present) file system entry points, but it does
// intercept some of them, so we have to privately declare those here.

BOOL    FSDMGR_CloseAllFiles(HVOL hVol, HANDLE hProc);


/*  MISC.C functions
 */

void    InitList(PDLINK pdl);
BOOL    IsListEmpty(PDLINK pdl);
void    AddListItem(PDLINK pdlIns, PDLINK pdlNew);
void    RemoveListItem(PDLINK pdl);
BOOL    CompareFSDs(HMODULE hFSD1, HMODULE hFSD2);
PFNAPI  GetFSDProcAddress(PFSD pFSD, PCWSTR pwsBaseFunc);
BOOL    GetFSDProcArray(PFSD pFSD, PFNAPI *apfnFuncs, CONST PFNAPI *apfnStubs, CONST PCWSTR *apwsBaseFuncs, int cFuncs);
int     GetAFSName(int iAFS, PWSTR pwsAFS, int cchMax);
BOOL    FsdGetRegistryValue(HKEY hKey, PCTSTR szValueName, PDWORD pdwValue);
BOOL    FsdGetRegistryString( HKEY hKey, PCTSTR szValueName, PTSTR szValue, DWORD dwSize);
int     FsdStringFromGuid(GUID *pGuid, LPTSTR pszBuf);
BOOL    FsdGuidFromString(LPCTSTR pszGuid,GUID *pGuid);
BOOL    FsdLoadFlag(HKEY hKey, const TCHAR * szValueName, PDWORD pdwFlag, DWORD dwSet);


/*  SERV.C functions
 */

DWORD   FSDMGR_GetDiskInfo(HDSK hDsk, PFDI pfdi);
DWORD   FSDMGR_ReadDisk(HDSK hDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
DWORD   FSDMGR_WriteDisk(HDSK hDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
DWORD   FSDMGR_ReadDiskEx(PFSGI pfsgi, PFSGR pfsgr);
DWORD   FSDMGR_WriteDiskEx(PFSGI pfsgi, PFSGR pfsgr);
PVOL    FSDMGR_RegisterVolume(HDSK hDsk, PWSTR pwsName, PVOLUME pVolume);
int     FSDMGR_GetVolumeName(PVOL pVol, PWSTR pwsName, int cchMax);
void    FSDMGR_DeregisterVolume(PVOL pVol);
HANDLE  FSDMGR_CreateFileHandle(PVOL pVol, HANDLE hProc, PFILE pFile);
HANDLE  FSDMGR_CreateSearchHandle(PVOL pVol, HANDLE hProc, PSEARCH pSearch);
HANDLE  FSDMGR_FindFirstChangeNotificationW(PVOL pVol, HANDLE hProc, LPCWSTR lpPathName, BOOL bWatchSubtree, DWORD dwNotifyFilter);

typedef DWORD (*PFN_FORMATVOLUME)(HANDLE,  LPVOID);
typedef DWORD (*PFN_SCANVOLUME)(HANDLE, LPVOID);

 
/*  STUBS.C functions
 */

#undef  FSDAPI
#define FSDAPI  FSDMGRStub
#include <fsdmgr.h>     // leverage the prototypes in fsdmgr.h to declare our stubs

#undef  FSDAPI
#define FSDAPI  FILTER
#include <fsdmgr.h>     // leverage the prototypes in fsdmgr.h to declare our filter functions


#ifdef __cplusplus
    }
#endif /* __cplusplus */


/*  TABLES.C globals
 */

#ifdef DEBUG
extern  DBGPARAM        dpCurSettings;
#endif

extern  CONST PCWSTR    apwsAFSAPIs[NUM_AFS_APIS];
extern  CONST PFNAPI    apfnAFSAPIs[NUM_AFS_APIS];
extern  CONST PFNAPI    apfnAFSStubs[NUM_AFS_APIS];
extern  CONST DWORD     asigAFSAPIs[NUM_AFS_APIS];

extern  CONST PCWSTR    apwsFileAPIs[NUM_FILE_APIS];
extern  CONST PFNAPI    apfnFileAPIs[NUM_FILE_APIS];
extern  CONST PFNAPI    apfnFileStubs[NUM_FILE_APIS];
extern  CONST DWORD     asigFileAPIs[NUM_FILE_APIS];

extern  CONST PCWSTR    apwsFindAPIs[NUM_FIND_APIS];
extern  CONST PFNAPI    apfnFindAPIs[NUM_FIND_APIS];
extern  CONST PFNAPI    apfnFindStubs[NUM_FIND_APIS];
extern  CONST DWORD     asigFindAPIs[NUM_FIND_APIS];


/*  Last but not least, DEBUG-only stuff
 */

#define DEBUGMSGW               DEBUGMSG
#ifndef ASSERT
#define ASSERT(c)               DEBUGCHK(c)
#endif

#if defined(XREF_CPP_FILE)
#define ERRFALSE(exp)
#elif !defined(ERRFALSE)
//  This macro is used to trigger a compile error if exp is false.
//  If exp is false, i.e. 0, then the array is declared with size 0, triggering a compile error.
//  If exp is true, the array is declared correctly.
//  There is no actual array however.  The declaration is extern and the array is never actually referenced.
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif


#ifdef DEBUG
#ifdef UNDER_CE
#define OWNCRITICALSECTION(cs)  ((cs)->LockCount > 0 && (DWORD)(cs)->OwnerThread == GetCurrentThreadId())
#else
#define OWNCRITICALSECTION(cs)  TRUE    
#endif

#else   // !DEBUG

#define OWNCRITICALSECTION(cs)  TRUE

#endif  // !DEBUG


#define DBGTEXT(fmt)            TEXT(fmt)
#define DBGTEXTW(fmt)           TEXTW(fmt)
#define RETAILMSGBREAK(cond,msg) if (cond) {RETAILMSG(TRUE,msg); DebugBreak();} else

#if 0
typedef struct _IOREQ
{
    HANDLE      hThread; 
    HANDLE      hProcess;
    DWORD       dwCode;
    DWORD       dwCount;
    PVOL        pVol;
    PHDL        pHdl;      
    DWORD       dwFlags;
    DWORD       dwStatus;
    struct _IOREQ *pPrev;
    struct _IOREQ *pNext;
} IOREQ, * PIOREQ;
#endif

typedef struct _FSDLOADLIST
{
    TCHAR       szPath[MAX_PATH];
    TCHAR       szName[MAX_PATH];
    DWORD       dwOrder;
    struct _FSDLOADLIST *pNext;
} FSDLOADLIST, *PFSDLOADLIST;

#define LOAD_FLAG_SYNC      0x00000001
#define LOAD_FLAG_ASYNC     0x00000002

PFSDLOADLIST LoadFSDList( HKEY hKey, DWORD dwFlags, TCHAR *szPath = NULL, PFSDLOADLIST pExisting = NULL, BOOL bReverse = FALSE);
PDSK HookFilters(PDSK pDsk, PFSD pFSD);


#endif  // FSDMGRP_H
