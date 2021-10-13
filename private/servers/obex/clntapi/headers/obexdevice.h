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
// ObexDevice.h: interface for the CObexDevice class.
//
//////////////////////////////////////////////////////////////////////

#ifndef OBEXDEVICE_H
#define OBEXDEVICE_H

#define OBEX_INQUIRY_FAILURES   3
extern int g_iCredits;

#define MAX_PASS_SIZE 30

class CObexDevice : public IObexDevice
{
    friend class CObexStream;
public:
    CObexDevice(IPropertyBag *pPropBag, CLSID clsidTrans);
    ~CObexDevice();

    HRESULT STDMETHODCALLTYPE Connect (LPCWSTR pszPassword, DWORD dwCapability, LPHEADERCOLLECTION pHeaderCollection);
    HRESULT STDMETHODCALLTYPE Disconnect(LPHEADERCOLLECTION pHeaders);
    HRESULT STDMETHODCALLTYPE Get(LPHEADERCOLLECTION pHeaders, IStream **pStream);
    HRESULT STDMETHODCALLTYPE Put(LPHEADERCOLLECTION pHeaders, IStream **pStream);
    HRESULT STDMETHODCALLTYPE Abort(LPHEADERCOLLECTION pHeaders);
    HRESULT STDMETHODCALLTYPE SetPath(LPCWSTR pszName, DWORD dwFlags, LPHEADERCOLLECTION pHeaders);
    HRESULT STDMETHODCALLTYPE EnumProperties(REFIID riid, void **ppv);
    HRESULT STDMETHODCALLTYPE SetPassword(LPCWSTR pszPassword);
    HRESULT STDMETHODCALLTYPE BindToStorage(DWORD dwCapability, IStorage **ppStorage);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

public:
        HRESULT         GetDeviceName(VARIANT *pvarAddr);
        HRESULT         GetDeviceAddress(VARIANT *pvarAddr);
        HRESULT         GetPropertyBag(LPPROPERTYBAG2 *ppBag);
        HRESULT         GetDevicePort(VARIANT *pvarAddr);
        HRESULT     GetDeviceTransport(VARIANT *pvarAddr);

        CLSID GetTransport (void) {
                return clsidTransport;
        }

        BOOL GetPresent (void) {
                return iPresent != 0;
        }

        VOID SetPresent (int fPresent) {
                iPresent = fPresent ? g_iCredits : iPresent - 1;
                if (iPresent < 0)
                        iPresent = 0;
        }

        BOOL GetModified (void) {
                return iModified != 0;
        }

        VOID SetModified (int fModified) {
                iModified = fModified ? g_iCredits : iModified - 1;
                if (iModified < 0)
                        iModified = 0;
        }

        UINT GetUpdateStatus() {return uiUpdateStatus;}
        void SetUpdateStatus(UINT i) {uiUpdateStatus = i;}

        HRESULT ConnectSocket();
        IPropertyBag *GetPropertyBag() { return pPropBag; }
        UINT GetStreamID() {return uiActiveStreamID;}

protected:
    ULONG _refCount;

    IPropertyBag *pPropBag;
    IObexTransportConnection *pConnection;
    IObexTransportSocket *pSocket;
    unsigned int        uiConnectionId;
        unsigned int    uiMaxPacket;
        CLSID                   clsidTransport;
        int                             iPresent;
        int                     iModified;
        UINT                    uiUpdateStatus;
        WCHAR                   wcPassword[MAX_PASS_SIZE];
//      CObexStream     *pActiveStream;
        UINT                    uiActiveStreamID;

    HRESULT SendConnectPacket(IHeaderCollection *pHeaderCollection);


private:
    HRESULT ConnectToTransportSocket();
    HRESULT CompleteDiscovery();
};


UINT SizeOfHeader(IHeaderCollection *pHeader);
HRESULT ObexSendRecvWithAuth(IObexTransportConnection *pConn, UINT uiMaxPacket, WCHAR *wcPassword, BYTE cOpCode, char *additionalDta, UINT cAddnlDta, IHeaderCollection *pHeaderCollection, unsigned char **pNewPacket, ULONG *pSize);
HRESULT ObexSendRecv(IObexTransportConnection *pConn, UINT uiMaxPacket, BYTE cOpCode, char *additionalDta, UINT cAddnlDta, IHeaderCollection *pHeaderCollection, unsigned char **pNewPacket, ULONG *pSize);

#endif // !defined(OBEXDEVICE_H)
