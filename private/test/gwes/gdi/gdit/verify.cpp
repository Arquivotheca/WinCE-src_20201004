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
#include "verify.h"

#ifndef UNDER_CE
#include <strsafe.h>
#endif

#define VALIDDC(tdc) tdc?tdc->GetDC():NULL
#define VALIDVDC(tdc) tdc?tdc->GetVDC():NULL
#define VALIDBMP(hbmp) hbmp?hbmp->GetBitmap():NULL
#define VALIDVBMP(hbmp) hbmp?hbmp->GetVBitmap():NULL
#define MaxStackSize 75

extern BOOL g_fUseGetDC;
extern BOOL g_fUsePrimaryDisplay;
extern TCHAR *gszDisplayPath;

/***********************************************************************************
***
*** MyCreateDIBSection(HDC...)
***     shared between verify and global
************************************************************************************/

//**********************************************************************************
HBITMAP myCreateDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, UINT Usage, struct MYBITMAPINFO * UserBMI, BOOL randSize)
{
    DWORD   dwError;
    HBITMAP hBmp = NULL;

    // bmi is used to temporarily hold the bitmap info for CreateDIBSection.
    // If the user specifies a "UserBMI" the data is copied from this MYBITMAPINFO to it at the end.
    struct MYBITMAPINFO bmi;

    memset(&bmi, 0, sizeof(MYBITMAPINFO));

    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = w;
    bmi.bmih.biHeight = h;
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = (WORD)d;
    bmi.bmih.biCompression = BI_RGB;
    bmi.bmih.biSizeImage = bmi.bmih.biXPelsPerMeter = bmi.bmih.biYPelsPerMeter = 0;
    bmi.bmih.biClrUsed = bmi.bmih.biClrImportant = 0;

    // if the user requested a random sized palette and it's 8bpp, then pick
    // a random sized palette.
    if(randSize && bmi.bmih.biBitCount <= 8 && Usage != DIB_PAL_COLORS)
    {
        bmi.bmih.biClrUsed = GenRand() % 256;
        if(bmi.bmih.biClrUsed > 0)
            bmi.bmih.biClrImportant = GenRand() % bmi.bmih.biClrUsed;
        else bmi.bmih.biClrImportant= 0;
    }

    if (bmi.bmih.biBitCount == 1)
    {
        setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
        setRGBQUAD(&bmi.ct[1], 0xFF, 0xFF, 0xFF);
    }
    else if (bmi.bmih.biBitCount == 2)
    {
        setRGBQUAD( &bmi.ct[0], 0x0, 0x0, 0x0);
        setRGBQUAD( &bmi.ct[1], 0x55, 0x55, 0x55);
        setRGBQUAD( &bmi.ct[2], 0xaa, 0xaa, 0xaa);
        setRGBQUAD( &bmi.ct[3], 0xff, 0xff, 0xff);
    }
    else if (bmi.bmih.biBitCount == 4)
    {
        for (int i = 0; i < 16; i++)
        {
            bmi.ct[i].rgbRed = bmi.ct[i].rgbGreen = bmi.ct[i].rgbBlue = (BYTE)(i << 4);
            bmi.ct[i].rgbReserved = 0;
        }
        // make sure it has White available
        setRGBQUAD(&bmi.ct[15], 0xff, 0xff, 0xff);
    }
    else if (bmi.bmih.biBitCount == 8)
    {
        for (int i = 0; i < 256; i++)
        {
            bmi.ct[i].rgbRed = bmi.ct[i].rgbGreen = bmi.ct[i].rgbBlue = (BYTE)i;
            bmi.ct[i].rgbReserved = 0;
        }
    }
    // Leave the structure empty for 16, 24, and 32bpp DIB's
    // any DIB's with specific bitfields will use the CreateRGBDIBSection function.

    // create the DIB from the BITMAPINFO filled in above.
    hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, Usage, ppvBits, NULL, NULL);

    if (!hBmp)
    {
        if(w*h == 0)
        {
            // silently succeed with invalid width and hight, as expected, to reduce noise
        }
        // 8bpp should succeed with DIB_PAL_COLORS, everything else should fail.
        else if(Usage == DIB_PAL_COLORS && bmi.bmih.biBitCount != 8)
            info(DETAIL, TEXT("Fail as expected: DIB_PAL_COLORS CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d"),hdc, w, h, d, Usage, GetLastError());
        else if(!hdc && Usage == DIB_PAL_COLORS && bmi.bmih.biBitCount == 8)
            info(DETAIL, TEXT("Fail as expected: DIB_PAL_COLORS w/ invalid hdc and CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d"),hdc, w, h, d, Usage, GetLastError());
        else
        {
            // Failure could be due to running outof memory
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d: out of memory"), hdc, w, h, d, Usage, GetLastError());
            else
                info(FAIL, TEXT("CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d"), hdc, w, h, d, Usage, dwError);
        }
    }
    else
    {
        if (*ppvBits == NULL || (hdc == NULL && Usage == DIB_PAL_COLORS && bmi.bmih.biBitCount != 24 && bmi.bmih.biBitCount > 8) || w*h == 0)
        {
            info(FAIL, TEXT("Succeeded CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d): *ppvBIts = NULL: GLE:%d"), hdc, w, h, d, Usage, GetLastError());
            DeleteObject(hBmp);
            hBmp = NULL;
        }
    }

    // if the user wants the BITMAPINFO, then copy it over from the BITMAPINFO used to create the DIB.
    if(UserBMI)
        memcpy(UserBMI, &bmi, sizeof(struct MYBITMAPINFO));

    return hBmp;
}


//**********************************************************************************
HBITMAP myCreateRGBDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, int nBitMask, DWORD dwCompression, struct MYBITMAPINFO * UserBMI)
{
    HBITMAP hBmp = NULL;

    // bmi is used to temporarily hold the bitmap info for CreateDIBSection.
    // If the user specifies a "UserBMI" the data is copied from this MYBITMAPINFO to it at the end.
    struct MYBITMAPINFO bmi;

    // CE ignores dwCompression for 24bpp DIB's, desktop fails BI_BITFIELDS 24bpp DIB.
    #ifndef UNDER_CE
        if(24 == d)
            dwCompression = BI_RGB;
    #endif

    memset(&bmi, 0, sizeof(MYBITMAPINFO));

    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = w;
    bmi.bmih.biHeight = h;
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = (WORD)d;
    bmi.bmih.biCompression = dwCompression;
    bmi.bmih.biSizeImage = bmi.bmih.biXPelsPerMeter = bmi.bmih.biYPelsPerMeter = 0;
    bmi.bmih.biClrUsed = bmi.bmih.biClrImportant = 0;

    switch(nBitMask)
    {
    case RGB4444:
        bmi.ct[3].rgbMask = 0xF000;
        bmi.ct[2].rgbMask = 0x000F;
        bmi.ct[1].rgbMask = 0x00F0;
        bmi.ct[0].rgbMask = 0x0F00;
        break;
    case RGB565:
        bmi.ct[2].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x07E0;
        bmi.ct[0].rgbMask = 0xF800;
        break;
    case RGB555:
        bmi.ct[2].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x03E0;
        bmi.ct[0].rgbMask = 0x7C00;
        break;
    case RGB1555:
        bmi.ct[3].rgbMask = 0x8000;
        bmi.ct[2].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x03E0;
        bmi.ct[0].rgbMask = 0x7C00;
        break;
    case BGR4444:
        bmi.ct[3].rgbMask = 0xF000;
        bmi.ct[0].rgbMask = 0x000F;
        bmi.ct[1].rgbMask = 0x00F0;
        bmi.ct[2].rgbMask = 0x0F00;
        break;
    case BGR565:
        bmi.ct[0].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x07E0;
        bmi.ct[2].rgbMask = 0xF800;
        break;
    case BGR555:
        bmi.ct[0].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x03E0;
        bmi.ct[2].rgbMask = 0x7C00;
        break;
    case BGR1555:
        bmi.ct[3].rgbMask = 0x8000;
        bmi.ct[0].rgbMask = 0x001F;
        bmi.ct[1].rgbMask = 0x03E0;
        bmi.ct[2].rgbMask = 0x7C00;
        break;
    case RGB8888:
        bmi.ct[3].rgbMask = 0xFF000000;
        bmi.ct[2].rgbMask = 0x000000FF;
        bmi.ct[1].rgbMask = 0x0000FF00;
        bmi.ct[0].rgbMask = 0x00FF0000;
        break;
    case RGB888:
        bmi.ct[2].rgbMask = 0x000000FF;
        bmi.ct[1].rgbMask = 0x0000FF00;
        bmi.ct[0].rgbMask = 0x00FF0000;
        break;
    case BGR8888:
        bmi.ct[3].rgbMask = 0xFF000000;
        bmi.ct[0].rgbMask = 0x000000FF;
        bmi.ct[1].rgbMask = 0x0000FF00;
        bmi.ct[2].rgbMask = 0x00FF0000;
        break;
    case BGR888:
        bmi.ct[0].rgbMask = 0x000000FF;
        bmi.ct[1].rgbMask = 0x0000FF00;
        bmi.ct[2].rgbMask = 0x00FF0000;
        break;
    case RGBEMPTY:
        bmi.ct[3].rgbMask = 0x0;
        bmi.ct[2].rgbMask = 0x0;
        bmi.ct[1].rgbMask = 0x0;
        bmi.ct[0].rgbMask = 0x0;
        break;
    case RGBERROR:
    default:
        info(FAIL, TEXT("CreateRGBDIBSection unknown bit mask %d"), nBitMask);
        break;
    }

    // create the DIB from the BITMAPINFO filled in above.
    // we assume that all creations will succeed.
    CheckNotRESULT(NULL, hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, ppvBits, NULL, NULL));

    // if the user wants the BITMAPINFO, then copy it over from the BITMAPINFO used to create the DIB.
    if(UserBMI)
        memcpy(UserBMI, &bmi, sizeof(struct MYBITMAPINFO));

    return hBmp;
}

BOOL g_OutputBitmaps;
// By default, we'll output PNG failure images at all times, since they're so cheap.
BOOL    g_OutputPNG;
int  g_nFailureNum = 0;

void
SaveSurfaceToBMP(HDC hdcSrc, TCHAR *pszName, int nWidth, int nHeight)
{
    HBITMAP hBmpStock = NULL;
    HBITMAP hDIB = NULL;
    HDC hdc;
    TCHAR tszPNGName[MAX_PATH] = {0};
    TCHAR * pszExtension = NULL;
    static int iMaxBitmapsToSave = 1000;

    if (iMaxBitmapsToSave <= 0)
    {
        info(DETAIL, TEXT("Already saved too many bitmaps, not saving"));
        return;
    }
    --iMaxBitmapsToSave;

    StringCchCopy(tszPNGName, _countof(tszPNGName), pszName);
    pszExtension = _tcsrchr(tszPNGName, _T('.'));
    if (pszExtension && !_tcsicmp(pszExtension, _T(".bmp")))
    {
        *pszExtension = 0;
        StringCchCat(tszPNGName, _countof(tszPNGName), _T(".png"));
    }

    // Create a memory DC
    CheckNotRESULT(NULL, hdc = CreateCompatibleDC(hdcSrc));

    if (hdc)
    {
        // Create a 24-bit bitmap
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = nWidth;
        bmi.bmiHeader.biHeight      = nHeight;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        // Create the DIB Section
        LPBYTE lpbBits = NULL;
        CheckNotRESULT(NULL, hDIB = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (VOID**)&lpbBits, NULL, 0));
        CheckNotRESULT(NULL, hBmpStock = (HBITMAP) SelectObject(hdc, hDIB));

        if (hDIB && lpbBits)
        {
            DIBSECTION ds;
            HRESULT hr;
            cCompareResults cCr;

            CheckNotRESULT(0, GetObject(hDIB, sizeof (DIBSECTION), &ds));

            CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, nWidth, nHeight, hdcSrc, 0, 0, SRCCOPY));

            cCr.Reset();
            // Try saving as PNG first (even if /OutputBitmaps is specified, always try PNG first since they're smaller)
            hr = SavePNG(tszPNGName, lpbBits, bmi, ds.dsBm.bmWidthBytes, &cCr);
            if (FAILED(hr) && g_OutputBitmaps)
            {
                info(DETAIL, TEXT("Saving as PNG did not succeed, will save as BMP"));
                OutputLogInfo(&cCr);
                hr = SaveBitmap(pszName, lpbBits, bmi, ds.dsBm.bmWidthBytes, NULL);
            }

            if (FAILED(hr) && g_OutputBitmaps)
            {
                info(FAIL, TEXT("Failed to save the bitmap to a file."));
                OutputLogInfo(&cCr);
            }
        }

        // Remove our DIB section from our DC by putting the stock bitmap back.
        if (hdc && hBmpStock)
            CheckNotRESULT(NULL, SelectObject(hdc, hBmpStock));

        // Free our DIB Section
        if (hDIB)
            CheckNotRESULT(FALSE, DeleteObject(hDIB));

        // Free our DIB DC
        if (hdc)
            CheckNotRESULT(FALSE, DeleteDC(hdc));
    }
}

// this is needed for some tests which access the verification driver directly.
HDC     g_hdcSecondary = NULL;

// if SURFACE_VERIFY is defined, from global.h, then we want to compile with the following functions.
#ifdef SURFACE_VERIFY

// all of the global variables for verification, only defined if verification is turned in when compiling.
static  BOOL    g_Verify,
                g_TestVerify;

static  DOUBLE   g_MaxErrors;
RMSCOMPAREDATA g_RMSCompareData;

// the comparison function takes a byte pointer and casts it to what it expects.
BYTE *g_SurfaceVerifyCompareParameter;
// the current comparison function
PFNCOMPAREPIXELS gpfnSurfaceVerifyPixelCompare;

static PFNGRADIENTFILLINTERNAL gpfnGradientFillInternal;
static PFNALPHABLENDINTERNAL gpfnAlphaBlendInternal;
static PFNSTARTDOCINTERNAL gpfnStartDocInternal;
static PFNENDDOCINTERNAL gpfnEndDocInternal;
static PFNSTARTPAGEINTERNAL gpfnStartPageInternal;
static PFNENDPAGEINTERNAL gpfnEndPageInternal;
static PFNABORTDOCINTERNAL gpfnAbortDocInternal;
static PFNSETABORTPROCINTERNAL gpfnSetAbortProcInternal;

/***********************************************************************************
***
*** Wrapper classes for the duplicated GDI objects
************************************************************************************/

//**********************************************************************************
class   cTBitmap
{
  public:

    cTBitmap(const HBITMAP hbmpUserIn, const HBITMAP hbmpVerificationIn = NULL) :
        m_UserHBitmap(hbmpUserIn),
        m_VerificationBitmap(hbmpVerificationIn)
    {
    }

    ~cTBitmap()
    {
        if (m_UserHBitmap)
            DeleteObject(m_UserHBitmap);
        if (m_VerificationBitmap)
            DeleteObject(m_VerificationBitmap);
    }

    HBITMAP GetBitmap()
    {
        return m_UserHBitmap;
    }
    void    SetBitmap(const HBITMAP InHBitmap)
    {
        m_UserHBitmap = InHBitmap;
    }

    HBITMAP GetVBitmap()
    {
        return m_VerificationBitmap;
    }


    void    SetVBitmap(const HBITMAP InHBitmap)
    {
        m_VerificationBitmap = InHBitmap;
    }

  private:
    HBITMAP m_UserHBitmap;
    HBITMAP m_VerificationBitmap;
};

//**********************************************************************************
static cTBitmap g_tbmpStock(NULL);

// used for bad parameter testing for API's that are dynamically linked.
// values are initialized during InitVerify.
cTBitmap g_tbmpBAD((HBITMAP) NULL, (HBITMAP) NULL);
TBITMAP g_hbmpBAD = &g_tbmpBAD;

cTBitmap g_tbmpBAD2((HBITMAP) NULL, (HBITMAP) NULL);
TBITMAP g_hbmpBAD2 = &g_tbmpBAD2;

//**********************************************************************************
class   cTdc
{
  public:
    cTdc(const HDC hdcUserIn, const HDC hdcVerificationIn, const HWND hwndIn, const TBITMAP tbmp) :
        m_UserHdc(hdcUserIn),
        m_VerificationHdc(hdcVerificationIn),
        m_hWnd(hwndIn),
        m_tbmp(tbmp),
        m_stackIndex(0)
    {
        m_tbmpSaved[0] = &g_tbmpStock;
    }

    ~cTdc()
    {
        // we don't know whether the user dc came from GetDC or CreateCompatibleDC, so we'll
        // depend on the user to delete it.  The verification DC always comes from CreateDC.
        if (m_VerificationHdc)
            DeleteDC(m_VerificationHdc);
    }

    HDC     GetDC()
    {
        return m_UserHdc;
    }

    HDC     GetVDC()
    {
        return m_VerificationHdc;
    }

    void    SetDC(const HDC InHDC, const HWND InHWND)
    {
        m_UserHdc = InHDC;
        m_hWnd = InHWND;
    }

    void    SetVDC(const HDC InHDC)
    {
        m_VerificationHdc = InHDC;
    }

    HWND GetHWND(void)
    {
        return m_hWnd;
    }

    TBITMAP GetBitmap()
    {
        return m_tbmp;
    }

    TBITMAP SelectBitmap(const TBITMAP tbmp)
    {
        TBITMAP tbmpTmp = m_tbmp;
        // the primary has a NULL m_tbmp, don't change it from NULL
        if(!m_tbmp)
            info(FAIL, TEXT("Selecting a bitmap into a DC that has an associated t-bitmap of NULL"));

        m_tbmp = tbmp;

        return tbmpTmp;
    }

    void    PushBitmap()
    {
        // make sure we don't access outside of the stack.
        m_stackIndex++;
        assert(m_stackIndex < MaxStackSize);
        m_tbmpSaved[m_stackIndex] = m_tbmp;
    }

    void    PopBitmap(int nIndex)
    {
        // this function should never be called when the index is 0, and the index should never go below 0
        assert(m_stackIndex > 0);

        // if we're popping to position 0, or if the position requested isn't inside of the stack
        // do nothing
        if( nIndex != 0 && nIndex <= m_stackIndex)
        {
            // negative index means go back relative to the current position
            if(nIndex < 0)
                m_stackIndex += (nIndex + 1);
            // positive index means go to that position
            else
                m_stackIndex = nIndex;

            // retrieve the bitmap associated, and move the index to the new top
            m_tbmp = m_tbmpSaved[m_stackIndex];
            m_stackIndex--;
        }
    }

  private:
    HDC       m_UserHdc;
    HDC       m_VerificationHdc;
    HWND    m_hWnd;
    TBITMAP m_tbmp;
    TBITMAP m_tbmpSaved[MaxStackSize];
    int m_stackIndex;
};

// used to signify a DC was created without using GetDC. 
const HWND NON_PRIMARY_DC = (HWND)-1;

// used for bad parameter testing for API's that are dynamically linked.
// values are initialized during InitVerify.
cTdc g_tdcBAD((HDC) NULL, (HDC) NULL, NON_PRIMARY_DC, NULL);
TDC g_hdcBAD = &g_tdcBAD;

cTdc g_tdcBAD2((HDC) NULL, (HDC) NULL, NON_PRIMARY_DC, NULL);
TDC g_hdcBAD2 = &g_tdcBAD2;

//***********************************************************************************

// Query the device if the compositor is enabled and check the source of our DC to
// see if it is writable or not. Only DCs which point to the primary are considered
// write-protected, so we will keep track of our DC source with the GetHWND method.
BOOL
IsWritableHDC(TDC tdc)
{
    BOOL fPrimaryHDC = FALSE;

    // no TDC specified, assume writable
    if (NULL==tdc)
        return TRUE;

    // a NULL hwnd source indicates our HDC is pointing to the primary. All other HWND
    // values indicate a writable backbuffer.
    fPrimaryHDC = tdc?(BOOL)(tdc->GetHWND()==NULL):FALSE;

    // an HDC is writable if the compositor is disabled or not pointed to the primary
    return !(IsCompositorEnabled() && fPrimaryHDC);
}

// Query the device if the window manager compositor is running. Since there is no API
// which does this, we're using an API which is only implemented when the compositor
// is in the image and checking the error return value.
BOOL
IsCompositorEnabled(void)
{
    static BOOL fDoOnce     = FALSE;
    static BOOL fCompositor = FALSE;

    if (!fDoOnce)
    {
        fDoOnce = TRUE;

        // assume the compositor APIs are implemented
        fCompositor = TRUE;
        
        // by using one of the compositor APIs we can check if the
        // compositor is enabled on the primary or not
        SetLastError(ERROR_SUCCESS);
        if (0 == GetWindowOpacity(NULL))
            if (ERROR_CALL_NOT_IMPLEMENTED == GetLastError())
                fCompositor = FALSE;

        // compositor cannot be enabled for secondary displays
        if (!g_fUsePrimaryDisplay)
            fCompositor = FALSE;
    }

    return fCompositor;
}

// Returns the bitdepth for the window compositor backbuffer format. This is
// only in use when the compositor is enabled and the deafult is 16bpp. The
// default can be modified in the registry so we need to check if this value
// has been modified.
DWORD
GetBackbufferBPP(void)
{
    // the default window backbuffer format is 16bpp
    static BOOL fDoOnce          = FALSE;
    static DWORD dwBackbufferBPP = 16;

    if (!fDoOnce)
    {
        fDoOnce = TRUE;

        HKEY hKey = NULL;
        DWORD dwSize = sizeof(dwBackbufferBPP);

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            TEXT("SYSTEM\\GWE\\Compositor"),
            0, // Reseved. Must == 0.
            0, // Must == 0.
            &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueEx(hKey,
                TEXT("OpaqueBufferBPP"),
                NULL,  // Reserved. Must == NULL.
                NULL,  // Do not need type info.
                (BYTE *)&dwBackbufferBPP,
                &dwSize) == ERROR_SUCCESS)
            {
            }
            RegCloseKey(hKey);
        }
    }

    return dwBackbufferBPP;
}

//**********************************************************************************

// this function allows the other test cases and such to output Bitmaps
void
OutputBitmap(TDC tdc, int nWidth, int nHeight)
{
    TCHAR   szFileName[MAX_PATH] = {NULL};

    if(g_OutputBitmaps || g_OutputPNG)
    {
        // save the primary
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-Failure.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        info(DETAIL, TEXT("Failure BMP is %s/.png"), szFileName);
        SaveSurfaceToBMP(VALIDDC(tdc), szFileName, nWidth, nHeight);
        g_nFailureNum++;
    }
}

// this function allows the other test cases and such to output Bitmaps
void
OutputScreenHalvesBitmap(TDC tdc, int nWidth, int nHeight)
{
    TCHAR   szFileName[MAX_PATH] = {NULL};
    TDC tdcTmp;
    TBITMAP tbmpTmp, tbmpStock;

    if(g_OutputBitmaps || g_OutputPNG)
    {
        // save the primary
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-Failure.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        SaveSurfaceToBMP(VALIDDC(tdc), szFileName, nWidth, nHeight);

        info(DETAIL, TEXT("Failure BMP is %s/.png"), szFileName);

        // save a left/right diff
        tdcTmp = CreateCompatibleDC(tdc);
        tbmpTmp = CreateCompatibleBitmap(tdc, nWidth/2, nHeight);
        tbmpStock = (TBITMAP) SelectObject(tdcTmp, tbmpTmp);
        // copy from the primary to the temporary bitmap.
        BitBlt(tdcTmp, 0, 0, nWidth/2, nHeight, tdc, 0, 0, SRCCOPY);
        // copy from the left half of the screen to the right half and invert so we get a difference bitmap.
        BitBlt(tdcTmp, 0, 0, nWidth/2, nHeight, tdc, nWidth/2, 0, SRCINVERT);
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-FailureDiff.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        SaveSurfaceToBMP(VALIDDC(tdcTmp), szFileName, nWidth, nHeight);

        if (tdcTmp && tbmpStock)
        {
            SelectObject(tdcTmp, tbmpStock);
        }

        if (tbmpTmp)
        {
            DeleteObject(tbmpTmp);
        }

        if (tdcTmp)
        {
            DeleteDC(tdcTmp);
        }

        g_nFailureNum++;
    }
}

// this function is used by verification to output bitmaps.
void
OutputBitmaps(HDC hdc, HDC hdcV, int nWidth, int nHeight)
{
    TCHAR   szFileName[MAX_PATH] = {NULL};
    HDC hdcDiff;
    HBITMAP hbmpDiff, hbmpStock;

    if(g_OutputBitmaps || g_OutputPNG)
    {
        CheckNotRESULT(NULL, hdcDiff = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpDiff = CreateCompatibleBitmap(hdc, nWidth, nHeight));
        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcDiff, hbmpDiff));

        // initialize the diff bitmap in case the sources are smaller than the screen
        CheckNotRESULT(FALSE, PatBlt(hdcDiff, 0, 0, nWidth, nHeight, BLACKNESS));

        CheckNotRESULT(FALSE, BitBlt(hdcDiff, 0, 0, nWidth, nHeight, hdcV, 0, 0, SRCCOPY));
        CheckNotRESULT(FALSE, BitBlt(hdcDiff, 0, 0, nWidth, nHeight, hdc, 0, 0, SRCINVERT));
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-FailureDiff.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        SaveSurfaceToBMP(hdcDiff, szFileName, nWidth, nHeight);
        CheckNotRESULT(NULL, SelectObject(hdcDiff, hbmpStock));
        CheckNotRESULT(FALSE, DeleteObject(hbmpDiff));
        CheckNotRESULT(FALSE, DeleteObject(hdcDiff));

        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-Failure.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        SaveSurfaceToBMP(hdc, szFileName, nWidth, nHeight);
        info(DETAIL, TEXT("Failure BMP is %s/.png"), szFileName);
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%sgdiapi_%04d_%03d-FailureVerifier.bmp"), gszBitmapDestination, g_dwCurrentTestCase, g_nFailureNum);
        SaveSurfaceToBMP(hdcV, szFileName, nWidth, nHeight);

        if (hdcDiff && hbmpStock)
        {
            SelectObject(hdcDiff, hbmpStock);
        }

        if (hbmpDiff)
        {
            DeleteObject(hbmpDiff);
        }

        if (hdcDiff)
        {
            DeleteDC(hdcDiff);
        }

        g_nFailureNum++;
    }
}

/***********************************************************************************
***
*** Surface Verification Functions
***
************************************************************************************/

//**********************************************************************************
// CompareSurfaceWorker
//
// Based off of CheckAllWhite, except using two dib sections and comparing them,
// instead of comparing to white.
void
CompareSurfaceWorker(HDC hdc, HDC hdcV, BOOL expected = TRUE)
{
    // comparing palette entry #'s doesn't make sense when comparing two different display drivers.
    // 24bpp so everything will always be in the highest possible resolution for color comparisons (alpha doesn't matter)
    int     bpp = 24;
    DWORD dwMaxPrintMessages = 20,
            dwError = 0;
    BYTE   *lpBits = NULL,
           *lpBitsV = NULL;
    HDC     myHdc = NULL,
            myHdcV = NULL;
    HBITMAP hBmp = NULL,
            stockBmp = NULL,
            hBmpV = NULL,
            stockBmpV = NULL;
    DIBSECTION ds;
    RECT    rc;
    int nclipboxRval;
    cCompareResults cCr;
    DOUBLE dData = 0;
    HRESULT hr;


    SetLastError(ERROR_SUCCESS);

    if (NULL == (myHdc = CreateCompatibleDC(hdc)))
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleDC(0x%08x) fail for the primary dc: Out of memory: err = %ld"), hdc, dwError);
        else
            info(FAIL, TEXT("CreateCompatibleDC(0x%08x) fail for the primary dc: err = %ld"), hdc, dwError);

        return;
    }

    if (NULL == (myHdcV = CreateCompatibleDC(hdcV)))
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleDC(0x%08x) fail for the verification dc: Out of memory: err = %ld"), hdcV, dwError);
        else
            info(FAIL, TEXT("CreateCompatibleDC(0x%08x) fail for the verification dc: err = %ld"), hdcV, dwError);

        CheckNotRESULT(FALSE, DeleteDC(myHdc));

        return;
    }

    CheckNotRESULT(ERROR, nclipboxRval = GetClipBox(hdc, &rc));

    if(nclipboxRval == COMPLEXREGION)
    {
        info(FAIL, TEXT("GetClipBox returned a complex region, or an error.  Is there a window above the test suite?"));
    }

    SetLastError(ERROR_SUCCESS);
    // BUGBUG: the height of the dib sections are increased by 1 because this function accesses
    // BUGBUG: by DWORD's, which end up stepping past the end of the surface and excepting.
    if (NULL == (hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, bpp, zx, -(zy+1), DIB_RGB_COLORS, NULL, FALSE)) ||
       NULL == (hBmpV = myCreateDIBSection(hdcV, (VOID **) & lpBitsV, bpp, zx, -(zy+1), DIB_RGB_COLORS, NULL, FALSE)))
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("myCreateDIBSection() fail: Out of memory: err = %ld"), dwError);
        else
            info(FAIL, TEXT("myCreateDIBSection() fail: err = %ld"), dwError);

        if (hBmp)
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        if (hBmpV)
            CheckNotRESULT(FALSE, DeleteObject(hBmpV));

        CheckNotRESULT(FALSE, DeleteDC(myHdc));
        CheckNotRESULT(FALSE, DeleteDC(myHdcV));

        return;
    }

    // temporarily reset the ViewportOrg and WindowOrg to (0, 0) in case
    // a test has interfered with these values.
    POINT ptViewportOrg  = { 0 };
    POINT ptViewportOrgV = { 0 };
    POINT ptWindowOrg    = { 0 };
    POINT ptWindowOrgV   = { 0 };

    GetViewportOrgEx(hdc, &ptViewportOrg);
    GetViewportOrgEx(hdcV, &ptViewportOrgV);
    GetWindowOrgEx(hdc, &ptWindowOrg);
    GetWindowOrgEx(hdcV, &ptWindowOrgV);

    SetViewportOrgEx(hdc, 0, 0, NULL);
    SetViewportOrgEx(hdcV, 0, 0, NULL);
    SetWindowOrgEx(hdc, 0, 0, NULL);
    SetWindowOrgEx(hdcV, 0, 0, NULL);

    // initialize the new hdc's first.
    CheckNotRESULT(NULL, stockBmp = (HBITMAP) SelectObject(myHdc, hBmp));
    CheckNotRESULT(NULL, stockBmpV = (HBITMAP) SelectObject(myHdcV, hBmpV));
    CheckNotRESULT(FALSE, PatBlt(myHdc, 0, 0, zx, zy, WHITENESS));
    CheckNotRESULT(FALSE, PatBlt(myHdcV, 0, 0, zx, zy, WHITENESS));

    CheckNotRESULT(0, GetObject(hBmp, sizeof (DIBSECTION), &ds));
    if (ds.dsBm.bmHeight < 0)
        info(FAIL, TEXT("SurfaceVerify - GetObject returned bmp with negative height"));

    // copy the bits
    CheckNotRESULT(FALSE, BitBlt(myHdc, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
    CheckNotRESULT(FALSE, BitBlt(myHdcV, 0, 0, zx, zy, hdcV, 0, 0, SRCCOPY));

    cCr.Reset();
    cCr.SetMaxResults(dwMaxPrintMessages);

    // negative height because this is a top down dib.
    hr = gpfnSurfaceVerifyPixelCompare(lpBitsV, zx, -zy, ds.dsBm.bmWidthBytes, lpBits, zx, -zy, ds.dsBm.bmWidthBytes, &cCr, g_SurfaceVerifyCompareParameter);

    if(FAILED(hr) && hr != E_FAIL)
    {
        info(FAIL, TEXT("Driver verification comparison call failed"));
        OutputLogInfo(&cCr);
    }
    else if(cCr.GetPassResult() != expected)
    {
        if(cCr.GetPassResult() == FALSE)
        {
            info(FAIL, TEXT("Driver verification mismatch detected."));
            OutputBitmaps(myHdc, myHdcV, zx, zy);
            OutputLogInfo(&cCr);
        }
        else
        {
            info(FAIL, TEXT("Driver verification did not fail as expected. Is the driver verification broken?"));
        }
    }

    // restore the ViewportOrg and WindowOrg to the original value.
    SetViewportOrgEx(hdc, ptViewportOrg.x, ptViewportOrg.y, NULL);
    SetViewportOrgEx(hdcV, ptViewportOrgV.x, ptViewportOrgV.y, NULL);
    SetWindowOrgEx(hdc, ptWindowOrg.x, ptWindowOrg.y, NULL);
    SetWindowOrgEx(hdcV, ptWindowOrgV.x, ptWindowOrgV.y, NULL);

    CheckNotRESULT(NULL, SelectObject(myHdc, stockBmp));
    CheckNotRESULT(NULL, SelectObject(myHdcV, stockBmpV));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteObject(hBmpV));
    CheckNotRESULT(FALSE, DeleteDC(myHdc));
    CheckNotRESULT(FALSE, DeleteDC(myHdcV));

    return;
}

//**********************************************************************************
// SurfaceVerify
//
// Compares the HDC with the Verification HDC within the TDC.
void
SurfaceVerify(TDC tdc, BOOL force = FALSE, BOOL expected = TRUE)
{
    // Pick a random number between 0 and verifyfreq between verifications
    const int verifyfreq = 500;

    // static local counter variable, when it hits 0, we do a verify, otherwise we just decrement.
    static int tries = 0;
    DWORD   dwErrors = 0;

    // if the handle is NULL, or g_hdcBAD, or g_hdcBAD2, then it was a bad parameter call, so we don't want to verify it.
    if (!tdc || tdc == g_hdcBAD || tdc == g_hdcBAD2)
        return;

    if (force || verifyFlag==EVerifyAlways || --tries <= 0)
    {
        // comparing palette entry #'s doesn't make sense when comparing two different display drivers.
        // 24bpp so everything will always be in the highest possible resolution for color comparisons (alpha doesn't matter)
        HDC     hdc = NULL,
                hdcV = NULL;

        // reset the random counter for the next run.
        tries = GenRand() % verifyfreq;

        hdc = VALIDDC(tdc);
        hdcV = VALIDVDC(tdc);

        CompareSurfaceWorker(hdc, hdcV, expected);
    }

    return;
}

//**********************************************************************************
// CompareSurface
//
// Compare the two HDCs from the input TDCs.
void
CompareSurface(TDC tdcActual, TDC tdcExpected)
{
    // if the handle is NULL, or g_hdcBAD, or g_hdcBAD2, then it was a bad parameter call, so we don't want to verify it.
    if (!tdcActual   || tdcActual == g_hdcBAD   || tdcActual == g_hdcBAD2 ||
        !tdcExpected || tdcExpected == g_hdcBAD || tdcExpected == g_hdcBAD2)
    {
        return;
    }

    HDC hdcActual   = VALIDDC(tdcActual);
    HDC hdcExpected = VALIDDC(tdcExpected);

    CompareSurfaceWorker(hdcActual, hdcExpected, TRUE);
}

/***********************************************************************************
***
*** called by main, for initialization and freeing
***
************************************************************************************/

//**********************************************************************************
// called in main.cpp, initialized the verification globals.
void
InitVerify()
{
    // setup the stock bitmap, initalize verification flags
    HDC     hdc         = NULL;
    HDC     hdcCompat   = NULL;
    HDC     hdcInvalid  = NULL;
    HBITMAP hbmpStock   = NULL;
    HBITMAP hbmpInvalid = NULL;

    // initialize our verification globals
    g_Verify = FALSE;
    g_TestVerify = FALSE;
    g_hdcSecondary = NULL;
    g_MaxErrors = 0;
    g_SurfaceVerifyCompareParameter = (BYTE *) &g_MaxErrors;
    gpfnSurfaceVerifyPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;

    // verify driver verification is supported, if requested
    if (DriverVerify == EVerifyDDITEST)
    {
        // driver verification forced on regardless of memory or availability.
        // if CreateDC fails, tests will fail.
        g_hdcSecondary = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);
        g_Verify = g_TestVerify = TRUE;

        if (!g_hdcSecondary)
        {
            info(ABORT, TEXT("Unable to create a DC for the verification driver: %s."), gszVerifyDriverName);
            info(ABORT, TEXT("Has the verification display driver been copied to the correct directory?"));
            info(ABORT, TEXT("Does the device have enough memory to load the driver?"));
        }
    }
    else if (DriverVerify == EVerifyNone)
    {
        info(DETAIL, TEXT("Driver verification option turned off by command line"));
    }
    else 
    {
        info(DETAIL, TEXT("Unknown driver verification method"));
    }

    // if gpfngradientfill was set up in main, then we support gradient fill,
    // take over the function pointer and keep the original for internal use
    if (gpfnGradientFill)
    {
        info(DETAIL, TEXT("Gradient fill available in the image"));
        gpfnGradientFillInternal = (PFNGRADIENTFILLINTERNAL) gpfnGradientFill;
        gpfnGradientFill = & GradientFill;
    }
    else 
    {
        info(DETAIL, TEXT("Gradient fill unavailable to test"));
    }

    // if gpfnAlphaBlend was set up in main, then we support AlphaBlend,
    // take over the function pointer and keep the original for internal use
    if (gpfnAlphaBlend)
    {
        info(DETAIL, TEXT("AlphaBlend available in the image"));
        gpfnAlphaBlendInternal = (PFNALPHABLENDINTERNAL) gpfnAlphaBlend;
        gpfnAlphaBlend = & AlphaBlend;
    }
    else 
    {
        info(DETAIL, TEXT("AlphaBlend unavailable to test"));
    }

    // check for Right-To-Left support
    if (!g_bRTL)
    {
        info(DETAIL, TEXT("Using default LTR window layout"));
    }
    else
    {
        info(DETAIL, TEXT("Using RTL window layout"));
    }

    // check for printing support
    if (gpfnStartDoc && gpfnEndDoc && gpfnStartPage && gpfnEndPage && gpfnAbortDoc && gpfnSetAbortProc)
    {
        info(DETAIL, TEXT("Printing available in the image"));
        gpfnStartDocInternal = (PFNSTARTDOCINTERNAL) gpfnStartDoc;
        gpfnEndDocInternal = (PFNENDDOCINTERNAL) gpfnEndDoc;
        gpfnStartPageInternal = (PFNSTARTPAGEINTERNAL) gpfnStartPage;
        gpfnEndPageInternal = (PFNENDPAGEINTERNAL) gpfnEndPage;
        gpfnAbortDocInternal = (PFNABORTDOCINTERNAL) gpfnAbortDoc;
        gpfnSetAbortProcInternal = (PFNSETABORTPROCINTERNAL) gpfnSetAbortProc;

        gpfnStartDoc = & StartDoc;
        gpfnEndDoc = & EndDoc;
        gpfnStartPage = & StartPage;
        gpfnEndPage = & EndPage;
        gpfnAbortDoc = & AbortDoc;
        gpfnSetAbortProc = & SetAbortProc;
    }
    else
    {
        info(DETAIL, TEXT("Printing unavailable to test"));
    }


    // retrieve a stock bitmap object for use later
    if (g_fUseGetDC)
    {
        CheckNotRESULT(NULL, hdc = GetDC(NULL));
    }
    else
    {
        CheckNotRESULT(NULL, hdc = CreateDC(gszDisplayPath, NULL, NULL, NULL));
    }
    
    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmpStock = (HBITMAP) GetCurrentObject(hdcCompat, OBJ_BITMAP));

    if (g_fUseGetDC)
    {
        CheckNotRESULT(0, ReleaseDC(NULL, hdc));
    }
    else
    {
        CheckNotRESULT(0, DeleteDC(hdc));
    }

    // cleanup
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    hdc = NULL;
    hdcCompat = NULL;

    // save our stock bitmap
    g_tbmpStock.SetBitmap(hbmpStock);
    g_tbmpStock.SetVBitmap(hbmpStock);

    // create invalid objects by immediately releasing but holding on to their handle
    CheckNotRESULT(NULL, hbmpInvalid = CreateBitmap(1, 1, 1, 1, NULL));
    CheckNotRESULT(0, DeleteObject(hbmpInvalid));

    CheckNotRESULT(NULL, hdcInvalid = GetDC(NULL));
    CheckNotRESULT(0, ReleaseDC(NULL, hdcInvalid));

    // primary global bad parameter values, initialization.
    // by using objects which used to be valid but have been released, we guarantee 
    // we won't conflict with a handle value which is currently in use.
    g_tbmpBAD.SetBitmap(hbmpInvalid);
    g_tbmpBAD.SetVBitmap(hbmpInvalid);

    g_tdcBAD.SetDC(hdcInvalid, NON_PRIMARY_DC);
    g_tdcBAD.SetVDC(hdcInvalid);

    // secondary global bad paraemter values, intialization.
    // hpen and hbrush are arbitrary, as long as they're valid objects, but not valid
    // dc's/bitmaps.
    g_tbmpBAD2.SetBitmap((HBITMAP) g_hpenStock);
    g_tbmpBAD2.SetVBitmap((HBITMAP) g_hpenStock);

    g_tdcBAD2.SetDC((HDC) g_hbrushStock, NON_PRIMARY_DC);
    g_tdcBAD2.SetVDC((HDC) g_hbrushStock);
}

//**********************************************************************************
void
FreeVerify()
{
    // if we loaded up a secondary display driver, free it.
    if (g_hdcSecondary)
    {
        CheckNotRESULT(FALSE, DeleteDC(g_hdcSecondary));
        g_hdcSecondary = NULL;
    }

    // global bad paraemter values, reset them so we don't try to delete them.
    g_tbmpBAD.SetBitmap(NULL);
    g_tbmpBAD.SetVBitmap(NULL);

    g_tbmpBAD2.SetBitmap(NULL);
    g_tbmpBAD2.SetVBitmap(NULL);

    g_tdcBAD.SetDC(NULL, NULL);
    g_tdcBAD.SetVDC(NULL);

    g_tdcBAD2.SetDC(NULL, NULL);
    g_tdcBAD2.SetVDC(NULL);
}

void
ResetVerifyDriver()
{
    if(g_hdcSecondary)
    {
        CheckNotRESULT(FALSE, DeleteDC(g_hdcSecondary));
        CheckNotRESULT(NULL, g_hdcSecondary = CreateDC(gszVerifyDriverName, NULL, NULL, NULL));
        if(!g_hdcSecondary)
        {
            info(ABORT, TEXT("UNABLE TO RELOAD SECONDARY DISPLAY DRIVER!"));
            g_Verify = FALSE;
        }
    }
}

/***********************************************************************************
***
***  Verification control functions
***
************************************************************************************/

//**********************************************************************************
BOOL SetSurfVerify(BOOL bNewVerify)
{
    BOOL    OldVerify = g_TestVerify;

    g_TestVerify = bNewVerify;
    return OldVerify;
}

//**********************************************************************************
BOOL GetSurfVerify()
{
    return g_TestVerify;
}

//**********************************************************************************
inline  BOOL
DoVerify()
{
    return (g_Verify ? g_TestVerify : FALSE);
}

//**********************************************************************************
void
SetMaxErrorPercentage(DOUBLE MaxError)
{
    g_MaxErrors = MaxError;
    g_SurfaceVerifyCompareParameter = (BYTE *) &g_MaxErrors;
    gpfnSurfaceVerifyPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;
}

//**********************************************************************************
void
SetRMSPercentage(UINT Threshold, UINT AvgThreshold, DOUBLE PercentDiff, DOUBLE RMSRed, DOUBLE RMSGreen, DOUBLE RMSBlue)
{
    g_RMSCompareData.uiThreshold = Threshold;
    g_RMSCompareData.uiAvgThreshold = AvgThreshold;
    g_RMSCompareData.dfPercentageDiffPixels = PercentDiff;
    g_RMSCompareData.dResultRMSRed = RMSRed;
    g_RMSCompareData.dResultRMSGreen = RMSGreen;
    g_RMSCompareData.dResultRMSBlue = RMSBlue;

    g_SurfaceVerifyCompareParameter = (BYTE *) &g_RMSCompareData;
    gpfnSurfaceVerifyPixelCompare = (PFNCOMPAREPIXELS) &ComparePixelsRMS;
}

//**********************************************************************************
void
ResetCompareConstraints()
{
    g_MaxErrors = 0;
    g_SurfaceVerifyCompareParameter = (BYTE *) &g_MaxErrors;
    gpfnSurfaceVerifyPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;
}

/***********************************************************************************
***
*** my*()
***     overrides gdi functionality, called directly by the tests.
************************************************************************************/

//**********************************************************************************
TBITMAP myCreateDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, UINT type, struct MYBITMAPINFO *bmi, BOOL randSize)
{
    TBITMAP tbmp = NULL;

    HBITMAP hbmpUser = myCreateDIBSection(VALIDDC(tdc), ppvBits, d, w, h, type, bmi, randSize);
    HBITMAP hbmpVer = NULL;
    int         LastErrorSave = GetLastError();

    if (hbmpUser)
    {
        if (DoVerify())
        {
            LPVOID lpDummy = NULL;

            hbmpVer = myCreateDIBSection(VALIDVDC(tdc), &lpDummy, d, w, h, type, bmi, randSize);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                *ppvBits = NULL;
                return NULL;
            }
        }
        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;
}

/***********************************************************************************
***
*** my*()
***     overrides gdi functionality, called directly by the tests.
************************************************************************************/

//**********************************************************************************
TBITMAP myCreateRGBDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, int nBitMask, DWORD dwCompression, struct MYBITMAPINFO * UserBMI)
{
    TBITMAP tbmp = NULL;

    HBITMAP hbmpUser = myCreateRGBDIBSection(VALIDDC(tdc), ppvBits, d, w, h, nBitMask, dwCompression, UserBMI);
    HBITMAP hbmpVer = NULL;
    int         LastErrorSave = GetLastError();

    if (hbmpUser)
    {
        if (DoVerify())
        {
            LPVOID lpDummy = NULL;
            hbmpVer = myCreateRGBDIBSection(VALIDVDC(tdc), &lpDummy, d, w, h, nBitMask, dwCompression, UserBMI);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                *ppvBits = NULL;
                return NULL;
            }
        }
        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;
}

//**********************************************************************************
TDC myGetDC(HWND hWnd)
{
    HDC hdcPrim  = NULL,
        hdcSec   = NULL;
    TDC tdc      = NULL;
    int LastErrorSave = GetLastError();

    if (g_fUseGetDC)
        CheckNotRESULT(NULL, hdcPrim = GetDC(hWnd));
    else
        CheckNotRESULT(NULL, hdcPrim = CreateDC(gszDisplayPath, NULL, NULL, NULL));

    if (hdcPrim)
    {
        if(g_bRTL)
        {
            SetLayout(hdcPrim, LAYOUT_RTL);
        }
        else
        {
            SetLayout(hdcPrim, LAYOUT_LTR);
        }

        if (DoVerify())
        {
            hdcSec = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);

            if (!hdcSec)
            {
                info(ABORT, TEXT("myGetDC: unable to load the verification display driver: %s"), gszVerifyDriverName);
                DWORD dwLastError = GetLastError();
                ReleaseDC(hWnd, hdcPrim);
                SetLastError(dwLastError);
                return NULL;
            }
            else
            {
                if(g_bRTL)
                {
                    SetLayout(hdcSec, LAYOUT_RTL);
                }
                else
                {
                    SetLayout(hdcSec, LAYOUT_LTR);
                }
            }
        }
        tdc = new cTdc(hdcPrim, hdcSec, hWnd, NULL);
    }
    SetLastError(LastErrorSave);
    return tdc;
}

//**********************************************************************************
BOOL myReleaseDC(HWND hWnd, TDC tdc)
{
    BOOL    result = TRUE;
    BOOL    vresult = TRUE;
    int        LastErrorSave;
    if (tdc)
    {
        if (DoVerify() && IsWritableHDC(tdc))
        {
            SurfaceVerify(tdc, TRUE);
        }

        if (g_fUseGetDC)
            result = ReleaseDC(hWnd, tdc->GetDC());
        else
            result = DeleteDC(tdc->GetDC());
        tdc->SetDC(NULL, NULL);
        LastErrorSave = GetLastError();

        // if verification was turned off after creation, we
        // still need to release the DC
        if (tdc->GetVDC())
            vresult = DeleteDC(tdc->GetVDC());
        tdc->SetVDC(NULL);

        delete(tdc);

        SetLastError(LastErrorSave);
    }
    return (result && vresult);
}

/***********************************************************************************
***
*** GDI function overrides
***
************************************************************************************/

//**********************************************************************************
HBRUSH CreatePatternBrush(TBITMAP hbmp)
{
    return CreatePatternBrush(VALIDBMP(hbmp));
}

//**********************************************************************************
DWORD GetObjectType(TDC tdc)
{
    return GetObjectType(VALIDDC(tdc));
}

//**********************************************************************************
DWORD GetObjectType(TBITMAP tbmp)
{
    return GetObjectType(VALIDBMP(tbmp));
}

//**********************************************************************************
BOOL ScrollDC(TDC tdc, int dx, int dy, const RECT *lprcScroll, const RECT *lprcClip, HRGN hrgnUpdate, LPRECT lprcUpdate)
{
    BOOL    brvalUser = ScrollDC(VALIDDC(tdc), dx, dy, lprcScroll, lprcClip, hrgnUpdate, lprcUpdate);
    BOOL    brvalVer = brvalUser;
    int       LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        UINT nVerifyWidth  = GetDeviceCaps(VALIDVDC(tdc), HORZRES);
        UINT nVerifyHeight = GetDeviceCaps(VALIDVDC(tdc), VERTRES);

        // verify the size of our verification HDC matches the size of our test HDC
        if (zx != nVerifyWidth|| zy != nVerifyHeight)
        {
            // From MSDN: 
            //   If lprcClip is NULL, the entire client area is used. 
            //
            // We cannot allow the entire client area to be used for the verification driver if the verification
            // HDC is larger than our test HDC.
            //
            // To work around this, use the [zx, zy] size as our clipping region to ensure we only use the bits
            // within our expected boundary. If this is not done, we may scroll uninitialized content from our
            // verification HDC within the client area.
            RECT rcClip = { 0, 0, zx, zy };
            if (lprcClip)
            {
                CheckNotRESULT(FALSE, CopyRect(&rcClip, lprcClip));
            }

            brvalVer = ScrollDC(VALIDVDC(tdc), dx, dy, lprcScroll, &rcClip, NULL, NULL);
        }
        else
        {
            brvalVer = ScrollDC(VALIDVDC(tdc), dx, dy, lprcScroll, lprcClip, NULL, NULL);
        }

        SurfaceVerify(tdc);
    }
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL PatBlt(TDC tdc, int nXLeft, int nYLeft, int nWidth, int nHeight, DWORD dwRop)
{
    BOOL    brvalUser = PatBlt(VALIDDC(tdc), nXLeft, nYLeft, nWidth, nHeight, dwRop);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = PatBlt(VALIDVDC(tdc), nXLeft, nYLeft, nWidth, nHeight, dwRop);
        SurfaceVerify(tdc);
    }

    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL InvertRect(TDC tdc, RECT * rc)
{
    BOOL    brvalUser = InvertRect(VALIDDC(tdc), rc);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = InvertRect(VALIDVDC(tdc), rc);
        SurfaceVerify(tdc);
    }

    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
TBITMAP CreateDIBSection(TDC tdc, CONST BITMAPINFO * pbmi, UINT iUsage, VOID ** ppvBits, HANDLE hSection, DWORD dwOffset)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateDIBSection(VALIDDC(tdc), pbmi, iUsage, ppvBits, hSection, dwOffset);
    HBITMAP hbmpVer = NULL;
    int         LastErrorSave = GetLastError();

    if (hbmpUser)
    {
        if (DoVerify())
        {
            LPVOID lpDummy;
            hbmpVer = CreateDIBSection(VALIDVDC(tdc), pbmi, iUsage, &lpDummy, hSection, dwOffset);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }
        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;
}

LONG SetBitmapBits(TBITMAP tbmp, DWORD cBytes, CONST VOID *lpBits)
{
    LONG lrvalUser = SetBitmapBits(VALIDBMP(tbmp), cBytes, lpBits);
    LONG lrvalVer = lrvalUser;
    int     LastErrorSave = GetLastError();

    if(DoVerify())
    {
        lrvalVer = SetBitmapBits(VALIDVBMP(tbmp), cBytes, lpBits);
    }
    SetLastError(LastErrorSave);
    return lrvalUser;
}

//**********************************************************************************
BOOL BitBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TRUE);

    BOOL    brvalUser = BitBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, dwRop);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest) && IsWritableHDC(tdcSrc))
    {
        brvalVer = BitBlt(VALIDVDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDVDC(tdcSrc), nXSrc, nYSrc, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
TBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
    HBITMAP hbmpVer = NULL;
    int         LastErrorSave = GetLastError();

    if (hbmpUser == g_tbmpStock.GetBitmap())
    {
        tbmp = &g_tbmpStock;
    }
    else if (hbmpUser)
    {
        if (DoVerify())
        {
            hbmpVer = CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }
        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;
}

//**********************************************************************************
TBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = LoadBitmap(hInstance, lpBitmapName);
    HBITMAP hbmpVer = NULL;
    int         LastErrorSave = GetLastError();

    if (hbmpUser)
    {
        if (DoVerify())
        {
            hbmpVer = LoadBitmap(hInstance, lpBitmapName);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }
        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;

}

//**********************************************************************************
TBITMAP CreateCompatibleBitmap(TDC tdc, int nWidth, int nHeight)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateCompatibleBitmap(VALIDDC(tdc), nWidth, nHeight);
    HBITMAP hbmpVer = NULL;
    int          LastErrorSave = GetLastError();

    if (hbmpUser)
    {
        if (DoVerify())
        {
            hbmpVer = CreateCompatibleBitmap(VALIDVDC(tdc), nWidth, nHeight);
            if (!hbmpVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteObject(hbmpUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }

        tbmp = new cTBitmap(hbmpUser, hbmpVer);
    }
    SetLastError(LastErrorSave);
    return tbmp;
}

//**********************************************************************************
TDC CreateCompatibleDC(TDC tdc)
{
    TDC     tdctmp = NULL;
    HDC     hdcUser = CreateCompatibleDC(VALIDDC(tdc));
    HDC     hdcVer = NULL;
    int       LastErrorSave = GetLastError();

    if (hdcUser)
    {
        if (DoVerify())
        {
            hdcVer = CreateCompatibleDC(VALIDVDC(tdc));
            if (!hdcVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteDC(hdcUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }
        tdctmp = new cTdc(hdcUser, hdcVer, NON_PRIMARY_DC, &g_tbmpStock);
    }
    SetLastError(LastErrorSave);
    return tdctmp;
}

//**********************************************************************************
TDC TCreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData)
{
    TDC tdctemp = NULL;
    HDC hdcUser = CreateDC (lpszDriver, lpszDevice, lpszOutput, lpInitData);
    HDC hdcVer = NULL;
    int   LastErrorSave = GetLastError();

    if (hdcUser)
    {
        if(g_bRTL)
        {
            SetLayout(hdcUser, LAYOUT_RTL);
        }
        else
        {
            SetLayout(hdcUser, LAYOUT_LTR);
        }

        if (DoVerify())
        {
            hdcVer = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);
            if (!hdcVer)
            {
                info(ABORT, TEXT("TCreateDC: unable to load the verification display driver: %s"), gszVerifyDriverName);
                DWORD dwLastError = GetLastError();
                DeleteDC(hdcUser);
                SetLastError(dwLastError);
                return NULL;
            }
            else
            {
                if(g_bRTL)
                {
                    SetLayout(hdcVer, LAYOUT_RTL);
                }
                else
                {
                    SetLayout(hdcVer, LAYOUT_LTR);
                }
            }
        }
        tdctemp = new cTdc(hdcUser, hdcVer, NON_PRIMARY_DC, &g_tbmpStock);
    }
    SetLastError(LastErrorSave);
    return tdctemp;
}

//**********************************************************************************
BOOL DeleteDC(TDC tdc)
{
    BOOL    bTmpU = TRUE;
    BOOL    bTmpV = TRUE;
    int     LastErrorSave;

    if (tdc)
    {
        if (DoVerify() && IsWritableHDC(tdc))
        {
            SurfaceVerify(tdc, TRUE);
        }

        bTmpU = DeleteDC(VALIDDC(tdc));
        LastErrorSave = GetLastError();

        // if verification was turned off after creation, we
        // still need to release the DC
        if (tdc->GetVDC())
            bTmpV = DeleteDC(VALIDVDC(tdc));

        // keep our invalid HDC objects around for use later
        if (tdc != g_hdcBAD && tdc != g_hdcBAD2)
        {
            tdc->SetDC(NULL, NULL);
            tdc->SetVDC(NULL);

            delete tdc;
        }

        SetLastError(LastErrorSave);
    }
    return (bTmpU && bTmpV);
}

//**********************************************************************************
BOOL DrawEdge(TDC tdc, LPRECT lprc, UINT uEdgeType, UINT uFlags)
{
    RECT    rc;
    BOOL    brvalUser;
    BOOL    brvalVer;
    int        LastErrorSave;

    // DrawEdge modified the rect to be the new client area, but also uses it to define
    // the rectangle to draw, so the second call to DrawEdge will not be the same if the
    // second call uses the modified rectangle.
    if(lprc)
        rc = *lprc;

    brvalUser = DrawEdge(VALIDDC(tdc), lprc, uEdgeType, uFlags);
    brvalVer = brvalUser;
    LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = DrawEdge(VALIDVDC(tdc), &rc, uEdgeType, uFlags);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL DrawFocusRect(TDC tdc, CONST RECT * lprc)
{
    BOOL    brvalUser = DrawFocusRect(VALIDDC(tdc), lprc);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = DrawFocusRect(VALIDVDC(tdc), lprc);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
DrawText(TDC tdc, LPCWSTR lpszStr, int cchStr, RECT * lprc, UINT wFormat)
{
    int     irvalUser = DrawText(VALIDDC(tdc), lpszStr, cchStr, lprc, wFormat);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        irvalVer = DrawText(VALIDVDC(tdc), lpszStr, cchStr, lprc, wFormat);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    if (irvalUser == 0 || irvalVer == 0)
        return 0;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL Ellipse(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    BOOL    brvalUser = Ellipse(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = Ellipse(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
EnumFontFamilies(TDC tdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam)
{
    return EnumFontFamilies(VALIDDC(tdc), lpszFamily, lpEnumFontFamProc, lParam);
}

int
EnumFontFamiliesEx(TDC tdc, LPLOGFONT lpLogFont, FONTENUMPROC lpEnumFontFamExProc, LPARAM lParam, DWORD dwFlags)
{
    return EnumFontFamiliesEx(VALIDDC(tdc), lpLogFont, lpEnumFontFamExProc, lParam, dwFlags);
}

//**********************************************************************************
int
EnumFonts(TDC tdc, LPCWSTR lpszFaceName, FONTENUMPROC lpEnumFontProc, LPARAM lParam)
{
    return EnumFonts(VALIDDC(tdc), lpszFaceName, lpEnumFontProc, lParam);
}

//**********************************************************************************
int
ExcludeClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    int     irvalUser = ExcludeClipRect(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        irvalVer = ExcludeClipRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    if (irvalUser == ERROR || irvalVer == ERROR)
        return ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL ExtTextOut(TDC tdc, int X, int Y, UINT fuOptions, CONST RECT * lprc, LPCWSTR lpszString, UINT cbCount, CONST INT * lpDx)
{
    BOOL    brvalUser = ExtTextOut(VALIDDC(tdc), X, Y, fuOptions, lprc, lpszString, cbCount, lpDx);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = ExtTextOut(VALIDVDC(tdc), X, Y, fuOptions, lprc, lpszString, cbCount, lpDx);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
UINT SetTextAlign(TDC tdc, UINT fMode)
{
    UINT    uirvalUser = SetTextAlign(VALIDDC(tdc), fMode);
    UINT    uirvalVer = uirvalUser;
    int       LastErrorSave = GetLastError();

    if (DoVerify())
        uirvalVer = SetTextAlign(VALIDVDC(tdc), fMode);
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    if (uirvalUser == GDI_ERROR || uirvalVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return uirvalUser;
}

//**********************************************************************************
UINT GetTextAlign(TDC tdc)
{
    return GetTextAlign(VALIDDC(tdc));
}

//**********************************************************************************
int
FillRect(TDC tdc, CONST RECT * lprc, HBRUSH hbr)
{
    int     irvalUser = FillRect(VALIDDC(tdc), lprc, hbr);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        irvalVer = FillRect(VALIDVDC(tdc), lprc, hbr);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    if (irvalUser == 0 || irvalVer == 0)
        return 0;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL
GradientFill(TDC tdc, PTRIVERTEX pVertex, ULONG dwNumVertex,  PVOID pMesh, ULONG dwNumMesh,   ULONG dwMode)
{

    BOOL brvalUser = gpfnGradientFillInternal(VALIDDC(tdc), pVertex, dwNumVertex, pMesh, dwNumMesh, dwMode);
    BOOL brvalVer = brvalUser;
    int     LastErrorSave = GetLastError();

    if(DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = gpfnGradientFillInternal(VALIDVDC(tdc), pVertex, dwNumVertex, pMesh, dwNumMesh, dwMode);
        SurfaceVerify(tdc);
    }

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}
//**********************************************************************************
COLORREF GetBkColor(TDC tdc)
{
    return GetBkColor(VALIDDC(tdc));
}

//**********************************************************************************
int
GetBkMode(TDC tdc)
{
    return GetBkMode(VALIDDC(tdc));
}

//**********************************************************************************
int
GetClipRgn(TDC tdc, HRGN hrgn)
{
    return GetClipRgn(VALIDDC(tdc), hrgn);
}

//**********************************************************************************
int
GetClipBox(TDC tdc, LPRECT lprc)
{
    return GetClipBox(VALIDDC(tdc), lprc);
}

//**********************************************************************************
HGDIOBJ GetCurrentObject(TDC tdc, UINT uObjectType)
{
    TBITMAP tbmp = NULL;
    HGDIOBJ hobjUser = GetCurrentObject(VALIDDC(tdc), uObjectType);
    int         LastErrorSave = GetLastError();

    // if we have a valid hobj and it's not a bitmap or a pen/brush/etc.
    if (hobjUser && uObjectType == OBJ_BITMAP)
    {
        // it's a bitmap, so get the bitmap of our current dc
        tbmp = VALIDBMP(tdc);
        // make sure that they match; just in case, they should always match though,
        // because we update our dc to match whenever we select object.
        if (tbmp->GetBitmap() == hobjUser)
            hobjUser = (HGDIOBJ) tbmp;
        else
            hobjUser = NULL;
    }
    SetLastError(LastErrorSave);
    return hobjUser;
}

//**********************************************************************************
int
GetDeviceCaps(TDC tdc, int nIndex)
{
    return GetDeviceCaps(VALIDDC(tdc), nIndex);
}

//**********************************************************************************
COLORREF GetNearestColor(TDC tdc, COLORREF crColor)
{
    return GetNearestColor(VALIDDC(tdc), crColor);
}

//**********************************************************************************
COLORREF GetPixel(TDC tdc, int nXPos, int nYPos)
{
    NOPRINTERDC(NULL, 0);

    return GetPixel(VALIDDC(tdc), nXPos, nYPos);
}

//**********************************************************************************
COLORREF GetTextColor(TDC tdc)
{
    return GetTextColor(VALIDDC(tdc));
}

//**********************************************************************************
BOOL GetTextExtentExPoint(TDC tdc, LPCWSTR lpszStr, int cchString, int nMaxExtent, LPINT lpnFit, LPINT alpDx, LPSIZE lpSize)
{
    return GetTextExtentExPoint(VALIDDC(tdc), lpszStr, cchString, nMaxExtent, lpnFit, alpDx, lpSize);
}

//**********************************************************************************
int
GetTextFace(TDC tdc, int nCount, LPWSTR lpFaceName)
{
    return GetTextFace(VALIDDC(tdc), nCount, lpFaceName);
}

//**********************************************************************************
BOOL GetTextMetrics(TDC tdc, LPTEXTMETRIC lptm)
{
    return GetTextMetrics(VALIDDC(tdc), lptm);
}

//**********************************************************************************
BOOL GetCharWidth32(TDC tdc, UINT iFirstChar, UINT iLastChar, LPINT lpBuffer)
{
    return GetCharWidth32(VALIDDC(tdc), iFirstChar, iLastChar, lpBuffer);
}

//**********************************************************************************
BOOL GetCharABCWidths(TDC tdc, UINT iFirstChar, UINT iLastChar, LPABC lpBuffer)
{
    return GetCharABCWidths(VALIDDC(tdc), iFirstChar, iLastChar, lpBuffer);
}

//**********************************************************************************
int
IntersectClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    int     irvalUser = IntersectClipRect(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        irvalVer = IntersectClipRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    if (irvalUser == ERROR || irvalVer == ERROR)
        return ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TRUE);

    BOOL    brvalUser = MaskBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, VALIDBMP(hbmMask), xMask, yMask, dwRop);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest) && IsWritableHDC(tdcSrc))
    {
        brvalVer = MaskBlt(VALIDVDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDVDC(tdcSrc), nXSrc, nYSrc, VALIDVBMP(hbmMask), xMask, yMask, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL MoveToEx(TDC tdc, int X, int Y, LPPOINT lpPoint)
{
    BOOL    brvalUser = MoveToEx(VALIDDC(tdc), X, Y, lpPoint);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify())
        brvalVer = MoveToEx(VALIDVDC(tdc), X, Y, lpPoint);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL LineTo(TDC tdc, int nXEnd, int nYEnd)
{
    BOOL    brvalUser = LineTo(VALIDDC(tdc), nXEnd, nYEnd);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = LineTo(VALIDVDC(tdc), nXEnd, nYEnd);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL GetCurrentPositionEx(TDC tdc, LPPOINT lpPoint)
{
    return GetCurrentPositionEx(VALIDDC(tdc), lpPoint);
}

//**********************************************************************************
BOOL Polygon(TDC tdc, CONST POINT * lpPoints, int nCount)
{
    BOOL    brvalUser = Polygon(VALIDDC(tdc), lpPoints, nCount);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = Polygon(VALIDVDC(tdc), lpPoints, nCount);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, nCount);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL Polyline(TDC tdc, CONST POINT * lppt, int cPoints)
{
    BOOL    brvalUser = Polyline(VALIDDC(tdc), lppt, cPoints);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = Polyline(VALIDVDC(tdc), lppt, cPoints);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, cPoints);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL Rectangle(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    BOOL    brvalUser = Rectangle(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = Rectangle(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL RestoreDC(TDC tdc, int nSavedDC)
{
    BOOL    brvalUser = RestoreDC(VALIDDC(tdc), nSavedDC);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (brvalUser)
    {
        if (tdc)
            tdc->PopBitmap(nSavedDC);
    }

    if (DoVerify())
        brvalVer = RestoreDC(VALIDVDC(tdc), nSavedDC);

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL RoundRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect, int nWidth, int nHeight)
{
    BOOL    brvalUser = RoundRect(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect, nWidth, nHeight);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = RoundRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect, nWidth, nHeight);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
SaveDC(TDC tdc)
{
    int     UserSaveDC = SaveDC(VALIDDC(tdc));
    int     VerSaveDC = UserSaveDC;
    int     LastErrorSave = GetLastError();

    if (tdc)
        tdc->PushBitmap();

    if (DoVerify())
        VerSaveDC = SaveDC(VALIDVDC(tdc));

    // two seperate stacks, the saves/restores should be in lock step, so
    // all indicies should match.
    assert(UserSaveDC == VerSaveDC);

    SetLastError(LastErrorSave);
    return UserSaveDC;
}

//**********************************************************************************
int
SelectClipRgn(TDC tdc, HRGN hrgn)
{
    int     irvalUser = SelectClipRgn(VALIDDC(tdc), hrgn);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    // if verification was turned off part way through, we still
    // want to select/deselect clip regions to release them to
    // delete the dc's, selecting a clip retion into a NULL dc is harmless
    irvalVer = SelectClipRgn(VALIDVDC(tdc), hrgn);

    SetLastError(LastErrorSave);
    if (irvalUser == ERROR || (DoVerify() &&  irvalVer == ERROR))
        return ERROR;
    return irvalUser;
}

//**********************************************************************************
HGDIOBJ SelectObject(TDC tdc, HGDIOBJ hgdiobj)
{
    HGDIOBJ hobjUser = SelectObject(VALIDDC(tdc), hgdiobj);
    HGDIOBJ hobjVer = hobjUser;
    int         LastErrorSave = GetLastError();

    if (DoVerify())
        hobjVer = SelectObject(VALIDVDC(tdc), hgdiobj);

    SetLastError(LastErrorSave);
    if (hobjUser == NULL || hobjVer == NULL)
    {
        return NULL;
    }
    return hobjUser;
}

//**********************************************************************************
TBITMAP SelectObject(TDC tdc, TBITMAP hgdiobj)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = (HBITMAP) SelectObject(VALIDDC(tdc), VALIDBMP(hgdiobj));
    HBITMAP hbmpVer = NULL;
    int          LastErrorSave = GetLastError();

    // if verification was turned off part way through, we still want to select.
    // selecting something into a NULL dc is harmless
    hbmpVer = (HBITMAP) SelectObject(VALIDVDC(tdc), VALIDVBMP(hgdiobj));

    if (hbmpUser && tdc)
    {
        tbmp = tdc->SelectBitmap(hgdiobj);
    }

    SetLastError(LastErrorSave);
    return tbmp;
}

//**********************************************************************************
int
GetObject(TBITMAP hgdiobj, int cbBuffer, LPVOID lpvObject)
{
    return GetObject(VALIDBMP(hgdiobj), cbBuffer, lpvObject);
}

//**********************************************************************************
BOOL DeleteObject(TBITMAP hObject)
{
    BOOL    brvalUser = DeleteObject(VALIDBMP(hObject));
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    // if verification was turned off part way through, we still
    // want to delete the bitmap, deleting a NULL bitmap is harmless
    brvalVer = DeleteObject(VALIDVBMP(hObject));

    // if the user tries to delete the stock bitmap, don't do it...
    // but still return the gdi result as if they did.
    // if the user deletion above fails, then the object is still valid, so don't delete leak the handle.
    if (brvalUser && hObject && hObject != &g_tbmpStock)
    {
        hObject->SetBitmap(NULL);
        hObject->SetVBitmap(NULL);
        delete  hObject;
    }

    SetLastError(LastErrorSave);
    // user result if !DoVerify, user & ver if do verify
    return (brvalUser && (!DoVerify() || (DoVerify() && brvalVer)));

}

//**********************************************************************************
COLORREF SetBkColor(TDC tdc, COLORREF crColor)
{
    COLORREF crvalUser = SetBkColor(VALIDDC(tdc), crColor);
    COLORREF crvalVer = crvalUser;
    int           LastErrorSave = GetLastError();

    if (DoVerify())
        crvalVer = SetBkColor(VALIDVDC(tdc), crColor);

    SetLastError(LastErrorSave);
    if (crvalUser == CLR_INVALID || crvalVer == CLR_INVALID)
        return CLR_INVALID;
    else
        return crvalUser;
}

//**********************************************************************************
int
SetBkMode(TDC tdc, int iBkMode)
{
    int     irvalUser = SetBkMode(VALIDDC(tdc), iBkMode);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        irvalVer = SetBkMode(VALIDVDC(tdc), iBkMode);

    SetLastError(LastErrorSave);
    if (irvalUser == 0 || irvalVer == 0)
        return NULL;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL SetBrushOrgEx(TDC tdc, int nXOrg, int nYOrg, LPPOINT lppt)
{
    BOOL    brvalUser = SetBrushOrgEx(VALIDDC(tdc), nXOrg, nYOrg, lppt);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify())
        brvalVer = SetBrushOrgEx(VALIDVDC(tdc), nXOrg, nYOrg, lppt);

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int SetStretchBltMode(TDC tdc, int nMode)
{
    int nModeUser = SetStretchBltMode(VALIDDC(tdc), nMode);
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = SetStretchBltMode(VALIDVDC(tdc), nMode);

    SetLastError(LastErrorSave);

    if (nModeUser == 0 || nModeVer == 0)
        return 0;
    else
        return nModeUser;
}

//**********************************************************************************
int GetStretchBltMode(TDC tdc)
{
    int nModeUser = GetStretchBltMode(VALIDDC(tdc));
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = GetStretchBltMode(VALIDVDC(tdc));

    SetLastError(LastErrorSave);

    if (nModeUser == 0 || nModeVer == 0)
        return 0;
    else
        return nModeUser;
}

//**********************************************************************************
int SetTextCharacterExtra(TDC tdc, int nExtra)
{
    int nModeUser = SetTextCharacterExtra(VALIDDC(tdc), nExtra);
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = SetTextCharacterExtra(VALIDVDC(tdc), nExtra);

    SetLastError(LastErrorSave);

    if (nModeUser == 0 || nModeVer == 0)
        return 0;
    else
        return nModeUser;
}

//**********************************************************************************
int GetTextCharacterExtra(TDC tdc)
{
    int nModeUser = GetTextCharacterExtra(VALIDDC(tdc));
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = GetTextCharacterExtra(VALIDVDC(tdc));

    SetLastError(LastErrorSave);

    if (nModeUser == 0 || nModeVer == 0)
        return 0;
    else
        return nModeUser;
}

//**********************************************************************************
DWORD GetLayout(TDC tdc)
{
    int nModeUser = GetLayout(VALIDDC(tdc));
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = GetLayout(VALIDVDC(tdc));

    SetLastError(LastErrorSave);

    if (nModeUser == GDI_ERROR || nModeVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return nModeUser;
}

//**********************************************************************************
DWORD SetLayout(TDC tdc, DWORD dwLayout)
{
    int nModeUser = SetLayout(VALIDDC(tdc), dwLayout);
    int nModeVer = nModeUser;
    int LastErrorSave = GetLastError();

    if (DoVerify())
        nModeVer = SetLayout(VALIDVDC(tdc), dwLayout);

    SetLastError(LastErrorSave);

    if (nModeUser == GDI_ERROR || nModeVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return nModeUser;
}

//**********************************************************************************
COLORREF SetPixel(TDC tdc, int X, int Y, COLORREF crColor)
{
    COLORREF crUser = SetPixel(VALIDDC(tdc), X, Y, crColor);
    COLORREF crVer = crUser;
    int           LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
        crVer = SetPixel(VALIDVDC(tdc), X, Y, crColor);
    PrinterMemCntrl(tdc, 1);

    SetLastError(LastErrorSave);
    if (crUser == -1 || crVer == -1)
        return (COLORREF)-1;
    else
        return crUser;
}

//**********************************************************************************
COLORREF SetTextColor(TDC tdc, COLORREF crColor)
{
    COLORREF crUser = SetTextColor(VALIDDC(tdc), crColor);
    COLORREF crVer = crUser;
    int           LastErrorSave = GetLastError();

    if (DoVerify())
        crVer = SetTextColor(VALIDVDC(tdc), crColor);

    SetLastError(LastErrorSave);
    if (crUser == CLR_INVALID || crVer == CLR_INVALID)
        return CLR_INVALID;
    else
        return crUser;
}

//**********************************************************************************
BOOL StretchBlt(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc, int nXOriginSrc,
                int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TRUE);

    BOOL    brvalUser = StretchBlt(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest) && IsWritableHDC(tdcSrc))
    {
        brvalVer = StretchBlt(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
StretchDIBits(TDC tdc, int XDest, int YDest, int nDestWidth, int nDestHeight, int XSrc, int YSrc, int nSrcWidth, int nSrcHeight,
              CONST VOID * lpBits, CONST BITMAPINFO * lpBitsInfo, UINT iUsage, DWORD dwRop)
{
    int     irvalUser = StretchDIBits(VALIDDC(tdc), XDest, YDest, nDestWidth, nDestHeight, XSrc, YSrc, nSrcWidth, nSrcHeight, lpBits, lpBitsInfo, iUsage, dwRop);

    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        irvalVer = StretchDIBits(VALIDVDC(tdc), XDest, YDest, nDestWidth, nDestHeight, XSrc, YSrc, nSrcWidth, nSrcHeight, lpBits, lpBitsInfo, iUsage, dwRop);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, PCINIT + 1);
    SetLastError(LastErrorSave);
    if (irvalUser == GDI_ERROR || irvalVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
int SetDIBitsToDevice(TDC tdc, int XDest, int YDest, DWORD dwWidth, DWORD dwHeight, int XSrc, int YSrc, UINT uStartScan, UINT cScanLines, CONST VOID *lpvBits, CONST BITMAPINFO *lpbmi, UINT fuColorUse)
{
    int     irvalUser = SetDIBitsToDevice(VALIDDC(tdc), XDest, YDest, dwWidth, dwHeight, XSrc, YSrc, uStartScan, cScanLines, lpvBits, lpbmi, fuColorUse);

    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        irvalVer = SetDIBitsToDevice(VALIDVDC(tdc), XDest, YDest, dwWidth, dwHeight, XSrc, YSrc, uStartScan, cScanLines, lpvBits, lpbmi, fuColorUse);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, PCINIT + 1);
    SetLastError(LastErrorSave);
    if (irvalUser == GDI_ERROR || irvalVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
HPALETTE SelectPalette(TDC tdc, HPALETTE hPal, BOOL bForceBackground)
{
    HPALETTE hpalUser = SelectPalette(VALIDDC(tdc), hPal, bForceBackground);
    HPALETTE hpalVer = hpalUser;
    int           LastErrorSave = GetLastError();

    if (DoVerify())
        hpalVer = SelectPalette(VALIDVDC(tdc), hPal, bForceBackground);

    SetLastError(LastErrorSave);

    if (hpalUser == NULL || hpalVer == NULL)
        return NULL;
    else
        return hpalUser;
}

//**********************************************************************************
UINT RealizePalette(TDC tdc)
{
    UINT    uirvalUser = RealizePalette(VALIDDC(tdc));
    UINT    uirvalVer = uirvalUser;
    int       LastErrorSave = GetLastError();

    if (DoVerify())
        uirvalVer = RealizePalette(VALIDVDC(tdc));

    SetLastError(LastErrorSave);

    if (uirvalUser == GDI_ERROR || uirvalVer == GDI_ERROR)
        return GDI_ERROR;
    else
        return uirvalUser;

}

//**********************************************************************************
UINT GetSystemPaletteEntries(TDC tdc, UINT iStart, UINT nEntries, LPPALETTEENTRY pPe)
{
    return GetSystemPaletteEntries(VALIDDC(tdc), iStart, nEntries, pPe);
}

//**********************************************************************************
UINT GetDIBColorTable(TDC tdc, UINT uStartIndex, UINT cEntries, RGBQUAD * pColors)
{
    return GetDIBColorTable(VALIDDC(tdc), uStartIndex, cEntries, pColors);
}

//**********************************************************************************
UINT SetDIBColorTable(TDC tdc, UINT uStartIndex, UINT cEntries, CONST RGBQUAD * pColors)
{
    UINT    uirvalUser = SetDIBColorTable(VALIDDC(tdc), uStartIndex, cEntries, pColors);
    UINT    uirvalVer = uirvalUser;
    int       LastErrorSave = GetLastError();

    if (DoVerify())
        uirvalVer = SetDIBColorTable(VALIDVDC(tdc), uStartIndex, cEntries, pColors);

    SetLastError(LastErrorSave);

    if (uirvalUser == 0 || uirvalVer == 0)
        return 0;
    else
        return uirvalUser;
}

//**********************************************************************************
BOOL FillRgn(TDC tdc, HRGN hrgn, HBRUSH hbr)
{
    BOOL    brvalUser = FillRgn(VALIDDC(tdc), hrgn, hbr);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdc))
    {
        brvalVer = FillRgn(VALIDVDC(tdc), hrgn, hbr);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
SetROP2(TDC tdc, int fnDrawMode)
{
    int     irvalUser = SetROP2(VALIDDC(tdc), fnDrawMode);
    int     irvalVer = irvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
    {
        irvalVer = SetROP2(VALIDVDC(tdc), fnDrawMode);
    }

    SetLastError(LastErrorSave);
    if (irvalUser == 0 || irvalVer == 0)
        return 0;
    else
        return irvalUser;
}

//**********************************************************************************
int
GetROP2(TDC tdc)
{
    return GetROP2(VALIDDC(tdc));
}

//**********************************************************************************
BOOL RectVisible(TDC tdc, CONST RECT * lprc)
{
    return RectVisible(VALIDDC(tdc), lprc);
}

//**********************************************************************************
BOOL SetViewportOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint)
{
    BOOL    brvalUser = SetViewportOrgEx(VALIDDC(tdc), X, Y, lpPoint);
    BOOL    brvalVer = brvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        brvalVer = SetViewportOrgEx(VALIDVDC(tdc), X, Y, lpPoint);

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL GetViewportOrgEx(TDC tdc, LPPOINT lpPoint)
{
    return GetViewportOrgEx(VALIDDC(tdc), lpPoint);
}

//**********************************************************************************
BOOL GetViewportExtEx(TDC tdc, LPSIZE lpSize)
{
    return GetViewportExtEx(VALIDDC(tdc), lpSize);
}

//**********************************************************************************
BOOL SetWindowOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint)
{
    BOOL    brvalUser = SetWindowOrgEx(VALIDDC(tdc), X, Y, lpPoint);
    BOOL    brvalVer = brvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        brvalVer = SetWindowOrgEx(VALIDVDC(tdc), X, Y, lpPoint);

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL GetWindowOrgEx(TDC tdc, LPPOINT lpPoint)
{
    return GetWindowOrgEx(VALIDDC(tdc), lpPoint);
}

//**********************************************************************************
BOOL OffsetViewportOrgEx(TDC tdc, int X, int Y, LPPOINT lpPoint)
{
    BOOL    brvalUser = OffsetViewportOrgEx(VALIDDC(tdc), X, Y, lpPoint);
    BOOL    brvalVer = brvalUser;
    int     LastErrorSave = GetLastError();

    if (DoVerify())
        brvalVer = OffsetViewportOrgEx(VALIDVDC(tdc), X, Y, lpPoint);

    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL GetWindowExtEx(TDC tdc, LPSIZE lpSize)
{
    return GetWindowExtEx(VALIDDC(tdc), lpSize);
}

//**********************************************************************************
BOOL TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC hSrc,
                      int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor)
{
    NOPRINTERDC(hSrc, TRUE);

    BOOL    brvalUser = TransparentImage(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest))
    {
        brvalVer = TransparentImage(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TBITMAP hSrc,
                      int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor)
{
    BOOL    brvalUser = TransparentImage(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDBMP(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest))
    {
        brvalVer = TransparentImage(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVBMP(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL ExtEscape(TDC tdc, int iEscape, int cjInput, LPCSTR lpInData, int cjOutput, LPSTR lpOutData)
{
    return ExtEscape(VALIDDC(tdc), iEscape, cjInput, lpInData, cjOutput, lpOutData);
}

//**********************************************************************************
BOOL BitmapEscape(TBITMAP hbmp, int iEscape, int cjInput, LPCSTR lpszInData, int cjOutput, LPSTR lpszOutData)
{
    return BitmapEscape(VALIDBMP(hbmp), iEscape, cjInput, lpszInData, cjOutput, lpszOutData);
}

//**********************************************************************************
int    StartDoc(TDC hdc, CONST DOCINFO *lpdi)
{
    return gpfnStartDocInternal(VALIDDC(hdc), lpdi);
}

//**********************************************************************************
int    StartPage(TDC hdc)
{
    return gpfnStartPageInternal(VALIDDC(hdc));
}

//**********************************************************************************
int    EndPage(TDC hdc)
{
    return gpfnEndPageInternal(VALIDDC(hdc));
}

//**********************************************************************************
int    EndDoc(TDC hdc)
{
    return gpfnEndDocInternal(VALIDDC(hdc));
}

//**********************************************************************************
int    AbortDoc(TDC hdc)
{
    return gpfnAbortDocInternal(VALIDDC(hdc));
}

//**********************************************************************************
int    SetAbortProc(TDC hdc, ABORTPROC ap)
{
    return gpfnSetAbortProcInternal(VALIDDC(hdc), ap);
}

//**********************************************************************************
BOOL AlphaBlend(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc,
                         int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction)
{
    NOPRINTERDC(tdcSrc, TRUE);

    BOOL    brvalUser = gpfnAlphaBlendInternal(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, blendFunction);
    BOOL    brvalVer = brvalUser;
    int        LastErrorSave = GetLastError();

    if (DoVerify() && IsWritableHDC(tdcDest) && IsWritableHDC(tdcSrc))
    {
        brvalVer = gpfnAlphaBlendInternal(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, blendFunction);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    SetLastError(LastErrorSave);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
// PrinterMemCntrl scales the number of meta records which are created to avoid OOMing the system
// from meta files.
int PrinterMemCntrl(HDC hdc, int decr)
{
    if (GetDeviceCaps(hdc, TECHNOLOGY) != DT_RASPRINTER)
    {
        return -1;
    }

    // check if it's time to flush our meta record
    g_iPrinterCntr -= decr;
    if (g_iPrinterCntr < 0)
    {
        HPEN hPen;
        HBRUSH hBrush;
        HFONT hFont;
        HPALETTE hPalette;
        COLORREF crBkColor;
        COLORREF crTextColor;
        int nBkMode;
        int nRop2;
        UINT unTextAlign;
        POINT ptViewportOrgEx;

        // since we deviate from documented MSDN behavior, we need to internally retain the 
        // state of our device context when switching pages, so save our attributes now
        CheckNotRESULT(NULL, hPen     = (HPEN)    GetCurrentObject(hdc, OBJ_PEN));
        CheckNotRESULT(NULL, hBrush   = (HBRUSH)  GetCurrentObject(hdc, OBJ_BRUSH));
        CheckNotRESULT(NULL, hFont    = (HFONT)   GetCurrentObject(hdc, OBJ_FONT));
        CheckNotRESULT(NULL, hPalette = (HPALETTE)GetCurrentObject(hdc, OBJ_PAL));
        CheckNotRESULT(CLR_INVALID, crTextColor = GetTextColor(hdc));
        CheckNotRESULT(CLR_INVALID, crBkColor = GetBkColor(hdc));
        CheckNotRESULT(0, nBkMode = GetBkMode(hdc));
        CheckNotRESULT(0, nRop2 = GetROP2(hdc));
        CheckNotRESULT(GDI_ERROR, unTextAlign = GetTextAlign(hdc));
        CheckNotRESULT(0, GetViewportOrgEx(hdc, &ptViewportOrgEx));

        // create a new meta record
        gpfnEndPageInternal(hdc);
        gpfnStartPageInternal(hdc);

        // reset the internal counter
        g_iPrinterCntr = PCINIT;

        // restore the original settings
        CheckNotRESULT(NULL, SelectObject(hdc, hPen));
        CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, hFont));
        CheckNotRESULT(NULL, SelectPalette(hdc, hPalette, FALSE));
        CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, crTextColor));
        CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, crBkColor));
        CheckNotRESULT(0, SetBkMode(hdc, nBkMode));
        CheckNotRESULT(0, SetROP2(hdc, nRop2));
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, unTextAlign));
        CheckNotRESULT(0, SetViewportOrgEx(hdc, ptViewportOrgEx.x, ptViewportOrgEx.y, NULL));
    }
    return g_iPrinterCntr;
}

int PrinterMemCntrl(TDC tdc, int decr)
{
    return PrinterMemCntrl(VALIDDC(tdc), decr);;
}

#else

/***********************************************************************************
***
*** Used when compiling with hdc's instead of tdc's.
***
************************************************************************************/

BOOL IsWritableHDC(HDC hdc)
{
    return TRUE;
}

BOOL IsCompositorEnabled(void)
{
    return FALSE;
}

DWORD GetBackbufferBPP(void)
{
    return 0x00;
}

void OutputBitmap(HDC hdc, int nWidth, int nHeight)
{
    TCHAR   szFileName[MAX_PATH] = {NULL};

    if(g_OutputBitmaps)
    {
        // save the primary
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%s%d-PFailure.bmp"), gszBitmapDestination, g_nFailureNum);
        info(DETAIL, TEXT("Failure BMP id is %d"), g_nFailureNum);
        SaveSurfaceToBMP(hdc, szFileName, nWidth, nHeight);
        g_nFailureNum++;
    }
}

void OutputScreenHalvesBitmap(HDC hdc, int nWidth, int nHeight)
{
    TCHAR   szFileName[MAX_PATH] = {NULL};
    HDC hdcTmp;
    HBITMAP hbmpTmp, hbmpStock;

    if(g_OutputBitmaps)
    {
        // save the primary
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%s%d-PFailure.bmp"), gszBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdc, szFileName, nWidth, nHeight);

        // save a left/right diff
        hdcTmp = CreateCompatibleDC(hdc);
        hbmpTmp = CreateCompatibleBitmap(hdc, nWidth/2, nHeight);
        hbmpStock = (TBITMAP) SelectObject(hdcTmp, hbmpTmp);
        BitBlt(hdcTmp, 0, 0, nWidth/2, nHeight, hdc, 0, 0, SRCCOPY);
        BitBlt(hdcTmp, 0, 0, nWidth/2, nHeight, hdc, nWidth/2, 0, SRCINVERT);
        _sntprintf_s(szFileName, countof(szFileName) - 1, TEXT("%s%d-DFailure.bmp"), gszBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdcTmp, szFileName, nWidth, nHeight);

        info(DETAIL, TEXT("Failure BMP id is %d"), g_nFailureNum);

        g_nFailureNum++;
    }
}

//**********************************************************************************
HBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName)
{
    return LoadBitmap(hInstance, lpBitmapName);
}

//**********************************************************************************
HBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits)
{
    return CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
}

//**********************************************************************************
HDC myGetDC(HWND hWnd)
{
    return GetDC(hWnd);
}

//**********************************************************************************
BOOL myReleaseDC(HWND hWnd, HDC hdc)
{
    return ReleaseDC(hWnd, hdc);
}


//**********************************************************************************
HDC     TCreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData)
{
    return CreateDC(lpszDriver, lpszDevice, lpszOutput, lpInitData);
}

//**********************************************************************************
void SurfaceVerify(TDC tdc, BOOL force, BOOL expected) {}
void CompareSurface(TDC tdcActual, TDC tdcExpected) {}
void ResetVerifyDriver() {}

// invalid pointers for use in bad parameter testing of dynamically linked api's.
HBITMAP g_hbmpBAD;
HBITMAP g_hbmpBAD2;
HDC g_hdcBAD;
HDC g_hdcBAD2;

void
InitVerify()
{
    HDC     hdcInvalid  = NULL;
    HBITMAP hbmpInvalid = NULL;

    // create invalid objects by immediately releasing but holding on to their handle
    CheckNotRESULT(NULL, hbmpInvalid = CreateBitmap(1, 1, 1, 1, NULL));
    CheckNotRESULT(0, DeleteObject(hbmpInvalid));

    CheckNotRESULT(NULL, hdcInvalid = GetDC(NULL));
    CheckNotRESULT(0, ReleaseDC(NULL, hdcInvalid));

    // primary global bad parameter values, initialization.
    // by using objects which used to be valid but have been released, we guarantee 
    // we won't conflict with a handle value which is currently in use.
    g_hbmpBAD = hbmpInvalid;
    g_hdcBAD = hdcInvalid;

    // secondary global bad paraemter values, intialization.
    // these are setup in the stock object setup function, so we have to set them at run time.
    // hpen and hbrush are arbitrary, as long as they're valid objects, but not valid
    // dc's/bitmaps.
    g_hbmpBAD2 = (TBITMAP) g_hpenStock;
    g_hdcBAD2 = (TDC) g_hbrushStock;
}

void
FreeVerify()
{
}
#endif
