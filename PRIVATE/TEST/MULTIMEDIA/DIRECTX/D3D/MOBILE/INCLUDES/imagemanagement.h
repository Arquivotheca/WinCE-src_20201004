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

//
// Functions implemented in ImageManagement source code
//
HRESULT SetImageManagementLogger(void(*)(LPCTSTR,...));
void DeleteFiles(const WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiBeginFrame, UINT uiFrameCount, const FRAMEDESCRIPTION * const pFrameDescriptors);
HRESULT CreateDeltas(const WCHAR *wszPath, UINT uiTestID, UINT uiInstance1, UINT uiInstance2, UINT uiInstanceResult, UINT uiBeginFrame, UINT uiEndFrame, const FRAMEDESCRIPTION * const pFrameDescriptors);
HRESULT CheckImageDeltas(const WCHAR *wszPath, UINT uiTestID, UINT uiInstance, UINT uiBeginFrame, UINT uiEndFrame, const FRAMEDESCRIPTION * const pFrameDescriptors);
HRESULT DumpFrame(const WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HWND hWnd, RECT *pRect = NULL);
HRESULT DumpFrame(const WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HWND hWnd, BOOL Windowed, RECT *pRect = NULL);
HRESULT DumpFrame(const WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HDC hDC);
HRESULT DumpFlushedFrame(LPTESTCASEARGS pTestCaseArgs, UINT uiFrameNumber, RECT *pRect = NULL);



HRESULT ForceFlush(LPDIRECT3DMOBILEDEVICE pd3dDevice);

//
// Functions from CaptureBMP.LIB
//    
HRESULT SaveSurfaceToBMP(HWND hWnd, TCHAR *pszName, RECT *pRect=NULL);
HRESULT SaveSurfaceToBMP(HDC hDC, TCHAR *pszName);
HRESULT GetPixel(HDC hDC, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);
HRESULT GetPixel(HWND hWnd, int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);
HRESULT GetPixel(int iXPos, int iYPos, BYTE *pRed, BYTE *pGreen, BYTE *pBlue);

//
// Functions from DeltaBMP.LIB
//
HRESULT DeltaBMP(CONST TCHAR *pszFile1, CONST TCHAR *pszFile2, CONST TCHAR *pszResultFile);
HRESULT ValidateBitmapHeaders(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi);

//
// Functions from ImageQuality.LIB
//
HRESULT ImageQualityCheck(CONST TCHAR *pszDiffFile, float fIgnoredColorDeviation, float fMandatoryMatch);
