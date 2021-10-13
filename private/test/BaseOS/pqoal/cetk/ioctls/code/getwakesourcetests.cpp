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


////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/
#include "tuxIoctls.h"
#include "commonIoctlTests.h"
#include <nkintr.h>

#define NUM_ITERS 1000

DWORD getRandomDword ();
/****** Kernel mode **********************************************************/
__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}

////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

TESTPROCAPI
testGetWakeSource(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD returnVal = TPR_FAIL;
    DWORD dwBytesReturned = 0;
    DWORD dwRet = 0;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("This test uses KernelIoControl to call IOCTL_HAL_GET_WAKE_SOURCE once."));
    Info(_T("The retured output buffer is checked to make sure the wake source is valid."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    if(KernelIoControl(IOCTL_HAL_GET_WAKE_SOURCE,
                              NULL, 0,
                              &dwRet, sizeof(dwRet),
                              &dwBytesReturned))
    {

        Info(_T("IOCTL_HAL_GET_WAKE_SOURCE passed returning %d bytes and wake sources %d"),dwBytesReturned,dwRet);
        if(dwRet == SYSWAKE_UNKNOWN || (dwRet >= 1 && dwRet < SYSWAKE_MAXIMUM))
        {
            Info(_T("Wake Source is valid"));
        }
        else
        {
            Error(_T("Wake Source is not valid"));
            goto cleanAndReturn;
        }
    }
    else
    {
        Error(_T("IOCTL_HAL_GET_WAKE_SOURCE Failed with GetLastError of %d"),GetLastError());
        goto cleanAndReturn;
    }
   
    returnVal = TPR_PASS;
    Info(_T("testGetWakeSource passed."));

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}

TESTPROCAPI
testGetWakeSourceInParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwI = 0;
    DWORD dwRet = 0, dwBytesReturned = 0;
    DWORD dwInBuf, dwSize;
    DWORD returnVal = TPR_FAIL;
        
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("Running testGetWakeSourceInParam"));
    Info(_T("This test will call KernelIoControl with IOCTL_HAL_GET_WAKE_SOURCE using"));
    Info(_T("an input buffer, which should be ignored.  It will call the Ioctl %d times."),NUM_ITERS);
    Info(_T("The input will be 1-4 bytes with a random value in the buffer."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    //Test In Parameters
    for(dwI = 0; dwI < NUM_ITERS; dwI++)
    {
        dwInBuf = getRandomDword();
        dwSize = (dwI % 4) + 1;
        
        if(KernelIoControl(IOCTL_HAL_GET_WAKE_SOURCE,
                                  &dwInBuf, dwSize,
                                  &dwRet, sizeof(dwRet),
                                  &dwBytesReturned))
        {

            Info(_T("IOCTL_HAL_GET_WAKE_SOURCE passed returning %d bytes and wake sources %d"),dwBytesReturned,dwRet);
            if(dwRet == SYSWAKE_UNKNOWN || (dwRet >= 1 && dwRet < SYSWAKE_MAXIMUM))
            {
                Info(_T("Wake Source is valid"));
            }
            else
            {
                Error(_T("Wake Source is not valid"));
                goto cleanAndReturn;
            }
        }
        else
        {
            Error(_T("IOCTL_HAL_GET_WAKE_SOURCE Failed with GetLastError of %d"),GetLastError());
            goto cleanAndReturn;
        }
    
    }

    returnVal = TPR_PASS;
    Info(_T("testGetWakeSourceInParam passed."));

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}


TESTPROCAPI
testGetWakeSourceOutParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwRet = 0, dwBytesReturned = 0;
    BYTE rgbBuffer[1024];
    DWORD dwI;
    DWORD returnVal = TPR_FAIL;
    
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("Running testGetWakeSourceOutParam"));
    Info(_T("This test will call KernelIoControl with IOCTL_HAL_GET_WAKE_SOURCE using"));
    Info(_T("corrent and incorrect output buffer arguments to test whether the IOCTL passes"));
    Info(_T("and fails at appropriate times."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    Info(_T("First we call IOCTL_HAL_GET_WAKE_SOURCE with a an oversized buffer.  This should be okay."));

    if(KernelIoControl(IOCTL_HAL_GET_WAKE_SOURCE,
                              NULL, 0,
                              rgbBuffer, 1024,
                              &dwBytesReturned))
    {
        dwRet = *(DWORD*)&rgbBuffer[0];
        Info(_T("IOCTL_HAL_GET_WAKE_SOURCE passed returning %d bytes and wake sources %d"),dwBytesReturned,dwRet);
        if(dwRet == SYSWAKE_UNKNOWN || (dwRet >= 1 && dwRet < SYSWAKE_MAXIMUM))
        {
            Info(_T("Wake Source is valid"));
        }
        else
        {
            Error(_T("Wake Source is not valid"));
            goto cleanAndReturn;
        }
    }
    else
    {
        Error(_T("IOCTL_HAL_GET_WAKE_SOURCE Failed with GetLastError of %d"),GetLastError());
        goto cleanAndReturn;
    }

    Info(_T("Now we call IOCTL_HAL_GET_WAKE_SOURCE with undersized buffers less than 4 bytes."));
    Info(_T("These calls should fail."));

    //Make sure this fails with Buffer size less than 4
    dwI = 3;
    do {
        if(KernelIoControl(IOCTL_HAL_GET_WAKE_SOURCE,
                                  NULL, 0,
                                  rgbBuffer, dwI,
                                  &dwBytesReturned))
        {
            Error(_T("IOCTL_HAL_GET_WAKE_SOURCE Passed with an output buffer of %d"),dwI);
            goto cleanAndReturn;
        }
        else
        {
            Info(_T("IOCTL_HAL_GET_WAKE_SOURCE correctly failed with output buffer of %d"),dwI);
        }
    } while (dwI--);

    //Make sure this fails with a NULL Buffer
    Info(_T("Now we call IOCTL_HAL_GET_WAKE_SOURCE with a null output buffer."));
    Info(_T("This call should fail."));
    if(KernelIoControl(IOCTL_HAL_GET_WAKE_SOURCE,
                              NULL, 0,
                              NULL, 4,
                              &dwBytesReturned))
    {
        Error(_T("IOCTL_HAL_GET_WAKE_SOURCE Passed with a null output buffer"));
        goto cleanAndReturn;
    }
    else
    {
        Info(_T("IOCTL_HAL_GET_WAKE_SOURCE correctly failed with a null output buffer"));
    }
 
    returnVal = TPR_PASS;
    Info(_T("testGetWakeSourceOutParam passed."));

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}