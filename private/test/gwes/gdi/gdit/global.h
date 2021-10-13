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
#include <math.h>
#include "imgcompare.h"
#include "bcrypt.h"
#ifndef UNDER_CE
#include <stdio.h>
#else
#include <pm.h>
#include <bldver.h>
#endif // UNDER_CE
#include "skiptable.h"

#define GDITEST_CHANGE_VER 101
#define RAND_BIT_MASK      0x7FFF

/***********************************************************************************
***
***   defines
***
************************************************************************************/
#define  testCycles        50             // number of random tests to run
#define  NO_MESSAGES       do{ if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED; }while(0)
#define  IDS_String_9962   100  // DrawTextClipRegion() use this
#define  RESIZE(n)         ((int)((float)(n)*adjustRatio))
#define  countof(x)        (sizeof(x) / sizeof(x[0]))
#define  NAMEVALENTRY(x)   { x, TEXT(#x) }
#define  MINSCREENSIZE 176
#define  MAXSTRINGLENGTH 256
#define  NOPRINTERDC(dc, retval) do{ if (isPrinterDC(dc)) { return (retval);} }while(0)
#define  NOPRINTERDCV(dc)        do{ if (isPrinterDC(dc)) { return;} }while(0)

// scale factor
#define  zx                (grcInUse.right - grcInUse.left)
#define  zy                (grcInUse.bottom - grcInUse.top)

// Bitmaps
#define  MAX_PARIS       10    // paris
#define  PARIS_X         71
#define  PARIS_Y         85
#define  UPSIZE          30    // up
#define  MAX_GLOBE       9     // globe
#define  GLOBE_SIZE      45
#define  NUMREGIONTESTS  15
#define  MAXERRORMESSAGES 100
#define NOERRORFOUND TEXT("Detailed error information not found")

// This is the number to reset g_iPrinterCntr to when we open a printer DC or when
// we start a new page on the DC
#define PCINIT 2000


// CE has an RGBA macro defined.
#ifndef RGBA
#define RGBA(r,g,b,a)        ((COLORREF)( (((DWORD)(BYTE)(a))<<24) | RGB(r,g,b) ))
#endif

#define GetAValue(cr) ((cr & 0xFF000000) >> 24)

#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS BI_BITFIELDS
#endif

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY  5
#endif

#ifndef CLEARTYPE_COMPAT_QUALITY
// nt doesn't support cleartype_compat_quality, so just test cleartype quality instead
#define CLEARTYPE_COMPAT_QUALITY  CLEARTYPE_QUALITY
#endif

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

    // update the internal NameValPair on the fly
    inline void cMapClass::set(struct NameValPair *nvp) { m_funcEntryStruct=nvp; }
    // returns the internal NameValPair for easier usage scenarios
    inline struct NameValPair* cMapClass::get(void) { return m_funcEntryStruct; } 
    
    TCHAR * operator [](int index);
    int operator ()(TCHAR * tcString);

private:
    struct NameValPair *m_funcEntryStruct;
};

typedef cMapClass MapClass;

// font data structures
typedef struct __fileData {
    TCHAR *tszFileName;
    TCHAR *tszFullPath;
    FILETIME ft;
    DWORD dwFileSizeHigh;
    DWORD dwFileSizeLow;
    DWORD dwType;
    DWORD dwFileType;
    BOOL bFontAdded;
    DWORD dwErrorCode;
} fileData;

// font file types
enum fontfileType {
   TRUETYPE_FILETYPE,
   TRUETYPECOLLECTION_FILETYPE,
   TRUETYPEAGFA_FILETYPE,
   RASTERFON_FILETYPE,
   RASTERFNT_FILETYPE
};

typedef struct __KnownfontInfo {
    TCHAR       *tszFaceName;
    TCHAR       *tszFullName;
    TCHAR       *tszFileName;
    int *pnFontIndex;
    LONG lWeight;
    BYTE bItalics;
    FILETIME ft;
    DWORD dwFileSizeLow;
    DWORD dwFileSizeHigh;
    DWORD dwType;
} KnownfontInfo;

typedef struct __fontInfo {
    TCHAR       *tszFaceName;
    TCHAR       *tszFullName;
    LONG lWeight;
    BYTE bItalics;
    DWORD dwType;
    SKIPTABLE st;
    int nLinkedFont;
    int nFontLinkMethod;
} fontInfo;

// bitmap data
typedef union tagMYRGBQUAD {
    struct {
      BYTE rgbBlue;
      BYTE rgbGreen;
      BYTE rgbRed;
      BYTE rgbReserved;
    };
    UINT rgbMask;
} MYRGBQUAD;

struct MYBITMAPINFO {
    BITMAPINFOHEADER bmih;
    MYRGBQUAD ct[256];
};

/***********************************************************************************
***
***   function pointers
***
************************************************************************************/

typedef int (WINAPI * PFNGRADIENTFILL)(TDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);
extern PFNGRADIENTFILL gpfnGradientFill;

typedef BOOL (WINAPI * PFNALPHABLEND)(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);
extern PFNALPHABLEND gpfnAlphaBlend;

typedef int (WINAPI * PFNSTARTDOC)(TDC hdc, CONST DOCINFO *lpdi);
extern PFNSTARTDOC gpfnStartDoc;

typedef int (WINAPI * PFNENDDOC)(TDC hdc);
extern PFNENDDOC gpfnEndDoc;

typedef int (WINAPI * PFNSTARTPAGE)(TDC hdc);
extern PFNSTARTPAGE gpfnStartPage;

typedef int (WINAPI * PFNENDPAGE)(TDC hdc);
extern PFNENDPAGE gpfnEndPage;

typedef int (WINAPI * PFNABORTDOC)(TDC hdc);
extern PFNABORTDOC gpfnAbortDoc;

typedef int (WINAPI * PFNSETABORTPROC)(TDC hdc, ABORTPROC ap);
extern PFNSETABORTPROC gpfnSetAbortProc;

typedef HRESULT (WINAPI * PFNSHIDLETIMERRESETEX)(DWORD dwFlags);
extern PFNSHIDLETIMERRESETEX gpfnSHIdleTimerResetEx;

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

// From brush.cpp
enum bitmapTypes {
   compatibleBitmap  = 0,
   lb,
   DibRGB,
   DibPal,
   numBitMapTypes
};

enum PixelFormats {
    // special values
    RGBERROR = 0,
    RGBFIRST = 1,

    // 16bpp
    RGB4444 = 1,
    RGB555,
    RGB1555,
    RGB565,

    BGR4444,
    BGR555,
    BGR1555,
    BGR565,

    // 32bpp
    RGB8888,
    RGB888,
    BGR8888,
    BGR888,

    // special values
    RGBEMPTY,
    RGBLAST
};
// CE supports bilinear, XP doesn't.
#ifndef UNDER_CE
#define BILINEAR BLACKONWHITE
#endif

/***********************************************************************************
***
***   extern vars
***
************************************************************************************/
// From Global.cpp
extern BYTE *glpBits;
extern struct MYBITMAPINFO gBitmapInfo;
extern int gBitmapType;
extern int DCFlag;
extern int verifyFlag;
extern int errorFlag;
extern int DriverVerify;
extern fontInfo *fontArray;
extern RECT rTests[NUMREGIONTESTS];
extern RECT center;

extern DWORD g_dwCurrentTestCase;

// all of the font file data available on the system
extern fileData *gfdWindowsFonts;
extern int     gnWindowsFontsEntries;
extern fileData *gfdWindowsFontsFonts;
extern int     gnWindowsFontsFontsEntries;
extern fileData *gfdUserFonts;
extern int     gnUserFontsEntries;

// Global.cpp enum's
extern NameValPair rgBitmapTypes[numBitMapTypes];

// From global.cpp - font externs
extern int     tahomaFont;
extern int     legacyTahomaFont;
extern int     BigTahomaFont;
extern int     courierFont;
extern int     symbolFont;
extern int     timesFont;
extern int     wingdingFont;
extern int     verdanaFont;
extern int     aFont;
extern int     MSLOGO;
extern int     arialRasterFont;
extern int     numFonts;

// from Main.cpp
extern HWND     ghwndTopMain;
extern HPEN g_hpenStock;
extern HBRUSH g_hbrushStock;
extern HPALETTE g_hpalStock;
extern HBITMAP g_hbmpStock;
extern HFONT g_hfontStock;
// grcInUse is the current area to work on.
extern RECT     grcInUse;
// grcToUSe is the area requested by the user
extern RECT     grcToUse;
// grcTopMain is the area of the topmost window
extern RECT     grcTopMain;
// grcWorkArea is the work area of the full screen.
extern RECT     grcWorkArea;
extern BOOL     gSanityChecks;
extern int      g_iPrinterCntr;
extern BOOL gResizeAvail;
extern BOOL gResizeDisabledByUser;
extern DWORD gdwMaxDisplayModes();
extern BOOL gRotateAvail;
extern BOOL gRotateDisabledByUser;
extern int gBpp;
extern DWORD gWidth;
extern DWORD gHeight;
extern TCHAR gszBitmapDestination[];
extern TCHAR gszFontLocation[];
extern DWORD gMonitorToUse;
extern BOOL g_bMonitorSpread;
extern BOOL g_bRTL;                 // set TRUE for RTL windows
extern TCHAR gszVerifyDriverName[];

// From specdc
extern HINSTANCE  globalInst;
extern HWND       myHwnd;
extern float      adjustRatio;
extern DWORD gdwRedBitMask;
extern DWORD gdwGreenBitMask;
extern DWORD gdwBlueBitMask;
extern DWORD gdwShiftedRedBitMask;
extern DWORD gdwShiftedGreenBitMask;
extern DWORD gdwShiftedBlueBitMask;

// From Verify.cpp
extern BOOL g_OutputBitmaps;
extern int g_nFailureNum;
extern TDC g_hdcBAD;
extern TDC g_hdcBAD2;
extern TBITMAP g_hbmpBAD;
extern TBITMAP g_hbmpBAD2;
extern HDC g_hdcSecondary;

/***********************************************************************************
***
***   Function Prototypes
***
************************************************************************************/
// From Fonts
void  initFonts(void);
void  freeFonts(void);

void  AddAllUserFonts(void);
void  RemoveAllUserFonts(void);


// From specdc
void  InitSurf(void);
void  InitAdjust(void);
void  FreeSurf(void);
TDC   myGetDC(void);
BOOL  myReleaseDC(TDC hdc);
BOOL  isMemDC(void);
BOOL  isWinDC(void);
BOOL isPalDIBDC(void);
BOOL isSysPalDIBDC(void);
BOOL isDIBDC(void);
BOOL  isPrinterDC(TDC hdc);
BOOL  SetWindowConstraint(int, int);
void pumpMessages(void);

// From brush.cpp
HBRUSH setupBrush(TDC, int, int, int, BOOL, BOOL);
BYTE GetChannelValue(BYTE, BYTE, BYTE, DWORD);
BYTE GetROP2ChannelValue(BYTE, BYTE, DWORD);

// From Draw.cpp
void WritableBitmapTest(int testFunc);

// from dc.cpp
void OutputDeviceCapsInfo(TDC hdc);

// from main.cpp
void RehideShell(HWND);
BOOL GetUSPMode(void);

/***********************************************************************************
***
***   From main.cpp
***
************************************************************************************/
extern void info(infoType, LPCTSTR,...);
extern TESTPROCAPI getCode(void);
extern void OutputLogInfo(cCompareResults * cCr);


/***********************************************************************************
***
***   Error checking macro's
***
************************************************************************************/

void InternalCheckNotRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum);
void InternalCheckForRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum);
void InternalCheckForLastError(DWORD dwExpectedResult, const TCHAR *File, int LineNum);

// This is for verifying that functions return something other than the failure code.
// If there was a failure, reset the last error so info and FormatMessage don't interfere with it.
#define CheckNotRESULT(__RESULT, __FUNC) \
do{ \
    InternalCheckNotRESULT((DWORD) __RESULT, (DWORD) (__FUNC), TEXT(#__FUNC), TEXT(__FILE__), __LINE__); \
}while(0)

// this verifies that the function returns what it's expected to return.
// If there was a failure, reset the last error so info and FormatMessage don't interfere with it.
#define CheckForRESULT(__RESULT, __FUNC) \
do{ \
    InternalCheckForRESULT((DWORD) __RESULT, (DWORD) (__FUNC), TEXT(#__FUNC), TEXT(__FILE__), __LINE__); \
}while(0)

// don't check the last error for printer DC's.  Metafiles don't set last errors.
#ifdef UNDER_CE
#define CheckForLastError(__RESULT) \
do{ \
    if(!isPrinterDC(NULL)) \
        InternalCheckForLastError((DWORD) __RESULT, TEXT(__FILE__), __LINE__); \
}while(0)
#else
#define CheckForLastError(__RESULT) 0
#endif

/***********************************************************************************
***
***   From global.cpp
***
************************************************************************************/
#define xrand(x) goodRandomNum(x)


#define numStockObj 12

extern int myStockObj[numStockObj];

// random number
extern int        goodRandomNum(int range);
extern int        badRandomNum(void);
int      GenRand();
extern COLORREF      randColorRef(void);
extern COLORREF      randColorRefA(void);

// my Functions
extern void       myDeletePal(TDC hdc, HPALETTE hPal);
extern HPALETTE   myCreatePal(TDC hdc, int code);
extern HPALETTE   myCreateNaturalPal(TDC hdc);
extern UINT       myRealizePalette(TDC hdc);
extern int        myGetBitDepth();

// font
HFONT setupFont(TCHAR *tszFaceName, long Hei, long Wid, long Esc, long Ori, long Wei, BYTE Ita, BYTE Und, BYTE Str);
HFONT setupKnownFont(int index, long Hei, long Wid, long Esc, long Ori, BYTE Und, BYTE Str);

// test functions
extern COLORREF SwapRGB(COLORREF cr);
extern int        passCheck(int result, int expected, TCHAR *args);
extern void       passEmptyRect(int testFunc, TCHAR *name);
extern void       AskMessage(TCHAR *APIname, TCHAR *FuncName, TCHAR *outStr);
extern int        CheckAllWhite(TDC hdc, BOOL quiet, DWORD dwExpected);
extern void       goodBlackRect(TDC hdc,RECT *r);
extern BOOL       CheckScreenHalves(TDC hdc);
void SetMaxScreenHalvesErrorPercentage(DOUBLE MaxError);
void SetScreenHalvesRMSPercentage(UINT Threshold, UINT AvgThreshold);
void ResetScreenHalvesCompareConstraints();
extern void       setRect(RECT *aRect, int l, int t, int r, int b);
extern BOOL       isEqualRect(RECT *a, RECT *b);
extern void       randomRect(RECT *temp);
extern void OrderRect(RECT * rc);
extern BOOL overlap(RECT * a, RECT * b, BOOL isPoint);
extern int roundUp(double);
extern void randomRectWithinArea(int ntop, int nleft, int nright, int nbottom, RECT * rcResult);
extern void randomWORectWithinArea(int ntop, int nleft, int nright, int nbottom, RECT * rcResult);
extern void       setRGBQUAD(MYRGBQUAD *rgbq, int r, int g, int b);
extern void       setRGBQUAD(RGBQUAD *rgbq, int r, int g, int b);
COLORREF GetColorRef(RGBQUAD * rgbq);
COLORREF GetColorRef(MYRGBQUAD * rgbq);
extern BOOL       isWhitePixel(TDC hdc, DWORD color, int x, int y, int pass);
extern BOOL       isBlackPixel(TDC hdc, DWORD color, int x, int y, int pass);
extern BOOL       TestGradientFill();
void ShiftBitMasks(DWORD *dwRed, DWORD *dwGreen, DWORD *dwBlue);

// UNDER_CE Pending
extern void       pegGdiSetBatchLimit(int i);
extern void       pegGdiFlush(void);

// Visual Check
extern void        MyDelay(int i);

// api names
extern NameValPair funcEntryStruct[];
extern MapClass funcName;

// surface groups
extern NameValPair DCEntryCommonStruct[];
extern NameValPair DCEntryExtendedStruct[];
extern NameValPair DCEntryAllStruct[];
extern MapClass DCName;

/***********************************************************************************
***
***   API IDs
***
************************************************************************************/
enum {
   EInvalidEntry=0,

   // inits
   EVerifyManual,
   EVerifyAuto,
   EVerifyDDITEST,
   EVerifyNone,
   EVerifyAlways,

   // gdi surfaces
   EWin_Primary = 10,
   EFull_Screen,
   EGDI_VidMemory,
   EGDI_SysMemory,
   ECreateDC_Primary,
   EGDI_Printer,

   EGDI_1bppBMP,
   EGDI_2bppBMP,
   EGDI_4bppBMP,
   EGDI_8bppBMP,
   EGDI_16bppBMP,
   EGDI_24bppBMP,
   EGDI_32bppBMP,

   // DIBDCSTART and DIBDCEND are used for comparisons of the
   // dc in use to figure out if it's a DIB or not.
   EGDI_DIBDCSTART,
   EGDI_8bppPalDIB = EGDI_DIBDCSTART,

   EGDI_1bppRGBDIB,
   EGDI_2bppRGBDIB,
   EGDI_4bppRGBDIB,
   EGDI_8bppRGBDIB,
   EGDI_16bppRGBDIB,
   EGDI_16bppRGB4444DIB,
   EGDI_16bppRGB1555DIB,
   EGDI_16bppRGB555DIB,
   EGDI_16bppRGB565DIB,
   EGDI_16bppBGR4444DIB,
   EGDI_16bppBGR1555DIB,
   EGDI_16bppBGR555DIB,
   EGDI_16bppBGR565DIB,
   EGDI_24bppRGBDIB,
   EGDI_32bppRGBDIB,
   EGDI_32bppRGB8888DIB,
   EGDI_32bppRGB888DIB,
   EGDI_32bppBGR8888DIB,
   EGDI_32bppBGR888DIB,

   EGDI_8bppPalDIBBUB,

   EGDI_1bppRGBDIBBUB,
   EGDI_2bppRGBDIBBUB,
   EGDI_4bppRGBDIBBUB,
   EGDI_8bppRGBDIBBUB,
   EGDI_16bppRGBDIBBUB,
   EGDI_16bppRGB4444DIBBUB,
   EGDI_16bppRGB1555DIBBUB,
   EGDI_16bppRGB555DIBBUB,
   EGDI_16bppRGB565DIBBUB,
   EGDI_16bppBGR4444DIBBUB,
   EGDI_16bppBGR1555DIBBUB,
   EGDI_16bppBGR555DIBBUB,
   EGDI_16bppBGR565DIBBUB,
   EGDI_24bppRGBDIBBUB,
   EGDI_32bppRGBDIBBUB,
   EGDI_32bppRGB8888DIBBUB,
   EGDI_32bppRGB888DIBBUB,
   EGDI_32bppBGR8888DIBBUB,
   EGDI_32bppBGR888DIBBUB,
   EGDI_DIBDCEND = EGDI_32bppBGR888DIBBUB,
   //********************************
   // end of internal enum values, start of test section

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
   ESetBitmapBits,
   EDrawFocusRect,
   ESetROP2,
   EAlphaBlend,
   EMapWindowPoints,
   EGetROP2,

   // pal
   // corresponds to tactics entry, don't change.
   EGetNearestColor = 300,
   EGetNearestPaletteIndex,
   EGetSystemPaletteEntries,
   EGetPaletteEntries,
   ESetPaletteEntries,
   ECreatePalette,
   ESelectPalette,
   ERealizePalette,

   // GDI
   // corresponds to tactics entry, don't change.
   EGdiFlush = 400,
   EGdiSetBatchLimit,
   EChangeDisplaySettingsEx,

   // region
   // corresponds to tactics entry, don't change.
   ECombineRgn = 500,
   ECreateRectRgn,
   ECreateRectRgnIndirect,
   EEqualRgn,
   EFillRgn,
   EFrameRgn,
   EGetRegionData,
   EGetRgnBox,
   EOffsetRgn,
   EPtInRegion,
   ERectInRegion,
   ESetRectRgn,
   EExtCreateRegion,

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
   ECreatePen,

   // da
   // corresponds to tactics entry, don't change.
   EGetBkColor = 700,
   EGetBkMode,
   EGetTextColor,
   ESetBkColor,
   ESetBkMode,
   ESetTextColor,
   ESetViewportOrgEx,
   ESetStretchBltMode,
   EGetStretchBltMode,
   ESetTextCharacterExtra,
   EGetTextCharacterExtra,
   ESetLayout,
   EGetLayout,
   EGetViewportOrgEx,
   EGetViewportExtEx,
   ESetWindowOrgEx,
   EGetWindowOrgEx,
   EOffsetViewportOrgEx,
   EGetWindowExtEx,

   // dc
   // corresponds to tactics entry, don't change.
   ECreateCompatibleDC = 800,
   EDeleteDC,
   EGetDCOrgEx,
   EGetDeviceCaps,
   ERestoreDC,
   ESaveDC,
   EScrollDC,
   ECreateDC,
   EExtEscape,
   EBitmapEscape,

   // do
   // corresponds to tactics entry, don't change.
   EDeleteObject = 900,
   EGetCurrentObject,
   EGetObject,
   EGetObjectType,
   EGetStockObject,
   ESelectObject,
   EUnrealizeObject,

   // font
   // corresponds to tactics entry, don't change.
   EAddFontResource = 1000,
   ECreateFontIndirect,
   EEnumFonts,
   EEnumFontFamilies,
   EEnumFontFamProc,
   EGetCharABCWidths,
   EGetCharWidth,
   EGetCharWidth32,
   ERemoveFontResource,
   EEnableEUDC,
   ETranslateCharsetInfo,
   EEnumFontFamiliesEx,

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

   // Print
   // corresponds to tactics entry, don't change.
   EStartDoc = 1200,
   EEndDoc,
   EStartPage,
   EEndPage,
   EAbortDoc,
   ESetAbortProc,

   // testchk
   // corresponds to tactics entry, don't change.
   ECheckAllWhite = 1300,
   ECheckScreenHalves,
   EGetReleaseDC,
   EThrash,
   ECheckDriverVerify,

   //Manual
   // corresponds to tactics entry, don't change
   EManualFont = 1400,
   EManualFontClip,
   ELastName,
};


#endif // Globals
