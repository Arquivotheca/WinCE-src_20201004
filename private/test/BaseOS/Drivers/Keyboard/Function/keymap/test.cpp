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
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright  1998-2004  Microsoft Corporation.  All Rights Reserved.
//
// Module Name:
//
//   testSequence.cpp  
// 
// Abstract:
// 
//   This file contains the test procdures for each key mapping.  The tests
//   prove VK_KEY to WM_CHAR unicode mappings.
//
// Authors:
//  unknown     unknown
//  11/24/04    chetl   added support for multiple states, testing all keyboards in an image.
//  12/01/04    chetl   added support for many Intl keyboards, expanded key sequences.
//

#include <windows.h>

#include "mappings.h"
#include "keyengin.h"
#include "TuxMain.h"
#include "Errmacro.h"

#define countof(x) (sizeof(x)/sizeof(x[0]))

testKeyboard TestKeyboards[] = {
    //Non-smartphone is special so it should always be first in the list.
    //HKL is 0 since it isn't really a keyboard
    { 0x0,  KBDTests_NONSMARTPHON,  countof(KBDTests_NONSMARTPHON)},
    { HKL_KBD_US_QWERTY,  KBDTests_USEnglish,  countof(KBDTests_USEnglish)},
    { HKL_KBD_US_DVORAK,  KBDTests_USDVORAK,   countof(KBDTests_USDVORAK)},
    { HKL_KBD_JAPANESE1,  KBDTests_Japanese1,  countof(KBDTests_Japanese1)},
    { HKL_KBD_HINDI,      KBDTests_Hindi,      countof(KBDTests_Hindi)},
    { HKL_KBD_GUJARATI,   KBDTests_Gujarati,   countof(KBDTests_Gujarati)},
    { HKL_KBD_TAMIL,      KBDTests_Tamil,      countof(KBDTests_Tamil)},
    { HKL_KBD_KANNADA,    KBDTests_Kannada,    countof(KBDTests_Kannada)},
    { HKL_KBD_MARATHI,    KBDTests_Marathi,    countof(KBDTests_Marathi)},
    { HKL_KBD_PUNJABI,    KBDTests_Punjabi,    countof(KBDTests_Punjabi)},
    { HKL_KBD_TELUGU,     KBDTests_Telugu,     countof(KBDTests_Telugu)},
    { HKL_KBD_HEBREW,     KBDTests_Hebrew,     countof(KBDTests_Hebrew)},
    { HKL_KBD_ARABIC,     KBDTests_Arabic,     countof(KBDTests_Arabic)},
    { HKL_KBD_ARABIC_102, KBDTests_Arabic_102, countof(KBDTests_Arabic_102)},
    { HKL_KBD_THAI,       KBDTests_Thai,       countof(KBDTests_Thai)},
    { HKL_KBD_FRENCH,     KBDTests_French,     countof(KBDTests_French)},

};

//
// GetXXX
//
// The next few functions hide details on how we have arranged
// the test matrix data structure.  Use these functions instead
// of directly accessing the structures so we can change things
// later on easily.  -chetl 11/30/04
//

inline KEYBOARDSTATE GetState(testSequence testSeq[], WORD ndx)
{
    KEYBOARDSTATE newstate = testSeq[ndx].nNumVirtualKeys;
    return (newstate & STATE_STATEMASK);
}

inline VOID GetExpected(__in testSequence testSeq[],
                        __in WORD ndx,
                        __out_ecount(nNumChars) WCHAR wcaUnicodeChars[],
                        __in INT nNumChars)
{
    for(INT i = 0; i < nNumChars; i++)
        wcaUnicodeChars[i] = testSeq[ndx].wcaUnicodeChars[i];
    return;
}

inline LPBYTE GetKeys(testSequence testSeq[], WORD ndx)
{
    return testSeq[ndx].byaVirtualKeys;
}

inline WORD GetNumberOfKeys(testSequence testSeq[], WORD ndx)
{
    KEYBOARDSTATE newstate = testSeq[ndx].nNumVirtualKeys;
    return (newstate & STATE_NUMMASK);
}

inline INT GetNumberOfChars(testSequence testSeq[], WORD ndx)
{
    return testSeq[ndx].nNumUnicodeChars;
}
//
// Get_KeyboardTestNdx
//
// Finds a keyboard based on its HKL and returns the index
// for the test data.
//
// Return
//  0xFFFF  no keyboard test data
//  0-200   index into Keyboard test structures
//

inline UINT Get_KeyboardTestNdx(HKL Keyboard)
{
    for(UINT ndx = 0; ndx < countof(TestKeyboards); ndx++)
    {
        if(TestKeyboards[ndx].InputLocale == Keyboard)
            return ndx;
    }
    return KEYBOARD_NOTESTDATA;
}

#define KEYBOARD_SWITCH_SLEEP 2000

//****************************************************************************************
// 
// performKeyMapTest:
// 
//   This function tests the virtual key to unicode mapping in the test sequence
//   through the keyboard driver.
// 
//Arguments:
// 
//   struct testSequence * testSeq :  The mapping to be tested
// 
//Return Value:
// 
//   TUX return value:  TPR_PASS or TPR_FAIL
//
//Notes:
//  The keyboard tests are executed in order, so to test keys in a given state
//  add 1st add a test that puts the keyboard in the state then add tests that
//  test each key, lastly a test that turns off the state.  See keyboard structure
//  at top of file for some examples.
//

TESTPROCAPI performKeyMapTest(testKeyboard *pTestKeyboard) 
{
    testSequence *testSeq = pTestKeyboard->pKeyMaps;
    WORD dwNumberTestCases = pTestKeyboard->numKeyMaps;
    HWND    hTestWnd = NULL;
    INT     iResult = TPR_PASS;
    bool    State_Caps;

    // Following was introduced to make the CAPSLOCK to off. by a-rajtha 10/2/98
    // This works for both US and Japanese keyboards
    BOOL CState=GetKeyState(VK_CAPITAL);
    if(CState) 
    {
        keybd_event(VK_SHIFT,0,0,0);        // why was shift here? -chetl
        keybd_event(VK_CAPITAL, 0, 0, 0 );
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 ); 
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP,0);
        g_pKato->Log(LOG_DETAIL,TEXT("CapsLOCK turned OFF"));
    }
    else
        g_pKato->Log(LOG_DETAIL,TEXT("CapsLOCK is OFF"));

    State_Caps = false;     // caps lock is toggled off

    hTestWnd = CreateKeyMapTestWindow(g_spsShellInfo.hLib, g_pKato);
    //Wait for test window to be ready, otherwise it changes the keyboard back
    Sleep(KEYBOARD_SWITCH_SLEEP);
    ChangeKeyboard(hTestWnd, pTestKeyboard->InputLocale);

    // loop for each key to test
    for(WORD ndx = 0; ndx < dwNumberTestCases; ndx++) 
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Testing case number %d"), ndx+1);

        // Check if the next char causes keyboard to enter or leave a state.
        // If it is toggle the key... 
        KEYBOARDSTATE newstate = GetState(testSeq, ndx);
        if(newstate)
            switch(newstate)
        {
            case STATE_CAPSLOCK: 
                {
                    keybd_event (VK_SHIFT,NULL,0,0);        // why shift keys here -chetl
                    keybd_event( VK_CAPITAL, NULL, 0, 0 );
                    keybd_event( VK_CAPITAL, NULL, KEYEVENTF_KEYUP, 0 ); 
                    keybd_event (VK_SHIFT, NULL, KEYEVENTF_KEYUP,0);
                    g_pKato->Log(LOG_DETAIL, TEXT("        ****  CAPS LOCK PRESSED *****"));

                    // Can't use GetKeyState as its state
                    // changes only after the previous messages are processed.
                    // probably have to keep track with our own internal flags -chetl.
                    State_Caps = !State_Caps;
                    if(State_Caps)
                        g_pKato->Log(LOG_DETAIL, TEXT(" CAPSLOCK is now ON"));
                    else 
                        g_pKato->Log(LOG_DETAIL, TEXT(" CAPSLOCK is now OFF"));
                }
                break;
            default:
                g_pKato->Log(LOG_DETAIL, TEXT(" TEST ERROR - Unknown state/toggle pressed"));
                break;
        }
        else        
        {           // othewise, we are just a series of keystrokes so test them...
            SetForegroundWindow(hTestWnd);
            WORD wNumKeys = GetNumberOfKeys(testSeq, ndx);
            LPBYTE pKeys = GetKeys(testSeq, ndx);
            INT nNumChars = GetNumberOfChars(testSeq, ndx);
            WCHAR wcaExpecteds[MAX_NUM_GEN_CHARS];
            GetExpected(testSeq, ndx, wcaExpecteds, MAX_NUM_GEN_CHARS);
            if(!TestKeyMapChar(g_pKato, hTestWnd, pKeys, wNumKeys, wcaExpecteds, nNumChars))
                iResult = TPR_FAIL;
        }
    }

    RemoveTestWindow(hTestWnd); 

    // warn if a newly added table left the keyboard in a non-default state as that
    // will screw up the next keyboard tested...
    if(State_Caps)
        g_pKato->Log(LOG_DETAIL, TEXT("TEST ERROR - internal keyboard table left CAPS toggled on"));

    //restoring the orignal CAPS lock state. If the caps lock was on leave it on else turn it off. 
    if(CState)
    {
        keybd_event(VK_SHIFT,0,0,0);      // chetl - why shift keys here?
        keybd_event(VK_CAPITAL, 0, 0, 0 );
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 ); 
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP,0);
        g_pKato->Log(LOG_DETAIL, TEXT(" CAPSLOCK left ON"));
    }

    if(iResult == TPR_PASS)
        g_pKato->Log(LOG_DETAIL, TEXT(" Tests Passed"));
    else
        g_pKato->Log(LOG_DETAIL, TEXT(" Tests Failed"));

    return iResult;
}

//
// test_PreHelp
//
// macro that takes care of common TUX tasks for a test procedure.  We may be
// called for number of threads needed for the test or some other reason.  The
// do/while loop forces the macro to act like a statement/function call with a
// required semicolon.  -ctl 11/30/04
//

#define test_PreHelp(uMsg, tpParam)\
        if( uMsg == TPM_QUERY_THREAD_COUNT )\
        {\
            ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;\
            return SPR_HANDLED;\
        }\
        else if (uMsg != TPM_EXECUTE)\
        {\
            return TPR_NOT_HANDLED;\
        }

//
// IsKeyboardInImage
//
// Looks for a given keyboard in the OS Image
//
// Return Code
//  true    if keyboard is included in the OS Image
//  false   otherwise
//

BOOL IsKeyboardInImage(HKL FindKeyboard)
{
    BOOL fReturn = FALSE;

    for (UINT uiPreloadIdx = 1; uiPreloadIdx <= g_cInputLocale; uiPreloadIdx++) {
        if (FindKeyboard == g_rghKeyboardLayout [uiPreloadIdx - 1]) {
            fReturn = TRUE;
        }
    }
    return fReturn;
}

//
// startKeyMapTest, helper function designed for cleaning up test functions
//
TESTPROCAPI startKeyMapTest(HKL InputLocaleID)
{
    INT iResult = TPR_PASS;

    //Get the project type
    wchar_t projectName[MAX_STRING_NAME];
    int projectCode = GetProjectCode(projectName, _countof(projectName));

    if(!IsKeyboardInImage(InputLocaleID))
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Keyboard is not installed/active in OS image, skipping test"));
        return TPR_SKIP;
    }
    // if we have test data on this keyboard, then test it
    UINT uiTestndx = Get_KeyboardTestNdx(InputLocaleID);
    if(uiTestndx != KEYBOARD_NOTESTDATA)
    {
        iResult = performKeyMapTest(&TestKeyboards[uiTestndx]);
        //Only test esc on non-smartphone
        if (!(projectCode == PROJECT_SMARTFON))
        {
            INT iESCResult = performKeyMapTest(&TestKeyboards[0]);
            if (iESCResult == TPR_FAIL)
                iResult = TPR_FAIL;
        }
    }
    else
        g_pKato->Log(LOG_DETAIL, TEXT("*** TEST ERROR *** No data for the keyboard"));

    return iResult;
}

//****************************************************************************************

TESTPROCAPI testUSKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/ )
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning US English keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_US_QWERTY);
}


TESTPROCAPI testJapKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/ )
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Japanese-English keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_JAPANESE1);
}
//
// test_AllKeyboardsinImage
//
// finds each keyboard stored in the CE OS image and then test it provided
// we have information on that keyboard.
//

TESTPROCAPI test_AllKeyboardsinImage( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/ )
{
    WORD wResult = TPR_PASS;
    BOOL bFail = FALSE;

    static TCHAR CurrKeyboardName[KL_NAMELENGTH];
    test_PreHelp(uMsg, tpParam);

    //Get the project type
    wchar_t projectName[MAX_STRING_NAME];
    int projectCode = GetProjectCode(projectName, _countof(projectName));
    
    // Remember old active keyboard layout 
    HKL oldLayout = GetKeyboardLayout(NULL);

    // test each keyboard in image
    for(UINT ndx = 0; ndx < g_cInputLocale; ndx++)
    {
        // Get name of each keyboard
        HKL CurrHKL = g_rghKeyboardLayout [ndx];
        ActivateKeyboardLayout(CurrHKL, NULL);
        GetKeyboardLayoutName(CurrKeyboardName);
        g_pKato->Log(LOG_DETAIL, TEXT("Test keyboard with HKL 0x%04x named %s"), CurrHKL, CurrKeyboardName);

        // if we have test data on this keyboard, then test it
        UINT uiTestndx = Get_KeyboardTestNdx(CurrHKL);
        if(uiTestndx != KEYBOARD_NOTESTDATA)
        {
            g_pKato->Log(LOG_DETAIL, TEXT("Beginning %s keyboard mapping test"), CurrKeyboardName);
            INT iTestResult = performKeyMapTest(&TestKeyboards[uiTestndx]);
            //Test non-smartphone keys
            if (!(projectCode == PROJECT_SMARTFON))
            {
                iTestResult = performKeyMapTest(&TestKeyboards[0]);
            }

            if(iTestResult != TPR_PASS)
            {
                g_pKato->Log(LOG_DETAIL, TEXT("** FAILED ** test failed on keyboard"));
                wResult = TPR_FAIL;
                bFail = TRUE;
            }
        } 
        else    // othewise notify user we skipped this keyboard
        {
            g_pKato->Log(LOG_DETAIL, TEXT("No test data for keyboard %s"), CurrKeyboardName);
            wResult = TPR_SKIP;
        }
    }

    // reset active keyboard layout to be whatever was active prior to our test
    ActivateKeyboardLayout(oldLayout, NULL);

    // if one failed - the test case failed
    if (bFail)
        return TPR_FAIL;

    // if none failed, then either SKIP or PASS depending on there is any missing keyboard data.
    return wResult;
}

//
// testARAKeyMapping
//
TESTPROCAPI testARAKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/ )
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning ARA (101) keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_ARABIC);
}

//
// testARA102KeyMapping
//
TESTPROCAPI testARA102KeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/ )
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning ARA (102) keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_ARABIC_102);
}

// 
// testHEBKeyMapping
//
TESTPROCAPI testHEBKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Hebrew keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_HEBREW);
}

// 
// testHINKeyMapping
//
TESTPROCAPI testHINKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Hindi keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_HINDI);
}

// testGUJKeyMapping
//
TESTPROCAPI testGUJKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Gujarati keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_GUJARATI);
}

// 
// testTAMKeyMapping
//
TESTPROCAPI testTAMKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Tamil keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_TAMIL);
}

// 
// testKANKeyMapping
//
TESTPROCAPI testKANKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Kannada keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_KANNADA);
}

// 
// testMARKeyMapping
//
TESTPROCAPI testMARKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Marathi keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_MARATHI);
}

// 
// testPUNKeyMapping
//
TESTPROCAPI testPUNKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Punjabi keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_PUNJABI);
}

// 
// testTELKeyMapping
//
TESTPROCAPI testTELKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Telugu keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_TELUGU);
}

// 
// testTHAKeyMapping
//
TESTPROCAPI testTHAKeyMapping(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    test_PreHelp(uMsg, tpParam);

    g_pKato->Log(LOG_DETAIL, TEXT("Beginning Thai keyboard mapping test"));

    return startKeyMapTest(HKL_KBD_THAI);
}



/// <summary>
///   Returns the platform type.
/// </summary>
/// <param name="pPlatformName"> Buffer to hold the platform name</param>
/// <returns>
///   INT: 
///     returns the platform code.. Only the test platform are defined here. If the platform is not defined
///     it returns PLATFORM_UNSUPPORTED (0xD903)
/// </returns>
/// <remarks>
/// Below are the defined platforms. Inorder to increase the efficeincy of the test, the platform names are defined with integer
/// constants. And, these constant values are returned.
/// PLATFORM_CEPC = 0x10
/// PLATFORM_UNSUPPORTED = 0xD903
/// PLATFORM_TORNADO = 0x12;
/// PLATFORM_EXCALIBER = 0x13

/// If the buffer is not, it returns the platform name
/// </remarks>
int GetPlatformCode(_In_count_(uiLen) wchar_t *pPlatformName, UINT uiLen)
{
    wchar_t platformName[MAX_STRING_NAME];
    SetLastError(ERROR_SUCCESS);
    int platformRet = SystemParametersInfo(
        SPI_GETPLATFORMNAME, 
        MAX_STRING_NAME,
        (PVOID *)platformName,
        0
        );
    if(pPlatformName)
    {
        wcscpy_s(pPlatformName, uiLen, platformName);
    }
    if(platformRet == 0)
    {
        RETAILMSG(TRUE, (TEXT("ERROR: Unable to get the platform name. Error code is %d"), GetLastError()));
        return PLATFORM_UNSUPPORTED;
    }
    if(wcsnicmp(platformName,L"CEPC",MAX_STRING_NAME) == 0)
    {
        return PLATFORM_CEPC;
    }
    else if(wcsnicmp(platformName,L"TORNADO",MAX_STRING_NAME) == 0)
    {
        return PLATFORM_TORNADO;
    }
    else if(wcsnicmp(platformName,L"EXCALIBER",MAX_STRING_NAME) == 0)
    {
        return PLATFORM_EXCALIBER;
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("The platform is %s. It is not supported for our test"),platformName));
        return PLATFORM_UNSUPPORTED;
    }
}

/// <summary>
///   Returns the project type.
/// </summary>
/// <param name="pPlatformName"> Buffer to hold the project name</param>
/// <returns>
///   INT: 
///     Returns the project code. If the project is cebase, it returns PROJECT_CE. If the project is smartfon, it returns PROJECT_SMARTFON
///     Otherwise, it returns PROJECT_UNSUPPORTED
/// </returns>
/// <remarks>
/// Below are the defined project codes. Defining the Project names in to project code improves the performance of the test code
/// PROJECT_CE = 0x01;
/// PROJECT_SMARTFON = 0x02;
/// PROJECT_UNSUPPORTED = 0xD903;

/// If pProjectName is not null then the project name is returned.
/// </remarks>
int GetProjectCode(_In_count_(uiLen) wchar_t *pProjectName, UINT uiLen)
{
    wchar_t projectName[MAX_STRING_NAME];
    SetLastError(ERROR_SUCCESS);
    int projectRet = SystemParametersInfo(
        SPI_GETPROJECTNAME, 
        MAX_STRING_NAME,
        (PVOID *)projectName,
        0
        );
    if(pProjectName)
    {
        wcscpy_s(pProjectName, uiLen, projectName);
    }
    if(projectRet == 0)
    {
        RETAILMSG(TRUE, (TEXT("ERROR: Unable to get the project name. Error code is %d"), GetLastError()));
        return PROJECT_UNSUPPORTED;
    }
    if(wcsnicmp(projectName,L"CEBASE",MAX_STRING_NAME) == 0)
    {
        return PROJECT_CE;
    }
    else if(wcsnicmp(projectName,L"SMARTPHONE",MAX_STRING_NAME) == 0)
    {
        return PROJECT_SMARTFON;
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("The project is %s. It is not supported for our test"),projectName));
        return PROJECT_UNSUPPORTED;
    }
}

static TCHAR *szNULLStr = TEXT ("\0");

/// <summary>
///      If an IME HKL is given, the HKL for the Keyboard Layout is fetched.
/// </summary>
/// <param name="hInputLocale"> Contains the IME HKL </param>
/// <param name="phKeyboardLayoutHandle"> Pointer to HKL in which the HKL to the Keyboard Layout would be stored </param>
/// <returns>
///      BOOL:
///      Returns TRUE if IME HKL is found at HKLM\System\CurrentControlSet\Control\Layouts\ and "Keyboard Layout" value is found under it.
///      Else FALSE
/// </returns>
BOOL GetKeyboardHandleFromInputLocaleID (HKL hInputLocale, HKL *phKeyboardLayoutHandle)
{
    HKEY hKey = NULL;
    TCHAR szRegKey[_countof (LAYOUT_PATH) + 8] = { NULL }; //8 is the # of TCHARs in an HKL
    TCHAR szKeyboardLayoutHandle[MAX_PATH] = { NULL };
    ULONG ulHKLSize = sizeof (szKeyboardLayoutHandle);
    BOOL fReturn = TRUE;

    StringCchPrintf (szRegKey, _countof (szRegKey), TEXT ("%s%08x"), LAYOUT_PATH, (UINT_PTR)hInputLocale);
    
    //Read value "Keyboard Layout" from HKLM\System\CurrentControlSet\Control\Layouts\<HKL>
    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx (hKey, TEXT ("Keyboard Layout"), NULL, NULL, (LPBYTE)szKeyboardLayoutHandle, &ulHKLSize) == ERROR_SUCCESS) {
            *phKeyboardLayoutHandle = (HKL)(_tcstoul (szKeyboardLayoutHandle, &szNULLStr, 16));
        } else { LOGF_FAIL (TEXT ("RegQueryValue Ex () failed")); fReturn = FALSE; }
        RegCloseKey (hKey);
    } else { LOGF_FAIL (TEXT ("RegOpenKeyEx () failed")); fReturn = FALSE; }

    return fReturn;
}

/// <summary>
///     Loads the HKLs present at HKCR\Keyboard Layout\Preload to g_rghInputLocale 
///     If the HKL is an IME, read the HKLM\System\CurrentControlSet\Control\Layouts\<IME_HKL>\"Keyboard Layout" value into g_rghKeyboardLayout
/// </summary>
/// <returns>
///   BOOL: 
///   Returns TRUE if HKCR\Keyboard Layout\Preload is found and HKLs have been retrieved. And also if any of the HKLs is an IME, the corresponding Keyboard Layout HKL has been retrieved
///   Else returns FALSE
/// </returns>

BOOL GetPreloadHKLs ()
{
    BOOL fReturn = TRUE;
    TCHAR szRegKey[_countof (PRELOAD_PATH) + 2] = {NULL}; // 2 is the # of TCHARS in the MAX_HKL_PRELOAD_COUNT
    HKEY hKey = 0;
    TCHAR szLayout[8 + 1] = {NULL}; //8 is the # of TCHARS in an HKL
    DWORD ulLayoutSize = sizeof (szLayout);

    for (int iPreloadNum = 1; iPreloadNum <= MAX_HKL_PRELOAD_COUNT; iPreloadNum++) {
        ulLayoutSize = sizeof (szLayout);
        StringCchPrintf (szRegKey, _countof (szRegKey), TEXT ("%s%d"), PRELOAD_PATH, iPreloadNum);
        
        //query the registry and retrieve the current values for input layouts 
        if (RegOpenKeyEx(HKEY_CURRENT_USER, (LPCWSTR)szRegKey, 0,0,&hKey)==ERROR_SUCCESS) {
            if(RegQueryValueEx(hKey,_T("Default"),NULL,NULL,(LPBYTE)szLayout, &ulLayoutSize)==ERROR_SUCCESS) {

                g_rghInputLocale[g_cInputLocale++] = (HKL) _tcstoul (szLayout,&szNULLStr,16);
                g_rghKeyboardLayout[g_cInputLocale - 1] = g_rghInputLocale [g_cInputLocale - 1];

                //If HKL is an IME, get the "Keyboard Layout" value from HKLM\System\CurrentControlSet\Control\Layouts\<IME_HKL>
                if ((0xE0000000 & (UINT_PTR) g_rghInputLocale [g_cInputLocale - 1])) {
                    if (GetKeyboardHandleFromInputLocaleID(g_rghInputLocale [g_cInputLocale - 1], &g_rghKeyboardLayout [g_cInputLocale - 1]) != TRUE) {
                        LOGF_FAIL (TEXT ("GetKeyboardHandleFromInputLocalID () failed"));
                        fReturn = FALSE;
                    }
                }

                RegCloseKey(hKey);
            } else { LOGF_FAIL (TEXT ("RegQueryValueEx () failed")); fReturn = FALSE; }
        } else { LOGF_DETAIL (TEXT("RegOpenKeyEx () failed")); }
    }

    return fReturn;
}
