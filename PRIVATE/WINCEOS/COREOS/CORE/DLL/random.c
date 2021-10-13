//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------------
//
//
// Module:
//
//    random.c
//
// Purpose:
//
//    Implementation of Random.
//
// --------------------------------------------------------------------------

#include <windows.h>

DWORD sr;

int RandBit() {
	if (!sr)
		if (!(sr = GetTickCount()))
			sr = 1;
    sr = ((((sr>>7) ^ (sr>>5) ^ (sr>>2) ^ (sr>>1) ^ sr) & 1) << 31) |
         (sr >> 1);
    return (sr & 1);
}

DWORD WINAPI Random () {
    DWORD res = 0, loop;
    for (loop = 0; loop < 32; loop++)
        res = ((res<<1) | RandBit());
    return res;
}

