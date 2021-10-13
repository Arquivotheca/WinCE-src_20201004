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
#include "kxmips.h"

        SBTTL("_setjmp")
//++
//
// Dummy implementation of _setjmp for COREDLL.  This is here because _setjmp
// is a permanent intrinsic to the compiler, and therefore cannot be defined.
//
//--

    LEAF_ENTRY(_setjmp)

        break   1
        j       ra

    .end    _setjmp

