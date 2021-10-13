//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/**************************************************************************


Module Name:

   fontData.h

Abstract:

   Code used by region tests

***************************************************************************/
#ifndef __FONTDATA_H__
#define __FONTDATA_H__

// for CE check support
#define testRast    12        // mscanada
#define testTT        0        // arial    

static int NTFontMetrics[][20] = { // UNDER_CE Supported: GetTextMetrics
    {20,17,3,4,0,7,31,400,0,32,65532,31,32,0,0,0,39,0},//Tahoma
    {20,15,5,3,0,10,11,400,0,32,65532,31,32,0,0,0,54,0},//Courier New
    {20,16,4,4,0,10,18,400,0,0,61695,31,32,0,0,0,23,2},//Symbol
    {20,16,4,2,1,7,47,400,0,32,65532,31,32,0,0,0,23,0},//times New Roman
    {20,16,4,2,0,16,24,400,0,0,61695,31,32,0,0,0,7,2},//wingdings
    {20,17,3,3,0,9,25,400,0,32,64258,31,32,0,0,0,39,0},//verdana
};


static int NTExtentResults[][90] = { 
    // Tahoma
   {  { 4},{ 5},{ 9},{ 7},{ 12},{ 9},{ 3},{ 5},{ 5},{ 8},{ 8},{ 4},{ 5},{ 4},{ 5},{ 7},
      { 7},{ 7},{ 7},{ 7},{ 7},{ 7},{ 7},{ 7},{ 7},{ 5},{ 5},{ 9},{ 9},{ 9},{ 6},{ 12},
      { 8},{ 7},{ 8},{ 8},{ 7},{ 7},{ 8},{ 8},{ 4},{ 5},{ 7},{ 6},{10},{ 8},{ 9},{ 7},
      { 9},{ 8},{ 8},{ 8},{ 8},{ 8},{ 12},{ 8},{ 7},{ 7},{ 5},{ 5},{ 5},{ 9},{ 7},{ 7},
      { 7},{ 7},{ 6},{ 7},{ 7},{ 4},{ 7},{ 7},{ 3},{ 4},{ 6},{ 3},{ 11},{ 7},{ 7},{ 7},
      { 7},{ 5},{ 6},{ 4},{ 7},{ 6},{ 10},{ 6},{ 6},{ 6},
   },
   // Courier New
   { { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
      { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
      { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
      { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
      { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
      { 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},{ 8},
   },
   /*
   // Symbol (slightly different than the desktop, fails on nt)
   {  {  3},{  9},{ 6},{ 8},{ 11},{ 10},{  6},{  4},{  4},{  7},{ 7},{  4},{  7},{  3},{  4},{ 7},
      {   7},{  7},{  7},{  7},{  7},{  7},{  7},{  7},{ 7},{  3},{  4},{  7},{  7},{  7},{  5},{ 8},
      {  9},{  9},{  9},{  8},{  8},{ 10},{  8},{  8},{  3},{ 9},{  9},{  9},{ 12},{ 10},{  9},{ 10},
      {  9},{  7},{  8},{  7},{  9},{  5},{ 11},{  9},{ 11},{  8},{  5},{ 11},{  4},{ 10},{  7},{  6},
      {  8},{  7},{  7},{  5},{  5},{  5},{  5},{  8},{  4},{  7},{  7},{  6},{  7},{  7},{  7},{  6},
      {  7},{  8},{  7},{  5},{  8},{  9},{  8},{  5},{  9},{  5},
   },
   */
   // Times
   {  {  5},{  5},{  7},{  7},{ 12},{ 11},{  3},{  5},{  5},{  7},{ 8},{  4},{  5},{  4},{  4},{  7},
      {  7},{  7},{  7},{  7},{  7},{  7},{  7},{  7},{  7},{  3},{  4},{  8},{  8},{  8},{  6},{ 13},
      {  9},{  8},{  9},{ 10},{  8},{  8},{ 10},{  9},{  5},{  6},{  9},{  8},{ 12},{ 10},{ 10},{ 8},
      { 10},{  9},{  8},{ 9},{ 9},{ 9},{ 13},{ 10},{ 9},{ 8},{  5},{  4},{  5},{  6},{  7},{  4},
      {  6},{  7},{  7},{  7},{  6},{  5},{  7},{  7},{  3},{  3},{  7},{  3},{ 11},{  7},{  7},{  7},
      {  7},{  5},{  6},{  4},{  7},{  7},{ 11},{  7},{  7},{  6},
   },
   // wingding
   {  { 15},{ 17},{ 20},{ 20},{ 13},{ 18},{  7},{ 16},{ 13},{ 17},{ 17},{ 18},{ 18},{ 22},{ 22},{ 16},
      { 20},{ 10},{ 10},{ 13},{  8},{  9},{ 16},{ 14},{ 16},{ 14},{ 13},{ 13},{ 13},{ 13},{  14},{ 14},
      { 9},{ 12},{ 10},{ 10},{ 14},{ 14},{  8},{ 8},{ 13},{  13},{ 13},{ 13},{ 17},{ 10},{ 13},{ 16},
      { 13},{ 13},{ 10},{ 12},{ 11},{ 11},{ 11},{ 10},{ 12},{ 13},{ 13},{ 13},{ 13},{ 17},{ 16},{ 14},
      { 16},{ 14},{ 14},{ 14},{ 15},{ 14},{ 16},{ 16},{ 12},{ 16},{ 19},{ 11},{ 14},{ 11},{ 13},{ 13},
      { 13},{ 13},{  7},{ 11},{ 15},{ 13},{  9},{ 16},{ 16},{ 13},
   },
   // verdana
   {  {  5},{  5},{ 10},{  8},{ 13},{ 9},{  3},{  6},{  6},{  9},{  9},{  5},{  7},{  5},{  6},{  8},
      {  8},{  8},{  8},{  8},{  8},{  8},{  8},{  8},{  8},{  6},{  6},{  9},{  9},{  9},{  7},{ 13},
      {  9},{  8},{ 9},{ 9},{ 8},{ 8},{  9},{ 9},{ 5},{  6},{ 8},{ 7},{ 11},{ 9},{ 10},{ 8},
      { 10},{  8},{  9},{  9},{ 9},{ 9},{ 13},{  9},{  9},{ 9},{  6},{  6},{  6},{ 11},{  8},{  8},
      { 8},{ 8},{ 8},{ 8},{ 8},{  5},{ 8},{  8},{  3},{  4},{  7},{  3},{ 11},{  8},{ 8},{ 8},
      { 8},{ 5},{  7},{ 6},{ 8},{ 8},{ 11},{ 7},{ 8},{ 7},
   },
   // Arial Raster font
   {  {   3},{  5},{   8},{   7},{ 12},{  9},{   2},{   4},{   4},{  5},{  8},{  4},{  4},{   4},{   4},{  7},
      {   7},{   7},{  7},{   7},{  7},{   7},{   7},{   7},{  7},{   4},{   4},{  8},{  8},{  8},{   7},{ 13},
      {  9},{   9},{   9},{   9},{  9},{  8},{  10},{  9},{   3},{   6},{  9},{  7},{ 11},{  9},{  10},{  9},
      { 10},{  9},{  9},{    7},{  9},{ 11},{ 13},{  7},{   9},{  7},{  4},{   4},{  4},{   5},{    7},{   4},
      {   7},{  7},{   7},{   7},{   7},{  4},{   7},{   7},{   3},{  4},{  7},{  3},{ 11},{   7},{  7},{   7},
      {   7},{  4},{  7},{   4},{   7},{   5},{   9},{   7},{  7},{  7},
   },
};

static int NTExtentZResults[] ={ 700, 800, 800, 800, 1300, 900, 700};

#ifdef UNDER_CE
static ABC NT_ABCWidths[3][90] = { // tahoma only
  {{  4,  3,  3 },{  2, 9, 1 },{  2, 19,  2 },{  2, 14,  1 },{  2, 27,  1 },{  1, 21, -1 },
   {  2,  3,  2 },{  2,  9,  1 },{  1,  9,  2 },{  2,  14,  1 },{  2, 19,  2 },{  1, 6, 2 },
   {  1,  9,  1 },{  3,  4,  2 },{  0, 11,  1 },{  1, 14,  2 },{  4, 11,  2 },{  2, 14,  1 },
   {  1, 14,  2 },{  1, 16,  0 },{  2, 13,  2 },{  1, 15,  1 },{  2, 14,  1 },{  1, 15,  1 },
   {  1, 15,  1 },{  4,  4,  3 },{  2,  6,  3 },{  3, 16,  4 },{  3, 17,  3 },{  3, 16,  4 },
   {  2, 12,  1 },{  2, 24,  2 },{ 0, 19,  0 },{  2, 15,  1 },{  1, 17,  1 },{  2, 18,  1 },
   {  2, 14,  1 },{  2, 14,  0 },{  1, 19,  1 },{  2, 17,  2 },{  1,  9,  2 },{  0,  11,  2 },
   {  2, 16,  0 },{  2, 13,  0 },{  2, 20,  2 },{  2, 17,  2 },{  1, 20,  1 },{  2, 14,  1 },
   {  1, 20,  1 },{  2, 18,  -1 },{  1, 15,  1 },{  0, 19,  -1 },{  2, 16,  2 },{ 0, 19,  0 },
   {  0, 28,  0 },{ 0, 18, 0 },{  0, 19,  -1 },{  1, 15,  1 },{  3,  8,  1 },{  1, 11, 0 },
   {  1,  8,  3 },{  2, 19,  2 },{  0, 17,  0 },{  4,  6,  7 },{  1, 13,  2 },{  2, 14,  1 },
   {  1, 12,  1 },{  1, 14,  2 },{  1, 14,  1 },{  0, 11, -1 },{  1, 14,  2 },{  2, 13,  2 },
   {  2,  3,  2 },{  -1,  8,  2 },{  2, 14,  -1 },{  2,  3,  2 },{  2, 23,  1 },{  2, 13,  2 },
   {  1, 15,  1 },{  2, 14,  1 },{  1, 14,  2 },{  2,  9,  0 },{  1, 12,  1 },{  0,  10,  0 },
   {  2, 13,  2 },{  0, 15,  0 },{  0, 23,  0 },{  0, 15,  0 },{  0, 15,  0 }, {1, 12, 1} },
};
#else
static ABC NT_ABCWidths[3][90] = { // tahoma only
  {{  4,  3,  4 },{  2, 9, 2 },{  2, 18,  3 },{  2, 14,  1 },{  2, 28,  1 },{  1, 22, -1 },
   {  2,  3,  2 },{  2,  9,  1 },{  1,  9,  2 },{  2,  14,  1 },{  2, 19,  2 },{  1, 7, 2 },
   {  1,  9,  2 },{  3,  4,  3 },{  0, 12,  0 },{  1, 15,  1 },{  4, 11,  2 },{  2, 14,  1 },
   {  2, 14,  1 },{  1, 16,  0 },{  2, 14,  1 },{  1, 15,  1 },{  2, 15,  0 },{  1, 15,  1 },
   {  1, 15,  1 },{  4,  4,  3 },{  2,  7,  2 },{  3, 17,  3 },{  3, 17,  3 },{  3, 17,  3 },
   {  2, 13,  0 },{  2, 25,  2 },{ 0, 19,  0 },{  2, 16,  1 },{  1, 17,  1 },{  2, 19,  1 },
   {  2, 15,  1 },{  2, 15,  0 },{  1, 18,  2 },{  2, 18,  2 },{  1,  9,  2 },{  0,  11,  2 },
   {  2, 18,  -1 },{  2, 14,  0 },{  2, 21,  2 },{  2, 17,  2 },{  1, 21,  1 },{  2, 15,  1 },
   {  1, 21,  1 },{  2, 19,  -1 },{  1, 16,  1 },{  0, 19,  0 },{  2, 17,  2 },{ 0, 19,  0 },
   {  0, 29,  0 },{ 0, 19, 0 },{  0, 19,  -1 },{  1, 16,  1 },{  3,  8,  1 },{  1, 12,  -1 },
   {  2,  8,  2 },{  2, 19,  2 },{  0, 17,  0 },{  4,  7,  6 },{  1, 14,  2 },{  2, 15,  1 },
   {  1, 13,  1 },{  1, 15,  2 },{  1, 15,  1 },{  0, 11, -1 },{  1, 15,  2 },{  2, 14,  2 },
   {  2,  3,  2 },{  -1,  8,  2 },{  2, 15,  -1 },{  2,  3,  2 },{  2, 23,  2 },{  2, 14,  2 },
   {  1, 15,  1 },{  2, 15,  1 },{  1, 15,  2 },{  2, 10,  0 },{  1, 12,  1 },{  0,  11,  0 },
   {  2, 14,  2 },{  0, 16,  0 },{  0, 24,  0 },{  0, 16,  0 },{  0, 16,  0 }, {1, 12, 1} },
};
#endif

#ifdef UNDER_CE
static int NT_32Widths[] = {
   10, 12, 23, 17, 30, 21, 7, 12, 12, 17, 23, 9, 11, 9, 12, 17, 17, 17, 17, 17,
   17, 17, 17, 17, 17, 11, 11, 23, 23, 23, 15, 28, 19, 18, 19, 21, 17, 16, 21, 21,
   12, 13, 18, 15, 24, 21, 22, 17, 22, 19, 17, 18, 20, 19, 28, 18, 18, 17, 12, 12,
   12, 23, 17, 17, 16, 17, 14, 17, 16, 10, 17, 17, 7, 9, 15, 7, 26, 17, 17, 17,
   17, 11, 14, 10, 17, 15, 23, 15, 15, 14
};
#else

static int NT_32Widths[] = {
   11, 13, 23, 17, 31, 22, 7, 12, 12, 17, 23, 10, 12, 10, 12, 17, 17, 17, 17, 17,
   17, 17, 17, 17, 17, 11, 11, 23, 23, 23, 15, 29, 19, 19, 19, 22, 18, 17, 21, 22,
   12, 13, 19, 16, 25, 21, 23, 18, 23, 20, 18, 19, 21, 19, 29, 19, 18, 18, 12, 12,
   12, 23, 17, 17, 17, 18, 15, 18, 17, 10, 18, 18, 7, 9, 16, 7, 27, 18, 17, 18,
   18, 12, 14, 11, 18, 16, 24, 16, 16, 14
};
#endif

static TCHAR *people[] = {
   TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),
   TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),
   TEXT(" Presenting "),
   TEXT("Microsoft"),
   TEXT(" Windows CE "),
   TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),
   TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT(""),
};


static int numPeople = sizeof(people)/sizeof(TCHAR *);

static int mode[] = {
   TA_BASELINE,
   TA_BOTTOM,
   TA_TOP,
   TA_CENTER,
   TA_LEFT,
   TA_RIGHT,
#ifndef UNDER_CE
   VTA_BASELINE,
   VTA_LEFT,
   VTA_RIGHT,
   VTA_CENTER,
   VTA_BOTTOM,
   VTA_TOP
#endif
};

static int numModes = sizeof(mode)/sizeof(int);

#endif // __FONTDATA_H__
