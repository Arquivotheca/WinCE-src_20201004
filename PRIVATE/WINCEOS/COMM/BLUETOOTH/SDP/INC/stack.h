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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
