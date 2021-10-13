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

#ifndef __PERFDDI_H__
#define __PERFDDI_H__

#ifdef ENABLE_PERFDDI


#include "perfdata.h"   // Need PERFDATA class


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Class:

    PERFDDI

Owner:

    AnthonyL

Description:

    PERFDDI does all kinds of sleazy stuff to let us measure the
    timer we're spending in the driver.

-------------------------------------------------------------------*/
class PERFDDI
{
    //
    // Since PERFDDI is completely static (all members and methods), don't
    // allow CTOR or DTOR to be called publically
    //
private:
    PERFDDI()  { }
    ~PERFDDI() { }
public:

    //
    // StartProfile and StopProfile used to start and stop the collection
    // of performance data.
    //
    static void StartProfile();
    static void StopProfile();

    //
    // This is where we dump the accumulated data
    //
    static void ShowRecords();

    //
    // Internal helper to delete the list of PERFDATA records
    //
    static void DeleteRecords();

    //
    // Method to add the given PERFDATA record to the static list
    //
    static void AddToList(PERFDATA *pperfdata);

    // 
    // Cache entry points into real driver
    //
    static DRVENABLEDATA s_drvenabledataRealDriver;

    //
    // Keep a DRVENABLEDATA that points at the PERFDDI methods.
    // This function table will be used to overwrite the function
    // table of the driver being analyzed.
    //
    static DRVENABLEDATA s_drvenabledataPerfDDI;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                  BEGIN PERFDDI DRVxxx FUNCTIONS                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

    static DHPDEV DrvEnablePDEV(
        DEVMODEW *pdm,
        LPWSTR    pwszLogAddress,
        ULONG     cPat,
        HSURF    *phsurfPatterns,
        ULONG     cjCaps,
        ULONG    *pdevcaps,
        ULONG     cjDevInfo,
        DEVINFO  *pdi,
        HDEV      hdev,
        LPWSTR    pwszDeviceName,
        HANDLE    hDriver
        );

    static VOID DrvDisablePDEV(
        DHPDEV dhpdev
        );

    static HSURF DrvEnableSurface(
        DHPDEV dhpdev
        );

    static VOID DrvDisableSurface(
        DHPDEV dhpdev
        );

    static HBITMAP DrvCreateDeviceBitmap(
        DHPDEV dhpdev,
        SIZEL sizl,
        ULONG iFormat
        );

    static VOID DrvDeleteDeviceBitmap(
        DHSURF dhsurf
        );

    static BOOL DrvRealizeBrush(
        BRUSHOBJ *pbo,
        SURFOBJ  *psoTarget,
        SURFOBJ  *psoPattern,
        SURFOBJ  *psoMask,
        XLATEOBJ *pxlo,
        ULONG    iHatch
        );

    static BOOL DrvStrokePath(
        SURFOBJ   *pso,
        PATHOBJ   *ppo,
        CLIPOBJ   *pco,
        XFORMOBJ  *pxo,
        BRUSHOBJ  *pbo,
        POINTL    *pptlBrushOrg,
        LINEATTRS *plineattrs,
        MIX        mix
        );

    static BOOL DrvFillPath(
        SURFOBJ  *pso,
        PATHOBJ  *ppo,
        CLIPOBJ  *pco,
        BRUSHOBJ *pbo,
        POINTL   *pptlBrushOrg,
        MIX       mix,
        FLONG     flOptions
        );

    static BOOL DrvPaint(
        SURFOBJ  *pso,
        CLIPOBJ  *pco,
        BRUSHOBJ *pbo,
        POINTL   *pptlBrushOrg,
        MIX       mix
        );

    static BOOL DrvBitBlt(
        SURFOBJ  *psoTrg,
        SURFOBJ  *psoSrc,
        SURFOBJ  *psoMask,
        CLIPOBJ  *pco,
        XLATEOBJ *pxlo,
        RECTL    *prclTrg,
        POINTL   *pptlSrc,
        POINTL   *pptlMask,
        BRUSHOBJ *pbo,
        POINTL   *pptlBrush,
        ROP4      rop4
        );

    static BOOL DrvCopyBits(
        SURFOBJ  *psoDest,
        SURFOBJ  *psoSrc,
        CLIPOBJ  *pco,
        XLATEOBJ *pxlo,
        RECTL    *prclDest,
        POINTL   *pptlSrc
        );

    static BOOL DrvAnyBlt(
        SURFOBJ         *psoDest,
        SURFOBJ         *psoSrc,
        SURFOBJ         *psoMask,
        CLIPOBJ         *pco,
        XLATEOBJ        *pxlo,
        POINTL          *pptlHTOrg,
        RECTL           *prclDest,
        RECTL           *prclSrc,
        POINTL          *pptlMask,
    	BRUSHOBJ        *pbo,
    	POINTL          *pptlBrush,
    	ROP4             rop4,
    	ULONG            iMode,
    	ULONG	         bltFlags
        );

    static BOOL DrvTransparentBlt(
        SURFOBJ         *psoDest,
        SURFOBJ         *psoSrc,
        CLIPOBJ         *pco,
        XLATEOBJ        *pxlo,
        RECTL           *prclDest,
        RECTL           *prclSrc,
        ULONG            TransColor
        );

    static BOOL DrvSetPalette(
        DHPDEV  dhpdev,
        PALOBJ *ppalo,
        FLONG   fl,
        ULONG   iStart,
        ULONG   cColors
        );

    static ULONG DrvSetPointerShape(
        SURFOBJ  *pso,
        SURFOBJ  *psoMask,
        SURFOBJ  *psoColor,
        XLATEOBJ *pxlo,
        LONG      xHot,
        LONG      yHot,
        LONG      x,
        LONG      y,
        RECTL    *prcl,
        FLONG     fl
        );

    static VOID DrvMovePointer(
        SURFOBJ *pso,
        LONG x,
        LONG y,
        RECTL *prcl
        );

    static ULONG DrvGetModes(
        HANDLE    hDriver,
        ULONG     cjSize,
        DEVMODEW *pdm
        );

    static ULONG DrvRealizeColor(
        USHORT     iDstType,
        ULONG      cEntries,
        ULONG     *pPalette,
        ULONG      rgbColor
        );
    
    static ULONG *DrvGetMasks(
        DHPDEV     dhpdev
        );
    
    static ULONG DrvUnrealizeColor(
        USHORT     iSrcType,
        ULONG      cEntries,
        ULONG     *pPalette,
        ULONG      iRealizedColor
        );

    static BOOL DrvContrastControl(
        DHPDEV     dhpdev,
        ULONG      cmd,
        ULONG     *pValue
        );
    
    static VOID DrvPowerHandler(
        DHPDEV     dhpdev,
        BOOL       bOff
        );
    
    static BOOL DrvEndDoc(
        IN SURFOBJ  *pso,   
        IN FLONG     fl    
        );

    static BOOL DrvStartDoc(
        IN SURFOBJ  *pso,   
        IN PWSTR     pwszDocName,  
        IN DWORD     dwJobId   
        );

    static BOOL DrvStartPage(
        IN SURFOBJ  *pso    
        );

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                   END PERFDDI DRVxxx FUNCTIONS                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


    //
    // Here's where we store the performance data for all drivers.
    // (We can't record data per driver because once we're in the DrvXXX
    // function, we have no idea what PERFDDI we're using.)
    //
    // We expect profiling runs to be relatively short, so we just
    // keep a list of PERFDATA records.  We can change this if
    // recording the data introduces too much drag.
    //
    static PERFDATA *s_pperfdataList;
};

typedef PERFDDI *PPERFDDI;

//
// g_fPerfDDI is TRUE when we're actively collecting data
//
extern FLAG g_fPerfDDI;


#endif  // ENABLE_PERFDDI
#endif  // !__PERFDDI_H__

