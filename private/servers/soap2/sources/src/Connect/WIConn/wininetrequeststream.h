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
