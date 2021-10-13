/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     edittest.cpp  

Abstract:

  The edit test.


Notes:
	
  Adapted from the multitst suite "keys2.cpp"
--*/

#include "global.h"

static const TCHAR cszThisTest[] = TEXT("Edit Test");
static HWND  hEdit = NULL;

/*++
 
EditTextWndProc:
 
	This is the windows procedure for the EditText test.
 
Arguments:
 
    Standard windows procedure arguments.	
 
Return Value:
 
	Standard windows procedure returns.
--*/
LRESULT CALLBACK EditTextWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) 
{
   static int   goal = 0;
   PAINTSTRUCT ps;
   RECT rectWorking;

   switch(message) 
   {
   case WM_PAINT:
        BeginPaint(hwnd, &ps);
        initWindow(ps.hdc,
                   TEXT("Driver Tests: Edit Text"),
                   TEXT("Edit text in the block below when done, TAP this box.\nBe sure to test Shift lock, <Shift> + <Caps Shift>") );
		EndPaint(hwnd, &ps);
        return 0;

   case WM_INIT:
        goal     = (int)wParam;  
        SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
        hEdit = CreateWindow(TEXT("edit"),
                             NULL,
                             WS_VISIBLE | WS_CHILD |
                             ES_MULTILINE | ES_LEFT,
                             7, 97, rectWorking.right - 14, 
                             rectWorking.bottom - 97 -7,
                             globalHwnd, (HMENU)1,
                             globalInst, NULL );
        if( NULL == hEdit )
        {
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d: Couldn't create Edit Control: %d"), 
                          cszThisTest, __LINE__, GetLastError() );
            return 1;                                    
        }

        SetFocus( hEdit );
        
        return 0;

    case WM_LBUTTONUP:
        goal = 1;
        return 0;
    
    case WM_GOAL:
        if( goal )
            if( NULL != hEdit )
            {
                DestroyWindow( hEdit );
                hEdit = NULL;

            } // end if( NULL != hEdit )
            
        return goal;
        
   } // end switch(message)
   
   return (DefWindowProc(hwnd, message, wParam, lParam));
   
} // end LRESULT CALLBACK EditTextWndProc(HWND, UINT,WPARAM, LPARAM)


/*++
 
EditText_T:
 
	This test test nomal text editing features.
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Unknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI EditText_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
	
    /* -------------------------------------------------------------------
     	The real work.
    ------------------------------------------------------------------- */
    BOOL bDone = FALSE;

    g_pKato->Log(LOG_DETAIL, TEXT("In EditText_T") );
    
    SendMessage( globalHwnd, WM_SWITCHPROC, EDITTEXT, 0);
 //   int tm1 ;
//	 tm1 = GetKeyboardType (int 0);
	 
    do
    {
        SendMessage( globalHwnd, WM_INIT, 0, 0);
        doUpdate(NULL);

    	g_pKato->Log(LOG_DETAIL, TEXT("Pumping EditText_T") );
    	
        if (pump(hEdit) == 2)
            bDone = TRUE;
        else if (askMessage(TEXT("Did editing work properly?"),0)) 
            bDone = TRUE;
        else if (!askMessage(TEXT("Do you want to try again?"),1))
            bDone = TRUE;

    } while( !bDone );

    g_pKato->Log(LOG_DETAIL, TEXT("Done With EditText_T") );

    return getCode();
    	
} // end EditText_T( ... ) 
