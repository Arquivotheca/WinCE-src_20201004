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


#include <windows.h>
#include <tchar.h>

#include "prettyPrinting.h"

#define IRQ_LIST_LENGTH 256

TCHAR gIRQList[IRQ_LIST_LENGTH];

TCHAR *
collateIRQs (const DWORD *const dwIRQs, DWORD dwCount)
{
  /* null terminate */
  gIRQList[0] = 0;

  if (!dwIRQs || dwCount == 0)
    {
      /* return empty string */
      return (gIRQList);
    }

  DWORD dwCurrUsedLength = 0;

  for (DWORD i = 0; i < dwCount; i++)
    {
      _sntprintf_s (gIRQList + dwCurrUsedLength,
                   IRQ_LIST_LENGTH - dwCurrUsedLength, 
                   IRQ_LIST_LENGTH - dwCurrUsedLength -1,
                   _T("%u "), dwIRQs[i]);

      /* force null term in case we overwrote it */
      gIRQList[IRQ_LIST_LENGTH - 1] = 0;

      /* recalculate used length, ignoring null term */
      dwCurrUsedLength = _tcslen (gIRQList);
    }

  return (gIRQList);
}
