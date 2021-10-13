/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keyspt.cpp  

Abstract:

  Support functions for the Key Press and Key Sequence tests.


Notes:
	
  Adapted from the multitst suite "keys.cpp"
--*/

#include "keyspt.h"


/***********************************************************************************
***
***   Globals
***
***********************************************************************************/

//These two variables are used together (i.e. must be consistent)
TCHAR keyBuffer[9][512];
BOOL keyBufferIsChar;

TCHAR instructBuffer[512];


struct chList {
   TCHAR *str;
   int   id;
   BOOL  bIsChar;
};

struct chList messNames[] = {
   TEXT("WM_KEYDOWN"),     WM_KEYDOWN,		FALSE,	
   TEXT("WM_KEYUP"),       WM_KEYUP,		FALSE,	
   TEXT("WM_CHAR"),        WM_CHAR,			TRUE,
   TEXT("WM_DEADCHAR"),    WM_DEADCHAR,		TRUE,
   TEXT("WM_SYSKEYDOWN"),  WM_SYSKEYDOWN,	FALSE,
   TEXT("WM_SYSKEYUP"),    WM_SYSKEYUP,		FALSE,
   TEXT("WM_SYSCHAR"),     WM_SYSCHAR,		TRUE,
   TEXT("WM_SYSDEADCHAR"), WM_SYSDEADCHAR,	TRUE,
   TEXT("UKNOWN"),         0,				
};

/***********************************************************************************
***
***   Support Functions
***
************************************************************************************/


//  zero out the buffers
void emptyKeyAndInstrBuffers() {
	memset((LPVOID)&keyBuffer,0,sizeof(keyBuffer));
    memset((LPVOID)&instructBuffer,0,sizeof(instructBuffer));
	keyBufferIsChar = FALSE;
}
//***********************************************************************************
void recordKey (int message, UINT wParam, LONG lParam) {

   RECT rectWorking;
   
   SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
   
   RECT  rect = {0,90,rectWorking.right,rectWorking.bottom};


   for(int i = 0; i < 7 && messNames[i].id != message; i++) 
      /*loop*/;
   keyBufferIsChar = messNames[i].bIsChar;
   _stprintf(keyBuffer[0],TEXT("%s"), messNames[i].str);
   _stprintf(keyBuffer[1],TEXT("%d"), wParam);
   _stprintf(keyBuffer[2],TEXT("%c"), (BYTE) wParam);
   _stprintf(keyBuffer[3],TEXT("%u"), LOWORD (lParam));
   _stprintf(keyBuffer[4],TEXT("%d"), HIWORD (lParam) & 0xFF);
   _stprintf(keyBuffer[5],TEXT("%s"), (LPTSTR)(0x01000000&lParam?TEXT("Yes"):TEXT("No")));
   _stprintf(keyBuffer[6],TEXT("%s"), (LPTSTR)(0x20000000&lParam?TEXT("Yes"):TEXT("No")));
   _stprintf(keyBuffer[7],TEXT("%s"), (LPTSTR)(0x40000000&lParam?TEXT("Down"):TEXT("Up")));
   _stprintf(keyBuffer[8],TEXT("%s"), (LPTSTR)(0x80000000&lParam?TEXT("Up"):TEXT("Down")));
        
   g_pKato->Log( LOG_DETAIL, TEXT("%s: 0x%02X, %c, %u"), 
                 messNames[i].str, wParam, (BYTE)wParam, LOWORD(lParam) );

}

//***********************************************************************************
void recordInstruction(TCHAR *str) {
   _stprintf(instructBuffer,TEXT("%s         "), str);
}


//***********************************************************************************
///showKeyEvent scrolls the body area and prints current key event
///    fFormat is 1 for characters, 0 for other key events
void showKeyEvent(HDC hdc) {
   // szFormat[0] is key event, szFormat[1] is char event
   static TCHAR *szFormat[] = { TEXT("%-14s %3s    %s %6s %4s %3s %3s %4s %4s"),
							    TEXT("%-14s    %3s %s %6s %4s %3s %3s %4s %4s") };
   int fFormat;
   // choose proper format for key ewent
   if (keyBufferIsChar)
	   fFormat = 1;
   else
	   fFormat = 0;
   TCHAR szBuffer[80];
   wsprintf(szBuffer, szFormat[fFormat], keyBuffer[0], keyBuffer[1],
	   (fFormat ? keyBuffer[2] : TEXT(" ")), keyBuffer[3], keyBuffer[4], keyBuffer[5], 
	   keyBuffer[6], keyBuffer[7], keyBuffer[8]);
   
   scrollBody(hdc, szBuffer);
}
//***********************************************************************************
void showInstruction(HDC hdc) {
    RECT rectWorking;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
    bodyText(hdc, 0, rectWorking.right,0,instructBuffer);
}
