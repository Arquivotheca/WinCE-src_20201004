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
#include "PowerTest.h"

/*GLOBALS*/
TCHAR g_szPath[MAX_PATH] = _T("");
DWORD g_dwFileSize;
DWORD g_dwTransferSize;
BOOL  g_bWriteThrough;
BOOL  g_bCache;
DWORD g_dwTestTime;
TEST_TYPE g_Test_Type;

TEST_TYPE_LOOKUP ttLookup[] = 
{
      {READ,TEXT("READ")},
      {WRITE,TEXT("WRITE")},
      {READWRITE,TEXT("READWRITE")}
};

int __cdecl _tmain( int  argc , _TCHAR *  argv  [] )
{
    //build the command line
    TCHAR szCmdLine[MAX_PATH];
    szCmdLine[0] = '\0';
    for(int i = 0; i < argc; i++)
    {
        _tcscat(szCmdLine,argv[i]);
        _tcscat(szCmdLine,_T(" "));
    }
    //now parse it
    if(!ProcessCmdLine(szCmdLine))
    {
      Usage();
      return 0;
    }
    TestReadWritePower();
	 /* if (0==wcscmp(lpCmdLine,L"install"))
      {
			//start the thread

			//set a timer.  The thread will be killed by the
            MMRESULT mmResult = timeSetEvent(TIMER_LENGTH,1000,NULL,WRITE_LARGE,TIME_CALLBACK_FUNCTION);
			
	  }*/
      return 0;
}


static bool ProcessCmdLine(LPCTSTR szCmdLine)
{
    Debug(_T("CmdLine = %s\n"),szCmdLine);
    CClParse cmdLine(szCmdLine);
    TCHAR tempVal[MAX_PATH];
    //PATH
    if(!cmdLine.GetOptString(_T("path"), g_szPath, MAX_PATH))
    {
        Debug(_T("FATFSPOWER: path not specified."));
        return false;
    }
    //File Size
    if(!cmdLine.GetOptString(_T("FileSize"), tempVal,MAX_PATH))
    {
        Debug(_T("FATFSPOWER: FileSize not specified"));
        return false;
    }
    else
    {
        TCHAR* stopString;
        g_dwFileSize =  _tcstol(tempVal,&stopString,10);
        g_dwFileSize = NearestPowerOfTwo(g_dwFileSize);
        g_dwFileSize *= 512;
    }
    //Transfer Size
    if(!cmdLine.GetOptString(_T("TransferSize"), tempVal,MAX_PATH))
    {
        Debug(_T("FATFSPOWER: TransferSize not specified"));
        return false;
    }
    else
    {
        TCHAR* stopString;
        g_dwTransferSize =  _tcstol(tempVal,&stopString,10);
        g_dwTransferSize = NearestPowerOfTwo(g_dwTransferSize);
        g_dwTransferSize *= 512;
    }
    //Transfer Size
    if(!cmdLine.GetOptString(_T("TestTime"), tempVal,MAX_PATH))
    {
        Debug(_T("FATFSPOWER: TestTime not specified"));
        return false;
    }
    else
    {
        TCHAR* stopString;
        g_dwTestTime =  _tcstol(tempVal,&stopString,10);
    }
    //writethrough
    g_bWriteThrough = cmdLine.GetOpt(_T("WriteThrough"));
    //cache
    g_bCache = cmdLine.GetOpt(_T("Cache"));
    //TestType
    if(!cmdLine.GetOptString(_T("TestType"), tempVal,MAX_PATH))
    {
        Debug(_T("FATFSPOWER: TestType not specified"));
        return false;
    }
    else
    {
        g_Test_Type = UNKNOWN;
        for(int i = 0; i < sizeof(ttLookup) / sizeof(TEST_TYPE_LOOKUP); i++)
        {
            if(_tcscmp(ttLookup[i].str,_tcsupr(tempVal)) == 0)
            {
              g_Test_Type = ttLookup[i].testType;
              break;   
            }
        }
        if(g_Test_Type == UNKNOWN)
        {
          Debug(_T("TestType '%s' is unknown\n"),  tempVal);
          return false;  
        }
        Debug(_T("TestType = %d\n"),g_Test_Type);
    }
    return true;
}


static void Usage()
{    
    Debug(TEXT(""));
    Debug(TEXT("FATFSPOWER: Usage: tux -o -d fatfspower -c\"/path Storage Path /FileSize <size> /TransferSize <size> /TestType [READ,WRITE,READWRITE] [/WriteThrough /Cache]\""));
    Debug(TEXT("       /path <Storage Path>: name of the path to test (e.g. \\Hard Disk:);"));
    Debug(TEXT("       /TestTime <seconds>: Amount of time to run the test"));
    Debug(TEXT("       /FileSize <number>: Size of file (in 512 byte sectors) you want to use for read/write operations (e.g. 1 = 512 bytes and 65536 = 32MB);"));
    Debug(TEXT("       /TransferSize <number>: Transfer size (in 512 byte sectors) to use in read/write operations;"));
    Debug(TEXT("       /Writethrough (Optional): Sets writethrough cache on;"));
    Debug(TEXT("       /Cache (Optional): Sets cache on;"));
    Debug(TEXT(""));
}
