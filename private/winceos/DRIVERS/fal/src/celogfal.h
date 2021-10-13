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
#ifndef _CELOGFAL_H_
#define _CELOGFAL_H_

#ifndef UNDER_NT


#include <celog.h>

#define CELID_FLASH_COMPACTION             CELID_USER+0
typedef struct  __CEL_FLASH_COMPACTION {
    BOOL fCritical;
    DWORD dwBlock;
} CEL_FLASH_COMPACTION, *PCEL_FLASH_COMPACTION;

_inline void CELOG_CompactBlock (BOOL fCritical, DWORD dwBlock)
{
    if (ZONE_CELOG_COMPACTION && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CEL_FLASH_COMPACTION cl;
        cl.fCritical = fCritical;
        cl.dwBlock = dwBlock;
        CeLogData(TRUE, CELID_FLASH_COMPACTION, (PVOID) &cl,
                  (WORD) (sizeof (CEL_FLASH_COMPACTION)),
                  0, CELZONE_ALWAYSON, 0, FALSE);
    }
}

_inline void CELOG_ReadSectors (DWORD dwStartSector, DWORD dwNumSectors)
{
    if (ZONE_CELOG_VERBOSE && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CeLogMsg(TEXT("%MSFLASH!ReadSectors - Start: %d, Num: %d"), 
                 dwStartSector, dwNumSectors);
    }
}


_inline void CELOG_WriteSectors (DWORD dwStartSector, DWORD dwNumSectors)
{
    if (ZONE_CELOG_VERBOSE && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CeLogMsg(TEXT("%MSFLASH!WriteSectors - Start: %d, Num: %d"), 
                 dwStartSector, dwNumSectors);
    }
}

_inline void CELOG_DeleteSectors (DWORD dwStartSector, DWORD dwNumSectors)
{
    if (ZONE_CELOG_VERBOSE && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CeLogMsg(TEXT("%MSFLASH!DeleteSectors - Start: %d, Num: %d"), 
                 dwStartSector, dwNumSectors);
    }
}

_inline void CELOG_GetPhysicalSectorAddr (DWORD dwLogicalSector, DWORD dwPhysicalSector)
{
    if (ZONE_CELOG_VERBOSE && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CeLogMsg(TEXT("%MSFLASH!GetPhysicalSectorAddr - Logical sector %d mapped to physical sector %d"), 
                 dwLogicalSector, dwPhysicalSector);
    }
}

_inline void CELOG_MapLogicalSector (DWORD dwLogicalSector, DWORD dwPhysicalSector, DWORD dwOldPhysicalSector)
{
    if (ZONE_CELOG_VERBOSE && IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CeLogMsg(TEXT("%MSFLASH!MapLogicalSector - Writing logical sector %d to physical sector %d, was physical sector %d"), 
                 dwLogicalSector, dwPhysicalSector, dwOldPhysicalSector);
    }
}


#else // UNDER_NT

#define CELOG_CompactBlock(fCritical,dwBlock)
#endif // UNDER_NT

#ifdef DEBUG

#define ReportError(Printf_exp) \
    ASSERT(0); \
    DEBUGMSG(ZONE_ERROR,Printf_exp); \
    DEBUGCELOGMSG(ZONE_CELOG_ERROR,Printf_exp);

#else

#define ReportError(Printf_exp) \
    RETAILMSG(ZONE_ERROR,(TEXT("MSFlash Error: Function: %S,  Line: %d"), __FUNCTION__, __LINE__)); \
    RETAILCELOGMSG(ZONE_CELOG_ERROR,(TEXT("MSFlash Error: Function: %S,  Line: %d"), __FUNCTION__, __LINE__));

#endif

#endif  // _CELOGFAL_H_


