//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// SdpNodeContainer.cpp : Implementation of CSdpStream
#include "stdafx.h"
#include "BthAPI.h"
#include "sdplib.h"
#include "SdpNodeList.h"
#include "SdpNodeContainer.h"
#include "util.h"

#define LOCK_LIST() ListLock l(&listLock)

/////////////////////////////////////////////////////////////////////////////
// CSdpNodeCotainer

CSdpNodeContainer::~CSdpNodeContainer()
{
    DeleteCriticalSection(&listLock);
}

void CSdpNodeContainer::Init()
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    m_pUnkMarshaler = NULL;
#endif    
    containerType = NodeContainerTypeSequence;
    locked = 0;
    streamSize = 0;
    streamSizeValid = FALSE;

    InitializeCriticalSection(&listLock);
}

STDMETHODIMP CSdpNodeContainer::CreateFromStream(UCHAR *pStream, ULONG size)
{
    NTSTATUS status;

    if (size == 0) {
        return E_INVALIDARG;
    }
    else if (pStream == NULL) {
        return E_POINTER;
    }

    status = ValidateStream(pStream, size, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return E_INVALIDARG;
    }

    UCHAR type, sizeIndex;
     
    SdpRetrieveHeader(pStream, type, sizeIndex);

    if (type == SDP_TYPE_ALTERNATIVE || type == SDP_TYPE_SEQUENCE) {
        /* do nothing */ ;
    }
    else {
        return E_INVALIDARG;
    }

    pStream++;

    ULONG elementSize, storageSize;
    SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);

    LOCK_LIST();

    if (type == SDP_TYPE_ALTERNATIVE) {
        containerType = NodeContainerTypeAlternative;
    }
    else {
        containerType = NodeContainerTypeSequence;
    }

    HRESULT err;

    if (elementSize > 0) {
        err = nodeList.CreateFromStream(pStream + storageSize, elementSize);
    }
    else {
        err = S_OK;
    }

    return err;
}

STDMETHODIMP CSdpNodeContainer::GetNodeCount(ULONG *pNodeCount)
{
    if (pNodeCount == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();
    *pNodeCount = nodeList.nodeCount;
    return S_OK;
}

STDMETHODIMP CSdpNodeContainer::LockContainer(UCHAR lock)
{
    LOCK_LIST();

    if (lock) {
        locked++;
    }
    else {
        locked--;
        if (locked == 0) {
            streamSizeValid = FALSE;
        }
    }

    nodeList.Lock(lock);

    return S_OK;
}

STDMETHODIMP CSdpNodeContainer::GetNode(ULONG nodeIndex, NodeData *pData)
{
    if (pData == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    if (nodeIndex >= nodeList.nodeCount) {
        return E_INVALIDARG;
    }
    else {
        CreateNodeDataFromSdpNode(nodeList.GetNode(nodeIndex), pData);
        return S_OK;
    }
}

STDMETHODIMP CSdpNodeContainer::GetNodeStringData(ULONG nodeIndex, NodeData *pData)
{
    HRESULT err;

    if (pData == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    if (nodeIndex >= nodeList.nodeCount) {
        return E_INVALIDARG;
    }
    else {
        PSDP_NODE pNode = nodeList.GetNode(nodeIndex);
        if (pNode->hdr.Type == SDP_TYPE_STRING) {
            err = CopyStringDataToNodeData(pNode->u.string, pNode->DataSize, &pData->u.str);
        }
        else if (pNode->hdr.Type == SDP_TYPE_URL) {
            err = CopyStringDataToNodeData(pNode->u.url, pNode->DataSize, &pData->u.url);
        }
        else {
            return E_INVALIDARG;
        }

        if (SUCCEEDED(err)) {
            pData->type = pNode->hdr.Type;
            pData->specificType = SDP_ST_NONE;
        }

        return err;
    }
}

STDMETHODIMP CSdpNodeContainer::SetNode(ULONG nodeIndex, NodeData *pData)
{
    HRESULT err = S_OK;

    if (pData == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

    if (nodeIndex >= nodeList.nodeCount) {
        return E_INVALIDARG;
    }
    else {
        PSDP_NODE pOldNode = NULL, pNewNode;

        if (pData == NULL) {
            pOldNode = nodeList.GetNode(nodeIndex);
            Sdp_RemoveEntryList(&pOldNode->hdr.Link);

            nodeList.nodeCount--;
        }
        else {
            err = CreateSdpNodeFromNodeData(pData, &pNewNode);

            if (SUCCEEDED(err)) {

                pOldNode = nodeList.GetNode(nodeIndex);
                Sdp_InsertEntryList(&pOldNode->hdr.Link, &pNewNode->hdr.Link);
                Sdp_RemoveEntryList(&pOldNode->hdr.Link);
            }
        }

        if (pOldNode) {
            SdpFreeOrphanedNode(pOldNode);
        }
    }

    return err;
}

#define S_NO_RECURSE ERROR_NOT_A_REPARSE_POINT 

STDMETHODIMP CSdpNodeContainer::Walk(ISdpWalk *pWalk)
{
    HRESULT err = S_OK;
    NodeData ndc;
    UCHAR endContainer = TRUE;

    if (pWalk == NULL) {
        return E_POINTER;
    }

    LockContainer(TRUE);

    ndc.type = SDP_TYPE_CONTAINER;
    ndc.specificType = SDP_ST_CONTAINER_INTERFACE;
    ndc.u.container = this;

    err = pWalk->WalkNode(&ndc, 1);
    if (err == S_NO_RECURSE) {
        LockContainer(FALSE);
        return err;
    }

    if (SUCCEEDED(err)) {
        err = nodeList.Walk(pWalk);
    }

    if (SUCCEEDED(err)) {
        pWalk->WalkNode(&ndc, 0);
    }

    LockContainer(FALSE);

    return err;
}

STDMETHODIMP CSdpNodeContainer::SetType(NodeContainerType type)
{
    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

    if (type == NodeContainerTypeSequence || 
        type == NodeContainerTypeAlternative) {
        containerType = type;
        return S_OK;
    }
    else {
        return E_INVALIDARG;
    }
}

STDMETHODIMP CSdpNodeContainer::GetType(NodeContainerType *pType)
{
    if (pType == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();
    *pType = containerType;

    return S_OK;
}

STDMETHODIMP CSdpNodeContainer::AppendNode(NodeData *pData)
{
    if (pData == NULL) {
        return E_POINTER;
    }

    LOCK_LIST();

    if (locked) {
        return E_ACCESSDENIED;
    }

    PSDP_NODE pNode;
    HRESULT err;

    err = CreateSdpNodeFromNodeData(pData, &pNode);
    if (!SUCCEEDED(err)) {
        return err;
    }

    Sdp_InsertTailList(&nodeList.Link, &pNode->hdr.Link);
    nodeList.nodeCount++;

    return S_OK;
}

STDMETHODIMP CSdpNodeContainer::CreateStream(UCHAR **ppStream, ULONG *pSize)
{
    ULONG size;
    HRESULT err = S_OK;

    if (ppStream == NULL || pSize == NULL) {
        return E_POINTER;
    }

    LockContainer(TRUE);    

    if (streamSizeValid == FALSE) {
        // ULONG d;
        GetStreamSize(&size);
    }
    else {
        size = streamSize + GetContainerHeaderSize(streamSize);
    }

    *ppStream = (PUCHAR) CoTaskMemAlloc(size);
    if (*ppStream) {
        ULONG numBytes;

        ZeroMemory(*ppStream, size);

        *pSize = size;
        WriteStream(*ppStream, &numBytes);
    }
    else {
        err = E_OUTOFMEMORY;
    }

    LockContainer(FALSE);    
    return err;
}

STDMETHODIMP CSdpNodeContainer::WriteStream(UCHAR *pStream, ULONG *pNumBytesWritten)
{
    ULONG size, numBytes = 0;
    UCHAR *pTmp;

    if (pStream == NULL || pNumBytesWritten == NULL) {
        return E_POINTER;
    }

    LockContainer(TRUE);

    if (streamSizeValid == FALSE) {
        GetStreamSize(&size);
    }
    
    UCHAR type = SDP_TYPE_SEQUENCE;
    if (containerType == NodeContainerTypeAlternative) {
        type = SDP_TYPE_ALTERNATIVE;
    }

    pTmp = WriteVariableSizeToStream(type, streamSize, pStream);
    numBytes = (ULONG) (pTmp - pStream);
    pStream = pTmp;

    *pNumBytesWritten = numBytes + nodeList.WriteStream(pStream);

    LockContainer(FALSE);

    return S_OK;
}

STDMETHODIMP CSdpNodeContainer::GetStreamSize(ULONG *pSize)
{
    ULONG size = 0;

    if (pSize == NULL) {
        return E_POINTER;
    }

    LockContainer(TRUE);

    if (nodeList.nodeCount == 0) {
        streamSize = 0;
    }
    else {
        //
        // We are interested in the size of the contents of the container itself
        // and not the amount of storage required for the header and variable
        // size
        //
        streamSize = nodeList.GetStreamSize();
    }

    //
    // Add on the storage for the variable size and header
    //
    *pSize = streamSize + GetContainerHeaderSize(streamSize);
    streamSizeValid = TRUE;

    LockContainer(FALSE);

    return S_OK;
}
