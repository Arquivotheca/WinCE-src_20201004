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
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    ReflServ.h
Abstract:
    User Mode Driver service object.

Notes: 
--*/



#pragma once

#include <Csync.h>
#include <CRefCon.h>
#include "Refl_imp.hpp"
class UserDriverService : public CRefObject {
public:
    UserDriverService (CReflector * pReflect,UserDriverService * pNext = NULL);
    virtual ~UserDriverService();
    virtual BOOL Init();
    virtual HANDLE CreateAPIHandle(HANDLE hAPISetHandle);
    BOOL ReflService( DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
        ASSERT(m_pReflect!=NULL);
        return m_pReflect->ReflService(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }
    UserDriverService * GetNextObject() { return m_pNextDriverService ;};
    void SetNextObject (UserDriverService * pNext) { m_pNextDriverService = pNext; };
protected:
    CReflector * m_pReflect;
    UserDriverService * m_pNextDriverService;
};

class UDServiceContainer: public CLinkedContainer <UserDriverService> {
public:
    UDServiceContainer();
    ~UDServiceContainer();
    BOOL Init();
    HANDLE GetApiSetHandle() { return m_hDevFileApiHandle; };
protected:
    HANDLE m_hDevFileApiHandle;
};

BOOL REFL_DevCloseFileHandle(UserDriverService *pReflService);
BOOL REFL_DevDeviceIoControl(UserDriverService *pReflService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) ;
HANDLE REFL_CreateDeviceServiceHandle(CReflector * pReflect) ;

extern UDServiceContainer * g_pServiceContainer;

