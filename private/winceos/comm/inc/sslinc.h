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

//
// SHolden - BUGBUG: When WINCEMACRO is defined, ole2.h is not included in
//           windows.h; therefore, GUID is not defined. We need WINCEMACRO
//           to call into AFD. So for now, I will define GUID for us here.
//

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

