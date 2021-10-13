//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "gse2.h"

// GSE Gesture Touch registry keys
#define REGGSE_GESTURE_HIVE                     HKEY_LOCAL_MACHINE
#define REGGSE_GESTURE_KEY                      TEXT("SOFTWARE\\Microsoft\\Test\\GSE2\\Touch")
#define REGGSE_GESTURE_VALUE_VISUALCUE          TEXT("VisualCue")
#define REGGSE_GESTURE_VALUE_SAMPLINGRATE       TEXT("SamplingRate")
#define REGGSE_GESTURE_VISUALCUE                3

class GSEHelper
{
public:
    GSEHelper();
    ~GSEHelper();

    inline HRESULT DoGesture(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData);
    inline HRESULT DoGestureWithCallback(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData, ONSENDCALLBACK fOnSendCallback);
    inline HRESULT DoFreeFormGesture(BYTE nGestureDataCount, GestureDataMultiPoint* pGestureData, BYTE bInterpolationMethod);
    inline HRESULT GestureChainStart();
    inline HRESULT GestureChainAddDelay(UINT nDelay);
    inline void GestureChainEnd(BOOL bAsync);
    inline HRESULT SendRandomGesture();

    inline HRESULT FreeformSessionInitialize();
    inline HRESULT FreeformSessionSend();
    inline HRESULT FreeformSessionClose();
    inline HRESULT FreeformStreamCreate(DWORD *pdwStreamID, int xPos, int yPos);
    inline HRESULT FreeformStreamAddSegment(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos);
    inline HRESULT FreeformStreamAddGap(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos);    
    inline HRESULT FreeformSessionSendWithCallback(ONSENDCALLBACK fOnSendCallback);

    static void EnableVisualCue();
    static void DisableVisualCue();

    // Obtains the minimum sampling interval between two consecutive samples, given
    // the current sampling rate. For 50 samples per second, this evalueates to 20ms
    static DWORD GetMinSampleIntervalMs();

    BOOL m_bWasGSELoaded;

private:
    HMODULE m_hGSE;

    BOOL InitGSE();
    void DeInitGSE();

    DOGESTUREPROC                   m_fDoGesture;
    DOGESTUREPROCWITHCALLBACK       m_fDoGestureWithCallback;
    DOFREEFORMGESTUREPROC           m_fDoFreeFormGesture;
    GESTURECHAINSTARTPROC           m_fGestureChainStart;
    GESTURECHAINADDDELAYPROC        m_fGestureChainAddDelay;
    GESTURECHAINENDPROC             m_fGestureChainEnd;
    SENDRANDOMGESTUREPROC           m_fSendRandomGesture;
    FREEFORMSESSIONINITIALIZE       m_fFreeformSessionInitialize;
    FREEFORMSESSIONSEND             m_fFreeformSessionSend;
    FREEFORMSESSIONCLOSE            m_fFreeformSessionClose;
    FREEFORMSTREAMCREATE            m_fFreeformStreamCreate;
    FREEFORMSTREAMADDSEGMENT        m_fFreeformStreamAddSegment;
    FREEFORMSTREAMADDGAP            m_fFreeformStreamAddGap;
    FREEFORMSESSIONSENDWITHCALLBACK m_fFreeformSessionSendWithCallback;

    static void SetVisualCue(DWORD dwValue);
};


inline HRESULT GSEHelper::DoGesture(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData)
{
    return (m_fDoGesture)(gestureType, nGestureDataCount, pGestureData);
}

inline HRESULT GSEHelper::DoGestureWithCallback(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData, ONSENDCALLBACK fOnSendCallback)
{
    return (m_fDoGestureWithCallback)(gestureType, nGestureDataCount, pGestureData, fOnSendCallback);
}

inline HRESULT GSEHelper::DoFreeFormGesture(BYTE nGestureDataCount, GestureDataMultiPoint* pGestureData, BYTE bInterpolationMethod)
{
    return (m_fDoFreeFormGesture)(nGestureDataCount, pGestureData, bInterpolationMethod);
}

inline HRESULT GSEHelper::GestureChainStart()
{
    return (m_fGestureChainStart)();
}

inline HRESULT GSEHelper::GestureChainAddDelay(UINT nDelay)
{
    return (m_fGestureChainAddDelay)(nDelay);
}

inline void GSEHelper::GestureChainEnd(BOOL bAsync)
{
    (m_fGestureChainEnd)(bAsync);
}

// Starts a new freeform session. This session must eventually be closed with a call to FreeformSessionClose().
// Once a session has been initialized, all subsequent FreeformXXXX calls must be made from the same thread.
// If FreeformSessionInitialize is called twice without a FreeformSessionClose() in between, the earlier session
// will be closed and any subsequent calls made in-session will fail. The later session will be initialized correctly.
inline HRESULT GSEHelper::FreeformSessionInitialize()
{
    return (m_fFreeformSessionInitialize)();
}

// Generates samples and sends them to the appropriate driver. Samples will represent the stream defined
// with previous calls to FreeformStreamAddSegment() and FreeformStreamAddGap()
inline HRESULT GSEHelper::FreeformSessionSend()
{
    return (m_fFreeformSessionSend)();
}

// Generates samples and sends them to the appropriate driver. Samples will represent the stream defined
// with previous calls to FreeformStreamAddSegment() and FreeformStreamAddGap()
// A function will be called before each sample is sent.
inline HRESULT GSEHelper::FreeformSessionSendWithCallback(ONSENDCALLBACK fOnSendCallback)
{
    return (m_fFreeformSessionSendWithCallback)(fOnSendCallback);
}


// Closes an open FreeformSession and frees up any resources held.
inline HRESULT GSEHelper::FreeformSessionClose()
{
    return (m_fFreeformSessionClose)();
}

// Creates a new stream in the current session. A unique streamID will be copied into *pwdStreamID. This
// should be used for subsequent calls to FreeformStreamAddSegment and FreeformStreamAddGap.
// The first point in the stream will be at xPos, yPos
inline HRESULT GSEHelper::FreeformStreamCreate(DWORD *pdwStreamID, int xPos, int yPos)
{
    return (m_fFreeformStreamCreate)(pdwStreamID, xPos, yPos);
}

// Adds a linear sequence of points to a touch stream. The streamID must be one returned by FreeformStreamCreate in the
// current session. The sequence will last dwDurationMs milliseconds, which is approximately
// (dwDurationMs/1000)/(number of samples per second). If dwDurationMs is too small to generate a sample given the
// current sample rate, it will be increased to 1000/sampleRate.
// The position xPos, yPos defines the last point in this sequence. Points will be linerarly interpolated between
// the last point in the previous sequence and the xPos, yPos parameters.
// Regardless of the timings there will always be at least one sample generated at exactly xPos, yPos.
inline HRESULT GSEHelper::FreeformStreamAddSegment(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos)
{
    return (m_fFreeformStreamAddSegment)(dwStreamID, dwDurationMs, xPos, yPos);
}

// Inserts a gap into the touch stream. The streamID must be one returned by FreeformStreamCreate in the
// current session. The gap can be one of two types:
//    a time gap, if dwDurationMs is non-zero or
//    a spatial gap, if dwDurationMs is zero.
// A time gap simulates interrupted contact with the touch screen for dwDurationMs milliseconds, with contact being
// reinstated at xPos, yPos. This generates appropriate UP and DOWN samples on the stream.
// A spatial gap simulates a sudden shift in touch co-ordinates that could be produced by driver thread starvation. No
// UP and DOWN samples are generated in this case. The stream instantly shifts to xPos, yPos.
// If dwDurationMs is non-zero but too small to generate a sample given the current sample rate,
// it will be increased to 1000/sampleRate.
// Please note that if there is a point in time where all streams in a session are inside a gap the system will interpret
// this as the end of a session - the next valid sample will be the start of a new one.
inline HRESULT GSEHelper::FreeformStreamAddGap(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos)
{
    return (m_fFreeformStreamAddGap)(dwStreamID, dwDurationMs, xPos, yPos);
}