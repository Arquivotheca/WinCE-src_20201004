//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
