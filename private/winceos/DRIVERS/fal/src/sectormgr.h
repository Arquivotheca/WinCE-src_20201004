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

Module Name:    SECTORMGR.H

Abstract:       Sector Manager module
  
-----------------------------------------------------------------------------*/

#ifndef _SECTORMGR_H_
#define _SECTORMGR_H_ 

class Fal;
class Compactor;

#include <windows.h>
#include "falmain.h"
#include "compactor.h"
#include "fal.h"


#define NUM_LIST_TYPES 3

//---------------------------- Structure Definitions -----------------------------
typedef struct _SMNode
{
    SECTOR_ADDR        startSectorAddr;
    SECTOR_ADDR        lastSectorAddr;
    struct _SMNode*    pNext;

} SMNode, *PSMNode;

class SMList
{
    friend class SectorMgr;
    
public:
    VOID Init();
    VOID Deinit();
    BOOL AddSectors (SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors);
    DWORD AreSectorsInList (DWORD dwStartSector, DWORD dwNumSectors);

private:    
    PSMNode m_head;           // Pointer to head of free sector linked-list
    PSMNode m_cursor;         // Pointer to current free sector linked-list
    PSMNode m_tail;           // Pointer to tail of free sector linked-list
    DWORD m_dwNumSectors;
}; 

class DirtyList
{
public:
    BOOL Init (PFlashRegion pRegion);
    BOOL Deinit();
    VOID AddDirtySectors(DWORD dwBlock, DWORD dwNumSectors);
    BOOL RemoveDirtySectors (DWORD dwBlock, DWORD dwNumSectors);
    DWORD GetDirtyCount(BLOCK_ID blockID);

private:
    LPBYTE m_pDirtyList;
    DWORD m_cbDirtyCountEntry;
    PFlashRegion m_pRegion;
};

//------------------------------- Public Interface ------------------------------
class SectorMgr 
{    
public:
    enum ListType { Free, ReadOnly, XIP };

    // Return values for SM_AreSectorsInList
    enum SectorsInList { NotInList, PartiallyInList, EntirelyInList };

    
public:
    BOOL Init(PFlashRegion pRegion, Fal* pFal, Compactor* pCompactor);
    BOOL Deinit();

    BOOL AddSectorsToList (ListType listType, SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors);
    BOOL  UnmarkSectorsAsFree(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors);

    DWORD AreSectorsInList (ListType listType, DWORD dwStartSector, DWORD dwNumSectors);

    BOOL  GetNextFreeSector(PSECTOR_ADDR pPhysicalSectorAddr, BOOL bCritical);
    DWORD GetNumberOfFreeSectors(VOID);
    BOOL  IsBlockFree(BLOCK_ID blockID);

    BOOL  MarkSectorsAsDirty(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors);
    BOOL  UnmarkSectorsAsDirty(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors);
    DWORD GetDirtyCount(BLOCK_ID blockID);

    SECTOR_ADDR GetStartSectorInBlock (BLOCK_ID blockID);
    BLOCK_ID GetBlockFromSector (SECTOR_ADDR sector);


public:
    inline DWORD GetNumDirtySectors() { return m_dwNumDirtySectors; }
    inline BOOL MarkBlockUnusable (BLOCK_ID blockID) { m_dwNumUnusableBlocks++; return TRUE; }
    inline DWORD GetNumUnusableBlocks() { return m_dwNumUnusableBlocks; }
    inline SMList* GetList(ListType type) { return &m_lists[type]; }


private:
    SMList m_lists[NUM_LIST_TYPES];

    DirtyList m_dirtyList;

    DWORD   m_dwNumDirtySectors;       // Number of dirty sectors on the FLASH media
    DWORD   m_dwNumUnusableBlocks;     // Number of BAD or RESERVED blocks on the FLASH media

    DWORD   m_dwCritCompactThreshold;  // Determines when compaction must occur before a WRITE can complete

    PFlashRegion m_pRegion;
    Fal* m_pFal;
    Compactor* m_pCompactor;
    
};

//------------------------------ Helper Functions ------------------------------

#endif _SECTORMGR_H_
