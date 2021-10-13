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
#ifndef __YCALLS_H__
#define __YCALLS_H__

#include <windows.h>
#include <bldver.h>

typedef struct _APICALL {
    DWORD dwValue;
    LPTSTR szText;
} APICALLS;

#define ECHO_NEVER  0
#define ECHO_FAIL   1
#define ECHO_ALWAYS 2

#ifndef UNDER_CE
#define CreateFileForMapping    CreateFile
#endif


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

HLOCAL YLocalAlloc(
    UINT uFlags,    // allocation attributes 
    UINT uBytes     // number of bytes to allocate  
   );
HLOCAL YLocalFree(
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

HANDLE YCreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // pointer to thread security attributes  
    DWORD dwStackSize,                          // initial thread stack size, in bytes 
    LPTHREAD_START_ROUTINE lpStartAddress,      // pointer to thread function 
    LPVOID lpParameter,                         // argument for new thread 
    DWORD dwCreationFlags,                      // creation flags 
    LPDWORD lpThreadId                          // pointer to returned thread identifier 
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

HANDLE YCreateEvent(
    LPSECURITY_ATTRIBUTES lpEventAttributes,    // pointer to security attributes  
    BOOL bManualReset,                          // flag for manual-reset event 
    BOOL bInitialState,                         // flag for initial state 
    LPCTSTR lpName                              // pointer to event-object name  
   );

BOOL YSetEvent(
    HANDLE hEvent   // handle of event object 
   );

BOOL YCreateDirectory(
    LPCTSTR lpPathName,                         // pointer to a directory path string
    LPSECURITY_ATTRIBUTES lpSecurityAttributes  // pointer to a security descriptor 
   );   
BOOL YRemoveDirectory(
    LPCTSTR lpPathName  // address of directory to remove  
   );

HANDLE YGetCurrentProcess(VOID);

DWORD  YGetCurrentProcessId(VOID);

HANDLE YGetCurrentThread(VOID);

DWORD  YGetCurrentThreadId(VOID);

HINSTANCE YLoadLibrary(
    LPCTSTR lpLibFileName   // String name of module to load
   );

FARPROC YGetProcAddress(
    HMODULE hModule,    // handle to DLL module  
    LPCTSTR lpProcName  // name of function 
   );

BOOL YGetExitCodeThread(
    HANDLE hThread,     // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   );
BOOL YGetExitCodeProcess(
    HANDLE hProcess,        // handle to the thread 
    LPDWORD lpExitCode  // address to receive termination status 
   );

BOOL YFreeLibrary( HMODULE hLibModule);

#define MAKEAPI(x) { (DWORD)x, TEXT(#x) TEXT(" ") }

#define COUNT(x) sizeof(x)/sizeof(APICALLS)

#endif // __YCALLS_H__
