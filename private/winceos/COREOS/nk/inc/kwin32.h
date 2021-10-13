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
/**     TITLE("Kernel Win32 function declarations")
 *++
 *
 *
 * Module Name:
 *
 *    KWin32.h
 *
 * Abstract:
 *
 *  This file contains the function prototypes and defines used by the kernel part
 * of the Win32 API implemented via the Win32 system API handle.
 *
 *--
 */
#include "kernel.h"
#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif  /* __cplusplus */


BOOL NKKernelLibIoControl(HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
BOOL EXTKernelLibIoControl(HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
BOOL KernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);


void NKSleep(DWORD dwMilliseconds);
void NKSetLastError(DWORD dwErr);
DWORD NKGetLastError(void);

typedef struct _MODULEINFO MODULEINFO, *LPMODULEINFO;

void CaptureContext(void);

VOID NKGetSystemInfo (LPSYSTEM_INFO lpSystemInfo);
DWORD NKGetFSHeapInfo (void);

DWORD NKGetDirectCallerProcessId (void);
DWORD NKGetCallerVMProcessId (void);
DWORD NKGetProcessVersion (DWORD dwId);

DWORD EXTGetCallerProcessId (void);
DWORD EXTOldGetCallerProcess (void);

void NKPowerOnWriteBarrier(void);
void NKCacheRangeFlush (LPVOID pAddr, DWORD dwLength, DWORD dwFlags);

DWORD NKGetRomFileBytes (DWORD type, DWORD count, DWORD pos, LPVOID buffer, DWORD nBytesToRead);
BOOL NKGetRomFileInfo (DWORD type, LPWIN32_FIND_DATA lpfd, DWORD count);

LPBYTE NKCreateLocaleView (LPDWORD pdwViewSize);
LPBYTE EXTCreateLocaleView (LPDWORD pdwViewSize);

BOOL NKIsProcessorFeaturePresent (DWORD dwProcessorFeature);
BOOL NKQueryInstructionSet(DWORD dwInstSet, LPDWORD lpdwOSInstSet);

__int64 NKGetRandomSeed (void);

DWORD NKGetIdleTime (void);

DWORD NKStringCompress (LPBYTE bufin, DWORD lenin, LPBYTE bufout, DWORD  lenout);
DWORD NKStringDecompress (LPBYTE bufin, DWORD  lenin, LPBYTE bufout, DWORD  lenout);
DWORD NKBinaryCompress (LPBYTE bufin, DWORD  lenin, LPBYTE bufout, DWORD  lenout);
DWORD NKBinaryDecompress (LPBYTE bufin, DWORD  lenin, LPBYTE bufout, DWORD  lenout, DWORD  skip);
DWORD ExtBinaryDecompress (const BYTE *bufin, DWORD  lenin, LPBYTE bufout, DWORD  lenout, DWORD  skip);
DWORD NKDecompressBinaryBlock (LPBYTE  lpbSrc, DWORD   cbSrc, LPBYTE  lpbDest, DWORD cbDest);

DWORD NKTlsCall (DWORD type, DWORD slot);
BOOL NKIsBadPtr (DWORD flag, LPBYTE lpv, DWORD len);

BOOL NKQueryPerformanceFrequency (LARGE_INTEGER *lpFrequency);
BOOL NKQueryPerformanceCounter (LARGE_INTEGER *lpPerformanceCount);

BOOL NKSetRealTime (SYSTEMTIME *lpst);

void NKUpdateNLSInfo (DWORD ocp, DWORD acp, DWORD sysloc, DWORD userloc);
BOOL NKWriteRegistryToOEM (DWORD dwFlags, LPBYTE pBuf, DWORD len);
DWORD NKReadRegistryFromOEM (DWORD dwFlags, LPBYTE pBuf, DWORD len);

BOOL NKGetStdioPathW (DWORD id, PWSTR pwszBuf, LPDWORD lpdwLen);
BOOL NKSetStdioPathW (DWORD id, LPCWSTR pwszPath);
BOOL NKSetHardwareWatch (LPVOID vAddr, DWORD flags);
BOOL NKSetPowerHandler (FARPROC pfn, DWORD dwHndlerType);
void NKPowerOffSystem (void);

#ifdef IN_KERNEL
DWORD NKGetCPUInfo (DWORD dwProcessor, DWORD dwInfoType);
BOOL  NKCPUPowerFunc (DWORD dwProcessor, BOOL fOnOff, DWORD dwHint); 
#endif 

LPVOID NKCreateStaticMapping (DWORD dwPhysBase, DWORD dwSize);
THSNAP *NKTHCreateSnapshot (DWORD dwFlags, DWORD dwProcID);

BOOL NKRegisterDbgZones (PMODULE pMod, LPDBGPARAM pdbgparam);
BOOL NKSetDbgZone (DWORD dwProcId, PMODULE pMod, LPVOID BasePtr, DWORD dwZone, LPDBGPARAM pdbgTgt);

int NKwvsprintfW  (LPWSTR lpOut, LPCWSTR lpFmt, va_list lpParms, int maxchars);
void ProfilerHit (UINT addr);
DWORD GetEPC (void);

void NKPSLNotify (DWORD dwFlags, DWORD dwProcId, DWORD dwThrdId);

HLOCAL NKRemoteLocalAlloc(UINT uFlags, UINT uBytes);
HLOCAL NKRemoteLocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags);
UINT   NKRemoteLocalSize(HLOCAL hMem);
HLOCAL NKRemoteLocalFree(HLOCAL hMem);

BOOL RemoteUserHeapInit (HANDLE hCoreDll);

// SH4 speific APIs
BOOL NKSetRAMMode(BOOL bEnable, LPVOID *lplpvAddress, LPDWORD lpLength); 
LPVOID NKSetStoreQueueBase(DWORD dwPhysPage); 

BOOL NKWaitForDebugEvent (LPDEBUG_EVENT lpDebugEvent, DWORD dwMilliseconds); 
BOOL NKContinueDebugEvent(DWORD dwProcessId, DWORD dwThreadId, DWORD dwContinueStatus); 
void NKDebugNotify(DWORD dwFlags, DWORD data);
BOOL NKDebugSetProcessKillOnExit(BOOL fKillOnExit);
BOOL NKDebugActiveProcess(DWORD dwProcessId);
BOOL NKDebugActiveProcessStop(DWORD dwProcessId);


#ifdef __cplusplus
};
#endif  /* __cplusplus */

