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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    kitlpriv.h
    
Abstract:  
    Private interface to KITL library
    
Functions:

Notes: 

--*/
#ifndef __KITLPRIV_H__
#define __KITLPRIV_H__

typedef struct _KITLPRIV {
    PPROCESS pprcNK;
    struct KDataStruct *pKData;
    LPDWORD pdwKeys;
    volatile SPINLOCK **ppKitlSpinLock;
    
    LPVOID (* pfnAllocMem) (ulong poolnum);
    void (* pfnFreeMem) (LPVOID pMem, ulong poolnum);
    
    LPVOID (* pfnVMAlloc) (PPROCESS pprc, LPVOID lpvaddr, DWORD cbSize, DWORD fAllocType, DWORD fProtect);
    BOOL (* pfnVMFreeAndRelease) (PPROCESS pprc, LPVOID lpvAddr, DWORD cbSize);

    PHDATA (* pfnLockHandleData) (HANDLE h, PPROCESS pprc);
    BOOL (* pfnUnlockHandleData) (PHDATA phd);

    HANDLE (* pfnNKCreateEvent) (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName);
    BOOL (* pfnHNDLCloseHandle) (PPROCESS pprc, HANDLE h);

    DWORD (* pfnDoWaitForObjects) (DWORD cObjects, PHDATA * phds, DWORD dwTimeout);
    BOOL (* pfnEVNTModify) (PEVENT lpe, DWORD type);

    BOOL (* pfnIntrInitialize) (DWORD idInt, HANDLE hEvent, LPVOID pvData, DWORD cbData);
    
    int (* pfnKCall) (PKFN pfn, ...);

    // thread function (to create IST)
    HANDLE (* pfnCreateKernelThread) (LPTHREAD_START_ROUTINE lpStartAddr, LPVOID lpvThreadParm, WORD prio, DWORD dwFlags);

    BOOL (* pfnExtKITLIoctl) (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

    PFN_SPINLOCKFUNC pfnInitSpinLock;
    PFN_SPINLOCKFUNC pfnAcquireSpinLock;
    PFN_SPINLOCKFUNC pfnReleaseSpinLock;

    PFNVOID pfnStopAllOtherCPUs;
    PFNVOID pfnResumeAllOtherCPUs;

#ifdef ARM
    BOOL (* pfnInSysCall) (void);
#endif
    
    void  (* pfnIntrDone) (DWORD idInt);
    void  (* pfnIntrDisable) (DWORD idInt);
    void  (* pfnIntrMask) (DWORD idInt, BOOL fDisable);
} KITLPRIV, *PKITLPRIV;

#endif // __KITLPRIV_H__

