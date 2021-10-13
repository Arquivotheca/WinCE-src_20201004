/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keypress.cpp  

Abstract:

  The Key Press test.


Notes:
	
  Adapted from the multitst suite "keys.cpp"
--*/

#include "keyspt.h"


LRESULT CALLBACK KeyPressWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   static int  keyPress, goal;
   PAINTSTRUCT ps;
   HDC hdc;

   switch(message) {
   case WM_PAINT:
      BeginPaint(hwnd, &ps);
	  initWindow(ps.hdc, TEXT("Driver Tests: Keyboard: Manual Tests"),
		  TEXT("Type on the keyboard (avoid sys keys i.e. CTRL-ESC). \r\nPress <SPACE> to continue."));
      drawKeyEventHeader(ps.hdc); 
	  EndPaint(hwnd, &ps);
      return 0;
   case WM_INIT:
	  keyPress = 0;
      goal     = (int)wParam;  // stopping charcter
      emptyKeyAndInstrBuffers();
      return 0;
   case WM_KEYDOWN:
//	   return (keyPress == goal);
//	    break;
   case WM_KEYUP:
   case WM_SYSKEYDOWN:  
   case WM_SYSKEYUP:
   case WM_CHAR:
   case WM_DEADCHAR:
   case WM_SYSCHAR:
   case WM_SYSDEADCHAR:
	  keyPress = (int)wParam;
	  hdc = GetDC(hwnd);
      recordKey(message, wParam, lParam);
	  showKeyEvent(hdc);
	  ReleaseDC(hwnd, hdc);
	  return 0;
   case WM_GOAL:
      return (keyPress == goal);
   }
   return (DefWindowProc(hwnd, message, wParam, lParam));
}


//***********************************************************************************
void ManualDepressTest(void) {
   int   done = 0;

   clean();
   SendMessage(globalHwnd,WM_SWITCHPROC,KEYPRESS,0);
   do {
      SendMessage(globalHwnd,WM_INIT,goKey,0);
      doUpdate(NULL);
      if (pump() == 2)
         done = 1;
      else if (askMessage(TEXT("Did you see the correct characters?"),0)) 
         done = 1;
      else if (!askMessage(TEXT("Do you want to try again?"),1))
         done = 1;
   } while (!done);
}

//***********************************************************************************
//  Entry point for Manual Key Press Test
TESTPROCAPI ManualDepress_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
   ManualDepressTest();
   return getCode();
}