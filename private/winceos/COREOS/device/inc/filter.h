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

Filter.h

Abstract:

Device loader structures and defines

Notes: 


--*/

#pragma once

#include <drfilter.h>
#include <devmgrp.h>
#ifdef __cplusplus
extern "C" {
#endif

// Implementation
DWORD   DriverInit(fsdev_t * pDev, DWORD dwContext,LPVOID lpParam);
BOOL    DriverPreDeinit(fsdev_t * pDev,DWORD dwContext);
BOOL    FDriverDeinit(fsdev_t * pDev, DWORD dwContext);
DWORD   DriverOpen(fsdev_t * pDev,DWORD dwConext,DWORD dwAccessCode,DWORD SharedMode);
BOOL    DriverPreClose(fsdev_t * pDev, DWORD dwOpenCont);
BOOL    DriverClose(fsdev_t * pDev,DWORD dwOpenCont);
DWORD   DriverRead(fsdev_t * pDev, DWORD dwOpenCont,LPVOID pInBuf,DWORD cbSize,HANDLE hAsyncRef);
DWORD   DriverWrite(fsdev_t * pDev, DWORD dwOpenCont,LPCVOID pOutBuf,DWORD cbSize,HANDLE hAsyncRef);    
DWORD   DriverSeek(fsdev_t * pDev, DWORD dwOpenCont,long lDistanceToMove,DWORD dwMoveMethod) ;    
BOOL    DriverControl(fsdev_t * pDev,DWORD dwOpenCont,DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
          PDWORD pdwActualOut,HANDLE hAsyncRef);
void    DriverPowerup(fsdev_t * pDev, DWORD dwConext);    
void    DriverPowerdn(fsdev_t * pDev, DWORD dwConext);
BOOL    DriverCancelIo(fsdev_t * pDev, DWORD dwOpenCont, HANDLE hAsyncHandle);
    
PVOID   CreateDriverFilter(fsdev_t * pDev, LPCTSTR lpFilter);


#ifdef __cplusplus
};
#endif


#ifdef __cplusplus
#include <csync.h>
class FilterFolder;
class DriverFilterMgr :  public fsdev_t,protected CLockObject  {
protected:
    PDRIVER_FILTER  m_pFilter;
    DRIVER_FILTER   m_DummyFilter;
    FilterFolder *  m_pFilterFolder;
public: 
    DriverFilterMgr();
    virtual ~DriverFilterMgr();
    FilterFolder * CreateDriverFilter(LPCTSTR lpFilter);
    DWORD InitDriverFilters(LPCTSTR lpDeviceRegPath);
    DWORD   DriverInit(DWORD dwContext,LPVOID lpParam) {
        return m_pFilter? m_pFilter->fnInit(dwContext, lpParam, m_pFilter): this->fnInit(dwContext,lpParam);
    }
    BOOL    DriverPreDeinit(DWORD dwContext) {
        return m_pFilter? m_pFilter->fnPreDeinit(dwContext, m_pFilter): this->fnPreDeinit(dwContext);
    }
    BOOL    DriverDeinit(DWORD dwContext) {
        return m_pFilter? m_pFilter->fnDeinit(dwContext,m_pFilter): this->fnDeinit(dwContext);
    }
    DWORD    DriverOpen(DWORD dwConext,DWORD dwAccessCode,DWORD SharedMode) {
        return m_pFilter? m_pFilter->fnOpen(dwConext,dwAccessCode, SharedMode, m_pFilter): this->fnOpen(dwConext,dwAccessCode, SharedMode);
    }
    BOOL    DriverPreClose(DWORD dwOpenCont) {
        return m_pFilter? m_pFilter->fnPreClose(dwOpenCont,m_pFilter): this->fnPreClose(dwOpenCont);
    }
    BOOL    DriverClose(DWORD dwOpenCont) {
        return m_pFilter? m_pFilter->fnClose(dwOpenCont,m_pFilter): this->fnClose(dwOpenCont);
    }
    DWORD   DriverRead(DWORD dwOpenCont,LPVOID pInBuf,DWORD cbSize,HANDLE hAsyncRef) {
        return m_pFilter? m_pFilter->fnRead(dwOpenCont,pInBuf, cbSize,hAsyncRef, m_pFilter): this->fnRead(dwOpenCont,pInBuf, cbSize,hAsyncRef);
    }
    DWORD   DriverWrite(DWORD dwOpenCont,LPCVOID pOutBuf,DWORD cbSize,HANDLE hAsyncRef) {
        return m_pFilter? m_pFilter->fnWrite(dwOpenCont,pOutBuf, cbSize,hAsyncRef, m_pFilter): this->fnWrite(dwOpenCont,pOutBuf, cbSize,hAsyncRef);
    }
    DWORD   DriverSeek(DWORD dwOpenCont,long lDistanceToMove,DWORD dwMoveMethod){
        return m_pFilter? m_pFilter->fnSeek(dwOpenCont,lDistanceToMove,dwMoveMethod,m_pFilter): this->fnSeek(dwOpenCont,lDistanceToMove,dwMoveMethod);
    }
    BOOL    DriverControl(DWORD dwOpenCont,DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut,HANDLE hAsyncRef) {
        return m_pFilter? m_pFilter->fnControl(dwOpenCont,dwCode, pBufIn, dwLenIn, pBufOut, dwLenOut ,pdwActualOut, hAsyncRef, m_pFilter): 
            this->fnControl(dwOpenCont,dwCode, pBufIn, dwLenIn, pBufOut, dwLenOut ,pdwActualOut, hAsyncRef);
         
    }
    void    DriverPowerup(DWORD dwConext) {
        m_pFilter? m_pFilter->fnPowerup(dwConext,m_pFilter) : this->fnPowerup(dwConext);
    }
    void    DriverPowerdn(DWORD dwConext) {
        m_pFilter? m_pFilter->fnPowerdn(dwConext,m_pFilter) : this->fnPowerdn(dwConext);
    }
    BOOL    DriverCancelIo(DWORD dwOpenCont, HANDLE hAsyncHandle) {
        return m_pFilter? m_pFilter->fnCancelIo(dwOpenCont, hAsyncHandle,m_pFilter) : this->fnCancelIo(dwOpenCont,hAsyncHandle);
    }
    
};
#endif




