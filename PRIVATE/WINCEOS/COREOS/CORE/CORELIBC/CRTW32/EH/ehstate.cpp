//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "kernel.h"
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

