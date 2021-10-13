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
//
//  TUXTEST TUX DLL
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////


#include "main.h"
#include "globals.h"
#include "test.h"
#include "utility.h"
#include <windows.h>
#include "ProgressPackage.h"

const DWORD		DO_NOT_CHECK			= -1;
const unsigned	ERROR_UNDEFINED			= DO_NOT_CHECK;

extern	const	wchar_t * sinfile; 
extern	const	wchar_t * soutfile; 
extern	const	wchar_t * sfailifexist; 
extern	const	wchar_t * sprogress; 
extern	const	wchar_t * scancel; 
extern	const	wchar_t * stoolong; 
extern	const	wchar_t * sretval;
extern	const	wchar_t * serrexp;
extern	const	wchar_t * spassexpected;
extern	const	wchar_t * spassalways;
extern	const	wchar_t * sqmark;
extern	const	wchar_t * swhackwhackqmarkwhack;


wchar_t * AllocAndFillLongName(void);

DWORD CALLBACK ProgressRoutine(
  LARGE_INTEGER TotalFileSize,
  LARGE_INTEGER TotalBytesTransferred,
  LARGE_INTEGER StreamSize,
  LARGE_INTEGER StreamBytesTransferred,
  DWORD dwStreamNumber,
  DWORD dwCallbackReason,
  HANDLE hSourceFile,
  HANDLE hDestinationFile,
  LPVOID lpData
);

void Usage(void)
{
	g_pKato->Log(LOG_COMMENT, TEXT(""));
    g_pKato->Log(LOG_COMMENT, TEXT("Usage:"));
	g_pKato->Log(LOG_COMMENT, TEXT("%s [file_name | %s], \n\twhere file_name is a file name, %s means use a too-long (generated) name, leave empty to use an empty string for a file name"), sinfile, stoolong, stoolong);
	g_pKato->Log(LOG_COMMENT, TEXT("%s [file_name | %s], \n\twhere file_name is a file name, %s means use a too-long (generated) name, leave empty to use an empty string for a file name"), soutfile, stoolong, stoolong);
	g_pKato->Log(LOG_COMMENT, TEXT("[%s] means use the progress routine, default is not to"), sprogress);
	g_pKato->Log(LOG_COMMENT, TEXT("[%s] means turn the 'fail if exists' flag ON during file copy, default is not to"), sfailifexist);
	g_pKato->Log(LOG_COMMENT, TEXT("[%s <retval>] sets the value to be returned from the progress function"), sretval);
	g_pKato->Log(LOG_COMMENT, TEXT("\tLegitimate values are 0->CONTINUE, 1->CANCEL, 2->STOP, 3->QUIET"));
	g_pKato->Log(LOG_COMMENT, TEXT("[%s <cancel>] sets the cancellation flag for the progress function"), scancel);
	g_pKato->Log(LOG_COMMENT, TEXT("\tLegitimate values are 0->NULL, 1->TRUE, 2->FALSE"));
	g_pKato->Log(LOG_COMMENT, TEXT("[%s <errval>] sets the expected value to be returned from GetLastError() after CopyFileEx() runs"), serrexp);
	g_pKato->Log(LOG_COMMENT, TEXT("\tGood values are 0->NO_ERROR, 2->FILE_NOT_FOUND, 5->ACCESS_DENIED; default is not to check"));
	g_pKato->Log(LOG_COMMENT, TEXT("[%s 0|1] sets the expected (BOOLEAN) value to be returned by CopyFileEx()"), spassexpected);


}

////////////////////////////////////////////////////////////////////////////////
// TestProc
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI CFEX(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

	if(!g_pShellInfo->szDllCmdLine || !*g_pShellInfo->szDllCmdLine)
	{
		Usage();
		return TPR_FAIL; 
	}

	if(FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, sqmark))
		Usage();
    
	//snag file names from command line ... 
	wchar_t		* sInFile = 0, * sOutFile = 0; 
	wchar_t		sInFileBuf[MAX_PATH] = {0,}, sOutFileBuf[MAX_PATH] = {0,}; 
	
	if(!GetStringFromCmdLine(g_pShellInfo->szDllCmdLine, sInFileBuf, CHARCOUNT(sInFileBuf), sinfile))
		sInFileBuf[0] = 0; //use empty string

	if(!GetStringFromCmdLine(g_pShellInfo->szDllCmdLine, sOutFileBuf, CHARCOUNT(sOutFileBuf), soutfile))
		sOutFileBuf[0] = 0; //use empty string

	//TODO: generate too-long names here if need be ...
	
	wchar_t * sLongIn = 0, * sLongOut = 0;
	if(!wcsicmp(sInFileBuf, stoolong))
	{
		sLongIn = AllocAndFillLongName(); 
		sInFile = sLongIn; 
	}

	if(!wcsicmp(sOutFileBuf, stoolong))
	{
		sLongOut = AllocAndFillLongName(); 
		sOutFile = sLongOut; 
	}

	sInFile = sInFileBuf; 
	sOutFile = sOutFileBuf;

	//are we using the progress routine?
	LPPROGRESS_ROUTINE	pProgressRoutine = 0;
	if(FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, sprogress))
		pProgressRoutine = ProgressRoutine; 
	else
		pProgressRoutine = 0; 

	//get cancel-in-progress flag
	//we can use a null pointer ... 
	unsigned	ulCancel = 0;
	CancelType cancel = False;
	BOOL	bCancel = false; 
	BOOL	* pbCancel = 0;

	if(FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, scancel))
		if(GetULIntFromCmdLine(g_pShellInfo->szDllCmdLine, scancel, &ulCancel))
			switch(ulCancel)
			{
				case	0:
					cancel = CancelNull; 
					break;

				case	1:
					cancel = True; 
					break;

				case	2:
					cancel = False; 
					break;

				default:
					g_pKato->Log(LOG_COMMENT, TEXT("Invalid value for Cancellation flag, use 0, 1, or 2"));
					Usage();
					return TPR_FAIL;
			};

	if(cancel == True) bCancel = TRUE; 
	if(cancel == False) bCancel = FALSE; 

	//we can use a null cancellation flag ptr ... 
	if(cancel == True || cancel == False)
		pbCancel = &bCancel; 
	else
		pbCancel = 0;
					
	//get copy flags, default is 0
	//TODO:support other flags when CE does ...
	unsigned		dwCopyFlags		= 0;
	if(FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, sfailifexist))
		dwCopyFlags |= COPY_FILE_FAIL_IF_EXISTS;
	
	//get copy flags, default is 0
	unsigned		dwDesiredReturn		= 0;

	//does not touch dwDesiredReturn if fail, OK
	GetULIntFromCmdLine(g_pShellInfo->szDllCmdLine, sretval, &dwDesiredReturn); 

	
	unsigned dwErrExp = DO_NOT_CHECK;
	//does not touch dwErrExp if fail, OK
	GetULIntFromCmdLine(g_pShellInfo->szDllCmdLine, serrexp, &dwErrExp); 


	switch(dwDesiredReturn)
	{
	case	0:
		dwDesiredReturn = PROGRESS_CONTINUE; 
		break;


	case	1:
		dwDesiredReturn = PROGRESS_CANCEL; 
		break;

	case	2:
		dwDesiredReturn = PROGRESS_STOP; 
		break;

	case	3:
		dwDesiredReturn = PROGRESS_QUIET; 
		break;

	default:
		g_pKato->Log(LOG_COMMENT, TEXT("Invalid value for retval flag, use 0, 1, 2, or 3"));
		Usage();
		return TPR_FAIL;
	}


	ProgressPackage pp(dwCopyFlags, cancel, dwDesiredReturn);

	BOOL b = CopyFileEx(sInFile, sOutFile, pProgressRoutine, &pp, pbCancel, dwCopyFlags);

	//check pp.nRet here, if it fails ... do something
	//check GetLastError() here, test against expected error return, default is 0
	DWORD dwErr = GetLastError();

	if(sLongIn) free(sLongIn); 
	if(sLongOut) free(sLongOut); 

	unsigned ulPassExpected = DO_NOT_CHECK; 
	GetULIntFromCmdLine(g_pShellInfo->szDllCmdLine, spassexpected, &ulPassExpected);
	
	
	unsigned ulPassAlways = false; 
	GetULIntFromCmdLine(g_pShellInfo->szDllCmdLine, spassalways, &ulPassAlways);
	if(ulPassAlways) 
		return TPR_PASS; 
	else
	{
		if(pp.bRet == false)
		{
			g_pKato->Log(LOG_COMMENT, TEXT("Progress function reported an error"));
			return TPR_FAIL; 
		}

		if(dwErrExp != DO_NOT_CHECK && dwErrExp != dwErr)
		{
			g_pKato->Log(LOG_COMMENT, TEXT("Expected GetLastError() to return %d, got %d instead"), dwErrExp, dwErr);
			return TPR_FAIL; 
		}

		if(ulPassExpected != DO_NOT_CHECK && ((ulPassExpected && !b) || (!ulPassExpected && b)))
		{
			g_pKato->Log(LOG_COMMENT, TEXT("Got %d from CopyFileEx(), expected %d"), b, ulPassExpected);
			return TPR_FAIL; 
		}

		return TPR_PASS;

	}
}

DWORD CALLBACK ProgressRoutine(
  LARGE_INTEGER TotalFileSize,
  LARGE_INTEGER TotalBytesTransferred,
  LARGE_INTEGER StreamSize,
  LARGE_INTEGER StreamBytesTransferred,
  DWORD dwStreamNumber,
  DWORD dwCallbackReason,
  HANDLE hSourceFile,
  HANDLE hDestinationFile,
  LPVOID lpData
)
{
	ProgressPackage * pp = (ProgressPackage *)lpData;

	bool b = pp->Evaluate(
				 TotalFileSize, 
				 TotalBytesTransferred, 
				 StreamSize, 
				 StreamBytesTransferred, 
				 dwStreamNumber, 
				 dwCallbackReason, 
				 hSourceFile, 
				 hDestinationFile
				);

	if(!b)
		g_pKato->Log(LOG_COMMENT, TEXT("Progress routine will report FAIL"));

	
	//hold them for later
	pp->StoreParams(
				 TotalFileSize, 
				 TotalBytesTransferred, 
				 StreamSize, 
				 StreamBytesTransferred, 
				 dwStreamNumber, 
				 dwCallbackReason, 
				 hSourceFile, 
				 hDestinationFile
				);

	pp->Print();

	

	pp->dwLastReturned = pp->dwRequestedReturn;
	return pp->dwLastReturned;
}


