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

HRESULT SetLightingStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SupportsLightingTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SetupLightingGeometry(LPDIRECT3DMOBILEDEVICE pDevice, HWND hWnd, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB);

typedef struct _LIGHT_TESTS {

	//
	// Lighting
	//
	D3DMLIGHT Light;

	//
	// Material settings
	//

	D3DMMATERIAL Material;

	//
	// Render states
	//
	BOOL Lighting;            // D3DMRS_LIGHTING
	BOOL SpecularEnable;      // D3DMRS_SPECULARENABLE
	BOOL LocalViewer;         // D3DMRS_LOCALVIEWER
	BOOL ColorVertex;         // D3DMRS_COLORVERTEX
	D3DMMATERIALCOLORSOURCE DiffuseMatSrc;  // D3DMRS_DIFFUSEMATERIALSOURCE
	D3DMMATERIALCOLORSOURCE SpecularMatSrc; // D3DMRS_SPECULARMATERIALSOURCE
	D3DMMATERIALCOLORSOURCE AmbientMatSrc;  // D3DMRS_AMBIENTMATERIALSOURCE
	D3DMCOLOR GlobalAmbient;                // D3DMRS_AMBIENT
	BOOL NormalizeNormals;                  // D3DMRS_NORMALIZENORMALS


	D3DMVECTOR vecEye;        // Eye point, from which view matrix is computed, looking at geometry

} LIGHT_TESTS;

#define FLOAT_DONTCARE 1.0f


//
// NOTES ON RELATIONSHIP AMONGST:
//
//    * D3DMRS_COLORVERTEX
//    * D3DMRS_DIFFUSEMATERIALSOURCE
//    * D3DMRS_SPECULARMATERIALSOURCE
//    * D3DMRS_AMBIENTMATERIALSOURCE
//
// On drivers that do not expose D3DMVTXPCAPS_MATERIALSOURCE, the available control for toggling
// color sources is only configurable based on D3DMRS_COLORVERTEX:
// 
//
//                                    COLOR SOURCE BEHAVIOR
//                          FOR NON-D3DMVTXPCAPS_MATERIALSOURCE DRIVERS
// 
//                   |                                  |                                  |
//                   |    D3DMRS_COLORVERTEX == TRUE    |   D3DMRS_COLORVERTEX == FALSE    |
// ------------------+----------------------------------+----------------------------------+
//                   |                                  |                                  |
//   Diffuse Source  | "FVF Color 1" if it exists, else |       D3DMMATERIAL.Diffuse       |
//                   |       D3DMMATERIAL.Diffuse       |                                  |   
//                   |                             ( 1) |                             ( 4) |
// ------------------+----------------------------------+----------------------------------+
//                   |                                  |                                  |
//  Specular Source  | "FVF Color 2" if it exists, else |       D3DMMATERIAL.Specular      |
//                   |       D3DMMATERIAL.Specular      |                                  |
//                   |                             ( 2) |                             ( 5) |
// ------------------+----------------------------------+----------------------------------+
//                   |                                  |                                  |
//   Ambient Source  |        D3DMMATERIAL.Ambient      |       D3DMMATERIAL.Ambient       |
//                   |                             ( 3) |                             ( 6) |
// ------------------+----------------------------------+----------------------------------+
//
//
//
// On drivers that do expose D3DMVTXPCAPS_MATERIALSOURCE, the available control for toggling
// color sources is configurable based on D3DMRS_COLORVERTEX, and D3DMRS_*MATERIALSOURCE
// 
//                                    COLOR SOURCE BEHAVIOR
//                            FOR D3DMVTXPCAPS_MATERIALSOURCE DRIVERS
// 
//                   |                                    |                                  |
//                   |      D3DMRS_COLORVERTEX == TRUE    |   D3DMRS_COLORVERTEX == FALSE    |
// ------------------+------------------------------------+----------------------------------+
//                   |                                    |                                  |
//                   |  D3DMRS_DIFFUSEMATERIALSOURCE if   |                                  |
//                   |  designated source exists, else    |                                  |
//   Diffuse Source  |       D3DMMATERIAL.Ambient         |       D3DMMATERIAL.Diffuse       |   
//                   |                                    |                                  |
//                   | (default RS value: D3DMMCS_COLOR1) |                                  |
//                   |                               ( 7) |                             (10) |
// ------------------+------------------------------------+----------------------------------+
//                   |                                    |                                  |
//                   |  D3DMRS_SPECULARMATERIALSOURCE if  |                                  |
//                   |  designated source exists, else    |                                  |
//  Specular Source  |       D3DMMATERIAL.Ambient         |       D3DMMATERIAL.Specular      |
//                   |                                    |                                  |
//                   | (default RS value: D3DMMCS_COLOR2) |                                  |
//                   |                               ( 8) |                             (11) |
// ------------------+------------------------------------+----------------------------------+
//                   |                                    |                                  |
//                   |   D3DMRS_AMBIENTMATERIALSOURCE if  |                                  |
//                   |   designated source exists, else   |                                  |
//   Ambient Source  |      D3DMMATERIAL.Ambient          |       D3DMMATERIAL.Ambient       |
//                   |                                    |                                  |
//                   |(default RS value: D3DMMCS_MATERIAL)|                                  |
//                   |                               ( 9) |                             (12) |
// ------------------+------------------------------------+----------------------------------+
//

//
// Isolating Lighting Formulas
//
//
// 
// 
//

//
// Some test cases do not require the use of a dynamic light
//
#define D3DMLIGHT_UNUSED (D3DMLIGHTTYPE)0

__declspec(selectany) LIGHT_TESTS LightTestCases[D3DMQA_LIGHTTEST_COUNT] = {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
//                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
//   ** == Should not influence outcome of directional lighting calculations                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
//                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
//  |                     | |                                   |  |                                   |  |                                   |   |                                |  |                                |   |  **   | |  **   | |  **   | |  **   |  |                                           | |                                           | |                                           | |             | |                     |   |                       | |                       | |                     | |                  | |                  | |                  | |                           |  |                           | |                                   |
//  |                     | |         D3DMLIGHT DIFFUSE         |  |        D3DMLIGHT SPECULAR         |  |         D3DMLIGHT AMBIENT         |   |      D3DMLIGHT POSITION **     |  |      D3DMLIGHT DIRECTION       |   |       | |       | |       | |       |  |            D3DMMATERIAL.Diffuse           | |            D3DMMATERIAL.Ambient           | |            D3DMMATERIAL.Specular          | |D3DMMATERIAL.| |                     |   |                       | |                       | |                     | |                  | |                  | |                  | |                           |  |                           | |                                   | 
//  |                     | |                                   |  |                                   |  |                                   |   |                                |  |                                |   | RANGE | | Attn0 | | Attn1 | | Attn2 |  |                                           | |                                           | |                                           | |    Power    | |   D3DMRS_LIGHTING   |   | D3DMRS_SPECULARENABLE | |   D3DRS_LOCALVIEWER   | | D3DMRS_COLORVERTEX  | |  D3DMRS_DIFFUSE  | |  D3DMRS_SPECULAR | |  D3DMRS_AMBIENT  | |                           |  |                           | |                                   |      
//  |   D3DMLIGHT.Type    | |    R   |    G   |    B   |    A   |  |    R   |    G   |    B   |    A   |  |    R   |    G   |    B   |    A   |   |     X    |     Y    |     Z    |  |     X    |     Y    |     Z    |   |       | |       | |       | |       |  |     R    |     G    |     B    |     A    | |     R    |     G    |     B    |     A    | |     R    |     G    |     B    |     A    | |             | |                     |   |                       | |                       | |                     | |  MATERIALSOURCE  | |  MATERIALSOURCE  | |  MATERIALSOURCE  | |       D3DMRS_AMBIENT      |  |  D3DMRS_NORMALIZENORMALS  | |         D3DMVECTOR vecEye         |
//  +---------------------+ +--------+--------+--------+--------+  +--------+--------+--------+--------+  +--------+--------+--------+--------+   +----------+----------+----------+  +----------+----------+----------+   +-------+ +-------+ +-------+ +-------+  +----------+----------+----------+----------+ +----------+----------+----------+----------+ +----------+----------+----------+----------+ +-------------+ +---------------------+   +-----------------------+ +-----------------------+ +---------------------+ +------------------+ +------------------+ +------------------+ +---------------------------+  +---------------------------+ +-----------------------------------+

//
// (1 Case ) D3DMRS_LIGHTING==FALSE Test
//
{   {     D3DMLIGHT_UNUSED, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, { _M(1.0f),_M(1.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                 FALSE,                      FALSE,                    FALSE,                  FALSE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) D3DMRS_COLORVERTEX==FALSE Tests (color fetch from material regardless of D3DMRS_*MATERIALSOURCE)
//
//               * Diffuse color test
//               * Specular color test
//               * Ambient color test
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, { _M(1.0f),_M(1.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) D3DMRS_AMBIENTMATERIALSOURCE Tests: 
//
//               * D3DMMCS_COLOR1
//               * D3DMMCS_COLOR2
//               * D3DMMCS_MATERIAL
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR1,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR2,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) D3DMRS_DIFFUSEMATERIALSOURCE Tests:
//
//               * D3DMMCS_COLOR1
//               * D3DMMCS_COLOR2
//               * D3DMMCS_MATERIAL
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) D3DMRS_SPECULARMATERIALSOURCE Tests w/D3DMRS_SPECULARENABLE==FALSE:
//
//               * D3DMMCS_COLOR1
//               * D3DMMCS_COLOR2
//               * D3DMMCS_MATERIAL
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR1,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) D3DMRS_SPECULARMATERIALSOURCE Tests w/D3DMRS_SPECULARENABLE==TRUE:
//
//               * D3DMMCS_COLOR1
//               * D3DMMCS_COLOR2
//               * D3DMMCS_MATERIAL
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR1,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(255,255,255),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
//
// AMBIENT LIGHTING TESTS
//
//

//
// (2 Cases) Ambient Lighting Tests; verify computations
//
//               * mult between Ca and Ga
//               * add between Ga and Lai
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.8f), _M( 0.8f), _M( 0.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(192,192,192),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(1.0f),_M(1.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 1.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,255,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
//
// SPECULAR LIGHTING TESTS
//
//

//
// (2 Case) Specular Lighting Halfway Vector Basic Tests:
//
//               * D3DMRS_LOCALVIEWER==FALSE
//               * D3DMRS_LOCALVIEWER==TRUE
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 1.0f), _M( 1.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f),  _M(11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 1.0f), _M( 1.0f), _M( 1.0f), _M( 1.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                  FALSE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f),  _M(11.0f)}},

//
// (7 Cases) Specular Lighting Test (D3DMRS_LOCALVIEWER==FALSE): Various D3DMLIGHT.Direction vectors 
// 
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(-10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( -1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( 10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},

//
// (7 Cases) Specular Lighting Test (D3DMRS_LOCALVIEWER==TRUE): Various D3DMLIGHT.Direction vectors
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(-10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( -1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( 10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                     TRUE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},

//
// (3 Cases) Specular Test: Various directional light colors
//
//               * Red
//               * Green
//               * Blue
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(1.0f),_M(0.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(0.0f),_M(0.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (3 Cases) Specular Test: Light direction vector normalization test
//
//               * Shorter than normalized length
//               * Normalized
//               * Longer than normalized length
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(100.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (5 Cases) Specular Test:  Various Powers
//
//               * 0.0f
//               * 0.5f
//               * 1.0f
//               * 2.0f
//               * 10.0f
//
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          0.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          0.5f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          2.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},         10.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

//
// (5 Cases) Specular Test:  D3DMLIGHT_POINT Position Test
//
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M( -5.0f),_M( -5.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M( -5.0f),_M(  5.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  5.0f),_M(  5.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  5.0f),_M( -5.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
 
//
// (5 Cases) Specular Test:  D3DMLIGHT_POINT Range Test
//
//               * Range = 1.0f
//               * Range = 2.0f
//               * Range = 3.0f
//               * Range = 4.0f
//               * Range = 5.0f
//
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      1.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      2.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      3.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      4.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      5.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},


//
// (3 Cases) Specular Test:  Point Lights & Attenuation
//
//               * Attenuation0 (Constant attenuation)
//               * Attenuation1 (Linear attenuation)
//               * Attenuation2 (Quadratic attenuation)
//
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     0.0f,     1.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     0.0f,     0.0f,     1.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                       TRUE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
 

//
//
// DIFFUSE LIGHTING TESTS
//
//


//
// (7 Cases) Diffuse Lighting Test: Various D3DMLIGHT.Direction vectors
//
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(-10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( -1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  1.0f),_M(  0.0f),_M( 10.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M( 10.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M( -1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M( -4.0f), _M( -4.0f), _M(-8.0f)}},


//
// (3 Cases) Diffuse Test: Various directional light colors
//
//               * Red
//               * Green
//               * Blue
//
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(0.0f),_M(1.0f),_M(0.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(0.0f),_M(0.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},


//
// (3 Cases) Diffuse Test: Light direction vector normalization test
//
//               * Shorter than normalized length
//               * Normalized
//               * Longer than normalized length
//
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.1f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(  1.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_DIRECTIONAL, { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)}, {_M(  0.0f),_M(  0.0f),_M(100.0f)},      0.0f,     0.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},


//
// (5 Cases) Diffuse Test:  D3DMLIGHT_POINT Position Test
//
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M( -5.0f),_M( -5.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M( -5.0f),_M(  5.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  5.0f),_M(  5.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  5.0f),_M( -5.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
 
//
// (5 Cases) Diffuse Test:  D3DMLIGHT_POINT Range Test
//
//               * Range = 5.0f
//               * Range = 5.5f
//               * Range = 6.0f
//               * Range = 6.5f
//               * Range = 7.0f
//
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      5.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      5.5f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      6.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      6.5f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -5.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},      7.0f,     1.0f,     0.0f,     0.0f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},


//
// (3 Cases) Diffuse Test:  Point Lights & Attenuation
//
//               * Attenuation0 (Constant attenuation)
//               * Attenuation1 (Linear attenuation)
//               * Attenuation2 (Quadratic attenuation)
//
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -3.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     1.0f,    0.00f,    0.00f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -3.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},     10.0f,     0.0f,    0.40f,    0.00f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},
{   {D3DMLIGHT_POINT      , { _M(1.0f),_M(1.0f),_M(1.0f),_M(1.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(0.0f)}, {_M(0.0f),_M(0.0f),_M(0.0f),_M(1.0f)}, {_M(  0.0f),_M(  0.0f),_M( -3.0f)}, {_M(  0.0f),_M(  0.0f),_M(  0.0f)},  10000.0f,     0.0f,    0.00f,    0.05f}, {{_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)}, {_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f)},          1.0f},                  TRUE,                      FALSE,                    FALSE,                   TRUE,      D3DMMCS_COLOR1,      D3DMMCS_COLOR2,    D3DMMCS_MATERIAL,  D3DMCOLOR_XRGB(  0,  0,  0),                         FALSE, {_M(  0.0f), _M(  0.0f), _M(-11.0f)}},

};


