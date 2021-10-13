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
// Classes providing synthesis and analysis - header file
// 

#ifndef _SYNTHESIZER_HXX_
#define _SYNTHESIZER_HXX_


#include <sbc.hxx>
#include <math.h>
//#define SBC_TEST_PRECISSION_LOSS
// GENERAL: 
// 1. Valid numbers of subbands are 4 and 8.
// 2. The length of subbands_samples and audio_samples arrays is always equal to a number of subbands.

// Synthesis
#ifdef DECODE_ON
class Synthesizer {
public:
    // initialize an object, correct numbers of subbands: 4 and 8
    void init(int subbands);
    // conduct synthesis
    void process(const float *subband_samples, short *audio_samples);
private:
    int subbands;
    float V[20 * SBCSUBBANDS];
    const float *D;
};
#endif
// Analysis

void process(const short *audio_samples, int *subband_samples, int &XStart, short * pX);
class Analyzer {

public:
    // initialize an object
    void init(int m_subbands);
    int m_XStart;
    short m_X[10 * SBCSUBBANDS];
protected:
    int m_subbands;

    int m_XEnd;

    const short * m_C;
    int m_C2[2*SBCSUBBANDS*SBCSUBBANDS];
    int m_XEndPlus1;

#ifdef SBC_TEST_PRECISSION_LOSS
public:
    void process_fullprecision(const short *audio_samples, double *subband_samples);
    double X[10 * SBC_MAX_SUBBANDS];
#endif
 
};

#endif

