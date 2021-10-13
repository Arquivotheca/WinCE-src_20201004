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
// fsclsid.h

// {0FD4BD29-B3C7-45f6-8668-E1FC78740C99}
DEFINE_GUID(CLSID_IVideoFrameStepPID,
0xfd4bd29, 0xb3c7, 0x45f6, 0x86, 0x68, 0xe1, 0xfc, 0x78, 0x74, 0xc, 0x99);

class CVideoFrameStepPID : public CUnknown, public IDistributorNotify, public IVideoFrameStep
{
public:
    CVideoFrameStepPID(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr);
    ~CVideoFrameStepPID();
    
    DECLARE_IUNKNOWN

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    // ==========================
    // IDistributorNotify methods
    // ==========================
    // Called when the set of filters in the filter graph change or their connections change.
    STDMETHODIMP NotifyGraphChange();
    // Called when a new clock is registered.
    STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
    // Called when the filter graph is entering a paused state.
    STDMETHODIMP Pause();
    // Called when the filter graph is entering a running state.
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    // Called when the filter graph is entering a stopped state.
    STDMETHODIMP Stop();

    // =======================
    // IVideoFrameStep methods
    // =======================
    // Causes the filter graph to step forward by the specified number of frames.
    STDMETHODIMP Step(DWORD dwFrames, IUnknown *pStepObject);
    // Determines the stepping capabilities of the specified filter.
    STDMETHODIMP CanStep(long lMultiple, IUnknown *pStepObject);
    // Cancels the previous step operation.
    STDMETHODIMP CancelStep();

private:
    HRESULT RefreshInterfaces();

private:
    BOOL                m_bDirty;
    IKsPropertySet      *m_pKsFrameSet;
    IUnknown            *m_pUnk;
};



