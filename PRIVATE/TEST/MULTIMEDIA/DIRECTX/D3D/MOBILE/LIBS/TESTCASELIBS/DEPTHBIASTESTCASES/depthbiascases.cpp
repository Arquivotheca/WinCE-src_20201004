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
#include "DepthBiasCases.h"
#include "QAD3DMX.h"
#include <SurfaceTools.h>
#include <BufferTools.h>
#include "utils.h"
#include "DebugOutput.h"

namespace DepthBiasNamespace
{
    HRESULT CreateAndPrepareVertexBuffers(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex, 
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBufferBase,
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBufferTest,
        UINT                         *pVBStrideBase,
        UINT                         *pVBStrideTest)
    {
        HRESULT hr = S_OK;
        UINT uiShift;
    	FLOAT *pPosX, *pPosY;
    	UINT uiSX;
    	UINT uiSY;
    	//
    	// Window client extents
    	//
    	RECT ClientExtents;
    	//
    	// Retrieve device capabilities
    	//
    	if (0 == GetClientRect( hWnd,            // HWND hWnd, 
    	                       &ClientExtents)) // LPRECT lpRect 
    	{
    		hr = HRESULT_FROM_WIN32(GetLastError());
    		DebugOut(DO_ERROR, _T("GetClientRect failed. (hr = 0x%08x) "), hr);
    		goto cleanup;
    	}

        //
        // Reset the inputs
        //
        *ppVertexBufferBase = NULL;
        *ppVertexBufferTest = NULL;
    	
    	//
    	// Scale factors
    	//
    	uiSX = ClientExtents.right;
    	uiSY = ClientExtents.bottom;

    	if (Vertical == DepthBiasCases[dwTableIndex].eOrientation 
    	    && fabs(DepthBiasCases[dwTableIndex].rBiasValue) == 0.5)
    	{
    	    DepthBiasCases[dwTableIndex].rBiasValue *= (float)uiSY;
    	}
    	else if (Horizontal == DepthBiasCases[dwTableIndex].eOrientation 
    	    && fabs(DepthBiasCases[dwTableIndex].rBiasValue) == 0.5)
    	{
    	    DepthBiasCases[dwTableIndex].rBiasValue *= (float)uiSX;
    	}

        //////////////////////////////////////////////////////////////////////
        // Create the base primitives vertex buffer. These will be used as the
        // z-buffer references.
        //
        
    	//
    	// Scale to window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & DepthBiasCases[dwTableIndex].dwBaseFVF)
    	{
    		for (uiShift = 0; uiShift < DepthBiasCases[dwTableIndex].uiBaseNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(DepthBiasCases[dwTableIndex].pBaseVertexData) + uiShift * DepthBiasCases[dwTableIndex].uiBaseFVFSize);
    			pPosY = &(pPosX[1]);

    			(*pPosX) *= (float)(uiSX - 1);
    			(*pPosY) *= (float)(uiSY - 1);
    		}
    	}

    	if (FAILED(hr = CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
    	                                         ppVertexBufferBase,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
    	                                         DepthBiasCases[dwTableIndex].pBaseVertexData, // BYTE *pVertices,
    	                                         DepthBiasCases[dwTableIndex].uiBaseFVFSize,   // UINT uiVertexSize,
    	                                         DepthBiasCases[dwTableIndex].uiBaseNumVerts,  // UINT uiNumVertices,
    	                                         DepthBiasCases[dwTableIndex].dwBaseFVF)))     // DWORD dwFVF
    	{
    		DebugOut(DO_ERROR, _T("CreateActiveBuffer failed. (hr = 0x%08x) "), hr);
            hr = S_FALSE;
            goto cleanup;
    	}
    	
    	//
    	// "Unscale" from window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & DepthBiasCases[dwTableIndex].dwBaseFVF)
    	{
    		for (uiShift = 0; uiShift < DepthBiasCases[dwTableIndex].uiBaseNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(DepthBiasCases[dwTableIndex].pBaseVertexData) + uiShift * DepthBiasCases[dwTableIndex].uiBaseFVFSize);
    			pPosY = &(pPosX[1]);

    			(*pPosX) /= (float)(uiSX - 1);
    			(*pPosY) /= (float)(uiSY - 1);
    		}
    	}
    	
        //////////////////////////////////////////////////////////////////////
        // Create the test primitives vertex buffer. These will be used as the
        // biased primitives. As the bias changes, the visibility of these will
        // vary.
        //

    	//
    	// Scale to window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & DepthBiasCases[dwTableIndex].dwTestFVF)
    	{
    		for (uiShift = 0; uiShift < DepthBiasCases[dwTableIndex].uiTestNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(DepthBiasCases[dwTableIndex].pTestVertexData) + uiShift * DepthBiasCases[dwTableIndex].uiTestFVFSize);
    			pPosY = &(pPosX[1]);

    			(*pPosX) *= (float)(uiSX - 1);
    			(*pPosY) *= (float)(uiSY - 1);
    		}
    	}

    	if (FAILED(hr = CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
    	                                         ppVertexBufferTest,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
    	                                         DepthBiasCases[dwTableIndex].pTestVertexData, // BYTE *pVertices,
    	                                         DepthBiasCases[dwTableIndex].uiTestFVFSize,   // UINT uiVertexSize,
    	                                         DepthBiasCases[dwTableIndex].uiTestNumVerts,  // UINT uiNumVertices,
    	                                         DepthBiasCases[dwTableIndex].dwTestFVF)))     // DWORD dwFVF
    	{
    		DebugOut(DO_ERROR, _T("CreateActiveBuffer failed. (hr = 0x%08x) "), hr);
            hr = S_FALSE;
            goto cleanup;
    	}
    	
    	//
    	// "Unscale" from window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & DepthBiasCases[dwTableIndex].dwTestFVF)
    	{
    		for (uiShift = 0; uiShift < DepthBiasCases[dwTableIndex].uiTestNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(DepthBiasCases[dwTableIndex].pTestVertexData) + uiShift * DepthBiasCases[dwTableIndex].uiTestFVFSize);
    			pPosY = &(pPosX[1]);

    			(*pPosX) /= (float)(uiSX - 1);
    			(*pPosY) /= (float)(uiSY - 1);
    		}
    	}
    cleanup:
        return hr;
    }

    HRESULT SetupTextureStages(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
    	TEX_STAGE_SETTINGS FirstTexture  = { D3DMTOP_SELECTARG1, D3DMTOP_SELECTARG1,   D3DMTA_TEXTURE,   D3DMTA_CURRENT,   D3DMTA_TEXTURE,   D3DMTA_CURRENT};

    	if (
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP,   D3DMTOP_SELECTARG1 ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_DIFFUSE     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP,   D3DMTOP_SELECTARG1 ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_DIFFUSE     ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT     ))) /*||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ADDRESSU,  D3DMTADDRESS_CLAMP )))*/
    		)
    	{
    		DebugOut(DO_ERROR, _T("SetTextureStageState failed."));
    		return E_FAIL;
    	}

    	return S_OK;

    }

    HRESULT SetupBaseRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
        HRESULT hr;
        D3DMCAPS Caps;

        pDevice->GetDeviceCaps(&Caps);

    	//
    	// Set up the shade mode
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
    	{
    		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_SHADEMODE) failed. (hr = 0x%08x) "), hr);
        }

        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SLOPESCALEDEPTHBIAS, 0)))
        {
            DebugOut(DO_ERROR, _T("SetRenderState(Bias Mode) failed. (hr = 0x%08x) "), hr);
        }
        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_DEPTHBIAS, 0)))
        {
            DebugOut(DO_ERROR, _T("SetRenderState(Bias Mode) failed. (hr = 0x%08x) "), hr);
        }
        return hr;
    }

    HRESULT SetupTestRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
        HRESULT hr;
        D3DMCAPS Caps;

        pDevice->GetDeviceCaps(&Caps);

        if (D3DMRS_DEPTHBIAS == DepthBiasCases[dwTableIndex].RenderState && !(Caps.RasterCaps & D3DMPRASTERCAPS_DEPTHBIAS))
        {
            return S_FALSE;
        }
        if (D3DMRS_SLOPESCALEDEPTHBIAS == DepthBiasCases[dwTableIndex].RenderState && !(Caps.RasterCaps & D3DMPRASTERCAPS_SLOPESCALEDEPTHBIAS))
        {
            return S_FALSE;
        }

    	//
    	// Set up the shade mode
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
    	{
    		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_SHADEMODE) failed. (hr = 0x%08x) "), hr);
        }

        if (FAILED(hr = pDevice->SetRenderState(DepthBiasCases[dwTableIndex].RenderState, D3DM_MAKE_RSVALUE(DepthBiasCases[dwTableIndex].rBiasValue))))
        {
            DebugOut(DO_ERROR, _T("SetRenderState(Bias Mode) failed. (hr = 0x%08x) "), hr);
        }
        return hr;
    }

    UINT GetBasePrimitiveCount(DWORD dwTableIndex)
    {
        DWORD dwCurrentPrimType = D3DMQA_PRIMTYPE;
        if (dwCurrentPrimType == D3DMPT_TRIANGLESTRIP || dwCurrentPrimType == D3DMPT_TRIANGLEFAN)
        {
            return DepthBiasCases[dwTableIndex].uiBaseNumVerts - 2;
        }
        
        if (dwCurrentPrimType == D3DMPT_TRIANGLELIST)
        {
            return DepthBiasCases[dwTableIndex].uiBaseNumVerts / 3;
        }
        return 0;
    }
    UINT GetTestPrimitiveCount(DWORD dwTableIndex)
    {
        DWORD dwCurrentPrimType = D3DMQA_PRIMTYPE;
        if (dwCurrentPrimType == D3DMPT_TRIANGLESTRIP || dwCurrentPrimType == D3DMPT_TRIANGLEFAN)
        {
            return DepthBiasCases[dwTableIndex].uiTestNumVerts - 2;
        }
        
        if (dwCurrentPrimType == D3DMPT_TRIANGLELIST)
        {
            return DepthBiasCases[dwTableIndex].uiTestNumVerts / 3;
        }
        return 0;
    }
};
