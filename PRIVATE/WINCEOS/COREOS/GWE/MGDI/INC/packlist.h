//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

LPVOID      NodePointer (LPPACKLIST lpWhichList, UINT32 iWhichNode);
LPVOID      NextPointer (LPPACKLIST lpWhichList, UINT32 iWhichNode);
UINT32      NodeIndex (LPPACKLIST lpWhichList, LPVOID lpWhichNode);
LPVOID      HeadNode (LPPACKLIST lpWhichList);
LPVOID      NextNode (LPPACKLIST lpWhichList, LPVOID lpWhichNode);
void        AddNodeToFreeList (LPPACKLIST lpWhichList, UINT32 iWhichNode);
LPPACKLIST  AllocateNodes (LPPACKLIST lpWhichList, UINT32 cbNodesRequested);
LPVOID      AddNode (LPPACKLIST lpWhichList);
void        RemoveNode (LPPACKLIST lpWhichList, LPVOID lpWhichNode, LPVOID lpPreviousNode);
void        RemoveAllNodes (LPPACKLIST lpWhichList);
void        MoveNode (LPPACKLIST lpWhichList, LPVOID lpWhichNode, LPVOID lpPreviousNode, LPVOID lpWhereToMove);
LPPACKLIST  CreateList (UINT32 uCreateNodeDataSize);
void        DestroyList (LPPACKLIST lpWhichList);
