//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include "precomp.h"

#ifdef DEBUG
#ifndef HAL_ZONE_INIT
#define HAL_ZONE_INIT     GPE_ZONE_INIT
#endif
#endif


DWORD WINAPI HalGetDriverInfo(LPDDHAL_GETDRIVERINFODATA lpInput)
{
    DWORD dwSize;
	DEBUGMSG(HAL_ZONE_INIT,(TEXT("GetDriverInfo invoked !!\r\n")));

    lpInput->ddRVal = DDERR_CURRENTLYNOTAVAIL;

    if (IsEqualIID(lpInput->guidInfo, GUID_KernelCallbacks) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_KernelCallbacks\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDKERNELCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDKERNELCALLBACKS);

        memcpy(lpInput->lpvData, &KernelCallbacks, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_HALMemory) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_KernelCallbacks\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDHALMEMORYCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDHALMEMORYCALLBACKS);

        memcpy(lpInput->lpvData, &HalMemoryCallbacks, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_KernelCaps))
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_KernelCaps\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDKERNELCAPS));
        lpInput->dwActualSize = sizeof(DDHAL_DDKERNELCALLBACKS);

        memcpy(lpInput->lpvData, &KernelCaps, dwSize );
        lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_ColorControlCallbacks) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_ColorControlCallbacks\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDCOLORCONTROLCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDCOLORCONTROLCALLBACKS);

        memcpy(lpInput->lpvData, &ColorControlCallbacks, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_MiscellaneousCallbacks) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_MiscellaneousCallbacks\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDMISCELLANEOUSCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDMISCELLANEOUSCALLBACKS);

        memcpy(lpInput->lpvData, &MiscellaneousCallbacks, dwSize);
        lpInput->ddRVal = DD_OK;
    }
#ifdef VIDEOPORT_SUPPORT
    else if (IsEqualIID(lpInput->guidInfo, GUID_VideoPortCallbacks) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_VideoPortCallbacks\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDVIDEOPORTCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDVIDEOPORTCALLBACKS);

        memcpy(lpInput->lpvData, &VideoPortCallbacks, dwSize);
        lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_VideoPortCaps) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_VideoPortCaps\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDVIDEOPORTCAPS));
        lpInput->dwActualSize = sizeof(DDVIDEOPORTCAPS);

        memcpy(lpInput->lpvData, &VideoPortCaps, dwSize);
        lpInput->ddRVal = DD_OK;
    }
#endif //VIDEOPORT_SUPPORT
#if 0
    else if (IsEqualIID(lpInput->guidInfo, GUID_NonLocalVidMemCaps) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_NonLocalVidMemCaps\r\n")));
        dwSize = min(lpInput->dwExpectedSize, sizeof(DDHAL_DDCOLORCONTROLCALLBACKS));
        lpInput->dwActualSize = sizeof(DDHAL_DDCOLORCONTROLCALLBACKS);

        memcpy(lpInput->lpvData, &VideoPortCaps, dwSize);
        lpInput->ddRVal = DD_OK;
    }
#endif //0
#ifdef DIRECT3D_SUPPORT
    else if (IsEqualIID(lpInput->guidInfo, GUID_D3DCallbacks2) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_D3DCallbacks2\r\n")));
		D3DHAL_CALLBACKS2 D3DCallbacks2;
		memset(&D3DCallbacks2, 0, sizeof(D3DCallbacks2));

		dwSize = min(lpInput->dwExpectedSize, sizeof(D3DHAL_CALLBACKS2));
		lpInput->dwActualSize = sizeof(D3DHAL_CALLBACKS2);

		D3DCallbacks2.dwSize = dwSize;
		// set up D3DCallbacks2 elements

		memcpy(lpInput->lpvData, &D3DCallbacks2, dwSize);
		lpInput->ddRVal = DD_OK;
    }
    else if (IsEqualIID(lpInput->guidInfo, GUID_D3DExtendedCaps) )
    {
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("GUID_D3DExtendedCaps\r\n")));
		D3DHAL_D3DEXTENDEDCAPS ec;
		memset(&ec, 0, sizeof(ec));
		dwSize = min(lpInput->dwExpectedSize, sizeof(D3DHAL_D3DEXTENDEDCAPS));
		ec.dwSize = sizeof(ec);

		// set up ec elements
		
		memcpy(lpInput->lpvData, &ec, dwSize);
		lpInput->ddRVal = DD_OK;
    }
#endif // DIRECT3D_SUPPORT

	if( lpInput->ddRVal != DD_OK )
	{
		DEBUGMSG(HAL_ZONE_INIT,(TEXT("HalGetDriverInfo: Currently not available\r\n")));
	}

    return DDHAL_DRIVER_HANDLED;
}


//////////////////////////// DDHAL_DDCALLBACKS ////////////////////////////

DWORD WINAPI HalDestroyDriver( LPDDHAL_DESTROYDRIVERDATA pd )
{
	DEBUGENTER( HalDestroyDriver );
	/*
	typedef struct _DDHAL_DESTROYDRIVERDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;   		// driver struct
	    HRESULT                     ddRVal; 		// return value
	    LPDDHAL_DESTROYDRIVER       DestroyDriver;  // PRIVATE: ptr to callback
	} DDHAL_DESTROYDRIVERDATA;
	*/
	DDHAL_FLIPTOGDISURFACEDATA d;

	DDGPEFlipToGDISurface( &d );

	pd->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

// not to be confused with
// DWORD WINAPI HalSetColorKey( LPDDHAL_SETCOLORKEYDATA pd )
DWORD WINAPI HalSetColorKey( LPDDHAL_DRVSETCOLORKEYDATA pd )
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
	
	pd->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetMode( LPDDHAL_SETMODEDATA pd )
{
	DEBUGENTER( HalSetMode );
	/*
	typedef struct _DDHAL_SETMODEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL lpDD;               // driver struct
	    DWORD               dwModeIndex;    		// new mode
	    HRESULT             ddRVal;         		// return value
	    LPDDHAL_SETMODE     SetMode;        		// PRIVATE: ptr to callback
	    BOOL                inexcl;         		// in exclusive mode
	} DDHAL_SETMODEDATA;
	*/

	// implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_NOTHANDLED;	// We only support 1 mode
}

DWORD WINAPI HalWaitForVerticalBlank( LPDDHAL_WAITFORVERTICALBLANKDATA pd )
{
	DEBUGENTER( HalWaitForVerticalBlank );
	/*
	typedef struct _DDHAL_WAITFORVERTICALBLANKDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL	lpDD;           	// driver struct
	    DWORD					dwFlags;        	// flags
	    DWORD					bIsInVB;        	// is in vertical blank
	    DWORD					hEvent;         	// event
	    HRESUL					ddRVal;				// return value
	    LPDDHAL_WAITFORVERTICALBLANK WaitForVerticalBlank;
	    											// PRIVATE: ptr to callback
	} DDHAL_WAITFORVERTICALBLANKDATA;
	*/

	WAIT_FOR_VBLANK;

	pd->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalGetScanLine( LPDDHAL_GETSCANLINEDATA pd )
{
	DEBUGENTER( HalGetScanLine );
	/*
	typedef struct _DDHAL_GETSCANLINEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    DWORD                       dwScanLine;     // returned scan line
	    HRESULT                     ddRVal;         // return value
	    LPDDHAL_GETSCANLINE         GetScanLine;    // PRIVATE: ptr to callback
	} DDHAL_GETSCANLINEDATA;
	*/
	pd->ddRVal = DDERR_UNSUPPORTED;
	return DDHAL_DRIVER_HANDLED;
}

DWORD WINAPI HalSetExclusiveMode( LPDDHAL_SETEXCLUSIVEMODEDATA pd )
{
	DEBUGENTER( HalSetExclusiveMode );
	/*typedef struct _DDHAL_SETEXCLUSIVEMODEDATA
	{
		LPDDRAWI_DIRECTDRAW_GBL    lpDD;             // driver struct
		DWORD                      dwEnterExcl;      // TRUE if entering exclusive mode, FALSE is leaving
		DWORD                      dwReserved;       // reserved for future use
		HRESULT                    ddRVal;           // return value
		LPDDHAL_SETEXCLUSIVEMODE   SetExclusiveMode; // PRIVATE: ptr to callback
	} DDHAL_SETEXCLUSIVEMODEDATA;*/

	// implementation
	pd->ddRVal = DD_OK;

	return DDHAL_DRIVER_HANDLED;
}

