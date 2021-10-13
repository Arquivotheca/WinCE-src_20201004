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
//

#include "excpt.h"


#include "ehdata.h"     // Declarations of all types used for EH
#include "ehstate.h"
#include "eh.h"
#include "ehhooks.h"

//
// This routine is a replacement for the corresponding macro in 'ehdata.h'
//

__ehstate_t GetCurrentState(
    EHRegistrationNode  *pFrame,
    PDISPATCHER_CONTEXT  pDC,
    FuncInfo            *pFuncInfo
    )
{
    unsigned int    index;          //  loop control variable
    unsigned int    nIPMapEntry;    //  # of IpMapEntry; must be > 0
    ULONG           ControlPc;      //  aligned address

    ControlPc = pDC->ControlPc;

    DEBUGCHK(pFuncInfo != NULL);
    nIPMapEntry = FUNC_NIPMAPENT(*pFuncInfo);

    DEBUGCHK(FUNC_IPMAP(*pFuncInfo) != NULL);

    for (index = 0; index < nIPMapEntry; index++) {
        if (ControlPc < FUNC_IPTOSTATE(*pFuncInfo, index).Ip) {
            break;
        }
    }

    if (index == 0) {
        // We are at the first entry, could be an error

        return EH_EMPTY_STATE;
    }

    // We over-shot one iteration; return state from the previous slot

    return FUNC_IPTOSTATE(*pFuncInfo, index - 1).State;
}

