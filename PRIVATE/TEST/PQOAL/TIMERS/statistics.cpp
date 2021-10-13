//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*
 * statistics.cpp
 */

#include "statistics.h"

BOOL CalcStatisticsFromList (
			     DWORD * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     )
{
  if (dwNumInList < 2)
    {
      return (FALSE);
    }

  double doSum = 0.0, prev;

  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      prev = doSum;
      doSum += (double) dwNumList[dwPos];

      /* 
       * check for overflow.  The second condition makes sure that
       * we didn't try to add the maximim double value, which should
       * result in the same value
       */
      if ((doSum < prev) || 
	  ((doSum == prev) && (dwNumList[dwPos] != 0)))
	{
	  /* found overflow */
	  return (FALSE);
	}
    }

  doAverage = doSum / (double) dwNumInList;

  double doSquaredDiff = 0.0;

  /* now calculate the stddev */
  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      double doVal = (double) dwNumList[dwPos];

      doSquaredDiff += (doVal - doAverage) * (doVal - doAverage);
    }

  double doVariance = doSquaredDiff / (dwNumInList - 1);

  assert (doVariance >= 0.0);

  doStdDev = sqrt (doVariance);

  return (TRUE);
}

BOOL CalcStatisticsFromList (
			     double * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     )
{
  if (dwNumInList < 2)
    {
      return (FALSE);
    }

  double doSum = 0.0, prev;

  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      prev = doSum;
      doSum += (double) dwNumList[dwPos];
    }

  doAverage = doSum / (double) dwNumInList;

  double doSquaredDiff = 0.0;

  /* now calculate the stddev */
  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      double doVal = (double) dwNumList[dwPos];

      doSquaredDiff += (doVal - doAverage) * (doVal - doAverage);
      
      assert (doSquaredDiff >= 0.0);
    }

  double doVariance = doSquaredDiff / (dwNumInList - 1);

  doStdDev = sqrt (doVariance);

  return (TRUE);
}

BOOL CalcStatisticsFromList (
			     ULONGLONG * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     )
{
  if (dwNumInList < 2)
    {
      return (FALSE);
    }

  double doSum = 0.0, prev;

  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      prev = doSum;
      doSum += (double) dwNumList[dwPos];

      /* 
       * check for overflow.  The second condition makes sure that
       * we didn't try to add the maximim double value, which should
       * result in the same value
       */
      if ((doSum < prev) || 
	  ((doSum == prev) && (dwNumList[dwPos] != 0)))
	{

	  /* found overflow */
	  return (FALSE);
	}
    }

  doAverage = doSum / (double) dwNumInList;

  double doSquaredDiff = 0.0;

  /* now calculate the stddev */
  for (DWORD dwPos = 0; dwPos < dwNumInList; dwPos++)
    {
      double doVal = (double) dwNumList[dwPos];

      doSquaredDiff += (doVal - doAverage) * (doVal - doAverage);
    }

  double doVariance = doSquaredDiff / (dwNumInList - 1);

  assert (doVariance >= 0.0);

  doStdDev = sqrt (doVariance);

  return (TRUE);
}
