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

// -----------------------------------------------------------------------------
// CUDFMedia
// -----------------------------------------------------------------------------

//
// This is a generic class that will be used to call into any of the real
// media types.
//
class CUDFMedia
{
public:
    CUDFMedia();
    
    virtual BOOL Initialize( CVolume* pVolume );
    
    virtual LRESULT ReadRange( DWORD dwSector, 
                               DWORD dwSectorOffset,
                               DWORD dwSize, 
                               BYTE* pBuffer,
                               DWORD* pdwBytesRead ) = 0;
    virtual LRESULT WriteRange( DWORD dwSector, 
                                DWORD dwSectorOffset,
                                DWORD dwSize,
                                BYTE* pBuffer,
                                DWORD* pdwBytesWritten ) = 0;
public:
    virtual DWORD GetMinBlocksPerRead() const = 0;
    virtual DWORD GetMinBlocksPerWrite() const = 0;

protected:
    CVolume* m_pVolume;
};

// -----------------------------------------------------------------------------
// CReadOnlyMedia
// -----------------------------------------------------------------------------

//
// Characterstics of this media type are:
// 1. Each sector can only be written to once.
//
class CReadOnlyMedia : public CUDFMedia
{
public:
    LRESULT ReadRange( DWORD dwSector, 
                       DWORD dwSectorOffset,
                       DWORD dwSize, 
                       BYTE* pBuffer,
                       DWORD* pdwBytesRead );
    LRESULT WriteRange( DWORD dwSector, 
                        DWORD dwSectorOffset,
                        DWORD dwSize,
                        BYTE* pBuffer,
                        DWORD* pdwBytesWritten );

public:
    inline DWORD GetMinBlocksPerRead() const
    {
        return 1;
    }
    
    inline DWORD GetMinBlocksPerWrite() const
    {
        return 0;
    }
};

// -----------------------------------------------------------------------------
// CWriteOnceMedia
// -----------------------------------------------------------------------------
class CWriteOnceMedia : public CUDFMedia
{
public:
    CWriteOnceMedia();
    
    BOOL Initialize( CVolume* pVolume );
    
    LRESULT ReadRange( DWORD dwSector, 
                       DWORD dwSectorOffset,
                       DWORD dwSize, 
                       BYTE* pBuffer,
                       DWORD* pdwBytesRead );
    LRESULT WriteRange( DWORD dwSector, 
                        DWORD dwSectorOffset,
                        DWORD dwSize,
                        BYTE* pBuffer,
                        DWORD* pdwBytesWritten );
    
    DWORD GetMinBlocksPerRead() const;
    DWORD GetMinBlocksPerWrite() const;

private:
    BOOL m_fDiscClosed;
};

// -----------------------------------------------------------------------------
// CRewritableMedia
// -----------------------------------------------------------------------------
class CRewritableMedia : public CUDFMedia
{
public:
    LRESULT ReadRange( DWORD dwSector, 
                       DWORD dwSectorOffset,
                       DWORD dwSize, 
                       BYTE* pBuffer,
                       DWORD* pdwBytesRead );
    LRESULT WriteRange( DWORD dwSector, 
                        DWORD dwSectorOffset,
                        DWORD dwSize,
                        BYTE* pBuffer,
                        DWORD* pdwBytesWritten );
    
    DWORD GetMinBlocksPerRead() const;
    DWORD GetMinBlocksPerWrite() const;
};

// -----------------------------------------------------------------------------
// CHardDiskMedia
// -----------------------------------------------------------------------------
class CHardDiskMedia : public CUDFMedia
{
public:
    LRESULT ReadRange( DWORD dwSector, 
                       DWORD dwSectorOffset,
                       DWORD dwSize, 
                       BYTE* pBuffer,
                       DWORD* pdwBytesRead );
    LRESULT WriteRange( DWORD dwSector, 
                        DWORD dwSectorOffset,
                        DWORD dwSize,
                        BYTE* pBuffer,
                        DWORD* pdwBytesWritten );
    
    DWORD GetMinBlocksPerRead() const;
    DWORD GetMinBlocksPerWrite() const;
};
