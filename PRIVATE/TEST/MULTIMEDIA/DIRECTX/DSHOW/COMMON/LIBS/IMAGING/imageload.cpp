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
#include <winuser.h>
#include <wingdi.h>
#include <stdio.h>
#include <tchar.h>
#include <streams.h>
#include <dvdmedia.h>
#include "Imaging.h"
#include <cs.h>
#include <csmedia.h>

extern "C" { WINGDIAPI HBITMAP WINAPI CreateBitmapFromPointer( CONST BITMAPINFO *pbmi, int iStride, PVOID pvBits); }

LoaderTableEntry LoaderTable[] = {
    {MEDIASUBTYPE_YUY2, LoadYUY2Image},
    {MEDIASUBTYPE_YVYU, LoadYVYUImage},
    {MEDIASUBTYPE_YV12, LoadYV12Image},
    {MEDIASUBTYPE_YV16, LoadYV16Image},
    {MEDIASUBTYPE_UYVY, LoadUYVYImage},
    {MEDIASUBTYPE_RGB24, LoadRGB24Image},
    {MEDIASUBTYPE_RGB32, LoadRGB32Image},
    {MEDIASUBTYPE_RGB555, LoadRGB555Image},
    {MEDIASUBTYPE_RGB565, LoadRGB565Image},
};

const int nLoaderTable = sizeof(LoaderTable)/sizeof(LoaderTable[0]);

ImageLoader GetLoader(BITMAPINFO* pBmi, GUID subtype)
{
    ImageLoader loader = NULL;
    for(int i = 0; i < nLoaderTable; i++)
    {
        if (LoaderTable[i].subtype == subtype)
        {
            loader = LoaderTable[i].loader;
            break;
        }
    }

    return loader;
}

// This method sets the bits in pBuffer with the format specified in bmi. 
// Unless an image file is specified, the bits are set to random values.
HRESULT LoadImage(BYTE* pBuffer, BITMAPINFO* bmi, GUID subtype, const TCHAR* imagefile)
{
    ImageLoader loader = GetLoader(bmi, subtype);
    return (loader) ? loader(pBuffer, bmi, imagefile) : E_NOTIMPL;
}

int AllocateBitmapBuffer(int width, int height, WORD bitcount, DWORD compression, BYTE** ppBuffer)
{
    HRESULT hr = NOERROR;
    int size = 0;

    switch(compression)
    {
    case FOURCC_YUY2:
    case FOURCC_YVYU:
    case FOURCC_UYVY:
        size = width*height*2;
        break;
    
    case FOURCC_YV12:
        size = width*height + 2*(width/2)*(height/2);
        break;

    case FOURCC_YV16:
        size = width*height + 2*(width/2)*height;
        break;

    case BI_RGB:
    case BI_BITFIELDS:
        switch(bitcount)
        {
        case 32:
            size = width*height*4;
            break;
        case 24:
            size = width*height*3;
            break;
        case 16:
            size = width*height*2;
            break;
        default:
            break;
        }
        break;

    default:
        size = 0;
        break;
    }
    
    if (size && ppBuffer)
        *ppBuffer = new BYTE[size];

    return size;
}

BITMAPINFO* GetBitmapInfo(const AM_MEDIA_TYPE *pMediaType)
{
    BITMAPINFO* pBmi = NULL;
    if (!pMediaType  ||  !pMediaType->pbFormat)
        return NULL;
    if ((pMediaType->formattype == FORMAT_VideoInfo) &&
        (pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER)))
    {
        pBmi = (BITMAPINFO*)&(((VIDEOINFOHEADER*)(pMediaType->pbFormat))->bmiHeader);
    }
    else if ((pMediaType->formattype == FORMAT_VideoInfo2) &&
        (pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER2)))       
    {
        pBmi = (BITMAPINFO*)&(((VIDEOINFOHEADER2*)(pMediaType->pbFormat))->bmiHeader);
    }
    return pBmi;
}

HRESULT LoadYVYUImage(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    BYTE *pRGB24Image = NULL;
    BITMAPINFO rgb24bmi;
    memset(&rgb24bmi, 0, sizeof(BITMAPINFO));
    rgb24bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    rgb24bmi.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
    rgb24bmi.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
    rgb24bmi.bmiHeader.biPlanes = 1;
    rgb24bmi.bmiHeader.biBitCount = 24;
    rgb24bmi.bmiHeader.biCompression = BI_RGB;
    rgb24bmi.bmiHeader.biSizeImage = bmi->bmiHeader.biWidth * bmi->bmiHeader.biHeight;

    // Allocate a 24-bit image to load the imagefile into
    AllocateBitmapBuffer(bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 24, BI_RGB, &pRGB24Image);
    if (!pRGB24Image)
        return E_OUTOFMEMORY;

    // Load into allocated image
    hr = LoadRGB24Image(pRGB24Image, bmi, imagefile);
    if (FAILED(hr))
    {
        delete pRGB24Image;
        return S_FALSE;
    }

    // Convert the 24 bit image to a YVYU image    
    hr = RGB24_To_YVYU(pRGB24Image, &rgb24bmi, 0, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,  pBuffer, bmi, 0, 0, 0);

    if (pRGB24Image)
        delete pRGB24Image;

    return hr;
}

HRESULT LoadYV12Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    BYTE *pRGB24Image = NULL;
    BITMAPINFO rgb24bmi;
    memset(&rgb24bmi, 0, sizeof(BITMAPINFO));
    rgb24bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    rgb24bmi.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
    rgb24bmi.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
    rgb24bmi.bmiHeader.biPlanes = 1;
    rgb24bmi.bmiHeader.biBitCount = 24;
    rgb24bmi.bmiHeader.biCompression = BI_RGB;
    rgb24bmi.bmiHeader.biSizeImage = bmi->bmiHeader.biWidth * bmi->bmiHeader.biHeight;

    // Allocate memory for a 24 bit bitmap
    AllocateBitmapBuffer(bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 24, BI_RGB, &pRGB24Image);
    if (!pRGB24Image)
        return E_OUTOFMEMORY;

    // Load from the specified bitmap into a 24-bit bitmap
    hr = LoadRGB24Image(pRGB24Image, bmi, imagefile);
    if (FAILED(hr))
    {
        delete pRGB24Image;
        return S_FALSE;
    }

    // Convert to a packed YV12 image
    hr = RGB24_To_YV12(pRGB24Image, &rgb24bmi, 0, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,  pBuffer, bmi, 0, 0, 0);

    if (pRGB24Image)
        delete pRGB24Image;

    return hr;
}

HRESULT LoadYV16Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    BYTE *pRGB24Image = NULL;
    BITMAPINFO rgb24bmi;
    memset(&rgb24bmi, 0, sizeof(BITMAPINFO));
    rgb24bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    rgb24bmi.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
    rgb24bmi.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
    rgb24bmi.bmiHeader.biPlanes = 1;
    rgb24bmi.bmiHeader.biBitCount = 24;
    rgb24bmi.bmiHeader.biCompression = BI_RGB;
    rgb24bmi.bmiHeader.biSizeImage = bmi->bmiHeader.biWidth * bmi->bmiHeader.biHeight;

    // Allocate memory for a 24 bit bitmap
    AllocateBitmapBuffer(bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 24, BI_RGB, &pRGB24Image);
    if (!pRGB24Image)
        return E_OUTOFMEMORY;

    // Load from the specified bitmap into a 24-bit bitmap
    hr = LoadRGB24Image(pRGB24Image, bmi, imagefile);
    if (FAILED(hr))
    {
        delete pRGB24Image;
        return S_FALSE;
    }

    // Convert to a packed YV16 image
    hr = RGB24_To_YV16(pRGB24Image, &rgb24bmi, 0, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,  pBuffer, bmi, 0, 0, 0);

    if (pRGB24Image)
        delete pRGB24Image;

    return hr;
}

HRESULT LoadYUY2Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    // YUY2 same as YUYV same as YUV422 packed
    HRESULT hr = NOERROR;
    BYTE *pRGB24Image = NULL;
    BITMAPINFO rgb24bmi;
    memset(&rgb24bmi, 0, sizeof(BITMAPINFO));
    rgb24bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    rgb24bmi.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
    rgb24bmi.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
    rgb24bmi.bmiHeader.biPlanes = 1;
    rgb24bmi.bmiHeader.biBitCount = 24;
    rgb24bmi.bmiHeader.biCompression = BI_RGB;
    rgb24bmi.bmiHeader.biSizeImage = 0;

    // Allocate memory for a 24 bit bitmap
    AllocateBitmapBuffer(bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 24, BI_RGB, &pRGB24Image);
    if (!pRGB24Image)
        return E_OUTOFMEMORY;

    // Load from the specified bitmap into a 24-bit bitmap
    hr = LoadRGB24Image(pRGB24Image, bmi, imagefile);
    if (FAILED(hr))
    {
        delete pRGB24Image;
        return S_FALSE;
    }

    // Convert to a packed YUYV image    
    hr = RGB24_To_YUY2(pRGB24Image, &rgb24bmi, 0, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,  pBuffer, bmi, 0, 0, 0);

    if (pRGB24Image)
        delete pRGB24Image;

    return hr;
}

HRESULT LoadUYVYImage(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    BYTE *pRGB24Image = NULL;
    BITMAPINFO rgb24bmi;
    memset(&rgb24bmi, 0, sizeof(BITMAPINFO));
    rgb24bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    rgb24bmi.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
    rgb24bmi.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
    rgb24bmi.bmiHeader.biPlanes = 1;
    rgb24bmi.bmiHeader.biBitCount = 24;
    rgb24bmi.bmiHeader.biCompression = BI_RGB;
    rgb24bmi.bmiHeader.biSizeImage = bmi->bmiHeader.biWidth * bmi->bmiHeader.biHeight;

    // Allocate a 24-bit image to load the imagefile into
    AllocateBitmapBuffer(bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, 24, BI_RGB, &pRGB24Image);
    if (!pRGB24Image)
        return E_OUTOFMEMORY;

    // Load into allocated image
    hr = LoadRGB24Image(pRGB24Image, bmi, imagefile);
    if (FAILED(hr))
    {
        delete pRGB24Image;
        return S_FALSE;
    }

    // Convert the 24 bit image to a YVYU image    
    hr = RGB24_To_UYVY(pRGB24Image, &rgb24bmi, 0, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,  pBuffer, bmi, 0, 0, 0);

    if (pRGB24Image)
        delete pRGB24Image;

    return hr;
}

HRESULT LoadRGB555Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    if (imagefile)
    {
        BITMAPINFO targetBm;
        memset(&targetBm, 0x00, sizeof(targetBm));
        targetBm.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        targetBm.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
        targetBm.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
        targetBm.bmiHeader.biPlanes = 1;
        targetBm.bmiHeader.biBitCount = 16;
        targetBm.bmiHeader.biCompression = BI_RGB;
        targetBm.bmiHeader.biSizeImage = 0; // May be set to 0 for BI_RGB images
        hr = LoadBitmapFile(pBuffer, &targetBm, imagefile);
    }
    else {
        // Fill in gradient values
        BYTE* ptr = pBuffer;
        DWORD tick = GetTickCount();
        for(int i = 0; i < bmi->bmiHeader.biHeight; i++)
        {
            for(int j = 0; j < bmi->bmiHeader.biWidth; j++)
            {
                ptr = pBuffer + i*bmi->bmiHeader.biWidth*2 + j*2;
                unsigned char val = (unsigned char)(tick%32);
                *(WORD*)ptr = (val << 10) | (val << 5) | val;
            }
        }
    }
    return hr;
}

HRESULT LoadRGB565Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    if (imagefile)
    {
        BITMAPINFO * ptargetBm;

        ptargetBm = (BITMAPINFO *) new BYTE[sizeof(BITMAPINFO) + 8];

        memset(ptargetBm, 0x00, sizeof(BITMAPINFO) + 8);
        ptargetBm->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        ptargetBm->bmiHeader.biWidth = bmi->bmiHeader.biWidth;
        ptargetBm->bmiHeader.biHeight = bmi->bmiHeader.biHeight;
        ptargetBm->bmiHeader.biPlanes = 1;
        ptargetBm->bmiHeader.biBitCount = 16;
        ptargetBm->bmiHeader.biCompression = BI_BITFIELDS;
        ptargetBm->bmiHeader.biSizeImage = bmi->bmiHeader.biWidth * bmi->bmiHeader.biHeight * 2;
        *((DWORD *) (&ptargetBm->bmiColors[0])) = 0xF800;
        *((DWORD *) (&ptargetBm->bmiColors[1])) = 0x07E0;
        *((DWORD *) (&ptargetBm->bmiColors[2])) = 0x001F;
        hr = LoadBitmapFile(pBuffer, ptargetBm, imagefile);

        delete[] ptargetBm;
    }
    else {
        // Fill in gradient values
        BYTE* ptr = pBuffer;
        DWORD tick = GetTickCount()%32;
        for(int i = 0; i < bmi->bmiHeader.biHeight; i++)
        {
            for(int j = 0; j < bmi->bmiHeader.biWidth; j++)
            {
                ptr = pBuffer + i*bmi->bmiHeader.biWidth*2 + j*2;
                unsigned char val = (unsigned char)(tick%32);
                *(WORD*)ptr = (val << 10) | (val << 5) | val;
            }
        }
    }
    return hr;
}

HRESULT LoadRGB24Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    if (imagefile)
    {
        BITMAPINFO targetBm;
        memset(&targetBm, 0x00, sizeof(targetBm));
        targetBm.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        targetBm.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
        targetBm.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
        targetBm.bmiHeader.biPlanes = 1;
        targetBm.bmiHeader.biBitCount = 24;
        targetBm.bmiHeader.biCompression = BI_RGB;
        targetBm.bmiHeader.biSizeImage = 0; // May be set to 0 for BI_RGB images
        hr = LoadBitmapFile(pBuffer, &targetBm, imagefile);
    }
    else {
        // Fill in gradient values
        BYTE* ptr = pBuffer;
        DWORD tick = GetTickCount();
        for(int i = 0; i < bmi->bmiHeader.biHeight; i++)
        {
            for(int j = 0; j < bmi->bmiHeader.biWidth; j++)
            {
                ptr = pBuffer + i*bmi->bmiHeader.biWidth*3 + j*3;
                unsigned char val = (unsigned char)(tick%32);
                *ptr = val;
                *(ptr + 1) = val;
                *(ptr + 2) = val;
            }
        }
    }
    return hr;
}

HRESULT LoadRGB32Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    if (imagefile)
    {
        BITMAPINFO targetBm;
        memset(&targetBm, 0x00, sizeof(targetBm));
        targetBm.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        targetBm.bmiHeader.biWidth = bmi->bmiHeader.biWidth;
        targetBm.bmiHeader.biHeight = bmi->bmiHeader.biHeight;
        targetBm.bmiHeader.biPlanes = 1;
        targetBm.bmiHeader.biBitCount = 32;
        targetBm.bmiHeader.biCompression = BI_RGB;
        targetBm.bmiHeader.biSizeImage = 0; // May be set to 0 for BI_RGB images
        hr = LoadBitmapFile(pBuffer, &targetBm, imagefile);
    }
    else {
        // Fill in gradient values
        BYTE* ptr = pBuffer;
        DWORD tick = GetTickCount();
        for(int i = 0; i < bmi->bmiHeader.biHeight; i++)
        {
            for(int j = 0; j < bmi->bmiHeader.biWidth; j++)
            {
                ptr = pBuffer + i*bmi->bmiHeader.biWidth*4 + j*4;
                unsigned char val = (unsigned char)(tick%32);
                *ptr = val;
                *(ptr + 1) = val;
                *(ptr + 2) = val;
            }
        }
    }
    return hr;
}

HRESULT GetBitmapFileInfo(TCHAR* imagefile, BITMAP* pBitmap)
{
    HBITMAP hLoadedBmp = 0;

#ifdef UNDER_CE
    hLoadedBmp = SHLoadDIBitmap(imagefile);    
#else
    hLoadedBmp = (HBITMAP)LoadImage(NULL, imagefile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
#endif

    if (!hLoadedBmp)
        return E_FAIL;

    int size  = GetObject(hLoadedBmp, sizeof(BITMAP), pBitmap);

    DeleteObject(hLoadedBmp);

    return (size > 0) ? S_OK : E_FAIL;
}

HRESULT GetBitmapInfo(HBITMAP hBitmap, BITMAP* pBitmap)
{
    if (!hBitmap)
        return E_FAIL;

    int size  = GetObject(hBitmap, sizeof(BITMAP), pBitmap);

    return (size > 0) ? S_OK : E_FAIL;
}

HRESULT BltBitmapFile(BITMAPINFO *pTargetBm, int x, int y, const TCHAR* imagefile)
{
    return S_OK;
}

// The BITMAPINFO structure need not match the imagefile's format. 
// If it is not the same, the BitBlt will take care of the conversion.
HRESULT LoadBitmapFile(BYTE* pBuffer, BITMAPINFO *pTargetBm, const TCHAR* imagefile)
{
    HRESULT hr = NOERROR;
    HBITMAP hLoadedBmp = NULL;
    HBITMAP hCreatedBMP = NULL;
    HDC hdcCreatedBMP = NULL;
    HDC hdcLoadedBmp = NULL;
    UINT biWidthBytes = CS_DIBWIDTHBYTES (pTargetBm->bmiHeader);

    if (!imagefile)
        return E_FAIL;

#ifdef UNDER_CE
    if(NULL == (hLoadedBmp = SHLoadDIBitmap(imagefile)))
    {
        hLoadedBmp = LoadBitmap(globalInst, imagefile);
    }
#else
    hLoadedBmp = (HBITMAP)LoadImage(NULL, imagefile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
#endif

    if (!hLoadedBmp)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    hdcCreatedBMP = CreateCompatibleDC(NULL);
    if (!hdcCreatedBMP)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    hdcLoadedBmp = CreateCompatibleDC(NULL);
    if (!hdcLoadedBmp)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    hCreatedBMP = CreateBitmapFromPointer(pTargetBm, biWidthBytes, (PVOID) pBuffer);
    if (!hCreatedBMP)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // Blit original bitmap into one dc and the DIB into another
    if (!SelectObject(hdcCreatedBMP, hCreatedBMP))
    {
        hr = E_FAIL;
        goto cleanup;
    }

    if (!SelectObject(hdcLoadedBmp, hLoadedBmp))
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // clear out our surface in case the loaded bitmap is smaller than our dest
    if(!PatBlt(hdcCreatedBMP, 0, 0, pTargetBm->bmiHeader.biWidth, pTargetBm->bmiHeader.biHeight, WHITENESS))
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // copy as much as possible from the loaded bitmap into the created bitmap, if the loaded bitmap is larger GDI will clip
    if(!BitBlt(hdcCreatedBMP, 0, 0, pTargetBm->bmiHeader.biWidth, pTargetBm->bmiHeader.biHeight, hdcLoadedBmp, 0, 0, SRCCOPY))
    {
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:
    if (hdcCreatedBMP)
        DeleteDC(hdcCreatedBMP);
    if (hdcLoadedBmp)
        DeleteDC(hdcLoadedBmp);
    if (hCreatedBMP)
        DeleteObject(hCreatedBMP);
    if (hLoadedBmp)
        DeleteObject(hLoadedBmp);
    return hr;
}

bool IsImageFile(const TCHAR* file)
{
    if (!file)
        return false;
    // If BMP extension return true else false
    TCHAR* ext = (TCHAR*)_tcsrchr(file, TEXT('.'));

    // if we found an extension check it. If no extension was found
    // then we'll assume this is the name of a resource to load by LoadBitmap
    if(ext)
    {
        ext++;
        // include null character in search
        if (!_tcsncicmp(TEXT("bmp"), ext, 4))
            return true;
        return false;
    }
    else return true;
}

// BUGBUG: use a table?
bool IsYUVImage(GUID subtype)
{
    if ((subtype == MEDIASUBTYPE_YUY2) ||
        (subtype == MEDIASUBTYPE_YVYU) ||
        (subtype == MEDIASUBTYPE_YV12) ||
        (subtype == MEDIASUBTYPE_YV16))
        return true;
    return false;
}

// BUGBUG: use a table?
bool IsRGBImage(GUID subtype)
{
    if ((subtype == MEDIASUBTYPE_RGB24) ||
        (subtype == MEDIASUBTYPE_RGB555) ||
        (subtype == MEDIASUBTYPE_RGB565) ||
        (subtype == MEDIASUBTYPE_RGB32))
        return true;
    return false;
}
