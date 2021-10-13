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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the ConnMgr_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConnMgr_t_
#define _DEFINED_ConnMgr_t_
#pragma once

#include "ConnStatus_t.hpp"

#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// This class encapsulates methods and information for interacting with the
// Connection Manager to retrieve and modify our connection information.
//
class ConnMgr_t
{
private:

    // Copy and assignment are deliberately disabled:
    ConnMgr_t(const ConnMgr_t &rhn);
    ConnMgr_t &operator = (const ConnMgr_t &rhs);

public:
    
    // Constructor and destructor:
    ConnMgr_t(void);
   ~ConnMgr_t(void);

    // Updates the current connection status information and selects
    // the next connection to be used:
    HRESULT
    UpdateConnStatus(
        OUT ConnStatus_t *pStatus,
        OUT MemBuffer_t  *pConnection);

};

};
};
#endif /* _DEFINED_ConnMgr_t_ */
// ----------------------------------------------------------------------------
