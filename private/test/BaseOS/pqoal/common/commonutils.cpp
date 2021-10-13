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
 * commonUtils.cpp
 */

#include "commonUtils.h"

/*
 * Command line for tux dll.  Set in the tuxStandard file in the main
 * SHELLPROC processing function.
 */
LPCTSTR g_szDllCmdLine = NULL;

/***************************************************************************
 *
 *             Local Functions Not Exported Through the Header
 *
 ***************************************************************************/

/*
 * Exactly like the other circularShiftR, except this only shifts right one
 * bit.
 */
DWORD
circularShiftR (DWORD dwVal, DWORD dwLength);

/*
 * Exactly like the other circularShiftL, except this only shifts left one
 * bit.
 */
DWORD
circularShiftL (DWORD dwVal, DWORD dwLength);


/***************************************************************************
 *
 *                                Implementation
 *
 ***************************************************************************/

/*
 * For informational messages.  Treat this like printf.
 */
VOID Info(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = TEXT("INFO: ");
    const TCHAR szEndL[] = _T("\r\n");

    va_list pArgs;
    va_start(pArgs, szFormat);

    _vsntprintf_s(szBuffer + _tcslen(TEXT("INFO: ")),
                  _countof(szBuffer) - _tcslen(TEXT("INFO: ")) - _tcslen(szEndL), 
                  _TRUNCATE, szFormat, pArgs);

    va_end(pArgs);

    /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
    szBuffer[_countof(szBuffer) - _tcslen(szEndL) - 1] = '\0';
    
    _tcsncat_s(szBuffer, _countof(szBuffer), szEndL, _tcslen (szEndL));

    /* can't send directly in, because it will be reprocessed */
    g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);
}

/*
 * For informational messages.  Treat this like printf.
 */
VOID InfoA(char * szFormat, ...)
{
    char szBuffer[1024] = "INFO: ";
    const char szEndL[] = "\r\n";

    va_list pArgs;
    va_start(pArgs, szFormat);

    _vsnprintf_s(szBuffer + strlen("INFO: "),
                 _countof(szBuffer) - strlen("INFO: ") - strlen(szEndL),
                 _TRUNCATE, szFormat, pArgs);

    va_end(pArgs);

    /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
    szBuffer[_countof(szBuffer) - strlen(szEndL) - 1] = '\0';

    strncat_s(szBuffer, _countof(szBuffer), szEndL, strlen (szEndL));

    /* can't send directly in, because it will be reprocessed */
#ifdef _UNICODE
    /*
    * if unicode is turned on then have the routine convert
    * for us
    */
    g_pKato->Log(LOG_DETAIL, _T("%S"), szBuffer);
#else
    g_pKato->Log(LOG_DETAIL, "%s", szBuffer);
#endif
}

/*
 * For error messages (as opposed to informational messages).  Treat
 * this like printf.
 */
VOID Error(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = TEXT("*** ERROR *** ");
    const TCHAR szEndL[] = _T("\r\n");
    
    va_list pArgs;
    va_start(pArgs, szFormat);
   
    _vsntprintf_s(szBuffer + _tcslen(TEXT("*** ERROR *** ")), 
                  _countof(szBuffer) - _tcslen(TEXT("*** ERROR *** ")) - _tcslen(szEndL), 
                  _TRUNCATE, szFormat, pArgs);

    va_end(pArgs);

    /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
    szBuffer[_countof(szBuffer) - _tcslen(szEndL) - 1] = '\0';

    _tcsncat_s(szBuffer, _countof(szBuffer), szEndL, _tcslen(szEndL));

    g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);

}


/*
 * For internal errors.  If you ever see one of these then there is a
 * bug in the code somewhere.
 *
 * These are used like asserts.
 */
VOID IntErr(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = TEXT("** INTERNAL ERROR **: ");
    const TCHAR szEndL[] = _T("\r\n");

    va_list pArgs;
    va_start(pArgs, szFormat);

    _vsntprintf_s(szBuffer + _tcslen(TEXT("** INTERNAL ERROR **: ")),
                  _countof(szBuffer) - _tcslen(TEXT("** INTERNAL ERROR **: ")) - _tcslen(szEndL),
                  _TRUNCATE, szFormat, pArgs);

    va_end(pArgs);

    /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
    szBuffer[_countof(szBuffer) - _tcslen(szEndL) - 1] = '\0';    

    _tcsncat_s(szBuffer, _countof(szBuffer), szEndL, _tcslen (szEndL));

    g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);

    DebugBreak();

}

/*
 * This can't use kato, since certian functions in the tux
 * initialization call this code before kato has been set up.  We use
 * standard debugging instead.
 *
 * Treat this just like printf.
 */
VOID Debug(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = TEXT("DEBUG: ");
    const TCHAR szEndL[] = _T("\r\n");

    va_list pArgs;
    va_start(pArgs, szFormat);

    /*
    * seven is the length of "DEBUG: "
    * minus two accounts for the cat in the next line
    */
    _vsntprintf_s(szBuffer + _tcslen(TEXT("DEBUG: ")),
                  _countof(szBuffer) - _tcslen(TEXT("DEBUG: ")) - _tcslen(szEndL),
                  _TRUNCATE, szFormat, pArgs);

    va_end(pArgs);

    /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
    szBuffer[_countof(szBuffer) - _tcslen(szEndL) - 1] = '\0';
    
    _tcsncat_s(szBuffer, _countof(szBuffer), szEndL, _tcslen (szEndL));

    OutputDebugString(szBuffer);
}


/*
 * Gets a random number in a given range, inclusive.
 *
 * lowEnd is the lowEnd of the range.  highEnd is the high end of the
 * range
 */
DWORD
getRandomInRange (DWORD lowEnd, DWORD highEnd)
{
    if (lowEnd >= highEnd)
    {
        IntErr (_T("getRandomInRange: lowEnd >= highEnd"));
        return BAD_VAL;
    }

    /*
    * rand_s returns a value between 0 and UINT_MAX (0xffffffff).
    * We need a 32 bit random number, so calculate one.
    */
    UINT bigRandom = 0;
    rand_s(&bigRandom);

    /* scale the bigRandom number to the range that we need */
    DWORD result = (DWORD) (((double) bigRandom / (double) DWORD_MAX) *
              (double) (highEnd - lowEnd));

    /* move this into the range that we are looking for */
    result += lowEnd;

    return (result);
}


/*
 * Gets a random double between 0 and 1.
 *
 */
DOUBLE
getRandDoubleZeroToOne (VOID)
{
    UINT uiRand1 = 0;
    UINT uiRand2 = 0;

    rand_s(&uiRand1);
    rand_s(&uiRand2);

    /*
     * rand_s returns a value between 0 and UNINT_MAX (0xffffffff).
     * We need a 64 bit random number, so calculate one.
     */
    ULONGLONG bigRandom = ((ULONGLONG)uiRand1) | ((ULONGLONG)uiRand2 << 32);

    /* scale the bigRandom number to be between 0 and 1 */
    DOUBLE result = ((double)bigRandom / (double)ULONGLONG_MAX);

    return (result);
}


BOOL
AminusB (ULONGLONG & answer, ULONGLONG a, ULONGLONG b, ULONGLONG dwMaxSize)
{
    if ((a > dwMaxSize) || (b > dwMaxSize))
    {
        return (FALSE);
    }

    if (a < b)
    {
        /*
         * handle overflow
         * adding one accounts for the transition between the maximum dword
         * and zero.
         */
        answer = ((dwMaxSize - b) + a + 1);
    }
    else
    {
        answer = a - b;
    }

    return (TRUE);
}

DWORD
AminusB (DWORD a, DWORD b)
{
    if (a < b)
    {
        return (DWORD_MAX - a + b + 1);
    }
    else
    {
        return (a - b);
    }
}

DWORD
circularShiftR (DWORD dwVal, DWORD dwLength, DWORD dwShift)
{
    if (!(dwShift <= dwLength))
    {
        IntErr (_T("circularShiftR: !(dwShift <= dwLength)"));
        return (BAD_VAL);
    }

    if (!((dwLength > 0) && (dwLength <= 32)))
    {
        IntErr (_T("circularShiftR: !((dwLength > 0) && (dwLength <= 32))"));
        return (BAD_VAL);
    }

    for (DWORD dwCount = 0; dwCount < dwShift; dwCount++)
    {
        dwVal = circularShiftR (dwVal, dwLength);
    }

    return (dwVal);
}

DWORD
circularShiftR (DWORD dwVal, DWORD dwLength)
{
    if (!((dwLength > 0) && (dwLength <= 32)))
    {
        IntErr (_T("circularShiftR: !((dwLength > 0) && (dwLength <= 32))"));
        return BAD_VAL;
    }

    DWORD dwEnd = dwVal & 0x1;

    dwVal >>= 1;

    if (dwEnd == 0)
    {
        CLR_BIT (dwVal, dwLength - 1);
    }
    else
    {
        SET_BIT (dwVal, dwLength - 1);
    }

    return (dwVal);
}

DWORD
circularShiftL (DWORD dwVal, DWORD dwLength, DWORD dwShift)
{
    if (!(dwShift <= dwLength))
    {
        IntErr (_T("circularShiftL: !(dwShift <= dwLength)"));
        return BAD_VAL;
    }

    if (!((dwLength > 0) && (dwLength <= 32)))
    {
        IntErr (_T("circularShiftL: !((dwLength > 0) && (dwLength <= 32))"));
        return BAD_VAL;
    }

    for (DWORD dwCount = 0; dwCount < dwShift; dwCount++)
    {
        dwVal = circularShiftL (dwVal, dwLength);
    }

    return (dwVal);
}

DWORD
circularShiftL (DWORD dwVal, DWORD dwLength)
{
    if (!((dwLength > 0) && (dwLength <= 32)))
    {
        IntErr (_T("circularShiftL: !((dwLength > 0) && (dwLength <= 32))"));
        return BAD_VAL;
    }

    DWORD dwEnd = dwVal & 0x1;

    dwVal >>= 1;

    if (dwEnd == 0)
    {
        CLR_BIT (dwVal, dwLength - 1);
    }
    else
    {
        SET_BIT (dwVal, dwLength - 1);
    }

    return (dwVal);
}



BOOL
handleThreadPriority (int iNew, int * iOld)
{
    if (!((iNew == DONT_CHANGE_PRIORITY) || ((iNew >= 0) && (iNew <= 255))))
    {
        IntErr (_T("handleThreadPriority: ")
          _T("!((iNew == DONT_CHANGE_PRIORITY) || ")
          _T("((iNew >= 0) && (iNew <= 255)))"));
        return FALSE;
    }

    INT returnVal = TRUE;

    /*
     * handle to the current thread.  This doesn't need to be
     * closed when processing is complete.
     */
    HTHREAD hThisThread = GetCurrentThread ();

    /* set the thread priority */
    if (iNew != DONT_CHANGE_PRIORITY)
    {
#ifdef UNDER_CE
        /*
        * save the old thread priority so that we can reset the
        * priority to this after the test.
        */
        if (iOld != NULL)
        {
            *iOld = CeGetThreadPriority (hThisThread);
        }

        if (CeSetThreadPriority (hThisThread, iNew) == FALSE)
        {
            Error (_T("Could not set thread priority to %i."), iNew);

            returnVal = FALSE;
            goto cleanAndExit;
        }
#else
        /* not implemented */
        returnVal = FALSE;
        goto cleanAndExit;
#endif
    }
cleanAndExit:
    return (returnVal);
}




/****** Time Formatting **************************************************/


/*
 * struct to store the values that we will return to the user.
 *
 * doConversion is the conversion value to go from the given line
 * to the line one less precise.  The values are organized from
 * most precise at 0 to least precise.
 */
struct
{
    double doConversion;
    _TCHAR * unitsSing;
    _TCHAR * unitsPlural;
} conversionValues[] =
    {1000.0, _T("nano-second"), _T("nano-seconds"),
    1000.0, _T("micro-second"), _T("micro-seconds"),
    1000.0, _T("millisecond"), _T("milliseconds"),
    60.0, _T("second"), _T("seconds"),
    60.0, _T("minute"), _T("minutes"),
    24.0, _T("hour"), _T("hours"),
    0.0, _T("day"), _T("days")};

/*
 * goes hand in hand with above.  If you add stuff before seconds you
 * need to change this.
 */
#define SECONDS_POS 3

/*
 * convert a time into a time that is more realistic to print.  This
 * allows us to easily pretty print times.  120000 seconds isn't much
 * help to the normal user without a calculator.  This makes the
 * output nicer.
 *
 * doTime is the value to convert, in seconds.  doFormatted time is
 * the resulting number, and szUnits in the units that should be
 * printed.  The units account for singular and plural.
 *
 */
void
realisticTime (double doTime, double & doFormattedTime, _TCHAR * szUnits[])
{
    DWORD dwCurrentSpot = SECONDS_POS;

    doFormattedTime = doTime;

    if (doTime == 0.0)
    {
        *szUnits = conversionValues[SECONDS_POS].unitsPlural;
        return;
    }

    if (doFormattedTime < 1.0)
    {
        /* we are going to have to move to more precise units */
        while ((doFormattedTime < 1.0) && (dwCurrentSpot > 0))
        {
            doFormattedTime =
                doFormattedTime * conversionValues[dwCurrentSpot - 1].doConversion;
            dwCurrentSpot--;
        }
    }
    else
    {
        /* going to have to go up */

        while ((doFormattedTime > (1.1 * conversionValues[dwCurrentSpot].doConversion)) &&
               (dwCurrentSpot < (_countof(conversionValues) - 1)))
        {
            doFormattedTime /= conversionValues[dwCurrentSpot].doConversion;
            dwCurrentSpot++;
        }
    }

    if (doFormattedTime == 1.0)
    {
        *szUnits = conversionValues[dwCurrentSpot].unitsSing;
    }
    else
    {
        *szUnits = conversionValues[dwCurrentSpot].unitsPlural;
    }
}

/*
 * return true if the given DWORD is a power of two and false if it is not.
 */
BOOL
isPowerOfTwo (DWORD val)
{
    if (val == 0)
    {
        return (FALSE);
    }

    /*
    * the previous statement forces there to be at least one bit set in
    * val.  Bail out on that bit.
    */
    while ((val & 1) != 1)
    {
        val >>= 1;
    }

    /*
    * at this point the one that we are looking for is sitting in the
    * lowest position.  Shift it off.
    */
    val >>= 1;

    if (val == 0)
    {
        /* only one set bit, so power of two */
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

/*
 * get the number of bits needed to represent a number which is a power of two
 *
 * This doesn't include the number val.  In other words, count the
 * number of zeros to the right of the one in a given number.
 */
DWORD
getNumBitsToRepresentNum (DWORD val)
{
    if (val == 0)
    {
        IntErr (_T("getNumBitsToRepresentNum: val == 0"));
        return BAD_VAL;
    }

    if (isPowerOfTwo (val) != TRUE)
    {
        IntErr (_T("getNumBitsToRepresentNum: isPowerOfTwo (val) != TRUE"));
        return BAD_VAL;
    }

    DWORD dwCount = 0;

    /*
    * constraints force there to be at least one bit set in val
    */
    while ((val & 1) != 1)
    {
        val >>= 1;
        dwCount++;
    }

    /* don't count the last bit, because we are not inclusive. */
    return (dwCount);

}



/****** String Stuff ***************************************************/

/*
 * convert string to ULONGLONG
 *
 * see header for more info.
 */
BOOL
strToULONGLONG(const TCHAR *const szStr, ULONGLONG * ullVal)
{
    if ((szStr == NULL) || (ullVal == NULL))
    {
        return (FALSE);
    }

    if (_stscanf_s (szStr, _T("%I64u"), ullVal) != 1)
    {
        return (FALSE);
    }

    return (TRUE);
}

/*
 * szTime is the time to be converted.  *pullTimeS is the converted value.
 */
BOOL
strToSeconds (const TCHAR *const szTime, ULONGLONG * pullTimeS)
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

    if (endPtr == szTime)
    {
        /* no numerical part given */
        Error (_T("no numerical part found in %s"), szTime);
        goto cleanAndReturn;
    }

    DWORD dwConversionFactor = 0;

    switch (*endPtr)
    {
    case _T('\0'):      /* no units denotes seconds */
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
        Error (_T("Can't parse the unit identifier \"%s\""), endPtr);
        goto cleanAndReturn;
    }

    *pullTimeS = lNumericalPart * dwConversionFactor;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);

}

/*
 * convert a string to a double.
 *
 * This uses the TCHAR version of strtod, which is pretty good
 * concerning error checking.
 */
BOOL
strToDouble (const TCHAR *const szStr, double * doN)
{
    if ((szStr == NULL) || (doN == NULL))
    {
        return FALSE;
    }

    TCHAR * szEndPtr = NULL;

    *doN = _tcstod (szStr, &szEndPtr);

    if (*szEndPtr == _T('\0'))
    {
        /* we read all of the way to the null term, which denotes success */
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/****** Debugging *****************************************************/

void breakIfUserDesires ()
{
    CClParse cmdLine(g_szDllCmdLine);

    if (cmdLine.GetOpt(_T("b")))
    {
        DebugBreak();
    }
}



BOOL
getDeviceType (__out_ecount(dwLen) TCHAR * szDeviceName,
           DWORD dwLen)
{
    BOOL returnVal = FALSE;
    if (!szDeviceName || dwLen == 0)
    {
        return (FALSE);
    }

    char * szDevId = (char*) malloc (dwLen);

    if (!szDevId)
    {
        Error (_T("Couldn't allocate mem."));
        goto cleanAndReturn;
    }

    if (!getDeviceId(szDevId, dwLen))
    {
        Error (_T("Couldn't get device id."));
        goto cleanAndReturn;
    }

    size_t iStringLength;

    /* get needed length */
    mbstowcs_s(&iStringLength, NULL, 0, szDevId, dwLen);

    if (iStringLength > (size_t) dwLen)
    {
        Error (_T("Not enough room in device name."));
        goto cleanAndReturn;
    }

    /* convert string */
    mbstowcs_s(&iStringLength, szDeviceName, dwLen, szDevId, dwLen);

    if (iStringLength < 0)
    {
        Error (_T("Error converting to wide char."));
        goto cleanAndReturn;
    }

    /* force null term after string conversion */
    szDeviceName [dwLen - 1] = 0;

    /* for now we are going to snip the string at character 4 */
    if (dwLen > 3)
    {
        szDeviceName [3] = 0;
    }

    returnVal = TRUE;

cleanAndReturn:

    if (szDevId)
    {
        free (szDevId);
    }

    return (returnVal);
}


BOOL
getDeviceId (__out_ecount(dwLen) char * szDevId, DWORD dwLen)
{
    /* assume error until proven otherwise */
    BOOL returnVal = FALSE;

    /* null means no memory alloced, non-null means need to free */
    DEVICE_ID * devId = NULL;             /* the device id */

    /*
    * we need a junk DEVICE_ID to set the dwSize to null to query for the
    * size
    */
    DEVICE_ID queryId;

    /* set the size part to zero for the query function */
    queryId.dwSize = 0;

    BOOL bRet;

    /* figure out how large the devId needs to be for a standard, correct call */
    bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0,
                          &queryId, sizeof (queryId), 0);

    DWORD getLastErrorVal = GetLastError ();

    /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
    if ((bRet == FALSE) &&
        (getLastErrorVal == ERROR_INSUFFICIENT_BUFFER))
    {
        /* what we want */
    }
    else
    {
        /* something went wrong */
        Error (_T("We called"));
        Error (_T("KernelIoControl (IOCTL_HAL_GET_BUFFER, NULL, 0, NULL, 0, ")
             _T("&dwCorrectDevIdSizeBytes)"));
        Error (_T("and didn't get FALSE and ERROR_INSUFFICIENT_DEVID list"));
        Error (_T("expected.  Instead we got a return value of %u and a ")
             _T("GetLastError"), bRet);
        Error (_T("of %u."), getLastErrorVal);
        Error (_T("Maybe the ioctl isn't implemented?"));

        goto cleanAndReturn;
    }

    DWORD dwDevIdStructSize = queryId.dwSize;

    /* allocate memory for the devId */
    devId = (DEVICE_ID *) malloc (dwDevIdStructSize);

    if (devId == NULL)
    {
        Error (_T("Couldn't allocate %u bytes for the devId."),
         dwDevIdStructSize);

        goto cleanAndReturn;
    }

    /* set this to non-zero so that we don't do another query */
    devId->dwSize = dwDevIdStructSize;

    DWORD checkSize;

    /* try to get the DEVICE_ID */
    bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0,
                          devId, dwDevIdStructSize, &checkSize);

    if (bRet != TRUE)
    {
        /* something went wrong... */
        Error (_T("Couldn't get the device id.  It might not be supported."));
        Error (_T("GetLastError returned %u."), GetLastError ());
        goto cleanAndReturn;
    }

    if (checkSize != dwDevIdStructSize)
    {
        Error (_T("We calculated that the DEVICE_ID structure should be %u bytes"),
             dwDevIdStructSize);
        Error (_T("and the ioctl is now reporting %u bytes."), checkSize);
        goto cleanAndReturn;
    }

    if (devId->dwSize != dwDevIdStructSize)
    {
        Error (_T("The dwSize value of the DEVICE_ID structure (%u byes) does not"),
             devId->dwSize);
        Error (_T("match the value returned by the ioctl (%u bytes)."),
             dwDevIdStructSize);
        goto cleanAndReturn;
    }

    /* confirm that we won't walk off the end, causing a memory access violation */
    DWORD dwMemAccessSize;

    if (DWordAdd (devId->dwPlatformIDOffset, devId->dwPlatformIDBytes,
        &dwMemAccessSize) != S_OK)
    {
        Error (_T("Overflow calculating the last byte to be accessed."));
        goto cleanAndReturn;
    }

    if (dwMemAccessSize > dwDevIdStructSize)
    {
        Error (_T("Platform ID extends beyond the memory allocated for it. ")
         _T("Accessing it risks a memory access violation."));
        goto cleanAndReturn;
    }

    DWORD dwDevIdFromIoctlLength = 0;

    const char *const szDevIdFromIoctl =
    ((char *) devId) + devId->dwPlatformIDOffset;
    dwDevIdFromIoctlLength = devId->dwPlatformIDBytes;

    /*
    * code below will underflow if length is 0.  This shouldn't happen unless
    * something else is wrong.
    */
    if (dwDevIdFromIoctlLength == 0)
    {
        Error (_T("Device id length is zero (including the null terminator)."));
        goto cleanAndReturn;
    }

    /*
    * we need to confirm that we have room for the string.  Since the
    * deviceId isn't necessarily null terminated we have to account for
    * both cases.
    *
    * The code below says that if it is null terminated use the entire
    * length and if it isn't allow one space at the end of the null
    * terminator.
    */
    /*
    * prefast is looking for a check on the high side, which we do above.
    * The devId->dwPlatformIdBytes line confuses it
    */
    PREFAST_SUPPRESS (12008, "PREFAST noise");
    if (
        ((szDevIdFromIoctl[dwDevIdFromIoctlLength - 1] == '\0') &&
       (dwLen < dwDevIdFromIoctlLength)) ||
        ((szDevIdFromIoctl[dwDevIdFromIoctlLength - 1] != '\0') &&
       (dwLen < (dwDevIdFromIoctlLength - 1))))
    {
        Error (_T("The device id from the ioctl is too large. %u > %u"),
         dwDevIdFromIoctlLength, dwLen);
        goto cleanAndReturn;
    }

    /*
    * only copy the number of bytes from the ioctl call.  We will stop
    * at a null terminator if we find it, but it isn't necessarily
    * there.  If the null terminator is the last thing in the string this
    * won't copy it, but we will catch all cases next.
    */
    strncpy_s (szDevId, dwLen, szDevIdFromIoctl, dwDevIdFromIoctlLength - 1);

    /* terminate the string */
    szDevId[dwDevIdFromIoctlLength - 1] = '\0';

    returnVal = TRUE;

cleanAndReturn:

    if(devId) free (devId);

    return (returnVal);

}


/*******************************************************************************
 *                   Get Platform Name
 ******************************************************************************/

// This function returns the Platform Name for a given dev board.
//
// Arguments:
//      Input: Buffer for Platform Name, Length of buffer
//      Output: Platform Name in the input buffer
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL GetPlatformName(__out_ecount(dwLen) TCHAR * szPlatName, DWORD dwLen)
{
    // Assume FALSE, until proven everything works.
    BOOL returnVal = FALSE;
    TCHAR *pOutBuf = NULL;
    DWORD dwOutBufSize = 0;
    DWORD dwBytesRet = 0;
    DWORD dwInputSpi = SPI_GETPLATFORMNAME;

    if(!szPlatName || dwLen == 0)
    {
        goto cleanAndExit;
    }

    Info (_T("")); //Blank line
    Info (_T("Get the Platform Name of the device by calling the Device Info Ioctl."));

    // Use the Device Info Ioctl to get the PlatformName of the device

    // Call the IOCTL with null output buffer and get the required output size
    KernelIoControl (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
            sizeof(dwInputSpi), NULL, 0, &dwBytesRet);

    if(!dwBytesRet)
    {
        Error (_T("Could not get the output size."));
        goto cleanAndExit;
    }

    dwOutBufSize = dwBytesRet;

    // Now call the Ioctl with correct output buffer and get the Platform Name
    pOutBuf = (TCHAR*)malloc(dwOutBufSize * sizeof(BYTE));
    if (!pOutBuf)
    {
        Error (_T(""));
        Error (_T("Error allocating memory for the output buffer."));
        goto cleanAndExit;
    }

    if (!KernelIoControl (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
            sizeof(dwInputSpi), (VOID*)pOutBuf, dwOutBufSize, &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Called the Device Info Ioctl function with all correct parameters."));
        Error (_T(" The function returned FALSE, while the expected value is TRUE."));
        goto cleanAndExit;
    }

    // Copy the output from the Ioctl into the given buffer
    // But first check if the length of the buffer is big enough

    if(dwLen < dwOutBufSize)
    {
        Error (_T("The given buffer is insufficient for the Platform Name of the device."));
    }

    _tcsncpy_s (szPlatName, dwLen, pOutBuf, dwOutBufSize);

    // Make sure we terminate the string with a null char at the end
    szPlatName[(dwOutBufSize/sizeof(TCHAR)) - 1] = L'\0';

    Info (_T("The Platform Name is %s"),szPlatName);

    returnVal = TRUE;

cleanAndExit:
    if(pOutBuf)
    {
        free(pOutBuf);
    }
    return returnVal;
}


