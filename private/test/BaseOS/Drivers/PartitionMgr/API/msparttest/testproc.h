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
#include <TuxMain.H>

#pragma once

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

