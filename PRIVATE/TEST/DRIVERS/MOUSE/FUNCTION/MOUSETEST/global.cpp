/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-1999  Microsoft Corporation.  All Rights Reserved.

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
int zx,zy;

/***********************************************************************************
***
***   Size Calc
***
************************************************************************************/
void InitAdjust(void) {
	adjustRatio = (((float)zy/(float)zx)+((float)zx/(float)640))/2;
}



/***********************************************************************************
***
***   Window Support
***
************************************************************************************/
WNDPROC func[LASTPROC] = {
   GenericWndProc,
   InstructWindProc,
   TapWindProc,
   DragWindProc,
   WheelWindProc
   
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


//***********************************************************************************
void initAll(void) {
		RECT rcx;
	int ZX1,ZY1, ZX2,ZY2;

   InitAdjust();
	localAtom = MakeMeAWndClass();
   if (!localAtom)
    g_pKato->Log( LOG_FAIL,TEXT("FAIL in %s @ line %d: MakeMeAClass failed: %d"), 
                  szThisFile, __LINE__, GetLastError());
   

     SystemParametersInfo(SPI_GETWORKAREA,0,&rcx,0);
	ZX1 = rcx.left;
	ZX2 = rcx.right;
	ZY1 = rcx.top;
	ZY2 = rcx.bottom;

	

    globalHwnd  = CreateWindow (localAtom,WNDNAME,WS_POPUP | WS_VISIBLE, 
								ZX1,ZY1,
								ZX2-ZX1, ZY2-ZY1,
								NULL, NULL, globalInst, NULL);

   
   if (!globalHwnd)
	   g_pKato->Log( LOG_FAIL, 
	                 TEXT("FAIL in %s @ line %d: CreateWindow failed"),
	                 szThisFile, __LINE__ );

   ShowWindow(globalHwnd, SW_SHOW);
   SetFocus(globalHwnd);
   SetForegroundWindow(globalHwnd);

   // create a fixed pitch font, used by scrollBody and drawKeyEventHeader
   LOGFONT     lgFont;
   memset((LPVOID) &lgFont, 0, sizeof(LOGFONT));
   lgFont.lfCharSet= DEFAULT_CHARSET;
   lgFont.lfHeight = 20;   // 20 pixels in height
   lgFont.lfWeight = 400;  // Normal
   lgFont.lfPitchAndFamily = TMPF_FIXED_PITCH;
   hfont = CreateFontIndirect(&lgFont);

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
   {
   	if( msg.message == WM_PAINT && msg.wParam == 0 )
   	{
		// PeekMessage will not remove a WM_PAINT message with a null DC,
		// so we'll need to account for that situation...
		break;
   	}
   };  // clean out queue
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
      if( 0 == SetTimer(globalHwnd,777,20000,NULL) )
      {
      	g_pKato->Log( LOG_FAIL, TEXT("Line: %d   SetTimer failed. Error: %d"), __LINE__, GetLastError() );
      }
      GetMessage(&msg,NULL,0,0);
      if ((msg.message == WM_TIMER)&&(msg.hwnd==globalHwnd)) 
      
		  if (!askMessage(hwndTestWindow, TEXT("Time out. Do you want to try again?"),2)) {
			  g_pKato->Log( LOG_FAIL, TEXT("User decided not to try again.") );
            return 2;
		  }
      if (FALSE == KillTimer(globalHwnd,777) )
      {
      	g_pKato->Log( LOG_FAIL, TEXT("Line %d   KillTimer failed. Error: %d"), __LINE__, GetLastError() );
      }
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
     if (pass==2) {
	   g_pKato->Log( result ? LOG_DETAIL : LOG_SKIP,
		             TEXT ("%s %s @ line %d: User replied <%s> to <%s>"),
					 result ? TEXT("In") : TEXT("FAIL in"), szThisFile, __LINE__,
					result ? TEXT("YES") : TEXT("No"), str );
    }
	else {
 
   g_pKato->Log( (pass || result) ? LOG_DETAIL : LOG_FAIL,
                 TEXT("%s %s @ line %d: User replied <%s> to <%s>"), 
                 (pass || result) ? TEXT("In") : TEXT("FAIL in"), szThisFile, __LINE__,
                 result ? TEXT("YES") : TEXT("No"), str );
	}
   SetFocus(hwndTestWindow);
   return result;
}
//***********************************************************************************
void initObjects(HDC hdc) {
   SelectObject(hdc, GetStockObject (SYSTEM_FONT));
   SelectObject(hdc,(HBRUSH)GetStockObject(WHITE_BRUSH));
   SelectObject(hdc,(HPEN)GetStockObject(BLACK_PEN));
}


//***********************************************************************************
void initWindow(HDC hdc, TCHAR *top, TCHAR *middle) {
   RECT  r[4]     = {{0,0,zx,zy}, {0,0,zx,40}, {0,40,zx,90}, {0,90,zx,zy}},
         textr[2] = {{0+7,0+7,zx-7,40-7}, {0+7,40+7,zx-7,90-7}};
   HRGN  hRgn     = CreateRectRgnIndirect(&r[0]);;

   initObjects(hdc);
   
   SelectClipRgn(hdc,hRgn);
   for(int i = 1; i < 4; i++)
      ExcludeClipRect(hdc,r[i].left+5,r[i].top+5,r[i].right-5,r[i].bottom-5);
   FillRect(hdc,&r[0],(HBRUSH)GetStockObject(LTGRAY_BRUSH));
   SelectClipRgn(hdc,NULL);
   
   for(i = 1; i < 4; i++)
      Rectangle(hdc,r[i].left+5,r[i].top+5,r[i].right-5,r[i].bottom-5);
   DrawText(hdc, top,_tcslen(top), &textr[0],DT_CENTER);
   DrawText(hdc, middle,_tcslen(middle),&textr[1],DT_CENTER);
   DeleteObject(hRgn);
}


//***********************************************************************************
void bodyText(HDC hdc, int x0, int x1, int line, TCHAR *str) {
   RECT  textr[1] = {x0+7,90+7+line*20,x1-7,zy-7};

   initObjects(hdc);
   DrawText(hdc, str,_tcslen(str), &textr[0],DT_LEFT);
}
//************************************************************************************

//************************************************************************************

//Scroll the body and display new text, print a keyboard message header
// used by wheel test 
void scrollBody(HDC hdc, TCHAR *szText) {
	HFONT hfontPrev;

    RECT rectWorking;

    SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );

    // right and bottom edges are the width and height of our screen area
    DWORD x = rectWorking.right;
    DWORD y = rectWorking.bottom;
  
	RECT rectScroll = {7, 97 + 20, x - 7, y - 7};
    RECT rectDraw = {7, y - 7 - 20, x - 7, y - 7};

	ScrollDC(hdc, 0, -20, &rectScroll, &rectScroll, NULL, NULL);
	hfontPrev = (HFONT) SelectObject(hdc, hfont);
	DrawText(hdc, szText, _tcslen(szText), &rectDraw, DT_LEFT);
	SelectObject(hdc, hfontPrev);  // put back old font
	ValidateRect(globalHwnd, NULL);

}


//***********************************************************************************
void drawKeyEventHeader(HDC hdc) {
	HFONT hfontPrev;

   	RECT rectHeader = {7, 97, zx - 7, 97 + 20};
	
	TCHAR szHeader[] =    TEXT("Message        Key Char Repeat Scan Ext ALT Prev Tran");

	hfontPrev = (HFONT) SelectObject(hdc, hfont);
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
   TEXT("Output goes to DEBUG out (view using: terminal, windbg, etc.)."),
};


//***********************************************************************************
LRESULT CALLBACK InstructWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   static int  keyPress, goal;
   int         strSize, i;
   PAINTSTRUCT ps;

   switch(message) {
   case WM_PAINT:
      BeginPaint(hwnd, &ps);
      strSize = sizeof(str)/sizeof(TCHAR *);
      initWindow(ps.hdc,TEXT("Driver Tests: Instructions"),TEXT("Press <SPACE> to continue."));
      g_pKato->Log(LOG_DETAIL,TEXT("From the WinCE device."));
      for(i = 0; i < strSize; i++) {
         g_pKato->Log(LOG_DETAIL,str[i]);
         bodyText(ps.hdc,0, zx,i,str[i]);
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

//************************************************************************************


void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
 	if (uMsg == WM_TIMER)
		  if (!askMessage(globalHwnd, TEXT("Time out. Do you want to try again?"),2)) {
			  g_pKato->Log( LOG_FAIL, TEXT("User decided not to try again.") );
            
		  }
		 else
      if (FALSE == KillTimer(globalHwnd,777) )
      {
      	g_pKato->Log( LOG_FAIL, TEXT("Line %d   KillTimer failed. Error: %d"), __LINE__, GetLastError() );
      }
}




#ifdef UNDER_CE



// --------------------------------------------------------------------
BOOL GetPlatformName(LPTSTR pszPlatformName, DWORD cLen)
// --------------------------------------------------------------------
{
    DWORD dwPlatformInfoCmd = SPI_GETPLATFORMTYPE;
    
    BOOL fRet = KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO , &dwPlatformInfoCmd, sizeof(DWORD),
        pszPlatformName, cLen * sizeof(TCHAR), NULL);

    if(!fRet)
        _tcscpy(pszPlatformName, _T("Unknown Platform"));
    
    return fRet;
}
#endif


