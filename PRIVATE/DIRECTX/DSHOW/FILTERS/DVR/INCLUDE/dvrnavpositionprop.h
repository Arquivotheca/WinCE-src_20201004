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
/*
**  DVRNavPositionProp.h:
**
**
**  Defines a property set and property used to confirm the correct
**  initial post-flush video rendering position, to assist in the
**  computation of the correct upstream a/v position.
*/

#ifndef _DVRNavPositionProp_h
#define _DVRNavPositionProp_h

// {9646C68D-2409-4b6a-BEF6-256D45DEDCC3}
DEFINE_GUID(DVRENG_PROPSETID_DVR_SupplementalInfo, 
0x9646c68d, 0x2409, 0x4b6a, 0xbe, 0xf6, 0x25, 0x6d, 0x45, 0xde, 0xdc, 0xc3);

typedef enum DVRENG_PROPSET_PROPERTY_ID
{
	DVRENG_PROPID_DVR_FirstSamplePosition = 0
} DVRENG_PROPSET_PROPERTY_ID;

#endif /* _DVRNavPositionProp_h */
