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
#ifndef _CONPOINT_H_
#define _CONPOINT_H_

class CEnumConnectionPoints : public IEnumConnectionPoints
{
public:
	CEnumConnectionPoints(IConnectionPoint *pcnpInit);
	~CEnumConnectionPoints();

public:
	HRESULT STDMETHODCALLTYPE Next( ULONG cConnections, LPCONNECTIONPOINT *ppCP, ULONG *pcFetched);
	HRESULT STDMETHODCALLTYPE Skip(ULONG cConnections);
	HRESULT STDMETHODCALLTYPE Reset(void);
	HRESULT STDMETHODCALLTYPE Clone(IEnumConnectionPoints **ppEnum) { return E_NOTIMPL; }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
	ULONG   STDMETHODCALLTYPE AddRef();
	ULONG   STDMETHODCALLTYPE Release();

private:
	ULONG	_ulRefs;
	ULONG	_ulIndex;

	IConnectionPoint *_pcnp;
};

///////////////////////////////////////////////////////////////////////////////////////

#define SN_LOCKED	0x1
#define SN_REMOVED	0x2

typedef struct SINKNODE
{
	SINKNODE	*pNext;
	IObexSink	*pObSink;
	ULONG		ulCookie;
	ULONG		ulFlags;

public:
	SINKNODE (void)
	{
		memset (this, 0, sizeof(*this));
	}
} SINKNODE, *PSINKNODE;


class CConnectionPoint : public IConnectionPoint
{
public:
	CConnectionPoint(IConnectionPointContainer *pCPC);
	~CConnectionPoint();

    void    ContainerReleased();
	HRESULT    Notify(OBEX_EVENT evt, IUnknown *pUnk1, IUnknown *pUnk2);
	
    HRESULT STDMETHODCALLTYPE GetConnectionInterface(IID *pIID);
    HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(IConnectionPointContainer **ppCPC);
    HRESULT STDMETHODCALLTYPE Advise(IUnknown *pUnkSink, DWORD *pdwCookie);
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie);
    HRESULT STDMETHODCALLTYPE EnumConnections(IEnumConnections **ppEnum);

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
	ULONG   STDMETHODCALLTYPE AddRef();
	ULONG   STDMETHODCALLTYPE Release();

private:
	ULONG						_ulRefs;
	ULONG						_ulNextCookie;
	PSINKNODE					_pSinkList;
	ULONG						_ulConns;
	IConnectionPointContainer	*_pCPC;
};

///////////////////////////////////////////////////////////////////////////////////////

class CEnumConnections:public IEnumConnections
{
public:
	CEnumConnections();
	~CEnumConnections();

	HRESULT Init(SINKNODE *pSinkList, ULONG ulConns);

public:
	HRESULT STDMETHODCALLTYPE Next( ULONG cConnections, CONNECTDATA  *pCD, ULONG *pcFetched);
	HRESULT STDMETHODCALLTYPE Skip(ULONG cConnections);
	HRESULT STDMETHODCALLTYPE Reset(void);
	HRESULT STDMETHODCALLTYPE Clone(IEnumConnections **ppCD) { return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
	ULONG   STDMETHODCALLTYPE AddRef();
	ULONG   STDMETHODCALLTYPE Release();

private:
	ULONG			_ulRefs;
	ULONG			_ulIndex;
	ULONG			_ulConns;
	CONNECTDATA		*_pConnData;
};

#endif
