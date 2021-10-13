//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop

#include "com_macros.h"

#include "DeviceDescription.h"
#include "Device.h"

// DeviceDescription
DeviceDescription::DeviceDescription(UINT nLifeTime/* = 0*/)
	: m_pTempDevice(NULL),
	  m_pRootDevice(NULL),
	  m_nPass(0),
	  m_ReadyState(READYSTATE_UNINITIALIZED),
	  m_hrLoadResult(E_UNEXPECTED),
	  m_bAbort(false),
	  m_nLifeTime(nLifeTime),
	  m_bParsedRootElement(false),
	  m_punkCallback(NULL)
{
}


// ~DeviceDescription
DeviceDescription::~DeviceDescription()
{
	delete m_pRootDevice;
	delete m_pTempDevice;

	if(m_punkCallback)
		m_punkCallback->Release();
}


// get_ReadyState
STDMETHODIMP DeviceDescription::get_ReadyState(/* [retval][out] */ LONG *plReadyState)
{
	CHECK_POINTER(plReadyState);

	ce::gate<ce::critical_section> _gate(m_cs);

	*plReadyState = m_ReadyState;
	
	return S_OK;
}


// Load
STDMETHODIMP DeviceDescription::Load(/* [in] */ BSTR bstrUrl)
{
	CHECK_POINTER(bstrUrl);

	{
		ce::gate<ce::critical_section> _gate(m_cs);

		if(m_ReadyState == READYSTATE_LOADING)
			return E_FAIL;

		if(m_pRootDevice != NULL)
			return E_FAIL;
		
		m_ReadyState = READYSTATE_LOADING;
		
		m_hrLoadResult = E_PENDING;
	}

	m_strUrl = bstrUrl;

	LoadImpl();

	return m_hrLoadResult;
}	


// LoadAsync
STDMETHODIMP DeviceDescription::LoadAsync(/* [in] */ BSTR bstrUrl, /* [in] */ IUnknown *punkCallback)
{
	CHECK_POINTER(punkCallback);
	CHECK_POINTER(bstrUrl);

	{
		ce::gate<ce::critical_section> _gate(m_cs);

		if(m_ReadyState == READYSTATE_LOADING)
			return E_FAIL;

		if(m_pRootDevice != NULL)
			return E_FAIL;

		m_ReadyState = READYSTATE_LOADING;
		
		m_hrLoadResult = E_PENDING;
	}
	
	HRESULT hr;
	
	if(SUCCEEDED(hr = punkCallback->QueryInterface(IID_IUnknown, (void**)&m_punkCallback)))
	{	
		m_strUrl = bstrUrl;

		CreateThread(NULL, 0, LoadAsyncThread, this, 0, NULL);
	}
	
	return hr;
}


// LoadAsyncThread
DWORD WINAPI DeviceDescription::LoadAsyncThread(LPVOID lpParameter)
{
	DeviceDescription* pThis = (DeviceDescription*)lpParameter;

	Assert(pThis);

	pThis->LoadImpl();

	return 0;
}


// LoadImpl
void DeviceDescription::LoadImpl()
{
	ce::string	strURL;

	ce::WideCharToMultiByte(CP_ACP, m_strUrl, -1, &strURL);
	
	HRESULT hrParseResult = Parse(strURL);

	IUPnPDescriptionDocumentCallback*	pCallback = NULL;
	IDispatch*							pDispatch = NULL;
	
	{
		ce::gate<ce::critical_section> _gate(m_cs);

		if(m_bAbort)
		    m_hrLoadResult = E_ABORT;
		else
		    m_hrLoadResult = hrParseResult;
		    
		if(FAILED(m_hrLoadResult))
		{
			m_bAbort = false;

			delete m_pRootDevice;
			m_pRootDevice = NULL;

			delete m_pTempDevice;
			m_pTempDevice = NULL;

			m_listDevices.erase(m_listDevices.begin(), m_listDevices.end());
		}

		m_ReadyState = READYSTATE_COMPLETE;

		if(m_punkCallback)
		{
			if(FAILED(m_punkCallback->QueryInterface(IID_IUPnPDescriptionDocumentCallback, (void**)&pCallback)))
				m_punkCallback->QueryInterface(IID_IDispatch, (void**)&pDispatch);

			m_punkCallback->Release();
			m_punkCallback = NULL;
		}
	}

	if(pCallback)
	{
		pCallback->LoadComplete(m_hrLoadResult);
		pCallback->Release();
	}
	else
		if(pDispatch)
		{
			DISPPARAMS  DispParams = {0};
			VARIANT		vtArg1;
			UINT        uArgErr;
			
			DispParams.cArgs = 1;
			DispParams.rgvarg = &vtArg1;

			V_VT(&vtArg1) = VT_I4;
			V_I4(&vtArg1) = m_hrLoadResult;

			pDispatch->Invoke(DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD, &DispParams, NULL, NULL, &uArgErr);
			pDispatch->Release();
		}
}


// get_LoadResult
STDMETHODIMP DeviceDescription::get_LoadResult(/* [retval][out] */ long *phrError)
{
	CHECK_POINTER(phrError);

	ce::gate<ce::critical_section> _gate(m_cs);
	
	*phrError = m_hrLoadResult;

	return S_OK;
}


// Abort
STDMETHODIMP DeviceDescription::Abort(void)
{
	ce::gate<ce::critical_section> _gate(m_cs);

	if(m_ReadyState == READYSTATE_LOADING)
		m_bAbort = true;
	
	return S_OK;
}


// RootDevice
STDMETHODIMP DeviceDescription::RootDevice(/* [retval][out] */ IUPnPDevice **ppudRootDevice)
{
	CHECK_POINTER(ppudRootDevice);

	ce::gate<ce::critical_section> _gate(m_cs);

	if(m_ReadyState != READYSTATE_COMPLETE)
		return E_FAIL;

	if(m_pRootDevice)
		return m_pRootDevice->QueryInterface(IID_IUPnPDevice, (void**)ppudRootDevice);
	else
		return E_FAIL;
}


// DeviceByUDN
STDMETHODIMP DeviceDescription::DeviceByUDN(/* [in] */ BSTR bstrUDN, /* [retval][out] */ IUPnPDevice** ppDevice)
{
	ce::gate<ce::critical_section> _gate(m_cs);

	if(m_ReadyState != READYSTATE_COMPLETE)
		return E_FAIL;

	for(ce::list<Device*>::iterator it = m_listDevices.begin(), itEnd = m_listDevices.end(); it != itEnd; ++it)
		if((*it)->m_strUDN == bstrUDN)
			return (*it)->QueryInterface(IID_IUPnPDevice, (void**)ppDevice);

	return E_FAIL;
}
