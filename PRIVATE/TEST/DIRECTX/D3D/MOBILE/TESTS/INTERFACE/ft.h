//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <tux.h>
#include "TestIDs.h"

TESTPROCAPI TuxTest(UINT uTuxMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//
// Tux Function Table Entry for all IDirect3DMobile interface tests
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

//|                            Description                              | Tree  | TestProc |                 Unique ID for               |          |
//|                            of Test Case                             | Depth |   Arg    |                   test case                 | TestProc |
//+---------------------------------------------------------------------+-------+----------+---------------------------------------------+----------+
{ TEXT("Direct3D Mobile Interface Tests                              "),       0,         0, 0                                           , NULL     },

{ TEXT("IDirect3DMobile::QueryInterface                              "),       1,         0, D3DQAID_OBJ_QUERYINTERFACE                  , TuxTest  },
{ TEXT("IDirect3DMobile::AddRef                                      "),       1,         0, D3DQAID_OBJ_ADDREF                          , TuxTest  },
{ TEXT("IDirect3DMobile::Release                                     "),       1,         0, D3DQAID_OBJ_RELEASE                         , TuxTest  },
{ TEXT("IDirect3DMobile::RegisterSoftwareDevice                      "),       1,         0, D3DQAID_OBJ_REGISTERSOFTWAREDEVICE          , TuxTest  },
{ TEXT("IDirect3DMobile::GetAdapterCount                             "),       1,         0, D3DQAID_OBJ_GETADAPTERCOUNT                 , TuxTest  },
{ TEXT("IDirect3DMobile::GetAdapterIdentifier                        "),       1,         0, D3DQAID_OBJ_GETADAPTERIDENTIFIER            , TuxTest  },
{ TEXT("IDirect3DMobile::GetAdapterModeCount                         "),       1,         0, D3DQAID_OBJ_GETADAPTERMODECOUNT             , TuxTest  },
{ TEXT("IDirect3DMobile::EnumAdapterModes                            "),       1,         0, D3DQAID_OBJ_ENUMADAPTERMODES                , TuxTest  },
{ TEXT("IDirect3DMobile::GetAdapterDisplayMode                       "),       1,         0, D3DQAID_OBJ_GETADAPTERDISPLAYMODE           , TuxTest  },
{ TEXT("IDirect3DMobile::CheckDeviceType                             "),       1,         0, D3DQAID_OBJ_CHECKDEVICETYPE                 , TuxTest  },
{ TEXT("IDirect3DMobile::CheckDeviceFormat                           "),       1,         0, D3DQAID_OBJ_CHECKDEVICEFORMAT               , TuxTest  },
{ TEXT("IDirect3DMobile::CheckDeviceMultiSampleType                  "),       1,         0, D3DQAID_OBJ_CHECKDEVICEMULTISAMPLETYPE      , TuxTest  },
{ TEXT("IDirect3DMobile::CheckDepthStencilMatch                      "),       1,         0, D3DQAID_OBJ_CHECKDEPTHSTENCILMATCH          , TuxTest  },
{ TEXT("IDirect3DMobile::CheckProfile                                "),       1,         0, D3DQAID_OBJ_CHECKPROFILE                    , TuxTest  },
{ TEXT("IDirect3DMobile::GetDeviceCaps                               "),       1,         0, D3DQAID_OBJ_GETDEVICECAPS                   , TuxTest  },
{ TEXT("IDirect3DMobile::CreateDevice                                "),       1,         0, D3DQAID_OBJ_CREATEDEVICE                    , TuxTest  },
{ TEXT("IDirect3DMobile::CheckDeviceFormatConversion                 "),       1,         0, D3DQAID_OBJ_CHECKDEVICEFORMATCONVERSION     , TuxTest  },

{ TEXT("IDirect3DMobileDevice::QueryInterface Test                   "),       1,         0, D3DQAID_DEV_QUERYINTERFACE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::TestCooperativeLevelTest Test         "),       1,         0, D3DQAID_DEV_TESTCOOPERATIVELEVEL            , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetAvailablePoolMem Test              "),       1,         0, D3DQAID_DEV_GETAVAILABLEPOOLMEM             , TuxTest  },
{ TEXT("IDirect3DMobileDevice::ResourceManagerDiscardBytes Test      "),       1,         0, D3DQAID_DEV_RESOURCEMANAGERDISCARDBYTES     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetDirect3D Test                      "),       1,         0, D3DQAID_DEV_GETDIRECT3D                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetDeviceCaps Test                    "),       1,         0, D3DQAID_DEV_GETDEVICECAPS                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetDisplayMode Test                   "),       1,         0, D3DQAID_DEV_GETDISPLAYMODE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetCreationParameters Test            "),       1,         0, D3DQAID_DEV_GETCREATIONPARAMETERS           , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateAdditionalSwapChain Test        "),       1,         0, D3DQAID_DEV_CREATEADDITIONALSWAPCHAIN       , TuxTest  },
{ TEXT("IDirect3DMobileDevice::Reset Test                            "),       1,         0, D3DQAID_DEV_RESET                           , TuxTest  },
{ TEXT("IDirect3DMobileDevice::Present Test                          "),       1,         0, D3DQAID_DEV_PRESENT                         , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetBackBuffer Test                    "),       1,         0, D3DQAID_DEV_GETBACKBUFFER	                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateTexture Test                    "),       1,         0, D3DQAID_DEV_CREATETEXTURE                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateVertexBuffer Test               "),       1,         0, D3DQAID_DEV_CREATEVERTEXBUFFER              , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateIndexBuffer Test                "),       1,         0, D3DQAID_DEV_CREATEINDEXBUFFER               , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateRenderTarget Test               "),       1,         0, D3DQAID_DEV_CREATERENDERTARGET              , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateDepthStencilSurface Test        "),       1,         0, D3DQAID_DEV_CREATEDEPTHSTENCILSURFACE       , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CreateImageSurface Test               "),       1,         0, D3DQAID_DEV_CREATEIMAGESURFACE              , TuxTest  },
{ TEXT("IDirect3DMobileDevice::CopyRects Test                        "),       1,         0, D3DQAID_DEV_COPYRECTS                       , TuxTest  },
{ TEXT("IDirect3DMobileDevice::UpdateTexture Test                    "),       1,         0, D3DQAID_DEV_UPDATETEXTURE                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetFrontBuffer Test                   "),       1,         0, D3DQAID_DEV_GETFRONTBUFFER                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetRenderTarget Test                  "),       1,         0, D3DQAID_DEV_SETRENDERTARGET                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetRenderTarget Test                  "),       1,         0, D3DQAID_DEV_GETRENDERTARGET                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetDepthStencilSurface Test           "),       1,         0, D3DQAID_DEV_GETDEPTHSTENCILSURFACE          , TuxTest  },
{ TEXT("IDirect3DMobileDevice::BeginScene Test                       "),       1,         0, D3DQAID_DEV_BEGINSCENE                      , TuxTest  },
{ TEXT("IDirect3DMobileDevice::EndScene Test                         "),       1,         0, D3DQAID_DEV_ENDSCENE                        , TuxTest  },
{ TEXT("IDirect3DMobileDevice::Clear Test                            "),       1,         0, D3DQAID_DEV_CLEAR                           , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetTransform Test                     "),       1,         0, D3DQAID_DEV_SETTRANSFORM                    , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetTransform Test                     "),       1,         0, D3DQAID_DEV_GETTRANSFORM                    , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetViewport Test                      "),       1,         0, D3DQAID_DEV_SETVIEWPORT                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetViewport Test                      "),       1,         0, D3DQAID_DEV_GETVIEWPORT                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetMaterial Test                      "),       1,         0, D3DQAID_DEV_SETMATERIAL                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetMaterial Test                      "),       1,         0, D3DQAID_DEV_GETMATERIAL                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetLight Test                         "),       1,         0, D3DQAID_DEV_SETLIGHT                        , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetLight Test                         "),       1,         0, D3DQAID_DEV_GETLIGHT                        , TuxTest  },
{ TEXT("IDirect3DMobileDevice::LightEnable Test                      "),       1,         0, D3DQAID_DEV_LIGHTENABLE                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetLightEnable Test                   "),       1,         0, D3DQAID_DEV_GETLIGHTENABLE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetRenderState Test                   "),       1,         0, D3DQAID_DEV_SETRENDERSTATE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetRenderState Test                   "),       1,         0, D3DQAID_DEV_GETRENDERSTATE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetClipStatus Test                    "),       1,         0, D3DQAID_DEV_SETCLIPSTATUS                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetClipStatus Test                    "),       1,         0, D3DQAID_DEV_GETCLIPSTATUS                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetTexture Test                       "),       1,         0, D3DQAID_DEV_GETTEXTURE                      , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetTexture Test                       "),       1,         0, D3DQAID_DEV_SETTEXTURE                      , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetTextureStageState Test             "),       1,         0, D3DQAID_DEV_GETTEXTURESTAGESTATE            , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetTextureStageState Test             "),       1,         0, D3DQAID_DEV_SETTEXTURESTAGESTATE            , TuxTest  },
{ TEXT("IDirect3DMobileDevice::ValidateDevice Test                   "),       1,         0, D3DQAID_DEV_VALIDATEDEVICE                  , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetInfo Test                          "),       1,         0, D3DQAID_DEV_GETINFO                         , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetPaletteEntries Test                "),       1,         0, D3DQAID_DEV_SETPALETTEENTRIES               , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetPaletteEntries Test                "),       1,         0, D3DQAID_DEV_GETPALETTEENTRIES               , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetCurrentTexturePalette Test         "),       1,         0, D3DQAID_DEV_SETCURRENTTEXTUREPALETTE        , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetCurrentTexturePalette Test         "),       1,         0, D3DQAID_DEV_GETCURRENTTEXTUREPALETTE        , TuxTest  },
{ TEXT("IDirect3DMobileDevice::DrawPrimitive Test                    "),       1,         0, D3DQAID_DEV_DRAWPRIMITIVE                   , TuxTest  },
{ TEXT("IDirect3DMobileDevice::DrawIndexedPrimitive Test             "),       1,         0, D3DQAID_DEV_DRAWINDEXEDPRIMITIVE            , TuxTest  },
{ TEXT("IDirect3DMobileDevice::ProcessVertices Test                  "),       1,         0, D3DQAID_DEV_PROCESSVERTICES                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetStreamSource Test                  "),       1,         0, D3DQAID_DEV_SETSTREAMSOURCE                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetStreamSource Test                  "),       1,         0, D3DQAID_DEV_GETSTREAMSOURCE                 , TuxTest  },
{ TEXT("IDirect3DMobileDevice::SetIndices Test                       "),       1,         0, D3DQAID_DEV_SETINDICES                      , TuxTest  },
{ TEXT("IDirect3DMobileDevice::GetIndices Test                       "),       1,         0, D3DQAID_DEV_GETINDICES                      , TuxTest  },
{ TEXT("IDirect3DMobileDevice::StretchRect Test                      "),       1,         0, D3DQAID_DEV_STRETCHRECT                     , TuxTest  },
{ TEXT("IDirect3DMobileDevice::ColorFill Test                        "),       1,         0, D3DQAID_DEV_COLORFILL                       , TuxTest  },
// Misc Tests:     
{ TEXT("IDirect3DMobileDevice Auto Mip-Map Extents Test              "),       1,         0, D3DQAID_DEV_VERIFYAUTOMIPLEVELEXTENTS       , TuxTest  },
{ TEXT("IDirect3DMobileDevice Presentation Interval Test             "),       1,         0, D3DQAID_DEV_PRESENTATIONINTERVAL            , TuxTest  },
{ TEXT("IDirect3DMobileDevice MultiSample/Swap Effect Mismatch Test  "),       1,         0, D3DQAID_DEV_SWAPEFFECTMULTISAMPLEMISMATCH   , TuxTest  },
{ TEXT("IDirect3DMobileDevice Present Flag/Multisample Mismatch Test "),       1,         0, D3DQAID_DEV_PRESENTFLAGMULTISAMPLEMISMATCH  , TuxTest  },
{ TEXT("IDirect3DMobileDevice Back-Buffer Constraints Test           "),       1,         0, D3DQAID_DEV_VERIFYBACKBUFLIMITSWAPEFFECTCOPY, TuxTest  },
{ TEXT("IDirect3DMobileDevice Swap Chain Manip During Reset Test     "),       1,         0, D3DQAID_DEV_SWAPCHAINMANIPDURINGRESET       , TuxTest  },
{ TEXT("IDirect3DMobileDevice Command Buffer Ref Counting Test       "),       1,         0, D3DQAID_DEV_COMMANDBUFREFCOUNTING           , TuxTest  },
{ TEXT("IDirect3DMobile object creation test                         "),       1,         0, D3DQAID_DEV_OBJECTCREATE                    , TuxTest  },

{ TEXT("IDirect3DMobileIndexBuffer::QueryInterface                   "),       1,         0, D3DQAID_IB_QUERYINTERFACE                   , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::AddRef                           "),       1,         0, D3DQAID_IB_ADDREF                           , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::Release                          "),       1,         0, D3DQAID_IB_RELEASE                          , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::GetDevice                        "),       1,         0, D3DQAID_IB_GETDEVICE                        , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::SetPriority                      "),       1,         0, D3DQAID_IB_SETPRIORITY                      , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::GetPriority                      "),       1,         0, D3DQAID_IB_GETPRIORITY                      , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::PreLoad                          "),       1,         0, D3DQAID_IB_PRELOAD                          , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::GetType                          "),       1,         0, D3DQAID_IB_GETTYPE                          , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::Lock                             "),       1,         0, D3DQAID_IB_LOCK                             , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::Unlock                           "),       1,         0, D3DQAID_IB_UNLOCK                           , TuxTest  },
{ TEXT("IDirect3DMobileIndexBuffer::GetDesc                          "),       1,         0, D3DQAID_IB_GETDESC                          , TuxTest  },

{ TEXT("IDirect3DMobileSurface::QueryInterface                       "),       1,         0, D3DQAID_SRF_QUERYINTERFACE                  , TuxTest  },
{ TEXT("IDirect3DMobileSurface::AddRef                               "),       1,         0, D3DQAID_SRF_ADDREF                          , TuxTest  },
{ TEXT("IDirect3DMobileSurface::Release                              "),       1,         0, D3DQAID_SRF_RELEASE                         , TuxTest  },
{ TEXT("IDirect3DMobileSurface::GetDevice                            "),       1,         0, D3DQAID_SRF_GETDEVICE                       , TuxTest  },
{ TEXT("IDirect3DMobileSurface::GetContainer                         "),       1,         0, D3DQAID_SRF_GETCONTAINER                    , TuxTest  },
{ TEXT("IDirect3DMobileSurface::GetDesc                              "),       1,         0, D3DQAID_SRF_GETDESC                         , TuxTest  },
{ TEXT("IDirect3DMobileSurface::LockRect                             "),       1,         0, D3DQAID_SRF_LOCKRECT                        , TuxTest  },
{ TEXT("IDirect3DMobileSurface::UnlockRect                           "),       1,         0, D3DQAID_SRF_UNLOCKRECT                      , TuxTest  },
{ TEXT("IDirect3DMobileSurface::GetDC                                "),       1,         0, D3DQAID_SRF_GETDC                           , TuxTest  },
{ TEXT("IDirect3DMobileSurface::ReleaseDC                            "),       1,         0, D3DQAID_SRF_RELEASEDC                       , TuxTest  },

{ TEXT("IDirect3DMobileTexture::QueryInterface                       "),       1,         0, D3DQAID_TEX_QUERYINTERFACE                  , TuxTest  },
{ TEXT("IDirect3DMobileTexture::AddRef                               "),       1,         0, D3DQAID_TEX_ADDREF                          , TuxTest  },
{ TEXT("IDirect3DMobileTexture::Release                              "),       1,         0, D3DQAID_TEX_RELEASE                         , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetDevice                            "),       1,         0, D3DQAID_TEX_GETDEVICE                       , TuxTest  },
{ TEXT("IDirect3DMobileTexture::SetPriority                          "),       1,         0, D3DQAID_TEX_SETPRIORITY                     , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetPriority                          "),       1,         0, D3DQAID_TEX_GETPRIORITY                     , TuxTest  },
{ TEXT("IDirect3DMobileTexture::PreLoad                              "),       1,         0, D3DQAID_TEX_PRELOAD                         , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetType                              "),       1,         0, D3DQAID_TEX_GETTYPE                         , TuxTest  },
{ TEXT("IDirect3DMobileTexture::SetLOD                               "),       1,         0, D3DQAID_TEX_SETLOD                          , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetLOD                               "),       1,         0, D3DQAID_TEX_GETLOD                          , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetLevelCount                        "),       1,         0, D3DQAID_TEX_GETLEVELCOUNT                   , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetLevelDesc                         "),       1,         0, D3DQAID_TEX_GETLEVELDESC                    , TuxTest  },
{ TEXT("IDirect3DMobileTexture::GetSurfaceLevel                      "),       1,         0, D3DQAID_TEX_GETSURFACELEVEL                 , TuxTest  },
{ TEXT("IDirect3DMobileTexture::LockRect                             "),       1,         0, D3DQAID_TEX_LOCKRECT                        , TuxTest  },
{ TEXT("IDirect3DMobileTexture::UnlockRect                           "),       1,         0, D3DQAID_TEX_UNLOCKRECT                      , TuxTest  },
{ TEXT("IDirect3DMobileTexture::AddDirtyRect                         "),       1,         0, D3DQAID_TEX_ADDDIRTYRECT                    , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface QueryInterface          "),       1,         0, D3DQAID_TEX_SRFQUERYINTERFACE               , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface AddRef                  "),       1,         0, D3DQAID_TEX_SRFADDREF                       , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface Release                 "),       1,         0, D3DQAID_TEX_SRFRELEASE                      , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface GetDevice               "),       1,         0, D3DQAID_TEX_SRFGETDEVICE                    , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface GetContainer            "),       1,         0, D3DQAID_TEX_SRFGETCONTAINER                 , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface GetDesc                 "),       1,         0, D3DQAID_TEX_SRFGETDESC                      , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface LockRect                "),       1,         0, D3DQAID_TEX_SRFLOCKRECT                     , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface UnlockRect              "),       1,         0, D3DQAID_TEX_SRFUNLOCKRECT                   , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface GetDC                   "),       1,         0, D3DQAID_TEX_SRFGETDC                        , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface ReleaseDC               "),       1,         0, D3DQAID_TEX_SRFRELEASEDC                    , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface StretchRect             "),       1,         0, D3DQAID_TEX_SRFSTRETCHRECT                  , TuxTest  },
{ TEXT("IDirect3DMobileTexture Level Surface ColorFill               "),       1,         0, D3DQAID_TEX_SRFCOLORFILL                    , TuxTest  },

{ TEXT("IDirect3DMobileVertexBuffer::QueryInterface                  "),       1,         0, D3DQAID_VB_QUERYINTERFACE                   , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::AddRef                          "),       1,         0, D3DQAID_VB_ADDREF                           , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::Release                         "),       1,         0, D3DQAID_VB_RELEASE                          , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::GetDevice                       "),       1,         0, D3DQAID_VB_GETDEVICE                        , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::SetPriority                     "),       1,         0, D3DQAID_VB_SETPRIORITY                      , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::GetPriority                     "),       1,         0, D3DQAID_VB_GETPRIORITY                      , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::PreLoad                         "),       1,         0, D3DQAID_VB_PRELOAD                          , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::GetType                         "),       1,         0, D3DQAID_VB_GETTYPE                          , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::Lock                            "),       1,         0, D3DQAID_VB_LOCK                             , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::Unlock                          "),       1,         0, D3DQAID_VB_UNLOCK                           , TuxTest  },
{ TEXT("IDirect3DMobileVertexBuffer::GetDesc                         "),       1,         0, D3DQAID_VB_GETDESC                          , TuxTest  },
// PSL Test (last because it might cause problems.
{ TEXT("IDirect3DMobile PSL Randomizer test                          "),       1,         0, D3DQAID_DEV_PSLRANDOMIZER                   , TuxTest  },

{ NULL,                                                                        0,         0, 0                                           , NULL     }
};

//  Processes messages from the TUX shell.
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

// Shell info structure (contains instance handle, window handle, DLL handle, command line args, etc.)
SPS_SHELL_INFO   *g_pShellInfo;

