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

class CVolume;
class CDirectory;
class CFile;
class CStream;
class CFileLink;

// /////////////////////////////////////////////////////////////////////////////
// FILE_HANDLE
//
// In the case of a volume handle, the pStream is NULL and we use pVolume
// instead of pLink.
//
typedef struct _FILE_HANDLE
{
    Uint64  Position;
    BOOL    fOverlapped;
    Uint32  dwAccess;
    Uint32  dwShareMode;

    union
    {
        CFileLink* pLink;
        CVolume* pVolume;
    };

    CStream* pStream;
} FILE_HANDLE, *PFILE_HANDLE, **PPFILE_HANDLE;

// /////////////////////////////////////////////////////////////////////////////
// SEARCH_HANDLE
//
typedef struct _SEARCH_HANDLE : public FILE_HANDLE
{
    CFileName* pFileName;

} SEARCH_HANDLE, *PSEARCH_HANDLE;

// /////////////////////////////////////////////////////////////////////////////
// HANDLE_LIST
//
typedef struct _HANDLE_LIST
{
    PFILE_HANDLE pHandle;
    struct _HANDLE_LIST* pNext;
} HANDLE_LIST, *PHANDLE_LIST;

// /////////////////////////////////////////////////////////////////////////////
// HANDLE_MAP
//
typedef struct _HANDLE_MAP
{
    PHANDLE_LIST pHandles;
    CFile* pFile;
    struct _HANDLE_MAP* pNextMap;
} HANDLE_MAP, *PHANDLE_MAP;

// /////////////////////////////////////////////////////////////////////////////
// FID_LIST
//
typedef struct _FID_LIST
{
    BYTE* pFids;
    DWORD dwSize;
    NSRLBA Location;
    BOOL  fModified;
    struct _FID_LIST* pNext;
} FID_LIST, *PFID_LIST;

// /////////////////////////////////////////////////////////////////////////////
// FILE_LIST
//
typedef struct _FILE_LIST
{
    CFile* pFile;
    struct _FILE_LIST* pNextFile;
} FILE_LIST, *PFILE_LIST;

// /////////////////////////////////////////////////////////////////////////////
// GetHashValue
//
inline DWORD GetHashValue( CFile* pFile, DWORD dwHashSize );

// /////////////////////////////////////////////////////////////////////////////
// CHandleManager
//
// This keeps track of all the open handles on the system.
//
// /////////////////////////////////////////////////////////////////////////////
// CHandleManager
//
class CHandleManager
{
public:
    CHandleManager();
    ~CHandleManager();

    BOOL Initialize( CVolume* pVolume );
    LRESULT AddHandle( PFILE_HANDLE pFileHandle );

    LRESULT RemoveHandle( PFILE_HANDLE pFileHandle );

public:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

private:
    CVolume* m_pVolume;
    CRITICAL_SECTION m_cs;

    DWORD m_dwHashSize;
    PHANDLE_MAP m_pHandleMap;

    DWORD m_dwNumHandles;
};

// /////////////////////////////////////////////////////////////////////////////
// CFileManager
//
class CFileManager
{
public:
    CFileManager();
    ~CFileManager();

    BOOL Initialize( CVolume* pVolume );
    LRESULT AddFile( CFile* pFile, CFile** ppExistingFile );
    void Cleanup();
    PFILE_LIST CleanFile( CFile* pFile );

public:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

    inline void SetCleanup( BOOL fValue )
    {
        //
        // Missing this once isn't going to be a big deal, so not using a
        // lock on this should be ok for now.
        //
        m_fAttemptCleanup = fValue;
    }

protected:
    LRESULT FindFile( CFile* pFile, CFile** ppExistingFile );

private:
    CRITICAL_SECTION m_cs;
    CVolume* m_pVolume;
    BOOL m_fAttemptCleanup;

    DWORD m_dwHashSize;
    PFILE_LIST m_pFileList;
};

// /////////////////////////////////////////////////////////////////////////////
// CAllocExtent
//
// This class represents the mapping of one segment of the file's allocation to
// the on disk location.
//
class CAllocExtent
{
public:
    CAllocExtent();
    ~CAllocExtent();

    BOOL Initialize( Uint64 FileOffset,
                     Uint32 Length,
                     Uint32 Lbn,
                     Uint16 Partition,
                     CStream* pStream );

    BOOL Consume( CAllocExtent* pSegment );
    BOOL ContainsOffset( Uint64 Offset );

public:
    inline Uint64 GetStreamOffset() const
    {
        return m_StreamOffset;
    }

    inline Uint32 GetNumBlocks() const
    {
        return m_NumBlocks;
    }

    inline Uint32 GetNumBytes() const
    {
        return m_Length;
    }

    inline NSRLBA GetLocation() const
    {
        NSRLBA lba;

        lba.Lbn = m_Lbn;
        lba.Partition = m_Partition;
        return lba;
    }

    inline Uint16 GetPartition() const
    {
        return m_Partition;
    }

    inline Uint32 GetBlock() const
    {
        return m_Lbn;
    }

private:
    Uint64  m_StreamOffset;
    Uint32  m_Lbn;
    Uint32  m_Length;
    Uint16  m_Partition;

    Uint32   m_NumBlocks;
    CStream* m_pStream;
};

// /////////////////////////////////////////////////////////////////////////////
// ALLOC_CHUNK
//
// This is used to keep track of multiple allocation segments.  For mastered
// media there simply will not be a lot of fragmentation.
//
typedef struct _ALLOC_CHUNK
{
    CAllocExtent* pSegments;
    Uint32 ExtentsInChunk;
    struct _ALLOC_CHUNK* pNext;
    struct _ALLOC_CHUNK* pPrev;
} ALLOC_CHUNK, *PALLOC_CHUNK;

// /////////////////////////////////////////////////////////////////////////////
//
typedef struct _AED_LIST
{
    NSRLBA Location;
    struct _AED_LIST* pNext;
} AED_LIST, *PAED_LIST;

// /////////////////////////////////////////////////////////////////////////////
// CStreamAllocation
//
class CStreamAllocation
{
public:
    CStreamAllocation();
    ~CStreamAllocation();

    BOOL Initialize( CStream* pStream );

    LRESULT AddExtent( Uint64 FileOffset,
                       Uint32 Length,
                       Uint32 Lba,
                       Uint16 Partition );

    LRESULT GetExtent( Uint64 StreamOffset, CAllocExtent** ppExtent );
    LRESULT GetNextExtent( Uint64 FileOffset, CAllocExtent** ppExtent );

private:
    ALLOC_CHUNK m_Allocation;

    Uint32   m_ExtentsPerChunk;
    CStream* m_pStream;
};

// /////////////////////////////////////////////////////////////////////////////
// CFileInfo
//
// This will contain data to be returned about the file.  Does this need to be
// a class?
//
class CFileInfo
{
public:
    CFileInfo();

    void SetFileAttribute( DWORD dwAttribute );
    void SetFileCreationTime( const FILETIME& CreateTime );
    void SetFileModifyTime( const FILETIME& ModifyTime );
    void SetFileAccessTime( const FILETIME& AccessTime );
    void SetFileSize( Uint64 FileSize );
    void SetFileSizeHigh( Uint32 FileSizeHigh );
    void SetFileSizeLow( Uint32 FileSizeLow );
    void SetObjectId( DWORD ObjectId );
    BOOL SetFileName( const WCHAR* strFileName, DWORD dwCount );

    BOOL GetFindData( PWIN32_FIND_DATAW pWin32FindData );

public:
    inline DWORD GetMaxFileNameCount() const
    {
        return sizeof( m_FindData.cFileName ) / sizeof(WCHAR);
    }

    WCHAR* GetFileNameBuffer()
    {
        return m_FindData.cFileName;
    }

private:
    WIN32_FIND_DATAW m_FindData;
};

// /////////////////////////////////////////////////////////////////////////////
// CFileLink
//
// Because UDF supports hard links, we have to make sure that files are not
// opened more than once.  To do so could lead to inconsistant data or loss
// of data.  This structure provides for a quick link to other parents that a
// child might have.
//
// Note that there cannot be hard links to directories or named streams, but
// only to the main stream of a file.
//
class CFileLink
{
public:
    CFileLink();
    ~CFileLink();

    BOOL Initialize( CFile* pChild,
                     CDirectory* pParent,
                     const WCHAR* strFileName );

public:
    inline CDirectory* GetParent() const
    {
        return m_pParent;
    }

    inline CFile* GetChild() const
    {
        return m_pChild;
    }

    inline CFileLink* GetNextParent() const
    {
        return m_pNextParent;
    }

    inline CFileLink* GetNextChild() const
    {
        return m_pNextChild;
    }

    inline void SetNextChild( CFileLink* pLink )
    {
        m_pNextChild = pLink;
    }

    inline void SetNextParent( CFileLink* pLink )
    {
        m_pNextParent = pLink;
    }

    inline const WCHAR* GetFileName() const
    {
        return m_strFileName;
    }

    inline BOOL IsHidden() const
    {
        //
        // Whether the child is hidden.
        //
        return m_fHidden;
    }

    inline void SetHidden( BOOL fHidden )
    {
        //
        // Set whether the child is hidden.
        //
        m_fHidden = fHidden;
    }

private:
    CFile* m_pChild;
    CDirectory* m_pParent;

    //
    // Any access to this pointer needs to lock the directory.
    //
    CFileLink* m_pNextChild;

    //
    // Any access to this pointer needs to lock the file.
    //
    CFileLink* m_pNextParent;

    WCHAR* m_strFileName;
    BOOL m_fHidden;

    CRITICAL_SECTION m_cs;
};

// /////////////////////////////////////////////////////////////////////////////
// CFile
//
class CFile
{
public:

    CFile();
    virtual ~CFile();

    virtual LRESULT Initialize( CVolume* pVolume, NSRLBA Location );
    LRESULT Read();

    BOOL AddParent( CFileLink* pParent );
    BOOL RemoveParent( CFileLink* pParent );

    BOOL IsSameFile( CFile* pFile );

public:
    inline CVolume* GetVolume() const
    {
        return m_pVolume;
    }

    inline ULONGLONG GetUniqueId() const
    {
        return m_UniqueId;
    }

    virtual inline BOOL IsDirectory() const
    {
        return FALSE;
    }

    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

    inline void Reference()
    {
        #ifdef DBG
        Lock();
        #endif

        InterlockedIncrement( &m_RefCount );

        #ifdef DBG
        DEBUGMSG(ZONE_FUNCTION,(TEXT("UDFS!Reference - %d\r\n"), m_RefCount );
        Unlock();
        #endif
    }

    void Dereference();

    inline LONG GetReferenceCount()
    {
        //
        // The file should be locked when calling into this function or the
        // reference count may change.
        //

        return m_RefCount;
    }

    inline Uint32 GetLBN() const
    {
        return m_IcbLocation.Lbn;
    }

    inline CStream* GetMainStream()
    {
        return m_pMainStream;
    }

    inline CDirectory* GetFirstParent()
    {
        CDirectory* pParent = NULL;

        Lock();
        if( m_pIncomingLink )
        {
            pParent = m_pIncomingLink->GetParent();
        }
        Unlock();

        return pParent;
    }

protected:
    CFileLink* m_pIncomingLink;
    CStream* m_pMainStream;
    CStream* m_pNamedStreams;

    CVolume* m_pVolume;
    CRITICAL_SECTION m_cs;
    LONG m_RefCount;

    NSRLBA m_IcbLocation;

protected:
    //
    // On disk information.
    //
    LONGAD m_StreamDirIcb;
    Uint64 m_UniqueId;
};

// /////////////////////////////////////////////////////////////////////////////
// CDirectory
//
class CDirectory : public CFile
{
public:
    CDirectory();
    virtual ~CDirectory();

    //
    // This will look for a file name starting at the given offset within the
    // directory's data stream.  it will return the offset into the data
    // stream to the start of the FID, or 0 if the file name is not found.
    //
    LRESULT FindFileName( Uint64 StartingOffset,
                          CFileName* pFileName,
                          __in_ecount(dwCharCount) WCHAR* strFileName,
                          DWORD dwCharCount,
                          PNSR_FID pStaticFid,
                          Uint64* pNextFid );

    LRESULT FindNextFile( PSEARCH_HANDLE pSearchHandle, CFileInfo* pFileInfo );

    //
    // We're overwriting this method in order to add some functionality for
    // directories.  We will still call into CFile::Initialize, but we also
    // want to read in the actual data here in order to get all the file
    // entries for this directory.
    //
    LRESULT Initialize( CVolume* pVolume, NSRLBA Location );

    //
    // This will add a link in the m_pChildren list.
    //
    CFileLink* AddChildPointer( CFile* pFile,
                                const WCHAR* strFileName,
                                BOOL fHidden );

    //
    // This will delete the pointer and remove it from both the parent
    // and the child.
    //
    BOOL RemoveChildPointer( CFile* pFile );

    //
    // This is used to find an open file under this directory.
    //
    CFileLink* FindOpenFile( CFileName* pFileName );

public:
    inline BOOL IsDirectory() const
    {
        return TRUE;
    }

    inline BOOL IsEmpty() const
    {
        return m_pChildren == NULL;
    }

protected:
    //
    // This will not delete the pointer, but will remove it from the list.
    //
    BOOL RemoveChildPointer( CFileLink* pLink );

    BOOL GetFidName( __in_ecount(dwCount) WCHAR* strName,
                     DWORD dwCount,
                     PNSR_FID pFid );

private:
    FID_LIST m_FidList;

    CFileLink* m_pChildren;
};

// /////////////////////////////////////////////////////////////////////////////
// CStream
//
// This class does most of the work for all "file" APIs.  The only information
// stored in the file itself is currently the unique id for the file, and the
// size of the entire file, including all of the streams.
//
class CStream
{
public:

    CStream();
    virtual ~CStream();

    BOOL Initialize( CFile* pFile, PICBFILE pIcb, Uint16 Partition );
    BOOL Initialize( CFile* pFile, PICBEXTFILE pIcb, Uint16 Partition );

    LRESULT GetExtentInformation( Uint64 StreamOffset, LONGAD* pLongAd );

public:

    virtual LRESULT Close();

    LRESULT ReadFile ( ULONGLONG Position,
                       __out_bcount(Length) PVOID pData,
                       DWORD Length,
                       PDWORD pLengthRead );

    LRESULT ReadFileScatter( FILE_SEGMENT_ELEMENT SegmentArray[],
                             DWORD NumberOfBytesToRead,
                             FILE_SEGMENT_ELEMENT* OffsetArray,
                             ULONGLONG Position );

    LRESULT WriteFile ( ULONGLONG Position,
                        PVOID pData,
                        DWORD Length,
                        PDWORD pLengthWritten );

    LRESULT WriteFileGather( FILE_SEGMENT_ELEMENT SegmentArray[],
                             DWORD NumberOfBytesToWrite,
                             FILE_SEGMENT_ELEMENT* OffsetArray,
                             ULONGLONG Position );

    LRESULT SetEndOfFile( ULONGLONG Position );

    LRESULT GetFileInformation( CFileLink* pLink,
                                LPBY_HANDLE_FILE_INFORMATION pFileInfo );

    LRESULT FlushFileBuffers();

    LRESULT GetFileTime( LPFILETIME pCreation,
                         LPFILETIME pLastAccess,
                         LPFILETIME pLastWrite );

    LRESULT SetFileTime( const FILETIME* pCreation,
                         const FILETIME* pLastAccess,
                         const FILETIME* pLastWrite );

    DWORD GetFileAttributes( CFileLink* pLink );
    LRESULT SetFileAttributes( DWORD Attributes );

    ULONGLONG GetFileSize();

    LRESULT DeviceIoControl( PFILE_HANDLE pHandle,
                             DWORD IoControlCode,
                             LPVOID pInBuf,
                             DWORD InBufSize,
                             LPVOID pOutBuf,
                             DWORD OutBufSize,
                             LPDWORD pBytesReturned,
                             LPOVERLAPPED pOverlapped );

    LRESULT CopyFileExternal( PFILE_COPY_EXTERNAL pInCopyReq,
                              LPVOID pOutBuf,
                              DWORD OutBufSize );

public:
    inline CVolume* GetVolume() const
    {
        return m_pVolume;
    }

    inline CFile* GetFile() const
    {
        return m_pFile;
    }

    inline BOOL IsEmbedded() const
    {
        return (m_IcbFlags & ICBTAG_F_ALLOC_IMMEDIATE) == ICBTAG_F_ALLOC_IMMEDIATE;
    }

    inline Uint8 GetAllocType() const
    {
        return (m_IcbFlags & ICBTAG_F_ALLOC_MASK);
    }

    inline CStreamAllocation* GetRecordedAllocation()
    {
        return &m_RecordedSpace;
    }

    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

protected:
    LRESULT ReadStreamAllocation();
    LRESULT ParseADBlock( __in_ecount(BufferSize) BYTE* pBuffer,
                          Uint32 BufferSize,
                          Uint64* pFileBytes );
    LRESULT AddAED( const NSRLBA& Location );
    LRESULT AddExtent( Uint64 Offset,
                       const NSRLBA& Location,
                       const NSRLENGTH& Length );
    BOOL CommonInitialize( Uint32 IcbSize, const BYTE* pADs );
    BOOL InitCreationFromEAs( PICBFILE pIcb );
    BOOL ValidateEAHeader( PNSR_EAH pHeader,
                           DWORD dwEALength,
                           DWORD* pdwImpSize );
    BOOL ExtractEATime( BYTE* pBuffer, DWORD dwEASize );

private:
    CFile*   m_pFile;
    CVolume* m_pVolume;
    CRITICAL_SECTION m_cs;

    Uint16 m_PartitionIndx;

    CStream* m_pNextStream;

    //
    // There are three types of allocation in UDF.
    // 1. Allocated and recorded.  This means that the area is marked as
    //    allocated in the bitmap descriptor and that the data for the extent
    //    is actually recorded.
    // 2. Allocated and not recorded.  Again, the extent is marked as
    //    allocated, but no data has been recorded in the extent and the
    //    file system should return zeros if this extent is read.
    // 3. Not allocated.  This is used as a sparse file.  No allocation exists
    //    for this area of the file, and the file system should just return
    //    zeros if this area is read.
    //
    // If the offset to read is not found in one of the two allocation
    // containers below, it is a whole in the file (not allocated) and we
    // should:
    // 1. Return all zeros on read.
    // 2. Allocate the extent on write.
    //
    // If we try to write to the file in an extent found in the NotRecorded
    // container, then we need to move that extent to the allocation and
    // recorded space.
    //
    CStreamAllocation m_RecordedSpace;
    CStreamAllocation m_NotRecordedSpace;

    PAED_LIST m_pAEDs;

    BYTE* m_pEmbeddedData;

private:
    //
    // On disk information.
    //
    Uint16 m_LinkCount;
    Uint64 m_InformationLength;
    Uint64 m_BlocksRecorded; // Why is this a 64-bit value?
    Uint32 m_Checkpoint;     // Is this ever used?

    //
    // TODO::Add extended attribute support.
    //
    Uint32 m_EALength;

    Uint32 m_ADLength;
    Uint8  m_FileType;    // 4 = directory, 5 = files, 12 = symbolic link
    Uint16 m_IcbFlags;

    FILETIME m_AccessTime;    // Last-Accessed Time
    FILETIME m_ModifyTime;    // Last-Modification Time
    FILETIME m_CreationTime;  // Creation Time
};


