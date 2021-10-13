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

Module Name:    FALMAIN.H

Abstract:       FLASH Abstraction Layer Interface for Windows CE 
  
-----------------------------------------------------------------------------*/

#ifndef _FALMAIN_H_
#define _FALMAIN_H_ 

class Fal;
class MappingTable;
class SectorMgr;
class Compactor;


#include <windows.h>
#include <diskio.h>
#include <devload.h>
#include "fmd.h"
#include "pm.h"

#define NOT_A_POWER_OF_2            0xFF

//----------------------------- Debug zone information ------------------------------
extern  DBGPARAM    dpCurSettings;

#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_ERROR      DEBUGZONE(1)
#define ZONE_COMPACTOR  DEBUGZONE(2)
#define ZONE_WRITE_OPS  DEBUGZONE(3)
#define ZONE_READ_OPS   DEBUGZONE(4)
#define ZONE_FUNCTION   DEBUGZONE(5)

#define ZONE_CELOG_ERROR       DEBUGZONE(13)
#define ZONE_CELOG_COMPACTION  DEBUGZONE(14)
#define ZONE_CELOG_VERBOSE     DEBUGZONE(15)

//----------------------------------------------------------------------------------
#include "celogfal.h"



//--------------------------------- #DEFINEs for SectorMappingInfo --------------------------------------

// Applies to logicalSectorAddr
#define UNMAPPED_LOGICAL_SECTOR         0xFFFFFFFF

// Applies to fDataStatus
#define  FREE_SECTOR                    0xFFFF                  // Indicates sector is free (erased)
#define  DIRTY_SECTOR                   0x0001                  // Indicates sector is ready to erase
#define  SECTOR_WRITE_IN_PROGRESS       0x0002                  // Indicates sector write is in progress
#define  SECTOR_WRITE_COMPLETED         0x0004                  // Indicates sector write completed (data is valid)
#define  COMPACTION_IN_PROGRESS         0x0008                  // Indicates previous block is being compacted
#define  COMPACTION_COMPLETED           0x0010                  // Indicates previous block compaction completed
#define  SECURE_WIPE_IN_PROGRESS        0x0020                  // Indicates secure wipe of region is in progress

#define IsSectorFree(x)                  (x.fDataStatus == FREE_SECTOR)             
#define IsSectorDirty(x)                (!(x.fDataStatus & DIRTY_SECTOR) || (IsSectorWriteInProgress(x) && !IsSectorWriteCompleted(x)))
#define IsSectorWriteInProgress(x)      !(x.fDataStatus & SECTOR_WRITE_IN_PROGRESS)
#define IsSectorWriteCompleted(x)       !(x.fDataStatus & SECTOR_WRITE_COMPLETED)
#define IsSectorMapped(x)               (!IsSectorDirty(x) && IsSectorWriteCompleted(x))
#define IsCompactionInProgress(x)       !(x.fDataStatus & COMPACTION_IN_PROGRESS)
#define IsCompactionCompleted(x)        !(x.fDataStatus & COMPACTION_COMPLETED)
#define IsSecureWipeInProgress(x)       !(x.fDataStatus & SECURE_WIPE_IN_PROGRESS)
#define IsReadOnlyBlock(x)              !(x.bOEMReserved & OEM_BLOCK_READONLY)
#define IsReservedBlock(x)              !(x.bOEMReserved & OEM_BLOCK_RESERVED)

#define MarkSectorFree(x)               (x.fDataStatus  = 0xFFFF)
#define MarkSectorDirty(x)              (x.fDataStatus &= ~DIRTY_SECTOR)
#define MarkSectorWriteInProgress(x)    (x.fDataStatus &= ~SECTOR_WRITE_IN_PROGRESS)
#define MarkSectorWriteCompleted(x)     (x.fDataStatus &= ~SECTOR_WRITE_COMPLETED)
#define MarkCompactionInProgress(x)     (x.fDataStatus &= ~COMPACTION_IN_PROGRESS)
#define MarkCompactionCompleted(x)      (x.fDataStatus &= ~COMPACTION_COMPLETED)
#define MarkSecureWipeInProgress(x)     (x.fDataStatus &= ~SECURE_WIPE_IN_PROGRESS)

#define IsBlockBad(blockID) ((FMD.pGetBlockStatus (blockID) & BLOCK_STATUS_BAD) > 0)
#define IsBlockWriteable(blockID) ((FMD.pGetBlockStatus (blockID) & (BLOCK_STATUS_BAD|BLOCK_STATUS_READONLY|BLOCK_STATUS_RESERVED)) == 0)

// Applies to fMediaStatus
#define  INVALID_SECTOR         0x01                            // Indicates sector can no longer be written/erased

//------------------------------------------------------------------------------------------------------

extern DWORD g_dwAvailableSectors;
extern FMDInterface FMD;
extern PFlashInfoEx g_pFlashMediaInfo;

//--------------------------- Structure Definitions -----------------------------
typedef struct _SectorMappingInfo
{
    SECTOR_ADDR  logicalSectorAddr;
    BYTE         bOEMReserved;          // For use by OEM
    BYTE         bBadBlock;             // Indicates if block is BAD
    WORD         fDataStatus;
    
} SectorMappingInfo, *PSectorMappingInfo;


//
//  Physical Device Object controlled by this driver
//  All per instance data stored here
//
typedef struct _DEVICE
{
    DWORD   dwID;
    PVOID   hFMD;
    PVOID   hFMDHook;

} DEVICE , *PDEVICE;


//------------------------------- Public Interface ------------------------------
extern "C" 
{
DWORD DSK_Init(DWORD dwContext);
BOOL  DSK_Deinit(DWORD dwContext);
DWORD DSK_Open(DWORD dwData, DWORD dwAccess, DWORD dwShareMode);
BOOL  DSK_Close(DWORD Handle);
DWORD DSK_Read(DWORD Handle, LPVOID pBuffer, DWORD dwNumBytes);
DWORD DSK_Write(DWORD Handle, LPCVOID pBuffer, DWORD dwInBytes);
DWORD DSK_Seek(DWORD Handle, long lDistance, DWORD dwMoveMethod);
BOOL  DSK_IOControl(DWORD Handle, DWORD dwIoControlCode, PBYTE pInBuf, DWORD nInBufSize,
                    PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned);
VOID  DSK_PowerUp(VOID);
VOID  DSK_PowerDown(VOID);
}
//------------------------------- Helper Functions ------------------------------
BOOL GetDiskInfo(PDISK_INFO pDiskInfo);
BOOL ReadFromMedia(PSG_REQ pSG_req);
BOOL WriteToMedia(PSG_REQ pSG_req);
BOOL FormatMedia(VOID);
BOOL DeleteSectors(PDELETE_SECTOR_INFO pDeleteSectorInfo);
BOOL SecureWipe(PDELETE_SECTOR_INFO pDeleteSectorInfo);
BOOL SetSecureWipeFlag(PDELETE_SECTOR_INFO pDeleteSectorInfo);
BOOL GetPowerCapabilities(PPOWER_CAPABILITIES ppc);
BOOL SetPowerState (PCEDEVICE_POWER_STATE pNewPowerState);
BOOL GetPowerState (PCEDEVICE_POWER_STATE pPowerState);

UCHAR ComputeLog2(DWORD dwNum);
VOID GetFMDInterface(PDEVICE pDevice);
Fal* GetFALObject (DWORD dwStartSector, DWORD dwNumSectors);
BOOL CheckSg (PSG_REQ pSG_req, BOOL fRead, LPBOOL pfCombineSg, LPDWORD pdwTotalSize);
BOOL GetPhysSectorAddr (PSECTOR_ADDR pLogicalSectors, PSECTOR_ADDR pPhysAddrs, DWORD dwNumSectors);
BOOL CalculateLogicalRange(PFlashRegion pRegion);
BOOL CalculatePhysRange(PFlashRegion pRegion);

typedef  LONG  (*pRegCloseKey) (     HKEY     );

typedef HKEY (*pOpenDeviceKey) (LPCTSTR ActiveKey);

extern CRITICAL_SECTION   g_csFlashDevice;                  // Used to make driver re-entrant

#endif _FALMAIN_H_

