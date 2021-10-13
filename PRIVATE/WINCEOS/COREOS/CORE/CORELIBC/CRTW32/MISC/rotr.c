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
#pragma function(_rotr, _lrotr, _rotr64)

unsigned _rotr(unsigned val, int shift) {
	register unsigned lobit;	/* non-zero means lo bit set */
	register unsigned num = val;	/* number to rotate */
	shift &= 0x1f;			/* modulo 32 -- this will also make
					   negative shifts work */
	while (shift--) {
		lobit = num & 1;	/* get high bit */
		num >>= 1;		/* shift right one bit */
		if (lobit)
			num |= 0x80000000;  /* set hi bit if lo bit was set */
	}
	return num;
}

unsigned long _lrotr (unsigned long val, int shift) {
	return( (unsigned long) _rotr((unsigned) val, shift));
}

unsigned __int64 _rotr64 (unsigned __int64 val, int shift) {
    unsigned __int64 lobit;                     /* non-zero means lo bit set */
    unsigned __int64 num = val;                 /* number to rotate */
    shift &= 0x3f;   		       	   /* modulo 64 -- this will also make
 	                                         negative shifts work */
    while (shift--) {
        lobit = num & 0x1;             /* get low bit */
        num >>= 1;                     /* shift right one bit */
        if (lobit)
            num |= 0x8000000000000000; /* set hi bit if lo bit was set */
    }

    return num;
}
