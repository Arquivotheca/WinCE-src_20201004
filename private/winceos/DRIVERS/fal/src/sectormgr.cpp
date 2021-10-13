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

Module Name:    SECTORMGR.C

Abstract:       Sector Manager module

Notes:          The following module is responsible for managing the FREE and
                DIRTY sectors on the FLASH media.

                To store the list of FREE sectors on the media, a form of compression 
                called Consecutive Item Compression (CIC) is used.  CIC is data structure
                that lends itself to data that appears consecutively (a.k.a. a "run" of
                items) but that may have "holes" in it.  A run of data is represented by 
                a node that stores the first number in the run, the last number in the run,
                and a pointer to the next node.  Notice that if we use a DWORD for storing
                the first/last number, a "run" of 2^64 numbers can be represented (assuming
                wrap-around) by using only one node.
                
                As an example, consider the number sequence 

                    0,1,2,3,4,5,6,7,8,12,13,14,15,16,17,18,20,21,400,401,402...1082,23576,etc.  

                In this example, we can see that there are 3 distinct sets of "runs" in the 
                data and 2 holes.  Using CIC, this data stream could be represented as follows:

                     --------------       ---------------       -----------------
                    | firstNum = 0 |     | firstNum = 12 |     | firstNum = 400  |
                    | lastNum  = 8 | --> | lastNum  = 21 | --> | lastNum  = 1082 | --> etc.
                     --------------       ---------------       -----------------

                The *reason* that CIC works so well for representing the FREE sector list
                is because the compactor ALWAYS compacts blocks in consecutive order AND
                frees an entire block of sectors at a time.  Practically, this means that after
                a compaction process a block's worth of sectors can almost always be just added 
                to the last node (the only exception is if BAD blocks occur).  For example, if 
                we suppose that there are 32 sectors in a block (and the blocks are good), then
                the compactor can add a block of FREE sectors to the Sector Manager by just 
                incrementing the lastNum variable in the last node by 32.  Note that no extra
                memory was needed to account for these 32 new FREE sectors.

                Using this argument, one can quickly see that ALL free sectors on the media can
                be represented in CIC by just one node!  That is, no matter how large the underlying
                FLASH media, all of the FREE sectors (barring BAD blocks) can be represented by
                just 12 bytes (the size of one node).  In reality, however, there are typically a few
                BAD blocks on the device (i.e. NAND devices) and a couple of nodes (around 24-36 bytes)
                are needed to represent the FREE sector list.
                
                The last important point to note about CIC is that it allows O(1) insertion and O(1)
                deletion from the FREE sector list.  Again, insertion is simply incrementing the lastNum
                variable of the last node in the list (following the tail pointer) and deletion is simply
                incrementing the firstNum variable in the first node in the list (following the head pointer).
                Note that even if a new node must be allocated (to account for a BAD block), the worst case
                insertion/deletion times are still constant time (a.k.a. O(1)).


-----------------------------------------------------------------------------*/

#include "sectormgr.h"

//------------------------------ GLOBALS -------------------------------------------


extern DWORD g_dwCompactionPrio256;
extern DWORD g_dwCompactionCritPrio256;

//----------------------------------------------------------------------------------

BOOL DirtyList::Init (PFlashRegion pRegion)
{
    m_pRegion = pRegion;
    m_cbDirtyCountEntry = (pRegion->dwSectorsPerBlock > 0xff) ? 2 : 1;    
    
    m_pDirtyList = (LPBYTE) LocalAlloc (LPTR, pRegion->dwNumPhysBlocks * m_cbDirtyCountEntry);

    return (m_pDirtyList != NULL);
}

BOOL DirtyList::Deinit()
{
    LocalFree (m_pDirtyList);
    return TRUE;
}

VOID DirtyList::AddDirtySectors(DWORD dwBlock, DWORD dwNumSectors)
{
    DWORD dwIndex = dwBlock - m_pRegion->dwStartPhysBlock;
    
    if (m_cbDirtyCountEntry == 1)
    {
        m_pDirtyList[dwIndex] += (BYTE)dwNumSectors;
        ASSERT (m_pDirtyList[dwIndex] <= m_pRegion->dwSectorsPerBlock);
    }
    else 
    {
        LPWORD pwList = (LPWORD)m_pDirtyList;
        pwList[dwIndex] += (WORD)dwNumSectors;
        ASSERT (pwList[dwIndex] <= m_pRegion->dwSectorsPerBlock);
    }
}    

BOOL DirtyList::RemoveDirtySectors (DWORD dwBlock, DWORD dwNumSectors)
{
    DWORD dwIndex = dwBlock - m_pRegion->dwStartPhysBlock;
    
    if (m_cbDirtyCountEntry == 1)
    {
        if (m_pDirtyList[dwIndex] < (BYTE)dwNumSectors)
        {
            RETAILMSG (1, (TEXT("SectorMgr::UnmarkSectorsAsDirty: Error, Invalid dirty sector count")));
            return FALSE;
        }
        
        m_pDirtyList[dwIndex] -= (BYTE)dwNumSectors;
    }
    else 
    {
        LPWORD pwList = (LPWORD)m_pDirtyList;
        if (pwList[dwIndex] < (WORD)dwNumSectors)
        {
            RETAILMSG (1, (TEXT("SectorMgr::UnmarkSectorsAsDirty: Error, Invalid dirty sector count")));
            return FALSE; 
        }
        
        pwList[dwIndex] -= (WORD)dwNumSectors;
    }
    return TRUE;
}

DWORD DirtyList::GetDirtyCount(BLOCK_ID blockID)
{
    DWORD dwIndex = blockID - m_pRegion->dwStartPhysBlock;
    return (m_cbDirtyCountEntry == 1) ? m_pDirtyList[dwIndex] : ((LPWORD)m_pDirtyList)[dwIndex];          
}


VOID SMList::Init()
{
    m_head = NULL; 
    m_cursor = NULL; 
    m_tail = NULL; 
    m_dwNumSectors = 0;    
}

VOID SMList::Deinit()
{
    //----- 1. Traverse the free sector linked-list and free all the nodes -----
    while(m_head != NULL)
    {
        m_cursor = m_head;                            // Set cursor to front of list
        m_head = m_head->pNext;                       // Advance head pointer

        //----- 2. Free the node.  If LocalFree() fails, just keep going anyway -----
        if(LocalFree(m_cursor) != NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:DeinitMapper() - Unable to deallocate node!\r\n")));
        }
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::AddSectorsToList()

Description:    Marks the specified  sectors to the list.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/

BOOL SMList::AddSectors (SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors)
{   
    //----- 1. Can we put these physical sector addresses into the last node in the list? -----
    if((m_tail != NULL) && ((m_tail->lastSectorAddr+1) == startingSectorAddr))
    {
        m_tail->lastSectorAddr += dwNumSectors;
    
    }else
    {
        //----- 2. We can NOT add these physical sector address to the last node, let's create a new node -----
        if((m_cursor = (PSMNode)LocalAlloc(LPTR, sizeof(SMNode))) == NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:SectorMgr::MarkSectorAsFree() Unable to create node for sector %x!\r\n"), startingSectorAddr));
            goto MARK_ERROR;
        }

        //----- 3. Copy in physical sector address -----
        m_cursor->startSectorAddr = startingSectorAddr;
        m_cursor->lastSectorAddr  = (startingSectorAddr + dwNumSectors - 1);
        m_cursor->pNext                   = NULL;

        //----- 4. Add to the end of free sector list -----
        if(m_tail == NULL)
        {
            m_tail = m_cursor;
            m_head = m_cursor;
        }else
        {
            m_tail->pNext = m_cursor;
            m_tail = m_cursor;
        }
    }
    
    //----- 5. Increase the number of free sectors -----
    m_dwNumSectors += dwNumSectors;

    return TRUE;

MARK_ERROR:
    return FALSE;
    
}


DWORD SMList::AreSectorsInList (DWORD dwStartSector, DWORD dwNumSectors)
{
    PSMNode tempNode = m_head;
    DWORD dwEndSector = dwStartSector + dwNumSectors - 1;
    
    while(tempNode != NULL)
    {
        if ((dwStartSector >= tempNode->startSectorAddr) && (dwEndSector <= tempNode->lastSectorAddr))
        {
            return SectorMgr::EntirelyInList;        
        }
        else if ((dwStartSector <= tempNode->lastSectorAddr) && (dwEndSector >= tempNode->startSectorAddr))
        {
            return SectorMgr::PartiallyInList;
        }
        tempNode = tempNode->pNext;
    }

    return SectorMgr::NotInList;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::InitSectorManager()

Description:    Initializes the Sector Manager.

Notes:          This function should be called at driver initialization.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::Init(PFlashRegion pRegion, Fal* pFal, Compactor* pCompactor)
{
    m_pFal = pFal;
    m_pCompactor = pCompactor;
    m_pRegion = pRegion;
    
    //----- 1. Initialize the free sector linked list -----
    for (int i = 0; i < NUM_LIST_TYPES; i++) {
        m_lists[i].Init();
    }
    //m_freeList.Init();
    //m_readOnlyList.Init();
    //m_XIPList.Init();
   
    //----- 2. Initialize the number of DIRTY sectors -----
    m_dwNumDirtySectors = 0;

    m_dwNumUnusableBlocks = 0;

    //----- 3. Compute the compaction thresholds. -----
    m_dwCritCompactThreshold = 2 * pRegion->dwSectorsPerBlock;

    if (!m_dirtyList.Init(pRegion)) {
        return FALSE;
    }

    return TRUE;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::DeinitSectorManager()

Description:    Determines if the specified physical sector is free for use.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::Deinit()
{   
    for (int i = 0; i < NUM_LIST_TYPES; i++) {
        m_lists[i].Deinit();
    }

    m_dirtyList.Deinit();
    
    return TRUE;
}

BOOL SectorMgr::AddSectorsToList (ListType listType, SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors)
{
    return m_lists[listType].AddSectors (startingSectorAddr, dwNumSectors);
}

DWORD SectorMgr::AreSectorsInList (ListType listType, DWORD dwStartSector, DWORD dwNumSectors)
{
    return m_lists[listType].AreSectorsInList (dwStartSector, dwNumSectors);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::UnmarkSectorsAsFree()

Description:    Unmarks the specified physical sectors as being FREE.

Notes:          Currently, this routine assumes that the specified physical 
                sectors are consecutively arranged starting from the starting
                sector address of the node containing the free sectors to
                unmark.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::UnmarkSectorsAsFree(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors)
{
    SMList* pFreeList = &m_lists[Free];
    PSMNode pPrevNode = NULL;

    DWORD endSectorAddr = startingPhysicalSectorAddr + dwNumSectors - 1;
    
    //----- 1. Obtain a pointer to the first node in the FREE sector list -----
    if((pFreeList->m_cursor = pFreeList->m_head) == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:SectorMgr::UnmarkSectorsAsFree() - Unable to unmark sectors 0x%08x - 0x%08x as FREE\r\n"), 
                           startingPhysicalSectorAddr, endSectorAddr));
        return FALSE;
    }

    //----- 2. Find the node that falls within the specified range... -----
    while ((pFreeList->m_cursor->startSectorAddr <  startingPhysicalSectorAddr) ||
          (pFreeList->m_cursor->startSectorAddr > endSectorAddr))
    {
        pPrevNode = pFreeList->m_cursor;
        pFreeList->m_cursor = pFreeList->m_cursor->pNext;
        if (pFreeList->m_cursor == NULL)
        {
            // The block doesn't have any free sectors.
            return TRUE;
        }
    }
    
    //----- 3. Remove the specified sectors from this node -----
    while (pFreeList->m_cursor->startSectorAddr <= endSectorAddr)
    {
        pFreeList->m_dwNumSectors--;
        pFreeList->m_cursor->startSectorAddr++;
    }

    //----- 4. If this is the last physical sector address for this node, remove the node from the free sector list -----
    if(pFreeList->m_cursor->startSectorAddr >= (pFreeList->m_cursor->lastSectorAddr+1))
    {
        if (pPrevNode)
        {
            // Not removing head node
            pPrevNode->pNext = pFreeList->m_cursor->pNext;
        }
        else
        {
            // Removing head node
            pFreeList->m_head = pFreeList->m_cursor->pNext;
            ASSERT(pFreeList->m_head != NULL);
        }

        if (pFreeList->m_cursor == pFreeList->m_tail)
        {
            pFreeList->m_tail = pPrevNode;
        }

        if(LocalFree(pFreeList->m_cursor) != NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:SectorMgr::UnmarkSectorsAsFree() - Unable to free physical sector node memory!\r\n")));
        }
        pFreeList->m_cursor = NULL;
    }           

    return TRUE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::GetNextFreeSector()

Description:    Returns the next FREE physical sector (if there is one).  This 
                physical sector is also REMOVED from the free sector list.  

Notes:          This function automatically determines when/if the Compactor 
                needs to be restarted in order to reclaim DIRTY sectors and 
                recycle them into FREE sectors.  However, if bCritical == TRUE,
                the compaction step is SKIPPED and free sectors are returned
                until the supply is exhausted.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::GetNextFreeSector(PSECTOR_ADDR pPhysicalSectorAddr, BOOL bCritical)
{
    SMList* pFreeList = &m_lists[Free];
    
    //----- 1. Check the parameters to insure they are legitimate -----
    if(pPhysicalSectorAddr == NULL)
    {
        goto GETFREE_ERROR;
    }

    //----- 2. Is this request critical?  If so, SKIP compaction and simply try to fulfill the request -----
    if(m_pCompactor && (bCritical == FALSE))
    {
        //----- 3. Request is NOT critical; do we need to start the compactor? -----
        if(pFreeList->m_dwNumSectors <= m_dwCritCompactThreshold)
        {
            DWORD dwCritPriority = g_dwCompactionCritPrio256;
            if (dwCritPriority == 0xffffffff)
            {
                // Setting priority to -1 means that we should use the caller's thread priority if it is above normal
                dwCritPriority = min (CeGetThreadPriority (GetCurrentThread()), THREAD_PRIORITY_NORMAL + 248);
            }
        
            //----- 4. Critical threshold exceeded!  We must compact BEFORE we can return a free sector. -----
            // NOTE: This call is SYNCHRONOUS (a.k.a. it will not return until the compaction stage completes)
            if(!m_pCompactor->StartCompactor(dwCritPriority, CRITICAL_SECTORS_TO_RECLAIM, TRUE))
            {
                ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to start compactor in critical situation!!!\r\n")));
            }
            
            // Compaction stage complete - check to make sure that some free sectors were reclaimed.  If no free sectors
            // were reclaimed, then we must FAIL
            if(pFreeList->m_dwNumSectors <= m_dwCritCompactThreshold)
            {
                ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to reclaim any free sectors in a critical compaction stage.  Media must be full.\r\n")));
                goto GETFREE_ERROR;
            }

        }
        else if(pFreeList->m_dwNumSectors < m_dwNumDirtySectors)
        {
            //----- 5. "Service" threshold exceeded (i.e. there are more DIRTY sectors than FREE sectors on the media) -----
            //          Start the compactor in the background (asynchronously) to reclaim these sectors
            if(!m_pCompactor->StartCompactor(g_dwCompactionPrio256, m_dwNumDirtySectors, FALSE))
            {
                ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to start compactor in background!!!\r\n")));
            }
        }
    }

    //----- 6. Are we out of free sectors entirely? -----
    if(((pFreeList->m_cursor = pFreeList->m_head) == NULL) || (pFreeList->m_dwNumSectors == 0))
    {
        ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to retreive free physical sector... ALL OUT!!!\r\n")));
        goto GETFREE_ERROR;
    }

    //----- 7. Copy the physical sector address -----
    *pPhysicalSectorAddr = pFreeList->m_cursor->startSectorAddr++;

    //----- 8. If this is the last physical sector address for this node, remove the node from the free sector list -----
    if(pFreeList->m_cursor->startSectorAddr >= (pFreeList->m_cursor->lastSectorAddr+1))
    {
        pFreeList->m_head = pFreeList->m_head->pNext;

        if(LocalFree(pFreeList->m_cursor) != NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to free physical sector %x node memory!\r\n"), *pPhysicalSectorAddr));
            goto GETFREE_ERROR;
        }
        pFreeList->m_cursor = NULL;
    }

    //----- 9. Decrease the number of free sectors -----
    pFreeList->m_dwNumSectors--;
    
    return TRUE;

GETFREE_ERROR:
    return FALSE;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::GetNumberOfFreeSectors()

Description:    Returns the number of FREE physical sectors.

Returns:        Number of free physical sectors.
------------------------------------------------------------------------------*/
DWORD SectorMgr::GetNumberOfFreeSectors(VOID)
{
    return m_lists[Free].m_dwNumSectors;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::IsBlockFree()

Description:    Determines whether entire block contains free sectors.

------------------------------------------------------------------------------*/
BOOL SectorMgr::IsBlockFree(BLOCK_ID blockID)
{
    SECTOR_ADDR startSector = GetStartSectorInBlock(blockID);
    return m_lists[Free].AreSectorsInList(startSector, m_pRegion->dwSectorsPerBlock) == EntirelyInList;
}

//******************************************************************************//


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::MarkSectorsAsDirty()

Description:    Marks the specified physical sectors as being DIRTY.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::MarkSectorsAsDirty(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors)
{
    // Since dwNumSectors always is 1, no need to check if the sectors span a block
    // This code needs to be modified if the assumption no longer holds.
    DWORD dwBlock = GetBlockFromSector (startingPhysicalSectorAddr);

    if (dwBlock < m_pRegion->dwStartPhysBlock || dwBlock >= (m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks))
    {
        RETAILMSG (1, (TEXT("MarkSectorsAsDirty: Invalid Block 0x%x\r\n"), dwBlock));
        return FALSE;
    }

    m_dirtyList.AddDirtySectors (dwBlock, dwNumSectors);

    m_dwNumDirtySectors += dwNumSectors;
   
    return TRUE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::UnmarkSectorsAsDirty()

Description:    Unmarks the specified physical sectors as being DIRTY.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::UnmarkSectorsAsDirty(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors)
{
    // Since dwNumSectors always is 1, no need to check if the sectors span a block
    // This code needs to be modified if the assumption no longer holds.
    DWORD dwBlock = GetBlockFromSector (startingPhysicalSectorAddr);

    if (dwBlock < m_pRegion->dwStartPhysBlock || dwBlock >= (m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks))
    {
        RETAILMSG (1, (TEXT("UnmarkSectorsAsDirty: Invalid Block 0x%x\r\n"), dwBlock));
        return FALSE;
    }

    if (!m_dirtyList.RemoveDirtySectors (dwBlock, dwNumSectors))
        return FALSE;

    ASSERT (m_dwNumDirtySectors >= dwNumSectors);

    m_dwNumDirtySectors -= dwNumSectors;
    return TRUE;
}

DWORD SectorMgr::GetDirtyCount(BLOCK_ID blockID)
{
    return m_dirtyList.GetDirtyCount (blockID);
}


//------------------------- General Purpose Public Routines --------------------------


SECTOR_ADDR SectorMgr::GetStartSectorInBlock (DWORD dwBlockID)
{
    return m_pFal->GetStartPhysSector() + (dwBlockID - m_pRegion->dwStartPhysBlock) * m_pRegion->dwSectorsPerBlock;
}

BLOCK_ID SectorMgr::GetBlockFromSector (DWORD dwSector)
{
    return m_pRegion->dwStartPhysBlock + (dwSector - m_pFal->GetStartPhysSector()) / m_pRegion->dwSectorsPerBlock;
}


//-------------------------------- Helper Functions -------------------------------





