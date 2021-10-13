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
#include <tapi.h>

LONG xxx_lineInitializeEx(
    LPHLINEAPP lphLineApp,
    HINSTANCE hInstance,
    LINECALLBACK lpfnCallback,
    LPCWSTR lpszFriendlyAppName,
    LPDWORD lpdwNumDevs,
    LPDWORD lpdwAPIVersion,
    LPLINEINITIALIZEEXPARAMS lpLineInitializeExParams
    )
{
    return TAPIlineInitializeEx(lphLineApp,
                                hInstance,
                                lpfnCallback,
                                lpszFriendlyAppName,
                                lpdwNumDevs,
                                lpdwAPIVersion,
                                lpLineInitializeExParams);
}

LONG xxx_lineClose (HLINE hLine)
{
    return lineClose (hLine);
}

LONG xxx_lineDeallocateCall (HCALL hCall)
{
    return lineDeallocateCall (hCall);
}

LONG xxx_lineDrop (HCALL hCall, LPCTSTR lpsUserUserInfo, DWORD dwSize)
{
    return lineDrop (hCall, lpsUserUserInfo, dwSize);
}

LONG xxx_lineGetDevCaps (HLINEAPP hLineApp, DWORD dwDeviceID, 
                         DWORD dwAPIVersion, DWORD dwExtVersion, 
                         LPLINEDEVCAPS lpLineDevCaps)
{
    return lineGetDevCaps (hLineApp, dwDeviceID, 
                           dwAPIVersion, dwExtVersion, 
                           lpLineDevCaps);
}

LONG xxx_lineGetDevConfig (DWORD dwDeviceID, LPVARSTRING lpDeviceConfig, 
                           LPCTSTR lpszDeviceClass)
{
    return lineGetDevConfig (dwDeviceID, lpDeviceConfig, 
                             lpszDeviceClass);
}

LONG xxx_lineGetTranslateCaps (HLINEAPP hLineApp, DWORD dwAPIVersion, 
                               LPLINETRANSLATECAPS lpTranslateCaps)
{
    return lineGetTranslateCaps (hLineApp, dwAPIVersion, 
                                 lpTranslateCaps);
}

LONG xxx_lineMakeCall (HLINE hLine, LPHCALL lphCall, LPCTSTR lpszDestAddress, 
                       DWORD dwCountryCode,
                       LPLINECALLPARAMS const lpCallParams)
{
    return lineMakeCall (hLine, lphCall, lpszDestAddress, 
                         dwCountryCode, lpCallParams);
}

LONG xxx_lineNegotiateAPIVersion (HLINEAPP hLineApp, DWORD dwDeviceID, 
                                  DWORD dwAPILowVersion,
                                  DWORD dwAPIHighVersion, 
                                  LPDWORD lpdwAPIVersion,
                                  LPLINEEXTENSIONID lpExtensionID)
{
    return lineNegotiateAPIVersion (hLineApp, dwDeviceID, 
                                    dwAPILowVersion, dwAPIHighVersion, 
                                    lpdwAPIVersion, lpExtensionID);
}

LONG xxx_lineOpen (HLINEAPP hLineApp, DWORD dwDeviceID, LPHLINE lphLine,
                   DWORD dwAPIVersion, DWORD dwExtVersion, 
                   DWORD dwCallbackInstance, DWORD dwPrivileges, 
                   DWORD dwMediaModes, LPLINECALLPARAMS const lpCallParams)
{
    return lineOpen (hLineApp, dwDeviceID, lphLine,
                     dwAPIVersion, dwExtVersion, 
                     dwCallbackInstance, dwPrivileges, 
                     dwMediaModes, lpCallParams);
}

LONG xxx_lineSetDevConfig (DWORD dwDeviceID, LPVOID const lpDeviceConfig, 
                           DWORD dwSize, LPCTSTR lpszDeviceClass)
{
    return lineSetDevConfig (dwDeviceID, lpDeviceConfig, 
                             dwSize, lpszDeviceClass);
}

LONG xxx_lineSetStatusMessages (HLINE hLine, DWORD dwLineStates, 
                                DWORD dwAddressStates)
{
    return lineSetStatusMessages (hLine, dwLineStates, 
                                  dwAddressStates);
}

LONG xxx_lineTranslateAddress (HLINEAPP hLineApp, DWORD dwDeviceID, 
                               DWORD dwAPIVersion, LPCTSTR lpszAddressIn, 
                               DWORD dwCard, DWORD dwTranslateOptions,
                               LPLINETRANSLATEOUTPUT lpTranslateOutput)
{
    return lineTranslateAddress (hLineApp, dwDeviceID, 
                                 dwAPIVersion, lpszAddressIn, 
                                 dwCard, dwTranslateOptions,
                                 lpTranslateOutput);
}

LONG xxx_lineTranslateDialog (HLINEAPP hLineApp, DWORD dwDeviceID, 
                              DWORD dwAPIVersion, HWND hwndOwner,
                              LPCTSTR lpszAddressIn)
{
    return lineTranslateDialog (hLineApp, dwDeviceID, 
                                dwAPIVersion, hwndOwner, lpszAddressIn);
}

LONG xxx_lineConfigDialogEdit (DWORD dwDeviceID, HWND hwndOwner, 
                               LPCTSTR lpszDeviceClass,
                               LPVOID const lpDeviceConfigIn, 
                               DWORD dwSize, LPVARSTRING lpDeviceConfigOut)
{
    return lineConfigDialogEdit (dwDeviceID, hwndOwner, 
                                 lpszDeviceClass, lpDeviceConfigIn, 
                                 dwSize, lpDeviceConfigOut);
}

LONG xxx_lineGetID (HLINE hLine, DWORD dwAddressID, HCALL hCall,   
                    DWORD dwSelect, LPVARSTRING lpDeviceID,    
                    LPCTSTR lpszDeviceClass)
{
    return lineGetID (hLine, dwAddressID, hCall,   
                      dwSelect, lpDeviceID,    
                      lpszDeviceClass);
}

LONG xxx_lineAddProvider(LPCTSTR lpszProviderFilename,
                         HWND hwndOwner,
                         LPDWORD lpdwPermanentProviderID)
{
    return lineAddProvider(lpszProviderFilename, hwndOwner,
                           lpdwPermanentProviderID);
}

LONG xxx_lineSetCurrentLocation(HLINEAPP hLineApp,
                                DWORD dwLocation)
{
    return lineSetCurrentLocation(hLineApp, dwLocation);
}


//
// New for Cedar
//

LONG xxx_lineAccept(HCALL hCall, LPCSTR lpsUserUserInfo, DWORD dwSize)
{
    return lineAccept(hCall, lpsUserUserInfo, dwSize);
}

LONG xxx_lineAddToConference(HCALL hConfCall, HCALL hConsultCall)
{
    return lineAddToConference(hConfCall, hConsultCall);
}

LONG xxx_lineAnswer(HCALL hCall, LPCSTR lpsUserUserInfo, DWORD dwSize)
{
    return lineAnswer(hCall, lpsUserUserInfo, dwSize);
}

LONG xxx_lineBlindTransfer(HCALL hCall, LPCWSTR lpszDestAddressW,
               DWORD dwCountryCode)
{
    return lineBlindTransfer(hCall, lpszDestAddressW, dwCountryCode);
}

LONG xxx_lineCompleteTransfer(HCALL hCall, HCALL hConsultCall,
               LPHCALL lphConfCall, DWORD dwTransferMode)
{
    return lineCompleteTransfer(hCall, hConsultCall, lphConfCall, dwTransferMode);
}

LONG xxx_lineDevSpecific(HLINE hLine, DWORD dwAddressID, HCALL hCall,
               LPVOID lpParams, DWORD dwSize)
{
    return lineDevSpecific(hLine, dwAddressID, hCall, lpParams, dwSize);
}

LONG xxx_lineDial(HCALL hCall, LPCWSTR lpszDestAddress, DWORD dwCountryCode)
{
    return lineDial(hCall, lpszDestAddress, dwCountryCode);
}

LONG xxx_lineForward(HLINE hLine, DWORD bAllAddresses, DWORD dwAddressID,
               LPLINEFORWARDLIST const lpForwardList, DWORD dwNumRingsNoAnswer,
               LPHCALL lphConsultCall, LPLINECALLPARAMS const lpCallParams)
{
    return lineForward(hLine, bAllAddresses, dwAddressID,
               lpForwardList, dwNumRingsNoAnswer,
               lphConsultCall, lpCallParams);
}

LONG xxx_lineGenerateDigits(HCALL hCall, DWORD dwDigitMode,
               LPCWSTR lpszDigits, DWORD dwDuration)
{
    return lineGenerateDigits(hCall, dwDigitMode, lpszDigits, dwDuration);
}

LONG xxx_lineGenerateTone(HCALL hCall, DWORD dwToneMode, DWORD dwDuration,
               DWORD dwNumTones, LPLINEGENERATETONE const lpTones)
{
    return lineGenerateTone(hCall, dwToneMode, dwDuration, dwNumTones, lpTones);
}

LONG xxx_lineStartDTMF(HCALL hCall, CHAR cDTMFDigit)
{
    return lineStartDTMF(hCall, cDTMFDigit);
}

LONG xxx_lineStopDTMF(HCALL hCall)
{
    return lineStopDTMF(hCall);
}

LONG xxx_lineGetAddressCaps(HLINEAPP hLineApp, DWORD dwDeviceID,
               DWORD dwAddressID, DWORD dwAPIVersion, DWORD dwExtVersion,
               LPLINEADDRESSCAPS lpAddressCaps)
{
    return lineGetAddressCaps(hLineApp, dwDeviceID,
               dwAddressID, dwAPIVersion, dwExtVersion, lpAddressCaps);
}

LONG xxx_lineGetAddressID(HLINE hLine, LPDWORD lpdwAddressID,
               DWORD dwAddressMode, LPCWSTR lpsAddress, DWORD dwSize)
{
    return lineGetAddressID(hLine, lpdwAddressID, dwAddressMode, lpsAddress, dwSize);
}

LONG xxx_lineGetAddressStatus(HLINE hLine, DWORD dwAddressID,
               LPLINEADDRESSSTATUS lpAddressStatus)
{
    return lineGetAddressStatus(hLine, dwAddressID, lpAddressStatus);
}


LONG xxx_lineGetAppPriority(LPCWSTR lpszAppFilename, DWORD dwMediaMode,
               LPLINEEXTENSIONID lpExtensionID, DWORD dwRequestMode,
               LPVARSTRING lpExtensionName, LPDWORD lpdwPriority)
{
    return lineGetAppPriority(lpszAppFilename, dwMediaMode,
               lpExtensionID, dwRequestMode,
               lpExtensionName, lpdwPriority);
}

LONG xxx_lineGetCallInfo(HCALL hCall, LPLINECALLINFO lpCallInfo)
{
    return lineGetCallInfo(hCall, lpCallInfo);
}

LONG xxx_lineGetCallStatus(HCALL hCall, LPLINECALLSTATUS lpCallStatus)
{
    return lineGetCallStatus(hCall, lpCallStatus);
}

LONG xxx_lineGetConfRelatedCalls(HCALL hCall, LPLINECALLLIST lpCallList)
{
    return lineGetConfRelatedCalls(hCall, lpCallList);
}
 
LONG xxx_lineGetIcon(DWORD dwDeviceID, LPCWSTR lpszDeviceClass,
               LPHICON lphIcon)
{
    return lineGetIcon(dwDeviceID, lpszDeviceClass, lphIcon);
}

LONG xxx_lineGetLineDevStatus(HLINE hLine, LPLINEDEVSTATUS lpLineDevStatus)
{
    return lineGetLineDevStatus(hLine, lpLineDevStatus);
}

LONG xxx_lineGetMessage(HLINEAPP hLineApp, LPLINEMESSAGE lpMessage,
               DWORD dwTimeout)
{
    return lineGetMessage(hLineApp, lpMessage, dwTimeout);
}

    
LONG xxx_lineGetNewCalls(HLINE hLine, DWORD dwAddressID, DWORD dwSelect,
               LPLINECALLLIST lpCallList)
{
    return lineGetNewCalls(hLine, dwAddressID, dwSelect, lpCallList);
}

LONG xxx_lineGetNumRings(HLINE hLine, DWORD dwAddressID,
               LPDWORD lpdwNumRings)
{
    return lineGetNumRings(hLine, dwAddressID, lpdwNumRings);
}

LONG xxx_lineGetProviderList(DWORD dwAPIVersion,
               LPLINEPROVIDERLIST lpProviderList)
{
    return lineGetProviderList(dwAPIVersion, lpProviderList);
}

LONG xxx_lineGetStatusMessages(HLINE hLine, LPDWORD lpdwLineStates,
               LPDWORD lpdwAddressStates)
{
    return lineGetStatusMessages(hLine, lpdwLineStates, lpdwAddressStates);
}

LONG xxx_lineHandoff(HCALL hCall, LPCWSTR lpszFileName, DWORD dwMediaMode)
{
    return lineHandoff(hCall, lpszFileName, dwMediaMode);
}

LONG xxx_lineHold(HCALL hCall)
{
    return lineHold(hCall);
}

LONG xxx_lineMonitorDigits(HCALL hCall, DWORD dwDigitModes)
{
    return lineMonitorDigits(hCall, dwDigitModes);
}

LONG xxx_lineMonitorMedia(HCALL hCall, DWORD dwMediaModes)
{
    return lineMonitorMedia(hCall, dwMediaModes);
}

LONG xxx_lineNegotiateExtVersion(HLINEAPP hLineApp, DWORD dwDeviceID,
               DWORD dwAPIVersion, DWORD dwExtLowVersion,
               DWORD dwExtHighVersion, LPDWORD lpdwExtVersion)
{
    return lineNegotiateExtVersion(hLineApp, dwDeviceID,
               dwAPIVersion, dwExtLowVersion,
               dwExtHighVersion, lpdwExtVersion);
}

LONG xxx_linePickup(HLINE hLine, DWORD dwAddressID, LPHCALL lphCall,
               LPCWSTR lpszDestAddress, LPCWSTR lpszGroupID)
{
    return linePickup(hLine, dwAddressID, lphCall,
               lpszDestAddress, lpszGroupID);
}

LONG xxx_linePrepareAddToConference(HCALL hConfCall, LPHCALL lphConsultCall,
               LPLINECALLPARAMS const lpCallParams)
{
    return linePrepareAddToConference(hConfCall, lphConsultCall, lpCallParams);
}

LONG xxx_lineRedirect(HCALL hCall, LPCWSTR lpszDestAddress,
               DWORD dwCountryCode)
{
    return lineRedirect(hCall, lpszDestAddress, dwCountryCode);
}

LONG xxx_lineReleaseUserUserInfo(HCALL hCall)
{
    return lineReleaseUserUserInfo(hCall);
}

LONG xxx_lineRemoveFromConference(HCALL hCall)
{
    return lineRemoveFromConference(hCall);
}

LONG xxx_lineSendUserUserInfo(HCALL hCall, LPCSTR lpsUserUserInfo,
               DWORD dwSize)
{
    return lineSendUserUserInfo(hCall, lpsUserUserInfo, dwSize);
}

LONG xxx_lineSetAppPriority(LPCWSTR lpszAppFilename, DWORD dwMediaMode,
               LPLINEEXTENSIONID lpExtensionID, DWORD dwRequestMode,
               LPCWSTR lpszExtensionName, DWORD dwPriority)
{
    return lineSetAppPriority(lpszAppFilename, dwMediaMode, lpExtensionID,
                dwRequestMode, lpszExtensionName, dwPriority);
}

LONG xxx_lineSetCallParams(HCALL hCall, DWORD dwBearerMode, DWORD dwMinRate,
               DWORD dwMaxRate, LPLINEDIALPARAMS const lpDialParams)
{
    return lineSetCallParams(hCall, dwBearerMode, dwMinRate, dwMaxRate, lpDialParams);
}

LONG xxx_lineSetCallPrivilege(HCALL hCall, DWORD dwCallPrivilege)
{
    return lineSetCallPrivilege(hCall, dwCallPrivilege);
}

LONG xxx_lineSetMediaMode(HCALL hCall, DWORD dwMediaModes)
{
    return lineSetMediaMode(hCall, dwMediaModes);
}

LONG xxx_lineSetNumRings(HLINE hLine, DWORD dwAddressID, DWORD dwNumRings)
{
    return lineSetNumRings(hLine, dwAddressID, dwNumRings);
}

LONG xxx_lineSetTerminal(HLINE hLine, DWORD dwAddressID, HCALL hCall,
               DWORD dwSelect, DWORD dwTerminalModes, DWORD dwTerminalID,
               DWORD bEnable)
{
    return lineSetTerminal(hLine, dwAddressID, hCall,
               dwSelect, dwTerminalModes, dwTerminalID, bEnable);
}

LONG xxx_lineSetTollList(HLINEAPP hLineApp, DWORD dwDeviceID,
               LPCWSTR lpszAddressInW, DWORD dwTollListOption)
{
    return lineSetTollList(hLineApp, dwDeviceID, lpszAddressInW, dwTollListOption);
}


LONG xxx_lineSetupConference(HCALL hCall, HLINE hLine, LPHCALL lphConfCall,
               LPHCALL lphConsultCall, DWORD dwNumParties,
               LPLINECALLPARAMS const lpCallParams)
{
    return lineSetupConference(hCall, hLine, lphConfCall,
               lphConsultCall, dwNumParties, lpCallParams);
}

LONG xxx_lineSetupTransfer(HCALL hCall, LPHCALL lphConsultCall,
               LPLINECALLPARAMS const lpCallParams)
{
    return lineSetupTransfer(hCall, lphConsultCall, lpCallParams);
}

LONG xxx_lineSwapHold(HCALL hActiveCall, HCALL hHeldCall)
{
    return lineSwapHold(hActiveCall, hHeldCall);
}

LONG xxx_lineUnhold(HCALL hCall)
{
    return lineUnhold(hCall);
}

LONG xxx_lineShutdown(
    HLINEAPP hLineApp
    )
{
    return TAPIlineShutdown(hLineApp);
}
LONG xxx_phoneInitializeEx(
    LPHPHONEAPP lphPhoneApp,
    HINSTANCE hInstance,
    PHONECALLBACK lpfnCallback,
    LPCWSTR lpszFriendlyAppName,
    LPDWORD lpdwNumDevs,
    LPDWORD lpdwAPIVersion,
    LPPHONEINITIALIZEEXPARAMS lpPhoneInitializeExParams)
{
    return    TAPIphoneInitializeEx(lphPhoneApp,
                                    hInstance,
                                    lpfnCallback,
                                    lpszFriendlyAppName,
                                    lpdwNumDevs,
                                    lpdwAPIVersion,
                                    lpPhoneInitializeExParams);
}

LONG xxx_phoneClose(HPHONE hPhone)
{
    return phoneClose(hPhone);
}

LONG xxx_phoneConfigDialog(DWORD dwDeviceID, HWND hwndOwner,
               LPCWSTR lpszDeviceClass)
{
    return phoneConfigDialog(dwDeviceID, hwndOwner, lpszDeviceClass);
}

LONG xxx_phoneDevSpecific(HPHONE hPhone, LPVOID lpParams, DWORD dwSize)
{
    return phoneDevSpecific(hPhone, lpParams, dwSize);
}

LONG xxx_phoneGetDevCaps(HPHONEAPP hPhoneApp, DWORD dwDeviceID,
               DWORD dwAPIVersion, DWORD dwExtVersion, LPPHONECAPS lpPhoneCaps)
{
    return phoneGetDevCaps(hPhoneApp, dwDeviceID,
               dwAPIVersion, dwExtVersion, lpPhoneCaps);
}

LONG xxx_phoneGetGain(HPHONE hPhone, DWORD dwHookSwitchDev, LPDWORD lpdwGain)
{
    return phoneGetGain(hPhone, dwHookSwitchDev, lpdwGain);
}

LONG xxx_phoneGetHookSwitch(HPHONE hPhone, LPDWORD lpdwHookSwitchDevs)
{
    return phoneGetHookSwitch(hPhone, lpdwHookSwitchDevs);
}

LONG xxx_phoneGetIcon(DWORD dwDeviceID, LPCWSTR lpszDeviceClass,
               LPHICON lphIcon)
{
    return phoneGetIcon(dwDeviceID, lpszDeviceClass, lphIcon);
}


LONG xxx_phoneGetID(HPHONE hPhone, LPVARSTRING lpDeviceID,
               LPCWSTR lpszDeviceClass)
{
    return phoneGetID(hPhone, lpDeviceID, lpszDeviceClass);
}

LONG xxx_phoneGetMessage(HPHONEAPP hPhoneApp, LPPHONEMESSAGE lpMessage,
               DWORD dwTimeout)
{
    return phoneGetMessage(hPhoneApp, lpMessage, dwTimeout);
}

LONG xxx_phoneGetRing(HPHONE hPhone, LPDWORD lpdwRingMode,
               LPDWORD lpdwVolume)
{
    return phoneGetRing(hPhone, lpdwRingMode, lpdwVolume);
}

LONG xxx_phoneGetStatus(HPHONE hPhone, LPPHONESTATUS lpPhoneStatus)
{
    return phoneGetStatus(hPhone, lpPhoneStatus);
}

LONG xxx_phoneGetStatusMessages(HPHONE hPhone, LPDWORD lpdwPhoneStates,
               LPDWORD lpdwButtonModes, LPDWORD lpdwButtonStates)
{
    return phoneGetStatusMessages(hPhone, lpdwPhoneStates,
               lpdwButtonModes, lpdwButtonStates);
}

LONG xxx_phoneGetVolume(HPHONE hPhone, DWORD dwHookSwitchDev,
               LPDWORD lpdwVolume)
{
    return phoneGetVolume(hPhone, dwHookSwitchDev, lpdwVolume);
}

LONG xxx_phoneNegotiateAPIVersion(HPHONEAPP hPhoneApp, DWORD dwDeviceID,
               DWORD dwAPILowVersion, DWORD dwAPIHighVersion,
               LPDWORD lpdwAPIVersion, LPPHONEEXTENSIONID lpExtensionID)
{
    return phoneNegotiateAPIVersion(hPhoneApp, dwDeviceID, dwAPILowVersion,
               dwAPIHighVersion, lpdwAPIVersion, lpExtensionID);
}

LONG xxx_phoneNegotiateExtVersion(HPHONEAPP hPhoneApp, DWORD dwDeviceID,
               DWORD dwAPIVersion, DWORD dwExtLowVersion,
               DWORD dwExtHighVersion, LPDWORD lpdwExtVersion)
{
    return phoneNegotiateExtVersion(hPhoneApp, dwDeviceID,
               dwAPIVersion, dwExtLowVersion,
               dwExtHighVersion, lpdwExtVersion);
}

LONG xxx_phoneOpen(HPHONEAPP hPhoneApp, DWORD dwDeviceID, LPHPHONE lphPhone,
               DWORD dwAPIVersion, DWORD dwExtVersion,
               DWORD dwCallbackInstance, DWORD dwPrivilege)
{
    return phoneOpen(hPhoneApp, dwDeviceID, lphPhone, dwAPIVersion, dwExtVersion,
               dwCallbackInstance, dwPrivilege);
}

LONG xxx_phoneSetGain(HPHONE hPhone, DWORD dwHookSwitchDev, DWORD dwGain)
{
    return phoneSetGain(hPhone, dwHookSwitchDev, dwGain);
}

LONG xxx_phoneSetHookSwitch(HPHONE hPhone, DWORD dwHookSwitchDevs,
               DWORD dwHookSwitchMode)
{
    return phoneSetHookSwitch(hPhone, dwHookSwitchDevs, dwHookSwitchMode);
}

LONG xxx_phoneSetRing(HPHONE hPhone, DWORD dwRingMode, DWORD dwVolume)
{
    return phoneSetRing(hPhone, dwRingMode, dwVolume);
}

LONG xxx_phoneSetStatusMessages(HPHONE hPhone, DWORD dwPhoneStates,
               DWORD dwButtonModes, DWORD dwButtonStates)
{
    return phoneSetStatusMessages(hPhone, dwPhoneStates,
               dwButtonModes, dwButtonStates);
}

LONG xxx_phoneSetVolume(HPHONE hPhone, DWORD dwHookSwitchDev, DWORD dwVolume)
{
    return phoneSetVolume(hPhone, dwHookSwitchDev, dwVolume);
}

LONG
xxx_phoneShutdown(   // sync
    HPHONEAPP hPhoneApp
    )
{
    return TAPIphoneShutdown(hPhoneApp);
}
