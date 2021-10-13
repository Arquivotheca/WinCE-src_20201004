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
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    ProMgr.h
Abstract:
    User Mode Driver Processor Manager object.

Notes: 
--*/



#pragma once

#include <CRegEdit.h>
#include <Csync.h>
#include <CRefCon.h>
class UserDriverProcessor;
#include <Refl_imp.hpp>


BOOL IsReflectValid(CReflector *pReflect) ;
BOOL DeleteReflector(CReflector *pReflect) ;
CReflector * CreateReflector(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName,DWORD dwFlags );


class UserDriverProcessor : public CRefObject, public  CLockObject {
public:
    UserDriverProcessor(DWORD dwProcID,LPCTSTR lpProcName, LPCTSTR lpProcVolName,DWORD dwTimeout, UserDriverProcessor * pNextProcessor = NULL);
    virtual ~UserDriverProcessor();
    virtual BOOL Init();
    virtual CReflector * CreateReflector(LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags) ;
    virtual BOOL DeleteReflector(CReflector *pReflector) ;
    DWORD GetProcID() { return m_dwProcID; };
    PROCESS_INFORMATION GetUserDriverPorcessorInfo() { return m_ProcessInformation; };
    BOOL CallAnyOneRefService(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
    BOOL SendIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned) {
        return CeFsIoControl(m_lpProcVolName,
                dwIoControlCode,lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned,NULL);
    }
    UserDriverProcessor * GetNextObject() { return m_pNextProcessor ;};
    void SetNextObject (UserDriverProcessor *pNext) { m_pNextProcessor = pNext; };
protected:
    PROCESS_INFORMATION m_ProcessInformation;
    UserDriverProcessor * m_pNextProcessor;
    DWORD m_dwProcID;
    LPTSTR m_lpProcName;
    LPTSTR m_lpProcVolName;
    DWORD   m_dwTimewout;
// Reflector Manage.
public:
    CReflector * FindReflectorBy(CReflector *pReflector );
protected:
    BOOL        m_fTerminate;
    ReflectorContainer m_ReflectorList;
    BOOL CheckReflectorForEmpty();
    
};

#define UPD_REG_PROCESSOR_NAME_VAL  TEXT("ProcName")
#define UPD_REG_PROCESSOR_NAME_TYPE REG_SZ
#define UDP_REG_PROCESSOR_VOLPREFIX_VAL     TEXT("ProcVolPrefix")
#define UDP_REG_PROCESSOR_VOLPREFIX_TYPE    REG_SZ

#define UDP_REG_PROCESSOR_TIMEOUT_VAL   TEXT("ProcTimeout")
#define UDP_REG_PROCESSOR_TIMEOUT_TYPE  REG_DWORD
#define UDP_REG_PROCESSOR_TIMEOUT_DEFAULT   (20*1000)

#define UDP_REGKEY_PROCESSOR_GROUP_PREFIX   TEXT("ProcGroup")
#define UDP_RANDOM_PROCESSOR_START_OFFSET   0x1000


class UDPContainer: public CLinkedContainer <UserDriverProcessor> {
private:
    DWORD m_dwCurIndex;
public :
    UDPContainer();
    ~UDPContainer();
    BOOL Init();
    UserDriverProcessor * CreateUserProc () ;
    UserDriverProcessor * FindUserProcByID(DWORD dwUserProcGroupID,BOOL fCreateOnNoExit=FALSE);
    UserDriverProcessor * FindUserProcBy(UserDriverProcessor * pUDP) {
        return FoundObjectBy( pUDP ) ;
    }
    UserDriverProcessor * FindUserProcByProcID(DWORD dwProcessId);
    UserDriverProcessor * RemoveUserProc(UserDriverProcessor * pObject);
protected:
    UserDriverProcessor * CreateUserProcByGroupID(DWORD dwUserProcGroupID);
    LPTSTR m_lpProcName;
    LPTSTR m_lpProcVolName;
    DWORD  m_dwProgTimeout;

};

extern UDPContainer *g_pUDPContainer;

inline UserDriverProcessor * CreateUserProc () 
{
    if (g_pUDPContainer)
        return g_pUDPContainer->CreateUserProc ();
    else
        return NULL;
}
inline BOOL DeleteUserProc (UserDriverProcessor * pUDP) 
{
    if (g_pUDPContainer)
        return (g_pUDPContainer->RemoveObjectBy(pUDP)!=NULL) ;
    else
        return FALSE;
}
inline UserDriverProcessor * FindUserProcByID(DWORD dwUserProcGroupID,BOOL fCreateOnNoExit)
{
    if (g_pUDPContainer)
        return g_pUDPContainer->FindUserProcByID(dwUserProcGroupID,fCreateOnNoExit);
    else
        return NULL;
}
inline UserDriverProcessor * FindUserProcBy(UserDriverProcessor * pUDP)
{
    if (g_pUDPContainer)
        return g_pUDPContainer->FindUserProcBy(pUDP);
    else
        return NULL;
}

inline DWORD GetCallerProcessId(void) 
{ 
    return ((DWORD)GetDirectCallerProcessId()); 
};

