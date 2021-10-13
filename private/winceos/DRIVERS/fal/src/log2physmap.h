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

Module Name:    LOG2PHYSMAP.H

Abstract:       Logical --> Physical Sector Mapper module
  
-----------------------------------------------------------------------------*/

#ifndef _LOG2PHYSMAP_H_
#define _LOG2PHYSMAP_H_ 

#include <windows.h>
#include "falmain.h"

//---------------------------- Macro Definitions -----------------------------
#define  MASTER_TABLE_SIZE      256
#define  NUM_BITS_IN_BYTE       8


#define  MAX_PHYSICAL_ADDR_FOR_ONE_BYTES        0x000000FF
#define  MAX_PHYSICAL_ADDR_FOR_TWO_BYTES        0x0000FFFF
#define  MAX_PHYSICAL_ADDR_FOR_THREE_BYTES      0x00FFFFFF
#define  MAX_PHYSICAL_ADDR_FOR_FOUR_BYTES       0xFFFFFFFF

#define  ONE_BYTE_PHYSICAL_ADDR                 1
#define  TWO_BYTE_PHYSICAL_ADDR                 2
#define  THREE_BYTE_PHYSICAL_ADDR               3
#define  FOUR_BYTE_PHYSICAL_ADDR                4

#define  COMPUTE_OFFSET(x)                                                                                      \
                              /* NOTE: Notice that a shift operation is used to avoid a potentially expensive   \
                              /        division operation if the # of sectors/secondary table is a power of 2 */\
                              if(m_bIsNumSectorsPerSecTableLog2)                                                \
                              {                                                                                 \
                                 x = x - ((x >> m_dwNumSectorsPerSecTable) << m_dwNumSectorsPerSecTable);       \
                              }else{                                                                            \
                                 x = x % m_dwNumSectorsPerSecTable;                                             \
                              }                                                                                 \
                                                                                                                \
                              switch(m_cbPhysicalAddr)                                                \
                              {                                                                                 \
                                case ONE_BYTE_PHYSICAL_ADDR:    x = x;              break;                      \
                                case TWO_BYTE_PHYSICAL_ADDR:    x <<= 1;            break;                      \
                                case THREE_BYTE_PHYSICAL_ADDR:  x = (x << 1) + x;   break;                      \
                                case FOUR_BYTE_PHYSICAL_ADDR:   x <<= 2;            break;                      \
                              }                                                                                 \
                                                            


//------------------------------- Public Interface ------------------------------
class MappingTable
{
public:
    MappingTable(DWORD dwStartLogSector, DWORD dwStartPhysSector);
    BOOL Init(PFlashRegion pRegion);
    BOOL Deinit();
     
    PBYTE GetPhysicalSectorAddr(SECTOR_ADDR logicalSectorAddr, PSECTOR_ADDR pPhysicalSectorAddr);
    PBYTE MapLogicalSector(SECTOR_ADDR logicalSectorAddr, SECTOR_ADDR physicalSectorAddr, 
                           PSECTOR_ADDR pExistingPhysicalSectorAddr);
    BOOL  IsValidLogicalSector(SECTOR_ADDR logicalSectorAddr );

private:
    PUCHAR   m_pDynamicLUT[MASTER_TABLE_SIZE]; // Dynamic look-up table (DLUT) that stores logical --> physical
                                               // sector mapping information (see above)

    DWORD    m_cbPhysicalAddr;                 // Number of bytes used to store the physical sector address
    BOOL     m_bIsNumSectorsPerSecTableLog2;   // Indicates if # of sectors per secondary table is a power of 2
    DWORD    m_dwNumSectorsPerSecTable;        // Number of sectors mapped in each secondary table
    DWORD    m_dwSecondaryTableSize;           // Size (in bytes) of secondary tables

    DWORD    m_dwStartLogSector;
    DWORD    m_dwNumLogSectors;
    DWORD    m_dwStartPhysSector;
    DWORD    m_dwNumPhysSectors;

};
//------------------------------ Helper Functions ------------------------------

#endif _LOG2PHYSMAP_H_
