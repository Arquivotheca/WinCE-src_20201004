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
/*++


    Module Name:

        DRM.h

    Abstract:

        Drm definitions

    Revision History:

        27-Mar-2002    created

--*/

#ifndef __ENCDEC_DRM_H__
#define __ENCDEC_DRM_H__

#ifdef BUILD_WITH_DRM

#include "des.h"                // all include files from the DRMInc directory
#include "sha.h"
#include "pkcrypto.h"
#include "drmerr.h"
#include "drmstub.h"
#include "drmutil.h"
#include "license.h"

#endif //BUILD_WITH_DRM


#endif  // __ENCDEC_DRM_H__
