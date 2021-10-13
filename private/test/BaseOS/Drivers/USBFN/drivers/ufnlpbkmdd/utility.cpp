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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

// 
// Module Name:  
//     utility.cpp
// 
// Abstract:  This file implements some common utility funcitons 
//            
//     
// Notes: 
//

#include <usbfn.h>
#include "ufnloopback.h"

/*
    --Why do this? because this driver already has spawned a lot of threads for normal data transfer tests,
    especially if number of pipes are big. so for special tests cases, we want to create threads on the fly, only
    create thread when neccessary.
*/
BOOL
FindVacantThreadPos(PSPTREHADINFO pThreads, PUCHAR puThread){

    if(pThreads == NULL || puThread == NULL){//this should not happen
        return FALSE;
    }

    for(UCHAR i = 0; i < NUM_OF_SPTHEADS; i++){
        if(pThreads[i].hThread == NULL){
            *puThread = i;
            return TRUE;
        }
        else if(pThreads[i].fRunning == FALSE){
            CloseHandle(pThreads[i].hThread);
            pThreads[i].hThread = NULL;
            *puThread = i;
            return TRUE;
        }
    }

    return FALSE;
    
}


