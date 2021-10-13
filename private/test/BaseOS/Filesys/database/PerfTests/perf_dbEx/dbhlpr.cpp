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
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>
#include "dbhlpr.h"


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   L_Hlp_GetRecordCount
//
//  Gets the number of records gived a DB handle. 
//  It does this by side effect by seeking to the end and getting the dwindex
//  Return Val : returns TRUE on success (FALSE otherwise) 
//
//
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_GetRecordCount_FromDBOidEx(CEGUID& VolGUID, CEOID DBOid, DWORD *pdwRecordCount)
{
	BOOL fRetVal=FALSE;
	CEOIDINFO DBInfo;

	// Query the num of sort orders
	if (!CeOidGetInfoEx(&VolGUID, DBOid, &DBInfo))
		GENERIC_FAIL(CeOidGetInfo);
	
	if (DBInfo.wObjType != OBJTYPE_DATABASE)
		goto ErrorReturn;
		
	*pdwRecordCount = DBInfo.infDatabase.wNumRecords;
	fRetVal=TRUE;
	
ErrorReturn :
	return fRetVal;
	
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   Hlp_GetSysTimeAsFileTime
//
//  Returns TRUE on success (or FALSE otherwise)
//  returns the current SYSTEMTIME as a FILETIME
//
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_GetSysTimeAsFileTime(FILETIME *retFileTime)
{
	SYSTEMTIME  mySysTime;
	
	GetSystemTime(&mySysTime);
	if (!SystemTimeToFileTime(&mySysTime, retFileTime))
		GENERIC_FAIL(SystemTimeToFileTime);
	
	return TRUE;
ErrorReturn :
	return FALSE;
}


///////////////////////////////////////////////////////////////////////////////
//      Proposed Flags :
//      HLP_FILL_RANDOM
//      HLP_FILL_SEQUENTIAL
//      HLP_FILL_DONT_CARE  -   fastest
//  
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags)
{
	DWORD i=0;
	
	switch (dwFlags)
	{
		case HLP_FILL_RANDOM :
			for (i=0; i<cbBuffer; i++)
			{
				pbBuffer[i] = (BYTE)Random();
			}
			break;
		case HLP_FILL_SEQUENTIAL :
			for (i=0; i<cbBuffer; i++)
			{
				pbBuffer[i] = (BYTE)i;
			}
			break;
		case HLP_FILL_DONT_CARE :
			memset(pbBuffer, 33, cbBuffer);
		break;
	}
	
	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function    :   Hlp_GenStringData
//
//  Description :   generates string data. 
//
//  Params : 
//  dwFlags     TST_FLAG_READABLE
//                  TST_FLAG_RANDOM (default)
//                  TST_FLAG_ALPHA
//                  TST_FLAG_ALPHA_NUM
//
//  cChars      is the count of characters that the buffer
//                  can hold, including the NULL character.
//
//
///////////////////////////////////////////////////////////////////////////////
LPTSTR  Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags)
{
	UINT i=0;
	BYTE bData=0;
	BOOL fDone=FALSE;

	ASSERT(pszString);
	ASSERT(cChars);
	
	if (!cChars)
		return NULL;
	
	// Generate cChars-1 characters (leaving space for the terminating NULL)
	for (i=0; i<cChars-1; i++)
	{
		fDone = FALSE;
		while(!fDone)
		{
			bData=(BYTE)Random();
			if (bData<0) bData *= (BYTE)-1;
			bData = bData % 0xFF;  //  generate random chars between 0 and 255

			switch (dwFlags)
			{
				case TST_FLAG_READABLE :
					if ((bData >= 32) && (bData <= 126))
						fDone=TRUE;
				break;
				case TST_FLAG_ALPHA_NUM :
					if ((bData >= '0') && (bData <= '9')
						|| (bData >= 'a') && (bData <= 'z')
						|| (bData >= 'A') && (bData <= 'Z'))
						fDone=TRUE;
				break;
				case TST_FLAG_ALPHA :
					if ((bData >= 'a') && (bData <= 'z')
						|| (bData >= 'A') && (bData <= 'Z'))
						fDone=TRUE;
				break;
				case TST_FLAG_NUMERIC :
					if ((bData >= '0') && (bData <= '9'))
						fDone=TRUE;
				break;

				default : 
					TRACE(_T("Should never reach here. Unknown Flags\n"));
					ASSERT(FALSE);
					GENERIC_FAIL(Hlp_GenStringData);
			}
		}
		pszString[i] = (TCHAR) bData;
	}// for 

	// NULL terminate
	pszString[i] = _T('\0');

	return pszString;
ErrorReturn :
	return NULL;

}


void Hlp_GenRandomFileTime(FILETIME *myFileTime)
{
	// Generate random time
	(*myFileTime).dwLowDateTime = Random();
	(*myFileTime).dwHighDateTime = Random();
}


///////////////////////////////////////////////////////////////////////////////
//
//      Displays the systems current memory division
//
///////////////////////////////////////////////////////////////////////////////
void Hlp_DisplayMemoryInfo(void)
{
	DWORD dwStorePages=0;
	DWORD dwStorePages_Free=0;

	DWORD dwRamPages=0;
	DWORD dwRamPages_Free=0;
	
	DWORD dwPageSize=0;
	STORE_INFORMATION StoreInfo;
	MEMORYSTATUS memStatus;

	memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));
	memset((PBYTE)&memStatus, 0, sizeof(MEMORYSTATUS));

	// Get the page division information
	if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
		GENERIC_FAIL(GetSystemMemoryDivision);


	// Get information about store pages. 
	if (!GetStoreInformation(&StoreInfo))
		GENERIC_FAIL(GetStoreInformation);

	// Get information about memory pages
	GlobalMemoryStatus(&memStatus);

	dwRamPages_Free = memStatus.dwAvailPhys/dwPageSize;
	dwStorePages_Free = StoreInfo.dwFreeSize/dwPageSize;
	
	TRACE(_T("SystemMemory Info :\n"));
	TRACE(_T("StorePages : Free= %6d Used= %6d Total= %6d\n"), 
					dwStorePages_Free, 
					dwStorePages - dwStorePages_Free,
					dwStorePages);
					
	TRACE(_T("RamPages   : Free= %6d Used= %6d Total= %6d\n"), 
					dwRamPages_Free, 
					dwRamPages - dwRamPages_Free,
					dwRamPages);
	TRACE(_T("PageSize = %d bytes/page \n"), dwPageSize);

ErrorReturn :
	;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Returns the Num of Pages of Free storage mem 
//
///////////////////////////////////////////////////////////////////////////////
DWORD   Hlp_GetFreeStorePages(void)
{
	DWORD dwRetVal=0;
	DWORD dwStorePages=0;
	DWORD dwRamPages=0;
	DWORD dwPageSize=0;
	STORE_INFORMATION StoreInfo;

	memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));

	// Get the page division information
	if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
		GENERIC_FAIL(GetSystemMemoryDivision);


	// Get information about store pages. 
	if (!GetStoreInformation(&StoreInfo))
		GENERIC_FAIL(GetStoreInformation);

	dwRetVal = StoreInfo.dwFreeSize/dwPageSize;

ErrorReturn :
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Returns the Num of Pages of Free storage mem 
//
///////////////////////////////////////////////////////////////////////////////
DWORD   Hlp_GetFreePrgmPages(void)
{
	DWORD dwRetVal=0;
	DWORD dwStorePages=0;

	DWORD dwRamPages=0;
	DWORD dwPageSize=0;

	STORE_INFORMATION StoreInfo;
	MEMORYSTATUS memStatus;

	memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));
	memset((PBYTE)&memStatus, 0, sizeof(MEMORYSTATUS));

	// Get the page division information
	if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
		GENERIC_FAIL(GetSystemMemoryDivision);


	// Get information about memory pages
	GlobalMemoryStatus(&memStatus);

	dwRetVal = memStatus.dwAvailPhys/dwPageSize;

ErrorReturn :
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Can either increase or decrease the amount of allocated Program memory
//
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_BumpProgramMem(int dwNumPages)
{
	DWORD dwStorePages=0;
	DWORD dwRamPages=0;
	DWORD dwPageSize=0;
	DWORD dwAdjust=0;
	DWORD dwResult=0;
	BOOL fRetVal=0;

	dwAdjust = dwNumPages;

	if (dwNumPages>0)
		TRACE(_T("Increasing Program Mem by %d pages\n"), dwNumPages);
	else
		TRACE(TEXT("Reducing Program Mem by %d pages\r\n"), -1*dwNumPages);

	// Get the system memory information
	if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
		GENERIC_FAIL(GetSystemMemoryDivision);

	dwResult = SetSystemMemoryDivision(dwStorePages-dwAdjust);
	switch(dwResult)
	{
		case SYSMEM_FAILED:
			TRACE( _T( "SetSystemMemoryDivision: ERROR value out of range!\n" ) );
			goto ErrorReturn;
		break;

		case SYSMEM_MUSTREBOOT:
		case SYSMEM_REBOOTPENDING:
			TRACE( _T( "SetSystemMemoryDivision: ERROR reboot request!\r\n" ) );
			goto ErrorReturn;
		break;
	}

	fRetVal=TRUE;

ErrorReturn :
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Reduces the allocated amount of program memory on the system by the 
//  dwNumPages number of pages. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_ReduceProgramMem(int dwNumPages)
{
	return Hlp_BumpProgramMem(-dwNumPages);
}


///////////////////////////////////////////////////////////////////////////////
//
//  This sets the system memory division so that we are left with 
//  dwNumPagesToLeave amount of Program memory. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_EatAll_Prgm_MemEx(DWORD dwNumPagesToLeave)
{
	BOOL fRetVal=FALSE;
	DWORD dwFreePrgmPages = 0;

	dwFreePrgmPages = Hlp_GetFreePrgmPages();
	if (dwFreePrgmPages < dwNumPagesToLeave)
	{
		TRACE(TEXT(">> WARNING : Num of Free Prgm pages = %d. Requesting to set to %d\r\n"), dwFreePrgmPages, dwNumPagesToLeave);
	}

	dwFreePrgmPages -= dwNumPagesToLeave;
	if (!Hlp_ReduceProgramMem(dwFreePrgmPages))
		goto ErrorReturn;

	fRetVal=TRUE;
ErrorReturn :
	return fRetVal;
}


BOOL Hlp_SetMemPercentage(DWORD dwStorePcent, DWORD dwRamPcent)
{
	BOOL fRetVal=FALSE;
	DWORD dwStorePages=0;
	DWORD dwRamPages=0;
	DWORD dwPageSize=0;
	float dwNewStorePages=0;
	DWORD dwResult=0;
	
	ASSERT(100 == dwStorePcent+ dwRamPcent);
	if (100 != dwStorePcent+ dwRamPcent)
	{
		TRACE(TEXT(">>>>> ERROR : dwStorePCent=%d + dwRamPcent=%d is not 100 ????\r\n"));
		GENERIC_FAIL(Hlp_SetMemPercentage);
	}

	// Get the page division information
	if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
		GENERIC_FAIL(GetSystemMemoryDivision);
	

	dwNewStorePages = (float)(dwStorePages+dwRamPages)*((float)dwStorePcent/(float)100);

	TRACE(TEXT("Setting SetSystemMemoryDivision to %d storage pages\r\n"), (DWORD)dwNewStorePages);
	dwResult = SetSystemMemoryDivision((DWORD)dwNewStorePages);
	switch(dwResult)
	{
		case SYSMEM_FAILED:
			TRACE( _T( "SetSystemMemoryDivision: ERROR value out of range!\n" ) );
			goto ErrorReturn;
		break;

		case SYSMEM_MUSTREBOOT:
		case SYSMEM_REBOOTPENDING:
			TRACE( _T( "SetSystemMemoryDivision: ERROR reboot request!\r\n" ) );
			goto ErrorReturn;
		break;
	}
	
	fRetVal=TRUE;
	
ErrorReturn :
	return fRetVal;

}
