//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      WinInetRequestStream.h
//
// Contents:
//
//      CWinInetRequestStream class declaration
//
//-----------------------------------------------------------------------------

#ifndef __WININETREQUESTSTREAM_H_INCLUDED__
#define __WININETREQUESTSTREAM_H_INCLUDED__


#define FORCE_RETRY_MAX     (10)


#ifdef UNDER_CE
#include "httprequest.h"
#endif /* UNDER_CE */


// forward declaration
class CWinInetConnector;


class CWinInetRequestStream : public CConnectorStream
{
protected:
    CWinInetConnector          *m_pOwner;
    CMemoryStream               m_MemoryStream;

protected:
	CWinInetRequestStream();
	virtual ~CWinInetRequestStream();

public:
    HRESULT set_Owner(CWinInetConnector *pOwner);

#ifndef UNDER_CE
    HRESULT Send(HINTERNET hRequest);
#else
	HRESULT Send(async_internet_handle &hRequest);	
#endif 

    HRESULT EmptyStream();

public:

    //
    // ISequentialStream
    //

    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
    STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);

    DECLARE_INTERFACE_MAP;

private:
    static const WCHAR s_szContentLength[];
};


#endif //__WININETREQUESTSTREAM_H_INCLUDED__
