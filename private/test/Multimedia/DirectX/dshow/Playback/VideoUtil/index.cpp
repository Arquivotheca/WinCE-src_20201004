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
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <tchar.h>
#include "globals.h"
#include "logging.h"
#include "Index.h"

TimeStampIndex::TimeStampIndex(TCHAR* szIndexFile) :
    m_nFrames(0)
{
    _tcsncpy_s(m_szIndexFile, countof(m_szIndexFile),szIndexFile, _TRUNCATE);
    ParseIndexFile();
}

HRESULT TimeStampIndex::ParseIndexFile()
{
    HRESULT hr = S_OK;
    
    FILE* fp = NULL;
    
    errno_t error = _tfopen_s(&fp,m_szIndexFile, TEXT("r"));
    if (!error)
    {
        LOG(TEXT("File open error: %d"),error);
        return E_FAIL;
    }

    TCHAR line[1024];

    // Read and discard the first line
    if (!_ftscanf_s(fp, TEXT("%[^\n]"), line,countof(line)))
    {
        LOG(TEXT("%S: ERROR %d@%s - error in parsing index file."), __FUNCTION__, __LINE__, __FILE__);
        return E_FAIL;
    }
    fgetc(fp);

    TimeStampIndexEntry entry;
    int nEntries = 0;
    while (!feof(fp))
    {
        if (!_ftscanf_s(fp, TEXT("%[^\n]"), line,countof(line)))
        {
            LOG(TEXT("%S: ERROR %d@%s - error in parsing index file."), __FUNCTION__, __LINE__, __FILE__);
            hr = E_FAIL;
            break;
        }
        fgetc(fp);

        memset(&entry, 0, sizeof(entry));

        TCHAR szSample[2];
        LONGLONG ats;
        DWORD datalen;
        TCHAR syncPoint = TEXT(' ');
        TCHAR preroll = TEXT(' ');
        TCHAR discontinuity = TEXT(' ');

        int nFieldsParsed = 0;
        nFieldsParsed = _stscanf_s(line, TEXT("%s %d %I64d:%I64d %I64d %d %c%c%c"),
                            szSample,countof(szSample),&entry.framenum, &entry.start, &entry.stop, &ats, &datalen, &syncPoint,sizeof(syncPoint)/sizeof(TCHAR),&preroll,sizeof(preroll)/sizeof(TCHAR),&discontinuity,sizeof(discontinuity)/sizeof(TCHAR));

        // We should atleast parse 6 fields - excluding the last three flags
        if (nFieldsParsed < 6)
            break;

        if (syncPoint == TEXT('K'))
            entry.bSyncPoint = true;

        if (preroll == TEXT('P'))
            entry.bPreroll = true;

        if (discontinuity == TEXT('D'))
            entry.bDiscontinuity = true;

        m_IndexList.push_back(entry);
        nEntries++;
    }

    // Save the number of frames parsed - there are some implicit assumptions here
    if (SUCCEEDED(hr))
        m_nFrames = nEntries;
    
    // Close the file
    if (fp)
        fclose(fp);

    return hr;
}

HRESULT TimeStampIndex::GetNumFrames(int* pNumFrames)
{
    if (!pNumFrames)
        return E_POINTER;

    *pNumFrames = m_nFrames;
    return S_OK;
}

HRESULT TimeStampIndex::GetTimeStamps(int framenum, LONGLONG* pStart, LONGLONG* pStop)
{
    if ((framenum < 0) || (framenum >= m_nFrames))
        return E_INVALIDARG;

    TimeStampIndexEntry entry = m_IndexList.at(framenum);
    if (pStart)
        *pStart = entry.start;
    if (pStop)
        *pStop = entry.stop;

    return S_OK;
}

HRESULT TimeStampIndex::GetNearestKeyFrames(LONGLONG start, int* pPrevKeyFrame, int* pNextKeyFrame)
{
    TimeStampIndexList::iterator iterator = m_IndexList.begin();

    int prevKeyFrame = -1;
    int nextKeyFrame = -1;

    while(iterator != m_IndexList.end())
    {
        TimeStampIndexEntry& entry = *iterator;

        // If this is a sync point, note down the frame no
        // Note it down as the prev key frame if the start time is less than the passed in start time
        // Note it down as both the next and prev key frame if it is equal
        // Note it down as the next key frame if it is greater
        if (entry.bSyncPoint)
        {
            if (entry.start == start)
                nextKeyFrame = prevKeyFrame = entry.framenum;
            else if (entry.start < start)
                prevKeyFrame = entry.framenum;
            else
                nextKeyFrame = entry.framenum;
        }

        // As soon as both the prev and next are set, break out of the loop
        if ((prevKeyFrame != -1) && (nextKeyFrame != -1))
            break;

        // Increment to the next entry
        iterator++;
    }

    if (pPrevKeyFrame)
        *pPrevKeyFrame = prevKeyFrame;

    if (pNextKeyFrame)
        *pNextKeyFrame = nextKeyFrame;

    return S_OK;
}

HRESULT TimeStampIndex::GetNearestKeyFrames(int framenum, 
                            LONGLONG* pPrevStart, LONGLONG* pPrevStop, 
                            LONGLONG* pNextStart, LONGLONG* pNextStop)
{
    return E_NOTIMPL;
}

FrameIndex::FrameIndex(TCHAR* szIndexFile)
{
}

HRESULT FrameIndex::VerifyFrame(int framenum, IMediaSample* pMediaSample)
{
    return E_NOTIMPL;
}

HRESULT FrameIndex::GetFrame(int framenum, IMediaSample** ppMediaSample)
{
    return E_NOTIMPL;
}