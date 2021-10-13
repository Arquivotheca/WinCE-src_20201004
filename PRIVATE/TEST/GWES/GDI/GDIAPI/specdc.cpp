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

   specdc.cpp

Abstract:

   Gdi Tests:

***************************************************************************/

#include "global.h"
#include "hqamem.h"

int     DCFlag = EGDI_Primary;
static int windRef;

// set defaults
int     verifyFlag = EVerifyAuto;
int     errorFlag = EErrorIgnore;
DWORD   screenSize;
HINSTANCE globalInst;
float   adjustRatio = 1;
COLORREF gcrWhite;
int     gMinWinWidth = 0;
int     gMinWinHeight = 0;

/***********************************************************************************
***
***   Tux Entry Points
***
************************************************************************************/

//**********************************************************************************
TESTPROCAPI Info_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    info(ECHO, TEXT("%d"), lpFTE->dwUniqueID);
    TDC     hdc = myGetDC();

    info(ECHO, TEXT("Verification Type:%20s"), funcName[verifyFlag]);
    info(ECHO, TEXT("Extended Error Checking:%20s"), funcName[errorFlag]);
    if (hdc)
    {
        info(ECHO, TEXT("Physical Screen: %dx%d   bits per pixel:%d"), GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES),
             GetDeviceCaps(hdc, BITSPIXEL));
        myReleaseDC(hdc);
    }
    info(ECHO, TEXT("DC Type:%20s"), funcName[DCFlag]);

    return TPR_PASS;
}

/***********************************************************************************
***
***   DumpDir
***
************************************************************************************/

void
ProcessDir(LPTSTR szPath)
{
    LPTSTR  szAppend = szPath + _tcslen(szPath);

    OutputDebugString(szPath);
    OutputDebugString(TEXT("\r\n"));
    _tcscpy(szAppend, TEXT("*.*"));
    WIN32_FIND_DATA findData;
    HANDLE  hFind = FindFirstFile(szPath, &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (findData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
            {
                if (_tcscmp(findData.cFileName, TEXT(".")) && _tcscmp(findData.cFileName, TEXT("..")))
                {
                    _tcscat(_tcscpy(szAppend, findData.cFileName), TEXT("\\"));
                    ProcessDir(szPath);
                }
            }
            else
            {
                _tcscpy(szAppend, findData.cFileName);
                OutputDebugString(szPath);
                OutputDebugString(TEXT("\r\n"));
            }
        }
        while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
}

//***********************************************************************************
TESTPROCAPI DumpDir_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    info(ECHO, TEXT("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"));
    info(ECHO, TEXT("Dumping Current Directory Structure:"));

    TCHAR   szPath[1024] = TEXT("\\");

    ProcessDir(szPath);

    info(ECHO, TEXT("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"));
    return getCode();
}

/***********************************************************************************
***
***   Memory DC Support
***
************************************************************************************/

static TBITMAP stockBmp = NULL;

//***********************************************************************************
TDC doMemDCCreate(void)
{
    BOOL    bVerify;
    TDC     tempDC,
            hdc = NULL;
    TBITMAP hBmp = NULL;
    BYTE   *lpBits = NULL;
    
    tempDC = myGetDC(NULL);
    if (tempDC)
    {
        hdc = CreateCompatibleDC(tempDC);

        if (hdc)
        {
            switch(DCFlag)
            {
                case EGDI_VidMemory:
                    info(ECHO, TEXT("Using Vid Memory DC"));
                    hBmp = CreateCompatibleBitmap(tempDC, zx, zy);
                    break;
                case EGDI_SysMemory:
                    info(ECHO, TEXT("Using Sys Memory DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, GetDeviceCaps(tempDC, BITSPIXEL), NULL);
                    break;
                case EGDI_1bppBMP:
                    info(ECHO, TEXT("Using 1bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 1, NULL);
                    break;
                case EGDI_2bppBMP:
                    info(ECHO, TEXT("Using 2bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 2, NULL);
                    break;
                case EGDI_4bppBMP:
                    info(ECHO, TEXT("Using 4bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 4, NULL);
                    break;
                case EGDI_8bppBMP:
                    info(ECHO, TEXT("Using 8bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 8, NULL);
                    break;
                case EGDI_16bppBMP:
                    info(ECHO, TEXT("Using 16bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 16, NULL);
                    break;
                case EGDI_24bppBMP:
                    info(ECHO, TEXT("Using 24bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 24, NULL);
                    break;
                case EGDI_32bppBMP:
                    info(ECHO, TEXT("Using 32bpp DC"));
                    hBmp = myCreateBitmap(zx, zy, 1, 32, NULL);
                    break;
                case EGDI_1bppPalDIB:
                    info(ECHO, TEXT("Using 1bpp syspal DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 1, zx, zy, DIB_PAL_COLORS);
                    break;
               case EGDI_2bppPalDIB:
                info(ECHO, TEXT("Using 2bpp syspal DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 2, zx, zy, DIB_PAL_COLORS);
                    break;
               case EGDI_4bppPalDIB:
                    info(ECHO, TEXT("Using 4bpp syspal DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 4, zx, zy, DIB_PAL_COLORS);
                    break;
               case EGDI_8bppPalDIB:
                    info(ECHO, TEXT("Using 8bpp syspal DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 8, zx, zy, DIB_PAL_COLORS);
                    break;
                case EGDI_1bppRGBDIB:
                    info(ECHO, TEXT("Using 1bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 1, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_2bppRGBDIB:
                    info(ECHO, TEXT("Using 2bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 2, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_4bppRGBDIB:
                    info(ECHO, TEXT("Using 4bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 4, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_8bppRGBDIB:
                    info(ECHO, TEXT("Using 8bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 8, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_16bppRGBDIB:
                    info(ECHO, TEXT("Using 16bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 16, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_24bppRGBDIB:
                    info(ECHO, TEXT("Using 24bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 24, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_32bppRGBDIB:
                    info(ECHO, TEXT("Using 32bpp RGB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 32, zx, zy, DIB_RGB_COLORS);
                    break;
                case EGDI_1bppPalDIBBUB:
                    info(ECHO, TEXT("Using 1bpp syspal BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 1, zx, -zy, DIB_PAL_COLORS);
                    break;
                case EGDI_2bppPalDIBBUB:
                    info(ECHO, TEXT("Using 2bpp syspal BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 2, zx, -zy, DIB_PAL_COLORS);
                    break;
                case EGDI_4bppPalDIBBUB:
                    info(ECHO, TEXT("Using 4bpp syspal BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 4, zx, -zy, DIB_PAL_COLORS);
                    break;
                case EGDI_8bppPalDIBBUB:
                    info(ECHO, TEXT("Using 8bpp syspal BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 8, zx, -zy, DIB_PAL_COLORS);
                    break;
                case EGDI_1bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 1bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 1, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_2bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 2bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 2, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_4bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 4bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 4, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_8bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 8bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 8, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_16bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 16bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 16, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_24bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 24bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 24, zx, -zy, DIB_RGB_COLORS);
                    break;
                case EGDI_32bppRGBDIBBUB:
                    info(ECHO, TEXT("Using 32bpp RGB BUB DIB DC"));
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, 32, zx, -zy, DIB_RGB_COLORS);
                    break;
            }
            
            if (hBmp)
            {
                stockBmp = (TBITMAP) SelectObject(hdc, hBmp);
            }
            else
            {
                info(FAIL, TEXT("doMemDCCreate: CreateCompatibleBitmap Failure(%d %d)"), zx, zy);
                DeleteDC(hdc);
                hdc = NULL;
            }
        }
        else
        {
            info(FAIL, TEXT("doMemDCCreate: CreateCompatibleDC Failure(%d %d)"), zx, zy);
        }
        
        // turn off verification when releasing the tempdc, we didn't blt to it, so it will fail.
        bVerify = SetSurfVerify(FALSE);
        myReleaseDC(NULL, tempDC);
        SetSurfVerify(bVerify);
    }
    
    return hdc;
}

//***********************************************************************************
BOOL doMemDCRelease(TDC hdc)
{
    TBITMAP hBmp = (TBITMAP) SelectObject(hdc, stockBmp);

    if ((TBITMAP)GDI_ERROR != hBmp)
    {
        DeleteObject(hBmp);
    }
    
    return DeleteDC(hdc);
}

/***********************************************************************************
***
***   Window Support
***
************************************************************************************/
HWND    myHwnd = NULL;
LPCTSTR myAtom;

//***********************************************************************************
void
setGlobalClass(void)
{
    WNDCLASS wc;
    static  i;
    TCHAR  *pszStyle = TEXT("???");
    TCHAR   szBuf[6];

    _stprintf(szBuf, TEXT("%d"), i++);  // This will give us a unique ID each time

    memset(&wc, 0, sizeof (WNDCLASS));
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;   // Window Procedure
    wc.cbWndExtra = 8;
    wc.hInstance = globalInst;  // Owner of this class
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1); // Default color
    wc.lpszClassName = szBuf;
    wc.style = CS_GLOBALCLASS;
    pszStyle = TEXT("Global Class");
    
    myAtom = (LPCTSTR) RegisterClass(&wc);
    if (!myAtom)
        info(FAIL, TEXT("MakeMeAClass failed GLE:%d"), GetLastError());

    info(ECHO, TEXT("Creating HWND with Style = %s"), pszStyle);
}

//***********************************************************************************
void
pumpMessages(void)
{
    MSG     msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//***********************************************************************************
TDC doWindowCreate(void)
{
    RECT rcPosition;
    DWORD dwWinStyle;

    do
        {
        rcPosition.top = 0;
        rcPosition.left = 0;
        
        SystemParametersInfo(SPI_GETWORKAREA, 0, &grcTopMain, 0);
        
        // grcTopMain is now the screen width and height, make the window width a subset of that.
        // divided/multipled by 4 to make the window divisable by 4, for some tests
        // subtracted by width/4, increased by width/4 to make the minimum size 1/2 of the screen (dont' want to go too small).
        rcPosition.right = (((rand() % (grcTopMain.right - gMinWinWidth)) + gMinWinWidth)/4) *4;
        rcPosition.bottom = (rand() % (grcTopMain.bottom - gMinWinHeight)) + gMinWinHeight;

        // make the top left corner somewhere inside the virtual desktop
        rcPosition.left = rand()%(GetSystemMetrics(SM_CXVIRTUALSCREEN) - rcPosition.right);
        // make the left corner somewhere inside the main desktop (when you have multimon and don't want to use it)
        //rcPosition.left = rand()%(grcTopMain.right - rcPosition.right);

        rcPosition.top = rand()%(grcTopMain.bottom - rcPosition.bottom);

        // reset the width/height to compensate for a non-zero position
        // bottom right should still be within the primary due to the positioning of the top left within the
        // top left of the unused area on the screen.
        rcPosition.right += rcPosition.left;
        rcPosition.bottom += rcPosition.top;
        
        dwWinStyle = WS_POPUP | WS_VISIBLE;

        if(rand() % 2)
            dwWinStyle |= WS_BORDER;
        if(rand() % 2)
            dwWinStyle |= WS_CAPTION;
        if(rand() % 2)
            dwWinStyle |= WS_HSCROLL;
        if(rand() % 2)
            dwWinStyle |= WS_THICKFRAME;
        if(rand() % 2)
            dwWinStyle |= WS_SYSMENU;
        if(rand() % 2)
            dwWinStyle |= WS_VSCROLL;

        // this will adjust the top, left, bottom, and right to compensate for the outside of the windo, so we may have
        // a negative top left, that just means the borders and title bar will be off the edge of the screen.
        // we also may have a bottom right that's greater than the work area, that means that the bottom right
        // of the borders will be off the screen.  The actual work area should never go offscreen, that will cause test failures
        AdjustWindowRectEx(&rcPosition, dwWinStyle, FALSE, NULL);

        // if the window style is such that we end up not a multiple of 2, then retry
    }while(((rcPosition.right - rcPosition.left)/2)*2 != rcPosition.right - rcPosition.left);
    
    if (windRef++ == 0)
    {
        setGlobalClass();
        myHwnd = CreateWindow(myAtom, TEXT("GDI"), dwWinStyle, rcPosition.left, rcPosition.top, rcPosition.right - rcPosition.left, rcPosition.bottom - rcPosition.top, NULL, NULL, globalInst, NULL);
        
        if (!myHwnd)
            info(FAIL, TEXT("CreateWindow failed GLE:%d"), GetLastError());
    }
    
    // set the width and height of the rectangle for the rest of the test to scale to.
    GetClientRect(myHwnd, &grcTopMain);

    // screen has to be divisable by 2.
    grcTopMain.right = grcTopMain.right/2 * 2;
    
    SetFocus(myHwnd);
    SetForegroundWindow(myHwnd);
    ShowWindow(myHwnd, SW_SHOW);
    UpdateWindow(myHwnd);
    Sleep(100);                   // NT4.0 needs this
    pumpMessages();
    Sleep(100);                   // NT4.0 needs this
    TDC tdctmp = myGetDC(myHwnd);
    return tdctmp;
}

//***********************************************************************************
BOOL doWindowRelease(TDC oldDC)
{

    BOOL    result = myReleaseDC(myHwnd, oldDC);

    if (--windRef == 0)
    {
        DestroyWindow(myHwnd);
        UnregisterClass(myAtom, globalInst);
        myHwnd = NULL;
        myAtom = NULL;
    }
    return result;
}

//***********************************************************************************
BOOL isWindowType(void)
{

    if (DCFlag == EWin_Primary)
        return 1;

    return 0;
}

//***********************************************************************************
#ifdef UNDER_NT
TDC CreateNTPrnDC(void)
{
    BOOL bPrintDlg;             // Return code from PrintDlg function
    HDC hdc = NULL;

    static BOOL s_fInitialized = FALSE;
    static PRINTDLG s_pd;

    if (!s_fInitialized)
    {
        //  Initialize variables     hDC = NULL;
        memset (&s_pd, 0, sizeof (PRINTDLG));
        /* Initialize the PRINTDLG members. */
        s_pd.lStructSize = sizeof (PRINTDLG);
        s_pd.hwndOwner = NULL;
        // PD_RETURNDC was removed.
        s_pd.Flags = PD_PRINTSETUP;
        s_pd.hDevMode = (HANDLE) NULL;
        s_pd.hDevNames = (HANDLE) NULL;
        s_pd.nFromPage = 1;
        s_pd.nToPage = 1;
        s_pd.nMinPage = 0;
        s_pd.nMaxPage = 0;
        s_pd.nCopies = 1;
        s_pd.hInstance = NULL;

        bPrintDlg = PrintDlg (&s_pd);
        if (!bPrintDlg)             
        {
            return (NULL);
        }
    }

    // now create the printer DC
    DEVNAMES* pdn = (DEVNAMES*) GlobalLock (s_pd.hDevNames);
    DEVMODE* pdm = (DEVMODE*) GlobalLock (s_pd.hDevMode);

    hdc = CreateDC(_TEXT("winspool"), (TCHAR*)pdn + pdn->wDeviceOffset, NULL, pdm);
    if (!hdc)
    {
        info (ECHO, _TEXT("CreateNTPrnDC: CreateDC Failed, GLE = %d"), GetLastError());
    }
    else
    {
        // we have a working configuration
        s_fInitialized = TRUE;
    }

    GlobalUnlock (s_pd.hDevMode);
    GlobalUnlock (s_pd.hDevNames);
    return hdc;

}
#endif // UNDER_NT

//***********************************************************************************
TDC doPrnDCCreate(void)
{
    TDC hdcPrint = NULL;
    DOCINFO docinfo;
#ifdef UNDER_CE
    DEVMODE dm;

    // set up the DEVMODE for CreateDC
    memset (&dm, 0, sizeof (DEVMODE));
    dm.dmSize = sizeof (DEVMODE);
    dm.dmSpecVersion = 0x400;

    dm.dmFields |= DM_COPIES;
    dm.dmCopies = 1;         // CE only support 1: (short) ( (dwUserData & PRN_SELECT_PAGE_NUM) >> 24 );

    dm.dmFields |= DM_COLOR;
    dm.dmColor = DMCOLOR_COLOR;
    
    dm.dmFields |= DM_PAPERSIZE;
    dm.dmPaperSize = DMPAPER_LETTER;
    
    dm.dmFields |= DM_PRINTQUALITY;
    dm.dmPrintQuality = DMRES_DRAFT;

    dm.dmFields |= DM_ORIENTATION;
    dm.dmOrientation = DMORIENT_PORTRAIT;

    // Open up the dummy printer driver. 
    hdcPrint = TCreateDC (TEXT("PCL_test.DLL"), NULL, TEXT("\\\\invalid\\printer"), &dm);
    if (!hdcPrint)
    {
        info(FAIL, TEXT("TCreateDC failed in doPrnDCCreate, GLE:%d"), GetLastError());
    }
#else
    hdcPrint = CreateNTPrnDC ();
#endif

    // set up the physical parameters (compensating for the paper margins).
    grcTopMain.left = GetDeviceCaps (hdcPrint, PHYSICALOFFSETX);
    grcTopMain.top = GetDeviceCaps (hdcPrint, PHYSICALOFFSETY);
    grcTopMain.right = GetDeviceCaps (hdcPrint, PHYSICALWIDTH) - 2 * GetDeviceCaps (hdcPrint, PHYSICALOFFSETX);
    grcTopMain.bottom = GetDeviceCaps (hdcPrint, PHYSICALHEIGHT) - 2 * GetDeviceCaps (hdcPrint, PHYSICALOFFSETY);


    docinfo.cbSize = sizeof (docinfo);
    // Set print to file option (does nothing on CE, redirects output on NT
    docinfo.lpszOutput = TEXT("printoutput.prn");  // default to printer
    docinfo.lpszDatatype = NULL;
    docinfo.fwType = 0;

    g_iPrinterCntr = PCINIT;

    StartDoc(hdcPrint, &docinfo);
    StartPage (hdcPrint);

    return hdcPrint;
}

//**********************************************************************************
BOOL doPrnDCRelease(TDC tdcPrinter)
{
    BOOL pass = 1;
    if (tdcPrinter)
    {
        pass = pass && EndPage(tdcPrinter);
        pass = pass && EndDoc(tdcPrinter);
        pass = pass && DeleteDC(tdcPrinter);
    }
    return pass ? 1 : 0;
}

/***********************************************************************************
***
***   Exposed Functions
***
************************************************************************************/

//***********************************************************************************
void
InitSurf(void)
{
    HDC     hdc = GetDC(NULL);

    gBpp = GetDeviceCaps(hdc, BITSPIXEL);

    SetPixel(hdc, 0, 0, RGB(0xFF, 0xFF, 0xFF));
    gcrWhite = GetPixel(hdc, 0, 0);

    info(DETAIL, TEXT("Mask for Red is: 0x%x Mask for Green is: 0x%x Mask for Blue is: 0x%x "),
         GetRValue(gcrWhite), GetGValue(gcrWhite), GetBValue(gcrWhite));

    ReleaseDC(NULL, hdc);

    // set the min windows size to the default
    SetWindowConstraint(0, 0);
}

//***********************************************************************************
void
FreeSurf(void)
{
}

void
RandRotate()
{
#ifdef UNDER_CE
    DEVMODE devMode;

    if(gRotateAvail)
    {
        int nDirection = DMDO_0;
        switch(rand() % 4)
        {
            case 0:
                nDirection = DMDO_0;
                break;
            case 1:
                nDirection = DMDO_90;
                break;
            case 2:
                nDirection = DMDO_180;
                break;
            case 3:
                nDirection = DMDO_270;
                break;
        }
        memset(&devMode,0x00,sizeof(devMode));
        devMode.dmSize=sizeof(devMode);
        devMode.dmFields = DM_DISPLAYORIENTATION;
        devMode.dmDisplayOrientation = nDirection;
        ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL);
        // let the rotation take effect.
        ResetVerifyDriver();
        // reset everything to the new width and height
        SystemParametersInfo(SPI_GETWORKAREA, 0, &grcTopMain, 0);
        grcTopMain.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        MoveWindow(ghwndTopMain, grcTopMain.left, grcTopMain.top, grcTopMain.right, grcTopMain.bottom, TRUE);
    }
#endif
}

//***********************************************************************************
TDC myGetDC(void)
{
    TDC     hdc = NULL;
    TBITMAP hbmp = NULL;
    BITMAP bmp;

    static BOOL s_fFirst = TRUE;

    RandRotate();

    switch (DCFlag)
    {
        case EGDI_Primary:
            info(ECHO, TEXT("Using HWND of Top Window"));
            hdc = myGetDC(ghwndTopMain);
            break;
        case EGDI_VidMemory:
        case EGDI_SysMemory:
        case EGDI_1bppBMP:
        case EGDI_2bppBMP:
        case EGDI_4bppBMP:
        case EGDI_8bppBMP:
        case EGDI_16bppBMP:
        case EGDI_24bppBMP:
        case EGDI_32bppBMP:
        case EGDI_1bppPalDIB:
        case EGDI_2bppPalDIB:
        case EGDI_4bppPalDIB:
        case EGDI_8bppPalDIB:
        case EGDI_1bppRGBDIB:
        case EGDI_2bppRGBDIB:
        case EGDI_4bppRGBDIB:
        case EGDI_8bppRGBDIB:
        case EGDI_16bppRGBDIB:
        case EGDI_24bppRGBDIB:
        case EGDI_32bppRGBDIB:
        case EGDI_1bppPalDIBBUB:
        case EGDI_2bppPalDIBBUB:
        case EGDI_4bppPalDIBBUB:
        case EGDI_8bppPalDIBBUB:
        case EGDI_1bppRGBDIBBUB:
        case EGDI_2bppRGBDIBBUB:
        case EGDI_4bppRGBDIBBUB:
        case EGDI_8bppRGBDIBBUB:
        case EGDI_16bppRGBDIBBUB:
        case EGDI_24bppRGBDIBBUB:
        case EGDI_32bppRGBDIBBUB:
            hdc = doMemDCCreate();
            break;
        case EGDI_Default:
            info(ECHO, TEXT("Using NULL HWND"));
            hdc = myGetDC(NULL);
            break;
        case EWin_Primary:
            hdc = doWindowCreate();
            break;
        case EGDI_Printer:
            info(ECHO, TEXT("Using Printer HDC"));
            hdc = doPrnDCCreate();
            break;
    }

    if (!hdc)
        info(FAIL, TEXT("myGetDC: GetDC Failed"));

    // if the width decreases, then we'll scale down, if the height decreases, 
    // we'll scale down, etc.  we'll only scale up if both scale up.
    adjustRatio = min((float) zy / (float) 480, (float) zx / (float) 640);
    //info(ECHO, TEXT("gdit: adjustRatio = %7.3f\r\n"), adjustRatio);

    // update our bitmask for the new surface
    if(DCFlag != EGDI_Printer)
    {
        SetPixel(hdc, 0, 0, RGB(0xFF, 0xFF, 0xFF));
        gcrWhite = GetPixel(hdc, 0, 0);
    }

    // get the bit depth of the new surface if it's a bitmap, otherwise set bitdepth to the primary.
    if((hbmp = (TBITMAP) GetCurrentObject(hdc, OBJ_BITMAP)) != NULL && GetObject(hbmp, sizeof (BITMAP), (LPBYTE) & bmp))
            gBpp = bmp.bmBitsPixel;
    else gBpp = GetDeviceCaps(hdc, BITSPIXEL);
                
    // put the dc into a known state for the test
    HBRUSH hbr = CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255));
    if(hbr)
    {
        RECT rc = { 0, 0, zx, zy};
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
    }
    else
        // not enough memory to make the brush, just initialize the surface to white.
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    
#ifndef UNDER_CE
    HRGN    hRgn = CreateRectRgn(0, 0, zx, zy);

    SelectClipRgn(hdc, hRgn);
    DeleteObject(hRgn);
    GdiSetBatchLimit(1);
#endif

    if (s_fFirst)
    {
        int iRasterCaps = GetDeviceCaps (hdc, RASTERCAPS);
        int iCurveCaps = GetDeviceCaps (hdc, CURVECAPS);
        int iLineCaps = GetDeviceCaps (hdc, LINECAPS);
        int iPolygonalCaps = GetDeviceCaps (hdc, POLYGONALCAPS);
        int iTextCaps = GetDeviceCaps (hdc, TEXTCAPS);
        int iTechnology = GetDeviceCaps (hdc, TECHNOLOGY);
        info (ECHO, TEXT("Capabilities of DC: RasterCaps = 0x%08X; CurveCaps = 0x%08X; LineCaps = 0x%08X, PolygonalCaps = 0x%08X, TextCaps = 0x%08X"),
                iRasterCaps, iCurveCaps, iLineCaps, iPolygonalCaps, iTextCaps);
        info (ECHO, TEXT("DC Technology: 0x%08X"), iTechnology);
        s_fFirst = FALSE;
    }
    
    return hdc;
}

//***********************************************************************************
BOOL myReleaseDC(TDC hdc)
{
    BOOL    result = 0;

    switch (DCFlag)
    {
        case EGDI_Primary:
            result = myReleaseDC(ghwndTopMain, hdc);
            break;
        case EGDI_VidMemory:
        case EGDI_SysMemory:
        case EGDI_1bppBMP:
        case EGDI_2bppBMP:
        case EGDI_4bppBMP:
        case EGDI_8bppBMP:
        case EGDI_16bppBMP:
        case EGDI_24bppBMP:
        case EGDI_32bppBMP:
        case EGDI_1bppPalDIB:
        case EGDI_2bppPalDIB:
        case EGDI_4bppPalDIB:    
        case EGDI_8bppPalDIB:
        case EGDI_1bppRGBDIB:
        case EGDI_2bppRGBDIB:
        case EGDI_4bppRGBDIB:
        case EGDI_8bppRGBDIB:
        case EGDI_16bppRGBDIB:
        case EGDI_24bppRGBDIB:
        case EGDI_32bppRGBDIB:
        case EGDI_1bppPalDIBBUB:
        case EGDI_2bppPalDIBBUB:
        case EGDI_4bppPalDIBBUB:
        case EGDI_8bppPalDIBBUB:
        case EGDI_1bppRGBDIBBUB:
        case EGDI_2bppRGBDIBBUB:
        case EGDI_4bppRGBDIBBUB:
        case EGDI_8bppRGBDIBBUB:
        case EGDI_16bppRGBDIBBUB:
        case EGDI_24bppRGBDIBBUB:
        case EGDI_32bppRGBDIBBUB:
            result = doMemDCRelease(hdc);
            break;
        case EGDI_Default:
            result = myReleaseDC(NULL, hdc);
            break;
        case EWin_Primary:
            result = doWindowRelease(hdc);
            break;
        case EGDI_Printer:
            result = doPrnDCRelease(hdc);
            break;
    }
    return result;
}

//***********************************************************************************
BOOL isMemDC(void)
{
    // if it's not a primary, default, window, or printer, it's a memory dc.
    return (!(DCFlag == EGDI_Primary || DCFlag == EGDI_Default || DCFlag == EWin_Primary || DCFlag == EGDI_Printer));
}

// send 0,0 to set normal width and heigh constraints
BOOL SetWindowConstraint(int nx, int ny)
{
    BOOL fIsValidConstraint = TRUE;
    RECT rcWorkArea;
    
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);

    // grab the max of the min width or the default width.
    // if the min width/height is 0, then we'll always take the default width
    gMinWinWidth = max(nx,(rcWorkArea.right - rcWorkArea.left)/2);
    gMinWinHeight = max(ny,(rcWorkArea.bottom - rcWorkArea.top)/2);

    // if the requested width is too big, we can't do it, so fail the call, and set to the default width
    if(nx > (rcWorkArea.right - rcWorkArea.left) || ny > (rcWorkArea.bottom - rcWorkArea.top))
    {
       fIsValidConstraint = FALSE;
       gMinWinWidth = (rcWorkArea.right - rcWorkArea.left)/2;
       gMinWinHeight = (rcWorkArea.bottom - rcWorkArea.top)/2;
    }
    
    return fIsValidConstraint;
}
