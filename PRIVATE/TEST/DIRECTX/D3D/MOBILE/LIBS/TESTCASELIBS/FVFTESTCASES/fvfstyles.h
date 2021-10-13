//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#define D3DQA_NUMVERTS 4
#define D3DQA_NUMPRIM  2
#define D3DQA_PRIMTYPE D3DMPT_TRIANGLESTRIP



//    (1)        (2) 
//     +--------+  +
//     |       /  /|
//     |      /  / |
//     |     /  /  |
//     |    /  /   |
//     |   /  /    |
//     |  /  /     |
//     | /  /      | 
//     |/  /       |
//     +  +--------+
//    (3)         (4)
//

#define POSX1  0.0f
#define POSY1  0.0f
#define POSZ1  0.0f

#define POSX2  1.0f
#define POSY2  0.0f
#define POSZ2  0.0f

#define POSX3  0.0f
#define POSY3  1.0f
#define POSZ3  0.0f

#define POSX4  1.0f
#define POSY4  1.0f
#define POSZ4  0.0f


#include "TestCases.h"

#include "ColorCases.h"
#include "OneDimTexCases.h"
#include "TwoDimTexCases.h"
#include "PositionOnlyCases.h"
#include "ProjTexCoordCases.h"
#include "PrimOrientationCases.h"


typedef struct _D3DMFVF_TEST_DATA {
	DWORD dwFVF;
	PBYTE pVertexData;
	DWORD uiFVFSize;
	DWORD uiNumVerts;
	BOOL bProjected;
	UINT uiTTFStage;
	DWORD dwTTF;
	D3DMSHADEMODE ShadeMode;
} D3DMFVF_TEST_DATA;


__declspec(selectany) D3DMFVF_TEST_DATA TestData[D3DMQA_FVFTEST_COUNT] = {
//  |                       |                    |                              |                   |                  |   Projected   |              Projected              |                 |
//  |          FVF          |     Vert Data      |         Size Per Vert        |  Number of Verts  |  Projected Tex?  |   TTF Stage   |                 TTF                 |    ShadeMode    | 
//  +-----------------------+--------------------+------------------------------+-------------------+------------------+---------------+-------------------------------------+-----------------+
//

	//
	// 1D Tex Coord Test Cases
	//

	// Floating point primitive with 1D tex coords (vertical)
	{D3DMFVFTEST_ONED01_FVF,  (PBYTE)FVFOneD01,   sizeof(D3DMFVFTEST_ONED01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 1D tex coords (horizontal)
	{D3DMFVFTEST_ONED02_FVF,  (PBYTE)FVFOneD02,   sizeof(D3DMFVFTEST_ONED02),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //


	//
	// 2D Tex Coord Test Cases
	//

	// Floating point primitive with 1 set 2D tex coords
	{D3DMFVFTEST_TWOD01_FVF,  (PBYTE)FVFTwoD01,   sizeof(D3DMFVFTEST_TWOD01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 2 sets 2D tex coords
	{D3DMFVFTEST_TWOD02_FVF,  (PBYTE)FVFTwoD02,   sizeof(D3DMFVFTEST_TWOD02),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 3 sets 2D tex coords
	{D3DMFVFTEST_TWOD03_FVF,  (PBYTE)FVFTwoD03,   sizeof(D3DMFVFTEST_TWOD03),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 4 sets 2D tex coords
	{D3DMFVFTEST_TWOD04_FVF,  (PBYTE)FVFTwoD04,   sizeof(D3DMFVFTEST_TWOD04),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 1 set 2D tex coords; blended with diffuse (gouraud)
	{D3DMFVFTEST_TWOD05_FVF,  (PBYTE)FVFTwoD05,   sizeof(D3DMFVFTEST_TWOD05),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,     D3DMSHADE_GOURAUD}, //

	// Floating point primitive with 1 set 2D tex coords; blended with diffuse (flat)
	{D3DMFVFTEST_TWOD05_FVF,  (PBYTE)FVFTwoD05,   sizeof(D3DMFVFTEST_TWOD05),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with 1 set 2D tex coords; blended with specular (gouraud)
	{D3DMFVFTEST_TWOD06_FVF,  (PBYTE)FVFTwoD06,   sizeof(D3DMFVFTEST_TWOD06),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,     D3DMSHADE_GOURAUD}, //

	// Floating point primitive with 1 set 2D tex coords; blended with specular (flat)
	{D3DMFVFTEST_TWOD06_FVF,  (PBYTE)FVFTwoD06,   sizeof(D3DMFVFTEST_TWOD06),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //
	

	//
	// Color Vertices
	//

	// Floating point primitive with diffuse (gouraud shade)
	{D3DMFVFTEST_COLR01_FVF,  (PBYTE)FVFColr01,   sizeof(D3DMFVFTEST_COLR01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,     D3DMSHADE_GOURAUD}, //

	// Floating point primitive with diffuse (flat shade)
	{D3DMFVFTEST_COLR01_FVF,  (PBYTE)FVFColr01,   sizeof(D3DMFVFTEST_COLR01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with specular (gouraud shade)
	{D3DMFVFTEST_COLR02_FVF,  (PBYTE)FVFColr02,   sizeof(D3DMFVFTEST_COLR02),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,     D3DMSHADE_GOURAUD}, //

	// Floating point primitive with specular (flat shade)
	{D3DMFVFTEST_COLR02_FVF,  (PBYTE)FVFColr02,   sizeof(D3DMFVFTEST_COLR02),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitive with diffuse + specular (gouraud shade)
	{D3DMFVFTEST_COLR03_FVF,  (PBYTE)FVFColr03,   sizeof(D3DMFVFTEST_COLR03),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,     D3DMSHADE_GOURAUD}, //
                                     
	// Floating point primitive with diffuse + specular (flat shade)
	{D3DMFVFTEST_COLR03_FVF,  (PBYTE)FVFColr03,   sizeof(D3DMFVFTEST_COLR03),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //



	//
	// Position only test cases
	//

	// Floating point primitive - position only
	{D3DMFVFTEST_ONLY01_FVF,  (PBYTE)FVFOnly01,   sizeof(D3DMFVFTEST_ONLY01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //


	//
	// Primitive orientation test cases
	//


	// Floating point primitives (position 1) with 1D tex coords
	{D3DMFVFTEST_ORNT01_FVF,  (PBYTE)FVFOrnt01,   sizeof(D3DMFVFTEST_ORNT01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 2) with 1D tex coords
	{D3DMFVFTEST_ORNT02_FVF,  (PBYTE)FVFOrnt02,   sizeof(D3DMFVFTEST_ORNT01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 3) with 1D tex coords
	{D3DMFVFTEST_ORNT03_FVF,  (PBYTE)FVFOrnt03,   sizeof(D3DMFVFTEST_ORNT01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 4) with 1D tex coords
	{D3DMFVFTEST_ORNT04_FVF,  (PBYTE)FVFOrnt04,   sizeof(D3DMFVFTEST_ORNT01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 5) with 1D tex coords
	{D3DMFVFTEST_ORNT05_FVF,  (PBYTE)FVFOrnt05,   sizeof(D3DMFVFTEST_ORNT01),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 1) with 2D tex coords
	{D3DMFVFTEST_ORNT06_FVF,  (PBYTE)FVFOrnt06,   sizeof(D3DMFVFTEST_ORNT06),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 2) with 2D tex coords
	{D3DMFVFTEST_ORNT07_FVF,  (PBYTE)FVFOrnt07,   sizeof(D3DMFVFTEST_ORNT07),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 3) with 2D tex coords
	{D3DMFVFTEST_ORNT08_FVF,  (PBYTE)FVFOrnt08,   sizeof(D3DMFVFTEST_ORNT08),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 4) with 2D tex coords
	{D3DMFVFTEST_ORNT09_FVF,  (PBYTE)FVFOrnt09,   sizeof(D3DMFVFTEST_ORNT09),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //

	// Floating point primitives (position 5) with 2D tex coords
	{D3DMFVFTEST_ORNT10_FVF,  (PBYTE)FVFOrnt10,   sizeof(D3DMFVFTEST_ORNT10),     D3DQA_NUMVERTS,            FALSE,               0,                                   0,        D3DMSHADE_FLAT}, //


	//
	// Projected texture coordinate test cases
	//

	// Floating point projected to 1D tex coords (vertical)
	{D3DMFVFTEST_PROJ01_FVF,  (PBYTE)FVFProj01,   sizeof(D3DMFVFTEST_PROJ01),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 1D tex coords (horizontal)
	{D3DMFVFTEST_PROJ02_FVF,  (PBYTE)FVFProj02,   sizeof(D3DMFVFTEST_PROJ02),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 2D tex coords
	{D3DMFVFTEST_PROJ03_FVF,  (PBYTE)FVFProj03,   sizeof(D3DMFVFTEST_PROJ03),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,        D3DMSHADE_FLAT}, //

	// Floating point projected to 1D tex coords (vertical); blended with diffuse
	{D3DMFVFTEST_PROJ04_FVF,  (PBYTE)FVFProj04,   sizeof(D3DMFVFTEST_PROJ04),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ04_FVF,  (PBYTE)FVFProj04,   sizeof(D3DMFVFTEST_PROJ04),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 1D tex coords (horizontal); blended with diffuse
	{D3DMFVFTEST_PROJ05_FVF,  (PBYTE)FVFProj05,   sizeof(D3DMFVFTEST_PROJ05),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ05_FVF,  (PBYTE)FVFProj05,   sizeof(D3DMFVFTEST_PROJ05),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 2D tex coords; blended with diffuse
	{D3DMFVFTEST_PROJ06_FVF,  (PBYTE)FVFProj06,   sizeof(D3DMFVFTEST_PROJ06),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ06_FVF,  (PBYTE)FVFProj06,   sizeof(D3DMFVFTEST_PROJ06),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,        D3DMSHADE_FLAT}, //

	// Floating point projected to 1D tex coords (vertical); blended with specular
	{D3DMFVFTEST_PROJ07_FVF,  (PBYTE)FVFProj07,   sizeof(D3DMFVFTEST_PROJ07),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ07_FVF,  (PBYTE)FVFProj07,   sizeof(D3DMFVFTEST_PROJ07),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 1D tex coords (horizontal); blended with specular
	{D3DMFVFTEST_PROJ08_FVF,  (PBYTE)FVFProj08,   sizeof(D3DMFVFTEST_PROJ08),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ08_FVF,  (PBYTE)FVFProj08,   sizeof(D3DMFVFTEST_PROJ08),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT2,        D3DMSHADE_FLAT}, //

	// Floating point projected to 2D tex coords; blended with specular
	{D3DMFVFTEST_PROJ09_FVF,  (PBYTE)FVFProj09,   sizeof(D3DMFVFTEST_PROJ09),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,     D3DMSHADE_GOURAUD}, //
	{D3DMFVFTEST_PROJ09_FVF,  (PBYTE)FVFProj09,   sizeof(D3DMFVFTEST_PROJ09),     D3DQA_NUMVERTS,             TRUE,               0,  D3DMTTFF_PROJECTED | D3DMTTFF_COUNT3,        D3DMSHADE_FLAT}, //
};
