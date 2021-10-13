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
    ProMgr.cpp
Abstract:
    User Mode Driver Processor Manager object.

Notes: 
--*/

#include <windows.h>
#include <devload.h>
#include <DevZones.h>
#include <sid.h>
#include "ProMgr.hpp"
#include "ReflServ.hpp"

UDPContainer *g_pUDPContainer = NULL;
UDServiceContainer * g_pServiceContainer = NULL;
LPCTSTR ConvertStringToUDeviceSID(LPCTSTR szAccount, DWORD cchLen);


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
    UserDriverProcessor* pUDP;

    if (!g_pUDPContainer)
        goto cleanUp;
    
    if (dwIoControlCode != IOCTL_REF_REGISTER_UDEVMGR_HANDLE)
    {
        pUDP = g_pUDPContainer->FindUserProcByProcID(GetCallerVMProcessId());
    }
    else
    {
        pUDP = g_pUDPContainer->GetUDPPendingInit(GetCallerVMProcessId());
    }
    
    if (pUDP)
    {
        bRet = pUDP->CallAnyOneRefService(dwIoControlCode, lpInBuf,nInBufSize, lpOutBuf,nOutBufSize, lpBytesReturned);
        pUDP->DeRef();
    }

cleanUp:
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
        if (m_lpProcName && dwLen > 0)
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
    if (!(regKey.IsKeyOpened() && regKey.GetRegValue(UDP_REG_PROCESSOR_TIMEOUT_VAL,(LPBYTE)&m_dwProgTimeout,sizeof(DWORD))))
    { // If failed we use default.
        m_dwProgTimeout = UDP_REG_PROCESSOR_TIMEOUT_DEFAULT ;
    }

    m_pUDPPendingInit = NULL;
    InitializeListHead(&m_pDefaultProcList);
}

struct ProcGroupAccount : public LIST_ENTRY {
    TCHAR szAccount[MAX_PATH];
    DWORD ProcGroup;
    BOOL  fService;
};

UDPContainer::~UDPContainer()
{
    SetUDPPendingInit(NULL);

    while(!IsListEmpty(&m_pDefaultProcList)){
        ProcGroupAccount* pProcGroupAccount = 
            static_cast<ProcGroupAccount*>(m_pDefaultProcList.Flink);
        RemoveEntryList(static_cast<PLIST_ENTRY>(pProcGroupAccount));
        delete pProcGroupAccount;
    }
    
    if (m_lpProcName)
        delete [] m_lpProcName;
    if (m_lpProcVolName)
        delete [] m_lpProcVolName;
}

BOOL UDPContainer::Init()
{
    HKEY hKeyDrivers;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEVLOAD_DRIVERS_KEY, 0, KEY_READ, &hKeyDrivers) 
        != ERROR_SUCCESS)
        return NULL;

    TCHAR szName[MAX_PATH];
    DWORD i = 0, cchName = _countof(szName);
    while (cchName = _countof(szName), 
        RegEnumKeyEx(hKeyDrivers, i++, szName, &cchName, 0, 0, 0, 0) == ERROR_SUCCESS) 
    {
        if (wcsncmp(
            szName, 
            UDP_REGKEY_PROCESSOR_GROUP_PREFIX, 
            _countof(UDP_REGKEY_PROCESSOR_GROUP_PREFIX)-1)
            )
        {
            // Key doesn't start with UDP_REGKEY_PROCESSOR_GROUP_PREFIX
            continue;
        }

        DWORD dwUserProcGroupID = wcstol(&szName[_countof(UDP_REGKEY_PROCESSOR_GROUP_PREFIX)], NULL, 16);

        HKEY hKeyProcGroup;
        if (RegOpenKeyEx(hKeyDrivers, szName, 0, KEY_READ, &hKeyProcGroup) 
            == ERROR_SUCCESS) 
        {
            DWORD dwType, dwDefault = 0, cbDefault = sizeof(dwDefault);
            if (RegQueryValueEx(
                hKeyProcGroup, 
                UDP_REGKEY_PROCESSOR_DEFAULT, 
                0, 
                &dwType, 
                reinterpret_cast<BYTE*>(&dwDefault), 
                &cbDefault) 
                != ERROR_SUCCESS || dwDefault == 0) 
            {
                // group doesn't have "default":=1, 
                // so don't use it to place further drivers in
                continue;
            }

            // work out the accoundsid for this group
            TCHAR szProcSidFriendly[MAX_PATH];
            szProcSidFriendly[0] = NULL;
            
            DWORD cbProcSidFriendly = sizeof(szProcSidFriendly);
            LPCTSTR pszProcSid;
            if (RegQueryValueEx(
                hKeyProcGroup, 
                DEVLOAD_USERACCOUNTSID_VALNAME, 
                0, 
                &dwType, 
                reinterpret_cast<BYTE*>(szProcSidFriendly), 
                &cbProcSidFriendly) 
                == ERROR_SUCCESS) 
            {
                szProcSidFriendly[MAX_PATH - 1] = NULL;

                pszProcSid = ConvertStringToUDeviceSID(
                    szProcSidFriendly, 
                    lstrlen(szProcSidFriendly)
                    );
                if (!pszProcSid)
                    pszProcSid = szProcSidFriendly;
            } 
            else 
            {
                 pszProcSid = SID_UDEVICE_TCB; //else TCB. TODO: temp. remove
            }

            // work out the accoundsid for this group
            TCHAR szProcVolPrefix[MAX_PATH];
            szProcVolPrefix[0] = NULL;
            DWORD cbProcVolPrefix = sizeof(szProcVolPrefix);
            BOOL fIsService = FALSE;
            if (RegQueryValueEx(
                hKeyProcGroup, 
                UDP_REG_PROCESSOR_VOLPREFIX_VAL, 
                0, 
                &dwType, 
                reinterpret_cast<BYTE*>(szProcVolPrefix), 
                &cbProcVolPrefix) 
                == ERROR_SUCCESS) 
            {
                fIsService = lstrcmp(szProcVolPrefix, L"$services") == 0;
            } 
            else 
            {
                // all procgroups should have a prefix
                ASSERT(FALSE);
            }

            ProcGroupAccount* pProcGroupAccount = new ProcGroupAccount;
            if (!pProcGroupAccount)
                // OOM on boot up
                return FALSE;

            pProcGroupAccount->ProcGroup = dwUserProcGroupID;
            pProcGroupAccount->fService  = fIsService;

            VERIFY(SUCCEEDED(StringCchCopy(
                pProcGroupAccount->szAccount, 
                _countof(pProcGroupAccount->szAccount), 
                pszProcSid
                )));
            
            InsertTailList(
                &m_pDefaultProcList, 
                static_cast<PLIST_ENTRY>(pProcGroupAccount)
                );
        }
    }

    ASSERT(m_lpProcVolName!=NULL && m_lpProcName!=NULL);
    return (m_lpProcVolName!=NULL && m_lpProcName!=NULL);
}
UserDriverProcessor * UDPContainer::CreateUserProcByGroupID(
    DWORD dwUserProcGroupID,
    LPCTSTR lpAccountId
    )
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
    UserDriverProcessor * pNewProc = new UserDriverProcessor(
                                        dwUserProcGroupID,
                                        lpProcName,
                                        lpProcVolName,
                                        lpAccountId,
                                        dwProgTimeout);

    if (pNewProc)
    {
        SetUDPPendingInit(pNewProc);
        
        if (!pNewProc->Init() ||
            !InsertObjectBy(pNewProc))
            pNewProc = NULL;

        SetUDPPendingInit(NULL);    //this should delete pNewProc if unused
    }
    
    Unlock();

    return pNewProc;
};

void UDPContainer::SetUDPPendingInit(
    UserDriverProcessor* udp
)
{
    m_pendingInitLock.Lock();

    if (udp != m_pUDPPendingInit)
    {
        if (m_pUDPPendingInit)
        {
            m_pUDPPendingInit->DeRef();
            m_pUDPPendingInit = NULL;
        }
        if (udp)
        {
            udp->AddRef();
            m_pUDPPendingInit = udp;
        }
    }
    m_pendingInitLock.Unlock();
}

UserDriverProcessor * UDPContainer::GetUDPPendingInit(
    DWORD dwProcessId
)
{
    UserDriverProcessor* udp;

    m_pendingInitLock.Lock();
    
    udp = m_pUDPPendingInit;
    
    if (udp &&
        udp->GetUserDriverProcessorInfo().dwProcessId == dwProcessId)
    {
        udp->AddRef();
    }
    else
    {
        udp = NULL;
    }
    
    m_pendingInitLock.Unlock();

    return udp;    
}

UserDriverProcessor * UDPContainer::CreateUserProc(
    LPCTSTR lpAccountId
    ) 
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
    pReturn = CreateUserProcByGroupID(m_dwCurIndex, lpAccountId);
    Unlock();
    ASSERT(pReturn);
    return pReturn;
}

UserDriverProcessor * UDPContainer:: FindUserProcBy(
    UserDriverProcessor * pUDP
    )
{
    return FoundObjectBy(pUDP);
}

UserDriverProcessor * UDPContainer::FindUserProcByID(
    DWORD dwUserProcGroupID,
    LPCTSTR lpAccountId,
    BOOL fCreateOnNoExit
    )
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
            UserDriverProcessor * pNewProc = CreateUserProcByGroupID(
                                                dwUserProcGroupID,
                                                lpAccountId);
                                                
            if (pNewProc) { // This is newly created. So it should succeeded.
                pReturn = FindUserProcBy(pNewProc);
                ASSERT(pReturn);
            }
        }
        Unlock();
    }
    else {
        UserDriverProcessor * pNewProc = CreateUserProc(lpAccountId) ;
        if (pNewProc) {
            pReturn = FindUserProcBy(pNewProc);
            ASSERT(pReturn);
        }
    }
    ASSERT(pReturn);
    return pReturn;
}

UserDriverProcessor * UDPContainer::FindUserProcByAccount(
    __in_z LPCTSTR pszDriverSidFriendly,
    BOOL fIsService
    )
{
    LPCTSTR pszDriverSid = ConvertStringToUDeviceSID(
                    pszDriverSidFriendly, 
                    lstrlen(pszDriverSidFriendly)
                    );

    if (!pszDriverSid)
        pszDriverSid = pszDriverSidFriendly;

    if (!IsListEmpty(&m_pDefaultProcList)) 
    {
        // Iterate the list and find a Proc with the same AccountSid
        ProcGroupAccount* pTrav = 
            static_cast<ProcGroupAccount *>(m_pDefaultProcList.Flink);
        do 
        {
            if (lstrcmpi(pTrav->szAccount, pszDriverSid) == 0 
                && pTrav->fService == fIsService) 
            {
                return FindUserProcByID(pTrav->ProcGroup, NULL, TRUE);
            }

            pTrav = static_cast<ProcGroupAccount *>(pTrav->Flink);
        } while (pTrav != static_cast<ProcGroupAccount *>(&m_pDefaultProcList));
    }

    return NULL;
}

UserDriverProcessor * UDPContainer::FindUserProcByProcID(DWORD dwProcessId)
{
    UserDriverProcessor * pReturn = NULL;
    Lock();
    UserDriverProcessor *  pCur = m_rgLinkList;
    while (pCur) {
        if (pCur->GetUserDriverProcessorInfo().dwProcessId == dwProcessId ) {
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

LPCTSTR ConvertStringToUDeviceSID(LPCTSTR szAccount, DWORD cchLen)
{
    if (_tcsnicmp(szAccount,
                  SID_UDEVICE_ELEVATED_STR, 
                  cchLen) == 0)
    {
        return SID_UDEVICE_ELEVATED;
    }
    else if (_tcsnicmp(szAccount,
                       SID_UDEVICE_STANDARD_STR,
                       cchLen) == 0)
    {
        return SID_UDEVICE_STANDARD;
    }
    else if (_tcsnicmp(szAccount,
                       SID_UDEVICE_TCB_STR,
                       cchLen) == 0)
    {
        return SID_UDEVICE_TCB;
    }
    if (cchLen > 2 && (szAccount[0] != L'S' || szAccount[1] != L'-')) 
    {
        // szAccount looks like a string constant (not a sid)
        // but doesn't match any constants we know of.
        ASSERT(FALSE);
    }
    return NULL;
}

UserDriverProcessor::UserDriverProcessor(
    DWORD dwProcID,
    LPCTSTR lpProcName,
    LPCTSTR lpProcVolName,
    LPCTSTR lpAccountId,    // accountid to run the udevice.exe process
    DWORD dwTimeout,
    UserDriverProcessor * pNextProcessor
    )
    :   m_dwProcID(dwProcID)
    ,   m_pNextProcessor(pNextProcessor)
    ,   m_dwTimeOut(dwTimeout)
    ,   m_fAllocedAccountId(FALSE)
{
    m_lpProcName = NULL;
    m_lpProcVolName = NULL;
    m_lpAccountId = NULL;
    m_ProcessInformation.hProcess = NULL;
    m_ProcessInformation.hThread = NULL ;
    m_ProcessInformation.dwProcessId  = 0;
    m_ProcessInformation.dwThreadId = 0 ;
    
    size_t cchLen ;
    // Create Processor Name
    if (lpProcName && SUCCEEDED(StringCchLength(lpProcName,MAX_PATH,&cchLen)))
    {
        m_lpProcName = new TCHAR [cchLen + 1];
        if (((cchLen+1) > cchLen) &&  /* check for int. overflow */
            m_lpProcName &&
            !SUCCEEDED(StringCchCopy(m_lpProcName,cchLen+1,lpProcName)))
        {
            delete[] m_lpProcName;
            m_lpProcName = NULL;
        }
    }
    // Create Volume Name.
    TCHAR strVol[MAX_PATH];
    if (lpProcVolName && SUCCEEDED(StringCchPrintf(strVol,MAX_PATH,TEXT("%s_%04x"),lpProcVolName,dwProcID)) &&
            SUCCEEDED(StringCchLength(strVol,MAX_PATH,&cchLen))) {
        m_lpProcVolName = new TCHAR[cchLen + 1];
        if (m_lpProcVolName && !SUCCEEDED(StringCchCopy(m_lpProcVolName,cchLen+1 ,strVol))) {
            delete [] m_lpProcVolName;
            m_lpProcVolName = NULL;
        }
    }

    m_hUDevMgrHndAvailEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    m_hUDevMgr = INVALID_HANDLE_VALUE;

    // match accountid string
    if (lpAccountId
        && SUCCEEDED(StringCchLength(lpAccountId, MAX_PATH, &cchLen)))
    {
        // does it match one of the predefined strings?
        m_lpAccountId = const_cast<LPTSTR>(ConvertStringToUDeviceSID(lpAccountId, cchLen));

        if (m_lpAccountId == NULL) 
        {
            m_lpAccountId = new TCHAR[cchLen+1];
            m_fAllocedAccountId = TRUE;
            if (m_lpAccountId &&
                !SUCCEEDED(StringCchCopy(m_lpAccountId, cchLen+1, lpAccountId)))
            {
                delete[] m_lpAccountId;
                m_lpAccountId = NULL;
            }
        }
    }

    m_fTerminate = FALSE;
}
UserDriverProcessor::~UserDriverProcessor()
{
    Lock();
    SendIoControl(IOCTL_USERPROCESSOR_EXIT,NULL,0,NULL,0,NULL);

    CloseHandle(m_hUDevMgrHndAvailEvent);
    CloseHandle(m_hUDevMgr);
    
    if (m_lpProcName)
        delete[] m_lpProcName;
    if (m_lpProcVolName)
        delete[] m_lpProcVolName;
    if (m_lpAccountId && m_fAllocedAccountId == TRUE)
        delete[] m_lpAccountId;
    if ( m_ProcessInformation.hProcess != NULL)
        CloseHandle(m_ProcessInformation.hProcess);
    if (m_ProcessInformation.hThread != NULL)
        CloseHandle(m_ProcessInformation.hThread);
    Unlock();
}




BOOL UserDriverProcessor::Init()
{
    BOOL fReturn = FALSE;
    Lock();
    if (m_lpProcName!=NULL && m_lpProcVolName!=NULL) 
    {
        // launch udevice.exe
        //

        // coredll better be ready or createprocess will fail
        ASSERT(IsNamedEventSignaled (COREDLL_READY_EVENT, 0));

        CE_PROCESS_INFORMATION processInfo = {0};
        processInfo.cbSize = sizeof(CE_PROCESS_INFORMATION);
        //create suspended to get processInfo
        processInfo.dwFlags = CREATE_SUSPENDED;

        DEBUGMSG(ZONE_INIT, (
            L"devmgr: Creating udevice process %s, vol=%s, accountid=%s\r\n",
            m_lpProcName, m_lpProcVolName, processInfo.lpwszAccountName
        ));

        fReturn = CeCreateProcessEx(m_lpProcName, m_lpProcVolName, &processInfo);

        if (!fReturn)
        {
            RETAILMSG(1, (
                L"ERROR!: Reflector: Error creating process %s. "
                L"GetLastError=0x%x\r\n",
                m_lpProcName,
                GetLastError()
            ));
        }
        else
        {
            m_ProcessInformation = processInfo.ProcInfo;

            //processInfo available. Resume.
            ResumeThread(m_ProcessInformation.hThread);

            //wait to get communication handle from the created process
            fReturn = WaitForInitComplete();
        }        
    }
    Unlock();
    ASSERT(fReturn);
    return fReturn;
}

BOOL UserDriverProcessor::WaitForInitComplete()
{
// while waiting for hosting process to respond print out a message every PRINTTIMEOUT secs
#define PRINTTIMEOUT    20*1000 // 20 secs

    // wait m_dwTimeOut to get handle from udevice.exe
    // print out an info message every PRINTTIMEOUT secs (this will help in debugging)
    //
    // comments: sometimes udevice.exe might take a long time to respond. Here
    // is one of the scenarios we've seen it happen: 
    // 1) we launch a udevice.exe or servicesd.exe process
    // 2) kernel starts to load the .exe but it has to take a loader lock
    // 3) another DLL like rillog.DLL is being copied over RELFSD at the same moment
    //    and this DLL is big so it's taking a long time to be copied over relfsd
    // 4) kernel holds the loader lock while it's loading this DLL over relfsd
    // 5) so kernel blocks waiting for the loader lock it needs to load the .exe or .dll in step 2)
    // 6) so udevice.exe doesn't get to run for a while and never signals back
    //    to device manager.
    // to avoid this right now we are changing the default timeout to 180 secs
    // and putting out an Info message whenever it happens.
    // This situation should not happen on a real device.

    ASSERT(m_hUDevMgrHndAvailEvent != NULL);

    DWORD dwRet, dwTimeWaited = 0;
    do
    {
        dwRet = WaitForSingleObject(m_hUDevMgrHndAvailEvent, PRINTTIMEOUT);
        if (dwRet == WAIT_TIMEOUT)
        {
            RETAILMSG(1, (L"INFO:Reflector: Launched udevice.exe/servicesd.exe process (processId=%d), waiting to get response...\r\n",
                m_ProcessInformation.dwProcessId));
        }
        else
        {
            break;  // not a timeout. event signaled or error
        }

        dwTimeWaited += PRINTTIMEOUT;
    }
    while (dwTimeWaited < m_dwTimeOut);

    if (dwRet != WAIT_OBJECT_0)
    {
        RETAILMSG(1, (
            L"ERROR! Reflector: failed to get response (IOCTL_REF_REGISTER_UDEVICE_HANDLE) "
            L" from udevic.exe/servicesd.exe process (processid=%d). Aborting load of user-mode driver/service.\r\n",
            m_ProcessInformation.dwProcessId
        ));
        ASSERT(FALSE);
        return FALSE;
    }
    else
    {
        //let go of the event
        CloseHandle(m_hUDevMgrHndAvailEvent);
        m_hUDevMgrHndAvailEvent = NULL;
    }
    return TRUE;
}

BOOL UserDriverProcessor::SendIoControl(
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned
)
{
    ASSERT(m_hUDevMgr != INVALID_HANDLE_VALUE);

    if (m_hUDevMgr != INVALID_HANDLE_VALUE)
    {
        return DeviceIoControl(m_hUDevMgr, dwIoControlCode,lpInBuffer, nInBufferSize,
            lpOutBuffer, nOutBufferSize, lpBytesReturned, NULL);  
    }
    else
    {
        return FALSE;
    }
}

CReflector * UserDriverProcessor::CreateReflector(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName, DWORD dwFlags)
{
    CReflector * pRetReflect = NULL;
    if (!m_fTerminate) {
        pRetReflect = new CReflector(this,lpszDeviceKey,lpPreFix, lpDrvName, dwFlags);
        Lock();
        if (pRetReflect && !pRetReflect->Init()) {
            delete pRetReflect;
            pRetReflect = NULL;
        }
        if (pRetReflect) {
            m_ReflectorList.InsertObjectBy(pRetReflect) ;
        }
        CheckReflectorForEmpty();
        Unlock();
    }
    return pRetReflect;
    
}
BOOL UserDriverProcessor::DeleteReflector(CReflector *pReflector)
{
    Lock();
    BOOL fReturn = FALSE;
    if (pReflector ) {
        fReturn = (m_ReflectorList.RemoveObjectBy(pReflector)!=NULL);
    }
    CheckReflectorForEmpty();
    Unlock();
    ASSERT(fReturn);
    return fReturn;
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
    BOOL fReturn = FALSE;
    HANDLE hUDevMgr = INVALID_HANDLE_VALUE;

    // Check if udevmgr is sending the API handle
    if (m_hUDevMgr == INVALID_HANDLE_VALUE)
    {
        if (dwIoControlCode == IOCTL_REF_REGISTER_UDEVMGR_HANDLE &&
            lpInBuf && 
            nInBufSize == sizeof(HANDLE))
        {
            hUDevMgr = *((HANDLE*)(lpInBuf));

            //duplicate the handle
            if (hUDevMgr != INVALID_HANDLE_VALUE)
            {
                if (DuplicateHandle(
                        (HANDLE)GetCallerVMProcessId(),
                        hUDevMgr,
                        GetCurrentProcess(),
                        &m_hUDevMgr,
                        0,
                        FALSE, 
                        DUPLICATE_SAME_ACCESS))
                {
                    //notify handle is available
                    ASSERT(m_hUDevMgrHndAvailEvent != NULL);
                    SetEvent(m_hUDevMgrHndAvailEvent);
                    fReturn = TRUE;
                }
                else
                {
                    DEBUGMSG(ZONE_ERROR, (
                        L"ERROR! Reflector: Duplicating handle failed."
                        L"GetLastError=0x%x\r\n",
                        GetLastError()
                    ));
                }
                
            }       
        }
    }
    else if (!m_fTerminate && !m_ReflectorList.IsEmpty()) {
        Lock(); // During the lock, there is no remove and insertion.
        CReflector * pCurReflect = m_ReflectorList.GetHeaderPtr();
        while (pCurReflect) {
            fReturn = pCurReflect->ReflService(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, NULL);
            CReflector * pNextReflector = pCurReflect->GetNextObject();
            if (fReturn)
                break;
            else
                pCurReflect = pNextReflector ;
        }
        Unlock();
    }
    return fReturn;
}

PROCESS_INFORMATION UserDriverProcessor::GetUserDriverProcessorInfo() const
{
    return m_ProcessInformation;
}

LPTSTR UserDriverProcessor::GetAccountId() const
{
    return m_lpAccountId;
}

DWORD UserDriverProcessor::GetProcID() const
{
    return m_dwProcID;
}

UserDriverProcessor * UserDriverProcessor::GetNextObject()
{
    return m_pNextProcessor;
}

void UserDriverProcessor::SetNextObject (UserDriverProcessor *pNext)
{
    m_pNextProcessor = pNext;
}

