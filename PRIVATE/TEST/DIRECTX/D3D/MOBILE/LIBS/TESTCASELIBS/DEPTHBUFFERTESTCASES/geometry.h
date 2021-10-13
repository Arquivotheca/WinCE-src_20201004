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

HRESULT RenderGeometry(LPDIRECT3DMOBILEDEVICE pDevice);

#define D3DQA_DEPTHBUFTEST_VERT_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)

typedef struct _D3DQA_DEPTHBUFTEST_VERT {
	float x,y,z,rhw;
	D3DMCOLOR Diffuse;
} D3DQA_DEPTHBUFTEST_VERT, *LPD3DQA_DEPTHBUFTEST_VERT;

#define D3DQA_DEPTHBUFTEST_TRIVERT_PT D3DMPT_TRIANGLELIST
#define D3DQA_DEPTHBUFTEST_TRIVERTCOUNT 90
#define D3DQA_DEPTHBUFTEST_TRIPRIMCOUNT 30
#define D3DQA_DEPTHBUFTEST_TRIVERTSTART  0
/*
#define D3DQA_DEPTHBUFTEST_LINEVERT_PT D3DMPT_LINESTRIP
#define D3DQA_DEPTHBUFTEST_LINEVERTCOUNT 70
#define D3DQA_DEPTHBUFTEST_LINEPRIMCOUNT 69
#define D3DQA_DEPTHBUFTEST_LINEVERTSTART 90

#define D3DQA_DEPTHBUFTEST_POINTVERT_PT D3DMPT_POINTLIST
#define D3DQA_DEPTHBUFTEST_POINTVERTCOUNT  48 
#define D3DQA_DEPTHBUFTEST_POINTPRIMCOUNT  48 
#define D3DQA_DEPTHBUFTEST_POINTVERTSTART 160

#define D3DQA_DEPTHBUFTEST_VERTCOUNT (D3DQA_DEPTHBUFTEST_TRIVERTCOUNT+D3DQA_DEPTHBUFTEST_LINEVERTCOUNT+D3DQA_DEPTHBUFTEST_POINTVERTCOUNT)
*/
#define D3DQA_DEPTHBUFTEST_VERTCOUNT (D3DQA_DEPTHBUFTEST_TRIVERTCOUNT)


//
// Scale the following vertex data to viewport extents
//
__declspec(selectany) D3DQA_DEPTHBUFTEST_VERT DepthBufTestVerts[D3DQA_DEPTHBUFTEST_VERTCOUNT] = {
// |     X    |     Y     |    Z    |    RHW   |            DIFFUSE              |
// +----------+-----------+---------+----------+---------------------------------+

// Overlapping triangles, in back to front sorted order
	
{       0.00f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },
{       0.50f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },
{       0.00f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },

{       0.00f,      0.05f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },
{       0.45f,      0.50f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },
{       0.00f,      0.50f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },

{       0.00f,      0.10f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },
{       0.40f,      0.50f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },
{       0.00f,      0.50f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },

{       0.00f,      0.15f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },
{       0.35f,      0.50f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },
{       0.00f,      0.50f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },

{       0.00f,      0.20f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.30f,      0.50f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.00f,      0.50f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     

{       0.00f,      0.25f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.25f,      0.50f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.00f,      0.50f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     

{       0.00f,      0.30f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.20f,      0.50f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.00f,      0.50f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.00f,      0.35f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.15f,      0.50f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },
{       0.00f,      0.50f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     

{       0.00f,      0.40f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     
{       0.10f,      0.50f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     
{       0.00f,      0.50f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     

{       0.00f,      0.45f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     
{       0.05f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     
{       0.00f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     

{       0.00f,      0.45f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     
{       0.05f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     
{       0.00f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     

/*
{       0.00f,      0.??f,     0.?0f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.??f,      0.50f,     0.?0f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.00f,      0.50f,     0.?0f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
*/

// Overlapping triangles, in front to back sorted order


{       0.50f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     
{       0.50f,      0.05f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     
{       0.45f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,128)  },     

{       0.50f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     
{       0.50f,      0.05f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     
{       0.45f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,128,  0)  },     

{       0.50f,      0.00f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     
{       0.50f,      0.10f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     
{       0.40f,      0.00f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,  0,  0)  },     

{       0.50f,      0.00f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.50f,      0.15f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.35f,      0.00f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     

{       0.50f,      0.00f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.50f,      0.20f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.30f,      0.00f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.50f,      0.00f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.50f,      0.25f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.25f,      0.00f,     0.30f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     

{       0.50f,      0.00f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.50f,      0.30f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.20f,      0.00f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     

{       0.50f,      0.00f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.50f,      0.35f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.15f,      0.00f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     

{       0.50f,      0.00f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.50f,      0.40f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.10f,      0.00f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     

{       0.50f,      0.00f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.50f,      0.45f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.05f,      0.00f,     0.98f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     

{       0.50f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.50f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.00f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     


/*
{       0.50f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.50f,      0.??f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.??f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
*/

// Intersecting triangles

{       0.50f,      0.10f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.60f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       1.00f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.70f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.80f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.75f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     

{       0.90f,      0.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       1.00f,      0.10f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.50f,      0.50f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     

{       1.00f,      0.20f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       1.00f,      0.30f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.50f,      0.25f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     

{       1.00f,      0.40f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.90f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.50f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     

{       0.80f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.70f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.75f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     

{       0.60f,      0.50f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.50f,      0.40f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       1.00f,      0.00f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.50f,      0.30f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.50f,      0.20f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       1.00f,      0.25f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
/*
//
// Line strips: in back to front sorted order
//
{       0.00f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.00f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.00f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.00f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.00f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.00f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.00f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.05f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.05f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.05f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.05f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.05f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.05f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.05f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.10f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.10f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.10f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.10f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.10f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.10f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.10f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.15f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.15f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.15f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.15f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.15f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.15f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.15f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

{       0.20f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.20f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.20f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.20f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.20f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.20f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.20f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     

//
// Line strips: in front to back sorted order
//
{       0.25f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.25f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.25f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.25f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.25f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.25f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.25f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.30f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.30f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.30f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.30f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.30f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.30f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.30f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.35f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.35f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.35f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.35f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.35f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.35f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.35f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.40f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.40f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.40f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.40f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.40f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.40f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.40f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.45f,      0.75f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.45f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.45f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.45f,      0.90f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.45f,      0.60f,     0.90f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.45f,      1.00f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.45f,      0.50f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

//
// Point lists:  back to front sorted order
//
{       0.70f,      0.70f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.70f,      0.70f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.70f,      0.70f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.70f,      0.70f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.70f,      0.70f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.70f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.70f,      0.70f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.70f,      0.70f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     

{       0.75f,      0.70f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.75f,      0.70f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.75f,      0.70f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.75f,      0.70f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.75f,      0.70f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.75f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.75f,      0.70f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.75f,      0.70f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     

{       0.80f,      0.70f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
{       0.80f,      0.70f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.80f,      0.70f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.80f,      0.70f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.80f,      0.70f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.80f,      0.70f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.80f,      0.70f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.80f,      0.70f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     

//
// Point lists:  front to back sorted order
//
{       0.70f,      0.80f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.70f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.70f,      0.80f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.70f,      0.80f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.70f,      0.80f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.70f,      0.80f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.70f,      0.80f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.70f,      0.80f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.75f,      0.80f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.75f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.75f,      0.80f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.75f,      0.80f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.75f,      0.80f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.75f,      0.80f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.75f,      0.80f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.75f,      0.80f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     

{       0.80f,      0.80f,     0.00f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,  0)  },     
{       0.80f,      0.80f,     0.01f,     1.00f,     D3DMCOLOR_XRGB(128,128,128)  },     
{       0.80f,      0.80f,     0.10f,     1.00f,     D3DMCOLOR_XRGB(255,  0,255)  },     
{       0.80f,      0.80f,     0.20f,     1.00f,     D3DMCOLOR_XRGB(  0,255,255)  },     
{       0.80f,      0.80f,     0.70f,     1.00f,     D3DMCOLOR_XRGB(255,255,  0)  },     
{       0.80f,      0.80f,     0.80f,     1.00f,     D3DMCOLOR_XRGB(  0,  0,255)  },     
{       0.80f,      0.80f,     0.99f,     1.00f,     D3DMCOLOR_XRGB(  0,255,  0)  },     
{       0.80f,      0.80f,     1.00f,     1.00f,     D3DMCOLOR_XRGB(255,  0,  0)  },     
*/

};
