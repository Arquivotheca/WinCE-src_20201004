//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "stdafx.h"
#include "BthApi.h"
#include "sdplib.h"
#include "SdpNodeList.h"
#include "util.h"

SdpNodeList::SdpNodeList()
{
    nodeCount = 0;
    SdpInitializeNodeHeader(this);
}

SdpNodeList::~SdpNodeList()
{
    Destroy();
}

HRESULT SdpNodeList::CreateFromStream(UCHAR *pStream, ULONG size)
{
    NTSTATUS status;
    HRESULT err = S_OK;

    //
    // clear out previous contents
    //
    Destroy();

    status = SdpTreeFromStream(pStream,
                               size,
                               this,
                               NULL,
                               FALSE);

    if (NT_SUCCESS(status)) {
//      PSDP_NODE pNode;

        nodeCount = 0;

        for (PLIST_ENTRY entry = Link.Flink;
             entry != &Link;
             ) {
            nodeCount++;

            PSDP_NODE pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);

            if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
                CComPtr<ISdpNodeContainer> container;

#ifdef UNDER_CE
                // WinCE uses ATL 2.1, no contaiener support.
                err = CoCreateInstance(CLSID_SdpNodeContainer,NULL,CLSCTX_ALL,
                                       __uuidof(ISdpNodeContainer),(void**)&container);
#else            
                err = container.CoCreateInstance(CLSID_SdpNodeContainer);
#endif                
                if (SUCCEEDED(err)) {
                    err =  container->CreateFromStream(pNode->u.stream, pNode->u.streamLength);
                    if (SUCCEEDED(err)) {
                        //
                        // Must AddRef because when CComPtr goes out of
                        // scope it will call Release()
                        //
                        pNode->u.container = container;
                        pNode->u.container->AddRef();
                        pNode->hdr.SpecificType = SDP_ST_CONTAINER_INTERFACE;
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }

            entry = entry->Flink;
        }
    }
    else {
        err = MapNtStatusToHresult(status);
    }

    if (!SUCCEEDED(err)) {
        Destroy();
    }

    return err;
}

void SdpNodeList::Lock(UCHAR lock)
{
    PLIST_ENTRY entry;

    for (entry = Link.Flink;
         entry != &Link;
         entry = entry->Flink) {
        PSDP_NODE pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
        if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
            pNode->u.container->LockContainer(lock);
        }
    }
}

PSDP_NODE SdpNodeList::GetNode(ULONG nodeIndex)
{
    PLIST_ENTRY entry = Link.Flink;

    while (nodeIndex > 0) {
        entry = entry->Flink;
        nodeIndex--;
    }

    return CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
}

void SdpNodeList::Destroy()
{
    PSDP_NODE pNode;

    if (!Sdp_IsListEmpty(&Link)) {
        //
        // SdpFreeNode will iteratively free all the nodes in the list and call
        // Release() on all the container interfaces
        //
        pNode = CONTAINING_RECORD(Link.Flink, SDP_NODE, hdr.Link);
        SdpFreeNode(pNode);
    }

    nodeCount = 0;
}

ULONG SdpNodeList::WriteStream(PUCHAR pStream)
{
    PUCHAR pTmp;
    ULONG numBytes = 0;

    for (PLIST_ENTRY entry = Link.Flink;
         entry != &Link;
         entry = entry->Flink) {

        PSDP_NODE pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);

        if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
            ULONG bytes = 0;
            pNode->u.container->WriteStream(pStream, &bytes);
            numBytes += bytes;
            pStream += bytes;
        }
        else {
            pTmp = WriteLeafToStream(pNode, pStream);
            numBytes += (ULONG) (pTmp - pStream);
            pStream = pTmp;
        }
    }

    return numBytes;
}

ULONG SdpNodeList::GetStreamSize()
{
    PLIST_ENTRY entry = Link.Flink;
    PSDP_NODE pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
    ULONG size = 0;

    //
    // This will compute the size of all the nodes this container holds that
    // are not containers themselvesj
    //
    ComputeNodeListSize(pNode, &size);

    for ( ; entry != &Link; entry = entry->Flink) {
        pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
        if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
            ULONG tmp = 0;
            pNode->u.container->GetStreamSize(&tmp);
            pNode->DataSize = tmp;
            size += tmp;
        }
    }

    //
    // We are interested in the size of the contents of the container itself
    // and not the amount of storage required for the header and variable
    // size
    //
    return size;
}

HRESULT SdpNodeList::Walk(ISdpWalk *pWalk)
{
    HRESULT err = S_OK;

    for (PLIST_ENTRY entry = Link.Flink;
         entry != &Link;
         entry = entry->Flink) {

        NodeData nd;
        PSDP_NODE pNode;
        PCHAR str = NULL;

        ZeroMemory(&nd, sizeof(nd));
        pNode = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
        CreateNodeDataFromSdpNode(pNode, &nd);
        if (pNode->hdr.Type == SDP_TYPE_CONTAINER) {
            err = pNode->u.container->Walk(pWalk);
            if (!SUCCEEDED(err)) {
                break;
            }
        }
        else {
            if (pNode->hdr.Type == SDP_TYPE_URL) {
                CopyStringDataToNodeData(pNode->u.url,
                                         pNode->DataSize,
                                         &nd.u.url);

                str = nd.u.url.val;

                nd.type = pNode->hdr.Type;
                nd.specificType = pNode->hdr.SpecificType;
            }
            else if (pNode->hdr.Type == SDP_TYPE_STRING) {
                CopyStringDataToNodeData(pNode->u.url,
                                         pNode->DataSize,
                                         &nd.u.str);

                str = nd.u.str.val;

                nd.type = pNode->hdr.Type;
                nd.specificType = pNode->hdr.SpecificType;
            }

            err = pWalk->WalkNode(&nd, 0);

            if (str) {
                CoTaskMemFree(str);
            }

            if (!SUCCEEDED(err)) {
                break;
            }
        }
    }

    return err;
}
