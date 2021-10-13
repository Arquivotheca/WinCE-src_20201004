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

Module Name:    LOG2PHYSMAP.C

Abstract:       Logical --> Physical Sector Mapper module

Notes:          The following module is used to translate logical sector addresses to physical 
                sector addresses on the media.  
                
                The following diagram illustrates the overall driver architecture:

                                ---------------
                                | File System |
                                ---------------
                                       |             uses logical sector addresses
                                ---------------
                                |     FAL     |
                                ---------------
                                       |             uses physical sector addresses
                                ---------------
                                |     FMD     |
                                ---------------
                                       |             uses program/erase algorithms
                               ------------------
                               | FLASH Hardware |
                               ------------------       
                
                From this diagram, it should be clear that the file system uses "logical" sector
                addresses while the FLASH Media Driver (FMD) only uses "physical" sector addresses.
                The translation scheme is very similar to a virtual memory system, although its 
                use is for an entirely different reason.  The logical --> physical translation 
                process is used to help facilitate "wear-leveling" of the FLASH media.  Thus, using
                this translation layer the data can be moved to *any* physical location on the 
                underlying media without the file system's knowledge.

                To store the mapping information, a dynamic look-up table (DLUT) is used.  The structure
                is as follows:

                        Master Table            Secondary Tables...
                          
                            -----     -------------------------------------
                            | 0 | --> | LS0 | LS1 | LS2 | LS3 | ... | LSk |
                            -----     -------------------------------------
                            | 1 | ...
                            -----
                            | 2 | --> NULL
                            -----
                             ...
                            | N | --> NULL
                            -----

                That is, each index in the "master table" points to "secondary table" that contains the actual
                logical --> physical mapping information.  Using this scheme, we only need to allocate the 
                storage for the "secondary tables" whenever the appropriate logical sectors are accessed.  If
                a logical sector is never accessed, there is no need to allocate memory for the logical --> physical
                mapping information.  Thus, we can greatly decrease the memory usage in systems that do not access
                the entire range of logical sectors on the FLASH media.
                
                To help clarify the technique employed, let us given an example.  Suppose that the file system
                wants to write to the first 20 logical sectors (LS0-LS19).  In this scenario, we only have to allocate
                the appropriate number of secondary tables so that logical sectors 0-19 can be mapped.  Practically,
                this means that logical sectors 80, 255, 67, 2461, etc. do not have to be mapped in memory and thereby
                do not have to waste memory.  Hence, using this strategy, we have a dynamic look-up table.

                Notice that the total number of sectors on the FLASH media (i.e. the size of the media) and the choice
                of N for the master table determine how many logical sectors can be mapped in a single secondary 
                table.  Further notice that the size of the FLASH media determines how many bytes we must allocate
                to record the physical sector address for each logical sector.  For example, a 16MB FLASH device only has
                2^15 addressable sectors on the media.  Hence, 2 bytes can be used to store the physical sector address.
                If we choose N=256 for our master table, this means that we need to map 128 logical sectors for each
                of the secondary tables.  Each physical sector address requires 2 bytes, so a total of (128*2)=256 bytes
                are required every time a secondary table is allocated.  Referring to our example above, we can effectively
                map logical sectors 0-19 (and all the way up to 127) by only using 256 bytes of memory (excluding the
                memory used for the master table)!


Environment:    This module is linked into the FAL to procduce (FAL.LIB).  (FAL.LIB) is then linked 
                with a FLASH Media Driver (FMD.LIB) in order to create the full FLASH driver (FLASHDRV.DLL).

-----------------------------------------------------------------------------*/

#include "log2physmap.h"

//------------------------------ GLOBALS -------------------------------------------

//----------------------------------------------------------------------------------


MappingTable::MappingTable(DWORD dwStartLogSector, DWORD dwStartPhysSector) :
    m_cbPhysicalAddr(0),
    m_bIsNumSectorsPerSecTableLog2(TRUE),
    m_dwNumSectorsPerSecTable(0),
    m_dwSecondaryTableSize(0),
    m_dwStartLogSector(dwStartLogSector),
    m_dwStartPhysSector(dwStartPhysSector)
{
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       MappingTable::InitMapper()

Description:    Initializes the logical --> physical mapper.

Notes:          This function should be called at driver initialization.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL MappingTable::Init(PFlashRegion pRegion)
{   
    DWORD i                         = 0;
    DWORD dwLastPhysSector = 0;

    //***** Using the size information for the FLASH media, determine the size characteristics for the DLUT *****
    m_dwNumLogSectors = pRegion->dwSectorsPerBlock * pRegion->dwNumLogicalBlocks;
    m_dwNumPhysSectors = pRegion->dwSectorsPerBlock * pRegion->dwNumPhysBlocks;
    dwLastPhysSector = m_dwStartPhysSector + m_dwNumPhysSectors - 1;
    
    //----- 1. Determine number of sectors for each secondary table... -----
    //         NOTE: If the # of sectors per secondary table is a power of 2, we cache this value
    //               into m_dwNumSectorsPerSecTable - doing so avoids a potentially expensive division 
    //               operation in the MappingTable::GetPhysicalSectorAddr() routine

    if((m_dwNumSectorsPerSecTable = ComputeLog2((m_dwNumLogSectors + MASTER_TABLE_SIZE - 1) / MASTER_TABLE_SIZE)) == NOT_A_POWER_OF_2)
    {
        m_bIsNumSectorsPerSecTableLog2 = FALSE;                 // # of sectors per secondary table is NOT a power of 2
        
        m_dwNumSectorsPerSecTable = (m_dwNumLogSectors + MASTER_TABLE_SIZE - 1) / MASTER_TABLE_SIZE;
    }

    //----- 2. Determine number of bytes needed to store the physical sector address -----
    m_cbPhysicalAddr = ONE_BYTE_PHYSICAL_ADDR;
    if(dwLastPhysSector >= MAX_PHYSICAL_ADDR_FOR_ONE_BYTES)
    {
        m_cbPhysicalAddr = TWO_BYTE_PHYSICAL_ADDR;
        if(dwLastPhysSector >= MAX_PHYSICAL_ADDR_FOR_TWO_BYTES)
        {
            m_cbPhysicalAddr = THREE_BYTE_PHYSICAL_ADDR;
            if(dwLastPhysSector >= MAX_PHYSICAL_ADDR_FOR_THREE_BYTES)
            {
                m_cbPhysicalAddr = FOUR_BYTE_PHYSICAL_ADDR;
            }
        }
    }

    //----- 3. Compute the size of the secondary tables (needed for later allocations) -----
    if(m_bIsNumSectorsPerSecTableLog2)
    {
        m_dwSecondaryTableSize = (0x00000001 << m_dwNumSectorsPerSecTable) * m_cbPhysicalAddr;        
    }else
    {
        m_dwSecondaryTableSize = m_dwNumSectorsPerSecTable * m_cbPhysicalAddr;        
    }

    //----- 4. Initialize the master table (set all of the secondary table pointers to NULL) -----
    for(i=0; i<MASTER_TABLE_SIZE; i++)
    {
        m_pDynamicLUT[i] = NULL;
    }

    return TRUE;

}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       MappingTable::DeinitMapper()

Description:    Deinitializes the logical --> physical mapper.

Notes:          This function should be called at driver deinitialization.

Returns:        Boolean indicating success.
------------------------------------------------------------------------------*/
BOOL MappingTable::Deinit()
{
    DWORD i = 0;
    
    //----- 1. Free all of the secondary tables allocated off the master table -----
    for(i=0; i<MASTER_TABLE_SIZE; i++)
    {
        if((m_pDynamicLUT[i] != NULL) && (LocalFree(m_pDynamicLUT[i]) != NULL))
        {
            ReportError((TEXT("FLASHDRV.DLL:DeinitMapper() - Unable to deallocate secondary table %d!\r\n"), i));
        }
    }

    return TRUE;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       MappingTable::GetPhysicalSectorAddr()

Description:    Returns the physical sector address for the specified logical 
                sector address.  If the logical sector address is NOT mapped, 
                then pPhysicalSectorAddr is set to 0xFFFFFFFF.

Returns:        Pointer to the position in secondary table containing physical sector
                address; otherwise NULL if the secondary table doesn't exist.
------------------------------------------------------------------------------*/
PBYTE MappingTable::GetPhysicalSectorAddr(SECTOR_ADDR logicalSectorAddr, PSECTOR_ADDR pPhysicalSectorAddr)
{
    DWORD dwSecondaryTableID = 0;

    //----- 0. Check the parameters to insure they are legitimate -----
    if(pPhysicalSectorAddr == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:MappingTable::GetPhysicalSectorAddr() - Invalid parameter: pPhysicalSectorAddr == NULL\r\n")));
        return NULL;
    }

    *pPhysicalSectorAddr = UNMAPPED_LOGICAL_SECTOR;
        
    if (logicalSectorAddr < m_dwStartLogSector || logicalSectorAddr >= (m_dwStartLogSector + m_dwNumLogSectors)) 
    {
        ReportError((TEXT("FLASHDRV.DLL:MappingTable::GetPhysicalSectorAddr() - Logical sector(0x%x) is out of range\r\n"), logicalSectorAddr));
        return NULL;
    }

    //----- 1.  Rebase the logical sector that we are interested in before looking it up in the secondary table -----
    logicalSectorAddr -= m_dwStartLogSector;

    //----- 2. Determine the secondary table for this logical sector address -----
    //         NOTE: Notice that a shift operation is used to avoid a potentially expensive
    //               division operation if the # of sectors/secondary table is a power of 2
    if(m_bIsNumSectorsPerSecTableLog2)
    {
        dwSecondaryTableID = logicalSectorAddr >> m_dwNumSectorsPerSecTable;
    }else
    {
        dwSecondaryTableID = logicalSectorAddr / m_dwNumSectorsPerSecTable;
    }

    if (dwSecondaryTableID >= MASTER_TABLE_SIZE)
        goto GET_ERROR;    

    //----- 3. If this logical sector's secondary table isn't setup, a logical --> physical mapping does NOT exist -----
    if(m_pDynamicLUT[dwSecondaryTableID] == NULL)   
    {
        DEBUGMSG(ZONE_INIT, (TEXT("FLASHDRV.DLL:MappingTable::GetPhysicalSectorAddr() - Secondary table doesn't exist for logical sector 0x%x!!!\r\n"), logicalSectorAddr));
        *pPhysicalSectorAddr = 0xFFFFFFFF;      // Logical sector is NOT mapped to a physical sector...
        goto GET_ERROR;
    }

    //----- 4. Determine the offset within the secondary table for this logical sector address -----
    COMPUTE_OFFSET(logicalSectorAddr);  

    //----- 5. Lookup the physical sector address in the secondary table -----
    //         NOTE: For performance reasons, all multiplications are avoided.  Hence, it is necessary to explicitly 
    //               handle the four different cases for the physical sector address width.
    switch(m_cbPhysicalAddr)
    {
    case ONE_BYTE_PHYSICAL_ADDR:                //  0  >= FLASH media <= 2^8  sectors
        *pPhysicalSectorAddr  = MAX_PHYSICAL_ADDR_FOR_ONE_BYTES & (UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr));
        if(*pPhysicalSectorAddr == MAX_PHYSICAL_ADDR_FOR_ONE_BYTES)
        {
            *pPhysicalSectorAddr = 0xFFFFFFFF;  // Logical sector is NOT mapped to a physical sector...
        }
        break;

    case TWO_BYTE_PHYSICAL_ADDR:                // 2^8 >  FLASH media <= 2^16 sectors
        *pPhysicalSectorAddr  = MAX_PHYSICAL_ADDR_FOR_TWO_BYTES & ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr))<<8);
        *pPhysicalSectorAddr += (UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+1));
        if(*pPhysicalSectorAddr == MAX_PHYSICAL_ADDR_FOR_TWO_BYTES)
        {
            *pPhysicalSectorAddr = 0xFFFFFFFF;  // Logical sector is NOT mapped to a physical sector...
        }       
        break;
        
    case THREE_BYTE_PHYSICAL_ADDR:              // 2^16 >  FLASH media <= 2^24 sectors
        *pPhysicalSectorAddr  = MAX_PHYSICAL_ADDR_FOR_THREE_BYTES & ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr))<<16);
        *pPhysicalSectorAddr += ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+1))<<8);
        *pPhysicalSectorAddr += (UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+2));
        if(*pPhysicalSectorAddr == MAX_PHYSICAL_ADDR_FOR_THREE_BYTES)
        {
            *pPhysicalSectorAddr = 0xFFFFFFFF;  // Logical sector is NOT mapped to a physical sector...
        }       
        break;

    case FOUR_BYTE_PHYSICAL_ADDR:               // 2^24 > FLASH media <= 2^32 sectors
        *pPhysicalSectorAddr  = MAX_PHYSICAL_ADDR_FOR_FOUR_BYTES & ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr))<<24);
        *pPhysicalSectorAddr += ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+1))<<16);
        *pPhysicalSectorAddr += ((UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+2))<<8);
        *pPhysicalSectorAddr += (UCHAR)(*(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr+3));
        break;
    }

    CELOG_GetPhysicalSectorAddr (logicalSectorAddr, *pPhysicalSectorAddr);

    return (PBYTE)(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr);

GET_ERROR:
    return NULL;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       MappingTable::MapLogicalSector()

Description:    Maps the specified logical sector address to the specified
                physical sector address.

Notes:          If the logical sector is already mapped to a physical sector,
                the secondary table is appropriately updated with the new
                physical sector address.

Returns:        Pointer to the position in secondary table containing physical sector
                address; otherwise NULL if the secondary table doesn't exist.
------------------------------------------------------------------------------*/
PBYTE MappingTable::MapLogicalSector(SECTOR_ADDR logicalSectorAddr, SECTOR_ADDR physicalSectorAddr, 
                           PSECTOR_ADDR pExistingPhysicalSectorAddr)

{
    PBYTE       pMappedPhysicalSectorAddr  = 0;
    DWORD       dwSecondaryTableID         = 0;

    //----- 0. Check the parameters to insure they are legitimate -----
    if(pExistingPhysicalSectorAddr == NULL)
    {
        ReportError((TEXT("FLASHDRV.DLL:MappingTable::MapLogicalSector() - Invalid parameter: pExistingPhysicalSectorAddr == NULL\r\n")));
        return NULL;
    }

    *pExistingPhysicalSectorAddr = UNMAPPED_LOGICAL_SECTOR;        

    if (logicalSectorAddr < m_dwStartLogSector || logicalSectorAddr >= (m_dwStartLogSector + m_dwNumLogSectors)) 
    {
        ReportError((TEXT("FLASHDRV.DLL:MappingTable::MapLogicalSector() - Logical sector(0x%x) is out of range\r\n"), logicalSectorAddr));
        return NULL;
    }

    //----- 1. Get a pointer to the mapped physical sector address in our secondary table -----
    pMappedPhysicalSectorAddr = GetPhysicalSectorAddr(logicalSectorAddr, pExistingPhysicalSectorAddr);

    //----- 2.  Rebase the logical sector that we are interested in before looking it up in the secondary table -----
    logicalSectorAddr -= m_dwStartLogSector;

    if(pMappedPhysicalSectorAddr == NULL)
    {
        //----- 3. Secondary table for this logical sector address does NOT exist, let's create it... -----
        //         NOTE: Notice that a shift operation is used to avoid a potentially expensive
        //               division operation if the # of sectors/secondary table is a power of 2
        if(m_bIsNumSectorsPerSecTableLog2)
        {
            dwSecondaryTableID = logicalSectorAddr >> m_dwNumSectorsPerSecTable;
        }else
        {
            dwSecondaryTableID = logicalSectorAddr / m_dwNumSectorsPerSecTable;
        }

        if (dwSecondaryTableID >= MASTER_TABLE_SIZE)
            goto MAPPING_ERROR;    

        if((m_pDynamicLUT[dwSecondaryTableID] = (PBYTE)LocalAlloc(LMEM_FIXED, m_dwSecondaryTableSize)) == NULL)
        {
            ReportError((TEXT("FLASHDRV.DLL:MapLogicalSector() - Unable to allocate secondary table for logical sector %d\r\n"), logicalSectorAddr));
            goto MAPPING_ERROR;
        }

        //----- 4. Initialize the secondary table so that all logical sector addresses are unmapped (physical sector address is all '1's) -----
        memset(m_pDynamicLUT[dwSecondaryTableID], 0xFF, m_dwSecondaryTableSize);

        //----- 5. Determine the offset within the secondary table for this logical sector address -----
        COMPUTE_OFFSET(logicalSectorAddr);

        //----- 6. Get a pointer to the position in the secondary table that stores the physical sector address -----
        pMappedPhysicalSectorAddr = (PBYTE)(m_pDynamicLUT[dwSecondaryTableID]+logicalSectorAddr);
    }

    //----- 7. Map the specified logical sector address to the specified physical sector address -----
    //         NOTE: For performance reasons, all multiplications are avoided.  Hence, it is necessary to explicitly 
    //               handle the four different cases for the physical sector address width.
    switch(m_cbPhysicalAddr)
    {
    case ONE_BYTE_PHYSICAL_ADDR:                //  0  >= FLASH media <= 2^8  sectors
        *pMappedPhysicalSectorAddr     = (UCHAR)physicalSectorAddr;
        break;

    case TWO_BYTE_PHYSICAL_ADDR:                // 2^8 >  FLASH media <= 2^16 sectors
        *pMappedPhysicalSectorAddr     = (UCHAR)(physicalSectorAddr>>8);
        *(pMappedPhysicalSectorAddr+1) = (UCHAR)(physicalSectorAddr);
        break;

    case THREE_BYTE_PHYSICAL_ADDR:              // 2^16 > FLASH media <= 2^24 sectors
        *pMappedPhysicalSectorAddr     = (UCHAR)(physicalSectorAddr>>16);
        *(pMappedPhysicalSectorAddr+1) = (UCHAR)(physicalSectorAddr>>8);
        *(pMappedPhysicalSectorAddr+2) = (UCHAR)(physicalSectorAddr);
        break;

    case FOUR_BYTE_PHYSICAL_ADDR:               // 2^24 > FLASH media <= 2^32 sectors
        *pMappedPhysicalSectorAddr     = (UCHAR)(physicalSectorAddr>>24);
        *(pMappedPhysicalSectorAddr+1) = (UCHAR)(physicalSectorAddr>>16);
        *(pMappedPhysicalSectorAddr+2) = (UCHAR)(physicalSectorAddr>>8);
        *(pMappedPhysicalSectorAddr+3) = (UCHAR)(physicalSectorAddr);
        break;
    }

    CELOG_MapLogicalSector(logicalSectorAddr, physicalSectorAddr, *pMappedPhysicalSectorAddr);

    return pMappedPhysicalSectorAddr;

MAPPING_ERROR:
    return NULL;
}



//--------------------------------------- Helper Functions ------------------------------------
BOOL MappingTable::IsValidLogicalSector(SECTOR_ADDR logicalSectorAddr)
{
    if (logicalSectorAddr < m_dwStartLogSector || logicalSectorAddr >= (m_dwStartLogSector + m_dwNumLogSectors)) 
        return FALSE;
    else
        return TRUE;
}


