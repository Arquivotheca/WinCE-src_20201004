//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

// Intended to be included from qamath.h



// Produces a set of numbers that have an exact representation in both IEEE754-1985
// single-precision floating point, and in s15.16 fixed point.
//
HRESULT GetValuesCommonToFixedAndFloat(float **ppfValues, UINT *puiNumValues);

//
// Convert an array of floats to s15.16 fixed point
//
void FloatToFixedArray(DWORD *pdwValues, UINT uiNumEntries);

//
// Structure for specifying ranges of numbers to cover, with specified "step"
//
typedef struct _VALRANGE {
	INT  iStart;     // Beginning of Range
	UINT uiNumSteps; // Number of "steps" (size iStep) to test
    INT  iStep;      // Meaning:  step in increments of 1/(2^iStep)
    INT  StepDirection; // Toward +inf, towards -inf
} VALRANGE;

//
// All values specified per this table, should be exactly-representable in either
// single-precision floating point or s15.16 fixed point.
//
// Most of the range/step constraints could be generated on-the-fly, 
// but for the sake of clarity it is provided in table form instead.
//
// To make testing most easily accessible for use with matrices, it is most
// convenient to have the number of steps equal a multiple of 16 (4x4 matrix)
//
#define D3DQA_NUMFIXEDFLOAT_RANGES 34
static
CONST VALRANGE ValidFixedAndFloat[]={ //|   DATA TYPE PRECISION    |

//   | Start |# Steps| Step  |StepDir|  | Floating Pt |  Fixed Pt  | Test Expectation |
//   +-------+-------+-------+-------+  +---------------------------------------------+
//   |       |       |       |       |  |             |            |                  |
//
//     (Pos Pow-Of-Two Steps)                                                           
     {     1 ,    96 ,   16  ,    1  },//  1/(2^23)   |  1/(2^16)  |      1/(2^16)    | *  
     {     2 ,    96 ,   16  ,    1  },//  1/(2^22)   |  1/(2^16)  |      1/(2^16)    | ** 
     {     4 ,    96 ,   16  ,    1  },//  1/(2^21)   |  1/(2^16)  |      1/(2^16)    | *** 
     {     8 ,    96 ,   16  ,    1  },//  1/(2^20)   |  1/(2^16)  |      1/(2^16)    | **** CONSTRAINED BY
     {    16 ,    96 ,   16  ,    1  },//  1/(2^19)   |  1/(2^16)  |      1/(2^16)    | **** FIXED PRECISION
     {    32 ,    96 ,   16  ,    1  },//  1/(2^18)   |  1/(2^16)  |      1/(2^16)    | *** 
     {    64 ,    96 ,   16  ,    1  },//  1/(2^17)   |  1/(2^16)  |      1/(2^16)    | ** 
     {   128 ,    96 ,   16  ,    1  },//  1/(2^16)   |  1/(2^16)  |      1/(2^16)    | * 
     {   256 ,    96 ,   15  ,    1  },//  1/(2^15)   |  1/(2^16)  |      1/(2^15)    | @ 
     {   512 ,    96 ,   14  ,    1  },//  1/(2^14)   |  1/(2^16)  |      1/(2^14)    | @@  
     {  1024 ,    96 ,   13  ,    1  },//  1/(2^13)   |  1/(2^16)  |      1/(2^13)    | @@@   
     {  2048 ,    96 ,   12  ,    1  },//  1/(2^12)   |  1/(2^16)  |      1/(2^12)    | @@@@ CONSTRAINED BY FLT PRECISION
     {  4096 ,    96 ,   11  ,    1  },//  1/(2^11)   |  1/(2^16)  |      1/(2^11)    | @@@   
     {  8192 ,    96 ,   10  ,    1  },//  1/(2^10)   |  1/(2^16)  |      1/(2^10)    | @@   
     { 16384 ,    96 ,    9  ,    1  },//  1/(2^9 )   |  1/(2^16)  |      1/(2^9 )    | @ 

//     (Neg Pow-Of-Two Steps)                                                           
     {    -1 ,    96 ,   16  ,   -1  },//  1/(2^23)   |  1/(2^16)  |      1/(2^16)    | *  
     {    -2 ,    96 ,   16  ,   -1  },//  1/(2^22)   |  1/(2^16)  |      1/(2^16)    | ** 
     {    -4 ,    96 ,   16  ,   -1  },//  1/(2^21)   |  1/(2^16)  |      1/(2^16)    | *** 
     {    -8 ,    96 ,   16  ,   -1  },//  1/(2^20)   |  1/(2^16)  |      1/(2^16)    | **** CONSTRAINED BY
     {   -16 ,    96 ,   16  ,   -1  },//  1/(2^19)   |  1/(2^16)  |      1/(2^16)    | **** FIXED PRECISION
     {   -32 ,    96 ,   16  ,   -1  },//  1/(2^18)   |  1/(2^16)  |      1/(2^16)    | *** 
     {   -64 ,    96 ,   16  ,   -1  },//  1/(2^17)   |  1/(2^16)  |      1/(2^16)    | ** 
     {  -128 ,    96 ,   16  ,   -1  },//  1/(2^16)   |  1/(2^16)  |      1/(2^16)    | * 
     {  -256 ,    96 ,   15  ,   -1  },//  1/(2^15)   |  1/(2^15)  |      1/(2^15)    | @ 
     {  -512 ,    96 ,   14  ,   -1  },//  1/(2^14)   |  1/(2^14)  |      1/(2^14)    | @@  
     { -1024 ,    96 ,   13  ,   -1  },//  1/(2^13)   |  1/(2^13)  |      1/(2^13)    | @@@   
     { -2048 ,    96 ,   12  ,   -1  },//  1/(2^12)   |  1/(2^12)  |      1/(2^12)    | @@@@ CONSTRAINED BY FLT PRECISION
     { -4096 ,    96 ,   11  ,   -1  },//  1/(2^11)   |  1/(2^11)  |      1/(2^11)    | @@@   
     { -8192 ,    96 ,   10  ,   -1  },//  1/(2^10)   |  1/(2^10)  |      1/(2^10)    | @@   
     {-16384 ,    96 ,    9  ,   -1  },//  1/(2^9 )   |  1/(2^9 )  |      1/(2^9 )    | @ 

//       (Special Cases)         
     {     0 ,    96 ,   16  ,    1  },//             |  1/(2^16)  |      1/(2^16)    |   
     {     0 ,    96 ,   16  ,   -1  },//             |  1/(2^16)  |      1/(2^16)    |   
     { 32768 ,    96 ,    9  ,   -1  },//  1/(2^9 )   |  1/(2^16)  |      1/(2^16)    |  Max range of s15.16
     {-32768 ,    96 ,    9  ,    1  } //  1/(2^9 )   |  1/(2^16)  |      1/(2^16)    |  Min range of s15.16 (not a typo)
};


//
// This is a table of "interesting" IEEE 754-1985 single precision floating point
// values.
//
#define D3DQA_NUM_UNUSUAL_SINGLE_IEEE754 12
__declspec(selectany) DWORD UnusualSinglePrecisionIEE754[] =
{
	0x7F7FFFFF, // Largest positive float (FLT_MAX)
	0xFF7FFFFF, // Smallest negative float ((-1) * FLT_MAX)
	0x00800001, // Smallest positive normalized float (FLT_MIN)
	0x80800001, // Largest negative normalized float ((-1) * FLT_MIN)
	0x80000000, // Negative Zero
	0x00000000, // Positive Zero
	0x00000001, // Smallest positive denormalized float
	0x80000001, // Largest negative denormalized float
	0x7F800001, // NaN; can change f field to any other non-zero value (it will still be NaN)
	0xFF800001, // NaN; can change f field to any other non-zero value (it will still be NaN)
	0x7F800000, // Positive Infinity
	0xFF800000  // Negative Infinity
};

//
// Avoid type conversion when retrieving items from the floating point table
//
#define GetElementUnusualSinglePrecision(_index) (*(float*)(&UnusualSinglePrecisionIEE754[_index]))

