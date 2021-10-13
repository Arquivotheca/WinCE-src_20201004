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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once
#include <windows.h>
#include <d3dm.h>
#include "TestCases.h"

namespace AddtlSwapChainsNamespace
{
#define _M(_a) D3DM_MAKE_D3DMVALUE(_a),

#define countof(x) (sizeof(x) / sizeof(*x)),

    HRESULT PrepareMainHWnd(
        LPDIRECT3DMOBILEDEVICE  pDevice,
        HWND                    hWnd);

    HRESULT CreateSwapChains(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex,
        LPDIRECT3DMOBILESWAPCHAIN   **prgpSwapChains,
        HWND                        **ppSwapWindows);

    HRESULT DestroySwapChains(
        DWORD                       dwTableIndex,
        LPDIRECT3DMOBILESWAPCHAIN * rgpSwapChains,
        HWND                      * pSwapWindows);
        
    HRESULT CreateAndPrepareVertexBuffers(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex, 
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffer,
        UINT                         *pVBStride);
        
    HRESULT PrepareDeviceWithSwapChain(
        LPDIRECT3DMOBILEDEVICE pDevice,
        LPDIRECT3DMOBILESWAPCHAIN * rgpSwapChains,
        LPDIRECT3DMOBILEVERTEXBUFFER pVertexBuffer,
        DWORD CurrentSwapChain);

    HRESULT SetupTextureStages(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex);

    HRESULT SetupRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex);

    const float FLOAT_DONTCARE = 1.0f;

    //////////////////////////////////////////////
    //
    // Geometry definitions
    //
    /////////////////////////////////////////////

    const D3DMPRIMITIVETYPE D3DMQA_PRIMTYPE = D3DMPT_TRIANGLESTRIP;

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
    //    (3)         (4),
    //

    const float POSX1 = 0.0f;
    const float POSY1 = 0.0f;
    const float POSZ1 = 0.0f;

    const float POSX2 = 1.0f;
    const float POSY2 = 0.0f;
    const float POSZ2 = 0.0f;

    const float POSX3 = 0.0f;
    const float POSY3 = 1.0f;
    const float POSZ3 = 0.0f;

    const float POSX4 = 1.0f;
    const float POSY4 = 1.0f;
    const float POSZ4 = 0.0f;

    const DWORD D3DM_ADDTLSWAPCHAINS_FVF = (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE);
        
    typedef struct _D3DM_ADDTLSWAPCHAINS {
        float x, y, z, rhw;
        DWORD diffuse;
    } D3DM_ADDTLSWAPCHAINS;

    const UINT VERTCOUNT = 4;

    enum ESize {
        Smaller,
        Same,
        Larger
    };

    const DWORD ASCLOC_ECLIPSE    = 0;
    const DWORD ASCLOC_UPPERLEFT  = 1;
    const DWORD ASCLOC_UPPERRIGHT = 2;
    const DWORD ASCLOC_LOWERLEFT  = 3;
    const DWORD ASCLOC_LOWERRIGHT = 4;
    const DWORD ASCLOC_NOOVERLAP  = 5;

    const DWORD ASCSWAP_DEFAULT = 0;
    const DWORD ASCSWAP_ADDTL1  = 1;
    const DWORD ASCSWAP_ADDTL2  = 2;
    const DWORD ASCSWAP_ADDTL3  = 3;
    const DWORD ASCSWAP_ADDTL4  = 4;
    const DWORD ASCSWAP_ADDTL5  = 5;
    const DWORD ASCSWAP_ADDTL6  = 6;

#define ASCLOC(swap, location) ((location) << ((swap)*3))
#define ASCUNLOC(swap, location) (((location) >> ((swap)*3)) & 0x7)

    typedef struct _ADDTLSWAPCHAINS_TESTS {
        // Size: Are the Addtl. swap chains larger, same, or smaller than the device?
        ESize Size;

        // DevZOrder: Where does the Device window come in the ZOrder of windows
        DWORD DevZOrder;

        // DevPresent: When do we present the device swap chain relative to the others?
        DWORD DevPresent;

        // AddtlSwapChainsCount: How many addtl swap chains will be created?
        DWORD AddtlSwapChainsCount;

        // Which Swap chain will be captured in this test case? (0 = device swap chain),
        DWORD TargetSwapChain;

        // Locations: Where are each of the swap chains located relative to the device?
        DWORD Locations;
    } ADDTLSWAPCHAINS_TESTS;


    __declspec(selectany) ADDTLSWAPCHAINS_TESTS AddtlSwapChainsCases[D3DMQA_ADDTLSWAPCHAINSTEST_COUNT] = {
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // One additional swap chain, with different sizes, on top of the device
    //  Size   | DevZOrder | DevPresent | AddtlSwapChainsCount | TargetSwapChain | Locations
        Smaller, 1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Smaller, 1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Larger,  1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Larger,  1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // One additional swap chain, with same size, but different locations, presentation orders, and Z orders.

    // Device is on top and presented first
    //  Size   | DevZOrder | DevPresent | AddtlSwapChainsCount | TargetSwapChain | Locations
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    0,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
        Same,    0,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
    // Device is on top and presented last
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    0,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
        Same,    0,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
    // Device is on bottom and presented first
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    1,          0,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
        Same,    1,          0,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
    // Device is on bottom and presented last
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE),
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT),
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT),
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT),
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERRIGHT),
        Same,    1,          1,           1,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),
        Same,    1,          1,           1,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_NOOVERLAP),

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Two additional swap chains, same size with device on bottom

    // Device is presented first
    //  Size   | DevZOrder | DevPresent | AddtlSwapChainsCount | TargetSwapChain | Locations
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          0,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
    // Device is presented second
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          1,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
    // Device is presented third
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERRIGHT),
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERLEFT),
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_UPPERRIGHT)| ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),
        Same,    2,          2,           2,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_LOWERLEFT) | ASCLOC(ASCSWAP_ADDTL2, ASCLOC_LOWERRIGHT),

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Six additional swap chains, same size with device on bottom (these will skip if not possible.

    // Device is presented first and first in zorder
    //  Size   | DevZOrder | DevPresent | AddtlSwapChainsCount | TargetSwapChain | Locations
        Same,    0,          0,           6,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     3,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     4,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     5,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          0,           6,                     6,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
    // Device is presented last, but first in z order
        Same,    0,          6,           6,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     3,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     4,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     5,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    0,          6,           6,                     6,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
    // Device is presented first, but last in z order
        Same,    6,          0,           6,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     3,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     4,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     5,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          0,           6,                     6,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
    // Device is presented last and is last in z order
        Same,    6,          6,           6,                     0,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     1,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     2,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     3,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     4,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     5,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP),
        Same,    6,          6,           6,                     6,                ASCLOC(ASCSWAP_ADDTL1, ASCLOC_ECLIPSE) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL2, ASCLOC_UPPERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL3, ASCLOC_UPPERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL4, ASCLOC_LOWERLEFT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL5, ASCLOC_LOWERRIGHT) | 
                                                                                   ASCLOC(ASCSWAP_ADDTL6, ASCLOC_NOOVERLAP)
    };
};

