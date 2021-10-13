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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "TestDescParser.h"
#include "BaseVRTestDescParser.h"

HRESULT ParseWaitOp( CXMLElement *hElement, Operation** pOp )
{
	HRESULT hr = S_OK;
	if ( !hElement || !pOp ) return E_FAIL;

	WaitOp *pWaitOp = new WaitOp();
	if (!pWaitOp)
		return E_OUTOFMEMORY;

	// Parse the wait time
	DWORD time = 0;
	hr = ParseDWORD(hElement, &time);
	if (SUCCEEDED(hr))
	{		
		pWaitOp->time = time;
		if (pOp)
			*pOp = pWaitOp;
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse WaitOp time."), __FUNCTION__, __LINE__, __FILE__);	
	}

	if (FAILED(hr) || !pOp)
		delete pWaitOp;
	
	return hr;
}


HRESULT ParseSwitchGDIOp( CXMLElement *hElement, Operation** pOp)
{
	HRESULT hr = S_OK;
	if ( !hElement || !pOp ) return E_FAIL;

	SwitchGDIOp *pSwitchGDIOp = new SwitchGDIOp();
	if (!pSwitchGDIOp)
		return E_OUTOFMEMORY;

	if (pOp)
		*pOp = pSwitchGDIOp;

	if (FAILED(hr) || !pOp)
		delete pSwitchGDIOp;
	
	return hr;
}

HRESULT ParseSwitchDDrawOp( CXMLElement *hElement, Operation** pOp )
{
	HRESULT hr = S_OK;
	if ( !hElement || !pOp ) return E_FAIL;

	SwitchDDrawOp *pSwitchDDrawOp = new SwitchDDrawOp();
	if (!pSwitchDDrawOp)
		return E_OUTOFMEMORY;

	if (pOp)
		*pOp = pSwitchDDrawOp;

	if (FAILED(hr) || !pOp)
		delete pSwitchDDrawOp;
	
	return hr;
}

HRESULT ParseOperation( CXMLElement *hElement, Operation** pOp)
{
	HRESULT hr = S_OK;
	CXMLElement *hXmlChild = 0;

	if ( !hElement || !pOp ) return E_FAIL;

	hXmlChild = hElement->GetFirstChildElement();
	if ( !hXmlChild )
		return S_OK;

	char szOpName[MAX_OP_NAME_LEN];
	const char* szOp = hElement->GetValue();
	
	// Parse the op name
	int pos = 0;
	ParseStringListString(szOp, STR_OP_DELIMITER, szOpName, countof(szOpName), pos);

	if (!strcmp(STR_WaitOp, szOpName))
	{
		hr = ParseWaitOp(hXmlChild, pOp);
	}
	else if (!strcmp(STR_SwitchGDIOp, szOpName))
	{
		hr = ParseSwitchGDIOp(hXmlChild, pOp);
	}
	else if (!strcmp(STR_SwitchDDrawOp, szOpName))
	{
		hr = ParseSwitchDDrawOp(hXmlChild, pOp);
	}
	else {
		hr = E_FAIL;
	}

	if (FAILED(hr))
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to parse %s: %s."), __FUNCTION__, __LINE__, __FILE__, 
						hXmlChild->GetName(), hXmlChild->GetValue() );	
	}

	return hr;
}

HRESULT ParseOpList( CXMLElement *hElement, OpList* pOpList)
{
	HRESULT hr = S_OK;
	CXMLElement *hXmlChild = 0;

	if ( !hElement || !pOpList ) return E_FAIL;

	hXmlChild = hElement->GetFirstChildElement();
	if (!hXmlChild)
		return S_OK;

	while (true)
	{
		if (!strcmp( STR_Op, hXmlChild->GetName() ) )
		{
			Operation* pOp = NULL;
			hr = ParseOperation(hXmlChild, &pOp);
			pOpList->push_back(pOp);
			if (FAILED(hr))
				break;
		}
		else {
			hr = E_FAIL;
			break;
		}

		hXmlChild = hXmlChild->GetNextSiblingElement();
		if (hXmlChild == 0)
			return S_OK;
	}
	if (FAILED(hr))
		LOG(TEXT("%S: ERROR %d@%S - failed to parse oplist."), __FUNCTION__, __LINE__, __FILE__);	

	return hr;
}



HRESULT ParseWndScale(const char* szWndScale, WndScale* pWndScale)
{
	HRESULT hr = S_OK;
	int listmarker = 0;

	char szX[32];
	char szY[32];

	// Read the X coord
	hr = ParseStringListString(szWndScale, WNDSCALE_DELIMITER, szX, countof(szX), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read X coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndScale->x = atol(szX);
	}

	// Read the Y coord
	hr = ParseStringListString(szWndScale, WNDSCALE_DELIMITER, szY, countof(szY), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read Y coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndScale->y = atol(szY);
	}

	return hr;
}

HRESULT ParseWndPos(const char* szWndPos, WndPos* pWndPos)
{
	HRESULT hr = S_OK;
	int listmarker = 0;

	char szX[32];
	char szY[32];

	// Read the X coord
	hr = ParseStringListString(szWndPos, WNDPOS_DELIMITER, szX, countof(szX), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read X coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndPos->x = atol(szX);
	}

	// Read the Y coord
	hr = ParseStringListString(szWndPos, WNDPOS_DELIMITER, szY, countof(szY), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read Y coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndPos->y = atol(szY);
	}

	return hr;
}

HRESULT ParseWndScaleList(CXMLElement *hElement, WndScaleList* pWndScaleList)
{
	HRESULT hr = S_OK;
	CXMLElement * hXmlChild = 0;

	if ( !hElement || !pWndScaleList ) return E_FAIL;

	const char* szWndScaleList = hElement->GetValue();
	if (!szWndScaleList)
		return S_OK;
	char szWndScale[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window scaleition
		hr = ParseStringListString( szWndScaleList, 
								    WNDSCALE_LIST_DELIMITER, 
									szWndScale, 
									countof(szWndScale), 
									listmarker );
		if (FAILED(hr))
			break;

		WndScale wndScale;
		hr = ParseWndScale( szWndScale, &wndScale );
		if ( FAILED(hr) )
		{
			LOG( TEXT("%S: ERROR %d@%S - failed to parse window scaleition %S."), 
						__FUNCTION__, __LINE__, __FILE__, szWndScale );
		}
		pWndScaleList->push_back( wndScale );
	}
	
	return hr;
}

HRESULT ParseWndPosList(CXMLElement *hElement, WndPosList* pWndPosList)
{
	HRESULT hr = S_OK;
	CXMLElement *hXmlChild = 0;

	if ( !hElement || !pWndPosList ) return E_FAIL;

	const char* szWndPosList = hElement->GetValue();
	if (!szWndPosList)
		return S_OK;
	char szWndPos[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window position
		hr = ParseStringListString( szWndPosList, 
									WNDPOS_LIST_DELIMITER, 
									szWndPos, 
									countof(szWndPos), 
									listmarker );
		if (FAILED(hr))
			break;

		WndPos wndPos;
		hr = ParseWndPos(szWndPos, &wndPos);
		if (FAILED(hr))
		{
			LOG( TEXT("%S: ERROR %d@%S - failed to parse window position %S."), 
						__FUNCTION__, __LINE__, __FILE__, szWndPos );
		}
		pWndPosList->push_back(wndPos);
	}
	
	return hr;
}

HRESULT ParseWndRect(const char* szWndRect, WndRect* pWndRect)
{
	HRESULT hr = S_OK;
	int listmarker = 0;

	char szTop[32];
	char szLeft[32];
	char szBottom[32];
	char szRight[32];

	// Read the left coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szLeft, countof(szLeft), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read left coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->left = atol(szLeft);
	}

	// Read the top coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szTop, countof(szTop), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read top coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->top = atol(szTop);
	}

	// Read the right coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szRight, countof(szRight), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read right coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->right = atol(szRight);
	}


	// Read the bottom coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szBottom, countof(szBottom), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read bottom coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->bottom = atol(szBottom);
	}

	return hr;
}

HRESULT ParseWndRectList(CXMLElement *hElement, WndRectList* pWndRectList)
{
	HRESULT hr = S_OK;
	CXMLElement *hXmlChild = 0;
	if ( !hElement || !pWndRectList ) return E_FAIL;

	const char* szWndRectList = hElement->GetValue();
	if (!szWndRectList)
		return S_OK;
	char szWndRect[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window position
		hr = ParseStringListString( szWndRectList, 
									WNDRECT_LIST_DELIMITER, 
									szWndRect, 
									countof(szWndRect), 
									listmarker );
		if (FAILED(hr))
			break;

		WndRect wndRect;
		hr = ParseWndRect(szWndRect, &wndRect);
		if (FAILED(hr))
		{
			LOG( TEXT("%S: ERROR %d@%S - failed to parse window position %S."), 
						__FUNCTION__, __LINE__, __FILE__, szWndRect );
		}
		pWndRectList->push_back(wndRect);
	}
	
	return hr;
}




HRESULT 
ParseRendererSurfaceType( const char* szRendererSurfaceType, 
						  VideoRendererMode *pVRMode, 
						  DWORD* pSurfaceType )
{
	HRESULT hr = S_OK;

	int pos = 0;
	char szRendererMode[64] = "";
	char szSurfaceType[512] = "";
	hr = ParseStringListString( szRendererSurfaceType, 
								RENDERER_SURFACE_MODE_STR_DELIMITER, 
								szRendererMode, 
								countof(szRendererMode), 
								pos );
	if (FAILED(hr))
		return hr;
	
	if (pos != -1)
	{
		hr = ParseStringListString( szRendererSurfaceType, 
									RENDERER_SURFACE_MODE_STR_DELIMITER, 
									szSurfaceType, 
									countof(szSurfaceType), 
									pos );
		if (FAILED(hr))
			return hr;
	}

	if (strcmp(szRendererMode, ""))
		hr = ParseRendererMode(szRendererMode, pVRMode);

	if (FAILED(hr))
		return hr;

	if (strcmp(szSurfaceType, ""))
		hr = ParseSurfaceType(szSurfaceType, pSurfaceType);

	return hr;
}

HRESULT 
ParseStreamInfo( CXMLElement *hElement, STREAMINFO* pStreamInfo )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hXmlChild )
        return E_FAIL;

    //Empty the downloadlocation string and media name first
    _tcscpy_s(pStreamInfo->szDownloadLocation, MAX_PATH, TEXT(""));
    _tcscpy_s(pStreamInfo->szMedia, MAX_PATH, TEXT(""));

    // For each verification type specified, parse the type and value
    while( hXmlChild && SUCCEEDED(hr) )
    {
        PCSTR pName = hXmlChild->GetName();
        if ( !strcmp(STR_StreamID, pName ) )
        {
            hr = ParseDWORD( hXmlChild, &pStreamInfo->dwStreamID );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the stream ID."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        if ( !strcmp(STR_Media, pName) )
        {
            hr = ParseString( hXmlChild, pStreamInfo->szMedia, MAX_PATH );
            if (FAILED(hr))
                LOG(TEXT("%S: ERROR %d@%S - failed to parse the media for the stream."), 
                            __FUNCTION__, __LINE__, __FILE__);
        }

        if ( !strcmp(STR_DownloadLocation, pName) )
        {
            hr = ParseString( hXmlChild, 
                              pStreamInfo->szDownloadLocation, 
                              countof(pStreamInfo->szDownloadLocation) );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the download location for the stream."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        
        if ( !strcmp( STR_DRM, pName ) )
        {
            pStreamInfo->m_bDRM = TRUE;
        }

        if ( !strcmp(STR_ZOrder, pName) )
        {
            hr = ParseINT32( hXmlChild, &pStreamInfo->iZOrder );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the Z order."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        if ( !strcmp(STR_Alpha, pName) )
        {
            hr = ParseFLOAT( hXmlChild, &pStreamInfo->fAlpha );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the alpha value of the stream."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        if ( !strcmp(STR_OutputRect, pName) )
        {
            RECT tmpRect;
            hr = ParseRect( hXmlChild, &tmpRect );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the output rect."), 
                            __FUNCTION__, __LINE__, __FILE__ );
            pStreamInfo->nrOutput.left = (FLOAT)tmpRect.left / 100.0f;
            pStreamInfo->nrOutput.top = (FLOAT)tmpRect.top / 100.0f;
            pStreamInfo->nrOutput.bottom = (FLOAT)tmpRect.bottom / 100.0f;
            pStreamInfo->nrOutput.right = (FLOAT)tmpRect.right / 100.0f;
        }

        if ( !strcmp(STR_BkgColor, pName) )
        {
            hr = ParseDWORD( hXmlChild, &pStreamInfo->clrBkg );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the background color."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }

        if ( !strcmp(STR_MixingPrefs, pName) )
        {
            hr = ParseMixingPrefs( hXmlChild->GetValue(), pStreamInfo->dwMixerPrefs );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the mixing prefs."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
        else  // If we failed to parse the verification, skip the remaining verifications
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the stream info."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }
    }

    return hr;
}

HRESULT 
ParseTestMode( CXMLElement *hDeviceOrient, VMR_TEST_MODE* pTestMode )
{
    HRESULT hr = S_OK;

    if (!pTestMode)
        return E_POINTER;

    const char* szMode = NULL;
    szMode = hDeviceOrient->GetValue();
    if ( !szMode )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse Test Mode."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }else {
        if (!strcmp(szMode, STR_ImgSaveMode))
            *pTestMode = IMG_SAVE_MODE;
        else if (!strcmp(szMode, STR_ImgVerificationMode))
            *pTestMode = IMG_VERIFICATION_MODE;
        else {
            hr = E_FAIL;
        }
    }
    return hr;
}

HRESULT 
ParseBitmapSource( const char* szBitmapSource, DWORD &dwFlags )
{
    HRESULT hr = S_OK;
    dwFlags = 0;
    
    if ( !strcmp(szBitmapSource, STR_HDCBitmap) )
        dwFlags |= VMRBITMAP_HDC;
    else if ( !strcmp(szBitmapSource, STR_DDrawBitmap) )
        dwFlags &= ~VMRBITMAP_HDC;
    else {
        return E_FAIL;
    }

    return hr;
}

HRESULT 
ParseMixingPrefs( const char* szMixingPrefs, DWORD &dwMixerPrefs )
{
    HRESULT hr = S_OK;
    dwMixerPrefs = 0;
    
    if ( !strcmp(szMixingPrefs, STR_RenderTargetRGB) )
        dwMixerPrefs |= MixerPref_RenderTargetRGB;
    else if ( !strcmp(szMixingPrefs, STR_RenderTargetYUV) )
        dwMixerPrefs |= MixerPref_RenderTargetYUV;
    else {
        return E_FAIL;
    }

    return hr;
}

HRESULT 
ParseAlphaBitmap( CXMLElement *hElement, VMRALPHABITMAP* pBitmap )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hXmlChild )
        return E_FAIL;

    // For each verification type specified, parse the type and value
    while( hXmlChild && SUCCEEDED(hr) )
    {
        PCSTR pName = hXmlChild->GetName();
        if ( !strcmp(STR_BitmapSource, pName ) )
        {
            hr = ParseBitmapSource( hXmlChild->GetValue(), pBitmap->dwFlags );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the bitmap source."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
    
        if ( !strcmp(STR_ColorRef, pName) )
        {
            hr = ParseDWORD( hXmlChild, &pBitmap->clrSrcKey );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse source color key."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }

        if ( !strcmp(STR_Alpha, pName) )
        {
            hr = ParseFLOAT( hXmlChild, &pBitmap->fAlpha );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the alpha value of the bitmap."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }

        if ( !strcmp(STR_SrcRect, pName) )
        {
            hr = ParseRect( hXmlChild, &pBitmap->rSrc );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the source rect."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }

        if ( !strcmp(STR_DstRect, pName) )
        {
            RECT tmpRect;
            hr = ParseRect( hXmlChild, &tmpRect );
            if (FAILED(hr))
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the dest rect."), 
                            __FUNCTION__, __LINE__, __FILE__ );
            pBitmap->rDest.left = (FLOAT)tmpRect.left / 100.0f;
            pBitmap->rDest.top = (FLOAT)tmpRect.top / 100.0f;
            pBitmap->rDest.bottom = (FLOAT)tmpRect.bottom / 100.0f;
            pBitmap->rDest.right = (FLOAT)tmpRect.right / 100.0f;
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
        else  // If we failed to parse the verification, skip the remaining verifications
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the alpha bitmap."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }
    }

    return hr;
}

HRESULT 
ParseRenderPrefSurface( CXMLElement *hElement, DWORD *dwRenderPrefSurface)
{
    HRESULT hr = S_OK;

    if ( !hElement || !dwRenderPrefSurface ) 
        return E_POINTER;

    const char* szFlag = hElement->GetValue();
    if ( !szFlag )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse render pref caps."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }else {
        if (!strcmp(szFlag, "RenderPrefs_ForceOffscreen"))
            *dwRenderPrefSurface= RenderPrefs_ForceOffscreen;
        else if (!strcmp(szFlag, "RenderPrefs_AllowOverlays"))
            *dwRenderPrefSurface = RenderPrefs_AllowOverlays;
        else if (!strcmp(szFlag, "RenderPrefs_ForceOverlays"))
            *dwRenderPrefSurface = RenderPrefs_ForceOverlays;
        else {
            hr = E_FAIL;
        }
    }
    return hr;
}
