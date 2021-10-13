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
// ObexDevice.cpp: implementation of the CObexDevice class.
//
//////////////////////////////////////////////////////////////////////


//#ERROR -- use fixed memory
#include "common.h"
#include "ObexDevice.h"
#include "ObexParser.h"
#include "ObexStream.h"
#include "ObexPacketInfo.h"
#include "HeaderEnum.h"
#include "HeaderCollection.h"

#include "ObexStrings.h"
#include <MD5.h>
#include <intsafe.h>


void MD5(void *dest, void *orig, int len)
{
    MD5_CTX context;
    MD5Init(&context);
    MD5Update(&context, (UCHAR *)orig, len);
    MD5Final(&context);
    memcpy(dest, context.digest, 16);
}


#if defined(DEBUG) || defined(_DEBUG)
#define SVSLOG_BPR 8
void DumpBuff (unsigned char *lpBuffer, unsigned int cBuffer)
{
    WCHAR szLine[5 + 7 + 2 + 4 * SVSLOG_BPR];

    for (int i = 0 ; i < (int)cBuffer ; i += SVSLOG_BPR) {
        int bpr = cBuffer - i;
        if (bpr > SVSLOG_BPR)
            bpr = SVSLOG_BPR;

        StringCchPrintfW (szLine, _countof(szLine), L"%04x ", i);
        WCHAR *p = szLine + wcslen (szLine);

        for (int j = 0 ; j < bpr ; ++j) {
            WCHAR c = (lpBuffer[i + j] >> 4) & 0xf;
            if (c > 9) c += L'a' - 10; else c += L'0';
            *p++ = c;
            c = lpBuffer[i + j] & 0xf;
            if (c > 9) c += L'a' - 10; else c += L'0';
            *p++ = c;
            *p++ = L' ';
        }

        for ( ; j < SVSLOG_BPR ; ++j) {
            *p++ = L' ';
            *p++ = L' ';
            *p++ = L' ';
        }

        *p++ = L' ';
        *p++ = L' ';
        *p++ = L' ';
        *p++ = L'|';
        *p++ = L' ';
        *p++ = L' ';
        *p++ = L' ';

        for (j = 0 ; j < bpr ; ++j) {
            WCHAR c = lpBuffer[i + j];
            if ((c < L' ') || (c >= 127))
                c = L'.';

            *p++ = c;
        }

        for ( ; j < SVSLOG_BPR ; ++j)
            *p++ = L' ';

        *p++ = L'\n';
        *p++ = L'\0';

        SVSUTIL_ASSERT (p == szLine + sizeof(szLine)/sizeof(szLine[0]));
        DEBUGMSG(OBEX_DUMP_PACKETS_ZONE, (L"%s", szLine));
    }
}
#endif

//////////////////////////////////////////////////////////////////////
// Utility section
//////////////////////////////////////////////////////////////////////

static HRESULT MakeResponse (const WCHAR *_pszPassToUse, BYTE *bChallenge, BYTE *bResponse) {
    INT cp = CP_UTF8;

    UCHAR ucPassLen = (UCHAR)WideCharToMultiByte(cp, 0, _pszPassToUse, -1, NULL, 0, NULL, NULL);

    if (! ucPassLen)
    {
        cp = CP_ACP;
        ucPassLen = (UCHAR)WideCharToMultiByte(cp, 0, _pszPassToUse, -1, NULL, 0, NULL, NULL);
    }

    if (! ucPassLen)
        return E_FAIL;

    UINT uiEncodeSize = 16 + 1 + ucPassLen;
    BYTE bEncodeBuffer[512];
    BYTE *bToEncode = (uiEncodeSize > sizeof(bEncodeBuffer)) ? new BYTE[uiEncodeSize] : bEncodeBuffer;

    if (! bToEncode)
        return E_FAIL;

    memcpy (bToEncode, bChallenge, 16);

    bToEncode[16] = ':';

    HRESULT hr = E_FAIL;

    if (WideCharToMultiByte (cp, 0, _pszPassToUse, -1, (char *)(bToEncode + 17), ucPassLen, NULL, NULL))
    {
        MD5(bResponse, bToEncode, uiEncodeSize - 1);    // -1 because we don't want terminating '\0'
        hr = ERROR_SUCCESS;
    }

    if (bToEncode != bEncodeBuffer)
        delete [] bToEncode;

    return ERROR_SUCCESS;
}

static HRESULT MakeNonce (const WCHAR *_pszPassToUse, BYTE *bMyNonce)
{
    SYSTEMTIME st;
    GetSystemTime(&st);

    INT cp = CP_UTF8;

    UCHAR ucPassLen = (UCHAR)WideCharToMultiByte(cp, 0, _pszPassToUse, -1, NULL, 0, NULL, NULL);

    if (! ucPassLen)
    {
        cp = CP_ACP;
        ucPassLen = (UCHAR)WideCharToMultiByte(cp, 0, _pszPassToUse, -1, NULL, 0, NULL, NULL);
    }

    if (! ucPassLen)
        return E_FAIL;

    UINT uiEncodeSize = 16 + 1 + ucPassLen;
    BYTE bEncodeBuffer[512];
    size_t bToEncodeSize = 0;
    BYTE *bToEncode = NULL;

    if (uiEncodeSize > sizeof(bEncodeBuffer))
    {
        bToEncode = new BYTE[uiEncodeSize];
        bToEncodeSize = uiEncodeSize;
    }
    else
    {
        bToEncode = bEncodeBuffer;
        bToEncodeSize = sizeof(bEncodeBuffer);
    }

    if (! bToEncode)
        return E_FAIL;

    //16 bytes plus NULL ... so its okay to directly write in bToEncode... the NULL will be replaced!
    StringCbPrintfA((char *)bToEncode, bToEncodeSize, "%4.4d%2.2d%2.2dT%2.2d%2.2d%2.2dZ", st.wYear,st.wMonth,st.wDay, st.wHour,st.wMinute,st.wSecond);
    SVSUTIL_ASSERT(16 == strlen((char *)bToEncode));

    bToEncode[16] = ':';

    HRESULT hr = E_FAIL;

    if (WideCharToMultiByte (cp, 0, _pszPassToUse, -1, (char *)(bToEncode + 17), ucPassLen, NULL, NULL))
    {
        MD5(bMyNonce, bToEncode, uiEncodeSize - 1);    // -1 because we don't want terminating '\0'
        hr = ERROR_SUCCESS;
    }

    if (bToEncode != bEncodeBuffer)
        delete [] bToEncode;

    return hr;
}



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CObexDevice::CObexDevice(IPropertyBag *_pPropBag, CLSID clsidTrans) :
    _refCount(1),
    pConnection(0),
    pSocket(0),
    uiConnectionId(OBEX_INVALID_CONNECTION),
    uiMaxPacket(255 - OBEX_AUTH_HEADER_SIZE), //the min packet size for OBEX is 255... this value can get
                                                // renegotiated during CONNECT
    pPropBag(_pPropBag),
    iPresent(g_iCredits),
    iModified(g_iCredits),
    uiUpdateStatus(0),
    uiActiveStreamID(0xFFFFFFFF)
{
    wcPassword[0] = '\0';
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CObexDevice() -- propbag: 0x%x -- me: 0x%x\n",(int)_pPropBag, (int)this));
    PREFAST_ASSERT(_pPropBag);
    _pPropBag->AddRef();
    clsidTransport = clsidTrans;
}

CObexDevice::~CObexDevice()
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::~CObexDevice() -- me: 0x%x\n", (int)this));

    if(pConnection)
        pConnection->Release();
    if(pPropBag)
        pPropBag->Release();
    if(pSocket)
        pSocket->Release();
}


HRESULT STDMETHODCALLTYPE
CObexDevice::Disconnect(LPHEADERCOLLECTION pHeaders)
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Disconnect()\n"));
    PREFAST_ASSERT(pHeaders);

    //first if there is an ongoing stream, deactiveate it
    uiActiveStreamID = 0xFFFFFFFF;

    //make sure we have a connection ID
    if(uiConnectionId != OBEX_INVALID_CONNECTION)
        pHeaders->AddConnectionId(uiConnectionId);

    HRESULT hRes = E_FAIL;
    if(pHeaders)
    {
        UCHAR *pNewPacket = NULL;
        ULONG uiNewPackSize;

        //send off the packet
        hRes = ObexSendRecv(pConnection, uiMaxPacket, OBEX_OP_DISCONNECT, 0,0, pHeaders, &pNewPacket, &uiNewPackSize);

        if (FAILED(hRes) || pNewPacket[0] != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
            hRes = E_FAIL;
        else
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Disconnect() -- failed?  socket went down?\n"));
        }

        if(pNewPacket)
            delete [] pNewPacket;
    }
    return hRes;
}


HRESULT STDMETHODCALLTYPE
CObexDevice::Get(LPHEADERCOLLECTION pHeaders, IStream **pStream)
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Get()\n"));

    //if a get is requested the old stream is marked invalid
    //  note: this will be replaced below once the stream is created
    uiActiveStreamID =0xFFFFFFFF;

    //if we have an obex device but dont have a connection, they have not
    //  used a connection point (and thus have not 'BoundToDevice'...
    //  do the transport connection for them
    if(!pConnection && FAILED(ConnectSocket()))
        return E_FAIL;



    //return an obex stream
    CObexStream *pOBEXStream = new CObexStream(pConnection, this, uiMaxPacket, uiConnectionId, wcPassword, &pHeaders);

    if(pOBEXStream && pOBEXStream->IsInited())
    {
        *pStream = pOBEXStream;

        uiActiveStreamID = pOBEXStream->GetStreamID();
        return S_OK;
    }
    else if(pOBEXStream)
    {
         //pOBEXStream fails to initialize
         delete pOBEXStream;
         return E_OUTOFMEMORY;
    }
    else
        return E_OUTOFMEMORY;
}


HRESULT STDMETHODCALLTYPE
CObexDevice::Put(LPHEADERCOLLECTION pHeaders, IStream **pStream)
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Put()\n"));

     //
     //  Make sure they arent trying to squeeze in a BODY or BODY_END
     //     with our interfaces, this is an error... they are using a
     //        STREAM and by definition that uses the BODY/BODY_END fields.
     //
     IHeaderEnum *pHeaderEnum;
     OBEX_HEADER *myHeader;
     ULONG ulFetched;

     //if a Put is requested the old stream is not used anymore -- so release it
     uiActiveStreamID = 0xFFFFFFFF;

     pHeaders->EnumHeaders(&pHeaderEnum);
     while(SUCCEEDED(pHeaderEnum->Next(1, &myHeader, &ulFetched)))
     {
        if(myHeader->bId == OBEX_HID_BODY || myHeader->bId == OBEX_HID_BODY_END)
        {
            DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexStream::Write() -- failed.  CANT use BODY/BODY_END with a PUT!"));
            pHeaderEnum->Release();
            return E_FAIL;
        }
     }
     pHeaderEnum->Release();


    //if we have an obex device but dont have a connection, they have not
    //  used a connection point (and thus have not 'BoundToDevice'...
    //  do the transport connection for them
    if(!pConnection && FAILED(ConnectSocket()))
        return E_FAIL;



    //return an obex stream
    CObexStream *pOBEXStream = new CObexStream(pConnection, this, uiMaxPacket, uiConnectionId, wcPassword, pHeaders);

    //if there is an existing stream, release it
    if(pOBEXStream && pOBEXStream->IsInited())
    {
        *pStream = pOBEXStream;
        uiActiveStreamID = pOBEXStream->GetStreamID();
        return S_OK;
    }
    else if(pOBEXStream)
    {
         //pOBEXStream fails to initialize
         delete pOBEXStream;
         return E_OUTOFMEMORY;
    }
    else
        return E_OUTOFMEMORY;
}


HRESULT STDMETHODCALLTYPE
CObexDevice::Abort(LPHEADERCOLLECTION pHeaders)
{
    HRESULT hRes = S_OK;

    //invalidate our stream
    uiActiveStreamID = 0xFFFFFFFF;

    //if we have an obex device but dont have a connection, they have not
    //  used a connection point (and thus have not 'BoundToDevice'...
    //  do the transport connection for them
    if(!pConnection && FAILED(ConnectSocket()))
        return E_FAIL;

    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Abort() -- sending OBEX Abort packet\n"));

    if(uiConnectionId != OBEX_INVALID_CONNECTION)
        pHeaders->AddConnectionId(uiConnectionId);

    UCHAR *pNewPacket = NULL;
    ULONG uiNewPackSize;

    //send off the packet
    hRes = ObexSendRecv(pConnection, uiMaxPacket, OBEX_OP_ABORT, 0,0, pHeaders, &pNewPacket, &uiNewPackSize);

    if (FAILED(hRes) || pNewPacket[0] != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
        hRes = E_FAIL;

    delete [] pNewPacket;

    return hRes;
}



HRESULT STDMETHODCALLTYPE
CObexDevice::SetPath(LPCWSTR szRemotePath, DWORD dwFlags, LPHEADERCOLLECTION pHeaders)
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::SetPath()\n"));
    PREFAST_ASSERT(szRemotePath);

    //if a Put is requested the old stream is not used anymore -- so release it
    uiActiveStreamID = 0xFFFFFFFF;

    //if we have an obex device but dont have a connection, they have not
    //  used a connection point (and thus have not 'BoundToDevice'...
    //  do the transport connection for them
    if(!pConnection && FAILED(ConnectSocket()))
        return E_FAIL;


    //sanity checks
    SVSUTIL_ASSERT(szRemotePath);

    if(!pHeaders)
        pHeaders = new CHeaderCollection();
    else
        pHeaders->AddRef();

    if(NULL == pHeaders)
        return E_OUTOFMEMORY;

    HRESULT hResult;
    unsigned char cOpCode = OBEX_OP_SETPATH;

    struct PUTFields
    {
        char flags;
        char constants;
    };

    PUTFields fields;
    memset(&fields, 0, sizeof(PUTFields));

    if (*szRemotePath == L'\\')
    {
        //fill in cPacket independent fields
        fields.flags = (char)dwFlags;
        fields.constants = 0;

        if(uiConnectionId != OBEX_INVALID_CONNECTION)
           pHeaders->AddConnectionId(uiConnectionId);

        pHeaders->AddName(L"");

        //move beyond the '\'
        ++szRemotePath;
        ASSERT(*szRemotePath != '\\');

        UCHAR *pNewPacket = NULL;
        ULONG uiNewPackSize;

        //send off the packet
        hResult = ObexSendRecvWithAuth(pConnection, uiMaxPacket, wcPassword, cOpCode, (char *)&fields, sizeof(PUTFields), pHeaders, &pNewPacket, &uiNewPackSize);

        if (FAILED(hResult) || pNewPacket[0] != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
        {
            pHeaders->Release();
            return E_FAIL;
        }

        pHeaders->Release();
        pHeaders = new CHeaderCollection();

        delete [] pNewPacket;

        if(!pHeaders)
            return E_OUTOFMEMORY;
    }

    char fError = FALSE;
    while ((! fError) && (*szRemotePath != '\0'))
    {
        pHeaders->Release();
        pHeaders = new CHeaderCollection();

        if(!pHeaders)
            return E_OUTOFMEMORY;

        WCHAR *p = (WCHAR*)wcschr (szRemotePath, L'\\');

        //clear fields
        memset(&fields, 0, sizeof(PUTFields));

        if (p) {
            *p = '\0';
            ASSERT(*(p+1) != '\\');
       }

        if (wcscmp (szRemotePath, L"..") == 0)
        {
            fields.flags = 3;
            fields.constants = 0;
            if(uiConnectionId != OBEX_INVALID_CONNECTION)
                pHeaders->AddConnectionId(uiConnectionId);
        }
        else if (wcscmp (szRemotePath, L".") == 0)
        {
            if (p)
            {
                *p = '\\';
                szRemotePath = p + 1;
                 continue;
            }
            else
                break;
        }
        else
        {
            //fill in cPacket independent fields
            fields.flags = (char)dwFlags;
            fields.constants = 0;

            if(uiConnectionId != OBEX_INVALID_CONNECTION)
                pHeaders->AddConnectionId(uiConnectionId);

            pHeaders->AddName(szRemotePath);
        }

        //send off the packet
        UCHAR *pNewPacket;
        ULONG uiNewPackSize;
        hResult = ObexSendRecvWithAuth(pConnection, uiMaxPacket, wcPassword, cOpCode, (char *)&fields, sizeof(PUTFields), pHeaders, &pNewPacket, &uiNewPackSize);

        if(FAILED(hResult))
            return E_FAIL;

        if (pNewPacket[0] != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
        {
            SetLastError (ERROR_INVALID_NAME);
            fError = TRUE;
        }

        delete [] pNewPacket;

        if (p)
        {
            *p = '\\';
            szRemotePath = p + 1;
        }
        else
            break;
    }

    pHeaders->Release();

    if(fError)
        return E_FAIL;

    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexDevice::EnumProperties(REFIID riid, void **ppv)
{
    SVSUTIL_ASSERT(pPropBag);
    if (!ppv)
        return E_INVALIDARG;
    return pPropBag->QueryInterface(riid, ppv);
}

HRESULT STDMETHODCALLTYPE
CObexDevice::SetPassword(LPCWSTR _pszPassToUse)
{
    if(wcslen(_pszPassToUse) >= MAX_PASS_SIZE)
    {
        return E_FAIL;
    }


    //
    //  Set the password
    //
    StringCchCopyW(wcPassword, MAX_PASS_SIZE, _pszPassToUse);

    //
    //  return...
    //
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexDevice::BindToStorage(DWORD dwCapability, IStorage **ppStorage)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
CObexDevice::QueryInterface(REFIID riid, void** ppv)
{
  DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::QueryInterface()\n"));
  if(!ppv)
        return E_POINTER;
    if(riid == IID_IObexDevice)
        *ppv = this;
    else if(riid == IID_IObexDevice)
        *ppv = static_cast<IObexDevice*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}


ULONG STDMETHODCALLTYPE
CObexDevice::AddRef()
{
    ULONG ret = InterlockedIncrement((LONG *)&_refCount);
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexDevice::AddRef(%d) -- 0x%x\n",ret, (int)this));
    return ret;
}

ULONG STDMETHODCALLTYPE
CObexDevice::Release()
{
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexDevice::Release(%d) -- 0x%x\n",ret, (int)this));
    if(!ret)
        delete this;
    return ret;
}



HRESULT
CObexDevice::CompleteDiscovery()
{
    //first make sure that we have figured out all fields required for connection
    //  (the user might have clicked on a BT device before the discovery had
    //    completed)
    if(GetUpdateStatus() != 0xFFFFFFFF)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- dont have full device property list... querying for it now\n"));
        IObexTransport *pTransport = NULL;

        HRESULT hr = CoCreateInstance(clsidTransport, NULL, CLSCTX_INPROC_SERVER, IID_IObexTransport,
            (void **)&pTransport);

        if(SUCCEEDED(hr) && pTransport)
        {
            IObexAbortTransportEnumeration *pAbortEnum = NULL;
            if(SUCCEEDED(pTransport->QueryInterface(IID_IObexAbortTransportEnumeration, (LPVOID *)&pAbortEnum))) {
                    pAbortEnum->Resume();
                    pAbortEnum->Release();
            }

            UINT uiUpdateStatus;
            hr = pTransport->UpdateDeviceProperties(pPropBag, NULL, TRUE, &uiUpdateStatus);


            if (FAILED(hr) || 0 == (uiUpdateStatus & CAN_CONNECT))
            {
                DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- updated device to level 0x%x -- NOT GOOD ENOUGH TO CONNECT\n", uiUpdateStatus));
                hr = E_FAIL;
            }
            else
            {
                DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- updated device to level 0x%x -- ready to GO!\n", uiUpdateStatus));
            }

            pTransport->Release();
            pTransport = NULL;
            return hr;
        }
        else
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- ERROR couldnt get a transport!\n"));
            return E_FAIL;
        }
    }
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- SUCCESS -- no need to query, its already been done\n"));
    return S_OK;
}


HRESULT
CObexDevice::ConnectSocket()
{
    HRESULT hRes = S_OK;

    //
    //  complete the discovery phase... if it fails we have to quit
    //
    if(FAILED((hRes=CompleteDiscovery())))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ConnectSocket() --- Failure completing discovery!\n"));
        return hRes;
    }
    else
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ConnectSocket() --- SUCCEEDED completing discovery!\n"));


    //
    //  if we are not connected, get connected
    //
    if(!pConnection)
    {

        if(FAILED((hRes = ConnectToTransportSocket())))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ConnectSocket() -- cannot ConnectToTransportSocket\n"));
            return hRes;
        }
        else
             DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ConnectSocket() --- SUCCEEDED with ConnectToTransportSocket!\n"));
    }
    else
       DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ConnectSocket() --- already have connection!\n"));


    return hRes;
}

HRESULT
CObexDevice::Connect(LPCWSTR _pszPassToUse, DWORD dwCapability, LPHEADERCOLLECTION pHeaderCollection)
{
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Connect()\n"));
    HRESULT hRes;
    BYTE bMyNonce[16];
    BOOL fOkayToContinueWithoutAuthentication = TRUE;
    BOOL fHaveAuthentication = FALSE;

    //verify password size
    if(_pszPassToUse && wcslen(_pszPassToUse) >= MAX_PASS_SIZE)
        return E_FAIL;

    //if we have an obex device but dont have a connection, they have not
    //  used a connection point (and thus have not 'BoundToDevice'...
    //  do the transport connection for them
    if(!pConnection && FAILED(ConnectSocket()))
        return E_FAIL;

    //
    //  if there is no global password, set the one passed in as global
    //
    if(wcPassword[0] == '\0' && _pszPassToUse)
    {
        StringCchCopyW(wcPassword, MAX_PASS_SIZE, _pszPassToUse);
    }
    //
    //  As per spec: use the Connect() password first, if it does not
    //     exist, use the cached one from SetPassword
    //
    else if(!_pszPassToUse)
    {
        _pszPassToUse = wcPassword;
    }


    //
    //  if a password was sent down, its not okay to continue without authentication
    //      so build up a nonce and put it in along with the headers
    //
    if(_pszPassToUse && wcPassword[0] != '\0')
    {
        fOkayToContinueWithoutAuthentication = FALSE;

        hRes = MakeNonce (_pszPassToUse, bMyNonce);
        if (hRes != ERROR_SUCCESS)
            return hRes;

        BYTE bChallenge[18];
        memcpy(bChallenge+2, bMyNonce, 16);
        bChallenge[0] = 0x00;
        bChallenge[1] = 0x10;

        pHeaderCollection->AddByteArray(OBEX_HID_AUTH_CHALLENGE, 18, bChallenge);
    }



    //
    //  Build the CONNECT headers
    //
    char packet[4];
    WORD wMaxSizeNBO = htons(static_cast<u_short>(g_uiMaxFileChunk));
    packet[0] = OBEX_VERSION;                  //version
    packet[1] = 0x00;                          //flags
    packet[2] = ((char *)(&wMaxSizeNBO))[0]; //max PACKET size (in nbo)
    packet[3] = ((char *)(&wMaxSizeNBO))[1];


    //
    //  Send out the CONNECT packet, and recieve a response
    //
    UCHAR *pPacket = 0;
    ULONG uiPackSize;
    hRes = ObexSendRecv(
        pConnection,
        uiMaxPacket,
        OBEX_OP_CONNECT,
        packet,
        4,
        pHeaderCollection,
        &pPacket,
        &uiPackSize);

    if(FAILED(hRes))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Connect() -- error with ObexSendRecv!\n"));
        return hRes;
    }

    //
    //  Parse the return packet for its fields
    //
    ObexParser *p = new ObexParser(pPacket, uiPackSize);

    if(!p) {
        return E_OUTOFMEMORY;
    }

    //
    //  if the opcode on the return packet is 0xA0 (OK, Success)
    //      fill out the max packet size and prepare the data
    //        for parsing below.  If it was UNAUTHORIZED (0xC1) then
    //        we need to complete the authorization part
    //
    if (p->Op() == (OBEX_STAT_OK | OBEX_OP_ISFINAL))
    {
        WORD wMaxLen;
        ((char *)(&wMaxLen))[0] = pPacket[5];
        ((char *)(&wMaxLen))[1] = pPacket[6];
        uiMaxPacket = htons(wMaxLen) - OBEX_AUTH_HEADER_SIZE;

        if(uiMaxPacket > g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE)
            uiMaxPacket = g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE;

        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexConnect() -- setting max packet size to : %d\n",uiMaxPacket));


        //
        //  Fix up the ObexPacketData by pointing at the start of headers
        //     huh you ask? :)  the CONNECT response is like this
        //        byte0: response code
        //       byte1: connect response packet length
        //       byte2: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //        byte3: OBEX version #
        //       byte4: flags
        //       byte5: max OBEX packet length
        //     byte6: ^^^^^^^^^^^^^^^^^^^^^^
        //  so we advance the packet by 7 bytes to account for this header stuff
        p->ppkt = pPacket;
        p->start = p->current = (pPacket + 7);
        p->length = uiPackSize - 7;
    }
    else if(p->Op() == (OBEX_STAT_UNAUTHORIZED | OBEX_OP_ISFINAL))
    {
        BYTE bServerChallenge[16];
        BOOL fChallengeFound = FALSE;

        WORD wMaxLen;
        ((char *)(&wMaxLen))[0] = pPacket[5];
        ((char *)(&wMaxLen))[1] = pPacket[6];
        uiMaxPacket = htons(wMaxLen) - OBEX_AUTH_HEADER_SIZE;

        if(uiMaxPacket > g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE)
            uiMaxPacket = g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE;

        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexConnect() -- DENIED access, but setting max packet size to : %d -- retrying connection with password\n",uiMaxPacket));

        //
        //  Fix up the ObexPacketData by pointing at the start of headers
        //     huh you ask? :)  the CONNECT response is like this
        //        byte0: response code
        //       byte1: connect response packet length
        //       byte2: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //        byte3: OBEX version #
        //       byte4: flags
        //       byte5: max OBEX packet length
        //     byte6: ^^^^^^^^^^^^^^^^^^^^^^
        //  so we advance the packet by 7 bytes to account for this header stuff
        p->ppkt = pPacket;
        p->start = p->current = (pPacket + 7);
        p->length = uiPackSize - 7;


        //
        //  if we've been denied access, we have to authenticate ourselves
        //
        fOkayToContinueWithoutAuthentication = FALSE;

        //
        //  pull out the challenge generated by the server and get a copy for ourselves
        //
        while (! p->__EOF ())
        {
            if (p->IsA (OBEX_HID_AUTH_CHALLENGE))
            {
                UCHAR *pTempServerChallenge = NULL;
                UCHAR *pServerChallenge = NULL;
                INT iTempServerChallenge;
                p->GetBytes(&pTempServerChallenge, &iTempServerChallenge);

                while(iTempServerChallenge)
                {
                    //decease the buffer by the size of this TAG/NAME/VALUE
                    iTempServerChallenge -= (2 + *(pTempServerChallenge+1));

                    //
                    //  if the tag is 0x00, the size is 16 (the size of the
                    //    MD5 challenge, and the only option that could be set
                    //      is READ/ONLY, we've got our challenge so stop looking
                    //
                    if(0x00 == *pTempServerChallenge &&
                       0x10 == *(pTempServerChallenge+1))

                    {
                        pServerChallenge = pTempServerChallenge + 2;
                        fChallengeFound = TRUE;
                        break;
                    }
                    else
                        pTempServerChallenge += (*(pTempServerChallenge+1) + 2);
                }

                //
                //  if we have actually got a valid looking challenge, cache it away
                //       into bServerChallenge
                if(pServerChallenge)
                {
                    memcpy(bServerChallenge, pServerChallenge, 16);
                }
                else
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- ERROR! challenge recieved, but was not a format we accept\n"));
                    delete [] pPacket;
                    delete p;
                    return OBEX_E_CONNECTION_NOT_ACCEPTED;
                }
            }
            p->Next ();
        }

        //
        //  If there wasnt a challenge, we cant continue on. (they denied us... they didnt challenge us?)
        //
        if(!fChallengeFound)
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- ERROR! Unauthorized reply without challenge... most likely a server bug!\n"));
            delete [] pPacket;
            delete p;
            return OBEX_E_CONNECTION_NOT_ACCEPTED;
        }

        //
        //  build up a response to the servers challenge and add it to the
        //    header collection
        //
        BYTE bAuthResponse[18];

        bAuthResponse[0]=0;
        bAuthResponse[1]=16;

        hRes = MakeResponse (_pszPassToUse, bServerChallenge, bAuthResponse + 2);
        if (hRes != ERROR_SUCCESS)
            return E_FAIL;

        pHeaderCollection->AddByteArray(OBEX_HID_AUTH_RESPONSE, 18, bAuthResponse);


        //
        //  clear out the packet parser, delete the old packet, and RESend the CONNECT packet
        //    but this time its got an AUTHENICATION RESPONSE for the server
        //
        delete p;
        delete [] pPacket;
        pPacket = NULL;
        p = NULL;
        hRes = ObexSendRecv(pConnection,
                            uiMaxPacket,
                            OBEX_OP_CONNECT,
                            packet,
                            4,
                            pHeaderCollection,
                            &pPacket,
                            &uiPackSize);
        if(FAILED(hRes))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Connect() -- error with ObexSendRecv!\n"));
            return hRes;
        }


        //
        //  create a new parser, and double check the opcode
        //
        p = new ObexParser(pPacket, uiPackSize);

        if (!p || p->Op() != (OBEX_STAT_OK | OBEX_OP_ISFINAL))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Connect() -- error with ObexSendRecv - connection not accepted!\n"));
            if(p)
                delete p;
            delete [] pPacket;
            return OBEX_E_CONNECTION_NOT_ACCEPTED;
        }

        ((char *)(&wMaxLen))[0] = pPacket[5];
        ((char *)(&wMaxLen))[1] = pPacket[6];
        uiMaxPacket = htons(wMaxLen) - OBEX_AUTH_HEADER_SIZE;

        if(uiMaxPacket > g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE)
            uiMaxPacket = g_uiMaxFileChunk - OBEX_AUTH_HEADER_SIZE;

        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexConnect() -- GRANTED ACCESS access, setting max packet size to : %d\n",uiMaxPacket));


        //
        //  Fix up the ObexPacketData by pointing at the start of headers
        //     huh you ask? :)  the CONNECT response is like this
        //        byte0: response code
        //       byte1: connect response packet length
        //       byte2: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //        byte3: OBEX version #
        //       byte4: flags
        //       byte5: max OBEX packet length
        //     byte6: ^^^^^^^^^^^^^^^^^^^^^^
        //  so we advance the packet by 7 bytes to account for this header stuff
        p->ppkt = pPacket;
        p->start = p->current = (pPacket + 7);
        p->length = uiPackSize - 7;

    }
    else
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::Connect() -- unrecognized response to CONNECT!\n"));
        delete [] pPacket;
        delete p;
        return OBEX_E_CONNECTION_NOT_ACCEPTED;
    }


    //
    //  parse the headers and put them in the property bag
    //
    PREFAST_ASSERT(p);
    while (! p->__EOF ())
    {
        if (p->IsA (OBEX_HID_CONNECTIONID)) {
            p->GetDWORD ((DWORD *)&uiConnectionId);
            pHeaderCollection->AddConnectionId(uiConnectionId);
        }
        else if (p->IsA (OBEX_HID_AUTH_RESPONSE))
        {
            UCHAR *pTempServerResponseToChallenge = NULL;
            UCHAR *pServerResponseToChallenge = NULL;
            INT iServerResponseToChallenge = 0;
            p->GetBytes(&pTempServerResponseToChallenge, &iServerResponseToChallenge);
            PREFAST_ASSERT(pTempServerResponseToChallenge && iServerResponseToChallenge);

            while(iServerResponseToChallenge)
            {
                //decease the buffer by the size of this TAG/NAME/VALUE
                iServerResponseToChallenge -= (2 + *(pTempServerResponseToChallenge+1));

                //
                //  if the tag is 0x00, the size is 16 (the size of the
                //    MD5 challenge, and the only option that could be set
                //      is READ/ONLY, we've got our challenge so stop looking
                //
                if(0x00 == *pTempServerResponseToChallenge &&
                   0x10 == *(pTempServerResponseToChallenge+1))
                {
                    pServerResponseToChallenge = pTempServerResponseToChallenge + 2;
                    break;
                }
                else
                    pTempServerResponseToChallenge += (*(pTempServerResponseToChallenge+1) + 2);
            }
            pTempServerResponseToChallenge = NULL;

            //
            //  if we have a response to the challenge, generate a MD5 of what
            //    we are expecting and compare it to what was given
            //
            if(pServerResponseToChallenge)
            {
                BYTE bResponseExpected[16];

                hRes = MakeResponse (_pszPassToUse, bMyNonce, bResponseExpected);

                if((hRes == ERROR_SUCCESS) && (0 == memcmp(bResponseExpected, pServerResponseToChallenge, 16)))
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- VERIFIED identity!\n"));
                    fHaveAuthentication = TRUE;
                }
                else
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- ERROR!!! invalid identity!\n"));
                    fHaveAuthentication = FALSE;
                }
            }
            else
            {
                DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- ERROR! challenge recieved, but was not a format we accept\n"));
                delete [] pPacket;
                return OBEX_E_CONNECTION_NOT_ACCEPTED;
            }
        }
        else if((p->Op() & OBEX_TYPE_MASK) == OBEX_TYPE_DWORD)
        {
           ULONG val;
           p->GetDWORD(&val);
           pHeaderCollection->AddLong((BYTE)(p->Op()), val);
        }
        else if((p->Op() & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
        {
            UCHAR *pBuf;
            INT ulBufSize;
            p->GetBytes(&pBuf, &ulBufSize);
            pHeaderCollection->AddByteArray((UCHAR)p->Op(), ulBufSize, pBuf);
        }
        else if((p->Op() & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE)
        {
            WCHAR *pBuf;
            p->GetString(&pBuf);
            pHeaderCollection->AddUnicodeString((UCHAR)p->Op(), pBuf);
        }

        else if((p->Op() & OBEX_TYPE_MASK) == OBEX_TYPE_BYTE)
        {
            BYTE byte;
            p->GetBYTE(&byte);
            pHeaderCollection->AddByte((UCHAR)p->Op(), byte);
        }

        p->Next ();
    }


    //
    //  delete our temporary storage and return
    //
    delete p;
    delete [] pPacket;

    if(!fOkayToContinueWithoutAuthentication && !fHaveAuthentication)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- ERROR! we needed authentication, but it failed\n"));
        return OBEX_E_CONNECTION_NOT_ACCEPTED;
    }

    return S_OK;
}




/*****************************************************************************/
/*  CObexDevice::ObexSendRecvWithAuth -- our job is to do the same thing     */
/*     (send and receive 1 packet) as ObexSendRecv but in the event our      */
/*       request is denied because of authorization, we send our credientials  */
/*     and perform authorization (this function makes authorization          */
/*       transparent to SEND/RECV                                                 */
/*****************************************************************************/
HRESULT
ObexSendRecvWithAuth(IObexTransportConnection *pConnection,
                    UINT uiMaxPacket,
                    WCHAR *wcPassword,
                    BYTE cOpCode,
                    char *additionalDta,
                    UINT cAddnlDta,
                    IHeaderCollection *pHeaderCollection,
                    unsigned char **pNewPacket,
                    ULONG *pSize)
{
    HRESULT hr = E_FAIL;
    ObexParser *p = NULL;
    UINT uiPacketSize;
    UCHAR *pPacket;

    //first things first, send the packet out as-is
    hr = ObexSendRecv(pConnection, uiMaxPacket, cOpCode, additionalDta, cAddnlDta, pHeaderCollection, pNewPacket, pSize);

    if(FAILED(hr))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- ObexSendRecv failed!\n"));
        return hr;
    }


    pPacket = *pNewPacket;

    if(*pPacket != (OBEX_STAT_UNAUTHORIZED | OBEX_OP_ISFINAL) )
    {
       return hr;
    }


    if(wcPassword[0] == '\0')
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- PASSWORD NOT SET and AUTHENTICATION REQUIRED!\n"));
        return OBEX_E_CONNECTION_NOT_ACCEPTED;
    }

    //
    //  Parse the return packet for its fields
    //
    uiPacketSize = *pSize;
    p = new ObexParser(pPacket, *pSize);

    if(!p)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- ObexSendRecv failed!\n"));
        return hr;
    }


    //
    //  if the opcode is UNAUTHORIZED we need to handle
    //        this for the caller.  We do so by building
    //        up a response and adding it to the property bag for them
    //        we then simply reissue their origional packet
    //
    UCHAR *pServerChallenge = NULL;

    //
    //  Fix up the packet data by pointing at the start of headers
    //     huh you ask? :)  the CONNECT response is like this
    //        byte0: response code
    //       byte1: connect response packet length
    //       byte2: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //  so we advance the packet by 3 bytes to account for this header stuff
    p->ppkt = pPacket;
    p->start = p->current = (pPacket + 3);
    p->length = uiPacketSize - 3;


    //
    //  pull out the challenge generated by the server and get a copy for ourselves
    //
    while (! p->__EOF ())
    {
        if (p->IsA (OBEX_HID_AUTH_CHALLENGE))
        {
            UCHAR *pTempServerChallenge = NULL;
            INT iTempServerChallenge;
            p->GetBytes(&pTempServerChallenge, &iTempServerChallenge);

            while(iTempServerChallenge)
            {
                //decease the buffer by the size of this TAG/NAME/VALUE
                iTempServerChallenge -= (2 + *(pTempServerChallenge+1));

                //
                //  if the tag is 0x00, the size is 16 (the size of the
                //    MD5 challenge, and the only option that could be set
                //      is READ/ONLY, we've got our challenge so stop looking
                //
                if(*pTempServerChallenge == 0x00 &&
                   *(pTempServerChallenge+1) == 0x10)
                {
                    pServerChallenge = pTempServerChallenge + 2;
                    break;
                }
                else
                    pTempServerChallenge += (*(pTempServerChallenge+1) + 2);
            }
        }
        p->Next ();
    }

    //
    //  If there wasnt a challenge, we cant continue on. (they denied us... they didnt challenge us?)
    //
    if(!pServerChallenge)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- ERROR! Unauthorized reply without challenge... most likely a server bug!\n"));
        delete [] pPacket;
        delete p;
        return E_FAIL;
    }

    BYTE bAuthResponse[18];

    bAuthResponse[0]=0;
    bAuthResponse[1]=16;

    HRESULT hRes = MakeResponse (wcPassword, pServerChallenge, bAuthResponse + 2);
    if (hRes != ERROR_SUCCESS)
        return hRes;

    pHeaderCollection->AddByteArray(OBEX_HID_AUTH_RESPONSE, 18, bAuthResponse);


    //
    //  clear out the packet parser, delete the old packet, and RESend the  packet
    //    but this time its got an AUTHENICATION RESPONSE for the server
    //
    delete p;
    delete [] pPacket;
    pPacket = NULL;
    p = NULL;

    hRes = ObexSendRecv(pConnection,
                                uiMaxPacket,
                                cOpCode,
                                additionalDta,
                                cAddnlDta,
                                pHeaderCollection,
                                pNewPacket,
                                pSize);
    if(FAILED(hRes))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- error with ObexSendRecv!\n"));
        return hRes;
    }


    //
    //  create a new parser, and double check the opcode
    //
    pPacket = *pNewPacket;
    uiPacketSize = *pSize;
    p = new ObexParser(pPacket, uiPacketSize);

    if (!p || p->Op() == (OBEX_STAT_UNAUTHORIZED | OBEX_OP_ISFINAL))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecvWithAuth() -- we were *NOT* verified... wrong password?!\n"));
        if(p)
            delete p;
        return OBEX_E_CONNECTION_NOT_ACCEPTED;
    }
    else
        hr = S_OK;

    delete p;
    return hr;
}




HRESULT
ObexSendRecv(IObexTransportConnection *pConnection,
                 UINT uiMaxPacket,
                 BYTE opCode,
                 char *additionalDta,
                 UINT cAddnlDta,
                 IHeaderCollection *pHeaders,
                 unsigned char **pNewPacket,
                 ULONG *pSize)
{
    SVSUTIL_ASSERT(pHeaders && pConnection);

    *pNewPacket = 0;
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend()\n"));

    if(!pConnection)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSend() -- pConnection not there!\n"));
        return E_FAIL;
    }

    //send off the packet
    HRESULT hRet = S_OK;
    UINT uiHeaderSize = 0;
    char *pHeader = 0;

    //if there are headers, make up the data
    if(pHeaders && (uiHeaderSize = SizeOfHeader(pHeaders)) > 0)
    {
        IHeaderEnum *pHeaderEnum;
        if(FAILED(pHeaders->EnumHeaders(&pHeaderEnum)))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- cant enum headers!\n"));
            return E_FAIL;
        }

        OBEX_HEADER *myHeader = 0;
        ULONG ulFetched;
        UINT uiPacketSize;

        //see how big the headers are
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"  ObexSend::Seeing how big the headers are\n"));

        //so here's the scoop here... I've allocated 21 special bytes that
        //   are used for authentication.  At this point they MIGHT not have
        //   been used by the authentication (maybe the server doesnt care who we are)
        //   so dotn check for it
        //if(1 + 2 + cAddnlDta + uiHeaderSize > uiMaxPacket + OBEX_AUTH_HEADER_SIZE)
        uiPacketSize = 1 + 2;
        if(FAILED(UIntAdd(cAddnlDta, uiPacketSize, &uiPacketSize)) ||
           FAILED(UIntAdd(uiHeaderSize, uiPacketSize, &uiPacketSize))) {
            SVSUTIL_ASSERT(FALSE); //dont remove this ASSERT... if we got here
                                   //  there is a bug in the client
            pHeaderEnum->Release();
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- packet too large! internal error\n"));
            return E_FAIL;
        }

        if(uiPacketSize > uiMaxPacket + OBEX_AUTH_HEADER_SIZE)
        {
            SVSUTIL_ASSERT(FALSE); //dont remove this ASSERT... if we got here
                                   //  there is a bug in the client
            pHeaderEnum->Release();
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- packet too large! internal error\n"));
            return E_FAIL;
        }

        //allocate enough memory to hold them
        UINT uiRemaining = uiHeaderSize;
        pHeader = new char[uiHeaderSize];
        if(!pHeader)
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"Couldnt allocate: %d bytes\n", uiHeaderSize));
            return E_OUTOFMEMORY;
        }
        char *pHeaderTemp = pHeader;

        //copy in the data
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"  ObexSend::Copying in headers\n"));
        pHeaderEnum->Reset();
        while(SUCCEEDED(pHeaderEnum->Next(1, &myHeader, &ulFetched)))
        {
            if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_DWORD)
            {
                *pHeaderTemp = myHeader->bId;
                DWORD valNBO = htonl(myHeader->value.dwData);
                if(uiRemaining < 4)
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- cant fit everything into buffer\n"));
                    ASSERT(FALSE);
                    return E_FAIL;
                }
                uiRemaining -= 5;

                memcpy(pHeaderTemp+1, (char *)&valNBO, 4);
                pHeaderTemp += (4 + 1);
            }
            else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTE)
            {
                if(uiRemaining < 2)
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- cant fit everything into buffer\n"));
                    ASSERT(FALSE);
                    return E_FAIL;
                }
                uiRemaining -= 2;

                *pHeaderTemp = myHeader->bId;
                *(pHeaderTemp + 1) = myHeader->value.bData;
                pHeaderTemp+=2;
            }
            else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
            {
                SVSUTIL_ASSERT(myHeader->value.ba.dwSize + 3 <= 0xFFFF);
                WORD dwSize = (WORD)(myHeader->value.ba.dwSize + 3);
                WORD dwSizeNBO = htons(dwSize);

                //
                // Make sure everything fits and look for overflow on value.ba.dwSize
                if(myHeader->value.ba.dwSize > OBEX_MAX_PACKET_SIZE-3 || uiRemaining < dwSize)
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- cant fit everything into buffer\n"));
                    ASSERT(FALSE);
                    return E_FAIL;
                }
                uiRemaining -= dwSize;

                *pHeaderTemp = myHeader->bId;
                pHeaderTemp[1] = ((char *)&dwSizeNBO)[0];
                pHeaderTemp[2] = ((char *)&dwSizeNBO)[1];
                PREFAST_SUPPRESS(508, "overflow is checked about 10 lines up");
                memcpy(pHeaderTemp+3, myHeader->value.ba.pbaData, myHeader->value.ba.dwSize);
                pHeaderTemp += dwSize;
            }
            else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE)
            {
                WORD dwStrLen = (WORD)wcslen(myHeader->value.pszData);
                WORD dwSize = (dwStrLen ? ((dwStrLen + 1) * sizeof(WCHAR)) : 0) + 3;

                if(uiRemaining < dwSize)
                {
                    DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- cant fit everything into buffer\n"));
                    ASSERT(FALSE);
                    return E_FAIL;
                }
                uiRemaining -= 3; // account for the header below

                *pHeaderTemp = myHeader->bId;
                pHeaderTemp[1] = (dwSize >> 8) & 0xff;
                pHeaderTemp[2] = dwSize & 0xff;;

                if (dwStrLen)
                {
                    unsigned char *pbuf = (unsigned char *)(pHeaderTemp+3);
                    WCHAR *psz = myHeader->value.pszData;

                    //copy in the data, making it into NBO
                    while (*psz && uiRemaining>=2)
                    {
                        *pbuf++ = (*psz >> 8) & 0xff;
                        *pbuf++ = (*psz) & 0xff;
                        uiRemaining-=2;
                        ++psz;
                    }

                    *pbuf++ = 0;
                    *pbuf++ = 0;
                    uiRemaining-=2;
                }
                pHeaderTemp += dwSize;
            }
        }
        ASSERT(uiRemaining == 0);
        pHeaderEnum->Release();
    }

    //build up a packet
    WORD wPacketSize = 0;
    WORD wAddnlDta = (WORD)cAddnlDta;
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"  SendPacket::Building packet\n"));

    if(cAddnlDta > 0xFFFF ||
       FAILED(WordAdd(1, 2, &wPacketSize)) ||
       FAILED(WordAdd(wAddnlDta, wPacketSize, &wPacketSize)) ||
       FAILED(WordAdd((USHORT)uiHeaderSize, wPacketSize, &wPacketSize))) {
            delete [] pHeader;
            pHeader = 0;
            return E_OUTOFMEMORY;
    }

    WORD wPacketSizeNBO = htons(wPacketSize);
    char *pPacket = new char[wPacketSize];

    if(!pPacket)
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"SendPacket:: Couldnt allocate: %d bytes\n", wPacketSize));
        delete [] pHeader;
        pHeader = 0;
        return E_OUTOFMEMORY;
    }

    pPacket[0] = (char)opCode;
    memcpy(pPacket+1, (char *)&wPacketSizeNBO, 2);
    memcpy(pPacket+3, additionalDta, wAddnlDta);
    if(pHeader)
    {
        memcpy(pPacket+3+wAddnlDta, pHeader, uiHeaderSize);
        delete [] pHeader;
        pHeader = 0;
    }

#if defined(DEBUG) || defined(_DEBUG)
    DEBUGMSG(OBEX_DUMP_PACKETS_ZONE, (L"Sending -------------------------------"));
    DumpBuff ((UCHAR *)&pPacket[0], wPacketSize);
#endif


    //send out pPacket with size wPacketSize and recieve an ACK
    *pNewPacket = new UCHAR[g_uiMaxFileChunk];
    if( !(*pNewPacket) )
    {
        delete [] pPacket;
        return E_OUTOFMEMORY;
    }
    SVSUTIL_ASSERT(wPacketSize <= uiMaxPacket + OBEX_AUTH_HEADER_SIZE);
    hRet = pConnection->SendRecv(wPacketSize, (UCHAR *)pPacket, g_uiMaxFileChunk, *pNewPacket, pSize,0);
    delete [] pPacket;
    if (FAILED(hRet))
    {
        delete[] *pNewPacket;
        *pNewPacket = 0;
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::ObexSendRecv -- SendRecv Failed!\n"));
    }
    return hRet;
}


HRESULT
CObexDevice::ConnectToTransportSocket()
{
    PREFAST_ASSERT(pPropBag);
    DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket"));


    HRESULT hr;

    //if we are alreay connected, release the connection
    if(pConnection)
    {
        pConnection->Release();
        pConnection = NULL;
    }
    if(pSocket)
    {
        pSocket->Release();
        pSocket = NULL;
    }

    //get the transport guid from the propterybag
    VARIANT var;
    VariantInit(&var);
    hr = pPropBag->Read(c_szDevicePropTransport, &var, 0);

    if(FAILED(hr)){
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket -- cannot read from transport!"));
        return E_FAIL;
    }

    CLSID clsid;
    hr = CLSIDFromString(var.bstrVal, &clsid);
    VariantClear(&var);

    if(FAILED(hr))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket -- cannot get CLSID from string!"));
        return hr;
    }

    //seek out a transport
    IObexTransport *pTransport;
    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IObexTransport,
        (void **)&pTransport);

    if(FAILED(hr))
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket -- cannot CoCreate an !"));
        return E_FAIL;
    }

    IObexAbortTransportEnumeration *pAbortEnum = NULL;
    if(SUCCEEDED(pTransport->QueryInterface(IID_IObexAbortTransportEnumeration, (LPVOID *)&pAbortEnum))) {
            pAbortEnum->Resume();
            pAbortEnum->Release();
    }

    if(SUCCEEDED(pTransport->CreateSocket(pPropBag, &pSocket)))
    {
        hr = pSocket->Connect(pPropBag, 0, &pConnection);
        if(FAILED(hr))
        {
            DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket -- cannot Connect!"));
            pSocket->Release();
            pSocket = 0;
        }
    }
    else
    {
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE, (L"CObexDevice::ConnectToTransportSocket -- cannot CreateSocket()!"));
    }

    pTransport->Release();

    if(SUCCEEDED(hr))
        DEBUGMSG(OBEX_OBEXDEVICE_ZONE,(L"CObexDevice::CompleteDiscovery() -- SUCCEEDED\n"));
    return hr;
}

HRESULT
CObexDevice::GetDeviceName(VARIANT *pvarName)
{
    return pPropBag->Read(c_szDevicePropName, pvarName, NULL);
}

HRESULT
CObexDevice::GetDeviceAddress(VARIANT *pvarAddr)
{
    return pPropBag->Read(c_szDevicePropAddress, pvarAddr, NULL);
}

HRESULT
CObexDevice::GetDevicePort(VARIANT *pvarAddr)
{
    return pPropBag->Read(c_szPort, pvarAddr, NULL);
}

HRESULT
CObexDevice::GetDeviceTransport(VARIANT *pvarAddr)
{
    return pPropBag->Read(c_szDevicePropTransport, pvarAddr, NULL);
}

HRESULT
CObexDevice::GetPropertyBag(LPPROPERTYBAG2 *ppBag)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexDevice::GetPropertyBag()\n"));
    PREFAST_ASSERT(ppBag);
    PREFAST_ASSERT(pPropBag);

    pPropBag->AddRef();
    *ppBag = pPropBag;
    return S_OK;
}


UINT
SizeOfHeader(IHeaderCollection *pHeader)
{
    DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CObexDevice::SizeOfHeader()\n"));
    UINT uiHSize = 0;
    OBEX_HEADER *myHeader;
    ULONG ulFetched;
    LPHEADERENUM pHeaderEnum;

    pHeader->EnumHeaders(&pHeaderEnum);

    //see how big the headers are
    while(SUCCEEDED(pHeaderEnum->Next(1, &myHeader, &ulFetched)))
    {
        //determine the lenght of the data
        if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_DWORD)
            uiHSize += (4 + 1);
        else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
            uiHSize += (myHeader->value.ba.dwSize + 3);
        else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE)
        {
            DWORD dwCsLen = wcslen(myHeader->value.pszData);
            uiHSize += dwCsLen ? (((dwCsLen+1) * sizeof(WCHAR)) + 3) : 3;
        }
        else if((myHeader->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTE)
            uiHSize += (1 + 1);
    }

    pHeaderEnum->Release();

    return uiHSize;
}
