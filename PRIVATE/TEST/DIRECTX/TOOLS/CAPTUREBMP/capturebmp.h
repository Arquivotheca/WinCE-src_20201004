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
