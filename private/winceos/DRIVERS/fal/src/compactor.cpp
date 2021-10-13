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

Module Name:    COMPACTOR.C

Abstract:       Compactor module

Notes:          The following module is responsible for compacting the FLASH media blocks.
                A compaction operation works as follows (using psuedo-code):
                 
                   foreach(physicalSector in Block b)
                   {
                        if(IsSectorMapped(physicalSector))
                        {
                            ReadSectorData(physicalSector, buffer);
                            newPhysicalSectorAddr = SM_GetNextFreeSector();
                            WriteSectorData(newPhysicalSector, buffer);
                            UpdateLogicalToPhysicalMapping(logicalSectorAddr, newPhysicalSectorAddr);   
                            MarkSectorDirty(physicalSector);
                        }

                        if(IsSectorDirty(physicalSector)
                        {
                            dwNumDirtySectorsFound++;
                        }
                    }
                    EraseBlock(Block b);
                    AddFreeSectorsToSectorManager(Block b);

                That is, to compact a block we just need to move any good data to another physical location 
                on the media and simply ignore any DIRTY physical sectors.  Once the entire block has been 
                processed, it can then safely be erased.  Notice also that all of the good data is still 
                retained after the ERASE operation and that data CANNOT be lost during a power-failure situation 
                because the old data is marked DIRTY after the data has been safely moved to another physical 
                sector address.   

                There are several important points to consider in this algorithm:

                    1)  The compactor uses the *exact* same code to move good data to another portion of the 
                        FLASH media as the FAL uses to write new data from the file system.  Practically, this 
                        means that the compactor queries the Sector Manager for the next FREE physical sector 
                        when it needs to perform a WRITE operation.

                    2)  The algorithm takes the necessary precautions to avoid *any* data loss during a 
                        power-failure condition.  In the worst case scenario, the FLASH media will end up with 
                        two separate physical copies of the SAME data, which will be cleaned up by the FAL during 
                        the next boot sequence.

                    3)  The algorithm is simple.  Notice that there are NO special situations or end cases to consider.  
                        The Compactor simply moves good data to another potion of the disk (a.k.a. compacting the data) 
                        and then erases the block.
                        
                The Compactor *always* processes FLASH blocks consecutively.  Once the Compactor reaches the last 
                FLASH block on the media, it simply moves back to the first FLASH block and continues as necessary.

-----------------------------------------------------------------------------*/

#include "compactor.h"


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::InitCompactor()

Description:    Initializes the compactor. 

Notes:          The compactor is not started by default.  The caller must invoke
                Compactor::StartCompactor() to start the compactor thread.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL Compactor::Init(PFlashRegion pRegion, FileSysFal* pFal)
{
    m_pRegion = pRegion;
    m_pFal = pFal;
    m_pSectorMgr = pFal->m_pSectorMgr;
    m_pMap = pFal->m_pMap;
    m_compactingBlockID = pRegion->dwStartPhysBlock;
    m_bEndCompactionThread = FALSE;

    if (pRegion->dwCompactBlocks < MINIMUM_FLASH_BLOCKS_TO_RESERVE)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::InitCompactor() - Invalid number of compaction blocks in region.\r\n")));
        goto INIT_ERROR;
    }
    
    //----- 1. Create the event that controls the compactor -----
    if((m_hStartCompacting = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::InitCompactor() - Unable to create compactor control event object!!!\r\n")));
        goto INIT_ERROR;
    }

    //----- 2. Create the event that signals when compacting is complete -----
    if((m_hCompactionComplete = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::InitCompactor() - Unable to create compactor completion event object!!!\r\n")));
        goto INIT_ERROR;
    }


    //----- 4. Allocate a buffer to use for moving MAPPED data during a compaction -----
    if((m_pSectorData = (PBYTE)LocalAlloc(LMEM_FIXED, g_pFlashMediaInfo->dwDataBytesPerSector)) == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::InitCompactor() - Unable to allocate memory for sector data.\r\n")));
        goto INIT_ERROR;
    }

    //----- 5. Initialize the "nested compactions" counter.  This counter is used to prevent an infinite -----
    //         recursion situation that can occur if WRITE operations keep failing within Compactor::CompactBlock()
    m_dwNestedCompactions = 0;

    //----- 6. Retrieve the ID for the block that we need to process on the next compaction -----
    //         NOTE: Currently, the FAL computes the block ID during system initialization and
    //               calls Compactor::SetCompactingBlock() appropriately.  Hence, we don't need to explicitly
    //               store/retrieve this state information on the FLASH media.


    //----- 7. Create the compactor thread -----
    if((m_hCompactorThread = CreateThread(NULL, 0, ::CompactorThread, (LPVOID)this, 0, &m_dwThreadID)) == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::InitCompactor() - Unable to create compactor thread!!!\r\n")));
        goto INIT_ERROR;
    }

    return TRUE;

INIT_ERROR:
    CloseHandle(m_hStartCompacting);
    CloseHandle(m_hCompactionComplete);
    CloseHandle(m_hCompactorThread);
    return FALSE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::DeinitCompactor()

Description:    Deinitializes the compactor. 

Notes:          The compactor is first stopped.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL Compactor::Deinit()
{
    // By entering the device critical section, we are ensuring the current compaction 
    // operation is complete
    EnterCriticalSection(&g_csFlashDevice);
    
    m_dwFreeSectorsNeeded = 0;
    m_bEndCompactionThread = TRUE;
    SetEvent (m_hStartCompacting);
    
    LeaveCriticalSection(&g_csFlashDevice);

    WaitForSingleObject(m_hCompactorThread, 10000);
    
    //----- 2. Free all the compactor thread's system resources -----
    if(LocalFree(m_pSectorData) != NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::Deinit() - Unable to free buffer used for moving MAPPED data\r\n")));
    }
    
    CloseHandle(m_hCompactionComplete);
    CloseHandle(m_hStartCompacting);
    CloseHandle(m_hCompactorThread);
    return TRUE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::StartCompactor()

Description:    Starts the compactor using the specified thread priority.  The
                compactor thread then attempts to find the the specified number 
                of free sectors.  If bCritical == TRUE, this function doesn't
                return until the compaction stage completes.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL Compactor::StartCompactor(DWORD dwPriority, DWORD dwFreeSectorsNeeded, BOOL bCritical)
{

    //----- 1. Are there enough DIRTY sectors on the media to fulfill this request? -----
    if(dwFreeSectorsNeeded > m_pSectorMgr->GetNumDirtySectors())
    {
        if(m_pSectorMgr->GetNumDirtySectors() == 0)
        {
            ReportError((TEXT("FLASHDRV.DLL:Compactor::StartCompactor() - There aren't any DIRTY sectors left; the compactor can't be started\r\n")));
            goto START_ERROR;
        }else{
            // Set this compaction operation to retrieve the rest of the DIRTY sectors...
            dwFreeSectorsNeeded = m_pSectorMgr->GetNumDirtySectors();
        }
    }

    //----- 2. Set the number of DIRTY sectors to recycle into FREE sectors -----
    m_dwFreeSectorsNeeded = dwFreeSectorsNeeded;

    m_bCritical = bCritical;

    //----- 3. If this is a critical compaction, make sure the "compaction completed" wait event is reset -----
    if(bCritical == TRUE)
    {
        if(!ResetEvent(m_hCompactionComplete))
        {
            ReportError((TEXT("FLASHDRV.DLL:Compactor::StartCompactor() - Unable to reset the critical Compaction Complete wait event!!!\r\n")));
            goto START_ERROR;
        }
    }

    //----- 4. Signal the compactor to start retrieving the requested number of sectors -----
    if(!SetEvent(m_hStartCompacting))
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::StartCompactor() - Unable to signal compactor thread!!!\r\n")));
        goto START_ERROR;
    }

    //----- 5. Set the thread priority accordingly -----
    if(!CeSetThreadPriority(m_hCompactorThread, dwPriority))
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::StartCompactor() - Unable to set compactor's thread priority!!!\r\n")));
        goto START_ERROR;
    }

    //----- 6. SPECIAL CASE: If this is a CRITICAL compaction request, we must wait for the compaction process -----
    //                       to complete before returning (this compaction is then SYNCHRONOUSLY executed for the caller).
    if(bCritical == TRUE)
    {
        LeaveCriticalSection( &g_csFlashDevice);

        if(WaitForSingleObject(m_hCompactionComplete, INFINITE) == WAIT_FAILED)
        {
            ReportError((TEXT("FLASHDRV.DLL:CompactorThread() - Unable to wait on m_hCompactionComplete event!\r\n")));
            EnterCriticalSection( &g_csFlashDevice);
            goto START_ERROR;
        }
        EnterCriticalSection( &g_csFlashDevice);
    }
    return TRUE;


START_ERROR:
    return FALSE;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::GetCompactingBlock()

Description:    Retrieves the ID for the block currently being compacted.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL Compactor::GetCompactingBlock(PBLOCK_ID pBlockID)
{
    //----- 1. Check the parameters to insure they are legitimate -----
    if(pBlockID == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:Compactor::GetCompactingBlockID() - pBlockID == NULL !!!\r\n")));
        return FALSE;
    }

    //----- 2. Return the current block being compacted -----
    *pBlockID = m_compactingBlockID;

    return TRUE;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::SetCompactingBlock()

Description:    Specifies which block to use for the next compaction process.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL Compactor::SetCompactingBlock(BLOCK_ID blockID)
{
    m_compactingBlockID = blockID;
    return TRUE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       CompactorThread()

Description:    Compactor thread that does the actual work necessary for coalescing
                the dirty, mapped, and free sectors on the media.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
DWORD Compactor::CompactorThread()
{
    DWORD           dwNumDirtySectorsFound  = 0;
    DWORD           Result = FALSE;

    for(;;)
    {
        //----- 1. Wait for the signal to start compaction process -----
        if(WaitForSingleObject(m_hStartCompacting, INFINITE) == WAIT_FAILED)
        {
            DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL:CompactorThread() - Unable to wait on m_hStartCompacting event.  Exiting compactor.\r\n")));
            goto COMPACTOR_EXIT;
        }

        if (m_bEndCompactionThread)
        {
            DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL:CompactorThread() - Exiting compactor.\r\n")));
            Result = TRUE;
            goto COMPACTOR_EXIT;
        }            

        //----- For debugging purposes, print out the statistics for this compaction request
#ifdef DEBUG
        EnterCriticalSection(&g_csFlashDevice);
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL:------------ Starting Compaction... ------------\r\n")));
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("              Looking to recycle %d DIRTY sectors into FREE sectors\r\n"), m_dwFreeSectorsNeeded));
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("              >>> Media State Information: >>>\r\n")));
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("                 Free  Sectors = %d\r\n"), m_pSectorMgr->GetNumberOfFreeSectors() )); 
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("                 Dirty Sectors = %d\r\n"), m_pSectorMgr->GetNumDirtySectors() )); 
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL:------------------------------------------------\r\n")));
        LeaveCriticalSection(&g_csFlashDevice);
#endif

        for(;;)
        {
            DWORD dwBlkCount = 0;
        
            EnterCriticalSection(&g_csFlashDevice);

            //----- 2. Are we done with this compaction process? Compaction is over if the number of -----
            //         requested free sectors have been reclaimed or there are no more DIRTY sectors
            //         left to recycle.
            if((m_dwFreeSectorsNeeded == 0) || (m_pSectorMgr->GetNumDirtySectors() == 0) )
            {
                LeaveCriticalSection( &g_csFlashDevice);
                break;
            }

            //----- 3. Compact the next block... -----
            m_compactingBlockID = GetNextCompactionBlock(m_compactingBlockID);

            if ((dwBlkCount >= m_pRegion->dwNumPhysBlocks) || (m_compactingBlockID == INVALID_BLOCK_ID))
            {
                RETAILMSG(1, (TEXT("FLASHDRV.DLL:CompactorThread() - CompactorThread(0x%x) failed; unable to compact!!!\r\n"), m_compactingBlockID));
                LeaveCriticalSection( &g_csFlashDevice);
                goto COMPACTOR_EXIT;
            }                

            //----- For debugging purposes, print out the compaction block information... -----
            DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL:CompactorThread() - Compacting BLOCK %d\r\n"), m_compactingBlockID));

            //----- 5. Compact the current block... -----
            if((dwNumDirtySectorsFound = CompactBlock(m_compactingBlockID, USE_SECTOR_MAPPING_INFO)) == BLOCK_COMPACTION_ERROR)
            {
                ReportError((TEXT("FLASHDRV.DLL:CompactorThread() - Compactor::CompactBlock(%d) failed; unable to compact!!!\r\n"), m_compactingBlockID));
                LeaveCriticalSection( &g_csFlashDevice);
                break;
            }

            //----- 7. Update the number of DIRTY sectors recycled into FREE sectors while compacting this block -----
            if(m_dwFreeSectorsNeeded < dwNumDirtySectorsFound)
            {
                m_dwFreeSectorsNeeded = 0;                              // Done compacting...
            }else
            {
                m_dwFreeSectorsNeeded -= dwNumDirtySectorsFound;        // Subtract # of recycled DIRTY sectors and keep going...
            }

            LeaveCriticalSection( &g_csFlashDevice);
        }
    
        //----- For debugging purposes, print out the compaction block information... -----
        DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL   >>> Compaction process complete: %d DIRTY sectors recycled \r\n"), dwNumDirtySectorsFound));     


        //----- 9. Signal that the compaction operation is complete... -----
        if(!SetEvent(m_hCompactionComplete))
        {
            ReportError((TEXT("FLASHDRV.DLL:CompactorThread() - Unable to signal compaction is complete!!!\r\n")));
            goto COMPACTOR_EXIT;
        }       

    }

COMPACTOR_EXIT:
    return Result;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::CompactBlock()

Description:    Compacts the specified block.  Data in MAPPED sectors are relocated 
                to another part of the media (using the Sector Manager) and DIRTY
                sectors are recycled into FREE sectors.

Notes:          The treatAsDirtySectorAddr parameter is used to indicate a single
                sector in the block that should be treated as DIRTY regardless of
                what the mapping information states.  Practically, this situation
                only occurs when we are trying to re-map data to a new sector on the
                media and we are NOT able to mark the old data as DIRTY.  In this case,
                we need to compact this block and treat the data in this sector as 
                DIRTY.  Once compaction is complete, the old data will be deleted
                from the media and recycled into a FREE sector.

                Setting treatAsDirtySectorAddr == USE_SECTOR_MAPPING_INFO indicates that
                the sector mapping information should be used and no sectors need to
                explicitly be treated as DIRTY.

Returns:        Number of DIRTY sectors recycled into FREE sectors.
------------------------------------------------------------------------------*/
DWORD Compactor::CompactBlock(BLOCK_ID compactionBlockID, SECTOR_ADDR treatAsDirtySectorAddr)
{
    SECTOR_ADDR         newPhysicalSectorAddr   = 0;
    SECTOR_ADDR         physicalSectorAddr      = 0;

    DWORD               dwNumDirtySectorsFound  = 0;
    DWORD               i                       = 0;
    SectorMappingInfo sectorMappingInfo;    

    // Record compaction in CELOG
    CELOG_CompactBlock(m_bCritical, compactionBlockID);

    //----- 1. Determine starting physical sector address for the specified block being compacted -----
    //         NOTE: If # of sectors/block is a power of 2, we can avoid a potentially expensive multiplication 
    //         (Refer to Compactor::InitCompactor() for details...)
    physicalSectorAddr = m_pSectorMgr->GetStartSectorInBlock (compactionBlockID);

    // Unmark any sectors in this block that are free, since all of the sectors will be marked
    // free at the end of this operation
    m_pSectorMgr->UnmarkSectorsAsFree(physicalSectorAddr, m_pRegion->dwSectorsPerBlock);

    //----- 2. Compact the specified block.  If any data is still MAPPED, move it... -----
    dwNumDirtySectorsFound = 0;
    for(i=0; i<m_pRegion->dwSectorsPerBlock; i++, physicalSectorAddr++)
    {
        //----- 3. Read the sector mapping information for the current physical sector -----
        if(!FMD.pReadSector(physicalSectorAddr, NULL, (PSectorInfo)&sectorMappingInfo, 1))
        {
            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to read sector mapping info for physical sector 0x%08x\r\n"), physicalSectorAddr));
            goto COMPACTION_ERROR;
        }

        //----- 4. DIRTY sector?  Unmark this sector as DIRTY (using the Sector Manager) because -----
        //         we are going to recycle it into a FREE sector through an erase operation.
        if( IsSectorDirty(sectorMappingInfo) || 
           ((treatAsDirtySectorAddr != USE_SECTOR_MAPPING_INFO) && (treatAsDirtySectorAddr == physicalSectorAddr))
          )
        {
            //----- For debugging purposes, print out the state of this sector -----
            DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL     >>> Physical sector 0x%08x is DIRTY\r\n"), physicalSectorAddr));       

            if(!m_pSectorMgr->UnmarkSectorsAsDirty(physicalSectorAddr, 1))
            {
                ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to unmark physical sector 0x%08x as DIRTY\r\n"), physicalSectorAddr));
            }
            dwNumDirtySectorsFound++;
            continue;
        }

        //----- 5. Data is MAPPED.  Move it to another free sector on the FLASH media -----
        if(IsSectorMapped(sectorMappingInfo))
        {
            //----- For debugging purposes, print out the state of this sector -----
            DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL     >>> Physical sector 0x%08x is MAPPED\r\n"), physicalSectorAddr));      

            // Should check if LSA is valid or not before copying the sector
            // to avoid that all blocks have this invalid sector by the Compactor.
            if(!m_pMap->IsValidLogicalSector(sectorMappingInfo.logicalSectorAddr))
            {
                ReportError((TEXT("FLASHDRV.DLL     >>> Logical sector(=0x%08x) of physical sector(=0x%08x) is incorrect.\r\n"), sectorMappingInfo.logicalSectorAddr, physicalSectorAddr));
                goto COMPACTION_ERROR;
            }

            //----- 6. Read the data into a temporary buffer -----
            if(!FMD.pReadSector(physicalSectorAddr, m_pSectorData, NULL, 1))
            {
                ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to read sector data for physical sector 0x%08x\r\n"), physicalSectorAddr));
                goto COMPACTION_ERROR;
            }

            //----- Write the sector data to the media.  Notice that this operation is repetitively -----
            //      tried ONLY if FMD.pWriteSector() fails.  Unless the FLASH block we are writing to
            //      has just gone bad, this loop will only execute once.
            for(;;)
            {
                //----- 7. Get a free physical sector to store the data into -----
                //         NOTE: Notice that the second parameter is TRUE; this forces the Sector Manager
                //               to give us the next FREE sector regardless of how many are left.  This is
                //               important so that this call to the Sector Manager doesn't trigger another
                //               compaction.
                if(!m_pSectorMgr->GetNextFreeSector(&newPhysicalSectorAddr, TRUE))
                {
                    ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to get next free physical sector address for writing!  The media is FULL\r\n")));
                    goto COMPACTION_ERROR;
                }

                //----- For debugging purposes, print out where the MAPPED data is being moved... -----
                DEBUGMSG(ZONE_COMPACTOR, (TEXT("FLASHDRV.DLL     >>> Moving data from physical sector 0x%08x to physical sector 0x%08x\r\n"), 
                                                physicalSectorAddr, newPhysicalSectorAddr));        


                //----- 8. Start the write operation (used to safeguard against power failure conditions) -----
                //         The rationale is as follows: If we lose power during before the data is written
                //         to the media, this bit should be set and this will allow us to detect that a 
                //         WRITE was in progress when the power failed.
                MarkSectorFree(sectorMappingInfo);
                MarkSectorWriteInProgress(sectorMappingInfo);
    
                if(!FMD.pWriteSector(newPhysicalSectorAddr, NULL, (PSectorInfo)&sectorMappingInfo, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to start write operation.  FMD_WriteSector() failed.\r\n")));
                                                
                    // WRITE operation failed, try to recover only if it is within the maximum number of nested compactions...
                    if(++m_dwNestedCompactions < MAX_NUM_NESTED_COMPACTIONS)
                    {
                        if(!m_pFal->HandleWriteFailure(newPhysicalSectorAddr))
                        {
                            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                            goto COMPACTION_ERROR;
                        }
                        m_dwNestedCompactions--;
                    }
                    continue;   
                }


                //----- 9. Invoke the FLASH Media Driver (FMD) to write the sector data to the media -----
                //         NOTE: The write will be marked as "completed" if FMD_WriteSector() succeeeds
                //
                MarkSectorWriteCompleted(sectorMappingInfo);
                if(!FMD.pWriteSector(newPhysicalSectorAddr, m_pSectorData, (PSectorInfo)&sectorMappingInfo, 1))  
                {   
                    ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to write to physical sector 0x%08x!\r\n"), newPhysicalSectorAddr));
                        
                    // WRITE operation failed, try to recover only if it is within the maximum number of nested compactions...
                    if(++m_dwNestedCompactions < MAX_NUM_NESTED_COMPACTIONS)
                    {
                        if(!m_pFal->HandleWriteFailure(newPhysicalSectorAddr))
                        {
                            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                            goto COMPACTION_ERROR;
                        }
                        m_dwNestedCompactions--;
                    }
                    continue;
                }

                //----- 10. Perform the LOGICAL -> PHYSICAL mapping -----
                if(!m_pMap->MapLogicalSector(sectorMappingInfo.logicalSectorAddr, newPhysicalSectorAddr, &physicalSectorAddr))
                {
                    ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"), 
                                       sectorMappingInfo.logicalSectorAddr, newPhysicalSectorAddr));
                    goto COMPACTION_ERROR;
                }

                //----- 11. Mark the "stale data" in the existing physical sector as DIRTY -----
                //          NOTE: This is done in case of a power failure before the whole
                //                block is compacted.
                        
                //NOTE: Since we don't know the contents of the sector mapping info for this physical sector,
                //      we can safely write all '1's (except the bit used to indicate the sector is "dirty").
                memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
                MarkSectorDirty(sectorMappingInfo);                   

                if(!FMD.pWriteSector(physicalSectorAddr, NULL, (PSectorInfo)&sectorMappingInfo, 1))
                {   
                    ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to mark old physical sector 0x%08x as DIRTY!\r\n"), physicalSectorAddr));
                    
                    // WRITE operation failed, try to recover only if it is within the maximum number of nested compactions...
                    if(++m_dwNestedCompactions < MAX_NUM_NESTED_COMPACTIONS)
                    {
                        if(!m_pFal->HandleWriteFailure(physicalSectorAddr))
                        {
                            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                            goto COMPACTION_ERROR;
                        }
                        m_dwNestedCompactions--;
                    }
                    continue;
                }

                //----- 12. At this point, the WRITE operation has completed successfully. -----
                break;
            }
        }

    }   

    //----- 13. Compaction on this block is complete.  Erase the block and recycle the DIRTY sectors -----
    //          into FREE sectors.
    if(!FMD.pEraseBlock(compactionBlockID))
    {       
        ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to erase block %d!!!  These sectors can't be reused...\r\n"), compactionBlockID));
        
        if(!IsBlockBad(compactionBlockID))
        {
            if(!FMD.pSetBlockStatus(compactionBlockID, BLOCK_STATUS_BAD))
            {
                ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to mark block %d as BAD!!!\r\n"), compactionBlockID));
            }
        }

        if(!m_pSectorMgr->MarkBlockUnusable(compactionBlockID))
        {
            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - m_pSectorMgr->MarkBlockUnusable(%d) failed.\r\n"), compactionBlockID));
        }

    }else
    {
        //----- 14. Inform the Sector Manager that these sectors are now FREE. -----
        //          NOTE: Notice that the starting address for the FREE sectors is
        //                (physicalSectorAddr - m_pFlashMediaInfo->wSectorsPerBlock)
        //                This is to account for the fact that physicalSectorAddr
        //                was incremented (above) and points to the next block!
        if(!m_pSectorMgr->AddSectorsToList (SectorMgr::Free, (physicalSectorAddr - m_pRegion->dwSectorsPerBlock), m_pRegion->dwSectorsPerBlock))
        {
            ReportError((TEXT("FLASHDRV.DLL:Compactor::CompactBlock() - Unable to mark physical sectors 0x%08x - 0x%08x as free...\r\n"), 
                               physicalSectorAddr, (physicalSectorAddr + m_pRegion->dwSectorsPerBlock) ));
        }
    }

    return dwNumDirtySectorsFound;

COMPACTION_ERROR:
    return 0;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::GetNextCompactionBlock()

Description:    Gets the next FLASH block on the media to compact based upon
                the specified "current" compaction block.

Returns:        ID for the next block to compact.
------------------------------------------------------------------------------*/
BLOCK_ID Compactor::GetNextCompactionBlock(BLOCK_ID blockID)
{
    BLOCK_ID nextBlockID;
    static BOOL bRandomBlock = FALSE;
    
    if (m_bCritical) 
    {
        nextBlockID = GetDirtiestBlock(blockID);
    }    
    else
    {        
        // Idle compaction will alternate between compacting the dirtiest block
        // and compacting a random block to provide even wear leveling
        
        if (bRandomBlock)
        {
            nextBlockID = GetNextRandomBlock(blockID);

            // Don't bother to compact a block that is completely free
            if ((nextBlockID == INVALID_BLOCK_ID) || m_pSectorMgr->IsBlockFree (blockID))
            {
                nextBlockID = GetDirtiestBlock(blockID);
            }
        }
        else
        {
            nextBlockID = GetDirtiestBlock(blockID);
        }

        // Toggle the random block flag
        bRandomBlock = !bRandomBlock;        
    }
    
    return nextBlockID;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::GetDirtiestBlock()

Description:    Gets the next FLASH block on the media to compact based upon
                the specified "current" compaction block.

Returns:        ID for the next block to compact.
------------------------------------------------------------------------------*/
BLOCK_ID Compactor::GetDirtiestBlock(BLOCK_ID blockID)
{
    DWORD dwEndBlock = m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks - 1;
    
    DWORD iBlock = (blockID + 1) <= dwEndBlock ? blockID + 1: m_pRegion->dwStartPhysBlock;
    DWORD dwBestBlock = iBlock;
    DWORD dwMaxDirtySectors = 0;
    
    while (iBlock != blockID)
    {
        DWORD dwDirtyCount = m_pSectorMgr->GetDirtyCount (iBlock);
        
        if (dwDirtyCount > dwMaxDirtySectors)
        {
            dwBestBlock = iBlock;
            dwMaxDirtySectors = dwDirtyCount;
        }

        iBlock++;
        if (iBlock > dwEndBlock)
        {
            iBlock = m_pRegion->dwStartPhysBlock;        
        }
    }

    if (!IsBlockWriteable(dwBestBlock))
    {
        RETAILMSG(1, (TEXT("FLASHDRV.DLL:Compactor::GetNextCompactionBlock() Error: Cannot write to block %x!\r\n"), dwBestBlock));
        ASSERT (0);
        return INVALID_BLOCK_ID;
    }    

    return dwBestBlock;
}




/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       Compactor::GetNextRandomBlock()

Description:    Gets the next FLASH block on the media to compact based upon
                the specified "current" compaction block.

Returns:        ID for the next block to compact.
------------------------------------------------------------------------------*/
BLOCK_ID Compactor::GetNextRandomBlock(BLOCK_ID blockID)
{
    DWORD dwNextBlock = INVALID_BLOCK_ID;
    DWORD dwLoopCount = 0;
        
    do
    {           
        dwNextBlock = m_pRegion->dwStartPhysBlock + (Random() % m_pRegion->dwNumPhysBlocks);

        if (++dwLoopCount > m_pRegion->dwNumPhysBlocks)
        {
            return INVALID_BLOCK_ID;
        }
        
    } while (!IsBlockWriteable(dwNextBlock));
  
    return dwNextBlock;
}

DWORD WINAPI CompactorThread(LPVOID lpParameter)
{
    Compactor* compactor = (Compactor*)lpParameter;
    return compactor->CompactorThread();
}

