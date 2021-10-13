//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "DepthStencilTools.h"
#include <tchar.h>


//
// NextDepthStencilFormat
//
//   Intended to be called iteratively to cycle through all valid depth/stencil formats.
//   
// Arguments:
//
//   LPDIRECT3DMOBILE pD3D:  Direct3D Object
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMFORMAT RenderTargetFormat:  For match verification
//   D3DMFORMAT *pFormat:  Resultant D/S format
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT NextDepthStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT RenderTargetFormat, D3DMFORMAT *pFormat)
{
	static UINT uiFirstFormatToTryDS = 0;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
	}

	//
	// Retrieve current display mode
	//
    if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		return E_FAIL;
	}

	struct CHECKDEPTHSTENCILMATCH_ARGS {
		UINT Adapter;
		D3DMDEVTYPE DeviceType;
		D3DMFORMAT AdapterFormat;
		D3DMFORMAT RenderTargetFormat;
		D3DMFORMAT DepthStencilFormat;
	} FormatArgs = {Caps.AdapterOrdinal, Caps.DeviceType, Mode.Format, RenderTargetFormat, D3DMFMT_UNKNOWN };
	

	switch(uiFirstFormatToTryDS)
	{
	case 0: // D3DMFMT_D32    
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D32)))
		{
			*pFormat = D3DMFMT_D32;
			return S_OK;
		}
	case 1: // D3DMFMT_D15S1  
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D15S1)))
		{
			*pFormat = D3DMFMT_D15S1;
			return S_OK;
		}
	case 2: // D3DMFMT_D24S8  
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D24S8)))
		{
			*pFormat = D3DMFMT_D24S8;
			return S_OK;
		}
	case 3: // D3DMFMT_D16    
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D16)))
		{
			*pFormat = D3DMFMT_D16;
			return S_OK;
		}
	case 4: // D3DMFMT_D24X8  
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D24X8)))
		{
			*pFormat = D3DMFMT_D24X8;
			return S_OK;
		}
	case 5: // D3DMFMT_D24X4S4
		uiFirstFormatToTryDS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D24X4S4)))
		{
			*pFormat = D3DMFMT_D24X4S4;
			return S_OK;
		}
	default:
		uiFirstFormatToTryDS = 0;
		return E_FAIL;
	}

}

//
// NextStencilFormat
//
//   Intended to be called iteratively to cycle through all valid stencil formats.
//   
// Arguments:
//
//   LPDIRECT3DMOBILE pD3D:  Direct3D Object
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMFORMAT RenderTargetFormat:  For match verification
//   D3DMFORMAT *pFormat:  Resultant stencil format
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT NextStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT RenderTargetFormat, D3DMFORMAT *pFormat)
{
	static UINT uiFirstFormatToTryS = 0;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
	}

	//
	// Retrieve current display mode
	//
    if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		return E_FAIL;
	}

	struct CHECKDEPTHSTENCILMATCH_ARGS {
		UINT Adapter;
		D3DMDEVTYPE DeviceType;
		D3DMFORMAT AdapterFormat;
		D3DMFORMAT RenderTargetFormat;
		D3DMFORMAT DepthStencilFormat;
	} FormatArgs = {Caps.AdapterOrdinal, Caps.DeviceType, Mode.Format, RenderTargetFormat, D3DMFMT_UNKNOWN };

	switch(uiFirstFormatToTryS)
	{
	case 0: // D3DMFMT_D15S1  
		uiFirstFormatToTryS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D15S1)))
		{
			*pFormat = D3DMFMT_D15S1;
			return S_OK;
		}
	case 1: // D3DMFMT_D24S8  
		uiFirstFormatToTryS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D24S8)))
		{
			*pFormat = D3DMFMT_D24S8;
			return S_OK;
		}
	case 2: // D3DMFMT_D24X4S4
		uiFirstFormatToTryS++;
		if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat,
			D3DMFMT_D24X4S4)))
		{
			*pFormat = D3DMFMT_D24X4S4;
			return S_OK;
		}
	default:
		uiFirstFormatToTryS = 0;
		return E_FAIL;
	}

}

//
// StencilBits
//
//   Indicates the number of stenciling bits in a specific D/S format.
//   
// Arguments:
//
//   D3DMFORMAT FmtDepthStencil:  D/S format
//    
// Return Value:
//    
//   UINT:  Number of stencil bits
//
UINT StencilBits(D3DMFORMAT FmtDepthStencil)
{
	switch(FmtDepthStencil)
	{
	case D3DMFMT_D15S1:
		return 1;
	case D3DMFMT_D24S8:
		return 8;
	case D3DMFMT_D24X4S4:
		return 4;
	default:
		return 0;
	}
}




//
// IsValidDepthStencilFormat
//
//   Verifies that a device supports a particular D/S format, and that the device supports
//   the combination of that D/S format with the current RT format.
//   
// Arguments:
//
//
//   LPDIRECT3DMOBILE pD3D:  Direct3D Object
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMFORMAT DepthStencilFormat:  D/S format
//    
// Return Value:
//    
//   BOOL:  TRUE if supported; FALSE otherwise
//
BOOL IsValidDepthStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT DepthStencilFormat)

{
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return FALSE;
	}

	//
	// Retrieve current display mode
	//
    if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		return FALSE;
	}
	
	struct CHECKDEPTHSTENCILMATCH_ARGS {
		UINT Adapter;
		D3DMDEVTYPE DeviceType;
		D3DMFORMAT AdapterFormat;
		D3DMFORMAT RenderTargetFormat;
		D3DMFORMAT DepthStencilFormat;
	} FormatArgs = {Caps.AdapterOrdinal, Caps.DeviceType, Mode.Format, Mode.Format, DepthStencilFormat };


	if (SUCCEEDED(pD3D->CheckDepthStencilMatch(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.RenderTargetFormat, FormatArgs.DepthStencilFormat)))
	{
		return TRUE;
	}

	return FALSE;
}



//
// NextRenderTargetFormat
//
//   Intended to be called iteratively to cycle through all valid render target formats.
//   
// Arguments:
//
//   LPDIRECT3DMOBILE pD3D:  Direct3D Object
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMFORMAT *pFormat:  Resultant render target format
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT NextRenderTargetFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT *pFormat)
{
	static UINT uiFirstFormatToTryRT = 0;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Display mode, used to determine a format to use
	//
	D3DMDISPLAYMODE Mode;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
	}

	//
	// Retrieve current display mode
	//
    if (FAILED(pDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		return E_FAIL;
	}

	struct CHECKDEVICEFORMAT_ARGS {
		UINT Adapter;
		D3DMDEVTYPE DeviceType;
		D3DMFORMAT AdapterFormat;
		DWORD Usage;
		D3DMRESOURCETYPE RType;
		D3DMFORMAT CheckFormat;
	} FormatArgs = {Caps.AdapterOrdinal, Caps.DeviceType, Mode.Format, D3DMUSAGE_RENDERTARGET, D3DMRTYPE_SURFACE, D3DMFMT_UNKNOWN };
	

	switch(uiFirstFormatToTryRT)
	{
	case 0: // D3DMFMT_R8G8B8         
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_R8G8B8)))
		{
			*pFormat = D3DMFMT_R8G8B8;
			return S_OK;
		}
	case 1: // D3DMFMT_A8R8G8B8       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A8R8G8B8)))
		{
			*pFormat = D3DMFMT_A8R8G8B8;
			return S_OK;
		}
	case 2: // D3DMFMT_X8R8G8B8       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_X8R8G8B8)))
		{
			*pFormat = D3DMFMT_X8R8G8B8;
			return S_OK;
		}
	case 3: // D3DMFMT_R5G6B5         
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_R5G6B5)))
		{
			*pFormat = D3DMFMT_R5G6B5;
			return S_OK;
		}
	case 4: // D3DMFMT_X1R5G5B5       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_X1R5G5B5)))
		{
			*pFormat = D3DMFMT_X1R5G5B5;
			return S_OK;
		}
	case 5: // D3DMFMT_A1R5G5B5       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A1R5G5B5)))
		{
			*pFormat = D3DMFMT_A1R5G5B5;
			return S_OK;
		}
	case 6: // D3DMFMT_A4R4G4B4       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A4R4G4B4)))
		{
			*pFormat = D3DMFMT_A4R4G4B4;
			return S_OK;
		}
	case 7: // D3DMFMT_R3G3B2         
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_R3G3B2)))
		{
			*pFormat = D3DMFMT_R3G3B2;
			return S_OK;
		}
	case 8: // D3DMFMT_A8R3G3B2       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A8R3G3B2)))
		{
			*pFormat = D3DMFMT_A8R3G3B2;
			return S_OK;
		}
	case 9: // D3DMFMT_X4R4G4B4       
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_X4R4G4B4)))
		{
			*pFormat = D3DMFMT_X4R4G4B4;
			return S_OK;
		}
	case 10: // D3DMFMT_A8P8           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A8P8)))
		{
			*pFormat = D3DMFMT_A8P8;
			return S_OK;
		}
	case 11: // D3DMFMT_P8             
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_P8)))
		{
			*pFormat = D3DMFMT_P8;
			return S_OK;
		}
	case 12: // D3DMFMT_A8             
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_A8)))
		{
			*pFormat = D3DMFMT_A8;
			return S_OK;
		}
	case 13: // D3DMFMT_UYVY           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_UYVY)))
		{
			*pFormat = D3DMFMT_UYVY;
			return S_OK;
		}
	case 14: // D3DMFMT_YUY2           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_YUY2)))
		{
			*pFormat = D3DMFMT_YUY2;
			return S_OK;
		}
	case 15: // D3DMFMT_DXT1           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_DXT1)))
		{
			*pFormat = D3DMFMT_DXT1;
			return S_OK;
		}
	case 16: // D3DMFMT_DXT2           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_DXT2)))
		{
			*pFormat = D3DMFMT_DXT2;
			return S_OK;
		}
	case 17: // D3DMFMT_DXT3           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_DXT3)))
		{
			*pFormat = D3DMFMT_DXT3;
			return S_OK;
		}
	case 18: // D3DMFMT_DXT4           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_DXT4)))
		{
			*pFormat = D3DMFMT_DXT4;
			return S_OK;
		}
	case 19: // D3DMFMT_DXT5           
		uiFirstFormatToTryRT++;
		if (SUCCEEDED(pD3D->CheckDeviceFormat(FormatArgs.Adapter,FormatArgs.DeviceType,FormatArgs.AdapterFormat,FormatArgs.Usage,FormatArgs.RType,
			D3DMFMT_DXT5)))
		{
			*pFormat = D3DMFMT_DXT5;
			return S_OK;
		}
	default:
		uiFirstFormatToTryRT = 0;
		return E_FAIL;
	}

}



//
// SetStencilStates
//
//   Set all stencil-related render states
//   
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice: Underlying device
//   DWORD StencilEnable:  Value for D3DMRS_STENCILENABLE
//   DWORD StencilMask:  Value for D3DMRS_STENCILMASK
//   DWORD StencilWriteMask:  Value for D3DMRS_STENCILWRITEMASK
//   D3DMSTENCILOP StencilZFail:  Value for D3DMRS_STENCILZFAIL
//   D3DMSTENCILOP StencilFail:  Value for D3DMRS_STENCILFAIL
//   D3DMSTENCILOP StencilPass:  Value for D3DMRS_STENCILPASS
//   D3DMCMPFUNC StencilFunc:  Value for D3DMRS_STENCILFUNC
//   DWORD StencilRef:  Value for D3DMRS_STENCILREF
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT SetStencilStates(LPDIRECT3DMOBILEDEVICE pDevice, 
                         DWORD StencilEnable,
                         DWORD StencilMask,
                         DWORD StencilWriteMask,
                         D3DMSTENCILOP StencilZFail,
                         D3DMSTENCILOP StencilFail,
                         D3DMSTENCILOP StencilPass,
                         D3DMCMPFUNC StencilFunc,
                         DWORD StencilRef)
{

	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILENABLE,   StencilEnable)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILENABLE) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILMASK,     StencilMask)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILMASK) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILWRITEMASK,StencilWriteMask)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILWRITEMASK) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILZFAIL,    StencilZFail)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILZFAIL) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILFAIL,     StencilFail)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILFAIL) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILPASS,     StencilPass)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILPASS) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILFUNC,     StencilFunc)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILFUNC) failed."));
		return E_FAIL;
	}
	if (FAILED(pDevice->SetRenderState( D3DMRS_STENCILREF,      StencilRef)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_STENCILREF) failed."));
		return E_FAIL;
	}

	return S_OK;
}
