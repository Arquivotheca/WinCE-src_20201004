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
#define GETDEVICEUNIQUEID_DEVICE_NAME       _T("UID0:")
#define IOCTL_GETDEVICEUNIQUEID_MARSHALL    6678
#define FUNC_GETDEVICEUNIQUEID_INVOKE       1


DWORD
stub_GetDeviceUniqueID(ce::psl_buffer_wrapper<BYTE *> arrApplicationData,
                       DWORD dwDeviceIDVersion,
                       ce::psl_buffer_wrapper<BYTE *> arrDeviceIDOutput,
                       DWORD *pcbDeviceIDOutput,
                       HRESULT *phrResult);


HRESULT
impl_GetDeviceUniqueID(LPBYTE pbApplicationData, 
                       DWORD cbApplicationData, 
                       DWORD dwDeviceIDVersion,
                       LPBYTE pbDeviceIDOutput, 
                       DWORD *pcbDeviceIDOutput);


#define ZONE_ERROR DEBUGZONE(14)
#define ZONE_DEBUG DEBUGZONE(15)

