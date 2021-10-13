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
Module Name:    FAL.CPP

Abstract:       FLASH Abstraction Layer (FAL) for Windows CE

-----------------------------------------------------------------------------*/
#include <windows.h>
#include <windev.h>
#include "storemgr.h"

#include "fal.h"

Fal::Fal (DWORD dwStartLogSector, DWORD dwStartPhysSector, BOOL fReadOnly) :
    m_dwStartLogSector(dwStartLogSector),
    m_dwStartPhysSector(dwStartPhysSector),
    m_dwNumLogSectors(0),
    m_fReadOnly(fReadOnly)
{
    m_pMap = new MappingTable(dwStartLogSector, dwStartPhysSector);
    m_pSectorMgr = new SectorMgr();
}

Fal::~Fal()
{
    delete m_pMap;
    delete m_pSectorMgr;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       FormatMedia()

Description:    Formats the FLASH by erasing all of the physical blocks
                on the media.

Notes:          The logical --> physical mapper and sector manager
                are automatically re-initialized.

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL Fal::FormatRegion(VOID)
{
    DWORD i = 0;

    if (m_fReadOnly)
    {
        SetLastError(ERROR_WRITE_PROTECT);
        return FALSE;
    }

    //----- 3. Erase all of the valid blocks on the FLASH media -----
    //         NOTE: The file system is responsible for then
    //               "logically" initializing the media as needed
    for(i = m_pRegion->dwStartPhysBlock; i < m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks; i++)
    {
        if(FMD.pGetBlockStatus((BLOCK_ID)i) & (BLOCK_STATUS_BAD | BLOCK_STATUS_RESERVED))
        {
            // Don't erase bad blocks on the media or blocks reserved for other use (such as bootloader).
            continue;
        }

        if(!FMD.pEraseBlock((BLOCK_ID)i))
        {
            ReportError((TEXT("FLASHDRV.DLL:FormatMedia() - FMD_EraseBlock(%d) failed\r\n"), i));

            if(!FMD.pSetBlockStatus((BLOCK_ID)i, BLOCK_STATUS_BAD))
            {
                ReportError((TEXT("FLASHDRV.FormatMedia() - FMD_MarkBlockBad(%d) failed\r\n"), i));
            }
        }
    }
    return TRUE;
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       InternalReadFromMedia()

Description:    Performs the specified scatter/gather READ request from the media.

Notes:          After parsing the parameters, the actual READ request is handled
                by the FLASH Media Driver (FMD).

Returns:        Boolean indicating success.
-------------------------------------------------------------------------------*/
BOOL Fal::ReadFromMedia(PSG_REQ pSG_req, BOOL fDoMap)
{
    PSG_BUF      pSGbuff             = NULL;
    PUCHAR       pBuff               = NULL;

    DWORD        dwSGBuffNum         = 0;
    DWORD        dwSGBuffPos         = 0;
    DWORD        dwSectorAddr        = pSG_req->sr_start;

    SECTOR_ADDR  physicalSectorAddr  = 0;

    CELOG_ReadSectors(pSG_req->sr_start, pSG_req->sr_num_sec);

    //----- 1. Parse the scatter/gather request to determine if it can be fulfilled -----
    pSG_req->sr_status = ERROR_IO_PENDING;

    //----- 2. Now service the sector READ request(s) -----
    //         NOTE: The FLASH Media Driver (FMD) is invoked to read the data off the media
    for(dwSGBuffNum=0; dwSGBuffNum<pSG_req->sr_num_sg; dwSGBuffNum++)
    {
        dwSGBuffPos  = 0;

        // Get a pointer to the scatter/gather buffer
        pSGbuff = &(pSG_req->sr_sglist[dwSGBuffNum]);

        if (fDoMap)
        {
            // Open the sg buffer; performs caller access check.
            HRESULT hr = CeOpenCallerBuffer (
                            (PVOID*)&pBuff,
                            pSGbuff->sb_buf,
                            pSGbuff->sb_len,
                            ARG_O_PTR,
                            FALSE );

            if (FAILED (hr))
            {
                ReportError((TEXT("FLASHDRV.DLL:ReadFromMedia() - CeOpenCallerBuffer() failed, couldn't obtain pointer to buffer.\r\n"), 0));
                switch(hr)
                {
                case E_ACCESSDENIED:
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;

                case E_INVALIDARG:
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;

                case E_OUTOFMEMORY:
                    pSG_req->sr_status = ERROR_OUTOFMEMORY;
                    break;

                default:
                    // This return value isn't documented in MSDN.
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;
                }
                goto READ_ERROR;
            }

        }
        else
        {
            pBuff = pSGbuff->sb_buf;
        }

        while (dwSectorAddr < (pSG_req->sr_start + pSG_req->sr_num_sec) && 
              ((pSGbuff->sb_len - dwSGBuffPos) >= g_pFlashMediaInfo->dwDataBytesPerSector))
        {
            //----- 3. Perform the LOGICAL -> PHYSICAL mapping to determine the physical sector address. -----
            if((!m_pMap->GetPhysicalSectorAddr((SECTOR_ADDR)dwSectorAddr, &physicalSectorAddr)) || (physicalSectorAddr == UNMAPPED_LOGICAL_SECTOR))
            {
                DEBUGMSG(ZONE_READ_OPS, (TEXT("FLASHDRV.DLL:ReadFromMedia() - Unable to determine physical sector address for logical sector 0x%08x\r\n"), dwSectorAddr));

                // The file system has just asked us to read a sector that it has NEVER written to;  
                // in this situation, we don't read any data from the media and just zero the buffer.
                ZeroMemory (pBuff+dwSGBuffPos, g_pFlashMediaInfo->dwDataBytesPerSector);
            }
            else
            {
                //----- For debugging purposes, print out the logical --> physical sector mapping... -----
                DEBUGMSG(ZONE_READ_OPS, (TEXT("FLASHDRV.DLL:ReadFromMedia() - logicalSector 0x%08x --> physicalSector 0x%08x\r\n"), dwSectorAddr, physicalSectorAddr));

                //----- 4. Invoke the FLASH Media Driver (FMD) to read the sector data from the media -----
                //         NOTE: Currently, only one sector is read at a time (last parameter)
                if(!FMD.pReadSector(physicalSectorAddr, (pBuff+dwSGBuffPos), NULL, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:FMD_ReadSector() failed!\r\n")));

                    if (fDoMap)
                    {
                        // Close the sg buffer.
                        VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                            pBuff,
                            pSGbuff->sb_buf,
                            pSGbuff->sb_len,
                            ARG_O_PTR )));
                    }

                    pSG_req->sr_status = ERROR_READ_FAULT;
                    goto READ_ERROR;
                }
            }

            dwSGBuffPos  += g_pFlashMediaInfo->dwDataBytesPerSector;
            dwSectorAddr++;
        }

        if (fDoMap)
        {
            // Close the sg buffer.
            VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                pBuff,
                pSGbuff->sb_buf,
                pSGbuff->sb_len,
                ARG_O_PTR )));
        }

        pBuff = NULL;
    }

    pSG_req->sr_status = ERROR_SUCCESS;
    return TRUE;

READ_ERROR:
    return FALSE;
}

BOOL Fal::WriteToMedia(PSG_REQ pSG_req, BOOL fDoMap)
{
    PSG_BUF      pSGbuff                     = NULL;
    PUCHAR       pBuff                       = NULL;

    DWORD        dwSGBuffNum                  = 0;
    DWORD        dwSGBuffPos                  = 0;

    CELOG_WriteSectors(pSG_req->sr_start, pSG_req->sr_num_sec);

    DWORD dwCurSector = pSG_req->sr_start;

    if (m_fReadOnly)
    {
        pSG_req->sr_status = ERROR_WRITE_PROTECT;
        return FALSE;
    }

    //----- 1. Service the sector WRITE request(s) -----
    //         NOTE: The FLASH Media Driver (FMD) is invoked to write the data to the media
    for(dwSGBuffNum=0;
        dwSGBuffNum<pSG_req->sr_num_sg;
        dwSGBuffNum++)
    {
        dwSGBuffPos  = 0;

        // Get a pointer to the scatter/gather buffer
        pSGbuff = &(pSG_req->sr_sglist[dwSGBuffNum]);

        if (fDoMap)
        {
            // Open the sg buffer; performs caller access check.
            HRESULT hr = CeOpenCallerBuffer (
                            (PVOID*)&pBuff,
                            pSGbuff->sb_buf,
                            pSGbuff->sb_len,
                            ARG_I_PTR,
                            FALSE );
            if (FAILED (hr))
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - CeOpenCallerBuffer() failed, couldn't obtain pointer to buffer.\r\n"), 0));
                switch(hr)
                {
                case E_ACCESSDENIED:
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;

                case E_INVALIDARG:
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;

                case E_OUTOFMEMORY:
                    pSG_req->sr_status = ERROR_OUTOFMEMORY;
                    break;

                default:
                    // This return value isn't documented in MSDN.
                    pSG_req->sr_status = ERROR_INVALID_PARAMETER;
                    break;
                }
                return FALSE;
            }

        }
        else
        {
            pBuff = pSGbuff->sb_buf;
        }

        DWORD dwNumSectors = pSGbuff->sb_len / g_pFlashMediaInfo->dwDataBytesPerSector;
        DWORD dwError = InternalWriteToMedia (dwCurSector, dwNumSectors, pBuff);

        if (fDoMap)
        {
            // Close the sg buffer.
            VERIFY (SUCCEEDED (CeCloseCallerBuffer (
                pBuff,
                pSGbuff->sb_buf,
                pSGbuff->sb_len,
                ARG_I_PTR )));
        }

        pBuff = NULL;

        if (dwError != ERROR_SUCCESS)
        {
            ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - InternalWriteToMedia() failed.\r\n")));
            pSG_req->sr_status = dwError;
            return FALSE;
        }
        dwCurSector += dwNumSectors;

    }

    pSG_req->sr_status = ERROR_SUCCESS;
    return TRUE;
}


BOOL Fal::IsInRange (DWORD dwStartSector, DWORD dwNumSectors)
{
    return (dwStartSector >= m_dwStartLogSector &&
           ((dwStartSector + dwNumSectors) <= (m_dwStartLogSector + m_dwNumLogSectors)));
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StartupFAL()

Description:    Starts up the FLASH Abstraction Layer (FAL)

Notes:          The FLASH Media Driver (FMD) needs to be completely
                initialized before this function is called.  If
                startup fails, the caller is responsible for "cleaning
                things up" by calling ShutdownFAL().

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL Fal::StartupFAL(PFlashRegion pRegion)
{

    //----- 1. Cache size properties of the FLASH media -----
    m_dwSectorsPerBlock = pRegion->dwSectorsPerBlock;
    m_pRegion = pRegion;
    m_dwNumLogSectors = pRegion->dwNumLogicalBlocks * m_dwSectorsPerBlock;

    //----- 2. Initialize the LOGICAL -> PHYSICAL sector mapper -----
    if (!m_pMap->Init (pRegion))
    {
        ReportError((TEXT("FLASHDRV.DLL:StartupFAL() - Unable to initialize Logical --> Physical Mapper\r\n")));
        return FALSE;
    }

    //----- 3. Build the logical --> physical lookup table, free list, etc. -----
    if(!BuildupMappingInfo())
    {
        ReportError((TEXT("FLASHDRV.DLL:StartupFAL() - Unable to build the logical --> physical lookup table, free list, etc.!!!\r\n")));
        return FALSE;
    }

    return TRUE;
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ShutdownFAL()

Description:    Shuts down the FLASH Abstraction Layer (FAL)

Notes:          The logical --> physical mapper and sector manager
                are appropriately deinitialized.  Note that the caller
                is responsible for deinitializing the FMD.

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL Fal::ShutdownFAL()
{
    //----- 1. Deinitialize the LOGICAL -> PHYSICAL sector mapper -----
    if (!m_pMap->Deinit())
    {
        ReportError((TEXT("FLASHDRV.DLL:ShutdownFAL() - Unable to deinitialize Logical --> Physical Mapper\r\n")));
    }

    //----- 2. Deinitialize the sector manager -----
    if (!m_pSectorMgr->Deinit())
    {
        ReportError((TEXT("FLASHDRV.DLL:ShutdownFAL() - Unable to deinitialize Sector Manager\r\n")));
    }

    return TRUE;
}


BOOL XipFal::StartupFAL(PFlashRegion pRegion)
{

    if (!m_pSectorMgr->Init (pRegion, this, NULL))
    {
        ReportError((TEXT("FLASHDRV.DLL:StartupFAL() - Unable to initialize Sector Manager\r\n")));
        return FALSE;
    }

    return Fal::StartupFAL(pRegion);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       BuildupMappingInfo()

Description:    Reads the sector info for each sector on the media
                and builds up the MAPPING, FREE, and DIRTY sector info.

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL XipFal::BuildupMappingInfo()
{
    DWORD dwBlockID = 0;
    DWORD dwPhysSector = m_dwStartPhysSector;
    DWORD dwLogSector = m_dwStartLogSector;
    DWORD dwExistingPhysSector;

    DEBUGMSG(1, (TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Enter. \r\n"), dwBlockID));

    for (dwBlockID = m_pRegion->dwStartPhysBlock; dwBlockID < m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks; dwBlockID++)
    {
        DWORD iSector;
        DWORD dwStatus = FMD.pGetBlockStatus (dwBlockID);

        if (dwStatus & (BLOCK_STATUS_RESERVED | BLOCK_STATUS_BAD))
        {
            if(!m_pSectorMgr->MarkBlockUnusable(dwBlockID))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - SectorMgr::MarkBlockUnusable(%d) failed. \r\n"), dwBlockID));
            }
            dwPhysSector += m_pRegion->dwSectorsPerBlock;
            continue;
        }
        else if (dwStatus & BLOCK_STATUS_READONLY)
        {
            if(!m_pSectorMgr->AddSectorsToList (SectorMgr::ReadOnly, dwLogSector, m_pRegion->dwSectorsPerBlock))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as read only.\r\n"), dwPhysSector));
            }
        }
        else
        {
            if(!m_pSectorMgr->AddSectorsToList (SectorMgr::XIP, dwLogSector, m_pRegion->dwSectorsPerBlock))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as XIP.\r\n"), dwPhysSector));
            }
        }


        // Map logical sectors 1:1 with physical sectors
        for (iSector = 0; iSector < m_pRegion->dwSectorsPerBlock; iSector++)
        {
            if(!m_pMap->MapLogicalSector(dwLogSector + iSector, dwPhysSector + iSector, &dwExistingPhysSector))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"),
                                            dwLogSector + iSector, dwPhysSector + iSector));
            }
        }

        dwLogSector += m_pRegion->dwSectorsPerBlock;
        dwPhysSector += m_pRegion->dwSectorsPerBlock;
    }

    return TRUE;
}


DWORD XipFal::InternalWriteToMedia(DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuffer)
{
    LPBYTE pbBlock = NULL;
    DWORD dwError = ERROR_GEN_FAILURE;
    DWORD iSector;

    DWORD dwNumSectorsLeft = dwNumSectors;
    DWORD dwCurSector = dwStartSector;

    while (dwNumSectorsLeft)
    {
        DWORD dwPhysSector, dwPhysBlock, dwBlockOffset;
        LPBYTE pTmpBuffer = pBuffer;
        DWORD dwSectorsToWrite = m_pRegion->dwSectorsPerBlock;

        if (!m_pMap->GetPhysicalSectorAddr (dwCurSector, &dwPhysSector))
        {
            ReportError((TEXT("FLASHDRV.DLL:WriteXIPSectors() - Could not get physical sector for logical sector 0x%x.\r\n"), dwCurSector));
            dwError = ERROR_INVALID_PARAMETER;
            goto XIP_ERROR;
        }

        dwPhysBlock = dwPhysSector / m_pRegion->dwSectorsPerBlock;
        dwBlockOffset = dwPhysSector % m_pRegion->dwSectorsPerBlock;

        if (dwBlockOffset || (dwNumSectorsLeft < m_pRegion->dwSectorsPerBlock))
        {
            // If the write does not contain the entire block, read in the block,
            // apply to partial change to the buffer, erase the block, and write
            // it back out.

            if (!pbBlock)
            {
                pbBlock = (LPBYTE)LocalAlloc (LMEM_FIXED, m_pRegion->dwBytesPerBlock);
                if (!pbBlock)
                {
                    dwError = ERROR_OUTOFMEMORY;
                    goto XIP_ERROR;
                }
            }

            pTmpBuffer = pbBlock;

            //
            // Read the block in
            //

            for (iSector = 0; iSector < m_pRegion->dwSectorsPerBlock; iSector++)
            {
                if (!FMD.pReadSector(dwPhysBlock * m_pRegion->dwSectorsPerBlock + iSector, pTmpBuffer, NULL, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:WriteXIPSectors() - Read sector failed for 0x%x.\r\n"), dwPhysBlock * m_pRegion->dwSectorsPerBlock + iSector));
                    goto XIP_ERROR;
                }
                pTmpBuffer += g_pFlashMediaInfo->dwDataBytesPerSector;
            }

            dwSectorsToWrite = MIN (dwNumSectorsLeft, m_pRegion->dwSectorsPerBlock - dwBlockOffset);

            memcpy (pbBlock + dwBlockOffset * g_pFlashMediaInfo->dwDataBytesPerSector, pBuffer, dwSectorsToWrite * g_pFlashMediaInfo->dwDataBytesPerSector);

            pTmpBuffer = pbBlock;

        }

        if (!FMD.pEraseBlock (dwPhysBlock))
        {
            ReportError((TEXT("FLASHDRV.DLL:WriteXIPSectors() - Erase block failed for 0x%x.\r\n"), dwPhysBlock));
            goto XIP_ERROR;
        }

        //
        // Write the entire block out
        //

        for (iSector = 0; iSector < m_pRegion->dwSectorsPerBlock; iSector++)
        {
            if (!FMD.pWriteSector(dwPhysBlock * m_pRegion->dwSectorsPerBlock + iSector, pTmpBuffer, NULL, 1))
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteXIPSectors() - Write sector failed for 0x%x.\r\n"), dwPhysBlock * m_pRegion->dwSectorsPerBlock + iSector));
                goto XIP_ERROR;
            }
            pTmpBuffer += g_pFlashMediaInfo->dwDataBytesPerSector;
        }

        pBuffer += dwSectorsToWrite * g_pFlashMediaInfo->dwDataBytesPerSector;
        dwCurSector += dwSectorsToWrite;
        dwNumSectorsLeft -= dwSectorsToWrite;

        DEBUGMSG(1, (TEXT("%d\n"), dwPhysBlock));
    }

    dwError = ERROR_SUCCESS;

XIP_ERROR:

    if (pbBlock)
        LocalFree (pbBlock);

    return dwError;

}

BOOL XipFal::DeleteSectors(DWORD dwStartLogSector, DWORD dwNumSectors)
{
    return FALSE;
}

BOOL XipFal::SecureWipe()
{
    return FALSE;
}

DWORD XipFal::SetSecureWipeFlag(DWORD dwStartBlock)
{
    return INVALID_BLOCK_ID;
}

FileSysFal::FileSysFal (DWORD dwStartLogSector, DWORD dwStartPhysSector, BOOL fReadOnly) :
    Fal(dwStartLogSector, dwStartPhysSector, fReadOnly)
{
    m_pCompactor = new Compactor();
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StartupFAL()

Description:    Starts up the FLASH Abstraction Layer (FAL)

Notes:          The FLASH Media Driver (FMD) needs to be completely
                initialized before this function is called.  If
                startup fails, the caller is responsible for "cleaning
                things up" by calling ShutdownFAL().

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL FileSysFal::StartupFAL(PFlashRegion pRegion)
{

    //----- 3. Initialize the sector manager -----
    if (!m_pSectorMgr->Init (pRegion, this, m_pCompactor))
    {
        ReportError((TEXT("FLASHDRV.DLL:StartupFAL() - Unable to initialize Sector Manager\r\n")));
        return FALSE;
    }

    //----- 5. Initialize and start the compactor -----
    if(!m_pCompactor->Init (pRegion, this))
    {
        ReportError((TEXT("FLASHDRV.DLL:StartupFAL() - Unable to initialize the compactor!!!\r\n")));
        return FALSE;
    }

    return Fal::StartupFAL(pRegion);
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ShutdownFAL()

Description:    Shuts down the FLASH Abstraction Layer (FAL)

Notes:          The logical --> physical mapper and sector manager
                are appropriately deinitialized.  Note that the caller
                is responsible for deinitializing the FMD.

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL FileSysFal::ShutdownFAL()
{
    if(!m_pCompactor->Deinit())
    {
        ReportError((TEXT("FLASHDRV.DLL:ShutdownFAL() - Unable to deinitialize the Compactor\r\n")));
    }

    return Fal::ShutdownFAL();
}


BOOL FileSysFal::BuildupMappingInfo()
{
    DWORD i = 0;
    DWORD dwBlockID = 0;
    DWORD dwPhysSector = m_dwStartPhysSector;
    DWORD dwExistingPhysSector = 0;
    SectorMappingInfo  sectorMappingInfo;

    DEBUGMSG(1, (TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Enter. \r\n"), dwBlockID));

    //----- 1. Read the entire media to determine the status of all sectors -----
    for (dwBlockID = m_pRegion->dwStartPhysBlock; dwBlockID < m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks; dwBlockID++)
    {
        //----- 2. What is the status of this block? -----
        DWORD dwStatus = FMD.pGetBlockStatus (dwBlockID);

        if (dwStatus & (BLOCK_STATUS_BAD | BLOCK_STATUS_RESERVED))
        {
            if(!m_pSectorMgr->MarkBlockUnusable(dwBlockID))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - SectorMgr::MarkBlockUnusable(%d) failed. \r\n"), dwBlockID));
            }
            dwPhysSector += m_pRegion->dwSectorsPerBlock; // Move to first sector in next block
            continue;
        }

        //----- 4. Read the sector information stored on the physical media for each sector -----
        for(i=0; i<(m_pRegion->dwSectorsPerBlock); i++, dwPhysSector++)
        {

            //----- Notice that SectorMappingInfo is cast to SectorInfo.  SectorInfo is the public -----
            //      definition of the private structure SectorMappingInfo.  Consequently, any
            //      changes to the SectorMappingInfo structure will require the SectorInfo
            //      structure's size be updated accordingly.
            if(!FMD.pReadSector(dwPhysSector, NULL, (PSectorInfo)&sectorMappingInfo, 1))
            {
                ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to read sector information for sector %d\r\n"), dwPhysSector));
                goto INIT_ERROR;
            }

            if (IsSecureWipeInProgress(sectorMappingInfo))
            {
                SecureWipe();
                MarkSectorFree(sectorMappingInfo);
            }


            //----- 5. Free sector?  If so, add it to the "free sector list" the Sector Manager uses -----
            //         NOTE: Notice that we ONLY mark this sector as FREE if it is in a valid block.
            if(IsSectorFree(sectorMappingInfo))
            {
                // OPTIMIZATION: Because the compactor always adds whole blocks of FREE sectors to the
                //               Sector Manager, we know that if the first sector in a block is FREE then
                //               all other sectors in this block are also FREE.  Consequently, when the first
                //               sector in a block is FREE we don't have to read the other sectors in the block.
                if(i == 0)
                {
                    if(!m_pSectorMgr->AddSectorsToList(SectorMgr::Free, dwPhysSector, m_pRegion->dwSectorsPerBlock))
                    {
                        ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as free.\r\n"), dwPhysSector));
                    }
                    dwPhysSector += m_pRegion->dwSectorsPerBlock;
                    break;

                }

                // The whole block isn't FREE; add just this sector to the Sector Manager's FREE list...
                if(!m_pSectorMgr->AddSectorsToList(SectorMgr::Free, dwPhysSector, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as free.\r\n"), dwPhysSector));
                }

                continue;
            }

            //----- 6. DIRTY sector?  If so, inform the Sector Manager... -----
            if(IsSectorDirty(sectorMappingInfo))
            {
                if(!m_pSectorMgr->MarkSectorsAsDirty(dwPhysSector, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as dirty.\r\n"), dwPhysSector));
                }
                continue;
            }

            //----- 7. Mapped sector? If so, use logical --> sector mapper to create a mapping -----
            if(IsSectorMapped(sectorMappingInfo))
            {
                // First sector in a block must be marked RO to indicate all sectors in the block are RO.
                if ((dwStatus & BLOCK_STATUS_READONLY) && (i == 0))
                {
                    DWORD iSector;

                    if(!m_pSectorMgr->AddSectorsToList(SectorMgr::ReadOnly, sectorMappingInfo.logicalSectorAddr, m_pRegion->dwSectorsPerBlock))
                    {
                        ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark sector %x as read only.\r\n"), dwPhysSector));
                    }

                    for (iSector = 0; iSector < m_pRegion->dwSectorsPerBlock; iSector++)
                    {
                        if(!m_pMap->MapLogicalSector(sectorMappingInfo.logicalSectorAddr + iSector, dwPhysSector + iSector, &dwExistingPhysSector))
                        {
                            ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"),
                                                        sectorMappingInfo.logicalSectorAddr + iSector, dwPhysSector + iSector));
                        }
                    }

                    dwPhysSector += m_pRegion->dwSectorsPerBlock;
                    break;
                }


                if(!m_pMap->MapLogicalSector(sectorMappingInfo.logicalSectorAddr, dwPhysSector, &dwExistingPhysSector))
                {
                    ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"),
                                                sectorMappingInfo.logicalSectorAddr, dwPhysSector));
                }

                // SPECIAL CASE: It is possible that a power-failure occured just after the contents of a MAPPED
                //               logical sector were updated but BEFORE the old data was marked DIRTY.  This situation
                //               can be detected if (dwExistingPhysSector != UNMAPPED_LOGICAL_SECTOR).  To fix
                //               this case, we should keep the latest logical --> physical mapping information and
                //               mark the data in the dwExistingPhysSector as DIRTY.
                if ((dwExistingPhysSector != UNMAPPED_LOGICAL_SECTOR) && !m_fReadOnly)
                {
                    DEBUGMSG(ZONE_WRITE_OPS,(TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Power failure during the last WRITE operation is detected,  \
                                                   insuring data integrity for logical sector %08x\r\n"), sectorMappingInfo.logicalSectorAddr));

                    // NOTE: Since we don't know the contents of the sector mapping info for this physical sector,
                    //       we can safely write all '1's (except the bit used to indicate the sector is "dirty").
                    memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
                    MarkSectorDirty(sectorMappingInfo);

                    if(!FMD.pWriteSector(dwExistingPhysSector, NULL, (PSectorInfo)&sectorMappingInfo, 1))
                    {
                        DEBUGMSG(ZONE_WRITE_OPS,(TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to mark old physical sector 0x%08x as DIRTY!  Calling HandleWriteFailure()\r\n"),
                                                       dwExistingPhysSector));

                        // WRITE operation failed, try to recover...
                        if(!HandleWriteFailure(dwExistingPhysSector))
                        {
                            ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), dwPhysSector));
                            goto INIT_ERROR;
                        }
                    }


                    // Inform the Sector Manager that this sector is DIRTY...
                    if(!m_pSectorMgr->MarkSectorsAsDirty(dwExistingPhysSector, 1))
                    {
                        ReportError((TEXT("FLASHDRV.DLL:BuildupMappingInfo() - FATAL_ERROR: SM_MarkSectorsAsDirty(0x%08x) failed!\r\n"), dwExistingPhysSector));
                        goto INIT_ERROR;
                    }
                }
            }

        }
    }

    return TRUE;


INIT_ERROR:
    return FALSE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       InternalWriteToMedia()

Description:    Performs the specified scatter/gather WRITE request to the media.

Notes:          After parsing the parameters, the actual WRITE request is handled
                by the FLASH Media Driver (FMD).

Returns:        Boolean indicating success.
-------------------------------------------------------------------------------*/
DWORD FileSysFal::InternalWriteToMedia(DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuffer)
{
    DWORD        dwSGBuffPos                  = 0;
    DWORD        dwSectorAddr                = 0;

    SECTOR_ADDR  physicalSectorAddr          = 0;
    SECTOR_ADDR  existingPhysicalSectorAddr  = 0;

    SectorMappingInfo sectorMappingInfo;

    DWORD dwError = ERROR_GEN_FAILURE;


    for(dwSectorAddr = dwStartSector; dwSectorAddr < (dwStartSector + dwNumSectors); dwSectorAddr++)
    {
        //----- 2. Write the sector data to disk.  Notice that this operation is repetitively -----
        //         tried ONLY if FMD_WriteSector() fails.  Unless the FLASH block we are writing to
        //         has just gone bad, this loop will only execute once.
        for(;;)
        {
            //----- 3. Get a free physical sector to store the data into.  If this call FAILS, the WRITE -----
            //         cannot succeed because the media is full.
            if(!m_pSectorMgr->GetNextFreeSector(&physicalSectorAddr, FALSE))
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to get next free physical sector address for writing!  The media is full...\r\n")));
                dwError = ERROR_DISK_FULL;
                goto WRITE_ERROR;
            }

            //----- For debugging purposes, print out the logical --> physical mapping... -----
            DEBUGMSG(ZONE_WRITE_OPS, (TEXT("FLASHDRV.DLL:WriteToMedia() - logicalSector 0x%08x --> physicalSector 0x%08x\r\n"), dwSectorAddr, physicalSectorAddr));


            //----- 4. Start the write operation (used to safeguard against power failure conditions) -----
            //         The rationale is as follows: If we lose power during before the data is written
            //         to the media, this bit should be set and this will allow us to detect that a
            //         WRITE was in progress when the power failed.
            memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
            sectorMappingInfo.logicalSectorAddr = dwSectorAddr;
            MarkSectorWriteInProgress(sectorMappingInfo);

            if(!FMD.pWriteSector(physicalSectorAddr, NULL, (PSectorInfo)&sectorMappingInfo, 1))
            {
                DEBUGMSG(ZONE_WRITE_OPS, (TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to start WRITE operation.  Calling HandleWriteFailure()\r\n")));

                // WRITE operation failed, try to recover...
                if(!HandleWriteFailure(physicalSectorAddr))
                {
                    ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                    goto WRITE_ERROR;
                }
                continue;                                           // Try the WRITE at another physical sector...
            }

            //----- 5. Invoke the FLASH Media Driver (FMD) to write the sector data to the media -----
            //         NOTE: The write will be marked as "completed" if FMD_WriteSector() succeeeds
            //
            //         NOTE: Currently, only one sector is written at a time (last parameter)
            MarkSectorWriteCompleted(sectorMappingInfo);
            if(!FMD.pWriteSector(physicalSectorAddr, (pBuffer+dwSGBuffPos), (PSectorInfo)&sectorMappingInfo, 1))
            {
                DEBUGMSG(ZONE_WRITE_OPS,(TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to write to physical sector 0x%08x!  Calling HandleWriteFailure()\r\n"),
                                               physicalSectorAddr));

                // WRITE operation failed, try to recover...
                if(!HandleWriteFailure(physicalSectorAddr))
                {
                    ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                    goto WRITE_ERROR;
                }
                continue;                                           // Try the WRITE at another physical sector...
            }

            //----- 6. Perform the LOGICAL -> PHYSICAL mapping... -----
            if(!m_pMap->MapLogicalSector(dwSectorAddr, physicalSectorAddr, &existingPhysicalSectorAddr))
            {
                ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() = FATAL_ERROR: Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"),
                                            dwSectorAddr, physicalSectorAddr));
                goto WRITE_ERROR;
            }

            //----- 7. If this logical sector was already mapped, mark the "stale data" in the existing -----
            //         physical sector as "dirty."
            if(existingPhysicalSectorAddr != UNMAPPED_LOGICAL_SECTOR)
            {
                // NOTE: Since we don't know the contents of the sector mapping info for this physical sector,
                //       we can safely write all '1's (except the bit used to indicate the sector is "dirty").
                memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
                MarkSectorDirty(sectorMappingInfo);

                if(!FMD.pWriteSector(existingPhysicalSectorAddr, NULL, (PSectorInfo)&sectorMappingInfo, 1))
                {
                    DEBUGMSG(ZONE_WRITE_OPS,(TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to mark old physical sector 0x%08x as DIRTY!  Calling HandleWriteFailure()\r\n"),
                                                   existingPhysicalSectorAddr));

                    // WRITE operation failed, try to recover...
                    if(!HandleWriteFailure(existingPhysicalSectorAddr))
                    {
                        ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - Unable to handle the WRITE failure to sector 0x%08x\r\n"), physicalSectorAddr));
                        goto WRITE_ERROR;
                    }
                    continue;                                           // Try the WRITE at another physical sector...
                }


                // Inform the Sector Manager that this sector is DIRTY...
                if(!m_pSectorMgr->MarkSectorsAsDirty(existingPhysicalSectorAddr, 1))
                {
                    ReportError((TEXT("FLASHDRV.DLL:WriteToMedia() - FATAL_ERROR: SM_MarkSectorsAsDirty(0x%08x) failed!\r\n"), existingPhysicalSectorAddr));
                    goto WRITE_ERROR;
                }
            }

            //----- 8. At this point, the WRITE operation has completed successfully. -----
            break;
        }

        dwSGBuffPos  += g_pFlashMediaInfo->dwDataBytesPerSector;
    }

    return ERROR_SUCCESS;

WRITE_ERROR:
    return dwError;
}




/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       HandleWriteFailure()

Description:    Handles the case when a WRITE to the media fails.  If a WRITE operation
                fails, this is an indication that this FLASH block is going bad.
                Consequently, any remaining FREE sectors in this block should be removed
                from the Sector Manager and the block should be compacted.

Notes:          The second parameter to CompactBlock() handles the rare situation when
                marking an existing MAPPED sector as DIRTY fails.  In this situation, the
                block needs to be compacted and this sector needs to be treated as DIRTY
                regardless of what the mapping information says...

Returns:        Boolean indicating success.
------------------------------------------------------------------------------------*/
BOOL FileSysFal::HandleWriteFailure(SECTOR_ADDR physicalSectorAddr)
{
    BLOCK_ID     dwBlockID                    = 0;
    SECTOR_ADDR  firstPhysicalSectorAddr    = 0;

    //----- 1. Determine the dwBlockID for this physical sector address -----
    dwBlockID = m_pSectorMgr->GetBlockFromSector (physicalSectorAddr);

    //----- 2. Determine first physical sector address for this block -----
    firstPhysicalSectorAddr = m_pSectorMgr->GetStartSectorInBlock (dwBlockID);

    //----- 3. Inform the Sector Manager that any FREE sectors in this block should be removed... -----
    if(!m_pSectorMgr->UnmarkSectorsAsFree(firstPhysicalSectorAddr, m_pRegion->dwSectorsPerBlock))
    {
        ReportError((TEXT("FLASHDRV.DLL:HandleWriteFailure() - m_pSectorMgr->UnmarkSectorsAsFree(%d, %d) failed!\r\n"), firstPhysicalSectorAddr, m_pRegion->dwSectorsPerBlock));
        // NOTE: If this call FAILS, keep on going because CompactBlock() may be still be able to FREE some sectors in order for
        //       for the WRITE to complete.
    }

    //----- 4. Mark the failed sector as dirty, since CompactBlock will unmark it as dirty
    //
    if(!m_pSectorMgr->MarkSectorsAsDirty(physicalSectorAddr, 1))
    {
        ReportError((TEXT("FLASHDRV.DLL:HandleWriteFailure() - Unable to mark physical sector 0x%08x as DIRTY\r\n"), physicalSectorAddr));
    }

    //----- 5. Compact the block.  Any MAPPED sectors are relocated to another portion of the media -----
    //         and any DIRTY sectors are recycled into FREE sectors.
    if(m_pCompactor->CompactBlock(dwBlockID, physicalSectorAddr) == BLOCK_COMPACTION_ERROR)
    {
        ReportError((TEXT("FLASHDRV.DLL:HandleWriteFailure() - CompactBlock(%d, 0x%08x) failed!\r\n"), dwBlockID, physicalSectorAddr));
        goto HANDLE_WRITE_FAILURE_ERROR;
    }

    return TRUE;

HANDLE_WRITE_FAILURE_ERROR:
    return FALSE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       DeleteSectors()

Description:    Deletes the specified range of logical sectors.

Notes:          To "delete" a logical sector, the appropriate physical
                sector simply needs to be marked as DIRTY.

Returns:        Boolean indicating success.
-------------------------------------------------------------------*/
BOOL FileSysFal::DeleteSectors(DWORD dwStartLogSector, DWORD dwNumSectors)
{
    SECTOR_ADDR dwPhysSector = 0;
    DWORD i = 0;
    SectorMappingInfo sectorMappingInfo;

    if (m_fReadOnly)
    {
        SetLastError(ERROR_WRITE_PROTECT);
        return FALSE;
    }

    //----- 1. Check to make sure this deletion request doesn't run off the end of the media! -----
    if((dwStartLogSector+dwNumSectors) > g_dwAvailableSectors)
    {
        ReportError((TEXT("FLASHDRV.DLL:DeleteSectors() - Delete sectors (0x%08x - 0x%08x) exceeds the media size!\r\n"),
                                    dwStartLogSector, dwStartLogSector+dwNumSectors));
        goto DELETE_ERROR;
    }

    CELOG_DeleteSectors(dwStartLogSector, dwNumSectors);

    //----- 2. Setup the sector mapping info to mark this physical sector as DIRTY -----
    //         NOTE: Since we don't know the contents of the sector mapping info for this physical sector,
    //               we can safely write all '1's (except the bit used to indicate the sector is DIRTY ).
    memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
    MarkSectorDirty(sectorMappingInfo);

    for(i=0; i<dwNumSectors; i++)
    {

        //----- 3. Lookup the physical sector address for this logical sector -----
        if((!m_pMap->GetPhysicalSectorAddr((SECTOR_ADDR)(dwStartLogSector+i), &dwPhysSector)) || (dwPhysSector == UNMAPPED_LOGICAL_SECTOR))
        {
            // Redundant call to delete sector.  Just ignore it.
            return TRUE;
        }

        //----- 4. Mark the physical sector address as DIRTY -----
        if(!FMD.pWriteSector(dwPhysSector, NULL, (PSectorInfo)&sectorMappingInfo, 1))
        {
            ReportError((TEXT("FLASHDRV.DLL:DeleteSectors() - Unable to mark old physical sector 0x%08x as dirty!\r\n"), dwPhysSector));
            goto DELETE_ERROR;
        }

        //----- 5. Update the logical-->physical mapper to indicate this logical sector is no longer mapped -----
        if(!m_pMap->MapLogicalSector((dwStartLogSector+i), UNMAPPED_LOGICAL_SECTOR, &dwPhysSector))
        {
            ReportError((TEXT("FLASHDRV.DLL:DeleteSectors() = FATAL_ERROR: Unable to map logical sector 0x%08x to physical sector 0x%08x\r\n"),
                                        (dwStartLogSector+i), UNMAPPED_LOGICAL_SECTOR));
            goto DELETE_ERROR;
        }

        //----- 6. Inform the Sector Manager that this sector is dirty... -----
        if(!m_pSectorMgr->MarkSectorsAsDirty(dwPhysSector, 1))
        {
            ReportError((TEXT("FLASHDRV.DLL:DeleteSectors() - FATAL_ERROR: SM_MarkSectorsAsDirty(0x%08x) failed!\r\n"), dwPhysSector));
            goto DELETE_ERROR;
        }
    }

    return TRUE;

DELETE_ERROR:
    return FALSE;
}

BOOL FileSysFal::SecureWipe()
{
    DWORD i = 0;
    DWORD dwFirstGoodBlock = 0;
    DWORD dwSecondGoodBlock = 0;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:SecureWipe() - Enter\r\n")));

    // ----- 1. Mark the first good block with secure in progress
    dwFirstGoodBlock = SetSecureWipeFlag (m_pRegion->dwStartPhysBlock);
    if (dwFirstGoodBlock == INVALID_BLOCK_ID)
    {
        return FALSE;
    }

    // ----- 2. Erase all of the valid blocks on the FLASH region, except
    // -----    except the first, which contains the secure in progress bit
    for(i = dwFirstGoodBlock + 1; i < m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks; i++)
    {
        if(FMD.pGetBlockStatus((BLOCK_ID)i) & (BLOCK_STATUS_BAD | BLOCK_STATUS_RESERVED))
        {
            // Don't erase bad blocks on the media or blocks reserved for other use (such as bootloader).
            continue;
        }

        if(!FMD.pEraseBlock((BLOCK_ID)i))
        {
            ReportError((TEXT("FLASHDRV.DLL:SecureWipe() - FMD_EraseBlock(%d) failed\r\n"), i));

            if(!FMD.pSetBlockStatus((BLOCK_ID)i, BLOCK_STATUS_BAD))
            {
                ReportError((TEXT("FLASHDRV.SecureWipe() - FMD_MarkBlockBad(%d) failed\r\n"), i));
            }
            continue;
        }
    }

    // ----- 3. Mark the second good block with secure in progress
    dwSecondGoodBlock = SetSecureWipeFlag (dwFirstGoodBlock + 1);
    if (dwSecondGoodBlock == INVALID_BLOCK_ID)
    {
        return FALSE;
    }

    // ----- 4. Erase the first good block
    if(!FMD.pEraseBlock((BLOCK_ID)dwFirstGoodBlock))
    {
        ReportError((TEXT("FLASHDRV.DLL:SecureWipe() - FMD_EraseBlock(%d) failed\r\n"), dwFirstGoodBlock));

        if(!FMD.pSetBlockStatus((BLOCK_ID)dwFirstGoodBlock, BLOCK_STATUS_BAD))
        {
            ReportError((TEXT("FLASHDRV.SecureWipe() - FMD_MarkBlockBad(%d) failed\r\n"), dwFirstGoodBlock));
        }
    }

    // ----- 5. Erase the second good block
    if(!FMD.pEraseBlock((BLOCK_ID)dwSecondGoodBlock))
    {
        ReportError((TEXT("FLASHDRV.DLL:SecureWipe() - FMD_EraseBlock(%d) failed\r\n"), dwSecondGoodBlock));

        if(!FMD.pSetBlockStatus((BLOCK_ID)dwSecondGoodBlock, BLOCK_STATUS_BAD))
        {
            ReportError((TEXT("FLASHDRV.SecureWipe() - FMD_MarkBlockBad(%d) failed\r\n"), dwSecondGoodBlock));
        }
    }
    DEBUGMSG(ZONE_FUNCTION, (TEXT("FLASHDRV.DLL:SecureWipe() - Exit\r\n")));

    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SetSecureWipeFlag()

Description:    Sets the secure wipe flag in the sector info.  Starts
                looking at the block specified by dwStartBlock.  If
                dwStartBlock is INVALID_BLOCK_ID, start from the
                beginning of the region.

Returns:        The block in which the flag was set.
                INVALID_BLOCK_ID on error.
-------------------------------------------------------------------*/

DWORD FileSysFal::SetSecureWipeFlag(DWORD dwStartBlock)
{
    SectorMappingInfo sectorMappingInfo;
    DWORD dwBlock = dwStartBlock;

    if (m_fReadOnly)
    {
        SetLastError(ERROR_WRITE_PROTECT);
        return INVALID_BLOCK_ID;
    }

    // If a starting block wasn't provided, start from the beginning of the region.
    if (dwStartBlock == INVALID_BLOCK_ID)
    {
        dwBlock = m_pRegion->dwStartPhysBlock;
    }

    if (!m_pRegion->dwNumPhysBlocks || (dwBlock < m_pRegion->dwStartPhysBlock))
    {
        return INVALID_BLOCK_ID;
    }

    while (FMD.pGetBlockStatus((BLOCK_ID)dwBlock) & (BLOCK_STATUS_BAD | BLOCK_STATUS_RESERVED) &&
           dwBlock < m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks)
    {
        dwBlock++;
    }

    if (dwBlock >= m_pRegion->dwStartPhysBlock + m_pRegion->dwNumPhysBlocks)
    {
        return INVALID_BLOCK_ID;
    }

    // Mark the first good block with secure in progress
    DWORD dwPhysSector = m_pSectorMgr->GetStartSectorInBlock (dwBlock);
    memset(&sectorMappingInfo, 0xFF, sizeof(SectorMappingInfo));
    MarkSecureWipeInProgress(sectorMappingInfo);

    if(!FMD.pWriteSector(dwPhysSector, NULL, (PSectorInfo)&sectorMappingInfo, 1))
    {
        ReportError((TEXT("FLASHDRV.DLL:FileSysFal::SecureWipe() - Unable to mark sector in progress!\r\n")));
        return INVALID_BLOCK_ID;
    }

    return dwBlock;
}

