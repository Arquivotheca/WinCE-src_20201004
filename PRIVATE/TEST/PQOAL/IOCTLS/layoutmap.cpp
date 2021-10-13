//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * layoutMap.cpp
 */

#include "layoutMap.h"

/*
 * implementation of functions defined in header file
 */

/*******  Defines and Declarations local to this file  ************************/

/*
 * swap the contents of two data structures
 */
void
swapLayoutEntries (sLayoutMap * s1, sLayoutMap * s2);

/*******  Implementation  *****************************************************/

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
sortAndCheckLayoutMap (sLayoutMap * map, DWORD dwSize)
{
  if (map == NULL)
    {
      IntErr (_T("map == NULL"));
      return (BAD_VAL);
    }
 
  for (DWORD dwMainPos = 0; dwMainPos < dwSize; dwMainPos++)
    {
      for (DWORD dwInnerPos = dwMainPos + 1; dwInnerPos < dwSize; dwInnerPos++)
        {
          if (map[dwMainPos].dwLowerBound > map[dwInnerPos].dwLowerBound)
            {
              swapLayoutEntries (&(map[dwMainPos]), &(map[dwInnerPos]));
            }

          /*
           * this will happen only if we have a 
           * equal.
           */
          if (map[dwMainPos].dwLowerBound >= map[dwInnerPos].dwLowerBound)
            {
              // we have a problem
              Error (_T("Failed: ")
		     _T("Appears that we have overlapping bounds ")
		     _T("when checking layout."));
              return (FALSE);
            }
          
          /*
           * upper bound will always be greater than lower bound in a given
           * structure, so we check to see if the structures overlap.  We don't
           * check to equality because the upper bound numbers aren't 
           * inclusive.  An rva of 2000 with a size of 1000 can rest up against
           * an rva of 3000 with no problems.
           */
          if (map[dwMainPos].dwUpperBound > map[dwInnerPos].dwLowerBound)
            {
              Error (_T("Failed: ")
		     _T("Appears that two sections overlap in virtual ")
		     _T("memory.\n"));
              return (FALSE);
            }
          
        } // for dwInnerPos
    } // for dwMainPos

  return (TRUE);
}

/*
 * swap the contents of two data structures
 */
void
swapLayoutEntries (sLayoutMap * s1, sLayoutMap * s2)
{
  if ((s1 == NULL) || (s2 == NULL))
    {
      IntErr (_T("swapLayoutEntries: ((s1 == NULL) || (s2 == NULL))"));
      return;
    }

  sLayoutMap temp;
  
  memcpy (&temp, s1, sizeof (sLayoutMap));
  memcpy (s1, s2, sizeof (sLayoutMap));
  memcpy (s2, &temp, sizeof (sLayoutMap));
  
  return;
}

          
