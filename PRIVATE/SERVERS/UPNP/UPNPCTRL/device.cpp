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

#include <combook.h>
#include "Device.h"

/******************************************************************************

UPnP has concept of root and embedded devices. Device description document 
contains description of the root device and (optionally) its embedded devices.

In Windows UPnP API a Device object has collection of embedded devices. Root
device and all its direct and indirect embedded devices form a tree. Device finder 
may return a pointer to root device or any of the embedded devices within the
tree. Application can traverse the tree using Device object properties such as 
ParentDevice, RootDevice, Children. 

From application point of view, each Device object is a COM object which is 
ref-counted. Application that holds a reference to any Device object or
collection of its embadded devices within the tree can get reference to any other 
device or its embadded devices collection using properties mentioned above. 
This means that all objects in the tree much stay alive as long as an application 
keeps a reference to any one of them.

Above requirements are implemented using following ref-counting scheme.

DeviceDescription is an object that keeps global reference count for the whole 
tree of the devices originating from given device description document. 
DeviceDescription holds a weak (not ref-counted) pointer to root Device 
object. 
When ref-count comes down to zero DeviceDescription object deletes
itself. Upon destruction DeviceDescription deletes the root Device object.

Device object is a COM object with a custom ref-counting implementation which 
delegates AddRef and Release calls to DeviceDescription object. Device object
holds a weak (not ref-counted) pointer to DeviceDescription and to EmbeddedDevices,
an object representing collection of its embedded devices. 
Upon destruction Device object deletes the EmbeddedDevices object.

EmbeddedDevices is COM object with a custom ref-counting implementation which 
delegates AddRef and Release calls to DeviceDescription object. EmbeddedDevices 
is used to implement Children property of Device object. EmbeddedDevices
holds a week (not ref-counted) pointer to Device objects contained in the 
collection. 
Upon destruction EmbeddedDevices deletes contained Device objects.

Devices is a COM object with standard ref-counting. Devices is used to 
implement a standard collection of Device objects. It holds reference to each
of the contained objects. Devices object is used to implement collection that 
is returned by device finder. 
When ref-count comes down to zero Devices object deletes itself. 
Upon destruction Devices object releases reference to contained Device objects.

******************************************************************************/



// Device
Device::Device(DeviceDescription* pDeviceDescription, Device* pRootDevice/* = NULL*/, Device* pParentDevice/* = NULL*/)
	: m_pDeviceDescription(pDeviceDescription),
	  m_lRefCount(0),
	  m_bRootDevice(false),
	  m_bHasChildren(false),
	  m_pTempDevice(NULL),
	  m_pServices(NULL),
	  m_pDevices(NULL),
	  m_pRootDevice(pRootDevice),	 // not AddRef-ed to avoid circular references
	  m_pParentDevice(pParentDevice) // not AddRef-ed to avoid circular references
{
	// if pRootDevice is NULL this is the root device
	if(!m_pRootDevice)
	{
		m_bRootDevice = true;
		m_pRootDevice = this;
	}

	m_pServices = new Services;
	m_pServices->AddRef();

	// child devices collection is hold w\o ref count
	m_pDevices = new EmbeddedDevices(m_pDeviceDescription);
}


// ~Device
Device::~Device()
{
	delete m_pTempDevice;
	delete m_pDevices;

	if(m_pServices)
		m_pServices->Release();
}


///////////////////////////////////
// IUnknown methods

inline BOOL InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((PLONG) &rguid1)[0] == ((PLONG) &rguid2)[0] &&
      ((PLONG) &rguid1)[1] == ((PLONG) &rguid2)[1] &&
      ((PLONG) &rguid1)[2] == ((PLONG) &rguid2)[2] &&
      ((PLONG) &rguid1)[3] == ((PLONG) &rguid2)[3]);
}


// AddRef
ULONG STDMETHODCALLTYPE Device::AddRef(void)
{
	// device description keeps actual ref count for the whole tree of devices
	return m_pDeviceDescription->AddRef();
}


// Release
ULONG STDMETHODCALLTYPE Device::Release(void)
{
	// device description keeps actual ref count for the whole tree of devices
	return m_pDeviceDescription->Release();
}


// QueryInterface
STDMETHODIMP Device::QueryInterface(REFIID iid, void **ppvObject)
{
	CHECK_POINTER(ppvObject);

	if(InlineIsEqualGUID(iid, IID_IUnknown) || 
       InlineIsEqualGUID(iid, IID_IDispatch) ||
	   InlineIsEqualGUID(iid, IID_IUPnPDevice))
    {
        *ppvObject = this;

		AddRef();

        return S_OK;
    }
    else
        return E_NOINTERFACE;
}


// ReturnString
HRESULT Device::ReturnString(const ce::wstring& str, BSTR* pbstr)
{
    CHECK_POINTER(pbstr);
    
    *pbstr = NULL;
    
    if(str.empty())
        return S_FALSE;
        
    *pbstr = SysAllocString(str);

	return *pbstr == NULL ? E_OUTOFMEMORY : S_OK;
}


///////////////////////////////////
// IUPnPDevice methods

// get_IsRootDevice
STDMETHODIMP Device::get_IsRootDevice(/* [out] */ VARIANT_BOOL * pvarb)
{
	CHECK_POINTER(pvarb);

	*pvarb = m_bRootDevice ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}


// get_HasChildren
STDMETHODIMP Device::get_HasChildren(/* [out] */ VARIANT_BOOL * pvarb)
{
	CHECK_POINTER(pvarb);

	*pvarb = m_bHasChildren ? VARIANT_TRUE : VARIANT_FALSE;
	
	return S_OK;
}


// get_UniqueDeviceName
STDMETHODIMP Device::get_UniqueDeviceName(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strUDN, pbstr);
}


// get_FriendlyName
STDMETHODIMP Device::get_FriendlyName(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strFriendlyName, pbstr);
}


// get_Type
STDMETHODIMP Device::get_Type(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strDeviceType, pbstr);
}


// get_PresentationURL
STDMETHODIMP Device::get_PresentationURL(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strPresentationURL, pbstr);
}


// get_ManufacturerName
STDMETHODIMP Device::get_ManufacturerName(/* [out] */ BSTR * pbstr)
{
    return ReturnString(m_strManufacturer, pbstr);
}


// get_ManufacturerURL
STDMETHODIMP Device::get_ManufacturerURL(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strManufacturerURL, pbstr);
}


// get_ModelName
STDMETHODIMP Device::get_ModelName(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strModelName, pbstr);
}


// get_ModelNumber
STDMETHODIMP Device::get_ModelNumber(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strModelNumber, pbstr);
}


// get_Description
STDMETHODIMP Device::get_Description(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strModelDescription, pbstr);
}


// get_ModelURL
STDMETHODIMP Device::get_ModelURL(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strModelURL, pbstr);
}


// get_UPC
STDMETHODIMP Device::get_UPC(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strUPC, pbstr);
}


// get_SerialNumber
STDMETHODIMP Device::get_SerialNumber(/* [out] */ BSTR * pbstr)
{
	return ReturnString(m_strSerialNumber, pbstr);
}


// get_RootDevice
STDMETHODIMP Device::get_RootDevice(/* [out] */ IUPnPDevice ** ppudDeviceRoot)
{
	Assert(m_pRootDevice);

	return m_pRootDevice->QueryInterface(IID_IUPnPDevice, (void**)ppudDeviceRoot);
}


// get_ParentDevice
STDMETHODIMP Device::get_ParentDevice(/* [out] */ IUPnPDevice ** ppudDeviceParent)
{
	if(!m_pParentDevice)
	{
		*ppudDeviceParent = NULL;
		return S_FALSE;
	}

	return m_pParentDevice->QueryInterface(IID_IUPnPDevice, (void**)ppudDeviceParent);
}


// get_Children
STDMETHODIMP Device::get_Children(/* [out] */ IUPnPDevices ** ppudChildren)
{
	if(!m_pDevices)
		return E_OUTOFMEMORY;

	return m_pDevices->QueryInterface(IID_IUPnPDevices, (void**)ppudChildren);
}


// get_Services
STDMETHODIMP Device::get_Services(/* [out] */ IUPnPServices ** ppusServices)
{
	if(!m_pServices)
		return E_OUTOFMEMORY;

	return m_pServices->QueryInterface(IID_IUPnPServices, (void**)ppusServices);
}


//icon utility functions stolen from NT

/***************************************************************************\
* ulMyAbs (stolen from "MyAbs")
*
* Calcules my weighted absolute value of the difference between 2 nums.
* This of course normalizes values to >= zero.  But it also doubles them
* if valueHave < valueWant.  This is because you get worse results trying
* to extrapolate from uless info up then interpolating from more info down.
* I paid $150 to take the SAT.  I'm damned well going to get my vocab's
* money worth.
*
\***************************************************************************/

static ULONG
ulMyAbs(LONG lValueHave,
        LONG lValueWant)
{
    LONG lDiff;

    lDiff = lValueHave - lValueWant;

    if (lDiff < 0)
    {
       lDiff = -2 * lDiff;
    }

    return lDiff;
}


/***************************************************************************\
* ulMatchImage (stolen from "MatchImage")
*
* This function takes ulPINTs for width & height in case of "real size".
* For this option, we use dimensions of 1st icon in resdir as size to
* uload, instead of system metrics.
*
* Returns a number that measures how "far away" the given image is
* from a desired one.  The value is 0 for an exact match.  Note that our
* formula has the following properties:
*     (1) Differences in width/height count much more than differences in
*         color format.
*     (2) Fewer colors give a smaller difference than more
*     (3) Bigger images are better than smaller, since shrinking produces
*             better results than stretching.
*
* The formula is the sum of the following terms:
*     ulog2(colors wanted) - ulog2(colors really), times -2 if the image
*         has more colors than we'd ulike.  This is because we will ulose
*         information when converting to fewer colors, ulike 16 color to
*         monochrome.
*     ulog2(width really) - ulog2(width wanted), times -2 if the image is
*         narrower than what we'd ulike.  This is because we will get a
*         better result when consolidating more information into a smaller
*         space, than when extrapolating from uless information to more.
*     ulog2(height really) - ulog2(height wanted), times -2 if the image is
*         shorter than what we'd ulike.  This is for the same reason as
*         the width.
*
* ulet's step through an example.  Suppose we want a 16 (4-bit) color, 32x32 image,
* and are choosing from the following ulist:
*     16 color, 64x64 image
*     16 color, 16x16 image
*      8 color, 32x32 image
*      2 color, 32x32 image
*
* We'd prefer the images in the following order:
*      8 color, 32x32         : Match value is 0 + 0 + 1     == 1
*     16 color, 64x64         : Match value is 1 + 1 + 0     == 2
*      2 color, 32x32         : Match value is 0 + 0 + 3     == 3
*     16 color, 16x16         : Match value is 2*1 + 2*1 + 0 == 4
*
\***************************************************************************/

ULONG
ulMatchImage(ULONG ulCurrentX,
             ULONG ulCurrentY,
             ULONG ulCurrentBpp,
             ULONG ulDesiredX,
             ULONG ulDesiredY,
             ULONG ulDesiredBpp)
{
    ULONG ulResult;

    ULONG ulBppScore;
    ULONG ulXScore;
    ULONG ulYScore;

    /*
     * Here are the rules for our "match" formula:
     *      (1) A close size match is much preferable to a color match
     *      (2) Fewer colors are better than more
     *      (3) Bigger icons are better than smaller
     */

    ulBppScore = ulMyAbs(ulCurrentBpp, ulDesiredBpp);
    ulXScore = ulMyAbs(ulCurrentX, ulDesiredX);
    ulYScore = ulMyAbs(ulCurrentY, ulDesiredY);

    ulResult = 2 * ulBppScore + ulXScore + ulYScore;

    return ulResult;
}


// IconURL
STDMETHODIMP Device::IconURL(/* in */  BSTR bstrEncodingFormat,
                             /* in */  LONG lSizeX,
                             /* in */  LONG lSizeY,
                             /* in */  LONG lBitDepth,
                             /* out */ BSTR * pbstrIconUrl)
{
	CHECK_POINTER(bstrEncodingFormat);
	CHECK_POINTER(pbstrIconUrl);
	
	ce::wstring *pstrURL = NULL;
	int			nMatch, nBestMatch = -1;
	
	for(ce::list<icon>::iterator it = m_listIcons.begin(), itEnd = m_listIcons.end(); it != itEnd; ++it)
		if(it->m_strMimeType == bstrEncodingFormat)
		{
			nMatch = ulMatchImage(it->m_nWidth, it->m_nHeight, it->m_nDepth, lSizeX, lSizeY, lBitDepth);

			if(nMatch > nBestMatch)
			{
				nBestMatch = nMatch;
				pstrURL = &it->m_strURL;
			}
		}
	
	if(pstrURL)
	{
		*pbstrIconUrl = SysAllocString(*pstrURL);

		return *pbstrIconUrl ? S_OK : E_OUTOFMEMORY;
	}
	
	return E_FAIL;
}


// QueryInterface
STDMETHODIMP EmbeddedDevices::QueryInterface(REFIID iid, void **ppvObject)
{
	CHECK_POINTER(ppvObject);
	
	if(InlineIsEqualGUID(iid, IID_IUnknown) || 
       InlineIsEqualGUID(iid, IID_IDispatch) ||
	   InlineIsEqualGUID(iid, IID_IUPnPDevices))
    {
        *ppvObject = this;

		AddRef();

        return S_OK;
    }
    else
        return E_NOINTERFACE;
}

