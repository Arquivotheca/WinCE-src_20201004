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


// This file contains tests related to the following IOCTLs.
// IOCTL_HAL_GET_DEVICE_INFO
// IOCTL_PROCESSOR_INFORMATION


////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/
#include <windows.h>

#include "tuxIoctls.h"
#include "commonIoctlTests.h"

////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/


/******************************  Helper Functions *****************************/

// This function prints the output of the Device Info IOCTL.
BOOL PrintDeviceInfoOutput(DWORD dwInputSpi);

// This function verifies invalid inputs for Device Info IOCTL.
// Possible DWORD values other than valid SPI values are tested here.
BOOL CheckDeviceInfoInvalidInputs(DWORD dwInputSpi);

// This function prints the output of the Processor Information IOCTL.
BOOL PrintProcInfoOutput(VOID);

////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the Ioctl tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */

TESTPROCAPI
IoctlsTestUsageMessage(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("These tests aim to test Ioctls."));

  Info (_T(""));
  Info (_T("The tests require no arguments from the user."));
  Info (_T("However, the tests to check the output values of the IOCTLs"));
  Info (_T("require the tester to visually verify the correctness of the output."));

  Info (_T(""));

  return (TPR_PASS);
}


/*******************************************************************************
 *
 *               DEVICE INFO IOCTL - VERIFY OUTPUT VALUE
 *
 ******************************************************************************/
/*
 * This test prints the output of the Device Info IOCTL.
 * The user is required to visually verify if it is correct.
 */

TESTPROCAPI
testIoctlDevInfoOutputValue(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;


  if(dwSpi == 0xFFFFFFFF) //value is user specified
  {
    Info(_T("This test requires the -SystemParam command line argument"));
    Info(_T("It needs to be an SPI value that is supported by IOCTL_HAL_GET_DEVICE_INFO."));
    Info(_T("Check the winuser.h header to get the values."));
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;
        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndExit;
        }

        if (getOptionDWord (&tokens, _T("-SystemParam"), &dwSpi))
        {
            Info (_T("System Parameter set to %d"),dwSpi);
        }
        else
        {
            Info (_T("No system parameter specifed on the command line"));
            returnVal = TPR_SKIP;
            goto cleanAndExit;
        }
    }
    else
    {
        Info (_T("No command line detected"));
        goto cleanAndExit;
    }
  }

  if(!PrintDeviceInfoOutput(dwSpi))
  {
     Error (_T(""));
     Error (_T("Could not print the output."));
     goto cleanAndExit;
  }

  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}

/*******************************************************************************
 *
 *               DEVICE INFO IOCTL - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test will check all the incorrect inbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlDevInfoInParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;
  DWORD dwOutputSize = 0;

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
  _T("parameters."));
  
  if (!GetOutputSize(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, sizeof(dwSpi),
     &dwOutputSize) )
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }
  

  if((!CheckIncorrectInOutParameters(IOCTL_HAL_GET_DEVICE_INFO, INBOUND,
     (VOID*)&dwSpi, sizeof(dwSpi), dwOutputSize))||(!CheckDeviceInfoInvalidInputs(dwSpi)))
  {
     Error (_T(""));
     Error (_T("The verify incorrect Inbound parameters test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The verify incorrect Inbound parameters test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}

/*******************************************************************************
 *
 *               DEVICE INFO IOCTL - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test will check all the incorrect outbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlDevInfoOutParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;
  DWORD dwOutputSize = 0;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
        _T("parameters."));
  
  if (!GetOutputSize(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, sizeof(dwSpi), &dwOutputSize) )
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }
  
  if(!CheckIncorrectInOutParameters(IOCTL_HAL_GET_DEVICE_INFO, OUTBOUND,
                                             (VOID*)&dwSpi, sizeof(dwSpi), dwOutputSize))
  {
     Error (_T(""));
     Error (_T("The verify incorrect Outbound parameters test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The verify incorrect Outbound parameters test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}


/*******************************************************************************
 *
 *         DEVICE INFO IOCTL - CHECK ALIGNMENT OF INBOUND BUFFER
 *
 ******************************************************************************/
/*
 * This test checks all the different alignments of the inbound buffer.
 * The test also checks for overflow of the input buffer.
 */

TESTPROCAPI
testIoctlDevInfoInBufferAlignment(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;
  DWORD dwOutputSize = 0;

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the inbound"));
  Info (_T("buffer. It also checks for inbound buffer overflow."));

  if (!GetOutputSize(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, sizeof(dwSpi), &dwOutputSize)
    ||  (!CheckDeviceInfoInvalidInputs(dwSpi)) )  
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }
  
  if(!CheckInboundBufferAlignment(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi,
                                                             sizeof(dwSpi), dwOutputSize))
  {
     Error (_T(""));
     Error (_T("The check alignment of inbound buffer test failed."));
     goto cleanAndExit;
  }  

  Info (_T(""));
  Info (_T("The check alignment of inbound buffer test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}

/*******************************************************************************
 *
 *           DEVICE INFO IOCTL - CHECK ALIGNMENT OF OUTBOUND BUFFER
 *
 ******************************************************************************/
/*
 * This test checks all the different alignments of the outbound buffer.
 * The test also checks for overflow of the output buffer.
 */

TESTPROCAPI
testIoctlDevInfoOutBufferAlignment(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;
  DWORD dwOutputSize = 0;

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the outbound"));
  Info (_T("buffer. It also checks for outbound buffer overflow."));

  if (!GetOutputSize(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, sizeof(dwSpi), &dwOutputSize))  
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }
  BOOL bOutputIsConstant = TRUE;
  if(!CheckOutboundBufferAlignment(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi,
                                                             sizeof(dwSpi), dwOutputSize, bOutputIsConstant))
  {
     Error (_T(""));
     Error (_T("The check alignment of outbound buffer test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The check alignment of outbound buffer test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}


/*******************************************************************************
 *
 *               PROCESSOR INFORMATION IOCTL - VERIFY OUTPUT VALUE
 *
 ******************************************************************************/
/*
 * This test prints the output of the Device Info IOCTL.
 * The user is required to visually verify if it is correct.
 */

TESTPROCAPI
testIoctlProcInfoOutputValue(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test prints the output of the Processor Information IOCTL."));

  if(!PrintProcInfoOutput())
  {
     Error (_T(""));
     Error (_T("Could not get one or more of the output values that are ")
            _T("required to be set."));
     goto cleanAndExit;
  }

  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}

/*******************************************************************************
 *
 *               PROCESSOR INFORMATION IOCTL - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test will check all the incorrect inbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlProcInfoInParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;
  DWORD dwOutputSize = 0;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
        _T("parameters."));

 if (!GetOutputSize(IOCTL_PROCESSOR_INFORMATION, NULL, 0, &dwOutputSize))  
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }

  if(!CheckIfInboundParametersIgnored(IOCTL_PROCESSOR_INFORMATION, dwOutputSize, TRUE))
  {
     Error (_T(""));
     Error (_T("The verify incorrect Inbound parameters test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The verify incorrect Inbound parameters test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}


/*******************************************************************************
 *
 *               PROCESSOR INFORMATION IOCTL - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test will check all the incorrect outbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlProcInfoOutParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;
  DWORD dwOutputSize = 0;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
    _T("parameters."));


 if (!GetOutputSize(IOCTL_PROCESSOR_INFORMATION, NULL, 0, &dwOutputSize))  
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }

  if(!CheckIncorrectInOutParameters(IOCTL_PROCESSOR_INFORMATION, OUTBOUND, NULL, 0, dwOutputSize))
  {
     Error (_T(""));
     Error (_T("The verify incorrect Outbound parameters test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The verify incorrect Outbound parameters test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}


/*******************************************************************************
 *
 *     PROCESSOR INFORMATION IOCTL - CHECK MIS-ALIGNED OUTBOUND PARAMETER
 *
 ******************************************************************************/
/*
 * This test checks all the different alignments of the outbound buffer.
 * The test also checks for overflow of the output buffer.
 */

TESTPROCAPI
testIoctlProcInfoOutBufferAlignment(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_FAIL;
    DWORD dwOutputSize = 0;
    
  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the outbound"));


 if (!GetOutputSize(IOCTL_PROCESSOR_INFORMATION, NULL, 0, &dwOutputSize))  
  {
     Error (_T(""));
     Error (_T("Could not get output size."));
     goto cleanAndExit;
  }

  Info (_T("buffer. It also checks for outbound buffer overflow."));

    BOOL bOutputIsConstant = TRUE;
  if(!CheckOutboundBufferAlignment(IOCTL_PROCESSOR_INFORMATION, NULL, 0, dwOutputSize, bOutputIsConstant))
  {
     Error (_T(""));
     Error (_T("The check alignment of outbound buffer test failed."));
     goto cleanAndExit;
  }

  Info (_T(""));
  Info (_T("The check alignment of outbound buffer test passed."));
  returnVal = TPR_PASS;

cleanAndExit:
  Info (_T(""));
  return (returnVal);
}


////////////////////////////////////////////////////////////////////////////////
/******************************  Functions  ***********************************/

/*******************************************************************************
 *                   Print output of the DeviceInfo IOCTL
 ******************************************************************************/

// This function prints the output of the Device Info IOCTL
//
// Arguments:
//      Input: SPI code
//             Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

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


  ////////////Check if the IOCTL is supported ///////////
  if(!VerifyIoctlSupport (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
                                     sizeof(dwInputSpi)))
  {
    Error (_T("IOCTL not supported."));
    goto cleanAndExit;
  }
  ////////////get the output size ///////////
  if(!GetOutputSize (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
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
                        || (dwInputSpi == SPI_GETBOOTMENAME)
                        || (dwInputSpi == SPI_GETOEMINFO)
                        || (dwInputSpi == SPI_GETPLATFORMMANUFACTURER))
  {
     TCHAR *pszDeviceInfo;

     pszDeviceInfo = (TCHAR*)pCorrectOutBuf;

     pszDeviceInfo[(dwCorrectOutSize/sizeof(TCHAR)) - 1] = L'\0';

     Info (_T("The output value is %s"),pszDeviceInfo);
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
        goto cleanAndExit;
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
//      Input: None
//             Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

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
  
  ////////////Check if the IOCTL is supported ///////////
  if(!VerifyIoctlSupport (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
                                     sizeof(dwInputSpi)))
  {
    Error (_T("IOCTL not supported."));
    goto cleanAndExit;
  }
  ////////////get the output size /////////
  if(!GetOutputSize (IOCTL_HAL_GET_DEVICE_INFO, &dwInputSpi,
                                      sizeof(dwInputSpi), &dwCorrectOutSize))
  {
    Error (_T("Could not get the output size."));
    goto cleanAndExit;
  }

  //////////Check all invalid inputs ////////////////////////////
  DWORD dwValidSpiArr[] = {SPI_GETPLATFORMNAME, SPI_GETPROJECTNAME,
                           SPI_GETPLATFORMTYPE, SPI_GETBOOTMENAME, SPI_GETOEMINFO,
                           SPI_GETUUID, SPI_GETGUIDPATTERN};
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

  for(DWORD i = 0; i < sizeof(dwValidSpiArr)/sizeof(DWORD); i++)
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
 *                     Print output of the Processor Information IOCTL
 ******************************************************************************/

// This function prints the output of the Processor Information IOCTL
//
// Arguments:
//      Input: None
//             Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

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

  ////////////Check if the IOCTL is supported ///////////
  if(!VerifyIoctlSupport (IOCTL_PROCESSOR_INFORMATION, NULL, 0))
  {
    Error (_T("IOCTL not supported."));
    goto cleanAndExit;
  }
  ////////////get the output size ///////////
  if(!GetOutputSize (IOCTL_PROCESSOR_INFORMATION, NULL, 0,
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
    switch (pszProcInfo -> dwInstructionSet) {
    case 0:
        Info (_T("Flag Set for Instruction Set : NONE. This is a valid Instruction Set with no FP, no DSP, no 16-bit instruction"));
        break;
    case PROCESSOR_FLOATINGPOINT:
        Info (_T("Flag Set for Instruction Set : PROCESSOR_FLOATINGPOINT"));
        break;
    case PROCESSOR_DSP:
        Info (_T("Flag Set for Instruction Set : PROCESSOR_DSP"));
        break;
    case PROCESSOR_16BITINSTRUCTION:
        Info (_T("Flag Set for Instruction Set : PROCESSOR_16BITINSTRUCTION"));
        break;
    case (PROCESSOR_FLOATINGPOINT + PROCESSOR_DSP):
        Info (_T("Flag Set for Instruction Set : PROCESSOR_FLOATINGPOINT and PROCESSOR_DSP"));
        break;
    case (PROCESSOR_FLOATINGPOINT + PROCESSOR_16BITINSTRUCTION):
        Info (_T("Flag Set for Instruction Set : PROCESSOR_FLOATINGPOINT and PROCESSOR_16BITINSTRUCTION"));
        break;
    case (PROCESSOR_DSP + PROCESSOR_16BITINSTRUCTION):
        Info (_T("Flag Set for Instruction Set : PROCESSOR_DSP and PROCESSOR_16BITINSTRUCTION"));
        break;
    case (PROCESSOR_FLOATINGPOINT + PROCESSOR_DSP + PROCESSOR_16BITINSTRUCTION):
        Info (_T("Flag Set for Instruction Set : PROCESSOR_FLOATINGPOINT, PROCESSOR_DSP and PROCESSOR_16BITINSTRUCTION"));
        break;
    default :
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




