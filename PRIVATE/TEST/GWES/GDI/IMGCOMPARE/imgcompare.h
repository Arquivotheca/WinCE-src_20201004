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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "results.h"

//structure containing the information to pass to the CompareRMS function
typedef struct tagRMSCOMPAREDATA {
    UINT uiThreshold;                    //IN: threshold to use in the RMS algorithm
    UINT uiAvgThreshold;                //IN: average threshold to use in the RMS algorithm
    DOUBLE dfPercentageDiffPixels;        //OUT: percentage of differing pixels
    DOUBLE dResultRMSRed;                //OUT: RMS result for red channel
    DOUBLE dResultRMSGreen;                //OUT: RMS result for green channel
    DOUBLE dResultRMSBlue;                //OUT: RMS result for blue channel

} RMSCOMPAREDATA;

typedef HRESULT (WINAPI * PFNCOMPAREPIXELS)(BYTE *bPtrBitsRef, int iWidthRef, INT iHeightRef, INT iWidthBytesRef, BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur, cCompareResults *cCR, VOID *pData);

//functions declaration (see implemetation for details)
//This assumes a fixed PixelDepth of 16
HRESULT ComparePixels(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef, 
                      BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
                      cCompareResults *cCR,
                      BYTE *bpPercentageDiffPixels);

HRESULT ComparePixelsPixelDepth(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef,
                      BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
					  WORD wPixelDepthBits,
                      cCompareResults *cCR,
                      byte *bpPercentageDiffPixels,
					  double *dpOutPercentageDiffPixels);


HRESULT ComparePixelsRMS(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef, 
                         BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
                         cCompareResults *cCR,
                         BYTE *pbRMSData    
                         );


HRESULT ComputeDiff(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef, 
                    BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur, 
                    cCompareResults *cCR,
                    const TCHAR *tszDiffFileName);

HRESULT GetFileDiff(const TCHAR *tszRefFileName, 
                    const TCHAR *tszCurFileName,
                    const TCHAR *tszDiffFileName,
                    cCompareResults *cCR);
                     

HBITMAP LoadDIBitmap(const TCHAR * tszName, cCompareResults *cCR);
HRESULT SaveBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes, cCompareResults *cCR);
HRESULT ValidateBitmapHeaders(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi, cCompareResults *cCR);

HRESULT GetDCRawData(HDC hDC, LPVOID *ppvBits, PINT Width, PINT Height, PINT Stride, cCompareResults *cCR);
HRESULT GetWndRawData(HWND hWnd, LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR);
HBITMAP GetFileRawData(const TCHAR * tszName, LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR);
HRESULT GetScreenRawData(LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR);


BOOL RecursiveCreateDirectory(TCHAR * tszPath);
void AppendResults(cCompareResults *cCR, TCHAR *szFormat, ...);
