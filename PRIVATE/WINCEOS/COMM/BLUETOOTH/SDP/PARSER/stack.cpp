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


