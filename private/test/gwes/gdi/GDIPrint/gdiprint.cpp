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

#include "global.h"

#pragma warning(disable:4068)
#pragma prefast(push)
#pragma prefast(disable:326, "Test is testing itself by comparing two constants.")

TESTPROCAPI
BitBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("BitBlt_T"));

    TCHAR *tszTestBitmap[] = {
                                            TEXT("JADE1"),
                                            TEXT("JADE4"),
                                            TEXT("JADE8"),
                                            TEXT("JADE24")
                                            };

    TCHAR *tszTestDescription[] = {
                                                        TEXT("This test verifies the functionality of BitBlt.  This image should be a 1bpp picture of a puppy. As 1bpp only has black and white, the expected image is mostly black with a few white highlites from the grass and outlines of the puppy."),
                                                        TEXT("This test verifies the functionality of BitBlt.  This image should be a 4bpp picture of a puppy, it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of BitBlt.  This image should be a 8bpp picture of a puppy, it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of BitBlt.  This image should be a 24bpp picture of a puppy, it should contain color if color was selected as a print option.")
                                                        };
    DCDATA dcd;

    if(countof(tszTestBitmap) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {

        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, tszTestBitmap[i]));
            if(hbmp)
            {
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(gPrintHandler.TestStart(bmp.bmWidth, bmp.bmHeight))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat = NULL;
                        HDC hdcVidCompat = NULL;
                        HBITMAP hbmpStock;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat = CreateCompatibleDC(dcd.hdcVideoMemory));

                        // select the bitmap, blit from the printer dc with the bitmap to the printer, and then deselect the bitmap.
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmp));
                        CheckNotRESULT(FALSE, BitBlt(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth, bmp.bmHeight, hdcPrintCompat, 0, 0, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpStock));

                        // select the bitmap into the video memory dc, blit it to the video memory surface, then deselect the bitmap
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcVidCompat, hbmp));
                        CheckNotRESULT(FALSE, BitBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat, 0, 0, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmp));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                    // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap %s"), tszTestBitmap[i]);
        }
    }

    return getCode();
}

void
DrawDiagonals(HDC hdc,int w, int h)
{
    bool bResult = true;

    // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
    double fAddx = ((double) w)/20.0, fAddy = ((double) h)/20.0;
    double fw =0, fh = 0;

    PatBlt(hdc, 0, 0, w, h, WHITENESS);

    while((int) fw <= w && (int) fh <= h)
    {
        PatBlt(hdc, (int) fw, (int) fh, (int) fAddx, (int) fAddy, BLACKNESS);
        fw += fAddx;
        fh += fAddy;
    }

    // draw another diagonal covering the other corners, but make it smaller boxes so they are visually different.
    fAddx = ((double) w)/40.0;
    fAddy = ((double) h)/40.0;
    fw = 0;
    fh = ((double) h) - fAddy;
    while((int) fw <= w && (int) fh >= 0)
    {
        PatBlt(hdc, (int) fw, (int) fh, (int) fAddx, (int) fAddy, BLACKNESS);
        fw += fAddx;
        fh -= fAddy;
    }
}

TESTPROCAPI
MaskBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("MaskBlt_T"));

    DWORD dwRops[] = {
                            MAKEROP4(SRCCOPY, SRCCOPY),
                            MAKEROP4(SRCCOPY, PATCOPY),
                            MAKEROP4(PATCOPY, SRCCOPY)
                        };
    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of MaskBlt.  The bitmap on the left should match the bitmap on the right, have no artifacts in the inerior, and be in color if color was selected as a print option."),
                                                            TEXT("This test verifies the functionality of MaskBlt.  The bitmap on the left should match the bitmap on the right, and have a set of diagonal boxes from each of the corners, the boxes should be in blue if color was selected."),
                                                            TEXT("This test verifies the functionality of MaskBlt.  The bitmap on the left should match the bitmap on the right, and the box should be mostly blue with portions of the puppy picture in the diagonal boxes across the square.")
                                                        };
    DCDATA dcd;

    if(countof(dwRops) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            // multiple bitmap depths aren't needed for this case, a single bit depth but multiple ROP4's are used.
            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
            if(hbmp)
            {
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(gPrintHandler.TestStart(bmp.bmWidth, bmp.bmHeight))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat = NULL;
                        HDC hdcVidCompat = NULL;
                        HBITMAP hbmpPrintMask = NULL, hbmpVidMask = NULL;
                        HBITMAP hbmpStock = NULL, hbmpStock2 = NULL;
                        HBRUSH hbrSolid = NULL, hbrStock = NULL;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat = CreateCompatibleDC(dcd.hdcVideoMemory));


                        // Create the masks
                        CheckNotRESULT(NULL, hbmpVidMask = CreateBitmap(bmp.bmWidth, bmp.bmHeight, 1, 1, NULL));
                        CheckNotRESULT(NULL, hbmpPrintMask = CreateBitmap(bmp.bmWidth, bmp.bmHeight, 1, 1, NULL));

                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmpPrintMask));
                        CheckNotRESULT(NULL, hbmpStock2 = (HBITMAP) SelectObject(hdcVidCompat, hbmpVidMask));

                        // setup the two masks
                        DrawDiagonals(hdcPrintCompat, bmp.bmWidth, bmp.bmHeight);
                        DrawDiagonals(hdcVidCompat, bmp.bmWidth, bmp.bmHeight);

                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpStock));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmpStock2));

                        // create a blue brush, to use when using pattern rops.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(RGB(0x00, 0x00, 0xFF)));

                        // select the bitmap, blit from the printer dc with the bitmap to the printer, and then deselect the bitmap.
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmp));
                        CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(dcd.hdcPrint, hbrSolid));

                        CheckNotRESULT(FALSE, MaskBlt(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth, bmp.bmHeight, hdcPrintCompat, 0, 0, hbmpPrintMask, 0, 0, dwRops[i]));

                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpStock));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock));

                        // select the bitmap into the video memory dc, blit it to the video memory surface, then deselect the bitmap
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcVidCompat, hbmp));
                        CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(dcd.hdcVideoMemory, hbrSolid));

                        CheckNotRESULT(FALSE, MaskBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat, 0, 0, hbmpVidMask, 0, 0, dwRops[i]));

                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmp));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpVidMask));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpPrintMask));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                     // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap: JADE24"));
        }
    }

    return getCode();
}

TESTPROCAPI
StretchBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("StretchBlt_T"));

    TCHAR *tszTestBitmap[] = {
                                            // first half of the array are shinks
                                            TEXT("JADE1"),
                                            TEXT("JADE4"),
                                            TEXT("JADE8"),
                                            TEXT("JADE24"),

                                            // second half are for stretches
                                            TEXT("JADE1"),
                                            TEXT("JADE4"),
                                            TEXT("JADE8"),
                                            TEXT("JADE24")
                                            };

    TCHAR *tszTestDescription[] = {
                                                        // first half of the array are for shrinks
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 1bpp picture of a puppy, and it should be 1/2 the size of the image on the right. As 1bpp only has black and white, the expected image is mostly black with a few white highlites from the grass and outlines of the puppy."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 4bpp picture of a puppy, it should be 1/2 the size of the image on the right, and it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 8bpp picture of a puppy, it should be 1/2 the size of the image on the right, and it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 24bpp picture of a puppy, it should be 1/2 the size of the image onthe right, and it should contain color if color was selected as a print option."),

                                                        // second half are for stretches
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 1bpp picture of a puppy, and it should be 2x the size of the image on the right. As 1bpp only has black and white, the expected image is mostly black with a few white highlites from the grass and outlines of the puppy."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 4bpp picture of a puppy, it should be 2x the size of the image on the right, and it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 8bpp picture of a puppy, it should be 2x the size of the image on the right, and it should contain color if color was selected as a print option."),
                                                        TEXT("This test verifies the functionality of StretchBlt.  This image should be a 24bpp picture of a puppy, it should be 2x the size of the image onthe right, and it should contain color if color was selected as a print option.")
                                                        };
    DCDATA dcd;
    int nTestCount = countof(tszTestDescription);

    if(countof(tszTestBitmap) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < nTestCount; i++)
    {

        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, tszTestBitmap[i]));
            if(hbmp)
            {
                int nModifiedBitmapWidth = 0, nModifiedBitmapHeight = 0, nBitmapEffectiveHeight;

                // retrieve the bitmap information, and calculate the height the test will need.
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(i < nTestCount /2)
                {
                     nModifiedBitmapWidth = bmp.bmWidth /2;
                     nModifiedBitmapHeight = bmp.bmHeight /2;
                     nBitmapEffectiveHeight = bmp.bmHeight;
                }
                else
                {
                    nModifiedBitmapWidth = bmp.bmWidth * 2;
                    nModifiedBitmapHeight = bmp.bmHeight * 2;
                    nBitmapEffectiveHeight = nModifiedBitmapHeight;
                }

                // the effective height is either the bitmap height or the modified height, whichever is larger,
                // this means that when stretching the height will be the stretched height, when shrinking the height
                // will be the reference bitmap height.
                if(gPrintHandler.TestStart(bmp.bmWidth, nBitmapEffectiveHeight))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat = NULL;
                        HDC hdcVidCompat = NULL;
                        HBITMAP hbmpStock;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat = CreateCompatibleDC(dcd.hdcVideoMemory));

                        // select the bitmap, blit from the printer dc with the bitmap to the printer, and then deselect the bitmap.
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmp));
                        CheckNotRESULT(FALSE, StretchBlt(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, nModifiedBitmapWidth, nModifiedBitmapHeight, hdcPrintCompat, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpStock));

                        // select the bitmap into the video memory dc, blit it to the video memory surface, then deselect the bitmap
                        // the base bitmap in this test is a constent size
                        CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcVidCompat, hbmp));
                        CheckNotRESULT(FALSE, BitBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat, 0, 0, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmp));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                    // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap %s"), tszTestBitmap[i]);
        }
    }

    return getCode();
}

TESTPROCAPI
PatBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("PatBlt_T"));

    DWORD dwRops[] = {
                            BLACKNESS,
                            WHITENESS,
                            PATCOPY
                        };
    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of PatBlt. The left should match the right.  This should be a black box."),
                                                            TEXT("This test verifies the functionality of PatBlt. The left should match the right and this should be a white box (empty)."),
                                                            TEXT("This test verifies the functionality of PatBlt. The left should match the right, and it should be four pictures of the puppy layed out in a tiled wallpaper fashion."),
                                                        };
    DCDATA dcd;

    if(countof(dwRops) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            // multiple bitmap depths aren't needed for this case, a single bit depth but multiple ROP4's are used.
            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
            if(hbmp)
            {
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(gPrintHandler.TestStart(bmp.bmWidth * 2, bmp.bmHeight * 2))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HBRUSH hbrPattern = NULL, hbrStock = NULL;


                        // create a blue brush, to use when using pattern rops.
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));

                        // select the bitmap, blit from the printer dc with the bitmap to the printer, and then deselect the bitmap.
                        // set the brush origin before selecting the brush, so it takes effect.
                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));
                        CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(dcd.hdcPrint, hbrPattern));
                        CheckNotRESULT(FALSE, PatBlt(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth * 2, bmp.bmHeight * 2, dwRops[i]));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock));

                        // select the bitmap into the video memory dc, blit it to the video memory surface, then deselect the bitmap
                        CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(dcd.hdcVideoMemory, hbrPattern));
                        CheckNotRESULT(FALSE, PatBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth * 2, bmp.bmHeight * 2, dwRops[i]));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap: JADE24"));
        }
    }

    return getCode();
}

void
DrawBoxes(HDC hdc, int w, int h)
{
    RECT rc;
    HBRUSH hbr;

    // clear the surface to white.
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, w, h, WHITENESS));

    // first square, black.
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, w/3, h/3, BLACKNESS));

    // second square, red
    rc.left = 2*w/3;
    rc.top = 0;
    rc.right = w;
    rc.bottom = h/3;
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(0xFF, 0x0, 0x0)));
    CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
    CheckNotRESULT(FALSE, DeleteObject(hbr));

    // third square, green
    rc.left = 0;
    rc.top = 2*h/3;
    rc.right = w/3;
    rc.bottom = h;
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(0x00, 0xFF, 0x0)));
    CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
    CheckNotRESULT(FALSE, DeleteObject(hbr));

    // Fourth square, blue
    rc.left = 2*w/3;
    rc.top = 2*h/3;
    rc.right = w;
    rc.bottom = h;
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(0x00, 0x00, 0xFF)));
    CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
    CheckNotRESULT(FALSE, DeleteObject(hbr));
}

TESTPROCAPI
TransparentBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("TransparentBlt_T"));

    DWORD dwTransparentColors[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0x00, 0x00, 0xFF)
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of TransparentBlt.  The left image should match the right, and there should be four squares of color.  If color printing was enabled, going clockwise the colors should be black, red, blue, and green."),
                                                            TEXT("This test verifies the functionality of TransparentBlt.  The left image should match the right, and there should be three squares of color with the upper left square (black) missing."),
                                                            TEXT("This test verifies the functionality of TransparentBlt.  The left image should match the right, and there should be three squares of color, with the upper right square (red) missing."),
                                                            TEXT("This test verifies the functionality of TransparentBlt.  The left image should match the right, and there should be three squares of color, with the upper lower left (green) missing."),
                                                            TEXT("This test verifies the functionality of TransparentBlt.  The left image should match the right, and there should be three squares of color, with the lower right square (blue) missing.")
                                                        };
    DCDATA dcd;
    int nTestCount = countof(tszTestDescription);

    if(countof(dwTransparentColors) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < nTestCount; i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            // retrieve the base DC data.  This doesn't get us the handles to the surfaces, but it does get the size.
            if(gPrintHandler.GetDCData(&dcd))
            {
                // take the minimum, half of the width we're given, or 1/5th of the height (so all 5 tests will fit on a single page)
                // subtract nTestCount*12 to compensate for the height used by the test descriptions.
                int nSquare = min(dcd.nWidth/2, dcd.nHeight/nTestCount - nTestCount * 24);
                // This test scales itself to the available print size
                if(gPrintHandler.TestStart(nSquare, nSquare))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat = NULL;
                        HDC hdcVidCompat = NULL;
                        HBITMAP hbmpVid = NULL;
                        HBITMAP hbmpPrint = NULL;
                        HBITMAP hbmpVidStock;
                        HBITMAP hbmpPrintStock;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat = CreateCompatibleDC(dcd.hdcVideoMemory));

                        hbmpPrint = CreateCompatibleBitmap(dcd.hdcPrint, nSquare, nSquare);
                        hbmpVid = CreateCompatibleBitmap(dcd.hdcVideoMemory, nSquare, nSquare);
                        CheckNotRESULT(NULL, hbmpPrintStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmpPrint));
                        CheckNotRESULT(NULL, hbmpVidStock = (HBITMAP) SelectObject(hdcVidCompat, hbmpVid));

                        DrawBoxes(hdcPrintCompat, nSquare, nSquare);
                        DrawBoxes(hdcVidCompat, nSquare, nSquare);

                        CheckNotRESULT(FALSE, TransparentBlt(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, nSquare, nSquare, hdcPrintCompat, 0, 0, nSquare, nSquare, dwTransparentColors[i]));
                        CheckNotRESULT(FALSE, TransparentBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, nSquare, nSquare, hdcVidCompat, 0, 0, nSquare, nSquare, dwTransparentColors[i]));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpPrintStock));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmpVidStock));
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpPrint));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpVid));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                     // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(FAIL, TEXT("Failed to retrieve initial DC data"));
        }
    }

    return getCode();
}

TESTPROCAPI
AlphaBlend_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("AlphaBlend_T"));

    int nAlphas[] = {
                                255,
                                192,
                                128,
                                64,
                                0
                            };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of AlphaBlend.  The left image should match the right, and it should be an opaque picture of a puppy."),
                                                            TEXT("This test verifies the functionality of AlphaBlend.  The left image should match the right, and it should be a 75% opaque picture of a puppy."),
                                                            TEXT("This test verifies the functionality of AlphaBlend.  The left image should match the right, and it should be a 50% opaque picture of a puppy."),
                                                            TEXT("This test verifies the functionality of AlphaBlend.  The left image should match the right, and it should be a 25% opaque picture of a puppy."),
                                                            TEXT("This test verifies the functionality of AlphaBlend.  The left image should match the right, and it should be an empty area from a 0% opaque (100% transparent) picture of a puppy.")
                                                         };
    DCDATA dcd;

    HINSTANCE hInstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));;

    if(countof(nAlphas) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    if(hInstCoreDLL)
    {
        PFNALPHABLEND pfnAlphaBlend = (PFNALPHABLEND) GetProcAddress(hInstCoreDLL, TEXT("AlphaBlend"));
        if(pfnAlphaBlend)
        {

            for(int i = 0; i < countof(tszTestDescription); i++)
            {
                // the framework handles printing the tests description before the case, on the primary, and to the debug stream
                if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
                {
                    HBITMAP hbmp = NULL;
                    BLENDFUNCTION bf;
                    BITMAP bmp;

                    memset(&bmp, 0, sizeof(bmp));

                    bf.BlendOp = AC_SRC_OVER;
                    bf.BlendFlags = 0;
                    bf.SourceConstantAlpha = nAlphas[i];
                    bf.AlphaFormat = 0;

                    // multiple bitmap depths aren't needed for this case, a single bit depth but multiple ROP4's are used.
                    CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                    if(hbmp)
                    {
                        GetObject(hbmp, sizeof(bmp), &bmp);

                        if(gPrintHandler.TestStart(bmp.bmWidth, bmp.bmHeight))
                        {
                            // get dc data
                            if(gPrintHandler.GetDCData(&dcd))
                            {
                                HDC hdcPrintCompat = NULL;
                                HDC hdcVidCompat = NULL;
                                HBITMAP hbmpStock = NULL, hbmpStock2 = NULL;

                                // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                                // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                                CheckNotRESULT(NULL, hdcPrintCompat = CreateCompatibleDC(dcd.hdcPrint));
                                CheckNotRESULT(NULL, hdcVidCompat = CreateCompatibleDC(dcd.hdcVideoMemory));

                                // select the bitmap, blit from the printer dc with the bitmap to the printer, and then deselect the bitmap.
                                CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcPrintCompat, hbmp));
                                CheckNotRESULT(FALSE, pfnAlphaBlend(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth, bmp.bmHeight, hdcPrintCompat, 0, 0, bmp.bmWidth, bmp.bmHeight, bf));
                                CheckNotRESULT(NULL, SelectObject(hdcPrintCompat, hbmpStock));

                                // select the bitmap into the video memory dc, blit it to the video memory surface, then deselect the bitmap
                                CheckNotRESULT(NULL, hbmpStock = (HBITMAP) SelectObject(hdcVidCompat, hbmp));
                                CheckNotRESULT(FALSE, pfnAlphaBlend(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat, 0, 0, bmp.bmWidth, bmp.bmHeight, bf));
                                CheckNotRESULT(NULL, SelectObject(hdcVidCompat, hbmp));

                                // cleanup
                                CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat));
                                CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat));
                            }
                            else info(ABORT, TEXT("Failed to retrieve the DC data."));

                             // test finished
                            gPrintHandler.TestFinished();
                        }
                        else info(ABORT, TEXT("Failed to start the test."));

                        // release our allocated test bitmap
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));
                    }
                    else info(FAIL, TEXT("Failed to load the test bitmap: JADE24"));
                }
            }
        }
        else info(SKIP, TEXT("Alphablend unavailable"));
        FreeLibrary(hInstCoreDLL);
    }
    else info(ABORT, TEXT("Failed to retrieve the Coredll instance to retrieve the Alphablend function pointer."));
    return getCode();
}

TESTPROCAPI
StretchDIBits_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("StretchDIBits_T"));

    int nIdentifiers[] = {
                                    RGB16,
                                    RGB4444,
                                    RGB565,
                                    RGB555,
                                    RGB1555,
                                    BGR4444,
                                    BGR565,
                                    BGR555,
                                    BGR1555,
                                    RGB24,
                                    RGB32,
                                    RGB8888,
                                    RGB888,
                                    BGR8888,
                                    BGR888,
                                    PAL1,
                                    PAL2,
                                    PAL4,
                                    PAL8,
                                    RGB1,
                                    RGB2,
                                    RGB4,
                                    RGB8
                                    };

    TCHAR *tszTestDescription[] = {
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB16
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp RGB4444 DIB to the printer, the left image should match the right."), // RGB4444
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp RGB565 DIB to the printer, the left image should match the right."), // RGB565
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp RGB555 DIB to the printer, the left image should match the right."), // RGB555
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp RGB1555 DIB to the printer, the left image should match the right."), // RGB1555
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp BGR4444 DIB to the printer, the left image should match the right."), // BGR4444
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp BGR565 DIB to the printer, the left image should match the right."), // BGR565
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp BGR555 DIB to the printer, the left image should match the right."), // BGR555
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 16bpp BGR1555 DIB to the printer, the left image should match the right."), // BGR1555
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 24bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB24
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 32bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB32
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 32bpp RGB8888 DIB to the printer, the left image should match the right."), // RGB8888
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 32bpp RGB888 DIB to the printer, the left image should match the right."), // RGB888
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 32bpp BGR8888 DIB to the printer, the left image should match the right."), // BGR8888
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 32bpp BGR888 DIB to the printer, the left image should match the right."), // BGR888
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 1bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right.  The image should be mostly black with a few white specs."), // PAL1
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 2bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right.  Due to the limited colors in a 2bpp surface, this only has a bare resemblence to a puppy."), // PAL2
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 4bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right."), // PAL4
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 8bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right."), // PAL8
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 1bpp user paletted DIB to the printer, the left image should match the right.  The images should be mostly black with a few white specs."), // RGB1
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 2bpp user paletted DIB to the printer, the left image should match the right.  This should be a black box."), // RGB2
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 4bpp user paletted DIB to the printer, the left image should match the right.  This should be a grayscale picture of the puppy."), // RGB4
                                                        TEXT("This test verifies the functionality of StretchDIBits.  This is testing a 8bpp user paletted DIB to the printer, the left image should match the right.  This should be a grayscale picture of the puppy.")  // RGB8
                                                        };
    DCDATA dcd;

    if(countof(nIdentifiers) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {

        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
            if(hbmp)
            {
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(gPrintHandler.TestStart(bmp.bmWidth, bmp.bmHeight))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat1 = NULL, hdcPrintCompat2 = NULL;
                        HDC hdcVidCompat1 = NULL, hdcVidCompat2 = NULL;

                        MYBITMAPINFO bmi;
                        VOID *pBits;
                        HBITMAP hbmpDIB;
                        HBITMAP hbmpStock1, hbmpStock2;
                        int nUsage;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat1 = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcPrintCompat2 = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat1 = CreateCompatibleDC(dcd.hdcVideoMemory));
                        CheckNotRESULT(NULL, hdcVidCompat2 = CreateCompatibleDC(dcd.hdcVideoMemory));

                        CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(dcd.hdcPrint, bmp.bmWidth, bmp.bmHeight, nIdentifiers[i], &pBits, &bmi, &nUsage));

                        // select the loaded bitmap and the dib
                        CheckNotRESULT(NULL, hbmpStock1 = (HBITMAP) SelectObject(hdcPrintCompat1, hbmp));
                        CheckNotRESULT(NULL, hbmpStock2 = (HBITMAP) SelectObject(hdcPrintCompat2, hbmpDIB));
                        // copy from the loaded bitmap to the dib
                        CheckNotRESULT(FALSE, BitBlt(hdcPrintCompat2, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcPrintCompat1, 0, 0, SRCCOPY));
                        // copy from the DIB to the printer/primary
                        StretchDIBits(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth, bmp.bmHeight, 0, 0, bmp.bmWidth, bmp.bmHeight, pBits, (BITMAPINFO *) &bmi, nUsage, SRCCOPY);
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat1, hbmpStock1));
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat2, hbmpStock2));

                        // select the loaded bitmap and the dib
                        CheckNotRESULT(NULL, hbmpStock1 = (HBITMAP) SelectObject(hdcVidCompat1, hbmp));
                        CheckNotRESULT(NULL, hbmpStock2 = (HBITMAP) SelectObject(hdcVidCompat2, hbmpDIB));
                        // blt from the loaded bitmap to the dib, and then from the dib to the primary
                        CheckNotRESULT(FALSE, BitBlt(hdcVidCompat2, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcVidCompat1, 0, 0, SRCCOPY));
                        CheckNotRESULT(FALSE, BitBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat2, 0, 0, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat1, hbmpStock1));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat2, hbmpStock2));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat1));
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat2));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat1));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat2));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                    // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap."));
        }
    }

    return getCode();
}

TESTPROCAPI
SetDIBitsToDevice_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("SetDIBitsToDevice_T"));
    int nIdentifiers[] = {
                                    RGB16,
                                    RGB4444,
                                    RGB565,
                                    RGB555,
                                    RGB1555,
                                    BGR4444,
                                    BGR565,
                                    BGR555,
                                    BGR1555,
                                    RGB24,
                                    RGB32,
                                    RGB8888,
                                    RGB888,
                                    BGR8888,
                                    BGR888,
                                    PAL1,
                                    PAL2,
                                    PAL4,
                                    PAL8,
                                    RGB1,
                                    RGB2,
                                    RGB4,
                                    RGB8
                                    };

    TCHAR *tszTestDescription[] = {
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB16
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp RGB4444 DIB to the printer, the left image should match the right."), // RGB4444
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp RGB565 DIB to the printer, the left image should match the right."), // RGB565
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp RGB555 DIB to the printer, the left image should match the right."), // RGB555
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp RGB1555 DIB to the printer, the left image should match the right."), // RGB1555
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp BGR4444 DIB to the printer, the left image should match the right."), // BGR4444
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp BGR565 DIB to the printer, the left image should match the right."), // BGR565
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp BGR555 DIB to the printer, the left image should match the right."), // BGR555
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 16bpp BGR1555 DIB to the printer, the left image should match the right."), // BGR1555
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 24bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB24
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 32bpp BI_RGB DIB to the printer, the left image should match the right."), // RGB32
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 32bpp RGB8888 DIB to the printer, the left image should match the right."), // RGB8888
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 32bpp RGB888 DIB to the printer, the left image should match the right."), // RGB888
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 32bpp BGR8888 DIB to the printer, the left image should match the right."), // BGR8888
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 32bpp BGR888 DIB to the printer, the left image should match the right."), // BGR888
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 1bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right.  The image should be mostly black with a few white specs."), // PAL1
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 2bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right.  Due to the limited colors in a 2bpp surface, this only has a bare resemblence to a puppy."), // PAL2
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 4bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right."), // PAL4
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 8bpp DIB_PAL_COLORS DIB to the printer, the left image should match the right."), // PAL8
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 1bpp user paletted DIB to the printer, the left image should match the right.  The images should be mostly black with a few white specs."), // RGB1
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 2bpp user paletted DIB to the printer, the left image should match the right.  This should be a black box."), // RGB2
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 4bpp user paletted DIB to the printer, the left image should match the right.  This should be a grayscale picture of the puppy."), // RGB4
                                                        TEXT("This test verifies the functionality of SetDIBitsToDevice.  This is testing a 8bpp user paletted DIB to the printer, the left image should match the right.  This should be a grayscale picture of the puppy.")  // RGB8
                                                        };
    DCDATA dcd;

    if(countof(nIdentifiers) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {

        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            HBITMAP hbmp = NULL;
            BITMAP bmp;

            memset(&bmp, 0, sizeof(bmp));

            CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
            if(hbmp)
            {
                GetObject(hbmp, sizeof(bmp), &bmp);

                if(gPrintHandler.TestStart(bmp.bmWidth, bmp.bmHeight))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HDC hdcPrintCompat1 = NULL, hdcPrintCompat2 = NULL;
                        HDC hdcVidCompat1 = NULL, hdcVidCompat2 = NULL;

                        MYBITMAPINFO bmi;
                        VOID *pBits;
                        HBITMAP hbmpDIB;
                        HBITMAP hbmpStock1, hbmpStock2;
                        int nUsage;

                        // two compatible dc's to select the bitmap into, one compatible to a printer dc, one compatible to the primary.
                        // as a bitmap cannot be selected into two dc's at the same time, we select into one and do the operations, then switch to the other.
                        CheckNotRESULT(NULL, hdcPrintCompat1 = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcPrintCompat2 = CreateCompatibleDC(dcd.hdcPrint));
                        CheckNotRESULT(NULL, hdcVidCompat1 = CreateCompatibleDC(dcd.hdcVideoMemory));
                        CheckNotRESULT(NULL, hdcVidCompat2 = CreateCompatibleDC(dcd.hdcVideoMemory));

                        CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(dcd.hdcPrint, bmp.bmWidth, bmp.bmHeight, nIdentifiers[i], &pBits, &bmi, &nUsage));

                        // select the loaded bitmap and the dib
                        CheckNotRESULT(NULL, hbmpStock1 = (HBITMAP) SelectObject(hdcPrintCompat1, hbmp));
                        CheckNotRESULT(NULL, hbmpStock2 = (HBITMAP) SelectObject(hdcPrintCompat2, hbmpDIB));
                        // copy from the loaded bitmap to the dib
                        CheckNotRESULT(FALSE, BitBlt(hdcPrintCompat2, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcPrintCompat1, 0, 0, SRCCOPY));
                        // copy from the DIB to the printer/primary
                        SetDIBitsToDevice(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, bmp.bmWidth, bmp.bmHeight, 0, 0, 0, bmp.bmHeight, pBits, (BITMAPINFO *) &bmi, nUsage);
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat1, hbmpStock1));
                        CheckNotRESULT(NULL, SelectObject(hdcPrintCompat2, hbmpStock2));

                        // select the loaded bitmap and the dib
                        CheckNotRESULT(NULL, hbmpStock1 = (HBITMAP) SelectObject(hdcVidCompat1, hbmp));
                        CheckNotRESULT(NULL, hbmpStock2 = (HBITMAP) SelectObject(hdcVidCompat2, hbmpDIB));
                        // blt from the loaded bitmap to the dib, and then from the dib to the primary
                        CheckNotRESULT(FALSE, BitBlt(hdcVidCompat2, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcVidCompat1, 0, 0, SRCCOPY));
                        CheckNotRESULT(FALSE, BitBlt(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, bmp.bmWidth, bmp.bmHeight, hdcVidCompat2, 0, 0, SRCCOPY));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat1, hbmpStock1));
                        CheckNotRESULT(NULL, SelectObject(hdcVidCompat2, hbmpStock2));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat1));
                        CheckNotRESULT(FALSE, DeleteDC(hdcPrintCompat2));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat1));
                        CheckNotRESULT(FALSE, DeleteDC(hdcVidCompat2));
                        CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));

                    // test finished
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));

                // release our allocated test bitmap
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(FAIL, TEXT("Failed to load the test bitmap."));
        }
    }


    return getCode();
}

TESTPROCAPI
GradientFill_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("GradientFill_T"));

    COLORREF crGradients1[] =
                                        {
// horizontal cases
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0xFF, 0x00, 0x00),

// vertical cases
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0xFF, 0x00, 0x00),
                                        };
    COLORREF crGradients2[] =
                                        {
// horizontal cases
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0xFF, 0xFF, 0xFF),
                                            RGB(0x00, 0xFF, 0x00),

// vertical cases
                                            RGB(0x00, 0x00, 0x00),
                                            RGB(0xFF, 0xFF, 0xFF),
                                            RGB(0x00, 0xFF, 0x00),
                                        };
    TCHAR *tszTestDescription[] = {
// horizontal cases
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a solid black box"),
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a black to white gradient changing from left to right."),
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a red to green gradient changing from left to right."),

// vertical cases
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a solid black box"),
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a black to white gradient changing from top to bottom."),
                                                            TEXT("This test verifies the functionality of GradientFill.  The left image should match the right.  This should be a red to green gradient changing from top to bottom."),
                                                         };
    DCDATA dcd;
    int nMode;

    int nTestCount = countof(crGradients1);

    if(!((countof(crGradients1) == countof(crGradients2)) && (countof(crGradients2) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    HINSTANCE hInstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));;
    if(hInstCoreDLL)
    {
        PFNGRADIENTFILL pfnGradientFill = (PFNGRADIENTFILL) GetProcAddress(hInstCoreDLL, TEXT("GradientFill"));
        if(pfnGradientFill)
        {

            for(int i = 0; i < nTestCount; i++)
            {
                // the framework handles printing the tests description before the case, on the primary, and to the debug stream
                if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
                {
                    // retrieve the base DC data.  This doesn't get us the handles to the surfaces, but it does get the size.
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        // take the minimum, half of the width we're given, or 1/5th of the height (so all 5 tests will fit on a single page)
                        // subtract nTestCount*12 to compensate for the height used by the test descriptions.
                        int nSquare = min(dcd.nWidth/2, dcd.nHeight/nTestCount - nTestCount * 12);
                        // This test scales itself to the available print size
                        if(gPrintHandler.TestStart(nSquare, nSquare))
                        {
                            // get dc data
                            if(gPrintHandler.GetDCData(&dcd))
                            {
                                TRIVERTEX        vert[2];
                                GRADIENT_RECT    gRect[1];

                                // first half of the tests are horizontal, second half vertical
                                if(i < nTestCount / 2)
                                    nMode = GRADIENT_FILL_RECT_H;
                                else nMode = GRADIENT_FILL_RECT_V;

                                gRect[0].UpperLeft = 0;
                                gRect[0].LowerRight = 1;
                                vert [0] .Red    = GetRValue(crGradients1[i]) << 8;
                                vert [0] .Green  = GetGValue(crGradients1[i]) << 8;
                                vert [0] .Blue   = GetBValue(crGradients1[i]) << 8;
                                vert [0] .Alpha  = 0x0000;
                                vert [1] .Red  = GetRValue(crGradients2[i]) << 8;
                                vert [1] .Green   = GetGValue(crGradients2[i]) << 8;
                                vert [1] .Blue  = GetBValue(crGradients2[i]) << 8;
                                vert [1] .Alpha    = 0x0000;

                                vert [0] .x      = dcd.nCurrentX;
                                vert [0] .y      = dcd.nCurrentY;
                                vert [1] .x      = dcd.nCurrentX + nSquare;
                                vert [1] .y      = dcd.nCurrentY + nSquare;

                                CheckNotRESULT(FALSE, pfnGradientFill(dcd.hdcPrint, vert, 2, gRect, 1, nMode));

                                vert [0] .x      = dcd.nVidCurrentX;
                                vert [0] .y      = dcd.nVidCurrentY;
                                vert [1] .x      = dcd.nVidCurrentX + nSquare;
                                vert [1] .y      = dcd.nVidCurrentY + nSquare;

                                CheckNotRESULT(FALSE, pfnGradientFill(dcd.hdcVideoMemory, vert, 2, gRect, 1, nMode));

                            }
                            else info(ABORT, TEXT("Failed to retrieve the DC data."));

                             // test finished
                            gPrintHandler.TestFinished();
                        }
                        else info(ABORT, TEXT("Failed to start the test."));
                    }
                    else info(FAIL, TEXT("Failed to retrieve initial DC data"));
                }
            }
        }
        else info(SKIP, TEXT("GradientFill unavailable."));
        FreeLibrary(hInstCoreDLL);
    }
    else info(ABORT, TEXT("Failed to retrieve the Coredll instance to retrieve the Alphablend function pointer."));

    return getCode();
}

TESTPROCAPI
FillRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("FillRect_T"));

    HBRUSH hbrSolid, hbrPattern;
    HBRUSH *hbr[] = {
                                &hbrSolid,
                                &hbrPattern
                              };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of FillRect.  The image to the left should match the image to the right, and be a red box if color printing was enabled."),
                                                            TEXT("This test verifies the functionality of FillRect.  The image to the left should match the image to the right, and be a tiled picture of a puppy."),
                                                        };
    DCDATA dcd;

    if(countof(hbr) != countof(tszTestDescription))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare, nSquare))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HBITMAP hbmp;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare, dcd.nCurrentY + nSquare};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare, dcd.nVidCurrentX + nSquare};

                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(RGB(0xFF, 0x00, 0x00)));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, *hbr[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, *hbr[i]));

                        CheckNotRESULT(FALSE, DeleteObject(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }


    return getCode();
}

TESTPROCAPI
PolyLine_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("PolyLine_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_BLACK,
                            R2_COPYPEN,
                            R2_WHITE,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0x00, 0x00, 0x00),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0x00, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of PolyLine.  The image on the left should match the image on the right, the image should be a triangle with a thin black pen and an empty interior."),
                                                            TEXT("This test verifies the functionality of PolyLine.  The image on the left should match the image on the right, the image should be a triangle with a thin red dashed pen and an empty interior."),
                                                            TEXT("This test verifies the functionality of PolyLine.  The image on the left should match the image on the right, the image should be a triangle with a wide pen, it should be white on a black background."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops)) &&
            (countof(dwRops) == countof(dwPenWidth))&&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor)) &&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));
                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the lines to the printer
                        pt[0].x = dcd.nCurrentX + nSquare/2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nCurrentY + + dwPenWidth[i]/2;
                        pt[1].x = dcd.nCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, Polyline(dcd.hdcPrint, pt, ncPoints));

                        // and now to the verification surface
                        pt[0].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        pt[1].x = dcd.nVidCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nVidCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, Polyline(dcd.hdcVideoMemory, pt, ncPoints));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
Polygon_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("Polygon_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrSolid,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0x00, 0xFF),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0xFF, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of Polygon.  The image on the left should match the image on the right, the image should be a triangle with a thin black pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Polygon.  The image on the left should match the image on the right, the image should be a triangle with a thin green pen and a solid filled interior."),
                                                            TEXT("This test verifies the functionality of Polygon.  The image on the left should match the image on the right, the image should be a triangle with a thin dashed pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Polygon.  The image on the left should match the image on the right, the image should be a triangle with a wide blue pen, it should be on a green background, and have a puppy filled interior."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops))&&
            (countof(dwRops) == countof(dwPenWidth)) &&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor)) &&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));
                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the lines to the printer
                        pt[0].x = dcd.nCurrentX + nSquare/2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nCurrentY + + dwPenWidth[i]/2;
                        pt[1].x = dcd.nCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, Polygon(dcd.hdcPrint, pt, ncPoints));

                        // and now to the verification surface
                        pt[0].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        pt[1].x = dcd.nVidCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nVidCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, Polygon(dcd.hdcVideoMemory, pt, ncPoints));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
LineTo_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("LineTo_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_BLACK,
                            R2_COPYPEN,
                            R2_WHITE,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0x00, 0x00, 0x00),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0x00, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of MoveTo/LineTo.  The image on the left should match the image on the right, the image should be a triangle with a thin black pen and an empty interior."),
                                                            TEXT("This test verifies the functionality of MoveTo/LineTo.  The image on the left should match the image on the right, the image should be a triangle with a thin red dashed pen and an empty interior."),
                                                            TEXT("This test verifies the functionality of MoveTo/LineTo.  The image on the left should match the image on the right, the image should be a triangle with a wide pen, it should be white on a black background."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops)) &&
            (countof(dwRops) == countof(dwPenWidth)) &&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor))&&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};
                        int PointIndex;

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));
                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the lines to the printer
                        pt[0].x = dcd.nCurrentX + nSquare/2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nCurrentY + + dwPenWidth[i]/2;
                        pt[1].x = dcd.nCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, MoveToEx(dcd.hdcPrint, pt[0].x, pt[0].y, NULL));
                        for(PointIndex = 1; PointIndex < ncPoints; PointIndex++)
                            CheckNotRESULT(FALSE, LineTo(dcd.hdcPrint, pt[PointIndex].x, pt[PointIndex].y));

                        // and now to the verification surface
                        pt[0].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[0].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        pt[1].x = dcd.nVidCurrentX + nSquare + dwPenWidth[i]/2;
                        pt[1].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[2].x = dcd.nVidCurrentX + dwPenWidth[i]/2;
                        pt[2].y = dcd.nVidCurrentY + nSquare + dwPenWidth[i]/2;
                        pt[3].x = dcd.nVidCurrentX + nSquare /2 + dwPenWidth[i]/2;
                        pt[3].y = dcd.nVidCurrentY + dwPenWidth[i]/2;
                        CheckNotRESULT(FALSE, MoveToEx(dcd.hdcVideoMemory, pt[0].x, pt[0].y, NULL));
                        for(PointIndex = 1; PointIndex < ncPoints; PointIndex++)
                            CheckNotRESULT(FALSE, LineTo(dcd.hdcVideoMemory, pt[PointIndex].x, pt[PointIndex].y));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
Rectangle_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("Rectangle_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrSolid,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0x00, 0xFF),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0xFF, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of Rectangle.  The image on the left should match the image on the right, the image should be a rectangle with a thin black pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Rectangle.  The image on the left should match the image on the right, the image should be a rectangle with a thin green pen and a solid red filled interior."),
                                                            TEXT("This test verifies the functionality of Rectangle.  The image on the left should match the image on the right, the image should be a rectangle with a thin dashed pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Rectangle.  The image on the left should match the image on the right, the image should be a rectangle with a wide blue pen on a green background, with a puppy filled interior."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops)) &&
            (countof(dwRops) == countof(dwPenWidth)) &&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor)) &&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));

                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the rectangle to the printer
                        CheckNotRESULT(FALSE, Rectangle(dcd.hdcPrint, dcd.nCurrentX + dwPenWidth[i], dcd.nCurrentY +  dwPenWidth[i], dcd.nCurrentX + nSquare - dwPenWidth[i], dcd.nCurrentY + nSquare  - dwPenWidth[i]));

                        // and now to the verification surface
                        CheckNotRESULT(FALSE, Rectangle(dcd.hdcVideoMemory, dcd.nVidCurrentX + dwPenWidth[i], dcd.nVidCurrentY +  dwPenWidth[i], dcd.nVidCurrentX + nSquare - dwPenWidth[i], dcd.nVidCurrentY + nSquare  - dwPenWidth[i]));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
RoundRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("RoundRect_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrSolid,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0x00, 0xFF),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0xFF, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of RoundRect.  The image on the left should match the image on the right, the image should be a rounded rectangle with a thin black pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of RoundRect.  The image on the left should match the image on the right, the image should be a rounded rectangle with a thin green pen and a solid red filled interior."),
                                                            TEXT("This test verifies the functionality of RoundRect.  The image on the left should match the image on the right, the image should be a rounded rectangle with a thin dashed pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of RoundRect.  The image on the left should match the image on the right, the image should be a rounded rectangle with a wide blue pen on a green background, with a puppy filled interior."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops)) &&
            (countof(dwRops) == countof(dwPenWidth)) &&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor)) &&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));

                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the rectangle to the printer
                        CheckNotRESULT(FALSE, RoundRect(dcd.hdcPrint, dcd.nCurrentX + dwPenWidth[i], dcd.nCurrentY +  dwPenWidth[i], dcd.nCurrentX + nSquare - dwPenWidth[i], dcd.nCurrentY + nSquare  - dwPenWidth[i], nSquare/4, nSquare/4));

                        // and now to the verification surface
                        CheckNotRESULT(FALSE, RoundRect(dcd.hdcVideoMemory, dcd.nVidCurrentX + dwPenWidth[i], dcd.nVidCurrentY +  dwPenWidth[i], dcd.nVidCurrentX + nSquare - dwPenWidth[i], dcd.nVidCurrentY + nSquare  - dwPenWidth[i], nSquare/4, nSquare/4));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
Ellipse_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("Ellipse_T"));

    HBRUSH hbrSolid, hbrBK, hbrPattern;
    // always select the pattern brush into the DC,
    // should be irrelevent to PolyLine.
    HBRUSH *hbr[] = {
                                &hbrPattern,
                                &hbrSolid,
                                &hbrPattern,
                                &hbrPattern
                              };

    DWORD dwRops[] = {
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                            R2_COPYPEN,
                        };

    DWORD dwPenWidth[] = {
                            1,
                            1,
                            0,
                            30,
                        };

    DWORD dwPenStyle[] = {
                            PS_SOLID,
                            PS_SOLID,
                            PS_DASH,
                            PS_SOLID,
                        };

    COLORREF crPenColor[] = {
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0xFF, 0x00),
                            RGB(0x00, 0x00, 0x00),
                            RGB(0x00, 0x00, 0xFF),
                        };

    COLORREF crBKColor[] = {
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0xFF, 0xFF, 0xFF),
                            RGB(0x00, 0xFF, 0x00),
                        };

    COLORREF crBrushColor[] = {
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                            RGB(0xFF, 0x00, 0x00),
                        };

    TCHAR *tszTestDescription[] = {
                                                            TEXT("This test verifies the functionality of Ellipse.  The image on the left should match the image on the right, the image should be a circle with a thin black pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Ellipse.  The image on the left should match the image on the right, the image should be a circle with a thin green pen and a solid red filled interior."),
                                                            TEXT("This test verifies the functionality of Ellipse.  The image on the left should match the image on the right, the image should be a circle with a thin dashed pen and a puppy filled interior."),
                                                            TEXT("This test verifies the functionality of Ellipse.  The image on the left should match the image on the right, the image should be a circle with a wide blue pen on a green background, with a puppy filled interior."),
                                                        };
    POINT pt[4];
    int ncPoints = countof(pt);
    DCDATA dcd;

    if(!((countof(hbr) == countof(dwRops) )&&
            (countof(dwRops) == countof(dwPenWidth)) &&
            (countof(dwPenWidth) == countof(dwPenStyle)) &&
            (countof(dwPenStyle) == countof(crPenColor)) &&
            (countof(crPenColor) == countof(crBKColor)) &&
            (countof(crBKColor) == countof(crBrushColor)) &&
            (countof(crBrushColor) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            if(gPrintHandler.GetDCData(&dcd))
            {
                int nSquare = min(dcd.nWidth /2, dcd.nHeight / 2);
                if(gPrintHandler.TestStart(nSquare + dwPenWidth[i] + 1, nSquare + dwPenWidth[i] + 1))
                {
                    // get dc data
                    if(gPrintHandler.GetDCData(&dcd))
                    {
                        HPEN hpn;
                        HPEN hpnStock1, hpnStock2;
                        HBITMAP hbmp;
                        HBRUSH hbrStock1, hbrStock2;
                        int nPrintStockRop2, nVidStockRop2;
                        RECT rc1 = {dcd.nCurrentX, dcd.nCurrentY, dcd.nCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nCurrentY + nSquare + dwPenWidth[i] + 1};
                        RECT rc2 = {dcd.nVidCurrentX, dcd.nVidCurrentX, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1, dcd.nVidCurrentX + nSquare + dwPenWidth[i] + 1};

                        // set up our brushes, and initialize the backgrounds.
                        CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crBrushColor[i]));
                        CheckNotRESULT(NULL, hbrBK = CreateSolidBrush(crBKColor[i]));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcPrint, &rc1, hbrBK));
                        CheckNotRESULT(FALSE, FillRect(dcd.hdcVideoMemory, &rc2, hbrBK));
                        CheckNotRESULT(NULL, hbmp = LoadBitmap(globalInst, TEXT("JADE24")));
                        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
                        CheckNotRESULT(FALSE, DeleteObject(hbmp));

                        CheckNotRESULT(NULL, hpn = CreatePen(dwPenStyle[i], dwPenWidth[i], crPenColor[i]));
                        nPrintStockRop2 = SetROP2(dcd.hdcPrint, dwRops[i]);
                        nVidStockRop2 = SetROP2(dcd.hdcVideoMemory, dwRops[i]);
                        CheckNotRESULT(NULL, hpnStock1 = (HPEN) SelectObject(dcd.hdcPrint, hpn));
                        CheckNotRESULT(NULL, hpnStock2 = (HPEN) SelectObject(dcd.hdcVideoMemory, hpn));

                        CheckNotRESULT(NULL, SetBrushOrgEx(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL));

                        CheckNotRESULT(NULL, hbrStock1 = (HBRUSH) SelectObject(dcd.hdcPrint, *hbr[i]));
                        CheckNotRESULT(NULL, hbrStock2 = (HBRUSH) SelectObject(dcd.hdcVideoMemory, *hbr[i]));

                        // draw the rectangle to the printer
                        CheckNotRESULT(FALSE, Ellipse(dcd.hdcPrint, dcd.nCurrentX + dwPenWidth[i], dcd.nCurrentY +  dwPenWidth[i], dcd.nCurrentX + nSquare - dwPenWidth[i], dcd.nCurrentY + nSquare  - dwPenWidth[i]));

                        // and now to the verification surface
                        CheckNotRESULT(FALSE, Ellipse(dcd.hdcVideoMemory, dcd.nVidCurrentX + dwPenWidth[i], dcd.nVidCurrentY +  dwPenWidth[i], dcd.nVidCurrentX + nSquare - dwPenWidth[i], dcd.nVidCurrentY + nSquare  - dwPenWidth[i]));

                        // cleanup
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hpnStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hpnStock2));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hbrStock1));
                        CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hbrStock2));
                        CheckNotRESULT(FALSE, DeleteObject(hpn));
                        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
                        CheckNotRESULT(FALSE, DeleteObject(hbrSolid));
                        CheckNotRESULT(FALSE, DeleteObject(hbrBK));
                        SetROP2(dcd.hdcPrint, nPrintStockRop2);
                        SetROP2(dcd.hdcVideoMemory, nVidStockRop2);
                    }
                    else info(ABORT, TEXT("Failed to retrieve the DC data."));
                    gPrintHandler.TestFinished();
                }
                else info(ABORT, TEXT("Failed to start the test."));
            }
            else info(ABORT, TEXT("Failed to retrieve the initial DC data."));
        }
    }

    return getCode();
}

TESTPROCAPI
DrawText_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("DrawText_T"));
    COLORREF crForeground[] = {
                                                    RGB(0x00, 0x00, 0x00),
                                                    RGB(0xFF, 0xFF, 0xFF),
                                                    RGB(0x00, 0xFF, 0x00)

                                                    };

    COLORREF crBackground[] = {
                                                    RGB(0xFF, 0xFF, 0xFF),
                                                    RGB(0x00, 0x00, 0x00),
                                                    RGB(0xFF, 0x00, 0xFF)
                                                    };

    TCHAR *tszTestDescription[] = {
                                                        TEXT("This test verifies the functionality of DrawText. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be black text on a white background."),
                                                        TEXT("This test verifies the functionality of DrawText. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be white text on a black background."),
                                                        TEXT("This test verifies the functionality of DrawText. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be green text on a fuscha background.")
                                                      };
    DCDATA dcd;

    // this is an estimate of the maximum width/height that this test will use from the page.
    int nOperationHeight = 100;
    int nOperationWidth = 600;

    if(!((countof(crBackground) == countof(tszTestDescription))&&(countof(crForeground) == countof(crBackground))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {
        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            // tell the framework what we're using in terms of width and height of a page,
            // it'll give us the new page if it's needed.
            if(gPrintHandler.TestStart(nOperationWidth, nOperationHeight))
            {
                // get dc data
                if(gPrintHandler.GetDCData(&dcd))
                {
                    RECT rc;
                    LOGFONT lf;
                    HFONT hfont;
                    HFONT hfontStock1, hfontStock2;
                    COLORREF crText, crBack;

                    memset(&lf, 0, sizeof(LOGFONT));
                    lf.lfHeight = nOperationHeight;
                    CheckNotRESULT(NULL, hfont = CreateFontIndirect(&lf));

                    CheckNotRESULT(NULL, hfontStock1 = (HFONT) SelectObject(dcd.hdcPrint, hfont));
                    CheckNotRESULT(NULL, hfontStock2 = (HFONT) SelectObject(dcd.hdcVideoMemory, hfont));

                    // print some text to both dc's
                    rc.left = dcd.nCurrentX;
                    rc.top = dcd.nCurrentY;
                    rc.right = dcd.nWidth;
                    rc.bottom = dcd.nHeight;
                    crText = SetTextColor(dcd.hdcPrint, crForeground[i]);
                    crBack = SetBkColor(dcd.hdcPrint, crBackground[i]);
                    CheckNotRESULT(FALSE, DrawText(dcd.hdcPrint, TEXT("TEST STRING"), -1, &rc, NULL));
                    SetTextColor(dcd.hdcPrint, crText);
                    SetBkColor(dcd.hdcPrint, crBack);

                    rc.left = dcd.nVidCurrentX;
                    rc.top = dcd.nVidCurrentY;
                    rc.right = dcd.nVidWidth;
                    rc.bottom = dcd.nVidHeight;
                    SetTextColor(dcd.hdcVideoMemory, crForeground[i]);
                    SetBkColor(dcd.hdcVideoMemory, crBackground[i]);
                    CheckNotRESULT(FALSE, DrawText(dcd.hdcVideoMemory, TEXT("TEST STRING"), -1, &rc, NULL));
                    SetTextColor(dcd.hdcVideoMemory, crText);
                    SetBkColor(dcd.hdcVideoMemory, crBack);

                    // now that the test is done, update our current Y location, cleanup, and tell the test suite we're done.
                    dcd.nCurrentY += nOperationHeight;

                    CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hfontStock1));
                    CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hfontStock2));
                    CheckNotRESULT(FALSE, DeleteObject(hfont));
                }
                else info(ABORT, TEXT("Failed to retrieve the DC data."));

                // test finished
                gPrintHandler.TestFinished();
            }
            else info(ABORT, TEXT("Failed to start the test."));
        }
    }
    return getCode();
}

TESTPROCAPI
ExtTextOut_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES
    info(ECHO, TEXT("ExtTextOut_T"));
    COLORREF crForeground[] = {
                                                    RGB(0x00, 0x00, 0x00),
                                                    RGB(0xFF, 0xFF, 0xFF),
                                                    RGB(0x00, 0xFF, 0x00)

                                                    };

    COLORREF crBackground[] = {
                                                    RGB(0xFF, 0xFF, 0xFF),
                                                    RGB(0x00, 0x00, 0x00),
                                                    RGB(0xFF, 0x00, 0xFF)
                                                    };

    TCHAR *tszTestDescription[] = {
                                                        TEXT("This test verifies the functionality of ExtTextOut. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be black text on a white background."),
                                                        TEXT("This test verifies the functionality of ExtTextOut. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be white text on a black background."),
                                                        TEXT("This test verifies the functionality of ExtTextOut. The string \"Test String\" should be printed on the page.  Please also verify that the left half of the page matches the right half.  It should be green text on a fuscha background.")
                                                      };
    DCDATA dcd;

    // this is an estimate of the maximum width/height that this test will use from the page.
    int nOperationHeight = 100;
    int nOperationWidth = 600;

    if(!((countof(crForeground) == countof(crBackground)) && (countof(crBackground) == countof(tszTestDescription))))
        info(FAIL, TEXT("Test bug, array sizes don't match"));

    for(int i = 0; i < countof(tszTestDescription); i++)
    {

        // the framework handles printing the tests description before the case, on the primary, and to the debug stream
        if(gPrintHandler.SetTestDescription(tszTestDescription[i]))
        {
            // tell the framework what we're using in terms of width and height of a page,
            // it'll give us the new page if it's needed.
            if(gPrintHandler.TestStart(nOperationWidth, nOperationHeight))
            {
                // get dc data
                if(gPrintHandler.GetDCData(&dcd))
                {
                    RECT rc;
                    LOGFONT lf;
                    HFONT hfont;
                    HFONT hfontStock1, hfontStock2;
                    COLORREF crText, crBack;

                    memset(&lf, 0, sizeof(LOGFONT));
                    lf.lfHeight = nOperationHeight;
                    CheckNotRESULT(NULL, hfont = CreateFontIndirect(&lf));

                    CheckNotRESULT(NULL, hfontStock1 = (HFONT) SelectObject(dcd.hdcPrint, hfont));
                    CheckNotRESULT(NULL, hfontStock2 = (HFONT) SelectObject(dcd.hdcVideoMemory, hfont));

                    // print some text to both dc's
                    rc.left = dcd.nCurrentX;
                    rc.top = dcd.nCurrentY;
                    rc.right = dcd.nWidth;
                    rc.bottom = dcd.nHeight;

                    crText = SetTextColor(dcd.hdcPrint, crForeground[i]);
                    crBack = SetBkColor(dcd.hdcPrint, crBackground[i]);
                    CheckNotRESULT(FALSE, ExtTextOut(dcd.hdcPrint, dcd.nCurrentX, dcd.nCurrentY, NULL, &rc, TEXT("TEST STRING"), _tcslen(TEXT("TEST STRING")), NULL));
                    SetTextColor(dcd.hdcPrint, crText);
                    SetBkColor(dcd.hdcPrint, crBack);

                    rc.left = dcd.nVidCurrentX;
                    rc.top = dcd.nVidCurrentY;
                    rc.right = dcd.nVidWidth;
                    rc.bottom = dcd.nVidHeight;

                    crText = SetTextColor(dcd.hdcVideoMemory, crForeground[i]);
                    crBack = SetBkColor(dcd.hdcVideoMemory, crBackground[i]);
                    CheckNotRESULT(FALSE, ExtTextOut(dcd.hdcVideoMemory, dcd.nVidCurrentX, dcd.nVidCurrentY, NULL, &rc, TEXT("TEST STRING"), _tcslen(TEXT("TEST STRING")), NULL));
                    SetTextColor(dcd.hdcVideoMemory, crText);
                    SetBkColor(dcd.hdcVideoMemory, crBack);

                    // now that the test is done, update our current Y location, cleanup, and tell the test suite we're done.
                    dcd.nCurrentY += nOperationHeight;

                    CheckNotRESULT(NULL, SelectObject(dcd.hdcPrint, hfontStock1));
                    CheckNotRESULT(NULL, SelectObject(dcd.hdcVideoMemory, hfontStock2));
                    CheckNotRESULT(FALSE, DeleteObject(hfont));
                }
                else info(ABORT, TEXT("Failed to retrieve the DC data."));

                // test finished
                gPrintHandler.TestFinished();
            }
            else info(ABORT, TEXT("Failed to start the test."));
        }
    }
    return getCode();
}

#pragma prefast(pop)

