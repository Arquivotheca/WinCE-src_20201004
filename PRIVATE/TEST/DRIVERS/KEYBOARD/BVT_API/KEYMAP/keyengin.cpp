/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keyengin.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	Select.cpp
 
Abstract:

	This file contains the source code for the Runtime TUX Test slection
	library.
  


	Uknown (unknown)
 
Notes:

   
--*/
#include <windows.h>
#include <tux.h>
#include <katoex.h>
#include "ErrMacro.h"
#include <tchar.h>

#define WM_LAST_CHAR   (WM_USER + 1)
#define TEST_CHAR_EVENT      0
/* #define  WM_INIT        7772
#define  WM_GOAL        7771
#define  zy  GetSystemMetrics(SM_CYSCREEN)
#define  zx  GetSystemMetrics(SM_CXSCREEN)
#define  WM_SWITCHPROC  7773
int       goKey = 32;   // Space

  */

HANDLE             g_hThread  = NULL;

#ifdef _DEBUG
static TCHAR szDebug[1024];
#endif

static const LPTSTR cszThisFile   =     TEXT("KeyEngin.cpp");

/* ------------------------------------------------------------------------
	The struct for creating a thread.
------------------------------------------------------------------------ */
typedef struct KEY_MAP_TEST_PARAMtag
{
    CKato       *pKato;
    HINSTANCE   hInstance;
    HANDLE      hInitEvent;
    HWND        hWnd;

} KEY_MAP_TEST_PARAM, *PKEY_MAP_TEST_PARAM;


/*++
 
KeyMapTestWndProc:
 
	This function is the windows proceedure for the Keyboard mapping test.
	This WndProc remembers the last WM_CHAR, WM_KEYDOWN and WM_SYSKEYDOWN
	received.  When the window receive one of the custom messages 
	WM_LAST_CHAR or WM_LAST_KEY it returns the last char or key respectively.
 
Arguments:
 
	Standard Windows Procedure Aguments
 
Return Value:
 
	Standard Windows Procedure returns
 


    Uknown (unknown) 
Notes:
 
--*/
LRESULT CALLBACK KeyMapTestWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
	HDC 			hdc;
	PAINTSTRUCT		ps;
	RECT			rect;
    static TCHAR    cLastChar = 0;
    static CKato    *pKato    = NULL;
    static int		iMsg	  = 0;
    static LPTSTR	aszMsg[]  = { TEXT("Look in Log for test results"),
		                          TEXT("keymap -x50 for US-Keyboard -x60 for Japanese "),
    						      TEXT("Test complete") };
    
    switch(message) 
    {

    case WM_CREATE:
        /* ----------------------------------------------------------------
            The pointer to kato is passed in as the lpCreateParams member
            of the CREATESTRUCT pointed to by lParam.
        ---------------------------------------------------------------- */
        pKato = (CKato *)((LPCREATESTRUCT)lParam)->lpCreateParams;

        __try {

        pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s Created"),
                    cszThisFile, __LINE__, ((LPCREATESTRUCT)lParam)->lpszName );

        } __except( (GetExceptionCode() == STATUS_ACCESS_VIOLATION) ?
                     EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

        SetLastError( ERROR_INVALID_DATA );                        
        return -1;

        } // end __try-__except

        return 0;

    case WM_PAINT: 
    	hdc = BeginPaint( hwnd, &ps );
    	GetClientRect( hwnd, &rect );
		DrawText( hdc, aszMsg[iMsg], -1, &rect, 
			      DT_CENTER );	// | DT_VCENTER );    	
		DrawText( hdc, aszMsg[iMsg+1], -1, &rect, 
			      DT_CENTER | DT_VCENTER );    	

		EndPaint( hwnd, &ps);
		return 0;
        
    case WM_CHAR:

        cLastChar = (TCHAR) wParam;
        pKato->Log( LOG_DETAIL, 
                    TEXT("In %s @ line %d: Test window received character \"%c\""), 
                    cszThisFile, __LINE__, cLastChar );
        SetEvent( (HANDLE)GetWindowLong( hwnd, TEST_CHAR_EVENT ) );
        
        return 0;
        
    case WM_LAST_CHAR:

        return cLastChar;

    case WM_DESTROY:

		iMsg = 2;
		InvalidateRect( hwnd, NULL, TRUE );
		UpdateWindow( hwnd );
		
        pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Test window received WM_DESTROY"),
                    cszThisFile, __LINE__ );
        PostQuitMessage( 0 );
        return 0;
     
    } // end switch(message)
    
    return DefWindowProc(hwnd, message, wParam, lParam);
    
} // end LRESULT CALLBACK SelectWindProc(HWND , UINT ,WPARAM, LPARAM)


/*++
 
KeyMapTestThread:
 
	This function provides the test window that use used to determine if
	what characters or keys have been pressed. The Thread creates and
	window and then pumps its messages until the window is closed. 
 
Arguments:
 
	pKeyMapTestParams   a struct that has the parameter for creating the
	                    window and returning the handle to the parent
	                    thread
 
Return Value:
 
    The return value of the WndProc.	
 

 
	Uknown (unknown)
 
Notes:
 
--*/
DWORD KeyMapTestThread( PKEY_MAP_TEST_PARAM pKeyMapTestParams )
{

    HWND hWnd;
    WNDCLASS  wc;
    static TCHAR    szAppName[]     = TEXT("Key Map Test Window");
    static TCHAR    szClassName[]   = TEXT("KeyMapTestWindow");
    MSG msg;
    HINSTANCE       hInstance;
    HANDLE          hCharEvent;

    // Set local variables
    hInstance = pKeyMapTestParams->hInstance;

    /* --------------------------------------------------------------------
    	The following event is used by the window thread to signal the
    	test thread when a WM_CHAR is recieved.
    -------------------------------------------------------------------- */
    hCharEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    FUNCTION_ERROR( pKeyMapTestParams->pKato, NULL == hCharEvent, return NULL );
    
    /* --------------------------------------------------------------------
    	All the user to set the window name app name.
    -------------------------------------------------------------------- */
    wc.style          = 0L;
    wc.lpfnWndProc    = (WNDPROC)KeyMapTestWndProc;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(DWORD);
    wc.hInstance      = hInstance;
	wc.hIcon          = NULL;
    wc.hCursor        = NULL;    
    wc.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName   = NULL;
    wc.lpszClassName  = szClassName;
   
    if( !RegisterClass(&wc) )
    {
        ExitThread( GetLastError() );
        
    } // end if( !RegisterClass(&wc) )
    
    hWnd = CreateWindow(szClassName,
                        szAppName,
                        WS_OVERLAPPED,
                        0,
                        0,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        NULL, NULL,
                        hInstance,
                        (LPVOID)(pKeyMapTestParams->pKato));

    pKeyMapTestParams->pKato->Log( LOG_DETAIL,
                                   TEXT("In %s @ line %d: hWnd == %08Xh"),
                                   cszThisFile, __LINE__, hWnd );

    pKeyMapTestParams->hWnd = hWnd;
    SetEvent( pKeyMapTestParams->hInitEvent );

    if( NULL == hWnd )  
        ExitThread( GetLastError() );

    SetWindowLong( hWnd, TEST_CHAR_EVENT, (long)hCharEvent );
    
    ShowWindow( hWnd, SW_SHOW );
    
    while( GetMessage(&msg,NULL,0,0) )
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
    } // end while( GetMessage(&msg,NULL,0,0) )

    UnregisterClass( szClassName, hInstance );
    CloseHandle( hCharEvent );
    
    /* --------------------------------------------------------------------
    	If not zero then erro
    -------------------------------------------------------------------- */
    ExitThread( msg.wParam );
    // Return is never reached
    return msg.wParam;

}  // end LPFUNCTION_TABLE_ENTRY TuxSelectTests( LPFUNCTION_TABLE_ENTRY lpFTE )


/*++
 
CreateKeyMapTestWindow:
 
    This function creates the KeyMapTestThread that creates the Key Map Test
    Window that receives the key map test keys.
 
Arguments:
 
	pKato   pointer to the kato object to use or NULL if so specified.
 
Return Value:
 
	HWND    handle to the window created.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
HWND CreateKeyMapTestWindow( HINSTANCE hInstance, CKato *pKato )
{
    KEY_MAP_TEST_PARAM KeyMapTestParams;    
    DWORD              dwRtn;

    KeyMapTestParams.hInstance  = hInstance;
    KeyMapTestParams.pKato      = pKato;
    KeyMapTestParams.hWnd       = NULL;
    
    KeyMapTestParams.hInitEvent = CreateEvent( NULL, TRUE, FALSE, NULL ); 
    if( NULL == KeyMapTestParams.hInitEvent ) return NULL;

    __try {
    
    g_hThread = CreateThread( NULL, 0,
                            (LPTHREAD_START_ROUTINE)KeyMapTestThread,
                            (LPVOID)&KeyMapTestParams, 0,
                            &dwRtn ); 
    if( NULL == g_hThread ) __leave;

    // Wait for window to be created
    dwRtn = WaitForSingleObject( KeyMapTestParams.hInitEvent, 60000 );
    if( WAIT_TIMEOUT == dwRtn ) 
    {
        SetLastError( ERROR_GEN_FAILURE );
        __leave;
        
    } // end if( WAIT_TIMEOUT == dwRtn ) 

    // Insure that window creation didn't fail
    if( FALSE ==  GetExitCodeThread( g_hThread, &dwRtn ) ) __leave;
    
    if( STILL_ACTIVE != dwRtn )
    {
        SetLastError( dwRtn );
        __leave;

    } // end if( STILL_ACTIVE != dwRtn )

    } __finally {

    CloseHandle( KeyMapTestParams.hInitEvent );    

    } // end __try-__finally    

    return KeyMapTestParams.hWnd;

} // end HWND CreateKeyMapTestWindow( CKato *pKato )


/*++
 
RemoveTestWindow:
 
	This function sends a WM_CLOSE message to the test window.
 
Arguments:
 
	hWnd    the test window
 
Return Value:
 
    result returned by SendMessage
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#define THREAD_WAIT_TIME        60000
DWORD RemoveTestWindow( HWND hWnd )
{
    DWORD   dwResult; 
    
    dwResult = SendMessage( hWnd, WM_CLOSE, 0, 0 ); 

    
    if(WAIT_OBJECT_0 == WaitForSingleObject(g_hThread, THREAD_WAIT_TIME))
        CloseHandle( g_hThread );

    return dwResult;

} // end DWORD RemoveTestWindow( HWND hWnd )


/*++
 
TestKeyMapChar:
 
	This function performs a keymap test for a character.
 
Arguments:

    pKato   Kato object to log too
	hWnd    Handle of the window to receive the character
	pKeys   Pointer to a list of keys to type
	nKeys   Number of keys to type
	cExpect The character to expect
 
Return Value:
 
	TRUE if pass, else FALSE
 

 
	Uknown (unknown)
 
Notes:
 
--*/
BOOL TestKeyMapChar( CKato *pKato, HWND hWnd, LPBYTE pKeys, DWORD nKeys, const TCHAR cExpect )
{
    DWORD   dwRtn;
    TCHAR   cResult = 0;
    INT     iIdx;
    HANDLE  hCharEvent;

    __try {

    hCharEvent = (HANDLE)GetWindowLong( hWnd, TEST_CHAR_EVENT );

    for( iIdx = 0; iIdx < (INT)nKeys; iIdx++ )
        keybd_event( pKeys[iIdx], NULL, 0, 0 ); 

    dwRtn = WaitForSingleObject( hCharEvent, 5000 );
    if( WAIT_OBJECT_0 != dwRtn )
    {
        pKato->Log( LOG_FAIL, 
                    TEXT("FAIL in %s @ line %d: Didn't receive Character read event"),
                    cszThisFile, __LINE__ );

    } // end if( WAIT_OBJECT_0 != dwRtn )
    else
    {
        ResetEvent( hCharEvent );
        
    } // end if( WAIT_OBJECT_0 != dwRtn ) else
    
    cResult = (TCHAR)SendMessage( hWnd, WM_LAST_CHAR, 0, 0 );

    for( --iIdx; iIdx >= 0; iIdx-- )
        keybd_event( pKeys[iIdx], NULL, KEYEVENTF_KEYUP, 0 );
    
    if( cExpect == cResult )
    {
        return TRUE;
        
    } // end if( cExpect == cResult )

    else
    {
        pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  Window expected \"%c\" received \"%c\""),
                    cszThisFile, __LINE__, cExpect, cResult );
        return FALSE;
        
    } // end if( cExpect == cResult )

    } __except( (GetExceptionCode() == STATUS_ACCESS_VIOLATION) ?
                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

//    } __except( (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ?
//                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

    SetLastError( ERROR_INVALID_DATA );
    return FALSE;
    
    } // end __try-__except

    return FALSE;
    
} // end BOOL TestKeyMapChar( CKato *, HWND , LPBYTE , DWORD , const TCHAR  )


// Orig code for testkeymap ---  
//    for US keyboard **********************************

BOOL USTestKeyMapChar( CKato *pKato, HWND hWnd, LPBYTE pKeys, DWORD nKeys, const TCHAR cExpect )
{
    DWORD   dwRtn;
    TCHAR   cResult = 0;
    INT     iIdx;
    HANDLE  hCharEvent;

    __try {

    hCharEvent = (HANDLE)GetWindowLong( hWnd, TEST_CHAR_EVENT );

    for( iIdx = 0; iIdx < (INT)nKeys; iIdx++ )
        keybd_event( pKeys[iIdx], NULL, 0, 0 ); 

    dwRtn = WaitForSingleObject( hCharEvent, 5000 );
    if( WAIT_OBJECT_0 != dwRtn )
    {
        pKato->Log( LOG_FAIL, 
                    TEXT("FAIL in %s @ line %d: Didn't receive Character read event"),
                    cszThisFile, __LINE__ );

    } // end if( WAIT_OBJECT_0 != dwRtn )
    else
    {
        ResetEvent( hCharEvent );
        
    } // end if( WAIT_OBJECT_0 != dwRtn ) else
    
    cResult = (TCHAR)SendMessage( hWnd, WM_LAST_CHAR, 0, 0 );

    for( --iIdx; iIdx >= 0; iIdx-- )
        keybd_event( pKeys[iIdx], NULL, KEYEVENTF_KEYUP, 0 );
    
    if( cExpect == cResult )
    {
        return TRUE;
        
    } // end if( cExpect == cResult )

    else
    {
        pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  Window expected \"%c\" received \"%c\""),
                    cszThisFile, __LINE__, cExpect, cResult );
        return FALSE;
        
    } // end if( cExpect == cResult )

    } __except( (GetExceptionCode() == STATUS_ACCESS_VIOLATION) ?
                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

//    } __except( (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ?
//                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

    SetLastError( ERROR_INVALID_DATA );
    return FALSE;
    
    } // end __try-__except

    return FALSE;
    
} // end BOOL TestKeyMapChar( CKato *, HWND , LPBYTE , DWORD , const TCHAR  )

 


