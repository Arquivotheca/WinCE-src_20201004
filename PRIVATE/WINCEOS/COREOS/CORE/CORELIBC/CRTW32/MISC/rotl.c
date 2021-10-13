//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <cmnintrin.h>

#pragma warning(disable:4163)
#pragma function(_rotl, _lrotl, _rotl64)

unsigned _rotl (unsigned val, int shift) {
	register unsigned hibit;	/* non-zero means hi bit set */
	register unsigned num = val;	/* number to rotate */
	shift &= 0x1f;			/* modulo 32 -- this will also make negative shifts work */
	while (shift--) {
		hibit = num & 0x80000000;  /* get high bit */
		num <<= 1;		/* shift left one bit */
		if (hibit)
			num |= 1;	/* set lo bit if hi bit was set */
	}

	return num;
}

unsigned long _lrotl(unsigned long val,	int shift) {
	return( (unsigned long) _rotl((unsigned) val, shift) );
}

unsigned __int64 _rotl64 (unsigned __int64 val, int shift) {
    unsigned __int64 hibit;                  /* non-zero means hi bit set */
    unsigned __int64 num = val;              /* number to rotate */
    shift &= 0x3f;               	   /* modulo 64 -- this will also make
                                     	  	negative shifts work */
    while (shift--) {
        hibit = num & 0x8000000000000000;  /* get high bit */
        num <<= 1;                         /* shift left one bit */
        if (hibit)
	        num |= 0x1;                        /* set lo bit if hi bit was set */
    }

    return num;
}
