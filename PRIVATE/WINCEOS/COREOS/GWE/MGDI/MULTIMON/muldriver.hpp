//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:
    multimon.hpp

Abstract:

    Multi monitor structures and defines

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#ifndef __MULTIDRIVER__INCLUDED__
#define __MULTIDRIVER__INCLUDED__

//Define the maximum number of monitors supported is 4!!
//This number should be updated accordingly in GDI if ever changed! 
#define SURF_TYPE_MULTI 	2L

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
first to build up a simplified multi monitor system based on the following 
 design or implementation: 

 1. All the monitors share the same palette
 2. Memory bitmap bits is in system. 
 
 so the difference I should make for the multimon now is for WBITMAP, I gotta 
 have an array of struct to points different video surface.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

struct DISPSURF {
     DISPSURF  *pdsBltNext; //for screen to screen blt.
     ULONG     iDispSurf;   // the ith monitor in the system
     RECTL     rcl;
     SURFOBJ  *pso;
     HDEV      hdev;       // point to the display driver
};


struct DEVDATA
{
	DHPDEV  dhpdev;  //point to the physical device,i.e.,display driver object
	HDEV    hDev; 	 //point to the DRIVER object
	RECTL   rcl;
};

struct MULTIDEVMODE
{
	DEVMODEW devmodew;
	ulong    cDev;
	DEVDATA  dev[MONITORS_MAX];
};


// virtual display device, corresponding to a virtual GPE
class VDISPLAY : public GPE
{
public:
     DISPSURF   m_ds[MONITORS_MAX];
    DISPSURF   *pdsBlt;     // start of the DISPSURF to blit
     int        m_cSurfaces; // number of monitors in the system
     ULONG      m_flGraphicsCaps; 
     RECTL      m_rcVScreen; 
     ULONG      m_iBpp;
    // SURFOBJ  *m_pso;    //will point to the *main* surfobj, which
    // == m_hSurf 		  //is a surfobj with virtual screen dimension
     					  // dhsurf ==> &gpvdisplay->m_ds;
     					  // iType ==> SURF_TYPE_MULTI,i.e., a multi surface
     					  // dhpdev ==> gpvdisplay
     					  // hsurf  ==> this m_pso
     					  // hdev ==> g_driverMain
     					  // lDelta = 0; pvScan0 = 0;
     					  // sizlBitmap = {width, height}
    XCLIPOBJ  m_co;						
    HANDLE   hdriver;	 
    BYTE     byteBusyBits;
    
    VDISPLAY(MULTIDEVMODE *pMode, HANDLE hdriver);
    ~VDISPLAY();
    virtual SCODE BltPrepare(GPEBltParms * pBltParms);
    virtual SCODE BltComplete(GPEBltParms * pBltParms);
    virtual SCODE Line(GPELineParms *pLineParms,	EGPEPhase phase = gpeSingle);
    virtual SCODE AllocSurface(GPESurf * * ppSurf, int width, int height, EGPEFormat format, int surfaceFlags);
    virtual SCODE SetPointerShape(GPESurf * pMask, GPESurf * pColorSurf, int xHot, int yHot, int cx, int cy);
    virtual SCODE MovePointer(int x, int y);
    virtual SCODE SetPalette(const PALETTEENTRY *src, unsigned short firstEntry,unsigned short numEntries);
    virtual SCODE GetModeInfo(GPEMode *pMode, int modeNo);
    virtual int   NumModes();
    virtual SCODE SetMode(int modeId, HPALETTE * pPaletteHandle);
    virtual int    InVBlank();
    virtual int IsPaletteSettable() {return (m_iBpp == 8);}
    virtual void WaitForNotBusy();
    
    DISPSURF *PDispFromPoint(long x, long y);
    DISPSURF *GetBoundPointInScreen(long *px, long *py, long dirX, long dirY);
};


//helper struct to help iterate the data
class MSURF
{
private:
	RECTL    m_rclBoundsOrig;		//save rclBounds of CLIPOBJ passed in
        BYTE 	 m_iDComplexityOrig;            //save iDComplexity of CLIPOBJ passsed in
        RGNDAT  *m_pRgndatOrig;                     //save m_prgnd of CLIPOBJ passed in 
        RECTL 	 m_rclDraw;			//area affected by current drawing command
public:
	DISPSURF *m_pds;
	SURFOBJ  *m_pso;
	CLIPOBJ  *m_pco;

        BOOL bFindSurface(SURFOBJ *,CLIPOBJ *,RECTL *);
        BOOL bNextSurface();
};



extern VDISPLAY *GetVDisplay();

/******************************Public*Data*********************************\
* MIX translation table
*
* Translates a mix 1-16, into an old style Rop 0-255.
*
\**************************************************************************/
#endif //_MULTI_MON_
