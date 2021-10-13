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
#include <windows.h>
#include <math.h>
#include "imgcompare.h"
#include "utils.h"

bool ComparePixel(BYTE* pBitsOffsetRef, BYTE* pBitsOffsetCur, WORD wPixelByteCount, DWORD* pPixelDataRef, DWORD* pPixelDataCur);

//
// ComparePixels
//
//   The algorithm of this function does a pixel perfect comparison. It takes two images
//   as input and counts the number of pixels that are not identical. The width and height
//   can be different.  If PercentageDiffPixels is available, it will pass fail depending on whether
//   or not the percentage different is larger than the constraint given.
//
// Arguments:
//
//   BYTE *bPtrBitsRef: pointer to the bitmap data of the reference image
//   UINT iWidthRef: width of the reference image
//   UINT iHeightRef: height of the reference image
//   UINT iWidthBytesRef: width in bytes of the reference image
//
//   BYTE *bPtrBitsCur: pointer to the bitmap data of the image to compare
//   UINT bPtrBitsCur: width of the image to compare
//   UINT iWidthCur: height of the image to compare
//   UINT iWidthBytesCur: width in bytes of the image to compare
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//   DOUBLE *dpPercentageDiffPixels: pointer to a variable that will contain the resulting percentage of
//                                     dirrering pixels (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code, E_FAIL on mismatch, E_ABORT or some more descriptive
//
//
HRESULT ComparePixels(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef,
                      BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
                      cCompareResults *cCR,
                      byte *bpPercentageDiffPixels)
{
	return ComparePixelsPixelDepth(bPtrBitsRef, iWidthRef, iHeightRef, iWidthBytesRef,
							bPtrBitsCur, iWidthCur, iHeightCur, iWidthBytesCur, 16,
							cCR, bpPercentageDiffPixels, NULL);
}

//
// ComparePixels
//
//   The algorithm of this function does a pixel perfect comparison. It takes two images
//   as input and counts the number of pixels that are not identical. The width and height
//   can be different.  If PercentageDiffPixels is available, it will pass fail depending on whether
//   or not the percentage different is larger than the constraint given.
//
// Arguments:
//
//   BYTE *bPtrBitsRef: pointer to the bitmap data of the reference image
//   UINT iWidthRef: width of the reference image
//   UINT iHeightRef: height of the reference image
//   UINT iWidthBytesRef: width in bytes of the reference image
//
//   BYTE *bPtrBitsCur: pointer to the bitmap data of the image to compare
//   UINT bPtrBitsCur: width of the image to compare
//   UINT iWidthCur: height of the image to compare
//   UINT iWidthBytesCur: width in bytes of the image to compare
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//   DOUBLE *dpPercentageDiffPixels: pointer to a variable that will contain the resulting percentage of
//                                     dirrering pixels (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code, E_FAIL on mismatch, E_ABORT or some more descriptive
//
//
HRESULT ComparePixelsPixelDepth(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef,
                      BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
					  WORD wPixelDepthBits,
                      cCompareResults *cCR,
                      byte *bpPercentageDiffPixels,
					  double *dpOutPercentageDiffPixels)

{
    BYTE    *pBitsOffsetRef,
            *pBitsOffsetCur;

    DWORD   dwPixelDataRef = 0,
            dwPixelDataCur = 0;

    INT    iWidth = 0,
            iHeight = 0,
            iX = 0,
            iY = 0;
    UINT uiDiffCount = 0;
	DOUBLE diffPerecnt = 0;

	//Calculate how many bytes each pixel is
	WORD wPixelByteCount = wPixelDepthBits >> 3;

    HRESULT hr = S_OK;
    DOUBLE *dpPercentageDiffPixels = (DOUBLE *) bpPercentageDiffPixels;
	DOUBLE maxPercentageDiffPixels = 0;

    bool fTopDown = false;

	if (bpPercentageDiffPixels)
	{
		maxPercentageDiffPixels = *dpPercentageDiffPixels;
	}

    //
    // Argument validation
    //
    if ((NULL == bPtrBitsRef) ||
        (NULL == bPtrBitsCur))
    {
        AppendResults(cCR, TEXT("Invalid Bitmap Pointer\n"));
        return E_POINTER;
    }

    if ((0 >= iWidthRef) ||
        (0 >= iWidthCur))
    {
        AppendResults(cCR, TEXT("Invalid Width\n"));
        return E_INVALIDARG;
    }

    if ((0 == iHeightRef) ||
        (0 == iHeightRef))
    {
        AppendResults(cCR, TEXT("Invalid Height\n"));
        return E_INVALIDARG;
    }

    if ((0 >= iWidthBytesRef) ||
        (0 >= iWidthBytesCur))
    {
        AppendResults(cCR, TEXT("Invalid Byte Width\n"));
        return E_INVALIDARG;
    }

    if(iHeightRef < 0 || iHeightCur < 0)
    {
        if((iHeightRef < 0 && iHeightCur > 0) ||
            (iHeightRef > 0 && iHeightCur < 0))
        {
            AppendResults(cCR, TEXT("If either image is top down, then both must be top down."));
            return E_INVALIDARG;
        }
        else
        {
            fTopDown = true;
            iHeightRef *= -1;
            iHeightCur *= -1;
        }
    }

    //need that in case size of the images are different
    iWidth = min(iWidthRef, iWidthCur);
    iHeight = min(iHeightRef, iHeightCur);

    for (iY = 0; iY < iHeight; iY++)
    {
        // point the expected and actual pixel pointers to the beginning of the scanline
        // the (iY + iHeightRef - iHeight) allows us to compare images of different heights
        pBitsOffsetRef = bPtrBitsRef + (iY + iHeightRef - iHeight) * iWidthBytesRef;
        pBitsOffsetCur = bPtrBitsCur + (iY + iHeightCur - iHeight) * iWidthBytesCur;

        for (iX = 0; iX < iWidth; iX++, pBitsOffsetRef += wPixelByteCount, pBitsOffsetCur += wPixelByteCount)
        {
            
            if (!ComparePixel(pBitsOffsetRef, pBitsOffsetCur, wPixelByteCount, &dwPixelDataRef, &dwPixelDataCur))
            {
                uiDiffCount++;
				diffPerecnt = (DOUBLE) ((DOUBLE)uiDiffCount / ((DOUBLE)iWidth * (DOUBLE)iHeight)) * 100.0f;
				
                if(!dpPercentageDiffPixels || diffPerecnt > maxPercentageDiffPixels)
                {
                    AppendResults(cCR, TEXT("Comparison precent (%lf) exceeded threshold (%lf), incorrect pixel at (%d, %d) expected (0x%08X) found (0x%08X)\n"),
									   diffPerecnt,
									   maxPercentageDiffPixels,
                                       iX,
                                       fTopDown?iY:iHeight - iY - 1,
                                       dwPixelDataRef,
                                       dwPixelDataCur);
                    hr = E_FAIL;
                }
                else
                {
                    AppendResults(cCR, TEXT("Comparison found incorrect pixel at (%d, %d) expected (0x%08X) found (0x%08X) total diff (%lf) \n"),
                                       iX,
                                       fTopDown?iY:iHeight - iY - 1,
                                       dwPixelDataRef,
                                       dwPixelDataCur,
									   diffPerecnt);
                }
            }


        }
    }

	//cCR can be NULL
	if (cCR)
	{
		cCR->SetPassResult(SUCCEEDED(hr));
	}

	if (dpOutPercentageDiffPixels)
	{
		*dpOutPercentageDiffPixels = diffPerecnt;
	}

    return hr;
}

//
// ComparePixelsRMS
//
//   This function uses the RMS algorithm to verify that there aren't clusters of pixels
//   that are each off by quite a bit but still within the single-pixel threshold.
//
//   The RMS algorithm works over a 4x4 pixel area. It calculates the square of the difference
//   for each color channel for each pixel between the reference image and the test image.
//   It then adds these squared differences from each pixel in the 4x4 pixel block.
//   It then takes the square root of this, and compares the result with the given threshold.
//
//   The purpose of this algorithm is to find errors in lossy image formats, such as JPEG.
//   In a lossy image format, we want to make sure that on average, the resulting decompressed image
//   looks like the original uncompressed image. The single-pixel comparison will catch things like a pixel
//   that is black when it is supposed to be green, but in a lossy JPEG, there will be pixels that are quite
//   a bit different from the original but still acceptable when looked at because the surrounding
//   pixels are much closer.
//
// Arguments:
//
//   BYTE *bPtrBitsRef: pointer to the bitmap data of the reference image
//   UINT iWidthRef: width of the reference image
//   UINT iHeightRef: height of the reference image
//   UINT iWidthBytesRef: width in bytes of the reference image
//
//   BYTE *bPtrBitsCur: pointer to the bitmap data of the image to compare
//   UINT bPtrBitsCur: width of the image to compare
//   UINT iWidthCur: height of the image to compare
//   UINT iWidthBytesCur: width in bytes of the image to compare
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//   RMSCOMPAREDATA *pRMSData: a pointer to the RMSCOMPAREDATA struct, see header file for details (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code, E_FAIL on mismatch, E_ABORT or a more descriptive error code on error.
//
HRESULT ComparePixelsRMS(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef,
                         BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
                         cCompareResults *cCR,
                         BYTE *pbRMSData
                         )
{
    short siRed,
          siBlue,
          siGreen;

    // 24 bpp bitmaps are stored in BGR format.
    const int iRedOffsetT    = 2;
    const int iGreenOffsetT = 1;
    const int iBlueOffsetT    = 0;

    // The reference can sometimes be an Imaging bitmap, and the imaging
    // library doesn't support 24bpp bgr
    int iRedOffsetR   = 2,
        iGreenOffsetR = 1,
        iBlueOffsetR  = 0;

    // These are doubles to prevent the overflow that is possible with longs.
    double dfGlobalRMSRed,
           dfGlobalRMSGreen,
           dfGlobalRMSBlue;

    INT    iWidth = 0,
            iHeight = 0,
            iX = 0,
            iY = 0;
    UINT uiDiffCount = 0,
            uiThreshold = 6,        //this is the default threshold (might need revision)
            uiAvgThreshold = 6;        //this is the default average threshold (might need revision)

    BITMAPINFO bmi24;
    HBITMAP hbmpDif = NULL;
    BYTE * pbDifPixels = NULL;
    DIBSECTION dibDif;
    RMSCOMPAREDATA *pRMSData = (RMSCOMPAREDATA *) pbRMSData;

    HRESULT hr = S_OK;

    bool fTopDown = false;

    //
    // Argument validation
    //
    if ((NULL == bPtrBitsRef) ||
        (NULL == bPtrBitsCur))
    {
        AppendResults(cCR, TEXT("Invalid Bitmap Pointer\n"));
        return E_POINTER;
    }

    if ((0 >= iWidthRef) ||
        (0 >= iWidthCur))
    {
        AppendResults(cCR, TEXT("Invalid Width\n"));
        return E_INVALIDARG;
    }

    if ((0 == iHeightRef) ||
        (0 == iHeightRef))
    {
        AppendResults(cCR, TEXT("Invalid Height\n"));
        return E_INVALIDARG;
    }

    if ((0 >= iWidthBytesRef) ||
        (0 >= iWidthBytesCur))
    {
        AppendResults(cCR, TEXT("Invalid Byte Width\n"));
        return E_INVALIDARG;
    }

    if(iHeightRef < 0 || iHeightCur < 0)
    {
        if((iHeightRef < 0 && iHeightCur > 0) ||
            (iHeightRef > 0 && iHeightCur < 0))
        {
            AppendResults(cCR, TEXT("If either image is top down, then both must be top down."));
            return E_INVALIDARG;
        }
        else
        {
            fTopDown = true;
            iHeightRef *= -1;
            iHeightCur *= -1;
        }
    }

    if (pRMSData)
    {
        uiThreshold = pRMSData->uiThreshold;
        uiAvgThreshold = pRMSData->uiAvgThreshold;
    }


    //need that in case size of the images are different
    iWidth = min(iWidthRef, iWidthCur);
    iHeight = min(iHeightRef, iHeightCur);

    memset(&bmi24, 0x00, sizeof(bmi24));

    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    bmi24.bmiHeader.biHeight = iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;

    // Creates a device indipendent bitmap
    hbmpDif = CreateDIBSection(NULL, (BITMAPINFO*)&bmi24, DIB_RGB_COLORS, (void**)&pbDifPixels, NULL, 0);
    if (!hbmpDif)
    {
        AppendResults(cCR, TEXT("CreateDIBSection failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_HANDLE;

        goto LPFinish;
    }

    if (!GetObject((HGDIOBJ) hbmpDif, sizeof(dibDif), (void*)&dibDif))
    {
        AppendResults(cCR, TEXT("GetObject failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_ABORT;

        goto LPFinish;
    }

    // Do pixel by pixel compare
    // Also compute a running RMS of the error.
    dfGlobalRMSRed = dfGlobalRMSGreen = dfGlobalRMSBlue = 0;
    for (iY = 0; iY < iHeight; iY++)
    {
        // All bitmaps should be 24 bpp
        // the (iY + iHeightRef - iHeight) allows us to compare images of different heights
        int iYOffsetRef = iWidthBytesRef * (iY + iHeightRef - iHeight);
        int iYOffsetTest = iWidthBytesCur * (iY + iHeightCur - iHeight);
        int iYOffsetDif = dibDif.dsBm.bmWidthBytes * iY;
        for (iX = 0; iX < iWidth; iX++)
        {
            // Get the RGB values for the test image
            siBlue  = bPtrBitsRef[iYOffsetRef + 3*iX + iBlueOffsetT];
            siGreen = bPtrBitsRef[iYOffsetRef + 3*iX + iGreenOffsetT];
            siRed   = bPtrBitsRef[iYOffsetRef + 3*iX + iRedOffsetT];

            // Subtract the reference image from the test image
            siBlue  -= bPtrBitsCur[iYOffsetTest + 3*iX + iBlueOffsetR];
            siGreen -= bPtrBitsCur[iYOffsetTest + 3*iX + iGreenOffsetR];
            siRed   -= bPtrBitsCur[iYOffsetTest + 3*iX + iRedOffsetR];

            if (   uiThreshold < (UINT) abs(siRed)
                || uiThreshold < (UINT) abs(siBlue)
                || uiThreshold < (UINT) abs(siGreen))
            {
                // iHeight - iY - 1 is because the DIBs are by default
                // bottom-up, and we need to convert to normal coordinates for
                // manual checking
                AppendResults(cCR, TEXT("Comparison found incorrect pixel at (%d, %d), ")
                    TEXT("expected RGB(%d, %d, %d), found RGB(%d, %d, %d)\n"),
                    iX,
                    fTopDown?iY:iHeight - iY - 1,
                    bPtrBitsRef[iYOffsetRef + 3*iX + iRedOffsetR],
                    bPtrBitsRef[iYOffsetRef + 3*iX + iGreenOffsetR],
                    bPtrBitsRef[iYOffsetRef + 3*iX + iBlueOffsetR],
                    bPtrBitsCur[iYOffsetTest + 3*iX + iRedOffsetT],
                    bPtrBitsCur[iYOffsetTest + 3*iX + iGreenOffsetT],
                    bPtrBitsCur[iYOffsetTest + 3*iX + iBlueOffsetT]);

                hr = E_FAIL;

                uiDiffCount++;

            }

            // Keep track of the sum of the squares for Global RMS Calculation
            dfGlobalRMSRed += siRed * siRed;
            dfGlobalRMSGreen += siGreen * siGreen;
            dfGlobalRMSBlue += siBlue * siBlue;

            // Normalize the values, since they might be negative.
            siRed += 128;
            siBlue += 128;
            siGreen += 128;

            pbDifPixels[iYOffsetDif + 3 * iX + iBlueOffsetT] = (BYTE)siBlue;
            pbDifPixels[iYOffsetDif + 3 * iX + iGreenOffsetT] = (BYTE)siGreen;
            pbDifPixels[iYOffsetDif + 3 * iX + iRedOffsetT] = (BYTE)siRed;
        }
    }

    // Compute the percentage of diff pixels and
    if (pRMSData)
        pRMSData->dfPercentageDiffPixels = (DOUBLE) uiDiffCount / (iWidth * iHeight) * 100 ;

    // Compute the mean of the squares
    dfGlobalRMSRed = dfGlobalRMSRed / (iWidth * iHeight);
    dfGlobalRMSGreen = dfGlobalRMSGreen / (iWidth * iHeight);
    dfGlobalRMSBlue = dfGlobalRMSBlue / (iWidth * iHeight);

    // Compute the root of the mean of the squares (RMS)
    dfGlobalRMSRed = sqrt(dfGlobalRMSRed);
    dfGlobalRMSGreen = sqrt(dfGlobalRMSGreen);
    dfGlobalRMSBlue = sqrt(dfGlobalRMSBlue);

    if (   dfGlobalRMSRed > uiAvgThreshold
        || dfGlobalRMSGreen > uiAvgThreshold
        || dfGlobalRMSBlue > uiAvgThreshold)
    {
        AppendResults(cCR, TEXT("Error RMS exceeded acceptable threshold. Error: (%lf, %lf, %lf), threshold: %d\n"),
                            dfGlobalRMSRed, dfGlobalRMSGreen, dfGlobalRMSBlue, uiAvgThreshold);
        hr = E_FAIL;
    }

    //Save the results in the structure passed
    if (pRMSData)
    {
        pRMSData->dResultRMSRed = dfGlobalRMSRed;
        pRMSData->dResultRMSGreen = dfGlobalRMSGreen;
        pRMSData->dResultRMSBlue = dfGlobalRMSBlue;
    }

    // only useful if there can be some error
    if (uiAvgThreshold)
    {
        // Compute the localized RMS of the error.
        // The idea behind this: with lossy compression (and dithering), the
        // test image should contain approximately the same color information over
        // large areas.
        // The methodology: I will look at the dif image in overlapping 4x4 squares. I
        // will start in upper left corner, compute the average of the 4x4 square up
        // in the corner, verify that it is close enough to 128, then move over by
        // 2 pixels and so forth.

        // This is totally a slow slow algorithm. The way-inner loops will be going
        // outside the cache like crazy.

        // half the width/height of the block to use (must be multiple of 2 or = 1)
        int iSize = 2;
        int iArea = 4 * iSize * iSize;
        // If one or both of the dimensions of the image are less than 4 pixels,
        // the area will be smaller.
        if ((int) iHeight < iSize * 2)
        {
            iArea /= iSize * 2;
            iArea *= iHeight;
        }
        if ((int) iWidth < iSize * 2)
        {
            iArea /= iSize * 2;
            iArea += iWidth;
        }
        UINT uiAverageRed;
        UINT uiAverageBlue;
        UINT uiAverageGreen;
        UINT uiTotAvgThreshold;
        int iErrorRed;
        int iErrorGreen;
        int iErrorBlue;
        int iXSum;
        int iYSum;
        int iTarget = iArea * 128;
        int iYOffset;
        int iXOffset;

        // Don't want to be doing any division in the inner loops
        uiTotAvgThreshold = uiAvgThreshold * uiAvgThreshold * iArea;
        for (iY = iSize; iY < iHeight-1; iY += iSize)
        {
            if (iY + iSize > iHeight)
                iY = iHeight - iSize - 1;
            int iYOffsetDif = dibDif.dsBm.bmWidthBytes * iY;

            for (iX = iSize; iX < iWidth-1; iX += iSize)
            {
                if (iX + iSize > iWidth)
                    iX = iWidth - iSize - 1;

                uiAverageRed = uiAverageBlue = uiAverageGreen = 0;

                for (iYSum = -iSize; iYSum < iSize; iYSum++)
                {
                    // bye bye L1 cache.
                    if (((int)iY) + iYSum < 0 || ((int)iY) + iYSum > (int)iY)
                        continue;
                    iYOffset = dibDif.dsBm.bmWidthBytes * (iY + iYSum);
                    for (iXSum = -iSize; iXSum < iSize; iXSum++)
                    {
                        if (((int)iX) + iXSum < 0 || ((int)iX) + iXSum > (int)iX)
                            continue;
                        iXOffset = 3 * (iX + iXSum);
                        iErrorRed   = pbDifPixels[iYOffset + iXOffset + iRedOffsetT] - 128;
                        iErrorGreen = pbDifPixels[iYOffset + iXOffset + iGreenOffsetT] - 128;
                        iErrorBlue  = pbDifPixels[iYOffset + iXOffset + iBlueOffsetT] - 128;
                        uiAverageBlue  += iErrorBlue * iErrorBlue;
                        uiAverageGreen += iErrorGreen * iErrorGreen;
                        uiAverageRed   += iErrorRed * iErrorRed;
                    }
                }

                if ((  uiTotAvgThreshold < uiAverageRed
                    || uiTotAvgThreshold < uiAverageGreen
                    || uiTotAvgThreshold < uiAverageBlue))
                {
                    AppendResults(cCR, TEXT("Localized Error RMS exceeded threshold (%d) at location (%d, %d): ")
                        TEXT("Red E-RMS (%lf), Green E-RMS (%lf), Blue E-RMS (%lf)\n"),
                        (int)sqrt((double) uiTotAvgThreshold / (double) iArea),
                        iX,
                        fTopDown?iY:iHeight - iY - 1,
                        sqrt(((double) uiAverageRed) / ((double) iArea)),
                        sqrt(((double) uiAverageGreen) / ((double) iArea)),
                        sqrt(((double) uiAverageBlue) / ((double) iArea)));

                    hr = E_FAIL;
                }
            }
        }
    }


LPFinish:

    DeleteObject((HGDIOBJ) hbmpDif);

    cCR->SetPassResult(SUCCEEDED(hr));

    return hr;

}

//
// GetFileDiff
//
//   The algorithm takes two bitmap filenames, produce a diff image
//   and saves it to a file
//
// Arguments:
//
//   const TCHAR *tszRefFileName: the filename of the reference image
//   const TCHAR *tszCurFileName: the filename of the image to compare
//   const TCHAR *tszDiffFileName: the file name of that file that will contain the diff image
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT GetFileDiff(const TCHAR *tszRefFileName,
                    const TCHAR *tszCurFileName,
                    const TCHAR *tszDiffFileName,
                    cCompareResults *cCR)
{
    HRESULT hr = E_FAIL;

    HBITMAP hBmpRef = NULL,
            hBmpCur = NULL,
            hBmpDiff = NULL;

    INT    iWidthRef,
            iHeightRef,
            iWidthBytesRef;

    INT    iWidthCur,
            iHeightCur,
            iWidthBytesCur;

    LPBYTE  lpbBitsRef = NULL,
            lpbBitsCur = NULL;

    //
    // Argument validation
    //
    if ((NULL == tszRefFileName) ||
        (NULL == tszCurFileName) ||
        (NULL == tszDiffFileName))
    {
        AppendResults(cCR, TEXT("Invalid filename\n"));
        return E_POINTER;
    }

    hBmpRef = GetFileRawData(tszRefFileName, (void**)&lpbBitsRef, &iWidthRef, &iHeightRef, &iWidthBytesRef, cCR);
    if (NULL == hBmpRef)
        goto LPFinish;

    hBmpCur = GetFileRawData(tszCurFileName, (void**)&lpbBitsCur, &iWidthCur, &iHeightCur, &iWidthBytesCur, cCR);
    if (NULL == hBmpCur)
        goto LPFinish;

    hr = ComputeDiff(lpbBitsRef, iWidthRef, iHeightRef, iWidthBytesRef,
                     lpbBitsCur, iWidthCur, iHeightCur, iWidthBytesCur,
                     cCR,
                     tszDiffFileName);
LPFinish:

    if (hBmpRef) DeleteObject(hBmpRef);
    if (hBmpCur) DeleteObject(hBmpCur);

    return hr;

}

//
// ComputeDiff
//
//   The algorithm take two input images (pointer, stride, extents) and produce a diff image
//   and saves it to a file
//
// Arguments:
//
//   BYTE *bPtrBitsRef: pointer to the bitmap data of the reference image
//   UINT iWidthRef: width of the reference image
//   UINT iHeightRef: height of the reference image
//   UINT iWidthBytesRef: width in bytes of the reference image
//
//   BYTE *bPtrBitsCur: pointer to the bitmap data of the image to compare
//   UINT bPtrBitsCur: width of the image to compare
//   UINT iWidthCur: height of the image to compare
//   UINT iWidthBytesCur: width in bytes of the image to compare
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//   const TCHAR *tszDiffFileName: the file name of that file that will contain the diff image
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT ComputeDiff(BYTE *bPtrBitsRef, INT iWidthRef, INT iHeightRef, INT iWidthBytesRef,
                    BYTE *bPtrBitsCur, INT iWidthCur, INT iHeightCur, INT iWidthBytesCur,
                    cCompareResults *cCR,
                    const TCHAR *tszDiffFileName
                    )
{
    short siRed,
          siBlue,
          siGreen;

    // 24 bpp bitmaps are stored in BGR format.
    const int iRedOffsetT    = 2;
    const int iGreenOffsetT = 1;
    const int iBlueOffsetT    = 0;

    // The reference can sometimes be an Imaging bitmap, and the imaging
    // library doesn't support 24bpp bgr
    int iRedOffsetR   = 2,
        iGreenOffsetR = 1,
        iBlueOffsetR  = 0;


    INT    iWidth = 0,
            iHeight = 0,
            iX = 0,
            iY = 0;
    UINT uiDiffCount = 0;

    BITMAPINFO bmi24;
    HBITMAP hbmpDif = NULL;
    BYTE * pbDifPixels = NULL;
    DIBSECTION dibDif;

    HRESULT hr = S_OK;

    //
    // Argument validation
    //
    if ((NULL == bPtrBitsRef) ||
        (NULL == bPtrBitsCur))
    {
        AppendResults(cCR, TEXT("Invalid Bitmap Pointer\n"));
        return E_POINTER;
    }

    if (NULL == tszDiffFileName)
    {
        AppendResults(cCR, TEXT("Invalid filename\n"));
        return E_POINTER;
    }

    if ((0 >= iWidthRef) ||
        (0 >= iWidthCur))
    {
        AppendResults(cCR, TEXT("Invalid Width\n"));
        return E_INVALIDARG;
    }

    if ((0 == iHeightRef) ||
        (0 == iHeightRef))
    {
        AppendResults(cCR, TEXT("Invalid Height\n"));
        return E_INVALIDARG;
    }

    if ((0 >= iWidthBytesRef) ||
        (0 >= iWidthBytesCur))
    {
        AppendResults(cCR, TEXT("Invalid Byte Width\n"));
        return E_INVALIDARG;
    }

    if(iHeightRef < 0 || iHeightCur < 0)
    {
        if((iHeightRef < 0 && iHeightCur > 0) ||
            (iHeightRef > 0 && iHeightCur < 0))
        {
            AppendResults(cCR, TEXT("If either image is top down, then both must be top down."));
            return E_INVALIDARG;
        }
        else
        {
            iHeightRef *= -1;
            iHeightCur *= -1;
        }
    }

    //need that in case size of the images are different
    iWidth = min(iWidthRef, iWidthCur);
    iHeight = min(iHeightRef, iHeightCur);

    memset(&bmi24, 0x00, sizeof(bmi24));

    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    bmi24.bmiHeader.biHeight = iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;

    // Creates a device indipendent bitmap
    hbmpDif = CreateDIBSection(NULL, (BITMAPINFO*)&bmi24, DIB_RGB_COLORS, (void**)&pbDifPixels, NULL, 0);
    if (!hbmpDif)
    {
        AppendResults(cCR, TEXT("CreateDIBSection failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_FAIL;

        goto LPFinish;
    }

    if (!GetObject((HGDIOBJ) hbmpDif, sizeof(dibDif), (void*)&dibDif))
    {
        AppendResults(cCR, TEXT("GetObject failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_FAIL;

        goto LPFinish;
    }

    for (iY = 0; iY < iHeight; iY++)
    {
        // All bitmaps should be 24 bpp
        // the (iY + iHeightRef - iHeight) allows us to compare images of different heights
        int iYOffsetRef = iWidthBytesRef * (iY + iHeightRef - iHeight);
        int iYOffsetTest = iWidthBytesCur * (iY + iHeightCur - iHeight);
        int iYOffsetDif = dibDif.dsBm.bmWidthBytes * iY;
        for (iX = 0; iX < iWidth; iX++)
        {
            // Get the RGB values for the test image
            siBlue  = bPtrBitsRef[iYOffsetRef + 3*iX + iBlueOffsetT];
            siGreen = bPtrBitsRef[iYOffsetRef + 3*iX + iGreenOffsetT];
            siRed   = bPtrBitsRef[iYOffsetRef + 3*iX + iRedOffsetT];

            // Subtract the reference image from the test image
            siBlue  -= bPtrBitsCur[iYOffsetTest + 3*iX + iBlueOffsetR];
            siGreen -= bPtrBitsCur[iYOffsetTest + 3*iX + iGreenOffsetR];
            siRed   -= bPtrBitsCur[iYOffsetTest + 3*iX + iRedOffsetR];

            // Normalize the values, since they might be negative.
            siRed = abs(siRed);
            siBlue = abs(siBlue);
            siGreen = abs(siGreen);

            pbDifPixels[iYOffsetDif + 3 * iX + iBlueOffsetT] = (BYTE)siBlue;
            pbDifPixels[iYOffsetDif + 3 * iX + iGreenOffsetT] = (BYTE)siGreen;
            pbDifPixels[iYOffsetDif + 3 * iX + iRedOffsetT] = (BYTE)siRed;
        }
    }

    // Save the diff bitmap to a file if so specified
    SaveBitmap(tszDiffFileName,
               pbDifPixels,
               iWidth,
               iHeight,
               dibDif.dsBm.bmWidthBytes,
               cCR);


LPFinish:

    if (cCR)
    {
        BOOL bResult = (S_OK == hr) ? TRUE : FALSE;
        cCR->SetPassResult(bResult);
    }

    DeleteObject((HGDIOBJ) hbmpDif);

    return hr;

}


//
// GetWndRawData
//
//   Takes an HWND as input and returns a pointer to the raw R8G8B8 bits, stride and extent
//
// Arguments:
//
//   HWND hWnd: handle to the window
//   LPVOID *ppvBits: pointer to a variable that receives a pointer to the location of
//                    the device-independent bitmap's bit values
//
//   PUINT piWidth: pointer to a variable that receives the image width
//   PUINT piHeight: pointer to a variable that receives the image height
//   PUINT piWidthBytes: pointer to a variable that receives the image stride
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT GetWndRawData(HWND hWnd, LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR)
{
    //gets the handle to the display device context
    //if hWnd is NULL it retrieves the device context for the entire screen
    HDC hDC = GetDC(hWnd);

    if (hDC)
        return GetDCRawData(hDC,
                            ppvBits,
                            piWidth,
                            piHeight,
                            piWidthBytes,
                            cCR);
    else
        return ERROR_INVALID_HANDLE;

}

//
// GetScreenRawData
//
//   Takes take a full-screen capture and returns a pointer to the raw R8G8B8 bits, stride and extent
//
// Arguments:
//
//   LPVOID *ppvBits: pointer to a variable that receives a pointer to the location of
//                    the device-independent bitmap's bit values
//
//   PUINT piWidth: pointer to a variable that receives the image width
//   PUINT piHeight: pointer to a variable that receives the image height
//   PUINT piWidthBytes: pointer to a variable that receives the image stride
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT GetScreenRawData(LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR)
{
    return GetWndRawData(NULL,
                         ppvBits,
                         piWidth,
                         piHeight,
                         piWidthBytes,
                         cCR);

}

//
// GetFileRawData
//
//   Take a bitmap filename as input and returns a pointer to the raw R8G8B8 bits, stride and extent
//
// Arguments:
//
//   LPVOID *ppvBits: pointer to a variable that receives a pointer to the location of
//                    the device-independent bitmap's bit values
//
//   PUINT piWidth: pointer to a variable that receives the image width
//   PUINT piHeight: pointer to a variable that receives the image height
//   PUINT piWidthBytes: pointer to a variable that receives the image stride
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HBITMAP GetFileRawData(const TCHAR * tszFileName, LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR)
{
    HBITMAP hBmp;
    DIBSECTION dibSection;

    //
    // Argument validation
    //
    if (NULL == tszFileName ||
        NULL == ppvBits ||
        NULL == piWidth ||
        NULL == piHeight ||
        NULL == piWidthBytes
        )
    {
        AppendResults(cCR, TEXT("Invalid pointer in one of the arguments\n"));
        return NULL;
    }

    //load the bmp associated with the specified file
    hBmp = LoadDIBitmap(tszFileName, cCR);
    if (NULL == hBmp)
        return NULL;

    if (!GetObject((HGDIOBJ) hBmp, sizeof(dibSection), (void*)&dibSection))
    {
        AppendResults(cCR, TEXT("GetObject failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        return NULL;

    }

   *piWidth = dibSection.dsBm.bmWidth;
   *piHeight = dibSection.dsBm.bmHeight;
   *piWidthBytes = dibSection.dsBm.bmWidthBytes;
   *ppvBits = dibSection.dsBm.bmBits;

   return hBmp;

}

//
// GetDCRawData
//
//   Takes an HDC as input and returns a pointer to the raw R8G8B8 bits, stride and extent
//
// Arguments:
//
//   HDC hDC: handle to the device context
//
//   LPVOID *ppvBits: pointer to a variable that receives a pointer to the location of
//                    the device-independent bitmap's bit values
//
//   PUINT piWidth: pointer to a variable that receives the image width
//   PUINT piHeight: pointer to a variable that receives the image height
//   PUINT piWidthBytes: pointer to a variable that receives the image stride
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT GetDCRawData(HDC hDC, LPVOID *ppvBits, PINT piWidth, PINT piHeight, PINT piWidthBytes, cCompareResults *cCR)
{
    HRESULT        hr = S_OK;
    LPBYTE        lpbBits = NULL;
    HBITMAP        hBmp;
    DIBSECTION  dibSection;
    RECT        rc;
    HDC            hCompDC = NULL;
    UINT        iWidth, iHeight;
    INT            iClipBoxRetVal;

    //
    // Argument validation
    //
    if (NULL == ppvBits ||
        NULL == piWidth ||
        NULL == piHeight ||
        NULL == piWidthBytes
        )
    {
        AppendResults(cCR, TEXT("Invalid pointer in one of the arguments\n"));
        return E_FAIL;
    }


    //get the dimensions of the tightest bounding rectangle that can be drawn around
    //the current visible area on the device context
    iClipBoxRetVal = GetClipBox(hDC, &rc);

    switch ( iClipBoxRetVal ) {

    case SIMPLEREGION:
        iWidth = rc.right - rc.left + 1;
        iHeight = rc.bottom - rc.top + 1;
        break;

    case COMPLEXREGION:
        AppendResults(cCR, TEXT("GetClipBox returned a COMPLEXREGION.\n"));
        return E_FAIL;

    case NULLREGION:
    default:
        AppendResults(cCR, TEXT("GetClipBox returned a NULLREGION\n"));
        return E_FAIL;
    }


    //creates a memory device context compatible with the device
    hCompDC = CreateCompatibleDC(hDC);
    if (NULL == hCompDC)
        return E_FAIL;

   // Create a 24-bit bitmap
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = iWidth;
    bmi.bmiHeader.biHeight      = iHeight;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Create the DIB Section
    hBmp = CreateDIBSection(hCompDC, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);

    //get the bitmap object
    if (!GetObject((HGDIOBJ) hBmp, sizeof(dibSection), (void*)&dibSection))
    {
        AppendResults(cCR, TEXT("GetObject failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_FAIL;

        goto LPFinish;
    }

    //select the bitmap object
    if (!SelectObject( hCompDC
        , (HGDIOBJ) hBmp))
    {
        AppendResults(cCR, TEXT("SelectObject failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));
        hr = E_FAIL;

        goto LPFinish;
    }

    //initializes the bitmap
    PatBlt(hCompDC, 0, 0, iWidth, iHeight, BLACKNESS);
    //copy the bitmap
    BitBlt(hCompDC, 0, 0, iWidth, iHeight, hDC, 0, 0, SRCCOPY);


    *piWidth = dibSection.dsBm.bmWidth;
    *piHeight = dibSection.dsBm.bmHeight;
    *piWidthBytes = dibSection.dsBm.bmWidthBytes;
    *ppvBits = dibSection.dsBm.bmBits;

LPFinish:

    DeleteObject((HGDIOBJ) hBmp);

    return hr;
}

//
// LoadDIBitmap
//
//   Load a bmp file from a specified file
//
// Arguments:
//
//   TCHAR *tszName: variable containing filename to load
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  a handle to the bitmap or NULL if unsuccesfull
//
HBITMAP LoadDIBitmap(const TCHAR * tszName, cCompareResults *cCR)
{
    HANDLE hFile = NULL;
    HBITMAP hBmp = NULL;
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmi;
    DWORD dwBytesRead;
    BYTE * pbPixelData = NULL;
    bool fSuccess = false;
    int iStride = 0;

    //
    // Argument validation
    //
    if (NULL == tszName)
    {
        AppendResults(cCR, TEXT("Invalid Filename Pointer\n"));
        return NULL;
    }

    // Opens the file
    hFile = CreateFile(tszName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (NULL == hFile)
    {
        goto LPError;
    }

    // Read the BITMAPFILEHEADER header
    if (!ReadFile(hFile, (void*)&bmfh, sizeof(bmfh), &dwBytesRead, NULL))
    {
        goto LPError;
    }

    // Read the BITMAPINFOHEADER header
    if (!ReadFile(hFile, (void*)&bmi, sizeof(bmi), &dwBytesRead, NULL))
    {
        goto LPError;
    }

    // Validate the headers
    if (S_OK != ValidateBitmapHeaders( &bmfh, &bmi, cCR))
    {
        goto LPError;
    }

    // Creates a device indipendent bitmap
    hBmp = CreateDIBSection(NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, (void**) &pbPixelData, NULL, 0);
    if (!hBmp)
    {
        goto LPError;
    }

    if (0xffffffff == SetFilePointer(hFile, bmfh.bfOffBits, NULL, FILE_BEGIN) && GetLastError() != NO_ERROR)
    {
        goto LPError;
    }

    // Calculate the stride
    iStride = ((bmi.biWidth * bmi.biBitCount >> 3) + 3) & ~3;
    if (!ReadFile(hFile, (void*) pbPixelData, bmi.biHeight * iStride, &dwBytesRead, NULL))
    {
        goto LPError;
    }


    if (hFile)
        CloseHandle(hFile);
    return hBmp;
LPError:
    if (!fSuccess && hBmp)
    {
        DeleteObject(hBmp);
        hBmp = NULL;
    }

    AppendResults(cCR, TEXT("LoadDIBitmap failed with error 0x%08X\n"), HRESULT_FROM_WIN32(GetLastError()));

    if (hFile)
        CloseHandle(hFile);
    return NULL;
}



//
// SaveBitmap
//
//   Save the bitmap to a specified file
//
// Arguments:
//
//   TCHAR *tszPath: variable containing filename for the bitmap
//   BYTE *pPixels: pointer to the bitmap data
//   UINT iWidth: width of the image
//   UINT iHeight: height of the image
//   UINT iWidthBytes: width in bytes of the image
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT SaveBitmap(const TCHAR * tszFileName, BYTE *pPixels, INT iWidth, INT iHeight, INT iWidthBytes, cCompareResults *cCR)
{
    DWORD dwBmpSize;
    BITMAPFILEHEADER bmfh;
    DWORD dwBytesWritten;
    BITMAPINFO bmi24;
    HANDLE hBmpFile = NULL;
    HRESULT hr;
    TCHAR tszBitmapCopy[MAX_PATH + 1];
    TCHAR * pBack;

    //
    // Argument validation
    //
    if (NULL == tszFileName)
    {
        AppendResults(cCR, TEXT("Invalid Filename Pointer\n"));
        return E_POINTER;
    }

    if (NULL == pPixels)
    {
        AppendResults(cCR, TEXT("Invalid Bitmap Pointer\n"));
        return E_POINTER;
    }

    if (0 >= iWidth)
    {
        AppendResults(cCR, TEXT("Invalid Width\n"));
        return E_INVALIDARG;
    }

    if (0 == iHeight)
    {
        AppendResults(cCR, TEXT("Invalid Height\n"));
        return E_INVALIDARG;
    }

    if (0 >= iWidthBytes)
    {
        AppendResults(cCR, TEXT("Invalid Byte Width\n"));
        return E_INVALIDARG;
    }

    // if iHeight is negative, then it's a top down dib.  Save it as it is.

    memset(&bmi24, 0x00, sizeof(bmi24));

    bmi24.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    bmi24.bmiHeader.biWidth = iWidth;
    bmi24.bmiHeader.biHeight = iHeight;
    bmi24.bmiHeader.biPlanes = 1;
    bmi24.bmiHeader.biBitCount = 24;
    bmi24.bmiHeader.biCompression = BI_RGB;
    bmi24.bmiHeader.biSizeImage = bmi24.bmiHeader.biXPelsPerMeter = bmi24.bmiHeader.biYPelsPerMeter = 0;
    bmi24.bmiHeader.biClrUsed = bmi24.bmiHeader.biClrImportant = 0;

    memset(&bmfh, 0x00, sizeof(BITMAPFILEHEADER));
    bmfh.bfType = 'M' << 8 | 'B';
    bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmi24);

    //created the directory to save the file if it doesn't exist
    wcsncpy(tszBitmapCopy, tszFileName, countof(tszBitmapCopy));
    tszBitmapCopy[countof(tszBitmapCopy) - 1] = 0;
    pBack = wcsrchr(tszBitmapCopy, TEXT('\\'));
    if (pBack)
    {
        *pBack = 0;
        RecursiveCreateDirectory(tszBitmapCopy);
        *pBack = TEXT('\\');
    }

    hBmpFile = CreateFile(
        tszFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (INVALID_HANDLE_VALUE == hBmpFile)
        goto LPError;

    bmi24.bmiHeader.biSizeImage = dwBmpSize = iWidthBytes * abs(iHeight);
    bmfh.bfSize = bmfh.bfOffBits + dwBmpSize;

    if (!WriteFile(
            hBmpFile,
            (void*)&bmfh,
            sizeof(bmfh),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)&bmi24,
            sizeof(bmi24),
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }
    if (!WriteFile(
            hBmpFile,
            (void*)pPixels,
            dwBmpSize,
            &dwBytesWritten,
            NULL))
    {
        goto LPError;
    }

    CloseHandle(hBmpFile);
    return S_OK;
LPError:
    hr = HRESULT_FROM_WIN32(GetLastError());

    AppendResults(cCR, TEXT("SaveBitmap failed with error 0x%08X\n"), hr);

    if (hBmpFile)
        CloseHandle(hBmpFile);
    return hr;
}


//
// ValidateBitmapHeaders
//
//   Examines the following bitmap-related structures:
//
//     * BITMAPFILEHEADER
//     * BITMAPINFOHEADER
//
//   If any of the members of these structures indicate a type of bitmap that is
//   not supported by this utility, failure is indicated in the return value for
//   this function.  This utility is only intended to handle a common format of BMP
//   that is simple to parse and also widely supported.  Restrictions:
//
//       * Must be 24 BPP
//       * Row width must not require any end-of-line padding (must naturally
//         end on a LONG boundery)
//       * Must be a bottom-up DIB
//       * Must be 1 plane only
//       * Must not be compressed (i.e., BI_RGB required)
//
// Arguments:
//
//   BITMAPFILEHEADER: File header for BMP
//   BITMAPINFOHEADER: Bitmap info header for BMP
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//
// Return Value:
//
//   HRESULT:  S_OK if success or the specific error code
//
HRESULT ValidateBitmapHeaders(CONST UNALIGNED BITMAPFILEHEADER *bmfh, CONST UNALIGNED BITMAPINFOHEADER *bmi, cCompareResults *cCR)
{
    //
    // Argument validation
    //
    if ((NULL == bmfh) ||
        (NULL == bmi))
    {
        AppendResults(cCR, TEXT("Invalid BITMAPFILEHEADER pointer\n"));
        return E_POINTER;
    }

    //
    // Structure validation, for BITMAPFILEHEADER.  No validation is performed
    // for the following fields:
    //
    //     * bfReserved1
    //     * bfReserved2
    //     * bfSize
    //
    if (bmfh->bfType != 0x4D42)
    {
        AppendResults(cCR, TEXT("Invalid bitmap signature\n"));
        return E_INVALIDARG;
    }

    // In SaveBitmap, line 1228, uses the sizeof(BITMAPINFOHEADER) + sizeof(BITMAPINFO)
    // this means any bitmap saved with SaveBitmap will not be able to be loaded with
    // LoadBitmap, due to a numerical mismatch in the header.
    //
    // OLD:
    // if (bmfh->bfOffBits != (sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER)))
    // NEW:
    if (bmfh->bfOffBits != (sizeof(BITMAPINFO) + sizeof(BITMAPFILEHEADER)))
    {
        AppendResults(cCR, TEXT("Unknown bitmap header structure size\n"));
        return E_INVALIDARG;
    }

    //
    // Structure validation, for BITMAPINFO.  No validation is needed
    // for the following fields:
    //
    //     * biWidth
    //     * biHeight
    //     * biSizeImage
    //     * biXPelsPerMeter
    //     * biYPelsPerMeter
    //     * biClrUsed
    //     * biClrImportant
    //
    if (sizeof(BITMAPINFOHEADER) != bmi->biSize)
    {
        AppendResults(cCR, TEXT("Unknown bitmap header structure size\n"));
        return E_INVALIDARG;
    }

    //
    // If the height of the bitmap is positive, the bitmap is a bottom-up DIB and its origin is the lower-left corner.
    // If the height is negative, the bitmap is a top-down DIB and its origin is the upper left corner.
    //
    // Only allow the common case, the bottom-up DIB.
    //
    if (bmi->biHeight < 0)
    {
        AppendResults(cCR, TEXT("Top-down DIBs are not supported.\n"));
        return E_INVALIDARG;
    }

    //
    // Enforce "1 plane" restriction
    //
    if (1 != bmi->biPlanes)
    {
        AppendResults(cCR, TEXT("Unsupported plane count.\n"));
        return ERROR_NOT_SUPPORTED;
    }

    //
    // Enforce "24 BPP" restriction
    //
    if (24 != bmi->biBitCount)
    {
        AppendResults(cCR, TEXT("Unsupported bit count.\n"));
        return ERROR_NOT_SUPPORTED;
    }

    //
    // Enforce "uncompressed" restriction
    //
    if (BI_RGB != bmi->biCompression)
    {
        AppendResults(cCR, TEXT("Unsupported compression type.\n"));
        return ERROR_NOT_SUPPORTED;
    }

    return S_OK;
}



//
// RecursiveCreateDirectory
//
//   Creates a directory for the path specified
//
// Arguments:
//
//   TCHAR *tszPath: variable containing the path we want to create
//
// Return Value:
//
//   TRUE if succesfull
//
BOOL RecursiveCreateDirectory(TCHAR * tszPath)
{
    if (!CreateDirectory(tszPath, NULL) &&
        (GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND))
    {
        TCHAR * pBack;
        pBack = wcsrchr(tszPath, TEXT('\\'));
        if (pBack)
        {
            *pBack = 0;
            RecursiveCreateDirectory(tszPath);
            *pBack = TEXT('\\');
        }
        else
            return false;
    }
    if (CreateDirectory(tszPath, NULL)
        || (GetLastError() == ERROR_FILE_EXISTS
            || GetLastError() == ERROR_ALREADY_EXISTS)
        )
    {
        return true;
    }
    return false;
}


//
// AppendResults
//
//   Appends a string to the log info.
//
//   Takes a variable argument list and build a string to pass AppendLogInfo
//   if pointer to the log info object is null, output info to the debug spew.
//
// Arguments:
//
//   cCompareResults *cCR: a pointer to cCompareResults object used to log the compare results (can be null)
//   TCHAR *szFormat, ...: variable argument list
//
// Return Value:
//
//   none
//
void AppendResults(cCompareResults *cCR, TCHAR *szFormat, ...)
{
    TCHAR szBuffer[1024];

    // if the buffer is full, skip composing the string as a perf tweak
    if(cCR && cCR->LogIsFull())
        return;

    va_list pArgs;
    va_start(pArgs, szFormat);
    wvsprintf(szBuffer, szFormat, pArgs);
    va_end(pArgs);

    //if pointer to the log info object is null, output info to the debug spew.
    if (cCR)
        cCR->AppendLogInfo(szBuffer);
    else
        OutputDebugString(szBuffer);
}

//
// ComparePixel
//
// A function that will compare two pixels of a given size and retun the pixels as Dwords
//
// Arguments:
//  pBitsOffsetRef - Pointer to Reference Pixel
//	pBitsOffsetCur - Pointer to Compared Pixel
//	wPixelByteCount - Number of Bytes per Pixel
//  out dwPixelDataRef - returned Pixel Data for Reference Pixel
//  out dwPixelDataCur - returned Pixel Data for Compared Pixel
//
// Return Value:
//  True if Same
bool ComparePixel(BYTE* pBitsOffsetRef, BYTE* pBitsOffsetCur, WORD wPixelByteCount, DWORD* pPixelDataRef, DWORD* pPixelDataCur)
{
	bool ReturnValue = true; 
	if (pPixelDataRef)
		*pPixelDataRef = 0;

	if (pPixelDataCur)
		*pPixelDataCur = 0;

	for (int i = 0 ; i < wPixelByteCount ; i++)
	{
		//Compare current byte
		if (*pBitsOffsetRef != *pBitsOffsetCur)
		{
			ReturnValue = false;
		}
		
		if (pPixelDataRef)
		{
			*pPixelDataRef = *pPixelDataRef << 8;
			*pPixelDataRef += *pBitsOffsetRef;
		}

		if (pPixelDataCur)
		{
			*pPixelDataCur = *pPixelDataCur << 8;
			*pPixelDataCur += *pBitsOffsetCur;
		}

		//Increment byte
		pBitsOffsetRef++;
		pBitsOffsetCur++;

	}

	
	return ReturnValue;
}