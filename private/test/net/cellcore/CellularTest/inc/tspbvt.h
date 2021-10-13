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

#define TAPI_API_LOW_VERSION 0x00020000
#define TAPI_API_HIGH_VERSION 0x00020000
#define EXT_API_LOW_VERSION 0x00010000
#define EXT_API_HIGH_VERSION 0x00010000

extern BOOL g_fUseDTMFDS;

struct TAPILineEventWorkerThreadProcData
{
    HLINEAPP hLineApp;
    HANDLE hTAPIEvent;
    WaitForTAPILineData* pwftldData;
    HANDLE hNewCallEvent;
    HCALL hIncomingCall;
    BOOL bIncomingCallValid;
    DWORD dwIncomingCallPrivilege;
    BOOL bCallDisconnected;
};

typedef enum
{
    CALL_BACK,
    STAY_ONLINE,
    FLASH,
    FLASH_DIAL,
}
DIALUP_SERVER_ACTION;

typedef enum
{
    CONNECT_VOICE,
    BUSY_VOICE,
    CONNECT_CSD,
    CONNECT_GPRS,
    CONNECT_VOICE_CONFERENCE1,
    CONNECT_VOICE_CONFERENCE2,
	CONNECT_VOICE_CONFERENCE3,
	CONNECT_VOICE_CONFERENCE4,
    CONNECT_VOICE_CONFERENCE5
}
DIAL_TYPE;

// BVT functions
BOOL RecoverTestData(LPTSTR lpCmdLine);
void Run_TSP_BVT(HLINE hLine, DWORD dwLineID, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
void Run_TSP_Incoming(HLINE hLine, DWORD dwLineID, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);

BOOL BVT_Dial(const HLINE hLine, HCALL &hCall, WaitForTAPILineData& rwftldData, DIAL_TYPE dialType);
BOOL BVT_DialHangup(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DialHoldDialConferenceHangup(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DialConferenceHeldHangup(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_SendDialupServerDTMF(HCALL hCall, DIALUP_SERVER_ACTION dsAction, WaitForTAPILineData& rwftldData, TCHAR* ptszCustomString = NULL);
BOOL BVT_AnswerIncomingVoice(HCALL &hCall, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
BOOL BVT_DialFlashDialIncoming (const HLINE hLine, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
BOOL BVT_DialBusy(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DialDTMFHangup_AnswerHangup(const HLINE hLine, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
BOOL BVT_DialStayOn(const HLINE hLine, WaitForTAPILineData &rwftldData, DWORD dwExpectedDuration, DWORD *dwActualDuration, HCALL *hCall);
BOOL BVT_WaitForIdle(HCALL &hCall, WaitForTAPILineData& rwftldData);
BOOL BVT_DropCall(HCALL hCall, WaitForTAPILineData& rwftldData, BOOL fVerifyDisconnectionCode = FALSE);
BOOL BVT_Unhold(const HCALL hCall, WaitForTAPILineData& rwftldData);
BOOL BVT_HoldUnhold(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DailDTMFHangup(const HLINE hLine, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
BOOL BVT_WaitIncomingCall(HCALL &hCall, TAPILineEventWorkerThreadProcData& rtlewtpdData, WaitForTAPILineData& rwftldData);
BOOL BVT_2WayCall(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DailNoAnswer(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_DailReject(const HLINE hLine, WaitForTAPILineData& rwftldData);
BOOL BVT_MultipleWayCall(const HLINE hLine, DWORD dwCallCounts, WaitForTAPILineData& rwftldData);

// Wrapped TAPI API calls
BOOL Test_lineInitializeEx(LPHLINEAPP lphLineApp, HINSTANCE hInstance,
                           LINECALLBACK lpfnCallback, LPCWSTR lpszFriendlyAppName,
                           LPDWORD lpdwNumDevs, LPDWORD lpdwAPIVersion,
                           LPLINEINITIALIZEEXPARAMS lpLineInitializeExParams);


BOOL Test_lineShutdown(HLINEAPP hLineApp);

BOOL Test_lineClose(HLINE hLine);

BOOL Test_lineOpen(HLINEAPP hLineApp, DWORD dwDeviceID, LPHLINE lphLine,
                   DWORD dwAPIVersion, DWORD dwExtVersion,
                   DWORD dwCallbackInstance, DWORD dwPrivileges,
                   DWORD dwMediaModes, LPLINECALLPARAMS const lpCallParams);

BOOL Test_lineGetID(HLINE hLine, DWORD dwAddressID, HCALL hCall,
                    DWORD dwSelect, LPVARSTRING lpDeviceID,
                    LPCWSTR lpszDeviceClass);

BOOL Test_lineSetStatusMessages(HLINE hLine, DWORD dwLineStates, DWORD dwAddressStates);

BOOL Test_lineGetAddressCaps(HLINEAPP hLineApp, DWORD dwDeviceID,
                             DWORD dwAddressID, DWORD dwAPIVersion, DWORD dwExtVersion,
                             LPLINEADDRESSCAPS lpAddressCaps);

BOOL Test_lineMakeCall(HLINE hLine, LPHCALL lphCall, LPCWSTR lpszDestAddress, DWORD dwCountryCode, LPLINECALLPARAMS const lpCallParams, LONG &lResult);

BOOL Test_lineGetCallStatus(HCALL hCall, LPLINECALLSTATUS lpCallStatus);

BOOL Test_lineGetCallInfo(HCALL hCall, LPLINECALLINFO lpCallInfo);

BOOL Test_lineDrop(HCALL hCall, LPCSTR lpsUserUserInfo, DWORD dwSize, LONG &lResult);

BOOL Test_lineDeallocateCall(HCALL hCall);

BOOL Test_lineAnswer(HCALL hCall, LPCSTR lpsUserUserInfo, DWORD dwSize, LONG &lResult);

BOOL Test_lineGenerateDigits(HCALL hCall, DWORD dwDigitMode, LPCWSTR lpszDigits, DWORD dwDuration);

BOOL Test_lineHold(HCALL hCall, LONG &lResult);

BOOL Test_lineUnhold(HCALL hCall, LONG &lResult);

BOOL Test_lineSwapHold(HCALL hActiveCall, HCALL hHeldCall, LONG &lResult);

BOOL Test_lineSetupConference(HCALL hCall, HLINE hLine, LPHCALL lphConfCall,
                              LPHCALL lphConsultCall, DWORD dwNumParties,
                              LPLINECALLPARAMS const lpCallParams, LONG &lResult);

BOOL Test_lineAddToConference(HCALL hConfCall, HCALL hConsultCall, LONG &lResult);

BOOL Test_lineRemoveFromConference(HCALL hCall, LONG &lResult);

BOOL Test_lineForward(HLINE hLine, DWORD bAllAddresses, DWORD dwAddressID,
                      LPLINEFORWARDLIST const lpForwardList, DWORD dwNumRingsNoAnswer,
                      LPHCALL lphConsultCall, LPLINECALLPARAMS const lpCallParams, LONG &lResult);

BOOL Test_lineGetAddressStatus(HLINE hLine, DWORD dwAddressID, LPLINEADDRESSSTATUS lpAddressStatus);

BOOL Test_lineGetLineDevStatus(HLINE hLine, LPLINEDEVSTATUS lpLineDevStatus);

BOOL Test_lineSetDevConfig(DWORD dwDeviceID, LPVOID const lpDeviceConfig, DWORD dwSize, LPCWSTR lpszDeviceClass);

BOOL Test_lineGetGeneralInfo(HLINE hLine, LPLINEGENERALINFO lpLineGeneralInfo);
