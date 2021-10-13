//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <CReg.hxx>
#include <authhlp.h>

#include "SMB_Globals.h"
#include "ShareInfo.h"
#include "CriticalSection.h"
#include "encrypt.h"
#include "PrintQueue.h"
#include "FileServer.h"
#include "ConnectionManager.h"


//
// Just used to make sure (in debug) that multiple instances of ShareManager/TIDManager
//   dont get created
#ifdef DEBUG
        BOOL ShareManager::fFirstInst = TRUE;
        BOOL TIDManager::fFirstInst = TRUE;
#endif

UniqueID                           TIDState::UIDTidGenerator;
pool_allocator<10, FileStream>     SMB_Globals::g_FileStreamAllocator;
pool_allocator<10, PrintStream>    SMB_Globals::g_PrintStreamAllocator;
pool_allocator<10, TIDState>       SMB_Globals::g_TIDStateAllocator;



Share::Share() {
       
    pShareName = NULL;
    pShareNameSZ = NULL;
        
    pRemark = NULL;
    fHidden = FALSE;
    shareType = 0xFF;
    
    pPrintQueue = NULL;
    fRequireAuth = TRUE;
}

Share::~Share() {
    if(pShareName)
        delete [] pShareName;
    if(pShareNameSZ)
        delete [] pShareNameSZ; 
    if(pRemark)
        delete [] pRemark;
    if(pPrintQueue)
        delete pPrintQueue;
}


HRESULT
Share::SetPrintQueue(SMBPrintQueue *_pPrintQueue)
{
    ASSERT(NULL == pPrintQueue);
    pPrintQueue = _pPrintQueue;
    
    return S_OK;
}

SMBPrintQueue *
Share::GetPrintQueue()
{
    return pPrintQueue;
}

HRESULT 
Share::SetRemark(const CHAR *_pRemark)
{
    HRESULT hr;
    UINT uiRemarkLen;
    
    if(NULL == _pRemark) {
        hr = E_INVALIDARG;
        goto Done;
    }
    if(NULL != pRemark) {
        delete [] pRemark;
    }
    
    uiRemarkLen = strlen(_pRemark) + 1;
    pRemark = new CHAR[uiRemarkLen];
    
    if(NULL == pRemark) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }
    memcpy(pRemark, _pRemark, uiRemarkLen);
    
    Done: 
        return hr;
    
}


       

HRESULT 
Share::SetShareName(const WCHAR *_pShareName)
{
    HRESULT hr;
    WCHAR *pOldShare = pShareName;
    CHAR  *pOldShareSZ = pShareNameSZ;
    UINT  uiSize = wcslen(_pShareName) + 1;    
    UINT uiMultiSize = WideCharToMultiByte(CP_ACP, 0, _pShareName, -1, NULL, NULL, NULL, NULL); 

    //
    // Share name must be < 13 bytes total (with null)
    if(uiSize > 13) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRV: ERROR: Sharename must be < 13 characters with null"));
        ASSERT(FALSE); 
        return E_INVALIDARG;
    }
    if(NULL != pShareName) {
        delete [] pShareName;
    }
    if(NULL != pShareNameSZ) {
        delete [] pShareNameSZ;
    }
    
    pShareName = new WCHAR[uiSize];
    pShareNameSZ = new CHAR[uiMultiSize];
    
    if(NULL == pShareName || NULL == pShareNameSZ) {
        hr = E_OUTOFMEMORY;         
        goto Done;
    }
            
    WideCharToMultiByte(CP_ACP, 0, _pShareName, -1, pShareNameSZ, uiMultiSize,NULL,NULL);
    wcscpy(pShareName, _pShareName);
    
    if(pOldShare)
        delete [] pOldShare;
    if(pOldShareSZ)    
        delete [] pOldShareSZ;
    hr = S_OK; 
         
    Done:
        if(FAILED(hr)) {
           if(pShareName)
                delete [] pShareName;
           if(pShareNameSZ)
                delete [] pShareNameSZ;
           pShareName = pOldShare;
           pShareNameSZ = pOldShareSZ;
        }
        return hr;
}




HRESULT 
Share::SetDirectoryName(const WCHAR *_pDirectory)
{
    m_DirectoryName.Clear();
    return m_DirectoryName.append(_pDirectory);
}

HRESULT 
Share::SetServiceName(const WCHAR *_pServiceName)
{
    m_ServiceName.Clear();
    return m_ServiceName.append(_pServiceName);
}


const WCHAR *
Share::GetServiceName()
{
    return m_ServiceName.GetString();
}

const CHAR *       
Share::GetShareNameSZ() 
{
    return pShareNameSZ;
}

const WCHAR *
Share::GetShareName()
{
    return pShareName;
}

const WCHAR *
Share::GetDirectoryName()
{
    return m_DirectoryName.GetString();
}

const CHAR *
Share::GetRemark() 
{
    return pRemark;
}

BYTE 
Share::GetShareType()
{
    return shareType;
}

HRESULT 
Share::SetShareType(BYTE _shareType)
{
    shareType = _shareType;
    return S_OK;
}

HRESULT 
Share::VerifyPermissions(const WCHAR *pUserName, DWORD dwAccessPerms)
{    
   if(SUCCEEDED(AllowUser(pUserName))) {
        return S_OK;
   } else {
        return E_ACCESSDENIED;
   }
}

HRESULT 
Share::AllowUser(const WCHAR *pUserName)
{
    if(TRUE == IsAccessAllowed((WCHAR *)pUserName, NULL, (WCHAR *)(m_ACLList.GetString()), FALSE)) {
        return S_OK;
    } else {
        CReg RegAllowAll;
        BOOL fAllowAll = FALSE;
        if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
            fAllowAll = RegAllowAll.ValueDW(L"NoSecurity", FALSE);
            if(TRUE == fAllowAll) {
               return S_OK;
            }
        }
        return E_ACCESSDENIED;
    }
    return S_OK;
}
HRESULT 
Share::SetACL(const WCHAR *pACL) {
    HRESULT hr = m_ACLList.Clear();
    if(FAILED(hr)) {
        return hr;
    }
    return m_ACLList.append(pACL);
}

HRESULT
Share::IsValidPath(const WCHAR *pPath)
{
    if(0 != wcsstr(pPath, L"..\\")) {
        RETAILMSG(1, (L"SMBSRV: Invalid char in path %s\n", pPath));
        return E_FAIL;
    } else if(0 != wcsstr(pPath, L"\\..")) {
        RETAILMSG(1, (L"SMBSRV: Invalid char in path %s\n", pPath));
        return E_FAIL;
    } else if(0 != wcsstr(pPath, L"\\\\")) {
        RETAILMSG(1, (L"SMBSRV: Invalid char in path %s\n", pPath));
        return E_FAIL;
    }
    else {
        return S_OK;
    }
}

TIDManager::TIDManager() 
{
#ifdef DEBUG
    //
    // Just used to make sure (in debug) that multiple instances of TIDManager
    //   dont get created
    if(fFirstInst) {
        fFirstInst = FALSE;
    }
    else {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"Only one instance of TIDManager allowed!!!"));
    }
#endif 
    InitializeCriticalSection(&csTIDManager);
}

TIDManager::~TIDManager()
{
    //
    // Just used to make sure (in debug) that multiple instances of TIDManager
    //   dont get created
#ifdef DEBUG
    ASSERT(FALSE == fFirstInst);
    fFirstInst = TRUE;
#endif      

    //clear our the share lists
    CCritSection csLock(&csTIDManager);
    csLock.Lock();
    
    //
    // Delete all TIDStates that are outstanding (there should be NONE!)
    ASSERT(0 == ListOfBoundShares.size());
    while(ListOfBoundShares.size()) {
        TIDState *pToDel = ListOfBoundShares.front();
        ASSERT(pToDel);
        ListOfBoundShares.pop_front();
        delete pToDel;
    }
    
    csLock.UnLock();
    DeleteCriticalSection(&csTIDManager);
}


HRESULT 
TIDManager::UnBindTID(USHORT usTid)
{
    HRESULT hr = E_FAIL;
    CCritSection csLock(&csTIDManager);
    csLock.Lock();
    
    //find our Tid
    ce::list<TIDState *, SHARE_MANANGER_TID_ALLOC >::iterator it;
    ce::list<TIDState *, SHARE_MANANGER_TID_ALLOC >::iterator itEnd;
   
  
    for(it = ListOfBoundShares.begin(), itEnd = ListOfBoundShares.end(); it != itEnd; ++it) 
    {
        if((*it)->TID() == usTid) {
            TIDState *pToDel = *it;
            ListOfBoundShares.erase(it);
            delete pToDel;
            hr = S_OK;
            break;
        }
    }
    
    if(SUCCEEDED(hr)) {      
        TRACEMSG(ZONE_DETAIL, (L"SMBSRV: TIDManager -- unbound Tid (0x%x)?", usTid));
    } else {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TIDManager -- couldnt unbind Tid(0x%x)!!?", usTid)); 
    }
    return hr;
}


HRESULT 
TIDManager::BindToTID(Share *pShare, TIDState **ppTIDState, BYTE *password, USHORT usPassLen)
{        
    HRESULT hr = E_FAIL;
    TIDState *pNewState = NULL;
    CCritSection csLock(&csTIDManager);
    if(NULL == pShare || NULL == ppTIDState) {
        ASSERT(FALSE); //<-- this is an internal function!  should never be misused!
        return E_INVALIDARG;
    }
        
    csLock.Lock();        
     
    //
    // Using this new TID, create a container for this transmission
    if(NULL == (pNewState = new TIDState(pShare))) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }    
    ListOfBoundShares.push_back(pNewState);
    hr = S_OK; 
        
Done:             
    if(SUCCEEDED(hr)) {      
      TRACEMSG(ZONE_DETAIL, (L"SMBSRV: TIDManager -- using Tid (0x%x)?", pNewState->TID()));
    } else {
        TRACEMSG(ZONE_DETAIL, (L"SMBSRV: TIDManager -- couldnt make new TIDState!!?"));
        if(NULL != pNewState) {
            delete pNewState;
            pNewState = NULL;
        }
    }        
    *ppTIDState = pNewState;
    return hr;
}


HRESULT 
TIDManager::FindTIDState(USHORT usTid, TIDState **ppTIDState)
{
    HRESULT hr = E_FAIL;   
    TIDState *pMyState = NULL;
    
    if(NULL == ppTIDState) {
        ASSERT(FALSE); //<-- this is an internal function!  should never be misused!
        return E_INVALIDARG;
    }
    
    CCritSection csLock(&csTIDManager);
    csLock.Lock();
    
    //go find the Tid
    ce::list<TIDState *, SHARE_MANANGER_TID_ALLOC >::iterator it;
    ce::list<TIDState *, SHARE_MANANGER_TID_ALLOC>::iterator itEnd;
   
    for(it = ListOfBoundShares.begin(), itEnd = ListOfBoundShares.end(); it != itEnd; ++it) 
    {
        if((*it)->TID() == usTid) {
            pMyState = (*it);
            hr = S_OK;
            goto Done;
        }    
    }
    
    Done:        
        if(SUCCEEDED(hr)) {      
          TRACEMSG(ZONE_DETAIL, (L"SMBSRV: TIDManager -- found state for Tid (0x%x)?", usTid));
        } else {
            TRACEMSG(ZONE_DETAIL, (L"SMBSRV: TIDManager -- couldnt get find state for Tid (0x%x)!!?", usTid));        
        }
        *ppTIDState = pMyState;
        return hr;
}




/******************************************************************************
 *
 *  ShareManager -- this utility class manages shares on the system.  
 *     The class is used to collect functions related to which shares are on the 
 *     system
 *
 ******************************************************************************/
ShareManager::ShareManager() 
{
#ifdef DEBUG
    //
    // Just used to make sure (in debug) that multiple instances of ShareManager
    //   dont get created
    if(fFirstInst) {
        fFirstInst = FALSE;
    }
    else {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"Only one instance of ShareManager allowed!!!"));
    }
#endif 
    InitializeCriticalSection(&csShareManager);
}

ShareManager::~ShareManager()
{
    //
    // Just used to make sure (in debug) that multiple instances of ShareManager
    //   dont get created
#ifdef DEBUG
    ASSERT(FALSE == fFirstInst);
    fFirstInst = TRUE;
#endif      

    //clear our the share lists
    CCritSection csLock(&csShareManager);
    csLock.Lock();
       
    //
    // Delete all Shares that are outstanding
    while(MasterListOfShares.size()) {
        Share *pToDel = MasterListOfShares.front();
        ASSERT(pToDel);
        MasterListOfShares.pop_front();
        delete pToDel;
    }   
    csLock.UnLock();
    DeleteCriticalSection(&csShareManager);       
}

HRESULT 
ShareManager::AddShare(Share *pShare) 
{
    if(NULL == pShare) 
        return E_INVALIDARG;
    CCritSection csLock(&csShareManager);
    csLock.Lock();
    MasterListOfShares.push_front(pShare);
    return S_OK;       
}


Share *
ShareManager::SearchForShare(const WCHAR *pName) 
{
    Share *pRet = NULL;
    if(NULL == pName) {
        ASSERT(FALSE);
        return NULL;
    }
    CCritSection csLock(&csShareManager);
    csLock.Lock();

    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator itEnd = MasterListOfShares.end();
    
    for(it = MasterListOfShares.begin(); it != itEnd; ++it) {
        const WCHAR *pShareName = (*it)->GetShareName();
        ASSERT(pShareName);

        if(pShareName && 0 == _wcsicmp(pShareName, pName)) {
            pRet = *it;
            goto Done;
        }    
    }    
    Done: 
        return pRet;
}


Share *
ShareManager::SearchForShare(UINT uiIdx) {    
    Share *pRet = NULL;
    
    CCritSection csLock(&csShareManager);
    csLock.Lock();
    
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator itEnd = MasterListOfShares.end();
    
    if(uiIdx+1 > MasterListOfShares.size()) {
        pRet = NULL;
        goto Done;
    }
      
    for(it = MasterListOfShares.begin(); it != itEnd, uiIdx; ++it, uiIdx--) {
    } 
    pRet = *it;
    
    Done:
        return pRet;  
}




UINT
ShareManager::NumShares()
{
    UINT uiSize;
    CCritSection csLock(&csShareManager);
    csLock.Lock();
        uiSize = MasterListOfShares.size();    
    return uiSize;
}


UINT 
ShareManager::NumVisibleShares() 
{
    UINT uiSize = 0;
    CCritSection csLock(&csShareManager);
    csLock.Lock();
    
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator itEnd = MasterListOfShares.end();

    for(it = MasterListOfShares.begin(); it != itEnd; ++it) {
        if((*it)->IsHidden())
            uiSize ++;
    }   
    return uiSize;
}


//
// NOTE: to use this function, you *MUST* Release() the pJobRet
HRESULT
ShareManager::SearchForPrintJob(USHORT usJobID, PrintJob **pJobRet, SMBPrintQueue **pQueueRet)
{
    HRESULT hr = E_FAIL;
   
    CCritSection csLock(&csShareManager);
    csLock.Lock();

    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator itEnd = MasterListOfShares.end();
    PrintJob *pRet = NULL;
    
    for(it = MasterListOfShares.begin(); it != itEnd; ++it) {
        SMBPrintQueue *pQueue = (*it)->GetPrintQueue();
        
        if(pQueue && SUCCEEDED(pQueue->FindPrintJob(usJobID, &pRet))) {     
            if(pJobRet)
                *pJobRet = pRet;
            if(pQueueRet)
                *pQueueRet = pQueue;
            hr = S_OK;
            break;
        }          
    } 
    return hr;
}





TIDState::TIDState(Share *_pShare):pShare(NULL), 
                    usTid(0xFFFF)
{    
    ASSERT(0 == FIDList.size());
         
    //ONLY init ONCE!
    ASSERT(NULL == pShare && 0xFFFF == usTid);
    
    //
    // Get a fresh TID
    if(FAILED(UIDTidGenerator.GetID(&usTid))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: Error -- TIDState couldnt get fresh TID!"));
        ASSERT(FALSE);
        return;
    }
    
    pShare = _pShare;
}

TIDState::~TIDState() 
{
    //
    // Clean up all FIDS as the manager is going down (most likely because
    //   the TID manager is pulling us down... maybe the connction died
    //   or the server is being stopped)
    while(FIDList.size()) {
        SMBFileStream *pToDel = FIDList.front();
        FIDList.pop_front();
        delete pToDel;
    }
    
    ASSERT(0 == FIDList.size());
        
    //
    // Free up our TID
    UIDTidGenerator.RemoveID(usTid);
}    
   
USHORT 
TIDState::TID() {
    return usTid;
}


Share *
TIDState::GetShare() {
    return pShare;
}
  
HRESULT 
TIDState::RemoveFileStream(USHORT usJobID)
{
    ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator it = FIDList.begin();
    ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();  
    BOOL fFound = FALSE;
    
    while(it != itEnd) {
        ASSERT(NULL != *it);
        if(NULL != *it && usJobID == (*it)->FID())  {
            SMBFileStream *pToDel = *it;            
            FIDList.erase(it++);
            delete pToDel;            
            fFound = TRUE;
            break;
        }
        it++;
    }
    
    if(fFound) 
        return S_OK;
    else
        return E_FAIL;
}


HRESULT 
TIDState::AddFileStream(SMBFileStream *pConnection)
{
    FIDList.push_front(pConnection);
    return S_OK;
}


HRESULT 
TIDState::FindFileStream(USHORT usFID, SMBFileStream **ppStream)
{
    HRESULT hr = E_FAIL;
    ASSERT(NULL != ppStream);
    
    *ppStream = NULL;
    ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator it;
    ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();

    for(it = FIDList.begin(); it != itEnd; ++it) {
        SMBFileStream *pFound = *it;
        ASSERT(NULL != pFound);
        if(pFound && usFID == pFound->FID()) {  
            *ppStream = pFound;
            hr = S_OK;
            break;
        }          
    }     
    return hr;
} 


// 
// Class factory for building stream -- it builds the stream based on
//   the Share attached to the TIDState
SMBFileStream *        
TIDState::CreateFileStream(ActiveConnection *pMyConnection) 
{    
    SMBFileStream *pRet = NULL;
    Share *pShare = this->GetShare();
    PrintJob *pNewJob = NULL;
    
    // 
    // Make sure this is actually a print queue and fetch queue    
    if(g_fPrintServer && STYPE_PRINTQ == pShare->GetShareType()){ 
        TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_OpenX -- the TID is for a printer!!"));                 
    }
    else if(g_fFileServer && STYPE_DISKTREE == pShare->GetShareType()) {
        TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_OpenX -- the TID is for file server!!"));
    }
    else if(STYPE_IPC == pShare->GetShareType()) {
        TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_OpenX -- the TID is for IPC!!"));
    }
    else {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- the Tid is for something we dont understand!!"));           
        goto Done;
    }  
    
    //
    // If the TIDState is for a print share, give them back a print stream
    if(STYPE_PRINTQ == pShare->GetShareType()) {     

        //
        // Create a new print job    
        if(NULL == (pNewJob = new PrintJob(this->GetShare()))) {
            goto Done;
        }    
        
        //
        // Set our username
        if(FAILED(pNewJob->SetOwnerName(pMyConnection->UserName()))) {
            goto Done;
        }        
        
        //
        // Create a new PrintStream and then add the job to TIDState        
        if(NULL == (pRet = new PrintStream(pNewJob,this))) {       
           goto Done;
        }       
    } else if(STYPE_DISKTREE == pShare->GetShareType()) {  
        if(NULL == (pRet = GetNewFileStream(this))) {
            goto Done;
        }       
    } else if(STYPE_IPC == pShare->GetShareType()) {
        goto Done;
    }
    
    Done:
        if(NULL != pNewJob) {
            pNewJob->Release();
        }
        return pRet;
}



HRESULT 
TIDState::Read(USHORT usFID,
             BYTE *pDest, 
             DWORD dwOffset, 
             DWORD dwReqSize, 
             DWORD *pdwRead)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find FID:0x%x on Read()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Read(pDest, dwOffset, dwReqSize, pdwRead))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Read() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}
 
 HRESULT 
 TIDState::Write(USHORT usFID,
              BYTE *pSrc, 
              DWORD dwOffset, 
              DWORD dwReqSize, 
              DWORD *pdwWritten)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find FID:0x%x on Write()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Write(pSrc, dwOffset, dwReqSize, pdwWritten))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Write() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}
 
HRESULT 
TIDState::Open(USHORT usFID,
              const WCHAR *pFileName,                              
              DWORD dwAccess, 
              DWORD dwDisposition, 
              DWORD dwAttributes, 
              DWORD dwShareMode)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on Open()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Open(pFileName, dwAccess, dwDisposition, dwAttributes, dwShareMode))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Open() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}

HRESULT 
TIDState::IsLocked(USHORT usFID, 
                  USHORT usPID,
                  ULONG Offset,
                  ULONG Length,
                  BOOL *pfLocked)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: IsLocked(TID:0x%x) cant find (FID:0x%x) on IsLocked()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->IsLocked(usPID, Offset, Length, pfLocked))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: IsLocked(TID:0x%x) (FID:0x%x) operation IsLocked() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;

    Done:
        return hr;
}

HRESULT 
TIDState::Lock(USHORT usFID, 
               USHORT usPID,
               ULONG Offset,
               ULONG Length)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: Lock(TID:0x%x) cant find (FID:0x%x) on Lock()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Lock(usPID, Offset, Length))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: Lock(TID:0x%x) (FID:0x%x) operation Lock() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;

    Done:
        return hr;
}

HRESULT 
TIDState::UnLock(USHORT usFID, 
                 USHORT usPID,
                 ULONG Offset,
                 ULONG Length)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: UnLock(TID:0x%x) cant find (FID:0x%x) on UnLock()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->UnLock(usPID, Offset, Length))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: UnLock(TID:0x%x) (FID:0x%x) operation UnLock() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;    
    Done: 
        return hr;
}


HRESULT 
TIDState::SetEndOfStream(USHORT usFID,
                       DWORD dwOffset)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on SetEndOfStream()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->SetEndOfStream(dwOffset))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation SetEndOfStream() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}
 
 
                          
HRESULT 
TIDState::SetFilePointer(USHORT usFID, 
                    LONG lDistance, 
                    DWORD dwMoveMethod, 
                    ULONG *pOffset)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on SetFilePointer()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->SetFilePointer(lDistance, dwMoveMethod, pOffset))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation SetFilePointer() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}
 
                    
 
HRESULT 
TIDState::Close(USHORT usFID)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on Close()", this->usTid, usFID));
        hr = E_INVALID_FID;
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Close())) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Close() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}
 
HRESULT 
TIDState::GetFileSize(USHORT usFID,
                   DWORD *pdwSize)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on GetFileSize()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->GetFileSize(pdwSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation GetFileSize() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}


HRESULT 
TIDState::SetFileTime(USHORT usFID, 
                    FILETIME *pCreation, 
                    FILETIME *pAccess, 
                    FILETIME *pWrite)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on SetFileTime()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->SetFileTime(pCreation, pAccess, pWrite))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation SetFileTime() failed", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}


HRESULT 
TIDState::Flush(USHORT usFID)
{
    HRESULT hr = E_FAIL;
    SMBFileStream *pStream = NULL;
    
    //
    // if the FID is 0xFFFF flush everyone (per smbhlp.zip spec)
    if(0xFFFF == usFID)  {
        ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator it;
        ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();

        for(it = FIDList.begin(); it != itEnd; ++it) {
            SMBFileStream *pFound = *it;
            if(FAILED(hr = pFound->Flush())) {
                TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on batch Flush()", this->usTid, pFound->FID()));            
                goto Done;
            }
        }     
    } else {
    
        //
        // Search out the SMBFileStream
        if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
            TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on Flush()", this->usTid, usFID));
            goto Done;
        }
        
        //
        // Perform our operation
        if(FAILED(hr = pStream->Flush())) {
            TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Flush() failed", this->usTid, usFID));
            goto Done;
        }
    }
    
    //
    // Success
    hr = S_OK;
    Done:
        return hr;
}


HRESULT 
TIDState::QueryFileInformation(USHORT usFID, WIN32_FIND_DATA *fd)
{
    HANDLE hFileHand = INVALID_HANDLE_VALUE; // = FindFirstFile(pObject->FileName(), &fd);
    SMBFileStream *pStream = NULL;
    HRESULT hr = E_FAIL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, &pStream)) || NULL == pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on QueryFileInformation()", this->usTid, usFID));
        goto Done;
    }   
   
    //
    // Make the query
    hr = pStream->QueryFileInformation(fd);
    
    Done:
        return hr;
}

