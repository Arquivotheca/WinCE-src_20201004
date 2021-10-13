//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SdpRecord.cpp : Implementation of CSdpRecord
#include "stdafx.h"
#include "BthAPI.h"
#include "sdplib.h"
#include "SdpNodeList.h"
#include "SdpRecord.h"
#include "util.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include <mlang.h>
#include <assert.h>

struct MIBEnumToCharset {
    USHORT mibeNum;
    WCHAR* pszCharset;
};

//
// MIBE numbers came from iana website
//      (http://www.isi.edu/in-notes/iana/assignments/character-sets)
//
// matching string names were manually matched from the iana site in the table
//  in nt\shell\ext\mlan\mimedb.cpp
//
const MIBEnumToCharset g_MibeMapping[] = {
    {3,  L"us-ascii"},      // ascii
    {4,  L"iso-8859-1"},    // latin
    {5,  L"iso-8859-2"},    // latin2
    {6,  L"iso-8859-3"},    // latin3
    {7,  L"iso-8859-4"},    // latin4
    {8,  L"iso-8859-5"},    // latin cyrillic
    {9,  L"iso-8859-6"},    // arabic
    {10, L"iso-8859-7"},    // latin greek
    {11, L"iso-8859-8"},    // latin hebrew
    {12, L"iso-8859-9"},    // latin5
    {17, L"ms_Kanji"},      // kanji / japanese
    {18, L"euc-jp"},     
    {19, L"Extended_UNIX_Code_Packed_Format_for_Japanese"}, 
    {25, L"NS_4551-1"},     // danish, norwegian
    {36, L"iso-ir-149"},    // korean
    {37, L"iso-2022-kr" },  // korean
    {38, L"euc-kr" },       // korean
    {39, L"iso-2022-jp" },  // japanese
    {40, L"iso-2022-jp" },  // L"iso-2022-jp-2" really, but no match in IMultiLanguage
    {45, L"csISOLatinGreek" },
    {57, L"GB_2312-80"},
    {85, L"ISO_8859-8-I"},
    {103, L"unicode-1-1-utf-7"},
    {106, L"utf-8"},
    {109, L"iso-8859-13"},
    {111, L"iso-8859-15"},
    {1010, L"csUnicode11"},
    {1012, L"utf-7"},
    {1013, L"UTF-16BE"},
    {1014, L"UTF-16LE"},
    {1015, L"utf-16"},
    {2009, L"ibm850"},
    {2010, L"ibm852"},
    {2011, L"IBM437"},
    {2024, L"csWindows31J"},      
    {2025, L"GB2312"},      // PRC mixed set
    {2026, L"Big5"},        // Chinese for taiwan mb set 
    {2027, L"macintosh"},
    {2028, L"ebcdic-cp-us"},
    {2047, L"ibm857"},
    {2049, L"ibm861"},
    {2054, L"ibm869"},
    {2084, L"koi8-r"},
    {2085, L"hz-gb-2312"},
    {2086, L"ibm866"},
    {2087, L"ibm775"},
    {2088, L"koi8-u"},
    {2250, L"windows-1250"},
    {2251, L"windows-1251"},
    {2252, L"windows-1252"},
    {2253, L"windows-1253"},
    {2253, L"windows-1253"},
    {2254, L"windows-1254"},
    {2255, L"windows-1255"},
    {2256, L"windows-1256"},
    {2257, L"windows-1257"},
    {2258, L"windows-1258"},
    {2259, L"TIS-620"},
};

const ULONG g_MibeMappingCount = sizeof(g_MibeMapping) / sizeof(g_MibeMapping[0]);
#endif // UNDER_CE

#define LOCK_LIST() ListLock l(&listLock)

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
WCHAR* FindCharset(USHORT mibeNum)
{
    ULONG iStart = 0, iEnd = g_MibeMappingCount-1, iMiddle;

    //
    // Binary search through the array (sorted by mibeNum)
    //
    while (iStart <= iEnd) {
        iMiddle = (iStart + iEnd) / 2;
        if (mibeNum == g_MibeMapping[iMiddle].mibeNum) {
            return g_MibeMapping[iMiddle].pszCharset;
        }
        else if (mibeNum > g_MibeMapping[iMiddle].mibeNum) {
            iStart = iMiddle + 1;
        }
        else {
            iEnd = iMiddle - 1;
        }
    }

    return NULL;
}
#endif



/////////////////////////////////////////////////////////////////////////////
// CSdpRecord

void CSdpRecord::Init(void)
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    m_pUnkMarshaler = NULL;
#endif    
    locked = 0;

    InitializeCriticalSection(&listLock);
}

CSdpRecord::~CSdpRecord()
{
    DeleteCriticalSection(&listLock);
    DestroyRecord();
}

void CSdpRecord::LockRecord(UCHAR lock)
{
    LOCK_LIST();

    if (lock) {
        locked++;
    }
    else {
        lock--;
    }

    nodeList.Lock(lock);
}

void CSdpRecord::DestroyRecord(void)
{
    nodeList.Destroy(); 
}

STDMETHODIMP CSdpRecord::CreateFromStream(UCHAR *pStream, ULONG size)
{
    NTSTATUS status;

    if (pStream == NULL) {
        return E_POINTER;
    }

    //
    // validate the format of the stream
    //
    status = SdpIsStreamRecord(pStream, size);
    if (!NT_SUCCESS(status)) {
        return E_INVALIDARG;
    }

    //
    // make sure that each attribute is of the correct format
    //
    USHORT attribId = 0xFFFF;
    status = SdpVerifyServiceRecord(pStream, size, 0, &attribId);
    if (!NT_SUCCESS(status)) {
        return E_INVALIDARG;
    }

    UCHAR type, sizeIndex;

    SdpRetrieveHeader(pStream, type, sizeIndex);
    pStream++;

    ULONG elementSize, storageSize;
    SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);
    
    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

    HRESULT err;

    if (elementSize > 0) {

        DestroyRecord();

        err = nodeList.CreateFromStream(pStream + storageSize, elementSize);

        if (!SUCCEEDED(err)) {
            DestroyRecord();
        }
    }
    else {
        err = S_OK;
    }

	return err;
}

STDMETHODIMP CSdpRecord::WriteToStream(
    UCHAR **ppStream,
    ULONG *pStreamSize,
    ULONG preSize,
    ULONG postSize
    )
{
    if (ppStream == NULL || pStreamSize == NULL) {
        return E_POINTER;
    }

    HRESULT err = S_OK;

    LockRecord(TRUE);
    
    ULONG streamSize, size;
    streamSize = nodeList.GetStreamSize();
    size = streamSize + GetContainerHeaderSize(streamSize);

    PUCHAR stream = (PUCHAR) CoTaskMemAlloc(size + preSize + postSize);
    if (stream) {
        *ppStream = stream;
        *pStreamSize = size;

        ZeroMemory(*ppStream, size + preSize + postSize);

        stream = WriteVariableSizeToStream(SDP_TYPE_SEQUENCE, streamSize, stream + preSize);
        nodeList.WriteStream(stream);
    }

    LockRecord(FALSE);

	return err;
}

STDMETHODIMP CSdpRecord::SetAttribute(USHORT attribute, NodeData *pData)
{
    PSDP_NODE pNode = NULL;

    //
    // pData == NULL signifies removal
    //
    if (pData != NULL) {
        HRESULT err = CreateSdpNodeFromNodeData(pData, &pNode);
        if (!SUCCEEDED(err)) {
            return err;
        }

#if 0
        //
        // Check its format.  For universal attributes, we know the format of the 
        // attribute, so convert this node to a stream and see if it is the correct
        // one
        //
        // Only check if we know the format
        //
        if (attribute <= 0x200) {
            PSDP_NODE pTree;
            PUCHAR stream;
            ULONG size;
            NTSTATUS status;

            pTree = SdpCreateNodeTree();
            if (pTree == NULL) {
                SdpFreeOrphanedNode(pNode);
                return E_OUTOFMEMORY;
            }


            status = SdpAppendNodeToContainerNode(pTree, pNode);
            if (!NT_SUCCESS(status)) {
                SdpFreeOrphanedNode(pNode);
                SdpFreeTree(pTree);
                return MapNtStatusToHresult(status);
            }

            status = SdpStreamFromTree(pTree, &stream, &size);

            if (!NT_SUCCESS(status)) {
                //
                // no need to free the orphan b/c it is now a part of the tree
                //
                SdpFreeTree(pTree);
                return MapNtStatusToHresult(status);
            }

            //
            // verify the attribute value
            //
            status = SdpVerifyServiceRecord(stream,
                                            size,
                                            VERIFY_STREAM_IS_ATTRIBUTE_VALUE,
                                            &attribute);

            SdpFreePool(stream);
            stream = NULL;

            if (!NT_SUCCESS(status)) {
                SdpFreeTree(pTree);
                return MapNtStatusToHresult(status);
            }

            //
            // Get the node out of the linked list so we can insert it into the
            // nodeList
            //
            PLIST_ENTRY entry = Sdp_RemoveHeadList(&pTree->hdr.Link);
            pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);

            Sdp_InitializeListHead(&pNode->hdr.Link);

            //
            // don't need the tree root anymore
            //
            SdpFreeTree(pTree);
        }
#endif // 0
    }

    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

//  NTSTATUS status = SdpAddAttributeToTree((PSDP_NODE) &nodeList, attribute, pNode);
    NTSTATUS status = SdpAddAttributeToNodeHeader(&nodeList, attribute, pNode);

    if (!NT_SUCCESS(status)) {
        return MapNtStatusToHresult(status);
    }

    //
    // successful remove, maintain the node count (id and value are now gone)
    //
    if (pNode == NULL) {
        nodeList.nodeCount -= 2;
    }

	return S_OK;
}

STDMETHODIMP CSdpRecord::SetAttributeFromStream(USHORT attribute, UCHAR *pStream, ULONG size)
{
    PSDP_NODE pNode = NULL;

    if (!NT_SUCCESS(ValidateStream(pStream, size, NULL, NULL, NULL))) {
        return E_INVALIDARG;
    }

    if (!NT_SUCCESS(SdpVerifyServiceRecord(pStream,
                                           size,
                                           VERIFY_STREAM_IS_ATTRIBUTE_VALUE,
                                           &attribute))) {
        return E_INVALIDARG;
    }

    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

    //
    // if pStream or size are 0, then the caller wants to remove the attribute
    //
    if (pStream != NULL && size != 0) {
        SdpNodeList tmpList;
        HRESULT err;

        err = tmpList.CreateFromStream(pStream, size);
        if (!SUCCEEDED(err)) {
            return err;
        }

        //
        // Attribute values can be just one element, whether it be a uint or a
        // container
        //
        if (tmpList.nodeCount != 1) {
            return E_INVALIDARG;
        }

        PLIST_ENTRY entry = Sdp_RemoveHeadList(&tmpList.Link);
        pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);

        Sdp_InitializeListHead(&pNode->hdr.Link);
    }

//  NTSTATUS status = SdpAddAttributeToTree((PSDP_NODE) &nodeList, attribute, pNode);
    NTSTATUS status = SdpAddAttributeToNodeHeader(&nodeList, attribute, pNode);

    if (!NT_SUCCESS(status)) {
        SdpFreeOrphanedNode(pNode);
        return MapNtStatusToHresult(status);
    }

    //
    // successful remove, maintain the node count (id and value are now gone)
    //
    if (pNode == NULL) {
        nodeList.nodeCount -= 2;
    }

	return S_OK;
}

PSDP_NODE CSdpRecord::GetAttributeValueNode(USHORT attribute)
{
    PLIST_ENTRY current = nodeList.Link.Flink;

    while (1) {
        PSDP_NODE pId, pValue;

        pId = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
        current = current->Flink;
        pValue = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);

        if (pId->u.uint16 == attribute) {
            return pValue;
        }

        current = current->Flink;

        if (current == &nodeList.Link) {
            //
            // end of the list, no match
            //
            return NULL;
        }
    }
}

STDMETHODIMP CSdpRecord::GetAttribute(USHORT attribute, NodeData *pData)
{
    if (pData == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    if (nodeList.nodeCount < 2) {
        return E_FAIL;
    }

    PSDP_NODE pNode = GetAttributeValueNode(attribute);

    if (pNode) {
        CreateNodeDataFromSdpNode(pNode, pData);

        if (pNode->hdr.Type == SDP_TYPE_URL) {
            CopyStringDataToNodeData(pNode->u.url,
                                     pNode->DataSize,
                                     &pData->u.url);
        }
        else if (pNode->hdr.Type == SDP_TYPE_STRING) {
            CopyStringDataToNodeData(pNode->u.string,
                                     pNode->DataSize,
                                     &pData->u.str);
        }

        return S_OK;
    }
    else {
        return E_FAIL;
    }
}

STDMETHODIMP CSdpRecord::GetAttributeAsStream(USHORT attribute, UCHAR **ppStream, ULONG *pSize)
{
    if (ppStream == NULL || pSize == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    PSDP_NODE pNode = GetAttributeValueNode(attribute);
    if (pNode) {
        
        if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
            return pNode->u.container->CreateStream(ppStream, pSize);
        }
        else {
            //
            // save the links in the list to be restored later
            //
            LIST_ENTRY e = pNode->hdr.Link;

            //
            // ComputeNodeListSize will iterate over the entire list, so we want to 
            // single this node out for the call
            //
            SDP_NODE listHead;
            SdpInitializeNodeHeader(&listHead.hdr);
            Sdp_InsertTailList(&listHead.hdr.Link,&pNode->hdr.Link);

            ULONG size = 0;
            ComputeNodeListSize(pNode, &size);

            //
            // OK, we are done, restore the links back again
            //
            pNode->hdr.Link = e;

            PUCHAR stream = (PUCHAR) CoTaskMemAlloc(size);
            if (stream == NULL) {
                return E_OUTOFMEMORY;
            }

            ZeroMemory(stream, size);
            WriteLeafToStream(pNode, stream);

            *pSize = size;
            *ppStream = stream;

            return S_OK;
        }
    }
    else {
        return E_FAIL;
    }
}

STDMETHODIMP CSdpRecord::Walk(ISdpWalk *pWalk)
{
    HRESULT err;
//  NodeData ndc;

    if (pWalk == NULL) {
        return E_POINTER;
    }

    LockRecord(TRUE);
    err = nodeList.Walk(pWalk);
    LockRecord(FALSE);

	return err;
}

STDMETHODIMP CSdpRecord::GetAttributeList(USHORT **ppList, ULONG *pListSize)
{
    if (ppList == NULL || pListSize == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    ULONG size = nodeList.nodeCount / 2; 
    USHORT *list;

    list = (PUSHORT) CoTaskMemAlloc(size * sizeof(USHORT));
    if (list == NULL) {
        ZeroMemory(list, size * sizeof(USHORT));

        *ppList = NULL;
        *pListSize = 0;
        return E_OUTOFMEMORY;
    }

    *ppList = list;
    *pListSize = size;

    PLIST_ENTRY entry = nodeList.Link.Flink;

    for (ULONG c = 0; c < size; c++, entry = entry->Flink->Flink, list++) {
        PSDP_NODE pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
        *list = pNode->u.uint16;
    }

	return S_OK;
}

HRESULT CSdpRecord::GetLangBaseInfo(USHORT *pLangId, USHORT *pMibeNum, USHORT *pAttribId)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    return E_NOTIMPL;
#else
    PSDP_NODE pNode;

    //
    // Iterate over the languages supported by this record and see if we 
    // find a match
    //
    pNode = GetAttributeValueNode(SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST);

    if (pNode && pNode->hdr.Type == SDP_TYPE_CONTAINER) {
        ISdpNodeContainer *pCont = pNode->u.container;
        ULONG count, i;

        pCont->GetNodeCount(&count);

        if ((count % 3) != 0) {
            //
            // malformed data
            //
            return E_FAIL;
        }

        //
        // The language ID list has the following format
        // SEQ
        //  UINT16 language
        //  UINT16 character encoding           
        //  UINT16 starting attribute
        //  ... triplets etc ...
        //
        for (i = 0; i < count; /* i incremented in the loop */ ) {
            NodeData nd;
            USHORT langId, mibeNum, id;

            //
            // get the language id
            // 
            pCont->GetNode(i++, &nd);
            if (nd.type != SDP_TYPE_UINT || nd.specificType != SDP_ST_UINT16 ) {
                return E_INVALIDARG;
            }
            langId = nd.u.uint16;

            //
            // grab the encoding scheme
            //
            pCont->GetNode(i++, &nd);
            if (nd.type != SDP_TYPE_UINT || nd.specificType != SDP_ST_UINT16 ) {
                return E_INVALIDARG;
            }
            mibeNum = nd.u.uint16;

            //
            // grab the attribute where this string starts
            //
            pCont->GetNode(i++, &nd);
            if (nd.type != SDP_TYPE_UINT || nd.specificType != SDP_ST_UINT16 ) {
                return E_INVALIDARG;
            }
            id = nd.u.uint16;

            //
            // If pLangid is NULL, then the caller wants the default string.  
            // The  default string is always the first one
            //
            if (pLangId == NULL || *pLangId == langId) {
                *pMibeNum = mibeNum;
                *pAttribId = id;

                return S_OK;
            }
        }
    }

    return E_FAIL;
#endif // UNDER_CE
}

STDMETHODIMP CSdpRecord::GetString(USHORT offset, USHORT *pLangId, WCHAR **ppString)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    return E_NOTIMPL;
#else
    PSDP_NODE pNode;
    HRESULT hres = S_OK;
    USHORT id, mibeNum, langId;

    if (ppString == NULL) {
        return E_INVALIDARG;
    }

    *ppString = NULL;

    LOCK_LIST();

    hres = GetLangBaseInfo(pLangId, &mibeNum, &id);
    if (!SUCCEEDED(hres) && pLangId == NULL) {
        //
        // Assume the defaults since the record itself does not contain any
        // language base information
        //
        mibeNum = 106;          // utf 8
        id = LANG_DEFAULT_ID;   // 0x100
        hres = S_OK;
    }

    if (SUCCEEDED(hres)) {
        pNode = GetAttributeValueNode(id + offset);
        WCHAR *pszCharset = FindCharset(mibeNum);

        if (pNode && pNode->hdr.Type == SDP_TYPE_STRING && pszCharset != NULL) {
            IMultiLanguage2 * pMLang;

            hres = CoCreateInstance(CLSID_CMultiLanguage,
                                    NULL,
                                    CLSCTX_INPROC_SERVER,
                                    __uuidof(IMultiLanguage2),
                                    (PVOID *) &pMLang);

            if (SUCCEEDED(hres)) {
                MIMECSETINFO mime = { 0 };

                hres = pMLang->GetCharsetInfo(pszCharset, &mime);
                if (SUCCEEDED(hres)) {
                    DWORD dwMode = 0;
                    UINT inSize = pNode->DataSize, outSize = 0; 
                    BOOLEAN appendNull = FALSE;

                    //
                    // The string in the record does not have to be null
                    // terminated.  If it isn't, we must compensate for this 
                    // later on when we allocate memory and after the conversion
                    //
                    if (pNode->u.string[inSize-1] != NULL) {
                        appendNull = TRUE;
                    }

                    //
                    // Get the size required
                    //
                    hres = pMLang->ConvertStringToUnicode(&dwMode,
                                                          mime.uiInternetEncoding,
                                                          pNode->u.string,
                                                          &inSize,
                                                          NULL,
                                                          &outSize);

                    if (SUCCEEDED(hres)) {
                        if (appendNull) {
                            // allocate an extra character
                            outSize++;
                        }
                        
                        *ppString = (WCHAR*) CoTaskMemAlloc(outSize * sizeof(WCHAR));
                        if (*ppString) {
                            //
                            // finally, convert the string
                            //
                            hres = pMLang->
                                ConvertStringToUnicode(&dwMode,
                                                       mime.uiInternetEncoding,
                                                       pNode->u.string,
                                                       &inSize,
                                                       *ppString,
                                                       &outSize);

                            if (SUCCEEDED(hres)) {
                                if (appendNull) {
                                    //
                                    // Need to append a null, outSize contains
                                    // the number of chars written, and since
                                    // the size of the buffer is outSize+1, we
                                    // can safely use outSize as an index
                                    //
                                    (*ppString)[outSize] = 0;
                                }
                            }
                            else {
                                CoTaskMemFree(*ppString);
                                *ppString = NULL;
                            }
                        }
                        else {
                            hres = E_OUTOFMEMORY;
                        }
                    }
                }

                pMLang->Release();
            }
        }
        else {
            hres = E_FAIL;
        }
    }

	return hres;
#endif // UNDER_CE
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
struct ClassToID {
    const GUID *sc;
    WORD id; 
} g_ClassIconMapping[] = {
    { &SerialPortServiceClass_UUID,          IDI_SERIAL },
    { &GenericNetworkingServiceClass_UUID,   IDI_NET },
    { &FaxServiceClass_UUID,                 IDI_DUN },
    { &DialupNetworkingServiceClass_UUID,    IDI_DUN },
    { &GenericAudioServiceClass_UUID,        IDI_SPEAKER },
    { &HeadsetServiceClass_UUID,             IDI_SPEAKER },
    { &OBEXFileTransferServiceClass_UUID,    IDI_OBEX },
    { &OBEXObjectPushServiceClass_UUID,      IDI_OBEX },
};
#endif // UNDER_CE

STDMETHODIMP CSdpRecord::GetIcon(int cxRes, int cyRes, HICON *phIcon)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    return E_NOTIMPL;
#else

    GUID sc;

    


    HRESULT hres = GetServiceClass(&sc);
    if (SUCCEEDED(hres)) {
        hres = E_FAIL;
        for (ULONG i = 0; i < ARRAY_SIZE(g_ClassIconMapping); i++) {
            if (*g_ClassIconMapping[i].sc == sc) {
                *phIcon = (HICON) 
                    LoadImage(_Module.GetModuleInstance(),
                              MAKEINTRESOURCE(g_ClassIconMapping[i].id),
                              IMAGE_ICON,
                              cxRes,
                              cyRes,
                              0);

                if (*phIcon != NULL) {
                    hres = S_OK;
                }
                break;
            }
        }
    }

    if (!SUCCEEDED(hres)) {
        // load default icon by default
        *phIcon = (HICON) 
            LoadImage(_Module.GetModuleInstance(),
                      MAKEINTRESOURCE(IDI_DEFAULT_MAJOR_CLASS),
                      IMAGE_ICON,
                      cxRes,
                      cyRes,
                      0);
        hres = S_OK;
    }

    if (SUCCEEDED(hres) && *phIcon == NULL) {
        return E_FAIL;
    }

	return hres;
#endif // UNDER_CE
}

STDMETHODIMP CSdpRecord::GetServiceClass(LPGUID pServiceClass)
{
    HRESULT hres;

    if (pServiceClass == NULL) {
        return E_INVALIDARG;
    }

    NodeData nd;
    hres = GetAttribute(SDP_ATTRIB_CLASS_ID_LIST, &nd);
    if (SUCCEEDED(hres)) {
        if (nd.type == SDP_TYPE_CONTAINER) {
            NodeData ndUuid;

            hres = nd.u.container->GetNode(0, &ndUuid);
            if (SUCCEEDED(hres)) {
                return pNormalizeUuid(&ndUuid, pServiceClass);
            }
        }
        else {
            hres = E_FAIL;
        }
    }

    return hres;
}

STDMETHODIMP CSdpRecord::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::QueryGetData(FORMATETC *pformatetc)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::DUnadvise(DWORD dwConnection)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSdpRecord::EnumDAdvise(IEnumSTATDATA **ppenumAdvise)
{
    return E_NOTIMPL;
}


