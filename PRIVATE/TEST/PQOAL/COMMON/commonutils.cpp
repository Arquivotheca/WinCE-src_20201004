//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * commonUtils.cpp
 */

#include "commonUtils.h"

/*
 * Command line for tux dll.  Set in the tuxStandard file in the main
 * SHELLPROC processing function.
 */
LPCTSTR g_szDllCmdLine = NULL;

/***************************************************************************
 *
 *             Local Functions Not Exported Through the Header
 *
 ***************************************************************************\

/*
 * Exactly like the other circularShiftR, except this only shifts right one
 * bit.
 */
DWORD
circularShiftR (DWORD dwVal, DWORD dwLength);

/*
 * Exactly like the other circularShiftL, except this only shifts left one
 * bit.
 */
DWORD
circularShiftL (DWORD dwVal, DWORD dwLength);


/***************************************************************************
 *
 *                                Implementation
 *
 ***************************************************************************/

/*
 * For informational messages.  Treat this like printf.
 */
VOID Info(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("INFO: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   /* 
    * six is the length of "INFO: " 
    * minus two accounts for the cat in the next line
    */
   _vsntprintf(szBuffer + 6, countof(szBuffer) - 6 - 2, szFormat, pArgs);
   va_end(pArgs);

   /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
   szBuffer[countof(szBuffer) - 2 - 1] = '\0';

   TCHAR szEndL[] = _T("\r\n");
   _tcsncat(szBuffer, szEndL, _tcslen (szEndL));

   /* can't send directly in, because it will be reprocessed */
   g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);
}

/*
 * For error messages (as opposed to informational messages).  Treat
 * this like printf.
 */
VOID Error(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("ERROR: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   /*
    * seven is the length of "ERROR: " 
    * minus two accounts for the cat in the next line 
    */
   _vsntprintf(szBuffer + 7, countof(szBuffer) - 7 - 2, szFormat, pArgs);
   va_end(pArgs);
   
   /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
   szBuffer[countof(szBuffer) - 2 - 1] = '\0';

   TCHAR szEndL[] = _T("\r\n");
   _tcsncat(szBuffer, szEndL, _tcslen (szEndL));

   g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);

}


/*
 * For internal errors.  If you ever see one of these then there is a
 * .
 *
 * These are used like asserts.
 */
VOID IntErr(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("** INTERNAL ERROR **: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   /*
    * seven is the length of "ERROR: " 
    * minus two accounts for the cat in the next line 
    */
   _vsntprintf(szBuffer + 21, countof(szBuffer) - 21 - 2, szFormat, pArgs);
   va_end(pArgs);
   
   /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
   szBuffer[countof(szBuffer) - 2 - 1] = '\0';

   TCHAR szEndL[] = _T("\r\n");
   _tcsncat(szBuffer, szEndL, _tcslen (szEndL));
 
   g_pKato->Log(LOG_DETAIL, _T("%s"), szBuffer);

   DebugBreak();

}

/*
 * This can't use kato, since certian functions in the tux
 * initialization call this code before kato has been set up.  We use
 * standard debugging instead.
 *
 * Treat this just like printf.
 */
VOID Debug(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("DEBUG: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   /*
    * seven is the length of "DEBUG: "
    * minus two accounts for the cat in the next line 
    */
   _vsntprintf(szBuffer + 7, countof(szBuffer) - 7 - 2, szFormat, pArgs);
   va_end(pArgs);
   
   /*
    * if overrun buffer then not guaranteed to be null terminated, so
    * force it to be.  The - 1 grabs the location, since we index from
    * zero.  This leaves space for appending the end line in the next
    * statement.
    */
   szBuffer[countof(szBuffer) - 2 - 1] = '\0';

   TCHAR szEndL[] = _T("\r\n");
   _tcsncat(szBuffer, szEndL, _tcslen (szEndL));

   OutputDebugString(szBuffer);

}


/*
 * Gets a random number in a given range, inclusive.
 *
 * lowEnd is the lowEnd of the range.  highEnd is the high end of the
 * range
 */
DWORD
getRandomInRange (DWORD lowEnd, DWORD highEnd)
{
  if (lowEnd >= highEnd)
    {
      IntErr (_T("getRandomInRange: lowEnd >= highEnd"));
      return BAD_VAL;
    }

  /* 
   * rand returns a value between 0 and RAND_MAX (0x7fff), inclusive.
   * We need a 32 bit random number, so calculate one.
   */
  DWORD bigRandom = 
    rand () |
    (rand () << 15) |
    (rand () << 30);

  /* scale the bigRandom number to the range that we need */
  DWORD result = (DWORD) (((double) bigRandom / (double) MAX_DWORD) * 
			  (double) (highEnd - lowEnd));

  /* move this into the range that we are looking for */
  result += lowEnd;

  return (result);
}

BOOL
AminusB (ULONGLONG & answer, ULONGLONG a, ULONGLONG b, ULONGLONG dwMaxSize)
{
  if ((a > dwMaxSize) || (b > dwMaxSize))
    {
      return (FALSE);
    }

  if (a < b) 
    {
      /* 
       * handle overflow
       * adding one accounts for the transition between the maximum dword
       * and zero.
       */
      answer = ((dwMaxSize - b) + a + 1);
    }
  else
    {
      answer = a - b;
    }
  
  return (TRUE);
}

DWORD
circularShiftR (DWORD dwVal, DWORD dwLength, DWORD dwShift)
{
  if (!(dwShift <= dwLength))
    {
      IntErr (_T("circularShiftR: !(dwShift <= dwLength)"));
      return (BAD_VAL);
    }
  
  if (!((dwLength > 0) && (dwLength <= 32)))
    {
      IntErr (_T("circularShiftR: !((dwLength > 0) && (dwLength <= 32))"));
      return (BAD_VAL);
    }

  for (DWORD dwCount = 0; dwCount < dwShift; dwCount++)
    {
      dwVal = circularShiftR (dwVal, dwLength);
    }

  return (dwVal);
}

DWORD
circularShiftR (DWORD dwVal, DWORD dwLength)
{
  if (!((dwLength > 0) && (dwLength <= 32)))
    {
      IntErr (_T("circularShiftR: !((dwLength > 0) && (dwLength <= 32))"));
      return BAD_VAL;
    }

  DWORD dwEnd = dwVal & 0x1;

  dwVal >>= 1;

  if (dwEnd == 0)
    {
      CLR_BIT (dwVal, dwLength - 1);
    }
  else
    {
      SET_BIT (dwVal, dwLength - 1);
    }
  
  return (dwVal);
}
  
DWORD
circularShiftL (DWORD dwVal, DWORD dwLength, DWORD dwShift)
{
  if (!(dwShift <= dwLength))
    {
      IntErr (_T("circularShiftL: !(dwShift <= dwLength)"));
      return BAD_VAL;
    }

  if (!((dwLength > 0) && (dwLength <= 32)))
    {
      IntErr (_T("circularShiftL: !((dwLength > 0) && (dwLength <= 32))"));
      return BAD_VAL;
    }

  for (DWORD dwCount = 0; dwCount < dwShift; dwCount++)
    {
      dwVal = circularShiftL (dwVal, dwLength);
    }

  return (dwVal);
}

DWORD
circularShiftL (DWORD dwVal, DWORD dwLength)
{
  if (!((dwLength > 0) && (dwLength <= 32)))
    {
      IntErr (_T("circularShiftL: !((dwLength > 0) && (dwLength <= 32))"));
      return BAD_VAL;
    }

  DWORD dwEnd = dwVal & 0x1;

  dwVal >>= 1;

  if (dwEnd == 0)
    {
      CLR_BIT (dwVal, dwLength - 1);
    }
  else
    {
      SET_BIT (dwVal, dwLength - 1);
    }
  
  return (dwVal);
}
  


BOOL
handleThreadPriority (int iNew, int * iOld)
{ 
  if (!((iNew == DONT_CHANGE_PRIORITY) || ((iNew >= 0) && (iNew <= 255))))
    {
      IntErr (_T("handleThreadPriority: ")
	      _T("!((iNew == DONT_CHANGE_PRIORITY) || ")
	      _T("((iNew >= 0) && (iNew <= 255)))"));
      return FALSE;
    }

  INT returnVal = TRUE;
  
  /*
   * handle to the current thread.  This doesn't need to be 
   * closed when processing is complete.
   */
  HANDLE hThisThread = GetCurrentThread ();

  /* set the thread priority */
  if (iNew != DONT_CHANGE_PRIORITY)
    {
      /*
       * save the old thread priority so that we can reset the 
       * priority to this after the test.
       */
      if (iOld != NULL)
	{
	  *iOld = CeGetThreadPriority (hThisThread);
	}

      if (CeSetThreadPriority (hThisThread, iNew) == FALSE)
	{
	  Error (_T("Could not set thread priority to %i."),
		 iNew);

	  returnVal = FALSE;
	  goto cleanAndExit;
	}
    }
 cleanAndExit:
  return (returnVal);
}




/****** Time Formatting **************************************************/


/*
 * struct to store the values that we will return to the user.
 *
 * doConversion is the conversion value to go from the given line
 * to the line one less precise.  The values are organized from
 * most precise at 0 to least precise.
 */
struct
{
  double doConversion;
  _TCHAR * unitsSing;
  _TCHAR * unitsPlural;
} conversionValues[] =
  {1000.0, _T("nano-second"), _T("nano-seconds"),
   1000.0, _T("micro-second"), _T("micro-seconds"),
   1000.0, _T("millisecond"), _T("milliseconds"),   
   60.0, _T("second"), _T("seconds"),    
   60.0, _T("minute"), _T("minutes"),
   24.0, _T("hour"), _T("hours"),
   0.0, _T("day"), _T("days")};

/*
 * goes hand in hand with above.  If you add stuff before seconds you
 * need to change this.
 */
#define SECONDS_POS 3

/*
 * convert a time into a time that is more realistic to print.  This
 * allows us to easily pretty print times.  120000 seconds isn't much
 * help to the normal user without a calculator.  This makes the
 * output nicer.
 *
 * doTime is the value to convert, in seconds.  doFormatted time is
 * the resulting number, and szUnits in the units that should be
 * printed.  The units account for singular and plural.
 *
 */ 
void
realisticTime (double doTime, double & doFormattedTime, _TCHAR * szUnits[])
{
  DWORD dwCurrentSpot = SECONDS_POS;

  doFormattedTime = doTime;

  if (doTime == 0.0)
    {
      *szUnits = conversionValues[SECONDS_POS].unitsPlural;
      return;
    }

  if (doFormattedTime < 1.0)
    {
      /* we are going to have to move to more precise units */
      
      while ((doFormattedTime < 1.0) && (dwCurrentSpot > 0))
	{
	  doFormattedTime = 
	    doFormattedTime * conversionValues[dwCurrentSpot - 1].doConversion;
	  dwCurrentSpot--;
	}

      /* done */
    }
  else
    {
      /* going to have to go up */
      
      while ((doFormattedTime > (1.1 * conversionValues[dwCurrentSpot].doConversion)) && 
	     (dwCurrentSpot < (countof(conversionValues) - 1)))
	{
	  doFormattedTime /= conversionValues[dwCurrentSpot].doConversion;
	  dwCurrentSpot++;
	}

      /* done */
    }
      
  if (doFormattedTime == 1.0)
    {
      *szUnits = conversionValues[dwCurrentSpot].unitsSing;
    }
  else
    {
      *szUnits = conversionValues[dwCurrentSpot].unitsPlural;
    }
}

/*
 * return true if the given DWORD is a power of two and false if it is not.
 */
BOOL
isPowerOfTwo (DWORD val)
{
  if (val == 0)
    {
      return (FALSE);
    }

  /* 
   * the previous statement forces there to be at least one bit set in
   * val.  Bail out on that bit.
   */
  while ((val & 1) != 1)
    {
      val >>= 1;
    }

  /* 
   * at this point the one that we are looking for is sitting in the
   * lowest position.  Shift it off.
   */
  val >>= 1;

  if (val == 0)
    {
      /* only one set bit, so power of two */
      return (TRUE);
    }
  else
    {
      return (FALSE);
    }
}

/*
 * get the number of bits needed to represent a number which is a power of two
 *
 * This doesn't include the number val.  In other words, count the
 * number of zeros to the right of the one in a given number.
 */
DWORD
getNumBitsToRepresentNum (DWORD val)
{
  if (val == 0)
    {
      IntErr (_T("getNumBitsToRepresentNum: val == 0"));
      return BAD_VAL;
    }

  if (isPowerOfTwo (val) != TRUE)
    {
      IntErr (_T("getNumBitsToRepresentNum: isPowerOfTwo (val) != TRUE"));
      return BAD_VAL;
    }

  DWORD dwCount = 0;

  /* 
   * constraints force there to be at least one bit set in val
   */
  while ((val & 1) != 1)
    {
      val >>= 1;
      dwCount++;
    }

  /* don't count the last bit, because we are not inclusive. */
  return (dwCount);

}



/****** String Stuff ***************************************************/

/*
 * convert string to ULONGLONG
 * 
 * see header for more info.
 */
BOOL
strToULONGLONG(TCHAR * szStr, ULONGLONG * ullVal)
{
  if ((szStr == NULL) || (ullVal == NULL))
    {
      return (FALSE);
    }

  if (_stscanf (szStr, _T("%I64u"), ullVal) != 1)
    {
      return (FALSE);
    }

  return (TRUE);
}
