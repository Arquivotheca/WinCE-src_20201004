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
//      SoapConnectorFactory.h
//
// Contents:
//
//      CSoapConnectorFactory class declaration
//
//-----------------------------------------------------------------------------

#ifndef __SOAPCONNECTORFACTORY_H_INCLUDED__
#define __SOAPCONNECTORFACTORY_H_INCLUDED__


class CSoapConnectorFactory : public CDispatchImpl<ISoapConnectorFactory>
{
protected:
    CSoapConnectorFactory();
    ~CSoapConnectorFactory();

public:
    //
    // ISoapConnectorFactory
    //
    STDMETHOD(CreatePortConnector)(IWSDLPort *pPort, ISoapConnector **ppConnector);

private:
    static HRESULT CreateConnector(ISoapConnector **pConnector);

    DECLARE_INTERFACE_MAP;
};


#endif //__SOAPCONNECTORFACTORY_H_INCLUDED__
