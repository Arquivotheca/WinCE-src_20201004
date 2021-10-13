//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/
#ifndef __PERFDATA_H__
#define __PERFDATA_H__

#ifdef ENABLE_PERFDDI


// 
// Performance data is stored as milliseconds, and type is DWORD
// to match GetTickCount.  Both of these may change once higher-
// resolution timers become available.
//
typedef DWORD MSEC;


//
// Here is a list of every DDI operation we track.
// This list should correspond 1-to-1 with the members
// of the DRVENABLEDATA struct.
//
enum KPERFOP
{
    kperfopFirst = 0,

    // ---------------------------------------------
    // IF YOU CHANGE THIS LIST, FIX PwszFromKperfop!
    // ---------------------------------------------

    kperfopEnablePDEV = 0, 
    kperfopDisablePDEV,
    kperfopEnableSurface,
    kperfopDisableSurface,
    kperfopCreateDeviceBitmap,
    kperfopDeleteDeviceBitmap,
    kperfopRealizeBrush,
    kperfopStrokePath,
    kperfopFillPath,
    kperfopPaint,
    kperfopBitBlt,
    kperfopCopyBits,
    kperfopAnyBlt,
    kperfopTransparentBlt,
    kperfopSetPalette,
    kperfopSetPointerShape,
    kperfopMovePointer,
    kperfopGetModes,
    kperfopRealizeColor,
    kperfopGetMasks,
    kperfopUnrealizeColor,
    kperfopContrastControl,
    kperfopPowerHandler,
    kperfopEndDoc,
    kperfopStartDoc,
    kperfopStartPage,

    // ---------------------------------------------
    // IF YOU CHANGE THIS LIST, FIX PwszFromKperfop!
    // ---------------------------------------------

    kperfopMax,
    kperfopNil = -1
};

#define ForAllKperfop(kperfop)          \
    for ((kperfop) = kperfopFirst;      \
         (kperfop) < kperfopMax;        \
         (kperfop) = ((KPERFOP)((kperfop) + 1)))

// Returns the string name of the given KPERFOP
PCWSTR PwszFromKperfop(KPERFOP kperfop);


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Class:

    PERFDATA

Owner:

    AnthonyL

Description:

    Base class for all PERFDDI-driven data collection.  There is
    one subclass of PERFDATA for each interesting DDI entrypoint.

-------------------------------------------------------------------*/
class PERFDATA
{
public:
    friend class PERFDDI;
    friend int __cdecl QsortPerfdataHelper(const void *p1, const void *p2);

    //
    // CTOR and DTOR
    //
    PERFDATA(KPERFOP kperfop) : 
        m_kperfop(kperfop), 
        m_pperfdataNext(NULL),
        m_nargs(0)
        { }

    ~PERFDATA() { }

    //
    // Methods to start and stop the timer on this PERFDATA
    //
    void StartTime() { m_msec = (MSEC)-(LONG)GetTickCount(); }
    void StopTime()  { m_msec += GetTickCount(); }

    //
    // Virtual method for dumping collected data, and for showing
    // legend describing data format
    //
    virtual void ShowData();
    virtual void ShowLegend() { OutputDebugString(TEXT("PerfOp     msec\r\n")); }

protected:
    //
    // Keep a pointer to the next PERFDATA in the list.
    //
    PERFDATA *m_pperfdataNext;

    //
    // Collected data must go at the end of this class.  That way, 
    // the derived classes can append their data in a way that the 
    // base class can get to it.
    //
    KPERFOP m_kperfop;      // Kind of operation being profiled
    MSEC    m_msec;         // Elapsed time for this operation
    int     m_nargs;        // Number of args to follow

    // Derived-class stores collected data here.  Derived class must be
    // large enough to handle the data, and all data must be plain
    // old DWORDs.
    // DWORD   m_rgdwArgs[0];  
};

typedef PERFDATA *PPERFDATA;


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                    BEGIN PERFDATA SUBCLASSES                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PERFDATA_EnablePDEV : public PERFDATA
{
public:
    PERFDATA_EnablePDEV() : PERFDATA(kperfopEnablePDEV) { }
};

class PERFDATA_DisablePDEV : public PERFDATA
{
public:
    PERFDATA_DisablePDEV() : PERFDATA(kperfopDisablePDEV) { }
};

class PERFDATA_EnableSurface : public PERFDATA
{
public:
    PERFDATA_EnableSurface() : PERFDATA(kperfopEnableSurface) { }
};

class PERFDATA_DisableSurface : public PERFDATA
{
public:
    PERFDATA_DisableSurface() : PERFDATA(kperfopDisableSurface) { }
};

class PERFDATA_CreateDeviceBitmap : public PERFDATA
{
public:
    PERFDATA_CreateDeviceBitmap() : PERFDATA(kperfopCreateDeviceBitmap) { }
};

class PERFDATA_DeleteDeviceBitmap : public PERFDATA
{
public:
    PERFDATA_DeleteDeviceBitmap() : PERFDATA(kperfopDeleteDeviceBitmap) { }
};

class PERFDATA_RealizeBrush : public PERFDATA
{
public:
    PERFDATA_RealizeBrush() : PERFDATA(kperfopRealizeBrush) { }
};

class PERFDATA_StrokePath : public PERFDATA
{
public:
    PERFDATA_StrokePath() : PERFDATA(kperfopStrokePath) { }

    void Record(MIX mix) { m_nargs = 1; m_mix = mix; }
    void ShowLegend() { OutputDebugString(TEXT("StrokePath msec    MIX\r\n")); }
    DWORD m_mix;
};

class PERFDATA_FillPath : public PERFDATA
{
public:
    PERFDATA_FillPath() : PERFDATA(kperfopFillPath) { }

    void Record(MIX mix) { m_nargs = 1; m_mix = mix; }
    void ShowLegend() { OutputDebugString(TEXT("FillPath   msec    MIX\r\n")); }
    DWORD m_mix;
};

class PERFDATA_Paint : public PERFDATA
{
public:
    PERFDATA_Paint() : PERFDATA(kperfopPaint) { }

    void Record(MIX mix) { m_nargs = 1; m_mix = mix; }
    void ShowLegend() { OutputDebugString(TEXT("Paint      msec    MIX\r\n")); }
    DWORD m_mix;
};

class PERFDATA_BitBlt : public PERFDATA
{
public:
    PERFDATA_BitBlt() : PERFDATA(kperfopBitBlt) { }

    void Record(DWORD rop, int area, int bppSrc, int bppDst)
        { m_nargs = 4; m_rop = rop; m_area = area; m_bppSrc = bppSrc; m_bppDst = bppDst; }
    void ShowLegend() { OutputDebugString(TEXT("BitBlt     msec    ROP     AREA     BPP-src  BPP-dst\r\n")); }
    DWORD m_rop;
    DWORD m_area;
    DWORD m_bppSrc;
    DWORD m_bppDst;
};

class PERFDATA_CopyBits : public PERFDATA
{
public:
    PERFDATA_CopyBits() : PERFDATA(kperfopCopyBits) { }
};

class PERFDATA_AnyBlt : public PERFDATA
{
public:
    PERFDATA_AnyBlt() : PERFDATA(kperfopAnyBlt) { }

    void Record(int area, int bppSrc, int bppDst)
        { m_nargs = 3; m_area = area; m_bppSrc = bppSrc; m_bppDst = bppDst; }
    void ShowLegend() { OutputDebugString(TEXT("AnyBlt msec    AREA     BPP-src  BPP-dst\r\n")); }
    DWORD m_area;
    DWORD m_bppSrc;
    DWORD m_bppDst;
};

class PERFDATA_TransparentBlt : public PERFDATA
{
public:
    PERFDATA_TransparentBlt() : PERFDATA(kperfopTransparentBlt) { }

    void Record(int area, int bppSrc, int bppDst)
        { m_nargs = 3; m_area = area; m_bppSrc = bppSrc; m_bppDst = bppDst; }
    void ShowLegend() { OutputDebugString(TEXT("TransBlt   msec    AREA     BPP-src  BPP-dst\r\n")); }
    DWORD m_area;
    DWORD m_bppSrc;
    DWORD m_bppDst;
};

class PERFDATA_SetPalette : public PERFDATA
{
public:
    PERFDATA_SetPalette() : PERFDATA(kperfopSetPalette) { }

    void Record(int cColors) { m_nargs = 2; m_cColors = cColors; }
    void ShowLegend() { OutputDebugString(TEXT("SetPalette msec    #Colors\r\n")); }
    DWORD m_cColors;
};

class PERFDATA_SetPointerShape : public PERFDATA
{
public:
    PERFDATA_SetPointerShape() : PERFDATA(kperfopSetPointerShape) { }
};

class PERFDATA_MovePointer : public PERFDATA
{
public:
    PERFDATA_MovePointer() : PERFDATA(kperfopMovePointer) { }
};

class PERFDATA_GetModes : public PERFDATA
{
public:
    PERFDATA_GetModes() : PERFDATA(kperfopGetModes) { }
};

class PERFDATA_RealizeColor : public PERFDATA
{
public:
    PERFDATA_RealizeColor() : PERFDATA(kperfopRealizeColor) { }

    void Record(COLORREF colorref) { m_nargs = 1; m_colorref = colorref; }
    void ShowLegend() { OutputDebugString(TEXT("RealizeCol msec    COLORREF\r\n")); }
    DWORD m_colorref;
};

class PERFDATA_GetMasks : public PERFDATA
{
public:
    PERFDATA_GetMasks() : PERFDATA(kperfopGetMasks) { }
};

class PERFDATA_UnrealizeColor : public PERFDATA
{
public:
    PERFDATA_UnrealizeColor() : PERFDATA(kperfopUnrealizeColor) { }

    void Record(COLORREF colorref) { m_nargs = 1; m_colorref = colorref; }
    void ShowLegend() { OutputDebugString(TEXT("UnrealizeC msec    COLORREF\r\n")); }
    DWORD m_colorref;
};

class PERFDATA_ContrastControl : public PERFDATA
{
public:
    PERFDATA_ContrastControl() : PERFDATA(kperfopContrastControl) { }
};

class PERFDATA_PowerHandler : public PERFDATA
{
public:
    PERFDATA_PowerHandler() : PERFDATA(kperfopPowerHandler) { }
};

class PERFDATA_EndDoc : public PERFDATA
{
public:
    PERFDATA_EndDoc() : PERFDATA(kperfopEndDoc) { }
};

class PERFDATA_StartDoc : public PERFDATA
{
public:
    PERFDATA_StartDoc() : PERFDATA(kperfopStartDoc) { }
};

class PERFDATA_StartPage : public PERFDATA
{
public:
    PERFDATA_StartPage() : PERFDATA(kperfopStartPage) { }
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                     END PERFDATA SUBCLASSES                          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


#endif  // ENABLE_PERFDDI
#endif  // !__PERFDATA_H__

