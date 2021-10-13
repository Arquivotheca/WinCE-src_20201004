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
/*++


 Module Name:    bincomp.h

 Abstract:       

 Contents:

--*/

#ifndef _BINCOMP_H_
#define _BINCOMP_H_

// Error codes for BinCompress and BinDecompress APIs.                                    
#define BC_ERROR_NO_ERROR           0
#define BC_ERROR_NOT_ENOUGH_MEMORY  1
#define BC_ERROR_INVALID_PARAMETER  2
#define BC_ERROR_BUFFER_OVERFLOW    3
#define BC_ERROR_FAILURE            4
#define BC_ERROR_CONFIGURATION      5

PVOID AllocBCBuffer (DWORD cbSize);

int BinCompress(
    IN      DWORD  dwWindowSize,
    IN      PVOID  pvSource,
    IN      DWORD  cbSource,
       OUT  PVOID  pvDestination,
    IN OUT  PDWORD pcbDestination
    );

int BinCompressROM(
    IN      DWORD  dwWindowSize,
    IN      PVOID  pvSource,
    IN      DWORD  cbSource,
       OUT  PVOID  pvDestination,
    IN OUT  PDWORD pcbDestination
    );

int BinDecompress(
    IN      PVOID  pvSource,
    IN      DWORD  cbSource,
       OUT  PVOID  pvDestination,
    IN OUT  PDWORD pcbDestination
    );

int BinDecompressROM(
    IN      PVOID  pvSource,
    IN      DWORD  cbSource,
       OUT  PVOID  pvDestination,
    IN OUT  PDWORD pcbDestination
    );

#endif // _BINCOMP_H_
