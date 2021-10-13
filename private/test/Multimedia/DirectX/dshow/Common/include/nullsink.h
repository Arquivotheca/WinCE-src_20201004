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

#ifndef NULLSINK_H
#define NULLSINK_H
#include "videostamp.h"

typedef enum {
    CONNECTIONTYPE_NONE   = 0x0,
    CONNECTIONTYPE_RGB32  = 0x1,
    CONNECTIONTYPE_YV12   = 0x2,
    CONNECTIONTYPE_RGB565 = 0x4,
    CONNECTIONTYPE_NV12   = 0x8,
    CONNECTIONTYPE_YUY2   = 0x10,
    CONNECTIONTYPE_UYVY   = 0x12,
    CONNECTIONTYPE_ALL    = 0x1f
} ConnectionType;

class CNullSink;
class CNullInputPin;

//
// "Null" allocator is implemented to support rotated buffers
//
class CNullAllocator : public CMemAllocator
{
    CNullSink *m_pNullSink;     // Main filter
    CNullInputPin *m_pNullPin;  // Main filter's pin
    CCritSec *m_pLock;          // The receive lock

public:
    CNullAllocator(CNullSink *pFilter,         // Main null sink object
                   CNullInputPin *pPin,        // Main filter's pin
                   CCritSec *pLock,
                   HRESULT *phr);              // Constructor return code
    ~CNullAllocator();

    // Overriden to delegate reference counts to the filter
    STDMETHODIMP_(ULONG) NonDelegatingAddRef();
    STDMETHODIMP_(ULONG) NonDelegatingRelease();

    STDMETHODIMP SetProperties(ALLOCATOR_PROPERTIES* pRequest,
                               ALLOCATOR_PROPERTIES* pActual);
    STDMETHODIMP GetBuffer(IMediaSample **ppSample,
                           REFERENCE_TIME *pStartTime,
                           REFERENCE_TIME *pEndTime,
                           DWORD dwFlags);
};


//
// "Null" input pin is implemented to support rotated buffers
//
class CNullInputPin : public CRendererInputPin
{
public:

    // Constructor
    CNullInputPin(CNullSink *pNullSink,       // Used to delegate locking
        HRESULT *phr,                         // OLE failure return code
        LPCWSTR pPinName);                    // This pins identification
    ~CNullInputPin();

    // Manage our null allocator
    STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator);
    STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator,BOOL bReadOnly);

    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT SetScaling(float fRatio);
    HRESULT SetMaxScaling(float fRatio);
    HRESULT SetRotation(AM_ROTATION_ANGLE rotationAngle);
    float   GetMaxScaling() { return m_fMaxRatio; }
    float   GetScaling() { return m_fRatio; }
    AM_ROTATION_ANGLE GetRotation() { return m_RotationAngle; }
    LONG    GetXPitch() { return m_lXPitch; }
    LONG    GetYPitch() { return m_lYPitch; }
    DWORD   GetWidth()  { return m_dwWidth; }
    DWORD   GetHeight()  { return m_dwHeight; }
    void    SetVSRRotation(DWORD rotationAngle) {VSRRotation = rotationAngle;}

    HRESULT OnGetBuffer(IMediaSample *pSample,
                        REFERENCE_TIME *pStartTime,
                        REFERENCE_TIME *pEndTime,
                        DWORD dwFlags);

private:
    BOOL IsUpstreamScalingAvailable(ULONG xRatio, ULONG yRatio);

    CNullSink *m_pNullSink;                                // The null sink that owns us

    // scaling/rotation functionality
    float           m_fRatio;
    float           m_fMaxRatio;
    int             m_NativeVideoWidth;
    int             m_NativeVideoHeight;
    CMediaType      m_mtNew;
    BOOL            m_bDynamicFormatNeeded;
    DWORD           m_dwRotationCaps;
    DWORD           m_dwScalingCaps;
    DWORD           m_dwNScalingRatios;
    AMScalingRatio *m_pScalingRatios;                      // Scaling ratios of upstream filter
    BYTE           *m_pOrigSamplePointer;
    AM_ROTATION_ANGLE   m_RotationAngle;
    LONG            m_lXPitch;
    LONG            m_lYPitch;
    DWORD           m_dwWidth;
    DWORD           m_dwHeight;
    IAMVideoTransformComponent *m_pUpstreamTransform;      // Upstream filter interface to set different rotation angles and buffer parameters
    DWORD           VSRRotation;
};

MIDL_INTERFACE("3A17606B-5CC4-4341-9BE7-1C1741814996")
INullSink : public IUnknown
{
    public:
    virtual void SetVSRScaling(float fRatio);
    virtual void SetVSRRotation(DWORD rotationAngle);
    virtual void SetVSRRect(RECT stampRect);
    virtual void SetVSRTemplate(TCHAR *tszTemplate);
    virtual HRESULT SetScaling(float fRatio);
    virtual HRESULT SetMaxScaling(float fRatio);
    virtual HRESULT SetRotation(AM_ROTATION_ANGLE rotationAngle);
    virtual HRESULT SetMediaTypes(DWORD dwConnectionTypeMask);
    virtual bool GetSamplePassed();
    virtual HRESULT IsContinuous(BOOL *bContinuous);
    virtual int GetDiscontinuityCount();

};

class CNullSink : public CBaseRenderer, INullSink
{
public:
    DECLARE_IUNKNOWN;

    CNullSink(LPUNKNOWN pUnk, HRESULT *phr);
    ~CNullSink();
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // CBaseRenderer overrides
    HRESULT SetMediaType(const CMediaType *pMediaType);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT Receive(IMediaSample *pSample);
    HRESULT DoRenderSample(IMediaSample *pMediaSample);
    CBasePin *GetPin(int n);

// INullSink implementation
#ifdef UNDER_CE
    virtual LPAMOVIESETUP_FILTER GetSetupData();
#endif

    // CNullSink methods
    HRESULT SetMediaTypes(DWORD dwConnectionTypeMask);
    ConnectionType GetConnectionType() { return m_ConnectionType; }
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    HRESULT SetScaling(float fRatio);
    HRESULT SetMaxScaling(float fRatio);
    HRESULT SetRotation(AM_ROTATION_ANGLE rotationAngle);
    HRESULT IsContinuous(BOOL *bContinuous);
    int GetDiscontinuityCount();
    void SetVSRScaling(float fRatio) {VSRScale = fRatio;}
    void SetVSRRotation(DWORD rotationAngle) {VSRRotation = rotationAngle; m_InputPin.SetVSRRotation(rotationAngle);}
    void SetVSRRect(RECT stampRect) {VSRRect = stampRect;}
    void SetVSRTemplate(TCHAR *tszTemplate) {_tcsncpy(m_tszTemplateBitmap, tszTemplate, MAX_PATH);}
    float GetMaxScaling() { return m_InputPin.GetMaxScaling(); }
    float GetScaling() { return m_InputPin.GetScaling(); }
    AM_ROTATION_ANGLE GetRotation() { return m_InputPin.GetRotation(); }
    bool GetSamplePassed() { return bSamplePassed; }
    float VSRScale;
    DWORD VSRRotation;
    RECT VSRRect;
    TCHAR m_tszTemplateBitmap[MAX_PATH];
	CVideoStampReader VideoStampReader;

private:
    // helpers
    ConnectionType FourCCToConnectionType(BITMAPINFOHEADER *pBMI, WCHAR *pFourCCString, int cchLength);
    BOOL IsUpstreamScalingAvailable(ULONG xRatio, ULONG yRatio);
    HRESULT DumpSample(IMediaSample *pSample, WCHAR *sFilename);

    DWORD m_dwNumFrames;

    DWORD          m_dwConnectionTypeMask;  // Mask of connections which are allowed
    ConnectionType m_ConnectionType;        // Which connection type we are using
    WCHAR          m_FourCCString[100];
    CMediaType     m_mt;
    bool           bSamplePassed;
    AM_ROTATION_ANGLE   m_RotationAngle;

    
    CNullInputPin  m_InputPin;              // IPin based interfaces

public:
    CNullAllocator m_Allocator;             // Our allocator
};

#endif  // NULLSINK_H