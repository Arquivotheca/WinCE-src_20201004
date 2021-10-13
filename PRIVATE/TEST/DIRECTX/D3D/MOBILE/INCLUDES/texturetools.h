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
#include <d3dmtypes.h>
#include <d3dm.h>
#include "ColorConv.h"


//
// Valid arguments for SetTextureStageState with:
// 
//   * D3DMTSS_COLOROP
//   * D3DMTSS_ALPHAOP
// 
static const DWORD D3DMTOP_VALS[] = {
D3DMTOP_DISABLE,
D3DMTOP_SELECTARG1,
D3DMTOP_SELECTARG2,
D3DMTOP_MODULATE,
D3DMTOP_MODULATE2X,
D3DMTOP_MODULATE4X,
D3DMTOP_ADD,
D3DMTOP_ADDSIGNED,
D3DMTOP_ADDSIGNED2X,
D3DMTOP_SUBTRACT,
D3DMTOP_ADDSMOOTH,
D3DMTOP_BLENDDIFFUSEALPHA,
D3DMTOP_BLENDTEXTUREALPHA,
D3DMTOP_BLENDFACTORALPHA,
D3DMTOP_BLENDTEXTUREALPHAPM,
D3DMTOP_BLENDCURRENTALPHA,
D3DMTOP_PREMODULATE,
D3DMTOP_MODULATEALPHA_ADDCOLOR,
D3DMTOP_MODULATECOLOR_ADDALPHA,
D3DMTOP_MODULATEINVALPHA_ADDCOLOR,
D3DMTOP_MODULATEINVCOLOR_ADDALPHA,
D3DMTOP_DOTPRODUCT3,
D3DMTOP_MULTIPLYADD,
D3DMTOP_LERP
};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_COLORARG0
//   * D3DMTSS_COLORARG1
//   * D3DMTSS_COLORARG2
//   * D3DMTSS_ALPHAARG0
//   * D3DMTSS_ALPHAARG1
//   * D3DMTSS_ALPHAARG2
//   * D3DMTSS_RESULTARG
//
static const DWORD D3DMTA_VALS[] = {
D3DMTA_SELECTMASK,
D3DMTA_DIFFUSE,       
D3DMTA_CURRENT,       
D3DMTA_TEXTURE,       
D3DMTA_TFACTOR,      
D3DMTA_SPECULAR,
D3DMTA_TEMP,    
D3DMTA_OPTIONMASK,    
D3DMTA_COMPLEMENT,
D3DMTA_ALPHAREPLICATE
};


//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_TEXCOORDINDEX
// 
static const DWORD D3DMTEXCOORDINDEX_VALS[] = {
0,                                           //
0 | D3DMTSS_TCI_PASSTHRU,                    //
0 | D3DMTSS_TCI_CAMERASPACENORMAL,           // Tex Coord Index Zero 
0 | D3DMTSS_TCI_CAMERASPACEPOSITION,         //
0 | D3DMTSS_TCI_CAMERASPACEREFLECTIONVECTOR, //

1,                                           //
1 | D3DMTSS_TCI_PASSTHRU,                    //
1 | D3DMTSS_TCI_CAMERASPACENORMAL,           // Tex Coord Index One 
1 | D3DMTSS_TCI_CAMERASPACEPOSITION,         //
1 | D3DMTSS_TCI_CAMERASPACEREFLECTIONVECTOR, //

2,                                           //
2 | D3DMTSS_TCI_PASSTHRU,                    //
2 | D3DMTSS_TCI_CAMERASPACENORMAL,           // Tex Coord Index Two 
2 | D3DMTSS_TCI_CAMERASPACEPOSITION,         //
2 | D3DMTSS_TCI_CAMERASPACEREFLECTIONVECTOR, //

3,                                           //
3 | D3DMTSS_TCI_PASSTHRU,                    //
3 | D3DMTSS_TCI_CAMERASPACENORMAL,           // Tex Coord Index Three 
3 | D3DMTSS_TCI_CAMERASPACEPOSITION,         //
3 | D3DMTSS_TCI_CAMERASPACEREFLECTIONVECTOR  //

};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_ADDRESSU
//   * D3DMTSS_ADDRESSV
//   * D3DMTSS_ADDRESSW
// 
static const DWORD D3DMTEXADDR_VALS[] = {
D3DMTADDRESS_WRAP,
D3DMTADDRESS_MIRROR, 
D3DMTADDRESS_CLAMP,  
D3DMTADDRESS_BORDER
}; 

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_BORDERCOLOR
//
// Values are examples only; any other D3DMCOLOR is also
// a valid option
//
static const DWORD D3DMTEXBORDERCOLOR_VALS[] = {
D3DMCOLOR_XRGB(0xFF,0x00,0x00),
D3DMCOLOR_XRGB(0x00,0xFF,0x00),
D3DMCOLOR_XRGB(0x00,0x00,0xFF)
};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_MAGFILTER
//   * D3DMTSS_MINFILTER
//   * D3DMTSS_MIPFILTER
// 
static const DWORD D3DMTEXFILTER_VALS[] = {
D3DMTEXF_NONE,         
D3DMTEXF_POINT,        
D3DMTEXF_LINEAR,       
D3DMTEXF_ANISOTROPIC
};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_MIPMAPLODBIAS
//
// Values are examples only; many other floats are
// valid options
//
static const DWORD D3DMTEXLODBIAS_VALS[] = {
static_cast<DWORD>(-5.0f),
static_cast<DWORD>(-4.0f),
static_cast<DWORD>(-3.0f),
static_cast<DWORD>(-2.0f),
static_cast<DWORD>(-1.0f),
static_cast<DWORD>(-0.0f),
static_cast<DWORD>( 1.0f),
static_cast<DWORD>( 2.0f),
static_cast<DWORD>( 3.0f),
static_cast<DWORD>( 4.0f),
static_cast<DWORD>( 5.0f)
};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_MAXMIPLEVEL
//
static const DWORD D3DMTEXMAXMIPLEVEL_VALS[] = {
0, // all levels can be used
1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17, // Max level number
18,19,20,21,22,23,24,25,26,27,28,29,30,31  // for mip maps
};

//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_MAXANISOTROPY
// 
// Values are examples only, and not an exaustive list
// of possibilities.
//
static const DWORD D3DMTEXMAXANISOTROPY_VALS[] = {
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,
18,19,20,21,22,23,24,25,26,27,28,29,30,31
};


//
// Valid arguments for SetTextureStageState with 
// 
//   * D3DMTSS_TEXTURETRANSFORMFLAGS
// 
static const DWORD D3DMTEXTFF_VALS[] = {
D3DMTTFF_DISABLE,
D3DMTTFF_COUNT1,
D3DMTTFF_COUNT2,
D3DMTTFF_COUNT3,
D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,
D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,
};

//
// Structure for organizing data for SetTextureStageState args
//
typedef struct _SETTEXTURESTAGESTATEARGS {
    D3DMTEXTURESTAGESTATETYPE StateType;
    ULONG ulNumEntries;
    DWORD *pdwValues;
} SETTEXTURESTAGESTATEARGS;

//
// Table of arguments for SetTextureStageState testing
//
#define COUNTOF(x)  (sizeof(x)/sizeof(*(x)))
static const SETTEXTURESTAGESTATEARGS TexStageStateArgs[]= {
//|  D3DMTEXTURESTAGESTATETYPE   |        Number of Elements in Array          |         Array of DWORDs            |
//+------------------------------+---------------------------------------------+------------------------------------+
  { D3DMTSS_COLOROP              , COUNTOF(D3DMTOP_VALS)                       , (DWORD*)D3DMTOP_VALS               },
  { D3DMTSS_COLORARG1            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_COLORARG2            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_ALPHAOP              , COUNTOF(D3DMTOP_VALS)                       , (DWORD*)D3DMTOP_VALS               },
  { D3DMTSS_ALPHAARG1            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_ALPHAARG2            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_TEXCOORDINDEX        , COUNTOF(D3DMTEXCOORDINDEX_VALS)             , (DWORD*)D3DMTEXCOORDINDEX_VALS     },
  { D3DMTSS_ADDRESSU             , COUNTOF(D3DMTEXADDR_VALS)                   , (DWORD*)D3DMTEXADDR_VALS           },
  { D3DMTSS_ADDRESSV             , COUNTOF(D3DMTEXADDR_VALS)                   , (DWORD*)D3DMTEXADDR_VALS           },
  { D3DMTSS_BORDERCOLOR          , COUNTOF(D3DMTEXBORDERCOLOR_VALS)            , (DWORD*)D3DMTEXBORDERCOLOR_VALS    },
  { D3DMTSS_MAGFILTER            , COUNTOF(D3DMTEXFILTER_VALS)                 , (DWORD*)D3DMTEXFILTER_VALS         },
  { D3DMTSS_MINFILTER            , COUNTOF(D3DMTEXFILTER_VALS)                 , (DWORD*)D3DMTEXFILTER_VALS         },
  { D3DMTSS_MIPFILTER            , COUNTOF(D3DMTEXFILTER_VALS)                 , (DWORD*)D3DMTEXFILTER_VALS         },
  { D3DMTSS_MIPMAPLODBIAS        , COUNTOF(D3DMTEXLODBIAS_VALS)                , (DWORD*)D3DMTEXLODBIAS_VALS        },
  { D3DMTSS_MAXMIPLEVEL          , COUNTOF(D3DMTEXMAXMIPLEVEL_VALS)            , (DWORD*)D3DMTEXMAXMIPLEVEL_VALS    },
  { D3DMTSS_MAXANISOTROPY        , COUNTOF(D3DMTEXMAXANISOTROPY_VALS)          , (DWORD*)D3DMTEXMAXANISOTROPY_VALS  },
  { D3DMTSS_TEXTURETRANSFORMFLAGS, COUNTOF(D3DMTEXTFF_VALS)                    , (DWORD*)D3DMTEXTFF_VALS            },
  { D3DMTSS_ADDRESSW             , COUNTOF(D3DMTEXADDR_VALS)                   , (DWORD*)D3DMTEXADDR_VALS           },
  { D3DMTSS_COLORARG0            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_ALPHAARG0            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
  { D3DMTSS_RESULTARG            , COUNTOF(D3DMTA_VALS)                        , (DWORD*)D3DMTA_VALS                },
};
#define D3DQA_NUM_SETTEXTURESTAGESTATEARGS COUNTOF(TexStageStateArgs)

//
// According to the device capabilities, are the supplied extents valid?
//
BOOL CapsAllowCreateTexture(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiWidth, UINT uiHeight);

//
// Utilities for filling textures and surfaces
//
HRESULT ReadFileToMemory(CONST TCHAR *ptszFilename, BYTE **ppByte);
HRESULT ReadFileToMemoryTimed(CONST TCHAR *ptszFilename, BYTE **ppByte);
HRESULT GetTexture(BYTE *pBMP, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice);
HRESULT GetTextureFromFile(CONST TCHAR *pszFile, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice);
HRESULT GetTextureFromResource(CONST TCHAR *pszName, HMODULE hMod, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice);
HRESULT GetTextureFromMem(BYTE *pBMP, PIXEL_UNPACK *pDstFormat, D3DMFORMAT d3dFormat, D3DMPOOL d3dPool, IDirect3DMobileTexture** ppTexture, IDirect3DMobileDevice* pd3dDevice);
HRESULT FillSurface(IDirect3DMobileSurface* pSurface, BYTE *pBMP);
HRESULT FillSurface(IDirect3DMobileSurface* pSurface, CONST TCHAR *pszFile);

HRESULT GetMipLevelExtents(UINT uiTopWidth, UINT uiTopHeight,
                           UINT uiLevel, UINT *puiLevelWidth, UINT *puiLevelHeight);

HRESULT GetSolidTexture(LPDIRECT3DMOBILEDEVICE pDevice, D3DMCOLOR Color, LPDIRECT3DMOBILETEXTURE *ppTexture);

HRESULT GetBitmapResourcePointer(INT iResourceID, PBYTE *ppByte);
