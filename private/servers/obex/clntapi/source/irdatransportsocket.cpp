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
#include "IRDATransportSocket.h"
#include "ObexTransportConnection.h"
#include "ObexIRDATransport.h"

#include "ObexStrings.h"

/******globals******/

    extern CRITICAL_SECTION g_TransportSocketCS;
    static IRDA_CONNECTION_HOLDER *g_pConnectionList = 0;


UINT g_uiIRDAMaxRetries;


CIRDATransportSocket::CIRDATransportSocket():
    _refCount(1)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::CIRDATransportSocket()\n"));

    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(2,2), &wsd);
}

CIRDATransportSocket::~CIRDATransportSocket()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::~CIRDATransportSocket()\n"));
    Close();

    //clean up winsock
    WSACleanup();
 }

HRESULT STDMETHODCALLTYPE
CIRDATransportSocket::Close()
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::Close()\n"));

    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();

    HRESULT hRes = E_FAIL;


    IRDA_CONNECTION_HOLDER *pTemp = 0;
    IRDA_CONNECTION_HOLDER *pPrev = 0;

    if(g_pConnectionList)
    {
        pTemp = g_pConnectionList->pNext;
        pPrev = g_pConnectionList;
    }
    else
    {
        return S_OK;
    }

    if(pPrev)
    {
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(memcmp(pPrev->DeviceID, connectionID, 4) == 0)
        {
            IRDA_CONNECTION_HOLDER *pDel = pPrev;
            g_pConnectionList = pPrev->pNext;
            delete pDel;
            pTemp = 0;
            hRes = S_OK;
        }
    }

    while(pTemp && pTemp->pNext)
    {
        //its okay that we dont free up the memory pointed to here!
        // actually we CANT because we dont own a reference to it
        // to prevent a loop of refcounts
        if(memcmp(pTemp->pNext->DeviceID, connectionID, 4) == 0)
        {
            IRDA_CONNECTION_HOLDER *pDel = pTemp;
            pPrev->pNext = pTemp->pNext;

            delete pDel;
            hRes = S_OK;
            break;
        }

        pPrev = pTemp;
        pTemp = pTemp->pNext;
    }
    return hRes;
}

HRESULT STDMETHODCALLTYPE
CIRDATransportSocket::Listen(LPTRANSPORTCONNECTION *ppConnection)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::Listen()\n"));
    return E_NOTIMPL;
}

int
CIRDATransportSocket::reenum_connect(IObexAbortTransportEnumeration *pAbortEnum, SOCKET s,const SOCKADDR_IRDA *name, int namelen, char *pDeviceName)
{
    SVSUTIL_ASSERT(name);

    int returnValue = SOCKET_ERROR;

    //retry connecting (seems that the the server will quit listening if the
    //  user delays for about 6 seconds)... do this by enuming
    //  the IRDA devices looking for ours
    for ( UINT uiReTries=0; uiReTries < g_uiIRDAMaxRetries; uiReTries ++)
    {
        // This is to ensure that the server is ready to receive.
        SOCKET pollSock = socket(AF_IRDA, SOCK_STREAM, 0);
        if(INVALID_SOCKET != pollSock)
        {
            //memory to hold possible servers
            unsigned char   DevListBuff[sizeof(DEVICELIST) -
                sizeof(IRDA_DEVICE_INFO) +
                (sizeof(IRDA_DEVICE_INFO) * DEVICE_LIST_LEN)];

            int DevListLen   = sizeof(DevListBuff);
            PDEVICELIST   pDevList   = (PDEVICELIST) DevListBuff;

            pDevList->numDevice = 0;

            for(UINT uiTries=0; uiTries < g_uiIRDAMaxRetries; uiTries ++)
            {
                int optReturn = getsockopt(pollSock, SOL_IRLMP, IRLMP_ENUMDEVICES,(char *) pDevList, &DevListLen);

                // if we are told to abort, do so
                if(NULL != pAbortEnum) {
                    BOOL fAbort;
                    pAbortEnum->IsAborting(&fAbort);

                    if(TRUE == fAbort) {
                        closesocket(pollSock);
                        return SOCKET_ERROR;
                    }
                }

                //if there is an error see if it is an INPROGRESS one
                //  if so, try again (up to MAX_IRDA_TRIES) if not
                //  something happened that is more serious so bail out
                //  of the function with SOCKET_ERROR
                if(SOCKET_ERROR == optReturn)
                {
                    int errorCode = WSAGetLastError();
                    if(WSAEINPROGRESS == errorCode)
                    {
                        Sleep(500);
                        continue;
                    }
                    else
                    {
                        closesocket(pollSock);
                        return SOCKET_ERROR;
                    }
                }
                if(pDevList->numDevice != 0)
                    break;
            }

            closesocket(pollSock);

            //if we have devices, check to see if we have OUR device
            //   if not, bail out cause our device walked off
            if ( DevListLen != 0 )
            {
                 for(UINT i=0; i<pDevList->numDevice; i++)
                 {
                     SVSUTIL_ASSERT(sizeof(pDevList->Device[i].irdaDeviceName) >= strlen(pDeviceName));

                     if(0 == strcmp(pDevList->Device[i].irdaDeviceName, pDeviceName))
                         {
                             DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX-IRDA] Attempting reconnect\n"));

                            //patch up the device ID
                              SVSUTIL_ASSERT(4 == sizeof(pDevList->Device[i].irdaDeviceID));
                            memcpy((char *)name->irdaDeviceID, (char *)pDevList->Device[i].irdaDeviceID, 4);

                             //reconnect
                             returnValue = connect(s, (const struct sockaddr *) name, namelen);

                             if(SOCKET_ERROR != returnValue)
                             {
                                 DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] Got a connection!\n"));
                                 return returnValue;
                             }
                             else
                             {
                                 DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] Error(%x) connecting... maybe the device isnt in range?  trying again.\n", WSAGetLastError()));
                             }

                         }
                 }
            }
        }
        //an error getting an IRDA socket occured...  print an error, but dont
        //  do anything (we will retry)
        else
        {
            DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] Error getting an IRDA socket, trying again\n"));
        }
    }
    return returnValue;
}

HRESULT STDMETHODCALLTYPE
CIRDATransportSocket::Connect(LPPROPERTYBAG2 pDeviceProps,DWORD dwCapability, LPTRANSPORTCONNECTION * ppConnection)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::Connect()\n"));
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();

    PREFAST_ASSERT(pDeviceProps);
    *ppConnection = NULL;

    //set the default error to return
    HRESULT hRet = E_FAIL;

    //variables
    SOCKET sMySock = INVALID_SOCKET;
    IRDA_CONNECTION_HOLDER *pTemp = NULL;

    //fetch the device ID from the property bag
    VARIANT pDeviceVar;
    VARIANT pNameVar;
    VARIANT pPortVar;
    VariantInit(&pDeviceVar);
    VariantInit(&pNameVar);
    VariantInit(&pPortVar);

    //place to hold the port (if the port is NULL try the default ports
    //  of OBEX:IrXfer and then OBEX
    char cPortName[25];
    BOOL fFoundPortName = FALSE;
    char pDeviceName[25];

    if(FAILED(pDeviceProps->Read(c_szDevicePropAddress, &pDeviceVar, 0)))
    {
        hRet = E_FAIL;
        goto connect_done;
    }
    if(FAILED(pDeviceProps->Read(c_szDevicePropName, &pNameVar, 0)) || VT_BSTR != V_VT(&pNameVar))
    {
        hRet = E_FAIL;
        goto connect_done;
    }
    //also get the wide char version
    else
    {
        wcstombs_s(NULL, pDeviceName, sizeof(pDeviceName), pNameVar.bstrVal, 25);
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- using device name %s\n", pNameVar.bstrVal));
    }

    //read the port and convert from a wide char (BSTR)
    if(!FAILED(pDeviceProps->Read(c_szPort, &pPortVar, 0)))
    {
        wcstombs_s(NULL, cPortName, sizeof(cPortName), pPortVar.bstrVal, 25);
        fFoundPortName = TRUE;
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- using port name %s\n", pPortVar.bstrVal));
    }

    if(!(VT_I4 == V_VT(&pDeviceVar) &&
          VT_BSTR == V_VT(&pPortVar)))
    {
        DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX] ----- incorrect var type!\n"));
        hRet = E_FAIL;
        goto connect_done;
    }


    //figure out if we already have this guy allocated...
    pTemp = g_pConnectionList;

    //if we DO have it allocated, this loop will return an instance
    while(pTemp)
    {
        if(0 == memcmp(pTemp->DeviceID, (char *)&pDeviceVar.lVal, 4))
        {
            //get a copy of the id for our records
            memcpy(connectionID, pTemp->DeviceID, 4);

            DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - irda] ----->  reusing connection!\n"));

            (*ppConnection) = pTemp->pItem;
            (*ppConnection)->AddRef();

            hRet = S_OK;
            goto connect_done;
        }
        pTemp = pTemp->pNext;
    }

    //since we DONT have the instance, make one create a socket
    sMySock = socket(AF_IRDA, SOCK_STREAM, 0);
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - irda] -->Created Socket\n"));

    if(INVALID_SOCKET == sMySock)
    {
        SetLastError (ERROR_DEVICE_NOT_CONNECTED);
        hRet = E_FAIL;
        goto connect_done;
    }

    //information on the IrDA to connect to
    SOCKADDR_IRDA    DestSockAddr;
    memset(&DestSockAddr, 0, sizeof(SOCKADDR_IRDA));

    //move in the appropriate values (telling the service name and address family)
    DestSockAddr.irdaAddressFamily = AF_IRDA;

    // move in the device id for the selected device
    memcpy(&DestSockAddr.irdaDeviceID[0], (char *)&pDeviceVar.lVal, 4);

    //get a copy of the id for our records
    memcpy(connectionID, (char *)&pDeviceVar.lVal, 4);


    /*Connect up to the server-----------------------------------------------*/
    if(fFoundPortName)
    {
        //copy the name and begin the connection
        StringCchCopyA(DestSockAddr.irdaServiceName, _countof(DestSockAddr.irdaServiceName), cPortName);

        //connect to that device
        if (SOCKET_ERROR == reenum_connect(NULL, sMySock, &DestSockAddr,
            sizeof(SOCKADDR_IRDA),pDeviceName))
        {
            closesocket(sMySock);
            DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - irda] Error connecting: %d\n", WSAGetLastError()));

            hRet = E_FAIL;
            goto connect_done;
        }
    }
    else
    {
        //its okay to use strncpy because we've already 0'ed out the struct
        StringCchCopyNA(DestSockAddr.irdaServiceName, _countof(DestSockAddr.irdaServiceName), "OBEX", 4);

        //connect to that device
        if (SOCKET_ERROR == reenum_connect(NULL, sMySock, &DestSockAddr,
            sizeof(SOCKADDR_IRDA),pDeviceName))
        {
            //its okay to use strncpy because we've already 0'ed out the struct
            StringCchCopyNA(DestSockAddr.irdaServiceName, _countof(DestSockAddr.irdaServiceName), "OBEX:IrXfer", 11);

            //connect to that device
            if (SOCKET_ERROR == reenum_connect(NULL, sMySock, &DestSockAddr,
                sizeof(SOCKADDR_IRDA),pDeviceName)) {
                closesocket(sMySock);
                DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE, (L"[OBEX - irda] Error connecting: %d\n", WSAGetLastError()));

                hRet = E_FAIL;
                goto connect_done;
            }
        }
    }
    /*------------------------------------------------------------------------*/



    //if we are here, we dont have a transport connecction, so make one
    *ppConnection = new CObexTransportConnection(this, pNameVar.bstrVal, sMySock);
    if( !(*ppConnection) )
    {
         hRet = E_OUTOFMEMORY;
         goto connect_done;
    }

    //create and insert a new connection holder
    pTemp = new IRDA_CONNECTION_HOLDER();
    if( !pTemp )
    {
         delete *ppConnection;
         *ppConnection = NULL;

         hRet = E_OUTOFMEMORY;
         goto connect_done;
    }
    pTemp->pItem = *ppConnection;

    memcpy(pTemp->DeviceID, (char *)&pDeviceVar.lVal, 4);
    pTemp->pNext = g_pConnectionList;
    g_pConnectionList = pTemp;

    hRet = S_OK;

    connect_done:
        VariantClear(&pDeviceVar);
        VariantClear(&pNameVar);
        VariantClear(&pPortVar);

    return hRet;
}

HRESULT STDMETHODCALLTYPE
CIRDATransportSocket::GetProperties(LPPROPERTYBAG2 * ppListenProps)
{
    return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE
CIRDATransportSocket::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CIRDATransportSocket::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CIRDATransportSocket::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CIRDATransportSocket::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CIRDATransportSocket::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_TRANSPORTSOCKET_ZONE,(L"CIRDATransportSocket::QueryInterface()\n"));
    if(!ppv)
        return E_POINTER;
    if(riid == IID_IPropertyBag)
        *ppv = static_cast<IObexTransportSocket*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}




























