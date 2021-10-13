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
#ifndef OBEX_IRDA_TRANSPORT_SOCKET_H
#define OBEX_IRDA_TRANSPORT_SOCKET_H

struct IRDA_CONNECTION_HOLDER
{
    LPTRANSPORTCONNECTION pItem;
    UCHAR DeviceID[4];
    IRDA_CONNECTION_HOLDER *pNext;   
};


class CIRDATransportSocket : public IObexTransportSocket
{
public:
	CIRDATransportSocket();
	~CIRDATransportSocket();

    HRESULT STDMETHODCALLTYPE Close();
    HRESULT STDMETHODCALLTYPE Listen(LPTRANSPORTCONNECTION *ppConnection);
    HRESULT STDMETHODCALLTYPE Connect(LPPROPERTYBAG2 pDeviceProps, DWORD dwCapability, LPTRANSPORTCONNECTION * ppConnection);
	HRESULT STDMETHODCALLTYPE GetProperties(LPPROPERTYBAG2 * ppListenProps);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

	static int reenum_connect(IObexAbortTransportEnumeration *pAbortEnum, 
                              SOCKET s,
							  const SOCKADDR_IRDA *name, 
							  int namelen,
							  char *pDeviceName);
							  
private:
    ULONG _refCount;    
    static IRDA_CONNECTION_HOLDER *pConnectionList;   
    UCHAR connectionID[4];  
};

#endif // !defined(OBEX_TRANSPORT_SOCKET_H)
