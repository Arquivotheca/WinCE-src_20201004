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
