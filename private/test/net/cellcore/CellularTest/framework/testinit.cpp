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
#include <Ccoreutl.h>
#include <simmgr.h>
#include <tsp.h>

#include "framework.h"
#include "radiotest.h"
#include "radiotest_tsp.h"
#include "radiotest_sim.h"
#include "radiotest_sms.h"
#include "radiotest_metric.h"
#include "radiotest_utils.h"

TEST_DATA_T g_TestData;

extern CSIMTest * g_pSIMTest;

static BOOL fRestoreLock = FALSE;

TEST_STATUS g_TestStatus;

void SetTestStatus(CASE_STATUS *pCS, BOOL fOK, LPCTSTR pCaseName, DWORD dwTestPriority)
{
    if (!pCS->pszCaseName)
    {
        pCS->pszCaseName = pCaseName;
    }

    if (!pCS->dwPriority)
    {
        pCS->dwPriority = dwTestPriority;
    }
    
    if (fOK)
    {
        ++(pCS->dwPass);
    }
    else
    {
        ++(pCS->dwFail);
    }
}


int CalculateTestScore()
{
    const DWORD WEIGHT_VOICE    = 1000;
    const DWORD WEIGHT_DATA     = 800;
    const DWORD WEIGHT_SMS      = 400;
    const DWORD WEIGHT_SIM      = 200;
    const DWORD WEIGHT_COMPLEX  = 200;
    const DWORD WEIGHT_RADIO    = 100;

    DWORD dwTestScore = 0;
    DWORD dwTotalScore = 0;

    // voice score
    DWORD dwVoicePassed = 0, dwVoiceTotal = 0;
    
    DWORD dwCaseNumber = sizeof(g_TestStatus) / sizeof(CASE_STATUS);
    CASE_STATUS *pCS = (CASE_STATUS *)&g_TestStatus;
    for (DWORD i = 0; i < dwCaseNumber; ++i)
    {
        const WCHAR *VOICE_PREFIX = L"Voice_";
        if (pCS[i].pszCaseName 
            && (0 == _wcsnicmp(pCS[i].pszCaseName, VOICE_PREFIX, wcslen(VOICE_PREFIX))))
        {
            dwVoicePassed += pCS[i].dwPass;
            dwVoiceTotal += pCS[i].dwPass + pCS[i].dwFail;
        }
    }

    if (dwVoiceTotal > 0)
    {
        dwTestScore += WEIGHT_VOICE * dwVoicePassed / dwVoiceTotal;
        dwTotalScore += WEIGHT_VOICE;
    }

    // data score
    const DWORD DATA_WEIGHT_HIGH_TO_LOW_RATIO = 10;
    DWORD dwDataPassed = 0, dwDataTotal = 0;
    
    dwDataPassed = (g_TestStatus.Data_Download.dwPass + g_TestStatus.Data_Upload.dwPass) * DATA_WEIGHT_HIGH_TO_LOW_RATIO 
        + (g_TestStatus.Data_GPRSMultipleAPNs.dwPass + g_TestStatus.Data_GPRSSequentialAPNs.dwPass);

    dwDataTotal = dwDataPassed + (g_TestStatus.Data_Download.dwFail+ g_TestStatus.Data_Upload.dwFail) * DATA_WEIGHT_HIGH_TO_LOW_RATIO 
        + (g_TestStatus.Data_GPRSMultipleAPNs.dwFail + g_TestStatus.Data_GPRSSequentialAPNs.dwFail);

    if (dwDataTotal > 0)
    {
        dwTestScore += WEIGHT_DATA * dwDataPassed / dwDataTotal;
        dwTotalScore += WEIGHT_DATA;
    }

    // SMS score
    DWORD dwSMSPassed = 0, dwSMSTotal = 0;
    dwSMSPassed = g_TestStatus.SMS_SendRecv.dwPass + g_TestStatus.SMS_LongMessage.dwPass;
    dwSMSTotal = dwSMSPassed + g_TestStatus.SMS_SendRecv.dwFail + g_TestStatus.SMS_LongMessage.dwFail;

    if (dwSMSTotal > 0)
    {
        dwTestScore += WEIGHT_SMS * dwSMSPassed / dwSMSTotal;
        dwTotalScore += WEIGHT_SMS;
    }

    // SIM score
    const DWORD SIM_WEIGHT_HIGH_TO_LOW_RATIO = 5;
    DWORD dwSIMPassed = 0, dwSIMTotal = 0;
    
    dwSIMPassed = g_TestStatus.SIM_PhoneBookRead.dwPass * SIM_WEIGHT_HIGH_TO_LOW_RATIO 
        + g_TestStatus.SIM_PhoneBook.dwPass + g_TestStatus.SIM_SMS.dwPass;

    dwSIMTotal = dwSIMPassed + g_TestStatus.SIM_PhoneBookRead.dwFail * SIM_WEIGHT_HIGH_TO_LOW_RATIO 
        + g_TestStatus.SIM_PhoneBook.dwFail + g_TestStatus.SIM_SMS.dwFail;

    if (dwSIMTotal > 0)
    {
        dwTestScore += WEIGHT_SIM * dwSIMPassed / dwSIMTotal;
        dwTotalScore += WEIGHT_SIM;
    }

    // Complex scenario score
    DWORD dwComplexPassed = 0, dwComplexTotal = 0;
    dwComplexPassed = g_TestStatus.Complex_Voice_Data.dwPass + g_TestStatus.Complex_Voice_SMS.dwPass;
    dwComplexTotal = dwComplexPassed + g_TestStatus.Complex_Voice_SMS.dwFail + g_TestStatus.Complex_Voice_SMS.dwFail;

    if (dwComplexTotal > 0)
    {
        dwTestScore += WEIGHT_COMPLEX * dwComplexPassed / dwComplexTotal;
        dwTotalScore += WEIGHT_COMPLEX;
    }

    // Radio score
    DWORD dwRadioPassed = 0, dwRadioTotal = 0;
    dwRadioPassed = g_TestStatus.RadioOnOff.dwPass;
    dwRadioTotal = dwRadioPassed + g_TestStatus.RadioOnOff.dwFail;

    if (dwRadioTotal > 0)
    {
        dwTestScore += WEIGHT_RADIO * dwRadioPassed / dwRadioTotal;
        dwTotalScore += WEIGHT_RADIO;
    }

    // calculate test score
    int iScore = -1;
    if (dwTotalScore > 0)
    {
        iScore = (int)(dwTestScore * 100 / dwTotalScore);
    }
    
    return iScore;
}



BOOL TEST_Summary(DWORD dwUserData)
{
    DWORD dwCaseNumber = sizeof(g_TestStatus) / sizeof(CASE_STATUS);
    CASE_STATUS *pCS = (CASE_STATUS *)&g_TestStatus;
    
    g_pLog->Log(LOG, L" ");
    g_pLog->Log(LOG, L"Test Result Summary:");
    g_pLog->Log(LOG, L"Test Name                Priority  Pass    Fail    Pass Rate(%%)");

    for (DWORD i = 0; i < dwCaseNumber; i++)
    {
        if (pCS[i].pszCaseName)
        {
            DWORD dwPass = 100;     // pass rate is 100%
            
            if (pCS[i].dwFail != 0) // pass rate < 100%
            {
                dwPass = (pCS[i].dwPass * 100) / (pCS[i].dwPass + pCS[i].dwFail);
            }
            
            g_pLog->Log(LOG, L"%-25s%-10d%-8d%-8d%-13d", 
                pCS[i].pszCaseName, pCS[i].dwPriority, pCS[i].dwPass, pCS[i].dwFail, dwPass);
        }
    }
    g_pLog->Log(LOG, L" ");

    extern DATA_TEST_RESULTS g_DataTestResults;
    DWORD dwThroughput = 0;
    DWORD dwTransferred = 0;
    DWORD dwDuration = 0;
    
    g_pLog->Log(LOG, L"Data Performance Summary:");
    g_pLog->Log(LOG, L"Scenario  Transferred(KB)   Duration(ms)   Throughput(KB/s)");

    if (g_DataTestResults.dwGPRS_BytesTransferred > 0)
    {
        dwDuration = g_DataTestResults.dwGPRS_MSTransfer;
        dwTransferred = g_DataTestResults.dwGPRS_BytesTransferred / 1024;
    }
    
    if (dwDuration > 0)
    {
        dwThroughput = (ULONGLONG)dwTransferred * 1000 / dwDuration;
        g_pLog->Log(LOG, L"%-10s%-18d%-15d%-16d", L"Download", dwTransferred, dwDuration, dwThroughput);
    }
    else
    {
        g_pLog->Log(LOG, L"%-10s%-18d%-15dN/A", L"Download", dwTransferred, dwDuration);
    }

    dwDuration = 0;
    dwTransferred = 0;
    if (g_DataTestResults.dwUpload_BytesTransferred > 0)
    {
        dwDuration = g_DataTestResults.dwUpload_MSTransfer;
        dwTransferred = g_DataTestResults.dwUpload_BytesTransferred / 1024;
    }
    
    if (dwDuration > 0)
    {
        dwThroughput = (ULONGLONG)dwTransferred * 1000 / dwDuration;
        g_pLog->Log(LOG, L"%-10s%-18d%-15d%-16d", L"Upload", dwTransferred, dwDuration, dwThroughput);
    }
    else
    {
        g_pLog->Log(LOG, L"%-10s%-18d%-15dN/A", L"Upload", dwTransferred, dwDuration);
    }
    
    g_pLog->Log(LOG, L" ");

    extern DWORD g_dwSMSSendTimeMS;
    extern DWORD g_dwSMSSendCount;
    g_pLog->Log(LOG, L"SMS Performance Summary:");
    g_pLog->Log(LOG, L"Sent SMS Count   Total Sent Time(ms)   Average Sent Time(ms)");

    if (g_dwSMSSendCount > 0)
    {
        g_pLog->Log(LOG, L"%-17d%-22d%-21d", g_dwSMSSendCount, g_dwSMSSendTimeMS, 
            g_dwSMSSendTimeMS/g_dwSMSSendCount);
    }
    else
    {
        g_pLog->Log(LOG, L"%-17d%-22dN/A", g_dwSMSSendCount, g_dwSMSSendTimeMS);
    }
    g_pLog->Log(LOG, L" ");

    // calculate test score
    int iScore = CalculateTestScore();

    if (iScore >= 0)
    {
        g_pLog->Log(LOG, L"Test Score: %d%%", iScore);
    }
    else
    {
        g_pLog->Log(LOG, L"Test Score: N/A");
    }
    g_pLog->Log(LOG, L" ");
    return TRUE;
}

// With help from the country/region code (MCC) changes a local number into international format
VOID MangleOwnNumber(VOID)
{
    DWORD dwMCC=0;

    // MCC comprises the first three digitis of the IMSI
    if(g_TestData.tszIMSI[0])
    {
        TCHAR tszMCC[SERIAL_NUMBER_LENGTH + 1];
        _tcscpy_s(tszMCC, SERIAL_NUMBER_LENGTH + 1, g_TestData.tszIMSI);
        tszMCC[3] = 0;
        dwMCC=(DWORD)_wtoi(tszMCC);
        g_pLog->Log(LOG, TEXT("MCC = %d"), dwMCC);
    }

    // If the string begins with a '+' then we are cool
    if(L'+'!=g_TestData.tszNumber_OwnNumber[0])
    {
        switch(dwMCC)
        {
            // France - guess
            case 208:
                // If number starts with a zero, we have a national number
                if(L'0'==g_TestData.tszNumber_OwnNumber[0])
                {
                    _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, TEXT("+33"));
                    _tcscat_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, &g_TestData.tszNumber_OwnNumber[1]);
                }
                break;

            // United Kingdom
            case 234:
                // If number starts with a zero, we have a national number
                if(L'0'==g_TestData.tszNumber_OwnNumber[0])
                {
                    _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, TEXT("+44"));
                    _tcscat_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, &g_TestData.tszNumber_OwnNumber[1]);
                }
                break;

            // Germany - guess
            case 262:
                // If number starts with a zero, we have a national number
                if(L'0'==g_TestData.tszNumber_OwnNumber[0])
                {
                    _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, TEXT("+49"));
                    _tcscat_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, &g_TestData.tszNumber_OwnNumber[1]);
                }
                break;

            // USA
            case 310:
            case 311:
            case 316:
                if(10==wcslen(g_TestData.tszNumber_OwnNumber))
                {
                    // Dialup server doesn't work with international number, remove the "+" to work around this issue.
                    _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, TEXT("1"));
                    _tcscat_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, g_TestData.tszNumber_OwnNumber);
                }
                break;

            default:
                break;
        }
    }

    // If we couldn't mangle properly, just use the initial number
    if(!g_TestData.tszNumber_OwnNumberIntl[0])
        _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, g_TestData.tszNumber_OwnNumber);
}

BOOL DisableSIMLockingStatus (VOID)
{
    HSIM    hSim = 0;
    BOOL    fLocked = TRUE;

    if (g_TestData.fCDMA)
        return TRUE;

    if (SUCCEEDED( SimInitialize( SIM_INIT_NONE, NULL, NULL, &hSim ) )) {

        // Assuming the phone is turned on and unlocked
        if (SUCCEEDED( SimGetLockingStatus( hSim, SIM_LOCKFACILITY_SIM, g_TestData.tszSIM_PIN, &fLocked ) )) {

            if (fLocked) {

                g_pLog->Log( UI_LOG, _T("SIM lock is enabled, disabling it ...") );
                if (SUCCEEDED( SimSetLockingStatus( hSim, SIM_LOCKFACILITY_SIM, g_TestData.tszSIM_PIN, FALSE ) )) {
                    fLocked = FALSE;
                   fRestoreLock = TRUE;
                }
            }
        }
        else {
            g_pLog->Log( UI_LOG, _T("Can't query locking status.  Check pwd in init file") );
        }
        SimDeinitialize(hSim);
    }

    return !fLocked;
}


VOID RestoreSIMLockingStatus (VOID)
{
    HSIM    hSim = 0;
    BOOL    fUnlocked = FALSE;

    if (!fRestoreLock) {
        return;
    }

    if (SUCCEEDED( SimInitialize( SIM_INIT_NONE, NULL, NULL, &hSim ) )) {

        SimSetLockingStatus( hSim, SIM_LOCKFACILITY_SIM, g_TestData.tszSIM_PIN, TRUE );
        fRestoreLock = FALSE;
    }
    SimDeinitialize(hSim);

    return;

}

// initialization of Cellular test
BOOL TestInit()
{
    // SIM initialization
    if (g_pSIMTest)
    {
        g_pLog->Log(
            LOG,
            L"WARN: SIM test has been initialized."
            );
    }
    else
    {
        g_pSIMTest = new CSIMTest();

        if (! g_pSIMTest)
        {
            g_pLog->Log(
                LOG,
                L"ERROR: Memory is not enough for SIM test allocation."
                );
            return FALSE;
        }
        if (! g_pSIMTest->Init())
        {
            if (g_TestData.fCDMA)
            {
                g_pSIMTest->DeInit();
                delete g_pSIMTest;
                g_pSIMTest = NULL;
            }
            else
            {
                return FALSE;
            }
        }
    }

    // TAPI initialization
    if (! TSP_Init())
    {
        return FALSE;
    }

    // SMS initialization
    if (! SMS_Init())
    {
        return FALSE;
    }

    return TRUE;
}

// de-initialization of Cellular test
BOOL TestDeinit()
{
    BOOL fRet = TRUE;
    
    // SIM deinitialization
    if (g_pSIMTest)
    {
        if (!g_pSIMTest->DeInit())
        {
            fRet = FALSE;
        }
        delete g_pSIMTest;
        g_pSIMTest = NULL;
    }

    // TAPI deinitialization
    if (! TSP_DeInit())
    {
        fRet = FALSE;
    }

    // SMS deinitialization
    if (! SMS_DeInit())
    {
        fRet = FALSE;
    }

    RestoreDTPTConnection();
    return fRet;
}

// Entry point from the test worker task
BOOL TuxEntryPoint(VOID)
{
    BOOL  fNumberProvided = FALSE;
    DWORD dwLevel = 0;

    g_pLog->Log(
            LOG,
            L"INFO: Cellular test uses %s suites.",
            g_TestData.fCDMA ? L"CDMA" : L"GSM"
            );
    
    // Data from stinger.ini including dialup server numbers
    if(!ReadStaticTestData())
    {
        g_pLog->Log(UI_LOG, TEXT("Test aborted.  Make sure %s exists and is not write protected"), INI_FILENAME);
        return FALSE;
    }

    // Disable SIM lock if it's on
    if (!DisableSIMLockingStatus()) {
        g_pLog->Log( UI_LOG, _T("+++WARNING: SIM may be locked.  Test will proceed anyway.") );
    }

    // Make sure device uses line 1
    SetCurrentLine( NULL, 0 );

    // Now read the device information (do not override number provided in stinger.ini)
    fNumberProvided = (g_TestData.tszNumber_OwnNumber[0] != 0);
    if (fNumberProvided)
        TSP_RecoverOwnNumber(NULL, g_TestData.tszIMSI, g_TestData.tszSerialNumber );
    else
        TSP_RecoverOwnNumber(g_TestData.tszNumber_OwnNumber, g_TestData.tszIMSI, g_TestData.tszSerialNumber );

    g_pLog->Log(LOG, TEXT("---- SIM/DEVICE INFORMATION ----") );
    g_pLog->Log(LOG, TEXT("Device type = %s"), g_TestData.fCDMA  ? _T("CDMA") : _T("GSM/UMTS") );
    g_pLog->Log(LOG, TEXT("Suspend/Resume = %s"), g_TestData.fSuspendResume  ? _T("enabled") : _T("disabled") );
    g_pLog->Log(LOG, TEXT("Own Number = %s"), g_TestData.tszNumber_OwnNumber);
    g_pLog->Log(LOG, TEXT("IMSI = %s"), g_TestData.tszIMSI);
    g_pLog->Log(LOG, TEXT("Serial Number = %s"), g_TestData.tszSerialNumber);


    // Now mangle the number into international format, unless the number is provided in stinger.ini, in which case just use it
    if (!fNumberProvided)
        MangleOwnNumber();
    else
        _tcscpy_s(g_TestData.tszNumber_OwnNumberIntl, TELEPHONE_NUMBER_LENGTH, g_TestData.tszNumber_OwnNumber);

    g_pLog->Log(LOG, TEXT("---- INTERNATIONAL OWN NUMBER ----") );
    g_pLog->Log(LOG, TEXT("Own Number Intl = %s"), g_TestData.tszNumber_OwnNumberIntl);

    // Must have own number present to continue with testing
    if(!g_TestData.tszNumber_OwnNumber[0])
    {
        g_pLog->Log(LOG, TEXT("Test aborted") );
        g_pLog->Log(LOG, TEXT("MSISDN must be present within SIM, '-own_number <number>' or") );
        g_pLog->Log(LOG, TEXT("%s prior to test"), INI_FILENAME );
        return FALSE;
    }

    TerminateSMSApps();

    DeleteConnections();
    
    return TRUE;
}
