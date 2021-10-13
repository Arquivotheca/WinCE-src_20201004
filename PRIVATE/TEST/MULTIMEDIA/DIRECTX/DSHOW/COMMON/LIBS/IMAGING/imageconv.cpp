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
#include "Imaging.h"

// Intermediate conversions through RGB24
DECLARE_INTERMEDIATE_RGB24_CONVERT(YVYU,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YVYU,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YVYU,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(UYVY,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(UYVY,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(UYVY,RGB32);
#ifndef UNDER_CE
DECLARE_INTERMEDIATE_RGB24_CONVERT(AYUV,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(AYUV,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(AYUV,RGB32);
#endif
DECLARE_INTERMEDIATE_RGB24_CONVERT(YUY2,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YUY2,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YUY2,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV12,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV12,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV12,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV16,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV16,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(YV16,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,YV12);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,YV16);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,YVYU);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,UYVY);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,YUY2);
#ifndef UNDER_CE
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB32,AYUV);
#endif
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,YV12);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,YV16);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,YVYU);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,YUY2);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,UYVY);
#ifndef UNDER_CE
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB565,AYUV);
#endif
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,RGB32);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,RGB565);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,RGB555);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,YV12);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,YV16);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,YVYU);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,UYVY);
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,YUY2);
#ifndef UNDER_CE
DECLARE_INTERMEDIATE_RGB24_CONVERT(RGB555,AYUV);
#endif

ConverterTableEntry ConverterTable[] = {
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_RGB32,     YUY2_To_RGB32, RGB32_To_YUY2},
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_RGB24,     YUY2_To_RGB24, RGB24_To_YUY2},
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_RGB555, YUY2_To_RGB555, RGB555_To_YUY2},
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_RGB565, YUY2_To_RGB565, RGB565_To_YUY2},
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_YV12, YUY2_To_YV12, NULL},
    {MEDIASUBTYPE_YUY2, MEDIASUBTYPE_YUY2, YUY2_To_YUY2, NULL},

    {MEDIASUBTYPE_YVYU, MEDIASUBTYPE_RGB32,        YVYU_To_RGB32, RGB32_To_YVYU},
    {MEDIASUBTYPE_YVYU, MEDIASUBTYPE_RGB24,        YVYU_To_RGB24, RGB24_To_YVYU},
    {MEDIASUBTYPE_YVYU, MEDIASUBTYPE_RGB555,    YVYU_To_RGB555, RGB555_To_YVYU},
    {MEDIASUBTYPE_YVYU, MEDIASUBTYPE_RGB565,    YVYU_To_RGB565, RGB565_To_YVYU},

    {MEDIASUBTYPE_UYVY, MEDIASUBTYPE_RGB32,        UYVY_To_RGB32, RGB32_To_UYVY},
    {MEDIASUBTYPE_UYVY, MEDIASUBTYPE_RGB24,        UYVY_To_RGB24, RGB24_To_UYVY},
    {MEDIASUBTYPE_UYVY, MEDIASUBTYPE_RGB555,    UYVY_To_RGB555, RGB555_To_UYVY},
    {MEDIASUBTYPE_UYVY, MEDIASUBTYPE_RGB565,    UYVY_To_RGB565, RGB565_To_UYVY},
    {MEDIASUBTYPE_UYVY, MEDIASUBTYPE_UYVY, UYVY_To_UYVY, NULL},

    {MEDIASUBTYPE_YV12, MEDIASUBTYPE_RGB32,        YV12_To_RGB32, RGB32_To_YV12},
    {MEDIASUBTYPE_YV12, MEDIASUBTYPE_RGB24,        YV12_To_RGB24, RGB24_To_YV12},
    {MEDIASUBTYPE_YV12, MEDIASUBTYPE_RGB555,    YV12_To_RGB555, RGB555_To_YV12},
    {MEDIASUBTYPE_YV12, MEDIASUBTYPE_RGB565,    YV12_To_RGB565, RGB565_To_YV12},
    {MEDIASUBTYPE_YV12, MEDIASUBTYPE_YV12,        YV12_To_YV12, YV12_To_YV12},

    {MEDIASUBTYPE_YV16, MEDIASUBTYPE_RGB32,        YV16_To_RGB32, RGB32_To_YV16},
    {MEDIASUBTYPE_YV16, MEDIASUBTYPE_RGB24,        YV16_To_RGB24, RGB24_To_YV16},
    {MEDIASUBTYPE_YV16, MEDIASUBTYPE_RGB555,    YV16_To_RGB555, RGB555_To_YV16},
    {MEDIASUBTYPE_YV16, MEDIASUBTYPE_RGB565,    YV16_To_RGB565, RGB565_To_YV16},
    {MEDIASUBTYPE_YV16, MEDIASUBTYPE_YV16,        YV16_To_YV16, YV16_To_YV16},

#ifndef UNDER_CE
    {MEDIASUBTYPE_AYUV, MEDIASUBTYPE_RGB32,        AYUV_To_RGB32, RGB32_To_AYUV},
    {MEDIASUBTYPE_AYUV, MEDIASUBTYPE_RGB24,        AYUV_To_RGB24, RGB24_To_AYUV},
    {MEDIASUBTYPE_AYUV, MEDIASUBTYPE_RGB555,    AYUV_To_RGB555, RGB555_To_AYUV},
    {MEDIASUBTYPE_AYUV, MEDIASUBTYPE_RGB565,    AYUV_To_RGB565, RGB565_To_AYUV},
#endif

    {MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB24,    RGB555_To_RGB24, RGB24_To_RGB555},
    {MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB565,    RGB555_To_RGB565, RGB565_To_RGB555},
    {MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB555,    RGB555_To_RGB555, RGB555_To_RGB555},
    {MEDIASUBTYPE_RGB555, MEDIASUBTYPE_RGB32,    RGB555_To_RGB32, RGB32_To_RGB555},
    
    {MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB32,    RGB24_To_RGB32, RGB32_To_RGB24},
    {MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB24,    RGB24_To_RGB24, RGB24_To_RGB24},
    {MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB555,    RGB24_To_RGB555, RGB555_To_RGB24},
    {MEDIASUBTYPE_RGB24, MEDIASUBTYPE_RGB565,    RGB24_To_RGB565, RGB565_To_RGB24},
    
    {MEDIASUBTYPE_RGB565, MEDIASUBTYPE_RGB24,    RGB565_To_RGB24, RGB24_To_RGB565},
    {MEDIASUBTYPE_RGB565, MEDIASUBTYPE_RGB32,    RGB565_To_RGB32, RGB32_To_RGB565},
    {MEDIASUBTYPE_RGB565, MEDIASUBTYPE_RGB565,    RGB565_To_RGB565, RGB565_To_RGB565},

#ifndef UNDER_CE
    {MEDIASUBTYPE_RGB32, MEDIASUBTYPE_ARGB32,    RGB32_To_ARGB32, NULL},
#endif
    {MEDIASUBTYPE_RGB32, MEDIASUBTYPE_RGB32,    RGB32_To_RGB32, RGB32_To_RGB32}
};

const int nConverterTable = sizeof(ConverterTable)/sizeof(ConverterTable[0]);

ImageConverter GetConverter(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmi, GUID dstsubtype)
{
    ImageConverter converter = NULL;
    for(int i = 0; i < nConverterTable; i++)
    {
        if ((ConverterTable[i].subtype1 == srcsubtype) &&
            (ConverterTable[i].subtype2 == dstsubtype))
        {
            converter = ConverterTable[i].converter1;
            break;
        }
        if ((ConverterTable[i].subtype2 == srcsubtype) &&
            (ConverterTable[i].subtype1 == dstsubtype))
        {
            converter = ConverterTable[i].converter2;
            break;
        }
    }

    return converter;
}

bool CanConvertImage(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmi, GUID dstsubtype)
{
    return GetConverter(pSrcBmi, srcsubtype, pDstBmi, dstsubtype) ? true : false;
}

HRESULT ConvertImage(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int convWidth, int convHeight,
                     BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pSrcBmi || !pDstBits || !pDstBmi)
        return E_POINTER;

    ImageConverter converter = GetConverter(pSrcBmi, srcsubtype, pDstBmi, dstsubtype);
    return (converter) ? converter(pSrcBits, pSrcBmi, 0, srcX, srcY, convWidth, convHeight, pDstBits, pDstBmi, 0, dstX, dstY) : E_NOTIMPL;
}


HRESULT ConvertImage(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int srcStride, int convWidth, int convHeight,
                     BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY, int dstStride)
{
    // Basic pointer checking
    if (!pSrcBits || !pSrcBmi || !pDstBits || !pDstBmi)
        return E_POINTER;

    ImageConverter converter = GetConverter(pSrcBmi, srcsubtype, pDstBmi, dstsubtype);
    return (converter) ? converter(pSrcBits, pSrcBmi, srcStride, srcX, srcY, convWidth, convHeight, pDstBits, pDstBmi, dstStride, dstX, dstY) : E_NOTIMPL;
}



// From here on are conversion routines


// RGB24_To_XXX
HRESULT RGB24_To_YUY2(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r0, g0, b0, r1, g1, b1;
    float y0, u0, v0, y1, u1, v1;
    unsigned char* ptr = pSrcBits;
    unsigned char* dstptr = pDstBits;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth * 2;

    pDstBits = pDstBits + dstY*dstStride + dstX * 2;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            ptr = pSrcBits + i*srcStride + j*3;

            b0 = (float)*ptr;
            g0 = (float)*(ptr + 1);
            r0 = (float)*(ptr + 2);
            ptr += 3;
            b1 = (float)*ptr;
            g1 = (float)*(ptr + 1);
            r1 = (float)*(ptr + 2);

            RGB2YUV(r0, g0, b0, y0, u0, v0);
            RGB2YUV(r1, g1, b1, y1, u1, v1);

            // Clamp the floats
            CLAMP(y0, 0, 255);
            CLAMP(u0, 0, 255);
            CLAMP(v0, 0, 255);
            CLAMP(y1, 0, 255);
            CLAMP(u1, 0, 255);
            CLAMP(v1, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*2);
            *dstptr = (unsigned char)y0;
            *(dstptr + 1) = (unsigned char)u0;
            *(dstptr + 2) = (unsigned char)y1;
            *(dstptr + 3) = (unsigned char)v0;
        }
    }

    return NOERROR;
}

HRESULT RGB24_To_YVYU(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r0, g0, b0, r1, g1, b1;
    float y0, u0, v0, y1, u1, v1;

    unsigned char* ptr = pSrcBits;
    unsigned char* dstptr = pDstBits;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth * 2;

    pDstBits = pDstBits + dstY*dstStride + dstX * 2;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            ptr = pSrcBits + i*srcStride + j*3;

            b0 = (float)*ptr;
            g0 = (float)*(ptr + 1);
            r0 = (float)*(ptr + 2);
            ptr += 3;
            b1 = (float)*ptr;
            g1 = (float)*(ptr + 1);
            r1 = (float)*(ptr + 2);

            RGB2YUV(r0, g0, b0, y0, u0, v0);
            RGB2YUV(r1, g1, b1, y1, u1, v1);

            // Clamp the floats
            CLAMP(y0, 0, 255);
            CLAMP(u0, 0, 255);
            CLAMP(v0, 0, 255);
            CLAMP(y1, 0, 255);
            CLAMP(u1, 0, 255);
            CLAMP(v1, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*2);
            *dstptr = (unsigned char)y0;
            *(dstptr + 1) = (unsigned char)v0;
            *(dstptr + 2) = (unsigned char)y1;
            *(dstptr + 3) = (unsigned char)u0;
        }
    }

    return NOERROR;
}

HRESULT RGB24_To_UYVY(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r0, g0, b0, r1, g1, b1;
    float y0, u0, v0, y1, u1, v1;

    unsigned char* ptr = pSrcBits;
    unsigned char* dstptr = pDstBits;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth * 2;

    pDstBits = pDstBits + dstY*dstStride + dstX * 2;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            ptr = pSrcBits + i*srcStride + j*3;

            b0 = (float)*ptr;
            g0 = (float)*(ptr + 1);
            r0 = (float)*(ptr + 2);
            ptr += 3;
            b1 = (float)*ptr;
            g1 = (float)*(ptr + 1);
            r1 = (float)*(ptr + 2);

            RGB2YUV(r0, g0, b0, y0, u0, v0);
            RGB2YUV(r1, g1, b1, y1, u1, v1);

            // Clamp the floats
            CLAMP(y0, 0, 255);
            CLAMP(u0, 0, 255);
            CLAMP(v0, 0, 255);
            CLAMP(y1, 0, 255);
            CLAMP(u1, 0, 255);
            CLAMP(v1, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*2);
            *dstptr = (unsigned char)u0;
            *(dstptr + 1) = (unsigned char)y0;
            *(dstptr + 2) = (unsigned char)v0;
            *(dstptr + 3) = (unsigned char)y1;
        }
    }

    return NOERROR;
}

HRESULT RGB24_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r0, g0, b0, r1, g1, b1;
    float y0, u0, v0, y1, u1, v1;

    unsigned char* ptr;
    unsigned char* ydstptr;
    unsigned char* udstptr;
    unsigned char* vdstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth;

    int uvStride = dstStride /2;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            ptr = pSrcBits + i*srcStride + j*3;

            b0 = (float)*ptr;
            g0 = (float)*(ptr + 1);
            r0 = (float)*(ptr + 2);
            ptr += 3;
            b1 = (float)*ptr;
            g1 = (float)*(ptr + 1);
            r1 = (float)*(ptr + 2);

            RGB2YUV(r0, g0, b0, y0, u0, v0);
            RGB2YUV(r1, g1, b1, y1, u1, v1);

            // Clamp the floats
            CLAMP(y0, 0, 255);
            CLAMP(u0, 0, 255);
            CLAMP(v0, 0, 255);
            CLAMP(y1, 0, 255);
            CLAMP(u1, 0, 255);
            CLAMP(v1, 0, 255);

            ydstptr = (pDstBits + (i + dstY)*dstStride + (j + dstX));
            vdstptr = (pDstBits + dstHeight*dstStride + ((i + dstY)/2)*uvStride + (j + dstX)/2);
            udstptr = (pDstBits + dstHeight*dstStride+ (dstHeight/2)*uvStride + ((i + dstY)/2)*uvStride + (j + dstX)/2);
            *ydstptr = (unsigned char)y0;
            *(ydstptr + 1) = (unsigned char)y1;
            *udstptr = (unsigned char)u0;
            *vdstptr = (unsigned char)v0;
        }
    }

    return NOERROR;
}

HRESULT RGB24_To_YV16(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r0, g0, b0, r1, g1, b1;
    float y0, u0, v0, y1, u1, v1;

    unsigned char* ptr;
    unsigned char* ydstptr;
    unsigned char* udstptr;
    unsigned char* vdstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth;

    int uvStride = dstStride /2;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            ptr = pSrcBits + i*srcStride + j*3;

            b0 = (float)*ptr;
            g0 = (float)*(ptr + 1);
            r0 = (float)*(ptr + 2);
            ptr += 3;
            b1 = (float)*ptr;
            g1 = (float)*(ptr + 1);
            r1 = (float)*(ptr + 2);

            RGB2YUV(r0, g0, b0, y0, u0, v0);
            RGB2YUV(r1, g1, b1, y1, u1, v1);

            // Clamp the floats
            CLAMP(y0, 0, 255);
            CLAMP(u0, 0, 255);
            CLAMP(v0, 0, 255);
            CLAMP(y1, 0, 255);
            CLAMP(u1, 0, 255);
            CLAMP(v1, 0, 255);

            ydstptr = (pDstBits + (i + dstY)*dstStride + (j + dstX));
            vdstptr = (pDstBits + dstHeight*dstStride + (i + dstY)*uvStride + (j + dstX)/2);
            udstptr = (pDstBits + dstHeight*dstStride+ dstHeight*uvStride + (i + dstY)*uvStride + (j + dstX)/2);
            *ydstptr = (unsigned char)y0;
            *(ydstptr + 1) = (unsigned char)y1;
            *udstptr = (unsigned char)u0;
            *vdstptr = (unsigned char)v0;
        }
    }

    return NOERROR;
}

HRESULT RGB24_To_AYUV(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y, u, v;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    if(dstStride == 0)
        dstStride = dstWidth * 4;

    pDstBits = pDstBits + dstY*dstStride + dstX * 4;

    // for FourCC formats, the sign of the height is ignored.
    dstHeight = abs(dstHeight);

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*3);

            b = (float)*ptr;
            g = (float)*(ptr + 1);
            r = (float)*(ptr + 2);

            RGB2YUV(r, g, b, y, u, v);
            CLAMP(y, 0, 255);
            CLAMP(u, 0, 255);
            CLAMP(v, 0, 255);

            ptr = (pDstBits + i*dstStride + j*4);
            *ptr = 0;
            *(ptr + 1) = (unsigned char)y;
            *(ptr + 2) = (unsigned char)u;
            *(ptr + 3) = (unsigned char)v;
        }
    }
    return S_OK;
}

HRESULT RGB24_To_RGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*4;


    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*3);
            b = *ptr;
            g = *(ptr + 1);
            r = *(ptr + 2);

            ptr = (pDstBits + i*dstStride + j*4);
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
            *(ptr + 3) = 0;
        }
    }
    return S_OK;
}

HRESULT RGB24_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = pSrcBits + i*srcStride + j*3;
            b = *ptr;
            g = *(ptr + 1);
            r = *(ptr + 2);

            ptr = pDstBits + i*dstStride + j*3;
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
        }
    }
    return S_OK;
}

HRESULT RGB24_To_RGB565(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    WORD r, g, b;
    BYTE* pRGB24;
    WORD* pRGB565;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

// BUGBUG: RGB 565 should be BI_BITFIELDS, however no bitmask is passed in the bmi.
//    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;
    
    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*2;

    for(int i = 0; i < convHeight; i++)
    {
        pRGB24 = pSrcBits + i*srcStride;
        pRGB565 = (WORD*)(pDstBits + (i + dstY)*dstStride);

        for(int j = 0; j < convWidth; j++)
        {

            b = *pRGB24 >> 3;
            g = *(pRGB24 + 1) >> 2;
            r = *(pRGB24 + 2) >> 3;

            *pRGB565 = (r << 11) | (g << 5) | b;

            pRGB24 += 3;
            pRGB565++;
        }
    }
    return S_OK;
}

HRESULT RGB24_To_RGB555(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    WORD r, g, b;
    BYTE* pRGB24;
    WORD* pRGB555;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

// BUGBUG: RGB 555 could be BI_BITFIELDS, however no bitmask is passed in the bmi.
//    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*3;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*2;

    for(int i = 0; i < convHeight; i++)
    {
        pRGB24 = pSrcBits + i*srcStride;
        pRGB555 = (WORD*)(pDstBits + i*dstStride);

        for(int j = 0; j < convWidth; j++)
        {
            b = *pRGB24 >> 3;
            g = *(pRGB24 + 1) >> 3;
            r = *(pRGB24 + 2) >> 3;

            *pRGB555 = (r << 10) | (g << 5) | b;

            pRGB24 += 3;
            pRGB555++;
        }
    }
    return S_OK;
}

HRESULT RGB32_To_RGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*4;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*4;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*4);

            b = *ptr;
            g = *(ptr + 1);
            r = *(ptr + 2);

            ptr = (pDstBits + i*dstStride + j*4);
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
            *(ptr + 3) = 0;
        }
    }
    return S_OK;
}

HRESULT RGB32_To_ARGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*4;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*4;
    
    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*4);
            b = *ptr;
            g = *(ptr + 1);
            r = *(ptr + 2);

            ptr = (pDstBits + i*dstStride + j*4);
            *ptr = 255;
            *(ptr + 1) = r;
            *(ptr + 2) = g;
            *(ptr + 3) = b;
        }
    }
    return S_OK;
}

HRESULT YUY2_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    // Copy [Y0, pSrcBits]; [Y1, pSrcBits + 2]; [Y2, pSrcBits + 4]; [Y3, pSrcBits + 6] and so on to 
    //      [Y0, pDstBits]; [Y1, pDstBits + 1]; [Y2, pDstBits + 2]; [Y3, pDstBits + 3] etc
    // Copy [u0, pSrcBits + 1]; [u1, pSrcBits + 5]; [u2, pSrcBits + 9]; [u3, pSrcBits + 13] and so on to
    // pDstBits = pDstBits + dstHeight*dstHeight
    //      [u0, pDstBits]; [u1, pDstBits + 1]; [u2, pDstBits + 3] and so on
    // Copy [v0, pSrcBits + 3]; [v1, pSrcBits + 7]; [v2, pSrcBits + 11]; [v3, pSrcBits + 15] and so on to
    // pDstBits = pDstBits + dstHeight*dstWeight + (dstHeight*dstWeight)/4
    //      [v0, pDstBits]; [v1, pDstBits + 1]; [v2, pDstBits + 3] and so on

    // for FourCC formats, the direction of the height is ignored
    srcHeight = abs(srcHeight);
    dstHeight = abs(dstHeight);

    unsigned char y0, u0, v0, y1;

    unsigned char *ptr;
    int block = dstWidth*dstHeight;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;
    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth;
    int dstuvStride = dstStride / 2;

    // Adjust the src pointer
    // BUGBUG: srcX and srcY need to be even
    pSrcBits = pSrcBits + srcY*srcWidth*2 + srcX*2;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 bytes at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has Y0U0Y1V1Y2U2 etc, so two bytes for each pixel group in a row. Take two pixels at a time.
            // 2*srcWidth is the stride of the image; to get to 
            // YUY2 is sub-sampled in the horizontal direction but not in the vertical direction
            ptr = (pSrcBits + i*srcStride + j*2);
            y0 = *ptr;
            u0 = *(ptr + 1);
            y1 = *(ptr + 2);
            v0 = *(ptr + 3);

            // Convert to planar format
            // First block of pixels is y; next quarter-block is v; next quarter-block is u
            *(pDstBits + (i + dstY)*dstStride + (j + dstX)) = y0;
            *(pDstBits + (i + dstY)*dstStride + (j + 1 + dstX)) = y1;

            *(pDstBits + block + ((i + dstY)/2)*dstuvStride + (j + dstX)/2) = v0;
            *(pDstBits + block + ((i + dstY)/2)*dstuvStride + (j + 1 + dstX)/2) = v0;

            *(pDstBits + block + block/4 + ((i + dstY)/2)*dstuvStride + (j + dstX)/2) = u0;
            *(pDstBits + block + block/4 + ((i + dstY)/2)*dstuvStride + (j + 1 + dstX)/2) = u0;
        }
    }
    return S_OK;
}

// XXX_To_RGB24
HRESULT YUY2_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y0, u0, v0, y1;

    unsigned char *ptr;
    unsigned char *dstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every sencond pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has Y0U0Y1V1Y2U2 etc, so two bytes for each pixel group in a row. Take two pixels at a time.
            ptr = (pSrcBits + i*srcStride + j*2);
            y0 = (float)*ptr;
            u0 = (float)*(ptr + 1);
            y1 = (float)*(ptr + 2);
            v0 = (float)*(ptr + 3);

            // Convert 1st pixel
            YUV2RGB(y0, u0, v0, r, g, b);
    
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*3);
            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;

            // Convert 2nd pixel
            YUV2RGB(y1, u0, v0, r, g, b);
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);
            dstptr += 3;

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT YVYU_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y0, u0, v0, y1;

    unsigned char *ptr;
    unsigned char *dstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every sencond pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has Y0U0Y1V1Y2U2 etc, so two bytes for each pixel group in a row. Take two pixels at a time.
            ptr = (pSrcBits + i*srcStride + j*2);
            y0 = (float)*ptr;
            v0 = (float)*(ptr + 1);
            y1 = (float)*(ptr + 2);
            u0 = (float)*(ptr + 3);

            // Convert 1st pixel
            YUV2RGB(y0, u0, v0, r, g, b);
    
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*3);

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;

            // Convert 2nd pixel
            YUV2RGB(y1, u0, v0, r, g, b);
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr += 3;

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT UYVY_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y0, u0, v0, y1;

    unsigned char *ptr;
    unsigned char *dstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every second pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has U0Y0V1Y1U2Y2 etc. so that means you have to access two pixels at a time, with four bytes per two pixels.
            // so addressing has to be done as though it's two bytes per pixel.
            ptr = (pSrcBits + i*srcStride + j*2);
            u0 = (float)*ptr;
            y0 = (float)*(ptr + 1);
            v0 = (float)*(ptr + 2);
            y1 = (float)*(ptr + 3);

            // Convert 1st pixel
            YUV2RGB(y0, u0, v0, r, g, b);
    
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr = (pDstBits + i*dstStride + j*3);

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;

            // Convert 2nd pixel
            YUV2RGB(y1, u0, v0, r, g, b);
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr += 3;

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT AYUV_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float y, u, v;
    float r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 4;

    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*4);
            y = (float)*(ptr + 1);
            u = (float)*(ptr + 2);
            v = (float)*(ptr + 3);

            YUV2RGB(y, u, v, r, g, b);
    
            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            ptr = (pDstBits + i*dstStride + j*3);
            *ptr = (unsigned char)b;
            *(ptr + 1) = (unsigned char)g;
            *(ptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT YV12_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y0, u0, v0;

    unsigned char *yptr;
    unsigned char *uptr;
    unsigned char *vptr;
    unsigned char* dstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth;
    int uvsrcStride = srcStride/2;

    BYTE * yptrStart = pSrcBits;
    BYTE * vptrStart = yptrStart + srcHeight*srcStride;
    BYTE * uptrStart = vptrStart + (srcHeight/2)*uvsrcStride;
    BYTE *pRGBBits;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pRGBBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // YV12 is planar
            // First the srcWidth*srcHeight Y plane followed by (srcWidth/2)*(srcHeight/2) u plane and then the v plane
            yptr = yptrStart + (srcY + i)*srcStride + (srcX + j);
            vptr = vptrStart + ((srcY + i)/2)*uvsrcStride + (j/2);
            uptr = uptrStart + ((srcY + i)/2)*uvsrcStride + (j/2);

            y0 = (float)*yptr;
            v0 = (float)*vptr;
            u0 = (float)*uptr;

            // Convert 1st pixel
            YUV2RGB(y0, u0, v0, r, g, b);

            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr = pRGBBits + i*dstStride + j*3;

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT YV16_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    float r, g, b;
    float y0, u0, v0;

    unsigned char *yptr;
    unsigned char *uptr;
    unsigned char *vptr;
    unsigned char* dstptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // ignore the orientation of the height for fourcc formats
    srcHeight = abs(srcHeight);

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth;
    int uvsrcStride = srcStride/2;

    BYTE * yptrStart = pSrcBits;
    BYTE * uptrStart = yptrStart + srcHeight*srcStride;
    BYTE * vptrStart = uptrStart + srcHeight*uvsrcStride;
    BYTE *pRGBBits;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pRGBBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // YV16 is planar
            // First the srcWidth*srcHeight Y plane followed by (srcWidth/2)*(srcHeight/2) u plane and then the v plane
            yptr = yptrStart + (srcY + i)*srcStride + (srcX + j);
            vptr = uptrStart + ((srcY + i))*uvsrcStride + (j/2);
            uptr = vptrStart + ((srcY + i))*uvsrcStride + (j/2);

            y0 = (float)*yptr;
            u0 = (float)*uptr;
            v0 = (float)*vptr;

            // Convert 1st pixel
            YUV2RGB(y0, u0, v0, r, g, b);

            CLAMP(r, 0, 255);
            CLAMP(g, 0, 255);
            CLAMP(b, 0, 255);

            dstptr = pRGBBits + i*dstStride + j*3;

            *dstptr = (unsigned char)b;
            *(dstptr + 1) = (unsigned char)g;
            *(dstptr + 2) = (unsigned char)r;
        }
    }
    return S_OK;
}

HRESULT YV12_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    unsigned char* yptr, *uptr, *vptr;
    unsigned char* dstyptr, *dstuptr, *dstvptr;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth;
    int uvsrcStride = srcStride/2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth;
    int uvdstStride = dstStride/2;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // YV12 is planar
            // First the srcWidth*srcHeight Y plane followed by (srcWidth/2)*(srcHeight/2) u plane and then the v plane
            yptr = pSrcBits + (srcY + i)*srcStride + (srcX + j);
            vptr = pSrcBits + srcHeight*srcStride +                          (i + srcY)/2*uvsrcStride + (srcX + j)/2;
            uptr = pSrcBits + srcHeight*srcStride + srcHeight/2*uvsrcStride + (i + srcY)/2*uvsrcStride + (srcX + j)/2;

            dstyptr = pDstBits + (i + dstY)*dstStride + (j + dstX);
            dstvptr = pDstBits + dstHeight*dstStride + (i + dstY)/2*uvdstStride + (j + dstX)/2;
            dstuptr = pDstBits + dstHeight*dstStride + dstHeight/2*uvdstStride + (i + dstY)/2*uvdstStride + (j + dstX)/2;

            *dstyptr = *yptr;
            *dstuptr = *uptr;
            *dstvptr = *vptr;
        }
    }
    return S_OK;
}

HRESULT YV16_To_YV16(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    unsigned char* yptr, *uptr, *vptr;
    unsigned char* dstyptr, *dstuptr, *dstvptr;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth;
    int uvsrcStride = srcStride/2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth;
    int uvdstStride = dstStride/2;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // YV16 is planar
            // First the srcWidth*srcHeight Y plane followed by (srcWidth/2)*(srcHeight) u plane and then the v plane
            yptr = pSrcBits + (srcY + i)*srcStride + (srcX + j);
            vptr = pSrcBits + srcHeight*srcStride +                          (i + srcY)*uvsrcStride + (srcX + j)/2;
            uptr = pSrcBits + srcHeight*srcStride + srcHeight*uvsrcStride + (i + srcY)*uvsrcStride + (srcX + j)/2;

            dstyptr = pDstBits + (i + dstY)*dstStride + (j + dstX);
            dstvptr = pDstBits + dstHeight*dstStride + (i + dstY)*uvdstStride + (j + dstX)/2;
            dstuptr = pDstBits + dstHeight*dstStride + dstHeight*uvdstStride + (i + dstY)*uvdstStride + (j + dstX)/2;

            *dstyptr = *yptr;
            *dstuptr = *uptr;
            *dstvptr = *vptr;
        }
    }
    return S_OK;
}

HRESULT YUY2_To_YUY2(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    unsigned char *ptr;
    unsigned char *dstptr;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 2;

    // Adjust the src pointer
    // BUGBUG: srcX and srcY need to be even
    pSrcBits = pSrcBits + srcY*srcWidth*2 + srcX*2;
    pDstBits = pDstBits + dstY*dstWidth*2 + dstX*2;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every sencond pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has Y0U0Y1V1Y2U2 etc, so two bytes for each pixel group in a row. Take two pixels at a time.
            ptr = (pSrcBits + i*srcStride + j*2);
            dstptr = (pDstBits + i*dstStride + j*2);
            *dstptr = *ptr;
            *(dstptr + 1) = *(ptr + 1);
            *(dstptr + 2) = *(ptr + 2);
            *(dstptr + 3) = *(ptr + 3);
        }
    }    return S_OK;
}

HRESULT UYVY_To_UYVY(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    unsigned char *ptr;
    unsigned char *dstptr;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 2;

    // Adjust the src pointer
    // BUGBUG: srcX and srcY need to be even
    pSrcBits = pSrcBits + srcY*srcWidth*2 + srcX*2;
    pDstBits = pDstBits + dstY*dstWidth*2 + dstX*2;

    for(int i = 0; i < convHeight; i++)
    {
        // Iterate over every sencond pixel since we deal with 2 at a time
        for(int j = 0; j < convWidth; j += 2)
        {
            // Each row has U0Y0V0Y1 etc, so two bytes for each pixel group in a row. Take two pixels at a time.
            ptr = (pSrcBits + i*srcStride + j*2);
            dstptr = (pDstBits + i*dstStride + j*2);
            *dstptr = *ptr;
            *(dstptr + 1) = *(ptr + 1);
            *(dstptr + 2) = *(ptr + 2);
            *(dstptr + 3) = *(ptr + 3);
        }
    }    return S_OK;
}

HRESULT RGB555_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

// TODO: RGB555 may be bi_bitfields, however no bitmasks are passed in.
//    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // BUGBUG: Split n:1 r:5 g:5 b:5
            ptr = (pSrcBits + i*srcStride + j*2);

            WORD rgb555 = *(WORD*)ptr;
            r = (rgb555 >> 7) & 0xF8;
            r |= r >> 5;
            g = (rgb555 >> 2) & 0xF8;
            g |= g >> 5;
            b = (rgb555 << 3) & 0xF8;
            b |= b >> 5;

            ptr = (pDstBits + i*dstStride + j*3);
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
        }
    }
    return S_OK;
}

HRESULT RGB565_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

// TODO: RGB565 may be bi_bitfields, however no bitmasks are passed in.
//    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            // BUGBUG: Split r:5 g:6 b:5
            ptr = pSrcBits + i*srcStride + j*2;

            WORD rgb565 = *(WORD*)ptr;

            r = (rgb565 >> 8) & 0xF8;
            r |= r >> 5;
            g = (rgb565 >> 3) & 0xFC;
            g |= g >> 6;
            b = (rgb565 << 3) & 0xF8;
            b |= b >> 5;

            ptr = (pDstBits + i*dstStride + j*3);
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
        }
    }
    return S_OK;
}

HRESULT RGB565_To_RGB565(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE* srcptr;
    BYTE* dstptr;

// TODO: RGB555/565 may be bi_bitfields, however no bitmasks are passed in.
//    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;
//    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
//        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*2;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 2;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*2;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            srcptr = (pSrcBits + i*srcStride + j*2);

            // Split r:5 g:6 b:5
            //r = (*ptr >> 5) << 3;
            //g = (((*ptr & 0x111) << 3) & (*(ptr + 1) >> 5)) << 2;
            //b = (*(ptr + 1) & 0x1f) << 3;
            // if we're bottom up, then add in the stride
            dstptr = (pDstBits + i*dstStride + j*2);

            *dstptr = *srcptr;
            *(dstptr + 1) = *(srcptr + 1);
        }
    }
    return S_OK;
}

HRESULT RGB32_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, int srcStride, int srcX, int srcY, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, int dstStride, int dstX, int dstY)
{
    // Basic pointer checking
    if (!pSrcBits || !pDstBits)
        return E_POINTER;

    int srcHeight = srcBmi->bmiHeader.biHeight;
    int srcWidth = srcBmi->bmiHeader.biWidth;
    int dstHeight = dstBmi->bmiHeader.biHeight;
    int dstWidth = dstBmi->bmiHeader.biWidth;

    BYTE r, g, b;
    BYTE* ptr;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((srcBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // TODO: Today, only RGB with BI_RGB (default bitmasks) is handled.
    if((dstBmi->bmiHeader.biCompression & ~BI_SRCPREROTATE) != BI_RGB)
        return E_NOTIMPL;

    // if the passed in stride is invalid, assume a standard stride
    if(srcStride == 0)
        srcStride = srcWidth * 4;

    // this is a top down bitmap, so swap the direction traveled.
    if(srcHeight > 0)
    {
        srcStride = -srcStride;
        pSrcBits = pSrcBits + ((srcHeight - 1)*abs(srcStride));
    }
    pSrcBits = pSrcBits + srcY*srcStride + srcX*4;

    // if the passed in stride is invalid, assume a standard stride
    if(dstStride == 0)
        dstStride = dstWidth * 3;

    // this is a top down bitmap, so swap the direction traveled.
    if(dstHeight > 0)
    {
        dstStride = -dstStride;
        pDstBits = pDstBits + ((dstHeight - 1)*abs(dstStride));
    }
    pDstBits = pDstBits + dstY*dstStride + dstX*3;

    for(int i = 0; i < convHeight; i++)
    {
        for(int j = 0; j < convWidth; j++)
        {
            ptr = (pSrcBits + i*srcStride + j*4);
            b = *(ptr);
            g = *(ptr + 1);
            r = *(ptr + 2);

            ptr = (pDstBits + i*dstStride + j*3);
            *ptr = b;
            *(ptr + 1) = g;
            *(ptr + 2) = r;
        }
    }
    return S_OK;
}

