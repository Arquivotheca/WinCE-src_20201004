//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    TxtrUtil.cpp

Abstract:

    Contains Direct3D Texture functionality.

-------------------------------------------------------------------*/

// ++++ Include Files +++++++++++++++++++++++++++++++++++++++++++++++
#include "Optimal.hpp"
#include "colorconv.h"
#include "TextureTools.h"


extern HANDLE g_hInstance;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    LoadTexture

Description:

    Creates a texture map surface and texture object from the named 
    bitmap file.  This is done in a two step process.  A source 
    DirectDraw surface and bitmap are created in system memory.  A second,
    initially empty, texture surface is created (in video memory if hardware
    is present).  The source bitmap is loaded into the destination texture
    surface and then discarded. This process allows a device to compress or
    reformat a texture map as it enters video memory during the Load call.

Arguments:

    TCHAR *tstrFileName  - Name of the texture to open

Return Value:

    LPDIRECT3DTEXTURE2   - Pointer to the loaded Texture (or NULL if failure)

-------------------------------------------------------------------*/
LPDIRECT3DMOBILETEXTURE
LoadTexture(TCHAR *tstrName)
{
    LPDIRECT3DMOBILETEXTURE  pd3dtexture = NULL;
    D3DMDISPLAYMODE mode;
    D3DMFORMAT format;

    g_pd3d->GetAdapterDisplayMode(g_uiAdapterOrdinal, &mode);

    for (format = D3DMFMT_R8G8B8; format <= D3DMFMT_X4R4G4B4; format = (D3DMFORMAT)(format + 1))
    {
        if (SUCCEEDED(g_pd3d->CheckDeviceFormat(g_uiAdapterOrdinal, D3DMDEVTYPE_DEFAULT, mode.Format, 0, D3DMRTYPE_TEXTURE, format)))
        {
            break;
        }
    }

    if (format <= D3DMFMT_X4R4G4B4)
    {
        GetTextureFromResource(
            tstrName,
            (HMODULE)g_hInstance,
            &UnpackFormat[format], 
            format, 
            g_memorypool, 
            &pd3dtexture, 
            g_p3ddevice);
//        GetTextureFromFile(
//            tstrFileName, 
//            &UnpackFormat[format], 
//            format, 
//            g_memorypool, 
//            &pd3dtexture, 
//            g_p3ddevice);
    }

    return pd3dtexture;
}
