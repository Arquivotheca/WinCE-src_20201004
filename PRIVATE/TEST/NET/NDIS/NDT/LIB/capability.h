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
#ifndef __CAPABILITY_H
#define __CAPABILITY_H

#include "ndtlibwlan.h"

//------------------------------------------------------------------------------

class CWlanCapability
{
   public :
    NDIS_802_11_DEVICE_TYPE m_dwDeviceType;
    
    BOOL m_bWPASupported;
    BOOL m_bWPA2Supported;
    BOOL m_bTKIPSupported;
    BOOL m_bAESSupported;
    BOOL m_bWPAAdhocSupported;
    BOOL m_bKeyMappingKeysSupported;
    BOOL m_bWMESupported;
    BOOL m_bMediaStreamSupported;
         
    BOOL m_bWPATKIPSupported;
    BOOL m_bWPAAESSupported;
    BOOL m_bWPAPSKTKIPSupported;
    BOOL m_bWPAPSKAESSupported;
       
    BOOL m_bWPA2TKIPSupported;
    BOOL m_bWPA2AESSupported;
    BOOL m_bWPA2PSKTKIPSupported;
    BOOL m_bWPA2PSKAESSupported;
       
    BOOL m_bWPANoneTKIPSupported;
    BOOL m_bWPANoneAESSupported;
       
    ULONG m_NumPMKIDsSupported;

    HANDLE m_hAdapter;
    
    BOOL m_bSessionSet;

   CWlanCapability(HANDLE hAdapter);
   HRESULT GetDeviceCapabilities();
   HRESULT PrintCapablities();
   
};

#endif __CAPABILITY_H

