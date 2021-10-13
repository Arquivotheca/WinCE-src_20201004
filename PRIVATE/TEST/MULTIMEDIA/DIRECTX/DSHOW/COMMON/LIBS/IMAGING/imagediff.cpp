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
#include <math.h>
#include "Imaging.h"

DifferTableEntry DifferTable[] = {
    {MEDIASUBTYPE_YUY2, DiffYUY2Image},
    {MEDIASUBTYPE_YVYU, DiffYVYUImage},
    {MEDIASUBTYPE_YV12, DiffYV12Image},
    {MEDIASUBTYPE_YV16, DiffYV16Image},
    {MEDIASUBTYPE_UYVY, DiffUYVYImage},
    {MEDIASUBTYPE_RGB24, DiffRGB24Image},
    {MEDIASUBTYPE_RGB32, DiffRGB32Image},
    {MEDIASUBTYPE_RGB555, DiffRGB555Image},
    {MEDIASUBTYPE_RGB565, DiffRGB565Image},
};

const int nDifferTable = sizeof(DifferTable)/sizeof(DifferTable[0]);

ImageDiffer GetDiffer(BITMAPINFO* pBmi, GUID subtype)
{
    ImageDiffer differ = NULL;
    for(int i = 0; i < nDifferTable; i++)
    {
        if (DifferTable[i].subtype == subtype)
        {
            differ = DifferTable[i].differ;
            break;
        }
    }

    return differ;
}

float DiffImage(BYTE* pSrcBits, BYTE* pDstBits, BITMAPINFO* pBmi, GUID subtype)
{
    if (!pSrcBits || !pDstBits || !pBmi)
        return -1;

    ImageDiffer differ = GetDiffer(pBmi, subtype);
    return (differ) ? differ(pSrcBits, 0, pDstBits, 0, pBmi->bmiHeader.biWidth, pBmi->bmiHeader.biHeight) : -1.0f;
}

float DiffImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, BITMAPINFO* pBmi, GUID subtype)
{
    if (!pSrcBits || !pDstBits || !pBmi)
        return -1;

    ImageDiffer differ = GetDiffer(pBmi, subtype);
    return (differ) ? differ(pSrcBits, srcStride, pDstBits, dstStride, pBmi->bmiHeader.biWidth, pBmi->bmiHeader.biHeight) : -1.0f;
}

float DiffRGB555Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    BYTE r1, g1, b1;
    BYTE r2, g2, b2;
    BYTE* ptr;
    float peldiff = 0;

    if(srcStride == 0)
        srcStride = width * 2;

    if(dstStride == 0)
        dstStride = width * 2;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            ptr = pSrcBits + i*srcStride + j*2;
            WORD rgb = *(WORD*)ptr;
            r1 = (rgb >> 7) & 0xF8;
            r1 |= r1 >> 5;
            g1 = (rgb >> 2) & 0xF8;
            g1 |= g1 >> 5;
            b1 = (rgb << 3) & 0xF8;
            b1 |= b1 >> 5;

            ptr = pDstBits + i*dstStride + j*2;
            rgb = *(WORD*)ptr;
            r2 = (rgb >> 7) & 0xF8;
            r2 |= r2 >> 5;
            g2 = (rgb >> 2) & 0xF8;
            g2 |= g2 >> 5;
            b2 = (rgb << 3) & 0xF8;
            b2 |= b2 >> 5;

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*15);
    return peldiff;
}

float DiffRGB565Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    BYTE r1, g1, b1;
    BYTE r2, g2, b2;
    float peldiff = 0;
    BYTE* ptr;

    if(srcStride == 0)
        srcStride = width * 2;

    if(dstStride == 0)
        dstStride = width * 2;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            // Split r:5 g:6 b:5
            ptr = pSrcBits + i*srcStride + j*2;
            WORD rgb = *(WORD*)ptr;
            r1 = (rgb >> 8) & 0xF8;
            r1 |= r1 >> 5;
            g1 = (rgb >> 3) & 0xFC;
            g1 |= g1 >> 6;
            b1 = (rgb << 3) & 0xF8;
            b1 |= b1 >> 5;


            ptr = pDstBits + i*dstStride + j*2;
            rgb = *(WORD*)ptr;
            r2 = (rgb >> 8) & 0xF8;
            r2 |= r2 >> 5;
            g2 = (rgb >> 3) & 0xFC;
            g2 |= g2 >> 6;
            b2 = (rgb << 3) & 0xF8;
            b2 |= b2 >> 5;

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*16);
    return peldiff;
}

float DiffRGB24Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    BYTE r1, g1, b1;
    BYTE r2, g2, b2;
    BYTE* ptr;
    float peldiff = 0;

    if(srcStride == 0)
        srcStride = width * 3;

    if(dstStride == 0)
        dstStride = width * 3;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            ptr = pSrcBits + i*srcStride + j*3;
            r1 = *ptr;
            g1 = *(ptr + 1);
            b1 = *(ptr + 2);

            ptr = pDstBits + i*dstStride + j*3;
            r2 = *ptr;
            g2 = *(ptr + 1);
            b2 = *(ptr + 2);

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffRGB32Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    BYTE r1, g1, b1;
    BYTE r2, g2, b2;
    BYTE* ptr;
    float peldiff = 0;

    if(srcStride == 0)
        srcStride = width * 4;

    if(dstStride == 0)
        dstStride = width * 4;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            ptr = pSrcBits + i*srcStride + j*4;
            r1 = *ptr;
            g1 = *(ptr + 1);
            b1 = *(ptr + 2);

            ptr = pDstBits + i*dstStride + j*4;
            r2 = *ptr;
            g2 = *(ptr + 1);
            b2 = *(ptr + 2);

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffYUY2Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    float peldiff = 0;
    unsigned char *ptr;
    unsigned char* dstptr;
    float srcr1, srcg1, srcb1;
    float srcr2, srcg2, srcb2;
    float dstr1, dstg1, dstb1;
    float dstr2, dstg2, dstb2;

    float y0, u0, v0, y1;

    if(srcStride == 0)
        srcStride = width * 2;

    if(dstStride == 0)
        dstStride = width * 2;

    for(int i = 0; i < height; i++)
    {
        // we're dealing with two pixels per surface at a time, so we're
        // converting four pixels from YUV to RGB and calculating the diff.
        for(int j = 0; j < width; j += 2)
        {
            ptr = (pSrcBits + i*srcStride + j*2);
            dstptr = (pDstBits + i*dstStride + j*2);

            y0 = (float)*ptr;
            u0 = (float)*(ptr + 1);
            y1 = (float)*(ptr + 2);
            v0 = (float)*(ptr + 3);

            // Convert 1st and second pixel of the source YUV surface
            YUV2RGB(y0, u0, v0, srcr1, srcg1, srcb1);
            YUV2RGB(y1, u0, v0, srcr2, srcg2, srcb2);
    
            CLAMP(srcr1, 0, 255);
            CLAMP(srcg1, 0, 255);
            CLAMP(srcb1, 0, 255);
            CLAMP(srcr2, 0, 255);
            CLAMP(srcg2, 0, 255);
            CLAMP(srcb2, 0, 255);

            y0 = (float)*dstptr;
            u0 = (float)*(dstptr + 1);
            y1 = (float)*(dstptr + 2);
            v0 = (float)*(dstptr + 3);

            // Convert 1st and second pixel of the dest YUV surface
            YUV2RGB(y0, u0, v0, dstr1, dstg1, dstb1);
            YUV2RGB(y1, u0, v0, dstr2, dstg2, dstb2);
            CLAMP(dstr1, 0, 255);
            CLAMP(dstg1, 0, 255);
            CLAMP(dstb1, 0, 255);
            CLAMP(dstr2, 0, 255);
            CLAMP(dstg2, 0, 255);
            CLAMP(dstb2, 0, 255);

#if DIFF_ABS
            peldiff += abs(srcr1 - dstr1) + abs(srcg1 - dstg1) + abs(srcb1 - dstb1);
            peldiff += abs(srcr2 - dstr2) + abs(srcg2 - dstg2) + abs(srcb2 - dstb2);
#else
            peldiff += (float)sqrt((float)(srcr1 - dstr1)*(srcr1 - dstr1) + (float)(srcg1 - dstg1)*(srcg1 - dstg1) + (float)(srcb1 - dstb1)*(srcb1 - dstb1));
            peldiff += (float)sqrt((float)(srcr2 - dstr2)*(srcr2 - dstr2) + (float)(srcg2 - dstg2)*(srcg2 - dstg2) + (float)(srcb2 - dstb2)*(srcb2 - dstb2));
#endif

        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffYUYVImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    // YUYV same as YVYU same as YUV422 packed
    return DiffYUY2Image(pSrcBits, srcStride, pDstBits, dstStride, width, height);
}

float DiffYVYUImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    float peldiff = 0;
    unsigned char *ptr;
    unsigned char* dstptr;
    float srcr1, srcg1, srcb1;
    float srcr2, srcg2, srcb2;
    float dstr1, dstg1, dstb1;
    float dstr2, dstg2, dstb2;
    float y0, u0, v0, y1;

    if(srcStride == 0)
        srcStride = width * 2;

    if(dstStride == 0)
        dstStride = width * 2;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j += 2)
        {
            ptr = (pSrcBits + i*srcStride + j*2);
            dstptr = (pDstBits + i*dstStride + j*2);
            y0 = (float)*ptr;
            u0 = (float)*(ptr + 1);
            y1 = (float)*(ptr + 2);
            v0 = (float)*(ptr + 3);

            // Convert 1st and second pixel of the source YUV surface
            YUV2RGB(y0, u0, v0, srcr1, srcg1, srcb1);
            YUV2RGB(y1, u0, v0, srcr2, srcg2, srcb2);
    
            CLAMP(srcr1, 0, 255);
            CLAMP(srcg1, 0, 255);
            CLAMP(srcb1, 0, 255);
            CLAMP(srcr2, 0, 255);
            CLAMP(srcg2, 0, 255);
            CLAMP(srcb2, 0, 255);

            y0 = (float)*dstptr;
            u0 = (float)*(dstptr + 1);
            y1 = (float)*(dstptr + 2);
            v0 = (float)*(dstptr + 3);

            // Convert 1st and second pixel of the dest YUV surface
            YUV2RGB(y0, u0, v0, dstr1, dstg1, dstb1);
            YUV2RGB(y1, u0, v0, dstr2, dstg2, dstb2);
            CLAMP(dstr1, 0, 255);
            CLAMP(dstg1, 0, 255);
            CLAMP(dstb1, 0, 255);
            CLAMP(dstr2, 0, 255);
            CLAMP(dstg2, 0, 255);
            CLAMP(dstb2, 0, 255);

#if DIFF_ABS
            peldiff += abs(srcr1 - dstr1) + abs(srcg1 - dstg1) + abs(srcb1 - dstb1);
            peldiff += abs(srcr2 - dstr2) + abs(srcg2 - dstg2) + abs(srcb2 - dstb2);
#else
            peldiff += (float)sqrt((float)(srcr1 - dstr1)*(srcr1 - dstr1) + (float)(srcg1 - dstg1)*(srcg1 - dstg1) + (float)(srcb1 - dstb1)*(srcb1 - dstb1));
            peldiff += (float)sqrt((float)(srcr2 - dstr2)*(srcr2 - dstr2) + (float)(srcg2 - dstg2)*(srcg2 - dstg2) + (float)(srcb2 - dstb2)*(srcb2 - dstb2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffUYVYImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    float peldiff = 0;
    unsigned char *ptr;
    unsigned char* dstptr;
    float srcr1, srcg1, srcb1;
    float srcr2, srcg2, srcb2;
    float dstr1, dstg1, dstb1;
    float dstr2, dstg2, dstb2;
    float y0, u0, v0, y1;

    if(srcStride == 0)
        srcStride = width * 2;

    if(dstStride == 0)
        dstStride = width * 2;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j += 2)
        {
            ptr = (pSrcBits + i*srcStride + j*2);
            dstptr = (pDstBits + i*dstStride + j*2);
            u0 = (float)*ptr;
            y0 = (float)*(ptr + 1);
            v0 = (float)*(ptr + 2);
            y1 = (float)*(ptr + 3);

            // Convert 1st and second pixel of the source YUV surface
            YUV2RGB(y0, u0, v0, srcr1, srcg1, srcb1);
            YUV2RGB(y1, u0, v0, srcr2, srcg2, srcb2);
    
            CLAMP(srcr1, 0, 255);
            CLAMP(srcg1, 0, 255);
            CLAMP(srcb1, 0, 255);
            CLAMP(srcr2, 0, 255);
            CLAMP(srcg2, 0, 255);
            CLAMP(srcb2, 0, 255);

            u0 = (float)*dstptr;
            y0 = (float)*(dstptr + 1);
            v0 = (float)*(dstptr + 2);
            y1 = (float)*(dstptr + 3);

            // Convert 1st and second pixel of the dest YUV surface
            YUV2RGB(y0, u0, v0, dstr1, dstg1, dstb1);
            YUV2RGB(y1, u0, v0, dstr2, dstg2, dstb2);
            CLAMP(dstr1, 0, 255);
            CLAMP(dstg1, 0, 255);
            CLAMP(dstb1, 0, 255);
            CLAMP(dstr2, 0, 255);
            CLAMP(dstg2, 0, 255);
            CLAMP(dstb2, 0, 255);

#if DIFF_ABS
            peldiff += abs(srcr1 - dstr1) + abs(srcg1 - dstg1) + abs(srcb1 - dstb1);
            peldiff += abs(srcr2 - dstr2) + abs(srcg2 - dstg2) + abs(srcb2 - dstb2);
#else
            peldiff += (float)sqrt((float)(srcr1 - dstr1)*(srcr1 - dstr1) + (float)(srcg1 - dstg1)*(srcg1 - dstg1) + (float)(srcb1 - dstb1)*(srcb1 - dstb1));
            peldiff += (float)sqrt((float)(srcr2 - dstr2)*(srcr2 - dstr2) + (float)(srcg2 - dstg2)*(srcg2 - dstg2) + (float)(srcb2 - dstb2)*(srcb2 - dstb2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffYV12Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    unsigned char* yptr, *uptr, *vptr;
    unsigned char* dstyptr, *dstuptr, *dstvptr;
    float peldiff = 0;
    float r1, g1, b1;
    float r2, g2, b2;
    int uvsrcStride, uvdstStride;

    if(srcStride == 0)
        srcStride = width;

    uvsrcStride = srcStride/2;

    if(dstStride == 0)
        dstStride = width;

    uvdstStride = dstStride /2;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {
            // YV12 is planar
            // First the width*height Y plane followed by (width/2)*(height/2) v plane and then the u plane
            yptr = pSrcBits + i*srcStride + j;
            vptr = pSrcBits + height*srcStride + i/2*uvsrcStride + j/2;
            uptr = pSrcBits + height*srcStride + height/2*uvsrcStride + i/2*uvsrcStride + j/2;

            dstyptr = pDstBits + i*dstStride + j;
            dstvptr = pDstBits + height*dstStride + i/2*uvdstStride + j/2;
            dstuptr = pDstBits + height*dstStride + height/2*uvdstStride + i/2*uvdstStride + j/2;

            YUV2RGB(*yptr, *uptr, *vptr, r1, g1, b1);
            CLAMP(r1, 0, 255);
            CLAMP(g1, 0, 255);
            CLAMP(b1, 0, 255);

            YUV2RGB(*dstyptr, *dstuptr, *dstvptr, r2, g2, b2);
            CLAMP(r2, 0, 255);
            CLAMP(g2, 0, 255);
            CLAMP(b2, 0, 255);

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}

float DiffYV16Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height)
{
    if (!pSrcBits || !pDstBits)
        return -1;

    unsigned char* yptr, *uptr, *vptr;
    unsigned char* dstyptr, *dstuptr, *dstvptr;
    float peldiff = 0;
    float r1, g1, b1;
    float r2, g2, b2;

    int uvsrcStride, uvdstStride;

    if(srcStride == 0)
        srcStride = width;

    uvsrcStride = srcStride/2;

    BYTE * srcyptrStart = pSrcBits;
    BYTE * srcuptrStart = srcyptrStart + height*srcStride;
    BYTE * srcvptrStart = srcuptrStart + height*uvsrcStride;

    if(dstStride == 0)
        dstStride = width;

    uvdstStride = dstStride /2;

    BYTE * dstyptrStart = pDstBits;
    BYTE * dstuptrStart = dstyptrStart + height*dstStride;
    BYTE * dstvptrStart = dstuptrStart + height*uvdstStride;

    for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {

            // YV16 is planar
            // First the width*height Y plane followed by (width/2)*(height) u plane and then the v plane
            yptr = srcyptrStart + i*srcStride + j;
            vptr = srcuptrStart + i*uvsrcStride + (j/2);
            uptr = srcvptrStart + i*uvsrcStride + (j/2);

            dstyptr = dstyptrStart + i*dstStride + j;
            dstvptr = dstuptrStart + i*uvdstStride + (j/2);
            dstuptr = dstvptrStart + i*uvdstStride + (j/2);

            YUV2RGB(*yptr, *uptr, *vptr, r1, g1, b1);
            CLAMP(r1, 0, 255);
            CLAMP(g1, 0, 255);
            CLAMP(b1, 0, 255);

            YUV2RGB(*dstyptr, *dstuptr, *dstvptr, r2, g2, b2);
            CLAMP(r2, 0, 255);
            CLAMP(g2, 0, 255);
            CLAMP(b2, 0, 255);

#if DIFF_ABS
            peldiff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
#else
            peldiff += (float)sqrt((float)(r1 - r2)*(r1 - r2) + (float)(g1 - g2)*(g1 - g2) + (float)(b1 - b2)*(b1 - b2));
#endif
        }
    }
    // Calculate error per bit
    peldiff = peldiff/(width*height*24);
    return peldiff;
}
