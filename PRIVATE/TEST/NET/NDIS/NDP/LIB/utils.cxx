//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include <winsock.h>
#include "utils.h"

#ifndef UNDER_CE

//Routines which define Safer versions of strings those are used in NDP or related to it.

STDAPI StringCchCopy(LPTSTR pszDest,
                     size_t cchDest,
                     LPCTSTR pszSrc)
{
	_tcsncpy(pszDest,pszSrc,cchDest);
	return S_OK;
}

STDAPI StringCchCat(LPTSTR pszDest,
                    size_t cchDest,
                    LPCTSTR pszSrc)
{
	_tcsncat( pszDest, pszSrc, cchDest );
	return S_OK;
}

STDAPI StringCchVPrintf(LPTSTR pszDest,
                        size_t cchDest,
                        LPCTSTR pszFormat,
                        va_list argList)
{
	_vsntprintf(pszDest, cchDest, pszFormat, argList );
	return S_OK;
}


#endif

//------------------------------------------------------------------------------

BOOL _IsOptionChar(TCHAR cInp)
{
   const char acOptionChars[] = { _T('/'), _T('-') };
   const int nOptionChars = 2;

   for (int i = 0; i < nOptionChars; i++)	{
      if (acOptionChars[i] == cInp) break;
   }
   return (i < nOptionChars);
}

//------------------------------------------------------------------------------

BOOL GetOptionFlag(int *pargc, LPTSTR argv[], LPCTSTR szOption)
{
   BOOL bResult = FALSE;
   int i = 0;

   while (i < *pargc) {
      if (_IsOptionChar(argv[i][0])) {
         if (_tcsicmp(&argv[i][1], szOption) == 0) {
            bResult = TRUE;
            break;
         }
      }
      i++;
   }

   if (bResult) {
      for (int j = i + 1; j < *pargc; j++) argv[j - 1] = argv[j];
      (*pargc)--;
   }
   return bResult;
}

//------------------------------------------------------------------------------

LPTSTR GetOptionString(int *pargc, LPTSTR argv[], LPCTSTR szOption)
{
   LPTSTR szResult = NULL;
   int i = 0;
   int nRemove = 1;

   while (i < *pargc) {
      if (_IsOptionChar(argv[i][0])) {
         if (_tcsicmp(&argv[i][1], szOption) == 0) {
            szResult = &argv[i][_tcslen(szOption) + 1];
            if (*szResult == _T('\0') && (i + 1) < *pargc) {
               szResult = argv[i + 1];
               nRemove++;
            }
            break;
         }
      }
      i++;
   }

   if (szResult != NULL) {
      for (int j = i + nRemove; j < *pargc; j++) argv[j - nRemove] = argv[j];
      (*pargc) -= nRemove;
   }
   return szResult;
}

//------------------------------------------------------------------------------

LONG GetOptionLong(int *pargc, LPTSTR argv[], LONG lDefault, LPCTSTR szOption)
{
   LPTSTR szValue = NULL;
   LONG nResult = 0;
   TCHAR cDigit = _T('\0');
   INT nBase = 10;
   BOOL bMinus = FALSE;

   szValue = GetOptionString(pargc, argv, szOption);
   if (szValue == NULL) {
      nResult = lDefault;
      goto cleanUp;
   }

   // Check minus sign
   if (*szValue == _T('-')) {
      bMinus = TRUE;
      szValue++;
   }

   // And plus sign
   if (*szValue == _T('+')) szValue++;

   if (
      *szValue == _T('0') && 
      (*(szValue + 1) == _T('x') || *(szValue + 1) == _T('X'))
   ) {
      nBase = 16;
      szValue += 2;
   }

   nResult = 0;
   while (TRUE) {
      if (*szValue >= _T('0') && *szValue <= _T('9')) {
         nResult = nResult * nBase + (*szValue - _T('0'));
         szValue++;
      } else if (nBase == 16) {
         if (*szValue >= _T('a') && *szValue <= _T('f')) {
            nResult = nResult * nBase + (*szValue - _T('a') + 10);
            szValue++;
         } else if (*szValue >= _T('A') && *szValue <= _T('F')) {
            nResult = nResult * nBase + (*szValue - _T('A') + 10);
            szValue++;
         } else {
            break;
         }
      } else {
         break;
      }
   }

   if (bMinus) nResult = -nResult;

cleanUp:
   return nResult;
}

//------------------------------------------------------------------------------

void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]) 
{
   int i, nArgc = 0;
   BOOL bInQuotes, bEndFound = FALSE;
   TCHAR *p = szCommandLine;

   if (szCommandLine == NULL || argc == NULL || argv == NULL) return;

   for (i = 0; i < *argc; i++) {
        
      // Clear our quote flag
      bInQuotes = FALSE;

      // Move past and zero out any leading whitespace
      while (*p && _istspace(*p)) *(p++) = _T('\0');

      // If the next character is a quote, move over it and remember it
      if (*p == _T('\"')) {
        *(p++) = _T('\0');
        bInQuotes = TRUE;
      }

      // Point the current argument to our current location
      argv[i] = p;

      // If this argument contains some text, then update our argument count
      if (*p != _T('\0')) nArgc = i + 1;

      // Move over valid text of this argument.
      while (*p != _T('\0')) {
         if (_istspace(*p) || (bInQuotes && (*p == _T('\"')))) {
            *(p++) = _T('\0');
            break;
         }
         p++;
      }
      
      // If reach end of string break;
      if (*p == _T('\0')) break;
   }
   *argc = nArgc;
}

TCHAR *
MultiSzToSz(OUT TCHAR * tsz,IN TCHAR *tszMulti)
//
//	Turn a multi_sz string into a single string, with spaces between
//	the components of the string.
//
{
	TCHAR *tszCurrSrc = tszMulti;
	TCHAR *tszCurrDest = tsz;

	while (*tszCurrSrc != TEXT('\0'))
	{
		_tcscpy(tszCurrDest,tszCurrSrc);

		// Find the end of the current string
		INT cbLen = _tcslen(tszCurrSrc);
		tszCurrSrc += cbLen+1;
		tszCurrDest += cbLen;

		// Replace the string terminator with a space char
		*tszCurrDest = TEXT(' ');

		// Advance to the next component of the multi_sz
		tszCurrDest++;
	}

	*tszCurrDest = TEXT('\0');
	return tsz;
}

//------------------------------------------------------------------------------
