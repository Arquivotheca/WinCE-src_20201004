//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <TuxMain.H>

#ifndef __TESTPROC_H__

#define PART_COUNT      3

TESTPROCAPI Tst_Template        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_StoreBVT        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_PartBVT         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_MultiPart       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_PartRename      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_ReadWrite       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_DiskInfo        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_InfoUpdate      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_PartitionBounds (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

#endif // __TESTPROC_H__
