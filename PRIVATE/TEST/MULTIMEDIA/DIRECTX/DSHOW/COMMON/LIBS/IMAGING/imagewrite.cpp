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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <imaging.h>

HRESULT SaveBitmap(TCHAR *tszFileName, BITMAPINFO* pBmi, GUID MediaType, BYTE* pBuffer)
{
    BITMAPINFO bmi;
    BYTE *pRGBBits = NULL;
    HRESULT hr = E_OUTOFMEMORY;

    memset(&bmi, 0, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pBmi->bmiHeader.biWidth;
    bmi.bmiHeader.biHeight = pBmi->bmiHeader.biHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biXPelsPerMeter = bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = bmi.bmiHeader.biClrImportant = 0;
    bmi.bmiHeader.biSizeImage = bmi.bmiHeader.biWidth * abs(bmi.bmiHeader.biHeight);

    int alloc2 = AllocateBitmapBuffer(bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, bmi.bmiHeader.biBitCount, bmi.bmiHeader.biCompression, &pRGBBits);

    // if the allocation size was non-zero and the allocation succeeded
    if(alloc2 && pRGBBits)
    {
        hr = ConvertImage(pBuffer, pBmi, MediaType, 0, 0, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, 
                                        pRGBBits, &bmi, MEDIASUBTYPE_RGB24, 0, 0);

        if(SUCCEEDED(hr))
            hr = SaveRGBBitmap(tszFileName, pRGBBits, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, bmi.bmiHeader.biWidth * 3);

        delete[] pRGBBits;
    }

    return hr;
}

BOOL RecursiveCreateDirectory(TCHAR * tszPath)
{
    if (!CreateDirectory(tszPath, NULL) && 
        (GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND))
    {
        TCHAR * pBack;
        pBack = wcsrchr(tszPath, TEXT('\\'));
        if (pBack)
        {
            *pBack = 0;
            RecursiveCreateDirectory(tszPath);
            *pBack = TEXT('\\');
        }
        else
            return false;
    }
    if (CreateDirectory(tszPath, NULL) 
        || (GetLastError() == ERROR_FILE_EXISTS
            || GetLastError() == ERROR_ALREADY_EXISTS)
        )
    {
        return true;
    }
    return false;
}

HRESULT SaveRGBBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes)
{
    DWORD dwBmpSize;
    BITMAPFILEHEADER bmfh;
    DWORD dwBytesWritten;
    BITMAPINFO bmi24;
    HANDLE hBmpFile = NULL;
    HRESULT hr;
    TCHAR tszBitmapCopy[MAX_PATH + 1];
    TCHAR * pBack;

    //
    // Argument validation
    //
    if (NULL == tszFileName)
    {
        return E_POINTER;
    }

    if (NULL == pPixels)
    {
        return E_POINTER;
    }

    if (0 >= iWidth)
    {
        return E_INVALIDARG;
    }

    if (0 == iHeight) 
    {
        return E_INVALIDARG;
    }

    if (0 >= iWidthBytes) 
    {
        return E_INVALIDARG;
    }

    // if iHeight is negative, then it's a top down dib.  Save it as it is.
    
    memset(&bmi24, 0x00, sizeof(bmi24));
    
    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    bmi24.bmiHeader.biHeight = iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;
    
    memset(&bmfh, 0x00, sizeof(BITMAPFILEHEADER));
    bmfh.bfType = 'M' << 8 | 'B';
    bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmi24);

    //created the directory to save the file if it doesn't exist
    wcsncpy(tszBitmapCopy, tszFileName, MAX_PATH + 1);
    tszBitmapCopy[MAX_PATH] = 0;
    pBack = wcsrchr(tszBitmapCopy, TEXT('\\'));
    if (pBack)
    {
        *pBack = 0;
        RecursiveCreateDirectory(tszBitmapCopy);
        *pBack = TEXT('\\');
    }

    hBmpFile = CreateFile(
        tszFileName, 
        GENERIC_WRITE, 
        0, 
        NULL, 
        CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, 
        NULL);
    if (INVALID_HANDLE_VALUE == hBmpFile)
        goto LPError;
    
    bmi24.bmiHeader.biSizeImage = dwBmpSize = iWidthBytes * abs(iHeight);
    bmfh.bfSize = bmfh.bfOffBits + dwBmpSize;
    
    if (!WriteFile(
            hBmpFile,
            (void*)&bmfh,
            sizeof(bmfh),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)&bmi24,
            sizeof(bmi24),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)pPixels,
            dwBmpSize,
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    
    CloseHandle(hBmpFile);
    return S_OK;
LPError:
    hr = HRESULT_FROM_WIN32(GetLastError());

    if (hBmpFile)
        CloseHandle(hBmpFile);
    return hr;
}



