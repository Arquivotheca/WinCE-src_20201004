//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#ifndef __GPETEST_H__
#define __GPETEST_H__

class GPETest : public DDGPE
{
private:

    GPEMode           m_ModeInfo;
    HANDLE            m_hVFBMapping;
    LPVOID            m_pvFileView;
    LPVOID            m_VirtualFrameBuffer;
    int               m_nVideoMemorySize;     // Size in bytes of video RAM total
    SurfaceHeap       *m_pVideoMemoryHeap;     // Base entry representing all video memory
    DWORD             m_cbScanLineLength;
    DWORD             m_cxPhysicalScreen;
    DWORD             m_cyPhysicalScreen;
    DWORD             m_colorDepth;
    DWORD             m_RedMaskSize;
    DWORD             m_RedMaskPosition;
    DWORD             m_GreenMaskSize;
    DWORD             m_GreenMaskPosition;
    DWORD             m_BlueMaskSize;
    DWORD             m_BlueMaskPosition;
    DWORD             m_AlphaMaskSize;
    DWORD             m_AlphaMaskPosition;
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
#ifdef CLEARTYPE
    virtual ULONG   DrvEscape(
                        SURFOBJ *pso,
                        ULONG    iEsc,
                        ULONG    cjIn,
                        PVOID    pvIn,
                        ULONG    cjOut,
                        PVOID    pvOut);
#endif 

    SCODE             WrappedEmulatedLine (GPELineParms *lineParameters);
    void              CursorOn (void);
    void              CursorOff (void);
    virtual SCODE      AllocSurface(GPESurf **surface, int width, int height, EGPEFormat format, int surfaceFlags);
    friend void       buildDDHALInfo( LPDDHALINFO lpddhi, DWORD modeidx );
};

class TestSurf : public DDGPESurf
{
private:
    SurfaceHeap     *m_pHeap;
public:
                    // video memory surface
                    TestSurf(int width, int height, ULONG offset, 
                                PVOID pBits, int stride, EGPEFormat format, 
                                SurfaceHeap *pHeap);
                    // video memory surface
                    TestSurf(int width, int height, ULONG offset,
                                PVOID pBits, int stride, EGPEFormat format,
                                EDDGPEPixelFormat pixelFormat, 
                                SurfaceHeap *pHeap);
                    // system memory surface
                    TestSurf(int width, int height, int stride,
                                EGPEFormat format, 
                                EDDGPEPixelFormat pixelFormat);

    virtual         ~TestSurf(void);
};


#endif __GPETEST_H__

