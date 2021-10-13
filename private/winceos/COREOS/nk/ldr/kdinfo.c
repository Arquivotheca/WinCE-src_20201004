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
/*
 * Kernel Debugger Information structure.
 *
 * Module Name:
 *
 *              kdinfo.c
 *
 * Abstract:
 *
 *              This file contains information that the debugger needs in order to debug an image.
 *
 */

#include "kernel.h"
#include "osaxs_common.h"


// Both OsAxsDataBlock_2 and OsAxsHwTrap have been located to LDR in CE 6.0 because the .nb0
// format is not documented and we depend on the makeimg tools to generate
// a metadata file containing information normally parsed from a BIN file.
// Unfortunately, this metadata only contains information for NK.EXE.
//
// On the positive side, NK.EXE now has a very tiny data section, and memory searches for
// the OsAxsDataBlock are now extremely swift.

// Most of the fields for these structures are now initialized in SysDebugInit().
_declspec(align(64)) OSAXS_KERN_POINTERS_2 OsAxsDataBlock_2 =
{
    SIGNATURE_OSAXS_KERN_POINTERS_2,
    sizeof(OsAxsDataBlock_2)
};

// OsAxsHwTrap's fields are initialized in ConnectHdstub().
_declspec(align(64)) OSAXS_HWTRAP OsAxsHwTrap = 
{
    SIGNATURE_OSAXS_HWTRAP,
    sizeof(OsAxsHwTrap)
};


void SetOsAxsDataBlockPointer(struct KDataStruct *pKData)
{
    // Note regarding adding pointers to g_pKData.
    // On SH4, there are actually two KData structures, one for LDR that serves as a "proto" KData
    // and the actual KData structure in kernel.dll (there for performance reasons.)
    //
    // If you are going to add a pointer to KData to pass into kernel.dll, remember to update
    // SHInit() in mdsh3.c to copy the new pointer to the actual KData structure.
    
    pKData->pOsAxsHwTrap = &OsAxsHwTrap;
    pKData->pOsAxsDataBlock = &OsAxsDataBlock_2;
}
