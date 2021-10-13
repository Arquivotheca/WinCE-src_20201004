//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    multimon.cpp

Abstract:
    
    APIs support multi-monitors in CE

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "precomp.h"

static   VDISPLAY *gpvdisplay = NULL;
static   DRVENABLEDATA gGPEDrvEnableData;


// This prototype avoids problems exporting from .lib
BOOL	APIENTRY	GPEEnableDriver(ULONG iEngineVersion, ULONG cj, DRVENABLEDATA *pded, PENGCALLBACKS pEngCallbacks);


/******************************Public*Data*********************************\
* MIX translation table
*
* Translates a mix 1-16, into an old style Rop 0-255.
*
\**************************************************************************/
//can we use the one in ddi_if.cpp?????

// extern BYTE gaMix[];

  const BYTE gaMix[] =
  {
      0xFF,  // R2_WHITE        - Allow rop = gaMix[mix & 0x0F]
      0x00,  // R2_BLACK
      0x05,  // R2_NOTMERGEPEN
      0x0A,  // R2_MASKNOTPEN
      0x0F,  // R2_NOTCOPYPEN
      0x50,  // R2_MASKPENNOT
      0x55,  // R2_NOT
      0x5A,  // R2_XORPEN
      0x5F,  // R2_NOTMASKPEN
      0xA0,  // R2_MASKPEN
      0xA5,  // R2_NOTXORPEN
      0xAA,  // R2_NOP
      0xAF,  // R2_MERGENOTPEN
      0xF0,  // R2_COPYPEN
      0xF5,  // R2_MERGEPENNOT
      0xFA,  // R2_MERGEPEN
      0xFF   // R2_WHITE        - Allow rop = gaMix[mix & 0xFF]
  };


GPE *GetGPE(void)
{
    // when called, it should be already initialized!! 
	ASSERT(gpvdisplay != NULL);
	return (GPE *)gpvdisplay;
}


VDISPLAY *GetVDisplay()
{
//should not be called before it is allocated.
	ASSERT(gpvdisplay != NULL);
	return gpvdisplay;
}

inline GPE *SurfobjToGPE( SURFOBJ *pso )
{
	return (GPE *)(pso->dhpdev);
}

int BPPToBMF(int bpp)
{
	switch (bpp)
	{
	case 1:
		return BMF_1BPP;
	case 2:
		return BMF_2BPP;
	case 4:
		return BMF_4BPP;
	case 8:
		return BMF_8BPP;
	case 16:
		return BMF_16BPP;
	case 24:
		return BMF_24BPP;
	case 32:
		return BMF_32BPP;
	default:
		return 0;
	}
}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
some helper functions
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
GetIntersectRect() 

  get intersection of two RECTL. return true if there is intersection, else 
  false.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL GetIntersectRect(RECTL *prc1, RECTL *prc2, RECTL *prcResult)
{
	prcResult->left = max(prc1->left, prc2->left);
	prcResult->right= min(prc1->right, prc2->right);
	if (prcResult->left < prcResult->right)
	{
		prcResult->top = max(prc1->top, prc2->top);
		prcResult->bottom = min(prc1->bottom, prc2->bottom);
		if (prcResult->top < prcResult->bottom)
			return TRUE;
	}
	return FALSE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CopyRgnDat()

	helper function for getting a clone of RGNDAT
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void CopyRgnDat(RGNDAT *pDst, RGNDAT *pSrc, int cjSize)
{
	CopyMemory((void *)pDst, (void *)pSrc, cjSize);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 ClipRgnDatClip()

  	helper function -- clip all the clip rects in RGNDAT
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void ClipRgnDatClip(RGNDAT *prgnd, RECTL *prcl)
{
	RECTCOUNT i, j;
    RECTS *prcStart;
	long left, right, top, bottom;

	left = top = LONG_MAX;
	right = bottom = LONG_MIN;
    prcStart = (RECTS *)prgnd->Buffer;

    for (i= 0, j = 0; i < prgnd->rdh.nCount; i ++)
    {
        if ( (prcStart[i].left > prcl->right) ||
        	 (prcStart[i].right < prcl->left) ||
        	 (prcStart[i].top > prcl->bottom) ||
        	 (prcStart[i].bottom < prcl->top) )
        	{
 				continue;       		
        	}
        	
        if (prcStart[i].left < prcl->left)
        	prcStart[j].left = (INT16)prcl->left;
        else 
        	prcStart[j].left = prcStart[i].left;
		if (prcStart[j].left < left)
			left = prcStart[j].left;
			
        if (prcStart[i].right > prcl->right)
        	prcStart[j].right = (INT16)prcl->right;
        else
        	prcStart[j].right = prcStart[i].right;
		if (prcStart[j].right > right)
			right = prcStart[j].right;
			
        if (prcStart[i].top < prcl->top)
        	prcStart[j].top = (INT16)prcl->top;
        else 
        	prcStart[j].top = prcStart[i].top;
		if (prcStart[j].top < top)
			top = prcStart[j].top;
			
        if (prcStart[i].bottom > prcl->bottom )
        	prcStart[j].bottom = (INT16)prcl->bottom;
        else 
        	prcStart[j].bottom = prcStart[i].bottom;
		if (prcStart[j].bottom > bottom)
			bottom = prcStart[j].bottom;
			
       	j ++;
     }

     //dont need to check if j == 0 because this function is only called in bFindSurface() 
     //and bNextSurface(). before calling, it checks it must have an overlap. 
     prgnd->rdh.nCount = j;
     prgnd->rdh.rcBound.left = (INT16)left;
     prgnd->rdh.rcBound.right = (INT16)right;
     prgnd->rdh.rcBound.top = (INT16)top;
     prgnd->rdh.rcBound.bottom = (INT16)bottom;

     for (; j < i; j ++)
     {
     	prcStart[j].left = 0;
     	prcStart[j].top = 0;
     	prcStart[j].right = prcStart[j].bottom = 0;
     	
     }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	MSURF::bFindSurface()

	Helper function to find the first surface that intersect/contains the prclDraw.
	return FALSE if no surface ever intersect with prclDraw

	psoOrig ==> surfobj that is of SURF_TYPE_MULTI
	pcoOrig ==> the CLIPOBJ 
	prclDraw ==> the drawing area
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

BOOL MSURF::bFindSurface(SURFOBJ *psoOrig, CLIPOBJ *pcoOrig, RECTL *prclDraw)
{
	VDISPLAY *pvd = (VDISPLAY *)psoOrig->dhpdev;
	XCLIPOBJ *pxco = NULL;
	RGNDAT *prgnd;
	
	if ((pcoOrig == NULL))
	{
		m_pco = (CLIPOBJ *)&pvd->m_co;
		m_iDComplexityOrig = DC_TRIVIAL;
		m_rclBoundsOrig = m_pco->rclBounds;
		m_rclDraw = *prclDraw;
		m_pRgndatOrig = NULL;
	} else {
		m_pco	= pcoOrig;
		m_iDComplexityOrig = m_pco->iDComplexity;
		m_rclBoundsOrig =    m_pco->rclBounds;
		GetIntersectRect(prclDraw, &m_rclBoundsOrig, &m_rclDraw);
	}

	pxco = (XCLIPOBJ *)m_pco;
	m_pRgndatOrig = pxco->prgndClipGet();
	for (int i=0; i < pvd->m_cSurfaces; i++ )
	{
		m_pds = &pvd->m_ds[i];

		if ((m_iDComplexityOrig == DC_TRIVIAL) &&
			(m_rclDraw.left >= m_pds->rcl.left) 	&&
			(m_rclDraw.top >= m_pds->rcl.top) &&
			(m_rclDraw.right < m_pds->rcl.right) &&
			(m_rclDraw.bottom < m_pds->rcl.bottom))
		{
			m_pco->iDComplexity = DC_TRIVIAL;
			m_pco->rclBounds = m_rclDraw;
			m_pso = m_pds->pso;
			
			return (TRUE);
		} else if (GetIntersectRect(&m_rclDraw, &m_pds->rcl, &m_pco->rclBounds))
		{
			m_pco->iDComplexity = (m_iDComplexityOrig != DC_TRIVIAL) ? m_iDComplexityOrig : DC_RECT;
			m_pso = m_pds->pso;

			int cjsize = sizeof(RGNDATHEADER) + 
				 (pxco->m_prgnd->rdh.nCount)*sizeof(RECTS);
			prgnd = (RGNDAT *)new BYTE[cjsize];
			CopyRgnDat(prgnd, m_pRgndatOrig, cjsize);
			ClipRgnDatClip(prgnd, &m_pds->rcl);
			pxco->prgndClipSet(prgnd);
			
			return TRUE;
		}
	}

	m_pco->rclBounds = m_rclBoundsOrig;
	m_pco->iDComplexity = m_iDComplexityOrig;
	
	prgnd = pxco->prgndClipGet();
	pxco->prgndClipSet(m_pRgndatOrig);
	if (prgnd && prgnd != m_pRgndatOrig)
		delete [] (BYTE *)prgnd;

	return FALSE;		
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MSURF::bNextSurface()

Helper function to find the next surface that intersect the rectangle
return FALSE if none is found.

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL MSURF::bNextSurface()
{
	VDISPLAY *pvd = GetVDisplay();
	
	for (long i= (long)m_pds->iDispSurf+1; i < gpvdisplay->m_cSurfaces; i ++)
	{	
		m_pds = &pvd->m_ds[i];

		if (m_pds == NULL)
			break;
		
		if ((m_iDComplexityOrig == DC_TRIVIAL) &&
			(m_rclDraw.left >= m_pds->rcl.left) 	&&
			(m_rclDraw.top >= m_pds->rcl.top) &&
			(m_rclDraw.right < m_pds->rcl.right) &&
			(m_rclDraw.bottom < m_pds->rcl.bottom))
		{
		// actually this couldn't happen cuz if rclDraw is inside some pds->rcl
		// it should be detected in bFindSurface!!
			m_pco->iDComplexity = DC_TRIVIAL;
			m_pco->rclBounds = m_rclDraw;
			m_pso = m_pds->pso;
			
			return (TRUE);
		} else if (GetIntersectRect(&m_rclDraw, &m_pds->rcl, &m_pco->rclBounds))
		{
			m_pco->iDComplexity = (m_iDComplexityOrig != DC_TRIVIAL) ? m_iDComplexityOrig : DC_RECT;
			m_pso = m_pds->pso;

			if (m_pRgndatOrig)
			{
				int cjsize = sizeof(RGNDATHEADER) + 
				    (m_pRgndatOrig->rdh.nCount)*sizeof(RECTS);
				RGNDAT *prgnd = (RGNDAT *)new BYTE[cjsize];
				CopyRgnDat(prgnd, m_pRgndatOrig, cjsize);
				delete (BYTE *)(((XCLIPOBJ *)m_pco)->prgndClipGet());
				ClipRgnDatClip(prgnd, &m_pds->rcl);
				((XCLIPOBJ *)m_pco)->prgndClipSet(prgnd);
			}
			return TRUE;
		}
	}

	m_pco->rclBounds = m_rclBoundsOrig;
	m_pco->iDComplexity = m_iDComplexityOrig;
	if (m_pRgndatOrig)
	{	
		RGNDAT *prgnd = ((XCLIPOBJ *)m_pco)->prgndClipGet();
		((XCLIPOBJ *)m_pco)->prgndClipSet(m_pRgndatOrig);
		if (prgnd && prgnd != m_pRgndatOrig)
			delete [] (BYTE *)prgnd;
	}
	return FALSE;	
}


void RestartEnumClip(XCLIPOBJ *pxco)
{
	pxco->m_ircCur = 0;
    pxco->m_ircInc = 1;
    pxco->m_ircEnd = pxco->m_prgnd->rdh.nCount;

    if (pxco->m_prgnd)
    {
        //
        // REVIEW martsh: Replace the RGNDAT type with XCLIPOBJ, so this can 
        // be cleaned up.
        //
        pxco->rclBounds.left = pxco->m_prgnd->rdh.rcBound.left;
        pxco->rclBounds.top = pxco->m_prgnd->rdh.rcBound.top;
        pxco->rclBounds.right = pxco->m_prgnd->rdh.rcBound.right;
        pxco->rclBounds.bottom = pxco->m_prgnd->rdh.rcBound.bottom;
        
        if (pxco->m_prgnd->rdh.nCount <= 1)
            pxco->iDComplexity = DC_RECT;
        else 
            pxco->iDComplexity = DC_COMPLEX;
    }
}

void PathObj_vOffset(XPATHOBJ *ppo, LONG x, LONG y)
{
	EPOINTL eptl(x,y);

	if (!ppo)
		return;

	if ((x == 0) && (y == 0))
		return;
		
	*((ERECTFX*) &ppo->rcfxBoundBox) += eptl;

    register PATHRECORD* ppr;
    for (ppr = ppo->pprfirst; ppr != (PPATHREC) NULL; ppr = ppr->pprnext)
    {
        for (register EPOINTFIX *peptfx = (EPOINTFIX *)ppr->aptfx;
             peptfx < (EPOINTFIX *)&ppr->aptfx[ppr->count];
             peptfx += 1)
        {
            *peptfx += eptl;
        }
    }
}

void ClipObj_vOffset(XCLIPOBJ *pco, LONG x, LONG y)
{
	if (!pco)
		return;

	if ( (x == 0) && (y == 0))
		return;
	
	pco->rclBounds.left += x;
	pco->rclBounds.right += x;
	pco->rclBounds.top += y;
	pco->rclBounds.bottom += y;
	
	if (pco->iDComplexity != DC_TRIVIAL)
	{
		RECTCOUNT cRects;
    	RECTS *prcStart;

    	prcStart = (RECTS *)pco->m_prgnd->Buffer;
    	cRects = pco->m_prgnd->rdh.nCount;

		pco->m_prgnd->rdh.rcBound.left += (INT16)x;
		pco->m_prgnd->rdh.rcBound.right += (INT16)x;
		pco->m_prgnd->rdh.rcBound.top += (INT16)y;
		pco->m_prgnd->rdh.rcBound.bottom += (INT16)y;
	
  	  	while (cRects > 0) {
  	  	
			prcStart->left += (INT16)x;
			prcStart->right += (INT16)x;
			prcStart->top += (INT16)y;
			prcStart->bottom += (INT16)y;
		
        	prcStart++;
        	cRects--;
    	}
	}
}





/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
DHPDEV vDrvEnablePDEV

Creates a single large Virtual DEV which will be the combination of all the 
single PDEVs. The virtual DEV keeps the appropriate data to be passed down to
each driver
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
extern int AllocConverters();

DHPDEV APIENTRY vDrvEnablePDEV(
				   DEVMODEW * pdm,    //to pass in MULDEVICE data struct
			       LPWSTR pwszLogAddress, 
			       ULONG cPat, 
			       HSURF * phsurfPatterns, 
			       ULONG cjCaps, 
			       ULONG * pdevcaps, 
			       ULONG cjDevInfo, 
			       DEVINFO * pdi, 
			       HDEV hdev, 
			       LPWSTR pwszDeviceName, 
			       HANDLE hDriver)
{	
    if (gpvdisplay == NULL)
    {   
    	MULTIDEVMODE *pMode;
		pMode = (MULTIDEVMODE *)pdm;
		
		gpvdisplay = new VDISPLAY(pMode, hDriver);
					 
		if (gpvdisplay == NULL)
	    	return NULL;
    }

	if (!AllocConverters())
	{
		DEBUGMSG(GPE_ZONE_ENTER, 
		(TEXT("Failed Allocate color converters! Leaving vDrvEnablePDEV\r\n")));
		return (DHPDEV)NULL;
		}

	//The following is copied from ddi_if.cpp
	GDIINFO *pgdiinfo = (GDIINFO *)pdevcaps;
	pgdiinfo->ulVersion = DDI_DRIVER_VERSION;
	pgdiinfo->ulTechnology = DT_RASDISPLAY;
	pgdiinfo->ulHorzSize = 64;
	pgdiinfo->ulVertSize = 60;
	pgdiinfo->ulHorzRes     = pdm->dmPelsWidth;
    pgdiinfo->ulVertRes     = pdm->dmPelsHeight;
	pgdiinfo->ulLogPixelsX  = 96;
    pgdiinfo->ulLogPixelsY  = 96;
    pgdiinfo->cBitsPixel    = pdm->dmBitsPerPel;
    pgdiinfo->cPlanes       = 1;
    pgdiinfo->ulNumColors   = 1 << pdm->dmBitsPerPel;
    pgdiinfo->ulAspectX     = 1;
    pgdiinfo->ulAspectY     = 1;
    pgdiinfo->ulAspectXY    = 1;
    	//what this should be?????
	pgdiinfo->flRaster		= RC_BITBLT
							| RC_STRETCHBLT
							| ((gpvdisplay->IsPaletteSettable())?RC_PALETTE:0);


    pdi->flGraphicsCaps = gpvdisplay->m_flGraphicsCaps;
    	
    return (DHPDEV)gpvdisplay;
}
   
extern void FreeConverters();

VOID APIENTRY vDrvDisablePDEV(DHPDEV dhpdev)
{
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering DrvDisablePDEV\r\n")));
	FreeConverters();
	delete ((VDISPLAY *)dhpdev);
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving DrvDisablePDEV\r\n")));
}


VOID APIENTRY vDrvDisableSurface(DHPDEV dhpdev)
{
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering vDrvDisableSurface\r\n")));
	EngDeleteSurface((HSURF)((GPE *)dhpdev)->GetHSurf());
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving vDrvDisableSurface\r\n")));
}

EGPEFormat BppToEGPFormat(int bpp)
{
	switch (bpp)
	{
	case 1:
		return gpe1Bpp;
	case 2:
		return gpe2Bpp;
	case 4:
		return gpe4Bpp;
	case 8:
		return gpe8Bpp;
	case 16:
		return gpe16Bpp;
	case 24:
		return gpe24Bpp;
	case 32:
		return gpe32Bpp;
	default:
		return gpeUndefined;
	}
}

HSURF APIENTRY vDrvEnableSurface(DHPDEV dhpdev)
{
	GPE *pGPE= (GPE *)dhpdev;
	SIZEL sizl;
	HSURF hsurf;
	SURFOBJ *psurf;
	VDISPLAY *pvd = (VDISPLAY *)dhpdev;
	GPESurf *pSurf = NULL;
	
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering vDrvEnableSurface\r\n")));
	
	sizl.cx = pGPE->ScreenWidth();
	sizl.cy = pGPE->ScreenHeight();

	
	//EngCreateDeviceSurface don't really do anything
	//but create a new SURFOBJ object.
	//the m_ds in the vdisplay point to an array of surfobj's :)
	pSurf = new GPESurf(sizl.cx, sizl.cy , NULL, (( (pvd->m_iBpp * sizl.cx + 7 )/ 8 + 3 ) & ~3L),
		BppToEGPFormat(pvd->m_iBpp));
	//hsurf = EngCreateDeviceSurface((DHSURF)&pvd->m_ds, sizl, pvd->m_iBpp);
	hsurf = EngCreateDeviceSurface((DHSURF)pSurf, sizl, pvd->m_iBpp);
	
	pGPE->SetHSurf(unsigned long(hsurf));

	psurf = (SURFOBJ *)hsurf;
	psurf->iType = SURF_TYPE_MULTI;  //the dhsurf points to multiple surfaces
	//do we need to create a DIBSection to assosicate with the surface
	//and create a clipobj?????
	//not to worry about this for now pco = new CLIPOBJ;

	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving vDrvEnableSurface\r\n")));
	
	return hsurf;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvGetModes

For Multimon virtual driver, the devmode 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

// Do all the display have a common mode?? i.e. the same color depth and resolution
DWORD HaveTheSameMode(Disp_Driver *pdriver)
{
	DWORD bpp;

	bpp = pdriver->m_devcaps.m_BITSPIXEL;
	for (; pdriver; pdriver = pdriver->m_pDisplayNext)
	{
		if (pdriver->m_devcaps.m_BITSPIXEL != bpp)
			return 0;
	}

  	return bpp;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  vDrvGetModes()
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
ULONG APIENTRY vDrvGetModes(HANDLE hDriver, ULONG cjSize, DEVMODEW * pdm)
{
	ULONG bytesReq = sizeof(MULTIDEVMODE);
	MULTIDEVMODE *pMode = (MULTIDEVMODE *)pdm;
	Disp_Driver *pdriver;
	RECTL  rcl;
	LONG  left, right, top, bottom;	
	
	if (pdm == NULL)
		return bytesReq;

	if (cjSize != bytesReq )
		return 0;

	memset(pMode, 0, cjSize);

	pMode->devmodew.dmSize = sizeof(DEVMODEW);
	pMode->devmodew.dmDriverExtra = (WORD)(cjSize - pMode->devmodew.dmSize);
		
	pMode->cDev = 0;

	rcl.left = rcl.top = rcl.bottom = rcl.right = 0;
	
	left = top = 0x7fffffff;
	right = bottom = 0x80000000;

	pdriver = (Disp_Driver *)hDriver;   //this driver should be g_driverMain. 
	for (pdriver = pdriver->m_pDisplayNext; pdriver; pdriver = pdriver->m_pDisplayNext)
	{
		pMode->dev[pdriver->m_iDriver].dhpdev = pdriver->m_dhpdev;
		pMode->dev[pdriver->m_iDriver].hDev = (HDEV)pdriver;
		pMode->cDev ++;
	}

	//do this loop again in order to make the primary origin at (0,0)
	for (int i=0; i < (int)(pMode->cDev); i++)
	{
		RECTL *prc = &pMode->dev[i].rcl;
		pdriver = (Disp_Driver *)(pMode->dev[i].hDev);
		
		rcl.left = rcl.right; //start from next pixel
		rcl.bottom = pdriver->m_surfobj.sizlBitmap.cy; 
		rcl.right += pdriver->m_surfobj.sizlBitmap.cx;

		prc->left = rcl.left;
		prc->right = rcl.right;
		prc->top = rcl.top;
		prc->bottom = rcl.bottom;
		
		left = min(left, rcl.left);
		right = max(right, rcl.right);
		top = min(top, rcl.top);
		bottom = max(bottom, rcl.bottom);
	}
	
	// See if they have the same mode!!
	pdriver = (Disp_Driver *)hDriver;
	if ((pMode->devmodew.dmBitsPerPel= HaveTheSameMode(pdriver->m_pDisplayNext)) == 0)
	{
		//can't find the mode, then say no modes
		return 0; 
	}
	
	pMode->devmodew.dmPelsWidth = right - left;
	pMode->devmodew.dmPelsHeight = bottom - top;
	
	return cjSize;	
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvCreateDeviceBitmap

The driver can store the bitmap anywhere (in system or video memory) and in any format provided......

Should we create a bitmap with multi bitmap and each one coresponding to each driver, so comply 
with the device format.  
the question is then each time should update all the bitmaps????? and the memory considerations......

maybe eventually we need to take just one memory bitmap!

return:  HBITMAP ==> SURFOBJ . dhsurf --> GPESurf(sizl,iFormat) . m_nHandle --> this (surfobj,i.e. hbitmap) 
							 . hsurf -->  this(surfobj, i.e. hbitmap)
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

HBITMAP APIENTRY vDrvCreateDeviceBitmap(DHPDEV dhpdev, SIZEL sizl, ULONG iFormat)
{
	GPESurf *psurf;

	psurf = new GPESurf(sizl.cx, sizl.cy, IFormatToEGPEFormat[iFormat]);

	if (psurf == NULL)
	{
		return (HBITMAP)0xffffffff;
	}
	if (psurf->Buffer() == NULL)
	{
		delete psurf;
		return (HBITMAP)0xffffffff;	
	}

	HBITMAP hbm = EngCreateDeviceBitmap((DHSURF)psurf, sizl, iFormat);
	psurf->m_nHandle = (ULONG)hbm;
	((SURFOBJ *)hbm)->dhpdev = dhpdev;

	if (!hbm)
		return (HBITMAP)0xffffffff;
	else 
		return hbm;    
}



VOID APIENTRY vDrvDeleteDeviceBitmap(DHSURF dhsurf)
{
	GPESurf *psurf = (GPESurf *)dhsurf;
	HBITMAP hbm = (HBITMAP)psurf->m_nHandle;

	EngDeleteSurface((HSURF)hbm);
	delete psurf;	
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvMovePointer()

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static POINT g_CursorPoint = {0,0};

inline Disp_Driver *SurfobjToDriver( SURFOBJ *pso )
{
	return (Disp_Driver *)(pso->hdev);
}

VOID APIENTRY vDrvMovePointer(SURFOBJ * pso, LONG x, LONG y, RECTL * prcl)
{
	VDISPLAY *pvd = (VDISPLAY *)pso->dhpdev;
	DISPSURF *pdsFrom, *pdsTo;
//	LONG xMonitor, yMonitor;
	
	ASSERT(pso->iType == SURF_TYPE_MULTI);

	pdsTo = pvd->PDispFromPoint(x,y);

	if (!pdsTo) 
	{
		pdsTo = pvd->GetBoundPointInScreen(&x,&y, g_CursorPoint.x, g_CursorPoint.y);
	}
	
	pdsFrom = pvd->PDispFromPoint(g_CursorPoint.x, g_CursorPoint.y);

	g_CursorPoint.x = x;
	g_CursorPoint.y = y;
	
	if (pdsFrom && pdsFrom != pdsTo) {
		(SurfobjToDriver(pdsFrom->pso))->m_drvenabledata.DrvMovePointer(pdsFrom->pso,-1,-1,prcl); 
	}

	x = x - pdsTo->rcl.left;
	y = y - pdsTo->rcl.top;

	if (prcl)
	{
		prcl->left -= pdsTo->rcl.left;
		prcl->right -= pdsTo->rcl.left;
		prcl->top -= pdsTo->rcl.top;
		prcl->bottom -= pdsTo->rcl.top;
	}

	(SurfobjToDriver(pdsTo->pso))->m_drvenabledata.DrvMovePointer(pdsTo->pso,x,y,prcl);
}



ULONG APIENTRY vDrvSetPointerShape(SURFOBJ * pso, 
								    SURFOBJ * psoMask, 
								    SURFOBJ * psoColor, 
								    XLATEOBJ * pxlo, 
								    LONG xHot, 
								    LONG yHot, 
								    LONG x, 
								    LONG y, 
								    RECTL * prcl, 
								    FLONG fl)
{
	VDISPLAY *pvd = (VDISPLAY *)pso->dhpdev;
	ULONG   uRet = 0;
	Disp_Driver  *pdriver;
	LONG    xMonitor, yMonitor;
	RECTL   *prclMonitor, rclMonitor;
	
 	g_CursorPoint.x = x;
 	g_CursorPoint.y = y;
 	

	for (int i=0; i < pvd->m_cSurfaces; i ++)
	{
		pdriver = (Disp_Driver *)(pvd->m_ds[i].hdev);

		if ( (x != -1) || (y != -1) )
		{
			RECTL *prcl = &pvd->m_ds[i].rcl;
			if ((x < prcl->left) || ( x >= prcl->right)
				|| (y < prcl->top) || (y >= prcl->bottom))
			{
				xMonitor = 0;
				yMonitor = 0;
			} else
			{
				xMonitor = x - pvd->m_ds[i].rcl.left;
				yMonitor = y - pvd->m_ds[i].rcl.top;
			}
		}
		else 
		{
			xMonitor = x;
			yMonitor = y;
		}

		if (prcl)
		{
			rclMonitor.left = prcl->left - pvd->m_ds[i].rcl.left;
			rclMonitor.right = prcl->right - pvd->m_ds[i].rcl.left;
			rclMonitor.top = prcl->top - pvd->m_ds[i].rcl.top;
			rclMonitor.bottom = prcl->bottom - pvd->m_ds[i].rcl.top;

			prclMonitor = &rclMonitor;
		} else
			prclMonitor = prcl;
		
		uRet |= pdriver->m_drvenabledata.DrvSetPointerShape(pvd->m_ds[i].pso,psoMask,psoColor,
												pxlo,xHot,yHot,xMonitor,yMonitor,prclMonitor,fl);
	}

	return uRet;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvSetPalette()

Need to set the palette of all the displays
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

BOOL APIENTRY vDrvSetPalette(DHPDEV dhpdev, PALOBJ * ppalo, FLONG fl, ULONG iStart, ULONG cColors)
{
	VDISPLAY *pvd;
	BOOL bRet = TRUE;

	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering vDrvSetPalette\r\n")));

	pvd = (VDISPLAY *)dhpdev;

	for (int i=0; i < pvd->m_cSurfaces; i ++)
	{
		Disp_Driver *pdriver;

		pdriver= (Disp_Driver *)pvd->m_ds[i].hdev;
//		if (pdriver->FSettableHwPal())
		if (pdriver->m_devcaps.m_RASTERCAPS & RC_PALETTE )
			bRet &= (pdriver->m_drvenabledata.DrvSetPalette(pdriver->m_surfobj.dhpdev,
														  ppalo,
														  fl,
														  iStart,
														  cColors));
	}

	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving vDrvSetPalette\r\n")));

	return bRet;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The following functions are helper functions for driver blits 
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This function determines if the src will be required to do the blit to the dest 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

inline BOOL bSrcIntersectDstByOffset(RECTL *prclDst, RECTL *prclSrc, LONG dx, LONG dy)
{
	return ( (prclDst->left < prclSrc->right + dx) &&
			 (prclDst->right > prclSrc->left + dx) &&
			 (prclDst->top < prclSrc->bottom + dy) &&
			 (prclDst->bottom > prclSrc->top + dy));
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This function sorts the list of surfobj for the correct ordering for screen to screen
blits that span borads.

It returns a DISPSURF pointer which points to the first DISPSURF that should be processed
and each dispsurf's pdsbltnext will point to the next one in order.
	
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DISPSURF *vSortBltOrder(VDISPLAY *pvd, LONG dx, LONG dy)
{
	DISPSURF *pdsHeadOld = pvd->pdsBlt;
	DISPSURF *pdsHeadNew = pdsHeadOld;

	pdsHeadOld = pdsHeadOld->pdsBltNext;
	pdsHeadNew->pdsBltNext = NULL;

	while (pdsHeadOld)
	{
		DISPSURF *pdsInsert;
		DISPSURF *pdsPrev = pdsHeadNew;
		DISPSURF *pdsCur = pdsHeadNew;

		pdsInsert = pdsHeadOld;
		pdsHeadOld = pdsHeadOld->pdsBltNext;
		while ((pdsCur) && (!bSrcIntersectDstByOffset(&pdsInsert->rcl,&pdsCur->rcl,dx,dy)))
		{
			pdsPrev = pdsCur;
			pdsCur = pdsCur->pdsBltNext;
		}

		if (pdsCur == pdsHeadNew)
		{
			pdsHeadNew = pdsInsert;
			pdsInsert->pdsBltNext = pdsCur;
		} else 
		{
			pdsPrev->pdsBltNext = pdsInsert;
			pdsInsert->pdsBltNext = pdsCur;
		}
	}

	pvd->pdsBlt = pdsHeadNew;
	return pdsHeadNew;
}





/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bBitBltScreenToScreen

Handles screen to screen blits. 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

BOOL bBitBltScreenToScreen(SURFOBJ * pso, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo,  
						  RECTL * prclDest, 
						  POINTL * pptlSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4,
						  unsigned long bltFlags)
{
	VDISPLAY *pvd;
	LONG dx, dy, dxMask, dyMask;
	POINTL ptlMaskCopy;
	DISPSURF *pdsSrc, *pdsDst;
	RECTL  rclDst, rclSrc;
	Disp_Driver   *pdriver;
	BOOL bRet;
	
	bRet = TRUE;
	pvd = (VDISPLAY *)pso->dhpdev;

	//the offset between dest and src origin
	dx = prclDest->left - pptlSrc->x;
	dy = prclDest->top - pptlSrc->y;

	if (pptlMask)
	{
		ptlMaskCopy = *pptlMask;
		dxMask = prclDest->left - pptlMask->x;
		dyMask = prclDest->top - pptlMask->y;
	}
	
	pdsDst = vSortBltOrder(pvd, dx, dy);

	for (;pdsDst; pdsDst = pdsDst->pdsBltNext)
	{
		pdriver = (Disp_Driver *)pdsDst->hdev;
		for (pdsSrc = pdsDst; pdsSrc; pdsSrc = pdsSrc->pdsBltNext)
		{
			rclDst.left = dx + pdsSrc->rcl.left;
			rclDst.right = dx + pdsSrc->rcl.right;
			rclDst.top = dy + pdsSrc->rcl.top;
			rclDst.bottom = dy + pdsSrc->rcl.bottom;

			if (GetIntersectRect(prclDest, &rclDst, &rclDst) &&
				GetIntersectRect(&rclDst, &pdsDst->rcl, &rclDst))
			{
				rclSrc.left = rclDst.left - dx;
				rclSrc.top = rclDst.top - dy;
				rclSrc.right = rclDst.right - dx;
				rclSrc.bottom = rclDst.bottom - dy;

				if (pptlMask)
				{
					pptlMask->x = rclDst.left - dxMask;
					pptlMask->y = rclDst.top - dyMask;
				}
				
				LONG xOffset = pdsDst->rcl.left;
				LONG yOffset = pdsDst->rcl.top;

				ClipObj_vOffset((XCLIPOBJ *)pco, -xOffset, -yOffset);
				OffsetRect((RECT *)&rclDst, -xOffset, -yOffset);
				OffsetRect((RECT *)&rclSrc, -pdsSrc->rcl.left, -pdsSrc->rcl.top);

				pvd->byteBusyBits = 1 << pdsSrc->iDispSurf;
				pvd->byteBusyBits |= 1 << pdsDst->iDispSurf;
				
				bRet &= pdriver->m_drvenabledata.DrvAnyBlt(pdsDst->pso,
														   pdsSrc->pso,
														   psoMask,
														   pco,
														   pxlo,
														  (POINTL *)NULL,
														  &rclDst,
														  &rclSrc,
														   pptlMask,
														   pbo,
														   pptlBrush,
														   rop4,
														   0,
														   bltFlags);
														
				ClipObj_vOffset((XCLIPOBJ *)pco, xOffset, yOffset);
				RestartEnumClip((XCLIPOBJ *)pco);
			}
		}
	}	

	if (pptlMask)
		*pptlMask = ptlMaskCopy;
	return bRet;
}


/*
bBitBltFromScreen 

Handles screen-to-bitmap blits that may possibly span multiple displays
*/

BOOL bBitBltFromScreen(SURFOBJ * psoDest, 
						  SURFOBJ * psoSrc, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo,  
						  RECTL * prclDest, 
						  POINTL* pptlSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4,
						  unsigned long bltFlags)
{
	VDISPLAY *pvd;
	LONG     dx, dy, dxMask, dyMask;
	RECTL	  rclDraw, rclDst, rclSrc;
	POINTL   ptlMaskCopy;
	MSURF     msurf;
	BOOL     bRet;

	bRet = TRUE;
	pvd = (VDISPLAY *)psoSrc->dhpdev;

	//offset of the dest and src origin
	dx = prclDest->left - pptlSrc->x;
	dy = prclDest->top - pptlSrc->y;

	if (pptlMask)
	{
		ptlMaskCopy = *pptlMask;
		dxMask = pptlMask->x - pptlSrc->x;
		dyMask = pptlMask->y - pptlSrc->y;
	}
	
	rclDraw = *prclDest;

	if ((pco != NULL) && ( pco->iDComplexity != DC_TRIVIAL))
	{
		if (!GetIntersectRect(&pco->rclBounds, &rclDraw, &rclDraw))
			return TRUE; 
	}

	//rclDraw now is to src coordinate!
	rclDraw.left -= dx;
	rclDraw.right -= dx;
	rclDraw.top -= dy;
	rclDraw.bottom -= dy;

	if (msurf.bFindSurface(psoSrc, NULL, &rclDraw))
	{
		do {
	//since dest is memory bitmap and we don't need to worry about the 
	//so......
			Disp_Driver *pdriver = (Disp_Driver *)msurf.m_pso->hdev;

			// since the screen is the source, but the clip bounds
			// apply to the dest, so we have to convert to dest coord.
//			rclSrc.left = msurf.m_pco->rclBounds.left;
//			rclSrc.top = msurf.m_pco->rclBounds.top;
//			rclSrc.right = msurf.m_pco->rclBounds.right;
//			rclSrc.bottom = msurf.m_pco->rclBounds.bottom;
	        rclSrc = msurf.m_pco->rclBounds;		
			rclDst = msurf.m_pco->rclBounds;

			rclDst.left += dx;
			rclDst.right += dx;
			rclDst.top += dy;
			rclDst.bottom += dy;				

			if (pptlMask)
			{
				pptlMask->x = rclSrc.left + dxMask;
				pptlMask->y = rclSrc.top + dyMask;
			}
			
			LONG xOffset = msurf.m_pds->rcl.left;
			LONG yOffset = msurf.m_pds->rcl.top;

			rclSrc.left -= xOffset;
			rclSrc.top -= yOffset;
			rclSrc.right -= xOffset;
			rclSrc.bottom -= yOffset;

			pvd->byteBusyBits = 1 <<  msurf.m_pds->iDispSurf;
			bRet &= pdriver->m_drvenabledata.DrvAnyBlt(psoDest,
													   msurf.m_pso,
													   psoMask,
													   pco,
													   pxlo,
													   (POINTL *)NULL,
													   &rclDst,
													   &rclSrc,
													   pptlMask,
													   pbo,
													   pptlBrush,
													   rop4,
													   0,
													   bltFlags);
			
		} while (msurf.bNextSurface());
	}		

	if (pptlMask)
		*pptlMask = ptlMaskCopy;
	return bRet;
}



BOOL bBitBltToScreen(SURFOBJ * psoDest, 
						  SURFOBJ * psoSrc, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo, 
						  RECTL * prclDest, 
						  POINTL * pptlSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4,
						  unsigned long bltFlags)
{
	VDISPLAY *pvd;
	MSURF    msurf;
	RECTL   rclDst, rclSrc;
	POINTL   ptlMaskCopy;
	BOOL  	 bRet;

	bRet = TRUE;
	pvd = (VDISPLAY *)psoDest->dhpdev;

	if( pptlSrc )
	{
		rclSrc.top = pptlSrc->y;
		rclSrc.left = pptlSrc->x;
	}
	else
	{
		rclSrc.top = 0;
		rclSrc.left = 0;
	}
	rclSrc.bottom = rclSrc.top + prclDest->bottom - prclDest->top;
	rclSrc.right = rclSrc.left + prclDest->right - prclDest->left;

	if (pptlMask)
		ptlMaskCopy = *pptlMask;
	
	if (msurf.bFindSurface(psoDest, pco, prclDest))
	{
		do {
		Disp_Driver *pdriver = (Disp_Driver *)msurf.m_pso->hdev;
		RestartEnumClip((XCLIPOBJ *)msurf.m_pco);
		
		LONG xOffset = msurf.m_pds->rcl.left;
		LONG yOffset = msurf.m_pds->rcl.top;
		GetIntersectRect(prclDest, &msurf.m_pds->rcl, &rclDst);
		LONG dx = rclDst.left - prclDest->left;
		LONG dy = rclDst.top - prclDest->top;
		
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, -xOffset, -yOffset);
		OffsetRect((RECT *)&rclDst, -xOffset, -yOffset);

		rclSrc.left += dx;
		rclSrc.top += dy;
		rclSrc.right = rclSrc.left + (rclDst.right - rclDst.left); 
		rclSrc.bottom = rclSrc.top + (rclDst.bottom - rclDst.top);

		if (pptlMask)
		{
			pptlMask->x = ptlMaskCopy.x + dx;
			pptlMask->y = ptlMaskCopy.y +dy;
		}
		
		pvd->byteBusyBits = 1 << msurf.m_pds->iDispSurf;
		bRet &= pdriver->m_drvenabledata.DrvAnyBlt(msurf.m_pso,
												   psoSrc,
												   psoMask,
												   msurf.m_pco,
												   pxlo,
												   (POINTL *)NULL,
												   &rclDst,
												   &rclSrc,
												   pptlMask,
												   pbo,
												   pptlBrush,
												   rop4,
												   0,
												   bltFlags);
												   
		rclSrc.left -= dx;
		rclSrc.top -= dy;
		
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, xOffset, yOffset);										   
		} while (msurf.bNextSurface());
	}

	if (pptlMask)
		*pptlMask = ptlMaskCopy;
		
	return bRet;
}


extern BOOL AnyBlt(SURFOBJ * psoTrg, 
					SURFOBJ * psoSrc, 
					SURFOBJ * psoMask, 
					CLIPOBJ * pco, 
					XLATEOBJ * pxlo, 
					RECTL * prclTrg, 
					RECTL * prclSrc, 
					POINTL * pptlMask, 
					BRUSHOBJ * pbo, 
					POINTL * pptlBrush, 
					ROP4 rop4, 
					unsigned long bltFlags);


BOOL APIENTRY vDrvBitBltHelp(SURFOBJ * psoTrg, 
						  SURFOBJ * psoSrc, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo, 
						  RECTL * prclTrg, 
						  POINTL * pptlSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4, 
						  unsigned long bltFlags)
{
	BOOL  bFromScreen, bToScreen;
	RECTL rclDst;
	BOOL  bRet;
	POINTL ptlSrc;
	
	bRet = TRUE;

	bFromScreen = (psoSrc != NULL) && (psoSrc->iType == SURF_TYPE_MULTI);
	bToScreen = (psoTrg->iType == SURF_TYPE_MULTI);
		
	rclDst = *prclTrg;


	if (pptlSrc)
	{
		ptlSrc.y = pptlSrc->y;
		ptlSrc.x = pptlSrc->x;
	} else 
	{
		ptlSrc.y = 0;
		ptlSrc.x = 0;
	}
		

	if (bFromScreen && bToScreen)
	{
		return bBitBltScreenToScreen(psoTrg,psoMask,pco,pxlo,&rclDst,&ptlSrc,pptlMask,pbo,pptlBrush,rop4,bltFlags);
	} else if (bFromScreen)
	{
		return bBitBltFromScreen(psoTrg,psoSrc,psoMask,pco,pxlo,&rclDst,&ptlSrc,pptlMask,pbo,pptlBrush,rop4,bltFlags);
	} else if (bToScreen)
	{
		return bBitBltToScreen(psoTrg,psoSrc,psoMask,pco,pxlo,&rclDst,&ptlSrc,pptlMask,pbo,pptlBrush,rop4,bltFlags);
	}	
	else // memory to memory
	{
		RECTL rclSrc;

		rclSrc.left = ptlSrc.x;
		rclSrc.top = ptlSrc.y;
		rclSrc.bottom = rclSrc.top + prclTrg->bottom - prclTrg->top;
		rclSrc.right = rclSrc.left + prclTrg->right - prclTrg->left;

		return AnyBlt(psoTrg, 
					   psoSrc, 
					   psoMask, 
					   pco, 
					   pxlo, 
					   prclTrg, 
					   &rclSrc, 
					   pptlMask, 
					   pbo, 
					   pptlBrush, 
					   rop4, 
					   bltFlags);
	}
}

BOOL APIENTRY vDrvBitBlt(SURFOBJ * psoTrg, 
						  SURFOBJ * psoSrc, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo, 
						  RECTL * prclTrg, 
						  POINTL * pptlSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4)
{
	return vDrvBitBltHelp(psoTrg, 
					 psoSrc, 
					 psoMask, 
					 pco, 
					 pxlo, 
					 prclTrg, 
					 pptlSrc, 
					 pptlMask, 
					 pbo, 
					 pptlBrush, 
					 rop4, 
					 0);
}

BOOL APIENTRY vDrvCopyBits(SURFOBJ * psoDest, 
							SURFOBJ * psoSrc, 
							CLIPOBJ * pco, 
							XLATEOBJ * pxlo, 
							RECTL * prclDest, 
							POINTL * pptlSrc)
{
	// Should be used by printer only!!
	// see comment in gpe/ddi_if.cpp

	RETAILMSG(TRUE,(TEXT("Qiong: Entering vDrvCopyBits! - SHOULD ONLY BE USED BY PRINTER Disp_Driver \r\n")));
	ASSERT(0);
	return FALSE;
}


#define CLIP_LIMIT 50
typedef struct _CLIPENUM {
    LONG    c;
    RECTL   arcl[CLIP_LIMIT];   // Space for enumerating complex clipping

} CLIPENUM;                         /* ce, pce */

extern SCODE ClipBlt(GPE * pGPE, GPEBltParms * pBltParms);
/* 
vStretchBlt()
*/
BOOL vStretchBlt(SURFOBJ * psoDest, 
			     SURFOBJ * psoSrc, 
				 SURFOBJ * psoMask, 
				 CLIPOBJ * pco, 
			     XLATEOBJ * pxlo,
			     POINTL*    pptlHTOrg,
				 RECTL * prclDest, 
				 RECTL * prclSrc, 
				 POINTL * pptlMask, 
				 BRUSHOBJ * pbo, 
				 POINTL * pptlBrush, 
				 ROP4 rop4,
				 ULONG iMode,
				 ULONG  bltFlags)
{
	BOOL  bFromScreen, bToScreen;
	RECTL rclDst,rclSrcCopy, rclDstCopy, rclMask;
	VDISPLAY *pvd = GetVDisplay();	
	SCODE sc1;
	SURFOBJ *psoSrcMem, *psoDestMem;
	GPE *pGPE = NULL;
	GPESurf gpeSurfSrc, gpeSurfDst, gpeSurfMask;
	GPESurf *pSurfSrc, *pSurfDst, *pSurfMask;
	BOOL  bRet = FALSE;

	bFromScreen = (psoSrc != NULL) && (psoSrc->iType == SURF_TYPE_MULTI);
	bToScreen = (psoDest->iType == SURF_TYPE_MULTI);

	psoSrcMem = psoSrc;
	psoDestMem = psoDest;
	if (bFromScreen)
	{
		SIZEL  sizlSrc;
		RECTL rclSrcClip = *prclSrc;

		// right now assume the virtual system is always from (0,0), (positive dx, positive dy)
		if (rclSrcClip.left < pvd->m_rcVScreen.left)
			rclSrcClip.left = pvd->m_rcVScreen.left;
		if (rclSrcClip.top < pvd->m_rcVScreen.top)
			rclSrcClip.top = pvd->m_rcVScreen.top;
		if (rclSrcClip.right > pvd->m_rcVScreen.right)
			rclSrcClip.right = pvd->m_rcVScreen.right;
		if (rclSrcClip.bottom > pvd->m_rcVScreen.bottom)
			rclSrcClip.bottom = pvd->m_rcVScreen.bottom;

	    if (rclSrcClip.top >= rclSrcClip.bottom ||
	    	rclSrcClip.left >= rclSrcClip.right)
	    	return TRUE;

	   SetRect((RECT *)&rclDst, 0, 0, rclSrcClip.right - rclSrcClip.left, rclSrcClip.bottom - rclSrcClip.top);

	   POINTL ptSrcOrig = {rclSrcClip.left, rclSrcClip.top};
	   sizlSrc.cx = rclSrcClip.right - rclSrcClip.left;
	   sizlSrc.cy = rclSrcClip.bottom - rclSrcClip.top;

	   psoSrcMem = (SURFOBJ *)vDrvCreateDeviceBitmap((DHPDEV)pvd, sizlSrc, BPPToBMF(pvd->m_iBpp));
	   if (psoSrcMem == (SURFOBJ *)0xFFFFFFFF)
	   {
	   		goto errRtn;
	   }
	   
	   if (!vDrvBitBlt(psoSrcMem, 
	   					psoSrc, 
	   					NULL, 
	   					(CLIPOBJ *)NULL, 
	   					NULL, 
	   					&rclDst, 
	   					&ptSrcOrig, 
	   					NULL, 
	   					NULL, 
	   					NULL, 
	   					0xcccc))
	   {
	   	  goto errRtn;
	   }
	  rclSrcCopy = rclDst;
	   
	  prclSrc = &rclSrcCopy;	  	    		
	}

	rclDst = *prclDest;
	if (bToScreen)
	{
		SIZEL  sizlDst;
		POINTL ptlDstOrig;

		rclDstCopy.left = rclDstCopy.top = 0;
		if (prclDest->left <= prclDest->right)
		{
			rclDst.left = 0;
			rclDst.right = prclDest->right - prclDest->left;
			ptlDstOrig.x = prclDest->left;
			rclDstCopy.right = rclDst.right;
		}
		else
		{
			rclDst.left = prclDest->left - prclDest->right;
			rclDst.right = 0;
			ptlDstOrig.x = prclDest->right;
			rclDstCopy.right = rclDst.left;
		}
		
		if (prclDest->top <= prclDest->bottom) 
		{
			rclDst.top = 0;
			rclDst.bottom = prclDest->bottom - prclDest->top;
			ptlDstOrig.y = prclDest->top;
			rclDstCopy.bottom = rclDst.bottom;
		}
		else
		{
			rclDst.top = prclDest->top - prclDest->bottom;
			rclDst.bottom = 0;
			ptlDstOrig.y = prclDest->bottom;
			rclDstCopy.bottom = rclDst.top;
		}	
	
	    sizlDst.cx = rclDstCopy.right;
	    sizlDst.cy = rclDstCopy.bottom;
	    psoDestMem = (SURFOBJ *)vDrvCreateDeviceBitmap((DHPDEV)pvd, sizlDst, BPPToBMF(pvd->m_iBpp));
	    if(psoDestMem == (SURFOBJ *)0xFFFFFFFF)
	    {
	    	goto errRtn;
	    }
	    
		if (!vDrvBitBlt(psoDestMem, 
						 psoDest, 
						 NULL, 
						 (CLIPOBJ *)NULL, 
						 (XLATEOBJ *)NULL, 
						 &rclDstCopy, 
						 &ptlDstOrig, 
						 (POINTL *)NULL, 
						 (BRUSHOBJ *)NULL, 
						 (POINTL *)NULL, 
						 0xcccc))
		{
			goto errRtn;
		}	

	// offset for later use
		rclDstCopy.left += ptlDstOrig.x;
		rclDstCopy.right += ptlDstOrig.x;
		rclDstCopy.top += ptlDstOrig.y;
		rclDstCopy.bottom += ptlDstOrig.y;
	}


	if (pptlMask)
	{
		rclMask.top = pptlMask->y;
		rclMask.left = pptlMask->x;
		rclMask.bottom = pptlMask->y + prclDest->bottom - prclDest->top;
		rclMask.right = pptlMask->x + prclDest->right - prclDest->left;
	} else 
		rclMask = *prclSrc;

  	if (psoDestMem->dhsurf)
		pSurfDst = (GPESurf *)psoDestMem->dhsurf;
	else 
	{
		gpeSurfDst.Init(psoDestMem->sizlBitmap.cx, psoDestMem->sizlBitmap.cy, psoDestMem->pvScan0, psoDestMem->lDelta,
    				IFormatToEGPEFormat[psoDestMem->iBitmapFormat]);
    	pSurfDst = &gpeSurfDst;
	}

	if (psoSrcMem)
	{
		if (psoSrcMem->dhsurf)
			pSurfSrc = (GPESurf *)psoSrcMem->dhsurf;
		else 
		{
			gpeSurfSrc.Init(psoSrcMem->sizlBitmap.cx, psoSrcMem->sizlBitmap.cy, psoSrcMem->pvScan0, psoSrcMem->lDelta,
						IFormatToEGPEFormat[psoSrcMem->iBitmapFormat]);
    		pSurfSrc = &gpeSurfSrc;
		}
	}
	else
		pSurfSrc = NULL;

	if (psoMask)
	{
		if (psoMask->dhsurf)
			pSurfMask = (GPESurf *)(psoMask->dhsurf);
		else 
		{
			gpeSurfMask.Init(psoMask->sizlBitmap.cx, psoMask->sizlBitmap.cy, psoMask->pvScan0, psoMask->lDelta,
 					IFormatToEGPEFormat[psoMask->iBitmapFormat]);
 			pSurfMask = &gpeSurfMask;
		}
	}
	else 
		pSurfMask = NULL;
	
    GPEBltParms parms;

    parms.pDst = pSurfDst;
    parms.prclDst = &rclDst;
    parms.xPositive = 1;
    parms.yPositive = 1;
    parms.rop4 = rop4;
    parms.pLookup = (ULONG *)NULL;
    parms.pConvert = NULL;
    parms.pBrush = (GPESurf *)NULL;
    parms.solidColor = 0xffffffff;
	parms.bltFlags = bltFlags;
	
    if ((rop4 ^ (rop4 >> 2)) & 0x3333)
    {
    	parms.prclSrc = prclSrc;
    	parms.pSrc = pSurfSrc;
    	if (!prclDest || !prclSrc)
    		goto errRtn;
  		parms.bltFlags |= BLT_STRETCH;
    } else 
    {
    	parms.prclSrc = (RECTL*)NULL;
    	parms.pSrc = NULL;
    	parms.bltFlags &= ~BLT_STRETCH;
    }

	// see if pattern matters
	if ( (rop4 ^ (rop4 >> 4)) & 0x0f0f )
	{
		parms.pptlBrush = pptlBrush;
		if (pbo)
		{
			if (pbo->iSolidColor == 0xffffffff)
			{
				if (pbo->pvRbrush == NULL)
					parms.pBrush = (GPESurf *)(BRUSHOBJ_pvGetRbrush(pbo));
				else 
					parms.pBrush = (GPESurf *)(pbo->pvRbrush);
			} else 
			{
				parms.solidColor = pbo->iSolidColor;
				parms.pptlBrush = (POINTL *)NULL;
			}
		} else 
			goto errRtn;
	} else 
	{
		parms.pptlBrush = (POINTL *)NULL;
		if (pbo)
			parms.solidColor = pbo->iSolidColor;
	}

	//see if mask matters
	if ( (rop4 ^ (rop4 >> 8)) & 0x00ff)
	{
		parms.prclMask = &rclMask;
		parms.pMask = pSurfMask;
	} else 
	{
		parms.prclMask = NULL;
		parms.pMask = NULL;
	}

	if (parms.pSrc)
	{
		ColorConverter::InitConverter(pxlo, 
									  &parms.pColorConverter,
									  (unsigned long(ColorConverter :: * *)(unsigned long))&parms.pConvert, 
									  &parms.pLookup);
	}

    parms.prclClip = NULL;
    pGPE = GetGPE();
	if (pco)
	{
		ClipObj_vOffset((XCLIPOBJ *)pco, rclDst.left - prclDest->left, rclDst.top - prclDest->top);

		if( FAILED(pGPE->BltPrepare( &parms ) ) )
		{
			DEBUGMSG(GPE_ZONE_ERROR,(TEXT("failed to prepare blt\r\n")));
			DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving DrvBitBlt\r\n")));
			return FALSE;
		}
		
		if (pco->iDComplexity == DC_RECT)
		{
			parms.prclClip = &(pco->rclBounds);
			sc1 = ClipBlt(pGPE, &parms);
		}
		else if (pco->iDComplexity == DC_COMPLEX)
		{
			CLIPENUM ce;
			int moreClipLists; 
			RECTL *prclCurr;

			CLIPOBJ_cEnumStart(pco, TRUE, CT_RECTANGLES, CD_ANY, 0);
			for (ce.c = 0, moreClipLists=1; ce.c || moreClipLists;)
			{
				if (ce.c == 0)
				{
					moreClipLists = CLIPOBJ_bEnum(pco, sizeof(ce), (ULONG *)&ce );
					prclCurr = ce.arcl;
					if (!ce.c)
						continue;
				}

				parms.prclClip = prclCurr++;
				ce.c--;

				if( FAILED( sc1 = ClipBlt(pGPE, &parms)) )
				{
					DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving vStretchBlt\r\n")));
					break;
				}
			}
		}
		
		ClipObj_vOffset((XCLIPOBJ *)pco, prclDest->left-rclDst.left, prclDest->top - rclDst.top);
	}
	else 
		sc1 = ClipBlt(pGPE, &parms);
		
    if (FAILED(sc1))
    	goto errRtn;

	//finally copy everything back to dst if dst is device.
	if (psoDestMem != psoDest) {
		POINTL ptlSrc = {0,0};
		
		if (!vDrvBitBlt(psoDest, 
						psoDestMem, 
						(SURFOBJ *)NULL, 
						pco,
						(XLATEOBJ *)NULL, 
						&rclDstCopy, 
						&ptlSrc, 
						(POINTL *)NULL, 
						(BRUSHOBJ *)NULL, 
						(POINTL *)NULL, 
						0xcccc))
		{	
			goto errRtn;
		}
	}
	
	bRet = TRUE;

errRtn:
		if (psoSrcMem && (psoSrcMem != (SURFOBJ *)0xFFFFFFFF) && (psoSrcMem != psoSrc))
		{
			vDrvDeleteDeviceBitmap(psoSrcMem->dhsurf);
		}
		if (psoDestMem && (psoDestMem != (SURFOBJ *)0xFFFFFFFF) && (psoDestMem != psoDest))
		{
			vDrvDeleteDeviceBitmap(psoDestMem->dhsurf);
		}
		// if psoDstMem fail, it should already be cleared in vDrvCreateDeviceBitmap()
		return bRet;
 }



BOOL APIENTRY vDrvAnyBlt(SURFOBJ * psoDest, 
						  SURFOBJ * psoSrc, 
						  SURFOBJ * psoMask, 
						  CLIPOBJ * pco, 
						  XLATEOBJ * pxlo, 
						  POINTL * pptlHTOrg, 
						  RECTL * prclDest, 
						  RECTL * prclSrc, 
						  POINTL * pptlMask, 
						  BRUSHOBJ * pbo, 
						  POINTL * pptlBrush, 
						  ROP4 rop4, 
						  ULONG iMode,
						  ULONG  bltFlags)
{
	// need to take care of the SURFOBJ.iType.  assume SURF_TYPE_MULTI has multiple surfaces, i.e.
	// screen surface
	
	if ((prclDest->right == prclDest->left ) || 
		(prclDest->bottom == prclDest->top) ||
		(prclSrc->right == prclSrc->left) ||
		(prclSrc->bottom == prclSrc->top))
		return TRUE;
		
	if (((prclDest->right - prclDest->left) == (prclSrc->right - prclSrc->left)) &&
		((prclDest->bottom - prclDest->top) == (prclSrc->bottom - prclSrc->top)))
	{
		POINTL   ptlSrc = {prclSrc->left, prclSrc->top};
		return vDrvBitBltHelp(psoDest,
					psoSrc, 
					psoMask,
					pco, 
					pxlo, 
					prclDest, 
					&ptlSrc, 
					pptlMask, 
					pbo, 
					pptlBrush,
					rop4,
					bltFlags);
					
	}

	
	return vStretchBlt(psoDest, 
				 psoSrc, 
				 psoMask, 
				 pco, 
				 pxlo, 
				 pptlHTOrg, 
				 prclDest, 
				 prclSrc, 
				 pptlMask, 
				 pbo, 
				 pptlBrush, 
				 rop4,
				 iMode,
				 bltFlags);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvTransparentBlt()

Implement it!
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL APIENTRY vDrvTransparentBlt(SURFOBJ * psoDest, SURFOBJ * psoSrc, CLIPOBJ * pco, XLATEOBJ * pxlo, RECTL * prclDest, RECTL * prclSrc, ULONG TransColor)
{
	//right now, just call DrvAnyBlt() with special case just like in driver function;
	//eventually need some work to accelerate this function call without calling the 
	// whole horse power. 
	BRUSHOBJ bo;
	bo.iSolidColor = TransColor;
		
	return vDrvAnyBlt(psoDest,psoSrc,NULL,pco,pxlo,NULL,prclDest,prclSrc,NULL,&bo,NULL,0xCCCC,0,BLT_TRANSPARENT);
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvFillPath()

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL APIENTRY vDrvFillPath(SURFOBJ * pso, 
							PATHOBJ * ppo, 
							CLIPOBJ * pco, 
							BRUSHOBJ * pbo, 
							POINTL * pptlBrushOrg, 
							MIX mix, 
							FLONG flOptions)
{
	VDISPLAY *pvd = (VDISPLAY *)pso->dhpdev;
	BOOL     bRet = TRUE;
	MSURF 	 msurf;
	RECTL    rcl;
	RECTFX   rcfxBounds;
	BOOL     b1 = TRUE;

	if(pso->iType != SURF_TYPE_MULTI)
	{
	      return gGPEDrvEnableData.DrvFillPath(pso, ppo, pco, pbo,pptlBrushOrg, mix, flOptions);
	}

	PATHOBJ_vGetBounds(ppo, &rcfxBounds);

	rcl.left 	= FXTOL(rcfxBounds.xLeft);
	rcl.top  	= FXTOL(rcfxBounds.yTop);
	rcl.right	= FXTOL(rcfxBounds.xRight) + 1;
	rcl.bottom	= FXTOL(rcfxBounds.yBottom) + 1;
	
	if (msurf.bFindSurface(pso,pco,&rcl))
	{
		do {
		RestartEnumClip((XCLIPOBJ *)msurf.m_pco);
		
		LONG xOffset = msurf.m_pds->rcl.left;
		LONG yOffset = msurf.m_pds->rcl.top;
		Disp_Driver	*pdriver = (Disp_Driver *)msurf.m_pds->hdev;
		
		PathObj_vOffset((XPATHOBJ *)ppo, -xOffset, -yOffset);
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, -xOffset, -yOffset);

		pvd->byteBusyBits = 1 << msurf.m_pds->iDispSurf;
		bRet &= pdriver->m_drvenabledata.DrvFillPath(msurf.m_pso,ppo,msurf.m_pco,pbo,pptlBrushOrg,mix,flOptions);

		PathObj_vOffset((XPATHOBJ *)ppo, xOffset, yOffset);
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, xOffset, yOffset);
		} while (msurf.bNextSurface());
	}
	
	return bRet;
}




BOOL APIENTRY vDrvPaint(SURFOBJ * pso, CLIPOBJ * pco, BRUSHOBJ * pbo, POINTL * pptlBrush, MIX mix)
{
	ROP4 rop4;

	rop4 = (((ROP4)gaMix[(mix >> 8)&0xf]) << 8) | gaMix[mix & 0xf];
	return vDrvBitBlt(pso,NULL,NULL,pco,NULL,&pco->rclBounds,NULL,NULL,pbo,pptlBrush,rop4);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvRealizeBrush

do we need multi brushes for multi displays separately????? right now all the brushes functions
takes up 

psoPattern from m_pBrush(BRUSH)->m_pBitmap, so should be MBITMAP
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


BOOL APIENTRY vDrvRealizeBrush(BRUSHOBJ * pbo, 
						        SURFOBJ * psoTarget, 
						        SURFOBJ * psoPattern, 
						        SURFOBJ * psoMask, 
						        XLATEOBJ * pxlo, 
						        ULONG     iHatch)
{	
	return gGPEDrvEnableData.DrvRealizeBrush(pbo, psoTarget,psoPattern,psoMask,pxlo,iHatch);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvStrokePath


++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL APIENTRY vDrvStrokePath(SURFOBJ * pso, 
							  PATHOBJ * ppo, 
							  CLIPOBJ * pco, 
							  XFORMOBJ * pxo, 
							  BRUSHOBJ * pbo, 
							  POINTL * pptlBrushOrg, 
							  LINEATTRS * plineattrs, 
							  MIX mix)
{
	VDISPLAY *pvd = (VDISPLAY *)pso->dhpdev;
	BOOL bRet = TRUE;
	FLOAT_LONG elStyleState;
	MSURF msurf;
	RECTL rcl;
	RECTFX rcfx;
	PATHOBJ_vGetBounds(ppo, &rcfx);

	if (pso->iType != SURF_TYPE_MULTI)
	{
		return	gGPEDrvEnableData.DrvStrokePath(pso, 
						ppo, 
						pco, 
						pxo, 
						pbo, 
						pptlBrushOrg,
						plineattrs, 
						mix);
	}
	

	rcl.left = FXTOL(rcfx.xLeft);
	rcl.top = FXTOL(rcfx.yTop);
	rcl.right = FXTOL(rcfx.xRight) + 1;
	rcl.bottom = FXTOL(rcfx.yBottom) + 1;

	if (plineattrs)
		elStyleState = plineattrs->elStyleState;
	
	if (msurf.bFindSurface(pso, pco, &rcl))
	{
		do {
		RestartEnumClip((XCLIPOBJ *)msurf.m_pco);	
		LONG xOffset = msurf.m_pds->rcl.left;
		LONG yOffset = msurf.m_pds->rcl.top;

		PathObj_vOffset((XPATHOBJ *)ppo, -xOffset, -yOffset);
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, -xOffset, -yOffset);
		
		if (plineattrs)
			plineattrs->elStyleState = elStyleState;
		Disp_Driver	*pdd = (Disp_Driver *)msurf.m_pds->hdev;

		pvd->byteBusyBits = 1 << msurf.m_pds->iDispSurf;
		bRet &= pdd->m_drvenabledata.DrvStrokePath(msurf.m_pso,ppo,msurf.m_pco,pxo,pbo,pptlBrushOrg,plineattrs,mix);

		PathObj_vOffset((XPATHOBJ *)ppo, xOffset, yOffset);
		ClipObj_vOffset((XCLIPOBJ *)msurf.m_pco, xOffset, yOffset);	
		} while (msurf.bNextSurface());
	}

	return bRet;
}


ULONG APIENTRY vDrvRealizeColor(USHORT iDstType, ULONG cEntries, ULONG * pPalette, ULONG rgbColor)
{
	
	return gGPEDrvEnableData.DrvRealizeColor(iDstType, cEntries, pPalette, rgbColor);	
}


ULONG APIENTRY vDrvUnrealizeColor(USHORT iSrcType, ULONG cEntries, ULONG * pPalette, ULONG iRealizedColor)
{
	
	return gGPEDrvEnableData.DrvUnrealizeColor(iSrcType, cEntries, pPalette, iRealizedColor);		
}

BOOL APIENTRY vDrvContrastControl(DHPDEV dhpdev, ULONG cmd, ULONG * pValue)
{
	//in gwe_s.cpp it calls DispDrvrContrastControl(CONTRAST_CMD_GET), then 
	// DispDrvrContrastControl(CONTRAST_CMD_SET).
	
	//for multimon, we could have more than one monitors
	//so should we get different contrast value for different
	//monitors or they share the same????? 
	//right now, let them share the same!! so the implementation is sth like this.
	
	VDISPLAY *pvd;
	Disp_Driver   *pdd;
	BOOL bRet = TRUE;

	pvd = (VDISPLAY *)dhpdev;

	if (cmd == CONTRAST_CMD_GET)
	{
		
		bRet &= gGPEDrvEnableData.DrvContrastControl(dhpdev,cmd,pValue);
	} else if ( cmd == CONTRAST_CMD_SET)
	{
		//need to loop all the display drivers and set them!!
		for (int i=0; i < pvd->m_cSurfaces; i ++)
		{
			pdd = (Disp_Driver *)pvd->m_ds[i].hdev;
			bRet &= (pdd->m_drvenabledata.DrvContrastControl(pdd->m_surfobj.dhpdev,
														   cmd,
														   pValue));		
		}
	}

	return bRet;
}

VOID APIENTRY vDrvPowerHandler(DHPDEV dhpdev, BOOL bOff)
{
	//should not be called.  in Dispdrvr.cpp, it calls ForAllDrivers(pdriver)
	// to loop all the drivers and shut them off!!
	;
}

ULONG APIENTRY vDrvEscape(DHPDEV dhpdev, SURFOBJ * pso, ULONG iEsc, ULONG cjIn, PVOID pvIn, ULONG cjOut, PVOID pvOut)
{
//no document for this function in CE
//what need to be filled in, should loop all the drivers?????
	VDISPLAY *pvd;
	ULONG     ul = 0;

	pvd = (VDISPLAY *)dhpdev;

	for (int i=0; i < pvd->m_cSurfaces; i ++)
	{
		Disp_Driver *pdd;

		pdd = (Disp_Driver *)pvd->m_ds[i].hdev;
		ul |= (pdd->m_drvenabledata.DrvEscape(pdd->m_surfobj.dhpdev,
														  &pdd->m_surfobj,
														  iEsc,
														  cjIn,
														  pvIn,
														  cjOut,
														  pvOut));
		
	}

	return ul;
}

ULONG *APIENTRY vDrvGetMasks(DHPDEV dhpdev)
{
	return gGPEDrvEnableData.DrvGetMasks(dhpdev);
}
//ULONG gBitMasks[] = { 0x0001, 0x0002, 0x0000};

// assume all the display cards are the same, i.e., have the same masks. 
ULONG *APIENTRY DrvGetMasks(DHPDEV dhpdev)
{
	return gGPEDrvEnableData.DrvGetMasks(dhpdev);
}

const DRVENABLEDATA vDrvFn = 
{
    {   vDrvEnablePDEV           },
    {   vDrvDisablePDEV          },
    {   vDrvEnableSurface        },
    {   vDrvDisableSurface       },
    {   vDrvCreateDeviceBitmap   },
    {   vDrvDeleteDeviceBitmap   },
    {   vDrvRealizeBrush         },
    {   vDrvStrokePath           },
    {   vDrvFillPath             },
    {   vDrvPaint                },
    {   vDrvBitBlt               },
    {   vDrvCopyBits             },
    {   vDrvAnyBlt               },
    {   vDrvTransparentBlt       },
    {   vDrvSetPalette           },
    {   vDrvSetPointerShape      },
    {   vDrvMovePointer          },
    {   vDrvGetModes             },
    {   vDrvRealizeColor         },
    {   vDrvGetMasks             },
    {   vDrvUnrealizeColor       },
    {   vDrvContrastControl      },
    {   vDrvPowerHandler         },
    {   NULL /* DrvEndDoc    */ },
    {   NULL /* DrvStartDoc  */ },
    {   NULL /* DrvStartPage */ },
    {   vDrvEscape               }	
};


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
vDrvEnableDriver()
	== entry point for multimon driver
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL vDrvEnableDriver(ULONG iEngineVersion, ULONG cj, DRVENABLEDATA *pded,
	PENGCALLBACKS  pEngCallbacks)
{
	HINSTANCE hinstance = NULL;
	
	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering vDrvEnableDriver\r\n")));

	GPEEnableDriver(iEngineVersion,cj, &gGPEDrvEnableData,pEngCallbacks);

	if ( iEngineVersion != DDI_DRIVER_VERSION || cj != sizeof(DRVENABLEDATA) )
		return FALSE;

	*pded = vDrvFn;

	DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving vDrvEnableDriver\r\n")));

	return TRUE;
}



BOOL APIENTRY DrvEnableDriver(ULONG engineVersion, ULONG cj, DRVENABLEDATA *pdrvData,
									PENGCALLBACKS engineCallbacks)
{
	return vDrvEnableDriver(engineVersion, cj, pdrvData, engineCallbacks);
}


void	RegisterDDHALAPI(void)
{
	return;	// no DDHAL support
}






