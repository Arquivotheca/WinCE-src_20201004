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
#include "ObexTransportConnection.h"
#include "ObexDevice.h"
#include "PropertyBag.h"
#include "obexstream.h"

DWORD g_ResponseTimeout;


int RecvHelper(SOCKET s, unsigned char *buf, int len, int flags);

extern CRITICAL_SECTION g_TransportSocketCS;

CObexTransportConnection::CObexTransportConnection(IObexTransportSocket *_pSocket, LPTSTR pszName, SOCKET _sMySock) :
     _refCount(1),
     sMySock(_sMySock),
     pSocket(_pSocket),
     fHasClosed(FALSE)
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::CObexTransportConnection()\n"));
    pSocket->AddRef();

    pbstrName = SysAllocString(pszName);
    pbstrNameField = SysAllocString(L"Name");
    nameVar.bstrVal = pbstrName;
}
CObexTransportConnection::~CObexTransportConnection()
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::~CObexTransportConnection()\n"));

    if(pbstrName)
        SysFreeString(pbstrName);
    if(pbstrNameField)
        SysFreeString(pbstrNameField);

    if(sMySock != INVALID_SOCKET)
    {
        DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE, (L"CObexTransportConnection::~CObexTransportConnection() -->Closing socket\n"));
        closesocket(sMySock);
        sMySock = INVALID_SOCKET;
    }

    pSocket->Close();
    pSocket->Release();
}

HRESULT STDMETHODCALLTYPE
CObexTransportConnection::Close()
{
   /*DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::Close()\n"));

   if(pSocket && !fHasClosed)
   {
       fHasClose = TRUE;
       pSocket->Close();
   }  */
   return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexTransportConnection::Read(ULONG ulNumBytes, BYTE *pPacket, ULONG *pulBytesRead, DWORD dwTimeout)
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::Read()\n"));
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();

    *pPacket = 0;
    *pulBytesRead = 0;


    if(INVALID_SOCKET == sMySock || ulNumBytes < 3)
    {
        DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::Read() -- invalid socket or requesting fewer thn 3 bytes\n"));
        return E_FAIL;
    }

    WORD wPacketSize;

    //get the opcode and packetsize
    int iRet = RecvHelper(sMySock, pPacket, 3, 0);

    if(iRet == 0 || iRet!= 3 || iRet == SOCKET_ERROR)
    {
        closesocket(sMySock);
        sMySock=INVALID_SOCKET;
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"Socket error recieving...\n"));
        return E_FAIL;
    }

    //get the opcode and the header size
    ((char *)&wPacketSize)[0] = pPacket[1];
    ((char *)&wPacketSize)[1] = pPacket[2];
    wPacketSize = ntohs(wPacketSize);


    SVSUTIL_ASSERT(wPacketSize <= g_uiMaxFileChunk);

    if(ulNumBytes < wPacketSize)
    {
         DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::Read() -- invalid packet size!\n"));
         return E_FAIL;
    }

    if(wPacketSize > 3)
    {
        //fetch the remainder of the packet
        int retVal = RecvHelper(sMySock, pPacket+3, wPacketSize - 3, 0);
        SVSUTIL_ASSERT(retVal == wPacketSize - 3);

        if(retVal == 0 || retVal != wPacketSize-3 || retVal == SOCKET_ERROR)
        {
            closesocket(sMySock);
            sMySock=INVALID_SOCKET;
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexTransportConnection::Read()   Socket error recieving...\n"));
            return E_FAIL;
        }


    }

//do a quick test to make sure nothing is on the wire
#ifdef OBEX_NETWORK_SANITY_CHECKS
        FD_SET fd;
        FD_ZERO (&fd);
        FD_SET (sMySock, &fd);
        timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
        int val =  select (0, &fd, NULL, NULL, &tv);
        SVSUTIL_ASSERT(val == 0);
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L">Nothing on wire... life is good\n"));
#endif

    *pulBytesRead = wPacketSize;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexTransportConnection::Write(ULONG wPacketSize, BYTE *pbData)
{
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();

    if(INVALID_SOCKET == sMySock)
    {
        DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::Write() -- invalid socket\n"));
        return E_FAIL;
    }

    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE, (L"CObexTransportConnection::Write\n"));
    HRESULT hRet = S_OK;
    char *pPacketTemp = (char *)pbData;

    while(wPacketSize)
    {

        int retCode = send(sMySock, pPacketTemp, wPacketSize, 0);

        if(retCode == 0 || retCode == SOCKET_ERROR)
        {
            closesocket(sMySock);
            sMySock = INVALID_SOCKET;
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"Error(0x%08x)--send failed\n", WSAGetLastError()));
            hRet = E_FAIL;
            break;
        }

        pPacketTemp += retCode;
        wPacketSize -= retCode;
    }
    return hRet;
}



/******************************************************************************/
/*  NOTE: VERY IMPORTANT!  --- this MUST be under a critical section
/*     We do a write of a packet, then a read --- we MUST do both under
/*     a crit section since the received packet doesnt have a connection ID
/******************************************************************************/
HRESULT STDMETHODCALLTYPE
CObexTransportConnection::SendRecv(ULONG ulSendBytes, BYTE *pbSendData,
                                       ULONG ulRecvBytes, BYTE *pbRecvData,
                                       ULONG *pulBytesRecvd, DWORD dwRecvTimeout)
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::SendRecv()\n"));
    CCritSection TransportCS(&g_TransportSocketCS);
    TransportCS.Lock();

    //send off the packet
    HRESULT hRet;
    hRet = Write(ulSendBytes, pbSendData);
    if(SUCCEEDED(hRet))
    {
        hRet = Read(ulRecvBytes, pbRecvData, pulBytesRecvd, INFINITE);

        if(FAILED(hRet))
        {
            DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::SendRecv -- Read FAILED\n"));
        }
    }
    else
    {
        DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::SendRecv -- Write FAILED\n"));
    }
    return hRet;
}

HRESULT STDMETHODCALLTYPE
CObexTransportConnection::EnumProperties(LPPROPERTYBAG2 *pprops)
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::EnumProperties()\n"));
    CPropertyBag *pProps = new CPropertyBag();

    if(NULL == pProps)
        return E_OUTOFMEMORY;

    //create a name field and save the property
    pProps->Write(pbstrNameField, &nameVar);

    *pprops = pProps;

    return S_OK;
}


   ULONG STDMETHODCALLTYPE
CObexTransportConnection::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexTransportConnection::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CObexTransportConnection::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexTransportConnection::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF && _refCount != 0);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CObexTransportConnection::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_TRASPORTCONNECTION_ZONE,(L"CObexTransportConnection::QueryInterface()\n"));
    if(!ppv)
        return E_POINTER;
    if(riid == IID_IObexTransportConnection)
        *ppv = static_cast<IObexTransportConnection*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}


int RecvHelper(SOCKET s, unsigned char *buf, int len, int flags)
{
    ASSERT(INVALID_SOCKET != s);

    int iTimeLeft = g_ResponseTimeout;
    int received = 0;
    int retValue = SOCKET_ERROR;

    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::RecvHelper()\n"));
    SVSUTIL_ASSERT(s != INVALID_SOCKET && buf);

    DWORD dwNow = GetTickCount ();

    FD_SET fd;
    FD_ZERO (&fd);

// Suppress the warning for the do {} while(0) in the FD_SET macro
#pragma warning(push)
#pragma warning(disable:4127)
    FD_SET (s, &fd);
#pragma warning(pop)

    #if defined(DEBUG) || defined(_DEBUG)
        UCHAR *pBufOrig = buf;
    #endif

    //while we still need data, try to get it
    while(received < len && iTimeLeft >= 0)
    {
        int iJustRead;
        timeval tv;

        tv.tv_sec = iTimeLeft;
        tv.tv_usec = 0;
        if (1 != select (0, &fd, NULL, NULL, &tv))
        {
            retValue = SOCKET_ERROR;
            goto recvHelperDone;
        }

        //if there is something available, fetch it
        if(FD_ISSET(s, &fd))
        {
            iJustRead = recv(s, (char *)buf, len - received, flags);
        }
        else
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"[OBEX] recv timed out!\n"));
            retValue = SOCKET_ERROR;
            goto recvHelperDone;
        }

        //either the socket closed, or error'ed... so return that
        if(iJustRead == 0 || iJustRead == SOCKET_ERROR)
        {
            retValue = iJustRead;
            goto recvHelperDone;
        }

        buf += iJustRead;
        received += iJustRead;
        iTimeLeft = g_ResponseTimeout - (GetTickCount () - dwNow) / 1000;
    }

#if defined(DEBUG) || defined(_DEBUG)
        void DumpBuff (unsigned char *lpBuffer, unsigned int cBuffer);
        DEBUGMSG(OBEX_DUMP_PACKETS_ZONE, (L"Recv'ed -------------------------------"));
        DumpBuff (pBufOrig, received);
#endif

    SVSUTIL_ASSERT(iTimeLeft);
    SVSUTIL_ASSERT(received == len);
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"RECV'ed: %d bytes\n", received));
    retValue = received;

    recvHelperDone:

    return retValue;
}
