//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "upnp.h"

#include "Collection.hxx"
#include "DispatchImpl.hxx"
#include "string.hxx"
#include "list.hxx"
#include "sax.h"

#include "DeviceDescription.h"

class EmbeddedDevices;
class Services;

class Device : public ce::DispatchImpl<IUPnPDevice, &IID_IUPnPDevice>,
			   ce::SAXContentHandler
{
public:
	friend class DeviceDescription;

public:
	Device(DeviceDescription* pDeviceDescription, Device* pRootDevice = NULL, Device* pParentDevice = NULL);
	~Device();

// IUnknown	
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);
    STDMETHOD(QueryInterface)(REFIID iid, void **ppvObject);
    
// IUPnPDevice Methods
public:		
    STDMETHOD(get_IsRootDevice)     (/* [out] */ VARIANT_BOOL * pvarb);
    STDMETHOD(get_RootDevice)       (/* [out] */ IUPnPDevice ** ppudDeviceRoot);
    STDMETHOD(get_ParentDevice)     (/* [out] */ IUPnPDevice ** ppudDeviceParent);
    STDMETHOD(get_HasChildren)      (/* [out] */ VARIANT_BOOL * pvarb);
    STDMETHOD(get_Children)         (/* [out] */ IUPnPDevices ** ppudChildren);
    STDMETHOD(get_UniqueDeviceName) (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_FriendlyName)     (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_Type)             (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_PresentationURL)  (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_ManufacturerName) (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_ManufacturerURL)  (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_ModelName)        (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_ModelNumber)      (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_Description)      (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_ModelURL)         (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_UPC)              (/* [out] */ BSTR * pbstr);
    STDMETHOD(get_SerialNumber)     (/* [out] */ BSTR * pbstr);
    STDMETHOD(IconURL)              (/* in */  BSTR bstrEncodingFormat,
                                     /* in */  LONG lSizeX,
                                     /* in */  LONG lSizeY,
                                     /* in */  LONG lBitDepth,
                                     /* out */ BSTR * pbstrIconUrl);
    STDMETHOD(get_Services)         (/* [out] */ IUPnPServices ** ppusServices);

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

	virtual HRESULT STDMETHODCALLTYPE endDocument(void);

private:
	void    FixDeviceURL();
	void    MakeAbsoluteURL(ce::wstring* pstr);
	HRESULT ReturnString(const ce::wstring& str, BSTR* pbstr);

private:
	struct icon
	{
		icon(LPCWSTR pwszMimeType, LPCWSTR pwszWidth, LPCWSTR pwszHeight, LPCWSTR pwszDepth, LPCWSTR pwszURL)
			: m_strMimeType(pwszMimeType),
			  m_strURL(pwszURL)
		{
			m_nWidth = _wtoi(pwszWidth);
			m_nHeight = _wtoi(pwszHeight);
			m_nDepth = _wtoi(pwszDepth);
		}

		ce::wstring	m_strURL;
		ce::wstring	m_strMimeType;
		int			m_nWidth;
		int			m_nHeight;
		int			m_nDepth;
	};

private:
	// properties
	ce::wstring		m_strDeviceType;
	ce::wstring		m_strFriendlyName;
	ce::wstring		m_strManufacturer;
	ce::wstring		m_strManufacturerURL;
	ce::wstring		m_strModelDescription;
	ce::wstring		m_strModelName;
	ce::wstring		m_strModelNumber;
	ce::wstring		m_strModelURL;
	ce::wstring		m_strSerialNumber;
	ce::wstring		m_strUDN;
	ce::wstring		m_strUPC;
	ce::wstring		m_strPresentationURL;
	ce::list<icon>	m_listIcons;

	// implementation
	DeviceDescription* m_pDeviceDescription;
	Device*			m_pParentDevice;
	Device*			m_pRootDevice;
	Services*		m_pServices;
	EmbeddedDevices* m_pDevices;
	bool			m_bRootDevice;
	bool			m_bHasChildren;
	long			m_lRefCount;
	
	// used during parsing
	ce::wstring		m_strServiceType;
	ce::wstring		m_strServiceId;
	ce::wstring		m_strServiceDescriptionURL;
	ce::wstring		m_strServiceControlURL;
	ce::wstring		m_strServiceEventSubURL;
	ce::wstring		m_strIconWidth;
	ce::wstring		m_strIconHeight;
	ce::wstring		m_strIconDepth;
	ce::wstring		m_strIconURL;
	ce::wstring		m_strIconMimeType;
	Device*			m_pTempDevice;
};


// EmbeddedDevices
class EmbeddedDevices : public ce::Collection<IUPnPDevice*, IUPnPDevices, &IID_IUPnPDevices, ce::com_element<ce::owned_ptr<Device>, &IID_IUPnPDevice> >
{
public:
	EmbeddedDevices(DeviceDescription* pDeviceDescription)
		: m_pDeviceDescription(pDeviceDescription)
	{
	}

// IUnknown	
	virtual ULONG STDMETHODCALLTYPE AddRef(void)
		{return m_pDeviceDescription->AddRef(); }

    virtual ULONG STDMETHODCALLTYPE Release(void)
		{return m_pDeviceDescription->Release(); }

    STDMETHOD(QueryInterface)(REFIID iid, void **ppvObject);

private:
	DeviceDescription* m_pDeviceDescription;
};


// Devices
class Devices : public ce::Collection<IUPnPDevice*, IUPnPDevices, &IID_IUPnPDevices, ce::com_element<ce::com_ptr<IUPnPDevice>, &IID_IUPnPDevice> >
{
public:
	// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(Devices)
        IMPLEMENTS_INTERFACE(IUPnPDevices)
    END_INTERFACE_TABLE()

    // implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(Devices)
};


// Services
class Services : public ce::Collection<IUPnPService*, IUPnPServices, &IID_IUPnPServices, ce::com_element<ce::com_ptr<IUPnPService>, &IID_IUPnPService> >
{
public:
	// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(Services)
        IMPLEMENTS_INTERFACE(IUPnPServices)
    END_INTERFACE_TABLE()

    // implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(Services)
};

#endif // __DEVICE_H__
