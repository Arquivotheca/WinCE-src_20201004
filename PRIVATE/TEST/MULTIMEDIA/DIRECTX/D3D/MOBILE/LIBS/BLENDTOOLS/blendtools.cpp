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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <tchar.h>
#include "BlendTools.h"
#include "BufferTools.h"
#include "DebugOutput.h"

// 
// FloatToByteClamp
// 
//   Useful utility for color channel computations; clamps a float to top or bottom of
//   color range, if needed.
// 
// Arguments:
// 
//   float fValue:  Computed color component value
//   
// Return Value:
//   
//   BYTE:  Clamped color component value
// 
BYTE FloatToByteClamp(float fValue)
{
	if (fValue > 255.0f)
		return 0xFF;
	else if (fValue < 0.0f)
		return 0x00;
	else
		return (BYTE)fValue;
}

// 
// FloatToScaleClampByte
// 
//   Useful utility for color channel computations; scales a float from [0.0f, 1.0f] to
//   [0,255], clamping if needed.
// 
// Arguments:
// 
//   float fValue:  Computed color component value
//   
// Return Value:
//   
//   BYTE:  Clamped color component value
// 
BYTE FloatToScaleClampByte(float fValue)
{
	fValue *= 255.0f;

	return FloatToByteClamp(fValue);
}



// 
// PerformBlendOp
// 
//   Given a pre-scaled source and dest set of color components, blend.
// 
// Arguments:
// 
//   D3DMBLENDOP Blend
//   DWORD dwBlendFor
//   BYTE bRedSrc, BYTE bGreenSrc, BYTE bBlueSrc, BYTE bAlphaSrc
//   BYTE bRedDst, BYTE bGreenDst, BYTE bBlueDst, BYTE bAlphaDst
//   float *pfRed, float *pfGreen, float *pfBlue, float *pfAlpha
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT PerformBlendOp(D3DMBLENDOP BlendOp,
				       float fRedSrc, float fGreenSrc, float fBlueSrc, float fAlphaSrc,
				       float fRedDst, float fGreenDst, float fBlueDst, float fAlphaDst,
				       float *pfRed,  float *pfGreen,  float *pfBlue,  float *pfAlpha)
{
	switch((DWORD)BlendOp)
	{
	case D3DMBLENDOP_ADD:
		*pfRed   = fRedSrc + fRedDst;
		*pfGreen = fGreenSrc + fGreenDst;
		*pfBlue  = fBlueSrc + fBlueDst;
		*pfAlpha = fAlphaSrc + fAlphaDst;
		break;
	case D3DMBLENDOP_SUBTRACT:
		*pfRed   = fRedSrc - fRedDst;
		*pfGreen = fGreenSrc - fGreenDst;
		*pfBlue  = fBlueSrc - fBlueDst;
		*pfAlpha = fAlphaSrc - fAlphaDst;
		break;
	case D3DMBLENDOP_REVSUBTRACT:
		*pfRed   = fRedDst - fRedSrc;
		*pfGreen = fGreenDst - fGreenSrc;
		*pfBlue  = fBlueDst - fBlueSrc;
		*pfAlpha = fAlphaDst - fAlphaSrc;
		break;
	case D3DMBLENDOP_MIN:
		*pfRed   = (fRedSrc > fRedDst)     ? fRedDst   : fRedSrc;
		*pfGreen = (fGreenSrc > fGreenDst) ? fGreenDst : fGreenSrc;
		*pfBlue  = (fBlueSrc > fBlueDst)   ? fBlueDst  : fBlueSrc;
		*pfAlpha = (fAlphaSrc > fAlphaDst) ? fAlphaDst : fAlphaSrc;
		break;
	case D3DMBLENDOP_MAX:
		*pfRed   = (fRedSrc > fRedDst)     ? fRedSrc   : fRedDst;
		*pfGreen = (fGreenSrc > fGreenDst) ? fGreenSrc : fGreenDst;
		*pfBlue  = (fBlueSrc > fBlueDst)   ? fBlueSrc  : fBlueDst;
		*pfAlpha = (fAlphaSrc > fAlphaDst) ? fAlphaSrc : fAlphaDst;
		break;
	default:
		return E_FAIL;
		break;
	}

	return S_OK;
}

// 
// PerformBlendOp
// 
//   Wrapper for float implementation of PerformBlendOp
//
HRESULT PerformBlendOp(D3DMBLENDOP BlendOp,
				       float fRedSrc, float fGreenSrc, float fBlueSrc, float fAlphaSrc,
				       float fRedDst, float fGreenDst, float fBlueDst, float fAlphaDst,
				       BYTE *pbRed,   BYTE *pbGreen,   BYTE *pbBlue,   BYTE *pbAlpha)
{
	float fRed, fGreen, fBlue, fAlpha;

	if (FAILED(PerformBlendOp(BlendOp,
				              fRedSrc, fGreenSrc, fBlueSrc, fAlphaSrc,
       				          fRedDst, fGreenDst, fBlueDst, fAlphaDst,
				              &fRed, &fGreen, &fBlue, &fAlpha)))
		  return E_FAIL;

	*pbRed   = FloatToScaleClampByte(fRed  );
	*pbGreen = FloatToScaleClampByte(fGreen);
	*pbBlue  = FloatToScaleClampByte(fBlue );
	*pbAlpha = FloatToScaleClampByte(fAlpha);

	return S_OK;
}


// 
// Blend
// 
//   Performs blending; based on source blend, dest blend, blend op,
//   and color inputs.
// 
// Arguments:
// 
//   D3DMBLENDOP BlendOp:  Blend operation
//   D3DMBLEND BlendSrc:  Source blend state
//   D3DMBLEND BlendDst:  Dest blend state
//   DWORD dwColorSrc:  RGBA source input
//   DWORD dwColorDst:  RGBA dest input
//   DWORD *pdwColorResult:  RGBA output (result of blend)
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT Blend(D3DMBLENDOP BlendOp,    D3DMBLEND BlendSrc, D3DMBLEND BlendDst,
			  DWORD dwColorSrc, DWORD dwColorDst, DWORD *pdwColorResult)
{
	float fRedSrc, fGreenSrc, fBlueSrc, fAlphaSrc;
	float fRedDst, fGreenDst, fBlueDst, fAlphaDst;
	BYTE RedRes, GreenRes, BlueRes, AlphaRes;

	if ((BlendOp != D3DMBLENDOP_MIN) &&
		(BlendOp != D3DMBLENDOP_MAX))
	{
	
		//
		// Compute scaled source
		// 
		if (FAILED(BlendColor(BlendSrc,
							D3DMRS_SRCBLEND,
							D3D_GETR(dwColorSrc), D3D_GETG(dwColorSrc), D3D_GETB(dwColorSrc), D3D_GETA(dwColorSrc), 
							D3D_GETR(dwColorDst), D3D_GETG(dwColorDst), D3D_GETB(dwColorDst), D3D_GETA(dwColorDst), 
							&fRedSrc,  &fGreenSrc,  &fBlueSrc,  &fAlphaSrc)))
		{
			DebugOut(DO_ERROR, _T("BlendColor failed."));
			return E_FAIL;
		}

		//
		// Compute scaled dest
		// 
		if (FAILED(BlendColor(BlendDst,
							D3DMRS_DESTBLEND,
							D3D_GETR(dwColorSrc), D3D_GETG(dwColorSrc), D3D_GETB(dwColorSrc), D3D_GETA(dwColorSrc), 
							D3D_GETR(dwColorDst), D3D_GETG(dwColorDst), D3D_GETB(dwColorDst), D3D_GETA(dwColorDst), 
							&fRedDst, &fGreenDst, &fBlueDst, &fAlphaDst)))
		{
			DebugOut(DO_ERROR, _T("BlendColor failed."));
			return E_FAIL;
		}
	}
	else
	{
		// Special case for MIN, MAX; no blend multiplication prior to blend op

		fRedSrc   = (float)D3D_GETR(dwColorSrc) / 255.0f;
		fGreenSrc = (float)D3D_GETG(dwColorSrc) / 255.0f;
		fBlueSrc  = (float)D3D_GETB(dwColorSrc) / 255.0f;
		fAlphaSrc = (float)D3D_GETA(dwColorSrc) / 255.0f;

		fRedDst   = (float)D3D_GETR(dwColorDst) / 255.0f;
		fGreenDst = (float)D3D_GETG(dwColorDst) / 255.0f;
		fBlueDst  = (float)D3D_GETB(dwColorDst) / 255.0f;
		fAlphaDst = (float)D3D_GETA(dwColorDst) / 255.0f;
	}
	
	//
	// Blend source and dest
	// 
	if (FAILED(PerformBlendOp(BlendOp ,
				              fRedSrc ,  fGreenSrc,  fBlueSrc, fAlphaSrc,
				              fRedDst ,  fGreenDst,  fBlueDst, fAlphaDst,
						      &RedRes ,  &GreenRes,  &BlueRes, &AlphaRes)))
	{
		DebugOut(DO_ERROR, _T("PerformBlendOp failed."));
		return E_FAIL;
	}

	*pdwColorResult = D3DMCOLOR_RGBA(RedRes, GreenRes, BlueRes, AlphaRes);

	return S_OK;
}

// 
// BlendColor
// 
//   Compute a scaled pixel value to prepare for blending.
// 
// Arguments:
// 
//   D3DMBLEND Blend
//   DWORD dwBlendFor
//   BYTE bRedSrc, BYTE bGreenSrc, BYTE bBlueSrc, BYTE bAlphaSrc
//   BYTE bRedDst, BYTE bGreenDst, BYTE bBlueDst, BYTE bAlphaDst
//   float *pfRed, float *pfGreen, float *pfBlue, float *pfAlpha
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT BlendColor(D3DMBLEND Blend,
				   DWORD dwBlendFor,
				   BYTE bRedSrc, BYTE bGreenSrc, BYTE bBlueSrc, BYTE bAlphaSrc,
				   BYTE bRedDst, BYTE bGreenDst, BYTE bBlueDst, BYTE bAlphaDst,
				   float *pfRed, float *pfGreen, float *pfBlue, float *pfAlpha)
{
	float fTemp = 0.0f;

	switch((DWORD)Blend)
	{
	case D3DMBLEND_ZERO:
		*pfRed = 0.0f;
		*pfGreen = 0.0f;
		*pfBlue = 0.0f;
		*pfAlpha = 0.0f;
		break;
	case D3DMBLEND_ONE:
		*pfRed   = 1.0f;
		*pfGreen = 1.0f;
		*pfBlue  = 1.0f;
		*pfAlpha = 1.0f;
		break;
	case D3DMBLEND_SRCCOLOR:
		*pfRed   = (float)bRedSrc   / 255.0f;
		*pfGreen = (float)bGreenSrc / 255.0f;
		*pfBlue  = (float)bBlueSrc  / 255.0f;
		*pfAlpha = (float)bAlphaSrc / 255.0f;
		break;
	case D3DMBLEND_INVSRCCOLOR:
		*pfRed   = 1.0f - ((float)bRedSrc   / 255.0f);
		*pfGreen = 1.0f - ((float)bGreenSrc / 255.0f);
		*pfBlue  = 1.0f - ((float)bBlueSrc  / 255.0f);
		*pfAlpha = 1.0f - ((float)bAlphaSrc / 255.0f);
		break;
	case D3DMBLEND_SRCALPHA:
		*pfRed   = (float)bAlphaSrc / 255.0f;
		*pfGreen = (float)bAlphaSrc / 255.0f;
		*pfBlue  = (float)bAlphaSrc / 255.0f;
		*pfAlpha = (float)bAlphaSrc / 255.0f;
		break;
	case D3DMBLEND_INVSRCALPHA:
		*pfRed   = 1.0f - ((float)bAlphaSrc / 255.0f);
		*pfGreen = 1.0f - ((float)bAlphaSrc / 255.0f);
		*pfBlue  = 1.0f - ((float)bAlphaSrc / 255.0f);
		*pfAlpha = 1.0f - ((float)bAlphaSrc / 255.0f);
		break;
	case D3DMBLEND_DESTALPHA:
		*pfRed   = (float)bAlphaDst / 255.0f;
		*pfGreen = (float)bAlphaDst / 255.0f;
		*pfBlue  = (float)bAlphaDst / 255.0f;
		*pfAlpha = (float)bAlphaDst / 255.0f;
		break;
	case D3DMBLEND_INVDESTALPHA:
		*pfRed   = 1.0f - ((float)bAlphaDst / 255.0f);
		*pfGreen = 1.0f - ((float)bAlphaDst / 255.0f);
		*pfBlue  = 1.0f - ((float)bAlphaDst / 255.0f);
		*pfAlpha = 1.0f - ((float)bAlphaDst / 255.0f);
		break;
	case D3DMBLEND_DESTCOLOR:
		*pfRed   = (float)bRedDst   / 255.0f;
		*pfGreen = (float)bGreenDst / 255.0f;
		*pfBlue  = (float)bBlueDst  / 255.0f;
		*pfAlpha = (float)bAlphaDst / 255.0f;
		break;
	case D3DMBLEND_INVDESTCOLOR:
		*pfRed   = 1.0f - ((float)bRedDst   / 255.0f);
		*pfGreen = 1.0f - ((float)bGreenDst / 255.0f);
		*pfBlue  = 1.0f - ((float)bBlueDst  / 255.0f);
		*pfAlpha = 1.0f - ((float)bAlphaDst / 255.0f);
		break;
	case D3DMBLEND_SRCALPHASAT:

		fTemp = (1.0f - ((float)bAlphaDst / 255.0f));
		if (((float)bAlphaSrc / 255.0f) < fTemp)
			fTemp = ((float)bAlphaSrc / 255.0f);

		*pfRed   = fTemp;
		*pfGreen = fTemp;
		*pfBlue  = fTemp;
		*pfAlpha = 1.0f;
		break;
	default:
		return E_FAIL;
		break;
	}

	switch (dwBlendFor)
	{
	case (DWORD)D3DMRS_SRCBLEND:
		*pfRed   *= ((float)bRedSrc   / 255.0f);
		*pfGreen *= ((float)bGreenSrc / 255.0f);
		*pfBlue  *= ((float)bBlueSrc  / 255.0f);
		*pfAlpha *= ((float)bAlphaSrc / 255.0f);
		break;
	case (DWORD)D3DMRS_DESTBLEND:
		*pfRed   *= ((float)bRedDst   / 255.0f);
		*pfGreen *= ((float)bGreenDst / 255.0f);
		*pfBlue  *= ((float)bBlueDst  / 255.0f);
		*pfAlpha *= ((float)bAlphaDst / 255.0f);
		break;
	default:
		return E_FAIL;
		break;
	}

	return S_OK;
}

// 
// IsBlendSupported
// 
//   Checks capability bits to determine support (or lack thereof) of
//   a particular blend.
// 
// Arguments:
// 
//   D3DMBLEND Blend
//   DWORD dwCaps:  Caps bits for either SrcBlendCaps or DestBlendCaps
//   
// Return Value:
//   
//   bool:  true if blend is supported, false otherwise
// 
bool IsBlendSupported(D3DMBLEND Blend, DWORD dwCaps)
{
	switch(Blend)
	{
	case D3DMBLEND_DESTALPHA:
		return ((dwCaps & D3DMPBLENDCAPS_DESTALPHA) ? true : false);
	case D3DMBLEND_DESTCOLOR:
		return ((dwCaps & D3DMPBLENDCAPS_DESTCOLOR) ? true : false);
	case D3DMBLEND_INVDESTALPHA:
		return ((dwCaps & D3DMPBLENDCAPS_INVDESTALPHA) ? true : false);
	case D3DMBLEND_INVDESTCOLOR:
		return ((dwCaps & D3DMPBLENDCAPS_INVDESTCOLOR) ? true : false);
	case D3DMBLEND_INVSRCALPHA:
		return ((dwCaps & D3DMPBLENDCAPS_INVSRCALPHA) ? true : false);
	case D3DMBLEND_INVSRCCOLOR:
		return ((dwCaps & D3DMPBLENDCAPS_INVSRCCOLOR) ? true : false);
	case D3DMBLEND_ONE:
		return ((dwCaps & D3DMPBLENDCAPS_ONE) ? true : false);
	case D3DMBLEND_SRCALPHA:
		return ((dwCaps & D3DMPBLENDCAPS_SRCALPHA) ? true : false);
	case D3DMBLEND_SRCALPHASAT:
		return ((dwCaps & D3DMPBLENDCAPS_SRCALPHASAT) ? true : false);
	case D3DMBLEND_SRCCOLOR:
		return ((dwCaps & D3DMPBLENDCAPS_SRCCOLOR) ? true : false);
	case D3DMBLEND_ZERO:	
		return ((dwCaps & D3DMPBLENDCAPS_ZERO) ? true : false);
	default:
		return false;
	}
}


// 
// IsDestBlendSupported
// 
//   Wrapper for IsBlendSupported; to check support for dest blend support.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   D3DMBLEND Blend:  Dest blend state
// 
// Return Value:
//
//   true if BLEND is supported; false otherwise
//
bool IsDestBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND Blend)
{
    HRESULT hr;
	D3DMCAPS Caps;

	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
	    DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
		return false;
    }

	return IsBlendSupported(Blend,Caps.DestBlendCaps);
}

// 
// IsSrcBlendSupported
// 
//   Wrapper for IsBlendSupported; to check support for source blend support.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   D3DMBLEND Blend:  Source blend state
// 
// Return Value:
//
//   true if BLEND is supported; false otherwise
//
bool IsSrcBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND Blend)
{
    HRESULT hr;
	D3DMCAPS Caps;

	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
	    DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
		return false;
    }

	return IsBlendSupported(Blend,Caps.SrcBlendCaps);
}

// 
// IsBlendOpSupported
// 
//   Indicates whether or not a particular blend op is supported; based
//   on device capabilities.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   D3DMBLENDOP BlendOp:  Blend operation
// 
// Return Value:
//
//   true if BLENDOP is supported; false otherwise
//
bool IsBlendOpSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLENDOP BlendOp)
{
    HRESULT hr;
	D3DMCAPS Caps;

	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
	    DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
		return false;
    }

	switch(BlendOp)
	{
	case D3DMBLENDOP_ADD:
		return ((Caps.BlendOpCaps & D3DMBLENDOPCAPS_ADD) ? true : false);
	case D3DMBLENDOP_SUBTRACT:
		return ((Caps.BlendOpCaps & D3DMBLENDOPCAPS_SUBTRACT) ? true : false);
	case D3DMBLENDOP_REVSUBTRACT:
		return ((Caps.BlendOpCaps & D3DMBLENDOPCAPS_REVSUBTRACT) ? true : false);
	case D3DMBLENDOP_MIN:
		return ((Caps.BlendOpCaps & D3DMBLENDOPCAPS_MIN) ? true : false);
	case D3DMBLENDOP_MAX:
		return ((Caps.BlendOpCaps & D3DMBLENDOPCAPS_MAX) ? true : false);
	default:
		return false;
	}

}


// 
// IsBlendSupported
// 
//   Indicates whether or not a particular combination of D3DMBLENDOP, with D3DMBLEND source and
//   D3DMBLEND dest, is supported.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   D3DMBLEND SourceBlend:  Source blend state
//   D3DMBLEND DestBlend:  Dest blend state
//   D3DMBLENDOP BlendOp:  Blend operation
// 
// Return Value:
//
//   true if supported; false otherwise
//
bool IsBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND SourceBlend, D3DMBLEND DestBlend, D3DMBLENDOP BlendOp)
{
	if (!(IsDestBlendSupported(pDevice, DestBlend)))
	{
		DebugOut(DO_ERROR, _T("Destination blend not supported."));
		return false;
	}
	if (!(IsSrcBlendSupported(pDevice, SourceBlend)))
	{
		DebugOut(DO_ERROR, _T("Source blend not supported."));
		return false;
	}
	if (!(IsBlendOpSupported(pDevice, BlendOp)))
	{
		DebugOut(DO_ERROR, _T("Blend op not supported."));
		return false;
	}

	return true;
}
