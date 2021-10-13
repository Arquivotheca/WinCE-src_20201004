//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
#include "SMB_Globals.h"
#include "ConnectionManager.h"
#include "Cracker.h"
#include "SMBCommands.h"
#include "PC_NET_PROG.h"
#include "FileServer.h"

UniqueID   ActiveConnection::m_UIDFindFirstHandle;

#ifdef DEBUG
BOOL ConnectionManager::m_fFirstInst = TRUE;
#endif

#define MAX_NUM_GUESTS      20


ActiveConnection::ActiveConnection():m_ulConnectionID(0xFFFFFFFF),
                                   m_fIsUnicode(FALSE), m_fContextSet(FALSE),
                                   m_fIsGuest(TRUE)
{
       IFDBG(m_uiAcceptCalled = 0;);
       m_dwLastUsed = GetTickCount();
       InitializeCriticalSection(&m_csConnectionLock);
}

#ifdef DEBUG
VOID
ActiveConnection::DebugPrint()
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    TRACEMSG(ZONE_ERROR, (L"Connection: "));
    TRACEMSG(ZONE_ERROR, (L"    USER: %s", m_UserName.GetString()));

    // Find First handles
    ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itFF;
    ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itFFEnd = m_FindHandleList.end();
    TRACEMSG(ZONE_ERROR, (L"    FindFirst Handles:"));
    for(itFF = m_FindHandleList.begin(); itFF != itFFEnd; itFF++) {
        TRACEMSG(ZONE_ERROR, (L"  %d -- %s", (*itFF).SearchString.GetString(), (*itFF).usSID));
    }


    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC >::iterator itSM;
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC >::iterator itSMEnd = m_MyTidList.end();
    TRACEMSG(ZONE_ERROR, (L"    TID Handles:"));
    for(itSM = m_MyTidList.begin(); itSM != itSMEnd; itSM++) {
        TRACEMSG(ZONE_ERROR, (L"    TID: %d", (*itSM)->TID()));
        (*itSM)->DebugPrint();
    }

    ce::list<NotificationNode>::iterator itNN;
    ce::list<NotificationNode>::iterator itNNEnd = m_NotificationNodeList.end();
    TRACEMSG(ZONE_ERROR, (L"    Notification Handles:"));
    for(itNN = m_NotificationNodeList.begin(); itNN != itNNEnd; itNN++) {
        TRACEMSG(ZONE_ERROR, (L"    NID: %d", (*itNN).pMyNode->GetID()));
    }
}
#endif

ActiveConnection::~ActiveConnection()
{
    //
    // Destroy any handles we may have
    while(m_FindHandleList.size()) {
        ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC>::iterator it = m_FindHandleList.begin();
        ActiveConnection::CloseFindHandle((*it).usSID);
    }

    //
    // Destroy any credentials we may still have
    if(TRUE == m_fContextSet) {
        FreeCredentialsHandle(&(m_Credentials));
        DeleteSecurityContext(&(m_ContextHandle));
    }

    //
    // Purge out any active notifications we may have
    SMB_Globals::g_pWakeUpOnEvent->Freeze();
        EnterCriticalSection(&m_csConnectionLock);
            while(m_NotificationNodeList.size()) {
                m_NotificationNodeList.front().pMyNode->Terminate(true);
                m_NotificationNodeList.pop_front();
            }
        LeaveCriticalSection(&m_csConnectionLock);
    SMB_Globals::g_pWakeUpOnEvent->UnFreeze();

    DeleteCriticalSection(&m_csConnectionLock);
}


BOOL
ActiveConnection::HasOpenedResources()
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator it;
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator itEnd = m_MyTidList.end();

    //
    // Find our node
    for(it = m_MyTidList.begin(); it != itEnd; it++) {
       if((*it)->HasOpenedResources()) {
            return TRUE;
       }
    }
    return FALSE;
}

VOID
ActiveConnection::SetUnicode(BOOL fStatus)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    m_fIsUnicode = fStatus;
}

BOOL
ActiveConnection::SupportsUnicode(SMB_HEADER *pRequest, BYTE smb, BYTE subCmd)
{
    if(!(pRequest->Flags2 & SMB_FLAGS2_UNICODE)) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

VOID
ActiveConnection::SetConnectionInfo(ULONG ulConnectionID, USHORT usUid)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    m_ulConnectionID = ulConnectionID;
    m_usUid = usUid;
}

ULONG
ActiveConnection::ConnectionID()
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    return m_ulConnectionID;
}

USHORT
ActiveConnection::Uid()
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    return m_usUid;
}


HRESULT
ActiveConnection::CancelWakeUpEvent(USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    HRESULT hr = E_FAIL;

    TRACEMSG(ZONE_DETAIL, (L"SMB_SRVR: cancel wake up event UID=%d, PID=%d, TID=%d, MID=%d", usUid, usPid, usTid, usMid));

    ce::list<NotificationNode>::iterator it, itEnd;

    for(it = m_NotificationNodeList.begin(), itEnd = m_NotificationNodeList.end(); it != itEnd; ++it) {
        if((*it).usUid == usUid &&
            (*it).usPid== usPid &&
            (*it).usTid == usTid &&
            (*it).usMid == usMid) {
                (*it).pMyNode->Terminate();
                m_NotificationNodeList.erase(it);
                hr = S_OK;
                break;
        }
    }
    return hr;
}
HRESULT ActiveConnection::AddWakeUpEvent(WakeUpNode *pNode, USHORT usId, USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();

    TRACEMSG(ZONE_DETAIL, (L"SMB_SRVR: add wake up event %d", usId));

    m_NotificationNodeList.insert(m_NotificationNodeList.begin());

    m_NotificationNodeList.front().pMyNode = pNode;
    m_NotificationNodeList.front().usUid = usUid;
    m_NotificationNodeList.front().usPid = usPid;
    m_NotificationNodeList.front().usTid = usTid;
    m_NotificationNodeList.front().usMid = usMid;

    return S_OK;
}
HRESULT ActiveConnection::RemoveWakeUpEvent(USHORT usId)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    HRESULT hr = E_FAIL;

    TRACEMSG(ZONE_DETAIL, (L"SMB_SRVR: remove wake up event %d", usId));

    ce::list<NotificationNode>::iterator it, itEnd;

    for(it = m_NotificationNodeList.begin(), itEnd = m_NotificationNodeList.end(); it != itEnd; ++it) {
        if((*it).pMyNode->GetID() == usId) {
            m_NotificationNodeList.erase(it);
            hr = S_OK;
            break;
        }
    }
    return hr;
}


HRESULT
ActiveConnection::BindToTID(Share *pShare, ce::smart_ptr<TIDState> &pTIDState)
{
    TIDState *pNewState;
    HRESULT hr;
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();

    if(NULL == (pNewState = new TIDState())) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }
    if(FAILED(pNewState->Init(pShare))) {
        delete pNewState;
        hr = E_UNEXPECTED;
        goto Done;
    }

    if(!m_MyTidList.push_back(pNewState)) {
        return NULL;
    }
    pTIDState = m_MyTidList.back();


    hr = S_OK;
    Done:
           return hr;
}


HRESULT
ActiveConnection::TerminateConnectionsForShare(Share *pShare)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator it, itEnd;
    BOOL fFound = FALSE;

    



    for(it = m_MyTidList.begin(), itEnd = m_MyTidList.end(); it != itEnd; ) {
        if((*it)->GetShare() == pShare) {
           RETAILMSG(1, (L"SMBSRV: Cant delete share because its in use!"));
           fFound = TRUE;
           ++it;
        } else {
            ++it;
        }
    }
    return (TRUE==fFound)?E_FAIL:S_OK;
}



HRESULT
ActiveConnection::UnBindTID(USHORT usTid)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator it;
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator itEnd = m_MyTidList.end();
    ASSERT(0xFFFF != usTid );
    BOOL fFound = FALSE;

    //
    // Find our node
    for(it = m_MyTidList.begin(); it != itEnd; it++) {
        if((*it)->TID() == usTid) {
            fFound = TRUE;
            m_MyTidList.erase(it);
            break;
        }
    }
    return (TRUE==fFound)?S_OK:E_FAIL;
}

HRESULT
ActiveConnection::FindTIDState(USHORT usTid, ce::smart_ptr<TIDState> &pTIDState, DWORD dwAccessPerms)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator it;
    ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>::iterator itEnd = m_MyTidList.end();
    ASSERT(0xFFFF != usTid );


    HRESULT hr = E_FAIL;
    pTIDState = NULL;

    //
    // Find our node
    for(it = m_MyTidList.begin(); it != itEnd; it++) {
        if((*it)->TID() == usTid) {
            pTIDState = *it;
            break;
        }
    }

    if(pTIDState) {
        if((!pTIDState->GetShare()) || (E_ACCESSDENIED == (hr = (pTIDState->GetShare()->AllowUser(UserName(), dwAccessPerms))))) {
            TRACEMSG(ZONE_SECURITY, (L"SMB_SRVR: user denied access to share state!"));
            pTIDState = NULL;
            hr = E_ACCESSDENIED;
        }
    } else {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: someone is going after a TID they dont own!  (being hacked?)"));
    }
    return hr;
}



HRESULT
ActiveConnection::CreateNewFindHandle(const WCHAR *pSearchString, USHORT *pusHandle, BOOL fUnicodeRules)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    HANDLE hSearch = INVALID_HANDLE_VALUE;
    USHORT usHandle = 0xFFFF;
    HRESULT hr;
    WIN32_FIND_DATA w32FindData;
    PREFAST_ASSERT(NULL != pSearchString && NULL != pusHandle);
    StringConverter WildSearch;
    BOOL  fDotsMatch = FALSE;
    WCHAR *pSubSearchString = NULL;

    //
    // Get a handle
    if(FAILED(m_UIDFindFirstHandle.GetID(&usHandle))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: Error getting FindFirstHandle from unique generator!"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }

    //
    // Convert the search string to proper wildcard format
    if(FAILED(hr = ConvertWildCard(pSearchString, &WildSearch, fUnicodeRules))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: Error converting wildcard in FindFirstFile! on %s", pSearchString));
        goto Done;
    }

    //
    // See if . or .. match the search pattern
    // We do this b/c the CE FS doesnt have this concept
    pSubSearchString = WildSearch.GetUnsafeString();
    pSubSearchString = pSubSearchString + WildSearch.Length() - 1;
    ASSERT(NULL == *pSubSearchString);
    while('\\' != *pSubSearchString && pSubSearchString > WildSearch.GetUnsafeString()) {
        pSubSearchString --;
    }
    if('\\' == *pSubSearchString) {
        pSubSearchString ++;
    }
    if(TRUE == MatchesWildcard(wcslen(pSubSearchString), pSubSearchString, 1, L".") ||
        TRUE == MatchesWildcard(wcslen(pSubSearchString), pSubSearchString, 2, L"..")) {
        fDotsMatch = TRUE;
    }

    //
    // Open up the search handle
    if (INVALID_HANDLE_VALUE == (hSearch = FindFirstFile (WildSearch.GetString(), &w32FindData)) && FALSE == fDotsMatch) {
        TRACEMSG(ZONE_FILES, (L"SMB_SRVR: Error calling FindFirstFile! on %s", WildSearch.GetString()));
        hr = E_FAIL;
        goto Done;
    }

    //
    // Success
    hr = S_OK;
    Done:
        if(FAILED(hr)) {
            if(INVALID_HANDLE_VALUE != hSearch) {
                FindClose(hSearch);
            }
            if(0xFFFF != usHandle) {
                m_UIDFindFirstHandle.RemoveID(usHandle);
            }
            *pusHandle = 0xFFFF;
        } else {
            ASSERT(0xFFFF != usHandle);

            ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it;
            m_FindHandleList.insert(m_FindHandleList.end());
            it = --(m_FindHandleList.end());

            //
            // Create a node in our linked list
            if(INVALID_HANDLE_VALUE != hSearch) {
                memcpy(&(*it).w32LastNode, &w32FindData, sizeof(WIN32_FIND_DATA));
                (*it).fLastNodeUsed = TRUE;
            } else {
                memset(&(*it).w32LastNode, 0, sizeof(WIN32_FIND_DATA));
                (*it).fLastNodeUsed = FALSE;
            }
            (*it).SearchString.append(WildSearch.GetString());
            (*it).usSID = usHandle;
            (*it).hHandle = hSearch;
            if(TRUE == fDotsMatch) {
                (*it).fGivenDot = FALSE;
                (*it).fGivenDotDot = FALSE;
            } else {
                (*it).fGivenDot = TRUE;
                (*it).fGivenDotDot = TRUE;
            }
            *pusHandle = usHandle;
        }

        return hr;
}

HRESULT
ActiveConnection::ResetSearch(USHORT usHandle)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it;
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itEnd = m_FindHandleList.end();
    ASSERT(0xFFFF != usHandle );
    HRESULT hr = E_FAIL;

    //
    // Find our node
    for(it = m_FindHandleList.begin(); it != itEnd; it++) {
        if((*it).usSID == usHandle) {
            BOOL fDotsMatch = FALSE;
            WCHAR *pSubSearchString = NULL;

            if(INVALID_HANDLE_VALUE != (*it).hHandle) {
               FindClose((*it).hHandle);
            }

            //
            // See if . or .. match the search pattern
            // We do this b/c the CE FS doesnt have this concept
            pSubSearchString = (*it).SearchString.GetUnsafeString();
            pSubSearchString = pSubSearchString + wcslen(pSubSearchString);
            while('\\' != *pSubSearchString && pSubSearchString >(*it).SearchString.GetUnsafeString()) {
                pSubSearchString --;
            }
            if('\\' == *pSubSearchString) {
                pSubSearchString ++;
            }
            if(TRUE == MatchesWildcard(wcslen(pSubSearchString), pSubSearchString, 1, L".") ||
                TRUE == MatchesWildcard(wcslen(pSubSearchString), pSubSearchString, 2, L"..")) {
                fDotsMatch = TRUE;
            }

            //
            // Open up the search handle
            if (INVALID_HANDLE_VALUE == ((*it).hHandle = FindFirstFile ((*it).SearchString.GetString(), &((*it).w32LastNode))) &&
                FALSE == fDotsMatch) {
                (*it).fLastNodeUsed = FALSE;
                TRACEMSG(ZONE_FILES, (L"SMB_SRVR: Error calling FindFirstFile during reset!"));
                hr = E_FAIL;
                goto Done;
            }

            //
            // In the case where we have dots but no handle -- invalidate the last node
            if(INVALID_HANDLE_VALUE == (*it).hHandle) {
                (*it).fLastNodeUsed = FALSE;
            } else {
                (*it).fLastNodeUsed = TRUE;
            }

            //
            // Set the dots given flags
            if(TRUE == fDotsMatch) {
                (*it).fGivenDot = FALSE;
                (*it).fGivenDotDot = FALSE;
            } else {
                (*it).fGivenDot = TRUE;
                (*it).fGivenDotDot = TRUE;
            }

            hr = S_OK;
            break;
        }
    }

    Done:
        return hr;
}


HRESULT
ActiveConnection::NextFile(USHORT usHandle, WIN32_FIND_DATA **ppData)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it;
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itEnd = m_FindHandleList.end();
    PREFAST_ASSERT(0xFFFF != usHandle && NULL != ppData);
    SYSTEMTIME STCurrentTime;
    FILETIME   FTCurrentTime;
    GetSystemTime(&STCurrentTime);

    if(0 == SystemTimeToFileTime(&STCurrentTime, &FTCurrentTime)) {
        memset(&FTCurrentTime, 0, sizeof(FILETIME));
        ASSERT(FALSE);
    }

    *ppData = NULL;
    //
    // Find our node
    for(it = m_FindHandleList.begin(); it != itEnd; it++) {

      if((*it).usSID == usHandle) {
          if(FALSE == (*it).fGivenDot) {
                memset(&((*it).w32TempNode), 0, sizeof(WIN32_FIND_DATA));
                (*it).w32TempNode.dwFileAttributes = 16;
                (*it).w32TempNode.ftCreationTime = FTCurrentTime;
                (*it).w32TempNode.ftLastAccessTime = FTCurrentTime;
                (*it).w32TempNode.ftLastWriteTime = FTCurrentTime;
                swprintf((*it).w32TempNode.cFileName, L".");
                *ppData = &((*it).w32TempNode);
          } else if (FALSE == (*it).fGivenDotDot) {
                memset(&((*it).w32TempNode), 0, sizeof(WIN32_FIND_DATA));
                (*it).w32TempNode.dwFileAttributes = 16;
                (*it).w32TempNode.ftCreationTime = FTCurrentTime;
                (*it).w32TempNode.ftLastAccessTime = FTCurrentTime;
                (*it).w32TempNode.ftLastWriteTime = FTCurrentTime;
                swprintf((*it).w32TempNode.cFileName, L"..");
                *ppData = &((*it).w32TempNode);
          } else if(TRUE == (*it).fLastNodeUsed) {
              *ppData = &((*it).w32LastNode);
          }
          break;
      }
    }

    if(NULL == *ppData) {
        return E_FAIL;
    } else {
        return S_OK;
    }
}


HRESULT
ActiveConnection::AdvanceToNextFile(USHORT usHandle)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it;
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itEnd = m_FindHandleList.end();
    ASSERT(0xFFFF != usHandle );
    HRESULT hr = E_FAIL;
    //
    // Find our node
    for(it = m_FindHandleList.begin(); it != itEnd; it++) {

      if((*it).usSID == usHandle) {
          if(FALSE == (*it).fGivenDot) {
            (*it).fGivenDot = TRUE;
            hr = S_OK;
          } else if(FALSE == (*it).fGivenDotDot) {
            (*it).fGivenDotDot = TRUE;
            hr = S_OK;
          } else if(0 != FindNextFile ((*it).hHandle, &((*it).w32LastNode))) {
               (*it).fLastNodeUsed = TRUE;
               hr = S_OK;
          } else {
               (*it).fLastNodeUsed = FALSE;
               hr = E_FAIL;
          }
          break;
      }
    }
    return hr;
}


HRESULT
ActiveConnection::CloseFindHandle(USHORT usHandle)
{
   CCritSection csLock(&m_csConnectionLock);
   csLock.Lock();
   ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it;
   ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itEnd = m_FindHandleList.end();
   ASSERT(0xFFFF != usHandle );
   HRESULT hr = E_FAIL;

   //
   // Find our node
   for(it = m_FindHandleList.begin(); it != itEnd; it++) {
       if((*it).usSID == usHandle) {
          if(INVALID_HANDLE_VALUE != (*it).hHandle) {
             FindClose((*it).hHandle);
          }
          if(0xFFFF != (*it).usSID) {
            m_UIDFindFirstHandle.RemoveID((*it).usSID);
          }
          m_FindHandleList.erase(it);
          hr = S_OK;
          break;
       }
   }
   return hr;
}

HRESULT
ActiveConnection::SetUserName(const WCHAR *pName)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    m_UserName.Clear();
    TRACEMSG(ZONE_SECURITY, (L"SMB_SRV:  Setting username to %s", pName));
    return m_UserName.append(pName);
}

const WCHAR *
ActiveConnection::UserName()
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    return m_UserName.GetString();
}


BOOL
ActiveConnection::SendPacket(ChangeNotificationWakeUpNode *pWakeUpNode)
{
    CCritSection csLock(&m_csConnectionLock);
    csLock.Lock();
    return pWakeUpNode->SendPacket();
}


ConnectionManager::ConnectionManager() : m_fZeroUIDTaken(FALSE)
{
    #ifdef DEBUG
        //
        // Just used to make sure (in debug) that multiple instances of the connection manager
        //   dont get created
        if(m_fFirstInst) {
            m_fFirstInst = FALSE;
        }
        else {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"Only one instance of ConnectionManager allowed!!!"));
        }
    #endif

    InitializeCriticalSection(&m_MyCS);
}

ConnectionManager::~ConnectionManager()
{
    if(TRUE == m_fZeroUIDTaken) {
        //
        // Give back the zero UID
        if(FAILED(m_UIDGenerator.RemoveID(0))) {
            ASSERT(FALSE);
        }
    }
#ifdef DEBUG
    ASSERT(0 == m_MyConnections.size());
    m_fFirstInst = TRUE;
#endif
    DeleteCriticalSection(&m_MyCS);
}

ce::smart_ptr<ActiveConnection>
ConnectionManager::FindConnection(SMB_PACKET *pSMB)
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();

    for(it=m_MyConnections.begin(); it!=itEnd; it++) {
        ActiveConnection *pUse = *it;

        if(pUse->ConnectionID() == pSMB->ulConnectionID &&
            pUse->Uid() == pSMB->pInSMB->Uid) {

            //
            // Mark the connection as being used -- we use this number
            //   to terminate connections that have become stale
            pUse->m_dwLastUsed = GetTickCount();
            return *it;
       }
    }
    return NULL;
}


HRESULT
ConnectionManager::FindAllConnections(ULONG ulConnectionID, ce::list<ce::smart_ptr<ActiveConnection>, ACTIVE_CONN_PTR_LIST > &ConnList)
{

    ASSERT(0 == ConnList.size());
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();

    for(it=m_MyConnections.begin(); it!=itEnd; it++) {
        if((*it)->ConnectionID() == ulConnectionID) {
            if(!ConnList.push_back(*it)) {
                return E_OUTOFMEMORY;
            }
        }
    }
    return S_OK;
}



struct IDLE_LIST {
    ULONG ulConnectionID;
    DWORD dwLastUsed;
    ActiveConnection *pConn;
};

ULONG
ConnectionManager::FindStaleConnection(DWORD dwIdleTime)
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ActiveConnection *pAC = NULL;
    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();
    ce::list<IDLE_LIST> idleList;
    ce::list<IDLE_LIST>::iterator itIdleList;
    IDLE_LIST *pThis = NULL;
    PREFAST_ASSERT(pThis);

    DWORD dwNow = GetTickCount();

    for(it=m_MyConnections.begin(); it!=itEnd; it++) {
        pAC = *it;
        pThis = NULL;
        //
        // Seek out this connection ID
        for(itIdleList=idleList.begin(); itIdleList!=idleList.end(); itIdleList++) {
            if((*itIdleList).ulConnectionID == pAC->ConnectionID()) {
                pThis = &(*itIdleList);
            }
        }

        //
        // If its a new node, add it
        if(NULL == pThis) {
            ce::list<IDLE_LIST>::iterator itIdle;
            if(idleList.end() == (itIdle = idleList.insert(idleList.begin()))) {
                return 0xFFFFFFFF;
            }
            itIdle->ulConnectionID = pAC->ConnectionID();
            itIdle->dwLastUsed = pAC->m_dwLastUsed;
            itIdle->pConn = pAC;

            pThis = &(*itIdle);

            ASSERT(pThis->ulConnectionID == pAC->ConnectionID());
            ASSERT(pThis->dwLastUsed == pAC->m_dwLastUsed);
        }
        ASSERT(NULL != pThis);
        ASSERT(pAC->ConnectionID() == pThis->ulConnectionID);

        //
        // If this current connection has been active more recently than the one in the
        //   list, update the list
        if(dwNow-pAC->m_dwLastUsed <= dwNow-pThis->dwLastUsed) {
            pThis->dwLastUsed = pAC->m_dwLastUsed;
            pThis->pConn = pAC;
        }
    }

    //
    // Now we have a list of the most recently used connections (and each
    //    connection is only represented once) find the one that was used
    //    the longest ago -- connections that are in use are off limits
    pThis = NULL;
    if(idleList.size()) {
        pThis = &(*idleList.begin());

        for(itIdleList=idleList.begin(); itIdleList!=idleList.end(); itIdleList++) {
           if(!itIdleList->pConn->HasOpenedResources() && (dwNow - pThis->dwLastUsed <= dwNow - (*itIdleList).dwLastUsed)) {
                pThis = &(*itIdleList);
           }
        }
    }

    if(pThis && (dwNow - pThis->dwLastUsed) >= dwIdleTime) {
       return pThis->ulConnectionID;
    } else {
        return 0xFFFFFFFF;
    }
}



HRESULT
ConnectionManager::AddConnection(SMB_PACKET *pSMB)
{
  CCritSection csLock(&m_MyCS);
  csLock.Lock();
  USHORT usUid;
  HRESULT hr;
  BOOL fNewConnection = TRUE;

  //
  // Check to see if we could be getting hacked
  if(NumGuests() > MAX_NUM_GUESTS) {
        RETAILMSG(1, (L"SMBSRV: we have too many guest users... someone probably is trying to DoS us"));
        ASSERT(FALSE); //<-- we could remove this ASSERT if its a prob for test (if DoS tests are made), its just here to make
                       //   sure we dont hide a problem if one exists
        hr = E_UNEXPECTED;
        goto Done;
  }

  //
  // Get a unique UID for them
  if(FAILED(m_UIDGenerator.GetID(&usUid))) {
      ASSERT(FALSE);
      hr = E_UNEXPECTED;
      goto Done;
  }

  //
  // Zero is a special number of UID -- dont give it out
  if(0 == usUid) {
      m_fZeroUIDTaken = TRUE;
      if(FAILED(m_UIDGenerator.GetID(&usUid))) {
           ASSERT(FALSE);
           hr = E_UNEXPECTED;
           goto Done;
       }
  }
  pSMB->pInSMB->Uid = usUid;

  //
  // Be super sure that we dont have multiple instances of this connection
  //   if we do, blast the old one
  hr = RemoveConnection(pSMB->ulConnectionID, usUid);
  ASSERT(FAILED(hr));

  m_MyConnections.push_front(new ActiveConnection);
  ((m_MyConnections.front()))->SetConnectionInfo(pSMB->ulConnectionID, usUid);

  hr = S_OK;

  Done:
    return hr;
}


//
// Return the # of active connections (there could be multiple SESSIONS on ONE connection)
UINT
ConnectionManager::NumConnections(UINT uiExemptConnection)
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<UINT> ConnList;
    ce::list<UINT>::iterator itConnList;
    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itAC;

    //
    // Count the # of unique connections
    for(itAC=m_MyConnections.begin(); itAC!=m_MyConnections.end(); ++itAC) {
        BOOL fInList = FALSE;
        for(itConnList=ConnList.begin(); itConnList!=ConnList.end(); ++itConnList) {
            if((*itAC)->ConnectionID() == (*itConnList)) {
                fInList = TRUE;
                break;
            }
        }
        if(!fInList && (*itAC)->ConnectionID() != uiExemptConnection) {
            if(!ConnList.push_back((*itAC)->ConnectionID())) {
                goto Done;
            }
        }
    }

    Done:
        return ConnList.size();
}

UINT
ConnectionManager::NumGuests()
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    UINT uiGuests = 0;

    for(it=m_MyConnections.begin(); it!=m_MyConnections.end(); it++) {
        if((*it)->IsGuest()) {
            uiGuests ++;
        }
    }
    return uiGuests;
}

HRESULT
ConnectionManager::TerminateTIDsOnShare(Share *pShare)
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    HRESULT hr = S_OK;
    for(it=m_MyConnections.begin(); it!=m_MyConnections.end(); it++) {
        if(FAILED(hr = (*it)->TerminateConnectionsForShare(pShare))) {
            goto Done;
        }
    }
    Done:
        return hr;
}


HRESULT
ConnectionManager::RemoveConnection(ULONG ulConnectionID, USHORT usUid)
{
    HRESULT hr = E_FAIL;
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    for(ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it=m_MyConnections.begin(); it!=m_MyConnections.end(); ) {
      ActiveConnection *pAC = *it;

      if(0xFFFF == usUid &&  pAC->ConnectionID() == ulConnectionID) {
            TRACEMSG(ZONE_DETAIL, (L"SMB_SRV: Killing ALL connections on transport connection: 0x%x", ulConnectionID));

            //
            // Give back the unique UID
            m_UIDGenerator.RemoveID(pAC->Uid());
            m_MyConnections.erase(it++);
            hr = S_OK;
      }
      else if(pAC->ConnectionID() == ulConnectionID && pAC->Uid() == usUid) {
            TRACEMSG(ZONE_DETAIL, (L"SMB_SRV: Killing specific connection on transport connection: 0x%x", ulConnectionID));

            //
            // Give back the unique UID
            m_UIDGenerator.RemoveID(pAC->Uid());
            m_MyConnections.erase(it++);
            hr = S_OK;
      } else {
            ++it;
      }
    }

    //
    // If usUid is 0xFFFF we are pulling the entire connection down
    //    so remove any CHALLENGE nodes
    if(0xFFFF == usUid) {
        hr = RemoveChallenge(ulConnectionID);
    }
    return hr;
}


HRESULT
ConnectionManager::AddChallenge(ULONG ulConnectionID, BYTE *pChallenge)
{
    HRESULT hr;
    CCritSection csLock(&m_MyCS);
    OldChallengeNode newNode;
    newNode.ulConnectionID = ulConnectionID;
    memcpy(newNode.Challenge, pChallenge, sizeof(newNode.Challenge));

    if(FAILED(hr = RemoveChallenge(ulConnectionID))) {
        goto Done;
    }

    csLock.Lock();
    if(!m_OldChallengeList.push_front(newNode)) {
        hr = E_OUTOFMEMORY;
    } else {
        hr = S_OK;
    }

    Done:
        return hr;
}

HRESULT
ConnectionManager::RemoveChallenge(ULONG ulConnectionID)
{
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<OldChallengeNode, OLD_CHALLENGE_ALLOC >::iterator  itChal = m_OldChallengeList.begin();
    IFDBG(UINT uiHits = 0);
    for(itChal = m_OldChallengeList.begin();
        itChal != m_OldChallengeList.end();) {

        if(ulConnectionID == (*itChal).ulConnectionID) {
            m_OldChallengeList.erase(itChal++);
            IFDBG(uiHits ++);
        } else {
            ++itChal;
        }
    }
    return S_OK;
}

HRESULT
ConnectionManager::FindChallenge(ULONG ulConnectionID, BYTE **pChallenge)
{
    HRESULT hr = E_FAIL;
    CCritSection csLock(&m_MyCS);
    csLock.Lock();

    ce::list<OldChallengeNode, OLD_CHALLENGE_ALLOC >::iterator  itChal = m_OldChallengeList.begin();
    for(itChal = m_OldChallengeList.begin();
        itChal != m_OldChallengeList.end();) {

        if(ulConnectionID == (*itChal).ulConnectionID) {
            *pChallenge = (*itChal).Challenge;
            hr = S_OK;
            break;
        } else {
            ++itChal;
        }
    }
    ASSERT(SUCCEEDED(hr));
    return hr;
}



VOID
ConnectionManager::ListConnectedUsers(ce::wstring &sRet)
{
  CCritSection csLock(&m_MyCS);
  csLock.Lock();
  //ce::wstring sRet;

  ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
  ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();


  for(it = m_MyConnections.begin(); it != itEnd; it++) {
       sRet += (*it)->UserName();
       sRet.resize(sRet.length() + 1);
  }

  //return sRet;
}


HRESULT
ConnectionManager::CancelWakeUpEvent(SMB_PACKET *pSMB, USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid)
{
    HRESULT hr = S_OK;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;


    //
    //    locking the connection manager ensures that active connections will not deadlock
    //    in subsequent calls that may cause SMB packets to get sent out on the network
    //
    CCritSection csLock(&m_MyCS);
    csLock.Lock();


    //
    //  find our connection state
    //
    pMyConnection = this->FindConnection( pSMB );
    if( !pMyConnection ) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: %s: -- cant find connection 0x%x!", __FUNCTION__, pSMB->ulConnectionID));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }


    //
    //  see if we can cancel the request
    //
    if(FAILED( hr = pMyConnection->CancelWakeUpEvent( usUid, usPid, usTid, usMid ) ) ) {
        TRACEMSG(ZONE_WARNING, (L"SMBSRV: %s: -- cant cancel wake up event 0x%x!", __FUNCTION__, pSMB->ulConnectionID));
        //
        //  it is possible that the wake up event has already been removed
        //  in which case we want to ignore this state and continue
        //
        hr = S_OK;
        goto Done;
    }

Done:
    return hr;
}


BOOL
ConnectionManager::SendPacket(SMB_PACKET *pSMB, ChangeNotificationWakeUpNode *pWakeUpNode)
{
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;


    //
    //    locking the connection manager ensures that active connections will not deadlock
    //    in subsequent calls that may cause SMB packets to get sent out on the network
    //
    CCritSection csLock(&m_MyCS);
    csLock.Lock();


    //
    //  find our connection state
    //
    pMyConnection = this->FindConnection( pSMB );
    if( !pMyConnection ) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: %s: -- cant find connection 0x%x!", __FUNCTION__, pSMB->ulConnectionID));
        ASSERT(FALSE);
        return FALSE;
    }

    return pMyConnection->SendPacket( pWakeUpNode );
}


#ifdef DEBUG
VOID
ConnectionManager::DebugPrint()
{
  HRESULT hr = E_FAIL;
  CCritSection csLock(&m_MyCS);
  csLock.Lock();

  ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
  ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();


  for(it = m_MyConnections.begin(); it != itEnd; it++) {
       (*it)->DebugPrint();
  }

}
#endif

