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

#define SBCSUBBANDS 8
#define X_END_PLUS_1 80

#define FACTOR1_LOG_2 15
#define FACTOR_LOG_2 14
#define FACTOR1_MINUS_FACTOR_LOG_2 1

#define EXTRA_FACTOR_LOG_2 4

const short Proto_8[74] = {

//These factors are multiplied by an extra factor of
//EXTRA_FACTOR. This allows for higher precision arithmetic
         82,   180,   291,   432,   598,   774,   935,
//end EXTRA_FACTOR 

  185,  263,  343,  418,  480,  521,  532,  502,
 2228, 2719, 3197, 3644, 4039, 4367, 4612, 4764,
-2228,-1743,-1280, -856, -480, -161,   96, 290,


//These factors are multiplied by an extra factor of
//EXTRA_FACTOR. This allows for higher precision arithmetic
-2967,-1834,  -865,   -94,   473,   848,  1046,  1103,


1055, 1103, 1046,  848,   94,  865, 1834,
//end EXTRA_FACTOR 

  424,  290,   96, -161,  856, 1280, 1743,
 4815, 4764, 4612, 4367, 3644, 3197, 2719,
  424,  502,  532,  521,  418,  343,  263,


 //These factors are multiplied by an extra factor of
//EXTRA_FACTOR. This allows for higher precision arithmetic
1055,  935,  774,  598, -291,  180,   82
//end EXTRA_FACTOR 
};




#define D_8034 16069
#define D_4551 9102
#define D_1598 3196
#define D_6811 13623
#define D_7568 15137
#define D_3135 6270

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


void process(const short *audio_samples, int *subband_samples, int &XStart, short * pBaseX) 
{
    int Y[2 * SBCSUBBANDS];
    int i; 
    // Shifting and input
    int idx = XStart;

    pBaseX[idx] = audio_samples[0];

    pBaseX[idx - 1] = audio_samples[2];
    pBaseX[idx - 2] = audio_samples[4];
    pBaseX[idx - 3] = audio_samples[6];
    pBaseX[idx - 4] = audio_samples[8];
    pBaseX[idx - 5] = audio_samples[10];
    pBaseX[idx - 6] = audio_samples[12];
    pBaseX[idx - 7] = audio_samples[14];


    // Handle index looping around
    XStart -= SBCSUBBANDS;
    if (XStart < 0) {
        XStart = XStart + X_END_PLUS_1;
    } 

    int idxStart = XStart + 1;

    short * pX ;
    
    if(idxStart == 80)
        idxStart = 0;

    i = idxStart>>3;
    
    const short * P8 = Proto_8;
    pX = pBaseX + indexes[i][0] + 1;
    
    Y[1] = *pX *  *P8; P8++; pX++;
    Y[2] = *pX *  *P8; P8++; pX++;
    Y[3] = *pX *  *P8; P8++; pX++;
    Y[4] = *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;    
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++;

    pX = pBaseX + indexes[i][4]; 
    P8 += 24;

    Y[0] = *pX *  *P8; P8++; pX++;    
    Y[1] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[4] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;    
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++;
    Y[0] = Y[0] >> EXTRA_FACTOR_LOG_2;
    Y[1] = Y[1] >> EXTRA_FACTOR_LOG_2;
    Y[2] = Y[2] >> EXTRA_FACTOR_LOG_2;
    Y[3] = Y[3] >> EXTRA_FACTOR_LOG_2;
    Y[4] = Y[4] >> EXTRA_FACTOR_LOG_2;

    P8 -= 32;

    
    pX = pBaseX + indexes[i][1];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[4] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][2];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[4] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++;

    
    pX = pBaseX + indexes[i][3]; 
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;


    //would divide by 2^FACTOR1_LOG_2 then multiply 
    //by 2^FACTOR_LOG_2, instead just multiply by the
    //difference of the 2 factors
    //Y[4] = (Y[4] + (*pX *  *P8)) >> FACTOR1_LOG_2; pX++;P8++;
    //Y[4] *= (16384);
    Y[4] = (Y[4] + (*pX *  *P8)) >> FACTOR1_MINUS_FACTOR_LOG_2; pX++;P8++;
    Y[3] = (Y[3] + (*pX *  *P8)) >> FACTOR1_LOG_2; pX++;P8++;
    Y[2] = (Y[2] + (*pX *  *P8)) >> FACTOR1_LOG_2; pX++;P8++;
    Y[1] = (Y[1] + (*pX *  *P8)) >> FACTOR1_LOG_2; P8++;
    P8 += 8; 

    
    pX = pBaseX + indexes[i][5];
    Y[0] += ((*pX *  *P8) >> EXTRA_FACTOR_LOG_2); P8++; pX++;
    Y[9] = *pX *  *P8; P8++; pX++;
    Y[10] = *pX *  *P8; P8++; pX++;
    Y[11] = *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++;


    pX = pBaseX + indexes[i][9];
    P8 += 21;

    Y[0]  += ((*pX *  *P8) >> EXTRA_FACTOR_LOG_2); P8++; pX++;
    Y[9]  += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    Y[10] -= *pX *  *P8; P8++; pX++;
    Y[9]  -= *pX *  *P8; P8++;
    Y[9]  = Y[9] >> EXTRA_FACTOR_LOG_2;
    Y[10] = Y[10] >> EXTRA_FACTOR_LOG_2;
    Y[11] = Y[11] >> EXTRA_FACTOR_LOG_2;

    P8 -= 28;
    
    pX = pBaseX + indexes[i][6];
    Y[0]  += *pX *  *P8; P8++; pX++;
    Y[9]  += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[9]  += *pX *  *P8; P8++;
    
    pX = pBaseX + indexes[i][7];
    Y[0]  += *pX *  *P8; P8++; pX++;
    Y[9]  += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] -= *pX *  *P8; P8++; pX++;
    Y[10] -= *pX *  *P8; P8++; pX++;
    Y[9]  -= *pX *  *P8; P8++;


    pX = pBaseX + indexes[i][8];
    Y[0]  = (Y[0] + (*pX *  *P8)) >> FACTOR1_LOG_2; P8++; pX++;
    Y[0]  *= 11585;
    Y[9]  += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] = (Y[11] - (*pX *  *P8)) >> FACTOR1_LOG_2; P8++; pX++;
    Y[10] = (Y[10] - (*pX *  *P8)) >> FACTOR1_LOG_2; P8++;pX++;
    Y[9]  = (Y[9] -  (*pX *  *P8)) >> FACTOR1_LOG_2;



    int s_same_sign, s_reverse_sign, s_same_sign_next1, s_same_sign_next2;
    
    //precision loss here, on top of loss on Y[k] and CC2
    s_reverse_sign = (Y[1]) * D_6811;
    s_same_sign_next1  = (Y[2]) * D_7568;
    s_reverse_sign += (Y[3]) * D_8034;
    s_same_sign = Y[4] + Y[0];     
    s_reverse_sign += (Y[9]) * D_4551;
    s_same_sign_next1 += ((Y[10]) * D_3135);
    s_reverse_sign += (Y[11]) * D_1598;


    s_same_sign += s_same_sign_next1;
    subband_samples[0] = ((s_same_sign+s_reverse_sign) >> FACTOR_LOG_2);
    subband_samples[7] = ((s_same_sign-s_reverse_sign) >> FACTOR_LOG_2);




    s_reverse_sign = (Y[1]) * D_1598;
    s_same_sign_next2  = (Y[2]) * D_3135;
    s_reverse_sign -= (Y[3]) * D_6811;
    s_same_sign = Y[4] - Y[0];
    s_reverse_sign += (Y[9]) * D_8034;
    s_same_sign_next2 -= (Y[10]) * D_7568;
    s_reverse_sign += (Y[11]) * D_4551;

    s_same_sign += s_same_sign_next2;
    subband_samples[1] = ((s_same_sign-s_reverse_sign) >> FACTOR_LOG_2);
    subband_samples[6] = ((s_same_sign+s_reverse_sign) >> FACTOR_LOG_2);




    s_reverse_sign = ((Y[3]) * D_4551);
    s_reverse_sign -= (Y[1]) * D_8034;//3
    s_reverse_sign += (Y[9]) * D_1598;//4
    s_reverse_sign += ((Y[11]) * D_6811);

    s_same_sign -= (s_same_sign_next2<<1);
    subband_samples[2] = ((s_same_sign+s_reverse_sign) >> FACTOR_LOG_2);
    subband_samples[5] = ((s_same_sign-s_reverse_sign) >> FACTOR_LOG_2);



    s_reverse_sign = -(Y[1] * D_4551);
    s_reverse_sign += (Y[3]) * D_1598;
    s_same_sign = Y[4] + Y[0];     
    s_reverse_sign += (Y[9]) * D_6811;
    s_reverse_sign -= (Y[11]) * D_8034;

    s_same_sign -= s_same_sign_next1;
    subband_samples[3] = ((s_same_sign+s_reverse_sign) >> FACTOR_LOG_2);
    subband_samples[4] = ((s_same_sign-s_reverse_sign) >> FACTOR_LOG_2);

}


