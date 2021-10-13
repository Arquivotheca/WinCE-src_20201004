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

#ifndef _BASE_VIDEO_RENDERER_TESTDESC_PARSER_H
#define _BASE_VIDEO_RENDERER_TESTDESC_PARSER_H

#include "dxxml.h"
#include "TestDesc.h"
#include "MMTestParser.h"
#include "Operation.h"
#include "BaseVRTestDesc.h"

const char* const STR_OpList = "OpList";
const char* const STR_Op = "Op";
const char* const STR_WaitOp = "Wait";
const char* const STR_SwitchGDIOp = "SwitchGDI";
const char* const STR_SwitchDDrawOp = "SwitchDDraw";
const char* const STR_SrcRect = "SrcRect";
const char* const STR_SrcRectList = "SrcRectList";
const char* const STR_DstRect = "DstRect";
const char* const STR_DstRectList = "DstRectList";
const char* const STR_WndPosList = "WindowPositionList";
const char* const STR_WndScaleList = "WindowScaleList";
const char* const STR_WndRectList = "WindowRectList";
const char* const STR_UseVideoRenderer = "UseVideoRenderer";

const char STR_OP_DELIMITER = ':';
const char WNDPOS_LIST_DELIMITER = ',';
const char WNDPOS_DELIMITER = ':';
const char WNDRECT_LIST_DELIMITER = ',';
const char WNDRECT_DELIMITER = ':';
const char WNDSCALE_LIST_DELIMITER = ',';
const char WNDSCALE_DELIMITER = ':';
const char RENDERER_SURFACE_MODE_STR_DELIMITER = ':';

const char* const STR_MixingMode = "MixingMode";
const char* const STR_RenderingMode = "RenderingMode";
const char* const STR_NumOfStreams = "NumOfStreams";
const char* const STR_AlphaBitmap = "AlphaBitmap";

const char* const STR_BitmapSource = "BitmapSource";
const char* const STR_HDCBitmap = "HDC";
const char* const STR_DDrawBitmap = "DDraw";
const char* const STR_ColorRef = "ColorRef";
const char* const STR_Alpha = "Alpha";

const char* const STR_StreamInfo = "Stream";
const char* const STR_DRM = "DRM";
const char* const STR_StreamID = "ID";
const char* const STR_ZOrder = "ZOrder";
const char* const STR_OutputRect = "OutputRect";
const char* const STR_BkgColor = "BkgColor";
const char* const STR_MixingPrefs = "MixingPrefs";
const char* const STR_RenderTargetRGB = "RenderTargetRGB";
const char* const STR_RenderTargetYUV = "RenderTargetYUV";

const char* const STR_PassRate = "PassRate";
const char* const STR_SrcImgPath = "SrcImgPath";
const char* const STR_DestImgPath = "DestImgPath";
const char* const STR_TestMode = "TestMode";
const char* const STR_ImgSaveMode = "ImgSaveMode";
const char* const STR_ImgVerificationMode = "ImgVerificationMode";
const char* const STR_ImgCaptureMode = "ImgCaptureMode";
const char* const STR_RenderingPrefsSurface = "RenderingPrefsSurface";
const char* const STR_CheckUsingOverlay = "CheckUsingOverlay";

extern HRESULT 
ParseWaitOp( CXMLElement *hElement, Operation** pOp );
extern HRESULT 
ParseSwitchGDIOp( CXMLElement *hElement, Operation** pOp);
extern HRESULT 
ParseSwitchDDrawOp( CXMLElement *hElement, Operation** pOp );
extern HRESULT 
ParseOperation( CXMLElement *hElement, Operation** pOp);
extern HRESULT 
ParseOpList( CXMLElement *hElement, OpList* pOpList);
extern HRESULT 
ParseWndScale(const char* szWndScale, WndScale* pWndScale);
extern HRESULT 
ParseWndPos(const char* szWndPos, WndPos* pWndPos);
extern HRESULT 
ParseWndScaleList(CXMLElement *hElement, WndScaleList* pWndScaleList);
extern HRESULT 
ParseWndPosList(CXMLElement *hElement, WndPosList* pWndPosList);
extern HRESULT 
ParseWndRect(const char* szWndRect, WndRect* pWndRect);
extern HRESULT 
ParseWndRectList(CXMLElement *hElement, WndRectList* pWndRectList);
extern HRESULT
ParseRendererSurfaceType( const char* szRendererSurfaceType, 
						  VideoRendererMode *pVRMode, 
						  DWORD* pSurfaceType );

extern HRESULT 
ParseStreamInfo( CXMLElement *hElement, STREAMINFO* pStreamInfo );

extern HRESULT 
ParseTestMode( CXMLElement *hDeviceOrient, VMR_TEST_MODE* pTestMode );

extern HRESULT 
ParseBitmapSource( const char* szBitmapSource, DWORD &dwFlags );

extern HRESULT 
ParseMixingPrefs( const char* szMixingPrefs, DWORD &dwMixerPrefs );

extern HRESULT 
ParseAlphaBitmap( CXMLElement *hElement, VMRALPHABITMAP* pBitmap );

extern HRESULT 
ParseRenderPrefSurface( CXMLElement *hElement, DWORD *dwRenderPrefSurface);


#endif
