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



#define SBCSUBBANDS 8
#define X_END_PLUS_1 80




#define BITSHIFT_FACTOR 13
#define FACTOR 8192
#define DIVIDE_FACTOR 2

const short Proto_8[74] = {
          1,    3,    4,    6,    9,    12,    14,
   46,   66,   85,  104,  120,  130,   133,   125,
  557,  680,  799,  911, 1010, 1091,  1153,  1191,
 -557, -435, -320, -214, -120,  -40,    24,    72,
  -46,  -28,  -13,   -1,    7,   13,    16,    17,

         16,   17,   16,   13,    1,    13,    28,
  106,   72,   24,  -40,  214,  320,   435,
 1204, 1191, 1153, 1091,  911,  799,   680,
  106,  125,  133,  130,  104,   85,    66,
   16,   14,   12,    9,    4,    3
};

#define D_8034 8034
#define D_4551 4551
#define D_1598 1598
#define D_6811 6811
#define D_7568 7568
#define D_3135 3135


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
    pX = pBaseX + indexes[i][1];
    Y[0] = *pX *  *P8; P8++; pX++;
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
    Y[4] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][4];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[1] += *pX *  *P8; P8++; pX++;
    Y[2] += *pX *  *P8; P8++; pX++;
    Y[3] += *pX *  *P8; P8++; pX++;
    Y[4] = (Y[4] + *pX * *P8)>> BITSHIFT_FACTOR; pX++;P8++;
    Y[4] *= (16384/DIVIDE_FACTOR);
    Y[3] = (Y[3] + *pX * *P8)>> BITSHIFT_FACTOR; pX++;P8++;
    Y[2] = (Y[2] + *pX * *P8)>> BITSHIFT_FACTOR; pX++;P8++;
    Y[1] = (Y[1] + *pX * *P8)>> BITSHIFT_FACTOR; P8++;


    pX = pBaseX + indexes[i][5];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[9] = *pX *  *P8; P8++; pX++;
    Y[10] = *pX *  *P8; P8++; pX++;
    Y[11] = *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][6];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][7];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] -= *pX *  *P8; P8++; pX++;
    Y[10] -= *pX *  *P8; P8++; pX++;
    Y[9] -= *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][8];
    Y[0] += *pX *  *P8; P8++; pX++;
    Y[9] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] -= *pX *  *P8; P8++; pX++;
    Y[10] -= *pX *  *P8; P8++; pX++;
    Y[9] -= *pX *  *P8; P8++;
    pX = pBaseX + indexes[i][9];
    Y[0] = (Y[0] + *pX * *P8)>> BITSHIFT_FACTOR; P8++; pX++;
    Y[0] *= (11585/DIVIDE_FACTOR);
    Y[9] += *pX *  *P8; P8++; pX++;
    Y[10] += *pX *  *P8; P8++; pX++;
    Y[11] += *pX *  *P8; P8++; pX++;
    pX++;
    Y[11] = (Y[11] + *pX * *P8)>> BITSHIFT_FACTOR; P8++; pX++;
    Y[10] = (Y[10] - (*pX * *P8))>> BITSHIFT_FACTOR; pX++;
    Y[9] = (Y[9] - *pX)>> BITSHIFT_FACTOR;






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
    subband_samples[0] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR);
    subband_samples[7] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR);




    s_reverse_sign = (Y[1]) * D_1598;
    s_same_sign_next2  = (Y[2]) * D_3135;
    s_reverse_sign -= (Y[3]) * D_6811;
    s_same_sign = Y[4] - Y[0];
    s_reverse_sign += (Y[9]) * D_8034;
    s_same_sign_next2 -= (Y[10]) * D_7568;
    s_reverse_sign += (Y[11]) * D_4551;

    s_same_sign += s_same_sign_next2;
    subband_samples[1] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR);
    subband_samples[6] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR);




    s_reverse_sign = ((Y[3]) * D_4551);
    s_reverse_sign -= (Y[1]) * D_8034;//3
    s_reverse_sign += (Y[9]) * D_1598;//4
    s_reverse_sign += ((Y[11]) * D_6811);

    s_same_sign -= (s_same_sign_next2<<1);
    subband_samples[2] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR);
    subband_samples[5] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR);



    s_reverse_sign = -(Y[1] * D_4551);
    s_reverse_sign += (Y[3]) * D_1598;
    s_same_sign = Y[4] + Y[0];     
    s_reverse_sign += (Y[9]) * D_6811;
    s_reverse_sign -= (Y[11]) * D_8034;

    s_same_sign -= s_same_sign_next1;
    subband_samples[3] = ((s_same_sign+s_reverse_sign)>>BITSHIFT_FACTOR);
    subband_samples[4] = ((s_same_sign-s_reverse_sign)>>BITSHIFT_FACTOR);

}


