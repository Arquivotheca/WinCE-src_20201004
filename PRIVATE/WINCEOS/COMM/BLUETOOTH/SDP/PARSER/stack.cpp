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
#include "pch.h"

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#pragma code_seg("PAGE", "CODE")
#endif

SdpStack::SdpStack()
{                                                                   
    maxDepth = 0;
    RtlZeroMemory(orig, sizeof(orig));

    originalStack = orig;

    stackSize = sizeof(orig) / sizeof(orig[0]);                                   
    current = originalStack = stack = orig; 
}                               

SdpStack::~SdpStack()
{                                                       
    if (stack != originalStack) {   
        SdpFreePool(stack);                   
    }                                                   
}

NTSTATUS SdpStack::Push(PSDP_NODE Node, ULONG Size, PLIST_ENTRY Head)
{                                                                              
    PSD_STACK_ENTRY newStack;                                            

    if (current == (stack + stackSize)) {        
        newStack = (PSD_STACK_ENTRY)                                          
            SdpAllocatePool(stackSize * 2 * sizeof(SD_STACK_ENTRY));

        if (newStack == NULL) {                                                
            return STATUS_INSUFFICIENT_RESOURCES; 
        }                                                                      
        else {                                                                 
            RtlCopyMemory(newStack,                                            
                          stack,                                     
                          stackSize * sizeof(SD_STACK_ENTRY));      

            current = newStack + stackSize;                

            if (stack != originalStack) {                  
                SdpFreePool(stack);                                  
            }                                                                  

            stack = newStack;                                        
            stackSize *= 2;                                          
        }                                                                      
    }                                                                          
    else {                                                                     
        current->Size = Size;
        current->Node = Node; 
        current->Head = Head;
        current++;                                                   

        if ((current - stack) > maxDepth) {      
            maxDepth++;                                              
        }                                                                      
    }                                                                          

    return STATUS_SUCCESS;
}


