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
// DeviceEnum.h: interface for the CDeviceEnum class.
//
//////////////////////////////////////////////////////////////////////

#ifndef DEVICEENUM_H
#define DEVICEENUM_H


#include "ObexDevice.h"
#include "propertybag.h"

struct DEVICE_LIST
{
	CObexDevice *pDevice;
	DEVICE_LIST *pNext;
};

struct DEVICE_PROPBAG_LIST
{
	IPropertyBag *pBag;
	DEVICE_PROPBAG_LIST *pNext;
};


struct DEVICE_NODE
{
    CObexDevice *pItem;
    DEVICE_NODE *pNext;

public:
	DEVICE_NODE (void)
	{
		memset (this, 0, sizeof(*this));
	}
};


class CDeviceEnum : public IDeviceEnum
{
     friend class CObex;
public:
	CDeviceEnum();
	~CDeviceEnum();

    //Next:  return 'celt' items
    HRESULT STDMETHODCALLTYPE Next (ULONG celt, LPDEVICE *rgelt, ULONG *pceltFetched);

    //Skip: skip 'celt' items
    HRESULT STDMETHODCALLTYPE Skip(ULONG celt);

    //Reset: reset enumeration to beginning
    HRESULT STDMETHODCALLTYPE Reset();

    //Clone: clone the enumeration, including location
    HRESULT STDMETHODCALLTYPE Clone(IDeviceEnum **ppenum);


    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

private:
	HRESULT	StartEnumRun(REFCLSID clsid);
	HRESULT	FinishEnumRun(DEVICE_PROPBAG_LIST **ppDevicesToRemove, REFCLSID uuidTransport);

	HRESULT EnumDevices(LPDEVICEENUM *ppDeviceEnum, REFCLSID uuidTransport);
	HRESULT FindDevicesThatMatch(LPPROPERTYBAG2 pBag, DEVICE_LIST **pDeviceList);

	//insert a device into the list
    HRESULT STDMETHODCALLTYPE Insert(CObexDevice *pDevice);

private:
    DEVICE_NODE *pListHead;
    DEVICE_NODE *pListCurrent;


    ULONG _refCount;
};

#endif // !defined(DEVICEENUM_H)
