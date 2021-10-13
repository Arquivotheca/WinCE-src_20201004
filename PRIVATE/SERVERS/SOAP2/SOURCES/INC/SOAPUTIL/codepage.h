//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      codepage.h
//
// Contents:
//
//      Codepage definitions
//
//----------------------------------------------------------------------------------

#ifndef _CODEPAGE_H_
#define _CODEPAGE_H_

typedef UINT CODEPAGE;              // Codepage corresponds to Mlang ID

#define CP_UNDEFINED    CODEPAGE(-1)

#define CP_UTF_16       1200
#define CP_1250         1250        // windows Latin 2 (central europe) 
#define CP_1251         1251        // windows Cyrillic (Slavic)
#define CP_1252         1252        // windows Latin 1 (ANSI)
#define CP_1253         1253        // windows Greek
#define CP_1254         1254        // windows Latin 5 (Turkish)
#define CP_1255         1255        // windows Hebrew
#define CP_1256         1256        // windows Arabic
#define CP_1257         1257        // windows Baltic Rim
#define CP_1258         1258        // windows Viet nam

#define CP_28591        28591       // iso-8859-1
#define CP_28592        28592       // iso-8859-2
#define CP_28593        28593       // iso-8859-3
#define CP_28594        28594       // iso-8859-4
#define CP_28595        28595       // iso-8859-5
#define CP_28596        28596       // iso-8859-6
#define CP_28597        28597       // iso-8859-7
#define CP_28598        28598       // iso-8859-8
#define CP_28599        28599       // iso-8859-9

#define CP_936          936         // chinese, simplified
#define CP_950          950         // chinese, traditional
#define CP_949          949         // korean
#define CP_932          932         // japanese, shitf-jis

#define CP_20127        20127       // us-ascii

#define CP_UTF_7        65000
#define CP_UTF_8        65001
#define CP_UCS_4        12000
#define CP_UCS_2        12001

#endif



