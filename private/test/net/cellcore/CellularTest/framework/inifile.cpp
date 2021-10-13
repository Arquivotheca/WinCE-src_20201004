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
#include <stdio.h>
#include "istring.h"
#include "radiotest.h"
#include "radiotest_utils.h"
#include "radiotest_data.h"
#include "framework.h"


// Strings denoting start/end of test sequence information
#define BEGIN_SEQUENCE_STRING   TEXT("BeginTestSequence")
#define END_SEQUENCE_STRING     TEXT("EndTestSequence")

// Grabs string from INI file using stock test routine
static BOOL GetString(LPVOID pIS, LPVOID lpvBuffer, DWORD dwBuffChars, LPTSTR ptszKey, LPTSTR ptszFilename, BOOL fSilent)
{
    LPTSTR ptsz = IString_Load( pIS, ptszKey );

    if(ptsz)
    {
        if (dwBuffChars < 2048) {      // Prefast size validation
            StringCchPrintf((TCHAR*)lpvBuffer, dwBuffChars, TEXT("%s"), ptsz);
            return TRUE;
        }
    }
    else
    {
        if (!fSilent)
            g_pLog->Log(UI_LOG, TEXT("**** Failed to find %s within %s"), ptszKey, ptszFilename);
    }
    return FALSE;
}

//  Grabs DWORD from INI file using stock test routine
static BOOL GetDword(LPVOID pIS, DWORD &dw, LPTSTR ptszKey, LPTSTR ptszFilename, BOOL fSilent)
{
    LPTSTR ptsz = IString_Load( pIS, ptszKey );

    if(ptsz)
    {
        dw=(DWORD)_wtoi(ptsz);
        return TRUE;
    }
    else
    {
        if (!fSilent)
            g_pLog->Log(UI_LOG, TEXT("**** Failed to find %s within %s"), ptszKey, ptszFilename);
        return FALSE;
    }
}


// Parses Stinger.Ini and extracts information contained within such as dialup server numbers, etc
BOOL ReadStaticTestData(VOID)
{
    LPVOID pIS=NULL;

    // Open stinger.ini input file using dubious routine
    if( !IString_Init(&pIS, INI_FILENAME) || !pIS )
    {
        g_pLog->Log(UI_LOG, TEXT("Failed to open %s"), INI_FILENAME);
        return FALSE;
    }

    // Dialup server voice answer line
    if(!g_TestData.tszNumber_VoiceAnswer[0] 
        && !GetString( pIS, g_TestData.tszNumber_VoiceAnswer, TELEPHONE_NUMBER_LENGTH, _T("DS_VOICE_NUMBER0"), INI_FILENAME, FALSE ))
        return FALSE;

    // Dialup server conference line 1
    if(!g_TestData.tszNumber_VoiceConference1[0] 
        && !GetString( pIS, g_TestData.tszNumber_VoiceConference1, TELEPHONE_NUMBER_LENGTH, _T("DS_VOICE_NUMBER1"), INI_FILENAME, FALSE ))
        return FALSE;

    // Dialup server conference line 2
    if(!g_TestData.tszNumber_VoiceConference2[0] 
        && !GetString( pIS, g_TestData.tszNumber_VoiceConference2, TELEPHONE_NUMBER_LENGTH, _T("DS_VOICE_NUMBER2"), INI_FILENAME, FALSE ))
        return FALSE;

    // Dialup server voice busy line
    if(!GetString( pIS, g_TestData.tszNumber_VoiceBusy, TELEPHONE_NUMBER_LENGTH, _T("DS_NUMBER_BUSY"), INI_FILENAME, FALSE ))
        return FALSE;

    // Dialup server voice no answer line
    if(!GetString( pIS, g_TestData.tszNumber_VoiceNoAnswer, TELEPHONE_NUMBER_LENGTH, _T("DS_NUMBER_NoAnswr"), INI_FILENAME, FALSE ))
        return FALSE;
    
    // Dialup server CSD answer line
    if(!GetString( pIS, g_TestData.tszNumber_CSDAnswer, TELEPHONE_NUMBER_LENGTH, _T("DS_DATA_NUMBER0"), INI_FILENAME, FALSE ))
        return FALSE;
    
    // APN count
    g_TestData.dwGPRSAPNCount = 1; // default = 1
    if(GetDword(pIS, g_TestData.dwGPRSAPNCount, _T("MAX_GPRS_APN_COUNT"), INI_FILENAME, TRUE))
    {
        if (g_TestData.dwGPRSAPNCount > MAX_GPRS_ACTIVATED_CONTEXTS)
        {
            g_TestData.dwGPRSAPNCount = MAX_GPRS_ACTIVATED_CONTEXTS;
        }
        Log(TEXT("APN count = %d"), g_TestData.dwGPRSAPNCount);
    }
    
    // APN1 ~ APNx
    for (DWORD i = 0; i < g_TestData.dwGPRSAPNCount; i++)
    {
        // APN name
        {
            TCHAR tszAPN[MAX_PATH];

            StringCchPrintf(tszAPN, MAX_PATH, _T("GPRS_APN%d"), i+1);
            if(!g_TestData.tszGPRS_APN[i][0] 
                && !GetString(pIS, g_TestData.tszGPRS_APN[i], APN_LENGTH, tszAPN, INI_FILENAME, TRUE))
            {
                Log(TEXT("!ERROR: Failed to find %s within stinger.ini file"), tszAPN);
                return FALSE;
            }
            else
            {
                Log(TEXT("APN%d[%s]"), i+1, g_TestData.tszGPRS_APN[i]);
            }
        }

        // user name
        {
            TCHAR tszUser[MAX_PATH];

            StringCchPrintf(tszUser, MAX_PATH, _T("GPRS_USERNAME%d"), i+1);
            if(!g_TestData.tszGPRS_USER_NAME[i][0] 
                && !GetString(pIS, g_TestData.tszGPRS_USER_NAME[i], GPRS_USER_NAME_LENGTH, tszUser, INI_FILENAME, TRUE))
            {
                Log(TEXT("!WARNING: Failed to find %s within stinger.ini file"), tszUser);
            }
            else
            {
                Log(TEXT("Username%d[%s]"), i+1, g_TestData.tszGPRS_USER_NAME[i]);
            }
        }

        // password
        {
            TCHAR tszPassword[MAX_PATH];

            StringCchPrintf(tszPassword, MAX_PATH, _T("GPRS_PASSWORD%d"), i+1);
            if(!g_TestData.tszGPRS_PASSWORD[i][0] 
                && !GetString(pIS, g_TestData.tszGPRS_PASSWORD[i], GPRS_PASSWORD_LENGTH, tszPassword, INI_FILENAME, TRUE))
            {
                Log(TEXT("!WARNING: Failed to find %s within stinger.ini file"), tszPassword);
            }
            else
            {
                Log(TEXT("Password%d[%s]"), i+1, g_TestData.tszGPRS_PASSWORD[i]);
            }
        }

        // domain
        {
            TCHAR tszDomain[MAX_PATH];

            StringCchPrintf(tszDomain, MAX_PATH, _T("GPRS_DOMAIN%d"), i+1);
            if(!g_TestData.tszGPRS_DOMAIN[i][0] 
                && !GetString(pIS, g_TestData.tszGPRS_DOMAIN[i], GPRS_DOMAIN_LENGTH, tszDomain, INI_FILENAME, TRUE))
            {
                Log(TEXT("!WARNING: Failed to find %s within stinger.ini file"), tszDomain);
            }
            else
            {
                Log(TEXT("Password%d[%s]"), i+1, g_TestData.tszGPRS_DOMAIN[i]);
            }
        }        
    }

    // Multiple APN test: number of bytes to download per APN
    if(!GetDword(pIS, g_TestData.dwMAPNSize, _T("GPRS_MAPN_SIZE"), INI_FILENAME, TRUE)) {
        g_TestData.dwMAPNSize = DEFAULT_GPRS_MAPN_SIZE;
    }

    // Number of blocks to download per data transfer test
    if(!GetDword( pIS, g_TestData.dwBlocksPerSession, _T("BLOCKS_PER_SESSION"), INI_FILENAME, TRUE )) {
        g_TestData.dwBlocksPerSession = DEFAULT_BLOCK_PER_SESSION;
    }
    
    // Size of each block
    if(!GetDword( pIS, g_TestData.dwBlockSize, _T("BLOCK_SIZE"), INI_FILENAME, TRUE )) {
        g_TestData.dwBlockSize = DEFAULT_BLOCK_SIZE;
    }

    if(!GetDword(pIS, g_TestData.dwUploadSize, _T("UPLOAD_SIZE"), INI_FILENAME, TRUE)) {
        g_TestData.dwUploadSize = DEFAULT_UPLOAD_SIZE;
    }

    // URL to pull data from
    if(!GetString( pIS, g_TestData.tszDownloadURL, URL_LENGTH, _T("DOWNLOAD_URL"), INI_FILENAME, TRUE )) {
        _tcscpy_s(g_TestData.tszDownloadURL, URL_LENGTH, DEFAULT_DOWNLOAD_PATH);
    }
    
    if(!GetString( pIS, g_TestData.tszUploadURL, URL_LENGTH, _T("UPLOAD_URL"), INI_FILENAME, TRUE )) {
        _tcscpy_s(g_TestData.tszUploadURL, URL_LENGTH, DEFAULT_UPLOAD_PATH);
    }
    
    // Maximum number of bytes expected to be transferred over GPRS
    if(!GetDword( pIS, g_TestData.dwTotalGPRSBytesExpected, _T("TOTAL_GPRS_SIZE"), INI_FILENAME, TRUE))
    {
        g_TestData.dwTotalGPRSBytesExpected= 0;   // N/A
    }

    // Wininet timeouts
    if(!GetDword( pIS, g_dwWinInetConnectTimeoutMinute, _T("CONN_TIMEOUT"), INI_FILENAME, TRUE))
    {
        g_dwWinInetConnectTimeoutMinute = DEFAULT_CONN_TIMEOUT;
    }
    
    if(!GetDword( pIS, g_dwWinInetDataTimeoutMinute, _T("DATA_TIMEOUT"), INI_FILENAME, TRUE))
    {
        g_dwWinInetDataTimeoutMinute = DEFAULT_DATA_TIMEOUT;
    }

    // HTTP (GPRS) Proxy Name - optional
    if (!GetString( pIS, g_TestData.tszGPRSHTTPProxy, MAX_PATH, _T("GPRS_HTTP_PROXY"), INI_FILENAME, TRUE))
    {
        // No default, just make sure the string is NULL
        g_TestData.tszGPRSHTTPProxy[0] = _T('\0');
    }
    
    // HTTP (CSD) Proxy Name - optional
    if (!GetString( pIS, g_TestData.tszCSDHTTPProxy, MAX_PATH, _T("CSD_HTTP_PROXY"), INI_FILENAME, TRUE))
    {
        // No default, just make sure the string is NULL
        g_TestData.tszCSDHTTPProxy[0] = _T('\0');
    }

    // SIM PIN - CHV1
    if(!GetString( pIS, g_TestData.tszSIM_PIN, PIN_LENGTH, _T("RILT_SIM_PWD"), INI_FILENAME, FALSE ))
        return FALSE;

    // Whether network supports call busy
    DWORD   dwCallBusyVal = 0;
    if(!GetDword( pIS, dwCallBusyVal, _T("RILT_CALLBUSY"), INI_FILENAME, TRUE ))
        g_TestData.fCallBusy = TRUE;        // default is true
    else
        g_TestData.fCallBusy = (dwCallBusyVal != 0);

    // DS Delay
    if(!GetDword( pIS, g_TestData.dwDsDelay, _T("DS_DELAY"), INI_FILENAME, TRUE ))
        g_TestData.dwDsDelay = DEFAULT_DS_DELAY;

    // DS sleep
    if(!GetDword( pIS, g_TestData.dwDsSleep, _T("DS_SLEEP"), INI_FILENAME, TRUE))
        g_TestData.dwDsSleep = DEFAULT_DS_SLEEP;

    // DS Call duration sleep
    if(!GetDword( pIS, g_TestData.dwDsCallDuration, _T("DS_CALL_DURATION"), INI_FILENAME, TRUE ))
        g_TestData.dwDsCallDuration = DEFAULT_DS_CALL_DURATION;

    // Long call duration
    if(!GetDword( pIS, g_TestData.dwLongCallDuration, _T("LONG_CALL_DURATION"), INI_FILENAME, TRUE ))
        g_TestData.dwLongCallDuration = DEFAULT_LONG_CALL_DURATION;
    
    // Busy sleep
    if(!GetDword( pIS, g_TestData.dwBusySleep, _T("DS_BUSY_SLEEP"), INI_FILENAME, TRUE ))
        g_TestData.dwBusySleep = DEFAULT_BUSY_SLEEP;

    // SMS sleep
    if(!GetDword( pIS, g_TestData.dwSMSSleep, _T("SMS_SLEEP"), INI_FILENAME, TRUE))
        g_TestData.dwSMSSleep = DEFAULT_SMS_SLEEP;

    // SMS timeout
    if(!GetDword( pIS, g_TestData.dwSMSTimeout, _T("SMS_TIMEOUT"), INI_FILENAME, TRUE))
        g_TestData.dwSMSTimeout = DEFAULT_SMS_TIMEOUT;

    // Network delay, used mainly for supplementary services test cases
    if (!GetDword( pIS, g_TestData.dwNWDelay, _T("NW_DELAY"), INI_FILENAME, TRUE))
        g_TestData.dwNWDelay = DEFAULT_NW_DELAY;

    // TAPI_TIMEOUT, used for WaitForLINE_CALLSTATE, such as waiting for line to become idle
    if (!GetDword( pIS, g_TestData.dwTAPITimeout, _T("TAPI_TIMEOUT"), INI_FILENAME, TRUE))
        g_TestData.dwTAPITimeout = DEFAULT_TAPI_TIMEOUT;

    // Own number
    // It is acceptable for this to be blank at this stage since we will read the MSISDN from the SIM later
    if (!g_TestData.tszNumber_OwnNumber[0] 
        && !GetString( pIS, g_TestData.tszNumber_OwnNumber, TELEPHONE_NUMBER_LENGTH, _T("SIM_VOICE_NUMBER"), INI_FILENAME, TRUE ))
    {
        g_TestData.tszNumber_OwnNumber[0] = _T('\0');
    }
    
    if(!GetDword(pIS, g_TestData.dwStressIteration, _T("STRESS_ITERATION"), INI_FILENAME, TRUE))
    {
        g_TestData.dwStressIteration = DEFAULT_STRESS_ITERATION;
    }

    // Call forwarding number - optional
    GetString( pIS, g_TestData.tszNumber_CallForward, TELEPHONE_NUMBER_LENGTH, _T("SIM_CALL_FORWARD_NUMBER"), INI_FILENAME, TRUE );

    g_pLog->Log(LOG, TEXT("---- %s ----"), INI_FILENAME);
    g_pLog->Log(LOG, TEXT("Own Number = %s"), (g_TestData.tszNumber_OwnNumber[0] == _T('\0')) ? _T("<Not specified>") : g_TestData.tszNumber_OwnNumber);
    g_pLog->Log(LOG, TEXT("GPRS APN = %s"), g_TestData.tszGPRS_APN[0]);
    g_pLog->Log(LOG, TEXT("SIM PIN = %s"), g_TestData.tszSIM_PIN);
    g_pLog->Log(LOG, TEXT("Dialup Server Voice = %s"), g_TestData.tszNumber_VoiceAnswer);
    g_pLog->Log(LOG, TEXT("Dialup Server Busy = %s"), g_TestData.tszNumber_VoiceBusy);
    g_pLog->Log(LOG, TEXT("Dialup Server Conference 1 = %s"), g_TestData.tszNumber_VoiceConference1);
    g_pLog->Log(LOG, TEXT("Dialup Server Conference 2 = %s"), g_TestData.tszNumber_VoiceConference2);
    g_pLog->Log(LOG, TEXT("Dialup Server Data = %s"), g_TestData.tszNumber_CSDAnswer);
    g_pLog->Log(LOG, TEXT("Dialup Server Sleep = %d secs"), g_TestData.dwDsSleep);
    g_pLog->Log(LOG, TEXT("Dialup Server Delay = %d secs"), g_TestData.dwDsDelay);
    g_pLog->Log(LOG, TEXT("SMS Sleep = %d secs"), g_TestData.dwSMSSleep);
    if (g_TestData.tszNumber_CallForward[0])
    {
        g_pLog->Log(LOG, TEXT("Call forwarding = %s"), g_TestData.tszNumber_CallForward);
    }
    g_pLog->Log(LOG, TEXT("Network delay = %d secs"), g_TestData.dwNWDelay);
    g_pLog->Log(LOG, TEXT("TAPI Timeout = %d secs"), g_TestData.dwTAPITimeout);
    g_pLog->Log(LOG, TEXT("GPRS proxy = %s"), g_TestData.tszGPRSHTTPProxy);
    g_pLog->Log(LOG, TEXT("Stress Iteration = %u"), g_TestData.dwStressIteration);  

    // print other configuration of stinger.ini
    PrintConfiguration();

    // Close file and free any memory that we consumed in the process
    IString_DeInit(&pIS);
    return TRUE;
}

void PrintConfiguration()
{
    g_pLog->Log(LOG, TEXT("tszDownloadURL = %s"), g_TestData.tszDownloadURL);
    g_pLog->Log(LOG, TEXT("dwBlocksPerSession = %d"), g_TestData.dwBlocksPerSession);
    g_pLog->Log(LOG, TEXT("dwBlockSize = %d"), g_TestData.dwBlockSize);
    g_pLog->Log(LOG, TEXT("tszUploadURL = %s"), g_TestData.tszUploadURL);
    g_pLog->Log(LOG, TEXT("dwUploadSize = %d"), g_TestData.dwUploadSize);   
    g_pLog->Log(LOG, TEXT("dwTotalGPRSBytesExpected = %d"), g_TestData.dwTotalGPRSBytesExpected);
    g_pLog->Log(LOG, TEXT("dwLongCallDuration = %d"), g_TestData.dwLongCallDuration);
    g_pLog->Log(LOG, TEXT("dwDsSleep = %d"), g_TestData.dwDsSleep);
    g_pLog->Log(LOG, TEXT("dwDsCallDuration = %d"), g_TestData.dwDsCallDuration);
    g_pLog->Log(LOG, TEXT("dwBusySleep = %d"), g_TestData.dwBusySleep);
    g_pLog->Log(LOG, TEXT("dwSMSSleep = %d"), g_TestData.dwSMSSleep);
    g_pLog->Log(LOG, TEXT("dwDurationHrs = %d"), g_TestData.dwDurationHrs);
    g_pLog->Log(LOG, TEXT("dwConsecutiveFailsAllowed = %d"), g_TestData.dwConsecutiveFailsAllowed);
    g_pLog->Log(LOG, TEXT("dwConsecutiveFailsOccurred = %d"), g_TestData.dwConsecutiveFailsOccurred);
    g_pLog->Log(LOG, TEXT("uliTestEndTime = %d"), g_TestData.uliTestEndTime);
    g_pLog->Log(LOG, TEXT("fComputeMetric = %d"), g_TestData.fComputeMetric);
    g_pLog->Log(LOG, TEXT("fWeightedRandomTests = %d"), g_TestData.fWeightedRandomTests);
    g_pLog->Log(LOG, TEXT("fCDMA = %d"), g_TestData.fCDMA);
    g_pLog->Log(LOG, TEXT("fSuspendResume = %d"), g_TestData.fSuspendResume);
    g_pLog->Log(LOG, TEXT("fCallBusy = %d"), g_TestData.fCallBusy);
    g_pLog->Log(LOG, TEXT("dwTestDelaySeconds = %d"), g_TestData.dwTestDelaySeconds);
    g_pLog->Log(LOG, TEXT("dwNWDelay = %d"), g_TestData.dwNWDelay);
    g_pLog->Log(LOG, TEXT("dwTAPITimeout = %d"), g_TestData.dwTAPITimeout);
    g_pLog->Log(LOG, TEXT("dwCallDurationMinutes = %d"), g_TestData.dwCallDurationMinutes);
    g_pLog->Log(LOG, TEXT("dwDHCPSleepMinutes = %d"), g_TestData.dwDHCPSleepMinutes);
    g_pLog->Log(LOG, TEXT("dwGPRSSleepMinutes = %d"), g_TestData.dwGPRSSleepMinutes);
}

// Parses RadioTest.Ini (or a file specified by the user through the command-line)
// and extracts test configuration information. Test composition, cycles, etc.
BOOL ReadTestConfig(VOID)
{
    return true;
}
