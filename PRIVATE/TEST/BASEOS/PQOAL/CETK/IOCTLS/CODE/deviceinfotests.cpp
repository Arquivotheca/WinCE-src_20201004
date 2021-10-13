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


// This file contains tests related to the following IOCTLs. Any tests that 
// need special cased test code will be in this file.
// IOCTL_HAL_GET_DEVICE_INFO
// IOCTL_HAL_GET_DEVICEID
// IOCTL_HAL_GET_UUID
// IOCTL_PROCESSOR_INFORMATION


////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

#include "deviceInfoTests.h"

////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

/*******************************************************************************
 *    Outbound parameter checks for Device ID
 ******************************************************************************/
// The error codes set by ioctl IOCTL_HAL_GET_DEVICEID are not always
// consistent with the rest of the Ioctls. But since the DeviceID IOCTL
// is going to be deprecated, no further changes will be made to it.
// We have separate testing for it.

ioctlParamCheck deviceIDOutParamTests[] = {

	{BUF_CORRECT, SIZE_CORRECT, BUF_NULL,  SIZE_ZERO, TRUE, FALSE, 
	 ERROR_INVALID_PARAMETER, 
	 _T("Outbound buffer is NULL and oubound buffer size is zero.") },

	{ BUF_CORRECT, SIZE_CORRECT, BUF_NULL,  SIZE_CORRECT, TRUE, FALSE, 
	  ERROR_INVALID_PARAMETER, 
      _T("Outbound buffer is NULL and outbound buffer size is size of correct output.") },

	{ BUF_CORRECT, SIZE_CORRECT, BUF_CORRECT, SIZE_ZERO, TRUE, FALSE, 
	  ERROR_INVALID_PARAMETER, 
      _T("Outbound buffer is correct but outbound buffer size is zero.") },
	                      
	{ BUF_CORRECT, SIZE_CORRECT, BUF_CORRECTSIZE_MINUS_ONE, SIZE_BUF, TRUE, FALSE, 
	  ERROR_INSUFFICIENT_BUFFER, 
      _T("Outbound buffer is correct size minus one.") }, 
	                      
	{ BUF_CORRECT, SIZE_CORRECT, BUF_CORRECTSIZE_PLUS_ONE,  SIZE_BUF, TRUE, TRUE, 0,  
      _T("Outbound buffer is correct size plus one.") },

    { BUF_CORRECT, SIZE_CORRECT, BUF_CORRECT,  SIZE_CORRECT, FALSE, TRUE, 0,  
	  _T("All correct outbound parameters but BytesRet pointer set to NULL.") },    
};


////////////////////////////////////////////////////////////////////////////////
/******************************  Functions  ***********************************/

/*******************************************************************************
 *                   Print output of the DeviceInfo IOCTL
 ******************************************************************************/

// This function prints the output of the Device Info IOCTL
//
// Arguments: 
// 		Input: SPI code
//  	       Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

BOOL PrintDeviceInfoOutput(DWORD dwInputSpi)
{
  // Assume FALSE, until proven everything works.
  BOOL returnVal = FALSE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 
   
  Info (_T("")); //Blank line
  Info (_T("The output value of the Device Info IOCTL function for this SPI"));
  Info (_T("will be printed and it should be visually verified by the tester."));

  breakIfUserDesires();

    
  ////////////Check if the IOCTL is supported and get the output size ///////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi, 
  	                                 sizeof(dwInputSpi), &dwCorrectOutSize))
  {
  	Error (_T("Could not get the output size."));
	goto cleanAndExit;
  }

  
  //////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Call the IOCTL with correct parameters to get the output..."));
  
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));      	  
  if (!pCorrectOutBuf)
  {
      Error (_T(""));
      Error (_T("Error allocating memory for the output buffer."));
      goto cleanAndExit;
  }  
   
  if (!KernelIoControl (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi, 
   	        sizeof(dwInputSpi), pCorrectOutBuf, dwCorrectOutSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      goto cleanAndExit;
  }

  //Print out the output and verify its value
  Info (_T("Print the output...")); 
  Info (_T("")); //Blank line

  Info (_T("/*************************************************************/"));
  if((dwInputSpi == SPI_GETPLATFORMNAME) 
  	                    || (dwInputSpi == SPI_GETPROJECTNAME)
		            || (dwInputSpi == SPI_GETPLATFORMTYPE) 
 	                    || (dwInputSpi == SPI_GETBOOTMENAME) 
 	                    || (dwInputSpi == SPI_GETOEMINFO))
  {
     TCHAR *pszDeviceInfo;
 
     pszDeviceInfo = (TCHAR*)pCorrectOutBuf;

     pszDeviceInfo[(dwCorrectOutSize/sizeof(TCHAR)) - 1] = L'\0';

     Info (_T("The output value is %s"),pszDeviceInfo);
  }
  else if((dwInputSpi == SPI_GETPLATFORMVERSION))
  {
     // This ouput is an array of PLATFORMVERSION structs.
     // Both CEBase and Windows Mobile have only one struct 
     // {{CE_MAJOR_VER, CE_MINOR_VER}} in Yamazaki.

     PLATFORMVERSION *pPlatVer;
     pPlatVer = (PLATFORMVERSION*) pCorrectOutBuf;	

     if(dwCorrectOutSize == 8)
     {
       Info (_T("The platform version is:"));
       Info (_T("{{CE_MAJOR_VER, CE_MINOR_VER}} = {{%u,%u}}"), pPlatVer->dwMajor, pPlatVer->dwMinor);
     }
     else
     // Previously PPC had two PLATFORMVERSION structs for Platform Version. 
     // But this is deprecated in Yamazaki. 
     // It should now be the same as above, i.e., one struct.
     {
	Error (_T("The size of the platform version is reported as %u."),dwCorrectOutSize); 
	Error (_T("The expected value is 8 which is the size of one"));
	Error (_T("PLATFORMVERSION struct whose value is supposed to be:"));
	Error (_T("{{CE_MAJOR_VER, CE_MINOR_VER}}."));
	goto cleanAndExit;
     }	
  }
  else  // If UUID, GUID Pattern
  {
     if(dwCorrectOutSize == sizeof(GUID))
     {
         GUID *pGuidDevInfo;
         pGuidDevInfo = (GUID*)pCorrectOutBuf;
	   
         Info (_T("The output value is: "));
	     
         Info (_T("{0x%x, 0x%x, 0x%x, {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}}"),
   	     pGuidDevInfo->Data1, pGuidDevInfo->Data2, pGuidDevInfo->Data3, 
  	     pGuidDevInfo->Data4[0], pGuidDevInfo->Data4[1], pGuidDevInfo->Data4[2],
             pGuidDevInfo->Data4[3], pGuidDevInfo->Data4[4], pGuidDevInfo->Data4[5],
             pGuidDevInfo->Data4[6], pGuidDevInfo->Data4[7]); 
     }
     else
     {
	    Error (_T("The output is not the size of a GUID value, which is %u bytes."), sizeof(GUID));
	    Error (_T("Instead the output we got is of size %u bytes."), dwCorrectOutSize);
     }
  }
  Info (_T("/*************************************************************/"));

  Info (_T("")); //Blank line
  Info (_T("Please verify that the above value is as expected."));
  Info (_T(""));

  returnVal = TRUE;

cleanAndExit:
	if(pCorrectOutBuf)
       {
  	    free(pCorrectOutBuf);
       }
	return returnVal;
}



/*******************************************************************************
 *              Check Invalid Inputs for Device Info IOCTL
 ******************************************************************************/

// This function sets all invalid inbound parameters from zero to hundred.
//
// Arguments: 
// 		Input: None
//  	       Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

// Set all values from zero to hundred except valid SPI values.
BOOL CheckDeviceInfoInvalidInputs(DWORD dwInputSpi)
{
  BOOL returnVal = TRUE;

  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  
  Info (_T("")); //Blank line
  Info (_T("Set the input to invalid SPI values and verify the return value"));
  Info (_T("in each case. Test all values from 1 to 100 except valid"));
  Info (_T("SPI inputs for the Device Info IOCTL."));  
    
  ////////////Check if the IOCTL is supported and get the output size /////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi, 
  	                                  sizeof(dwInputSpi), &dwCorrectOutSize))
  {
  	Error (_T("Could not get the output size."));
	goto cleanAndExit;
  }

  //////////Check all invalid inputs ////////////////////////////
  DWORD dwValidSpiArr[] = {SPI_GETPLATFORMNAME, SPI_GETPROJECTNAME, 
                           SPI_GETPLATFORMTYPE, SPI_GETBOOTMENAME, SPI_GETOEMINFO,
		           SPI_GETPLATFORMVERSION, SPI_GETUUID, SPI_GETGUIDPATTERN};
  DWORD dwInvalidSpiArr[100];
  DWORD dwSpi;

  // Set the output buffer
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));      	  
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      goto cleanAndExit;
  }  

  for(DWORD i = 0; i < 100; i++)
  {
      dwInvalidSpiArr[i] = i;
  }
	
  for(DWORD i = 0; i < 9; i++)
  {
    if(dwValidSpiArr[i] < 100)
    {
	dwInvalidSpiArr[dwValidSpiArr[i]] = 0;
    }
  }
	
  Info (_T("Call the IOCTL with each of the invalid input values between"));
  Info (_T("one and hundred..."));
  Info (_T("All calls should return false."));
  Info (_T(""));
  
  for(DWORD i = 0; i < 100; i++)
  {
     dwSpi = dwInvalidSpiArr[i];
		
     if(KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, sizeof(dwSpi),
	                                pCorrectOutBuf, dwCorrectOutSize, NULL))
     {
	Error (_T("The function returned TRUE with an invalid input of %u."),dwSpi);
	returnVal = FALSE;
     }
  }

  Info (_T("Done checking for invalid inputs."));

cleanAndExit:

	if(pCorrectOutBuf)
       {
  	    free(pCorrectOutBuf);
       }
		
  return returnVal;
}


/*******************************************************************************
 *                     Print output of the DeviceID IOCTL
 ******************************************************************************/

// This function prints the output of the Device ID IOCTL
//
// Arguments: 
// 		Input: None
//  	       Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

BOOL PrintDeviceIDOutput(VOID)
{
  // Assume FALSE, until proven everything works.
  BOOL returnVal = FALSE;
 
  DEVICE_ID *pszDeviceID = NULL;
  DWORD dwDeviceIDStructSize;
  DWORD dwBytesRet;
   
  Info (_T("")); //Blank line
  Info (_T("The output value of the Device ID IOCTL will be printed and it"));
  Info (_T("should be visually verified by the tester."));    
    
  ////////////Check if the IOCTL is supported and get the output size ///////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
  	                                                 &dwDeviceIDStructSize))
  {
     Error (_T("Could not get the output size."));
     goto cleanAndExit;
  }

  //////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Call the IOCTL with correct parameters to get the output..."));
  pszDeviceID = (DEVICE_ID*)malloc(dwDeviceIDStructSize);      	  
  if (!pszDeviceID)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      goto cleanAndExit;
  }  

  // Set the dwSize in the DEVICE_ID struct so that we don't do another query
  pszDeviceID->dwSize = dwDeviceIDStructSize;
   
  if (!KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, pszDeviceID, 
   	                                        dwDeviceIDStructSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      goto cleanAndExit;
  }

  // Confirm that we didn't walk off the end of the structure; 
  // causes a memory access violation
  DWORD dwMemAccessSize;

  if (DWordAdd (pszDeviceID->dwPresetIDOffset, pszDeviceID->dwPresetIDBytes, 
		&dwMemAccessSize) != S_OK)
  {
      Error (_T("Overflow calculating the last byte to be accessed for PresetID."));
      goto cleanAndExit;
  }
  if (dwMemAccessSize > dwDeviceIDStructSize)
  { 
      Error (_T("Preset ID extends beyond the memory allocated for it. ")
	     _T("Accessing it risks a memory access violation."));
      goto cleanAndExit;
  }	  

  if (DWordAdd (pszDeviceID->dwPlatformIDOffset, pszDeviceID->dwPlatformIDBytes, 
		&dwMemAccessSize) != S_OK)
  {
      Error (_T("Overflow calculating the last byte to be accessed for PlatformID."));
      goto cleanAndExit;
  }
  if (dwMemAccessSize > dwDeviceIDStructSize)
  { 
      Error (_T("Platform ID extends beyond the memory allocated for it. ")
	     _T("Accessing it risks a memory access violation."));
      goto cleanAndExit;
  }

  // Get the PresetID and PlatformID from the structure
  char *pszPresetID;
  char *pszPlatformID;
  DWORD dwPresetIDSize, dwPlatformIDSize;

  pszPresetID = ((char*)pszDeviceID) + pszDeviceID->dwPresetIDOffset;
  dwPresetIDSize = pszDeviceID->dwPresetIDBytes;

  if(dwPresetIDSize == 0)
  {
    Error (_T(""));
    Error (_T("No preset identifier is available for this platform."));
    goto cleanAndExit;
  }

  pszPlatformID = ((char*)pszDeviceID) + pszDeviceID->dwPlatformIDOffset;
  dwPlatformIDSize = pszDeviceID->dwPlatformIDBytes;

  if(dwPlatformIDSize == 0)
  {
    Error (_T(""));
    Error (_T("No platform identifier is available for this platform."));
    goto cleanAndExit;
  }

  //Print out the output and verify its value
  Info (_T("Print the output...")); 
  Info (_T("")); //Blank line

  Info (_T("/*************************************************************/"));

      char *pszDevPresetID;
      pszDevPresetID = pszPresetID;
      pszDevPresetID[dwPresetIDSize - 1] = '\0';

      Info (_T("The Preset ID value is %s"),pszDevPresetID);

      char *pszDevPlatformID;
      pszDevPlatformID = pszPlatformID;
      pszDevPlatformID[dwPlatformIDSize - 1] = '\0';

      // We need to convert the multibyte string to unicode.  
      DWORD dwLen = 256;
      TCHAR szDeviceName[256];

      if (mbstowcs(NULL, pszDevPlatformID, dwLen) > (size_t) dwLen)
      {
          Error (_T("Not enough room in device name."));
          goto cleanAndExit;
      }

      if (mbstowcs(szDeviceName, pszDevPlatformID, dwLen) < 0)
      {
          Error (_T("Could not convert to unicode."));
          goto cleanAndExit;
      }

      /* force null term after string conversion */
      szDeviceName [dwLen - 1] = 0;

      Info (_T("The Platform ID value is %s"),szDeviceName);
	  
  Info (_T("/*************************************************************/"));
  
  Info (_T("")); //Blank line
  Info (_T("Please verify that the above values are as expected."));


  returnVal = TRUE;

cleanAndExit:

	if(pszDeviceID)
       {
  	free(pszDeviceID);
       }

	return returnVal;
}

/*******************************************************************************
 *          Check Incorrect Out Parameters for Device ID IOCTL
 ******************************************************************************/

// This function will test the Incorrect outbound parameters for the
// DeviceID IOCTL. All combinations of incorrect values are passed into the
// IOCTL function and the return values are verified. Any incorrect parameters 
// should not crash the system.
//
// Arguments: 
// 		Input: None
//     Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

BOOL DevIDCheckIncorrectOutParameters(VOID)
{
  // Assume TRUE, until proven otherwise.
  BOOL returnVal = TRUE;
 
  VOID *pCorrectOutBuf = NULL;
  DWORD dwCorrectOutSize;
  DWORD dwBytesRet; 

  Info (_T("")); //Blank line
  Info (_T("All combinations of incorrect values are passed into the IOCTL ")
  	    _T("function and the return values are verified."));    
  Info (_T("Any incorrect parameters should not crash the system."));

  ////////////Check if the IOCTL is supported and get the output size //////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
  	                                                   &dwCorrectOutSize))
  {
  	Error (_T("Could not get the output size."));
	returnVal = FALSE;
	goto cleanAndExit;
  }

  /////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Calling the IOCTL with correct parameters to get the output..."));
  
  pCorrectOutBuf = (VOID*)malloc(dwCorrectOutSize * sizeof(BYTE));      	  
  if (!pCorrectOutBuf)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }      
   
  if (!KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, pCorrectOutBuf,
   	                                     dwCorrectOutSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      returnVal = FALSE;
      goto cleanAndExit;
  }  
   
  ////////////////////////////Now verify the parameters ////////////////////////
  Info (_T("")); //Blank line
  Info (_T("Iterate through all combinations of incorrect parameters."));

    VOID *pOutBuf, *pAllocateOutBuf = NULL;
    DWORD dwOutBufSize;
    DWORD dwBufSize;
	
    BOOL bOutValue;
    DWORD *pdwBytesRet;

  Info (_T("A total of %u tests will be run on the Outbound parameters."),
				                countof(deviceIDOutParamTests));
  Info (_T("Correct inbound parameters will be used in these tests."));
  
  // Allocate memory for the max size of output buffer we will use for the test (dwCorrectOutsize + 1)
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
  
  // Iterate through all the tests in the array 
  for(DWORD dwCount=0; dwCount<countof(deviceIDOutParamTests); dwCount++)
  {
    Info (_T("")); //Blank line
    Info (_T("This is test %u"),dwCount + 1);
		 
    // Get the test description
    Info (_T("Description: %s"), deviceIDOutParamTests[dwCount].szComment); 

    Info (_T("Set up the inbound and outbound parameters..."));

    // Get the output buffer
    switch (deviceIDOutParamTests[dwCount].dwOutBuf)
    {
  	case BUF_NULL:
		pOutBuf = NULL;
		dwBufSize = 0;
		break;
			
	case BUF_CORRECT:
              pOutBuf = pAllocateOutBuf;
		dwBufSize = dwCorrectOutSize;
		break;
			
	case BUF_CORRECTSIZE_MINUS_ONE:
              pOutBuf = pAllocateOutBuf;
		dwBufSize = dwCorrectOutSize - 1;
		break;
		
	case BUF_CORRECTSIZE_PLUS_ONE:
              pOutBuf = pAllocateOutBuf;
		dwBufSize = dwCorrectOutSize + 1;
		break;
    }
 

    // Get the output buffer size
    if(deviceIDOutParamTests[dwCount].dwOutBufSize == SIZE_CORRECT)
    {
	dwOutBufSize = dwCorrectOutSize;
    }
    else if (deviceIDOutParamTests[dwCount].dwOutBufSize == SIZE_BUF)
    {
  	dwOutBufSize = dwBufSize;
    }
    else
    {
	dwOutBufSize = 0;
    }

    // Get the output buffer size return pointer
    if (deviceIDOutParamTests[dwCount].bBytesRet)
    {
       pdwBytesRet = &dwBytesRet;
    }
    else
    {
       pdwBytesRet = NULL;
    }	
	
    //Call IOCTL function now
    Info (_T("Call the IOCTL using KernelIoControl function now..."));
    bOutValue = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, pOutBuf, 
  	                                               dwOutBufSize, pdwBytesRet);	

  	   
    //Check returned value
    Info (_T("Check the returned value...")); 
    if(bOutValue != deviceIDOutParamTests[dwCount].bOutput)
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
    if(deviceIDOutParamTests[dwCount].dwErrorCode)
    { 
      Info (_T("Check GetLastError..."));

      DWORD dwGetLastError = GetLastError();
      Info (_T("GetLastError is %u"),dwGetLastError);

      if(deviceIDOutParamTests[dwCount].dwErrorCode != dwGetLastError)
      {
        Error (_T("The error code returned is %u while the expected value is %u."), 
   	               dwGetLastError, deviceIDOutParamTests[dwCount].dwErrorCode);
	returnVal = FALSE;
      }
		
      //Check if the returned size is correct
      if(pdwBytesRet)
      {
        if(*pdwBytesRet != dwCorrectOutSize)
        {
          Error (_T("The output size returned is incorrect. The correct size is ")
           _T("%u while the size returned is %u."), dwCorrectOutSize, *pdwBytesRet);
          returnVal = FALSE;
        }
      }	 	
    }
  }
    
cleanAndExit:

       if(pCorrectOutBuf)
       {
  	    free(pCorrectOutBuf);
       }

       if(pAllocateOutBuf)
       {
  	    free(pAllocateOutBuf);
       }
		
	return returnVal;
}


/*******************************************************************************
 *                             Print output of the UUID IOCTL
 ******************************************************************************/

// This function prints the output of the Device ID IOCTL
//
// Arguments: 
// 		Input: None
//  	       Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

BOOL PrintUUIDOutput(VOID)
{
  // Assume FALSE, until proven everything works.
  BOOL returnVal = FALSE;
 
  GUID *pguidUUID = NULL;
  DWORD dwBytesRet;
   
  Info (_T("")); //Blank line
  Info (_T("The output value of the UUID IOCTL will be printed and it should"));
  Info (_T("be visually verified by the tester."));    
    
  ////////////Check if the IOCTL is supported and get the output size //////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_HAL_GET_UUID, NULL, 0, &dwBytesRet))
  {
      Error (_T("Could not get the output size."));
      goto cleanAndExit;
  }

  if(dwBytesRet != sizeof(GUID))
  {
  	Error (_T("The output size returned by the Ioctl is not equal to"));
	Error (_T("size of GUID.The Ioctl may be returning incorrect data."));
	goto cleanAndExit;
  }

  ////////////////////////// Get the output /////////////////////////////////////
  Info (_T("Call the IOCTL with correct parameters to get the output..."));
  pguidUUID = (GUID*)malloc(sizeof(GUID));      	  
  if (!pguidUUID)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      goto cleanAndExit;
  }  

  if (!KernelIoControl (IOCTL_HAL_GET_UUID, NULL, 0, pguidUUID, sizeof(GUID),
   	                                                           &dwBytesRet))
  {
      Error (_T("")); //Blank line 
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE."));     
      goto cleanAndExit;
  }

  //Print out the output and verify its value
  Info (_T("Print the output...")); 
  Info (_T("")); //Blank line

  Info (_T("/*************************************************************/"));
	   
  Info (_T("The UUID value is as follows: "));
	     
  Info (_T("{0x%x, 0x%x, 0x%x, {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}}"),
       pguidUUID->Data1, pguidUUID->Data2, pguidUUID->Data3, 
       pguidUUID->Data4[0], pguidUUID->Data4[1], pguidUUID->Data4[2], 
       pguidUUID->Data4[3], pguidUUID->Data4[4], pguidUUID->Data4[5], 
       pguidUUID->Data4[6], pguidUUID->Data4[7]); 

  Info (_T("/*************************************************************/"));

  Info (_T(""));
  Info (_T("Please verify that the above value is as expected."));
  Info (_T(""));
  
  returnVal = TRUE;

cleanAndExit:

	if(pguidUUID)
	{
		free(pguidUUID);
	}
	
	return returnVal;
}



/*******************************************************************************
 *                     Print output of the Processor Information IOCTL
 ******************************************************************************/

// This function prints the output of the Processor Information IOCTL
//
// Arguments: 
// 		Input: None
//  	       Output: None
//
// Return value:
//    	TRUE if successful, FALSE otherwise

BOOL PrintProcInfoOutput(VOID)
{
  // Assume TRUE, until proven otherwise.
  BOOL returnVal = TRUE;
 
  PPROCESSOR_INFO pszProcInfo = NULL;
  DWORD dwProcInfoStructSize;
  DWORD dwBytesRet;
   
  Info (_T("")); //Blank line
  Info (_T("The output value of the Processor Information IOCTL will be printed"));
  Info (_T("and it should be visually verified by the tester."));    
    
  ////////////Check if the IOCTL is supported and get the output size ///////////
  if(!VerifyIoctlAndGetOutputSize (IOCTL_PROCESSOR_INFORMATION, NULL, 0, 
  	                                               &dwProcInfoStructSize))
  {
  	Error (_T("Could not get the output size."));
	returnVal = FALSE;
	goto cleanAndExit;
  }

  //////////////////////////// Get the output ///////////////////////////////////
  Info (_T("Call the IOCTL with correct parameters to get the output..."));
  pszProcInfo = (PROCESSOR_INFO*)malloc(dwProcInfoStructSize);      	  
  if (!pszProcInfo)
  {
      Error (_T("")); //Blank line
      Error (_T("Error allocating memory for the output buffer."));
      returnVal = FALSE;
      goto cleanAndExit;
  }  

  if(!memset(pszProcInfo, 0xFF, dwProcInfoStructSize))
  {
     Error (_T("Error setting the memory allocated to the output buffer to 0xFF bytes"));
     returnVal = FALSE;
     goto cleanAndExit;
  }
  
  // Now call the Ioctl to get the output
  if (!KernelIoControl (IOCTL_PROCESSOR_INFORMATION, NULL, 0, pszProcInfo, 
   	                                     dwProcInfoStructSize, &dwBytesRet))
  {
      Error (_T("")); //Blank line
      Error (_T("Called the IOCTL function with all correct parameters. The"));
      Error (_T("function returned FALSE, while the expected value is TRUE.")); 
	  returnVal = FALSE;
      goto cleanAndExit;
  }


  //Print out the output and verify its value
  Info (_T("Print the output...")); 
  Info (_T("")); //Blank line

  Info (_T("/*************************************************************/"));

  // Get information about the microprocessor

  // Version - must be set
  Info (_T("")); //Blank line
  if(pszProcInfo -> wVersion == 1)
  {
     Info (_T("Version: 1"));
  }
  else if(pszProcInfo -> wVersion == 0xFFFF)
  {
     Error (_T("Version is not being set. It must be set to 1."));
     returnVal = FALSE;
  }
  else 
  {
     Error (_T("Version should be set to 1, but it is set to %u instead."), 
                                                        pszProcInfo -> wVersion);
     returnVal = FALSE;
  }

  // Name of the microprocessor core - optional
  Info (_T("")); //Blank line
  if(pszProcInfo -> szProcessCore[0] == 0)
  {
     Info (_T("Name of the microprocessor core is not set."));
  }
  else if(pszProcInfo -> szProcessCore[0] == 0xFFFF)
  {
     Error (_T("Name of the microprocessor core is not set."));
     Error (_T("When the name is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Name of the microprocessor core: %s"), pszProcInfo -> szProcessCore);
  }

  // Revision number of the microprocessor core - optional
  Info (_T("")); //Blank line
  if(!pszProcInfo -> wCoreRevision)
  {
  	Info (_T("Revision number of the microprocessor core is not set."));
  }
  else if(pszProcInfo -> wCoreRevision == 0xFFFF)
  {
     Error (_T("Revision number of the microprocessor core is not set."));
     Error (_T("When the revision is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Revision number of the microprocessor core: %u"), 
                                                pszProcInfo -> wCoreRevision);
  }

  // Name of the actual microprocessor - optional
  Info (_T("")); //Blank line
  if(pszProcInfo -> szProcessorName[0] == 0)
  {
     Info (_T("Name of the actual microprocessor is not set."));
  }
  else if(pszProcInfo -> szProcessorName[0] == 0xFFFF)
  {
     Error (_T("Name of the actual microprocessor is not set."));
     Error (_T("When the name is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Name of the actual microprocessor: %s"), 
                                               pszProcInfo -> szProcessorName);
  }

  // Revision number of the microprocessor - optional
  Info (_T("")); //Blank line
  if(!pszProcInfo -> wProcessorRevision)
  {
     Info (_T("Revision number of the microprocessor is not set."));
  }
  else if(pszProcInfo -> wProcessorRevision == 0xFFFF)
  {
     Error (_T("Revision number of the microprocessor is not set."));
     Error (_T("When the revision is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Revision number of the microprocessor: %u."), 
                                            pszProcInfo -> wProcessorRevision);
  }

  // Catalog number of the processor - optional
  Info (_T("")); //Blank line
  if(pszProcInfo -> szCatalogNumber[0] == 0)
  {
     Info (_T("Catalog number of the microprocessor is not set."));
  }
  else if(pszProcInfo -> szCatalogNumber[0] == 0xFFFF)
  {
     Error (_T("Catalog number of the microprocessor is not set."));
     Error (_T("When the number is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Catalog number of the microprocessor: %s"), 
                                               pszProcInfo -> szCatalogNumber);
  }

  // Name of the microprocessor vendor - optional
  Info (_T("")); //Blank line
  if(pszProcInfo -> szVendor[0] == 0)
  {
     Info (_T("Name of the microprocessor vendor is not set."));
  }
  else if(pszProcInfo -> szVendor[0] == 0xFFFF)
  {
     Error (_T("Name of the microprocessor vendor is not set."));
     Error (_T("When the vendor is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Name of the microprocessor vendor: %s"), pszProcInfo -> szVendor);
  }

  // Instruction set flag - must be set
  Info (_T("")); //Blank line
  if(!pszProcInfo -> dwInstructionSet)
  {
     Info (_T("No flags are specified for the instruction set. "));
  }
  else if(pszProcInfo -> dwInstructionSet == PROCESSOR_FLOATINGPOINT)
  {
     Info (_T("Flag set for instruction set: PROCESSOR_FLOATINGPOINT."));
  }
  else if(pszProcInfo -> dwInstructionSet == PROCESSOR_DSP)
  {
     Info (_T("Flag set for instruction set: PROCESSOR_DSP."));
  }
  else if(pszProcInfo -> dwInstructionSet == PROCESSOR_16BITINSTRUCTION)
  {
     Info (_T("Flag set for instruction set: PROCESSOR_16BITINSTRUCTION."));
  }
  else if(pszProcInfo -> dwInstructionSet == 0xFFFFFFFF)
  {
     Error (_T("Instruction set flags are not being set."));
     Error (_T("When there is no flag, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else 
  {
     Error (_T("Unknown flag specified for the instruction set."));
     returnVal = FALSE;
  }

    // Maximum clock speed of the CPU - optional
  Info (_T("")); //Blank line
  if(!pszProcInfo -> dwClockSpeed)
  {
     Info (_T("Maximum clock speed of the CPU is not set."));
  }
  else if(pszProcInfo -> dwClockSpeed == 0xFFFFFFFF)
  {
     Error (_T("Maximum clock speed of the CPU is not set."));
     Error (_T("When the clock speed is not available, its value should be set to 0 but it is not."));
     returnVal = FALSE;
  }
  else
  {
     Info (_T("Maximum clock speed of the CPU: %u."), pszProcInfo -> dwClockSpeed);
  }
  
  Info (_T("/*************************************************************/"));
  
  Info (_T("")); //Blank line
  Info (_T("Please verify that the above values are as expected."));
  Info (_T(""));

cleanAndExit:
	
     if(pszProcInfo)
     {
  	free(pszProcInfo);
     }

     return returnVal;
}

