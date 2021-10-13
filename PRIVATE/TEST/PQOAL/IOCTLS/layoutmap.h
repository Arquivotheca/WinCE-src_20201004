//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * layoutMap.h
 */

#ifndef LAYOUT_MAP_H
#define LAYOUT_MAP_H

#include "..\common\commonUtils.h"
#include <windows.h>


/*
 * code to determine if a given layout overlaps
 */

typedef struct _sLayoutMap
{
  DWORD dwLowerBound;
  DWORD dwUpperBound;
} sLayoutMap;

/*
 * sort the map and check for overlapping areas.  I use the swap sort
 * algorithm, and check for overlapping memory areas at each step.  I
 * could do it afterward but this makes the code a little cleaner and
 * a little easier to read.
 *
 * This allows holes in the map.
 * 
 * map is the memory map and dwSize is the number of valid entries in this
 * map.
 * 
 * If dwSize is 0 or 1 the function returns TRUE.
 *
 * Returns TRUE if the checks work and FALSE otherwise.
 */
BOOL
sortAndCheckLayoutMap (sLayoutMap * map, DWORD dwSize);


#endif 
