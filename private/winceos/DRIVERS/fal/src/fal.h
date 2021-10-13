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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:    FAL.H

Abstract:       FLASH Abstraction Layer Interface for Windows CE 
  
-----------------------------------------------------------------------------*/

#ifndef _DSK_H_
#define _DSK_H_ 

#include <windows.h>
#include <diskio.h>
#include <devload.h>
#include "fmd.h"
#include "falmain.h"
#include "sectormgr.h"
#include "compactor.h"
#include "log2physmap.h"

class Fal
{
public:
    Fal (DWORD dwStartLogSector, DWORD dwStartPhysSector, BOOL fReadOnly);
    ~Fal();
    
    virtual BOOL StartupFAL(PFlashRegion pRegion);
    virtual BOOL ShutdownFAL();    

    BOOL ReadFromMedia(PSG_REQ pSG_req, BOOL fDoMap);
    BOOL WriteToMedia(PSG_REQ pSG_req, BOOL fDoMap);
    BOOL IsInRange (DWORD dwStartSector, DWORD dwNumSectors);
    BOOL FormatRegion(VOID);

    inline DWORD GetStartLogSector() { return m_dwStartLogSector; }
    inline DWORD GetNumLogSectors() { return m_dwNumLogSectors; }
    inline DWORD GetStartPhysSector() { return m_dwStartPhysSector; }
    inline DWORD GetNumPhysSectors() { return m_pRegion->dwNumPhysBlocks * m_dwSectorsPerBlock; }
    inline PFlashRegion GetFlashRegion() { return m_pRegion; }

public:
    virtual BOOL DeleteSectors(DWORD dwStartLogSector, DWORD dwNumSectors) = 0;    
    virtual BOOL SecureWipe() = 0;    
    virtual DWORD SetSecureWipeFlag(DWORD dwStartBlock) = 0;        

protected:
    virtual BOOL BuildupMappingInfo() = 0;
    virtual DWORD InternalWriteToMedia(DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuffer) = 0;


protected:
    PFlashRegion m_pRegion;

    DWORD m_dwSectorsPerBlock;

    DWORD m_dwStartLogSector;
    DWORD m_dwNumLogSectors;    
    DWORD m_dwStartPhysSector;   

    BOOL m_fReadOnly;

public:
    MappingTable* m_pMap;
    SectorMgr* m_pSectorMgr;
};

class XipFal : public Fal
{
public:
    XipFal (DWORD dwStartLogSector, DWORD dwStartPhysSector, BOOL fReadOnly) : 
        Fal(dwStartLogSector, dwStartPhysSector, fReadOnly) {}
        
    virtual BOOL StartupFAL(PFlashRegion pRegion);
    virtual BOOL DeleteSectors(DWORD dwStartLogSector, DWORD dwNumSectors);
    virtual BOOL SecureWipe();
    virtual DWORD SetSecureWipeFlag(DWORD dwStartBlock);

protected:               
    virtual BOOL BuildupMappingInfo();
    virtual DWORD InternalWriteToMedia(DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuffer);
};

class FileSysFal : public Fal
{
public:
    FileSysFal (DWORD dwStartLogSector, DWORD dwStartPhysSector, BOOL fReadOnly);
    ~FileSysFal();
    
    virtual BOOL StartupFAL(PFlashRegion pRegion);
    virtual BOOL ShutdownFAL();
    virtual BOOL DeleteSectors(DWORD dwStartLogSector, DWORD dwNumSectors);    
    virtual BOOL SecureWipe();
    virtual DWORD SetSecureWipeFlag(DWORD dwStartBlock);

    BOOL HandleWriteFailure(SECTOR_ADDR physicalSectorAddr);

protected:    
    virtual BOOL BuildupMappingInfo();
    virtual DWORD InternalWriteToMedia(DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuffer);

private:
    Compactor* m_pCompactor;      
};

#endif _DSK_H_
