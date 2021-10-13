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



////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

#include "tuxIoctls.h"
#include "deviceInfoTests.h"

/* common utils */
#include "..\..\..\common\commonUtils.h"
#include "..\..\..\common\commonIoctlTests.h"

#include <windows.h>
//#include <tchar.h>
#include <tux.h>

	
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

  Info (_T(""));
  Info (_T("This test prints the output of the Device Info IOCTL."));
  
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

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
  	_T("parameters."));
  
  if((!CheckIncorrectInOutParameters(IOCTL_HAL_GET_DEVICE_INFO, INBOUND, 
     (VOID*)&dwSpi, sizeof(dwSpi)))||(!CheckDeviceInfoInvalidInputs(dwSpi)))
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

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  DWORD dwSpi = lpFTE->dwUserData;

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
  	    _T("parameters."));
  
  if(!CheckIncorrectInOutParameters(IOCTL_HAL_GET_DEVICE_INFO, OUTBOUND, 
  	                                         (VOID*)&dwSpi, sizeof(dwSpi)))
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

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the inbound"));
  Info (_T("buffer. It also checks for inbound buffer overflow."));
  
  if(!CheckInboundBufferAlignment(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, 
  	                                                         sizeof(dwSpi)))
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

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the outbound"));
  Info (_T("buffer. It also checks for outbound buffer overflow."));
  
  if(!CheckOutboundBufferAlignment(IOCTL_HAL_GET_DEVICE_INFO, (VOID*)&dwSpi, 
  	                                                         sizeof(dwSpi)))
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
 *               DEVICE ID IOCTL - VERIFY OUTPUT VALUE
 *
 ******************************************************************************/
/* 
 * This test prints the output of the Device Info IOCTL. 
 * The user is required to visually verify if it is correct.
 */

TESTPROCAPI
testIoctlDevIDOutputValue(
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
  Info (_T("This test prints the output of the Device ID IOCTL."));
  
  if(!PrintDeviceIDOutput())
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
 *               DEVICE ID IOCTL - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/
/* 
 * This test will check all the incorrect inbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlDevIDInParam(
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
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
  	_T("parameters."));
  
  if(!CheckIfInboundParametersIgnored(IOCTL_HAL_GET_DEVICEID)) 
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
 *               DEVICE ID IOCTL - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/
/* 
 * This test will check all the incorrect outbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlDevIDOutParam(
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
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
  	    _T("parameters."));
  
  if(!DevIDCheckIncorrectOutParameters())
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
 *               DEVICE ID IOCTL - CHECK MIS-ALIGNED OUTBOUND PARAMETER
 *
 ******************************************************************************/
/* 
 * This test checks all the different alignments of the outbound buffer.
 * The test also checks for overflow of the output buffer.
 */

TESTPROCAPI
testIoctlDevIDOutBufferAlignment(
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
  Info (_T("This test verifies all the different alignments of the outbound"));
  Info (_T("buffer. It also checks for outbound buffer overflow."));
  
  if(!CheckOutboundBufferAlignment(IOCTL_HAL_GET_DEVICEID, NULL, 0))
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
 *               UUID IOCTL - VERIFY OUTPUT VALUE
 *
 ******************************************************************************/
/* 
 * This test prints the output of the Device Info IOCTL. 
 * The user is required to visually verify if it is correct.
 */

TESTPROCAPI
testIoctlUUIDOutputValue(
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
  Info (_T("This test prints the output of the UUID IOCTL."));
  
  if(!PrintUUIDOutput())
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
 *               UUID IOCTL - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/
/* 
 * This test will check all the incorrect inbound parameters for the IOCTL.
 */


TESTPROCAPI
testIoctlUUIDInParam(
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
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
  	    _T("parameters."));
  
  if(!CheckIfInboundParametersIgnored(IOCTL_HAL_GET_UUID))
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
 *               UUID IOCTL - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/
/* 
 * This test will check all the incorrect outbound parameters for the IOCTL.
 */

TESTPROCAPI
testIoctlUUIDOutParam(
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
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
  	    _T("parameters."));
  
  if(!CheckIncorrectInOutParameters(IOCTL_HAL_GET_UUID, OUTBOUND, NULL, 0))
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
 *               UUID IOCTL - CHECK MIS-ALIGNED OUTBOUND PARAMETER
 *
 ******************************************************************************/
/* 
 * This test checks all the different alignments of the outbound buffer.
 * The test also checks for overflow of the output buffer.
 */

TESTPROCAPI
testIoctlUUIDOutBufferAlignment(
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
  Info (_T("This test verifies all the different alignments of the outbound"));
  Info (_T("buffer. It also checks for outbound buffer overflow."));
  
  if(!CheckOutboundBufferAlignment(IOCTL_HAL_GET_UUID, NULL, 0))
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

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Inbound ")
  	    _T("parameters."));
  
  if(!CheckIfInboundParametersIgnored(IOCTL_PROCESSOR_INFORMATION))
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

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies calling the IOCTL with incorrect Outbound ")
  	_T("parameters."));
  
  if(!CheckIncorrectInOutParameters(IOCTL_PROCESSOR_INFORMATION, OUTBOUND, NULL, 0))
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

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
  {
     return (TPR_NOT_HANDLED);
  }

  Info (_T(""));
  Info (_T("This test verifies all the different alignments of the outbound"));
  Info (_T("buffer. It also checks for outbound buffer overflow."));
  
  if(!CheckOutboundBufferAlignment(IOCTL_PROCESSOR_INFORMATION, NULL, 0))
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