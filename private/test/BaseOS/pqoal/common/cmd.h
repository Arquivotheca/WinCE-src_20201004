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
 * cmd.h
 */

#pragma once

#include <windows.h>

#include "commonUtils.h"

class cTokenControl
{
public:
  cTokenControl ();         /* null constructor */
  ~cTokenControl ();        /* null constructor */

  BOOL add (const TCHAR *const szStr, DWORD dwSizeWithNull);

  DWORD listLength ();

  TCHAR *& operator[] (const DWORD index);

private:
  DWORD dwLength;
  DWORD dwMaxLength;
  TCHAR ** ppszTokens;
};

BOOL
tokenize (const TCHAR *const szStr, cTokenControl * tokens);

BOOL
findTokenI(cTokenControl * tokens,
       const TCHAR *const szLookingFor,
       DWORD * pdwIndex);

BOOL
findTokenI(cTokenControl * tokens,
       const TCHAR *const szLookingFor, DWORD dwStartingIndex,
       DWORD * pdwIndex);

#define CMD_FOUND_IT 1
#define CMD_NOT_FOUND 2
#define CMD_ERROR 3
/*
 * returns true if it is found and false if it isn't found or can't convert
 */
DWORD
getOptionPlusTwo (cTokenControl * tokens,
          DWORD dwCurrSpot,
          const TCHAR *const szName,
          double * pdo1, double * pdo2);

BOOL
getOptionTimeUlonglong (cTokenControl * tokens,
            const TCHAR *const szCmdName,
            ULONGLONG * pullVal);
BOOL
getOptionDouble (cTokenControl * tokens,
         const TCHAR *const szCmdName,
         double * pdoVal);

BOOL
getOptionString (cTokenControl * tokens,
         const TCHAR *const szCmdName,
         TCHAR ** pszVal);
BOOL
isOptionPresent (cTokenControl * tokens,
         const TCHAR *const szOption);

BOOL
getOptionDWord (cTokenControl * tokens,
        const TCHAR *const szCmdName,
        DWORD * pdwVal);

