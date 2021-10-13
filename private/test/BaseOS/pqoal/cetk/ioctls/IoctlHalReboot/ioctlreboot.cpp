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
#include "tuxIoctlReboot.h"

/* common utils */
#include "..\..\..\common\commonUtils.h"


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the Ioctl_Hal_Reboot tests. It tells the user 
 * what the tests do and also specifies the input that the user needs to 
 * provide to run the tests. 
 */
TESTPROCAPI IoctlHalRebootUsageMessage(
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
  Info (_T("These tests verify if IOCTL_HAL_REBOOT is working correctly."));
  Info (_T(""));
  Info (_T("The tests need to be run in kernel mode. To do this,"));
  Info (_T("append \"-n\" to the command line."));
  Info (_T("Tests do not take any command line parameters."));
  Info (_T(""));

  return TPR_PASS;
}



/*******************************************************************************
 *
 *     CHECK IF THE IOCTL IS WORKING CORRECTLY
 *
 ******************************************************************************/
/*
 * This test verifies if Ioctl_Hal_Reboot reboots the device as expected.
 */
TESTPROCAPI testIoctlHalReboot(
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
    
  DWORD dwErrorCode;

  Info (_T(""));
  Info (_T("This test checks if IOCTL_HAL_REBOOT is working correctly."));
  
  // Call the Ioctl and reboot device
  Info (_T(""));
  Info (_T("Attempting to reboot the device in 10 sec..."));
  Info (_T("Test passes if device reboots successfully, else it fails."));
  Info (_T(""));

  Sleep(10000);
  
  BOOL fret = KernelIoControl( IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
  
  if(fret)
  {
    Error (_T("Calling the Ioctl returned TRUE. With a successful call the device"));
       Error (_T("reboots and the IOCTL never returns. We should never get here."));
  }
  else
  {
       dwErrorCode = GetLastError();
    Error (_T("Calling the Ioctl returned FALSE. With a successful call the device"));
       Error (_T("reboots and the IOCTL never returns. We should never get here."));
    Error (_T("GetLastError returned %u"), dwErrorCode);
  }
  
  Error (_T(""));
  Error (_T("Test Failed."));
  Error (_T(""));
  Error (_T("Check if you are running the test in kernel mode. To do this,"));
  Error (_T("append \"-n\" to the command line."));
  Error (_T(""));
       
  return (returnVal);
}


