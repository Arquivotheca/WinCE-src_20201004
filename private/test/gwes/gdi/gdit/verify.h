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
#include <windows.h>
#include "global.h"

#ifndef __VERIFY_H__
#define __VERIFY_H__

// these are defined regardless of surface verification being turned on or not
HBITMAP myCreateDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, UINT type, struct MYBITMAPINFO *UserBmi, BOOL randSize);
HBITMAP myCreateRGBDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, int nBitMask, DWORD dwCompression, struct MYBITMAPINFO * UserBMI);
TESTPROCAPI DriverVerify_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
// initialization functions called in main.
void    InitVerify();
void    FreeVerify();


#ifdef SURFACE_VERIFY

// HDC wrapper class
class   cTdc;
typedef cTdc *TDC;

// Bitmap wrapper class
class   cTBitmap;
typedef cTBitmap *TBITMAP;

typedef int (WINAPI * PFNGRADIENTFILLINTERNAL)(HDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);
typedef BOOL (WINAPI * PFNALPHABLENDINTERNAL)(HDC hdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC hdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);
typedef int (WINAPI * PFNSTARTDOCINTERNAL)(HDC hdc, CONST DOCINFO *lpdi);
typedef int (WINAPI * PFNENDDOCINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNSTARTPAGEINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNENDPAGEINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNABORTDOCINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNSETABORTPROCINTERNAL)(HDC hdc, ABORTPROC ap);

// verification control functions
BOOL   SetSurfVerify(BOOL bNewVerify);
BOOL   GetSurfVerify();
inline BOOL DoVerify();
void   SetMaxErrorPercentage(DOUBLE MaxError);
void   SetRMSPercentage(UINT Threshold, UINT AvgThreshold, DOUBLE PercentDiff, DOUBLE RMSRed, DOUBLE RMSGreen, DOUBLE RMSBlue);
void   ResetCompareConstraints();
int    PrinterMemCntrl(HDC hdc, int decr);
int    PrinterMemCntrl(TDC tdc, int decr);
BOOL   IsWritableHDC(TDC tdc);
BOOL   IsCompositorEnabled(void);
DWORD  GetBackbufferBPP(void);
void   OutputBitmap(TDC tdc, int nWidth, int nHeight);
void   OutputScreenHalvesBitmap(TDC tdc, int nWidth, int nHeight);
void   SurfaceVerify(TDC tdc, BOOL force, BOOL expected);
void   CompareSurface(TDC tdcActual, TDC tdcExpected);
void   ResetVerifyDriver();

// overrides of GDI functionality.
TBITMAP myCreateDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, UINT type, struct MYBITMAPINFO *UserBmi, BOOL randSize);
TBITMAP myCreateRGBDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, int nBitMask, DWORD dwCompression, struct MYBITMAPINFO * UserBMI);
TBITMAP myCreateRotatedDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, UINT type, struct MYBITMAPINFO *UserBmi, BOOL randSize, int iRotate);
TDC     myGetDC(HWND hWnd);
BOOL    myReleaseDC(HWND hWnd, TDC tdc);
TBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName);
TBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits);
LONG SetBitmapBits(TBITMAP tbmp, DWORD cBytes, CONST VOID *lpBits);
HBRUSH  CreatePatternBrush(TBITMAP hbmp);
DWORD   GetObjectType(TDC tdc);
DWORD   GetObjectType(TBITMAP tbmp);
TBITMAP CreateDIBSection(TDC tdc, CONST BITMAPINFO * pbmi, UINT iUsage, VOID ** ppvBits, HANDLE hSection, DWORD dwOffset);
BOOL ScrollDC(TDC tdc, int dx, int dy, const RECT *lprcScroll, const RECT *lprcClip, HRGN hrgnUpdate, LPRECT lprcUpdate);
BOOL    PatBlt(TDC tdcDest, int nXLeft, int nYLeft, int nWidth, int nHeight, DWORD dwRop);
BOOL    InvertRect(TDC tdcDest, RECT * rc);
BOOL    BitBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop);
BOOL    BitBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop);
BOOL    BitBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop);
TBITMAP CreateCompatibleBitmap(TDC tdc, int nWidth, int nHeight);
TDC     CreateCompatibleDC(TDC tdc);
TDC     TCreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData);
BOOL    DeleteDC(TDC tdc);
BOOL    DrawEdge(TDC tdc, LPRECT lprc, UINT uEdgeType, UINT uFlags);
BOOL    DrawFocusRect(TDC tdc, CONST RECT * lprc);
int     DrawText(TDC tdc, LPCWSTR lpszStr, int cchStr, RECT * lprc, UINT wFormat);
BOOL    Ellipse(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
int     EnumFontFamilies(TDC tdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);
int     EnumFontFamiliesEx(TDC tdc, LPLOGFONT lpLogFont, FONTENUMPROC lpEnumFontFamExProc, LPARAM lParam, DWORD dwFlags);
int     EnumFonts(TDC tdc, LPCWSTR lpszFaceName, FONTENUMPROC lpEnumFontProc, LPARAM lParam);
int     ExcludeClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
BOOL    ExtTextOut(TDC tdc, int X, int Y, UINT fuOptions, CONST RECT * lprc, LPCWSTR lpszString, UINT cbCount, CONST INT * lpDx);
UINT    SetTextAlign(TDC tdc, UINT fMode);
UINT    GetTextAlign(TDC tdc);
int     FillRect(TDC tdc, CONST RECT * lprc, HBRUSH hbr);
BOOL    GradientFill(TDC tdc, PTRIVERTEX pVertex, ULONG dwNumVertex,  PVOID pMesh, ULONG dwNumMesh,   ULONG dwMode);
COLORREF GetBkColor(TDC tdc);
int     GetBkMode(TDC tdc);
int     GetClipRgn(TDC tdc, HRGN hrgn);
int     GetClipBox(TDC tdc, LPRECT lprc);
HGDIOBJ GetCurrentObject(TDC tdc, UINT uObjectType);
int     GetDeviceCaps(TDC tdc, int nIndex);
COLORREF GetNearestColor(TDC tdc, COLORREF crColor);
COLORREF GetPixel(TDC tdc, int nXPos, int nYPos);
COLORREF GetTextColor(TDC tdc);
BOOL    GetTextExtentExPoint(TDC tdc, LPCWSTR lpszStr, int cchString, int nMaxExtent, LPINT lpnFit, LPINT alpDx, LPSIZE lpSize);
int     GetTextFace(TDC tdc, int nCount, LPWSTR lpFaceName);
BOOL    GetTextMetrics(TDC tdc, LPTEXTMETRIC lptm);
BOOL    GetCharWidth32(TDC tdc, UINT iFirstChar, UINT iLastChar, LPINT lpBuffer);
BOOL    GetCharABCWidths(TDC tdc, UINT iFirstChar, UINT iLastChar, LPABC lpBuffer);
int     IntersectClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
BOOL    MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
                int xMask, int yMask, DWORD dwRop);
BOOL    MoveToEx(TDC tdc, int X, int Y, LPPOINT lpPoint);
BOOL    LineTo(TDC tdc, int nXEnd, int nYEnd);
BOOL    GetCurrentPositionEx(TDC tdc, LPPOINT lpPoint);
BOOL    Polygon(TDC tdc, CONST POINT * lpPoints, int nCount);
BOOL    Polyline(TDC tdc, CONST POINT * lppt, int cPoints);
BOOL    Rectangle(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
BOOL    RestoreDC(TDC tdc, int nSavedDC);
BOOL    RoundRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect, int nWidth, int nHeight);
int     SaveDC(TDC tdc);
int     SelectClipRgn(TDC tdc, HRGN hrgn);
HGDIOBJ SelectObject(TDC tdc, HGDIOBJ hgdiobj);
TBITMAP SelectObject(TDC tdc, TBITMAP hgdiobj);
int     GetObject(TBITMAP hgdiobj, int cbBuffer, LPVOID lpvObject);
BOOL    DeleteObject(TBITMAP hObject);
COLORREF SetBkColor(TDC tdc, COLORREF crColor);
int     SetBkMode(TDC tdc, int iBkMode);
BOOL    SetBrushOrgEx(TDC tdc, int nXOrg, int nYOrg, LPPOINT lppt);
BOOL    SetStretchBltMode(TDC tdc, int nMode);
BOOL    GetStretchBltMode(TDC tdc);
BOOL    SetTextCharacterExtra(TDC tdc, int nExtra);
BOOL    GetTextCharacterExtra(TDC tdc);
DWORD   GetLayout(TDC tdc);
DWORD   SetLayout(TDC tdc, DWORD dwLayout);
COLORREF SetPixel(TDC tdc, int X, int Y, COLORREF crColor);
COLORREF SetTextColor(TDC tdc, COLORREF crColor);
BOOL    StretchBlt(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc,
                   int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop);
BOOL    StretchBlt(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC tdcSrc,
                   int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop);
BOOL    StretchBlt(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc,
                   int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop);
int     StretchDIBits(TDC tdc, int XDest, int YDest, int nDestWidth, int nDestHeight, int XSrc, int YSrc, int nSrcWidth,
                      int nSrcHeight, CONST VOID * lpBits, CONST BITMAPINFO * lpBitsInfo, UINT iUsage, DWORD dwRop);
int     SetDIBitsToDevice(TDC tdc, int XDest, int YDest, DWORD dwWidth, DWORD dwHeight, int XSrc, int YSrc,
                          UINT uStartScan, UINT cScanLines, CONST VOID *lpvBits, CONST BITMAPINFO *lpbmi, UINT fuColorUse);
HPALETTE SelectPalette(TDC tdc, HPALETTE hPal, BOOL bForceBackground);
UINT    RealizePalette(TDC tdc);
UINT    GetSystemPaletteEntries(TDC tdc, UINT iStart, UINT nEntries, LPPALETTEENTRY pPe);
UINT    GetDIBColorTable(TDC tdc, UINT uStartIndex, UINT cEntries, RGBQUAD * pColors);
UINT    SetDIBColorTable(TDC tdc, UINT uStartIndex, UINT cEntries, CONST RGBQUAD * pColors);
BOOL    FillRgn(TDC tdc, HRGN hrgn, HBRUSH hbr);
int     SetROP2(TDC tdc, int fnDrawMode);
int     GetROP2(TDC tdc);
BOOL    RectVisible(TDC tdc, CONST RECT * lprc);
BOOL    SetViewportOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint);
BOOL    GetViewportOrgEx(TDC tdc, LPPOINT lpPoint);
BOOL    GetViewportExtEx(TDC tdc, LPSIZE lpSize);
BOOL    SetWindowOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint);
BOOL    GetWindowOrgEx(TDC tdc, LPPOINT lpPoint);
BOOL    OffsetViewportOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint);
BOOL    GetWindowExtEx(TDC tdc, LPSIZE lpSize);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TBITMAP hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TBITMAP hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HBITMAP hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    ExtEscape(TDC tdc, int iEscape, int cjInput, LPCSTR lpInData, int cjOutput, LPSTR lpOutData);
BOOL    BitmapEscape(TBITMAP hbmp, int iEscape, int cjInput, LPCSTR lpszInData, int cjOutput, LPSTR lpszOutData);
int    StartDoc(TDC hdc, CONST DOCINFO *lpdi);
int    StartPage(TDC hDC);
int    EndPage(TDC hdc);
int    EndDoc(TDC hdc);
int    AbortDoc(TDC hdc);
int    SetAbortProc(TDC hdc, ABORTPROC ap);

BOOL AlphaBlend(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);

#else
// surface verify not defined

// no verification, so TDC's are the same as HDC's
typedef HDC TDC;
typedef HBITMAP TBITMAP;

// use the standard versions of these functions, defined in verify.cpp at the bottom.
HBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName);
HBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits);
HDC     myGetDC(HWND hWnd);
BOOL    myReleaseDC(HWND hWnd, HDC hdc);
HDC     TCreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData);
BOOL    IsWritableHDC(HDC hdc);
BOOL    IsCompositorEnabled(void);
DWORD   GetBackbufferBPP(void);
void    OutputBitmap(HDC tdc, int nWidth, int nHeight);
void    OutputScreenHalvesBitmap(HDC tdc, int nWidth, int nHeight);
void    SurfaceVerify(TDC tdc, BOOL force, BOOL expected);
void    CompareSurface(TDC tdcActual, TDC tdcExpected);
void    ResetVerifyDriver();

// these functions do nothing without verification turned on
#define SetSurfVerify(a) 0
#define GetSurfVerify() 0
#define SetMaxErrorPercentage(a) 0
#define SetRMSPercentage(a, b, c, d, e, f) 0
#define ResetCompareConstraints() 0
#define PrinterMemCntrl(a,b) 0

#endif // surface verify

#endif // header protection
