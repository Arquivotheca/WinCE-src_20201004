//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include "precomp.h"


DWORD WINAPI HalCreatePalette( LPDDHAL_CREATEPALETTEDATA pd )
{
	/*
	typedef struct _DDHAL_CREATEPALETTEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWPALETTE_GBL   lpDDPalette;    // ddraw palette struct
	    LPPALETTEENTRY              lpColorTable;   // colors to go in palette
	    HRESULT                     ddRVal;         // return value
	    LPDDHAL_CREATEPALETTE       CreatePalette;  // PRIVATE: ptr to callback
	    BOOL                        is_excl;        // process has exclusive mode
	} DDHAL_CREATEPALETTEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPECreatePalette(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

//////////////////////////// DDHAL_DDPALETTECALLBACKS ////////////////////////////

DWORD WINAPI HalDestroyPalette( LPDDHAL_DESTROYPALETTEDATA pd )
{
	DEBUGENTER( HalDestroyPalette );
	/*
	typedef struct _DDHAL_DESTROYPALETTEDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWPALETTE_GBL   lpDDPalette;    // palette struct
	    HRESULT                     ddRVal;         // return value
	    LPDDHALPALCB_DESTROYPALETTE DestroyPalette; // PRIVATE: ptr to callback
	} DDHAL_DESTROYPALETTEDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPEDestroyPalette(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}


DWORD WINAPI HalSetEntries( LPDDHAL_SETENTRIESDATA pd )
{
	DEBUGENTER( HalSetEntries );
	/*
	typedef struct _DDHAL_SETENTRIESDATA
	{
	    LPDDRAWI_DIRECTDRAW_GBL     lpDD;           // driver struct
	    LPDDRAWI_DDRAWPALETTE_GBL   lpDDPalette;    // palette struct
	    DWORD                       dwBase;         // base palette index
	    DWORD                       dwNumEntries;   // number of palette entries
	    LPPALETTEENTRY              lpEntries;      // color table
	    HRESULT                     ddRVal;         // return value
	    LPDDHALPALCB_SETENTRIES     SetEntries;     // PRIVATE: ptr to callback
	} DDHAL_SETENTRIESDATA;
	*/

	// Implementation
	// Check/Log in parameters
	
	DWORD dwRet = DDGPESetEntries(pd);

	// Check/Log out parameters

	return dwRet;
	
	//pd->ddRVal = DD_OK;
	//return DDHAL_DRIVER_HANDLED;
}

