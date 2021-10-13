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
#define MAX_SIZE 512
TCHAR keyBuffer[9][MAX_SIZE];
BOOL keyBufferIsChar;

TCHAR instructBuffer[512];


struct chList {
   TCHAR *str;
   int   id;
   BOOL  bIsChar;
};

struct chList messNames[] = {
   TEXT("WM_KEYDOWN"),     WM_KEYDOWN,      FALSE,  
   TEXT("WM_KEYUP"),       WM_KEYUP,        FALSE,  
   TEXT("WM_CHAR"),        WM_CHAR,         TRUE,
   TEXT("WM_DEADCHAR"),    WM_DEADCHAR,     TRUE,
   TEXT("WM_SYSKEYDOWN"),  WM_SYSKEYDOWN,   FALSE,
   TEXT("WM_SYSKEYUP"),    WM_SYSKEYUP,     FALSE,
   TEXT("WM_SYSCHAR"),     WM_SYSCHAR,      TRUE,
   TEXT("WM_SYSDEADCHAR"), WM_SYSDEADCHAR,  TRUE,
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
   TCHAR wpar[MAX_SIZE]         ={NULL};
   TCHAR lpLow[MAX_SIZE]     ={NULL};
   TCHAR lpHigh[MAX_SIZE]     ={NULL};

   SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );

   for(int i = 0; i < 7 && messNames[i].id != message; i++) 
      /*loop*/;
   TCHAR wp=(BYTE)wParam;
   keyBufferIsChar = messNames[i].bIsChar;
   _itot_s(wParam, wpar, _countof(wpar), 10);
   _itot_s(LOWORD(lParam), lpLow, _countof(lpLow), 10);
   _itot_s((HIWORD (lParam) & 0xFF), lpHigh, _countof(lpHigh), 10);
   

   StringCchCopy(keyBuffer[0], _countof(keyBuffer[0]), messNames[i].str);
   StringCchCopy(keyBuffer[1], _countof(keyBuffer[1]), wpar);
   StringCchCopy(keyBuffer[2], _countof(keyBuffer[2]), &wp);
   StringCchCopy(keyBuffer[3], _countof(keyBuffer[3]), lpLow);
   StringCchCopy(keyBuffer[4], _countof(keyBuffer[4]), lpHigh);
   StringCchCopy(keyBuffer[5], _countof(keyBuffer[5]), (LPTSTR)(0x01000000&lParam?TEXT("Yes"):TEXT("No")));
   StringCchCopy(keyBuffer[6], _countof(keyBuffer[6]), (LPTSTR)(0x20000000&lParam?TEXT("Yes"):TEXT("No")));
   StringCchCopy(keyBuffer[7], _countof(keyBuffer[7]), (LPTSTR)(0x40000000&lParam?TEXT("Down"):TEXT("Up")));
   StringCchCopy(keyBuffer[8], _countof(keyBuffer[8]), (LPTSTR)(0x80000000&lParam?TEXT("Up"):TEXT("Down")));
        
   g_pKato->Log( LOG_DETAIL, TEXT("%s: 0x%02X, %c, %u"), 
                 messNames[i].str, wParam, (BYTE)wParam, LOWORD(lParam) );

}

//***********************************************************************************
void recordInstruction(const TCHAR *str) {
    StringCchPrintf(instructBuffer, _countof(instructBuffer), TEXT("%s         "), str);
}


//***********************************************************************************
///showKeyEvent scrolls the body area and prints current key event
///    fFormat is 1 for characters, 0 for other key events
void showKeyEvent(HDC hdc) {
   // szFormat[0] is key event, szFormat[1] is char event
   static TCHAR szFormat[][80] = { TEXT("%-14s %3s    %s %6s %4s %3s %3s %4s %4s"),
                                TEXT("%-14s    %3s %s %6s %4s %3s %3s %4s %4s") };
   int fFormat;
   // choose proper format for key ewent
   if (keyBufferIsChar)
       fFormat = 1;
   else
       fFormat = 0;
   TCHAR szBuffer[80];
   StringCchPrintf(szBuffer,
        _countof(szBuffer),
        szFormat[fFormat],
        keyBuffer[0],
        keyBuffer[1],
        (fFormat ? keyBuffer[2] : TEXT(" ")),
        keyBuffer[3],
        keyBuffer[4],
        keyBuffer[5],
        keyBuffer[6],
        keyBuffer[7],
        keyBuffer[8]);
   
   scrollBody(hdc, szBuffer);
}
//***********************************************************************************
void showInstruction(HDC hdc) {
    RECT rectWorking;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
    bodyText(hdc, 0, rectWorking.right,0,instructBuffer);
}
