//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//


#ifndef SMB_IOCTL_H
#define SMB_IOCTL_H


#define IOCTL_SET_MAX_CONNS 4

#ifdef DEBUG
#define IOCTL_DEBUG_PRINT 5
#endif


#define SMB_IOCTL_INVOKE 1000
#define IOCTL_CHANGE_ACL                1
#define IOCTL_ADD_SHARE                 2
#define IOCTL_DEL_SHARE                 3
#define IOCTL_LIST_USERS_CONNECTED     4
#define IOCTL_QUERY_AMOUNT_TRANSFERED 5
#define IOCTL_NET_UPDATE_ALIASES            6
#define IOCTL_NET_UPDATE_IP_ADDRESS     7


#endif
