//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef SHAREINFO_H
#define SHAREINFO_H

#include "Utils.h"

class SMBPrintQueue; //<--forward declare this to keep compiler calm
class SMBFileStream;
class PrintJob;
class TIDState;
struct SMB_OPENX_CLIENT_REQUEST;

/*
 * definitions for o_type - matches LAN Manager
 */
#define STYPE_DISKTREE             0     
#define STYPE_PRINTQ               1
#define STYPE_DEVICE               2
#define STYPE_IPC                  3

//
// Bits used to define the type of stream
#define FILE_STREAM  1  //use BITS here!
#define PRINT_STREAM 2

#define SHARE_MANANGER_SHARE_ALLOC    pool_allocator<10, Share *>
#define SHARE_MANANGER_TID_ALLOC      pool_allocator<10, TIDState *>
#define TIDSTATE_FID_STREAM_ALLOC     pool_allocator<10, SMBFileStream *>

class Share {
    public:
        Share();
        ~Share();
        HRESULT SetACL(const WCHAR *pACL);
        HRESULT SetRemark(const CHAR *pRemark);
        HRESULT SetShareName(const WCHAR *pShareName);
        HRESULT SetDirectoryName(const WCHAR *pDirName);
        HRESULT SetShareType(BYTE shareType);
        HRESULT SetServiceName(const WCHAR *pServiceName);
        HRESULT SetPrintQueue(SMBPrintQueue *pPrintQueue);
        HRESULT VerifyPermissions(const WCHAR *pUserName, DWORD dwAccessPerms);
        HRESULT AllowUser(const WCHAR *pUserName);
        HRESULT IsValidPath(const WCHAR *pPath);
        
        BOOL    RequireAuth() {
            return fRequireAuth;
        }
        VOID    SetRequireAuth(BOOL val) {fRequireAuth = val;}

        BOOL         IsHidden() {return fHidden;}
        VOID         SetHidden(BOOL f) {fHidden = f;}
        const CHAR  *GetServiceNameSZ();
        const WCHAR *GetServiceName();
        const CHAR  *GetShareNameSZ();
        const WCHAR *GetShareName();
        const WCHAR *GetDirectoryName();
              BYTE   GetShareType();
        const CHAR  *GetRemark();  
        SMBPrintQueue *GetPrintQueue(); 
                    
    private:  
        StringConverter m_ServiceName;  
        StringConverter m_DirectoryName;
        StringConverter m_ACLList;

        CHAR  *pShareNameSZ;
        WCHAR *pShareName;
        CHAR  *pRemark;
        BYTE   shareType;   
        BOOL   fHidden;
        BOOL   fRequireAuth;
        SMBPrintQueue *pPrintQueue;
};

class ShareManager {
    public:
        ShareManager();
        ~ShareManager();
   
        HRESULT AddShare(Share *pShare);
        Share *SearchForShare(const WCHAR *pName);        
        Share *SearchForShare(UINT uiIdx);
        
        UINT NumShares();
        UINT NumVisibleShares();
        HRESULT SearchForPrintJob(USHORT usJobID, PrintJob **pJobRet, SMBPrintQueue **pQueueRet);

        
    private:  
        ce::list<Share *, SHARE_MANANGER_SHARE_ALLOC > MasterListOfShares;
        CRITICAL_SECTION  csShareManager;

#ifdef DEBUG
        static BOOL fFirstInst;
#endif
};


class TIDManager {
    public:
        TIDManager();
       ~TIDManager();
        
        //functions used to bind to this share and access
        HRESULT BindToTID(Share *pShare, TIDState **ppTIDState, BYTE *pPassword, USHORT usPassLen);
        HRESULT UnBindTID(USHORT usTid);
        HRESULT FindTIDState(USHORT usTid, TIDState **ppTIDState);
   
    private:
        ce::list<TIDState *, SHARE_MANANGER_TID_ALLOC>   ListOfBoundShares;        
        CRITICAL_SECTION       csTIDManager;
        
#ifdef DEBUG
        static BOOL fFirstInst;
#endif
};

class TIDState 
{
    public:
        TIDState(Share *pShare);
        ~TIDState(); 
            
        void * operator new(size_t size) { 
             return SMB_Globals::g_TIDStateAllocator.allocate(size);
        }
        void   operator delete(void *mem) {
            SMB_Globals::g_TIDStateAllocator.deallocate(mem);
        }   
        
        USHORT TID();
        Share *GetShare();
        
        HRESULT AddFileStream(SMBFileStream *pStream);        
        HRESULT RemoveFileStream(USHORT usFID);

        SMBFileStream *CreateFileStream(ActiveConnection *pMyConnection);

        HRESULT Read(USHORT usFID,
                     BYTE *pDest, 
                     DWORD dwOffset, 
                     DWORD dwReqSize, 
                     DWORD *pdwRead);
         HRESULT Write(USHORT usFID,
                      BYTE *pSrc, 
                      DWORD dwOffset, 
                      DWORD dwReqSize, 
                      DWORD *pdwWritten); 
         HRESULT Open(USHORT usFID,
                      const WCHAR *pFileName,                               
                      DWORD dwAccess = GENERIC_READ, 
                      DWORD dwDisposition = OPEN_EXISTING, 
                      DWORD dwAttributes=FILE_ATTRIBUTE_READONLY, 
                      DWORD dwShareMode = 0); 
         HRESULT IsLocked(USHORT usFID, 
                         USHORT usPID,
                         ULONG Offset,
                         ULONG Length,
                         BOOL *pfLocked);
         HRESULT Lock(USHORT usFID, 
                      USHORT usPID,
                      ULONG Offset,
                      ULONG Length);
         HRESULT UnLock(USHORT usFID, 
                        USHORT usPID,
                        ULONG Offset,
                        ULONG Length);
            
         HRESULT SetEndOfStream(USHORT usFID,
                               DWORD dwOffset); 
         HRESULT Close(USHORT usFID);
         HRESULT GetFileSize(USHORT usFID,
                           DWORD *pdwSize);
         HRESULT Delete(USHORT usFID,
                       const WCHAR *pFileName);
         HRESULT SetFileTime(USHORT usFID, 
                            FILETIME *pCreation, 
                            FILETIME *pAccess, 
                            FILETIME *pWrite);
         HRESULT Flush(USHORT usFID);

         HRESULT QueryFileInformation(USHORT usFID, WIN32_FIND_DATA *fd);
         //
         // for dwMoveMethod:
         //    0 = start of file
         //    1 = current file pointer
         //    2 = end of file
         HRESULT SetFilePointer(USHORT usFID, 
                             LONG lDistance, 
                             DWORD dwMoveMethod, 
                             ULONG *pOffset);
         

    protected:
        HRESULT FindFileStream(USHORT usFID, SMBFileStream **ppStream);
        
    private:       
       Share *pShare;
       USHORT usTid;  
       ce::list<SMBFileStream *, TIDSTATE_FID_STREAM_ALLOC > FIDList;

       // 
       // TID generator for the TIDState
       static UniqueID   UIDTidGenerator;
       
};

//
// SMBFileStream is a pure virtual base class for all IO operations.  
//    Under printer, most operations will be no-op's but having this base
//    class allows for some abstraction in the main code
class SMBFileStream {
    public:        
        SMBFileStream(UINT _uiStreamType, TIDState *_pMyState) {
            m_uiStreamType = _uiStreamType;
            m_pMyTIDState = _pMyState;
            m_usFID = 0xFFFF;
        }
        virtual ~SMBFileStream() {}
        virtual HRESULT Read(BYTE *pDest, 
                             DWORD dwOffset, 
                             DWORD dwReqSize, 
                             DWORD *pdwRead) = 0;
        virtual HRESULT Write(BYTE *pSrc, 
                             DWORD dwOffset, 
                             DWORD dwReqSize, 
                             DWORD *pdwWritten) = 0;

        //
        // Open the file -- all values map to CreateFile, pActionTaken returns
        //    1 = file existed and was opened
        //    2 = file didnt exist but was created
        //    3 = file existed and was truncated
        virtual HRESULT Open(const WCHAR *pFileName,                               
                             DWORD dwAccess = GENERIC_READ, 
                             DWORD dwDisposition = OPEN_EXISTING, 
                             DWORD dwAttributes=FILE_ATTRIBUTE_READONLY, 
                             DWORD dwShareMode = 0,
                             DWORD *pdwActionTaken = NULL) = 0;
        
        VOID  SetFID(USHORT _FID) {ASSERT(0xFFFF == m_usFID); m_usFID = _FID;}
        USHORT FID() {return m_usFID;}
        UINT   StreamType() {return m_uiStreamType;}
        TIDState *GetTIDState() {return m_pMyTIDState;}
        

        //
        // The following functions are implemented here and ASSERT -- they are just stubs
        //   and are not always required of a FileStream (for example the case of print)
        virtual HRESULT SetEndOfStream(DWORD dwOffset) {ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT Close() { ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT GetFileSize(DWORD *pdwSize) {return E_NOTIMPL;}
        virtual HRESULT SetFileTime(FILETIME *pCreation, FILETIME *pAccess, FILETIME *pWrite) {return E_NOTIMPL;}
        virtual HRESULT Flush() {ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT GetFileTime(LPFILETIME lpCreationTime,
                                    LPFILETIME lpAccessTime,
                                    LPFILETIME lpWriteTime) {return E_NOTIMPL;}
        virtual HRESULT QueryFileInformation(WIN32_FIND_DATA *fd) {return E_NOTIMPL;}
        virtual HRESULT IsLocked(USHORT usPID,ULONG Offset,ULONG Length, BOOL *pfLocked){*pfLocked=TRUE;ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT Lock(USHORT usPID,ULONG Offset,ULONG Length){ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT UnLock(USHORT usPID,ULONG Offset, ULONG Length){ASSERT(FALSE); return E_NOTIMPL;}

        //
        // for dwMoveMethod:
        //    0 = start of file
        //    1 = current file pointer
        //    2 = end of file
        virtual HRESULT SetFilePointer(LONG lDistance, 
                                    DWORD dwMoveMethod, 
                                    ULONG *pOffset) {ASSERT(FALSE); return E_NOTIMPL;}
    private:
       USHORT m_usFID;
       UINT m_uiStreamType;
       TIDState *m_pMyTIDState;
};
#endif

