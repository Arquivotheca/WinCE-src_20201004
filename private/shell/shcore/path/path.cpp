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

#include "shcpriv.h"
#define _SHLWAPI_
#include <shlwapi.h>
#include <shcore.h>
#include <storemgr.h>
#include <extfile.h>
#include <miscsvcs.h>

///<topic name="pathapi" displayname="Path API">
///<summary>
///</summary>
///<topic_scope tref="shell"/>
///</topic>
///<topic_scope tref="pathapi">

BOOL WINAPI  PathIsNandFlash(LPCTSTR lpszPath);

/// <summary>
///   PathIsValidFileName determines whether a file name is valid for the system.
/// </summary>
/// <param name="pPath">File string to check</param>
/// <returns>
///   BOOL
///     True if pPath is a valid file name. False if it is not.
/// </returns>
/// <remarks>
///   <para>
///     The following characters are considered invalid in file names: '|'  '>'  '<'  '"'  ':'  '?'  '*'  '\'  '/'
///     The following characters are allowed, but not encouraged: ';'  ','  ' '
///   </para>
/// </remarks>
BOOL WINAPI PathIsValidFileName(LPCTSTR pPath)
{
    if (pPath)
    {
        while (*pPath)
        {
            if ( ! (PathGetCharType(*pPath) & (GCT_SHORTCHAR|GCT_LFNCHAR)))
            {
                return FALSE;
            }
            pPath ++;
        }
        return TRUE;
    }

    return FALSE;
}

/// <summary>
///   PathIsValidPath determines whether a file path is valid for the system.
/// </summary>
/// <param name="pPath">File path to check</param>
/// <returns>
///   BOOL
///     True if pPath is a valid file path. False if it is not.
/// </returns>
/// <remarks>
///   <para>
///     The following characters are considered invalid in file paths: '|'  '>'  '<'  '"'  ':'  '?'  '*'  '\'  '/'
///     The following characters are allowed, but not encouraged: ';'  ','  ' '
///   </para>
/// </remarks>
BOOL WINAPI PathIsValidPath(LPCTSTR pPath)
{
    if (pPath)
    {
        while (*pPath)
        {
            if ( ! (PathGetCharType(*pPath) & (GCT_SHORTCHAR|GCT_LFNCHAR|GCT_SEPERATOR)))
            {
                return FALSE;
            }
            pPath ++;
        }
        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI PathIsRemovableDevice(LPCTSTR lpszPath)
{
    BOOL bRet = FALSE;
    TCHAR *psz, szPath[MAX_PATH];
    CE_VOLUME_INFO VolumeInfo;
        
    if (!lpszPath ||
        (lpszPath[0] == L'\0') ||
        FAILED(StringCchCopy(szPath, lengthof(szPath), lpszPath)))
    {
        goto leave;
    }
    
    psz = szPath;
    
    // Skip the initial set of backslashes.
    while (*psz == TEXT('\\') || *psz == TEXT('/'))
    {
        psz++;
    }
    
    if (psz == szPath)
    {   // if there are no initial backslashes this isn't a full path
        goto leave;
    }
    
    // Find the second backslash in the path.
    while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
    {
        psz++;
    }
    
    // Truncate here to leave us with the first component of the path.
    *psz = TEXT('\0');
    
    VolumeInfo.cbSize = sizeof(CE_VOLUME_INFO);
    
    if (CeGetVolumeInfo(szPath, CeVolumeInfoLevelStandard, &VolumeInfo))
    {
        if (CE_VOLUME_ATTRIBUTE_REMOVABLE & VolumeInfo.dwAttributes)
        {
            bRet = TRUE;
        }
    }
    
leave:
    return bRet;
}

/// <summary>
///   PathRemoveTrailingSlashes removes trailing slashes from passed in string.
/// </summary>
/// <param name="lpszString">[IN][OUT] Path from which to remove trailing slashes</param>
/// <returns>
///   void
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If \path\test\\\ is passed in as lpszString, this string will be updated to \path\test
///   </para>
/// </remarks>
void WINAPI PathRemoveTrailingSlashes(LPTSTR lpszString)
{
    LPTSTR lpszPosn = lpszString;

    if (lstrcmp(lpszString, TEXT("\\")))
    {
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
}

/// <summary>
///   PathRemoveQuotesAndArgs removes quotes and arguments from passed in string.
/// </summary>
/// <param name="pszPath">[IN][OUT] Path from which to remove quotes and arguments</param>
/// <returns>
///   void
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If "\path\test\file.txt" arg1 arg2 arg3 is passed in as pszPath, 
///                 this string will be updated to \path\test\file.txt
///   </para>
/// </remarks>
void WINAPI PathRemoveQuotesAndArgs(LPTSTR pszPath)
{
    LPTSTR szOrig = pszPath;

    if (!pszPath)
    {
        return;
    }

    if (*pszPath == TEXT('"'))
    {
        pszPath = CharNext(pszPath);
        while(*pszPath && *pszPath != TEXT('"'))
        {
            pszPath = CharNext(pszPath);
        }
        *pszPath = TEXT('\0');
        StringCchCopy(szOrig, (pszPath-szOrig)+1, CharNext(szOrig));
    }
    else
    {
        PathRemoveArgs(pszPath);
    }
}

/// <summary>
///   PathRemoveQuotes removes quotes from passed in string.
/// </summary>
/// <param name="pszPath">[IN][OUT] Path from which to remove quotes</param>
/// <returns>
///   void
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If "\path\test\file.txt" is passed in as pszPath, 
///                 this string will be updated to \path\test\file.txt
///   </para>
/// </remarks>
void WINAPI PathRemoveQuotes(LPTSTR pszPath)
{
    LPTSTR szOrig = pszPath;
    if (!pszPath)
    {
        return;
    }

    if (*pszPath == TEXT('"'))
    {
        pszPath = CharNext(pszPath);
        while(*pszPath && *pszPath != TEXT('"'))
        {
            pszPath = CharNext(pszPath);
        }
        // That's right: If you put a quote in the middle of your
        // string we truncate at that point.
        if (*pszPath == L'"')
        {
            *pszPath = TEXT('\0');
        }
        StringCchCopy(szOrig, (pszPath-szOrig)+1, CharNext(szOrig));
    }
}

/// <summary>
///   PathCompactSlashes removes excess slashes from passed in string. 
/// </summary>
/// <param name="pszPath">[IN][OUT] Path from which to compact slashes</param>
/// <returns>
///   void
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If \\path\\test\\file\\\ is passed in as pszPath, 
///                 this string will be updated to \path\test\file\
///   </para>
/// </remarks>
void WINAPI PathCompactSlashes(LPTSTR pszPath)
{
    BOOL bSep = FALSE;
    int iSrc;
    int iDest;
    int cch;

    if (!pszPath)
    {
        return;
    }

    cch = wcslen(pszPath);
    for (iSrc = 0, iDest = 0; iSrc < cch; iSrc++)
    {
        if (!((pszPath[iSrc] == L'\\') && bSep))
        {
            pszPath[iDest] = pszPath[iSrc];
            iDest++;
        }
        bSep = (pszPath[iSrc] == L'\\');
    }

    pszPath[iDest] = L'\0';
}

BOOL WINAPI PathIsDatabase(LPCTSTR pszPath)
{
    if (!pszPath)
    {
        return FALSE;
    }

    TCHAR const c_szDataBaseDir[] = TEXT("\\\003Databases"); // This should be somewhere common
    if (!_tcsnicmp(pszPath, c_szDataBaseDir, lstrlen(c_szDataBaseDir)) &&
        !PathFileExists(pszPath))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/// <summary>
///   PathGetAssociation finds the registry file association for the passed in file path.
/// </summary>
/// <param name="pszPath">[IN] File path to get association for</param>
/// <param name="pszAssoc">[OUT] Output parameter for storing association</param>
/// <returns>
///   BOOL
///     Returns true if file extension has an association that was correctly returned.
///     Returns false otherwise.
/// </returns>
BOOL WINAPI PathGetAssociation(__in LPCTSTR pszPath, __out_ecount(MAX_PATH) LPTSTR pszAssoc)
{
    HKEY hKey, hkeySub;
    DWORD dw, dwType;
    TCHAR szTemp[MAX_PATH];
    LPTSTR lpszExtension;

    if ((NULL == pszPath) || (NULL == pszAssoc))
    {
        return FALSE;
    }

    lpszExtension = PathFindExtension(pszPath);
    *pszAssoc = L'\0';

    if (*lpszExtension)
    {
        LONG lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, lpszExtension, 0, 0, &hKey);
        // Error means no association which is fine.
        if (lRet != ERROR_SUCCESS)
        {
            return FALSE;
        }

        // Fetch the file extension type.
        dw = MAX_PATH;
        lRet = RegQueryValueEx(hKey, NULL, 0, &dwType, (LPBYTE)szTemp, &dw);
        RegCloseKey(hKey);

        // Ensure we are NULL terminated
        szTemp[MAX_PATH-1] = L'\0';

        if (lRet != ERROR_SUCCESS)
        {
            return FALSE;
        }

        // Open the key for this extension type.
        lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, szTemp, 0, 0, &hKey);

        if (lRet != ERROR_SUCCESS)
        {
            return FALSE;
        }

        *szTemp = 0;
        lRet = RegOpenKeyEx(hKey, L"Shell\\Open\\Command", 0, 0, &hkeySub);

        if (lRet == ERROR_SUCCESS)
        {
            dw = MAX_PATH;
            RegQueryValueEx(hkeySub, NULL, NULL, &dwType, (LPBYTE) szTemp, &dw);
            RegCloseKey(hkeySub);
        }
        RegCloseKey(hKey);

        // We got it, so copy to the output buffer.
        StringCchCopy(pszAssoc, MAX_PATH, szTemp);
    }

    return TRUE;

}

BOOL EatHexString(LPCTSTR lpsz, int Num, BOOL fDash)
{
    for (int i=0; i<Num; i++)
    {
        if (lpsz[i]>='0' && lpsz[i]<='9')
        {
            continue;
        }

        if (lpsz[i]>='A' && lpsz[i]<='F')
        {
            continue;
        }

        if (lpsz[i]>='a' && lpsz[i]<='f')
        {
            continue;
        }
        return FALSE;
    }

    if (!fDash)
    {
        return TRUE;
    }
    else 
    {
        return (lpsz[Num]=='-');
    }
}

/// <summary>
///   PathIsGUID verifies whether a passed in GUID is valid.
/// </summary>
/// <param name="lpsz">GUID to verify</param>
/// <returns>
///   BOOL
///     Returns true if the passed in GUID is valid.
///     Returns false otherwise.
/// </returns>
HRESULT WINAPI PathIsGUID(LPCTSTR lpsz)
{
    if( (lpsz[0]  == '{' ) &&
        (lpsz[37] == '}' ) &&
        (lpsz[38] == 0 ) &&
        EatHexString(lpsz+1, 8, TRUE) &&
        EatHexString(lpsz+10, 4, TRUE) &&
        EatHexString(lpsz+15, 4, TRUE) &&
        EatHexString(lpsz+20, 4, TRUE) &&
        EatHexString(lpsz+25, 12, FALSE) )
    {
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

BOOL WINAPI PathOnExtList(LPCTSTR pszExtList, LPCTSTR pszExt)
{
    BOOL bRet = FALSE;

    if (!pszExtList || !pszExt)
    {
        goto leave;
    }

    for (; *pszExtList; pszExtList += lstrlen(pszExtList) + 1)
    {
        if (0 == wcsicmp(pszExt, pszExtList))
        {
            bRet = TRUE;        // yes
            break;
        }
    }

leave:
    return bRet;
}

#ifdef UNDER_CE
const TCHAR achExes[] = TEXT(".exe\0");
#else
const TCHAR achExes[] = TEXT(".bat\0.pif\0.exe\0.com\0");
#endif

/// <summary>
///   PathIsExe verifies whether a passed in file path is an exe.
/// </summary>
/// <param name="szFile">File path to verify as to whether it is an exe</param>
/// <returns>
///   BOOL
///     Returns true if the passed in file path is an exe.
///     Returns false otherwise.
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If \path\test\file.exe is passed in, true should be returned.
///   </para>
/// </remarks>
BOOL WINAPI PathIsExe(LPCTSTR szFile)
{
    BOOL bRet = FALSE;

    if (!szFile)
    {
        goto leave;
    }

    bRet = PathOnExtList((LPCWSTR) achExes, PathFindExtension(szFile));

leave:
    return bRet;
}

/// <summary>
///   PathIsLink verifies whether a passed in file path is a link.
/// </summary>
/// <param name="szFile">File path to verify as to whether it is a link</param>
/// <returns>
///   BOOL
///     Returns true if the passed in file path is a link.
///     Returns false otherwise.
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If \path\link.lnk is passed in, true should be returned.
///   </para>
/// </remarks>
BOOL WINAPI PathIsLink(LPCTSTR szFile)
{
    BOOL bRet = FALSE;

    if (!szFile)
    {
        goto leave;
    }

    bRet = (0 == wcsicmp(L".lnk", PathFindExtension(szFile)));

leave:
    return bRet;
}

const TCHAR c_aszNonImages[] = TEXT(".exe\0.dll\0");
const TCHAR c_aszImages[] = TEXT(".bmp\0.gif\0.jpg\0.jpe\0.jpeg\0.ico\0.png\0.2bp\0.wbmp\0");
const TCHAR c_szPerceivedType[] = TEXT("PerceivedType");
const TCHAR c_szImage[] = TEXT("image");

BOOL WINAPI PathIsImage(LPCTSTR szFile)
{
    HKEY hKey = NULL;
    DWORD dw, dwType;
    TCHAR szTemp[MAX_PATH];
    LPTSTR pszExtension = PathFindExtension(szFile);
    LONG lRet;
    
    if (NULL == pszExtension || TEXT('\0') == *pszExtension)
    {

        return FALSE;
    }

    // Filter out most non-image types
    if (PathOnExtList((LPCWSTR)c_aszNonImages, pszExtension))
    {
        return FALSE;
    }

    // Check the known image types
    if (PathOnExtList((LPCWSTR)c_aszImages, pszExtension))
    {
        return TRUE;
    }

    // Unknown type. Check the info from registry...
    lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, pszExtension, 0, 0, &hKey);
    // Error means no association which is not a image.
    if (lRet != ERROR_SUCCESS)
    {
        return FALSE;
    }

    // Fetch the file perceived type.
    dw = MAX_PATH;
    lRet = RegQueryValueEx(hKey, c_szPerceivedType, 0, &dwType, (LPBYTE)szTemp, &dw);
    RegCloseKey(hKey);

    // Ensure we are NULL terminated
    szTemp[MAX_PATH-1] = L'\0';

    if (lRet != ERROR_SUCCESS)
    {
        return FALSE;
    }


    return (0 == lstrcmpi(c_szImage, szTemp));
}

/// <summary>
///   PathIsExtension verifies that the extension of the passed in file path matches
///   the passed in extension.
/// </summary>
/// <param name="szFile">File path to verify as to whether it is an extension</param>
/// <param name="szExt">Extension to match with szFile</param>
/// <returns>
///   BOOL
///     Returns true if the passed in file path matches with the passed in extension.
///     Returns false otherwise.
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: If PathIsExtension(TEXT("\path\\test.exe"), TEXT(".exe")) is called, true should be returned.
///   </para>
/// </remarks>
BOOL WINAPI PathIsExtension(LPCTSTR szFile, LPCTSTR szExt)
{
    BOOL bRet = FALSE;

    if (!szFile || !szExt)
    {
        goto leave;
    }

    bRet = (0 == wcsicmp(szExt, PathFindExtension(szFile)));

leave:
    return bRet;
}

// helper for PathMatchSpec.
// originally PathMatchSpec had this logic embedded in it and it recursively called itself.
// only problem is the recursion picked up all the extra specs, so for example
// PathMatchSpec("foo....txt", "*.txt;*.a;*.b;*.c;*.d;*.e;*.f;*.g") called itself too much
// and wound up being O(N^3).
// in fact this logic doesnt match strings efficiently, but we shipped it so leave it be.
// just test one spec at a time and its all good.
// pszSpec is a pointer within the pszSpec passed to PathMatchSpec so we terminate at ';' in addition to '\0'.
BOOL PathMatchSingleSpec(LPCTSTR pszFileParam, LPCTSTR pszSpec)
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
            pszFile = pszFile + 1;
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
            pszSpec = pszSpec + 1;

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

                        if (PathMatchSingleSpec(pszFile, pszSpec))
                            return TRUE;

                    }
                    else
                        pszFile = pszFile + 1;
                }

                return FALSE;   // No item found so go to next pattern
            }
            else
            {
                // Not simply looking for extension, so recurse through
                // each of the characters until we find a match or the
                // end of the file name
                while (*pszFile)
                {
                    // recurse on our self to see if we have a match
                    if (PathMatchSingleSpec(pszFile, pszSpec))
                        return TRUE;
                    pszFile = pszFile + 1;
                }

                return FALSE;   // No item found so go to next pattern
            }

        default:
            if (CharUpper((TCHAR*)*pszSpec) == CharUpper((TCHAR*)*pszFile))
            {
                pszFile++;
                pszSpec++;
            }
            else
            {
                return FALSE;
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
    return FALSE;
}


BOOL WINAPI PathMatchSpec(LPCTSTR pszFileParam, LPCTSTR pszSpec)
{
    if (pszSpec && pszFileParam)
    {
        // Special case empty string, "*", and "*.*"...
        //
        if (*pszSpec == 0)
        {
            return TRUE;
        }

        // loop over the spec, break off at ';', and call our helper for each.
        do
        {
            if (PathMatchSingleSpec(pszFileParam, pszSpec))
                return TRUE;

            // Skip to the end of the path spec...
            while (*pszSpec && *pszSpec != TEXT(';'))
                pszSpec = pszSpec + 1;

        // If we have more specs, keep looping...
        } while (*pszSpec++ == TEXT(';'));
    }

    return FALSE;
}



BOOL
WINAPI
PathRemoveStem(LPTSTR pszPath, BOOL bIsDirectory)
{
    LPTSTR pszRest, pszEndUniq;

    // NOTE: This function will remove the following "(number)", so the first
    //       thing this function will do is trim off the extension, if there
    //       is one. The next step is to remove a number enclosed in parenthesis.
    //       and then remove ther extra space, if there is one.
    //
    //        "\foo\bar (3).lnk" -->   "\foo\bar"

    if (!bIsDirectory)
    {
        PathRemoveExtension(pszPath);
    }

    pszRest = _tcschr(pszPath, TEXT('('));
    while (pszRest)
    {
        // First validate that this is the right one
        pszEndUniq = CharNext(pszRest);
        while (*pszEndUniq && *pszEndUniq >= TEXT('0') && *pszEndUniq <= TEXT('9'))
        {
            pszEndUniq++;
        }

        if (*pszEndUniq == TEXT(')'))
        {
            break;  // We have the right one!
        }
        pszRest = _tcschr(CharNext(pszRest), TEXT('('));
    }

    if (pszRest)
    {
        if (pszRest != pszPath)
        {
            pszRest--;
            if (*pszRest != TEXT(' '))
            {
                pszRest++;
            }
        }
        *pszRest = 0;
    }

    return TRUE;
}

/// <summary>
///   PathMakeUniqueName verifies that the passed in file path is unique on the system.
///   If the file is not unique, a unique file name is found and returned (usually the passed
///   in file with a number appended).
///   If lpszT1 and/or lpszT2 are present, they can be used in combination to create filenames.
///     If lpszT1 = "Shortcut to %s" and lpszT2 = NULL, output = "Shortcut to file (n)"
///     If lpszT1 = "Copy", lpszT2 = "of", output = "Copy (n) of file"
/// </summary>
/// <param name="lpszPath">File path to verify for a unique name</param>
/// <param name="lpszT1">Template for first string</param>
/// <param name="lpszT2">Template for second string</param>
/// <param name="bLink"></param>
/// <param name="lpszUniqueName">Unique file name based on lpszPath</param>
/// <param name="cchUniqueName">Size of the buffer that lpszUniqueName points to</param>
/// <returns>
///   BOOL
///     Returns true if the a unique name is returned.
///     Returns false otherwise.
/// </returns>
BOOL
WINAPI
PathMakeUniqueName(LPCTSTR lpszPath,
                   LPCTSTR lpszT1,
                   LPCTSTR lpszT2,
                   BOOL    bLink,
                   __out_ecount(cchUniqueName) LPTSTR  lpszUniqueName,
                   UINT    cchUniqueName)
{
    return PathMakeUniqueNameEx(lpszPath,
                lpszT1,
                lpszT2,
                bLink,
                FALSE,
                lpszUniqueName,
                cchUniqueName);

}

BOOL
WINAPI
PathMakeUniqueNameEx(LPCTSTR lpszPath,
                     LPCTSTR lpszT1,
                     LPCTSTR lpszT2,
                     BOOL    bLink,
                     BOOL    bSourceIsDirectory,
                     __out_ecount(cchUniqueName) LPTSTR  lpszUniqueName,
                     UINT    cchUniqueName)
{
    BOOL bRet = FALSE;
    int nItems = 1, nT1, nT2, nPath, nName, nCounter = 0;
    LPTSTR szPath = lpszUniqueName;
    UINT cchPath = cchUniqueName;
    LPTSTR szDisplayName;
    LPCTSTR szParsePath = lpszPath;
    TCHAR szFile[MAX_PATH];
    TCHAR szBuf[MAX_PATH];
    SHFILEINFO sfiFile;

    // Path shouldn't be bigger than MAX_PATH
    ASSERT(MAX_PATH >= cchPath);

    if (!lpszPath || !lpszUniqueName)
    {
        goto leave;
    }

    // NOTE: If either of these strings exist then there will also be a space
    //       involved.
    nT1 = (lpszT1) ? lstrlen(lpszT1) + 1 : 0;
    nT2 = (lpszT2) ? lstrlen(lpszT2) + 1 : 0;

    // By default, the display name is assumed to be the filename from the
    // path string passed in to the function - however, we try to get the actual
    // display name by calling SHGetFileInfo
    szDisplayName = PathFindFileName(lpszPath);
    if(SHGetFileInfo(lpszPath, 0, &sfiFile, sizeof(sfiFile), SHGFI_DISPLAYNAME))
    {
        szDisplayName = sfiFile.szDisplayName;
    }
 
    StringCchCopy(szPath, cchPath, lpszPath);
    PathRemoveFileSpec(szPath);

    // NOTE: Adding one to this to account for the slash after the path
    nPath = lstrlen(szPath) + 1;

    // NOTE: So as not to go over MAX_PATH, I am imcluding the NULL terminator
    //       in this string count.
    if (bLink)
    {
        StringCchCopy(szPath, cchPath, szDisplayName);
        PathRemoveStem(szPath, bSourceIsDirectory);
        nName = lstrlen(szPath) + 1;
        nName += 4;  // extension ".lnk"
    }
    else
    {
        nName  = lstrlen(PathFindFileName(lpszPath)) + 1;
    }

    do
    {
        if (nItems == 1)
        {
            nCounter = 0;
        }
        else if (nItems < 10)
        {
            nCounter = 4;
        }
        else if (nItems < 100)
        {
            nCounter = 5;
        }
        else
        {
            nCounter = 6;
        }

        if ((nPath + nT1 + nT2 + nName + nCounter) > MAX_PATH)
        {
            SetLastError(ERROR_FILENAME_EXCED_RANGE);
            goto leave;
        }

        StringCchCopy(szPath, cchPath, lpszPath);
        PathRemoveTrailingSlashes(szPath);
        PathRemoveFileSpec(szPath);
        PathAddBackslash(szPath);

        if (lpszT1)
        {
            // This processing is added for compatible of Win95.
            //
            // [English]
            //    in. lpszT1 = "Shortcut to %s"
            //        lpszT1 = "Copy", lpszT2 = "of"
            //      out.  "Shortcut to file (n)"
            //          "Copy (n) of file"
            //
            // [Japanese]
            //    in. lpszT1 = "%s no Shortcut"
            //        lpszT1 = "Copy", lpszT2 = "~"
            //      out.  "file no Shortcut (n)"
            //          "Copy (n) ~ file"

            if (FAILED(StringCchCopy(szFile,
                            lengthof(szFile),
                            PathFindFileName(lpszPath))))
            {
                goto leave;
            }

            if (bLink && !bSourceIsDirectory)
            {
                PathRemoveExtension(szFile);
            }

            StringCchPrintf(szBuf, lengthof(szBuf), lpszT1, szFile);
            StringCchCat(szPath, cchPath, szBuf);

            if (nItems > 1)
            {
                StringCchCat(szPath, cchPath, TEXT(" "));
                StringCchPrintf(szBuf, lengthof(szBuf), TEXT("(%d)"), nItems);
                StringCchCat(szPath, cchPath, szBuf);
            }

            if (lpszT2)
            {
                StringCchCat(szPath, cchPath, TEXT(" "));
                StringCchCat(szPath, cchPath, lpszT2);
                StringCchCat(szPath, cchPath, TEXT(" "));
            }

            if (bLink)
            {
                StringCchCat(szPath, cchPath, TEXT(".lnk"));
            }
            else
            {
                StringCchCat(szPath, cchPath, szFile);
            }
            szParsePath = szPath;
        }
        else
        {
            StringCchCat(szPath, cchPath, szDisplayName);
            PathRemoveExtension(szPath);
            
            if (bLink)
            {
                PathRemoveStem(szPath, bSourceIsDirectory);
            }

            if (nItems > 1)
            {
                StringCchPrintf(szBuf, lengthof(szBuf), TEXT(" (%d)"), nItems);
                StringCchCat(szPath, cchPath, szBuf);
                szParsePath = szPath;
            }

            if (bLink)
            {
                StringCchCat(szPath, cchPath, TEXT(".lnk"));
            }
            else
            {
                StringCchCat(szPath, cchPath, PathFindExtension(lpszPath));
            }
        }

        // NOTE: Sanity Check
        if (++nItems >= 1000)
        {
            goto leave;
        }

    } while (GetFileAttributes(szParsePath) != (UINT)-1);

    RETAILMSG(0, (TEXT("PathMakeUniqueName: '%s'\r\n"), szPath));
    bRet = TRUE;

leave:
    return bRet;
}

BOOL PathIsSameDevice(LPCTSTR pszPath1, LPCTSTR pszPath2)
{
    static const lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    TCHAR sz1[MAX_PATH];
    TCHAR sz2[MAX_PATH];

    StringCchCopy(sz1, lengthof(sz1), pszPath1);
    StringCchCopy(sz2, lengthof(sz2), pszPath2);

    PathFindRootDevice(sz1);
    PathFindRootDevice(sz2);

    return (CSTR_EQUAL == ::CompareString(lcid, NORM_IGNORECASE, sz1, -1, sz2, -1));
}

BOOL PathIsLocal(LPCTSTR pszPath)
{
    static const lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    size_t cch;

    // Check for a network share (\\foo\bar)
    if (PathIsUNC(pszPath))
    {
        return FALSE;
    }

    // Check for a storage card (\Storage Card)
    if (PathIsRemovableDevice(pszPath))
    {
        return FALSE;
    }

    // Check for CE style network path (\network)
    CEOIDINFO ceOidInfo = {0};
    if (::CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK), &ceOidInfo))
    {
        cch = _tcslen(ceOidInfo.infDirectory.szDirName);
        if (CSTR_EQUAL == ::CompareString(lcid, NORM_IGNORECASE, ceOidInfo.infDirectory.szDirName, cch, pszPath, cch))
        {
            if ((TEXT('\0') == pszPath[cch]) || (TEXT('\\') == pszPath[cch]))
            {
                return FALSE;
            }
        }
    }

    // Check for release directory (\release)
    HANDLE hRelease = CreateEvent(NULL, TRUE, FALSE, TEXT("ReleaseFSD"));
    if (NULL != hRelease)
    {
        CloseHandle(hRelease);

        cch = 8; // _tcslen(TEXT("\\release"));
        if (CSTR_EQUAL == ::CompareString(lcid, NORM_IGNORECASE, TEXT("\\release"), cch, pszPath, cch))
        {
            if ((TEXT('\0') == pszPath[cch]) || (TEXT('\\') == pszPath[cch]))
            {
                return FALSE;
            }
        }
    }

    // Check for "Nand Flash" or "Object Store"
    if (PathIsNandFlash(pszPath))
    {
        return FALSE;
    }


    return TRUE;
}

/// <summary>
///   PathFindRootDevice returns the root of the passed in path.
/// </summary>
/// <param name="pszPath">File path with root to find</param>
/// <returns>
///   LPCTSTR
///     On success returns the the path to the root, on failure returns NULL
/// </returns>
/// <remarks>
///   <para>
///     EXAMPLE: "\directory\file.txt"               returns "\"
///              "\release\directory\file.txt"       returns "\release"
///              "\storage card\directory\file.txt"  returns "\storage card"
///              "\\server\share\directory\file.txt" returns "\\server\share"
///   </para>
/// </remarks>
LPCTSTR PathFindRootDevice(LPTSTR pszPath)
{
    if ((NULL == pszPath) || (TEXT('\0') == *pszPath) || (TEXT('\\') != *pszPath))
    {
        // Bad string or no fully qualified path
        return NULL;
    }

    if (!PathIsLocal(pszPath))
    {
        LPTSTR psz = pszPath;

        if (PathIsUNC(psz))
        {
            psz += 2; // Skip the "\\"
   
            // Skip the sever name.
            while ((TEXT('\0') != *psz) && (TEXT('\\') != *psz))
            {
                psz++;
            }
        }

        // Skip over the initial slash
        if (TEXT('\\') == *psz)
        {
            psz++;
        }

        // Find the next slash in the path.
        while ((TEXT('\0') != *psz) && (TEXT('\\') != *psz))
        {
            psz++;
        }

        // Finally terminate the path.
        *psz = TEXT('\0');
    }
    else
    {
        // local path, just chop off everything after the initial '\'
        pszPath[1] = TEXT('\0');
    }

    return pszPath;
}

///</topic_scope> pathapi

/***************************************************************************
    Helper used by FixupPath

    Fails if it doesn't find the prefix

***************************************************************************/
HRESULT ReplacePrefix(
    const TCHAR* pszPrefix, // in - string to look for as the prefix
    const TCHAR* pszReplace, // in - string to replace the prefix with
    const TCHAR* pszSearch, // in - string to search
    TCHAR* pszOut, // out - buffer where we put the fixed up string
    int cchOut // in - size of output buffer
    )
{
    HRESULT hr = S_OK;
    int cchPrefix = _tcslen(pszPrefix);
    TCHAR szTemp[MAX_PATH]; // need a temp buffer since pszSearch and pszOut may point to the same memory and I'm too lazy to deal with the ins and outs of doing it inplace
    TCHAR* pszTemp;
    size_t cchRemaining;

    if (0 != _tcsnicmp(pszPrefix, pszSearch, cchPrefix))
    {
        hr = E_FAIL;
        goto Error;
    }

    hr = StringCchCopyEx(szTemp, 
        lengthof(szTemp), 
        pszReplace, 
        &pszTemp, 
        &cchRemaining, 
        STRSAFE_IGNORE_NULLS);
    if (FAILED(hr))
    {
        goto Error;
    }
    hr = StringCchCopyEx(pszTemp, 
        cchRemaining, 
        pszSearch + cchPrefix, 
        NULL, 
        NULL, 
        STRSAFE_IGNORE_NULLS);
    if (FAILED(hr))
    {
        goto Error;
    }

    hr = StringCchCopy(pszOut, cchOut, szTemp);

Error:
    return(hr);
}

/***************************************************************************
    Replaces outdated path elements with the correct path.
***************************************************************************/
HRESULT FixupPathEx(
    TCHAR* pszStorage, // in - root directory of the persistent store.  We may modify this.
    const TCHAR* pszPath, // in - path that possibly starts with \IPSM
    TCHAR* pszPathOut, // out - buffer where we put the fixed up path
    int cchOut // in - size of output buffer
    )
{
    HRESULT hr = S_OK;
    TCHAR szStoragePath[MAX_PATH];

    // Fixup the storage location.  The path of the
    // storage doesn't seem to be very locked down
    // (that is, it is not specific about where the \'s
    // should be) so we make it how we want.

    // make sure it is prefixed with a "\"
    if(pszStorage[0] != TEXT('\\'))
    {
        hr = StringCchPrintf(szStoragePath, 
                lengthof(szStoragePath), 
                TEXT("\\%s"), 
                pszStorage);
    }
    else
    {
        hr = StringCchCopy(szStoragePath, lengthof(szStoragePath), pszStorage);
    }
    if (FAILED(hr))
    {
        goto Error;
    }

    // Add a \ if the storage name does not have it.
    PathAddBackslash(szStoragePath);

    // OK, replace stuff
    if(FAILED(ReplacePrefix(TEXT("\\IPSM\\"), szStoragePath, pszPath, pszPathOut, cchOut)))
    {
        if(FAILED(ReplacePrefix(TEXT("IPSM\\"), szStoragePath, pszPath, pszPathOut, cchOut)))
        {
            if(FAILED(ReplacePrefix(TEXT("\\Storage\\"), szStoragePath, pszPath, pszPathOut, cchOut)))
            {
                if(FAILED(ReplacePrefix(TEXT("Storage\\"), szStoragePath, pszPath, pszPathOut, cchOut)))
                {
                    hr = E_FAIL; // didn't fixup a path.
                }
            }
        }
    }
    
Error:
    return(hr);
}

/// <summary>
///   FixupPath replaces outdated path elements with the correct path.
/// </summary>
/// <param name="pszPath">path that possibly starts with \IPSM</param>
/// <param name="pszPathOut">buffer where we put the fixed up path</param>
/// <param name="cchOut">size of output buffer</param>
/// <returns>
///   HRESULT
///     S_OK on success, HRESULT error otherwise
/// </returns>
HRESULT FixupPath(
    const TCHAR* pszPath, // in - path that possibly starts with \IPSM
    TCHAR* pszPathOut, // out - buffer where we put the fixed up path
    int cchOut // in - size of output buffer
    )
{
    HRESULT hr = S_OK;
    TCHAR szStorage[100];

    // Interesting, GetDataDirectory returns the number of characters copied
    // on success and the number of characters needed on failure.  If it fails
    // for some reason other than buffer too small, you have no way of knowing.
    szStorage[0] = 0;

    // -3 because FixupPath may append some stuff on to szStorage later...
    if (0 == GetDataDirectory(szStorage, 
        lengthof(szStorage)) < (lengthof(szStorage) - 3)) 
    {
        goto Error;
    }
    if (0 == szStorage[0])
    {
        goto Error;
    }

    hr = FixupPathEx(szStorage, pszPath, pszPathOut, cchOut);
  
Error:
    return(hr);
}

//----------------------------------------------------------------------------
inline BOOL DirectoryExists(LPCTSTR pszDirectory)
{
    DWORD dwAttr;
    dwAttr = GetFileAttributes(pszDirectory);
    if (dwAttr == (DWORD)-1)
    {
        return FALSE;
    }

    return ((dwAttr & FILE_ATTRIBUTE_DIRECTORY)!=0);
}



/// <summary>
///   CreateDirectoryPath creates a directory path if it doesn't already exist.
/// </summary>
/// <param name="_pszPath">Path to desired directory</param>
/// <returns>
///   HRESULT
///     S_OK on success, HRESULT error otherwise
/// </returns>
HRESULT CreateDirectoryPath(LPCWSTR _pszPath)
{
    HRESULT hr = S_OK;

    if ((!_pszPath) || (!_pszPath[0]))
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    // If the directory already exists, nothing to do
    if (DirectoryExists(_pszPath))
    {
        goto Exit;
    }

    TCHAR szOrig[MAX_PATH];

    StringCchCopy(szOrig, lengthof(szOrig), _pszPath);

    LPTSTR pszPath = szOrig;

    // Go past initial \ character
    if (L'\\' == *pszPath)
    {
        pszPath++;
    }

    // Try creating each directory in the path
    // Don't worry about errors for now
    for (; *pszPath; pszPath++)
    {
        if (L'\\' == *pszPath)
        {
            *pszPath = L'\0';
            CreateDirectory(szOrig, NULL);
            *pszPath = L'\\';
        }
    }

    CreateDirectory(szOrig, NULL);

    // Verify that the final directory exists
    BOOL bSuccess = DirectoryExists(_pszPath);
    hr = bSuccess ? S_OK : E_FAIL;

Error:
Exit:
    return hr;
}

//  *************************************************************************
//  IsCompactFlashPath
//
//  Purpose:
//      Helper function to determine if the specified directory is a
//      removable storage card.
//
//  Details:
//      This helper function is created to abstract the details for what
//      determines that a directory is in fact removable storage. Prior
//      implementations depended on the file attributes to distinguish
//      removable storage, but this is not sufficient since the mount
//      directory for persistent storage has the same file attributes yet
//      it is not removable.
//
//  Parameters:
//      lpszFileName     [in]    a directory file name
//      dwFileAttributes [in]    the directory's file attributes
//
//  Returns:
//      TRUE    if the directory is a removable storage card
//      FALSE   if the directory is fixed (i.e., in RAM or persistent storage)
//
//  Side Effects:
//      none
//  *************************************************************************
#ifndef CF_CARDS_FLAGS
#define CF_CARDS_FLAGS (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_TEMPORARY)
#endif

// Handy macro to remove front slash, if present
#define REMOVE_FRONT_SLASH(str) ((str[0] == TEXT('\\')) ? &str[1] : &str[0])

const TCHAR c_szReleaseDir[] = TEXT("Release");

BOOL IsCompactFlashPath(LPCTSTR lpszFileName, DWORD dwFileAttributes)
{
    BOOL bRet = FALSE;

    //
    // A storage card should not have invalid attributes.
    //
    if ((DWORD)-1 == dwFileAttributes)
        return FALSE;
    
#ifndef TARGET_NT

#ifdef DEBUG
    // for debugging
    // we'll allow the name CF* to be detected as a flash card.
    static const TCHAR szDebugStorageDir[] = TEXT("CF");

    if ((dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ! _tcsncmp(lpszFileName, szDebugStorageDir, _tcslen(szDebugStorageDir)))
    {
        bRet = TRUE;
        goto Error;
    }
#endif

    // Is this a temporary directory?
    if ((dwFileAttributes & CF_CARDS_FLAGS) != CF_CARDS_FLAGS)
        goto Error;

    // Is this a known temp dir that is not flash?
    // E.g.: Persistent storage data directory, network and release directory
    TCHAR   szDataDir[MAX_PATH];
    LPCTSTR pszDataAfterSlash;
    LPCTSTR pszAfterSlash;
    LPCTSTR pszNetworksDir;
    CEOIDINFO ceOIDInfo;

    pszAfterSlash = REMOVE_FRONT_SLASH(lpszFileName);

    GetDataDirectory(szDataDir, lengthof(szDataDir));
    pszDataAfterSlash = REMOVE_FRONT_SLASH(szDataDir);

#if !defined(SHIP_BUILD) || defined(x86)

    if(!_tcsicmp(pszAfterSlash, c_szReleaseDir))
        goto Error;

#endif // !defined(SHIP_BUILD) || defined(x86)

    //get the localized network path
    if(CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK),
                    &ceOIDInfo))
    {
        pszNetworksDir =
            REMOVE_FRONT_SLASH(ceOIDInfo.infDirectory.szDirName);

        // Make sure it's not the network dir
        if(!_tcsicmp(pszAfterSlash, pszNetworksDir))
            goto Error;
    }


    if(!_tcsicmp(pszAfterSlash, pszDataAfterSlash))
        goto Error;

    bRet = TRUE;
#else
    // This portion (TARGET_NT) is defined in many versions of IsCompactFlash, so copied
    // to main one here

    // under NT search for "Storage Card" dir
    static const TCHAR szDebugStorageDir[] = TEXT("Storage Card");

    bRet =  ((dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !_tcsncmp(dwFileAttributes, szDebugStorageDir, _tcslen(szDebugStorageDir)));
#endif // TARGET_NT

Error:
    return bRet;
}

static TCHAR const c_szFindRootFiles[] = TEXT("\\*");

//  *************************************************************************
//  FindFirstStorageCard
//
//  Purpose:
//      This function searches for a storage card.
//
//  Details:
//      This helper function is created to abstract the details for finding
//      removable storage cards. It is designed to mimic the behavior of
//      FindFirstFile and FindNextFile except that no path is necessary
//      in the call to FindFirstStorageCard. A valid handle returned from
//      this function must be closed using FindClose. These functions also
//      rely on the OS functions for error handling, such as checking for
//      invalid handles and pointers.
//
//  Parameters:
//      lpFindData  [in]    a pointer to a WIN32_FIND_DATA structure
//
//  Returns:
//      a handle that can be used for subsequent calls to FindNextStorageCard
//      INVALID_HANDLE_VALUE indicates that no storage cards were found
//
//  Side Effects:
//      none
//  *************************************************************************
HANDLE FindFirstStorageCardPriv(LPWIN32_FIND_DATA lpFindData)
{
    //
    // Find all files in the root directory.
    //

    HANDLE hFind = FindFirstFile(c_szFindRootFiles, lpFindData);
    if (INVALID_HANDLE_VALUE != hFind)
    {
        //
        // Keep scanning files until a storage card is found.
        //

        do
        {
            //
            // When the storage card is found, return
            // the find handle for FindNext. The caller
            // must close the handle using FindClose.
            //

            if (IsCompactFlash(lpFindData))
                return hFind;

        } while (FindNextFile(hFind, lpFindData));

        //
        // If none of the files are storage cards,
        // then close the find handle and set the
        // return value to invalid to indicate that
        // no storage cards were found.
        //

        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }

    return hFind;
}



//  *************************************************************************
//  FindNextStorageCard
//
//  Purpose:
//      This function continues a search for a storage card.
//
//  Details:
//      This helper function is created to abstract the details for finding
//      removable storage cards. It is designed to mimic the behavior of
//      FindFirstFile and FindNextFile except that no path is necessary
//      in the call to FindFirstStorageCard.
//
//  Parameters:
//      hFind       [in]    a handle returned from a call to FindFirstStorageCard
//      lpFindData  [in]    a pointer to a WIN32_FIND_DATA structure
//
//  Returns:
//      TRUE if another storage card has been found
//      FALSE if no more storage cards could be found
//
//  Side Effects:
//      none
//  *************************************************************************
BOOL FindNextStorageCardPriv(HANDLE hFind, LPWIN32_FIND_DATA lpFindData)
{
    //
    // Continue scanning files from a call to FindFirstStorageCard.
    //

    while (FindNextFile(hFind, lpFindData))
    {
        //
        // If another storage card is found, return TRUE.
        //

        if (IsCompactFlash(lpFindData))
            return TRUE;
    }

    //
    // No more files.
    //

    return FALSE;
}

BOOL WINAPI  PathIsNandFlash(LPCTSTR lpszPath)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    TCHAR *psz,szPath[MAX_PATH];
    int cLeadingSlashes = 0;
    BOOL bRet = FALSE;

    if (!lpszPath ||
        (lpszPath[0] == L'\0') ||
        FAILED(StringCchCopy(szPath, lengthof(szPath), lpszPath)))
    {
        return bRet;
    }

    // Check for a network share (\\foo\bar)
    if (PathIsUNC(lpszPath))
    {
        return bRet;
    }
    
    psz = szPath;


    // Skip all initial slashes.
    while (*psz == TEXT('\\') || *psz == TEXT('/'))
    {
        cLeadingSlashes++;
        psz++;
    }

    // Find the next slash in the path.
    while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
    {
        psz++;
    }

    // Now we're at the end of the first component.  If this is a network
    // path we need to continue skipping ahead to the next slash.
    if (cLeadingSlashes > 1 && psz[0] && psz[1] != '\0')
    {
        psz++;
        while (*psz && (*psz != TEXT('\\')) && (*psz != TEXT('/')))
        {
            psz++;
        }
    }

    // Finally terminate the path.
    *psz = 0;

    DWORD dw = GetFileAttributes(szPath);;
    
    // "FILE_ATTRIBUTE_TEMPORARY" will be set for \release, \Network and "\Nand Flash" .
    // If its not release or Network, then it wil be Nand Flash
    if ((dw != (UINT)-1) && (dw & FILE_ATTRIBUTE_TEMPORARY))
    {
        static const lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
        size_t cch;
        bRet = TRUE;        
        // Check for CE style network path (\network)
        CEOIDINFO ceOidInfo = {0};
        if (::CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK), &ceOidInfo))
        {
            cch = _tcslen(ceOidInfo.infDirectory.szDirName);
            if (CSTR_EQUAL == ::CompareString(lcid, NORM_IGNORECASE, ceOidInfo.infDirectory.szDirName, cch, lpszPath, cch))
            {
                if ((TEXT('\0') == lpszPath[cch]) || (TEXT('\\') == lpszPath[cch]))
                {
                    bRet = FALSE;
                }
            }
        }

        // Check for release directory (\release)
        HANDLE hRelease = CreateEvent(NULL, TRUE, FALSE, TEXT("ReleaseFSD"));
        if (NULL != hRelease)
        {
            CloseHandle(hRelease);

            cch = 8; // _tcslen(TEXT("\\release"));
            if (CSTR_EQUAL == ::CompareString(lcid, NORM_IGNORECASE, TEXT("\\release"), cch, lpszPath, cch))
            {
                if ((TEXT('\0') == lpszPath[cch]) || (TEXT('\\') == lpszPath[cch]))
                {
                    bRet = FALSE;
                }
            }
        }        
    }

    return bRet;
}

