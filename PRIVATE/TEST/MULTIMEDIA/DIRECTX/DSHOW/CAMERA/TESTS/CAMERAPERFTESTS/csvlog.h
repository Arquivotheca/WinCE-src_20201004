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
#pragma once

#include <windows.h>
#include "captureframework.h"
#include "globals.h"
#include "testvector.h"

typedef class cCSVFile
{
    public:
        cCSVFile();
        ~cCSVFile();

        BOOL OpenFile(TCHAR *tszFileName);
        BOOL CloseFile();

        BOOL LogLine(LPCTSTR szFormat, ...);

    private:
        FILE *m_hFile;
}CSVFILE, *PCSVFILE;

#define TIMESTAMP_INTERVAL 500

typedef class cCameraCSV:public cCSVFile
{
    friend DWORD myThreadProc(LPVOID lpParameter);

    public:
        cCameraCSV();
        ~cCameraCSV();

        BOOL Init(TCHAR *tszFileName, PCAPTUREFRAMEWORK pCaptureFramework, PTESTVECTOR pVectorData);
        BOOL Cleanup();

        BOOL StartTest(int nTestID, int nTestLength, DWORD dwStream);
        BOOL StopTest();

        BOOL StartRun();
        BOOL StopRun();

    private:
        BOOL OutputTimestampData();
        BOOL OutputHeaders();
        int GetFileSize();
        BOOL GetStreamInfo(DWORD dwStream, AM_MEDIA_TYPE **ppamt);

        PCAPTUREFRAMEWORK m_pCaptureGraph;
        PTESTVECTOR m_pTestVector;

        HANDLE m_hLoggingMutex;

        HANDLE m_hLoggerThread;
        HANDLE m_hLoggingEvent;
        BOOL m_bLoggingThreadActive;

        int m_nTestCaseNumber;
        int m_nRunNumber;
        int m_nTestID;
        DWORD m_dwStream;
}CAMERACSV, *PCAMERACSV;

