/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998  Microsoft Corporation.  All Rights Reserved.

Module Name:

     testSequence.cpp  
 
Abstract:
 
	This file contains the test procdures for each key mapping.
--*/
#include <windows.h>
#include "testSequences.h"
#include "keyengin.h"
#include "TuxMain.h"

#define countof(x) (sizeof(x)/sizeof(x[0]))
TESTPROCAPI performKeyMapTest(testSequence testSeq [], int nNumberTestCases);

//****************************************************************************************

TESTPROCAPI testUSKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	
	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

	g_pKato->Log(LOG_DETAIL, TEXT("Beginning US English keyboard mapping test"));
	return performKeyMapTest(USEnglish, countof(USEnglish));
}


TESTPROCAPI testJapKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	
	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

	g_pKato->Log(LOG_DETAIL, TEXT("Beginning Japanese-English keyboard mapping test"));
	return performKeyMapTest(Japanese1, countof(Japanese1));
/*    DWORD result1;

	result1 = performKeyMapTest(Japanese1, countof(Japanese1),FALSE);
	return (result1 ); */
}

//****************************************************************************************
/*++
 
performKeyMapTest:
 
	This function tests the virtual key to unicode mapping in the test sequence
	through the keyboard driver.
 
Arguments:
 
	struct testSequence * testSeq :  The mapping to be tested
	int nNumberTestCases		  :  Number of test cases in the mapping
 
Return Value:
 
	TUX return value:  TPR_PASS or TPR_FAIL
--*/
TESTPROCAPI performKeyMapTest(testSequence testSeq[], int nNumberTestCases ) {
	HWND	hTestWnd = NULL;
	DWORD	dwResult = TPR_PASS;
    
 // Following was introduced to make the CAPSLOCK to off. by a-rajtha 10/2/98
 // This works for both US and Japanese keyboards

      BOOL  CState=GetKeyState(VK_CAPITAL);
	if (CState) {
			keybd_event (VK_SHIFT,0,0,0); 
			keybd_event (VK_CAPITAL, 0, 0, 0 );
			keybd_event (VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 ); 
			keybd_event (VK_SHIFT, 0, KEYEVENTF_KEYUP,0);
//    for (int i =0; i < 200000; i++); 
			g_pKato->Log(LOG_DETAIL,TEXT ("CapsLOCK is now OFF "));
	} // Shut off the capslock


	hTestWnd = CreateKeyMapTestWindow( g_spsShellInfo.hLib, g_pKato );
	
	for(int x=0; x < nNumberTestCases; x++) {
		g_pKato->Log(LOG_DETAIL, TEXT("Testing case number %d"), x+1);
			// Check if the next char is caps lock by looking at the nNumVirtualKeys. if it is 
		    //  Caps lock,  toggle the caps lock. 

		if (testSeq[x].nNumVirtualKeys ==9) {
			keybd_event (VK_SHIFT,NULL,0,0); 
			keybd_event( VK_CAPITAL, NULL, 0, 0 );
			keybd_event( VK_CAPITAL, NULL, KEYEVENTF_KEYUP, 0 ); 
			keybd_event (VK_SHIFT, NULL, KEYEVENTF_KEYUP,0);
			g_pKato->Log(LOG_DETAIL, TEXT("        ****  CAPS LOCK PRESSED *****"));

			if (GetKeyState(VK_CAPITAL)) 
                 g_pKato->Log(LOG_DETAIL, TEXT(" CAPSLOCK IS ON"));
			else 
                 g_pKato->Log(LOG_DETAIL, TEXT(" CAPSLOCK IS OFF"));
			}
		else {
			if (!TestKeyMapChar(g_pKato, hTestWnd, testSeq[x].byaVirtualKeys, 
			testSeq[x].nNumVirtualKeys, testSeq[x].wcUnicodeChar))
			dwResult = TPR_FAIL; }
	}
	
	
	RemoveTestWindow(hTestWnd); 
	//restoring the orignal CAPS lock state. If the caps lock was on leave it on else turn it off. 
	if ( !CState)
	{
            keybd_event (VK_SHIFT,0,0,0); 
		keybd_event (VK_CAPITAL, 0, 0, 0 );
		keybd_event (VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 ); 
		keybd_event (VK_SHIFT, 0, KEYEVENTF_KEYUP,0);
	    }

	return dwResult;
}
