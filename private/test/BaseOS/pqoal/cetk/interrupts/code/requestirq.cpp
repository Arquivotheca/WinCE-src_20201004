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

#include "tuxInterrupts.h"

/* for device loc structure */
#include "ceddk.h"

#include "interruptCmdLine.h"


#define REQUEST_IRQ_RAND_ITERATIONS 10000

/*
 * bAllowedToPass helps with confirming that the routines are
 * working correctly from user mode.  In this case the routines
 * should never succeed.  Using this flag, we can run the test
 * in both kernel mode and user mode and easily tell the dll
 * what we want it to do.
 */


DWORD
getRandomDword ()
{
    UINT uiRand = 0;
    rand_s(&uiRand);
    return (DWORD)(uiRand);
}

BOOL
requestIRQ (DEVICE_LOCATION * deLoc, DWORD * pdwReturnedIRQ)
{

    if (!deLoc)
    {
       return (FALSE);
    }

    BOOL rc = KernelIoControl (IOCTL_HAL_REQUEST_IRQ, deLoc, sizeof (*deLoc),
                 pdwReturnedIRQ, sizeof (*pdwReturnedIRQ), NULL);
  return (rc);
}

BOOL
exerciseRequestIRQRandom (DWORD dwMaxIt, BOOL bAllowedToPass);

BOOL 
exerciseRequestIRQUserParams(BOOL bAllowedToPass);

TESTPROCAPI exerciseRequestIRQRandom(
                                      UINT uMsg,
                                      TPPARAM tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE)
{
  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
    {
      return (TPR_NOT_HANDLED);
    }

  breakIfUserDesires ();

  BOOL bAllowedToPass = TRUE;

  if (lpFTE->dwUserData & MUST_FAIL)
    {
      bAllowedToPass = FALSE;
      Info (_T("If the IOCTL call succeeds for any of these tests, it ")
        _T("is a failure."));
    }
  else
    {
      Info (_T("This test will pass, no matter what the IOCTL return value is."));
    }

  if (exerciseRequestIRQRandom (REQUEST_IRQ_RAND_ITERATIONS, bAllowedToPass))
    {
      return TPR_PASS;
    }
  else
    {
      return TPR_FAIL;
    }
}


BOOL
exerciseRequestIRQRandom (DWORD dwMaxIt, BOOL bAllowedToPass)
{
  /* want the loop to terminate */
  if (dwMaxIt == DWORD_MAX)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    Info (_T(""));
    Info (_T("This test calls IOCTL_HAL_REQUEST_IRQ with a DEVICE_LOCATION"));
    Info (_T("structure randomly filled with data."));
    Info (_T("This test will do this %u times."), dwMaxIt);
    if(bAllowedToPass)
    {
        Info (_T("This test will pass regardless of how the IOCTL returns."));
        Info (_T("It's meant to exercise the IOCTL with random values to ensure"));
        Info (_T("that this IOCTL will not cause the kernel to crash."));
    }
    else
    {
        Info (_T("Because this test is run in user mode, the IOCTL should NEVER pass"));
        Info (_T("If any call to this IOCTL passes, the test fails."));
        Info (_T("It's also meant to exercise the IOCTL with random values to ensure"));
        Info (_T("that this IOCTL will not cause the kernel to crash."));
    }
    Info (_T(""));

    DWORD numberOfSuccesses = 0;

    Info (_T("Setting PhysicalLoc to zero (it is reserved)."));
    Info (_T("All other members are random"));

    for (DWORD dwIt = 0; dwIt < dwMaxIt; dwIt++)
    {
        DEVICE_LOCATION deLoc;

        deLoc.IfcType = getRandomDword ();
        deLoc.BusNumber = getRandomDword();
        deLoc.LogicalLoc = getRandomDword();
        deLoc.PhysicalLoc = NULL;
        deLoc.Pin = getRandomDword();
        Info (_T("Testing (%u) with DEVICE_LOCATION IfcType %u BusNumber %u LogicalLoc %u PhysicalLoc %u Pin %u"), dwIt, (DWORD) deLoc.IfcType, 
            (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);

        DWORD dwReturnedIRQ;
        __try {
            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    numberOfSuccesses++;
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }
        } __except ( EXCEPTION_EXECUTE_HANDLER ) {
            Info (_T("EXCEPTION: Test failed (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), 
                dwIt, (DWORD) deLoc.IfcType, 
                (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, 
                (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);
            goto cleanAndReturn;
        } 
    }    
    
    Info (_T("IOCTL returned true %u times and false %u times."),
        numberOfSuccesses, dwIt - numberOfSuccesses);
    
    numberOfSuccesses = 0;
    Info (_T("Choosing Pin between 1 and 4."));
    Info (_T("Setting PhysicalLoc to zero (it is reserved)."));

    for (DWORD dwIt = 0; dwIt < dwMaxIt; dwIt++)
    {
        DEVICE_LOCATION deLoc;

        deLoc.IfcType = getRandomDword();
        deLoc.BusNumber = getRandomDword();
        deLoc.LogicalLoc = getRandomDword();
        deLoc.PhysicalLoc = NULL;
        deLoc.Pin = getRandomInRange (1, 4);
        Info (_T("Testing (%u) with DEVICE_LOCATION %u %u %u %u %u"), dwIt, (DWORD) deLoc.IfcType, 
            (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);

        DWORD dwReturnedIRQ;
        __try {
            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    numberOfSuccesses++;
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }

        } __except ( EXCEPTION_EXECUTE_HANDLER ) {
            Info (_T("EXCEPTION: Test failed (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), 
                dwIt, (DWORD) deLoc.IfcType, 
                (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, 
                (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);
            goto cleanAndReturn;
        } 
    }
    Info (_T("IOCTL returned true %u times and false %u times."),
        numberOfSuccesses, dwIt - numberOfSuccesses);

    numberOfSuccesses = 0;
    Info (_T("Choosing Pin between 1 and 4."));
    Info (_T("IfcType restricted to valid types."));
    Info (_T("Setting PhysicalLoc to zero (it is reserved)."));


    for (DWORD dwIt = 0; dwIt < dwMaxIt; dwIt++)
    {
        DEVICE_LOCATION deLoc;

        deLoc.IfcType = getRandomInRange (0,MaximumInterfaceType + 1) - 1;
        deLoc.BusNumber = getRandomDword();
        deLoc.LogicalLoc = getRandomDword();
        deLoc.PhysicalLoc = NULL;
        deLoc.Pin = getRandomInRange (1, 4);
        Info (_T("Testing (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), dwIt, (DWORD) deLoc.IfcType, 
            (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);

        DWORD dwReturnedIRQ;
        __try {
            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    numberOfSuccesses++;
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }
        } __except ( EXCEPTION_EXECUTE_HANDLER ) {
            Info (_T("EXCEPTION: Test failed (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), 
                dwIt, (DWORD) deLoc.IfcType, 
                (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, 
                (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);
            goto cleanAndReturn;
        } 
    }
    Info (_T("IOCTL returned true %u times and false %u times."),
        numberOfSuccesses, dwIt - numberOfSuccesses);

    numberOfSuccesses = 0;
    Info (_T("Choosing Pin between 1 and 4."));
    Info (_T("IfcType restricted to valid types."));
    Info (_T("LogicalLoc limited to range of 0,1024."));
    Info (_T("Setting PhysicalLoc to zero (it is reserved)."));


    for (DWORD dwIt = 0; dwIt < dwMaxIt; dwIt++)
    {
        DEVICE_LOCATION deLoc;

        deLoc.IfcType = getRandomInRange (0,MaximumInterfaceType + 1) - 1;
        deLoc.BusNumber = getRandomDword();
        deLoc.LogicalLoc = getRandomInRange(0,1024);
        deLoc.PhysicalLoc = NULL;
        deLoc.Pin = getRandomInRange (1, 4);
        Info (_T("Testing (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), dwIt, (DWORD) deLoc.IfcType, 
            (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);

        DWORD dwReturnedIRQ;
        __try {
            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    numberOfSuccesses++;
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }
        } __except ( EXCEPTION_EXECUTE_HANDLER ) {
            Info (_T("EXCEPTION: Test failed (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), 
                dwIt, (DWORD) deLoc.IfcType, 
                (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, 
                (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);
            goto cleanAndReturn;
        } 
    }
    Info (_T("IOCTL returned true %u times and false %u times."),
        numberOfSuccesses, dwIt - numberOfSuccesses);
    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

TESTPROCAPI  exerciseRequestIRQUserParams(
                                      UINT uMsg,
                                      TPPARAM tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE)
{
  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
    {
      return (TPR_NOT_HANDLED);
    }

  breakIfUserDesires ();

  BOOL bAllowedToPass = TRUE;

  if (lpFTE->dwUserData & MUST_FAIL)
    {
      bAllowedToPass = FALSE;
      Info (_T("If the IOCTL call succeeds for any of these tests, it ")
        _T("is a failure."));
    }
  else
    {
      Info (_T("This test will pass, not matter the IOCTL return value is."));
    }

  if (exerciseRequestIRQUserParams (bAllowedToPass))
    {
      return TPR_PASS;
    }
  else
    {
      return TPR_FAIL;
    }
}

BOOL
parseAndConvert (const TCHAR *const pszString, DWORD * pdwVal)
{
  if (!pszString || !pdwVal)
    {
      return (FALSE);
    }
  
  BOOL returnVal = FALSE;

  ULONGLONG ullVal;

  /* at this point we will not read past the end of the array */
  if (!((strToULONGLONG (pszString, &ullVal)) &&
    (ullVal <= DWORD_MAX)))
    {
      Error (_T("Error reading and parsing param %s. ")
         _T("Must be DWORD <= DWORD_MAX."), pszString);
      goto cleanAndReturn;
    }

  *pdwVal = (DWORD) ullVal;

  returnVal = TRUE;
 cleanAndReturn:

  return (returnVal);
}


DEVICE_LOCATION g_DefaultParams[] = 
{
    {0,0,3,NULL,1},
    {1,1,3,NULL,2},
    {0,0,4,NULL,3},
    {0,0,4,NULL,4},
    {0xffffffff,1,4,NULL,4},
    {Internal,1,4,NULL,4},
    {Isa,1,4,NULL,4},
    {Eisa,1,4,NULL,4},
    {MicroChannel,1,4,NULL,4},
    {TurboChannel,1,4,NULL,4},
    {PCIBus,1,4,NULL,4},
    {VMEBus,1,4,NULL,4},
    {PCIBus,1,4,NULL,4},
    {NuBus,1,4,NULL,4},
    {PCMCIABus,1,4,NULL,4},
    {CBus,1,4,NULL,4},
    {MPIBus,1,4,NULL,4},
    {MPSABus,1,4,NULL,4},
    {ProcessorInternal,1,4,NULL,4},
    {InternalPowerBus,1,4,NULL,4},
    {PNPISABus,1,4,NULL,4},
    {PNPBus,1,4,NULL,4},
    {MaximumInterfaceType,1,4,NULL,4},
    {Internal,1,0x010500000,NULL,1},
    {Internal,1,0x320,NULL,1},
    {Internal,1,0x08000300,NULL,1},
    {Internal,1,0x04000000,NULL,1},
    {Internal,1,0x2,NULL,1},
    {Internal,1,0x2,NULL,1},
    {PCIBus, 1, 0, NULL, 1},
    {PCIBus, 0x201, 0, NULL, 2},
    {PCIBus, 0x503, 0, NULL, 3},
    {PCIBus, 0x709, 0, NULL, 4},
};
/*
DWORD IfcType;
  DWORD BusNumber;
  DWORD LogicalLoc;
  PVOID PhysicalLoc;
  DWORD Pin;

/*
 * This test passes unless there is a parsing error on the command
 * line.  We can not determine programmatically whether the users
 * input should fail or pass the ioctl.  As long it is in the correct
 * form we pass.
 */
BOOL 
exerciseRequestIRQUserParams(BOOL bAllowedToPass)
{
    BOOL returnVal = FALSE;
    BOOL bUserParamGiven = FALSE;
    cTokenControl tokens;

    Info (_T(""));
    Info (_T("This test allows one to specify values ")
        _T("IOCTL_HAL_REQUEST_IRQ."));
    Info (_T(""));
    Info (_T("In automated testing, the inputs to this IOCTL are so spread out, it is quite"));
    Info (_T("unlikely that random testing will find positive values.  The"));
    Info (_T("testing would then consist of just negative testing, which"));
    Info (_T("is not ideal.  Therfore, we have this test to allow the user"));
    Info (_T("to input specific values."));
    Info (_T(""));
    Info (_T("To specify a given value, the test needs the four parameters"));
    Info (_T("that make up the DEVICE_LOCATION structure passed to this"));
    Info (_T("IOCTL.  Note that PhysicalLoc is reserved and is currently"));
    Info (_T("a pointer, so is not taken as an argument."));
    Info (_T(""));
    Info (_T("args:  %s <IfcType> <BusNumber> <LogicalLoc> <Pin>"),
        ARG_STR_DEV_LOC);
    Info (_T(""));
    Info (_T("These are all given as non-negative numbers.  The structure "));
    Info (_T("takes DWORDS, and will parse any value >= 0xfffffffff."));
    Info (_T(""));
    Info (_T("IfcType is defined as an enum.  The numerical values associated"));
    Info (_T("with this enum are:"));
    Info (_T("  InterfaceTypeUndefined = %u, Internal = %u, Isa = %u, Eisa = %u,"),
        InterfaceTypeUndefined, Internal, Isa, Eisa);
    Info (_T("  MicroChannel = %u, TurboChannel = %u, PCIBus = %u, VMEBus = %u,"),
        MicroChannel,  TurboChannel,  PCIBus,  VMEBus);
    Info (_T("  NuBus = %u, PCMCIABus = %u, CBus = %u, MPIBus = %u, MPSABus = %u,"),
        NuBus,  PCMCIABus,  CBus,  MPIBus,  MPSABus);
    Info (_T("  ProcessorInternal = %u, InternalPowerBus = %u, PNPISABus = %u,"),
        ProcessorInternal,  InternalPowerBus,  PNPISABus);
    Info (_T("  PNPBus = %u, MaximumInterfaceType = %u"), 
        PNPBus, MaximumInterfaceType);
    Info (_T(""));
    Info (_T("Multiple sets of %s are supported on a single command line.  Keep"),
        ARG_STR_DEV_LOC);
    Info (_T("that CE limits command lines (including tux options) to 255 chars."));
    Info (_T(""));
    Info (_T(""));

    /*
    * if user doesn't specify the -c param to tux the cmd line global
    * var is null.  In this case don't even try to parse command line
    * args.
    */
    if (g_szDllCmdLine != NULL && tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
    {
        /* inside this block assume false */
        BOOL bCalledIoctl = FALSE;

        for (DWORD dwTokenIndex = 0; dwTokenIndex < tokens.listLength();
            dwTokenIndex++)
        {
            DWORD dwDevLocIndex;

            if (!findTokenI(&tokens, ARG_STR_DEV_LOC, dwTokenIndex,
                  &dwDevLocIndex))
            {
                /* done */
                returnVal = bCalledIoctl ? TRUE : FALSE;
                goto cleanAndReturn;
            }

            /* we found something at dwDevLocIndex */
            bUserParamGiven = TRUE;

            /* next 4 values should be the location data */

            DWORD dwEndingIndex;

            if (DWordAdd (dwDevLocIndex, 4, &dwEndingIndex) != S_OK)
            {
                Error (_T("Overflow when finding ending index."));
                goto cleanAndReturn;
            }
      
            if (dwEndingIndex >= tokens.listLength())
            {
                Error (_T("Not enough params given to %s option."),
                 ARG_STR_DEV_LOC);
                goto cleanAndReturn;
            }
      
      /* at this point we will not read past the end of the array */

            DEVICE_LOCATION deLoc;

            BOOL bRet = TRUE;

            bRet = parseAndConvert (tokens[dwDevLocIndex + 1], &deLoc.IfcType);
            bRet = parseAndConvert (tokens[dwDevLocIndex + 2], &deLoc.BusNumber);
            bRet = parseAndConvert (tokens[dwDevLocIndex + 3], &deLoc.LogicalLoc);
            deLoc.PhysicalLoc = NULL;
            bRet = parseAndConvert (tokens[dwDevLocIndex + 4], &deLoc.Pin);

            if (!bRet)
            {
                goto cleanAndReturn;
            }

            /* structure is loaded */

            Info (_T("Loaded a DEVICE_LOCATION stucture with:"));
            Info (_T("IfcType = %u = 0x%x"), deLoc.IfcType, deLoc.IfcType);
            Info (_T("BusNumber = %u = 0x%x"), deLoc.BusNumber, deLoc.BusNumber);
            Info (_T("LogicalLoc = %u = 0x%x"), deLoc.LogicalLoc, deLoc.LogicalLoc);
            Info (_T("PhysicalLoc = %u = 0x%x ")
            _T("(reserved field.  test uses default)"), 
            deLoc.PhysicalLoc, deLoc.PhysicalLoc);
            Info (_T("Pin = %u = 0x%x"), deLoc.Pin, deLoc.Pin);
      
            /* make the call */

            DWORD dwReturnedIRQ;
            
            bCalledIoctl = TRUE;
            
            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    Info (_T("Successful when calling IOCTL_HAL_REQUEST_IRQ."));
                    Info (_T("Returned IRQ is %u = 0x%x."), 
                    dwReturnedIRQ, dwReturnedIRQ);
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }
            else
            {
                Info (_T("Not successful when calling IOCTL_HAL_REQEST_IRQ"));
            }

            Info (_T(""));

            /* 
            * devLocIndex sits on the index for the arg param itself.  we have to move 
            * past this.  If we move past the end of the array the for loop will
            * bail out.  If we don't then we will look for more arguments.
            */
            dwTokenIndex = dwDevLocIndex + 1;

        } /* for */

        /* if we get to this point in the block it all worked */
        returnVal = TRUE;
    } /* if gz_Cmd.... */
    

    if(!bUserParamGiven)
    {
         returnVal = FALSE;
         Info (_T("No arguments were given for this test, so we"));
         Info (_T("will test some default values."));
         Info (_T("However, we strongly recomend that OEM's "));
         Info (_T("run this test manually to make sure the put in "));
         Info (_T("that will cause the IOCTL to return true."));
         for(DWORD dwI = 0; dwI < sizeof(g_DefaultParams)/sizeof(DEVICE_LOCATION); dwI++)
         {
            DEVICE_LOCATION deLoc;
            deLoc = g_DefaultParams[dwI];
            Info (_T("Testing (%u) with DEVICE_LOCATION %u %u %u %u %u\n\r"), dwI, (DWORD) deLoc.IfcType, 
                (DWORD) deLoc.BusNumber, (DWORD) deLoc.LogicalLoc, (DWORD) deLoc.PhysicalLoc, (DWORD) deLoc.Pin);
            
            DWORD dwReturnedIRQ;

            if (requestIRQ (&deLoc, &dwReturnedIRQ))
            {
                if (bAllowedToPass)
                {
                    Info (_T("Successful when calling IOCTL_HAL_REQUEST_IRQ."));
                    Info (_T("Returned IRQ is %u = 0x%x."), 
                        dwReturnedIRQ, dwReturnedIRQ);
                }
                else
                {
                    Error (_T("The ioctl call shouldn't have returned true but")
                     _T("it did."));
                    Error (_T("Got back IRQ of %u = 0x%x"),
                     dwReturnedIRQ, dwReturnedIRQ);
                    Error (_T("Failing..."));
                    goto cleanAndReturn;
                }
            }
            else
            {
                Info (_T("Not successful when calling IOCTL_HAL_REQEST_IRQ"));
            }
         }
         returnVal = TRUE;
    }

cleanAndReturn:

    return (returnVal);
}

