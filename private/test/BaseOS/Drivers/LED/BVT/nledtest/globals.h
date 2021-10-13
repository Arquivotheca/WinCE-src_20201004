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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------

#pragma once

#include "tuxmain.h"
#include "debug.h"
#include "pm.h"
#include "pkfuncs.h"
#include "time.h"
#include "pwrtstutil.h"

#define NO_SETTINGS_ADJUSTABLE         0
#define All_BLINK_TIMES_ADJUSTABLE     1
#define TOTAL_CYCLE_TIME_ADJUSTABLE    2
#define ON_TIME_ADJUSTABLE             3
#define OFF_TIME_ADJUSTABLE            4
#define META_CYCLE_ON_ADJUSTABLE       5
#define META_CYCLE_OFF_ADJUSTABLE      6
#define LED_OFF                        0
#define LED_ON                         1
#define LED_BLINK                      2
#define META_CYCLE_ON                  10
#define META_CYCLE_OFF                 10
#define LED_INDEX_FIRST                0
#define INVALID_VALUE                  -5
#define VALID_SWBLINKCTRL              1
#define ZERO_SWBLINKCTRL               0
#define VALID_BLINKABLE                1
#define ZERO_BLINKABLE                 0
#define INVALID_KEY_SIZE               260
#define ONE_LED                        1
#define LED_TYPE_UNKNOWN               0
#define LED_TYPE_NLED                  1
#define LED_TYPE_VIBRATOR              2
#define LED_TYPE_INVALID               3
#define ADJUST_TOTAL_CYCLE_TIME        0
#define ADJUST_ON_TIME                 1
#define ADJUST_OFF_TIME                2
#define ADJUST_META_CYCLE_ON           3 
#define ADJUST_META_CYCLE_OFF          4
#define MAX_THREAD_COUNT               30
#define DEFAULT_MIN_NUM_OF_NLEDS       0
#define DEFAULT_MAX_MEMORY_UTILIZATION 1
#define DEFAULT_MAX_CPU_UTILIZATION    1
#define DEFAULT_MAX_TIME_DELAY         2000
#define SLEEP_FOR_FIVE_SECONDS         5000
#define SLEEP_FOR_ONE_SECOND           1000
#define NO_NLEDS_PRESENT               0
#define NO_MATCHING_NLED               -1
#define INVALID_REG_KEY_VALUE          -1
#define TIME_BEFORE_RESUME             30
#define TRANSITION_CHANGE_MAX          5
#define CYCLETIME_CHANGE_VALUE         5
#define CALCULATE_CPU_UTIL_THREE_TIMES 3
#define LEAST_OPERATOR_VALUE           0
#define MAX_OPERATOR_VALUE             4
#define VALID_METACYCLE_VALUE          1
#define VALID_ADJUST_TYPE_VALUE        1
#define VALID_CYCLEADJUST_VALUE        1
#define MEASURE_CYCLETIME_THREE_TIMES  3
#define MAX_SIZE                       10000
#define MAX_WAIT_TIME                  3000
#define SERVICE__EVENT_BASE_ROOT_VALUE 0x80000000
#define REG_NLED_KEY_PATH              (L"Drivers\\BuiltIn\\Nled")
#define REG_NLED_KEY_CONFIG_PATH       (L"Drivers\\BuiltIn\\Nled\\Config")
#define NLED_GUID_VALUE                TEXT("CBB4F234-F35F-485b-A490-ADC7804A4EF3")
#define TEMPO_EVENT_PATH               TEXT("Drivers\\BuiltIn\\NLED\\TempoEvent")
#define TEMPORARY_EVENT_PATH           TEXT("Drivers\\BuiltIn\\NLed\\TemporaryEvent")
#define INVALID_EVENT_PATH             TEXT("Drivers\\BuiltIn\\NLED\\InvalidLEDServiceEvent")
#define countof(a)                     (sizeof(a)/sizeof(*(a)))
#define MAX_THREAD_TIMEOUT_VAL         30000

extern BOOL g_fInteractive;
extern BOOL g_fVibrator;
extern UINT g_nMinNumOfNLEDs;
extern BOOL g_fGeneralTests;
extern BOOL g_fPQDTests;
extern BOOL g_fAutomaticTests;
extern BOOL g_fServiceTests;
extern UINT g_nMaxTimeDelay;
extern UINT g_nMaxMemoryUtilization;
extern UINT g_nMaxCpuUtilization;
extern CKato *g_pKato;
extern BOOL g_fMULTIPLENLEDSETFAILED;
extern UINT g_nThreadCount;
extern HANDLE g_hNledDeviceHandle;
extern HANDLE g_hThreadHandle[MAX_THREAD_COUNT];
extern TCHAR g_szInputMsg[100];

enum ADJUSTMENT_TYPE { PROMPT_ONLY, SHORTEN, LENGTHEN };
enum SETTING_INDICATOR { METACYCLEON, METACYCLEOFF };

typedef struct {
    DWORD dwLedNum;
    DWORD dwEventRoot;
    BYTE* bpPath;
    BYTE* bpValueName;
    BYTE* bpEventName;
    DWORD dwMask;
    DWORD dwValue;
    DWORD dwOperator;
    DWORD dwPrio;
    DWORD dwState;
    DWORD dwTotalCycleTime;
    DWORD dwOnTime;
    DWORD dwOffTime;
    DWORD dwMetaCycleOn;
    DWORD dwMetaCycleOff;
}NLED_EVENT_DETAILS;

typedef struct
{
    LONG AdjustType;
    DWORD Blinkable;
    LONG CycleAdjust;
    LONG LedGroup;
    DWORD LedNum;
    DWORD LedType;
    LONG MetaCycle;
    LONG SWBlinkCtrl;
}NLED_LED_DETAILS;

// Helper function prototypes
BOOL SetLEDOffOnBlink( UINT unLedNum, int iOffOnBlink, BOOL bInteractive );
BOOL TestLEDBlinkTimeSettings( DWORD dwSetting,
                               UINT  LedNum,
                               BOOL  Interactive );
BOOL  ValidateLedSettingsInfo( NLED_SETTINGS_INFO ledSettingsInfo,
                               NLED_SETTINGS_INFO ledSettingsInfo_Valid,
                               NLED_SUPPORTS_INFO ledSupportsInfo);
DWORD ValidateMetaCycleSettings( NLED_SETTINGS_INFO ledSettingsInfo,
                                 SETTING_INDICATOR  uSettingIndicator,
                                 UINT               uAdjusmentType,
                                 BOOL               bInteractive,
                                 NLED_SUPPORTS_INFO ledSupportsInfo);
DWORD VerifyNLEDSupportsOption( BOOL bOption, LPCTSTR szPrompt );
BOOL setAllLedSettings( NLED_SETTINGS_INFO *pLedSettingsInfo );
BOOL saveAllLedSettings( NLED_SETTINGS_INFO *pLedSettingsInfo, int OffOnBlink );
DWORD GetCpuUtilization();
DWORD BlinkNled( UINT nLedNum );
UINT GetLedCount();
INT GetBlinkingNledNum();
DWORD SuspendResumeDevice( UINT nLedNum, INT nOffOnBlink );
DWORD GetSWBlinkCtrlLibraryNLed( DWORD );
DWORD RestoreOriginalNledSettings( NLED_SETTINGS_INFO *originalLedSettingsInfo );
DWORD GetUserResponse( NLED_SUPPORTS_INFO );

// Driver tests private function declarations
DWORD TestNledWithInvalidLedName( HKEY *hLedNameKey );
DWORD TestNLEDSetGetDeviceInfo( DWORD );
DWORD TestNledWithInvalidLedType( HKEY *hLedNameKey, DWORD );
DWORD InvalidBlinkableValidCycleAdjustTest( HKEY *hLedNameKey, DWORD );
DWORD TestNledWithInvalidBlinkableKey( HKEY *hLedNameKey, DWORD );
DWORD InvalidBlinkableValidAdjustTypeTest( HKEY *hLedNameKey, DWORD );
DWORD TestNledWithInvalidMetaCycle( HKEY *hLedNameKey, DWORD );
DWORD InvalidBlinkableValidMetaCycleTest( HKEY *hLedNameKey, DWORD );
DWORD TestNledWithInvalidAdjustType( HKEY *hLedNameKey, DWORD );
DWORD CheckSystemBehavior( UINT nLedNum, UINT nLeds );
BOOL WINAPI ThreadProc( NLED_SETTINGS_INFO *ledSettingsInfo_Set );
DWORD GetNledGroupIds( DWORD *g_pLedNumGroupId );
INT GetNonBlinkingNledNum();
DWORD GetSWBlinkCtrlNleds( DWORD *pSWBlinkCtrlNledNums );
void SetNledThread( NLED_SETTINGS_INFO *ledSettingsInfo_Set );
BOOL CallNledDeviceDriverIOCTL( void );
BOOL LoadUnloadTestFunc();
BOOL DeleteLEDRegistry( HKEY * );
BOOL ReadLEDDetails( NLED_LED_DETAILS *, HKEY* );
BOOL RestoreLEDDetails( NLED_LED_DETAILS *, HKEY* );
BOOL CorruptLEDDetails( HKEY * );

// Nled Service helper functions declarations
BOOL RaiseTemporaryEvent( HKEY hLEDKey, 
                          HKEY hEventSubKey,
                          NLED_SETTINGS_INFO *pLedSettingsInfoAfterEvent );
BOOL GetStringFromReg( HKEY hKey, LPCWSTR pszName, BYTE **ppszValue );
BOOL GetDwordFromReg( HKEY hKey, LPCWSTR pszName, DWORD *pValue );
BOOL GetStringFromReg_FreeMem( BYTE **ppszValue );
BOOL UnloadLoadService();
DWORD BlinkLED( /* REMOVED-> HKEY hNLEDConfigKey, */
                HKEY hLEDKey, 
                TCHAR *szLedName,
                HKEY hEventKey,
                TCHAR *szEventName );

DWORD BlinkLEDWithDefaultParams(/* REMOVED->  HKEY hNLEDConfigKey, */
                                 HKEY hLEDKey, 
                                 TCHAR *szLedName,
                                 HKEY hEventKey,
                                 TCHAR *szEventName );
DWORD VerifyBlinkSettings( /* REMOVED-> HKEY hNLEDConfigKey,*/
                           HKEY hLEDKey, 
                           TCHAR *szLedName,
                           HKEY hEventKey,
                           TCHAR *szEventName );

BOOL ReadEventDetails( HKEY hLEDKey, 
                       HKEY hEventSubKey,
                       NLED_EVENT_DETAILS *nledEventDetailsStruct );
BOOL Create_Set_TemporaryTrigger( HKEY hEventSubKey, 
                                  WCHAR *strTempoEventPath,
                                  NLED_EVENT_DETAILS *pnledEventDetailsStruct, 
                                  HKEY *phTempoEventKey );
BOOL RaiseTemporaryTrigger( HKEY hTempoEventKey,
                            NLED_EVENT_DETAILS *pnledEventDetailsStruct );
BOOL GetCurrentNLEDDriverSettings( NLED_EVENT_DETAILS pnledEventDetailsStruct,
                                   NLED_SETTINGS_INFO *pLedSettingsInfo );
BOOL Unset_TemporaryTrigger( HKEY hEventSubKey, 
                             HKEY hTempoEventKey,
                             NLED_EVENT_DETAILS *pnledEventDetailsStruct );
BOOL DeleteTemporaryTrigger( HKEY hTempoEventKey, 
                             WCHAR *strTempoEventPath,
                             NLED_SETTINGS_INFO *pLedSettingsInfo,
                             NLED_EVENT_DETAILS *pnledEventDetailsStruct );
BOOL RestoreAllNledServiceEventPaths( NLED_EVENT_DETAILS *, HKEY *, DWORD );
BOOL UnSetAllNledServiceEvents( NLED_EVENT_DETAILS *,  /* REMOVED-> DWORD,*/ HKEY *, HKEY * );
DWORD NledServiceInvalidEventPath( VOID );
DWORD NledServiceDeleteEventPath( VOID );
DWORD NledServiceInvalidOnOffTime( VOID );
DWORD NledServiceInvalidOperatorValue( VOID );
DWORD NledServiceInvalidTotalCycleTime( VOID );
DWORD NledServiceInvalidLEDNum( VOID );
DWORD NledServiceDeleteLEDRegistry( VOID );
DWORD NledServiceCorruptLEDRegistry( VOID );
DWORD GetNledEventIds( DWORD *pLedNumEventId );
BOOL IsServiceRunning( LPWSTR lpszDllServiceName );
BOOL fireEventsForRobustnessTests();
BOOL ResetEventsForRobustnessTests();
DWORD CheckThreadCompletion(HANDLE threadHandle[MAX_THREAD_COUNT], INT nThreadCount, INT nTimeOutVal);