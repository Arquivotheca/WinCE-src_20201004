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

#ifndef __DRIVER_HPP_INCLUDED__
#define __DRIVER_HPP_INCLUDED__

#include "GdiObj.h"


//	Used to cache info about current system palette
class  COLORTBL_CACHE
{
private:

	// Last realized logical palette. This data member is used only in palette
	// realization optimization - to find out whether realization is NOP or it
	// really has to be done.
	HPALETTE	m_hpalLog;

	// Number of entries in the pEntries aray, equals to the number of
	// entries in the driver's default palette (even if last realized palette
	// had different number of entries).
	USHORT           m_cEntries;

	// Array that reflects current status of the system (hardware) palette.
	// Might be not the same as last realized palette if that palette had less
	// entries than driver's default palette. In this case first X entries of
	// m_pEntries are overwritten. (X - size of the last realized palette).
	PALETTEENTRY    *m_pEntries;


public:

	//	Called once per driver initialization.
	//	Cache is init-ed with driver's default palette values.
	bool
	FInit(
		HPALETTE		hpal,
		ColorTable_t*	pcolortbl
		);

	BOOL
	Update(
		Palette_t*	pPal,
		ULONG		cColors
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
	HINSTANCE	m_hinstance;	//	DLLs HINSTANCE when loaded
	int			m_refcnt;		//	Refcount on this driver

	//	Last realized palette cache. Used in GetSystemPaletteEntries and
	//	in optimization of palette realization. For paltforms that do not
	//	support palette realization (!(m_devcaps.m_RASTERCAPS & RC_PALETTE))
	//	this member is not initialized.
	COLORTBL_CACHE	m_colortbl_cache;

	//	These members describe the physical surface owned by the driver.
	HRGN			m_hrgnSysClip;

	void
	CacheDeviceCaps(
		GDIINFO*
		);

    ~Driver_t(
		void
		);

public:

	DRVENABLEDATA	m_drvenabledata;	//	Entry points into driver
	DHPDEV			m_dhpdev;			//	Handle to physical device
	SURFOBJ			m_surfobj;			//	Driver's primary surface
	WBITMAP*		m_pwbitmap;

	//	Points to the ColorTable_t the driver provided in the call to
	//	EngCreatePalette on startup.
	ColorTable_t*	m_pcolortbl;

	HPALETTE		m_hpalette;			//	Device's default palette
	ULONG			m_flGraphicsCaps;	//	Driver's graphics capabilites flags
	Driver_t*	    m_pdriverNext;		//	Link to next loaded driver
	long            m_iDriver;           // for multimon, if it is SINGLE_DRIVER_HANDLE, the driver is not in multimon system           

public:
	Driver_t(
		void
		);

	//	Every time a DC is created or destroyed, that DC's driver
	//	must be notified via these methods.  When a driver's refcount
	//	hits zero, the driver is automatically unloaded.
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
	OwnOrStockColorTable(
		unsigned long	BitmapFormat
		);

	ColorTable_t*
	PaletteColorTable(
		void
		);


	bool
	ColorTableIsSame(
		ColorTable_t*	pColorTable
		);
//    if (pDc->m_pdriver->Pcolortbl() == pcolortbl)
//        bSamePixelFormat = TRUE;

	LONG 
	SetRotateParams(void);
	
//REVIEW should just have one function to create/load driver given the dll name.
	//	Returns a pointer to the Driver_t associated with the
	//	loaded DLL specified by hinstance.  If the driver hasn't
	//	been loaded yet, returns NULL.
	static
	Driver_t*
	Lookup(
		HINSTANCE	hinstance
		);


	//	If PdriverLookup fails, call PdriverLoad to load the given
	//	driver, initialize it, and insert it into the driver list.
	static
	Driver_t*
	Load(
				HINSTANCE	hinstance,		//	Driver's DLL
        const	WCHAR*		pOutput,		//	Port/file name
		const	DEVMODEW*	pdevmodeInit	//	Optional init data (may be NULL)
        );


	// Returns TRUE iff this driver is for a printer.  Note that
	// we consider plotters to be printers.
    BOOL
	IsPrint(
		void
		);
//{
//	return	   m_devcaps.m_TECHNOLOGY == DT_RASPRINTER
//			|| m_devcaps.m_TECHNOLOGY == DT_PLOTTER;
//}

	//	Does driver support palette realization
    BOOL
	FSettableHwPal(
		void
		);
//	{
//		return(m_devcaps.m_RASTERCAPS & RC_PALETTE);
//	}

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
//	{
//		return &m_colortbl_cache;
//	}

	//	The DEVCAPS members are used to satisfy calls to GetDeviceCaps.
	//	For example, the return value for GetDeviceCaps(hdc, HORZSIZE)
	//	is stored in hdc.m_driver.m_devcaps.m_HORZSIZE
    struct DEVCAPS
    {
        // REVIEW Vladimir: We can delete any of these if we also
        // kill the corresponding macro definition (DRIVERVERSION, etc.)
        // in wingdi.h.  We need to evaluate exactly which GetDeviceCaps
        // values we need to support.
		//REVIEW why do we have this completely new structure since most
		//of it is the same as GDIINFO?
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

        DWORD m_PHYSICALWIDTH;      // For printers
        DWORD m_PHYSICALHEIGHT;     // For printers
        DWORD m_PHYSICALOFFSETX;    // For printers
        DWORD m_PHYSICALOFFSETY;    // For printers

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
        DWORD m_FRAMEBUFFER;
    } m_devcaps;

	DEVMODE m_devMode;
    
protected:
bool
	FInit(
				HINSTANCE	hinstance,		//	Driver's DLL
		const	WCHAR*		pOutput,		//	Port/file name
		const	DEVMODEW*	pdevmodeInit	//	Optional init data (may be NULL)
		);

};

/*
	Disp_Driver encapsulates the display driver
*/
class Disp_Driver : public  Driver_t
{
	public:
		Disp_Driver();
		~Disp_Driver();

	static
	bool
	InitMainDriver(
		HINSTANCE	hinstance
		);

	static 
	Disp_Driver *
	PPrimaryMonitor();

	LONG 
	SetRotateParams();
	
	BOOL 
	IsPrimary();

	Disp_Driver *m_pDisplayNext;
	RECT           m_rcMonitor;
	RECT           m_rcWork;
	
	private:
	static
	BOOL
	FindMainDriver(
	        LPTSTR pszNameBuf,
	        DWORD dwNameBufSize,
               LPTSTR pszInstanceBuf,
               DWORD dwInstanceBufSize
	        );
	bool
	FInit(
		HINSTANCE	hinstance,		//	Driver's DLL
		const	WCHAR*		pOutput,		//	Port/file name
		const	DEVMODEW*	pdevmodeInit	//	Optional init data (may be NULL)
		);
};

// Iterator for all loaded drivers.  Usage:
//
//      PDRIVER pdriver;
//      ForAllDrivers(pdriver)
//      {
//          // Do something with pdriver
//      }
#define ForAllDrivers(pdriver)  \
    for (pdriver = g_pdriverMain; pdriver; pdriver = pdriver->m_pdriverNext)

extern Disp_Driver* g_pdriverMain;

extern int g_cMonitorsExtra;

//	Currently being loaded driver, needed in EngCreatePalette
extern Driver_t *g_pdriverCurrent;


//	These two functions are used to find out what text capabilities
//	are supported by the system.
//REVIEW these don't belong here.
DWORD dwGetTextCaps();
DWORD dwGetTextCapsItalic();

#endif

