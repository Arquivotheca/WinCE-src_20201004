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
//      SoapConnectorFactory.cpp
//
// Contents:
//
//      CSoapConnectorFactory class implementation
//
//-----------------------------------------------------------------------------


#include "Headers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Interface Map
////////////////////////////////////////////////////////////////////////////////////////////////////

TYPEINFOIDS(ISoapConnectorFactory, MSSOAPLib)

BEGIN_INTERFACE_MAP(CSoapConnectorFactory)
    ADD_IUNKNOWN(CSoapConnectorFactory, ISoapConnectorFactory)
    ADD_INTERFACE(CSoapConnectorFactory, ISoapConnectorFactory)
    ADD_INTERFACE(CSoapConnectorFactory, IDispatch)
END_INTERFACE_MAP(CSoapConnectorFactory)


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapConnectorFactory::CSoapConnectorFactory()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapConnectorFactory::CSoapConnectorFactory()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapConnectorFactory::~CSoapConnectorFactory()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapConnectorFactory::~CSoapConnectorFactory()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapConnectorFactory::CreatePortConnector(IWSDLPort *pPort, ISoapConnector **ppConnector)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapConnectorFactory::CreatePortConnector(IWSDLPort *pPort, ISoapConnector **ppConnector)
{
    CHK_ARG(pPort);
    CHK_ARG(ppConnector);

    HRESULT          hr              = S_OK;
    ISoapConnector  *pSoapConnector  = 0;

    hr = CreateConnector(&pSoapConnector);

    if (FAILED(hr) || ! pSoapConnector)
    {
        goto Cleanup;
    }

    ASSERT(pSoapConnector);

    CHK(pSoapConnector->ConnectWSDL(pPort));

    *ppConnector   = pSoapConnector;
    pSoapConnector = 0;
    hr             = S_OK;

Cleanup:
    ReleaseInterface(pSoapConnector);
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapConnectorFactory::CreateConnector(ISoapConnector **pConnector)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapConnectorFactory::CreateConnector(ISoapConnector **pConnector)
{
    ASSERT(pConnector && ! *pConnector);

    ISoapConnector *pSoapConnector = 0;
    HRESULT         hr = S_OK;

    hr = ::CoCreateInstance(CLSID_HttpConnector, 0, CLSCTX_INPROC_SERVER,
                            IID_ISoapConnector, reinterpret_cast<void **>(&pSoapConnector));

    *pConnector = pSoapConnector;
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

