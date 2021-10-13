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
#ifndef _REMOTE_EVENT_H
#define _REMOTE_EVENT_H

#include "globals.h"
#include "GraphEvent.h"

#define FILTER_NAME_LEN    64
#define PIN_NAME_LEN        32
#define NUM_FILTERS        10
#define NUM_CONNECTIONS    16
#define NUM_PINS            4


enum RemoteEventType
{
    REMOTE_GRAPH_DESC,
    REMOTE_GRAPH_EVENT,
    END_OF_SESSION
};

class RemotePinDesc
{
    TCHAR szPinName[PIN_NAME_LEN];
};

struct RemoteFilterDesc
{
    TCHAR szName[FILTER_NAME_LEN];
    int nInPins;
    int nOutPins;

    // RemotePinDesc inPins[NUM_PINS];
    // RemotePinDesc outPins[NUM_PINS];

    bool bIsTap;

public:
    RemoteFilterDesc()
    {
    }

    RemoteFilterDesc(const TCHAR* szName, int nInPins, int nOutPins, bool bIsTap)
    {
        errno_t  Error;
        Error = _tcsncpy_s (this->szName,countof(this->szName),szName, _TRUNCATE);
        this->nInPins = nInPins;
        this->nOutPins = nOutPins;
        this->bIsTap = bIsTap;
    }

    HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
    HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);

};

struct RemoteConnectionDesc
{
    TCHAR szFilter1[FILTER_NAME_LEN];
    TCHAR szFilter2[FILTER_NAME_LEN];
    int pinFrom;
    int pinTo;

public:
    RemoteConnectionDesc()
    {
    }

    RemoteConnectionDesc(const TCHAR* szFilter1, int pinFrom, const TCHAR* szFilter2, int pinTo)
    {
        errno_t  Error;
        Error = _tcsncpy_s (this->szFilter1,_tcslen(szFilter1), szFilter1, _TRUNCATE);
        Error = _tcsncpy_s (this->szFilter2,_tcslen(szFilter2),szFilter2, _TRUNCATE);
        this->pinFrom = pinFrom ;
        this->pinTo = pinTo;
    }

    HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
    HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct RemoteGraphDesc
{
    int nFilters;
    RemoteFilterDesc filter[NUM_FILTERS];
    int nConnections;
    RemoteConnectionDesc connection[NUM_CONNECTIONS];


public:
    RemoteGraphDesc()
    {
        nFilters = 0;
        nConnections = 0;
    }

    void AddFilter(RemoteFilterDesc* pFilterDesc)
    {
        filter[nFilters] = *pFilterDesc;
        nFilters++;
    }

    void AddConnection(RemoteConnectionDesc* pConnectionDesc)
    {
        connection[nConnections] = *pConnectionDesc;
        nConnections++;
    }

    HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten)
    {
        //BUGBUG: no check for output size
        BYTE* outbuf = buffer;

        // Output the type
        *(RemoteEventType*)outbuf = REMOTE_GRAPH_DESC;
        outbuf += sizeof(RemoteEventType);

        // Placeholder for the size of the event in the buffer
        DWORD* pSize = (DWORD*)outbuf;
        outbuf += sizeof(DWORD);

        // Output the graph desc
        memcpy(outbuf, this, sizeof(RemoteGraphDesc));
        outbuf += sizeof(RemoteGraphDesc);

        *pSize = outbuf - buffer;

        if (pBytesWritten)
            *pBytesWritten = *pSize;

        return S_OK;
    }

    HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

#if 0
struct RemoteTapFilterDesc
{
    TCHAR szTapName[FILTER_NAME_LEN];
    TCHAR szFilter1[FILTER_NAME_LEN];
    TCHAR szFilter2[FILTER_NAME_LEN];
    int pinFrom;
    int pinTo;

public:
    HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
    HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};
#endif

struct RemoteGraphEvent
{
public:
    TCHAR szTapName[FILTER_NAME_LEN];
    GraphEvent graphEvent;
    int size;
    BYTE pGraphEventData[1];

public:
    static HRESULT Serialize(BYTE* buffer, int bufsize, TCHAR* szTapName, int szTapNameLength, GraphEvent graphEvent, BYTE* pGraphEventData, int eventDataSize, DWORD* pBytesWritten);
    //static HRESULT Deserialize(BYTE* buffer, int size, RemoteGraphEvent** ppRemoteGraphEvent, DWORD* pBytesRead);
};

// Any remote event will look like this
struct RemoteEvent
{
public:
    RemoteEventType type;
    DWORD size;
    BYTE pRemoteEventData[1];

public:
    //static HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

#endif
