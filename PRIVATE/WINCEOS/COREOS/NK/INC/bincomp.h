/*++

 Copyright (c) 1998-2000 Microsoft Corporation

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
