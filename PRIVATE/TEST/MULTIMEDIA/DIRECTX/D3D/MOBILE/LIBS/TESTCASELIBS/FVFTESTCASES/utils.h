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

HRESULT ResizeTarget(LPDIRECT3DMOBILEDEVICE pDevice,
                     HWND hWnd,
                     D3DMPRESENT_PARAMETERS* pPresentParms,
                     UINT uiRenderExtentX,
                     UINT uiRenderExtentY);

HRESULT SetupTexture(LPDIRECT3DMOBILEDEVICE pDevice,
                     TCHAR *ptchSrcFile,
                     LPDIRECT3DMOBILETEXTURE *ppTexture,
                     UINT uiTextureStage);

HRESULT SetupTexture(LPDIRECT3DMOBILEDEVICE pDevice,
                     INT iResourceID,
                     LPDIRECT3DMOBILETEXTURE *ppTexture,
                     UINT uiTextureStage);


//
// Convenient package for texture stage state settings
//
typedef struct _TEX_STAGE_SETTINGS {
	D3DMTEXTUREOP ColorOp;
	D3DMTEXTUREOP AlphaOp;
	DWORD ColorTexArg1;
	DWORD ColorTexArg2;
	DWORD AlphaTexArg1;
	DWORD AlphaTexArg2;
} TEX_STAGE_SETTINGS;

HRESULT SetTestTextureStates(LPDIRECT3DMOBILEDEVICE pDevice,
                             TEX_STAGE_SETTINGS *pTexStageSettings,
                             UINT uiStageIndex);

HRESULT SetDefaultTextureStates(LPDIRECT3DMOBILEDEVICE pDevice);
