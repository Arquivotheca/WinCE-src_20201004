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
 * cacheTest.cpp
 */

/*
 * Main test code for caches.  The TESTPROCs call into here after
 * setting stuff up.
 *
 * This is pretty straight forward.  The only oddities occur in how we
 * try to find errors.
 *
 * trackingHash allows us to produce a hash of a given position in the
 * array at a given iteration.  We do this so that the values that are
 * written into a given position differ for each iteration.  We then
 * get a different value in each spot, which means that stale
 * information can more easily be detected.
 *
 * For the cases when we just write the values once and read others
 * around them, we use hash1.  This doesn't need to know about the
 * iteration.  These are guard values to make sure that these values
 * don't change while others are being written.
 */



#include "cacheTest.h"
#include "..\..\..\common\commonUtils.h"

/* grab the routines for randomizing our walks through the array */
#include "randomNumbers.h"

/***************************************************************************
 *
 *             Local Declarations Not Exported Through the Headers
 *
 ***************************************************************************/

/* for tracking correct values in the array */
DWORD
hash1(DWORD key);

DWORD
trackingHash (DWORD dwVal, DWORD dwIter);

/* convert a virtual address into a physical address */
BOOL
getPhyAddress (DWORD * pdwVirtual, DWORD * pdwPhysical);

/*
 * defines internal return values for a test.  This allows the function to
 * determine why a test failed and tell the user the correct things about
 * it.
 */
enum RETURN_VAL {R_TRUE, R_FALSE, R_BAD_CACHE};

#ifdef ALWAYS_OPTIMIZE
#pragma optimize("t", on)
#endif

/***************************************************************************
 *
 *                                Implementation
 *
 ***************************************************************************/

/***************************************************************************
 *
 *  Cache Test Unweighted
 *
 ***************************************************************************/

/*
 * See header for more information about this function.
 */
BOOL
cacheTestWriteEverythingReadEverything(__inout_ecount(dwArraySize) volatile DWORD * vals,
                       DWORD dwArraySize,
                       DWORD dwIterations)

{
  if (vals == NULL)
    {
      IntErr (_T("cacheTestWriteEverythingReadEverything: vals == NULL)"));
      return FALSE;
    }

  if (dwArraySize == 0)
    {
      IntErr (_T("cacheTestWriteEverythingReadEverything: dwArraySize == 0"));
      return FALSE;
    }

  /* assume true until proven otherwise */
  RETURN_VAL eReturnVal = R_TRUE;

  /* current loop iteration */
  DWORD dwIter = UNINIT_DW;

  /*
   * run through the loop dwIterations, checking after each value has been
   * loaded whether it is correct.
   */
  for (dwIter = 0; dwIter < dwIterations; dwIter++)
    {
      /*
       * we could zero memory here, but if we use a tracking value then we
       * don't have to.  Zeroing memory will also clear the cache, which might
       * hide bugs.  It is a tradeoff (we could do both).
       */

      /*
       * load the values into memory, hitting the cache
       */
      for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos++)
    {
      vals[dwPos] = trackingHash (dwPos, dwIter);
    }

      /* now, read the values back and make sure that they are correct */

      /*
       * start past zero so that the dwDelta doesn't cause a negative
       * index.  End before the last elements so that we don't overrun
       * the array's end.
       */
      for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos++)
    {
      if (vals[dwPos] != trackingHash (dwPos, dwIter))
        {
          DWORD dwPhyAdd = BAD_VAL;

          /* getPhyAddress should print an error if it fails */
          /*
           * cast off the volatile, because we don't care about optimizations
           * in this function.
           */
          if (getPhyAddress ((DWORD *)(vals + dwPos), &dwPhyAdd) == TRUE)
        {
          Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  Got = 0x%.8x")
             _T("  Physical Address = 0x%x"),
             dwPos, dwPos, trackingHash (dwPos, dwIter),
             vals[dwPos], dwPhyAdd);
        }
          else
        {
          Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  Got = 0x%.8x")
             _T("  Physical Address = UNKNOWN"),
             dwPos, dwPos, trackingHash (dwPos, dwIter),
             vals[dwPos]);
        }

          /*
           * record that we have an error, but don't bail out
           * because we want to print all values that are erronous
           */
          eReturnVal = R_BAD_CACHE;

        } /* if */

    } /* checking for loop */

      if (eReturnVal != R_TRUE)
    {
      Error (_T("Test failed on pass %u."), dwIter);

      /*
       * we have printed out information about the env, so now
       * bail out
       */
      goto cleanAndReturn;
    }

    } /* dwIter for loop */

 cleanAndReturn:

  /* return a boolean */
  if (eReturnVal == R_TRUE)
    {
      return (TRUE);
    }
  else
    {
      return (FALSE);
    }

}


/***************************************************************************
 *
 *  Cache Test Write Boundaries Read Everything
 *
 ***************************************************************************/

/*
 * Please see the header file for more info.
 */
BOOL
cacheTestWriteBoundariesReadEverything(__inout_ecount(dwArraySize) volatile DWORD * vals, DWORD dwArraySize, DWORD dwMaxIterations, DWORD dwStep)
{
  if (vals == NULL)
    {
      IntErr (_T("cacheTestWriteBoundariesReadEverything: vals == NULL"));
      return FALSE;
    }

  if (dwArraySize < 8)
    {
      IntErr (_T("cacheTestWriteBoundariesReadEverything: dwArraySize < 8"));
      return FALSE;
    }

  /* assume true until proven otherwise */
  RETURN_VAL eReturnVal = R_TRUE;

  /*
   * load array with hash values, so that we can track correctness of the
   * values that aren't going to be accessed.
   */
  for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos++)
    {
      vals[dwPos] = hash1 (dwPos);
    }

  /* do this test for a given number of times */
  for (DWORD dwIter = 0; dwIter < dwMaxIterations; dwIter++)
    {
      /******* move forwards through the array ****************************/

      /* twiddle the values on the boardaries */
      /*
       * the ending statement is designed to catch all sets at the beginning
       * and end of the dwStep boundary.  We don't catch the potential lone
       * value in this loop.
       */
      for (DWORD dwPos = 0;
       dwPos <= dwArraySize - (dwArraySize % dwStep) - dwStep;
       dwPos += dwStep)
    {
      vals[dwPos] = trackingHash (dwPos, dwIter);
      vals[dwPos + dwStep - 1] = trackingHash (dwPos + dwStep - 1, dwIter);
    }

      /*
       * The lone value will occur if the arraysize isn't exactly equal to
       * the step size.  In this case set this value so that the loop
       * below can easily check the entire range.
       */
      if ((dwArraySize % dwStep) != 0)
    {
      vals[dwArraySize - (dwArraySize % dwStep)] =
        trackingHash (dwArraySize - (dwArraySize % dwStep), dwIter);
    }

      /* now, read the values back and make sure that they are correct */
      for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos ++)
    {
      /* value that we expect to get; calculated from the correct hash */
      DWORD dwExpectedValue = BAD_VAL;

      /* string representing which hash was used */
      const TCHAR * szType = NULL;

      if ((dwPos % dwStep == 0) || ((dwPos % dwStep) == (dwStep - 1)))
        {
          /* this value was one that we twiddled */
          dwExpectedValue = trackingHash (dwPos, dwIter);
          szType = _T("twiddled");
        }
      else
        {
          /* this value hasn't been written since the initialization phase */
          dwExpectedValue = hash1 (dwPos);
          szType = _T("guard");
        }

      if (vals[dwPos] != dwExpectedValue)
        {
          DWORD dwPhyAdd = BAD_VAL;

          /* getPhyAddress should print an error if it fails */
          /*
           * cast off the volatile, because we don't care about optimizations
           * for this error condition
           */
          if (getPhyAddress ((DWORD *) (vals + dwPos), &dwPhyAdd) == TRUE)
        {
          Error (_T("%s val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = 0x%x"),
             szType,
             dwPos, dwPos, dwExpectedValue, vals[dwPos], dwPhyAdd);
        }
          else
        {
          Error (_T("%s val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = UNKNOWN"),
             szType,
             dwPos, dwPos, dwExpectedValue, vals[dwPos]);
        }

          /*
           * record that we have an error, but don't bail out
           * because we want to print all values that are erronous
           */
          eReturnVal = R_BAD_CACHE;

        } /* if vals aren't equal */

    } /* checking for loop */

      if (eReturnVal != R_TRUE)
    {
      Error (_T("Test failed walking forward through the array on ")
         _T("iteration %u.  Guard values should not have been touched ")
         _T("during the test.  Twiddled values are changed by the ")
         _T("test."), dwIter);

      /* we have printed out information about the env, so now bail out */
      goto cleanAndReturn;
    }

      /******* move backwards through the array ****************************/

      /*
       * The lone value will occur if the arraysize isn't exactly equal to
       * the step size.  In this case set this value so that the loop
       * below can easily check the entire range.
       */
      if ((dwArraySize % dwStep) != 0)
    {
      vals[dwArraySize - (dwArraySize % dwStep)] =
        trackingHash (dwArraySize - (dwArraySize % dwStep), dwIter);
    }

      for (DWORD dwPos = dwArraySize - (dwArraySize % dwStep) - dwStep;
       /* see break */ ;
       dwPos -= dwStep)
    {
      vals[dwPos] = trackingHash (dwPos, dwIter);
      vals[dwPos + dwStep - 1] = trackingHash (dwPos + dwStep - 1, dwIter);

      if (dwPos == 0)
        {
          /*
           * Working with DWORDS, so we can't use negative as a flag.
           * Furthermore, we need to end after we execute the zero case,
           * not before.
           */
          break;
        }
    }

      /* now, read the values back and make sure that they are correct */
      for (DWORD dwPos = dwArraySize - 1; /* see break */ ; dwPos --)
    {
      /* value that we expect to get; calculated from the correct hash */
      DWORD dwExpectedValue = BAD_VAL;

      /* string representing which hash was used */
      const TCHAR * szType = NULL;

      if ((dwPos % dwStep == 0) || ((dwPos % dwStep) == (dwStep - 1)))
        {
          /* this value was one that we twiddled */
          dwExpectedValue = trackingHash (dwPos, dwIter);
          szType = _T("twiddled");
        }
      else
        {
          /* this value hasn't been written since the initialization phase */
          dwExpectedValue = hash1 (dwPos);
          szType = _T("guard");
        }

      if (vals[dwPos] != dwExpectedValue)
        {
          DWORD dwPhyAdd = BAD_VAL;

          /* getPhyAddress should print an error if it fails */
          /*
           * cast off the volatile, because we don't care about optimizations
           * for this error condition
           */
          if (getPhyAddress ((DWORD *) (vals + dwPos), &dwPhyAdd) == TRUE)
        {
          Error (_T("%s val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = 0x%x"),
             szType,
             dwPos, dwPos, dwExpectedValue, vals[dwPos], dwPhyAdd);
        }
          else
        {
          Error (_T("%s val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = UNKNOWN"),
             szType,
             dwPos, dwPos, dwExpectedValue, vals[dwPos]);
        }

          /*
           * record that we have an error, but don't bail out
           * because we want to print all values that are erronous
           */
          eReturnVal = R_BAD_CACHE;

        } /* if vals aren't equal */

      if (dwPos == 0)
        {
          /*
           * Working with DWORDS, so we can't use negative as a flag.
           * Furthermore, we need to end after we execute the zero case,
           * not before.
           */
          break;
        }

    } /* checking for loop */

      if (eReturnVal != R_TRUE)
    {
      Error (_T("Test failed walking backward through the array on ")
         _T("iteration %u.  Guard values should not have been touched ")
         _T("during the test.  Twiddled values are changed by the ")
         _T("test."), dwIter);

      /* we have printed out information about the env, so now bail out */
      goto cleanAndReturn;
    }

    } /* for loop */

 cleanAndReturn:

  /* return a boolean */
  if (eReturnVal == R_TRUE)
    {
      return (TRUE);
    }
  else
    {
      return (FALSE);
    }
}


/***************************************************************************
 *
 *  Cache Test Vary Powers of Two
 *
 ***************************************************************************/

/*
 * See the header file for more information.
 */
BOOL
cacheTestVaryPowersOfTwo(__inout_ecount(dwArraySize) volatile DWORD * vals, DWORD dwArraySize,
             DWORD dwNumIterations)
{
  if (vals == NULL)
    {
      IntErr (_T("cacheTestVaryPowersOfTwo: vals == NULL"));
      return (FALSE);
    }

  if (dwArraySize < 8)
    {
      IntErr (_T("cacheTestVaryPowersOfTwo: dwArraySize < 8"));
      return (FALSE);
    }

  if (isPowerOfTwo (dwArraySize) != TRUE)
    {
      IntErr (_T("cacheTestVaryPowersOfTwo: isPowerOfTwo (dwArraySize) != TRUE"));
      return (FALSE);
    }

  /* assume true until proven otherwise */
  RETURN_VAL eReturnVal = R_TRUE;

  /* step size for the for loop */
  DWORD dwStep = BAD_VAL;

  for (DWORD dwIteration = 0; dwIteration < dwNumIterations; dwIteration++)
    {
      /*
       * Start with a step size of 4.  Ideally, we could use 1.  The
       * algorithm below, though, assumes that each value is written
       * at most once before it is read back.  Using a step size less
       * than 4, while accessing the values one below and one above,
       * will cause the algorithm to access values twice, thereby
       * breaking the test.  The smallest size at which this doesn't
       * occur is 4.
       */
      for (dwStep = 4; dwStep < dwArraySize; dwStep *= 2)
    {
      /*
       * expected value from the hash functions for the checking
       * part of the code
       */
      DWORD dwExpectedValue = BAD_VAL;

      /* position in the array */
      DWORD dwRel = BAD_VAL;

      /*
       * we could zero memory here, but if we use a tracking value
       * then we don't have to.  Zeroing memory will also clear
       * the cache, which might hide bugs.  It is a tradeoff (we
       * could do both).
       */

      /* catch zero and one */
      vals[0] = trackingHash (0, dwIteration);
      vals[1] = trackingHash (1, dwIteration);

      /*
       * start past zero so that the dwDelta doesn't cause a negative
       * index.  End before the last elements so that we don't overrun
       * the array's end.
       */
      for (DWORD dwPos = dwStep; dwPos < (dwArraySize - dwStep);
           dwPos += dwStep)
        {
          for (INT dwDelta = -1; dwDelta <= 1; dwDelta++)
        {
          vals[dwPos + dwDelta] =
            trackingHash (dwPos + dwDelta, dwIteration);
        }
        }

      /* catch last two on the end */
      PREFAST_SUPPRESS (420, "prefast false positive");
      vals[dwArraySize - 2] = trackingHash (dwArraySize - 2, dwIteration);
      vals[dwArraySize - 1] = trackingHash (dwArraySize - 1, dwIteration);

      /* now, read the values back and make sure that they are correct */

      /*
       * catch zero and one
       */
      for (dwRel = 0; dwRel <= 1; dwRel++)
        {
          if (vals[dwRel] !=
          (dwExpectedValue = trackingHash (dwRel, dwIteration)))
        {
          DWORD dwPhyAdd;

          /*
           * cast off the volatile, because we don't care
           * about optimizations for this error condition
           */
          if (getPhyAddress ((DWORD *)(vals + dwRel), &dwPhyAdd) == TRUE)
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = 0x%x"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos],
                 dwPhyAdd);
            }
          else
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = UNKNOWN"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos]);
            }

          /* don't bail yet so that all bad values are printed */
          eReturnVal = R_BAD_CACHE;
        }
        }

      /*
       * start past zero so that the dwDelta doesn't cause a negative
       * index.  End before the last elements so that we don't overrun
       * the array's end.
       */
      for (DWORD dwPos = dwStep; dwPos < (dwArraySize - dwStep);
           dwPos += dwStep)
        {
          for (INT dwDelta = -1; dwDelta <= 1; dwDelta++)
        {
          dwRel = dwPos + dwDelta;

          if (vals[dwRel] !=
              (dwExpectedValue = trackingHash (dwRel, dwIteration)))
            {
              DWORD dwPhyAdd;

              /*
               * cast off the volatile, because we don't care
               * about optimizations for this error condition
               */
              if (getPhyAddress ((DWORD *)(vals + dwRel), &dwPhyAdd)
              == TRUE)
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = 0x%x"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos],
                 dwPhyAdd);
            }
              else
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = UNKNOWN"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos]);
            }

              /* don't bail yet so that all bad values are printed */
              eReturnVal = R_BAD_CACHE;
            }
        }
        }

      /* catch last two on the end */
      for (dwRel = dwArraySize - 2; dwRel <= dwArraySize - 1; dwRel++)
        {
          if (vals[dwRel] !=
          (dwExpectedValue = trackingHash (dwRel, dwIteration)))
        {
          DWORD dwPhyAdd;

          /*
           * cast off the volatile, because we don't care
           * about optimizations for this error condition
           */
          if (getPhyAddress ((DWORD *)(vals + dwRel), &dwPhyAdd) == TRUE)
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = 0x%x"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos],
                 dwPhyAdd);
            }
          else
            {
              Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
                 _T("Got = 0x%.8x   Physical Address = UNKNOWN"),
                 dwRel, dwRel, dwExpectedValue, vals[dwPos]);
            }

          /* don't bail yet so that all bad values are printed */
          eReturnVal = R_BAD_CACHE;
        }
        }

      if (eReturnVal != R_TRUE)
        {
          Error (_T("Test failed on iteration %u using step size %u."),
             dwIteration, dwStep);

          /*
           * we have printed out information about the env, so now
           * bail out
           */
          goto cleanAndReturn;
        }

    } /* stepping for loop */
    }/* dwIteration */

 cleanAndReturn:

  /* return a boolean */
  if (eReturnVal == R_TRUE)
    {
      return (TRUE);
    }
  else
    {
      return (FALSE);
    }
}

/***************************************************************************
 *
 *  Cache Test Write Everything Read Everything Mixed Order
 *
 ***************************************************************************/

/*
 * Please see the header file for more information.
 */
BOOL
cacheTestWriteEverythingReadEverythingMixedUp(volatile DWORD * vals,
                          DWORD dwArraySize,
                          DWORD dwIterations)
{
  if (vals == NULL)
    {
      IntErr (_T("cacheTestWriteEverythingReadEverythingMixedUp: vals == NULL"));
      return FALSE;
    }

  if (dwArraySize == 0)
    {
      IntErr (_T("cacheTestWriteEverythingReadEverythingMixedUp: ")
          _T("dwArraySize == 0"));
      return FALSE;
    }

  if (isPowerOfTwo (dwArraySize) != TRUE)
    {
      IntErr (_T("cacheTestWriteEverythingReadEverythingMixedUp: ")
          _T("isPowerOfTwo (dwArraySize) != TRUE"));
      return FALSE;
    }

  /* needed for the randomization function */
  DWORD dwNumBits = getNumBitsToRepresentNum (dwArraySize);

  /* assume true until proven otherwise */
  RETURN_VAL eReturnVal = R_TRUE;

  /* current loop iteration */
  DWORD dwIter = UNINIT_DW;

  DWORD dwCurrentRandom = UNINIT_DW;

  /*
   * run through the loop dwIterations, checking after each load pass
   * whether the values are correct.
   */
  for (dwIter = 0; dwIter < dwIterations; dwIter++)
    {
      /*
       * we could zero memory here, but if we use a tracking value then we
       * don't have to.  Zeroing memory will also clear the cache, which might
       * hide bugs.  It is a tradeoff (we could do both).
       */

      /*
       * current value for the mixing function
       * start this at zero because the mixing function can take any value.
       *
       * We could set this to something based from the dwIter, and
       * that would make it random between runs.  For now we really
       * don't want this test to do this.  If you do this, you have to
       * reset it; you can't just let it rollover, because it will be
       * zero at the end of the loop below.  dwNumBits is calculated
       * from the dwArraySize, so the period is equal to dwArraySize.
       *
       * Something such as currentRandom = (dwIter * 999) is completely
       * arbitrary and should mix stuff up enough.
       */
      dwCurrentRandom = 0;

      /*
       * load the values into memory
       */
      for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos++)
    {
      /*
       * use currentRandom first, since we want to grab the value set
       * in the initialization.
       *
       * currentRandom won't wrap around, because we are reading
       * exactly dwArraySize values, and that is the size of the
       * space.
       *
       * The trackingHash is independent of dwPos, luckily.  dwPos
       * is only here to tell us when we are done.
       */
      vals[dwCurrentRandom] = trackingHash (dwCurrentRandom, dwIter);

      dwCurrentRandom = getNextMixed (dwCurrentRandom, dwNumBits);
      if( dwCurrentRandom >= dwArraySize ) {
          Error( _T("Bad value of dwCurrentRandom returned from getNextMixed\r\n"));
          eReturnVal = R_FALSE;
          goto cleanAndReturn;
      }


    }

      /* now, read the values back and make sure that they are correct */

      dwCurrentRandom = 0;

      /*
       * read the values back.
       */
      for (DWORD dwPos = 0; dwPos < dwArraySize; dwPos++)
    {
      DWORD dwExpectedValue = BAD_VAL;

      if (vals[dwCurrentRandom] !=
          (dwExpectedValue = trackingHash (dwCurrentRandom, dwIter)))
        {
          DWORD dwPhyAdd = BAD_VAL;

          /* getPhyAddress should print an error if it fails */
          /*
           * cast off the volatile, because we don't care about optimizations
           * for this error condition
           */
          if (getPhyAddress ((DWORD *)(vals + dwPos), &dwPhyAdd) == TRUE)
        {
          Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = 0x%x"),
             dwPos, dwPos, dwExpectedValue, vals[dwPos], dwPhyAdd);
        }
          else
        {
          Error (_T("val[%u = 0x%x]:  Wanted = 0x%.8x  ")
             _T("Got = 0x%.8x  Physical Address = UNKNOWN"),
             dwPos, dwPos, dwExpectedValue, vals[dwPos]);
        }

          /*
           * record that we have an error, but don't bail out
           * because we want to print all values that are erronous
           */
          eReturnVal = R_BAD_CACHE;
        }

      /* calculate next random position */
      dwCurrentRandom = getNextMixed (dwCurrentRandom, dwNumBits);
      if( dwCurrentRandom >= dwArraySize ) {
          Error( _T("Bad value of dwCurrentRandom returned from getNextMixed\r\n"));
          eReturnVal = R_FALSE;
          goto cleanAndReturn;
      }

    } /* checking for loop */

      if (eReturnVal != R_TRUE)
    {
      Error (_T("Test failed on iteration %u."),
         dwIter);

      /* we have printed out information about the env, so now bail out */
      goto cleanAndReturn;
    }

    } /* dwIter for loop */

 cleanAndReturn:

  /* return a boolean */
  if (eReturnVal == R_TRUE)
    {
      return (TRUE);
    }
  else
    {
      return (FALSE);
    }

}

/*
 * Returns a hash of the incoming value.
 *
 * This value is not guaranteed to be non-clustering.  It isn't
 * random, so don't use it for searching and the like.  It easily
 * allows us to assign different values to different cells.  It is
 * very unlikely that a bug will mistakenly return the value that we
 * are looking for.
 *
 * Observant readers will notice that this is a modified version of
 * the ANSI C rand function.
 *
 * Send in the DWORD to be hashed.
 */
DWORD
hash1(DWORD key)
{
  /* overflow doesn't matter and will actually help */
  return (1103515425 * key + 12345);

  /*
   * if you know that you have an error and are trying to debug, you
   * might want to comment out the above and use this.  It might not
   * catch the really convoluted bugs, so we don't use it by default.
   */
  /*
    return (key);
  */
}

/*
 * This hash is used for tracking values that we want to differ from
 * run to run.  If the values differ for each run then we don't have
 * to set the cache to a known value before running the tests.
 *
 * The hash below is designed to produce a different value for a given
 * dwVal for each run.  It is not designed to be random or to work with
 * a hash table.
 *
 * 40169 is chosen arbitrarily.  It is a prime close to 40,000.  We add
 * one to dwVal so that trackingHash (0, 0) won't return zero.
 *
 * Returns the hash for the given dwVal and iteration.
 */
DWORD
trackingHash (DWORD dwVal, DWORD dwIter)
{
  return (dwIter + (dwVal + 1) * 40169);

  /*
   * if you know that you have an error and are trying to debug, you
   * might want to comment out the above and use this.  It might not
   * catch the really convoluted bugs, so we don't use it by default.
   */
  /*
    return (dwVal + (dwIter + 1) * 40169);
  */
}

/* reset to whatever the user requested */
#ifdef ALWAYS_OPTIMIZE
#pragma optimize("", off)
#endif

/*
 * Convert a virtual address into a physical address.
 *
 * This function figures out the physical address corresponding to the
 * a given virtual address.  This address can't immediately be used
 * for memory accesses in it's raw form.  It must be converted into a
 * uncached or cached address to be used in this way.
 *
 * pdwVirtual is the virtual pointer that is being converted.
 * dwPhysical is the corresponding physical address, as calculated by
 * this function.  This value is represented as a DWORD, as opposed to
 * a DWORD *, because it can't be used for anything until it is
 * converted into a cached or uncached address.
 *
 * This function returns TRUE on success and FALSE with an error
 * message on failure.
 */
BOOL
getPhyAddress (DWORD * pdwVirtual, DWORD * pdwPhysical)
{
  if (pdwPhysical == NULL)
    {
      return (FALSE);
    }

  /* assume no error until proven otherwise */
  BOOL bReturnVal = TRUE;

  DWORD dwPageTableEntry = 0;

  /* query the page tables */
  if (LockPages (pdwVirtual, sizeof (DWORD),
         &dwPageTableEntry, LOCKFLAG_QUERY_ONLY) != TRUE)
    {
      Error (_T("getPhyAddress: Call to LockPages failed.  ")
         _T("Virtual address was 0x%x."), (DWORD) pdwVirtual);
      bReturnVal = FALSE;
      goto cleanAndReturn;
    }

  DWORD dwPageTablePart = 0;

  /* shift left to make into a real address */
  dwPageTablePart = dwPageTableEntry << UserKInfo[KINX_PFN_SHIFT];

  /* figure out the offset in the page */
  DWORD dwOffsetInPage = ((DWORD) pdwVirtual) & (~UserKInfo[KINX_PFN_MASK]);

  /* combine the page table part and offset to create the address */
  *pdwPhysical = dwPageTablePart | dwOffsetInPage;

 cleanAndReturn:

  return (bReturnVal);
}

