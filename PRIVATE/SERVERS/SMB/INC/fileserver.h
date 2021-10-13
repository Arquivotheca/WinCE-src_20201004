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
#pragma once

#include "SMB_Globals.h"
#include "SMBPackets.h"
#include "ShareInfo.h"

//
// Forward declare w/o pulling in headers
struct SMB_PACKET;
class PerFileState;

#ifndef NO_POOL_ALLOC
#define RANGE_NODE_ALLOC                       ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(RANGE_NODE) >
#else
#define RANGE_NODE_ALLOC ce::allocator
#endif
class RANGE_NODE {
    public:
    LARGE_INTEGER Offset;
    LARGE_INTEGER Length;
    USHORT usFID;
};

class RangeLock {
    public:
        RangeLock() {}
        ~RangeLock() {ASSERT(0 == m_LockList.size());}

        HRESULT IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfVal, USHORT *usFID = NULL);
        HRESULT Lock(USHORT usFID, SMB_LARGELOCK_RANGE *pRangeLock);
        HRESULT UnLock(USHORT usFID, SMB_LARGELOCK_RANGE *pRangeLock);
        HRESULT UnLockAll(USHORT usFID);

   private:
        ce::list<RANGE_NODE, RANGE_NODE_ALLOC> m_LockList;
};


//
// Open disposition commands
//     spec in smbhlp.zip
//#define DISPOSITION_MASK      0x03
#define DISP_OPEN_EXISTING    0x01
#define DISP_CREATE_NEW       0x10
#define DISP_OPEN_ALWAYS      0x11
#define DISP_CREATE_ALWAYS    0x12

//
//  TRANS2_QUERY_FS_INFORMATION subcommands
#define SMB_INFO_ALLOCATION               0x01
#define SMB_INFO_VOLUME                   0x02
#define SMB_QUERY_FS_VOLUME_INFO          0x102
#define SMB_QUERY_DISK_ALLOCATION_NT      0x103
#define SMB_QUERY_FS_ATTRIBUTE_INFO       0x105


//
// TRANS2_QUERY_FILE_INFORMATION subcommands
#define SMB_QUERY_FILE_ALL_INFO_ID           0x0107

#define SMB_QUERY_FILE_BASIC_INFO_ID         0x0101
#define SMB_QUERY_FILE_STANDARD_INFO_ID      0x0102
#define SMB_QUERY_FILE_EA_INFO_ID            0x0103
#define SMB_QUERY_FILE_STREAM_INFO_ID        0x0109



#define SMB_SET_FILE_BASIC_INFO              0x0101
#define SMB_SET_FILE_DISPOSITION_INFO        0x0102
#define SMB_SET_FILE_ALLOCATION_INFO         0x0103
#define SMB_SET_FILE_END_OF_FILE_INFO        0x0104


//
//TRANS2 commands
#define TRANS2_FIND_FIRST2                0x01
#define TRANS2_FIND_NEXT2                 0x02
#define TRANS2_QUERY_FS_INFORMATION       0x03
#define TRANS2_QUERY_PATH_INFO            0x05
#define TRANS2_QUERY_FILE_INFORMATION     0x07
#define TRANS2_SET_FILE_INFORMATION       0x08



//
// Allocation functions
#ifndef NO_POOL_ALLOC
#define FID_INFO_ALLOC           ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(FID_INFO) >
#define AFS_FILEOBJECT_ALLOC     ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(FileObject) >
#define AFS_PERFILESTATE_ALLOC   ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(PerFileState) >
#define BLOCKED_HANDLES_ALLOC    ce::singleton_allocator< ce::fixed_block_allocator<5>, sizeof(HANDLE) >
#else
#define FID_INFO_ALLOC           ce::allocator
#define AFS_FILEOBJECT_ALLOC     ce::allocator
#define AFS_PERFILESTATE_ALLOC   ce::allocator
#define BLOCKED_HANDLES_ALLOC    ce::allocator
#endif

//
// TRANS2 Query File Info
// Spec in cifs9f.doc
#define QFI_FILE_READ_DATA                  0x000001
#define QFI_FILE_WRITE_DATA                 0x000002
#define QFI_FILE_APPEND_DATA                0x000004
#define QFI_FILE_READ_EA                    0x000008
#define QFI_FILE_WRITE_EA                   0x000010
#define QFI_FILE_EXECUTE                    0x000020
#define QFI_FILE_READ_ATTRIBUTES            0x000080
#define QFI_FILE_WRITE_ATTRIBUTES           0x000100
#define QFI_DELETE                          0x010000
#define QFI_READ_CONTROL                    0x020000
#define QFI_WRITE_DAC                       0x040000
#define QFI_WRITE_OWNER                     0x080000
#define QFI_SYNCHRONIZE                     0x100000

#define QFI_FILE_WRITE_THROUGH              0x02
#define QFI_FILE_SEQUENTIAL_ONLY            0x04
#define QFI_FILE_SYNCHRONOUS_IO_ALERT       0x10
#define QFI_FILE_SYNCHRONOUS_IO_NONALERT    0x20

#define QFI_FILE_BYTE_ALIGNMENT             0x000
#define QFI_FILE_WORD_ALIGNMENT             0x001
#define QFI_FILE_LONG_ALIGNMENT             0x003
#define QFI_FILE_QUAD_ALIGNMENT             0x007
#define QFI_FILE_OCTA_ALIGNMENT             0x00f
#define QFI_FILE_32_BYTE_ALIGNMENT          0x01f
#define QFI_FILE_64_BYTE_ALIGNMENT          0x03f
#define QFI_FILE_128_BYTE_ALIGNMENT         0x07f
#define QFI_FILE_256_BYTE_ALIGNMENT         0x0ff
#define QFI_FILE_512_BYTE_ALIGNMENT         0x1ff

#define FILE_ATTRIBUTE_MUST_BE_DIR          0x10000000
#define FILE_ATTRIBUTE_CANT_BE_DIR          0x20000000


namespace SMB_FILE {
    DWORD SMB_Com_CheckPath(SMB_PROCESS_CMD *_pRawRequest,
         SMB_PROCESS_CMD *_pRawResponse,
         UINT *puiUsed,
         SMB_PACKET *pSMB);
    DWORD SMB_Com_Search(SMB_PROCESS_CMD *_pRawRequest,
             SMB_PROCESS_CMD *_pRawResponse,
             UINT *puiUsed,
             SMB_PACKET *pSMB);
    DWORD SMB_Com_TRANS2(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_Query_Information(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_Query_Information_Disk(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_Query_EX_Information(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed);
    DWORD SMB_Com_LockX(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_DeleteFile(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_RenameFile(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_NT_TRASACT(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_SetInformation(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_SetInformation2(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_Flush(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_MakeDirectory(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_DelDirectory(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_FindClose2(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
    DWORD SMB_Com_NT_Cancel(SMB_PROCESS_CMD *_pRawRequest,
             SMB_PROCESS_CMD *_pRawResponse,
             UINT *puiUsed,
             SMB_PACKET *pSMB);
    DWORD SMB_Com_Seek(SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 SMB_PACKET *pSMB);
};




//
// Forward declar classes here
class FileObject;

struct FID_INFO {
    USHORT FID;
    BOOL fHasExclusiveRead;
    BOOL fHasExclusiveWrite;
};

//
// Abstraction used for all file operations -- mainly here to implement range
//    locking
class FileObject
{
    public:
        FileObject(USHORT usFID);
        ~FileObject();

        HRESULT Close();
        HRESULT Delete();
        HRESULT Open(const WCHAR *pFile,
                                DWORD dwDisposition,
                                DWORD dwAttributes,
                                DWORD dwAccess,
                                DWORD dwShareMode,
                                DWORD *pdwActionTaken = NULL);
        HRESULT Read(USHORT usFID,
                                BYTE *pDest,
                                LONG lOffset,
                                DWORD dwReqSize,
                                DWORD *pdwRead);
        HRESULT Write(USHORT usFID,
                                BYTE *pSrc,
                                LONG lOffset,
                                DWORD dwReqSize,
                                DWORD *pdwWritten);
        HRESULT SetFileTime(FILETIME *pCreation,
                                FILETIME *pAccess,
                                FILETIME *pWrite);

        HRESULT GetFileTime(FILETIME *lpCreationTime,
                                FILETIME *lpLastAccessTime,
                                FILETIME *lpLastWriteTime);

        HRESULT GetFileInformation(DWORD* pdwFileAttributes,
                                FILETIME* lpCreationTime,
                                FILETIME* lpLastAccessTime,
                                FILETIME* lpLastWriteTime,
                                DWORD* pdwFileSizeHigh,
                                DWORD* pdwFileSizeLow,
                                DWORD* pdwNumberOfLinks,
                                DWORD* pdwFileIndexHigh,
                                DWORD* pdwFileIndexLow);

        HRESULT SetEndOfStream(DWORD dwOffset);
        HRESULT GetFileSize(DWORD *pdwSize);
        USHORT  FID();
        const WCHAR  *FileName();
        HRESULT Flush();

        VOID SetPerFileState(ce::smart_ptr<PerFileState> &pNode);
        ce::smart_ptr<PerFileState> GetPerFileState();
        HRESULT IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfLocked);
        HRESULT Lock(SMB_LARGELOCK_RANGE *pRangeLock);
        HRESULT UnLock(SMB_LARGELOCK_RANGE *pRangeLock);
        BOOL   IsShareDelete() {return m_fShareDelete;}
        VOID    SetShareDelete(BOOL fVal) {m_fShareDelete = fVal;}
        const WCHAR *GetFileName();
    private:
        HANDLE m_hFile;
        USHORT m_usFID;
        BOOL m_fShareDelete;

        StringConverter m_FileName;
        RangeLock *m_LockNode;
        ce::smart_ptr<PerFileState> m_PerFileState;
        CRITICAL_SECTION m_csFileLock;
};


class PerFileState {
    public:
        StringConverter m_FileName;
        RangeLock       m_RangeLock;
        UINT            m_uiRefCnt;
        BOOL            m_fDeleteOnClose;
        UINT            m_uiNonShareDeleteOpens;

        BOOL            m_fOpBreakSent;
        BOOL            m_fBatchOpLock;
        USHORT          m_usLockTID;
        USHORT          m_usLockFID;
        ULONG           m_ulLockConnectionID;
        QueuePacketToBeSent m_pfnQueueFunction;
        DeleteTransportToken m_pfnDeleteToken;
        VOID           *m_pTransToken;
        ce::list<HANDLE, BLOCKED_HANDLES_ALLOC > m_BlockedOnOpLockList;
};


//
// Abstraction for file system (it adds functionality for things the CE filesystem doesnt support)
class AbstractFileSystem
{
    public:
        AbstractFileSystem();
        ~AbstractFileSystem();

        //NOTE:  this API must call SetLastError!!!
        HRESULT AFSDeleteFile(const WCHAR *pFile);
        HRESULT Open(const WCHAR *pFileName,
                     USHORT usFID,
                     DWORD dwAccess,
                     DWORD dwDisposition,
                     DWORD dwAttributes,
                     DWORD dwShareMode,
                     DWORD *pdwActionTaken = NULL,
                     SMBFile_OpLock_PassStruct *pOpLock = NULL);
        HRESULT Close(USHORT usFID);
        HRESULT FindFile(USHORT usFID, ce::smart_ptr<FileObject> &pFileObject);
        HRESULT BreakOpLock(PerFileState *pFileState);
    private:
        ce::list<ce::smart_ptr<FileObject>, AFS_FILEOBJECT_ALLOC >        m_FileObjectList;  //Use only under AFSLock!
        ce::list<ce::smart_ptr<PerFileState>,  AFS_PERFILESTATE_ALLOC >   m_PerFileState;   //Use only under AFSLock!
        CRITICAL_SECTION AFSLock;
};

//
// A FileStream is the generic abstraction for a SMBFileStream.
class FileStream : public SMBFileStream
{
    public:
        FileStream(TIDState *pMyState, USHORT usFID);
        ~FileStream();

        void * operator new(size_t size) {
             return SMB_Globals::g_FileStreamAllocator.allocate(size);
        }
        void   operator delete(void *mem, size_t size) {
            SMB_Globals::g_FileStreamAllocator.deallocate(mem, size);
        }

        //
        // Functions required by SMBFileStream
        HRESULT Read(BYTE *pDest,
                     DWORD dwOffset,
                     DWORD dwReqSize,
                     DWORD *pdwRead);
        HRESULT Write(BYTE *pSrc,
                     DWORD dwOffset,
                     DWORD dwReqSize,
                     DWORD *pdwWritten);
        HRESULT Open(const WCHAR *pFileName,
                     DWORD dwAccess,
                     DWORD dwDisposition,
                     DWORD dwAttributes,
                     DWORD dwShareMode,
                     DWORD *pdwActionTaken = NULL,
                     SMBFile_OpLock_PassStruct *pdwOpLock = NULL);
        HRESULT SetEndOfStream(DWORD dwOffset);
        HRESULT SetFilePointer(LONG lDistance,
                            DWORD dwMoveMethod,
                            ULONG *pOffset);
        HRESULT GetFileSize(DWORD *pdwSize);
        HRESULT SetFileTime(FILETIME *pCreation,
                            FILETIME *pAccess,
                            FILETIME *pWrite);
        HRESULT GetFileTime(LPFILETIME lpCreationTime,
                           LPFILETIME lpLastAccessTime,
                           LPFILETIME lpLastWriteTime);
        HRESULT Flush();
        HRESULT Close();
        HRESULT QueryFileInformation(WIN32_FIND_DATA *fd, DWORD *pdwNumberOfLinks);
        BOOL    CanRead();
        BOOL    CanWrite();

        HRESULT IsLocked(SMB_LARGELOCK_RANGE *pRangeLock, BOOL *pfLocked);
        HRESULT Lock(SMB_LARGELOCK_RANGE *pRangeLock);
        HRESULT UnLock(SMB_LARGELOCK_RANGE *pRangeLock);
        HRESULT BreakOpLock();
        HRESULT Delete();
        const WCHAR *GetFileName();

        HRESULT GetChangeNotification(HANDLE *pHand, const WCHAR **pListenOn);
        HRESULT SetChangeNotification(HANDLE h, const WCHAR *pListenOn);

    private:
        HANDLE m_hChangeNotification;
        StringConverter m_ChangeListeningTo;

        BOOL m_fOpened;
        DWORD m_dwAccess;
        DWORD m_dwCurrentOffset;
};

//
// Stubbing function for SMB file
FileStream *GetNewFileStream(TIDState *pMyState, USHORT usFID);


class ChangeNotificationWakeUpNode : public WakeUpNode
{
    public:
        ChangeNotificationWakeUpNode(SMB_PACKET *pSMB, ce::smart_ptr<ActiveConnection> pActiveConnection, HANDLE h, UINT uiMaxReturnBuffer);
        ~ChangeNotificationWakeUpNode();

        VOID WakeUp();
        VOID Terminate(bool nolock);

        BOOL SendPacket();

    private:
        CRITICAL_SECTION m_csSendLock;
        ActiveConnection *m_pActiveConn;
        SMB_PACKET *m_pSMB;
        HANDLE m_h;
        UINT m_uiMaxReturnBuffer;
};

