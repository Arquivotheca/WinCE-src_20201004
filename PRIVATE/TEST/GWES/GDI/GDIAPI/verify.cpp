//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "verify.h"

#define VALIDDC(tdc) tdc?tdc->GetDC():NULL
#define VALIDVDC(tdc) tdc?tdc->GetVDC():NULL
#define VALIDBMP(hbmp) hbmp?hbmp->GetBitmap():NULL
#define VALIDVBMP(hbmp) hbmp?hbmp->GetVBitmap():NULL
#define MaxStackSize 75

// NoPrinterDC can be called with both TDC's and HDC's
#define NOPRINTERDC(dc, str, retval) \
if ((dc) && ISPRINTERDC(dc)) \
{ \
    return (retval); \
}

/***********************************************************************************
***
*** MyCreateDIBSection(HDC...) 
***     shared between verify and global
************************************************************************************/

//**********************************************************************************
HBITMAP myCreateDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, UINT Usage, BOOL randSize)
{
    DWORD   dwError;

    HBITMAP hBmp = NULL;
    BITMAPINFOHEADER temp;

    // 1 & 2
    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi2;

    // 4 bit
    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[16];
    }
    bmi4;

    //8 bit
    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[256];
    }
    bmi8;

    //24 bit
    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct;
    }
    bmi24;
    
    // 16 & 32 bit
    struct
    {
        BITMAPINFOHEADER bmih;
        DWORD   ct[3];
    }
    bmi32;

    temp.biSize = sizeof (BITMAPINFOHEADER);
    temp.biWidth = w;
    temp.biHeight = h;
    temp.biPlanes = 1;
    temp.biBitCount = (WORD)d;
    temp.biCompression = (temp.biBitCount < 16 || temp.biBitCount == 24) ? BI_RGB : BI_BITFIELDS;
    temp.biSizeImage = temp.biXPelsPerMeter = temp.biYPelsPerMeter = 0;
    temp.biClrUsed = temp.biClrImportant = 0;

    if(randSize && temp.biBitCount <= 8 && Usage != DIB_PAL_COLORS)
    {
        temp.biClrUsed = rand() % 256;
        if(temp.biClrUsed > 0)
            temp.biClrImportant = rand() % temp.biClrUsed;
        else temp.biClrImportant= 0;
    }

    if (temp.biBitCount == 1 || temp.biBitCount == 2)
    {
        bmi2.bmih = temp;
        setRGBQUAD(&bmi2.ct[0], 0x0, 0x0, 0x0);
        setRGBQUAD(&bmi2.ct[1], 0x55, 0x55, 0x55);
        setRGBQUAD(&bmi2.ct[2], 0xaa, 0xaa, 0xaa);
        setRGBQUAD(&bmi2.ct[3], 0xff, 0xff, 0xff);
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi2, Usage, ppvBits, NULL, NULL);
    }
    else if (temp.biBitCount == 4)
    {
        bmi4.bmih = temp;
        for (int i = 0; i < 16; i++)
        {
            bmi4.ct[i].rgbRed = bmi4.ct[i].rgbGreen = bmi4.ct[i].rgbBlue = (BYTE)(i << 4);
            bmi4.ct[i].rgbReserved = 0;
        }
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi4, Usage, ppvBits, NULL, NULL);
    }
    else if (temp.biBitCount == 8)
    {
        bmi8.bmih = temp;
        for (int i = 0; i < 256; i++)
        {
            bmi8.ct[i].rgbRed = bmi8.ct[i].rgbGreen = bmi8.ct[i].rgbBlue = (BYTE)i;
            bmi8.ct[i].rgbReserved = 0;
        }
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi8, Usage, ppvBits, NULL, NULL);
    }
    else if (temp.biBitCount == 24)
        {
            bmi24.bmih = temp;
            hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi24, Usage, ppvBits, NULL, NULL);
        }
    else
    {
        // we expect creation of of a DIB_RGB_COLORS 16, or 32bpp surface to fail.
        Usage = DIB_RGB_COLORS;
        bmi32.bmih = temp;
        if (temp.biBitCount == 16)
        {
            bmi32.ct[2] = 0x001F;
            bmi32.ct[1] = 0x07E0;
            bmi32.ct[0] = 0xF800;
        }
        else
        {
            bmi32.ct[2] = 0x000000FF;
            bmi32.ct[1] = 0x0000FF00;
            bmi32.ct[0] = 0x00FF0000;
        }
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi32, Usage, ppvBits, NULL, NULL);
    }
    if (!hBmp)
    {
        if(w*h == 0)
        {
            info(DETAIL, TEXT("Fail as expected: invalid width and height: CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d"),hdc, w, h, d, Usage,
                GetLastError());
        }
        // 8bpp should succeed with DIB_PAL_COLORS, everything else should fail.
        else if(Usage == DIB_PAL_COLORS && temp.biBitCount != 8)
            info(DETAIL, TEXT("Fail as expected: DIB_PAL_COLORS CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d) GLE:%d"),hdc, w, h, d, Usage, GetLastError());
        else if(!hdc && Usage == DIB_PAL_COLORS && temp.biBitCount == 8)
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
        if (*ppvBits == NULL || (hdc == NULL && Usage == DIB_PAL_COLORS && temp.biBitCount != 24 && temp.biBitCount > 8) || w*h == 0)
        {
            info(FAIL, TEXT("Succeeded CreateDIBSection(dc:%d, w:%d, h:%d, d:%d, u:%d): *ppvBIts = NULL: GLE:%d"), hdc, w, h, d, Usage, GetLastError());
            DeleteObject(hBmp);
            hBmp = NULL;
        }
    }

    return hBmp;
}

BOOL    g_OutputBitmaps;
static int g_nFailureNum = 0;

void
SaveSurfaceToBMP(HDC hdcSrc, TCHAR *pszName)
{
    HBITMAP hBmpStock = NULL;
    HBITMAP hDIB = NULL;         

    // Create a memory DC
    HDC hdc = CreateCompatibleDC(hdcSrc);

    if (NULL == hdc)
    {
         info(DETAIL, TEXT("DC creation failed"));
    }
    else
    {
        // Create a 24-bit bitmap
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = zx;
        bmi.bmiHeader.biHeight      = zy;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
                
        // Calculate the size of the image data
        DWORD dwImageDataSize = bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * (bmi.bmiHeader.biBitCount / 8);
                
        // Create and fill in a Bitmap File header
        BITMAPFILEHEADER bfh;
        memset(&bfh, 0, sizeof(bfh));
        bfh.bfType = 'M' << 8 | 'B';
        bfh.bfOffBits = sizeof(bfh) + sizeof(bmi);
        bfh.bfSize = bfh.bfOffBits + dwImageDataSize;

        // Create the DIB Section
        LPBYTE lpbBits = NULL;
        hDIB = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (VOID**)&lpbBits, NULL, 0);
        if (!hDIB || !lpbBits)
        {
            info(DETAIL, TEXT("DIB creation failed"));
        }
        else
        {
            // Select the DIB into the memory DC
            hBmpStock = (HBITMAP)SelectObject(hdc, hDIB);

            // initialize the source bitmap in case we're smaller than fullscreen.
            PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);

            // Copy from the surface DC to the bitmap DC
            BitBlt(hdc, 0, 0, zx, zy, hdcSrc, 0, 0, SRCCOPY);

            HANDLE file = CreateFile(pszName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (INVALID_HANDLE_VALUE != file)
            {
                DWORD cbWritten;

                WriteFile(file, (BYTE*)&bfh, sizeof(bfh), &cbWritten, NULL);
                WriteFile(file, (BYTE*)&bmi, sizeof(bmi), &cbWritten, NULL);
                WriteFile(file, (BYTE*)lpbBits, dwImageDataSize, &cbWritten, NULL);
                CloseHandle(file);
            }
            else info(DETAIL, TEXT("Failed to open file for outputting failure bitmap."));
        }
            
        // Remove our DIB section from our DC by putting the stock bitmap back.
        if (hdc && hBmpStock)
            SelectObject(hdc, hBmpStock);

        // Free our DIB Section
        if (hDIB)
            DeleteObject(hDIB);

        // Free our DIB DC
        if (hdc)
            DeleteDC(hdc);
    }
}

// if SURFACE_VERIFY is defined, from global.h, then we want to compile with the following functions.
#ifdef SURFACE_VERIFY

// all of the global variables for verification, only defined if verification is turned in when compiling.
static  HDC     g_hdcSecondary;
static  BOOL    g_Verify,
                g_TestVerify;
static  LPTSTR  gszVerifyDriverName = TEXT("ddi_test.dll");
static  float   g_MaxErrors;

static PFNGRADIENTFILLINTERNAL gpfnGradientFillInternal;
static PFNSTARTDOCINTERNAL gpfnStartDocInternal;
static PFNENDDOCINTERNAL gpfnEndDocInternal;
static PFNSTARTPAGEINTERNAL gpfnStartPageInternal;
static PFNENDPAGEINTERNAL gpfnEndPageInternal;

static bool g_InsufficientMemory;

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
static cTBitmap g_tbmpStock(NULL);;

//**********************************************************************************
class   cTdc
{
  public:
    cTdc(const HDC hdcUserIn, const HDC hdcVerificationIn, const TBITMAP tbmp) :
        m_UserHdc(hdcUserIn),
        m_VerificationHdc(hdcVerificationIn),
        m_tbmp(tbmp),
        m_stackIndex(0)
    {
        m_tbmpSaved[0] = &g_tbmpStock;
    }

    ~cTdc()
    {
        // we don't know whether the user dc came from GetDC or CreateCompatibleDC, so we'll
        // depend on the user to delete it.  The verification DC always comes from CreateDC.
        if (m_VerificationHdc != NULL)
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

    void    SetDC(const HDC InHDC)
    {
        m_UserHdc = InHDC;
    }

    void    SetVDC(const HDC InHDC)
    {
        m_VerificationHdc = InHDC;
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
    TBITMAP m_tbmp;
    TBITMAP m_tbmpSaved[MaxStackSize];
    int m_stackIndex;
};

// this function allows the other test cases and such to output Bitmaps
void
OutputBitmap(TDC tdc)
{
    TCHAR   szFileName[MAX_PATH];
    
    if(g_OutputBitmaps)
    {
        wsprintf(szFileName, TEXT("%s%d-PFailure.bmp"), gtcBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(VALIDDC(tdc), szFileName);
        info(DETAIL, TEXT("Failure BMP id is %d"), g_nFailureNum);
        g_nFailureNum++;
    }
}
// this function is used by verification to output bitmaps.
void
OutputBitmaps(HDC hdc, HDC hdcV)
{
    TCHAR   szFileName[MAX_PATH];
    HDC hdcDiff;
    HBITMAP hbmpDiff, hbmpStock;
    
    if(g_OutputBitmaps)
    {
        hdcDiff = CreateCompatibleDC(hdc);
        hbmpDiff = CreateCompatibleBitmap(hdc, zx, zy);
        hbmpStock = (HBITMAP) SelectObject(hdcDiff, hbmpDiff);

        // initialize the diff bitmap in case the sources are smaller than the screen
        PatBlt(hdcDiff, 0, 0, zx, zy, BLACKNESS);

        BitBlt(hdcDiff, 0, 0, zx, zy, hdcV, 0, 0, SRCCOPY);
        BitBlt(hdcDiff, 0, 0, zx, zy, hdc, 0, 0, SRCINVERT);
        wsprintf(szFileName, TEXT("%d-DFailure.bmp"), g_nFailureNum);
        wsprintf(szFileName, TEXT("%s%d-DFailure.bmp"), gtcBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdcDiff, szFileName);
        SelectObject(hdcDiff, hbmpStock);
        DeleteObject(hbmpDiff);
        DeleteObject(hdcDiff);
                              
        wsprintf(szFileName, TEXT("%d-PFailure.bmp"), g_nFailureNum);
        wsprintf(szFileName, TEXT("%s%d-PFailure.bmp"), gtcBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdc, szFileName);
        wsprintf(szFileName, TEXT("%d-VFailure.bmp"), g_nFailureNum);
        wsprintf(szFileName, TEXT("%s%d-VFailure.bmp"), gtcBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdcV, szFileName);
        info(DETAIL, TEXT("Failure BMP id is %d"), g_nFailureNum);
        g_nFailureNum++;
    }
}

/***********************************************************************************
***
*** Surface Verification function
*** based off of CheckAllWhite, except using two dib sections and comparing them, 
*** instead of comparing to white.
************************************************************************************/

//**********************************************************************************
int
SurfaceVerify(TDC tdc, BOOL force = FALSE)
{
    // Pick a random number between 0 and verifyfreq between verifications
    const int verifyfreq = 500;
    
    // static local counter variable, when it hits 0, we do a verify, otherwise we just decrement.
    static int tries = 0;
    DWORD   dwErrors = 0;

    if (!tdc)
        return 0;
    
    if (force || --tries <= 0)
    {
        int     bpp = 0,
                step = 0;
        DWORD   maxErrors = (DWORD)  (g_MaxErrors * zx * zy),
                dwMaxPrintMessages = 20,
                dwMask = 0,
                tempDWORD = 0,
                tempDWORDV = 0,
                dwError = 0;
        BOOL    bMatch = FALSE;
        BYTE   *lpBits = NULL,
               *lpBitsV = NULL,
               *pBitsExpected,
               *pBitsExpectedV;
        HDC     myHdc = NULL,
                myHdcV = NULL,
                hdc = NULL,
                hdcV = NULL;
        HBITMAP hBmp = NULL,
                stockBmp = NULL,
                hBmpV = NULL,
                stockBmpV = NULL;
        TBITMAP tbmpTmp;
        DIBSECTION ds;
        
        // reset the random counter for the next run.
        tries = rand() % verifyfreq;

        hdc = tdc->GetDC();
        hdcV = tdc->GetVDC();
        tbmpTmp = tdc->GetBitmap();

        // comparing palette entry #'s doesn't make sense when comparing two different display drivers.
        // copy paletted surfaces to a 16bpp surface for both drivers, and compare the colors realized.
        // compare at the bit depth specified for all non-paletted surfaces.
        bpp = max(myGetBitDepth(), 16);

        step = bpp / 8;

        if (bpp < 8)
            step = 1;    

        dwMask = (32 == bpp) ? 0x00FFFFFF : (0xFFFFFFFF >> (32 - bpp)); // 32 bits --> ignore alpha :
                                                                        // BLUE  = 0x000000FF
                                                                        // Green = 0x0000FF00
                                                                        // RED   = 0x00FF0000

        // make two compatible dc's
        if (NULL == (myHdc = CreateCompatibleDC(hdc)) || NULL == (myHdcV = CreateCompatibleDC(hdcV)))
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("CreateCompatibleDC(hdc) fail: Out of memory: err = %ld"), dwError);
            else
                info(FAIL, TEXT("CreateCompatibleDC(hdc) fail: err = %ld"), dwError);
            if (myHdc)
                DeleteDC(myHdc);
            if (myHdcV)
                DeleteDC(myHdcV);
            
            return 0;
        }

        int     clipx = zx,
                clipy = zy;
        RECT    rc;
        int iclipboxRval = GetClipBox(hdc, &rc);
        if ( iclipboxRval == SIMPLEREGION)
        {
            clipx = (rc.right < zx) ? rc.right : zx;
            clipy = (rc.bottom < zy) ? rc.bottom : zy;
        }
        else if(iclipboxRval == ERROR || iclipboxRval == COMPLEXREGION)
        {
            info(FAIL, TEXT("GetClipBox returned a complex region, or an error"));
        }
        
        SetLastError(0);
        


        if (NULL == (hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, bpp, clipx, -(clipy+1), DIB_RGB_COLORS)) ||
           NULL == (hBmpV = myCreateDIBSection(hdcV, (VOID **) & lpBitsV, bpp, clipx, -(clipy+1), DIB_RGB_COLORS)))
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("myCreateDIBSection() fail: Out of memory: err = %ld"), dwError);
            else
                info(FAIL, TEXT("myCreateDIBSection() fail: err = %ld"), dwError);
            if (hBmp)
                DeleteObject(hBmp);
            if (hBmpV)
                DeleteObject(hBmpV);

            DeleteDC(myHdc);
            DeleteDC(myHdcV);

            return 0;
        }

        // copy the bits
        stockBmp = (HBITMAP) SelectObject(myHdc, hBmp);
        stockBmpV = (HBITMAP) SelectObject(myHdcV, hBmpV);
        
        GetObject(hBmp, sizeof (DIBSECTION), &ds);
        if (ds.dsBm.bmHeight < 0)
            info(FAIL, TEXT("SurfaceVerify - GetObject returned bmp with negative height"));

        BitBlt(myHdc, 0, 0, clipx, clipy, hdc, 0, 0, SRCCOPY);
        BitBlt(myHdcV, 0, 0, clipx, clipy, hdcV, 0, 0, SRCCOPY);

        for (int y = 0; y < clipy && dwErrors <= maxErrors; y++)
        {
            // point the expected and actual pixel pointers to the beginning of the scanline
            pBitsExpected = lpBits + y * ds.dsBm.bmWidthBytes;
            pBitsExpectedV = lpBitsV + y * ds.dsBm.bmWidthBytes;

            for (int x = 0; x < clipx && dwErrors <= maxErrors; x++, pBitsExpected += step, pBitsExpectedV += step)
            {
                tempDWORD = *(DWORD UNALIGNED *) pBitsExpected;
                tempDWORDV = *(DWORD UNALIGNED *) pBitsExpectedV;
                bMatch = ((tempDWORD & dwMask) == (tempDWORDV & dwMask));

                if (!bMatch)
                {
                    ++dwErrors;
                    if(dwErrors < dwMaxPrintMessages)
                        info(DETAIL, TEXT("Driver verification at (%d, %d) Found (0x%06X) != (0x%06X)"), x, y, tempDWORD & dwMask, tempDWORDV & dwMask);
                }

                if (dwErrors > maxErrors)
                {
                    OutputBitmaps(myHdc, myHdcV);
                    info(FAIL, TEXT("Driver verification exiting due to large number of errors"));

                    if(GetPixel(myHdc, x, y) == GetPixel(myHdcV, x, y))
                        info(FAIL, TEXT("GetPixel for both DIB's match, however manual surface compare doesn't, possible color conversion or test bug"));
                    else if(GetPixel(hdc, x, y) == GetPixel(hdcV, x, y))
                        info(FAIL, TEXT("GetPixel for both surfaces match, however DIB doesn't, possible color conversion or test bug"));
                    else info(FAIL, TEXT("Colors according to GetPixel are Primary:%d, Verification:%d, Location:(%d, %d)"), GetPixel(hdc, x, y), GetPixel(hdcV, x, y), x, y);
                }
            }
        }
        
        SelectObject(myHdc, stockBmp);
        SelectObject(myHdcV, stockBmpV);
        DeleteObject(hBmp);
        DeleteObject(hBmpV);
        DeleteDC(myHdc);
        DeleteDC(myHdcV);
    }

    return (int) dwErrors;
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
    MEMORYSTATUS ms;
    RECT rc;
    DWORD dwMemNeeded = 0;
    DWORD dwArea;
    HDC     hdc = GetDC(NULL);
    HDC     hdcCompat = CreateCompatibleDC(hdc);
    HBITMAP hbmpStock;

    // init to verify true, so if the test is run without setting verification, it'll just default to have verification
    g_Verify = TRUE;
    g_TestVerify = TRUE;
    g_hdcSecondary = NULL;
    g_MaxErrors = 0;
    g_InsufficientMemory = FALSE;

    // test for insufficient memory after loading the verification driver,
    // to account for it's internal allocations of surfaces
    g_hdcSecondary = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);
    if(g_hdcSecondary)
    {
        // get the number of pixels on a full screen surface, so we can estimate the number of bytes needed
        // for a certain number of those surfaces
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
        dwArea = (rc.bottom - rc.top) * (rc.right - rc.left);
                    
        // memory needed (bytes) is the full screen area * largest bit depth * max fullscreens allocated (in sysmem) at one time
        // we need enough space for atleast 2 fullscreen DIB sections + a little, so we round up to 3.
        dwMemNeeded = (32/8 * dwArea) * 3;
        memset(&ms, 0, sizeof(ms));
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatus(&ms);

        // if the available memory (after loading the secondary display driver) is less than the
        // memory needed to run the test, then turn off verification.
        if(dwMemNeeded > ms.dwAvailPhys)
        {
            g_InsufficientMemory = TRUE;
            info(DETAIL, TEXT("Verification option disabled due to insufficient memory"));
        }
        else info(DETAIL, TEXT("Driver Verification is available"));

        DeleteDC(g_hdcSecondary);
        g_hdcSecondary = NULL;
    }
    else info(DETAIL, TEXT("Unable to create a dc for the verification driver, Driver Verification unavailable"));
    // if gpfngradientfill was set up in main, then we support gradient fill,
    // take over the function pointer and keep the original for internal use
    if(gpfnGradientFill)
    {
        info(DETAIL, TEXT("Gradient fill available in the image"));
        gpfnGradientFillInternal = (PFNGRADIENTFILLINTERNAL) gpfnGradientFill;
        gpfnGradientFill = & GradientFill;
    }
    else info(DETAIL, TEXT("Gradient fill unavailable to test"));

    if(gpfnStartDoc && gpfnEndDoc && gpfnStartPage && gpfnEndPage)
    {
        info(DETAIL, TEXT("Printing available in the image"));
        gpfnStartDocInternal = (PFNSTARTDOCINTERNAL) gpfnStartDoc;
        gpfnEndDocInternal = (PFNENDDOCINTERNAL) gpfnEndDoc;
        gpfnStartPageInternal = (PFNSTARTPAGEINTERNAL) gpfnStartPage;
        gpfnEndPageInternal = (PFNENDPAGEINTERNAL) gpfnEndPage;
        gpfnStartDoc = & StartDoc;
        gpfnEndDoc = & EndDoc;
        gpfnStartPage = & StartPage;
        gpfnEndPage = & EndPage;
    }
    else info(DETAIL, TEXT("Printing unavailable to test"));
    
    hbmpStock = (HBITMAP) GetCurrentObject(hdcCompat, OBJ_BITMAP);

    ReleaseDC(NULL, hdc);
    DeleteDC(hdcCompat);

    g_tbmpStock.SetBitmap(hbmpStock);
    g_tbmpStock.SetVBitmap(hbmpStock);


    // Because this is the CETK, and i always want driver verification, initilize it and turn it on here.
    // load one copy of the dc at the begining of the test, that way the driver isn't
    // loaded and unloaded between each and every test.
    if (!g_hdcSecondary && !g_InsufficientMemory)
        g_hdcSecondary = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);

    // if there's insufficient memory, don't verify.  if ddi_test failed to load, don't verify
    g_Verify = g_hdcSecondary && !g_InsufficientMemory;
}

//**********************************************************************************
void
FreeVerify()
{
    // if we loaded up a secondary display driver, free it.
    if (g_hdcSecondary)
        DeleteDC(g_hdcSecondary);
}

void
ResetVerifyDriver()
{
    if(g_hdcSecondary)
    {
        DeleteDC(g_hdcSecondary);
        g_hdcSecondary = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);
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
inline  BOOL
DoVerify()
{
    return (g_Verify ? g_TestVerify : FALSE);
}

//**********************************************************************************
float SetMaxErrorPercentage(float MaxError)
{
    float fMaxError = g_MaxErrors;
    g_MaxErrors = MaxError;
    return fMaxError;
}

/***********************************************************************************
***
*** my*()
***     overrides gdi functionality, called directly by the tests.
************************************************************************************/

//**********************************************************************************
TBITMAP myCreateDIBSection(TDC tdc, VOID ** ppvBits, int d, int w, int h, UINT type, BOOL randSize)
{
    TBITMAP tbmp = NULL;

    HBITMAP hbmpUser = myCreateDIBSection(VALIDDC(tdc), ppvBits, d, w, h, type, randSize);
    HBITMAP hbmpVer = NULL;

    if (hbmpUser)
    {
        if (DoVerify())
        {
            LPVOID lpDummy = NULL;
            hbmpVer = myCreateDIBSection(VALIDVDC(tdc), &lpDummy, d, w, h, type, randSize);
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

    return tbmp;
}

//**********************************************************************************
TDC myGetDC(HWND hWnd)
{
    HDC     hdcPrim = GetDC(hWnd),
            hdcSec = NULL;
    TDC     tdc = NULL;

    if (hdcPrim)
    {
        if (DoVerify())
        {
            hdcSec = CreateDC(gszVerifyDriverName, NULL, NULL, NULL);
            
            if (!hdcSec)
            {
                DWORD dwLastError = GetLastError();
                ReleaseDC(hWnd, hdcPrim);
                SetLastError(dwLastError);  
                return NULL;
            }          
        }
        tdc = new cTdc(hdcPrim, hdcSec, NULL);
    }

    return tdc;
}

//**********************************************************************************
BOOL myReleaseDC(HWND hWnd, TDC tdc)
{
    BOOL    result = TRUE;
    BOOL    vresult = TRUE;

    if (tdc)
    {
        if (DoVerify())
        {
            SurfaceVerify(tdc, TRUE);
        }

        // if verification was turned off after creation, we
        // still need to release the DC
        if (tdc->GetVDC())
            vresult = DeleteDC(tdc->GetVDC());
        tdc->SetVDC(NULL);
        
        result = ReleaseDC(hWnd, tdc->GetDC());
        tdc->SetDC(NULL);

        delete(tdc);
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

    if (DoVerify())
    {
        // the updated rectangles may be different because of the different clip regions associated with each
        // driver/surface (we always use the full surface on the verification driver, no clip region)
        brvalVer = ScrollDC(VALIDVDC(tdc), dx, dy, lprcScroll, lprcClip, NULL, NULL);
        SurfaceVerify(tdc);
    }

    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL PatBlt(TDC tdcDest, int nXLeft, int nYLeft, int nWidth, int nHeight, DWORD dwRop)
{
    BOOL    brvalUser = PatBlt(VALIDDC(tdcDest), nXLeft, nYLeft, nWidth, nHeight, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = PatBlt(VALIDVDC(tdcDest), nXLeft, nYLeft, nWidth, nHeight, dwRop);
        SurfaceVerify(tdcDest);
    }

    PrinterMemCntrl(tdcDest, 1);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL InvertRect(TDC tdcDest, RECT * rc)
{
    BOOL    brvalUser = InvertRect(VALIDDC(tdcDest), rc);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = InvertRect(VALIDVDC(tdcDest), rc);
        SurfaceVerify(tdcDest);
    }

    PrinterMemCntrl(tdcDest, 1);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
TBITMAP CreateDIBSection(TDC tdc, CONST BITMAPINFO * pbmi, UINT iUsage, VOID ** ppvBits, HANDLE hSection, DWORD dwOffset)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateDIBSection(VALIDDC(tdc), pbmi, iUsage, ppvBits, hSection, dwOffset);
    HBITMAP hbmpVer = NULL;

    if (hbmpUser)
    {
        if (DoVerify())
        {
            hbmpVer = CreateDIBSection(VALIDVDC(tdc), pbmi, iUsage, ppvBits, hSection, dwOffset);
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

    return tbmp;
}

//**********************************************************************************
BOOL BitBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TEXT("BitBlt"), TRUE)
    
    BOOL    brvalUser = BitBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = BitBlt(VALIDVDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDVDC(tdcSrc), nXSrc, nYSrc, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
// this is only used for bad param checking, no point in verifying.
BOOL BitBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop)
{
    BOOL    brvalUser = BitBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, tdcSrc, nXSrc, nYSrc, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
        brvalVer = BitBlt(VALIDVDC(tdcDest), nXDest, nYDest, nWidth, nHeight, tdcSrc, nXSrc, nYSrc, dwRop);
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
// this is only used for bad param checking, no point in verifying.
BOOL BitBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, DWORD dwRop)
{
    BOOL    brvalUser = BitBlt(tdcDest, nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, dwRop);
    BOOL    brvalVer = brvalUser;
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
TBITMAP myCreateBitmap(int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID * lpvBits)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
    HBITMAP hbmpVer = NULL;

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

    return tbmp;
}

//**********************************************************************************
TBITMAP myLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName)
{
    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = LoadBitmap(hInstance, lpBitmapName);
    HBITMAP hbmpVer = NULL;

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
    return tbmp;

}

//**********************************************************************************
TBITMAP CreateCompatibleBitmap(TDC tdc, int nWidth, int nHeight)
{

    TBITMAP tbmp = NULL;
    HBITMAP hbmpUser = CreateCompatibleBitmap(VALIDDC(tdc), nWidth, nHeight);
    HBITMAP hbmpVer = NULL;
    
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
    return tbmp;
}

//**********************************************************************************
TDC CreateCompatibleDC(TDC tdc)
{
    TDC     tdctmp = NULL;
    HDC     hdcUser = CreateCompatibleDC(VALIDDC(tdc));
    HDC     hdcVer = NULL;

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
        tdctmp = new cTdc(hdcUser, hdcVer, &g_tbmpStock);
    }

    return tdctmp;
}

//**********************************************************************************
TDC TCreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput, CONST DEVMODE *lpInitData)
{
    TDC tdctemp = NULL;
    HDC hdcUser = CreateDC (lpszDriver, lpszDevice, lpszOutput, lpInitData);
    HDC hdcVer = NULL;

    if (hdcUser)
    {
        if (DoVerify())
        {
            hdcVer = CreateCompatibleDC(hdcUser);
            if (!hdcVer)
            {
                DWORD dwLastError = GetLastError();
                DeleteDC(hdcUser);
                SetLastError(dwLastError);
                return NULL;
            }
        }
        tdctemp = new cTdc(hdcUser, hdcVer, &g_tbmpStock);
    }

    return tdctemp;
}

//**********************************************************************************
BOOL DeleteDC(TDC tdc)
{
    BOOL    bTmpU = DeleteDC(VALIDDC(tdc));
    BOOL    bTmpV = bTmpU;

    if(tdc)
        tdc->SetDC(NULL);

    if (DoVerify())
    {
        bTmpV = DeleteDC(VALIDVDC(tdc));
        if(tdc)
            tdc->SetVDC(NULL);
    }

    delete  tdc;

    return (bTmpU && bTmpV);
}

//**********************************************************************************
BOOL DrawEdge(TDC tdc, LPRECT lprc, UINT uEdgeType, UINT uFlags)
{
    RECT    rc;
    BOOL    brvalUser;
    BOOL    brvalVer;

    // DrawEdge modified the rect to be the new client area, but also uses it to define
    // the rectangle to draw, so the second call to DrawEdge will not be the same if the
    // second call uses the modified rectangle.
    if(lprc)
        rc = *lprc;

    brvalUser = DrawEdge(VALIDDC(tdc), lprc, uEdgeType, uFlags);
    brvalVer = brvalUser;
    if (DoVerify())
    {
        brvalVer = DrawEdge(VALIDVDC(tdc), &rc, uEdgeType, uFlags);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL DrawFocusRect(TDC tdc, CONST RECT * lprc)
{
    BOOL    brvalUser = DrawFocusRect(VALIDDC(tdc), lprc);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = DrawFocusRect(VALIDVDC(tdc), lprc);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
DrawTextW(TDC tdc, LPCWSTR lpszStr, int cchStr, RECT * lprc, UINT wFormat)
{
    int     irvalUser = DrawTextW(VALIDDC(tdc), lpszStr, cchStr, lprc, wFormat);
    int     irvalVer = irvalUser;

    if (DoVerify())
    {
        irvalVer = DrawTextW(VALIDVDC(tdc), lpszStr, cchStr, lprc, wFormat);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
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

    if (DoVerify())
    {
        brvalVer = Ellipse(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
EnumFontFamiliesW(TDC tdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam)
{
    return EnumFontFamiliesW(VALIDDC(tdc), lpszFamily, lpEnumFontFamProc, lParam);
}

//**********************************************************************************
int
EnumFontsW(TDC tdc, LPCWSTR lpszFaceName, FONTENUMPROC lpEnumFontProc, LPARAM lParam)
{
    return EnumFontsW(VALIDDC(tdc), lpszFaceName, lpEnumFontProc, lParam);
}

//**********************************************************************************
int
ExcludeClipRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    int     irvalUser = ExcludeClipRect(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    int     irvalVer = irvalUser;

    if (DoVerify())
        irvalVer = ExcludeClipRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    PrinterMemCntrl(tdc, 1);
    if (irvalUser == ERROR || irvalVer == ERROR)
        return ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL ExtTextOutW(TDC tdc, int X, int Y, UINT fuOptions, CONST RECT * lprc, LPCWSTR lpszString, UINT cbCount, CONST INT * lpDx)
{
    BOOL    brvalUser = ExtTextOutW(VALIDDC(tdc), X, Y, fuOptions, lprc, lpszString, cbCount, lpDx);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = ExtTextOutW(VALIDVDC(tdc), X, Y, fuOptions, lprc, lpszString, cbCount, lpDx);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
UINT SetTextAlign(TDC tdc, UINT fMode)
{
    UINT    uirvalUser = SetTextAlign(VALIDDC(tdc), fMode);
    UINT    uirvalVer = uirvalUser;

    if (DoVerify())
        uirvalVer = SetTextAlign(VALIDVDC(tdc), fMode);
    PrinterMemCntrl(tdc, 1);
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

    if (DoVerify())
    {
        irvalVer = FillRect(VALIDVDC(tdc), lprc, hbr);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
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

    if(DoVerify())
    {
        brvalVer = gpfnGradientFillInternal(VALIDVDC(tdc), pVertex, dwNumVertex, pMesh, dwNumMesh, dwMode);
        SurfaceVerify(tdc);
    }
    
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

    // if we have a valid hobj and it's not a bitmap or a pen/brush/etc.
    if (hobjUser && uObjectType == OBJ_BITMAP)
    {
        // it's a bitmap, so get the bitmap of our current dc
        tbmp = tdc->GetBitmap();
        // make sure that they match; just in case, they should always match though, 
        // because we update our dc to match whenever we select object.
        if (tbmp->GetBitmap() == hobjUser)
            hobjUser = (HGDIOBJ) tbmp;
        else
            hobjUser = NULL;
    }

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
    NOPRINTERDC(tdc, TEXT("GetPixel"), 0)
    return GetPixel(VALIDDC(tdc), nXPos, nYPos);
}

//**********************************************************************************
COLORREF GetTextColor(TDC tdc)
{
    return GetTextColor(VALIDDC(tdc));
}

//**********************************************************************************
BOOL GetTextExtentExPointW(TDC tdc, LPCWSTR lpszStr, int cchString, int nMaxExtent, LPINT lpnFit, LPINT alpDx, LPSIZE lpSize)
{
    return GetTextExtentExPointW(VALIDDC(tdc), lpszStr, cchString, nMaxExtent, lpnFit, alpDx, lpSize);
}

//**********************************************************************************
int
GetTextFaceW(TDC tdc, int nCount, LPWSTR lpFaceName)
{
    return GetTextFaceW(VALIDDC(tdc), nCount, lpFaceName);
}

//**********************************************************************************
BOOL GetTextMetricsW(TDC tdc, LPTEXTMETRIC lptm)
{
    return GetTextMetricsW(VALIDDC(tdc), lptm);
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

    if (DoVerify())
        irvalVer = IntersectClipRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    PrinterMemCntrl(tdc, 1);
    if (irvalUser == ERROR || irvalVer == ERROR)
        return ERROR;
    else
        return irvalUser;
}

//**********************************************************************************
BOOL MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TEXT("MaskBlt"), TRUE)

    BOOL    brvalUser = MaskBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, VALIDBMP(hbmMask), xMask, yMask, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = MaskBlt(VALIDVDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDVDC(tdcSrc), nXSrc, nYSrc, VALIDVBMP(hbmMask), xMask, yMask, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

// no verification done, for bad parameter testing only.
//**********************************************************************************
BOOL MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{ 
    BOOL    brvalUser = MaskBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, tdcSrc, nXSrc, nYSrc, VALIDBMP(hbmMask), xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{
    BOOL    brvalUser = MaskBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, hbmMask, xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MaskBlt(TDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{
    BOOL    brvalUser = MaskBlt(VALIDDC(tdcDest), nXDest, nYDest, nWidth, nHeight, tdcSrc, nXSrc, nYSrc, hbmMask, xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{ 
    BOOL    brvalUser = MaskBlt(tdcDest, nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, VALIDBMP(hbmMask), xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC tdcSrc, int nXSrc, int nYSrc, TBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{ 
    BOOL    brvalUser = MaskBlt(tdcDest, nXDest, nYDest, nWidth, nHeight, tdcSrc, nXSrc, nYSrc, VALIDBMP(hbmMask), xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MaskBlt(HDC tdcDest, int nXDest, int nYDest, int nWidth, int nHeight, TDC tdcSrc, int nXSrc, int nYSrc, HBITMAP hbmMask,
             int xMask, int yMask, DWORD dwRop)
{ 
    BOOL    brvalUser = MaskBlt(tdcDest, nXDest, nYDest, nWidth, nHeight, VALIDDC(tdcSrc), nXSrc, nYSrc, hbmMask, xMask, yMask, dwRop);
    return (brvalUser);
}

//**********************************************************************************
BOOL MoveToEx(TDC tdc, int X, int Y, LPPOINT lpPoint)
{
    BOOL    brvalUser = MoveToEx(VALIDDC(tdc), X, Y, lpPoint);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
        brvalVer = MoveToEx(VALIDVDC(tdc), X, Y, lpPoint);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL LineTo(TDC tdc, int nXEnd, int nYEnd)
{
    BOOL    brvalUser = LineTo(VALIDDC(tdc), nXEnd, nYEnd);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = LineTo(VALIDVDC(tdc), nXEnd, nYEnd);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
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

    if (DoVerify())
    {
        brvalVer = Polygon(VALIDVDC(tdc), lpPoints, nCount);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL Polyline(TDC tdc, CONST POINT * lppt, int cPoints)
{
    BOOL    brvalUser = Polyline(VALIDDC(tdc), lppt, cPoints);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = Polyline(VALIDVDC(tdc), lppt, cPoints);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL Rectangle(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
    BOOL    brvalUser = Rectangle(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = Rectangle(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL RestoreDC(TDC tdc, int nSavedDC)
{
    BOOL    brvalUser = RestoreDC(VALIDDC(tdc), nSavedDC);
    BOOL    brvalVer = brvalUser;

    if (brvalUser)
    {
        if (tdc)
            tdc->PopBitmap(nSavedDC);
    }

    if (DoVerify())
        brvalVer = RestoreDC(VALIDVDC(tdc), nSavedDC);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL RoundRect(TDC tdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect, int nWidth, int nHeight)
{
    BOOL    brvalUser = RoundRect(VALIDDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect, nWidth, nHeight);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = RoundRect(VALIDVDC(tdc), nLeftRect, nTopRect, nRightRect, nBottomRect, nWidth, nHeight);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
SaveDC(TDC tdc)
{
    int     UserSaveDC = SaveDC(VALIDDC(tdc));
    int     VerSaveDC = UserSaveDC;

    if (tdc)
        tdc->PushBitmap();

    if (DoVerify())
        VerSaveDC = SaveDC(VALIDVDC(tdc));

    // two seperate stacks, the saves/restores should be in lock step, so
    // all indicies should match.
    assert(UserSaveDC == VerSaveDC);

    return UserSaveDC;
}

//**********************************************************************************
int
SelectClipRgn(TDC tdc, HRGN hrgn)
{
    int     irvalUser = SelectClipRgn(VALIDDC(tdc), hrgn);
    int     irvalVer = irvalUser;

    // if verification was turned off part way through, we still
    // want to select/deselect clip regions to release them to
    // delete the dc's, selecting a clip retion into a NULL dc is harmless
    irvalVer = SelectClipRgn(VALIDVDC(tdc), hrgn);

    if (irvalUser == ERROR || (DoVerify() &&  irvalVer == ERROR))
        return ERROR;
    return irvalUser;
}

//**********************************************************************************
HGDIOBJ SelectObject(TDC tdc, HGDIOBJ hgdiobj)
{
    HGDIOBJ hobjUser = SelectObject(VALIDDC(tdc), hgdiobj);
    HGDIOBJ hobjVer = hobjUser;

    if (DoVerify())
        hobjVer = SelectObject(VALIDVDC(tdc), hgdiobj);

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

    // if verification was turned off part way through, we still want to select.
    // selecting something into a NULL dc is harmless
    hbmpVer = (HBITMAP) SelectObject(VALIDVDC(tdc), VALIDVBMP(hgdiobj));

    if (hbmpUser)
    {
        tbmp = tdc->SelectBitmap(hgdiobj);
    }

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

    // if verification was turned off part way through, we still
    // want to delete the bitmap, deleting a NULL bitmap is harmless
    brvalVer = DeleteObject(VALIDVBMP(hObject));

    // if the user tries to delete the stock bitmap, don't do it...
    // but still return the gdi result as if they did.
    if (hObject && hObject != &g_tbmpStock)
    {
        hObject->SetBitmap(NULL);
        hObject->SetVBitmap(NULL);
        delete  hObject;
    }
    // user result if !DoVerify, user & ver if do verify
    return (brvalUser && (!DoVerify() || (DoVerify() && brvalVer)));

}

//**********************************************************************************
COLORREF SetBkColor(TDC tdc, COLORREF crColor)
{
    COLORREF crvalUser = SetBkColor(VALIDDC(tdc), crColor);
    COLORREF crvalVer = crvalUser;

    if (DoVerify())
    {
        crvalVer = SetBkColor(VALIDVDC(tdc), crColor);
        SurfaceVerify(tdc);
    }

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

    if (DoVerify())
        irvalVer = SetBkMode(VALIDVDC(tdc), iBkMode);

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

    if (DoVerify())
        brvalVer = SetBrushOrgEx(VALIDVDC(tdc), nXOrg, nYOrg, lppt);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
COLORREF SetPixel(TDC tdc, int X, int Y, COLORREF crColor)
{
    COLORREF crUser = SetPixel(VALIDDC(tdc), X, Y, crColor);
    COLORREF crVer = crUser;

    if (DoVerify())
        crVer = SetPixel(VALIDVDC(tdc), X, Y, crColor);
    PrinterMemCntrl(tdc, 1);
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

    if (DoVerify())
        crVer = SetTextColor(VALIDVDC(tdc), crColor);

    if (crUser == CLR_INVALID || crVer == CLR_INVALID)
        return CLR_INVALID;
    else
        return crUser;
}

//**********************************************************************************
BOOL StretchBlt(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc, int nXOriginSrc,
                int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop)
{
    NOPRINTERDC(tdcSrc, TEXT("StretchBlt"), TRUE)

    BOOL    brvalUser = StretchBlt(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = StretchBlt(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
// only used for bad param checking, no point in verifying.
BOOL StretchBlt(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC tdcSrc, int nXOriginSrc,
                int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop)
{
    BOOL    brvalUser = StretchBlt(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, tdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
        brvalVer = StretchBlt(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, tdcSrc, nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
// only used for bad param checking, no point in verifying.
BOOL StretchBlt(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC tdcSrc, int nXOriginSrc,
                int nYOriginSrc, int nWidthSrc, int nHeightSrc, DWORD dwRop)
{
    BOOL    brvalUser = StretchBlt(tdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
        brvalVer = StretchBlt(tdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(tdcSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, dwRop);
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
StretchDIBits(TDC tdc, int XDest, int YDest, int nDestWidth, int nDestHeight, int XSrc, int YSrc, int nSrcWidth, int nSrcHeight,
              CONST VOID * lpBits, CONST BITMAPINFO * lpBitsInfo, UINT iUsage, DWORD dwRop)
{
    int     irvalUser = StretchDIBits(VALIDDC(tdc), XDest, YDest, nDestWidth, nDestHeight, XSrc, YSrc, nSrcWidth, nSrcHeight, lpBits, lpBitsInfo, iUsage, dwRop);

    int     irvalVer = irvalUser;

    if (DoVerify())
    {
        irvalVer = StretchDIBits(VALIDVDC(tdc), XDest, YDest, nDestWidth, nDestHeight, XSrc, YSrc, nSrcWidth, nSrcHeight, lpBits, lpBitsInfo, iUsage, dwRop);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 2001);
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

    if (DoVerify())
    {
        irvalVer = SetDIBitsToDevice(VALIDVDC(tdc), XDest, YDest, dwWidth, dwHeight, XSrc, YSrc, uStartScan, cScanLines, lpvBits, lpbmi, fuColorUse);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
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

    if (DoVerify())
        hpalVer = SelectPalette(VALIDVDC(tdc), hPal, bForceBackground);

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

    if (DoVerify())
        uirvalVer = RealizePalette(VALIDVDC(tdc));

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

    if (DoVerify())
        uirvalVer = SetDIBColorTable(VALIDVDC(tdc), uStartIndex, cEntries, pColors);

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

    if (DoVerify())
    {
        brvalVer = FillRgn(VALIDVDC(tdc), hrgn, hbr);
        SurfaceVerify(tdc);
    }
    PrinterMemCntrl(tdc, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
int
SetROP2(TDC tdc, int fnDrawMode)
{
    int     irvalUser = SetROP2(VALIDDC(tdc), fnDrawMode);
    int     irvalVer = irvalUser;

    if (DoVerify())
    {
        irvalVer = SetROP2(VALIDVDC(tdc), fnDrawMode);
        SurfaceVerify(tdc);
    }

    if (irvalUser == 0 || irvalVer == 0)
        return 0;
    else
        return irvalUser;
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

    if (DoVerify())
        brvalVer = SetViewportOrgEx(VALIDVDC(tdc), X, Y, lpPoint);

    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TDC hSrc,
                      int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor)
{
    NOPRINTERDC(hSrc, TEXT("TransparentImage"), TRUE)

    BOOL    brvalUser = TransparentImage(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDDC(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = TransparentImage(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVDC(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL TransparentImage(TDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, TBITMAP hSrc,
                      int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, COLORREF TransparentColor)
{
    BOOL    brvalUser = TransparentImage(VALIDDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDBMP(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
    BOOL    brvalVer = brvalUser;

    if (DoVerify())
    {
        brvalVer = TransparentImage(VALIDVDC(tdcDest), nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, VALIDVBMP(hSrc), nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc, TransparentColor);
        SurfaceVerify(tdcDest);
    }
    PrinterMemCntrl(tdcDest, 1);
    return (brvalUser && brvalVer);
}

//**********************************************************************************
BOOL ExtEscape(TDC tdc, int iEscape, int cjInput, LPCSTR lpInData, int cjOutput, LPSTR lpOutData)
{
    return ExtEscape(VALIDDC(tdc), iEscape, cjInput, lpInData, cjOutput, lpOutData);
}

//**********************************************************************************
int    StartDoc(TDC hdc, CONST DOCINFO *lpdi)
{
    return gpfnStartDocInternal(VALIDDC(hdc), lpdi);
}

//**********************************************************************************
int    StartPage(TDC hDC)
{
    return gpfnStartPageInternal(VALIDDC(hDC));
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
int PrinterMemCntrl(HDC hdc, int decr)
{
    if (!ISPRINTERDC(hdc))
    {
        return -1;
    }

    g_iPrinterCntr -= decr;
    if (g_iPrinterCntr < 0)
    {
        gpfnEndPageInternal(hdc);
        gpfnStartPageInternal(hdc);
        g_iPrinterCntr = PCINIT;
    }
    return g_iPrinterCntr;
}

int PrinterMemCntrl(TDC tdc, int decr)
{
    return PrinterMemCntrl(VALIDDC(tdc), decr);
}

#else

/***********************************************************************************
***
*** Used when compiling with hdc's instead of tdc's.
***
************************************************************************************/
void OutputBitmap(HDC hdc)
{
    TCHAR   szFileName[MAX_PATH];

    if(g_OutputBitmaps)
    {
        wsprintf(szFileName, TEXT("%s%d-PFailure.bmp"), gtcBitmapDestination, g_nFailureNum);
        SaveSurfaceToBMP(hdc, szFileName);
        info(DETAIL, TEXT("Failure BMP id is %d"), g_nFailureNum);
        g_nFailureNum++;
    }
    return;
}

//**********************************************************************************
TESTPROCAPI DriverVerify_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY)
{
    return TPR_PASS;
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


#endif
