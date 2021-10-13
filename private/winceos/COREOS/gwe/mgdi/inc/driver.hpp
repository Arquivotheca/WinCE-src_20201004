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

#ifndef __DRIVER_HPP_INCLUDED__
#define __DRIVER_HPP_INCLUDED__

#ifndef _GDIOBJ_H_
#include "GdiObj.h"
#endif // _GDIOBJ_H_

#include <hgtypes.h>

// Used to cache info about current system palette
class  COLORTBL_CACHE
{
private:

    // Last realized logical palette. This data member is used only in palette
    // realization optimization - to find out whether realization is NOP or it
    // really has to be done.
    HPALETTE m_hpalLog;

    // Number of entries in the pEntries aray, equals to the number of
    // entries in the driver's default palette (even if last realized palette
    // had different number of entries).
    USHORT m_cEntries;

    // Array that reflects current status of the system (hardware) palette.
    // Might be not the same as last realized palette if that palette had less
    // entries than driver's default palette. In this case first X entries of
    // m_pEntries are overwritten. (X - size of the last realized palette).
    PALETTEENTRY* m_pEntries;

public:

    // Called once per driver initialization.
    // Cache is init-ed with driver's default palette values.
    bool
    FInit(
        HPALETTE      hpal,
        ColorTable_t* pcolortbl
        );

    BOOL
    Update(
        Palette_t* pPal,
        ULONG      cColors
        );


    HPALETTE
    Hpalette(
        void
        ) const
    {
        return m_hpalLog;
    }

    USHORT
    CEntries(
        void
        ) const
    {
        return m_cEntries;
    }

    PALETTEENTRY*
    PPalentries(
        void
        ) const
    {
        return m_pEntries;
    }

    ~COLORTBL_CACHE();
};

/*
    Driver_t encapsulates all knowledge of loadable MGDI drivers.
    It is used to handle the main display driver, printer
    drivers, faceplate drivers, etc.
*/

class Driver_t
{
private:


typedef VOID (*DrvDeleteDeviceBitmap_t)
                (
                DHSURF dhsurf
                );


typedef HBITMAP (*DrvCreateDeviceBitmap_t)
                    (
                    DHPDEV dhpdev,
                    SIZEL sizl,
                    ULONG iFormat
                    );

private:
    DEVMODEW*     m_pDisplayModes;
    DWORD         m_DisplayModeSize;
    DEVMODEW*     m_pCurrentMode;
    int           m_nBootAngle;

    DRVENABLEDATA        m_drvenabledata1;    //    Entry points into driver
    DHPDEV                m_dhpdev;            //    Handle to physical device
    SurfObjWrapper_t    m_surfobj1;            //    Driver's primary surface

    struct MINIMIZEDFSE
    {
        DWORD process;
        DEVMODEW *devMode;
        HWND  exclusiveWindow;
        MINIMIZEDFSE *next;
    };

    MINIMIZEDFSE* m_minimizedFullscreenProcesses;
    DEVMODEW* m_preExclusiveMode;

public:

    WBITMAP*            m_pwbitmap;

    // Window compositor data associated with this display driver.
    HHGDISPLAY          m_hHgDisplay;

    // Points to the ColorTable_t the driver provided in the call to
    // EngCreatePalette on startup.
    ColorTable_t* m_pcolortbl;

    HPALETTE      m_hpalette;       // Device's default palette
    ULONG         m_flGraphicsCaps; // Driver's graphics capabilites flags
    Driver_t*     m_pdriverNext;    // Link to next loaded driver
    long          m_iDriver;        // for multimon, if it is SINGLE_DRIVER_HANDLE, the driver is not in multimon system

    Driver_t*     m_pMultimonNext;  // Link to next multimon device.

    // The DEVCAPS members are used to satisfy calls to GetDeviceCaps.
    // For example, the return value for GetDeviceCaps(hdc, HORZSIZE)
    // is stored in hdc.m_driver.m_devcaps.m_HORZSIZE
    struct DEVCAPS
    {
        // REVIEW Vladimir: We can delete any of these if we also
        // kill the corresponding macro definition (DRIVERVERSION, etc.)
        // in wingdi.h.  We need to evaluate exactly which GetDeviceCaps
        // values we need to support.
        // REVIEW why do we have this completely new structure since most
        // of it is the same as GDIINFO?
        DWORD m_DRIVERVERSION;
        DWORD m_TECHNOLOGY;
        DWORD m_HORZSIZE;
        DWORD m_VERTSIZE;
        DWORD m_HORZRES;
        DWORD m_VERTRES;
        DWORD m_LOGPIXELSX;
        DWORD m_LOGPIXELSY;
        DWORD m_BITSPIXEL;
        DWORD m_PLANES;
        DWORD m_NUMBRUSHES;
        DWORD m_NUMPENS;
        DWORD m_NUMFONTS;
        DWORD m_NUMCOLORS;
        DWORD m_ASPECTX;
        DWORD m_ASPECTY;
        DWORD m_ASPECTXY;
        DWORD m_TOUCHTARGETSIZE;

        DWORD m_PHYSICALWIDTH;      // For printers
        DWORD m_PHYSICALHEIGHT;     // For printers
        DWORD m_PHYSICALOFFSETX;    // For printers
        DWORD m_PHYSICALOFFSETY;    // For printers

        DWORD m_SHADEBLENDCAPS;

        //
        // REVIEW Vladimir: It's unclear how the driver tells MGDI
        // about the following fields.  Can we just drop support for
        // these values altogether?  If so, let's delete the macro
        // definitions from wingdi.h.
        //
        DWORD m_PDEVICESIZE;
        DWORD m_CLIPCAPS;
        DWORD m_SIZEPALETTE;
        DWORD m_NUMRESERVED;
        DWORD m_COLORRES;
        //
        // MGDI: RC_PALETTE flag for m_RASTERCAPS indicates that hw supports
        // palette realization
        //
        DWORD m_RASTERCAPS;
        DWORD m_CURVECAPS;
        DWORD m_LINECAPS;
        DWORD m_POLYGONALCAPS;
        DWORD m_TEXTCAPS;
    } m_devcaps;

    DEVMODE m_DevMode;

    RECT    m_rcMonitor;
    RECT    m_rcWork;

    Driver_t(
             void
            );

    // Every time a DC is created or destroyed, that DC's driver
    // must be notified via these methods.  When a driver's refcount
    // hits zero, the driver is automatically unloaded.
    void
    IncRefcnt(
        void
        );

    void
    DecRefcnt(
        void
        );

    ColorTable_t*
    OwnOrPaletteColorTable(
        void
        );

    ColorTable_t*
    PaletteColorTable(
        void
        );

    ColorTable_t*
    OwnOrStockColorTable(
        unsigned long    BitmapFormat
        );

    const DRVENABLEDATA*
    DrvEnableData(
        void
        ) const;


    PFN_DrvAlphaBlend
    DrvAlphaBlendFuncPtr(
        void
        ) const;


    PFN_DrvRealizeColor
    DrvRealizeColorFuncPtr(
        void
        ) const;


    PFN_DrvSetPalette
    DrvSetPaletteFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvSetPalette;
    }


    PFN_DrvSetPointerShape
    DrvSetPointerShapeFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvSetPointerShape;
    }


    PFN_DrvMovePointer
    DrvMovePointerFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvMovePointer;
    }


    PFN_DrvAnyBlt
    DrvAnyBltFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvAnyBlt;
    }


    PFN_DrvFillPath
    DrvFillPathFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvFillPath;
    }


    PFN_DrvStrokePath
    DrvStrokePathFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvStrokePath;
    }


    PFN_DrvGetMasks
    DrvGetMasksFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvGetMasks;
    }


    PFN_DrvDisableDriver
    DrvDisableDriverFuncPtr(
       void
       ) const
    {
        return m_drvenabledata1.DrvDisableDriver;
    }


    BOOL
    DrvContrastControl(
        int        Cmd,
        DWORD*    pValue
        ) const
    {
        //REVIEW this is screwy.  Why is dhpdev coming from the surface instead of the driver itself?
        return m_drvenabledata1.DrvContrastControl(m_surfobj1.m_dhpdev, Cmd, pValue);
    }


    ULONG
    DrvGetModes(
        HANDLE        hDriver,
        ULONG        cjSize,
        DEVMODEW*    pdm
        ) const;



    ULONG
    DrvEscape(
        SurfObjWrapper_t*    pso,
        ULONG                  iEsc,
        ULONG                  cjIn,
        PVOID                  pvIn,
        ULONG                  cjOut,
        PVOID                  pvOut
        );


    ULONG
    DrvEscape(
        DHPDEV                 dhpdev,
        SurfObjWrapper_t*      pso,
        ULONG                  iEsc,
        ULONG                  cjIn,
        PVOID                  pvIn,
        ULONG                  cjOut,
        PVOID                  pvOut
        ) const
    {
        return m_drvenabledata1.DrvEscape(dhpdev, pso -> SURFOBJ_Ptr(), iEsc, cjIn, pvIn, cjOut, pvOut);
    }


    DHPDEV
    Dhpdev(
        void
        ) const
    {
        return m_dhpdev;
    }


    bool
    ColorTableIsSame(
        UINT32 nBpp,
        ColorTable_t*    pColorTable
        );

    LONG
    SetRotateParams( );

    DEVMODEW*
    MatchDisplayMode(
        const DEVMODEW* pNewMode
        );

    DEVMODEW*
    GetCurrentMode( ) const;

    BOOL
    UnSetDisplayMode( );

    BOOL
    SetDisplayMode(
        const DEVMODEW* pNewMode
        );

    // Returns TRUE iff this driver is for a printer.  Note that
    // we consider plotters to be printers.
    BOOL
    IsPrint(
        void
        );

    BOOL
    IsPrimary();

    //    Does driver support palette realization
    BOOL
    FSettableHwPal(
        void
        );

    Palette_t*
    PPalDef(
        void
        );

    HPALETTE
    HPalDef(
        void
        );

    COLORTBL_CACHE*
    PColorTblCache(
        void
        );

    BOOL
    EnterExclusiveMode(
        DWORD Process,
        HWND  Window
        );

    BOOL
    LeaveExclusiveMode(
        DWORD Process
        );

    int
    TestCooperativeLevel(
        DWORD Process
        );

    BOOL
    IsAnyExclusive(
        void
        );

    HWND
    DeactivateAnyExclusive(
        void
        );

    void ActivateIfExclusive(
        DWORD Process
        );

    void CleanupExclusiveTable( DWORD Process );
    HINSTANCE GetHINSTANCE() const { return m_hinstDriverDll; }

    HMONITOR
    GetHMONITOR() const;

    static
    bool
    InitMainDriver(
        HINSTANCE    hinstance
        );

    static
    Driver_t *
    PPrimaryMonitor();

    const SurfObjWrapper_t&
    Surface() const
    {
        return m_surfobj1;
    }

    SurfObjWrapper_t&
    Surface()
    {
        return m_surfobj1;
    }

    //REVIEW should just have one function to create/load driver given the dll name.
    //    Returns a pointer to the Driver_t associated with the
    //    loaded DLL specified by hinstance.  If the driver hasn't
    //    been loaded yet, returns NULL.
    static
    Driver_t*
    Lookup(
        HINSTANCE    hinstance
        );

    //    If PdriverLookup fails, call PdriverLoad to load the given
    //    driver, initialize it, and insert it into the driver list.
    static
    Driver_t*
    Load1(
                HINSTANCE    hinstance,
        const    WCHAR*        pOutput,
        const    DEVMODEW*    pdevmodeInit
        );


    static
    Driver_t *
    GetDispDriverFromName(
        wchar_t * DeviceName
        );



    PFN_DrvBitBlt
    DrvBitBltFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvBitBlt;
    }



    DrvDeleteDeviceBitmap_t
    DrvDeleteDeviceBitmap(
        void
        ) const
    {
        return m_drvenabledata1.DrvDeleteDeviceBitmap;
    }



    HBITMAP
    DrvCreateDeviceBitmap(
        SIZEL sizl,
        ULONG iFormat
        )
    {
        return m_drvenabledata1.DrvCreateDeviceBitmap(m_dhpdev, sizl, iFormat);
    }



    DrvCreateDeviceBitmap_t
    DrvCreateDeviceBitmapFuncPtr(
        void
        ) const
    {
        return m_drvenabledata1.DrvCreateDeviceBitmap;
    }



    void
    DrvPowerHandler(
        BOOL    bOff
        ) const
    {
       if ( Surface().m_dhpdev )
          {
             m_drvenabledata1.DrvPowerHandler(Surface().m_dhpdev, bOff);
          }
        return;
    }


    void
    DrvMovePointer(
        int        X,
        int        Y
        )
    {
        m_drvenabledata1.DrvMovePointer(Surface().SURFOBJ_Ptr(), X, Y, NULL);
        return;
    }


    BOOL IsRotated() const;

protected:
    //  GdiMonitor_t is used to map an HMONITOR to a display driver.
    GdiMonitor_t*   m_pMonitor;

private:

   static
   bool
   InitMainDriver(
                  HINSTANCE   hinstance,
                  LPCTSTR     pwszDriverName,
                  LPCTSTR     pwszMainDisplayInstance
                 );

   static
    BOOL
    FindMainDriver(
        LPTSTR    pszNameBuf,
        DWORD    dwNameBufSize,
        LPTSTR    pszInstanceBuf,
        DWORD    dwInstanceBufSize
        );

    void
    DeleteDriverDevModes(
        DEVMODEW*    pDevModeAlloc
        ) const;

    bool
    InitializeDevModeFromDriver(
        HANDLE    hDriver
        );

    bool
    InitializeDevModeForCreateDc(
                HANDLE        hDriver,
        const    DEVMODEW*    pInitialDevMode
        );



    bool
    Initialize(
              HINSTANCE hinstance,
        const WCHAR*    pOutput,
        const DEVMODEW*    pdevmodeInit
        );

    void
    InitializeDeviceCaps(
        const    GDIINFO&
        );    

    ~Driver_t(
        void
        );

    DWORD m_ExclusiveProcess;

    HINSTANCE    m_hinstDriverDll;
    int            m_refcnt;        //    Refcount on this driver

    HWND  m_ExclusiveWindow;

    // Last realized palette cache. Used in GetSystemPaletteEntries and
    // in optimization of palette realization. For paltforms that do not
    // support palette realization (!(m_devcaps.m_RASTERCAPS & RC_PALETTE))
    // this member is not initialized.
    COLORTBL_CACHE m_colortbl_cache;

    // These members describe the physical surface owned by the driver.
    HRGN m_hrgnSysClip;
};



class MainDisplayDriver_t
{

public:

    static
    int
    LogicalPixelsY(
        void
        );



};


// Iterator for all loaded drivers.  Usage:
//
//      Driver_t * pdriver;
//      ForAllDrivers(pdriver)
//      {
//          // Do something with pdriver
//      }
#define ForAllDrivers(pdriver)  \
    for (pdriver = g_pdriverMain; pdriver; pdriver = pdriver->m_pdriverNext)

extern Driver_t* g_pdriverMain;
extern Driver_t* g_pdriverCurrent;

extern int g_cMonitorsExtra;

//    These two functions are used to find out what text capabilities
//    are supported by the system.
//REVIEW these don't belong here.
DWORD dwGetTextCaps();
DWORD dwGetTextCapsItalic();

// TestCooperativeLevel return values.

#define COOPLVL_PROCOWNSEXCLUSIVE     0
#define COOPLVL_EXCLUSIVEALREADYOWNED 1
#define COOPLVL_NORMALMODE            2

#endif

