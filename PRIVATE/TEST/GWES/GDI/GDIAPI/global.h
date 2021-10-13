//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/**************************************************************************


Module Name:

   global.h

Abstract:

   Code used by the entire GDI test suite

***************************************************************************/

#ifndef GLOBALS
#define GLOBALS

#ifdef UNDER_CE
#define SURFACE_VERIFY
#endif
/***********************************************************************************
***
***   Headers
***
************************************************************************************/
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <assert.h>
#include "verify.h"
#include "cefile.h"
#include <math.h>
#ifndef UNDER_CE
#include <stdio.h>
#else
#include <pm.h>
#endif // UNDER_CE

/***********************************************************************************
***
***   defines
***
************************************************************************************/
#define  testCycles        50             // number of random tests to run
#define  DEMO              1
#define  NADA              0x10000309     // customer bit(29)=1 e_code=777
#define  NO_MESSAGES       if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;
#define  IDS_String_9962   100  // DrawTextClipRegion() use this
#define  RESIZE(n)         ((int)((float)(n)*adjustRatio))
#define  countof(x)        (sizeof(x) / sizeof(x[0]))
#define  NAMEVALENTRY(x)   { x, TEXT(#x) } 

// scale factor
#define  zx                grcTopMain.right
#define  zy                grcTopMain.bottom

// Bitmaps
#define MAX_PARIS       10    // paris
#define PARIS_X         71
#define PARIS_Y         85
#define UPSIZE          30    // up
#define MAX_GLOBE       9     // globe
#define GLOBE_SIZE      45
#define MAX_FONT_ARRAY  25
#define NUMREGIONTESTS  15

#define ISPRINTERDC(dc) (GetDeviceCaps(dc, TECHNOLOGY) == DT_RASPRINTER)
// This is the number to reset g_iPrinterCntr to when we open a printer DC or when
// we start a new page on the DC
#define PCINIT 2000

/***********************************************************************************
***
***   Structs
***
************************************************************************************/
struct NameValPair {
   DWORD dwVal;
   TCHAR *szName;
};

class cMapClass
{
public:
    cMapClass() {}
    cMapClass(struct NameValPair *nvp):m_funcEntryStruct(nvp) {}
    ~cMapClass() {}
    
    TCHAR * operator [](int index);

private:
    struct NameValPair *m_funcEntryStruct;
};

typedef cMapClass MapClass;

enum fileTypes {
   ETrueType,
   ERaster,
   EJapanSpecial,
   EBad,
   EOther,
};

struct fontInfo {
   TCHAR       *fileName;
   int         fileSize;
   TCHAR       *faceName;
   fileTypes   type;
   BOOL        hardcoded;
};

/***********************************************************************************
***
***   function pointers
***
************************************************************************************/

typedef int (WINAPI * PFNGRADIENTFILL)(TDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);
extern PFNGRADIENTFILL gpfnGradientFill;

typedef int (WINAPI * PFNSTARTDOC)(TDC hdc, CONST DOCINFO *lpdi);
extern PFNSTARTDOC gpfnStartDoc;

typedef int (WINAPI * PFNENDDOC)(TDC hdc);
extern PFNENDDOC gpfnEndDoc;

typedef int (WINAPI * PFNSTARTPAGE)(TDC hdc);
extern PFNSTARTPAGE gpfnStartPage;

typedef int (WINAPI * PFNENDPAGE)(TDC hdc);
extern PFNENDPAGE gpfnEndPage;

/***********************************************************************************
***
***   enums
***
************************************************************************************/
// universal logging
enum infoType {
   FAIL,
   ECHO,
   DETAIL,
   ABORT,
   SKIP,
};

// screen states
enum screenSize {
   Screen640x480,
   Screen320x240,
   Screen480x240,
   ScreenUnknown,
};

// From brush.cpp
enum bitmapTypes {
   compatibleBitmap  = 0,
   lb,
   DibRGB,
   DibPal,
   numBitMapTypes
};

/***********************************************************************************
***
***   extern vars
***
************************************************************************************/
extern HWND     ghwndTopMain;
extern RECT     grcTopMain;
extern BOOL     gSanityChecks;
extern COLORREF PegPal[4];
extern COLORREF gcrWhite;
extern BOOL     g_fPrinterDC;
extern int      g_iPrinterCntr;
extern BOOL gRotateAvail;
extern int gBpp;
extern COLORREF gcrWhite;

// From specdc
extern HINSTANCE  globalInst;
extern int        DCFlag;
extern int        verifyFlag;
extern HWND       myHwnd;
extern int        errorFlag;
extern float      adjustRatio;

// Coresponding to enums
extern NameValPair rgBitmapTypes[numBitMapTypes];

// From global.cpp - font externs
extern int     tahomaFont;
extern int     courierFont;
extern int     symbolFont;
extern int     timesFont;
extern int     wingdingFont;
extern int     verdanaFont;
extern int     aFont;
extern int     MSLOGO;
extern int     arialRasterFont;
extern int     numFonts;

extern struct fontInfo fontArray[MAX_FONT_ARRAY];

extern BOOL g_OutputBitmaps;

// From global.cpp - rect definitions
extern RECT    rTests[NUMREGIONTESTS];
extern RECT    center;

/***********************************************************************************
***
***   Function Prototypes
***
************************************************************************************/
// From Fonts
void  initFonts(void);
void  freeFonts(void);

// From specdc
void  InitSurf(void);
void  InitAdjust(void);
void  FreeSurf(void);
TDC   myGetDC(void);
BOOL  myReleaseDC(TDC hdc);
BOOL  isMemDC(void);
BOOL  SetWindowConstraint(int, int);

// From brush.cpp
HBRUSH setupBrush(TDC, int, int, int, BOOL, BOOL);

/***********************************************************************************
***
***   From main.cpp
***
************************************************************************************/
extern void info(infoType, LPCTSTR,...);
extern TESTPROCAPI getCode(void);

/***********************************************************************************
***
***   From globe.cpp
***
************************************************************************************/
#define xrand(x) goodRandomNum(x)


#define numStockObj 8

extern int myStockObj[numStockObj];

// random number
extern int        goodRandomNum(int range);
extern int        badRandomNum(void);
extern COLORREF      randColorRef(void);

// my Functions
extern void       myDeletePal(TDC hdc, HPALETTE hPal);
extern HPALETTE   myCreatePal(TDC hdc, int code);
extern HPALETTE   myCreateNaturalPal(TDC hdc);
extern TDC        myCreateCompatibleDC(TDC oHdc);
extern void       myDeleteDC(TDC hdc);
extern BOOL       myPatBlt(TDC  hdc,int  nXLeft,int  nYLeft,int  nWidth,int  nHeight, DWORD  dwRop);
extern BOOL       myBitBlt(TDC dDC,int dx,int dy,int dwx,int dwy, TDC sDC,int sx,int sy,DWORD op);
extern BOOL       myStretchBlt(TDC dDC,int dx,int dy,int dwx,int dwy, TDC sDC,int sx,int sy, int swx, int swy, DWORD op);
extern BOOL       myMaskBlt(TDC dDC,int dx,int dy,int dwx,int dwy, TDC sDC,int sx,int sy,TBITMAP hBmp, int bwx, int bwy, DWORD op);
extern BOOL       myGradientFill(TDC tdc, PTRIVERTEX pVertex, ULONG dwNumVertex,  PVOID pMesh, ULONG dwNumMesh,   ULONG dwMode); 
extern BOOL       myGetLastError(DWORD expected);
extern void       mySetLastError(void);
extern UINT       myRealizePalette(TDC hdc);
extern TBITMAP    myCreateDIBSection(TDC hdc,VOID **ppvBits, int d, int w, int h);
extern int        myGetSystemMetrics(int flag);
extern int        myGetBitDepth();
// font
extern HFONT      setupFont(BOOL AddFont, int i, TDC hdc, long Hei, long Wid, long Esc, long Ori, long Wei, BYTE Ita, BYTE Und, BYTE Str);
extern HFONT      setupFontMetrics(BOOL AddFont, int i, TDC hdc, long Hei, long Wid, long Esc, long Ori, long Wei,BYTE Ita, BYTE Und, BYTE Str);
extern void       cleanupFont(BOOL AddFont, int i, TDC hdc, HFONT hFont);
extern void       RemoveAllFonts(void);

// test functions
extern int        passCheck(int result, int expected, TCHAR *args);
extern void       passEmptyRect(int testFunc, TCHAR *name);
extern HANDLE     StartProcess(LPCTSTR szCommand);
extern void       AskMessage(TCHAR *APIname, TCHAR *FuncName, TCHAR *outStr);
extern int        CheckAllWhite(TDC hdc, BOOL details, BOOL complete, BOOL quiet,int expected);
extern void       goodBlackRect(TDC hdc,RECT *r);
extern BOOL       CheckScreenHalves(TDC hdc);
extern void       setRect(RECT *aRect, int l, int t, int r, int b);
extern BOOL       isEqualRect(RECT *a, RECT *b);
extern void       randomRect(RECT *temp);
extern void       setRGBQUAD(RGBQUAD *rgbq, int r, int g, int b);
extern BOOL       isWhitePixel(TDC hdc, DWORD color, int x, int y, int pass);
extern int  WINAPI myAddFontResource(LPTSTR szFontName);
extern BOOL       TestGradientFill();

// UNDER_CE Pending
extern void       pegGdiSetBatchLimit(int i);
extern void       pegGdiFlush(void);

// Visual Check
extern void        MyDelay(int i);

// api names
extern NameValPair funcEntryStruct[];
extern MapClass funcName;


/***********************************************************************************
***
***   API IDs
***
************************************************************************************/
enum {
   EInvalidEntry=0,
   EInfo,
   // inits
   // corresponds to tactics entry, don't change.
   EVerifyManual = 10,
   EVerifyAuto,

   // corresponds to tactics entry, don't change.
   EVerifyDDITEST = 20,
   EVerifyNone,
   
   // corresponds to tactics entry, don't change.
   EErrorCheck = 30,
   EErrorIgnore,

   // gdi surfaces
   // corresponds to tactics entry, don't change.
   EGDI_Primary = 60,
   EGDI_VidMemory,
   EGDI_Default,
   EWin_Primary,
   EGDI_SysMemory,
   EGDI_Printer,

   EGDI_1bppBMP,
   EGDI_2bppBMP,
   EGDI_4bppBMP,
   EGDI_8bppBMP,
   EGDI_16bppBMP,
   EGDI_24bppBMP,
   EGDI_32bppBMP,

   EGDI_1bppPalDIB,
   EGDI_2bppPalDIB,
   EGDI_4bppPalDIB,
   EGDI_8bppPalDIB,

   EGDI_1bppRGBDIB,
   EGDI_2bppRGBDIB,
   EGDI_4bppRGBDIB,
   EGDI_8bppRGBDIB,
   EGDI_16bppRGBDIB,
   EGDI_24bppRGBDIB,
   EGDI_32bppRGBDIB,

   EGDI_1bppPalDIBBUB,
   EGDI_2bppPalDIBBUB,
   EGDI_4bppPalDIBBUB,
   EGDI_8bppPalDIBBUB,

   EGDI_1bppRGBDIBBUB,
   EGDI_2bppRGBDIBBUB,
   EGDI_4bppRGBDIBBUB,
   EGDI_8bppRGBDIBBUB,
   EGDI_16bppRGBDIBBUB,
   EGDI_24bppRGBDIBBUB,
   EGDI_32bppRGBDIBBUB,
   //********************************
   // end of init, start of test section

   // clip
   // corresponds to tactics entry, don't change.
   EExcludeClipRect = 100,
   EGetClipBox,
   EGetClipRgn,
   EIntersectClipRect,
   ESelectClipRgn,

   // draw
   // corresponds to tactics entry, don't change.
   EBitBlt = 200,
   EClientToScreen,
   ECreateBitmap,
   ECreateCompatibleBitmap,
   ECreateDIBSection,
   EEllipse,
   EDrawEdge,
   EGetPixel,
   EMaskBlt,
   EPatBlt,
   EPolygon,
   EPolyline,
   ERectangle,
   EFillRect,
   ERectVisible,
   ERoundRect,
   EScreenToClient,  
   ESetPixel,
   EStretchBlt,
   ETransparentImage,
   EStretchDIBits,
   ESetDIBitsToDevice,
   EGradientFill,
   EInvertRect,
   EMoveToEx,
   ELineTo,
   EGetDIBColorTable,
   ESetDIBColorTable,
   
   //brush & pens
   // corresponds to tactics entry, don't change.
   ECreateDIBPatternBrushPt = 600,
   ECreatePatternBrush,
   ECreateSolidBrush,
   EGetBrushOrgEx,
   EGetSysColorBrush,
   ESetBrushOrgEx,
   ECreatePenIndirect,
   ESystemStockBrush,
   
   // dc
   // corresponds to tactics entry, don't change.
   ECreateCompatibleDC = 800,
   EDeleteDC,
   EGetDCOrgEx,
   EGetDeviceCaps,
   ERestoreDC,
   ESaveDC,
   EScrollDC,
   
   // do
   // corresponds to tactics entry, don't change.
   EDeleteObject = 900,
   EGetCurrentObject,
   EGetObject,
   EGetObjectType,
   EGetStockObject,
   ESelectObject,
   EUnrealizeObject,

   // text
   // corresponds to tactics entry, don't change.
   EDrawTextW = 1100,
   EExtTextOut,
   EGetTextAlign,
   EGetTextExtentPoint,
   EGetTextExtentPoint32,
   EGetTextExtentExPoint,
   EGetTextFace,
   EGetTextMetrics,
   ESetTextAlign,
   
   // testchk
   // corresponds to tactics entry, don't change.
   ECheckAllWhite = 1200,
   ECheckScreenHalves,
   EGetReleaseDC,
   EThrash,  

   //Manual
   // corresponds to tactics entry, don't change
   ERop2 = 1300,
   EManualFont,
   EManualFontClip,
   ELastName,
};


#endif // Globals
