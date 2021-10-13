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
 * cacheDefs.h
 */

/*
 * This file stores the modes both returned by the cache control
 * functions and used by the tux function table to specify the tests.
 */

#ifndef __CACHE_DEFS_H
#define __CACHE_DEFS_H

// the smallest array size in KB
#define MIN_ARRAY_SIZE 4

// the biggest array size in KB
#define MAX_ARRAY_SIZE 512

// the smallest stride (in bytes) to access the cache
#define MIN_STRIDE_SIZE 4

// the biggest stride (in bytes) to access the cache
#define MAX_STRIDE_SIZE 256

// the biggest stride to search the access jump
#define JUMP_SEARCH_LIMIT 256

// the maximum running loop
#define MAX_RUNNING_LOOP 5

// cache size jump search threshold 15%
#define CACHE_SIZE_JUMP_THRESHOLD 15

// cache line size jump threshold 15%
#define CACHE_LINE_JUMP_THRESHOLD 50

// enumeration for mark the jump position
enum JUMP_STATUS {
  SUCCESSFUL,
  FAILED,
  NOT_CALCULATED,
  NONE
};

/*
 * The default running time for the first array's access. 
 * Other arrays will be accessed with the same access 
 * point number based on the iteration times
 */
#define MAX_RUNNING_TIME_MS 100

// for format print purposes
#define BUFFER_SIZE 1024

/*
 * cache access need to save these related information for
 * the analysis of cache size and cache line/block size
 */
struct CACHE_ACCESS_INFO {
  DWORD dwPt;
  DWORD dwArray;
  DWORD dwStride;
  ULONGLONG dwPnt;
  ULONGLONG ullTime;
};

#endif

