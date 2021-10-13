/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     chording.cpp  

Abstract:

  The chording test.


Notes:
	
  Adapted from the multitst suite "keys2.cpp"
--*/

#include "global.h"
#include "keyspt.h"

static const TCHAR cszThisTest[] = TEXT("Chording Test");

static BOOL g_fLastChordFailed = FALSE;

struct KeyChordItem {
    TCHAR   *str;
    WORD    wKeyCode;
    BOOL    fCtrl;
    BOOL    fAlt;
    BOOL    fShift;
};

static KeyChordItem aKeyChordList[] = {
// fails on ODO    TEXT("CTRL-ALT-SHIFT-<RIGHT>")  , VK_RIGHT,     1, 1, 1,
    TEXT("CTRL-SHIFT-<RIGHT>")      , VK_RIGHT,     1, 0, 1,

    TEXT("CTRL-ALT-SHIFT-T")        , (WORD)'T',    1, 1, 1,
    TEXT("CTRL-ALT-SHIFT-M")        , (WORD)'M',    1, 1, 1,
    TEXT("CTRL-ALT-SHIFT-<Delete>") , VK_DELETE,    1, 1, 1,
// fails on ODO      TEXT("CTRL-ALT-SHIFT-<UP>")     , VK_UP,        1, 1, 1,

    TEXT("CTRL-ALT-P")              , (WORD)'P',    1, 1, 0,
    TEXT("CTRL-ALT-X")              , (WORD)'X',    1, 1, 0,
// fails on ODO      TEXT("CTRL-ALT-<RIGHT>")        , VK_RIGHT,     1, 1, 0,
    TEXT("CTRL-ALT-SPACE")          , VK_SPACE,     1, 1, 0,
    
    TEXT("CTRL-SHIFT-A")            , (WORD)'A',    1, 0, 1,

    TEXT("CTRL-SHIFT-<LEFT>")       , VK_LEFT,      1, 0, 1,
    TEXT("CTRL-SHIFT-<DOWN>")       , VK_DOWN,      1, 0, 1,    
    
    TEXT("ALT-SHIFT-9")             , (WORD)'9',    0, 1, 1,
    TEXT("ALT-SHIFT-3")             , (WORD)'3',    0, 1, 1,
// fails on ODO      TEXT("ALT-SHIFT-<DOWN>")        , VK_DOWN,      0, 1, 1,
// fails on ODO      TEXT("ALT-SHIFT-<LEFT>")        , VK_LEFT,      0, 1, 1,
    NULL                            , 0,            0, 0, 0,
};



/*++
 
KeyChording_T:
 
	This test tests 3, and 4 key combinations.
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
--*/
TESTPROCAPI KeyChording_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

#ifndef UNDER_CE
	return TPR_SKIP;			// some key codes don't match NT or Win9x
#endif
	

    TCHAR szBuffer[512];
    int   done = 0;
    int   iIdx = 0;
    BOOL  fCapsLock = GetKeyState( VK_CAPITAL );
    BOOL  fDone = FALSE;
    

    //
    // The test is run twice, the first time CapsLock must be off, the
    // second time CapsLock must be on
    //
    while( fCapsLock )
    {
        MessageBox(NULL,
            TEXT("Please turn CapsLock keyboard light off, then press OK."),
            TEXT("Toggle Caps Lock"),
            MB_OK );
        fCapsLock = GetKeyState( VK_CAPITAL );            
    }            
  
    SendMessage( globalHwnd, WM_SWITCHPROC, KEYCHORDING, 0);
	clean();

    while( !fDone )
    {
        if( fCapsLock )
        {           
            g_pKato->Log( LOG_DETAIL, "Testing with CapsLock on" );
        }
        else
        {
            g_pKato->Log( LOG_DETAIL, "Testing with CapsLock off" );
        }
        
        while( aKeyChordList[iIdx].str )
        {
            while( GetKeyState( VK_CAPITAL ) != fCapsLock )
            {
                MessageBox(NULL,
                    TEXT("Please toggle CapsLock key, then press OK."),
                    TEXT("Toggle Caps Lock"),
                    MB_OK );
            }
            
            SendMessage( globalHwnd, WM_INIT, 0, (LPARAM)(&aKeyChordList[iIdx]) );
            wsprintf( szBuffer,
                      TEXT("Press the <%s> keys.                           "),
                      aKeyChordList[iIdx].str);   
            recordInstruction(szBuffer);
            g_pKato->Log( LOG_DETAIL, szBuffer );
            doUpdate(NULL);
            done = pump();

    		clean();
            if( done == 2 ) break;
            
            if( g_fLastChordFailed )
            {
                wsprintf( szBuffer,
                          TEXT("If you didn't enter %s, would you like to retry?"), 
                          aKeyChordList[iIdx].str );
                int iRtn = MessageBox( globalHwnd, 
                                       szBuffer,
                                       TEXT("Key Chording Error"),
                                       MB_YESNO | MB_ICONQUESTION );
                if( IDYES == iRtn )
                {
                    g_pKato->Log( LOG_DETAIL, 
                                  TEXT("In %s: Retrying %s"), 
                                  cszThisTest, aKeyChordList[iIdx].str );
                    continue;
                }
    			else {
    				g_pKato->Log( LOG_FAIL, 
                                  TEXT("In %s: Invalid key %s"), 
                                  cszThisTest, aKeyChordList[iIdx].str );
                }
            } // end if( g_fLastChardFailed ) 
                    
            iIdx++;
                
        }  // end for;

        if( fCapsLock ) 
        {
            fDone = TRUE;
        }

        else
        {
            iIdx = 0;
            while( !fCapsLock )
            {
                MessageBox(NULL,
                    TEXT("Please turn CapsLock keyboard light on, then press OK."),
                    TEXT("Toggle Caps Lock"),
                    MB_OK );
                fCapsLock = GetKeyState( VK_CAPITAL ); 
            }
        }           
    }        

    return getCode();
    	
} // end KeyChording_T( ... ) 


/*++
 
KeyChordingWinProc:
 
	This is the windows proceedure for the Key Chording tests.
 
Arguments:
 
	
 
Return Value:
 
	
 

 
	Unknown (unknown)
 
Notes:
 
--*/
LRESULT CALLBACK KeyChordingWinProc( HWND hwnd, 
                                     UINT message,
                                     WPARAM wParam, 
                                     LPARAM lParam) 
{
    static int goal;
    static KeyChordItem  *pKeyChord = NULL;
    PAINTSTRUCT ps;
	HDC hdc;

    switch(message) 
    {
    case WM_PAINT:
        BeginPaint(hwnd, &ps);

        initWindow( ps.hdc,
                    TEXT("Driver Tests: Keyboard: Key Chording"),
                    TEXT("Follow direction in the bottom box.\r\nPress <X-Y-Z> means press <X, Y, and Z simultaneously>."));
        showInstruction(ps.hdc);
        EndPaint(hwnd, &ps);
        return 0;
      
    case WM_INIT:
        pKeyChord = (KeyChordItem *)lParam;
        goal = 0;
        g_fLastChordFailed = FALSE;        
        emptyKeyAndInstrBuffers();
        return 0;

    // get everthing by keyups   
    case WM_CHAR:
        // FALL THROUGH
 
    case WM_DEADCHAR:
        // FALL THROUGH
	case WM_SYSDEADCHAR:
        // FALL THROUGH
    case WM_SYSCHAR:
        // FALL THROUGH
        hdc = GetDC(hwnd);
        recordKey(message, wParam, lParam);
        showKeyEvent(hdc);
		ReleaseDC(hwnd, hdc);
		return 0;      

    case WM_KEYDOWN:
        // FALL THROUGH
    case WM_SYSKEYDOWN:  
		hdc = GetDC(hwnd);
        recordKey(message, wParam, lParam);
		showKeyEvent(hdc);
		ReleaseDC(hwnd, hdc);
    
        if( wParam == pKeyChord->wKeyCode )
		{
              // Modified by rajendran to trap the control keys while reading a char
			  // Following variables are for detecting the expected and actual key state 
			  // for Shift, Control and ALT keys.
		  bool sft,ctl,altk;
	      bool e_sft,e_ctl,e_alt;
		  if ((pKeyChord->fAlt)) e_alt=TRUE; else e_alt=FALSE; 
		  if ((pKeyChord->fCtrl)) e_ctl=TRUE; else e_ctl=FALSE ; 
		  if ((pKeyChord->fShift)) e_sft = TRUE; else e_sft = FALSE; 

          if ((GetKeyState(VK_SHIFT))) sft =TRUE; else sft =FALSE; ;
		  if ((GetKeyState(VK_CONTROL))) ctl = TRUE; else ctl = FALSE; 
		  if ((GetKeyState(VK_MENU))) altk= TRUE; else altk= FALSE ;


		  // Trap the error and display ------------ a-rajtha
		  if ( (sft != e_sft) || (altk != e_alt ) || (ctl != e_ctl) ){
  			   
			   if (sft !=e_sft && sft ==0) { 
				   g_pKato->Log(LOG_DETAIL, TEXT("SHIFT IS EXPECTED"));
			   }
			   else { if (sft !=e_sft && sft ==1) g_pKato->Log(LOG_DETAIL, TEXT("SHIFT KEY NOT EXPECTED"));}
  
			   if (ctl != e_ctl && ctl==0) {
				   g_pKato->Log(LOG_DETAIL, TEXT("Control KEY EXPECTED"));
			   } else {if (ctl != e_ctl && ctl ==1) g_pKato->Log(LOG_DETAIL, TEXT("CONTROL KEY NOT EXPECTED"));}

			   if (altk != e_alt && altk==0) {
				   g_pKato->Log(LOG_DETAIL, TEXT("ALT KEY EXPECTED"));
			   } else {if (altk != e_alt && altk==1) g_pKato->Log(LOG_DETAIL, TEXT("ALT KEY NOT EXPECTED"));}

			   g_fLastChordFailed = TRUE;
		  }
		  else  
		  {
			  g_pKato->Log( LOG_DETAIL,TEXT("KEYS PRESSED Properly")); }

		  
  
		
		/* ------------------------------------------------------------
            	ERROR states.
            ------------------------------------------------------------ 
            if( GetKeyState( VK_CONTROL ) < 0  && !pKeyChord->fCtrl )
            {
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_CONTROL unexpected." ), 
                              cszThisTest);
                g_fLastChordFailed = TRUE;
            }
            if( pKeyChord->fCtrl && GetKeyState( VK_CONTROL ) > 0)
            {
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_CONTROL expected." ), 
                              cszThisTest);
                g_fLastChordFailed = TRUE;
            }
            
            if( GetKeyState( VK_MENU ) < 0 && !pKeyChord->fAlt )
            {
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_MENU unexpected." ), 
                              cszThisTest);
                g_fLastChordFailed = TRUE;
            }
            if( pKeyChord->fAlt && GetKeyState( VK_MENU ) > 0)
            {
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_MENU expected." ), 
                              cszThisTest);
                g_fLastChordFailed = TRUE;
            }
            
            if( GetKeyState( VK_SHIFT ) < 0 && !pKeyChord->fShift )
            {
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_SHIFT unexpected." ), 
                              cszThisTest);
                g_fLastChordFailed = TRUE;
            }
            if( pKeyChord->fShift && GetKeyState( VK_SHIFT ) > 0)
                g_pKato->Log( LOG_WARN, 
                              TEXT("WARNING in %s: VK_SHIFT expected."), 
                              cszThisTest);
            */
            goal = 1;
            
        } // end if( wParam == pKeyChord->wKeyCode )
        
        // skip wParam == 0 and VK_MENU when useing alt+<arrow key>            
        else if( (wParam == 0 || wParam == VK_MENU) && 
                 (VK_PRIOR == pKeyChord->wKeyCode ||
                  VK_END   == pKeyChord->wKeyCode ||
                  VK_NEXT  == pKeyChord->wKeyCode ||
                  VK_HOME  == pKeyChord->wKeyCode) )
            return 0;                                    
        else if( wParam == VK_CONTROL && pKeyChord->fCtrl )
            return 0;
        else if( wParam == VK_MENU    && pKeyChord->fAlt )
            return 0;
        else if( wParam == VK_SHIFT   && pKeyChord->fShift )
            return 0;
        else
        {
            // ERROR CONDITION
            g_pKato->Log( LOG_WARN, 
                          TEXT("WARNING in %s: Expected key 0x%02X got 0x%02X"),
                          cszThisTest, pKeyChord->wKeyCode, wParam);
            goal = 1;
            g_fLastChordFailed = TRUE;

        } // end if( wParam ... ) else
        
        return 0;       
      
   case WM_GOAL:
      return goal;
      
   } // end switch( message )
   
   return (DefWindowProc(hwnd, message, wParam, lParam));
   
} // end KeyChordingWinProc



