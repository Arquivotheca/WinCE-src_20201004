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

#ifndef __GPETEST_H__
#define __GPETEST_H__

#include <ddgpe.h> // For SurfaceHeap

class GPETest : public GPE
{
private:

    GPEMode           m_ModeInfo;
    LPVOID            m_VirtualFrameBuffer;
    int               m_nVideoMemorySize;     // Size in bytes of video RAM total
    SurfaceHeap       *m_pVideoMemoryHeap;     // Base entry representing all video memory
    DWORD             m_cbScanLineLength;
    DWORD             m_cxPhysicalScreen;
    DWORD             m_cyPhysicalScreen;
    DWORD             m_colorDepth;
    DWORD             m_RedMask;
    DWORD             m_GreenMask;
    DWORD             m_BlueMask;
    DWORD             m_VesaMode;

public:
    GPETest(void);
    ~GPETest(void);
    virtual INT        NumModes(void);
    virtual SCODE      SetMode(INT modeId,    HPALETTE *palette);
    virtual INT        InVBlank(void);
    virtual SCODE      SetPalette(const PALETTEENTRY *source, USHORT firstEntry,
                                USHORT numEntries);
    virtual SCODE      GetModeInfo(GPEMode *pMode,    INT modeNumber);
    virtual SCODE      SetPointerShape(GPESurf *mask, GPESurf *colorSurface,
                                    INT xHot, INT yHot, INT cX, INT cY);
    virtual SCODE      MovePointer(INT xPosition, INT yPosition);
    virtual void       WaitForNotBusy(void);
    virtual INT        IsBusy(void);
    void               GetVirtualVideoMemory(unsigned long * virtualMemoryBase, unsigned long *videoMemorySize);
    virtual SCODE      Line(GPELineParms *lineParameters, EGPEPhase phase);
    virtual SCODE      BltPrepare(GPEBltParms *blitParameters);
    virtual SCODE      BltComplete(GPEBltParms *blitParameters);
    virtual ULONG      GetGraphicsCaps();

    SCODE             WrappedEmulatedLine (GPELineParms *lineParameters);
    void              CursorOn (void);
    void              CursorOff (void);
    virtual SCODE      AllocSurface(GPESurf **surface, int width, int height, EGPEFormat format, int surfaceFlags);
    friend void       buildDDHALInfo( LPDDHALINFO lpddhi, DWORD modeidx );
};

class TestSurf : public GPESurf
{
private:
    SurfaceHeap     *m_pHeap;
public:
                    // video memory surface
                    TestSurf(int width, int height, ULONG offset,
                                PVOID pBits, int stride, EGPEFormat format,
                                SurfaceHeap *pHeap);

                    // system memory surface
                    TestSurf(int width, int height,
                                EGPEFormat format);

    virtual         ~TestSurf(void);
};


#endif __GPETEST_H__

