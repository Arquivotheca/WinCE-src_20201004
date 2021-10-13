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
/*


*/
#define TX_LIST_ALLOC_INCR  16  // allocate list nodes in increments of 16

#define NOT_AN_INDEX        0xffffffff

typedef struct {
    UINT32    iHeadOfUsedList;
    UINT32    iHeadOfFreeList;
    UINT32    cbUsedNodes;
    UINT32    cbTotalNodes;
    UINT32    uNodeDataSize;
    UINT32    uNodeSize;
    LPVOID    lpFirstNode;
} PACKLIST, *LPPACKLIST;

LPVOID      HeadNode(LPPACKLIST lpWhichList);
LPVOID      NextNode(LPPACKLIST lpWhichList, LPVOID lpWhichNode);
LPVOID      NodePointer(LPPACKLIST lpWhichList, UINT32 iWhichNode);
LPVOID      AddNode(LPPACKLIST lpWhichList);
UINT32      NodeIndex(LPPACKLIST lpWhichList, LPVOID lpWhichNode);
void        RemoveNode(LPPACKLIST lpWhichList, LPVOID lpWhichNode, LPVOID lpPreviousNode);
LPPACKLIST  CreateList(UINT32 uCreateNodeDataSize);
void        MoveNode(LPPACKLIST lpWhichList, LPVOID lpWhichNode, LPVOID lpPreviousNode, LPVOID lpWhereToMove);
LPPACKLIST  AllocateNodes(LPPACKLIST lpWhichList, UINT32 cbNodesRequested);

