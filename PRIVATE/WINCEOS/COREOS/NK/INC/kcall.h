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
// This header file defines the function names that correspond with KCall ID's


#define KCALL_NextThread    10  // NextThread's entry in array

WCHAR* pKCallName[MAX_KCALL_PROFILE] = {
    L"HandleException",           //0
    L"WaitConfig",                //1
    L"PutThreadToSleep",          //2
    L"SemAdd",                    //3
    L"DoReprioCrit",              //4
    L"ThreadSuspend",             //5
    L"DisableThreadCalls",        //6
    L"NextCloseHandle",           //7
    L"SetupHandle",               //8
    L"ZapHandle",                 //9
    L"NextThread",                //10
    L"DoIncRef",                  //11
    L"CopyRefs",                  //12
    L"Not Used",           //13
    L"WakeIfDebugWait",           //14
    L"EventModMan",               //15
    L"EventModAuto",              //16
    L"SetThreadToDie",            //17
    L"WaitOneMore",               //18
    L"CSWaitPart1",               //19
    L"LeaveCrit",                 //20
    L"LeaveMutex",                //21
    L"ProfileThreadGetContext",   //22
    L"ProfileThreadSetContext",   //23
    L"GetADirtyPage",             //24
    L"StopProc2",                 //25
    L"GetKCallProfile",           //26
    L"StopProc",                  //27
    L"SetThreadBasePrio",         //28
    L"DoReprioMutex",             //29
    L"DoDecRef",                  //30
    L"DequeuePrioProxy",          //31
    L"LinkPhysPage",              //32
    L"GrabFirstPhysPage",         //33
    L"DoFreeMutex",               //34
    L"ThreadYield",               //35
    L"FinalRemoveThread",         //36
    L"SemPop",                    //37
    L"UnlinkPhysPage",            //38
    L"MakeRunIfNeeded",           //39
    L"TakeTwoPages",              //40
    L"WakeOneThreadFlat",         //41
    L"EventModIntr",              //42
    L"RemoveThread",              //43
    L"KCNextThread",              //44
    L"DoFreeCrit",                //45
    L"LoadSwitch",                //46
    L"ThreadResume",              //47
    L"GetCurThreadKTime",         //48
    L"CheckLastRef",              //49
    L"LaterLinkCritMut",          //50
    L"PreUnlinkCritMut",          //51
    L"PostUnlinkCritMut",         //52
    L"PreLeaveCrit",              //53
    L"DequeueFlatProxy",          //54
    L"LaterLinkMutOwner",         //55
    L"PostBoostMut",              //56
    L"PostBoostCrit1",            //57
    L"CSWaitPart2",               //58
    L"CritFinalBoost",            //59
    L"PostBoostCrit2",            //60
    L"MIPS/SHx:SwitchFPUOwner",   //61
    L"x86:SetCPUASID",            //62
    L"ARM:InvalidateSoftwareTLB", //63
    L"SuspendSelf",               //64
    L"CommonHandleException",     //65
    L"AdjustPrioDown",            //66
    L"DoLeaveSomething",          //67
    L"SHx:SwitchDSPOwner",        //68
    L"x86:InitializeFPU",         //69
    L"x86:InitializeEmx87",       //70
    L"x86:InitNPXHPHandler",      //71
    L"x86:InvalidatePageTables",  //72
    L"x86:SetCPUHardwareWatch",   //73
    L"CeLogReSync",               //74
    L"CeLogFlushBuffer",           //75
    L"WaitForOneManualNonInterruptEvent"               //76
    

    // NOTE: when updating this table, fix MAX_KCALL_PROFILE in kernel.h
    
//    L"pKDIoControl",              //  These are KCalls, but they are in the OAL...
//    L"OEMEthGetSecs",             //
//    L"OEMEthGetFrame",            //
//    L"OEMEthEnableInts",          //
//    L"OEMEthISR",                 //
//    L"OEMEthSendFrame",           //
};


