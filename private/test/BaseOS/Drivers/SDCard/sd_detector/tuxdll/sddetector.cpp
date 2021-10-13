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

#include "main.h"

TESTPROCAPI DetectSD(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    IGNORE_NONEXECUTE_CMDS(uMsg);

    return hasSDMemory() ? TPR_PASS : TPR_FAIL;
}

TESTPROCAPI DetectSDHC(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    IGNORE_NONEXECUTE_CMDS(uMsg)
    return hasSDHCMemory() ? TPR_PASS : TPR_FAIL;
}

TESTPROCAPI DetectMMC(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    IGNORE_NONEXECUTE_CMDS(uMsg)
    return hasMMC() ? TPR_PASS : TPR_FAIL;
}

TESTPROCAPI DetectMMCHC(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    IGNORE_NONEXECUTE_CMDS(uMsg)
    return hasMMCHC() ? TPR_PASS : TPR_FAIL;
}

TESTPROCAPI DetectSDIO(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    IGNORE_NONEXECUTE_CMDS(uMsg)
    return hasSDIO() ? TPR_PASS : TPR_FAIL;
}
