//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <sspi.h>
#include <allocator.hxx>
#include <block_allocator.hxx>
#include <auto_xxx.hxx>
#include <string.hxx>

#include "SMB_Globals.h"
#include "ShareInfo.h"
#include "FileServer.h"

#ifndef NO_POOL_ALLOC
#define ACTIVE_CONN_FIND_FIRST_ALLOC              ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(FIND_FIRST_NODE) >
#define ACTIVE_CONN_TID_ALLOC                     ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(USHORT) >
#define CONNECTION_MANAGER_CONNECTION_ALLOC       ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(ActiveConnection) >
#define OLD_CHALLENGE_ALLOC                       ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(OldChallengeNode) >
#define ACTIVE_CONN_PTR_LIST                      ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(ActiveConnection *) >
#else
#define ACTIVE_CONN_FIND_FIRST_ALLOC             ce::allocator
#define ACTIVE_CONN_TID_ALLOC                     ce::allocator
#define CONNECTION_MANAGER_CONNECTION_ALLOC     ce::allocator
#define OLD_CHALLENGE_ALLOC                       ce::allocator
#define ACTIVE_CONN_PTR_LIST                      ce::allocator
#endif


//
// Forward declare
struct SMB_PACKET;

struct FIND_FIRST_NODE {
    WIN32_FIND_DATA w32LastNode;
    WIN32_FIND_DATA w32TempNode;
    BOOL fLastNodeUsed;
    USHORT usSID;
    HANDLE hHandle;
    StringConverter SearchString;
    BOOL fGivenDot;
    BOOL fGivenDotDot;
};

struct OldChallengeNode {
    ULONG ulConnectionID;
    BYTE Challenge[8];
};

struct NotificationNode {
    //HANDLE h;
    //USHORT usID;
    WakeUpNode *pMyNode;
    BOOL fIsActive;

    USHORT usUid;
    USHORT usPid;
    USHORT usTid;
    USHORT usMid;
};

//
//  Use ActiveConnection to define anything that is specific to one
//     pending connection.   Feel free to put any kind of accessor functions
//     here (to add/remove/search) for anything you want.  When the connection
//     goes down for whatever reason the deconstructor will be called so you
//     need to put all cleanup code there
class ActiveConnection {
    public:
        ActiveConnection();
        ~ActiveConnection();

        VOID SetConnectionInfo(ULONG ulConnectionID, USHORT usUid);

        ULONG ConnectionID();
        USHORT Mid();
        USHORT Uid();

        //
        // Add a notification handle
        HRESULT AddWakeUpEvent(WakeUpNode *pNode, USHORT usId, USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid);
        HRESULT RemoveWakeUpEvent(USHORT usPid);
        HRESULT CancelWakeUpEvent(USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid);

        //
        // TID accessor functions (a TID contains its own FID information)
        HRESULT BindToTID(Share *pShare, ce::smart_ptr<TIDState> &pTIDState);
        HRESULT UnBindTID(USHORT usTid);
        HRESULT FindTIDState(USHORT usTid, ce::smart_ptr<TIDState> &pTIDState, DWORD dwAccessPerms);
        HRESULT TerminateConnectionsForShare(Share *pShare);

        //
        // FindFirst/Next/Close -- NOTE: NextFile doesnt advance!
        HRESULT CreateNewFindHandle(const WCHAR *pSearchString, USHORT *pusHandle, BOOL fUnicodeRules);
        HRESULT ResetSearch(USHORT usHandle);
        HRESULT NextFile(USHORT usHandle, WIN32_FIND_DATA **ppData);
        HRESULT AdvanceToNextFile(USHORT usHandle);
        HRESULT CloseFindHandle(USHORT usHandle);

        //
        // User name stuff
        HRESULT SetUserName(const WCHAR *pName);
        const WCHAR *UserName();

        //
        // Unicode support (or not)
        VOID SetUnicode(BOOL fStatus);
        BOOL SupportsUnicode(SMB_HEADER *pRequest, BYTE smb = 0xFF, BYTE subCmd = 0xFF);

        //
        //Guest stuff
        VOID SetGuest(BOOL fVal) {
            CCritSection csLock(&m_csConnectionLock);
            csLock.Lock();
            m_fIsGuest = fVal;
        }
        BOOL IsGuest() {
            CCritSection csLock(&m_csConnectionLock);
            csLock.Lock();
            return m_fIsGuest;
        }

        //
        // For Auth
        IFDBG(UINT m_uiAcceptCalled;);
        CredHandle m_Credentials;
        CtxtHandle m_ContextHandle;
        BOOL       m_fContextSet;


        //
        // For General info
        BOOL HasOpenedResources();
        DWORD  m_dwLastUsed;
        
        //
        //  sending response packets through the connection
        //
        BOOL SendPacket(ChangeNotificationWakeUpNode *pWakeUpNode);


        #ifdef DEBUG
            VOID DebugPrint();
        #endif

    private:
        ULONG  m_ulConnectionID;
        USHORT m_usUid;
        BOOL   m_fIsUnicode;
        BOOL       m_fIsGuest;

        //
        // UserName
        StringConverter m_UserName;

        ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC > m_FindHandleList;
        ce::list<ce::smart_ptr<TIDState>, SHARE_MANANGER_TID_ALLOC>           m_MyTidList;

        CRITICAL_SECTION m_csConnectionLock;
        ce::list<NotificationNode>                             m_NotificationNodeList;

        //
        // Static privates
        static UniqueID    m_UIDFindFirstHandle;
};

class ConnectionManager {
    public:
        ConnectionManager();
        ~ConnectionManager();

        ce::smart_ptr<ActiveConnection> FindConnection(SMB_PACKET *pSMB);
        HRESULT FindAllConnections(ULONG ulConnectionID, ce::list<ce::smart_ptr<ActiveConnection>, ACTIVE_CONN_PTR_LIST > &ConnList);
        HRESULT AddConnection(SMB_PACKET *pSMB);
        HRESULT RemoveConnection(ULONG ulConnectionID, USHORT usUid);

        HRESULT AddChallenge(ULONG ulConnectionID, BYTE *pChallenge);
        HRESULT RemoveChallenge(ULONG ulConnectionID);
        HRESULT FindChallenge(ULONG ulConnectionID, BYTE **pChallenge);
        ULONG FindStaleConnection(DWORD dwIdleTime);
        UINT    NumGuests();
        UINT    NumConnections(UINT uiExemptConnection);

        HRESULT TerminateTIDsOnShare(Share *pShare);
        VOID ListConnectedUsers(ce::wstring &sRet);

        HRESULT CancelWakeUpEvent(SMB_PACKET *pSMB, USHORT usUid, USHORT usPid, USHORT usTid, USHORT usMid);


        //
        //  send response packet
        //
        BOOL SendPacket(SMB_PACKET *pSMB, ChangeNotificationWakeUpNode *pWakeUpNode);

#ifdef DEBUG
         VOID DebugPrint();
#endif

    private:
        ce::list<ce::smart_ptr<ActiveConnection> , CONNECTION_MANAGER_CONNECTION_ALLOC > m_MyConnections;
        ce::list<OldChallengeNode, OLD_CHALLENGE_ALLOC>                  m_OldChallengeList;

        UniqueID           m_UIDGenerator;
        BOOL               m_fZeroUIDTaken;
        CRITICAL_SECTION  m_MyCS;

#ifdef DEBUG
        static BOOL m_fFirstInst;
#endif
};

#endif
