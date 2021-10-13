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
//
// Subband Codec
// This header file defines constants and SBC version of the WAVEFORMATEX structure
// 

#ifndef _SBC_HXX_
#define _SBC_HXX_
#if !defined(UNDER_CE)
#include <assert.h>
#define ASSERT(c) assert(c)
#endif
#define SBC_MAX_SUBBANDS 8

#define SBCSUBBANDS 8

#include "mmreg.h"
#define DEBUG_SBC_ERROR 1
#define DEBUG_SBC_TRACE 0

#if !defined(UNDER_CE)
#define DEBUGMSG
#endif

// virtual sampling frequencies values (names in kilos)
#define SBC_SAMPL_FREQ_16 0
#define SBC_SAMPL_FREQ_32 1
#define SBC_SAMPL_FREQ_44_1 2
#define SBC_SAMPL_FREQ_48 3

// virtual block size values
#define SBC_BLOCK_SIZE_4 0
#define SBC_BLOCK_SIZE_8 1
#define SBC_BLOCK_SIZE_12 2
#define SBC_BLOCK_SIZE_16 3

// channel modes
#define SBC_CHANNEL_MODE_MONO 0
#define SBC_CHANNEL_MODE_DUAL 1
#define SBC_CHANNEL_MODE_STEREO 2
#define SBC_CHANNEL_MODE_JOINT 3

// allocation types
#define SBC_ALLOCATION_LOUDNESS 0
#define SBC_ALLOCATION_SNR 1

// subbands
#define SBC_SUBBANDS_4 0
#define SBC_SUBBANDS_8 1


#define SBCBLOCKS 16
#define SBCCHANNELS 2
#define SBCFREQUENCY 44100
#define SBCVIRTUALFREQUENCY SBC_SAMPL_FREQ_44_1
#define SBCALLOCATION SBC_ALLOCATION_LOUDNESS

// bits per sample (at the moment we only accept 16 bit PCM, but it would be easy to extend)
#define SBC_BITS_PER_SAMPLE 16

// size of extension
#define SBC_WFX_EXTRA_BYTES (sizeof(SBCWAVEFORMAT)-sizeof(WAVEFORMATEX))

// VERY IMPORTANT!!!: This is wave-audio format type of Subband Codec. 
// At the moment it is WAVE_FORMAT_DEVELOPMENT. If you want to use it for real
// you should register the codec, and get a number.
#define WAVE_FORMAT_SBC WAVE_FORMAT_DEVELOPMENT

// VERY IMPORTANT!!!: This is product id (wPid)
// At the moment it is 0, it's good for develoment, but if you want to use it for real
// you should be assigned a number.
#define  MM_MSFT_ACM_SBC 0

//#define DECODE_ON 0

// Extension to the standard WAVEFORMATEX ACM structure
struct SBCWAVEFORMAT {
    WAVEFORMATEX wfx;
    unsigned nBlocks; // number of blocks: 4, 8, 12, 16
    unsigned channel_mode; // channel mode: SBC_CHANNEL_MODE_???
    unsigned allocation_method; // allocation_method: SBC_ALLOCATION_????
    unsigned nSubbands; // number of subbands: 4 or 8
    unsigned bitpool; // bitpool, number of bits assigned to a channel or to a stream, it depends on the channel mode

    SBCWAVEFORMAT() {
        memset(this, 0, sizeof(SBCWAVEFORMAT));
        wfx.wFormatTag = WAVE_FORMAT_SBC;
        wfx.cbSize = SBC_WFX_EXTRA_BYTES;
    }

    // validate a structure
    bool verify() const {
        // check type
        if (WAVE_FORMAT_SBC != wfx.wFormatTag) return false;
        // check cbSize
        if (SBC_WFX_EXTRA_BYTES != wfx.cbSize) return false;
           // nBlocks must be validated (correct values are 4, 8, 12, and 16)
            if (0 != nBlocks % 4 || 1 > nBlocks / 4 || 4 < nBlocks / 4)
                return false;
        // check the number of channels and channel mode
        if (channel_mode > 3) return false;
        if (wfx.nChannels == 1) {
            if (channel_mode != SBC_CHANNEL_MODE_MONO && channel_mode != SBC_CHANNEL_MODE_DUAL)
                return false;
            } else if (wfx.nChannels == 2) {
            if (channel_mode != SBC_CHANNEL_MODE_STEREO && channel_mode != SBC_CHANNEL_MODE_JOINT)
                return false;
            } else {
                return false;
            }
         //  allocation_method must be validated
            if (0 != allocation_method && 1 != allocation_method)
                return false;
        // nSubbands must be validated (correct values are 4 and 8)
            if (4 != nSubbands && 8 != nSubbands)
                return false;
        // bitpool must be validated
            if (channel_mode == SBC_CHANNEL_MODE_MONO || channel_mode == SBC_CHANNEL_MODE_DUAL) {
            if (bitpool > 16u * nSubbands) 
                return false;
            } else {
            if (bitpool > 32u * nSubbands) 
                return false;
            }
        if (16000 != wfx.nSamplesPerSec && 32000 != wfx.nSamplesPerSec &&
             44100 != wfx.nSamplesPerSec && 48000 != wfx.nSamplesPerSec)
            return false;
        // wBitspersample validation
        if (SBC_BITS_PER_SAMPLE!= wfx.wBitsPerSample)
            return false;
        //  now verify that the block alignment is correct..
            if (blockAlignment() != wfx.nBlockAlign)
            return false;
         //  verify that avg bytes per second is correct
        if (averageBytesPerSec() != wfx.nAvgBytesPerSec)
            return false;

        return true;
    }
    // validate a structure
    bool verifyEncoder() const {
        //encode only supports certain modes
        if(verify()){
            if(SBCSUBBANDS != nSubbands)
                return false;
            if(wfx.nChannels != 2)
                return false;
                
            return true;
        }
        return false;
    }

    // evaluates a size of a single frame
    unsigned blockAlignment() const {
        unsigned result = 4 + (4 * nSubbands * wfx.nChannels)  / 8;
        unsigned td;
        if (channel_mode == SBC_CHANNEL_MODE_MONO || channel_mode == SBC_CHANNEL_MODE_DUAL) {
            td = nBlocks * wfx.nChannels * bitpool;
        } else {
            td = nBlocks * bitpool;
            if (channel_mode == SBC_CHANNEL_MODE_JOINT)
                td += nSubbands;
        }
        result += (td - 1) / 8 + 1;
        return result;
    }

    // evaluates a size of a PCM block corresponding to a single SBC frame
    unsigned pcmBlock() const {
        return 2 * wfx.nChannels * nSubbands * nBlocks;
    }

       // number of samples per second
    unsigned samplesPerSec() const {
        return wfx.nSamplesPerSec * wfx.nChannels;
    }

    // number of bytes per second in the stream
    unsigned averageBytesPerSec() const {
        return blockAlignment() * samplesPerSec() / (nSubbands * nBlocks * wfx.nChannels);
    }

    // number of samples in a frame
    unsigned samplesPerBlock() const {
        return wfx.nChannels * nBlocks * nSubbands;
    }

    // convert a frequency to a virtual value
    unsigned getVirtualFreq() const {
        switch (wfx.nSamplesPerSec) {
        case 16000:
            return SBC_SAMPL_FREQ_16;
        case 32000:
            return SBC_SAMPL_FREQ_32;
        case 44100:
            return SBC_SAMPL_FREQ_44_1;
        default:
            return SBC_SAMPL_FREQ_48;
        }
    }

    // set a virtual frequency
    void setVirtualFreq(unsigned val) {
        const unsigned conversion[] = {16000, 32000, 44100, 48000};
        wfx.nSamplesPerSec = conversion[val];
    }

    // return a virtual value for number of subbands
    unsigned getVirtualSubbands() const {
        if (nSubbands == 4)
            return SBC_SUBBANDS_4;
        else
            return SBC_SUBBANDS_8;
    }

    // set a virtual number of subbands
    void setVirtualSubbands(unsigned val) {
        const unsigned conversion[] = {4, 8};
        nSubbands = conversion[val];
    }

    // return virtual number of blocks
    unsigned getVirtualBlocks() const {
        switch (nBlocks) {
        case 4:
            return SBC_BLOCK_SIZE_4;
        case 8:
            return SBC_BLOCK_SIZE_8;
        case 12:
            return SBC_BLOCK_SIZE_12;
        default:
            return SBC_BLOCK_SIZE_16;
        }
    }

    // set a virtual number of blocks
    void setVirtualBlocks(unsigned val) {
        const unsigned conversion[] = {4, 8, 12, 16};
        nBlocks = conversion[val];
    }

    // set channel mode
    void setChannelMode(unsigned mode) {
        channel_mode = mode;
        wfx.nChannels = (mode == SBC_CHANNEL_MODE_MONO) ? 1 : 2;
    }

    // eavluate non-SBC codec values
    void evalNonstandard() {
        wfx.wFormatTag = WAVE_FORMAT_SBC;
        wfx.nAvgBytesPerSec = averageBytesPerSec();
        wfx.nBlockAlign = blockAlignment();
        wfx.wBitsPerSample = 16;
        wfx.cbSize = SBC_WFX_EXTRA_BYTES;
    }

    bool operator==(const SBCWAVEFORMAT &s) const {
        return memcmp(this, &s, sizeof(SBCWAVEFORMAT)) == 0;
    }

    bool operator!=(const SBCWAVEFORMAT &s) const {
        return !(*this == s);
    }
};

typedef SBCWAVEFORMAT FAR *LPSBCWAVEFORMAT;

#endif

