//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include "TestCases.h"

#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)

HRESULT AcquireDestSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex);
HRESULT AcquireSourceSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex);
HRESULT SetupSourceSurface(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex);
HRESULT GetStretchRects(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwIteration,
    DWORD dwTableIndex,
    LPRECT pRectSource,
    LPRECT pRectDest);
HRESULT GetFilterType(
    LPDIRECT3DMOBILEDEVICE pDevice,
    D3DMTEXTUREFILTERTYPE * d3dtft,
    DWORD dwTableIndex);

HRESULT GetColorsSquare(
    D3DMFORMAT Format,
    int iX,
    int iY,
    int Width,
    int Height,
    float * prRed,
    float * prGreen,
    float * prBlue,
    float * prAlpha);

enum D3DQASurfaceCreator {
    D3DQA_BACKBUFFER,
    D3DQA_CREATE_TEXTURE,
    D3DQA_CREATE_IMAGE_SURFACE,
    D3DQA_CREATE_RENDER_TARGET,
    D3DQA_CREATE_DEPTH_STENCIL_SURFACE,
};

enum D3DQAStretchType {
    D3DQA_STRETCH,
    D3DQA_SHRINK,
    D3DQA_COPY,
};

typedef struct _STRETCH_RECT_TESTS {

    //
    // Source surface information
    //
    D3DQASurfaceCreator CreatorSource;
    D3DMPOOL ResourcePoolSource;

    //
    // Destination surface information
    //
    D3DQASurfaceCreator CreatorDest;
    D3DMPOOL ResourcePoolDest;

    //
    // Information about the stretching we will be doing
    //
    D3DQAStretchType StretchType;
    DWORD Iterations;
    D3DMTEXTUREFILTERTYPE FilterType;

    PFNGETCOLORS pfnGetColors;
} STRETCH_RECT_TESTS;

#define FLOAT_DONTCARE 1.0f


__declspec(selectany) STRETCH_RECT_TESTS StretchRectCases [D3DMQA_STRETCHRECTTEST_COUNT] = {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Filtering:    D3DMTEXF_POINT
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//  Source:      Image Surface 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Image Surface 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      BackBuffer 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter         | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_POINT , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_POINT , NULL             ,

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Filtering:    D3DMTEXF_LINEAR
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//  Source:      Image Surface 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Image Surface 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      BackBuffer 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_LINEAR , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_LINEAR , NULL             ,

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Filtering:    D3DMTEXF_NONE
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//  Source:      Image Surface 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Image Surface 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Image Surface 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      BackBuffer 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter        | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SHRINK  , 5                  , D3DMTEXF_NONE , NULL             ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_COPY    , 1                  , D3DMTEXF_NONE , NULL             ,


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Filtering:    D3DMTEXF_LINEAR, with square source picture.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//  Source:      Image Surface 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                  , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Image Surface 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                  , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Image Surface 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Image Surface 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      BackBuffer 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: BackBuffer
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Image Surface
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (SYSMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (VIDMEM)
//  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Stretch Type  | Stretch Iterations | Filter          | Color Function   |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_STRETCH , 10                 , D3DMTEXF_LINEAR , GetColorsSquare  ,

};


