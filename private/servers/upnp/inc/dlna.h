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
#pragma once

#include <wininet.h>  // for HTTP_STATUS_OK and HTTP_STATUS_AMBIGUOUS

// DLNA Versions
#define DLNA_MAJOR_VER 1
#define DLNA_MINOR_VER 50

// UPNP Versions
#define UPNP_MAJOR_VER 1
#define UPNP_MINOR_VER 0

// DLNA Headers
#define DLNA_HEADER_TRANSFER_MODE               L"transferMode.dlna.org:";
#define DLNA_HEADER_TRANSFER_BACKGROUND_MODE    L"Background";

#define IsHttpStatusOK(request) (HTTP_STATUS_OK <= request && request < HTTP_STATUS_AMBIGUOUS)

