//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    sslinc.h

Abstract:

    SSL/WinSock2 LSP internal master header file.

--*/

typedef int WINSOCK_STATUS;
//#include <windows.h>
//#include <winsock.h>
//#include <ws2spi.h>

#ifndef UNDER_CE
#include <wchar.h>
#endif // !UNDER_CE







#include <stdlib.h>
#include <string.h>

#include <schnlsp.h>

#define SECURITY_WIN32
#include <security.h>
#include <spseal.h>
#include <sspi.h>


//#include <debug.h>

#include "sslsock.h"
//#include "sslinst.h"
#include "ssltypes.h"
//#include "hndshake.h"
//#include "sslfuncs.h"

