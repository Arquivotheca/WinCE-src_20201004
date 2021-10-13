//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include "dib.h"

HPALETTE ghPal = NULL;
HINSTANCE g_hInstance = NULL;
HWND ghwndMain;
TCHAR		gszClassName[] = TEXT("s2_dib");
int gIndex = 0;

VOID MessagePump(HWND hWnd = NULL)
{
	MSG msg;
	const MSGPUMP_LOOPCOUNT = 10;
	const MSGPUMP_LOOPDELAY = 50; // msec

	for (INT i = 0; i < MSGPUMP_LOOPCOUNT; i++)
	{
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(MSGPUMP_LOOPDELAY);
	}
}

BOOL InitApplication( HANDLE hInstance )
{
	WNDCLASS wc;

	memset( &wc, 0, sizeof(WNDCLASS) );
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)DefWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = (HINSTANCE) hInstance;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = gszClassName;

	return RegisterClass( &wc );
}

BOOL InitInstance( HANDLE hInstance, int nCmdShow )
{
	int cy = GetSystemMetrics(SM_CYSCREEN);
	if (cy < 300)
		cy = 240;

	ghwndMain = CreateWindowEx(
		0,
		gszClassName,
		gszClassName,
		WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE,
		rand()%100, 0,400, cy,
		NULL,
		NULL,
		(HINSTANCE) hInstance,
		NULL );
	if( ghwndMain == NULL )
		return FALSE;
	ShowWindow( ghwndMain, nCmdShow );
	UpdateWindow( ghwndMain );
	MessagePump();
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
							/*[in]*/ MODULE_PARAMS* pmp,
							/*[out]*/ UINT* pnThreads
							)
{
	*pnThreads = 1;
	InitializeStressUtils (
							_T("s2_dib"),	// Module name to be used in logging
							(SLOG_SPACE_GDI << 16) | SLOG_BITMAP | SLOG_DRAW,	    // Logging zones used by default
							pmp			    // Forward the Module params passed on the cmd line
							);

    int iSeed = GetTickCount();

    if (pmp->tszUser && !_stscanf(pmp->tszUser, TEXT("%d"), &iSeed))
    {
        iSeed = GetTickCount();
    }
	TCHAR tsz[MAX_PATH];

    GetModuleFileName((HINSTANCE) g_hInstance, tsz, MAX_PATH);
    tsz[MAX_PATH - 1] = 0;

	LogComment(_T("Module File Name: %s"), tsz);

    LogComment(TEXT("Using random seed %d"), iSeed);
    srand(iSeed);

    InitApplication (g_hInstance);
    InitInstance (g_hInstance, SW_SHOWNORMAL);
	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
						/*[in]*/ HANDLE hThread,
						/*[in]*/ DWORD dwThreadId,
						/*[in,out]*/ LPVOID pv /*unused*/)
{
    int width = (rand() % (600-16)) + 16;
    gIndex++;

    UINT uiRet = TestDIBSections( ghwndMain, width, awBPPOpts[rand()%7]);
    MessagePump();
    return uiRet;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    InvalidateRect(ghwndMain, NULL, TRUE);
    UpdateWindow(ghwndMain);
	MessagePump();

    DestroyWindow(ghwndMain);
	return ((DWORD) -1);
}

/* **********************************************************************
  REMARKS:
    Creates a DIBSection based on the criteria passed in as parameters.

    The DIBSection is created with a default color table - for 8bpp and
    above, this is a spectrum palette. For 4bpp, it is a stock 16 color
    table, and for 1bpp it is black and white.
********************************************************************** */
HBITMAP DSCreateDIBSection( int nX, int nY, WORD wBits )

{
    HBITMAP         hBitmap;
    LPBYTE          pBits;
    LPWORD          pword;
    LPDWORD			pdword;
    int             nInfoSize;
    LPBITMAPINFO    pbmi;
    HDC             hRefDC;
    int             iBand, iRow, i,  nWidthBytes ;
    BITMAP          bm;

    nInfoSize = sizeof( BITMAPINFOHEADER );
    if( wBits <= 8 )
    {
        nInfoSize += sizeof(RGBQUAD) * (1 << wBits);
    }

    if( ( wBits == 16 ) || ( wBits == 32 ) )
        nInfoSize += 3 * sizeof(DWORD);

	 // 24 bits: no color table

    // Create the header big enough to contain color table and bitmasks if needed
    pbmi = (LPBITMAPINFO) LocalAlloc(LPTR, nInfoSize );
    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = nX;

	 // -nY: read from the top left of the bits array
    pbmi->bmiHeader.biHeight = -nY;

    pbmi->bmiHeader.biPlanes = 1;
    pbmi->bmiHeader.biBitCount = wBits;
    pbmi->bmiHeader.biCompression = BI_RGB; // override below for 16 and 32bpp

    if( wBits <= 8 )
    {
        pbmi->bmiHeader.biClrUsed = (1 << wBits);
        pbmi->bmiHeader.biClrImportant = (1 << wBits);
    }


    switch( wBits )
	{
    case 24:// 24bpp requires no special handling
        break;

    case 16:
        {   // if it's 16bpp, fill in the masks and override the compression
            // these are the default masks - you could change them if needed
        LPDWORD pMasks = (LPDWORD)(pbmi->bmiColors);
        pMasks[0] = 0x00007c00;
        pMasks[1] = 0x000003e0;
        pMasks[2] = 0x0000001f;
        pbmi->bmiHeader.biCompression = BI_BITFIELDS;
        }
        break;
    case 32:
        {   // if it's 32bpp, fill in the masks and override the compression
            // these are the default masks - you could change them if needed
        LPDWORD pMasks = (LPDWORD)(pbmi->bmiColors);
        pMasks[0] = 0x00ff0000;
        pMasks[1] = 0x0000ff00;
        pMasks[2] = 0x000000ff;
        pbmi->bmiHeader.biCompression = BI_BITFIELDS;
        }
        break;

    case 8:
        {
        PALETTEENTRY    pe[256];
            // at this point, prgb points to the color table, even
            // if bitmasks are present
        ghPal = DSCreateSpectrumPalette();
        if (ghPal)
        {
            GetPaletteEntries( ghPal, 0, 256, pe );
            for (i=0;i<256;i++)
            {
                pbmi->bmiColors[i].rgbRed = pe[i].peRed;
                pbmi->bmiColors[i].rgbGreen = pe[i].peGreen;
                pbmi->bmiColors[i].rgbBlue = pe[i].peBlue;
                pbmi->bmiColors[i].rgbReserved = 0;
            }
        }
        }
        break;

    case 4:
        {   // Use a default 16 color table for 4bpp DIBSections
        RGBTRIPLE rgb[16] = { { 0x00, 0x00, 0x00 }, // black
                              { 0x80, 0x00, 0x00 }, // dark blue
                              { 0x00, 0x80, 0x00 }, // dark green
                              { 0x80, 0x80, 0x00 }, // dark cyan
                              { 0x00, 0x00, 0x80 }, // dark red
                              { 0x80, 0x00, 0x80 }, // dark magenta
                              { 0x00, 0x80, 0x80 }, // dark yellow
                              { 0xC0, 0xC0, 0xC0 }, // light gray
                              { 0x80, 0x80, 0x80 }, // medium gray
                              { 0xFF, 0x00, 0x00 }, // blue
                              { 0x00, 0xFF, 0x00 }, // green
                              { 0xFF, 0xFF, 0x00 }, // cyan
                              { 0x00, 0x00, 0xFF }, // red
                              { 0xFF, 0x00, 0xFF }, // magenta
                              { 0x00, 0xFF, 0xFF }, // yellow
                              { 0xFF, 0xFF, 0xFF } };// white

        for(i=0;i<16;i++)
        {
            pbmi->bmiColors[i].rgbRed = rgb[i].rgbtRed;
            pbmi->bmiColors[i].rgbGreen = rgb[i].rgbtGreen;
            pbmi->bmiColors[i].rgbBlue = rgb[i].rgbtBlue;
            pbmi->bmiColors[i].rgbReserved = 0;
        }
        }
        break;
    case 1: // BW
        pbmi->bmiColors[0].rgbRed = pbmi->bmiColors[0].rgbGreen = pbmi->bmiColors[0].rgbBlue = 0;
        pbmi->bmiColors[1].rgbRed = pbmi->bmiColors[1].rgbGreen = pbmi->bmiColors[1].rgbBlue = 255;
        pbmi->bmiColors[0].rgbReserved = pbmi->bmiColors[1].rgbReserved = 0;
        break;

    case 2: //  4 colors
        pbmi->bmiColors[0].rgbRed = pbmi->bmiColors[0].rgbGreen = pbmi->bmiColors[0].rgbBlue = 0x0;
        pbmi->bmiColors[1].rgbRed = pbmi->bmiColors[1].rgbGreen = pbmi->bmiColors[1].rgbBlue = 0x88;
        pbmi->bmiColors[2].rgbRed = pbmi->bmiColors[2].rgbGreen = pbmi->bmiColors[2].rgbBlue = 0xCC;
        pbmi->bmiColors[3].rgbRed = pbmi->bmiColors[3].rgbGreen = pbmi->bmiColors[3].rgbBlue = 0xFF;

        pbmi->bmiColors[0].rgbReserved = pbmi->bmiColors[1].rgbReserved = 0;
        pbmi->bmiColors[2].rgbReserved = pbmi->bmiColors[3].rgbReserved = 0;
        break;
    }
    hRefDC = GetDC( NULL );
    hBitmap = CreateDIBSection( hRefDC, pbmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0 );
    ReleaseDC( NULL, hRefDC );

	if( hBitmap != NULL &&  pBits != NULL )
	{
    	// DWORD alignment.
    	nWidthBytes = BYTESPERLINE(pbmi->bmiHeader.biWidth,pbmi->bmiHeader.biBitCount*pbmi->bmiHeader.biPlanes);

    	// NOTE: bm.bmWidthBytes not DOWRD alignment.
    	// if using that value,  color filled in is not correct.
    	GetObject(hBitmap, sizeof(BITMAP), &bm);
    	LogVerbose(TEXT("width=%d bits=%d: nWidthBytes=%d, bm.bmWidthBytes=%d\r\n"),
    		pbmi->bmiHeader.biWidth, wBits, nWidthBytes, bm.bmWidthBytes);

    	switch (wBits)
		{
		case 24:  // 4 bans:  Red, green, blue and black
			for (iBand=0; iBand < 4; iBand++)
				{
				for (iRow=0;  iRow < (nY/4); iRow++)
					{
					int nLimit = nWidthBytes/3;
					int nByteLeft = nWidthBytes - nLimit * 3;
					for(i=0; i < nWidthBytes/3; i++)
						{
						switch (iBand)
							{
							case 0:		   // RED
								*pBits++ = 0;
								*pBits++ = 0;
								*pBits++ = 255;
								break;
							case 1:        // Green
								*pBits++ = 0;
								*pBits++ = 255;
								*pBits++ = 0;
								break;
							case 2:        // blue
								*pBits++ = 255;
								*pBits++ = 0;
								*pBits++ = 0;
								break;
							case 3:        // black
								*pBits++ = 0;
								*pBits++ = 0;
								*pBits++ = 0;
								break;
							}
						}

					while (nByteLeft--)
						{
                        *pBits++ = 0;
						}
					}
				}
			break;
		case 1:					// two band: White and black
			for (iRow = 0; iRow < nY; iRow++)
				{
				for(i=0; i<nWidthBytes; i++)
					{
					*pBits++ = (iRow <= (nY / 2)) ? 0xFF:0;
					}
				}
			break;
		case 2:   // 4 band: white, light gray, dark gray and black
			for (iBand=0; iBand < 4; iBand++)
				{
				for (iRow=0;  iRow < (nY/4); iRow++)
					{
					for(i=0; i < nWidthBytes; i++)
						{
						switch (iBand)
							{
							case 0:  // white
								*pBits++ = 0xFF; break;	 //1111=F color table index = 3
							case 1:  // Light gray
								*pBits++ = 0xAA; break;  //1010=A  color table index = 2
							case 2:  // Dark Gray
								*pBits++ = 0x55; break;  //0101=5  color table index = 1
							case 3:  //	Black
								*pBits++ = 0; break;		 //0000=0  color table index = 0
							}
						}
               }
				}
			break;
		case 4:
			for (iBand=0; iBand < 16; iBand++)
				{
				for (iRow=0;  iRow < (nY/16); iRow++)
					{
					for(i=0; i < nWidthBytes; i++)
						{
						*pBits++ = (iBand << 4) | iBand;
						}
                    }
				}
			break;

		case 8:  // byte value is the index into the color table
			for (iRow=0;  iRow < nY; iRow++)
				{
				for (i=0; i < nWidthBytes; i++)
					{
					*pBits++ = iRow;
					}
				}
			break;

		case 16:// 2 bytes/per pixel:
		        //pbmi->bmiHeader.biCompression = BI_BITFIELDS
			pword = (PWORD) pBits;
			for (iBand=0; iBand < 4; iBand++)
				{
				for (iRow=0;  iRow < (nY/4); iRow++)
					{
					for(i=0; i < nWidthBytes / 2; i++)
						{
						switch (iBand)
							{
							case 0:  // RED:
								*pword++ = 0x7C00; break;
							case 1:  // //Green:
								*pword++ = 0x3E0; break;
							case 2:  // //Blue:
								*pword++ = 0x001F; break;
							case 3:  // //Black:
								*pword++ = 0; break;
							}
						}
                    }
				}
			break;
		case 32:// 4 bytes/per pixel:
		        //pbmi->bmiHeader.biCompression = BI_BITFIELDS
			pdword = (LPDWORD) pBits;
			for (iBand=0; iBand < 4; iBand++)
				{
				for (iRow=0;  iRow < (nY/4); iRow++)
					{
					for(i=0; i < nWidthBytes / 4; i++)
						{
						switch (iBand)
							{
							case 0:  // RED:
								*pdword++ = 0xFF0000;
								break;
							case 1:  // //Green:
								*pdword++ = 0xFF00;
								break;
							case 2:  // //Blue:
								*pdword++ = 0x00FF;
								break;
							case 3:  // //Black:
								*pdword++ = 0;
								break;
							}
						}
                    }
				}
			break;
		}
	}
    LocalFree( pbmi );
    return hBitmap;
}


/* **********************************************************************
  HPALETTE DSCreateSpectrumPalette( void )
  RETURNS:
    HPALETTE - a handle to a spectrum palette - NULL on failure

    This function will build a palette with a spectrum of colors.  It is
    useful when you want to display a number of DIBs each with a different
    palette yet still have an a good selection of colors to which the
    DIBs' colors will be mapped.
********************************************************************** */
HPALETTE WINAPI DSCreateSpectrumPalette( void )
{
    HPALETTE hPal;
    LPLOGPALETTE lplgPal;
    BYTE red, green, blue;
    int i, fGrayScale ;
    static int iCalled = 0;

    if (iCalled == 2)
    	LogVerbose(TEXT("iCalled=%d: create a palette with a spectrum of colors\r\n"), iCalled);
	else
		LogVerbose(TEXT("iCalled=%d: create a palette with gray scale\r\n"), iCalled);


    lplgPal = (LPLOGPALETTE)LocalAlloc( LPTR, sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * 256 );
    if (!lplgPal)
    {
		 LogFail(TEXT("iCalled=%d: DSCreateSpectrumPalette: LocalAlloc failed: err=%ld\r\n"), iCalled, GetLastError());
		 return NULL;
    }

    lplgPal->palVersion = 0x300;
    lplgPal->palNumEntries = 256;

    red = green = blue = 0;
    // special test case for case 1. Don't select the gray palette into screen DC
    if (iCalled == 0)
        fGrayScale  = TRUE;
    else
    {
        fGrayScale  = iCalled % 2;
    }
    for (i = 0; i < 256; i++)
    {
        lplgPal->palPalEntry[i].peRed   = red;
        lplgPal->palPalEntry[i].peGreen = green;
        lplgPal->palPalEntry[i].peBlue  = blue;

        if (fGrayScale)
		{
			red++;
			green++;
			blue++;
		}
		else
		{

			if (!(red += 32))
				 if (!(green += 32))
					  blue += 64;
		}
	}
    if ((hPal = CreatePalette(lplgPal)) == NULL)
    {
		 LogFail(TEXT("iCalled=%d: DSCreateSpectrumPalette: CreatePalette: failed: err=%ld\r\n"), iCalled, GetLastError());
    }
    LocalFree((HLOCAL)lplgPal);
    iCalled++;
    return hPal;
}



/* ******************************************************************* */
UINT TestDIBSections( HWND hWndParent, int width, WORD wBits )
{
	int     nRet, nDCBits, fSupportPalette;
	HBITMAP hBitmap, hOldBitmap;
	HDC	  hMemDC, hDC;
	TCHAR   szDebug[64];
	HPALETTE  hPalSav = NULL; // for 8 bits DIB bitmap
    static int iShow8bits = 0;	  // for 8 bits DIB bitmap
    UINT uiRet = CESTRESS_PASS;


	wsprintf(szDebug, TEXT("test=%d width=%d bits=%d"), gIndex+1, width, wBits);
	//SetWindowText(hWndParent, szDebug);

	hDC = GetDC(hWndParent);
	// Check if this DC support Palette:
	nDCBits = GetDeviceCaps(hDC,BITSPIXEL);
	nRet = GetDeviceCaps(hDC,RASTERCAPS);

	fSupportPalette =  (nRet & RC_PALETTE);
	LogVerbose(TEXT("DC = %d bits: %s palette.-----\r\n"), nDCBits,
      (LPTSTR) (fSupportPalette? TEXT("Support") : TEXT("NOT Support")));

	if (fSupportPalette  && (SelectPalette(hDC, (HPALETTE)GetStockObject(DEFAULT_PALETTE ), FALSE) == NULL))
	{
		LogWarn1(TEXT("System supports Palette: But SelectPalette() failed: err=%ld.-----\r\n"),
			GetLastError());
	    uiRet = CESTRESS_WARN1;
	}

	if (fSupportPalette   && (RealizePalette(hDC) == GDI_ERROR))
	{
		LogWarn1(TEXT("System supports Palette: But RealizePalette() failed: err=%ld.-----\r\n"),
			GetLastError());
		uiRet = CESTRESS_WARN1;
	}

	hBitmap = DSCreateDIBSection( width, width, wBits );
	if (!hBitmap)
	{
	    LogFail(TEXT("DSCreateDIBSection failed"));
	    ReleaseDC(hWndParent, hDC);
	    return CESTRESS_FAIL;
	}

	hMemDC = CreateCompatibleDC( hDC );
	if (!hMemDC)
	{
	    LogWarn1(TEXT("CreateCompatibleDC failed, GLE: %d"), GetLastError());
	    uiRet = CESTRESS_WARN1;
	}
	hOldBitmap = (HBITMAP) SelectObject( hMemDC, hBitmap );
	if (wBits == 8 && (GetForegroundWindow() == hWndParent))
	{
        //iShow8bits == 0: don't select the palette to screen DC
		if (iShow8bits > 0)
		{
			if ((hPalSav = SelectPalette(hDC, ghPal, FALSE)) == NULL)
			{
				LogFail(TEXT("nDC bits=%d: display 8bit bitmap:  SelectPalette(hDC, ghPal) failed: err=%ld\r\n"),
					nDCBits,GetLastError());
				uiRet = CESTRESS_FAIL;
			}

			if (RealizePalette(hDC) == GDI_ERROR)
			{
				LogFail(TEXT("nDC bits=%d: display 8bit bitmap:  RelizePalette(hDC) failed: err=%ld\r\n"),
					nDCBits,GetLastError());
				uiRet = CESTRESS_FAIL;
			}
			else
			{
				LogVerbose(TEXT("nDC bits=%d: display 8bit bitmap:   Select and RelizePalette(hDC) Succeeded\r\n"),
					nDCBits);
			}
		}
        iShow8bits++;
	}

    BitBlt( hDC, 10, 0, width, width, hMemDC, 0, 0, SRCCOPY );
	SelectObject( hMemDC, hOldBitmap );
	DeleteDC( hMemDC );
	DeleteObject( hBitmap );
	if (wBits == 8)
	{
		if (iShow8bits > 0)
			SelectPalette(hDC, hPalSav, FALSE);
		DeleteObject( ghPal );
	}
	ReleaseDC(hWndParent, hDC);
	CheckMemory();
	return uiRet;
}

void CheckMemory()
{
    static DWORD s_dwAvailVirtual = 0xffffffff;
    MEMORYSTATUS ms;

    ms.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&ms);
    if (ms.dwAvailVirtual < s_dwAvailVirtual)
    {
        LogComment(TEXT("Memory Usage now at %d"), ms.dwTotalVirtual - ms.dwAvailVirtual);
        s_dwAvailVirtual = ms.dwAvailVirtual;
    }
}

///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HANDLE hInstance,
					ULONG dwReason,
					LPVOID lpReserved
					)
{
    g_hInstance = (HINSTANCE) hInstance;
    return TRUE;
}
