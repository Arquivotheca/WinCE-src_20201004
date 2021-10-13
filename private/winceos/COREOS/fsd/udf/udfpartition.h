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

enum UDF_PART_TYPE
{
    UDF_PART_PHYSICAL,
    UDF_PART_SPARING,
    UDF_PART_VIRTUAL,
    UDF_PART_METADATA
};

#define SPARE_ENTRIES_PER_BLOCK 128

typedef struct _SPARE_ENTRY
{
    Uint32 OriginalBlock;
    Uint32 SparedBlock;
} SPARE_ENTRY, *PSPARE_ENTRY;

// /////////////////////////////////////////////////////////////////////////////
// CUDFPartition
//
// This is an abstract class that defines an interface common to all UDF
// partitions. It is the base class for any UDF partition.
//
class CUDFPartition
{
public:
    virtual Uint16 GetReferenceNumber() const = 0;
    virtual Uint8 GetPartitionType() const = 0;
    virtual CVolume* GetVolume() const = 0;

    virtual LRESULT Read( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer ) = 0;
    virtual LRESULT Write( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer ) = 0;

    virtual Uint32 GetPhysicalBlock( NSRLBA lba ) = 0;

protected:
    virtual void Lock() = 0;
    virtual void Unlock() = 0;
};

// /////////////////////////////////////////////////////////////////////////////
// CPhysicalPartition
//
// This is the base class for all physical partitions.  It is a valid
// implementation for the plain physical partition, either for hard drive type
// media or as the base class for a virtual or metadata partition.
//
class CPhysicalPartition : public CUDFPartition
{
public:
    CPhysicalPartition();
    virtual ~CPhysicalPartition();

    virtual BOOL Initialize( CVolume* pVolume, Uint16 ReferenceNumber );

    virtual LRESULT Read( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );
    virtual LRESULT Write( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );

    virtual Uint32 GetPhysicalBlock( NSRLBA lba );

public:
    inline Uint16 GetReferenceNumber() const
    {
        return m_ReferenceNumber;
    }

    inline CVolume* GetVolume() const
    {
        return m_pVolume;
    }

    virtual inline Uint8 GetPartitionType() const
    {
        return UDF_PART_PHYSICAL;
    }

public:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

protected:
    CRITICAL_SECTION m_cs;
    CVolume* m_pVolume;
    Uint16 m_ReferenceNumber;
};

// /////////////////////////////////////////////////////////////////////////////
// CSparingPartition
//
// This is a physical partition that may be used by itself (UDF 2.01 on CD-RW)
// or as the base partition for a metadata partition (UDF 2.50 on CD-RW).
//
class CSparingPartition : public CPhysicalPartition
{
public:
    CSparingPartition();
    ~CSparingPartition();

    BOOL InitializeSparingInfo( PPARTMAP_SPARABLE pSparingMap );

    LRESULT Read( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );
    LRESULT Write( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );

public:
    inline Uint8 GetPartitionType() const
    {
        return UDF_PART_SPARING;
    }

private:
    LRESULT AddSparingEntry( Uint32 Original, Uint32 Spared );
    Uint32 GetPacketLocation( Uint32 Original );

private:
    Uint16 m_PacketLength;
    Uint8  m_NumberOfTables;
    Uint32 m_TableSize;
    Uint32 m_TableStart[4];

    Uint16 m_NumberSparedEntries;
    Uint16 m_NumberOfDeadEntries;
    Uint16 m_TotalNumberOfEntries;
    PSPARE_ENTRY m_pEntries;
};

// /////////////////////////////////////////////////////////////////////////////
// CFilePartition
//
// This in an abstract class defining an interface for all partitions that are
// really just a file inside of a physical partition.  Currently this includes
// virtual partitions (for any packet written write once media) and metadata
// partitions for UDF 2.50 re-writable media.
//
class CFilePartition : public CUDFPartition
{
public:
    virtual LRESULT Initialize( CPhysicalPartition* pBasePartition,
                                Uint16 ReferenceNumber ) = 0;
};

// /////////////////////////////////////////////////////////////////////////////
// CVirtualPartition
//
// This is a file partition to be used on write once media.  It's backing file
// contains a table that remaps metadata on disk for the appearance of
// overwritable media.
//
class CVirtualPartition : public CFilePartition
{
public:
    CVirtualPartition();
    virtual ~CVirtualPartition();

    LRESULT Initialize( CPhysicalPartition* pBasePartition,
                        Uint16 ReferenceNumber );

//    BOOL InitializeVirtualInfo( PPARTMAP_VIRTUAL pVirtualMap );

    LRESULT Read( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );
    LRESULT Write( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );

    Uint32 GetPhysicalBlock( NSRLBA lba );

    Uint32 GetVATSector();

public:
    inline Uint16 GetReferenceNumber() const
    {
        return m_ReferenceNumber;
    }

    inline Uint8 GetPartitionType() const
    {
        return UDF_PART_VIRTUAL;
    }

    inline CVolume* GetVolume() const
    {
        return m_pVolume;
    }

protected:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

    inline Uint32 GetBlockThroughVAT( Uint32 Block ) const
    {
        Uint32 ReturnBlock = Block;

        if( Block < m_NumEntries )
        {
            if( m_pEntries[Block] != UDF_VAT_UNUSED_ENTRY )
            {
                ReturnBlock = m_pEntries[Block];
            }
        }

        return ReturnBlock;
    }

    BOOL FoundVAT( DWORD dwSector, BYTE* pBuffer, DWORD dwBufferSize );

private:
    CRITICAL_SECTION m_cs;
    CPhysicalPartition* m_pBasePartition;
    CVolume* m_pVolume;
    Uint16 m_ReferenceNumber;

    CWriteOnceMedia* m_pMedia;
    CFile m_Vat;

    UCHAR m_FileType;         // This will let us know which type of VAT we have.

    VAT_HEADER m_Header;    // For UDF 2.00 and later.
    VAT_TRAILER m_Trailer;  // For UDF 1.50 - glad they changed this!
    Uint32 m_NumEntries;
    Uint32* m_pEntries;
};

// /////////////////////////////////////////////////////////////////////////////
// CMetadataPartition
//
// This is a file partition used on UDF 2.50+ volumes for re-writable media.  It
// stores all of the metadata in a file, with a possible mirror file, and a
// bitmap that describes which sectors of the file are allocated.
//
class CMetadataPartition : public CFilePartition
{
public:
    CMetadataPartition();
    virtual ~CMetadataPartition();

    LRESULT Initialize( CPhysicalPartition* pBasePartition,
                        Uint16 ReferenceNumber );

    BOOL InitializeMetadataInfo( PPARTMAP_METADATA pMetadataMap );

    LRESULT Read( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );
    LRESULT Write( NSRLBA lba, Uint32 Offset, Uint32 Size, BYTE* pBuffer );

    Uint32 GetPhysicalBlock( NSRLBA lba );

public:
    inline Uint16 GetReferenceNumber() const
    {
        return m_ReferenceNumber;
    }

    inline Uint8 GetPartitionType() const
    {
        return UDF_PART_METADATA;
    }

    inline CVolume* GetVolume() const
    {
        return m_pVolume;
    }

protected:
    inline void Lock()
    {
        EnterCriticalSection( &m_cs );
    }

    inline void Unlock()
    {
        LeaveCriticalSection( &m_cs );
    }

private:
    CRITICAL_SECTION m_cs;
    CPhysicalPartition* m_pBasePartition;
    CVolume* m_pVolume;
    Uint16 m_ReferenceNumber;

    CFile m_MDFile;
    CFile m_MDMirrorFile;
    CFile m_MDBitmapFile;

    Uint32 m_MetadataFileLocation;
    Uint32 m_MirrorLocation;
    Uint32 m_BitmapLocation;
    Uint32 m_AllocationUnitSize;
    Uint16 m_AlignmentUnitSize;

    BOOL m_fMirror;
};

