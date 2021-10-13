//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "SMB_Globals.h"
#include "SMBPackets.h"
#include "ShareInfo.h"
#include "FixedAlloc.h"

//
// Forward declare w/o pulling in headers
struct SMB_PACKET;


#define RANGE_NODE_ALLOC                       pool_allocator<10, RANGE_NODE>

class RANGE_NODE {
    public:
    ULONG Offset;
    ULONG Length;
    USHORT usFID;
};

class RangeLock {
    public:
        RangeLock() {}
        ~RangeLock() {ASSERT(0 == m_LockList.size());}
        StringConverter FileName;
        UINT uiRefCnt;

        HRESULT IsLocked(ULONG Offset,ULONG Length, BOOL *pfVal, USHORT *usFID = NULL);
        HRESULT Lock(USHORT usFID, ULONG Offset, ULONG Length);
        HRESULT UnLock(USHORT usFID, ULONG Offset, ULONG Length);
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
#define	SMB_QUERY_FS_VOLUME_INFO	      0x102
#define SMB_QUERY_FS_ATTRIBUTE_INFO       0x105

//
// TRANS2_QUERY_FILE_INFORMATION subcommands
#define SMB_QUERY_FILE_ALL_INFO_ID           0x0107
#define SMB_QUERY_FILE_BASIC_INFO_ID         0x0101


//
//TRANS2 commands
#define TRANS2_FIND_FIRST2                0x01
#define TRANS2_FIND_NEXT2                 0x02
#define TRANS2_QUERY_FS_INFORMATION       0x03
#define TRANS2_QUERY_FILE_INFORMATION     0x07

//
// Allocation functions
#define FID_INFO_ALLOC       pool_allocator<10, FID_INFO>
#define AFS_FILEOBJECT_ALLOC pool_allocator<10, FileObject>
#define AFS_LOCKNODE_ALLOC   pool_allocator<10, RangeLock>


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
                 UINT *puiUsed);
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
        
        HRESULT SetEndOfStream(DWORD dwOffset);
        HRESULT GetFileSize(DWORD *pdwSize);
        USHORT  FID();
        const WCHAR  *FileName();
        HRESULT Flush();
        
        VOID SetLockNode(RangeLock *pNode);
        HRESULT IsLocked(USHORT usPID,ULONG Offset,ULONG Length, BOOL *pfLocked);
        HRESULT Lock(USHORT usPID,ULONG Offset,ULONG Length);
        HRESULT UnLock(USHORT usPID,ULONG Offset, ULONG Length);
    private:               
        HANDLE m_hFile; 
        USHORT m_usFID;
        StringConverter m_FileName;  
        RangeLock *m_LockNode;
};


//
// Abstraction for file system (it adds functionality for things the CE filesystem doesnt support)
class AbstractFileSystem
{
    public:
        AbstractFileSystem();
        ~AbstractFileSystem();

        HRESULT Open(const WCHAR *pFile, 
                     USHORT usFID,
                     FileObject **ppFile,
                     DWORD dwAccess, 
                     DWORD dwDisposition,
                     DWORD dwAttributes, 
                     DWORD dwShareMode,
                     DWORD *pdwActionTaken = NULL);
        HRESULT Close(USHORT usFID);
        HRESULT FindFile(USHORT usFID, FileObject **ppFileObject);
    private:
        ce::list<FileObject, AFS_FILEOBJECT_ALLOC > FileObjectList;  
        ce::list<RangeLock,  AFS_LOCKNODE_ALLOC >   LockNodeList;  
};

//
// A FileStream is the generic abstraction for a SMBFileStream.  
class FileStream : public SMBFileStream 
{
    public:
        FileStream(TIDState *pMyState);      
        ~FileStream();  

        void * operator new(size_t size) { 
             return SMB_Globals::g_FileStreamAllocator.allocate(size);
        }
        void   operator delete(void *mem) {
            SMB_Globals::g_FileStreamAllocator.deallocate(mem);
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
                     DWORD *pdwActionTaken = NULL);
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
        HRESULT QueryFileInformation(WIN32_FIND_DATA *fd);
        BOOL    CanRead();
        BOOL    CanWrite();

        HRESULT IsLocked(USHORT usPID,ULONG Offset,ULONG Length, BOOL *pfLocked);
        HRESULT Lock(USHORT usPID,ULONG Offset,ULONG Length);
        HRESULT UnLock(USHORT usPID,ULONG Offset, ULONG Length);

    private:       
        BOOL m_fOpened;
        DWORD m_dwAccess;
        DWORD m_dwCurrentOffset;        
};

//
// Stubbing function for SMB file
FileStream *GetNewFileStream(TIDState *pMyState);

