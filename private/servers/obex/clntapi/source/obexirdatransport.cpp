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
#include "common.h"
#include "ObexIRDATransport.h"
#include "IRDATransportSocket.h"
#include "CObex.h"
#include "PropertyBag.h"
#include "PropertyBagEnum.h"

#include "ObexStrings.h"

/*----------globals---------------*/
LPSOCKET CObexIRDATransport::pSocket = 0;

CObexIRDATransport::CObexIRDATransport() : _refCount(1), dwTimeOfLastEnum(0), fIsAborting(FALSE)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::CObexIRDATransport()\n"));

    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(2,2), &wsd);
}

CObexIRDATransport::~CObexIRDATransport()
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::~CObexIRDATransport()\n"));

    //clean up winsock
    WSACleanup();
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::Init(void)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::Init()\n"));
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexIRDATransport::Shutdown(void)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
//singleton that holds a socket object
CObexIRDATransport::CreateSocket(LPPROPERTYBAG2 pPropertyBag,
                                 LPSOCKET  *ppSocket)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::CreateSocket()\n"));

    *ppSocket = new CIRDATransportSocket();
    if( !(*ppSocket) )
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexIRDATransport::CreateSocketBlob(unsigned long ulSize,
                                     byte __RPC_FAR *pbData,
                                     LPSOCKET __RPC_FAR *ppSocket)
{
        return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::EnumDevices(LPPROPERTYBAGENUM *ppDevices)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::EnumDevices()\n"));

    //
    //  Because IRDA is very quick on enum we can end up spin locking
    //    during query.  this limits the frequency of call to 500 ms
    //
    if(GetTickCount() - dwTimeOfLastEnum < 500)
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX] ObexIRDATransport::UpdateDeviceProperties() -- sleeping for a bit to avoid spinlock\n"));
        Sleep(500);
    }
    dwTimeOfLastEnum = GetTickCount();

    //start by setting the enumeration to be 0 (error)
    *ppDevices = 0;

    //create a socket
    SOCKET s = socket(AF_IRDA, SOCK_STREAM, 0);
    if(s == INVALID_SOCKET) {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::EnumDevices() -- got an invalid socket\n"));
        SetLastError (ERROR_DEVICE_NOT_CONNECTED);
        return E_FAIL;
    }
    //memory to hold possible servers
    unsigned char   DevListBuff[sizeof(DEVICELIST) -
        sizeof(IRDA_DEVICE_INFO) +
        (sizeof(IRDA_DEVICE_INFO) * DEVICE_LIST_LEN)];

    int DevListLen   = sizeof(DevListBuff);
    PDEVICELIST   pDevList   = (PDEVICELIST)DevListBuff;
    pDevList->numDevice = 0;

    //search to see what IrDA devices are visible
    for(UINT uiTries=0; uiTries <= g_uiIRDAMaxRetries; uiTries ++)  {
        int optReturn = getsockopt(s,
                                   SOL_IRLMP,
                                   IRLMP_ENUMDEVICES,
                                   (char *) pDevList,
                                   &DevListLen);

        if (optReturn == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();

            //if a blocking call is being made, just sleep and
            //  try again, otherwise bail out
            if(WSAEINPROGRESS == errorCode)
            {
                DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::EnumDevices() -- error in progress.. trying again in 500 ms\n"));

                //clean out the structure so there is no
                //  confusion later (since it hasnt been inited
                //  by getsockopt)
                pDevList->numDevice = 0;

                //sleep for half a second to give the other
                // function time to finish up (note: this stinks
                // but there isnt anything we can do but wait it
                // out)
                Sleep(500);

            }
            else
            {
                closesocket(s);
                DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"Error code: %d\n", WSAGetLastError()));
                return E_FAIL;
            }
        }
        if(pDevList->numDevice != 0)
            break;

    }


    CPropertyBagEnum *pPropEnum = new CPropertyBagEnum();

    if(!pPropEnum)
    {
        closesocket(s);
        return E_OUTOFMEMORY;
    }

    CPropertyBag *pPropBag;
    HRESULT hr = S_OK;

    VARIANT var;
    VariantInit(&var);
    for(UINT i=0; i<pDevList->numDevice; i++)
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::EnumDevices() -- found %d devices\n", pDevList->numDevice));

        hr = S_OK;
        pPropBag = new CPropertyBag();

        if(NULL == pPropBag)
        {
            closesocket(s);
            pPropBag->Release();
            return E_OUTOFMEMORY;
        }


        // set the device ID
        var.vt = VT_I4;
        memcpy((char *)&var.lVal, pDevList->Device[i].irdaDeviceID, 4);

        hr = pPropBag->Write(c_szDevicePropAddress, &var);
        SVSUTIL_ASSERT(SUCCEEDED(hr));

        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::EnumDevices() -- filling in props for device\n"));


        // now the name
        WCHAR pszName[MAX_PATH];
        int nRet = MultiByteToWideChar(CP_ACP, 0, pDevList->Device[i].irdaDeviceName, -1, pszName, MAX_PATH);
        if ((nRet > 0) && (nRet < MAX_PATH))
        {
            VariantClear(&var);
            var.vt = VT_BSTR;
            var.bstrVal = SysAllocString(pszName);

            if(NULL == var.bstrVal)
            {
                closesocket(s);
                pPropBag->Release();
                return E_OUTOFMEMORY;
            }

            hr = pPropBag->Write(c_szDevicePropName, &var);
            SVSUTIL_ASSERT(SUCCEEDED(hr));
        }

        // finally the transport ID
        LPOLESTR pszClsid = NULL;
        hr = StringFromCLSID((REFCLSID) CLSID_IrdaTransport, &pszClsid);
           SVSUTIL_ASSERT(SUCCEEDED(hr));
        VariantClear(&var);
        var.vt = VT_BSTR;
        var.bstrVal = SysAllocString(pszClsid);
        CoTaskMemFree(pszClsid);

        hr = pPropBag->Write(c_szDevicePropTransport, &var);

        SVSUTIL_ASSERT(SUCCEEDED(hr));

        // add to the list
        pPropEnum->Insert(pPropBag);
        pPropBag->Release();
        VariantClear(&var);
    }


    *ppDevices = pPropEnum;
    closesocket(s);
    return hr;
}


HRESULT STDMETHODCALLTYPE
CObexIRDATransport::UpdateDeviceProperties(LPPROPERTYBAG2 __RPC_FAR pDevice,
                                          IPropertyBagEnum **_ppNewBagEnum,
                                          BOOL fGetJustEnoughToConnect,
                                          UINT *uiUpdateStatus)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"ObexIRDATransport::UpdateDeviceProperties()\n"));

    PREFAST_ASSERT(pDevice && uiUpdateStatus);

    //set the default error to return
    HRESULT hRet = E_FAIL;

    //variables
    SOCKET sMySock = INVALID_SOCKET;

    BOOL fHaveIrXFer = FALSE;
    BOOL fHaveObex = FALSE;


    //fetch the device ID from the property bag
    VARIANT pDeviceVar;
    VARIANT pNameVar;
    VARIANT varMarkTest;

    VariantInit(&pDeviceVar);
    VariantInit(&pNameVar);
    VariantInit(&varMarkTest);

    char pDeviceName[25];
    BOOL fAbort;


    //
    //  Do a test to see if we already have enough info (if we have been here)
    //
    if(SUCCEEDED(pDevice->Read(L"BeenVisited", &varMarkTest, 0)) &&
        1 == varMarkTest.lVal)
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX] ObexIRDATransport::UpdateDeviceProperties() -- already visited!\n"));
        *uiUpdateStatus = 0xFFFFFFFF;
        VariantClear(&varMarkTest);
        return S_OK;
    }
    VariantClear(&varMarkTest);


    if(FAILED(pDevice->Read(c_szDevicePropAddress, &pDeviceVar, 0)))
    {
        hRet = E_FAIL;
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX] ObexIRDATransport::UpdateDeviceProperties() -- cant find address!\n"));
        goto connect_done;
    }
    if(FAILED(pDevice->Read(c_szDevicePropName, &pNameVar, 0)))
    {
        hRet = E_FAIL;
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX] ObexIRDATransport::UpdateDeviceProperties() -- cant find IRDA name!\n"));
        goto connect_done;
    }
    //also get the wide char version
    else
    {
        *uiUpdateStatus = *uiUpdateStatus | FOUND_NAME;
        wcstombs_s(NULL, pDeviceName, sizeof(pDeviceName), pNameVar.bstrVal, 25);
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX] ----- using device name %s\n", pNameVar.bstrVal));
    }

    //create a socket
    sMySock = socket(AF_IRDA, SOCK_STREAM, 0);
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE, (L"[OBEX - IR] --> Created Socket\n"));

    if(INVALID_SOCKET == sMySock)
    {
        hRet = E_FAIL;
        goto connect_done;
    }

    //information on the IrDA to connect to
    SOCKADDR_IRDA DestSockAddr;

    memset(&DestSockAddr, 0, sizeof(SOCKADDR_IRDA));

    //move in the appropriate values (telling the service name and address family)
    DestSockAddr.irdaAddressFamily = AF_IRDA;

    // move in the device id for the selected device
    memcpy(&(DestSockAddr.irdaDeviceID[0]), (char *)&pDeviceVar.lVal, 4);

    //its okay to use strncpy because we've already 0'ed out the struct
    StringCchCopyNA(DestSockAddr.irdaServiceName, _countof(DestSockAddr.irdaServiceName), "OBEX", 4);

    //connect to that device

    if (SOCKET_ERROR != CIRDATransportSocket::reenum_connect(static_cast<IObexAbortTransportEnumeration*>(this),
                                                            sMySock,
                                                            &DestSockAddr,
                                                            sizeof(SOCKADDR_IRDA),
                                                            pDeviceName))
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"ObexIRDATransport::UpdateDeviceProp() -- found OBEX!\n"));
        fHaveObex = TRUE;
    }

    int iSockCloseRet = closesocket(sMySock);
    SVSUTIL_ASSERT(0 == iSockCloseRet);

    // if we are told to abort, do so
    IsAborting(&fAbort);
    if(TRUE == fAbort) {
        hRet = E_ABORT;
        goto connect_done;
    }

    sMySock = socket(AF_IRDA, SOCK_STREAM, 0);

    if(INVALID_SOCKET == sMySock)
    {
        hRet = E_FAIL;
        goto connect_done;
    }

    //its okay to use strncpy because we've already 0'ed out the struct
    StringCchCopyNA(DestSockAddr.irdaServiceName, _countof(DestSockAddr.irdaServiceName), "OBEX:IrXfer", 11);

    //connect to that device
    if (SOCKET_ERROR != CIRDATransportSocket::reenum_connect(static_cast<IObexAbortTransportEnumeration*>(this),
                                                    sMySock,
                                                    &DestSockAddr,
                                                    sizeof(SOCKADDR_IRDA),
                                                    pDeviceName))
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"ObexIRDATransport::UpdateDeviceProp() -- found OBEX:IrXfer!\n"));
        fHaveIrXFer = TRUE;
    }
    iSockCloseRet = closesocket(sMySock);
    SVSUTIL_ASSERT(0 == iSockCloseRet);

    // if we are told to abort, do so
    IsAborting(&fAbort);
    if(TRUE == fAbort) {
        hRet = E_ABORT;
        goto connect_done;
    }

    //put in a marker saying we have been here
    VARIANT    varMarker;
    VariantInit(&varMarker);
    varMarker.vt = VT_I4;
    varMarker.lVal = 1;
    hRet = pDevice->Write(L"BeenVisited", &varMarker);
       SVSUTIL_ASSERT(SUCCEEDED(hRet));


    //
    //  Now we have port information....
    //
    if(!fHaveObex && !fHaveIrXFer)
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"ObexIRDATransport::UpdateDeviceProp() -- dont have anything...erroring out -- no obex here\n"));

        if(_ppNewBagEnum)
            *_ppNewBagEnum = NULL;
        hRet = E_FAIL;
    }

    else if(fHaveObex && fHaveIrXFer && _ppNewBagEnum)
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"ObexIRDATransport::UpdateDeviceProp() -- got both OBEX and OBEX:IrXfer\n"));
        //
        //  Begin by creating a new device... (it will be OBEX)
        //
        CPropertyBagEnum *pNewBagEnum = new CPropertyBagEnum();
        if(!pNewBagEnum) {
            return E_OUTOFMEMORY;
        }

        *_ppNewBagEnum = pNewBagEnum; //<-- chain in the enumerator

        CPropertyBag *pNewBag = new CPropertyBag();
        if(!pNewBag) {
            return E_OUTOFMEMORY;
        }
        HRESULT hr;

        //copy the transport
        VARIANT pTransportVar;
        VariantInit(&pTransportVar);
        if(SUCCEEDED(pDevice->Read(c_szDevicePropTransport, &pTransportVar, 0)))
        {
            pNewBag->Write(c_szDevicePropTransport, &pTransportVar);
            SVSUTIL_ASSERT(0 == wcscmp(pTransportVar.bstrVal, L"{30A7BC02-59B6-40BB-AA2B-89EB49EF274E}"));
        }
        VariantClear(&pTransportVar);

        //write known values
        hr = pNewBag->Write(c_szDevicePropName, &pNameVar);
          SVSUTIL_ASSERT(SUCCEEDED(hr));
        hr = pNewBag->Write(c_szDevicePropAddress, &pDeviceVar);
          SVSUTIL_ASSERT(SUCCEEDED(hr));

        //put in a dont-revisit-me-again-marker
        hr = pNewBag->Write(L"BeenVisited", &varMarker);
          SVSUTIL_ASSERT(SUCCEEDED(hr));

        //and finally add the port (OBEX)_
        VARIANT pPortVar;
        VariantInit(&pPortVar);
        pPortVar.vt = VT_BSTR;
        pPortVar.bstrVal = SysAllocString(L"OBEX");
        hr = pNewBag->Write(c_szPort, &pPortVar);
           SVSUTIL_ASSERT(SUCCEEDED(hr));
        VariantClear(&pPortVar);


        pNewBagEnum->Insert(pNewBag);
        pNewBag->Release();
        //
        //  Now just fix up our current device.. its assuming OBEX:IrXfer
        //
        pPortVar.vt = VT_BSTR;
        pPortVar.bstrVal = SysAllocString(L"OBEX:IrXfer");
        hr = pDevice->Write(c_szPort, &pPortVar);
            SVSUTIL_ASSERT(SUCCEEDED(hr));
        VariantClear(&pPortVar);

        *uiUpdateStatus = 0xFFFFFFFF;
        hRet = S_OK;
    }
    else
    {
        DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"ObexIRDATransport::UpdateDeviceProp() -- just got one port\n"));

        HRESULT hr;

        //and finally add the port (OBEX)_
        VARIANT pPortVar;
        VariantInit(&pPortVar);
        pPortVar.vt = VT_BSTR;

        if(_ppNewBagEnum)
            *_ppNewBagEnum = NULL;

        if(fHaveObex)
            pPortVar.bstrVal = SysAllocString(L"OBEX");
        else
            pPortVar.bstrVal = SysAllocString(L"OBEX:IrXfer");

        hr = pDevice->Write(c_szPort, &pPortVar);
           SVSUTIL_ASSERT(SUCCEEDED(hr));
        VariantClear(&pPortVar);

        *uiUpdateStatus = 0xFFFFFFFF;
        hRet = S_OK;
    }

    VariantClear(&varMarker);

connect_done:
    VariantClear(&pDeviceVar);
    VariantClear(&pNameVar);
    return hRet;
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::Abort()
{
    fIsAborting = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::Resume()
{
    fIsAborting = FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::IsAborting(BOOL *_fIsAborting)
{
    *_fIsAborting = fIsAborting;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexIRDATransport::EnumProperties(LPPROPERTYBAG2 __RPC_FAR *ppProps)
{
     return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE
CObexIRDATransport::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexIRDATransport::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CObexIRDATransport::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexIRDATransport::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CObexIRDATransport::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_IRDATRANSPORT_ZONE,(L"CObexIRDATransport::QueryInterface()\n"));
       if(!ppv)
        return E_POINTER;
    else if(riid == IID_IUnknown)
        *ppv = this;
    else if(riid == IID_IObexTransport)
        *ppv = static_cast<IObexTransport*>(this);
    else if(riid == IID_IObexAbortTransportEnumeration)
        *ppv = static_cast<IObexAbortTransportEnumeration*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}



