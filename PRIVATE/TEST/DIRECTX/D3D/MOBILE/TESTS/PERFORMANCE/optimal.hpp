//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    Optimal.hpp

Abstract:

   Include file for D3DStrip

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++

#define D3D_OVERLOADS

#ifndef UNDER_CE
#include <tchar.h>
#endif
#include <windows.h>
//#include <ddraw.h>
//#include <d3d.h>
#include <d3dm.h>
//#include <d3dx8.h>
//#include <dx7todx8.h>
//#include <debugmemlib.h>
//#include "FloatMathLib.h"
#include "Tracker.hpp"
#include "qamath.h"
#ifdef TUXDLL
#include <kato.h>
#endif

#define MKFT(x) (*(float*)(&(x)))



// ++++ Enumerated Types ++++++++++++++++++++++++++++++++++++++++++++

// Output types
enum toutputlevel { OUTPUT_ALL = 0, OUTPUT_FAILURESONLY };

// ++++ Types +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// A structure for untransformed, unlit, textured vertex types
struct UNLIT_TEXTURED_VERTEX_TYPE
{
    FLOAT x, y, z;      // The untransformed position for the vertex
    FLOAT nx, ny, nz;   // Normal vector
    FLOAT tu, tv;       // Texture coordinates
};
// Our custom FVF, which describes a lit, texture vertex structure
#define UNLIT_TEXTURED_VERTEX_FVF (D3DMFVF_XYZ_FLOAT|D3DMFVF_NORMAL_FLOAT|D3DMFVF_TEX1)
typedef UNLIT_TEXTURED_VERTEX_TYPE D3DVERTEX, *LPD3DVERTEX;

// A structure for untransformed, lit, textured vertex types
struct LIT_TEXTURED_VERTEX_TYPE
{
    FLOAT x, y, z;              // The untransformed position for the vertex
    D3DMCOLOR color, specular;   // Vertex color
    FLOAT tu, tv;               // Texture coordinates
};
// Our custom FVF, which describes a lit, texture vertex structure
#define LIT_TEXTURED_VERTEX_FVF (D3DMFVF_XYZ_FLOAT|D3DMFVF_DIFFUSE|D3DMFVF_SPECULAR|D3DMFVF_TEX1)
typedef LIT_TEXTURED_VERTEX_TYPE D3DLVERTEX, *LPD3DLVERTEX;

// A structure for transformed, lit, textured vertex types
struct LIT_TRANSFORMED_TEXTURED_VERTEX_TYPE
{
    FLOAT x, y, z, rhw;
    D3DMCOLOR color, specular;
    FLOAT tu, tv;
};
// Our custom FVF, which describes a lit, transformed, textured vertex structure
#define LIT_TRANSFORMED_TEXTURED_VERTEX_FVF (D3DMFVF_XYZRHW_FLOAT|D3DMFVF_DIFFUSE|D3DMFVF_SPECULAR|D3DMFVF_TEX1|D3DMFVF_TEXCOORDSIZE2(0))
typedef LIT_TRANSFORMED_TEXTURED_VERTEX_TYPE D3DTVERTEX, *LPD3DTVERTEX;

// Error type
typedef int terr;

// ++++ Defines +++++++++++++++++++++++++++++++++++++++++++++++++++++

// Align vertex pools on cache-line boundaries for maximum throughput.
#define ALIGN_ALLOCS

// D3DUtil.cpp defines
#define PI			3.1415926f
#define MAX_LIGHTS	10

// ++++ Macros ++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define countof(x)  (sizeof(x) / sizeof(*(x)))

// ++++ Global Variables ++++++++++++++++++++++++++++++++++++++++++++

// Optimal.cpp variables
extern D3DMPOOL            g_memorypool;
extern HWND                g_hwndApp;                // HWND of the application
extern HINSTANCE           g_hinst;                  // HINSTANCE of the application
extern int                 g_nUserLights;        
extern BOOL                g_fRasterize;
extern BOOL                g_fRotating;
#ifdef TUXDLL
extern CKato              *g_pKato;
#endif


// Error.cpp variables
extern toutputlevel g_outputlevel;                      // Amount of detail in error messages
extern terr g_errLast;                                  // Error return code of last function

// D3DUtil.cpp variables
extern UINT                     g_uiAdapterOrdinal;
extern WCHAR                    g_pwszDriver[MAX_PATH + 1];
extern HMODULE                  g_hSWDeviceDLL;
extern LPDIRECT3DMOBILEDEVICE   g_p3ddevice;              // Main Direct3D Device
extern LPDIRECT3DMOBILE         g_pd3d;                   // Direct3D object
extern D3DMMATERIAL        g_pmatWhite;              // Materials
extern D3DMMATRIX          g_matIdent;               // Global identity matrix.
extern int				   g_nLights;

// Tracker.cxx variables
extern CTracker *g_ptrackerStrip;

// ++++ Global Functions ++++++++++++++++++++++++++++++++++++++++++++

// D3DUtil.cpp functions
BOOL InitDirect3D();
D3DMMATERIAL CreateMaterial(float rRed,   float rGreen, float rBlue, float rAlpha, float rPower, 
               float rSpecR, float rSpecG, float rSpecB, 
               float rEmisR, float rEmisG, float rEmisB);
void SetProjectionMatrix(D3DMMATRIX * pmatxDest, float rNear, float rFar,  float rFov, float rAspect);
void SetMatrixRotation(D3DMMATRIX * pmatxDest, D3DMVECTOR * pvectDir, D3DMVECTOR * pvectUp);
void SetViewMatrix(D3DMMATRIX * pmatxDest, D3DMVECTOR * pvectFrom, D3DMVECTOR * pvectAt, D3DMVECTOR * pvectUp);
void NormalizeVector(D3DMVECTOR * pvect);
void SetMatrixAxisRotation(D3DMMATRIX * pM, D3DMVECTOR * pvectAxis, float rAngle);
BOOL AddLight(float rRed, float rGreen, float rBlue, float rX, float rY, float rZ);

// TxtrUtil.cpp functions
LPDIRECT3DMOBILETEXTURE LoadTexture(TCHAR *tszFileName);

// Error.cpp functions
BOOL CheckError(TCHAR *tszErr);
void RetailOutput(TCHAR *tszErr, ...);

#ifdef TUXDLL
void DebugOutput(TCHAR *tszErr, ...);
#define Output(x, y) g_pKato->Log(x, y)

//
// Logging levels
//
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

#else // TUXDLL
#define Output(x, y) OutputDebugString(y)
#ifdef DEBUG
void DebugOutput(TCHAR *tszErr, ...);
#else
__inline void DebugOutput(TCHAR *tszErr, ...) {};
#endif
#endif // TUXDLL

// Network.cpp functions
bool InitNetwork (void);
bool ConnectNetwork (LPCSTR szServer, int nPortNum);
bool DisconnectNetwork ();
void CleanUpNetwork (void);
bool DoNetwork (void);
