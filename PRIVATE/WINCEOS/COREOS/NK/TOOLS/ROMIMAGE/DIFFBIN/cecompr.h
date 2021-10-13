//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// cecompr.h

#pragma once

////////////////////////////////////////////////////////////
// Compression routine declarations
////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

DWORD CECompress(               // Length of output
    LPBYTE  lpbSrc,             // Input buffer
    DWORD   cbSrc,              // Input size (after applying steprate)
    LPBYTE  lpbDest,            // Output buffer (or NULL for length return)
    DWORD   cbDest,             // Maximum output size
    WORD    wStep);             // Must be 1 or 2

DWORD CEDecompress(             // Length of output
    LPBYTE  lpbSrc,             // Input buffer
    DWORD   cbSrc,              // Input length
    LPBYTE  lpbDest,            // Output buffer
    DWORD   cbDest,             // Maximium output size (after applying steprate)
    DWORD   dwSkip,             // Number of outputs to skip
    WORD    wStep);             // Must be 1 or 2

#ifdef __cplusplus
}
#endif

