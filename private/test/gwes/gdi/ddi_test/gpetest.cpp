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

#include    "precomp.h"
#include    <syspal.h>    // for 8Bpp we use the natural palette

// needed for cleartype and aa text selection.
#include <ctblt.h>
#include <aablt.h>


INSTANTIATE_GPE_ZONES(0x3,"MGDI Driver","unused1","unused2")    // Start with errors and warnings

static GPE    *gGPE = (GPE*)NULL;

// This prototype avoids problems exporting from .lib
BOOL APIENTRY GPEEnableDriver(ULONG engineVersion, ULONG cj, DRVENABLEDATA *data,
                              PENGCALLBACKS  engineCallbacks);

BOOL APIENTRY DrvEnableDriver(ULONG engineVersion, ULONG cj, DRVENABLEDATA *data,
                              PENGCALLBACKS  engineCallbacks)
{
    return GPEEnableDriver(engineVersion, cj, data, engineCallbacks);
}
extern "C" DWORD SetKMode(DWORD);


void RegisterDDHALAPI(void)
{
    return;
}

//
// Main entry point for a GPE-compliant driver
//

GPE *GetGPE(void)
{
    if (!gGPE)
    {
        gGPE = new GPETest();
    }

    return gGPE;
}

ULONG gBitMasks[] = { 0xf800,0x07e0,0x001f };

GPETest::GPETest (void)
{
    ULONG fbOffset;
    ULONG offsetX, offsetY;
    HKEY  hKey;
    TCHAR szBuffer[MAX_PATH] = {NULL};
    HDC   hdc = NULL;
    int   bpp;
    DIBSECTION ds;

    DEBUGMSG(GPE_ZONE_INIT,(TEXT("GPETest::GPETest\r\n")));

    // if we're running on a secondary display, retrieve the driver path
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("CETK"),
                        0, 0, &hKey) == ERROR_SUCCESS)
    {
        ULONG nDriverName = (sizeof(szBuffer) / sizeof(szBuffer[0]));
        if (RegQueryValueEx(hKey,
                            TEXT("DisplayDriver"),
                            NULL, NULL,
                            (BYTE *)szBuffer,
                            &nDriverName) == ERROR_SUCCESS)
        {
            hdc = CreateDC(szBuffer, NULL, NULL, NULL);
        }
        RegCloseKey(hKey);
    }

    if (hdc)
    {
        // get the masks from the secondary
        bpp = GetDeviceCaps(hdc,BITSPIXEL);
        HBITMAP hbmp = CreateCompatibleBitmap(hdc, 1, 1);
        GetObject(hbmp, sizeof(DIBSECTION), &ds);

        DeleteObject(hbmp);
        DeleteDC(hdc);
        hdc = NULL;
    }
    else
    {
        // get the masks from the primary
        hdc = GetDC(NULL);

        bpp = GetDeviceCaps(hdc,BITSPIXEL);
        HBITMAP hbmp = CreateCompatibleBitmap(hdc, 1, 1);
        GetObject(hbmp, sizeof(DIBSECTION), &ds);

        DeleteObject(hbmp);
        ReleaseDC(NULL, hdc);
        hdc = NULL;
   }

    m_nScreenWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    m_colorDepth = bpp;
    m_cxPhysicalScreen = m_nScreenWidth;
    m_cyPhysicalScreen = m_nScreenHeight;
    m_cbScanLineLength = m_nScreenWidth * m_colorDepth / 8;

    // assume we should use bit fields from DIBSECTION
    m_RedMask   = ds.dsBitfields[0];
    m_GreenMask = ds.dsBitfields[1];
    m_BlueMask  = ds.dsBitfields[2];

    // set rest of ModeInfo values
    m_ModeInfo.modeId = 0;
    m_ModeInfo.width = m_nScreenWidth;
    m_ModeInfo.height = m_nScreenHeight;
    m_ModeInfo.Bpp = m_colorDepth;
    m_ModeInfo.frequency = 60;    // ?
    switch (m_colorDepth)
    {
        case    8:
            m_ModeInfo.format = gpe8Bpp;

            // if we're less than 16bpp, then we're paletted and the bitmasks don't apply.
            m_RedMask   = 0x00FF0000;
            m_GreenMask = 0x0000FF00;
            m_BlueMask  = 0x000000FF;
            break;

        case    16:
            m_ModeInfo.format = gpe16Bpp;
            break;

        case    24:
            m_ModeInfo.format = gpe24Bpp;
            break;

        case    32:
            m_ModeInfo.format = gpe32Bpp;
            break;

        default:
            DEBUGMSG(GPE_ZONE_ERROR,(TEXT("Invalid BPP value passed to driver - %d\r\n"), m_ModeInfo.Bpp));
            m_ModeInfo.format = gpeUndefined;
            break;
    }
    gBitMasks[0] = m_RedMask;
    gBitMasks[1] = m_GreenMask;
    gBitMasks[2] = m_BlueMask;

    m_pMode = &m_ModeInfo;

    // compute frame buffer displayable area offset
    offsetX = (m_cxPhysicalScreen - m_nScreenWidth) / 2;
    offsetY = (m_cyPhysicalScreen - m_nScreenHeight) / 2;
    fbOffset = (offsetY * m_cbScanLineLength) + offsetX;

    // compute physical frame buffer size and set that to the amount of video memory available
    m_nVideoMemorySize = (((m_cyPhysicalScreen * m_cbScanLineLength) >> 20) + 1) << 20;        // set size to next highest 1MB boundary

    // Null the memory pointers
    m_pVideoMemoryHeap = NULL;
    m_VirtualFrameBuffer = NULL;
}

GPETest::~GPETest (void)
{
    // cleanup the primary.
    if(NULL != m_pPrimarySurface)
        delete m_pPrimarySurface;

    // Cleanup from "SetMode"
    delete m_pVideoMemoryHeap;
    if (NULL != m_VirtualFrameBuffer)
        VirtualFree(m_VirtualFrameBuffer, 0, MEM_RELEASE);
}

SCODE    GPETest::SetMode (INT modeId, HPALETTE *palette)
{
    DEBUGMSG(GPE_ZONE_INIT,(TEXT("GPETest::SetMode\r\n")));

    if (modeId != 0)
    {
        DEBUGMSG(GPE_ZONE_ERROR,(TEXT("GPETest::SetMode Want mode %d, only have mode 0\r\n"),modeId));
        return    E_INVALIDARG;
    }
    else
    {
        // Clean up any previous mode setup
        if (NULL != m_pVideoMemoryHeap)
        {
            delete m_pVideoMemoryHeap;
            m_pVideoMemoryHeap = NULL;
        }
        if (m_VirtualFrameBuffer != NULL)
        {
            VirtualFree(m_VirtualFrameBuffer, 0, MEM_RELEASE);
            m_VirtualFrameBuffer = NULL;
        }

        // Commit the shared memory that is being used to emulate vid mem.
        // (Do it here, so we can return a fail code.)
        m_VirtualFrameBuffer = VirtualAlloc(NULL, m_nVideoMemorySize, MEM_COMMIT, PAGE_READWRITE);
        if (NULL == m_VirtualFrameBuffer)
            return E_OUTOFMEMORY;

        // Clear all of video memory to black
        memset (m_VirtualFrameBuffer, 0x0, m_nVideoMemorySize);

        // Put a zero as the base "address" so all of the block
        // addresses are actually offsets.
        m_pVideoMemoryHeap = new SurfaceHeap(m_nVideoMemorySize, 0); //m_VirtualFrameBuffer);
        if (NULL == m_pVideoMemoryHeap)
        {
            VirtualFree(m_VirtualFrameBuffer, 0, MEM_RELEASE);
            return E_OUTOFMEMORY;
        }


        AllocSurface((GPESurf**)&m_pPrimarySurface,
                     m_nScreenWidth,
                     m_nScreenHeight,
                     m_ModeInfo.format,
                     GPE_REQUIRE_VIDEO_MEMORY);
    }

    if (palette)
    {
        switch (m_colorDepth)
        {
            case    8:
                *palette = EngCreatePalette (PAL_INDEXED,
                                             PALETTE_SIZE,
                                             (ULONG*)_rgbIdentity,
                                             0,
                                             0,
                                             0);
                break;

            case    16:
            case    24:
            case    32:
                *palette = EngCreatePalette (PAL_BITFIELDS,
                                             3,
                                             gBitMasks,
                                             0,
                                             0,
                                             0);
                break;
        }
    }

    return S_OK;
}

SCODE    GPETest::GetModeInfo(GPEMode *mode,    INT modeNumber)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::GetModeInfo\r\n")));

    if (modeNumber != 0)
    {
        return E_INVALIDARG;
    }

    *mode = m_ModeInfo;

    return S_OK;
}

int        GPETest::NumModes()
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::NumModes\r\n")));
    return    1;
}

void    GPETest::CursorOn (void)
{
}

void    GPETest::CursorOff (void)
{
}

SCODE    GPETest::SetPointerShape(GPESurf *pMask, GPESurf *pColorSurf, INT xHot, INT yHot, INT cX, INT cY)
{
    return    S_OK;
}

SCODE    GPETest::MovePointer(INT xPosition, INT yPosition)
{
    DEBUGMSG(GPE_ZONE_CURSOR, (TEXT("GPETest::MovePointer(%d, %d)\r\n"), xPosition, yPosition));
    return    S_OK;
}

void    GPETest::WaitForNotBusy(void)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::WaitForNotBusy\r\n")));
    return;
}

int        GPETest::IsBusy(void)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::IsBusy\r\n")));
    return    0;
}

void    GPETest::GetVirtualVideoMemory(unsigned long *virtualMemoryBase, unsigned long *videoMemorySize)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::GetVirtualVideoMemory\r\n")));

    *virtualMemoryBase = (DWORD)m_VirtualFrameBuffer;
    *videoMemorySize = m_cbScanLineLength * m_cyPhysicalScreen;
}

SCODE    GPETest::AllocSurface(GPESurf **surface, INT width, INT height, EGPEFormat format, INT surfaceFlags)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::AllocSurface\r\n")));

    DWORD bpp  = EGPEFormatToBpp[format];
    DWORD stride = ((bpp * width + 63) >> 6) << 3; // quadword align
    DWORD nSurfaceBytes = stride * height;
    if (((__int64)stride) * ((__int64) height) > ((__int64)0xffffffff) || height > nSurfaceBytes)
    {
        return E_OUTOFMEMORY;
    }

    if ((surfaceFlags & GPE_REQUIRE_VIDEO_MEMORY) ||
        (surfaceFlags & GPE_PREFER_VIDEO_MEMORY))
    {
        // Attempt to allocate from video memory
        SurfaceHeap *pHeap = m_pVideoMemoryHeap->Alloc(nSurfaceBytes);

        if (pHeap)
        {
            ULONG offset = pHeap->Address();
            *surface = new TestSurf(width, height, offset,
                                      (void*)((PUCHAR)m_VirtualFrameBuffer + offset), stride,
                                      format, pHeap);
            if (!(*surface))
            {
                pHeap->Free();
                return E_OUTOFMEMORY;
            }
            return S_OK;
        }

        if (surfaceFlags & GPE_REQUIRE_VIDEO_MEMORY)
        {
            *surface = NULL;
            return E_OUTOFMEMORY;
        }
    }

    // Allocate from system memory
    *surface = new TestSurf(width, height, format);
    if (*surface)
    {
        // check we allocated bits succesfully
        if (!((*surface)->Buffer()))
        {
            delete *surface;
            return E_OUTOFMEMORY;
        }
    }
    return S_OK;

}

SCODE    GPETest::WrappedEmulatedLine (GPELineParms *lineParameters)
{
    SCODE    retval;
    RECT    bounds;
    int        N_plus_1;                // Minor length of bounding rect + 1

    // calculate the bounding-rect to determine overlap with cursor
    if (lineParameters->dN)            // The line has a diagonal component (we'll refresh the bounding rect)
    {
        N_plus_1 = 2 + ((lineParameters->cPels * lineParameters->dN) / lineParameters->dM);
    }
    else
    {
        N_plus_1 = 1;
    }

    switch(lineParameters->iDir)
    {
        case 0:
            bounds.left = lineParameters->xStart;
            bounds.top = lineParameters->yStart;
            bounds.right = lineParameters->xStart + lineParameters->cPels + 1;
            bounds.bottom = bounds.top + N_plus_1;
            break;
        case 1:
            bounds.left = lineParameters->xStart;
            bounds.top = lineParameters->yStart;
            bounds.bottom = lineParameters->yStart + lineParameters->cPels + 1;
            bounds.right = bounds.left + N_plus_1;
            break;
        case 2:
            bounds.right = lineParameters->xStart + 1;
            bounds.top = lineParameters->yStart;
            bounds.bottom = lineParameters->yStart + lineParameters->cPels + 1;
            bounds.left = bounds.right - N_plus_1;
            break;
        case 3:
            bounds.right = lineParameters->xStart + 1;
            bounds.top = lineParameters->yStart;
            bounds.left = lineParameters->xStart - lineParameters->cPels;
            bounds.bottom = bounds.top + N_plus_1;
            break;
        case 4:
            bounds.right = lineParameters->xStart + 1;
            bounds.bottom = lineParameters->yStart + 1;
            bounds.left = lineParameters->xStart - lineParameters->cPels;
            bounds.top = bounds.bottom - N_plus_1;
            break;
        case 5:
            bounds.right = lineParameters->xStart + 1;
            bounds.bottom = lineParameters->yStart + 1;
            bounds.top = lineParameters->yStart - lineParameters->cPels;
            bounds.left = bounds.right - N_plus_1;
            break;
        case 6:
            bounds.left = lineParameters->xStart;
            bounds.bottom = lineParameters->yStart + 1;
            bounds.top = lineParameters->yStart - lineParameters->cPels;
            bounds.right = bounds.left + N_plus_1;
            break;
        case 7:
            bounds.left = lineParameters->xStart;
            bounds.bottom = lineParameters->yStart + 1;
            bounds.right = lineParameters->xStart + lineParameters->cPels + 1;
            bounds.top = bounds.bottom - N_plus_1;
            break;
        default:
            DEBUGMSG(GPE_ZONE_ERROR,(TEXT("Invalid direction: %d\r\n"), lineParameters->iDir));
            return E_INVALIDARG;
    }


    // do emulated line
    retval = EmulatedLine (lineParameters);

    return    retval;
}

SCODE    GPETest::Line(GPELineParms *lineParameters, EGPEPhase phase)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::Line\r\n")));

    if (phase == gpeSingle || phase == gpePrepare)
    {

        if ((lineParameters->pDst != m_pPrimarySurface))
        {
            lineParameters->pLine = &GPE::EmulatedLine;
        }
        else
        {
            lineParameters->pLine = (SCODE (GPE::*)(struct GPELineParms *)) &GPETest::WrappedEmulatedLine;
        }
    }
    return S_OK;
}

SCODE    GPETest::BltPrepare(GPEBltParms *blitParameters)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::BltPrepare\r\n")));

    blitParameters->pBlt = &GPE::EmulatedBlt_Internal;

    // Check to see if this should be handled by the ClearType(tm) or
    // AAFont libraries.
    if (0xAAF0 == blitParameters->rop4)
    {
        ClearTypeBltSelect(blitParameters);
        AATextBltSelect(blitParameters);
    }

    // Check to see if we need to use bilinear or halftone stretching
    if (blitParameters->prclDst != NULL)
    {
        LONG DstWidth  = blitParameters->prclDst->right - blitParameters->prclDst->left;
        LONG DstHeight = blitParameters->prclDst->bottom - blitParameters->prclDst->top;

        if (BLT_STRETCH == blitParameters->bltFlags
            && blitParameters->xPositive
            && blitParameters->yPositive
            && blitParameters->pDst->Format() > gpe8Bpp
            && DstWidth > 0
            && DstHeight > 0)
        {
            if (BILINEAR == blitParameters->iMode
                && (0xCCCC == blitParameters->rop4
                    || 0xEEEE == blitParameters->rop4
                    || 0x8888 == blitParameters->rop4)
                && DstWidth >= blitParameters->prclSrc->right - blitParameters->prclSrc->left
                && DstHeight >= blitParameters->prclSrc->bottom - blitParameters->prclSrc->top)
            {
                blitParameters->pBlt = &GPE::EmulatedBlt_Bilinear;
            }
            else if (HALFTONE == blitParameters->iMode
                     && 0xCCCC == blitParameters->rop4
                     && DstWidth <= blitParameters->prclSrc->right - blitParameters->prclSrc->left
                     && DstHeight <= blitParameters->prclSrc->bottom - blitParameters->prclSrc->top)
            {
                blitParameters->pBlt = &GPE::EmulatedBlt_Halftone;
            }
        }
    }

    return S_OK;
}

SCODE    GPETest::BltComplete(GPEBltParms *blitParameters)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::BltComplete\r\n")));
    return S_OK;
}

INT        GPETest::InVBlank(void)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::InVBlank\r\n")));
    return 0;
}

SCODE    GPETest::SetPalette(const PALETTEENTRY *source, USHORT firstEntry, USHORT numEntries)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::SetPalette\r\n")));

    if (firstEntry < 0 || firstEntry + numEntries > 256 || source == NULL)
    {
        return    E_INVALIDARG;
    }

    for(; numEntries; numEntries--)
    {
        char    red, green, blue, index;

        index = (unsigned char)(firstEntry++);
        red = (char) (source->peRed >> 2);
        green = (char) (source->peGreen >> 2);
        blue = (char) (source->peBlue >> 2);

        source++;
    }

    return    S_OK;
}

ULONG    GPETest::GetGraphicsCaps()
{
    return 0;
}

TestSurf::TestSurf(
    int width,
    int height,
    ULONG offset,
    PVOID pBits,
    int stride,
    EGPEFormat format,
    SurfaceHeap *pHeap
    )
    : GPESurf(width, height, pBits, stride, format)
{
    m_pHeap = pHeap;
    m_fInVideoMemory = TRUE;
    m_nOffsetInVideoMemory = offset;
}

TestSurf::TestSurf(
    int width,
    int height,
    EGPEFormat format
    )
    : GPESurf(width, height, format)
{
    m_pHeap = NULL;
}


TestSurf::~TestSurf(void)
{
    if (m_pHeap)
    {
        m_pHeap->Free();
    }
}

ULONG *APIENTRY DrvGetMasks(DHPDEV dhpdev)
{
    return gBitMasks;
}
