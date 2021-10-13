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
 * cmd.cpp
 */

#include "cmd.h"


cTokenControl::cTokenControl ()
{
    dwLength = 0;
    dwMaxLength = 0;
    ppszTokens = NULL;

}

cTokenControl::~cTokenControl ()
{
    if ((ppszTokens == NULL) && (dwLength != 0))
    {
        IntErr (_T("In destructor cTokenControl"));
        return;
    }

    /* free memory if something is there */
    for (DWORD dwCurr = 0; dwCurr < dwLength; dwCurr++)
    {
        free (ppszTokens[dwCurr]);

    }

    free (ppszTokens);
}

BOOL
cTokenControl::add (const TCHAR *const szStr, DWORD dwNumCharsWithNull)
{
    BOOL returnVal = FALSE;

    if (dwLength >= dwMaxLength)
    {
        /* expand array */
        dwMaxLength = dwMaxLength * 2 + 1;

        PREFAST_ASSERT( dwMaxLength > dwLength );

        TCHAR ** ppszTemp = (TCHAR **) malloc (dwMaxLength * sizeof (*ppszTokens));

        if (ppszTemp == NULL)
        {
            Error (_T("Couldn't get memory."));
            goto cleanAndReturn;
        }

        for (DWORD dwCurr = 0; dwCurr < dwLength; dwCurr++)
        {
            ppszTemp[dwCurr] = ppszTokens[dwCurr];
        }

        free (ppszTokens);

        ppszTokens = ppszTemp;
    }

    /* add string */

    DWORD dwAllocSize = 0;
    if(S_OK != DWordMult( dwNumCharsWithNull, sizeof(TCHAR), &dwAllocSize) ) {
        Error( _T("Overflow occurred on dwNumCharsWithNull\r\n") );
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    ppszTokens[dwLength] = (TCHAR *) malloc (dwAllocSize);

    if (ppszTokens[dwLength] == NULL)
    {
        Error (_T("Couldn't get memory."));
        goto cleanAndReturn;
    }

    _tcsncpy_s (ppszTokens[dwLength], dwNumCharsWithNull, szStr, _TRUNCATE);

    /* force null terminate */
    ppszTokens[dwLength][dwNumCharsWithNull - 1] = 0;

    dwLength++;

    returnVal = TRUE;

 cleanAndReturn:
    return (returnVal);
}

DWORD
cTokenControl::listLength ()
{
    return (dwLength);
}

TCHAR *&
cTokenControl::operator[] (const DWORD index)
{
    return (ppszTokens[index]);
}


BOOL
tokenize (const TCHAR *const szStr, cTokenControl * tokens)
{
    if (!szStr || !tokens)
    {
        return (FALSE);
    }

    /***** make a copy */

    /* get size */
    DWORD dwNeedSize = _tcslen(szStr) + 1;

    if (dwNeedSize == 0)
    {
        /* overflow */
        return(FALSE);
    }

    TCHAR * szCopy = (TCHAR *) malloc (dwNeedSize * sizeof (*szStr));

    if (!szCopy)
    {
        Error (_T("Couldn't allocate memory."));
        return (FALSE);
    }

    _tcsncpy_s (szCopy, dwNeedSize, szStr, _TRUNCATE);
    /* force null term */
    szCopy[dwNeedSize - 1] = 0;

    /***** tokenize it */
    /* pointer only changes, so make underlying object constant */
    const TCHAR * tok = NULL;

    const TCHAR *const szSeps = _T(" ");

    TCHAR *szNextTok = NULL;

    tok = _tcstok_s (szCopy, szSeps, &szNextTok);

    while (tok != NULL)
    {
        tokens->add (tok, _tcslen (tok) + 1);

        tok = _tcstok_s (NULL, szSeps, &szNextTok);
    }

    free (szCopy);

    return (TRUE);
}


/*
 * returns true if you can find it and false otherwise */

BOOL
findTokenI(cTokenControl * tokens,
       const TCHAR *const szLookingFor,
       DWORD * pdwIndex)
{
    /* start at zero */
    return (findTokenI (tokens, szLookingFor, 0, pdwIndex));
}

BOOL
findTokenI(cTokenControl * tokens,
       const TCHAR *const szLookingFor, DWORD dwStartingIndex,
       DWORD * pdwIndex)
{
    if (!tokens || !szLookingFor || !pdwIndex)
    {
        return(FALSE);
    }

    DWORD dwCurrIndex = dwStartingIndex;

    /* order matters */
    while ((dwCurrIndex < tokens->listLength()) &&
     (_tcsicmp (szLookingFor, (*tokens)[dwCurrIndex]) != 0))
    {
        dwCurrIndex++;
    }

    if (dwCurrIndex >= tokens->listLength())
    {
        /* we didn't find it */
        return (FALSE);
    }
    else
    {
        *pdwIndex = dwCurrIndex;
        return (TRUE);
    }
}




/*
 * returns true if it is found and false if it isn't found or can't convert
 */
DWORD
getOptionPlusTwo (cTokenControl * tokens,
          DWORD dwCurrSpot,
          const TCHAR *const szName,
          double * pdo1, double * pdo2)
{
    DWORD returnVal = CMD_ERROR;

    if (!tokens || !dwCurrSpot || !szName || !pdo1 || !pdo2)
    {
        goto cleanAndReturn;
    }

    if (dwCurrSpot >= tokens->listLength())
    {
        /* no tokens on the list, so bail out */
        returnVal = CMD_NOT_FOUND;
        goto cleanAndReturn;
    }

    if (_tcsicmp (szName, (*tokens)[dwCurrSpot]) != 0)
    {
        /* didn't fine it, so return */
        returnVal = CMD_NOT_FOUND;
        goto cleanAndReturn;
    }

    DWORD dwEnd;
    if ((DWordAdd (dwCurrSpot, 2, &dwEnd) != S_OK) ||
        (dwEnd >= tokens->listLength()))
    {
        Error (_T("Not enough params for %s."), szName);
        goto cleanAndReturn;
    }

    /* found it, so pull next two values */

    BOOL bLocalReturn = TRUE;

    if (!strToDouble ((*tokens)[dwCurrSpot + 1], pdo1))
    {
        /* it failed */
        Error (_T("Could not convert %s to a double.  Is the number in ")
         _T("common decimal form?"), (*tokens)[dwCurrSpot + 1]);
        bLocalReturn = FALSE;
    }

    if (!strToDouble ((*tokens)[dwCurrSpot + 2], pdo2))
    {
        /* it failed */
        Error (_T("Could not convert %s to a double.  Is the number in ")
         _T("common decimal form?"), (*tokens)[dwCurrSpot + 2]);
        bLocalReturn = FALSE;
    }

    if (!bLocalReturn)
    {
        goto cleanAndReturn;
    }

    returnVal = CMD_FOUND_IT;
 cleanAndReturn:

    return (returnVal);
}

/*
 * if couldn't find it or other error return false.  Else return true.
 * pullVal isn't touched on error.
 */
BOOL
getOptionTimeUlonglong (cTokenControl * tokens,
            const TCHAR *const szCmdName,
            ULONGLONG * pullVal)
{
    BOOL returnVal = FALSE;

    if (!tokens || !szCmdName || !pullVal)
    {
        return (FALSE);
    }

    DWORD dwIndex;

    if (!findTokenI (tokens, szCmdName, &dwIndex))
    {
        goto cleanAndReturn;
    }

    /* we found it */
    if ((dwIndex >= tokens->listLength() - 1) || (dwIndex == DWORD_MAX))
    {
        /* last param in list */
        Error (_T("%s given no parameter."), szCmdName);
        goto cleanAndReturn;
    }

    ULONGLONG ullTimeS = 0;

    /* overflow check is above */
    dwIndex++;

    /* there is one more param */
    if (!strToSeconds ((*tokens)[dwIndex], &ullTimeS))
    {
        Error (_T("Error on param %s.  Couldn't convert %s to ")
         _T("seconds."),
         szCmdName, (*tokens)[dwIndex]);
        goto cleanAndReturn;
    }

    *pullVal = ullTimeS;

    returnVal = TRUE;

 cleanAndReturn:
    return (returnVal);
}

/*
 * return true if the option is present and false otherwise
 */
BOOL
isOptionPresent (cTokenControl * tokens,
         const TCHAR *const szOption)
{
    if (!szOption)
    {
        return (FALSE);
    }

    DWORD dwIndex = 0;

    if (findTokenI (tokens, szOption, &dwIndex))
    {
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

/*
 * get an option, converting to double.
 *
 * Returns true if value is found and converted and false otherwise.
 */
BOOL
getOptionDouble (cTokenControl * tokens,
         const TCHAR *const szCmdName,
         double * pdoVal)
{
    BOOL returnVal = FALSE;

    if (!tokens || !szCmdName || !pdoVal)
    {
        return (FALSE);
    }

    DWORD dwIndex;

    if (!findTokenI (tokens, szCmdName, &dwIndex))
    {
        goto cleanAndReturn;
    }

    /* we found it */
    if ((dwIndex >= tokens->listLength() - 1) || (dwIndex == DWORD_MAX))
    {
        /* last param in list */
        Error (_T("%s given no parameter."), szCmdName);
        goto cleanAndReturn;
    }

    /* overflow check is above */
    dwIndex++;

    double doTemp = 0;

    /* there is one more param */
    if (!strToDouble ((*tokens)[dwIndex], &doTemp))
    {
        Error (_T("Error on param %s.  Couldn't convert %s to ")
         _T("double."),
         szCmdName, (*tokens)[dwIndex]);
        goto cleanAndReturn;
    }

    *pdoVal = doTemp;

    returnVal = TRUE;

 cleanAndReturn:
    return (returnVal);
}


/*
 * get an option, converting to double.
 *
 * Returns true if value is found and converted and false otherwise.
 */
BOOL
getOptionString (cTokenControl * tokens,
         const TCHAR *const szCmdName,
         TCHAR ** pszVal)
{
    BOOL returnVal = FALSE;

    if (!tokens || !szCmdName || !pszVal)
    {
        return (FALSE);
    }

    DWORD dwIndex = 0;

    if (!findTokenI (tokens, szCmdName, &dwIndex))
    {
        goto cleanAndReturn;
    }

    /* we found it */
    if ((dwIndex >= tokens->listLength() - 1) || (dwIndex == DWORD_MAX))
    {
        /* last param in list */
        Error (_T("%s given no parameter."), szCmdName);
        goto cleanAndReturn;
    }

    /* overflow check is above */
    dwIndex++;

    *pszVal = (*tokens)[dwIndex];

    returnVal = TRUE;

 cleanAndReturn:
    return (returnVal);
}


BOOL
getOptionDWord (cTokenControl * tokens,
        const TCHAR *const szCmdName,
        DWORD * pdwVal)
{
    BOOL returnVal = FALSE;

    if (!tokens || !szCmdName || !pdwVal)
    {
        return (FALSE);
    }

    DWORD dwIndex;

    if (!findTokenI (tokens, szCmdName, &dwIndex))
    {
        goto cleanAndReturn;
    }

    /* we found it */
    if ((dwIndex >= tokens->listLength() - 1) || (dwIndex == DWORD_MAX))
    {
        /* last param in list */
        Error (_T("%s given no parameter."), szCmdName);
        goto cleanAndReturn;
    }

    /* overflow check is above */
    dwIndex++;

    TCHAR * endPtr = NULL;

    /* use strtoul to convert the numerical part, if possible */
    unsigned long lNumericalPart;
    LPCWSTR pszHexPrefix = TEXT("0x");

    if(0 == _tcsnccmp((*tokens)[dwIndex], pszHexPrefix, _tcslen(pszHexPrefix)))
    {
          /* 16 is the base */
          lNumericalPart = _tcstoul ((*tokens)[dwIndex], &endPtr, 16);
    }
    else
    {
           /* 10 is the base */
          lNumericalPart = _tcstoul ((*tokens)[dwIndex], &endPtr, 10);
    }
    

    if (!endPtr || *endPtr != _T('\0'))
    {
        Error (_T("Expected a number and got %s."), (*tokens)[dwIndex]);
        goto cleanAndReturn;
    }

    /* at this point we have the numerical part */
    if (lNumericalPart > DWORD_MAX)
    {
        Error (_T("Value is too large. %u > DWORD_MAX"), lNumericalPart);
        goto cleanAndReturn;
    }

    *pdwVal = (DWORD) lNumericalPart;

    returnVal = TRUE;

 cleanAndReturn:
    return (returnVal);
}
