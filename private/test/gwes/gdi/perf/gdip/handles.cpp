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
#include "handles.h"

void
setRGBQUAD(MYRGBQUAD * rgbq, int r, int g, int b)
{
    rgbq->rgbBlue = (BYTE)b;
    rgbq->rgbGreen = (BYTE)g;
    rgbq->rgbRed = (BYTE)r;
    rgbq->rgbReserved = 0;
}

// Desktop doesn't make a destinction on alpha or no alpha.
#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS BI_BITFIELDS
#endif

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif

#ifndef CLEARTYPE_COMPAT_QUALITY
#define CLEARTYPE_COMPAT_QUALITY CLEARTYPE_QUALITY
#endif

HBITMAP myCreateDIBSection(HDC hdc, VOID ** ppvBits, int d, int w, int h, int nBitmask, UINT Usage)
{
    HBITMAP hBmp = NULL;
    struct MYBITMAPINFO bmi;

    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = w;
    bmi.bmih.biHeight = h;
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = (WORD)d;
    bmi.bmih.biCompression = BI_RGB;
    bmi.bmih.biSizeImage = bmi.bmih.biXPelsPerMeter = bmi.bmih.biYPelsPerMeter = 0;
    bmi.bmih.biClrUsed = bmi.bmih.biClrImportant = 0;

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
    // fill in the structure for 16 and 32bpp.  24bpp doesn't need anything else filled in.
    else if (bmi.bmih.biBitCount == 16 || bmi.bmih.biBitCount == 32)
    {
        switch(nBitmask)
        {
            case 4444:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xF000;
                bmi.ct[2].rgbMask = 0x000F;
                bmi.ct[1].rgbMask = 0x00F0;
                bmi.ct[0].rgbMask = 0x0F00;
                break;
            case 565:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x07E0;
                bmi.ct[0].rgbMask = 0xF800;
                break;
            case 555:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[0].rgbMask = 0x7C00;
                break;
            case 1555:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0x8000;
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[0].rgbMask = 0x7C00;
                break;
            case 8888:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xFF000000;
                bmi.ct[2].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[0].rgbMask = 0x00FF0000;
                break;
            case 888:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[2].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[0].rgbMask = 0x00FF0000;
                break;
            case 4444 + BGROFFSET:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xF000;
                bmi.ct[0].rgbMask = 0x000F;
                bmi.ct[1].rgbMask = 0x00F0;
                bmi.ct[2].rgbMask = 0x0F00;
                break;
            case 565 + BGROFFSET:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x07E0;
                bmi.ct[2].rgbMask = 0xF800;
                break;
            case 555 + BGROFFSET:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[2].rgbMask = 0x7C00;
                break;
            case 1555 + BGROFFSET:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0x8000;
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[2].rgbMask = 0x7C00;
                break;
            case 8888 + BGROFFSET:
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xFF000000;
                bmi.ct[0].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[2].rgbMask = 0x00FF0000;
                break;
            case 888 + BGROFFSET:
                bmi.bmih.biCompression = BI_BITFIELDS;
                bmi.ct[0].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[2].rgbMask = 0x00FF0000;
                break;
            case RGBEMPTY:
                // BI_RGB
                bmi.ct[3].rgbMask = 0x0;
                bmi.ct[2].rgbMask = 0x0;
                bmi.ct[1].rgbMask = 0x0;
                bmi.ct[0].rgbMask = 0x0;
                break;
            default:
                info(FAIL, TEXT("CreateRGBDIBSection unknown bit mask %d"), nBitmask);
                break;
        }
    }
    else if(bmi.bmih.biBitCount != 24)
        info(FAIL, TEXT("Unknown bit depth %d"), bmi.bmih.biBitCount);

    // create the DIB from the BITMAPINFO filled in above.
    hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, Usage, ppvBits, NULL, NULL);

    if (!hBmp)
    {
        info(FAIL, TEXT("Surface creation failed."));
    }

    return hBmp;
}

HDC
myCreateSurface(TSTRING tsDeviceName, TSTRING tsSurfaceType, UINT *SurfaceBitDepth, HWND *hWnd)
{
    HDC hSurface = NULL;
    RECT rcWorkArea;
    HDC hdcPrimary = NULL;
    HBITMAP hBmp = NULL;
    VOID *pVoid = NULL;
    int nDepth = 0;

    // if the return pointer for the bit depth is invalid, then fail the call.
    if(NULL == SurfaceBitDepth)
    {
        info(FAIL, TEXT("Return pointer for bit depth is invalid, surface creation failed."));
    }
    // if the return pointer for our HWND is invalid, fail the call.
    else if(NULL == hWnd)
    {
        info(FAIL, TEXT("Return pointer for HWND is invalid, surface creation failed."));
    }
    // retrieve the work area for system/video memory surface creation.
    // if we can't, then fail the call.
    else if(SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0))
    {
        // just retrieve the DC for the primary
        if(tsSurfaceType == TEXT("Primary"))
        {
            info(DETAIL, TEXT("Primary surface requested."));

            *hWnd = CreateWindowEx(0x00, TEXT("GDI Performance"), tsSurfaceType.c_str(),
                WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                NULL, NULL, NULL, NULL);

            if (*hWnd)
            {
                ShowWindow(*hWnd, SW_SHOW);
                UpdateWindow(*hWnd);
                hSurface = GetDC(*hWnd);
                *SurfaceBitDepth = GetDeviceCaps(hSurface, BITSPIXEL);
            }
            else
                info(FAIL, TEXT("Failed to create an HWND."));
        }
        else 
        {
            // retrieve the primary dc for compatible creation and querying parameters.
            if(hdcPrimary = CreateDC(tsDeviceName.c_str(), NULL, NULL, NULL))
            {
                *SurfaceBitDepth = GetDeviceCaps(hdcPrimary, BITSPIXEL);
                // create our compatible DC for selection.
                if(hSurface = CreateCompatibleDC(hdcPrimary))
                {
                    // create the surface, video memory or system memory.
                    if(tsSurfaceType.npos != tsSurfaceType.find(TEXT("System_Memory")))
                    {
                        if(_stscanf_s(tsSurfaceType.c_str(), TEXT("System_Memory%d"), &nDepth) == 1)
                            *SurfaceBitDepth = nDepth;

                        info(DETAIL, TEXT("System Memory%d surface requested."), *SurfaceBitDepth);

                        hBmp = CreateBitmap(rcWorkArea.right - rcWorkArea.left, 
                                                  rcWorkArea.bottom - rcWorkArea.top, 1, *SurfaceBitDepth, NULL);
                    }
                    else if(tsSurfaceType == TEXT("Video_Memory"))
                    {
                        info(DETAIL, TEXT("Video memory surface requested."));
                        hBmp = CreateCompatibleBitmap(hdcPrimary, rcWorkArea.right - rcWorkArea.left, 
                                                                                    rcWorkArea.bottom - rcWorkArea.top);
                    }
                    else if(tsSurfaceType.npos != tsSurfaceType.find(TEXT("DIB")))
                    {
                        int nColors = DIB_RGB_COLORS, nMask = RGBEMPTY;
                        int nEntriesRecieved = 0;
                        TCHAR tcFormat[MAXRGBDESCRIPTORLENGTH] = {NULL};
                        TSTRING ts;

                        // retrieved the format
                        nEntriesRecieved = _stscanf_s(tsSurfaceType.c_str(), TEXT("DIB%d_%03s%d"), &nDepth, tcFormat, &nMask);
                        ts = tcFormat;

                        // the bitmask should be empty if there are 2 or fewer entries found
                        // if there are 0, then it's an invalid bit depth and myCreateDIBSection will handle it.
                        if(nEntriesRecieved <= 2)
                            nMask = RGBEMPTY;
                        // if there are 2 or more, then ts is valid, so check it.
                        else 
                        {
                            if(ts == TEXT("PAL"))
                                nColors = DIB_PAL_COLORS;
                            else if(ts == TEXT("BGR"))
                                nMask += BGROFFSET;
                            else if(ts != TEXT("RGB"))
                                info(FAIL, TEXT("Unknown surface format requested %s."), ts.c_str());
                        }
                        *SurfaceBitDepth = nDepth;
                        hBmp = myCreateDIBSection(hdcPrimary, (VOID **) pVoid, nDepth, rcWorkArea.right - rcWorkArea.left, 
                                                                  rcWorkArea.bottom - rcWorkArea.top, nMask, nColors);
                    }
                    else info(FAIL, TEXT("Unknown surface requested %s."), tsSurfaceType.c_str());

                    // if the bitmap creation succeeded, then select it
                    if(hBmp)
                    {
                        // if the selection fails (unlikely but possible), cleanup and fail the call.
                        if(!SelectObject(hSurface, hBmp))
                        {
                            info(FAIL, TEXT("Surface selection failed."));
                            DeleteObject(hBmp);
                            DeleteObject(hSurface);
                            hSurface = NULL;
                        }
                    }
                    else 
                    {
                        info(FAIL, TEXT("Surface creation failed."));
                        DeleteObject(hSurface);
                        hSurface = NULL;
                    }
                }
            }
            // release the temporary primary DC since we're done with it.
            ReleaseDC(NULL, hdcPrimary);
        }
    }

    return hSurface;
}

BOOL
myDeleteSurface(HDC hSurface, TSTRING tsSurfaceType, HWND hWnd)
{
    // retrieve a handle to the stock bitmap.
    HBITMAP hbmpStock = CreateBitmap(1, 1, 1, 1, NULL);
    BOOL bRval = TRUE;
    // if it's the primary, release it.
    if(tsSurfaceType == TEXT("Primary"))
    {
        bRval = bRval && ReleaseDC(hWnd, hSurface);
        if (hWnd)
            bRval = bRval && DestroyWindow(hWnd);
    }
    else
    {
        // it's not the primary, so it's a selected bitmap.
        HBITMAP hbmpOrig = (HBITMAP) SelectObject(hSurface, hbmpStock);
        // if the select object failed, hbmpOrig will be NULL, so DeleteObject will fail.
        bRval = bRval && DeleteObject(hbmpOrig);
        bRval = bRval && DeleteDC(hSurface);
    }

    DeleteObject(hbmpStock);
    
    return bRval;
}

struct NameValPair nvBrushes[] = { {BLACK_BRUSH, TEXT("BLACK")},
                                                     {DKGRAY_BRUSH, TEXT("DKGRAY")},
                                                     {GRAY_BRUSH, TEXT("GRAY")},
                                                     {HOLLOW_BRUSH, TEXT("HOLLOW")},
                                                     {LTGRAY_BRUSH, TEXT("LTGRAY")},
                                                     {NULL_BRUSH, TEXT("NULL")},
                                                     {WHITE_BRUSH, TEXT("WHITE")},
                                                     {0, NULL}
                                                  };

HBRUSH
myCreateBrush(TSTRING tsBrushName)
{
    HBRUSH hbr = NULL;
    HBITMAP hbmp;
    DWORD dwStockBrushID;

    if(nvSearch(nvBrushes, tsBrushName, &dwStockBrushID))
        hbr = (HBRUSH) GetStockObject(dwStockBrushID);
    else if(tsBrushName == TEXT("Solid"))
    {
        hbr = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
    }
    else if(tsBrushName.npos != tsBrushName.find(TEXT("Pattern")))
    {
        int nWidth = 0, nHeight = 0, nDepth = 0;

        if(tsBrushName == TEXT("Pattern"))
        {
            hbmp = CreateBitmap(8, 8, 1, 16, NULL);
            hbr = CreatePatternBrush(hbmp);
            DeleteObject(hbmp);
        }
        else if( 3 == _stscanf_s(tsBrushName.c_str(), TEXT("Pattern%dx%d_%dbpp"), &nWidth, &nHeight, &nDepth))
        {
            hbmp = CreateBitmap(nWidth, nHeight, 1, nDepth, NULL);
            hbr = CreatePatternBrush(hbmp);
            DeleteObject(hbmp);
        }
        else info(FAIL, TEXT("Brush string %s failed to parse correctly."), tsBrushName.c_str());

    }
    else info(FAIL, TEXT("Unknown brush requested %s"), tsBrushName.c_str());

    return hbr;
}

struct NameValPair nvPens[] = { {NULL_PEN, TEXT("NULL")},
                                               {BLACK_PEN, TEXT("BLACK")},
                                               {WHITE_PEN, TEXT("WHITE")}
                                            };

HPEN
myCreatePen(TSTRING tsPenName)
{
    HPEN hpn = NULL;
    int nWidth = 0, nStyle = 0;
    DWORD dwStockPenID;

    if(nvSearch(nvPens, tsPenName, &dwStockPenID))
        hpn = (HPEN) GetStockObject(dwStockPenID);
    else if(tsPenName.npos != tsPenName.find(TEXT("SOLID")))
    {
        if( 0 == _stscanf_s(tsPenName.c_str(), TEXT("SOLID%d"), &nWidth))
            nWidth = 1;
        hpn = CreatePen(PS_SOLID, nWidth, RGB(0x55, 0x55, 0x55));
    }
    else if(tsPenName.npos != tsPenName.find(TEXT("DASH")))
    {
        if( 0 == _stscanf_s(tsPenName.c_str(), TEXT("DASH%d"), &nWidth))
            nWidth = 1;
        hpn = CreatePen(PS_DASH, nWidth, RGB(0x55, 0x55, 0x55));
    }
    else if(tsPenName.npos != tsPenName.find(TEXT("NULL")))
    {
        if( 0 == _stscanf_s(tsPenName.c_str(), TEXT("NULL%d"), &nWidth))
            nWidth = 1;
        hpn = CreatePen(PS_NULL, nWidth, RGB(0x55, 0x55, 0x55));
    }
    else info(FAIL, TEXT("Unknown pen requested %s"), tsPenName.c_str());

    return hpn;
}

struct NameValPair nvFontWeights[] = {
                                                         {FW_DONTCARE, TEXT("FW_DONTCARE")},
                                                         {FW_THIN, TEXT("FW_THIN")},
                                                         {FW_EXTRALIGHT, TEXT("FW_EXTRALIGHT")},
//                                                         {FW_ULTRALIGHT, TEXT("FW_ULTRALIGHT")},
                                                         {FW_LIGHT, TEXT("FW_LIGHT")},
                                                         {FW_NORMAL, TEXT("FW_NORMAL")},
//                                                         {FW_REGULAR, TEXT("FW_REGULAR")},
                                                         {FW_MEDIUM, TEXT("FW_MEDIUM")},
                                                         {FW_SEMIBOLD, TEXT("FW_SEMIBOLD")},
//                                                         {FW_DEMIBOLD, TEXT("FW_DEMIBOLD")},
                                                         {FW_BOLD, TEXT("FW_BOLD")},
                                                         {FW_EXTRABOLD, TEXT("FW_EXTRABOLD")},
//                                                         {FW_ULTRABOLD, TEXT("FW_ULTRABOLD")},
                                                         {FW_HEAVY, TEXT("FW_HEAVY")},
//                                                         {FW_BLACK, TEXT("FW_BLACK")},
                                                         {0, NULL}
                                                       };


struct NameValPair nvCharsets[] = {
                                                         {ANSI_CHARSET, TEXT("ANSI_CHARSET") },
                                                         {CHINESEBIG5_CHARSET, TEXT("CHINESEBIG5_CHARSET") },
                                                         {EASTEUROPE_CHARSET, TEXT("EASTEUROPE_CHARSET") },
                                                         {GREEK_CHARSET, TEXT("GREEK_CHARSET") },
                                                         {MAC_CHARSET, TEXT("MAC_CHARSET") },
                                                         {RUSSIAN_CHARSET, TEXT("RUSSIAN_CHARSET") },
                                                         {SYMBOL_CHARSET, TEXT("SYMBOL_CHARSET") },
                                                         {BALTIC_CHARSET, TEXT("BALTIC_CHARSET") },
                                                         {DEFAULT_CHARSET, TEXT("DEFAULT_CHARSET") },
                                                         {GB2312_CHARSET, TEXT("GB2312_CHARSET") },
//                                                         {HANGUL_CHARSET, TEXT("HANGUL_CHARSET") },
                                                         {OEM_CHARSET, TEXT("OEM_CHARSET") },
                                                         {SHIFTJIS_CHARSET, TEXT("SHIFTJIS_CHARSET") },
                                                         {TURKISH_CHARSET, TEXT("TURKISH_CHARSET") },
                                                         {JOHAB_CHARSET, TEXT("JOHAB_CHARSET") },
                                                         {HEBREW_CHARSET, TEXT("HEBREW_CHARSET") },
                                                         {ARABIC_CHARSET, TEXT("ARABIC_CHARSET") },
                                                         {THAI_CHARSET, TEXT("THAI_CHARSET") },
                                                         {0, NULL}
                                                 };

struct NameValPair nvOutPrecision[] = {
                                                         {OUT_DEFAULT_PRECIS, TEXT("OUT_DEFAULT_PRECIS") },
                                                         {OUT_RASTER_PRECIS, TEXT("OUT_RASTER_PRECIS") },
                                                         {OUT_STRING_PRECIS, TEXT("OUT_STRING_PRECIS") },
                                                         {0, NULL}
                                                 };

struct NameValPair nvClipPrecision[] = {
                                                        {CLIP_DEFAULT_PRECIS, TEXT("CLIP_DEFAULT_PRECIS") },
                                                        {CLIP_CHARACTER_PRECIS, TEXT("CLIP_CHARACTER_PRECIS") },
                                                        {CLIP_STROKE_PRECIS, TEXT("CLIP_STROKE_PRECIS") },
                                                        {0, NULL}
                                                 };

struct NameValPair nvQuality[] = {
                                                        {ANTIALIASED_QUALITY, TEXT("ANTIALIASED_QUALITY") },
                                                        {NONANTIALIASED_QUALITY, TEXT("NONANTIALIASED_QUALITY") },
                                                        {CLEARTYPE_COMPAT_QUALITY, TEXT("CLEARTYPE_COMPAT_QUALITY") },
                                                        {CLEARTYPE_QUALITY, TEXT("CLEARTYPE_QUALITY") },
                                                        {DEFAULT_QUALITY, TEXT("DEFAULT_QUALITY") },
                                                        {DRAFT_QUALITY, TEXT("DRAFT_QUALITY") },
                                                        {0, NULL}
                                               };

struct NameValPair nvPitch[] = {
                                                        {DEFAULT_PITCH, TEXT("DEFAULT_PITCH")},
                                                        {FIXED_PITCH, TEXT("FIXED_PITCH")},
                                                        {VARIABLE_PITCH, TEXT("VARIABLE_PITCH")},
                                                        {0, NULL}
                                           };

struct NameValPair nvFamily[] = {
                                                        {FF_DECORATIVE, TEXT("FF_DECORATIVE")},
                                                        {FF_DONTCARE, TEXT("FF_DONTCARE")},
                                                        {FF_MODERN, TEXT("FF_MODERN")},
                                                        {FF_ROMAN, TEXT("FF_ROMAN")},
                                                        {FF_SCRIPT, TEXT("FF_SCRIPT")},
                                                        {FF_SWISS, TEXT("FF_SWISS")},
                                                        {0, NULL}
                                              };

HFONT
myCreateFont(DWORD dwHeight, DWORD dwWidth, DWORD dwEscapement, 
                    TSTRING tsWeight, BOOL bItalic, BOOL bUnderline, BOOL bStrikeOut, 
                    TSTRING tsCharSet, TSTRING tsOutPrecision, TSTRING tsClipPrecision, 
                    TSTRING tsQuality, TSTRING tsPitch, TSTRING tsFamily, TSTRING tsFaceName)
{
    HFONT hFont = NULL;
    LOGFONT lf;
    DWORD dwSearchVal;

    lf.lfHeight = dwHeight;
    lf.lfWidth = dwWidth;
    lf.lfEscapement = lf.lfOrientation = dwEscapement;
    lf.lfItalic = bItalic;
    lf.lfUnderline = bUnderline;
    lf.lfStrikeOut = bStrikeOut;

    if(nvSearch(nvFontWeights, tsWeight, &dwSearchVal))
        lf.lfWeight = dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid weight given %s."), tsWeight.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvCharsets, tsCharSet, &dwSearchVal))
        lf.lfCharSet = (BYTE) dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid charset given %s."), tsCharSet.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvOutPrecision, tsOutPrecision, &dwSearchVal))
        lf.lfOutPrecision = (BYTE) dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid out precision given %s."), tsOutPrecision.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvClipPrecision, tsClipPrecision, &dwSearchVal))
        lf.lfClipPrecision = (BYTE) dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid clip precision given %s."), tsClipPrecision.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvQuality, tsQuality, &dwSearchVal))
        lf.lfQuality = (BYTE) dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid quality given %s."), tsQuality.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvPitch, tsPitch, &dwSearchVal))
        lf.lfPitchAndFamily = (BYTE) dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid pitch given %s."), tsPitch.c_str());
        goto ReturnError;
    }

    if(nvSearch(nvFamily, tsFamily, &dwSearchVal))
        lf.lfPitchAndFamily |= dwSearchVal;
    else
    {
        info(FAIL, TEXT("Invalid family given %s."), tsFamily.c_str());
        goto ReturnError;
    }

    _tcsncpy_s(lf.lfFaceName, tsFaceName.c_str(), _TRUNCATE);

    hFont = CreateFontIndirect(&lf);

ReturnError:
    return hFont;
}

