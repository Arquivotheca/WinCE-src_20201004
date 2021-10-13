/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     global.h  

Abstract:

   Code used for keyboard testing

***************************************************************************/

/***********************************************************************************
***
***   Headers
***
************************************************************************************/
#ifndef GLOBALS
#define GLOBALS


#ifndef WINCEOEM
#define WINCEOEM
#endif  WINCEOEM
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <TUX.h>
#include <KatoEX.h>


#ifndef UNDER_CE
#include <stdio.h>
#endif // UNDER_CE

#define  abort(s) g_pKato->Log(LOG_SKIP,s);
#define  NO_MESSAGES if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED
#define  WM_GOAL        7771
#define  WM_INIT        7772
#define  WM_SWITCHPROC  7773
#define  WM_PLAY        7774
#define  WM_EVENT       7775 
#define  WM_WAKE        7776
#define  WNDNAME    TEXT("Keyboard Test")
#define  LOG_WARN       (LOG_FAIL+1)    

// scale factor
//#define  zx  GetSystemMetrics(SM_CXSCREEN)
#define  zy  GetSystemMetrics(SM_CYSCREEN)
extern   float	adjustRatio;
#define  RESIZE(n) ((int)((float)(n)*adjustRatio))
#define countof(a) (sizeof(a)/sizeof(*(a)))

/***********************************************************************************
***
***   From main.cxx
***
************************************************************************************/
extern TESTPROCAPI getCode(void);
extern HINSTANCE globalInst;
extern HWND      globalHwnd;
extern HDC       globalHDC;
extern int       goKey;
extern CKato     *g_pKato;

/***********************************************************************************
***
***   From global.cxx
***
************************************************************************************/
extern BOOL askMessage(TCHAR *str, int pass);
extern BOOL askMessage(HWND hwndTestWindow, TCHAR * str, int pass);
extern void initAll(void);
extern void initWindow(HDC hdc, TCHAR *top, TCHAR *middle);
extern void bodyText(HDC hdc, int x0, int x1, int line, TCHAR *str);
extern void closeAll(void);
extern int  pump(void);
extern int  pump(HWND);    //added for edit test
extern void doUpdate(CONST RECT *aRect);
extern void clean(void);

extern void scrollBody(HDC hdc, TCHAR *szText);

void drawKeyEventHeader(HDC hdc) ;

/***********************************************************************************
***
***   WNDPROCS
***
************************************************************************************/
extern LRESULT CALLBACK GenericWndProc(HWND hWnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK InstructWindProc(HWND hWnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK KeyPressWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK KeySequenceWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK KeyChordingWinProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK EditTextWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK RepeatRateKeyDelayWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK AsyncWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam);

enum {
   GENERIC,
   INSTRUCT,
   KEYPRESS,
   KEYSEQUENCE,
   KEYCHORDING,
   EDITTEXT,
   KEYRATEDELAY,
   AYSNCKEY,
   LASTPROC,
};

#endif // GLOBALS
