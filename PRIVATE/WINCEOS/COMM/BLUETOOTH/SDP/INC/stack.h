//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __STACK_H__
#define __STACK_H__

#define SD_STACK_SIZE 20

typedef struct _SD_STACK_ENTRY {
    ULONG Size;
    PSDP_NODE Node;
    PLIST_ENTRY Head;
} SD_STACK_ENTRY, *PSD_STACK_ENTRY;

struct SdpStack {
    SdpStack();
    ~SdpStack();

    NTSTATUS Push(PSDP_NODE Node = NULL, ULONG Size = 0, PLIST_ENTRY Head = NULL);
    inline PSD_STACK_ENTRY Pop() { return --current; }
    
    inline ULONG_PTR Depth()    { return current - stack; }
    inline LONG_PTR GetMaxDepth() { return maxDepth; }

protected:
    LONG_PTR maxDepth;
    PSD_STACK_ENTRY current;
    PSD_STACK_ENTRY stack;
    PSD_STACK_ENTRY originalStack;
    ULONG stackSize;

    SD_STACK_ENTRY orig[SD_STACK_SIZE];
};

#endif // __STACK_H__
