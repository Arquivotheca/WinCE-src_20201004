//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrtstorage.h>

void __cdecl srand(unsigned int seed) {
	crtGlob_t *pcrtg;
	if (pcrtg = GetCRTStorage()) {
		pcrtg->rand = (long)seed;
	}
}

int __cdecl rand(void) {
	crtGlob_t *pcrtg;
	if (pcrtg = GetCRTStorage()) {
		return(((pcrtg->rand = pcrtg->rand * 214013L + 2531011L) >> 16) & 0x7fff);
	}
	return 0;
}

