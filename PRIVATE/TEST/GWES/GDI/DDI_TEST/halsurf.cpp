//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include "precomp.h"


DWORD WINAPI HalFlipToGDISurface( LPDDHAL_FLIPTOGDISURFACEDATA pd )
{
	DEBUGENTER( HalFlipToGDISurface );
	/*
	typedef struct _DDHAL_FLIPTOGDISURFACEDATA
	{
		LPDDRAWI_DIRECTDRAW_GBL    lpDD;             // driver struct
		DWORD                      dwToGDI;          // TRUE if flipping to the GDI surface, FALSE if flipping away
		DWORD                      dwReserved;       // reserved for future use
		HRESULT                    ddRVal;           // return value
		LPDDHAL_FLIPTOGDISURFACE   FlipToGDISurface; // PRIVATE: ptr to callback
	} DDHAL_FLIPTOGDISURFACEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEFlipToGDISurface(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalCreateSurface( LPDDHAL_CREATESURFACEDATA pd )
{
	DEBUGENTER( HalCreateSurface );
	/*
	typedef struct _DDHAL_CREATESURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDSURFACEDESC             lpDDSurfaceDesc;// description of surface being created
	    LPDDRAWI_DDRAWSURFACE_LCL   FAR *lplpSList; // list of created surface objects
	    DWORD                       dwSCnt;         // number of surfaces in SList
	    HRESULT                     ddRVal;         // return value
	    LPDDHAL_CREATESURFACE       CreateSurface;  // PRIVATE: ptr to callback
	} DDHAL_CREATESURFACEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPECreateSurface(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

//////////////////////////// DDHAL_DDEXEBUFCALLBACKS ////////////////////////////

DWORD WINAPI HalCanCreateSurface( LPDDHAL_CANCREATESURFACEDATA pd )
{
	DEBUGENTER( HalCanCreateSurface );
	/*
	typedef struct _DDHAL_CANCREATESURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL	lpDD;				// driver struct
	    LPDDSURFACEDESC			lpDDSurfaceDesc;	// description of surface being created
	    DWORD					bIsDifferentPixelFormat;
	    											// pixel format differs from primary surface
	    HRESULT					ddRVal;				// return value
	    LPDDHAL_CANCREATESURFACE	CanCreateSurface;
	    											// PRIVATE: ptr to callback
	} DDHAL_CANCREATESURFACEDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalCanCreateExecuteBuffer( LPDDHAL_CANCREATESURFACEDATA pd )
{
	DEBUGENTER( HalCanCreateExecuteBuffer );
	/*
	typedef struct _DDHAL_CANCREATESURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL	lpDD;				// driver struct
	    LPDDSURFACEDESC			lpDDSurfaceDesc;	// description of surface being created
	    DWORD					bIsDifferentPixelFormat;
	    											// pixel format differs from primary surface
	    HRESULT					ddRVal;				// return value
	    LPDDHAL_CANCREATESURFACE	CanCreateSurface;
	    											// PRIVATE: ptr to callback
	} DDHAL_CANCREATESURFACEDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalCreateExecuteBuffer( LPDDHAL_CREATESURFACEDATA pd )
{
	DEBUGENTER( HalCreateExecuteBuffer );
	/*
	typedef struct _DDHAL_CREATESURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDSURFACEDESC             lpDDSurfaceDesc;// description of surface being created
	    LPDDRAWI_DDRAWSURFACE_LCL   FAR *lplpSList; // list of created surface objects
	    DWORD                       dwSCnt;         // number of surfaces in SList
	    HRESULT                     ddRVal;         // return value
	    LPDDHAL_CREATESURFACE       CreateSurface;  // PRIVATE: ptr to callback
	} DDHAL_CREATESURFACEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPECreateExecuteBuffer(pd);

	// Check/Log out parameters

	return dwRet;
    
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}


DWORD WINAPI HalDestroyExecuteBuffer( LPDDHAL_DESTROYSURFACEDATA pd )
{
	DEBUGENTER( HalDestroyExecutebuffer );
	/*
	typedef struct _DDHAL_DESTROYSURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_DESTROYSURFACE DestroySurface;// PRIVATE: ptr to callback
		BOOL						fDestroyGlobal;
	} DDHAL_DESTROYSURFACEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEDestroyExecuteBuffer(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalLock( LPDDHAL_LOCKDATA pd )
{
	DEBUGENTER( HalLock );
	/*
	typedef struct _DDHAL_LOCKDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    DWORD                       bHasRect;       // rArea is valid
	    RECTL                       rArea;          // area being locked
	    LPVOID                      lpSurfData;     // pointer to screen memory (return value)
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_LOCK          Lock;           // PRIVATE: ptr to callback
	} DDHAL_LOCKDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPELock(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalUnlock( LPDDHAL_UNLOCKDATA pd )
{
	DEBUGENTER( HalUnlock );
	/*
	typedef struct _DDHAL_UNLOCKDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    HRESULT                     ddRVal;			// return value
	    LPDDHALSURFCB_UNLOCK        Unlock;         // PRIVATE: ptr to callback
	} DDHAL_UNLOCKDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEUnlock(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

//////////////////////////// DDHAL_DDSURFACECALLBACKS ////////////////////////////

DWORD WINAPI HalDestroySurface( LPDDHAL_DESTROYSURFACEDATA pd )
{
	DEBUGENTER( HalDestroySurface );
	/*
	typedef struct _DDHAL_DESTROYSURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_DESTROYSURFACE DestroySurface;// PRIVATE: ptr to callback
		BOOL						fDestroyGlobal;
	} DDHAL_DESTROYSURFACEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEDestroySurface(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetSurfaceDesc(LPDDHAL_HALSETSURFACEDESCDATA pd)
{
	DEBUGENTER( HalSetSurfaceDesc );
	/*
		typedef struct _DDHAL_HALSETSURFACEDESCDATA
		{
		    DWORD       	dwSize;             	// Size of this structure
		    LPDDRAWI_DDRAWSURFACE_LCL  lpDDSurface; // Surface
			LPDDSURFACEDESC	lpddsd;					// Description of surface
		    HRESULT     	ddrval;
		} DDHAL_HALSETSURFACEDESCDATA;
	*/

	// Implementation
	pd->ddrval = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalFlip( LPDDHAL_FLIPDATA pd )
{
	DEBUGENTER( HalFlip );
	/*
	typedef struct _DDHAL_FLIPDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpSurfCurr;     // current surface
	    LPDDRAWI_DDRAWSURFACE_LCL   lpSurfTarg;     // target surface (to flip to)
	    DWORD                       dwFlags;        // flags
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_FLIP          Flip;           // PRIVATE: ptr to callback
	} DDHAL_FLIPDATA;
	*/

	if( IS_BUSY )
	{
		if( pd->dwFlags & DDFLIP_WAIT )
		{
			WAIT_FOR_NOT_BUSY;
		}
		else
		{
			DEBUGMSG(GPE_ZONE_FLIP,(TEXT("Graphics engine busy\r\n")));
			pd->ddRVal = DDERR_WASSTILLDRAWING;
			return DDHAL_DRIVER_HANDLED;
		}
	}

	//////
	//WAIT_FOR_VBLANK;
	//////

	DDGPEFlip(pd);


	//RETAILMSG(1,(TEXT("Flip!\r\n")));
	DEBUGMSG(GPE_ZONE_FLIP,(TEXT("Flip done\r\n")));

	pd->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetClipList( LPDDHAL_SETCLIPLISTDATA pd )
{
	DEBUGENTER( HalSetClipList );
	/*
	typedef struct _DDHAL_SETCLIPLISTDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_SETCLIPLIST   SetClipList;    // PRIVATE: ptr to callback
	} DDHAL_SETCLIPLISTDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}


DWORD WINAPI HalBlt( LPDDHAL_BLTDATA pd )
{
	DEBUGENTER( HalBlt );
	/*
	typedef struct _DDHAL_BLTDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDDestSurface;// dest surface
	    RECTL                       rDest;          // dest rect
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSrcSurface; // src surface
	    RECTL                       rSrc;           // src rect
	    DWORD                       dwFlags;        // blt flags
	    DWORD                       dwROPFlags;     // ROP flags (valid for ROPS only)
	    DDBLTFX                     bltFX;          // blt FX
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_BLT           Blt;            // PRIVATE: ptr to callback
	} DDHAL_BLTDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEBlt(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

// not to be confused with
// DWORD WINAPI HalSetColorKey( LPDDHAL_DRVSETCOLORKEYDATA pd )
DWORD WINAPI HalSetColorKey( LPDDHAL_SETCOLORKEYDATA pd )
{
	DEBUGENTER( HalSetColorKey );
	/*
	typedef struct _DDHAL_DRVSETCOLORKEYDATA
	{
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    DWORD                       dwFlags;        // flags
	    DDCOLORKEY                  ckNew;          // new color key
	    HRESULT                     ddRVal;         // return value
	    LPDDHAL_SETCOLORKEY         SetColorKey;    // PRIVATE: ptr to callback
	} DDHAL_DRVSETCOLORKEYDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPESetColorKey(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalAddAttachedSurface( LPDDHAL_ADDATTACHEDSURFACEDATA pd )
{
	DEBUGENTER( HalAddAttachedSurface );
	/*
	typedef struct _DDHAL_ADDATTACHEDSURFACEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL		lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL	lpDDSurface;    // surface struct
	    LPDDRAWI_DDRAWSURFACE_LCL	lpSurfAttached; // surface to attach
	    HRESULT						ddRVal;         // return value
	    LPDDHALSURFCB_ADDATTACHEDSURFACE AddAttachedSurface;
	    											// PRIVATE: ptr to callback
	} DDHAL_ADDATTACHEDSURFACEDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalGetBltStatus( LPDDHAL_GETBLTSTATUSDATA pd )
{
	DEBUGENTER( HalGetBltStatus );
	/*
	typedef struct _DDHAL_GETBLTSTATUSDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    DWORD                       dwFlags;        // flags
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_GETBLTSTATUS  GetBltStatus;   // PRIVATE: ptr to callback
	} DDHAL_GETBLTSTATUSDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalGetFlipStatus( LPDDHAL_GETFLIPSTATUSDATA pd )
{
	DEBUGENTER( HalGetFlipStatus );
	/*
	typedef struct _DDHAL_GETFLIPSTATUSDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    DWORD                       dwFlags;        // flags
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_GETFLIPSTATUS GetFlipStatus;  // PRIVATE: ptr to callback
	} DDHAL_GETFLIPSTATUSDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEGetFlipStatus(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalUpdateOverlay( LPDDHAL_UPDATEOVERLAYDATA pd )
{
	DEBUGENTER( HalUpdateOverlay );
	/*
//	typedef struct _DDHAL_UPDATEOVERLAYDATA
//	{
//	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
//	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDDestSurface;// dest surface
//	    RECTL                       rDest;          // dest rect
//	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSrcSurface; // src surface
//	    RECTL                       rSrc;           // src rect
//	    DWORD                       dwFlags;        // flags
//	    DDOVERLAYFX                 overlayFX;      // overlay FX
//	    HRESULT                     ddRVal;         // return value
//	    LPDDHALSURFCB_UPDATEOVERLAY UpdateOverlay;  // PRIVATE: ptr to callback
//	} DDHAL_UPDATEOVERLAYDATA;
//	*/
//

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetOverlayPosition( LPDDHAL_SETOVERLAYPOSITIONDATA pd )
{
	DEBUGENTER( HalSetOverlayPosition );
	/*
	typedef struct _DDHAL_SETOVERLAYPOSITIONDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSrcSurface; // src surface
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDDestSurface;// dest surface
	    LONG                        lXPos;          // x position
	    LONG                        lYPos;          // y position
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_SETOVERLAYPOSITION SetOverlayPosition;
	    											// PRIVATE: ptr to callback
	} DDHAL_SETOVERLAYPOSITIONDATA;
	*/

	// Implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetPalette( LPDDHAL_SETPALETTEDATA pd )
{
	DEBUGENTER( HalSetPalette );
	/*
	typedef struct _DDHAL_SETPALETTEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSurface;    // surface struct
	    LPDDRAWI_DDRAWPALETTE_GBL   lpDDPalette;    // palette to set to surface
	    HRESULT                     ddRVal;         // return value
	    LPDDHALSURFCB_SETPALETTE    SetPalette;     // PRIVATE: ptr to callback
	    BOOL                        Attach;         // attach this palette?
	} DDHAL_SETPALETTEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPESetPalette(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}
