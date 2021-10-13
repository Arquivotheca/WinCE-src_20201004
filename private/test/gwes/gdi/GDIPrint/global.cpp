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

HINSTANCE globalInst;
PrintHandler gPrintHandler;

//******************************************************************************
void
info(infoType iType, LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[1024] = {NULL};

    va_list pArgs;

    va_start(pArgs, szFormat);
    if(FAILED(StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szFormat, pArgs)))
       OutputDebugString(TEXT("StringCbVPrintf failed"));
    va_end(pArgs);

    switch (iType)
    {
        case FAIL:
            g_pKato->Log(MY_FAIL, TEXT("FAIL: %s"), szBuffer);
            break;
        case ECHO:
            g_pKato->Log(MY_PASS, szBuffer);
            break;
        case DETAIL:
            g_pKato->Log(MY_PASS + 1, TEXT("    %s"), szBuffer);
            break;
        case ABORT:
            g_pKato->Log(MY_ABORT, TEXT("Abort: %s"), szBuffer);
            break;
        case SKIP:
            g_pKato->Log(MY_SKIP, TEXT("Skip: %s"), szBuffer);
            break;
    }
}

//******************************************************************************
TESTPROCAPI getCode(void)
{

    for (int i = 0; i < 15; i++)
        if (g_pKato->GetVerbosityCount((DWORD) i) > 0)
            switch (i)
            {
                case MY_EXCEPTION:
                    return TPR_HANDLED;
                case MY_FAIL:
                    return TPR_FAIL;
                case MY_ABORT:
                    return TPR_ABORT;
                case MY_SKIP:
                    return TPR_SKIP;
                case MY_NOT_IMPLEMENTED:
                    return TPR_HANDLED;
                case MY_PASS:
                    return TPR_PASS;
                default:
                    return TPR_NOT_HANDLED;
            }
    return TPR_PASS;
}

/***********************************************************************************
***
***   Error checking macro's
***
************************************************************************************/

void
InternalCheckNotRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum)
{
    if(dwExpectedResult == dwResult)
    {
        DWORD dwError = GetLastError();
        LPVOID lpMsgBuf  = NULL;
        info(FAIL, TEXT("%s returned %d expected not to be %d, File - %s, Line - %d"), FuncText, dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwError, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("GLE returned %d - %s"), dwError, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
        SetLastError(dwError);
    }
}

void
InternalCheckForRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum)
{
    if(dwExpectedResult != dwResult)
    {
        DWORD dwError = GetLastError();
        LPVOID lpMsgBuf  = NULL;
        info(FAIL, TEXT("%s returned %d expected to be %d, File - %s, Line - %d"), FuncText, dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwError, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("GLE returned %d - %s"), dwError, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
        SetLastError(dwError);
    }
}

void
InternalCheckForLastError(DWORD dwExpectedResult, const TCHAR *File, int LineNum)
{
    DWORD dwResult = GetLastError();
    if(dwExpectedResult != dwResult)
    {
        LPVOID lpMsgBuf = NULL;
        info(FAIL, TEXT("GetLastError() returned %d expected to be %d, File - %s, Line - %d"), dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwResult, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("Last Error Value %d = %s"), dwResult, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
    }
}

//**********************************************************************************
HBITMAP myCreateDIBSection(HDC hdc, int w, int h, int nIdentifier, VOID ** ppvBits, struct MYBITMAPINFO * UserBMI, int *nUsage)
{
    HBITMAP hBmp = NULL;
    DWORD dwColors, dwCompression, d;

    // bmi is used to temporarily hold the bitmap info for CreateDIBSection.
    // If the user specifies a "UserBMI" the data is copied from this MYBITMAPINFO to it at the end.
    struct MYBITMAPINFO bmi;
    memset(&bmi, 0, sizeof(MYBITMAPINFO));

    switch(nIdentifier)
    {
        // 16 bit per pixel surfaces
        case RGB16:
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 16;
            break;
        case RGB4444:
            bmi.ct[3].rgbMask = 0xF000;
            bmi.ct[2].rgbMask = 0x000F;
            bmi.ct[1].rgbMask = 0x00F0;
            bmi.ct[0].rgbMask = 0x0F00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 16;
            break;
        case RGB565:
            bmi.ct[2].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x07E0;
            bmi.ct[0].rgbMask = 0xF800;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 16;
            break;
        case RGB555:
            bmi.ct[2].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x03E0;
            bmi.ct[0].rgbMask = 0x7C00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 16;
            break;
        case RGB1555:
            bmi.ct[3].rgbMask = 0x8000;
            bmi.ct[2].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x03E0;
            bmi.ct[0].rgbMask = 0x7C00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 16;
            break;
        case BGR4444:
            bmi.ct[3].rgbMask = 0xF000;
            bmi.ct[0].rgbMask = 0x000F;
            bmi.ct[1].rgbMask = 0x00F0;
            bmi.ct[2].rgbMask = 0x0F00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 16;
            break;
        case BGR565:
            bmi.ct[0].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x07E0;
            bmi.ct[2].rgbMask = 0xF800;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 16;
            break;
        case BGR555:
            bmi.ct[0].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x03E0;
            bmi.ct[2].rgbMask = 0x7C00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 16;
            break;
        case BGR1555:
            bmi.ct[3].rgbMask = 0x8000;
            bmi.ct[0].rgbMask = 0x001F;
            bmi.ct[1].rgbMask = 0x03E0;
            bmi.ct[2].rgbMask = 0x7C00;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 16;
            break;

        // 24 bit per pixel surface
        case RGB24:
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 24;
            break;

        // 32 bit per pixel surfaces
        case RGB32:
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 32;
            break;
        case RGB8888:
            bmi.ct[3].rgbMask = 0xFF000000;
            bmi.ct[2].rgbMask = 0x000000FF;
            bmi.ct[1].rgbMask = 0x0000FF00;
            bmi.ct[0].rgbMask = 0x00FF0000;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 32;
            break;
        case RGB888:
            bmi.ct[2].rgbMask = 0x000000FF;
            bmi.ct[1].rgbMask = 0x0000FF00;
            bmi.ct[0].rgbMask = 0x00FF0000;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 32;
            break;
        case BGR8888:
            bmi.ct[3].rgbMask = 0xFF000000;
            bmi.ct[0].rgbMask = 0x000000FF;
            bmi.ct[1].rgbMask = 0x0000FF00;
            bmi.ct[2].rgbMask = 0x00FF0000;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_ALPHABITFIELDS;
            d = 32;
            break;
        case BGR888:
            bmi.ct[0].rgbMask = 0x000000FF;
            bmi.ct[1].rgbMask = 0x0000FF00;
            bmi.ct[2].rgbMask = 0x00FF0000;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_BITFIELDS;
            d = 32;
            break;

        // paletted surfaces using the system palette
        case PAL1:
            dwColors = DIB_PAL_COLORS;
            dwCompression = BI_RGB;
            d = 1;
            break;
        case PAL2:
            dwColors = DIB_PAL_COLORS;
            dwCompression = BI_RGB;
            d = 2;
            break;
        case PAL4:
            dwColors = DIB_PAL_COLORS;
            dwCompression = BI_RGB;
            d = 4;
            break;
        case PAL8:
            dwColors = DIB_PAL_COLORS;
            dwCompression = BI_RGB;
            d = 8;
            break;

        // paletted surfaces using a user palette
        case RGB1:
            bmi.ct[0].rgbMask = 0x00000000;
            bmi.ct[1].rgbMask = 0x00FFFFFF;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 1;
            break;
        case RGB2:
            bmi.ct[0].rgbMask, 0x00000000;
            bmi.ct[1].rgbMask, 0x00555555;
            bmi.ct[2].rgbMask, 0x00AAAAAA;
            bmi.ct[3].rgbMask, 0x00FFFFFF;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 2;
            break;
        case RGB4:
            for (int i = 0; i < 16; i++)
            {
                bmi.ct[i].rgbRed = bmi.ct[i].rgbGreen = bmi.ct[i].rgbBlue = (BYTE)(i << 4);
                bmi.ct[i].rgbReserved = 0;
            }
            // make sure it has White available
            bmi.ct[15].rgbMask = 0x00FFFFFF;
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 4;
            break;
        case RGB8:
            for (int i = 0; i < 256; i++)
            {
                bmi.ct[i].rgbRed = bmi.ct[i].rgbGreen = bmi.ct[i].rgbBlue = (BYTE)i;
                bmi.ct[i].rgbReserved = 0;
            }
            dwColors = DIB_RGB_COLORS;
            dwCompression = BI_RGB;
            d = 8;
            break;
    }

    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = w;
    bmi.bmih.biHeight = h;
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = (WORD)d;
    bmi.bmih.biCompression = dwCompression;

    // create the DIB from the BITMAPINFO filled in above.
    // we assume that all creations will succeed.
    CheckNotRESULT(NULL, hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, dwColors, ppvBits, NULL, NULL));

    // if the user wants the BITMAPINFO, then copy it over from the BITMAPINFO used to create the DIB.
    if(UserBMI)
        memcpy(UserBMI, &bmi, sizeof(struct MYBITMAPINFO));

    if(nUsage)
        *nUsage = dwColors;

    return hBmp;
}

