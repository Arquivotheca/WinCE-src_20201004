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


#include <windows.h>
#include <tchar.h>


#include "commonUtils.h"

#include "interruptCmdLine.h"

void
printDisclaimer ()
{
    Info (_T("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"));
    Info (_T(""));
    Info (_T("These tests allocate, free, and manipulate the interrupt "));
    Info (_T("routines.  The tests, if they complete successfully, should"));
    Info (_T("leave the device in the starting state.  The tests will"));
    Info (_T("manipulate the interrupt routines, but they should reset"));
    Info (_T("everything back correctly."));
    Info (_T(""));
    Info (_T("If the interrupt routines are not implemented correctly it is"));
    Info (_T("highly probable that these tests might crash or lock up the"));
    Info (_T("device.  Futhermore, the tests can pass and the device can"));
    Info (_T("still display abnormal behavior.  Abnormal device behavior"));
    Info (_T("after running these tests generally suggests a bug in the"));
    Info (_T("interrupt routines, not a test issue."));
    Info (_T(""));
    Info (_T("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"));
    Info (_T(""));
}

void
printUsage ()
{
    printDisclaimer ();

    Info (_T("These tests confirm that the interrupt routines are working"));
    Info (_T("correctly.  The testing is broken into general categories, "));
    Info (_T("which are then broken into individual test cases."));
    Info (_T(""));
    Info (_T("Kernel Mode vs User Mode - The IOCTLs behave differently in these"));
    Info (_T("two modes.  Run the kernel mode tests in kernel mode (-n to tux)"));
    Info (_T("and everything else in user mode.  Note that doing differently"));
    Info (_T("can cause false positives/negatives."));
    Info (_T(""));
    Info (_T("To run the entire test suite, you need to run tux twice:"));
    Info (_T(" Kernel:  tux -o -n -d <name> -x500-9999,20000-29999"));
    Info (_T(" User  :  tux -o -d <name> -x500-1000,10000-19999,20000-20099,30000-39999"));
    Info (_T(""));
    Info (_T(" *** IOCTL_HAL_REQUEST_SYSINTR / IOCTL_HAL_RELEASE_SYSINTR ***"));
    Info (_T(""));
    Info (_T("These tests exercise the above ioctls.  These ioctls control "));
    Info (_T("the allocation and release of the SYSINTRs on the system."));
    Info (_T(""));
    Info (_T("\"In Range\" - The tests need knowledge of the valid IRQs on the"));
    Info (_T("system.  With this knowledge the tests can verify that both"));
    Info (_T("supported and erronous values are handled correctly.  The \"In"));
    Info (_T("Range\" moniker denotes tests that work with only IRQs that are"));
    Info (_T("in the supported range for the given system.  The other tests"));
    Info (_T("work with any possible IRQ, some of which might be outside of the"));
    Info (_T("system's supported range."));
    Info (_T(""));
    Info (_T("The valid IRQ range for a given system is platform dependent.  The"));
    Info (_T("user can either tell the test the given IRQ range or the test can"));
    Info (_T("guess it.  The guessing routine starts at %u and works down, "),
    TEST_IRQ_END);
    Info (_T("looking for the first interrupt that could be allocated.  For"));
    Info (_T("situations in which this method won't find the upper bound of the"));
    Info (_T("interrupt range, the user can specify this value through the"));
    Info (_T("command line arguments.  The lower bound is currently fixed at"));
    Info (_T("zero and can not be guessed by the test."));
    Info (_T(""));
    Info (_T("Tests that are not influenced by whether the IRQ values are valid"));
    Info (_T("for the given platform do not need the \"In Range\" moniker."));
    Info (_T("They are only included once in the function tables."));
    Info (_T(""));
    Info (_T("args:"));
    Info (_T("  %s  Gives the IRQ to begin at (inclusive)"), ARG_STR_IRQ_BEGIN);
    Info (_T("  %s    Gives the IRQ to end at (inclusive)"), ARG_STR_IRQ_END);
    Info (_T(""));
    Info (_T("For Multiple IRQ tests:"));
    Info (_T("  %s  Gives the max number of IRQs per SYSINTR"),
    ARG_STR_MAX_IRQ_PER_SYSINTRS);
    Info (_T(""));

    Info (_T("              *** IOCTL_HAL_REQUEST_IRQ testing ***"));
    Info (_T(""));
    Info (_T("This IOCTL provides data about IRQs on the system."));
    Info (_T("The test calls this IOCTL with both random data and targeted data"));
    Info (_T("provided by the user.  See the test themselves for more info."));
    Info (_T(""));
    Info (_T("args:"));
    Info (_T("  %s <IfcType> <BusNumber> <LogicalLoc> <Pin>"),
    ARG_STR_DEV_LOC);
    Info (_T(""));

}



/*
 * handle the command line args for the irq range.
 *
 * this allows the user to specify the irq range on the command line.
 * This function parses these values and returns them through the
 * DWORD pointers.  If no options are given the function doesn't touch
 * these values.
 *
 * return true on successful parsing or if no options are present.
 * Returns false if parsing fails (bad commond line options or
 * something else go wrong).
 */
BOOL
handleCmdLineIRQRange (DWORD * pdwBegin, DWORD * pdwEnd)
{
    BOOL returnVal = FALSE;

    if (!pdwBegin || !pdwEnd)
    {
        return (FALSE);
    }

    /*
   * if user doesn't specify the -c param to tux the cmd line global
   * var is null.  In this case don't even try to parse command line
   * args.
   */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (getOptionDWord (&tokens, ARG_STR_IRQ_BEGIN, pdwBegin))
        {
            Info (_T("User specified beginning IRQ, which is %u."), *pdwBegin);
        }
        else
        {
            Info (_T("Using default beginning IRQ, which is %u."), *pdwBegin);
        }

        if (getOptionDWord (&tokens, ARG_STR_IRQ_END, pdwEnd))
        {
            Info (_T("User specified ending IRQ, which is %u."), *pdwEnd);
        }
        else
        {
            Info (_T("Using default ending IRQ, which is %u."), *pdwEnd);
        }
    }

    if (*pdwBegin > *pdwEnd)
    {
        Error (_T("Beginning IRQ cannot be > ending IRQ.  %u > %u"),
        *pdwBegin, *pdwEnd);
        goto cleanAndReturn;
    }

    returnVal = TRUE;
cleanAndReturn:

    return (TRUE);
}

