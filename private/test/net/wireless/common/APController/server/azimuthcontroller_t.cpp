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
// Implementation of the AzimuthController_t class.
//
// ----------------------------------------------------------------------------

#include "AzimuthController_t.hpp"
#include "RFAttenuatorState_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

#ifdef _PREFAST_
#pragma warning(disable:25028)
#else
#pragma warning(disable:4068)
#endif

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
AzimuthController_t::
AzimuthController_t(
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : AttenuatorController_t(pDeviceType, pDeviceName)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AzimuthController_t::
~AzimuthController_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Runs the specified Tcl commands and returns the results.
//
static DWORD
RunTclScript(
    const TCHAR *pCommands,
    ce::tstring *pResults,
    long         MaxSecondsToWait)
{
    HRESULT hr;
    DWORD result;

    ce::tstring shellCmdLine;

    ce::auto_handle childProcess;
    TCHAR           scriptInName[MAX_PATH] = TEXT("");
    ce::auto_handle scriptInFile;
    TCHAR           scriptOutName[MAX_PATH] = TEXT("");
    ce::auto_handle scriptOutFile;

    // Get the name of the temporary directory.
    TCHAR tempDir[MAX_PATH-14];
    DWORD tempDirChars = COUNTOF(tempDir);
    if (GetTempPath(tempDirChars - 1, tempDir) > tempDirChars)
    {
        result = ERROR_OUTOFMEMORY;
        LogError(TEXT("Can't get temp directory name"));
        goto Cleanup;
    }

    // Synchronize these operations to avoid file-system collisions.
    else
    {
        static ce::critical_section LockObject;
        ce::gate<ce::critical_section> locker(LockObject);

        // Generate the temporary file names.
        result = GetTempFileName(tempDir, TEXT("tci"), 0, scriptInName);
        if (0 == result)
        {
            LogError(TEXT("Can't get temp script input file name: %s"),
                     Win32ErrorText(result));
            goto Cleanup;
        }
        result = GetTempFileName(tempDir, TEXT("tco"), 0, scriptOutName);
        if (0 == result)
        {
            LogError(TEXT("Can't get temp script output file name: %s"),
                     Win32ErrorText(result));
            goto Cleanup;
        }

        // Redirect the script's output to the temporary file.
        ce::tstring redirected;
        result = WiFUtils::FmtMessage(&redirected,
                                      TEXT("set OUT [ open {%s} w ]\n%s"),
                                      scriptOutName, pCommands);
        if (NO_ERROR != result)
        {
            goto Cleanup;
        }

        // Convert the commands to ASCII.
        ce::string mbCommands;
        hr = WiFUtils::ConvertString(&mbCommands, redirected,
                                     "Tcl commands");
        if (FAILED(hr))
        {
            result = HRESULT_CODE(hr);
            goto Cleanup;
        }
    
        // Copy the command(s) to the temporary input file.
        scriptInFile = CreateFile(scriptInName,
                                  GENERIC_WRITE, 0, NULL,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (!scriptInFile.valid() || INVALID_HANDLE_VALUE == scriptInFile)
        {
            result = GetLastError();
            LogError(TEXT("Can't open script file \"%s\": %s"),
                     scriptInName, Win32ErrorText(result));
            goto Cleanup;
        }

        DWORD writeSize;
        if (!WriteFile(scriptInFile, &mbCommands[0],
                                      mbCommands.length(), &writeSize, NULL)
         || mbCommands.length() != writeSize)
        {
            result = GetLastError();
            LogError(TEXT("Can't write Tcl commands to script file: %s"),
                     Win32ErrorText(result));
            goto Cleanup;
        }

        scriptInFile.close();

        // Generate the shell command-line.
        result = WiFUtils::FmtMessage(&shellCmdLine, 
                                      TEXT("cmd.exe /Q /A /C %s %s"), 
                                      APCUtils::pShellExecName, scriptInName);
        if (NO_ERROR != result)
        {
            goto Cleanup;
        }
    
        // Execute the tclsh process.
        PROCESS_INFORMATION procInfo;
        memset(&procInfo, 0, sizeof(procInfo));

        STARTUPINFO startInfo;
        memset(&startInfo, 0, sizeof(startInfo));
        startInfo.cb = sizeof(startInfo);
        startInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
        startInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        startInfo.dwFlags |= STARTF_USESTDHANDLES;

        TCHAR *cmdLine = shellCmdLine.get_buffer();
        if (!CreateProcess(NULL, cmdLine,
                           NULL,          // process security attributes
                           NULL,          // primary thread security attributes
                           TRUE,          // handles are inherited
                           0,             // creation flags
                           NULL,          // use parent's environment
                           NULL,          // use parent's current directory
                           &startInfo,    // STARTUPINFO
                           &procInfo))    // PROCESS_INFORMATION
        {
            result = GetLastError();
            LogError(TEXT("CreateProcess \"%s\" failed: %s"),
                     cmdLine, Win32ErrorText(result));
            goto Cleanup;
        }
        childProcess = procInfo.hProcess;
        CloseHandle(procInfo.hThread);
    }

    // Wait for the child to finish.
    LogDebug(TEXT("[AC] Waiting %d seconds for Tcl script"), MaxSecondsToWait);
    result = WaitForSingleObject(childProcess, MaxSecondsToWait * 1000);
    if (WAIT_OBJECT_0 != result)
    {
        LogError(TEXT("Shell failed: %s"), Win32ErrorText(result));
        goto Cleanup;
    }
    if (GetExitCodeProcess(childProcess, &result) && 0 != result)
    {
        LogError(TEXT("Shell failed: probably")
                 TEXT(" unable to exec Tcl Shell \"%s\""), 
                 APCUtils::pShellExecName);
        result = ERROR_FILE_NOT_FOUND;
        goto Cleanup;
    }

    // Read the script output and convert it to unicode.
    pResults->clear();
    scriptOutFile = CreateFile(scriptOutName,
                               GENERIC_READ, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
    if (!scriptOutFile.valid() || INVALID_HANDLE_VALUE == scriptOutFile)
    {
        result = GetLastError();
        LogError(TEXT("Can't open Tcl script output file \"%s\": %s"),
                 scriptOutName, Win32ErrorText(result));
        goto Cleanup;
    }
    for (;;)
    {
        char readBuffer[1024];
        DWORD bytesRead = 0;
        if (!ReadFile(scriptOutFile, readBuffer,
                             COUNTOF(readBuffer), &bytesRead, NULL))
        {
            result = GetLastError();
            if (ERROR_MORE_DATA != result)
            {
                LogError(TEXT("Can't read Tcl script output: %s"),
                         Win32ErrorText(result));
                goto Cleanup;
            }
        }
        if (0 == bytesRead)
            break;

        ce::tstring tResults;
        hr = WiFUtils::ConvertString(&tResults, readBuffer,
                                     "Tcl results", bytesRead);
        if (FAILED(hr))
        {
            result = HRESULT_CODE(hr);
            goto Cleanup;
        }
        if (!pResults->append(tResults))
        {
            result = ERROR_OUTOFMEMORY;
            goto Cleanup;
        }
    }

    result = NO_ERROR;

Cleanup:

    if (TEXT('\0') != scriptInName[0])
    {
        scriptInFile.close();
        DeleteFile(scriptInName);
    }
    if (TEXT('\0') != scriptOutName[0])
    {
        scriptOutFile.close();
        DeleteFile(scriptOutName);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the current, minimum and maximum RF attenuation.
//
DWORD
AzimuthController_t::
GetAttenuator(
    RFAttenuatorState_t *pResponse)
{
    DWORD       result;
    ce::tstring script;
    ce::tstring scriptResults;

    // Build a Tcl script to retrieve the attenuation values.
    static const TCHAR ScriptPattern[] =
        TEXT("if { [catch {package require Azimuth-Sdk} retmsg] } {\n")
        TEXT("    puts $OUT \"ERROR: cannot load Azimuth package: err=$retmsg\"\n")
        TEXT("    exit\n")
        TEXT("}\n")
        TEXT("if { [catch {atten_init %s} retmsg] } {\n")
        TEXT("    puts $OUT \"ERROR: cannot init attenuator %s: err=$retmsg\"\n")
        TEXT("    exit\n")
        TEXT("}\n")
        TEXT("set currentAtten [ atten_get %s ]\n")
        TEXT("set maximumAtten [ atten_get_max %s ]\n")
        TEXT("puts $OUT \"Current Attenuation = $currentAtten\"\n")
        TEXT("puts $OUT \"Maximum Attenuation = $maximumAtten\"\n");
    static const TCHAR CurrentLabel[] = TEXT("Current Attenuation =");
    static const TCHAR MaximumLabel[] = TEXT("Maximum Attenuation =");

    result = WiFUtils::FmtMessage(&script, ScriptPattern,
                                  GetDeviceName(), GetDeviceName(),
                                  GetDeviceName(), GetDeviceName());
    if (NO_ERROR != result)
    {
        LogError(TEXT("Tcl script generation error: %s"),
                 Win32ErrorText(result));
        return result;
    }

    // Run the Tcl script.
    result = RunTclScript(script, &scriptResults, 20);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Tcl script processing error: %s"),
                 Win32ErrorText(result));
        return result;
    }

    // Just return script errors.
    if (_tcsstr(scriptResults, TEXT("ERROR")) != NULL)
    {
        LogError(TEXT("%s"), &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    // Extract the attenuation values.
    const TCHAR *curStr = _tcsstr(scriptResults, CurrentLabel);
    const TCHAR *maxStr = _tcsstr(scriptResults, MaximumLabel);
    if (NULL == curStr || NULL == maxStr)
    {
        LogError(TEXT("Malformed script output:\n%s"),
                 &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    curStr += COUNTOF(CurrentLabel);
    maxStr += COUNTOF(MaximumLabel);
    TCHAR *curEnd;
    TCHAR *maxEnd;
    double curVal = _tcstod(curStr, &curEnd);
    double maxVal = _tcstod(maxStr, &maxEnd);

    if (NULL == curEnd || !_istspace(curEnd[0]) || 0.0 > curVal
     || NULL == maxEnd || !_istspace(maxEnd[0]) || 0.0 > maxVal)
    {
        LogError(TEXT("Malformed script output:\n%s"),
                 &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    pResponse->Clear();
    pResponse->SetCurrentAttenuation((int)(curVal + 0.5));
    pResponse->SetMinimumAttenuation(0);
    pResponse->SetMaximumAttenuation((int)(maxVal + 0.5));
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sets the current attenuation for the RF attenuator.
//
static DWORD
DoSetAttenuator(
    const TCHAR *pDeviceName,
    int          NewAttenuation,
    RFAttenuatorState_t *pResponse)
{
    DWORD       result;
    ce::tstring script;
    ce::tstring scriptResults;

    // Build a Tcl script to retrieve the attenuation values.
    static const TCHAR ScriptPattern[] =
        TEXT("if { [catch {package require Azimuth-Sdk} retmsg] } {\n")
        TEXT("    puts $OUT \"ERROR: cannot load Azimuth package: err=$retmsg\"\n")
        TEXT("    exit\n")
        TEXT("}\n")
        TEXT("if { [catch {atten_init %s} retmsg] } {\n")
        TEXT("    puts $OUT \"ERROR: cannot init attenuator %s: err=$retmsg\"\n")
        TEXT("    exit\n")
        TEXT("}\n")
        TEXT("set currentAtten [ atten_set %s %d ]\n")
        TEXT("set maximumAtten [ atten_get_max %s ]\n")
        TEXT("puts $OUT \"Current Attenuation = $currentAtten\"\n")
        TEXT("puts $OUT \"Maximum Attenuation = $maximumAtten\"\n");
    static const TCHAR CurrentLabel[] = TEXT("Current Attenuation =");
    static const TCHAR MaximumLabel[] = TEXT("Maximum Attenuation =");

    result = WiFUtils::FmtMessage(&script, ScriptPattern,
                                  pDeviceName,
                                  pDeviceName,
                                  pDeviceName,
                                  NewAttenuation,
                                  pDeviceName);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Tcl script generation error: %s"),
                 Win32ErrorText(result));
        return result;
    }

    // Run the Tcl script.
    result = RunTclScript(script, &scriptResults, 20);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Tcl script processing error: %s"),
                 Win32ErrorText(result));
        return result;
    }

    // Just return script errors.
    if (_tcsstr(scriptResults, TEXT("ERROR")) != NULL)
    {
        LogError(TEXT("%s"), &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    // Extract the attenuation values.
    const TCHAR *curStr = _tcsstr(scriptResults, CurrentLabel);
    const TCHAR *maxStr = _tcsstr(scriptResults, MaximumLabel);
    if (NULL == curStr || NULL == maxStr)
    {
        LogError(TEXT("Malformed script output:\n%s"),
                 &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    curStr += COUNTOF(CurrentLabel);
    maxStr += COUNTOF(MaximumLabel);
    TCHAR *curEnd;
    TCHAR *maxEnd;
    double curVal = _tcstod(curStr, &curEnd);
    double maxVal = _tcstod(maxStr, &maxEnd);

    if (NULL == curEnd || !_istspace(curEnd[0]) || 0.0 > curVal
     || NULL == maxEnd || !_istspace(maxEnd[0]) || 0.0 > maxVal)
    {
        LogError(TEXT("Malformed script output:\n%s"),
                 &scriptResults[0]);
        return ERROR_INVALID_DATA;
    }

    // Make sure the new attenuation is reasonably close.
    int newAtten = (int)(curVal + 0.5);
    if (newAtten != NewAttenuation)
    {
        LogError(TEXT("Attenuation set failed: asked for %d, got %g"),
                 NewAttenuation, curVal);
        return ERROR_INVALID_DATA;
    }

    pResponse->Clear();
    pResponse->SetCurrentAttenuation(newAtten);
    pResponse->SetMinimumAttenuation(0);
    pResponse->SetMaximumAttenuation((int)(maxVal + 0.5));
    return NO_ERROR;
}

DWORD
AzimuthController_t::
SetAttenuator(
    const RFAttenuatorState_t &NewState,
          RFAttenuatorState_t *pResponse)
{
    DWORD result = NO_ERROR;

    // Validate the input parameters.
    const long MinimumStepTime = 400;
    long stepTime = NewState.GetStepTimeMS();
    if (stepTime < MinimumStepTime)
        stepTime = MinimumStepTime;

    // Until the attenuation reaches the desired setting...
    int  newAtten = NewState.GetCurrentAttenuation();
    long  stopTime = (long)NewState.GetAdjustTime() * 1000;
    DWORD startTime = GetTickCount();
    for (;;)
    {
        // Set the attenuation.
        result = DoSetAttenuator(GetDeviceName(), newAtten, pResponse);
        if (NO_ERROR != result)
            break;

        // Stop if we've reached the desired attenuation setting.
        int remaining = NewState.GetFinalAttenuation() - newAtten;
        if (remaining == 0)
            break;

        // Calculate the next attenuation step and sleep until it's time.
        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        long remainingTime = stopTime - runTime;
        if (remainingTime < MinimumStepTime)
        {
            newAtten += remaining;
        }
        else
        {
            long remainingSteps = (remainingTime + stepTime - 1) / stepTime;
            if (remainingSteps < 2)
                remainingSteps = 1;
            else
            {
                while (remaining / remainingSteps == 0)
                       remainingSteps--;
            }
            newAtten += remaining / remainingSteps;
            Sleep(remainingTime / remainingSteps);
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
