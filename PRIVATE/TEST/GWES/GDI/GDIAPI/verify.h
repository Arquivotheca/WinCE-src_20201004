//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "global.h"

#ifndef __VERIFY_H__
#define __VERIFY_H__

// these are defined regardless of surface verification being turned on or not
HBITMAP myCreateDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, UINT type, BOOL randSize = FALSE);
TESTPROCAPI DriverVerify_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);


#ifdef SURFACE_VERIFY

// HDC wrapper class
class   cTdc;
typedef cTdc *TDC;

// Bitmap wrapper class
class   cTBitmap;
typedef cTBitmap *TBITMAP;

typedef int (WINAPI * PFNGRADIENTFILLINTERNAL)(HDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);
typedef int (WINAPI * PFNSTARTDOCINTERNAL)(HDC hdc, CONST DOCINFO *lpdi);
typedef int (WINAPI * PFNENDDOCINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNSTARTPAGEINTERNAL)(HDC hdc);
typedef int (WINAPI * PFNENDPAGEINTERNAL)(HDC hdc);

// initialization functions called in main.
void    InitVerify();
void    FreeVerify();

// verification control functions
BOOL    SetSurfVerify(BOOL bNewVerify);
inline BOOL DoVerify();
float   SetMaxErrorPercentage(float MaxError);
int PrinterMemCntrl(HDC hdc, int decr);
int PrinterMemCntrl(TDC tdc, int decr);
void OutputBitmap(TDC tdc);
void ResetVerifyDriver();

// overrides of GDI functionality.
TBITMAP myCreateDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, UINT type, BOOL randSize = FALSE);
TDC     myGetDC(HWND hWnd);
BOOL    myReleaseDC(HWND hWnd, TDC tdc);
TBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName);
TBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits);
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
int     DrawTextW(TDC tdc, LPCWSTR lpszStr, int cchStr, RECT * lprc, UINT wFormat);
BOOL    Ellipse(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
int     EnumFontFamiliesW(TDC tdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);
int     EnumFontsW(TDC tdc, LPCWSTR lpszFaceName, FONTENUMPROC lpEnumFontProc, LPARAM lParam);
int     ExcludeClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
BOOL    ExtTextOutW(TDC tdc, int X, int Y, UINT fuOptions, CONST RECT * lprc, LPCWSTR lpszString, UINT cbCount, CONST INT * lpDx);
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
BOOL    GetTextExtentExPointW(TDC tdc, LPCWSTR lpszStr, int cchString, int nMaxExtent, LPINT lpnFit, LPINT alpDx, LPSIZE lpSize);
int     GetTextFaceW(TDC tdc, int nCount, LPWSTR lpFaceName);
BOOL    GetTextMetricsW(TDC tdc, LPTEXTMETRIC lptm);
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
BOOL    RectVisible(TDC tdc, CONST RECT * lprc);
BOOL    SetViewportOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TBITMAP hSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor);
BOOL    ExtEscape(TDC tdc, int iEscape, int cjInput, LPCSTR lpInData, int cjOutput, LPSTR lpOutData);
int    StartDoc(TDC hdc, CONST DOCINFO *lpdi);
int    StartPage(TDC hDC);
int    EndPage(TDC hdc);
int    EndDoc(TDC hdc);

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
void OutputBitmap(HDC tdc);

// these functions do nothing without verification turned on
#define SetSurfVerify(a) 0
#define InitVerify() 0
#define FreeVerify() 0
#define SetMaxErrorPercentage(a) 0
#define PrinterMemCntrl(a,b) 0

#endif // surface verify

#endif // header protection
