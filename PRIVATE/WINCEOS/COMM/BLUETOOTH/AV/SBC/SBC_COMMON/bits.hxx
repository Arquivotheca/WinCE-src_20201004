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
// Access to bits in memory - header file
//
// In bytes the most significant bit goes first.
//

#ifndef _BITS_HXX_
#define _BITS_HXX_
//#include <windows.h>
// "getUI" returns an unsigned integer that is at the position "shift" starting from "address",
// and is "bits" (<32) bits long

unsigned getUI(void *address, unsigned shift, unsigned bits);
// "getUI" assigns "value" to an unsigned integer
// that is at the position "shift" starting from "address", and is "bits" (<=32) bits long
void setUI(void *address, unsigned shift, unsigned bits, unsigned value);

// The BitStream class provides access to memory as to a stream of bits.
// Position in a stream can only be positive.
class BitStream {
public:
	// BitStream class constructor
	// parameters:
	//     address - address in memory
	//     offset - offset in bits (>0)
	BitStream(void *address)
		: address(address), position(0) {}

	// Read next "bits" bits as an unsigned integer (bits <= 32)
	unsigned read(unsigned bits) {
		unsigned tmp = getUI(address, position, bits);
		position += bits;
		return tmp;
	}

	
	// Set next "bits" bits to "value" (bits < 32)
	void write(unsigned bits, unsigned value) {
		setUI(address, position, bits, value);
		position += bits;
	}

	// Go to position position
	void goTo(unsigned pos) {
		position = pos;
	}

	// Get an actual position
	unsigned getPosition() {
		return position;
	}
		
private:
	void *address;
	unsigned position;
};

#endif

