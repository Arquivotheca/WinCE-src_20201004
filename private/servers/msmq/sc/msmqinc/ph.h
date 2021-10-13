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

    ph.h

Abstract:

    Falcon Packet Header master include file

--*/

#ifndef __PH_H
#define __PH_H

/*+++

    Falcon Packet header sections order

+------------------------------+-----------------------------------------+----------+
| SECTION NAME                 | DESCRIPTION                             | SIZE     |
+------------------------------+-----------------------------------------+----------+
| Base                         |                                         |          |
+---------------+--------------+-----------------------------------------+----------+
| User          |              |                                         |          |
+---------------+              +-----------------------------------------+----------+
| Security      | Internal     |                                         |          |
+---------------+              +-----------------------------------------+----------+
| Properties    |              |                                         |          |
+---------------+--------------+-----------------------------------------+----------+
| Debug                        |                                         |          |
+------------------------------+-----------------------------------------+----------+
| Session                      |                                         |          |
+---------------+--------------+-----------------------------------------+----------+

---*/

//
//  Alignment on DWORD bounderies
//
#define ALIGNUP4(x) ((((ULONG)(x))+3) & ~3)
#define ISALIGN4(x) (((ULONG)(x)) == ALIGNUP4(x))

#include <_ta.h>
#include "qformat.h"
#include "phbase.h"
#include "phuser.h"
#include "phprop.h"
#include "phsecure.h"
#include "phxact.h"

#include "phcompoundmsg.h"
#include "pheod.h"
#include "pheodack.h"
#include "phSoap.h"
#include "phsrmpenv.h"
#include "phgeneric.h"
#include "phsoapext.h"

#endif // __PH_H
