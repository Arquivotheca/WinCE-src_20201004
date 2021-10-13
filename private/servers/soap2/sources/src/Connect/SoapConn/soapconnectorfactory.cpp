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
    ReleaseInterface((const ISoapConnector*&)pSoapConnector);
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

