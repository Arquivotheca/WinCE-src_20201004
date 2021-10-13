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
    
    m_pDirtyList = (LPWORD) LocalAlloc (LPTR, pRegion->dwNumPhysBlocks * sizeof(WORD));
    ZeroMemory(m_pDirtyList, pRegion->dwNumPhysBlocks * sizeof(WORD));

    return (m_pDirtyList != NULL);
}

BOOL DirtyList::Deinit()
{
    LocalFree (m_pDirtyList);
    m_pDirtyList = NULL;
    return TRUE;
}

VOID DirtyList::AddDirtySectors(DWORD dwBlock, DWORD dwNumSectors)
{
    DWORD dwIndex = dwBlock - m_pRegion->dwStartPhysBlock;
    
    do {
        if (m_pDirtyList[dwIndex]==m_pRegion->dwSectorsPerBlock)
        {
            DEBUGMSG(1, (TEXT("FLASHDRV.DLL:MarkSectorsAsDirty() - trying to dirty a sector from a completely dirty block \r\n")));
        }
        else
        {
            m_pDirtyList[dwIndex]++;
        }
    } while (--dwNumSectors);
}    

BOOL DirtyList::RemoveDirtySectors (DWORD dwBlock, DWORD dwNumSectors)
{
    DWORD dwIndex = dwBlock - m_pRegion->dwStartPhysBlock;
    
    do {
        if (m_pDirtyList[dwIndex])
        {
            m_pDirtyList[dwIndex]--;
        }
        else
        {
            DEBUGMSG(1, (TEXT("FLASHDRV.DLL:UnmarkSectorsAsDirty() - trying to undirty sector from a clean block \r\n")));
        }
    } while (--dwNumSectors);
    return TRUE;
}

DWORD DirtyList::GetDirtyCount(BLOCK_ID blockID)
{
    DWORD dwIndex = blockID - m_pRegion->dwStartPhysBlock;
    return m_pDirtyList[dwIndex];          
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
            ReportError((TEXT("FLASHDRV.DLL:Deinit() - Unable to deallocate node!\r\n")));
        }
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SMList::AddSectors()

Description:    Marks the specified  sectors to the list.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/

BOOL SMList::AddSectors (SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors)
{
    SECTOR_ADDR runEndSector;

    runEndSector = startingSectorAddr + dwNumSectors - 1;
    
    //----- 1. Can we put these physical sector addresses into the last node in the list? -----
    if((m_tail != NULL) && ((m_tail->lastSectorAddr+1) == startingSectorAddr))
    {
        m_tail->lastSectorAddr += dwNumSectors;
        //Increase the number of free sectors -----
        m_dwNumSectors += dwNumSectors;
        return TRUE;    
    }
    
    if(m_head == NULL)
    {
        /* empty list.  add a new node */
        if((m_cursor = (PSMNode)LocalAlloc(LPTR, sizeof(SMNode))) == NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:SMList::AddSectors Unable to create node for sector %x!\r\n"), startingSectorAddr));
            goto MARK_ERROR;
        }

        //Copy in physical sector address -----
        m_cursor->startSectorAddr = startingSectorAddr;
        m_cursor->lastSectorAddr  = runEndSector;
        m_cursor->pNext                   = NULL;

        m_tail = m_cursor;
        m_head = m_cursor;
        //Increase the number of free sectors -----
        m_dwNumSectors += dwNumSectors;
        return TRUE;    
    }
    
    /* --------------------------------------------- */
    /* if we get to this point the list is not empty */
    /* --------------------------------------------- */

    if(TRUE == UnionRunWithList(startingSectorAddr, runEndSector, dwNumSectors))
    {
        return TRUE;
    }

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
Private Function: UnionRunWithList

This function takes the a run, then adds any sectors in the run that are not 
already on the list. 

Returns: Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SMList::UnionRunWithList(SECTOR_ADDR startingSectorAddr, SECTOR_ADDR runEndSector, DWORD dwNumSectors)
{
    PSMNode pCursor;
    PSMNode pPrevNode;

    /* find the node that starts at or after the current start sector */

    pCursor = m_head; /* can't be NULL */
    pPrevNode = NULL;
    do {
        if (pCursor->startSectorAddr >= startingSectorAddr)
        {
            break;
        }
        pPrevNode = pCursor;
        pCursor = pCursor->pNext;
    } while (pCursor);

    /* now iterate from this point */

    do {
        /* 
           cases:

           |PREVPREVPREVPREV|         |CURSORCURSORCURSOR|
               |---------|                                      (0)  starts after start of previous node's run and ends before its end
               |------------|                                   (1)  starts after start of previous node's run but ends exactly at its end
               |--------------->                                (2)  starts after start of previous node's run, ends after end 
                                                                     >> devolves into
                            |---|                               (3)  starts at end of existing previous node and ends before start of cursor if one exists
                            |---------|                         (4)  starts at end of existing previous node and goes exactly to existing cursor
                            |----------------|                  (5)  starts at end of existing previous node and ends somewhere within existing cursor node
                            |-------------------------------|   (6)  starts at end of existing previous node and goes past end of existing cursor node
                               |----|                           (7)  starts after previous or no previous, and ends before cursor node if one exists
                               |------|                         (8)  starts after previous or no previous, and ends at existing cursor node
                               |-------------|                  (9)  starts after previous or no previous, and ends within existing cursor node
                               |----------------------------|   (10) starts after previous or no previous, and goes past end of existing cursor node
                                      |------|                  (11) starts at existing cursor node and ends within it
                                      |---------------------|   (12) starts at existing cursor node and ends after its end

           actions:

           (0) no-op
           (1) no-op
           (2) clip to end of previous and continue
           (3) extend previous node
           (4) merge previous and cursor and gap
           (5) merge previous and cursor and gap
           (6) merge previous and cursor and gap, move start of run past start of cursor, update prev and cursor to next nodes, reiterate
           (7) create new independent node in between prev and cursor
           (8) extend cursor node
           (9) extend cursor node
           (10) extend cursor node, move start of run past start of cursor, update prev and cursor to next nodes, reiterate
           (11) no-op
           (12) move start of run past start of cursor, update prev and cursor to next nodes, reiterate
        */

        /* At least ONE of pCursor and pPrevNode will NOT be NULL */

        if (pPrevNode != NULL)
        {
            /* cases (0) -> (6) : pCursor may be NULL */

            if (startingSectorAddr <= pPrevNode->lastSectorAddr)
            {
                DWORD overlap;

                if (runEndSector <= pPrevNode->lastSectorAddr)
                {
                    // case (0) and (1) no-op
                    return TRUE;
                }
                
                // case (2) - clip out portion that overlaps the previous node and continue
                overlap = pPrevNode->lastSectorAddr - startingSectorAddr + 1;
                startingSectorAddr += overlap;
                dwNumSectors -= overlap;
            }

            /* run will no longer have any portion inside previous node at this point */

            if (startingSectorAddr == pPrevNode->lastSectorAddr+1)
            {
                DWORD numInGap;

                /* starts at end of existing previous node - case (3),(4),(5), or (6) */

                if ((pCursor==NULL) || (runEndSector < pCursor->startSectorAddr-1))
                {
                    // case (3) extend previous node
                    pPrevNode->lastSectorAddr = runEndSector;
                    m_dwNumSectors += dwNumSectors;
                    return TRUE;
                }

                /* run completely fills the gap between prev and cursor at this point, and cursor node exists */

                if (runEndSector > pCursor->lastSectorAddr)
                {
                    // case (6) move start of run past start of cursor, update prev and cursor to next nodes, reiterate
                    startingSectorAddr = pCursor->startSectorAddr+1;
                    dwNumSectors = runEndSector - startingSectorAddr + 1;
                    // this converts (6) to (4) + (11) or (12)
                }
                else
                {
                    /* cases (4) or (5). 'numInGap' is being added to prev node to completely fill the gap.
                                          setting this to zero tells us to exit right after the merge */
                    dwNumSectors = 0;
                }

                numInGap = pCursor->startSectorAddr - (pPrevNode->lastSectorAddr+1);

                // cases (4), (5), or converted (6) - merge prev node and cursor node and add gap to count of free sectors
                pPrevNode->lastSectorAddr = pCursor->lastSectorAddr;
                pPrevNode->pNext = pCursor->pNext;
                
                if (pCursor==m_tail)
                {
                    m_tail = pPrevNode;
                }

                m_dwNumSectors += numInGap;
                LocalFree(pCursor);

                if (!dwNumSectors)
                {
                    /* cases (4) or (5) */
                    return TRUE;
                }

                /* here case (6) has been converted to (11) or (12) */
                pCursor = pPrevNode->pNext;
            }
            else if (pCursor==NULL)
            {
                /* create new node after previous that is the new end of the list */
                m_cursor = (PSMNode)LocalAlloc(LPTR, sizeof(SMNode));
                if (!m_cursor)
                {
                    goto MARK_ERROR;
                }
                m_cursor->startSectorAddr = startingSectorAddr;
                m_cursor->lastSectorAddr = runEndSector;
                m_cursor->pNext = NULL;

                /* add to the end of the nonempty list */
                pPrevNode->pNext = m_cursor;
                m_tail = m_cursor;
                m_dwNumSectors += dwNumSectors;
                return TRUE;
            }
        }

        if (pCursor != NULL)
        {
            /* cases (7) -> (12) : pPrevNode may be NULL */
            if ((pCursor->startSectorAddr!=0) && (runEndSector < pCursor->startSectorAddr-1))
            {
                /* case (7) - create new independent node in between prev (if it exists) and cursor */
                m_cursor = (PSMNode)LocalAlloc(LPTR, sizeof(SMNode));
                if (!m_cursor)
                {
                    goto MARK_ERROR;
                }
                m_cursor->startSectorAddr = startingSectorAddr;
                m_cursor->lastSectorAddr = runEndSector;
                m_cursor->pNext = pCursor;

                /* add to the start of the nonempty list */
                if (!pPrevNode)
                {
                    m_head = m_cursor;
                }
                else
                {
                    pPrevNode->pNext = m_cursor;
                }
                m_dwNumSectors += dwNumSectors;
                return TRUE;
            }

            /* at this point the run ends at or after the cursor run */

            if (startingSectorAddr < pCursor->startSectorAddr)
            {
                /* run starts before the cursor run, and ends at or after it */
                /* cases (8) to (10) */
                DWORD addToStartOfCursor = pCursor->startSectorAddr - startingSectorAddr;
                pCursor->startSectorAddr = startingSectorAddr;
                m_dwNumSectors += addToStartOfCursor;
                /* this converts (8) to (10) into (11) or (12) */
            }

            /* only cases (11) and (12) left */

            if (runEndSector <= pCursor->lastSectorAddr)
            {
                /* case (11) - no-op */
                return TRUE;
            }

            /* case (12) - move start of run past start of cursor, update prev and cursor to next nodes, reiterate */
            startingSectorAddr++;
            dwNumSectors--;
            pPrevNode = pCursor;
            pCursor = pCursor->pNext;
        }

    } while (TRUE);

    /* should never get here */
    return TRUE;

MARK_ERROR:
    return FALSE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Private Function: SMList::RemoveRunIntersectionFromList

This function takes the a run, then removes from the specified list
any sectors in the list that are in the run. 

Returns: Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SMList::RemoveRunIntersectionFromList(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors)
{
    PSMNode pPrevNode = NULL;
    PSMNode pNewNode;
    SECTOR_ADDR runEndSector;

    runEndSector = startingPhysicalSectorAddr + dwNumSectors - 1;
    
    m_cursor = m_head;
    if (m_cursor == NULL)
    {
        return TRUE;
    }

    do {
        if (m_cursor->startSectorAddr >= startingPhysicalSectorAddr)
        {
            break;
        }
        pPrevNode = m_cursor;
        m_cursor = m_cursor->pNext;
    } while (m_cursor);
    
    do {
        /*
           cases:

           |PREVPREVPREVPREV|         |CURSORCURSORCURSOR|
               |---------|                                      (0)  starts after start of previous node's run but does not consume it completely
               |------------|                                   (1)  starts after start of previous node's run and consumes it exactly
               |--------------->                                (2)  starts after start of previous node's run, ends after end 
                                                                     >> devolves into
                            |---|                               (3)  starts at end of existing previous node and ends before start of cursor if one exists
                            |---------|                         (4)  starts at end of existing previous node and goes exactly to existing cursor
                            |----------------|                  (5)  starts at end of existing previous node and ends somewhere within existing cursor node
                            |-------------------------------|   (6)  starts at end of existing previous node and goes past end of existing cursor node
                               |----|                           (7)  starts after previous or no previous, and ends before cursor node if one exists
                               |------|                         (8)  starts after previous or no previous, and ends at existing cursor node
                               |-------------|                  (9)  starts after previous or no previous, and ends within existing cursor node
                               |----------------------------|   (10) starts after previous or no previous, and goes past end of existing cursor node
                                      |------|                  (11) starts at existing cursor node and ends within it
                                      |---------------------|   (12) starts at existing cursor node and ends after its end

           actions:

           (0)  clip previous node, create new node
           (1)  clip previous node
           (2)  clip previous node, convert to (3) to (6)
           (3)  no-op
           (4)  no-op
           (5)  convert to (11)
           (6)  convert to (12)
           (7)  no-op
           (8)  no-op
           (9)  convert to (11)
           (10) convert to (12)
           (11) clip cursor node
           (12) delete cursor node, update cursor to point to next node
        */

        /* At least ONE of m_cursor and pPrevNode will NOT be NULL */

        if (pPrevNode != NULL)
        {
            /* cases (0) -> (2) : m_cursor may be NULL */

            if (startingPhysicalSectorAddr <= pPrevNode->lastSectorAddr)
            {
                DWORD overlap;

                if (runEndSector < pPrevNode->lastSectorAddr)
                {
                    // case (0) clip previous node, create new node
                    pNewNode = (PSMNode)LocalAlloc(LPTR, sizeof(SMNode));
                    if (!pNewNode)
                    {
                        return FALSE;
                    }
                    pNewNode->startSectorAddr = runEndSector+1;
                    pNewNode->lastSectorAddr = pPrevNode->lastSectorAddr;
                    pNewNode->pNext = m_cursor;

                    pPrevNode->lastSectorAddr = startingPhysicalSectorAddr-1;
                    pPrevNode->pNext = pNewNode;

                    if (m_cursor == NULL)
                    {
                        m_tail = pNewNode;
                    }
                    m_dwNumSectors -= dwNumSectors;
                    
                    return TRUE;
                }

                /* cases (1) and (2) convert to (3) to (6) */
                overlap = pPrevNode->lastSectorAddr - startingPhysicalSectorAddr + 1;
                pPrevNode->lastSectorAddr -= overlap;
                m_dwNumSectors -= overlap;
                startingPhysicalSectorAddr += overlap;
                dwNumSectors -= overlap;

                if (!dwNumSectors)
                {
                    /* case (1) */
                    return TRUE;
                }
            }

            /* run will no longer have any portion inside previous node at this point */
            if (m_cursor == NULL)
            {
                /* no cursor node - can't overlap anything else at this point */
                return TRUE;
            }
        }

        if (m_cursor != NULL)
        {
            DWORD count;

            if (runEndSector < m_cursor->startSectorAddr)
            {
                /* case (3), (4), (7) and (8) */
                return TRUE;
            }

            if (startingPhysicalSectorAddr < m_cursor->startSectorAddr)
            {
                /* convert (5), (6), (9) and (10) to (11) or (12) */
                count = m_cursor->startSectorAddr - startingPhysicalSectorAddr;
                startingPhysicalSectorAddr = m_cursor->startSectorAddr;
                dwNumSectors -= count;
            }

            /* startingPhysicalSectorAddr == m_cursor->startSectorAddr here */

            if (runEndSector < m_cursor->lastSectorAddr)
            {
                /* case (11) - clip cursor */
                m_cursor->startSectorAddr += dwNumSectors;
                m_dwNumSectors -= dwNumSectors;
                return TRUE;
            }

            /* case 12 - delete cursor node */
            count = m_cursor->lastSectorAddr - m_cursor->startSectorAddr + 1;
            if (pPrevNode)
            {
                pPrevNode->pNext = m_cursor->pNext;
            }
            else
            {
                m_head = m_cursor->pNext;
            }
            if (!m_cursor->pNext)
            {
                m_tail = pPrevNode;
            }
            m_dwNumSectors -= count;
            startingPhysicalSectorAddr += count;
            dwNumSectors -= count;
            LocalFree(m_cursor);

            if (pPrevNode)
            {
                m_cursor = pPrevNode->pNext;
            }
            else
            {
                m_cursor = m_head;
            }

            if (m_cursor == NULL)
            {
                /* nothing left to remove after this */
                return TRUE;
            }
        }

    } while (TRUE);

    return TRUE;
}

//Constructor
SectorMgr::SectorMgr()
{
    m_bSecMgrInited = FALSE;
    m_pFal = NULL;
    m_pCompactor = NULL;
    m_pRegion = NULL;
}

//Destructor
SectorMgr::~SectorMgr()
{
    m_pFal = NULL;
    m_pCompactor = NULL;
    m_pRegion = NULL;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::Init()

Description:    Initializes the Sector Manager.

Notes:          This function should be called at driver initialization.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::Init(PFlashRegion pRegion, Fal* pFal, Compactor* pCompactor)
{
    if(m_bSecMgrInited)
    {
        return FALSE;
    }

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

    m_bSecMgrInited = TRUE;

    return TRUE;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::Deinit()

Description:    Determines if the specified physical sector is free for use.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::Deinit()
{
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    for (int i = 0; i < NUM_LIST_TYPES; i++) {
        m_lists[i].Deinit();
    }

    m_dirtyList.Deinit();

    m_bSecMgrInited = FALSE;
    
    return TRUE;
}

BOOL SectorMgr::AddSectorsToList (ListType listType, SECTOR_ADDR startingSectorAddr, DWORD dwNumSectors)
{
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    if(!SanitizeRun(&startingSectorAddr, &dwNumSectors))
    {
        return FALSE;
    }

    if(dwNumSectors == 0)
    {
        return TRUE;
    }

    return m_lists[listType].AddSectors (startingSectorAddr, dwNumSectors);
}

DWORD SectorMgr::AreSectorsInList (ListType listType, DWORD dwStartSector, DWORD dwNumSectors)
{
    if(!m_bSecMgrInited)
    {
        return SectorMgr::NotInList;
    }

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
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    SMList* pFreeList = &m_lists[Free];

    if(!SanitizeRun(&startingPhysicalSectorAddr, &dwNumSectors))
    {
        return FALSE;
    }
    if(dwNumSectors == 0)
    {
        return TRUE;
    }

    return pFreeList->RemoveRunIntersectionFromList(startingPhysicalSectorAddr, dwNumSectors);
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
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

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
    *pPhysicalSectorAddr = pFreeList->m_cursor->startSectorAddr;

    //----- 8. If this is the last physical sector address for this node, remove the node from the free sector list -----
    if(pFreeList->m_cursor->startSectorAddr == pFreeList->m_cursor->lastSectorAddr)
    {
        pFreeList->m_head = pFreeList->m_head->pNext;

        if(!pFreeList->m_head)
        {
            pFreeList->m_tail = NULL;
        }

        if(LocalFree(pFreeList->m_cursor) != NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:SectorMgr::GetNextFreeSector() - Unable to free physical sector %x node memory!\r\n"), *pPhysicalSectorAddr));
        }
        pFreeList->m_cursor = NULL;
    }
    else
    {
        pFreeList->m_head->startSectorAddr++;
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
    if(!m_bSecMgrInited)
    {
        return 0;
    }

    return m_lists[Free].m_dwNumSectors;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::IsBlockFree()

Description:    Determines whether entire block contains free sectors.

------------------------------------------------------------------------------*/
BOOL SectorMgr::IsBlockFree(BLOCK_ID blockID)
{
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    SECTOR_ADDR startSector = GetStartSectorInBlock(blockID);
    if(startSector != NULL)
    {
        return m_lists[Free].AreSectorsInList(startSector, m_pRegion->dwSectorsPerBlock) == EntirelyInList;
    }
    return FALSE;
}

//******************************************************************************//


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SectorMgr::MarkSectorsAsDirty()

Description:    Marks the specified physical sectors as being DIRTY.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::MarkSectorsAsDirty(SECTOR_ADDR startingPhysicalSectorAddr, DWORD dwNumSectors)
{
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    if(!SanitizeRun(&startingPhysicalSectorAddr, &dwNumSectors))
    {
        return FALSE;
    }

    if(!dwNumSectors)
    {
        return TRUE;
    }

    DWORD dwBlock = GetBlockFromSector (startingPhysicalSectorAddr);

    if (dwBlock < m_pRegion->dwStartPhysBlock || dwBlock >= (m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks))
    {
        RETAILMSG (1, (TEXT("SectorMgr::MarkSectorsAsDirty: Invalid Block 0x%x\r\n"), dwBlock));
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
    if(!m_bSecMgrInited)
    {
        return FALSE;
    }

    if(!SanitizeRun(&startingPhysicalSectorAddr, &dwNumSectors))
    {
        return FALSE;
    }

    if(!dwNumSectors)
    {
        return TRUE;
    }

    DWORD dwBlock = GetBlockFromSector (startingPhysicalSectorAddr);

    if (dwBlock < m_pRegion->dwStartPhysBlock || dwBlock >= (m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks))
    {
        RETAILMSG (1, (TEXT("SectorMgr::UnmarkSectorsAsDirty: Invalid Block 0x%x\r\n"), dwBlock));
        return FALSE;
    }

    if (!m_dirtyList.RemoveDirtySectors (dwBlock, dwNumSectors))
    {
        return FALSE;
    }

    ASSERT (m_dwNumDirtySectors >= dwNumSectors);

    m_dwNumDirtySectors -= dwNumSectors;
    return TRUE;
}

DWORD SectorMgr::GetDirtyCount(BLOCK_ID blockID)
{
    if(!m_bSecMgrInited)
    {
        return 0;
    }

    if (blockID >= m_pRegion->dwNumPhysBlocks)
    {
        return 0;
    }
    return m_dirtyList.GetDirtyCount (blockID);
}


//------------------------- General Purpose Public Routines --------------------------


SECTOR_ADDR SectorMgr::GetStartSectorInBlock (DWORD dwBlockID)
{
    if(!m_bSecMgrInited)
    {
        return NULL;
    }

    return m_pFal->GetStartPhysSector() + (dwBlockID - m_pRegion->dwStartPhysBlock) * m_pRegion->dwSectorsPerBlock;
}

BLOCK_ID SectorMgr::GetBlockFromSector (DWORD dwSector)
{
    if(!m_bSecMgrInited)
    {
        return -1;
    }

    return m_pRegion->dwStartPhysBlock + (dwSector - m_pFal->GetStartPhysSector()) / m_pRegion->dwSectorsPerBlock;
}


//-------------------------------- Helper Functions -------------------------------

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Local Function: SectorMgr::SanitizeRun

This function takes the parameters for a run and returns if they are at least
partially valid (can clip to end of flash region).  A 0-length run is a valid run.

Returns: Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL SectorMgr::SanitizeRun(SECTOR_ADDR *apStartSector, DWORD *apNumSectors)
{
    SECTOR_ADDR runEndSector;

    if ((!apStartSector) || (!apNumSectors))
    {
        return FALSE;
    }

    if ((*apNumSectors)==0)
        return TRUE;    /* 0-length run is not a failure */

    runEndSector = (*apStartSector) + (*apNumSectors) - 1;

    if (runEndSector < (*apStartSector))
    {
        return FALSE;    /* wrap around */
    }

    if ((*apStartSector) >= (m_pRegion->dwNumPhysBlocks * m_pRegion->dwSectorsPerBlock))
    {
        return FALSE;    /* start is off end of valid range */
    }

    if (runEndSector >= (m_pRegion->dwNumPhysBlocks * m_pRegion->dwSectorsPerBlock))
    {
        /* clip to end */
        runEndSector = (m_pRegion->dwNumPhysBlocks * m_pRegion->dwSectorsPerBlock) - 1;
        (*apNumSectors) = runEndSector - (*apStartSector) + 1;
    }

    return TRUE;
}