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
#ifndef __NDT_NDIS_H
#define __NDT_NDIS_H

//------------------------------------------------------------------------------

#define NDT_FILTER_DIRECTED                  0x00000001
#define NDT_FILTER_MULTICAST                 0x00000002
#define NDT_FILTER_ALL_MULTICAST             0x00000004
#define NDT_FILTER_BROADCAST                 0x00000008
#define NDT_FILTER_SOURCE_ROUTING            0x00000010
#define NDT_FILTER_PROMISCUOUS               0x00000020
#define NDT_FILTER_SMT                       0x00000040
#define NDT_FILTER_ALL_LOCAL                 0x00000080
#define NDT_FILTER_GROUP_PKT                 0x00001000
#define NDT_FILTER_ALL_FUNCTIONAL            0x00002000
#define NDT_FILTER_FUNCTIONAL                0x00004000
#define NDT_FILTER_MAC_FRAME                 0x00008000

//------------------------------------------------------------------------------

#define NDT_PHYSICAL_MEDIUM_RESERVED0        0  
#define NDT_PHYSICAL_MEDIUM_WIRELESSLAN      1
#define NDT_PHYSICAL_MEDIUM_CABLEMODEM       2 
#define NDT_PHYSICAL_MEDIUM_PHONELINE        3  
#define NDT_PHYSICAL_MEDIUM_POWERLINE        4  
#define NDT_PHYSICAL_MEDIUM_DSL              5
#define NDT_PHYSICAL_MEDIUM_FIBRECHANNEL     6
#define NDT_PHYSICAL_MEDIUM_MEDIUM1394       7       
#define NDT_PHYSICAL_MEDIUM_MEDIUMMAX        8

//------------------------------------------------------------------------------

#endif
