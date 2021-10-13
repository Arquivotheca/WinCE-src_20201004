/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     global.cpp  

Abstract:

   Code used by the entire GDI test suite
***************************************************************************/
#include "global.h"

HINSTANCE globalInst;
HWND      globalHwnd;
HDC       globalHDC;
HFONT     hfont;        // For fixed pitch draw text
LPCTSTR   localAtom;
int       goKey = 32;   // Space
float	  adjustRatio;

static TCHAR     szThisFile[] = TEXT("global.cpp");

/***********************************************************************************
***
***   Size Calc
***
************************************************************************************/
void InitAdjust(void) {   
    RECT rectWorking;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
	adjustRatio = (((float)rectWorking.bottom/(float)rectWorking.right) +
	    ((float)rectWorking.right/(float)640))/2;
}



/***********************************************************************************
***
***   Window Support
***
************************************************************************************/
WNDPROC func[LASTPROC] = {
   GenericWndProc,
   InstructWindProc,
   KeyPressWindProc,
   KeySequenceWindProc,
   KeyChordingWinProc,
   EditTextWndProc,
   RepeatRateKeyDelayWndProc,
   AsyncWndProc,
};


//***********************************************************************************
LRESULT CALLBACK GenericWndProc(HWND hWnd, UINT message,WPARAM wParam, LPARAM lParam) {
   static int currentProc; 

   if (message == WM_SWITCHPROC) {
      currentProc = (int)wParam;
      return 0;
   }

   switch (message) {
   case WM_SETFOCUS:
      doUpdate(NULL);
      return 0;
   }

   if (currentProc != GENERIC) {
      if (currentProc > -1 && currentProc < LASTPROC)
         return (LRESULT)func[currentProc](hWnd,message,wParam,lParam);
   } 
   return DefWindowProc(hWnd, message, wParam, lParam);
}


//***********************************************************************************
LPCTSTR MakeMeAWndClass(void) {
   WNDCLASS wc;
   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = (WNDPROC)GenericWndProc;   // Window Procedure                       
   wc.hInstance     = globalInst;                // Owner of this class
   wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); 
   wc.lpszClassName = WNDNAME;
   wc.style         = CS_DBLCLKS;
   return(LPCTSTR)RegisterClass(&wc);
}

HFONT GetNormalFont(INT size)
{
    LOGFONT logfont;
    HDC     hdc;

   ASSERT(size>1);

    memset(&logfont, 0, sizeof (logfont));

    hdc = GetDC(NULL);
    logfont.lfHeight = -( GetDeviceCaps(hdc, LOGPIXELSY) / size);
    ReleaseDC(NULL, hdc);

    logfont.lfWeight         = FW_NORMAL;
    logfont.lfCharSet        = DEFAULT_CHARSET;
    logfont.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    logfont.lfQuality        = DEFAULT_QUALITY;
    logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

   
    return CreateFontIndirect(&logfont);
}

//***********************************************************************************
void initAll(void) {

    RECT rectWorking;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
    
   InitAdjust();
	localAtom = MakeMeAWndClass();
   if (!localAtom)
    g_pKato->Log( LOG_FAIL,TEXT("FAIL in %s @ line %d: MakeMeAClass failed: %d"), 
                  szThisFile, __LINE__, GetLastError());
   
   globalHwnd = CreateWindow(localAtom, WNDNAME, WS_POPUP | WS_VISIBLE|WS_HSCROLL,
      0,0,rectWorking.right,rectWorking.bottom,NULL,NULL,globalInst,NULL);
   if (!globalHwnd)
	   g_pKato->Log( LOG_FAIL, 
	                 TEXT("FAIL in %s @ line %d: CreateWindow failed"),
	                 szThisFile, __LINE__ );

   ShowWindow(globalHwnd, SW_SHOW);
   SetFocus(globalHwnd);
   SetForegroundWindow(globalHwnd);

   // create a fixed pitch font, used by scrollBody and drawKeyEventHeader
   
   hfont = GetNormalFont(9);

   Sleep(0);
}


//***********************************************************************************
// free up all the UI resources
void closeAll(void) {
   static BOOL bClosed = FALSE;  // only need to do this once
   if (!bClosed) {
		ReleaseDC(globalHwnd,globalHDC);
		DestroyWindow(globalHwnd);
		UnregisterClass(localAtom,globalInst);
		globalHwnd  = NULL;
		localAtom   = NULL;
		bClosed = TRUE;
   }
}


//***********************************************************************************
void clean(void) {
   MSG   msg;
   while (PeekMessage(&msg,globalHwnd,0,0,PM_REMOVE)) 
      ;  // clean out queue
}


//***********************************************************************************
// pump: message loop, until WM_GOAL message returns true or user input times out
int pump(void) {
   return pump(globalHwnd);
}

// same functionality as pump(void), which makes sure a specific window get the focus
//  if after a timeout message box.  Created for edit text.
int pump(HWND hwndTestWindow) {
	MSG   msg;
    do {
      SetTimer(globalHwnd,777,20000,NULL);
      GetMessage(&msg,NULL,0,0);
      if (msg.message == WM_TIMER)
         if (!askMessage(hwndTestWindow, TEXT("Time out. Do you want to try again?"),1))
            return 2;
      KillTimer(globalHwnd,777);
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (SendMessage(globalHwnd,WM_GOAL,0,0)) 
         return 1;
   } while (msg.message != WM_QUIT);
   return 0;
}
/***********************************************************************************
***
***   UI
***
************************************************************************************/

//***********************************************************************************
BOOL askMessage(TCHAR *str, int pass) {
   return askMessage(globalHwnd, str, pass);
}

BOOL askMessage(HWND hwndTestWindow, TCHAR *str, int pass) {
   clean();
   BOOL result = (IDYES == MessageBox(hwndTestWindow, str, TEXT("Input Required"),MB_YESNO|MB_DEFBUTTON1));
   g_pKato->Log( (pass || result) ? LOG_DETAIL : LOG_FAIL,
                 TEXT("%s %s @ line %d: User replied <%s> to <%s>"), 
                 (pass || result) ? TEXT("In") : TEXT("FAIL in"), szThisFile, __LINE__,
                 result ? TEXT("YES") : TEXT("No"), str );
   SetFocus(hwndTestWindow);
   return result;
}


//***********************************************************************************
void initObjects(HDC hdc) {


   SelectObject(hdc, GetNormalFont(9));
   SelectObject(hdc,(HBRUSH)GetStockObject(WHITE_BRUSH));
   SelectObject(hdc,(HPEN)GetStockObject(BLACK_PEN));
}
 

//***********************************************************************************
void initWindow(HDC hdc, TCHAR *top, TCHAR *middle) {

   RECT rectWorking;

   SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );

   // right and bottom edges are the width and height of our screen area
   DWORD x = rectWorking.right;
   DWORD y = rectWorking.bottom;

   RECT  r[4]     = {{0,0,x,y}, {0,0,x,40}, {0,40,x,90}, {0,90,x,y}},
         textr[2] = {{0+7,0+7,x-7,40-7}, {0+7,40+7,x-7,90-7}};
   HRGN  hRgn     = CreateRectRgnIndirect(&r[0]);;

   initObjects(hdc);
   
   SelectClipRgn(hdc,hRgn);
   for(int i = 1; i < 4; i++)
      ExcludeClipRect(hdc,r[i].left+5,r[i].top+5,r[i].right-5,r[i].bottom-5);
   FillRect(hdc,&r[0],(HBRUSH)GetStockObject(LTGRAY_BRUSH));
   SelectClipRgn(hdc,NULL);
   
   for(i = 1; i < 4; i++)
      Rectangle(hdc,r[i].left+5,r[i].top+5,r[i].right-5,r[i].bottom-5);
   DrawText(hdc, top,_tcslen(top), &textr[0],DT_CENTER|DT_WORDBREAK);
   DrawText(hdc, middle,_tcslen(middle),&textr[1],DT_CENTER|DT_WORDBREAK);
   DeleteObject(hRgn);
}


//***********************************************************************************
void bodyText(HDC hdc, int x0, int x1, int line, TCHAR *str) {
   RECT rectWorking;
   SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
   
   RECT  textr[1] = {x0+7,90+7+line*20,x1-7,rectWorking.bottom-7};

   initObjects(hdc);
   DrawText(hdc, str,_tcslen(str), &textr[0],DT_LEFT|DT_WORDBREAK);
}
//************************************************************************************

//Scroll the body and display new text, print a keyboard message header
// used by keyboard event viewers
void scrollBody(HDC hdc, TCHAR *szText) {
	HFONT hfontPrev,hfontCur;

    RECT rectWorking;

    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );

    // right and bottom edges are the width and height of our screen area
    DWORD x = rectWorking.right;
    DWORD y = rectWorking.bottom;
  
	RECT rectScroll = {7, 97 + 20, x - 7, y - 7};
    RECT rectDraw = {7, y - 7 - 20, x - 7, y - 7};

	ScrollDC(hdc, 0, -20, &rectScroll, &rectScroll, NULL, NULL);
       hfontCur=GetNormalFont(11);
	hfontPrev = (HFONT) SelectObject(hdc, hfontCur);
	DrawText(hdc, szText, _tcslen(szText), &rectDraw, DT_LEFT);
	SelectObject(hdc, hfontPrev);  // put back old font
	ValidateRect(globalHwnd, NULL);

}
//***********************************************************************************
void drawKeyEventHeader(HDC hdc) {
	HFONT hfontPrev,hfontCur;

    RECT rectWorking;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );

   	RECT rectHeader = {7, 97, rectWorking.right - 7, 97 + 20};
	
	TCHAR szHeader[] =    TEXT("Message        Key Char Repeat Scan Ext ALT Prev Tran");

        hfontCur=GetNormalFont(11);
        
	hfontPrev = (HFONT) SelectObject(hdc, hfontCur);
    
	DrawText(hdc, szHeader, _tcslen(szHeader), &rectHeader, DT_LEFT);

	SelectObject(hdc, hfontPrev);  // put back old font
}
//***********************************************************************************
void doUpdate(CONST RECT *aRect) {
   InvalidateRect(globalHwnd,aRect,0);
   UpdateWindow(globalHwnd);
}


/***********************************************************************************
***
***   Instructions
***
************************************************************************************/
TCHAR *str[] = {
   TEXT("Top box: describes the current test."),
   TEXT("Second box: instructions for the current test."),
   TEXT("This box: shows the output (if any) from the current test."),
   TEXT(""),
   TEXT("Test Hung? - wait 20 seconds - a dialog will let you move on."),
    TEXT(""),
   TEXT("Output goes to DEBUG out (view using: terminal, windbg, etc.)."),
};


//***********************************************************************************
LRESULT CALLBACK InstructWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   static int  keyPress, goal;
   int         strSize, i;
   PAINTSTRUCT ps;

   RECT rectWorking;

   switch(message) {
   case WM_PAINT:
      SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
      BeginPaint(hwnd, &ps);
      strSize = sizeof(str)/sizeof(TCHAR *);
      initWindow(ps.hdc,TEXT("Keyboard Tests: Instructions"),TEXT("Press <SPACE> to continue."));
      g_pKato->Log(LOG_DETAIL,TEXT("From the WinCE device."));
      for(i = 0; i < strSize; i++) {
         g_pKato->Log(LOG_DETAIL,str[i]);
         bodyText(ps.hdc,0, rectWorking.right,i,str[i]);
      }
      EndPaint(hwnd, &ps);
      return 0;
   case WM_INIT:
      keyPress = 0;
      goal = (int)wParam;
      return 0;
   case WM_KEYDOWN:
      keyPress = (int)wParam;
      return 0;
   case WM_GOAL:
      return keyPress == goal;     
   }
   return DefWindowProc(hwnd, message, wParam, lParam);
}


//***********************************************************************************
TESTPROCAPI Instructions_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {			   
   NO_MESSAGES;

   SendMessage(globalHwnd,WM_SWITCHPROC,INSTRUCT,0);
   SendMessage(globalHwnd,WM_INIT,goKey,0);
   doUpdate(NULL);
   pump();
   return getCode();
}
