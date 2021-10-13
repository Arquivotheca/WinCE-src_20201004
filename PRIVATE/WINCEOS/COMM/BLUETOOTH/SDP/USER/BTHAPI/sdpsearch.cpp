//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SdpSearch.cpp : Implementation of CSdpSearch
#include "stdafx.h"
#include "BthAPI.h"
#include "SdpSearch.h"

/////////////////////////////////////////////////////////////////////////////
// CSdpSearch


#define PRELOG()    if (hRadio == INVALID_HANDLE_VALUE || hConnection == 0) return E_ACCESSDENIED;

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
STDMETHODIMP CSdpSearch::Begin(ULONGLONG *pAddress, ULONG fConnect) { return E_NOTIMPL; }
STDMETHODIMP CSdpSearch::End() { return E_NOTIMPL; }

STDMETHODIMP
CSdpSearch::ServiceSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    ULONG *pHandles,
    USHORT *pNumHandles
    )
{ return E_NOTIMPL; }


STDMETHODIMP
CSdpSearch::AttributeSearch(
    ULONG handle,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord **ppSdpRecord
    )
{ return E_NOTIMPL; }

STDMETHODIMP
CSdpSearch::ServiceAndAttributeSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord ***pppSdpRecords,
    ULONG *pNumRecords
    )
{ return E_NOTIMPL; }


#else // UNDER_CE
CSdpSearch::~CSdpSearch()
{
    //
    // Clean up a connection if needed
    //
    End();
}

void
CSdpSearch::LocalRadioInsertion(
    TCHAR *pszDeviceName,
    BOOL Enumerated
    )
{
    if (Enumerated == FALSE) {
        return;
    }

    if (hRadio == INVALID_HANDLE_VALUE) {
        hRadio = CreateFile(pszDeviceName,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, 
                            OPEN_EXISTING,
                            0, 
                            NULL); 
    }
}

#define VALID_CONNECT_FLAGS (SDP_SEARCH_LOCAL | SDP_SEARCH_CACHED)

STDMETHODIMP CSdpSearch::Begin(ULONGLONG *pAddress, ULONG fConnect)
{
    HRESULT hres = E_FAIL;

    //
    // Make sure an address is specified (unless we are searching locally)
    //
    if (pAddress == NULL && (fConnect & SDP_SEARCH_LOCAL) == 0) {
        return E_INVALIDARG;
    }

    


    if ((fConnect & ~VALID_CONNECT_FLAGS) != 0) {
        return E_INVALIDARG;
    }

    if (hConnection != 0 || hRadio != INVALID_HANDLE_VALUE) {
        //
        // kill the current connection
        //
        End();
    }

    EnumerateRadios();

    if (hRadio != INVALID_HANDLE_VALUE) {
        if (fConnect & SDP_SEARCH_LOCAL) {
            hConnection = HANDLE_SDP_LOCAL;
            hres = S_OK;
        }
        else {
            BthConnect connect = { 0 };
            DWORD dwActual = 0;
            BOOL result;

            connect.btAddress = *pAddress;

            result = DeviceIoControl(hRadio,
                                     IOCTL_BTH_SDP_CONNECT,
                                     &connect,
                                     sizeof(connect),
                                     &connect,
                                     sizeof(connect),
                                     &dwActual,
                                     NULL);

            if (result) {
                hConnection = connect.hConnection;
                hres = S_OK;
            }
            else {
                CloseHandle(hRadio);
                hRadio = INVALID_HANDLE_VALUE;
                hres = HRESULT_FROM_WIN32(GetLastError());
            }
        }
    }
    else {
        
        hres = E_FAIL;
    }

    return hres;
}

STDMETHODIMP CSdpSearch::End()
{
    if (hRadio != INVALID_HANDLE_VALUE) {
        if (hConnection != 0 && hConnection != HANDLE_SDP_LOCAL) {
            BthDisconnect dc;
            DWORD dwActual;

            dc.hConnection = hConnection;

            DeviceIoControl(hRadio,
                            IOCTL_BTH_SDP_DISCONNECT,
                            &dc,
                            sizeof(dc),
                            NULL,
                            0,
                            &dwActual,
                            NULL);

            hConnection = 0;
        }

        CloseHandle(hRadio);
        hRadio = INVALID_HANDLE_VALUE;
    }

	return S_OK;
}

STDMETHODIMP
CSdpSearch::ServiceSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    ULONG *pHandles,
    USHORT *pNumHandles
    )
{
    PRELOG();

    if (pHandles == NULL || pHandles == NULL ||
        pUuidList == NULL || listSize == 0 || listSize > MAX_UUIDS_IN_QUERY) {
        return E_INVALIDARG;
    }

    BthSdpServiceSearchRequest ssr = { 0 };
    DWORD dwActual = 0;
    HRESULT hres;
    BOOL result;

    ssr.hConnection = hConnection;
    CopyMemory(ssr.uuids, pUuidList, listSize * sizeof(SdpQueryUuid));

    result = DeviceIoControl(hRadio,
                             IOCTL_BTH_SDP_SERVICE_SEARCH,
                             &ssr,
                             sizeof(ssr),
                             pHandles,
                             *pNumHandles * sizeof(ULONG),
                             &dwActual,
                             NULL);

    if (result) {
        *pNumHandles = (USHORT) (dwActual / sizeof(ULONG));
        hres = S_OK;
    }
    else {
        hres = HRESULT_FROM_WIN32(GetLastError());
    }

	return hres;
}

STDMETHODIMP
CSdpSearch::AttributeSearch(
    ULONG handle,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord **ppSdpRecord
    )
{
    PRELOG();

    if (pRangeList == NULL || numRanges == 0 || ppSdpRecord == NULL) {
        return E_INVALIDARG;
    }

    *ppSdpRecord = NULL;

    BthSdpStreamResponse *pResponse = NULL;
    BthSdpAttributeSearchRequest *pSearch = NULL;
    ULONG sizeSearch, sizeResponse;
    HRESULT hres = E_FAIL;
    BOOL result;
    DWORD dwActual;

    sizeSearch = sizeof(*pSearch) + (sizeof(SdpAttributeRange) * (numRanges - 1));
    pSearch = (BthSdpAttributeSearchRequest*) LocalAlloc(LPTR, sizeSearch);

    if (pSearch != NULL) {
        CopyMemory(pSearch->range, pRangeList, numRanges * sizeof(SdpAttributeRange));
        pSearch->hConnection = hConnection;
        pSearch->recordHandle = handle;

        //
        // Start out with 4K
        //
        sizeResponse = 4096;
        pResponse = (BthSdpStreamResponse *) LocalAlloc(LPTR, sizeResponse);
        if (pResponse) {
            while (TRUE) {
                result = DeviceIoControl(hRadio,
                                         IOCTL_BTH_SDP_ATTRIBUTE_SEARCH,
                                         pSearch,
                                         sizeSearch,
                                         pResponse,
                                         sizeResponse,
                                         &dwActual,
                                         NULL);
                
                if (result) {
                    if (pResponse->responseSize == pResponse->requiredSize) {
                        //
                        // we acquired the attributes successfully
                        //
                        hres = S_OK;
                    }
                    else {
                        //
                        // not enough memory!  retry
                        //
                        LocalFree(pResponse);
                        pResponse = NULL;

                        sizeResponse = pResponse->requiredSize + sizeof(*pResponse);
                        pResponse = (BthSdpStreamResponse*)
                            LocalAlloc(LPTR, sizeResponse);

                        if (pResponse == NULL) {
                            hres = E_OUTOFMEMORY;
                        }
                        else {
                            continue;
                        }
                    }
                }
                else {
                    hres = HRESULT_FROM_WIN32(GetLastError()); 
                }

                break;
            }

            if (SUCCEEDED(hres)) {
                hres = CoCreateInstance(__uuidof(SdpRecord),
                                        NULL,
                                        CLSCTX_INPROC_SERVER,
                                        __uuidof(ISdpRecord),
                                        (LPVOID *) ppSdpRecord);

                if (SUCCEEDED(hres)) {
                    hres = (*ppSdpRecord)->CreateFromStream(pResponse->response,
                                                            pResponse->responseSize);
                    if (!SUCCEEDED(hres)) {
                        (*ppSdpRecord)->Release();
                        *ppSdpRecord = NULL;
                    }
                }
            }
        }
    }
    else {
        hres = E_OUTOFMEMORY;
    }

    if (pSearch) {
        LocalFree(pSearch);
        pSearch = NULL;
    }
    if (pResponse) {
        LocalFree(pResponse);
        pResponse = NULL;
    }

	return hres;
}

STDMETHODIMP
CSdpSearch::ServiceAndAttributeSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord ***pppSdpRecords,
    ULONG *pNumRecords
    )
{
    PRELOG();

    if (pUuidList == NULL || listSize == 0 || listSize > MAX_UUIDS_IN_QUERY ||
        pRangeList == NULL || numRanges == 0 ||
        pppSdpRecords == NULL || pNumRecords == NULL) {
        return E_INVALIDARG;
    }

    *pppSdpRecords = NULL;
    *pNumRecords = 0;

    ISdpStream *pIStream = NULL;
    BthSdpStreamResponse *pResponse = NULL;
    BthSdpServiceAttributeSearchRequest *pSearch = NULL;
    ULONG sizeSearch, sizeResponse;
    HRESULT hres = E_FAIL;
    BOOL result;
    DWORD dwActual;

    hres = CoCreateInstance(__uuidof(SdpStream),
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),
                            (LPVOID *) &pIStream);

    if (!SUCCEEDED(hres)) {
        return hres;
    }

    sizeSearch = sizeof(*pSearch) + (sizeof(SdpAttributeRange) * (numRanges - 1));
    pSearch = (BthSdpServiceAttributeSearchRequest*) LocalAlloc(LPTR, sizeSearch);

    if (pSearch != NULL) {
        CopyMemory(pSearch->uuids, pUuidList, listSize * sizeof(SdpQueryUuid));
        CopyMemory(pSearch->range, pRangeList, numRanges * sizeof(SdpAttributeRange));
        pSearch->hConnection = hConnection;

        //
        // Start out with 4K
        //
        sizeResponse = 4096;
        pResponse = (BthSdpStreamResponse *) LocalAlloc(LPTR, sizeResponse);
        if (pResponse != NULL) {
            while (TRUE) {
                result = DeviceIoControl(hRadio,
                                         IOCTL_BTH_SDP_SERVICE_ATTRIBUTE_SEARCH,
                                         pSearch,
                                         sizeSearch,
                                         pResponse,
                                         sizeResponse,
                                         &dwActual,
                                         NULL);
                
                if (result) {
                    if (pResponse->responseSize == pResponse->requiredSize) {
                        //
                        // we acquired the attributes successfully
                        //
                        hres = S_OK;
                    }
                    else {
                        //
                        // not enough memory!  retry
                        //
                        LocalFree(pResponse);
                        pResponse = NULL;

                        sizeResponse = pResponse->requiredSize + sizeof(*pResponse);
                        pResponse = (BthSdpStreamResponse*)
                            LocalAlloc(LPTR, sizeResponse);

                        if (pResponse == NULL) {
                            hres = E_OUTOFMEMORY;
                        }
                        else {
                            continue;
                        }
                    }
                }
                else {
                    hres = HRESULT_FROM_WIN32(GetLastError()); 
                }

                break;
            }

            if (SUCCEEDED(hres)) {
                hres = pIStream->Validate(pResponse->response,
                                          pResponse->responseSize,
                                          NULL);

                if (SUCCEEDED(hres)) {
                    hres = pIStream->VerifySequenceOf(pResponse->response,
                                                      pResponse->responseSize,
                                                      SDP_TYPE_SEQUENCE,
                                                      NULL,
                                                      pNumRecords);
                    if (SUCCEEDED(hres) && *pNumRecords > 0) {
                        *pppSdpRecords = (ISdpRecord **)
                            CoTaskMemAlloc(sizeof(ISdpRecord*) * (*pNumRecords));

                        if (pppSdpRecords != NULL) {
                            hres = pIStream->RetrieveRecords(pResponse->response,
                                                             pResponse->responseSize,
                                                             *pppSdpRecords,
                                                             pNumRecords);
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

            }
        }
    }
    else {
        hres = E_OUTOFMEMORY;
    }

    //
    // Clean up
    //
    if (pIStream != NULL) {
        pIStream->Release();
        pIStream = NULL;
    }

    if (pSearch != NULL) {
        LocalFree(pSearch);
        pSearch = NULL;
    }
    if (pResponse != NULL) {
        LocalFree(pResponse);
        pResponse = NULL;
    }

	return hres;
}
#endif // UNDER_CE
