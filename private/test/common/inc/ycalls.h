//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
/*****************************************************************************

  Ycalls.h - header file for kernel wrapper calls

*****************************************************************************/



#ifndef __YCALLS_H__
#define __YCALLS_H__

#include <windows.h>
#include <bldver.h>
#include <AcctID.h>
#include <cerwlock.h>

typedef struct _APICALL {
    DWORD dwValue;
    LPTSTR szText;
} APICALLS;

#ifdef _PREFAST_
#define PREFAST_SUPPRESS( cWarning, comment) __pragma ( prefast(suppress: cWarning, comment) )
#else
#define PREFAST_SUPPRESS( cWarning, comment)
#endif

#define ECHO_NEVER  0
#define ECHO_FAIL   1
#define ECHO_ALWAYS 2

#ifndef UNDER_CE
#define CreateFileForMapping    CreateFile
#endif

#ifdef NO_YCALLS
#define YFormatMessage          FormatMessage
#define YVirtualAlloc           VirtualAlloc 
#define YVirtualFree            VirtualFree
#define YVirtualProtect         VirtualProtect
#define YVirtualQuery           VirtualQuery
#define YIsBadCodePtr           IsBadCodePtr
#define YIsBadWritePtr          IsBadWritePtr
#define YIsBadReadPtr           IsBadReadPtr
#define YHeapAlloc              HeapAlloc
#define YHeapFree               HeapFree
#define YHeapCreate             HeapCreate
#define YHeapDestroy            HeapDestroy
#define YHeapValidate           HeapValidate
#define YLocalAlloc             LocalAlloc
#define YLocalFree              LocalFree
#define YLocalReAlloc           LocalReAlloc
#define YLocalFlags             LocalFlags
#define YLocalSize              LocalSize
#define YGetProcessHeap         GetProcessHeap
#define YCreateFile             CreateFile
#define YCreateFileForMapping   CreateFileForMapping
#define YGetFileAttributes      GetFileAttributes
#define YSetFileAttributes      SetFileAttributes
#define YCreateDirectory        CreateDirectory
#define YRemoveDirectory        RemoveDirectory
#define YSetFilePointer         SetFilePointer
#define YCloseHandle            CloseHandle
#define YFindClose              FindClose
#define YWriteFile              WriteFile
#define YReadFile               ReadFile
#define YCreateEvent            CreateEvent
#define YPulseEvent             PulseEvent
#define YResetEvent             ResetEvent
#define YSetEvent               SetEvent
#define YSetEventData           SetEventData
#define YCreateMutex            CreateMutex
#define YReleaseMutex           ReleaseMutex
#define YWaitForSingleObject    WaitForSingleObject
#define YWaitForMultipleObjects WaitForMultipleObjects

#ifdef  WMMSG // define this if you do have GWES
#define YSendMessage            SendMessage
#define YPostMessage            PostMessage
#endif //WMMSG

#define YCreateFileMapping      CreateFileMapping
#define YMapViewOfFile          MapViewOfFile
#define YUnmapViewOfFile        UnmapViewOfFile
#define YFlushViewOfFile        FlushViewOfFile
#define YGetCurrentProcess      GetCurrentProcess
#define YGetCurrentProcessId    GetCurrentProcessId
#define YGetCurrentThread       GetCurrentThread
#define YGetCurrentThreadId     GetCurrentThreadId
#define YLoadLibrary            LoadLibrary
#define YLoadLibraryEx          LoadLibraryEx
#define YGetModuleHandle        GetModuleHandle
#define YGetProcAddress         GetProcAddress
#define YGetExitCodeThread      GetExitCodeThread
#define YGetExitCodeProcess     GetExitCodeProcess
#define YQueryPerformanceFrequency  QueryPerformanceFrequency
#define YQueryPerformanceCounter    QueryPerformanceCounter
#define YSetLocalTime           SetLocalTime
#define YSetLocalTime           SetLocalTime
#define YGetSystemTime          GetSystemTime
#define YGetSystemTime          GetSystemTime
#define YSetTimeZoneInformation SetTimeZoneInformation
#define YGetTimeZoneInformation GetTimeZoneInformation

#ifdef TLHLP // needs to link with toolhelp.lib to compile
#define YCreateToolhelp32Snapshot   CreateToolhelp32Snapshot
#define YProcess32First             Process32First
#define YProcess32Next              Process32Next
#define YThread32First              Thread32First
#define YThread32Next               Thread32Next
#define YModule32Next               Module32Next
#define YModule32First              YModule32First
#define YToolhelp32ReadProcessMemory    Toolhelp32ReadProcessMemory
#define YHeap32First                Heap32First
#define YHeap32Next                 Heap32Next
#define YHeap32ListFirst            Heap32ListFirst
#define YHeap32ListNext             Heap32ListNext
#define YCloseToolhelp32Snapshot    CloseToolhelp32Snapshot
#endif //TLHLP

#define YCreateThread               CreateThread
#define YResumeThread               ResumeThread
#define YSuspendThread              SuspendThread
#define YOpenThread                 OpenThread
#define YTerminateThread            TerminateThread
#define YSetThreadPriority          SetThreadPriority
#define YGetThreadPriority          GetThreadPriority
#define YGetThreadTimes             GetThreadTimes
#define YCreateProcess              CreateProcess
#define YCeCreateProcess            CeCreateProcessEx
#define YCeCreateProcessEx          CeCreateProcessEx
#define YTerminateProcess           TerminateProcess
#define YOpenProcess                OpenProcess
#define YOpenEvent                  OpenEvent
#define YDuplicateHandle            DuplicateHandle
#define YCreateAPIHandle            CreateAPIHandle
#define YCreateAPISet               CreateAPISet
#define YSetAPIErrorHandler         SetAPIErrorHandler
#define YRegisterAPISet             RegisterAPISet
#define YRegisterDirectMethods      RegisterDirectMethods
#define YIsAPIReady                 IsAPIReady
#define YWaitForAPIReady            WaitForAPIReady
#define YQueryAPISetID              QueryAPISetID
#define YLockAPIHandle              LockAPIHandle
#define YUnlockAPIHandle            UnlockAPIHandle
#define YVerifyAPIHandle            VerifyAPIHandle
#define YCreateWatchDogTimer        CreateWatchDogTimer
#define YOpenWatchDogTimer          OpenWatchDogTimer
#define YStartWatchDogTimer         StartWatchDogTimer
#define YStopWatchDogTimer          StopWatchDogTimer
#define YRefreshWatchDogTimer       RefreshWatchDogTimer
#define YPageOutModule              PageOutModule
#define YGetModuleFileName          GetModuleFileName
#define YCeSetProcessVersion        CeSetProcessVersion
#define YGetProcessVersion          GetProcessVersion
#define YCeGetModuleInfo            CeGetModuleInfo
#define YTlsAlloc                   TlsAlloc
#define YTlsFree                    TlsFree
#define YTlsGetValue                TlsGetValue
#define YTlsSetValue                TlsSetValue
#define YCeTlsAlloc                 CeTlsAlloc
#define YCeTlsFree                  CeTlsFree
#define YCreateMsgQueue             CreateMsgQueue
#define YOpenMsgQueue               OpenMsgQueue
#define YCloseMsgQueue              CloseMsgQueue
#define YGetMsgQueueInfo            GetMsgqueueInfo
#define YWriteMsgQueue              WriteMsgQueue
#define YReadMsgQueue               ReadMsgQueue
#define YCeCreateRwLock             CeCreateRwLock
#define YCeDeleteRwLock             CeDeleteRwLock
#define YCeAcquireRwLock            CeAcquireRwLock
#define YCeReleaseRwLock            CeReleaseRwLock
#define YCeIsRwLockAcquired         CeIsRwLockAcquired
#else

DWORD YFormatMessage(
  DWORD dwFlags, 
  LPCVOID lpSource, 
  DWORD dwMessageId, 
  DWORD dwLanguageId, 
  LPTSTR lpBuffer, 
  DWORD nSize, 
  va_list* Arguments
); 
LPVOID YVirtualAlloc( 
    LPVOID lpAddress,       // address of region to reserve or commit  
    DWORD dwSize,           // size of region 
    DWORD flAllocationType, // type of allocation 
    DWORD flProtect         // type of access protection    
   );
BOOL YVirtualFree( 
    LPVOID lpAddress,   // address of region of committed pages  
    DWORD dwSize,       // size of region 
    DWORD dwFreeType    // type of free operation    
   );

BOOL YVirtualProtect( 
    LPVOID lpAddress,   // address of region of committed pages 
    DWORD dwSize,       // size of the region 
    DWORD flNewProtect, // desired access protection 
    PDWORD lpflOldProtect   // address of variable to get old protection 
    );
 
DWORD YVirtualQuery( 
    LPCVOID lpAddress,  // address of region 
    PMEMORY_BASIC_INFORMATION lpBuffer, // address of information buffer 
    DWORD dwLength      // size of buffer 
    );
 
BOOL YIsBadCodePtr( 
    FARPROC lpfn    // address of function 
    );

BOOL YIsBadReadPtr( 
    LPVOID lp,      // address of memory block 
    UINT ucb        // size of block 
    );

BOOL YIsBadWritePtr( 
    LPVOID lp,      // address of memory block 
    UINT ucb        // size of block 
    ) ;

HANDLE YHeapCreate(
    DWORD flOptions,    // heap allocation flag 
    DWORD dwInitialSize,    // initial heap size 
    DWORD dwMaximumSize     // maximum heap size 
   );
BOOL YHeapDestroy(
    HANDLE hHeap    // handle to the heap 
   );   
HANDLE YGetProcessHeap(VOID);
LPVOID YHeapAlloc(
    HANDLE hHeap,   // handle to the private heap block 
    DWORD dwFlags,  // heap allocation control flags 
    DWORD dwBytes   // number of bytes to allocate    
   );
BOOL YHeapFree(
    HANDLE hHeap,   // handle to the heap 
    DWORD dwFlags,  // heap freeing flags 
    LPVOID lpMem    // pointer to the memory to free 
   );

BOOL YHeapSize(
    HANDLE hHeap,   // handle to the heap 
    DWORD dwFlags,  // heap freeing flags 
    LPVOID lpMem    // pointer to the memory to free 
   );

BOOL YHeapValidate( 
    HANDLE hHeap,   // handle to the heap of interest 
    DWORD dwFlags,  // bit flags that control heap access during function operation 
    LPCVOID lpMem   // optional pointer to individual memory block to validate 
    );
 
HLOCAL YLocalAlloc(
    UINT uFlags,    // allocation attributes 
    UINT uBytes     // number of bytes to allocate  
   );
HLOCAL YLocalFree(
    HLOCAL hMem     // handle of local memory object 
   );
UINT YLocalFlags(
    HLOCAL hMem     // handle of local memory object 
   );
HLOCAL YLocalReAlloc(
    HLOCAL hMem,    // handle of local memory object 
    UINT uBytes,    // new size of block 
    UINT uFlags     // how to reallocate object 
   );

UINT YLocalSize( 
    HLOCAL hMem     // handle of local memory object 
    );

HANDLE YCreateFile(
    LPCTSTR lpFileName,                         // pointer to name of the file 
    DWORD dwDesiredAccess,                      // access (read-write) mode 
    DWORD dwShareMode,                          // share mode 
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, // pointer to security attributes 
    DWORD dwCreationDistribution,               // how to create 
    DWORD dwFlagsAndAttributes,                 // file attributes 
    HANDLE hTemplateFile                        // handle to file with attributes to copy  
   );

//
// YCreateFileForMapping is deprecated, please use YCreateFile instead
//
HANDLE YCreateFileForMapping(
    LPCTSTR lpFileName,                         // pointer to name of the file 
    DWORD dwDesiredAccess,                      // access (read-write) mode 
    DWORD dwShareMode,                          // share mode 
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, // pointer to security attributes 
    DWORD dwCreationDistribution,               // how to create 
    DWORD dwFlagsAndAttributes,                 // file attributes 
    HANDLE hTemplateFile                        // handle to file with attributes to copy  
   );
DWORD YGetFileAttributes(
    LPCTSTR lpFileName  // address of the name of a file or directory  
   );
BOOL YSetFileAttributes(
    LPCTSTR  lpFileName,        // address of filename 
    DWORD  dwFileAttributes     // address of attributes to set 
   );
BOOL YCreateDirectory(
    LPCTSTR lpPathName,                         // pointer to a directory path string
    LPSECURITY_ATTRIBUTES lpSecurityAttributes  // pointer to a security descriptor 
   );   
BOOL YRemoveDirectory(
    LPCTSTR lpPathName  // address of directory to remove  
   );
BOOL YMoveFile(
    LPCTSTR  lpExistingFileName,    // address of name of the existing file  
    LPCTSTR  lpNewFileName      // address of new name for the file 
   );
BOOL YCopyFile(
    LPCTSTR  lpExistingFileName,// pointer to name of an existing file 
    LPCTSTR  lpNewFileName, // pointer to filename to copy to 
    BOOL  bFailIfExists     // flag for operation if file exists 
   );
BOOL YDeleteFile(
    LPCTSTR lpFileName  // pointer to name of file to delete  
   );   

BOOL YCloseHandle(
    HANDLE hObject  // handle to object to close  
   );
BOOL YFindClose(
    HANDLE hObject  // handle to object to close  
   );

DWORD YSetFilePointer(
    HANDLE hFile,                   // handle of file 
    LONG lDistanceToMove,           // number of bytes to move file pointer 
    PLONG lpDistanceToMoveHigh,     // address of high-order word of distance to move  
    DWORD dwMoveMethod              // how to move 
   );

BOOL YWriteFile(
    HANDLE hFile,                   // handle to file to write to 
    LPCVOID lpBuffer,               // pointer to data to write to file 
    DWORD nNumberOfBytesToWrite,    // number of bytes to write 
    LPDWORD lpNumberOfBytesWritten, // pointer to number of bytes written 
    LPOVERLAPPED lpOverlapped       // pointer to structure needed for overlapped I/O
   );

BOOL YReadFile(
    HANDLE hFile,                   // handle of file to read 
    LPVOID lpBuffer,                // address of buffer that receives data  
    DWORD nNumberOfBytesToRead,     // number of bytes to read 
    LPDWORD lpNumberOfBytesRead,    // address of number of bytes read 
    LPOVERLAPPED lpOverlapped       // address of structure for data 
   );
HANDLE YFindFirstFile(
    LPCTSTR  lpFileName,    // address of name of file to search for  
    LPWIN32_FIND_DATA  lpFindFileData   // address of returned information 
   );
BOOL YFindNextFile(
    HANDLE  hFindFile,  // handle of search  
    LPWIN32_FIND_DATA  lpFindFileData   // address of structure for data on found file  
   );

HTHREAD YCreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // pointer to thread security attributes  
    DWORD dwStackSize,                          // initial thread stack size, in bytes 
    LPTHREAD_START_ROUTINE lpStartAddress,      // pointer to thread function 
    LPVOID lpParameter,                         // argument for new thread 
    DWORD dwCreationFlags,                      // creation flags 
    LPDWORD lpThreadId                          // pointer to returned thread identifier 
   );

DWORD YResumeThread( HTHREAD hThread);
DWORD YSuspendThread( HTHREAD hThread);

BOOL YTerminateThread(
    HTHREAD hThread,
    DWORD dwExitCode
    );

HTHREAD YOpenThread(
    DWORD fdwAccess,
    BOOL fInherit,
    DWORD IDThread
    );

BOOL YSetThreadPriority(
    HTHREAD hThread, // handle to the thread 
    int nPriority   // thread priority level 
   );

int YGetThreadPriority(
    HTHREAD hThread  // handle to the thread 
   );

BOOL YGetThreadTimes(
    HTHREAD hThread,
    LPFILETIME lpCreationTime,
    LPFILETIME lpExitTime,
    LPFILETIME lpKernelTime,
    LPFILETIME lpUserTime
   );

DWORD YWaitForSingleObject(
    HANDLE hHandle,                 // handle of object to wait for 
    DWORD dwMilliseconds            // time-out interval in milliseconds  
   );
BOOL YCreateProcess(
    LPCTSTR lpApplicationName,                  // pointer to name of executable module 
    LPTSTR lpCommandLine,                       // pointer to command line string
    LPSECURITY_ATTRIBUTES lpProcessAttributes,  // pointer to process security attributes 
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // pointer to thread security attributes 
    BOOL bInheritHandles,                       // handle inheritance flag 
    DWORD dwCreationFlags,                      // creation flags 
    LPVOID lpEnvironment,                       // pointer to new environment block 
    LPTSTR lpCurrentDirectory,                  // pointer to current directory name 
    LPSTARTUPINFO lpStartupInfo,                // pointer to STARTUPINFO 
    LPPROCESS_INFORMATION lpProcessInformation  // pointer to PROCESS_INFORMATION  
   );

// TODO: remove this alias and update tests once CeCreateProcess is renamed in mainline
#define YCeCreateProcess YCeCreateProcessEx
BOOL YCeCreateProcessEx (
    LPCWSTR pszImageName,
    LPCWSTR pszCmdLine,
    LPCE_PROCESS_INFORMATION lpCeProcStartupInfo
    );

BOOL YTerminateProcess(
    HPROCESS hProcess,
    DWORD uExitCode
    );

HPROCESS YOpenProcess(
    DWORD fdwAccess,
    BOOL  fInherit,
    DWORD IDProcess
    );
HANDLE YCreateEvent(
    LPSECURITY_ATTRIBUTES lpEventAttributes,    // pointer to security attributes  
    BOOL bManualReset,                          // flag for manual-reset event 
    BOOL bInitialState,                         // flag for initial state 
    LPCTSTR lpName                              // pointer to event-object name  
   );
HANDLE YOpenEvent(
    DWORD dwDesiredAccess,      // access flag 
    BOOL bInheritHandle,        // inherit flag 
    LPCTSTR lpName              // pointer to event-object name  
   );
BOOL YSetEvent(
    HANDLE hEvent   // handle of event object 
   );
BOOL YSetEventData(
        HANDLE hEvent,
        DWORD  dwData
    );
BOOL YPulseEvent(
    HANDLE hEvent   // handle of event object 
   );
BOOL YResetEvent(
    HANDLE hEvent   // handle of event object 
   );

HANDLE YCreateMutex(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,    // pointer to security attributes 
    BOOL bInitialOwner,                         // flag for initial ownership 
    LPCTSTR lpName                              // pointer to mutex-object name  
   );

BOOL YReleaseMutex(
    HANDLE hMutex   // handle of mutex object  
   );

HANDLE YOpenMutex(
    DWORD dwDesiredAccess,  // access flag 
    BOOL bInheritHandle,    // inherit flag 
    LPCTSTR lpName          // pointer to mutex-object name 
   );

DWORD YWaitForMultipleObjects(
    DWORD nCount,               // number of handles in the object handle array 
    CONST HANDLE *lpHandles,    // pointer to the object-handle array 
    BOOL  bWaitAll,             // wait flag 
    DWORD dwMilliseconds        // time-out interval in milliseconds 
   );

#ifdef WMMSG
LRESULT YSendMessage(
    HWND   hWnd,    // handle of destination window
    UINT   uMsg,    // message to send
    WPARAM wParam,  // first message parameter
    LPARAM lParam   // second message parameter
   );
LRESULT YPostMessage(
    HWND   hWnd,    // handle of destination window
    UINT   uMsg,    // message to send
    WPARAM wParam,  // first message parameter
    LPARAM lParam   // second message parameter
   );
#endif //NO_WMMSG

HANDLE YCreateFileMapping(
    HANDLE hFile,                                   // handle to file to map 
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,  // optional security attributes 
    DWORD flProtect,                                // protection for mapping object 
    DWORD dwMaximumSizeHigh,                        // high-order 32 bits of object size  
    DWORD dwMaximumSizeLow,                         // low-order 32 bits of object size  
    LPCTSTR lpName                                  // name of file-mapping object 
   );
LPVOID YMapViewOfFile(
    HANDLE hFileMappingObject,  // file-mapping object to map into address space  
    DWORD dwDesiredAccess,  // access mode 
    DWORD dwFileOffsetHigh, // high-order 32 bits of file offset 
    DWORD dwFileOffsetLow,  // low-order 32 bits of file offset 
    DWORD dwNumberOfBytesToMap  // number of bytes to map 
   );
BOOL YUnmapViewOfFile(
    LPCVOID lpBaseAddress   // address where mapped view begins  
   );

BOOL YFlushViewOfFile( 
    LPCVOID lpBaseAddress,          // start address of byte range to flush 
    DWORD dwNumberOfBytesToFlush    // number of bytes in range 
    ) ;

HPROCESS YGetCurrentProcess(VOID);

DWORD  YGetCurrentProcessId(VOID);

HTHREAD YGetCurrentThread(VOID);

DWORD  YGetCurrentThreadId(VOID);

HINSTANCE YLoadLibrary(
    LPCTSTR lpLibFileName   // String name of module to load
   );

HINSTANCE YLoadLibraryEx(
    LPCTSTR lpLibFileName,  // String of module to load
    HANDLE  hFile,          // Reserved
    DWORD   dwFlags         
   );

HINSTANCE YGetModuleHandle(
    LPCTSTR lpModuleName    // name of module (dll) 
   );

FARPROC YGetProcAddress(
    HMODULE hModule,    // handle to DLL module  
    LPCTSTR lpProcName  // name of function 
   );

BOOL YFreeLibrary( HMODULE hLibModule);

BOOL YGetExitCodeThread(
    HTHREAD hThread,     // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   );
BOOL YGetExitCodeProcess(
    HPROCESS hProcess,        // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   );

LONG YRegCreateKeyEx(
    HKEY hKey,                                      // handle of an open key 
    LPCTSTR lpSubKey,                               // address of subkey name 
    DWORD Reserved,                                 // reserved 
    LPTSTR lpClass,                                 // address of class string 
    DWORD dwOptions,                                // special options flag 
    REGSAM samDesired,                              // desired security access 
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,     // address of key security structure 
    PHKEY phkResult,                                // address of buffer for opened handle  
    LPDWORD lpdwDisposition                         // address of disposition value buffer 
   );
LONG YRegCloseKey(
    HKEY hKey   // handle of key to close  
   );
LONG YRegDeleteKey(
    HKEY hKey,          // handle of open key 
    LPCTSTR lpSubKey    // address of name of subkey to delete 
   );
LONG YRegDeleteValue(
    HKEY hKey,  // handle of key 
    LPCTSTR lpValueName     // address of value name 
   );
LONG YRegEnumKeyEx(
    HKEY hKey,                      // handle of key to enumerate 
    DWORD dwIndex,                  // index of subkey to enumerate 
    LPTSTR lpName,                  // address of buffer for subkey name 
    LPDWORD lpcbName,               // address for size of subkey buffer 
    LPDWORD lpReserved,             // reserved 
    LPTSTR lpClass,                 // address of buffer for class string 
    LPDWORD lpcbClass,              // address for size of class buffer 
    PFILETIME lpftLastWriteTime     // address for time key last written to 
   );
LONG YRegEnumValue(
    HKEY hKey,              // handle of key to query 
    DWORD dwIndex,          // index of value to query 
    LPTSTR lpValueName,     // address of buffer for value string 
    LPDWORD lpcbValueName,  // address for size of value buffer 
    LPDWORD lpReserved,     // reserved 
    LPDWORD lpType,         // address of buffer for type code 
    LPBYTE lpData,          // address of buffer for value data 
    LPDWORD lpcbData        // address for size of data buffer 
   );
LONG YRegOpenKeyEx(
    HKEY hKey,          // handle of open key 
    LPCTSTR lpSubKey,   // address of name of subkey to open 
    DWORD ulOptions,    // reserved 
    REGSAM samDesired,  // security access mask 
    PHKEY phkResult     // address of handle of open key 
   );
LONG YRegQueryValueEx(
    HKEY hKey,              // handle of key to query 
    LPTSTR lpValueName,     // address of name of value to query 
    LPDWORD lpReserved,     // reserved 
    LPDWORD lpType,         // address of buffer for value type 
    LPBYTE lpData,          // address of data buffer 
    LPDWORD lpcbData        // address of data buffer size 
   );
LONG YRegSetValueEx(
    HKEY hKey,              // handle of key to set value for  
    LPCTSTR lpValueName,    // address of value to set 
    DWORD Reserved,         // reserved 
    DWORD dwType,           // flag for value type 
    CONST BYTE *lpData,     // address of value data 
    DWORD cbData            // size of value data 
   );

void YSetLastError( DWORD dwErrCode);
void YSleep( DWORD dwMilliseconds);

BOOL YQueryPerformanceFrequency( 
    LARGE_INTEGER *lpFrequency
    );
BOOL YQueryPerformanceCounter(
    LARGE_INTEGER *lpCount
    );


BOOL YSetLocalTime( const SYSTEMTIME *lpSystemTime );
void YGetLocalTime(LPSYSTEMTIME lpSystemTime);

BOOL YSetSystemTime( const SYSTEMTIME *lpSystemTime );
void YGetSystemTime(LPSYSTEMTIME lpSystemTime);


BOOL YSetTimeZoneInformation(const TIME_ZONE_INFORMATION *lpTimeZoneInformation );
DWORD YGetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation );

BOOL YDuplicateHandle(
    HPROCESS hSourceProcessHandle,
    HANDLE hSourceHandle,
    HPROCESS hTargetProcessHandle,
    LPHANDLE lpTargetHandle,
    DWORD dwDesiredAccess,
    BOOL bInheritHandle,
    DWORD dwOptions
);

HANDLE YCreateAPIHandle( HANDLE hASet, LPVOID pvData);

HANDLE YCreateAPISet( char acName[4], USHORT cFunctions, const PFNVOID *ppfnMethods, const ULONGLONG *pu64Sig);

BOOL YRegisterAPISet( HANDLE hASet, DWORD dwSetID );

BOOL YRegisterDirectMethods( HANDLE hApiSet, const PFNVOID *ppfnDirectMethod );

BOOL YSetAPIErrorHandler( HANDLE hApiSet, PFNAPIERRHANDLER pfnHandler );

BOOL YIsAPIReady( DWORD hAPI );

DWORD YWaitForAPIReady( DWORD dwAPISet, DWORD dwTimeout );

int YQueryAPISetID( char *pName);

HANDLE YLockAPIHandle( HANDLE hApiSet, HANDLE hSrcProc, HANDLE h, LPVOID *ppvObj );

BOOL YUnlockAPIHandle( HANDLE hApiSet, HANDLE hLock );

LPVOID YVerifyAPIHandle( HANDLE hASet, HANDLE h );

HANDLE YCreateWatchDogTimer(
    LPCWSTR pszWatchDogName, 
    DWORD dwPeriod, 
    DWORD dwWait, 
    DWORD dwDfltAction, 
    DWORD dwParam, 
    DWORD dwFlags 
    );

HANDLE YOpenWatchDogTimer(
    LPCWSTR pszWatchDogName, 
     DWORD dwFlags 
    );

BOOL YStartWatchDogTimer(
    HANDLE hWatchDog, 
    DWORD dwFlags 
    );

BOOL YStopWatchDogTimer(
    HANDLE hWatchDog, 
    DWORD dwFlags 
    );

BOOL YRefreshWatchDogTimer(
    HANDLE hWatchDog, 
    DWORD dwFlags 
);

BOOL YPageOutModule(
    HANDLE hModule,
    DWORD dwFlags
);

DWORD YGetModuleFileName( 
    HMODULE hModule,
    LPWSTR lpFilename, 
    DWORD nSize
);

BOOL YCeSetProcessVersion(
    HANDLE hProcess,
    DWORD dwVersion
);

DWORD YGetProcessVersion(
    DWORD ProcessId
);

BOOL YCeGetModuleInfo(
    HANDLE hProcess,
    LPVOID lpBaseAddr,
    DWORD  infoType,
    LPVOID pBuf,
    DWORD  cbBufSize
);

DWORD YTlsAlloc(void); 

DWORD YCeTlsAlloc(PFN_CETLSFREE);

BOOL YTlsFree( 
    DWORD dwTlsIndex
);

BOOL YCeTlsFree( 
    DWORD dwTlsIndex
);

LPVOID YTlsGetValue( 
    DWORD dwTlsIndex
);

BOOL YTlsSetValue( 
    DWORD dwTlsIndex, 
    LPVOID lpTlsValue
);

HANDLE YCreateMsgQueue(
    LPCWSTR lpszName,
    LPMSGQUEUEOPTIONS lpOptions
);

HANDLE YOpenMsgQueue(
  HANDLE hSrcProc,
  HANDLE hMsgQ,
  LPMSGQUEUEOPTIONS lpOptions
);

BOOL YCloseMsgQueue(
    HANDLE hMsgQ
);

BOOL YGetMsgQueueInfo(
  HANDLE hMsgQ,
  LPMSGQUEUEINFO lpInfo
);

BOOL YWriteMsgQueue(
  HANDLE hMsgQ,
  LPVOID lpBuffer,
  DWORD cbDataSize,
  DWORD dwTimeout,
  DWORD dwFlags
);

BOOL YReadMsgQueue(
  HANDLE hMsgQ,
  LPVOID lpBuffer,
  DWORD cbBufferSize,
  LPDWORD lpNumberOfBytesRead,
  DWORD dwTimeout,
  DWORD* pdwFlags
);

HRWLOCK YCeCreateRwLock (
     DWORD dwFlags
    );

BOOL YCeDeleteRwLock(
    HRWLOCK hRWLock
    );

BOOL YCeAcquireRwLock(
    HRWLOCK hLock,
    CERW_TYPE typeLock,
    DWORD dwTimeout
    );

BOOL YCeReleaseRwLock(
    HRWLOCK hLock,
    CERW_TYPE typeLock
    );

BOOL YCeIsRwLockAcquired(
    HRWLOCK     hLock,
    CERW_TYPE typeLock
    );
#ifdef TLHLP // needs to link with toolhelp.lib to compile


HANDLE WINAPI YCreateToolhelp32Snapshot( 
    DWORD dwFlags, 
    DWORD th32ProcessID 
    ); 

BOOL WINAPI YProcess32First( 
    HANDLE hSnapshot, 
    LPPROCESSENTRY32 lppe 
    );

BOOL WINAPI YProcess32Next( 
    HANDLE hSnapshot, 
    LPPROCESSENTRY32 lppe 
    );

BOOL WINAPI YThread32First( 
    HANDLE hSnapshot, 
    LPTHREADENTRY32 lpte 
    ) ;

BOOL WINAPI YThread32Next( 
    HANDLE hSnapshot, 
    LPTHREADENTRY32 lpte 
    );

BOOL WINAPI YModule32First( 
    HANDLE hSnapshot, 
    LPMODULEENTRY32 lpme 
    ) ;

BOOL WINAPI YModule32Next( 
    HANDLE hSnapshot, 
    LPMODULEENTRY32 lpme 
    );

BOOL WINAPI YToolhelp32ReadProcessMemory( 
    DWORD th32ProcessID, 
    LPCVOID lpBaseAddress, 
    LPVOID lpBuffer, 
    DWORD cbRead, 
    LPDWORD lpNumberOfBytesRead 
    );

BOOL WINAPI YHeap32First(
    HANDLE hSnapshot,
    LPHEAPENTRY32 lphe, 
    DWORD th32ProcessID, 
    DWORD th32HeapID 
    );

BOOL WINAPI YHeap32Next(
    HANDLE hSnapshot,
    LPHEAPENTRY32 lphe 
    );

BOOL WINAPI YHeap32ListFirst( 
    HANDLE hSnapshot, 
    LPHEAPLIST32 lphl 
    ); 

BOOL WINAPI YHeap32ListNext(
    HANDLE hSnapshot, 
    LPHEAPLIST32 lphl 
    );

BOOL WINAPI YCloseToolhelp32Snapshot(
    HANDLE hSnapshot
    );

#endif // TLHLP    
#endif // NO_YCALLS

void LogSYSTEMTIME(LPSYSTEMTIME lpSystemTime);
void LogTIME_ZONE_INFORMATION(LPTIME_ZONE_INFORMATION lpTimeZoneInformation);

#define MAKEAPI(x) { (DWORD)x, TEXT(#x) TEXT(" ") }

#define COUNT(x) sizeof(x)/sizeof(APICALLS)

#ifdef UNDER_CE
    #ifndef THREAD_PRIORITY_ERROR_RETURN
    #define THREAD_PRIORITY_ERROR_RETURN  (0xFFFFFFFF)
    #endif
#endif

#endif // __YCALLS_H__
