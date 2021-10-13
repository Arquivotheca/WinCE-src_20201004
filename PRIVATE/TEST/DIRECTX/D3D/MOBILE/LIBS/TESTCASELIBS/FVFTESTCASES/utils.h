//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
