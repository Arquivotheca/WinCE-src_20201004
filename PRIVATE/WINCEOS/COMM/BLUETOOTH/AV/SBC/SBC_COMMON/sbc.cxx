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
// Coding and decoding
//
// For more details read "Advanced Audio Distribution Profile specification", chapter 12
//

#include <stdlib.h>

#include "utility.hxx"
#include "bits.hxx"
#include "sbcstruct.hxx"
#include "subbands.hxx"
#include "stream.hxx"
#include <svsutil.hxx>

#include <windef.h>

#include <windows.h>
#include <stdlib.h>
#include <sbc.hxx>

//#define SBC_TEST_PRECISSION_LOSS
//#define SBC_USE_FLOAT_VALUES
//#define SBC_ARITHMETIC_SANITY_CHECK
//#ifdef SBC_USE_FLOAT_VALUES
//#ifndef SBC_TEST_PRECISSION_LOSS
//#define SBC_TEST_PRECISSION_LOSS
//#endif
//#endif

#include "subbands.hxx"
#include "sbc_quality_loss.hxx"

// Potential optimizations: precomputing cosinus values, 

#pragma warning(disable: 4305) // I know I am casting these proto structures to floats
// Errors
#define SBC_ERROR_SYNC_FAILED 1
#define SBC_ERROR_CRC_FAILED 2
#define SBC_ERROR_BROKEN_FRAME 3
#define SBC_ERROR_TOO_SHORT_BUFFER 4
#define SBC_ERROR_INCONSISTENT_STREAM 5
#define SBC_ERROR_OUT_OF_MEM 6
#define SBC_ERROR_TOO_SHORT_SAMPLE 7
#define SBC_ERROR_BROKEN_SAMPLE 8
#define SBC_ERROR_TOO_HIGH_BITPOOL 9

// Number of diffrent frequencies
#define FREQUENCIES 4

#ifdef DECODE_ON
// Offsets for four subbands
const int offset4[4][FREQUENCIES] ={
    -1,-2,-2,-2,
    0,0,0,0,
    0,0,0,0,
    0,1,1,1
};

// Offsets for eight subbands
const int offset8[8][FREQUENCIES] ={
    -2,-3,-4,-4,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,1,1,1,
    1,2,2,2
};
#endif 

const short offset8_44[8] = {-4, 0,0,0,0,0,1,2};
// auxiliary definition 
typedef SHORT TwoShorts[2];

// allocation of bits for channels from ch1 to ch2
// arguments:
//    * format [in] - format of the stream
//    * factors [in] - scale factors
//    * bits [out] - bits assignment
//    * ch1...ch2 [in] - range
void rangeBitAllocation(const SBCWAVEFORMAT &format, const ScaleFactors &factors,
    int (*bits)[SBCSUBBANDS], unsigned ch1, unsigned ch2) {

    // bitneed values derivation
    int bitneed[SBCCHANNELS][SBCSUBBANDS];
    int SBC_MAX_bitneed = 0;

    //for joint stereo and stero, know ch1 is == 0
#if (SBCALLOCATION == SBC_ALLOCATION_SNR)    
    for(unsigned ch = 0; ch <= 1; ++ch)
        for(unsigned sb = 0; sb < SBCSUBBANDS; ++sb)
        {
            bitneed[ch][sb] = factors(ch, sb);
            SBC_MAX_bitneed = max(SBC_MAX_bitneed, bitneed[ch][sb]);
        }
#else

    for(unsigned ch = 0; ch <= 1; ++ch)
        for(unsigned sb = 0; sb < SBCSUBBANDS; ++sb) {
            if (factors(ch, sb) == 0) {
                bitneed[ch][sb] = -5;
            } else {
                int loudness = factors(ch, sb) - offset8_44[sb];
                if (loudness > 0)
                    bitneed[ch][sb] = loudness >> 1;  // loudness / 2
                else
                    bitneed[ch][sb] = loudness;
            }
            SBC_MAX_bitneed = max(SBC_MAX_bitneed, bitneed[ch][sb]);
    }
#endif

    // how many bitslices?
    unsigned bitcount = 0;
    unsigned slicecount = 0;
    //int bitslice = SBC_MAX_bitneed + 1;
    int bitslicePlus1 = SBC_MAX_bitneed + 2;
    int bitslicePlus16 = SBC_MAX_bitneed + 17;

    do {
        --bitslicePlus1;
        --bitslicePlus16;
        bitcount += slicecount;
        slicecount = 0;
        for (unsigned ch = 0; ch <= 1; ++ch)
            for(unsigned sb = 0; sb < SBCSUBBANDS; ++sb) {
                if ((bitneed[ch][sb] > bitslicePlus1) && (bitneed[ch][sb] < bitslicePlus16))
                    ++slicecount;
                else if (bitneed[ch][sb] == bitslicePlus1)
                    slicecount += 2;
            }
    } while(bitcount + slicecount < format.bitpool);


    
    if (bitcount + slicecount == format.bitpool) {
        bitcount += slicecount;
        --bitslicePlus1;
        --bitslicePlus16;
        }
        
    int bitslice = bitslicePlus1-1;
    // distribution
    for (unsigned ch = 0; ch <= 1; ++ch)
        for(unsigned sb = 0; sb < SBCSUBBANDS; ++sb) {
            if (bitneed[ch][sb] < bitslice + 2)
                bits[ch][sb] = 0;
            else
                bits[ch][sb] = min(bitneed[ch][sb] - bitslice, 16);
        }
        
    // remaining bits
    for(unsigned ch = 0, sb = 0; bitcount < format.bitpool && sb < SBCSUBBANDS;) {
        if ((bits[ch][sb] >= 2) && (bits[ch][sb] < 16)) {
            ++bits[ch][sb];
            ++bitcount;
        } else if ((bitneed[ch][sb] == bitslicePlus1) && (format.bitpool > bitcount + 1)) {
            bits[ch][sb] = 2;
            bitcount += 2;
        }
        //ch ? (ch = 0, ++sb) : ++ch;
        if (ch == 1) {
            ch = 0;
            ++sb;
        } else {
            ch = 1;
        }
    }
    
    PREFAST_SUPPRESS(12008, "Bitpool is a know value smaller than ff");
    for(unsigned ch = 0, sb = 0; bitcount < format.bitpool && sb < SBCSUBBANDS;) {
        if (bits[ch][sb] < 16) {
            ++bits[ch][sb];
            ++bitcount;
        }
        if (ch == 1) {
            ch = 0;
            ++sb;
        } else {
            ++ch;
        }
    }
}

// allocation of bits
// arguments:
//    * format [in] - format of the stream
//    * factors [in] - scale factors
//    * bits [out] - bits assignment
void bitAllocation(const SBCWAVEFORMAT &format, const ScaleFactors &factors,
    int (*bits)[SBCSUBBANDS]) 
{
#if (SBCCHANNELS == 2)
    //case SBC_CHANNEL_MODE_STEREO:
    //case SBC_CHANNEL_MODE_JOINT:
    rangeBitAllocation(format, factors, bits, 0, 1);
#else
    switch (format.channel_mode) {
    case SBC_CHANNEL_MODE_DUAL:
        rangeBitAllocation(format, factors, bits, 1, 1);
    case SBC_CHANNEL_MODE_MONO:
        rangeBitAllocation(format, factors, bits, 0, 0);
        break;
    }
#endif
}
#ifdef DECODE_ON

// evaluation of scale factors
// arguments:
//    * format [in] - format of the stream
//    * factors [in] - scale factors read from the stream (logarithmic form)
//    * scalefactors [out] - real values
static void evalScalefactors(const SBCWAVEFORMAT &format,
    const ScaleFactors &factors, int (*scalefactor)[SBCSUBBANDS]) {
    const unsigned channels = format.wfx.nChannels;
    const unsigned subbands = format.nSubbands;
    for(unsigned ch = 0; ch < channels; ++ch)
        for(unsigned sb = 0; sb < subbands; ++sb)
            scalefactor[ch][sb] = 1 << (factors(ch, sb) + 1);
}

// read audio samples from the stream
// arguments:
//   * format [in] - format of the stream
//   * s [modified] - stream
//    * bits [in] - bits assignment
//    * audio_sample [out] - read audio values
static void readAudioSamples(const SBCWAVEFORMAT &format, BitStream &s,
    int (*bits)[SBCSUBBANDS],    
    unsigned (*audio_sample)[SBCCHANNELS][SBCSUBBANDS]) {
    const unsigned blocks = format.nBlocks;
    const unsigned channels = format.wfx.nChannels;
    const unsigned subbands = format.nSubbands;
    for(unsigned blk = 0; blk < blocks; ++blk)
        for(unsigned ch = 0; ch < channels; ++ch)
            for(unsigned sb = 0; sb < subbands; ++sb)
                audio_sample[blk][ch][sb] = s.read(bits[ch][sb]);
}


// reconstruction of subband samples
// arguments:
//    * format [in] - format of the stream
//    * header [in] - header of a frame
//    * bits [in] - bits assignment
//    * scalefactor [in] - real scalefactors
//    * audio_sample [in] - audio samples
//    * subband_samples [out] - subband samples
static void reconstructSubbandSamples(const SBCWAVEFORMAT &format,
    const FrameHeader &header,
    int (*bits)[SBCSUBBANDS],
    int (*scalefactor)[SBCSUBBANDS],
    unsigned (*audio_sample)[SBCCHANNELS][SBCSUBBANDS],
    float (*sb_sample)[SBCCHANNELS][SBCSUBBANDS]) {
    int levels[SBCCHANNELS][SBCSUBBANDS];
    const unsigned blocks = format.nBlocks;
    const unsigned channels = format.wfx.nChannels;
    const unsigned subbands = format.nSubbands;
    for(unsigned ch = 0; ch < channels; ++ch)
        for(unsigned sb = 0; sb < subbands; ++sb)
            levels[ch][sb] = (1 << bits[ch][sb]) - 1;
    for(unsigned blk = 0; blk < blocks; ++blk)
        for(unsigned ch = 0; ch < channels; ++ch)
            for(unsigned sb = 0; sb < subbands; ++sb)
                if (levels[ch][sb] > 0)
                    sb_sample[blk][ch][sb] = (float) (scalefactor[ch][sb]
                        * ((audio_sample[blk][ch][sb] * 2.0 + 1.0) / levels[ch][sb] - 1.0));
                else
                    sb_sample[blk][ch][sb] = 0;
    if (format.channel_mode == SBC_CHANNEL_MODE_JOINT) {
        for(unsigned blk = 0; blk < blocks; ++blk)
            for(unsigned sb = 0; sb < subbands; ++sb)
                if (header.getJoin(sb)) {
                    sb_sample[blk][0][sb] = sb_sample[blk][0][sb] + sb_sample[blk][1][sb];
                    sb_sample[blk][1][sb] = sb_sample[blk][0][sb] - 2 * sb_sample[blk][1][sb];
                }
    }
}

// synthesis
// arguments:
//    * format [in] - format of the stream
//    * synthesizers [modified] - synthesizers (number of synthesizers = number of channels)
//    * sb_sample [in] - subband samples
//    * output [out] - audio samples, array of shorts
static void synthesize(const SBCWAVEFORMAT &format,
    Synthesizer *synthesizers,
    float (*sb_sample)[SBCCHANNELS][SBCSUBBANDS],    
    void *output) {
    const unsigned blocks = format.nBlocks;
    const unsigned subbands = format.nSubbands;
    if (format.wfx.nChannels == 1) {
        short *out = (short*)output;
        for(unsigned blk = 0; blk < blocks; ++blk)
            synthesizers[0].process(sb_sample[blk][0], out + blk * subbands);
    } else { // 2 channels
        TwoShorts *out = (TwoShorts*)output;
        short tmp[SBCSUBBANDS];
        for(unsigned blk = 0; blk < blocks; ++blk)
            for(unsigned ch = 0; ch < 2; ++ch) {
                synthesizers[ch].process(sb_sample[blk][ch], tmp);
                for(unsigned sb = 0; sb < subbands; ++sb)
                    out[blk * subbands + sb][ch] = tmp[sb];
            }
    }
}

#endif


// evaluate a scale factor
// arguments:
//    * value [in] - a value for which a scale factor should be evaluated
//    * scalefactor [out] - scale factor, normal form
//    * scale_factor [out] - scale factor, logarithmic form
inline void singleScaleFactor(unsigned value, unsigned &scale_factor) {
//no further precision loss on this func

    if(value <= 2)
    {
        scale_factor = 0; 
        return;
    }

    uint shift; 

    //Get the log base 2 minus 1 of the smallest 
    //power of 2 that is equal or greater to value.
    //This is equivalent to finding the log base 2
    //of value, unless value is a power of 2.
    //The subtraction below makes sure if that 
    //is the case the correct value is found. 
    
    //in case already a power of 2
    value--;    
    scale_factor = 0;
    shift = ( ( value & 0xFFFF0000 ) != 0 ) << 4; value >>= shift; scale_factor |= shift;
    shift = ( ( value & 0xFF00     ) != 0 ) << 3; value >>= shift; scale_factor |= shift;
    shift = ( ( value & 0xF0       ) != 0 ) << 2; value >>= shift; scale_factor |= shift;
    shift = ( ( value & 0xC        ) != 0 ) << 1; value >>= shift; scale_factor |= shift;
    shift = ( ( value & 0x2        ) != 0 ) << 0; value >>= shift; scale_factor |= shift;

#ifdef SBC_ARITHMETIC_SANITY_CHECK
    int myScaleFactor, myScale_Factor;
    for( myScaleFactor = 2,  myScale_Factor = 0; myScaleFactor < (int)value; myScaleFactor *= 2, myScale_Factor++)
    {;}
    ASSERT(myScale_Factor == (int)scale_factor);
    ASSERT(myScaleFactor > 0 && myScale_Factor > 0);
    ASSERT((int)value > 0);
#endif
}
// evaluate scale factors for encoding
// arguments:
//    * subbandsSample [in] - subband samples
//    * format [in] - format of the stream
//    * header [out] - header of the stream (join values are set if joint stero mode)
//    * scale_factors [out] - scale factors, logarithmic form
//    * scalefactor [out] - scale factors, normal form

//PRECISION LOSS:
//There is essentially no precision loss from running this function 
//using integer arithmetic (unless the factors are larger than
//MAX_UINT, but this does not happen with normal audio 
//input to this function)
// 
//This function finds the next power of 2 of the values in pppSubbandsSample,
//so there is no precision gain from using float arithmetic
//
//If using floating point arithmetic elsewhere, it is important to 
//pass this function the ceiling (for positive numbers) or the 
//floor (negative numbers), and not just a simple int cast
//as this might cause this function to find a factor that is too
//low, causing major quality degradation in the rest of the arithmetic.
//If using ceil and floor is too expensive, an option in to locally
//always add one to MaxAbs value that is used, as that will almost
//always reproduce the effect of ceil/floor, unless the value was
//already a precise power of 2. A larger scale factor than necessary
//causes the compression to be slighly less optimal, but a 
//smaller scale factor than necessary can cause major quality issues. 

inline void evalEncodingScalefactors(
                int (*pppSubbandsSample)[SBCCHANNELS][SBCSUBBANDS],
                const SBCWAVEFORMAT  &format, 
                FrameHeader &header,
                ScaleFactors &scale_factors
                ) 
{
    if(SBC_CHANNEL_MODE_JOINT == format.channel_mode){
        int pppJointSubbandSamples[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
        for(int sb = 0; sb < SBCSUBBANDS; ++sb) {
            unsigned maxAbs0 = 0;
            unsigned maxAbs1 = 0;
            unsigned maxSum = 0;
            unsigned maxDiff = 0;
            for(int blk = 0; blk < SBCBLOCKS; ++blk) {
                maxAbs0 = max(maxAbs0, (unsigned) (abs(pppSubbandsSample[blk][0][sb])));
                maxAbs1 = max(maxAbs1, (unsigned) (abs(pppSubbandsSample[blk][1][sb])));

                pppJointSubbandSamples[blk][0][sb] = (pppSubbandsSample[blk][0][sb]+pppSubbandsSample[blk][1][sb])>>1;
                pppJointSubbandSamples[blk][1][sb] = (pppSubbandsSample[blk][0][sb]-pppSubbandsSample[blk][1][sb])>>1;
                
                maxSum = max(maxSum, (unsigned)abs(pppJointSubbandSamples[blk][0][sb]));
                maxDiff = max(maxDiff, (unsigned)abs(pppJointSubbandSamples[blk][1][sb]));
            }
            
            if ((sb < SBCSUBBANDS - 1) && ((maxAbs0 + maxAbs1) > (maxSum + maxDiff))){

                for(int blk = 0; blk < SBCBLOCKS; ++blk) {
                    pppSubbandsSample[blk][0][sb] = pppJointSubbandSamples[blk][0][sb];
                    pppSubbandsSample[blk][1][sb] = pppJointSubbandSamples[blk][1][sb];
                }
                singleScaleFactor(maxSum, scale_factors(0, sb));
                singleScaleFactor(maxDiff, scale_factors(1, sb));

            } else {
                header.setJoin(sb, 0);
                singleScaleFactor(maxAbs0, scale_factors(0, sb));
                singleScaleFactor(maxAbs1, scale_factors(1, sb));
            }
        }
    }
    else{ 
        if(SBC_CHANNEL_MODE_STEREO != format.channel_mode){
            for(int sb = 0; sb < SBCSUBBANDS; ++sb)
                for(int ch = 0; ch < SBCCHANNELS; ++ch) {
                    unsigned maxAbs = 0;
                    for(int blk = 0; blk < SBCBLOCKS; ++blk)
                        maxAbs = max(maxAbs, (unsigned)abs(pppSubbandsSample[blk][ch][sb]));
                    singleScaleFactor(maxAbs, scale_factors(ch, sb));
                }
        }
        else{
            for(int sb = 0; sb < SBCSUBBANDS; ++sb){
                    unsigned maxAbs0 = 0;
                    unsigned maxAbs1 = 0;    

                    for(int blk = 0; blk < SBCBLOCKS; ++blk)
                    {
                        maxAbs0 = max(maxAbs0, (unsigned)abs(pppSubbandsSample[blk][0][sb]));
                        maxAbs1 = max(maxAbs1, (unsigned)abs(pppSubbandsSample[blk][1][sb]));
                    }
                    singleScaleFactor(maxAbs0, scale_factors(0, sb));
                    singleScaleFactor(maxAbs1, scale_factors(1, sb));
            }
        }
    }
}

// conversion of subband samples to the streaming form
// arguments:
//    * format [in] - format of the stream
//    * subbandsSample [in] - subbands samples
//    * scalefactor [in] - scale factors, normal form
//    * bits [in] - assigned numbers of bits
//    * quantized_sb_sample [out] - quantized subband samples
//    TODO - alters factors, fix it!

inline void quantize(
    const SBCWAVEFORMAT &format, 
    int (*pppSubbandsSample)[SBCCHANNELS][SBCSUBBANDS],
    ScaleFactors & factors,
    int (*bits)[SBCSUBBANDS], 
    unsigned (*quantized_sb_sample)[SBCCHANNELS][SBCSUBBANDS],
    double (*double_pppSubbandsSample)[SBCCHANNELS][SBCSUBBANDS] = NULL
    ) 
{
    int mul;
    unsigned levels[SBCCHANNELS][SBCSUBBANDS];
    unsigned levels_divider[SBCCHANNELS][SBCSUBBANDS];
#ifdef SBC_TEST_PRECISSION_LOSS
    SbcTestPrecisionLoss_t FunctionPrecionLoss(TEXT("quantize:overall function"));
    double double_quantized_sb_sample[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
    double dlevels[SBCCHANNELS][SBCSUBBANDS];
    bool Overflow;

 #endif
    factors.AddOne();
 
    for(unsigned sb = 0; sb < 16; ++sb)
        {

        ASSERT((*bits)[sb] <= 16); //bits is at most 16
        if((*bits)[sb] <= 15)
            {        
            (*levels_divider)[sb] = (15 - ((*bits)[sb]));
            (*levels)[sb] = (((1 << ((*bits)[sb])))-1) << (*levels_divider)[sb];
            (*levels_divider)[sb]++;
            }
        else
            {
            //2^15 - 1 
            (*levels)[sb] = 32767;
            (*levels_divider)[sb] = 0;
            }

#ifdef SBC_TEST_PRECISSION_LOSS
        (*dlevels)[sb] = pow(2, (*bits)[sb]) - 1;
#endif

    }

    
    for(unsigned blk = 0; blk < SBCBLOCKS; ++blk)
        for(unsigned ch = 0; ch < SBCCHANNELS; ++ch)
            for(unsigned sb = 0; sb < SBCSUBBANDS; ++sb)
                {
                //POTENTIAL quality loss
                mul = ((int)(pppSubbandsSample[blk][ch][sb] * (int)levels[ch][sb]))>>((int)factors(ch,sb));
                quantized_sb_sample[blk][ch][sb] = ((mul + levels[ch][sb]) >> (int)levels_divider[ch][sb]);

#ifdef SBC_TEST_PRECISSION_LOSS
                int test;
                Overflow = !safeIntSMul(pppSubbandsSample[blk][ch][sb], levels[ch][sb], &test);
                Overflow |= !safeIntSAdd(mul, levels[ch][sb], &test);
                if(Overflow)
                {
                    DEBUGMSG(1, (TEXT("Overflow! pppSubbandsSample[blk][ch][sb] [%d], levels[ch][sb] [%d] bits [%d]"),
                    pppSubbandsSample[blk][ch][sb], levels[ch][sb], bits[ch][sb]));
                }
                
                double dmul = ((pppSubbandsSample[blk][ch][sb] * dlevels[ch][sb]))/(ppScaleFactor[ch][sb]);
                double_quantized_sb_sample[blk][ch][sb] = floor((dmul + dlevels[ch][sb])/2);
                FunctionPrecionLoss.CompareValues(double_quantized_sb_sample[blk][ch][sb], quantized_sb_sample[blk][ch][sb], Overflow);
                if(double_pppSubbandsSample)
                    {
                    double_quantized_sb_sample[blk][ch][sb] = floor(((double_pppSubbandsSample[blk][ch][sb] /  ppScaleFactor[ch][sb] + 1.0)*dlevels[ch][sb])/2.0);   //dmul + dlevels[ch][sb])/2);
                    //TODO: global error
                    //FunctionPrecionLoss.CompareValues(double_quantized_sb_sample[blk][ch][sb], quantized_sb_sample[blk][ch][sb], Overflow);
                    }

 #endif

 #ifdef SBC_USE_FLOAT_VALUES
                quantized_sb_sample[blk][ch][sb] = (uint) double_quantized_sb_sample[blk][ch][sb];
 #endif
                }

#ifdef SBC_TEST_PRECISSION_LOSS
    FunctionPrecionLoss.PrintErrorVals();
#endif
    
}
BYTE f_pre_calc_crc = 0xf0;
BYTE master_header[5];

int encodeFrame(LPSTREAMINSTANCE h, unsigned inputSamples, LPVOID address, LPVOID output, void const * globalPrecisionLoss) {
    short *pRawInput = (short*)address;
    const int frameSampleSize = /*sizeof(short)*/ 2 * SBCCHANNELS* SBCBLOCKS* SBCSUBBANDS;

    // Encode the buffer in subband samples (analysis)
    //note: must always pass an entire frame, otherwise will break!
    int pppSubbandsSample[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
    for(int blk = 0; blk < SBCBLOCKS; ++blk) {
        int BlkBySubbandsBy2 = SBCSUBBANDS * blk*2;
        process(&pRawInput[BlkBySubbandsBy2], pppSubbandsSample[blk][0],h->analyzer[0].m_XStart, h->analyzer[0].m_X);
        process(&pRawInput[1+BlkBySubbandsBy2], pppSubbandsSample[blk][1],h->analyzer[1].m_XStart, h->analyzer[1].m_X);

    }
#ifdef SBC_TEST_PRECISSION_LOSS
    short input[SBCCHANNELS][SBCSUBBANDS * SBCBLOCKS];
    double subbandsSample_fullprecision[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];

    int allSamples = SBCBLOCKS * SBCSUBBANDS;
    for(int i = 0; i < allSamples; ++i)
        for(int ch = 0; ch < SBCCHANNELS; ++ch) {
            unsigned index = i * SBCCHANNELS + ch;
            input[ch][i] = (index < inputSamples) ? pRawInput[index] : 0;
        }
    for(int blk = 0; blk < SBCBLOCKS; ++blk)
        for(int ch = 0; ch < SBCCHANNELS; ++ch)
            h->analyzer[ch].process_fullprecision(&input[ch][SBCSUBBANDS * blk], subbandsSample_fullprecision[blk][ch]);

    SbcTestPrecisionLoss_t LocalPrecisionLoss1 (TEXT("encodeFrame:")); 
    for(int blk = 0; blk < SBCBLOCKS; ++blk)
        for(int ch = 0; ch < SBCCHANNELS; ++ch)
            for(int sb= 0; sb < SBCSUBBANDS; ++sb)
            {
                LocalPrecisionLoss1.CompareValues(subbandsSample_fullprecision[blk][ch][sb], pppSubbandsSample[blk][ch][sb], false);
            
#ifdef SBC_USE_FLOAT_VALUES
                pppSubbandsSample[blk][ch][sb] = subbandsSample_fullprecision[blk][ch][sb] > 0 ? 
                                                 (int) ceil(subbandsSample_fullprecision[blk][ch][sb]) :
                                                 (int) floor(subbandsSample_fullprecision[blk][ch][sb]);
#endif
            }
    LocalPrecisionLoss1.PrintErrorVals();
     
#endif 
    // Apply scale factors to subband samples
    //precision loss here
    FrameHeader header(h->format);
    ScaleFactors factors(h->format, header);
    evalEncodingScalefactors(pppSubbandsSample, h->format, header, factors);
    
    // Do bit allocation
    int bits[SBCCHANNELS][SBCSUBBANDS];
    bitAllocation(h->format, factors, bits);
    // Convert from subband samples to streaming form
    unsigned quantized_sb_sample[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
    memcpy(output, master_header, 4);

    // Write the frame to the output buffer
    BitStream s((BYTE*)output+4);

    if(SBC_CHANNEL_MODE_JOINT == h->format.channel_mode){
        header.writeJoint(s);
    }

    factors.write(s);

#ifdef SBC_USE_FLOAT_VALUES
    quantize(h->format, pppSubbandsSample, factors, bits, quantized_sb_sample, subbandsSample_fullprecision);
#else
    quantize(h->format, pppSubbandsSample, factors, bits, quantized_sb_sample);
#endif

    
    header.writeCRC(BitStream(output), header.evalCRCEncode(BitStream(output),f_pre_calc_crc));
    for(int blk = 0; blk < SBCBLOCKS; ++blk)
        for(int ch = 0; ch < SBCCHANNELS; ++ch)
            for(int sb = 0; sb < SBCSUBBANDS; ++sb)
                s.write(bits[ch][sb], quantized_sb_sample[blk][ch][sb]);
    while((s.getPosition() & 0x7) != 0)
        s.write(1,0);
    return 0;
}

void sbcEncodeReset(LPSTREAMINSTANCE psi,SBCWAVEFORMAT *format) {
    if (format)
    {
        psi->format = *format;
        for(int i = 0; i < psi->format.wfx.nChannels; ++i)
            psi->analyzer[i].init(psi->format.nSubbands);

        FrameHeader header(*format);
        BitStream s(master_header);
        header.write(s);
    
        //pre calc CRC  
        f_pre_calc_crc = header.preEvalCRC(BitStream(master_header));
    }
}

LRESULT FNGLOBAL sbcEncode(LPACMDRVSTREAMINSTANCE padsi, LPACMDRVSTREAMHEADER padsh) {
    LPSTREAMINSTANCE psi = (PSTREAMINSTANCE)(UINT)padsi->dwDriver;
    if (0 != (ACM_STREAMCONVERTF_START & padsh->fdwConvert))
        sbcEncodeReset(psi);
    bool fBlockAlign = (0 != (ACM_STREAMCONVERTF_BLOCKALIGN & padsh->fdwConvert));
    DEBUGMSG(DEBUG_SBC_TRACE, (TEXT("SBC: sbcEncode: fBlockAlign: "), fBlockAlign ? TEXT("TRUE\r\n") : TEXT("FALSE\r\n")));
    
    unsigned samples = padsh->cbSrcLength >> 1; //assume sample size == 16 bits
    unsigned spb = psi->format.samplesPerBlock();
    unsigned nBlocks = samples / spb;
    if (!fBlockAlign && 0 != samples % spb)
        nBlocks++;
    unsigned needed = nBlocks * psi->format.blockAlignment();
    if (needed > padsh->cbDstLength || needed < nBlocks || needed < psi->format.blockAlignment() )
    {
        ASSERT(0);
        return (ACMERR_NOTPOSSIBLE);
        }
    padsh->cbDstLengthUsed = needed;
    if (fBlockAlign) {
        padsh->cbSrcLengthUsed = spb * 2;
    } else {
        padsh->cbSrcLengthUsed = padsh->cbSrcLength;
    }

    LPBYTE pSrc = padsh->pbSrc;
    LPBYTE pDst = padsh->pbDst;

    for(unsigned frame = 0; frame < nBlocks; ++frame) {
        if (encodeFrame(psi, min<unsigned>(samples, spb), pSrc, pDst, NULL))
            return MMSYSERR_ERROR;
        pSrc += spb * 2;
        pDst += psi->format.wfx.nBlockAlign;
        samples -= min<unsigned>(samples, spb);
    }
#ifdef SBC_TEST_PRECISSION_LOSS
    SbcTestPrecisionLoss_t::PrintOverallErrorVals();
#endif
    return MMSYSERR_NOERROR;
}

// decoding
#ifdef DECODE_ON
static int decodeFrame(LPSTREAMINSTANCE h, LPVOID address, LPVOID output) {
    BitStream s(address);
    // reading header
    SBCWAVEFORMAT format;
    FrameHeader header(format);
    if (!header.read(s)) return SBC_ERROR_SYNC_FAILED;
    // consistency test
    if (format != h->format)
        return SBC_ERROR_INCONSISTENT_STREAM;
    // reading factors
    ScaleFactors factors(format, header);
    factors.read(s);
    // crc test
    if (header.evalCRC(BitStream(address)) != header.getCRC())
        return SBC_ERROR_CRC_FAILED;
    // allocating bits
    int bits[SBCCHANNELS][SBCSUBBANDS];
    bitAllocation(format, factors, bits);
    // reading audio samples
    unsigned audio_sample[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
    readAudioSamples(format, s, bits, audio_sample);
    // evaluating scale factors
    int scalefactor[SBCCHANNELS][SBCSUBBANDS];
    evalScalefactors(format, factors, scalefactor);
    // subband samples reconstruction
    float sb_sample[SBCBLOCKS][SBCCHANNELS][SBCSUBBANDS];
    reconstructSubbandSamples(format, header, bits, scalefactor, audio_sample, sb_sample);
    // synthesis
    synthesize(format, h->synthesizer, sb_sample, output);
    return 0;
}

void sbcDecodeReset(LPSTREAMINSTANCE psi, const SBCWAVEFORMAT *format) {
    if (format)
        psi->format = *format;
    for(int i = 0; i < psi->format.wfx.nChannels; ++i)
        psi->synthesizer[i].init(psi->format.nSubbands);
}

LRESULT FNGLOBAL sbcDecode(LPACMDRVSTREAMINSTANCE padsi, LPACMDRVSTREAMHEADER padsh) {
    LPSTREAMINSTANCE psi = (PSTREAMINSTANCE)(UINT)padsi->dwDriver;
    if (0 != (ACM_STREAMCONVERTF_START & padsh->fdwConvert))
        sbcDecodeReset(psi);
    bool fBlockAlign = (0 != (ACM_STREAMCONVERTF_BLOCKALIGN & padsh->fdwConvert));
    unsigned srcBlockSize = psi->format.blockAlignment();
    unsigned dstBlockSize = psi->format.samplesPerBlock() * 2;
    unsigned nBlocks = padsh->cbSrcLength / srcBlockSize;

    if (fBlockAlign && (padsh->cbSrcLength % srcBlockSize != 0))
        return MMSYSERR_ERROR;
    unsigned needed = nBlocks * dstBlockSize;
    if (needed > padsh->cbDstLength)
        return (ACMERR_NOTPOSSIBLE);
    padsh->cbDstLengthUsed = needed;
    padsh->cbSrcLengthUsed = nBlocks * srcBlockSize;
    // decode frames
    LPBYTE pSrc = padsh->pbSrc;
    LPBYTE pDst = padsh->pbDst;
    for(unsigned frame = 0; frame < nBlocks; ++frame) {
        if (decodeFrame(psi, pSrc, pDst))
            return MMSYSERR_ERROR;
        pSrc += srcBlockSize;
        pDst += dstBlockSize;
    }
    return MMSYSERR_NOERROR;
}


#endif
///////////////////////////////subbands.cxx
//
// Subband Codec
// Classes providing a synthesis and an analysis - implementation
// Description of the interface in subbands.hxx
// 
// For more details consult "Advanced Audio Distribution profile specification", 12.6.6 & 12.7.1
//


const short Proto_8[74] = {
1, 3, 4, 6, 9, 12, 14,
46, 66, 85, 104, 120, 130, 133, 125,
557, 680, 799, 911, 1010, 1091, 1153, 1191,
-557, -435, -320, -214, -120, -40, 24, 72,
-46, -28, -13, -1, 7, 13, 16, 17,

16, 17, 16, 13, 1, 13, 28,
106, 72, 24, -40, 214, 320, 435,
1204, 1191, 1153, 1091, 911, 799, 680,
106, 125, 133, 130, 104, 85, 66,
16, 14, 12, 9, 4, 3
};


#if (DECODE_ON || SBC_TEST_PRECISSION_LOSS)

const float Proto_4_f[40] = {
    0.00000000E+00, 5.36548976E-04, 1.49188357E-03, 2.73370904E-03,
    3.83720193E-03, 3.89205149E-03, 1.86581691E-03, -3.06012286E-03,
    1.09137620E-02, 2.04385087E-02, 2.88757392E-02, 3.21939290E-02,
    2.58767811E-02, 6.13245186E-03, -2.88217274E-02, -7.76463494E-02,
    1.35593274E-01, 1.94987841E-01, 2.46636662E-01, 2.81828203E-01,
    2.94315332E-01, 2.81828203E-01, 2.46636662E-01, 1.94987841E-01,
    -1.35593274E-01, -7.76463494E-02, -2.88217274E-02, 6.13245186E-03,
    2.58767811E-02, 3.21939290E-02, 2.88757392E-02, 2.04385087E-02,
    -1.09137620E-02, -3.06012286E-03, 1.86581691E-03, 3.89205149E-03,
    3.83720193E-03, 2.73370904E-03, 1.49188357E-03, 5.36548976E-04,
};

// filter coefficients for eight subbands


const float Proto_8_f[80] = {
    0.00000000E+00, 1.56575398E-04, 3.43256425E-04, 5.54620202E-04,
    8.23919506E-04, 1.13992507E-03, 1.47640169E-03, 1.78371725E-03,
    2.01182542E-03, 2.10371989E-03, 1.99454554E-03, 1.61656283E-03,
    9.02154502E-04, -1.78805361E-04, -1.64973098E-03, -3.49717454E-03,
    5.65949473E-03, 8.02941163E-03, 1.04584443E-02, 1.27472335E-02,
    1.46525263E-02, 1.59045603E-02, 1.62208471E-02, 1.53184106E-02,
    1.29371806E-02, 8.85757540E-03, 2.92408442E-03, -4.91578024E-03,
    -1.46404076E-02, -2.61098752E-02, -3.90751381E-02, -5.31873032E-02,
    6.79989431E-02, 8.29847578E-02, 9.75753918E-02, 1.11196689E-01,
    1.23264548E-01, 1.33264415E-01, 1.40753505E-01, 1.45389847E-01,
    1.46955068E-01, 1.45389847E-01, 1.40753505E-01, 1.33264415E-01,
    1.23264548E-01, 1.11196689E-01, 9.75753918E-02, 8.29847578E-02,
    -6.79989431E-02, -5.31873032E-02, -3.90751381E-02, -2.61098752E-02,
    -1.46404076E-02, -4.91578024E-03, 2.92408442E-03, 8.85757540E-03,
    1.29371806E-02, 1.53184106E-02, 1.62208471E-02, 1.59045603E-02,
    1.46525263E-02, 1.27472335E-02, 1.04584443E-02, 8.02941163E-03,
    -5.65949473E-03, -3.49717454E-03, -1.64973098E-03, -1.78805361E-04,
    9.02154502E-04, 1.61656283E-03, 1.99454554E-03, 2.10371989E-03,
    2.01182542E-03, 1.78371725E-03, 1.47640169E-03, 1.13992507E-03,
    8.23919506E-04, 5.54620202E-04, 3.43256425E-04, 1.56575398E-04,
};

// The number pi.
static const float PI = (float) atan(1) * 4;
#endif
#ifdef DECODE_ON
// The functions below implement the interface in a header file.


void Synthesizer::init(int sbs) {
    subbands = sbs;
    for(int i = 0; i < sbs *20; ++i)
        V[i] = 0;
    D = (subbands == 4) ? Proto_4_f : Proto_8_f;    
}

void Synthesizer::process(const float *subband_samples, short *audio_samples) {
    // Shifting
    for(int i = subbands*20 - 1; i >= 2 * subbands; --i)
        V[i] = V[i - 2 * subbands];
    // Matrixing
    for(int k = 0; k < 2 * subbands; ++k) {
        V[k] = 0;
        for(int i = 0; i < subbands; ++i)
            V[k] += subband_samples[i] * (float) cos((i + 0.5) * (k + subbands/2) * PI / subbands);
    }
    // Build a vector U
    float U[SBCSUBBANDS * 10];
    for(int i = 0; i <= 4; ++i)
        for(int j = 0; j < subbands; ++j) {
            U[i * 2 * subbands + j] = V[i * 4 * subbands + j];
            U[(i * 2 + 1) * subbands + j] = V[(i * 4 + 3) * subbands + j];
        }
    // Window by coefficients
    for(int i = 0; i < 10 * subbands; ++i)
        U[i] *= -subbands * D[i];
    // Calculate audio samples
    for(int j = 0; j < subbands; ++j) {
        float sample = 0;
        for(int i = 0; i < 10; ++i)
            sample += U[j+subbands*i];
        audio_samples[j] = (short)sample;
    }
}
#endif
#ifdef SBC_TEST_PRECISSION_LOSS

void Analyzer::process_fullprecision(const short *audio_samples, double *subband_samples) {
	// Shifting and input
	for(int i = 10 * SBCSUBBANDS - 1; i >= SBCSUBBANDS; --i)
		X[i] = X[i - SBCSUBBANDS];
	for(int i = SBCSUBBANDS - 1; i >= 0; --i)
		X[i] = audio_samples[SBCSUBBANDS - i - 1];
	// windowing and partial calculation
	double Y[2 * SBC_MAX_SUBBANDS];
	for(int i = 0; i < 2 * SBCSUBBANDS; ++i) {
		Y[i] = 0;
		for(int k = 0; k <= 4; ++k)
			Y[i] += X[i + k * 2 * SBCSUBBANDS] * Proto_8_f[i + k * 2 * SBCSUBBANDS];
	}
	// calculate subbands samples
	for(int i = 0; i < SBCSUBBANDS; ++i) {
		double s = 0;
		for(int k = 0; k < 2 * SBCSUBBANDS; ++k)
			s += Y[k] * cos((i + 0.5) * (k - SBCSUBBANDS /2) * PI / SBCSUBBANDS);
		subband_samples[i] = s;
	}
}
#endif
void Analyzer::init(int sbs) {
    /*Look up table for the analyzer*/

    m_subbands = sbs;
    ASSERT(m_subbands == SBCSUBBANDS);//subbands are now set at compile time
    
    m_subbands = SBCSUBBANDS;

    m_XStart = m_subbands * 10 - 1;
    m_XEndPlus1 = m_subbands * 10;
    
    for(int i = 0; i < m_subbands *10; ++i)
        m_X[i] = 0;
    m_C = Proto_8;  

#ifdef SBC_TEST_PRECISSION_LOSS
    for(int i = 0; i < SBCSUBBANDS *10; ++i)
        X[i] = 0;
#endif
}

const unsigned char indexes[10][10] =
{
  { 0, 16, 32, 48, 64,     8, 24, 40, 56, 72},
  { 8, 24, 40, 56, 72,    16, 32, 48, 64,  0},
  
  {16, 32, 48, 64,  0,    24, 40, 56, 72,  8},
  {24, 40, 56, 72,  8,    32, 48, 64,  0, 16},

  {32, 48, 64,  0, 16,    40, 56, 72,  8, 24},
  {40, 56, 72,  8, 24,    48, 64,  0, 16, 32},

  {48, 64,  0, 16, 32,    56, 72,  8, 24, 40},
  {56, 72,  8, 24, 40,    64,  0, 16, 32, 48},

  {64,  0, 16, 32, 48,    72,  8, 24, 40, 56},
  {72,  8, 24, 40, 56,     0, 16, 32, 48, 64},

  };

/////////////sbcstruct.cxx
#include "utility.hxx"
#include "sbcstruct.hxx"



void FrameHeader::write(BitStream &s) const {
    s.write(SIZE_SYNCWORD, SYNCWORD);
    s.write(SIZE_SAMPLING, format.getVirtualFreq());
    s.write(SIZE_BLOCKS, format.getVirtualBlocks());
    s.write(SIZE_CHANNEL, format.channel_mode);
    s.write(SIZE_ALLOCATION, format.allocation_method);
    s.write(SIZE_SUBBANDS, format.getVirtualSubbands());
    s.write(SIZE_BITPOOL, format.bitpool);
    s.write(SIZE_CRC, crc_check);
}

void FrameHeader::writeJoint(BitStream &s) const {
    //caller is responsible for only 
    //calling this function for joint

    for(unsigned i = 0; i < SBCSUBBANDS-1; ++i) {
         s.write(SIZE_JOIN, join[i]);
    }
    #if 0
    static int debug_counter = 0; //avoid excessive debug out

    if((join[0] || join[1]|| join[2]|| join[3]|| join[4]|| join[5]|| join[6]) && debug_counter < 100){
        debug_counter++;
        DEBUGMSG(1, (TEXT("Joint [%x-%x-%x-%x-%x-%x-%x ]"), join[0], join[1], join[2], join[3], join[4], join[5], join[6]));
    }
    #endif    

    s.write(SIZE_RFA, 0);
}
#ifdef DECODE_ON

bool FrameHeader::read(BitStream &s) {
    format.wfx.wFormatTag = WAVE_FORMAT_SBC;
    if (s.read(SIZE_SYNCWORD) != SYNCWORD)
        return false;
    format.setVirtualFreq(s.read(SIZE_SAMPLING));
    format.setVirtualBlocks(s.read(SIZE_BLOCKS));
    format.setChannelMode(s.read(SIZE_CHANNEL));
    format.allocation_method = s.read(SIZE_ALLOCATION);
    format.setVirtualSubbands(s.read(SIZE_SUBBANDS));
    format.bitpool = s.read(SIZE_BITPOOL);
    crc_check= s.read(SIZE_CRC);
    if (format.channel_mode == SBC_CHANNEL_MODE_JOINT) {
        const int sb = format.nSubbands;
        for(int i = 0; i < sb - 1; ++i) {
            join[i] = (s.read(SIZE_JOIN) != 0);
        }
        join[sb - 1] = 0;
        s.read(SIZE_RFA);
    }
    format.evalNonstandard();
    return true;
}
unsigned char FrameHeader::evalCRC(BitStream &s) const {
    CRC crc;
    s.read(SIZE_SYNCWORD);
    int toRead = SIZE_SAMPLING + SIZE_BLOCKS + SIZE_CHANNEL
        + SIZE_ALLOCATION + SIZE_SUBBANDS + SIZE_BITPOOL;
    for(int i = 0; i < toRead; ++i)
        crc.bit(s.read(1) == 1);
    s.read(SIZE_CRC);
    if (format.channel_mode == SBC_CHANNEL_MODE_JOINT) {
        toRead = (format.nSubbands - 1) * SIZE_JOIN + SIZE_RFA;
        for(int i = 0; i < toRead; ++i)
            crc.bit(s.read(1) == 1);
    }
    toRead = format.wfx.nChannels * format.nSubbands * SIZE_SCALE_FACTOR;
    for(int i = 0; i < toRead; ++i)
        crc.bit(s.read(1) == 1);
    return crc.getCRC();
}

#endif
unsigned char FrameHeader::evalCRCEncode(BitStream &s, BYTE pre_calc_crc) const {

    CRC crc;

    crc.setCRC(pre_calc_crc);
    s.read(SIZE_SYNCWORD+SIZE_CRC+SIZE_CRC_READ);

    int toRead = 0;

    if (format.channel_mode == SBC_CHANNEL_MODE_JOINT) {
        for(int i = 0; i < SBCSUBBANDS; ++i)
            crc.bit(s.read(1) == 1);
    }

    crc.bits32(s.read(32));
    crc.bits32(s.read(32));

    return crc.getCRC();
}


unsigned char FrameHeader::preEvalCRC(BitStream &s) const {
    CRC crc;
    s.read(SIZE_SYNCWORD);
    for(int i = 0; i < SIZE_CRC_READ; ++i)
        crc.bit(s.read(1) == 1);
    return crc.getCRC();
}

void FrameHeader::writeCRC(BitStream &s, unsigned char crc) const {
    s.goTo(s.getPosition() + SIZE_SYNCWORD + SIZE_SAMPLING + SIZE_BLOCKS + SIZE_CHANNEL
        + SIZE_ALLOCATION + SIZE_SUBBANDS + SIZE_BITPOOL);
    s.write(SIZE_CRC, crc);
}

#ifdef DECODE_ON
void ScaleFactors::read(BitStream &s) {
    for(unsigned i = 0; i < SBCCHANNELS; ++i)
        for(unsigned j = 0; j < SBCSUBBANDS; ++j)
            scale_factor[i][j]=s.read(SIZE_SCALE_FACTOR);
}
#endif

void ScaleFactors::write(BitStream &s) const{
    for(unsigned i = 0; i < SBCCHANNELS; ++i)
        for(unsigned j = 0; j < SBCSUBBANDS; ++j)
            s.write(SIZE_SCALE_FACTOR, scale_factor[i][j]);
}


