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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the Utils class.
//
// ----------------------------------------------------------------------------

#include "Utils.hpp"
#include "TuxMain.hpp"

#include <assert.h>
#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources:
void
Utils::
StartupInitialize(void)
{
}

void
Utils::
ShutdownCleanup(void)
{
}

// ----------------------------------------------------------------------------
//
// Converts the specified token to an integer.
// Returns the default value if the token is empty or invalid.
//
int
Utils::
GetIntToken(
    IN TCHAR *pToken,
    IN int DefaultValue)
{
    pToken = Utils::TrimToken(pToken);
    long value = _tcstol(pToken, &pToken, 10);
    return (pToken != NULL && *pToken == _T('\0'))? (int)value : DefaultValue;
}

// ----------------------------------------------------------------------------
//
// Gets the current system time in milliseconds since Jan 1, 1601.
//
_int64
Utils::
GetWallClockTime(void)
{
    SYSTEMTIME sysTime;
    FILETIME  fileTime;
    ULARGE_INTEGER itime;
    GetSystemTime(&sysTime);
    SystemTimeToFileTime(&sysTime, &fileTime);
    memcpy(&itime, &fileTime, sizeof(itime));
    return _int64(itime.QuadPart) / _int64(10000);
}
  
// ----------------------------------------------------------------------------
//
// Inserts a time-duration into the specified buffer.
//
void
Utils::
FormatRunTime(
    IN                          long    StartTime,
    OUT __out_ecount(BuffChars) TCHAR *pBuffer,
    IN                          int     BuffChars)
{
    assert(NULL != pBuffer);
    assert(0 != BuffChars);
    
    long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), StartTime) / 1000;
    TCHAR timeBuffer[30], *tp = &timeBuffer[COUNTOF(timeBuffer)];
    
                          *(--tp) = TEXT('\0');
                          *(--tp) = TEXT('0') + (runTime % 10); runTime /= 10;
                          *(--tp) = TEXT('0') + (runTime %  6); runTime /=  6;
                          *(--tp) = TEXT(':');
                          *(--tp) = TEXT('0') + (runTime % 10); runTime /= 10;
    if    (0 < runTime) { *(--tp) = TEXT('0') + (runTime %  6); runTime /=  6; }
    if    (0 < runTime) { *(--tp) = TEXT(':'); }
    while (0 < runTime) { *(--tp) = TEXT('0') + (runTime % 10); runTime /= 10; }
    
    while (1 < BuffChars-- && tp[0])
        *(pBuffer++) = *(tp++);
    *pBuffer = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Trims spaces from the front and back of the specified token.
//
TCHAR *
Utils::
TrimToken(
    OUT TCHAR *pToken)
{
    for (; _istspace(*pToken) ; ++pToken) ;
    TCHAR *pEnd = pToken;
    for (; *pEnd != _T('\0') ; ++pEnd) ;
    while (pEnd-- != pToken)
        if (_istspace(*pEnd))
            *pEnd = _T('\0');
        else break;
    return pToken;
}

// ----------------------------------------------------------------------------
//
// Runs the specified command and waits for it to finish.
//
DWORD
Utils::
RunCommand(
    IN const TCHAR *pProgram,
    IN const TCHAR *pCommand,
    IN DWORD        WaitTimeMS)
{
    DWORD result = ERROR_SUCCESS;

    LogDebug(TEXT("Starting program \"%s\" with command-line \"%s\""),
             pProgram, pCommand);

    tstring cmdBuff;
    if (!cmdBuff.assign(pCommand))
    {
        LogError(TEXT("Cannot allocate memory to start \"%s\""), pProgram);
        return ERROR_OUTOFMEMORY;
    }

    PROCESS_INFORMATION procInfo;
    memset((void*)&procInfo, 0, sizeof(procInfo));

    long startTime = GetTickCount();

    if (!CreateProcess(pProgram,   // LPCWSTR lpszImageName, 
                       cmdBuff.get_buffer(), // LPCWSTR lpszCmdLine, 
                       NULL,       // LPSECURITY_ATTRIBUTES lpsaProcess, 
                       NULL,       // LPSECURITY_ATTRIBUTES lpsaThread, 
                       FALSE,      // BOOL fInheritHandles,
                       0,          // DWORD fdwCreate, 
                       NULL,       // LPVOID lpvEnvironment, 
                       NULL,       // LPWSTR lpszCurDir, 
                       NULL,       // LPSTARTUPINFOW lpsiStartInfo, 
                      &procInfo))  // LPPROCESS_INFORMATION lppiProcInfo
    {
        result = GetLastError();
        LogError(TEXT("Cannot start \"%s\": %s"),
                 pProgram, Win32ErrorText(result));
    }
    else
    {
        result = WaitForSingleObject(procInfo.hProcess, WaitTimeMS);

        long numTicks = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        double duration = (double)numTicks / 1000.0;

        if (WAIT_OBJECT_0 == result)
        {
            result = ERROR_SUCCESS;
            DWORD exitCode = 0;
            GetExitCodeProcess(procInfo.hProcess, &exitCode);
            LogDebug(TEXT("Prog \"%s\" ended in %.02f seconds with exit %u"),
                     pProgram, duration, exitCode);
        }
        else
        {
            if (WAIT_TIMEOUT == result)
            {
                result = ERROR_TIMEOUT;
                LogError(TEXT("Prog \"%s\" did not stop within %.02f seconds"),
                         pProgram, duration);
            }
            else
            {
                result = GetLastError();
                LogError(TEXT("Prog \"%s\" failed after %.02f seconds: %s"),
                         pProgram, duration, Win32ErrorText(result));
            }

            TerminateProcess(procInfo.hProcess, result);
        }

        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
    }

    return result;
}

// ----------------------------------------------------------------------------
