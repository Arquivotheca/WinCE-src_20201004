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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#if !defined(_DIB_H)
#define _DIB_H

// Macro to determine the number of bytes per line in the DIB bits. This
//  accounts for DWORD alignment by adding 31 bits, then dividing by 32,
//  then rounding up to the next highest count of 4-bytes. Then, we
//  multiply by 4 to get the total byte count.
#define BYTESPERLINE(Width, BPP) ((WORD)((((DWORD)(Width) * (DWORD)(BPP) + 31) >> 5)) << 2)



HBITMAP DSCreateDIBSection( int nX, int nY, WORD wBits );
HPALETTE WINAPI DSCreateSpectrumPalette( void );
UINT TestDIBSections( HWND hWndParent, int width, WORD wBits );
void CheckMemory();

WORD awBPPOpts[] = {1, 2, 4, 8, 16, 24, 32};

#endif
