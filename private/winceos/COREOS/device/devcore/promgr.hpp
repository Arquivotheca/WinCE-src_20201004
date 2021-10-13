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
#include <sid.h>


BOOL IsReflectValid(CReflector *pReflect) ;
BOOL DeleteReflector(CReflector *pReflect) ;
CReflector * CreateReflector(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName,DWORD dwFlags );


class UserDriverProcessor : public CRefObject, public  CLockObject {
public:

    //constructor
    UserDriverProcessor(
        DWORD dwProcID,
        LPCTSTR lpProcName,
        LPCTSTR lpProcVolName,
        LPCTSTR lpAccountId,
        DWORD dwTimeout,
        UserDriverProcessor * pNextProcessor = NULL
    );
    //destructor
    virtual ~UserDriverProcessor();
    
    virtual BOOL Init();

    virtual CReflector * CreateReflector(LPCTSTR lpszDeviceKey,LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags) ;
    virtual BOOL DeleteReflector(CReflector *pReflector) ;
    DWORD GetProcID() const;
    PROCESS_INFORMATION GetUserDriverProcessorInfo() const;
    BOOL CallAnyOneRefService(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
    BOOL SendIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned);
    UserDriverProcessor * GetNextObject();
    void SetNextObject (UserDriverProcessor *pNext);
    LPTSTR GetAccountId() const;
    
    
protected:
    PROCESS_INFORMATION m_ProcessInformation;
    UserDriverProcessor * m_pNextProcessor;
    DWORD m_dwProcID;
    LPTSTR m_lpProcName;
    LPTSTR m_lpProcVolName;
    LPTSTR m_lpAccountId;
    DWORD   m_dwTimeOut;

    
// Reflector Manage.
public:
    CReflector * FindReflectorBy(CReflector *pReflector );
protected:
    BOOL        m_fTerminate;
    ReflectorContainer m_ReflectorList;
    BOOL CheckReflectorForEmpty();

private:
    virtual BOOL WaitForInitComplete();
    
    HANDLE m_hUDevMgr;
    HANDLE m_hUDevMgrHndAvailEvent; 

    BOOL m_fAllocedAccountId;
};

#define UPD_REG_PROCESSOR_NAME_VAL  TEXT("ProcName")
#define UPD_REG_PROCESSOR_NAME_TYPE REG_SZ
#define UDP_REG_PROCESSOR_VOLPREFIX_VAL     TEXT("ProcVolPrefix")
#define UDP_REG_PROCESSOR_VOLPREFIX_TYPE    REG_SZ

#define UDP_REG_PROCESSOR_TIMEOUT_VAL   TEXT("ProcTimeout")
#define UDP_REG_PROCESSOR_TIMEOUT_TYPE  REG_DWORD
#define UDP_REG_PROCESSOR_TIMEOUT_DEFAULT   (180*1000)

#define UDP_REGKEY_PROCESSOR_GROUP_PREFIX   TEXT("ProcGroup")
#define UDP_RANDOM_PROCESSOR_START_OFFSET   0x1000

#define UDP_REGKEY_PROCESSOR_DEFAULT   TEXT("default")

class UDPContainer: public CLinkedContainer <UserDriverProcessor>
{

public :
    UDPContainer();
    ~UDPContainer();
    BOOL Init();
    UserDriverProcessor * CreateUserProc(
        LPCTSTR lpAccountId
    ) ;
    UserDriverProcessor * FindUserProcByID(
        __in_opt DWORD dwUserProcGroupID,
        __in_opt LPCTSTR pszAccountId,
        BOOL fCreateOnNoExit=FALSE
    );
    UserDriverProcessor * FindUserProcBy(UserDriverProcessor * pUDP);
    UserDriverProcessor * FindUserProcByProcID(DWORD dwProcessId);
    UserDriverProcessor * FindUserProcByAccount(__in_z LPCTSTR pszDriverSidFriendly, BOOL fIsService);
    UserDriverProcessor * RemoveUserProc(UserDriverProcessor * pObject);
    UserDriverProcessor * GetUDPPendingInit(DWORD dwProcessId);  
    
protected:
    LPTSTR m_lpProcName;
    LPTSTR m_lpProcVolName;
    DWORD  m_dwProgTimeout;
    LIST_ENTRY m_pDefaultProcList;

private:
    UserDriverProcessor * CreateUserProcByGroupID(DWORD dwUserProcGroupID, LPCTSTR lpAccountId);
    void SetUDPPendingInit(UserDriverProcessor* udp);
    
    DWORD m_dwCurIndex;
    UserDriverProcessor* m_pUDPPendingInit;
    CLockObject m_pendingInitLock;
};

extern UDPContainer *g_pUDPContainer;

inline UserDriverProcessor * CreateUserProc(LPCTSTR lpAccountId) 
{
    if (g_pUDPContainer)
        return g_pUDPContainer->CreateUserProc(lpAccountId);
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
inline UserDriverProcessor * FindUserProcByID(
    DWORD dwUserProcGroupID, 
    LPCTSTR lpAccountId,
    BOOL fCreateOnNoExit)
{
    if (g_pUDPContainer)
        return g_pUDPContainer->FindUserProcByID(
                                    dwUserProcGroupID,
                                    lpAccountId,
                                    fCreateOnNoExit);
    else
        return NULL;
}
inline UserDriverProcessor * FindUserProcByAccount(
    __in_z LPCTSTR lpAccountId,
    BOOL fIsService)
{
    if (g_pUDPContainer)
        return g_pUDPContainer->FindUserProcByAccount(
                                    lpAccountId, 
                                    fIsService
                                    );
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

