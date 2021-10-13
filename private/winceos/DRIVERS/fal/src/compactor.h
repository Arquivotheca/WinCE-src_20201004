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

Module Name:    COMPACTOR.H

Abstract:       Compactor module
  
-----------------------------------------------------------------------------*/

#ifndef _COMPACTOR_H_
#define _COMPACTOR_H_ 

class SectorMgr;
class MappingTable;
class FileSysFal;

#include <windows.h>
#include "falmain.h"
#include "sectormgr.h"
#include "log2physmap.h"
#include "fal.h"

#define MINIMUM_FLASH_BLOCKS_TO_RESERVE                2
#define DEFAULT_FLASH_BLOCKS_TO_RESERVE                4

#define PERCENTAGE_OF_MEDIA_TO_RESERVE                400        // 0.25% of the media {NOTE: 100% / 0.25% == 400}
#define CRITICAL_SECTORS_TO_RECLAIM                 8       // Number of DIRTY sectors to reclaim during a critical compaction

#define MAX_NUM_NESTED_COMPACTIONS                  3       // Maximum # "nested" compactions before Compactor gives up...
#define USE_SECTOR_MAPPING_INFO                     0xFFFFFFFF

#define BLOCK_COMPACTION_ERROR                       0xFFFFFFFF

//---------------------------- Structure Definitions -----------------------------

//------------------------------- Public Interface ------------------------------
class Compactor
{
public:    
    BOOL Init(PFlashRegion pRegion, FileSysFal* pFal);
    BOOL  Deinit();
    BOOL  StartCompactor(DWORD dwPriority, DWORD dwFreeSectorsNeeded, BOOL bCritical);
    BOOL  StopCompactor(VOID);
    BOOL  GetCompactingBlock(PBLOCK_ID pBlockID);
    BOOL  SetCompactingBlock(BLOCK_ID blockID);

    DWORD     CompactBlock(BLOCK_ID blockID, SECTOR_ADDR treatAsDirtySectorAddr); 
    BLOCK_ID  GetNextCompactionBlock(BLOCK_ID blockID);

    DWORD CompactorThread();

protected:
    BLOCK_ID GetDirtiestBlock(BLOCK_ID blockID);
    BLOCK_ID GetNextRandomBlock(BLOCK_ID blockID);
    
private:
    HANDLE m_hCompactorThread;    // Handle to compactor thread
    DWORD m_dwThreadID;           // Compactor "work" thread's ID
    HANDLE m_hStartCompacting;    // Handle to event that "triggers" compaction
    HANDLE m_hCompactionComplete; // Handle to event that "signals" when a compaction completes
    BOOL m_bEndCompactionThread;  // Flag to exit the compaction thread

    BLOCK_ID m_compactingBlockID; // ID for the block currently being compacted
    DWORD m_dwFreeSectorsNeeded;  // Number of free sectors needed in the current compaction
    DWORD m_dwNestedCompactions;  // Number of nested (a.k.a. re-entrant) compactions in progress

    PBYTE m_pSectorData;          // Pointer to buffer used to move sector data
    BOOL m_bCritical;

    FMDInterface m_FMD;
    PFlashRegion m_pRegion;
    FileSysFal* m_pFal;
    SectorMgr* m_pSectorMgr;
    MappingTable* m_pMap;

};
//------------------------------ Helper Functions ------------------------------
DWORD     WINAPI CompactorThread(LPVOID lpParameter);

#endif _COMPACTOR_H_
