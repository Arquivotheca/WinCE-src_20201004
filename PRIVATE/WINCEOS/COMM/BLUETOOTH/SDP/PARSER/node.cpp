//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#pragma code_seg("PAGE", "CODE")

//
// Because of the differences between how  user and kernel declare
// InterlockedDecrement, just put it in the mode specific source
//
extern "C" inline LONG SdpInterlockedDecrement(PLONG Addend);
#else
#define SdpInterlockedDecrement(Addend)  InterlockedDecrement(Addend)
#endif


#define AllocSdpNode(_node)                                             \
        AllocSdpNodeX(_node, 0)

#define AllocSdpNodeX(_node, _extrasize)                              \
{                                                                       \
    (_node) = (PSDP_NODE)                                             \
        SdpAllocatePool(ROUND_SIZE(sizeof(SDP_NODE)) + (_extrasize));  \
    if ((_node) == NULL) {                                             \
        return NULL;                                                    \
    }                                                                   \
}

#define InitSdpNode(_node, _type, _spectype)                             \
{                                                                       \
    (_node)->hdr.Type = (_type);                                            \
    (_node)->hdr.SpecificType = (_spectype);                                \
    InitializeListHead(&(_node)->hdr.Link);                                 \
}

#define CreateSdpNode() PSDP_NODE node; AllocSdpNode(node)
    
void SdpInitializeNodeHeader(PSDP_NODE_HEADER Header)
{
    Header->Type = SDP_TYPE_LISTHEAD;
	Header->SpecificType = SDP_ST_NONE;
    InitializeListHead(&Header->Link);
}

PSDP_NODE SdpCreateNodeTree()
{
    CreateSdpNode();
	return SdpInitializeNodeTree(node);
}

PSDP_NODE SdpCreateNode()
{
    CreateSdpNode();
    return node;
}

PSDP_NODE SdpCreateNodeNil(void)
{
    CreateSdpNode();
    return SdpInitializeNodeNil(node);
}

PSDP_NODE SdpCreateNodeUInt128(PSDP_ULARGE_INTEGER_16 puint128)
{
    CreateSdpNode();
    return SdpInitializeNodeUInt128(node, puint128);
}

PSDP_NODE SdpCreateNodeUInt64(ULONGLONG uint64)
{
    CreateSdpNode();
    return SdpInitializeNodeUInt64(node, uint64);
}

PSDP_NODE SdpCreateNodeUInt32(ULONG uint32)
{
    CreateSdpNode();
    return SdpInitializeNodeUInt32(node, uint32);
}

PSDP_NODE SdpCreateNodeUInt16(USHORT uint16)
{
    CreateSdpNode();
    return SdpInitializeNodeUInt16(node, uint16);
}

PSDP_NODE SdpCreateNodeUInt8(UCHAR uint8)
{
    CreateSdpNode();
    return SdpInitializeNodeUInt8(node, uint8);
}


PSDP_NODE SdpCreateNodeInt128(PSDP_LARGE_INTEGER_16 pint128)
{
    CreateSdpNode();
    return SdpInitializeNodeInt128(node, pint128);
}

PSDP_NODE SdpCreateNodeInt64(LONGLONG int64)
{
    CreateSdpNode();
    return SdpInitializeNodeInt64(node, int64);
}

PSDP_NODE SdpCreateNodeInt32(LONG int32)
{
    CreateSdpNode();
    return SdpInitializeNodeInt32(node, int32);
}

PSDP_NODE SdpCreateNodeInt16(SHORT int16)
{
    CreateSdpNode();
    return SdpInitializeNodeInt16(node, int16);
}

PSDP_NODE SdpCreateNodeInt8(CHAR int8)
{
    CreateSdpNode();
    return SdpInitializeNodeInt8(node, int8);
}

PSDP_NODE SdpCreateNodeUUID128(const GUID *uuid128)
{
    CreateSdpNode();
    return SdpInitializeNodeUUID128(node, uuid128);
}

PSDP_NODE SdpCreateNodeUUID32(ULONG uuid32)
{
    CreateSdpNode();
    return SdpInitializeNodeUUID32(node, uuid32);
}

PSDP_NODE SdpCreateNodeUUID16(USHORT uuid16)
{
    CreateSdpNode();
    return SdpInitializeNodeUUID16(node, uuid16);
}

PSDP_NODE SdpCreateNodeString(PCHAR string, ULONG stringLength)
{
    PSDP_NODE node;
    PCHAR buffer;

    AllocSdpNodeX(node, stringLength);
    buffer = ((PCHAR) node) + sizeof (SDP_NODE);

    return SdpInitializeNodeString(node, string, stringLength, buffer);
}

PSDP_NODE SdpCreateNodeBoolean(UCHAR boolean)
{
    CreateSdpNode();
    return SdpInitializeNodeBoolean(node, boolean);
}

PSDP_NODE SdpCreateNodeSequence(void)
{
    CreateSdpNode();
    return SdpInitializeNodeSequence(node);
}

PSDP_NODE SdpCreateNodeAlternative(void)
{
    CreateSdpNode();
    return SdpInitializeNodeAlternative(node);
}

PSDP_NODE SdpCreateNodeUrl(PCHAR url, ULONG urlLength)
{
    PSDP_NODE node;
    PCHAR buffer;

    AllocSdpNodeX(node, urlLength);
    buffer = ((PCHAR) node) + sizeof (SDP_NODE);

    return SdpInitializeNodeUrl(node, url, urlLength, buffer);
}

PSDP_NODE SdpInitializeNodeTree(PSDP_NODE Node)
{
    InitSdpNode(Node, SDP_TYPE_LISTHEAD, SDP_ST_NONE);
	return Node;
}

PSDP_NODE SdpInitializeNodeNil(PSDP_NODE Node)
{
    InitSdpNode(Node, SDP_TYPE_NIL, SDP_ST_NONE);
    return Node;
}

PSDP_NODE SdpInitializeNodeUInt128(PSDP_NODE Node, PSDP_ULARGE_INTEGER_16 puint128)
{
    InitSdpNode(Node, SDP_TYPE_UINT, SDP_ST_UINT128);
    Node->DataSize = sizeof(Node->u.uint128);
    memcpy(&Node->u.uint128, puint128, sizeof(Node->u.uint128));
    return Node;
}

PSDP_NODE SdpInitializeNodeUInt64(PSDP_NODE Node, ULONGLONG uint64)
{
    InitSdpNode(Node, SDP_TYPE_UINT, SDP_ST_UINT64);
    Node->DataSize = sizeof(uint64);
    Node->u.uint64 = uint64;
    return Node;
}

PSDP_NODE SdpInitializeNodeUInt32(PSDP_NODE Node, ULONG uint32)
{
    InitSdpNode(Node, SDP_TYPE_UINT, SDP_ST_UINT32);
    Node->DataSize = sizeof(uint32);
    Node->u.uint32 = uint32;
    return Node;
}

PSDP_NODE SdpInitializeNodeUInt16(PSDP_NODE Node, USHORT uint16)
{
    InitSdpNode(Node, SDP_TYPE_UINT, SDP_ST_UINT16);
    Node->DataSize = sizeof(uint16);
    Node->u.uint16 = uint16;
    return Node;
}

PSDP_NODE SdpInitializeNodeUInt8(PSDP_NODE Node, UCHAR uint8)
{
    InitSdpNode(Node, SDP_TYPE_UINT, SDP_ST_UINT8);
    Node->DataSize = sizeof(uint8);
    Node->u.uint8 = uint8;
    return Node;
}


PSDP_NODE SdpInitializeNodeInt128(PSDP_NODE Node, PSDP_LARGE_INTEGER_16 pint128)
{
    InitSdpNode(Node, SDP_TYPE_INT, SDP_ST_INT128);
    Node->DataSize = sizeof(Node->u.int128);
    memcpy(&Node->u.int128, pint128, sizeof(Node->u.int128));
    return Node;
}

PSDP_NODE SdpInitializeNodeInt64(PSDP_NODE Node, LONGLONG int64)
{
    InitSdpNode(Node, SDP_TYPE_INT, SDP_ST_INT64);
    Node->DataSize = sizeof(int64);
    Node->u.int64 = int64;
    return Node;
}

PSDP_NODE SdpInitializeNodeInt32(PSDP_NODE Node, LONG int32)
{
    InitSdpNode(Node, SDP_TYPE_INT, SDP_ST_INT32);
    Node->DataSize = sizeof(int32);
    Node->u.int32 = int32;
    return Node;
}


PSDP_NODE SdpInitializeNodeInt16(PSDP_NODE Node, SHORT int16)
{
    InitSdpNode(Node, SDP_TYPE_INT, SDP_ST_INT16);
    Node->DataSize = sizeof(int16);
    Node->u.int16 = int16;
    return Node;
}

PSDP_NODE SdpInitializeNodeInt8(PSDP_NODE Node, CHAR int8)
{
    InitSdpNode(Node, SDP_TYPE_INT, SDP_ST_INT8);
    Node->DataSize = sizeof(int8);
    Node->u.int8 = int8;
    return Node;
}

PSDP_NODE SdpInitializeNodeUUID128(PSDP_NODE Node, const GUID *uuid128)
{
    InitSdpNode(Node, SDP_TYPE_UUID, SDP_ST_UUID128);
    Node->DataSize = sizeof(*uuid128);
    memcpy(&Node->u.uuid128, uuid128, sizeof(*uuid128));
    return Node;
}

PSDP_NODE SdpInitializeNodeUUID32(PSDP_NODE Node, ULONG uuid32)
{
    InitSdpNode(Node, SDP_TYPE_UUID, SDP_ST_UUID32);
    Node->DataSize = sizeof(uuid32);
    Node->u.uuid32 = uuid32;
    return Node;
}

PSDP_NODE SdpInitializeNodeUUID16(PSDP_NODE Node, USHORT uuid16)
{
    InitSdpNode(Node, SDP_TYPE_UUID, SDP_ST_UUID16);
    Node->DataSize = sizeof(uuid16);
    Node->u.uuid16 = uuid16;
    return Node;
}

PSDP_NODE SdpInitializeNodeString(PSDP_NODE Node, PCHAR string, ULONG stringLength, PCHAR nodeBuffer)
{
    InitSdpNode(Node, SDP_TYPE_STRING, SDP_ST_NONE);

    memcpy(nodeBuffer, string, stringLength);
    Node->DataSize = stringLength;
    Node->u.string = nodeBuffer;

    return Node;
}

PSDP_NODE SdpInitializeNodeBoolean(PSDP_NODE Node, UCHAR  boolean)
{
    InitSdpNode(Node, SDP_TYPE_BOOLEAN, SDP_ST_NONE);
    Node->DataSize = sizeof(boolean);
    Node->u.boolean = boolean;
    return Node;
}

PSDP_NODE SdpInitializeNodeSequence(PSDP_NODE Node)
{
    InitSdpNode(Node, SDP_TYPE_SEQUENCE, SDP_ST_NONE);
    SdpInitializeNodeHeader(&Node->u.sequence);
    return Node;
}

PSDP_NODE SdpInitializeNodeAlternative(PSDP_NODE Node)
{
    InitSdpNode(Node, SDP_TYPE_ALTERNATIVE, SDP_ST_NONE);
    SdpInitializeNodeHeader(&Node->u.alternative);
    return Node;
}

PSDP_NODE SdpInitializeNodeContainer(PSDP_NODE Node, PUCHAR Stream, ULONG Length)
{
    InitSdpNode(Node, SDP_TYPE_CONTAINER, SDP_ST_CONTAINER_STREAM);
    Node->u.stream = Stream;
    Node->u.streamLength = Length;
    return Node;
}

PSDP_NODE SdpInitializeNodeUrl(PSDP_NODE Node, PCHAR url, ULONG urlLength, PCHAR nodeBuffer)
{
    InitSdpNode(Node, SDP_TYPE_URL, SDP_ST_NONE);

    memcpy(nodeBuffer, url, urlLength);
    Node->DataSize = urlLength;
    Node->u.url = nodeBuffer;

    return Node;
}

NTSTATUS SdpAppendNodeToContainerNode(PSDP_NODE Parent, PSDP_NODE Node)
{
    switch (Parent->hdr.Type) {
    case SDP_TYPE_SEQUENCE:
        InsertTailList(&Parent->u.sequence.Link, &Node->hdr.Link);
        break;

    case SDP_TYPE_ALTERNATIVE:
        InsertTailList(&Parent->u.alternative.Link, &Node->hdr.Link);
        break;

    case SDP_TYPE_LISTHEAD:
        InsertTailList(&Parent->hdr.Link, &Node->hdr.Link);
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

ULONG GetListLength(PLIST_ENTRY Head)
{
    PLIST_ENTRY current;
    ULONG size = 0;

    current = Head->Flink;

    while (1) {
        size++;
        current = current->Flink;
        if (current == Head) {
            return size;
        }
    }
}

NTSTATUS SdpFreeTree(PSDP_NODE Tree)
{
    if (Tree->hdr.Type != SDP_TYPE_LISTHEAD) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsListEmpty(&Tree->hdr.Link)) {
        SdpFreeNode(CONTAINING_RECORD(Tree->hdr.Link.Flink, SDP_NODE, hdr.Link));
    }

    if (Tree->Reserved != NULL) {
        DecrementNodeRef((PSDP_NODE_REF) Tree->Reserved);
    }
    else {
        //
        // We have freed everything that is attached to this, free it
        //
        SdpFreePool(Tree);
    }

    return STATUS_SUCCESS;
}

NTSTATUS SdpFreeOrphanedNode(PSDP_NODE Node)
{
    SDP_NODE tmpHead;

    SdpInitializeNodeTree(&tmpHead);
    SdpAppendNodeToContainerNode(&tmpHead, Node);

    return SdpFreeNode(Node);
}

NTSTATUS SdpFreeNode(PSDP_NODE Node)
{
    SdpStack stack;
    SD_STACK_ENTRY *stackEntry;

    PLIST_ENTRY current;
    PSDP_NODE node;

    current = &Node->hdr.Link;

    while (1) {
        node = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);

        if ((node->hdr.Type == SDP_TYPE_SEQUENCE ||
             node->hdr.Type == SDP_TYPE_ALTERNATIVE) 
                                                &&
            (!IsListEmpty(&node->u.sequence.Link) ||
             !IsListEmpty(&node->u.alternative.Link))
             ) {

            if (!NT_SUCCESS(stack.Push(node))) {
                //
                // recover as gracefully as possible
                //
                return STATUS_INSUFFICIENT_RESOURCES;
            }
                    
            if (node->hdr.Type == SDP_TYPE_ALTERNATIVE) {
                current = node->u.alternative.Link.Flink;
            }
            else {
                current = node->u.sequence.Link.Flink;
            }

            continue;
        }

        //
        // Advance to the next link in the list
        //
        current = current->Flink;

        //
        // Remove the node from the list.  
        //
        RemoveEntryList(&node->hdr.Link);

        // When a container is created by a user APP, it doesn't matter what
        // its SpecificType is, most likely it will by SDP_ST_NONE.  In the
        // SdpInitializeNodeContainer we set Type=SDP_TYPE_CONTAINER and
        // SpecificType = SDP_ST_CONTAINER_STREAM and have this be a pointer
        // to a stream, rather than an ISdpNodeContainer.  In this case we
        // can't release ISdpNodeContainer object.
        
        if ((node->hdr.Type == SDP_TYPE_CONTAINER) && (node->hdr.SpecificType != SDP_ST_CONTAINER_STREAM)) {
            // hitting this ASSERT shouldn't be harmful, but it means someone is 
            // setting up their node in an unexpected way (and possibly wrong way), 
            // needs investigation. 
            SVSUTIL_ASSERT((node->hdr.SpecificType == SDP_ST_CONTAINER_INTERFACE) || (node->hdr.SpecificType == SDP_ST_NONE));
            SdpReleaseContainer(node->u.container);
        }

        //
        // Finally, free the node
        //
        if (node->Reserved == NULL) {
            SdpFreePool(node);
        } else {
			DecrementNodeRef((PSDP_NODE_REF) node->Reserved);
        }

        //
        // Check to see if we have reached the end of the linked list
        //
        while (IsListEmpty(current)) {
            //
            // Check to see if we have pushed any items.  If not, then we are 
            // done.
            //
            if (stack.Depth() == 0) {
                return STATUS_SUCCESS;
            }

            //
            // Get the stored values.  Since we are now removing an node,
            // decrement size.
            //
            stackEntry = stack.Pop();

            node = stackEntry->Node;

            //
            // Get the next link in the chain before removing the node from the
            // list
            //
            current = node->hdr.Link.Flink;
            RemoveEntryList(&node->hdr.Link);
        
            //
            // Finally, free the node
            //
            if (node->Reserved == NULL) {
                SdpFreePool(node);
            } else {
				DecrementNodeRef((PSDP_NODE_REF) node->Reserved);
            }
        }
    }
}

NTSTATUS SdpAddAttributeToNodeHeader(PSDP_NODE_HEADER Header, USHORT AttribId, PSDP_NODE AttribValue)
{
    PSDP_NODE nodeId, nodeValue, attrib;
    PLIST_ENTRY current;

    nodeId = CONTAINING_RECORD(Header->Link.Flink, SDP_NODE, hdr.Link);
    nodeValue = CONTAINING_RECORD(nodeId->hdr.Link.Flink, SDP_NODE, hdr.Link);

    current = &nodeValue->hdr.Link;

    //
    // iterate over the attribute id / value pairs  in the list and see where 
    // this entry goes.
    //
    // Attribute IDs are added in increasing numerical order!
    //
    while (1) {
        //
        // Do the following
        // 1 make sure we have a USHORT node AND
        // 2 if we are replacing a value OR
        // 3 if we are inserting a value OR
        // 4 going to the next pair in the list AND
        // 5 checking if we are at the end of the list, and if so, appending
        //   the pair
        //


        // Initial insertion
        if (IsListEmpty(&Header->Link)) {
            if (AttribValue == NULL) {
                return STATUS_NOT_FOUND;
            }

            attrib = SdpCreateNodeUInt16(AttribId);
            if (attrib == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            InsertTailList(&Header->Link, &attrib->hdr.Link);
            InsertTailList(&Header->Link, &AttribValue->hdr.Link);

            return STATUS_SUCCESS;
        }
        //
        // Check to see if we have a valid attrib id node
        //         
        else if (nodeId->hdr.Type != SDP_TYPE_UINT ||
            nodeId->hdr.SpecificType != SDP_ST_UINT16 ||
            nodeId->DataSize != sizeof(AttribId)) {
            return STATUS_INVALID_PARAMETER;
        }
        else if (AttribId == nodeId->u.uint16) {
            //
            // We are replacing or removing a value
            //

            //
            // Remove the value from the tree in both cases
            //
            RemoveEntryList(&nodeValue->hdr.Link);
            SdpFreeOrphanedNode(nodeValue);

            //
            // Check to see if we are removing
            //
            if (AttribValue == NULL) {
                //
                // Remove the ID as well
                //
                RemoveEntryList(&nodeId->hdr.Link);
                SdpFreeOrphanedNode(nodeId);
            }
            else {
                InsertEntryList(&nodeId->hdr.Link, &AttribValue->hdr.Link); 
            }

            return STATUS_SUCCESS;
        }
        else if (AttribId < nodeId->u.uint16) {

            //
            // We are inserting a value into the middle of the list
            //

            //
            // Must have a valid AttribValue in this case
            //
            if (AttribValue == NULL) {
                return STATUS_INVALID_PARAMETER;
            }

            attrib = SdpCreateNodeUInt16(AttribId);
            if (attrib == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // hdr.Link in the attribute ID before the current entry and
            // then link in the attribute value after the newly placed
            // ID
            //
            InsertEntryList(nodeId->hdr.Link.Blink, &attrib->hdr.Link);
            InsertEntryList(&attrib->hdr.Link, &AttribValue->hdr.Link);

            return STATUS_SUCCESS;
        }
        else {
            current = current->Flink;

            //
            // Check to see if we have wrapped to the end of the list.  If 
            // so, insert the ID / value pair at the end of the list
            //
            if (current == &Header->Link) {
                if (AttribValue == NULL) {
                    return STATUS_NOT_FOUND;
                }

                attrib = SdpCreateNodeUInt16(AttribId);
                if (attrib == NULL) {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                InsertTailList(&Header->Link, &attrib->hdr.Link);
                InsertTailList(&Header->Link, &AttribValue->hdr.Link);

                return STATUS_SUCCESS;
            }
            else {
                nodeId = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
                ASSERT(nodeId->hdr.Type != SDP_TYPE_LISTHEAD);

                current = current->Flink;
                nodeValue = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
            }
        }
    }
}

NTSTATUS SdpAddAttributeToTree(PSDP_NODE Root, USHORT AttribId, PSDP_NODE AttribValue)
{
    PSDP_NODE attrib, sequence;

    if (IsListEmpty(&Root->hdr.Link)) {
        if (AttribValue == NULL) {
            return STATUS_INVALID_PARAMETER;
        }

        sequence = SdpCreateNodeSequence();
        SdpAppendNodeToContainerNode((PSDP_NODE) Root, sequence);
    }
    else {
        sequence = CONTAINING_RECORD(Root->hdr.Link.Flink, SDP_NODE, hdr.Link);
    }

	if (sequence->hdr.Type != SDP_TYPE_SEQUENCE) {
		return STATUS_INVALID_PARAMETER;
	}

    if (IsListEmpty(&sequence->u.sequence.Link)) {
        if (AttribValue == NULL) {
            return STATUS_INVALID_PARAMETER;
        }

        attrib = SdpCreateNodeUInt16(AttribId);
        if (attrib == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        SdpAppendNodeToContainerNode(sequence, attrib);
        SdpAppendNodeToContainerNode(sequence, AttribValue);

        return STATUS_SUCCESS;
    }
    else {
//      PLIST_ENTRY current;
//      PSDP_NODE nodeId, nodeValue;

        if ((GetListLength(&sequence->u.sequence.Link) % 2) != 0) {
            //
            // Invalid list, must be a multiple of 2
            //
            return STATUS_INVALID_PARAMETER;
        }

        return SdpAddAttributeToNodeHeader(&sequence->u.sequence, AttribId, AttribValue);
    }
}

void DecrementNodeRef(PSDP_NODE_REF NodeRef)
{
	if (SdpInterlockedDecrement(&NodeRef->RefCount) == 0) {
		SdpFreePool(NodeRef->Alloc);
        SdpFreePool(NodeRef);
	}
}
