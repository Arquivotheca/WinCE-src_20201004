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
#ifdef SMB_NMPIPE
#include "ipcstream.h"
#endif


//
// Just used to make sure (in debug) that multiple instances of ShareManager/TIDManager
//   dont get created
#ifdef DEBUG
        BOOL ShareManager::fFirstInst = TRUE;
#endif

UniqueID                           TIDState::UIDTidGenerator;
UniqueID                           TIDState::UniqueFID;
ce::fixed_block_allocator<30>      SMB_Globals::g_FileStreamAllocator;
ce::fixed_block_allocator<10>      SMB_Globals::g_PrintStreamAllocator;

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
    if(pPrintQueue) {        
        delete pPrintQueue;
    }
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
    HRESULT hr=S_OK;;
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
Share::AllowUser(const WCHAR *pUserName, DWORD dwAccess)
{
    HRESULT hr = E_ACCESSDENIED;
    CReg RegAllowAll;
    BOOL fAllowAll = FALSE;
    
    //
    // See if they have full control first
    if(!RequireAuth() || IsAccessAllowed((WCHAR *)pUserName, NULL, (WCHAR *)(m_FullACLList.GetString()), FALSE)) {
        hr = S_OK;
        goto Done;
    } 

    //
    // See if they have read only (only if they are trying to get SEC_READ)
    if(SEC_READ == dwAccess && IsAccessAllowed((WCHAR *)pUserName, NULL, (WCHAR *)(m_ReadOnlyACLList.GetString()), FALSE)) {
        hr = S_OK;
        goto Done;
    }
    
    //
    // See if security is disabled wholesale 
    if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {        
        fAllowAll = !(RegAllowAll.ValueDW(L"UseAuthentication", TRUE));
        if(TRUE == fAllowAll) {
           hr = S_OK;
           goto Done;
        }
    }
    hr = E_ACCESSDENIED;

    Done:
        return hr;;
}

HRESULT 
Share::SetACL(const WCHAR *pFullACL, const WCHAR *pReadACL) {
    HRESULT hr;

    if(FAILED(hr = m_FullACLList.Clear())) {
        goto Done;
    }
    if(FAILED(hr = m_ReadOnlyACLList.Clear())) {
        goto Done;
    }    
    if(FAILED(hr = m_FullACLList.append(pFullACL))) {
        goto Done;
    }
    if(FAILED(hr = m_ReadOnlyACLList.append(pReadACL))) {
        goto Done;
    }

    Done:
        if(FAILED(hr)) {
            m_FullACLList.Clear();
            m_ReadOnlyACLList.Clear();
        }

        return hr;
}

HRESULT
Share::IsValidPath(const WCHAR *pPath)
{
    if(0 != wcsstr(pPath, L"\\..\\")) {
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

    if(SearchForShare(pShare->GetShareName())) {
        RETAILMSG(1, (L"SMBSRV: ERROR!  adding share %s when it already exists!", pShare->GetShareName()));
        return E_FAIL;
    }
    CCritSection csLock(&csShareManager);
    csLock.Lock();
    if(!MasterListOfShares.push_front(pShare)) {
        return E_OUTOFMEMORY;
    } else {
        return S_OK;       
    }
}

HRESULT 
ShareManager::DeleteShare(Share *pShare)
{
    if(NULL == pShare) 
        return E_INVALIDARG;

    BOOL fFound = FALSE;
    CCritSection csLock(&csShareManager);
    csLock.Lock();
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;

    
    for(it=MasterListOfShares.begin(); it!=MasterListOfShares.end(); it++) {
        if(pShare == (*it)) {
            MasterListOfShares.erase(it);
            delete pShare;
            fFound = TRUE;
            break;
        }
    }
    return (TRUE == fFound)?S_OK:E_FAIL;     
}

HRESULT
ShareManager::ReloadACLS()
{
    Share *pRet = NULL;
    CCritSection csLock(&csShareManager);
    csLock.Lock();

    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator it;
    ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC >::iterator itEnd = MasterListOfShares.end();
    
    for(it = MasterListOfShares.begin(); it != itEnd; ++it) {
        CReg ShareKey;
        WCHAR wcACL[MAX_PATH];
        WCHAR wcROACL[MAX_PATH];
        
        StringConverter ShareName;
        ShareName.append(L"Services\\SMBServer\\Shares\\");
        ShareName.append((*it)->GetShareName()); 
        
        if(FALSE == ShareKey.Open(HKEY_LOCAL_MACHINE, ShareName.GetString())) {            
            continue;
        }      
        if(FALSE == ShareKey.ValueSZ(L"UserList", wcACL, MAX_PATH/sizeof(WCHAR))) {            
            wcACL[0] = NULL;
        }
        if(FALSE == ShareKey.ValueSZ(L"ROUserList", wcROACL, MAX_PATH/sizeof(WCHAR))) {            
            wcROACL[0] = NULL;
        }
        (*it)->SetACL(wcACL, wcROACL);        
    }    
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



TIDState::TIDState():pShare(NULL), 
                     usTid(0xFFFF) {
    InitializeCriticalSection(&csTIDLock);
}                    


#ifdef DEBUG
VOID 
TIDState::DebugPrint()
{
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator it = FIDList.begin();
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();  

    for(it = FIDList.begin(); it != itEnd; it++) {
        TRACEMSG(ZONE_ERROR, (L"    FID(%d) : %s", (*it)->FID(), (*it)->GetFileName()));
    }       
}
#endif



HRESULT
TIDState::Init(Share *_pShare)
{    
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    
    ASSERT(0 == FIDList.size());
    HRESULT hr = E_FAIL;
         
    //ONLY init ONCE!
    ASSERT(NULL == pShare && 0xFFFF == usTid);
    
    //
    // Get a fresh TID
    if(FAILED(UIDTidGenerator.GetID(&usTid))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: Error -- TIDState couldnt get fresh TID!"));
        ASSERT(FALSE);
        goto Done;
    }    
    pShare = _pShare;

    hr = S_OK;
    Done:
        return hr;
}

TIDState::~TIDState() 
{
    //
    // Clean up all FIDS as the manager is going down (most likely because
    //   the TID manager is pulling us down... maybe the connction died
    //   or the server is being stopped)
    while(FIDList.size()) {  
        RemoveFileStream((FIDList.front())->FID());
    }
    
    ASSERT(0 == FIDList.size());
        
    //
    // Free up our TID
    if(0xFFFF != usTid) {
        UIDTidGenerator.RemoveID(usTid);
    }

    DeleteCriticalSection(&csTIDLock);
}    
   
USHORT 
TIDState::TID() {
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    return usTid;
}

  
HRESULT 
TIDState::RemoveFileStream(USHORT usFID)
{
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator it = FIDList.begin();
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();  
    BOOL fFound = FALSE;
    
    while(it != itEnd) {
        ASSERT(*it);
        if(*it && usFID == (*it)->FID())  {          
            FIDList.erase(it++);         
            fFound = TRUE;
            break;
        }
        it++;
    }
    
    if(fFound)  {
        UniqueFID.RemoveID(usFID);         
        return S_OK;
    }
    else
        return E_FAIL;
}


HRESULT 
TIDState::AddFileStream(SMBFileStream *pConnection)
{
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    if(!FIDList.push_front(pConnection)) {
        return E_OUTOFMEMORY;
    } else {
        return S_OK;
    }
}

// 
// Class factory for building stream -- it builds the stream based on
//   the Share attached to the TIDState
SMBFileStream *        
TIDState::CreateFileStream(ce::smart_ptr<ActiveConnection> pMyConnection) 
{    
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    
    SMBFileStream *pRet = NULL;
    Share *pShare = this->GetShare();
    PrintJob *pNewJob = NULL;
    USHORT usFID = 0xFFFF;
    
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
    // Generate a FID
    if(FAILED(UniqueFID.GetID(&usFID))) {
        goto Done;
    }
    
    //
    // If the TIDState is for a print share, give them back a print stream
    if(STYPE_PRINTQ == pShare->GetShareType()) {     

        //
        // Create a new print job    
        if(NULL == (pNewJob = new PrintJob(this->GetShare(), usFID))) {
            goto Done;
        }    
        
        //
        // Set our username
        if(FAILED(pNewJob->SetOwnerName(pMyConnection->UserName()))) {
            goto Done;
        }        
        
        //
        // Create a new PrintStream and then add the job to TIDState   
        if(!(pRet = new PrintStream(pNewJob,this))) {           
           goto Done;
        }       
    } else if(STYPE_DISKTREE == pShare->GetShareType()) {  
        if(!(pRet = GetNewFileStream(this, usFID))) {
            goto Done;
        }       
    } 
#ifdef SMB_NMPIPE
	else if(STYPE_IPC == pShare->GetShareType()) {
        if(!(pRet = new IPCStream(this, usFID))) {
            goto Done;
        }
    }
#endif
    
    Done:
        if(NULL != pNewJob) {
            pNewJob->Release();
        }
        if(!pRet && 0xFFFF != usFID) {
            UniqueFID.RemoveID(usFID);
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find FID:0x%x on Read()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Read(pDest, dwOffset, dwReqSize, pdwRead))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Read() failed", this->usTid, usFID));
        goto Done;
    } else {
        CCritSection csLock(&SMB_Globals::g_Bookeeping_CS);
        csLock.Lock();
        SMB_Globals::g_Bookeeping_TotalRead.QuadPart += *pdwRead;
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find FID:0x%x on Write()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Write(pSrc, dwOffset, dwReqSize, pdwWritten))) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Write() failed", this->usTid, usFID));
        goto Done;
    } else {
        CCritSection csLock(&SMB_Globals::g_Bookeeping_CS);
        csLock.Lock();
        SMB_Globals::g_Bookeeping_TotalWritten.QuadPart += *pdwWritten;
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
                  SMB_LARGELOCK_RANGE *pRangeLock,                 
                  BOOL *pfLocked)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: IsLocked(TID:0x%x) cant find (FID:0x%x) on IsLocked()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->IsLocked(pRangeLock, pfLocked))) {
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
               SMB_LARGELOCK_RANGE *pRangeLock)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;

    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: Lock(TID:0x%x) cant find (FID:0x%x) on Lock()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->Lock(pRangeLock))) {
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
                    SMB_LARGELOCK_RANGE *pRangeLock)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: UnLock(TID:0x%x) cant find (FID:0x%x) on UnLock()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->UnLock(pRangeLock))) {
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
TIDState::BreakOpLock(USHORT usFID)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: BreakOpLock(TID:0x%x) cant find (FID:0x%x) on UnLock()", this->usTid, usFID));
        goto Done;
    }
    
    //
    // Perform our operation
    if(FAILED(hr = pStream->BreakOpLock())) {
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
TIDState::Close(USHORT usFID)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on Close()", this->usTid, usFID));
        hr = E_FAIL;
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;

    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
TIDState::Delete(USHORT usFID)
{
    HRESULT hr = E_FAIL;
    ce::smart_ptr<SMBFileStream> pStream = NULL;

    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
       TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on Delete()", this->usTid, usFID));
       goto Done;
    }

    //
    // Perform our operation
    if(FAILED(hr = pStream->Delete())) {
       TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) (FID:0x%x) operation Delete() failed", this->usTid, usFID));
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;

    //
    // If they arent setting anything, return okay
    if(!pCreation && !pAccess && !pWrite) {
        hr = S_OK;
        goto Done;
    }
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    
    //
    // if the FID is 0xFFFF flush everyone (per smbhlp.zip spec)
    if(0xFFFF == usFID)  {
        ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator it;
        ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();

        for(it = FIDList.begin(); it != itEnd; ++it) {
            ce::smart_ptr<SMBFileStream> pFound = *it;
            if(FAILED(hr = pFound->Flush())) {
                TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on batch Flush()", this->usTid, pFound->FID()));            
                goto Done;
            }
        }     
    } else {
    
        //
        // Search out the SMBFileStream
        if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;
    HRESULT hr = E_FAIL;
    
    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
        TRACEMSG(ZONE_ERROR, (L"SMB-SRV: TIDState(TID:0x%x) cant find (FID:0x%x) on QueryFileInformation()", this->usTid, usFID));
        goto Done;
    }   
   
    //
    // Make the query
    hr = pStream->QueryFileInformation(fd);
    
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
    ce::smart_ptr<SMBFileStream> pStream = NULL;

    //
    // Search out the SMBFileStream
    if(FAILED(hr = FindFileStream(usFID, pStream)) || !pStream) {
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
TIDState::FindFileStream(USHORT usFID, ce::smart_ptr<SMBFileStream> &pStream)
{
    HRESULT hr = E_FAIL;
    CCritSection csLock(&csTIDLock);
    csLock.Lock();
    
    pStream = NULL;
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator it;
    ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC >::iterator itEnd = FIDList.end();

    for(it = FIDList.begin(); it != itEnd; ++it) {
        ce::smart_ptr<SMBFileStream> pFound = *it;
        ASSERT(pFound);
        if(pFound && usFID == pFound->FID()) {  
            pStream = pFound;
            hr = S_OK;
            break;
        }          
    }     
    return hr;
} 

