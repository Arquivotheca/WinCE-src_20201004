#include <windows.h>

//
// SaveSurfaceToBMP
//
//   Stores the pixel data from the specified device context, in a 24BPP bitmap.
//
//   This function does not pad for bitmap sizes that don't result in a LONG-aligned
//   row.  Thus, BMP width will be "rounded down" to nearest LONG-aligned width.
//    
//   If any operation fails during this process, an error is indicated via
//   this function's return value.
//
// Arguments:
//
//   HDC:  Device context to save
//   TCHAR*:  Filename to save BMP to
//   RECT*:  Portion of device context to capture
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT SaveSurfaceToBMP(HDC hDCSource, TCHAR *pszName, RECT *prExtents)
{
    HBITMAP hBmpStock = NULL;
    HBITMAP hDIB = NULL;
    HRESULT hr = S_OK;

    int zx, zy;


    // Compute dimensions, rounding if necessary
    zx = (prExtents->right - prExtents->left) & ~3;
    zy = prExtents->bottom - prExtents->top;

    // Create a memory DC
    HDC hdc = CreateCompatibleDC(hDCSource);
    if (NULL == hdc)
    {
        OutputDebugString(_T("CreateCompatibleDC failed."));
        hr = E_FAIL;
        goto cleanup;
    }

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
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + dwImageDataSize;

    // Create the DIB Section
    LPBYTE lpbBits = NULL;

    hDIB = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (VOID**)&lpbBits, NULL, 0);
    if (!(hDIB && lpbBits))
    {
        OutputDebugString(_T("CreateDIBSection failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    // Select the DIB into the memory DC
    hBmpStock = (HBITMAP)SelectObject(hdc, hDIB);

    //
    // Copy from the surface DC to the bitmap DC (Zero indicates failure)
    //
    if (0 == BitBlt(hdc, 0, 0, prExtents->right-prExtents->left, prExtents->bottom-prExtents->top, hDCSource, prExtents->left, prExtents->top, SRCCOPY))
    {
        OutputDebugString(_T("BitBlt failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    // Save the file
    HANDLE file = CreateFile(pszName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == file)
    {
        OutputDebugString(_T("CreateFile failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    DWORD cbWritten;

    WriteFile(file, (BYTE*)&bfh, sizeof(bfh), &cbWritten, NULL);
    WriteFile(file, (BYTE*)&bmi.bmiHeader, sizeof(BITMAPINFOHEADER), &cbWritten, NULL);
    WriteFile(file, (BYTE*)lpbBits, dwImageDataSize, &cbWritten, NULL);

cleanup:

    // Remove our DIB section from our DC by putting the stock bitmap back.
    if (hdc && hBmpStock)
        SelectObject(hdc, hBmpStock);

    // Free our DIB Section
    if (hDIB)
        DeleteObject(hDIB);

    // Free our DIB DC
    if (hdc)
        DeleteDC(hdc);

    // Close the file handle
    if (file)
        CloseHandle(file);

    return hr;
}



//
// SaveSurfaceToBMP
//
//   Stores the pixel data from the specified device context, in a 24BPP bitmap.
//
//   This function does not pad for bitmap sizes that don't result in a LONG-aligned
//   row.  Thus, BMP width will be "rounded down" to nearest LONG-aligned width.
//    
//   If any operation fails during this process, an error is indicated via
//   this function's return value.
//
// Arguments:
//
//   HDC:  Device context to save
//   RECT*:  Portion of device context to capture
//   TCHAR*:  Filename to save BMP to
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT SaveSurfaceToBMP(HDC hDC, TCHAR *pszName)
{
    RECT rExtents;

    rExtents.top = 0;
    rExtents.left = 0;
    rExtents.right = GetDeviceCaps(hDC, HORZRES);
    rExtents.bottom = GetDeviceCaps(hDC, VERTRES);

    return SaveSurfaceToBMP(hDC, pszName, &rExtents);
}


//
// SaveSurfaceToBMP
//
//   Stores the pixel data from the specified window, in a 24BPP bitmap.
//
//   This function does not pad for bitmap sizes that don't result in a LONG-aligned
//   row.  Thus, BMP width will be "rounded down" to nearest LONG-aligned width.
//    
//   If any operation fails during this process, an error is indicated via
//   this function's return value.
//
// Arguments:
//
//   HWND:  Window to save
//   TCHAR*:  Filename to save BMP to
//   RECT *pRect:  Capture region (NULL if full region)
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT SaveSurfaceToBMP(HWND hWnd, TCHAR *pszName, RECT *pRect)
{
    HRESULT hr;
    HDC hDC = NULL;
    RECT rExtents;

    hDC = GetDC(hWnd);
    if (NULL == hDC)
    {
        OutputDebugString(_T("Failed at GetDC."));
        hr = E_FAIL;
        goto cleanup;
    }

	if (NULL == pRect)
	{

		if (0 == GetClientRect( hWnd,       // HWND hWnd, 
								&rExtents)) // LPRECT lpRect 
		{
			OutputDebugString(_T("Failed at GetClientRect."));
			hr = E_FAIL;
			goto cleanup;
		}

		hr = SaveSurfaceToBMP(hDC, pszName, &rExtents);
	}
	else
	{
		hr = SaveSurfaceToBMP(hDC, pszName, pRect);
	}

cleanup:

    // Release source DC
    if (hDC)
        ReleaseDC(hWnd, hDC);

    return hr;

}


//
// GetPixel
//
//   Simple wrapper for GDI GetPixel; seperates channels from packed DWORD, returns
//   any channels of interest via pointer arguments.
//
// Arguments:
//
//   HDC:  Device context to inspect
//   int iXPos:  X Position to inspect
//   int iYPos:  Y Position to inspect
//   BYTE *pRed:   If non-NULL, store red byte at indicated location.
//   BYTE *pGreen: If non-NULL, store green byte at indicated location.
//   BYTE *pBlue:  If non-NULL, store blue byte at indicated location.
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT GetPixel(HDC hDC, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue)
{
    COLORREF ColorRef;
    
    ColorRef = GetPixel(hDC,    // HDC hdc, 
                        iXPos,  // int nXPos, 
                        iYPos); // int nYPos

    //
    // Test for GetPixel failure
    //
    if (CLR_INVALID == ColorRef)
        return E_FAIL;

    //
    // Extract requested components
    //
    if (pRed)
        *pRed = GetRValue(ColorRef);

    if (pGreen)
        *pGreen = GetGValue(ColorRef);
        
    if (pBlue)
        *pBlue = GetBValue(ColorRef);
    
    return S_OK;
}


//
// GetPixel
//
//   Simple wrapper for GDI GetPixel; seperates channels from packed DWORD, returns
//   any channels of interest via pointer arguments.
//
// Arguments:
//
//   HWND:  Window to inspect
//   int iXPos:  X Position to inspect
//   int iYPos:  Y Position to inspect
//   BYTE *pRed:   If non-NULL, store red byte at indicated location.
//   BYTE *pGreen: If non-NULL, store green byte at indicated location.
//   BYTE *pBlue:  If non-NULL, store blue byte at indicated location.
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT GetPixel(HWND hWnd, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue)
{
    HRESULT hr;
    HDC hDC = NULL;
    RECT rExtents;

    hDC = GetDC(NULL);
    if (NULL == hDC)
    {
        OutputDebugString(_T("Failed at GetDC."));
        hr = E_FAIL;
        goto cleanup;
    }

    if (0 == GetWindowRect( hWnd,       // HWND hWnd, 
                            &rExtents)) // LPRECT lpRect 
    {
        OutputDebugString(_T("Failed at GetWindowRect."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = GetPixel(hDC, iXPos + rExtents.left, iYPos + rExtents.top, pRed, pGreen, pBlue);

cleanup:

    // Release source DC
    if (hDC)
        ReleaseDC(hWnd, hDC);

    return hr;
}


//
// GetPixel
//
//   Simple wrapper for GDI GetPixel; seperates channels from packed DWORD, returns
//   any channels of interest via pointer arguments.
//
// Arguments:
//
//   int iXPos:  X Position to inspect
//   int iYPos:  Y Position to inspect
//   BYTE *pRed:   If non-NULL, store red byte at indicated location.
//   BYTE *pGreen: If non-NULL, store green byte at indicated location.
//   BYTE *pBlue:  If non-NULL, store blue byte at indicated location.
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT GetPixel(int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue)
{
    HRESULT hr;
    HDC hDC = NULL;

    hDC = GetDC(NULL);
    if (NULL == hDC)
    {
        OutputDebugString(_T("Failed at GetDC."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = GetPixel(hDC, iXPos, iYPos, pRed, pGreen, pBlue);

cleanup:

    // Release source DC
    if (hDC)
        ReleaseDC(NULL, hDC);

    return hr;
}
