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

TIDManager ActiveConnection::m_TIDManager;
UniqueID   ActiveConnection::m_UIDFindFirstHandle;

#ifdef DEBUG
BOOL ConnectionManager::fFirstInst = TRUE;
#endif

#define MAX_NUM_GUESTS      20


ActiveConnection::ActiveConnection():m_ulConnectionID(0xFFFFFFFF),
                                   m_fIsUnicode(FALSE), fContextSet(FALSE), 
                                   fIsGuest(TRUE),
                                   m_fIsWindows2000(FALSE)
{
    IFDBG(uiAcceptCalled = 0;);
}

ActiveConnection::~ActiveConnection()
{     
    //
    // Destroy any handles we may have
    while(m_FindHandleList.size()) {
        ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC>::iterator it = m_FindHandleList.begin();   
        ActiveConnection::CloseFindHandle((*it).usSID);   
    }
    
    //
    // Destroy all TID's that we have out
    while(m_MyTidList.size()) {
        ce::list<USHORT, ACTIVE_CONN_TID_ALLOC >::iterator it = m_MyTidList.begin();
        UnBindTID((*it));
    }
    
    //
    // Destroy any credentials we may still have
    if(TRUE == fContextSet) {
        FreeCredentialsHandle(&(Credentials));
        DeleteSecurityContext(&(ContextHandle));        
    }    
}


VOID 
ActiveConnection::SetWindows2000()
{
    m_fIsWindows2000 = TRUE;
}


VOID 
ActiveConnection::SetUnicode(BOOL fStatus)
{   
    m_fIsUnicode = fStatus;
}

BOOL 
ActiveConnection::SupportsUnicode(SMB_HEADER *pRequest, BYTE smb, BYTE subCmd) 
{    
    if(m_fIsWindows2000) {         
        if(SMB_COM_TREE_CONNECT_ANDX == smb) {
            return TRUE;
        }
        if(SMB_COM_TRANSACTION2 == smb && TRANS2_FIND_NEXT2 == subCmd) {
            return TRUE;
        }
        if(SMB_COM_TRANSACTION2 == smb && TRANS2_FIND_FIRST2 == subCmd) {
            return TRUE;
        }        
    }
    if(0 == (pRequest->Flags2 & SMB_FLAGS2_UNICODE)) {
        return FALSE;
    }
    else { 
        return TRUE;
    }
}

VOID
ActiveConnection::SetConnectionInfo(ULONG ulConnectionID, USHORT usMid, USHORT usPid, USHORT usUid)
{
    m_ulConnectionID = ulConnectionID;
    m_usUid = usUid;
    m_usMid = usMid;
    m_usPid = usPid;
    
}

ULONG
ActiveConnection::ConnectionID()
{
    return m_ulConnectionID;
}
    
USHORT 
ActiveConnection::Mid()
{
    return m_usMid;
}

USHORT
ActiveConnection::Uid()
{
    return m_usUid;
}

USHORT 
ActiveConnection::Pid()
{
    return m_usPid;
}



HRESULT 
ActiveConnection::BindToTID(Share *pShare, 
                           TIDState **ppTIDState, 
                           BYTE *pPassword, 
                           USHORT usPassLen)
{
    HRESULT hr;    
    hr = m_TIDManager.BindToTID(pShare,ppTIDState,pPassword,usPassLen);
    
    if(SUCCEEDED(hr)) {
        m_MyTidList.push_back((*ppTIDState)->TID());
    }
    
    return hr;
}

HRESULT 
ActiveConnection::UnBindTID(USHORT usTid)
{
    ce::list<USHORT, ACTIVE_CONN_TID_ALLOC >::iterator it; 
    ce::list<USHORT, ACTIVE_CONN_TID_ALLOC >::iterator itEnd = m_MyTidList.end();    
    ASSERT(0xFFFF != usTid );
    
    HRESULT hr = E_FAIL;
    BOOL fFound = FALSE;

    //
    // Find our node
    for(it = m_MyTidList.begin(); it != itEnd; it++) {
        if((*it) == usTid) {
            fFound = TRUE;
            m_MyTidList.erase(it);
            break;
        }    
    }
    
    if(TRUE == fFound) {
        hr = m_TIDManager.UnBindTID(usTid);
    } else {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: someone is trying to unbind a TID they dont own!  (being hacked?)"));            
    }  
    return hr;
}

HRESULT 
ActiveConnection::FindTIDState(USHORT usTid, TIDState **ppTIDState, DWORD dwAccessPerms)
{
    ce::list<USHORT, ACTIVE_CONN_TID_ALLOC >::iterator it; 
    ce::list<USHORT, ACTIVE_CONN_TID_ALLOC >::iterator itEnd = m_MyTidList.end();    
    ASSERT(0xFFFF != usTid );
    
    HRESULT hr = E_FAIL;
    BOOL fFound = FALSE;

    //
    // Find our node
    for(it = m_MyTidList.begin(); it != itEnd; it++) {
        if((*it) == usTid) {
            fFound = TRUE;
            break;
        }    
    }
    
    if(TRUE == fFound) {    
        if(SUCCEEDED(hr = m_TIDManager.FindTIDState(usTid, ppTIDState))) {
            if(E_ACCESSDENIED == (hr = ((*ppTIDState)->GetShare()->VerifyPermissions(UserName(), dwAccessPerms)))) {
                TRACEMSG(ZONE_SECURITY, (L"SMB_SRVR: user denied access to share state!"));
                *ppTIDState = NULL;
            }
        }        
    } else {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: someone is going after a TID they dont own!  (being hacked?)"));
        *ppTIDState = NULL;
    }
    
    return hr;
}



HRESULT 
ActiveConnection::CreateNewFindHandle(const WCHAR *pSearchString, USHORT *pusHandle, BOOL fUnicodeRules)
{
    HANDLE hSearch = INVALID_HANDLE_VALUE;
    USHORT usHandle = 0xFFFF;
    HRESULT hr; 
    WIN32_FIND_DATA w32FindData;
    ASSERT(NULL != pSearchString && NULL != pusHandle);
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
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator it; 
    ce::list<FIND_FIRST_NODE,ACTIVE_CONN_FIND_FIRST_ALLOC >::iterator itEnd = m_FindHandleList.end();
    ASSERT(0xFFFF != usHandle && NULL != ppData);
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
    m_UserName.Clear();
    TRACEMSG(ZONE_SECURITY, (L"SMB_SRV:  Setting username to %s", pName));
    return m_UserName.append(pName);
}

const WCHAR *
ActiveConnection::UserName()
{
    return m_UserName.GetString();
}

ConnectionManager::ConnectionManager() : fZeroUIDTaken(FALSE),uiUniqueConnections(0) 
{
    #ifdef DEBUG
        //
        // Just used to make sure (in debug) that multiple instances of the connection manager
        //   dont get created
        if(fFirstInst) {
            fFirstInst = FALSE;
        }
        else {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"Only one instance of ConnectionManager allowed!!!"));
        }
    #endif     
}

ConnectionManager::~ConnectionManager()
{
    if(TRUE == fZeroUIDTaken) {
        //
        // Give back the zero UID 
        if(FAILED(UIDGenerator.RemoveID(0))) {
            ASSERT(FALSE);
        } 
    }
#ifdef DEBUG
    ASSERT(0 == m_MyConnections.size());    
    fFirstInst = TRUE;
#endif
}

ActiveConnection *
ConnectionManager::FindConnection(SMB_PACKET *pSMB)
{
    ActiveConnection *pRet = NULL;
    ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator itEnd = m_MyConnections.end();   
    
    for(it=m_MyConnections.begin(); it!=itEnd; it++) {
        if((*it).ConnectionID() == pSMB->ulConnectionID &&
           (*it).Uid() == pSMB->pInSMB->Uid) {
            pRet = &(*it);
            goto Done;
        }
    }
        
    
    Done:
        return pRet;     
}

HRESULT 
ConnectionManager::AddConnection(SMB_PACKET *pSMB)
{
  ActiveConnection acNew;
  USHORT usUid;
  HRESULT hr;
  ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
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
  if(FAILED(UIDGenerator.GetID(&usUid))) {
      ASSERT(FALSE);
      hr = E_UNEXPECTED;
      goto Done;
  }
  
  //
  // Zero is a special number of UID -- dont give it out
  if(0 == usUid) {
      fZeroUIDTaken = TRUE;
      if(FAILED(UIDGenerator.GetID(&usUid))) {
           ASSERT(FALSE);
           hr = E_UNEXPECTED;
           goto Done;
       }
  }
  pSMB->pInSMB->Uid = usUid;
  
  //
  // Be super sure that we dont have multiple instances of this connection
  //   if we do, blast the old one
  hr = RemoveConnection(pSMB->ulConnectionID, 
                                pSMB->pInSMB->Mid, 
                                pSMB->pInSMB->Pid, usUid);
  ASSERT(FAILED(hr));   
  acNew.SetConnectionInfo(pSMB->ulConnectionID, 
                          pSMB->pInSMB->Mid, 
                          pSMB->pInSMB->Pid, usUid);

  //
  // Update # of active connections
  for(it=m_MyConnections.begin(); it!=m_MyConnections.end(); ++it) {
        if((*it).ConnectionID() == pSMB->ulConnectionID) {
                fNewConnection = FALSE;
                break;
        }                
  }
  if(TRUE == fNewConnection) {
        uiUniqueConnections ++;
  }
 
  m_MyConnections.push_back(acNew);  
  hr = S_OK;
  
  Done:
    return hr;  
}


//
// Return the # of active connections (there could be multiple SESSIONS on ONE connection)
UINT 
ConnectionManager::NumConnections() 
{
    ASSERT(uiUniqueConnections <= m_MyConnections.size());
    return uiUniqueConnections;    
}

UINT 
ConnectionManager::NumGuests()
{
    ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
    UINT uiGuests = 0;
    
    for(it=m_MyConnections.begin(); it!=m_MyConnections.end(); it++) {
        if(TRUE == (*it).fIsGuest) { 
            uiGuests ++;
        }
    }
    return uiGuests;
}

HRESULT 
ConnectionManager::RemoveConnection(ULONG ulConnectionID, USHORT usMid, USHORT usPid, USHORT usUid)
{  
    HRESULT hr = E_FAIL;
   
    for(ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it=m_MyConnections.begin(); it!=m_MyConnections.end(); ) {
    
      if(0xFFFF == usUid &&  (*it).ConnectionID() == ulConnectionID) {
            TRACEMSG(ZONE_DETAIL, (L"SMB_SRV: Killing ALL connections on transport connection: 0x%x", ulConnectionID));
                 
            //
            // Give back the unique UID 
            UIDGenerator.RemoveID((*it).Uid());            
            m_MyConnections.erase(it++);
            hr = S_OK;
      } 
      else if((*it).ConnectionID() == ulConnectionID && (*it).Uid() == usUid) {
            TRACEMSG(ZONE_DETAIL, (L"SMB_SRV: Killing specific connection on transport connection: 0x%x", ulConnectionID));
            
            //
            // Give back the unique UID 
            UIDGenerator.RemoveID((*it).Uid());            
            m_MyConnections.erase(it++);    
            hr = S_OK;
      } else {
            ++it;
      }
    }
    
    if(SUCCEEDED(hr)) {
          BOOL fFound = FALSE;
          ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC >::iterator it;
          
          //
          // Update # of active connections
          for(it=m_MyConnections.begin(); it!=m_MyConnections.end(); ++it) {
                if((*it).ConnectionID() == ulConnectionID) {
                        fFound = TRUE;
                        break;
                }                
          }
          if(FALSE == fFound) {
                uiUniqueConnections --;
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
    OldChallengeNode newNode;
    newNode.ulConnectionID = ulConnectionID;
    memcpy(newNode.Challenge, pChallenge, sizeof(newNode.Challenge));
    
    if(FAILED(hr = RemoveChallenge(ulConnectionID))) {
        goto Done;
    }
    
    m_OldChallengeList.push_front(newNode);
    
    //
    // Success
    hr = S_OK;
    Done:    
        return hr;
}

HRESULT 
ConnectionManager::RemoveChallenge(ULONG ulConnectionID)
{
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



