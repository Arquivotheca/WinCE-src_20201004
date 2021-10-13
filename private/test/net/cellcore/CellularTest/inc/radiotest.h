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

#ifndef _RadioTest_H
#define _RadioTest_H

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

// Mark code that requires attention while porting
#define PORTED  0
#define FAIL_LIMIT_THREHOLD 5

// Filenames
#define INI_FILENAME            TEXT("stinger.ini")

// A little dangerous to include this here but having everyone include that header isn't good at all
#include "tux.h"

#ifndef MAX_LOGBUFFSIZE
#define MAX_LOGBUFFSIZE 512
#endif

#ifndef COUNTOF
#define COUNTOF(ar) (sizeof(ar) / sizeof(ar[0]))
#endif

// permanent assert
VOID TAssert( BOOL fCondition, LPCSTR const pszExpression,
             LPCSTR const pszFileName, const INT iLineNumber );
// permanent assert
#define TASSERT( e ) TAssert( 0 != (e), #e, __FILE__, __LINE__ )
// First argument to Log() function
enum
{
    LOG = 0,    // Output to file/PB only
    UI,     // Output to editbox only
    UI_LOG, // Output to both
};
#define LOG_MASK_METRIC_FILE        0x80        // this mask can be OR'ed with the enum above to save the trace into the metric file
#define LOG_MASK_SUMMARY_FILE       0x40
#define STEP_NAME_LENGTH  50


#include <ril.h>
typedef HRESULT (*RIL_AT_REG_PROC)( HRIL hRil, BOOL fEnable );

/*
// gps function types
typedef BOOL (*GPS_INIT_PROC)(GPSNOTIFYCALLBACK callback, LPTSTR pszPortName, DWORD dwParam);
typedef BOOL (*GPS_DEINIT_PROC)(VOID);
*/

typedef struct _FAILEDTEST_STRUCT {

    LPCTSTR     ptszFailedTestName;
    DWORD       dwNumTimeFailed;

} FAILEDTEST, *PFAILEDTEST;


#define FAIL_LIST_MAX      99      // arbitrary, this ought to be big enough
//max depth of nested steps
#define MAX_STEPDEPTH  10
#define CT_INDENT      3

// We are interested only in the function that actually does the logging output
class CTestLogger
{
public:
    // Constructor
    CTestLogger(const LPTSTR ptszPrefix, const BOOL fLogEnable, const BOOL fEnableATHook);
    ~CTestLogger(void);

    // Our definition of the Log() does not inherit from the base class since the first argument is slightly different
    VOID Log( BYTE bOutputType, LPCTSTR ptszFormat, ... );

    // There are overloaded to log to the edit box
    virtual VOID BeginTest(const LPCTSTR ptszTestName);
    virtual BOOL EndTest();
    VOID BeginStep( const LPCTSTR ptszStepName, ... );
    BOOL EndStep( const BOOL fCondition );
    BOOL DoStep( const BOOL fCondition, const LPCTSTR ptszStepName );

    // Called in time of national emergency when the edit box is full
    // Flushes the last 10 lines back to the edit box

    VOID DisplayCache(VOID);
    VOID PrintFailList (void);

    friend VOID RadioMetrics_RIL_ResultCallback(
        HRIL        hRil,
        DWORD       dwCode,
        HRESULT     rilrCmdID,
        const void* lpData,
        DWORD       cbData,
        DWORD       dwParam
        )
    {
    }

    friend VOID RadioMetrics_RIL_NotifyCallback(
        HRIL        hRil, 
        DWORD       dwCode,
        const void* lpData,
        DWORD       cbData,
        DWORD       dwParam
        )
    {
        (( CTestLogger * )dwParam)->ProcessNotification( dwCode, lpData, cbData, dwParam );
    }

private:
    const static MAX_PREFIX_LENGTH = 16;

    DWORD       m_dwFailListCount;
    LPCTSTR     m_ptszCurrTestName;
    TCHAR       m_tszPrefix[MAX_PREFIX_LENGTH];
    PFAILEDTEST m_failList;

    HMODULE          m_hmodGPS;
    // GPS_INIT_PROC       m_pGPSInitProc;
    // GPS_DEINIT_PROC m_pGPSDeinitProc;

    //GPSRMCDATA          m_GPSRMCData;
    UINT64              m_uliLastGPSStatementTime;

    HRIL                    m_hRil;
    UINT m_uStepPass;
    UINT m_uStepFail;

    LPTSTR m_ptszStepName[MAX_STEPDEPTH];
    int m_iStepDepth;
    UINT m_uIndent;

    VOID ProcessNotification( const DWORD dwCode, const void *lpData,
        const DWORD cbData, const DWORD dwParam );
};

// ---- INI file parsing
#define SERIAL_NUMBER_LENGTH    50
#define TELEPHONE_NUMBER_LENGTH 100
#define APN_LENGTH              200
#define GPRS_USER_NAME_LENGTH   50
#define GPRS_PASSWORD_LENGTH    50
#define GPRS_DOMAIN_LENGTH  50
#define CSD_USER_NAME_LENGTH    50
#define CSD_PASSWORD_LENGTH     50
#define CSD_DOMAIN_LENGTH        50
#define PIN_LENGTH              10
#define URL_LENGTH              200
#define METRIC_NAME_LENGTH  50
#define DEVICE_INFO_LENGTH        200
#define NETWORK_NAME_LENGTH 50
#define MAXLENGTH_OPERATOR_NUMERIC     (16)

#define DEFAULT_DS_DELAY            0 //changed for time-being from 10
#define DEFAULT_DS_SLEEP        10
#define DEFAULT_DS_CALL_DURATION        30  // 30 secs
#define DEFAULT_BUSY_SLEEP      20
#define DEFAULT_SMS_SLEEP       15
#define DEFAULT_SMS_TIMEOUT  120
#define DEFAULT_NW_DELAY        15

#define DEFAULT_PREFERRED_OPERATOR  _T("310380")

#define DEFAULT_LONG_CALL_DURATION  (10 * 60 * 1000) // 10 minutes

#define DEFAULT_DHCP_SLEEP_MINS 240 // 4 hrs
#define DEFAULT_GPRS_SLEEP_MINS 2   // 2 minutes

#define DEFAULT_TAPI_TIMEOUT 45 // 45 seconds

#define MAX_GPRS_ACTIVATED_CONTEXTS 9   // 9 APNs at most

#define DEFAULT_SMS_COUNT 40
#define DEFAULT_STRESS_ITERATION 4
#define MAX_REMOTE_PARTIES_IN_CONFERENCE 5
#define MIN_REMOTE_PARTIES_IN_CONFERENCE 2

typedef struct
{
    TCHAR   tszMetric_Name[METRIC_NAME_LENGTH + 1];
    TCHAR   tszDevice_Name[DEVICE_INFO_LENGTH + 1];
    TCHAR   tszNumber_OwnNumber[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_OwnNumberIntl[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceAnswer[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceBusy[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceNoAnswer[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceConference1[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceConference2[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceConference3[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceConference4[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_VoiceConference5[TELEPHONE_NUMBER_LENGTH+1];
	TCHAR   tszNumber_CSDAnswer[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszNumber_CallForward[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszGPRS_APN[MAX_GPRS_ACTIVATED_CONTEXTS][APN_LENGTH+1];
    TCHAR   tszGPRS_USER_NAME[MAX_GPRS_ACTIVATED_CONTEXTS][GPRS_USER_NAME_LENGTH+1];
    TCHAR   tszGPRS_PASSWORD[MAX_GPRS_ACTIVATED_CONTEXTS][GPRS_PASSWORD_LENGTH+1];
    TCHAR   tszGPRS_DOMAIN[MAX_GPRS_ACTIVATED_CONTEXTS][GPRS_DOMAIN_LENGTH+1];
    TCHAR   tszCSD_NUMBER[TELEPHONE_NUMBER_LENGTH+1];
    TCHAR   tszCSD_USER_NAME[CSD_USER_NAME_LENGTH +1];
    TCHAR   tszCSD_PASSWORD[CSD_PASSWORD_LENGTH+1];
    TCHAR   tszCSD_DOMAIN[CSD_DOMAIN_LENGTH+1];
    TCHAR   tszIMSI[SERIAL_NUMBER_LENGTH+1];
    TCHAR   tszSerialNumber[SERIAL_NUMBER_LENGTH+1];
    TCHAR   tszSIM_PIN[PIN_LENGTH+1];
    TCHAR   tszDownloadURL[URL_LENGTH+1];
    TCHAR   tszUploadURL[URL_LENGTH+1];
    TCHAR   tszNetwork_Name[NETWORK_NAME_LENGTH+1];
    TCHAR   tszPreferredOperator[MAXLENGTH_OPERATOR_NUMERIC+1];
    DWORD   dwBlocksPerSession;
    DWORD   dwBlockSize;
    DWORD   dwTotalGPRSBytesExpected;
    DWORD   dwTotalCSDBytesExpected;
    DWORD   dwDsSleep;
    DWORD   dwDsDelay;
    DWORD   dwDsCallDuration;
    DWORD   dwLongCallDuration;
    DWORD   dwBusySleep;
    DWORD   dwSMSSleep;
    DWORD   dwSMSTimeout;
    DWORD   dwDurationHrs;
    DWORD   dwConsecutiveFailsAllowed;
    DWORD   dwConsecutiveFailsOccurred;
    UINT64  uliTestEndTime;
    BOOL    fComputeMetric;
    BOOL    fWeightedRandomTests;
    BOOL    fCDMA;
    BOOL    fSuspendResume;
    BOOL    fCallBusy;
    DWORD   dwTestDelaySeconds;
    DWORD   dwNWDelay;
    DWORD   dwTAPITimeout;
    DWORD   dwCallDurationMinutes;
    DWORD   dwDHCPSleepMinutes;
    DWORD   dwGPRSSleepMinutes;
    DWORD   dwGPRSAPNCount;
    DWORD   dwMAPNSize; 
    DWORD   dwUploadSize;
    DWORD   dwStressIteration;
    DWORD   dwRemotePartyCountInConf;
    TCHAR   tszGPRSHTTPProxy[MAX_PATH + 1];
    TCHAR   tszCSDHTTPProxy[MAX_PATH + 1];
}TEST_DATA_T;


struct CASE_STATUS
{
    DWORD   dwPass;
    DWORD   dwFail;
    DWORD   dwPriority;
    LPCTSTR pszCaseName;
};

struct TEST_STATUS
{
    // Voice
    CASE_STATUS Voice_OutNormal;
    CASE_STATUS Voice_OutBusy;
    CASE_STATUS Voice_OutNoAnswer;
    CASE_STATUS Voice_OutReject;
    CASE_STATUS Voice_InNormal;
    CASE_STATUS Voice_InNoAnswer;
    CASE_STATUS Voice_InReject;
    CASE_STATUS Voice_LongCall;
    CASE_STATUS Voice_HoldUnhold;
    CASE_STATUS Voice_Conference;
    CASE_STATUS Voice_HoldConference;
    CASE_STATUS Voice_2WayCall;
    CASE_STATUS Voice_MultipleWayCall;
	CASE_STATUS Voice_Flash;

    // Data
    CASE_STATUS Data_Download;
    CASE_STATUS Data_Upload;
    CASE_STATUS Data_GPRSSequentialAPNs;
    CASE_STATUS Data_GPRSMultipleAPNs;

    // SIM
    CASE_STATUS SIM_PhoneBookRead;
    CASE_STATUS SIM_PhoneBook;
    CASE_STATUS SIM_SMS;
    
    // SMS
    CASE_STATUS SMS_SendRecv;
    CASE_STATUS SMS_LongMessage;

    // Complex
    CASE_STATUS Complex_Voice_Data;
    CASE_STATUS Complex_Voice_SMS;
    
    // Misc
    CASE_STATUS RadioOnOff;
};

struct DATA_TEST_RESULTS
{
    DWORD dwGPRS_BytesTransferred;
    DWORD dwGPRS_MSTransfer;
    DWORD dwGPRS_SuccessConnections;
    DWORD dwGPRS_FailedConnections;
    DWORD dwGPRS_FailedURLConnections;
    DWORD dwGPRS_Timeouts;
    DWORD dwGPRS_TransferToError;
    DWORD dwCSD_BytesTransferred;
    DWORD dwCSD_MSTransfer;
    DWORD dwCSD_SuccessConnections;
    DWORD dwCSD_FailedConnections;
    DWORD dwCSD_FailedURLConnections;
    DWORD dwCSD_Timeouts;
    DWORD dwCSD_TransferToError;
    DWORD dwFailedCalls;
    DWORD dwUpload_BytesTransferred;
    DWORD dwUpload_MSTransfer;
	DWORD dwDataConnectionSetupCount;
    DWORD dwDataConnectionSetupTime;
};

// Externs needed by almost every file
extern CTestLogger      *g_pLog;
extern TEST_DATA_T      g_TestData;
extern TCHAR            g_tszConnName[];
extern TEST_STATUS      g_TestStatus;

#define CDMA_UNSUPPORTED()      if (g_TestData.fCDMA) { g_pLog->Log( LOG, _T("Test not supported in CDMA")); return TRUE;}

// call back of test function
typedef BOOL (* CBTestFunc)(DWORD);

// structure of test case running process
struct struCaseProcess
{
    LPCTSTR     wszName;    // test case name
    CBTestFunc  pInit;      // test initialization function pointer
    CBTestFunc  pExec;      // test execution function pointer
    CBTestFunc  pDeInit;    // test deinitialization function pointer
};
typedef struct struCaseProcess CaseProcess;

struct struSuiteInfo
{
    const CaseProcess *   pList;  // case list
    DWORD  baseID; // base ID of suite
    DWORD  count;  // count of case list
};
typedef struct struSuiteInfo SuiteInfo;

const DWORD COUNT_OF_SUITE = 10;
const DWORD STEP_OF_CASEID = 100;
const DWORD STEP_OF_ITERATIONID = COUNT_OF_SUITE * STEP_OF_CASEID;

TESTPROCAPI TestExecutor(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

const DWORD MAX_RADIONUM = 2; // 2 is maximum number for current device

void SetTestStatus(CASE_STATUS *pCS, BOOL fOK, LPCTSTR pCaseName, DWORD dwTestPriority);

extern void LogToViewer(LPCTSTR szFormat, ...);
extern void LogToViewerTitle(LPCTSTR szFormat, ...);
extern BOOL InitViewer();
extern void CloseViewer();

enum RADIO_TYPE
{
    RADIO_UNKNOWN,
    RADIO_FAKERIL,
    RADIO_ENFORA,
    // Add other radio here
};

BOOL IsRadio(RADIO_TYPE radioType);
#endif
