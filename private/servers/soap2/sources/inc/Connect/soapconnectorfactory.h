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
