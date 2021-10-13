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
