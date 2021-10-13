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
#include "CObex.h"
#include "ObexBTHTransport.h"
#include "BTHTransportSocket.h"
#include "bt_api.h"
#include <initguid.h>
#include "bt_sdp.h"
#include "bthapi.h"
#include "PropertyBag.h"
#include "PropertyBagEnum.h"

#include "ObexStrings.h"

#define OBEX_PROTOCOL_UUID 8

LPSOCKET CObexBTHTransport::pSocket = 0;

#define GUID_FORMAT     L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"
#define GUID_FORMAT_SEP L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x};"
#define GUID_ELEMENTS(p) \
    p.Data1,                 p.Data2,    p.Data3,\
    p.Data4[0], p.Data4[1], p.Data4[2], p.Data4[3],\
    p.Data4[4], p.Data4[5], p.Data4[6], p.Data4[7]

struct BT_UUID16_LIST
{
    GUID uuid;
    BT_UUID16_LIST *pNext;
};

struct BT_CHANNEL_LIST
{
    ULONG ulChannel;
    BT_UUID16_LIST *pUUIDList;
    BT_CHANNEL_LIST *pNext;
};


//code stolen from btdc\sdptest.cxx & sdpsearch
static BOOL IsRfcommUuid(NodeData *pNode)  {
    if (pNode->type != SDP_TYPE_UUID)
        return FALSE;

    if (pNode->specificType == SDP_ST_UUID16)
        return (pNode->u.uuid16 == RFCOMM_PROTOCOL_UUID16);
    else if (pNode->specificType == SDP_ST_UUID32)
        return (pNode->u.uuid32 == RFCOMM_PROTOCOL_UUID16);
    else if (pNode->specificType == SDP_ST_UUID128)
        return (0 == memcmp(&RFCOMM_PROTOCOL_UUID,&pNode->u.uuid128,sizeof(GUID)));

    return FALSE;
}

static BOOL GetChannel (NodeData *pChannelNode, ULONG *pulChannel) {
    if (pChannelNode->specificType == SDP_ST_UINT8)
        *pulChannel = pChannelNode->u.uint8;
    else if (pChannelNode->specificType == SDP_ST_INT8)
        *pulChannel = pChannelNode->u.int8;
    else if (pChannelNode->specificType == SDP_ST_UINT16)
        *pulChannel = pChannelNode->u.uint16;
    else if (pChannelNode->specificType == SDP_ST_INT16)
        *pulChannel = pChannelNode->u.int16;
    else if (pChannelNode->specificType == SDP_ST_UINT32)
        *pulChannel = pChannelNode->u.uint32;
    else if (pChannelNode->specificType == SDP_ST_INT32)
        *pulChannel = pChannelNode->u.int32;
    else
        return FALSE; // misformatted response.

    return TRUE;
}

// Frees memory associated with one BT_CHANNEL_LIST entry;
// does not adjust next pointers (caller does this)
static void DeleteBtChannelListNode(BT_CHANNEL_LIST *pChannel) {
    BT_UUID16_LIST *pList = pChannel->pUUIDList;

    while (pList) {
        BT_UUID16_LIST *pDelNode = pList;
        pList = pDelNode->pNext;
        delete pDelNode;
    }

    delete pChannel;
}

static STDMETHODIMP
ServiceAndAttributeSearchParse(
    UCHAR *szResponse,             // in - response returned from SDP ServiceAttribute query
    DWORD cbResponse,            // in - length of response
    ISdpRecord ***pppSdpRecords, // out - array of pSdpRecords
    ULONG *pNumRecords           // out - number of elements in pSdpRecords
    )
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ServiceAndAttributeSearchParse()\n"));

    HRESULT hres = E_FAIL;

    *pppSdpRecords = NULL;
    *pNumRecords = 0;

    ISdpStream *pIStream = NULL;

    hres = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);

    if ((FAILED(hres)) || (!pIStream))
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ServiceAndAttributeSearchParse() -- CoCreate failed\n"));
        return hres;
    }

    hres = pIStream->Validate(szResponse,cbResponse,NULL);

    if (SUCCEEDED(hres)) {
        hres = pIStream->VerifySequenceOf(szResponse,cbResponse,
                                          SDP_TYPE_SEQUENCE,NULL,pNumRecords);

        if (SUCCEEDED(hres) && *pNumRecords > 0) {
            *pppSdpRecords = (ISdpRecord **) CoTaskMemAlloc(sizeof(ISdpRecord*) * (*pNumRecords));

            if (pppSdpRecords != NULL) {
                hres = pIStream->RetrieveRecords(szResponse,cbResponse,*pppSdpRecords,pNumRecords);

                if (!SUCCEEDED(hres)) {
                    CoTaskMemFree(*pppSdpRecords);
                    *pppSdpRecords = NULL;
                    *pNumRecords = 0;
                }
            }
            else {
                hres = E_OUTOFMEMORY;
            }
        }
    }

    if (pIStream != NULL) {
        pIStream->Release();
        pIStream = NULL;
    }

    if(FAILED(hres))
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ServiceAndAttributeSearchParse() -- FAILED\n"));

    return hres;
}

static HRESULT GetRFCOMMChannel(UCHAR *szBuf, DWORD cbResponse, BT_CHANNEL_LIST **pBTCList) {
    PREFAST_ASSERT(szBuf && pBTCList);
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::GetRFCOMMChannel()\n"));

    ISdpRecord **pRecordArg;
    ISdpRecord *pRecord = NULL;
    ISdpStream *pIStream = NULL;

    BT_CHANNEL_LIST *pChannList = NULL;
    *pBTCList = pChannList;

    ULONG ulRecords = 0;
    ULONG i,j,k;
    ULONG  ulChannelId = 0;

    HRESULT hres = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);
    if ((FAILED(hres)) || (!pIStream))
    {
            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::CoCreateInstance() FAILED\n"));
            return hres;
    }

    if (ERROR_SUCCESS != ServiceAndAttributeSearchParse(szBuf,cbResponse,&pRecordArg,&ulRecords))
        goto done;

    for (i = 0; i < ulRecords; i++)
    {
        BOOL fHaveChannelID = FALSE;
        pRecord = pRecordArg[i];    // particular record to examine in this loop
        NodeData  protocolList;     // contains SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST data, if available
        NodeData  UUIDList;            // contains SD_
        BT_UUID16_LIST *pUUIDList = NULL;

        //
        //  Ask for the SDP attributes (CLASS list and PROTOCOL DESCRIPTOR LIST)
        //
        if (ERROR_SUCCESS != pRecord->GetAttribute(SDP_ATTRIB_CLASS_ID_LIST,&UUIDList) ||
            (UUIDList.type != SDP_TYPE_CONTAINER))
        {
            continue;
        }

        if (ERROR_SUCCESS != pRecord->GetAttribute(SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST,&protocolList) ||
            (protocolList.type != SDP_TYPE_CONTAINER))
        {
            continue;
        }

        //
        //  Go after the UUID(s) -- put them in a list
        //
        ISdpNodeContainer *pUUIDContainer = UUIDList.u.container;
        DWORD cUUIDs = 0;
        NodeData ndUUID;
        pUUIDContainer->GetNodeCount(&cUUIDs);


        for(j=0; j<cUUIDs; j++)
        {
            pUUIDContainer->GetNode(j,&ndUUID);

            BT_UUID16_LIST *pNew = new BT_UUID16_LIST();
            if( !pNew )
            {
                 //clean up UUID list
                while(pUUIDList)
                {
                    BT_UUID16_LIST *pDel = pUUIDList;
                    pUUIDList = pUUIDList->pNext;
                    delete pDel;
                }

                 hres = E_OUTOFMEMORY;
                 goto done;
            }
            pNew->pNext = pUUIDList;
            pIStream->NormalizeUuid(&ndUUID, &pNew->uuid);
            pUUIDList = pNew;
        }


        //
        //  Go after the channel
        //
        ISdpNodeContainer *pRecordContainer = protocolList.u.container;
        DWORD cProtocols = 0;
        NodeData protocolDescriptor; // information about a specific protocol (i.e. L2CAP, RFCOMM, ...)

        pRecordContainer->GetNodeCount(&cProtocols);
        for (j = 0; j < cProtocols; j++)
        {
            pRecordContainer->GetNode(j,&protocolDescriptor);

            if (protocolDescriptor.type != SDP_TYPE_CONTAINER)
                continue;

            ISdpNodeContainer *pProtocolContainer = protocolDescriptor.u.container;
            DWORD cProtocolAtoms = 0;
            pProtocolContainer->GetNodeCount(&cProtocolAtoms);

            for (k = 0; k < cProtocolAtoms; k++)
            {
                NodeData nodeAtom;  // individual data element, such as what protocol this is or RFCOMM channel id.

                pProtocolContainer->GetNode(k,&nodeAtom);

                if (IsRfcommUuid(&nodeAtom))
                {
                    if (k+1 == cProtocolAtoms)
                    {
                        // misformatted response.  Channel ID should follow RFCOMM uuid
                        break;
                    }
                    NodeData channelID;
                    pProtocolContainer->GetNode(k+1,&channelID);

                    if (GetChannel(&channelID,&ulChannelId))
                    {
                        //see if there is an existing node with the same channelID
                        BT_CHANNEL_LIST *pTemp = pChannList;
                        while(pTemp && pTemp->ulChannel != ulChannelId)
                            pTemp = pTemp->pNext;

                        //if the channel doesnt exist, make a new one
                        if(!pTemp)
                        {
                            pTemp = new BT_CHANNEL_LIST();
                            if( !pTemp )
                            {
                                //clean up UUID list
                                while(pUUIDList)
                                {
                                    BT_UUID16_LIST *pDel = pUUIDList;
                                    pUUIDList = pUUIDList->pNext;
                                    delete pDel;
                                }

                                hres = E_OUTOFMEMORY;
                                goto done;
                            }

                            pTemp->ulChannel = ulChannelId;
                            pTemp->pNext = pChannList;
                            pTemp->pUUIDList = pUUIDList;
                            pChannList = pTemp;
                        }

                        //if it DOES exist, chain the UUID list onto the old node
                        else
                        {
                            BT_UUID16_LIST *pLastNode = pUUIDList;
                            while(pLastNode && pLastNode->pNext)
                                pLastNode = pLastNode->pNext;

                            if(pLastNode)
                            {
                                SVSUTIL_ASSERT(!pLastNode->pNext);

                                pLastNode->pNext = pTemp->pUUIDList;
                                pTemp->pUUIDList = pUUIDList;
                            }
                        }

                        fHaveChannelID = TRUE;
                        pUUIDList = NULL;
                    }

                    break; // formatting error
                }
            }
            if(fHaveChannelID)
                break;
        }

        //
        //  Cleanup the UUID list... NOTE: this means that we DIDNT get a channelID
        //
        while(pUUIDList)
        {
            BT_UUID16_LIST *pDel = pUUIDList;
            pUUIDList = pUUIDList->pNext;
            delete pDel;
        }
    }

done:
    pIStream->Release();

    for (i = 0; i < ulRecords; i++)
        pRecordArg[i]->Release();

    CoTaskMemFree(pRecordArg);

    *pBTCList = pChannList;
    return hres;
}




static void InitWSAQuerySet(WSAQUERYSET *pw, BT_ADDR *pba=NULL, CSADDR_INFO *pcsaBuffer=NULL, SOCKADDR_BTH *psockBT=NULL, LPBLOB pBlob=NULL)  {
    memset(pw,0,sizeof(WSAQUERYSET));
    pw->dwSize      = sizeof(WSAQUERYSET);
    pw->dwNameSpace = NS_BTH;
    pw->lpBlob      = pBlob;

    if (pba)  {
        pw->lpcsaBuffer = pcsaBuffer;

        memset(pcsaBuffer,0,sizeof(CSADDR_INFO));
        memset(psockBT,0,sizeof(SOCKADDR_BTH));
        memcpy(&psockBT->btAddr,pba,sizeof(BT_ADDR));
        pcsaBuffer->RemoteAddr.lpSockaddr       = (LPSOCKADDR) psockBT;
        pcsaBuffer->RemoteAddr.iSockaddrLength  = sizeof(SOCKADDR_BTH);
    }
}




static HRESULT QueryForChannel(BT_ADDR bt, BT_CHANNEL_LIST **pBTCList)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::QueryForChannel()\n"));

    SVSUTIL_ASSERT(pBTCList);

    SdpQueryUuid   uuidObex;
    uuidObex.u.uuid16 = OBEX_PROTOCOL_UUID16;
    uuidObex.uuidType = SDP_ST_UUID16;

    HRESULT hr = E_FAIL;
    SdpAttributeRange     attribRange[2];
    attribRange[0].minAttribute = attribRange[0].maxAttribute = SDP_ATTRIB_CLASS_ID_LIST;
    attribRange[1].minAttribute = attribRange[1].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;


    WSAQUERYSET         wsaQuery;
    PBTHNS_RESTRICTIONBLOB prBlob;
    BLOB blob;
    unsigned long ulBlobLen = sizeof(BTHNS_RESTRICTIONBLOB) + sizeof(SdpAttributeRange);
    CHAR BTHNSStruct[sizeof(BTHNS_RESTRICTIONBLOB) + sizeof(SdpAttributeRange)];

    if(NULL == BTHNSStruct)
        return E_OUTOFMEMORY;

    prBlob = (PBTHNS_RESTRICTIONBLOB) BTHNSStruct;
    memset(BTHNSStruct,0,ulBlobLen); //required to have all UUID's following last
                                 //  valid UUID of prBlob->uuids be 0!

    prBlob->type          = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
    prBlob->numRange      =  2; //tells that there are 2 attribRange's

    //PREFAST NOTE:  this should be okay - note prBlob points to BTHNSStruct and thats large
    PREFAST_SUPPRESS(203, "PREfast error - the following line is valid");
    memcpy(prBlob->pRange, attribRange, sizeof(SdpAttributeRange) * 2);
    memcpy(prBlob->uuids, &uuidObex, sizeof(SdpQueryUuid));

    blob.cbSize    = ulBlobLen;
    blob.pBlobData = (BYTE*)prBlob;

    CSADDR_INFO csAddr;
    SOCKADDR_BTH sockBT;


    HINSTANCE hLib = LoadLibrary(L"btdrt.dll");
    if (!hLib) {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE, (L"CObexBTHTransport::QueryForChannel(): Failed to load btdrt.dll. GetLastError()=0x%08x\n", GetLastError()));
        return hr;
    }

    typedef int (*BTHNSLOOKUPSERVICEBEGIN)(LPWSAQUERYSET, DWORD, LPHANDLE );
    typedef int (*BTHNSLOOKUPSERVICENEXT)(HANDLE, DWORD, LPDWORD, LPWSAQUERYSET );
    typedef int (*BTHNSLOOKUPSERVICEEND)(HANDLE);
    BTHNSLOOKUPSERVICEBEGIN pfnBthNsLookupServiceBegin = (BTHNSLOOKUPSERVICEBEGIN )GetProcAddress(hLib, L"BthNsLookupServiceBegin");
    BTHNSLOOKUPSERVICENEXT pfnBthNsLookupServiceNext = (BTHNSLOOKUPSERVICENEXT )GetProcAddress(hLib, L"BthNsLookupServiceNext");
    BTHNSLOOKUPSERVICEEND pfnBthNsLookupServiceEnd = (BTHNSLOOKUPSERVICEEND )GetProcAddress(hLib, L"BthNsLookupServiceEnd");
    if ((!pfnBthNsLookupServiceBegin) || (!pfnBthNsLookupServiceNext) || (!pfnBthNsLookupServiceEnd))
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE, (L"CObexBTHTransport::QueryForChannel(): BthNsLookupServiceXXX functions not found in btdrt.dll\n"));
        FreeLibrary(hLib);
        return hr;
    }

    InitWSAQuerySet(&wsaQuery,&bt,&csAddr,&sockBT,&blob);

    DWORD dwFlags = 0;
    long lRet;
    HANDLE hLookup;

    lRet = pfnBthNsLookupServiceBegin(&wsaQuery,dwFlags,&hLookup);

    if (ERROR_SUCCESS == lRet)
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::QueryForChannel()  BthNsLookupServiceBegin returned 0x%08x\r\n",lRet));
        // Get Results
        DWORD dwSize  = 5000;
        CHAR *pQueryResults = new char[dwSize];

        if(!pQueryResults)
            hr = E_OUTOFMEMORY;
        else
        {
            LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) pQueryResults;

            dwFlags = LUP_RETURN_ALL;

            lRet = pfnBthNsLookupServiceNext(hLookup,dwFlags,&dwSize,pwsaResults);

            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::QueryForChannel()   BthNsLookupServiceNext returned 0x%08x \r\n",lRet));

            pfnBthNsLookupServiceEnd(hLookup);

            if (lRet == ERROR_SUCCESS || lRet == ERROR_NO_MORE_ITEMS && pwsaResults->lpBlob)
            {
                hr = GetRFCOMMChannel(pwsaResults->lpBlob->pBlobData, pwsaResults->lpBlob->cbSize, pBTCList);
                if(FAILED(hr))
                {
                    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::QueryForChannel -- GetRFCOMMChannel failed\n"));
                }
            }
        }

        if(pQueryResults)
            delete [] pQueryResults;
    }

    FreeLibrary(hLib);
    return hr;
}

CObexBTHTransport::CObexBTHTransport() : _refCount(1), pNameCache(NULL), fIsAborting(FALSE)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::CObexBTHTransport()\n"));

    //initilize the socket libs
    WSADATA wsd;
    WSAStartup (MAKEWORD(2,2), &wsd);
}

CObexBTHTransport::~CObexBTHTransport()
{
    //
    //  clean out name cache
    //
    while(pNameCache)
    {
        NAME_CACHE_NODE *pDel = pNameCache;
        VariantClear(&pDel->var);
        pNameCache = pNameCache->pNext;
        delete pDel;
    }

    //clean up winsock
    WSACleanup();

    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::~CObexBTHTransport()\n"));
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::Init(void)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::Init()\n"));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::Shutdown(void)
{
        return S_OK;
}

HRESULT STDMETHODCALLTYPE
//singleton that holds a socket object
CObexBTHTransport::CreateSocket(LPPROPERTYBAG2 pPropertyBag,
                                 LPSOCKET  *ppSocket)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::CreateSocket()\n"));

    *ppSocket = new CBTHTransportSocket();
    if( !(*ppSocket) )
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObexBTHTransport::CreateSocketBlob(unsigned long ulSize,
                                     byte __RPC_FAR *pbData,
                                     LPSOCKET __RPC_FAR *ppSocket)
{
        return E_NOTIMPL;
}


typedef int (*TBthPerformInquiry) (unsigned int LAP, unsigned char length, unsigned char num_responses, unsigned int cBuffer, unsigned int *pcRequired, BthInquiryResult *pir);

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::EnumDevices(LPPROPERTYBAGENUM *ppDevices)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices()\n"));
    HRESULT hRet = E_FAIL;
    HINSTANCE hLib = NULL;
    TBthPerformInquiry pBthPerformInquiry = NULL;
    BthInquiryResult *pIR = NULL;
    int iNumDevices = 0;
    int i;
    CPropertyBagEnum *pPropEnum = NULL;
    CPropertyBag *pPropBag = NULL;
    VARIANT var;
    VariantInit(&var);

    //
    // verify input params
    if (!ppDevices)
    {
        hRet = E_POINTER;
        goto Cleanup;
    }

    //
    // initialize input param
    *ppDevices = 0;


    //
    // load the btdrt dll, and get the BthPerformInquiry function
    if (NULL == (hLib = LoadLibrary (L"btdrt.dll")))
    {
        hRet = E_FAIL;
        goto Cleanup;
    }

    if (NULL == (pBthPerformInquiry = (TBthPerformInquiry)GetProcAddress (hLib, L"BthPerformInquiry")))
    {
        hRet = E_FAIL;
        goto Cleanup;
    }

    if(NULL == (pIR = new BthInquiryResult[BTH_INQUIRY_SIZE]))
    {
        hRet = E_OUTOFMEMORY;
        goto Cleanup;
    }

    if (ERROR_SUCCESS != pBthPerformInquiry(BT_ADDR_GIAC, BTH_INQUIRY_LEN, 0, BTH_INQUIRY_SIZE, (unsigned int *)&iNumDevices, pIR))
    {
        hRet = E_FAIL;
        goto Cleanup;
    }

    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices - Inquiry returned following devices...:\n"));
#if defined (DEBUG) || defined (_DEBUG)
    for (i = 0 ; i < iNumDevices ; ++i)
       DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices - (%04x%08x)\n", GET_NAP(pIR[i].ba), GET_SAP(pIR[i].ba)));
#endif


    if (NULL == (pPropEnum = new CPropertyBagEnum()))
    {
        hRet = E_OUTOFMEMORY;
        goto Cleanup;
    }


    for(i=0; i < iNumDevices; ++i)
    {
        if (NULL == (pPropBag = new CPropertyBag()))
        {
            hRet = E_OUTOFMEMORY;
            goto Cleanup;
        }

        //
        //  The name and the port will be the same (VB style BSTR here)
        //
        USHORT name[30];
        StringCchPrintfW(name, _countof(name), L"%04x%08x", GET_NAP(pIR[i].ba), GET_SAP(pIR[i].ba));
        var.vt = VT_BSTR;
        var.bstrVal = SysAllocString(name);

        // set the name and the address (again, they are the same)
        hRet = pPropBag->Write(c_szDevicePropAddress, &var);
            SVSUTIL_ASSERT(SUCCEEDED(hRet));
        hRet = pPropBag->Write(c_szDevicePropName, &var);
            SVSUTIL_ASSERT(SUCCEEDED(hRet));

        //
        //  Put a marker in saying that the name has NOT been completed
        //
        VariantClear(&var);
        var.vt = VT_I4;
        var.lVal = 0;
        hRet = pPropBag->Write(c_szDevicePropNameCompleted, &var);

        // add the transport ID
        LPOLESTR pszClsid = NULL;
        hRet = StringFromCLSID((REFCLSID) CLSID_BthTransport, &pszClsid);
           SVSUTIL_ASSERT(SUCCEEDED(hRet));
        var.vt = VT_BSTR;
        var.bstrVal = SysAllocString(pszClsid);
        CoTaskMemFree(pszClsid);

        hRet = pPropBag->Write(c_szDevicePropTransport, &var);
        SVSUTIL_ASSERT(SUCCEEDED(hRet));

        VariantClear (&var);

        // add to the enumeration
        pPropEnum->Insert(pPropBag);
        pPropBag->Release();
        pPropBag = NULL;
    }

    *ppDevices = pPropEnum;
    pPropEnum = NULL; // <-- set it null so we dont release it
    hRet = S_OK;

Cleanup:
    if(hLib)
        FreeLibrary(hLib);
    if(pPropEnum)
        pPropEnum->Release();
    if(pPropBag)
        pPropBag->Release();
    if(pIR)
        delete [] pIR;
    VariantClear(&var);

    return hRet;
}


/**********************************************************************************/
/*                                                                                  */
/*  NameQueryHelper:                                                              */
/*                                                                                  */
/*  Passed in is the VARIANT pointer NameVar                                      */
/*    it is just passed in (even though itsin the bag) because this is               */
/*      just a helper for UpdateDeviceProperties...                                  */
/*                                                                                  */
/*  Returned:                                                                      */
/*        HRESULT describing what happened... FAILURE only happens something bad    */
/*            happened                                                                */
/*                                                                                  */
/*        uiUpdateStatus is in/out -- it returns the status after being run          */
/**********************************************************************************/
HRESULT
CObexBTHTransport::NameQueryHelper(LPPROPERTYBAG2 pPropBag,
                                   VARIANT *pNameVar,
                                   BT_ADDR ba,
                                   UINT *uiUpdateStatus)
{
    HRESULT hRes = E_FAIL;

    //
    //  First consult our name cache... if the BT name is there, dont SDP it!
    //
    NAME_CACHE_NODE *pNameCacheTemp = pNameCache;
    while(pNameCacheTemp)
    {
        //oh yeah! the name is cached no SDP query needed!
        if(GET_NAP(ba) == GET_NAP(pNameCacheTemp->ba) &&
           GET_SAP(ba) == GET_SAP(pNameCacheTemp->ba))
        {
            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE, (L"[BT] -- UpdateDeviceProperties: Found name in cache! no need to SDP! yippie!\n"));

            hRes = pPropBag->Write(c_szDevicePropName, &pNameCacheTemp->var);
            SVSUTIL_ASSERT(SUCCEEDED(hRes));

               VARIANT NameCompletedVar;
            VariantInit(&NameCompletedVar);
            NameCompletedVar.vt = VT_I4;
            NameCompletedVar.lVal = 1;
            hRes = pPropBag->Write(c_szDevicePropNameCompleted, &NameCompletedVar);
               SVSUTIL_ASSERT(SUCCEEDED(hRes));
            VariantClear(&NameCompletedVar);


            *uiUpdateStatus = *uiUpdateStatus | FOUND_NAME;
            return S_OK;
        }
        pNameCacheTemp = pNameCacheTemp->pNext;
    }


    //
    //  If the name couldnt be found in our cache, we need to do a query for it
    //
    WCHAR szName[_MAX_PATH];
    unsigned int dwLength = 0;
    szName[0] = '\0';

    typedef int (*TBthRemoteNameQuery) (BT_ADDR *pba, unsigned int cBuffer, unsigned int *pcRequired, WCHAR *szString);

    HINSTANCE hLib = LoadLibrary (L"btdrt.dll");
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE, (L"[BT] -- UpdateDeviceProperties: getting name\n"));

    if (hLib)
    {
        TBthRemoteNameQuery pBthRemoteNameQuery = (TBthRemoteNameQuery)GetProcAddress (hLib, L"BthRemoteNameQuery");

        if (pBthRemoteNameQuery)
        {
            int iErr = pBthRemoteNameQuery (&ba, _MAX_PATH, &dwLength, szName);
            FreeLibrary(hLib);

            if(ERROR_SUCCESS != iErr)
            {
                szName[0] = L'\0';
                return (ERROR_TIMEOUT==iErr)?S_OK:E_FAIL;
            }
            else if ((dwLength <= 0) || (dwLength >= _MAX_PATH))
            {
                szName[0] = L'\0';
                return S_OK;
            }
        }
        else
        {
            FreeLibrary(hLib);
            return E_FAIL;
        }
    }
    else
        return E_FAIL;

    if (dwLength >= 1)
    {
        //if we have a name better than the one we had
        if(dwLength > 1)
        {
            VariantClear(pNameVar);

            pNameVar->vt = VT_BSTR;
            pNameVar->bstrVal = SysAllocString(szName);

            //put the newly found name into our name cache
            NAME_CACHE_NODE *pNewCacheNode = new NAME_CACHE_NODE();
            if(!pNewCacheNode) {
                return E_OUTOFMEMORY;
            }
            memcpy(&pNewCacheNode->ba,&ba,sizeof(BT_ADDR));
            VariantInit(&pNewCacheNode->var);
            hRes = VariantCopy(&pNewCacheNode->var, pNameVar);
                SVSUTIL_ASSERT(SUCCEEDED(hRes));

            if(FAILED(hRes))
                return hRes;

            pNewCacheNode->pNext = pNameCache;
            pNameCache = pNewCacheNode;

            hRes = pPropBag->Write(c_szDevicePropName, pNameVar);
                SVSUTIL_ASSERT(SUCCEEDED(hRes));
            if(FAILED(hRes))
                return hRes;
        }

        VARIANT NameCompletedVar;
        VariantInit(&NameCompletedVar);
        NameCompletedVar.vt = VT_I4;
        NameCompletedVar.lVal = 1;
        hRes= pPropBag->Write(c_szDevicePropNameCompleted, &NameCompletedVar);
           SVSUTIL_ASSERT(SUCCEEDED(hRes));
        VariantClear(&NameCompletedVar);

        //update status on what we have found
        *uiUpdateStatus = *uiUpdateStatus | FOUND_NAME;
        return S_OK;
    }
    return E_FAIL;
}


/**********************************************************************************/
/*
/*  ChannelQueryHelper:
/*
/*  Passed in are the VARIANT pointers (NameVar, NameCompletedVar, and AddressVar
/*    they are passed in (even though they are in the bag) because this is
/*      just a helper for UpdateDeviceProperties...
/*
/*  Returned:
/*        HRESULT describing what happened... FAILURE only happens if the
/*            device doesnt support OBEX or something bad happened
/*
/*        _ppNewBagEnum will return a properybag enum describing all newly discovered
/*            devices
/*
/*        uiUpdateStatus is in/out -- it returns the status after being run
/**********************************************************************************/
HRESULT
CObexBTHTransport::ChannelQueryHelper(LPPROPERTYBAG2 pPropBag,
                                     VARIANT *pNameVar,
                                     VARIANT *pNameCompletedVar,
                                     VARIANT *pAddressVar,
                                     BT_ADDR ba,
                                     IPropertyBagEnum **_ppNewBagEnum,
                                     UINT *uiUpdateStatus,
                                     BOOL fGetJustEnoughToConnect)
{
    HRESULT hr;
    BT_CHANNEL_LIST *pList = NULL;
    CPropertyBagEnum *pNewBagEnum = NULL;
    WCHAR *pRequestedService = NULL;
    WCHAR  szDefaultService1[] = L"{00001105-0000-1000-8000-00805f9b34fb}";
    WCHAR  szDefaultService2[] = L"{00001106-0000-1000-8000-00805f9b34fb}";

    BOOL fHasDefaultService = FALSE;
    BOOL fFoundEnoughToConnectOn = FALSE;

    VARIANT varTransportVar;
    VARIANT varUUID;
    VARIANT varPort;
    VARIANT varRequestedService;

    VariantInit(&varUUID);
    VariantInit(&varTransportVar);
    VariantInit(&varPort);
    VariantInit(&varRequestedService);

    //
    //  If the query succeeded but no list was returned, the device doesnt support
    //       obex so we return with an error.
    //
    //Check if this device supports Obex services : (Obex UUID = UUID16 8)
    hr = QueryForChannel(ba, &pList);
    if(SUCCEEDED(hr) && (NULL == pList)) {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices-Device (%04x%08x) does not support Obex.\n", GET_NAP(ba), GET_SAP(ba)));
        hr = E_FAIL;
        goto Done;
    }
    else if(FAILED(hr)) {
        SVSUTIL_ASSERT(!pList);
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices-Device (%04x%08x) query failed, will retry\n", GET_NAP(ba), GET_SAP(ba)));
        hr = S_OK;
        goto Done;
    }


    //
    //  Loop through all the returned nodes adding them (and creating new
    //    devices if required)
    //
    hr = pPropBag->Read(c_szDevicePropTransport, &varTransportVar, 0);
    if(pList && SUCCEEDED(hr)) {
        *uiUpdateStatus = *uiUpdateStatus | FOUND_TRANSPORT;
    }

    hr = pPropBag->Read(c_szDeviceRequestedServiceUUID ,&varRequestedService, 0);
    if(SUCCEEDED(hr) &&
       VT_BSTR == varRequestedService.vt) {
        pRequestedService = varRequestedService.bstrVal;
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices-Device (%04x%08x) trying to use default service %s\n", GET_NAP(ba), GET_SAP(ba),pRequestedService));
    } 

    while(pList)
    {
        //
        // Make sure everything is cleaned up
        VariantClear(&varUUID);
        VariantClear(&varPort);

        //
        //  Count UUID's, and make a string of them... will be placed into
        //        property bag below
        //
        if(pList->pUUIDList)
        {
            UINT cUUID = 0;
            BT_UUID16_LIST *pUUIDTemp = pList->pUUIDList;
            WCHAR UUIDString[39 * 3];
            WCHAR *pUUIDString = UUIDString;
            WCHAR *pUUIDTempString = pUUIDString;
            size_t UUIDTempStringLength = 0;

            while(pUUIDTemp) {
                cUUID++;
                pUUIDTemp = pUUIDTemp->pNext;
            }

            //36 = size of GUID + 2 for {}'s and 1 for ; (or null on last)
            if(sizeof(UUIDString) < (cUUID * 39 * sizeof(WCHAR))) {
                pUUIDTempString = pUUIDString = new WCHAR[(cUUID * 39)];
                UUIDTempStringLength = cUUID * 39;

                if(NULL == pUUIDString) {
                    hr = E_OUTOFMEMORY;
                    goto Done;
                }
            }
            else
            {
                UUIDTempStringLength = 39 * 3;
            }

            pUUIDTemp = pList->pUUIDList;
            while(pUUIDTemp) {
                int len;
                size_t nRemaining;

                if(pUUIDTemp->pNext)
                {
                    StringCchPrintfExW(pUUIDTempString, UUIDTempStringLength, NULL, &nRemaining,  0, GUID_FORMAT_SEP, GUID_ELEMENTS(pUUIDTemp->uuid));
                }
                else
                {
                    StringCchPrintfExW(pUUIDTempString, UUIDTempStringLength, NULL, &nRemaining,  0, GUID_FORMAT, GUID_ELEMENTS(pUUIDTemp->uuid));
                }

                len = (int)(UUIDTempStringLength - nRemaining);

                //
                // see if this UUID is for the request services or one of the default services (do a wcsstr since a ; may bein the tempstring)
                if(pRequestedService) {
                    if(pUUIDTempString == wcsstr(pUUIDTempString, pRequestedService)) {
                        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ChannelQueryHelper- found requested service!\n"));
                        fHasDefaultService = TRUE;
                    }
                } else if (pUUIDTempString == wcsstr(pUUIDTempString, szDefaultService1) || pUUIDTempString == wcsstr(pUUIDTempString, szDefaultService2)) {
                    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ChannelQueryHelper- found default service!\n"));
                    fHasDefaultService = TRUE;
                }
                pUUIDTempString += len;

                BT_UUID16_LIST *pDelNode = pUUIDTemp;
                pUUIDTemp = pUUIDTemp->pNext;
                delete pDelNode;
            }

            pList->pUUIDList = NULL;

            varUUID.vt = VT_BSTR;
            varUUID.bstrVal = SysAllocString(pUUIDString);

            if(pUUIDString != UUIDString) {
                delete [] pUUIDString;
            }
        }

        //if there are more nodes, clone this one and add it on
        if(_ppNewBagEnum && pList->pNext)
        {
            CPropertyBag *pNewBag = NULL;

            if(NULL == pNewBagEnum) {
                pNewBagEnum = new CPropertyBagEnum();
                if(NULL == pNewBagEnum) {
                    hr = E_OUTOFMEMORY;
                    goto Done;
                }

                *_ppNewBagEnum = pNewBagEnum;
            }

            if(NULL == (pNewBag = new CPropertyBag())) {
                delete pNewBagEnum;
                hr = E_OUTOFMEMORY;
                goto Done;
            }

            //fill in fields that we care about
            pNewBag->Write(c_szDevicePropAddress, pAddressVar);

            //fill in fields that we care about
            if(VT_EMPTY != varUUID.vt)
                pNewBag->Write(c_szDeviceServiceUUID, &varUUID);

            if(*uiUpdateStatus & FOUND_TRANSPORT)
                pNewBag->Write(c_szDevicePropTransport, &varTransportVar);

            if(*uiUpdateStatus & FOUND_NAME)
                pNewBag->Write(c_szDevicePropName, pNameVar);

            if(pNameCompletedVar)
                pNewBag->Write(c_szDevicePropNameCompleted, pNameCompletedVar);

            //
            // Add the port
            varPort.vt = VT_I4;
            varPort.lVal = pList->ulChannel;
            hr = pNewBag->Write(c_szPort, &varPort);
            SVSUTIL_ASSERT(SUCCEEDED(hr));

            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ChannelQueryHelper-Device (%04x%08x) running Obex server on channel %d\n", GET_NAP(ba), GET_SAP(ba), pList->ulChannel));

            //insert the new bag into the linked list
            pNewBagEnum->Insert(pNewBag);
            pNewBag->Release();
        }
        else if(FALSE == fGetJustEnoughToConnect ||
               ((TRUE == fGetJustEnoughToConnect) && (TRUE == fHasDefaultService))) {
                varPort.vt = VT_I4;
                varPort.lVal = pList->ulChannel;
                hr = pPropBag->Write(c_szPort, &varPort);
                SVSUTIL_ASSERT(SUCCEEDED(hr));

                //add the service UUID
                if(VT_EMPTY != varUUID.vt)
                    pPropBag->Write(c_szDeviceServiceUUID, &varUUID);

                DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::ChannelQueryHelper-Device (%04x%08x) running Obex server on channel %d -- stopping here\n", GET_NAP(ba), GET_SAP(ba), pList->ulChannel));

                if(TRUE == fGetJustEnoughToConnect) {
                    fFoundEnoughToConnectOn = TRUE;
                }
                break;
        }

        BT_CHANNEL_LIST *pDel = pList;
        pList = pList->pNext;
        DeleteBtChannelListNode(pDel);
    }

    if(TRUE == fGetJustEnoughToConnect && (FALSE == fFoundEnoughToConnectOn)) {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::EnumDevices - Device (%04x%08x) didnt have enough to connect to! failing\n", GET_NAP(ba), GET_SAP(ba)));
        hr = E_FAIL;
        goto Done;
    }

    //
    // Success!
    *uiUpdateStatus = *uiUpdateStatus | FOUND_PORT;
    *uiUpdateStatus = *uiUpdateStatus | CAN_CONNECT;
    hr = S_OK;

    Done:
        while(pList) {
            BT_CHANNEL_LIST *pDel = pList;
            pList = pList->pNext;
            DeleteBtChannelListNode(pDel);
        }

        VariantClear(&varUUID);
        VariantClear(&varTransportVar);
        VariantClear(&varPort);
        VariantClear(&varRequestedService);
        return hr;
}



//NOTE:  passing NULL to _ppNewBagEnum results in just the existing bag being updated... no enum
//  will be returned
HRESULT STDMETHODCALLTYPE
CObexBTHTransport::UpdateDeviceProperties(LPPROPERTYBAG2 pPropBag,
                                          IPropertyBagEnum **_ppNewBagEnum,
                                          BOOL fGetJustEnoughToConnect,
                                          UINT *uiUpdateStatus)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties on bag: 0x%x\n", (int)pPropBag));

    SVSUTIL_ASSERT(pPropBag && uiUpdateStatus);
    BOOL fAbort;
    HRESULT hRes = S_OK;

    // if we are told to abort, do so
    IsAborting(&fAbort);
    if(TRUE == fAbort) {
        return E_ABORT;
    }

    if(fGetJustEnoughToConnect)
    {
        SVSUTIL_ASSERT(!_ppNewBagEnum);
    }
    if(_ppNewBagEnum)
    {
        SVSUTIL_ASSERT(!fGetJustEnoughToConnect);
    }

    if (!pPropBag || !uiUpdateStatus)
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties -- INVALID PARAM!\n"));
        return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WINDOWS, ERROR_INVALID_PARAMETER);
    }

    if(_ppNewBagEnum)
        *_ppNewBagEnum = NULL;

    //set the mask to have nothing
    *uiUpdateStatus = 0;

    //varaibles
    BT_ADDR ba;

    //
    //  See how much information we already have
    //
    VARIANT AddressVar;
    VARIANT NameVar;
    VARIANT NameCompletedVar;
    VARIANT PortVar;

    BOOL fHaveNameCompleted = FALSE;

    VariantInit(&AddressVar);
    VariantInit(&NameVar);
    VariantInit(&NameCompletedVar);
    VariantInit(&PortVar);


    //grab the address and setup the BT_ADDR struct
    if ((FAILED(pPropBag->Read(c_szDevicePropAddress, &AddressVar, NULL))) || (AddressVar.vt != VT_BSTR))
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateDeviceProperties-Cant get BTA from BSTR!\n"));
        VariantClear(&AddressVar);
        return  E_FAIL;
    }
    // move in the device id for the selected device
    else if(!CObexBTHTransport::GetBA(AddressVar.bstrVal, &ba))
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties-Device addrress missing in property bag.\n"));
        VariantClear(&AddressVar);
        return E_FAIL;
    }

    //see how far we have progressed by reading from the property bag
    if(SUCCEEDED(pPropBag->Read(c_szDevicePropName, &NameVar, 0)))
        *uiUpdateStatus = *uiUpdateStatus | FOUND_NAME;
    if(SUCCEEDED(pPropBag->Read(c_szDevicePropNameCompleted, &NameCompletedVar, 0)))
        fHaveNameCompleted = TRUE;
    if(SUCCEEDED(pPropBag->Read(c_szPort, &PortVar, 0)))
        *uiUpdateStatus = *uiUpdateStatus | CAN_CONNECT | FOUND_PORT;


    if( !(g_dwObexCaps & SEND_DEVICE_UPDATES))
    {
        //lie about finding a port for PPC2002
        if(!fGetJustEnoughToConnect)
           *uiUpdateStatus |= FOUND_PORT;
        else
           *uiUpdateStatus &= ~(FOUND_PORT);
    }

    //
    //  If we have enough to connect, just return (w/o query) otherwise,
    //     we will go after the name/port.... UNLESS fGetJustEnoughToConnect is set
    //     then (assuming we can connect) just send back okay
    if(  (*uiUpdateStatus & FOUND_NAME) &&
         (*uiUpdateStatus & FOUND_PORT) &&
         (fHaveNameCompleted && 1 == NameCompletedVar.lVal)
      )
    {
        DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties - got enough to connect\n"));
        *uiUpdateStatus = 0xFFFFFFFF;
        hRes = S_OK;
    }
    else if(fGetJustEnoughToConnect && (*uiUpdateStatus & FOUND_PORT))
    {
        hRes = S_OK;
    }
    //if no name query for one
    else if(!fGetJustEnoughToConnect &&
            (0 == (*uiUpdateStatus & FOUND_NAME) ||
            !fHaveNameCompleted ||
            (fHaveNameCompleted &&  0 == NameCompletedVar.lVal)))
    {
        hRes = NameQueryHelper(pPropBag,
                               &NameVar,
                               ba,
                               uiUpdateStatus);
        if(FAILED(hRes))
            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties - NameQueryHelper FAILED\n"));

    }

    //if not port, query for one
    else if(0 == (*uiUpdateStatus & FOUND_PORT))
    {
        //ChannelQueryHelper return E_ if no OBEX
        //  otherwise, S_ (we might need to ask again)
        hRes = ChannelQueryHelper(pPropBag,
                                  &NameVar,
                                  &NameCompletedVar,
                                  &AddressVar,
                                  ba,
                                  _ppNewBagEnum,
                                  uiUpdateStatus,
                                  fGetJustEnoughToConnect);
        if(FAILED(hRes))
            DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::UpdateProperties - ChannelQueryHelper FAILED\n"));
    }




    //clean out all variants (we have what we need)
    VariantClear(&AddressVar);
    VariantClear(&NameVar);
    VariantClear(&PortVar);
    VariantClear(&NameCompletedVar);

     if(*uiUpdateStatus == 0xFFFFFFFF)
         return S_OK;
     else if(SUCCEEDED(hRes))
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_WINDOWS, ERROR_CONTINUE);
     else
        return hRes;
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::EnumProperties(LPPROPERTYBAG2 __RPC_FAR *ppProps)
{
     return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE
CObexBTHTransport::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexDevicePropBag::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CObexBTHTransport::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObexDevicePropBag::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_BTHTRANSPORT_ZONE,(L"CObexBTHTransport::QueryInterface()\n"));
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

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::Abort()
{
    fIsAborting = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::Resume()
{
    fIsAborting = FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObexBTHTransport::IsAborting(BOOL *_fIsAborting)
{
    *_fIsAborting = fIsAborting;
    return S_OK;
}


int
CObexBTHTransport::GetBA (WCHAR *pp, BT_ADDR *pba)
{
    while (*pp == ' ')
        ++pp;


    for (int i = 0 ; i < 12 ; ++i, ++pp)
    {
        if (! iswxdigit (*pp))
            return FALSE;

        int c = *pp;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        *pba = (*pba << 4) + c;
    }

    if ((*pp != ' ') && (*pp != '\0'))
        return FALSE;

    return TRUE;
}
