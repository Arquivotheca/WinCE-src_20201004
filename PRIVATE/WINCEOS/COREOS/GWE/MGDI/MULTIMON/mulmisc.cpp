//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "precomp.h"


#ifdef DEBUG
DBGPARAM dpCurSettings = {
	TEXT("GWE Server"), {
		TEXT("WndClass"),		//	0	0x0001
		TEXT("WndMgrServer"),	//	1	0x0002
		TEXT("WndMgrClient"),	//	2	0x0004
		TEXT("NCArea"),			//	3	0x0008
		TEXT("Menus"),			//	4	0x0010
		TEXT("DlgMgr"),			//	5	0x0020
		TEXT("Warnings"),		//	6	0x0040
		TEXT("WM Srv Dump"),	//	7	0x0080
		TEXT("WM Srv Details"),	//	8	0x0100
		TEXT("Regions"),		//	9	0x0200
		TEXT("Taskbar"),		//	10	0x0400
		TEXT("ActiveWindow"),	//	11	0x0800
		TEXT("Keyboard"),		//	12	0x1000
		TEXT("MsgAPI"), 		//	13	0x2000
		TEXT("MsgDetail1"),		//	14	0x4000
#ifdef FAREAST // FE_IME
		TEXT("IMM API")			//  15  0x8000
		},
	0x00000040
#else // FAREAST
		TEXT("TimerAPI") ,		//  15  0x8000
//		TEXT("TimerThread"),	//	16	0x10000
//		TEXT("Caret"),			//	17	0x20000
//		TEXT("Accelerators"),	//	18	0x40000
//		TEXT("Touch"),			//	19	0x80000
//		TEXT("F10 Input Dump")	//	20	0x100000
		},	
	0x00000040
#endif // !FAREAST
};
#endif


inline void RectlToRect(RECT *prc, RECTL *prcl)
{
	prc->left = prcl->left;
	prc->right = prcl->right;
	prc->top = prcl->top;
	prc->bottom = prcl->bottom;
}

/*
  for now we define the grcVirtualScreen to be sth like this
  |____________________________________________|_
  |           |        |         |             |^
  | primary   |  2nd   |  3rd    |    4th      || 
  | monitor   |monitor | monitor |  monitor    ||
  |           |        |         |             ||
  |           |        |---------|             |
  |           |        |         |             |SM_CYVIRTUALSCREEN
  |-----------|        |         |             |  
  |<--------->|--------|         |             ||
  |sm_cxscreen|                  |             ||  
  |                              |             || 
  |------------------------------|-------------|--
  |<---------- SM_CXVIRTUALSCREEN ------------>|
*/    


VDISPLAY::VDISPLAY(MULTIDEVMODE *pMode, HANDLE hdriver)
{
	Disp_Driver *pdriver;
	DISPSURF *pds;
	
	m_cSurfaces = pMode->cDev;
	
	m_rcVScreen.bottom = m_rcVScreen.top = m_rcVScreen.left = m_rcVScreen.right = 0;
	m_flGraphicsCaps = 0xffffffff;
	pds = NULL;
	
	for (int i=0; i < m_cSurfaces; i++)
	{
	    pdriver= (Disp_Driver *)(pMode->dev[i].hDev);
			
	    m_flGraphicsCaps &= pdriver->m_flGraphicsCaps;
	    CopyRect((RECT *)&m_ds[i].rcl, (RECT *)&pMode->dev[i].rcl);
	    m_ds[i].hdev = pMode->dev[i].hDev;
	   	m_ds[i].pso = &pdriver->m_surfobj;
	   	m_ds[i].iDispSurf = i;
	    m_ds[i].pdsBltNext = pds;  				//initialize the blt list for later blits function
		pds = &m_ds[i];

	    RectlToRect(&pdriver->m_rcMonitor, &m_ds[i].rcl);
	    pdriver->m_rcWork = pdriver->m_rcMonitor;
	    UnionRect((RECT *)&m_rcVScreen,(RECT *)&m_rcVScreen, (RECT *)&m_ds[i].rcl);
	}

	pdsBlt = pds;
	hdriver = hdriver;
	// right now assume they have the same bitmap format!
	m_iBpp = pMode->devmodew.dmBitsPerPel;		

//initialize the m_co for the vdisplay
	RECTL rcl = m_rcVScreen;
	rcl.right ++;
	rcl.bottom ++;
	
	m_co.fjOptions = 0;
	m_co.iDComplexity = DC_TRIVIAL;
	m_co.iFComplexity = 0;
	m_co.iMode = 0;
	m_co.iUniq = 0;
	m_co.rclBounds = rcl;
	m_co.m_prgnd = (RGNDAT *)(new BYTE[sizeof(RGNDATHEADER) + sizeof(RECTS)]);
	m_co.m_prgnd->rdh.nCount = 1;
	m_co.m_prgnd->rdh.rcBound.left = (INT16)rcl.left;
	m_co.m_prgnd->rdh.rcBound.top = (INT16)rcl.top;
	m_co.m_prgnd->rdh.rcBound.right = (INT16)rcl.right;
	m_co.m_prgnd->rdh.rcBound.bottom = (INT16)rcl.bottom;
	m_co.m_prgnd->rdh.sortorder = 0;  //is 0 the right one?????
	RECTS *prcs = (RECTS *)&m_co.m_prgnd->Buffer;
	*prcs = m_co.m_prgnd->rdh.rcBound;
	
	
	//initialize the GPE member data
	m_pPrimarySurface = NULL; 
	m_nScreenWidth = m_rcVScreen.right - m_rcVScreen.left;
	m_nScreenHeight = m_rcVScreen.bottom - m_rcVScreen.top;
	m_pMode = NULL;
	m_hSurf = NULL;  // will set later	
}

VDISPLAY::~VDISPLAY()
{
	if (m_co.m_prgnd)
		delete m_co.m_prgnd;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
which display surfobj contains this point?
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

DISPSURF *VDISPLAY::PDispFromPoint(long x, long y)
{
	RECTL   *prcl;

	for (int i=0; i < m_cSurfaces; i ++)
	{
		prcl = &m_ds[i].rcl;
		//the right and bottom line is not included in the monitor RECT
		if ( (x < prcl->left) || (x > prcl->right)
			|| (y < prcl->top) || (y > prcl->bottom))
			continue;

		break;	
	}

	if (i < m_cSurfaces)
		return (DISPSURF *)&m_ds[i];

	return NULL;
}

DISPSURF *VDISPLAY::GetBoundPointInScreen(long *px, long *py, long dirX, long dirY)
{
	
	long x = *px;
	long y = *py;
	
	if ( x < m_rcVScreen.left )
		*px = m_rcVScreen.left;
	else if ( x >= m_rcVScreen.right)
		*px = m_rcVScreen.right - 1;

	if ( y < m_rcVScreen.top )
		*py = m_rcVScreen.top;
	else if (y > m_rcVScreen.bottom)
		*py = m_rcVScreen.bottom - 1;

	return PDispFromPoint(*px, *py);
}



SCODE VDISPLAY::BltPrepare(GPEBltParms * pBltParms)
{
	pBltParms->pBlt = EmulatedBlt;
	return S_OK;
}

SCODE VDISPLAY::BltComplete(GPEBltParms * pBltParms)
{
	DEBUGMSG(GPE_ZONE_INIT, (TEXT("VDISPLAY::BltComplete\r\n")));
	return S_OK;
}

SCODE VDISPLAY::Line(GPELineParms *pLineParms,	EGPEPhase phase)
{
        if (phase == gpeSingle || phase == gpePrepare)
		pLineParms->pLine = EmulatedLine;
		
	return S_OK;
}

SCODE VDISPLAY::AllocSurface(GPESurf * * ppSurf, int width, int height, EGPEFormat format, int surfaceFlags)
{	
	//won't use it
	return S_OK;
}


SCODE VDISPLAY::SetPointerShape(GPESurf * pMask, GPESurf * pColorSurf, int xHot, int yHot, int cx, int cy)
{
	return S_OK;
}

SCODE VDISPLAY::MovePointer(int x, int y)
{
	return S_OK;
}

SCODE VDISPLAY::SetPalette(const PALETTEENTRY *src, unsigned short firstEntry,unsigned short numEntries)
{
	return S_OK;
}

SCODE VDISPLAY::GetModeInfo(GPEMode *pMode, int modeNo)
{
	return S_OK;
}

int VDISPLAY::NumModes()
{
	return 0;
}

SCODE VDISPLAY::SetMode(int modeId, HPALETTE * pPaletteHandle)
{
	return S_OK;
}

int    VDISPLAY::InVBlank()
{
	return 0;
}

void VDISPLAY::WaitForNotBusy()
{
	Disp_Driver *pdriver = NULL;
	if (byteBusyBits & 0x1)
	{
		pdriver = (Disp_Driver *)m_ds[0].hdev;
		((GPE *)(pdriver->m_dhpdev))->WaitForNotBusy();
	}
	if (byteBusyBits & 0x2)
	{
		pdriver = (Disp_Driver *)m_ds[1].hdev;
		((GPE *)(pdriver->m_dhpdev))->WaitForNotBusy();
	}
	if (byteBusyBits & 0x4)
	{
		pdriver = (Disp_Driver *)m_ds[2].hdev;
		((GPE *)(pdriver->m_dhpdev))->WaitForNotBusy();
	}
	if (byteBusyBits & 0x8)
	{
		pdriver = (Disp_Driver *)m_ds[3].hdev;
		((GPE *)(pdriver->m_dhpdev))->WaitForNotBusy();
	}
}
