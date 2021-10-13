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
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the cache tests
 */
#include "..\code\tuxcaches.h"

#define CACHE_BASE 1000

#define CACHE_BASE_CACHE_DISABLED (CACHE_BASE + 100)
#define CACHE_BASE_WRITE_THROUGH (CACHE_BASE + 200)
#define CACHE_BASE_WRITE_BACK (CACHE_BASE + 300)

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
  _T("Cache Tests"                                ), 0, 0, 0, NULL,

  _T(  "Cache Tests Usage / Help"), 1, CT_IGNORED, 1, cacheTestPrintUsage,
  _T(  "Print Cache Info"), 1, CT_IGNORED, CACHE_BASE + 1, printCacheInfo,

  /* 
   * The parameter tells the cache test how to set the test array size
   * and the cache mode.  See the cache test tux header tuxcaches.h
   * for more information.
   */
  _T(  "No caching"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,
    /*
    *   Re-run tests using virtual alloc
    */
  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 20, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 21, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 22, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 23, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 24, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 25, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 26, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 27, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 28, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 29, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 31, entryCacheTestWriteEverythingReadEverythingMixedUp,


  _T(  "Write-through cache mode"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,
    /*
    *   Re-run tests using virtual alloc
    */
  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 20, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 21, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 22, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 23, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 24, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 25, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 26, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 27, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 28, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 29, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 30, entryCacheTestWriteEverythingReadEverythingMixedUp,



  _T(  "Write-back cache mode"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_ALLOC_PHYS_MEM | CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_ALLOC_PHYS_MEM | CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_ALLOC_PHYS_MEM | CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,
    /*
    *   Re-run tests using virtual alloc
    */
  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 20, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 21, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 22, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 23, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 24, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 25, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 26, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 27, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 28, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 29, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 30, entryCacheTestWriteEverythingReadEverythingMixedUp,

   NULL,                                       0,   0,              0, NULL  // marks end of list
};


