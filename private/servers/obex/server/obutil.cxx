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
#include <windows.h>
#include <initguid.h>
#include "obexp.hxx"
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>
#include <intsafe.h>


#if defined (UNDER_CE)
#include <extfile.h>
#endif


#define MAX_SDPRECSIZE    4096

int obutil_IsLocal (WCHAR *szFileName) {
    if ((szFileName[0] == '\\') && (szFileName[1] == '\\'))
        return FALSE;

#if defined (UNDER_CE)
    CEOIDINFO ceOidInfo;

    if (CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK), &ceOidInfo)) {
        WCHAR *pszNetRoot   = ceOidInfo.infDirectory.szDirName;
        DWORD dwNetRootLen = wcslen(pszNetRoot);

        if (wcsnicmp(szFileName, pszNetRoot, dwNetRootLen) == 0)
            return FALSE;
    }
#endif

    return TRUE;
}

static int obutil_HexStringToDword (WCHAR*&lpsz, DWORD &Value, int cDigits, WCHAR chDelim) {
    Value = 0;

    for (int Count = 0; Count < cDigits; Count++, lpsz++)
    {
        if (*lpsz >= '0' && *lpsz <= '9')
            Value = (Value << 4) + *lpsz - '0';
        else if (*lpsz >= 'A' && *lpsz <= 'F')
            Value = (Value << 4) + *lpsz - 'A' + 10;
        else if (*lpsz >= 'a' && *lpsz <= 'f')
            Value = (Value << 4) + *lpsz - 'a' + 10;
        else
            return(FALSE);
    }

    if (chDelim != 0)
        return *lpsz++ == chDelim;
    else
        return TRUE;
}

int obutil_GetGUID (WCHAR *lpsz, GUID *pguid) {
    if (*lpsz++ != '{' )
        return FALSE;

    DWORD dw;

    if (! obutil_HexStringToDword (lpsz, pguid->Data1, sizeof(DWORD)*2, '-'))
        return FALSE;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(WORD)*2, '-'))
        return FALSE;

    pguid->Data2 = (WORD)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(WORD)*2, '-'))
        return FALSE;

    pguid->Data3 = (WORD)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[0] = (BYTE)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, '-'))
        return FALSE;

    pguid->Data4[1] = (BYTE)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[2] = (BYTE)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[3] = (BYTE)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[4] = (BYTE)dw;

    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[5] = (BYTE)dw;

    if (! obutil_HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[6] = (BYTE)dw;
    if (! obutil_HexStringToDword (lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[7] = (BYTE)dw;

    if (*lpsz++ != '}' )
        return FALSE;

    return TRUE;
}

int obutil_PollSocket (SOCKET s) {
    FD_SET fd;
    FD_ZERO (&fd);
// Suppress the warning for the do {} while(0) in the FD_SET macro
#pragma warning(push)
#pragma warning(disable:4127)
    FD_SET (s, &fd);
#pragma warning(pop)

    timeval tv;

    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    return select (0, &fd, NULL, NULL, &tv);
}

static void obutil_SetUUID16(NodeData *pNode, unsigned short uuid16) {
    pNode->type=SDP_TYPE_UUID;
    pNode->specificType=SDP_ST_UUID16;
    pNode->u.uuid16 = uuid16;
}

static void obutil_SetUINT8(NodeData *pNode, unsigned char uuint8) {
    pNode->type= SDP_TYPE_UINT;
    pNode->specificType = SDP_ST_UINT8;
    pNode->u.uint8 = uuint8;
}

static DWORD obutil_AppendContainer(ISdpNodeContainer *pMainContainer, ISdpNodeContainer *pNewContainer) {
    NodeData nodeData;
    nodeData.type=SDP_TYPE_CONTAINER;
    nodeData.specificType=SDP_ST_NONE;
    nodeData.u.container = pNewContainer;
    return pMainContainer->AppendNode(&nodeData);
}


int obutil_SdpDelRecord(ULONG hRecord) {
    BLOB blob;
    BTHNS_SETBLOB delBlob;
    WSAQUERYSET Service;

    blob.cbSize    = sizeof(BTHNS_SETBLOB);
    blob.pBlobData = (PBYTE) &delBlob;

        ULONG ulSdpVersion = BTH_SDP_VERSION;

    memset(&delBlob,0,sizeof(delBlob));
    delBlob.pRecordHandle = &hRecord;
        delBlob.pSdpVersion = &ulSdpVersion;

    memset(&Service,0,sizeof(Service));
    Service.dwSize = sizeof(Service);
    Service.lpBlob = &blob;
    Service.dwNameSpace = NS_BTH;


    HINSTANCE hLib = LoadLibrary(L"btdrt.dll");
    if (!hLib) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: Record too big. Max allowed size=%d\n", MAX_SDPRECSIZE));
        return GetLastError();
    }

    int iErr = ERROR_PROC_NOT_FOUND;
    typedef int (*BTHNSSETSERVICE)(LPWSAQUERYSET, WSAESETSERVICEOP, DWORD);
    BTHNSSETSERVICE pfnBthNsSetService = (BTHNSSETSERVICE )GetProcAddress(hLib, L"BthNsSetService");
    if (pfnBthNsSetService)
        iErr = pfnBthNsSetService(&Service,RNRSERVICE_DELETE,0);

    if (iErr != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: BthNsSetService returns 0x%08x.\n", iErr));
    }
    FreeLibrary(hLib);

    return iErr;
}


static int obutil_SdpAddRecord(unsigned char *pBuf, unsigned long nSize, ULONG *pRecHandle) {
    WSAQUERYSET Service;
    UCHAR *pSDPBuf = new UCHAR[MAX_SDPRECSIZE];

    if(NULL == pSDPBuf)
        return ERROR_OUTOFMEMORY;

    BLOB blob;
    PBTHNS_SETBLOB addBlob = (PBTHNS_SETBLOB) pSDPBuf;

    *pRecHandle = 0;

    //blob.cbSize = sizeof(BTHNS_SETBLOB) + nSize - 1;
    blob.cbSize  = sizeof(BTHNS_SETBLOB) - 1;
    if(FAILED(ULongAdd(nSize, blob.cbSize, &blob.cbSize))) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: Record too big. Max allowed size=%d\n", MAX_SDPRECSIZE));
        delete [] pSDPBuf;
        return ERROR_BUFFER_OVERFLOW;
    }
    blob.pBlobData = (PBYTE) addBlob;

    if (blob.cbSize > MAX_SDPRECSIZE) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: Record too big. Max allowed size=%d\n", MAX_SDPRECSIZE));
        delete [] pSDPBuf;
        ASSERT(FALSE);
        return ERROR_BUFFER_OVERFLOW;
    }

    ULONG ulSdpVersion = BTH_SDP_VERSION;

    addBlob->pRecordHandle  = pRecHandle;
    addBlob->pSdpVersion    = &ulSdpVersion;
    addBlob->fSecurity      = 0;
    addBlob->fOptions       = 0;
    addBlob->ulRecordLength = nSize;

    PREFAST_SUPPRESS(419, "Suppressing b/c blob.cbSize is sizeof(BTHNS_SETBLOB) + nSize - 1,  if this didnt overflow and is less than MAX_SDPRECSIZE we're okay b/c nSize is a component of cbSize & thats checked");
    memcpy(addBlob->pRecord,pBuf,nSize);

    memset(&Service,0,sizeof(Service));
    Service.dwSize = sizeof(Service);
    Service.lpBlob = &blob;
    Service.dwNameSpace = NS_BTH;

    HINSTANCE hLib = LoadLibrary(L"btdrt.dll");
    if (!hLib) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: Record too big. Max allowed size=%d\n", MAX_SDPRECSIZE));
        delete [] pSDPBuf;
        return GetLastError();
    }

    int iErr = ERROR_PROC_NOT_FOUND;
    typedef int (*BTHNSSETSERVICE)(LPWSAQUERYSET, WSAESETSERVICEOP, DWORD);
    BTHNSSETSERVICE pfnBthNsSetService = (BTHNSSETSERVICE )GetProcAddress(hLib, L"BthNsSetService");
    if (pfnBthNsSetService)
        iErr = pfnBthNsSetService(&Service,RNRSERVICE_REGISTER,0);

    if (iErr != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_SdpAddRecord: BthNsSetService returns 0x%08x.\n", iErr));
    }
    FreeLibrary(hLib);
    delete [] pSDPBuf;
    return iErr;
}


// ppSdpRecord: this IN/OUT allows creation of a new ISdpRecord object. If not passed in, an instance will be created
//              and returned - and caller is responsible for releasing the object.
static int obutil_WriteSDPRecord(
    unsigned char *pBuf, unsigned long nSize, unsigned char nPort, ULONG *pulHandle, ISdpRecord** ppSdpRecord) {

    SVSUTIL_ASSERT(pBuf);
    SVSUTIL_ASSERT((nPort > 0) && (nPort < 32));
    SVSUTIL_ASSERT(pulHandle);

    BOOL   fRet = FALSE;
    NodeData  nodeData;
    ISdpNodeContainer *pProtocolContainer = NULL;
    HRESULT hr;
    UCHAR *pSdpRecordStream=NULL;
    DWORD dwSize;

    IClassFactory *pCFContainer = NULL;
    ISdpNodeContainer *pL2CAPContainer = NULL;
    ISdpNodeContainer *pRFCOMMContainer = NULL;
    ISdpNodeContainer *pOBEXContainer = NULL;

    if (!(*ppSdpRecord)) {
        hr = CoCreateInstance(__uuidof(SdpRecord),NULL,CLSCTX_INPROC_SERVER,__uuidof(ISdpRecord),(LPVOID *) ppSdpRecord);
        if (FAILED(hr))
            goto done;
    }

    SVSUTIL_ASSERT(*ppSdpRecord);
    ISdpRecord *pSdpRecord = *ppSdpRecord;

    hr = pSdpRecord->CreateFromStream(pBuf, nSize);
    if (FAILED(hr))
        goto done;

    // remove any existing protocol info the extension may have set.
    pSdpRecord->SetAttribute(SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST,NULL);

    hr = CoGetClassObject(__uuidof(SdpNodeContainer),CLSCTX_INPROC_SERVER,NULL,
                          IID_IClassFactory,(PVOID*)&pCFContainer);

    if (FAILED(hr) || !pCFContainer)
        goto done;

    hr = pCFContainer->CreateInstance(NULL,__uuidof(ISdpNodeContainer),(PVOID*) &pProtocolContainer);

    if (FAILED(hr) || !pProtocolContainer)
        goto done;

    pProtocolContainer->SetType(NodeContainerTypeSequence);

    // L2CAP Entry
    hr = pCFContainer->CreateInstance(NULL,__uuidof(ISdpNodeContainer),(PVOID*) &pL2CAPContainer);

    if (FAILED(hr) || !pL2CAPContainer)
        goto done;

    pL2CAPContainer->SetType(NodeContainerTypeSequence);
    obutil_SetUUID16(&nodeData,L2CAP_PROTOCOL_UUID16);
    if (ERROR_SUCCESS != pL2CAPContainer->AppendNode(&nodeData))
        goto done;

    if (ERROR_SUCCESS != obutil_AppendContainer(pProtocolContainer,pL2CAPContainer))
        goto done;

    //  RFCOMM and cid
    hr = pCFContainer->CreateInstance(NULL,__uuidof(ISdpNodeContainer),(PVOID*) &pRFCOMMContainer);

    if (FAILED(hr) || !pRFCOMMContainer)
        goto done;

    pRFCOMMContainer->SetType(NodeContainerTypeSequence);

    obutil_SetUUID16(&nodeData,RFCOMM_PROTOCOL_UUID16);
    if (ERROR_SUCCESS != pRFCOMMContainer->AppendNode(&nodeData))
        goto done;

    obutil_SetUINT8(&nodeData,nPort); // channel ID!
    if (ERROR_SUCCESS != pRFCOMMContainer->AppendNode(&nodeData))
        goto done;

    if (ERROR_SUCCESS != obutil_AppendContainer(pProtocolContainer,pRFCOMMContainer))
        goto done;

    // OBEX
    hr = pCFContainer->CreateInstance(NULL,__uuidof(ISdpNodeContainer),(PVOID*) &pOBEXContainer);

    if (FAILED(hr) || !pOBEXContainer)
        goto done;

    pOBEXContainer->SetType(NodeContainerTypeSequence);
    obutil_SetUUID16(&nodeData,OBEX_PROTOCOL_UUID16);
    if (ERROR_SUCCESS != pOBEXContainer->AppendNode(&nodeData))
        goto done;

    if (ERROR_SUCCESS != obutil_AppendContainer(pProtocolContainer,pOBEXContainer))
        goto done;

    // insert everything into the record
    nodeData.type=SDP_TYPE_CONTAINER;
    nodeData.specificType=SDP_ST_NONE;
    nodeData.u.container = pProtocolContainer;
    if (ERROR_SUCCESS != pSdpRecord->SetAttribute(SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, &nodeData))
        goto done;

    if (ERROR_SUCCESS != pSdpRecord->WriteToStream(&pSdpRecordStream, &dwSize,0,0))
        goto done;

    if (ERROR_SUCCESS == obutil_SdpAddRecord(pSdpRecordStream, dwSize, pulHandle)) {
        DEBUGMSG(ZONE_INIT, (L"[OBEX] obutil_WriteSDPRecord successful\n"));
        fRet = TRUE;
    }

done:
    if (pL2CAPContainer)
        pL2CAPContainer->Release();

    if (pRFCOMMContainer)
        pRFCOMMContainer->Release();

    if (pOBEXContainer)
        pOBEXContainer->Release();

    if (pCFContainer)
        pCFContainer->Release();

    if (pSdpRecordStream)
        CoTaskMemFree(pSdpRecordStream);

    return fRet;

}

int obutil_RegisterPort(unsigned char nPort, ULONG *pBTHHandleArray, UINT *uiNumInArray, ISdpRecord** ppSdpRecord) {
    SVSUTIL_ASSERT(pBTHHandleArray && uiNumInArray);

    HKEY hKey;
    LONG lRes = ERROR_SUCCESS;
    WCHAR szGUID[128];
    UCHAR *pcData = new UCHAR[MAX_SDPRECSIZE];
    DWORD dwIndex = 0;
    DWORD cName = sizeof(szGUID)/sizeof(WCHAR);
    DWORD cbData;
    UINT uiTotalSlotsAvailable = *uiNumInArray;

    *uiNumInArray = 0;

    if(NULL == pcData)
        return ERROR_OUTOFMEMORY;

    UINT numArraySlotsUsed = 0;

    if ((nPort == 0) || (nPort > 31))  {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_RegisterPort: Invalid port number %d\n", nPort));
        delete [] pcData;
        return ERROR_INVALID_PARAMETER;
    }

    if (ERROR_SUCCESS != (lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Obex\\Services", 0, 0, &hKey))) {
        DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_RegisterPort: Failed to open HKLM\\Software\\Microsoft\\Obex\\Services\n"));
        delete [] pcData;
        return lRes;
    }

    SVSUTIL_ASSERT(hKey);
    BOOL bWritten = FALSE;
    while((ERROR_SUCCESS == (lRes = RegEnumKeyEx(hKey, dwIndex++, szGUID, &cName, NULL, NULL, NULL, NULL))) &&
        (cName < (sizeof(szGUID)/sizeof(WCHAR)) ) ) {
        HKEY hKey1;

        if (ERROR_SUCCESS == (lRes = RegOpenKeyEx(hKey, szGUID, 0, 0, &hKey1))) {
            SVSUTIL_ASSERT(hKey1);

            DWORD dwType = REG_BINARY;
            cbData = MAX_SDPRECSIZE;
            if ( (ERROR_SUCCESS == (lRes = RegQueryValueEx(hKey1, L"BluetoothSdpRecord", NULL, &dwType, pcData, &cbData)))
                && (dwType == REG_BINARY) && (cbData < MAX_SDPRECSIZE) ){

                ULONG ulHandle;

                //if there arent available spaces for the record (handle spaces)
                //   or if writing fails, send back an error
                if (numArraySlotsUsed >= uiTotalSlotsAvailable ||
                    !obutil_WriteSDPRecord(pcData, cbData, nPort, &ulHandle, ppSdpRecord))
                {
                    DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_RegisterPort: obutil_WriteSDPRecord fails for %s\n", szGUID));
                    lRes = ERROR_FUNCTION_FAILED;
                }
                else
                {
                    //
                    //  write down the handle for deletion later
                    //
                    DEBUGMSG(ZONE_ERROR, (L"[OBEX] AddressFamily::Start: cached SDP record handle\n"));
                    pBTHHandleArray[numArraySlotsUsed] = ulHandle;
                    numArraySlotsUsed++;
                    bWritten = TRUE;
                }


            }
            RegCloseKey(hKey1);

        } else {
            DEBUGMSG(ZONE_ERROR, (L"[OBEX] obutil_RegisterPort: RegOpenKeyEx failed for %s\n", szGUID));
        }

        cName = sizeof(szGUID)/sizeof(WCHAR);

    }

    *uiNumInArray = numArraySlotsUsed;

    RegCloseKey(hKey);
    delete [] pcData;

    //return ERROR_SUCCESS if we successfully wrote at least one SDP record
    return ((lRes == ERROR_NO_MORE_ITEMS) && (bWritten)) ? ERROR_SUCCESS : lRes;
}
