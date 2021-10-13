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
// This header file defines the function names that correspond with KCall ID's


#define KCALL_NextThread    10  // NextThread's entry in array

WCHAR* pKCallName[MAX_KCALL_PROFILE] = {
    L"HandleException",           //0
    L"SCHL_YieldToNewMutexOwner",                //1
    L"SCHL_PutThreadToSleep",          //2
    L"SCHL_SemAdd",                    //3
    L"SCHL_DoReprioCritMut",              //4
    L"SCHL_ThreadSuspend",             //5
    L"DisableThreadCalls",        //6
    L"NextCloseHandle",           //7
    L"SetupHandle",               //8
    L"ZapHandle",                 //9
    L"NextThread",                //10
    L"DoIncRef",                  //11
    L"CopyRefs",                  //12
    L"SCHL_SwitchOwnerToKernel",           //13
    L"WakeIfDebugWait",           //14
    L"SCHL_EventModMan",               //15
    L"SCHL_EventModAuto",              //16
    L"SCHL_SetThreadToDie",            //17
    L"SCHL_WaitOneMore",               //18
    L"SCHL_CSWaitPart1",               //19
    L"SCHL_ReleaseMutex",                 //20
    L"MemoryLock",                //21
    L"ProfileThreadGetContext",   //22
    L"ProfileThreadSetContext",   //23
    L"GetADirtyPage",             //24
    L"StopProc2",                 //25
    L"GetKCallProfile",           //26
    L"StopProc",                  //27
    L"SCHL_SetThreadBasePrio",         //28
    L"NKSendInterProcessorInterrupt",   //29
    L"DoDecRef",                  //30
    L"SCHL_DequeuePrioProxy",          //31
    L"LinkPhysPage",              //32
    L"GrabFirstPhysPage",         //33
    L"SCHL_DoFreeCritMut",               //34
    L"SCHL_ThreadYield",               //35
    L"SCHL_FinalRemoveThread",         //36
    L"SCHL_SemPop",                    //37
    L"UnlinkPhysPage",            //38
    L"SCHL_MakeRunIfNeeded",           //39
    L"TakeTwoPages",              //40
    L"SCHL_WakeOneThreadFlat",         //41
    L"SCHL_EventModIntr",              //42
    L"RemoveThread",              //43
    L"KCNextThread",              //44
    L"Not Used",                //45
    L"LoadSwitch",                //46
    L"SCHL_ThreadResume",              //47
    L"SCHL_GetCurThreadKTime",         //48
    L"CheckLastRef",              //49
    L"LaterLinkCritMut",          //50
    L"PreUnlinkCritMut",          //51
    L"SCHL_PostUnlinkCritMut",         //52
    L"SCHL_PreReleaseMutex",              //53
    L"SCHL_DequeueFlatProxy",          //54
    L"not used",         //55
    L"ODSInKCall",              //56
    L"OalSpinLock",            //57
    L"SCHL_CSWaitPart2",               //58
    L"SCHL_MutexFinalBoost",            //59
    L"SCHL_BoostMutexOwnerPriority",            //60
    L"SwitchFPUOwner",            //61
    L"x86:SetCPUASID",            //62
    L"ARM:InvalidateSoftwareTLB", //63
    L"SuspendSelf",               //64
    L"CommonHandleException",     //65
    L"SCHL_AdjustPrioDown",            //66
    L"DoLeaveSomething",          //67
    L"SHx:SwitchDSPOwner",        //68
    L"x86:InitializeFPU",         //69
    L"x86:InitializeEmx87",       //70
    L"Not Used",      //71
    L"x86:InvalidatePageTables",  //72
    L"x86:SetCPUHardwareWatch",   //73
    L"CeLogReSync",               //74
    L"CeLogFlushBuffer",           //75
    L"WaitForOneManualNonInterruptEvent",               //76


    // NOTE: when updating this table, fix MAX_KCALL_PROFILE in kernel.h
    
//    L"pKDIoControl",              //  These are KCalls, but they are in the OAL...
//    L"OEMEthGetSecs",             //
//    L"OEMEthGetFrame",            //
//    L"OEMEthEnableInts",          //
//    L"OEMEthISR",                 //
//    L"OEMEthSendFrame",           //
};


