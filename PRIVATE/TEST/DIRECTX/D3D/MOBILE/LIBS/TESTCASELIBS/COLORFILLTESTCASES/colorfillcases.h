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

HRESULT AcquireFillSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex);
HRESULT GetColorFillRects(
        LPDIRECT3DMOBILESURFACE pSurface,
        DWORD dwTableIndex,
        UINT uiIterations,
        LPRECT * ppRect,
        D3DMCOLOR * pColor);

enum D3DQASurfaceCreator {
    D3DQA_BACKBUFFER,
    D3DQA_CREATE_TEXTURE,
    D3DQA_CREATE_IMAGE_SURFACE,
    D3DQA_CREATE_RENDER_TARGET,
};

enum D3DQAColorFillType {
    D3DQA_CFFULLSURFACE,
    D3DQA_CFBIGRECTS,
    D3DQA_CFSMALLRECTS,
};

typedef struct _COLOR_FILL_TESTS {

    //
    // Source surface information
    //
    D3DQASurfaceCreator Creator;
    D3DMPOOL ResourcePool;

    //
    // Information about the stretching we will be doing
    //
    D3DQAColorFillType ColorFillType;
} COLOR_FILL_TESTS;

#define FLOAT_DONTCARE 1.0f


__declspec(selectany) COLOR_FILL_TESTS ColorFillCases [D3DMQA_COLORFILLTEST_COUNT] = {          
//  Surface Type: Image 
//  ----------------------------------------------------------------------
//  Surface Creator            | Resource Pool      | Copy Type           |
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM , D3DQA_CFFULLSURFACE ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM , D3DQA_CFBIGRECTS    ,
    D3DQA_CREATE_IMAGE_SURFACE , D3DMPOOL_SYSTEMMEM , D3DQA_CFSMALLRECTS  ,

//  Surface Type: BackBuffer 
//  ----------------------------------------------------------------------
//  Surface Creator            | Resource Pool      | Copy Type           |
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM , D3DQA_CFFULLSURFACE ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM , D3DQA_CFBIGRECTS    ,
    D3DQA_BACKBUFFER           , D3DMPOOL_VIDEOMEM , D3DQA_CFSMALLRECTS  ,

//  Surface Type: Texture (System Memory) 
//  ----------------------------------------------------------------------
//  Surface Creator            | Resource Pool      | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM , D3DQA_CFFULLSURFACE ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM , D3DQA_CFBIGRECTS    ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_SYSTEMMEM , D3DQA_CFSMALLRECTS  ,

//  Surface Type: Texture (Video Memory) 
//  ----------------------------------------------------------------------
//  Surface Creator            | Resource Pool      | Copy Type           |
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM  , D3DQA_CFFULLSURFACE ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM  , D3DQA_CFBIGRECTS    ,
    D3DQA_CREATE_TEXTURE       , D3DMPOOL_VIDEOMEM  , D3DQA_CFSMALLRECTS  ,

};


