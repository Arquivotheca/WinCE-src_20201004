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
//
// Classes holding data for SBC codec
//

#ifndef _SBCSTRUCT_HXX_
#define _SBCSTRUCT_HXX_

#include <windows.h>
#include <sbc.hxx>

#include "bits.hxx"

#define SYNCWORD 0x9c
#pragma warning(disable: 4800)// class evaluating CRC according to "Advanced Audio Distribution Profile specification", 12.6.1.1



// sizes of fields
#define SIZE_SYNCWORD 8
#define SIZE_SAMPLING 2
#define SIZE_BLOCKS 2
#define SIZE_CHANNEL 2
#define SIZE_ALLOCATION 1
#define SIZE_SUBBANDS 1
#define SIZE_BITPOOL 8
#define SIZE_CRC 8
#define SIZE_JOIN 1
#define SIZE_RFA 1
#define SIZE_SCALE_FACTOR 4
#define SIZE_CRC_READ SIZE_SAMPLING + SIZE_BLOCKS + SIZE_CHANNEL \
                        + SIZE_ALLOCATION + SIZE_SUBBANDS + SIZE_BITPOOL




class CRC {
public:
	// constructor, crc - initial state of the register
	CRC(unsigned char crc = 0x0f) : crc(crc) {}
	// accept another bit
	void bit(bool b) {
		bool s = (crc >> 7);
		crc <<= 1;
		if ((b && !s) || (!b && s))
			crc ^= 0x1d;
	}
	void bits32(DWORD a) {
	    for(int i=0; i  < 32; i++)
	    {
            bool s = (crc >> 7);
            crc <<= 1;
            bool b = ((a&0x80000000) == 0x80000000);            
            if (((b) && !s) || ((!b) && s))
            {
                crc ^= 0x1d;
            }
            a <<= 1;
	    }
	}
	// get state of the register
	unsigned char getCRC() {return crc;}
	void setCRC(unsigned char newCRC){crc = newCRC;}
private:
	unsigned char crc;
};

// Class provides the SBC frame header
class FrameHeader {
public:
	FrameHeader(SBCWAVEFORMAT &format) : format(format), crc_check(0) {
		for(int i = 0; i < SBCSUBBANDS; ++i)
			join[i] = 1;
	};

	// writes the header to the stream
	void write(BitStream &stream) const;
    void writeJoint(BitStream &s) const;



	// sets whether the left and the right channel are joined 
	void setJoin(unsigned index, bool value) {
		join[index] = value;
	}



	// returns a crc value 
	unsigned char getCRC() const {
		return crc_check;
	}

	// evaluate crc for a header in the stream
    unsigned char evalCRCEncode(BitStream &s,BYTE pre_calc_crc) const;
#ifdef DECODE_ON
	// read the header from the stream
	bool read(BitStream &stream);
    unsigned char evalCRC(BitStream &s) const;
    	// gets information whether the left and the right channel are joined
	bool getJoin(unsigned index) const {
		return join[index];
	}
#endif
    unsigned char preEvalCRC(BitStream &s) const;

	// writes the crc value, stream is at the beginning of the header
	void writeCRC(BitStream &s, unsigned char crc) const;
private:
	SBCWAVEFORMAT &format;
	unsigned crc_check;
	bool join[SBC_MAX_SUBBANDS];
};

// scale factors
class ScaleFactors {
public:
	ScaleFactors(SBCWAVEFORMAT &format, FrameHeader &header)
		: format(format), header(header) {}

#ifdef DECODE_ON
	// reads scale factors
	void read(BitStream &s);
#endif
	// writes scale factors
	void write(BitStream &s) const;

	// access to the scale_factor values
	inline unsigned operator()(int ch, int sb) const {return scale_factor[ch][sb];}
	inline unsigned &operator()(int ch, int sb) {return scale_factor[ch][sb];}
	inline void AddOne(){
	    for(uint i = 0; i < SBCCHANNELS*SBCSUBBANDS; i++){
			((*scale_factor)[i])++;
		}
	}
private:
	SBCWAVEFORMAT &format;
	FrameHeader &header;
	unsigned scale_factor[SBCCHANNELS][SBCSUBBANDS];
};

#endif

