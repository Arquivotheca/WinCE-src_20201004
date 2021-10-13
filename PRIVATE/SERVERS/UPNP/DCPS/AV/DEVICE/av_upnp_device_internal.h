//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
// Declarations for internal implementation of the UPnP AV toolkit's device functionality
//

#ifndef __AV_UPNP_DEVICE_INTERNAL_H
#define __AV_UPNP_DEVICE_INTERNAL_H

#include "av_upnp.h"

namespace av_upnp
{
namespace details
{


//
// Virtual service function calling macro
// (use MAKE_INSTANCE_CALL_x, where x is the number of arguments passed)
//

// Lookups up service instance for MAKE_INSTANCE_CALL_x
#define MAKE_INSTANCE_CALL_PRE {                                                \
    ce::gate<ce::critical_section> csMapInstances(m_csMapInstances);            \
                                                                                \
    const InstanceMap::iterator it = m_mapInstances.find(InstanceID);           \
    if(it == m_mapInstances.end())                                              \
        return m_ErrReport.ReportError(ERROR_AV_INVALID_INSTANCE);


// Error checking for MAKE_INSTANCE_CALL_x
#define MAKE_INSTANCE_CALL_POST                                                 \
    if(SUCCESS_AV != retCall)                                                   \
        return m_ErrReport.ReportError(retCall); }


// Lookup service instance and make call for a given number of arguments for given function:
// (Assumes: a ce::critical_section m_csMapInstances for InstanceMap m_mapInstances, the map
// from instance id to instance ptr, long InstanceID, and UPnPErrorReporting m_ErrReport exist.)

// 0 args
#define MAKE_INSTANCE_CALL_0(FN_NAME)                                           \
    MAKE_INSTANCE_CALL_PRE                                                      \
    const DWORD retCall = it->second.pService->FN_NAME();                       \
    MAKE_INSTANCE_CALL_POST

// 1 arg
#define  MAKE_INSTANCE_CALL_1(FN_NAME, ARG1)                                    \
    MAKE_INSTANCE_CALL_PRE                                                      \
    const DWORD retCall = it->second.pService->FN_NAME(ARG1);                   \
    MAKE_INSTANCE_CALL_POST

// 2 args
#define  MAKE_INSTANCE_CALL_2(FN_NAME, ARG1, ARG2)                              \
    MAKE_INSTANCE_CALL_PRE                                                      \
    const DWORD retCall = it->second.pService->FN_NAME(ARG1, ARG2);             \
    MAKE_INSTANCE_CALL_POST

// 3 args
#define  MAKE_INSTANCE_CALL_3(FN_NAME, ARG1, ARG2, ARG3)                        \
    MAKE_INSTANCE_CALL_PRE                                                      \
    const DWORD retCall = it->second.pService->FN_NAME(ARG1, ARG2, ARG3);       \
    MAKE_INSTANCE_CALL_POST


} // av_upnp::details
} // av_upnp

#endif // __AV_UPNP_DEVICE_INTERNAL_H
