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
// framestep.cpp

#include <streams.h>
#ifdef FILTER_DLL
#include <initguid.h>
#endif FILTER_DLL
#include "fsclsid.h"


#ifdef FILTER_DLL
CFactoryTemplate g_Templates[1] = {
    { L"Frame Step PID"
    , &CLSID_IVideoFrameStepPID
    , CVideoFrameStepPID::CreateInstance
    , NULL
    , NULL }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


/*
;e46a9787-2b71-444d-a4b5-1fab7b708d6a    IVideoFrameStep
[HKEY_CLASSES_ROOT\Interface\{e46a9787-2b71-444d-a4b5-1fab7b708d6a}\Distributor]
@="{0FD4BD29-B3C7-45f6-8668-E1FC78740C99}"
*/
HRESULT RegisterDShowPIDInterface(REFCLSID rclsid, REFIID riid)
{
    HRESULT     hr;
    LONG        lRet;
    HKEY        hKey;
    LPOLESTR    lpClSID = NULL;
    LPOLESTR    lpIID = NULL;
    WCHAR       achTemp[128];

    hr = StringFromCLSID(rclsid, &lpClSID);
    if (SUCCEEDED(hr))
    {
        hr = StringFromIID(riid, &lpIID);
    }
    if (SUCCEEDED(hr))
    {
        swprintf(achTemp, L"Interface\\%s\\Distributor", lpIID);
        lRet = RegCreateKey(HKEY_CLASSES_ROOT, (LPWSTR) achTemp, &hKey);
        if (lRet == ERROR_SUCCESS)
        {
            // got our key, now write the value
            swprintf(achTemp, L"%s", lpClSID);
            lRet = RegSetValueEx(hKey, NULL, 0, REG_SZ, (CONST BYTE *)achTemp, sizeof(WCHAR) * (wcslen(achTemp)+1));
            RegCloseKey(hKey);
        }
        if (lRet != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(lRet);
        }
    }

    if (lpClSID)
    {
        CoTaskMemFree(lpClSID);
    }
    if (lpIID)
    {
        CoTaskMemFree(lpIID);
    }

    return hr;
}

HRESULT UnregisterDShowPIDInterface(REFCLSID rclsid, REFIID riid)
{
    return S_OK;
}


// DllRegisterServer
STDAPI DllRegisterServer()
{
    HRESULT hr = AMovieDllRegisterServer2(TRUE);

    if (SUCCEEDED(hr))
    {
        /// register as a dshow PID
        hr = RegisterDShowPIDInterface(CLSID_IVideoFrameStepPID, IID_IVideoFrameStep);
        if (FAILED(hr))
        {
            AMovieDllRegisterServer2(FALSE);
        }
    }
    return hr;
}


// DllUnregisterServer
STDAPI DllUnregisterServer()
{
    HRESULT hr = AMovieDllRegisterServer2(FALSE);

    if (SUCCEEDED(hr))
    {
        // remove our dshow PID registration
        hr = UnregisterDShowPIDInterface(CLSID_IVideoFrameStepPID, IID_IVideoFrameStep);
    }

    return hr;
}
#endif // FILTER_DLL


// CreateInstance: Creator function for the class ID
CUnknown * WINAPI CVideoFrameStepPID::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    return new CVideoFrameStepPID(NAME("Frame Step PID"), pUnk, phr);
}

// Constructor
CVideoFrameStepPID::CVideoFrameStepPID(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr) :
    CUnknown(NAME("Frame Step PID"), pUnk),
    m_pKsFrameSet(NULL),
    m_bDirty(TRUE),
    m_pUnk(pUnk)
{
    ASSERT(phr);
}

// Destructor
CVideoFrameStepPID::~CVideoFrameStepPID()
{
    if (m_pKsFrameSet)
    {
        // we didn't AddRef this so we don't need to release it
    }
}


STDMETHODIMP CVideoFrameStepPID::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);
    ASSERT(ppv);
    *ppv = NULL;
    HRESULT hr = S_OK;

    // See what interface the caller is interested in.
    if (riid == IID_IVideoFrameStep)
    {
        return GetInterface((IVideoFrameStep *) this, ppv);
    }
    else if (riid == IID_IDistributorNotify)
    {
        return GetInterface((IDistributorNotify *) this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}


STDMETHODIMP CVideoFrameStepPID::NotifyGraphChange()
{
    // something in the graph changed so we mark any held interface pointers as dirty
    m_bDirty = TRUE;

    return S_OK;
}

STDMETHODIMP CVideoFrameStepPID::SetSyncSource(IReferenceClock *pClock)
{
    return S_OK;
}

STDMETHODIMP CVideoFrameStepPID::Pause()
{
    return S_OK;
}

STDMETHODIMP CVideoFrameStepPID::Run(REFERENCE_TIME tStart)
{
    return S_OK;
}

STDMETHODIMP CVideoFrameStepPID::Stop()
{
    return S_OK;
}


HRESULT FindFrameStepOnGraph(IUnknown *pUnkGraph, void **ppInterface)
{
    HRESULT hr = E_NOINTERFACE;

    IFilterGraph    *pFilterGraph = NULL;
    IBaseFilter     *pFilter = NULL;
    IEnumFilters    *pEnum = NULL;
    BOOL            bFound = FALSE;
    ULONG ulFetched = 0;

    if(!ppInterface)
        return E_FAIL;
    *ppInterface= NULL;

    if(!pUnkGraph)
        return E_FAIL;

    hr = pUnkGraph->QueryInterface(IID_IFilterGraph, (void **)&pFilterGraph);

    if (SUCCEEDED(hr))
    {
        hr = pFilterGraph->EnumFilters(&pEnum);
        if (SUCCEEDED(hr))
        {
            // find the first filter in the graph that supports riid interface
            while(!bFound && !*ppInterface && pEnum->Next(1, &pFilter, NULL) == S_OK)
            {
                hr = pFilter->QueryInterface(IID_IKsPropertySet, ppInterface);
                if (SUCCEEDED(hr))
                {
                    // check for required propset
                    DWORD dwType;
                    IKsPropertySet *pKsProp = (IKsPropertySet *) *ppInterface;
                    hr = pKsProp->QuerySupported(AM_KSPROPSETID_FrameStep, AM_PROPERTY_FRAMESTEP_STEP, &dwType);
                    if (SUCCEEDED(hr) && (dwType == KSPROPERTY_SUPPORT_SET))
                    {
                        // check if we can step on this interface
                        HRESULT hr1 = pKsProp->Set(AM_KSPROPSETID_FrameStep, AM_PROPERTY_FRAMESTEP_CANSTEP,
                            NULL, 0, NULL, 0);

                        if (hr1 == S_OK)
                        {
                            // return this interface
                            bFound = TRUE;
                        }
                        else
                        {
                            pKsProp->Release();
                            *ppInterface = NULL;
                        }
                    }
                    else
                    {
                        pKsProp->Release();
                        *ppInterface = NULL;
                    }
                }
                pFilter->Release();
            }
            pEnum->Release();
        }
        pFilterGraph->Release();
    }
    return hr;
}


HRESULT CVideoFrameStepPID::RefreshInterfaces()
{
    HRESULT hr = S_OK;

    if (m_bDirty)
    {
        m_pKsFrameSet = NULL;

        // see if we can find the interface we want.
        hr = FindFrameStepOnGraph(m_pUnk, (void **)&m_pKsFrameSet);
        if (SUCCEEDED(hr))
        {
            // we found the interface we want to avoid circular ref count issue
            // we never hold a refcount of this interface, the dshow plug-in
            // distributor model ensures that this interface will alway be valid
            // while our member m_bDirty == FALSE
            m_pKsFrameSet->Release();
        }
        // even though we may not have found a filter that support the frame step
        // property step set out dirty flag to false as the dirty flag will be reset
        // if any filters in the graph change.
        m_bDirty = FALSE;
    }

    if (SUCCEEDED(hr) && (m_pKsFrameSet == NULL))
    {
        // we must return a failure here as we don't have a required interface
        hr = VFW_E_FRAME_STEP_UNSUPPORTED;
    }

    return hr;
}

STDMETHODIMP CVideoFrameStepPID::Step(DWORD dwFrames, IUnknown *pStepObject)
{
    HRESULT hr = RefreshInterfaces();
    if (SUCCEEDED(hr) && m_pKsFrameSet)
    {
        AM_FRAMESTEP_STEP   amStep;

        amStep.dwFramesToStep = dwFrames;
        // set the frame step
        m_pKsFrameSet->Set(AM_KSPROPSETID_FrameStep, AM_PROPERTY_FRAMESTEP_STEP,
            NULL, 0, &amStep, sizeof(AM_FRAMESTEP_STEP));
    }
    return hr;
}

STDMETHODIMP CVideoFrameStepPID::CanStep(long lMultiple, IUnknown *pStepObject)
{
    HRESULT hr = RefreshInterfaces();
    if (SUCCEEDED(hr) && m_pKsFrameSet)
    {
        DWORD dwPropID = (lMultiple == 0) ? AM_PROPERTY_FRAMESTEP_CANSTEP : AM_PROPERTY_FRAMESTEP_CANSTEPMULTIPLE;
        // check if we can step on this interface
        hr = m_pKsFrameSet->Set(AM_KSPROPSETID_FrameStep, dwPropID,
            NULL, 0, NULL, 0);
    }
    return hr;
}

STDMETHODIMP CVideoFrameStepPID::CancelStep()
{
    return S_OK;
}

