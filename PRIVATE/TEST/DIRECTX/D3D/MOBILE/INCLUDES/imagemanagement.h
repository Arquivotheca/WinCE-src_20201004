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

//
// Functions implemented in ImageManagement source code
//
void DeleteFiles(WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiBeginFrame, UINT uiFrameCount);
HRESULT CreateDeltas(WCHAR *wszPath, UINT uiTestID, UINT uiInstance1, UINT uiInstance2, UINT uiInstanceResult, UINT uiBeginFrame, UINT uiEndFrame);
HRESULT CheckImageDeltas(WCHAR *wszPath, UINT uiTestID, UINT uiInstance, UINT uiBeginFrame, UINT uiEndFrame);
HRESULT DumpFrame(WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HWND hWnd, RECT *pRect = NULL);
HRESULT DumpFrame(WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HWND hWnd, BOOL Windowed, RECT *pRect = NULL);
HRESULT DumpFrame(WCHAR *wszPath, UINT uiInstance, UINT uiTestID, UINT uiFrameNumber, HDC hDC);
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
