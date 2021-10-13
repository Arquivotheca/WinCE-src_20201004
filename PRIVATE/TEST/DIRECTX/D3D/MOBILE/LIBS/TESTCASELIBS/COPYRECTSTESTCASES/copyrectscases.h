//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include <windows.h>
#include <d3dm.h>
#include "TestCases.h"

#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)

HRESULT AcquireCRDestSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex);
HRESULT AcquireCRSourceSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex);
HRESULT SetupCRSourceSurface(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex);
HRESULT GetCopyRects(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwTableIndex,
    LPRECT rgRectSource,
    LPPOINT rgPointDest,
    UINT * pRectCount,
    UINT * pPointCount);

enum D3DQASurfaceCreator {
    D3DQA_BACKBUFFER,
    D3DQA_CREATE_TEXTURE,
    D3DQA_CREATE_IMAGE_SURFACE,
    D3DQA_CREATE_RENDER_TARGET,
};

enum D3DQACopyType {
    D3DQA_FULLSURFACE,
    D3DQA_SOURCENODEST,
    D3DQA_SOURCEANDDEST,
};

typedef struct _COPY_RECTS_TESTS {

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
    D3DQACopyType CopyType;
} COPY_RECTS_TESTS;

#define FLOAT_DONTCARE 1.0f


__declspec(selectany) COPY_RECTS_TESTS CopyRectsCases [D3DMQA_COPYRECTSTEST_COUNT] = {          
//  Source:      Image Surface 
//  Destination: BackBuffer
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      Image Surface 
//  Destination: Image Surface
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Image Surface 
//  Destination: Texture Surface (SYSMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Image Surface 
//  Destination: Texture Surface (VIDMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      BackBuffer 
//  Destination: Image Surface
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (SYSMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      BackBuffer 
//  Destination: Texture Surface (VIDMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: BackBuffer
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Image Surface
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (SYSMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (SYSMEM) 
//  Destination: Texture Surface (VIDMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM   , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: BackBuffer
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_BACKBUFFER            , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Image Surface
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_IMAGE_SURFACE  , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (SYSMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_SYSTEMMEM   , D3DQA_SOURCEANDDEST ,

//  Source:      Texture Surface (VIDMEM) 
//  Destination: Texture Surface (VIDMEM)
//  -----------------------------------------------------------------------------------------------------------------------------
//  Source Surface Creator     | Source Resource Pool | Destination Surface Creator | Dest Resource Pool   | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_FULLSURFACE   ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCENODEST  ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM    , D3DQA_CREATE_TEXTURE        , D3DMPOOL_VIDEOMEM    , D3DQA_SOURCEANDDEST ,


};


