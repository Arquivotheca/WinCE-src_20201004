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
#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

// --------------------------------------------------------------------
// Function table definition
// --------------------------------------------------------------------

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

#define SINGLE_UNIT_REQUEST         0
#define MULTI_UNIT_REQUEST          1
#define MULTI_UNIT_SPARSE_REQUEST   2
// --------------------------------------------------------------------
// Function Table
// --------------------------------------------------------------------
BEGIN_FTE
FTH(0, "Required Functionality Tests")
    FTE(1, "Test the IOCTL_FLASH_PDD_GET_REGION_COUNT IOCTL",                                           1001, 0,                         Tst_GetRegionCount)
    FTE(1, "Test the IOCTL_FLASH_PDD_GET_REGION_INFO IOCTL",                                            1002, 0,                         Tst_GetRegionInfo)
    FTE(1, "Test the IOCTL_FLASH_PDD_GET_BLOCK_STATUS IOCTL (one block per query)",                     1003, SINGLE_UNIT_REQUEST,       Tst_GetBlockStatus)
    FTE(1, "Test the IOCTL_FLASH_PDD_GET_BLOCK_STATUS IOCTL (multiple blocks per query)",               1004, MULTI_UNIT_REQUEST,        Tst_GetBlockStatus)
    FTE(1, "Test the IOCTL_FLASH_PDD_SET_BLOCK_STATUS IOCTL (one block per request)",                   1005, SINGLE_UNIT_REQUEST,       Tst_SetBlockStatus)
    FTE(1, "Test the IOCTL_FLASH_PDD_SET_BLOCK_STATUS IOCTL (multiple blocks per request)",             1006, MULTI_UNIT_REQUEST,        Tst_SetBlockStatus)
    FTE(1, "Test the IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS IOCTL (one sector per request)",             1007, SINGLE_UNIT_REQUEST,       Tst_ReadSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS IOCTL (multiple sectors per request)",       1008, MULTI_UNIT_REQUEST,        Tst_ReadSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS IOCTL (multiple discontiguous sectors)",     1009, MULTI_UNIT_SPARSE_REQUEST, Tst_ReadSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS IOCTL (one sector per request)",            1010, SINGLE_UNIT_REQUEST,       Tst_WriteSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS IOCTL (multiple sectors per request)",      1011, MULTI_UNIT_REQUEST,        Tst_WriteSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS IOCTL (multiple discontiguous sectors)",    1012, MULTI_UNIT_SPARSE_REQUEST, Tst_WriteSectors)
    FTE(1, "Test the IOCTL_FLASH_PDD_ERASE_BLOCKS IOCTL (one block per request)",                       1013, SINGLE_UNIT_REQUEST,       Tst_EraseBlocks)
    FTE(1, "Test the IOCTL_FLASH_PDD_ERASE_BLOCKS IOCTL (multiple blocks per request)",                 1014, MULTI_UNIT_REQUEST,        Tst_EraseBlocks)
    FTE(1, "Test the IOCTL_FLASH_PDD_ERASE_BLOCKS IOCTL (multiple discontiguous blocks)",               1015, MULTI_UNIT_SPARSE_REQUEST, Tst_EraseBlocks)
FTH(0, "Optional Functionality Tests")
    FTE(1, "Test the IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS IOCTL",                                      2001, 0,                         Tst_CopySectors)
END_FTE

#endif // !__FT_H__ || __GLOBALS_CPP__
