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
#include "utility.h"
#include "PlaylistParser.h"
#include <clparse.h>
#include <tchar.h>

extern SPS_SHELL_INFO *g_pShellInfo;
extern CPlaylistParser g_PlaylistParser;

// Features that we support
TCHAR* SupportedStatus[] = { STATUS_CPU,
                             STATUS_MEM
                            };

TCHAR* SupportedProtocols[] = { PROTO_LOCAL,
                             PROTO_MMSU,
                             PROTO_MMST,
                             PROTO_HTTP
                            };

int nSupportedProtocols = sizeof(SupportedProtocols)/sizeof(SupportedProtocols[0]);

/**
  * This function parses the command line and extracts the parameters to be passed to the test.
  */
PerfTestConfig* ParseCmdLineToConfig(LPCTSTR szCmdLine)
{
    PerfTestConfig* pConfig = NULL;
    CClParse cmdFile(szCmdLine);
    TCHAR* pToken;
    int pos = 0;
    TCHAR string[4*MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    CClParse* cmdLine;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    //if the command line was a file, load up that file
    if (cmdFile.GetOptString(_T("CmdFile"), string, Countof(string)))
    {
        DWORD dwBytesRead = 0;
        hFile = CreateFile(string,                   // input file name
                        GENERIC_READ,              // Open for reading
                        FILE_SHARE_READ,           // Share for reading
                        NULL,                      // No security
                        OPEN_EXISTING,             // Existing file only
                        FILE_ATTRIBUTE_NORMAL,     // Normal file
                        NULL);                     // No template file

        if (hFile == INVALID_HANDLE_VALUE)
        {
            LOG(TEXT("%S: ERROR %d@%S - failed CreateFile on file:%s with GLE 0x%0x"), __FUNCTION__, __LINE__, __FILE__, string, GetLastError());
            goto PARSE_CMD_LINE_EXIT;
        }
        char cstring[512];
        memset(cstring,0, sizeof(cstring));
        //read the file
            if (ReadFile(hFile, cstring, sizeof(cstring), &dwBytesRead, NULL) == 0)
        {
            LOG(TEXT("%S: ERROR %d@%S - failed ReadFile on file:%s with GLE 0x%0x"), __FUNCTION__, __LINE__, __FILE__, string, GetLastError());
            goto PARSE_CMD_LINE_EXIT;
        }
        cstring[dwBytesRead] = 0;
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, cstring, -1, string, Countof(string));
        // Now the command line is in string

    }
    else {
    }

    //create a parser out of the command line
    cmdLine = new CClParse((hFile != INVALID_HANDLE_VALUE) ? string : szCmdLine);

    pConfig = new PerfTestConfig();

    // Get the XML playlist file if any, need to give complete path
    if (cmdLine->GetOptString(_T("Playlist"), string, Countof(string)))
    {
        pConfig->SetPlaylist(string);
    }
    else {
        //all this so that we can load playlist.xml from the current directory
        TCHAR szDir[254];
        DWORD dwActual, i =0;
        dwActual= GetModuleFileName(NULL, szDir, 254);
        if(dwActual) {
            i = dwActual -1;
            while( i >=0 && szDir[i]!='\\')
                i--;
            i++;
        }
        szDir[i] = '\0';
        wcsncat_s(szDir, _countof(szDir), TEXT("playlist.xml"), _TRUNCATE);

        pConfig->SetPlaylist(szDir);
    }

    // If the protocols to be used are specified
    if (cmdLine->GetOptString(_T("Protocol"), string, Countof(string)))
    {
        // Parse string into protocols
        pos = 0;
        while(pToken = GetNextToken(string, &pos, TEXT(','))) {
            pConfig->AddProtocol(pToken);
        }
    }
    else {
        pConfig->AddProtocol(ALL);
    }

    // Get the clip-ids
    if(cmdLine->GetOptString(_T("ClipID"), string, Countof(string)))
    {
        // Parse string into protocols
        pos = 0;
        while(pToken = GetNextToken(string, &pos, TEXT(',')))
            pConfig->AddClipID(pToken);
    }
    else {
        pConfig->AddClipID(ALL);
    }

    // Get the status to report
    if(cmdLine->GetOptString(_T("Status"), string, Countof(string)))
    {
        // Parse szStatusString
        pos = 0;
        while(pToken = GetNextToken(string, &pos, TEXT(',')))
            pConfig->AddStatusToReport(pToken);

        //if we want status, then user has option of determining the sampling interval
        //default is 120 ms, otherwise we will ignore this option
        if(cmdLine->GetOptString(_T("Interval"), string, Countof(string)))
        {
            int sampleInterval = _ttoi(string);
            pConfig->SetSampleInterval(sampleInterval);
        }
    }

    // Get the status to report
    if(cmdLine->GetOptString(_T("Perflog"), string, Countof(string)))
    {
        if (!_tcsncmp(string, TEXT("."), 1))
            pConfig->SetPerflog(NULL);
        else
            pConfig->SetPerflog(string);
    }

    // Are we using a DRM clip or not?
    if(cmdLine->GetOptString(_T("DRM"), string, Countof(string)))
    {
        if (!_tcsncmp(string, TEXT("true"), 4))
            pConfig->SetDRM(true);
        else
            pConfig->SetDRM(false);
    }
    else {
        pConfig->SetDRM(false);
    }

    // VMR Alpha blending test?
    if(cmdLine->GetOptString(_T("AlphaBlend"), string, Countof(string)))
    {
        if (!_tcsncmp(string, TEXT("true"), 4))
            pConfig->SetAlphaBlend(true);
        else
            pConfig->SetAlphaBlend(false);
    }
    else {
        pConfig->SetAlphaBlend(false);
    }

    if(cmdLine->GetOptString(_T("Alpha"), string, Countof(string)))
    {
        //double fAlpha = atof(string);
        float fAlpha = 0.0;
        _stscanf_s( string, _T("%f"), &fAlpha );
        pConfig->SetAlpha(fAlpha);
    }

    //how many times does each clip need to run
    if (cmdLine->GetOptString(_T("Repeat"), string, Countof(string)))
    {
        int reps = _ttoi(string);
        pConfig->SetRepeat(reps);
    }
    else
        pConfig->SetRepeat(1);

    //do we want to run the tests under a particular video renderer mode
    if (cmdLine->GetOptString(_T("VRMode"), string, Countof(string)))
    {
        if (!_tcsncmp(string, TEXT("GDI"), 3))
            pConfig->SetVRMode(AM_VIDEO_RENDERER_MODE_GDI);
        else
            pConfig->SetVRMode(AM_VIDEO_RENDERER_MODE_DDRAW);
    }

    //do we want back buffers other than default
    if (cmdLine->GetOptString(_T("MaxBackBuffers"), string, Countof(string)))
    {
        int buffers = _ttoi(string);
        pConfig->SetMaxbackBuffers(buffers);
    }

    delete cmdLine;

PARSE_CMD_LINE_EXIT:

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return pConfig;
}

/**
  * This function handles all messages other than execute.
  */
TESTPROCAPI HandleTuxMessages(UINT uMsg, TPPARAM tpParam)
{
    DWORD retval = SPR_NOT_HANDLED;
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retval = SPR_HANDLED;
    }
    return retval;
}

/**
  * Entry point to the perf tests.
  */
TESTPROCAPI SystematicPerfTest(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;

    TCHAR* szPlaylist;
    bool XMLRead = false;
    char playlist[MAX_PATH];

    // Process incoming tux messages, other than TPM_EXECUTE
    retval = HandleTuxMessages(uMsg, tpParam);
    if (retval == SPR_HANDLED)
        return retval;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Parse the tux command line
    PerfTestConfig *pConfig = ParseCmdLineToConfig((LPCTSTR) g_pShellInfo->szDllCmdLine);

    //run the test
    try
    {
        // Parse the playlist and get information about  the media
        if(szPlaylist = pConfig->GetPlaylist()) {
            if (0 < WideCharToMultiByte(CP_ACP, 0, szPlaylist, -1, playlist, sizeof(playlist), NULL, NULL))
            {
                XMLRead = g_PlaylistParser.Parse(playlist);
                if(false == XMLRead) {
                    LOG(TEXT("%S: EXCEPTION %d@%S Could not open playlist file: %s"), __FUNCTION__, __LINE__, __FILE__, szPlaylist);
                    throw PerfTestException(TEXT("Could not open playlist file."));
                }
            }
        }
        //create a new playback test and call it
        CPlaybackTest* pPlaybackTest = new CPlaybackTest();
        if (!pPlaybackTest) {
            LOG(TEXT("%S: EXCEPTION %d@%S: unable to alloc mem for PlaybackTest"), __FUNCTION__, __LINE__, __FILE__, szPlaylist);
            throw PerfTestException(TEXT("Unable to alloc mem for PlaybackTest"));
        }
        retval = pPlaybackTest->Execute(pConfig);
        delete pPlaybackTest;

    }
    catch(PerfTestException ex)
    {
        LOG(_T("%S: PerfTestException %d@%S: %s"), __FUNCTION__, __LINE__, __FILE__, ex.message);
    }

    if (pConfig)
        delete pConfig;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return retval;
}
