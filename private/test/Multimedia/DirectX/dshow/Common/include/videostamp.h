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

#ifndef VIDEOSTAMP_H
#define VIDEOSTAMP_H

#include <windows.h>
#include <dshow.h>
#include <streams.h>
#include <tchar.h>
#include "ocr.h"
#include "mmimaging.h"

#define DESCRIPTION_DURATION 30
#define DESCRIPTION_MAX_LENGTH 255

#define WRITEOFFSETFROMEDGE 3
#define READOFFSETFROMEDGE 2

#define FRAME_ID_WIDTH 121
#define FRAME_ID_HEIGHT FONTHEIGHT + 2
#define FRAME_ID_CHARACTERCOUNT 10

#define FRAME_PTS_HEIGHT FONTHEIGHT + 4
#define FRAME_PTS_WIDTH 228
#define FRAME_PTS_CHARACTERCOUNT 19

// minimum width and height is the PTS width
#define MIN_SAMPLE_WIDTH FRAME_ID_WIDTH + (2*WRITEOFFSETFROMEDGE)
#define MIN_SAMPLE_HEIGHT 120

#define FRAME_DESCRIPTION_WIDTH_PORTION 6.
#define FRAME_DESCRIPTION_HEIGHT_PORTION 4.
#define FRAME_DESCRIPTION_DIVIDER 8.

extern TCHAR *tszDefaultOCRTemplate;

HRESULT GetDefaultFrameIDRect(RECT *rcFrameID);
HRESULT GetDefaultTimestampRect(RECT *rcTimestamp);
HRESULT GetMaxWidthNumber(HDC hdc, TCHAR *tszChar);
HRESULT GetFrameIDRect(RECT *rcFrameID, const LOGFONT *lfFont, float Scaling);
HRESULT GetTimestampRect(RECT *rcTimestamp, const LOGFONT *lfFont, float Scaling);

typedef class CVideoStampWriter
{
    private:
    CRITICAL_SECTION m_Lock;

    AM_MEDIA_TYPE m_SampleMediaType;
    VIDEOINFOHEADER *m_pvih;
    int m_nSampleStride;

    TCHAR m_tszClipDescription[DESCRIPTION_MAX_LENGTH];
    int m_nSamplesTaggedWithDescription;
    int m_nCurrentFrameID;

    HDC m_hdcWorkSurface;
    HBITMAP m_hbmpWorkSurface;
    BITMAPINFO m_bmiWorkSurface;
    BYTE *m_pWorkSurfaceBits;

    BYTE *m_pBitmapBits;
    int m_nBitmapWidth;
    int m_nBitmapHeight;
    int m_nBitmapStride;
    int m_nBitmapx;
    int m_nBitmapy;
    GUID m_BitmapMediaSubtype;
    BITMAPINFO m_bmiBitmap;
    int m_BitmapDisplayFrameCount;
    DWORD m_dwRotation;

    LOGFONT m_lfOCRFont;
    RECT m_rcFrameID;
    RECT m_rcTimestamp;

    public:

    CVideoStampWriter();
    ~CVideoStampWriter();

    // gets ready to stamp media samples. We need the description the user wants on the image,
    // and the media type we're going to be processing.
    HRESULT Init(const AM_MEDIA_TYPE *pMediaType, DWORD Rotation = 0);
    HRESULT InitFromLogfont(const AM_MEDIA_TYPE *pMediaType, LOGFONT *lfFont = NULL, DWORD Rotation = 0);

    // create the template file using the OCR font.
    HRESULT CreateTemplate(TCHAR *tszOCRTemplate);
    HRESULT CreateTemplateFromLogfont(TCHAR *tszOCRTemplate, LOGFONT *lfFont, RECT *prcFrameID, RECT *prcTimestamp);

    // reset everything back to the base state..
    HRESULT Cleanup();

    // set a user requested bitmap to put into the image. it's displayed for nDisplayFrameCount media samples,
    // if the display frame count is -1 then it's put into all frames. Setting a display frame count to 0
    // removes the previous bitmap to display. The bitmap will be converted every time it's used so it can
    // be set once and used multiple times even though the image copied is changed.
    HRESULT SetBitmap(BYTE *pBits, int nWidth, int nHeight, int nStride, int X, int Y, GUID MediaSubtype, BITMAPINFO *pbmi, int nDisplayFrameCount);

    HRESULT SetDescription(TCHAR *tszDescription);

    // Is the user bitmap we're about to set supported to convert to the current media type?
    HRESULT IsSupportedBitmapFormat(const GUID MediaSubType);

    // is a format we're about to negotiate to a supported format to convert to and from?
    HRESULT IsSupportedSampleFormat(const AM_MEDIA_TYPE *pMediaType);

    // the real worker function. Given an IMediaSample it puts all of the data in the right places
    HRESULT ProcessMediaSample(IMediaSample *pMediaSample);
    HRESULT WriteStamp(BYTE *pBits, REFERENCE_TIME *PresentationTime);
    HRESULT WriteStamp(BYTE *pBits, REFERENCE_TIME *PresentationTime, int nFrameID);
    HRESULT SetFormat(const AM_MEDIA_TYPE *pMediaType);

    int GetFrameID();
} VideoStampWriter, *pVideoStampWriter;

typedef class CVideoStampReader
{
    private:
    CRITICAL_SECTION m_Lock;

    // the incoming media samples
    AM_MEDIA_TYPE m_SampleMediaType;
    VIDEOINFOHEADER *m_pvih;
    int m_nSampleStride;

    // our converted copy of the media samples
    BYTE *m_pSampleBits;
    int m_nSavedSampleStride;
    BITMAPINFO m_SavedSampleBMI;

    TCHAR m_tszClipDescription[DESCRIPTION_MAX_LENGTH];

    // information about the current sample(s) being processed
    int m_nDiscontinuityCount;
    int m_nPreviousSampleFrameID;
    REFERENCE_TIME m_PresentationTime;

    // OCR setup/data
    DShowOCR m_dsOCR;
    LOGFONT m_lfOCRFont;
    float m_fScaling;
    DWORD m_dwRotation;

    // location of timestamp data
    RECT m_rcFrameID;
    RECT m_rcTimestamp;

    public:

    CVideoStampReader();
    ~CVideoStampReader();

    // Setup the reader to process samples with this media type, scaling, and orientation. Also, the the only time you need to override the frame 
    // and timestamp rectangles is if you're using a non-standard template file. So, we only allow the override in that case.
    HRESULT Init(const AM_MEDIA_TYPE *pMediaType, TCHAR *tszOCRTemplate = tszDefaultOCRTemplate, float Scaling = 1.0, DWORD Rotation = 0, RECT *prcFrameID = NULL, RECT *prcTimestamp = NULL);
    HRESULT InitFromLogfont(const AM_MEDIA_TYPE *pMediaType, LOGFONT *lfTemplate = NULL, float Scaling = 1.0, DWORD Rotation = 0);

    // reset everything
    HRESULT Cleanup();

    // is a format we're about to negotiate to a supported format to convert to and from?
    HRESULT IsSupportedSampleFormat(const AM_MEDIA_TYPE *pMediaType);

    // get the presentation time from the OCR data in the media sample
    HRESULT GetPresentationTime(REFERENCE_TIME *pMediaTime);

    // Get the frame id of the sample just processed
    HRESULT GetFrameID(int *nFrameID);

    // Get the description of the clip, this needs to be called after the first
    // sample has been processed.
    HRESULT GetDescription(TCHAR *tszDescription, UINT nLength);

    // retrieve a copy of a piece of data in the sample
    HRESULT GetBitmap(BYTE *pBits, BITMAPINFO *pbmi, int stride, int X, int Y, int width, int height, GUID MediaSubtype);

    // Get the number of discontinuous frames read
    int GetDiscontinuityCount(){return m_nDiscontinuityCount;}

    // reset the frame tracking data
    HRESULT Reset();

    // indicate whether or not the media samples have been continuous
    HRESULT IsContinuous(BOOL *bContinuous);

    HRESULT ProcessMediaSample(IMediaSample *pMediaSample);
    HRESULT ReadStamp(BYTE *pBits);
    HRESULT SetFormat(const AM_MEDIA_TYPE *pMediaType);

} VideoStampReader, *pVideoStampReader;

#endif

