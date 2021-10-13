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

//*************This file contains code common to all IOCTLs ******************//

////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

#include "commonIoctlTests.h"


////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

/*******************************************************************************
 *    Inbound parameter checks
 ******************************************************************************/
ioctlParamCheck ioctlInParamTests[] = {

    { BUF_NULL, SIZE_ZERO, BUF_CORRECT,  SIZE_CORRECT, TRUE, FALSE, 
      ERROR_INVALID_PARAMETER, 
      _T("Inbound buffer is NULL and inbound buffer size is zero.") },   

    { BUF_NULL, SIZE_CORRECT, BUF_CORRECT,  SIZE_CORRECT, TRUE, FALSE, 
      ERROR_INVALID_PARAMETER, 
      _T("Inbound buffer is NULL and inbound buffer size is size of correct input.") },
   
    { BUF_CORRECT, SIZE_ZERO, BUF_CORRECT,  SIZE_CORRECT, TRUE, FALSE, 
      ERROR_INVALID_PARAMETER, 
      _T("Inbound buffer is correct but inbound buffer size is zero.") },
                      
    { BUF_CORRECTSIZE_MINUS_ONE, SIZE_BUF, BUF_CORRECT,  SIZE_CORRECT, TRUE, FALSE, 
      ERROR_INVALID_PARAMETER, 
      _T("Inbound buffer is correct size minus one.") }, 
                          
    { BUF_CORRECTSIZE_PLUS_ONE, SIZE_BUF, BUF_CORRECT,  SIZE_CORRECT, TRUE, FALSE,
      ERROR_INVALID_PARAMETER,  
      _T("Inbound buffer is correct size plus one.") },
};

DWORD dwNumParamTests = countof(ioctlInParamTests);

/*******************************************************************************
 *    Outbound parameter checks
 ******************************************************************************/
ioctlParamCheck ioctlOutParamTests[] = {

    {BUF_CORRECT, SIZE_CORRECT, BUF_NULL,  SIZE_ZERO, TRUE, FALSE, 
    ERROR_INSUFFICIENT_BUFFER, 
    _T("Outbound buffer is NULL and oubound buffer size is zero.") },

    { BUF_CORRECT, SIZE_CORRECT, BUF_NULL,  SIZE_CORRECT, TRUE, FALSE, 
      ERROR_INVALID_PARAMETER, 
      _T("Outbound buffer is NULL and outbound buffer size is size of correct output.") },

    { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECT, SIZE_ZERO, TRUE, FALSE, 
      ERROR_INSUFFICIENT_BUFFER, 
      _T("Outbound buffer is correct but outbound buffer size is zero.") },
                          
    { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECTSIZE_MINUS_ONE, SIZE_BUF, TRUE, FALSE, 
      ERROR_INSUFFICIENT_BUFFER, 
      _T("Outbound is correct size minus one.") }, 
                          
    { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECTSIZE_PLUS_ONE,  SIZE_BUF, TRUE, TRUE, 0,  
      _T("Outbound buffer is correct size plus one.") },

    { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECT,  SIZE_CORRECT, TRUE, TRUE, 0,  
      _T("All correct outbound parameters.") },

        { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECT,  SIZE_CORRECT, FALSE, TRUE, 0,  
      _T("All correct outbound parameters but lpBytesReturned pointer is NULL.") },    
};


////////////////////////////////////////////////////////////////////////////////
/*********************** Helper Functions *************************************/
// This function runs a single parameter checking test on the IOCTL function 
BOOL CheckParametersSingleTest(DWORD dwIoctlCode, 
          ioctlParamCheck *pIoctlParamTest, VOID *pCorrectInBuf, 
          DWORD dwCorrectInSize, VOID *pCorrectOutBuf, DWORD dwCorrectOutSize);



////////////////////////////////////////////////////////////////////////////////
/****************************** Functions *************************************/

/*******************************************************************************
 *                  Check Incorrect In/Out Parameters
 ******************************************************************************/

// This function will test the Incorrect inbound and outbound parameters for the
// IOCTLs. All combinations of incorrect values are passed into the IOCTL 
// function and the return values are verified. Any incorrect input parameters 
// should not crash the system.
//
// Arguments: 
//         Input: IOCTL code, Params to be tested(Inbound/Outbound), Input Buffer,
//             Input Buffer Size
//     Output: None
//
// Return value:
//        TRUE if successful, FALSE otherwise

BOOL CheckIncorrectInOutParameters(DWORD dwIoctlCode, DWORD dwInOrOutParam, 
                                     VOID* pCorrectInBuf, DWORD dwCorrectInSize)
{
  // Assume TRUE, until proven otherwise.
  BOOL returnVal = TRUE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 

  Info (_T("")); //Blank line
  Info (_T("All combinations of incorrect values are passed into the IOCTL function"));
  Info (_T("and the return values are verified."));    
  Info (_T("Any incorrect input parameters should not crash the system."));


  ////////Check if the IOCTL is supported and get the output size /////////////
  if(!VerifyIoctlAndGetOutputSize (dwIoctlCode, pCorrectInBuf, dwCorrectInSize,
                                                          &dwCorrectOutSize))
  {
      Error (_T("Could not get the output size."));
    returnVal = FALSE;
    goto cleanAndExit;
  }
  

  ////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Calling the IOCTL with correct parameters to get the output..."));
  
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));            
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }      
   
  if (!KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, 
                               pCorrectOutBuf, dwCorrectOutSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      returnVal = FALSE;
      goto cleanAndExit;
  }
  
   
  ///////////////////////////Verify the parameters ////////////////////////////
  Info (_T("")); //Blank line
  Info (_T("Iterate through all combinations of incorrect parameters."));

  DWORD dwCount;
  DWORD dwNumTests;
  ioctlParamCheck *pIoctlParamTest;

  if(dwInOrOutParam == INBOUND)
  {
      dwNumTests = countof(ioctlInParamTests);
    Info (_T("A total of %u tests will be run on the Inbound parameters."),
                                                  dwNumTests);
    Info (_T("Correct outbound parameters will be used in these tests."));
  }
  else
  {
      dwNumTests = countof(ioctlOutParamTests);
    Info (_T("A total of %u tests will be run on the Outbound parameters."),
                                                    dwNumTests);
    Info (_T("Correct inbound parameters will be used in these tests."));
  }
  

  // Iterate through all the tests in the array  
  for(dwCount=0; dwCount<dwNumTests; dwCount++)
  {
     if(dwInOrOutParam == INBOUND)
     { 
         Info (_T("")); //Blank line
         Info (_T("This is test %u"),dwCount + 1);
         
         pIoctlParamTest = &ioctlInParamTests[dwCount];
     }
     else
     {
         Info (_T("")); //Blank line
         Info (_T("This is test %u"),dwCount + 1);
         
         pIoctlParamTest = &ioctlOutParamTests[dwCount];
     }

    if(!CheckParametersSingleTest(dwIoctlCode, pIoctlParamTest, pCorrectInBuf, 
                    dwCorrectInSize, pCorrectOutBuf, dwCorrectOutSize))
    {
        Error (_T("The parameter checking test failed."));
    returnVal = FALSE;
    }
    else
    {
        Info (_T("The parameter checking test passed."));
    }
  }
    
cleanAndExit:

    if(pCorrectOutBuf)
       {
          free(pCorrectOutBuf);
       }
        
    return returnVal;
}


/*******************************************************************************
 *                       Check Parameters Single Test
 ******************************************************************************/

// This function runs a single parameter checking test on the IOCTL function 
//
// Arguments: 
//         Input: IOCTL code, pointer to the test struct, Input Buffer,
//             Input Buffer Size, Output Buffer, Output Buffer Size
//      Output: None
//
// Return value:
//        TRUE if print successful, FALSE otherwise

BOOL CheckParametersSingleTest(DWORD dwIoctlCode, 
            ioctlParamCheck *pIoctlParamTest, VOID *pCorrectInBuf,
            DWORD dwCorrectInSize, VOID *pCorrectOutBuf, DWORD dwCorrectOutSize)
{
  BOOL returnVal = TRUE;

  DWORD dwInBufSize;
  DWORD dwOutBufSize;
  DWORD dwBufSize;

  VOID *pInBuf, *pAllocateInBuf = NULL;
  VOID *pOutBuf, *pAllocateOutBuf = NULL;

  DWORD dwBytesRet = 0;
  DWORD *pdwBytesRet = NULL;
  BOOL bOutValue;

  // Check for null pointer in the input
  if((!pIoctlParamTest) || (!pCorrectOutBuf))
  {
      Error (_T("Invalid pointer in input arguments."));
    Error (_T("Cannot perform the tests."));
    return FALSE;
  }

  // Get the test description
  Info (_T("Description: %s"), pIoctlParamTest->szComment); 

  Info (_T("Set up the inbound and outbound parameters..."));
  
  // Get the input buffer
  // Allocate memory for the max size of input buffer we will use for the test (dwCorrectInSize + 1)
  if(pIoctlParamTest ->dwInBuf != BUF_NULL)
  {
      DWORD dwMaxTestInBufSize = dwCorrectInSize + 1;
      if(dwMaxTestInBufSize < dwCorrectInSize)
      {
         Error (_T("Overflow occured when calculating the Maximum Test Input Buffer Size."));
         Error (_T("The input buffer size is too large %u."), dwCorrectInSize);
         returnVal = FALSE;
         goto cleanAndExit;
      }
      
      pAllocateInBuf = (VOID*)malloc (dwMaxTestInBufSize * sizeof(BYTE));

      if(!pAllocateInBuf)
      {
         Error (_T("Could not allocate memory for the input buffer."));
         returnVal = FALSE;
         goto cleanAndExit;
      }

      if(!memcpy(pAllocateInBuf, pCorrectInBuf, dwCorrectInSize))
      {
         Error (_T("Could not copy input to the buffer."));
         returnVal = FALSE;
         goto cleanAndExit;
      }
  }

  pInBuf = pAllocateInBuf;
  
  switch (pIoctlParamTest ->dwInBuf)
  {
      case BUF_NULL:
        dwBufSize = 0;
        break;
            
    case BUF_CORRECT:
        dwBufSize = dwCorrectInSize;
        break;
            
    case BUF_CORRECTSIZE_MINUS_ONE:
        dwBufSize = dwCorrectInSize - 1;
        break;
        
    case BUF_CORRECTSIZE_PLUS_ONE:
        dwBufSize = dwCorrectInSize + 1;
        break;
  }

  // Get the input size
  if(pIoctlParamTest -> dwInBufSize == SIZE_CORRECT)
  {
    dwInBufSize = dwCorrectInSize;
  }
  else if (pIoctlParamTest -> dwInBufSize == SIZE_BUF)
  {
      dwInBufSize = dwBufSize;
  }
  else
  {
    dwInBufSize = 0;
  }
      
  // Get the output buffer
  // Allocate memory for the max size of output buffer we will use for the test (dwCorrectOutsize + 1)
  if(pIoctlParamTest ->dwOutBuf != BUF_NULL)
  {
      DWORD dwMaxTestOutBufSize = dwCorrectOutSize + 1;
      if(dwMaxTestOutBufSize < dwCorrectOutSize)
      {
         Error (_T("Overflow occured when calculating the Maximum Test Output Buffer Size."));
         Error (_T("The output buffer size required is too large %u."), dwCorrectOutSize);
         returnVal = FALSE;
         goto cleanAndExit;
      }
      
      pAllocateOutBuf = (VOID*)malloc (dwMaxTestOutBufSize * sizeof(BYTE));
      
      if(!pAllocateOutBuf)
      {
         Error (_T("Could not allocate memory for the output buffer."));
         returnVal = FALSE;
         goto cleanAndExit;
      }
  }
  
  pOutBuf = pAllocateOutBuf;

  switch (pIoctlParamTest ->dwOutBuf)
  {
      case BUF_NULL:
        dwBufSize = 0;
        break;
            
    case BUF_CORRECT:
        dwBufSize = dwCorrectOutSize;
        break;
            
    case BUF_CORRECTSIZE_MINUS_ONE:
        dwBufSize = dwCorrectOutSize -1;
        break;
        
    case BUF_CORRECTSIZE_PLUS_ONE:
        dwBufSize = dwCorrectOutSize + 1;
        break;
  }
  
  // Get the output size
  if(pIoctlParamTest -> dwOutBufSize == SIZE_CORRECT)
  {
    dwOutBufSize = dwCorrectOutSize;
  }
  else if (pIoctlParamTest -> dwOutBufSize == SIZE_BUF)
  {
      dwOutBufSize = dwBufSize;
  }
  else
  {
    dwOutBufSize = 0;
  }

  // Get the output buffer size return pointer
  if (pIoctlParamTest->bBytesRet)
  {
     pdwBytesRet = &dwBytesRet;
  }
  else
  {
     pdwBytesRet = NULL;
  }    
    
  //Call IOCTL function now
  Info (_T("Call the IOCTL using KernelIoControl function now..."));
  bOutValue = KernelIoControl (dwIoctlCode, pInBuf, dwInBufSize, pOutBuf, 
                                                 dwOutBufSize, pdwBytesRet);    

       
  //Check returned value
  Info (_T("Check the returned value...")); 
  if(bOutValue != pIoctlParamTest->bOutput)
  {
     if(bOutValue)
     {
        Error (_T("The IOCTL returned TRUE while the expected value is FALSE."));
    returnVal = FALSE;
     }
     else
     {
    Error (_T("The IOCTL returned FALSE while the expected value is TRUE."));
    returnVal = FALSE;
     }               
  }    

  //Check GetLastError
  if(pIoctlParamTest->dwErrorCode)
  { 
    Info (_T("Check GetLastError..."));

    DWORD dwGetLastError = GetLastError();
    Info (_T("GetLastError is %u"),dwGetLastError);

    if(pIoctlParamTest->dwErrorCode != dwGetLastError)
    {
       Error (_T("The error code returned is %u while the expected value is %u."), 
                                   dwGetLastError, pIoctlParamTest->dwErrorCode);
       returnVal = FALSE;
    }    
  
    //Check if the returned size is correct if checking outbound parameters
    if((dwGetLastError == ERROR_INSUFFICIENT_BUFFER) && (pdwBytesRet))                                       
    {
       if((*pdwBytesRet != dwCorrectOutSize))
       {
          Error (_T("The output size returned is incorrect. The correct size is %u"),
                                                                dwCorrectOutSize);
          Error (_T("while the size returned is %u."), *pdwBytesRet);
          returnVal = FALSE;
       }
    }      
  }

  // If all correct parameters are passed in with dwOutBufSize >= correct size,
  // (Ioctl should return true in these cases) and if lpBytesReturned is valid, then 
  // lpBytesReturned should return the number of bytes in the buffer that were
  // actually filled upon completion.
  if(bOutValue && (pdwBytesRet))
  {
     if((*pdwBytesRet != dwCorrectOutSize))
     {
        Error (_T("The Ioctl returned TRUE, but did not fill in the correct "));
        Error (_T("output size in lpBytesReturned. The correct size expected is %u"),
                                dwCorrectOutSize);
    Error (_T("while the size returned is %u."),*pdwBytesRet);
          
        returnVal = FALSE;
     }
  }

cleanAndExit:
    
       if(pAllocateInBuf)
       {
          free(pAllocateInBuf);
       }

       if(pAllocateOutBuf)
       {
          free(pAllocateOutBuf);
       }
        
  return returnVal;
}  


/*******************************************************************************
 *                 Check If Inbound Parameters Ignored
 ******************************************************************************/

// This function will test random input values for IOCTLs that don't require any
// input parameters.
//
// Arguments: 
//         Input: IOCTL code
//      Output: None
//
// Return value:
//        TRUE if successful, FALSE otherwise

BOOL CheckIfInboundParametersIgnored(DWORD dwIoctlCode)
{
  // Assume TRUE, until proven otherwise.
  BOOL returnVal = TRUE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 

  Info (_T("")); //Blank line
  Info (_T("This is for IOCTLs that don't require any inbound parameters."));
  Info (_T("The IOCTL is called with random inbound parameters and it is"));    
  Info (_T("verified that they do not affect the outcome."));


  ///////////Check if the IOCTL is supported and get the output size //////////
  if(!VerifyIoctlAndGetOutputSize (dwIoctlCode, NULL, 0, &dwCorrectOutSize))
  {
      Error (_T("Could not get the output size."));
    returnVal = FALSE;
    goto cleanAndExit;
  }
  
  ////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Calling the IOCTL with correct parameters to get the output..."));
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));            
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }      
   
  if (!KernelIoControl (dwIoctlCode, NULL, 0, pCorrectOutBuf, dwCorrectOutSize,
                                                                 &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      returnVal = FALSE;
      goto cleanAndExit;
  }

   //////////Call the IOCTL function with random inputs ///////////////////////
   Info (_T(""));
   Info (_T("Now call the IOCTL function with random input values..."));
   
   VOID *pOutBuf;
   DWORD dwBytesRet2;

   pOutBuf = (VOID*)malloc(dwCorrectOutSize);
   if(!pOutBuf)
   {
       Error (_T("Error allocating memory for the output buffer."));
    goto cleanAndExit;
   }


   Info (_T(""));
   Info (_T("Call with random DWORD input value."));
   DWORD dwInput = 5;
   if(!KernelIoControl(dwIoctlCode, (VOID*)&dwInput, sizeof(dwInput), pOutBuf, 
                                                  dwCorrectOutSize, &dwBytesRet2))
   {
    Error (_T("The IOCTL function returned FALSE when called with a DWORD ")
           _T("input of value %u."), dwInput);
    Error (_T("The function is expected to ignore the input and work normally")
           _T("returning TRUE."));
    returnVal = FALSE;
   }
   
   Info (_T("Verify the returned output value and the output size."));
   if(dwBytesRet2 != dwBytesRet)
   {
       Error (_T("The returned output size is incorrect."));
    goto cleanAndExit;
   }
   if(memcmp(pCorrectOutBuf, pOutBuf, dwCorrectOutSize))
   {
       Error (_T("The returned output value is incorrect."));
    goto cleanAndExit;
   }

   
   Info (_T(""));
   Info (_T("Call with random TCHAR input value."));
   TCHAR tcInput = 'a';
   if(!KernelIoControl(dwIoctlCode, (VOID*)&tcInput, sizeof(tcInput), pOutBuf, 
                                                dwCorrectOutSize, &dwBytesRet2))
   {
    Error (_T("The Device ID IOCTL returned FALSE when called with a TCHAR ")
           _T("input of value %s."), tcInput);
    Error (_T("The function is expected to ignore the input and work normally")
           _T("returning TRUE."));
    returnVal = FALSE;
   }
   Info (_T("Verify the returned output value and the output size."));
   if(dwBytesRet2 != dwBytesRet)
   {
       Error (_T("The returned output size is incorrect."));
    goto cleanAndExit;
   }
   if(memcmp(pCorrectOutBuf, pOutBuf, dwCorrectOutSize))
   {
       Error (_T("The returned output value is incorrect."));
    goto cleanAndExit;
   }
   
   
   Info (_T(""));
   Info (_T("Call with random CHAR input value."));
   CHAR chInput;
   if(!KernelIoControl(dwIoctlCode, (VOID*)&chInput, sizeof(chInput), pOutBuf, 
                                                dwCorrectOutSize, &dwBytesRet2))
   {
    Error (_T("The Device ID IOCTL returned FALSE when called with a DWORD ")
           _T("input of value %s."), chInput);
    Error (_T("The function is expected to ignore the input and work normally")
           _T("returning TRUE."));
    returnVal = FALSE;
   }
   Info (_T("Verify the returned output value and the output size."));
   if(dwBytesRet2 != dwBytesRet)
   {
       Error (_T("The returned output size is incorrect."));
    goto cleanAndExit;
   }
   if(memcmp(pCorrectOutBuf, pOutBuf, dwCorrectOutSize))
   {
       Error (_T("The returned output value is incorrect."));
    goto cleanAndExit;
   }
   
   Info (_T(""));
   Info (_T("Done checking invalid inbound parameters."));

    
cleanAndExit:

    if(pCorrectOutBuf)
    {
        free(pCorrectOutBuf);
    }

    if(pOutBuf)
    {
        free(pOutBuf);
    }
        
    return returnVal;
}



/*******************************************************************************
 *                    Check Inbound Buffer Alignment
 ******************************************************************************/

// This function verifies all the different alignments of the input buffer.
// The function also checks for input buffer overflows on each call.
// Arguments: 
//         Input: IOCTL code, Input buffer, Input Buffer Size
//      Output: None
//
// Return value:
//        TRUE if successful, FALSE otherwise

BOOL CheckInboundBufferAlignment(DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                                                         DWORD dwCorrectInSize)
{
  // Assume TRUE, until proven everything works.
  BOOL returnVal = TRUE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 
   
  Info (_T("")); //Blank line
  Info (_T("The inbound buffer is aligned on a DWORD offset of 0, 1, 2 and 3."));
  Info (_T("The IOCTL function should return FALSE on all alignments except"));
  Info (_T("for offset 0, which is DWORD aligned. The function should not"));
  Info (_T("throw an exception or crash. Buffer overflow check is performed"));
  Info (_T("on each call."));

  // Check for null pointer in the input
  if(!pCorrectInBuf)
  {
      Error (_T("NULL pointer supplied as the input buffer argument."));
    Error (_T("Cannot get the input buffer."));
    return FALSE;
  }
  
  ////////////Check if the IOCTL is supported and get the output size /////////
  if(!VerifyIoctlAndGetOutputSize (dwIoctlCode, pCorrectInBuf, dwCorrectInSize,
                                                          &dwCorrectOutSize))
  {
      Error (_T("Could not get the output size."));
    returnVal = FALSE;
    goto cleanAndExit;
  }

  ////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Calling the IOCTL with correct parameter to get the output..."));
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));            
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }   
   
  if (!KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, 
                                pCorrectOutBuf, dwCorrectOutSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));  
      returnVal = FALSE;
      goto cleanAndExit;
  }
   
  //Check input buffer alignment
  Info (_T(""));
  Info (_T("Check inbound buffer alignment."));

  BYTE alignarr[4];
  
  VOID *pInBuf;
  DWORD dwAlign;
  VOID *pOutBuf;
  
  pOutBuf = (VOID*)malloc(dwCorrectOutSize);            
  if (!pOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }   
  
  for (dwAlign = 0; dwAlign < 4; dwAlign++)
  {
    Info (_T(""));
    Info (_T("Allocate memory for the input buffer at offset %u."), dwAlign);
    pInBuf = (VOID*)guardedMalloc(dwCorrectInSize, alignarr + dwAlign);
    if(!pInBuf)
    {
        Error (_T("Failed to allocate memory for inbound buffer misaligned")
          _T("by %u bytes."), dwAlign);
    returnVal = FALSE;
    goto cleanAndExit;
    }

    Info (_T("Copy the input value at the given offset."));
    if(!memcpy(pInBuf, pCorrectInBuf, dwCorrectInSize))
    {
       Error (_T("Failed to copy input at offset %u."), dwAlign);
       returnVal = FALSE;
    }
       
    Info (_T("Call the IOCTL using KernelIoControl function now..."));
    BOOL bRet = KernelIoControl (dwIoctlCode, pInBuf, dwCorrectInSize, pOutBuf, 
                                            dwCorrectOutSize, &dwBytesRet);
    
    Info (_T("Check the returned value."));
    if(dwAlign) // Alignment is 1, 2 or 3
    {
      if (bRet)
      {
          Error (_T("")); //Blank line
          Error (_T("Called the IOCTL function with inbound parameter ")
           _T("misaligned by %u bytes."), dwAlign);
          Error (_T("The function returned TRUE, while the expected value ")
           _T("is FALSE."));  
          returnVal = FALSE;
      }
    }
    else
    {
       if(!bRet)  // No misalignment here, should work correctly
       {
          Error (_T("")); //Blank line
          Error (_T("Called the IOCTL function with no misalignment in the")
             _T("inbound parameter."));
            Error (_T("The function returned FALSE, while the expected value ")
             _T("is TRUE."));  
            returnVal = FALSE;
       }
    }

    Info (_T("Verify that the inbound buffer has not overflowed."));
    if(!guardedCheck(pInBuf,dwCorrectInSize))
    {
       Error (_T("Inbound buffer with a misalignment of %u bytes overflowed")
          _T("when called the IOCTL function."), dwAlign);
       returnVal = FALSE;
    }

    if(pInBuf)
    {
      guardedFree(pInBuf, dwCorrectInSize);
    }
  }

  Info (_T(""));
  Info (_T("Done checking inbound buffer alignment and buffer overflow."));
  
cleanAndExit:

    if(pCorrectOutBuf)
    {
       free(pCorrectOutBuf);
    }
    
    if(pOutBuf)
    {
       free(pOutBuf);
    }
    
    return returnVal;
}


/*******************************************************************************
 *                      Check Outbound Buffer Alignment
 ******************************************************************************/

// This function verifies all the different alignments of the output buffer.
// The function also checks for output buffer overflows on each call.
//
// Arguments: 
//         Input: IOCTL code, Input buffer, Input Buffer Size
//      Output: None
//
// Return value:
//        TRUE if successful, FALSE otherwise

BOOL CheckOutboundBufferAlignment(DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                                                         DWORD dwCorrectInSize)
{
  // Assume TRUE, until proven everything works.
  BOOL returnVal = TRUE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 
   
  Info (_T("")); //Blank line
  Info (_T("The outbound buffer is aligned on a DWORD offset of 0, 1, 2 and 3."));
  Info (_T("The IOCTL function should return TRUE and work correctly on all"));
  Info (_T("alignments. It should not throw an exception or crash. Buffer"));
  Info (_T("overflow check is performed on each call."));


  ////////////Check if the IOCTL is supported and get the output size /////////
  if(!VerifyIoctlAndGetOutputSize (dwIoctlCode, pCorrectInBuf, dwCorrectInSize,
                                                          &dwCorrectOutSize))
  {
      Error (_T("Could not get the output size."));
    returnVal = FALSE;
    goto cleanAndExit;
  }

  ////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Calling the IOCTL with correct parameters to get the output..."));
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));            
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }   
   
  if (!KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, 
                                  pCorrectOutBuf, dwCorrectOutSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));  
      returnVal = FALSE;
      goto cleanAndExit;
  }
   
  //Check output buffer alignment
  Info (_T(""));
  Info (_T("Check outbound buffer alignment."));  

  BYTE *pOutBuf;
  DWORD dwAlign;
  
  for (dwAlign = 0; dwAlign < 4; dwAlign++)
  {
    Info (_T(""));
    Info (_T("Allocate memory for the output buffer at offset %u."), dwAlign);
    pOutBuf = (BYTE*)guardedMalloc(dwCorrectOutSize, &dwAlign);
    if(!pOutBuf)
    {
      Error (_T("Failed to allocate memory for outbound buffer misaligned")
               _T("by %u bytes."), dwAlign);
    returnVal = FALSE;
    goto cleanAndExit;
    }

    Info (_T("Call the IOCTL using KernelIoControl function now..."));
    Info (_T("Check the returned value."));
    if (!KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, pOutBuf, 
                                            dwCorrectOutSize, &dwBytesRet))
    {
          Error (_T("")); //Blank line
          Error (_T("Called the IOCTL function with outbound parameter ")
           _T("misaligned by %u bytes."), dwAlign);
          Error (_T("The function returned FALSE, while the expected value ")
           _T("is TRUE."));  
          returnVal = FALSE;
    }

    Info (_T("Check the ouput buffer size returned."));
    if(dwBytesRet != dwCorrectOutSize)
    {
    Error (_T("The returned output size is not equal to the correct ")
           _T("output size."));
    returnVal = FALSE;
    }
    
    Info (_T("Check the output value."));
    if(memcmp(pOutBuf, pCorrectOutBuf, dwCorrectOutSize))
    {
    Error (_T("The output value returned does not match the previous")
           _T("output value."));
    returnVal = FALSE;
    }
    
    Info (_T("Verify that the outbound buffer has not overflowed."));
    if(!guardedCheck(pOutBuf,dwCorrectOutSize))
    {
      Error (_T("Outbound buffer overflowed with a misalignment of ")
           _T("%u bytes."), dwAlign);
    returnVal = FALSE;
    }

    if(pOutBuf)
    {
      guardedFree(pOutBuf, dwCorrectOutSize);
    }
  }

  Info (_T(""));
  Info (_T("Done checking outbound buffer alignment and buffer overflow."));
  
cleanAndExit:

    if(pCorrectOutBuf)
    {
       free(pCorrectOutBuf);
    }
    
    return returnVal;
}


/*******************************************************************************
 *                    Verify IOCTL and Get Output Size
 ******************************************************************************/

// This function verifies if the IOCTL is supported by the platform.
// It then retrives the required output size for the IOCTL.
// This works in case of IOCTLs that require inbound and outbound parameters and 
// for IOCTLs that require outbound parameters only, i.e, inbound are ignored.
// For IOCTLs that require inbound parameters only or for IOCTLs that require no 
// parameters, this function cannot be used.
//
// Arguments: 
//         Input: IOCTL code, Input buffer, Input Buffer Size
//      Output: Output Buffer Size
//
// Return value:
//        TRUE if successful, FALSE otherwise

BOOL VerifyIoctlAndGetOutputSize (DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                              DWORD dwCorrectInSize, DWORD *pdwCorrectOutSize)
{
  // Assume FALSE, until proven everything works.
  BOOL returnVal = FALSE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize = 0;
   
  DWORD dwBytesRet = 0;  
  BOOL bOutValue;

  DWORD dwErrorCode;  

  Info (_T(""));
  Info (_T("First verify if the IOCTL is supported by the platform and then"));
  Info (_T("get the required output size."));  

  // Check for null pointer in the input. 
  // pCorrectInBuf can be null if no input data is required by the Ioctl
  if(!pdwCorrectOutSize)
  {
      IntErr (_T("NULL pointer supplied as the output size argument."));
    IntErr (_T("Cannot return the output size."));
    return FALSE;
  }
  
  //////////////////////////////STEP 1 ////////////////////////////////////////
  Info (_T(""));
  Info (_T("Call the IOCTL using KernelIoControl function with output buffer "));
  Info (_T("NULL and buffer size zero..."));
  Info (_T(""));
  
  bOutValue = KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, NULL, 0, &dwBytesRet);
  dwErrorCode = GetLastError();
  

  // Check if it is not supported
  if( (!bOutValue) && (dwErrorCode == ERROR_NOT_SUPPORTED))
  {
    Error (_T("This IOCTL call is not supported on this platform."));
    Error (_T("Calling it has set this error code: ERROR_NOT_SUPPORTED.")); 
    goto cleanAndExit;
  }

  //Check if access is denied
  if( (!bOutValue) && (dwErrorCode == ERROR_ACCESS_DENIED))
  {
    Error (_T("Access to this IOCTL is being denied."));
    Error (_T("Calling it has set this error code: ERROR_ACCESS_DENIED.")); 
    goto cleanAndExit;
  }

  //Check the returned value
  if(bOutValue)
  {
     Error (_T("Called the IOCTL function with output buffer size set to zero."));
     Error (_T("The function returned TRUE, while the expected value is FALSE."));
     goto cleanAndExit;
  }
   
  // Check the bytes returned
  if(dwBytesRet == 0)
  {
      Error (_T("The IOCTL did not return the required output size."));
      goto cleanAndExit;
  }

  Info (_T("The required output buffer size(lpBytesRet) is %u"),dwBytesRet);

   
  //////////////////////////////STEP 2 /////////////////////////////////////////
  Info (_T("")); 
  Info (_T("We got the required size for the output buffer. Now set the"));
  Info (_T("output buffer and output size to the correct values and check if"));
  Info (_T("the Ioctl call returns TRUE..."));
  Info (_T("")); 
  
  dwCorrectOutSize = dwBytesRet;
      
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));            
  if (!pCorrectOutBuf)
  {
      Error (_T("Error allocating memory for the output buffer."));
      goto cleanAndExit;
  }      

  if (!KernelIoControl (dwIoctlCode, pCorrectInBuf, dwCorrectInSize, 
                              pCorrectOutBuf, dwCorrectOutSize, &dwBytesRet))
  { 
      if(GetLastError() == ERROR_NOT_SUPPORTED)
      {
        Error (_T("This IOCTL call is not implemented on this platform."));
    Error (_T("Calling it has set this error code: ERROR_NOT_SUPPORTED.")); 
    goto cleanAndExit;
      }
      Error (_T("Called the IOCTL function with all correct parameters. The "));
      Error (_T("function returned FALSE, while the expected value is TRUE."));
      Error (_T("The output size reported may not be accurate."));     
      goto cleanAndExit;
  }

  // Assign output size
  *pdwCorrectOutSize = dwCorrectOutSize;

  returnVal = TRUE;

  Info (_T("Done verifying the IOCTL."));

  cleanAndExit:
    Info (_T(""));

    if(pCorrectOutBuf)
    {
         free (pCorrectOutBuf);
    }
      
    return returnVal;    
}
  


