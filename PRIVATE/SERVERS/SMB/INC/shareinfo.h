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
#ifndef SHAREINFO_H
#define SHAREINFO_H

#include "Utils.h"
#include "PktHandler.h"

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

#ifndef NO_POOL_ALLOC
#define SHARE_MANANGER_SHARE_ALLOC    ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(Share*) >
#define SHARE_MANANGER_TID_ALLOC      ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(TIDState) >
#define TIDSTATE_FID_STREAM_ALLOC     ce::singleton_allocator< ce::fixed_block_allocator<30>, sizeof(SMBFileStream *) >
#else
#define SHARE_MANANGER_SHARE_ALLOC    ce::allocator
#define SHARE_MANANGER_TID_ALLOC      ce::allocator
#define TIDSTATE_FID_STREAM_ALLOC     ce::allocator
#endif


class Share {
    public:
        Share();
        ~Share();
        HRESULT SetACL(const WCHAR *pFullACL, const WCHAR *pReadOnlyACL);
        HRESULT SetRemark(const CHAR *pRemark);
        HRESULT SetShareName(const WCHAR *pShareName);
        HRESULT SetDirectoryName(const WCHAR *pDirName);
        HRESULT SetShareType(BYTE shareType);
        HRESULT SetServiceName(const WCHAR *pServiceName);
        HRESULT SetPrintQueue(SMBPrintQueue *pPrintQueue);
        HRESULT AllowUser(const WCHAR *pUserName, DWORD dwPerms);
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
        StringConverter m_FullACLList;
        StringConverter m_ReadOnlyACLList;


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

        HRESULT DeleteShare(Share *pShare);
        HRESULT AddShare(Share *pShare);
        Share *SearchForShare(const WCHAR *pName);
        Share *SearchForShare(UINT uiIdx);
        HRESULT ReloadACLS();

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


class TIDState
{
    public:
        TIDState();
        ~TIDState();
        HRESULT Init(Share *pShare);

        USHORT TID();
        Share *GetShare() {
            CCritSection csLock(&csTIDLock);
            csLock.Lock();
            return pShare;
        }

        HRESULT AddFileStream(SMBFileStream *pStream);
        HRESULT RemoveFileStream(USHORT usFID);

        SMBFileStream *CreateFileStream(ce::smart_ptr<ActiveConnection> pMyConnection);

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
                         SMB_LARGELOCK_RANGE *pRangeLock,
                         BOOL *pfLocked);
         HRESULT Lock(USHORT usFID,
                        SMB_LARGELOCK_RANGE *pRangeLock);
         HRESULT UnLock(USHORT usFID,
                        SMB_LARGELOCK_RANGE *pRangeLock);
         HRESULT BreakOpLock(USHORT usFID);

         HRESULT SetEndOfStream(USHORT usFID,
                               DWORD dwOffset);
         HRESULT Close(USHORT usFID);
         HRESULT GetFileSize(USHORT usFID,
                           DWORD *pdwSize);
         HRESULT Delete(USHORT usFID);
         HRESULT SetFileTime(USHORT usFID,
                            FILETIME *pCreation,
                            FILETIME *pAccess,
                            FILETIME *pWrite);
         HRESULT Flush(USHORT usFID);

         HRESULT QueryFileInformation(USHORT usFID, WIN32_FIND_DATA *fd, DWORD *pdwNumberOfLinks);
         //
         // for dwMoveMethod:
         //    0 = start of file
         //    1 = current file pointer
         //    2 = end of file
         HRESULT SetFilePointer(USHORT usFID,
                             LONG lDistance,
                             DWORD dwMoveMethod,
                             ULONG *pOffset);


        HRESULT FindFileStream(USHORT usFID, ce::smart_ptr<SMBFileStream> &pStream);

        BOOL  HasOpenedResources() {return FIDList.size()?TRUE:FALSE;}

#ifdef DEBUG
        VOID DebugPrint();
#endif


    private:
       Share *pShare;
       USHORT usTid;
       ce::list<ce::smart_ptr<SMBFileStream>, TIDSTATE_FID_STREAM_ALLOC > FIDList;
       CRITICAL_SECTION csTIDLock;

       //
       // TID generator for the TIDState
       static UniqueID   UIDTidGenerator;
       static UniqueID   UniqueFID;

};


struct SMBFile_OpLock_PassStruct {
    DWORD dwRequested;
    CHAR cGiven;
    QueuePacketToBeSent pfnQueuePacket;
    CopyTransportToken  pfnCopyTranportToken;
    DeleteTransportToken pfnDeleteTransportToken;
    VOID *pTransportToken;
    ULONG           ulConnectionID;
    USHORT          usLockTID;
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
                             DWORD *pdwActionTaken = NULL,
                             SMBFile_OpLock_PassStruct *pdwOpLock = NULL) = 0;

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
        virtual HRESULT QueryFileInformation(WIN32_FIND_DATA *fd, DWORD *pdwNumberOfLinks) {return E_NOTIMPL;}
        virtual HRESULT IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfLocked){*pfLocked=TRUE;ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT Lock(SMB_LARGELOCK_RANGE *pRangeLock){ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT UnLock(SMB_LARGELOCK_RANGE *pRangeLock){ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT BreakOpLock() {ASSERT(FALSE);return E_NOTIMPL;}
        virtual HRESULT Delete() { ASSERT(FALSE); return E_NOTIMPL;}
        virtual const WCHAR *GetFileName() {return NULL;}
        virtual HRESULT GetChangeNotification(HANDLE *pHand, const WCHAR **pListenOn){ASSERT(FALSE); return E_NOTIMPL;}
        virtual HRESULT SetChangeNotification(HANDLE h, const WCHAR *pListenOn){ASSERT(FALSE); return E_NOTIMPL;}

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

