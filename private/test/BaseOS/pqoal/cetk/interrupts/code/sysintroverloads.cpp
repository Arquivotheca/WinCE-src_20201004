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

#include "sysIntrOverloads.h"

/* for the OAL_* defines */
#include "tuxInterrupts.h"

/*
 * we overload the test functions so that we don't have heinous ioctl
 * calls all over the code.
 */

BOOL
getSysIntrNew (DWORD dwRequestType, const DWORD *const dwIRQs, DWORD dwCount,
           DWORD * pdwSysIntr);

/******************************************************************************/
/*                                                                            */
/*                               Function Overloads                           */
/*                                                                            */
/******************************************************************************/

/*
 * Allows the test code to easily call the ioctl.  This is the most
 * general entry point.  Supports all of the allocation methods.
 *
 * dwRequestType is the request type (see above).  Next comes the irq
 * array and its length.  Finally, the sysintr value is returned
 * through pdwSysIntr.
 *
 * returns true on success and false otherwise.
 */
BOOL
getSysIntr (DWORD dwRequestType, const DWORD *const dwIRQs, DWORD dwCount,
        DWORD * pdwSysIntr)
{
  if (dwRequestType == SPECIAL_OAL_INTR_STANDARD)
    {
      /* use the older model */
      if (!dwIRQs || dwCount != 1 || !pdwSysIntr)
    {
      IntErr (_T("Wrong args for standard call to getSysIntr"));
      return (FALSE);
    }

      return (getSysIntr (*dwIRQs, pdwSysIntr));
    }
  else
    {
      return (getSysIntrNew (dwRequestType, dwIRQs, dwCount, pdwSysIntr));
    }
}

/*
 * handles only the new model calls to the ioctl.  Does not handle the
 * standard (or older) calls.
 *
 * The new model calls use the structure with a -1 as the leading flag.
 *
 * dwRequestType is the request type (see above).  Next comes the irq
 * array and its length.  Finally, the sysintr value is returned
 * through pdwSysIntr.
 *
 * returns true on success and false otherwise.
 */

BOOL
getSysIntrNew (DWORD dwRequestType, const DWORD *const dwIRQs, DWORD dwCount,
        DWORD * pdwSysIntr)
{
  BOOL returnVal = FALSE;

  DWORD dwFlag = (DWORD)SYSINTR_UNDEFINED;

  BYTE * buffer = NULL;

  if (dwCount >= (DWORD_MAX / sizeof (DWORD) - sizeof (dwFlag) - sizeof (dwRequestType)))
    {
      Error (_T("Overflow"));
      goto cleanAndReturn;
    }

  DWORD dwBufferSizeBytes = sizeof (dwFlag) + sizeof (dwRequestType) + dwCount * sizeof (DWORD);

  buffer = (BYTE *) malloc (dwBufferSizeBytes);

  if (!buffer)
    {
      Error (_T("Couldn't allocate memory."));
      goto cleanAndReturn;
    }

  memcpy (buffer, &dwFlag, sizeof (dwFlag));
  PREFAST_SUPPRESS (419, "Check above");
  memcpy (buffer + sizeof(dwFlag), &dwRequestType, sizeof (dwRequestType));
  memcpy (buffer + sizeof(dwFlag) + sizeof(dwRequestType),
       dwIRQs, dwCount * sizeof (DWORD));


  BOOL rc = KernelIoControl (IOCTL_HAL_REQUEST_SYSINTR,
                 (VOID *) buffer,
                 2 * sizeof (DWORD) + dwCount * sizeof (DWORD),
                 pdwSysIntr, sizeof (*pdwSysIntr), NULL);

  returnVal = rc;

cleanAndReturn:

  if (buffer)
  {
        free (buffer);
  }

  return (returnVal);

}

/*
 * Standard or old version of the call.  Supports only one irq.
 */
BOOL
getSysIntr(DWORD dwIRQ, DWORD * pdwSysIntr)
{
  BOOL rc = KernelIoControl (IOCTL_HAL_REQUEST_SYSINTR, &dwIRQ, sizeof (dwIRQ),
                 pdwSysIntr, sizeof (*pdwSysIntr), NULL);

  return (rc);
}

BOOL
releaseSysIntr(DWORD dwSysIntr)
{
  return (KernelIoControl (IOCTL_HAL_RELEASE_SYSINTR, &dwSysIntr,
               sizeof (dwSysIntr), NULL, NULL, NULL));
}

BOOL
checkSysIntrRange (DWORD dwSysIntr)
{
  if (dwSysIntr >= SYSINTR_MAXIMUM)
    {
      Error (_T("SYSINTR %u should always be < %u"),
         dwSysIntr, SYSINTR_MAXIMUM);
      return (FALSE);
    }

  return (TRUE);
}
