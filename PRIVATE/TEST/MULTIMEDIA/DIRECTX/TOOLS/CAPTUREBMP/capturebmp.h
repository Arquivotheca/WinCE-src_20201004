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
#include <tchar.h>
//
// Stores the pixel data from the specified window, in a 24BPP bitmap.
//    
HRESULT SaveSurfaceToBMP(HWND hWnd, TCHAR *pszName, RECT *pRect);

//
// Stores the pixel data from the specified device context, in a 24BPP bitmap.
//    
HRESULT SaveSurfaceToBMP(HDC hDC, TCHAR *pszName);
HRESULT SaveSurfaceToBMP(HDC hDCSource, TCHAR *pszName, RECT *prExtents);

//
// Simple wrapper for GDI GetPixel; seperates channels from packed DWORD, returns
// any channels of interest via pointer arguments.
//
HRESULT GetPixel(HDC hDC, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);

//
// Simple wrapper for GDI GetPixel; seperates channels from packed DWORD, returns
// any channels of interest via pointer arguments.
//
HRESULT GetPixel(HWND hWnd, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);

//
// Screen coordinate GetPixel
//
HRESULT GetPixel(int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);
