//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*---------------------------------------------------------------------------*\
 *  module: path.cpp
\*---------------------------------------------------------------------------*/
#include "path.h"


UINT PathGetCharType(TCHAR ch)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    switch (ch) {
    case TEXT('|'):
    case TEXT('>'):
    case TEXT('<'):
    case TEXT('"'):
#ifdef UNDER_CE
	case TEXT(':'):       // CE doesn't have drives !
#endif
        return GCT_INVALID;

    case TEXT('?'):
    case TEXT('*'):
        return GCT_WILD;

    case TEXT('\\'):      // path separator
    case TEXT('/'):       // path sep
#ifndef UNDER_CE
	case TEXT(':'):       // drive colon
#endif
        return GCT_SEPERATOR;

    case TEXT(';'):
    case TEXT(','):
    case TEXT(' '):
        return GCT_LFNCHAR;     // actually valid in short names
                                // but we want to avoid this
    default:
        if (ch > TEXT(' '))
            return GCT_SHORTCHAR | GCT_LFNCHAR;
        else
            return GCT_INVALID;    // control character
    }

} /* PathGetCharType()
   */


BOOL WINAPI PathIsValidFileName(LPCTSTR pPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	if (pPath) {
		while (*pPath) {
			if ( ! (PathGetCharType(*pPath) & (GCT_SHORTCHAR|GCT_LFNCHAR))) {
				return FALSE;
			}
			pPath ++;
		}
		return TRUE;
	}

	return FALSE;
	
} /* PathIsValidFileName()
   */


BOOL WINAPI PathIsValidPath(LPCTSTR pPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	if (pPath) {
		while (*pPath) {
			if ( ! (PathGetCharType(*pPath) & (GCT_SHORTCHAR|GCT_LFNCHAR|GCT_SEPERATOR))) {
				return FALSE;
			}
			pPath ++;
		}
		return TRUE;
	}

	return FALSE;
	
} /* PathIsValidPath()
   */

	
BOOL WINAPI PathIsRemovableDevice(LPCTSTR lpszPath)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	TCHAR *psz, szPath[MAX_PATH];

	if (!lpszPath) {
		return FALSE;
	}

	if (!_tcslen(lpszPath)) {
		return FALSE;
	}
	
	_tcscpy(szPath, lpszPath);

	psz = szPath;
	// Skip the initial set of backslashes.
	while (*psz == TEXT('\\') || *psz == TEXT('/'))
		psz++;

	// Find the second backslash in the path.
	while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
		psz++;

	// Truncate here to leave us with the first component of the path.
	*psz = 0;

	return ((GetFileAttributes(szPath) & FILE_ATTRIBUTE_TEMPORARY) ? TRUE : FALSE);
	
} /* PathIsRemovableDevice()
   */


void WINAPI PathRemoveBlanks(LPTSTR lpszString)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPTSTR lpszPosn = lpszString;

	/* strip leading blanks */
    while(*lpszPosn == TEXT(' ')) {
        lpszPosn++;
    }
    if (lpszPosn != lpszString)
        lstrcpy(lpszString, lpszPosn);

    /* strip trailing blanks */
    for (lpszPosn=lpszString; *lpszString; lpszString=CharNext(lpszString))
    {
        if (*lpszString != TEXT(' '))
        {
            lpszPosn = lpszString;
        }
    }

    if (*lpszPosn)
    {
        *CharNext(lpszPosn) = TEXT('\0');
    }

} /* PathRemoveBlanks()
   */


void WINAPI PathRemoveTrailingSlashes(LPTSTR lpszString)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPTSTR lpszPosn = lpszString;

	if (lstrcmp(lpszString, TEXT("\\"))) {
		/* strip trailing slash */
		for (lpszPosn=lpszString; *lpszString; lpszString=CharNext(lpszString))
		{
			if (*lpszString != TEXT('\\'))
			{
				lpszPosn = lpszString;
			}
		}
		
		if (*lpszPosn)
		{
			*CharNext(lpszPosn) = TEXT('\0');
		}
	}
	
} /* PathRemoveTrailingSlashes()
   */


LPTSTR WINAPI PathGetArgs(LPCTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * returns a pointer to the arguments in a cmd type path or pointer to
 * NULL if no args exist
 * 
 * "foo.exe bar.txt"    -> "bar.txt"
 * "foo.exe"            -> ""
 * 
 * Spaces in filenames must be quoted.
 * " "A long name.txt" bar.txt " -> "bar.txt"
 * 
\*---------------------------------------------------------------------------*/
{
        BOOL fInQuotes = FALSE;

        if (!pszPath)
                return NULL;

        while (*pszPath)
        {
                if (*pszPath == TEXT('"'))
                        fInQuotes = !fInQuotes;
                else if (!fInQuotes && *pszPath == TEXT(' '))
                        return (LPTSTR)pszPath+1;
                pszPath = CharNext(pszPath);
        }

        return (LPTSTR)pszPath;
}


void WINAPI PathRemoveArgs(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    if (pszPath) {
        LPTSTR pArgs = PathGetArgs(pszPath);
        if (*pArgs)
            *(pArgs - 1) = TEXT('\0');   // clobber the ' '
        // Handle trailing space.
        else
        {
            pArgs = CharPrev(pszPath, pArgs);
            if (*pArgs == TEXT(' '))
                *pArgs = TEXT('\0');
        }
    }
}

void WINAPI PathRemoveQuotesAndArgs(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	LPTSTR szOrig = pszPath;
    if (!pszPath)
        return;

    if (*pszPath == TEXT('"'))
	{
		pszPath = CharNext(pszPath);
		while(*pszPath && *pszPath != TEXT('"')) {
			pszPath = CharNext(pszPath);
		}
		*pszPath = TEXT('\0');
		lstrcpy(szOrig, CharNext(szOrig));
	} else {
		PathRemoveArgs(pszPath);
	}
}

void WINAPI PathRemoveQuotes(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	LPTSTR szOrig = pszPath;
    if (!pszPath)
        return;

    if (*pszPath == TEXT('"'))
	{
		pszPath = CharNext(pszPath);
		while(*pszPath && *pszPath != TEXT('"')) {
			pszPath = CharNext(pszPath);
		}
		// That's right: If you put a quote in the middle of your
		// string we truncate at that point.
		if (*pszPath == L'"')
			*pszPath = TEXT('\0');
		lstrcpy(szOrig, CharNext(szOrig));
	}
}

// modify lpszPath in place so it fits within dx space (using the
// current font).  the base (file name) of the path is the minimal
// thing that will be left prepended with ellipses
//
// examples:
//      c:\foo\bar\bletch.txt -> c:\foo...\bletch.txt   -> TRUE
//      c:\foo\bar\bletch.txt -> c:...\bletch.txt       -> TRUE
//      c:\foo\bar\bletch.txt -> ...\bletch.txt         -> FALSE
//      relative-path         -> relative-...           -> TRUE
//
// in:
//      hDC         used to get font metrics
//      lpszPath    path to modify (in place)
//      dx          width in pixels
//
// returns:
//      TRUE    path was compacted to fit in dx
//      FALSE   base part didn't fit, the base part of the path was
//              bigger than dx

BOOL WINAPI PathCompactPath(HDC hDC, LPTSTR lpszPath, UINT dx)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
  int           len;
  UINT          dxFixed, dxEllipses;
  LPTSTR         lpEnd;          /* end of the unfixed string */
  LPTSTR         lpFixed;        /* start of text that we always display */
  BOOL          bEllipsesIn;
  SIZE sz;
  TCHAR szTemp[MAX_PATH];
  BOOL bRet = TRUE;
  HDC hdcGet = NULL;

  if (!hDC)
      hDC = hdcGet = GetDC(NULL);

  /* Does it already fit? */

  GetTextExtentPoint(hDC, lpszPath, lstrlen(lpszPath), &sz);
  if ((UINT)sz.cx <= dx)
      goto Exit;

  lpFixed = PathFindFileName(lpszPath);
  if (lpFixed != lpszPath)
      lpFixed = CharPrev(lpszPath, lpFixed);  // point at the slash

  /* Save this guy to prevent overlap. */
  _tcsncpy(szTemp, lpFixed, lengthof(szTemp) - sizeof(TCHAR));
  szTemp[lengthof(szTemp) - sizeof(TCHAR)] = 0;

  lpEnd = lpFixed;
  bEllipsesIn = FALSE;

  GetTextExtentPoint(hDC, lpFixed, lstrlen(lpFixed), &sz);
  dxFixed = sz.cx;

  GetTextExtentPoint(hDC, TEXT("..."), 3, &sz);
  dxEllipses = sz.cx;

  

    if (lpFixed == lpszPath) {
        // if we're just doing a file name, just tack on the ellipses at the end
        lpszPath = lpszPath + lstrlen(lpszPath);
        if ((3 + lpszPath - lpFixed) >= MAX_PATH)
            lpszPath = lpFixed + MAX_PATH - 4;

        for (;;) {
            lstrcpy(lpszPath, TEXT("..."));
            GetTextExtentPoint(hDC, lpFixed, 3 + lpszPath - lpFixed, &sz);
            if (sz.cx <= (int)dx)
                break;
            lpszPath--;
        }

    } else {
        for (;;) {

            GetTextExtentPoint(hDC, lpszPath, lpEnd - lpszPath, &sz);

            len = dxFixed + sz.cx;

            if (bEllipsesIn)
                len += dxEllipses;

			if (len <= (int)dx)
                break;

            bEllipsesIn = TRUE;

            if (lpEnd <= lpszPath) {
                /* Things didn't fit. */
                lstrcpy(lpszPath, TEXT("..."));
                lstrcat(lpszPath, szTemp);
                bRet = FALSE;
                goto Exit;
            }

            /* Step back a character. */
            lpEnd = CharPrev(lpszPath, lpEnd);
        }

        if (bEllipsesIn) {
            lstrcpy(lpEnd, TEXT("..."));
            lstrcat(lpEnd, szTemp);
        }
    }
Exit:
  if (hdcGet)
      ReleaseDC(NULL, hdcGet);

  return bRet;
}


LPTSTR WINAPI PathFindExtension(LPCTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPCTSTR pszDot;

    for (pszDot = NULL; *pszPath; pszPath = CharNext(pszPath))
    {
        switch (*pszPath) {
        case TEXT('.'):
            pszDot = pszPath;         // remember the last dot
            break;
        case TEXT('\\'):
        case TEXT(' '):         // extensions can't have spaces
            pszDot = NULL;       // forget last dot, it was in a directory
            break;
        }
    }

    // if we found the extension, return ptr to the dot, else
    // ptr to end of the string (NULL extension) (cast->non const)
    return pszDot ? (LPTSTR)pszDot : (LPTSTR)pszPath;
} /* PathFindExtension()
   */


LPTSTR WINAPI PathFindFileName(LPCTSTR pPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	LPCTSTR pT;
	
	for (pT = pPath; *pPath; pPath = CharNext(pPath)) {
		if ((pPath[0] == TEXT('\\') ||
			 pPath[0] == TEXT(':')) && pPath[1] && (pPath[1] != TEXT('\\')))
		{
			pT = pPath + 1;
		}
	}

    return (LPTSTR)pT;   // const -> non const
} /* PathFindFileName()
   */


void WINAPI PathStripPath(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPTSTR pszName = PathFindFileName(pszPath);

    if (pszName != pszPath) {
        lstrcpy(pszPath, pszName);
	}
	
} /* PathStripPath()
   */


void WINAPI PathCompactSlashes(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	LPTSTR psz = pszPath;
	BOOL fSep = FALSE;
	
	if (psz) {
		while (*psz) {
			if (!((*psz == TEXT('\\')) && fSep)) {
				*pszPath++ = *psz;
			}
			fSep = (*psz == TEXT('\\')) ? TRUE : FALSE;
			psz ++;
		}

		*pszPath = 0;
	}
	
} /* PathCompactSlashes()
   */

BOOL WINAPI PathIsDatabase(LPCTSTR pszPath)
{
	if (!pszPath)
		return FALSE;

	TCHAR const c_szDataBaseDir[] = TEXT("\\\003Databases"); // This should be somewhere common
	if (!_tcsnicmp(pszPath, c_szDataBaseDir, lstrlen(c_szDataBaseDir)) &&
		!PathFileExists(pszPath))
		return TRUE;
	else
		return FALSE;
}


BOOL WINAPI PathIsDirectory(LPCTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	if (!pszPath)
		return FALSE;

	DWORD dwAttribs = GetFileAttributes(pszPath);
	if (dwAttribs != (DWORD)-1) {
		return (BOOL)(dwAttribs & FILE_ATTRIBUTE_DIRECTORY);
	}
    return FALSE;
} /* PathIsDirectory()
   */

BOOL WINAPI PathGetAssociation(LPCTSTR pszPath, LPTSTR pszAssoc)
{
	HKEY hKey, hkeySub;
	DWORD dw, dwType;
	TCHAR szTemp[MAX_PATH];
	LPTSTR lpszExtension = PathFindExtension(pszPath);

	*pszAssoc = L'\0';

	if (*lpszExtension)
	{
		LONG lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, lpszExtension, 0, 0, &hKey);
		// Error means no association which is fine.
		if (lRet != ERROR_SUCCESS)
			return FALSE;

		// Fetch the file extension type.
		dw = MAX_PATH;
		lRet = RegQueryValueEx(hKey, NULL, 0, &dwType, (LPBYTE)szTemp, &dw);
		RegCloseKey(hKey);

		if (lRet != ERROR_SUCCESS)
			return FALSE;

		// Open the key for this extension type.
		lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, szTemp, 0, 0, &hKey);

		if (lRet != ERROR_SUCCESS)
			return FALSE;

		*szTemp = 0;
		lRet = RegOpenKeyEx(hKey, L"Shell\\Open\\Command", 0, 0, &hkeySub);

		if (lRet == ERROR_SUCCESS) {
			dw = MAX_PATH;
			RegQueryValueEx(hkeySub, NULL, NULL, &dwType, (LPBYTE) szTemp, &dw);
			RegCloseKey(hkeySub);
		}
		RegCloseKey(hKey);

		// We got it, so copy to the output buffer.
		lstrcpy(pszAssoc, szTemp);
	}

	return TRUE;

}


HRESULT WINAPI PathIsGUID(LPCTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	CLSID clsid;
	return CLSIDFromString((LPOLESTR)pszPath, &clsid);

} /* PathIsGUID()
   */


BOOL WINAPI PathOnExtList(LPCTSTR pszExtList, LPCTSTR pszExt)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    for (; *pszExtList; pszExtList += lstrlen(pszExtList) + 1) {
        if (!lstrcmpi(pszExt, pszExtList)) {
            return TRUE;        // yes
        }
    }

    return FALSE;
} /* OnExtList()
   */


#ifdef UNDER_CE
const TCHAR achExes[] = TEXT(".exe\0");
#else
const TCHAR achExes[] = TEXT(".bat\0.pif\0.exe\0.com\0");
#endif


BOOL WINAPI PathIsExe(LPCTSTR szFile)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPCTSTR temp = PathFindExtension(szFile);
    return PathOnExtList((LPCTSTR) achExes, temp);
} /* PathIsExe()
   */


BOOL WINAPI PathIsLink(LPCTSTR szFile)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    return lstrcmpi(TEXT(".lnk"), PathFindExtension(szFile)) == 0;
} /* PathIsLink()
   */


BOOL WINAPI PathIsExtension(LPCTSTR szFile, LPCTSTR szExt)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPCTSTR temp = PathFindExtension(szFile);

	if (!lstrcmpi(szExt, temp)) {
		return TRUE;
	}
	
	return FALSE;

} /* PathIsExtension()
   */

BOOL WINAPI PathMakePretty(LPTSTR lpPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPTSTR lp;

    // check for all uppercase
    for (lp = lpPath; *lp; lp = CharNext(lp)) {
        if ((*lp >= TEXT('a')) && (*lp <= TEXT('z')))
            return FALSE;       
    }

    CharLower(lpPath);
    CharUpperBuff(lpPath, 1);

    return TRUE;        // did the conversion
} /* PathMakePretty()
   */


BOOL WINAPI PathFileExists(LPCTSTR lpszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	return (UINT)GetFileAttributes(lpszPath) != (UINT)-1;
} /* PathFileExists()
   */


void WINAPI PathRemoveExtension(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
    LPTSTR pExt = PathFindExtension(pszPath);
    if (*pExt) {
        *pExt = 0;    // null out the "."
    }
	
} /* PathRemoveExtension()
   */


BOOL WINAPI PathRemoveFileSpec(LPTSTR pFile)
/*---------------------------------------------------------------------------*\
 * 
\*---------------------------------------------------------------------------*/
{
	LPTSTR pSZ = pFile;

	while (*pSZ != 0) {
		pSZ ++;
	}
		
	while (pSZ != pFile && *pSZ != TEXT('\\')) {
		pSZ --;
	}
	*pSZ = 0;

	return TRUE;
	
} /* PathRemoveFileSpec()
   */


BOOL WINAPI PathMatchSpec(LPCTSTR pszFileParam, LPCTSTR pszSpec)
/*---------------------------------------------------------------------------*\
 * Match a DOS wild card spec against a dos file name
\*---------------------------------------------------------------------------*/
{
    // Special case empty string, "*", and "*.*"...
    //
    if (*pszSpec == 0)
        return TRUE;

    do
    {
        LPCTSTR pszFile = pszFileParam;

        // Strip leading spaces from each spec.  This is mainly for commdlg
        // support;  the standard format that apps pass in for multiple specs
        // is something like "*.bmp; *.dib; *.pcx" for nicer presentation to
        // the user.
        while (*pszSpec == TEXT(' '))
            pszSpec++;

        while (*pszFile && *pszSpec && *pszSpec != TEXT(';'))
        {
            switch (*pszSpec)
            {
            case TEXT('?'):
                pszFile=CharNext(pszFile);
                pszSpec++;      // NLS: We know that this is a SBCS
                break;

            case TEXT('*'):

                // We found a * so see if this is the end of our file spec
                // or we have *.* as the end of spec, in which case we
                // can return true.
                //
                if (*(pszSpec + 1) == 0 || *(pszSpec + 1) == TEXT(';'))   // "*" matches everything
                    return TRUE;


                // Increment to the next character in the list
                pszSpec = CharNext(pszSpec);

                // If the next character is a . then short circuit the
                // recursion for performance reasons
                if (*pszSpec == TEXT('.'))
                {
                    pszSpec++;  // Get beyond the .

                    // Now see if this is the *.* case
                    if ((*pszSpec == TEXT('*')) &&
                            ((*(pszSpec+1) == TEXT('\0')) || (*(pszSpec+1) == TEXT(';'))))
                        return TRUE;

                    // find the extension (or end in the file name)
                    while (*pszFile)
                    {
                        // If the next char is a dot we try to match the
                        // part on down else we just increment to next item
                        if (*pszFile == TEXT('.'))
                        {
                            pszFile++;

                            if (PathMatchSpec(pszFile, pszSpec))
                                return(TRUE);

                        }
                        else
                            pszFile = CharNext(pszFile);
                    }

                    goto NoMatch;   // No item found so go to next pattern
                }
                else
                {
                    // Not simply looking for extension, so recurse through
                    // each of the characters until we find a match or the
                    // end of the file name
                    while (*pszFile)
                    {
                        // recurse on our self to see if we have a match
                        if (PathMatchSpec(pszFile, pszSpec))
                            return(TRUE);
                        pszFile = CharNext(pszFile);
                    }

                    goto NoMatch;   // No item found so go to next pattern
                }

            default:
                if (CharUpper((LPTSTR)(DWORD)*pszSpec) ==
                         CharUpper((LPTSTR)(DWORD)*pszFile))
                {
                    pszFile++;
                    pszSpec++;
                }
                else
                {
                    goto NoMatch;
                }
            }
        }

        // If we made it to the end of both strings, we have a match...
        //
        if (!*pszFile)
        {
            if ((!*pszSpec || *pszSpec == TEXT(';')))
                return TRUE;

            // Also special case if things like foo should match foo*
            // as well as foo*; for foo*.* or foo*.*;
            if ( (*pszSpec == TEXT('*')) &&
                ( (*(pszSpec+1) == TEXT('\0')) || (*(pszSpec+1) == TEXT(';')) ||
                    ((*(pszSpec+1) == TEXT('.')) &&  (*(pszSpec+2) == TEXT('*')) &&
                        ((*(pszSpec+3) == TEXT('\0')) || (*(pszSpec+3) == TEXT(';'))))))

                return TRUE;
        }

        // Skip to the end of the path spec...
        //
NoMatch:
        while (*pszSpec && *pszSpec != TEXT(';'))
            pszSpec = CharNext(pszSpec);

    // If we have more specs, keep looping...
    //
    } while (*pszSpec++ == TEXT(';'));

    return FALSE;
} /* PathMatchSpec()
   */


BOOL
WINAPI
PathRemoveStem(LPTSTR pszPath)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	LPTSTR pszRest, pszEndUniq;

	// NOTE: This function will remove the following "(number)", so the first
	//       thing this function will do is trim off the extension, if there
	//       is one. The next step is to remove a number enclosed in parenthesis.
	//       and then remove ther extra space, if there is one.
	//
	//        "\foo\bar (3).lnk" -->   "\foo\bar"

	PathRemoveExtension(pszPath);
	
	pszRest = _tcschr(pszPath, TEXT('('));
	while (pszRest)
	{
		// First validate that this is the right one
		pszEndUniq = CharNext(pszRest);
		while (*pszEndUniq && *pszEndUniq >= TEXT('0') && *pszEndUniq <= TEXT('9')) {
			pszEndUniq++;
		}
		if (*pszEndUniq == TEXT(')'))
			break;  // We have the right one!
		pszRest = _tcschr(CharNext(pszRest), TEXT('('));
	}

	if (pszRest) {
		if (pszRest != pszPath) {
			pszRest--;
			if (*pszRest != TEXT(' ')) {
				pszRest++;
			}
		}
		*pszRest = 0;
	}
	
	return TRUE;
	
} /* PathRemoveStem()
   */


BOOL
WINAPI
PathMakeUniqueName(LPCTSTR lpszPath,
				   LPCTSTR lpszT1,
				   LPCTSTR lpszT2,
				   BOOL    fLink,
				   LPTSTR  lpszUniqueName)
/*---------------------------------------------------------------------------*\
 *
 * NOTE: Win95 has a better implementation, but for the lack of a brain,
 *       I am just trying to implement this to solve our needs a simple as
 *       as possible. All this will do is make sure the path won't be bigger
 *       than MAX_PATH and also make sure that one doesn't already exist.
 * 
\*---------------------------------------------------------------------------*/
{
	int nItems = 1, nT1, nT2, nPath, nName, nCounter = 0;
	LPTSTR szPath = lpszUniqueName; // NOTE: This had better be MAX_PATH
	TCHAR szFile[MAX_PATH];
	
	if (!lpszPath || !lpszUniqueName) {
		return FALSE;
	}

	// NOTE: If either of these strings exist then there will also be a space
	//       involved.
	nT1 = (lpszT1) ? lstrlen(lpszT1) + 1 : 0;
	nT2 = (lpszT2) ? lstrlen(lpszT2) + 1 : 0;

	lstrcpy(szPath, lpszPath);
	PathRemoveFileSpec(szPath);
	
	// NOTE: Adding one to this to account for the slash after the path
	nPath = lstrlen(szPath) + 1;
 	
	// NOTE: So as not to go over MAX_PATH, I am imcluding the NULL terminator
	//       in this string count.
	if (fLink) {
		lstrcpy(szPath, PathFindFileName(lpszPath));
		PathRemoveStem(szPath);
		nName  = lstrlen(szPath) + 1;
		nName += lstrlen(TEXT(".lnk"));
	}else{
		nName  = lstrlen(PathFindFileName(lpszPath)) + 1;
	}
	
	do {
		if (nItems == 1) {
			nCounter = 0;
		}else if (nItems < 10) {
			nCounter = 4;
		}else if (nItems < 100) {
			nCounter = 5;
		}else{
			nCounter = 6;
		}

		if ((nPath + nT1 + nT2 + nName + nCounter) > MAX_PATH) {
			SetLastError(ERROR_FILENAME_EXCED_RANGE);
			return FALSE;
		}

		lstrcpy(szPath, lpszPath);
		PathRemoveFileSpec(szPath);
		
		lstrcat(szPath, TEXT("\\"));
		
		if (lpszT1) {
			//
			// This processing is added for compatible of Win95.
			//
			// [English]
			//	in. lpszT1 = "Shortcut to %s"
			//	    lpszT1 = "Copy", lpszT2 = "of"
			//      out.  "Shortcut to file (n)"
			//	      "Copy (n) of file"
			//
			// [Japanese]
			//	in. lpszT1 = "%s no Shortcut"
			//	    lpszT1 = "Copy", lpszT2 = "~"
			//      out.  "file no Shortcut (n)"
			//	      "Copy (n) ~ file"
			//
			lstrcpy(szFile, PathFindFileName(lpszPath));
			if (fLink) {
				PathRemoveExtension(szFile);
			}

			wsprintf(szPath+lstrlen(szPath), lpszT1, szFile);

			if (nItems > 1) {
				lstrcat(szPath, TEXT(" "));
				wsprintf(&szPath[lstrlen(szPath)], TEXT("(%d)"), nItems);
			}

			if (lpszT2) {
				lstrcat(szPath, TEXT(" "));
				lstrcat(szPath, lpszT2);
				lstrcat(szPath, TEXT(" "));
			}
			
			if (fLink) {
				lstrcat(szPath, TEXT(".lnk"));
			}else{
				lstrcat(szPath, szFile);
			}
			
		}else{	

			lstrcat(szPath, PathFindFileName(lpszPath));
			if (fLink) {
				PathRemoveStem(szPath);
			}
			
			if (nItems > 1) {
				wsprintf(&szPath[lstrlen(szPath)], TEXT(" (%d)"), nItems);
			}

			if (fLink) {
				lstrcat(szPath, TEXT(".lnk"));
			}
		}

		// NOTE: Sanity Check
		if (++nItems >= 1000) {
			return FALSE;
		}
		
	} while (GetFileAttributes(szPath) != (UINT)-1);

	RETAILMSG(0, (TEXT("PathMakeUniqueName: '%s'\r\n"), szPath));
	return TRUE;
	
} /* PathMakeUniqueName()
   */


LPCTSTR PathFindRootDevice(LPTSTR szPath)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	TCHAR *psz;
	int cLeadingSlashes = 0;

	psz = szPath;

	// Skip all initial slashes.
	while (*psz == TEXT('\\') || *psz == TEXT('/'))
	{
		cLeadingSlashes++;
		psz++;
	}

	// Find the next slash in the path.
	while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
		psz++;

	// Now we're at the end of the first component.  If this is a network
	// path we need to continue skipping ahead to the next slash.
	if (cLeadingSlashes > 1 && psz[0] && psz[1] != '\0')
	{
		psz++;
		while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
			psz++;
	}

	// Finally terminate the path.
	*psz = 0;	

	return szPath;
	
} /* PathFindRootDevice()
   */



BOOL PathIsSameDevice(LPCTSTR szPath1, LPCTSTR szPath2)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	TCHAR sz1[MAX_PATH], sz2[MAX_PATH];
	DWORD dw;
	
	_tcsncpy(sz1, szPath1, MAX_PATH - 1);
	sz1[MAX_PATH - 1] = 0;
	PathFindRootDevice(sz1);
	
	_tcsncpy(sz2, szPath2, MAX_PATH - 1);
	sz2[MAX_PATH - 1] = 0;
	PathFindRootDevice(sz2);

	// If either path name begins with slashes, then we have at least
	// one network path and can do a direct comparision.
	if (((sz1[0] == TEXT('\\') || sz1[0] == TEXT('/')) &&
		 (sz1[1] == TEXT('\\') || sz1[1] == TEXT('/'))) ||
		((sz2[0] == TEXT('\\') || sz2[0] == TEXT('/')) &&
		 (sz2[1] == TEXT('\\') || sz2[1] == TEXT('/'))))
		return (lstrcmpi(sz1, sz2) == 0);

	//NOTE: If the root is the same, then the device is the same!
	if (!lstrcmpi(sz1, sz2)) {
		return TRUE;
	}

	dw = GetFileAttributes(sz1);	
	if ((dw != (UINT)-1) && (dw & FILE_ATTRIBUTE_TEMPORARY)) {
		return FALSE;
	}

	dw = GetFileAttributes(sz2);
	if ((dw != (UINT)-1) && (dw & FILE_ATTRIBUTE_TEMPORARY)) {
		return FALSE;
	}
	return TRUE;
	
} /* PathIsSameDevice()
   */

