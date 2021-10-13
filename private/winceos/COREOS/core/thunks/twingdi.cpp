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
#include "limits.h"
#include <intsafe.h>
#include <strsafe.h>

#include <GweApiSet1.hpp>

extern GweApiSet1_t *pGweApiSet1Entrypoints;
extern GweApiSet2_t *pGweApiSet2Entrypoints;

// Enable CELOG info for all but ship builds.
#ifndef SHIP_BUILD
#define CELOG_GDI
#endif

#ifdef RDPGWELIB
#undef CELOG_GDI
#endif

#ifdef CELOG_GDI
#include "celog.h"

class GdiCeLog_t
{
    CEL_GDI_INFO    m_CELGdiInfo;

public:

    GdiCeLog_t(
        DWORD   op,
        DWORD   c1,
        DWORD   c2,
        DWORD   c3,
        DWORD   c4
        )
    {
    if ( IsCeLogZoneEnabled(CELZONE_GDI) )
        {
        LARGE_INTEGER liPerf;
        m_CELGdiInfo.dwGDIOp    = op;
        m_CELGdiInfo.dwContext  = (DWORD)c1;
        m_CELGdiInfo.dwContext2 = (DWORD)c2;
        m_CELGdiInfo.dwContext3 = (DWORD)c3;
        m_CELGdiInfo.dwContext4 = (DWORD)c4;
        QueryPerformanceCounter(&liPerf);
        m_CELGdiInfo.dwEntryTime = (DWORD)liPerf.LowPart;
        }
    }

    ~GdiCeLog_t(
        void
        )
    {
    if ( IsCeLogZoneEnabled(CELZONE_GDI) )
        {
        // Entry time is in the struct, exit time is logged by CeLog
        CELOGDATA(TRUE, CELID_GDI, &m_CELGdiInfo, sizeof(m_CELGdiInfo), 0, CELZONE_GDI);
        }
    }


};

#define CELOG_GDIEnter(op,c1, c2, c3, c4)   GdiCeLog_t(CEL_GDI_##op,  (DWORD)c1, (DWORD)c2, (DWORD)c3, (DWORD)c4)

//#define CELOG_GDIEnter(op,c1, c2, c3, c4) \
//  CEL_GDI_INFO    CELGdiInfo; \
//  LARGE_INTEGER   liPerf;     \
//    if (IsCeLogZoneEnabled(CELZONE_GDI)) { \
//        CELGdiInfo.dwGDIOp = CEL_GDI_##op; \
//        CELGdiInfo.dwTimeSpent = (DWORD)-1; \
//        CELGdiInfo.dwContext = (DWORD)c1; \
//        CELGdiInfo.dwContext2 = (DWORD)c2; \
//        CELGdiInfo.dwContext3 = (DWORD)c3; \
//        CELGdiInfo.dwContext4 = (DWORD)c4; \
//        QueryPerformanceCounter (&liPerf); \
//        CELOGDATA (TRUE, CELID_GDI, &CELGdiInfo, sizeof(CELGdiInfo), 0, CELZONE_GDI); \
//    }
//#define CELOG_GDILeave()  \
//    if (IsCeLogZoneEnabled(CELZONE_GDI)) { \
//        CELGdiInfo.dwTimeSpent = liPerf.LowPart; \
//        QueryPerformanceCounter (&liPerf); \
//        CELGdiInfo.dwTimeSpent = liPerf.LowPart - CELGdiInfo.dwTimeSpent; \
//        CELOGDATA (TRUE, CELID_GDI, &CELGdiInfo, sizeof(CELGdiInfo), 0, CELZONE_GDI); \
//    }

#else

#define CELOG_GDIEnter(op,c1, c2, c3, c4)


#endif

// SDK exports
int
AddFontResourceW(
    __in const   WCHAR*  lpszFilename
    )
{
    return pGweApiSet2Entrypoints->m_pAddFontResourceW(lpszFilename);
}

BOOL
PatBlt(
    HDC     hdcDest,
    int     nXLeft,
    int     nYLeft,
    int     nWidth,
    int     nHeight,
    DWORD   dwRop
    )
{
    CELOG_GDIEnter(PatBlt, hdcDest, dwRop, 0, 0);
    return pGweApiSet2Entrypoints->m_pPatBlt(hdcDest,nXLeft,nYLeft,nWidth,nHeight, dwRop);
}

BOOL
InvertRect(
    HDC     hdc,
    __in const   RECT*   lprc
    )
{
    CELOG_GDIEnter(InvertRect, hdc, lprc,0,0);
    BOOL bRet = pGweApiSet2Entrypoints->m_pInvertRect(hdc,lprc,sizeof(RECT));
    return bRet;
}

BOOL
BitBlt(
    HDC     hdcDest,
    int     nXDest,
    int     nYDest,
    int     nWidth,
    int     nHeight,
    HDC     hdcSrc,
    int     nXSrc,
    int     nYSrc,
    DWORD   dwRop
    )
{
    CELOG_GDIEnter(BitBlt, hdcDest, hdcSrc, dwRop, 0);
    return pGweApiSet2Entrypoints->m_pBitBlt(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc, dwRop);
}

extern "C"
int
CombineRgn(
    HRGN    hrgnDest,
    HRGN    hrgnSrc1,
    HRGN    hrgnSrc2,
    int     fnCombineMode
    )
{
    CELOG_GDIEnter(CombineRgn, hrgnDest, hrgnSrc1, hrgnSrc2, fnCombineMode);
    return pGweApiSet2Entrypoints->m_pCombineRgn(hrgnDest,hrgnSrc1,hrgnSrc2,fnCombineMode);
}

HBITMAP
CreateBitmap(
        int     nWidth,
        int     nHeight,
        UINT    cPlanes,
        UINT    cBitsPerPel,
        __in_opt const   void*   lpvBits
        )
{
    HBITMAP hbmp = NULL;

    size_t cbBits = 0;

    if (NULL != lpvBits)
        {
        unsigned int nWidthLocal = 0;
        unsigned int nHeightLocal = 0;

        if ( FAILED(IntToUInt(nWidth, &nWidthLocal)) ||
             FAILED(IntToUInt(nHeight, &nHeightLocal)) ||
             FAILED(UIntMult(nWidthLocal, cBitsPerPel, &cbBits)) ||
             FAILED(UIntAdd(cbBits, 15, &cbBits)) ||
             FAILED(UIntMult((cbBits>>4)*2, nHeightLocal, &cbBits)) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }
        }

    CELOG_GDIEnter(CreateBitmap, nWidth, nHeight, cPlanes, cBitsPerPel);
    hbmp = pGweApiSet2Entrypoints->m_pCreateBitmap(nWidth,nHeight, cPlanes, cBitsPerPel, lpvBits, cbBits);

leave:
    return hbmp;
}

HBITMAP
CreateCompatibleBitmap(
    HDC hdc,
    int nWidth,
    int nHeight
    )
{
    CELOG_GDIEnter(CreateCompatibleBitmap, hdc, nWidth, nHeight, 0);
    return pGweApiSet2Entrypoints->m_pCreateCompatibleBitmap(hdc, nWidth, nHeight);
}

LONG
SetBitmapBits(
    HBITMAP hbmp,
    DWORD   cBytes,
    __in_bcount(cBytes) const   void*   lpBits
    )
{
    return pGweApiSet2Entrypoints->m_pSetBitmapBits(hbmp,lpBits,cBytes);
}

extern "C"
HDC
CreateCompatibleDC(
    HDC hdc
    )
{
    CELOG_GDIEnter(CreateCompatibleDC, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreateCompatibleDC(hdc);
}

HBRUSH
CreateDIBPatternBrushPt(
    const   void*   lpPackedDIB,
            UINT    iUsage
    )
{
    // lpPackedDIB is surely larger than sizeof(BITMAPINFO), but the thorough validation will be
    // performed inside the API. Only the first sizeof(BITMAPINFOHEADER) bytes need to be verified
    // in order for the validation function to access the header safely.
    return pGweApiSet2Entrypoints->m_pCreateDIBPatternBrushPt(lpPackedDIB, sizeof(BITMAPINFO), iUsage);
}

extern "C"
HFONT
CreateFontIndirectW(
    __in const   LOGFONT*    lplf
    )
{
    CELOG_GDIEnter(CreateFontIndirectW, lplf, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreateFontIndirectW(lplf, sizeof(LOGFONT));
}

extern "C"
HRGN
CreateRectRgnIndirect(
    __in const   RECT*   lprc
    )
{
    CELOG_GDIEnter(CreateRectRgnIndirect, lprc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreateRectRgnIndirect(lprc, sizeof(RECT));
}

HPEN
CreatePenIndirect(
    __in const   LOGPEN* lplgpn
    )
{
    CELOG_GDIEnter(CreatePenIndirect, lplgpn, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreatePenIndirect(lplgpn, sizeof(LOGPEN));
}


extern "C"
HBRUSH
CreateSolidBrush(
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(CreateSolidBrush, crColor, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreateSolidBrush(crColor);
}


extern "C"
BOOL
DeleteDC(
    HDC hdc
    )
{
    CELOG_GDIEnter(DeleteDC, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pDeleteDC(hdc);
}

extern "C"
BOOL
DeleteObject(
    HGDIOBJ hObject
    )
{
    CELOG_GDIEnter(DeleteObject, hObject, 0, 0, 0);
    LPVOID pvDIBUserMem = NULL;
    BOOL bRet;
    bRet = pGweApiSet2Entrypoints->m_pDeleteObject(hObject, &pvDIBUserMem);
    if (bRet && pvDIBUserMem)
    {
        LocalFree(pvDIBUserMem);
    }
    return bRet;
}

BOOL
DrawEdge(
    HDC     hdc,
    __in RECT*   lprc,
    UINT    uEdgeType,
    UINT    uFlags
    )
{
    CELOG_GDIEnter(DrawEdge, hdc, lprc, uEdgeType, uFlags);
    return pGweApiSet2Entrypoints->m_pDrawEdge(hdc, lprc, sizeof(RECT), uEdgeType, uFlags);
}

BOOL
DrawFocusRect(
    HDC     hdc,
    __in const   RECT*   lprc
    )
{
    CELOG_GDIEnter(DrawFocusRect, hdc, lprc, 0, 0);
    return pGweApiSet2Entrypoints->m_pDrawFocusRect(hdc,lprc, sizeof(RECT));
}

extern "C"
int
DrawTextW(
    HDC     hdc,
    __in_ecount(cchStr) const   WCHAR*  lpszStr,
    int     cchStr,
    __in RECT*   lprc,
    UINT    wFormat
    )
{
    int nReturnValue = 0;
    unsigned int cchStrLocal = 0;
    size_t cbStr = 0;

    if (cchStr >= 0)
        {
        if ( FAILED(IntToUInt(cchStr, &cchStrLocal)) ||
             FAILED(UIntMult(cchStrLocal, sizeof(WCHAR), &cbStr)) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }
        }
    else if ( FAILED(StringCbLengthW(lpszStr, STRSAFE_MAX_CCH * sizeof(WCHAR), &cbStr)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(DrawTextW, hdc, lpszStr, cchStr, wFormat);
    nReturnValue = pGweApiSet2Entrypoints->m_pDrawTextW(hdc,lpszStr,cbStr,cchStr,lprc,sizeof(RECT),wFormat);

leave:
    return nReturnValue;
}

BOOL
Ellipse(
    HDC hdc,
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect
    )
{
    CELOG_GDIEnter(Ellipse, hdc, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pEllipse(hdc, nLeftRect, nTopRect, nRightRect, nBottomRect);
}

int
EnumFontFamiliesW(
    HDC             hdc,
    __in_opt const   WCHAR*          lpszFamily,
    FONTENUMPROC    lpEnumFontFamProc,
    LPARAM          lParam
    )
{
    CELOG_GDIEnter(EnumFontFamiliesW, hdc, lpszFamily, lpEnumFontFamProc, lParam);
    return pGweApiSet2Entrypoints->m_pEnumFontFamiliesW(hdc, lpszFamily, lpEnumFontFamProc, lParam);
}

int
EnumFontsW(
    HDC             hdc,
    __in_opt const   WCHAR*          lpszFaceName,
    FONTENUMPROC    lpEnumFontProc,
    LPARAM          lParam
    )
{
    CELOG_GDIEnter(EnumFontsW, hdc, lpszFaceName, lpEnumFontProc, lParam);
    return pGweApiSet2Entrypoints->m_pEnumFontsW(hdc, lpszFaceName, lpEnumFontProc, lParam);
}

int
ExcludeClipRect(
    HDC hdc,
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect
    )
{
    CELOG_GDIEnter(ExcludeClipRect, hdc, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pExcludeClipRect(hdc,nLeftRect,nTopRect,nRightRect,nBottomRect);
}

extern "C"
BOOL
ExtTextOutW(
    HDC     hdc,
    int     X,
    int     Y,
    UINT    fuOptions,
    __in_opt const   RECT*   lprc,
    __in_ecount(cchCount) const   WCHAR*  lpszString,
    UINT    cchCount,
    const   INT*    lpDx
    )
{
    BOOL fReturnValue = FALSE;

    size_t cbszString = 0;
    size_t cbDx = 0;

    if ( FAILED(UIntMult(cchCount, sizeof(WCHAR), &cbszString)) ||
         FAILED(UIntMult(cchCount, sizeof(INT), &cbDx)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(ExtTextOutW, hdc, X, Y, fuOptions);
    fReturnValue = pGweApiSet2Entrypoints->m_pExtTextOutW(
                                            hdc,
                                            X,
                                            Y,
                                            fuOptions,
                                            lprc,
                                            sizeof(RECT),
                                            lpszString,
                                            cbszString,
                                            cchCount,
                                            lpDx,
                                            cbDx
                                            );

leave:
    return fReturnValue;
}

UINT
SetTextAlign(
    HDC     hdc,
    UINT    fMode
    )
{
    CELOG_GDIEnter(SetTextAlign, hdc, fMode, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetTextAlign(hdc,fMode);
}

UINT
GetTextAlign(
    HDC hdc
    )
{
    CELOG_GDIEnter(GetTextAlign, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetTextAlign(hdc);
}

extern "C"
int
FillRect(
    HDC     hdc,
    __in const   RECT*   lprc,
    HBRUSH  hbr
    )
{
    CELOG_GDIEnter(FillRect, hdc, lprc, hbr, 0);
    return pGweApiSet2Entrypoints->m_pFillRect(hdc,lprc,sizeof(RECT),hbr);
}

COLORREF
GetBkColor(
    HDC hdc
    )
{
    CELOG_GDIEnter(GetBkColor, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetBkColor(hdc);
}

int
GetBkMode(
    HDC hdc
    )
{
    CELOG_GDIEnter(GetBkMode, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetBkMode(hdc);
}

int
GetClipRgn(
    HDC     hdc,
    HRGN    hrgn
    )
{
    CELOG_GDIEnter(GetClipRgn, hdc, hrgn, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetClipRgn(hdc,hrgn);
}

int
GetClipBox(
    HDC     hdc,
    __out RECT*   lprc
    )
{
    CELOG_GDIEnter(GetClipBox, hdc, lprc, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetClipBox(hdc,lprc,sizeof(RECT));
}

HGDIOBJ
GetCurrentObject(
    HDC     hdc,
    UINT    uObjectType
    )
{
    CELOG_GDIEnter(GetCurrentObject, hdc, uObjectType, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetCurrentObject(hdc, uObjectType);
}

int
GetDeviceCaps(
    HDC hdc,
    int nIndex
    )
{
    CELOG_GDIEnter(GetDeviceCaps, hdc, nIndex, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetDeviceCaps(hdc,nIndex);
}

COLORREF
GetNearestColor(
    HDC         hdc,
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(GetNearestColor, hdc, crColor, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetNearestColor(hdc,crColor);
}

extern "C"
int
GetObjectW(
    HGDIOBJ hgdiobj,
    int     cbBuffer,
    __out_bcount_opt(cbBuffer) void*   lpvObject
    )
{
    int nReturnValue = 0;

    if (cbBuffer < 0)
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetObjectW, hgdiobj, cbBuffer, lpvObject, 0);
    nReturnValue = pGweApiSet2Entrypoints->m_pGetObjectW(hgdiobj,lpvObject,cbBuffer);

leave:
    return nReturnValue;
}

DWORD
GetObjectType(
    HGDIOBJ hgdiobj
    )
{
    CELOG_GDIEnter(GetObjectType, hgdiobj, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetObjectType(hgdiobj);
}

COLORREF
GetPixel(
    HDC hdc,
    int nXPos,
    int nYPos
    )
{
    CELOG_GDIEnter(GetPixel, hdc, nXPos, nYPos, 0);
    return pGweApiSet2Entrypoints->m_pGetPixel(hdc,nXPos,nYPos);
}

DWORD
GetRegionData(
    HRGN        hRgn,
    DWORD       dwCount,
    __out_bcount_opt(dwCount) RGNDATA*    lpRgnData
    )
{
    CELOG_GDIEnter(GetRegionData, hRgn, dwCount, lpRgnData, 0);
    return pGweApiSet2Entrypoints->m_pGetRegionData(hRgn, lpRgnData, dwCount);
}

HBRUSH
GetSysColorBrush(
    int nIndex
    )
{
    CELOG_GDIEnter(GetSysColorBrush, nIndex, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetSysColorBrush(nIndex);
}

int
GetRgnBox(
    HRGN    hrgn,
    __out RECT*   lprc
    )
{
    CELOG_GDIEnter(GetRgnBox, hrgn, lprc, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetRgnBox(hrgn,lprc,sizeof(RECT));
}

HGDIOBJ
GetStockObject(
    int fnObject
    )
{
    CELOG_GDIEnter(GetStockObject, fnObject, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetStockObject(fnObject);
}

COLORREF
GetTextColor(
    HDC hdc
    )
{
    CELOG_GDIEnter(GetTextColor, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetTextColor(hdc);
}


extern "C"
BOOL
GetTextExtentExPointW(
    HDC     hdc,
    __in_ecount(cchString) const   WCHAR*  lpszStr,
    int     cchString,
    int     nMaxExtent,
    __out_opt int*    lpnFit,
    __out_ecount_opt(cchString) int*    alpDx,
    __out SIZE*   lpSize
    )
{
    BOOL fReturnValue = FALSE;

    unsigned int cchStringLocal = 0;
    size_t cbStr = 0;
    size_t cbDx = 0;

    if ( FAILED(IntToUInt(cchString, &cchStringLocal)) ||
         FAILED(UIntMult(cchStringLocal, sizeof(WCHAR), &cbStr)) ||
         FAILED(UIntMult(cchStringLocal, sizeof(INT), &cbDx)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetTextExtentExPointW, hdc, lpszStr, cchString, nMaxExtent);
    fReturnValue = pGweApiSet2Entrypoints->m_pGetTextExtentExPointW(
                                                    hdc,
                                                    lpszStr,
                                                    cbStr,
                                                    cchString,
                                                    nMaxExtent,
                                                    lpnFit,
                                                    alpDx,
                                                    cbDx,
                                                    lpSize,
                                                    sizeof(SIZE)
                                                    );

leave:
    return fReturnValue;
}

int
GetTextFaceW(
    HDC     hdc,
    int     nCount,
    __out_ecount_opt(nCount) WCHAR*  lpFaceName
    )
{
    int nReturnValue = 0;

    unsigned int nCountLocal = 0;
    size_t  cbFaceName = 0;

    if ( FAILED(IntToUInt(nCount, &nCountLocal)) ||
         FAILED(UIntMult(nCountLocal, sizeof(WCHAR), &cbFaceName)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetTextFaceW, hdc, nCount, lpFaceName, 0);
    nReturnValue = pGweApiSet2Entrypoints->m_pGetTextFaceW(hdc,nCount, lpFaceName, cbFaceName);

leave:
    return nReturnValue;
}

BOOL
GetTextMetricsW(
    HDC         hdc,
    __out TEXTMETRIC* lptm
    )
{
    CELOG_GDIEnter(GetTextMetricsW, hdc, lptm, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetTextMetricsW(hdc, lptm, sizeof(TEXTMETRIC));
}

UINT
GetOutlineTextMetricsW(
    HDC         hdc,
    UINT        cbData,
    __out_bcount_opt(cbData) LPOUTLINETEXTMETRICW lpOTM
    )
{
    return pGweApiSet2Entrypoints->m_pGetOutlineTextMetricsW(hdc, lpOTM, cbData);
}

DWORD
GetFontData(
    HDC hdc,
    DWORD   dwTable,
    DWORD   dwOffset,
    __out_bcount(cbData) LPVOID  lpvBuffer,
    DWORD   cbData
    )
{
    return pGweApiSet2Entrypoints->m_pGetFontData(hdc, dwTable, dwOffset, lpvBuffer, cbData);
}

BOOL
GetCharWidth32(
    HDC     hdc,
    UINT    iFirstChar,
    UINT    iLastChar,
    __out int*    lpBuffer
    )
{
    BOOL fReturnValue = FALSE;

    size_t cbBuffer = 0;

    if ( FAILED(UIntSub(iLastChar, iFirstChar, &cbBuffer)) ||
         FAILED(UIntAdd(cbBuffer, 1, &cbBuffer)) ||
         FAILED(UIntMult(cbBuffer, sizeof(INT), &cbBuffer)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetCharWidth32, hdc, iFirstChar, iLastChar, lpBuffer);
    fReturnValue = pGweApiSet2Entrypoints->m_pGetCharWidth32(hdc,iFirstChar,iLastChar,lpBuffer,cbBuffer);

leave:
    return fReturnValue;
}

BOOL
GetCharABCWidths(
    HDC     hdc,
    UINT    uFirstChar,
    UINT    uLastChar,
    __out ABC*    lpabc
    )
{
    size_t cbabc = 0;

    if ( FAILED(UIntSub(uLastChar, uFirstChar, &cbabc)) ||
         FAILED(UIntAdd(cbabc, 1, &cbabc)) ||
         FAILED(UIntMult(cbabc, sizeof(ABC), &cbabc)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetCharABCWidths, hdc, uFirstChar, uLastChar, lpabc);
    cbabc = pGweApiSet2Entrypoints->m_pGetCharABCWidths(hdc,uFirstChar,uLastChar,lpabc,cbabc);

leave:
    return cbabc;
}

BOOL
GetCharABCWidthsI(
    HDC hdc,
    UINT giFirst,
    UINT cgi,
    __in_ecount_opt(cgi) LPWORD pgi,
    __out_ecount(cgi) LPABC lpabc
    )
{
    BOOL fReturnValue = FALSE;

    size_t cbgi = 0;
    size_t cbabc = 0;

    if ( FAILED(UIntMult(cgi, sizeof(WORD), &cbgi)) ||
         FAILED(UIntMult(cgi, sizeof(ABC), &cbabc)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet2Entrypoints->m_pGetCharABCWidthsI(hdc, giFirst, cgi, pgi, cbgi, lpabc, cbabc);

leave:
    return fReturnValue;
}

int
IntersectClipRect(
    HDC hdc,
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect
    )
{
    CELOG_GDIEnter(IntersectClipRect, hdc, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pIntersectClipRect(hdc,nLeftRect,nTopRect,nRightRect,nBottomRect);
}

BOOL
MaskBlt(
    HDC     hdcDest,
    int     nXDest,
    int     nYDest,
    int     nWidth,
    int     nHeight,
    HDC     hdcSrc,
    int     nXSrc,
    int     nYSrc,
    HBITMAP hbmMask,
    int     xMask,
    int     yMask,
    DWORD   dwRop
    )
{
    CELOG_GDIEnter(MaskBlt, hdcDest, hdcSrc, dwRop, 0);
    return pGweApiSet2Entrypoints->m_pMaskBlt(
                    hdcDest,
                    nXDest,
                    nYDest,
                    nWidth,
                    nHeight,
                    hdcSrc,
                    nXSrc,
                    nYSrc,
                    hbmMask,
                    xMask,
                    yMask,
                    dwRop
                    );
}

int
OffsetRgn(
    HRGN    hrgn,
    int     nXOffset,
    int     nYOffset
    )
{
    CELOG_GDIEnter(OffsetRgn, hrgn, nXOffset, nYOffset, 0);
    return pGweApiSet2Entrypoints->m_pOffsetRgn(hrgn,nXOffset,nYOffset);
}

BOOL
MoveToEx(
    HDC     hdc,
    int     X,
    int     Y,
    __out_opt POINT*  lpPoint
    )
{
    CELOG_GDIEnter(MoveToEx, hdc, X, Y, lpPoint);
    return pGweApiSet2Entrypoints->m_pMoveToEx(hdc,X,Y,lpPoint,sizeof(POINT));
}

BOOL
LineTo(
    HDC hdc,
    int nXEnd,
    int nYEnd
    )
{
    CELOG_GDIEnter(LineTo, hdc, nXEnd, nYEnd, 0);
    return pGweApiSet2Entrypoints->m_pLineTo(hdc,nXEnd,nYEnd);
}

BOOL
GetCurrentPositionEx(
    HDC     hdc,
    __out POINT*  lpPoint
    )
{
    CELOG_GDIEnter(GetCurrentPositionEx, hdc, lpPoint, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetCurrentPositionEx(hdc,lpPoint,sizeof(POINT));
}

BOOL
Polygon(
    HDC     hdc,
    __in_ecount(nCount) const   POINT*  lpPoints,
    int     nCount
    )
{
    BOOL fReturnValue = FALSE;

    unsigned int nCountLocal = 0;
    size_t cbPoints = 0;

    if ( FAILED(IntToUInt(nCount, &nCountLocal)) ||
         FAILED(UIntMult(nCountLocal, sizeof(POINT), &cbPoints)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(Polygon, hdc, lpPoints, nCount, 0);
    fReturnValue = pGweApiSet2Entrypoints->m_pPolygon(hdc,lpPoints,cbPoints,nCount);

leave:
    return fReturnValue;
}

extern "C"
BOOL
Polyline(
    HDC     hdc,
    __in_ecount(cPoints) const   POINT*  lppt,
    int     cPoints
    )
{
    BOOL fReturnValue = FALSE;

    unsigned int cPointsLocal = 0;
    size_t cbPoints = 0;

    if ( FAILED(IntToUInt(cPoints, &cPointsLocal)) ||
         FAILED(UIntMult(cPointsLocal, sizeof(POINT), &cbPoints)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(Polyline, hdc, lppt, cPoints, 0);
    fReturnValue = pGweApiSet2Entrypoints->m_pPolyline(hdc,lppt,cbPoints,cPoints);

leave:
    return fReturnValue;
}

BOOL
PtInRegion(
    HRGN    hrgn,
    int     X,
    int     Y
    )
{
    CELOG_GDIEnter(PtInRegion, hrgn, X, Y, 0);
    return pGweApiSet2Entrypoints->m_pPtInRegion(hrgn,X,Y);
}

BOOL
Rectangle(
    HDC hdc,
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect
    )
{
    CELOG_GDIEnter(Rectangle, hdc, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pRectangle(hdc,nLeftRect,nTopRect,nRightRect,nBottomRect);
}

BOOL
RectInRegion(
    HRGN    hrgn,
    __in const   RECT*   lprc
    )
{
    CELOG_GDIEnter(RectInRegion, hrgn, lprc, 0, 0);
    return pGweApiSet2Entrypoints->m_pRectInRegion(hrgn,lprc,sizeof(RECT));
}

BOOL
RemoveFontResourceW(
    __in const   WCHAR*  lpFileName
    )
{
    CELOG_GDIEnter(RemoveFontResourceW, lpFileName, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pRemoveFontResourceW(lpFileName);
}

BOOL
RestoreDC(
    HDC hdc,
    int nSavedDC
    )
{
    CELOG_GDIEnter(RestoreDC, hdc, nSavedDC, 0, 0);
    return pGweApiSet2Entrypoints->m_pRestoreDC(hdc,nSavedDC);
}

BOOL
RoundRect(
    HDC hdc,
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect,
    int nWidth,
    int nHeight
    )
{
    CELOG_GDIEnter(RoundRect, hdc, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pRoundRect(
                    hdc,
                    nLeftRect,
                    nTopRect,
                    nRightRect,
                    nBottomRect,
                    nWidth,
                    nHeight
                    );
}

int
SaveDC(
    HDC hdc
    )
{
    CELOG_GDIEnter(SaveDC, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pSaveDC(hdc);
}

int
SelectClipRgn(
    HDC     hdc,
    HRGN    hrgn
    )
{
    CELOG_GDIEnter(SelectClipRgn, hdc, hrgn, 0, 0);
    return pGweApiSet2Entrypoints->m_pSelectClipRgn(hdc,hrgn);
}

extern "C"
HGDIOBJ
SelectObject(
    HDC     hdc,
    HGDIOBJ hgdiobj
    )
{
    CELOG_GDIEnter(SelectObject, hdc, hgdiobj, 0, 0);
    return pGweApiSet2Entrypoints->m_pSelectObject(hdc,hgdiobj);
}

COLORREF
SetBkColor(
    HDC         hdc,
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(SetBkColor, hdc, crColor, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetBkColor(hdc, crColor);
}

extern "C"
int
SetBkMode(
    HDC hdc,
    int iBkMode
    )
{
    CELOG_GDIEnter(SetBkMode, hdc, iBkMode, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetBkMode(hdc,iBkMode);
}


BOOL
SetBrushOrgEx(
    HDC     hdc,
    int     nXOrg,
    int     nYOrg,
    __out_opt POINT*  lppt
    )
{
    CELOG_GDIEnter(SetBrushOrgEx, hdc, nXOrg, nYOrg, lppt);
    return pGweApiSet2Entrypoints->m_pSetBrushOrgEx(hdc,nXOrg,nYOrg,lppt,sizeof(POINT));
}

COLORREF
SetPixel(
    HDC         hdc,
    int         X,
    int         Y,
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(SetPixel, hdc, X, Y, crColor);
    return pGweApiSet2Entrypoints->m_pSetPixel(hdc, X, Y, crColor);
}

extern "C"
COLORREF
SetTextColor(
    HDC         hdc,
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(SetTextColor, hdc, crColor, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetTextColor(hdc, crColor);
}

BOOL
StretchBlt(
    HDC     hdcDest,
    int     nXOriginDest,
    int     nYOriginDest,
    int     nWidthDest,
    int     nHeightDest,
    HDC     hdcSrc,
    int     nXOriginSrc,
    int     nYOriginSrc,
    int     nWidthSrc,
    int     nHeightSrc,
    DWORD   dwRop
    )
{
    CELOG_GDIEnter(StretchBlt, hdcDest, hdcSrc, dwRop, 0);
    return pGweApiSet2Entrypoints->m_pStretchBlt(
                    hdcDest,
                    nXOriginDest,
                    nYOriginDest,
                    nWidthDest,
                    nHeightDest,
                    hdcSrc,
                    nXOriginSrc,
                    nYOriginSrc,
                    nWidthSrc,
                    nHeightSrc,
                    dwRop
                    );
}

int
StretchDIBits(
            HDC         hdc,
            int         XDest,
            int         YDest,
            int         nDestWidth,
            int         nDestHeight,
            int         XSrc,
            int         YSrc,
            int         nSrcWidth,
            int         nSrcHeight,
    const   void*       lpBits,
    const   BITMAPINFO* lpBitsInfo,
            UINT        iUsage,
            DWORD       dwRop
    )
{
    CELOG_GDIEnter(StretchDIBits, hdc, XDest, YDest, nDestWidth);
    return pGweApiSet2Entrypoints->m_pStretchDIBits(
                    hdc,
                    XDest,
                    YDest,
                    nDestWidth,
                    nDestHeight,
                    XSrc,
                    YSrc,
                    nSrcWidth,
                    nSrcHeight,
                    lpBits,
                    lpBitsInfo,
                    iUsage,
                    dwRop
                    );
}

int
SetDIBitsToDevice(
            HDC         hdc,
            int         XDest,
            int         YDest,
            DWORD       dwWidth,
            DWORD       dwHeight,
            int         XSrc,
            int         YSrc,
            UINT        uStartScan,
            UINT        cScanLines,
    const   void*       lpvBits,
    const   BITMAPINFO* lpbmi,
            UINT        fuColorUse
    )
{
    CELOG_GDIEnter(SetDIBitsToDevice, hdc,XDest,YDest,dwWidth);
    return pGweApiSet2Entrypoints->m_pSetDIBitsToDevice(
                    hdc,
                    XDest,
                    YDest,
                    dwWidth,
                    dwHeight,
                    XSrc,
                    YSrc,
                    uStartScan,
                    cScanLines,
                    lpvBits,
                    lpbmi,
                    sizeof(BITMAPINFO),
                    fuColorUse
                    );
}


HPALETTE
CreatePalette(
    __in const   LOGPALETTE* pLogPal
    )
{
    HPALETTE hpal = NULL;
    size_t cbLogPal = 0;

    if ( (NULL == pLogPal)
         || ((pLogPal->palNumEntries > 1)
             && FAILED(UIntMult(pLogPal->palNumEntries-1, sizeof(PALETTEENTRY), &cbLogPal)))
         || FAILED(UIntAdd(cbLogPal, sizeof(LOGPALETTE), &cbLogPal)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(CreatePalette, pLogPal, 0, 0, 0);
    hpal = pGweApiSet2Entrypoints->m_pCreatePalette(pLogPal, cbLogPal);

leave:
    return hpal;
}

HPALETTE
SelectPalette(
    HDC         hdc,
    HPALETTE    hPal,
    BOOL        bForceBackground
    )
{
    CELOG_GDIEnter(SelectPalette, hdc, hPal, bForceBackground, 0);
    return pGweApiSet2Entrypoints->m_pSelectPalette(hdc, hPal, bForceBackground);
}

UINT
RealizePalette(
    HDC hdc
    )
{
    CELOG_GDIEnter(RealizePalette, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pRealizePalette(hdc);
}

UINT
GetPaletteEntries(
    HPALETTE        hPal,
    UINT            iStart,
    UINT            nEntries,
    __out_ecount_opt(nEntries) PALETTEENTRY*   pPe
    )
{
    UINT uiReturnValue = 0;
    size_t cbPe = 0;

    if ( FAILED(UIntMult(nEntries, sizeof(PALETTEENTRY), &cbPe)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetPaletteEntries, hPal, iStart, nEntries, pPe);
    uiReturnValue = pGweApiSet2Entrypoints->m_pGetPaletteEntries(hPal, iStart, nEntries, pPe, cbPe);

leave:
    return uiReturnValue;
}

UINT
SetPaletteEntries(
    HPALETTE    hpal,
    UINT        iStart,
    UINT        nEntries,
    __in_ecount(nEntries) const   PALETTEENTRY*   pPe
    )
{
    UINT uiReturnValue = 0;
    size_t cbPe = 0;

    if ( FAILED(UIntMult(nEntries, sizeof(PALETTEENTRY), &cbPe)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(SetPaletteEntries, hpal, iStart, nEntries, pPe);
    uiReturnValue = pGweApiSet2Entrypoints->m_pSetPaletteEntries(hpal, iStart, nEntries, pPe, cbPe);

leave:
    return uiReturnValue;
}

UINT
GetSystemPaletteEntries(
    HDC             hdc,
    UINT            iStart,
    UINT            nEntries,
    __out_ecount_opt(nEntries) PALETTEENTRY*   pPe
    )
{
    UINT uiReturnValue = 0;
    size_t cbPe = 0;

    if ( FAILED(UIntMult(nEntries, sizeof(PALETTEENTRY), &cbPe)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetSystemPaletteEntries, hdc, iStart, nEntries, pPe);
    uiReturnValue = pGweApiSet2Entrypoints->m_pGetSystemPaletteEntries(hdc, iStart, nEntries, pPe, cbPe);

leave:
    return uiReturnValue;
}

UINT
GetNearestPaletteIndex(
    HPALETTE    hpal,
    COLORREF    clrf
    )
{
    CELOG_GDIEnter(GetNearestPaletteIndex, hpal, clrf, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetNearestPaletteIndex(hpal, clrf);
}

UINT
GetDIBColorTable(
    HDC         hdc,
    UINT        uStartIndex,
    UINT        cEntries,
    __out_ecount(cEntries) RGBQUAD*    pColors
    )
{
    UINT uiReturnValue = 0;

    size_t cbColors = 0;

    if (FAILED(UIntMult(cEntries, sizeof(RGBQUAD), &cbColors)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GetDIBColorTable, hdc, uStartIndex, cEntries, pColors);
    uiReturnValue = pGweApiSet2Entrypoints->m_pGetDIBColorTable(hdc,uStartIndex,cEntries,pColors, cbColors);

leave:
    return uiReturnValue;
}

UINT
SetDIBColorTable(
    HDC     hdc,
    UINT    uStartIndex,
    UINT    cEntries,
    __in_ecount(cEntries) const   RGBQUAD*    pColors
    )
{
    UINT uiReturnValue = 0;

    size_t cbColors = 0;

    if (FAILED(UIntMult(cEntries, sizeof(RGBQUAD), &cbColors)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(SetDIBColorTable, hdc, uStartIndex, cEntries, pColors);
    uiReturnValue = pGweApiSet2Entrypoints->m_pSetDIBColorTable(hdc,uStartIndex,cEntries,pColors,cbColors);

leave:
    return uiReturnValue;
}

HPEN
CreatePen(
    int         fnPenStyle,
    int         nWidth,
    COLORREF    crColor
    )
{
    CELOG_GDIEnter(CreatePen, fnPenStyle, nWidth, crColor, 0);
    return pGweApiSet2Entrypoints->m_pCreatePen(fnPenStyle, nWidth, crColor);
}

int
StartDocW(
    HDC hdc,
    __in const   DOCINFOW*   pdocinfo
    )
{
    int nReturnValue = SP_ERROR;
    size_t cbdocinfo = sizeof(DOCINFOW);

    if ( NULL != pdocinfo )
        {
        if (sizeof(DOCINFOW) > pdocinfo->cbSize)
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }

        cbdocinfo = pdocinfo->cbSize;
        }

    CELOG_GDIEnter(StartDocW, hdc, pdocinfo, 0, 0);
    nReturnValue = pGweApiSet2Entrypoints->m_pStartDocW(hdc, pdocinfo, cbdocinfo);

leave:
    return nReturnValue;
}

int
EndDoc(
    HDC hdc
    )
{
    CELOG_GDIEnter(EndDoc, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pEndDoc(hdc);
}

int
StartPage(
    HDC hdc
    )
{
    CELOG_GDIEnter(StartPage, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pStartPage(hdc);
}

int
EndPage(
    HDC hdc
    )
{
    CELOG_GDIEnter(EndPage, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pEndPage(hdc);
}

int
AbortDoc(
    HDC hdc
    )
{
    CELOG_GDIEnter(AbortDoc, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pAbortDoc(hdc);
}

int
SetAbortProc(
    HDC         hdc,
    ABORTPROC   abortproc
    )
{
    CELOG_GDIEnter(SetAbortProc, hdc, abortproc, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetAbortProc(hdc, abortproc);
}

HDC
CreateDCW(
    __in_opt const   WCHAR*      lpszDriver,
    __in const   WCHAR*      lpszDevice,
    __in const   WCHAR*      lpszOutput,
    __in_opt const   DEVMODEW*   lpInitData
    )
{
    HDC hdc = NULL;

    size_t cbInitData = sizeof(DEVMODEW);

    if ( lpInitData &&
            ((sizeof(DEVMODEW) > lpInitData->dmSize) ||
                FAILED(UIntAdd(lpInitData->dmSize, lpInitData->dmDriverExtra, &cbInitData))
                )
            )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(CreateDCW, lpszDriver, lpszDevice, lpszOutput, lpInitData);
    hdc = pGweApiSet2Entrypoints->m_pCreateDCW(lpszDriver, lpszDevice, lpszOutput, lpInitData, cbInitData);

leave:
    return hdc;
}

HRGN
CreateRectRgn(
    int nLeftRect,
    int nTopRect,
    int nRightRect,
    int nBottomRect
    )
{
    CELOG_GDIEnter(CreateRectRgn, nLeftRect, nTopRect, nRightRect, nBottomRect);
    return pGweApiSet2Entrypoints->m_pCreateRectRgn(nLeftRect, nTopRect, nRightRect, nBottomRect);
}

HRGN
ExtCreateRegion(
    __in_opt const   XFORM*      lpXform,
    DWORD       nCount,
    __in_bcount(nCount) const   RGNDATA*    lpRgnData
    )
{
    CELOG_GDIEnter(ExtCreateRegion, lpXform, nCount, lpRgnData, 0);
    return pGweApiSet2Entrypoints->m_pExtCreateRegion(lpXform,sizeof(XFORM),lpRgnData,nCount);
}

BOOL
FillRgn(
    HDC     hdc,
    HRGN    hrgn,
    HBRUSH  hbr
    )
{
    CELOG_GDIEnter(FillRgn, hdc, hrgn, hbr, 0);
    return pGweApiSet2Entrypoints->m_pFillRgn(hdc, hrgn, hbr);
}

int
SetROP2(
    HDC hdc,
    int fnDrawMode
    )
{
    CELOG_GDIEnter(SetROP2, hdc, fnDrawMode, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetROP2(hdc, fnDrawMode);
}

int
GetROP2(
    HDC hdc
    )
{
    //CELOG_GDIEnter(GetROP2, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetROP2(hdc);
}

BOOL
RectVisible(
    HDC     hdc,
    __in const   RECT*   lprc
    )
{
    CELOG_GDIEnter(RectVisible, hdc, lprc, 0, 0);
    return pGweApiSet2Entrypoints->m_pRectVisible(hdc, lprc, sizeof(RECT));
}

BOOL
SetRectRgn(
    HRGN    hrgn,
    int     nLeftRect,
    int     nTopRect,
    int     nRightRect,
    int     nBottomRect
    )
{
    CELOG_GDIEnter(SetRectRgn, hrgn, nLeftRect, nTopRect, nRightRect);
    return pGweApiSet2Entrypoints->m_pSetRectRgn(hrgn, nLeftRect, nTopRect, nRightRect, nBottomRect);
}

HBRUSH
CreatePatternBrush(
    HBITMAP hbmp
    )
{
    CELOG_GDIEnter(CreatePatternBrush, hbmp, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pCreatePatternBrush(hbmp);
}

BOOL
SetViewportOrgEx(
    HDC     hdc,
    int     X,
    int     Y,
    __out_opt POINT*  lpPoint
    )
{
    CELOG_GDIEnter(SetViewportOrgEx, hdc, X, Y, lpPoint);
    return pGweApiSet2Entrypoints->m_pSetViewportOrgEx(hdc, X, Y, lpPoint, sizeof(POINT));
}

BOOL
GetViewportOrgEx(
    HDC     hdc,
    __out POINT*  lpPoint
    )
{
    //CELOG_GDIEnter(GetViewportOrgEx, hdc, lpPoint, 0 ,0);
    return pGweApiSet2Entrypoints->m_pGetViewportOrgEx(hdc, lpPoint, sizeof(POINT));
}


BOOL
OffsetViewportOrgEx(
    HDC hdc,         // handle to device context
     int nXOffset,    // horizontal offset
     int nYOffset,    // vertical offset
     __out_opt LPPOINT lpPoint  // original origin
    )
{
    //CELOG_GDIEnter(OffsetViewportOrgEx, hdc, nXOffset, nYOffset);
    return pGweApiSet2Entrypoints->m_pOffsetViewportOrgEx(hdc, nXOffset, nYOffset, lpPoint, sizeof(POINT));
}


BOOL
GetViewportExtEx(
    HDC hdc,      // handle to device context
    __out SIZE* lpSize // viewport dimensions
    )
{
    //CELOG_GDIEnter(GetViewportExtEx, hdc, lpSize, 0 ,0);
    return pGweApiSet2Entrypoints->m_pGetViewportExtEx(hdc, lpSize, sizeof(SIZE));
}


BOOL
SetWindowOrgEx(
    HDC     Hdc,
    int     X,
    int     Y,
    __out_opt LPPOINT lppt
    )
{
    //CELOG_GDIEnter(SetWindowOrgEx, hdc, X, Y ,lppt);
    return pGweApiSet2Entrypoints->m_pSetWindowOrgEx(Hdc, X, Y, lppt, sizeof(POINT));
}

BOOL
GetWindowOrgEx(
    HDC     Hdc,
    __out LPPOINT lppt
    )
{
    //CELOG_GDIEnter(GetWindowOrgEx, hdc, lppt, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetWindowOrgEx(Hdc, lppt, sizeof(POINT));
}

BOOL
GetWindowExtEx(
    HDC     Hdc,
    __out SIZE*   lpSize
    )
{
    //CELOG_GDIEnter(GetWindowExtEx, hdc, lppt, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetWindowExtEx(Hdc, lpSize, sizeof(SIZE));
}


BOOL
TransparentImage(
    HDC         hdcDest,
    int         nXOriginDest,
    int         nYOriginDest,
    int         nWidthDest,
    int         nHeightDest,
    HANDLE      hSrc,
    int         nXOriginSrc,
    int         nYOriginSrc,
    int         nWidthSrc,
    int         nHeightSrc,
    COLORREF    TransparentColor
    )
{
    CELOG_GDIEnter(TransparentImage, hdcDest, hSrc, 0, 0);
    return pGweApiSet2Entrypoints->m_pTransparentImage(
           hdcDest,nXOriginDest,nYOriginDest,nWidthDest,nHeightDest,hSrc,nXOriginSrc,nYOriginSrc,nWidthSrc,nHeightSrc,TransparentColor);
}

BOOL
GradientFill(
    HDC         hdc,
    __in_ecount(nVertex) TRIVERTEX*  pVertex,
    ULONG       nVertex,
    __in_ecount(nCount) void*       pMesh,
    ULONG       nCount,
    ULONG       ulMode
    )
{
    BOOL fReturnValue = FALSE;

    UINT uCount = 0;
    size_t cbVertex = 0;
    size_t cbMesh = 0;

    if (FAILED(ULongToUInt(nVertex, &uCount)) ||
        FAILED(UIntMult(uCount, sizeof(TRIVERTEX), &cbVertex)) ||
        FAILED(ULongToUInt(nCount, &uCount)) ||
        FAILED(UIntMult(uCount, sizeof(GRADIENT_RECT), &cbMesh)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(GradientFill, hdc, pVertex, nVertex, pMesh);
    fReturnValue = pGweApiSet2Entrypoints->m_pGradientFill(hdc, pVertex, cbVertex, nVertex, pMesh, cbMesh, nCount, ulMode);

leave:
    return fReturnValue;
}

BOOL
TranslateCharsetInfo(
    __inout DWORD*          lpSrc,
    __out CHARSETINFO*    lpCs,
    DWORD           dwFlags
    )
{
    DWORD dwSrc = 0;

    if (dwFlags != TCI_SRCFONTSIG)
        {
        dwSrc = (DWORD)lpSrc;
        lpSrc = NULL;
        }

    CELOG_GDIEnter(TranslateCharsetInfo, lpSrc, lpCs, dwFlags, 0);
    return pGweApiSet2Entrypoints->m_pTranslateCharsetInfo(dwSrc, lpSrc, 2*sizeof(DWORD), lpCs, sizeof(CHARSETINFO), dwFlags);
}

BOOL
ExtEscape(
    HDC     hdc,
    int     iEscape,
    int     cjInput,
    __in_bcount_opt(cjInput) const   char*   lpInData,
    int     cjOutput,
    __out_bcount_opt(cjOutput) char*   lpOutData
    )
{
    BOOL fReturnValue = FALSE;

    if ( (cjInput < 0) || (cjOutput < 0) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(ExtEscape, hdc, iEscape, cjInput, lpInData);
    fReturnValue = pGweApiSet2Entrypoints->m_pExtEscape(hdc, iEscape, lpInData, cjInput, lpOutData, cjOutput);

leave:
    return fReturnValue;
}

BOOL
BitmapEscape(
    HBITMAP hbmp,
    int     iEscape,
    int     cjInput,
    __in_bcount_opt(cjInput) const   char*   lpInData,
    int     cjOutput,
    __out_bcount_opt(cjOutput) char*   lpOutData
    )
{
    BOOL fReturnValue = FALSE;

    if ( (cjInput < 0) || (cjOutput < 0) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    CELOG_GDIEnter(BitmapEscape, hbmp, iEscape, cjInput, lpInData);
    fReturnValue = pGweApiSet2Entrypoints->m_pBitmapEscape(hbmp, iEscape, lpInData, cjInput, lpOutData, cjOutput);

leave:
    return fReturnValue;
}

BOOL
CeRemoveFontResource(
    CEOID   oid
    )
{
    return pGweApiSet2Entrypoints->m_pCeRemoveFontResource(oid);
}

BOOL
EnableEUDC(
    BOOL    bEnable
    )
{
    return pGweApiSet2Entrypoints->m_pEnableEUDC(bEnable);
}

#ifndef RDPGWELIB
extern "C"
BOOL
GetGweApiSetTables(__out_opt GweApiSet1_t *pApiSet1, __out_opt GweApiSet2_t *pApiSet2)
{
    if (!pGweApiSet2Entrypoints->m_pGetGweApiSetTables(pApiSet1, sizeof(GweApiSet1_t), pApiSet2, sizeof(GweApiSet2_t)))
    {
        if (pApiSet1)
            *pApiSet1 = *pGweApiSet1Entrypoints;
        if (pApiSet2)
            *pApiSet2 = *pGweApiSet2Entrypoints;
        return FALSE;
    }
    return TRUE;
}
#endif

int
GetStretchBltMode(
    HDC hdc
    )
{
    return pGweApiSet2Entrypoints->m_pGetStretchBltMode(hdc);
}

int
SetStretchBltMode(
    HDC hdc,
    int mode
    )
{
    return pGweApiSet2Entrypoints->m_pSetStretchBltMode(hdc, mode);
}

BOOL
AlphaBlend(
    HDC           hdcDest,
    int           nXOriginDest,
    int           nYOriginDest,
    int           nWidthDest,
    int           nHeightDest,
    HDC           hdcSrc,
    int           nXOriginSrc,
    int           nYOriginSrc,
    int           nWidthSrc,
    int           nHeightSrc,
    BLENDFUNCTION blendFunction
    )
{
    return pGweApiSet2Entrypoints->m_pAlphaBlend(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, hdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, blendFunction);
}

int
EnumFontFamiliesExW(
    HDC          hdc,
    LPLOGFONTW   pLogFont,
    FONTENUMPROC pfnFontEnumProc,
    LPARAM       lParam,
    DWORD        dwFlags
    )
{
    return pGweApiSet2Entrypoints->m_pEnumFontFamiliesExW(hdc, pLogFont, sizeof(LOGFONTW), pfnFontEnumProc, lParam, dwFlags);
}

DWORD
SetLayout(
    HDC           hdc,
    DWORD        dwLayout
    )
{
    CELOG_GDIEnter(SetLayout, hdc, dwLayout, 0, 0);
    return pGweApiSet2Entrypoints->m_pSetLayout(hdc, dwLayout);
}

DWORD
GetLayout(
    HDC           hdc
    )
{
    CELOG_GDIEnter(GetLayout, hdc, 0, 0, 0);
    return pGweApiSet2Entrypoints->m_pGetLayout(hdc);
}

int
SetTextCharacterExtra(
    HDC hdc,
    int nCharExtra
    )
{
    return pGweApiSet2Entrypoints->m_pSetTextCharacterExtra(hdc, nCharExtra);
}

int
GetTextCharacterExtra(
    HDC hdc
    )
{
    return pGweApiSet2Entrypoints->m_pGetTextCharacterExtra(hdc);
}

HANDLE
AddFontMemResourceEx(
    __in_bcount(cbFont) PVOID   pbFont,
    DWORD   cbFont,
    __in_opt PVOID   pdv,
    __out DWORD   *pcFonts
    )
{
    return pGweApiSet2Entrypoints->m_pAddFontMemResourceEx(pbFont,cbFont,pdv,pcFonts);
}

BOOL
RemoveFontMemResourceEx(
    HANDLE  fh
    )
{
    return pGweApiSet2Entrypoints->m_pRemoveFontMemResourceEx(fh);
}

