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
//
// Subband Codec
// Access to bits in memory -implementation
//
// Look at bits.hxx for description of the interface
//

#include "bits.hxx"
#include "utility.hxx"
#include <assert.h>
#include <windows.h>
typedef unsigned char BYTE;

static const unsigned BYTE_LEN = 8;
static const unsigned BYTES_PER_INT = 4;
static const unsigned BITS_PER_INT = (BYTES_PER_INT * BYTE_LEN);

// returns an unsigned integer of "len" consecutive set bits starting from the least significant
static inline unsigned mask(unsigned len) {
	return (unsigned) ((1 << len)-1);
}

unsigned getUI(void *address, unsigned shift, unsigned bits) {
	BYTE *a = (BYTE*)address;
	a += shift >> 3;
	shift &= 0x07; // optimized version of: shift%=8
	unsigned result = 0;
	while (bits) {
		unsigned nowBits = min(BYTE_LEN -shift, bits);
		unsigned int byte = *a;
		if (bits < BYTE_LEN - shift) {
			byte >>= BYTE_LEN - shift - bits;
		}
		bits -= nowBits;
		byte &= mask(nowBits);
		result |= byte << bits;
		// update variables for the next iteration
		shift = 0;
		//bits = left;
		++a;
	}
	return result;
}

void setUI(void *address, unsigned shift, unsigned bits, unsigned value) {
	BYTE *a = (BYTE*)address;
	a += shift >> 3;
	shift &= 0x07; // optimized version of: shift%=8
    while (bits) {
        unsigned nowBits = min((BYTE_LEN -shift), bits);
        unsigned left = bits - nowBits;
        BYTE m = mask(nowBits);
        unsigned v = (value >> left) & m;
        if (bits < BYTE_LEN -shift) {
            unsigned sh = BYTE_LEN -shift - bits;
            m <<= sh;
            v <<= sh;
        }
        *a &= (~m); //clearing bits from masked
        *a |= v; //setting new bits
        // update variables for the next iteration
        shift = 0;
        bits = left;
        ++a;		
    }
}

