/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1995-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keyState.cpp  

Abstract:

  Automated testing of keybd_event, GetKeyState, and GetAsyncKeyState APIs.

--*/

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "global.h"

//**************************************************************************
//    Function prototypes
//**************************************************************************

//Testing of keybd_event and getkeystate pressed state
void pressAllKeys();
BOOL checkPressStateOfKey(BYTE, BOOL);
void generateKeyMessagesThread();
BOOL pressKeyAndWait(HWND hwnd, BYTE byVirtualKey);
BOOL isPressKeySupported(BYTE byVirtualKey) ;
void performPressKeyStateTest();
void performKeyStateTest2();

// toggle testing of getKeyState
BOOL checkToggle(BYTE byVKey, BOOL bToggle);
BOOL isToggleKeySupported(BYTE byVirtualKey);
void performToggleKeyStateTest();
void toggleTestThread();

void performAsyncKeyState();

LRESULT CALLBACK KeyStateWndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);


//*****************************************************************************
//    Globals
//*****************************************************************************

// CE supports only the following virtual keys for GetKeyState
//  to determine if the key is pressed
static BYTE g_pbyPressStateSupportedVKs[] = {VK_CONTROL, 
										     VK_LCONTROL, 
										     VK_RCONTROL, 
										     VK_SHIFT,
										     VK_RSHIFT,
										     VK_LSHIFT,
										     VK_MENU,
										     VK_LMENU,
										     VK_RMENU										
};

// CE Toggle supported keys
static BYTE g_pbyToggleStateSupportedVKs[] = {VK_CAPITAL										
};

static HWND g_hwndTestWindow;

static HANDLE g_hReceivedKeyDown = NULL;
static HANDLE g_hReceivedKeyUp   = NULL;
static HANDLE g_hCheckedToggle   = NULL;

#define countof(a) (sizeof(a)/sizeof(*(a)))
#define WM_CHECKTOGGLE (WM_USER + 1)

//*****************************************************************************
//     TEST ENTRY POINT
//*****************************************************************************

TESTPROCAPI AutoKeyState_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	 NO_MESSAGES;

	 HINSTANCE hInstance = globalInst;

	 clean();
	 SendMessage(globalHwnd,WM_SWITCHPROC,KEYSTATE,0);

	 g_hwndTestWindow = globalHwnd;
     SetFocus(g_hwndTestWindow);
	 InvalidateRect(g_hwndTestWindow, NULL, TRUE);

	 // Create the event handles for sync
	 g_hReceivedKeyDown = CreateEvent(NULL, TRUE, FALSE, NULL );
	 if (g_hReceivedKeyDown == NULL) {
			g_pKato->Log( LOG_DETAIL, TEXT("Create Key Down Event failed!"));
			return TPR_ABORT;
	 }
	 g_hReceivedKeyUp = CreateEvent(NULL, TRUE, FALSE, NULL );
	 if (g_hReceivedKeyUp == NULL) {
			g_pKato->Log( LOG_DETAIL, TEXT("Create Key Up Event failed!"));
			return TPR_ABORT;
	 }
	 g_hCheckedToggle = CreateEvent(NULL, TRUE, FALSE, NULL );
	 if (g_hCheckedToggle == NULL) {
			g_pKato->Log( LOG_DETAIL, TEXT("Create Check Toggle Event failed!"));
			return TPR_ABORT;
	 }		   

	 // Run the tests
	   performToggleKeyStateTest();
	   performPressKeyStateTest();
	   performKeyStateTest2();
	   performAsyncKeyState();
 
	   clean();
	   CloseHandle(g_hReceivedKeyDown);
	   CloseHandle(g_hReceivedKeyUp);
	   CloseHandle(g_hCheckedToggle);

     return getCode();
}

//***************************************************************************

LRESULT CALLBACK KeyStateWndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
     {
	 HDC			hdc;
	 PAINTSTRUCT	ps;
	 RECT			rect;

     switch (iMsg)
          {
          case WM_KEYDOWN :
			   checkPressStateOfKey((BYTE)wParam, TRUE);
			   // ready for next one, the listening thread must reset event
			   SetEvent(g_hReceivedKeyDown);
			   return 0;

		  case WM_KEYUP :
			   checkPressStateOfKey((BYTE)wParam, FALSE);
			   // ready for next one, the listening thread must reset event
			   SetEvent(g_hReceivedKeyUp);
			   return 0;

		  case WM_CHECKTOGGLE :
			   checkToggle((BYTE)wParam, (BOOL)lParam);
			   SetEvent(g_hCheckedToggle);
			   return 0;

		  case WM_PAINT:
			   hdc = BeginPaint(hwnd, &ps);
			   GetClientRect(hwnd, &rect);
			   DrawText(hdc, 
				   TEXT("Automated Keyboard Testing\r\nDon't touch the keyboard."),
				   -1, &rect, DT_CENTER);
			   EndPaint(hwnd, &ps);
			   return 0;


          case WM_DESTROY :
               PostQuitMessage (0) ;
               return 0 ;
          }
     return DefWindowProc (hwnd, iMsg, wParam, lParam) ;
     }

//************************************************************************
/*++
 
performKeyStateTest2:
 
	Check the GetKeyState API for invalid virtual keys
	Is this Core OS or driver?
--*/
void performKeyStateTest2() {

	// make sure it doesn't crash system
	GetKeyState(256);
	GetKeyState(1000);
	GetKeyState(INT_MAX);
	GetKeyState(-1);
	GetKeyState(INT_MIN);
}

//************************************************************************
/*++
 
performAsyncKeyState:
 
	Check the GetAsyncKeyState API for all virtual keys
	and for invalid values.  Only makes sure system doesn't crash.
	Is this Core OS or driver?
--*/

void performAsyncKeyState() {

	for(int x = 0; x < 256; x++) {
		GetAsyncKeyState(x);
	}

	// try some invalid parameters
	g_pKato->Log( LOG_DETAIL, TEXT("Getting async on 256\n"));
	GetAsyncKeyState(256);
	g_pKato->Log( LOG_DETAIL, TEXT("Getting async on 1000\n"));
	GetAsyncKeyState(1000);
	g_pKato->Log( LOG_DETAIL, TEXT("Getting async on max int\n"));
	GetAsyncKeyState(INT_MAX);
	g_pKato->Log( LOG_DETAIL, TEXT("Getting async on -1\n"));
	GetAsyncKeyState(-1);
	g_pKato->Log( LOG_DETAIL, TEXT("Getting async on min int\n"));
	GetAsyncKeyState(INT_MIN);
}

//*******  TOGGLE TESTING *********************************************

//************************************************************************
/*++
 
performToggleKeyStateTEest:
 
	Check the GetKeyState API for proper toggle state of keys 
--*/
void performToggleKeyStateTest() {

	// ask user to turn off lights
	MessageBox(g_hwndTestWindow,
        TEXT("Please turn CapsLock keyboard light off, then press OK."),
        TEXT("Caps Lock testing"),
        MB_OK
        );

	g_pKato->Log( LOG_DETAIL, TEXT("Starting toggle state test\n")); 

	// Create the key driver thread
	 HANDLE hThread = NULL;
	 DWORD  dRtn;
	 hThread = CreateThread(NULL, 0, 
		 (LPTHREAD_START_ROUTINE)toggleTestThread, NULL, 0, &dRtn);
	 if (NULL == hThread) {
		 g_pKato->Log( LOG_DETAIL, 
			 TEXT("Could not create toggle test thread! Aborted.\n"));
		 return;
	 }

	 SetFocus(g_hwndTestWindow);
	 MSG         msg ;
     while (GetMessage (&msg, NULL, 0, 0))
          {
          TranslateMessage (&msg) ;
          DispatchMessage (&msg) ;
          }

	 // give thread a chance to exit
	 WaitForSingleObject(hThread, 500);

	 g_pKato->Log( LOG_DETAIL, TEXT("Press Toggle Test finished\n"));

	 CloseHandle(hThread);
}


void toggleTestThread() {
	//not concerned with testing keybd_event, so
	// just going to try supported keys
	DWORD  dwRtn ;
	TCHAR szBuffer[100];

	for(int x=0; x < countof(g_pbyToggleStateSupportedVKs); x++) {
		// turn on key
		pressKeyAndWait(g_hwndTestWindow, g_pbyToggleStateSupportedVKs[x]);
		PostMessage(g_hwndTestWindow, WM_CHECKTOGGLE, 
			g_pbyToggleStateSupportedVKs[x], TRUE);
		
	    dwRtn  = WaitForSingleObject(g_hCheckedToggle, 5000);
		if (WAIT_OBJECT_0 != dwRtn) {
			_stprintf(szBuffer, TEXT("*** FAILURE: Toggle event was not recieved\n"));
			g_pKato->Log( LOG_FAIL, szBuffer);
		}
		ResetEvent(g_hCheckedToggle);
		
		// turn off key
		pressKeyAndWait(g_hwndTestWindow, g_pbyToggleStateSupportedVKs[x]);
		PostMessage(g_hwndTestWindow, WM_CHECKTOGGLE, 
			g_pbyToggleStateSupportedVKs[x], FALSE);
		
	    dwRtn  = WaitForSingleObject(g_hCheckedToggle, 5000);
		if (WAIT_OBJECT_0 != dwRtn) {
			_stprintf(szBuffer, TEXT("*** FAILURE: Toggle event was not recieved\n"));
			g_pKato->Log( LOG_FAIL, szBuffer);
		}
		ResetEvent(g_hCheckedToggle);
	}

	PostMessage(g_hwndTestWindow, WM_DESTROY, 0, 0);
	ExitThread(0);
}
//************************************************************************
/*++
 
isToggleKeySupported:
 
	Check is virtual key is supported by CE for GetKeyState toggle state
 
Arguments:
 
    BYTE byVKey   :  Virtual Key to check out

Returns:  TRUE if the key is supported
--*/


BOOL isToggleKeySupported(BYTE byVirtualKey) {
	for (int x = 0; x < countof(g_pbyToggleStateSupportedVKs); x++) {
		if (byVirtualKey == g_pbyToggleStateSupportedVKs[x])
			return TRUE;
	}
	return FALSE;
}
//************************************************************************
/*++
 
checkToggle:
 
	Check toggled state of virtual key.  Log failure if it is in wrong state.
 
Arguments:
 
    BYTE byVKey   :  Virtual Key to check
	BOOL bToggle  :  Expected Toggle state of key (TRUE = toggled)

Returns:  TRUE if the key was in expected state, otherise FALSE
--*/
BOOL checkToggle(BYTE byVKey, BOOL bToggle) {
    if (isToggleKeySupported(byVKey)) {
		TCHAR szBuffer[100];
		SHORT sStatus = GetKeyState(byVKey);
		BOOL bToggleStateOfKey = (0x0001 & sStatus)?TRUE:FALSE;
		if (bToggle == bToggleStateOfKey) {
		
			_stprintf(szBuffer, TEXT("Key %d was %s as expected\n"), byVKey,
				bToggleStateOfKey? TEXT("toggleded"):TEXT("not toggled"));
			g_pKato->Log( LOG_DETAIL, szBuffer);
			return TRUE;
		}
		else {
			_stprintf(szBuffer, 
				TEXT("***FAILURE Key %d was unexpectedly %s.  Error:%d\n"), 
				byVKey, bToggleStateOfKey? TEXT("toggled"):TEXT("not toggled"),
				GetLastError());
			g_pKato->Log( LOG_FAIL, szBuffer);
			return FALSE;
		}		
	}
	// unsuported key
	return TRUE;
}

//**** PRESS STATE OF GETKEYSTATE TESTING

//************************************************************************
/*++
 
performPressKeyStateTEest:
 
	Check the GetKeyState API for proper pressed state of keys
	Make user keybd_event generates a message for each valid key
 
--*/

void performPressKeyStateTest() {
	 g_pKato->Log( LOG_DETAIL, TEXT("Starting press key state test\n")); 

	 // Create the key driver thread
	 HANDLE hThread = NULL;
	 DWORD  dRtn;
	 hThread = CreateThread(NULL, 0, 
		 (LPTHREAD_START_ROUTINE)generateKeyMessagesThread, NULL, 0, &dRtn);
	 if (NULL == hThread) {
		 g_pKato->Log( LOG_DETAIL, 
			 TEXT("Could not create key state test thread! Aborted.\n"));
		 return;
	 }

	 MSG         msg ;
     while (GetMessage (&msg, NULL, 0, 0))
          {
          TranslateMessage (&msg) ;
          DispatchMessage (&msg) ;
          }

	 // give thread a chance to exit
	 WaitForSingleObject(hThread, 500);

	 // untoggle anything that was turned on by this 
	 pressAllKeys();

	 g_pKato->Log( LOG_DETAIL, TEXT("Press Key State Test finished\n"));

	 CloseHandle(hThread);
}
//************************************************************************
/*++
 
generateKeyMessagesThread:
 
	Generates key messages that support GetKeyState, handling synchronization
	with the window.
	After running tests, sends WM_DESTROY to global window
 
--*/
void generateKeyMessagesThread() {
	BOOL bSuccess = TRUE;

	// lets do them all and make sure it doesn't crash
	for(int x = 0; x < 256; x ++) {
#ifdef x86
		if (x != 223) {   //CEPC 
			pressKeyAndWait(g_hwndTestWindow, x);
		}
#else
		pressKeyAndWait(g_hwndTestWindow, x);
#endif
	}

	PostMessage(g_hwndTestWindow, WM_DESTROY, 0, 0);
	ExitThread(0);
}

//************************************************************************
/*++
 
pressKeyAndWait:
 
	Generate a key stroke for a virtual key, handline synch.
 
Arguments:
 
	HWND Hwnd           :  Handle to test window
    BYTE byVirtualKey   :  Virtual Key to press

Returns:  Success of operation
--*/

BOOL pressKeyAndWait(HWND hwnd, BYTE byVirtualKey) {
	TCHAR szBuffer[50];
	BOOL  bSuccess = TRUE;
	
	keybd_event(byVirtualKey , 0, 0, 0 );
	DWORD  dwRtn     = WaitForSingleObject(g_hReceivedKeyDown, 5000);
	if (WAIT_OBJECT_0 != dwRtn) {
		_stprintf(szBuffer, TEXT("*** FAILURE: Key down %d was not recieved\n"), byVirtualKey);
		g_pKato->Log( LOG_FAIL, szBuffer);
		bSuccess = FALSE;
	}

	if (!ResetEvent(g_hReceivedKeyDown))
		bSuccess =  FALSE;

	keybd_event(byVirtualKey , 0, KEYEVENTF_KEYUP, 0 );
	dwRtn              = WaitForSingleObject(g_hReceivedKeyUp, 5000);
	if (WAIT_OBJECT_0 != dwRtn) {
		_stprintf(szBuffer, TEXT("*** FAILURE: Key up %d was not recieved\n"), byVirtualKey);
		g_pKato->Log( LOG_FAIL, szBuffer);
		bSuccess = FALSE;
	}

	if (!ResetEvent(g_hReceivedKeyUp))
		bSuccess = FALSE;

	return bSuccess;
}
	
//************************************************************************
/*++
 
checkPressStateOfKey:
 
	Check is virtual key is pressed (if bPressed = TRUE).  
	Otherwise check that it has been released.
	Log failure if key is in wrong state.
 
Arguments:
 
    BYTE     byVKey   :  Virtual Key to check
	BOOLEAN  bPressed :  Expected State of Key (TRUE = Pressed)

Returns:  TRUE if the key was in expected state, otherwise FALSE
--*/
BOOL checkPressStateOfKey(BYTE byVKey, BOOL bPressed) {
	//  CE only supports GetKeyState on a set of virtual keys
	if (isPressKeySupported(byVKey)) {
		TCHAR szBuffer[100];
		SHORT sStatus = GetKeyState(byVKey);
		BOOL bPressStateOfKey = (0x8000 & sStatus)?TRUE:FALSE;
		if (bPressed == bPressStateOfKey) {
		
			_stprintf(szBuffer, TEXT("Key %d was %s as expected\n"), byVKey,
				bPressStateOfKey? TEXT("pressed"):TEXT("not pressed"));
			g_pKato->Log( LOG_DETAIL, szBuffer);
			return TRUE;
		}
		else {
			_stprintf(szBuffer, 
				TEXT("***FAILURE Key %d was unexpectedly %s.  Error:%d\n"), 
				byVKey, bPressStateOfKey? TEXT("pressed"):TEXT("not pressed"),
				GetLastError());
			g_pKato->Log( LOG_FAIL, szBuffer);
			return FALSE;
		}		
	}
	// unsuported key
	return TRUE;
}
//************************************************************************
/*++
 
isPressKeySupported:
 
	Check is virtual key is supported by CE for GetKeyState pressed state
 
Arguments:
 
    BYTE byVKey   :  Virtual Key to check out

Returns:  TRUE if the key is supported
--*/


BOOL isPressKeySupported(BYTE byVirtualKey) {
	for (int x = 0; x < countof(g_pbyPressStateSupportedVKs); x++) {
		if (byVirtualKey == g_pbyPressStateSupportedVKs[x])
			return TRUE;
	}
	return FALSE;
}

//**************************************************************************
void pressAllKeys() {

	// press and hold all the keys
	int byVirtualKey = 0;
	for (byVirtualKey = 0; byVirtualKey < 256; byVirtualKey++) {
#ifdef x86
		if (byVirtualKey != 223)   //CEPC 
			keybd_event( byVirtualKey, 0, 0, 0 );
#else
		keybd_event( byVirtualKey, 0, 0, 0 );
#endif
	}  //for

	// release them
   for (byVirtualKey = 0; byVirtualKey < 256; byVirtualKey++) {
#ifdef x86
		if (byVirtualKey != 223)   //CEPC 
			keybd_event( byVirtualKey, 0, KEYEVENTF_KEYUP, 0 );
#else
			keybd_event( byVirtualKey, 0, KEYEVENTF_KEYUP, 0 );
#endif
   }  //for
}