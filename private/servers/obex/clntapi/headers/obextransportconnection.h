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
#ifndef OBEX_TRASPORT_CONNECTION_H
#define OBEX_TRASPORT_CONNECTION_H


// Default number of seconds to wait for an Obex response (if not registry overridden)
#define OBEX_DEFAULT_RESPONSE_TIMEOUT  300
// Number of seconds to wait for an Obex response
extern DWORD g_ResponseTimeout;

class CObexTransportConnection : public IObexTransportConnection
{
public:
    CObexTransportConnection(IObexTransportSocket *pSocket, LPTSTR pszName, SOCKET sMySock);
    ~CObexTransportConnection();
	HRESULT STDMETHODCALLTYPE Close();
	HRESULT STDMETHODCALLTYPE Read(ULONG ulNumBytes, BYTE* pbData, ULONG* pulBytesRead, DWORD dwTimeout);
	HRESULT STDMETHODCALLTYPE Write(ULONG ulNumBytes, BYTE *pbData);
	HRESULT STDMETHODCALLTYPE SendRecv(ULONG ulSendBytes, BYTE *pbSendData,
					                  ULONG ulRecvBytes, BYTE *pbRecvData,
					                  ULONG *pulBytesRecvd, DWORD dwRecvTimeout);

	HRESULT STDMETHODCALLTYPE EnumProperties(LPPROPERTYBAG2 * pprops);


    ULONG STDMETHODCALLTYPE AddRef();        
    ULONG STDMETHODCALLTYPE Release();        
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);   

private:
    ULONG _refCount;
    SOCKET sMySock;
    BSTR pbstrName;
    BSTR pbstrNameField;
    VARIANT nameVar;
    IObexTransportSocket *pSocket; 
    BOOL fHasClosed;
};
#endif
