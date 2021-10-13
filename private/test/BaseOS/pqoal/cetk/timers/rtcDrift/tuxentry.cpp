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
 * tuxRtcToNtp.cpp
 */

/***** Headers  **************************************************************/

/* gives TESTPROCs declarations for these functions */
#include "tuxRtcToNtp.h"

/* handles communication with ntp server */
#include "ntp.h"

#include <winsock2.h>

/* common utils */
#include "..\..\..\common\commonUtils.h"

#include "..\code\timerConvert.h"

/* default sleep time in seconds */
#define DEFAULT_SLEEP_TIME_S    (3 * 60 * 60)

/* maximum IP address list */
#define MAX_IP_ADDRESS_NUMS     (10)

#define NTP_ACTIVE_SERVERS      (2)

#define RTC_RESOLUTION_ITERS    (2)

#define GTC_RESOLUTION_ITERS    (10)

// Allow +/- 0.027778% drift, i.e. +/-~1sec per hour
#define GTC_DRIFT_TOLERANCE_PER_TICK   (0.00027778)


/* command line arg names */
#define ARG_STR_SLEEP_TIME (_T("-sleepTime"))
#define ARG_STR_SERVER_NAME (_T("-server"))
#define ARG_STR_QUERY_MODE (_T("-q"))

/***** command line processing functions */

/* Get the time server from the cmd line */
BOOL
getTimeServerInfo (__out_ecount(dwTimeServerNameMaxLength) TCHAR * szTimeServerName, DWORD dwTimeServerNameMaxLength, DWORD * ipVersion);

/* get the sleep time from the command line */
BOOL
getSleepTime (DWORD *);

BOOL
isQueryModeSet (BOOL * bQueryMode);

DWORD
FindWorkableTimeServers (TCHAR *pszServerList[], DWORD serverNum);

DWORD
ParseCommaSeparatedString (LPTSTR pszString, LPTSTR ppszList[], DWORD listMaxElements);

DWORD
GetTimeStampBasedOnTimeServers (BOOL isStart);

BOOL
GetTimeStampValue (NtpTimeRecord *pStart, NtpTimeRecord *pEnd, GtcTimeRecord *pGtcStart, GtcTimeRecord *pGtcEnd);

void
CalcTimeElapsed (
    NtpTimeRecord *pStart,
    NtpTimeRecord *pEnd,
    GtcTimeRecord *pGtcStart,
    GtcTimeRecord *pGtcEnd);

BOOL
compareTimerToNTP(VOID);

/*
 * NTP record structure
 */
struct ServerInUse
{
    ULONG serverIpAddress;      // server IP address
    DWORD isValid;              // the info in this structure is valid
    NtpTimeRecord start;        // time offset of the first call
    NtpTimeRecord end;          // time offset of the second call
    GtcTimeRecord gtcStart;
    GtcTimeRecord gtcEnd;
};

ServerInUse g_ActiveServer[NTP_ACTIVE_SERVERS];
DWORD g_ActiveServerNum = 0;

ULONGLONG g_ntpElapsed, g_rtcElapsed, g_gtcElapsed;
BOOL g_bMeasured = FALSE;


/*****************************************************************************
*
*                             Implementation
*
*****************************************************************************/

TESTPROCAPI usage(
                  UINT uMsg,
                  TPPARAM tpParam,
                  LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("           Compares the RTC/GTC to an NTP server                "));
    Info (_T(""));
    Info (_T("NTP uses the notion of offsets to record the time difference"));
    Info (_T("between two machines.  The offset is the difference between "));
    Info (_T("the time on two machines.  Since this test uses ntp, it is  "));
    Info (_T("easist to write this test in terms of offsets.  The test is "));
    Info (_T("for drift between the rtc/gtc on the host machine and the time  "));
    Info (_T("server.  This drift will show up as a shift in the offset   "));
    Info (_T("values.  If there is no drift the offsets should be the same"));
    Info (_T("throughout the test."));
    Info (_T(""));
    Info (_T("You can either specify the server name or use the list of"));
    Info (_T("default servers.  The query (-q) mode walks through either  "));
    Info (_T("the default server list or the the cmd line specified servers,"));
    Info (_T("determining which actually have NTP that we can access.     "));
    Info (_T(""));
    Info (_T("Args:"));
    Info (_T(""));
    Info (_T("  [-server <name1>,<name2>,<nameN>] [-sleepTime <time>] [-q]"));
    Info (_T("  <nameX> can be either IP (v4 only) or DNS."));
    Info (_T("  Use ',' to separate server names."));
    Info (_T("  Note: Do NOT insert space between server names."));
    Info (_T(""));
    returnVal = TPR_PASS;
cleanAndReturn:

    return (returnVal);
}

/* compare the rtc to the ntp server */
TESTPROCAPI compareRTCToNTP(
                            UINT uMsg,
                            TPPARAM tpParam,
                            LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    ULONGLONG rtcResolution = 0, looseResolution = 0, rtcRange = 0;
    ULONGLONG lowerBound = 0, fasterBound = 0;
    sTimer timer = { 0 };

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    /*
     * calculate loose resolution between NTP and RTC
     */

    Info (_T(""));
    Info (_T("Calculating RTC resolution, please wait."));

    /* System time timer */

    if (initTimer (&timer, &gTimeOfDayTimer) == FALSE)
    {
      Error (_T("Could not initalize the System Time (RTC) timer structure."));

      goto cleanAndReturn;
    }

    if (!getResolution(&timer, RTC_RESOLUTION_ITERS, &rtcResolution, &rtcRange, FALSE))
    {
      Error (_T("Failed to get resolution for timer %s."), timer.name);
      goto cleanAndReturn;
    }

    Info (_T("RTC resolution is %I64d"), rtcResolution);

    looseResolution = NTP_MAX_ROUND_TRIP_DELAY * MS_DIV_100NS;
    if (rtcResolution > looseResolution)
    {
        looseResolution = rtcResolution;
    }

    Info (_T("Loose resolution is %I64d"), looseResolution);

    /* Find workable time servers and get time stamps */
    if(!g_bMeasured)
    {
        if (!compareTimerToNTP())
        {
            Error (_T("Error getting time elapsed for RTC and NTP"));
            goto cleanAndReturn;
        }
        g_bMeasured = TRUE;
    }

    Info (_T(""));      // blank line in output


    /* calculate bounds, fasterbound +2 because the round-down then should add 2 for compensation */
    lowerBound = (g_ntpElapsed / looseResolution - 1) * looseResolution;
    fasterBound = (g_ntpElapsed / looseResolution + 2) * looseResolution;
    g_rtcElapsed = (g_rtcElapsed / looseResolution) * looseResolution;

    Info (_T("Raw NTP: %I64d and raw RTC: %I64d. All units in 100ns."), g_ntpElapsed, g_rtcElapsed);

    if (g_rtcElapsed >= lowerBound && g_rtcElapsed <= fasterBound)
    {
        Info (_T("No drift detected. NTP: %I64ds elapsed. RTC: %I64ds elapsed"),
            g_ntpElapsed / SECOND_DIV_100NS, g_rtcElapsed / SECOND_DIV_100NS);
    }
    else
    {
        Error (_T("Detected drift. NTP: %I64d (*100ns) elapsed."), g_ntpElapsed);
        Error (_T("RTC should be between %I64d and %I64d inclusive. Instead, it's %I64d."),
            lowerBound, fasterBound, g_rtcElapsed);
        goto cleanAndReturn;
    }

    Info (_T(""));

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);

}


/*
 * Compare the gtc to the ntp server.
 */
TESTPROCAPI compareGTCToNTP(
                 UINT uMsg,
                 TPPARAM tpParam,
                 LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    ULONGLONG dwTolerance = 0;
    ULONGLONG lowerBound = 0, fasterBound = 0;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

   /*
     * calculate loose resolution between NTP and GTC
     */
    ULONGLONG gtcResolution = 0, looseResolution = 0, gtcRange = 0;

    Info (_T(""));
    Info (_T("Calculating GTC resolution, please wait."));

    /* System time timer */
    sTimer timer;

    if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
        Error (_T("Could not initalize the GetTickCount (GTC) timer structure."));
        goto cleanAndReturn;
    }

    if (!getResolution(&timer, GTC_RESOLUTION_ITERS, &gtcResolution, &gtcRange, FALSE))
    {
        Error (_T("Failed to get resolution for timer %s."), timer.name);
        goto cleanAndReturn;
    }

    Info (_T("GTC resolution is %I64d"), gtcResolution);

    looseResolution = NTP_MAX_ROUND_TRIP_DELAY * MS_DIV_100NS;
    if (gtcResolution > looseResolution)
    {
        looseResolution = gtcResolution;
    }

    Info (_T("Loose resolution is %I64d"), looseResolution);

    /* Find workable time servers and get time stamps */
    if(!g_bMeasured)
    {
        if (!compareTimerToNTP())
        {
            Error (_T("Error getting time elapsed for GTC and NTP"));
            goto cleanAndReturn;
        }
        g_bMeasured = TRUE;
    }

    Info (_T(""));

    // Calculate a drift tolerance. OK to truncate since ntpElapsed is in 100ns units
    dwTolerance = (ULONGLONG)(g_ntpElapsed * GTC_DRIFT_TOLERANCE_PER_TICK);

    Info (_T("Note: Allowing a drift tolerance of +/- %I64d (*100ns)\r\n"), dwTolerance );

    /* calculate bounds, fasterbound */
    lowerBound = (g_ntpElapsed / looseResolution - 1) * looseResolution - dwTolerance;
    fasterBound = (g_ntpElapsed / looseResolution + 1) * looseResolution + dwTolerance ;

    Info (_T("Raw NTP: %I64d and raw GTC: %I64d. All units in 100ns."), g_ntpElapsed, g_gtcElapsed);

    if (g_gtcElapsed >= lowerBound && g_gtcElapsed <= fasterBound)
    {
        Info (_T("Tolerable drift detected. NTP: %I64ds elapsed. GTC: %I64ds elapsed"),
        g_ntpElapsed / SECOND_DIV_100NS, g_gtcElapsed / SECOND_DIV_100NS);
    }
    else
    {
        Error (_T("Detected intelorable drift. NTP: %I64d (*100ns) elapsed."), g_ntpElapsed);
        Error (_T("GTC should be between %I64d and %I64d inclusive. Instead, it's %I64d."),
            lowerBound, fasterBound, g_gtcElapsed);
        goto cleanAndReturn;
    }

    Info (_T(""));

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);

}


/* compare the rtc/gtc to the ntp server */
BOOL compareTimerToNTP(VOID)
{
    BOOL returnVal = FALSE;
    BOOL bCallWSACleanup = FALSE;   // set to true when call startup successfully

    DWORD dwIpVersion = 4;  // assume ipv4 unless set otherwise

    /* time server name */
    TCHAR szTimeServerCmdLine[DNS_MAX_NAME_BUFFER_LENGTH];

    TCHAR *pszTimeServers[MAX_IP_ADDRESS_NUMS];

    TCHAR *szDefaultTimeServers[] =
    {
        /* use this in case user did not provide a server */
        _T("time.windows.com"),
    };


    DWORD dwTimeServerNums = 0;

    DWORD dwSleepTimeS = DEFAULT_SLEEP_TIME_S;

    /*
     * set to true if we are in query mode, which says that we don't do
     * the test, only figure out if either the command line specified
     * server or the default ones are NTP servers that we can access.
     */
    BOOL bQueryMode = FALSE;


    /* get command line args.  If get false from either routine we have
     * an error.  Routine will print out messages, so we just exit in
     * this case.  We want to run both routines.
     */
    {
        if (!getSleepTime(&dwSleepTimeS))
        {
            goto cleanAndReturn;
        }

        if (!isQueryModeSet (&bQueryMode))
        {
            Error (_T("Couldn't process query mode."));
            goto cleanAndReturn;
        }

        if (getTimeServerInfo (szTimeServerCmdLine, DNS_MAX_NAME_BUFFER_LENGTH, &dwIpVersion)
            && ( (dwTimeServerNums = ParseCommaSeparatedString (szTimeServerCmdLine,
            pszTimeServers, MAX_IP_ADDRESS_NUMS)) > 0) )
        {
            /* use server from command line */
            Info (_T("Using server specified in command line"));
        }
    }

    /*
     * at this point command line args have been parsed.  Time server
     * name has been initialized and sleep time has been changed if user
     * desired.
     */

    /* figure out how long we will sleep */

    double doRealisticTime;
    TCHAR * pszRealisticUnits;

    realisticTime (double (dwSleepTimeS), doRealisticTime, &pszRealisticUnits);

    DWORD dwSleepTimeMS;

    if (DWordMult (dwSleepTimeS, 1000, &dwSleepTimeMS) != S_OK)
    {
        Error (_T("Overflow when calculating sleep time."));
        goto cleanAndReturn;
    }


    /* setup winsock */
    WORD wVersionRequested;
    wVersionRequested = MAKEWORD( 2, 2 );
    WSADATA wsaData;

    if (WSAStartup( wVersionRequested, &wsaData ) != 0)
    {
        Error (_T("WSAStartup Failed!"));
        goto cleanAndReturn;
    }

    /* successful call so cleanup */
    bCallWSACleanup = TRUE;

    Info (_T(""));
    Info (_T("NTP uses the notion of offsets to record the time difference"));
    Info (_T("between two machines.  The offset is the difference between"));
    Info (_T("the time on two machines.  Since this test uses ntp, it is "));
    Info (_T("easist to write this test in terms of offsets.  The test is "));
    Info (_T("for drift between the rtc on the host machine and the time "));
    Info (_T("server.  This drift will show up as a shift in the offset "));
    Info (_T("values.  If there is no drift the offsets should be the same"));
    Info (_T("throughout the test."));
    Info (_T(""));

    /* find a server that we can use */

    /* choose correct ip version and find workable time servers */
    switch (dwIpVersion)
    {
    case 4:
        g_ActiveServerNum = FindWorkableTimeServers (pszTimeServers, dwTimeServerNums);
        break;
    default:
        IntErr (_T("IP version not supported"));
        goto cleanAndReturn;
    }

    if (g_ActiveServerNum == 0)
    {
        Info (_T("Couldn't find a working server from command line."));
        Info (_T("Trying default servers"));

        /* using default servers */
        g_ActiveServerNum = FindWorkableTimeServers (szDefaultTimeServers,
            sizeof (szDefaultTimeServers) / sizeof (szDefaultTimeServers[0]));

        Info (_T(""));
        if (g_ActiveServerNum > 0) {
            Info (_T("Using default servers"));
        } else {
            /* couldn't find a workable server */
            Error (_T("Default time servers are not usable."));
            Error (_T(""));
            Error (_T("Please check the network connection of your device. "));
            Error (_T("Try checking cables or removing and inserting cards "));
            Error (_T("to make sure that your device detected it. "));
            Error (_T("After that if you could PING other machines in the LAN."));
            Error (_T("Then please check the Internet connection to make sure "));
            Error (_T("your decive can connect to Internet. Or use local NTP servers "));
            Error (_T("and specific their names or IP addresses in the command line."));
            goto cleanAndReturn;
        }
    }

    if (bQueryMode)
    {
        Info (_T("In query mode, so we are done.  Passing..."));
        returnVal = TPR_PASS;
        goto cleanAndReturn;
    }

    /* found a working server */

    Info (_T(""));
    Info (_T("*** Starting test ***"));
    Info (_T(""));

    BOOL rVal = FALSE;

    /* choose correct ip version and get the offset */
    switch (dwIpVersion)
    {
    case 4:
        /* get the start offset values */
        rVal = GetTimeStampBasedOnTimeServers (TRUE) > 0 ? TRUE : FALSE;
        break;
    default:
        IntErr (_T("IP version not supported"));
        rVal = FALSE;
    }

    if (!rVal)
    {
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("*** Sleeping for %g %s ***"), doRealisticTime, pszRealisticUnits);
    Info (_T(""));

    Sleep (dwSleepTimeMS);

    switch (dwIpVersion)
    {
    case 4:
        /* get the end offset values */
        rVal = GetTimeStampBasedOnTimeServers (FALSE) > 0 ? TRUE : FALSE;
        break;
    default:
        IntErr (_T("IP version not supported"));
        rVal = FALSE;
    }

    if (!rVal)
    {
        goto cleanAndReturn;
    }

    Info (_T(""));

    NtpTimeRecord startRec, endRec;
    GtcTimeRecord gtcStartRec, gtcEndRec;

    GetTimeStampValue (&startRec, &endRec, &gtcStartRec, &gtcEndRec);
    CalcTimeElapsed (&startRec, &endRec, &gtcStartRec, &gtcEndRec);

    Info (_T(""));

    returnVal = TRUE;

cleanAndReturn:

    if (bCallWSACleanup)
    {
        if (WSACleanup () != 0)
        {
            Error (_T("Error calling WSACleanup."));
            returnVal = FALSE;
        }
    }

    return returnVal;

}


/*
 * Get the time server name from the command line.  Return the name in
 * szTimeServerName.  Max size of this buffer is
 * dwTimeServerNameMaxLength.  The ip version of this server/transport
 * is return in dwIpVersion.
 *
 * The time server must be given on the command line.  The test takes
 * no default.
 *
 * This function prints error messages and the server name we are using
 * on success.
 */
BOOL
getTimeServerInfo (__out_ecount(dwTimeServerNameMaxLength) TCHAR * szTimeServerName, DWORD dwTimeServerNameMaxLength,
                   DWORD * ipVersion)
{
    BOOL returnVal = FALSE;

    if (!szTimeServerName || dwTimeServerNameMaxLength == 0 || !ipVersion)
    {
        return (FALSE);
    }

    /* currently only support ipv4 */
    *ipVersion = 4;

    /*
     * if user doesn't specify the -c param to tux the cmd line global
     * var is null.  In this case don't even try to parse command line
     * args.
     */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;
        TCHAR * szCmdString;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (getOptionString (&tokens, ARG_STR_SERVER_NAME, &szCmdString))
        {
            wcsncpy_s(szTimeServerName, dwTimeServerNameMaxLength, szCmdString, wcslen(szCmdString) );
            szTimeServerName[dwTimeServerNameMaxLength - 1] = 0;
        }
        else
        {
            Info (_T("No time server specified in command line."));
            goto cleanAndReturn;
        }
        returnVal = TRUE;
    }

cleanAndReturn:

    return (returnVal);
}

/*
 * Get the sleep time from the command line.  If it isn't given or the
 * option is bad then use the default.
 *
 * Print out which one we are using.
 */
BOOL
getSleepTime (DWORD * pdwSleepTimeS)
{
    BOOL returnVal = FALSE;

    if (!pdwSleepTimeS)
    {
        return (FALSE);
    }

    ULONGLONG ullSleepTimeS = (ULONGLONG) DEFAULT_SLEEP_TIME_S;

    BOOL bUserSuppliedParams = FALSE;

    /*
     * if user doesn't specify the -c param to tux the cmd line global
     * var is null.  In this case don't even try to parse command line
     * args.
     */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (getOptionTimeUlonglong (&tokens, ARG_STR_SLEEP_TIME,
            &ullSleepTimeS))
        {
            bUserSuppliedParams = TRUE;
        }
        else
        {
            /*
             * No option or bad option.  use default.  This is not
             * an error.
             */
        }
    }

    /* overflow? */
    if (ullSleepTimeS > (ULONGLONG) DWORD_MAX)
    {
        Error (_T("sleep time is too large."));
        goto cleanAndReturn;
    }

    /* convert to dword */
    *pdwSleepTimeS = (DWORD) ullSleepTimeS;

    /* convert to realistic time */
    double doTime;
    TCHAR * szUnits;
    realisticTime ((double) *pdwSleepTimeS, doTime, &szUnits);

    if (bUserSuppliedParams)
    {
        Info (_T("Using the user supplied sleep time, which is %g %s."),
            doTime, szUnits);
    }
    else
    {
        /* use default */
        Info (_T("Using the default sleep time, which is  %g %s"),
            doTime, szUnits);
    }

    returnVal = TRUE;
cleanAndReturn:

    return (returnVal);
}



BOOL
isQueryModeSet (BOOL * pbQueryMode)
{
    if (!pbQueryMode)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /*
     * if user doesn't specify the -c param to tux the cmd line global
     * var is null.  In this case don't even try to parse command line
     * args.
     */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (isOptionPresent (&tokens, ARG_STR_QUERY_MODE))
        {
            *pbQueryMode = TRUE;
        }
    }

    returnVal = TRUE;
cleanAndReturn:

    return (returnVal);
}


/*
 * Parse the string that separated with comma
 *
 * Caller provides the string to be separated, a pointer vector to hold the separated sub-string,
 * and the size of the pointer vector.
 * Return the number of parsed elements.
 */
DWORD
ParseCommaSeparatedString (
    LPTSTR pszString,
    LPTSTR ppszList[],
    DWORD listMaxElements)
{
    DWORD listElements = 0;
    TCHAR *pszToken = NULL;
    TCHAR seps[] = _T(",");

    if (pszString == NULL || ppszList == NULL || listMaxElements == 0)
    {
        goto cleanup;
    }

    /* first element is at the beginning */
    ppszList[listElements++] =  pszString;

    TCHAR *szNextToken = NULL;
    pszToken = _tcstok_s(pszString, seps, &szNextToken);
    while ( (pszToken = _tcstok_s(NULL, seps, &szNextToken)) != NULL
        && listElements < listMaxElements)
    {
        ppszList[listElements++] =  pszToken;
    }

cleanup:
    return listElements;
}


/*
 * Find workable time servers.
 * Return the number of found workable servers.
 */
DWORD
FindWorkableTimeServers (TCHAR *pszServerList[], DWORD serverNum)
{
    DWORD foundServerNum = 0;

    for (DWORD serverIndex = 0; (serverIndex < serverNum)
        && (foundServerNum < NTP_ACTIVE_SERVERS); serverIndex++)
    {
        ULONG serverIP;
        if (!GetHostIPAddress (pszServerList[serverIndex], &serverIP))
        {
            Info (_T("Couldn't get host ip address from %s"), pszServerList[serverIndex]);
            Info (_T(""));
            continue;
        }

        if (NtpGetValidTimeStamp (serverIP, NULL, NULL))
        {
            /* found a workable server */
            g_ActiveServer[foundServerNum].serverIpAddress = serverIP;
            g_ActiveServer[foundServerNum].isValid = TRUE;
            foundServerNum++;
        }
    }
    return foundServerNum;
}


/*
 * Get the time stamp based on NTP/SNTP servers
 * Passing in TRUE at the first call. And passing in FALSE at succeed call.
 * Return the number of valid records.
 */
DWORD
GetTimeStampBasedOnTimeServers (BOOL isStart)
{
    DWORD validCounts = 0;

    for (DWORD i = 0; i < g_ActiveServerNum; i++)
    {
        if (g_ActiveServer[i].isValid)
        {
            NtpTimeRecord *pTimeRec;
            GtcTimeRecord *pGtcRec;
            if (isStart) {
                pTimeRec = &g_ActiveServer[i].start;
                pGtcRec = &g_ActiveServer[i].gtcStart;
            }
            else {
                pTimeRec = &g_ActiveServer[i].end;
                pGtcRec = &g_ActiveServer[i].gtcEnd;
            }
            g_ActiveServer[i].isValid = NtpGetValidTimeStamp (g_ActiveServer[i].serverIpAddress, pTimeRec, pGtcRec);
        }
        if (g_ActiveServer[i].isValid) {
            validCounts++;
        }
    }
    return validCounts;
}


/*
 * Get the pre-stored time stamp values.
 * Return TURE if there is a valid record; Otherwise return FALSE
 */
BOOL
GetTimeStampValue (NtpTimeRecord *pStart, NtpTimeRecord *pEnd, GtcTimeRecord *pGtcStart, GtcTimeRecord *pGtcEnd)
{
    BOOL bFound = FALSE;
    for (DWORD i = 0; !bFound && (i < g_ActiveServerNum); i++)
    {
        if (g_ActiveServer[i].isValid)
        {
            if(pStart && pEnd)
            {
                *pStart = g_ActiveServer[i].start;
                *pEnd = g_ActiveServer[i].end;
            }
            if (pGtcStart && pGtcEnd)
            {
                *pGtcStart = g_ActiveServer[i].gtcStart;
                *pGtcEnd = g_ActiveServer[i].gtcEnd;
            }
            bFound = TRUE;
        }
    }
    return bFound;
}


/*
 * Calculate NTP & RTC time elapsed
 */
void
CalcTimeElapsed (
    NtpTimeRecord *pStart,
    NtpTimeRecord *pEnd,
    GtcTimeRecord *pGtcStart,
    GtcTimeRecord *pGtcEnd)
{
    LARGE_INTEGER tS1, tS2, tE1, tE2;

    /* calculate NTP time elapsed */
    tS1.HighPart = pStart->svRecvTime.dwHighDateTime;
    tS1.LowPart  = pStart->svRecvTime.dwLowDateTime;
    tS2.HighPart = pStart->svSendTime.dwHighDateTime;
    tS2.LowPart  = pStart->svSendTime.dwLowDateTime;
    tE1.HighPart = pEnd->svRecvTime.dwHighDateTime;
    tE1.LowPart  = pEnd->svRecvTime.dwLowDateTime;
    tE2.HighPart = pEnd->svSendTime.dwHighDateTime;
    tE2.LowPart  = pEnd->svSendTime.dwLowDateTime;

    g_ntpElapsed = ((tE1.QuadPart - tS1.QuadPart) + (tE2.QuadPart - tS2.QuadPart)) / 2;

    /* calculate RTC time elapsed */
    tS1.HighPart = pStart->clSendTime.dwHighDateTime;
    tS1.LowPart  = pStart->clSendTime.dwLowDateTime;
    tS2.HighPart = pStart->clRecvTime.dwHighDateTime;
    tS2.LowPart  = pStart->clRecvTime.dwLowDateTime;
    tE1.HighPart = pEnd->clSendTime.dwHighDateTime;
    tE1.LowPart  = pEnd->clSendTime.dwLowDateTime;
    tE2.HighPart = pEnd->clRecvTime.dwHighDateTime;
    tE2.LowPart  = pEnd->clRecvTime.dwLowDateTime;

    g_rtcElapsed = ((tE1.QuadPart - tS1.QuadPart) + (tE2.QuadPart - tS2.QuadPart)) / 2;

    /* calculate GTC time elapsed */
    g_gtcElapsed = (((LONGLONG)(pGtcEnd->gtcSendTime - pGtcStart->gtcSendTime) +
                        (LONGLONG)(pGtcEnd->gtcRecvTime - pGtcStart->gtcRecvTime)) / 2) * MS_DIV_100NS;

}


