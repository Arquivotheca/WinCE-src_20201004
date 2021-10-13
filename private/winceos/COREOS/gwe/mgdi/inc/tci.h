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
/*


*/
/*
    @doc BOTH INTERNAL
    @module tci.h | common structures of codepage and charset.
*/
#ifndef _TCI_H_
#define _TCI_H_

#define NCHARSETS      16
#define CHARSET_ARRAYS                                                      \
static unsigned int nCharsets = NCHARSETS;                                                 \
static unsigned int charsets[] = {                                                         \
      ANSI_CHARSET,   SHIFTJIS_CHARSET, HANGEUL_CHARSET, JOHAB_CHARSET,     \
      GB2312_CHARSET, CHINESEBIG5_CHARSET, HEBREW_CHARSET,                  \
      ARABIC_CHARSET, GREEK_CHARSET,       TURKISH_CHARSET,                 \
      BALTIC_CHARSET, EASTEUROPE_CHARSET,  RUSSIAN_CHARSET, THAI_CHARSET,   \
      VIETNAMESE_CHARSET, SYMBOL_CHARSET};                                  \
static unsigned int codepages[] ={ 1252, 932, 949, 1361,                                   \
                    936,  950, 1255, 1256,                                  \
                    1253, 1254, 1257, 1250,                                 \
                    1251, 874 , 1258, 42};                                  \
static DWORD fs[] = { FS_LATIN1,      FS_JISJAPAN,    FS_WANSUNG, FS_JOHAB,        \
               FS_CHINESESIMP, FS_CHINESETRAD, FS_HEBREW,  FS_ARABIC,       \
               FS_GREEK,       FS_TURKISH,     FS_BALTIC,  FS_LATIN2,       \
               FS_CYRILLIC,    FS_THAI,        FS_VIETNAMESE, FS_SYMBOL };


#endif //_TCI_H_
