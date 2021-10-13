//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __DEVICE_DESC_IMPL_H__
#define __DEVICE_DESC_IMPL_H__

#include "upnp.h"

#define DLLSVC 1
#include <combook.h>

#include "string.hxx"
#include "sax.h"
#include "list.hxx"
#include "DispatchImpl.hxx"
#include "sync.hxx"

class Device;

class DeviceDescription : public ce::DispatchImpl<IUPnPDescriptionDocument, &IID_IUPnPDescriptionDocument>, 
						  ce::SAXContentHandler
{
public:
	DeviceDescription(UINT nLifeTime = 0);
	~DeviceDescription();

	friend class Device;

	DEFAULT_CLASS_REGISTRY_TABLE(DeviceDescription,
								 TEXT("{1d8a9b47-3a28-4ce2-8a4b-bd34e45bceeb}"),	// CLSID
								 TEXT("UPnP DescriptionDocument Class"),			// friendly name
								 TEXT("UPnP.DescriptionDocument.1"),			    // ProgID
								 TEXT("UPnP.DescriptionDocument"),				    // version independent ProgID
								 TEXT("Both"));										// threading model

    BEGIN_INTERFACE_TABLE(DeviceDescription)
        IMPLEMENTS_INTERFACE(IUPnPDescriptionDocument)
        IMPLEMENTS_INTERFACE(IDispatch)
    END_INTERFACE_TABLE()

    IMPLEMENT_UNKNOWN(DeviceDescription);

    IMPLEMENT_CREATE_INSTANCE(DeviceDescription);

    IMPLEMENT_GENERIC_CLASS_FACTORY(DeviceDescription);

// IUPnPDescriptionDocument
public:
	STDMETHOD(get_ReadyState)	(/* [retval][out] */ LONG *plReadyState);
    STDMETHOD(Load)				(/* [in] */ BSTR bstrUrl);
    STDMETHOD(LoadAsync)		(/* [in] */ BSTR bstrUrl, /* [in] */ IUnknown *punkCallback);
    STDMETHOD(get_LoadResult)	(/* [retval][out] */ long *phrError);
    STDMETHOD(Abort)			(void);
    STDMETHOD(RootDevice)		(/* [retval][out] */ IUPnPDevice **ppudRootDevice);
    STDMETHOD(DeviceByUDN)		(/* [in] */ BSTR bstrUDN, /* [retval][out] */ IUPnPDevice **ppudDevice);

private:
	void				DeviceAddedCallback(Device* pDevice);
	void				LoadImpl();
	HRESULT				Parse(LPCSTR pszDescriptionURL);
	static DWORD WINAPI LoadAsyncThread(LPVOID lpParameter);

	typedef enum tagREADYSTATE
	{
		READYSTATE_UNINITIALIZED = 0,
		READYSTATE_LOADING = 1,
		READYSTATE_LOADED = 2,
		READYSTATE_INTERACTIVE = 3,
		READYSTATE_COMPLETE = 4         // _hrLoadResult is valid
	} READYSTATE;

// ISAXContentHandler
private:
	virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);

private:
	ce::string				m_strBaseURL;
	ce::wstring				m_strBaseURLTag;
	ce::wstring				m_strSpecVersionMajor;
	ce::wstring				m_strSpecVersionMinor;
	ce::wstring				m_strUrl;
	Device*					m_pRootDevice;
	Device*					m_pTempDevice;
	int						m_nPass;
	bool					m_bAbort;
	bool                    m_bParsedRootElement;
	ce::list<Device*>		m_listDevices;
	READYSTATE				m_ReadyState;
	HRESULT					m_hrLoadResult;
	ce::critical_section	m_cs;
	IUnknown*				m_punkCallback;
	UINT					m_nLifeTime;
};

#endif // __DEVICE_DESC_IMPL_H__
