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
#include <windows.h>
#include <d3dm.h>
#include "DebugOutput.h"

// 
// IsTexOpSupported
// 
//   Checks capability bits to determine support (or lack thereof) of
//   a particular texture operation.
// 
// Arguments:
// 
//   D3DMTEXTUREOP TexOp
//   
// Return Value:
//   
//   BOOL:  TRUE if supported; FALSE otherwise
// 
BOOL IsTexOpSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMTEXTUREOP TexOp)
{
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	HRESULT hr;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		return FALSE;
	}

	//
	// Check caps bit corresponding to texture op
	//
	switch(TexOp)
	{
	case D3DMTOP_DISABLE:
		return TRUE;
	case D3DMTOP_SELECTARG1:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_SELECTARG1) ? TRUE : FALSE);
	case D3DMTOP_SELECTARG2:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_SELECTARG2) ? TRUE : FALSE);
	case D3DMTOP_MODULATE:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATE) ? TRUE : FALSE);
	case D3DMTOP_MODULATE2X:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATE2X) ? TRUE : FALSE);
	case D3DMTOP_MODULATE4X:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATE4X) ? TRUE : FALSE);
	case D3DMTOP_ADD:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_ADD) ? TRUE : FALSE);
	case D3DMTOP_ADDSIGNED:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_ADDSIGNED) ? TRUE : FALSE);
	case D3DMTOP_ADDSIGNED2X:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_ADDSIGNED2X) ? TRUE : FALSE);
	case D3DMTOP_SUBTRACT:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_SUBTRACT) ? TRUE : FALSE);
	case D3DMTOP_ADDSMOOTH:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_ADDSMOOTH) ? TRUE : FALSE);
	case D3DMTOP_BLENDDIFFUSEALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_BLENDDIFFUSEALPHA) ? TRUE : FALSE);
	case D3DMTOP_BLENDTEXTUREALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_BLENDTEXTUREALPHA) ? TRUE : FALSE);
	case D3DMTOP_BLENDFACTORALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_BLENDFACTORALPHA) ? TRUE : FALSE);
	case D3DMTOP_BLENDTEXTUREALPHAPM:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_BLENDTEXTUREALPHAPM) ? TRUE : FALSE);
	case D3DMTOP_BLENDCURRENTALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_BLENDCURRENTALPHA) ? TRUE : FALSE);
	case D3DMTOP_PREMODULATE:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_PREMODULATE) ? TRUE : FALSE);
	case D3DMTOP_MODULATEALPHA_ADDCOLOR:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATEALPHA_ADDCOLOR) ? TRUE : FALSE);
	case D3DMTOP_MODULATECOLOR_ADDALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATECOLOR_ADDALPHA) ? TRUE : FALSE);
	case D3DMTOP_MODULATEINVALPHA_ADDCOLOR:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR) ? TRUE : FALSE);
	case D3DMTOP_MODULATEINVCOLOR_ADDALPHA:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA) ? TRUE : FALSE);
	case D3DMTOP_DOTPRODUCT3:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_DOTPRODUCT3) ? TRUE : FALSE);
	case D3DMTOP_MULTIPLYADD:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_MULTIPLYADD) ? TRUE : FALSE);
	case D3DMTOP_LERP:
		return ((Caps.TextureOpCaps & D3DMTEXOPCAPS_LERP) ? TRUE : FALSE);
	default:
		return FALSE;
	}
}


