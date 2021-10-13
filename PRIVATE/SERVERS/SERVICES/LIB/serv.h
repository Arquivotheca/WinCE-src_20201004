//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------------
//
//
// Module:
//
//     servicep.h
//
//
// --------------------------------------------------------------------------

#ifndef __SERVICES_H__
#define __SERVICES_H__ 1

#include <windows.h>
#include <memory.h>
#include <linklist.h>

#if defined (OLD_CE_BUILD)
  #include <winsock.h>
  #define AF_INET6 23
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

#if (_WIN32_WCE < 400)
// The following are used by services.exe internally only but aren't in service.h.
//
// (from public\common\ddk\inc\devload.h)
//
#define DEVFLAGS_NONE 0x00000000            // No flags defined
#define DEVFLAGS_UNLOAD 0x00000001          // Unolad driver after call to entry point returns
#define DEVFLAGS_LOADLIBRARY 0x00000002     // Use LoadLibrary instead of LoadDriver
#define DEVFLAGS_NOLOAD 0x00000004          // Don't load Dll
#define DEVLOAD_FLAGS_VALNAME     L"Flags"  // Flag to control loading/unloading (optional)

#define MapCallerPtr(p,len)                 (p)
#endif

// Used for loading regenum.dll
typedef HANDLE (*PLOADFN) (LPCTSTR, DWORD);
typedef void (*PUNLOADFN) (HANDLE);

typedef HANDLE (*PFN_SERVICE_ENTRY)(LPWSTR, PLOADFN, PUNLOADFN);   // Parameter is registry path of device's key


typedef DWORD (* pInitFn)(DWORD);
typedef DWORD (* pInitExFn)(HANDLE,PCWSTR,LPVOID);
typedef BOOL (* pDeinitFn)(DWORD);
typedef BOOL (* pOpenFn)(DWORD,DWORD,DWORD);
typedef BOOL (* pCloseFn)(DWORD);
typedef DWORD (* pReadFn)(DWORD,LPVOID,DWORD);
typedef DWORD (* pWriteFn)(DWORD,LPCVOID,DWORD);
typedef DWORD (* pSeekFn)(DWORD,long,DWORD);
typedef BOOL (* pControlFn)(DWORD,DWORD,PBYTE,DWORD,PBYTE,DWORD,PDWORD);
typedef void (* pPowerupFn)(DWORD);
typedef void (* pPowerdnFn)(DWORD);

#define DEVFLAGS_AUTO_DEREGISTER      0x0002

//
// Structure to track device drivers
//
typedef struct fsdev_t {
	LIST_ENTRY list;
	DWORD index;
	DWORD dwData;
	pInitFn fnInit;
	pDeinitFn fnDeinit;
	pOpenFn fnOpen;
	pCloseFn fnClose;
	pReadFn fnRead;
	pWriteFn fnWrite;
	pSeekFn fnSeek;
	pControlFn fnControl;
	HINSTANCE hLib;
    WCHAR type[3];
    WORD wFlags;
    DWORD dwRefCnt;
    WCHAR *szDllName;
    DWORD dwDllNameLen;
    WCHAR *szRegPath;
    DWORD dwRegPathLen;
    DWORD dwContext;    // context value passed to xxx_Init().
    DWORD dwFlags;
} fsdev_t;

//
// Note on reference counts:
// lpdwDevRefCnt points to the ref count of the associated fsdev_t. While fsdev_t.dwRefCnt is not 0, then
// the fsdev_t will not be freed.
// dwOpenRefCnt is the ref count for the fsopendev_t structure. While it is not 0, the fsopendev_t will
// not be freed.
// There were situations where the fsopendev_t structure was freed while a thread was blocked inside
// a DeviceIoControl call. dwOpenRefCnt will prevent this.
//
typedef struct fsopendev_t {
	struct fsopendev_t *nextptr;	// next one in linked list
	DWORD dwOpenData;
	fsdev_t *lpDev;
//	DWORD *lpdwDevRefCnt;	// since we set lpDev to NULL on device deregister
    DWORD dwOpenRefCnt;     // ref count for this structure 
	HANDLE KHandle;			// kernel handle pointing to this structure
	HPROCESS hProc;			// process owning this handle
} fsopendev_t;

BOOL SERV_ReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
	LPOVERLAPPED lpOverlapped);
BOOL SERV_WriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,
	LPOVERLAPPED lpOverlapped);
DWORD SERV_GetFileSize(fsopendev_t *fsodev, LPDWORD lpFileSizeHigh);
DWORD SERV_SetFilePointer(fsopendev_t *fsodev, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
	DWORD dwMoveMethod);
BOOL SERV_GetFileInformationByHandle(fsopendev_t *fsodev, LPBY_HANDLE_FILE_INFORMATION lpFileInfo);
BOOL SERV_FlushFileBuffers(fsopendev_t *fsodev);
BOOL SERV_GetFileTime(fsopendev_t *fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL SERV_SetFileTime(fsopendev_t *fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL SERV_SetEndOfFile(fsopendev_t *fsodev);
BOOL SERV_DeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
void SERV_ProcNotify(DWORD flags, HPROCESS proc, HTHREAD thread);
HANDLE SERV_RegisterService(LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo);
BOOL SERV_DeregisterService(HANDLE hDevice);
HANDLE SERV_ActivateService(LPCWSTR lpszDevKey, DWORD dwClientInfo);
void SERV_CloseAllServiceHandles(HPROCESS proc);
HANDLE SERV_CreateServiceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc);
BOOL SERV_CloseFileHandle(fsopendev_t *fsodev);
BOOL SERV_GetServiceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData);
HANDLE RegisterServiceEx( LPCWSTR lpszType, DWORD dwIndex, LPCWSTR lpszLib, DWORD dwInfo, DWORD dwFlags, LPCWSTR lpszFullRegPath);

BOOL SERV_EnumServices(PBYTE pBuffer, DWORD *pdwServiceEntries, DWORD *pdwBufferLen);
HANDLE SERV_GetServiceHandle(LPWSTR szPrefix, LPWSTR szDllName, DWORD *pdwDllBuf);
BOOL SERV_ServiceClosePort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, BOOL fRemoveFromRegistry);

BOOL SERV_ServiceIoControl(HANDLE hService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

BOOL SERV_ServiceAddPort(HANDLE hService, SOCKADDR *pSockAddr, int cbSockAddr, int iProtocol, WCHAR *szRegWritePath);
BOOL SERV_ServiceUnbindPorts(HANDLE hService);

BOOL StartSuperService(LPCWSTR szBuiltinPath, HANDLE hService, BOOL fSignalStarted);
BOOL SuperServicesInit(void);
void InitServicesDataStructures(void);
void DeInitServicesDataStructures(void);
BOOL UnloadSuperServices(fsdev_t *pOwner, BOOL fCallerHoldingCS, BOOL fReleaseCS);

DWORD WINAPI DeregisterServiceThread(LPVOID lpv);

extern CRITICAL_SECTION g_ServCS;
extern HANDLE g_hCleanEvt;

BOOL IsValidService(fsdev_t * lpdev, PLIST_ENTRY pList=NULL);
void FreeService(fsdev_t *lpdev, BOOL fRemoveFromDyingList=TRUE, BOOL fSpinThread=TRUE);
// Fast path service IO Control, for internal services.exe use only.
BOOL InternalServiceIOControl(fsdev_t *pOwner, DWORD dwIOControl, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut=NULL, DWORD dwLenOut=0);

HANDLE StartOneService(LPCWSTR RegPath, DWORD LoadOrder, BOOL fStandaloneProcess);


void NotifyAllServices(DWORD dwIOCtl, BYTE *pInData, DWORD cbInData);

int HandleServicesCommandLine(int argc, WCHAR **argv);
PSTR ValidatePSTR(const PSTR psz, const DWORD cbLen=MAX_PATH);
PWSTR ValidatePWSTR(const PWSTR psz, const DWORD cbLen=MAX_PATH);

#define ENTER_DEVICE_FUNCTION	{													\
	                                EnterCriticalSection(&g_ServCS);   				\
	    							__try

#define EXIT_DEVICE_FUNCTION        __except (EXCEPTION_EXECUTE_HANDLER) {			\
           							    SetLastError(ERROR_INVALID_PARAMETER);  	\
	                                }												\
	    							LeaveCriticalSection(&g_ServCS);    				\
	    						}

//
// Registry entries associated with services.exe loading 
//
// Base registry key of services.exe
#define     SERVICE_BUILTIN_KEY           L"Services"
#define     SERVICE_BUILTIN_KEY_STR_LEN   SVSUTIL_CONSTSTRLEN(SERVICE_BUILTIN_KEY)
#define     SERVICE_ACCEPT_KEY            L"\\Accept"
#define     SERVICE_ACCEPT_KEY_STR_LEN    SVSUTIL_CONSTSTRLEN(SERVICE_ACCEPT_KEY)
#define     SERVICE_CONFIG_KEY            L"\\Config"
#define     SERVICE_COFREEUNUSED_VALUE    L"StartCoFreeUnusedLibrariesThread"

// Values concerning SuperService...
#define     SERVICE_ACCEPT_SOCKADDR       L"SockAddr"
#define     SERVICE_ACCEPT_PROTOCOL       L"Protocol"

#define MAX_LOAD_ORDER 255

#define REG_PATH_LEN 256    // Length of registry path string

#ifdef DEBUG
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_ACCEPT     DEBUGZONE(5)
#define ZONE_ACTIVE     DEBUGZONE(6)
#define ZONE_TIMERS     DEBUGZONE(7)
#define ZONE_DYING      DEBUGZONE(9)
#endif  // DEBUG


// Make sure that thread was created by services.exe, and that we're
// not being called via a PSL.
#define VERIFY_SERVICES_EXE_IS_OWNER()   DEBUGCHK((DWORD)GetOwnerProcess() == GetCurrentProcessId())

#endif
