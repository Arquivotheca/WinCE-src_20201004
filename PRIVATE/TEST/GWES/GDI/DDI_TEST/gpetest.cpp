//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
    DWORD       oldMode;
    ULONG       fbOffset;
    ULONG       offsetX, offsetY;
    HDC         hdc = GetDC(NULL);
    int         bpp = GetDeviceCaps(hdc,BITSPIXEL);
    ReleaseDC(NULL, hdc);
       
    DEBUGMSG(GPE_ZONE_INIT,(TEXT("GPETest::GPETest\r\n")));

    oldMode = SetKMode(TRUE);

    m_nScreenWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    m_colorDepth = bpp;
    m_cxPhysicalScreen = m_nScreenWidth;
    m_cyPhysicalScreen = m_nScreenHeight;
    m_cbScanLineLength = m_nScreenWidth * m_colorDepth / 8;
    
    SetKMode(oldMode);

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
            break;

        case    16:
            m_ModeInfo.format = gpe16Bpp;
            
            // TODO: Make this configurable
            // Default to RGB565
            m_RedMaskSize = 5;
            m_GreenMaskSize = 6;
            m_BlueMaskSize = 5;
            m_RedMaskPosition = m_GreenMaskSize + m_BlueMaskSize;
            m_GreenMaskPosition = m_BlueMaskSize;
            m_BlueMaskPosition = 0;
            break;

        case    24:
            m_ModeInfo.format = gpe24Bpp;
            
            // TODO: Make this configurable
            // Default to RGB888
            m_RedMaskSize = 8;
            m_GreenMaskSize = 8;
            m_BlueMaskSize = 8;
            m_RedMaskPosition = m_GreenMaskSize + m_BlueMaskSize;
            m_GreenMaskPosition = m_BlueMaskSize;
            m_BlueMaskPosition = 0;
            break;

        case    32:
            m_ModeInfo.format = gpe32Bpp;
            
            // TODO: Make this configurable
            // Default to RGB0888
            m_AlphaMaskSize = 8;
            m_RedMaskSize = 8;
            m_GreenMaskSize = 8;
            m_BlueMaskSize = 8;
            m_AlphaMaskPosition = m_RedMaskSize + m_GreenMaskSize + m_BlueMaskSize;
            m_RedMaskPosition = m_GreenMaskSize + m_BlueMaskSize;
            m_GreenMaskPosition = m_BlueMaskSize;
            m_BlueMaskPosition = 0;
            break;

        default:
            DEBUGMSG(GPE_ZONE_ERROR,(TEXT("Invalid BPP value passed to driver - %d\r\n"), m_ModeInfo.Bpp));
            m_ModeInfo.format = gpeUndefined;
            break;
    }
    gBitMasks[0] =((1<<m_RedMaskSize) -1) << m_RedMaskPosition;  
    gBitMasks[1] =((1<<m_GreenMaskSize)-1) << m_GreenMaskPosition;
    gBitMasks[2] =((1<<m_BlueMaskSize) -1) << m_BlueMaskPosition;
                
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
    m_pvFileView = NULL;
    
    // Use CreateFileMapping/MapViewOfFile to guarantee the VirtualFrameBuffer
    // pointer is allocated in shared memory.
    m_hVFBMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, m_nVideoMemorySize, NULL);
    if (m_hVFBMapping != NULL)
    {
        m_pvFileView = MapViewOfFile(m_hVFBMapping, FILE_MAP_WRITE, 0, 0, 0);
    }
    
    if (m_pvFileView == NULL)
    {
        RETAILMSG(1, (TEXT("ERROR: Unable to map file view\n")));
    }
}

GPETest::~GPETest (void)
{
    // Cleanup from "SetMode"
    delete m_pVideoMemoryHeap;
    if (NULL != m_VirtualFrameBuffer)
        VirtualFree(m_VirtualFrameBuffer, 0, MEM_RELEASE);

    // Cleanup from Constructor
    if (NULL != m_pvFileView)
        UnmapViewOfFile(m_pvFileView);
    if (NULL != m_hVFBMapping)
        CloseHandle(m_hVFBMapping);
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
        m_VirtualFrameBuffer = VirtualAlloc(m_pvFileView, m_nVideoMemorySize, MEM_COMMIT, PAGE_READWRITE);
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

        
        ULONG lPrimOffset = 0;
        AllocVideoSurface((DDGPESurf**)&m_pPrimarySurface, 
                          m_nScreenWidth,
                          m_nScreenHeight,
                          m_ModeInfo.format,
                          EGPEFormatToEDDGPEPixelFormat[m_ModeInfo.format],
                          &lPrimOffset);
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
                                             0,
                                             NULL,
                                             ((1 << m_RedMaskSize) - 1) << m_RedMaskPosition,
                                             ((1 << m_GreenMaskSize) - 1) << m_GreenMaskPosition,
                                             ((1 << m_BlueMaskSize) - 1) << m_BlueMaskPosition);
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
    LONG align = 8; // quadword aligned
    DWORD alignedWidth = ((width + align - 1) & (- align));
    DWORD nSurfaceBytes = (bpp * (alignedWidth * height)) / 8;
    DWORD stride = ((alignedWidth * bpp) / 8);

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
                                      format/*, pixelFormat*/, pHeap);
            if (!(*surface))
            {
                pHeap->Free();
                return E_OUTOFMEMORY;
            }
            return S_OK;
        }

        if (surfaceFlags & GPE_REQUIRE_VIDEO_MEMORY)
        {
            *surface = (DDGPESurf *)NULL;
            return E_OUTOFMEMORY;
        }
    }

    // Allocate from system memory
    EDDGPEPixelFormat pixelFormat = EGPEFormatToEDDGPEPixelFormat[format];
    DEBUGMSG(GPE_ZONE_CREATE, 
        (TEXT("Creating a DDGPESurf in system memory. ")
         TEXT("EGPEFormat = %d, DDGPEFormat = %d\r\n"), 
         (int) format, (int) pixelFormat));
         
    // REVIEW: What about stuff like gpeDeviceCompatible?
    *surface = new TestSurf(width, height, stride, format, pixelFormat);
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
            lineParameters->pLine = EmulatedLine;
        }
        else
        {
            lineParameters->pLine = (SCODE (GPE::*)(struct GPELineParms *)) WrappedEmulatedLine;
        }
    }
    return S_OK;
}

SCODE    GPETest::BltPrepare(GPEBltParms *blitParameters)
{
    DEBUGMSG (GPE_ZONE_INIT, (TEXT("GPETest::BltPrepare\r\n")));

    // default to base EmulatedBlt_Internal routine to bypass all optimized software blt routines.
    blitParameters->pBlt = EmulatedBlt_Internal;

    // if SYSGEN_GPE_CLEARTYPE is set or SYSGEN_GPE_NOEMUL isn't
    // when the dummy driver is made, it will advertise that it can do cleartype, and we have
    // to honor that or the text will look completely wrong.

    // Check to see if this should be handled by the ClearType(tm) library.
    if (blitParameters->pBlt == EmulatedBlt_Internal)
    {
        ClearTypeBltSelect(blitParameters);
    }
	
    // Check to see if this should be handled by the AAFont library.
    if (blitParameters->pBlt == EmulatedBlt_Internal)
    {
        AATextBltSelect(blitParameters);
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
    return IN_VBLANK;
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
    : DDGPESurf(width, height, pBits, stride, format)
{
    m_pHeap = pHeap;
    m_fInVideoMemory = TRUE;
    m_nOffsetInVideoMemory = offset;
} 

TestSurf::TestSurf(
    int width,
    int height,
    ULONG offset,
    PVOID pBits,
    int stride,
    EGPEFormat format,
    EDDGPEPixelFormat pixelFormat,
    SurfaceHeap *pHeap
    )
    : DDGPESurf(width, height, pBits, stride, format, pixelFormat)
{
    m_pHeap = pHeap;
    m_fInVideoMemory = TRUE;
    m_nOffsetInVideoMemory = offset;
}

TestSurf::TestSurf(
    int width,
    int height,
    int stride,
    EGPEFormat format,
    EDDGPEPixelFormat pixelFormat
    )
    : DDGPESurf(width, height, stride, format, pixelFormat)
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
