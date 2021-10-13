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
/*
 * cmdLineParsing.cpp
 */

#include "commonUtils.h"

/*
 * szTime is the time to be converted.  *pullTimeS is the converted value.
 */
BOOL
strToSeconds (TCHAR * szTime, ULONGLONG * pullTimeS)
{  
  BOOL returnVal = FALSE;

  if (!szTime || !pullTimeS)
    {
      return FALSE;
    }

  /* szTimeValue now points to the string representing the argument */
  
  TCHAR * endPtr;

  /* use strtoul to convert the numerical part, if possible */
  /* 10 is the base */
  unsigned long lNumericalPart = _tcstoul (szTime, &endPtr, 10);

  if (!endPtr)
    {
      /* something went wrong */
      goto cleanAndReturn;
    }
  
  if (endPtr == szTimeValue)
    {
      /* no numerical part given */
      Error (_T("no numerical part found in %s"), szTimeValue);
      goto cleanAndReturn;
    }

  DWORD dwConversionFactor = 0;

  switch (*endPtr)
    {
    case _T('\0'):		/* no units denotes seconds */
    case _T('s'):
      dwConversionFactor = 1;
      break;
    case _T('m'):
      dwConversionFactor = 60;
      break;
    case _T('h'):
      dwConversionFactor = 60 * 60;
      break;
    case _T('d'):
      dwConversionFactor = 60 * 60 * 24;
      break;
    default:
      Error (_T("Can't parse the units identifier: %s"), endPtr);
      goto cleanAndReturn;
    }

  *pullTimeS = lNumericalPart * dwConversionFactor;

  returnVal = TRUE;

 cleanAndReturn:

  return (returnVal);

}


