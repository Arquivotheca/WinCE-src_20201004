////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////


#include "main.h"
#include "globals.h"
#include "utility.h"

const DWORD		DO_NOT_CHECK			= -1;

const unsigned	ERROR_UNDEFINED			= DO_NOT_CHECK;

extern	const	wchar_t * sinfile; 
extern	const	wchar_t * soutfile; 
extern	const	wchar_t * sfailifexist; 
extern	const	wchar_t * sprogress; 
extern	const	wchar_t * scancel; 
extern	const	wchar_t * stoolong; 
extern	const	wchar_t * sretval;
extern	const	wchar_t * sqmark;
extern	const	wchar_t * swhackwhackqmarkwhack;


bool CompareFiles(HANDLE h0, HANDLE h1);

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
	g_pKato->Log(LOG_COMMENT, TEXT("FDIFF:a utility for simple file comparisons + existance checking"));
    g_pKato->Log(LOG_COMMENT, TEXT("Usage:"));
	g_pKato->Log(LOG_COMMENT, TEXT("%s [file_name | %s], \n\twhere file_name is a file name, %s means use a too-long (generated) name, leave empty to use an empty string for a file name"), sinfile, stoolong, stoolong);
	g_pKato->Log(LOG_COMMENT, TEXT("%s [file_name | %s], \n\twhere file_name is a file name, %s means use a too-long (generated) name, leave empty to use an empty string for a file name"), soutfile, stoolong, stoolong);
	g_pKato->Log(LOG_COMMENT, TEXT("%s means means file should not exist, use it to check something was NOT created..."), sfailifexist);
	g_pKato->Log(LOG_COMMENT, TEXT("...by comparing it to itself with %s flag, PASS means it is not present, FAIL means it is ... "), sfailifexist);
	g_pKato->Log(LOG_COMMENT, TEXT("To test for existance of a file, compare it to itself, PASS means it exists, FAIL means it does not"));
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
TESTPROCAPI FDIFF(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

	WIN32_FILE_ATTRIBUTE_DATA	fadIn = {0,}, fadOut = {0,};

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
	HANDLE		hIn = INVALID_HANDLE_VALUE, hOut = INVALID_HANDLE_VALUE;
	
	if(!GetStringFromCmdLine(g_pShellInfo->szDllCmdLine, sInFileBuf, CHARCOUNT(sInFileBuf), sinfile))
		sInFileBuf[0] = 0; //use empty string

	if(!GetStringFromCmdLine(g_pShellInfo->szDllCmdLine, sOutFileBuf, CHARCOUNT(sOutFileBuf), soutfile))
		sOutFileBuf[0] = 0; //use empty string

	bool		bCheckExistance = false;
	bCheckExistance = FlagIsPresentOnCmdLine(g_pShellInfo->szDllCmdLine, sfailifexist);
		
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

	int nRet = TPR_PASS;

	hIn = CreateFile(
		sInFile, 
		GENERIC_READ, 
		FILE_SHARE_READ, 
		0, 
		OPEN_EXISTING, 
		0, 
		0); 

	if(hIn == INVALID_HANDLE_VALUE)
	{
		if(bCheckExistance)
			nRet = TPR_PASS; 
		else
			nRet = TPR_FAIL; 

		g_pKato->Log(LOG_COMMENT, TEXT("Cannot open [%s] as first file, reason %d"), sInFile, GetLastError());
		goto Exit;
	}
	else
	{
		if(bCheckExistance)
		{
			nRet = TPR_FAIL; 
			goto Exit;
		}
		else
			nRet = TPR_PASS; 
		
	}
		

	hOut = CreateFile(
		sOutFile, 
		GENERIC_READ, 
		FILE_SHARE_READ, 
		0, 
		OPEN_EXISTING, 
		0, 
		0); 

	if(hOut == INVALID_HANDLE_VALUE)
	{
		nRet = TPR_FAIL; 
		
		g_pKato->Log(LOG_COMMENT, TEXT("Cannot open [%s] as second file, reason %d"), sOutFile, GetLastError());
		goto Exit;
	}
		

	DWORD dwAttrIn = GetFileAttributes(sInFile), dwAttrOut = GetFileAttributes(sOutFile);
	if(dwAttrIn != dwAttrOut)
	{
		nRet = TPR_FAIL; 
		g_pKato->Log(LOG_COMMENT, TEXT("Attributes are nonidentical, first file 0%o, second file 0%o"), dwAttrIn, dwAttrOut);
		//goto Exit;
	}

	
	if(!GetFileAttributesEx(sInFile, GetFileExInfoStandard, &fadIn))
	{
		nRet = TPR_FAIL; 
		g_pKato->Log(LOG_COMMENT, TEXT("Cannot get attributes of [%s] as first file"), sInFile);
		goto Exit;
	}

	if(!GetFileAttributesEx(sOutFile, GetFileExInfoStandard, &fadOut))
	{
		nRet = TPR_FAIL; 
		g_pKato->Log(LOG_COMMENT, TEXT("Cannot get attributes of [%s] as second file"), sOutFile);
		//goto Exit;
	}

	if(fadIn.nFileSizeLow != fadOut.nFileSizeLow || fadIn.nFileSizeHigh != fadOut.nFileSizeHigh)
	{
		nRet = TPR_FAIL; 
		g_pKato->Log(LOG_COMMENT, TEXT("File sizes do not match"));
		//goto Exit;
	}

	if(!CompareFiles(hIn, hOut))
	{
		nRet = TPR_FAIL; 
		g_pKato->Log(LOG_COMMENT, TEXT("File contents are non-identical"));
		//goto Exit;
	}
	

Exit:
	if(hIn != INVALID_HANDLE_VALUE)
		CloseHandle(hIn);

	if(hOut != INVALID_HANDLE_VALUE)
		CloseHandle(hOut);


	if(sLongIn) free(sLongIn); 
	if(sLongOut) free(sLongOut); 
	
	return nRet; 
}


bool CompareFiles(HANDLE h0, HANDLE h1)
{
	unsigned char c0 = 0, c1 = 0; 
	BOOL	b0, b1;
	DWORD	dw0 = 0, dw1 = 0;
	for(;;)
	{

		b0 = ReadFile(h0, &c0, 1, &dw0, 0); 
		b1 = ReadFile(h1, &c1, 1, &dw1, 0); 
		
		if(b0 != b1) 
			return false; 

		if(dw0 != dw1) 
			return false; 

		if(!dw0 && !dw1) 
			break;

		
		if(c0 != c1) 
			return false;
		
		if(!b0 || !b1)
			break; 
	}
	return true;
}