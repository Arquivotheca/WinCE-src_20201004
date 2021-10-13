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
//      WinInetResponseStream.h
//
// Contents:
//
//      CWinInetResponseStream class declaration
//
//-----------------------------------------------------------------------------


#ifndef __WININETRESPONSESTREAM_H_INCLUDED__
#define __WININETRESPONSESTREAM_H_INCLUDED__


// forward declaration
class CWinInetConnector;

class CWinInetResponseStream : public CConnectorStream
{
protected:
    CWinInetConnector       *m_pOwner;

protected:
	CWinInetResponseStream();
	virtual ~CWinInetResponseStream();

public:
    HRESULT set_Owner(CWinInetConnector *pOwner);

public:

    //
    // ISequentialStream
    //

    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
    STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);

    DECLARE_INTERFACE_MAP;
};


#endif //__WININETRESPONSESTREAM_H_INCLUDED__
