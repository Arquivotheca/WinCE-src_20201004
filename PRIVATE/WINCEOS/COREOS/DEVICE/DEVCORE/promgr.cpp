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
    ProMgr.cpp
Abstract:
    User Mode Driver Processor Manager object.

Notes: 
--*/

#include <windows.h>
#include <devload.h>
#include <DevZones.h>
#include "ProMgr.hpp"
#include "ReflServ.hpp"

UDPContainer *g_pUDPContainer = NULL;
UDServiceContainer * g_pServiceContainer = NULL;

extern "C" BOOL InitUserProcMgr()
{
    if ( !g_pUDPContainer ) {
        g_pUDPContainer = new UDPContainer();
        if (g_pUDPContainer!=NULL && !g_pUDPContainer->Init()) {
            delete g_pUDPContainer;
            g_pUDPContainer = NULL;
        }
    }
    ASSERT(g_pUDPContainer!=NULL);
    if (!g_pServiceContainer) {
        g_pServiceContainer = new UDServiceContainer();
        if (g_pServiceContainer!=NULL && !g_pServiceContainer->Init()) {
            delete g_pServiceContainer;
            g_pServiceContainer = NULL;
        }
        
    }
    ASSERT(g_pServiceContainer!=NULL) ;
    return (g_pUDPContainer!=NULL && g_pServiceContainer!=NULL);
}
extern "C" BOOL DeiniUserProcMgr()
{
    if (g_pUDPContainer!=NULL) {
        delete g_pUDPContainer;
        g_pUDPContainer = NULL;
    }
    if (g_pServiceContainer!=NULL) {
        delete g_pServiceContainer;
        g_pServiceContainer = NULL;
    }
    return TRUE;
}
extern "C" BOOL DM_REL_UDriverProcIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL bRet = FALSE;
    SetLastError (ERROR_INVALID_PARAMETER);
    if (g_pUDPContainer) {
        UserDriverProcessor *  pUdriverProc = g_pUDPContainer->FindUserProcByProcID(GetCallerProcessId());
        if (pUdriverProc) {
            bRet = pUdriverProc->CallAnyOneRefService(dwIoControlCode, lpInBuf,nInBufSize, lpOutBuf,nOutBufSize, lpBytesReturned);
            pUdriverProc->DeRef();
        }
    }
    return bRet;
    
}


UDPContainer::UDPContainer()
{
    m_dwCurIndex = UDP_RANDOM_PROCESSOR_START_OFFSET ;
    m_lpProcName = NULL;
    DWORD dwLen = 0; 
    DWORD dwType;
    
    CRegistryEdit regKey(HKEY_LOCAL_MACHINE , DEVLOAD_DRIVERS_KEY );

    if (regKey.IsKeyOpened() && regKey.RegQueryValueEx(UPD_REG_PROCESSOR_NAME_VAL,&dwType,NULL,&dwLen) && dwType == UPD_REG_PROCESSOR_NAME_TYPE) {
        dwLen = min(dwLen / sizeof(TCHAR) + 1, MAX_PATH) ;
        m_lpProcName = new TCHAR [dwLen];
        if (m_lpProcName && !regKey.GetRegValue(UPD_REG_PROCESSOR_NAME_VAL,(LPBYTE)m_lpProcName,dwLen*sizeof(TCHAR))) {
            delete [] m_lpProcName;
            m_lpProcName = NULL;
        }
        if (m_lpProcName)
            m_lpProcName[dwLen-1] = 0;
    }
    m_lpProcVolName = NULL;
    if (regKey.IsKeyOpened() && regKey.RegQueryValueEx(UDP_REG_PROCESSOR_VOLPREFIX_VAL,&dwType,NULL,&dwLen) && dwType == UDP_REG_PROCESSOR_VOLPREFIX_TYPE) {
        dwLen = min(dwLen / sizeof(TCHAR) + 1, MAX_PATH) ;
        m_lpProcVolName = new TCHAR [dwLen];
        if (m_lpProcVolName && !regKey.GetRegValue(UDP_REG_PROCESSOR_VOLPREFIX_VAL,(LPBYTE)m_lpProcVolName,dwLen*sizeof(TCHAR))) {
            delete [] m_lpProcVolName;
            m_lpProcVolName = NULL;
        }
        if (m_lpProcVolName)
            m_lpProcVolName[dwLen-1] = 0;
    }
    if (!(regKey.IsKeyOpened() && regKey.GetRegValue(UDP_REG_PROCESSOR_TIMEOUT_VAL,(LPBYTE)&m_dwProgTimeout,sizeof(DWORD)))) { // If failed we use default.
        m_dwProgTimeout = UDP_REG_PROCESSOR_TIMEOUT_DEFAULT ;
    }

}
UDPContainer::~UDPContainer()
{
    if (m_lpProcName)
        delete [] m_lpProcName;
    if (m_lpProcVolName)
        delete [] m_lpProcVolName;
}
BOOL UDPContainer::Init()
{
    ASSERT(m_lpProcVolName!=NULL && m_lpProcName!=NULL);
    return (m_lpProcVolName!=NULL && m_lpProcName!=NULL);
}
UserDriverProcessor * UDPContainer::CreateUserProcByGroupID(DWORD dwUserProcGroupID)
{
    TCHAR lpGroupSubKeyPath[MAX_PATH] ;
    LPCTSTR lpProcName = m_lpProcName;
    LPCTSTR lpProcVolName = m_lpProcVolName;
    TCHAR localProcName[MAX_PATH];
    TCHAR localProcVolume[MAX_PATH];
    DWORD dwProgTimeout= m_dwProgTimeout ;
    
    CRegistryEdit regKey(HKEY_LOCAL_MACHINE , DEVLOAD_DRIVERS_KEY );
    if (regKey.IsKeyOpened() && SUCCEEDED(StringCchPrintf(lpGroupSubKeyPath,MAX_PATH,TEXT("%s_%04x"),UDP_REGKEY_PROCESSOR_GROUP_PREFIX,dwUserProcGroupID))) {
        CRegistryEdit groupSubKey(regKey.GetHKey(),lpGroupSubKeyPath);
        if (groupSubKey.IsKeyOpened()) {
            DWORD dwType;
            DWORD dwLen = sizeof(localProcName);
            if (groupSubKey.RegQueryValueEx(UPD_REG_PROCESSOR_NAME_VAL,&dwType,(LPBYTE)localProcName,&dwLen) && dwType == UPD_REG_PROCESSOR_NAME_TYPE) {
                localProcName[MAX_PATH-1] = 0 ; // Force to terminate if it is not.
                lpProcName = localProcName;
            }
            dwLen = sizeof(localProcVolume) ; 
            if (groupSubKey.RegQueryValueEx(UDP_REG_PROCESSOR_VOLPREFIX_VAL,&dwType,(LPBYTE)localProcVolume,&dwLen) && dwType == UDP_REG_PROCESSOR_VOLPREFIX_TYPE) {
                localProcVolume[MAX_PATH-1] = 0 ; // Force to terminate if it is not.
                lpProcVolName = localProcVolume;
            }
            DWORD dwTimeout;
            if (groupSubKey.GetRegValue(UDP_REG_PROCESSOR_TIMEOUT_VAL,(LPBYTE)&dwTimeout,sizeof(DWORD))) {
                dwProgTimeout = dwTimeout ;
            }
        }
    }
    Lock();
    UserDriverProcessor * pNewProc = new UserDriverProcessor(dwUserProcGroupID,lpProcName,lpProcVolName,dwProgTimeout);
    if (pNewProc!=NULL && !pNewProc->Init()) { // Init Fails.
        delete pNewProc;
        pNewProc = NULL;
    }
    if (pNewProc) {
        if (InsertObjectBy(pNewProc)==NULL) { // Something Really Bad.
            ASSERT(FALSE);
            delete pNewProc;
            pNewProc = NULL;
        }
    }
    Unlock();
    return pNewProc;
};
UserDriverProcessor * UDPContainer::CreateUserProc () 
{
    UserDriverProcessor * pReturn = NULL;
    Lock();
    do {
        pReturn = NULL;
        
        m_dwCurIndex++;
        if (m_dwCurIndex<UDP_RANDOM_PROCESSOR_START_OFFSET)
            m_dwCurIndex = UDP_RANDOM_PROCESSOR_START_OFFSET;
        
        UserDriverProcessor *  pCur = m_rgLinkList;
        while (pCur) {
            if (pCur->GetProcID() == m_dwCurIndex ) {
                pReturn = pCur;
                break;
            }
            else {
                pCur = pCur->GetNextObject();
            }
        }
            
    } while (pReturn != NULL) ;
    pReturn = CreateUserProcByGroupID(m_dwCurIndex);
    Unlock();
    ASSERT(pReturn);
    return pReturn;
}
UserDriverProcessor * UDPContainer::FindUserProcByID(DWORD dwUserProcGroupID,BOOL fCreateOnNoExit)
{
    UserDriverProcessor * pReturn = NULL;
    if (dwUserProcGroupID < UDP_RANDOM_PROCESSOR_START_OFFSET && dwUserProcGroupID != 0) {
        Lock();
        UserDriverProcessor *  pCur = m_rgLinkList;
        while (pCur) {
            if (pCur->GetProcID() == dwUserProcGroupID ) {
                pReturn = pCur;
                break;
            }
            else {
                pCur = pCur->GetNextObject();
            }
        }
        if (pReturn) {
            pReturn->AddRef();
        }
        if (pReturn == NULL && fCreateOnNoExit) {
            UserDriverProcessor * pNewProc = CreateUserProcByGroupID(dwUserProcGroupID);
            if (pNewProc) { // This is newly created. So it should succeeded.
                pReturn = FindUserProcBy(pNewProc);
                ASSERT(pReturn);
            }
        }
        Unlock();
    }
    else {
        UserDriverProcessor * pNewProc = CreateUserProc () ;
        if (pNewProc) {
            pReturn = FindUserProcBy(pNewProc);
            ASSERT(pReturn);
        }
    }
    ASSERT(pReturn);
    return pReturn;
}

UserDriverProcessor * UDPContainer::FindUserProcByProcID(DWORD dwProcessId)
{
    UserDriverProcessor * pReturn = NULL;
    Lock();
    UserDriverProcessor *  pCur = m_rgLinkList;
    while (pCur) {
        if (pCur->GetUserDriverPorcessorInfo().dwProcessId == dwProcessId ) {
            pReturn = pCur;
            break;
        }
        else {
            pCur = pCur->GetNextObject();
        }
    }
    if (pReturn) {
        pReturn->AddRef();
    }
    Unlock();
    ASSERT(pReturn);
    return pReturn;
}

UserDriverProcessor::UserDriverProcessor(DWORD dwProcID,LPCTSTR lpProcName, LPCTSTR lpProcVolName,DWORD dwTimeout,UserDriverProcessor * pNextProcessor)
:   m_dwProcID(dwProcID)
,   m_pNextProcessor(pNextProcessor)
,   m_dwTimewout(dwTimeout)
{
    m_lpProcName = NULL;
    m_lpProcVolName = NULL;
    m_ProcessInformation.hProcess = NULL;
    m_ProcessInformation.hThread = NULL ;
    m_ProcessInformation.dwProcessId  = 0;
    m_ProcessInformation.dwThreadId = 0 ;
    
    size_t dwLen ;
    // Create Processor Name
    if (lpProcName && SUCCEEDED(StringCchLength(lpProcName,MAX_PATH,&dwLen))) {
        m_lpProcName = new TCHAR [dwLen + 1];
        if (m_lpProcName && !SUCCEEDED(StringCchCopy(m_lpProcName,dwLen+1,lpProcName))) {
            delete[] m_lpProcName;
            m_lpProcName = NULL;
        }
    }
    // Create Volume Name.
    TCHAR strVol[MAX_PATH];
    if (lpProcVolName && SUCCEEDED(StringCchPrintf(strVol,MAX_PATH,TEXT("%s_%04x"),lpProcVolName,dwProcID)) &&
            SUCCEEDED(StringCchLength(strVol,MAX_PATH,&dwLen))) {
        m_lpProcVolName = new TCHAR[dwLen + 1];
        if (m_lpProcVolName && !SUCCEEDED(StringCchCopy(m_lpProcVolName,dwLen+1 ,strVol))) {
            delete [] m_lpProcVolName;
            m_lpProcVolName = NULL;
        }
    }
    m_fTerminate = FALSE;
}
UserDriverProcessor::~UserDriverProcessor()
{
    Lock();
    SendIoControl(IOCTL_USERPROCESSOR_EXIT,NULL,0,NULL,0,NULL);
    if (m_lpProcName)
        delete m_lpProcName;
    if (m_lpProcVolName)
        delete m_lpProcVolName;
    if ( m_ProcessInformation.hProcess != NULL)
        CloseHandle(m_ProcessInformation.hProcess);
    if (m_ProcessInformation.hThread != NULL)
        CloseHandle(m_ProcessInformation.hThread);
    Unlock();
}
#define TIMOUT_INTERVAL 10
BOOL UserDriverProcessor::Init()
{
    BOOL bReturn = FALSE;
    Lock();
    if (m_lpProcName!=NULL && m_lpProcVolName!=NULL) {
        // We have to lauch processor with the value name
        bReturn = CreateProcess( m_lpProcName, m_lpProcVolName, NULL, NULL, FALSE, 0, NULL, NULL, NULL,&m_ProcessInformation);
        if (bReturn)  {
            DWORD dwWaitTicks = m_dwTimewout ;
            while (!SendIoControl(IOCTL_USERPROCESSOR_ALIVE,NULL,0,NULL,0,NULL) && dwWaitTicks!=0) {
                if (WaitForSingleObject(m_ProcessInformation.hProcess,TIMOUT_INTERVAL)== WAIT_OBJECT_0)
                    break;
                if (dwWaitTicks<=TIMOUT_INTERVAL )
                    break;
                else 
                    dwWaitTicks-= TIMOUT_INTERVAL;
            }
            bReturn = (WaitForSingleObject(m_ProcessInformation.hProcess,1)!= WAIT_OBJECT_0);
            DEBUGMSG(ZONE_WARNING && !dwWaitTicks,(TEXT("REFLECTOR! Processor %s %s is not responding!\r\n"),m_lpProcName,m_lpProcVolName));
            DEBUGMSG(ZONE_ERROR && !bReturn,(TEXT("REFLECTOR! Processor %s %s is dead!!!\r\n"),m_lpProcName,m_lpProcVolName));
        }
    }
    Unlock();
    ASSERT(bReturn);
    return bReturn;
}
CReflector * UserDriverProcessor::CreateReflector( LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags)
{
    CReflector * pRetReflect = NULL;
    Lock();
    if (!m_fTerminate) {
        pRetReflect = new CReflector(this,lpPreFix, lpDrvName, dwFlags);
        if (pRetReflect && !pRetReflect->Init()) {
            delete pRetReflect;
            pRetReflect = NULL;
        }
        if (pRetReflect) {
            m_ReflectorList.InsertObjectBy(pRetReflect) ;
        }
        CheckReflectorForEmpty();
    }
    Unlock();
    return pRetReflect;
    
}
BOOL UserDriverProcessor::DeleteReflector(CReflector *pReflector)
{
    Lock();
    BOOL bReturn = FALSE;
    if (pReflector ) {
        bReturn = (m_ReflectorList.RemoveObjectBy(pReflector)!=NULL);
    }
    CheckReflectorForEmpty();
    Unlock();
    ASSERT(bReturn);
    return bReturn;
}
CReflector * UserDriverProcessor::FindReflectorBy(CReflector *pReflector )
{
    return m_ReflectorList.FoundObjectBy(pReflector);
}
BOOL UserDriverProcessor::CheckReflectorForEmpty()
{
    if (m_ReflectorList.IsEmpty()) { // It is empty. delete it.
        m_fTerminate = TRUE;
        ::DeleteUserProc(this);
        return TRUE;
    }
    else 
        return FALSE;
}
BOOL UserDriverProcessor::CallAnyOneRefService(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL bReturn = FALSE;
    if (!m_fTerminate && !m_ReflectorList.IsEmpty()) {
        Lock(); // During the lock, there is no remove and insertion.
        CReflector * pCurReflect = m_ReflectorList.GetHeaderPtr();
        while (pCurReflect) {
            bReturn = pCurReflect->ReflService(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, NULL);
            CReflector * pNextReflector = pCurReflect->GetNextObject();
            if (bReturn)
                break;
            else
                pCurReflect = pNextReflector ;
        }
        Unlock();
    }
    return bReturn;
}

