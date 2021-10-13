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

#define NUM_ITERS 1000

/****** Kernel mode **********************************************************/
__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}

DWORD
getRandomDword ()
{
    UINT uiRand = 0;
    rand_s(&uiRand);
    return (DWORD)uiRand;
}


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

//TestIoctlOnce is used by the test routine to test for a positive all IOCTL_HAL_GET_RANDOM_SEED
//It will take the buffers specificied with pInBuf and pOutData, call the Ioctl with them and check 
//the output is different than the data in pOutDataLast.  It will then copy the output to pOutDataLast.
INT TestIoctlOnce(BYTE* pInBuf, DWORD dwSizeBuf, BYTE* pOutData, DWORD dwOut,BYTE* pOutDataLast, DWORD dwOutLast)
{
    DWORD dwBytesRet;
    DWORD dwI;
    INT iRet = TPR_FAIL;

    if (!KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, pInBuf,
            dwSizeBuf, pOutData, dwOut, &dwBytesRet))
    {
        DWORD dwErr = GetLastError();
        if(dwErr == ERROR_NOT_SUPPORTED)
        {
            Error (_T("")); //Blank line
            Error (_T("IOCTL_HAL_GET_RANDOM_SEED is not supported"));

            iRet = TPR_SKIP;
        }
        else
        {
            Error (_T("")); //Blank line
            Error (_T("Called the IOCTL_HAL_GET_RANDOM_SEED function with all correct parameters."));
            Error (_T("The function returned FALSE, while the expected value is TRUE."));
            Error (_T("GetLastError is %d"),dwErr);
        }
        goto cleanAndReturn;
    }

    Info(_T("IOCTL_HAL_GET_RANDOM_SEED returned %d bytes."),dwBytesRet);
    
    if(dwBytesRet > dwOut)
    {
        Error (_T("")); //Blank line
        Error (_T("IOCTL_HAL_GET_RANDOM_SEED returned more bytes than the buffer supports"));
        goto cleanAndReturn;
    }

    for(dwI=0; dwI < dwBytesRet; dwI+=8)
    {
        Info(_T("%d  :  %02x %02x %02x %02x %02x %02x %02x %02x"),dwI,
            pOutData[dwI],pOutData[dwI+1],pOutData[dwI+2],pOutData[dwI+3],
            pOutData[dwI+4],pOutData[dwI+5],pOutData[dwI+6],pOutData[dwI+7]);
    }
    
    if(memcmp(pOutData,pOutDataLast,dwBytesRet) == 0)
    {
        Error (_T("")); //Blank line
        Error (_T("IOCTL_HAL_GET_RANDOM_SEED returned the same seed two times in a row"));
        goto cleanAndReturn;
    }
    
    memcpy(pOutDataLast, pOutData, dwBytesRet);

    iRet = TPR_PASS;

cleanAndReturn:
    return iRet;

}


/*******************************************************************************
 *
 *     IOCTL_HAL_GET_RANDOM_SEED - CHECK IF IOCTL SUPPORTED
 *
 ******************************************************************************/
/*

 */

TESTPROCAPI testRandomSeedIoctlSupported(
               UINT uMsg,
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    BYTE OutData[1024];
    BYTE OutDataLast[1024] = {0};
    DWORD dwBytesRet = 0;
    DWORD dwIter = NUM_ITERS;
    DWORD dwI;
    /* only supporting executing the test */

    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("Running testRandomSeedIoctlSupported"));
    Info(_T("This test will call correctly call KernelIoControl with IOCTL_HAL_GET_RANDOM_SEED"));
    Info(_T("It will call it with the maximum size 1024 byte output buffer %d times."),dwIter);
    Info(_T("It will make sure that the same buffer is not returned twice in a row."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    do {

        if (!KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,
                0, OutData, sizeof(OutData), &dwBytesRet))
        {
            DWORD dwErr = GetLastError();
            if(dwErr == ERROR_NOT_SUPPORTED)
            {
                Error (_T("")); //Blank line
                Error (_T("IOCTL_HAL_GET_RANDOM_SEED is not supported"));

                returnVal = TPR_SKIP;
            }
            else
            {
                Error (_T("")); //Blank line
                Error (_T("Called the IOCTL_HAL_GET_RANDOM_SEED function with all correct parameters."));
                Error (_T("The function returned FALSE, while the expected value is TRUE."));
                Error (_T("GetLastError is %d"),dwErr);
            }
            goto cleanAndReturn;
        }

        Info(_T("IOCTL_HAL_GET_RANDOM_SEED returned %d bytes."),dwBytesRet);
        
        if(dwBytesRet >sizeof(OutData))
        {
            Error (_T("")); //Blank line
            Error (_T("IOCTL_HAL_GET_RANDOM_SEED returned more bytes than the buffer supports"));
            goto cleanAndReturn;
        }

        for(dwI=0; dwI < dwBytesRet; dwI+=8)
        {
            Info(_T("%d  :  %02x %02x %02x %02x %02x %02x %02x %02x"),dwI,
                OutData[dwI],OutData[dwI+1],OutData[dwI+2],OutData[dwI+3],
                OutData[dwI+4],OutData[dwI+5],OutData[dwI+6],OutData[dwI+7]);
        }
        
        if(memcmp(OutData,OutDataLast,dwBytesRet) == 0)
        {
            Error (_T("")); //Blank line
            Error (_T("IOCTL_HAL_GET_RANDOM_SEED returned the same seed two times in a row"));
            goto cleanAndReturn;
        }
        
        memcpy(OutDataLast, OutData, dwBytesRet);

    }  while(dwIter--);

    Info(_T("testRandomSeedIoctlSupported passed"),dwBytesRet);
    returnVal = TPR_PASS;

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}



/*******************************************************************************
 *
 *     IOCTL_HAL_GET_RANDOM_SEED - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/


TESTPROCAPI
testRandomSeedIoctlInParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    BYTE OutData[1024];
    BYTE OutDataLast[1024] = {0};
    DWORD dwBytesRet = 0;
    DWORD dwIter = NUM_ITERS;
    BYTE* pInBuf;
    DWORD dwSizeBuf;
    DWORD arg;
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("Running testRandomSeedIoctlInParam"));
    Info(_T("This test will call KernelIoControl with IOCTL_HAL_GET_RANDOM_SEED using"));
    Info(_T("an input buffer, which should be ignored.  It will call the Ioctl %d times."),dwIter);
    Info(_T("A third of the calls will use a one byte input buffer, another third will use"));
    Info(_T("a two byte input buffer, and the last third will use a four byte buffer."));
    Info(_T("It will make sure that the same buffer is not returned twice in a row."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    do {
        pInBuf = (BYTE*)&arg;
        arg = getRandomDword();
        if(dwIter < NUM_ITERS/3)
        {
            dwSizeBuf = 1;
            Info(_T("Passing in buffer size 1 with value %d"),*(UCHAR*)pInBuf); 
        }
        else if(dwIter < 2*NUM_ITERS/3)
        {
            dwSizeBuf = 2;
            Info(_T("Passing in buffer size 2 with value %d"),*(USHORT*)pInBuf); 
        }
        else    
        {
            dwSizeBuf = 4;
            Info(_T("Passing in buffer size 4 with value %d"),*(ULONG*)pInBuf); 
        }


        returnVal = TestIoctlOnce(pInBuf, dwSizeBuf, OutData, sizeof(OutData),OutDataLast,sizeof(OutDataLast));
        if(TPR_PASS != returnVal)
        {
            goto cleanAndReturn;
        }
        else
        {
            returnVal = TPR_FAIL; // Reset to default value.
        }

    }  while(dwIter--);

    returnVal = TPR_PASS;
    Info(_T("testRandomSeedIoctlInParam passed"));

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}


/*******************************************************************************
 *
 *     IOCTL_HAL_GET_RANDOM_SEED - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/


TESTPROCAPI
testRandomSeedIoctlOutParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    BYTE OutData[2048] = {0};
    BYTE OutDataLast[2048] = {0};
    DWORD dwBytesRet = 0;
    DWORD dwIter = sizeof(OutData);
    DWORD dwI=0;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info(_T("Running testRandomSeedIoctlOutParam"));
    Info(_T("This test will call KernelIoControl with IOCTL_HAL_GET_RANDOM_SEED using"));
    Info(_T("correct and incorrect output buffer arguments."));
    Info(_T("It calls IOCTL_HAL_GET_RANDOM_SEED using buffer sizes 0-1024 which should pass."));
    Info(_T("It then calls IOCTL_HAL_GET_RANDOM_SEED using buffer sizes 1025-2048 which should fail."));
    Info(_T("It then calls IOCTL_HAL_GET_RANDOM_SEED using a buffer size of 0xffffffff, which should fail"));
    Info(_T("and it makes sure that a NULL output buffer fails."));

    if(!inKernelMode())
    {
        Error(_T("Test must be run in Kernel Mode, so we must fail the test"));
        Error(_T("Re-run with -n option"));
        goto cleanAndReturn;
    }

    do {

        dwI++;
        
        Info(_T("Using output size buffer size of %d\n"),dwI);
        if(dwI <= 1024)  //Test IOCTL should pass
        {
            returnVal = TestIoctlOnce(NULL, 0, OutData, dwI,OutDataLast,sizeof(OutDataLast));
            if(TPR_PASS != returnVal)
            {
                goto cleanAndReturn;
            }
            else
            {
                returnVal = TPR_FAIL; // Reset to default value.
            }
        }
        else
        {
            //Test IOCTL should fail
            if (KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,0, 
                OutData, dwI, &dwBytesRet))
            {
                Error(_T("IOCTL_HAL_GET_RANDOM_SEED must fail when given an output buffer greater than 1024"));
                Error(_T("IOCTL_HAL_GET_RANDOM_SEED passed with an output buffer of %d"),dwI);
                goto cleanAndReturn;
            }
        }

    }  while(dwIter--);

    //Test IOCTL should fail
    dwI = 0xffffffff;
    if (KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,0, 
        OutData, dwI, &dwBytesRet))
    {
        Error(_T("IOCTL_HAL_GET_RANDOM_SEED must fail when given an output buffer greater than 1024"));
        Error(_T("IOCTL_HAL_GET_RANDOM_SEED passed with an output buffer of 0xffffffff"),dwI);
        goto cleanAndReturn;
    }

   //Test IOCTL should fail
    dwI = 0;
    //This scenerio is actually undefined.  It's a useless call, but it passes on the common test,
    //so I will asume it should pass here.
    if (!KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,0, 
        OutData, dwI, &dwBytesRet))
    {
        Error(_T("IOCTL_HAL_GET_RANDOM_SEED must should pass zero output buffer length"));
        goto cleanAndReturn;
    }
   
    Info(_T("Make sure IOCTL_HAL_GET_RANDOM_SEED fails with null output buffer."));
    dwI = 512;
    if (KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,0, 
        NULL, dwI, &dwBytesRet))
    {
        Error(_T("IOCTL_HAL_GET_RANDOM_SEED must fail for a null output buffers"));
        goto cleanAndReturn;
    }

    dwI = 0;
    if (KernelIoControl (IOCTL_HAL_GET_RANDOM_SEED, NULL,0, 
        NULL, dwI, &dwBytesRet))
    {
        Error(_T("IOCTL_HAL_GET_RANDOM_SEED must fail for a null output buffer and zero size"));
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;
    Info(_T("testRandomSeedIoctlOutParam passed."));

cleanAndReturn:
    Info (_T(""));
    return (returnVal);
}


