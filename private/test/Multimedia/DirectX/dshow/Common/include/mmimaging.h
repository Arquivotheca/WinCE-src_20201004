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
#ifndef MMIMAGING_H
#define MMIMAGING_H

#include <tchar.h>

#ifdef UNDER_CE
#include <streams.h>
#include <cs.h>
#include <csmedia.h>
#else
#include <strmif.h>
#include <uuids.h>
#include <wmsdkidl.h>
#include <mswmdm.h>
#include <dshow.h>
#include <dvdmedia.h>

#ifndef CS_WIDTHBYTES
#define CS_WIDTHBYTES(bits) ((DWORD)(((bits)+31) & (~31)) / 8)
#endif

#ifndef CS_DIBWIDTHBYTES
#define CS_DIBWIDTHBYTES(bi) (DWORD)CS_WIDTHBYTES((DWORD)(bi).biWidth * (DWORD)(bi).biBitCount)
#endif

#endif

extern HINSTANCE globalInst;

#define FOURCC_YVYU MAKEFOURCC('Y','V','Y','U')
#define FOURCC_UYVY MAKEFOURCC('U','Y','V','Y')
#define FOURCC_YUY2 MAKEFOURCC('Y','U','Y','2')
#define FOURCC_YV12 MAKEFOURCC('Y','V','1','2')
#define FOURCC_I420 MAKEFOURCC('I','4','2','0')
#define FOURCC_YV16 MAKEFOURCC('Y','V','1','6')
#define FOURCC_AYUV MAKEFOURCC('A','Y','U','V')
#define FOURCC_JPEG  MAKEFOURCC('J', 'P', 'E', 'G')

// This is what our color converter seems to be using
//B0 = ((38142 * (Y0 - 16))                        + (66126 * (V - 128))) / 32768;
//G0 = ((38142 * (Y0 - 16)) - (5997  * (U - 128))  - (12812 * (V - 128))) / 32768;
//R0 = ((38142 * (Y0 - 16)) + (52298 * (U - 128))) / 32768;

// Which can also be written as
#define RGB2YUV(r, g, b, y, u, v) \
y = (0.257f*r) + (0.504f*g) + (0.098f*b) + 16; \
u = -(0.148f*r) - (0.291f*g) + (0.439f*b) + 128; \
v = (0.439f*r) - (0.368f*g) - (0.071f*b) + 128;

#define YUV2RGB(y, u, v, r, g, b) \
r = 1.164f*(y - 16) + 1.596f*(v - 128); \
g = 1.164f*(y - 16) - 0.813f*(v - 128) - 0.391f*(u - 128); \
b = 1.164f*(y - 16)                  + 2.018f*(u - 128)

// which can be done in fixed point with a slight loss in precision but faster on ARM.
// which can be done in fixed point with a slight loss in precision but faster on ARM.
// NOTE: make sure the shift amount isn't so much that we can get an overflow.
// worst case below we use 10 bits, leaving 22 bits to increase the precision of the
// fixed point calculation.
#define FIXED_MULT(a, b) ((((int) (a * (1 << 22)))*b)>>22)

#define FIXEDPT_RGB2YUV(r, g, b, y, u, v) \
y = FIXED_MULT(.257f, r) + FIXED_MULT(0.504f, g) + FIXED_MULT(0.098f,b) + 16; \
u = -FIXED_MULT(0.148f,r) - FIXED_MULT(0.291f,g) + FIXED_MULT(0.439f,b) + 128; \
v = FIXED_MULT(0.439f,r) - FIXED_MULT(0.368f,g) - FIXED_MULT(0.071f,b) + 128;

#define FIXEDPT_YUV2RGB(y, u, v, r, g, b) \
r = FIXED_MULT(1.164f,(y - 16)) + FIXED_MULT(1.596f,(v - 128)); \
g = FIXED_MULT(1.164f,(y - 16)) - FIXED_MULT(0.813f,(v - 128)) - FIXED_MULT(0.391f,(u - 128)); \
b = FIXED_MULT(1.164f,(y - 16))                                                 + FIXED_MULT(2.018f,(u - 128))


// NOTE: make sure the shift amount isn't so much that we can get an overflow.
// worst case below we use 10 bits, leaving 22 bits to increase the precision of the
// fixed point calculation.
#define FIXED_MULT(a, b) ((((int) (a * (1 << 22)))*b)>>22)

#define FIXEDPT_RGB2YUV(r, g, b, y, u, v) \
y = FIXED_MULT(.257f, r) + FIXED_MULT(0.504f, g) + FIXED_MULT(0.098f,b) + 16; \
u = -FIXED_MULT(0.148f,r) - FIXED_MULT(0.291f,g) + FIXED_MULT(0.439f,b) + 128; \
v = FIXED_MULT(0.439f,r) - FIXED_MULT(0.368f,g) - FIXED_MULT(0.071f,b) + 128;

#define FIXEDPT_YUV2RGB(y, u, v, r, g, b) \
r = FIXED_MULT(1.164f,(y - 16)) + FIXED_MULT(1.596f,(v - 128)); \
g = FIXED_MULT(1.164f,(y - 16)) - FIXED_MULT(0.813f,(v - 128)) - FIXED_MULT(0.391f,(u - 128)); \
b = FIXED_MULT(1.164f,(y - 16))                                                 + FIXED_MULT(2.018f,(u - 128))

//#define RGB2YUV(r, g, b, y, u, v)        \
//    y = 0.299f*r + 0.587f*g + 0.114f*b;    \
//    u = 0.596f*r - 0.274f*g - 0.322f*b;    \
//    v = 0.212f*r - 0.523f*g + 0.311f*b;
//
//#define YUV2RGB(y, u, v, r, g, b)    \
//    r = y + 0.956f*u + 0.621f*v;        \
//    g = y - 0.272f*u - 0.647f*v;        \
//    b = y - 1.105f*u + 1.702f*v;

#define CLAMP(x, lo, hi)        \
    if (x < lo)                    \
        x = lo;                    \
    if (x > hi)                    \
        x = hi;



extern DWORD AllocateBitmapBuffer(int width, int height, WORD bitcount, DWORD compression, BYTE** ppBuffer);
extern DWORD AllocateBitmapBuffer(int width, int height, GUID subtype, BYTE **ppBuffer);
extern DWORD AllocateBitmapBuffer(int width, int height, int stride, WORD bitcount, DWORD compression, BYTE** ppBuffer);
extern DWORD AllocateBitmapBuffer(int width, int height, int stride, GUID subtype, BYTE **ppBuffer);


typedef HRESULT (*ImageLoader)(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
typedef float (*ImageDiffer)(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
typedef HRESULT (*ImageConverter)(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight,
                                                            BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);

struct LoaderTableEntry
{
    GUID subtype;
    ImageLoader loader;
};

struct DifferTableEntry
{
    GUID subtype;
    ImageDiffer differ;
};

struct ConverterTableEntry
{
    GUID srcsubtype;
    GUID dstsubtype;
    ImageConverter converter;
    BOOL supportsrotate;
};

// image information
extern HRESULT GetFormatInfo(TCHAR *tszFormatInfo, UINT MaxStringLength, AM_MEDIA_TYPE *pMediaType);
DWORD GetDefaultStride(int width, GUID subtype);
DWORD GetDefaultStride(int width, WORD bitcount, DWORD compression);
DWORD GetDefaultStride(BITMAPINFOHEADER *bmi);

// BITMAP API
HRESULT GetBitmapFileInfo(TCHAR* imagefile, BITMAP* pBitmap);
HRESULT SaveBitmap(TCHAR *tszFileName, BITMAPINFO* pBmiHeader, GUID MediaType, BYTE* pBuffer);
HRESULT SaveBitmap(TCHAR *tszFileName, BITMAPINFO* pBmiHeader, GUID MediaType, BYTE* pBuffer, int srcStride);
HRESULT SaveRGBBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes);
HRESULT GetBitmapInfo(HBITMAP hBitmap, BITMAP* pBitmap);
BITMAPINFO* GetBitmapInfo(const AM_MEDIA_TYPE* pMediaType);

// Loader API
ImageLoader GetLoader(BITMAPINFO* pBmi, GUID subtype);
extern bool IsImageFile(const TCHAR* file);
extern bool IsVideoFile(const TCHAR* file);
bool IsYUVImage(GUID subtype);
bool IsRGBImage(GUID subtype);
extern HRESULT LoadImage(BYTE* pBuffer, BITMAPINFO* bmi, GUID subtype, const TCHAR* imagefile, BOOL bStretch = FALSE, float Zoom = 1.0);
extern HRESULT LoadBitmapFile(BYTE* pBuffer, BITMAPINFO *pBmInfo, const TCHAR* imagefile, BOOL bStretch = FALSE, float Zoom = 1.0);
extern HRESULT LoadYVYUImage(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadUYVYImage(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadYUY2Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadYV12Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadI420Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadYV16Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadRGB24Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadRGB32Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadRGB555Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);
extern HRESULT LoadRGB565Image(BYTE* pBuffer, BITMAPINFO* bmi, const TCHAR* imagefile, BOOL bStretch, float Zoom);

// Differ API
extern float DiffImage(BYTE* pSrcBits, BYTE* pDstBits, BITMAPINFO* pBmi, GUID subtype);
extern float DiffImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, BITMAPINFO* pBmi, GUID subtype);
extern ImageDiffer GetDiffer(BITMAPINFO* pBmi, GUID subtype);
extern float DiffRGB32Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffRGB24Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffRGB555Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffRGB565Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffUYVYImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffYUY2Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffYVYUImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffYV12Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffYV16Image(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);
extern float DiffUYVYImage(BYTE* pSrcBits, int srcStride, BYTE* pDstBits, int dstStride, int width, int height);

// Converter API
extern ImageConverter GetConverter(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmi, GUID dstsubtype);
extern ImageConverter GetConverter(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmi, GUID dstsubtype, DWORD Rotation);

extern bool CanConvertImage(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmih, GUID dstsubtype);
extern bool CanConvertImage(BITMAPINFO* pSrcBmi, GUID srcsubtype, BITMAPINFO* pDstBmih, GUID dstsubtype, DWORD Rotation);

extern HRESULT ConvertImage(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int convWidth, int convHeight,
                            BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY);
extern HRESULT ConvertImage(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int srcStride, int convWidth, int convHeight,
                            BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY, int dstStride);
extern HRESULT ConvertImage(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int srcStride, int convWidth, int convHeight,
                            BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY, int dstStride, DWORD Rotation);

HRESULT Intermediate_RGB24_Convert(BYTE* pSrcBits, BITMAPINFO* pSrcBmi, GUID srcsubtype, int srcX, int srcY, int srcStride, int convWidth, int convHeight,
                                                                      BYTE* pDstBits, BITMAPINFO* pDstBmi, GUID dstsubtype, int dstX, int dstY, int dstStride, DWORD Rotation);

// These are real conversions (not through intermediate)
extern HRESULT RGB24_To_YUY2(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_YVYU(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_UYVY(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_I420(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_YV16(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_AYUV(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_RGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_ARGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_RGB565(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_RGB555(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB24_To_RGB8(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB32_To_RGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB32_To_ARGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT ARGB32_To_RGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT ARGB32_To_ARGB32(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YUY2_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YUY2_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YVYU_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT UYVY_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT AYUV_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YV12_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YV16_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YV12_To_YV12(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YV16_To_YV16(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT YUY2_To_YUY2(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT UYVY_To_UYVY(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB555_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB565_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB8_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB565_To_RGB565(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT RGB32_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);
extern HRESULT ARGB32_To_RGB24(BYTE* pSrcBits, BITMAPINFO *srcBmi, GUID SrcSubType, int srcX, int srcY, int srcStride, int convWidth, int convHeight, BYTE* pDstBits, BITMAPINFO *dstBmi, GUID DstSubType, int dstX, int dstY, int dstStride, DWORD Rotation);


#endif
