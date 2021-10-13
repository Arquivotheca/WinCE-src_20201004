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

#include "SMB_Globals.h"
#include "ShareInfo.h"
#include "FixedAlloc.h"

#define ACTIVE_CONN_FIND_FIRST_ALLOC              pool_allocator<10, FIND_FIRST_NODE>
#define ACTIVE_CONN_TID_ALLOC                     pool_allocator<10, USHORT>
#define CONNECTION_MANAGER_CONNECTION_ALLOC       pool_allocator<10, ActiveConnection>
#define OLD_CHALLENGE_ALLOC                       pool_allocator<10, OldChallengeNode>



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
        VOID SetConnectionInfo(ULONG ulConnectionID, USHORT usMid, USHORT usPid, USHORT usUid);
           
        ULONG ConnectionID();           
        USHORT Mid();
        USHORT Uid();
        USHORT Pid();        
        
        //
        // TID accessor functions (a TID contains its own FID information)
        HRESULT BindToTID(Share *pShare, TIDState **ppTIDState, BYTE *pPassword, USHORT usPassLen);
        HRESULT UnBindTID(USHORT usTid);
        HRESULT FindTIDState(USHORT usTid, TIDState **ppTIDState, DWORD dwAccessPerms);

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
        // For special cases (to fix bugs in other clients)
        VOID SetWindows2000();

        //
        // For Auth
        IFDBG(UINT uiAcceptCalled;);
        CredHandle Credentials; 
        CtxtHandle ContextHandle;
        BOOL       fContextSet;
        BOOL       fIsGuest;
        
    private:
        ULONG  m_ulConnectionID;        
        USHORT m_usMid;
        USHORT m_usUid;
        USHORT m_usPid;        
        BOOL   m_fIsUnicode;
        BOOL   m_fIsWindows2000;

        //
        // UserName
        StringConverter m_UserName;
        
        ce::list<FIND_FIRST_NODE, ACTIVE_CONN_FIND_FIRST_ALLOC > m_FindHandleList;
        ce::list<USHORT, ACTIVE_CONN_TID_ALLOC>                  m_MyTidList;
        
        //
        // Static privates
        static UniqueID    m_UIDFindFirstHandle;
        static TIDManager  m_TIDManager;
        
};

class ConnectionManager {
    public:
        ConnectionManager();
        ~ConnectionManager();

        ActiveConnection *FindConnection(SMB_PACKET *pSMB);
        HRESULT AddConnection(SMB_PACKET *pSMB);           
        HRESULT RemoveConnection(ULONG ulConnectionID, USHORT usMid, USHORT usPid, USHORT usUid);

        HRESULT AddChallenge(ULONG ulConnectionID, BYTE *pChallenge);
        HRESULT RemoveChallenge(ULONG ulConnectionID);
        HRESULT FindChallenge(ULONG ulConnectionID, BYTE **pChallenge);
        UINT    NumGuests();
        UINT    NumConnections();
     
    private:  
        ce::list<ActiveConnection, CONNECTION_MANAGER_CONNECTION_ALLOC > m_MyConnections;
        ce::list<OldChallengeNode, OLD_CHALLENGE_ALLOC>                  m_OldChallengeList;
        UniqueID           UIDGenerator;
        BOOL               fZeroUIDTaken; 
        UINT 			   uiUniqueConnections;
      
#ifdef DEBUG
        static BOOL fFirstInst;
#endif
};

#endif
