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

// Abstract: Location framework specific IOCTLs.  Used only for
//           user-mode API to communicate with location framework service
//           internally (not exposed directly to app developers).

#include <psl_marshaler.hxx>

// Prefix to location framework service itself
#define LOC_DEV_PREFIX L"LOC"
// Index associated with location framework service
#define LOC_DEV_INDEX  0
// Complete name of service
#define LOC_DEV_NAME   L"LOC0:"


// Codes associated with individual IOCTLs
#define IOCTL_LF_INVOKE                   3000

#define IOCTL_LF_OPEN                        1
#define IOCTL_LF_CLOSE                       2
#define IOCTL_LF_REGISTER_FOR_REPORT         3               
#define IOCTL_LF_UN_REGISTER_FOR_REPORT      4
#define IOCTL_LF_GET_REPORT                  5
#define IOCTL_LF_GET_SERVICE_STATE           6
#define IOCTL_LF_GET_PLUGIN_INFO_FOR_REPORT  7
#define IOCTL_LF_GET_PROVIDERS_INFO          8
#define IOCTL_LF_GET_RESOLVERS_INFO          9
#define IOCTL_LF_PLUGIN_OPEN                10
#define IOCTL_LF_PLUGIN_IOCTL               11
#define IOCTL_LF_PLUGIN_CLOSE               12

