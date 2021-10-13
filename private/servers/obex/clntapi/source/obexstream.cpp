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
// ObexStream.cpp: implementation of the CObexStream class.
//
//////////////////////////////////////////////////////////////////////
#include "common.h"
#include "ObexDevice.h"
#include "ObexParser.h"
#include "ObexStream.h"
#include "HeaderEnum.h"
#include "HeaderCollection.h"
#include "ObexPacketInfo.h"
#include <intsafe.h>

//the size of the opcode(1)+size(2)
#define MIN_PUT_PACKET_SIZE 3


UINT GetNewStreamID()
{
    static UINT uiID = 0;
    uiID++;
    if(uiID == 0xFFFFFFFF)
        uiID = 0;

    return uiID;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CObexStream::CObexStream(
       IObexTransportConnection *_pConnection,
       CObexDevice *_pMyOBEXDevice,
       UINT _uiMaxPacket,
       UINT _uiConnectionId,
       WCHAR *_pwcPassword,
       LPHEADERCOLLECTION _pHeaders) :
pMyOBEXDevice(_pMyOBEXDevice),
_refCount(1),
uiBodyLen(0),
myHeaderCollection(0),
fHeaderAccounted(FALSE),
uiStreamType(OBEX_PUT),
pcBodyPtr(NULL),
pcBodyChunk(NULL),
fFirstPacket(TRUE),
fReadFinished(FALSE),
fHaveSentData(FALSE),
uiMaxPacket(_uiMaxPacket),
pConnection(_pConnection),
uiConnectionId(_uiConnectionId),
pResponseHeaders(NULL),
uiResponseHeaderLen(0),
pResponseHeadEnum(NULL),
m_Inited(TRUE)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::CObexStream()\n"));
    SVSUTIL_ASSERT(_pHeaders && pConnection && _pwcPassword);

    //get a new stream id
    uiStreamID = GetNewStreamID();

    if(pMyOBEXDevice)
        pMyOBEXDevice->AddRef();

    if(pConnection)
        pConnection->AddRef();
    if(_pwcPassword)
    {
        //make *SURE* this isnt a pointer
        ASSERT(sizeof(wcPassword) != sizeof(WCHAR *));
        StringCchCopyNW(wcPassword, _countof(wcPassword), _pwcPassword, _countof(wcPassword));
    }
    else
        wcPassword[0] = NULL;

    if(!_pHeaders)
    {
        myHeaderCollection = new CHeaderCollection();
        if( !myHeaderCollection )
             m_Inited = FALSE;
    }
    else
    {
        myHeaderCollection=_pHeaders;
        myHeaderCollection->AddRef();
    }
}

CObexStream::CObexStream(
       IObexTransportConnection *_pConnection,
       CObexDevice *_pMyOBEXDevice,
       UINT _uiMaxPacket,
       UINT _uiConnectionId,
       WCHAR *_pwcPassword,
       LPHEADERCOLLECTION *_pHeaders) :
pMyOBEXDevice(_pMyOBEXDevice),
_refCount(1),
uiBodyLen(0),
myHeaderCollection(0),
fHeaderAccounted(FALSE),
uiStreamType(OBEX_GET),
pcBodyPtr(NULL),
pcBodyChunk(NULL),
fFirstPacket(TRUE),
fReadFinished(FALSE),
fHaveSentData(FALSE),
uiMaxPacket(_uiMaxPacket),
pConnection(_pConnection),
uiConnectionId(_uiConnectionId),
pResponseHeaders(NULL),
uiResponseHeaderLen(0),
pResponseHeadEnum(NULL),
m_Inited(TRUE)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::CObexStream()\n"));
    PREFAST_ASSERT(_pHeaders && pConnection && _pwcPassword);

    //get a new stream id
    uiStreamID = GetNewStreamID();

    if(pMyOBEXDevice)
        pMyOBEXDevice->AddRef();

    if(pConnection)
        pConnection->AddRef();
    if(_pwcPassword)
    {
        //make *SURE* this isnt a pointer
        ASSERT(sizeof(wcPassword) != sizeof(WCHAR *));
        StringCchCopyNW(wcPassword, _countof(wcPassword), _pwcPassword, _countof(wcPassword));
    }


    else
        wcPassword[0] = NULL;

    if( !(*_pHeaders) )
    {
        myHeaderCollection = new CHeaderCollection();
        if( !myHeaderCollection )
             m_Inited = FALSE;
    }
    else
    {
          myHeaderCollection = *_pHeaders;
          (*_pHeaders)->AddRef();
    }

}


CObexStream::~CObexStream()
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::~CObexStream()\n"));

    //if this is a PUT and our stream is still valid
    if(uiStreamType == OBEX_PUT && !StreamDisabled())
    {
        FlushSendBuffers(OBEX_OP_PUT);
    }
    //if this is a GET, we havent finished reading, have sent a packet, and our stream
    //  id is still valid -- we must abort this connection
    else if(uiStreamType == OBEX_GET && !fReadFinished && fHaveSentData && !StreamDisabled())
    {
        HRESULT hr;
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexStream::Abort() -- sending OBEX Abort packet\n"));

        //abort the connection
        IHeaderCollection *pAbortHeaders = NULL;
        if(FAILED(hr = CoCreateInstance(__uuidof(HeaderCollection),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IHeaderCollection),
                                 (void **)&pAbortHeaders)))
            goto Done;

        if(uiConnectionId != OBEX_INVALID_CONNECTION)
            pAbortHeaders->AddConnectionId(uiConnectionId);

        UCHAR *pNewPacket = NULL;
        ULONG uiNewPackSize;

        //send off the packet
        hr = ObexSendRecvWithAuth(pConnection, uiMaxPacket, wcPassword, OBEX_OP_ABORT, 0,0, pAbortHeaders, &pNewPacket, &uiNewPackSize);

        if (FAILED(hr) || pNewPacket[0] != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexStream::Abort() -- sending abort failed... nothing we can do\n"));
        }

        if(pAbortHeaders)
            pAbortHeaders->Release();
        delete [] pNewPacket;
    }


Done:

    if(pMyOBEXDevice)
        pMyOBEXDevice->Release();
    if(pConnection)
        pConnection->Release();
    if(pcBodyChunk)
        delete [] pcBodyChunk;
    if(myHeaderCollection)
        myHeaderCollection->Release();
    if (pResponseHeaders)
        delete [] pResponseHeaders;

    if (pResponseHeadEnum) {
        pResponseHeadEnum->RemoveAll();
        delete pResponseHeadEnum;
    }
}


HRESULT
CObexStream::FlushSendBuffers(char _cOpCode)
{
    if(uiStreamType == OBEX_ERR || StreamDisabled())
    {
        if(pcBodyChunk)
            delete [] pcBodyChunk;

        pcBodyPtr = pcBodyChunk = NULL;
        uiBodyLen = 0;

        DEBUGMSG(OBEX_NETWORK_ZONE, (L" CObexStream::FlushSendBuffers -- ERROR FLAG MARKED\n"));
        return S_OK;
    }


    //send out the remaining packet
    BYTE cOpCode = ((BYTE)_cOpCode) | OBEX_OP_ISFINAL;

    unsigned char *pNewPacket = NULL;
    ULONG uiPacketSize;


    //add the connection ID
    if(uiConnectionId != OBEX_INVALID_CONNECTION)
        myHeaderCollection->AddConnectionId(uiConnectionId);

    if (pcBodyChunk) {
        myHeaderCollection->AddEndOfBody(uiBodyLen, pcBodyChunk);
        delete [] pcBodyChunk;
        pcBodyPtr = pcBodyChunk = NULL;
        uiBodyLen = 0;
    }
    else if(fHaveSentData)
    {
        ASSERT(0 == uiBodyLen);
        myHeaderCollection->AddEndOfBody(0, NULL);
    }

    //
    //  send off the packet if there is a body, grab it and put it in the chain of
    //    incoming data
    //
    HRESULT hr = ObexSendRecvWithAuthSaveHeaders(pConnection, uiMaxPacket, wcPassword, cOpCode, 0,0, myHeaderCollection, &pNewPacket, &uiPacketSize);
    if(SUCCEEDED(hr) && pNewPacket)
    {
        //
        //  parse out all interesting fields
        //
        ObexParser p (pNewPacket, uiPacketSize);

        if (((p.Op () & 0xF0) == (OBEX_STAT_CONTINUE | OBEX_OP_ISFINAL)) ||
            ((p.Op () & 0xF0) == (OBEX_STAT_OK | OBEX_OP_ISFINAL)))
        {
            while (! p.__EOF ())
            {
                if (p.IsA (OBEX_HID_BODY) || (p.IsA (OBEX_HID_BODY_END)))
                {
                    int cLen = 0;
                    unsigned char *pBuf;
                    if (! p.GetBytes (&pBuf, &cLen))
                    {
                        SVSUTIL_ASSERT(FALSE);
                        break;
                    }

                    //copy in the data (we cant have more than
                    //  we can hold)
                    if (pcBodyChunk) {
                        BYTE *pcNewChunk = new BYTE [uiBodyLen + cLen];
                        if (pcNewChunk)
                            memcpy (pcNewChunk, pcBodyPtr, uiBodyLen);

                        delete [] pcBodyChunk;
                        pcBodyChunk = pcBodyPtr = pcNewChunk;
                    } else {
                        pcBodyChunk = new BYTE[cLen];
                        pcBodyPtr = pcBodyChunk;
                    }

                    if (! pcBodyChunk)
                        return E_OUTOFMEMORY;

                    memcpy(pcBodyChunk + uiBodyLen, pBuf, cLen);
                    uiBodyLen += cLen;
                    myHeaderCollection->Release();
                    myHeaderCollection=new CHeaderCollection();
                    if( !myHeaderCollection )
                          return E_OUTOFMEMORY;

                    if (p.IsA (OBEX_HID_BODY_END) || p.Op() == (OBEX_STAT_OK | OBEX_OP_ISFINAL))
                    {
                      DEBUGMSG(OBEX_NETWORK_ZONE, (L"OBEX_HID_BODY_END reached...\n"));
                      fReadFinished = TRUE;
                    }
                }
                p.Next ();
            }
        }
        else
        {
            if(pNewPacket)
                delete [] pNewPacket;
            return E_FAIL;
        }

         //
         //  at this point we dont need the packet anymore so delete it
         //
         if(pNewPacket)
            delete [] pNewPacket;
    }
    else
    {
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE, (L"CObexStream::FlushSendBuffers -- Sending final packet failed\n"));
        return E_FAIL;
    }

    return S_OK;
}


HRESULT CObexStream::CopyResponseHeaders(unsigned char *pNewPacket, ULONG Size) {
    ASSERT(pResponseHeaders == NULL);
    ASSERT(uiResponseHeaderLen == 0);

    pResponseHeaders = new UCHAR[Size];
    if (NULL == pResponseHeaders)
        return E_OUTOFMEMORY;

    memcpy(pResponseHeaders,pNewPacket,Size);
    uiResponseHeaderLen = Size;
    return S_OK;
}


// Wraps underlying ObexSendRecvWithAuth function.  Saves the first header on a GET
// and the last header on a PUT so that client can retrieve this information later
// if needed.
HRESULT CObexStream::ObexSendRecvWithAuthSaveHeaders(
                   IObexTransportConnection *pConnection, UINT uiMaxPacket,
                   WCHAR *wcPassword, BYTE cOpCode, char *additionalDta,
                   UINT cAddnlDta, IHeaderCollection *pHeaderCollection,
                   unsigned char **pNewPacket, ULONG *pSize)
{
    fHaveSentData = TRUE;

    HRESULT hr = ObexSendRecvWithAuth(pConnection, uiMaxPacket,
                   wcPassword, cOpCode, additionalDta,
                   cAddnlDta, pHeaderCollection, pNewPacket, pSize);

    if (FAILED(hr))
        return hr;

    ObexParser p (*pNewPacket, *pSize);

    // allows client to query last response
    bLastResponseCode = (BYTE) p.Op();

    if (uiStreamType == OBEX_GET) {
        // Store headers initially for GET.  Don't parse them out at this stage
        // since the vast majority of obex clients will never need them
        if (NULL == pResponseHeaders) {
            return CopyResponseHeaders(*pNewPacket,*pSize);
        }
    }
    else if (uiStreamType == OBEX_PUT) {
        // We can receive multiple response packets during PUT session.
        // The last one server sends OBEX_STAT_OK rather than OBEX_STAT_CONTINUE.
        // These are the headers we're interested in.
        if ((p.Op () & 0xF0) == (OBEX_STAT_OK | OBEX_OP_ISFINAL)) {
            if (pResponseHeaders) {
                DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"OBEX: Obex client received more than one (OBEX_STAT_OK | OBEX_OP_ISFINAL) PUT response packets\r\n"));
                ASSERT(0); // We should only be able to get to this stage once
                return S_OK;
            }

            return CopyResponseHeaders(*pNewPacket,*pSize);
        }
    }

    return S_OK;
}


ULONG STDMETHODCALLTYPE
CObexStream::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexStream::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CObexStream::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexStream::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CObexStream::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::QueryInterface()\n"));
     if(!ppv)
        return E_POINTER;
    if(riid == IID_IUnknown)
        *ppv = this;
    else if(riid == IID_IStream)
        *ppv = static_cast<IStream*>(this);
    else if(riid == IID_IObexResponse)
        *ppv = static_cast<IObexResponse*>(this);
    else if(riid == IID_IHeaderEnum)
        *ppv = static_cast<IHeaderEnum*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}


HRESULT
CObexStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    HRESULT hr = S_OK;
    UINT cbTotalRead = 0;

    //check to make sure we CAN read on this connection
    if(!pv || uiStreamType != OBEX_GET || StreamDisabled())
    {
        DEBUGMSG(OBEX_NETWORK_ZONE, (L"Wrong connection type\n"));
        return E_FAIL;
    }

    while(cb)
    {
        ULONG cbJustRead = 0;

        if(FAILED(hr = ReadHelper(pv, cb, &cbJustRead)))
        {
            //if we have read SOMETHING, stop looping
            //  and give them S_OK.  if the stream has
            //  ended their next call will fail
            if(cbTotalRead)
                hr = S_OK;

            cbTotalRead += cbJustRead;
            goto Done;
        }

        cbTotalRead += cbJustRead;
        pv = (char *)pv + (UINT) cbJustRead;
        cb -= cbJustRead;
    }
Done:
    if(SUCCEEDED(hr))
    {
        *pcbRead = cbTotalRead;
    }
    return hr;
}


BOOL CObexStream::StreamDisabled()
{
    if(pMyOBEXDevice && GetStreamID()==pMyOBEXDevice->GetStreamID())
        return FALSE;
    else
        return TRUE;
}

HRESULT
CObexStream::ReadHelper(void *pv, ULONG cb, ULONG *pcbRead)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::Read()\n"));

    char *pDestBuf = (char *)pv;
    ULONG cbRead = 0;
    HRESULT hr = E_FAIL;

    //first see if we have enough data cached to complete the
    //  request, if not, deplete the cache and request a new
    //  packet
    if(uiBodyLen >= cb && !fFirstPacket)
    {
        memcpy(pDestBuf, pcBodyPtr, cb);
        pcBodyPtr += cb;
        uiBodyLen -= cb;
        *pcbRead = cb;
        return S_OK;
    }
    else if(uiBodyLen)
    {
        memcpy(pDestBuf, pcBodyPtr, uiBodyLen);
        pDestBuf += uiBodyLen;
        cbRead = uiBodyLen;
        cb -= uiBodyLen;
        uiBodyLen = 0;
        delete [] pcBodyChunk;
        pcBodyPtr = pcBodyChunk = NULL;

        if(fReadFinished)
        {
            *pcbRead = cbRead;
            return S_OK;
        }
    }

    //if there is no data, AND we are finished, quit
    if((! uiBodyLen) && fReadFinished)
    {
        DEBUGMSG(OBEX_NETWORK_ZONE, (L"-->No more data left\n"));
        return E_FAIL;
    }

    //account for the header size upfront
    if(!fHeaderAccounted)
    {
        if(uiConnectionId != OBEX_INVALID_CONNECTION)
            myHeaderCollection->AddConnectionId(uiConnectionId);

        fHeaderAccounted = TRUE;
    }


    //if we are sending off the first packet, there might be too much data
    //  in the headers (BODY being really large)... so we use the helper function
    //  WriteAll to help us out
    if(fFirstPacket)
    {
        fFirstPacket = FALSE;

        //if the data is too big to fit in one packet (this would most likely be
        //  be because the BODY field is really large... remove
        //  the BODY from the packet, and send it off to WriteAll (with
        //  the BODY data as the value to write... WriteAll will send
        //  the data as a BODY...  NOTE: WriteAll will break the request up
        //  if necessary (for example if there are so many headers that
        //  multiple packets must be sent)
        if(SizeOfHeader(myHeaderCollection) >= uiMaxPacket)
        {
            OBEX_HEADER *myHeader;
            ULONG ulFetched;
            LPHEADERENUM pHeaderEnum;
            ULONG ulDataSize = 0;
            ULONG ulWritten = 0;
            BYTE *pData = NULL;

            //
            //  get a copy of the BODY portion of the headers
            //
            myHeaderCollection->EnumHeaders(&pHeaderEnum);
            while(SUCCEEDED(pHeaderEnum->Next(1, &myHeader, &ulFetched)))
            {
                if(myHeader->bId == OBEX_HID_BODY)
                {
                    pData = new BYTE[myHeader->value.ba.dwSize];
                    ulDataSize = myHeader->value.ba.dwSize;

                    if(!pData)
                        return E_FAIL;

                    memcpy(pData, myHeader->value.ba.pbaData, ulDataSize);
                    break;
                }
            }
            pHeaderEnum->Release();


            //now make a call to WriteAll to packetize the data and send it out
            //  if there is no data, fall through and just run as normal
            if(pData && ulDataSize)
            {
                myHeaderCollection->Remove(OBEX_HID_BODY);
                hr = WriteAll(OBEX_OP_GET, pData, ulDataSize, &ulWritten);
                if(FAILED(hr))
                {
                    DEBUGMSG(OBEX_NETWORK_ZONE, (L"Write All in first packet of Write FAILED!\n"));
                    return hr;
                }

                hr = FlushSendBuffers(OBEX_OP_GET);
                if(FAILED(hr))
                {
                    DEBUGMSG(OBEX_NETWORK_ZONE, (L"FlushSendBuffer in first packet of Write FAILED!\n"));
                    return hr;
                }

                //
                //  because we already have recieved an answer (and have sent a packet)
                //    just skip to calling read again
                //
                goto CallReadAgain;
            }

            //if there is no body, we know that the headers are greater than one
            //  packet BUT there is no body.  simply call WriteAll passing
            //  in no body with no size
            else
            {
                hr = WriteAll(OBEX_OP_GET, 0, 0, &ulWritten);

                if(FAILED(hr))
                {
                    DEBUGMSG(OBEX_NETWORK_ZONE, (L"WriteAll() in first packet of large headers with no body FAILED!\n"));
                    return hr;
                }

                hr = FlushSendBuffers(OBEX_OP_GET);
                if(FAILED(hr))
                {
                    DEBUGMSG(OBEX_NETWORK_ZONE, (L"FlushSendBuffer in first packet of large headers with no body FAILED!\n"));
                    return hr;
                }

                goto CallReadAgain;
            }
         }
    }

    unsigned char *ucIncomingPacket;
    ULONG iRespSize;

    //send off the packet and get its answer
    hr = ObexSendRecvWithAuthSaveHeaders(pConnection, uiMaxPacket, wcPassword, OBEX_OP_GET | OBEX_OP_ISFINAL, 0,0, myHeaderCollection, &ucIncomingPacket, &iRespSize);

    if(SUCCEEDED(hr))
    {
                //clean out the headers we sent
                myHeaderCollection->Release();
              myHeaderCollection=new CHeaderCollection();
              if( !myHeaderCollection )
                  return E_OUTOFMEMORY;

                fHeaderAccounted = FALSE;

        //parse out all interesting fields
        ObexParser p (ucIncomingPacket, iRespSize);

        if (((p.Op () & 0xF0) == (OBEX_STAT_CONTINUE | OBEX_OP_ISFINAL)) ||
            ((p.Op () & 0xF0) == (OBEX_STAT_OK | OBEX_OP_ISFINAL)))
        {
            while (! p.__EOF ())
            {
                if (p.IsA (OBEX_HID_BODY) || (p.IsA (OBEX_HID_BODY_END)))
                {
                    int cLen = 0;
                    unsigned char *pBuf;
                    if (! p.GetBytes (&pBuf, &cLen))
                    {
                        SVSUTIL_ASSERT(FALSE);
                        break;
                    }

                    //copy in the data (we cant have more than
                    //  we can hold)
                    if (pcBodyChunk && cLen) {
                        BYTE *pcNewChunk = new BYTE [uiBodyLen + cLen];
                        if( !pcNewChunk )
                        {
                            delete [] pcBodyChunk;
                            pcBodyChunk = pcBodyPtr = NULL;

                            if(ucIncomingPacket)
                                 delete [] ucIncomingPacket;

                            return E_OUTOFMEMORY;
                        }

                        memcpy (pcNewChunk, pcBodyPtr, uiBodyLen);
                        delete [] pcBodyChunk;
                        pcBodyChunk = pcBodyPtr = pcNewChunk;
                    } else if(cLen){
                        pcBodyChunk = new BYTE[cLen];
                        pcBodyPtr = pcBodyChunk;
                    }

                    if (! pcBodyChunk && cLen)
                        return E_OUTOFMEMORY;

                    if(cLen)
                    {
                        memcpy(pcBodyChunk + uiBodyLen, pBuf, cLen);
                        uiBodyLen += cLen;
                    }

                    if (p.IsA (OBEX_HID_BODY_END))
                    {
                      DEBUGMSG(OBEX_NETWORK_ZONE, (L"OBEX_HID_BODY_END reached...\n"));
                      fReadFinished = TRUE;
                    }
                }
                p.Next ();
            }

            if (p.Op () == (OBEX_STAT_OK | OBEX_OP_ISFINAL))
            {
              // OK + FINAL indicates the last packet of this response
              DEBUGMSG(OBEX_NETWORK_ZONE, (L"OBEX_STAT_OK/Final response received...\n"));
              fReadFinished = TRUE;
            }

            //at this point we dont need the packet anymore so delete it
            if(ucIncomingPacket)
                delete [] ucIncomingPacket;
        }
        else
        {
            if(ucIncomingPacket)
                delete [] ucIncomingPacket;
            return E_FAIL;
        }
    }
    else
        return E_FAIL;


CallReadAgain:

    *pcbRead = cbRead;
    return S_OK;
}


//CObexStream::Write -- just do a sanity check for OBEX_PUT
//  and setup the opCode to be OBEX_OP_PUT (this is done because
//  the WriteAll function is used by Get's where the opcode
//  is different
HRESULT STDMETHODCALLTYPE
CObexStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::Write() -- %d bytes\n", cb));
    HRESULT hr = E_FAIL;

     if(uiStreamType != OBEX_PUT || StreamDisabled())
     {
        return E_FAIL;
     }

     hr = WriteAll(OBEX_OP_PUT, pv, cb, pcbWritten);

     if(FAILED(hr))
     {
         DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::Write() -- failed.  cutting off stream!\n"));
         uiStreamType = OBEX_ERR;
     }
     else
     {
        ASSERT(*pcbWritten == cb);
     }
     return hr;
}


//WriteAllHeaders is a utility used by WriteAll for transmitting a set of
//  user passed in headers that are bigger than one packet.  It returns when the
//  headers in myHeaderCollection are smaller in size than one OBEX packet
HRESULT
CObexStream::WriteAllHeaders(BYTE cOpCode, UINT uiMINPackSize)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- cant fit all headers into one packet -- breaking into multiple parts!"));

    //
    //  While the packet is too large to contain
    //     BODY's send off as many of the headers
    //       as will fit in a packet
    //
    HRESULT hr = E_FAIL;
    BYTE cOp;
    CHeaderCollection *pNewHeaderCollection = new CHeaderCollection();
    if( !pNewHeaderCollection )
        return E_OUTOFMEMORY;

    IHeaderEnum *pHeaderEnum = NULL;
    OBEX_HEADER *myHeader = NULL;
    ULONG ulFetched;
    UINT uiHeaderCount = 0;

    myHeaderCollection->EnumHeaders(&pHeaderEnum);
    while(SUCCEEDED((hr = pHeaderEnum->Next(1, &myHeader, &ulFetched))) && ulFetched)
    {
        SVSUTIL_ASSERT(pHeaderEnum && pNewHeaderCollection);

        cOp = myHeader->bId;

        //copy the header and insert it into the new list (note: the collection
        //  takes ownership
        OBEX_HEADER *pNewHeader = new OBEX_HEADER();
        if( !pNewHeader )
        {
              hr = E_OUTOFMEMORY;
             goto Done;
        }

        CHeaderCollection::CopyHeader(myHeader,pNewHeader);
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- Adding 0x%x to temp pNewHeaderCollectoin!", pNewHeader->bId));
        if(FAILED(hr = pNewHeaderCollection->InsertHeader(pNewHeader)))
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- Error inserting new header into list!"));
            goto Done;
        }

        uiHeaderCount ++;
        //
        //  If this header throws us over the edge in size, remove it
        //        and send off the header, otherwise, move (ie transfer) it from
        //        'myHeaderCollection' to 'pNewHeaderCollection'
        //
        if(uiMINPackSize + SizeOfHeader(pNewHeaderCollection) >= uiMaxPacket)
        {
            //remove the older header (note: this just removes it from the
            //  newheader collection and *NOT* the old one!)
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- Removing 0x%x from temp pNewHeaderCollectoin!", cOp));
            pNewHeaderCollection->Remove(cOp);
            uiHeaderCount --;

            //if there are NO headers, that means the one we just added is too
            //  big for one packet, so we are forced to quit... :(
            if(!uiHeaderCount)
            {
                DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- one of our headers is too large -- cant send packet!"));
                hr = E_FAIL;
                goto Done;
            }

            //add a connectionID (if required)
            if(uiConnectionId != OBEX_INVALID_CONNECTION)
                pNewHeaderCollection->AddConnectionId(uiConnectionId);

            //send out the packet, and grab the response
            unsigned char *pNewPacket;
            ULONG uiIPackSize;
            if(SUCCEEDED(hr = ObexSendRecvWithAuthSaveHeaders(pConnection, uiMaxPacket, wcPassword, cOpCode, 0,0, pNewHeaderCollection, &pNewPacket, &uiIPackSize)))
            {
                //parse out all interesting fields
                ObexParser p (pNewPacket, uiIPackSize);

                //get the command data
                if (p.Op() != 0x90)
                {
                    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- recieved an invalid return code! : op -- %d\n", p.Op()));
                    hr = E_FAIL;
                }
                else
                    hr = S_OK;

                //cleanup & init everything to be ready for a new packet
                delete [] pNewPacket;
                pNewHeaderCollection->Release();
                pNewHeaderCollection = new CHeaderCollection();
                if( !pNewHeaderCollection )
                {
                     hr = E_OUTOFMEMORY;
                     goto Done;
                }
                uiHeaderCount = 0;

                //reset the enumerator (so we can start where we left off)
                pHeaderEnum->Release();
                pHeaderEnum = 0;
                myHeaderCollection->EnumHeaders(&pHeaderEnum);


                if(FAILED(hr))
                    goto Done;

            }
            else
            {
                DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() --- ObexSendRecvWithAuth() failed!\n"));
                goto Done;
            }
        }
        else
        {
            //
            //we have moved the header to our new location... so remove it from
            //  the origin (thereby transfering the bag)
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- Removing 0x%x from temp pHeaderCollection!", cOp));
            myHeaderCollection->Remove(cOp);
        }


        //
        //    Recompute size of main header collection, if its too big loop again
        //    if it fits in one packet, break out of the loop
        UINT uiPacketSize = uiMINPackSize + SizeOfHeader(myHeaderCollection);


        if(!uiHeaderCount && 3 + uiPacketSize <= uiMaxPacket)
           break;
        //
        //  Flush the buffer
        //
        else if(uiHeaderCount && 3 + uiPacketSize <= uiMaxPacket)
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- down to a managable header size, flushing temp buffer and going on as usual\n"));

            //add a connectionID (if required)
            if(uiConnectionId != OBEX_INVALID_CONNECTION)
                pNewHeaderCollection->AddConnectionId(uiConnectionId);

            //send out the packet, and grab the response
            unsigned char *pNewPacket;
            ULONG uiIPackSize;
            if(SUCCEEDED(hr = ObexSendRecvWithAuthSaveHeaders(pConnection, uiMaxPacket, wcPassword, cOpCode, 0,0, pNewHeaderCollection, &pNewPacket, &uiIPackSize)))
            {
                //parse out all interesting fields
                ObexParser p (pNewPacket, uiIPackSize);

                //get the command data
                if (p.Op() != 0x90)
                {
                    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- recieved an invalid return code! : op -- %d\n", p.Op()));
                    hr = E_FAIL;
                }
                else
                    hr = S_OK;

                //cleanup & init everything to be ready for a new packet
                delete [] pNewPacket;
                pNewHeaderCollection->Release();
                pNewHeaderCollection = NULL;
                uiHeaderCount = 0;

                if(FAILED(hr))
                    goto Done;

            }
            else
            {
                DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() --- ObexSendRecvWithAuth() failed!\n"));
                hr = E_FAIL;
                goto Done;
            }

            break;
        }
    }

Done:
    if(pHeaderEnum)
        pHeaderEnum->Release();
    if(pNewHeaderCollection)
        pNewHeaderCollection->Release();

    return hr;
}

HRESULT STDMETHODCALLTYPE
CObexStream::WriteAll(char cOpCode, const void *pv, ULONG cb, ULONG *pcbWritten)
{
    HRESULT hr = S_OK;
    UINT cbTotalWritten = 0;
    if(!cb) {
        ULONG cbJustWritten = 0;
        if(FAILED(hr = WriteAllHelper((BYTE)cOpCode, pv, cb, &cbJustWritten)))
           goto Done;
        ASSERT(0 == cbJustWritten);
    }
    else {
        while(cb)
        {
            ULONG cbJustWritten = 0;
            if(FAILED(hr = WriteAllHelper((BYTE)cOpCode, pv, cb, &cbJustWritten)))
                goto Done;

            cbTotalWritten += cbJustWritten;
            pv = (char *)pv + (UINT) cbJustWritten;
            cb -= cbJustWritten;
        }
    }
Done:
    if(SUCCEEDED(hr))
    {
        *pcbWritten = cbTotalWritten;
    }


    return hr;
}

HRESULT
CObexStream::WriteAllHelper(BYTE cOpCode, const void *pv, ULONG cb, ULONG *pcbWritten)
{
    PREFAST_ASSERT(myHeaderCollection);
    PREFAST_ASSERT(pcbWritten);

    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- %d bytes\n", cb));

    UINT uiPacketSize = 0;
    UINT toWrite = cb;  // <-- make the first attempt the entire buffer
    UINT uiMINPackSize = MIN_PUT_PACKET_SIZE;
    UINT uiTemp; //holder for temp comparisons
    HRESULT hr = S_OK;

    *pcbWritten = 0;

     //determine the minimum packet we can transmit
     //   (5 bytes to transfer a connection ID)
     //   also remove a previous connection ID (if one exists)
     if(OBEX_INVALID_CONNECTION != uiConnectionId)
        uiMINPackSize += 5;
     myHeaderCollection->Remove(OBEX_HID_CONNECTIONID);

     //figure out the size of the packet (as it currently is...)
     if(FAILED(UIntAdd(uiMINPackSize, SizeOfHeader(myHeaderCollection), &uiPacketSize))||
        FAILED(UIntAdd(uiPacketSize, 3, &uiTemp))) {
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- WriteAllHeaders overflow of uiPacketSize!"));
        goto Done;
     }

     //if the packet is too large without any BODY send off a few packets
     //  before attaching a body
     if(uiTemp >= uiMaxPacket)
     {
         DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- cant fit all headers into one packet -- breaking into multiple parts!"));
         if(FAILED(hr = WriteAllHeaders(cOpCode, uiMINPackSize)))
         {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- WriteAllHeaders failed!"));
            goto Done;
         }
         SVSUTIL_ASSERT(uiMaxPacket >= uiMINPackSize + SizeOfHeader(myHeaderCollection));

         //recompute the packet size after sending as many headers as possible
         if(FAILED(UIntAdd(uiMINPackSize, SizeOfHeader(myHeaderCollection), &uiPacketSize))) {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- WriteAllHeaders overflow of uiPacketSize!"));
            goto Done;
         }
    }

    //
    //  figure out how much non header data we can put into one packet
    //
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- toWrite(%d) uiBodyLen(%d) uiPacketSize(%d) maxPacket(%d)\n", toWrite, uiBodyLen, uiPacketSize, uiMaxPacket));
    UINT uiTotalSize = 0;

    if(FAILED(UIntAdd(3, toWrite, &uiTotalSize)) ||
       FAILED(UIntAdd(uiBodyLen, uiTotalSize, &uiTotalSize)) ||
       FAILED(UIntAdd(uiPacketSize, uiTotalSize, &uiTotalSize))) {
            ASSERT(FALSE);
            hr = E_INVALIDARG;
            goto Done;
    }


    if (uiTotalSize > uiMaxPacket)
    {
        SVSUTIL_ASSERT(uiMaxPacket >= (uiBodyLen + 3 + uiPacketSize));
        toWrite = uiMaxPacket - uiBodyLen - 3 - uiPacketSize;
    }
    else
    {
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- oooh.  packet fits nicely.  we can cache it away for later. -- will be caching: %d bytes", toWrite + uiBodyLen));
    }


    //
    //  fill pcBodyChunk with data
    //
    if (pcBodyChunk && toWrite)
    {
        //since we have old cached data, put that in first, then as much of
        //  the new data as possible
        BYTE *pcNewChunk = new BYTE [uiBodyLen + toWrite];
        if (pcNewChunk)
            memcpy (pcNewChunk, pcBodyPtr, uiBodyLen);
        else
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- out of memory\n"));
            hr = E_OUTOFMEMORY;
            goto Done;
        }

        delete [] pcBodyChunk;
        pcBodyChunk = pcBodyPtr = pcNewChunk;

        //copy in new data
        memcpy(pcBodyChunk + uiBodyLen, pv, toWrite);
        uiBodyLen += toWrite;
    }
    else if(toWrite)
    {
        //if nothing is cached, simply
        SVSUTIL_ASSERT (uiBodyLen == 0);
        pcBodyChunk = new BYTE[toWrite];
        pcBodyPtr = pcBodyChunk;

        if (! pcBodyChunk)
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- out of memory\n"));
            hr = E_OUTOFMEMORY;
            goto Done;
        }

        //copy in new data
        memcpy(pcBodyChunk + uiBodyLen, pv, toWrite);
        uiBodyLen = toWrite;
    }


    unsigned char *pNewPacket;
    ULONG uiIPackSize;

    //the size is already accounted for
    myHeaderCollection->AddBody(uiBodyLen, pcBodyChunk);
    delete [] pcBodyChunk;
    uiBodyLen = 0;
    pcBodyChunk = pcBodyPtr = NULL;

    //add (if any) connection ID
    if(uiConnectionId != OBEX_INVALID_CONNECTION)
        myHeaderCollection->AddConnectionId(uiConnectionId);

    //send off the packet
    if(SUCCEEDED(hr = ObexSendRecvWithAuthSaveHeaders(pConnection, uiMaxPacket, wcPassword, cOpCode, 0,0, myHeaderCollection, &pNewPacket, &uiIPackSize)))
    {
        //parse out all interesting fields
        ObexParser p (pNewPacket, uiIPackSize);

        //get the command data
        if (p.Op() != 0x90 && p.Op() != 0xA0)
            hr = E_FAIL;
        else
            hr = S_OK;

        //cleanup & init everything to be ready for a new packet
        delete [] pNewPacket;
        myHeaderCollection->Release();
        myHeaderCollection = new CHeaderCollection();
        if( !myHeaderCollection )
        {
                hr = E_OUTOFMEMORY;
                goto Done;
        }

        if(SUCCEEDED(hr))
        {
            *pcbWritten = toWrite;
            goto Done;
        }
        else
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- INVALID OP CODE (not continue or okay)!\n"));
            goto Done;
        }
    }
    else
    {
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::WriteAll() -- ObexSendRecv FAILED!\n"));
        hr = E_FAIL;
        goto Done;
    }

Done:
    return hr;
}




HRESULT STDMETHODCALLTYPE
CObexStream::Seek(LARGE_INTEGER dlibMove,          //Offset relative to dwOrigin
                  DWORD dwOrigin,                  //Specifies the origin for the offset
                  ULARGE_INTEGER *plibNewPosition  //Pointer to location containing
                  // new seek pointer
                  )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexStream::SetSize(ULARGE_INTEGER libNewSize  //Specifies the new size of the stream object
                     )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexStream::CopyTo(
                    IStream *pstm,               //Pointer to the destination stream
                    ULARGE_INTEGER cb,           //Specifies the number of bytes to copy
                    ULARGE_INTEGER *pcbRead,     //Pointer to the actual number of bytes
                    // read from the source
                    ULARGE_INTEGER *pcbWritten   //Pointer to the actual number of
                    // bytes written to the destination
                    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexStream::Commit(DWORD grfCommitFlags)
{
    HRESULT hr = S_OK;
    //
    // If the stream is still active and is a PUT, flush it out
    //    this will allow the client application to read errors
    //    then render the stream worthless (at this point the END_BODY has
    //    been sent)... NOTE: since only one packet can possibly be put into
    //    queues we dont need to loop
    if(uiStreamType == OBEX_PUT && !StreamDisabled()) {
        hr = FlushSendBuffers(OBEX_OP_PUT);
        uiStreamType = OBEX_ERR;
        ASSERT(0 == uiBodyLen);
        ASSERT(NULL == pcBodyChunk);
        ASSERT(NULL == pcBodyPtr);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE
CObexStream::Revert(void)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE
CObexStream::LockRegion(ULARGE_INTEGER libOffset,  //Specifies the byte offset for
                        // the beginning of the range
                        ULARGE_INTEGER cb,         //Specifies the length of the range in bytes
                        DWORD dwLockType           //Specifies the restriction on
                        // accessing the specified range
                        )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexStream::UnlockRegion(ULARGE_INTEGER libOffset,  //Specifies the byte offset for
                          // the beginning of the range
                          ULARGE_INTEGER cb,         //Specifies the length of the range in bytes
                          DWORD dwLockType           //Specifies the access restriction
                          // previously placed on the range
                          )
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE
CObexStream::Stat(STATSTG *pstatstg,  //Location for STATSTG structure
                  DWORD grfStatFlag   //Values taken from the STATFLAG enumeration
                  )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexStream::Clone(IStream **ppstm  //Pointer to location for pointer to the new stream object
                   )
{
    return E_NOTIMPL;
}


//
//  IObexResponse Interface
//
HRESULT STDMETHODCALLTYPE
CObexStream::GetLastResponseCode(BYTE *pResponseCode)
{
    // We haven't performed first req<->resp yet, so no data to send.
    if (! fHaveSentData)
        return E_FAIL;

    *pResponseCode = bLastResponseCode;
    return S_OK;
}

//
// IHeaderEnum Interface
//

// First time this is called, if raw Obex headers are available (in pResponseHeaders)
// then fill out pResponseHeadEnum with appropriate header data
HRESULT CObexStream::SetupHeaderEnum(void) {
    // We've already performed the parsing operation
    if (pResponseHeadEnum)
        return S_OK;

    if (pResponseHeaders == NULL) {
        DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"OBEX: Attempt to access response headers was made, but headers not available yet\r\n"));
        return E_FAIL;
    }

    HRESULT hr = E_OUTOFMEMORY;

    // Allocate and parse headers
    pResponseHeadEnum = new CHeaderEnum;
    if( !pResponseHeadEnum )
        return E_OUTOFMEMORY;

    ObexParser p(pResponseHeaders,uiResponseHeaderLen);
    OBEX_HEADER *pNewHeader = NULL;

    while (! p.__EOF ()) {
        pNewHeader = new OBEX_HEADER;
        if (pNewHeader == NULL)
            goto done;

        memset(pNewHeader,0,sizeof(*pNewHeader));
        pNewHeader->bId = (BYTE)p.Code();

        switch (p.Type ()) {
        case OBEX_TYPE_UNICODE:
            if (! p.GetString(&pNewHeader->value.pszData))
                goto done;
        break;

        case OBEX_TYPE_BYTESEQ:
            if (! p.GetBytes((void**)&pNewHeader->value.ba.pbaData))
                goto done;

            pNewHeader->value.ba.dwSize = p.Length();
        break;

        case OBEX_TYPE_BYTE:
            if (! p.GetBYTE (&pNewHeader->value.bData))
                goto done;
        break;

        case OBEX_TYPE_DWORD:
            if (! p.GetDWORD (&pNewHeader->value.dwData))
                goto done;
            break;

        default:
            ASSERT(0);
            hr = E_FAIL;
            goto done;
        }

        pResponseHeadEnum->InsertBack(pNewHeader);
        pNewHeader = NULL;

        if (! p.Next ())
            break;
    }

    hr = S_OK;
done:
    if (FAILED(hr)) {
        if (pResponseHeadEnum) {
            pResponseHeadEnum->RemoveAll();
            delete pResponseHeadEnum;
            pResponseHeadEnum = NULL;
        }
        if (pNewHeader)
            delete pNewHeader;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE
CObexStream::Next(ULONG celt, OBEX_HEADER **rgelt, ULONG *pceltFetched)
{
    if (FAILED(SetupHeaderEnum()))
        return E_FAIL;

    return pResponseHeadEnum->Next(celt,rgelt,pceltFetched);
}

HRESULT STDMETHODCALLTYPE
CObexStream::Skip(ULONG celt)
{
    if (FAILED(SetupHeaderEnum()))
        return E_FAIL;

    return pResponseHeadEnum->Skip(celt);
}

HRESULT STDMETHODCALLTYPE
CObexStream::Reset(void)
{
    if (FAILED(SetupHeaderEnum()))
        return E_FAIL;

    return pResponseHeadEnum->Reset();
}

HRESULT STDMETHODCALLTYPE
CObexStream::Clone(IHeaderEnum **ppenum)
{
    if (FAILED(SetupHeaderEnum()))
        return E_FAIL;

    return pResponseHeadEnum->Clone(ppenum);
}


