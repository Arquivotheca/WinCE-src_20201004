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
    ProMgr.cpp
Abstract:
    Usb Mode Driver Processor Manager object.

Notes: 
--*/

#include <windows.h>
#include <devload.h>
#include <reflector.h>
#include "ReflServ.hpp"

const PFNVOID ReflServApiMethods[] = {
    (PFNVOID)REFL_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)REFL_DevDeviceIoControl,
};

#define NUM_REFL_SERV_APIS (sizeof(ReflServApiMethods)/sizeof(ReflServApiMethods[0]))

const ULONGLONG ReflServApiSigs[NUM_REFL_SERV_APIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),   // ReadFile
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),   // WriteFile
    FNSIG2(DW,O_PDW),                   // GetFileSize
    FNSIG4(DW,DW,O_PDW,DW),             // SetFilePointer
    FNSIG2(DW,O_PDW),                   // GetDeviceInformationByFileHandle
    FNSIG1(DW),                         // FlushFileBuffers
    FNSIG4(DW,O_PDW,O_PDW,O_PDW),       // GetFileTime
    FNSIG4(DW,IO_PDW,IO_PDW,IO_PDW),    // SetFileTime
    FNSIG1(DW),                         // SetEndOfFile,
    FNSIG8(DW, DW, I_PTR, DW, O_PTR, DW, O_PDW, IO_PDW), // DeviceIoControl
};

UDServiceContainer::UDServiceContainer()
{

    m_hDevFileApiHandle = INVALID_HANDLE_VALUE;
};

UDServiceContainer::~UDServiceContainer()
{
    if (m_hDevFileApiHandle!=INVALID_HANDLE_VALUE)
        CloseHandle(m_hDevFileApiHandle);
};
BOOL UDServiceContainer::Init()
{
    BOOL bReturn = FALSE;
    m_hDevFileApiHandle = CreateAPISet("REFL", NUM_REFL_SERV_APIS, ReflServApiMethods, ReflServApiSigs );
    if (m_hDevFileApiHandle!=INVALID_HANDLE_VALUE)
        bReturn =RegisterAPISet(m_hDevFileApiHandle, HT_FILE | REGISTER_APISET_TYPE);
    ASSERT(m_hDevFileApiHandle!=INVALID_HANDLE_VALUE && bReturn) ;
    return bReturn;
};

BOOL 
REFL_DevCloseFileHandle(UserDriverService *pReflService)
{
    BOOL bReturn = FALSE;
    if (pReflService && g_pServiceContainer ) {
        bReturn = (g_pServiceContainer->RemoveObjectBy(pReflService)!=NULL);
    }
    ASSERT(bReturn);
    return bReturn;
        
    
};
BOOL 
REFL_DevDeviceIoControl(UserDriverService *pReflService, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) 
{
    BOOL bReturn = FALSE;
    if (pReflService && g_pServiceContainer ) {
        pReflService = g_pServiceContainer->FoundObjectBy(pReflService);
        if (pReflService) {
            bReturn = pReflService->ReflService(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
            pReflService->DeRef();
        }
    }
    return bReturn;
};
HANDLE REFL_CreateDeviceServiceHandle(CReflector * pReflect)
{
    HANDLE hReturn = NULL;
    if (g_pServiceContainer) {
        UserDriverService * pNewService = new UserDriverService(pReflect);
        if (pNewService && pNewService->Init()) {
            hReturn = pNewService->CreateAPIHandle(g_pServiceContainer->GetApiSetHandle());
            if (hReturn!=NULL && g_pServiceContainer->InsertObjectBy(pNewService)==NULL ) { // Fail to insert.
                CloseHandle(hReturn);
                hReturn = NULL;
            }
        }
        if (hReturn == NULL && pNewService!=NULL) {
            delete pNewService;
        }
    }
    ASSERT(hReturn!=NULL);
    return hReturn;
}

UserDriverService::UserDriverService (CReflector * pReflect,UserDriverService * pNext)
:   m_pReflect(pReflect)
,   m_pNextDriverService(pNext)
{
    if (m_pReflect!=NULL)
        m_pReflect->AddRef();
}
UserDriverService::~UserDriverService()
{
    if (m_pReflect!=NULL)
        m_pReflect->DeRef();
}
BOOL UserDriverService::Init()
{
    return (m_pReflect!=NULL) ;
}
HANDLE UserDriverService::CreateAPIHandle(HANDLE hAPISetHandle)
{
    return( ::CreateAPIHandle(hAPISetHandle, this));
}

