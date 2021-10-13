//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
