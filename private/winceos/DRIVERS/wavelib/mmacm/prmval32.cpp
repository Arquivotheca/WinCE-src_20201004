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
//JRE
/****************************************************************************
    prmval32.c

    msacm

    Copyright (c) 1993-2000 Microsoft Corporation

****************************************************************************/

#include "acmi.h"

//------------------------------------------------------------------------------
//
//  @doc INTERNAL
//
//  @func BOOL | ValidateHandle | validates a handle created with NewHandle
//
//  @parm PHNDL | hLocal | handle returned from NewHandle
//  @parm UINT  | wType  | unique id describing handle type
//
//  @rdesc Returns TRUE  if <h> is a valid handle of type <wType>
//         Returns FALSE if <h> is not a valid handle
//
//  @comm  if the handle is invalid an error will be generated.
//
//------------------------------------------------------------------------------
BOOL
acm_IsValidHandle(HANDLE hLocal, UINT uType)
{
    BOOL fRet = FALSE;

    if (hLocal == NULL) {
        return FALSE;
    }

    try {

        if (TYPE_HACMOBJ == uType) {

            switch (((PACMDRIVERID)hLocal)->uHandleType) {

                case TYPE_HACMDRIVERID:
                case TYPE_HACMDRIVER:
                case TYPE_HACMSTREAM:
                    fRet = TRUE;
                    break;

                default:
                    fRet = FALSE;
                    break;
            }
        } else {
            fRet = (uType == ((PACMDRIVERID)hLocal)->uHandleType);
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
        ERRMSG("Exception in acm_IsValidHandle");
        fRet = FALSE;
    }

    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
acm_IsValidCallback(PVOID fnCallback)
{
    if (fnCallback == NULL)
        return(FALSE);

    return TRUE;
}

