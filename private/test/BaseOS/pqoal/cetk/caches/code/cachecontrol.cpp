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
/*
 * cacheControl.cpp
 */

#include "..\..\..\common\commonUtils.h"
#include "cacheControl.h"

/*
 * see the header file for more information
 */

/*
 * AllocPhysMem turns off caching when it allocates the memory block.
 * VirtualAlloc does not.
 */

#ifdef _X86_

/*
 * one bit switches between write-through and write-back
 * one bit for caching disabled vs. enabled
 */
#define CACHE_MODE_MASK           0x00000018
#define CACHE_MODE_WRITE_THROUGH  0x00000008
#define CACHE_MODE_WRITE_BACK     0x00000000
#define CACHE_MODE_DISABLED       0x00000010

#define PROC_TYPE (_T("X86"))

#elif _ARM_

/*
 * always set the cache enabled bit (bit 3).
 * Flip the buffering bit (bit 2).
 */
#define CACHE_MODE_MASK           0x0000000c
#define CACHE_MODE_WRITE_THROUGH  0x00000008
#define CACHE_MODE_WRITE_BACK     0x0000000c
#define CACHE_MODE_DISABLED       0x00000000

#define PROC_TYPE (_T("ARM"))

#elif _SHX_

/*
 * bit 7 is for cache / uncached (set = cached)
 * bit 3 is write-through (1) vs. write-back (0)
 */
#define CACHE_MODE_MASK           0x00000088
#define CACHE_MODE_WRITE_THROUGH  0x00000088
#define CACHE_MODE_WRITE_BACK     0x00000080
#define CACHE_MODE_DISABLED       0x00000000

#define PROC_TYPE (_T("SHX"))

#elif _MIPS_

/*
 * can only change the mode between cached and uncached
 *
 * The write policy is hardware implementation dependent,
 * so we only support enabled and disabled.
 */
#define CACHE_MODE_MASK           0x00000038
#define CACHE_MODE_ENABLED   0x00000018
#define CACHE_MODE_DISABLED       0x00000010

#define PROC_TYPE (_T("MIPS"))

#else
#error "Unknown processor type"
#endif

/****** Kenel mode **********************************************************/
__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}

/*
 * Set the cache mode to the value specified in the incoming
 * parameter.  The incoming paramter is set from the CT_* macros from
 * tuxCaches.h.
 *
 * return TRUE on success and FALSE otherwise
 */
BOOL
setCacheMode (DWORD * vals, DWORD dwSizeBytes, DWORD dwParam)
{
    if(!inKernelMode())
    {
        Error (_T(""));
        Error (_T("Error!!!  VirtualSetAttributes must be called in kernel mode."));
        Error (_T("Are you sure you are running the test correctly?"));
        Error (_T("Consider using -n option on the tux command line"));
        Error (_T(""));
        return FALSE;
    }

    if (vals == NULL)
    {
        IntErr (_T("(vals == NULL)"));
        return (FALSE);
    }

    if (dwSizeBytes == 0)
    {
        IntErr (_T("(dwSizeBytes == 0)"));
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /*
    * shx is funky.  The bit fields vary between sh3, sh4, and sh5.  Not
    * going to attempt to deal with it at this time.
    */
#ifdef _SHX_
    Error (_T("Currently not supporting SHX."));
    goto cleanAndReturn;
#endif

    /* take only the bits that we need */
    dwParam &= CT_WRITE_MODE_MASK;

    DWORD dwNewFlags = BAD_VAL;
    DWORD dwMask = CACHE_MODE_MASK;

    const TCHAR * szNewSetting = NULL;
    const TCHAR * szOldSetting = NULL;

    switch (dwParam)
    {
#ifdef _MIPS_
    case CT_WRITE_THROUGH:
    case CT_WRITE_BACK:
      /* both do the same thing */
      dwNewFlags = CACHE_MODE_ENABLED;
      szNewSetting = _T("enabled");
      break;
#else /* not MIPS */
    case CT_WRITE_THROUGH:
      /* set cache into write through mode */
      dwNewFlags = CACHE_MODE_WRITE_THROUGH;
      szNewSetting = _T("write-through");
      break;
    case CT_WRITE_BACK:
      /* set cache into write back mode */
      dwNewFlags = CACHE_MODE_WRITE_BACK;
      szNewSetting = _T("write-back");
      break;
#endif
    case CT_DISABLED:
      /* set cache into write back mode */
      dwNewFlags = CACHE_MODE_DISABLED;
      szNewSetting = _T("disabled");
      break;
    default:
      Error (_T("Cache mode to set to is not correct."));
      goto cleanAndReturn;
      break;
    }

    /* original setting for the entries */
    DWORD dwOriginalEntry = BAD_VAL;
    DWORD dwModifiedEntry = BAD_VAL;

  /* set the cache, saving the original entry */
    if (VirtualSetAttributes (vals, dwSizeBytes, dwNewFlags,
      dwMask, &dwOriginalEntry) != TRUE)
    {
        Error (_T("Couldn't set cache attributes."));
        Error (_T("VirtualSetAttributes failed."));
        goto cleanAndReturn;
    }

    /* query kernel for change */
    if (VirtualSetAttributes (vals, dwSizeBytes, 0, 0, &dwModifiedEntry) != TRUE)
    {
        Error (_T("Couldn't query kernel after setting cache attributes."));
        Error (_T("VirtualSetAttributes failed."));
        goto cleanAndReturn;
    }

    Info (_T("Processor type is %s"), PROC_TYPE);

    Info (_T("Orignal page table entry: 0x%x  New entry: 0x%x"),
        dwOriginalEntry, dwModifiedEntry);

    /* figure out which type the old setting corresponds to */
    switch (dwOriginalEntry & CACHE_MODE_MASK)
    {
#ifdef _MIPS_
    case CACHE_MODE_ENABLED:
      /* both do the same thing */
      szOldSetting = _T("enabled");
      break;
#else /* not MIPS */
    case CACHE_MODE_WRITE_THROUGH:
      szOldSetting = _T("write-through");
      break;
    case CACHE_MODE_WRITE_BACK:
      szOldSetting = _T("write-back");
      break;
#endif
    case CACHE_MODE_DISABLED:
      szOldSetting = _T("disabled");
      break;
    default:
      szOldSetting = _T("unknown");
    }

    Info (_T("Original cache mode for test array was %s.  It is now %s."),
        szOldSetting, szNewSetting);

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}


/*
 * Set the cache mode to the value specified in the incoming
 * parameter.  The incoming paramter is set from the CT_* macros from
 * tuxCaches.h.
 *
 * return TRUE on success and FALSE otherwise
 */
BOOL
queryDefaultCacheMode (DWORD * dwMode)
{
  BOOL returnVal = FALSE;

  /*
   * allocated memory to query on.  Forces us to get a page entry that
   * is fresh and will have the default flags.
   */
  DWORD * vals = NULL;

  /*
   * shx is funky.  The bit fields vary between sh3, sh4, and sh5.  Not
   * going to attempt to deal with it at this time.
   */
#ifdef _SHX_
  Error (_T("Currently not supporting SHX."));
  goto cleanAndReturn;
#endif

  /* page table entry */
  DWORD dwEntry;

  /*
   * Force the os to get us this memory.  Memory will be on a page boundary.
   */
  vals = (DWORD *) VirtualAlloc (NULL, sizeof (DWORD),
                                 MEM_COMMIT, PAGE_READWRITE);

  if (vals == NULL)
    {
      Error (_T("The test tried to allocate %u bytes of memory using ")
             _T("VirtualAlloc.  The call failed.  GetLastError returned %u."),
             sizeof (DWORD), GetLastError ());
      goto cleanAndReturn;
    }

  /* query kernel for page table entry */
  if (VirtualSetAttributes (vals, sizeof (DWORD), 0, 0, &dwEntry) != TRUE)
    {
      Error (_T("Couldn't query kernel after setting cache attributes."));
      goto cleanAndReturn;
    }

  Info (_T("Page table entry is 0x%x"), dwEntry);

    /* figure out which type the setting corresponds to */
  switch (dwEntry & CACHE_MODE_MASK)
    {
#ifdef _MIPS_
    case CACHE_MODE_ENABLED:
      *dwMode = CT_WRITE_THROUGH;
      break;
#else /* not MIPS */
    case CACHE_MODE_WRITE_THROUGH:
      *dwMode = CT_WRITE_THROUGH;
      break;
    case CACHE_MODE_WRITE_BACK:
      *dwMode = CT_WRITE_BACK;
      break;
#endif
    case CACHE_MODE_DISABLED:
      *dwMode = CT_DISABLED;
      break;
    default:
      returnVal = FALSE;
      *dwMode = CT_IGNORED;
    }

  returnVal = TRUE;

 cleanAndReturn:

  if (vals)
    {
      if (!VirtualFree (vals, 0, MEM_RELEASE))
        {
          Error (_T("Couldn't free virtually allocated memory."));
          Error (_T("getLastError is %u."), GetLastError());
          returnVal = FALSE;
        }
    }

  return (returnVal);
}

/*
 * Given a CT_* macro, give us the string representation of it.
 *
 * This will always return a constant string.
 */
const TCHAR *
convCacheMode (DWORD dwMode)
{
#ifdef _MIPS_
  /* mips standard doesn't specify.  Left up to the oems. */
  return (dwMode & CT_WRITE_THROUGH ? _T("enabled") :
      dwMode & CT_WRITE_BACK ? _T("enabled") :
      dwMode & CT_DISABLED ? _T("disabled") : _T("INTERNAL ERROR"));
#else
  return (dwMode & CT_WRITE_THROUGH ? _T("write-through") :
      dwMode & CT_WRITE_BACK ? _T("write-back") :
      dwMode & CT_DISABLED ? _T("disabled") : _T("INTERNAL ERROR"));
#endif
}
